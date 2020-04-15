/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#include <rte_log.h>
#include "bnxt.h"
#include "ulp_template_db.h"
#include "ulp_template_struct.h"
#include "bnxt_tf_common.h"
#include "ulp_utils.h"
#include "bnxt_ulp.h"
#include "tfp.h"
#include "tf_ext_flow_handle.h"
#include "ulp_mark_mgr.h"
#include "ulp_flow_db.h"
#include "ulp_mapper.h"

int32_t
ulp_mapper_action_tbls_process(struct bnxt_ulp_mapper_parms *parms);

/*
 * Get the size of the action property for a given index.
 *
 * idx [in] The index for the action property
 *
 * returns the size of the action property.
 */
static uint32_t
ulp_mapper_act_prop_size_get(uint32_t idx)
{
	if (idx >= BNXT_ULP_ACT_PROP_IDX_LAST)
		return 0;
	return ulp_act_prop_map_table[idx];
}

/*
 * Get the list of result fields that implement the flow action
 *
 * tbl [in] A single table instance to get the results fields
 * from num_flds [out] The number of data fields in the returned
 * array
 * returns array of data fields, or NULL on error
 */
static struct bnxt_ulp_mapper_result_field_info *
ulp_mapper_act_result_fields_get(struct bnxt_ulp_mapper_act_tbl_info *tbl,
				 uint32_t *num_rslt_flds,
				 uint32_t *num_encap_flds)
{
	uint32_t idx;

	if (!tbl || !num_rslt_flds || !num_encap_flds)
		return NULL;

	idx		= tbl->result_start_idx;
	*num_rslt_flds	= tbl->result_num_fields;
	*num_encap_flds = tbl->encap_num_fields;

	/* NOTE: Need template to provide range checking define */
	return &ulp_act_result_field_list[idx];
}

static int32_t
ulp_mapper_result_field_process(struct bnxt_ulp_mapper_parms *parms,
				struct bnxt_ulp_mapper_result_field_info *fld,
				struct ulp_blob *blob)
{
	uint16_t idx, size_idx;
	uint8_t	 *val = NULL;
	uint64_t regval;
	uint32_t val_size = 0, field_size = 0;

	switch (fld->result_opcode) {
	case BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT:
		val = fld->result_operand;
		if (!ulp_blob_push(blob, val, fld->field_bit_size)) {
			BNXT_TF_DBG(ERR, "Failed to add field\n");
			return -EINVAL;
		}
		break;
	case BNXT_ULP_RESULT_OPC_SET_TO_ACT_PROP:
		if (!ulp_operand_read(fld->result_operand,
				      (uint8_t *)&idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "operand read failed\n");
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);

		if (idx >= BNXT_ULP_ACT_PROP_IDX_LAST) {
			BNXT_TF_DBG(ERR, "act_prop[%d] oob\n", idx);
			return -EINVAL;
		}
		val = &parms->act_prop->act_details[idx];
		field_size = ulp_mapper_act_prop_size_get(idx);
		if (fld->field_bit_size < ULP_BYTE_2_BITS(field_size)) {
			field_size  = field_size -
			    ((fld->field_bit_size + 7) / 8);
			val += field_size;
		}
		if (!ulp_blob_push(blob, val, fld->field_bit_size)) {
			BNXT_TF_DBG(ERR, "push field failed\n");
			return -EINVAL;
		}
		break;
	case BNXT_ULP_RESULT_OPC_SET_TO_ACT_PROP_SZ:
		if (!ulp_operand_read(fld->result_operand,
				      (uint8_t *)&idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "operand read failed\n");
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);

		if (idx >= BNXT_ULP_ACT_PROP_IDX_LAST) {
			BNXT_TF_DBG(ERR, "act_prop[%d] oob\n", idx);
			return -EINVAL;
		}
		val = &parms->act_prop->act_details[idx];

		/* get the size index next */
		if (!ulp_operand_read(&fld->result_operand[sizeof(uint16_t)],
				      (uint8_t *)&size_idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "operand read failed\n");
			return -EINVAL;
		}
		size_idx = tfp_be_to_cpu_16(size_idx);

		if (size_idx >= BNXT_ULP_ACT_PROP_IDX_LAST) {
			BNXT_TF_DBG(ERR, "act_prop[%d] oob\n", size_idx);
			return -EINVAL;
		}
		memcpy(&val_size, &parms->act_prop->act_details[size_idx],
		       sizeof(uint32_t));
		val_size = tfp_be_to_cpu_32(val_size);
		val_size = ULP_BYTE_2_BITS(val_size);
		ulp_blob_push_encap(blob, val, val_size);
		break;
	case BNXT_ULP_RESULT_OPC_SET_TO_REGFILE:
		if (!ulp_operand_read(fld->result_operand,
				      (uint8_t *)&idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "operand read failed\n");
			return -EINVAL;
		}

		idx = tfp_be_to_cpu_16(idx);
		/* Uninitialized regfile entries return 0 */
		if (!ulp_regfile_read(parms->regfile, idx, &regval)) {
			BNXT_TF_DBG(ERR, "regfile[%d] read oob\n", idx);
			return -EINVAL;
		}

		val = ulp_blob_push_64(blob, &regval, fld->field_bit_size);
		if (!val) {
			BNXT_TF_DBG(ERR, "push field failed\n");
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* Function to alloc action record and set the table. */
static int32_t
ulp_mapper_action_alloc_and_set(struct bnxt_ulp_mapper_parms *parms,
				struct ulp_blob *blob)
{
	struct ulp_flow_db_res_params		fid_parms;
	struct tf_alloc_tbl_entry_parms		alloc_parms = { 0 };
	struct tf_free_tbl_entry_parms		free_parms = { 0 };
	struct bnxt_ulp_mapper_act_tbl_info	*atbls = parms->atbls;
	int32_t					rc = 0;
	int32_t trc;
	uint64_t				idx;

	/* Set the allocation parameters for the table*/
	alloc_parms.dir = atbls->direction;
	alloc_parms.type = atbls->table_type;
	alloc_parms.search_enable = atbls->srch_b4_alloc;
	alloc_parms.result = ulp_blob_data_get(blob,
					       &alloc_parms.result_sz_in_bytes);
	if (!alloc_parms.result) {
		BNXT_TF_DBG(ERR, "blob is not populated\n");
		return -EINVAL;
	}

	rc = tf_alloc_tbl_entry(parms->tfp, &alloc_parms);
	if (rc) {
		BNXT_TF_DBG(ERR, "table type= [%d] dir = [%s] alloc failed\n",
			    alloc_parms.type,
			    (alloc_parms.dir == TF_DIR_RX) ? "RX" : "TX");
		return rc;
	}

	/* Need to calculate the idx for the result record */
	/*
	 * TBD: Need to get the stride from tflib instead of having to
	 * understand the construction of the pointer
	 */
	uint64_t tmpidx = alloc_parms.idx;

	if (atbls->table_type == TF_TBL_TYPE_EXT)
		tmpidx = (alloc_parms.idx * TF_ACTION_RECORD_SZ) >> 4;
	else
		tmpidx = alloc_parms.idx;

	idx = tfp_cpu_to_be_64(tmpidx);

	/* Store the allocated index for future use in the regfile */
	rc = ulp_regfile_write(parms->regfile, atbls->regfile_wr_idx, idx);
	if (!rc) {
		BNXT_TF_DBG(ERR, "regfile[%d] write failed\n",
			    atbls->regfile_wr_idx);
		rc = -EINVAL;
		goto error;
	}

	/*
	 * The set_tbl_entry API if search is not enabled or searched entry
	 * is not found.
	 */
	if (!atbls->srch_b4_alloc || !alloc_parms.hit) {
		struct tf_set_tbl_entry_parms set_parm = { 0 };
		uint16_t	length;

		set_parm.dir	= atbls->direction;
		set_parm.type	= atbls->table_type;
		set_parm.idx	= alloc_parms.idx;
		set_parm.data	= ulp_blob_data_get(blob, &length);
		set_parm.data_sz_in_bytes = length / 8;

		if (set_parm.type == TF_TBL_TYPE_EXT)
			bnxt_ulp_cntxt_tbl_scope_id_get(parms->ulp_ctx,
							&set_parm.tbl_scope_id);
		else
			set_parm.tbl_scope_id = 0;

		/* set the table entry */
		rc = tf_set_tbl_entry(parms->tfp, &set_parm);
		if (rc) {
			BNXT_TF_DBG(ERR, "table[%d][%s][%d] set failed\n",
				    set_parm.type,
				    (set_parm.dir == TF_DIR_RX) ? "RX" : "TX",
				    set_parm.idx);
			goto error;
		}
	}

	/* Link the resource to the flow in the flow db */
	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.direction		= atbls->direction;
	fid_parms.resource_func		= atbls->resource_func;
	fid_parms.resource_type		= atbls->table_type;
	fid_parms.resource_hndl		= alloc_parms.idx;
	fid_parms.critical_resource	= 0;

	rc = ulp_flow_db_resource_add(parms->ulp_ctx,
				      parms->tbl_idx,
				      parms->fid,
				      &fid_parms);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to link resource to flow rc = %d\n",
			    rc);
		rc = -EINVAL;
		goto error;
	}

	return 0;
error:

	free_parms.dir	= alloc_parms.dir;
	free_parms.type	= alloc_parms.type;
	free_parms.idx	= alloc_parms.idx;

	trc = tf_free_tbl_entry(parms->tfp, &free_parms);
	if (trc)
		BNXT_TF_DBG(ERR, "Failed to free table entry on failure\n");

	return rc;
}

/*
 * Function to process the action Info. Iterate through the list
 * action info templates and process it.
 */
static int32_t
ulp_mapper_action_info_process(struct bnxt_ulp_mapper_parms *parms,
			       struct bnxt_ulp_mapper_act_tbl_info *tbl)
{
	struct ulp_blob					blob;
	struct bnxt_ulp_mapper_result_field_info	*flds, *fld;
	uint32_t					num_flds = 0;
	uint32_t					encap_flds = 0;
	uint32_t					i;
	int32_t						rc;
	uint16_t					bit_size;

	if (!tbl || !parms->act_prop || !parms->act_bitmap || !parms->regfile)
		return -EINVAL;

	/* use the max size if encap is enabled */
	if (tbl->encap_num_fields)
		bit_size = BNXT_ULP_FLMP_BLOB_SIZE_IN_BITS;
	else
		bit_size = tbl->result_bit_size;
	if (!ulp_blob_init(&blob, bit_size, parms->order)) {
		BNXT_TF_DBG(ERR, "action blob init failed\n");
		return -EINVAL;
	}

	flds = ulp_mapper_act_result_fields_get(tbl, &num_flds, &encap_flds);
	if (!flds || !num_flds) {
		BNXT_TF_DBG(ERR, "Template undefined for action\n");
		return -EINVAL;
	}

	for (i = 0; i < (num_flds + encap_flds); i++) {
		fld = &flds[i];
		rc = ulp_mapper_result_field_process(parms,
						     fld,
						     &blob);
		if (rc) {
			BNXT_TF_DBG(ERR, "Action field failed\n");
			return rc;
		}
		/* set the swap index if 64 bit swap is enabled */
		if (parms->encap_byte_swap && encap_flds) {
			if ((i + 1) == num_flds)
				ulp_blob_encap_swap_idx_set(&blob);
			/* if 64 bit swap is enabled perform the 64bit swap */
			if ((i + 1) == (num_flds + encap_flds))
				ulp_blob_perform_encap_swap(&blob);
		}
	}

	rc = ulp_mapper_action_alloc_and_set(parms, &blob);
	return rc;
}

/*
 * Function to process the action template. Iterate through the list
 * action info templates and process it.
 */
int32_t
ulp_mapper_action_tbls_process(struct bnxt_ulp_mapper_parms *parms)
{
	uint32_t	i;
	int32_t		rc = 0;

	if (!parms->atbls || !parms->num_atbls) {
		BNXT_TF_DBG(ERR, "No action tables for template[%d][%d].\n",
			    parms->dev_id, parms->act_tid);
		return -EINVAL;
	}

	for (i = 0; i < parms->num_atbls; i++) {
		rc = ulp_mapper_action_info_process(parms, &parms->atbls[i]);
		if (rc)
			return rc;
	}

	return rc;
}
