/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Huawei Technologies Co., Ltd
 */

/**
 * @file
 * This file contains implementation of SORING 'datapath' functions.
 *
 * Brief description:
 * ==================
 * enqueue/dequeue works the same as for conventional rte_ring:
 * any rte_ring sync types can be used, etc.
 * Plus there could be multiple 'stages'.
 * For each stage there is an acquire (start) and release (finish) operation.
 * After some elems are 'acquired' - user can safely assume that he has
 * exclusive possession of these elems till 'release' for them is done.
 * Note that right now user has to release exactly the same number of elems
 * he acquired before.
 * After 'release', elems can be 'acquired' by next stage and/or dequeued
 * (in case of last stage).
 *
 * Internal structure:
 * ===================
 * In addition to 'normal' ring of elems, it also has a ring of states of the
 * same size. Each state[] corresponds to exactly one elem[].
 * state[] will be used by acquire/release/dequeue functions to store internal
 * information and should not be accessed by the user directly.
 *
 * How it works:
 * =============
 * 'acquire()' just moves stage's head (same as rte_ring move_head does),
 * plus it saves in state[stage.cur_head] information about how many elems
 * were acquired, current head position and special flag value to indicate
 * that elems are acquired (SORING_ST_START).
 * Note that 'acquire()' returns to the user a special 'ftoken' that user has
 * to provide for 'release()' (in fact it is just a position for current head
 * plus current stage index).
 * 'release()' extracts old head value from provided ftoken and checks that
 * corresponding 'state[]' contains expected values(mostly for sanity
 * purposes).
 * Then it marks this state[] with 'SORING_ST_FINISH' flag to indicate
 * that given subset of objects was released.
 * After that, it checks does old head value equals to current tail value?
 * If yes, then it performs  'finalize()' operation, otherwise 'release()'
 * just returns (without spinning on stage tail value).
 * As updated state[] is shared by all threads, some other thread can do
 * 'finalize()' for given stage.
 * That allows 'release()' to avoid excessive waits on the tail value.
 * Main purpose of 'finalize()' operation is to walk through 'state[]'
 * from current stage tail up to its head, check state[] and move stage tail
 * through elements that already are in SORING_ST_FINISH state.
 * Along with that, corresponding state[] values are reset to zero.
 * Note that 'finalize()' for given stage can be done from multiple places:
 * 'release()' for that stage or from 'acquire()' for next stage
 * even from consumer's 'dequeue()' - in case given stage is the last one.
 * So 'finalize()' has to be MT-safe and inside it we have to
 * guarantee that only one thread will update state[] and stage's tail values.
 */

#include "soring.h"

#include <eal_export.h>

/*
 * Inline functions (fastpath) start here.
 */
static __rte_always_inline uint32_t
__rte_soring_stage_finalize(struct soring_stage_headtail *sht, uint32_t stage,
	union soring_state *rstate, uint32_t rmask, uint32_t maxn)
{
	int32_t rc;
	uint32_t ftkn, head, i, idx, k, n, tail;
	union soring_stage_tail nt, ot;
	union soring_state st;

	/* try to grab exclusive right to update tail value */
	ot.raw = rte_atomic_load_explicit(&sht->tail.raw,
			rte_memory_order_acquire);

	/* other thread already finalizing it for us */
	if (ot.sync != 0)
		return 0;

	nt.pos = ot.pos;
	nt.sync = 1;
	rc = rte_atomic_compare_exchange_strong_explicit(&sht->tail.raw,
		(uint64_t *)(uintptr_t)&ot.raw, nt.raw,
		rte_memory_order_release, rte_memory_order_relaxed);

	/* other thread won the race */
	if (rc == 0)
		return 0;

	/* Ensure the head is read before rstate[] */
	head = rte_atomic_load_explicit(&sht->head, rte_memory_order_relaxed);
	rte_atomic_thread_fence(rte_memory_order_acquire);

	/*
	 * start with current tail and walk through states that are
	 * already finished.
	 */

	n = RTE_MIN(head - ot.pos, maxn);
	for (i = 0, tail = ot.pos; i < n; i += k, tail += k) {

		idx = tail & rmask;
		ftkn = SORING_FTKN_MAKE(tail, stage);

		st.raw = rte_atomic_load_explicit(&rstate[idx].raw,
			rte_memory_order_relaxed);
		if ((st.stnum & SORING_ST_MASK) != SORING_ST_FINISH ||
				st.ftoken != ftkn)
			break;

		k = st.stnum & ~SORING_ST_MASK;
		rte_atomic_store_explicit(&rstate[idx].raw, 0,
				rte_memory_order_relaxed);
	}


	/* release exclusive right to update along with new tail value */
	ot.pos = tail;
	rte_atomic_store_explicit(&sht->tail.raw, ot.raw,
			rte_memory_order_release);

	return i;
}

static __rte_always_inline uint32_t
__rte_soring_move_prod_head(struct rte_soring *r, uint32_t num,
	enum rte_ring_queue_behavior behavior, enum rte_ring_sync_type st,
	uint32_t *head, uint32_t *next, uint32_t *free)
{
	uint32_t n;

	switch (st) {
	case RTE_RING_SYNC_ST:
	case RTE_RING_SYNC_MT:
		n = __rte_ring_headtail_move_head(&r->prod.ht, &r->cons.ht,
			r->capacity, st, num, behavior, head, next, free);
		break;
	case RTE_RING_SYNC_MT_RTS:
		n = __rte_ring_rts_move_head(&r->prod.rts, &r->cons.ht,
			r->capacity, num, behavior, head, free);
		*next = *head + n;
		break;
	case RTE_RING_SYNC_MT_HTS:
		n = __rte_ring_hts_move_head(&r->prod.hts, &r->cons.ht,
			r->capacity, num, behavior, head, free);
		*next = *head + n;
		break;
	default:
		/* unsupported mode, shouldn't be here */
		RTE_ASSERT(0);
		*free = 0;
		n = 0;
	}

	return n;
}

static __rte_always_inline uint32_t
__rte_soring_move_cons_head(struct rte_soring *r, uint32_t stage, uint32_t num,
	enum rte_ring_queue_behavior behavior, enum rte_ring_sync_type st,
	uint32_t *head, uint32_t *next, uint32_t *avail)
{
	uint32_t n;

	switch (st) {
	case RTE_RING_SYNC_ST:
	case RTE_RING_SYNC_MT:
		n = __rte_ring_headtail_move_head(&r->cons.ht,
			&r->stage[stage].ht, 0, st, num, behavior,
			head, next, avail);
		break;
	case RTE_RING_SYNC_MT_RTS:
		n = __rte_ring_rts_move_head(&r->cons.rts, &r->stage[stage].ht,
			0, num, behavior, head, avail);
		*next = *head + n;
		break;
	case RTE_RING_SYNC_MT_HTS:
		n = __rte_ring_hts_move_head(&r->cons.hts, &r->stage[stage].ht,
			0, num, behavior, head, avail);
		*next = *head + n;
		break;
	default:
		/* unsupported mode, shouldn't be here */
		RTE_ASSERT(0);
		*avail = 0;
		n = 0;
	}

	return n;
}

static __rte_always_inline void
__rte_soring_update_tail(struct __rte_ring_headtail *rht,
	enum rte_ring_sync_type st, uint32_t head, uint32_t next, uint32_t enq)
{
	uint32_t n;

	switch (st) {
	case RTE_RING_SYNC_ST:
	case RTE_RING_SYNC_MT:
		__rte_ring_update_tail(&rht->ht, head, next, st, enq);
		break;
	case RTE_RING_SYNC_MT_RTS:
		__rte_ring_rts_update_tail(&rht->rts);
		break;
	case RTE_RING_SYNC_MT_HTS:
		n = next - head;
		__rte_ring_hts_update_tail(&rht->hts, head, n, enq);
		break;
	default:
		/* unsupported mode, shouldn't be here */
		RTE_ASSERT(0);
	}
}

static __rte_always_inline uint32_t
__rte_soring_stage_move_head(struct soring_stage_headtail *d,
	const struct rte_ring_headtail *s, uint32_t capacity, uint32_t num,
	enum rte_ring_queue_behavior behavior,
	uint32_t *old_head, uint32_t *new_head, uint32_t *avail)
{
	uint32_t n, tail;

	*old_head = rte_atomic_load_explicit(&d->head,
			rte_memory_order_relaxed);

	do {
		n = num;

		/* Ensure the head is read before tail */
		rte_atomic_thread_fence(rte_memory_order_acquire);

		tail = rte_atomic_load_explicit(&s->tail,
				rte_memory_order_acquire);
		*avail = capacity + tail - *old_head;
		if (n > *avail)
			n = (behavior == RTE_RING_QUEUE_FIXED) ? 0 : *avail;
		if (n == 0)
			return 0;
		*new_head = *old_head + n;
	} while (rte_atomic_compare_exchange_strong_explicit(&d->head,
			old_head, *new_head, rte_memory_order_acq_rel,
			rte_memory_order_relaxed) == 0);

	return n;
}

static __rte_always_inline uint32_t
soring_enqueue(struct rte_soring *r, const void *objs,
	const void *meta, uint32_t n, enum rte_ring_queue_behavior behavior,
	uint32_t *free_space)
{
	enum rte_ring_sync_type st;
	uint32_t nb_free, prod_head, prod_next;

	RTE_ASSERT(r != NULL && r->nb_stage > 0);
	RTE_ASSERT(meta == NULL || r->meta != NULL);

	st = r->prod.ht.sync_type;

	n = __rte_soring_move_prod_head(r, n, behavior, st,
			&prod_head, &prod_next, &nb_free);
	if (n != 0) {
		__rte_ring_do_enqueue_elems(&r[1], objs, r->size,
			prod_head & r->mask, r->esize, n);
		if (meta != NULL)
			__rte_ring_do_enqueue_elems(r->meta, meta, r->size,
				prod_head & r->mask, r->msize, n);
		__rte_soring_update_tail(&r->prod, st, prod_head, prod_next, 1);
	}

	if (free_space != NULL)
		*free_space = nb_free - n;
	return n;
}

static __rte_always_inline uint32_t
soring_dequeue(struct rte_soring *r, void *objs, void *meta,
	uint32_t num, enum rte_ring_queue_behavior behavior,
	uint32_t *available)
{
	enum rte_ring_sync_type st;
	uint32_t entries, cons_head, cons_next, n, ns, reqn;

	RTE_ASSERT(r != NULL && r->nb_stage > 0);
	RTE_ASSERT(meta == NULL || r->meta != NULL);

	ns = r->nb_stage - 1;
	st = r->cons.ht.sync_type;

	/* try to grab exactly @num elems first */
	n = __rte_soring_move_cons_head(r, ns, num, RTE_RING_QUEUE_FIXED, st,
			&cons_head, &cons_next, &entries);
	if (n == 0) {
		/* try to finalize some elems from previous stage */
		n = __rte_soring_stage_finalize(&r->stage[ns].sht, ns,
			r->state, r->mask, 2 * num);
		entries += n;

		/* repeat attempt to grab elems */
		reqn = (behavior == RTE_RING_QUEUE_FIXED) ? num : 0;
		if (entries >= reqn)
			n = __rte_soring_move_cons_head(r, ns, num, behavior,
				st, &cons_head, &cons_next, &entries);
		else
			n = 0;
	}

	/* we have some elems to consume */
	if (n != 0) {
		__rte_ring_do_dequeue_elems(objs, &r[1], r->size,
			cons_head & r->mask, r->esize, n);
		if (meta != NULL)
			__rte_ring_do_dequeue_elems(meta, r->meta, r->size,
				cons_head & r->mask, r->msize, n);
		__rte_soring_update_tail(&r->cons, st, cons_head, cons_next, 0);
	}

	if (available != NULL)
		*available = entries - n;
	return n;
}

/*
 * Verify internal SORING state.
 * WARNING: if expected value is not equal to actual one, it means that for
 * whatever reason SORING data constancy is broken. That is a very serious
 * problem that most likely will cause race-conditions, memory corruption,
 * program crash.
 * To ease debugging it user might rebuild ring library with
 * RTE_SORING_DEBUG enabled.
 */
static __rte_always_inline void
soring_verify_state(const struct rte_soring *r, uint32_t stage, uint32_t idx,
	const char *msg, union soring_state val,  union soring_state exp)
{
	if (val.raw != exp.raw) {
#ifdef RTE_SORING_DEBUG
		rte_soring_dump(stderr, r);
		rte_panic("line:%d from:%s: soring=%p, stage=%#x, idx=%#x, "
			"expected={.stnum=%#x, .ftoken=%#x}, "
			"actual={.stnum=%#x, .ftoken=%#x};\n",
			__LINE__, msg, r, stage, idx,
			exp.stnum, exp.ftoken,
			val.stnum, val.ftoken);
#else
		SORING_LOG(EMERG, "from:%s: soring=%p, stage=%#x, idx=%#x, "
			"expected={.stnum=%#x, .ftoken=%#x}, "
			"actual={.stnum=%#x, .ftoken=%#x};",
			msg, r, stage, idx,
			exp.stnum, exp.ftoken,
			val.stnum, val.ftoken);
#endif
	}
}

/* check and update state ring at acquire op*/
static __rte_always_inline void
acquire_state_update(const struct rte_soring *r, uint32_t stage, uint32_t idx,
	uint32_t ftoken, uint32_t num)
{
	union soring_state st;
	const union soring_state est = {.raw = 0};

	st.raw = rte_atomic_load_explicit(&r->state[idx].raw,
			rte_memory_order_relaxed);
	soring_verify_state(r, stage, idx, __func__, st, est);

	st.ftoken = ftoken;
	st.stnum = (SORING_ST_START | num);

	rte_atomic_store_explicit(&r->state[idx].raw, st.raw,
			rte_memory_order_relaxed);
}

static __rte_always_inline uint32_t
soring_acquire(struct rte_soring *r, void *objs, void *meta,
	uint32_t stage, uint32_t num, enum rte_ring_queue_behavior behavior,
	uint32_t *ftoken, uint32_t *available)
{
	uint32_t avail, head, idx, n, next, reqn;
	struct soring_stage *pstg;
	struct soring_stage_headtail *cons;

	RTE_ASSERT(r != NULL && stage < r->nb_stage);
	RTE_ASSERT(meta == NULL || r->meta != NULL);

	cons = &r->stage[stage].sht;

	if (stage == 0)
		n = __rte_soring_stage_move_head(cons, &r->prod.ht, 0, num,
			behavior, &head, &next, &avail);
	else {
		pstg = r->stage + stage - 1;

		/* try to grab exactly @num elems */
		n = __rte_soring_stage_move_head(cons, &pstg->ht, 0, num,
			RTE_RING_QUEUE_FIXED, &head, &next, &avail);
		if (n == 0) {
			/* try to finalize some elems from previous stage */
			n = __rte_soring_stage_finalize(&pstg->sht, stage - 1,
				r->state, r->mask, 2 * num);
			avail += n;

			/* repeat attempt to grab elems */
			reqn = (behavior == RTE_RING_QUEUE_FIXED) ? num : 0;
			if (avail >= reqn)
				n = __rte_soring_stage_move_head(cons,
					&pstg->ht, 0, num, behavior, &head,
					&next, &avail);
			else
				n = 0;
		}
	}

	if (n != 0) {

		idx = head & r->mask;
		*ftoken = SORING_FTKN_MAKE(head, stage);

		/* check and update state value */
		acquire_state_update(r, stage, idx, *ftoken, n);

		/* copy elems that are ready for given stage */
		__rte_ring_do_dequeue_elems(objs, &r[1], r->size, idx,
				r->esize, n);
		if (meta != NULL)
			__rte_ring_do_dequeue_elems(meta, r->meta,
				r->size, idx, r->msize, n);
	}

	if (available != NULL)
		*available = avail - n;
	return n;
}

static __rte_always_inline void
soring_release(struct rte_soring *r, const void *objs,
	const void *meta, uint32_t stage, uint32_t n, uint32_t ftoken)
{
	uint32_t idx, pos, tail;
	struct soring_stage *stg;
	union soring_state st;

	const union soring_state est = {
		.stnum = (SORING_ST_START | n),
		.ftoken = ftoken,
	};

	RTE_ASSERT(r != NULL && stage < r->nb_stage);
	RTE_ASSERT(meta == NULL || r->meta != NULL);

	stg = r->stage + stage;

	pos = SORING_FTKN_POS(ftoken, stage);
	idx = pos & r->mask;
	st.raw = rte_atomic_load_explicit(&r->state[idx].raw,
			rte_memory_order_relaxed);

	/* check state ring contents */
	soring_verify_state(r, stage, idx, __func__, st, est);

	/* update contents of the ring, if necessary */
	if (objs != NULL)
		__rte_ring_do_enqueue_elems(&r[1], objs, r->size, idx,
			r->esize, n);
	if (meta != NULL)
		__rte_ring_do_enqueue_elems(r->meta, meta, r->size, idx,
			r->msize, n);

	/* set state to FINISH, make sure it is not reordered */
	rte_atomic_thread_fence(rte_memory_order_release);

	st.stnum = SORING_ST_FINISH | n;
	rte_atomic_store_explicit(&r->state[idx].raw, st.raw,
			rte_memory_order_relaxed);

	/* try to do finalize(), if appropriate */
	tail = rte_atomic_load_explicit(&stg->sht.tail.pos,
			rte_memory_order_relaxed);
	if (tail == pos)
		__rte_soring_stage_finalize(&stg->sht, stage, r->state, r->mask,
				r->capacity);
}

/*
 * Public functions (data-path) start here.
 */

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_soring_release, 25.03)
void
rte_soring_release(struct rte_soring *r, const void *objs,
	uint32_t stage, uint32_t n, uint32_t ftoken)
{
	soring_release(r, objs, NULL, stage, n, ftoken);
}


RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_soring_releasx, 25.03)
void
rte_soring_releasx(struct rte_soring *r, const void *objs,
	const void *meta, uint32_t stage, uint32_t n, uint32_t ftoken)
{
	soring_release(r, objs, meta, stage, n, ftoken);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_soring_enqueue_bulk, 25.03)
uint32_t
rte_soring_enqueue_bulk(struct rte_soring *r, const void *objs, uint32_t n,
	uint32_t *free_space)
{
	return soring_enqueue(r, objs, NULL, n, RTE_RING_QUEUE_FIXED,
			free_space);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_soring_enqueux_bulk, 25.03)
uint32_t
rte_soring_enqueux_bulk(struct rte_soring *r, const void *objs,
	const void *meta, uint32_t n, uint32_t *free_space)
{
	return soring_enqueue(r, objs, meta, n, RTE_RING_QUEUE_FIXED,
			free_space);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_soring_enqueue_burst, 25.03)
uint32_t
rte_soring_enqueue_burst(struct rte_soring *r, const void *objs, uint32_t n,
	uint32_t *free_space)
{
	return soring_enqueue(r, objs, NULL, n, RTE_RING_QUEUE_VARIABLE,
			free_space);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_soring_enqueux_burst, 25.03)
uint32_t
rte_soring_enqueux_burst(struct rte_soring *r, const void *objs,
	const void *meta, uint32_t n, uint32_t *free_space)
{
	return soring_enqueue(r, objs, meta, n, RTE_RING_QUEUE_VARIABLE,
			free_space);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_soring_dequeue_bulk, 25.03)
uint32_t
rte_soring_dequeue_bulk(struct rte_soring *r, void *objs, uint32_t num,
	uint32_t *available)
{
	return soring_dequeue(r, objs, NULL, num, RTE_RING_QUEUE_FIXED,
			available);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_soring_dequeux_bulk, 25.03)
uint32_t
rte_soring_dequeux_bulk(struct rte_soring *r, void *objs, void *meta,
	uint32_t num, uint32_t *available)
{
	return soring_dequeue(r, objs, meta, num, RTE_RING_QUEUE_FIXED,
			available);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_soring_dequeue_burst, 25.03)
uint32_t
rte_soring_dequeue_burst(struct rte_soring *r, void *objs, uint32_t num,
	uint32_t *available)
{
	return soring_dequeue(r, objs, NULL, num, RTE_RING_QUEUE_VARIABLE,
			available);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_soring_dequeux_burst, 25.03)
uint32_t
rte_soring_dequeux_burst(struct rte_soring *r, void *objs, void *meta,
	uint32_t num, uint32_t *available)
{
	return soring_dequeue(r, objs, meta, num, RTE_RING_QUEUE_VARIABLE,
			available);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_soring_acquire_bulk, 25.03)
uint32_t
rte_soring_acquire_bulk(struct rte_soring *r, void *objs,
	uint32_t stage, uint32_t num, uint32_t *ftoken, uint32_t *available)
{
	return soring_acquire(r, objs, NULL, stage, num,
			RTE_RING_QUEUE_FIXED, ftoken, available);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_soring_acquirx_bulk, 25.03)
uint32_t
rte_soring_acquirx_bulk(struct rte_soring *r, void *objs, void *meta,
	uint32_t stage, uint32_t num, uint32_t *ftoken, uint32_t *available)
{
	return soring_acquire(r, objs, meta, stage, num,
			RTE_RING_QUEUE_FIXED, ftoken, available);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_soring_acquire_burst, 25.03)
uint32_t
rte_soring_acquire_burst(struct rte_soring *r, void *objs,
	uint32_t stage, uint32_t num, uint32_t *ftoken, uint32_t *available)
{
	return soring_acquire(r, objs, NULL, stage, num,
			RTE_RING_QUEUE_VARIABLE, ftoken, available);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_soring_acquirx_burst, 25.03)
uint32_t
rte_soring_acquirx_burst(struct rte_soring *r, void *objs, void *meta,
	uint32_t stage, uint32_t num, uint32_t *ftoken, uint32_t *available)
{
	return soring_acquire(r, objs, meta, stage, num,
			RTE_RING_QUEUE_VARIABLE, ftoken, available);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_soring_count, 25.03)
unsigned int
rte_soring_count(const struct rte_soring *r)
{
	uint32_t prod_tail = r->prod.ht.tail;
	uint32_t cons_tail = r->cons.ht.tail;
	uint32_t count = (prod_tail - cons_tail) & r->mask;
	return (count > r->capacity) ? r->capacity : count;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_soring_free_count, 25.03)
unsigned int
rte_soring_free_count(const struct rte_soring *r)
{
	return r->capacity - rte_soring_count(r);
}
