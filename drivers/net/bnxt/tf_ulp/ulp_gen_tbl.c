/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#include <rte_log.h>
#include <rte_malloc.h>
#include "tf_core.h"
#include "tfp.h"
#include "ulp_mapper.h"
#include "ulp_flow_db.h"

/* Retrieve the generic table  initialization parameters for the tbl_idx */
static struct bnxt_ulp_generic_tbl_params*
ulp_mapper_gen_tbl_params_get(uint32_t tbl_idx)
{
	if (tbl_idx >= BNXT_ULP_GEN_TBL_MAX_SZ)
		return NULL;

	return &ulp_generic_tbl_params[tbl_idx];
}

/*
 * Initialize the generic table list
 *
 * mapper_data [in] Pointer to the mapper data and the generic table is
 * part of it
 *
 * returns 0 on success
 */
int32_t
ulp_mapper_generic_tbl_list_init(struct bnxt_ulp_mapper_data *mapper_data)
{
	struct bnxt_ulp_generic_tbl_params *tbl;
	struct ulp_mapper_gen_tbl_list *entry;
	uint32_t idx, size;

	/* Allocate the generic tables. */
	for (idx = 0; idx < BNXT_ULP_GEN_TBL_MAX_SZ; idx++) {
		tbl = ulp_mapper_gen_tbl_params_get(idx);
		if (!tbl) {
			BNXT_TF_DBG(ERR, "Failed to get gen table parms %d\n",
				    idx);
			return -EINVAL;
		}
		entry = &mapper_data->gen_tbl_list[idx];
		if (tbl->result_num_entries != 0) {
			/* add 4 bytes for reference count */
			entry->mem_data_size = (tbl->result_num_entries + 1) *
				(tbl->result_byte_size + sizeof(uint32_t));

			/* allocate the big chunk of memory */
			entry->mem_data = rte_zmalloc("ulp mapper gen tbl",
						      entry->mem_data_size, 0);
			if (!entry->mem_data) {
				BNXT_TF_DBG(ERR,
					    "Failed to allocate gen table %d\n",
					    idx);
				return -ENOMEM;
			}
			/* Populate the generic table container */
			entry->container.num_elem = tbl->result_num_entries;
			entry->container.byte_data_size = tbl->result_byte_size;
			entry->container.ref_count =
				(uint32_t *)entry->mem_data;
			size = sizeof(uint32_t) * (tbl->result_num_entries + 1);
			entry->container.byte_data = &entry->mem_data[size];
			entry->container.byte_order = tbl->result_byte_order;
		}
	}
	/* success */
	return 0;
}

/*
 * Free the generic table list
 *
 * mapper_data [in] Pointer to the mapper data and the generic table is
 * part of it
 *
 * returns 0 on success
 */
int32_t
ulp_mapper_generic_tbl_list_deinit(struct bnxt_ulp_mapper_data *mapper_data)
{
	struct ulp_mapper_gen_tbl_list *tbl_list;
	uint32_t idx;

	/* iterate the generic table. */
	for (idx = 0; idx < BNXT_ULP_GEN_TBL_MAX_SZ; idx++) {
		tbl_list = &mapper_data->gen_tbl_list[idx];
		if (tbl_list->mem_data) {
			rte_free(tbl_list->mem_data);
			tbl_list->mem_data = NULL;
		}
	}
	/* success */
	return 0;
}

/*
 * Get the generic table list entry
 *
 * ulp_ctxt [in] - Ptr to ulp_context
 * tbl_idx [in] -  Table index to the generic table list
 * key [in] - Key index to the table
 * entry [out] - output will include the entry if found
 *
 * returns 0 on success.
 */
int32_t
ulp_mapper_gen_tbl_entry_get(struct bnxt_ulp_context *ulp,
			     uint32_t tbl_idx,
			     uint32_t key,
			     struct ulp_mapper_gen_tbl_entry *entry)
{
	struct bnxt_ulp_mapper_data *mapper_data;
	struct ulp_mapper_gen_tbl_list *tbl_list;

	mapper_data = bnxt_ulp_cntxt_ptr2_mapper_data_get(ulp);
	if (!mapper_data || tbl_idx >= BNXT_ULP_GEN_TBL_MAX_SZ ||
	    !entry) {
		BNXT_TF_DBG(ERR, "invalid arguments %x:%x\n", tbl_idx, key);
		return -EINVAL;
	}
	/* populate the output and return the values */
	tbl_list = &mapper_data->gen_tbl_list[tbl_idx];
	if (key > tbl_list->container.num_elem) {
		BNXT_TF_DBG(ERR, "invalid key %x:%x\n", key,
			    tbl_list->container.num_elem);
		return -EINVAL;
	}
	entry->ref_count = &tbl_list->container.ref_count[key];
	entry->byte_data_size = tbl_list->container.byte_data_size;
	entry->byte_data = &tbl_list->container.byte_data[key *
		entry->byte_data_size];
	entry->byte_order = tbl_list->container.byte_order;
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
int32_t
ulp_mapper_gen_tbl_idx_calculate(uint32_t res_sub_type, uint32_t dir)
{
	int32_t tbl_idx;

	/* Validate for direction */
	if (dir >= TF_DIR_MAX) {
		BNXT_TF_DBG(ERR, "invalid argument %x\n", dir);
		return -EINVAL;
	}
	tbl_idx = (res_sub_type << 1) | (dir & 0x1);
	if (tbl_idx >= BNXT_ULP_GEN_TBL_MAX_SZ) {
		BNXT_TF_DBG(ERR, "invalid table index %x\n", tbl_idx);
		return -EINVAL;
	}
	return tbl_idx;
}

/*
 * Set the data in the generic table entry, Data is in Big endian format
 *
 * entry [in] - generic table entry
 * offset [in] - The offset in bits where the data has to be set
 * len [in] - The length of the data in bits to be set
 * data [in] - pointer to the data to be used for setting the value.
 * data_size [in] - length of the data pointer in bytes.
 *
 * returns 0 on success
 */
int32_t
ulp_mapper_gen_tbl_entry_data_set(struct ulp_mapper_gen_tbl_entry *entry,
				  uint32_t offset, uint32_t len, uint8_t *data,
				  uint32_t data_size)
{
	/* validate the null arguments */
	if (!entry || !data) {
		BNXT_TF_DBG(ERR, "invalid argument\n");
		return -EINVAL;
	}

	/* check the size of the buffer for validation */
	if ((offset + len) > ULP_BYTE_2_BITS(entry->byte_data_size) ||
	    data_size < ULP_BITS_2_BYTE(len)) {
		BNXT_TF_DBG(ERR, "invalid offset or length %x:%x:%x\n",
			    offset, len, entry->byte_data_size);
		return -EINVAL;
	}

	/* adjust the data pointer */
	data = data + (data_size - ULP_BITS_2_BYTE(len));

	/* Push the data into the byte data array */
	if (entry->byte_order == BNXT_ULP_BYTE_ORDER_LE) {
		if (ulp_bs_push_lsb(entry->byte_data, offset, len, data) !=
		    len) {
			BNXT_TF_DBG(ERR, "write failed offset = %x, len =%x\n",
				    offset, len);
			return -EIO;
		}
	} else {
		if (ulp_bs_push_msb(entry->byte_data, offset, len, data) !=
		    len) {
			BNXT_TF_DBG(ERR, "write failed offset = %x, len =%x\n",
				    offset, len);
			return -EIO;
		}
	}
	return 0;
}

/*
 * Get the data in the generic table entry, Data is in Big endian format
 *
 * entry [in] - generic table entry
 * offset [in] - The offset in bits where the data has to get
 * len [in] - The length of the data in bits to be get
 * data [out] - pointer to the data to be used for setting the value.
 * data_size [in] - The size of data in bytes
 *
 * returns 0 on success
 */
int32_t
ulp_mapper_gen_tbl_entry_data_get(struct ulp_mapper_gen_tbl_entry *entry,
				  uint32_t offset, uint32_t len, uint8_t *data,
				  uint32_t data_size)
{
	/* validate the null arguments */
	if (!entry || !data) {
		BNXT_TF_DBG(ERR, "invalid argument\n");
		return -EINVAL;
	}

	/* check the size of the buffer for validation */
	if ((offset + len) > ULP_BYTE_2_BITS(entry->byte_data_size) ||
	    len > ULP_BYTE_2_BITS(data_size)) {
		BNXT_TF_DBG(ERR, "invalid offset or length %x:%x:%x\n",
			    offset, len, entry->byte_data_size);
		return -EINVAL;
	}
	if (entry->byte_order == BNXT_ULP_BYTE_ORDER_LE)
		ulp_bs_pull_lsb(entry->byte_data, data, data_size, offset, len);
	else
		ulp_bs_pull_msb(entry->byte_data, data, offset, len);

	return 0;
}

/*
 * Free the generic table list entry
 *
 * ulp_ctx [in] - Pointer to the ulp context
 * res [in] - Pointer to flow db resource entry
 *
 * returns 0 on success
 */
int32_t
ulp_mapper_gen_tbl_res_free(struct bnxt_ulp_context *ulp_ctx,
			    struct ulp_flow_db_res_params *res)
{
	struct ulp_mapper_gen_tbl_entry entry;
	int32_t tbl_idx;
	uint32_t fid;

	/* Extract the resource sub type and direction */
	tbl_idx = ulp_mapper_gen_tbl_idx_calculate(res->resource_sub_type,
						   res->direction);
	if (tbl_idx < 0) {
		BNXT_TF_DBG(ERR, "invalid argument %x:%x\n",
			    res->resource_sub_type, res->direction);
		return -EINVAL;
	}

	/* Get the generic table entry*/
	if (ulp_mapper_gen_tbl_entry_get(ulp_ctx, tbl_idx, res->resource_hndl,
					 &entry)) {
		BNXT_TF_DBG(ERR, "Gen tbl entry get failed %x:%" PRIX64 "\n",
			    tbl_idx, res->resource_hndl);
		return -EINVAL;
	}

	/* Decrement the reference count */
	if (!ULP_GEN_TBL_REF_CNT(&entry)) {
		BNXT_TF_DBG(ERR, "generic table corrupt %x:%" PRIX64 "\n",
			    tbl_idx, res->resource_hndl);
		return -EINVAL;
	}
	ULP_GEN_TBL_REF_CNT_DEC(&entry);

	/* retain the details since there are other users */
	if (ULP_GEN_TBL_REF_CNT(&entry))
		return 0;

	/* Delete the generic table entry. First extract the fid */
	if (ulp_mapper_gen_tbl_entry_data_get(&entry, ULP_GEN_TBL_FID_OFFSET,
					      ULP_GEN_TBL_FID_SIZE_BITS,
					      (uint8_t *)&fid,
					      sizeof(fid))) {
		BNXT_TF_DBG(ERR, "Unable to get fid %x:%" PRIX64 "\n",
			    tbl_idx, res->resource_hndl);
		return -EINVAL;
	}
	fid = tfp_be_to_cpu_32(fid);

	/* Destroy the flow associated with the shared flow id */
	if (ulp_mapper_flow_destroy(ulp_ctx, BNXT_ULP_FDB_TYPE_REGULAR,
				    fid))
		BNXT_TF_DBG(ERR, "Error in deleting shared flow id %x\n", fid);

	/* clear the byte data of the generic table entry */
	memset(entry.byte_data, 0, entry.byte_data_size);

	return 0;
}
