// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 Arm Limited
 */

#define pr_fmt(fmt) "smccc: " fmt

#include <linux/cache.h>
#include <linux/init.h>
#include <linux/arm-smccc.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/sched/clock.h>
#include <asm/archrandom.h>
#include "smccc_trace.h"

static u32 smccc_version = ARM_SMCCC_VERSION_1_0;
static enum arm_smccc_conduit smccc_conduit = SMCCC_CONDUIT_NONE;

bool __ro_after_init smccc_trng_available = false;
u64 __ro_after_init smccc_has_sve_hint = false;

void __init arm_smccc_version_init(u32 version, enum arm_smccc_conduit conduit)
{
	smccc_version = version;
	smccc_conduit = conduit;

	smccc_trng_available = smccc_probe_trng();
	if (IS_ENABLED(CONFIG_ARM64_SVE) &&
	    smccc_version >= ARM_SMCCC_VERSION_1_3)
		smccc_has_sve_hint = true;
}

enum arm_smccc_conduit arm_smccc_1_1_get_conduit(void)
{
	if (smccc_version < ARM_SMCCC_VERSION_1_1)
		return SMCCC_CONDUIT_NONE;

	return smccc_conduit;
}
EXPORT_SYMBOL_GPL(arm_smccc_1_1_get_conduit);

u32 arm_smccc_get_version(void)
{
	return smccc_version;
}
EXPORT_SYMBOL_GPL(arm_smccc_get_version);

static int __init smccc_devices_init(void)
{
	struct platform_device *pdev;

	if (smccc_trng_available) {
		pdev = platform_device_register_simple("smccc_trng", -1,
						       NULL, 0);
		if (IS_ERR(pdev))
			pr_err("smccc_trng: could not register device: %ld\n",
			       PTR_ERR(pdev));
	}

	return 0;
}
device_initcall(smccc_devices_init);

void arm_smccc_smc(unsigned long a0, unsigned long a1, unsigned long a2, unsigned long a3,
		   unsigned long a4, unsigned long a5, unsigned long a6, unsigned long a7,
		   struct arm_smccc_res *res)
{
	u64 start, elapsed;

	trace_arm_smccc_smc_start(a0);
	start = local_clock();
	__arm_smccc_smc(a0, a1, a2, a3, a4, a5, a6, a7, res, NULL);
	elapsed = local_clock() - start;
	trace_arm_smccc_smc_end(a0, elapsed);
}
EXPORT_SYMBOL_GPL(arm_smccc_smc);
