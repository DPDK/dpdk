/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef RTE_MEMBER_SKETCH_H
#define RTE_MEMBER_SKETCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_vect.h>
#include <rte_ring_elem.h>

#define NUM_ROW_SCALAR 5
#define INTERVAL (1 << 15)

#if !RTE_IS_POWER_OF_2(INTERVAL)
#error sketch INTERVAL macro must be a power of 2
#endif

int
rte_member_create_sketch(struct rte_member_setsum *ss,
			 const struct rte_member_parameters *params,
			 struct rte_ring *r);

int
rte_member_lookup_sketch(const struct rte_member_setsum *setsum,
			 const void *key, member_set_t *set_id);

int
rte_member_add_sketch(const struct rte_member_setsum *setsum,
		      const void *key,
		      member_set_t set_id);

int
rte_member_add_sketch_byte_count(const struct rte_member_setsum *ss,
				 const void *key, uint32_t byte_count);

void
sketch_update_scalar(const struct rte_member_setsum *ss,
		     const void *key,
		     uint32_t count);

uint64_t
sketch_lookup_scalar(const struct rte_member_setsum *ss,
		     const void *key);

void
sketch_delete_scalar(const struct rte_member_setsum *ss,
		     const void *key);

int
rte_member_delete_sketch(const struct rte_member_setsum *setsum,
			 const void *key);

int
rte_member_query_sketch(const struct rte_member_setsum *setsum,
			const void *key, uint64_t *output);

void
rte_member_free_sketch(struct rte_member_setsum *ss);

void
rte_member_reset_sketch(const struct rte_member_setsum *setsum);

int
rte_member_report_heavyhitter_sketch(const struct rte_member_setsum *setsum,
				     void **key, uint64_t *count);

void
rte_member_update_heap(const struct rte_member_setsum *ss);

static __rte_always_inline uint64_t
count_min(const struct rte_member_setsum *ss, const uint32_t *hash_results)
{
	uint64_t *count_array = ss->table;
	uint64_t count;
	uint32_t cur_row;
	uint64_t min = UINT64_MAX;

	for (cur_row = 0; cur_row < ss->num_row; cur_row++) {
		uint64_t cnt = count_array[cur_row * ss->num_col + hash_results[cur_row]];

		if (cnt < min)
			min = cnt;
	}
	count = min;

	return count;
}

#ifdef __cplusplus
}
#endif

#endif /* RTE_MEMBER_SKETCH_H */
