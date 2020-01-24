/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <stdio.h>
#include <string.h>

#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_memzone.h>
#include <rte_atomic.h>
#include <rte_timer.h>
#include <rte_cycles.h>
#include <rte_mempool.h>
#include <rte_random.h>

#include "test.h"
#include "process.h"

#define NUM_TIMERS		(1 << 20) /* ~1M timers */
#define NUM_LCORES_NEEDED	3
#define TEST_INFO_MZ_NAME	"test_timer_info_mz"
#define MSECPERSEC		1E3

#define launch_proc(ARGV) process_dup(ARGV, RTE_DIM(ARGV), __func__)

struct test_info {
	unsigned int mstr_lcore;
	unsigned int mgr_lcore;
	unsigned int sec_lcore;
	uint32_t timer_data_id;
	volatile int expected_count;
	volatile int expired_count;
	struct rte_mempool *tim_mempool;
	struct rte_timer *expired_timers[NUM_TIMERS];
	int expired_timers_idx;
	volatile int exit_flag;
};

static int
timer_secondary_spawn_wait(unsigned int lcore)
{
	char coremask[10];
#ifdef RTE_EXEC_ENV_LINUXAPP
	char tmp[PATH_MAX] = {0};
	char prefix[PATH_MAX] = {0};

	get_current_prefix(tmp, sizeof(tmp));

	snprintf(prefix, sizeof(prefix), "--file-prefix=%s", tmp);
#else
	const char *prefix = "";
#endif
	char const *argv[] = {
		prgname,
		"-c", coremask,
		"--proc-type=secondary",
		prefix
	};

	snprintf(coremask, sizeof(coremask), "%x", (1 << lcore));

	return launch_proc(argv);
}

static void
handle_expired_timer(struct rte_timer *tim)
{
	struct test_info *test_info = tim->arg;

	test_info->expired_count++;
	test_info->expired_timers[test_info->expired_timers_idx++] = tim;
}

static int
timer_manage_loop(void *arg)
{
#define TICK_MSECS 1
	uint64_t tick_cycles = TICK_MSECS * rte_get_timer_hz() / MSECPERSEC;
	uint64_t prev_tsc = 0, cur_tsc, diff_tsc;
	struct test_info *test_info = arg;

	while (!test_info->exit_flag) {
		cur_tsc = rte_rdtsc();
		diff_tsc = cur_tsc - prev_tsc;

		if (diff_tsc > tick_cycles) {
			/* Scan timer list for expired timers */
			rte_timer_alt_manage(test_info->timer_data_id,
					     NULL,
					     0,
					     handle_expired_timer);

			/* Return expired timer objects back to mempool */
			rte_mempool_put_bulk(test_info->tim_mempool,
					     (void **)test_info->expired_timers,
					     test_info->expired_timers_idx);

			test_info->expired_timers_idx = 0;

			prev_tsc = cur_tsc;
		}

		rte_pause();
	}

	return 0;
}

int
test_timer_secondary(void)
{
	int proc_type = rte_eal_process_type();
	const struct rte_memzone *mz;
	struct test_info *test_info;
	int ret;

	if (proc_type == RTE_PROC_PRIMARY) {
		if (rte_lcore_count() < NUM_LCORES_NEEDED) {
			printf("Not enough cores for test_timer_secondary, expecting at least %u\n",
			       NUM_LCORES_NEEDED);
			return TEST_SKIPPED;
		}

		mz = rte_memzone_reserve(TEST_INFO_MZ_NAME, sizeof(*test_info),
					 SOCKET_ID_ANY, 0);
		test_info = mz->addr;
		TEST_ASSERT_NOT_NULL(test_info, "Couldn't allocate memory for "
				     "test data");

		test_info->tim_mempool = rte_mempool_create("test_timer_mp",
				NUM_TIMERS, sizeof(struct rte_timer), 0, 0,
				NULL, NULL, NULL, NULL, rte_socket_id(), 0);

		ret = rte_timer_data_alloc(&test_info->timer_data_id);
		TEST_ASSERT_SUCCESS(ret, "Failed to allocate timer data "
				    "instance");

		unsigned int *mstr_lcorep = &test_info->mstr_lcore;
		unsigned int *mgr_lcorep = &test_info->mgr_lcore;
		unsigned int *sec_lcorep = &test_info->sec_lcore;

		*mstr_lcorep = rte_get_master_lcore();
		*mgr_lcorep = rte_get_next_lcore(*mstr_lcorep, 1, 1);
		*sec_lcorep = rte_get_next_lcore(*mgr_lcorep, 1, 1);

		ret = rte_eal_remote_launch(timer_manage_loop,
					    (void *)test_info,
					    *mgr_lcorep);
		TEST_ASSERT_SUCCESS(ret, "Failed to launch timer manage loop");

		ret = timer_secondary_spawn_wait(*sec_lcorep);
		TEST_ASSERT_SUCCESS(ret, "Secondary process execution failed");

		rte_delay_ms(2000);

		test_info->exit_flag = 1;
		rte_eal_wait_lcore(*mgr_lcorep);

#ifdef RTE_LIBRTE_TIMER_DEBUG
		rte_timer_alt_dump_stats(test_info->timer_data_id, stdout);
#endif

		return test_info->expected_count == test_info->expired_count ?
			TEST_SUCCESS : TEST_FAILED;

	} else if (proc_type == RTE_PROC_SECONDARY) {
		uint64_t ticks, timeout_ms;
		struct rte_timer *tim;
		int i;

		mz = rte_memzone_lookup(TEST_INFO_MZ_NAME);
		test_info = mz->addr;
		TEST_ASSERT_NOT_NULL(test_info, "Couldn't lookup memzone for "
				     "test info");

		for (i = 0; i < NUM_TIMERS; i++) {
			rte_mempool_get(test_info->tim_mempool, (void **)&tim);

			rte_timer_init(tim);

			/* generate timeouts between 10 and 160 ms */
			timeout_ms = ((rte_rand() & 0xF) + 1) * 10;
			ticks = timeout_ms * rte_get_timer_hz() / MSECPERSEC;

			ret = rte_timer_alt_reset(test_info->timer_data_id,
						  tim, ticks, SINGLE,
						  test_info->mgr_lcore, NULL,
						  test_info);
			if (ret < 0)
				return TEST_FAILED;

			test_info->expected_count++;

			/* randomly leave timer running or stop it */
			if (rte_rand() & 1)
				continue;

			ret = rte_timer_alt_stop(test_info->timer_data_id,
						 tim);
			if (ret == 0) {
				test_info->expected_count--;
				rte_mempool_put(test_info->tim_mempool,
						(void *)tim);
			}

		}

		return TEST_SUCCESS;
	}

	return TEST_FAILED;
}

REGISTER_TEST_COMMAND(timer_secondary_autotest, test_timer_secondary);
