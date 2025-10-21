/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
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
#include "bnxt_ulp_utils.h"
#include "tfp.h"
#include "tf_ext_flow_handle.h"
#include "ulp_mark_mgr.h"
#include "ulp_mapper.h"
#include "ulp_flow_db.h"
#include "tf_util.h"
#include "ulp_template_db_tbl.h"
#include "ulp_port_db.h"
#include "ulp_ha_mgr.h"
#include "bnxt_tf_pmd_shim.h"
#ifdef TF_FLOW_SCALE_QUERY
#include "tf_resources.h"
#include "tfc_resources.h"
#endif /* TF_FLOW_SCALE_QUERY */

static uint8_t mapper_fld_zeros[16] = { 0 };

static uint8_t mapper_fld_ones[16] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static uint8_t mapper_fld_one[16] = {
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01
};

static int32_t
ulp_mapper_cond_opc_list_process(struct bnxt_ulp_mapper_parms *parms,
				 struct bnxt_ulp_mapper_cond_list_info *info,
				 int32_t *res);

static const struct ulp_mapper_core_ops *
bnxt_ulp_mapper_ops_get(struct bnxt *bp)
{
	int32_t rc;
	enum bnxt_ulp_device_id  dev_id;
	const struct ulp_mapper_core_ops *func_ops;

	rc = bnxt_ulp_devid_get(bp, &dev_id);
	if (unlikely(rc))
		return NULL;

	switch (dev_id) {
	case BNXT_ULP_DEVICE_ID_THOR2:
		func_ops = &ulp_mapper_tfc_core_ops;
		break;
	case BNXT_ULP_DEVICE_ID_THOR:
	case BNXT_ULP_DEVICE_ID_STINGRAY:
	case BNXT_ULP_DEVICE_ID_WH_PLUS:
		func_ops = &ulp_mapper_tf_core_ops;
		break;
	default:
		func_ops = NULL;
		break;
	}
	return func_ops;
}

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
	if (unlikely(!num_entries))
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
int32_t
ulp_mapper_glb_resource_read(struct bnxt_ulp_mapper_data *mapper_data,
			     enum tf_dir dir,
			     uint16_t idx,
			     uint64_t *regval,
			     bool *shared)
{
	if (unlikely(!mapper_data || !regval || !shared ||
		     dir >= TF_DIR_MAX || idx >= BNXT_ULP_GLB_RF_IDX_LAST))
		return -EINVAL;

	*regval = mapper_data->glb_res_tbl[dir][idx].resource_hndl;
	*shared = mapper_data->glb_res_tbl[dir][idx].shared;
	return 0;
}

/*
 * Write a global resource to the mapper global resource list
 *
 * The regval value must be in big-endian.
 *
 * return 0 on success.
 */
int32_t
ulp_mapper_glb_resource_write(struct bnxt_ulp_mapper_data *data,
			      struct bnxt_ulp_glb_resource_info *res,
			      uint64_t regval, bool shared)
{
	struct bnxt_ulp_mapper_glb_resource_entry *ent;

	/* validate the arguments */
	if (unlikely(!data || res->direction >= TF_DIR_MAX ||
		     res->glb_regfile_index >= BNXT_ULP_GLB_RF_IDX_LAST))
		return -EINVAL;

	/* write to the mapper data */
	ent = &data->glb_res_tbl[res->direction][res->glb_regfile_index];
	ent->resource_func = res->resource_func;
	ent->resource_type = res->resource_type;
	ent->resource_hndl = regval;
	ent->shared = shared;
	return 0;
}

/*
 * Internal function to allocate identity resource and store it in mapper data.
 *
 * returns 0 on success
 */
int32_t
ulp_mapper_resource_ident_allocate(struct bnxt_ulp_context *ulp_ctx,
				   struct bnxt_ulp_mapper_data *mapper_data,
				   struct bnxt_ulp_glb_resource_info *glb_res,
				   bool shared)
{
	const struct ulp_mapper_core_ops *op = mapper_data->mapper_oper;
	uint32_t session_type = BNXT_ULP_SESSION_TYPE_DEFAULT;
	struct ulp_flow_db_res_params res = { 0 };
	uint64_t regval, id = 0;
	int32_t rc = 0;

	session_type = shared ?  BNXT_ULP_SESSION_TYPE_SHARED :
				 BNXT_ULP_SESSION_TYPE_DEFAULT;

	/* Global identifiers are tracked by session */
	rc = op->ulp_mapper_core_ident_alloc_process(ulp_ctx,
						     session_type,
						     glb_res->resource_type,
						     glb_res->direction,
						     CFA_TRACK_TYPE_SID,
						     &id);
	if (unlikely(rc))
		return rc;

	/* entries are stored as big-endian format */
	regval = tfp_cpu_to_be_64(id);
	/*
	 * write to the mapper global resource
	 * Shared resources are never allocated through this method, so the
	 * shared flag is always false.
	 */
	rc = ulp_mapper_glb_resource_write(mapper_data, glb_res, regval, shared);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to write to global resource id\n");
		/* Free the identifier when update failed */
		res.direction = glb_res->direction;
		res.resource_type = glb_res->resource_type;
		res.resource_hndl = id;
		op->ulp_mapper_core_ident_free(ulp_ctx, &res);
		return rc;
	}
	return rc;
}

/*
 * Internal function to allocate index tbl resource and store it in mapper data.
 *
 * returns 0 on success
 */
int32_t
ulp_mapper_resource_index_tbl_alloc(struct bnxt_ulp_context *ulp_ctx,
				    struct bnxt_ulp_mapper_data *mapper_data,
				    struct bnxt_ulp_glb_resource_info *glb_res,
				    bool shared)
{
	const struct ulp_mapper_core_ops *op = mapper_data->mapper_oper;
	uint32_t session_type = BNXT_ULP_SESSION_TYPE_DEFAULT;
	struct ulp_flow_db_res_params res = { 0 };
	uint64_t regval, index = 0;
	int32_t rc = 0;

	session_type = shared ? BNXT_ULP_SESSION_TYPE_SHARED :
				BNXT_ULP_SESSION_TYPE_DEFAULT;

	op->ulp_mapper_core_index_tbl_alloc_process(ulp_ctx, session_type,
						    glb_res->resource_type,
						    glb_res->direction, &index);

	/* entries are stored as big-endian format */
	regval = tfp_cpu_to_be_64((uint64_t)index);
	/*
	 * write to the mapper global resource
	 * Shared resources are never allocated through this method, so the
	 * shared flag is always false.
	 */
	rc = ulp_mapper_glb_resource_write(mapper_data, glb_res, regval, shared);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to write to global resource id\n");
		/* Free the index when update failed */
		res.direction = glb_res->direction;
		res.resource_type = glb_res->resource_type;
		res.resource_hndl = index;
		rc = op->ulp_mapper_core_cmm_entry_free(ulp_ctx, &res, NULL);
		return rc;
	}
	return rc;
}

static int32_t
ulp_mapper_glb_field_tbl_get(struct bnxt_ulp_mapper_parms *parms,
			     uint32_t operand,
			     uint8_t *val)
{
	uint32_t t_idx;

	if (unlikely(operand >= BNXT_ULP_GLB_FIELD_TBL_SIZE)) {
		BNXT_DRV_DBG(ERR, "Invalid hdr field index %x:%x\n",
			     parms->class_tid, operand);
		*val = 0;
		return -EINVAL; /* error */
	}

	t_idx = ULP_COMP_FLD_IDX_RD(parms, BNXT_ULP_CF_IDX_HDR_SIG_ID);
	*val = ulp_class_match_list[t_idx].field_list[operand];
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
	if (unlikely(idx >= BNXT_ULP_ACT_PROP_IDX_LAST))
		return 0;
	return ulp_act_prop_map_table[idx];
}

static struct bnxt_ulp_mapper_cond_list_info *
ulp_mapper_tmpl_reject_list_get(struct bnxt_ulp_mapper_parms *mparms,
				uint32_t tid)
{
	const struct bnxt_ulp_template_device_tbls *dev_tbls;

	dev_tbls = &mparms->device_params->dev_tbls[mparms->tmpl_type];
	return &dev_tbls->tmpl_list[tid].reject_info;
}

static struct bnxt_ulp_mapper_cond_list_info *
ulp_mapper_cond_oper_list_get(struct bnxt_ulp_mapper_parms *mparms,
			      uint32_t idx)
{
	const struct bnxt_ulp_template_device_tbls *dev_tbls;

	dev_tbls = &mparms->device_params->dev_tbls[mparms->tmpl_type];
	if (unlikely(idx >= dev_tbls->cond_oper_list_size))
		return NULL;
	return &dev_tbls->cond_oper_list[idx];
}

static struct bnxt_ulp_mapper_cond_info *
ulp_mapper_tmpl_cond_list_get(struct bnxt_ulp_mapper_parms *mparms,
			      uint32_t idx)
{
	const struct bnxt_ulp_template_device_tbls *dev_tbls;

	dev_tbls = &mparms->device_params->dev_tbls[mparms->tmpl_type];
	if (unlikely(idx >= dev_tbls->cond_list_size))
		return NULL;
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
struct bnxt_ulp_mapper_key_info *
ulp_mapper_key_fields_get(struct bnxt_ulp_mapper_parms *mparms,
			  struct bnxt_ulp_mapper_tbl_info *tbl,
			  uint32_t *num_flds)
{
	uint32_t idx;
	const struct bnxt_ulp_template_device_tbls *dev_tbls;

	dev_tbls = &mparms->device_params->dev_tbls[mparms->tmpl_type];
	if (unlikely(!dev_tbls->key_info_list)) {
		*num_flds = 0;
		return NULL;
	}

	idx		= tbl->key_start_idx;
	*num_flds	= tbl->key_num_fields;

	return &dev_tbls->key_info_list[idx];
}

/*
 * Get the list of partial key fields that implement the flow.
 *
 * mparms [in] The mapper parms with information about the flow
 *
 * tbl [in] A single table instance to get the key fields from
 *
 * Return number of partial fields.return 0 if no partial fields
 */
uint32_t
ulp_mapper_partial_key_fields_get(struct bnxt_ulp_mapper_parms *mparms,
				  struct bnxt_ulp_mapper_tbl_info *tbl)
{
	const struct bnxt_ulp_template_device_tbls *dev_tbls;

	dev_tbls = &mparms->device_params->dev_tbls[mparms->tmpl_type];
	if (!dev_tbls->key_info_list)
		return 0;
	return tbl->partial_key_num_fields;
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
	if (unlikely(!dev_tbls->result_field_list)) {
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
	if (unlikely(!dev_tbls->ident_list)) {
		*num_flds = 0;
		return NULL;
	}

	idx = tbl->ident_start_idx;
	*num_flds = tbl->ident_nums;

	return &dev_tbls->ident_list[idx];
}

static struct bnxt_ulp_mapper_field_info *
ulp_mapper_tmpl_key_ext_list_get(struct bnxt_ulp_mapper_parms *mparms,
				 uint32_t idx)
{
	const struct bnxt_ulp_template_device_tbls *dev_tbls;

	dev_tbls = &mparms->device_params->dev_tbls[mparms->tmpl_type];
	if (unlikely(idx >= dev_tbls->key_ext_list_size))
		return NULL;
	return &dev_tbls->key_ext_list[idx];
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
	uint32_t pc_idx;

	pc_idx = (uint32_t)res->resource_hndl;

	/* reset the child flow bitset*/
	if (unlikely(ulp_flow_db_pc_db_parent_flow_set(ulp, pc_idx, parent_fid, 0))) {
		BNXT_DRV_DBG(ERR, "error in reset parent flow bitset %x:%x\n",
			     pc_idx, parent_fid);
		return -EINVAL;
	}
	return 0;
}

static inline int32_t
ulp_mapper_child_flow_free(struct bnxt_ulp_context *ulp,
			   uint32_t child_fid,
			   struct ulp_flow_db_res_params *res)
{
	uint32_t pc_idx;

	pc_idx = (uint32_t)res->resource_hndl;

	/* reset the child flow bitset*/
	if (unlikely(ulp_flow_db_pc_db_child_flow_set(ulp, pc_idx, child_fid, 0))) {
		BNXT_DRV_DBG(ERR,
			     "error in resetting child flow bitset %x:%x\n",
			     pc_idx, child_fid);
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
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR,
			     "Unable to allocate flow table entry\n");
		return -EINVAL;
	}
	/* Store the allocated fid in regfile*/
	val64 = rid;
	rc = ulp_regfile_write(parms->regfile, tbl->fdb_operand,
			       tfp_cpu_to_be_64(val64));
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Write regfile[%d] failed\n",
			     tbl->fdb_operand);
		ulp_flow_db_fid_free(parms->ulp_ctx,
				     BNXT_ULP_FDB_TYPE_RID, rid);
		return -EINVAL;
	}
	/* save the rid into the parms in case a flow fails before pushing the
	 * rid into the fid
	 */
	parms->rid = rid;
	return 0;
}

/*
 * Process the flow database opcode action.
 * returns 0 on success.
 */
int32_t
ulp_mapper_fdb_opc_process(struct bnxt_ulp_mapper_parms *parms,
			   struct bnxt_ulp_mapper_tbl_info *tbl,
			   struct ulp_flow_db_res_params *fid_parms)
{
	uint32_t push_fid;
	uint64_t val64 = 0;
	enum bnxt_ulp_fdb_type flow_type;
	int32_t rc = 0;

	switch (tbl->fdb_opcode) {
	case BNXT_ULP_FDB_OPC_PUSH_FID:
		push_fid = parms->flow_id;
		flow_type = parms->flow_type;
		break;
	case BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE:
		/* get the fid from the regfile */
		rc = ulp_regfile_read(parms->regfile, tbl->fdb_operand,
				      &val64);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "regfile[%d] read oob\n",
				     tbl->fdb_operand);
			return -EINVAL;
		}
		/* Use the extracted fid to update the flow resource */
		push_fid = (uint32_t)tfp_be_to_cpu_64(val64);
		flow_type = BNXT_ULP_FDB_TYPE_RID;
		break;
	case BNXT_ULP_FDB_OPC_PUSH_FID_SW_ONLY:
		push_fid = parms->flow_id;
		flow_type = parms->flow_type;
		fid_parms->fdb_flags = ULP_FDB_FLAG_SW_ONLY;
		break;
	default:
		return rc; /* Nothing to be done */
	}

	/* Add the resource to the flow database */
	rc = ulp_flow_db_resource_add(parms->ulp_ctx, flow_type,
				      push_fid, fid_parms);
	if (unlikely(rc))
		BNXT_DRV_DBG(ERR, "Failed to add res to flow %x rc = %d\n",
			     push_fid, rc);

	return rc;
}

/*
 * Process the flow database opcode action.
 * returns 0 on success.
 */
int32_t
ulp_mapper_priority_opc_process(struct bnxt_ulp_mapper_parms *parms,
				struct bnxt_ulp_mapper_tbl_info *tbl,
				uint32_t *priority)
{
	uint64_t regval = 0;
	int32_t rc = 0;

	switch (tbl->pri_opcode) {
	case BNXT_ULP_PRI_OPC_NOT_USED:
		*priority = bnxt_ulp_default_app_priority_get(parms->ulp_ctx);
		break;
	case BNXT_ULP_PRI_OPC_CONST:
		*priority = tbl->pri_operand;
		break;
	case BNXT_ULP_PRI_OPC_APP_PRI:
		*priority = parms->app_priority;
		break;
	case BNXT_ULP_PRI_OPC_APP_PRI_OR_CONST:
		if (parms->app_priority)
			*priority = parms->app_priority;
		else
			*priority = tbl->pri_operand;
		break;
	case BNXT_ULP_PRI_OPC_REGFILE:
		if (unlikely(ulp_regfile_read(parms->regfile, tbl->pri_operand,
					      &regval))) {
			BNXT_DRV_DBG(ERR, "regfile[%u] read oob\n",
				     tbl->pri_operand);
			rc = -EINVAL;
		}
		*priority = (uint32_t)tfp_be_to_cpu_64(regval);
		break;
	case BNXT_ULP_PRI_OPC_COMP_FIELD:
		if (likely(tbl->pri_operand < BNXT_ULP_CF_IDX_LAST)) {
			regval = ULP_COMP_FLD_IDX_RD(parms, tbl->pri_operand);
			*priority = regval;
		} else {
			BNXT_DRV_DBG(ERR, "comp field out of bounds %u\n",
				     tbl->pri_operand);
			rc = -EINVAL;
		}
		break;
	default:
		BNXT_DRV_DBG(ERR, "Priority opcode not supported %d\n",
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
int32_t
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
	if (unlikely(!byte_data)) {
		BNXT_DRV_DBG(ERR, "invalid argument\n");
		return -EINVAL;
	}

	/* Get the ident list and process each one */
	idents = ulp_mapper_ident_fields_get(parms, tbl, &num_idents);

	for (i = 0; i < num_idents; i++) {
		/* check the size of the buffer for validation */
		if (unlikely((idents[i].ident_bit_pos + idents[i].ident_bit_size) >
		    ULP_BYTE_2_BITS(byte_data_size) ||
			     idents[i].ident_bit_size > ULP_BYTE_2_BITS(sizeof(val64)))) {
			BNXT_DRV_DBG(ERR, "invalid offset or length %x:%x:%x\n",
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
		if (unlikely(ulp_regfile_write(parms->regfile,
					       idents[i].regfile_idx, val64))) {
			BNXT_DRV_DBG(ERR, "Regfile[%d] write failed.\n",
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
			 struct ulp_blob *key,
			 struct bnxt_ulp_mapper_ident_info *ident,
			 uint16_t *val)
{
	const struct ulp_mapper_core_ops *op = parms->mapper_data->mapper_oper;
	struct ulp_flow_db_res_params fid_parms = { 0 };
	bool global = false;
	uint64_t id = 0;
	uint8_t *context;
	uint16_t tmplen = 0;
	int32_t idx;
	int rc;

	fid_parms.direction = tbl->direction;
	fid_parms.resource_func = ident->resource_func;
	fid_parms.resource_type = ident->ident_type;
	fid_parms.critical_resource = tbl->critical_resource;

	if (tbl->resource_func == BNXT_ULP_RESOURCE_FUNC_GLOBAL_IDENTIFIER)
		global = true;

	if (!global) {
		ulp_flow_db_shared_session_set(&fid_parms, tbl->session_type);
		rc = op->ulp_mapper_core_ident_alloc_process(parms->ulp_ctx,
							     tbl->session_type,
							     ident->ident_type,
							     tbl->direction,
							     tbl->track_type,
							     &id);
	} else {
		context = ulp_blob_data_get(key, &tmplen);
		tmplen = ULP_BITS_2_BYTE(tmplen);
		rc = op->ulp_mapper_core_global_ident_alloc(parms->ulp_ctx,
							    ident->ident_type,
							    tbl->direction,
							    context,
							    tmplen,
							    &id);
		fid_parms.resource_func = tbl->resource_func;
	}

	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "identifier process failed\n");
		return rc;
	}

	fid_parms.resource_hndl = id;
	idx = ident->regfile_idx;
	if (unlikely(ulp_regfile_write(parms->regfile, idx, tfp_cpu_to_be_64(id)))) {
		BNXT_DRV_DBG(ERR, "Regfile[%d] write failed.\n", idx);
		rc = -EINVAL;
		/* Need to free the identifier, so goto error */
		goto error;
	}

	/* Link the resource to the flow in the flow db */
	if (!val) {
		rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR,
				     "Failed to link res to flow rc = %d\n",
				     rc);
			/* Need to free the identifier, so goto error */
			goto error;
		}
	} else {
		*val = id;
	}
	return 0;

error:
	/* Need to free the identifier */
	if (!global)
		op->ulp_mapper_core_ident_free(parms->ulp_ctx, &fid_parms);
	else
		op->ulp_mapper_core_global_ident_free(parms->ulp_ctx,
						      &fid_parms);
	return rc;
}

static int32_t
ulp_mapper_field_port_db_process(struct bnxt_ulp_mapper_parms *parms,
				 uint32_t port_id,
				 uint16_t val16,
				 uint8_t **val)
{
	enum bnxt_ulp_port_table port_data = val16;

	switch (port_data) {
	case BNXT_ULP_PORT_TABLE_DRV_FUNC_PARENT_MAC:
		if (unlikely(ulp_port_db_parent_mac_addr_get(parms->ulp_ctx, port_id,
							     val))) {
			BNXT_DRV_DBG(ERR, "Invalid port id %u\n", port_id);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_PORT_TABLE_DRV_FUNC_MAC:
		if (unlikely(ulp_port_db_drv_mac_addr_get(parms->ulp_ctx, port_id,
							  val))) {
			BNXT_DRV_DBG(ERR, "Invalid port id %u\n", port_id);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_PORT_TABLE_DRV_FUNC_PARENT_VNIC:
		if (unlikely(ulp_port_db_parent_vnic_get(parms->ulp_ctx, port_id,
							 val))) {
			BNXT_DRV_DBG(ERR, "Invalid port id %u\n", port_id);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_PORT_TABLE_PORT_IS_PF:
		if (unlikely(ulp_port_db_port_is_pf_get(parms->ulp_ctx, port_id,
							val))) {
			BNXT_DRV_DBG(ERR, "Invalid port id %u\n", port_id);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_PORT_TABLE_VF_FUNC_METADATA:
		if (unlikely(ulp_port_db_port_meta_data_get(parms->ulp_ctx, port_id,
							    val))) {
			BNXT_DRV_DBG(ERR, "Invalid port id %u\n", port_id);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_PORT_TABLE_TABLE_SCOPE:
		if (unlikely(ulp_port_db_port_table_scope_get(parms->ulp_ctx,
							      port_id, val))) {
			BNXT_DRV_DBG(ERR, "Invalid port id %u\n", port_id);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_PORT_TABLE_VF_FUNC_FID:
		if (unlikely(ulp_port_db_port_vf_fid_get(parms->ulp_ctx, port_id, val))) {
			BNXT_DRV_DBG(ERR, "Invalid port id %u\n", port_id);
			return -EINVAL;
		}
		break;
	default:
		BNXT_DRV_DBG(ERR, "Invalid port_data %d\n", port_data);
		return -EINVAL;
	}
	return 0;
}

static int32_t
ulp_mapper_field_port_db_write(struct bnxt_ulp_mapper_parms *parms,
			       uint32_t port_id,
			       uint16_t idx,
			       uint8_t *val,
			       uint32_t length)
{
	enum bnxt_ulp_port_table port_data = idx;
	uint32_t val32;

	switch (port_data) {
	case BNXT_ULP_PORT_TABLE_PHY_PORT_MIRROR_ID:
		if (ULP_BITS_2_BYTE(length) > sizeof(val32)) {
			BNXT_DRV_DBG(ERR, "Invalid data length %u\n", length);
			return -EINVAL;
		}
		memcpy(&val32, val, ULP_BITS_2_BYTE(length));
		if (unlikely(ulp_port_db_port_table_mirror_set(parms->ulp_ctx,
							       port_id,
							       val32))) {
			BNXT_DRV_DBG(ERR, "Invalid port id %u\n", port_id);
			return -EINVAL;
		}
		break;
	default:
		BNXT_DRV_DBG(ERR, "Invalid port_data %d\n", port_data);
		return -EINVAL;
	}
	return 0;
}

static int32_t
ulp_mapper_field_src_process(struct bnxt_ulp_mapper_parms *parms,
			     enum bnxt_ulp_field_src field_src,
			     uint8_t *field_opr,
			     enum tf_dir dir,
			     uint8_t is_key,
			     uint32_t bitlen,
			     uint8_t **val,
			     uint32_t *val_len,
			     uint64_t *value)
{
	struct bnxt_ulp_mapper_cond_list_info info = { 0 };
	struct bnxt_ulp_mapper_data *m;
	uint8_t bit;
	uint32_t port_id, val_size, field_size;
	uint16_t idx = 0, size_idx = 0, offset = 0;
	uint32_t bytelen = ULP_BITS_2_BYTE(bitlen);
	uint8_t *buffer;
	uint64_t lregval;
	int32_t cond_res;
	bool shared;
	uint8_t i = 0;

	*val_len = bitlen;
	*value = 0;
	/* Perform the action */
	switch (field_src) {
	case BNXT_ULP_FIELD_SRC_ZERO:
		*val = mapper_fld_zeros;
		break;
	case BNXT_ULP_FIELD_SRC_CONST:
		*val = field_opr;
		break;
	case BNXT_ULP_FIELD_SRC_ONES:
		*val = mapper_fld_ones;
		*value = 1;
		break;
	case BNXT_ULP_FIELD_SRC_CF:
		if (unlikely(ulp_operand_read(field_opr,
					      (uint8_t *)&idx, sizeof(uint16_t)))) {
			BNXT_DRV_DBG(ERR, "CF operand read failed\n");
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		if (unlikely(idx >= BNXT_ULP_CF_IDX_LAST || bytelen > sizeof(uint64_t))) {
			BNXT_DRV_DBG(ERR, "comp field [%d] read oob %d\n", idx,
				     bytelen);
			return -EINVAL;
		}
		buffer = (uint8_t *)&parms->comp_fld[idx];
		*val = &buffer[sizeof(uint64_t) - bytelen];
		*value = ULP_COMP_FLD_IDX_RD(parms, idx);
		break;
	case BNXT_ULP_FIELD_SRC_RF:
		if (unlikely(ulp_operand_read(field_opr,
					      (uint8_t *)&idx, sizeof(uint16_t)))) {
			BNXT_DRV_DBG(ERR, "RF operand read failed\n");
			return -EINVAL;
		}

		idx = tfp_be_to_cpu_16(idx);
		/* Uninitialized regfile entries return 0 */
		if (unlikely(ulp_regfile_read(parms->regfile, idx, &lregval)) ||
		    sizeof(uint64_t) < bytelen) {
			BNXT_DRV_DBG(ERR, "regfile[%d] read oob %u\n", idx,
				     bytelen);
			return -EINVAL;
		}
		buffer = (uint8_t *)&parms->regfile->entry[idx].data;
		*val = &buffer[sizeof(uint64_t) - bytelen];
		*value = tfp_be_to_cpu_64(lregval);
		break;
	case BNXT_ULP_FIELD_SRC_ACT_PROP:
		if (unlikely(ulp_operand_read(field_opr,
					      (uint8_t *)&idx, sizeof(uint16_t)))) {
			BNXT_DRV_DBG(ERR, "Action operand read failed\n");
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		if (unlikely(idx >= BNXT_ULP_ACT_PROP_IDX_LAST)) {
			BNXT_DRV_DBG(ERR, "act_prop[%d] oob\n", idx);
			return -EINVAL;
		}
		buffer = &parms->act_prop->act_details[idx];
		field_size = ulp_mapper_act_prop_size_get(idx);
		if (unlikely(bytelen > field_size)) {
			BNXT_DRV_DBG(ERR, "act_prop[%d] field size small %u\n",
				     idx, field_size);
			return -EINVAL;
		}
		*val = &buffer[field_size - bytelen];
		if (sizeof(*value) >= field_size) {
			*value = buffer[0];
			for (i = 1; i < field_size; i++)
				*value = (*value <<  8) | buffer[i];
		}
		break;
	case BNXT_ULP_FIELD_SRC_ACT_PROP_SZ:
		if (unlikely(ulp_operand_read(field_opr,
					      (uint8_t *)&idx, sizeof(uint16_t)))) {
			BNXT_DRV_DBG(ERR, "Action sz operand read failed\n");
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);

		if (unlikely(idx >= BNXT_ULP_ACT_PROP_IDX_LAST)) {
			BNXT_DRV_DBG(ERR, "act_prop_sz[%d] oob\n", idx);
			return -EINVAL;
		}
		*val = &parms->act_prop->act_details[idx];

		/* get the size index next */
		if (unlikely(ulp_operand_read(&field_opr[sizeof(uint16_t)],
					      (uint8_t *)&size_idx, sizeof(uint16_t)))) {
			BNXT_DRV_DBG(ERR, "Action sz operand read failed\n");
			return -EINVAL;
		}
		size_idx = tfp_be_to_cpu_16(size_idx);
		if (unlikely(size_idx >= BNXT_ULP_ACT_PROP_IDX_LAST)) {
			BNXT_DRV_DBG(ERR, "act_prop[%d] oob\n", size_idx);
			return -EINVAL;
		}
		memcpy(&val_size, &parms->act_prop->act_details[size_idx],
		       sizeof(uint32_t));
		val_size = tfp_be_to_cpu_32(val_size);
		*val_len = ULP_BYTE_2_BITS(val_size);
		break;
	case BNXT_ULP_FIELD_SRC_GLB_RF:
		if (unlikely(ulp_operand_read(field_opr,
					      (uint8_t *)&idx, sizeof(uint16_t)))) {
			BNXT_DRV_DBG(ERR, "Global regfile read failed\n");
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		if (unlikely(ulp_mapper_glb_resource_read(parms->mapper_data,
							  dir, idx, &lregval, &shared) ||
			     sizeof(uint64_t) < bytelen)) {
			BNXT_DRV_DBG(ERR, "Global regfile[%d] read failed %u\n",
				     idx, bytelen);
			return -EINVAL;
		}
		m = parms->mapper_data;
		buffer = (uint8_t *)&m->glb_res_tbl[dir][idx].resource_hndl;
		*val = &buffer[sizeof(uint64_t) - bytelen];
		*value = tfp_be_to_cpu_64(lregval);
		break;
	case BNXT_ULP_FIELD_SRC_HF:
	case BNXT_ULP_FIELD_SRC_SUB_HF:
		if (unlikely(ulp_operand_read(field_opr,
					      (uint8_t *)&idx, sizeof(uint16_t)))) {
			BNXT_DRV_DBG(ERR, "Header field read failed\n");
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		/* get the index from the global field list */
		if (unlikely(ulp_mapper_glb_field_tbl_get(parms, idx, &bit))) {
			BNXT_DRV_DBG(ERR, "invalid ulp_glb_field_tbl idx %d\n",
				     idx);
			return -EINVAL;
		}
		if (is_key)
			buffer = parms->hdr_field[bit].spec;
		else
			buffer = parms->hdr_field[bit].mask;

		field_size = parms->hdr_field[bit].size;
		if (!field_size) {
			/* To support field processing of undefined fields */
			*val = mapper_fld_zeros;
			break;
		} else if (unlikely(bytelen > field_size)) {
			BNXT_DRV_DBG(ERR, "Hdr field[%d] size small %u\n",
				     bit, field_size);
			return -EINVAL;
		}
		if (field_src == BNXT_ULP_FIELD_SRC_HF) {
			*val = &buffer[field_size - bytelen];
		} else {
			/* get the offset next */
			if (unlikely(ulp_operand_read(&field_opr[sizeof(uint16_t)],
					     (uint8_t *)&offset,
						      sizeof(uint16_t)))) {
				BNXT_DRV_DBG(ERR, "Hdr fld size read failed\n");
				return -EINVAL;
			}
			offset = tfp_be_to_cpu_16(offset);
			offset = ULP_BITS_2_BYTE_NR(offset);
			if (unlikely((offset + bytelen) > field_size)) {
				BNXT_DRV_DBG(ERR, "Hdr field[%d] oob\n", bit);
				return -EINVAL;
			}
			*val = &buffer[offset];
		}
		break;
	case BNXT_ULP_FIELD_SRC_HDR_BIT:
		if (unlikely(ulp_operand_read(field_opr,
					      (uint8_t *)&lregval, sizeof(uint64_t)))) {
			BNXT_DRV_DBG(ERR, "Header bit read failed\n");
			return -EINVAL;
		}
		lregval = tfp_be_to_cpu_64(lregval);
		if (unlikely(ULP_BITMAP_ISSET(parms->hdr_bitmap->bits, lregval))) {
			*val = mapper_fld_one;
			*value = 1;
		} else {
			*val = mapper_fld_zeros;
		}
		break;
	case BNXT_ULP_FIELD_SRC_ACT_BIT:
		if (unlikely(ulp_operand_read(field_opr,
					      (uint8_t *)&lregval, sizeof(uint64_t)))) {
			BNXT_DRV_DBG(ERR, "Action bit read failed\n");
			return -EINVAL;
		}
		lregval = tfp_be_to_cpu_64(lregval);
		if (unlikely(ULP_BITMAP_ISSET(parms->act_bitmap->bits, lregval))) {
			*val = mapper_fld_one;
			*value = 1;
		} else {
			*val = mapper_fld_zeros;
		}
		break;
	case BNXT_ULP_FIELD_SRC_FIELD_BIT:
		if (unlikely(ulp_operand_read(field_opr,
					      (uint8_t *)&idx, sizeof(uint16_t)))) {
			BNXT_DRV_DBG(ERR, "Field bit read failed\n");
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		/* get the index from the global field list */
		if (unlikely(ulp_mapper_glb_field_tbl_get(parms, idx, &bit))) {
			BNXT_DRV_DBG(ERR, "invalid ulp_glb_field_tbl idx %d\n",
				     idx);
			return -EINVAL;
		}
		if (unlikely(ULP_INDEX_BITMAP_GET(parms->fld_bitmap->bits, bit))) {
			*val = mapper_fld_one;
			*value = 1;
		} else {
			*val = mapper_fld_zeros;
		}
		break;
	case BNXT_ULP_FIELD_SRC_PORT_TABLE:
		if (unlikely(ulp_operand_read(field_opr,
					      (uint8_t *)&idx, sizeof(uint16_t)))) {
			BNXT_DRV_DBG(ERR, "CF operand read failed\n");
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		if (unlikely(idx >= BNXT_ULP_CF_IDX_LAST || bytelen > sizeof(uint64_t))) {
			BNXT_DRV_DBG(ERR, "comp field [%d] read oob %d\n", idx,
				     bytelen);
			return -EINVAL;
		}

		/* The port id is present in the comp field list */
		port_id = ULP_COMP_FLD_IDX_RD(parms, idx);
		/* get the port table enum  */
		if (unlikely(ulp_operand_read(field_opr + sizeof(uint16_t),
					      (uint8_t *)&idx, sizeof(uint16_t)))) {
			BNXT_DRV_DBG(ERR, "Port table enum read failed\n");
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		if (unlikely(ulp_mapper_field_port_db_process(parms, port_id, idx,
							      val))) {
			BNXT_DRV_DBG(ERR, "field port table failed\n");
			return -EINVAL;
		}
		break;
	case BNXT_ULP_FIELD_SRC_ENC_HDR_BIT:
		if (unlikely(ulp_operand_read(field_opr,
					      (uint8_t *)&lregval, sizeof(uint64_t)))) {
			BNXT_DRV_DBG(ERR, "Header bit read failed\n");
			return -EINVAL;
		}
		lregval = tfp_be_to_cpu_64(lregval);
		if (ULP_BITMAP_ISSET(parms->enc_hdr_bitmap->bits, lregval)) {
			*val = mapper_fld_one;
			*value = 1;
		} else {
			*val = mapper_fld_zeros;
		}
		break;
	case BNXT_ULP_FIELD_SRC_ENC_FIELD:
		if (unlikely(ulp_operand_read(field_opr,
					      (uint8_t *)&idx, sizeof(uint16_t)))) {
			BNXT_DRV_DBG(ERR, "Header field read failed\n");
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		/* get the index from the global field list */
		if (unlikely(idx >= BNXT_ULP_ENC_FIELD_LAST)) {
			BNXT_DRV_DBG(ERR, "invalid encap field tbl idx %d\n",
				     idx);
			return -EINVAL;
		}
		buffer = parms->enc_field[idx].spec;
		field_size = parms->enc_field[idx].size;
		if (unlikely(bytelen > field_size)) {
			BNXT_DRV_DBG(ERR, "Encap field[%d] size small %u\n",
				     idx, field_size);
			return -EINVAL;
		}
		*val = &buffer[field_size - bytelen];
		break;
	case BNXT_ULP_FIELD_SRC_SKIP:
		/* do nothing */
		*val = mapper_fld_zeros;
		*val_len = 0;
		break;
	case BNXT_ULP_FIELD_SRC_REJECT:
		return -EINVAL;
	case BNXT_ULP_FIELD_SRC_LIST_AND:
	case BNXT_ULP_FIELD_SRC_LIST_OR:
		/* read the cond table index and count */
		if (unlikely(ulp_operand_read(field_opr,
					      (uint8_t *)&idx, sizeof(uint16_t)))) {
			BNXT_DRV_DBG(ERR, "Cond idx operand read failed\n");
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);

		if (unlikely(ulp_operand_read(field_opr + sizeof(uint16_t),
					      (uint8_t *)&size_idx, sizeof(uint16_t)))) {
			BNXT_DRV_DBG(ERR, "Cond count operand read failed\n");
			return -EINVAL;
		}
		size_idx = tfp_be_to_cpu_16(size_idx);

		/* populate the extracted vales to create a temp cond list */
		if (field_src == BNXT_ULP_FIELD_SRC_LIST_AND)
			info.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND;
		else
			info.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_OR;
		info.cond_start_idx = idx;
		info.cond_nums = size_idx;
		if (unlikely(ulp_mapper_cond_opc_list_process(parms, &info, &cond_res))) {
			BNXT_DRV_DBG(ERR, "Cond evaluation failed\n");
			return -EINVAL;
		}
		if (cond_res) {
			*val = mapper_fld_one;
			*value = 1;
		} else {
			*val = mapper_fld_zeros;
			*value = 0;
		}
		break;
	case BNXT_ULP_FIELD_SRC_CF_BIT:
		if (unlikely(ulp_operand_read(field_opr,
					      (uint8_t *)&lregval, sizeof(uint64_t)))) {
			BNXT_DRV_DBG(ERR, "CF operand read failed\n");
			return -EINVAL;
		}
		lregval = tfp_be_to_cpu_64(lregval);
		if (unlikely(ULP_BITMAP_ISSET(parms->cf_bitmap, lregval))) {
			*val = mapper_fld_one;
			*value = 1;
		} else {
			*val = mapper_fld_zeros;
		}
		break;
	default:
		BNXT_DRV_DBG(ERR, "invalid field opcode 0x%x\n", field_src);
		return -EINVAL;
	}
	return 0;
}

static int32_t ulp_mapper_field_buffer_eval(uint8_t *buffer, uint32_t bitlen,
					    uint64_t *output)
{
	uint16_t val_16;
	uint32_t val_32;
	uint64_t val_64;
	uint32_t bytelen;

	bytelen = ULP_BITS_2_BYTE(bitlen);
	if (bytelen == sizeof(uint8_t)) {
		*output = *((uint8_t *)buffer);
	} else if (bytelen == sizeof(uint16_t)) {
		val_16 = *((uint16_t *)buffer);
		*output =  tfp_be_to_cpu_16(val_16);
	} else if (bytelen == sizeof(uint32_t)) {
		val_32 = *((uint32_t *)buffer);
		*output =  tfp_be_to_cpu_32(val_32);
	} else if (bytelen == sizeof(val_64)) {
		val_64 = *((uint64_t *)buffer);
		*output =  tfp_be_to_cpu_64(val_64);
	} else {
		*output = 0;
		return -EINVAL;
	}
	return 0;
}

static int32_t ulp_mapper_field_blob_write(enum bnxt_ulp_field_src fld_src,
					   struct ulp_blob *blob,
					   uint8_t *val,
					   uint32_t val_len,
					   uint8_t **out_val)
{
	if (fld_src == BNXT_ULP_FIELD_SRC_ZERO) {
		if (unlikely(ulp_blob_pad_push(blob, val_len) < 0)) {
			BNXT_DRV_DBG(ERR, "too large for blob\n");
			return -EINVAL;
		}
	} else if (fld_src == BNXT_ULP_FIELD_SRC_ACT_PROP_SZ) {
		if (unlikely(ulp_blob_push_encap(blob, val, val_len) < 0)) {
			BNXT_DRV_DBG(ERR, "encap blob push failed\n");
			return -EINVAL;
		}
	} else if (fld_src == BNXT_ULP_FIELD_SRC_SKIP) {
		/* do nothing */
	} else {
		if (unlikely(ulp_blob_push(blob, val, val_len))) {
			BNXT_DRV_DBG(ERR, "push of val1 failed\n");
			return -EINVAL;
		}
	}
	*out_val = val;
	return 0;
}

static int32_t
ulp_mapper_field_opc_next(struct bnxt_ulp_mapper_parms *parms,
			  enum tf_dir dir,
			  uint8_t *field_opr,
			  struct ulp_blob *blob,
			  uint8_t is_key,
			  const char *name)
{
	struct bnxt_ulp_mapper_field_info *field_info;
	uint16_t idx = 0;

	/* read the cond table index and count */
	if (unlikely(ulp_operand_read(field_opr,
				      (uint8_t *)&idx, sizeof(uint16_t)))) {
		BNXT_DRV_DBG(ERR, "field idx operand read failed\n");
		return -EINVAL;
	}
	idx = tfp_be_to_cpu_16(idx);

	field_info = ulp_mapper_tmpl_key_ext_list_get(parms, idx);
	if (unlikely(field_info == NULL)) {
		BNXT_DRV_DBG(ERR, "Invalid field idx %d\n", idx);
		return -EINVAL;
	}

	return ulp_mapper_field_opc_process(parms, dir, field_info,
					    blob, is_key, name);
}

static void
ulp_mapper_key_recipe_tbl_deinit(struct bnxt_ulp_mapper_data *mdata)
{
	struct bnxt_ulp_key_recipe_entry **recipes;
	enum bnxt_ulp_direction dir;
	uint32_t idx, ftype;

	/* If recipe table is not initialized then exit */
	if (!mdata->key_recipe_info.num_recipes)
		return;

	for (dir = 0; dir < BNXT_ULP_DIRECTION_LAST; dir++) {
		for (ftype = 0; ftype < ULP_RECIPE_TYPE_MAX; ftype++) {
			recipes = mdata->key_recipe_info.recipes[dir][ftype];
			for (idx = 0; idx < mdata->key_recipe_info.num_recipes;
			      idx++) {
				rte_free(recipes[idx]);
			}
			rte_free(mdata->key_recipe_info.recipes[dir][ftype]);
			mdata->key_recipe_info.recipes[dir][ftype] = NULL;
			rte_free(mdata->key_recipe_info.recipe_ba[dir][ftype]);
			mdata->key_recipe_info.recipe_ba[dir][ftype] = NULL;
		}
	}
	mdata->key_recipe_info.num_recipes = 0;
}

static int32_t
ulp_mapper_key_recipe_tbl_init(struct bnxt_ulp_context *ulp_ctx,
			       struct bnxt_ulp_mapper_data *mdata)
{
	struct bnxt_ulp_key_recipe_entry **recipes;
	enum bnxt_ulp_direction dir;
	uint32_t dev_id = 0, size_val;
	uint32_t num_recipes, ftype, pool_size;
	int32_t rc = 0;
	struct bitalloc *recipe_ba;

	rc = bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Unable to get device id from ulp.\n");
		return -EINVAL;
	}
	num_recipes = bnxt_ulp_num_key_recipes_get(ulp_ctx);
	if (!num_recipes)
		return rc;

	/* Need to write these values so that a failure will result in freeing
	 * the memory in the deinit
	 */
	mdata->key_recipe_info.num_recipes = num_recipes;
	mdata->key_recipe_info.max_fields = BNXT_ULP_KEY_RECIPE_MAX_FLDS;

	size_val = sizeof(struct bnxt_ulp_key_recipe_entry *);
	pool_size = BITALLOC_SIZEOF(num_recipes);

	/* The caller will deinit if failures occur, so just return fail instead
	 * of attempting to free allocated memory
	 **/
	for (dir = 0; dir < BNXT_ULP_DIRECTION_LAST; dir++) {
		for (ftype = 0; ftype < ULP_RECIPE_TYPE_MAX; ftype++) {
			recipes = rte_zmalloc("key_recipe_list",
					      size_val * num_recipes, 0);
			if (unlikely(!recipes)) {
				BNXT_DRV_DBG(ERR, "Unable to alloc memory\n");
				return -ENOMEM;
			}
			mdata->key_recipe_info.recipes[dir][ftype] = recipes;

			recipe_ba = rte_malloc("key_recipe_ba", pool_size, 0);
			if (unlikely(!recipe_ba)) {
				BNXT_DRV_DBG(ERR, "Unable to alloc memory\n");
				return -ENOMEM;
			}
			mdata->key_recipe_info.recipe_ba[dir][ftype] =
				recipe_ba;
			rc = ba_init(recipe_ba, num_recipes, true);
			if (unlikely(rc)) {
				BNXT_DRV_DBG(ERR,
					     "Unable to alloc recipe ba\n");
				return -ENOMEM;
			}
		}
	}
	return rc;
}

static struct bnxt_ulp_mapper_data *
ulp_mapper_key_recipe_args_validate(struct bnxt_ulp_context *ulp_ctx,
				    enum bnxt_ulp_direction dir,
				    enum bnxt_ulp_resource_sub_type stype,
				    uint32_t recipe_id)
{
	struct bnxt_ulp_mapper_data *mdata;

	mdata = (struct bnxt_ulp_mapper_data *)
		bnxt_ulp_cntxt_ptr2_mapper_data_get(ulp_ctx);
	if (unlikely(!mdata)) {
		BNXT_DRV_DBG(ERR, "Unable to get mapper data.\n");
		return NULL;
	}
	if (unlikely(dir >= BNXT_ULP_DIRECTION_LAST)) {
		BNXT_DRV_DBG(ERR, "Invalid dir (%d) in key recipe\n", dir);
		return NULL;
	}
	if (unlikely(mdata->key_recipe_info.num_recipes == 0)) {
		BNXT_DRV_DBG(ERR, "Recipes are not supported\n");
		return NULL;
	}
	if (unlikely(stype != BNXT_ULP_RESOURCE_SUB_TYPE_KEY_RECIPE_TABLE_WM &&
		     stype != BNXT_ULP_RESOURCE_SUB_TYPE_KEY_RECIPE_TABLE_EM)) {
		BNXT_DRV_DBG(ERR, "Invalid type (%d) in key recipe\n", stype);
		return NULL;
	}
	if (unlikely(recipe_id >= mdata->key_recipe_info.num_recipes ||
		     !mdata->key_recipe_info.num_recipes)) {
		BNXT_DRV_DBG(ERR, "Key recipe id out of range(%u >= %u)\n",
			     recipe_id, mdata->key_recipe_info.num_recipes);
		return NULL;
	}
	return mdata;
}

static struct bnxt_ulp_key_recipe_entry *
ulp_mapper_key_recipe_alloc(struct bnxt_ulp_context *ulp_ctx,
			    enum bnxt_ulp_direction dir,
			    enum bnxt_ulp_resource_sub_type stype,
			    uint32_t recipe_id, bool alloc_only,
			    uint8_t *max_fields)
{
	struct bnxt_ulp_key_recipe_entry **recipes;
	struct bnxt_ulp_mapper_data *mdata = NULL;
	uint32_t size_s = sizeof(struct bnxt_ulp_key_recipe_entry);

	mdata = ulp_mapper_key_recipe_args_validate(ulp_ctx, dir,
						    stype, recipe_id);
	if (unlikely(mdata == NULL))
		return NULL;

	recipes = mdata->key_recipe_info.recipes[dir][stype];
	if (alloc_only && recipes[recipe_id] == NULL) {
		recipes[recipe_id] = rte_zmalloc("key_recipe_entry", size_s, 0);
		if (recipes[recipe_id] == NULL) {
			BNXT_DRV_DBG(ERR, "Unable to alloc key recipe\n");
			return NULL;
		}
	} else if (alloc_only) {
		BNXT_DRV_DBG(ERR, "Recipe ID (%d) already allocated\n",
			     recipe_id);
	}
	*max_fields = mdata->key_recipe_info.max_fields;
	return recipes[recipe_id];
}

/* The free just marks the entry as not in use and resets the number of entries
 * to zero.
 */
static int32_t
ulp_mapper_key_recipe_free(struct bnxt_ulp_context *ulp_ctx,
			   uint8_t dir,
			   enum bnxt_ulp_resource_sub_type stype,
			   uint32_t index)
{
	struct bnxt_ulp_key_recipe_entry **recipes;
	struct bnxt_ulp_mapper_data *mdata = NULL;
	struct bitalloc *recipe_ba = NULL;
	int32_t rc;

	mdata = ulp_mapper_key_recipe_args_validate(ulp_ctx, dir,
						    stype, index);
	if (unlikely(mdata == NULL))
		return -EINVAL;

	recipe_ba = mdata->key_recipe_info.recipe_ba[dir][stype];
	rc = ba_free(recipe_ba, index);
	if (unlikely(rc < 0))
		BNXT_DRV_DBG(DEBUG, "Unable to free recipe id[%s][%u] = (%d)\n",
			     (dir == BNXT_ULP_DIRECTION_INGRESS) ? "rx" : "tx",
			     stype, index);

	recipes = mdata->key_recipe_info.recipes[dir][stype];
	if (unlikely(recipes[index] == NULL)) {
		BNXT_DRV_DBG(DEBUG, "recipe id[%s][%u] = (%d) already freed\n",
			     (dir == BNXT_ULP_DIRECTION_INGRESS) ? "rx" : "tx",
			     stype, index);
		return 0;
	}
	rte_free(recipes[index]);
	recipes[index] = NULL;
	return 0;
}

static void
ulp_mapper_key_recipe_copy_to_src1(struct bnxt_ulp_mapper_field_info *dst,
				   enum bnxt_ulp_field_src field_src,
				   uint8_t *field_opr,
				   struct bnxt_ulp_mapper_field_info *src,
				   bool *written)
{
	if (field_src != BNXT_ULP_FIELD_SRC_SKIP) {
		dst->field_opc = BNXT_ULP_FIELD_OPC_SRC1;
		dst->field_src1 = field_src;
		memcpy(dst->field_opr1, field_opr, 16);
		memcpy(dst->description, src->description, 64);
		dst->field_bit_size = src->field_bit_size;
		*written = true;
	}
}

struct bnxt_ulp_mapper_key_info *
ulp_mapper_key_recipe_fields_get(struct bnxt_ulp_mapper_parms *parms,
				 struct bnxt_ulp_mapper_tbl_info *tbl,
				 uint32_t *num_flds)
{
	struct bnxt_ulp_key_recipe_entry **recipes;
	enum bnxt_ulp_resource_sub_type stype;
	struct bnxt_ulp_mapper_data *mdata = NULL;
	uint64_t regval = 0;
	uint32_t recipe_id = 0;

	/* Don't like this, but need to convert from a tbl resource func to the
	 * subtype for key_recipes.
	 */
	switch (tbl->resource_func) {
	case BNXT_ULP_RESOURCE_FUNC_EM_TABLE:
		stype = BNXT_ULP_RESOURCE_SUB_TYPE_KEY_RECIPE_TABLE_EM;
		break;
	case BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE:
		stype = BNXT_ULP_RESOURCE_SUB_TYPE_KEY_RECIPE_TABLE_WM;
		break;
	default:
		BNXT_DRV_DBG(ERR, "Invalid res func(%d) for recipe fields\n",
			     tbl->resource_func);
		return NULL;
	};

	/* Get the recipe index from the registry file */
	if (ulp_regfile_read(parms->regfile, tbl->key_recipe_operand,
			     &regval)) {
		BNXT_DRV_DBG(ERR, "Failed to get tbl idx from regfile[%d].\n",
			     tbl->tbl_operand);
		return NULL;
	}
	recipe_id = (uint32_t)tfp_be_to_cpu_64(regval);
	mdata = ulp_mapper_key_recipe_args_validate(parms->ulp_ctx,
						    tbl->direction,
						    stype, recipe_id);
	if (mdata == NULL)
		return NULL;

	recipes = mdata->key_recipe_info.recipes[tbl->direction][stype];
	if (recipes[recipe_id] == NULL)
		return NULL;

	*num_flds = recipes[recipe_id]->cnt;
	return &recipes[recipe_id]->flds[0];
}

static int32_t
ulp_mapper_key_recipe_field_opc_next(struct bnxt_ulp_mapper_parms *parms,
				     enum bnxt_ulp_direction dir,
				     uint8_t *field_opr,
				     uint8_t is_key,
				     const char *name,
				     bool *written,
				     struct bnxt_ulp_mapper_field_info *ofld)
{
	struct bnxt_ulp_mapper_field_info *field_info;
	uint16_t idx = 0;

	/* read the cond table index and count */
	if (unlikely(ulp_operand_read(field_opr,
				      (uint8_t *)&idx, sizeof(uint16_t)))) {
		BNXT_DRV_DBG(ERR, "field idx operand read failed\n");
		return -EINVAL;
	}
	idx = tfp_be_to_cpu_16(idx);

	field_info = ulp_mapper_tmpl_key_ext_list_get(parms, idx);
	if (unlikely(field_info == NULL)) {
		BNXT_DRV_DBG(ERR, "Invalid field idx %d\n", idx);
		return -EINVAL;
	}

	return ulp_mapper_key_recipe_field_opc_process(parms, dir, field_info,
						       is_key, name,
						       written, ofld);
}

int32_t
ulp_mapper_key_recipe_field_opc_process(struct bnxt_ulp_mapper_parms *parms,
					uint8_t dir,
					struct bnxt_ulp_mapper_field_info *fld,
					uint8_t is_key,
					const char *name,
					bool *written,
					struct bnxt_ulp_mapper_field_info *ofld)
{
	uint8_t process_src1 = 0;
	uint32_t val1_len = 0;
	uint64_t value1 = 0;
	int32_t rc = 0;
	uint8_t *val1;

	/* prepare the field source and values */
	switch (fld->field_opc) {
	case BNXT_ULP_FIELD_OPC_SRC1:
		/* No logic, just take SRC1 and return */
		ulp_mapper_key_recipe_copy_to_src1(ofld, fld->field_src1,
						   fld->field_opr1, fld,
						   written);
		return rc;
	case BNXT_ULP_FIELD_OPC_SKIP:
		*written = false;
		return rc;
	case BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3:
	case BNXT_ULP_FIELD_OPC_TERNARY_LIST:
		process_src1 = 1;
		break;
	default:
		BNXT_DRV_DBG(ERR, "Invalid fld opcode %u\n", fld->field_opc);
		rc = -EINVAL;
		return rc;
	}

	/* process the src1 opcode  */
	if (process_src1) {
		if (unlikely(ulp_mapper_field_src_process(parms, fld->field_src1,
						 fld->field_opr1, dir, is_key,
						 fld->field_bit_size, &val1,
							  &val1_len, &value1))) {
			BNXT_DRV_DBG(ERR, "fld src1 process failed\n");
			return -EINVAL;
		}
	}

	if (fld->field_opc == BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3) {
		if (value1)
			ulp_mapper_key_recipe_copy_to_src1(ofld,
							   fld->field_src2,
							   fld->field_opr2,
							   fld, written);
		else
			ulp_mapper_key_recipe_copy_to_src1(ofld,
							   fld->field_src3,
							   fld->field_opr3,
							   fld, written);
	} else if (fld->field_opc == BNXT_ULP_FIELD_OPC_TERNARY_LIST) {
		if (value1) {
			/* check if src2 is next */
			if (fld->field_src2 == BNXT_ULP_FIELD_SRC_NEXT) {
				/* get the next field info */
				if (unlikely(ulp_mapper_key_recipe_field_opc_next(parms,
									 dir,
									fld->field_opr2,
									is_key,
									name,
									written,
										  ofld))) {
					BNXT_DRV_DBG(ERR,
						     "recipe fld next process fail\n");
					return -EINVAL;
				} else {
					return rc;
				}
			} else {
				ulp_mapper_key_recipe_copy_to_src1(ofld,
								   fld->field_src2,
								   fld->field_opr2,
								   fld, written);
			}
		} else {
			/* check if src3 is next */
			if (fld->field_src3 == BNXT_ULP_FIELD_SRC_NEXT) {
				/* get the next field info */
				if (unlikely(ulp_mapper_key_recipe_field_opc_next(parms,
									 dir,
									fld->field_opr3,
									is_key,
									name,
									written,
										  ofld))) {
					BNXT_DRV_DBG(ERR,
						     "recipt fld next process fail\n");
					return -EINVAL;
				} else {
					return rc;
				}
			} else {
				ulp_mapper_key_recipe_copy_to_src1(ofld,
								   fld->field_src3,
								   fld->field_opr3,
								   fld, written);
			}
		}
	}
	return rc;
}

static int32_t
ulp_mapper_key_recipe_tbl_process(struct bnxt_ulp_mapper_parms *parms,
				  struct bnxt_ulp_mapper_tbl_info *tbl)
{
	bool alloc = false, write = false, regfile = false;
	struct bnxt_ulp_mapper_key_info	*kflds, *rflds;
	struct bnxt_ulp_mapper_field_info *kfld, *rfld;
	struct bnxt_ulp_mapper_data *mdata = NULL;
	struct bnxt_ulp_key_recipe_entry *recipe;
	struct ulp_flow_db_res_params fid_parms;
	int32_t rc = 0, free_rc, tmp_recipe_id;
	enum bnxt_ulp_resource_sub_type stype;
	uint8_t max_rflds = 0, rnum_flds = 0;
	enum bnxt_ulp_direction dir;
	struct bitalloc *recipe_ba = NULL;
	uint32_t recipe_id = 0;
	uint32_t i, num_kflds;
	bool written = false;
	uint64_t regval = 0;

	dir = tbl->direction;
	stype = tbl->resource_sub_type;

	switch (tbl->tbl_opcode) {
	case BNXT_ULP_KEY_RECIPE_TBL_OPC_ALLOC_WR_REGFILE:
		alloc = true;
		write = true;
		regfile = true;
		break;
	case BNXT_ULP_KEY_RECIPE_TBL_OPC_ALLOC_REGFILE:
		alloc = true;
		regfile = true;
		break;
	case BNXT_ULP_KEY_RECIPE_TBL_OPC_WR_REGFILE:
		alloc = false;
		regfile = true;
		write = true;
		break;
	default:
		BNXT_DRV_DBG(ERR, "Invalid recipe table opcode %d\n",
			     tbl->tbl_opcode);
		return -EINVAL;
	};

	/* Get the recipe_id from the regfile */
	if (!alloc && regfile) {
		if (unlikely(ulp_regfile_read(parms->regfile,
					      tbl->tbl_operand,
					      &regval))) {
			BNXT_DRV_DBG(ERR,
				     "Fail to get tbl idx from regfile[%d].\n",
				     tbl->tbl_operand);
			return -EINVAL;
		}
		recipe_id = rte_be_to_cpu_64(regval);
	}

	if (alloc) {
		/* Allocate a recipe id based on the direction and type
		 * only supported types are EM and WC for now.
		 */
		mdata = ulp_mapper_key_recipe_args_validate(parms->ulp_ctx, dir,
							    stype, 0);
		if (unlikely(mdata == NULL))
			return -EINVAL;

		recipe_ba = mdata->key_recipe_info.recipe_ba[dir][stype];
		tmp_recipe_id = ba_alloc(recipe_ba);
		if (unlikely(tmp_recipe_id < 0)) {
			BNXT_DRV_DBG(ERR, "Failed to allocate a recipe id\n");
			return -EINVAL;
		} else if (unlikely((uint32_t)tmp_recipe_id >=
				    mdata->key_recipe_info.num_recipes)) {
			/* Shouldn't get here, but could be an issue with the
			 * allocator, so free the recipe_id
			 */
			BNXT_DRV_DBG(ERR,
				     "Allocated recipe id(%d) >= max(%d)\n",
				     tmp_recipe_id,
				     mdata->key_recipe_info.num_recipes);
			(void)ba_free(recipe_ba, tmp_recipe_id);
			return -EINVAL;
		}
		/* any error after this must goto error in order to free
		 * the recipe_id
		 */
		recipe_id = tmp_recipe_id;
	}

	if (alloc && regfile) {
		regval = rte_cpu_to_be_64(recipe_id);
		rc = ulp_regfile_write(parms->regfile, tbl->tbl_operand,
				       regval);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Failed to write regfile[%d] rc=%d\n",
				     tbl->tbl_operand, rc);
			if (recipe_ba)
				(void)ba_free(recipe_ba, recipe_id);
			return -EINVAL;
		}
	}

	/* allocate or Get the recipe entry based on alloc */
	recipe = ulp_mapper_key_recipe_alloc(parms->ulp_ctx, dir, stype,
					     recipe_id, alloc, &max_rflds);
	if (unlikely(!recipe || !max_rflds)) {
		BNXT_DRV_DBG(ERR, "Failed to get the recipe slot\n");
		if (recipe_ba)
			(void)ba_free(recipe_ba, recipe_id);
		return -EINVAL;
	}

	/* We have a recipe_id by now, write the data */
	if (write) {
		/* Get the key fields to process */
		kflds = ulp_mapper_key_fields_get(parms, tbl, &num_kflds);
		if (unlikely(!kflds || !num_kflds)) {
			BNXT_DRV_DBG(ERR, "Failed to get the key fields\n");
			rc = -EINVAL;
			goto error;
		}

		rflds = &recipe->flds[0];
		/* iterate over the key fields and write the recipe */
		for (i = 0; i < num_kflds; i++) {
			if (unlikely(rnum_flds >= max_rflds)) {
				BNXT_DRV_DBG(ERR,
					     "Max recipe fields exceeded (%d)\n",
					     rnum_flds);
				goto error;
			}
			written = false;
			kfld = &kflds[i].field_info_spec;
			rfld = &rflds[rnum_flds].field_info_spec;

			rc = ulp_mapper_key_recipe_field_opc_process(parms,
								     dir,
								     kfld, 1,
								     "KEY",
								     &written,
								     rfld);
			if (unlikely(rc))
				goto error;

			if (stype ==
			    BNXT_ULP_RESOURCE_SUB_TYPE_KEY_RECIPE_TABLE_WM) {
				kfld = &kflds[i].field_info_mask;
				rfld = &rflds[rnum_flds].field_info_mask;
				rc = ulp_mapper_key_recipe_field_opc_process(parms,
									     dir,
									     kfld,
									     0,
									     "MASK",
									     &written,
									     rfld);
				if (unlikely(rc))
					goto error;
			}
			if (written)
				rnum_flds++;
		}
		recipe->cnt = rnum_flds;
	}

	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.direction	= tbl->direction;
	fid_parms.resource_func	= tbl->resource_func;
	fid_parms.resource_type	= tbl->resource_type;
	fid_parms.resource_sub_type = tbl->resource_sub_type;
	fid_parms.resource_hndl	= recipe_id;
	fid_parms.critical_resource = tbl->critical_resource;

	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to link resource to flow rc = %d\n",
			     rc);
		goto error;
	}

	return rc;
error:
	/* Free the actual recipe */
	free_rc = ulp_mapper_key_recipe_free(parms->ulp_ctx, tbl->direction,
					     tbl->resource_sub_type, recipe_id);
	if (free_rc)
		BNXT_DRV_DBG(ERR, "Failed to free recipe on error: %d\n",
			     free_rc);
	return rc;
}

int32_t
ulp_mapper_field_opc_process(struct bnxt_ulp_mapper_parms *parms,
			     enum tf_dir dir,
			     struct bnxt_ulp_mapper_field_info *fld,
			     struct ulp_blob *blob,
			     uint8_t is_key,
			     const char *name)
{
	uint16_t write_idx = blob->write_idx;
	uint8_t *val = NULL, *val1, *val2, *val3;
	uint32_t val_len = 0, val1_len = 0, val2_len = 0, val3_len = 0;
	uint8_t process_src1 = 0, process_src2 = 0, process_src3 = 0;
	uint8_t eval_src1 = 0, eval_src2 = 0, eval_src3 = 0;
	uint64_t val_int = 0, val1_int = 0, val2_int = 0, val3_int = 0;
	uint64_t value1 = 0, value2 = 0, value3 = 0;
	int32_t rc = 0;

	/* prepare the field source and values */
	switch (fld->field_opc) {
	case BNXT_ULP_FIELD_OPC_SRC1:
		process_src1 = 1;
		break;
	case BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3:
	case BNXT_ULP_FIELD_OPC_TERNARY_LIST:
		process_src1 = 1;
		break;
	case BNXT_ULP_FIELD_OPC_SRC1_OR_SRC2_OR_SRC3:
	case BNXT_ULP_FIELD_OPC_SRC1_AND_SRC2_OR_SRC3:
		process_src3 = 1;
		eval_src3 = 1;
		process_src1 = 1;
		process_src2 = 1;
		eval_src1 = 1;
		eval_src2 = 1;
		break;
	case BNXT_ULP_FIELD_OPC_SRC1_PLUS_SRC2:
	case BNXT_ULP_FIELD_OPC_SRC1_MINUS_SRC2:
	case BNXT_ULP_FIELD_OPC_SRC1_PLUS_SRC2_POST:
	case BNXT_ULP_FIELD_OPC_SRC1_MINUS_SRC2_POST:
	case BNXT_ULP_FIELD_OPC_SRC1_OR_SRC2:
	case BNXT_ULP_FIELD_OPC_SRC1_AND_SRC2:
		process_src1 = 1;
		process_src2 = 1;
		eval_src1 = 1;
		eval_src2 = 1;
		break;
	default:
		break;
	}

	/* process the src1 opcode  */
	if (process_src1) {
		if (unlikely(ulp_mapper_field_src_process(parms, fld->field_src1,
							  fld->field_opr1, dir, is_key,
							  fld->field_bit_size, &val1,
							  &val1_len, &value1))) {
			BNXT_DRV_DBG(ERR, "fld src1 process failed\n");
			goto error;
		}
		if (eval_src1) {
			if (unlikely(ulp_mapper_field_buffer_eval(val1, val1_len,
								  &val1_int))) {
				BNXT_DRV_DBG(ERR, "fld src1 eval failed\n");
				goto error;
			}
		}
	}

	/* for "if then clause" set the correct process  */
	if (fld->field_opc == BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3) {
		if (value1)
			process_src2 = 1;
		else
			process_src3 = 1;
	} else if (fld->field_opc == BNXT_ULP_FIELD_OPC_TERNARY_LIST) {
		if (value1) {
			/* check if src2 is next */
			if (fld->field_src2 == BNXT_ULP_FIELD_SRC_NEXT) {
				/* get the next field info */
				if (unlikely(ulp_mapper_field_opc_next(parms, dir,
								       fld->field_opr2,
								       blob, is_key,
								       name))) {
					BNXT_DRV_DBG(ERR,
						     "fld next process fail\n");
					goto error;
				} else {
					return rc;
				}
			} else {
				process_src2 = 1;
			}
		} else {
			/* check if src2 is next */
			if (fld->field_src3 == BNXT_ULP_FIELD_SRC_NEXT) {
				/* get the next field info */
				if (unlikely(ulp_mapper_field_opc_next(parms, dir,
								       fld->field_opr3,
								       blob, is_key,
								       name))) {
					BNXT_DRV_DBG(ERR,
						     "fld next process fail\n");
					goto error;
				} else {
					return rc;
				}
			} else {
				process_src3 = 1;
			}
		}
	}

	/* process src2 opcode */
	if (process_src2) {
		if (unlikely(ulp_mapper_field_src_process(parms, fld->field_src2,
							  fld->field_opr2, dir, is_key,
							  fld->field_bit_size, &val2,
							  &val2_len, &value2))) {
			BNXT_DRV_DBG(ERR, "fld src2 process failed\n");
			goto error;
		}
		if (eval_src2) {
			if (unlikely(ulp_mapper_field_buffer_eval(val2, val2_len,
								  &val2_int))) {
				BNXT_DRV_DBG(ERR, "fld src2 eval failed\n");
				goto error;
			}
		}
	}

	/* process src3 opcode */
	if (process_src3) {
		if (unlikely(ulp_mapper_field_src_process(parms, fld->field_src3,
							  fld->field_opr3, dir, is_key,
							  fld->field_bit_size, &val3,
							  &val3_len, &value3))) {
			BNXT_DRV_DBG(ERR, "fld src3 process failed\n");
			goto error;
		}
		if (eval_src3) {
			if (unlikely(ulp_mapper_field_buffer_eval(val3, val3_len,
								  &val3_int))) {
				BNXT_DRV_DBG(ERR, "fld src3 eval failed\n");
				goto error;
			}
		}
	}

	val_len = fld->field_bit_size;
	/* process the field opcodes */
	switch (fld->field_opc) {
	case BNXT_ULP_FIELD_OPC_SRC1:
		rc = ulp_mapper_field_blob_write(fld->field_src1,
						 blob, val1, val1_len, &val);
		val_len = val1_len;
		break;
	case BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3:
	case BNXT_ULP_FIELD_OPC_TERNARY_LIST:
		if (value1) {
			rc = ulp_mapper_field_blob_write(fld->field_src2, blob,
							 val2, val2_len, &val);
			val_len = val2_len;
		} else {
			rc = ulp_mapper_field_blob_write(fld->field_src3, blob,
							 val3, val3_len, &val);
			val_len = val3_len;
		}
		break;
	case BNXT_ULP_FIELD_OPC_SRC1_PLUS_SRC2:
	case BNXT_ULP_FIELD_OPC_SRC1_PLUS_SRC2_POST:
		val_int = val1_int + val2_int;
		val_int = tfp_cpu_to_be_64(val_int);
		val = ulp_blob_push_64(blob, &val_int, fld->field_bit_size);
		if (unlikely(!val))
			rc = -EINVAL;
		break;
	case BNXT_ULP_FIELD_OPC_SRC1_MINUS_SRC2:
	case BNXT_ULP_FIELD_OPC_SRC1_MINUS_SRC2_POST:
		val_int = val1_int - val2_int;
		val_int = tfp_cpu_to_be_64(val_int);
		val = ulp_blob_push_64(blob, &val_int, fld->field_bit_size);
		if (unlikely(!val))
			rc = -EINVAL;
		break;
	case BNXT_ULP_FIELD_OPC_SRC1_OR_SRC2:
		val_int = val1_int | val2_int;
		val_int = tfp_cpu_to_be_64(val_int);
		val = ulp_blob_push_64(blob, &val_int, fld->field_bit_size);
		if (unlikely(!val))
			rc = -EINVAL;
		break;
	case BNXT_ULP_FIELD_OPC_SRC1_OR_SRC2_OR_SRC3:
		val_int = val1_int | val2_int | val3_int;
		val_int = tfp_cpu_to_be_64(val_int);
		val = ulp_blob_push_64(blob, &val_int, fld->field_bit_size);
		if (unlikely(!val))
			rc = -EINVAL;
		break;
	case BNXT_ULP_FIELD_OPC_SRC1_AND_SRC2:
		val_int = val1_int & val2_int;
		val_int = tfp_cpu_to_be_64(val_int);
		val = ulp_blob_push_64(blob, &val_int, fld->field_bit_size);
		if (unlikely(!val))
			rc = -EINVAL;
		break;
	case BNXT_ULP_FIELD_OPC_SRC1_AND_SRC2_OR_SRC3:
		val_int = val1_int & (val2_int | val3_int);
		val_int = tfp_cpu_to_be_64(val_int);
		val = ulp_blob_push_64(blob, &val_int, fld->field_bit_size);
		if (unlikely(!val))
			rc = -EINVAL;
		break;
	case BNXT_ULP_FIELD_OPC_SKIP:
		break;
	default:
		BNXT_DRV_DBG(ERR, "Invalid fld opcode %u\n", fld->field_opc);
		rc = -EINVAL;
		break;
	}

	if (!rc)
		return rc;
error:
	BNXT_DRV_DBG(ERR, "Error in %s:%s process %u:%u\n", name,
		     fld->description, (val) ? write_idx : 0, val_len);
	return -EINVAL;
}

/*
 * Result table process and fill the result blob.
 * data [out] - the result blob data
 */
int32_t
ulp_mapper_tbl_result_build(struct bnxt_ulp_mapper_parms *parms,
			    struct bnxt_ulp_mapper_tbl_info *tbl,
			    struct ulp_blob *data,
			    const char *name)
{
	struct bnxt_ulp_mapper_field_info *dflds;
	uint32_t i = 0, num_flds = 0, encap_flds = 0;
	const struct ulp_mapper_core_ops *oper;
	struct ulp_blob encap_blob;
	int32_t rc = 0;

	/* Get the result field list */
	dflds = ulp_mapper_result_fields_get(parms, tbl, &num_flds,
					     &encap_flds);

	/* validate the result field list counts */
	if (unlikely(!dflds || (!num_flds && !encap_flds))) {
		BNXT_DRV_DBG(ERR, "Failed to get data fields %x:%x\n",
			     num_flds, encap_flds);
		return -EINVAL;
	}

	/* process the result fields */
	for (i = 0; i < num_flds; i++) {
		rc = ulp_mapper_field_opc_process(parms, tbl->direction,
						  &dflds[i], data, 0, name);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "result field processing failed\n");
			return rc;
		}
	}

	/* process encap fields if any */
	if (encap_flds) {
		uint32_t pad = 0;
		/* Initialize the encap blob */
		if (unlikely(ulp_blob_init(&encap_blob,
				  ULP_BYTE_2_BITS(tbl->record_size),
					   parms->device_params->encap_byte_order))) {
			BNXT_DRV_DBG(ERR, "blob inits failed.\n");
			return -EINVAL;
		}
		for (; i < encap_flds; i++) {
			rc = ulp_mapper_field_opc_process(parms, tbl->direction,
							  &dflds[i],
							  &encap_blob, 0, name);
			if (unlikely(rc)) {
				BNXT_DRV_DBG(ERR,
					     "encap field processing failed\n");
				return rc;
			}
		}
		/* add the dynamic pad push */
		if (parms->device_params->dynamic_sram_en) {
			uint16_t rec_s = ULP_BYTE_2_BITS(tbl->record_size);
			uint16_t blob_len;

			oper = parms->mapper_data->mapper_oper;
			blob_len = ulp_blob_data_len_get(&encap_blob);

			/* Get the padding size */
			oper->ulp_mapper_core_dyn_tbl_type_get(parms, tbl,
							       blob_len,
							       &rec_s);
			pad = rec_s - blob_len;
		} else {
			pad = ULP_BYTE_2_BITS(tbl->record_size) -
				ulp_blob_data_len_get(&encap_blob);
		}
		if (unlikely(ulp_blob_pad_push(&encap_blob, pad) < 0)) {
			BNXT_DRV_DBG(ERR, "encap buffer padding failed\n");
			return -EINVAL;
		}

		/* perform the 64 bit byte swap */
		ulp_blob_perform_64B_byte_swap(&encap_blob);
		/* Append encap blob to the result blob */
		rc = ulp_blob_buffer_copy(data, &encap_blob);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "encap buffer copy failed\n");
			return rc;
		}
	}
	return rc;
}

int32_t
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
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to add mark to flow\n");
		return rc;
	}
	fid_parms.direction = tbl->direction;
	fid_parms.resource_func = BNXT_ULP_RESOURCE_FUNC_HW_FID;
	fid_parms.critical_resource = tbl->critical_resource;
	fid_parms.resource_type	= mark_flag;
	fid_parms.resource_hndl	= gfid;
	ulp_flow_db_shared_session_set(&fid_parms, tbl->session_type);

	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (unlikely(rc))
		BNXT_DRV_DBG(ERR, "Fail to link res to flow rc = %d\n", rc);
	return rc;
}

int32_t
ulp_mapper_mark_act_ptr_process(struct bnxt_ulp_mapper_parms *parms,
				struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct ulp_flow_db_res_params fid_parms;
	uint32_t act_idx, mark, mark_flag;
	uint64_t val64 = 0;
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

	if (unlikely(ulp_regfile_read(parms->regfile,
				      BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
				      &val64))) {
		BNXT_DRV_DBG(ERR, "read action ptr main failed\n");
		return -EINVAL;
	}
	act_idx = tfp_be_to_cpu_64(val64);
	mark_flag  = BNXT_ULP_MARK_LOCAL_HW_FID;
	rc = ulp_mark_db_mark_add(parms->ulp_ctx, mark_flag,
				  act_idx, mark);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to add mark to flow\n");
		return rc;
	}
	fid_parms.direction = tbl->direction;
	fid_parms.resource_func = BNXT_ULP_RESOURCE_FUNC_HW_FID;
	fid_parms.critical_resource = tbl->critical_resource;
	fid_parms.resource_type	= mark_flag;
	fid_parms.resource_hndl	= act_idx;
	ulp_flow_db_shared_session_set(&fid_parms, tbl->session_type);

	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (unlikely(rc))
		BNXT_DRV_DBG(ERR, "Fail to link res to flow rc = %d\n", rc);
	return rc;
}

int32_t
ulp_mapper_mark_vfr_idx_process(struct bnxt_ulp_mapper_parms *parms,
				struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct ulp_flow_db_res_params fid_parms;
	uint32_t act_idx, mark, mark_flag;
	uint64_t val64 = 0;
	enum bnxt_ulp_mark_db_opc mark_op = tbl->mark_db_opcode;
	int32_t rc = 0;

	if (mark_op == BNXT_ULP_MARK_DB_OPC_NOP ||
	    mark_op == BNXT_ULP_MARK_DB_OPC_PUSH_IF_MARK_ACTION)
		return rc; /* no need to perform mark action process */

	/* Get the mark id details from the computed field of dev port id */
	mark = ULP_COMP_FLD_IDX_RD(parms, BNXT_ULP_CF_IDX_DEV_PORT_ID);

	 /* Get the main action pointer */
	if (unlikely(ulp_regfile_read(parms->regfile,
				      BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
				      &val64))) {
		BNXT_DRV_DBG(ERR, "read action ptr main failed\n");
		return -EINVAL;
	}
	act_idx = tfp_be_to_cpu_64(val64);

	/* Set the mark flag to local fid and vfr flag */
	mark_flag  = BNXT_ULP_MARK_LOCAL_HW_FID | BNXT_ULP_MARK_VFR_ID;

	rc = ulp_mark_db_mark_add(parms->ulp_ctx, mark_flag,
				  act_idx, mark);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to add mark to flow\n");
		return rc;
	}
	fid_parms.direction = tbl->direction;
	fid_parms.resource_func = BNXT_ULP_RESOURCE_FUNC_HW_FID;
	fid_parms.critical_resource = tbl->critical_resource;
	fid_parms.resource_type	= mark_flag;
	fid_parms.resource_hndl	= act_idx;
	ulp_flow_db_shared_session_set(&fid_parms, tbl->session_type);

	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (unlikely(rc))
		BNXT_DRV_DBG(ERR, "Fail to link res to flow rc = %d\n", rc);
	return rc;
}

/* Tcam table scan the identifier list and allocate each identifier */
int32_t
ulp_mapper_tcam_tbl_ident_alloc(struct bnxt_ulp_mapper_parms *parms,
				struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct bnxt_ulp_mapper_ident_info *idents;
	uint32_t num_idents;
	uint32_t i;

	idents = ulp_mapper_ident_fields_get(parms, tbl, &num_idents);
	for (i = 0; i < num_idents; i++) {
		if (unlikely(ulp_mapper_ident_process(parms, tbl, NULL,
						      &idents[i], NULL)))
			return -EINVAL;
	}
	return 0;
}

/*
 * internal function to post process key/mask blobs for dynamic pad WC tcam tbl
 *
 * parms [in] The mappers parms with data related to the flow.
 *
 * key [in] The original key to be transformed
 *
 * mask [in] The original mask to be transformed
 *
 * tkey [in/out] The transformed key
 *
 * tmask [in/out] The transformed mask
 *
 * returns zero on success, non-zero on failure
 */
uint32_t
ulp_mapper_wc_tcam_tbl_dyn_post_process(struct bnxt_ulp_device_params *dparms,
					struct ulp_blob *key,
					struct ulp_blob *mask,
					struct ulp_blob *tkey,
					struct ulp_blob *tmask)
{
	uint16_t tlen, blen, clen, slice_width, num_slices, max_slices, offset;
	uint32_t cword, i, rc;
	int32_t pad;
	uint8_t *val;

	slice_width = dparms->wc_slice_width;
	clen = dparms->wc_ctl_size_bits;
	max_slices = dparms->wc_max_slices;
	blen = ulp_blob_data_len_get(key);

	/* Get the length of the key based on number of slices and width */
	num_slices = 1;
	tlen = slice_width;
	while (tlen < blen &&
	       num_slices <= max_slices) {
		num_slices = num_slices << 1;
		tlen = tlen << 1;
	}

	if (unlikely(num_slices > max_slices)) {
		BNXT_DRV_DBG(ERR, "Key size (%d) too large for WC\n", blen);
		return -EINVAL;
	}

	/* The key/mask may not be on a natural slice boundary, pad it */
	pad = tlen - blen;
	if (unlikely(ulp_blob_pad_push(key, pad) < 0 ||
		     ulp_blob_pad_push(mask, pad) < 0)) {
		BNXT_DRV_DBG(ERR, "Unable to pad key/mask\n");
		return -EINVAL;
	}

	/* The new length accounts for the ctrl word length and num slices */
	tlen = tlen + clen * num_slices;
	if (unlikely(ulp_blob_init(tkey, tlen, key->byte_order) ||
		     ulp_blob_init(tmask, tlen, mask->byte_order))) {
		BNXT_DRV_DBG(ERR, "Unable to post process wc tcam entry\n");
		return -EINVAL;
	}

	/* Build the transformed key/mask */
	cword = dparms->wc_mode_list[num_slices - 1];
	cword = tfp_cpu_to_be_32(cword);
	offset = 0;
	for (i = 0; i < num_slices; i++) {
		val = ulp_blob_push_32(tkey, &cword, clen);
		if (unlikely(!val)) {
			BNXT_DRV_DBG(ERR, "Key ctrl word push failed\n");
			return -EINVAL;
		}
		val = ulp_blob_push_32(tmask, &cword, clen);
		if (unlikely(!val)) {
			BNXT_DRV_DBG(ERR, "Mask ctrl word push failed\n");
			return -EINVAL;
		}
		rc = ulp_blob_append(tkey, key, offset, slice_width);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Key blob append failed\n");
			return rc;
		}
		rc = ulp_blob_append(tmask, mask, offset, slice_width);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Mask blob append failed\n");
			return rc;
		}
		offset += slice_width;
	}

	/* The key/mask are byte reversed on every 4 byte chunk */
	ulp_blob_perform_byte_reverse(tkey, 4);
	ulp_blob_perform_byte_reverse(tmask, 4);

	return 0;
}

/* Post process the key/mask blobs for wildcard tcam tbl */
void ulp_mapper_wc_tcam_tbl_post_process(struct ulp_blob *blob)
{
	ulp_blob_perform_64B_word_swap(blob);
	ulp_blob_perform_64B_byte_swap(blob);
}

static int32_t
ulp_mapper_gen_tbl_ref_cnt_process(struct bnxt_ulp_mapper_parms *parms,
				   struct bnxt_ulp_mapper_tbl_info *tbl,
				   struct ulp_mapper_gen_tbl_entry *entry)
{
	int32_t rc = 0;
	uint64_t val64;

	/* Allow the template to manage the reference count */
	switch (tbl->ref_cnt_opcode) {
	case BNXT_ULP_REF_CNT_OPC_INC:
		ULP_GEN_TBL_REF_CNT_INC(entry);
		break;
	case BNXT_ULP_REF_CNT_OPC_DEC:
		/* writes never decrement the ref count */
		if (tbl->tbl_opcode == BNXT_ULP_GENERIC_TBL_OPC_WRITE)
			return -EINVAL;

		ULP_GEN_TBL_REF_CNT_DEC(entry);
		break;
	case BNXT_ULP_REF_CNT_OPC_NOP:
		/* Nothing to be done, generally used when
		 * template gets the ref_cnt to make a decision
		 */
		break;
	case BNXT_ULP_REF_CNT_OPC_DEFAULT:
		/* This is the default case and is backward
		 * compatible with older templates
		 */
		if (tbl->fdb_opcode != BNXT_ULP_FDB_OPC_NOP)
			ULP_GEN_TBL_REF_CNT_INC(entry);
		break;
	default:
		BNXT_DRV_DBG(ERR, "Invalid REF_CNT_OPC %d\n",
			     tbl->ref_cnt_opcode);
		return -EINVAL;
	}

	if (tbl->tbl_opcode == BNXT_ULP_GENERIC_TBL_OPC_READ) {
		/* Add ref_cnt to the regfile for template to use. */
		val64 = (uint32_t)ULP_GEN_TBL_REF_CNT(entry);
		val64 = tfp_cpu_to_be_64(val64);
		rc = ulp_regfile_write(parms->regfile,
				       BNXT_ULP_RF_IDX_REF_CNT,
				       val64);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Failed to write regfile[ref_cnt]\n");
			return rc;
		}
	}

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
	enum ulp_gen_list_search_flag list_srch = ULP_GEN_LIST_SEARCH_MISSED;
	uint16_t keylen, datalen = 0;
	struct ulp_blob key, data;
	uint8_t *cache_key;
	int32_t tbl_idx;
	uint32_t i, num_kflds = 0, key_index = 0, num_par_kflds = 0, pad = 0;
	uint32_t gen_tbl_miss = 1, fdb_write = 0;
	uint8_t *byte_data;
	uint64_t regval = 0;
	int32_t rc = 0;

	/* Get the key fields list and build the key. */
	kflds = ulp_mapper_key_fields_get(parms, tbl, &num_kflds);
	if (unlikely(!kflds || !num_kflds)) {
		BNXT_DRV_DBG(ERR, "Failed to get key fields\n");
		return -EINVAL;
	}

	/* Get the partial key list number*/
	num_par_kflds = ulp_mapper_partial_key_fields_get(parms, tbl);

	if (num_par_kflds)
		pad = ULP_BYTE_2_BITS(sizeof(uint8_t)) -
		ULP_BITS_IS_BYTE_NOT_ALIGNED(tbl->key_bit_size);

	if (unlikely(ulp_blob_init(&key, tbl->key_bit_size + pad +
			  tbl->partial_key_bit_size,
				   parms->device_params->key_byte_order))) {
		BNXT_DRV_DBG(ERR, "Failed to alloc blob\n");
		return -EINVAL;
	}
	for (i = 0; i < num_kflds + num_par_kflds; i++) {
		/* Setup the key */
		rc = ulp_mapper_field_opc_process(parms, tbl->direction,
						  &kflds[i].field_info_spec,
						  &key, 1, "Gen Tbl Key");
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR,
				     "Failed to create key for Gen tbl rc=%d\n",
				     rc);
			return -EINVAL;
		}
		/* pad for the alignment between exact key and partial key */
		if (num_par_kflds && i == num_kflds - 1) {
			if (unlikely(ulp_blob_pad_push(&key, pad) < 0)) {
				BNXT_DRV_DBG(ERR, "key padding failed\n");
				return -EINVAL;
			}
		}
	}

	/* Calculate the table index for the generic table*/
	tbl_idx = ulp_mapper_gen_tbl_idx_calculate(tbl->resource_sub_type,
						   tbl->direction);
	if (unlikely(tbl_idx < 0)) {
		BNXT_DRV_DBG(ERR, "Invalid table index %x:%x\n",
			     tbl->resource_sub_type, tbl->direction);
		return -EINVAL;
	}

	/* The_key is a byte array convert it to a search index */
	cache_key = ulp_blob_data_get(&key, &keylen);

	/* get the generic table  */
	gen_tbl_list = &parms->mapper_data->gen_tbl_list[tbl_idx];

	/* perform basic validation of generic table */
	if (unlikely((gen_tbl_list->tbl_type == BNXT_ULP_GEN_TBL_TYPE_HASH_LIST &&
		      gen_tbl_list->hash_tbl == NULL) ||
		     gen_tbl_list->mem_data == NULL)) {
		BNXT_DRV_DBG(ERR, "Uninitialized gen table index %x:%x\n",
			     tbl->resource_sub_type, tbl->direction);
		return -EINVAL;
	}

	/* Check if generic hash table */
	if (gen_tbl_list->tbl_type == BNXT_ULP_GEN_TBL_TYPE_HASH_LIST) {
		if (unlikely(tbl->gen_tbl_lkup_type !=
			     BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH)) {
			BNXT_DRV_DBG(ERR, "%s: Invalid template lkup type\n",
				     gen_tbl_list->gen_tbl_name);
			return -EINVAL;
		}
		hash_entry.key_data = cache_key;
		hash_entry.key_length = ULP_BITS_2_BYTE(keylen);
		rc = ulp_gen_hash_tbl_list_key_search(gen_tbl_list->hash_tbl,
						      &hash_entry);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "%s: hash tbl search failed\n",
				     gen_tbl_list->gen_tbl_name);
			return rc;
		}
		if (hash_entry.search_flag == ULP_GEN_HASH_SEARCH_FOUND) {
			key_index = hash_entry.key_idx;
			/* Get the generic table entry */
			if (unlikely(ulp_mapper_gen_tbl_entry_get(gen_tbl_list,
								  key_index,
								  &gen_tbl_ent)))
				return -EINVAL;
			/* store the hash index in the fdb */
			key_index = hash_entry.hash_index;
		}
	} else if (gen_tbl_list->tbl_type == BNXT_ULP_GEN_TBL_TYPE_KEY_LIST) {
		/* convert key to index directly */
		if (unlikely(ULP_BITS_2_BYTE(keylen) > (int32_t)sizeof(key_index))) {
			BNXT_DRV_DBG(ERR, "%s: keysize is bigger then 4 bytes\n",
				     gen_tbl_list->gen_tbl_name);
			return -EINVAL;
		}
		memcpy(&key_index, cache_key, ULP_BITS_2_BYTE(keylen));
		/* Get the generic table entry */
		if (unlikely(ulp_mapper_gen_tbl_entry_get(gen_tbl_list, key_index,
							  &gen_tbl_ent)))
			return -EINVAL;
	} else if (gen_tbl_list->tbl_type ==
		   BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST &&
		   tbl->tbl_opcode !=
		   BNXT_ULP_GENERIC_TBL_OPC_ITERATE) {
		list_srch = ulp_gen_tbl_simple_list_search(gen_tbl_list,
							   cache_key,
							   &key_index);
		/* Get the generic table entry */
		if (unlikely(ulp_mapper_gen_tbl_entry_get(gen_tbl_list,
							  key_index,
							  &gen_tbl_ent)))
			return -EINVAL;
	}

	switch (tbl->tbl_opcode) {
	case BNXT_ULP_GENERIC_TBL_OPC_READ:
		if (gen_tbl_list->tbl_type == BNXT_ULP_GEN_TBL_TYPE_HASH_LIST &&
		    gen_tbl_list->hash_tbl) {
			if (hash_entry.search_flag != ULP_GEN_HASH_SEARCH_FOUND)
				break; /* nothing to be done , no entry */
		} else if (gen_tbl_list->tbl_type ==
			   BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST) {
			if (list_srch == ULP_GEN_LIST_SEARCH_MISSED ||
			    list_srch == ULP_GEN_LIST_SEARCH_FULL)
				break;
		}

		/* check the reference count */
		if (ULP_GEN_TBL_REF_CNT(&gen_tbl_ent)) {
			g = &gen_tbl_ent;
			/* Scan ident list and create the result blob*/
			rc = ulp_mapper_tbl_ident_scan_ext(parms, tbl,
							   g->byte_data,
							   g->byte_data_size,
							   g->byte_order);
			if (unlikely(rc)) {
				BNXT_DRV_DBG(ERR,
					     "Failed to scan ident list\n");
				return -EINVAL;
			}

			/* it is a hit */
			gen_tbl_miss = 0;
			fdb_write = 1;
		}
		break;
	case BNXT_ULP_GENERIC_TBL_OPC_WRITE:
		if (gen_tbl_list->tbl_type == BNXT_ULP_GEN_TBL_TYPE_HASH_LIST &&
		    gen_tbl_list->hash_tbl) {
			rc = ulp_mapper_gen_tbl_hash_entry_add(gen_tbl_list,
							       &hash_entry,
							       &gen_tbl_ent);
			if (unlikely(rc))
				return rc;
			/* store the hash index in the fdb */
			key_index = hash_entry.hash_index;
		} else if (gen_tbl_list->tbl_type ==
			   BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST) {
			if (unlikely(list_srch == ULP_GEN_LIST_SEARCH_FULL)) {
				BNXT_DRV_DBG(ERR, "failed to add gen entry\n");
				return -ENOMEM;
			}
		}

		/* check the reference count and ignore ref_cnt if NOP.
		 * NOP allows a write as an update.
		 */
		if (unlikely(tbl->ref_cnt_opcode != BNXT_ULP_REF_CNT_OPC_NOP &&
			     ULP_GEN_TBL_REF_CNT(&gen_tbl_ent))) {
			/* a hit then error */
			BNXT_DRV_DBG(ERR, "generic entry already present\n");
			return -EINVAL; /* success */
		}

		/* Initialize the blob data */
		if (unlikely(ulp_blob_init(&data, tbl->result_bit_size,
					   gen_tbl_ent.byte_order))) {
			BNXT_DRV_DBG(ERR, "Failed initial index table blob\n");
			return -EINVAL;
		}

		/* Get the result fields list */
		rc = ulp_mapper_tbl_result_build(parms, tbl, &data,
						 "Gen tbl Result");
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Failed to build the result blob\n");
			return rc;
		}
		byte_data = ulp_blob_data_get(&data, &datalen);
		rc = ulp_mapper_gen_tbl_entry_data_set(gen_tbl_list,
						       &gen_tbl_ent,
						       cache_key,
						       ULP_BITS_2_BYTE(keylen),
						       byte_data,
						       ULP_BITS_2_BYTE(datalen)
						       );
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Failed to write generic table\n");
			return -EINVAL;
		}

		fdb_write = 1;
		parms->shared_hndl = (uint64_t)tbl_idx << 32 | key_index;
		break;
	case BNXT_ULP_GENERIC_TBL_OPC_ITERATE:
		if (gen_tbl_list->tbl_type !=
		    BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST) {
			BNXT_DRV_DBG(ERR, "%s: Invalid table opcode\n",
				     gen_tbl_list->gen_tbl_name);
			return -EINVAL;
		}
		/* read the gen table index */
		if (unlikely(ulp_regfile_read(parms->regfile,
					      tbl->tbl_operand,
					      &regval))) {
			BNXT_DRV_DBG(ERR,
				     "Fail to get tbl idx from regfile[%d].\n",
				     BNXT_ULP_RF_IDX_GENERIC_TBL_INDEX);
			return -EINVAL;
		}
		key_index = (uint32_t)rte_be_to_cpu_64(regval);
		/* get the next index from the simple list table */
		rc = ulp_gen_tbl_simple_list_get_next(gen_tbl_list, &key_index);
		if (rc == ULP_GEN_LIST_SEARCH_FOUND) {
			gen_tbl_miss = 0; /* entry exits */
			regval = key_index;
			(void)ulp_regfile_write(parms->regfile,
						tbl->tbl_operand,
						tfp_cpu_to_be_64(regval));
			g = &gen_tbl_ent;
			rc = ulp_mapper_gen_tbl_entry_get(gen_tbl_list,
							  key_index,
							  &gen_tbl_ent);
			if (rc)
				return rc;

			/* scan the result list and update the regfile values */
			rc = ulp_mapper_tbl_ident_scan_ext(parms, tbl,
							   g->byte_data,
							   g->byte_data_size,
							   g->byte_order);
			if (unlikely(rc)) {
				BNXT_DRV_DBG(ERR, "Fail to scan ident list\n");
				return rc;
			}

		} else {
			gen_tbl_miss = 1; /* no more entries */
		}
		fdb_write = 0;
		break;
	default:
		BNXT_DRV_DBG(ERR, "Invalid table opcode %x\n", tbl->tbl_opcode);
		return -EINVAL;
	}

	/* Set the generic entry hit */
	rc = ulp_regfile_write(parms->regfile,
			       BNXT_ULP_RF_IDX_GENERIC_TBL_MISS,
			       tfp_cpu_to_be_64(gen_tbl_miss));
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Write regfile[%d] failed\n",
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
		ulp_flow_db_shared_session_set(&fid_parms, tbl->session_type);

		rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Fail to add gen ent flowdb %d\n",
				     rc);
			return rc;
		}

		/* Reset the in-flight RID when generic table is written and the
		 * rid has been pushed into a handle (rid or fid).  Once it has
		 * been written, we have persistent accounting of the resources.
		 */
		if (tbl->tbl_opcode == BNXT_ULP_GENERIC_TBL_OPC_WRITE &&
		    (tbl->fdb_opcode == BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE ||
		     tbl->fdb_opcode == BNXT_ULP_FDB_OPC_PUSH_FID))
			parms->rid = 0;

		rc = ulp_mapper_gen_tbl_ref_cnt_process(parms, tbl,
							&gen_tbl_ent);
	}

	return rc;
}

static int32_t
ulp_mapper_ctrl_tbl_process(struct bnxt_ulp_mapper_parms *parms,
			    struct bnxt_ulp_mapper_tbl_info *tbl)
{
	int32_t rc = 0;
	uint64_t val64 = 0;
	uint32_t rid;

	/* process the fdb opcode for alloc push */
	if (tbl->fdb_opcode == BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE) {
		rc = ulp_mapper_fdb_opc_alloc_rid(parms, tbl);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Failed to do fdb alloc\n");
			return rc;
		}
	} else if (tbl->fdb_opcode == BNXT_ULP_FDB_OPC_DELETE_RID_REGFILE) {
		rc = ulp_regfile_read(parms->regfile, tbl->fdb_operand, &val64);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Failed to get RID from regfile\n");
			return rc;
		}
		rid = (uint32_t)tfp_be_to_cpu_64(val64);
		rc = ulp_mapper_resources_free(parms->ulp_ctx,
					       BNXT_ULP_FDB_TYPE_RID,
					       rid,
					       NULL);
	}

	return rc;
}

static int32_t
ulp_mapper_vnic_tbl_process(struct bnxt_ulp_mapper_parms *parms,
			    struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct ulp_flow_db_res_params fid_parms;
	uint16_t vnic_idx = 0, vnic_id = 0;
	int32_t rc = 0;

	switch (tbl->resource_sub_type) {
	case BNXT_ULP_RESOURCE_SUB_TYPE_VNIC_TABLE_RSS:
		if (unlikely(tbl->tbl_opcode != BNXT_ULP_VNIC_TBL_OPC_ALLOC_WR_REGFILE)) {
			BNXT_DRV_DBG(ERR, "Invalid vnic table opcode\n");
			return -EINVAL;
		}
		rc = bnxt_pmd_rss_action_create(parms, &vnic_idx, &vnic_id);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Failed create rss action\n");
			return rc;
		}
		break;
	case BNXT_ULP_RESOURCE_SUB_TYPE_VNIC_TABLE_QUEUE:
		if (unlikely(tbl->tbl_opcode != BNXT_ULP_VNIC_TBL_OPC_ALLOC_WR_REGFILE)) {
			BNXT_DRV_DBG(ERR, "Invalid vnic table opcode\n");
			return -EINVAL;
		}
		rc = bnxt_pmd_queue_action_create(parms, &vnic_idx, &vnic_id);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Failed create queue action\n");
			return rc;
		}
		break;
	default:
		BNXT_DRV_DBG(ERR, "Invalid vnic table sub type\n");
		return -EINVAL;
	}

	/* Link the created vnic to the flow in the flow db */
	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.direction	= tbl->direction;
	fid_parms.resource_func	= tbl->resource_func;
	fid_parms.resource_type	= tbl->resource_type;
	fid_parms.resource_sub_type = tbl->resource_sub_type;
	fid_parms.resource_hndl	= vnic_idx;
	fid_parms.critical_resource = tbl->critical_resource;
	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to link resource to flow rc = %d\n",
			     rc);
		return rc;
	}
	rc = ulp_regfile_write(parms->regfile, tbl->tbl_operand,
			       (uint64_t)tfp_cpu_to_be_64(vnic_id));
	if (unlikely(rc))
		BNXT_DRV_DBG(ERR, "Failed to write regfile[%d] rc=%d\n",
			     tbl->tbl_operand, rc);

	return rc;
}

static int32_t
ulp_mapper_stats_cache_tbl_process(struct bnxt_ulp_mapper_parms *parms,
				   struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct ulp_flow_db_res_params fid_parms;
	uint64_t counter_handle;
	struct ulp_blob	data;
	uint16_t data_len = 0;
	uint8_t *tmp_data;
	int32_t rc = 0;

	/* Initialize the blob data */
	if (unlikely(ulp_blob_init(&data, tbl->result_bit_size,
				   BNXT_ULP_BYTE_ORDER_BE))) {
		BNXT_DRV_DBG(ERR, "Failed initial ulp_global table blob\n");
		return -EINVAL;
	}

	/* read the arguments from the result table */
	rc = ulp_mapper_tbl_result_build(parms, tbl, &data,
					 "ULP Global Result");
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to build the result blob\n");
		return rc;
	}

	tmp_data = ulp_blob_data_get(&data, &data_len);
	counter_handle = *(uint64_t *)tmp_data;
	counter_handle = tfp_be_to_cpu_64(counter_handle);

	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.direction	= tbl->direction;
	fid_parms.resource_func	= tbl->resource_func;
	fid_parms.resource_type	= tbl->resource_type;
	fid_parms.resource_sub_type = tbl->resource_sub_type;
	fid_parms.resource_hndl	    = counter_handle;
	fid_parms.critical_resource = tbl->critical_resource;
	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to link resource to flow rc = %d\n",
			     rc);
		return rc;
	}

	rc = ulp_sc_mgr_entry_alloc(parms, counter_handle, tbl);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to link resource to flow rc = %d\n",
			     rc);
		return rc;
	}
	return rc;
}

static int32_t
ulp_mapper_stats_cache_tbl_res_free(struct bnxt_ulp_context *ulp,
				    uint32_t fid)
{
	ulp_sc_mgr_entry_free(ulp, fid);
	return 0;
}

static int32_t
ulp_mapper_global_identifier_process(struct bnxt_ulp_mapper_parms *parms,
				     struct bnxt_ulp_mapper_tbl_info *tbl)
{
	int32_t rc = 0;
	struct bnxt_ulp_mapper_ident_info *idents;
	struct bnxt_ulp_mapper_key_info	*kflds;
	struct ulp_blob key;
	uint32_t num_idents;
	uint32_t num_kflds;
	uint32_t i;

	/* check the table opcode  */
	if (tbl->tbl_opcode != BNXT_ULP_GLOBAL_IDENTIFIER_TBL_OPC_ALLOC) {
		BNXT_DRV_DBG(ERR, "Invalid global ident table opcode %d\n",
			     tbl->tbl_opcode);
		return -EINVAL;
	}

	/* Create the key blob */
	if (unlikely(ulp_blob_init(&key, tbl->blob_key_bit_size,
				   BNXT_ULP_BYTE_ORDER_BE))) {
		BNXT_DRV_DBG(ERR, "blob init failed.\n");
		return -EINVAL;
	}

	kflds = ulp_mapper_key_fields_get(parms, tbl, &num_kflds);
	for (i = 0; i < num_kflds; i++) {
		rc = ulp_mapper_field_opc_process(parms, tbl->direction,
						  &kflds[i].field_info_spec,
						  &key, 1, "Global Id Context");
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Key field set failed %s\n",
				     kflds[i].field_info_spec.description);
			return rc;
		}
	}

	/* Get the identifiers to process it */
	idents = ulp_mapper_ident_fields_get(parms, tbl, &num_idents);
	for (i = 0; i < num_idents; i++) {
		if (unlikely(ulp_mapper_ident_process(parms, tbl, &key,
						      &idents[i], NULL)))
			return -EINVAL;
	}

	return rc;
}

static int32_t
ulp_mapper_global_idx_tbl_process(struct bnxt_ulp_mapper_parms *parms,
				  struct bnxt_ulp_mapper_tbl_info *tbl)
{
	const struct ulp_mapper_core_ops *op = parms->mapper_data->mapper_oper;
	struct bnxt_ulp_glb_resource_info glb_res = { 0 };
	struct ulp_flow_db_res_params fid_parms = { 0 };
	struct bnxt_ulp_mapper_key_info	*kflds;
	struct ulp_blob key;
	uint32_t num_kflds = 0;
	uint16_t tmplen = 0;
	uint64_t idx = 0;
	uint8_t *context;
	int32_t rc = 0;
	uint32_t i;

	/* check the table opcode  */
	if (tbl->tbl_opcode != BNXT_ULP_GLOBAL_IDX_TBL_OPC_ALLOC) {
		BNXT_DRV_DBG(ERR, "Invalid global idx table opcode %d",
			     tbl->tbl_opcode);
		return -EINVAL;
	}

	/* Create the key blob */
	if (unlikely(ulp_blob_init(&key, tbl->blob_key_bit_size,
				   BNXT_ULP_BYTE_ORDER_BE))) {
		BNXT_DRV_DBG(ERR, "blob init failed.");
		return -EINVAL;
	}

	kflds = ulp_mapper_key_fields_get(parms, tbl, &num_kflds);
	for (i = 0; i < num_kflds; i++) {
		rc = ulp_mapper_field_opc_process(parms, tbl->direction,
						  &kflds[i].field_info_spec,
						  &key, 1,
						  "Global Idx Context");
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Key field set failed %s",
				     kflds[i].field_info_spec.description);
			return rc;
		}
	}

	context = ulp_blob_data_get(&key, &tmplen);
	tmplen = ULP_BITS_2_BYTE(tmplen);

	if (unlikely(!op->ulp_mapper_core_glb_idx_tbl_alloc)) {
		BNXT_DRV_DBG(ERR, "global idx tbl process not supported");
		return -EINVAL;
	}

	rc = op->ulp_mapper_core_glb_idx_tbl_alloc(parms->ulp_ctx,
						   tbl->resource_type,
						   tbl->direction, context,
						   tmplen, &idx);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "global idx tbl process failed");
		return rc;
	}

	/* Add the table index to the flow db */
	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.direction = tbl->direction;
	fid_parms.resource_func = tbl->resource_func;
	fid_parms.resource_type	= tbl->resource_type;
	fid_parms.critical_resource = tbl->critical_resource;
	fid_parms.resource_hndl = idx;

	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Fail to link res to flow rc = %d", rc);
		goto error;
	}

	rc = bnxt_ulp_cntxt_dev_id_get(parms->ulp_ctx, &glb_res.device_id);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to get device id (%d)", rc);
		goto error;
	}

	rc = bnxt_ulp_cntxt_app_id_get(parms->ulp_ctx, &glb_res.app_id);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to get app id (%d)", rc);
		goto error;
	}

	glb_res.direction = tbl->direction;
	glb_res.resource_func = tbl->resource_func;
	glb_res.resource_type = tbl->resource_type;
	glb_res.glb_regfile_index = tbl->tbl_operand;
	/* Write the table index into the regfile*/
	if (ulp_mapper_glb_resource_write(parms->mapper_data, &glb_res,
					  tfp_cpu_to_be_64(idx), false)) {
		BNXT_DRV_DBG(ERR, "Glb Regfile[%d] write failed.",
			     tbl->tbl_operand);
		rc = -EINVAL;
		goto error;
	}
	return rc;
error:
	(void)op->ulp_mapper_core_glb_idx_tbl_free(parms->ulp_ctx, &fid_parms);
	return rc;
}

/* Free the vnic resource */
static int32_t
ulp_mapper_vnic_tbl_res_free(struct bnxt_ulp_context *ulp __rte_unused,
			     struct bnxt *bp,
			     struct ulp_flow_db_res_params *res)
{
	uint16_t vnic_idx = res->resource_hndl;

	if (res->resource_sub_type ==
	    BNXT_ULP_RESOURCE_SUB_TYPE_VNIC_TABLE_QUEUE)
		return bnxt_pmd_queue_action_delete(bp, vnic_idx);
	else
		return bnxt_pmd_rss_action_delete(bp, vnic_idx);
}

static int32_t
ulp_mapper_global_res_free(struct bnxt_ulp_context *ulp,
			   struct bnxt *bp __rte_unused,
			   struct ulp_flow_db_res_params *res)
{
	uint64_t handle = res->resource_hndl;

	return bnxt_pmd_global_tunnel_set(ulp, 0, res->resource_sub_type,
					  0, &handle);
}

static int32_t
ulp_mapper_global_register_tbl_process(struct bnxt_ulp_mapper_parms *parms,
				       struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct ulp_flow_db_res_params fid_parms	= { 0 };
	struct ulp_blob	data;
	uint16_t data_len = 0;
	uint8_t *tmp_data;
	uint16_t udp_port;
	uint64_t handle;
	int32_t rc = 0, write_reg = 0;

	/* Initialize the blob data */
	if (unlikely(ulp_blob_init(&data, tbl->result_bit_size,
				   BNXT_ULP_BYTE_ORDER_BE))) {
		BNXT_DRV_DBG(ERR, "Failed initial ulp_global table blob\n");
		return -EINVAL;
	}

	/* read the arguments from the result table */
	rc = ulp_mapper_tbl_result_build(parms, tbl, &data,
					 "ULP Global Result");
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to build the result blob\n");
		return rc;
	}

	switch (tbl->tbl_opcode) {
	case BNXT_ULP_GLOBAL_REGISTER_TBL_OPC_WR_REGFILE:
		write_reg = 1;
		break;
	case BNXT_ULP_GLOBAL_REGISTER_TBL_OPC_NOT_USED:
		break;
	default:
		BNXT_DRV_DBG(ERR, "Invalid global table opcode %d\n",
			     tbl->tbl_opcode);
		return -EINVAL;
	}

	tmp_data = ulp_blob_data_get(&data, &data_len);
	udp_port = *((uint16_t *)tmp_data);
	udp_port = tfp_be_to_cpu_16(udp_port);

	rc = bnxt_pmd_global_tunnel_set(parms->ulp_ctx,
					parms->port_id, tbl->resource_sub_type,
					udp_port, &handle);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Unable to set Type %d port\n",
			     tbl->resource_sub_type);
		return rc;
	}

	/* Set the common pieces of fid parms */
	fid_parms.direction = tbl->direction;
	fid_parms.resource_func	= tbl->resource_func;
	fid_parms.resource_sub_type = tbl->resource_sub_type;
	fid_parms.critical_resource = tbl->critical_resource;
	fid_parms.resource_hndl = handle;

	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);

	if (unlikely(rc))
		return rc;

	/* write to the regfile if opcode is set */
	if (write_reg) {
		rc = ulp_regfile_write(parms->regfile,
				       tbl->tbl_operand,
				       tfp_cpu_to_be_64(handle));
		if (rc)
			BNXT_DRV_DBG(ERR, "Regfile[%d] write failed.\n",
				     tbl->tbl_operand);
	}

	return rc;
}

static int32_t
ulp_mapper_glb_resource_info_init(struct bnxt_ulp_context *ulp_ctx,
				  struct bnxt_ulp_mapper_data *mapper_data)
{
	struct bnxt_ulp_glb_resource_info *glb_res;
	uint32_t num_entries = 0, idx, dev_id;
	uint8_t app_id;
	int32_t rc = 0;

	glb_res = ulp_mapper_glb_resource_info_list_get(&num_entries);
	/* Check if there are no resources */
	if (!num_entries)
		return 0;

	rc = bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to get device id for glb init (%d)\n",
			     rc);
		return rc;
	}

	rc = bnxt_ulp_cntxt_app_id_get(ulp_ctx, &app_id);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to get app id for glb init (%d)\n",
			     rc);
		return rc;
	}

	/* Iterate the global resources and process each one */
	for (idx = 0; idx < num_entries; idx++) {
		if (dev_id != glb_res[idx].device_id ||
		    glb_res[idx].app_id != app_id)
			continue;
		switch (glb_res[idx].resource_func) {
		case BNXT_ULP_RESOURCE_FUNC_IDENTIFIER:
			rc = ulp_mapper_resource_ident_allocate(ulp_ctx,
								mapper_data,
								&glb_res[idx],
								false);
			break;
		case BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE:
			rc = ulp_mapper_resource_index_tbl_alloc(ulp_ctx,
								 mapper_data,
								 &glb_res[idx],
								 false);
			break;
		default:
			BNXT_DRV_DBG(ERR, "Global resource %x not supported\n",
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
 * Iterate over the shared resources assigned during tf_open_session and store
 * them in the global regfile with the shared flag.
 */
static int32_t
ulp_mapper_app_glb_resource_info_init(struct bnxt_ulp_context *ulp_ctx,
				      struct bnxt_ulp_mapper_data *mapper_data)
{
	const struct ulp_mapper_core_ops *op = mapper_data->mapper_oper;

	return op->ulp_mapper_core_app_glb_res_info_init(ulp_ctx, mapper_data);
}

/*
 * Common conditional opcode process routine that is used for both the template
 * rejection and table conditional execution.
 */
static int32_t
ulp_mapper_cond_opc_process(struct bnxt_ulp_mapper_parms *parms,
			    enum bnxt_ulp_cond_opc opc,
			    uint64_t operand,
			    int32_t *res)
{
	enum bnxt_ulp_flow_mem_type mtype = BNXT_ULP_FLOW_MEM_TYPE_INT;
	uint32_t field_size = 0;
	int32_t rc = 0;
	uint8_t bit, tmp;
	uint64_t regval = 0, result = 0;

	switch (opc) {
	case BNXT_ULP_COND_OPC_CF_IS_SET:
		if (operand < BNXT_ULP_CF_IDX_LAST) {
			result = ULP_COMP_FLD_IDX_RD(parms, operand);
		} else {
			BNXT_DRV_DBG(ERR,
				     "comp field out of bounds %" PRIu64 "\n",
				     operand);
			rc = -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_CF_NOT_SET:
		if (likely(operand < BNXT_ULP_CF_IDX_LAST)) {
			result = !ULP_COMP_FLD_IDX_RD(parms, operand);
		} else {
			BNXT_DRV_DBG(ERR,
				     "comp field out of bounds %" PRIu64 "\n",
				     operand);
			rc = -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_ACT_BIT_IS_SET:
		if (likely(operand < BNXT_ULP_ACT_BIT_LAST)) {
			result = ULP_BITMAP_ISSET(parms->act_bitmap->bits,
						operand);
		} else {
			BNXT_DRV_DBG(ERR,
				     "action bit out of bounds %" PRIu64 "\n",
				     operand);
			rc = -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_ACT_BIT_NOT_SET:
		if (likely(operand < BNXT_ULP_ACT_BIT_LAST)) {
			result = !ULP_BITMAP_ISSET(parms->act_bitmap->bits,
					       operand);
		} else {
			BNXT_DRV_DBG(ERR,
				     "action bit out of bounds %" PRIu64 "\n",
				     operand);
			rc = -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_HDR_BIT_IS_SET:
		if (likely(operand < BNXT_ULP_HDR_BIT_LAST)) {
			result = ULP_BITMAP_ISSET(parms->hdr_bitmap->bits,
						operand);
		} else {
			BNXT_DRV_DBG(ERR,
				     "header bit out of bounds %" PRIu64 "\n",
				     operand);
			rc = -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_HDR_BIT_NOT_SET:
		if (likely(operand < BNXT_ULP_HDR_BIT_LAST)) {
			result = !ULP_BITMAP_ISSET(parms->hdr_bitmap->bits,
					       operand);
		} else {
			BNXT_DRV_DBG(ERR,
				     "header bit out of bounds %" PRIu64 "\n",
				     operand);
			rc = -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_FIELD_BIT_IS_SET:
		rc = ulp_mapper_glb_field_tbl_get(parms, operand, &bit);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR,
				     "invalid ulp_glb_field_tbl idx %" PRIu64 "\n",
				     operand);
			return -EINVAL;
		}
		result = ULP_INDEX_BITMAP_GET(parms->fld_bitmap->bits, bit);
		break;
	case BNXT_ULP_COND_OPC_FIELD_BIT_NOT_SET:
		rc = ulp_mapper_glb_field_tbl_get(parms, operand, &bit);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR,
				     "invalid ulp_glb_field_tbl idx %" PRIu64 "\n",
				     operand);
			return -EINVAL;
		}
		result = !ULP_INDEX_BITMAP_GET(parms->fld_bitmap->bits, bit);
		break;
	case BNXT_ULP_COND_OPC_RF_IS_SET:
		if (ulp_regfile_read(parms->regfile, operand, &regval)) {
			BNXT_DRV_DBG(ERR,
				     "regfile[%" PRIu64 "] read oob\n",
				     operand);
			return -EINVAL;
		}
		result = regval != 0;
		break;
	case BNXT_ULP_COND_OPC_RF_NOT_SET:
		if (unlikely(ulp_regfile_read(parms->regfile, operand, &regval))) {
			BNXT_DRV_DBG(ERR,
				    "regfile[%" PRIu64 "] read oob\n", operand);
			return -EINVAL;
		}
		result = regval == 0;
		break;
	case BNXT_ULP_COND_OPC_FLOW_PAT_MATCH:
		result = parms->flow_pattern_id == operand;
		break;
	case BNXT_ULP_COND_OPC_ACT_PAT_MATCH:
		result = parms->act_pattern_id == operand;
		break;
	case BNXT_ULP_COND_OPC_EXT_MEM_IS_SET:
		if (unlikely(bnxt_ulp_cntxt_mem_type_get(parms->ulp_ctx, &mtype))) {
			BNXT_DRV_DBG(ERR, "Failed to get the mem type\n");
			return -EINVAL;
		}
		result = (mtype == BNXT_ULP_FLOW_MEM_TYPE_INT) ? 0 : 1;
		break;
	case BNXT_ULP_COND_OPC_EXT_MEM_NOT_SET:
		if (unlikely(bnxt_ulp_cntxt_mem_type_get(parms->ulp_ctx, &mtype))) {
			BNXT_DRV_DBG(ERR, "Failed to get the mem type\n");
			return -EINVAL;
		}
		result = (mtype == BNXT_ULP_FLOW_MEM_TYPE_INT) ? 1 : 0;
		break;
	case BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET:
		if (likely(operand < BNXT_ULP_HDR_BIT_LAST)) {
			result = ULP_BITMAP_ISSET(parms->enc_hdr_bitmap->bits,
						operand);
		} else {
			BNXT_DRV_DBG(ERR,
				     "header bit out of bounds %" PRIu64 "\n",
				     operand);
			rc = -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_ENC_HDR_BIT_NOT_SET:
		if (likely(operand < BNXT_ULP_HDR_BIT_LAST)) {
			result = !ULP_BITMAP_ISSET(parms->enc_hdr_bitmap->bits,
						 operand);
		} else {
			BNXT_DRV_DBG(ERR,
				     "header bit out of bounds %" PRIu64 "\n",
				     operand);
			rc = -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_ACT_PROP_IS_SET:
	case BNXT_ULP_COND_OPC_ACT_PROP_NOT_SET:
		/* only supporting 1-byte action properties for now */
		if (unlikely(operand >= BNXT_ULP_ACT_PROP_IDX_LAST)) {
			BNXT_DRV_DBG(ERR,
				     "act_prop[%" PRIu64 "] oob\n", operand);
			return -EINVAL;
		}
		field_size = ulp_mapper_act_prop_size_get(operand);
		if (unlikely(sizeof(tmp) != field_size)) {
			BNXT_DRV_DBG(ERR,
				     "act_prop[%" PRIu64 "] field mismatch %u\n",
				     operand, field_size);
			return -EINVAL;
		}
		tmp = parms->act_prop->act_details[operand];
		if (opc == BNXT_ULP_COND_OPC_ACT_PROP_IS_SET)
			result = (int32_t)(tmp);
		else
			result = (int32_t)(!tmp);
		break;
	case BNXT_ULP_COND_OPC_CF_BIT_IS_SET:
	case BNXT_ULP_COND_OPC_CF_BIT_NOT_SET:
		if (likely(operand < BNXT_ULP_CF_BIT_LAST)) {
			result = ULP_BITMAP_ISSET(parms->cf_bitmap, operand);
		} else {
			BNXT_DRV_DBG(ERR,
				     "CF bit out of bounds %" PRIx64 "\n",
				     operand);
			rc = -EINVAL;
		}
		if (opc == BNXT_ULP_COND_OPC_CF_BIT_NOT_SET)
			result = !result;
		break;
	case BNXT_ULP_COND_OPC_WC_FIELD_BIT_IS_SET:
	case BNXT_ULP_COND_OPC_WC_FIELD_BIT_NOT_SET:
		rc = ulp_mapper_glb_field_tbl_get(parms, operand, &bit);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR,
				     "invalid ulp_glb_field idx %" PRIu64 "\n",
				     operand);
			return -EINVAL;
		}
		result = ULP_INDEX_BITMAP_GET(parms->wc_field_bitmap, bit);
		if (opc == BNXT_ULP_COND_OPC_WC_FIELD_BIT_NOT_SET)
			result = !result;
		break;
	case BNXT_ULP_COND_OPC_EXCLUDE_FIELD_BIT_IS_SET:
	case BNXT_ULP_COND_OPC_EXCLUDE_FIELD_BIT_NOT_SET:
		rc = ulp_mapper_glb_field_tbl_get(parms, operand, &bit);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR,
				     "invalid ulp_glb_field idx %" PRIu64 "\n",
				     operand);
			return -EINVAL;
		}
		result = ULP_INDEX_BITMAP_GET(parms->exclude_field_bitmap, bit);
		if (opc == BNXT_ULP_COND_OPC_EXCLUDE_FIELD_BIT_NOT_SET)
			result = !result;
		break;
	case BNXT_ULP_COND_OPC_FEATURE_BIT_IS_SET:
	case BNXT_ULP_COND_OPC_FEATURE_BIT_NOT_SET:
		regval = bnxt_ulp_feature_bits_get(parms->ulp_ctx);
		result = ULP_BITMAP_ISSET(regval, operand);
		if (opc == BNXT_ULP_COND_OPC_FEATURE_BIT_NOT_SET)
			result = !ULP_BITMAP_ISSET(regval, operand);
		break;
	default:
		BNXT_DRV_DBG(ERR, "Invalid conditional opcode %d\n", opc);
		rc = -EINVAL;
		break;
	}

	*res = !!result;
	return (rc);
}

static int32_t
ulp_mapper_func_opr_compute(struct bnxt_ulp_mapper_parms *parms,
			    enum tf_dir dir,
			    enum bnxt_ulp_func_src func_src,
			    uint64_t func_opr,
			    uint64_t *result)
{
	uint64_t regval;
	bool shared;

	*result =  false;
	switch (func_src) {
	case BNXT_ULP_FUNC_SRC_COMP_FIELD:
		if (unlikely(func_opr >= BNXT_ULP_CF_IDX_LAST)) {
			BNXT_DRV_DBG(ERR, "invalid index %u\n",
						(uint32_t)func_opr);
			return -EINVAL;
		}
		*result = ULP_COMP_FLD_IDX_RD(parms, func_opr);
		break;
	case BNXT_ULP_FUNC_SRC_REGFILE:
		if (unlikely(ulp_regfile_read(parms->regfile, func_opr, &regval))) {
			BNXT_DRV_DBG(ERR, "regfile[%d] read oob\n",
						(uint32_t)func_opr);
			return -EINVAL;
		}
		*result = tfp_be_to_cpu_64(regval);
		break;
	case BNXT_ULP_FUNC_SRC_GLB_REGFILE:
		if (unlikely(ulp_mapper_glb_resource_read(parms->mapper_data, dir,
							  func_opr, &regval, &shared))) {
			BNXT_DRV_DBG(ERR, "global regfile[%d] read failed.\n",
						(uint32_t)func_opr);
			return -EINVAL;
		}
		*result = tfp_be_to_cpu_64(regval);
		break;
	case BNXT_ULP_FUNC_SRC_CONST:
		*result = func_opr;
		break;
	case BNXT_ULP_FUNC_SRC_ACTION_BITMAP:
		*result = parms->act_bitmap->bits;
		break;
	case BNXT_ULP_FUNC_SRC_HEADER_BITMAP:
		*result = parms->hdr_bitmap->bits;
		break;
	default:
		BNXT_DRV_DBG(ERR, "invalid src code %u\n", func_src);
		return -EINVAL;
	}
	return 0;
}

static int32_t
ulp_mapper_vfr_mark_set(struct bnxt_ulp_mapper_parms *parms,
			uint32_t key, uint16_t port_id,
			struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct ulp_flow_db_res_params fid_parms;
	uint32_t mark_flag;
	int32_t rc;

	/* Set the mark flag to local fid and vfr flag */
	mark_flag  = BNXT_ULP_MARK_LOCAL_HW_FID | BNXT_ULP_MARK_VFR_ID;

	rc = ulp_mark_db_mark_add(parms->ulp_ctx, mark_flag,
				  key, port_id);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to add mark to flow\n");
		return rc;
	}
	fid_parms.direction = tbl->direction;
	fid_parms.resource_func = BNXT_ULP_RESOURCE_FUNC_HW_FID;
	fid_parms.critical_resource = tbl->critical_resource;
	fid_parms.resource_type	= mark_flag;
	fid_parms.resource_hndl	= key;
	fid_parms.resource_sub_type = 0;
	ulp_flow_db_shared_session_set(&fid_parms, tbl->session_type);

	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (rc) {
		int32_t trc = 0;
		BNXT_DRV_DBG(ERR, "Fail to link res to flow rc = %d\n", rc);
		trc = ulp_mark_db_mark_del(parms->ulp_ctx, mark_flag, key);
		if (trc)
			BNXT_DRV_DBG(ERR,
				     "Failed to cleanup mark rc = %d\n", rc);
	}
	return rc;
}

static int32_t
ulp_mapper_bd_act_set(struct bnxt_ulp_mapper_parms *parms __rte_unused,
		      uint16_t port_id, uint32_t action)
{
	return bnxt_pmd_bd_act_set(port_id, action);
}

/* oper size is in bits and res size are in bytes */
static int32_t
ulp_mapper_func_cond_list_process(struct bnxt_ulp_mapper_parms *parms,
				  uint32_t idx, uint8_t dir,
				  uint32_t oper_size, uint64_t *res,
				  uint32_t res_size)
{
	struct bnxt_ulp_mapper_field_info *fld;
	uint8_t *val = NULL;
	uint32_t val_len = 0;
	uint64_t value = 0;
	uint16_t ext_idx = 0;
	uint8_t *res_local = (uint8_t *)res;

	/* Get the field info from the key ext list */
	fld = ulp_mapper_tmpl_key_ext_list_get(parms, idx);
	if (unlikely(fld == NULL || fld->field_opc !=
		     BNXT_ULP_FIELD_OPC_TERNARY_LIST)) {
		BNXT_DRV_DBG(ERR, "Invalid field idx %d\n", idx);
		return -EINVAL;
	}

	/* process the condition  list */
	if (unlikely(ulp_mapper_field_src_process(parms, fld->field_src1,
						  fld->field_opr1, dir,
						  1, oper_size, &val,
						  &val_len, &value))) {
		BNXT_DRV_DBG(ERR, "error processing func opcode %u\n",
			     idx);
		return -EINVAL;
	}
	if (value) {
		if (fld->field_src2 == BNXT_ULP_FIELD_SRC_NEXT) {
			/* read the next key ext table index */
			if (unlikely(ulp_operand_read(fld->field_opr2,
						      (uint8_t *)&ext_idx,
						      sizeof(uint16_t)))) {
				BNXT_DRV_DBG(ERR,
					     "field idx operand read failed\n");
				return -EINVAL;
			}
			ext_idx = tfp_be_to_cpu_16(ext_idx);
			return ulp_mapper_func_cond_list_process(parms, ext_idx,
								 dir, oper_size,
								 res, res_size);
		} else {
			/* get the value from then part */
			if (unlikely(ulp_mapper_field_src_process(parms, fld->field_src2,
								  fld->field_opr2, dir,
								  1, oper_size,
								  &val, &val_len,
								  &value))) {
				BNXT_DRV_DBG(ERR,
					     "error processing func oper %u\n",
					     ext_idx);
				return -EINVAL;
			}
		}
	} else {
		if (fld->field_src3 == BNXT_ULP_FIELD_SRC_NEXT) {
			/* read the next key ext table index */
			if (unlikely(ulp_operand_read(fld->field_opr3,
						      (uint8_t *)&ext_idx,
						      sizeof(uint16_t)))) {
				BNXT_DRV_DBG(ERR,
					     "field idx operand read failed\n");
				return -EINVAL;
			}
			ext_idx = tfp_be_to_cpu_16(ext_idx);
			return ulp_mapper_func_cond_list_process(parms, ext_idx,
								 dir, oper_size,
								 res, res_size);
		} else {
			/* get the value from else part */
			if (unlikely(ulp_mapper_field_src_process(parms, fld->field_src3,
								  fld->field_opr3, dir,
								  1, oper_size,
								  &val, &val_len,
								  &value))) {
				BNXT_DRV_DBG(ERR,
					     "error processing func oper %u\n",
					     ext_idx);
				return -EINVAL;
			}
		}
	}
	/* write the value into result */
	if (unlikely(ulp_operand_read(val, res_local + res_size -
				      ULP_BITS_2_BYTE_NR(oper_size),
				      ULP_BITS_2_BYTE_NR(val_len)))) {
		BNXT_DRV_DBG(ERR,
			     "field idx operand read failed\n");
		return -EINVAL;
	}

	/* convert the data to cpu format */
	*res = tfp_be_to_cpu_64(*res);
	return 0;
}

static int32_t
ulp_mapper_func_info_process(struct bnxt_ulp_mapper_parms *parms,
			     struct bnxt_ulp_mapper_tbl_info *tbl)
{
	const struct ulp_mapper_core_ops *op = parms->mapper_data->mapper_oper;
	struct bnxt_ulp_mapper_func_info *func_info = &tbl->func_info;
	uint64_t res = 0, res1 = 0, res2 = 0;
	int32_t rc = 0;
	uint32_t process_src1 = 0, process_src2 = 0;

	/* determine which functional operands to compute */
	switch (func_info->func_opc) {
	case BNXT_ULP_FUNC_OPC_NOP:
		return rc;
	case BNXT_ULP_FUNC_OPC_EQ:
	case BNXT_ULP_FUNC_OPC_NE:
	case BNXT_ULP_FUNC_OPC_GE:
	case BNXT_ULP_FUNC_OPC_GT:
	case BNXT_ULP_FUNC_OPC_LE:
	case BNXT_ULP_FUNC_OPC_LT:
	case BNXT_ULP_FUNC_OPC_LEFT_SHIFT:
	case BNXT_ULP_FUNC_OPC_RIGHT_SHIFT:
	case BNXT_ULP_FUNC_OPC_BIT_OR:
	case BNXT_ULP_FUNC_OPC_BIT_AND:
	case BNXT_ULP_FUNC_OPC_BIT_XOR:
	case BNXT_ULP_FUNC_OPC_LOG_OR:
	case BNXT_ULP_FUNC_OPC_LOG_AND:
	case BNXT_ULP_FUNC_OPC_ADD:
	case BNXT_ULP_FUNC_OPC_SUB:
		process_src1 = 1;
		process_src2 = 1;
		break;
	case BNXT_ULP_FUNC_OPC_COPY_SRC1_TO_RF:
		process_src1 = 1;
		break;
	case BNXT_ULP_FUNC_OPC_HANDLE_TO_OFFSET:
	case BNXT_ULP_FUNC_OPC_VFR_MARK_SET:
	case BNXT_ULP_FUNC_OPC_BD_ACT_SET:
	case BNXT_ULP_FUNC_OPC_MTR_ID_TO_STATS_HANDLE:
	case BNXT_ULP_FUNC_OPC_TCAM_SET_PRIORITY:
		process_src1 = 1;
		process_src2 = 1;
		break;
	case BNXT_ULP_FUNC_OPC_NOT_NOT:
		process_src1 = 1;
	case BNXT_ULP_FUNC_OPC_COND_LIST:
	case BNXT_ULP_FUNC_OPC_APP_PRIORITY:
	case BNXT_ULP_FUNC_OPC_GET_HA_PRIORITY:
		break;
	case BNXT_ULP_FUNC_OPC_PORT_TABLE:
		process_src1 = 1;
		process_src2 = 1;
		break;
	default:
		break;
	}

	if (process_src1) {
		rc = ulp_mapper_func_opr_compute(parms, tbl->direction,
						 func_info->func_src1,
						 func_info->func_opr1, &res1);
		if (rc)
			return rc;
	}

	if (process_src2) {
		rc = ulp_mapper_func_opr_compute(parms, tbl->direction,
						 func_info->func_src2,
						 func_info->func_opr2, &res2);
		if (rc)
			return rc;
	}

	/* perform the functional opcode operations */
	switch (func_info->func_opc) {
	case BNXT_ULP_FUNC_OPC_EQ:
		if (res1 == res2)
			res = 1;
		break;
	case BNXT_ULP_FUNC_OPC_NE:
		if (res1 != res2)
			res = 1;
		break;
	case BNXT_ULP_FUNC_OPC_GE:
		if (res1 >= res2)
			res = 1;
		break;
	case BNXT_ULP_FUNC_OPC_GT:
		if (res1 > res2)
			res = 1;
		break;
	case BNXT_ULP_FUNC_OPC_LE:
		if (res1 <= res2)
			res = 1;
		break;
	case BNXT_ULP_FUNC_OPC_LT:
		if (res1 < res2)
			res = 1;
		break;
	case BNXT_ULP_FUNC_OPC_LEFT_SHIFT:
		res = res1 << res2;
		break;
	case BNXT_ULP_FUNC_OPC_RIGHT_SHIFT:
		res = res1 >> res2;
		break;
	case BNXT_ULP_FUNC_OPC_ADD:
		res = res1 + res2;
		break;
	case BNXT_ULP_FUNC_OPC_SUB:
		res = res1 - res2;
		break;
	case BNXT_ULP_FUNC_OPC_NOT_NOT:
		res = !!res1;
		break;
	case BNXT_ULP_FUNC_OPC_BIT_AND:
		res = res1 & res2;
		break;
	case BNXT_ULP_FUNC_OPC_BIT_OR:
		res = res1 | res2;
		break;
	case BNXT_ULP_FUNC_OPC_BIT_XOR:
		res = res1 ^ res2;
		break;
	case BNXT_ULP_FUNC_OPC_LOG_AND:
		res = res1 && res2;
		break;
	case BNXT_ULP_FUNC_OPC_LOG_OR:
		res = res1 || res2;
		break;
	case BNXT_ULP_FUNC_OPC_COPY_SRC1_TO_RF:
		res = res1;
		break;
	case BNXT_ULP_FUNC_OPC_RSS_CONFIG:
		/* apply the rss config using pmd method */
		return bnxt_rss_config_action_apply(parms);
	case BNXT_ULP_FUNC_OPC_GET_PARENT_MAC_ADDR:
		rc = bnxt_pmd_get_parent_mac_addr(parms, (uint8_t *)&res);
		if (unlikely(rc))
			return -EINVAL;
		res = tfp_be_to_cpu_64(res);
		break;
	case BNXT_ULP_FUNC_OPC_HANDLE_TO_OFFSET:
		rc = op->ulp_mapper_core_handle_to_offset(parms, res1,
							  res2, &res);
		break;
	case BNXT_ULP_FUNC_OPC_VFR_MARK_SET:
		/* res1 is key, res2 is portid */
		return ulp_mapper_vfr_mark_set(parms, res1, res2, tbl);
	case BNXT_ULP_FUNC_OPC_BD_ACT_SET:
		/* res1 is port_id, res2 is action */
		return ulp_mapper_bd_act_set(parms, res1, res2);
	case BNXT_ULP_FUNC_OPC_COND_LIST:
		if (unlikely(func_info->func_src1 != BNXT_ULP_FUNC_SRC_KEY_EXT_LIST)) {
			BNXT_DRV_DBG(ERR, "invalid func source %u\n",
				     func_info->func_opc);
			return -EINVAL;
		}
		if (ulp_mapper_func_cond_list_process(parms,
						      func_info->func_opr1,
						      tbl->direction,
						      func_info->func_oper_size,
						      &res, sizeof(res)))
			return -EINVAL;
		break;
	case BNXT_ULP_FUNC_OPC_PORT_TABLE:
		rc = ulp_mapper_field_port_db_write(parms, res1,
						    func_info->func_dst_opr,
						    (uint8_t *)&res2,
						    func_info->func_oper_size);
		return rc;
	case BNXT_ULP_FUNC_OPC_MTR_ID_TO_STATS_HANDLE:
		/* res1 is mtr_id, res2 is stats_id */
		return op->ulp_mapper_mtr_stats_hndl_set(parms, res1, res2);
	case BNXT_ULP_FUNC_OPC_APP_PRIORITY:
		res = parms->app_priority;
		break;
	case BNXT_ULP_FUNC_OPC_TCAM_SET_PRIORITY:
		return op->ulp_mapper_core_tcam_prio_update(parms,
							    tbl->direction,
							    tbl->track_type,
							    CFA_RSUBTYPE_TCAM_WC,
							    (uint32_t)res1,
							    (uint16_t)res2);
	case BNXT_ULP_FUNC_OPC_GET_HA_PRIORITY:
		res = bnxt_ulp_ha_priority_id_get(parms->ulp_ctx);
		break;
	default:
		BNXT_DRV_DBG(ERR, "invalid func code %u\n",
			     func_info->func_opc);
		return -EINVAL;
	}
	if (unlikely(ulp_regfile_write(parms->regfile, func_info->func_dst_opr,
				       tfp_cpu_to_be_64(res)))) {
		BNXT_DRV_DBG(ERR, "Failed write the func_opc %u\n",
			     func_info->func_dst_opr);
		return -EINVAL;
	}

	return rc;
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
				 struct bnxt_ulp_mapper_cond_list_info *info,
				 int32_t *res)
{
	struct bnxt_ulp_mapper_cond_info *cond_list;
	int32_t rc = 0, trc = 0;
	uint32_t i;

	switch (info->cond_list_opcode) {
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
		BNXT_DRV_DBG(ERR, "Invalid conditional list opcode %d\n",
			     info->cond_list_opcode);
		*res = 0;
		return -EINVAL;
	}

	cond_list = ulp_mapper_tmpl_cond_list_get(parms, info->cond_start_idx);
	for (i = 0; i < info->cond_nums; i++) {
		rc = ulp_mapper_cond_opc_process(parms,
						 cond_list[i].cond_opcode,
						 cond_list[i].cond_operand,
						 &trc);
		if (unlikely(rc))
			return rc;

		if (info->cond_list_opcode == BNXT_ULP_COND_LIST_OPC_AND) {
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

static int32_t
ulp_mapper_cond_reject_list_process(struct bnxt_ulp_mapper_parms *parms,
				    uint32_t tid, int32_t *res)
{
	struct bnxt_ulp_mapper_cond_list_info *reject_info;
	struct bnxt_ulp_mapper_cond_list_info *oper;
	int32_t cond_list_res = 0, cond_res = 0, rc = 0;
	uint32_t idx;

	/* set the rejection result to accept */
	*res = 0;

	/* get the reject condition list */
	reject_info = ulp_mapper_tmpl_reject_list_get(parms, tid);

	if (reject_info->cond_list_opcode == BNXT_ULP_COND_LIST_OPC_TRUE) {
		cond_list_res  = 1;
		goto jump_exit;
	}

	/* If there are no reject conditions then skip */
	if (!reject_info->cond_nums)
		return rc;

	/* Iterate the list to process the conditions */
	if (reject_info->cond_list_opcode == BNXT_ULP_COND_LIST_OPC_LIST_AND ||
	    reject_info->cond_list_opcode == BNXT_ULP_COND_LIST_OPC_LIST_OR) {
		/* Initialize the cond result */
		if (reject_info->cond_list_opcode ==
		    BNXT_ULP_COND_LIST_OPC_LIST_AND)
			cond_res  = 1;

		for (idx = reject_info->cond_start_idx;
		      idx < reject_info->cond_start_idx +
		      reject_info->cond_nums; idx++) {
			oper = ulp_mapper_cond_oper_list_get(parms, idx);
			if (unlikely(!oper)) {
				BNXT_DRV_DBG(ERR,
					     "Invalid cond oper idx %d\n",
					     idx);
				return -EINVAL;
			}
			rc = ulp_mapper_cond_opc_list_process(parms, oper,
							      &cond_list_res);
			/* if any error, then return */
			if (rc)
				goto jump_exit;

			/* early return if result is ever zero */
			if (cond_res /*and */ && !cond_list_res /*false*/)
				goto jump_exit;

			/* early return if result is ever non-zero */
			if (!cond_res /*or */ && cond_list_res /*true*/)
				goto jump_exit;
		}
	} else {
		rc = ulp_mapper_cond_opc_list_process(parms, reject_info,
						      &cond_list_res);
	}
jump_exit:
	*res = cond_list_res;
	/* Reject the template if True */
	if (cond_list_res)
		BNXT_DRV_DBG(ERR, "%s Template %d rejected.\n",
			     ulp_mapper_tmpl_name_str(parms->tmpl_type), tid);
	return rc;
}

static int32_t
ulp_mapper_cond_execute_list_process(struct bnxt_ulp_mapper_parms *parms,
				     struct bnxt_ulp_mapper_tbl_info *tbl,
				     int32_t *res)
{
	struct bnxt_ulp_mapper_cond_list_info *execute_info;
	struct bnxt_ulp_mapper_cond_list_info *oper;
	int32_t cond_list_res = 0, cond_res = 0, rc = 0;
	uint32_t idx;

	/* set the execute result to true */
	*res = 1;
	execute_info = &tbl->execute_info;

	/* If there are no execute conditions then skip */
	if (!execute_info->cond_nums)
		return rc;

	/* Iterate the list to process the conditions */
	if (execute_info->cond_list_opcode == BNXT_ULP_COND_LIST_OPC_LIST_AND ||
	    execute_info->cond_list_opcode == BNXT_ULP_COND_LIST_OPC_LIST_OR) {
		/* Initialize the cond result */
		if (execute_info->cond_list_opcode ==
		    BNXT_ULP_COND_LIST_OPC_LIST_AND)
			cond_res  = 1;

		for (idx = execute_info->cond_start_idx;
		      idx < execute_info->cond_start_idx +
		      execute_info->cond_nums; idx++) {
			oper = ulp_mapper_cond_oper_list_get(parms, idx);
			if (unlikely(!oper)) {
				BNXT_DRV_DBG(ERR,
					     "Invalid cond oper idx %d\n",
					     idx);
				return -EINVAL;
			}
			rc = ulp_mapper_cond_opc_list_process(parms, oper,
							      &cond_list_res);
			/* if any error, then return */
			if (rc)
				goto jump_exit;

			/* early return if result is ever zero */
			if (cond_res /*and */ && !cond_list_res /*false*/)
				goto jump_exit;

			/* early return if result is ever non-zero */
			if (!cond_res /*or */ && cond_list_res /*true*/)
				goto jump_exit;
		}
	} else {
		rc = ulp_mapper_cond_opc_list_process(parms, execute_info,
						      &cond_list_res);
	}
jump_exit:
	*res = cond_list_res;
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
	uint64_t regval = 0;
	uint64_t comp_sig;

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
			if (unlikely(ulp_regfile_read(parms->regfile,
						      BNXT_ULP_RF_IDX_GENERIC_TBL_MISS,
						      &regval))) {
				BNXT_DRV_DBG(ERR, "regfile[%d] read oob\n",
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
		if (unlikely(ulp_regfile_read(parms->regfile,
					      BNXT_ULP_RF_IDX_FLOW_SIG_ID,
					      &regval))) {
			BNXT_DRV_DBG(ERR, "regfile[%d] read oob\n",
				     BNXT_ULP_RF_IDX_FLOW_SIG_ID);
			return -EINVAL;
		}
		comp_sig = ULP_COMP_FLD_IDX_RD(parms,
					       BNXT_ULP_CF_IDX_FLOW_SIG_ID);
		regval = tfp_be_to_cpu_64(regval);
		if (likely(comp_sig == regval))
			*res = 1;
		else
			BNXT_DRV_DBG(ERR, "failed signature match 0x%016"
				    PRIX64 ":%x\n", comp_sig, (uint32_t)regval);
		break;
	default:
		BNXT_DRV_DBG(ERR, "Invalid accept opcode %d\n",
			     tbl->accept_opcode);
		return -EINVAL;
	}
	return rc;
}

static int32_t
ulp_mapper_allocator_tbl_process(struct bnxt_ulp_mapper_parms *parms,
				 struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct ulp_flow_db_res_params fid_parms;
	int32_t alloc_index, rc = 0;
	uint64_t regval = 0;

	/* Only Alloc opcode is supported for now */
	if (tbl->tbl_opcode != BNXT_ULP_ALLOC_TBL_OPC_ALLOC)
		return 0; /* nothing to done */

	/* allocate the index from the allocator */
	rc = ulp_allocator_tbl_list_alloc(parms->mapper_data,
					  tbl->resource_sub_type,
					  tbl->direction, &alloc_index);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "unable to alloc index %x:%x\n",
			     tbl->resource_sub_type, tbl->direction);
		return -EINVAL;
	}

	/* Write to the regfile */
	regval = rte_cpu_to_be_64(alloc_index);
	rc = ulp_regfile_write(parms->regfile, tbl->tbl_operand, regval);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to write regfile[%d] rc=%d\n",
			     tbl->tbl_operand, rc);
		return -EINVAL;
	}

	/* update the flow database */
	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.direction	= tbl->direction;
	fid_parms.resource_func	= tbl->resource_func;
	fid_parms.resource_type	= tbl->resource_type;
	fid_parms.resource_sub_type = tbl->resource_sub_type;
	fid_parms.resource_hndl	= alloc_index;
	fid_parms.critical_resource = tbl->critical_resource;

	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to link resource to flow rc = %d\n",
			     rc);
		goto error;
	}
	return rc;
error:
	/* Free the allocated index */
	(void)ulp_allocator_tbl_list_free(parms->mapper_data,
					  tbl->resource_sub_type,
					  tbl->direction, alloc_index);
	return rc;
}

static int32_t
ulp_mapper_tbls_process(struct bnxt_ulp_mapper_parms *parms, void *error)
{
	struct bnxt_ulp_mapper_tbl_info *tbls;
	struct bnxt_ulp_mapper_tbl_info *tbl;
	uint32_t num_tbls, tbl_idx;
	const struct ulp_mapper_core_ops *oper;
	int32_t rc = -EINVAL, cond_rc = 0;
	int32_t cond_goto = 1;
	uint32_t tid;

	oper = parms->mapper_data->mapper_oper;

	/* assign the template id based on template type */
	tid = (parms->tmpl_type == BNXT_ULP_TEMPLATE_TYPE_ACTION) ?
		parms->act_tid : parms->class_tid;

	rc = ulp_mapper_cond_reject_list_process(parms, tid, &cond_rc);
	/* if rc is failure or cond_rc is a reject then exit tbl processing */
	if (unlikely(rc || cond_rc))
		return -EINVAL;

	tbls = ulp_mapper_tbl_list_get(parms, tid, &num_tbls);
	if (unlikely(!tbls || !num_tbls)) {
		BNXT_DRV_DBG(ERR, "No %s tables for %d:%d\n",
			     ulp_mapper_tmpl_name_str(parms->tmpl_type),
			     parms->dev_id, tid);
		return -EINVAL;
	}

	for (tbl_idx = 0; tbl_idx < num_tbls && cond_goto;) {
		tbl = &tbls[tbl_idx];
		cond_goto = tbl->execute_info.cond_true_goto;
		/* Process the conditional func code opcodes */
		if (unlikely(ulp_mapper_func_info_process(parms, tbl))) {
			BNXT_DRV_DBG(ERR, "Failed to process cond update\n");
			rc = -EINVAL;
			goto error;
		}

		/* process the execute info of the table */
		rc = ulp_mapper_cond_execute_list_process(parms, tbl, &cond_rc);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Failed to proc cond opc list (%d)\n",
				     rc);
			goto error;
		}
		/* Skip the table if False */
		if (!cond_rc) {
			cond_goto = tbl->execute_info.cond_false_goto;
			goto next_iteration;
		}

		switch (tbl->resource_func) {
		case BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE:
			rc = oper->ulp_mapper_core_tcam_tbl_process(parms, tbl);
			break;
		case BNXT_ULP_RESOURCE_FUNC_EM_TABLE:
			rc = oper->ulp_mapper_core_em_tbl_process(parms, tbl,
								  error);
			break;
		case BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE:
			rc = oper->ulp_mapper_core_index_tbl_process(parms,
								     tbl);
			break;
		case BNXT_ULP_RESOURCE_FUNC_IF_TABLE:
			rc = oper->ulp_mapper_core_if_tbl_process(parms, tbl);
			break;
		case BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE:
			rc = ulp_mapper_gen_tbl_process(parms, tbl);
			break;
		case BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE:
			rc = ulp_mapper_ctrl_tbl_process(parms, tbl);
			break;
		case BNXT_ULP_RESOURCE_FUNC_VNIC_TABLE:
			rc = ulp_mapper_vnic_tbl_process(parms, tbl);
			break;
		case BNXT_ULP_RESOURCE_FUNC_GLOBAL_REGISTER_TABLE:
			rc = ulp_mapper_global_register_tbl_process(parms, tbl);
			break;
		case BNXT_ULP_RESOURCE_FUNC_CMM_TABLE:
		case BNXT_ULP_RESOURCE_FUNC_CMM_STAT:
			rc = oper->ulp_mapper_core_cmm_tbl_process(parms, tbl,
								   error);
			break;
		case BNXT_ULP_RESOURCE_FUNC_INVALID:
			rc = 0;
			break;
		case BNXT_ULP_RESOURCE_FUNC_KEY_RECIPE_TABLE:
			rc = ulp_mapper_key_recipe_tbl_process(parms, tbl);
			break;
		case BNXT_ULP_RESOURCE_FUNC_ALLOCATOR_TABLE:
			rc = ulp_mapper_allocator_tbl_process(parms, tbl);
			break;
		case BNXT_ULP_RESOURCE_FUNC_STATS_CACHE:
			rc = ulp_mapper_stats_cache_tbl_process(parms, tbl);
			break;
		case BNXT_ULP_RESOURCE_FUNC_GLOBAL_IDENTIFIER:
			rc = ulp_mapper_global_identifier_process(parms, tbl);
			break;
		case BNXT_ULP_RESOURCE_FUNC_GLOBAL_IDX_TBL:
			rc = ulp_mapper_global_idx_tbl_process(parms, tbl);
			break;
		default:
			BNXT_DRV_DBG(ERR, "Unexpected mapper resource %d\n",
				     tbl->resource_func);
			rc = -EINVAL;
			goto error;
		}

		if (rc) {
			BNXT_DRV_DBG(ERR, "Resource type %d failed\n",
				     tbl->resource_func);
			goto error;
		}

		/* perform the post table process */
		rc  = ulp_mapper_conflict_resolution_process(parms, tbl,
							     &cond_rc);
		if (unlikely(rc || !cond_rc)) {
			BNXT_DRV_DBG(ERR, "Failed due to conflict resolution\n");
			rc = -EINVAL;
			goto error;
		}
next_iteration:
		if (cond_goto < 0) {
			if (unlikely(((int32_t)tbl_idx + cond_goto) < 0)) {
				BNXT_DRV_DBG(ERR,
					     "invalid conditional goto %d\n",
					     cond_goto);
				goto error;
			}
		} else if (cond_goto == BNXT_ULP_COND_GOTO_REJECT) {
			if (tbl->false_message || tbl->true_message) {
				const char *msg = (tbl->false_message) ?
					tbl->false_message :
					tbl->true_message;

				BNXT_DRV_DBG(DEBUG, "%s\n", msg);
				if (unlikely(error))
					rte_flow_error_set(error, EINVAL,
							   RTE_FLOW_ERROR_TYPE_ITEM,
							   NULL, msg);
				return -EINVAL;
			}
			BNXT_DRV_DBG(ERR, "reject the flow\n");
			rc = -EINVAL;
			goto error;
		} else if (cond_goto & BNXT_ULP_COND_GOTO_RF) {
			int32_t rf_idx;
			uint64_t regval = 0;

			/* least significant 16 bits from reg_file index */
			rf_idx = (int32_t)(cond_goto & 0xFFFF);
			if (unlikely(ulp_regfile_read(parms->regfile, rf_idx,
						      &regval))) {
				BNXT_DRV_DBG(ERR, "regfile[%d] read oob\n",
					     rf_idx);
				rc = -EINVAL;
				goto error;
			}
			cond_goto = (int32_t)regval;
		}
		tbl_idx += cond_goto;
	}

	return rc;
error:
	BNXT_DRV_DBG(ERR, "%s tables failed operation for %d:%d\n",
		     ulp_mapper_tmpl_name_str(parms->tmpl_type),
		     parms->dev_id, tid);
	return rc;
}

static int32_t
ulp_mapper_resource_free(struct bnxt_ulp_context *ulp,
			 uint32_t fid,
			 struct ulp_flow_db_res_params *res,
			 void *error)
{
	const struct ulp_mapper_core_ops *mapper_op;
	struct bnxt_ulp_mapper_data *mdata;
	int32_t	rc = 0;

	if (unlikely(!res || !ulp)) {
		BNXT_DRV_DBG(ERR, "Unable to free resource\n ");
		return -EINVAL;
	}

	mapper_op = ulp_mapper_data_oper_get(ulp);
	switch (res->resource_func) {
	case BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE:
		rc = mapper_op->ulp_mapper_core_tcam_entry_free(ulp, res);
		break;
	case BNXT_ULP_RESOURCE_FUNC_EM_TABLE:
		rc = mapper_op->ulp_mapper_core_em_entry_free(ulp, res, error);
		break;
	case BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE:
		rc = mapper_op->ulp_mapper_core_index_entry_free(ulp, res);
		break;
	case BNXT_ULP_RESOURCE_FUNC_CMM_TABLE:
	case BNXT_ULP_RESOURCE_FUNC_CMM_STAT:
		rc = mapper_op->ulp_mapper_core_cmm_entry_free(ulp, res, error);
		break;
	case BNXT_ULP_RESOURCE_FUNC_IDENTIFIER:
		rc = mapper_op->ulp_mapper_core_ident_free(ulp, res);
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
		rc = ulp_mapper_gen_tbl_res_free(ulp, fid, res);
		break;
	case BNXT_ULP_RESOURCE_FUNC_VNIC_TABLE:
		rc = ulp_mapper_vnic_tbl_res_free(ulp, ulp->bp, res);
		break;
	case BNXT_ULP_RESOURCE_FUNC_GLOBAL_REGISTER_TABLE:
		rc = ulp_mapper_global_res_free(ulp, ulp->bp, res);
		break;
	case BNXT_ULP_RESOURCE_FUNC_KEY_RECIPE_TABLE:
		rc = ulp_mapper_key_recipe_free(ulp, res->direction,
						res->resource_sub_type,
						res->resource_hndl);
		break;
	case BNXT_ULP_RESOURCE_FUNC_ALLOCATOR_TABLE:
		mdata = bnxt_ulp_cntxt_ptr2_mapper_data_get(ulp);
		if (unlikely(!mdata)) {
			BNXT_DRV_DBG(ERR, "Unable to get mapper data\n");
			return -EINVAL;
		}
		rc = ulp_allocator_tbl_list_free(mdata,
						 res->resource_sub_type,
						 res->direction,
						 res->resource_hndl);
		break;
	case BNXT_ULP_RESOURCE_FUNC_STATS_CACHE:
		rc = ulp_mapper_stats_cache_tbl_res_free(ulp,
							 fid);
		break;
	case BNXT_ULP_RESOURCE_FUNC_GLOBAL_IDENTIFIER:
		rc = mapper_op->ulp_mapper_core_global_ident_free(ulp, res);
		break;
	case BNXT_ULP_RESOURCE_FUNC_GLOBAL_IDX_TBL:
		rc = mapper_op->ulp_mapper_core_glb_idx_tbl_free(ulp, res);
		break;
	default:
		break;
	}

	return rc;
}

int32_t
ulp_mapper_resources_free(struct bnxt_ulp_context *ulp_ctx,
			  enum bnxt_ulp_fdb_type flow_type,
			  uint32_t fid,
			  void *error)
{
	struct ulp_flow_db_res_params res_parms = { 0 };
	int32_t rc, trc, frc = 0;

	if (unlikely(!ulp_ctx)) {
		BNXT_DRV_DBG(ERR, "Invalid parms, unable to free flow\n");
		return -EINVAL;
	}

	/*
	 * Set the critical resource on the first resource del, then iterate
	 * while status is good
	 */
	res_parms.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES;

	rc = ulp_flow_db_resource_del(ulp_ctx, flow_type, fid, &res_parms);

	if (rc) {
		/*
		 * This is unexpected on the first call to resource del.
		 * It likely means that the flow did not exist in the flow db.
		 */
		BNXT_DRV_DBG(ERR, "Flow[%d][0x%08x] failed to free (rc=%d)\n",
			     flow_type, fid, rc);
		return rc;
	}

	while (!rc) {
		trc = ulp_mapper_resource_free(ulp_ctx, fid, &res_parms, error);
		if (trc) {
			/*
			 * On fail, we still need to attempt to free the
			 * remaining resources.  Don't return
			 */
			BNXT_DRV_DBG(ERR,
				     "Flow[%d][0x%x] Res[%d][0x%016" PRIX64
				     "] failed rc=%d.\n",
				     flow_type, fid, res_parms.resource_func,
				     res_parms.resource_hndl, trc);

			/* Capture error in final rc */
			frc = trc;
		}
		/* All subsequent call require the non-critical_resource */
		res_parms.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO;

		rc = ulp_flow_db_resource_del(ulp_ctx,
					      flow_type,
					      fid,
					      &res_parms);
	}

	/* Expected that flow_db should return no entry */
	if (rc != -ENOENT)
		frc = rc;

	/* Free the Flow ID since we've removed all resources */
	rc = ulp_flow_db_fid_free(ulp_ctx, flow_type, fid);

	/* Ensure that any error will be reported */
	if (rc)
		frc = rc;

#ifdef TF_FLOW_SCALE_QUERY
	/* update for regular flows only */
	if (flow_type == BNXT_ULP_FDB_TYPE_REGULAR)
		ulp_resc_usage_sync(ulp_ctx);
#endif /* TF_FLOW_SCALE_QUERY */

	return frc;
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
		for (idx = 0; idx < BNXT_ULP_GLB_RF_IDX_LAST; idx++) {
			ent = &mapper_data->glb_res_tbl[dir][idx];
			if (ent->resource_func ==
			    BNXT_ULP_RESOURCE_FUNC_INVALID ||
			    ent->shared)
				continue;
			memset(&res, 0, sizeof(struct ulp_flow_db_res_params));
			res.resource_func = ent->resource_func;
			res.direction = dir;
			res.resource_type = ent->resource_type;
			/*convert it from BE to cpu */
			res.resource_hndl =
				tfp_be_to_cpu_64(ent->resource_hndl);
			ulp_mapper_resource_free(ulp_ctx, 0, &res, NULL);
		}
	}
}

int32_t
ulp_mapper_flow_destroy(struct bnxt_ulp_context *ulp_ctx,
			enum bnxt_ulp_fdb_type flow_type,
			uint32_t fid,
			void *error)
{
	int32_t rc;

	if (unlikely(!ulp_ctx)) {
		BNXT_DRV_DBG(ERR, "Invalid parms, unable to free flow\n");
		return -EINVAL;
	}

	rc = ulp_mapper_resources_free(ulp_ctx, flow_type, fid, error);
	return rc;
}

/* Function to handle the mapping of the Flow to be compatible
 * with the underlying hardware.
 */
int32_t
ulp_mapper_flow_create(struct bnxt_ulp_context *ulp_ctx,
		       struct bnxt_ulp_mapper_parms *parms, void *error)
{
	const struct ulp_mapper_core_ops *oper;
	struct ulp_regfile regfile;
	int32_t	 rc = 0, trc;

	if (unlikely(!ulp_ctx || !parms))
		return -EINVAL;

	parms->regfile = &regfile;
	parms->ulp_ctx = ulp_ctx;

	oper = ulp_mapper_data_oper_get(ulp_ctx);

	/* Get the device id from the ulp context */
	if (unlikely(bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &parms->dev_id))) {
		BNXT_DRV_DBG(ERR, "Invalid ulp context\n");
		return -EINVAL;
	}
	if (unlikely(bnxt_ulp_cntxt_fid_get(ulp_ctx, &parms->fw_fid))) {
		BNXT_DRV_DBG(ERR, "Unable to get the func_id\n");
		return -EINVAL;
	}

	/* Get the device params, it will be used in later processing */
	parms->device_params = bnxt_ulp_device_params_get(parms->dev_id);
	if (unlikely(!parms->device_params)) {
		BNXT_DRV_DBG(ERR, "No device parms for device id %d\n",
			     parms->dev_id);
		return -EINVAL;
	}

	/*
	 * Get the mapper data for dynamic mapper data such as default
	 * ids.
	 */
	parms->mapper_data = (struct bnxt_ulp_mapper_data *)
		bnxt_ulp_cntxt_ptr2_mapper_data_get(ulp_ctx);
	if (unlikely(!parms->mapper_data)) {
		BNXT_DRV_DBG(ERR, "Failed to get the ulp mapper data\n");
		return -EINVAL;
	}

	/* initialize the registry file for further processing */
	if (unlikely(ulp_regfile_init(parms->regfile))) {
		BNXT_DRV_DBG(ERR, "regfile initialization failed.\n");
		return -EINVAL;
	}

	/* Start batching */
	rc = oper->ulp_mapper_mpc_batch_start(&parms->batch_info);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "MPC Batch start failed\n");
		return -EINVAL;
	}

	/* Process the action template list from the selected action table*/
	if (parms->act_tid) {
		parms->tmpl_type = BNXT_ULP_TEMPLATE_TYPE_ACTION;
		/* Process the action template tables */
		rc = ulp_mapper_tbls_process(parms, error);
		if (unlikely(rc))
			goto batch_error;
	}

	if (parms->class_tid) {
		parms->tmpl_type = BNXT_ULP_TEMPLATE_TYPE_CLASS;
		/* Process the class template tables.*/
		rc = ulp_mapper_tbls_process(parms, error);
		if (unlikely(rc))
			goto batch_error;
	}

	if (oper->ulp_mapper_mpc_batch_started(&parms->batch_info)) {
		/* Should only get here is there were no EM inserts */
		rc = oper->ulp_mapper_mpc_batch_end(&ulp_ctx->bp->tfcp,
						    &parms->batch_info);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "MPC Batch end failed\n");
			goto flow_error;
		}
	}

	/* setup the parent-child details */
	if (parms->parent_flow) {
		/* create a parent flow details */
		rc = ulp_flow_db_parent_flow_create(parms);
		if (unlikely(rc))
			goto flow_error;
	} else if (parms->child_flow) {
		/* create a child flow details */
		rc = ulp_flow_db_child_flow_create(parms);
		if (unlikely(rc))
			goto flow_error;
	}

#ifdef TF_FLOW_SCALE_QUERY
	ulp_resc_usage_sync(ulp_ctx);
#endif /* TF_FLOW_SCALE_QUERY */

	return rc;

batch_error:
	/*
	 * An error occurred after batching had started but before it
	 * ended. Call batch end and ignore any errors.
	 */
	if (oper->ulp_mapper_mpc_batch_started(&parms->batch_info))
		oper->ulp_mapper_mpc_batch_end(&ulp_ctx->bp->tfcp,
					       &parms->batch_info);

flow_error:
	if (parms->rid) {
		/* An RID was in-flight but not pushed, free the resources */
		trc = ulp_mapper_flow_destroy(ulp_ctx, BNXT_ULP_FDB_TYPE_RID,
					      parms->rid, NULL);
		if (trc)
			BNXT_DRV_DBG(ERR,
				     "Failed to free resources rid=0x%08x rc=%d\n",
				     parms->rid, trc);
		parms->rid = 0;
	}

	/* Free all resources that were allocated during flow creation */
	if (parms->flow_id) {
		trc = ulp_mapper_flow_destroy(ulp_ctx, parms->flow_type,
					      parms->flow_id, NULL);
		if (trc)
			BNXT_DRV_DBG(ERR,
				     "Failed to free resources fid=0x%08x rc=%d\n",
				     parms->flow_id, trc);
	}

	return rc;
}

#ifdef TF_FLOW_SCALE_QUERY
/* Sync resource usage state with firmware */
int ulp_resc_usage_sync(struct bnxt_ulp_context *ulp_ctx)
{
	uint32_t dev_id;
	if (unlikely(bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id))) {
		BNXT_DRV_DBG(ERR, "Invalid ulp context\n");
		return -EINVAL;
	}

	if (dev_id == BNXT_ULP_DEVICE_ID_THOR) {
		tf_resc_resume_usage_update();
		tf_resc_usage_update_all(ulp_ctx->bp);
	} else if (dev_id == BNXT_ULP_DEVICE_ID_THOR2) {
		tfc_resc_usage_query_all(ulp_ctx->bp);
	}

	return 0;
}
#endif /* TF_FLOW_SCALE_QUERY */

int32_t
ulp_mapper_init(struct bnxt_ulp_context *ulp_ctx)
{
	struct bnxt_ulp_mapper_data *data;
	int32_t rc;

	if (!ulp_ctx)
		return -EINVAL;

	data = rte_zmalloc("ulp_mapper_data",
			   sizeof(struct bnxt_ulp_mapper_data), 0);
	if (unlikely(!data)) {
		BNXT_DRV_DBG(ERR, "Failed to allocate the mapper data\n");
		return -ENOMEM;
	}

	/* set the mapper operations for the current platform */
	data->mapper_oper = bnxt_ulp_mapper_ops_get(ulp_ctx->bp);
	if (unlikely(data->mapper_oper == NULL)) {
		rte_free(data);
		BNXT_DRV_DBG(ERR, "Failed to get mapper ops\n");
		return -ENOMEM;
	}

	if (unlikely(bnxt_ulp_cntxt_ptr2_mapper_data_set(ulp_ctx, data))) {
		BNXT_DRV_DBG(ERR, "Failed to set mapper data in context\n");
		/* Don't call deinit since the prof_func wasn't allocated. */
		rte_free(data);
		return -ENOMEM;
	}

	/* Allocate the global resource ids */
	rc = ulp_mapper_glb_resource_info_init(ulp_ctx, data);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to initialize global resource ids\n");
		goto error;
	}

	/*
	 * Only initialize the app global resources if a shared session was
	 * created.
	 */
	if (bnxt_ulp_cntxt_shared_session_enabled(ulp_ctx)) {
		rc = ulp_mapper_app_glb_resource_info_init(ulp_ctx, data);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Failed to init app glb resources\n");
			goto error;
		}
	}

	/* Allocate the generic table list */
	rc = ulp_mapper_generic_tbl_list_init(ulp_ctx, data);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to initialize generic tbl list\n");
		goto error;
	}

	rc = ulp_mapper_key_recipe_tbl_init(ulp_ctx, data);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to initialize key_recipe tbl\n");
		goto error;
	}

	rc = ulp_allocator_tbl_list_init(ulp_ctx, data);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to initialize allocator tbl\n");
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

	if (unlikely(!ulp_ctx)) {
		BNXT_DRV_DBG(ERR,
			     "Failed to acquire ulp context, so data may not be released.\n");
		return;
	}

	data = (struct bnxt_ulp_mapper_data *)
		bnxt_ulp_cntxt_ptr2_mapper_data_get(ulp_ctx);
	if (unlikely(!data)) {
		/* Go ahead and return since there is no allocated data. */
		BNXT_DRV_DBG(ERR, "No data appears to have been allocated.\n");
		return;
	}

	/* Free the global resource info table entries */
	ulp_mapper_glb_resource_info_deinit(ulp_ctx, data);

	/* Free the generic table */
	(void)ulp_mapper_generic_tbl_list_deinit(data);

	/* Free the key recipe table */
	(void)ulp_mapper_key_recipe_tbl_deinit(data);

	/* Free the allocator table */
	(void)ulp_allocator_tbl_list_deinit(data);

	rte_free(data);
	/* Reset the data pointer within the ulp_ctx. */
	bnxt_ulp_cntxt_ptr2_mapper_data_set(ulp_ctx, NULL);
}
