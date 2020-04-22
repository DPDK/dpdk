/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <rte_cycles.h>
#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_eal_trace.h>
#include <rte_malloc.h>
#include <rte_lcore.h>

#include "test.h"
#include "test_trace.h"

struct test_data;

struct lcore_data {
	volatile bool done;
	volatile bool started;
	uint64_t total_cycles;
	uint64_t total_calls;
} __rte_cache_aligned;

struct test_data {
	unsigned int nb_workers;
	struct lcore_data ldata[];
} __rte_cache_aligned;

#define STEP 100
#define CENT_OPS(OP) do {     \
OP; OP; OP; OP; OP; OP; OP; OP; OP; OP; \
OP; OP; OP; OP; OP; OP; OP; OP; OP; OP; \
OP; OP; OP; OP; OP; OP; OP; OP; OP; OP; \
OP; OP; OP; OP; OP; OP; OP; OP; OP; OP; \
OP; OP; OP; OP; OP; OP; OP; OP; OP; OP; \
OP; OP; OP; OP; OP; OP; OP; OP; OP; OP; \
OP; OP; OP; OP; OP; OP; OP; OP; OP; OP; \
OP; OP; OP; OP; OP; OP; OP; OP; OP; OP; \
OP; OP; OP; OP; OP; OP; OP; OP; OP; OP; \
OP; OP; OP; OP; OP; OP; OP; OP; OP; OP; \
} while (0)

static void
measure_perf(const char *str, struct test_data *data)
{
	uint64_t hz = rte_get_timer_hz();
	uint64_t total_cycles = 0;
	uint64_t total_calls = 0;
	double cycles, ns;
	unsigned int workers;

	for (workers = 0; workers < data->nb_workers; workers++) {
		total_cycles += data->ldata[workers].total_cycles;
		total_calls += data->ldata[workers].total_calls;
	}

	cycles = total_calls ? (double)total_cycles / (double)total_calls : 0;
	cycles /= STEP;
	cycles /= 100; /* CENT_OPS */

	ns = (cycles / (double)hz) * 1E9;
	printf("%16s: cycles=%f ns=%f\n", str, cycles, ns);
}

static void
wait_till_workers_are_ready(struct test_data *data)
{
	unsigned int workers;

	for (workers = 0; workers < data->nb_workers; workers++)
		while (!data->ldata[workers].started)
			rte_pause();
}

static void
signal_workers_to_finish(struct test_data *data)
{
	unsigned int workers;

	for (workers = 0; workers < data->nb_workers; workers++) {
		data->ldata[workers].done = 1;
		rte_smp_wmb();
	}
}

#define WORKER_DEFINE(func) \
static void __rte_noinline \
__worker_##func(struct lcore_data *ldata) \
{ \
	uint64_t start; \
	int i; \
	while (!ldata->done) { \
		start = rte_get_timer_cycles(); \
		for (i = 0; i < STEP; i++) \
			CENT_OPS(func); \
		ldata->total_cycles += rte_get_timer_cycles() - start; \
		ldata->total_calls++; \
	} \
} \
static int \
worker_fn_##func(void *arg) \
{ \
	struct lcore_data *ldata = arg; \
	ldata->started = 1; \
	rte_smp_wmb(); \
	__worker_##func(ldata); \
	return 0; \
}


/* Test to find trace overhead */
#define GENERIC_VOID rte_eal_trace_generic_void()
#define GENERIC_U64 rte_eal_trace_generic_u64(0x120000)
#define GENERIC_INT rte_eal_trace_generic_int(-34)
#define GENERIC_FLOAT rte_eal_trace_generic_float(3.3f)
#define GENERIC_DOUBLE rte_eal_trace_generic_double(3.66666)
#define GENERIC_STR rte_eal_trace_generic_str("hello world")
#define VOID_FP app_dpdk_test_fp()

WORKER_DEFINE(GENERIC_VOID)
WORKER_DEFINE(GENERIC_U64)
WORKER_DEFINE(GENERIC_INT)
WORKER_DEFINE(GENERIC_FLOAT)
WORKER_DEFINE(GENERIC_DOUBLE)
WORKER_DEFINE(GENERIC_STR)
WORKER_DEFINE(VOID_FP)

static void
run_test(const char *str, lcore_function_t f, struct test_data *data, size_t sz)
{
	unsigned int id, worker = 0;

	memset(data, 0, sz);
	data->nb_workers = rte_lcore_count() - 1;
	RTE_LCORE_FOREACH_SLAVE(id)
		rte_eal_remote_launch(f, &data->ldata[worker++], id);

	wait_till_workers_are_ready(data);
	rte_delay_ms(100); /* Wait for some time to accumulate the stats */
	measure_perf(str, data);
	signal_workers_to_finish(data);

	RTE_LCORE_FOREACH_SLAVE(id)
		rte_eal_wait_lcore(id);
}

static int
test_trace_perf(void)
{
	unsigned int nb_cores, nb_workers;
	struct test_data *data;
	size_t sz;

	nb_cores = rte_lcore_count();
	nb_workers = nb_cores - 1;
	if (nb_cores < 2) {
		printf("Need minimum two cores for testing\n");
		return TEST_SKIPPED;
	}

	printf("Timer running at %5.2fMHz\n", rte_get_timer_hz()/1E6);
	sz = sizeof(struct test_data);
	sz += nb_workers * sizeof(struct lcore_data);

	data = rte_zmalloc(NULL, sz, RTE_CACHE_LINE_SIZE);
	if (data == NULL) {
		printf("Failed to allocate memory\n");
		return TEST_FAILED;
	}

	run_test("void", worker_fn_GENERIC_VOID, data, sz);
	run_test("u64", worker_fn_GENERIC_U64, data, sz);
	run_test("int", worker_fn_GENERIC_INT, data, sz);
	run_test("float", worker_fn_GENERIC_FLOAT, data, sz);
	run_test("double", worker_fn_GENERIC_DOUBLE, data, sz);
	run_test("string", worker_fn_GENERIC_STR, data, sz);
	run_test("void_fp", worker_fn_VOID_FP, data, sz);

	rte_free(data);
	return TEST_SUCCESS;
}

REGISTER_TEST_COMMAND(trace_perf_autotest, test_trace_perf);
