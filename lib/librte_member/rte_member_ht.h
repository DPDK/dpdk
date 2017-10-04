/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RTE_MEMBER_HT_H_
#define _RTE_MEMBER_HT_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of pushes for cuckoo path in HT mode. */
#define RTE_MEMBER_MAX_PUSHES 50

typedef uint16_t member_sig_t;			/* signature size is 16 bit */

/* The bucket struct for ht setsum */
struct member_ht_bucket {
	member_sig_t sigs[RTE_MEMBER_BUCKET_ENTRIES];	/* 2-byte signature */
	member_set_t sets[RTE_MEMBER_BUCKET_ENTRIES];	/* 2-byte set */
} __rte_cache_aligned;

int
rte_member_create_ht(struct rte_member_setsum *ss,
		const struct rte_member_parameters *params);

int
rte_member_lookup_ht(const struct rte_member_setsum *setsum,
		const void *key, member_set_t *set_id);

uint32_t
rte_member_lookup_bulk_ht(const struct rte_member_setsum *setsum,
		const void **keys, uint32_t num_keys,
		member_set_t *set_ids);

uint32_t
rte_member_lookup_multi_ht(const struct rte_member_setsum *setsum,
		const void *key, uint32_t match_per_key,
		member_set_t *set_id);

uint32_t
rte_member_lookup_multi_bulk_ht(const struct rte_member_setsum *setsum,
		const void **keys, uint32_t num_keys, uint32_t match_per_key,
		uint32_t *match_count,
		member_set_t *set_ids);

int
rte_member_add_ht(const struct rte_member_setsum *setsum,
		const void *key, member_set_t set_id);

void
rte_member_free_ht(struct rte_member_setsum *setsum);

int
rte_member_delete_ht(const struct rte_member_setsum *ss, const void *key,
		member_set_t set_id);

void
rte_member_reset_ht(const struct rte_member_setsum *setsum);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_MEMBER_HT_H_ */
