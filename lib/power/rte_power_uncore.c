/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 * Copyright(c) 2023 AMD Corporation
 */

#include <errno.h>

#include <rte_errno.h>
#include <rte_spinlock.h>

#include "rte_power_uncore.h"
#include "power_intel_uncore.h"

enum rte_uncore_power_mgmt_env default_uncore_env = RTE_UNCORE_PM_ENV_NOT_SET;

static rte_spinlock_t global_env_cfg_lock = RTE_SPINLOCK_INITIALIZER;

static uint32_t
power_get_dummy_uncore_freq(unsigned int pkg __rte_unused,
	       unsigned int die __rte_unused)
{
	return 0;
}

static int
power_set_dummy_uncore_freq(unsigned int pkg __rte_unused,
	       unsigned int die __rte_unused, uint32_t index __rte_unused)
{
	return 0;
}

static int
power_dummy_uncore_freq_max(unsigned int pkg __rte_unused,
	       unsigned int die __rte_unused)
{
	return 0;
}

static int
power_dummy_uncore_freq_min(unsigned int pkg __rte_unused,
	       unsigned int die __rte_unused)
{
	return 0;
}

static int
power_dummy_uncore_freqs(unsigned int pkg __rte_unused, unsigned int die __rte_unused,
		uint32_t *freqs __rte_unused, uint32_t num __rte_unused)
{
	return 0;
}

static int
power_dummy_uncore_get_num_freqs(unsigned int pkg __rte_unused,
	       unsigned int die __rte_unused)
{
	return 0;
}

static unsigned int
power_dummy_uncore_get_num_pkgs(void)
{
	return 0;
}

static unsigned int
power_dummy_uncore_get_num_dies(unsigned int pkg __rte_unused)
{
	return 0;
}

/* function pointers */
rte_power_get_uncore_freq_t rte_power_get_uncore_freq = power_get_dummy_uncore_freq;
rte_power_set_uncore_freq_t rte_power_set_uncore_freq = power_set_dummy_uncore_freq;
rte_power_uncore_freq_change_t rte_power_uncore_freq_max = power_dummy_uncore_freq_max;
rte_power_uncore_freq_change_t rte_power_uncore_freq_min = power_dummy_uncore_freq_min;
rte_power_uncore_freqs_t rte_power_uncore_freqs = power_dummy_uncore_freqs;
rte_power_uncore_get_num_freqs_t rte_power_uncore_get_num_freqs = power_dummy_uncore_get_num_freqs;
rte_power_uncore_get_num_pkgs_t rte_power_uncore_get_num_pkgs = power_dummy_uncore_get_num_pkgs;
rte_power_uncore_get_num_dies_t rte_power_uncore_get_num_dies = power_dummy_uncore_get_num_dies;

static void
reset_power_uncore_function_ptrs(void)
{
	rte_power_get_uncore_freq = power_get_dummy_uncore_freq;
	rte_power_set_uncore_freq = power_set_dummy_uncore_freq;
	rte_power_uncore_freq_max = power_dummy_uncore_freq_max;
	rte_power_uncore_freq_min = power_dummy_uncore_freq_min;
	rte_power_uncore_freqs  = power_dummy_uncore_freqs;
	rte_power_uncore_get_num_freqs = power_dummy_uncore_get_num_freqs;
	rte_power_uncore_get_num_pkgs = power_dummy_uncore_get_num_pkgs;
	rte_power_uncore_get_num_dies = power_dummy_uncore_get_num_dies;
}

int
rte_power_set_uncore_env(enum rte_uncore_power_mgmt_env env)
{
	int ret;

	rte_spinlock_lock(&global_env_cfg_lock);

	if (default_uncore_env != RTE_UNCORE_PM_ENV_NOT_SET) {
		RTE_LOG(ERR, POWER, "Uncore Power Management Env already set.\n");
		rte_spinlock_unlock(&global_env_cfg_lock);
		return -1;
	}

	ret = 0;
	if (env == RTE_UNCORE_PM_ENV_INTEL_UNCORE) {
		rte_power_get_uncore_freq = power_get_intel_uncore_freq;
		rte_power_set_uncore_freq = power_set_intel_uncore_freq;
		rte_power_uncore_freq_min  = power_intel_uncore_freq_min;
		rte_power_uncore_freq_max  = power_intel_uncore_freq_max;
		rte_power_uncore_freqs = power_intel_uncore_freqs;
		rte_power_uncore_get_num_freqs = power_intel_uncore_get_num_freqs;
		rte_power_uncore_get_num_pkgs = power_intel_uncore_get_num_pkgs;
		rte_power_uncore_get_num_dies = power_intel_uncore_get_num_dies;
	} else {
		RTE_LOG(ERR, POWER, "Invalid Power Management Environment(%d) set\n", env);
		ret = -1;
		goto out;
	}

	default_uncore_env = env;
out:
	rte_spinlock_unlock(&global_env_cfg_lock);
	return ret;
}

void
rte_power_unset_uncore_env(void)
{
	rte_spinlock_lock(&global_env_cfg_lock);
	default_uncore_env = RTE_UNCORE_PM_ENV_NOT_SET;
	reset_power_uncore_function_ptrs();
	rte_spinlock_unlock(&global_env_cfg_lock);
}

enum rte_uncore_power_mgmt_env
rte_power_get_uncore_env(void)
{
	return default_uncore_env;
}

int
rte_power_uncore_init(unsigned int pkg, unsigned int die)
{
	int ret = -1;

	switch (default_uncore_env) {
	case RTE_UNCORE_PM_ENV_INTEL_UNCORE:
		return power_intel_uncore_init(pkg, die);
	default:
		RTE_LOG(INFO, POWER, "Uncore Env isn't set yet!\n");
		break;
	}

	/* Auto detect Environment */
	RTE_LOG(INFO, POWER, "Attempting to initialise Intel Uncore power mgmt...\n");
	ret = power_intel_uncore_init(pkg, die);
	if (ret == 0) {
		rte_power_set_uncore_env(RTE_UNCORE_PM_ENV_INTEL_UNCORE);
		goto out;
	}

	if (default_uncore_env == RTE_UNCORE_PM_ENV_NOT_SET) {
		RTE_LOG(ERR, POWER, "Unable to set Power Management Environment "
			"for package %u Die %u\n", pkg, die);
		ret = 0;
	}
out:
	return ret;
}

int
rte_power_uncore_exit(unsigned int pkg, unsigned int die)
{
	switch (default_uncore_env) {
	case RTE_UNCORE_PM_ENV_INTEL_UNCORE:
		return power_intel_uncore_exit(pkg, die);
	default:
		RTE_LOG(ERR, POWER, "Uncore Env has not been set, unable to exit gracefully\n");
		break;
	}
	return -1;
}
