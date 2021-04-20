/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
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
