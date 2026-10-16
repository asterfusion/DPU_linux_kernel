// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Marvell International Ltd.
 */

#include <linux/edac.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>

#include "edac_module.h"

/* EDAC MC - EDAC memory controller driver */

#define MC_CTL0_REG		0x44
#define DATA_WIDTH(reg)		(((reg) >> 8) & 0x7)
#define DATA_WIDTH_X32		0x3
#define DATA_WIDTH_X64		0x4

#define RAS_CTRL_REG		0x4c
#define ECC_ENABLED		BIT(1)

#define INT_STAT_REG		0x140
#define ECC_INT_TRGD		BIT(12)

#define INT_ENA_REG		0x144
#define ECC_INT_ENABLED		BIT(12)

#define MMAP_REG(cs)		(0x200 + (cs) * 0x8)
#define CS_VALID		BIT(0)
#define AREA_LEN(reg)		(((reg) >> 16) & 0x1f)
#define AREA_LEN_768MB		0x1
#define AREA_LEN_1536MB		0x2
#define AREA_LEN_3GB		0x3
#define AREA_LEN_6GB		0x4
#define AREA_LEN_512MB		0xd
#define AREA_LEN_1GB		0xe
#define AREA_LEN_2GB		0xf
#define AREA_LEN_4GB		0x10
#define AREA_LEN_8GB		0x11
#define AREA_LEN_16GB		0x12

#define MC_CONFIG_REG(cs)	(0x220 + (cs) * 0x4)
#define DEV_TYPE(reg)		(((reg) >> 16) & 0x3)
#define DEV_TYPE_X8		0x1
#define DEV_TYPE_X16		0x2

#define MC_CTL2_REG		0x2c4
#define SDRAM_TYPE(reg)		(((reg) >> 4) & 0xf)
#define SDRAM_TYPE_DDR4		0x3
#define SDRAM_TYPE_LPDDR4	0xb

#define ECC_LOG_CFG_REG		0x360
#define ECC_1BIT_ERR_TH		1

#define ECC_1BIT_ERR_CNTR_REG	0x364
#define ECC_ERR_INFO_0_REG	0x368
#define ECC_2BIT_ERR		BIT(0)
#define ECC_ERR_CA(reg)		(((reg) >> 2) & 0xfff)
#define ECC_ERR_RA(reg)		(((reg) >> 14) & 0x3ffff)

#define ECC_ERR_INFO_1_REG	0x36c
#define ECC_ERR_SA(reg)		(((reg) >> 0) & 0x7)
#define ECC_ERR_BA(reg)		(((reg) >> 3) & 0x7)
#define ECC_ERR_BG(reg)		(((reg) >> 6) & 0x3)
#define ECC_ERR_CS(reg)		(((reg) >> 8) & 0xf)

#define ECC_TRIG_INT_REG	0xa60
#define ECC_INT_TRIGGERED	BIT(12)

#define SDRAM_CS_NUM		2

struct ap80x_mc_pdata {
	void __iomem *base;
	int irq;
	char msg[256];
};

static inline u32 ap80x_mc_ecc_enabled(void __iomem *base)
{
	return readl(base + RAS_CTRL_REG) & ECC_ENABLED;
}

static inline u32 ap80x_mc_cs_enabled(void __iomem *base, u32 cs)
{
	return readl(base + MMAP_REG(cs)) & CS_VALID;
}

static u32 ap80x_mc_cs_num_get(void __iomem *base)
{
	u32 cs, cs_num;

	for (cs = 0, cs_num = 0; cs < SDRAM_CS_NUM; cs++)
		if (ap80x_mc_cs_enabled(base, cs))
			cs_num++;

	return cs_num;
}

static u32 ap80x_mc_data_width_get(void __iomem *base)
{
	u32 reg, data_width;

	reg = readl(base + MC_CTL0_REG);

	switch (DATA_WIDTH(reg)) {
	case DATA_WIDTH_X32:
		data_width = 32;
		break;
	case DATA_WIDTH_X64:
		data_width = 64;
		break;
	default:
		data_width = 0;
	}

	return data_width;
}

static enum mem_type ap80x_mc_mtype_get(void __iomem *base)
{
	enum mem_type mtype;
	u32 reg;

	reg = readl(base + MC_CTL2_REG);

	switch (SDRAM_TYPE(reg)) {
	case SDRAM_TYPE_DDR4:
	case SDRAM_TYPE_LPDDR4:
		mtype = MEM_DDR4;
		break;
	default:
		mtype = MEM_UNKNOWN;
	}

	return mtype;
}

static enum dev_type ap80x_mc_dtype_get(void __iomem *base, u32 cs)
{
	enum dev_type dtype;
	u32 reg;

	reg = readl(base + MC_CONFIG_REG(cs));

	switch (DEV_TYPE(reg)) {
	case DEV_TYPE_X8:
		dtype = DEV_X8;
		break;
	case DEV_TYPE_X16:
		dtype = DEV_X16;
		break;
	default:
		dtype = DEV_UNKNOWN;
	}

	return dtype;
}

static u32 ap80x_mc_mem_size_get(void __iomem *base, u32 cs)
{
	u32 reg, mem_size;

	reg = readl(base + MMAP_REG(cs));

	switch (AREA_LEN(reg)) {
	case AREA_LEN_768MB:
		mem_size = 768;
		break;
	case AREA_LEN_1536MB:
		mem_size = 1536;
		break;
	case AREA_LEN_3GB:
		mem_size = 3072;
		break;
	case AREA_LEN_6GB:
		mem_size = 6144;
		break;
	case AREA_LEN_512MB:
		mem_size = 512;
		break;
	case AREA_LEN_1GB:
		mem_size = 1024;
		break;
	case AREA_LEN_2GB:
		mem_size = 2048;
		break;
	case AREA_LEN_4GB:
		mem_size = 4096;
		break;
	case AREA_LEN_8GB:
		mem_size = 8192;
		break;
	case AREA_LEN_16GB:
		mem_size = 16384;
		break;
	default:
		mem_size = 0;
	}

	return mem_size;
}

static void ap80x_mc_csrows_init(struct mem_ctl_info *mci)
{
	struct csrow_info *csrow;
	struct dimm_info *dimm;
	struct ap80x_mc_pdata *pdata = mci->pvt_info;
	u32 row, ch;
	u32 mem_size, data_width;

	for (row = 0; row < mci->nr_csrows; row++) {
		csrow = mci->csrows[row];

		/* find memory size per chip-select, MB */
		mem_size = ap80x_mc_mem_size_get(pdata->base, row);

		for (ch = 0; ch < csrow->nr_channels; ch++) {
			dimm = csrow->channels[ch]->dimm;
			dimm->edac_mode = EDAC_SECDED;

			/* set dram device type */
			dimm->dtype = ap80x_mc_dtype_get(pdata->base, row);

			/* set dram memory type */
			dimm->mtype = ap80x_mc_mtype_get(pdata->base);

			/* calculate number of pages */
			dimm->nr_pages = mem_size << (20 - PAGE_SHIFT);

			/* find ecc err grain based on dram data bus width */
			data_width = ap80x_mc_data_width_get(pdata->base);
			if (data_width)
				dimm->grain = (data_width == 64) ? 8 : 4;
		}
	}
}

#ifdef CONFIG_EDAC_DEBUG
static inline void ap80x_mc_trigger(void __iomem *base, unsigned long val)
{
	writel(ECC_INT_TRIGGERED, base + ECC_TRIG_INT_REG);
}

#define to_mci(k) container_of(k, struct mem_ctl_info, dev)

static ssize_t mc_trigger_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mem_ctl_info *mci = to_mci(dev);
	struct ap80x_mc_pdata *pdata = mci->pvt_info;
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	ap80x_mc_trigger(pdata->base, val);

	return count;
}

static DEVICE_ATTR_WO(mc_trigger);
#endif /* CONFIG_EDAC_DEBUG */

static struct attribute *ap80x_mc_dev_attrs[] = {
#ifdef CONFIG_EDAC_DEBUG
	&dev_attr_mc_trigger.attr,
#endif
	NULL
};
ATTRIBUTE_GROUPS(ap80x_mc_dev);

static void ap80x_mc_ecc_intr_enable(struct mem_ctl_info *mci)
{
	struct ap80x_mc_pdata *pdata = mci->pvt_info;
	u32 reg;

	reg = readl(pdata->base + INT_ENA_REG);

	/* set ecc 1-bit error threshold */
	writel(ECC_1BIT_ERR_TH, pdata->base + ECC_LOG_CFG_REG);

	/* enable ecc interrupt */
	writel(reg | ECC_INT_ENABLED, pdata->base + INT_ENA_REG);
}

static void ap80x_mc_ecc_intr_disable(struct mem_ctl_info *mci)
{
	struct ap80x_mc_pdata *pdata = mci->pvt_info;
	u32 reg;

	reg = readl(pdata->base + INT_ENA_REG);

	/* unset ecc 1-bit error threshold */
	writel(0, pdata->base + ECC_LOG_CFG_REG);

	/* disable ecc interrupt */
	writel(reg & ~ECC_INT_ENABLED, pdata->base + INT_ENA_REG);
}

static irqreturn_t ap80x_mc_isr(int irq, void *dev_id)
{
	struct mem_ctl_info *mci = dev_id;
	struct ap80x_mc_pdata *pdata = mci->pvt_info;
	u32 reg, info1, err_cnt;

	/* check interrupt status register for ecc interrupt */
	reg = readl(pdata->base + INT_STAT_REG);
	if (!(reg & ECC_INT_TRGD))
		return IRQ_NONE;

	/* handle ecc error */
	reg = readl(pdata->base + ECC_ERR_INFO_0_REG);
	info1 = readl(pdata->base + ECC_ERR_INFO_1_REG);
	err_cnt = readl(pdata->base + ECC_1BIT_ERR_CNTR_REG);

	snprintf(pdata->msg, sizeof(pdata->msg),
		 "cs %x, bg %x, ba %x, sa %x, ra %x, ca %x",
		 ECC_ERR_CS(info1),	/* chip-select */
		 ECC_ERR_BG(info1),	/* bank group */
		 ECC_ERR_BA(info1),	/* bank addr */
		 ECC_ERR_SA(info1),	/* stack addr */
		 ECC_ERR_RA(reg),	/* row addr */
		 ECC_ERR_CA(reg));	/* column addr */

	if (reg & ECC_2BIT_ERR)
		edac_mc_handle_error(HW_EVENT_ERR_UNCORRECTED, mci,
				     1, 0, 0, 0, 0, -1, -1,
				     pdata->msg, "");
	else
		edac_mc_handle_error(HW_EVENT_ERR_CORRECTED, mci,
				     err_cnt, 0, 0, 0, 0, -1, -1,
				     pdata->msg, "");

	/* reset ecc error info */
	writel(0, pdata->base + ECC_ERR_INFO_0_REG);
	writel(0, pdata->base + ECC_ERR_INFO_1_REG);

	/* reset ecc 1-bit error counter */
	writel(0, pdata->base + ECC_1BIT_ERR_CNTR_REG);

	/* clear ecc interrupt */
	writel(ECC_INT_TRGD, pdata->base + INT_STAT_REG);

	return IRQ_HANDLED;
}

static const struct of_device_id ap80x_mc_of_match[] = {
	{ .compatible = "marvell,armada-ap80x-edac-mc", },
	{},
};
MODULE_DEVICE_TABLE(of, ap80x_mc_of_match);

static int ap80x_mc_probe(struct platform_device *pdev)
{
	const struct of_device_id *id;
	struct edac_mc_layer layers[1];
	struct mem_ctl_info *mci;
	struct ap80x_mc_pdata *pdata;
	struct resource *res;
	void __iomem *base;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		edac_printk(KERN_ERR, EDAC_MC, "Failed to map regs\n");
		return PTR_ERR(base);
	}

	if (!ap80x_mc_ecc_enabled(base)) {
		edac_printk(KERN_INFO, EDAC_MC, "SDRAM ECC disabled\n");
		return -ENODEV;
	}

	/* define memory controller hierarchy */
	layers[0].type = EDAC_MC_LAYER_CHIP_SELECT;
	layers[0].size = ap80x_mc_cs_num_get(base);
	layers[0].is_virt_csrow = true;

	mci = edac_mc_alloc(0, ARRAY_SIZE(layers), layers, sizeof(*pdata));
	if (!mci)
		return -ENOMEM;

	mci->pdev = &pdev->dev;
	platform_set_drvdata(pdev, mci);

	pdata = mci->pvt_info;
	pdata->base = base;

	id = of_match_device(ap80x_mc_of_match, &pdev->dev);
	mci->mod_name = pdev->dev.driver->name;
	mci->ctl_name = id ? id->compatible : "unknown";
	mci->dev_name = dev_name(&pdev->dev);

	/* set memory controller capabilities and configuration */
	mci->mtype_cap = MEM_FLAG_DDR4;
	mci->edac_ctl_cap = EDAC_FLAG_NONE | EDAC_FLAG_SECDED;
	mci->edac_cap = EDAC_FLAG_SECDED;
	mci->scrub_mode = SCRUB_NONE;
	mci->ctl_page_to_phys = NULL;

	/* initialize per chip-select dram configuration */
	ap80x_mc_csrows_init(mci);

	pdata->irq = platform_get_irq(pdev, 0);
	if (pdata->irq < 0) {
		ret = pdata->irq;
		goto err_mc_free;
	}

	ret = devm_request_irq(&pdev->dev, pdata->irq,
			       ap80x_mc_isr, IRQF_SHARED,
			       dev_name(&pdev->dev), mci);
	if (ret < 0) {
		edac_printk(KERN_ERR, EDAC_MC, "Failed to allocate irq\n");
		ret = -ENODEV;
		goto err_mc_free;
	}

	ret = edac_mc_add_mc_with_groups(mci, ap80x_mc_dev_groups);
	if (ret < 0)
		goto err_mc_free;

	/* enable ecc errors handling */
	ap80x_mc_ecc_intr_enable(mci);

	return 0;

err_mc_free:
	edac_mc_free(mci);

	return ret;
}

static int ap80x_mc_remove(struct platform_device *pdev)
{
	struct mem_ctl_info *mci = platform_get_drvdata(pdev);

	ap80x_mc_ecc_intr_disable(mci);
	edac_mc_del_mc(mci->pdev);
	edac_mc_free(mci);

	return 0;
}

static struct platform_driver ap80x_mc_driver = {
	.probe = ap80x_mc_probe,
	.remove = ap80x_mc_remove,
	.driver = {
		.name = "armada-ap80x-edac-mc",
		.of_match_table = of_match_ptr(ap80x_mc_of_match),
	}
};

static struct platform_driver * const drivers[] = {
	&ap80x_mc_driver,
};

static int __init armada_ap80x_edac_init(void)
{
	int ret;

	/* only interrupt reporting method is supported */
	edac_op_state = EDAC_OPSTATE_INT;

	ret = platform_register_drivers(drivers, ARRAY_SIZE(drivers));
	if (ret)
		edac_printk(KERN_ERR, EDAC_MC,
			    "Failed to register Armada AP80x EDAC drivers\n");

	return 0;
}
module_init(armada_ap80x_edac_init);

static void __exit armada_ap80x_edac_exit(void)
{
	platform_unregister_drivers(drivers, ARRAY_SIZE(drivers));
}
module_exit(armada_ap80x_edac_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Victor Axelrod <victora@marvell.com>");
MODULE_DESCRIPTION("Marvell Armada AP80x EDAC drivers");
