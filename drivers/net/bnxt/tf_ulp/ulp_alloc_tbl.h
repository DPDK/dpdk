/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_ALLOC_TBL_H_
#define _ULP_ALLOC_TBL_H_

#include "bitalloc.h"

/* Structure to pass the allocator table values across APIs */
struct ulp_allocator_tbl_entry {
	const char	*alloc_tbl_name;
	uint16_t	num_entries;
	struct bitalloc	*ulp_bitalloc;
};

/* Forward declaration */
struct bnxt_ulp_mapper_data;
struct ulp_flow_db_res_params;

/*
 * Initialize the allocator table list
 *
 * ulp_ctx [in] - Pointer to the ulp context
 * mapper_data [in] Pointer to the mapper data and the generic table is
 * part of it
 *
 * returns 0 on success
 */
int32_t
ulp_allocator_tbl_list_init(struct bnxt_ulp_context *ulp_ctx,
			    struct bnxt_ulp_mapper_data *mapper_data);

/*
 * Free the allocator table list
 *
 * mapper_data [in] Pointer to the mapper data and the generic table is
 * part of it
 *
 * returns 0 on success
 */
int32_t
ulp_allocator_tbl_list_deinit(struct bnxt_ulp_mapper_data *mapper_data);

/*
 * allocate a index from allocator
 *
 * mapper_data [in] Pointer to the mapper data and the allocator table is
 * part of it
 *
 * returns index on success or negative number on failure
 */
int32_t
ulp_allocator_tbl_list_alloc(struct bnxt_ulp_mapper_data *mapper_data,
			     uint32_t res_sub_type, uint32_t dir,
			     int32_t *alloc_id);

/*
 * free a index in allocator
 *
 * mapper_data [in] Pointer to the mapper data and the allocator table is
 * part of it
 *
 * returns error
 */
int32_t
ulp_allocator_tbl_list_free(struct bnxt_ulp_mapper_data *mapper_data,
			    uint32_t res_sub_type, uint32_t dir,
			    int32_t index);
#endif /* _ULP_ALLOC_TBL_H_ */
