/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _RTE_MEMBER_VBF_H_
#define _RTE_MEMBER_VBF_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Currently we only support up to 32 sets in vBF */
#define RTE_MEMBER_MAX_BF 32

int
rte_member_create_vbf(struct rte_member_setsum *ss,
		const struct rte_member_parameters *params);

int
rte_member_lookup_vbf(const struct rte_member_setsum *setsum,
		const void *key, member_set_t *set_id);

uint32_t
rte_member_lookup_bulk_vbf(const struct rte_member_setsum *setsum,
		const void **keys, uint32_t num_keys,
		member_set_t *set_ids);

uint32_t
rte_member_lookup_multi_vbf(const struct rte_member_setsum *setsum,
		const void *key, uint32_t match_per_key,
		member_set_t *set_id);

uint32_t
rte_member_lookup_multi_bulk_vbf(const struct rte_member_setsum *setsum,
		const void **keys, uint32_t num_keys, uint32_t match_per_key,
		uint32_t *match_count,
		member_set_t *set_ids);

int
rte_member_add_vbf(const struct rte_member_setsum *setsum,
		const void *key, member_set_t set_id);

void
rte_member_free_vbf(struct rte_member_setsum *ss);

void
rte_member_reset_vbf(const struct rte_member_setsum *setsum);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_MEMBER_VBF_H_ */
