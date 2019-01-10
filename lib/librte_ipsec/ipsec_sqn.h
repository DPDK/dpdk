/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _IPSEC_SQN_H_
#define _IPSEC_SQN_H_

#define WINDOW_BUCKET_BITS		6 /* uint64_t */
#define WINDOW_BUCKET_SIZE		(1 << WINDOW_BUCKET_BITS)
#define WINDOW_BIT_LOC_MASK		(WINDOW_BUCKET_SIZE - 1)

/* minimum number of bucket, power of 2*/
#define WINDOW_BUCKET_MIN		2
#define WINDOW_BUCKET_MAX		(INT16_MAX + 1)

#define IS_ESN(sa)	((sa)->sqn_mask == UINT64_MAX)

/*
 * for given size, calculate required number of buckets.
 */
static uint32_t
replay_num_bucket(uint32_t wsz)
{
	uint32_t nb;

	nb = rte_align32pow2(RTE_ALIGN_MUL_CEIL(wsz, WINDOW_BUCKET_SIZE) /
		WINDOW_BUCKET_SIZE);
	nb = RTE_MAX(nb, (uint32_t)WINDOW_BUCKET_MIN);

	return nb;
}

/**
 * Based on number of buckets calculated required size for the
 * structure that holds replay window and sequence number (RSN) information.
 */
static size_t
rsn_size(uint32_t nb_bucket)
{
	size_t sz;
	struct replay_sqn *rsn;

	sz = sizeof(*rsn) + nb_bucket * sizeof(rsn->window[0]);
	sz = RTE_ALIGN_CEIL(sz, RTE_CACHE_LINE_SIZE);
	return sz;
}

#endif /* _IPSEC_SQN_H_ */
