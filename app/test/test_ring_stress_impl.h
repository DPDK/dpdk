/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include "test_ring_stress.h"

/**
 * Stress test for ring enqueue/dequeue operations.
 * Performs the following pattern on each worker:
 * dequeue/read-write data from the dequeued objects/enqueue.
 * Serves as both functional and performance test of ring
 * enqueue/dequeue operations under high contention
 * (for both over committed and non-over committed scenarios).
 */

#define RING_NAME	"RING_STRESS"
#define BULK_NUM	32
#define RING_SIZE	(2 * BULK_NUM * RTE_MAX_LCORE)

enum {
	WRK_CMD_STOP,
	WRK_CMD_RUN,
};

static uint32_t wrk_cmd __rte_cache_aligned = WRK_CMD_STOP;

/* test run-time in seconds */
static const uint32_t run_time = 60;
static const uint32_t verbose;

struct lcore_stat {
	uint64_t nb_cycle;
	struct {
		uint64_t nb_call;
		uint64_t nb_obj;
		uint64_t nb_cycle;
		uint64_t max_cycle;
		uint64_t min_cycle;
	} op;
};

struct lcore_arg {
	struct rte_ring *rng;
	struct lcore_stat stats;
} __rte_cache_aligned;

struct ring_elem {
	uint32_t cnt[RTE_CACHE_LINE_SIZE / sizeof(uint32_t)];
} __rte_cache_aligned;

/*
 * redefinable functions
 */
static uint32_t
_st_ring_dequeue_bulk(struct rte_ring *r, void **obj, uint32_t n,
	uint32_t *avail);

static uint32_t
_st_ring_enqueue_bulk(struct rte_ring *r, void * const *obj, uint32_t n,
	uint32_t *free);

static int
_st_ring_init(struct rte_ring *r, const char *name, uint32_t num);


static void
lcore_stat_update(struct lcore_stat *ls, uint64_t call, uint64_t obj,
	uint64_t tm, int32_t prcs)
{
	ls->op.nb_call += call;
	ls->op.nb_obj += obj;
	ls->op.nb_cycle += tm;
	if (prcs) {
		ls->op.max_cycle = RTE_MAX(ls->op.max_cycle, tm);
		ls->op.min_cycle = RTE_MIN(ls->op.min_cycle, tm);
	}
}

static void
lcore_op_stat_aggr(struct lcore_stat *ms, const struct lcore_stat *ls)
{

	ms->op.nb_call += ls->op.nb_call;
	ms->op.nb_obj += ls->op.nb_obj;
	ms->op.nb_cycle += ls->op.nb_cycle;
	ms->op.max_cycle = RTE_MAX(ms->op.max_cycle, ls->op.max_cycle);
	ms->op.min_cycle = RTE_MIN(ms->op.min_cycle, ls->op.min_cycle);
}

static void
lcore_stat_aggr(struct lcore_stat *ms, const struct lcore_stat *ls)
{
	ms->nb_cycle = RTE_MAX(ms->nb_cycle, ls->nb_cycle);
	lcore_op_stat_aggr(ms, ls);
}

static void
lcore_stat_dump(FILE *f, uint32_t lc, const struct lcore_stat *ls)
{
	long double st;

	st = (long double)rte_get_timer_hz() / US_PER_S;

	if (lc == UINT32_MAX)
		fprintf(f, "%s(AGGREGATE)={\n", __func__);
	else
		fprintf(f, "%s(lcore=%u)={\n", __func__, lc);

	fprintf(f, "\tnb_cycle=%" PRIu64 "(%.2Lf usec),\n",
		ls->nb_cycle, (long double)ls->nb_cycle / st);

	fprintf(f, "\tDEQ+ENQ={\n");

	fprintf(f, "\t\tnb_call=%" PRIu64 ",\n", ls->op.nb_call);
	fprintf(f, "\t\tnb_obj=%" PRIu64 ",\n", ls->op.nb_obj);
	fprintf(f, "\t\tnb_cycle=%" PRIu64 ",\n", ls->op.nb_cycle);
	fprintf(f, "\t\tobj/call(avg): %.2Lf\n",
		(long double)ls->op.nb_obj / ls->op.nb_call);
	fprintf(f, "\t\tcycles/obj(avg): %.2Lf\n",
		(long double)ls->op.nb_cycle / ls->op.nb_obj);
	fprintf(f, "\t\tcycles/call(avg): %.2Lf\n",
		(long double)ls->op.nb_cycle / ls->op.nb_call);

	/* if min/max cycles per call stats was collected */
	if (ls->op.min_cycle != UINT64_MAX) {
		fprintf(f, "\t\tmax cycles/call=%" PRIu64 "(%.2Lf usec),\n",
			ls->op.max_cycle,
			(long double)ls->op.max_cycle / st);
		fprintf(f, "\t\tmin cycles/call=%" PRIu64 "(%.2Lf usec),\n",
			ls->op.min_cycle,
			(long double)ls->op.min_cycle / st);
	}

	fprintf(f, "\t},\n");
	fprintf(f, "};\n");
}

static void
fill_ring_elm(struct ring_elem *elm, uint32_t fill)
{
	uint32_t i;

	for (i = 0; i != RTE_DIM(elm->cnt); i++)
		elm->cnt[i] = fill;
}

static int32_t
check_updt_elem(struct ring_elem *elm[], uint32_t num,
	const struct ring_elem *check, const struct ring_elem *fill)
{
	uint32_t i;

	static rte_spinlock_t dump_lock;

	for (i = 0; i != num; i++) {
		if (memcmp(check, elm[i], sizeof(*check)) != 0) {
			rte_spinlock_lock(&dump_lock);
			printf("%s(lc=%u, num=%u) failed at %u-th iter, "
				"offending object: %p\n",
				__func__, rte_lcore_id(), num, i, elm[i]);
			rte_memdump(stdout, "expected", check, sizeof(*check));
			rte_memdump(stdout, "result", elm[i], sizeof(*elm[i]));
			rte_spinlock_unlock(&dump_lock);
			return -EINVAL;
		}
		memcpy(elm[i], fill, sizeof(*elm[i]));
	}

	return 0;
}

static int
check_ring_op(uint32_t exp, uint32_t res, uint32_t lc,
	const char *fname, const char *opname)
{
	if (exp != res) {
		printf("%s(lc=%u) failure: %s expected: %u, returned %u\n",
			fname, lc, opname, exp, res);
		return -ENOSPC;
	}
	return 0;
}

static int
test_worker(void *arg, const char *fname, int32_t prcs)
{
	int32_t rc;
	uint32_t lc, n, num;
	uint64_t cl, tm0, tm1;
	struct lcore_arg *la;
	struct ring_elem def_elm, loc_elm;
	struct ring_elem *obj[2 * BULK_NUM];

	la = arg;
	lc = rte_lcore_id();

	fill_ring_elm(&def_elm, UINT32_MAX);
	fill_ring_elm(&loc_elm, lc);

	/* Acquire ordering is not required as the main is not
	 * really releasing any data through 'wrk_cmd' to
	 * the worker.
	 */
	while (__atomic_load_n(&wrk_cmd, __ATOMIC_RELAXED) != WRK_CMD_RUN)
		rte_pause();

	cl = rte_rdtsc_precise();

	do {
		/* num in interval [7/8, 11/8] of BULK_NUM */
		num = 7 * BULK_NUM / 8 + rte_rand() % (BULK_NUM / 2);

		/* reset all pointer values */
		memset(obj, 0, sizeof(obj));

		/* dequeue num elems */
		tm0 = (prcs != 0) ? rte_rdtsc_precise() : 0;
		n = _st_ring_dequeue_bulk(la->rng, (void **)obj, num, NULL);
		tm0 = (prcs != 0) ? rte_rdtsc_precise() - tm0 : 0;

		/* check return value and objects */
		rc = check_ring_op(num, n, lc, fname,
			RTE_STR(_st_ring_dequeue_bulk));
		if (rc == 0)
			rc = check_updt_elem(obj, num, &def_elm, &loc_elm);
		if (rc != 0)
			break;

		/* enqueue num elems */
		rte_compiler_barrier();
		rc = check_updt_elem(obj, num, &loc_elm, &def_elm);
		if (rc != 0)
			break;

		tm1 = (prcs != 0) ? rte_rdtsc_precise() : 0;
		n = _st_ring_enqueue_bulk(la->rng, (void **)obj, num, NULL);
		tm1 = (prcs != 0) ? rte_rdtsc_precise() - tm1 : 0;

		/* check return value */
		rc = check_ring_op(num, n, lc, fname,
			RTE_STR(_st_ring_enqueue_bulk));
		if (rc != 0)
			break;

		lcore_stat_update(&la->stats, 1, num, tm0 + tm1, prcs);

	} while (__atomic_load_n(&wrk_cmd, __ATOMIC_RELAXED) == WRK_CMD_RUN);

	cl = rte_rdtsc_precise() - cl;
	if (prcs == 0)
		lcore_stat_update(&la->stats, 0, 0, cl, 0);
	la->stats.nb_cycle = cl;
	return rc;
}
static int
test_worker_prcs(void *arg)
{
	return test_worker(arg, __func__, 1);
}

static int
test_worker_avg(void *arg)
{
	return test_worker(arg, __func__, 0);
}

static void
mt1_fini(struct rte_ring *rng, void *data)
{
	rte_free(rng);
	rte_free(data);
}

static int
mt1_init(struct rte_ring **rng, void **data, uint32_t num)
{
	int32_t rc;
	size_t sz;
	uint32_t i, nr;
	struct rte_ring *r;
	struct ring_elem *elm;
	void *p;

	*rng = NULL;
	*data = NULL;

	sz = num * sizeof(*elm);
	elm = rte_zmalloc(NULL, sz, __alignof__(*elm));
	if (elm == NULL) {
		printf("%s: alloc(%zu) for %u elems data failed",
			__func__, sz, num);
		return -ENOMEM;
	}

	*data = elm;

	/* alloc ring */
	nr = 2 * num;
	sz = rte_ring_get_memsize(nr);
	r = rte_zmalloc(NULL, sz, __alignof__(*r));
	if (r == NULL) {
		printf("%s: alloc(%zu) for FIFO with %u elems failed",
			__func__, sz, nr);
		return -ENOMEM;
	}

	*rng = r;

	rc = _st_ring_init(r, RING_NAME, nr);
	if (rc != 0) {
		printf("%s: _st_ring_init(%p, %u) failed, error: %d(%s)\n",
			__func__, r, nr, rc, strerror(-rc));
		return rc;
	}

	for (i = 0; i != num; i++) {
		fill_ring_elm(elm + i, UINT32_MAX);
		p = elm + i;
		if (_st_ring_enqueue_bulk(r, &p, 1, NULL) != 1)
			break;
	}

	if (i != num) {
		printf("%s: _st_ring_enqueue(%p, %u) returned %u\n",
			__func__, r, num, i);
		return -ENOSPC;
	}

	return 0;
}

static int
test_mt1(int (*test)(void *))
{
	int32_t rc;
	uint32_t lc, mc;
	struct rte_ring *r;
	void *data;
	struct lcore_arg arg[RTE_MAX_LCORE];

	static const struct lcore_stat init_stat = {
		.op.min_cycle = UINT64_MAX,
	};

	rc = mt1_init(&r, &data, RING_SIZE);
	if (rc != 0) {
		mt1_fini(r, data);
		return rc;
	}

	memset(arg, 0, sizeof(arg));

	/* launch on all workers */
	RTE_LCORE_FOREACH_WORKER(lc) {
		arg[lc].rng = r;
		arg[lc].stats = init_stat;
		rte_eal_remote_launch(test, &arg[lc], lc);
	}

	/* signal worker to start test */
	__atomic_store_n(&wrk_cmd, WRK_CMD_RUN, __ATOMIC_RELEASE);

	rte_delay_us(run_time * US_PER_S);

	/* signal worker to start test */
	__atomic_store_n(&wrk_cmd, WRK_CMD_STOP, __ATOMIC_RELEASE);

	/* wait for workers and collect stats. */
	mc = rte_lcore_id();
	arg[mc].stats = init_stat;

	rc = 0;
	RTE_LCORE_FOREACH_WORKER(lc) {
		rc |= rte_eal_wait_lcore(lc);
		lcore_stat_aggr(&arg[mc].stats, &arg[lc].stats);
		if (verbose != 0)
			lcore_stat_dump(stdout, lc, &arg[lc].stats);
	}

	lcore_stat_dump(stdout, UINT32_MAX, &arg[mc].stats);
	mt1_fini(r, data);
	return rc;
}

static const struct test_case tests[] = {
	{
		.name = "MT-WRK_ENQ_DEQ-MST_NONE-PRCS",
		.func = test_mt1,
		.wfunc = test_worker_prcs,
	},
	{
		.name = "MT-WRK_ENQ_DEQ-MST_NONE-AVG",
		.func = test_mt1,
		.wfunc = test_worker_avg,
	},
};
