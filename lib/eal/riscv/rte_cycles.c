/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Cavium, Inc
 * Copyright(c) 2022 StarFive
 * Copyright(c) 2022 SiFive
 * Copyright(c) 2022 Semihalf
 */

#include <stdio.h>

#include "eal_private.h"
#include "rte_byteorder.h"
#include "rte_cycles.h"
#include "rte_log.h"

/** Read generic counter frequency */
static uint64_t
__rte_riscv_timefrq(void)
{
#define TIMEBASE_FREQ_SIZE	8
	if (RTE_RISCV_TIME_FREQ > 0)
		return RTE_RISCV_TIME_FREQ;
	uint8_t buf[TIMEBASE_FREQ_SIZE];
	ssize_t cnt;
	FILE *file;

	file = fopen("/proc/device-tree/cpus/timebase-frequency", "rb");
	if (!file)
		goto fail;

	cnt = fread(buf, 1, TIMEBASE_FREQ_SIZE, file);
	fclose(file);
	switch (cnt) {
	case 8:
		return rte_be_to_cpu_64(*(uint64_t *)buf);
	case 4:
		return rte_be_to_cpu_32(*(uint32_t *)buf);
	default:
		break;
	}
fail:
	EAL_LOG(WARNING, "Unable to read timebase-frequency from FDT.");
	return 0;
}

uint64_t
get_tsc_freq_arch(void)
{
	EAL_LOG(NOTICE, "TSC using RISC-V %s.",
		RTE_RISCV_RDTSC_USE_HPM ? "rdcycle" : "rdtime");
	if (!RTE_RISCV_RDTSC_USE_HPM)
		return __rte_riscv_timefrq();
#define CYC_PER_1MHZ 1E6
	/*
	 * Use real time clock to estimate current cycle frequency
	 */
	uint64_t ticks, frq;
	uint64_t start_ticks, cur_ticks;
	uint64_t start_cycle, end_cycle;

	/* Do not proceed unless clock frequency can be obtained. */
	frq = __rte_riscv_timefrq();
	if (!frq)
		return 0;

	/* Number of ticks for 1/10 second */
	ticks = frq / 10;

	start_ticks = __rte_riscv_rdtime_precise();
	start_cycle = rte_rdtsc_precise();
	do {
		cur_ticks = __rte_riscv_rdtime();
	} while ((cur_ticks - start_ticks) < ticks);
	end_cycle = rte_rdtsc_precise();

	/* Adjust the cycles to next 1Mhz */
	return RTE_ALIGN_MUL_CEIL((end_cycle - start_cycle) * 10, CYC_PER_1MHZ);
}
