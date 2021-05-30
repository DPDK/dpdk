/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#include <rte_log.h>
#include <rte_malloc.h>
#include "bnxt.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "bnxt_tf_common.h"
#include "ulp_utils.h"
#include "bnxt_ulp.h"
#include "tfp.h"
#include "tf_ext_flow_handle.h"
#include "ulp_mark_mgr.h"
#include "ulp_mapper.h"
#include "ulp_flow_db.h"
#include "tf_util.h"
#include "ulp_template_db_tbl.h"
#include "ulp_port_db.h"

static uint8_t mapper_fld_ones[16] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static const char *
ulp_mapper_tmpl_name_str(enum bnxt_ulp_template_type tmpl_type)
{
	switch (tmpl_type) {
	case BNXT_ULP_TEMPLATE_TYPE_CLASS:
		return "class";
	case BNXT_ULP_TEMPLATE_TYPE_ACTION:
		return "action";
	default:
		return "invalid template type";
	}
}

static struct bnxt_ulp_glb_resource_info *
ulp_mapper_glb_resource_info_list_get(uint32_t *num_entries)
{
	if (!num_entries)
		return NULL;
	*num_entries = BNXT_ULP_GLB_RESOURCE_TBL_MAX_SZ;
	return ulp_glb_resource_tbl;
}

/*
 * Read the global resource from the mapper global resource list
 *
 * The regval is always returned in big-endian.
 *
 * returns 0 on success
 */
static int32_t
ulp_mapper_glb_resource_read(struct bnxt_ulp_mapper_data *mapper_data,
			     enum tf_dir dir,
			     uint16_t idx,
			     uint64_t *regval)
{
	if (!mapper_data || !regval ||
	    dir >= TF_DIR_MAX || idx >= BNXT_ULP_GLB_RF_IDX_LAST)
		return -EINVAL;

	*regval = mapper_data->glb_res_tbl[dir][idx].resource_hndl;
	return 0;
}

/*
 * Write a global resource to the mapper global resource list
 *
 * The regval value must be in big-endian.
 *
 * return 0 on success.
 */
static int32_t
ulp_mapper_glb_resource_write(struct bnxt_ulp_mapper_data *data,
			      struct bnxt_ulp_glb_resource_info *res,
			      uint64_t regval)
{
	struct bnxt_ulp_mapper_glb_resource_entry *ent;

	/* validate the arguments */
	if (!data || res->direction >= TF_DIR_MAX ||
	    res->glb_regfile_index >= BNXT_ULP_GLB_RF_IDX_LAST)
		return -EINVAL;

	/* write to the mapper data */
	ent = &data->glb_res_tbl[res->direction][res->glb_regfile_index];
	ent->resource_func = res->resource_func;
	ent->resource_type = res->resource_type;
	ent->resource_hndl = regval;
	return 0;
}

/*
 * Internal function to allocate identity resource and store it in mapper data.
 *
 * returns 0 on success
 */
static int32_t
ulp_mapper_resource_ident_allocate(struct bnxt_ulp_context *ulp_ctx,
				   struct bnxt_ulp_mapper_data *mapper_data,
				   struct bnxt_ulp_glb_resource_info *glb_res)
{
	struct tf_alloc_identifier_parms iparms = { 0 };
	struct tf_free_identifier_parms fparms;
	uint64_t regval;
	struct tf *tfp;
	int32_t rc = 0;

	tfp = bnxt_ulp_cntxt_tfp_get(ulp_ctx);
	if (!tfp)
		return -EINVAL;

	iparms.ident_type = glb_res->resource_type;
	iparms.dir = glb_res->direction;

	/* Allocate the Identifier using tf api */
	rc = tf_alloc_identifier(tfp, &iparms);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to alloc identifier [%s][%d]\n",
			    tf_dir_2_str(iparms.dir),
			    iparms.ident_type);
		return rc;
	}

	/* entries are stored as big-endian format */
	regval = tfp_cpu_to_be_64((uint64_t)iparms.id);
	/* write to the mapper global resource */
	rc = ulp_mapper_glb_resource_write(mapper_data, glb_res, regval);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to write to global resource id\n");
		/* Free the identifier when update failed */
		fparms.dir = iparms.dir;
		fparms.ident_type = iparms.ident_type;
		fparms.id = iparms.id;
		tf_free_identifier(tfp, &fparms);
		return rc;
	}
	return rc;
}

/*
 * Internal function to allocate index tbl resource and store it in mapper data.
 *
 * returns 0 on success
 */
static int32_t
ulp_mapper_resource_index_tbl_alloc(struct bnxt_ulp_context *ulp_ctx,
				    struct bnxt_ulp_mapper_data *mapper_data,
				    struct bnxt_ulp_glb_resource_info *glb_res)
{
	struct tf_alloc_tbl_entry_parms	aparms = { 0 };
	struct tf_free_tbl_entry_parms	free_parms = { 0 };
	uint64_t regval;
	struct tf *tfp;
	uint32_t tbl_scope_id;
	int32_t rc = 0;

	tfp = bnxt_ulp_cntxt_tfp_get(ulp_ctx);
	if (!tfp)
		return -EINVAL;

	/* Get the scope id */
	rc = bnxt_ulp_cntxt_tbl_scope_id_get(ulp_ctx, &tbl_scope_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to get table scope rc=%d\n", rc);
		return rc;
	}

	aparms.type = glb_res->resource_type;
	aparms.dir = glb_res->direction;
	aparms.search_enable = 0;
	aparms.tbl_scope_id = tbl_scope_id;

	/* Allocate the index tbl using tf api */
	rc = tf_alloc_tbl_entry(tfp, &aparms);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to alloc identifier [%s][%d]\n",
			    tf_dir_2_str(aparms.dir), aparms.type);
		return rc;
	}

	/* entries are stored as big-endian format */
	regval = tfp_cpu_to_be_64((uint64_t)aparms.idx);
	/* write to the mapper global resource */
	rc = ulp_mapper_glb_resource_write(mapper_data, glb_res, regval);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to write to global resource id\n");
		/* Free the identifier when update failed */
		free_parms.dir = aparms.dir;
		free_parms.type = aparms.type;
		free_parms.idx = aparms.idx;
		tf_free_tbl_entry(tfp, &free_parms);
		return rc;
	}
	return rc;
}

/* Retrieve the global template table */
static uint32_t *
ulp_mapper_glb_template_table_get(uint32_t *num_entries)
{
	if (!num_entries)
		return NULL;
	*num_entries = BNXT_ULP_GLB_TEMPLATE_TBL_MAX_SZ;
	return ulp_glb_template_tbl;
}

static int32_t
ulp_mapper_glb_field_tbl_get(struct bnxt_ulp_mapper_parms *parms,
			     uint32_t operand,
			     uint8_t *val)
{
	uint32_t t_idx;

	t_idx = parms->class_tid << (BNXT_ULP_HDR_SIG_ID_SHIFT +
				     BNXT_ULP_GLB_FIELD_TBL_SHIFT);
	t_idx += ULP_COMP_FLD_IDX_RD(parms, BNXT_ULP_CF_IDX_HDR_SIG_ID) <<
		BNXT_ULP_GLB_FIELD_TBL_SHIFT;
	t_idx += operand;

	if (t_idx >= BNXT_ULP_GLB_FIELD_TBL_SIZE) {
		BNXT_TF_DBG(ERR, "Invalid hdr field index %x:%x:%x\n",
			    parms->class_tid, t_idx, operand);
		*val = 0;
		return -EINVAL; /* error */
	}
	*val = ulp_glb_field_tbl[t_idx];
	return 0;
}

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

static struct bnxt_ulp_mapper_cond_info *
ulp_mapper_tmpl_reject_list_get(struct bnxt_ulp_mapper_parms *mparms,
				uint32_t tid,
				uint32_t *num_tbls,
				enum bnxt_ulp_cond_list_opc *opc)
{
	uint32_t idx;
	const struct bnxt_ulp_template_device_tbls *dev_tbls;

	dev_tbls = &mparms->device_params->dev_tbls[mparms->tmpl_type];
	*num_tbls = dev_tbls->tmpl_list[tid].reject_info.cond_nums;
	*opc = dev_tbls->tmpl_list[tid].reject_info.cond_list_opcode;
	idx = dev_tbls->tmpl_list[tid].reject_info.cond_start_idx;

	return &dev_tbls->cond_list[idx];
}

static struct bnxt_ulp_mapper_cond_info *
ulp_mapper_tbl_execute_list_get(struct bnxt_ulp_mapper_parms *mparms,
				struct bnxt_ulp_mapper_tbl_info *tbl,
				uint32_t *num_tbls,
				enum bnxt_ulp_cond_list_opc *opc)
{
	uint32_t idx;
	const struct bnxt_ulp_template_device_tbls *dev_tbls;

	dev_tbls = &mparms->device_params->dev_tbls[mparms->tmpl_type];
	*num_tbls = tbl->execute_info.cond_nums;
	*opc = tbl->execute_info.cond_list_opcode;
	idx = tbl->execute_info.cond_start_idx;

	return &dev_tbls->cond_list[idx];
}

/*
 * Get a list of classifier tables that implement the flow
 * Gets a device dependent list of tables that implement the class template id
 *
 * mparms [in] The mappers parms with data related to the flow.
 *
 * tid [in] The template id that matches the flow
 *
 * num_tbls [out] The number of classifier tables in the returned array
 *
 * returns An array of classifier tables to implement the flow, or NULL on
 * error
 */
static struct bnxt_ulp_mapper_tbl_info *
ulp_mapper_tbl_list_get(struct bnxt_ulp_mapper_parms *mparms,
			uint32_t tid,
			uint32_t *num_tbls)
{
	uint32_t idx;
	const struct bnxt_ulp_template_device_tbls *dev_tbls;

	dev_tbls = &mparms->device_params->dev_tbls[mparms->tmpl_type];

	idx = dev_tbls->tmpl_list[tid].start_tbl_idx;
	*num_tbls = dev_tbls->tmpl_list[tid].num_tbls;

	return &dev_tbls->tbl_list[idx];
}

/*
 * Get the list of key fields that implement the flow.
 *
 * mparms [in] The mapper parms with information about the flow
 *
 * tbl [in] A single table instance to get the key fields from
 *
 * num_flds [out] The number of key fields in the returned array
 *
 * Returns array of Key fields, or NULL on error.
 */
static struct bnxt_ulp_mapper_key_info *
ulp_mapper_key_fields_get(struct bnxt_ulp_mapper_parms *mparms,
			  struct bnxt_ulp_mapper_tbl_info *tbl,
			  uint32_t *num_flds)
{
	uint32_t idx;
	const struct bnxt_ulp_template_device_tbls *dev_tbls;

	dev_tbls = &mparms->device_params->dev_tbls[mparms->tmpl_type];
	if (!dev_tbls->key_info_list) {
		*num_flds = 0;
		return NULL;
	}

	idx		= tbl->key_start_idx;
	*num_flds	= tbl->key_num_fields;

	return &dev_tbls->key_info_list[idx];
}

/*
 * Get the list of data fields that implement the flow.
 *
 * mparms [in] The mapper parms with information about the flow
 *
 * tbl [in] A single table instance to get the data fields from
 *
 * num_flds [out] The number of data fields in the returned array.
 *
 * num_encap_flds [out] The number of encap fields in the returned array.
 *
 * Returns array of data fields, or NULL on error.
 */
static struct bnxt_ulp_mapper_field_info *
ulp_mapper_result_fields_get(struct bnxt_ulp_mapper_parms *mparms,
			     struct bnxt_ulp_mapper_tbl_info *tbl,
			     uint32_t *num_flds,
			     uint32_t *num_encap_flds)
{
	uint32_t idx;
	const struct bnxt_ulp_template_device_tbls *dev_tbls;

	dev_tbls = &mparms->device_params->dev_tbls[mparms->tmpl_type];
	if (!dev_tbls->result_field_list) {
		*num_flds = 0;
		*num_encap_flds = 0;
		return NULL;
	}

	idx		= tbl->result_start_idx;
	*num_flds	= tbl->result_num_fields;
	*num_encap_flds = tbl->encap_num_fields;

	return &dev_tbls->result_field_list[idx];
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
ulp_mapper_ident_fields_get(struct bnxt_ulp_mapper_parms *mparms,
			    struct bnxt_ulp_mapper_tbl_info *tbl,
			    uint32_t *num_flds)
{
	uint32_t idx;
	const struct bnxt_ulp_template_device_tbls *dev_tbls;

	dev_tbls = &mparms->device_params->dev_tbls[mparms->tmpl_type];
	if (!dev_tbls->ident_list) {
		*num_flds = 0;
		return NULL;
	}

	idx = tbl->ident_start_idx;
	*num_flds = tbl->ident_nums;

	return &dev_tbls->ident_list[idx];
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
ulp_mapper_index_entry_free(struct bnxt_ulp_context *ulp,
			    struct tf *tfp,
			    struct ulp_flow_db_res_params *res)
{
	struct tf_free_tbl_entry_parms fparms = {
		.dir	= res->direction,
		.type	= res->resource_type,
		.idx	= (uint32_t)res->resource_hndl
	};

	/*
	 * Just get the table scope, it will be ignored if not necessary
	 * by the tf_free_tbl_entry
	 */
	(void)bnxt_ulp_cntxt_tbl_scope_id_get(ulp, &fparms.tbl_scope_id);

	return tf_free_tbl_entry(tfp, &fparms);
}

static inline int32_t
ulp_mapper_em_entry_free(struct bnxt_ulp_context *ulp,
			 struct tf *tfp,
			 struct ulp_flow_db_res_params *res)
{
	struct tf_delete_em_entry_parms fparms = { 0 };
	int32_t rc;

	fparms.dir		= res->direction;
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
	return ulp_mark_db_mark_del(ulp,
				    res->resource_type,
				    res->resource_hndl);
}

static inline int32_t
ulp_mapper_parent_flow_free(struct bnxt_ulp_context *ulp,
			    uint32_t parent_fid,
			    struct ulp_flow_db_res_params *res)
{
	uint32_t idx, child_fid = 0, parent_idx;
	struct bnxt_ulp_flow_db *flow_db;

	parent_idx = (uint32_t)res->resource_hndl;

	/* check the validity of the parent fid */
	if (ulp_flow_db_parent_flow_idx_get(ulp, parent_fid, &idx) ||
	    idx != parent_idx) {
		BNXT_TF_DBG(ERR, "invalid parent flow id %x\n", parent_fid);
		return -EINVAL;
	}

	/* Clear all the child flows parent index */
	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp);
	while (!ulp_flow_db_parent_child_flow_next_entry_get(flow_db, idx,
							     &child_fid)) {
		/* update the child flows resource handle */
		if (ulp_flow_db_child_flow_reset(ulp, BNXT_ULP_FDB_TYPE_REGULAR,
						 child_fid)) {
			BNXT_TF_DBG(ERR, "failed to reset child flow %x\n",
				    child_fid);
			return -EINVAL;
		}
	}

	/* free the parent entry in the parent table flow */
	if (ulp_flow_db_parent_flow_free(ulp, parent_fid)) {
		BNXT_TF_DBG(ERR, "failed to free parent flow %x\n", parent_fid);
		return -EINVAL;
	}
	return 0;
}

static inline int32_t
ulp_mapper_child_flow_free(struct bnxt_ulp_context *ulp,
			   uint32_t child_fid,
			   struct ulp_flow_db_res_params *res)
{
	uint32_t parent_fid;

	parent_fid = (uint32_t)res->resource_hndl;
	if (!parent_fid)
		return 0; /* Already freed - orphan child*/

	/* reset the child flow bitset*/
	if (ulp_flow_db_parent_child_flow_set(ulp, parent_fid, child_fid, 0)) {
		BNXT_TF_DBG(ERR, "error in resetting child flow bitset %x:%x\n",
			    parent_fid, child_fid);
		return -EINVAL;
	}
	return 0;
}

/*
 * Process the flow database opcode alloc action.
 * returns 0 on success
 */
static int32_t
ulp_mapper_fdb_opc_alloc_rid(struct bnxt_ulp_mapper_parms *parms,
			     struct bnxt_ulp_mapper_tbl_info *tbl)
{
	uint32_t rid = 0;
	uint64_t val64;
	int32_t rc = 0;

	/* allocate a new fid */
	rc = ulp_flow_db_fid_alloc(parms->ulp_ctx,
				   BNXT_ULP_FDB_TYPE_RID,
				   0, &rid);
	if (rc) {
		BNXT_TF_DBG(ERR,
			    "Unable to allocate flow table entry\n");
		return -EINVAL;
	}
	/* Store the allocated fid in regfile*/
	val64 = rid;
	rc = ulp_regfile_write(parms->regfile, tbl->fdb_operand,
			       tfp_cpu_to_be_64(val64));
	if (rc) {
		BNXT_TF_DBG(ERR, "Write regfile[%d] failed\n",
			    tbl->fdb_operand);
		ulp_flow_db_fid_free(parms->ulp_ctx,
				     BNXT_ULP_FDB_TYPE_RID, rid);
		return -EINVAL;
	}
	return 0;
}

/*
 * Process the flow database opcode action.
 * returns 0 on success.
 */
static int32_t
ulp_mapper_fdb_opc_process(struct bnxt_ulp_mapper_parms *parms,
			   struct bnxt_ulp_mapper_tbl_info *tbl,
			   struct ulp_flow_db_res_params *fid_parms)
{
	uint32_t push_fid;
	uint64_t val64;
	enum bnxt_ulp_fdb_type flow_type;
	int32_t rc = 0;

	switch (tbl->fdb_opcode) {
	case BNXT_ULP_FDB_OPC_PUSH_FID:
		push_fid = parms->fid;
		flow_type = parms->flow_type;
		break;
	case BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE:
		/* get the fid from the regfile */
		rc = ulp_regfile_read(parms->regfile, tbl->fdb_operand,
				      &val64);
		if (!rc) {
			BNXT_TF_DBG(ERR, "regfile[%d] read oob\n",
				    tbl->fdb_operand);
			return -EINVAL;
		}
		/* Use the extracted fid to update the flow resource */
		push_fid = (uint32_t)tfp_be_to_cpu_64(val64);
		flow_type = BNXT_ULP_FDB_TYPE_RID;
		break;
	default:
		return rc; /* Nothing to be done */
	}

	/* Add the resource to the flow database */
	rc = ulp_flow_db_resource_add(parms->ulp_ctx, flow_type,
				      push_fid, fid_parms);
	if (rc)
		BNXT_TF_DBG(ERR, "Failed to add res to flow %x rc = %d\n",
			    push_fid, rc);
	return rc;
}

/*
 * Process the flow database opcode action.
 * returns 0 on success.
 */
static int32_t
ulp_mapper_priority_opc_process(struct bnxt_ulp_mapper_parms *parms,
				struct bnxt_ulp_mapper_tbl_info *tbl,
				uint32_t *priority)
{
	int32_t rc = 0;

	switch (tbl->pri_opcode) {
	case BNXT_ULP_PRI_OPC_NOT_USED:
		*priority = 0;
		break;
	case BNXT_ULP_PRI_OPC_CONST:
		*priority = tbl->pri_operand;
		break;
	case BNXT_ULP_PRI_OPC_APP_PRI:
		*priority = parms->app_priority;
		break;
	default:
		BNXT_TF_DBG(ERR, "Priority opcode not supported %d\n",
			    tbl->pri_opcode);
		rc = -EINVAL;
		break;
	}
	return rc;
}

/*
 * Process the identifier list in the given table.
 * Extract the ident from the table entry and
 * write it to the reg file.
 * returns 0 on success.
 */
static int32_t
ulp_mapper_tbl_ident_scan_ext(struct bnxt_ulp_mapper_parms *parms,
			      struct bnxt_ulp_mapper_tbl_info *tbl,
			      uint8_t *byte_data,
			      uint32_t byte_data_size,
			      enum bnxt_ulp_byte_order byte_order)
{
	struct bnxt_ulp_mapper_ident_info *idents;
	uint32_t i, num_idents = 0;
	uint64_t val64;

	/* validate the null arguments */
	if (!byte_data) {
		BNXT_TF_DBG(ERR, "invalid argument\n");
		return -EINVAL;
	}

	/* Get the ident list and process each one */
	idents = ulp_mapper_ident_fields_get(parms, tbl, &num_idents);

	for (i = 0; i < num_idents; i++) {
		/* check the size of the buffer for validation */
		if ((idents[i].ident_bit_pos + idents[i].ident_bit_size) >
		    ULP_BYTE_2_BITS(byte_data_size) ||
		    idents[i].ident_bit_size > ULP_BYTE_2_BITS(sizeof(val64))) {
			BNXT_TF_DBG(ERR, "invalid offset or length %x:%x:%x\n",
				    idents[i].ident_bit_pos,
				    idents[i].ident_bit_size,
				    byte_data_size);
			return -EINVAL;
		}
		val64 = 0;
		if (byte_order == BNXT_ULP_BYTE_ORDER_LE)
			ulp_bs_pull_lsb(byte_data, (uint8_t *)&val64,
					sizeof(val64),
					idents[i].ident_bit_pos,
					idents[i].ident_bit_size);
		else
			ulp_bs_pull_msb(byte_data, (uint8_t *)&val64,
					idents[i].ident_bit_pos,
					idents[i].ident_bit_size);

		/* Write it to the regfile, val64 is already in big-endian*/
		if (ulp_regfile_write(parms->regfile,
				      idents[i].regfile_idx, val64)) {
			BNXT_TF_DBG(ERR, "Regfile[%d] write failed.\n",
				    idents[i].regfile_idx);
			return -EINVAL;
		}
	}
	return 0;
}

/*
 * Process the identifier instruction and either store it in the flow database
 * or return it in the val (if not NULL) on success.  If val is NULL, the
 * identifier is to be stored in the flow database.
 */
static int32_t
ulp_mapper_ident_process(struct bnxt_ulp_mapper_parms *parms,
			 struct bnxt_ulp_mapper_tbl_info *tbl,
			 struct bnxt_ulp_mapper_ident_info *ident,
			 uint16_t *val)
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

	idx = ident->regfile_idx;

	iparms.ident_type = ident->ident_type;
	iparms.dir = tbl->direction;

	rc = tf_alloc_identifier(tfp, &iparms);
	if (rc) {
		BNXT_TF_DBG(ERR, "Alloc ident %s:%s failed.\n",
			    tf_dir_2_str(iparms.dir),
			    tf_ident_2_str(iparms.ident_type));
		return rc;
	}

	id = (uint64_t)tfp_cpu_to_be_64(iparms.id);
	if (ulp_regfile_write(parms->regfile, idx, id)) {
		BNXT_TF_DBG(ERR, "Regfile[%d] write failed.\n", idx);
		rc = -EINVAL;
		/* Need to free the identifier, so goto error */
		goto error;
	}

	/* Link the resource to the flow in the flow db */
	if (!val) {
		memset(&fid_parms, 0, sizeof(fid_parms));
		fid_parms.direction		= tbl->direction;
		fid_parms.resource_func	= ident->resource_func;
		fid_parms.resource_type	= ident->ident_type;
		fid_parms.resource_hndl	= iparms.id;
		fid_parms.critical_resource = tbl->critical_resource;

		rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed to link res to flow rc = %d\n",
				    rc);
			/* Need to free the identifier, so goto error */
			goto error;
		}
	} else {
		*val = iparms.id;
	}
	return 0;

error:
	/* Need to free the identifier */
	free_parms.dir		= tbl->direction;
	free_parms.ident_type	= ident->ident_type;
	free_parms.id		= iparms.id;

	(void)tf_free_identifier(tfp, &free_parms);

	BNXT_TF_DBG(ERR, "Ident process failed for %s:%s\n",
		    ident->description,
		    tf_dir_2_str(tbl->direction));
	return rc;
}

/*
 * Process the identifier instruction and extract it from result blob.
 * Increment the identifier reference count and store it in the flow database.
 */
static int32_t
ulp_mapper_ident_extract(struct bnxt_ulp_mapper_parms *parms,
			 struct bnxt_ulp_mapper_tbl_info *tbl,
			 struct bnxt_ulp_mapper_ident_info *ident,
			 struct ulp_blob *res_blob)
{
	struct ulp_flow_db_res_params	fid_parms;
	uint64_t id = 0;
	uint32_t idx = 0;
	struct tf_search_identifier_parms sparms = { 0 };
	struct tf_free_identifier_parms free_parms = { 0 };
	struct tf *tfp;
	int rc;

	/* Get the tfp from ulp context */
	tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx);
	if (!tfp) {
		BNXT_TF_DBG(ERR, "Failed to get tf pointer\n");
		return -EINVAL;
	}

	/* Extract the index from the result blob */
	rc = ulp_blob_pull(res_blob, (uint8_t *)&idx, sizeof(idx),
			   ident->ident_bit_pos, ident->ident_bit_size);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to extract identifier from blob\n");
		return -EIO;
	}

	/* populate the search params and search identifier shadow table */
	sparms.ident_type = ident->ident_type;
	sparms.dir = tbl->direction;
	/* convert the idx into cpu format */
	sparms.search_id = tfp_be_to_cpu_32(idx);

	/* Search identifier also increase the reference count */
	rc = tf_search_identifier(tfp, &sparms);
	if (rc) {
		BNXT_TF_DBG(ERR, "Search ident %s:%s:%x failed.\n",
			    tf_dir_2_str(sparms.dir),
			    tf_ident_2_str(sparms.ident_type),
			    sparms.search_id);
		return rc;
	}

	/* Write it to the regfile */
	id = (uint64_t)tfp_cpu_to_be_64(sparms.search_id);
	if (ulp_regfile_write(parms->regfile, ident->regfile_idx, id)) {
		BNXT_TF_DBG(ERR, "Regfile[%d] write failed.\n", idx);
		rc = -EINVAL;
		/* Need to free the identifier, so goto error */
		goto error;
	}

	/* Link the resource to the flow in the flow db */
	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.direction = tbl->direction;
	fid_parms.resource_func = ident->resource_func;
	fid_parms.resource_type = ident->ident_type;
	fid_parms.resource_hndl = sparms.search_id;
	fid_parms.critical_resource = tbl->critical_resource;
	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to link res to flow rc = %d\n",
			    rc);
		/* Need to free the identifier, so goto error */
		goto error;
	}

	return 0;

error:
	/* Need to free the identifier */
	free_parms.dir = tbl->direction;
	free_parms.ident_type = ident->ident_type;
	free_parms.id = sparms.search_id;
	(void)tf_free_identifier(tfp, &free_parms);
	BNXT_TF_DBG(ERR, "Ident extract failed for %s:%s:%x\n",
		    ident->description,
		    tf_dir_2_str(tbl->direction), sparms.search_id);
	return rc;
}

static int32_t
ulp_mapper_field_port_db_process(struct bnxt_ulp_mapper_parms *parms,
				 struct bnxt_ulp_mapper_field_info *fld,
				 uint32_t port_id,
				 uint16_t val16,
				 uint8_t **val)
{
	enum bnxt_ulp_port_table port_data = val16;

	switch (port_data) {
	case BNXT_ULP_PORT_TABLE_DRV_FUNC_PARENT_MAC:
		if (ulp_port_db_parent_mac_addr_get(parms->ulp_ctx, port_id,
						    val)) {
			BNXT_TF_DBG(ERR, "Invalid port id %u\n", port_id);
			return -EINVAL;
		}
		break;
	default:
		BNXT_TF_DBG(ERR, "Invalid port_data %s\n", fld->description);
		return -EINVAL;
	}
	return 0;
}

static int32_t
ulp_mapper_field_process_inc_dec(struct bnxt_ulp_mapper_field_info *fld,
				 struct ulp_blob *blob,
				 uint64_t *val64,
				 uint16_t const_val16,
				 uint32_t bitlen,
				 uint32_t *update_flag)
{
	uint64_t l_val64 = *val64;

	if (fld->field_opc == BNXT_ULP_FIELD_OPC_SRC1_PLUS_CONST ||
	    fld->field_opc == BNXT_ULP_FIELD_OPC_SRC1_PLUS_CONST_POST) {
		l_val64 += const_val16;
		l_val64 = tfp_be_to_cpu_64(l_val64);
		ulp_blob_push_64(blob, &l_val64, bitlen);
	} else if (fld->field_opc == BNXT_ULP_FIELD_OPC_SRC1_MINUS_CONST ||
		   fld->field_opc == BNXT_ULP_FIELD_OPC_SRC1_MINUS_CONST_POST) {
		l_val64 -= const_val16;
		l_val64 = tfp_be_to_cpu_64(l_val64);
		ulp_blob_push_64(blob, &l_val64, bitlen);
	} else {
		BNXT_TF_DBG(ERR, "Invalid field opcode %u\n", fld->field_opc);
		return -EINVAL;
	}

	if (fld->field_opc == BNXT_ULP_FIELD_OPC_SRC1_MINUS_CONST_POST ||
	    fld->field_opc == BNXT_ULP_FIELD_OPC_SRC1_PLUS_CONST_POST) {
		*val64 = l_val64;
		*update_flag = 1;
	}
	return 0;
}

static int32_t
ulp_mapper_field_process(struct bnxt_ulp_mapper_parms *parms,
			 enum tf_dir dir,
			 struct bnxt_ulp_mapper_field_info *fld,
			 struct ulp_blob *blob,
			 uint8_t is_key,
			 const char *name)
{
	uint32_t val_size = 0, field_size = 0;
	uint64_t hdr_bit, act_bit, regval;
	uint16_t write_idx = blob->write_idx;
	uint16_t idx, size_idx, bitlen, offset;
	uint8_t	*val = NULL;
	uint8_t tmpval[16];
	uint8_t bit;
	uint32_t src1_sel = 0;
	enum bnxt_ulp_field_src fld_src;
	uint8_t *fld_src_oper;
	enum bnxt_ulp_field_cond_src field_cond_src;
	uint16_t const_val = 0;
	uint32_t update_flag = 0;
	uint64_t src1_val64;
	uint32_t port_id;

	/* process the field opcode */
	if (fld->field_opc != BNXT_ULP_FIELD_OPC_COND_OP) {
		field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE;
		/* Read the constant from the second operand */
		memcpy(&const_val, fld->field_opr2, sizeof(uint16_t));
		const_val = tfp_be_to_cpu_16(const_val);
	} else {
		field_cond_src = fld->field_cond_src;
	}

	bitlen = fld->field_bit_size;
	/* Evaluate the condition */
	switch (field_cond_src) {
	case BNXT_ULP_FIELD_COND_SRC_TRUE:
		src1_sel = 1;
		break;
	case BNXT_ULP_FIELD_COND_SRC_CF:
		if (!ulp_operand_read(fld->field_cond_opr,
				      (uint8_t *)&idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "%s operand read failed.\n", name);
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		if (idx >= BNXT_ULP_CF_IDX_LAST) {
			BNXT_TF_DBG(ERR, "%s invalid index %u\n", name, idx);
			return -EINVAL;
		}
		/* check if the computed field is set */
		if (ULP_COMP_FLD_IDX_RD(parms, idx))
			src1_sel = 1;
		break;
	case BNXT_ULP_FIELD_COND_SRC_RF:
		if (!ulp_operand_read(fld->field_cond_opr,
				      (uint8_t *)&idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "%s operand read failed\n", name);
			return -EINVAL;
		}

		idx = tfp_be_to_cpu_16(idx);
		/* Uninitialized regfile entries return 0 */
		if (!ulp_regfile_read(parms->regfile, idx, &regval)) {
			BNXT_TF_DBG(ERR, "%s regfile[%d] read oob\n",
				    name, idx);
			return -EINVAL;
		}
		if (regval)
			src1_sel = 1;
		break;
	case BNXT_ULP_FIELD_COND_SRC_ACT_BIT:
		if (!ulp_operand_read(fld->field_cond_opr,
				      (uint8_t *)&act_bit, sizeof(uint64_t))) {
			BNXT_TF_DBG(ERR, "%s operand read failed\n", name);
			return -EINVAL;
		}
		act_bit = tfp_be_to_cpu_64(act_bit);
		if (ULP_BITMAP_ISSET(parms->act_bitmap->bits, act_bit))
			src1_sel = 1;
		break;
	case BNXT_ULP_FIELD_COND_SRC_HDR_BIT:
		if (!ulp_operand_read(fld->field_cond_opr,
				      (uint8_t *)&hdr_bit, sizeof(uint64_t))) {
			BNXT_TF_DBG(ERR, "%s operand read failed\n", name);
			return -EINVAL;
		}
		hdr_bit = tfp_be_to_cpu_64(hdr_bit);
		if (ULP_BITMAP_ISSET(parms->hdr_bitmap->bits, hdr_bit))
			src1_sel = 1;
		break;
	case BNXT_ULP_FIELD_COND_SRC_FIELD_BIT:
		if (!ulp_operand_read(fld->field_cond_opr, (uint8_t *)&idx,
				      sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "%s operand read failed.\n", name);
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		/* get the index from the global field list */
		if (ulp_mapper_glb_field_tbl_get(parms, idx, &bit)) {
			BNXT_TF_DBG(ERR, "invalid ulp_glb_field_tbl idx %d\n",
				    idx);
			return -EINVAL;
		}
		if (bit && (ULP_INDEX_BITMAP_GET(parms->fld_bitmap->bits, bit)))
			src1_sel = 1;
		break;
	default:
		BNXT_TF_DBG(ERR, "%s invalid field opcode 0x%x at %d\n",
			    name, fld->field_cond_src, write_idx);
		return -EINVAL;
	}

	/* pick the selected source */
	if (src1_sel) {
		fld_src = fld->field_src1;
		fld_src_oper = fld->field_opr1;
	} else {
		fld_src = fld->field_src2;
		fld_src_oper = fld->field_opr2;
	}

	/* Perform the action */
	switch (fld_src) {
	case BNXT_ULP_FIELD_SRC_ZERO:
		if (ulp_blob_pad_push(blob, bitlen) < 0) {
			BNXT_TF_DBG(ERR, "%s too large for blob\n", name);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_FIELD_SRC_CONST:
		val = fld_src_oper;
		if (!ulp_blob_push(blob, val, bitlen)) {
			BNXT_TF_DBG(ERR, "%s push to blob failed\n", name);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_FIELD_SRC_ONES:
		val = mapper_fld_ones;
		if (!ulp_blob_push(blob, val, bitlen)) {
			BNXT_TF_DBG(ERR, "%s too large for blob\n", name);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_FIELD_SRC_CF:
		if (!ulp_operand_read(fld_src_oper,
				      (uint8_t *)&idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "%s operand read failed.\n",
				    name);
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		if (idx >= BNXT_ULP_CF_IDX_LAST) {
			BNXT_TF_DBG(ERR, "%s comp field [%d] read oob\n",
				    name, idx);
			return -EINVAL;
		}
		if (fld->field_opc == BNXT_ULP_FIELD_OPC_COND_OP) {
			val = ulp_blob_push_32(blob, &parms->comp_fld[idx],
					       bitlen);
			if (!val) {
				BNXT_TF_DBG(ERR, "%s push to blob failed\n",
					    name);
				return -EINVAL;
			}
		} else if (fld->field_opc == BNXT_ULP_FIELD_OPC_PORT_TABLE) {
			port_id = ULP_COMP_FLD_IDX_RD(parms, idx);
			if (ulp_mapper_field_port_db_process(parms, fld,
							     port_id, const_val,
							     &val)) {
				BNXT_TF_DBG(ERR, "%s field port table failed\n",
					    name);
				return -EINVAL;
			}
			if (!ulp_blob_push(blob, val, bitlen)) {
				BNXT_TF_DBG(ERR, "%s push to blob failed\n",
					    name);
				return -EINVAL;
			}
		} else {
			src1_val64 = ULP_COMP_FLD_IDX_RD(parms, idx);
			if (ulp_mapper_field_process_inc_dec(fld, blob,
							     &src1_val64,
							     const_val,
							     bitlen,
							     &update_flag)) {
				BNXT_TF_DBG(ERR, "%s field cond opc failed\n",
					    name);
				return -EINVAL;
			}
			if (update_flag) {
				BNXT_TF_DBG(ERR, "%s invalid field cond opc\n",
					    name);
				return -EINVAL;
			}
		}
		break;
	case BNXT_ULP_FIELD_SRC_RF:
		if (!ulp_operand_read(fld_src_oper,
				      (uint8_t *)&idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "%s operand read failed\n", name);
			return -EINVAL;
		}

		idx = tfp_be_to_cpu_16(idx);
		/* Uninitialized regfile entries return 0 */
		if (!ulp_regfile_read(parms->regfile, idx, &regval)) {
			BNXT_TF_DBG(ERR, "%s regfile[%d] read oob\n",
				    name, idx);
			return -EINVAL;
		}
		if (fld->field_opc == BNXT_ULP_FIELD_OPC_COND_OP) {
			val = ulp_blob_push_64(blob, &regval, bitlen);
			if (!val) {
				BNXT_TF_DBG(ERR, "%s push to blob failed\n",
					    name);
				return -EINVAL;
			}
		} else {
			if (ulp_mapper_field_process_inc_dec(fld, blob,
							     &regval,
							     const_val,
							     bitlen,
							     &update_flag)) {
				BNXT_TF_DBG(ERR, "%s field cond opc failed\n",
					    name);
				return -EINVAL;
			}
			if (update_flag) {
				regval = tfp_cpu_to_be_64(regval);
				if (ulp_regfile_write(parms->regfile, idx,
						      regval)) {
					BNXT_TF_DBG(ERR,
						    "Write regfile[%d] fail\n",
						    idx);
					return -EINVAL;
				}
			}
		}
		break;
	case BNXT_ULP_FIELD_SRC_ACT_PROP:
		if (!ulp_operand_read(fld_src_oper,
				      (uint8_t *)&idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "%s operand read failed\n", name);
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);

		if (idx >= BNXT_ULP_ACT_PROP_IDX_LAST) {
			BNXT_TF_DBG(ERR, "%s act_prop[%d] oob\n", name, idx);
			return -EINVAL;
		}
		val = &parms->act_prop->act_details[idx];
		field_size = ulp_mapper_act_prop_size_get(idx);
		if (bitlen < ULP_BYTE_2_BITS(field_size)) {
			field_size  = field_size - ((bitlen + 7) / 8);
			val += field_size;
		}
		if (!ulp_blob_push(blob, val, bitlen)) {
			BNXT_TF_DBG(ERR, "%s push to blob failed\n", name);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_FIELD_SRC_ACT_PROP_SZ:
		if (!ulp_operand_read(fld_src_oper,
				      (uint8_t *)&idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "%s operand read failed\n", name);
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);

		if (idx >= BNXT_ULP_ACT_PROP_IDX_LAST) {
			BNXT_TF_DBG(ERR, "%s act_prop[%d] oob\n", name, idx);
			return -EINVAL;
		}
		val = &parms->act_prop->act_details[idx];

		/* get the size index next */
		if (!ulp_operand_read(&fld_src_oper[sizeof(uint16_t)],
				      (uint8_t *)&size_idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "%s operand read failed\n", name);
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
	case BNXT_ULP_FIELD_SRC_GLB_RF:
		if (!ulp_operand_read(fld_src_oper,
				      (uint8_t *)&idx,
				      sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "%s operand read failed.\n", name);
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		if (ulp_mapper_glb_resource_read(parms->mapper_data,
						 dir,
						 idx, &regval)) {
			BNXT_TF_DBG(ERR, "%s global regfile[%d] read failed.\n",
				    name, idx);
			return -EINVAL;
		}
		if (fld->field_opc == BNXT_ULP_FIELD_OPC_COND_OP) {
			val = ulp_blob_push_64(blob, &regval, bitlen);
			if (!val) {
				BNXT_TF_DBG(ERR, "%s push to blob failed\n",
					    name);
				return -EINVAL;
			}
		} else {
			if (ulp_mapper_field_process_inc_dec(fld, blob,
							     &regval,
							     const_val,
							     bitlen,
							     &update_flag)) {
				BNXT_TF_DBG(ERR, "%s field cond opc failed\n",
					    name);
				return -EINVAL;
			}
			if (update_flag) {
				BNXT_TF_DBG(ERR, "%s invalid field cond opc\n",
					    name);
				return -EINVAL;
			}
		}
		break;
	case BNXT_ULP_FIELD_SRC_HF:
		if (!ulp_operand_read(fld_src_oper, (uint8_t *)&idx,
				      sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "%s operand read failed.\n", name);
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		/* get the index from the global field list */
		if (ulp_mapper_glb_field_tbl_get(parms, idx, &bit)) {
			BNXT_TF_DBG(ERR, "invalid ulp_glb_field_tbl idx %d\n",
				    idx);
			return -EINVAL;
		}
		if (is_key)
			val = parms->hdr_field[bit].spec;
		else
			val = parms->hdr_field[bit].mask;

		/*
		 * Need to account for how much data was pushed to the header
		 * field vs how much is to be inserted in the key/mask.
		 */
		field_size = parms->hdr_field[bit].size;
		if (bitlen < ULP_BYTE_2_BITS(field_size)) {
			field_size  = field_size - ((bitlen + 7) / 8);
			val += field_size;
		}

		if (!ulp_blob_push(blob, val, bitlen)) {
			BNXT_TF_DBG(ERR, "%s push to blob failed\n", name);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_FIELD_SRC_HDR_BIT:
		if (!ulp_operand_read(fld_src_oper,
				      (uint8_t *)&hdr_bit, sizeof(uint64_t))) {
			BNXT_TF_DBG(ERR, "%s operand read failed\n", name);
			return -EINVAL;
		}
		hdr_bit = tfp_be_to_cpu_64(hdr_bit);
		memset(tmpval, 0, sizeof(tmpval));
		if (ULP_BITMAP_ISSET(parms->hdr_bitmap->bits, hdr_bit))
			tmpval[0] = 1;
		if (bitlen > ULP_BYTE_2_BITS(sizeof(tmpval))) {
			BNXT_TF_DBG(ERR, "%s field size is incorrect\n", name);
			return -EINVAL;
		}
		if (!ulp_blob_push(blob, tmpval, bitlen)) {
			BNXT_TF_DBG(ERR, "%s push to blob failed\n", name);
			return -EINVAL;
		}
		val = tmpval;
		break;
	case BNXT_ULP_FIELD_SRC_ACT_BIT:
		if (!ulp_operand_read(fld_src_oper,
				      (uint8_t *)&act_bit, sizeof(uint64_t))) {
			BNXT_TF_DBG(ERR, "%s operand read failed\n", name);
			return -EINVAL;
		}
		act_bit = tfp_be_to_cpu_64(act_bit);
		memset(tmpval, 0, sizeof(tmpval));
		if (ULP_BITMAP_ISSET(parms->act_bitmap->bits, act_bit))
			tmpval[0] = 1;
		if (bitlen > ULP_BYTE_2_BITS(sizeof(tmpval))) {
			BNXT_TF_DBG(ERR, "%s field size is incorrect\n", name);
			return -EINVAL;
		}
		if (!ulp_blob_push(blob, tmpval, bitlen)) {
			BNXT_TF_DBG(ERR, "%s push to blob failed\n", name);
			return -EINVAL;
		}
		val = tmpval;
		break;
	case BNXT_ULP_FIELD_SRC_FIELD_BIT:
		if (!ulp_operand_read(fld_src_oper, (uint8_t *)&idx,
				      sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "%s operand read failed.\n", name);
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		/* get the index from the global field list */
		if (ulp_mapper_glb_field_tbl_get(parms, idx, &bit)) {
			BNXT_TF_DBG(ERR, "invalid ulp_glb_field_tbl idx %d\n",
				    idx);
			return -EINVAL;
		}
		memset(tmpval, 0, sizeof(tmpval));
		if (ULP_INDEX_BITMAP_GET(parms->fld_bitmap->bits, bit))
			tmpval[0] = 1;
		if (bitlen > ULP_BYTE_2_BITS(sizeof(tmpval))) {
			BNXT_TF_DBG(ERR, "%s field size is incorrect\n", name);
			return -EINVAL;
		}
		if (!ulp_blob_push(blob, tmpval, bitlen)) {
			BNXT_TF_DBG(ERR, "%s push to blob failed\n", name);
			return -EINVAL;
		}
		val = tmpval;
		break;
	case BNXT_ULP_FIELD_SRC_SKIP:
		/* do nothing */
		break;
	case BNXT_ULP_FIELD_SRC_REJECT:
		return -EINVAL;
	case BNXT_ULP_FIELD_SRC_SUB_HF:
		if (!ulp_operand_read(fld_src_oper,
				      (uint8_t *)&idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "%s operand read failed\n", name);
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		/* get the index from the global field list */
		if (ulp_mapper_glb_field_tbl_get(parms, idx, &bit)) {
			BNXT_TF_DBG(ERR, "invalid ulp_glb_field_tbl idx %d\n",
				    idx);
			return -EINVAL;
		}

		/* get the offset next */
		if (!ulp_operand_read(&fld_src_oper[sizeof(uint16_t)],
				      (uint8_t *)&offset, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "%s operand read failed\n", name);
			return -EINVAL;
		}
		offset = tfp_be_to_cpu_16(offset);
		if ((offset + bitlen) >
		    ULP_BYTE_2_BITS(parms->hdr_field[bit].size) ||
		    ULP_BITS_IS_BYTE_NOT_ALIGNED(offset)) {
			BNXT_TF_DBG(ERR, "Hdr field[%s] oob\n", name);
			return -EINVAL;
		}
		offset = ULP_BITS_2_BYTE_NR(offset);

		/* write the value into blob */
		if (is_key)
			val = &parms->hdr_field[bit].spec[offset];
		else
			val = &parms->hdr_field[bit].mask[offset];

		if (!ulp_blob_push(blob, val, bitlen)) {
			BNXT_TF_DBG(ERR, "%s push to blob failed\n", name);
			return -EINVAL;
		}
		break;
	default:
		BNXT_TF_DBG(ERR, "%s invalid field opcode 0x%x at %d\n",
			    name, fld_src, write_idx);
		return -EINVAL;
	}
	return 0;
}

/*
 * Result table process and fill the result blob.
 * data [out] - the result blob data
 */
static int32_t
ulp_mapper_tbl_result_build(struct bnxt_ulp_mapper_parms *parms,
			    struct bnxt_ulp_mapper_tbl_info *tbl,
			    struct ulp_blob *data,
			    const char *name)
{
	struct bnxt_ulp_mapper_field_info *dflds;
	uint32_t i, num_flds = 0, encap_flds = 0;
	int32_t rc = 0;

	/* Get the result field list */
	dflds = ulp_mapper_result_fields_get(parms, tbl, &num_flds,
					     &encap_flds);

	/* validate the result field list counts */
	if ((tbl->resource_func == BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE &&
	     (!num_flds && !encap_flds)) || !dflds ||
	    (tbl->resource_func != BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE &&
		(!num_flds || encap_flds))) {
		BNXT_TF_DBG(ERR, "Failed to get data fields %x:%x\n",
			    num_flds, encap_flds);
		return -EINVAL;
	}

	/* process the result fields, loop through them */
	for (i = 0; i < (num_flds + encap_flds); i++) {
		/* set the swap index if encap swap bit is enabled */
		if (parms->device_params->encap_byte_swap && encap_flds &&
		    i == num_flds)
			ulp_blob_encap_swap_idx_set(data);

		/* Process the result fields */
		rc = ulp_mapper_field_process(parms, tbl->direction,
					      &dflds[i], data, 0, name);
		if (rc) {
			BNXT_TF_DBG(ERR, "data field failed\n");
			return rc;
		}
	}

	/* if encap bit swap is enabled perform the bit swap */
	if (parms->device_params->encap_byte_swap && encap_flds)
		ulp_blob_perform_encap_swap(data);

	return rc;
}

static int32_t
ulp_mapper_mark_gfid_process(struct bnxt_ulp_mapper_parms *parms,
			     struct bnxt_ulp_mapper_tbl_info *tbl,
			     uint64_t flow_id)
{
	struct ulp_flow_db_res_params fid_parms;
	uint32_t mark, gfid, mark_flag;
	enum bnxt_ulp_mark_db_opc mark_op = tbl->mark_db_opcode;
	int32_t rc = 0;

	if (mark_op == BNXT_ULP_MARK_DB_OPC_NOP ||
	    !(mark_op == BNXT_ULP_MARK_DB_OPC_PUSH_IF_MARK_ACTION &&
	     ULP_BITMAP_ISSET(parms->act_bitmap->bits,
			      BNXT_ULP_ACT_BIT_MARK)))
		return rc; /* no need to perform gfid process */

	/* Get the mark id details from action property */
	memcpy(&mark, &parms->act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_MARK],
	       sizeof(mark));
	mark = tfp_be_to_cpu_32(mark);

	TF_GET_GFID_FROM_FLOW_ID(flow_id, gfid);
	mark_flag  = BNXT_ULP_MARK_GLOBAL_HW_FID;

	rc = ulp_mark_db_mark_add(parms->ulp_ctx, mark_flag,
				  gfid, mark);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to add mark to flow\n");
		return rc;
	}
	fid_parms.direction = tbl->direction;
	fid_parms.resource_func = BNXT_ULP_RESOURCE_FUNC_HW_FID;
	fid_parms.critical_resource = tbl->critical_resource;
	fid_parms.resource_type	= mark_flag;
	fid_parms.resource_hndl	= gfid;
	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (rc)
		BNXT_TF_DBG(ERR, "Fail to link res to flow rc = %d\n", rc);
	return rc;
}

static int32_t
ulp_mapper_mark_act_ptr_process(struct bnxt_ulp_mapper_parms *parms,
				struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct ulp_flow_db_res_params fid_parms;
	uint32_t act_idx, mark, mark_flag;
	uint64_t val64;
	enum bnxt_ulp_mark_db_opc mark_op = tbl->mark_db_opcode;
	int32_t rc = 0;

	if (mark_op == BNXT_ULP_MARK_DB_OPC_NOP ||
	    !(mark_op == BNXT_ULP_MARK_DB_OPC_PUSH_IF_MARK_ACTION &&
	     ULP_BITMAP_ISSET(parms->act_bitmap->bits,
			      BNXT_ULP_ACT_BIT_MARK)))
		return rc; /* no need to perform mark action process */

	/* Get the mark id details from action property */
	memcpy(&mark, &parms->act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_MARK],
	       sizeof(mark));
	mark = tfp_be_to_cpu_32(mark);

	if (!ulp_regfile_read(parms->regfile,
			      BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
			      &val64)) {
		BNXT_TF_DBG(ERR, "read action ptr main failed\n");
		return -EINVAL;
	}
	act_idx = tfp_be_to_cpu_64(val64);
	mark_flag  = BNXT_ULP_MARK_LOCAL_HW_FID;
	rc = ulp_mark_db_mark_add(parms->ulp_ctx, mark_flag,
				  act_idx, mark);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to add mark to flow\n");
		return rc;
	}
	fid_parms.direction = tbl->direction;
	fid_parms.resource_func = BNXT_ULP_RESOURCE_FUNC_HW_FID;
	fid_parms.critical_resource = tbl->critical_resource;
	fid_parms.resource_type	= mark_flag;
	fid_parms.resource_hndl	= act_idx;
	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (rc)
		BNXT_TF_DBG(ERR, "Fail to link res to flow rc = %d\n", rc);
	return rc;
}

static int32_t
ulp_mapper_mark_vfr_idx_process(struct bnxt_ulp_mapper_parms *parms,
				struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct ulp_flow_db_res_params fid_parms;
	uint32_t act_idx, mark, mark_flag;
	uint64_t val64;
	enum bnxt_ulp_mark_db_opc mark_op = tbl->mark_db_opcode;
	int32_t rc = 0;

	if (mark_op == BNXT_ULP_MARK_DB_OPC_NOP ||
	    mark_op == BNXT_ULP_MARK_DB_OPC_PUSH_IF_MARK_ACTION)
		return rc; /* no need to perform mark action process */

	/* Get the mark id details from the computed field of dev port id */
	mark = ULP_COMP_FLD_IDX_RD(parms, BNXT_ULP_CF_IDX_DEV_PORT_ID);

	 /* Get the main action pointer */
	if (!ulp_regfile_read(parms->regfile,
			      BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
			      &val64)) {
		BNXT_TF_DBG(ERR, "read action ptr main failed\n");
		return -EINVAL;
	}
	act_idx = tfp_be_to_cpu_64(val64);

	/* Set the mark flag to local fid and vfr flag */
	mark_flag  = BNXT_ULP_MARK_LOCAL_HW_FID | BNXT_ULP_MARK_VFR_ID;

	rc = ulp_mark_db_mark_add(parms->ulp_ctx, mark_flag,
				  act_idx, mark);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to add mark to flow\n");
		return rc;
	}
	fid_parms.direction = tbl->direction;
	fid_parms.resource_func = BNXT_ULP_RESOURCE_FUNC_HW_FID;
	fid_parms.critical_resource = tbl->critical_resource;
	fid_parms.resource_type	= mark_flag;
	fid_parms.resource_hndl	= act_idx;
	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (rc)
		BNXT_TF_DBG(ERR, "Fail to link res to flow rc = %d\n", rc);
	return rc;
}

/* Tcam table scan the identifier list and allocate each identifier */
static int32_t
ulp_mapper_tcam_tbl_scan_ident_alloc(struct bnxt_ulp_mapper_parms *parms,
				     struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct bnxt_ulp_mapper_ident_info *idents;
	uint32_t num_idents;
	uint32_t i;

	idents = ulp_mapper_ident_fields_get(parms, tbl, &num_idents);
	for (i = 0; i < num_idents; i++) {
		if (ulp_mapper_ident_process(parms, tbl,
					     &idents[i], NULL))
			return -EINVAL;
	}
	return 0;
}

/*
 * Tcam table scan the identifier list and extract the identifier from
 * the result blob.
 */
static int32_t
ulp_mapper_tcam_tbl_scan_ident_extract(struct bnxt_ulp_mapper_parms *parms,
				       struct bnxt_ulp_mapper_tbl_info *tbl,
				       struct ulp_blob *data)
{
	struct bnxt_ulp_mapper_ident_info *idents;
	uint32_t num_idents = 0, i;
	int32_t rc = 0;

	/*
	 * Extract the listed identifiers from the result field,
	 * no need to allocate them.
	 */
	idents = ulp_mapper_ident_fields_get(parms, tbl, &num_idents);
	for (i = 0; i < num_idents; i++) {
		rc = ulp_mapper_ident_extract(parms, tbl, &idents[i], data);
		if (rc) {
			BNXT_TF_DBG(ERR, "Error in identifier extraction\n");
			return rc;
		}
	}
	return rc;
}

/* Internal function to write the tcam entry */
static int32_t
ulp_mapper_tcam_tbl_entry_write(struct bnxt_ulp_mapper_parms *parms,
				struct bnxt_ulp_mapper_tbl_info *tbl,
				struct ulp_blob *key,
				struct ulp_blob *mask,
				struct ulp_blob *data,
				uint16_t idx)
{
	struct tf_set_tcam_entry_parms sparms = { 0 };
	struct tf *tfp;
	uint16_t tmplen;
	int32_t rc;

	tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx);
	if (!tfp) {
		BNXT_TF_DBG(ERR, "Failed to get truflow pointer\n");
		return -EINVAL;
	}

	sparms.dir		= tbl->direction;
	sparms.tcam_tbl_type	= tbl->resource_type;
	sparms.idx		= idx;
	/* Already verified the key/mask lengths */
	sparms.key		= ulp_blob_data_get(key, &tmplen);
	sparms.mask		= ulp_blob_data_get(mask, &tmplen);
	sparms.key_sz_in_bits	= tbl->key_bit_size;
	sparms.result		= ulp_blob_data_get(data, &tmplen);

	if (tbl->result_bit_size != tmplen) {
		BNXT_TF_DBG(ERR, "Result len (%d) != Expected (%d)\n",
			    tmplen, tbl->result_bit_size);
		return -EINVAL;
	}
	sparms.result_sz_in_bits = tbl->result_bit_size;
	if (tf_set_tcam_entry(tfp, &sparms)) {
		BNXT_TF_DBG(ERR, "tcam[%s][%s][%x] write failed.\n",
			    tf_tcam_tbl_2_str(sparms.tcam_tbl_type),
			    tf_dir_2_str(sparms.dir), sparms.idx);
		return -EIO;
	}

	/* Mark action */
	rc = ulp_mapper_mark_act_ptr_process(parms, tbl);
	if (rc) {
		BNXT_TF_DBG(ERR, "failed mark action processing\n");
		return rc;
	}

	return rc;
}

/* internal function to post process the key/mask blobs for wildcard tcam tbl */
static void ulp_mapper_wc_tcam_tbl_post_process(struct ulp_blob *blob)
{
	ulp_blob_perform_64B_word_swap(blob);
	ulp_blob_perform_64B_byte_swap(blob);
}

static int32_t
ulp_mapper_tcam_tbl_process(struct bnxt_ulp_mapper_parms *parms,
			    struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct bnxt_ulp_mapper_key_info	*kflds;
	struct ulp_blob key, mask, data, update_data;
	uint32_t i, num_kflds;
	struct tf *tfp;
	int32_t rc, trc;
	struct tf_alloc_tcam_entry_parms aparms		= { 0 };
	struct tf_search_tcam_entry_parms searchparms   = { 0 };
	struct ulp_flow_db_res_params	fid_parms	= { 0 };
	struct tf_free_tcam_entry_parms free_parms	= { 0 };
	uint32_t hit = 0;
	uint16_t tmplen = 0;
	uint16_t idx;

	/* Skip this if table opcode is NOP */
	if (tbl->tbl_opcode == BNXT_ULP_TCAM_TBL_OPC_NOT_USED ||
	    tbl->tbl_opcode >= BNXT_ULP_TCAM_TBL_OPC_LAST) {
		BNXT_TF_DBG(ERR, "Invalid tcam table opcode %d\n",
			    tbl->tbl_opcode);
		return 0;
	}

	tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx);
	if (!tfp) {
		BNXT_TF_DBG(ERR, "Failed to get truflow pointer\n");
		return -EINVAL;
	}

	kflds = ulp_mapper_key_fields_get(parms, tbl, &num_kflds);
	if (!kflds || !num_kflds) {
		BNXT_TF_DBG(ERR, "Failed to get key fields\n");
		return -EINVAL;
	}

	if (!ulp_blob_init(&key, tbl->blob_key_bit_size,
			   parms->device_params->byte_order) ||
	    !ulp_blob_init(&mask, tbl->blob_key_bit_size,
			   parms->device_params->byte_order) ||
	    !ulp_blob_init(&data, tbl->result_bit_size,
			   parms->device_params->byte_order) ||
	    !ulp_blob_init(&update_data, tbl->result_bit_size,
			   parms->device_params->byte_order)) {
		BNXT_TF_DBG(ERR, "blob inits failed.\n");
		return -EINVAL;
	}

	if (tbl->resource_type == TF_TCAM_TBL_TYPE_WC_TCAM) {
		key.byte_order = BNXT_ULP_BYTE_ORDER_BE;
		mask.byte_order = BNXT_ULP_BYTE_ORDER_BE;
	}

	/* create the key/mask */
	/*
	 * NOTE: The WC table will require some kind of flag to handle the
	 * mode bits within the key/mask
	 */
	for (i = 0; i < num_kflds; i++) {
		/* Setup the key */
		rc = ulp_mapper_field_process(parms, tbl->direction,
					      &kflds[i].field_info_spec,
					      &key, 1, "TCAM Key");
		if (rc) {
			BNXT_TF_DBG(ERR, "Key field set failed %s\n",
				    kflds[i].field_info_spec.description);
			return rc;
		}

		/* Setup the mask */
		rc = ulp_mapper_field_process(parms, tbl->direction,
					      &kflds[i].field_info_mask,
					      &mask, 0, "TCAM Mask");
		if (rc) {
			BNXT_TF_DBG(ERR, "Mask field set failed %s\n",
				    kflds[i].field_info_mask.description);
			return rc;
		}
	}

	/* For wild card tcam perform the post process to swap the blob */
	if (tbl->resource_type == TF_TCAM_TBL_TYPE_WC_TCAM) {
		ulp_mapper_wc_tcam_tbl_post_process(&key);
		ulp_mapper_wc_tcam_tbl_post_process(&mask);
	}

	if (tbl->tbl_opcode == BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE) {
		/* allocate the tcam index */
		aparms.dir = tbl->direction;
		aparms.tcam_tbl_type = tbl->resource_type;
		aparms.key = ulp_blob_data_get(&key, &tmplen);
		aparms.key_sz_in_bits = tmplen;
		if (tbl->blob_key_bit_size != tmplen) {
			BNXT_TF_DBG(ERR, "Key len (%d) != Expected (%d)\n",
				    tmplen, tbl->blob_key_bit_size);
			return -EINVAL;
		}

		aparms.mask = ulp_blob_data_get(&mask, &tmplen);
		if (tbl->blob_key_bit_size != tmplen) {
			BNXT_TF_DBG(ERR, "Mask len (%d) != Expected (%d)\n",
				    tmplen, tbl->blob_key_bit_size);
			return -EINVAL;
		}

		/* calculate the entry priority */
		rc = ulp_mapper_priority_opc_process(parms, tbl,
						     &aparms.priority);
		if (rc) {
			BNXT_TF_DBG(ERR, "entry priority process failed\n");
			return rc;
		}

		rc = tf_alloc_tcam_entry(tfp, &aparms);
		if (rc) {
			BNXT_TF_DBG(ERR, "tcam alloc failed rc=%d.\n", rc);
			return rc;
		}
		idx = aparms.idx;
		hit = aparms.hit;
	} else {
		/*
		 * Searching before allocation to see if we already have an
		 * entry.  This allows re-use of a constrained resource.
		 */
		searchparms.dir = tbl->direction;
		searchparms.tcam_tbl_type = tbl->resource_type;
		searchparms.key = ulp_blob_data_get(&key, &tmplen);
		searchparms.key_sz_in_bits = tbl->key_bit_size;
		searchparms.mask = ulp_blob_data_get(&mask, &tmplen);
		searchparms.alloc = 1;
		searchparms.result = ulp_blob_data_get(&data, &tmplen);
		searchparms.result_sz_in_bits = tbl->result_bit_size;

		/* calculate the entry priority */
		rc = ulp_mapper_priority_opc_process(parms, tbl,
						     &searchparms.priority);
		if (rc) {
			BNXT_TF_DBG(ERR, "entry priority process failed\n");
			return rc;
		}

		rc = tf_search_tcam_entry(tfp, &searchparms);
		if (rc) {
			BNXT_TF_DBG(ERR, "tcam search failed rc=%d\n", rc);
			return rc;
		}

		/* Successful search, check the result */
		if (searchparms.search_status == REJECT) {
			BNXT_TF_DBG(ERR, "tcam alloc rejected\n");
			return -ENOMEM;
		}
		idx = searchparms.idx;
		hit = searchparms.hit;
	}

	/* Write the tcam index into the regfile*/
	if (ulp_regfile_write(parms->regfile, tbl->tbl_operand,
			      (uint64_t)tfp_cpu_to_be_64(idx))) {
		BNXT_TF_DBG(ERR, "Regfile[%d] write failed.\n",
			    tbl->tbl_operand);
		rc = -EINVAL;
		/* Need to free the tcam idx, so goto error */
		goto error;
	}

	/* if it is miss then it is same as no search before alloc */
	if (!hit || tbl->tbl_opcode == BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE) {
		/*Scan identifier list, allocate identifier and update regfile*/
		rc = ulp_mapper_tcam_tbl_scan_ident_alloc(parms, tbl);
		/* Create the result blob */
		if (!rc)
			rc = ulp_mapper_tbl_result_build(parms, tbl, &data,
							 "TCAM Result");
		/* write the tcam entry */
		if (!rc)
			rc = ulp_mapper_tcam_tbl_entry_write(parms, tbl, &key,
							     &mask, &data, idx);
	} else {
		/*Scan identifier list, extract identifier and update regfile*/
		rc = ulp_mapper_tcam_tbl_scan_ident_extract(parms, tbl, &data);
	}
	if (rc)
		goto error;

	/* Add the tcam index to the flow database */
	fid_parms.direction = tbl->direction;
	fid_parms.resource_func	= tbl->resource_func;
	fid_parms.resource_type	= tbl->resource_type;
	fid_parms.critical_resource = tbl->critical_resource;
	fid_parms.resource_hndl	= idx;
	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to link resource to flow rc = %d\n",
			    rc);
		/* Need to free the identifier, so goto error */
		goto error;
	}

	return 0;
error:
	free_parms.dir			= tbl->direction;
	free_parms.tcam_tbl_type	= tbl->resource_type;
	free_parms.idx			= idx;
	trc = tf_free_tcam_entry(tfp, &free_parms);
	if (trc)
		BNXT_TF_DBG(ERR, "Failed to free tcam[%d][%d][%d] on failure\n",
			    tbl->resource_type, tbl->direction, idx);
	return rc;
}

static int32_t
ulp_mapper_em_tbl_process(struct bnxt_ulp_mapper_parms *parms,
			  struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct bnxt_ulp_mapper_key_info	*kflds;
	struct ulp_blob key, data;
	uint32_t i, num_kflds;
	uint16_t tmplen;
	struct tf *tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx);
	struct ulp_flow_db_res_params	fid_parms = { 0 };
	struct tf_insert_em_entry_parms iparms = { 0 };
	struct tf_delete_em_entry_parms free_parms = { 0 };
	enum bnxt_ulp_flow_mem_type mtype;
	int32_t	trc;
	int32_t rc = 0;

	rc = bnxt_ulp_cntxt_mem_type_get(parms->ulp_ctx, &mtype);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to get the mem type for EM\n");
		return -EINVAL;
	}

	kflds = ulp_mapper_key_fields_get(parms, tbl, &num_kflds);
	if (!kflds || !num_kflds) {
		BNXT_TF_DBG(ERR, "Failed to get key fields\n");
		return -EINVAL;
	}

	/* Initialize the key/result blobs */
	if (!ulp_blob_init(&key, tbl->blob_key_bit_size,
			   parms->device_params->byte_order) ||
	    !ulp_blob_init(&data, tbl->result_bit_size,
			   parms->device_params->byte_order)) {
		BNXT_TF_DBG(ERR, "blob inits failed.\n");
		return -EINVAL;
	}

	/* create the key */
	for (i = 0; i < num_kflds; i++) {
		/* Setup the key */
		rc = ulp_mapper_field_process(parms, tbl->direction,
					      &kflds[i].field_info_spec,
					      &key, 1, "EM Key");
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
	rc = ulp_mapper_tbl_result_build(parms, tbl, &data, "EM Result");
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to build the result blob\n");
		return rc;
	}
	/* do the transpose for the internal EM keys */
	if (tbl->resource_type == TF_MEM_INTERNAL)
		ulp_blob_perform_byte_reverse(&key);

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
	iparms.mem			= tbl->resource_type;
	iparms.key			= ulp_blob_data_get(&key, &tmplen);
	iparms.key_sz_in_bits		= tbl->key_bit_size;
	iparms.em_record		= ulp_blob_data_get(&data, &tmplen);
	iparms.em_record_sz_in_bits	= tbl->result_bit_size;

	rc = tf_insert_em_entry(tfp, &iparms);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to insert em entry rc=%d.\n", rc);
		return rc;
	}

	/* Mark action process */
	if (mtype == BNXT_ULP_FLOW_MEM_TYPE_EXT &&
	    tbl->resource_type == TF_MEM_EXTERNAL)
		rc = ulp_mapper_mark_gfid_process(parms, tbl, iparms.flow_id);
	else if (mtype == BNXT_ULP_FLOW_MEM_TYPE_INT &&
		 tbl->resource_type == TF_MEM_INTERNAL)
		rc = ulp_mapper_mark_act_ptr_process(parms, tbl);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to add mark to flow\n");
		goto error;
	}

	/* Link the EM resource to the flow in the flow db */
	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.direction		= tbl->direction;
	fid_parms.resource_func		= tbl->resource_func;
	fid_parms.resource_type		= tbl->resource_type;
	fid_parms.critical_resource	= tbl->critical_resource;
	fid_parms.resource_hndl		= iparms.flow_handle;

	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
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
			     struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct ulp_flow_db_res_params fid_parms;
	struct ulp_blob	data;
	uint64_t regval = 0;
	uint16_t tmplen;
	uint32_t index, hit;
	int32_t rc = 0, trc = 0;
	struct tf_alloc_tbl_entry_parms aparms = { 0 };
	struct tf_search_tbl_entry_parms srchparms = { 0 };
	struct tf_set_tbl_entry_parms sparms = { 0 };
	struct tf_get_tbl_entry_parms gparms = { 0 };
	struct tf_free_tbl_entry_parms free_parms = { 0 };
	uint32_t tbl_scope_id;
	struct tf *tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx);
	uint16_t bit_size;
	bool alloc = false;
	bool write = false;
	bool search = false;
	uint64_t act_rec_size;

	/* use the max size if encap is enabled */
	if (tbl->encap_num_fields)
		bit_size = BNXT_ULP_FLMP_BLOB_SIZE_IN_BITS;
	else
		bit_size = tbl->result_bit_size;

	/* Initialize the blob data */
	if (!ulp_blob_init(&data, bit_size,
			   parms->device_params->byte_order)) {
		BNXT_TF_DBG(ERR, "Failed to initialize index table blob\n");
		return -EINVAL;
	}

	/* Get the scope id first */
	rc = bnxt_ulp_cntxt_tbl_scope_id_get(parms->ulp_ctx, &tbl_scope_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to get table scope rc=%d\n", rc);
		return rc;
	}

	switch (tbl->tbl_opcode) {
	case BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE:
		alloc = true;
		break;
	case BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE:
		/*
		 * Build the entry, alloc an index, write the table, and store
		 * the data in the regfile.
		 */
		alloc = true;
		write = true;
		break;
	case BNXT_ULP_INDEX_TBL_OPC_SRCH_ALLOC_WR_REGFILE:
		if (tbl->resource_type == TF_TBL_TYPE_EXT) {
			/* Not currently supporting with EXT */
			BNXT_TF_DBG(ERR,
				    "Ext Table Search Opcode not supported.\n");
			return -EINVAL;
		}
		/*
		 * Search for the entry in the tf core.  If it is hit, save the
		 * index in the regfile.  If it is a miss, Build the entry,
		 * alloc an index, write the table, and store the data in the
		 * regfile (same as ALLOC_WR).
		 */
		search = true;
		break;
	case BNXT_ULP_INDEX_TBL_OPC_WR_REGFILE:
		/*
		 * get the index to write to from the regfile and then write
		 * the table entry.
		 */
		if (!ulp_regfile_read(parms->regfile,
				      tbl->tbl_operand,
				      &regval)) {
			BNXT_TF_DBG(ERR,
				    "Failed to get tbl idx from regfile[%d].\n",
				    tbl->tbl_operand);
			return -EINVAL;
		}
		index = tfp_be_to_cpu_64(regval);
		/* For external, we need to reverse shift */
		if (tbl->resource_type == TF_TBL_TYPE_EXT)
			index = TF_ACT_REC_PTR_2_OFFSET(index);

		write = true;
		break;
	case BNXT_ULP_INDEX_TBL_OPC_WR_GLB_REGFILE:
		if (tbl->fdb_opcode != BNXT_ULP_FDB_OPC_NOP) {
			BNXT_TF_DBG(ERR, "Template error, wrong fdb opcode\n");
			return -EINVAL;
		}
		/*
		 * get the index to write to from the global regfile and then
		 * write the table.
		 */
		if (ulp_mapper_glb_resource_read(parms->mapper_data,
						 tbl->direction,
						 tbl->tbl_operand,
						 &regval)) {
			BNXT_TF_DBG(ERR,
				    "Failed to get tbl idx from Global "
				    "regfile[%d].\n",
				    tbl->tbl_operand);
			return -EINVAL;
		}
		index = tfp_be_to_cpu_64(regval);
		/* For external, we need to reverse shift */
		if (tbl->resource_type == TF_TBL_TYPE_EXT)
			index = TF_ACT_REC_PTR_2_OFFSET(index);
		write = true;
		break;
	case BNXT_ULP_INDEX_TBL_OPC_RD_REGFILE:
		/*
		 * The read is different from the rest and can be handled here
		 * instead of trying to use common code.  Simply read the table
		 * with the index from the regfile, scan and store the
		 * identifiers, and return.
		 */
		if (tbl->resource_type == TF_TBL_TYPE_EXT) {
			/* Not currently supporting with EXT */
			BNXT_TF_DBG(ERR,
				    "Ext Table Read Opcode not supported.\n");
			return -EINVAL;
		}
		if (!ulp_regfile_read(parms->regfile,
				      tbl->tbl_operand, &regval)) {
			BNXT_TF_DBG(ERR,
				    "Failed to get tbl idx from regfile[%d]\n",
				    tbl->tbl_operand);
			return -EINVAL;
		}
		index = tfp_be_to_cpu_64(regval);
		gparms.dir = tbl->direction;
		gparms.type = tbl->resource_type;
		gparms.data = ulp_blob_data_get(&data, &tmplen);
		gparms.data_sz_in_bytes = ULP_BITS_2_BYTE(tbl->result_bit_size);
		gparms.idx = index;
		rc = tf_get_tbl_entry(tfp, &gparms);
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed to read the tbl entry %d:%d\n",
				    tbl->resource_type, index);
			return rc;
		}
		/*
		 * Scan the fields in the entry and push them into the regfile.
		 */
		rc = ulp_mapper_tbl_ident_scan_ext(parms, tbl,
						   gparms.data,
						   gparms.data_sz_in_bytes,
						   data.byte_order);
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed to read fields on tbl read "
				    "rc=%d\n", rc);
			return rc;
		}
		return 0;
	default:
		BNXT_TF_DBG(ERR, "Invalid index table opcode %d\n",
			    tbl->tbl_opcode);
		return -EINVAL;
	}

	if (write || search) {
		/* Get the result fields list */
		rc = ulp_mapper_tbl_result_build(parms,
						 tbl,
						 &data,
						 "Indexed Result");
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed to build the result blob\n");
			return rc;
		}
	}

	if (search) {
		/* Use the result blob to perform a search */
		memset(&srchparms, 0, sizeof(srchparms));
		srchparms.dir = tbl->direction;
		srchparms.type = tbl->resource_type;
		srchparms.alloc	= 1;
		srchparms.result = ulp_blob_data_get(&data, &tmplen);
		srchparms.result_sz_in_bytes = ULP_BITS_2_BYTE(tmplen);
		srchparms.tbl_scope_id = tbl_scope_id;
		rc = tf_search_tbl_entry(tfp, &srchparms);
		if (rc) {
			BNXT_TF_DBG(ERR, "Alloc table[%s][%s] failed rc=%d\n",
				    tf_tbl_type_2_str(tbl->resource_type),
				    tf_dir_2_str(tbl->direction), rc);
			return rc;
		}
		if (srchparms.search_status == REJECT) {
			BNXT_TF_DBG(ERR, "Alloc table[%s][%s] rejected.\n",
				    tf_tbl_type_2_str(tbl->resource_type),
				    tf_dir_2_str(tbl->direction));
			return -ENOMEM;
		}
		index = srchparms.idx;
		hit = srchparms.hit;
		if (hit)
			write = false;
		else
			write = true;
	}

	if (alloc) {
		aparms.dir		= tbl->direction;
		aparms.type		= tbl->resource_type;
		aparms.tbl_scope_id	= tbl_scope_id;

		/* All failures after the alloc succeeds require a free */
		rc = tf_alloc_tbl_entry(tfp, &aparms);
		if (rc) {
			BNXT_TF_DBG(ERR, "Alloc table[%s][%s] failed rc=%d\n",
				    tf_tbl_type_2_str(tbl->resource_type),
				    tf_dir_2_str(tbl->direction), rc);
			return rc;
		}
		index = aparms.idx;
	}

	if (search || alloc) {
		/*
		 * Store the index in the regfile since we either allocated it
		 * or it was a hit.
		 *
		 * Calculate the idx for the result record, for external EM the
		 * offset needs to be shifted accordingly.
		 * If external non-inline table types are used then need to
		 * revisit this logic.
		 */
		if (tbl->resource_type == TF_TBL_TYPE_EXT)
			regval = TF_ACT_REC_OFFSET_2_PTR(index);
		else
			regval = index;

		rc = ulp_regfile_write(parms->regfile,
				       tbl->tbl_operand,
				       tfp_cpu_to_be_64(regval));
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed to write regfile[%d] rc=%d\n",
				    tbl->tbl_operand, rc);
			goto error;
		}
	}

	if (write) {
		sparms.dir = tbl->direction;
		sparms.type = tbl->resource_type;
		sparms.data = ulp_blob_data_get(&data, &tmplen);
		sparms.data_sz_in_bytes = ULP_BITS_2_BYTE(tmplen);
		sparms.idx = index;
		sparms.tbl_scope_id = tbl_scope_id;
		rc = tf_set_tbl_entry(tfp, &sparms);
		if (rc) {
			BNXT_TF_DBG(ERR,
				    "Index table[%s][%s][%x] write failed "
				    "rc=%d\n",
				    tf_tbl_type_2_str(sparms.type),
				    tf_dir_2_str(sparms.dir),
				    sparms.idx, rc);
			goto error;
		}

		/* Calculate action record size */
		if (tbl->resource_type == TF_TBL_TYPE_EXT) {
			act_rec_size = (ULP_BITS_2_BYTE_NR(tmplen) + 15) / 16;
			act_rec_size--;
			if (ulp_regfile_write(parms->regfile,
					      BNXT_ULP_RF_IDX_ACTION_REC_SIZE,
					      tfp_cpu_to_be_64(act_rec_size)))
				BNXT_TF_DBG(ERR,
					    "Failed write the act rec size\n");
		}
	}

	/* Link the resource to the flow in the flow db */
	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.direction	= tbl->direction;
	fid_parms.resource_func	= tbl->resource_func;
	fid_parms.resource_type	= tbl->resource_type;
	fid_parms.resource_sub_type = tbl->resource_sub_type;
	fid_parms.resource_hndl	= index;
	fid_parms.critical_resource = tbl->critical_resource;

	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to link resource to flow rc = %d\n",
			    rc);
		goto error;
	}

	/* Perform the VF rep action */
	rc = ulp_mapper_mark_vfr_idx_process(parms, tbl);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to add vfr mark rc = %d\n", rc);
		goto error;
	}
	return rc;
error:
	/*
	 * Free the allocated resource since we failed to either
	 * write to the entry or link the flow
	 */
	free_parms.dir	= tbl->direction;
	free_parms.type	= tbl->resource_type;
	free_parms.idx	= index;
	free_parms.tbl_scope_id = tbl_scope_id;

	trc = tf_free_tbl_entry(tfp, &free_parms);
	if (trc)
		BNXT_TF_DBG(ERR, "Failed to free tbl entry on failure\n");

	return rc;
}

static int32_t
ulp_mapper_if_tbl_process(struct bnxt_ulp_mapper_parms *parms,
			  struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct ulp_blob	data, res_blob;
	uint64_t idx;
	uint16_t tmplen;
	int32_t rc = 0;
	struct tf_set_if_tbl_entry_parms iftbl_params = { 0 };
	struct tf_get_if_tbl_entry_parms get_parms = { 0 };
	struct tf *tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx);
	enum bnxt_ulp_if_tbl_opc if_opc = tbl->tbl_opcode;
	uint32_t res_size;

	/* Initialize the blob data */
	if (!ulp_blob_init(&data, tbl->result_bit_size,
			   parms->device_params->byte_order)) {
		BNXT_TF_DBG(ERR, "Failed initial index table blob\n");
		return -EINVAL;
	}

	/* create the result blob */
	rc = ulp_mapper_tbl_result_build(parms, tbl, &data, "IFtable Result");
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to build the result blob\n");
		return rc;
	}

	/* Get the index details */
	switch (if_opc) {
	case BNXT_ULP_IF_TBL_OPC_WR_COMP_FIELD:
		idx = ULP_COMP_FLD_IDX_RD(parms, tbl->tbl_operand);
		break;
	case BNXT_ULP_IF_TBL_OPC_WR_REGFILE:
		if (!ulp_regfile_read(parms->regfile, tbl->tbl_operand, &idx)) {
			BNXT_TF_DBG(ERR, "regfile[%d] read oob\n",
				    tbl->tbl_operand);
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_64(idx);
		break;
	case BNXT_ULP_IF_TBL_OPC_WR_CONST:
		idx = tbl->tbl_operand;
		break;
	case BNXT_ULP_IF_TBL_OPC_RD_COMP_FIELD:
		/* Initialize the result blob */
		if (!ulp_blob_init(&res_blob, tbl->result_bit_size,
				   parms->device_params->byte_order)) {
			BNXT_TF_DBG(ERR, "Failed initial result blob\n");
			return -EINVAL;
		}

		/* read the interface table */
		idx = ULP_COMP_FLD_IDX_RD(parms, tbl->tbl_operand);
		res_size = ULP_BITS_2_BYTE(tbl->result_bit_size);
		get_parms.dir = tbl->direction;
		get_parms.type = tbl->resource_type;
		get_parms.idx = idx;
		get_parms.data = ulp_blob_data_get(&res_blob, &tmplen);
		get_parms.data_sz_in_bytes = res_size;

		rc = tf_get_if_tbl_entry(tfp, &get_parms);
		if (rc) {
			BNXT_TF_DBG(ERR, "Get table[%d][%s][%x] failed rc=%d\n",
				    get_parms.type,
				    tf_dir_2_str(get_parms.dir),
				    get_parms.idx, rc);
			return rc;
		}
		rc = ulp_mapper_tbl_ident_scan_ext(parms, tbl,
						   res_blob.data,
						   res_size,
						   res_blob.byte_order);
		if (rc)
			BNXT_TF_DBG(ERR, "Scan and extract failed rc=%d\n", rc);
		return rc;
	case BNXT_ULP_IF_TBL_OPC_NOT_USED:
		return rc; /* skip it */
	default:
		BNXT_TF_DBG(ERR, "Invalid tbl index opcode\n");
		return -EINVAL;
	}

	/* Perform the tf table set by filling the set params */
	iftbl_params.dir = tbl->direction;
	iftbl_params.type = tbl->resource_type;
	iftbl_params.data = ulp_blob_data_get(&data, &tmplen);
	iftbl_params.data_sz_in_bytes = ULP_BITS_2_BYTE(tmplen);
	iftbl_params.idx = idx;

	rc = tf_set_if_tbl_entry(tfp, &iftbl_params);
	if (rc) {
		BNXT_TF_DBG(ERR, "Set table[%d][%s][%x] failed rc=%d\n",
			    iftbl_params.type,/* TBD: add tf_if_tbl_2_str */
			    tf_dir_2_str(iftbl_params.dir),
			    iftbl_params.idx, rc);
		return rc;
	}

	/*
	 * TBD: Need to look at the need to store idx in flow db for restore
	 * the table to its original state on deletion of this entry.
	 */
	return rc;
}

static int32_t
ulp_mapper_gen_tbl_process(struct bnxt_ulp_mapper_parms *parms,
			   struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct ulp_mapper_gen_tbl_list *gen_tbl_list;
	struct bnxt_ulp_mapper_key_info *kflds;
	struct ulp_flow_db_res_params fid_parms;
	struct ulp_mapper_gen_tbl_entry gen_tbl_ent, *g;
	struct ulp_gen_hash_entry_params hash_entry;
	uint16_t tmplen;
	struct ulp_blob key, data;
	uint8_t *cache_key;
	int32_t tbl_idx;
	uint32_t i, num_kflds = 0, key_index = 0;
	uint32_t gen_tbl_miss = 1, fdb_write = 0;
	uint8_t *byte_data;
	int32_t rc = 0;

	/* Get the key fields list and build the key. */
	kflds = ulp_mapper_key_fields_get(parms, tbl, &num_kflds);
	if (!kflds || !num_kflds) {
		BNXT_TF_DBG(ERR, "Failed to get key fields\n");
		return -EINVAL;
	}

	if (!ulp_blob_init(&key, tbl->key_bit_size,
			   parms->device_params->byte_order)) {
		BNXT_TF_DBG(ERR, "Failed to alloc blob\n");
		return -EINVAL;
	}
	for (i = 0; i < num_kflds; i++) {
		/* Setup the key */
		rc = ulp_mapper_field_process(parms, tbl->direction,
					      &kflds[i].field_info_spec,
					      &key, 1, "Gen Tbl Key");
		if (rc) {
			BNXT_TF_DBG(ERR,
				    "Failed to create key for Gen tbl rc=%d\n",
				    rc);
			return -EINVAL;
		}
	}

	/* Calculate the table index for the generic table*/
	tbl_idx = ulp_mapper_gen_tbl_idx_calculate(tbl->resource_sub_type,
						   tbl->direction);
	if (tbl_idx < 0) {
		BNXT_TF_DBG(ERR, "Invalid table index %x:%x\n",
			    tbl->resource_sub_type, tbl->direction);
		return -EINVAL;
	}

	/* The_key is a byte array convert it to a search index */
	cache_key = ulp_blob_data_get(&key, &tmplen);
	/* get the generic table  */
	gen_tbl_list = &parms->mapper_data->gen_tbl_list[tbl_idx];

	/* Check if generic hash table */
	if (gen_tbl_list->hash_tbl) {
		if (tbl->gen_tbl_lkup_type !=
		    BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH) {
			BNXT_TF_DBG(ERR, "%s: Invalid template lkup type\n",
				    gen_tbl_list->gen_tbl_name);
			return -EINVAL;
		}
		hash_entry.key_data = cache_key;
		hash_entry.key_length = ULP_BITS_2_BYTE(tmplen);
		rc = ulp_gen_hash_tbl_list_key_search(gen_tbl_list->hash_tbl,
						      &hash_entry);
		if (rc) {
			BNXT_TF_DBG(ERR, "%s: hash tbl search failed\n",
				    gen_tbl_list->gen_tbl_name);
			return rc;
		}
		if (hash_entry.search_flag == ULP_GEN_HASH_SEARCH_FOUND) {
			key_index = hash_entry.key_idx;
			/* Get the generic table entry */
			if (ulp_mapper_gen_tbl_entry_get(gen_tbl_list,
							 key_index,
							 &gen_tbl_ent))
				return -EINVAL;
			/* store the hash index in the fdb */
			key_index = hash_entry.hash_index;
		}
	} else {
		/* convert key to index directly */
		memcpy(&key_index, cache_key, ULP_BITS_2_BYTE(tmplen));
		/* Get the generic table entry */
		if (ulp_mapper_gen_tbl_entry_get(gen_tbl_list, key_index,
						 &gen_tbl_ent))
			return -EINVAL;
	}
	switch (tbl->tbl_opcode) {
	case BNXT_ULP_GENERIC_TBL_OPC_READ:
		if (gen_tbl_list->hash_tbl) {
			if (hash_entry.search_flag != ULP_GEN_HASH_SEARCH_FOUND)
				break; /* nothing to be done , no entry */
		}

		/* check the reference count */
		if (ULP_GEN_TBL_REF_CNT(&gen_tbl_ent)) {
			g = &gen_tbl_ent;
			/* Scan ident list and create the result blob*/
			rc = ulp_mapper_tbl_ident_scan_ext(parms, tbl,
							   g->byte_data,
							   g->byte_data_size,
							   g->byte_order);
			if (rc) {
				BNXT_TF_DBG(ERR,
					    "Failed to scan ident list\n");
				return -EINVAL;
			}
			if (tbl->fdb_opcode != BNXT_ULP_FDB_OPC_NOP) {
				/* increment the reference count */
				ULP_GEN_TBL_REF_CNT_INC(&gen_tbl_ent);
			}

			/* it is a hit */
			gen_tbl_miss = 0;
			fdb_write = 1;
		}
		break;
	case BNXT_ULP_GENERIC_TBL_OPC_WRITE:
		if (gen_tbl_list->hash_tbl) {
			rc = ulp_mapper_gen_tbl_hash_entry_add(gen_tbl_list,
							       &hash_entry,
							       &gen_tbl_ent);
			if (rc)
				return rc;
			/* store the hash index in the fdb */
			key_index = hash_entry.hash_index;
		}
		/* check the reference count */
		if (ULP_GEN_TBL_REF_CNT(&gen_tbl_ent)) {
			/* a hit then error */
			BNXT_TF_DBG(ERR, "generic entry already present\n");
			return -EINVAL; /* success */
		}

		/* Initialize the blob data */
		if (!ulp_blob_init(&data, tbl->result_bit_size,
				   gen_tbl_ent.byte_order)) {
			BNXT_TF_DBG(ERR, "Failed initial index table blob\n");
			return -EINVAL;
		}

		/* Get the result fields list */
		rc = ulp_mapper_tbl_result_build(parms, tbl, &data,
						 "Gen tbl Result");
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed to build the result blob\n");
			return rc;
		}
		byte_data = ulp_blob_data_get(&data, &tmplen);
		rc = ulp_mapper_gen_tbl_entry_data_set(&gen_tbl_ent,
						       tmplen, byte_data,
						       ULP_BITS_2_BYTE(tmplen));
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed to write generic table\n");
			return -EINVAL;
		}

		/* increment the reference count */
		ULP_GEN_TBL_REF_CNT_INC(&gen_tbl_ent);
		fdb_write = 1;
		parms->shared_hndl = (uint64_t)tbl_idx << 32 | key_index;
		break;
	default:
		BNXT_TF_DBG(ERR, "Invalid table opcode %x\n", tbl->tbl_opcode);
		return -EINVAL;
	}

	/* Set the generic entry hit */
	rc = ulp_regfile_write(parms->regfile,
			       BNXT_ULP_RF_IDX_GENERIC_TBL_MISS,
			       tfp_cpu_to_be_64(gen_tbl_miss));
	if (rc) {
		BNXT_TF_DBG(ERR, "Write regfile[%d] failed\n",
			    BNXT_ULP_RF_IDX_GENERIC_TBL_MISS);
		return -EIO;
	}

	/* add the entry to the flow database */
	if (fdb_write) {
		memset(&fid_parms, 0, sizeof(fid_parms));
		fid_parms.direction = tbl->direction;
		fid_parms.resource_func	= tbl->resource_func;
		fid_parms.resource_sub_type = tbl->resource_sub_type;
		fid_parms.resource_hndl	= key_index;
		fid_parms.critical_resource = tbl->critical_resource;
		rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
		if (rc)
			BNXT_TF_DBG(ERR, "Fail to add gen ent flowdb %d\n", rc);
	}
	return rc;
}

static int32_t
ulp_mapper_ctrl_tbl_process(struct bnxt_ulp_mapper_parms *parms,
			    struct bnxt_ulp_mapper_tbl_info *tbl)
{
	int32_t rc = 0;

	/* process the fdb opcode for alloc push */
	if (tbl->fdb_opcode == BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE) {
		rc = ulp_mapper_fdb_opc_alloc_rid(parms, tbl);
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed to do fdb alloc\n");
			return rc;
		}
	}
	return rc;
}

static int32_t
ulp_mapper_glb_resource_info_init(struct bnxt_ulp_context *ulp_ctx,
				  struct bnxt_ulp_mapper_data *mapper_data)
{
	struct bnxt_ulp_glb_resource_info *glb_res;
	uint32_t num_glb_res_ids, idx;
	int32_t rc = 0;

	glb_res = ulp_mapper_glb_resource_info_list_get(&num_glb_res_ids);
	if (!glb_res || !num_glb_res_ids) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}

	/* Iterate the global resources and process each one */
	for (idx = 0; idx < num_glb_res_ids; idx++) {
		switch (glb_res[idx].resource_func) {
		case BNXT_ULP_RESOURCE_FUNC_IDENTIFIER:
			rc = ulp_mapper_resource_ident_allocate(ulp_ctx,
								mapper_data,
								&glb_res[idx]);
			break;
		case BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE:
			rc = ulp_mapper_resource_index_tbl_alloc(ulp_ctx,
								 mapper_data,
								 &glb_res[idx]);
			break;
		default:
			BNXT_TF_DBG(ERR, "Global resource %x not supported\n",
				    glb_res[idx].resource_func);
			rc = -EINVAL;
			break;
		}
		if (rc)
			return rc;
	}
	return rc;
}

/*
 * Function to process the memtype opcode of the mapper table.
 * returns 1 to skip the table.
 * return 0 to continue processing the table.
 *
 * defaults to skip
 */
static int32_t
ulp_mapper_tbl_memtype_opcode_process(struct bnxt_ulp_mapper_parms *parms,
				      struct bnxt_ulp_mapper_tbl_info *tbl)
{
	enum bnxt_ulp_flow_mem_type mtype = BNXT_ULP_FLOW_MEM_TYPE_INT;
	int32_t rc = 1;

	if (bnxt_ulp_cntxt_mem_type_get(parms->ulp_ctx, &mtype)) {
		BNXT_TF_DBG(ERR, "Failed to get the mem type\n");
		return rc;
	}

	switch (tbl->mem_type_opcode) {
	case BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_INT:
		if (mtype == BNXT_ULP_FLOW_MEM_TYPE_INT)
			rc = 0;
		break;
	case BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_EXT:
		if (mtype == BNXT_ULP_FLOW_MEM_TYPE_EXT)
			rc = 0;
		break;
	case BNXT_ULP_MEM_TYPE_OPC_NOP:
		rc = 0;
		break;
	default:
		BNXT_TF_DBG(ERR,
			    "Invalid arg in mapper in memtype opcode\n");
		break;
	}
	return rc;
}

/*
 * Common conditional opcode process routine that is used for both the template
 * rejection and table conditional execution.
 */
static int32_t
ulp_mapper_cond_opc_process(struct bnxt_ulp_mapper_parms *parms,
			    enum bnxt_ulp_cond_opc opc,
			    uint32_t operand,
			    int32_t *res)
{
	int32_t rc = 0;
	uint8_t bit;
	uint64_t regval;

	switch (opc) {
	case BNXT_ULP_COND_OPC_CF_IS_SET:
		if (operand < BNXT_ULP_CF_IDX_LAST) {
			*res = ULP_COMP_FLD_IDX_RD(parms, operand);
		} else {
			BNXT_TF_DBG(ERR, "comp field out of bounds %d\n",
				    operand);
			rc = -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_CF_NOT_SET:
		if (operand < BNXT_ULP_CF_IDX_LAST) {
			*res = !ULP_COMP_FLD_IDX_RD(parms, operand);
		} else {
			BNXT_TF_DBG(ERR, "comp field out of bounds %d\n",
				    operand);
			rc = -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_ACT_BIT_IS_SET:
		if (operand < BNXT_ULP_ACT_BIT_LAST) {
			*res = ULP_BITMAP_ISSET(parms->act_bitmap->bits,
						operand);
		} else {
			BNXT_TF_DBG(ERR, "action bit out of bounds %d\n",
				    operand);
			rc = -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_ACT_BIT_NOT_SET:
		if (operand < BNXT_ULP_ACT_BIT_LAST) {
			*res = !ULP_BITMAP_ISSET(parms->act_bitmap->bits,
					       operand);
		} else {
			BNXT_TF_DBG(ERR, "action bit out of bounds %d\n",
				    operand);
			rc = -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_HDR_BIT_IS_SET:
		if (operand < BNXT_ULP_HDR_BIT_LAST) {
			*res = ULP_BITMAP_ISSET(parms->hdr_bitmap->bits,
						operand);
		} else {
			BNXT_TF_DBG(ERR, "header bit out of bounds %d\n",
				    operand);
			rc = -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_HDR_BIT_NOT_SET:
		if (operand < BNXT_ULP_HDR_BIT_LAST) {
			*res = !ULP_BITMAP_ISSET(parms->hdr_bitmap->bits,
					       operand);
		} else {
			BNXT_TF_DBG(ERR, "header bit out of bounds %d\n",
				    operand);
			rc = -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_FIELD_BIT_IS_SET:
		rc = ulp_mapper_glb_field_tbl_get(parms, operand, &bit);
		if (rc) {
			BNXT_TF_DBG(ERR, "invalid ulp_glb_field_tbl idx %d\n",
				    operand);
			return -EINVAL;
		}
		*res = ULP_INDEX_BITMAP_GET(parms->fld_bitmap->bits, bit);
		break;
	case BNXT_ULP_COND_OPC_FIELD_BIT_NOT_SET:
		rc = ulp_mapper_glb_field_tbl_get(parms, operand, &bit);
		if (rc) {
			BNXT_TF_DBG(ERR, "invalid ulp_glb_field_tbl idx %d\n",
				    operand);
			return -EINVAL;
		}
		*res = !ULP_INDEX_BITMAP_GET(parms->fld_bitmap->bits, bit);
		break;
	case BNXT_ULP_COND_OPC_RF_IS_SET:
		if (!ulp_regfile_read(parms->regfile, operand, &regval)) {
			BNXT_TF_DBG(ERR, "regfile[%d] read oob\n", operand);
			return -EINVAL;
		}
		*res = regval != 0;
		break;
	case BNXT_ULP_COND_OPC_RF_NOT_SET:
		if (!ulp_regfile_read(parms->regfile, operand, &regval)) {
			BNXT_TF_DBG(ERR, "regfile[%d] read oob\n", operand);
			return -EINVAL;
		}
		*res = regval == 0;
		break;
	case BNXT_ULP_COND_OPC_FLOW_PAT_MATCH:
		if (parms->flow_pattern_id == operand) {
			BNXT_TF_DBG(ERR, "field pattern match failed %x\n",
				    parms->flow_pattern_id);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_ACT_PAT_MATCH:
		if (parms->act_pattern_id == operand) {
			BNXT_TF_DBG(ERR, "act pattern match failed %x\n",
				    parms->act_pattern_id);
			return -EINVAL;
		}
		break;
	default:
		BNXT_TF_DBG(ERR, "Invalid conditional opcode %d\n", opc);
		rc = -EINVAL;
		break;
	}
	return (rc);
}

/*
 * Processes a list of conditions and returns both a status and result of the
 * list.  The status must be checked prior to verifying the result.
 *
 * returns 0 for success, negative on failure
 * returns res = 1 for true, res = 0 for false.
 */
static int32_t
ulp_mapper_cond_opc_list_process(struct bnxt_ulp_mapper_parms *parms,
				 enum bnxt_ulp_cond_list_opc list_opc,
				 struct bnxt_ulp_mapper_cond_info *list,
				 uint32_t num,
				 int32_t *res)
{
	uint32_t i;
	int32_t rc = 0, trc = 0;

	switch (list_opc) {
	case BNXT_ULP_COND_LIST_OPC_AND:
		/* AND Defaults to true. */
		*res = 1;
		break;
	case BNXT_ULP_COND_LIST_OPC_OR:
		/* OR Defaults to false. */
		*res = 0;
		break;
	case BNXT_ULP_COND_LIST_OPC_TRUE:
		*res = 1;
		return rc;
	case BNXT_ULP_COND_LIST_OPC_FALSE:
		*res = 0;
		return rc;
	default:
		BNXT_TF_DBG(ERR, "Invalid conditional list opcode %d\n",
			    list_opc);
		return -EINVAL;
	}

	for (i = 0; i < num; i++) {
		rc = ulp_mapper_cond_opc_process(parms,
						 list[i].cond_opcode,
						 list[i].cond_operand,
						 &trc);
		if (rc)
			return rc;

		if (list_opc == BNXT_ULP_COND_LIST_OPC_AND) {
			/* early return if result is ever zero */
			if (!trc) {
				*res = trc;
				return rc;
			}
		} else {
			/* early return if result is ever non-zero */
			if (trc) {
				*res = trc;
				return rc;
			}
		}
	}

	return rc;
}

/*
 * Processes conflict resolution and returns both a status and result.
 * The status must be checked prior to verifying the result.
 *
 * returns 0 for success, negative on failure
 * returns res = 1 for true, res = 0 for false.
 */
static int32_t
ulp_mapper_conflict_resolution_process(struct bnxt_ulp_mapper_parms *parms,
				       struct bnxt_ulp_mapper_tbl_info *tbl,
				       int32_t *res)
{
	int32_t rc = 0;
	uint64_t regval;
	uint64_t comp_sig_id;

	*res = 0;
	switch (tbl->accept_opcode) {
	case BNXT_ULP_ACCEPT_OPC_ALWAYS:
		*res = 1;
		break;
	case BNXT_ULP_ACCEPT_OPC_FLOW_SIG_ID_MATCH:
		/* perform the signature validation*/
		if (tbl->resource_func ==
		    BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE) {
			/* Perform the check that generic table is hit or not */
			if (!ulp_regfile_read(parms->regfile,
					      BNXT_ULP_RF_IDX_GENERIC_TBL_MISS,
					      &regval)) {
				BNXT_TF_DBG(ERR, "regfile[%d] read oob\n",
					    BNXT_ULP_RF_IDX_GENERIC_TBL_MISS);
				return -EINVAL;
			}
			if (regval) {
				/* not a hit so no need to check flow sign*/
				*res = 1;
				return rc;
			}
		}
		/* compare the new flow signature against stored one */
		if (!ulp_regfile_read(parms->regfile,
				      BNXT_ULP_RF_IDX_FLOW_SIG_ID,
				      &regval)) {
			BNXT_TF_DBG(ERR, "regfile[%d] read oob\n",
				    BNXT_ULP_RF_IDX_FLOW_SIG_ID);
			return -EINVAL;
		}
		comp_sig_id = ULP_COMP_FLD_IDX_RD(parms,
						  BNXT_ULP_CF_IDX_FLOW_SIG_ID);
		regval = tfp_be_to_cpu_64(regval);
		if (comp_sig_id == regval)
			*res = 1;
		else
			BNXT_TF_DBG(ERR, "failed signature match %x:%x\n",
				    (uint32_t)comp_sig_id, (uint32_t)regval);
		break;
	default:
		BNXT_TF_DBG(ERR, "Invalid accept opcode %d\n",
			    tbl->accept_opcode);
		return -EINVAL;
	}
	return rc;
}

static int32_t
ulp_mapper_tbls_process(struct bnxt_ulp_mapper_parms *parms, uint32_t tid)
{
	struct bnxt_ulp_mapper_cond_info *cond_tbls = NULL;
	enum bnxt_ulp_cond_list_opc cond_opc;
	struct bnxt_ulp_mapper_tbl_info *tbls;
	struct bnxt_ulp_mapper_tbl_info *tbl;
	uint32_t num_tbls, tbl_idx, num_cond_tbls;
	int32_t rc = -EINVAL, cond_rc = 0;
	int32_t cond_goto = 1;

	cond_tbls = ulp_mapper_tmpl_reject_list_get(parms, tid,
						    &num_cond_tbls,
						    &cond_opc);
	/*
	 * Process the reject list if exists, otherwise assume that the
	 * template is allowed.
	 */
	if (cond_tbls && num_cond_tbls) {
		rc = ulp_mapper_cond_opc_list_process(parms,
						      cond_opc,
						      cond_tbls,
						      num_cond_tbls,
						      &cond_rc);
		if (rc)
			return rc;

		/* Reject the template if True */
		if (cond_rc) {
			BNXT_TF_DBG(ERR, "%s Template %d rejected.\n",
				    ulp_mapper_tmpl_name_str(parms->tmpl_type),
				    tid);
			return -EINVAL;
		}
	}

	tbls = ulp_mapper_tbl_list_get(parms, tid, &num_tbls);
	if (!tbls || !num_tbls) {
		BNXT_TF_DBG(ERR, "No %s tables for %d:%d\n",
			    ulp_mapper_tmpl_name_str(parms->tmpl_type),
			    parms->dev_id, tid);
		return -EINVAL;
	}

	for (tbl_idx = 0; tbl_idx < num_tbls && cond_goto;) {
		tbl = &tbls[tbl_idx];
		/* Handle the table level opcodes to determine if required. */
		if (ulp_mapper_tbl_memtype_opcode_process(parms, tbl)) {
			cond_goto = tbl->execute_info.cond_false_goto;
			goto next_iteration;
		}

		cond_tbls = ulp_mapper_tbl_execute_list_get(parms, tbl,
							    &num_cond_tbls,
							    &cond_opc);
		rc = ulp_mapper_cond_opc_list_process(parms, cond_opc,
						      cond_tbls, num_cond_tbls,
						      &cond_rc);
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed to process cond opc list "
				   "(%d)\n", rc);
			return rc;
		}
		/* Skip the table if False */
		if (!cond_rc) {
			cond_goto = tbl->execute_info.cond_false_goto;
			goto next_iteration;
		}

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
		case BNXT_ULP_RESOURCE_FUNC_IF_TABLE:
			rc = ulp_mapper_if_tbl_process(parms, tbl);
			break;
		case BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE:
			rc = ulp_mapper_gen_tbl_process(parms, tbl);
			break;
		case BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE:
			rc = ulp_mapper_ctrl_tbl_process(parms, tbl);
			break;
		case BNXT_ULP_RESOURCE_FUNC_INVALID:
			rc = 0;
			break;
		default:
			BNXT_TF_DBG(ERR, "Unexpected mapper resource %d\n",
				    tbl->resource_func);
			rc = -EINVAL;
			goto error;
		}

		if (rc) {
			BNXT_TF_DBG(ERR, "Resource type %d failed\n",
				    tbl->resource_func);
			goto error;
		}

		/* perform the post table process */
		rc  = ulp_mapper_conflict_resolution_process(parms, tbl,
							     &cond_rc);
		if (rc || !cond_rc) {
			BNXT_TF_DBG(ERR, "Failed due to conflict resolution\n");
			rc = -EINVAL;
			goto error;
		}
next_iteration:
		if (cond_goto < 0 && ((int32_t)tbl_idx + cond_goto) < 0) {
			BNXT_TF_DBG(ERR, "invalid conditional goto %d\n",
				    cond_goto);
			goto error;
		}
		tbl_idx += cond_goto;
	}

	return rc;
error:
	BNXT_TF_DBG(ERR, "%s tables failed creation for %d:%d\n",
		    ulp_mapper_tmpl_name_str(parms->tmpl_type),
		    parms->dev_id, tid);
	return rc;
}

static int32_t
ulp_mapper_resource_free(struct bnxt_ulp_context *ulp,
			 uint32_t fid,
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
		rc = ulp_mapper_em_entry_free(ulp, tfp, res);
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
	case BNXT_ULP_RESOURCE_FUNC_PARENT_FLOW:
		rc = ulp_mapper_parent_flow_free(ulp, fid, res);
		break;
	case BNXT_ULP_RESOURCE_FUNC_CHILD_FLOW:
		rc = ulp_mapper_child_flow_free(ulp, fid, res);
		break;
	case BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE:
		rc = ulp_mapper_gen_tbl_res_free(ulp, res);
		break;
	default:
		break;
	}

	return rc;
}

int32_t
ulp_mapper_resources_free(struct bnxt_ulp_context *ulp_ctx,
			  enum bnxt_ulp_fdb_type flow_type,
			  uint32_t fid)
{
	struct ulp_flow_db_res_params res_parms = { 0 };
	int32_t rc, trc;

	if (!ulp_ctx) {
		BNXT_TF_DBG(ERR, "Invalid parms, unable to free flow\n");
		return -EINVAL;
	}

	/*
	 * Set the critical resource on the first resource del, then iterate
	 * while status is good
	 */
	if (flow_type != BNXT_ULP_FDB_TYPE_RID)
		res_parms.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES;

	rc = ulp_flow_db_resource_del(ulp_ctx, flow_type, fid, &res_parms);

	if (rc) {
		/*
		 * This is unexpected on the first call to resource del.
		 * It likely means that the flow did not exist in the flow db.
		 */
		BNXT_TF_DBG(ERR, "Flow[%d][0x%08x] failed to free (rc=%d)\n",
			    flow_type, fid, rc);
		return rc;
	}

	while (!rc) {
		trc = ulp_mapper_resource_free(ulp_ctx, fid, &res_parms);
		if (trc)
			/*
			 * On fail, we still need to attempt to free the
			 * remaining resources.  Don't return
			 */
			BNXT_TF_DBG(ERR,
				    "Flow[%d][0x%x] Res[%d][0x%016" PRIX64
				    "] failed rc=%d.\n",
				    flow_type, fid, res_parms.resource_func,
				    res_parms.resource_hndl, trc);

		/* All subsequent call require the non-critical_resource */
		res_parms.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO;

		rc = ulp_flow_db_resource_del(ulp_ctx,
					      flow_type,
					      fid,
					      &res_parms);
	}

	/* Free the Flow ID since we've removed all resources */
	rc = ulp_flow_db_fid_free(ulp_ctx, flow_type, fid);

	return rc;
}

static void
ulp_mapper_glb_resource_info_deinit(struct bnxt_ulp_context *ulp_ctx,
				    struct bnxt_ulp_mapper_data *mapper_data)
{
	struct bnxt_ulp_mapper_glb_resource_entry *ent;
	struct ulp_flow_db_res_params res;
	uint32_t dir, idx;

	/* Iterate the global resources and process each one */
	for (dir = TF_DIR_RX; dir < TF_DIR_MAX; dir++) {
		for (idx = 0; idx < BNXT_ULP_GLB_RESOURCE_TBL_MAX_SZ;
		      idx++) {
			ent = &mapper_data->glb_res_tbl[dir][idx];
			if (ent->resource_func ==
			    BNXT_ULP_RESOURCE_FUNC_INVALID)
				continue;
			memset(&res, 0, sizeof(struct ulp_flow_db_res_params));
			res.resource_func = ent->resource_func;
			res.direction = dir;
			res.resource_type = ent->resource_type;
			/*convert it from BE to cpu */
			res.resource_hndl =
				tfp_be_to_cpu_64(ent->resource_hndl);
			ulp_mapper_resource_free(ulp_ctx, 0, &res);
		}
	}
}

int32_t
ulp_mapper_flow_destroy(struct bnxt_ulp_context *ulp_ctx,
			enum bnxt_ulp_fdb_type flow_type,
			uint32_t fid)
{
	int32_t rc;

	if (!ulp_ctx) {
		BNXT_TF_DBG(ERR, "Invalid parms, unable to free flow\n");
		return -EINVAL;
	}

	rc = ulp_mapper_resources_free(ulp_ctx, flow_type, fid);
	return rc;
}

/* Function to handle the default global templates that are allocated during
 * the startup and reused later.
 */
static int32_t
ulp_mapper_glb_template_table_init(struct bnxt_ulp_context *ulp_ctx)
{
	uint32_t *glbl_tmpl_list;
	uint32_t num_glb_tmpls, idx, dev_id;
	struct bnxt_ulp_mapper_parms parms;
	struct bnxt_ulp_mapper_data *mapper_data;
	int32_t rc = 0;

	glbl_tmpl_list = ulp_mapper_glb_template_table_get(&num_glb_tmpls);
	if (!glbl_tmpl_list || !num_glb_tmpls)
		return rc; /* No global templates to process */

	/* Get the device id from the ulp context */
	if (bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id)) {
		BNXT_TF_DBG(ERR, "Invalid ulp context\n");
		return -EINVAL;
	}

	mapper_data = bnxt_ulp_cntxt_ptr2_mapper_data_get(ulp_ctx);
	if (!mapper_data) {
		BNXT_TF_DBG(ERR, "Failed to get the ulp mapper data\n");
		return -EINVAL;
	}

	/* Iterate the global resources and process each one */
	for (idx = 0; idx < num_glb_tmpls; idx++) {
		/* Initialize the parms structure */
		memset(&parms, 0, sizeof(parms));
		parms.tfp = bnxt_ulp_cntxt_tfp_get(ulp_ctx);
		parms.ulp_ctx = ulp_ctx;
		parms.dev_id = dev_id;
		parms.mapper_data = mapper_data;
		parms.flow_type = BNXT_ULP_FDB_TYPE_DEFAULT;
		parms.tmpl_type = BNXT_ULP_TEMPLATE_TYPE_CLASS;

		/* Get the class table entry from dev id and class id */
		parms.class_tid = glbl_tmpl_list[idx];

		parms.device_params = bnxt_ulp_device_params_get(parms.dev_id);
		if (!parms.device_params) {
			BNXT_TF_DBG(ERR, "No device for device id %d\n",
				    parms.dev_id);
			return -EINVAL;
		}

		rc = ulp_mapper_tbls_process(&parms, parms.class_tid);
		if (rc)
			return rc;
	}
	return rc;
}

/* Function to handle the mapping of the Flow to be compatible
 * with the underlying hardware.
 */
int32_t
ulp_mapper_flow_create(struct bnxt_ulp_context *ulp_ctx,
		       struct bnxt_ulp_mapper_create_parms *cparms)
{
	struct bnxt_ulp_mapper_parms parms;
	struct ulp_regfile regfile;
	int32_t	 rc = 0, trc;

	if (!ulp_ctx || !cparms)
		return -EINVAL;

	/* Initialize the parms structure */
	memset(&parms, 0, sizeof(parms));
	parms.act_prop = cparms->act_prop;
	parms.act_bitmap = cparms->act;
	parms.hdr_bitmap = cparms->hdr_bitmap;
	parms.regfile = &regfile;
	parms.hdr_field = cparms->hdr_field;
	parms.fld_bitmap = cparms->fld_bitmap;
	parms.comp_fld = cparms->comp_fld;
	parms.tfp = bnxt_ulp_cntxt_tfp_get(ulp_ctx);
	parms.ulp_ctx = ulp_ctx;
	parms.act_tid = cparms->act_tid;
	parms.class_tid = cparms->class_tid;
	parms.flow_type = cparms->flow_type;
	parms.parent_flow = cparms->parent_flow;
	parms.parent_fid = cparms->parent_fid;
	parms.fid = cparms->flow_id;
	parms.tun_idx = cparms->tun_idx;
	parms.app_priority = cparms->app_priority;
	parms.flow_pattern_id = cparms->flow_pattern_id;
	parms.act_pattern_id = cparms->act_pattern_id;

	/* Get the device id from the ulp context */
	if (bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &parms.dev_id)) {
		BNXT_TF_DBG(ERR, "Invalid ulp context\n");
		return -EINVAL;
	}

	/* Get the device params, it will be used in later processing */
	parms.device_params = bnxt_ulp_device_params_get(parms.dev_id);
	if (!parms.device_params) {
		BNXT_TF_DBG(ERR, "No device parms for device id %d\n",
			    parms.dev_id);
		return -EINVAL;
	}

	/*
	 * Get the mapper data for dynamic mapper data such as default
	 * ids.
	 */
	parms.mapper_data = (struct bnxt_ulp_mapper_data *)
		bnxt_ulp_cntxt_ptr2_mapper_data_get(ulp_ctx);
	if (!parms.mapper_data) {
		BNXT_TF_DBG(ERR, "Failed to get the ulp mapper data\n");
		return -EINVAL;
	}

	/* initialize the registry file for further processing */
	if (!ulp_regfile_init(parms.regfile)) {
		BNXT_TF_DBG(ERR, "regfile initialization failed.\n");
		return -EINVAL;
	}

	/* Process the action template list from the selected action table*/
	if (parms.act_tid) {
		parms.tmpl_type = BNXT_ULP_TEMPLATE_TYPE_ACTION;
		/* Process the action template tables */
		rc = ulp_mapper_tbls_process(&parms, parms.act_tid);
		if (rc)
			goto flow_error;
	}

	if (parms.class_tid) {
		parms.tmpl_type = BNXT_ULP_TEMPLATE_TYPE_CLASS;

		/* Process the class template tables.*/
		rc = ulp_mapper_tbls_process(&parms, parms.class_tid);
		if (rc)
			goto flow_error;
	}

	/* setup the parent-child details */
	if (parms.parent_flow) {
		/* create a parent flow details */
		rc = ulp_flow_db_parent_flow_create(&parms);
		if (rc)
			goto flow_error;
	} else if (parms.parent_fid) {
		/* create a child flow details */
		rc = ulp_flow_db_child_flow_create(&parms);
		if (rc)
			goto flow_error;
	}

	return rc;

flow_error:
	/* Free all resources that were allocated during flow creation */
	trc = ulp_mapper_flow_destroy(ulp_ctx, parms.flow_type,
				      parms.fid);
	if (trc)
		BNXT_TF_DBG(ERR, "Failed to free all resources rc=%d\n", trc);

	return rc;
}

int32_t
ulp_mapper_init(struct bnxt_ulp_context *ulp_ctx)
{
	struct bnxt_ulp_mapper_data *data;
	struct tf *tfp;
	int32_t rc;

	if (!ulp_ctx)
		return -EINVAL;

	tfp = bnxt_ulp_cntxt_tfp_get(ulp_ctx);
	if (!tfp)
		return -EINVAL;

	data = rte_zmalloc("ulp_mapper_data",
			   sizeof(struct bnxt_ulp_mapper_data), 0);
	if (!data) {
		BNXT_TF_DBG(ERR, "Failed to allocate the mapper data\n");
		return -ENOMEM;
	}

	if (bnxt_ulp_cntxt_ptr2_mapper_data_set(ulp_ctx, data)) {
		BNXT_TF_DBG(ERR, "Failed to set mapper data in context\n");
		/* Don't call deinit since the prof_func wasn't allocated. */
		rte_free(data);
		return -ENOMEM;
	}

	/* Allocate the global resource ids */
	rc = ulp_mapper_glb_resource_info_init(ulp_ctx, data);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to initialize global resource ids\n");
		goto error;
	}

	/* Allocate the generic table list */
	rc = ulp_mapper_generic_tbl_list_init(data);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to initialize generic tbl list\n");
		goto error;
	}

	/* Allocate global template table entries */
	rc = ulp_mapper_glb_template_table_init(ulp_ctx);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to initialize global templates\n");
		goto error;
	}

	return 0;
error:
	/* Ignore the return code in favor of returning the original error. */
	ulp_mapper_deinit(ulp_ctx);
	return rc;
}

void
ulp_mapper_deinit(struct bnxt_ulp_context *ulp_ctx)
{
	struct bnxt_ulp_mapper_data *data;
	struct tf *tfp;

	if (!ulp_ctx) {
		BNXT_TF_DBG(ERR,
			    "Failed to acquire ulp context, so data may "
			    "not be released.\n");
		return;
	}

	data = (struct bnxt_ulp_mapper_data *)
		bnxt_ulp_cntxt_ptr2_mapper_data_get(ulp_ctx);
	if (!data) {
		/* Go ahead and return since there is no allocated data. */
		BNXT_TF_DBG(ERR, "No data appears to have been allocated.\n");
		return;
	}

	tfp = bnxt_ulp_cntxt_tfp_get(ulp_ctx);
	if (!tfp) {
		BNXT_TF_DBG(ERR, "Failed to acquire tfp.\n");
		/* Free the mapper data regardless of errors. */
		goto free_mapper_data;
	}

	/* Free the global resource info table entries */
	ulp_mapper_glb_resource_info_deinit(ulp_ctx, data);

free_mapper_data:
	/* Free the generic table */
	(void)ulp_mapper_generic_tbl_list_deinit(data);

	rte_free(data);
	/* Reset the data pointer within the ulp_ctx. */
	bnxt_ulp_cntxt_ptr2_mapper_data_set(ulp_ctx, NULL);
}
