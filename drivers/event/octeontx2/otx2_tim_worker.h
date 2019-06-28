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

#endif /* __OTX2_TIM_WORKER_H__ */
