/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CNXK_TIM_WORKER_H__
#define __CNXK_TIM_WORKER_H__

#include "cnxk_tim_evdev.h"

static inline uint8_t
cnxk_tim_bkt_fetch_lock(uint64_t w1)
{
	return (w1 >> TIM_BUCKET_W1_S_LOCK) & TIM_BUCKET_W1_M_LOCK;
}

static inline int16_t
cnxk_tim_bkt_fetch_rem(uint64_t w1)
{
	return (w1 >> TIM_BUCKET_W1_S_CHUNK_REMAINDER) &
	       TIM_BUCKET_W1_M_CHUNK_REMAINDER;
}

static inline int16_t
cnxk_tim_bkt_get_rem(struct cnxk_tim_bkt *bktp)
{
	return rte_atomic_load_explicit(&bktp->chunk_remainder, rte_memory_order_acquire);
}

static inline void
cnxk_tim_bkt_set_rem(struct cnxk_tim_bkt *bktp, uint16_t v)
{
	rte_atomic_store_explicit(&bktp->chunk_remainder, v, rte_memory_order_relaxed);
}

static inline void
cnxk_tim_bkt_sub_rem(struct cnxk_tim_bkt *bktp, uint16_t v)
{
	rte_atomic_fetch_sub_explicit(&bktp->chunk_remainder, v, rte_memory_order_relaxed);
}

static inline uint8_t
cnxk_tim_bkt_get_hbt(uint64_t w1)
{
	return (w1 >> TIM_BUCKET_W1_S_HBT) & TIM_BUCKET_W1_M_HBT;
}

static inline uint8_t
cnxk_tim_bkt_get_bsk(uint64_t w1)
{
	return (w1 >> TIM_BUCKET_W1_S_BSK) & TIM_BUCKET_W1_M_BSK;
}

static inline uint64_t
cnxk_tim_bkt_clr_bsk(struct cnxk_tim_bkt *bktp)
{
	/* Clear everything except lock. */
	const uint64_t v = TIM_BUCKET_W1_M_LOCK << TIM_BUCKET_W1_S_LOCK;

	return rte_atomic_fetch_and_explicit(&bktp->w1, v, rte_memory_order_acq_rel);
}

static inline uint64_t
cnxk_tim_bkt_fetch_sema_lock(struct cnxk_tim_bkt *bktp)
{
	return rte_atomic_fetch_add_explicit(&bktp->w1, TIM_BUCKET_SEMA_WLOCK,
					     rte_memory_order_acquire);
}

static inline uint64_t
cnxk_tim_bkt_fetch_sema(struct cnxk_tim_bkt *bktp)
{
	return rte_atomic_fetch_add_explicit(&bktp->w1, TIM_BUCKET_SEMA, rte_memory_order_relaxed);
}

static inline uint64_t
cnxk_tim_bkt_inc_lock(struct cnxk_tim_bkt *bktp)
{
	const uint64_t v = 1ull << TIM_BUCKET_W1_S_LOCK;

	return rte_atomic_fetch_add_explicit(&bktp->w1, v, rte_memory_order_acquire);
}

static inline void
cnxk_tim_bkt_dec_lock(struct cnxk_tim_bkt *bktp)
{
	rte_atomic_fetch_sub_explicit(&bktp->lock, 1, rte_memory_order_release);
}

static inline void
cnxk_tim_bkt_dec_lock_relaxed(struct cnxk_tim_bkt *bktp)
{
	rte_atomic_fetch_sub_explicit(&bktp->lock, 1, rte_memory_order_relaxed);
}

static inline uint32_t
cnxk_tim_bkt_get_nent(uint64_t w1)
{
	return (w1 >> TIM_BUCKET_W1_S_NUM_ENTRIES) &
	       TIM_BUCKET_W1_M_NUM_ENTRIES;
}

static inline void
cnxk_tim_bkt_inc_nent(struct cnxk_tim_bkt *bktp)
{
	rte_atomic_fetch_add_explicit(&bktp->nb_entry, 1, rte_memory_order_release);
}

static inline void
cnxk_tim_bkt_add_nent_relaxed(struct cnxk_tim_bkt *bktp, uint32_t v)
{
	rte_atomic_fetch_add_explicit(&bktp->nb_entry, v, rte_memory_order_relaxed);
}

static inline void
cnxk_tim_bkt_add_nent(struct cnxk_tim_bkt *bktp, uint32_t v)
{
	rte_atomic_fetch_add_explicit(&bktp->nb_entry, v, rte_memory_order_release);
}

static inline uint64_t
cnxk_tim_bkt_clr_nent(struct cnxk_tim_bkt *bktp)
{
	const uint64_t v =
		~(TIM_BUCKET_W1_M_NUM_ENTRIES << TIM_BUCKET_W1_S_NUM_ENTRIES);

	return rte_atomic_fetch_and_explicit(&bktp->w1, v, rte_memory_order_acq_rel) & v;
}

static inline uint64_t
cnxk_tim_bkt_fast_mod(uint64_t n, uint64_t d, struct rte_reciprocal_u64 R)
{
	return (n - (d * rte_reciprocal_divide_u64(n, &R)));
}

static inline uint64_t
cnxk_tim_bkt_wait_hbt(struct cnxk_tim_bkt *bkt)
{
	uint64_t hbt_state;

#ifdef RTE_ARCH_ARM64
	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "		ldxr %[hbt], [%[w1]]	\n"
		     "		tbz %[hbt], 33, .Ldne%=	\n"
		     "		sevl			\n"
		     ".Lrty%=:	wfe			\n"
		     "		ldxr %[hbt], [%[w1]]	\n"
		     "		tbnz %[hbt], 33, .Lrty%=\n"
		     ".Ldne%=:				\n"
		     : [hbt] "=&r"(hbt_state)
		     : [w1] "r"((&bkt->w1))
		     : "memory");
#else
	do {
		hbt_state = rte_atomic_load_explicit(&bkt->w1,
						     rte_memory_order_relaxed);
	} while (hbt_state & BIT_ULL(33));
#endif

	return hbt_state;
}

static inline void
cnxk_tim_format_event(const struct rte_event_timer *const tim, struct cnxk_tim_ent *const entry)
{
	entry->w0 = (tim->ev.event & 0xFFC000000000) >> 6 | (tim->ev.event & 0xFFFFFFFFF);
	entry->wqe = tim->ev.u64;
}

static __rte_always_inline void
cnxk_tim_get_target_bucket(struct cnxk_tim_ring *const tim_ring,
			   const uint32_t rel_bkt, struct cnxk_tim_bkt **bkt,
			   struct cnxk_tim_bkt **mirr_bkt)
{
	const uint64_t bkt_cyc =
		tim_ring->tick_fn(tim_ring->tbase) - tim_ring->ring_start_cyc;
	uint64_t bucket = rte_reciprocal_divide_u64(bkt_cyc, &tim_ring->fast_div);
	uint64_t mirr_bucket = 0;

	if ((bkt_cyc - bucket * tim_ring->tck_int) < tim_ring->tck_int / 2)
		bucket--;

	bucket += rel_bkt;
	bucket = cnxk_tim_bkt_fast_mod(bucket, tim_ring->nb_bkts,
				       tim_ring->fast_bkt);
	mirr_bucket =
		cnxk_tim_bkt_fast_mod(bucket + (tim_ring->nb_bkts >> 1),
				      tim_ring->nb_bkts, tim_ring->fast_bkt);
	*bkt = &tim_ring->bkt[bucket];
	*mirr_bkt = &tim_ring->bkt[mirr_bucket];
}

static struct cnxk_tim_ent *
cnxk_tim_clr_bkt(struct cnxk_tim_ring *const tim_ring,
		 struct cnxk_tim_bkt *const bkt)
{
#define TIM_MAX_OUTSTANDING_OBJ 64
	void *pend_chunks[TIM_MAX_OUTSTANDING_OBJ];
	struct cnxk_tim_ent *chunk;
	struct cnxk_tim_ent *pnext;
	uint8_t objs = 0;

	chunk = ((struct cnxk_tim_ent *)(uintptr_t)bkt->first_chunk);
	chunk = (struct cnxk_tim_ent *)(uintptr_t)(chunk +
						   tim_ring->nb_chunk_slots)
			->w0;
	while (chunk) {
		pnext = (struct cnxk_tim_ent *)(uintptr_t)(
			(chunk + tim_ring->nb_chunk_slots)->w0);
		if (objs == TIM_MAX_OUTSTANDING_OBJ) {
			rte_mempool_put_bulk(tim_ring->chunk_pool, pend_chunks,
					     objs);
			objs = 0;
		}
		pend_chunks[objs++] = chunk;
		chunk = pnext;
	}

	if (objs)
		rte_mempool_put_bulk(tim_ring->chunk_pool, pend_chunks, objs);

	return (struct cnxk_tim_ent *)(uintptr_t)bkt->first_chunk;
}

static struct cnxk_tim_ent *
cnxk_tim_refill_chunk(struct cnxk_tim_bkt *const bkt,
		      struct cnxk_tim_bkt *const mirr_bkt,
		      struct cnxk_tim_ring *const tim_ring)
{
	struct cnxk_tim_ent *chunk;

	if (bkt->nb_entry || !bkt->first_chunk) {
		if (unlikely(rte_mempool_get(tim_ring->chunk_pool,
					     (void **)&chunk)))
			return NULL;
		if (bkt->nb_entry) {
			*(uint64_t *)(((struct cnxk_tim_ent *)
					       mirr_bkt->current_chunk) +
				      tim_ring->nb_chunk_slots) =
				(uintptr_t)chunk;
		} else {
			bkt->first_chunk = (uintptr_t)chunk;
		}
	} else {
		chunk = cnxk_tim_clr_bkt(tim_ring, bkt);
		bkt->first_chunk = (uintptr_t)chunk;
	}
	*(uint64_t *)(chunk + tim_ring->nb_chunk_slots) = 0;

	return chunk;
}

static struct cnxk_tim_ent *
cnxk_tim_insert_chunk(struct cnxk_tim_bkt *const bkt,
		      struct cnxk_tim_bkt *const mirr_bkt,
		      struct cnxk_tim_ring *const tim_ring)
{
	struct cnxk_tim_ent *chunk;

	if (unlikely(rte_mempool_get(tim_ring->chunk_pool, (void **)&chunk)))
		return NULL;

	RTE_MEMPOOL_CHECK_COOKIES(tim_ring->chunk_pool, (void **)&chunk, 1, 0);
	*(uint64_t *)(chunk + tim_ring->nb_chunk_slots) = 0;
	if (bkt->nb_entry) {
		*(uint64_t *)(((struct cnxk_tim_ent *)(uintptr_t)
				       mirr_bkt->current_chunk) +
			      tim_ring->nb_chunk_slots) = (uintptr_t)chunk;
	} else {
		bkt->first_chunk = (uintptr_t)chunk;
	}
	return chunk;
}

static __rte_always_inline int
cnxk_tim_add_entry_sp(struct cnxk_tim_ring *const tim_ring,
		      const uint32_t rel_bkt, struct rte_event_timer *const tim,
		      const struct cnxk_tim_ent *const pent,
		      const uint8_t flags)
{
	struct cnxk_tim_ent *chunk = NULL;
	struct cnxk_tim_bkt *mirr_bkt;
	struct cnxk_tim_bkt *bkt;
	uint64_t lock_sema;
	int16_t rem;

__retry:
	cnxk_tim_get_target_bucket(tim_ring, rel_bkt, &bkt, &mirr_bkt);

	/* Get Bucket sema*/
	lock_sema = cnxk_tim_bkt_fetch_sema_lock(bkt);

	/* Bucket related checks. */
	if (unlikely(cnxk_tim_bkt_get_hbt(lock_sema))) {
		if (cnxk_tim_bkt_get_nent(lock_sema) != 0) {
			uint64_t hbt_state = cnxk_tim_bkt_wait_hbt(bkt);

			if (cnxk_tim_bkt_get_nent(lock_sema) != 0 &&
			    cnxk_tim_bkt_get_nent(hbt_state) == 0) {
				cnxk_tim_bkt_dec_lock(bkt);
				goto __retry;
			}

			if (cnxk_tim_bkt_get_nent(hbt_state) != 0)
				hbt_state |= BIT_ULL(34);

			if (!(hbt_state & BIT_ULL(34)) ||
			    !(hbt_state & GENMASK(31, 0))) {
				cnxk_tim_bkt_dec_lock(bkt);
				goto __retry;
			}
		} else {
			cnxk_tim_bkt_wait_hbt(bkt);
		}
	}
	/* Insert the work. */
	rem = cnxk_tim_bkt_fetch_rem(lock_sema);

	if (!rem) {
		if (flags & CNXK_TIM_ENA_FB)
			chunk = cnxk_tim_refill_chunk(bkt, mirr_bkt, tim_ring);
		if (flags & CNXK_TIM_ENA_DFB)
			chunk = cnxk_tim_insert_chunk(bkt, mirr_bkt, tim_ring);

		if (unlikely(chunk == NULL)) {
			bkt->chunk_remainder = 0;
			tim->impl_opaque[0] = 0;
			tim->impl_opaque[1] = 0;
			tim->state = RTE_EVENT_TIMER_ERROR;
			cnxk_tim_bkt_dec_lock(bkt);
			return -ENOMEM;
		}
		mirr_bkt->current_chunk = (uintptr_t)chunk;
		bkt->chunk_remainder = tim_ring->nb_chunk_slots - 1;
	} else {
		chunk = (struct cnxk_tim_ent *)mirr_bkt->current_chunk;
		chunk += tim_ring->nb_chunk_slots - rem;
	}

	/* Copy work entry. */
	*chunk = *pent;

	tim->impl_opaque[0] = (uintptr_t)chunk;
	tim->impl_opaque[1] = (uintptr_t)bkt;
	tim->state = RTE_EVENT_TIMER_ARMED;
	cnxk_tim_bkt_inc_nent(bkt);
	cnxk_tim_bkt_dec_lock(bkt);

	return 0;
}

static __rte_always_inline int
cnxk_tim_add_entry_mp(struct cnxk_tim_ring *const tim_ring,
		      const uint32_t rel_bkt, struct rte_event_timer *const tim,
		      const struct cnxk_tim_ent *const pent,
		      const uint8_t flags)
{
	struct cnxk_tim_ent *chunk = NULL;
	struct cnxk_tim_bkt *mirr_bkt;
	struct cnxk_tim_bkt *bkt;
	uint64_t lock_sema;
	int64_t rem;

__retry:
	cnxk_tim_get_target_bucket(tim_ring, rel_bkt, &bkt, &mirr_bkt);
	/* Get Bucket sema*/
	lock_sema = cnxk_tim_bkt_fetch_sema_lock(bkt);

	/* Bucket related checks. */
	if (unlikely(cnxk_tim_bkt_get_hbt(lock_sema))) {
		if (cnxk_tim_bkt_get_nent(lock_sema) != 0) {
			uint64_t hbt_state = cnxk_tim_bkt_wait_hbt(bkt);

			if (cnxk_tim_bkt_get_nent(lock_sema) != 0 &&
			    cnxk_tim_bkt_get_nent(hbt_state) == 0) {
				cnxk_tim_bkt_dec_lock(bkt);
				goto __retry;
			}

			if (cnxk_tim_bkt_get_nent(hbt_state) != 0)
				hbt_state |= BIT_ULL(34);

			if (!(hbt_state & BIT_ULL(34)) ||
			    !(hbt_state & GENMASK(31, 0))) {
				cnxk_tim_bkt_dec_lock(bkt);
				goto __retry;
			}
		} else {
			cnxk_tim_bkt_wait_hbt(bkt);
		}
	}

	rem = cnxk_tim_bkt_fetch_rem(lock_sema);
	if (rem < 0) {
		cnxk_tim_bkt_dec_lock(bkt);
#ifdef RTE_ARCH_ARM64
		asm volatile(PLT_CPU_FEATURE_PREAMBLE
			     "		ldxr %[rem], [%[crem]]		\n"
			     "		tbz %[rem], 63, .Ldne%=		\n"
			     "		sevl				\n"
			     ".Lrty%=:	wfe				\n"
			     "		ldxr %[rem], [%[crem]]		\n"
			     "		tbnz %[rem], 63, .Lrty%=	\n"
			     ".Ldne%=:					\n"
			     : [rem] "=&r"(rem)
			     : [crem] "r"(&bkt->w1)
			     : "memory");
#else
		while (rte_atomic_load_explicit((int64_t __rte_atomic *)&bkt->w1,
						rte_memory_order_relaxed) < 0)
			;
#endif
		goto __retry;
	} else if (!rem) {
		/* Only one thread can be here*/
		if (flags & CNXK_TIM_ENA_FB)
			chunk = cnxk_tim_refill_chunk(bkt, mirr_bkt, tim_ring);
		if (flags & CNXK_TIM_ENA_DFB)
			chunk = cnxk_tim_insert_chunk(bkt, mirr_bkt, tim_ring);

		if (unlikely(chunk == NULL)) {
			tim->impl_opaque[0] = 0;
			tim->impl_opaque[1] = 0;
			tim->state = RTE_EVENT_TIMER_ERROR;
			cnxk_tim_bkt_set_rem(bkt, 0);
			cnxk_tim_bkt_dec_lock(bkt);
			return -ENOMEM;
		}
		if (cnxk_tim_bkt_fetch_lock(lock_sema)) {
			do {
				lock_sema = rte_atomic_load_explicit(&bkt->w1,
								     rte_memory_order_relaxed);
			} while (cnxk_tim_bkt_fetch_lock(lock_sema) - 1);
		}
		cnxk_tim_bkt_inc_nent(bkt);
		rte_atomic_thread_fence(rte_memory_order_acquire);
		mirr_bkt->current_chunk = (uintptr_t)chunk;
		rte_atomic_store_explicit(&bkt->chunk_remainder, tim_ring->nb_chunk_slots - 1,
					  rte_memory_order_release);
	} else {
		chunk = (struct cnxk_tim_ent *)mirr_bkt->current_chunk;
		chunk += tim_ring->nb_chunk_slots - rem;
		cnxk_tim_bkt_inc_nent(bkt);
	}

	*chunk = *pent;
	tim->impl_opaque[0] = (uintptr_t)chunk;
	tim->impl_opaque[1] = (uintptr_t)bkt;
	tim->state = RTE_EVENT_TIMER_ARMED;
	cnxk_tim_bkt_dec_lock(bkt);

	return 0;
}

static inline uint16_t
cnxk_tim_cpy_wrk(uint16_t index, uint16_t cpy_lmt, struct cnxk_tim_ent *chunk,
		 struct rte_event_timer **const tim,
		 const struct cnxk_tim_ent *const ents,
		 const struct cnxk_tim_bkt *const bkt)
{
	for (; index < cpy_lmt; index++) {
		*chunk = *(ents + index);
		tim[index]->impl_opaque[0] = (uintptr_t)chunk++;
		tim[index]->impl_opaque[1] = (uintptr_t)bkt;
		tim[index]->state = RTE_EVENT_TIMER_ARMED;
	}

	return index;
}

/* Burst mode functions */
static inline int
cnxk_tim_add_entry_brst(struct cnxk_tim_ring *const tim_ring,
			const uint16_t rel_bkt,
			struct rte_event_timer **const tim,
			const struct cnxk_tim_ent *ents,
			const uint16_t nb_timers, const uint8_t flags)
{
	struct cnxk_tim_ent *chunk = NULL;
	struct cnxk_tim_bkt *mirr_bkt;
	struct cnxk_tim_bkt *bkt;
	int16_t chunk_remainder;
	uint16_t index = 0;
	uint64_t lock_sema;
	int16_t rem;
	uint8_t lock_cnt;

__retry:
	cnxk_tim_get_target_bucket(tim_ring, rel_bkt, &bkt, &mirr_bkt);

	/* Only one thread beyond this. */
	lock_sema = cnxk_tim_bkt_inc_lock(bkt);

	/* Bucket related checks. */
	if (unlikely(cnxk_tim_bkt_get_hbt(lock_sema))) {
		if (cnxk_tim_bkt_get_nent(lock_sema) != 0) {
			uint64_t hbt_state = cnxk_tim_bkt_wait_hbt(bkt);

			if (cnxk_tim_bkt_get_nent(lock_sema) != 0 &&
			    cnxk_tim_bkt_get_nent(hbt_state) == 0) {
				cnxk_tim_bkt_dec_lock(bkt);
				goto __retry;
			}

			if (cnxk_tim_bkt_get_nent(hbt_state) != 0)
				hbt_state |= BIT_ULL(34);

			if (!(hbt_state & BIT_ULL(34)) ||
			    !(hbt_state & GENMASK(31, 0))) {
				cnxk_tim_bkt_dec_lock(bkt);
				goto __retry;
			}
		} else {
			cnxk_tim_bkt_wait_hbt(bkt);
		}
	}

	lock_cnt = (uint8_t)((lock_sema >> TIM_BUCKET_W1_S_LOCK) &
			     TIM_BUCKET_W1_M_LOCK);
	if (lock_cnt) {
		cnxk_tim_bkt_dec_lock(bkt);
#ifdef RTE_ARCH_ARM64
		asm volatile(PLT_CPU_FEATURE_PREAMBLE
			     "		ldxrb %w[lock_cnt], [%[lock]]	\n"
			     "		tst %w[lock_cnt], 255		\n"
			     "		beq .Ldne%=			\n"
			     "		sevl				\n"
			     ".Lrty%=:	wfe				\n"
			     "		ldxrb %w[lock_cnt], [%[lock]]	\n"
			     "		tst %w[lock_cnt], 255		\n"
			     "		bne .Lrty%=			\n"
			     ".Ldne%=:					\n"
			     : [lock_cnt] "=&r"(lock_cnt)
			     : [lock] "r"(&bkt->lock)
			     : "memory");
#else
		while (rte_atomic_load_explicit(&bkt->lock, rte_memory_order_relaxed))
			;
#endif
		goto __retry;
	}

	chunk_remainder = cnxk_tim_bkt_fetch_rem(lock_sema);
	rem = chunk_remainder - nb_timers;
	if (rem < 0) {
		if (chunk_remainder > 0) {
			chunk = ((struct cnxk_tim_ent *)
					 mirr_bkt->current_chunk) +
				tim_ring->nb_chunk_slots - chunk_remainder;

			index = cnxk_tim_cpy_wrk(index, chunk_remainder, chunk,
						 tim, ents, bkt);
			cnxk_tim_bkt_sub_rem(bkt, chunk_remainder);
			cnxk_tim_bkt_add_nent_relaxed(bkt, chunk_remainder);
		}

		if (flags & CNXK_TIM_ENA_FB)
			chunk = cnxk_tim_refill_chunk(bkt, mirr_bkt, tim_ring);
		if (flags & CNXK_TIM_ENA_DFB)
			chunk = cnxk_tim_insert_chunk(bkt, mirr_bkt, tim_ring);

		if (unlikely(chunk == NULL)) {
			cnxk_tim_bkt_dec_lock(bkt);
			rte_errno = ENOMEM;
			tim[index]->state = RTE_EVENT_TIMER_ERROR;
			return index;
		}
		*(uint64_t *)(chunk + tim_ring->nb_chunk_slots) = 0;
		mirr_bkt->current_chunk = (uintptr_t)chunk;
		index = cnxk_tim_cpy_wrk(index, nb_timers, chunk, tim, ents,
					 bkt) -
			index;

		cnxk_tim_bkt_set_rem(bkt, tim_ring->nb_chunk_slots - index);
		cnxk_tim_bkt_add_nent(bkt, index);
	} else {
		chunk = (struct cnxk_tim_ent *)mirr_bkt->current_chunk;
		chunk += (tim_ring->nb_chunk_slots - chunk_remainder);

		cnxk_tim_cpy_wrk(index, nb_timers, chunk, tim, ents, bkt);
		cnxk_tim_bkt_sub_rem(bkt, nb_timers);
		cnxk_tim_bkt_add_nent(bkt, nb_timers);
	}

	cnxk_tim_bkt_dec_lock(bkt);

	return nb_timers;
}

static int
cnxk_tim_add_entry_hwwqe(struct cnxk_tim_ring *const tim_ring, struct rte_event_timer *const tim)
{
	uint64_t __rte_atomic *status;
	uint64_t wdata, pa;
	uintptr_t lmt_addr;
	uint16_t lmt_id;
	uint64_t *lmt;
	uint64_t rsp;
	int rc = 0;

	status = (uint64_t __rte_atomic *)&tim->impl_opaque[0];
	status[0] = 0;
	status[1] = 0;

	lmt_addr = tim_ring->lmt_base;
	ROC_LMT_BASE_ID_GET(lmt_addr, lmt_id);
	lmt = (uint64_t *)lmt_addr;

	lmt[0] = tim->timeout_ticks * tim_ring->tck_int;
	lmt[1] = 0x1;
	lmt[2] = (tim->ev.event & 0xFFC000000000) >> 6 | (tim->ev.event & 0xFFFFFFFFF);
	lmt[3] = (uint64_t)tim;

	/* One LMT line is used, CNTM1 is 0 and SIZE_VEC is not included. */
	wdata = lmt_id;
	/* SIZEM1 is 0 */
	pa = (tim_ring->tbase & ~0xFF) + TIM_LF_SCHED_TIMER0;
	pa |= (1UL << 4);
	roc_lmt_submit_steorl(wdata, pa);

	do {
		rsp = rte_atomic_load_explicit(status, rte_memory_order_relaxed);
		rsp &= 0xF0UL;
	} while (!rsp);

	rsp >>= 4;
	switch (rsp) {
	case 0x3:
		tim->state = RTE_EVENT_TIMER_ERROR_TOOEARLY;
		rc = !rc;
		break;
	case 0x4:
		tim->state = RTE_EVENT_TIMER_ERROR_TOOLATE;
		rc = !rc;
		break;
	case 0x1:
		tim->state = RTE_EVENT_TIMER_ARMED;
		break;
	default:
		tim->state = RTE_EVENT_TIMER_ERROR;
		rc = !rc;
		break;
	}

	return rc;
}

static int
cnxk_tim_add_entry_tmo_hwwqe(struct cnxk_tim_ring *const tim_ring,
			     struct rte_event_timer **const tim, uint64_t intvl, uint16_t nb_timers)
{
	uint64_t __rte_atomic *status;
	uint16_t cnt, i, j, done;
	uint64_t wdata, pa;
	uintptr_t lmt_addr;
	uint16_t lmt_id;
	uint64_t *lmt;
	uint64_t rsp;

	/* We have 32 LMTLINES per core, but use only 1 line as we need to check status */
	lmt_addr = tim_ring->lmt_base;
	ROC_LMT_BASE_ID_GET(lmt_addr, lmt_id);

	done = 0;
	lmt = (uint64_t *)lmt_addr;
	/* We can do up to 7 timers per LMTLINE */
	cnt = nb_timers / CNXK_TIM_ENT_PER_LMT;

	lmt[0] = intvl;
	lmt[1] = 0x1; /* Always relative */
	/* One LMT line is used, CNTM1 is 0 and SIZE_VEC is not included. */
	wdata = lmt_id;
	/* SIZEM1 is 0 */
	pa = (tim_ring->tbase & ~0xFF) + TIM_LF_SCHED_TIMER0;
	pa |= (uint64_t)(CNXK_TIM_ENT_PER_LMT << 4);
	for (i = 0; i < cnt; i++) {
		status = (uint64_t __rte_atomic *)&tim[i * CNXK_TIM_ENT_PER_LMT]->impl_opaque[0];

		for (j = 0; j < CNXK_TIM_ENT_PER_LMT; j++) {
			cnxk_tim_format_event(tim[(i * CNXK_TIM_ENT_PER_LMT) + j],
					      (struct cnxk_tim_ent *)&lmt[(j << 1) + 2]);
			tim[(i * CNXK_TIM_ENT_PER_LMT) + j]->impl_opaque[0] = 0;
			tim[(i * CNXK_TIM_ENT_PER_LMT) + j]->impl_opaque[1] = 0;
			tim[(i * CNXK_TIM_ENT_PER_LMT) + j]->state = RTE_EVENT_TIMER_ARMED;
		}

		roc_lmt_submit_steorl(wdata, pa);
		do {
			rsp = rte_atomic_load_explicit(status, rte_memory_order_relaxed);
			rsp &= 0xFUL;
		} while (!rsp);

		done += CNXK_TIM_ENT_PER_LMT;
		rsp &= 0xF;
		if (rsp != 0x1) {
			switch (rsp) {
			case 0x3:
				for (j = 0; j < CNXK_TIM_ENT_PER_LMT; j++)
					tim[(i * CNXK_TIM_ENT_PER_LMT) + j]->state =
						RTE_EVENT_TIMER_ERROR_TOOEARLY;
				done -= CNXK_TIM_ENT_PER_LMT;
				break;
			case 0x4:
				for (j = 0; j < CNXK_TIM_ENT_PER_LMT; j++)
					tim[(i * CNXK_TIM_ENT_PER_LMT) + j]->state =
						RTE_EVENT_TIMER_ERROR_TOOLATE;
				done -= CNXK_TIM_ENT_PER_LMT;
				break;
			case 0x2:
			default:
				for (j = 0; j < CNXK_TIM_ENT_PER_LMT; j++) {
					if ((rte_atomic_load_explicit(
						     (uint64_t __rte_atomic
							      *)&tim[(i * CNXK_TIM_ENT_PER_LMT) + j]
							     ->impl_opaque[0],
						     rte_memory_order_relaxed) &
					     0xF0) != 0x10) {
						tim[(i * CNXK_TIM_ENT_PER_LMT) + j]->state =
							RTE_EVENT_TIMER_ERROR;
						done--;
					}
				}
				break;
			}
			goto done;
		}
	}

	/* SIZEM1 is 0 */
	pa = (tim_ring->tbase & ~0xFF) + TIM_LF_SCHED_TIMER0;
	pa |= (uint64_t)((nb_timers - cnt) << 4);
	if (nb_timers - cnt) {
		status = (uint64_t __rte_atomic *)&tim[cnt]->impl_opaque[0];

		for (i = 0; i < nb_timers - cnt; i++) {
			cnxk_tim_format_event(tim[cnt + i],
					      (struct cnxk_tim_ent *)&lmt[(i << 1) + 2]);
			tim[cnt + i]->impl_opaque[0] = 0;
			tim[cnt + i]->impl_opaque[1] = 0;
			tim[cnt + i]->state = RTE_EVENT_TIMER_ARMED;
		}

		roc_lmt_submit_steorl(wdata, pa);
		do {
			rsp = rte_atomic_load_explicit(status, rte_memory_order_relaxed);
			rsp &= 0xFUL;
		} while (!rsp);

		done += (nb_timers - cnt);
		rsp &= 0xF;
		if (rsp != 0x1) {
			switch (rsp) {
			case 0x3:
				for (j = 0; j < nb_timers - cnt; j++)
					tim[cnt + j]->state = RTE_EVENT_TIMER_ERROR_TOOEARLY;
				done -= (nb_timers - cnt);
				break;
			case 0x4:
				for (j = 0; j < nb_timers - cnt; j++)
					tim[cnt + j]->state = RTE_EVENT_TIMER_ERROR_TOOLATE;
				done -= (nb_timers - cnt);
				break;
			case 0x2:
			default:
				for (j = 0; j < nb_timers - cnt; j++) {
					if ((rte_atomic_load_explicit(
						     (uint64_t __rte_atomic *)&tim[cnt + j]
							     ->impl_opaque[0],
						     rte_memory_order_relaxed) &
					     0xF0) != 0x10) {
						tim[cnt + j]->state = RTE_EVENT_TIMER_ERROR;
						done--;
					}
				}
				break;
			}
		}
	}

done:
	return done;
}

static int
cnxk_tim_rm_entry(struct rte_event_timer *tim)
{
	struct cnxk_tim_ent *entry;
	struct cnxk_tim_bkt *bkt;
	uint64_t lock_sema;

	if (tim->impl_opaque[1] == 0 || tim->impl_opaque[0] == 0)
		return -ENOENT;

	entry = (struct cnxk_tim_ent *)(uintptr_t)tim->impl_opaque[0];
	if (entry->wqe != tim->ev.u64) {
		tim->impl_opaque[0] = 0;
		tim->impl_opaque[1] = 0;
		return -ENOENT;
	}

	bkt = (struct cnxk_tim_bkt *)(uintptr_t)tim->impl_opaque[1];
	lock_sema = cnxk_tim_bkt_inc_lock(bkt);
	if (cnxk_tim_bkt_get_hbt(lock_sema) ||
	    !cnxk_tim_bkt_get_nent(lock_sema)) {
		tim->impl_opaque[0] = 0;
		tim->impl_opaque[1] = 0;
		cnxk_tim_bkt_dec_lock(bkt);
		return -ENOENT;
	}

	entry->w0 = 0;
	entry->wqe = 0;
	tim->state = RTE_EVENT_TIMER_CANCELED;
	tim->impl_opaque[0] = 0;
	tim->impl_opaque[1] = 0;
	cnxk_tim_bkt_dec_lock(bkt);

	return 0;
}

#endif /* __CNXK_TIM_WORKER_H__ */
