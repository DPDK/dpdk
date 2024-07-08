/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 * Copyright(c) 2018-2024 Arm Limited
 */

#ifndef COMPARE_SIGNATURES_GENERIC_H
#define COMPARE_SIGNATURES_GENERIC_H

#include <inttypes.h>

#include <rte_common.h>
#include <rte_vect.h>

#include "rte_cuckoo_hash.h"

static inline void
compare_signatures(uint32_t *prim_hash_matches, uint32_t *sec_hash_matches,
			const struct rte_hash_bucket *prim_bkt,
			const struct rte_hash_bucket *sec_bkt,
			uint16_t sig,
			__rte_unused enum rte_hash_sig_compare_function sig_cmp_fn)
{
	unsigned int i;

	/* For match mask the first bit of every two bits indicates the match */
	for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
		*prim_hash_matches |=
			((sig == prim_bkt->sig_current[i]) << (i << 1));
		*sec_hash_matches |=
			((sig == sec_bkt->sig_current[i]) << (i << 1));
	}
}
#endif /* COMPARE_SIGNATURES_GENERIC_H */
