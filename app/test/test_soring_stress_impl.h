/* SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdalign.h>

#include "test_soring_stress.h"

/**
 * Stress test for soring enqueue/dequeue/acquire/release operations.
 * Depending on the role, performs at least one of the following patterns
 * on each worker:
 * - dequeue/read-write data from/to the dequeued objects/enqueue.
 * - acquire/read-write data from/to the acquired objects/release.
 * Serves as both functional and performance test of soring
 * data-path API under high contention
 * (for both over committed and non-over committed scenarios).
 */

#define RING_NAME	"SORING_STRESS"
#define BULK_NUM	32
#define RING_SIZE	(2 * BULK_NUM * RTE_MAX_LCORE)

#define MAX_STAGES	16

enum {
	WRK_CMD_STOP,
	WRK_CMD_RUN,
};

static alignas(RTE_CACHE_LINE_SIZE) RTE_ATOMIC(uint32_t) wrk_cmd = WRK_CMD_STOP;

/* test run-time in seconds */
static const uint32_t run_time = 60;
static const uint32_t verbose;

static rte_spinlock_t dump_lock;

struct lcore_op_stat {
	uint64_t nb_lcore;
	uint64_t nb_call;
	uint64_t nb_obj;
	uint64_t nb_cycle;
	uint64_t max_cycle;
	uint64_t min_cycle;
};

struct lcore_stat {
	uint64_t nb_cycle;
	struct lcore_op_stat deqenq;
	uint32_t role_mask;
	uint32_t nb_stage;
	struct lcore_op_stat stage[MAX_STAGES];
};

#define	ROLE_DEQENQ	RTE_BIT32(0)
#define	ROLE_STAGE(n)	RTE_BIT32(n + 1)

struct __rte_cache_aligned lcore_arg {
	struct rte_soring *rng;
	struct lcore_stat stats;
};

struct __rte_cache_aligned ring_elem {
	uint32_t cnt[RTE_CACHE_LINE_SIZE / sizeof(uint32_t)];
};

/*
 * redefinable functions
 */

static uint32_t
_st_ring_dequeue_burst(struct rte_soring *r, void **obj, uint32_t n,
	uint32_t *avail);

static uint32_t
_st_ring_enqueue_bulk(struct rte_soring *r, void * const *obj, uint32_t n,
	uint32_t *free);

static uint32_t
_st_ring_acquire_burst(struct rte_soring *r, uint32_t stage, void **obj,
	uint32_t num, uint32_t *token, uint32_t *avail);

static void
_st_ring_release(struct rte_soring *r, uint32_t stage, uint32_t token,
	void * const *obj, uint32_t num);

static void
lcore_op_stat_update(struct lcore_op_stat *ls, uint64_t call, uint64_t obj,
	uint64_t tm, int32_t prcs)
{
	ls->nb_call += call;
	ls->nb_obj += obj;
	ls->nb_cycle += tm;
	if (prcs) {
		ls->max_cycle = RTE_MAX(ls->max_cycle, tm);
		ls->min_cycle = RTE_MIN(ls->min_cycle, tm);
	}
}

static void
lcore_stat_update(struct lcore_stat *ls, uint64_t call, uint64_t obj,
	uint64_t tm, int32_t prcs)
{
	uint32_t i;

	ls->nb_cycle += tm;
	lcore_op_stat_update(&ls->deqenq, call, obj, tm, prcs);
	for (i = 0; i != ls->nb_stage; i++)
		lcore_op_stat_update(ls->stage + i, call, obj, tm, prcs);
}

static void
lcore_op_stat_aggr(struct lcore_op_stat *ms, const struct lcore_op_stat *ls)
{
	ms->nb_call += ls->nb_call;
	ms->nb_obj += ls->nb_obj;
	ms->nb_cycle += ls->nb_cycle;
	ms->max_cycle = RTE_MAX(ms->max_cycle, ls->max_cycle);
	ms->min_cycle = RTE_MIN(ms->min_cycle, ls->min_cycle);
}

static void
lcore_stat_aggr(struct lcore_stat *ms, const struct lcore_stat *ls)
{
	uint32_t i;

	ms->nb_cycle = RTE_MAX(ms->nb_cycle, ls->nb_cycle);
	lcore_op_stat_aggr(&ms->deqenq, &ls->deqenq);
	ms->deqenq.nb_lcore += ((ls->role_mask & ROLE_DEQENQ) != 0);
	for (i = 0; i != ms->nb_stage; i++) {
		lcore_op_stat_aggr(ms->stage + i, ls->stage + i);
		ms->stage[i].nb_lcore += ((ls->role_mask & ROLE_STAGE(i)) != 0);
	}
}

static void
lcore_op_stat_dump(FILE *f, const struct lcore_op_stat *ls, const char *cap,
	long double st)
{
	fprintf(f, "\t%s={\n", cap);

	fprintf(f, "\t\tnb_lcore=%" PRIu64 ",\n", ls->nb_lcore);
	fprintf(f, "\t\tnb_call=%" PRIu64 ",\n", ls->nb_call);
	fprintf(f, "\t\tnb_obj=%" PRIu64 ",\n", ls->nb_obj);
	fprintf(f, "\t\tnb_cycle=%" PRIu64 ",\n", ls->nb_cycle);
	fprintf(f, "\t\tobj/call(avg): %.2Lf\n",
		(long double)ls->nb_obj / ls->nb_call);
	fprintf(f, "\t\tcycles/obj(avg): %.2Lf\n",
		(long double)ls->nb_cycle / ls->nb_obj);
	fprintf(f, "\t\tcycles/call(avg): %.2Lf\n",
		(long double)ls->nb_cycle / ls->nb_call);

	/* if min/max cycles per call stats was collected */
	if (ls->min_cycle != UINT64_MAX) {
		fprintf(f, "\t\tmax cycles/call=%" PRIu64 "(%.2Lf usec),\n",
			ls->max_cycle,
			(long double)ls->max_cycle / st);
		fprintf(f, "\t\tmin cycles/call=%" PRIu64 "(%.2Lf usec),\n",
			ls->min_cycle,
			(long double)ls->min_cycle / st);
	}

	fprintf(f, "\t},\n");
}

static void
lcore_stat_dump(FILE *f, uint32_t lc, const struct lcore_stat *ls)
{
	uint32_t i;
	long double st;
	char cap[64];

	st = (long double)rte_get_timer_hz() / US_PER_S;

	if (lc == UINT32_MAX)
		fprintf(f, "%s(AGGREGATE)={\n", __func__);
	else
		fprintf(f, "%s(lcore=%u)={\n", __func__, lc);

	fprintf(f, "\tnb_cycle=%" PRIu64 "(%.2Lf usec),\n",
		ls->nb_cycle, (long double)ls->nb_cycle / st);

	lcore_op_stat_dump(f, &ls->deqenq, "DEQ+ENQ", st);
	for (i = 0; i != ls->nb_stage; i++) {
		snprintf(cap, sizeof(cap), "%s#%u", "STAGE", i);
		lcore_op_stat_dump(f, ls->stage + i, cap, st);
	}

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
	const struct ring_elem *check, const struct ring_elem *fill,
	const char *fname, const char *opname, const struct rte_soring *sor)
{
	uint32_t i;

	for (i = 0; i != num; i++) {
		if (memcmp(check, elm[i], sizeof(*check)) != 0) {
			rte_spinlock_lock(&dump_lock);
			printf("%s:%s: %s(lc=%u, num=%u) failed at %u-th iter, "
				"offending object: %p\n",
				fname, opname, __func__, rte_lcore_id(), num, i,
				elm[i]);
			rte_memdump(stdout, "expected", check, sizeof(*check));
			rte_memdump(stdout, "result", elm[i], sizeof(*elm[i]));
			rte_soring_dump(stdout, sor);
			rte_spinlock_unlock(&dump_lock);
			return -EINVAL;
		}
		memcpy(elm[i], fill, sizeof(*elm[i]));
	}

	return 0;
}

static int
check_ring_op(uint32_t exp, uint32_t res, uint32_t lc,
	enum rte_ring_queue_behavior bhv, const char *fname, const char *opname,
	const struct rte_soring *sor)
{
	if ((bhv == RTE_RING_QUEUE_FIXED && exp != res) ||
			(bhv == RTE_RING_QUEUE_VARIABLE && exp < res)) {
		rte_spinlock_lock(&dump_lock);
		printf("%s(lc=%u) failure: %s expected: %u, returned %u\n",
			fname, lc, opname, exp, res);
		rte_soring_dump(stdout, sor);
		rte_spinlock_unlock(&dump_lock);
		return -ENOSPC;
	}
	return 0;
}

/* num in interval [7/8, 11/8] of BULK_NUM */
static inline uint32_t
rand_elem_num(void)
{
	uint32_t num;

	num = 7 * BULK_NUM / 8 + rte_rand() % (BULK_NUM / 2);
	return num;
}

/*
 * for each enabled stage do:
 *   acquire burst of objects
 *   read and check their contents
 *   update and check their contents
 *   release burst of objects
 * done
 */
static int32_t
test_worker_stages(struct lcore_arg *la, uint32_t lc, const char *fname,
	struct ring_elem *obj[2 * BULK_NUM],
	const struct ring_elem *def_elm, const struct ring_elem *loc_elm,
	const struct ring_elem stg_elm[MAX_STAGES], int32_t prcs)
{
	int32_t rc;
	uint32_t i, n, num, tkn;
	uint64_t tm0, tm1;
	const struct ring_elem *celm, *pelm;

	num = rand_elem_num();

	rc = 0;
	tkn = 0;
	for (i = 0, pelm = def_elm; i != la->stats.nb_stage; pelm = celm, i++) {

		celm = stg_elm + i;

		/* given stage is not enabled on that lcore */
		if ((la->stats.role_mask & ROLE_STAGE(i)) == 0)
			continue;

		/* reset all pointer values */
		memset(obj, 0, sizeof(*obj) * num);

		/* acquire num elems */
		tm0 = (prcs != 0) ? rte_rdtsc_precise() : 0;
		n = _st_ring_acquire_burst(la->rng, i, (void **)obj, num,
				&tkn, NULL);
		tm0 = (prcs != 0) ? rte_rdtsc_precise() - tm0 : 0;

		/* check return value and objects */
		rc = check_ring_op(num, n, lc, RTE_RING_QUEUE_VARIABLE, fname,
			RTE_STR(_st_ring_stage_acquire), la->rng);
		if (rc == 0)
			rc = check_updt_elem(obj, n, pelm, loc_elm, fname,
				RTE_STR(_st_ring_stage_acquire), la->rng);
		if (rc != 0)
			break;

		/* release num elems */
		rte_compiler_barrier();
		rc = check_updt_elem(obj, n, loc_elm, celm, fname,
			RTE_STR(_st_ring_stage_release), la->rng);
		if (rc != 0)
			break;

		if (n == 0)
			tm1 = 0;
		else {
			tm1 = (prcs != 0) ? rte_rdtsc_precise() : 0;
			_st_ring_release(la->rng, i, tkn,
					(void **)obj, n);
			tm1 = (prcs != 0) ? rte_rdtsc_precise() - tm1 : 0;
		}
		lcore_op_stat_update(la->stats.stage + i, 1, n, tm0 + tm1,
				prcs);
	}

	return rc;
}

static int32_t
test_worker_deqenq(struct lcore_arg *la, uint32_t lc, const char *fname,
	struct ring_elem *obj[2 * BULK_NUM],
	const struct ring_elem *def_elm, const struct ring_elem *loc_elm,
	const struct ring_elem *pelm, int32_t prcs)
{
	int32_t rc;
	uint32_t k, n, num;
	uint64_t tm0, tm1;

	num = rand_elem_num();

	/* reset all pointer values */
	memset(obj, 0, sizeof(*obj) * num);

	/* dequeue num elems */
	tm0 = (prcs != 0) ? rte_rdtsc_precise() : 0;
	n = _st_ring_dequeue_burst(la->rng, (void **)obj, num, NULL);

	tm0 = (prcs != 0) ? rte_rdtsc_precise() - tm0 : 0;

	/* check return value and objects */
	rc = check_ring_op(num, n, lc, RTE_RING_QUEUE_VARIABLE, fname,
			RTE_STR(_st_ring_dequeue_bulk), la->rng);
	if (rc == 0)
		rc = check_updt_elem(obj, n, pelm, loc_elm, fname,
			RTE_STR(_st_ring_dequeue_bulk), la->rng);
	if (rc != 0)
		return rc;

	/* enqueue n elems */
	rte_compiler_barrier();
	rc = check_updt_elem(obj, n, loc_elm, def_elm, fname,
		RTE_STR(_st_ring_enqueue_bulk), la->rng);
	if (rc != 0)
		return rc;

	tm1 = (prcs != 0) ? rte_rdtsc_precise() : 0;
	k = _st_ring_enqueue_bulk(la->rng, (void **)obj, n, NULL);
	tm1 = (prcs != 0) ? rte_rdtsc_precise() - tm1 : 0;

	/* check return value */
	rc = check_ring_op(n, k, lc, RTE_RING_QUEUE_FIXED, fname,
			RTE_STR(_st_ring_enqueue_bulk), la->rng);
	if (rc != 0)
		return rc;

	lcore_op_stat_update(&la->stats.deqenq, 1, n, tm0 + tm1, prcs);
	return 0;
}

static int
test_worker(void *arg, const char *fname, int32_t prcs)
{
	int32_t rc;
	uint32_t i, lc;
	uint64_t cl;
	struct lcore_arg *la;
	struct ring_elem *obj[2 * BULK_NUM];
	struct ring_elem *pelm, def_elm, loc_elm, stg_elm[MAX_STAGES];

	la = arg;
	lc = rte_lcore_id();

	fill_ring_elm(&def_elm, UINT32_MAX);
	fill_ring_elm(&loc_elm, lc);

	for (i = 0; i != RTE_DIM(stg_elm); i++)
		fill_ring_elm(stg_elm + i, (i + 1) << 24);

	pelm = stg_elm + la->stats.nb_stage - 1;

	/* Acquire ordering is not required as the main is not
	 * really releasing any data through 'wrk_cmd' to
	 * the worker.
	 */
	while (rte_atomic_load_explicit(&wrk_cmd, rte_memory_order_relaxed) !=
			WRK_CMD_RUN)
		rte_pause();

	cl = rte_rdtsc_precise();

	do {
		rc = 0;

		if ((la->stats.role_mask & ~ROLE_DEQENQ) != 0) {
			rc = test_worker_stages(la, lc, fname, obj,
				&def_elm, &loc_elm, stg_elm, prcs);
			if (rc != 0)
				break;
		}

		if ((la->stats.role_mask & ROLE_DEQENQ) != 0) {
			rc = test_worker_deqenq(la, lc, fname, obj,
				&def_elm, &loc_elm, pelm, prcs);
			if (rc != 0)
				break;
		}

	} while (rte_atomic_load_explicit(&wrk_cmd,
				rte_memory_order_relaxed) == WRK_CMD_RUN);

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
mt1_fini(struct rte_soring *rng, void *data)
{
	rte_free(rng);
	rte_free(data);
}

static int
mt1_init(struct rte_soring **rng, void **data, uint32_t num,
	enum rte_ring_sync_type prod_synt, enum rte_ring_sync_type cons_synt,
	uint32_t nb_stages)
{
	int32_t rc;
	size_t sz;
	uint32_t i;
	struct rte_soring *r;
	struct ring_elem *elm;
	void *p;
	struct rte_soring_param prm;

	*rng = NULL;
	*data = NULL;

	sz = num * sizeof(*elm);
	elm = rte_zmalloc(NULL, sz, alignof(typeof(*elm)));
	if (elm == NULL) {
		printf("%s: alloc(%zu) for %u elems data failed",
			__func__, sz, num);
		return -ENOMEM;
	}

	*data = elm;

	/* alloc soring */
	memset(&prm, 0, sizeof(prm));

	prm.name = __func__;
	prm.elems = num;
	prm.elem_size = sizeof(uintptr_t);
	prm.stages = nb_stages;
	prm.prod_synt = prod_synt;
	prm.cons_synt = cons_synt;

	sz = rte_soring_get_memsize(&prm);
	r = rte_zmalloc(NULL, sz, RTE_CACHE_LINE_SIZE);
	if (r == NULL) {
		printf("%s: alloc(%zu) for FIFO with %u elems failed",
			__func__, sz, prm.elems);
		return -ENOMEM;
	}

	*rng = r;

	rc = rte_soring_init(r, &prm);
	if (rc != 0) {
		printf("%s: rte_soring_init(r=%p,elems=%u,stages=%u) failed, "
			"error: %d(%s)\n",
			__func__, r, prm.elems, prm.stages, rc, strerror(-rc));
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
test_mt(int (*test)(void *), enum rte_ring_sync_type prod_synt,
	enum rte_ring_sync_type cons_synt, uint32_t nb_stage,
	const uint32_t role_mask[RTE_MAX_LCORE])
{
	int32_t rc;
	uint32_t i, lc, mc;
	struct rte_soring *r;
	void *data;
	struct lcore_arg arg[RTE_MAX_LCORE];

	static const struct lcore_op_stat init_stat = {
		.min_cycle = UINT64_MAX,
	};

	rc = mt1_init(&r, &data, RING_SIZE, prod_synt, cons_synt, nb_stage);

	if (rc != 0) {
		mt1_fini(r, data);
		return rc;
	}

	memset(arg, 0, sizeof(arg));

	/* launch on all workers */
	RTE_LCORE_FOREACH_WORKER(lc) {
		arg[lc].rng = r;
		arg[lc].stats.deqenq = init_stat;
		arg[lc].stats.nb_stage = nb_stage;
		arg[lc].stats.role_mask = role_mask[lc];
		for (i = 0; i != arg[lc].stats.nb_stage; i++)
			arg[lc].stats.stage[i] = init_stat;
		rte_eal_remote_launch(test, &arg[lc], lc);
	}

	/* signal workers to start test */
	rte_atomic_store_explicit(&wrk_cmd, WRK_CMD_RUN,
			rte_memory_order_release);

	rte_delay_us(run_time * US_PER_S);

	/* signal workers to stop test */
	rte_atomic_store_explicit(&wrk_cmd, WRK_CMD_STOP,
			rte_memory_order_release);

	/* wait for workers and collect stats. */
	mc = rte_lcore_id();
	arg[mc].stats.deqenq = init_stat;
	arg[mc].stats.nb_stage = nb_stage;
	for (i = 0; i != arg[mc].stats.nb_stage; i++)
		arg[mc].stats.stage[i] = init_stat;

	rc = 0;
	RTE_LCORE_FOREACH_WORKER(lc) {
		rc |= rte_eal_wait_lcore(lc);
		lcore_stat_aggr(&arg[mc].stats, &arg[lc].stats);
		if (verbose != 0)
			lcore_stat_dump(stdout, lc, &arg[lc].stats);
	}

	lcore_stat_dump(stdout, UINT32_MAX, &arg[mc].stats);
	rte_soring_dump(stdout, r);
	mt1_fini(r, data);
	return rc;
}

/*
 * launch all stages and deq+enq on all worker lcores
 */
static void
role_mask_sym(uint32_t nb_stage, uint32_t role_mask[RTE_MAX_LCORE])
{
	uint32_t lc;
	const uint32_t msk =  RTE_BIT32(nb_stage + 2) - 1;

	memset(role_mask, 0, sizeof(role_mask[0]) * RTE_MAX_LCORE);
	RTE_LCORE_FOREACH_WORKER(lc)
		role_mask[lc] = msk;
}

/*
 * Divide all workers in two (nearly) equal groups:
 * - workers from 'even' group do deque+enque
 * - workers from 'odd' group do acquire/release (for all stages)
 */
static void
role_mask_even_odd(uint32_t nb_stage, uint32_t role_mask[RTE_MAX_LCORE])
{
	uint32_t i, lc;
	const uint32_t msk[2] = {
		[0] = ROLE_DEQENQ,
		[1] =  RTE_GENMASK32(nb_stage + 1, 1),
	};

	memset(role_mask, 0, sizeof(role_mask[0]) * RTE_MAX_LCORE);

	i = 0;
	RTE_LCORE_FOREACH_WORKER(lc) {
		role_mask[lc] = msk[i & 1];
		i++;
	}
	if (i == 1) {
		lc = rte_get_next_lcore(-1, 1, 0);
		role_mask[lc] |= msk[i & 1];
	}
}

/*
 * Divide all workers (nearly) evenly among all possible stages
 */
static void
role_mask_div(uint32_t nb_stage, uint32_t role_mask[RTE_MAX_LCORE])
{
	const uint32_t msk_count = nb_stage + 1;
	uint32_t i, lc;
	uint32_t *msk = alloca(sizeof(uint32_t) * msk_count);

	memset(role_mask, 0, sizeof(role_mask[0]) * RTE_MAX_LCORE);

	for (i = 0; i != msk_count; i++) {
		msk[i] = RTE_BIT32(i);
	};

	i = 0;
	RTE_LCORE_FOREACH_WORKER(lc) {
		role_mask[lc] = msk[i % msk_count];
		i++;
	}
	if (i < msk_count) {
		lc = rte_get_next_lcore(-1, 1, 0);
		for (; i != msk_count; i++)
			role_mask[lc] |= msk[i % msk_count];
	}
}

/*
 * one worker does ST enqueue+dequeue, while all others - stages processing.
 */
static void
role_mask_denq_st(uint32_t nb_stage, uint32_t role_mask[RTE_MAX_LCORE])
{
	uint32_t i, lc;
	const uint32_t msk[2] = {
		[0] = ROLE_DEQENQ,
		[1] =  RTE_GENMASK32(nb_stage + 1, 1),
	};

	memset(role_mask, 0, sizeof(role_mask[0]) * RTE_MAX_LCORE);

	i = 0;
	RTE_LCORE_FOREACH_WORKER(lc) {
		if (i == 0)
			role_mask[lc] = msk[0];
		else
			role_mask[lc] = msk[1];
		i++;
	}
	if (i == 1) {
		lc = rte_get_next_lcore(-1, 1, 0);
		role_mask[lc] |= msk[1];
	}
}


static int
test_sym_mt1(int (*test)(void *))
{
	uint32_t role_mask[RTE_MAX_LCORE];
	const uint32_t nb_stage = 1;

	role_mask_sym(nb_stage, role_mask);
	return test_mt(test, RTE_RING_SYNC_MT, RTE_RING_SYNC_MT,
			nb_stage, role_mask);
}

static int
test_sym_mt4(int (*test)(void *))
{
	uint32_t role_mask[RTE_MAX_LCORE];

	const uint32_t nb_stage = 4;

	role_mask_sym(nb_stage, role_mask);
	return test_mt(test, RTE_RING_SYNC_MT, RTE_RING_SYNC_MT,
			nb_stage, role_mask);
}

static int
test_sym_mt_rts4(int (*test)(void *))
{
	uint32_t role_mask[RTE_MAX_LCORE];

	const uint32_t nb_stage = 4;

	role_mask_sym(nb_stage, role_mask);
	return test_mt(test, RTE_RING_SYNC_MT_RTS, RTE_RING_SYNC_MT_RTS,
			nb_stage, role_mask);
}

static int
test_sym_mt_hts4(int (*test)(void *))
{
	uint32_t role_mask[RTE_MAX_LCORE];

	const uint32_t nb_stage = 4;

	role_mask_sym(nb_stage, role_mask);
	return test_mt(test, RTE_RING_SYNC_MT_HTS, RTE_RING_SYNC_MT_HTS,
			nb_stage, role_mask);
}

static int
test_stdenq_stage4(int (*test)(void *))
{
	uint32_t role_mask[RTE_MAX_LCORE];

	const uint32_t nb_stage = 4;

	role_mask_denq_st(nb_stage, role_mask);
	return test_mt(test, RTE_RING_SYNC_ST, RTE_RING_SYNC_ST,
			nb_stage, role_mask);
}


static int
test_even_odd_mt5(int (*test)(void *))
{
	uint32_t role_mask[RTE_MAX_LCORE];

	const uint32_t nb_stage = 5;

	role_mask_even_odd(nb_stage, role_mask);
	return test_mt(test, RTE_RING_SYNC_MT, RTE_RING_SYNC_MT,
			nb_stage, role_mask);
}

static int
test_div_mt3(int (*test)(void *))
{
	uint32_t role_mask[RTE_MAX_LCORE];

	const uint32_t nb_stage = 3;

	role_mask_div(nb_stage, role_mask);
	return test_mt(test, RTE_RING_SYNC_MT, RTE_RING_SYNC_MT,
			nb_stage, role_mask);
}

static const struct test_case tests[] = {
	{
		.name = "MT_DEQENQ-MT_STG1-PRCS",
		.func = test_sym_mt1,
		.wfunc = test_worker_prcs,
	},
	{
		.name = "MT_DEQENQ-MT_STG1-AVG",
		.func = test_sym_mt1,
		.wfunc = test_worker_avg,
	},
	{
		.name = "MT_DEQENQ-MT_STG4-PRCS",
		.func = test_sym_mt4,
		.wfunc = test_worker_prcs,
	},
	{
		.name = "MT_DEQENQ-MT_STG4-AVG",
		.func = test_sym_mt4,
		.wfunc = test_worker_avg,
	},
	{
		.name = "MTRTS_DEQENQ-MT_STG4-PRCS",
		.func = test_sym_mt_rts4,
		.wfunc = test_worker_prcs,
	},
	{
		.name = "MTRTS_DEQENQ-MT_STG4-AVG",
		.func = test_sym_mt_rts4,
		.wfunc = test_worker_avg,
	},
	{
		.name = "MTHTS_DEQENQ-MT_STG4-PRCS",
		.func = test_sym_mt_hts4,
		.wfunc = test_worker_prcs,
	},
	{
		.name = "MTHTS_DEQENQ-MT_STG4-AVG",
		.func = test_sym_mt_hts4,
		.wfunc = test_worker_avg,
	},
	{
		.name = "MT_DEQENQ-MT_STG5-1:1-PRCS",
		.func = test_even_odd_mt5,
		.wfunc = test_worker_prcs,
	},
	{
		.name = "MT_DEQENQ-MT_STG5-1:1-AVG",
		.func = test_even_odd_mt5,
		.wfunc = test_worker_avg,
	},
	{
		.name = "MT_DEQENQ-MT_STG3-1:3-PRCS",
		.func = test_div_mt3,
		.wfunc = test_worker_prcs,
	},
	{
		.name = "MT_DEQENQ_MT_STG3-1:3-AVG",
		.func = test_div_mt3,
		.wfunc = test_worker_avg,
	},
	{
		.name = "ST_DEQENQ-MT_STG4-PRCS",
		.func = test_stdenq_stage4,
		.wfunc = test_worker_prcs,
	},
	{
		.name = "ST_DEQENQ-MT_STG4-AVG",
		.func = test_stdenq_stage4,
		.wfunc = test_worker_avg,
	},
};
