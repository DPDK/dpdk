/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

 /*
  * This is a simple functional test for rte_smp_mb() implementation.
  * I.E. make sure that LOAD and STORE operations that precede the
  * rte_smp_mb() call are globally visible across the lcores
  * before the the LOAD and STORE operations that follows it.
  * The test uses simple implementation of Peterson's lock algorithm
  * (https://en.wikipedia.org/wiki/Peterson%27s_algorithm)
  * for two execution units to make sure that rte_smp_mb() prevents
  * store-load reordering to happen.
  * Also when executed on a single lcore could be used as a approxiamate
  * estimation of number of cycles particular implementation of rte_smp_mb()
  * will take.
  */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include <rte_memory.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_pause.h>
#include <rte_random.h>
#include <rte_cycles.h>
#include <rte_vect.h>
#include <rte_debug.h>

#include "test.h"

#define ADD_MAX		8
#define ITER_MAX	0x1000000

enum plock_use_type {
	USE_MB,
	USE_SMP_MB,
	USE_NUM
};

struct plock {
	volatile uint32_t flag[2];
	volatile uint32_t victim;
	enum plock_use_type utype;
};

/*
 * Lock plus protected by it two counters.
 */
struct plock_test {
	struct plock lock;
	uint64_t val;
	uint64_t iter;
};

/*
 * Each active lcore shares plock_test struct with it's left and right
 * neighbours.
 */
struct lcore_plock_test {
	struct plock_test *pt[2]; /* shared, lock-protected data */
	uint64_t sum[2];          /* local copy of the shared data */
	uint64_t iter;            /* number of iterations to perfom */
	uint32_t lc;              /* given lcore id */
};

static inline void
store_load_barrier(uint32_t utype)
{
	if (utype == USE_MB)
		rte_mb();
	else if (utype == USE_SMP_MB)
		rte_smp_mb();
	else
		RTE_VERIFY(0);
}

/*
 * Peterson lock implementation.
 */
static void
plock_lock(struct plock *l, uint32_t self)
{
	uint32_t other;

	other = self ^ 1;

	l->flag[self] = 1;
	rte_smp_wmb();
	l->victim = self;

	store_load_barrier(l->utype);

	while (l->flag[other] == 1 && l->victim == self)
		rte_pause();
	rte_smp_rmb();
}

static void
plock_unlock(struct plock *l, uint32_t self)
{
	rte_smp_wmb();
	l->flag[self] = 0;
}

static void
plock_reset(struct plock *l, enum plock_use_type utype)
{
	memset(l, 0, sizeof(*l));
	l->utype = utype;
}

/*
 * grab the lock, update both counters, release the lock.
 */
static void
plock_add(struct plock_test *pt, uint32_t self, uint32_t n)
{
	plock_lock(&pt->lock, self);
	pt->iter++;
	pt->val += n;
	plock_unlock(&pt->lock, self);
}

static int
plock_test1_lcore(void *data)
{
	uint64_t tm;
	uint32_t lc, ln;
	uint64_t i, n;
	struct lcore_plock_test *lpt;

	lpt = data;
	lc = rte_lcore_id();

	/* find lcore_plock_test struct for given lcore */
	for (ln = rte_lcore_count(); ln != 0 && lpt->lc != lc; lpt++, ln--)
		;

	if (ln == 0) {
		printf("%s(%u) error at init\n", __func__, lc);
		return -1;
	}

	n = rte_rand() % ADD_MAX;
	tm = rte_get_timer_cycles();

	/*
	 * for each iteration:
	 * - update shared, locked protected data in a safe manner
	 * - update local copy of the shared data
	 */
	for (i = 0; i != lpt->iter; i++) {

		plock_add(lpt->pt[0], 0, n);
		plock_add(lpt->pt[1], 1, n);

		lpt->sum[0] += n;
		lpt->sum[1] += n;

		n = (n + 1) % ADD_MAX;
	}

	tm = rte_get_timer_cycles() - tm;

	printf("%s(%u): %" PRIu64 " iterations finished, in %" PRIu64
		" cycles, %#Lf cycles/iteration, "
		"local sum={%" PRIu64 ", %" PRIu64 "}\n",
		__func__, lc, i, tm, (long double)tm / i,
		lpt->sum[0], lpt->sum[1]);
	return 0;
}

/*
 * For N active lcores we allocate N+1 lcore_plock_test structures.
 * Each active lcore shares one lcore_plock_test structure with its
 * left lcore neighbor and one lcore_plock_test structure with its
 * right lcore neighbor.
 * During the test each lcore updates data in both shared structures and
 * its local copies. Then at validation phase we check that our shared
 * and local data are the same.
 */
static int
plock_test(uint64_t iter, enum plock_use_type utype)
{
	int32_t rc;
	uint32_t i, lc, n;
	uint64_t *sum;
	struct plock_test *pt;
	struct lcore_plock_test *lpt;

	/* init phase, allocate and initialize shared data */

	n = rte_lcore_count();
	pt = calloc(n + 1, sizeof(*pt));
	lpt = calloc(n, sizeof(*lpt));
	sum = calloc(n + 1, sizeof(*sum));

	printf("%s(iter=%" PRIu64 ", utype=%u) started on %u lcores\n",
		__func__, iter, utype, n);

	if (pt == NULL || lpt == NULL || sum == NULL) {
		printf("%s: failed to allocate memory for %u lcores\n",
			__func__, n);
		free(pt);
		free(lpt);
		free(sum);
		return -ENOMEM;
	}

	for (i = 0; i != n + 1; i++)
		plock_reset(&pt[i].lock, utype);

	i = 0;
	RTE_LCORE_FOREACH(lc) {

		lpt[i].lc = lc;
		lpt[i].iter = iter;
		lpt[i].pt[0] = pt + i;
		lpt[i].pt[1] = pt + i + 1;
		i++;
	}

	lpt[i - 1].pt[1] = pt;

	for (i = 0; i != n; i++)
		printf("lpt[%u]={lc=%u, pt={%p, %p},};\n",
			i, lpt[i].lc, lpt[i].pt[0], lpt[i].pt[1]);


	/* test phase - start and wait for completion on each active lcore */

	rte_eal_mp_remote_launch(plock_test1_lcore, lpt, CALL_MASTER);
	rte_eal_mp_wait_lcore();

	/* validation phase - make sure that shared and local data match */

	for (i = 0; i != n; i++) {
		sum[i] += lpt[i].sum[0];
		sum[i + 1] += lpt[i].sum[1];
	}

	sum[0] += sum[i];

	rc = 0;
	for (i = 0; i != n; i++) {
		printf("%s: sum[%u]=%" PRIu64 ", pt[%u].val=%" PRIu64 ", pt[%u].iter=%" PRIu64 ";\n",
			__func__, i, sum[i], i, pt[i].val, i, pt[i].iter);

		/* race condition occurred, lock doesn't work properly */
		if (sum[i] != pt[i].val || 2 * iter != pt[i].iter) {
			printf("error: local and shared sums don't match\n");
			rc = -1;
		}
	}

	free(pt);
	free(lpt);
	free(sum);

	printf("%s(utype=%u) returns %d\n", __func__, utype, rc);
	return rc;
}

static int
test_barrier(void)
{
	int32_t i, ret, rc[USE_NUM];

	for (i = 0; i != RTE_DIM(rc); i++)
		rc[i] = plock_test(ITER_MAX, i);

	ret = 0;
	for (i = 0; i != RTE_DIM(rc); i++) {
		printf("%s for utype=%d %s\n",
			__func__, i, rc[i] == 0 ? "passed" : "failed");
		ret |= rc[i];
	}

	return ret;
}

REGISTER_TEST_COMMAND(barrier_autotest, test_barrier);
