/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include <errno.h>

#include <eal_export.h>
#include "rte_cpuflags.h"
#include "rte_power_intrinsics.h"

/**
 *  Set wfet_en if WFET is supported
 */
#ifdef RTE_ARCH_64
static uint8_t wfet_en;
#endif /* RTE_ARCH_64 */

RTE_INIT(rte_power_intrinsics_init)
{
#ifdef RTE_ARCH_64
	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_WFXT))
		wfet_en = 1;
#endif /* RTE_ARCH_64 */
}

/**
 * This function uses WFE/WFET instruction to make lcore suspend
 * execution on ARM.
 */
RTE_EXPORT_SYMBOL(rte_power_monitor)
int
rte_power_monitor(const struct rte_power_monitor_cond *pmc,
		const uint64_t tsc_timestamp)
{
#ifdef RTE_ARCH_64
	const unsigned int lcore_id = rte_lcore_id();
	uint64_t cur_value;

	/* prevent non-EAL thread from using this API */
	if (lcore_id >= RTE_MAX_LCORE)
		return -EINVAL;

	if (pmc == NULL)
		return -EINVAL;

	if (pmc->fn == NULL)
		return -EINVAL;

	switch (pmc->size) {
	case sizeof(uint8_t):
		__RTE_ARM_LOAD_EXC_8(pmc->addr, cur_value, rte_memory_order_relaxed);
		break;
	case sizeof(uint16_t):
		__RTE_ARM_LOAD_EXC_16(pmc->addr, cur_value, rte_memory_order_relaxed);
		break;
	case sizeof(uint32_t):
		__RTE_ARM_LOAD_EXC_32(pmc->addr, cur_value, rte_memory_order_relaxed);
		break;
	case sizeof(uint64_t):
		__RTE_ARM_LOAD_EXC_64(pmc->addr, cur_value, rte_memory_order_relaxed);
		break;
	default:
		return -EINVAL; /* unexpected size */
	}

	if (wfet_en)
		__RTE_ARM_WFET(tsc_timestamp)
	else
		__RTE_ARM_WFE()

	return 0;
#else
	RTE_SET_USED(pmc);
	RTE_SET_USED(tsc_timestamp);

	return -ENOTSUP;
#endif /* RTE_ARCH_64 */
}

/**
 * This function is not supported on ARM.
 */
RTE_EXPORT_SYMBOL(rte_power_pause)
int
rte_power_pause(const uint64_t tsc_timestamp)
{
	RTE_SET_USED(tsc_timestamp);

	return -ENOTSUP;
}

/**
 * This function uses SEV instruction to wake up all cores
 * on ARM.
 * Note that lcore_id is not used here.
 */
RTE_EXPORT_SYMBOL(rte_power_monitor_wakeup)
int
rte_power_monitor_wakeup(const unsigned int lcore_id)
{
	RTE_SET_USED(lcore_id);

#ifdef RTE_ARCH_64
	__RTE_ARM_SEV()
	return 0;
#else
	return -ENOTSUP;
#endif /* RTE_ARCH_64 */
}

RTE_EXPORT_SYMBOL(rte_power_monitor_multi)
int
rte_power_monitor_multi(const struct rte_power_monitor_cond pmc[],
		const uint32_t num, const uint64_t tsc_timestamp)
{
	RTE_SET_USED(pmc);
	RTE_SET_USED(num);
	RTE_SET_USED(tsc_timestamp);

	return -ENOTSUP;
}
