/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#include <rte_log.h>
#include <rte_malloc.h>
#include "tf_core.h"
#include "ulp_mapper.h"
#include "ulp_alloc_tbl.h"
#include "bnxt_ulp_utils.h"

#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#include "ulp_template_debug_proto.h"
#include "ulp_tf_debug.h"
#endif

/* Retrieve the allocator table  initialization parameters for the tbl_idx */
static const struct bnxt_ulp_allocator_tbl_params*
ulp_allocator_tbl_params_get(struct bnxt_ulp_context *ulp_ctx, uint32_t tbl_idx)
{
	struct bnxt_ulp_device_params *dparms;
	const struct bnxt_ulp_allocator_tbl_params *alloc_tbl;
	uint32_t dev_id;

	if (tbl_idx >= BNXT_ULP_ALLOCATOR_TBL_MAX_SZ) {
		BNXT_DRV_DBG(ERR, "Allocator table out of bounds %d\n",
			     tbl_idx);
		return NULL;
	}

	if (bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id))
		return NULL;

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		BNXT_DRV_DBG(ERR, "Failed to get device parms\n");
		return NULL;
	}

	alloc_tbl = &dparms->allocator_tbl_params[tbl_idx];
	return alloc_tbl;
}

/*
 * Initialize the allocator table list
 *
 * mapper_data [in] Pointer to the mapper data and the allocator table is
 * part of it
 *
 * returns 0 on success
 */
int32_t
ulp_allocator_tbl_list_init(struct bnxt_ulp_context *ulp_ctx,
			    struct bnxt_ulp_mapper_data *mapper_data)
{
	const struct bnxt_ulp_allocator_tbl_params *tbl;
	struct ulp_allocator_tbl_entry *entry;
	uint32_t idx, pool_size;

	/* Allocate the generic tables. */
	for (idx = 0; idx < BNXT_ULP_ALLOCATOR_TBL_MAX_SZ; idx++) {
		tbl = ulp_allocator_tbl_params_get(ulp_ctx, idx);
		if (!tbl) {
			BNXT_DRV_DBG(ERR, "Failed to get alloc table parm %d\n",
				     idx);
			return -EINVAL;
		}
		entry = &mapper_data->alloc_tbl[idx];

		/* Allocate memory for result data and key data */
		if (tbl->num_entries != 0) {
			/* assign the name */
			entry->alloc_tbl_name = tbl->name;
			entry->num_entries = tbl->num_entries;
			pool_size = BITALLOC_SIZEOF(tbl->num_entries);
			/* allocate the big chunk of memory */
			entry->ulp_bitalloc = rte_zmalloc("ulp allocator",
							  pool_size, 0);
			if (!entry->ulp_bitalloc) {
				BNXT_DRV_DBG(ERR,
					     "%s:Fail to alloc bit alloc %d\n",
					     tbl->name, idx);
				return -ENOMEM;
			}
			if (ba_init(entry->ulp_bitalloc, entry->num_entries,
				    true)) {
				BNXT_DRV_DBG(ERR, "%s:Unable to alloc ba %d\n",
					     tbl->name, idx);
				return -ENOMEM;
			}
		} else {
			BNXT_DRV_DBG(DEBUG, "%s:Unused alloc tbl entry is %d\n",
				     tbl->name, idx);
			continue;
		}
	}
	return 0;
}

/*
 * Free the allocator table list
 *
 * mapper_data [in] Pointer to the mapper data and the generic table is
 * part of it
 *
 * returns 0 on success
 */
int32_t
ulp_allocator_tbl_list_deinit(struct bnxt_ulp_mapper_data *mapper_data)
{
	struct ulp_allocator_tbl_entry *entry;
	uint32_t idx;

	/* iterate the generic table. */
	for (idx = 0; idx < BNXT_ULP_ALLOCATOR_TBL_MAX_SZ; idx++) {
		entry = &mapper_data->alloc_tbl[idx];
		if (entry->ulp_bitalloc) {
			rte_free(entry->ulp_bitalloc);
			entry->ulp_bitalloc = NULL;
		}
	}
	/* success */
	return 0;
}

/*
 * utility function to calculate the table idx
 *
 * res_sub_type [in] - Resource sub type
 * dir [in] - Direction
 *
 * returns None
 */
static int32_t
ulp_allocator_tbl_idx_calculate(uint32_t res_sub_type, uint32_t dir)
{
	int32_t tbl_idx;

	/* Validate for direction */
	if (dir >= TF_DIR_MAX) {
		BNXT_DRV_DBG(ERR, "invalid argument %x\n", dir);
		return -EINVAL;
	}
	tbl_idx = (res_sub_type << 1) | (dir & 0x1);
	if (tbl_idx >= BNXT_ULP_ALLOCATOR_TBL_MAX_SZ) {
		BNXT_DRV_DBG(ERR, "invalid table index %x\n", tbl_idx);
		return -EINVAL;
	}
	return tbl_idx;
}

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
			     int32_t *alloc_id)
{
	struct ulp_allocator_tbl_entry *entry;
	int32_t idx;

	idx = ulp_allocator_tbl_idx_calculate(res_sub_type, dir);
	if (idx < 0)
		return -EINVAL;

	entry = &mapper_data->alloc_tbl[idx];
	if (!entry->ulp_bitalloc || !entry->num_entries) {
		BNXT_DRV_DBG(ERR, "invalid table index %x\n", idx);
		return -EINVAL;
	}
	*alloc_id = ba_alloc(entry->ulp_bitalloc);

	if (*alloc_id < 0) {
		BNXT_DRV_DBG(ERR, "unable to alloc index %x\n", idx);
		return -ENOMEM;
	}
	/* Not using zero index */
	*alloc_id += 1;
	return 0;
}

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
			    int32_t index)
{
	struct ulp_allocator_tbl_entry *entry;
	int32_t rc;
	int32_t idx;

	idx = ulp_allocator_tbl_idx_calculate(res_sub_type, dir);
	if (idx < 0)
		return -EINVAL;

	entry = &mapper_data->alloc_tbl[idx];
	if (!entry->ulp_bitalloc || !entry->num_entries) {
		BNXT_DRV_DBG(ERR, "invalid table index %x\n", idx);
		return -EINVAL;
	}
	/* not using zero index */
	index -= 1;
	if (index < 0 || index > entry->num_entries) {
		BNXT_DRV_DBG(ERR, "invalid alloc index %x\n", index);
		return -EINVAL;
	}
	rc = ba_free(entry->ulp_bitalloc, index);
	if (rc < 0) {
		BNXT_DRV_DBG(ERR, "%s:unable to free index %x\n",
			     entry->alloc_tbl_name, index);
		return -EINVAL;
	}
	return 0;
}
