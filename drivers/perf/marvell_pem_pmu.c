// SPDX-License-Identifier: GPL-2.0
/*
 * Marvell PEM(PCIe RC) Performance Monitor Driver
 *
 * Copyright (C) 2023 Marvell.
 */

#include <linux/acpi.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/perf_event.h>

#define MPAM_ASSOC_PC                   0x1000
#define MPAM_TABLE_SIZE                 256
#define MPAM_COUNTER_OFFSET_START       1
#define MPAM_COUNTER_MAX                8
#define MPAM_CNTR_SHIFT                 5

/*
 * Each of these events maps to a free running 64 bit counter
 * with no event control, but can be reset.
 *
 */
enum pem_events {
	IB_TLP_NPR,
	IB_TLP_PR,
	IB_TLP_CPL,
	IB_TLP_DWORDS_NPR,
	IB_TLP_DWORDS_PR,
	IB_TLP_DWORDS_CPL,
	IB_INFLIGHT,
	IB_READS,
	IB_REQ_NO_RO_NCB,
	IB_REQ_NO_RO_EBUS,
	OB_TLP_NPR,
	OB_TLP_PR,
	OB_TLP_CPL,
	OB_TLP_DWORDS_NPR,
	OB_TLP_DWORDS_PR,
	OB_TLP_DWORDS_CPL,
	OB_INFLIGHT,
	OB_READS,
	OB_MERGES_NPR,
	OB_MERGES_PR,
	OB_MERGES_CPL,
	ATS_TRANS,
	ATS_TRANS_LATENCY,
	ATS_PRI,
	ATS_PRI_LATENCY,
	ATS_INV,
	ATS_INV_LATENCY,
	PEM_EVENTIDS_MAX,
};

#define PEM_MPAM_SUPPORT_LIST                           \
			((1 << IB_TLP_CPL)      |       \
			(1 << IB_TLP_DWORDS_CPL)|	\
			(1 << OB_TLP_NPR)       |       \
			(1 << OB_TLP_PR)        |       \
			(1 << OB_TLP_CPL)       |       \
			(1 << OB_TLP_DWORDS_NPR)|	\
			(1 << OB_TLP_DWORDS_PR) |       \
			(1 << OB_TLP_DWORDS_CPL)|	\
			(1 << OB_INFLIGHT)      |       \
			(1 << OB_READS)         |       \
			(1 << OB_MERGES_NPR)    |       \
			(1 << OB_MERGES_PR)     |       \
			(1 << OB_MERGES_CPL))

static u64 eventid_to_offset_table[] = {
	[IB_TLP_NPR]	     = 0x0,
	[IB_TLP_PR]	     = 0x8,
	[IB_TLP_CPL]	     = 0x10,
	[IB_TLP_DWORDS_NPR]  = 0x100,
	[IB_TLP_DWORDS_PR]   = 0x108,
	[IB_TLP_DWORDS_CPL]  = 0x110,
	[IB_INFLIGHT]	     = 0x200,
	[IB_READS]	     = 0x300,
	[IB_REQ_NO_RO_NCB]   = 0x400,
	[IB_REQ_NO_RO_EBUS]  = 0x408,
	[OB_TLP_NPR]         = 0x500,
	[OB_TLP_PR]          = 0x508,
	[OB_TLP_CPL]         = 0x510,
	[OB_TLP_DWORDS_NPR]  = 0x600,
	[OB_TLP_DWORDS_PR]   = 0x608,
	[OB_TLP_DWORDS_CPL]  = 0x610,
	[OB_INFLIGHT]        = 0x700,
	[OB_READS]	     = 0x800,
	[OB_MERGES_NPR]      = 0x900,
	[OB_MERGES_PR]       = 0x908,
	[OB_MERGES_CPL]      = 0x910,
	[ATS_TRANS]          = 0x2D18,
	[ATS_TRANS_LATENCY]  = 0x2D20,
	[ATS_PRI]            = 0x2D28,
	[ATS_PRI_LATENCY]    = 0x2D30,
	[ATS_INV]            = 0x2D38,
	[ATS_INV_LATENCY]    = 0x2D40,
};

struct pem_pmu {
	struct pmu pmu;
	void __iomem *base;
	unsigned int cpu;
	struct	device *dev;
	/* Bitmap of active counters common for all events */
	DECLARE_BITMAP(mpam_active_counters, MPAM_COUNTER_MAX);
	/* Refcount of mpam counters common for all events */
	refcount_t mpam_counter_ref[MPAM_COUNTER_MAX];
	struct hlist_node node;
};

#define to_pem_pmu(p)	container_of(p, struct pem_pmu, pmu)

static int eventid_to_offset(int eventid)
{
	return eventid_to_offset_table[eventid];
}

/* Events */
static ssize_t pem_pmu_event_show(struct device *dev,
				  struct device_attribute *attr,
				  char *page)
{
	struct perf_pmu_events_attr *pmu_attr;

	pmu_attr = container_of(attr, struct perf_pmu_events_attr, attr);
	return sysfs_emit(page, "event=0x%02llx\n", pmu_attr->id);
}

#define PEM_EVENT_ATTR(_name, _id)					\
	(&((struct perf_pmu_events_attr[]) {				\
	{ .attr = __ATTR(_name, 0444, pem_pmu_event_show, NULL),	\
		.id = _id, }						\
	})[0].attr.attr)

static struct attribute *pem_perf_events_attrs[] = {
	PEM_EVENT_ATTR(ib_tlp_npr, IB_TLP_NPR),
	PEM_EVENT_ATTR(ib_tlp_pr, IB_TLP_PR),
	PEM_EVENT_ATTR(ib_tlp_cpl_partid, IB_TLP_CPL),
	PEM_EVENT_ATTR(ib_tlp_dwords_npr, IB_TLP_DWORDS_NPR),
	PEM_EVENT_ATTR(ib_tlp_dwords_pr, IB_TLP_DWORDS_PR),
	PEM_EVENT_ATTR(ib_tlp_dwords_cpl_partid, IB_TLP_DWORDS_CPL),
	PEM_EVENT_ATTR(ib_inflight, IB_INFLIGHT),
	PEM_EVENT_ATTR(ib_reads, IB_READS),
	PEM_EVENT_ATTR(ib_req_no_ro_ncb, IB_REQ_NO_RO_NCB),
	PEM_EVENT_ATTR(ib_req_no_ro_ebus, IB_REQ_NO_RO_EBUS),
	PEM_EVENT_ATTR(ob_tlp_npr_partid, OB_TLP_NPR),
	PEM_EVENT_ATTR(ob_tlp_pr_partid, OB_TLP_PR),
	PEM_EVENT_ATTR(ob_tlp_cpl_partid, OB_TLP_CPL),
	PEM_EVENT_ATTR(ob_tlp_dwords_npr_partid, OB_TLP_DWORDS_NPR),
	PEM_EVENT_ATTR(ob_tlp_dwords_pr_partid, OB_TLP_DWORDS_PR),
	PEM_EVENT_ATTR(ob_tlp_dwords_cpl_partid, OB_TLP_DWORDS_CPL),
	PEM_EVENT_ATTR(ob_inflight_partid, OB_INFLIGHT),
	PEM_EVENT_ATTR(ob_reads_partid, OB_READS),
	PEM_EVENT_ATTR(ob_merges_npr_partid, OB_MERGES_NPR),
	PEM_EVENT_ATTR(ob_merges_pr_partid, OB_MERGES_PR),
	PEM_EVENT_ATTR(ob_merges_cpl_partid, OB_MERGES_CPL),
	PEM_EVENT_ATTR(ats_trans, ATS_TRANS),
	PEM_EVENT_ATTR(ats_trans_latency, ATS_TRANS_LATENCY),
	PEM_EVENT_ATTR(ats_pri, ATS_PRI),
	PEM_EVENT_ATTR(ats_pri_latency, ATS_PRI_LATENCY),
	PEM_EVENT_ATTR(ats_inv, ATS_INV),
	PEM_EVENT_ATTR(ats_inv_latency, ATS_INV_LATENCY),
	NULL
};

static struct attribute_group pem_perf_events_attr_group = {
	.name = "events",
	.attrs = pem_perf_events_attrs,
};

PMU_FORMAT_ATTR(event, "config:0-5");
PMU_FORMAT_ATTR(partid, "config1:0-15");

static struct attribute *pem_perf_format_attrs[] = {
	&format_attr_event.attr,
	&format_attr_partid.attr,
	NULL
};

static struct attribute_group pem_perf_format_attr_group = {
	.name = "format",
	.attrs = pem_perf_format_attrs,
};

/* cpumask */
static ssize_t pem_perf_cpumask_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct pem_pmu *pmu = dev_get_drvdata(dev);

	return cpumap_print_to_pagebuf(true, buf, cpumask_of(pmu->cpu));
}

static struct device_attribute pem_perf_cpumask_attr =
	__ATTR(cpumask, 0444, pem_perf_cpumask_show, NULL);

static struct attribute *pem_perf_cpumask_attrs[] = {
	&pem_perf_cpumask_attr.attr,
	NULL
};

static struct attribute_group pem_perf_cpumask_attr_group = {
	.attrs = pem_perf_cpumask_attrs,
};

static const struct attribute_group *pem_perf_attr_groups[] = {
	&pem_perf_events_attr_group,
	&pem_perf_cpumask_attr_group,
	&pem_perf_format_attr_group,
	NULL
};

static int pem_perf_event_init(struct perf_event *event)
{
	struct pem_pmu *pmu = to_pem_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;

	if (event->attr.type != event->pmu->type)
		return -ENOENT;

	if (is_sampling_event(event) ||
	    event->attach_state & PERF_ATTACH_TASK) {
		return -EOPNOTSUPP;
	}

	if (event->cpu < 0)
		return -EOPNOTSUPP;

	/*  We must NOT create groups containing mixed PMUs */
	if (event->group_leader->pmu != event->pmu &&
	    !is_software_event(event->group_leader))
		return -EINVAL;

	/*
	 * Set ownership of event to one CPU, same event can not be observed
	 * on multiple cpus at same time.
	 */
	event->cpu = pmu->cpu;
	hwc->idx = -1;
	return 0;
}

/*
 * Create a default MPAM partid to counter mapping
 * Table offset => partid
 * Table entry => counter index(eight counters)
 *
 * PEM Hardware supports many(partid) to one counter mapping,
 * but we are restricting to one partid to one counter mapping
 * except for counter 0 for the sake of simplicity.
 * ie. Map counter 0 as the default target for all partitions.
 *
 * Counter alloc:
 * When perf selects a partition for filtering, we allocate a
 * free counter and program the table as, table[partid] => counter X.
 * Here X ranges from 1 to 7
 *
 * Counter free:
 * table[partid] => counter 0.
 *
 * MPAM partid to counter mapping table being common for all events,
 * we keep a refcount to track that.
 * Hence a counter is allocated/freed only by the first/last user.
 *
 * Having this default target helps us to have the table programming to
 * be done only for the selected partition and not for the entire table
 * while starting a perf event.
 */
static void pem_mpam_table_init(struct pem_pmu *pmu)
{
	int i;

	for (i = 0; i < MPAM_TABLE_SIZE; i++)
		writeq(0x0, pmu->base + MPAM_ASSOC_PC + (i << 3));
}

static inline int pem_partid_to_counter(struct pem_pmu *pmu, int partid)
{
	return readq(pmu->base + MPAM_ASSOC_PC + (partid << 3));
}

static int pem_mpam_counter_alloc(struct pem_pmu *pmu, int eventid, int partid)
{
	int cntr;

	if (!((1ULL << eventid) & PEM_MPAM_SUPPORT_LIST)) {
		dev_info(pmu->dev, "MPAM filtering not supported for this event\n");
		return 0;
	}

	cntr = pem_partid_to_counter(pmu, partid);
	/* If there is an existing mapping, just add a reference */
	if (cntr) {
		refcount_inc(&pmu->mpam_counter_ref[cntr]);
	} else {
		/* Create a new mapping */
		cntr = find_next_zero_bit(pmu->mpam_active_counters,
					  MPAM_COUNTER_MAX,
					  MPAM_COUNTER_OFFSET_START);
		if (cntr >= MPAM_COUNTER_MAX)
			return -ENOSPC;

		/* a. Update the counter bitmap */
		set_bit(cntr, pmu->mpam_active_counters);
		/* b. Update the hardware mapping table */
		writeq(cntr, pmu->base + MPAM_ASSOC_PC + (partid << 3));

		refcount_set(&pmu->mpam_counter_ref[cntr], 1);
	}

	return 0;
}

static void pem_mpam_counter_free(struct pem_pmu *pmu, int eventid, int partid)
{
	int cntr;

	if (!((1ULL << eventid) & PEM_MPAM_SUPPORT_LIST))
		return;

	cntr = pem_partid_to_counter(pmu, partid);
	/* Release counter mapping */
	if (refcount_dec_and_test(&pmu->mpam_counter_ref[cntr])) {
		/* a. Update the counter bitmap */
		clear_bit(cntr, pmu->mpam_active_counters);
		/* Update the hardware mapping table */
		writeq(0, pmu->base + MPAM_ASSOC_PC + (partid << 3));
	}
}

static void pem_perf_counter_reset(struct pem_pmu *pmu,
				   struct perf_event *event, int eventid)
{
	int cntr;

	if (!((1ULL << eventid) & PEM_MPAM_SUPPORT_LIST))
		cntr = 0;
	else
		cntr = pem_partid_to_counter(pmu, event->attr.config1);

	writeq_relaxed(0x0, pmu->base + eventid_to_offset(eventid) +
		       (cntr << MPAM_CNTR_SHIFT));
}

static u64 pem_perf_read_counter(struct pem_pmu *pmu,
				 struct perf_event *event, int eventid)
{
	int cntr;

	if (!((1ULL << eventid) & PEM_MPAM_SUPPORT_LIST))
		cntr = 0;
	else
		cntr = pem_partid_to_counter(pmu, event->attr.config1);

	return readq_relaxed(pmu->base + eventid_to_offset(eventid) +
			     (cntr << MPAM_CNTR_SHIFT));
}

static void pem_perf_event_update(struct perf_event *event)
{
	struct pem_pmu *pmu = to_pem_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	u64 prev_count, new_count;

	do {
		prev_count = local64_read(&hwc->prev_count);
		new_count = pem_perf_read_counter(pmu, event, hwc->idx);
	} while (local64_xchg(&hwc->prev_count, new_count) != prev_count);

	local64_add((new_count - prev_count), &event->count);
}

static void pem_perf_event_start(struct perf_event *event, int flags)
{
	struct pem_pmu *pmu = to_pem_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	int eventid = hwc->idx;

	local64_set(&hwc->prev_count, 0);

	pem_perf_counter_reset(pmu, event, eventid);

	hwc->state = 0;
}

static int pem_perf_event_add(struct perf_event *event, int flags)
{
	struct pem_pmu *pmu = to_pem_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	int err;

	hwc->idx = event->attr.config;
	if (hwc->idx >= PEM_EVENTIDS_MAX)
		return -EINVAL;
	err = pem_mpam_counter_alloc(pmu, hwc->idx, event->attr.config1);
	if (err)
		return err;
	hwc->state |= PERF_HES_STOPPED;

	if (flags & PERF_EF_START)
		pem_perf_event_start(event, flags);

	return 0;
}

static void pem_perf_event_stop(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;

	if (flags & PERF_EF_UPDATE)
		pem_perf_event_update(event);

	hwc->state |= PERF_HES_STOPPED;
}

static void pem_perf_event_del(struct perf_event *event, int flags)
{
	struct pem_pmu *pmu = to_pem_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;

	pem_perf_event_stop(event, PERF_EF_UPDATE);
	pem_mpam_counter_free(pmu, hwc->idx, event->attr.config1);
	hwc->idx = -1;
}

static int pem_pmu_offline_cpu(unsigned int cpu, struct hlist_node *node)
{
	struct pem_pmu *pmu = hlist_entry_safe(node, struct pem_pmu,
					       node);
	unsigned int target;

	if (cpu != pmu->cpu)
		return 0;

	target = cpumask_any_but(cpu_online_mask, cpu);
	if (target >= nr_cpu_ids)
		return 0;

	perf_pmu_migrate_context(&pmu->pmu, cpu, target);
	pmu->cpu = target;
	return 0;
}

static int pem_perf_probe(struct platform_device *pdev)
{
	struct pem_pmu *pem_pmu;
	struct resource *res;
	void __iomem *base;
	char *name;
	int ret;

	pem_pmu = devm_kzalloc(&pdev->dev, sizeof(*pem_pmu), GFP_KERNEL);
	if (!pem_pmu)
		return -ENOMEM;

	pem_pmu->dev = &pdev->dev;
	platform_set_drvdata(pdev, pem_pmu);

	base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	pem_pmu->base = base;

	pem_pmu->pmu = (struct pmu) {
		.module	      = THIS_MODULE,
		.capabilities = PERF_PMU_CAP_NO_EXCLUDE,
		.task_ctx_nr = perf_invalid_context,
		.attr_groups = pem_perf_attr_groups,
		.event_init  = pem_perf_event_init,
		.add	     = pem_perf_event_add,
		.del	     = pem_perf_event_del,
		.start	     = pem_perf_event_start,
		.stop	     = pem_perf_event_stop,
		.read	     = pem_perf_event_update,
	};

	/* Choose this cpu to collect perf data */
	pem_pmu->cpu = raw_smp_processor_id();

	/* Map counter 0 as always active */
	set_bit(0, pem_pmu->mpam_active_counters);

	pem_mpam_table_init(pem_pmu);

	name = devm_kasprintf(pem_pmu->dev, GFP_KERNEL, "mrvl_pcie_rc_pmu_%llx",
			      res->start);
	if (!name)
		return -ENOMEM;

	cpuhp_state_add_instance_nocalls
			(CPUHP_AP_PERF_ARM_MARVELL_PEM_ONLINE,
			 &pem_pmu->node);

	ret = perf_pmu_register(&pem_pmu->pmu, name, -1);
	if (ret)
		goto error;

	pr_info("Marvell PEM(PCIe RC) PMU Driver for pem@%llx\n", res->start);
	return 0;
error:
	cpuhp_state_remove_instance_nocalls
			(CPUHP_AP_PERF_ARM_MARVELL_PEM_ONLINE,
			 &pem_pmu->node);
	return ret;
}

static int pem_perf_remove(struct platform_device *pdev)
{
	struct pem_pmu *pem_pmu = platform_get_drvdata(pdev);

	cpuhp_state_remove_instance_nocalls
			(CPUHP_AP_PERF_ARM_MARVELL_PEM_ONLINE,
			 &pem_pmu->node);

	perf_pmu_unregister(&pem_pmu->pmu);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id pem_pmu_of_match[] = {
	{ .compatible = "marvell,odyssey-pem-pmu", },
	{ },
};
MODULE_DEVICE_TABLE(of, pem_pmu_of_match);
#endif

#ifdef CONFIG_ACPI
static const struct acpi_device_id pem_pmu_acpi_match[] = {
	{"MRVL000E", 0},
	{},
};
MODULE_DEVICE_TABLE(acpi, pem_pmu_acpi_match);
#endif

static struct platform_driver pem_pmu_driver = {
	.driver	= {
		.name   = "pem-pmu",
		.of_match_table = of_match_ptr(pem_pmu_of_match),
		.acpi_match_table = ACPI_PTR(pem_pmu_acpi_match),
		.suppress_bind_attrs = true,
	},
	.probe		= pem_perf_probe,
	.remove		= pem_perf_remove,
};

static int __init pem_pmu_init(void)
{
	int ret;

	ret = cpuhp_setup_state_multi(CPUHP_AP_PERF_ARM_MARVELL_PEM_ONLINE,
				      "perf/marvell/pem:online", NULL,
				       pem_pmu_offline_cpu);
	if (ret)
		return ret;

	ret = platform_driver_register(&pem_pmu_driver);
	if (ret)
		cpuhp_remove_multi_state(CPUHP_AP_PERF_ARM_MARVELL_PEM_ONLINE);
	return ret;
}

static void __exit pem_pmu_exit(void)
{
	platform_driver_unregister(&pem_pmu_driver);
	cpuhp_remove_multi_state(CPUHP_AP_PERF_ARM_MARVELL_PEM_ONLINE);
}

module_init(pem_pmu_init);
module_exit(pem_pmu_exit);

MODULE_DESCRIPTION("Marvell PEM Perf driver");
MODULE_AUTHOR("Linu Cherian <lcherian@marvell.com>");
MODULE_LICENSE("GPL v2");
