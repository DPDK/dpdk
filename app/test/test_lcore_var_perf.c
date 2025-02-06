/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Ericsson AB
 */

#define MAX_MODS 1024

#include <stdio.h>

#include <rte_bitops.h>
#include <rte_cycles.h>
#include <rte_lcore_var.h>
#include <rte_per_lcore.h>
#include <rte_random.h>

#include "test.h"

struct mod_lcore_state {
	uint64_t a;
	uint64_t b;
	uint64_t sum;
};

static void
mod_init(struct mod_lcore_state *state)
{
	state->a = rte_rand();
	state->b = rte_rand();
	state->sum = 0;
}

static __rte_always_inline void
mod_update(volatile struct mod_lcore_state *state)
{
	state->sum += state->a * state->b;
}

struct __rte_cache_aligned mod_lcore_state_aligned {
	struct mod_lcore_state mod_state;

	RTE_CACHE_GUARD;
};

static struct mod_lcore_state_aligned
sarray_lcore_state[MAX_MODS][RTE_MAX_LCORE];

static void
sarray_init(void)
{
	unsigned int lcore_id = rte_lcore_id();
	int mod;

	for (mod = 0; mod < MAX_MODS; mod++) {
		struct mod_lcore_state *mod_state = &sarray_lcore_state[mod][lcore_id].mod_state;

		mod_init(mod_state);
	}
}

static __rte_noinline void
sarray_update(unsigned int mod)
{
	unsigned int lcore_id = rte_lcore_id();
	struct mod_lcore_state *mod_state = &sarray_lcore_state[mod][lcore_id].mod_state;

	mod_update(mod_state);
}

struct mod_lcore_state_lazy {
	struct mod_lcore_state mod_state;
	bool initialized;
};

/*
 * Note: it's usually a bad idea have this much thread-local storage
 * allocated in a real application, since it will incur a cost on
 * thread creation and non-lcore thread memory usage.
 */
static RTE_DEFINE_PER_LCORE(struct mod_lcore_state_lazy, tls_lcore_state)[MAX_MODS];

static inline void
tls_init(struct mod_lcore_state_lazy *state)
{
	mod_init(&state->mod_state);

	state->initialized = true;
}

static __rte_noinline void
tls_lazy_update(unsigned int mod)
{
	struct mod_lcore_state_lazy *state =
		&RTE_PER_LCORE(tls_lcore_state[mod]);

	/* With thread-local storage, initialization must usually be lazy */
	if (!state->initialized)
		tls_init(state);

	mod_update(&state->mod_state);
}

static __rte_noinline void
tls_update(unsigned int mod)
{
	struct mod_lcore_state_lazy *state =
		&RTE_PER_LCORE(tls_lcore_state[mod]);

	mod_update(&state->mod_state);
}

RTE_LCORE_VAR_HANDLE(struct mod_lcore_state, lvar_lcore_state)[MAX_MODS];

static void
lvar_init(void)
{
	unsigned int mod;

	for (mod = 0; mod < MAX_MODS; mod++) {
		RTE_LCORE_VAR_ALLOC(lvar_lcore_state[mod]);

		struct mod_lcore_state *state = RTE_LCORE_VAR(lvar_lcore_state[mod]);

		mod_init(state);
	}
}

static __rte_noinline void
lvar_update(unsigned int mod)
{
	struct mod_lcore_state *state =	RTE_LCORE_VAR(lvar_lcore_state[mod]);

	mod_update(state);
}

static void
shuffle(unsigned int *elems, size_t len)
{
	size_t i;

	for (i = len - 1; i > 0; i--) {
		unsigned int other = rte_rand_max(i + 1);

		unsigned int tmp = elems[other];
		elems[other] = elems[i];
		elems[i] = tmp;
	}
}

#define ITERATIONS UINT64_C(10000000)

static inline double
benchmark_access(const unsigned int *mods, unsigned int num_mods,
		 void (*init_fun)(void), void (*update_fun)(unsigned int))
{
	unsigned int i;
	double start;
	double end;
	double latency;
	unsigned int num_mods_mask = num_mods - 1;

	RTE_VERIFY(rte_is_power_of_2(num_mods));

	if (init_fun != NULL)
		init_fun();

	/* Warm up cache and make sure TLS variables are initialized */
	for (i = 0; i < num_mods; i++)
		update_fun(i);

	start = rte_rdtsc();

	for (i = 0; i < ITERATIONS; i++)
		update_fun(mods[i & num_mods_mask]);

	end = rte_rdtsc();

	latency = (end - start) / (double)ITERATIONS;

	return latency;
}

static void
test_lcore_var_access_n(unsigned int num_mods)
{
	double sarray_latency;
	double tls_latency;
	double lazy_tls_latency;
	double lvar_latency;
	unsigned int mods[MAX_MODS];
	unsigned int i;

	for (i = 0; i < num_mods; i++)
		mods[i] = i;

	shuffle(mods, num_mods);

	sarray_latency =
		benchmark_access(mods, num_mods, sarray_init, sarray_update);

	tls_latency =
		benchmark_access(mods, num_mods, NULL, tls_update);

	lazy_tls_latency =
		benchmark_access(mods, num_mods, NULL, tls_lazy_update);

	lvar_latency =
		benchmark_access(mods, num_mods, lvar_init, lvar_update);

	printf("%17u %8.1f %14.1f %15.1f %10.1f\n", num_mods, sarray_latency,
			tls_latency, lazy_tls_latency, lvar_latency);
}

/*
 * The potential performance benefit of lcore variables compared to
 * the use of statically sized, lcore id-indexed arrays is not
 * shorter latencies in a scenario with low cache pressure, but rather
 * fewer cache misses in a real-world scenario, with extensive cache
 * usage. These tests are a crude simulation of such, using <N> dummy
 * modules, each with a small, per-lcore state. Note however that
 * these tests have very little non-lcore/thread local state, which is
 * unrealistic.
 */

static int
test_lcore_var_access(void)
{
	unsigned int num_mods = 1;

	printf("- Latencies [TSC cycles/update] -\n");
	printf("Number of           Static   Thread-local    Thread-local      Lcore\n");
	printf("Modules/Variables    Array        Storage  Storage (Lazy)  Variables\n");

	for (num_mods = 1; num_mods <= MAX_MODS; num_mods *= 2)
		test_lcore_var_access_n(num_mods);

	return TEST_SUCCESS;
}

static struct unit_test_suite lcore_var_testsuite = {
	.suite_name = "lcore variable perf autotest",
	.unit_test_cases = {
		TEST_CASE(test_lcore_var_access),
		TEST_CASES_END()
	},
};

static int
test_lcore_var_perf(void)
{
	return unit_test_suite_runner(&lcore_var_testsuite);
}

REGISTER_PERF_TEST(lcore_var_perf_autotest, test_lcore_var_perf);
