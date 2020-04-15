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

int32_t
ulp_mapper_class_tbls_process(struct bnxt_ulp_mapper_parms *parms);

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
 * ctxt [in] The ulp context
 *
 * tbl [in] A single table instance to get the key fields from
 *
 * num_flds [out] The number of key fields in the returned array
 *
 * returns array of Key fields, or NULL on error
 */
static struct bnxt_ulp_mapper_class_key_field_info *
ulp_mapper_key_fields_get(struct bnxt_ulp_mapper_class_tbl_info *tbl,
			  uint32_t *num_flds)
{
	uint32_t idx;

	if (!tbl || !num_flds)
		return NULL;

	idx		= tbl->key_start_idx;
	*num_flds	= tbl->key_num_fields;

	/* NOTE: Need template to provide range checking define */
	return &ulp_class_key_field_list[idx];
}

/*
 * Get the list of data fields that implement the flow.
 *
 * ctxt [in] The ulp context
 *
 * tbl [in] A single table instance to get the data fields from
 *
 * num_flds [out] The number of data fields in the returned array.
 *
 * Returns array of data fields, or NULL on error.
 */
static struct bnxt_ulp_mapper_result_field_info *
ulp_mapper_result_fields_get(struct bnxt_ulp_mapper_class_tbl_info *tbl,
			     uint32_t *num_flds)
{
	uint32_t idx;

	if (!tbl || !num_flds)
		return NULL;

	idx		= tbl->result_start_idx;
	*num_flds	= tbl->result_num_fields;

	/* NOTE: Need template to provide range checking define */
	return &ulp_class_result_field_list[idx];
}

/*
 * Get the list of result fields that implement the flow action.
 *
 * tbl [in] A single table instance to get the results fields
 * from num_flds [out] The number of data fields in the returned
 * array.
 *
 * Returns array of data fields, or NULL on error.
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

/*
 * Get the list of ident fields that implement the flow
 *
 * tbl [in] A single table instance to get the ident fields from
 *
 * num_flds [out] The number of ident fields in the returned array
 *
 * returns array of ident fields, or NULL on error
 */
static struct bnxt_ulp_mapper_ident_info *
ulp_mapper_ident_fields_get(struct bnxt_ulp_mapper_class_tbl_info *tbl,
			    uint32_t *num_flds)
{
	uint32_t idx;

	if (!tbl || !num_flds)
		return NULL;

	idx = tbl->ident_start_idx;
	*num_flds = tbl->ident_nums;

	/* NOTE: Need template to provide range checking define */
	return &ulp_ident_list[idx];
}

static inline int32_t
ulp_mapper_tcam_entry_free(struct bnxt_ulp_context *ulp  __rte_unused,
			   struct tf *tfp,
			   struct ulp_flow_db_res_params *res)
{
	struct tf_free_tcam_entry_parms fparms = {
		.dir		= res->direction,
		.tcam_tbl_type	= res->resource_type,
		.idx		= (uint16_t)res->resource_hndl
	};

	return tf_free_tcam_entry(tfp, &fparms);
}

static inline int32_t
ulp_mapper_index_entry_free(struct bnxt_ulp_context *ulp  __rte_unused,
			    struct tf *tfp,
			    struct ulp_flow_db_res_params *res)
{
	struct tf_free_tbl_entry_parms fparms = {
		.dir	= res->direction,
		.type	= res->resource_type,
		.idx	= (uint32_t)res->resource_hndl
	};

	return tf_free_tbl_entry(tfp, &fparms);
}

static inline int32_t
ulp_mapper_eem_entry_free(struct bnxt_ulp_context *ulp,
			  struct tf *tfp,
			  struct ulp_flow_db_res_params *res)
{
	struct tf_delete_em_entry_parms fparms = { 0 };
	int32_t rc;

	fparms.dir		= res->direction;
	fparms.mem		= TF_MEM_EXTERNAL;
	fparms.flow_handle	= res->resource_hndl;

	rc = bnxt_ulp_cntxt_tbl_scope_id_get(ulp, &fparms.tbl_scope_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to get table scope\n");
		return -EINVAL;
	}

	return tf_delete_em_entry(tfp, &fparms);
}

static inline int32_t
ulp_mapper_ident_free(struct bnxt_ulp_context *ulp __rte_unused,
		      struct tf *tfp,
		      struct ulp_flow_db_res_params *res)
{
	struct tf_free_identifier_parms fparms = {
		.dir		= res->direction,
		.ident_type	= res->resource_type,
		.id		= (uint16_t)res->resource_hndl
	};

	return tf_free_identifier(tfp, &fparms);
}

static inline int32_t
ulp_mapper_mark_free(struct bnxt_ulp_context *ulp,
		     struct ulp_flow_db_res_params *res)
{
	uint32_t flag;
	uint32_t fid;
	uint32_t gfid;

	fid	  = (uint32_t)res->resource_hndl;
	TF_GET_FLAG_FROM_FLOW_ID(fid, flag);
	TF_GET_GFID_FROM_FLOW_ID(fid, gfid);

	return ulp_mark_db_mark_del(ulp,
				    (flag == TF_GFID_TABLE_EXTERNAL),
				    gfid,
				    0);
}

static int32_t
ulp_mapper_ident_process(struct bnxt_ulp_mapper_parms *parms,
			 struct bnxt_ulp_mapper_class_tbl_info *tbl,
			 struct bnxt_ulp_mapper_ident_info *ident)
{
	struct ulp_flow_db_res_params	fid_parms;
	uint64_t id = 0;
	int32_t idx;
	struct tf_alloc_identifier_parms iparms = { 0 };
	struct tf_free_identifier_parms free_parms = { 0 };
	struct tf *tfp;
	int rc;

	tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx);
	if (!tfp) {
		BNXT_TF_DBG(ERR, "Failed to get tf pointer\n");
		return -EINVAL;
	}

	idx = ident->regfile_wr_idx;

	iparms.ident_type = ident->ident_type;
	iparms.dir = tbl->direction;

	rc = tf_alloc_identifier(tfp, &iparms);
	if (rc) {
		BNXT_TF_DBG(ERR, "Alloc ident %s:%d failed.\n",
			    (iparms.dir == TF_DIR_RX) ? "RX" : "TX",
			    iparms.ident_type);
		return rc;
	}

	id = (uint64_t)tfp_cpu_to_be_64(iparms.id);
	if (!ulp_regfile_write(parms->regfile, idx, id)) {
		BNXT_TF_DBG(ERR, "Regfile[%d] write failed.\n", idx);
		rc = -EINVAL;
		/* Need to free the identifier, so goto error */
		goto error;
	}

	/* Link the resource to the flow in the flow db */
	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.direction		= tbl->direction;
	fid_parms.resource_func	= ident->resource_func;
	fid_parms.resource_type	= ident->ident_type;
	fid_parms.resource_hndl	= iparms.id;
	fid_parms.critical_resource	= 0;

	rc = ulp_flow_db_resource_add(parms->ulp_ctx,
				      parms->tbl_idx,
				      parms->fid,
				      &fid_parms);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to link resource to flow rc = %d\n",
			    rc);
		/* Need to free the identifier, so goto error */
		goto error;
	}

	return 0;

error:
	/* Need to free the identifier */
	free_parms.dir		= tbl->direction;
	free_parms.ident_type	= ident->ident_type;
	free_parms.id		= iparms.id;

	(void)tf_free_identifier(tfp, &free_parms);

	BNXT_TF_DBG(ERR, "Ident process failed for %s:%s\n",
		    ident->name,
		    (tbl->direction == TF_DIR_RX) ? "RX" : "TX");
	return rc;
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
ulp_mapper_keymask_field_process(struct bnxt_ulp_mapper_parms *parms,
				 struct bnxt_ulp_mapper_class_key_field_info *f,
				 struct ulp_blob *blob,
				 uint8_t is_key)
{
	uint64_t regval;
	uint16_t idx, bitlen;
	uint32_t opcode;
	uint8_t *operand;
	struct ulp_regfile *regfile = parms->regfile;
	uint8_t *val = NULL;
	struct bnxt_ulp_mapper_class_key_field_info *fld = f;
	uint32_t field_size;

	if (is_key) {
		operand = fld->spec_operand;
		opcode	= fld->spec_opcode;
	} else {
		operand = fld->mask_operand;
		opcode	= fld->mask_opcode;
	}

	bitlen = fld->field_bit_size;

	switch (opcode) {
	case BNXT_ULP_SPEC_OPC_SET_TO_CONSTANT:
		val = operand;
		if (!ulp_blob_push(blob, val, bitlen)) {
			BNXT_TF_DBG(ERR, "push to key blob failed\n");
			return -EINVAL;
		}
		break;
	case BNXT_ULP_SPEC_OPC_ADD_PAD:
		if (!ulp_blob_pad_push(blob, bitlen)) {
			BNXT_TF_DBG(ERR, "Pad too large for blob\n");
			return -EINVAL;
		}

		break;
	case BNXT_ULP_SPEC_OPC_SET_TO_HDR_FIELD:
		if (!ulp_operand_read(operand, (uint8_t *)&idx,
				      sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "key operand read failed.\n");
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		if (is_key)
			val = parms->hdr_field[idx].spec;
		else
			val = parms->hdr_field[idx].mask;

		/*
		 * Need to account for how much data was pushed to the header
		 * field vs how much is to be inserted in the key/mask.
		 */
		field_size = parms->hdr_field[idx].size;
		if (bitlen < ULP_BYTE_2_BITS(field_size)) {
			field_size  = field_size - ((bitlen + 7) / 8);
			val += field_size;
		}

		if (!ulp_blob_push(blob, val, bitlen)) {
			BNXT_TF_DBG(ERR, "push to key blob failed\n");
			return -EINVAL;
		}
		break;
	case BNXT_ULP_SPEC_OPC_SET_TO_REGFILE:
		if (!ulp_operand_read(operand, (uint8_t *)&idx,
				      sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "key operand read failed.\n");
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);

		if (!ulp_regfile_read(regfile, idx, &regval)) {
			BNXT_TF_DBG(ERR, "regfile[%d] read failed.\n",
				    idx);
			return -EINVAL;
		}

		val = ulp_blob_push_64(blob, &regval, bitlen);
		if (!val) {
			BNXT_TF_DBG(ERR, "push to key blob failed\n");
			return -EINVAL;
		}
	default:
		break;
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

static int32_t
ulp_mapper_tcam_tbl_process(struct bnxt_ulp_mapper_parms *parms,
			    struct bnxt_ulp_mapper_class_tbl_info *tbl)
{
	struct bnxt_ulp_mapper_class_key_field_info	*kflds;
	struct ulp_blob key, mask, data;
	uint32_t i, num_kflds;
	struct tf *tfp;
	int32_t rc, trc;
	struct tf_alloc_tcam_entry_parms aparms		= { 0 };
	struct tf_set_tcam_entry_parms sparms		= { 0 };
	struct ulp_flow_db_res_params	fid_parms	= { 0 };
	struct tf_free_tcam_entry_parms free_parms	= { 0 };
	uint32_t hit = 0;
	uint16_t tmplen = 0;

	tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx);
	if (!tfp) {
		BNXT_TF_DBG(ERR, "Failed to get truflow pointer\n");
		return -EINVAL;
	}

	kflds = ulp_mapper_key_fields_get(tbl, &num_kflds);
	if (!kflds || !num_kflds) {
		BNXT_TF_DBG(ERR, "Failed to get key fields\n");
		return -EINVAL;
	}

	if (!ulp_blob_init(&key, tbl->key_bit_size, parms->order) ||
	    !ulp_blob_init(&mask, tbl->key_bit_size, parms->order) ||
	    !ulp_blob_init(&data, tbl->result_bit_size, parms->order)) {
		BNXT_TF_DBG(ERR, "blob inits failed.\n");
		return -EINVAL;
	}

	/* create the key/mask */
	/*
	 * NOTE: The WC table will require some kind of flag to handle the
	 * mode bits within the key/mask
	 */
	for (i = 0; i < num_kflds; i++) {
		/* Setup the key */
		rc = ulp_mapper_keymask_field_process(parms, &kflds[i],
						      &key, 1);
		if (rc) {
			BNXT_TF_DBG(ERR, "Key field set failed.\n");
			return rc;
		}

		/* Setup the mask */
		rc = ulp_mapper_keymask_field_process(parms, &kflds[i],
						      &mask, 0);
		if (rc) {
			BNXT_TF_DBG(ERR, "Mask field set failed.\n");
			return rc;
		}
	}

	aparms.dir		= tbl->direction;
	aparms.tcam_tbl_type	= tbl->table_type;
	aparms.search_enable	= tbl->srch_b4_alloc;
	aparms.key_sz_in_bits	= tbl->key_bit_size;
	aparms.key		= ulp_blob_data_get(&key, &tmplen);
	if (tbl->key_bit_size != tmplen) {
		BNXT_TF_DBG(ERR, "Key len (%d) != Expected (%d)\n",
			    tmplen, tbl->key_bit_size);
		return -EINVAL;
	}

	aparms.mask		= ulp_blob_data_get(&mask, &tmplen);
	if (tbl->key_bit_size != tmplen) {
		BNXT_TF_DBG(ERR, "Mask len (%d) != Expected (%d)\n",
			    tmplen, tbl->key_bit_size);
		return -EINVAL;
	}

	aparms.priority		= tbl->priority;

	/*
	 * All failures after this succeeds require the entry to be freed.
	 * cannot return directly on failure, but needs to goto error
	 */
	rc = tf_alloc_tcam_entry(tfp, &aparms);
	if (rc) {
		BNXT_TF_DBG(ERR, "tcam alloc failed rc=%d.\n", rc);
		return rc;
	}

	hit = aparms.hit;

	/* Build the result */
	if (!tbl->srch_b4_alloc || !hit) {
		struct bnxt_ulp_mapper_result_field_info *dflds;
		struct bnxt_ulp_mapper_ident_info *idents;
		uint32_t num_dflds, num_idents;

		/* Alloc identifiers */
		idents = ulp_mapper_ident_fields_get(tbl, &num_idents);

		for (i = 0; i < num_idents; i++) {
			rc = ulp_mapper_ident_process(parms, tbl, &idents[i]);

			/* Already logged the error, just return */
			if (rc)
				goto error;
		}

		/* Create the result data blob */
		dflds = ulp_mapper_result_fields_get(tbl, &num_dflds);
		if (!dflds || !num_dflds) {
			BNXT_TF_DBG(ERR, "Failed to get data fields.\n");
			rc = -EINVAL;
			goto error;
		}

		for (i = 0; i < num_dflds; i++) {
			rc = ulp_mapper_result_field_process(parms,
							     &dflds[i],
							     &data);
			if (rc) {
				BNXT_TF_DBG(ERR, "Failed to set data fields\n");
				goto error;
			}
		}

		sparms.dir		= aparms.dir;
		sparms.tcam_tbl_type	= aparms.tcam_tbl_type;
		sparms.idx		= aparms.idx;
		/* Already verified the key/mask lengths */
		sparms.key		= ulp_blob_data_get(&key, &tmplen);
		sparms.mask		= ulp_blob_data_get(&mask, &tmplen);
		sparms.key_sz_in_bits	= tbl->key_bit_size;
		sparms.result		= ulp_blob_data_get(&data, &tmplen);

		if (tbl->result_bit_size != tmplen) {
			BNXT_TF_DBG(ERR, "Result len (%d) != Expected (%d)\n",
				    tmplen, tbl->result_bit_size);
			rc = -EINVAL;
			goto error;
		}
		sparms.result_sz_in_bits = tbl->result_bit_size;

		rc = tf_set_tcam_entry(tfp, &sparms);
		if (rc) {
			BNXT_TF_DBG(ERR, "tcam[%d][%s][%d] write failed.\n",
				    sparms.tcam_tbl_type,
				    (sparms.dir == TF_DIR_RX) ? "RX" : "TX",
				    sparms.idx);
			goto error;
		}
	} else {
		BNXT_TF_DBG(ERR, "Not supporting search before alloc now\n");
		rc = -EINVAL;
		goto error;
	}

	/* Link the resource to the flow in the flow db */
	fid_parms.direction = tbl->direction;
	fid_parms.resource_func	= tbl->resource_func;
	fid_parms.resource_type	= tbl->table_type;
	fid_parms.critical_resource = tbl->critical_resource;
	fid_parms.resource_hndl	= aparms.idx;

	rc = ulp_flow_db_resource_add(parms->ulp_ctx,
				      parms->tbl_idx,
				      parms->fid,
				      &fid_parms);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to link resource to flow rc = %d\n",
			    rc);
		/* Need to free the identifier, so goto error */
		goto error;
	}

	return 0;
error:
	free_parms.dir			= tbl->direction;
	free_parms.tcam_tbl_type	= tbl->table_type;
	free_parms.idx			= aparms.idx;
	trc = tf_free_tcam_entry(tfp, &free_parms);
	if (trc)
		BNXT_TF_DBG(ERR, "Failed to free tcam[%d][%d][%d] on failure\n",
			    tbl->table_type, tbl->direction, aparms.idx);

	return rc;
}

static int32_t
ulp_mapper_em_tbl_process(struct bnxt_ulp_mapper_parms *parms,
			  struct bnxt_ulp_mapper_class_tbl_info *tbl)
{
	struct bnxt_ulp_mapper_class_key_field_info	*kflds;
	struct bnxt_ulp_mapper_result_field_info *dflds;
	struct ulp_blob key, data;
	uint32_t i, num_kflds, num_dflds;
	uint16_t tmplen;
	struct tf *tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx);
	struct ulp_rte_act_prop	 *a_prop = parms->act_prop;
	struct ulp_flow_db_res_params	fid_parms = { 0 };
	struct tf_insert_em_entry_parms iparms = { 0 };
	struct tf_delete_em_entry_parms free_parms = { 0 };
	int32_t	trc;
	int32_t rc = 0;

	kflds = ulp_mapper_key_fields_get(tbl, &num_kflds);
	if (!kflds || !num_kflds) {
		BNXT_TF_DBG(ERR, "Failed to get key fields\n");
		return -EINVAL;
	}

	/* Initialize the key/result blobs */
	if (!ulp_blob_init(&key, tbl->blob_key_bit_size, parms->order) ||
	    !ulp_blob_init(&data, tbl->result_bit_size, parms->order)) {
		BNXT_TF_DBG(ERR, "blob inits failed.\n");
		return -EINVAL;
	}

	/* create the key */
	for (i = 0; i < num_kflds; i++) {
		/* Setup the key */
		rc = ulp_mapper_keymask_field_process(parms, &kflds[i],
						      &key, 1);
		if (rc) {
			BNXT_TF_DBG(ERR, "Key field set failed.\n");
			return rc;
		}
	}

	/*
	 * TBD: Normally should process identifiers in case of using recycle or
	 * loopback.  Not supporting recycle for now.
	 */

	/* Create the result data blob */
	dflds = ulp_mapper_result_fields_get(tbl, &num_dflds);
	if (!dflds || !num_dflds) {
		BNXT_TF_DBG(ERR, "Failed to get data fields.\n");
		return -EINVAL;
	}

	for (i = 0; i < num_dflds; i++) {
		struct bnxt_ulp_mapper_result_field_info *fld;

		fld = &dflds[i];

		rc = ulp_mapper_result_field_process(parms,
						     fld,
						     &data);
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed to set data fields.\n");
			return rc;
		}
	}

	rc = bnxt_ulp_cntxt_tbl_scope_id_get(parms->ulp_ctx,
					     &iparms.tbl_scope_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to get table scope rc=%d\n", rc);
		return rc;
	}

	/*
	 * NOTE: the actual blob size will differ from the size in the tbl
	 * entry due to the padding.
	 */
	iparms.dup_check		= 0;
	iparms.dir			= tbl->direction;
	iparms.mem			= tbl->mem;
	iparms.key			= ulp_blob_data_get(&key, &tmplen);
	iparms.key_sz_in_bits		= tbl->key_bit_size;
	iparms.em_record		= ulp_blob_data_get(&data, &tmplen);
	iparms.em_record_sz_in_bits	= tbl->result_bit_size;

	rc = tf_insert_em_entry(tfp, &iparms);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to insert em entry rc=%d.\n", rc);
		return rc;
	}

	if (tbl->mark_enable &&
	    ULP_BITMAP_ISSET(parms->act_bitmap->bits,
			     BNXT_ULP_ACTION_BIT_MARK)) {
		uint32_t val, mark, gfid, flag;
		/* TBD: Need to determine if GFID is enabled globally */
		if (sizeof(val) != BNXT_ULP_ACT_PROP_SZ_MARK) {
			BNXT_TF_DBG(ERR, "Mark size (%d) != expected (%zu)\n",
				    BNXT_ULP_ACT_PROP_SZ_MARK, sizeof(val));
			rc = -EINVAL;
			goto error;
		}

		memcpy(&val,
		       &a_prop->act_details[BNXT_ULP_ACT_PROP_IDX_MARK],
		       sizeof(val));

		mark = tfp_be_to_cpu_32(val);

		TF_GET_GFID_FROM_FLOW_ID(iparms.flow_id, gfid);
		TF_GET_FLAG_FROM_FLOW_ID(iparms.flow_id, flag);

		rc = ulp_mark_db_mark_add(parms->ulp_ctx,
					  (flag == TF_GFID_TABLE_EXTERNAL),
					  gfid,
					  mark);
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed to add mark to flow\n");
			goto error;
		}

		/*
		 * Link the mark resource to the flow in the flow db
		 * The mark is never the critical resource, so it is 0.
		 */
		memset(&fid_parms, 0, sizeof(fid_parms));
		fid_parms.direction	= tbl->direction;
		fid_parms.resource_func	= BNXT_ULP_RESOURCE_FUNC_HW_FID;
		fid_parms.resource_type	= tbl->table_type;
		fid_parms.resource_hndl	= iparms.flow_id;
		fid_parms.critical_resource = 0;

		rc = ulp_flow_db_resource_add(parms->ulp_ctx,
					      parms->tbl_idx,
					      parms->fid,
					      &fid_parms);
		if (rc) {
			BNXT_TF_DBG(ERR, "Fail to link res to flow rc = %d\n",
				    rc);
			/* Need to free the identifier, so goto error */
			goto error;
		}
	}

	/* Link the EM resource to the flow in the flow db */
	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.direction		= tbl->direction;
	fid_parms.resource_func		= tbl->resource_func;
	fid_parms.resource_type		= tbl->table_type;
	fid_parms.critical_resource	= tbl->critical_resource;
	fid_parms.resource_hndl		= iparms.flow_handle;

	rc = ulp_flow_db_resource_add(parms->ulp_ctx,
				      parms->tbl_idx,
				      parms->fid,
				      &fid_parms);
	if (rc) {
		BNXT_TF_DBG(ERR, "Fail to link res to flow rc = %d\n",
			    rc);
		/* Need to free the identifier, so goto error */
		goto error;
	}

	return 0;
error:
	free_parms.dir		= iparms.dir;
	free_parms.mem		= iparms.mem;
	free_parms.tbl_scope_id	= iparms.tbl_scope_id;
	free_parms.flow_handle	= iparms.flow_handle;

	trc = tf_delete_em_entry(tfp, &free_parms);
	if (trc)
		BNXT_TF_DBG(ERR, "Failed to delete EM entry on failed add\n");

	return rc;
}

static int32_t
ulp_mapper_index_tbl_process(struct bnxt_ulp_mapper_parms *parms,
			     struct bnxt_ulp_mapper_class_tbl_info *tbl)
{
	struct bnxt_ulp_mapper_result_field_info *flds;
	struct ulp_flow_db_res_params	fid_parms;
	struct ulp_blob	data;
	uint64_t idx;
	uint16_t tmplen;
	uint32_t i, num_flds;
	int32_t rc = 0, trc = 0;
	struct tf_alloc_tbl_entry_parms	aparms = { 0 };
	struct tf_set_tbl_entry_parms	sparms = { 0 };
	struct tf_free_tbl_entry_parms	free_parms = { 0 };

	struct tf *tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx);

	if (!ulp_blob_init(&data, tbl->result_bit_size, parms->order)) {
		BNXT_TF_DBG(ERR, "Failed initial index table blob\n");
		return -EINVAL;
	}

	flds = ulp_mapper_result_fields_get(tbl, &num_flds);
	if (!flds || !num_flds) {
		BNXT_TF_DBG(ERR, "Template undefined for action\n");
		return -EINVAL;
	}

	for (i = 0; i < num_flds; i++) {
		rc = ulp_mapper_result_field_process(parms,
						     &flds[i],
						     &data);
		if (rc) {
			BNXT_TF_DBG(ERR, "data field failed\n");
			return rc;
		}
	}

	aparms.dir		= tbl->direction;
	aparms.type		= tbl->table_type;
	aparms.search_enable	= tbl->srch_b4_alloc;
	aparms.result		= ulp_blob_data_get(&data, &tmplen);
	aparms.result_sz_in_bytes = ULP_SZ_BITS2BYTES(tbl->result_bit_size);

	/* All failures after the alloc succeeds require a free */
	rc = tf_alloc_tbl_entry(tfp, &aparms);
	if (rc) {
		BNXT_TF_DBG(ERR, "Alloc table[%d][%s] failed rc=%d\n",
			    tbl->table_type,
			    (tbl->direction == TF_DIR_RX) ? "RX" : "TX",
			    rc);
		return rc;
	}

	/* Always storing values in Regfile in BE */
	idx = tfp_cpu_to_be_64(aparms.idx);
	rc = ulp_regfile_write(parms->regfile, tbl->regfile_wr_idx, idx);
	if (!rc) {
		BNXT_TF_DBG(ERR, "Write regfile[%d] failed\n",
			    tbl->regfile_wr_idx);
		goto error;
	}

	if (!tbl->srch_b4_alloc) {
		sparms.dir		= tbl->direction;
		sparms.type		= tbl->table_type;
		sparms.data		= ulp_blob_data_get(&data, &tmplen);
		sparms.data_sz_in_bytes =
			ULP_SZ_BITS2BYTES(tbl->result_bit_size);
		sparms.idx		= aparms.idx;

		rc = tf_set_tbl_entry(tfp, &sparms);
		if (rc) {
			BNXT_TF_DBG(ERR, "Set table[%d][%s][%d] failed rc=%d\n",
				    tbl->table_type,
				    (tbl->direction == TF_DIR_RX) ? "RX" : "TX",
				    sparms.idx,
				    rc);

			goto error;
		}
	}

	/* Link the resource to the flow in the flow db */
	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.direction	= tbl->direction;
	fid_parms.resource_func	= tbl->resource_func;
	fid_parms.resource_type	= tbl->table_type;
	fid_parms.resource_hndl	= aparms.idx;
	fid_parms.critical_resource	= 0;

	rc = ulp_flow_db_resource_add(parms->ulp_ctx,
				      parms->tbl_idx,
				      parms->fid,
				      &fid_parms);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to link resource to flow rc = %d\n",
			    rc);
		goto error;
	}

	return rc;
error:
	/*
	 * Free the allocated resource since we failed to either
	 * write to the entry or link the flow
	 */
	free_parms.dir	= tbl->direction;
	free_parms.type	= tbl->table_type;
	free_parms.idx	= aparms.idx;

	trc = tf_free_tbl_entry(tfp, &free_parms);
	if (trc)
		BNXT_TF_DBG(ERR, "Failed to free tbl entry on failure\n");

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

/* Create the classifier table entries for a flow. */
int32_t
ulp_mapper_class_tbls_process(struct bnxt_ulp_mapper_parms *parms)
{
	uint32_t	i;
	int32_t		rc = 0;

	if (!parms)
		return -EINVAL;

	if (!parms->ctbls || !parms->num_ctbls) {
		BNXT_TF_DBG(ERR, "No class tables for template[%d][%d].\n",
			    parms->dev_id, parms->class_tid);
		return -EINVAL;
	}

	for (i = 0; i < parms->num_ctbls; i++) {
		struct bnxt_ulp_mapper_class_tbl_info *tbl = &parms->ctbls[i];

		switch (tbl->resource_func) {
		case BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE:
			rc = ulp_mapper_tcam_tbl_process(parms, tbl);
			break;
		case BNXT_ULP_RESOURCE_FUNC_EM_TABLE:
			rc = ulp_mapper_em_tbl_process(parms, tbl);
			break;
		case BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE:
			rc = ulp_mapper_index_tbl_process(parms, tbl);
			break;
		default:
			BNXT_TF_DBG(ERR, "Unexpected class resource %d\n",
				    tbl->resource_func);
			return -EINVAL;
		}

		if (rc) {
			BNXT_TF_DBG(ERR, "Resource type %d failed\n",
				    tbl->resource_func);
			return rc;
		}
	}

	return rc;
}

static int32_t
ulp_mapper_resource_free(struct bnxt_ulp_context *ulp,
			 struct ulp_flow_db_res_params *res)
{
	struct tf *tfp;
	int32_t	rc = 0;

	if (!res || !ulp) {
		BNXT_TF_DBG(ERR, "Unable to free resource\n ");
		return -EINVAL;
	}

	tfp = bnxt_ulp_cntxt_tfp_get(ulp);
	if (!tfp) {
		BNXT_TF_DBG(ERR, "Unable to free resource failed to get tfp\n");
		return -EINVAL;
	}

	switch (res->resource_func) {
	case BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE:
		rc = ulp_mapper_tcam_entry_free(ulp, tfp, res);
		break;
	case BNXT_ULP_RESOURCE_FUNC_EM_TABLE:
		rc = ulp_mapper_eem_entry_free(ulp, tfp, res);
		break;
	case BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE:
		rc = ulp_mapper_index_entry_free(ulp, tfp, res);
		break;
	case BNXT_ULP_RESOURCE_FUNC_IDENTIFIER:
		rc = ulp_mapper_ident_free(ulp, tfp, res);
		break;
	case BNXT_ULP_RESOURCE_FUNC_HW_FID:
		rc = ulp_mapper_mark_free(ulp, res);
		break;
	default:
		break;
	}

	return rc;
}

int32_t
ulp_mapper_resources_free(struct bnxt_ulp_context	*ulp_ctx,
			  uint32_t fid,
			  enum bnxt_ulp_flow_db_tables	tbl_type)
{
	struct ulp_flow_db_res_params	res_parms = { 0 };
	int32_t				rc, trc;

	if (!ulp_ctx) {
		BNXT_TF_DBG(ERR, "Invalid parms, unable to free flow\n");
		return -EINVAL;
	}

	/*
	 * Set the critical resource on the first resource del, then iterate
	 * while status is good
	 */
	res_parms.critical_resource = 1;
	rc = ulp_flow_db_resource_del(ulp_ctx, tbl_type, fid, &res_parms);

	if (rc) {
		/*
		 * This is unexpected on the first call to resource del.
		 * It likely means that the flow did not exist in the flow db.
		 */
		BNXT_TF_DBG(ERR, "Flow[%d][0x%08x] failed to free (rc=%d)\n",
			    tbl_type, fid, rc);
		return rc;
	}

	while (!rc) {
		trc = ulp_mapper_resource_free(ulp_ctx, &res_parms);
		if (trc)
			/*
			 * On fail, we still need to attempt to free the
			 * remaining resources.  Don't return
			 */
			BNXT_TF_DBG(ERR,
				    "Flow[%d][0x%x] Res[%d][0x%016" PRIx64
				    "] failed rc=%d.\n",
				    tbl_type, fid, res_parms.resource_func,
				    res_parms.resource_hndl, trc);

		/* All subsequent call require the critical_resource be zero */
		res_parms.critical_resource = 0;

		rc = ulp_flow_db_resource_del(ulp_ctx,
					      tbl_type,
					      fid,
					      &res_parms);
	}

	/* Free the Flow ID since we've removed all resources */
	rc = ulp_flow_db_fid_free(ulp_ctx, tbl_type, fid);

	return rc;
}

int32_t
ulp_mapper_flow_destroy(struct bnxt_ulp_context	*ulp_ctx, uint32_t fid)
{
	if (!ulp_ctx) {
		BNXT_TF_DBG(ERR, "Invalid parms, unable to free flow\n");
		return -EINVAL;
	}

	return ulp_mapper_resources_free(ulp_ctx,
					 fid,
					 BNXT_ULP_REGULAR_FLOW_TABLE);
}
