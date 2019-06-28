/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_TIM_WORKER_H__
#define __OTX2_TIM_WORKER_H__

#include "otx2_tim_evdev.h"

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
	__atomic_add_fetch(&bktp->lock, 0xff, __ATOMIC_RELEASE);
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

static __rte_always_inline struct otx2_tim_bkt *
tim_get_target_bucket(struct otx2_tim_ring * const tim_ring,
		      const uint32_t rel_bkt, const uint8_t flag)
{
	const uint64_t bkt_cyc = rte_rdtsc() - tim_ring->ring_start_cyc;
	uint32_t bucket = rte_reciprocal_divide_u64(bkt_cyc,
			&tim_ring->fast_div) + rel_bkt;

	if (flag & OTX2_TIM_BKT_MOD)
		bucket = bucket % tim_ring->nb_bkts;
	if (flag & OTX2_TIM_BKT_AND)
		bucket = bucket & (tim_ring->nb_bkts - 1);

	return &tim_ring->bkt[bucket];
}

static struct otx2_tim_ent *
tim_clr_bkt(struct otx2_tim_ring * const tim_ring,
	    struct otx2_tim_bkt * const bkt)
{
	struct otx2_tim_ent *chunk;
	struct otx2_tim_ent *pnext;

	chunk = ((struct otx2_tim_ent *)(uintptr_t)bkt->first_chunk);
	chunk = (struct otx2_tim_ent *)(uintptr_t)(chunk +
			tim_ring->nb_chunk_slots)->w0;
	while (chunk) {
		pnext = (struct otx2_tim_ent *)(uintptr_t)
			((chunk + tim_ring->nb_chunk_slots)->w0);
		rte_mempool_put(tim_ring->chunk_pool, chunk);
		chunk = pnext;
	}

	return (struct otx2_tim_ent *)(uintptr_t)bkt->first_chunk;
}

static struct otx2_tim_ent *
tim_refill_chunk(struct otx2_tim_bkt * const bkt,
		 struct otx2_tim_ring * const tim_ring)
{
	struct otx2_tim_ent *chunk;

	if (bkt->nb_entry || !bkt->first_chunk) {
		if (unlikely(rte_mempool_get(tim_ring->chunk_pool,
					     (void **)&chunk)))
			return NULL;
		if (bkt->nb_entry) {
			*(uint64_t *)(((struct otx2_tim_ent *)(uintptr_t)
						bkt->current_chunk) +
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
		 struct otx2_tim_ring * const tim_ring)
{
	struct otx2_tim_ent *chunk;

	if (unlikely(rte_mempool_get(tim_ring->chunk_pool, (void **)&chunk)))
		return NULL;

	*(uint64_t *)(chunk + tim_ring->nb_chunk_slots) = 0;
	if (bkt->nb_entry) {
		*(uint64_t *)(((struct otx2_tim_ent *)(uintptr_t)
					bkt->current_chunk) +
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
	struct otx2_tim_ent *chunk;
	struct otx2_tim_bkt *bkt;
	uint64_t lock_sema;
	int16_t rem;

	bkt = tim_get_target_bucket(tim_ring, rel_bkt, flags);

__retry:
	/* Get Bucket sema*/
	lock_sema = tim_bkt_fetch_sema(bkt);

	/* Bucket related checks. */
	if (unlikely(tim_bkt_get_hbt(lock_sema)))
		goto __retry;

	/* Insert the work. */
	rem = tim_bkt_fetch_rem(lock_sema);

	if (!rem) {
		if (flags & OTX2_TIM_ENA_FB)
			chunk = tim_refill_chunk(bkt, tim_ring);
		if (flags & OTX2_TIM_ENA_DFB)
			chunk = tim_insert_chunk(bkt, tim_ring);

		if (unlikely(chunk == NULL)) {
			tim_bkt_set_rem(bkt, 0);
			tim->impl_opaque[0] = 0;
			tim->impl_opaque[1] = 0;
			tim->state = RTE_EVENT_TIMER_ERROR;
			return -ENOMEM;
		}
		bkt->current_chunk = (uintptr_t)chunk;
		tim_bkt_set_rem(bkt, tim_ring->nb_chunk_slots - 1);
	} else {
		chunk = (struct otx2_tim_ent *)(uintptr_t)bkt->current_chunk;
		chunk += tim_ring->nb_chunk_slots - rem;
	}

	/* Copy work entry. */
	*chunk = *pent;

	tim_bkt_inc_nent(bkt);

	tim->impl_opaque[0] = (uintptr_t)chunk;
	tim->impl_opaque[1] = (uintptr_t)bkt;
	tim->state = RTE_EVENT_TIMER_ARMED;

	return 0;
}

static __rte_always_inline int
tim_add_entry_mp(struct otx2_tim_ring * const tim_ring,
		 const uint32_t rel_bkt,
		 struct rte_event_timer * const tim,
		 const struct otx2_tim_ent * const pent,
		 const uint8_t flags)
{
	struct otx2_tim_ent *chunk;
	struct otx2_tim_bkt *bkt;
	uint64_t lock_sema;
	int16_t rem;

__retry:
	bkt = tim_get_target_bucket(tim_ring, rel_bkt, flags);

	/* Get Bucket sema*/
	lock_sema = tim_bkt_fetch_sema_lock(bkt);

	/* Bucket related checks. */
	if (unlikely(tim_bkt_get_hbt(lock_sema))) {
		tim_bkt_dec_lock(bkt);
		goto __retry;
	}

	rem = tim_bkt_fetch_rem(lock_sema);

	if (rem < 0) {
		/* Goto diff bucket. */
		tim_bkt_dec_lock(bkt);
		goto __retry;
	} else if (!rem) {
		/* Only one thread can be here*/
		if (flags & OTX2_TIM_ENA_FB)
			chunk = tim_refill_chunk(bkt, tim_ring);
		if (flags & OTX2_TIM_ENA_DFB)
			chunk = tim_insert_chunk(bkt, tim_ring);

		if (unlikely(chunk == NULL)) {
			tim_bkt_set_rem(bkt, 0);
			tim_bkt_dec_lock(bkt);
			tim->impl_opaque[0] = 0;
			tim->impl_opaque[1] = 0;
			tim->state = RTE_EVENT_TIMER_ERROR;
			return -ENOMEM;
		}
		bkt->current_chunk = (uintptr_t)chunk;
		tim_bkt_set_rem(bkt, tim_ring->nb_chunk_slots - 1);
	} else {
		chunk = (struct otx2_tim_ent *)(uintptr_t)bkt->current_chunk;
		chunk += tim_ring->nb_chunk_slots - rem;
	}

	/* Copy work entry. */
	*chunk = *pent;
	tim_bkt_dec_lock(bkt);
	tim_bkt_inc_nent(bkt);
	tim->impl_opaque[0] = (uintptr_t)chunk;
	tim->impl_opaque[1] = (uintptr_t)bkt;
	tim->state = RTE_EVENT_TIMER_ARMED;

	return 0;
}

#endif /* __OTX2_TIM_WORKER_H__ */
