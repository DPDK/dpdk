/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_TIM_WORKER_H__
#define __OTX2_TIM_WORKER_H__

#include "otx2_tim_evdev.h"

static inline uint8_t
tim_bkt_fetch_lock(uint64_t w1)
{
	return (w1 >> TIM_BUCKET_W1_S_LOCK) &
		TIM_BUCKET_W1_M_LOCK;
}

static inline int16_t
tim_bkt_fetch_rem(uint64_t w1)
{
	return (w1 >> TIM_BUCKET_W1_S_CHUNK_REMAINDER) &
		TIM_BUCKET_W1_M_CHUNK_REMAINDER;
}

static inline int16_t
tim_bkt_get_rem(struct otx2_tim_bkt *bktp)
{
	return __atomic_load_n(&bktp->chunk_remainder, __ATOMIC_ACQUIRE);
}

static inline void
tim_bkt_set_rem(struct otx2_tim_bkt *bktp, uint16_t v)
{
	__atomic_store_n(&bktp->chunk_remainder, v, __ATOMIC_RELAXED);
}

static inline void
tim_bkt_sub_rem(struct otx2_tim_bkt *bktp, uint16_t v)
{
	__atomic_fetch_sub(&bktp->chunk_remainder, v, __ATOMIC_RELAXED);
}

static inline uint8_t
tim_bkt_get_hbt(uint64_t w1)
{
	return (w1 >> TIM_BUCKET_W1_S_HBT) & TIM_BUCKET_W1_M_HBT;
}

static inline uint8_t
tim_bkt_get_bsk(uint64_t w1)
{
	return (w1 >> TIM_BUCKET_W1_S_BSK) & TIM_BUCKET_W1_M_BSK;
}

static inline uint64_t
tim_bkt_clr_bsk(struct otx2_tim_bkt *bktp)
{
	/* Clear everything except lock. */
	const uint64_t v = TIM_BUCKET_W1_M_LOCK << TIM_BUCKET_W1_S_LOCK;

	return __atomic_fetch_and(&bktp->w1, v, __ATOMIC_ACQ_REL);
}

static inline uint64_t
tim_bkt_fetch_sema_lock(struct otx2_tim_bkt *bktp)
{
	return __atomic_fetch_add(&bktp->w1, TIM_BUCKET_SEMA_WLOCK,
			__ATOMIC_ACQUIRE);
}

static inline uint64_t
tim_bkt_fetch_sema(struct otx2_tim_bkt *bktp)
{
	return __atomic_fetch_add(&bktp->w1, TIM_BUCKET_SEMA, __ATOMIC_RELAXED);
}

static inline uint64_t
tim_bkt_inc_lock(struct otx2_tim_bkt *bktp)
{
	const uint64_t v = 1ull << TIM_BUCKET_W1_S_LOCK;

	return __atomic_fetch_add(&bktp->w1, v, __ATOMIC_ACQUIRE);
}

static inline void
tim_bkt_dec_lock(struct otx2_tim_bkt *bktp)
{
	__atomic_fetch_sub(&bktp->lock, 1, __ATOMIC_RELEASE);
}

static inline void
tim_bkt_dec_lock_relaxed(struct otx2_tim_bkt *bktp)
{
	__atomic_fetch_sub(&bktp->lock, 1, __ATOMIC_RELAXED);
}

static inline uint32_t
tim_bkt_get_nent(uint64_t w1)
{
	return (w1 >> TIM_BUCKET_W1_S_NUM_ENTRIES) &
		TIM_BUCKET_W1_M_NUM_ENTRIES;
}

static inline void
tim_bkt_inc_nent(struct otx2_tim_bkt *bktp)
{
	__atomic_add_fetch(&bktp->nb_entry, 1, __ATOMIC_RELAXED);
}

static inline void
tim_bkt_add_nent(struct otx2_tim_bkt *bktp, uint32_t v)
{
	__atomic_add_fetch(&bktp->nb_entry, v, __ATOMIC_RELAXED);
}

static inline uint64_t
tim_bkt_clr_nent(struct otx2_tim_bkt *bktp)
{
	const uint64_t v = ~(TIM_BUCKET_W1_M_NUM_ENTRIES <<
			TIM_BUCKET_W1_S_NUM_ENTRIES);

	return __atomic_and_fetch(&bktp->w1, v, __ATOMIC_ACQ_REL);
}

static inline uint64_t
tim_bkt_fast_mod(uint64_t n, uint64_t d, struct rte_reciprocal_u64 R)
{
	return (n - (d * rte_reciprocal_divide_u64(n, &R)));
}

static __rte_always_inline void
tim_get_target_bucket(struct otx2_tim_ring *const tim_ring,
		      const uint32_t rel_bkt, struct otx2_tim_bkt **bkt,
		      struct otx2_tim_bkt **mirr_bkt)
{
	const uint64_t bkt_cyc = tim_cntvct() - tim_ring->ring_start_cyc;
	uint64_t bucket =
		rte_reciprocal_divide_u64(bkt_cyc, &tim_ring->fast_div) +
		rel_bkt;
	uint64_t mirr_bucket = 0;

	bucket =
		tim_bkt_fast_mod(bucket, tim_ring->nb_bkts, tim_ring->fast_bkt);
	mirr_bucket = tim_bkt_fast_mod(bucket + (tim_ring->nb_bkts >> 1),
				       tim_ring->nb_bkts, tim_ring->fast_bkt);
	*bkt = &tim_ring->bkt[bucket];
	*mirr_bkt = &tim_ring->bkt[mirr_bucket];
}

static struct otx2_tim_ent *
tim_clr_bkt(struct otx2_tim_ring * const tim_ring,
	    struct otx2_tim_bkt * const bkt)
{
#define TIM_MAX_OUTSTANDING_OBJ		64
	void *pend_chunks[TIM_MAX_OUTSTANDING_OBJ];
	struct otx2_tim_ent *chunk;
	struct otx2_tim_ent *pnext;
	uint8_t objs = 0;


	chunk = ((struct otx2_tim_ent *)(uintptr_t)bkt->first_chunk);
	chunk = (struct otx2_tim_ent *)(uintptr_t)(chunk +
			tim_ring->nb_chunk_slots)->w0;
	while (chunk) {
		pnext = (struct otx2_tim_ent *)(uintptr_t)
			((chunk + tim_ring->nb_chunk_slots)->w0);
		if (objs == TIM_MAX_OUTSTANDING_OBJ) {
			rte_mempool_put_bulk(tim_ring->chunk_pool, pend_chunks,
					     objs);
			objs = 0;
		}
		pend_chunks[objs++] = chunk;
		chunk = pnext;
	}

	if (objs)
		rte_mempool_put_bulk(tim_ring->chunk_pool, pend_chunks,
				objs);

	return (struct otx2_tim_ent *)(uintptr_t)bkt->first_chunk;
}

static struct otx2_tim_ent *
tim_refill_chunk(struct otx2_tim_bkt * const bkt,
		 struct otx2_tim_bkt * const mirr_bkt,
		 struct otx2_tim_ring * const tim_ring)
{
	struct otx2_tim_ent *chunk;

	if (bkt->nb_entry || !bkt->first_chunk) {
		if (unlikely(rte_mempool_get(tim_ring->chunk_pool,
					     (void **)&chunk)))
			return NULL;
		if (bkt->nb_entry) {
			*(uint64_t *)(((struct otx2_tim_ent *)
						mirr_bkt->current_chunk) +
					tim_ring->nb_chunk_slots) =
				(uintptr_t)chunk;
		} else {
			bkt->first_chunk = (uintptr_t)chunk;
		}
	} else {
		chunk = tim_clr_bkt(tim_ring, bkt);
		bkt->first_chunk = (uintptr_t)chunk;
	}
	*(uint64_t *)(chunk + tim_ring->nb_chunk_slots) = 0;

	return chunk;
}

static struct otx2_tim_ent *
tim_insert_chunk(struct otx2_tim_bkt * const bkt,
		 struct otx2_tim_bkt * const mirr_bkt,
		 struct otx2_tim_ring * const tim_ring)
{
	struct otx2_tim_ent *chunk;

	if (unlikely(rte_mempool_get(tim_ring->chunk_pool, (void **)&chunk)))
		return NULL;

	*(uint64_t *)(chunk + tim_ring->nb_chunk_slots) = 0;
	if (bkt->nb_entry) {
		*(uint64_t *)(((struct otx2_tim_ent *)(uintptr_t)
					mirr_bkt->current_chunk) +
				tim_ring->nb_chunk_slots) = (uintptr_t)chunk;
	} else {
		bkt->first_chunk = (uintptr_t)chunk;
	}
	return chunk;
}

static __rte_always_inline int
tim_add_entry_sp(struct otx2_tim_ring * const tim_ring,
		 const uint32_t rel_bkt,
		 struct rte_event_timer * const tim,
		 const struct otx2_tim_ent * const pent,
		 const uint8_t flags)
{
	struct otx2_tim_bkt *mirr_bkt;
	struct otx2_tim_ent *chunk;
	struct otx2_tim_bkt *bkt;
	uint64_t lock_sema;
	int16_t rem;

__retry:
	tim_get_target_bucket(tim_ring, rel_bkt, &bkt, &mirr_bkt);

	/* Get Bucket sema*/
	lock_sema = tim_bkt_fetch_sema_lock(bkt);

	/* Bucket related checks. */
	if (unlikely(tim_bkt_get_hbt(lock_sema))) {
		if (tim_bkt_get_nent(lock_sema) != 0) {
			uint64_t hbt_state;
#ifdef RTE_ARCH_ARM64
			asm volatile("		ldxr %[hbt], [%[w1]]	\n"
				     "		tbz %[hbt], 33, dne%=	\n"
				     "		sevl			\n"
				     "rty%=:	wfe			\n"
				     "		ldxr %[hbt], [%[w1]]	\n"
				     "		tbnz %[hbt], 33, rty%=	\n"
				     "dne%=:				\n"
				     : [hbt] "=&r"(hbt_state)
				     : [w1] "r"((&bkt->w1))
				     : "memory");
#else
			do {
				hbt_state = __atomic_load_n(&bkt->w1,
							    __ATOMIC_RELAXED);
			} while (hbt_state & BIT_ULL(33));
#endif

			if (!(hbt_state & BIT_ULL(34))) {
				tim_bkt_dec_lock(bkt);
				goto __retry;
			}
		}
	}
	/* Insert the work. */
	rem = tim_bkt_fetch_rem(lock_sema);

	if (!rem) {
		if (flags & OTX2_TIM_ENA_FB)
			chunk = tim_refill_chunk(bkt, mirr_bkt, tim_ring);
		if (flags & OTX2_TIM_ENA_DFB)
			chunk = tim_insert_chunk(bkt, mirr_bkt, tim_ring);

		if (unlikely(chunk == NULL)) {
			bkt->chunk_remainder = 0;
			tim->impl_opaque[0] = 0;
			tim->impl_opaque[1] = 0;
			tim->state = RTE_EVENT_TIMER_ERROR;
			tim_bkt_dec_lock(bkt);
			return -ENOMEM;
		}
		mirr_bkt->current_chunk = (uintptr_t)chunk;
		bkt->chunk_remainder = tim_ring->nb_chunk_slots - 1;
	} else {
		chunk = (struct otx2_tim_ent *)mirr_bkt->current_chunk;
		chunk += tim_ring->nb_chunk_slots - rem;
	}

	/* Copy work entry. */
	*chunk = *pent;

	tim->impl_opaque[0] = (uintptr_t)chunk;
	tim->impl_opaque[1] = (uintptr_t)bkt;
	__atomic_store_n(&tim->state, RTE_EVENT_TIMER_ARMED, __ATOMIC_RELEASE);
	tim_bkt_inc_nent(bkt);
	tim_bkt_dec_lock_relaxed(bkt);

	return 0;
}

static __rte_always_inline int
tim_add_entry_mp(struct otx2_tim_ring * const tim_ring,
		 const uint32_t rel_bkt,
		 struct rte_event_timer * const tim,
		 const struct otx2_tim_ent * const pent,
		 const uint8_t flags)
{
	struct otx2_tim_bkt *mirr_bkt;
	struct otx2_tim_ent *chunk;
	struct otx2_tim_bkt *bkt;
	uint64_t lock_sema;
	int16_t rem;

__retry:
	tim_get_target_bucket(tim_ring, rel_bkt, &bkt, &mirr_bkt);
	/* Get Bucket sema*/
	lock_sema = tim_bkt_fetch_sema_lock(bkt);

	/* Bucket related checks. */
	if (unlikely(tim_bkt_get_hbt(lock_sema))) {
		if (tim_bkt_get_nent(lock_sema) != 0) {
			uint64_t hbt_state;
#ifdef RTE_ARCH_ARM64
			asm volatile("		ldxr %[hbt], [%[w1]]	\n"
				     "		tbz %[hbt], 33, dne%=	\n"
				     "		sevl			\n"
				     "rty%=:	wfe			\n"
				     "		ldxr %[hbt], [%[w1]]	\n"
				     "		tbnz %[hbt], 33, rty%=	\n"
				     "dne%=:				\n"
				     : [hbt] "=&r"(hbt_state)
				     : [w1] "r"((&bkt->w1))
				     : "memory");
#else
			do {
				hbt_state = __atomic_load_n(&bkt->w1,
							    __ATOMIC_RELAXED);
			} while (hbt_state & BIT_ULL(33));
#endif

			if (!(hbt_state & BIT_ULL(34))) {
				tim_bkt_dec_lock(bkt);
				goto __retry;
			}
		}
	}

	rem = tim_bkt_fetch_rem(lock_sema);
	if (rem < 0) {
		tim_bkt_dec_lock(bkt);
#ifdef RTE_ARCH_ARM64
		uint64_t w1;
		asm volatile("		ldxr %[w1], [%[crem]]	\n"
			     "		tbz %[w1], 63, dne%=		\n"
			     "		sevl				\n"
			     "rty%=:	wfe				\n"
			     "		ldxr %[w1], [%[crem]]	\n"
			     "		tbnz %[w1], 63, rty%=		\n"
			     "dne%=:					\n"
			     : [w1] "=&r"(w1)
			     : [crem] "r"(&bkt->w1)
			     : "memory");
#else
		while (__atomic_load_n((int64_t *)&bkt->w1, __ATOMIC_RELAXED) <
		       0)
			;
#endif
		goto __retry;
	} else if (!rem) {
		/* Only one thread can be here*/
		if (flags & OTX2_TIM_ENA_FB)
			chunk = tim_refill_chunk(bkt, mirr_bkt, tim_ring);
		if (flags & OTX2_TIM_ENA_DFB)
			chunk = tim_insert_chunk(bkt, mirr_bkt, tim_ring);

		if (unlikely(chunk == NULL)) {
			tim->impl_opaque[0] = 0;
			tim->impl_opaque[1] = 0;
			tim->state = RTE_EVENT_TIMER_ERROR;
			tim_bkt_set_rem(bkt, 0);
			tim_bkt_dec_lock(bkt);
			return -ENOMEM;
		}
		*chunk = *pent;
		if (tim_bkt_fetch_lock(lock_sema)) {
			do {
				lock_sema = __atomic_load_n(&bkt->w1,
							    __ATOMIC_RELAXED);
			} while (tim_bkt_fetch_lock(lock_sema) - 1);
			rte_atomic_thread_fence(__ATOMIC_ACQUIRE);
		}
		mirr_bkt->current_chunk = (uintptr_t)chunk;
		__atomic_store_n(&bkt->chunk_remainder,
				tim_ring->nb_chunk_slots - 1, __ATOMIC_RELEASE);
	} else {
		chunk = (struct otx2_tim_ent *)mirr_bkt->current_chunk;
		chunk += tim_ring->nb_chunk_slots - rem;
		*chunk = *pent;
	}

	tim->impl_opaque[0] = (uintptr_t)chunk;
	tim->impl_opaque[1] = (uintptr_t)bkt;
	__atomic_store_n(&tim->state, RTE_EVENT_TIMER_ARMED, __ATOMIC_RELEASE);
	tim_bkt_inc_nent(bkt);
	tim_bkt_dec_lock_relaxed(bkt);

	return 0;
}

static inline uint16_t
tim_cpy_wrk(uint16_t index, uint16_t cpy_lmt,
	    struct otx2_tim_ent *chunk,
	    struct rte_event_timer ** const tim,
	    const struct otx2_tim_ent * const ents,
	    const struct otx2_tim_bkt * const bkt)
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
tim_add_entry_brst(struct otx2_tim_ring * const tim_ring,
		   const uint16_t rel_bkt,
		   struct rte_event_timer ** const tim,
		   const struct otx2_tim_ent *ents,
		   const uint16_t nb_timers, const uint8_t flags)
{
	struct otx2_tim_ent *chunk = NULL;
	struct otx2_tim_bkt *mirr_bkt;
	struct otx2_tim_bkt *bkt;
	uint16_t chunk_remainder;
	uint16_t index = 0;
	uint64_t lock_sema;
	int16_t rem, crem;
	uint8_t lock_cnt;

__retry:
	tim_get_target_bucket(tim_ring, rel_bkt, &bkt, &mirr_bkt);

	/* Only one thread beyond this. */
	lock_sema = tim_bkt_inc_lock(bkt);
	lock_cnt = (uint8_t)
		((lock_sema >> TIM_BUCKET_W1_S_LOCK) & TIM_BUCKET_W1_M_LOCK);

	if (lock_cnt) {
		tim_bkt_dec_lock(bkt);
#ifdef RTE_ARCH_ARM64
		asm volatile("		ldxrb %w[lock_cnt], [%[lock]]	\n"
			     "		tst %w[lock_cnt], 255		\n"
			     "		beq dne%=			\n"
			     "		sevl				\n"
			     "rty%=:	wfe				\n"
			     "		ldxrb %w[lock_cnt], [%[lock]]	\n"
			     "		tst %w[lock_cnt], 255		\n"
			     "		bne rty%=			\n"
			     "dne%=:					\n"
			     : [lock_cnt] "=&r"(lock_cnt)
			     : [lock] "r"(&bkt->lock)
			     : "memory");
#else
		while (__atomic_load_n(&bkt->lock, __ATOMIC_RELAXED))
			;
#endif
		goto __retry;
	}

	/* Bucket related checks. */
	if (unlikely(tim_bkt_get_hbt(lock_sema))) {
		if (tim_bkt_get_nent(lock_sema) != 0) {
			uint64_t hbt_state;
#ifdef RTE_ARCH_ARM64
			asm volatile("		ldxr %[hbt], [%[w1]]	\n"
				     "		tbz %[hbt], 33, dne%=	\n"
				     "		sevl			\n"
				     "rty%=:	wfe			\n"
				     "		ldxr %[hbt], [%[w1]]	\n"
				     "		tbnz %[hbt], 33, rty%=	\n"
				     "dne%=:				\n"
				     : [hbt] "=&r"(hbt_state)
				     : [w1] "r"((&bkt->w1))
				     : "memory");
#else
			do {
				hbt_state = __atomic_load_n(&bkt->w1,
							    __ATOMIC_RELAXED);
			} while (hbt_state & BIT_ULL(33));
#endif

			if (!(hbt_state & BIT_ULL(34))) {
				tim_bkt_dec_lock(bkt);
				goto __retry;
			}
		}
	}

	chunk_remainder = tim_bkt_fetch_rem(lock_sema);
	rem = chunk_remainder - nb_timers;
	if (rem < 0) {
		crem = tim_ring->nb_chunk_slots - chunk_remainder;
		if (chunk_remainder && crem) {
			chunk = ((struct otx2_tim_ent *)
					mirr_bkt->current_chunk) + crem;

			index = tim_cpy_wrk(index, chunk_remainder, chunk, tim,
					    ents, bkt);
			tim_bkt_sub_rem(bkt, chunk_remainder);
			tim_bkt_add_nent(bkt, chunk_remainder);
		}

		if (flags & OTX2_TIM_ENA_FB)
			chunk = tim_refill_chunk(bkt, mirr_bkt, tim_ring);
		if (flags & OTX2_TIM_ENA_DFB)
			chunk = tim_insert_chunk(bkt, mirr_bkt, tim_ring);

		if (unlikely(chunk == NULL)) {
			tim_bkt_dec_lock(bkt);
			rte_errno = ENOMEM;
			tim[index]->state = RTE_EVENT_TIMER_ERROR;
			return crem;
		}
		*(uint64_t *)(chunk + tim_ring->nb_chunk_slots) = 0;
		mirr_bkt->current_chunk = (uintptr_t)chunk;
		tim_cpy_wrk(index, nb_timers, chunk, tim, ents, bkt);

		rem = nb_timers - chunk_remainder;
		tim_bkt_set_rem(bkt, tim_ring->nb_chunk_slots - rem);
		tim_bkt_add_nent(bkt, rem);
	} else {
		chunk = (struct otx2_tim_ent *)mirr_bkt->current_chunk;
		chunk += (tim_ring->nb_chunk_slots - chunk_remainder);

		tim_cpy_wrk(index, nb_timers, chunk, tim, ents, bkt);
		tim_bkt_sub_rem(bkt, nb_timers);
		tim_bkt_add_nent(bkt, nb_timers);
	}

	tim_bkt_dec_lock(bkt);

	return nb_timers;
}

static int
tim_rm_entry(struct rte_event_timer *tim)
{
	struct otx2_tim_ent *entry;
	struct otx2_tim_bkt *bkt;
	uint64_t lock_sema;

	if (tim->impl_opaque[1] == 0 || tim->impl_opaque[0] == 0)
		return -ENOENT;

	entry = (struct otx2_tim_ent *)(uintptr_t)tim->impl_opaque[0];
	if (entry->wqe != tim->ev.u64) {
		tim->impl_opaque[0] = 0;
		tim->impl_opaque[1] = 0;
		return -ENOENT;
	}

	bkt = (struct otx2_tim_bkt *)(uintptr_t)tim->impl_opaque[1];
	lock_sema = tim_bkt_inc_lock(bkt);
	if (tim_bkt_get_hbt(lock_sema) || !tim_bkt_get_nent(lock_sema)) {
		tim->impl_opaque[0] = 0;
		tim->impl_opaque[1] = 0;
		tim_bkt_dec_lock(bkt);
		return -ENOENT;
	}

	entry->w0 = 0;
	entry->wqe = 0;
	tim->state = RTE_EVENT_TIMER_CANCELED;
	tim->impl_opaque[0] = 0;
	tim->impl_opaque[1] = 0;
	tim_bkt_dec_lock(bkt);

	return 0;
}

#endif /* __OTX2_TIM_WORKER_H__ */
