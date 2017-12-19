/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include <sys/types.h>

#include <rte_log.h>
#include <rte_power.h>
#include <rte_spinlock.h>

#include "power_manager.h"

#define RTE_LOGTYPE_POWER_MANAGER RTE_LOGTYPE_USER1

#define POWER_SCALE_CORE(DIRECTION, core_num , ret) do { \
	if (core_num >= POWER_MGR_MAX_CPUS) \
		return -1; \
	if (!(global_enabled_cpus & (1ULL << core_num))) \
		return -1; \
	rte_spinlock_lock(&global_core_freq_info[core_num].power_sl); \
	ret = rte_power_freq_##DIRECTION(core_num); \
	rte_spinlock_unlock(&global_core_freq_info[core_num].power_sl); \
} while (0)

#define POWER_SCALE_MASK(DIRECTION, core_mask, ret) do { \
	int i; \
	for (i = 0; core_mask; core_mask &= ~(1 << i++)) { \
		if ((core_mask >> i) & 1) { \
			if (!(global_enabled_cpus & (1ULL << i))) \
				continue; \
			rte_spinlock_lock(&global_core_freq_info[i].power_sl); \
			if (rte_power_freq_##DIRECTION(i) != 1) \
				ret = -1; \
			rte_spinlock_unlock(&global_core_freq_info[i].power_sl); \
		} \
	} \
} while (0)

struct freq_info {
	rte_spinlock_t power_sl;
	uint32_t freqs[RTE_MAX_LCORE_FREQS];
	unsigned num_freqs;
} __rte_cache_aligned;

static struct freq_info global_core_freq_info[POWER_MGR_MAX_CPUS];

static uint64_t global_enabled_cpus;

#define SYSFS_CPU_PATH "/sys/devices/system/cpu/cpu%u/topology/core_id"

static unsigned
set_host_cpus_mask(void)
{
	char path[PATH_MAX];
	unsigned i;
	unsigned num_cpus = 0;

	for (i = 0; i < POWER_MGR_MAX_CPUS; i++) {
		snprintf(path, sizeof(path), SYSFS_CPU_PATH, i);
		if (access(path, F_OK) == 0) {
			global_enabled_cpus |= 1ULL << i;
			num_cpus++;
		} else
			return num_cpus;
	}
	return num_cpus;
}

int
power_manager_init(void)
{
	unsigned int i, num_cpus, num_freqs;
	uint64_t cpu_mask;
	int ret = 0;

	num_cpus = set_host_cpus_mask();
	if (num_cpus == 0) {
		RTE_LOG(ERR, POWER_MANAGER, "Unable to detected host CPUs, please "
			"ensure that sufficient privileges exist to inspect sysfs\n");
		return -1;
	}
	rte_power_set_env(PM_ENV_ACPI_CPUFREQ);
	cpu_mask = global_enabled_cpus;
	for (i = 0; cpu_mask; cpu_mask &= ~(1 << i++)) {
		if (rte_power_init(i) < 0)
			RTE_LOG(ERR, POWER_MANAGER,
					"Unable to initialize power manager "
					"for core %u\n", i);
		num_freqs = rte_power_freqs(i, global_core_freq_info[i].freqs,
					RTE_MAX_LCORE_FREQS);
		if (num_freqs == 0) {
			RTE_LOG(ERR, POWER_MANAGER,
				"Unable to get frequency list for core %u\n",
				i);
			global_enabled_cpus &= ~(1 << i);
			num_cpus--;
			ret = -1;
		}
		global_core_freq_info[i].num_freqs = num_freqs;
		rte_spinlock_init(&global_core_freq_info[i].power_sl);
	}
	RTE_LOG(INFO, POWER_MANAGER, "Detected %u host CPUs , enabled core mask:"
					" 0x%"PRIx64"\n", num_cpus, global_enabled_cpus);
	return ret;

}

uint32_t
power_manager_get_current_frequency(unsigned core_num)
{
	uint32_t freq, index;

	if (core_num >= POWER_MGR_MAX_CPUS) {
		RTE_LOG(ERR, POWER_MANAGER, "Core(%u) is out of range 0...%d\n",
				core_num, POWER_MGR_MAX_CPUS-1);
		return -1;
	}
	if (!(global_enabled_cpus & (1ULL << core_num)))
		return 0;

	rte_spinlock_lock(&global_core_freq_info[core_num].power_sl);
	index = rte_power_get_freq(core_num);
	rte_spinlock_unlock(&global_core_freq_info[core_num].power_sl);
	if (index >= POWER_MGR_MAX_CPUS)
		freq = 0;
	else
		freq = global_core_freq_info[core_num].freqs[index];

	return freq;
}

int
power_manager_exit(void)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; global_enabled_cpus; global_enabled_cpus &= ~(1 << i++)) {
		if (rte_power_exit(i) < 0) {
			RTE_LOG(ERR, POWER_MANAGER, "Unable to shutdown power manager "
					"for core %u\n", i);
			ret = -1;
		}
	}
	global_enabled_cpus = 0;
	return ret;
}

int
power_manager_scale_mask_up(uint64_t core_mask)
{
	int ret = 0;

	POWER_SCALE_MASK(up, core_mask, ret);
	return ret;
}

int
power_manager_scale_mask_down(uint64_t core_mask)
{
	int ret = 0;

	POWER_SCALE_MASK(down, core_mask, ret);
	return ret;
}

int
power_manager_scale_mask_min(uint64_t core_mask)
{
	int ret = 0;

	POWER_SCALE_MASK(min, core_mask, ret);
	return ret;
}

int
power_manager_scale_mask_max(uint64_t core_mask)
{
	int ret = 0;

	POWER_SCALE_MASK(max, core_mask, ret);
	return ret;
}

int
power_manager_enable_turbo_mask(uint64_t core_mask)
{
	int ret = 0;

	POWER_SCALE_MASK(enable_turbo, core_mask, ret);
	return ret;
}

int
power_manager_disable_turbo_mask(uint64_t core_mask)
{
	int ret = 0;

	POWER_SCALE_MASK(disable_turbo, core_mask, ret);
	return ret;
}

int
power_manager_scale_core_up(unsigned core_num)
{
	int ret = 0;

	POWER_SCALE_CORE(up, core_num, ret);
	return ret;
}

int
power_manager_scale_core_down(unsigned core_num)
{
	int ret = 0;

	POWER_SCALE_CORE(down, core_num, ret);
	return ret;
}

int
power_manager_scale_core_min(unsigned core_num)
{
	int ret = 0;

	POWER_SCALE_CORE(min, core_num, ret);
	return ret;
}

int
power_manager_scale_core_max(unsigned core_num)
{
	int ret = 0;

	POWER_SCALE_CORE(max, core_num, ret);
	return ret;
}

int
power_manager_enable_turbo_core(unsigned int core_num)
{
	int ret = 0;

	POWER_SCALE_CORE(enable_turbo, core_num, ret);
	return ret;
}

int
power_manager_disable_turbo_core(unsigned int core_num)
{
	int ret = 0;

	POWER_SCALE_CORE(disable_turbo, core_num, ret);
	return ret;
}

int
power_manager_scale_core_med(unsigned int core_num)
{
	int ret = 0;

	if (core_num >= POWER_MGR_MAX_CPUS)
		return -1;
	if (!(global_enabled_cpus & (1ULL << core_num)))
		return -1;
	rte_spinlock_lock(&global_core_freq_info[core_num].power_sl);
	ret = rte_power_set_freq(core_num,
				global_core_freq_info[core_num].num_freqs / 2);
	rte_spinlock_unlock(&global_core_freq_info[core_num].power_sl);
	return ret;
}
