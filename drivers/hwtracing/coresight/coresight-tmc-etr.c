/*
 * Copyright(C) 2016 Linaro Limited. All rights reserved.
 * Author: Mathieu Poirier <mathieu.poirier@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/coresight.h>
#include <linux/dma-mapping.h>
#include <linux/arm-smccc.h>
#include "coresight-priv.h"
#include "coresight-tmc.h"

/* SW mode sync insertion interval
 *
 * Sync insertion interval for 1M is based on assumption of
 * trace data generated at  4bits/cycle ,cycle period of 0.4 ns
 * and atleast 4 syncs per buffer wrap.
 *
 * One limitation of fixing only 4 syncs per buffer wrap is that
 * we might loose 1/4 of the initial buffer data due to lack of sync.
 * But on the other hand, we could reduce the sync insertion frequency
 * by increasing the buffer size which seems to be a good compromise.
 */
#define SYNC_TICK_NS_PER_MB 200000 /* 200us */
#define SYNCS_PER_FILL 4

/* Global mode timer management */

/**
 * struct tmc_etr_tsync_global - Global mode timer
 * @drvdata_cpumap:	cpu to tmc drvdata map
 * @timer:		global timer shared by all cores
 * @tick:		gloabl timer tick period
 * @active_count:	timer reference count
 */
static struct tmc_etr_tsync_global {
	struct tmc_drvdata *drvdata_cpumap[NR_CPUS];
	struct hrtimer	timer;
	int active_count;
	u64 tick;
} tmc_etr_tsync_global;

/* Accessor functions for tsync global */
void tmc_etr_add_cpumap(struct tmc_drvdata *drvdata)
{
	tmc_etr_tsync_global.drvdata_cpumap[drvdata->cpu] = drvdata;
}

static inline struct tmc_drvdata *cpu_to_tmcdrvdata(int cpu)
{
	return tmc_etr_tsync_global.drvdata_cpumap[cpu];
}

static inline struct hrtimer *tmc_etr_tsync_global_timer(void)
{
	return &tmc_etr_tsync_global.timer;
}

static inline void tmc_etr_tsync_global_tick(u64 tick)
{
	tmc_etr_tsync_global.tick = tick;
}

/* Refernence counting is assumed to be always called from
 * an atomic context.
 */
static inline int tmc_etr_tsync_global_addref(void)
{
	return ++tmc_etr_tsync_global.active_count;
}

static inline int tmc_etr_tsync_global_delref(void)
{
	return --tmc_etr_tsync_global.active_count;
}

/* Sync insertion API */
static void tmc_etr_insert_sync(struct tmc_drvdata *drvdata)
{
	struct coresight_device *sdev = drvdata->etm_source;
	struct etr_tsync_data *syncd = &drvdata->tsync_data;
	int err = 0, len;
	u64 rwp;

	/* We have three contenders for ETM control.
	 * 1. User initiated ETM control
	 * 2. Timer sync initiated ETM control
	 * 3. No stop on flush initated ETM control
	 * They all run in an atomic context and that too in
	 * the same core. Either on a core in which ETM is associated
	 * or in the primary core thereby mutually exclusive.
	 *
	 * To avoid any sync insertion while ETM is disabled by
	 * user, we rely on the device hw_state.
	 * Like for example, hrtimer being in active state even
	 * after ETM is disabled by user.
	 */
	if (sdev->hw_state != USR_START)
		return;

	rwp = tmc_read_rwp(drvdata);
	if (!syncd->prev_rwp)
		goto sync_insert;

	if (syncd->prev_rwp <= rwp) {
		len = rwp - syncd->prev_rwp;
	} else { /* Buffer wrapped */
		goto sync_insert;
	}

	/* Check if we reached buffer threshold */
	if (len < syncd->len_thold)
		goto skip_insert;

	/* Software based sync insertion procedure */
sync_insert:
	/* Disable source */
	if (likely(sdev && source_ops(sdev)->disable_raw))
		source_ops(sdev)->disable_raw(sdev);
	else
		err = -EINVAL;

	/* Enable source */
	if (likely(sdev && source_ops(sdev)->enable_raw))
		source_ops(sdev)->enable_raw(sdev);
	else
		err = -EINVAL;

	if (!err) {
		/* Mark the write pointer of sync insertion */
		syncd->prev_rwp = tmc_read_rwp(drvdata);
	}

skip_insert:
	return;
}

/* Timer handler APIs */

static enum hrtimer_restart tmc_etr_timer_handler_percore(struct hrtimer *t)
{
	struct tmc_drvdata *drvdata;

	drvdata = container_of(t, struct tmc_drvdata, timer);
	hrtimer_forward_now(t, ns_to_ktime(drvdata->tsync_data.tick));
	tmc_etr_insert_sync(drvdata);
	return HRTIMER_RESTART;
}

static enum hrtimer_restart tmc_etr_timer_handler_global(struct hrtimer *t)
{
	cpumask_t active_mask;
	int cpu;

	hrtimer_forward_now(t, ns_to_ktime(tmc_etr_tsync_global.tick));

	active_mask = coresight_etm_active_list();
	/* Run sync insertions for all active ETMs */
	for_each_cpu(cpu, &active_mask)
		tmc_etr_insert_sync(cpu_to_tmcdrvdata(cpu));

	return HRTIMER_RESTART;
}

/* Timer init API common for both global and per core mode */
void tmc_etr_timer_init(struct tmc_drvdata *drvdata)
{
	struct hrtimer *timer;

	timer = is_etm_sync_mode_sw_global() ?
		tmc_etr_tsync_global_timer() : &drvdata->timer;
	hrtimer_init(timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
}

/* Timer setup API common for both global and per core mode
 *
 * Global mode: Timer gets started only if its not active already.
 *		Number of users managed by reference counting.
 * Percore mode: Timer gets started always
 *
 * Always executed in an atomic context either in IPI handler
 * on a remote core or with irqs disabled in the local core
 */
static void tmc_etr_timer_setup(void *data)
{
	struct tmc_drvdata *drvdata = data;
	struct hrtimer *timer;
	bool mode_global;
	u64 tick;

	tick = drvdata->tsync_data.tick;
	mode_global = is_etm_sync_mode_sw_global();
	if (mode_global) {
		if (tmc_etr_tsync_global_addref() == 1) {
			/* Start only if we are the first user */
			tmc_etr_tsync_global_tick(tick); /* Configure tick */
		} else {
			dev_dbg(drvdata->dev, "global timer active already\n");
			return;
		}
	}

	timer = mode_global ?
		tmc_etr_tsync_global_timer() : &drvdata->timer;
	timer->function = mode_global ?
		tmc_etr_timer_handler_global : tmc_etr_timer_handler_percore;
	dev_dbg(drvdata->dev, "Starting sync timer, mode:%s period:%lld ns\n",
		mode_global ? "global" : "percore", tick);
	hrtimer_start(timer, ns_to_ktime(tick), HRTIMER_MODE_REL_PINNED);
}

/* Timer cancel API common for both global and per core mode
 *
 * Global mode: Timer gets cancelled only if there are no other users
 * Percore mode: Timer gets cancelled always
 *
 * Always executed in an atomic context either in IPI handler
 * on a remote core or with irqs disabled in the local core
 */
static void tmc_etr_timer_cancel(void *data)
{
	struct tmc_drvdata *drvdata = data;
	struct hrtimer *timer;
	bool mode_global;

	mode_global = is_etm_sync_mode_sw_global();
	if (mode_global) {
		if (tmc_etr_tsync_global_delref() != 0) {
			/* Nothing to do if we are not the last user */
			return;
		}
	}

	timer = mode_global ?
		tmc_etr_tsync_global_timer() : &drvdata->timer;
	hrtimer_cancel(timer);
}

static void tmc_etr_enable_hw(struct tmc_drvdata *drvdata)
{
	u32 axictl, sts;

	/* Zero out the memory to help with debug */
	memset(drvdata->vaddr, 0, drvdata->size);

	CS_UNLOCK(drvdata->base);

	if (drvdata->etr_options & CSETR_QUIRK_RESET_CTL_REG)
		tmc_disable_hw(drvdata);

	/* Wait for TMCSReady bit to be set */
	tmc_wait_for_tmcready(drvdata);

	if (drvdata->etr_options & CSETR_QUIRK_BUFFSIZE_8BX)
		writel_relaxed(drvdata->size / 8, drvdata->base + TMC_RSZ);
	else
		writel_relaxed(drvdata->size / 4, drvdata->base + TMC_RSZ);
	writel_relaxed(TMC_MODE_CIRCULAR_BUFFER, drvdata->base + TMC_MODE);

	axictl = readl_relaxed(drvdata->base + TMC_AXICTL);
	axictl &= ~TMC_AXICTL_CLEAR_MASK;
	axictl |= (TMC_AXICTL_PROT_CTL_B1 | TMC_AXICTL_WR_BURST_16);
	axictl |= TMC_AXICTL_AXCACHE_OS;

	if (tmc_etr_has_cap(drvdata, TMC_ETR_AXI_ARCACHE)) {
		axictl &= ~TMC_AXICTL_ARCACHE_MASK;
		axictl |= TMC_AXICTL_ARCACHE_OS;
	}

	writel_relaxed(axictl, drvdata->base + TMC_AXICTL);

	if (drvdata->etr_options & CSETR_QUIRK_SECURE_BUFF)
		tmc_write_dba(drvdata, drvdata->s_paddr);
	else
		tmc_write_dba(drvdata, drvdata->paddr);
	/*
	 * If the TMC pointers must be programmed before the session,
	 * we have to set it properly (i.e, RRP/RWP to base address and
	 * STS to "not full").
	 */
	if (tmc_etr_has_cap(drvdata, TMC_ETR_SAVE_RESTORE)) {
		tmc_write_rrp(drvdata, drvdata->paddr);
		if (drvdata->etr_options & CSETR_QUIRK_SECURE_BUFF)
			tmc_write_rwp(drvdata, drvdata->s_paddr);
		else
			tmc_write_rwp(drvdata, drvdata->paddr);
		sts = readl_relaxed(drvdata->base + TMC_STS) & ~TMC_STS_FULL;
		writel_relaxed(sts, drvdata->base + TMC_STS);
	}

	writel_relaxed(TMC_FFCR_EN_FMT | TMC_FFCR_EN_TI |
		       TMC_FFCR_FON_FLIN | TMC_FFCR_FON_TRIG_EVT |
		       TMC_FFCR_TRIGON_TRIGIN,
		       drvdata->base + TMC_FFCR);
	writel_relaxed(drvdata->trigger_cntr, drvdata->base + TMC_TRG);
	tmc_enable_hw(drvdata);

	CS_LOCK(drvdata->base);
}

static void tmc_etr_dump_hw(struct tmc_drvdata *drvdata)
{
	const u32 *barrier;
	u32 val;
	u32 *temp;
	u64 rwp, offset;

	rwp = tmc_read_rwp(drvdata);
	val = readl_relaxed(drvdata->base + TMC_STS);

	if (drvdata->etr_options & CSETR_QUIRK_SECURE_BUFF)
		offset = rwp - drvdata->s_paddr;
	else
		offset = rwp - drvdata->paddr;
	/*
	 * Adjust the buffer to point to the beginning of the trace data
	 * and update the available trace data.
	 */
	if (val & TMC_STS_FULL) {
		drvdata->buf = drvdata->vaddr + offset;
		drvdata->len = drvdata->size;

		if (!drvdata->formatter_en) {
			/* TODO Need to handle this differently when formatter
			 * is not present
			 */
			return;
		}

		barrier = barrier_pkt;
		temp = (u32 *)drvdata->buf;

		while (*barrier) {
			*temp = *barrier;
			temp++;
			barrier++;
		}

	} else {
		drvdata->buf = drvdata->vaddr;
		drvdata->len = offset;
	}
}

static void tmc_etr_disable_hw(struct tmc_drvdata *drvdata)
{
	CS_UNLOCK(drvdata->base);

	tmc_flush_and_stop(drvdata);
	/*
	 * When operating in sysFS mode the content of the buffer needs to be
	 * read before the TMC is disabled.
	 */
	if (drvdata->mode == CS_MODE_SYSFS)
		tmc_etr_dump_hw(drvdata);
	tmc_disable_hw(drvdata);

	CS_LOCK(drvdata->base);
}

static int tmc_enable_etr_sink_sysfs(struct coresight_device *csdev)
{
	int ret = 0, buff_sec_mapped = 0;
	bool used = false;
	unsigned long flags;
	void __iomem *vaddr = NULL;
	dma_addr_t paddr = 0, s_paddr = 0;
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	if (!is_etm_sync_mode_hw()) {
		/* Calculate parameters for sync insertion */
		drvdata->tsync_data.len_thold =
			drvdata->size / (SYNCS_PER_FILL);
		drvdata->tsync_data.tick =
			(drvdata->size / SZ_1M) * SYNC_TICK_NS_PER_MB;
		drvdata->tsync_data.prev_rwp = 0;
		if (!drvdata->tsync_data.tick) {
			drvdata->tsync_data.tick = SYNC_TICK_NS_PER_MB;
			dev_warn(drvdata->dev,
				 "Trace bufer size not sufficient, sync insertion can fail\n");
		}
	}

	/*
	 * If we don't have a buffer release the lock and allocate memory.
	 * Otherwise keep the lock and move along.
	 */
	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (!drvdata->vaddr) {
		spin_unlock_irqrestore(&drvdata->spinlock, flags);

		/*
		 * Contiguous  memory can't be allocated while a spinlock is
		 * held.  As such allocate memory here and free it if a buffer
		 * has already been allocated (from a previous session).
		 */
		vaddr = dma_alloc_coherent(drvdata->dev, drvdata->size,
					   &paddr, GFP_KERNEL);
		if (!vaddr)
			return -ENOMEM;

		if (!(drvdata->etr_options & CSETR_QUIRK_SECURE_BUFF))
			goto skip_secure_buffer;

		/* Register driver allocated dma buffer for necessary
		 * mapping in the secure world
		 */
		if (tmc_register_drvbuf(drvdata, paddr, drvdata->size)) {
			dev_info(drvdata->dev,
				 "registration failed for paddr %llx\n", paddr);
			ret = -ENOMEM;
			goto err;
		}
		buff_sec_mapped = 1;

		/* Allocate secure trace buffer */
		if (tmc_alloc_secbuf(drvdata, drvdata->size, &s_paddr)) {
			dev_info(drvdata->dev, "secure buf alloc failed\n");
			ret = -ENOMEM;
				goto err;
		}

skip_secure_buffer:
		/* Let's try again */
		spin_lock_irqsave(&drvdata->spinlock, flags);
	}

	if (drvdata->reading) {
		ret = -EBUSY;
		goto out;
	}

	/*
	 * In sysFS mode we can have multiple writers per sink.  Since this
	 * sink is already enabled no memory is needed and the HW need not be
	 * touched.
	 */
	if (drvdata->mode == CS_MODE_SYSFS)
		goto out;

	/*
	 * If drvdata::buf == NULL, use the memory allocated above.
	 * Otherwise a buffer still exists from a previous session, so
	 * simply use that.
	 */
	if (drvdata->buf == NULL) {
		used = true;
		drvdata->vaddr = vaddr;
		drvdata->paddr = paddr;
		drvdata->s_paddr = s_paddr;
		drvdata->buf = drvdata->vaddr;
	}

	if (drvdata->mode == CS_MODE_READ_PREVBOOT)
		goto out;

	drvdata->mode = CS_MODE_SYSFS;
	tmc_etr_enable_hw(drvdata);
out:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	if (!ret && !is_etm_sync_mode_hw() &&
		(drvdata->mode != CS_MODE_READ_PREVBOOT))
		smp_call_function_single(drvdata->rc_cpu, tmc_etr_timer_setup,
					 drvdata, true);
err:
	/* Free memory outside the spinlock if need be */
	if (!used && vaddr) {
		if (buff_sec_mapped)
			tmc_unregister_drvbuf(drvdata, paddr,
					      drvdata->size);
		if (s_paddr)
			tmc_free_secbuf(drvdata, s_paddr,
					drvdata->size);
		dma_free_coherent(drvdata->dev, drvdata->size, vaddr, paddr);
	}

	if (!ret)
		dev_info(drvdata->dev, "TMC-ETR enabled\n");

	return ret;
}

static int tmc_enable_etr_sink_perf(struct coresight_device *csdev)
{
	int ret = 0;
	unsigned long flags;
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (drvdata->reading) {
		ret = -EINVAL;
		goto out;
	}

	/*
	 * In Perf mode there can be only one writer per sink.  There
	 * is also no need to continue if the ETR is already operated
	 * from sysFS.
	 */
	if (drvdata->mode != CS_MODE_DISABLED) {
		ret = -EINVAL;
		goto out;
	}

	drvdata->mode = CS_MODE_PERF;
	tmc_etr_enable_hw(drvdata);
out:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	return ret;
}

static int tmc_enable_etr_sink(struct coresight_device *csdev, u32 mode)
{
	switch (mode) {
	case CS_MODE_SYSFS:
		return tmc_enable_etr_sink_sysfs(csdev);
	case CS_MODE_PERF:
		return tmc_enable_etr_sink_perf(csdev);
	}

	/* We shouldn't be here */
	return -EINVAL;
}

static void tmc_disable_etr_sink(struct coresight_device *csdev)
{
	unsigned long flags;
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (drvdata->reading) {
		spin_unlock_irqrestore(&drvdata->spinlock, flags);
		return;
	}

	/* Disable the TMC only if it needs to */
	if (drvdata->mode != CS_MODE_DISABLED) {
		tmc_etr_disable_hw(drvdata);
		drvdata->mode = CS_MODE_DISABLED;
	}

	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	if (!is_etm_sync_mode_hw())
		smp_call_function_single(drvdata->rc_cpu, tmc_etr_timer_cancel,
					 drvdata, true);

	dev_info(drvdata->dev, "TMC-ETR disabled\n");
}

void tmc_register_source(struct coresight_device *csdev, void *source)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	drvdata->etm_source = source;
}

void tmc_unregister_source(struct coresight_device *csdev)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	drvdata->etm_source = NULL;
}

static const struct coresight_ops_sink tmc_etr_sink_ops = {
	.enable		= tmc_enable_etr_sink,
	.disable	= tmc_disable_etr_sink,
	.register_source = tmc_register_source,
	.unregister_source = tmc_unregister_source,
};

const struct coresight_ops tmc_etr_cs_ops = {
	.sink_ops	= &tmc_etr_sink_ops,
};


/* APIs to manage ETM start/stop when ETR stop on flush is broken */

void tmc_flushstop_etm_off(void *data)
{
	struct tmc_drvdata *drvdata = data;
	struct coresight_device *sdev = drvdata->etm_source;

	if (sdev->hw_state == USR_START) {
		source_ops(sdev)->disable_raw(sdev);
		sdev->hw_state = SW_STOP;
	}
}

void tmc_flushstop_etm_on(void *data)
{
	struct tmc_drvdata *drvdata = data;
	struct coresight_device *sdev = drvdata->etm_source;

	if (sdev->hw_state == SW_STOP) { /* Restore the user configured state */
		source_ops(sdev)->enable_raw(sdev);
		sdev->hw_state = USR_START;
	}
}

int tmc_read_prepare_etr(struct tmc_drvdata *drvdata)
{
	int ret = 0;
	unsigned long flags;

	/* config types are set a boot time and never change */
	if (WARN_ON_ONCE(drvdata->config_type != TMC_CONFIG_TYPE_ETR))
		return -EINVAL;

	if (drvdata->mode == CS_MODE_READ_PREVBOOT) {
		/* Initialize drvdata for reading trace data from last boot */
		ret = tmc_enable_etr_sink_sysfs(drvdata->csdev);
		if (ret)
			return ret;
		/* Update the buffer offset, len */
		tmc_etr_dump_hw(drvdata);
		return 0;
	}

	if (drvdata->etr_options & CSETR_QUIRK_NO_STOP_FLUSH)
		smp_call_function_single(drvdata->rc_cpu, tmc_flushstop_etm_off,
					 drvdata, true);

	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (drvdata->reading) {
		ret = -EBUSY;
		goto out;
	}

	/* Don't interfere if operated from Perf */
	if (drvdata->mode == CS_MODE_PERF) {
		ret = -EINVAL;
		goto out;
	}

	/* If drvdata::buf is NULL the trace data has been read already */
	if (drvdata->buf == NULL) {
		ret = -EINVAL;
		goto out;
	}

	/* Disable the TMC if need be */
	if (drvdata->mode == CS_MODE_SYSFS)
		tmc_etr_disable_hw(drvdata);

	drvdata->reading = true;
out:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	if (ret && drvdata->etr_options & CSETR_QUIRK_NO_STOP_FLUSH) {
		dev_warn(drvdata->dev, "ETM wrongly stopped\n");
		/* Restore back on error */
		smp_call_function_single(drvdata->rc_cpu, tmc_flushstop_etm_on,
					 drvdata, true);
	}

	return ret;
}

int tmc_read_unprepare_etr(struct tmc_drvdata *drvdata)
{
	unsigned long flags;
	dma_addr_t paddr, s_paddr;
	void __iomem *vaddr = NULL;

	/* config types are set a boot time and never change */
	if (WARN_ON_ONCE(drvdata->config_type != TMC_CONFIG_TYPE_ETR))
		return -EINVAL;

	spin_lock_irqsave(&drvdata->spinlock, flags);

	/* RE-enable the TMC if need be */
	if (drvdata->mode == CS_MODE_SYSFS) {
		/*
		 * The trace run will continue with the same allocated trace
		 * buffer. The trace buffer is cleared in tmc_etr_enable_hw(),
		 * so we don't have to explicitly clear it. Also, since the
		 * tracer is still enabled drvdata::buf can't be NULL.
		 */
		tmc_etr_enable_hw(drvdata);
	} else {
		/*
		 * The ETR is not tracing and the buffer was just read.
		 * As such prepare to free the trace buffer.
		 */
		vaddr = drvdata->vaddr;
		paddr = drvdata->paddr;
		s_paddr = drvdata->s_paddr;
		drvdata->buf = drvdata->vaddr = NULL;
		/* Assumes s_paddr, 0 is invalid */
		drvdata->s_paddr = 0;
	}

	drvdata->reading = false;
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	/* Free allocated memory out side of the spinlock */
	if (vaddr) {
		if (s_paddr) {
			tmc_unregister_drvbuf(drvdata, paddr,
					      drvdata->size);
			tmc_free_secbuf(drvdata, s_paddr,
					drvdata->size);
		}

		dma_free_coherent(drvdata->dev, drvdata->size, vaddr, paddr);
	}

	if ((drvdata->etr_options & CSETR_QUIRK_NO_STOP_FLUSH) &&
	    (drvdata->mode == CS_MODE_SYSFS))
		smp_call_function_single(drvdata->rc_cpu, tmc_flushstop_etm_on,
					drvdata, true);
	return 0;
}
