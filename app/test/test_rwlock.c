/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/queue.h>
#include <string.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_rwlock.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_cycles.h>

#include "test.h"

/*
 * rwlock test
 * ===========
 * Provides UT for rte_rwlock API.
 * Main concern is on functional testing, but also provides some
 * performance measurements.
 * Obviously for proper testing need to be executed with more than one lcore.
 */

#define ITER_NUM	0x80

#define TEST_SEC	5

static rte_rwlock_t sl;
static rte_rwlock_t sl_tab[RTE_MAX_LCORE];
static uint32_t synchro;

enum {
	LC_TYPE_RDLOCK,
	LC_TYPE_WRLOCK,
};

static struct {
	rte_rwlock_t lock;
	uint64_t tick;

	volatile union {
		uint8_t u8[RTE_CACHE_LINE_SIZE];
		uint64_t u64[RTE_CACHE_LINE_SIZE / sizeof(uint64_t)];
	} data;
} __rte_cache_aligned try_rwlock_data;

struct try_rwlock_lcore {
	int32_t rc;
	int32_t type;
	struct {
		uint64_t tick;
		uint64_t fail;
		uint64_t success;
	} stat;
} __rte_cache_aligned;

static struct try_rwlock_lcore try_lcore_data[RTE_MAX_LCORE];

static int
test_rwlock_per_core(__rte_unused void *arg)
{
	rte_rwlock_write_lock(&sl);
	printf("Global write lock taken on core %u\n", rte_lcore_id());
	rte_rwlock_write_unlock(&sl);

	rte_rwlock_write_lock(&sl_tab[rte_lcore_id()]);
	printf("Hello from core %u !\n", rte_lcore_id());
	rte_rwlock_write_unlock(&sl_tab[rte_lcore_id()]);

	rte_rwlock_read_lock(&sl);
	printf("Global read lock taken on core %u\n", rte_lcore_id());
	rte_delay_ms(100);
	printf("Release global read lock on core %u\n", rte_lcore_id());
	rte_rwlock_read_unlock(&sl);

	return 0;
}

static rte_rwlock_t lk = RTE_RWLOCK_INITIALIZER;
static volatile uint64_t rwlock_data;
static uint64_t time_count[RTE_MAX_LCORE] = {0};

#define MAX_LOOP 10000
#define TEST_RWLOCK_DEBUG 0

static int
load_loop_fn(__rte_unused void *arg)
{
	uint64_t time_diff = 0, begin;
	uint64_t hz = rte_get_timer_hz();
	uint64_t lcount = 0;
	const unsigned int lcore = rte_lcore_id();

	/* wait synchro for workers */
	if (lcore != rte_get_main_lcore())
		rte_wait_until_equal_32(&synchro, 1, __ATOMIC_RELAXED);

	begin = rte_rdtsc_precise();
	while (lcount < MAX_LOOP) {
		rte_rwlock_write_lock(&lk);
		++rwlock_data;
		rte_rwlock_write_unlock(&lk);

		rte_rwlock_read_lock(&lk);
		if (TEST_RWLOCK_DEBUG && !(lcount % 100))
			printf("Core [%u] rwlock_data = %"PRIu64"\n",
				lcore, rwlock_data);
		rte_rwlock_read_unlock(&lk);

		lcount++;
		/* delay to make lock duty cycle slightly realistic */
		rte_pause();
	}

	time_diff = rte_rdtsc_precise() - begin;
	time_count[lcore] = time_diff * 1000000 / hz;
	return 0;
}

static int
test_rwlock_perf(void)
{
	unsigned int i;
	uint64_t total = 0;

	printf("\nRwlock Perf Test on %u cores...\n", rte_lcore_count());

	/* clear synchro and start workers */
	__atomic_store_n(&synchro, 0, __ATOMIC_RELAXED);
	if (rte_eal_mp_remote_launch(load_loop_fn, NULL, SKIP_MAIN) < 0)
		return -1;

	/* start synchro and launch test on main */
	__atomic_store_n(&synchro, 1, __ATOMIC_RELAXED);
	load_loop_fn(NULL);

	rte_eal_mp_wait_lcore();

	RTE_LCORE_FOREACH(i) {
		printf("Core [%u] cost time = %"PRIu64" us\n",
			i, time_count[i]);
		total += time_count[i];
	}

	printf("Total cost time = %"PRIu64" us\n", total);
	memset(time_count, 0, sizeof(time_count));

	return 0;
}

/*
 * - There is a global rwlock and a table of rwlocks (one per lcore).
 *
 * - The test function takes all of these locks and launches the
 *   ``test_rwlock_per_core()`` function on each core (except the main).
 *
 *   - The function takes the global write lock, display something,
 *     then releases the global lock.
 *   - Then, it takes the per-lcore write lock, display something, and
 *     releases the per-core lock.
 *   - Finally, a read lock is taken during 100 ms, then released.
 *
 * - The main function unlocks the per-lcore locks sequentially and
 *   waits between each lock. This triggers the display of a message
 *   for each core, in the correct order.
 *
 *   Then, it tries to take the global write lock and display the last
 *   message. The autotest script checks that the message order is correct.
 */
static int
rwlock_test1(void)
{
	int i;

	rte_rwlock_init(&sl);
	for (i = 0; i < RTE_MAX_LCORE; i++)
		rte_rwlock_init(&sl_tab[i]);

	rte_rwlock_write_lock(&sl);

	RTE_LCORE_FOREACH_WORKER(i) {
		rte_rwlock_write_lock(&sl_tab[i]);
		rte_eal_remote_launch(test_rwlock_per_core, NULL, i);
	}

	rte_rwlock_write_unlock(&sl);

	RTE_LCORE_FOREACH_WORKER(i) {
		rte_rwlock_write_unlock(&sl_tab[i]);
		rte_delay_ms(100);
	}

	rte_rwlock_write_lock(&sl);
	/* this message should be the last message of test */
	printf("Global write lock taken on main core %u\n", rte_lcore_id());
	rte_rwlock_write_unlock(&sl);

	rte_eal_mp_wait_lcore();

	if (test_rwlock_perf() < 0)
		return -1;

	return 0;
}

static int
try_read(uint32_t lc)
{
	int32_t rc;
	uint32_t i;

	rc = rte_rwlock_read_trylock(&try_rwlock_data.lock);
	if (rc != 0)
		return rc;

	for (i = 0; i != RTE_DIM(try_rwlock_data.data.u64); i++) {

		/* race condition occurred, lock doesn't work properly */
		if (try_rwlock_data.data.u64[i] != 0) {
			printf("%s(%u) error: unexpected data pattern\n",
				__func__, lc);
			rte_memdump(stdout, NULL,
				(void *)(uintptr_t)&try_rwlock_data.data,
				sizeof(try_rwlock_data.data));
			rc = -EFAULT;
			break;
		}
	}

	rte_rwlock_read_unlock(&try_rwlock_data.lock);
	return rc;
}

static int
try_write(uint32_t lc)
{
	int32_t rc;
	uint32_t i, v;

	v = RTE_MAX(lc % UINT8_MAX, 1U);

	rc = rte_rwlock_write_trylock(&try_rwlock_data.lock);
	if (rc != 0)
		return rc;

	/* update by bytes in reverse order */
	for (i = RTE_DIM(try_rwlock_data.data.u8); i-- != 0; ) {

		/* race condition occurred, lock doesn't work properly */
		if (try_rwlock_data.data.u8[i] != 0) {
			printf("%s:%d(%u) error: unexpected data pattern\n",
				__func__, __LINE__, lc);
			rte_memdump(stdout, NULL,
				(void *)(uintptr_t)&try_rwlock_data.data,
				sizeof(try_rwlock_data.data));
			rc = -EFAULT;
			break;
		}

		try_rwlock_data.data.u8[i] = v;
	}

	/* restore by bytes in reverse order */
	for (i = RTE_DIM(try_rwlock_data.data.u8); i-- != 0; ) {

		/* race condition occurred, lock doesn't work properly */
		if (try_rwlock_data.data.u8[i] != v) {
			printf("%s:%d(%u) error: unexpected data pattern\n",
				__func__, __LINE__, lc);
			rte_memdump(stdout, NULL,
				(void *)(uintptr_t)&try_rwlock_data.data,
				sizeof(try_rwlock_data.data));
			rc = -EFAULT;
			break;
		}

		try_rwlock_data.data.u8[i] = 0;
	}

	rte_rwlock_write_unlock(&try_rwlock_data.lock);
	return rc;
}

static int
try_read_lcore(__rte_unused void *data)
{
	int32_t rc;
	uint32_t i, lc;
	uint64_t ftm, stm, tm;
	struct try_rwlock_lcore *lcd;

	lc = rte_lcore_id();
	lcd = try_lcore_data + lc;
	lcd->type = LC_TYPE_RDLOCK;

	ftm = try_rwlock_data.tick;
	stm = rte_get_timer_cycles();

	do {
		for (i = 0; i != ITER_NUM; i++) {
			rc = try_read(lc);
			if (rc == 0)
				lcd->stat.success++;
			else if (rc == -EBUSY)
				lcd->stat.fail++;
			else
				break;
			rc = 0;
		}
		tm = rte_get_timer_cycles() - stm;
	} while (tm < ftm && rc == 0);

	lcd->rc = rc;
	lcd->stat.tick = tm;
	return rc;
}

static int
try_write_lcore(__rte_unused void *data)
{
	int32_t rc;
	uint32_t i, lc;
	uint64_t ftm, stm, tm;
	struct try_rwlock_lcore *lcd;

	lc = rte_lcore_id();
	lcd = try_lcore_data + lc;
	lcd->type = LC_TYPE_WRLOCK;

	ftm = try_rwlock_data.tick;
	stm = rte_get_timer_cycles();

	do {
		for (i = 0; i != ITER_NUM; i++) {
			rc = try_write(lc);
			if (rc == 0)
				lcd->stat.success++;
			else if (rc == -EBUSY)
				lcd->stat.fail++;
			else
				break;
			rc = 0;
		}
		tm = rte_get_timer_cycles() - stm;
	} while (tm < ftm && rc == 0);

	lcd->rc = rc;
	lcd->stat.tick = tm;
	return rc;
}

static void
print_try_lcore_stats(const struct try_rwlock_lcore *tlc, uint32_t lc)
{
	uint64_t f, s;

	f = RTE_MAX(tlc->stat.fail, 1ULL);
	s = RTE_MAX(tlc->stat.success, 1ULL);

	printf("try_lcore_data[%u]={\n"
		"\trc=%d,\n"
		"\ttype=%s,\n"
		"\tfail=%" PRIu64 ",\n"
		"\tsuccess=%" PRIu64 ",\n"
		"\tcycles=%" PRIu64 ",\n"
		"\tcycles/op=%#Lf,\n"
		"\tcycles/success=%#Lf,\n"
		"\tsuccess/fail=%#Lf,\n"
		"};\n",
		lc,
		tlc->rc,
		tlc->type == LC_TYPE_RDLOCK ? "RDLOCK" : "WRLOCK",
		tlc->stat.fail,
		tlc->stat.success,
		tlc->stat.tick,
		(long double)tlc->stat.tick /
		(tlc->stat.fail + tlc->stat.success),
		(long double)tlc->stat.tick / s,
		(long double)tlc->stat.success / f);
}

static void
collect_try_lcore_stats(struct try_rwlock_lcore *tlc,
	const struct try_rwlock_lcore *lc)
{
	tlc->stat.tick += lc->stat.tick;
	tlc->stat.fail += lc->stat.fail;
	tlc->stat.success += lc->stat.success;
}

/*
 * Process collected results:
 *  - check status
 *  - collect and print statistics
 */
static int
process_try_lcore_stats(void)
{
	int32_t rc;
	uint32_t lc, rd, wr;
	struct try_rwlock_lcore rlc, wlc;

	memset(&rlc, 0, sizeof(rlc));
	memset(&wlc, 0, sizeof(wlc));

	rlc.type = LC_TYPE_RDLOCK;
	wlc.type = LC_TYPE_WRLOCK;
	rd = 0;
	wr = 0;

	rc = 0;
	RTE_LCORE_FOREACH(lc) {
		rc |= try_lcore_data[lc].rc;
		if (try_lcore_data[lc].type == LC_TYPE_RDLOCK) {
			collect_try_lcore_stats(&rlc, try_lcore_data + lc);
			rd++;
		} else {
			collect_try_lcore_stats(&wlc, try_lcore_data + lc);
			wr++;
		}
	}

	if (rc == 0) {
		RTE_LCORE_FOREACH(lc)
			print_try_lcore_stats(try_lcore_data + lc, lc);

		if (rd != 0) {
			printf("aggregated stats for %u RDLOCK cores:\n", rd);
			print_try_lcore_stats(&rlc, rd);
		}

		if (wr != 0) {
			printf("aggregated stats for %u WRLOCK cores:\n", wr);
			print_try_lcore_stats(&wlc, wr);
		}
	}

	return rc;
}

static void
try_test_reset(void)
{
	memset(&try_lcore_data, 0, sizeof(try_lcore_data));
	memset(&try_rwlock_data, 0, sizeof(try_rwlock_data));
	try_rwlock_data.tick = TEST_SEC * rte_get_tsc_hz();
}

/* all lcores grab RDLOCK */
static int
try_rwlock_test_rda(void)
{
	try_test_reset();

	/* start read test on all available lcores */
	rte_eal_mp_remote_launch(try_read_lcore, NULL, CALL_MAIN);
	rte_eal_mp_wait_lcore();

	return process_try_lcore_stats();
}

/* all worker lcores grab RDLOCK, main one grabs WRLOCK */
static int
try_rwlock_test_rds_wrm(void)
{
	try_test_reset();

	rte_eal_mp_remote_launch(try_read_lcore, NULL, SKIP_MAIN);
	try_write_lcore(NULL);
	rte_eal_mp_wait_lcore();

	return process_try_lcore_stats();
}

/* main and even worker lcores grab RDLOCK, odd lcores grab WRLOCK */
static int
try_rwlock_test_rde_wro(void)
{
	uint32_t lc, mlc;

	try_test_reset();

	mlc = rte_get_main_lcore();

	RTE_LCORE_FOREACH(lc) {
		if (lc != mlc) {
			if ((lc & 1) == 0)
				rte_eal_remote_launch(try_read_lcore,
						NULL, lc);
			else
				rte_eal_remote_launch(try_write_lcore,
						NULL, lc);
		}
	}
	try_read_lcore(NULL);
	rte_eal_mp_wait_lcore();

	return process_try_lcore_stats();
}

REGISTER_TEST_COMMAND(rwlock_test1_autotest, rwlock_test1);
REGISTER_TEST_COMMAND(rwlock_rda_autotest, try_rwlock_test_rda);
REGISTER_TEST_COMMAND(rwlock_rds_wrm_autotest, try_rwlock_test_rds_wrm);
REGISTER_TEST_COMMAND(rwlock_rde_wro_autotest, try_rwlock_test_rde_wro);
