/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef TF_RM_H_
#define TF_RM_H_

#include "tf_resources.h"
#include "tf_core.h"
#include "bitalloc.h"

struct tf;
struct tf_session;

/* Internal macro to determine appropriate allocation pools based on
 * DIRECTION parm, also performs error checking for DIRECTION parm. The
 * SESSION_POOL and SESSION pointers are set appropriately upon
 * successful return (the GLOBAL_POOL is used to globally manage
 * resource allocation and the SESSION_POOL is used to track the
 * resources that have been allocated to the session)
 *
 * parameters:
 *   struct tfp        *tfp
 *   enum tf_dir        direction
 *   struct bitalloc  **session_pool
 *   string             base_pool_name - used to form pointers to the
 *					 appropriate bit allocation
 *					 pools, both directions of the
 *					 session pools must have same
 *					 base name, for example if
 *					 POOL_NAME is feat_pool: - the
 *					 ptr's to the session pools
 *					 are feat_pool_rx feat_pool_tx
 *
 *  int                  rc - return code
 *			      0 - Success
 *			     -1 - invalid DIRECTION parm
 */
#define TF_RM_GET_POOLS(tfs, direction, session_pool, pool_name, rc) do { \
		(rc) = 0;						\
		if ((direction) == TF_DIR_RX) {				\
			*(session_pool) = (tfs)->pool_name ## _RX;	\
		} else if ((direction) == TF_DIR_TX) {			\
			*(session_pool) = (tfs)->pool_name ## _TX;	\
		} else {						\
			rc = -1;					\
		}							\
	} while (0)

#define TF_RM_GET_POOLS_RX(tfs, session_pool, pool_name)	\
	(*(session_pool) = (tfs)->pool_name ## _RX)

#define TF_RM_GET_POOLS_TX(tfs, session_pool, pool_name)	\
	(*(session_pool) = (tfs)->pool_name ## _TX)

/**
 * Resource query single entry
 */
struct tf_rm_query_entry {
	/** Minimum guaranteed number of elements */
	uint16_t min;
	/** Maximum non-guaranteed number of elements */
	uint16_t max;
};

/**
 * Resource single entry
 */
struct tf_rm_entry {
	/** Starting index of the allocated resource */
	uint16_t start;
	/** Number of allocated elements */
	uint16_t stride;
};

/**
 * Resource query array of HW entities
 */
struct tf_rm_hw_query {
	/** array of HW resource entries */
	struct tf_rm_query_entry hw_query[TF_RESC_TYPE_HW_MAX];
};

/**
 * Resource allocation array of HW entities
 */
struct tf_rm_hw_alloc {
	/** array of HW resource entries */
	uint16_t hw_num[TF_RESC_TYPE_HW_MAX];
};

/**
 * Resource query array of SRAM entities
 */
struct tf_rm_sram_query {
	/** array of SRAM resource entries */
	struct tf_rm_query_entry sram_query[TF_RESC_TYPE_SRAM_MAX];
};

/**
 * Resource allocation array of SRAM entities
 */
struct tf_rm_sram_alloc {
	/** array of SRAM resource entries */
	uint16_t sram_num[TF_RESC_TYPE_SRAM_MAX];
};

/**
 * Initializes the Resource Manager and the associated database
 * entries for HW and SRAM resources. Must be called before any other
 * Resource Manager functions.
 *
 * [in] tfp
 *   Pointer to TF handle
 */
void tf_rm_init(struct tf *tfp);

/**
 * Allocates and validates both HW and SRAM resources per the NVM
 * configuration. If any allocation fails all resources for the
 * session is deallocated.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_rm_allocate_validate(struct tf *tfp);

/**
 * Closes the Resource Manager and frees all allocated resources per
 * the associated database.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 *   - (-ENOTEMPTY) if resources are not cleaned up before close
 */
int tf_rm_close(struct tf *tfp);
#endif /* TF_RM_H_ */
