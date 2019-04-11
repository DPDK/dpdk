/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <unistd.h>
#include <fcntl.h>
#include <rte_log.h>

#include "oob_monitor.h"
#include "power_manager.h"
#include "channel_manager.h"

static volatile unsigned run_loop = 1;
static uint64_t g_branches, g_branch_misses;
static int g_active;

void branch_monitor_exit(void)
{
	run_loop = 0;
}

/* Number of microseconds between each poll */
#define INTERVAL 100
#define PRINT_LOOP_COUNT (1000000/INTERVAL)
#define IA32_PERFEVTSEL0 0x186
#define IA32_PERFEVTSEL1 0x187
#define IA32_PERFCTR0 0xc1
#define IA32_PERFCTR1 0xc2
#define IA32_PERFEVT_BRANCH_HITS 0x05300c4
#define IA32_PERFEVT_BRANCH_MISS 0x05300c5

static float
apply_policy(int core)
{
	struct core_info *ci;
	uint64_t counter = 0;
	uint64_t branches, branch_misses;
	uint64_t last_branches, last_branch_misses;
	int64_t hits_diff, miss_diff;
	float ratio;
	int ret;

	g_active = 0;
	ci = get_core_info();

	last_branches = ci->cd[core].last_branches;
	last_branch_misses = ci->cd[core].last_branch_misses;

	ret = pread(ci->cd[core].msr_fd, &counter,
			sizeof(counter), IA32_PERFCTR0);
	if (ret < 0)
		RTE_LOG(ERR, POWER_MANAGER,
				"unable to read counter for core %u\n",
				core);
	branches = counter;

	counter = 0;
	ret = pread(ci->cd[core].msr_fd, &counter,
			sizeof(counter), IA32_PERFCTR1);
	if (ret < 0)
		RTE_LOG(ERR, POWER_MANAGER,
				"unable to read counter for core %u\n",
				core);
	branch_misses = counter;


	ci->cd[core].last_branches = branches;
	ci->cd[core].last_branch_misses = branch_misses;

	/*
	 * Intentional right shift to make MSB 0 to avoid
	 * possible signed overflow or truncation.
	 */
	branches >>= 1;
	last_branches >>= 1;
	hits_diff = (int64_t)branches - (int64_t)last_branches;
	if (hits_diff <= 0) {
		/* Likely a counter overflow condition, skip this round */
		return -1.0;
	}

	/*
	 * Intentional right shift to make MSB 0 to avoid
	 * possible signed overflow or truncation.
	 */
	branch_misses >>= 1;
	last_branch_misses >>= 1;
	miss_diff = (int64_t)branch_misses - (int64_t)last_branch_misses;
	if (miss_diff <= 0) {
		/* Likely a counter overflow condition, skip this round */
		return -1.0;
	}

	g_branches = hits_diff;
	g_branch_misses = miss_diff;

	if (hits_diff < (INTERVAL*100)) {
		/* Likely no workload running on this core. Skip. */
		return -1.0;
	}

	ratio = (float)miss_diff * (float)100 / (float)hits_diff;

	if (ratio < ci->branch_ratio_threshold)
		power_manager_scale_core_min(core);
	else
		power_manager_scale_core_max(core);

	g_active = 1;
	return ratio;
}

int
add_core_to_monitor(int core)
{
	struct core_info *ci;
	char proc_file[UNIX_PATH_MAX];
	int ret;

	ci = get_core_info();

	if (core < ci->core_count) {
		long setup;

		snprintf(proc_file, UNIX_PATH_MAX, "/dev/cpu/%d/msr", core);
		ci->cd[core].msr_fd = open(proc_file, O_RDWR | O_SYNC);
		if (ci->cd[core].msr_fd < 0) {
			RTE_LOG(ERR, POWER_MANAGER,
					"Error opening MSR file for core %d "
					"(is msr kernel module loaded?)\n",
					core);
			return -1;
		}
		/*
		 * Set up branch counters
		 */
		setup = IA32_PERFEVT_BRANCH_HITS;
		ret = pwrite(ci->cd[core].msr_fd, &setup,
				sizeof(setup), IA32_PERFEVTSEL0);
		if (ret < 0) {
			RTE_LOG(ERR, POWER_MANAGER,
					"unable to set counter for core %u\n",
					core);
			return ret;
		}
		setup = IA32_PERFEVT_BRANCH_MISS;
		ret = pwrite(ci->cd[core].msr_fd, &setup,
				sizeof(setup), IA32_PERFEVTSEL1);
		if (ret < 0) {
			RTE_LOG(ERR, POWER_MANAGER,
					"unable to set counter for core %u\n",
					core);
			return ret;
		}
		/*
		 * Close the file and re-open as read only so
		 * as not to hog the resource
		 */
		close(ci->cd[core].msr_fd);
		ci->cd[core].msr_fd = open(proc_file, O_RDONLY);
		if (ci->cd[core].msr_fd < 0) {
			RTE_LOG(ERR, POWER_MANAGER,
					"Error opening MSR file for core %d "
					"(is msr kernel module loaded?)\n",
					core);
			return -1;
		}
		ci->cd[core].oob_enabled = 1;
	}
	return 0;
}

int
remove_core_from_monitor(int core)
{
	struct core_info *ci;
	char proc_file[UNIX_PATH_MAX];
	int ret;

	ci = get_core_info();

	if (ci->cd[core].oob_enabled) {
		long setup;

		/*
		 * close the msr file, then reopen rw so we can
		 * disable the counters
		 */
		if (ci->cd[core].msr_fd != 0)
			close(ci->cd[core].msr_fd);
		snprintf(proc_file, UNIX_PATH_MAX, "/dev/cpu/%d/msr", core);
		ci->cd[core].msr_fd = open(proc_file, O_RDWR | O_SYNC);
		if (ci->cd[core].msr_fd < 0) {
			RTE_LOG(ERR, POWER_MANAGER,
					"Error opening MSR file for core %d "
					"(is msr kernel module loaded?)\n",
					core);
			return -1;
		}
		setup = 0x0; /* clear event */
		ret = pwrite(ci->cd[core].msr_fd, &setup,
				sizeof(setup), IA32_PERFEVTSEL0);
		if (ret < 0) {
			RTE_LOG(ERR, POWER_MANAGER,
					"unable to set counter for core %u\n",
					core);
			return ret;
		}
		setup = 0x0; /* clear event */
		ret = pwrite(ci->cd[core].msr_fd, &setup,
				sizeof(setup), IA32_PERFEVTSEL1);
		if (ret < 0) {
			RTE_LOG(ERR, POWER_MANAGER,
					"unable to set counter for core %u\n",
					core);
			return ret;
		}

		close(ci->cd[core].msr_fd);
		ci->cd[core].msr_fd = 0;
		ci->cd[core].oob_enabled = 0;
	}
	return 0;
}

int
branch_monitor_init(void)
{
	return 0;
}

void
run_branch_monitor(void)
{
	struct core_info *ci;
	int print = 0;
	float ratio;
	int printed;
	int reads = 0;

	ci = get_core_info();

	while (run_loop) {

		if (!run_loop)
			break;
		usleep(INTERVAL);
		int j;
		print++;
		printed = 0;
		for (j = 0; j < ci->core_count; j++) {
			if (ci->cd[j].oob_enabled) {
				ratio = apply_policy(j);
				if ((print > PRINT_LOOP_COUNT) && (g_active)) {
					printf("  %d: %.4f {%lu} {%d}", j,
							ratio, g_branches,
							reads);
					printed = 1;
					reads = 0;
				} else {
					reads++;
				}
			}
		}
		if (print > PRINT_LOOP_COUNT) {
			if (printed)
				printf("\n");
			print = 0;
		}
	}
}
