// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2022 Marvell.

#ifndef __LINUX_ARM_MPAM_FIXUPS_H
#define __LINUX_ARM_MPAM_FIXUPS_H

#include <linux/bitfield.h>
#include <linux/cpumask.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/module.h>

#ifndef CONFIG_ACPI_MPAM
static inline int acpi_pptt_get_cpumask_from_cache_id(u32 cache_id,
						      cpumask_t *cpus)
{
	return -EINVAL;
}

static inline int acpi_pptt_get_cpus_from_container(u32 acpi_cpu_id,
						    cpumask_t *cpus)
{
	return -EINVAL;
}
#endif

static inline void __iomem *
devm_platform_get_and_ioremap_resource(struct platform_device *pdev,
				unsigned int index, struct resource **res)
{
	struct resource *r;

	r = platform_get_resource(pdev, IORESOURCE_MEM, index);
	if (res)
		*res = r;
	return devm_ioremap_resource(&pdev->dev, r);
}

/* Empty macros to workaround the build issues in Linux 5.4 */

/*
 * TODO: Backport them from 5.15.
 */
#define lockdep_assert_preemption_enabled()				\
do {									\
} while (0)

#define lockdep_assert_preemption_disabled()				\
do {									\
} while (0)

#endif /* __LINUX_ARM_MPAM_FIXUPS_H */
