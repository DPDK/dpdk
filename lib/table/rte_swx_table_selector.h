/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */
#ifndef __INCLUDE_RTE_SWX_TABLE_SELECTOR_H__
#define __INCLUDE_RTE_SWX_TABLE_SELECTOR_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE SWX Selector Table
 *
 * Selector table interface.
 */

#include <stdint.h>

#include <rte_compat.h>

#include "rte_swx_table.h"

/** Selector table creation parameters. */
struct rte_swx_table_selector_params {
	/** Group ID offset. */
	uint32_t group_id_offset;

	/** Selector size in bytes. Must be non-zero. */
	uint32_t selector_size;

	/** Offset of the first byte of the selector within the selector buffer. */
	uint32_t selector_offset;

	/** Mask of *selector_size* bytes logically laid over the bytes at positions
	 * selector_offset* .. (*selector_offset* + *selector_size* - 1) of the selector buffer in
	 * order to specify which bits from the selector buffer are part of the selector and which
	 * ones are not. A bit value of 1 in the *selector_mask* means the respective bit in the
	 * selector buffer is part of the selector, while a bit value of 0 means the opposite. A
	 * NULL value means that all the bits are part of the selector, i.e. the *selector_mask*
	 * is an all-ones mask.
	 */
	uint8_t *selector_mask;

	/** Member ID offset. */
	uint32_t member_id_offset;

	/** Maximum number of groups. Must be non-zero. */
	uint32_t n_groups_max;

	/** Maximum number of members per group. Must be non-zero. */
	uint32_t n_members_per_group_max;
};

/** Group member parameters. */
struct rte_swx_table_selector_member {
	/** Linked list connectivity. */
	RTE_TAILQ_ENTRY(rte_swx_table_selector_member) node;

	/** Member ID. */
	uint32_t member_id;

	/** Member weight. */
	uint32_t member_weight;
};

/** List of group members. */
RTE_TAILQ_HEAD(rte_swx_table_selector_member_list, rte_swx_table_selector_member);

/** Group parameters. */
struct rte_swx_table_selector_group {
	/** List of group members. */
	struct rte_swx_table_selector_member_list members;
};

/**
 * Selector table memory footprint get
 *
 * @param[in] n_groups_max
 *   Maximum number of groups. Must be non-zero.
 * @param[in] n_members_per_group_max
 *   Maximum number of members per group. Must be non-zero.
 * @return
 *   Selector table memory footprint in bytes.
 */
__rte_experimental
uint64_t
rte_swx_table_selector_footprint_get(uint32_t n_groups_max, uint32_t n_members_per_group_max);

/**
 * Selector table mailbox size get
 *
 * The mailbox is used to store the context of a select operation that is in
 * progress and it is passed as a parameter to the select operation. This allows
 * for multiple concurrent select operations into the same table.
 *
 * @return
 *   Selector table mailbox footprint in bytes.
 */
__rte_experimental
uint64_t
rte_swx_table_selector_mailbox_size_get(void);

/**
 * Selector table create
 *
 * @param[in] params
 *   Selector table creation parameters.
 * @param[in] groups
 *   Groups to be added to the table at creation time. When NULL, it signifies that all groups are
 *   invalid, otherwise it points to a pre-allocated array of size *n_groups_max*, where a NULL
 *   element indicates that the associated group is invalid.
 * @param[in] numa_node
 *   Non-Uniform Memory Access (NUMA) node.
 * @return
 *   Table handle, on success, or NULL, on error.
 */
__rte_experimental
void *
rte_swx_table_selector_create(struct rte_swx_table_selector_params *params,
			      struct rte_swx_table_selector_group **groups,
			      int numa_node);

/**
 * Group set
 *
 * @param[in] table
 *   Selector table handle.
 * @param[in] group_id
 *   Group ID.
 * @param[in] group
 *   Group parameters.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument(s);
 *   -ENOSPC: Too many group members.
 */
__rte_experimental
int
rte_swx_table_selector_group_set(void *table,
				 uint32_t group_id,
				 struct rte_swx_table_selector_group *group);

/**
 * Selector table select
 *
 * This operation selects a member from the given group based on a hasing scheme.
 *
 * Multiple invocations of this function may be required in order to complete a single select
 * operation for a given table and a given group ID. The completion of the operation is flagged by
 * a return value of 1; in case of a return value of 0, the function must be invoked again with
 * exactly the same arguments.
 *
 * The mailbox argument is used to store the context of each on-going  operation. The mailbox
 * mechanism allows for multiple concurrent select operations into the same table.
 *
 * The typical reason an implementation may choose to split the operation into multiple steps is to
 * hide the latency of the inherrent memory read operations: before a read operation with the
 * source data likely not in the CPU cache, the source data prefetch is issued and the operation is
 * postponed in favor of some other unrelated work, which the CPU executes in parallel with the
 * source data being fetched into the CPU cache; later on, the operation is resumed, this time with
 * the source data likely to be read from the CPU cache with no CPU pipeline stall, which
 * significantly improves the operation performance.
 *
 * @param[in] table
 *   Selector table handle.
 * @param[in] mailbox
 *   Mailbox for the current operation.
 * @param[in] group_id_buffer
 *   Buffer where the input group ID is located at offset *group_id_offset*.
 * @param[in] selector_buffer
 *   Buffer where the key to select a member within the identified group is located starting from
 *   offset *selector_offset*. Its size must be equal to the table *selector_size*.
 * @param[in] member_id_buffer
 *   Buffer where the output member ID is to be placed at offset *member_id_offset*.
 * @return
 *   0 when the operation is not yet completed, and 1 when the operation is complete. No other
 *   return values are allowed.
 */
__rte_experimental
int
rte_swx_table_selector_select(void *table,
			      void *mailbox,
			      uint8_t **group_id_buffer,
			      uint8_t **selector_buffer,
			      uint8_t **member_id_buffer);

/**
 * Selector table free
 *
 * @param[in] table
 *   Selector table handle.
 */
__rte_experimental
void
rte_swx_table_selector_free(void *table);

#ifdef __cplusplus
}
#endif

#endif
