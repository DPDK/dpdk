/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _TFC_CPM_H_
#define _TFC_CPM_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include "cfa_types.h"

/*
 * Set to 1 to force using just TS 0
 */
#define TFC_FORCE_POOL_0 1

/*
 * Temp to enable build. Remove when tfc_cmm is added
 */
struct tfc_cmm {
	int a;
};


struct tfc_cpm;

#define TFC_CPM_INVALID_POOL_ID 0xFFFF

/**
 * int tfc_cpm_open
 *
 * Initializes pre-allocated CPM structure. The cpm_db_size argument is
 * validated against the max_pools argument.
 *
 * @param[in, out] cpm
 *  Pointer to pointer of the allocated CPM data structure. The open will
 *  perform the alloc and return a pointer to the allocated memory.
 *
 * @param[in] max_pools
 *  Maximum number of pools
 *
 * Returns:
 * 0 - Success
 * -EINVAL - cpm_db_size is not correct
 * -ENOMEM - Failed to allocate memory for CPM data structures.
 *
 */
int tfc_cpm_open(struct tfc_cpm **cpm, uint32_t max_pools);

/**
 * int tfc_cpm_close
 *
 * Deinitialize data structures. Note this does not free the memory.
 *
 * @param[in] cpm
 *  Pointer to the CPM instance to free.
 *
 * Returns:
 * 0 - Success
 * -EINVAL - Invalid argument
 */
int tfc_cpm_close(struct tfc_cpm *cpm);

/**
 * int tfc_cpm_set_pool_size
 *
 * Sets number of entries for pools in a given region.
 *
 * @param[in] cpm
 *  Pointer to the CPM instance
 *
 * @param[in] pool_sz_in_records
 *  Max number of entries for each pool must be
 *  a power of 2.
 *
 * Returns:
 * 0 - Success
 * -EINVAL - Invalid argument
 *
 */
int tfc_cpm_set_pool_size(struct tfc_cpm *cpm, uint32_t pool_sz_in_records);

/**
 * int tfc_cpm_get_pool_size
 *
 * Returns the number of entries for pools in a given region.
 *
 * @param[in] cpm
 *  Pointer to the CPM instance
 *
 * @param[out] pool_sz_in_records
 *  Max number of entries for each pool
 *
 * Returns:
 * 0 - Success
 * -EINVAL - Invalid argument
 */
int tfc_cpm_get_pool_size(struct tfc_cpm *cpm, uint32_t *pool_sz_in_records);

/**
 * int tfc_cpm_set_cmm_inst
 *
 * Add CMM instance.
 *
 * @param[in] cpm
 *  Pointer to the CPM instance
 *
 * @param[in] pool_id
 *  Pool ID to use
 *
 * @param[in] valid
 *  Is entry valid
 *
 * @param[in] cmm
 *  Pointer to the CMM instance
 *
 * Returns:
 * 0 - Success
 * -EINVAL - Invalid argument
 */
int tfc_cpm_set_cmm_inst(struct tfc_cpm *cpm, uint16_t pool_id, struct tfc_cmm *cmm);

/**
 * int tfc_cpm_get_cmm_inst
 *
 * Get CMM instance.
 *
 * @param[in] cpm
 *  Pointer to the CPM instance
 *
 * @param[in] pool_id
 *  Pool ID to use
 *
 * @param[in] valid
 *  Is entry valid
 *
 * @param[out] cmm
 *  Pointer to the CMM instance
 *
 * Returns:
 * 0 - Success
 * -EINVAL - Invalid argument
 */
int tfc_cpm_get_cmm_inst(struct tfc_cpm *cpm, uint16_t pool_id, struct tfc_cmm **cmm);

/**
 * int tfc_cpm_get_avail_pool
 *
 * Returns the pool_id to use for the next EM  insert
 *
 * @param[in] cpm
 *  Pointer to the CPM instance
 *
 * @param[out] pool_id
 *  Pool ID to use for EM insert
 *
 * Returns:
 * 0 - Success
 * -EINVAL - Invalid argument
 */
int tfc_cpm_get_avail_pool(struct tfc_cpm *cpm, uint16_t *pool_id);

/**
 * int tfc_cpm_set_usage
 *
 * Set the usage_count and all_used fields for the specified pool_id
 *
 * @param[in] cpm
 *  Pointer to the CPM instance
 *
 * @param[in] pool_id
 *  Pool ID to update
 *
 * @param[in] used_count
 *  Number of entries used within specified pool
 *
 * @param[in] all_used
 *  Set if all pool entries are used
 *
 * Returns:
 * 0 - Success
 * -EINVAL - Invalid argument
 */
int tfc_cpm_set_usage(struct tfc_cpm *cpm, uint16_t pool_id, uint32_t used_count, bool all_used);

/**
 * int tfc_cpm_srchm_by_configured_pool
 *
 * Get the next configured pool
 *
 * @param[in] cpm
 *  Pointer to the CPM instance
 *
 * @param[in] srch_mode
 *  Valid pool id
 *
 * @param[out] pool_id
 *  Pointer to a valid pool id
 *
 * * @param[out] cmm
 *  Pointer to the associated CMM instance
 *
 * Returns:
 * 0 - Success
 * -EINVAL - Invalid argument
 */
int tfc_cpm_srchm_by_configured_pool(struct tfc_cpm *cpm, enum cfa_srch_mode srch_mode,
				     uint16_t *pool_id, struct tfc_cmm **cmm);

#endif /* _TFC_CPM_H_ */
