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

static uint8_t mapper_fld_zeros[16] = { 0 };

static uint8_t mapper_fld_ones[16] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static uint8_t mapper_fld_one[16] = {
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01
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
			     uint64_t *regval,
			     bool *shared)
{
	if (!mapper_data || !regval || !shared ||
	    dir >= TF_DIR_MAX || idx >= BNXT_ULP_GLB_RF_IDX_LAST)
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
static int32_t
ulp_mapper_glb_resource_write(struct bnxt_ulp_mapper_data *data,
			      struct bnxt_ulp_glb_resource_info *res,
			      uint64_t regval, bool shared)
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
	ent->shared = shared;
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
				   struct bnxt_ulp_glb_resource_info *glb_res,
				   bool shared)
{
	struct tf_alloc_identifier_parms iparms = { 0 };
	struct tf_free_identifier_parms fparms;
	uint64_t regval;
	struct tf *tfp;
	int32_t rc = 0;

	tfp = bnxt_ulp_cntxt_tfp_get(ulp_ctx, shared ?
				     BNXT_ULP_SESSION_TYPE_SHARED :
				     BNXT_ULP_SESSION_TYPE_DEFAULT);
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
	/*
	 * write to the mapper global resource
	 * Shared resources are never allocated through this method, so the
	 * shared flag is always false.
	 */
	rc = ulp_mapper_glb_resource_write(mapper_data, glb_res, regval, shared);
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
				    struct bnxt_ulp_glb_resource_info *glb_res,
				    bool shared)
{
	struct tf_alloc_tbl_entry_parms	aparms = { 0 };
	struct tf_free_tbl_entry_parms	free_parms = { 0 };
	uint64_t regval;
	struct tf *tfp;
	uint32_t tbl_scope_id;
	int32_t rc = 0;

	tfp = bnxt_ulp_cntxt_tfp_get(ulp_ctx, shared ?
				     BNXT_ULP_SESSION_TYPE_SHARED :
				     BNXT_ULP_SESSION_TYPE_DEFAULT);
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
	aparms.tbl_scope_id = tbl_scope_id;

	/* Allocate the index tbl using tf api */
	rc = tf_alloc_tbl_entry(tfp, &aparms);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to alloc index table [%s][%d]\n",
			    tf_dir_2_str(aparms.dir), aparms.type);
		return rc;
	}

	/* entries are stored as big-endian format */
	regval = tfp_cpu_to_be_64((uint64_t)aparms.idx);
	/*
	 * write to the mapper global resource
	 * Shared resources are never allocated through this method, so the
	 * shared flag is always false.
	 */
	rc = ulp_mapper_glb_resource_write(mapper_data, glb_res, regval, shared);
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

static int32_t
ulp_mapper_glb_field_tbl_get(struct bnxt_ulp_mapper_parms *parms,
			     uint32_t operand,
			     uint8_t *val)
{
	uint32_t t_idx;

	t_idx = parms->app_id << (BNXT_ULP_APP_ID_SHIFT +
				  BNXT_ULP_HDR_SIG_ID_SHIFT +
				  BNXT_ULP_GLB_FIELD_TBL_SHIFT);
	t_idx += parms->class_tid << (BNXT_ULP_HDR_SIG_ID_SHIFT +
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

static enum tf_tbl_type
ulp_mapper_dyn_tbl_type_get(struct bnxt_ulp_mapper_parms *mparms,
			    struct bnxt_ulp_mapper_tbl_info *tbl,
			    struct ulp_blob *bdata,
			    uint16_t *out_len)
{
	struct bnxt_ulp_device_params *d_params = mparms->device_params;
	uint16_t blob_len = ulp_blob_data_len_get(bdata);
	struct bnxt_ulp_dyn_size_map *size_map;
	uint32_t i;

	if (d_params->dynamic_sram_en) {
		switch (tbl->resource_type) {
		case TF_TBL_TYPE_ACT_ENCAP_8B:
		case TF_TBL_TYPE_ACT_ENCAP_16B:
		case TF_TBL_TYPE_ACT_ENCAP_32B:
		case TF_TBL_TYPE_ACT_ENCAP_64B:
		case TF_TBL_TYPE_ACT_ENCAP_128B:
			size_map = d_params->dyn_encap_sizes;
			for (i = 0; i < d_params->dyn_encap_list_size; i++) {
				if (blob_len <= size_map[i].slab_size) {
					*out_len = size_map[i].slab_size;
					return size_map[i].tbl_type;
				}
			}
			break;
		case TF_TBL_TYPE_ACT_MODIFY_8B:
		case TF_TBL_TYPE_ACT_MODIFY_16B:
		case TF_TBL_TYPE_ACT_MODIFY_32B:
		case TF_TBL_TYPE_ACT_MODIFY_64B:
			size_map = d_params->dyn_modify_sizes;
			for (i = 0; i < d_params->dyn_modify_list_size; i++) {
				if (blob_len <= size_map[i].slab_size) {
					*out_len = size_map[i].slab_size;
					return size_map[i].tbl_type;
				}
			}
			break;
		default:
			break;
		}
	}
	return tbl->resource_type;
}

static uint16_t
ulp_mapper_dyn_blob_size_get(struct bnxt_ulp_mapper_parms *mparms,
			     struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct bnxt_ulp_device_params *d_params = mparms->device_params;

	if (d_params->dynamic_sram_en) {
		switch (tbl->resource_type) {
		case TF_TBL_TYPE_ACT_ENCAP_8B:
		case TF_TBL_TYPE_ACT_ENCAP_16B:
		case TF_TBL_TYPE_ACT_ENCAP_32B:
		case TF_TBL_TYPE_ACT_ENCAP_64B:
		case TF_TBL_TYPE_ACT_MODIFY_8B:
		case TF_TBL_TYPE_ACT_MODIFY_16B:
		case TF_TBL_TYPE_ACT_MODIFY_32B:
		case TF_TBL_TYPE_ACT_MODIFY_64B:
			/* return max size */
			return BNXT_ULP_FLMP_BLOB_SIZE_IN_BITS;
		default:
			break;
		}
	} else if (tbl->encap_num_fields) {
		return BNXT_ULP_FLMP_BLOB_SIZE_IN_BITS;
	}
	return tbl->result_bit_size;
}

static inline int32_t
ulp_mapper_tcam_entry_free(struct bnxt_ulp_context *ulp,
			   struct tf *tfp,
			   struct ulp_flow_db_res_params *res)
{
	struct tf_free_tcam_entry_parms fparms = {
		.dir		= res->direction,
		.tcam_tbl_type	= res->resource_type,
		.idx		= (uint16_t)res->resource_hndl
	};

	/* If HA is enabled, we may have to remap the TF Type */
	if (bnxt_ulp_cntxt_ha_enabled(ulp)) {
		enum ulp_ha_mgr_region region;
		int32_t rc;

		switch (res->resource_type) {
		case TF_TCAM_TBL_TYPE_WC_TCAM_HIGH:
		case TF_TCAM_TBL_TYPE_WC_TCAM_LOW:
			rc = ulp_ha_mgr_region_get(ulp, &region);
			if (rc)
				/* Log this, but assume region is correct */
				BNXT_TF_DBG(ERR,
					    "Unable to get HA region (%d)\n",
					    rc);
			else
				fparms.tcam_tbl_type =
					(region == ULP_HA_REGION_LOW) ?
					TF_TCAM_TBL_TYPE_WC_TCAM_LOW :
					TF_TCAM_TBL_TYPE_WC_TCAM_HIGH;
			break;
		default:
			break;
		}
	}
	return tf_free_tcam_entry(tfp, &fparms);
}

static int32_t
ulp_mapper_clear_full_action_record(struct tf *tfp,
				    struct bnxt_ulp_context *ulp_ctx,
				    struct tf_free_tbl_entry_parms *fparms)
{
	struct tf_set_tbl_entry_parms sparms = { 0 };
	uint32_t dev_id = BNXT_ULP_DEVICE_ID_LAST;
	int32_t rc = 0;

	rc = bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to get the dev id from ulp.\n");
		return rc;
	}

	if (dev_id == BNXT_ULP_DEVICE_ID_THOR) {
		sparms.dir = fparms->dir;
		sparms.data = mapper_fld_zeros;
		sparms.type = fparms->type;
		sparms.data_sz_in_bytes = 16; /* FULL ACT REC SIZE - THOR */
		sparms.idx = fparms->idx;
		sparms.tbl_scope_id = fparms->tbl_scope_id;
		rc = tf_set_tbl_entry(tfp, &sparms);
		if (rc) {
			BNXT_TF_DBG(ERR,
				    "Index table[%s][%s][%x] write fail rc=%d\n",
				    tf_tbl_type_2_str(sparms.type),
				    tf_dir_2_str(sparms.dir),
				    sparms.idx, rc);
			return rc;
		}
	}
	return 0;
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

	if (fparms.type == TF_TBL_TYPE_FULL_ACT_RECORD)
		(void)ulp_mapper_clear_full_action_record(tfp, ulp, &fparms);

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
	uint32_t pc_idx;

	pc_idx = (uint32_t)res->resource_hndl;

	/* reset the child flow bitset*/
	if (ulp_flow_db_pc_db_parent_flow_set(ulp, pc_idx, parent_fid, 0)) {
		BNXT_TF_DBG(ERR, "error in reset parent flow bitset %x:%x\n",
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
	if (ulp_flow_db_pc_db_child_flow_set(ulp, pc_idx, child_fid, 0)) {
		BNXT_TF_DBG(ERR, "error in resetting child flow bitset %x:%x\n",
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
	case BNXT_ULP_PRI_OPC_APP_PRI_OR_CONST:
		if (parms->app_priority)
			*priority = parms->app_priority;
		else
			*priority = tbl->pri_operand;
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

	tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx, tbl->session_type);
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
	BNXT_TF_DBG(DEBUG, "Alloc ident %s:%s.success.\n",
		    tf_dir_2_str(iparms.dir),
		    tf_ident_2_str(iparms.ident_type));

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
		ulp_flow_db_shared_session_set(&fid_parms, tbl->session_type);

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
	tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx, tbl->session_type);
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
	BNXT_TF_DBG(DEBUG, "Search ident %s:%s:%x.success.\n",
		    tf_dir_2_str(sparms.dir),
		    tf_ident_2_str(sparms.ident_type),
		    sparms.search_id);

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
	ulp_flow_db_shared_session_set(&fid_parms, tbl->session_type);

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
	case BNXT_ULP_PORT_TABLE_DRV_FUNC_MAC:
		if (ulp_port_db_drv_mac_addr_get(parms->ulp_ctx, port_id,
						 val)) {
			BNXT_TF_DBG(ERR, "Invalid port id %u\n", port_id);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_PORT_TABLE_DRV_FUNC_PARENT_VNIC:
		if (ulp_port_db_parent_vnic_get(parms->ulp_ctx, port_id,
						val)) {
			BNXT_TF_DBG(ERR, "Invalid port id %u\n", port_id);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_PORT_TABLE_PORT_IS_PF:
		if (ulp_port_db_port_is_pf_get(parms->ulp_ctx, port_id,
					       val)) {
			BNXT_TF_DBG(ERR, "Invalid port id %u\n", port_id);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_PORT_TABLE_VF_FUNC_METADATA:
		if (ulp_port_db_port_meta_data_get(parms->ulp_ctx, port_id,
						   val)) {
			BNXT_TF_DBG(ERR, "Invalid port id %u\n", port_id);
			return -EINVAL;
		}
		break;
	default:
		BNXT_TF_DBG(ERR, "Invalid port_data %d\n", port_data);
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
	struct bnxt_ulp_mapper_data *m;
	uint8_t bit;
	uint32_t port_id, val_size, field_size;
	uint16_t idx, size_idx, offset;
	uint32_t bytelen = ULP_BITS_2_BYTE(bitlen);
	uint8_t *buffer;
	uint64_t lregval;
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
		if (!ulp_operand_read(field_opr,
				      (uint8_t *)&idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "CF operand read failed\n");
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		if (idx >= BNXT_ULP_CF_IDX_LAST || bytelen > sizeof(uint64_t)) {
			BNXT_TF_DBG(ERR, "comp field [%d] read oob %d\n", idx,
				    bytelen);
			return -EINVAL;
		}
		buffer = (uint8_t *)&parms->comp_fld[idx];
		*val = &buffer[sizeof(uint64_t) - bytelen];
		*value = ULP_COMP_FLD_IDX_RD(parms, idx);
		break;
	case BNXT_ULP_FIELD_SRC_RF:
		if (!ulp_operand_read(field_opr,
				      (uint8_t *)&idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "RF operand read failed\n");
			return -EINVAL;
		}

		idx = tfp_be_to_cpu_16(idx);
		/* Uninitialized regfile entries return 0 */
		if (!ulp_regfile_read(parms->regfile, idx, &lregval) ||
		    sizeof(uint64_t) < bytelen) {
			BNXT_TF_DBG(ERR, "regfile[%d] read oob %u\n", idx,
				    bytelen);
			return -EINVAL;
		}
		buffer = (uint8_t *)&parms->regfile->entry[idx].data;
		*val = &buffer[sizeof(uint64_t) - bytelen];
		*value = tfp_be_to_cpu_64(lregval);
		break;
	case BNXT_ULP_FIELD_SRC_ACT_PROP:
		if (!ulp_operand_read(field_opr,
				      (uint8_t *)&idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "Action operand read failed\n");
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		if (idx >= BNXT_ULP_ACT_PROP_IDX_LAST) {
			BNXT_TF_DBG(ERR, "act_prop[%d] oob\n", idx);
			return -EINVAL;
		}
		buffer = &parms->act_prop->act_details[idx];
		field_size = ulp_mapper_act_prop_size_get(idx);
		if (bytelen > field_size) {
			BNXT_TF_DBG(ERR, "act_prop[%d] field size small %u\n",
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
		if (!ulp_operand_read(field_opr,
				      (uint8_t *)&idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "Action sz operand read failed\n");
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);

		if (idx >= BNXT_ULP_ACT_PROP_IDX_LAST) {
			BNXT_TF_DBG(ERR, "act_prop_sz[%d] oob\n", idx);
			return -EINVAL;
		}
		*val = &parms->act_prop->act_details[idx];

		/* get the size index next */
		if (!ulp_operand_read(&field_opr[sizeof(uint16_t)],
				      (uint8_t *)&size_idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "Action sz operand read failed\n");
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
		*val_len = ULP_BYTE_2_BITS(val_size);
		break;
	case BNXT_ULP_FIELD_SRC_GLB_RF:
		if (!ulp_operand_read(field_opr,
				      (uint8_t *)&idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "Global regfile read failed\n");
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		if (ulp_mapper_glb_resource_read(parms->mapper_data,
						 dir, idx, &lregval, &shared) ||
		    sizeof(uint64_t) < bytelen) {
			BNXT_TF_DBG(ERR, "Global regfile[%d] read failed %u\n",
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
		if (!ulp_operand_read(field_opr,
				      (uint8_t *)&idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "Header field read failed\n");
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
			buffer = parms->hdr_field[bit].spec;
		else
			buffer = parms->hdr_field[bit].mask;

		field_size = parms->hdr_field[bit].size;
		if (bytelen > field_size) {
			BNXT_TF_DBG(ERR, "Hdr field[%d] size small %u\n",
				    bit, field_size);
			return -EINVAL;
		}
		if (field_src == BNXT_ULP_FIELD_SRC_HF) {
			*val = &buffer[field_size - bytelen];
		} else {
			/* get the offset next */
			if (!ulp_operand_read(&field_opr[sizeof(uint16_t)],
					      (uint8_t *)&offset,
					      sizeof(uint16_t))) {
				BNXT_TF_DBG(ERR, "Hdr fld size read failed\n");
				return -EINVAL;
			}
			offset = tfp_be_to_cpu_16(offset);
			offset = ULP_BITS_2_BYTE_NR(offset);
			if ((offset + bytelen) > field_size) {
				BNXT_TF_DBG(ERR, "Hdr field[%d] oob\n", bit);
				return -EINVAL;
			}
			*val = &buffer[offset];
		}
		break;
	case BNXT_ULP_FIELD_SRC_HDR_BIT:
		if (!ulp_operand_read(field_opr,
				      (uint8_t *)&lregval, sizeof(uint64_t))) {
			BNXT_TF_DBG(ERR, "Header bit read failed\n");
			return -EINVAL;
		}
		lregval = tfp_be_to_cpu_64(lregval);
		if (ULP_BITMAP_ISSET(parms->hdr_bitmap->bits, lregval)) {
			*val = mapper_fld_one;
			*value = 1;
		} else {
			*val = mapper_fld_zeros;
		}
		break;
	case BNXT_ULP_FIELD_SRC_ACT_BIT:
		if (!ulp_operand_read(field_opr,
				      (uint8_t *)&lregval, sizeof(uint64_t))) {
			BNXT_TF_DBG(ERR, "Action bit read failed\n");
			return -EINVAL;
		}
		lregval = tfp_be_to_cpu_64(lregval);
		if (ULP_BITMAP_ISSET(parms->act_bitmap->bits, lregval)) {
			*val = mapper_fld_one;
			*value = 1;
		} else {
			*val = mapper_fld_zeros;
		}
		break;
	case BNXT_ULP_FIELD_SRC_FIELD_BIT:
		if (!ulp_operand_read(field_opr,
				      (uint8_t *)&idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "Field bit read failed\n");
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		/* get the index from the global field list */
		if (ulp_mapper_glb_field_tbl_get(parms, idx, &bit)) {
			BNXT_TF_DBG(ERR, "invalid ulp_glb_field_tbl idx %d\n",
				    idx);
			return -EINVAL;
		}
		if (ULP_INDEX_BITMAP_GET(parms->fld_bitmap->bits, bit)) {
			*val = mapper_fld_one;
			*value = 1;
		} else {
			*val = mapper_fld_zeros;
		}
		break;
	case BNXT_ULP_FIELD_SRC_PORT_TABLE:
		if (!ulp_operand_read(field_opr,
				      (uint8_t *)&idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "CF operand read failed\n");
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		if (idx >= BNXT_ULP_CF_IDX_LAST || bytelen > sizeof(uint64_t)) {
			BNXT_TF_DBG(ERR, "comp field [%d] read oob %d\n", idx,
				    bytelen);
			return -EINVAL;
		}

		/* The port id is present in the comp field list */
		port_id = ULP_COMP_FLD_IDX_RD(parms, idx);
		/* get the port table enum  */
		if (!ulp_operand_read(field_opr + sizeof(uint16_t),
				      (uint8_t *)&idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "Port table enum read failed\n");
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		if (ulp_mapper_field_port_db_process(parms, port_id, idx,
						     val)) {
			BNXT_TF_DBG(ERR, "field port table failed\n");
			return -EINVAL;
		}
		break;
	case BNXT_ULP_FIELD_SRC_ENC_HDR_BIT:
		if (!ulp_operand_read(field_opr,
				      (uint8_t *)&lregval, sizeof(uint64_t))) {
			BNXT_TF_DBG(ERR, "Header bit read failed\n");
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
		if (!ulp_operand_read(field_opr,
				      (uint8_t *)&idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "Header field read failed\n");
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		/* get the index from the global field list */
		if (idx >= BNXT_ULP_ENC_FIELD_LAST) {
			BNXT_TF_DBG(ERR, "invalid encap field tbl idx %d\n",
				    idx);
			return -EINVAL;
		}
		buffer = parms->enc_field[idx].spec;
		field_size = parms->enc_field[idx].size;
		if (bytelen > field_size) {
			BNXT_TF_DBG(ERR, "Encap field[%d] size small %u\n",
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
	default:
		BNXT_TF_DBG(ERR, "invalid field opcode 0x%x\n", field_src);
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
		if (ulp_blob_pad_push(blob, val_len) < 0) {
			BNXT_TF_DBG(ERR, "too large for blob\n");
			return -EINVAL;
		}
	} else if (fld_src == BNXT_ULP_FIELD_SRC_ACT_PROP_SZ) {
		if (ulp_blob_push_encap(blob, val, val_len) < 0) {
			BNXT_TF_DBG(ERR, "encap blob push failed\n");
			return -EINVAL;
		}
	} else if (fld_src == BNXT_ULP_FIELD_SRC_SKIP) {
		/* do nothing */
	} else {
		if (!ulp_blob_push(blob, val, val_len)) {
			BNXT_TF_DBG(ERR, "push of val1 failed\n");
			return -EINVAL;
		}
	}
	*out_val = val;
	return 0;
}

static int32_t
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
		if (ulp_mapper_field_src_process(parms, fld->field_src1,
						 fld->field_opr1, dir, is_key,
						 fld->field_bit_size, &val1,
						 &val1_len, &value1)) {
			BNXT_TF_DBG(ERR, "fld src1 process failed\n");
			goto error;
		}
		if (eval_src1) {
			if (ulp_mapper_field_buffer_eval(val1, val1_len,
							 &val1_int)) {
				BNXT_TF_DBG(ERR, "fld src1 eval failed\n");
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
	}

	/* process src2 opcode */
	if (process_src2) {
		if (ulp_mapper_field_src_process(parms, fld->field_src2,
						 fld->field_opr2, dir, is_key,
						 fld->field_bit_size, &val2,
						 &val2_len, &value2)) {
			BNXT_TF_DBG(ERR, "fld src2 process failed\n");
			goto error;
		}
		if (eval_src2) {
			if (ulp_mapper_field_buffer_eval(val2, val2_len,
							 &val2_int)) {
				BNXT_TF_DBG(ERR, "fld src2 eval failed\n");
				goto error;
			}
		}
	}

	/* process src3 opcode */
	if (process_src3) {
		if (ulp_mapper_field_src_process(parms, fld->field_src3,
						 fld->field_opr3, dir, is_key,
						 fld->field_bit_size, &val3,
						 &val3_len, &value3)) {
			BNXT_TF_DBG(ERR, "fld src3 process failed\n");
			goto error;
		}
		if (eval_src3) {
			if (ulp_mapper_field_buffer_eval(val3, val3_len,
							 &val3_int)) {
				BNXT_TF_DBG(ERR, "fld src3 eval failed\n");
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
		if (!val)
			rc = -EINVAL;
		break;
	case BNXT_ULP_FIELD_OPC_SRC1_MINUS_SRC2:
	case BNXT_ULP_FIELD_OPC_SRC1_MINUS_SRC2_POST:
		val_int = val1_int - val2_int;
		val_int = tfp_cpu_to_be_64(val_int);
		val = ulp_blob_push_64(blob, &val_int, fld->field_bit_size);
		if (!val)
			rc = -EINVAL;
		break;
	case BNXT_ULP_FIELD_OPC_SRC1_OR_SRC2:
		val_int = val1_int | val2_int;
		val_int = tfp_cpu_to_be_64(val_int);
		val = ulp_blob_push_64(blob, &val_int, fld->field_bit_size);
		if (!val)
			rc = -EINVAL;
		break;
	case BNXT_ULP_FIELD_OPC_SRC1_OR_SRC2_OR_SRC3:
		val_int = val1_int | val2_int | val3_int;
		val_int = tfp_cpu_to_be_64(val_int);
		val = ulp_blob_push_64(blob, &val_int, fld->field_bit_size);
		if (!val)
			rc = -EINVAL;
		break;
	case BNXT_ULP_FIELD_OPC_SRC1_AND_SRC2:
		val_int = val1_int & val2_int;
		val_int = tfp_cpu_to_be_64(val_int);
		val = ulp_blob_push_64(blob, &val_int, fld->field_bit_size);
		if (!val)
			rc = -EINVAL;
		break;
	case BNXT_ULP_FIELD_OPC_SRC1_AND_SRC2_OR_SRC3:
		val_int = val1_int & (val2_int | val3_int);
		val_int = tfp_cpu_to_be_64(val_int);
		val = ulp_blob_push_64(blob, &val_int, fld->field_bit_size);
		if (!val)
			rc = -EINVAL;
		break;
	case BNXT_ULP_FIELD_OPC_SKIP:
		break;
	default:
		BNXT_TF_DBG(ERR, "Invalid fld opcode %u\n", fld->field_opc);
		rc = -EINVAL;
		break;
	}

	if (!rc)
		return rc;
error:
	BNXT_TF_DBG(ERR, "Error in %s:%s process %u:%u\n", name,
		    fld->description, (val) ? write_idx : 0, val_len);
	return -EINVAL;
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
	uint32_t i = 0, num_flds = 0, encap_flds = 0;
	struct ulp_blob encap_blob;
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

	/* process the result fields */
	for (i = 0; i < num_flds; i++) {
		rc = ulp_mapper_field_opc_process(parms, tbl->direction,
						  &dflds[i], data, 0, name);
		if (rc) {
			BNXT_TF_DBG(ERR, "result field processing failed\n");
			return rc;
		}
	}

	/* process encap fields if any */
	if (encap_flds) {
		uint32_t pad = 0;
		/* Initialize the encap blob */
		if (!tbl->record_size &&
		    !parms->device_params->dynamic_sram_en) {
			BNXT_TF_DBG(ERR, "Encap tbl record size incorrect\n");
			return -EINVAL;
		}
		if (!ulp_blob_init(&encap_blob,
				   ULP_BYTE_2_BITS(tbl->record_size),
				   parms->device_params->encap_byte_order)) {
			BNXT_TF_DBG(ERR, "blob inits failed.\n");
			return -EINVAL;
		}
		for (; i < encap_flds; i++) {
			rc = ulp_mapper_field_opc_process(parms, tbl->direction,
							  &dflds[i],
							  &encap_blob, 0, name);
			if (rc) {
				BNXT_TF_DBG(ERR,
					    "encap field processing failed\n");
				return rc;
			}
		}
		/* add the dynamic pad push */
		if (parms->device_params->dynamic_sram_en) {
			uint16_t rec_s = ULP_BYTE_2_BITS(tbl->record_size);

			(void)ulp_mapper_dyn_tbl_type_get(parms, tbl,
							  &encap_blob, &rec_s);
			pad = rec_s - ulp_blob_data_len_get(&encap_blob);
		} else {
			pad = ULP_BYTE_2_BITS(tbl->record_size) -
				ulp_blob_data_len_get(&encap_blob);
		}
		if (ulp_blob_pad_push(&encap_blob, pad) < 0) {
			BNXT_TF_DBG(ERR, "encap buffer padding failed\n");
			return -EINVAL;
		}


		/* perform the 64 bit byte swap */
		ulp_blob_perform_64B_byte_swap(&encap_blob);
		/* Append encap blob to the result blob */
		rc = ulp_blob_buffer_copy(data, &encap_blob);
		if (rc) {
			BNXT_TF_DBG(ERR, "encap buffer copy failed\n");
			return rc;
		}
	}
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
	ulp_flow_db_shared_session_set(&fid_parms, tbl->session_type);

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
	ulp_flow_db_shared_session_set(&fid_parms, tbl->session_type);

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
	ulp_flow_db_shared_session_set(&fid_parms, tbl->session_type);

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

	tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx, tbl->session_type);
	if (!tfp) {
		BNXT_TF_DBG(ERR, "Failed to get truflow pointer\n");
		return -EINVAL;
	}

	sparms.dir		= tbl->direction;
	sparms.tcam_tbl_type	= tbl->resource_type;
	sparms.idx		= idx;
	sparms.key		= ulp_blob_data_get(key, &tmplen);
	sparms.key_sz_in_bits	= tmplen;
	sparms.mask		= ulp_blob_data_get(mask, &tmplen);
	sparms.result		= ulp_blob_data_get(data, &tmplen);
	sparms.result_sz_in_bits = tmplen;
	if (tf_set_tcam_entry(tfp, &sparms)) {
		BNXT_TF_DBG(ERR, "tcam[%s][%s][%x] write failed.\n",
			    tf_tcam_tbl_2_str(sparms.tcam_tbl_type),
			    tf_dir_2_str(sparms.dir), sparms.idx);
		return -EIO;
	}
	BNXT_TF_DBG(DEBUG, "tcam[%s][%s][%x] write success.\n",
		    tf_tcam_tbl_2_str(sparms.tcam_tbl_type),
		    tf_dir_2_str(sparms.dir), sparms.idx);

	/* Mark action */
	rc = ulp_mapper_mark_act_ptr_process(parms, tbl);
	if (rc) {
		BNXT_TF_DBG(ERR, "failed mark action processing\n");
		return rc;
	}

	return rc;
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
static uint32_t
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

	if (num_slices > max_slices) {
		BNXT_TF_DBG(ERR, "Key size (%d) too large for WC\n", blen);
		return -EINVAL;
	}

	/* The key/mask may not be on a natural slice boundary, pad it */
	pad = tlen - blen;
	if (ulp_blob_pad_push(key, pad) < 0 ||
	    ulp_blob_pad_push(mask, pad) < 0) {
		BNXT_TF_DBG(ERR, "Unable to pad key/mask\n");
		return -EINVAL;
	}

	/* The new length accounts for the ctrl word length and num slices */
	tlen = tlen + clen * num_slices;
	if (!ulp_blob_init(tkey, tlen, key->byte_order) ||
	    !ulp_blob_init(tmask, tlen, mask->byte_order)) {
		BNXT_TF_DBG(ERR, "Unable to post process wc tcam entry\n");
		return -EINVAL;
	}

	/* Build the transformed key/mask */
	cword = dparms->wc_mode_list[num_slices - 1];
	cword = tfp_cpu_to_be_32(cword);
	offset = 0;
	for (i = 0; i < num_slices; i++) {
		val = ulp_blob_push_32(tkey, &cword, clen);
		if (!val) {
			BNXT_TF_DBG(ERR, "Key ctrl word push failed\n");
			return -EINVAL;
		}
		val = ulp_blob_push_32(tmask, &cword, clen);
		if (!val) {
			BNXT_TF_DBG(ERR, "Mask ctrl word push failed\n");
			return -EINVAL;
		}
		rc = ulp_blob_append(tkey, key, offset, slice_width);
		if (rc) {
			BNXT_TF_DBG(ERR, "Key blob append failed\n");
			return rc;
		}
		rc = ulp_blob_append(tmask, mask, offset, slice_width);
		if (rc) {
			BNXT_TF_DBG(ERR, "Mask blob append failed\n");
			return rc;
		}
		offset += slice_width;
	}

	/* The key/mask are byte reversed on every 4 byte chunk */
	ulp_blob_perform_byte_reverse(tkey, 4);
	ulp_blob_perform_byte_reverse(tmask, 4);

	return 0;
}

/* internal function to post process the key/mask blobs for wildcard tcam tbl */
static void ulp_mapper_wc_tcam_tbl_post_process(struct ulp_blob *blob)
{
	ulp_blob_perform_64B_word_swap(blob);
	ulp_blob_perform_64B_byte_swap(blob);
}

static int32_t ulp_mapper_tcam_is_wc_tcam(struct bnxt_ulp_mapper_tbl_info *tbl)
{
	if (tbl->resource_type == TF_TCAM_TBL_TYPE_WC_TCAM ||
	    tbl->resource_type == TF_TCAM_TBL_TYPE_WC_TCAM_HIGH ||
	    tbl->resource_type == TF_TCAM_TBL_TYPE_WC_TCAM_LOW)
		return 1;
	return 0;
}

static int32_t
ulp_mapper_tcam_tbl_process(struct bnxt_ulp_mapper_parms *parms,
			    struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct bnxt_ulp_mapper_key_info	*kflds;
	struct ulp_blob okey, omask, data, update_data;
	struct ulp_blob tkey, tmask; /* transform key and mask */
	struct ulp_blob *key, *mask;
	uint32_t i, num_kflds;
	struct tf *tfp;
	int32_t rc, trc;
	struct bnxt_ulp_device_params *dparms = parms->device_params;
	struct tf_alloc_tcam_entry_parms aparms		= { 0 };
	struct tf_search_tcam_entry_parms searchparms   = { 0 };
	struct ulp_flow_db_res_params	fid_parms	= { 0 };
	struct tf_free_tcam_entry_parms free_parms	= { 0 };
	uint32_t hit = 0;
	uint16_t tmplen = 0;
	uint16_t idx;
	enum bnxt_ulp_byte_order key_byte_order;

	/* Set the key and mask to the original key and mask. */
	key = &okey;
	mask = &omask;

	/* Skip this if table opcode is NOP */
	if (tbl->tbl_opcode == BNXT_ULP_TCAM_TBL_OPC_NOT_USED ||
	    tbl->tbl_opcode >= BNXT_ULP_TCAM_TBL_OPC_LAST) {
		BNXT_TF_DBG(ERR, "Invalid tcam table opcode %d\n",
			    tbl->tbl_opcode);
		return 0;
	}

	tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx, tbl->session_type);
	if (!tfp) {
		BNXT_TF_DBG(ERR, "Failed to get truflow pointer\n");
		return -EINVAL;
	}

	/* If only allocation of identifier then perform and exit */
	if (tbl->tbl_opcode == BNXT_ULP_TCAM_TBL_OPC_ALLOC_IDENT) {
		rc = ulp_mapper_tcam_tbl_scan_ident_alloc(parms, tbl);
		return rc;
	}

	kflds = ulp_mapper_key_fields_get(parms, tbl, &num_kflds);
	if (!kflds || !num_kflds) {
		BNXT_TF_DBG(ERR, "Failed to get key fields\n");
		return -EINVAL;
	}

	if (ulp_mapper_tcam_is_wc_tcam(tbl))
		key_byte_order = dparms->wc_key_byte_order;
	else
		key_byte_order = dparms->key_byte_order;

	if (!ulp_blob_init(key, tbl->blob_key_bit_size, key_byte_order) ||
	    !ulp_blob_init(mask, tbl->blob_key_bit_size, key_byte_order) ||
	    !ulp_blob_init(&data, tbl->result_bit_size,
			   dparms->result_byte_order) ||
	    !ulp_blob_init(&update_data, tbl->result_bit_size,
			   dparms->result_byte_order)) {
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
		rc = ulp_mapper_field_opc_process(parms, tbl->direction,
						  &kflds[i].field_info_spec,
						  key, 1, "TCAM Key");
		if (rc) {
			BNXT_TF_DBG(ERR, "Key field set failed %s\n",
				    kflds[i].field_info_spec.description);
			return rc;
		}

		/* Setup the mask */
		rc = ulp_mapper_field_opc_process(parms, tbl->direction,
						  &kflds[i].field_info_mask,
						  mask, 0, "TCAM Mask");
		if (rc) {
			BNXT_TF_DBG(ERR, "Mask field set failed %s\n",
				    kflds[i].field_info_mask.description);
			return rc;
		}
	}

	/* For wild card tcam perform the post process to swap the blob */
	if (ulp_mapper_tcam_is_wc_tcam(tbl)) {
		if (dparms->wc_dynamic_pad_en) {
			/* Sets up the slices for writing to the WC TCAM */
			rc = ulp_mapper_wc_tcam_tbl_dyn_post_process(dparms,
								     key, mask,
								     &tkey,
								     &tmask);
			if (rc) {
				BNXT_TF_DBG(ERR,
					    "Failed to post proc WC entry.\n");
				return rc;
			}
			/* Now need to use the transform Key/Mask */
			key = &tkey;
			mask = &tmask;
		} else {
			ulp_mapper_wc_tcam_tbl_post_process(key);
			ulp_mapper_wc_tcam_tbl_post_process(mask);
		}

	}

	if (tbl->tbl_opcode == BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE) {
		/* allocate the tcam index */
		aparms.dir = tbl->direction;
		aparms.tcam_tbl_type = tbl->resource_type;
		aparms.key = ulp_blob_data_get(key, &tmplen);
		aparms.key_sz_in_bits = tmplen;
		aparms.mask = ulp_blob_data_get(mask, &tmplen);

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
		searchparms.key = ulp_blob_data_get(key, &tmplen);
		searchparms.key_sz_in_bits = tbl->key_bit_size;
		searchparms.mask = ulp_blob_data_get(mask, &tmplen);
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
			BNXT_TF_DBG(ERR, "entry priority process failed\n");
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
			rc = ulp_mapper_tcam_tbl_entry_write(parms, tbl, key,
							     mask, &data, idx);
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
	ulp_flow_db_shared_session_set(&fid_parms, tbl->session_type);

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
	struct tf *tfp;
	struct ulp_flow_db_res_params	fid_parms = { 0 };
	struct tf_insert_em_entry_parms iparms = { 0 };
	struct tf_delete_em_entry_parms free_parms = { 0 };
	enum bnxt_ulp_flow_mem_type mtype;
	struct bnxt_ulp_device_params *dparms = parms->device_params;
	int32_t	trc;
	int32_t rc = 0;
	int32_t pad = 0;
	enum bnxt_ulp_byte_order key_order, res_order;

	tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx, tbl->session_type);
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

	key_order = dparms->em_byte_order;
	res_order = dparms->em_byte_order;

	/* Initialize the key/result blobs */
	if (!ulp_blob_init(&key, tbl->blob_key_bit_size, key_order) ||
	    !ulp_blob_init(&data, tbl->result_bit_size, res_order)) {
		BNXT_TF_DBG(ERR, "blob inits failed.\n");
		return -EINVAL;
	}

	/* create the key */
	for (i = 0; i < num_kflds; i++) {
		/* Setup the key */
		rc = ulp_mapper_field_opc_process(parms, tbl->direction,
						  &kflds[i].field_info_spec,
						  &key, 1, "EM Key");
		if (rc) {
			BNXT_TF_DBG(ERR, "Key field set failed.\n");
			return rc;
		}
	}

	/* if dynamic padding is enabled then add padding to result data */
	if (dparms->em_dynamic_pad_en) {
		/* add padding to make sure key is at byte boundary */
		ulp_blob_pad_align(&key, ULP_BUFFER_ALIGN_8_BITS);

		/* add the pad */
		pad = dparms->em_blk_align_bits - dparms->em_blk_size_bits;
		if (pad < 0) {
			BNXT_TF_DBG(ERR, "Invalid em blk size and align\n");
			return -EINVAL;
		}
		ulp_blob_pad_push(&data, (uint32_t)pad);
	}

	/* Create the result data blob */
	rc = ulp_mapper_tbl_result_build(parms, tbl, &data, "EM Result");
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to build the result blob\n");
		return rc;
	}
	if (dparms->em_dynamic_pad_en) {
		uint32_t abits = dparms->em_blk_align_bits;

		/* when dynamic padding is enabled merge result + key */
		rc = ulp_blob_block_merge(&data, &key, abits, pad);
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed to merge the result blob\n");
			return rc;
		}

		/* add padding to make sure merged result is at slice boundary*/
		ulp_blob_pad_align(&data, abits);

		ulp_blob_perform_byte_reverse(&data, ULP_BITS_2_BYTE(abits));
	}

	/* do the transpose for the internal EM keys */
	if (tbl->resource_type == TF_MEM_INTERNAL) {
		if (dparms->em_key_align_bytes) {
			int32_t b = ULP_BYTE_2_BITS(dparms->em_key_align_bytes);

			tmplen = ulp_blob_data_len_get(&key);
			ulp_blob_pad_push(&key, b - tmplen);
		}
		tmplen = ulp_blob_data_len_get(&key);
		ulp_blob_perform_byte_reverse(&key, ULP_BITS_2_BYTE(tmplen));
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
	iparms.mem			= tbl->resource_type;
	iparms.key			= ulp_blob_data_get(&key, &tmplen);
	iparms.key_sz_in_bits		= tbl->key_bit_size;
	iparms.em_record		= ulp_blob_data_get(&data, &tmplen);
	if (tbl->result_bit_size)
		iparms.em_record_sz_in_bits	= tbl->result_bit_size;
	else
		iparms.em_record_sz_in_bits	= tmplen;

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
	uint32_t index;
	int32_t rc = 0, trc = 0;
	struct tf_alloc_tbl_entry_parms aparms = { 0 };
	struct tf_set_tbl_entry_parms sparms = { 0 };
	struct tf_get_tbl_entry_parms gparms = { 0 };
	struct tf_free_tbl_entry_parms free_parms = { 0 };
	uint32_t tbl_scope_id;
	struct tf *tfp;
	struct bnxt_ulp_glb_resource_info glb_res = { 0 };
	uint16_t bit_size;
	bool alloc = false;
	bool write = false;
	bool global = false;
	uint64_t act_rec_size;
	bool shared = false;
	enum tf_tbl_type tbl_type = tbl->resource_type;

	tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx, tbl->session_type);
	/* compute the blob size */
	bit_size = ulp_mapper_dyn_blob_size_get(parms, tbl);

	/* Initialize the blob data */
	if (!ulp_blob_init(&data, bit_size,
			   parms->device_params->result_byte_order)) {
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
	case BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_GLB_REGFILE:
		/*
		 * Build the entry, alloc an index, write the table, and store
		 * the data in the global regfile.
		 */
		alloc = true;
		global = true;
		write = true;
		glb_res.direction = tbl->direction;
		glb_res.resource_func = tbl->resource_func;
		glb_res.resource_type = tbl->resource_type;
		glb_res.glb_regfile_index = tbl->tbl_operand;
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
						 &regval, &shared)) {
			BNXT_TF_DBG(ERR,
				    "Failed to get tbl idx from Glb RF[%d].\n",
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
			BNXT_TF_DBG(ERR,
				    "Failed to get flds on tbl read rc=%d\n",
				    rc);
			return rc;
		}
		return 0;
	default:
		BNXT_TF_DBG(ERR, "Invalid index table opcode %d\n",
			    tbl->tbl_opcode);
		return -EINVAL;
	}

	if (write) {
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

	if (alloc) {
		aparms.dir		= tbl->direction;
		tbl_type = ulp_mapper_dyn_tbl_type_get(parms, tbl,
						       &data, &tmplen);
		aparms.type = tbl_type;
		aparms.tbl_scope_id	= tbl_scope_id;

		/* All failures after the alloc succeeds require a free */
		rc = tf_alloc_tbl_entry(tfp, &aparms);
		if (rc) {
			BNXT_TF_DBG(ERR, "Alloc table[%s][%s] failed rc=%d\n",
				    tf_tbl_type_2_str(aparms.type),
				    tf_dir_2_str(tbl->direction), rc);
			return rc;
		}
		index = aparms.idx;

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
		regval = tfp_cpu_to_be_64(regval);

		if (global) {
			/*
			 * Shared resources are never allocated through this
			 * method, so the shared flag is always false.
			 */
			rc = ulp_mapper_glb_resource_write(parms->mapper_data,
							   &glb_res, regval,
							   false);
		} else {
			rc = ulp_regfile_write(parms->regfile,
					       tbl->tbl_operand, regval);
		}
		if (rc) {
			BNXT_TF_DBG(ERR,
				    "Failed to write %s regfile[%d] rc=%d\n",
				    (global) ? "global" : "reg",
				    tbl->tbl_operand, rc);
			goto error;
		}
	}

	if (write) {
		sparms.dir = tbl->direction;
		sparms.data = ulp_blob_data_get(&data, &tmplen);
		tbl_type = ulp_mapper_dyn_tbl_type_get(parms, tbl, &data,
						       &tmplen);
		sparms.type = tbl_type;
		sparms.data_sz_in_bytes = ULP_BITS_2_BYTE(tmplen);
		sparms.idx = index;
		sparms.tbl_scope_id = tbl_scope_id;
		if (shared)
			tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx,
						     tbl->session_type);
		rc = tf_set_tbl_entry(tfp, &sparms);
		if (rc) {
			BNXT_TF_DBG(ERR,
				    "Index table[%s][%s][%x] write fail rc=%d\n",
				    tf_tbl_type_2_str(sparms.type),
				    tf_dir_2_str(sparms.dir),
				    sparms.idx, rc);
			goto error;
		}
		BNXT_TF_DBG(DEBUG, "Index table[%s][%s][%x] write successful\n",
			    tf_tbl_type_2_str(sparms.type),
			    tf_dir_2_str(sparms.dir), sparms.idx);

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
	fid_parms.resource_type	= tbl_type;
	fid_parms.resource_sub_type = tbl->resource_sub_type;
	fid_parms.resource_hndl	= index;
	fid_parms.critical_resource = tbl->critical_resource;
	ulp_flow_db_shared_session_set(&fid_parms, tbl->session_type);

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
	/* Shared resources are not freed */
	if (shared)
		return rc;
	/*
	 * Free the allocated resource since we failed to either
	 * write to the entry or link the flow
	 */
	free_parms.dir	= tbl->direction;
	free_parms.type	= tbl_type;
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
	struct tf *tfp;
	enum bnxt_ulp_if_tbl_opc if_opc = tbl->tbl_opcode;
	uint32_t res_size;

	tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx, tbl->session_type);
	/* Initialize the blob data */
	if (!ulp_blob_init(&data, tbl->result_bit_size,
			   parms->device_params->result_byte_order)) {
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
				   parms->device_params->result_byte_order)) {
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
	BNXT_TF_INF("Set table[%s][%s][%x] success.\n",
		    tf_if_tbl_2_str(iftbl_params.type),
		    tf_dir_2_str(iftbl_params.dir),
		    iftbl_params.idx);

	/*
	 * TBD: Need to look at the need to store idx in flow db for restore
	 * the table to its original state on deletion of this entry.
	 */
	return rc;
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
		BNXT_TF_DBG(ERR, "Invalid REF_CNT_OPC %d\n",
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
		if (rc) {
			BNXT_TF_DBG(ERR,
				    "Failed to write regfile[ref_cnt]\n");
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
	uint16_t tmplen = 0;
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
			   parms->device_params->key_byte_order)) {
		BNXT_TF_DBG(ERR, "Failed to alloc blob\n");
		return -EINVAL;
	}
	for (i = 0; i < num_kflds; i++) {
		/* Setup the key */
		rc = ulp_mapper_field_opc_process(parms, tbl->direction,
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
		if (ULP_BITS_2_BYTE(tmplen) > (int32_t)sizeof(key_index)) {
			BNXT_TF_DBG(ERR, "%s: keysize is bigger then 4 bytes\n",
				    gen_tbl_list->gen_tbl_name);
			return -EINVAL;
		}
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

		/* check the reference count and ignore ref_cnt if NOP.
		 * NOP allows a write as an update.
		 */

		if (tbl->ref_cnt_opcode != BNXT_ULP_REF_CNT_OPC_NOP &&
		    ULP_GEN_TBL_REF_CNT(&gen_tbl_ent)) {
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
		ulp_flow_db_shared_session_set(&fid_parms, tbl->session_type);

		rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
		if (rc) {
			BNXT_TF_DBG(ERR, "Fail to add gen ent flowdb %d\n", rc);
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
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed to do fdb alloc\n");
			return rc;
		}
	} else if (tbl->fdb_opcode == BNXT_ULP_FDB_OPC_DELETE_RID_REGFILE) {
		rc = ulp_regfile_read(parms->regfile, tbl->fdb_operand, &val64);
		if (!rc) {
			BNXT_TF_DBG(ERR, "Failed to get RID from regfile\n");
			return rc;
		}
		rid = (uint32_t)tfp_be_to_cpu_64(val64);
		rc = ulp_mapper_resources_free(parms->ulp_ctx,
					       BNXT_ULP_FDB_TYPE_RID,
					       rid);
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
		if (tbl->tbl_opcode != BNXT_ULP_VNIC_TBL_OPC_ALLOC_WR_REGFILE) {
			BNXT_TF_DBG(ERR, "Invalid vnic table opcode\n");
			return -EINVAL;
		}
		rc = bnxt_pmd_rss_action_create(parms, &vnic_idx, &vnic_id);
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed create rss action\n");
			return rc;
		}
		break;
	case BNXT_ULP_RESOURCE_SUB_TYPE_VNIC_TABLE_QUEUE:
		if (tbl->tbl_opcode != BNXT_ULP_VNIC_TBL_OPC_ALLOC_WR_REGFILE) {
			BNXT_TF_DBG(ERR, "Invalid vnic table opcode\n");
			return -EINVAL;
		}
		rc = bnxt_pmd_queue_action_create(parms, &vnic_idx, &vnic_id);
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed create queue action\n");
			return rc;
		}
		break;
	default:
		BNXT_TF_DBG(ERR, "Invalid vnic table sub type\n");
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
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to link resource to flow rc = %d\n",
			    rc);
		return rc;
	}
	rc = ulp_regfile_write(parms->regfile, tbl->tbl_operand,
			       (uint64_t)tfp_cpu_to_be_64(vnic_id));
	if (rc)
		BNXT_TF_DBG(ERR, "Failed to write regfile[%d] rc=%d\n",
			    tbl->tbl_operand, rc);

	return rc;
}

/* Free the vnic resource */
static int32_t
ulp_mapper_vnic_tbl_res_free(struct bnxt_ulp_context *ulp __rte_unused,
			     struct tf *tfp,
			     struct ulp_flow_db_res_params *res)
{
	uint16_t vnic_idx = res->resource_hndl;

	if (res->resource_sub_type ==
	    BNXT_ULP_RESOURCE_SUB_TYPE_VNIC_TABLE_QUEUE)
		return bnxt_pmd_queue_action_delete(tfp, vnic_idx);
	else
		return bnxt_pmd_rss_action_delete(tfp, vnic_idx);
}

static int32_t
ulp_mapper_global_res_free(struct bnxt_ulp_context *ulp __rte_unused,
			   struct tf *tfp __rte_unused,
			   struct ulp_flow_db_res_params *res)
{
	uint16_t port_id = 0, dport = 0; /* Not needed for free */
	int32_t rc = 0;
	uint8_t ttype;
	uint32_t handle = res->resource_hndl;

	switch (res->resource_sub_type) {
	case BNXT_ULP_RESOURCE_SUB_TYPE_GLOBAL_REGISTER_CUST_VXLAN:
		ttype = BNXT_GLOBAL_REGISTER_TUNNEL_VXLAN;
		rc = bnxt_pmd_global_tunnel_set(port_id, ttype, dport,
						&handle);
		break;
	case BNXT_ULP_RESOURCE_SUB_TYPE_GLOBAL_REGISTER_CUST_ECPRI:
		ttype = BNXT_GLOBAL_REGISTER_TUNNEL_ECPRI;
		rc = bnxt_pmd_global_tunnel_set(port_id, ttype, dport,
						&handle);
		break;
	default:
		rc = -EINVAL;
		BNXT_TF_DBG(ERR, "Invalid ulp global resource type %d\n",
			    res->resource_sub_type);
		break;
	}

	return rc;
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
	uint32_t handle;
	int32_t rc = 0, write_reg = 0;
	uint8_t ttype;

	/* Initialize the blob data */
	if (!ulp_blob_init(&data, tbl->result_bit_size,
			   BNXT_ULP_BYTE_ORDER_BE)) {
		BNXT_TF_DBG(ERR, "Failed initial ulp_global table blob\n");
		return -EINVAL;
	}

	/* read the arguments from the result table */
	rc = ulp_mapper_tbl_result_build(parms, tbl, &data,
					 "ULP Global Result");
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to build the result blob\n");
		return rc;
	}

	switch (tbl->tbl_opcode) {
	case BNXT_ULP_GLOBAL_REGISTER_TBL_OPC_WR_REGFILE:
		write_reg = 1;
		break;
	case BNXT_ULP_GLOBAL_REGISTER_TBL_OPC_NOT_USED:
		break;
	default:
		BNXT_TF_DBG(ERR, "Invalid global table opcode %d\n",
			    tbl->tbl_opcode);
		return -EINVAL;
	}

	switch (tbl->resource_sub_type) {
	case BNXT_ULP_RESOURCE_SUB_TYPE_GLOBAL_REGISTER_CUST_VXLAN:
		tmp_data = ulp_blob_data_get(&data, &data_len);
		udp_port = *((uint16_t *)tmp_data);
		udp_port = tfp_be_to_cpu_16(udp_port);
		ttype = BNXT_GLOBAL_REGISTER_TUNNEL_VXLAN;

		rc = bnxt_pmd_global_tunnel_set(parms->port_id, ttype,
						udp_port, &handle);
		if (rc) {
			BNXT_TF_DBG(ERR, "Unable to set VXLAN UDP port\n");
			return rc;
		}
		break;
	case BNXT_ULP_RESOURCE_SUB_TYPE_GLOBAL_REGISTER_CUST_ECPRI:
		tmp_data = ulp_blob_data_get(&data, &data_len);
		udp_port = *((uint16_t *)tmp_data);
		udp_port = tfp_be_to_cpu_16(udp_port);
		ttype = BNXT_GLOBAL_REGISTER_TUNNEL_ECPRI;

		rc = bnxt_pmd_global_tunnel_set(parms->port_id, ttype,
						udp_port, &handle);
		if (rc) {
			BNXT_TF_DBG(ERR, "Unable to set eCPRI UDP port\n");
			return rc;
		}
	break;
	default:
		rc = -EINVAL;
		BNXT_TF_DBG(ERR, "Invalid ulp global resource type %d\n",
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

	if (rc)
		return rc;

	/* write to the regfile if opcode is set */
	if (write_reg) {
		rc = ulp_regfile_write(parms->regfile,
				       tbl->tbl_operand,
				       (uint64_t)tfp_cpu_to_be_64(handle));
		if (rc)
			BNXT_TF_DBG(ERR, "Regfile[%d] write failed.\n",
				    tbl->tbl_operand);
	}

	return rc;
}

static int32_t
ulp_mapper_glb_resource_info_init(struct bnxt_ulp_context *ulp_ctx,
				  struct bnxt_ulp_mapper_data *mapper_data)
{
	struct bnxt_ulp_glb_resource_info *glb_res;
	uint32_t num_glb_res_ids, idx, dev_id;
	uint8_t app_id;
	int32_t rc = 0;

	glb_res = ulp_mapper_glb_resource_info_list_get(&num_glb_res_ids);
	if (!glb_res || !num_glb_res_ids) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to get device id for glb init (%d)\n",
			    rc);
		return rc;
	}

	rc = bnxt_ulp_cntxt_app_id_get(ulp_ctx, &app_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to get app id for glb init (%d)\n",
			    rc);
		return rc;
	}

	/* Iterate the global resources and process each one */
	for (idx = 0; idx < num_glb_res_ids; idx++) {
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

static int32_t
ulp_mapper_app_glb_resource_info_init(struct bnxt_ulp_context *ulp_ctx,
				  struct bnxt_ulp_mapper_data *mapper_data)
{
	struct bnxt_ulp_glb_resource_info *glb_res;
	uint32_t num_glb_res_ids, idx, dev_id;
	uint8_t app_id;
	int32_t rc = 0;

	glb_res = bnxt_ulp_app_glb_resource_info_list_get(&num_glb_res_ids);
	if (!glb_res || !num_glb_res_ids) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to get device id for glb init (%d)\n",
			    rc);
		return rc;
	}

	rc = bnxt_ulp_cntxt_app_id_get(ulp_ctx, &app_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to get app id for glb init (%d)\n",
			    rc);
		return rc;
	}

	/* Iterate the global resources and process each one */
	for (idx = 0; idx < num_glb_res_ids; idx++) {
		if (dev_id != glb_res[idx].device_id ||
		    glb_res[idx].app_id != app_id)
			continue;
		switch (glb_res[idx].resource_func) {
		case BNXT_ULP_RESOURCE_FUNC_IDENTIFIER:
			rc = ulp_mapper_resource_ident_allocate(ulp_ctx,
								mapper_data,
								&glb_res[idx],
								true);
			break;
		case BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE:
			rc = ulp_mapper_resource_index_tbl_alloc(ulp_ctx,
								 mapper_data,
								 &glb_res[idx],
								 true);
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
	uint64_t regval, result = 0;

	switch (opc) {
	case BNXT_ULP_COND_OPC_CF_IS_SET:
		if (operand < BNXT_ULP_CF_IDX_LAST) {
			result = ULP_COMP_FLD_IDX_RD(parms, operand);
		} else {
			BNXT_TF_DBG(ERR,
				    "comp field out of bounds %" PRIu64 "\n",
				    operand);
			rc = -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_CF_NOT_SET:
		if (operand < BNXT_ULP_CF_IDX_LAST) {
			result = !ULP_COMP_FLD_IDX_RD(parms, operand);
		} else {
			BNXT_TF_DBG(ERR,
				    "comp field out of bounds %" PRIu64 "\n",
				    operand);
			rc = -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_ACT_BIT_IS_SET:
		if (operand < BNXT_ULP_ACT_BIT_LAST) {
			result = ULP_BITMAP_ISSET(parms->act_bitmap->bits,
						operand);
		} else {
			BNXT_TF_DBG(ERR,
				    "action bit out of bounds %" PRIu64 "\n",
				    operand);
			rc = -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_ACT_BIT_NOT_SET:
		if (operand < BNXT_ULP_ACT_BIT_LAST) {
			result = !ULP_BITMAP_ISSET(parms->act_bitmap->bits,
					       operand);
		} else {
			BNXT_TF_DBG(ERR,
				    "action bit out of bounds %" PRIu64 "\n",
				    operand);
			rc = -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_HDR_BIT_IS_SET:
		if (operand < BNXT_ULP_HDR_BIT_LAST) {
			result = ULP_BITMAP_ISSET(parms->hdr_bitmap->bits,
						operand);
		} else {
			BNXT_TF_DBG(ERR,
				    "header bit out of bounds %" PRIu64 "\n",
				    operand);
			rc = -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_HDR_BIT_NOT_SET:
		if (operand < BNXT_ULP_HDR_BIT_LAST) {
			result = !ULP_BITMAP_ISSET(parms->hdr_bitmap->bits,
					       operand);
		} else {
			BNXT_TF_DBG(ERR,
				    "header bit out of bounds %" PRIu64 "\n",
				    operand);
			rc = -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_FIELD_BIT_IS_SET:
		rc = ulp_mapper_glb_field_tbl_get(parms, operand, &bit);
		if (rc) {
			BNXT_TF_DBG(ERR,
				    "invalid ulp_glb_field_tbl idx %" PRIu64 "\n",
				    operand);
			return -EINVAL;
		}
		result = ULP_INDEX_BITMAP_GET(parms->fld_bitmap->bits, bit);
		break;
	case BNXT_ULP_COND_OPC_FIELD_BIT_NOT_SET:
		rc = ulp_mapper_glb_field_tbl_get(parms, operand, &bit);
		if (rc) {
			BNXT_TF_DBG(ERR,
				    "invalid ulp_glb_field_tbl idx %" PRIu64 "\n",
				    operand);
			return -EINVAL;
		}
		result = !ULP_INDEX_BITMAP_GET(parms->fld_bitmap->bits, bit);
		break;
	case BNXT_ULP_COND_OPC_RF_IS_SET:
		if (!ulp_regfile_read(parms->regfile, operand, &regval)) {
			BNXT_TF_DBG(ERR,
				    "regfile[%" PRIu64 "] read oob\n",
				    operand);
			return -EINVAL;
		}
		result = regval != 0;
		break;
	case BNXT_ULP_COND_OPC_RF_NOT_SET:
		if (!ulp_regfile_read(parms->regfile, operand, &regval)) {
			BNXT_TF_DBG(ERR,
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
		if (bnxt_ulp_cntxt_mem_type_get(parms->ulp_ctx, &mtype)) {
			BNXT_TF_DBG(ERR, "Failed to get the mem type\n");
			return -EINVAL;
		}
		result = (mtype == BNXT_ULP_FLOW_MEM_TYPE_INT) ? 0 : 1;
		break;
	case BNXT_ULP_COND_OPC_EXT_MEM_NOT_SET:
		if (bnxt_ulp_cntxt_mem_type_get(parms->ulp_ctx, &mtype)) {
			BNXT_TF_DBG(ERR, "Failed to get the mem type\n");
			return -EINVAL;
		}
		result = (mtype == BNXT_ULP_FLOW_MEM_TYPE_INT) ? 1 : 0;
		break;
	case BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET:
		if (operand < BNXT_ULP_HDR_BIT_LAST) {
			result = ULP_BITMAP_ISSET(parms->enc_hdr_bitmap->bits,
						operand);
		} else {
			BNXT_TF_DBG(ERR,
				    "header bit out of bounds %" PRIu64 "\n",
				    operand);
			rc = -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_ENC_HDR_BIT_NOT_SET:
		if (operand < BNXT_ULP_HDR_BIT_LAST) {
			result = !ULP_BITMAP_ISSET(parms->enc_hdr_bitmap->bits,
						 operand);
		} else {
			BNXT_TF_DBG(ERR,
				    "header bit out of bounds %" PRIu64 "\n",
				    operand);
			rc = -EINVAL;
		}
		break;
	case BNXT_ULP_COND_OPC_ACT_PROP_IS_SET:
	case BNXT_ULP_COND_OPC_ACT_PROP_NOT_SET:
		/* only supporting 1-byte action properties for now */
		if (operand >= BNXT_ULP_ACT_PROP_IDX_LAST) {
			BNXT_TF_DBG(ERR,
				    "act_prop[%" PRIu64 "] oob\n", operand);
			return -EINVAL;
		}
		field_size = ulp_mapper_act_prop_size_get(operand);
		if (sizeof(tmp) != field_size) {
			BNXT_TF_DBG(ERR,
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
	default:
		BNXT_TF_DBG(ERR, "Invalid conditional opcode %d\n", opc);
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
			    uint16_t func_opr,
			    uint64_t *result)
{
	uint64_t regval;
	bool shared;

	*result =  false;
	switch (func_src) {
	case BNXT_ULP_FUNC_SRC_COMP_FIELD:
		if (func_opr >= BNXT_ULP_CF_IDX_LAST) {
			BNXT_TF_DBG(ERR, "invalid index %u\n", func_opr);
			return -EINVAL;
		}
		*result = ULP_COMP_FLD_IDX_RD(parms, func_opr);
		break;
	case BNXT_ULP_FUNC_SRC_REGFILE:
		if (!ulp_regfile_read(parms->regfile, func_opr, &regval)) {
			BNXT_TF_DBG(ERR, "regfile[%d] read oob\n", func_opr);
			return -EINVAL;
		}
		*result = tfp_be_to_cpu_64(regval);
		break;
	case BNXT_ULP_FUNC_SRC_GLB_REGFILE:
		if (ulp_mapper_glb_resource_read(parms->mapper_data, dir,
						 func_opr, &regval, &shared)) {
			BNXT_TF_DBG(ERR, "global regfile[%d] read failed.\n",
				    func_opr);
			return -EINVAL;
		}
		*result = tfp_be_to_cpu_64(regval);
		break;
	case BNXT_ULP_FUNC_SRC_CONST:
		*result = func_opr;
		break;
	default:
		BNXT_TF_DBG(ERR, "invalid src code %u\n", func_src);
		return -EINVAL;
	}
	return 0;
}

static int32_t
ulp_mapper_func_info_process(struct bnxt_ulp_mapper_parms *parms,
			     struct bnxt_ulp_mapper_tbl_info *tbl)
{
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
		process_src1 = 1;
		process_src2 = 1;
		break;
	case BNXT_ULP_FUNC_OPC_COPY_SRC1_TO_RF:
		process_src1 = 1;
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
	case BNXT_ULP_FUNC_OPC_COPY_SRC1_TO_RF:
		res = res1;
		break;
	case BNXT_ULP_FUNC_OPC_RSS_CONFIG:
		/* apply the rss config using pmd method */
		return bnxt_rss_config_action_apply(parms);
	case BNXT_ULP_FUNC_OPC_GET_PARENT_MAC_ADDR:
		rc = bnxt_pmd_get_parent_mac_addr(parms, (uint8_t *)&res);
		if (rc)
			return -EINVAL;
		res = tfp_be_to_cpu_64(res);
		break;
	default:
		BNXT_TF_DBG(ERR, "invalid func code %u\n", func_info->func_opc);
		return -EINVAL;
	}
	if (ulp_regfile_write(parms->regfile, func_info->func_dst_opr,
			      tfp_cpu_to_be_64(res))) {
		BNXT_TF_DBG(ERR, "Failed write the func_opc %u\n",
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
		*res = 0;
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
		comp_sig = ULP_COMP_FLD_IDX_RD(parms,
					       BNXT_ULP_CF_IDX_FLOW_SIG_ID);
		regval = tfp_be_to_cpu_64(regval);
		if (comp_sig == regval)
			*res = 1;
		else
			BNXT_TF_DBG(ERR, "failed signature match 0x%016"
				    PRIX64 ":%x\n", comp_sig, (uint32_t)regval);
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
		cond_goto = tbl->execute_info.cond_true_goto;
		/* Process the conditional func code opcodes */
		if (ulp_mapper_func_info_process(parms, tbl)) {
			BNXT_TF_DBG(ERR, "Failed to process cond update\n");
			rc = -EINVAL;
			goto error;
		}

		cond_tbls = ulp_mapper_tbl_execute_list_get(parms, tbl,
							    &num_cond_tbls,
							    &cond_opc);
		rc = ulp_mapper_cond_opc_list_process(parms, cond_opc,
						      cond_tbls, num_cond_tbls,
						      &cond_rc);
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed to proc cond opc list (%d)\n",
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
		case BNXT_ULP_RESOURCE_FUNC_VNIC_TABLE:
			rc = ulp_mapper_vnic_tbl_process(parms, tbl);
			break;
		case BNXT_ULP_RESOURCE_FUNC_GLOBAL_REGISTER_TABLE:
			rc = ulp_mapper_global_register_tbl_process(parms, tbl);
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
		if (cond_goto == BNXT_ULP_COND_GOTO_REJECT) {
			BNXT_TF_DBG(ERR, "reject the flow\n");
			rc = -EINVAL;
			goto error;
		} else if (cond_goto & BNXT_ULP_COND_GOTO_RF) {
			uint32_t rf_idx;
			uint64_t regval;

			/* least significant 16 bits from reg_file index */
			rf_idx = (uint32_t)(cond_goto & 0xFFFF);
			if (!ulp_regfile_read(parms->regfile, rf_idx,
					      &regval)) {
				BNXT_TF_DBG(ERR, "regfile[%d] read oob\n",
					    rf_idx);
				rc = -EINVAL;
				goto error;
			}
			cond_goto = (int32_t)regval;
		}

		if (cond_goto < 0 && ((int32_t)tbl_idx + cond_goto) < 0) {
			BNXT_TF_DBG(ERR, "invalid conditional goto %d\n",
				    cond_goto);
			goto error;
		}
		tbl_idx += cond_goto;
	}

	return rc;
error:
	BNXT_TF_DBG(ERR, "%s tables failed operation for %d:%d\n",
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
	tfp = bnxt_ulp_cntxt_tfp_get(ulp, ulp_flow_db_shared_session_get(res));
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
		rc = ulp_mapper_gen_tbl_res_free(ulp, fid, res);
		break;
	case BNXT_ULP_RESOURCE_FUNC_VNIC_TABLE:
		rc = ulp_mapper_vnic_tbl_res_free(ulp, tfp, res);
		break;
	case BNXT_ULP_RESOURCE_FUNC_GLOBAL_REGISTER_TABLE:
		rc = ulp_mapper_global_res_free(ulp, tfp, res);
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
	parms.enc_hdr_bitmap = cparms->enc_hdr_bitmap;
	parms.regfile = &regfile;
	parms.hdr_field = cparms->hdr_field;
	parms.enc_field = cparms->enc_field;
	parms.fld_bitmap = cparms->fld_bitmap;
	parms.comp_fld = cparms->comp_fld;
	parms.ulp_ctx = ulp_ctx;
	parms.act_tid = cparms->act_tid;
	parms.class_tid = cparms->class_tid;
	parms.flow_type = cparms->flow_type;
	parms.parent_flow = cparms->parent_flow;
	parms.child_flow = cparms->child_flow;
	parms.fid = cparms->flow_id;
	parms.tun_idx = cparms->tun_idx;
	parms.app_priority = cparms->app_priority;
	parms.flow_pattern_id = cparms->flow_pattern_id;
	parms.act_pattern_id = cparms->act_pattern_id;
	parms.app_id = cparms->app_id;
	parms.port_id = cparms->port_id;

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
		cparms->shared_hndl = parms.shared_hndl;
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
	} else if (parms.child_flow) {
		/* create a child flow details */
		rc = ulp_flow_db_child_flow_create(&parms);
		if (rc)
			goto flow_error;
	}

	return rc;

flow_error:
	if (parms.rid) {
		/* An RID was in-flight but not pushed, free the resources */
		trc = ulp_mapper_flow_destroy(ulp_ctx, BNXT_ULP_FDB_TYPE_RID,
					      parms.rid);
		if (trc)
			BNXT_TF_DBG(ERR,
				    "Failed to free resources rid=0x%08x rc=%d\n",
				    parms.rid, trc);
		parms.rid = 0;
	}

	/* Free all resources that were allocated during flow creation */
	if (parms.fid) {
		trc = ulp_mapper_flow_destroy(ulp_ctx, parms.flow_type,
					      parms.fid);
		if (trc)
			BNXT_TF_DBG(ERR,
				    "Failed to free resources fid=0x%08x rc=%d\n",
				    parms.fid, trc);
	}

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

	tfp = bnxt_ulp_cntxt_tfp_get(ulp_ctx, BNXT_ULP_SESSION_TYPE_DEFAULT);
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

	/*
	 * Only initialize the app global resources if a shared session was
	 * created.
	 */
	if (bnxt_ulp_cntxt_shared_session_enabled(ulp_ctx)) {
		rc = ulp_mapper_app_glb_resource_info_init(ulp_ctx, data);
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed to init app glb resources\n");
			goto error;
		}
	}

	/* Allocate the generic table list */
	rc = ulp_mapper_generic_tbl_list_init(data);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to initialize generic tbl list\n");
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
			    "Failed to acquire ulp context, so data may not be released.\n");
		return;
	}

	data = (struct bnxt_ulp_mapper_data *)
		bnxt_ulp_cntxt_ptr2_mapper_data_get(ulp_ctx);
	if (!data) {
		/* Go ahead and return since there is no allocated data. */
		BNXT_TF_DBG(ERR, "No data appears to have been allocated.\n");
		return;
	}

	tfp = bnxt_ulp_cntxt_tfp_get(ulp_ctx, BNXT_ULP_SESSION_TYPE_DEFAULT);
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
