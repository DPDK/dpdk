/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
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
#include "ulp_flow_db.h"
#include "ulp_mapper.h"
#include "tf_util.h"

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
	    dir >= TF_DIR_MAX || idx >= BNXT_ULP_GLB_REGFILE_INDEX_LAST)
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
	    res->glb_regfile_index >= BNXT_ULP_GLB_REGFILE_INDEX_LAST)
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
			    (iparms.dir == TF_DIR_RX) ? "RX" : "TX",
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
	aparms.search_enable = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO;
	aparms.tbl_scope_id = tbl_scope_id;

	/* Allocate the index tbl using tf api */
	rc = tf_alloc_tbl_entry(tfp, &aparms);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to alloc identifier [%s][%d]\n",
			    (aparms.dir == TF_DIR_RX) ? "RX" : "TX",
			    aparms.type);
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

/* Retrieve the cache initialization parameters for the tbl_idx */
static struct bnxt_ulp_cache_tbl_params *
ulp_mapper_cache_tbl_params_get(uint32_t tbl_idx)
{
	if (tbl_idx >= BNXT_ULP_CACHE_TBL_MAX_SZ)
		return NULL;

	return &ulp_cache_tbl_params[tbl_idx];
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
 * Get the list of result fields that implement the flow action.
 * Gets a device dependent list of tables that implement the action template id.
 *
 * dev_id [in] The device id of the forwarding element
 *
 * tid [in] The action template id that matches the flow
 *
 * num_tbls [out] The number of action tables in the returned array
 *
 * Returns An array of action tables to implement the flow, or NULL on error.
 */
static struct bnxt_ulp_mapper_tbl_info *
ulp_mapper_action_tbl_list_get(uint32_t dev_id,
			       uint32_t tid,
			       uint32_t *num_tbls)
{
	uint32_t	idx;
	uint32_t	tidx;

	if (!num_tbls) {
		BNXT_TF_DBG(ERR, "Invalid arguments\n");
		return NULL;
	}

	/* template shift and device mask */
	tidx = ULP_DEVICE_PARAMS_INDEX(tid, dev_id);

	/* NOTE: Need to have something from template compiler to help validate
	 * range of dev_id and act_tid
	 */
	idx		= ulp_act_tmpl_list[tidx].start_tbl_idx;
	*num_tbls	= ulp_act_tmpl_list[tidx].num_tbls;

	return &ulp_act_tbl_list[idx];
}

/*
 * Get a list of classifier tables that implement the flow
 * Gets a device dependent list of tables that implement the class template id
 *
 * dev_id [in] The device id of the forwarding element
 *
 * tid [in] The template id that matches the flow
 *
 * num_tbls [out] The number of classifier tables in the returned array
 *
 * fdb_tbl_idx [out] The flow database index Regular or default
 *
 * returns An array of classifier tables to implement the flow, or NULL on
 * error
 */
static struct bnxt_ulp_mapper_tbl_info *
ulp_mapper_class_tbl_list_get(uint32_t dev_id,
			      uint32_t tid,
			      uint32_t *num_tbls,
			      uint32_t *fdb_tbl_idx)
{
	uint32_t idx;
	uint32_t tidx = ULP_DEVICE_PARAMS_INDEX(tid, dev_id);

	if (!num_tbls)
		return NULL;

	/* NOTE: Need to have something from template compiler to help validate
	 * range of dev_id and tid
	 */
	idx		= ulp_class_tmpl_list[tidx].start_tbl_idx;
	*num_tbls	= ulp_class_tmpl_list[tidx].num_tbls;
	*fdb_tbl_idx = ulp_class_tmpl_list[tidx].flow_db_table_type;
	return &ulp_class_tbl_list[idx];
}

/*
 * Get the list of key fields that implement the flow.
 *
 * ctxt [in] The ulp context
 *
 * tbl [in] A single table instance to get the key fields from
 *
 * num_flds [out] The number of key fields in the returned array
 *
 * Returns array of Key fields, or NULL on error.
 */
static struct bnxt_ulp_mapper_class_key_field_info *
ulp_mapper_key_fields_get(struct bnxt_ulp_mapper_tbl_info *tbl,
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
ulp_mapper_result_fields_get(struct bnxt_ulp_mapper_tbl_info *tbl,
			     uint32_t *num_flds,
			     uint32_t *num_encap_flds)
{
	uint32_t idx;

	if (!tbl || !num_flds)
		return NULL;

	idx		= tbl->result_start_idx;
	*num_flds	= tbl->result_num_fields;
	*num_encap_flds = tbl->encap_num_fields;

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
ulp_mapper_act_result_fields_get(struct bnxt_ulp_mapper_tbl_info *tbl,
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
ulp_mapper_ident_fields_get(struct bnxt_ulp_mapper_tbl_info *tbl,
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

static struct bnxt_ulp_mapper_cache_entry *
ulp_mapper_cache_entry_get(struct bnxt_ulp_context *ulp,
			   uint32_t id,
			   uint16_t key)
{
	struct bnxt_ulp_mapper_data *mapper_data;

	mapper_data = bnxt_ulp_cntxt_ptr2_mapper_data_get(ulp);
	if (!mapper_data || id >= BNXT_ULP_CACHE_TBL_MAX_SZ ||
	    !mapper_data->cache_tbl[id]) {
		BNXT_TF_DBG(ERR, "Unable to acquire the cache tbl (%d)\n", id);
		return NULL;
	}

	return &mapper_data->cache_tbl[id][key];
}

/*
 * Concatenates the tbl_type and tbl_id into a 32bit value for storing in the
 * resource_type.  This is done to conserve memory since both the tbl_type and
 * tbl_id are 16bit.
 */
static inline void
ulp_mapper_cache_res_type_set(struct ulp_flow_db_res_params *res,
			      uint16_t tbl_type,
			      uint16_t tbl_id)
{
	res->resource_type = tbl_type;
	res->resource_sub_type = tbl_id;
}

/* Extracts the tbl_type and tbl_id from the 32bit resource type. */
static inline void
ulp_mapper_cache_res_type_get(struct ulp_flow_db_res_params *res,
			      uint16_t *tbl_type,
			      uint16_t *tbl_id)
{
	*tbl_type = res->resource_type;
	*tbl_id = res->resource_sub_type;
}

static int32_t
ulp_mapper_cache_entry_free(struct bnxt_ulp_context *ulp,
			    struct tf *tfp,
			    struct ulp_flow_db_res_params *res)
{
	struct bnxt_ulp_mapper_cache_entry *cache_entry;
	struct tf_free_identifier_parms ident_parms;
	struct tf_free_tcam_entry_parms tcam_parms;
	uint16_t table_id, table_type;
	int32_t rc, trc, i;

	/*
	 * The table id, used for cache, and table_type, used for tcam, are
	 * both encoded within the resource.  We must first extract them to
	 * formulate the args for tf calls.
	 */
	ulp_mapper_cache_res_type_get(res, &table_type, &table_id);
	cache_entry = ulp_mapper_cache_entry_get(ulp, table_id,
						 (uint16_t)res->resource_hndl);
	if (!cache_entry || !cache_entry->ref_count) {
		BNXT_TF_DBG(ERR, "Cache entry (%d:%d) not valid on free.\n",
			    table_id, (uint16_t)res->resource_hndl);
		return -EINVAL;
	}

	/*
	 * See if we need to delete the entry.  The tcam and identifiers are all
	 * tracked by the cached entries reference count.  All are deleted when
	 * the reference count hit zero.
	 */
	cache_entry->ref_count--;
	if (cache_entry->ref_count)
		return 0;

	/*
	 * Need to delete the tcam entry and the allocated identifiers.
	 * In the event of a failure, need to try to delete the remaining
	 * resources before returning error.
	 */
	tcam_parms.dir = res->direction;
	tcam_parms.tcam_tbl_type = table_type;
	tcam_parms.idx = cache_entry->tcam_idx;
	rc = tf_free_tcam_entry(tfp, &tcam_parms);
	if (rc)
		BNXT_TF_DBG(ERR, "Failed to free tcam [%d][%s][0x%04x] rc=%d\n",
			    table_type,
			    (res->direction == TF_DIR_RX) ? "RX" : "TX",
			    tcam_parms.idx, rc);

	/*
	 * Free the identifiers associated with the tcam entry.  Entries with
	 * negative one are considered uninitialized.
	 */
	for (i = 0; i < BNXT_ULP_CACHE_TBL_IDENT_MAX_NUM; i++) {
		if (cache_entry->idents[i] == ULP_IDENTS_INVALID)
			continue;

		ident_parms.dir = res->direction;
		ident_parms.ident_type = cache_entry->ident_types[i];
		ident_parms.id = cache_entry->idents[i];
		trc = tf_free_identifier(tfp, &ident_parms);
		if (trc) {
			BNXT_TF_DBG(ERR, "Failed to free identifier "
				    "[%d][%s][0x%04x] rc=%d\n",
				    ident_parms.ident_type,
				    (res->direction == TF_DIR_RX) ? "RX" : "TX",
				    ident_parms.id, trc);
			rc = trc;
		}
	}

	return rc;
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
	if (res->resource_func == BNXT_ULP_RESOURCE_FUNC_EXT_EM_TABLE)
		fparms.mem = TF_MEM_EXTERNAL;
	else
		fparms.mem = TF_MEM_INTERNAL;
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
	if (!val) {
		memset(&fid_parms, 0, sizeof(fid_parms));
		fid_parms.direction		= tbl->direction;
		fid_parms.resource_func	= ident->resource_func;
		fid_parms.resource_type	= ident->ident_type;
		fid_parms.resource_hndl	= iparms.id;
		fid_parms.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO;

		rc = ulp_flow_db_resource_add(parms->ulp_ctx,
					      parms->tbl_idx,
					      parms->fid,
					      &fid_parms);
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
		    (tbl->direction == TF_DIR_RX) ? "RX" : "TX");
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
		BNXT_TF_DBG(ERR, "Search ident %s:%x failed.\n",
			    tf_dir_2_str(sparms.dir),
			    sparms.search_id);
		return rc;
	}
	BNXT_TF_DBG(INFO, "Search ident %s:%x.success.\n",
		    tf_dir_2_str(sparms.dir),
		    sparms.search_id);

	/* Write it to the regfile */
	id = (uint64_t)tfp_cpu_to_be_64(sparms.search_id);
	if (!ulp_regfile_write(parms->regfile, ident->regfile_idx, id)) {
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
	fid_parms.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO;
	rc = ulp_flow_db_resource_add(parms->ulp_ctx,
				      parms->tbl_idx,
				      parms->fid,
				      &fid_parms);
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
ulp_mapper_result_field_process(struct bnxt_ulp_mapper_parms *parms,
				enum tf_dir dir,
				struct bnxt_ulp_mapper_result_field_info *fld,
				struct ulp_blob *blob,
				const char *name)
{
	uint16_t idx, size_idx;
	uint8_t	 *val = NULL;
	uint64_t regval;
	uint32_t val_size = 0, field_size = 0;
	uint64_t act_bit;
	uint8_t act_val;

	switch (fld->result_opcode) {
	case BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT:
		val = fld->result_operand;
		if (!ulp_blob_push(blob, val, fld->field_bit_size)) {
			BNXT_TF_DBG(ERR, "%s failed to add field\n", name);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP:
		if (!ulp_operand_read(fld->result_operand,
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
		if (fld->field_bit_size < ULP_BYTE_2_BITS(field_size)) {
			field_size  = field_size -
			    ((fld->field_bit_size + 7) / 8);
			val += field_size;
		}
		if (!ulp_blob_push(blob, val, fld->field_bit_size)) {
			BNXT_TF_DBG(ERR, "%s push field failed\n", name);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_MAPPER_OPC_SET_TO_ACT_BIT:
		if (!ulp_operand_read(fld->result_operand,
				      (uint8_t *)&act_bit, sizeof(uint64_t))) {
			BNXT_TF_DBG(ERR, "%s operand read failed\n", name);
			return -EINVAL;
		}
		act_bit = tfp_be_to_cpu_64(act_bit);
		if (ULP_BITMAP_ISSET(parms->act_bitmap->bits, act_bit))
			act_val = 1;
		else
			act_val = 0;
		if (fld->field_bit_size > ULP_BYTE_2_BITS(sizeof(act_val))) {
			BNXT_TF_DBG(ERR, "%s field size is incorrect\n", name);
			return -EINVAL;
		}
		if (!ulp_blob_push(blob, &act_val, fld->field_bit_size)) {
			BNXT_TF_DBG(ERR, "%s push field failed\n", name);
			return -EINVAL;
		}
		val = &act_val;
		break;
	case BNXT_ULP_MAPPER_OPC_SET_TO_ENCAP_ACT_PROP_SZ:
		if (!ulp_operand_read(fld->result_operand,
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
		if (!ulp_operand_read(&fld->result_operand[sizeof(uint16_t)],
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
	case BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE:
		if (!ulp_operand_read(fld->result_operand,
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

		val = ulp_blob_push_64(blob, &regval, fld->field_bit_size);
		if (!val) {
			BNXT_TF_DBG(ERR, "%s push field failed\n", name);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE:
		if (!ulp_operand_read(fld->result_operand,
				      (uint8_t *)&idx,
				      sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "%s key operand read failed.\n", name);
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		if (ulp_mapper_glb_resource_read(parms->mapper_data,
						 dir,
						 idx, &regval)) {
			BNXT_TF_DBG(ERR, "%s regfile[%d] read failed.\n",
				    name, idx);
			return -EINVAL;
		}
		val = ulp_blob_push_64(blob, &regval, fld->field_bit_size);
		if (!val) {
			BNXT_TF_DBG(ERR, "%s push to key blob failed\n", name);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD:
		if (!ulp_operand_read(fld->result_operand,
				      (uint8_t *)&idx,
				      sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "%s key operand read failed.\n", name);
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		if (idx < BNXT_ULP_CF_IDX_LAST)
			val = ulp_blob_push_32(blob, &parms->comp_fld[idx],
					       fld->field_bit_size);
		if (!val) {
			BNXT_TF_DBG(ERR, "%s push to key blob failed\n", name);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_MAPPER_OPC_SET_TO_ZERO:
		if (ulp_blob_pad_push(blob, fld->field_bit_size) < 0) {
			BNXT_TF_DBG(ERR, "%s too large for blob\n", name);
			return -EINVAL;
		}

		break;
	case BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_ACT_PROP_ELSE_CONST:
		if (!ulp_operand_read(fld->result_operand,
				      (uint8_t *)&act_bit, sizeof(uint64_t))) {
			BNXT_TF_DBG(ERR, "%s operand read failed\n", name);
			return -EINVAL;
		}
		act_bit = tfp_be_to_cpu_64(act_bit);
		if (ULP_BITMAP_ISSET(parms->act_bitmap->bits, act_bit)) {
			/* Action bit is set so consider operand_true */
			if (!ulp_operand_read(fld->result_operand_true,
					      (uint8_t *)&idx,
					      sizeof(uint16_t))) {
				BNXT_TF_DBG(ERR, "%s operand read failed\n",
					    name);
				return -EINVAL;
			}
			idx = tfp_be_to_cpu_16(idx);
			if (idx >= BNXT_ULP_ACT_PROP_IDX_LAST) {
				BNXT_TF_DBG(ERR, "%s act_prop[%d] oob\n",
					    name, idx);
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
				BNXT_TF_DBG(ERR, "%s push field failed\n",
					    name);
				return -EINVAL;
			}
		} else {
			/* action bit is not set, use the operand false */
			val = fld->result_operand_false;
			if (!ulp_blob_push(blob, val, fld->field_bit_size)) {
				BNXT_TF_DBG(ERR, "%s failed to add field\n",
					    name);
				return -EINVAL;
			}
		}
		break;
	case BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_CONST_ELSE_CONST:
		if (!ulp_operand_read(fld->result_operand,
				      (uint8_t *)&act_bit, sizeof(uint64_t))) {
			BNXT_TF_DBG(ERR, "%s operand read failed\n", name);
			return -EINVAL;
		}
		act_bit = tfp_be_to_cpu_64(act_bit);
		if (ULP_BITMAP_ISSET(parms->act_bitmap->bits, act_bit)) {
			/* Action bit is set so consider operand_true */
			val = fld->result_operand_true;
		} else {
			/* action bit is not set, use the operand false */
			val = fld->result_operand_false;
		}
		if (!ulp_blob_push(blob, val, fld->field_bit_size)) {
			BNXT_TF_DBG(ERR, "%s failed to add field\n",
				    name);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_MAPPER_OPC_IF_COMP_FIELD_THEN_CF_ELSE_CF:
		if (!ulp_operand_read(fld->result_operand,
				      (uint8_t *)&idx,
				      sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "%s key operand read failed.\n", name);
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		if (idx >= BNXT_ULP_CF_IDX_LAST) {
			BNXT_TF_DBG(ERR, "%s invalid index %u\n", name, idx);
			return -EINVAL;
		}
		/* check if the computed field is set */
		if (ULP_COMP_FLD_IDX_RD(parms, idx))
			val = fld->result_operand_true;
		else
			val = fld->result_operand_false;

		/* read the appropriate computed field */
		if (!ulp_operand_read(val, (uint8_t *)&idx, sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "%s val operand read failed\n", name);
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		if (idx >= BNXT_ULP_CF_IDX_LAST) {
			BNXT_TF_DBG(ERR, "%s invalid index %u\n", name, idx);
			return -EINVAL;
		}
		val = ulp_blob_push_32(blob, &parms->comp_fld[idx],
				       fld->field_bit_size);
		if (!val) {
			BNXT_TF_DBG(ERR, "%s push to key blob failed\n", name);
			return -EINVAL;
		}
		break;
	default:
		BNXT_TF_DBG(ERR, "invalid result mapper opcode 0x%x\n",
			    fld->result_opcode);
		return -EINVAL;
	}
	return 0;
}

/* Function to alloc action record and set the table. */
static int32_t
ulp_mapper_keymask_field_process(struct bnxt_ulp_mapper_parms *parms,
				 enum tf_dir dir,
				 struct bnxt_ulp_mapper_class_key_field_info *f,
				 struct ulp_blob *blob,
				 uint8_t is_key,
				 const char *name)
{
	uint64_t val64;
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
	case BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT:
		val = operand;
		if (!ulp_blob_push(blob, val, bitlen)) {
			BNXT_TF_DBG(ERR, "%s push to key blob failed\n", name);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_MAPPER_OPC_SET_TO_ZERO:
		if (ulp_blob_pad_push(blob, bitlen) < 0) {
			BNXT_TF_DBG(ERR, "%s pad too large for blob\n", name);
			return -EINVAL;
		}

		break;
	case BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD:
		if (!ulp_operand_read(operand, (uint8_t *)&idx,
				      sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "%s key operand read failed.\n", name);
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
			BNXT_TF_DBG(ERR, "%s push to key blob failed\n", name);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD:
		if (!ulp_operand_read(operand, (uint8_t *)&idx,
				      sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "%s key operand read failed.\n", name);
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		if (idx < BNXT_ULP_CF_IDX_LAST)
			val = ulp_blob_push_32(blob, &parms->comp_fld[idx],
					       bitlen);
		if (!val) {
			BNXT_TF_DBG(ERR, "%s push to key blob failed\n", name);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE:
		if (!ulp_operand_read(operand, (uint8_t *)&idx,
				      sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "%s key operand read failed.\n", name);
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);

		if (!ulp_regfile_read(regfile, idx, &val64)) {
			BNXT_TF_DBG(ERR, "%s regfile[%d] read failed.\n",
				    name, idx);
			return -EINVAL;
		}

		val = ulp_blob_push_64(blob, &val64, bitlen);
		if (!val) {
			BNXT_TF_DBG(ERR, "%s push to key blob failed\n", name);
			return -EINVAL;
		}
		break;
	case BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE:
		if (!ulp_operand_read(operand, (uint8_t *)&idx,
				      sizeof(uint16_t))) {
			BNXT_TF_DBG(ERR, "%s key operand read failed.\n", name);
			return -EINVAL;
		}
		idx = tfp_be_to_cpu_16(idx);
		if (ulp_mapper_glb_resource_read(parms->mapper_data,
						 dir,
						 idx, &val64)) {
			BNXT_TF_DBG(ERR, "%s regfile[%d] read failed.\n",
				    name, idx);
			return -EINVAL;
		}
		val = ulp_blob_push_64(blob, &val64, bitlen);
		if (!val) {
			BNXT_TF_DBG(ERR, "%s push to key blob failed\n", name);
			return -EINVAL;
		}
		break;
	default:
		BNXT_TF_DBG(ERR, "invalid keymask mapper opcode 0x%x\n",
			    opcode);
		return -EINVAL;
		break;
	}

	return 0;
}

static int32_t
ulp_mapper_mark_gfid_process(struct bnxt_ulp_mapper_parms *parms,
			     struct bnxt_ulp_mapper_tbl_info *tbl,
			     uint64_t flow_id)
{
	enum bnxt_ulp_mark_db_opcode mark_op = tbl->mark_db_opcode;
	struct ulp_flow_db_res_params fid_parms;
	uint32_t mark, gfid, mark_flag;
	int32_t rc = 0;

	if (mark_op == BNXT_ULP_MARK_DB_OPCODE_NOP ||
	    !(mark_op == BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION &&
	      ULP_BITMAP_ISSET(parms->act_bitmap->bits,
			       BNXT_ULP_ACTION_BIT_MARK)))
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
	fid_parms.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO;
	fid_parms.resource_type	= mark_flag;
	fid_parms.resource_hndl	= gfid;
	rc = ulp_flow_db_resource_add(parms->ulp_ctx,
				      parms->tbl_idx,
				      parms->fid,
				      &fid_parms);
	if (rc)
		BNXT_TF_DBG(ERR, "Fail to link res to flow rc = %d\n", rc);
	return rc;
}

static int32_t
ulp_mapper_mark_act_ptr_process(struct bnxt_ulp_mapper_parms *parms,
				struct bnxt_ulp_mapper_tbl_info *tbl)
{
	enum bnxt_ulp_mark_db_opcode mark_op = tbl->mark_db_opcode;
	struct ulp_flow_db_res_params fid_parms;
	uint32_t act_idx, mark, mark_flag;
	uint64_t val64;
	int32_t rc = 0;

	if (mark_op == BNXT_ULP_MARK_DB_OPCODE_NOP ||
	    !(mark_op == BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION &&
	      ULP_BITMAP_ISSET(parms->act_bitmap->bits,
			       BNXT_ULP_ACTION_BIT_MARK)))
		return rc; /* no need to perform mark action process */

	/* Get the mark id details from action property */
	memcpy(&mark, &parms->act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_MARK],
	       sizeof(mark));
	mark = tfp_be_to_cpu_32(mark);

	if (!ulp_regfile_read(parms->regfile,
			      BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR,
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
	fid_parms.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO;
	fid_parms.resource_type	= mark_flag;
	fid_parms.resource_hndl	= act_idx;
	rc = ulp_flow_db_resource_add(parms->ulp_ctx,
				      parms->tbl_idx,
				      parms->fid,
				      &fid_parms);
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
	enum bnxt_ulp_mark_db_opcode mark_op = tbl->mark_db_opcode;
	int32_t rc = 0;

	if (mark_op == BNXT_ULP_MARK_DB_OPCODE_NOP ||
	    mark_op == BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION)
		return rc; /* no need to perform mark action process */

	/* Get the mark id details from the computed field of dev port id */
	mark = ULP_COMP_FLD_IDX_RD(parms, BNXT_ULP_CF_IDX_DEV_PORT_ID);

	 /* Get the main action pointer */
	if (!ulp_regfile_read(parms->regfile,
			      BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR,
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
	fid_parms.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO;
	fid_parms.resource_type	= mark_flag;
	fid_parms.resource_hndl	= act_idx;
	rc = ulp_flow_db_resource_add(parms->ulp_ctx,
				      parms->tbl_idx,
				      parms->fid,
				      &fid_parms);
	if (rc)
		BNXT_TF_DBG(ERR, "Fail to link res to flow rc = %d\n", rc);
	return rc;
}

/*
 * Tcam table - create the result blob.
 * data [out] - the result blob data
 */
static int32_t
ulp_mapper_tcam_tbl_result_create(struct bnxt_ulp_mapper_parms *parms,
				  struct bnxt_ulp_mapper_tbl_info *tbl,
				  struct ulp_blob *data)
{
	struct bnxt_ulp_mapper_result_field_info *dflds;
	uint32_t num_dflds;
	uint32_t encap_flds = 0;
	uint32_t i;
	int32_t rc = 0;

	/* Create the result data blob */
	dflds = ulp_mapper_result_fields_get(tbl, &num_dflds,
					     &encap_flds);
	if (!dflds || !num_dflds || encap_flds) {
		BNXT_TF_DBG(ERR, "Failed to get data fields.\n");
		return -EINVAL;
	}

	for (i = 0; i < num_dflds; i++) {
		rc = ulp_mapper_result_field_process(parms,
						     tbl->direction,
						     &dflds[i],
						     data,
						     "TCAM Result");
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed to set data fields\n");
			return -EINVAL;
		}
	}
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

	/*
	 * Since the cache entry is responsible for allocating
	 * identifiers when in use, allocate the identifiers only
	 * during normal processing.
	 */
	if (parms->tcam_tbl_opc ==
	    BNXT_ULP_MAPPER_TCAM_TBL_OPC_NORMAL) {
		idents = ulp_mapper_ident_fields_get(tbl, &num_idents);

		for (i = 0; i < num_idents; i++) {
			if (ulp_mapper_ident_process(parms, tbl,
						     &idents[i], NULL))
				return -EINVAL;
		}
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
	idents = ulp_mapper_ident_fields_get(tbl, &num_idents);
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
	BNXT_TF_DBG(INFO, "tcam[%s][%s][%x] write success.\n",
		    tf_tcam_tbl_2_str(sparms.tcam_tbl_type),
		    tf_dir_2_str(sparms.dir), sparms.idx);

	/* Update cache with TCAM index if the was cache allocated. */
	if (parms->tcam_tbl_opc ==
	    BNXT_ULP_MAPPER_TCAM_TBL_OPC_CACHE_ALLOC) {
		if (!parms->cache_ptr) {
			BNXT_TF_DBG(ERR, "Unable to update cache");
			return -EINVAL;
		}
		parms->cache_ptr->tcam_idx = idx;
	}

	/* Mark action */
	rc = ulp_mapper_mark_act_ptr_process(parms, tbl);
	if (rc) {
		BNXT_TF_DBG(ERR, "failed mark action processing\n");
		return rc;
	}

	return rc;
}

static int32_t
ulp_mapper_tcam_tbl_process(struct bnxt_ulp_mapper_parms *parms,
			    struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct bnxt_ulp_mapper_class_key_field_info	*kflds;
	struct ulp_blob key, mask, data, update_data;
	uint32_t i, num_kflds;
	struct tf *tfp;
	int32_t rc, trc;
	struct tf_alloc_tcam_entry_parms aparms		= { 0 };
	struct tf_search_tcam_entry_parms searchparms   = { 0 };
	struct ulp_flow_db_res_params	fid_parms	= { 0 };
	struct tf_free_tcam_entry_parms free_parms	= { 0 };
	enum bnxt_ulp_search_before_alloc search_flag;
	uint32_t hit = 0;
	uint16_t tmplen = 0;
	uint16_t idx;

	/* Skip this if was handled by the cache. */
	if (parms->tcam_tbl_opc == BNXT_ULP_MAPPER_TCAM_TBL_OPC_CACHE_SKIP) {
		parms->tcam_tbl_opc = BNXT_ULP_MAPPER_TCAM_TBL_OPC_NORMAL;
		return 0;
	}

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

	if (!ulp_blob_init(&key, tbl->key_bit_size,
			   parms->device_params->byte_order) ||
	    !ulp_blob_init(&mask, tbl->key_bit_size,
			   parms->device_params->byte_order) ||
	    !ulp_blob_init(&data, tbl->result_bit_size,
			   parms->device_params->byte_order) ||
	    !ulp_blob_init(&update_data, tbl->result_bit_size,
			   parms->device_params->byte_order)) {
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
		rc = ulp_mapper_keymask_field_process(parms, tbl->direction,
						      &kflds[i],
						      &key, 1, "TCAM Key");
		if (rc) {
			BNXT_TF_DBG(ERR, "Key field set failed.\n");
			return rc;
		}

		/* Setup the mask */
		rc = ulp_mapper_keymask_field_process(parms, tbl->direction,
						      &kflds[i],
						      &mask, 0, "TCAM Mask");
		if (rc) {
			BNXT_TF_DBG(ERR, "Mask field set failed.\n");
			return rc;
		}
	}

	if (tbl->srch_b4_alloc == BNXT_ULP_SEARCH_BEFORE_ALLOC_NO) {
		/*
		 * No search for re-use is requested, so simply allocate the
		 * tcam index.
		 */
		aparms.dir		= tbl->direction;
		aparms.tcam_tbl_type	= tbl->resource_type;
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
		 * All failures after this succeeds require the entry to be
		 * freed. cannot return directly on failure, but needs to goto
		 * error.
		 */
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
		searchparms.priority = tbl->priority;
		searchparms.alloc = 1;
		searchparms.result = ulp_blob_data_get(&data, &tmplen);
		searchparms.result_sz_in_bits = tbl->result_bit_size;

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

	/* if it is miss then it is same as no search before alloc */
	if (!hit)
		search_flag = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO;
	else
		search_flag = tbl->srch_b4_alloc;

	switch (search_flag) {
	case BNXT_ULP_SEARCH_BEFORE_ALLOC_NO:
		/*Scan identifier list, allocate identifier and update regfile*/
		rc = ulp_mapper_tcam_tbl_scan_ident_alloc(parms, tbl);
		/* Create the result blob */
		if (!rc)
			rc = ulp_mapper_tcam_tbl_result_create(parms, tbl,
							       &data);
		/* write the tcam entry */
		if (!rc)
			rc = ulp_mapper_tcam_tbl_entry_write(parms, tbl, &key,
							     &mask, &data, idx);
		break;
	case BNXT_ULP_SEARCH_BEFORE_ALLOC_SEARCH_IF_HIT_SKIP:
		/*Scan identifier list, extract identifier and update regfile*/
		rc = ulp_mapper_tcam_tbl_scan_ident_extract(parms, tbl, &data);
		break;
	case BNXT_ULP_SEARCH_BEFORE_ALLOC_SEARCH_IF_HIT_UPDATE:
		/*Scan identifier list, extract identifier and update regfile*/
		rc = ulp_mapper_tcam_tbl_scan_ident_extract(parms, tbl, &data);
		/* Create the result blob */
		if (!rc)
			rc = ulp_mapper_tcam_tbl_result_create(parms, tbl,
							       &update_data);
		/* Update/overwrite the tcam entry */
		if (!rc)
			rc = ulp_mapper_tcam_tbl_entry_write(parms, tbl, &key,
							     &mask,
							     &update_data, idx);
		break;
	default:
		BNXT_TF_DBG(ERR, "invalid search opcode\n");
		rc =  -EINVAL;
		break;
	}
	if (rc)
		goto error;

	/*
	 * Only link the entry to the flow db in the event that cache was not
	 * used.
	 */
	if (parms->tcam_tbl_opc == BNXT_ULP_MAPPER_TCAM_TBL_OPC_NORMAL) {
		fid_parms.direction = tbl->direction;
		fid_parms.resource_func	= tbl->resource_func;
		fid_parms.resource_type	= tbl->resource_type;
		fid_parms.critical_resource = tbl->critical_resource;
		fid_parms.resource_hndl	= idx;
		rc = ulp_flow_db_resource_add(parms->ulp_ctx,
					      parms->tbl_idx,
					      parms->fid,
					      &fid_parms);
		if (rc) {
			BNXT_TF_DBG(ERR,
				    "Failed to link resource to flow rc = %d\n",
				    rc);
			/* Need to free the identifier, so goto error */
			goto error;
		}
	} else {
		/*
		 * Reset the tcam table opcode to normal in case the next tcam
		 * entry does not use cache.
		 */
		parms->tcam_tbl_opc = BNXT_ULP_MAPPER_TCAM_TBL_OPC_NORMAL;
		parms->cache_ptr = NULL;
	}

	return 0;
error:
	parms->tcam_tbl_opc = BNXT_ULP_MAPPER_TCAM_TBL_OPC_NORMAL;
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
	struct bnxt_ulp_mapper_class_key_field_info	*kflds;
	struct bnxt_ulp_mapper_result_field_info *dflds;
	struct ulp_blob key, data;
	uint32_t i, num_kflds, num_dflds;
	uint16_t tmplen;
	struct tf *tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx);
	struct ulp_flow_db_res_params	fid_parms = { 0 };
	struct tf_insert_em_entry_parms iparms = { 0 };
	struct tf_delete_em_entry_parms free_parms = { 0 };
	int32_t	trc;
	enum bnxt_ulp_flow_mem_type mtype = parms->device_params->flow_mem_type;
	int32_t rc = 0;
	uint32_t encap_flds = 0;

	kflds = ulp_mapper_key_fields_get(tbl, &num_kflds);
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
		rc = ulp_mapper_keymask_field_process(parms, tbl->direction,
						      &kflds[i],
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
	dflds = ulp_mapper_result_fields_get(tbl, &num_dflds, &encap_flds);
	if (!dflds || !num_dflds || encap_flds) {
		BNXT_TF_DBG(ERR, "Failed to get data fields.\n");
		return -EINVAL;
	}

	for (i = 0; i < num_dflds; i++) {
		struct bnxt_ulp_mapper_result_field_info *fld;

		fld = &dflds[i];

		rc = ulp_mapper_result_field_process(parms,
						     tbl->direction,
						     fld,
						     &data,
						     "EM Result");
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed to set data fields.\n");
			return rc;
		}
	}

	/* do the transpose for the internal EM keys */
	if (tbl->resource_func == BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE)
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
			     struct bnxt_ulp_mapper_tbl_info *tbl,
			     bool is_class_tbl)
{
	struct bnxt_ulp_mapper_result_field_info *flds;
	struct ulp_flow_db_res_params	fid_parms;
	struct ulp_blob	data;
	uint64_t idx = 0;
	uint16_t tmplen;
	uint32_t i, num_flds, index, hit;
	int32_t rc = 0, trc = 0;
	struct tf_alloc_tbl_entry_parms	aparms = { 0 };
	struct tf_search_tbl_entry_parms srchparms = { 0 };
	struct tf_set_tbl_entry_parms	sparms = { 0 };
	struct tf_free_tbl_entry_parms	free_parms = { 0 };
	uint32_t tbl_scope_id;
	struct tf *tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx);
	uint16_t bit_size;
	uint32_t encap_flds = 0;

	/* Get the scope id first */
	rc = bnxt_ulp_cntxt_tbl_scope_id_get(parms->ulp_ctx, &tbl_scope_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to get table scope rc=%d\n", rc);
		return rc;
	}

	/* use the max size if encap is enabled */
	if (tbl->encap_num_fields)
		bit_size = BNXT_ULP_FLMP_BLOB_SIZE_IN_BITS;
	else
		bit_size = tbl->result_bit_size;

	/* Initialize the blob data */
	if (!ulp_blob_init(&data, bit_size,
			   parms->device_params->byte_order)) {
		BNXT_TF_DBG(ERR, "Failed initial index table blob\n");
		return -EINVAL;
	}

	/* Get the result fields list */
	if (is_class_tbl)
		flds = ulp_mapper_result_fields_get(tbl, &num_flds,
						    &encap_flds);
	else
		flds = ulp_mapper_act_result_fields_get(tbl, &num_flds,
							&encap_flds);

	if (!flds || (!num_flds && !encap_flds)) {
		BNXT_TF_DBG(ERR, "template undefined for the index table\n");
		return -EINVAL;
	}

	/* process the result fields, loop through them */
	for (i = 0; i < (num_flds + encap_flds); i++) {
		/* set the swap index if encap swap bit is enabled */
		if (parms->device_params->encap_byte_swap && encap_flds &&
		    (i == num_flds))
			ulp_blob_encap_swap_idx_set(&data);

		/* Process the result fields */
		rc = ulp_mapper_result_field_process(parms,
						     tbl->direction,
						     &flds[i],
						     &data,
						     "Indexed Result");
		if (rc) {
			BNXT_TF_DBG(ERR, "data field failed\n");
			return rc;
		}
	}

	/* if encap bit swap is enabled perform the bit swap */
	if (parms->device_params->encap_byte_swap && encap_flds) {
		ulp_blob_perform_encap_swap(&data);
	}

	/*
	 * Check for index opcode, if it is Global then
	 * no need to allocate the table, just set the table
	 * and exit since it is not maintained in the flow db.
	 */
	if (tbl->index_opcode == BNXT_ULP_INDEX_OPCODE_GLOBAL) {
		/* get the index from index operand */
		if (tbl->index_operand < BNXT_ULP_GLB_REGFILE_INDEX_LAST &&
		    ulp_mapper_glb_resource_read(parms->mapper_data,
						 tbl->direction,
						 tbl->index_operand,
						 &idx)) {
			BNXT_TF_DBG(ERR, "Glbl regfile[%d] read failed.\n",
				    tbl->index_operand);
			return -EINVAL;
		}
		/* set the Tf index table */
		sparms.dir		= tbl->direction;
		sparms.type		= tbl->resource_type;
		sparms.data		= ulp_blob_data_get(&data, &tmplen);
		sparms.data_sz_in_bytes = ULP_BITS_2_BYTE(tmplen);
		sparms.idx		= tfp_be_to_cpu_64(idx);
		sparms.tbl_scope_id	= tbl_scope_id;

		rc = tf_set_tbl_entry(tfp, &sparms);
		if (rc) {
			BNXT_TF_DBG(ERR,
				    "Glbl Set table[%d][%s][%d] failed rc=%d\n",
				    sparms.type,
				    (sparms.dir == TF_DIR_RX) ? "RX" : "TX",
				    sparms.idx,
				    rc);
			return rc;
		}
		return 0; /* success */
	}

	index = 0;
	hit = 0;
	/* Perform the tf table allocation by filling the alloc params */
	if (tbl->srch_b4_alloc) {
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
	} else {
		aparms.dir		= tbl->direction;
		aparms.type		= tbl->resource_type;
		aparms.search_enable	= tbl->srch_b4_alloc;
		aparms.result		= ulp_blob_data_get(&data, &tmplen);
		aparms.result_sz_in_bytes = ULP_BITS_2_BYTE(tmplen);
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
	/*
	 * calculate the idx for the result record, for external EM the offset
	 * needs to be shifted accordingly. If external non-inline table types
	 * are used then need to revisit this logic.
	 */
	if (tbl->resource_type == TF_TBL_TYPE_EXT)
		idx = TF_ACT_REC_OFFSET_2_PTR(index);
	else
		idx = index;

	/* Always storing values in Regfile in BE */
	idx = tfp_cpu_to_be_64(idx);
	if (tbl->index_opcode == BNXT_ULP_INDEX_OPCODE_ALLOCATE) {
		rc = ulp_regfile_write(parms->regfile, tbl->index_operand, idx);
		if (!rc) {
			BNXT_TF_DBG(ERR, "Write regfile[%d] failed\n",
				    tbl->index_operand);
			goto error;
		}
	}

	/* Perform the tf table set by filling the set params */
	if (!tbl->srch_b4_alloc || !hit) {
		sparms.dir		= tbl->direction;
		sparms.type		= tbl->resource_type;
		sparms.data		= ulp_blob_data_get(&data, &tmplen);
		sparms.data_sz_in_bytes = ULP_BITS_2_BYTE(tmplen);
		sparms.idx		= index;
		sparms.tbl_scope_id	= tbl_scope_id;

		rc = tf_set_tbl_entry(tfp, &sparms);
		if (rc) {
			BNXT_TF_DBG(ERR, "Set table[%d][%s][%d] failed rc=%d\n",
				    sparms.type,
				    (sparms.dir == TF_DIR_RX) ? "RX" : "TX",
				    sparms.idx,
				    rc);
			goto error;
		}
	}

	/* Link the resource to the flow in the flow db */
	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.direction	= tbl->direction;
	fid_parms.resource_func	= tbl->resource_func;
	fid_parms.resource_type	= tbl->resource_type;
	fid_parms.resource_sub_type = tbl->resource_sub_type;
	fid_parms.resource_hndl	= index;
	fid_parms.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO;

	rc = ulp_flow_db_resource_add(parms->ulp_ctx,
				      parms->tbl_idx,
				      parms->fid,
				      &fid_parms);
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
ulp_mapper_cache_tbl_process(struct bnxt_ulp_mapper_parms *parms,
			     struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct bnxt_ulp_mapper_class_key_field_info *kflds;
	struct bnxt_ulp_mapper_cache_entry *cache_entry;
	struct bnxt_ulp_mapper_ident_info *idents;
	uint32_t i, num_kflds = 0, num_idents = 0;
	struct ulp_flow_db_res_params fid_parms;
	struct tf_free_identifier_parms fparms;
	uint16_t tmplen, tmp_ident;
	struct ulp_blob key;
	uint8_t *cache_key;
	uint64_t regval;
	uint16_t *ckey;
	int32_t rc;

	/* Get the key fields list and build the key. */
	kflds = ulp_mapper_key_fields_get(tbl, &num_kflds);
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
		rc = ulp_mapper_keymask_field_process(parms, tbl->direction,
						      &kflds[i],
						      &key, 1, "Cache Key");
		if (rc) {
			BNXT_TF_DBG(ERR,
				    "Failed to create key for Cache rc=%d\n",
				    rc);
			return -EINVAL;
		}
	}

	/*
	 * Perform the lookup in the cache table with constructed key.  The
	 * cache_key is a byte array of tmplen, it needs to be converted to a
	 * index for the cache table.
	 */
	cache_key = ulp_blob_data_get(&key, &tmplen);
	ckey = (uint16_t *)cache_key;

	/*
	 * The id computed based on resource sub type and direction where
	 * dir is the bit0 and rest of the bits come from resource
	 * sub type.
	 */
	cache_entry = ulp_mapper_cache_entry_get(parms->ulp_ctx,
						 (tbl->resource_sub_type << 1 |
						 (tbl->direction & 0x1)),
						 *ckey);

	/*
	 * Get the identifier list for processing by both the hit and miss
	 * processing.
	 */
	idents = ulp_mapper_ident_fields_get(tbl, &num_idents);

	if (!cache_entry->ref_count) {
		/* Initialize the cache entry */
		cache_entry->tcam_idx = 0;
		cache_entry->ref_count = 0;
		for (i = 0; i < BNXT_ULP_CACHE_TBL_IDENT_MAX_NUM; i++)
			cache_entry->idents[i] = ULP_IDENTS_INVALID;

		/* Need to allocate identifiers for storing in the cache. */
		for (i = 0; i < num_idents; i++) {
			/*
			 * Since we are using the cache, the identifier does not
			 * get added to the flow db.  Pass in the pointer to the
			 * tmp_ident.
			 */
			rc = ulp_mapper_ident_process(parms, tbl,
						      &idents[i], &tmp_ident);
			if (rc)
				goto error;

			cache_entry->ident_types[i] = idents[i].ident_type;
			cache_entry->idents[i] = tmp_ident;
		}

		/* Tell the TCAM processor to alloc an entry */
		parms->tcam_tbl_opc = BNXT_ULP_MAPPER_TCAM_TBL_OPC_CACHE_ALLOC;
		/* Store the cache key for use by the tcam process code */
		parms->cache_ptr = cache_entry;
	} else {
		/* Cache hit, get values from result. */
		for (i = 0; i < num_idents; i++) {
			regval = (uint64_t)cache_entry->idents[i];
			if (!ulp_regfile_write(parms->regfile,
					       idents[i].regfile_idx,
					       tfp_cpu_to_be_64(regval))) {
				BNXT_TF_DBG(ERR,
					    "Failed to write to regfile\n");
				return -EINVAL;
			}
		}
		/*
		 * The cached entry is being used, so let the tcam processing
		 * know not to process this table.
		 */
		parms->tcam_tbl_opc = BNXT_ULP_MAPPER_TCAM_TBL_OPC_CACHE_SKIP;
	}

	/* Made through the cache processing, increment the reference count. */
	cache_entry->ref_count++;

	/* Link the cache to the flow db. */
	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.direction = tbl->direction;
	fid_parms.resource_func	= tbl->resource_func;

	/*
	 * Cache resource type is composed of table_type, resource
	 * sub type and direction, it needs to set appropriately via setter.
	 */
	ulp_mapper_cache_res_type_set(&fid_parms,
				      tbl->resource_type,
				      (tbl->resource_sub_type << 1 |
				       (tbl->direction & 0x1)));
	fid_parms.resource_hndl	= (uint64_t)*ckey;
	fid_parms.critical_resource = tbl->critical_resource;
	rc = ulp_flow_db_resource_add(parms->ulp_ctx,
				      parms->tbl_idx,
				      parms->fid,
				      &fid_parms);
	if (rc)
		BNXT_TF_DBG(ERR, "Failed to add cache to flow db.\n");

	return rc;
error:
	/*
	 * This error handling only gets called when the idents are being
	 * allocated for the cache on misses.  Using the num_idents that was
	 * previously set.
	 */
	for (i = 0; i < num_idents; i++) {
		if (cache_entry->idents[i] == ULP_IDENTS_INVALID)
			continue;

		fparms.dir = tbl->direction;
		fparms.ident_type = idents[i].ident_type;
		fparms.id = cache_entry->idents[i];
		tf_free_identifier(parms->tfp, &fparms);
	}

	return rc;
}

static int32_t
ulp_mapper_if_tbl_process(struct bnxt_ulp_mapper_parms *parms,
			  struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct bnxt_ulp_mapper_result_field_info *flds;
	struct ulp_blob	data;
	uint64_t idx;
	uint16_t tmplen;
	uint32_t i, num_flds;
	int32_t rc = 0;
	struct tf_set_if_tbl_entry_parms iftbl_params = { 0 };
	struct tf *tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx);
	uint32_t encap_flds;

	/* Initialize the blob data */
	if (!ulp_blob_init(&data, tbl->result_bit_size,
			   parms->device_params->byte_order)) {
		BNXT_TF_DBG(ERR, "Failed initial index table blob\n");
		return -EINVAL;
	}

	/* Get the result fields list */
	flds = ulp_mapper_result_fields_get(tbl, &num_flds, &encap_flds);

	if (!flds || !num_flds || encap_flds) {
		BNXT_TF_DBG(ERR, "template undefined for the IF table\n");
		return -EINVAL;
	}

	/* process the result fields, loop through them */
	for (i = 0; i < num_flds; i++) {
		/* Process the result fields */
		rc = ulp_mapper_result_field_process(parms,
						     tbl->direction,
						     &flds[i],
						     &data,
						     "IFtable Result");
		if (rc) {
			BNXT_TF_DBG(ERR, "data field failed\n");
			return rc;
		}
	}

	/* Get the index details from computed field */
	if (tbl->index_opcode == BNXT_ULP_INDEX_OPCODE_COMP_FIELD) {
		idx = ULP_COMP_FLD_IDX_RD(parms, tbl->index_operand);
	} else if (tbl->index_opcode == BNXT_ULP_INDEX_OPCODE_CONSTANT) {
		idx = tbl->index_operand;
	} else {
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
		BNXT_TF_DBG(ERR, "Set table[%d][%s][%d] failed rc=%d\n",
			    iftbl_params.type,
			    (iftbl_params.dir == TF_DIR_RX) ? "RX" : "TX",
			    iftbl_params.idx,
			    rc);
		return rc;
	}

	/*
	 * TBD: Need to look at the need to store idx in flow db for restore
	 * the table to its original state on deletion of this entry.
	 */
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
 * Function to process the conditional opcode of the mapper table.
 * returns 1 to skip the table.
 * return 0 to continue processing the table.
 */
static int32_t
ulp_mapper_tbl_cond_opcode_process(struct bnxt_ulp_mapper_parms *parms,
				   struct bnxt_ulp_mapper_tbl_info *tbl)
{
	int32_t rc = 1;

	switch (tbl->cond_opcode) {
	case BNXT_ULP_COND_OPCODE_NOP:
		rc = 0;
		break;
	case BNXT_ULP_COND_OPCODE_COMP_FIELD_IS_SET:
		if (tbl->cond_operand < BNXT_ULP_CF_IDX_LAST &&
		    ULP_COMP_FLD_IDX_RD(parms, tbl->cond_operand))
			rc = 0;
		break;
	case BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET:
		if (ULP_BITMAP_ISSET(parms->act_bitmap->bits,
				     tbl->cond_operand))
			rc = 0;
		break;
	case BNXT_ULP_COND_OPCODE_HDR_BIT_IS_SET:
		if (ULP_BITMAP_ISSET(parms->hdr_bitmap->bits,
				     tbl->cond_operand))
			rc = 0;
		break;
	case BNXT_ULP_COND_OPCODE_COMP_FIELD_NOT_SET:
		if (tbl->cond_operand < BNXT_ULP_CF_IDX_LAST &&
		    !ULP_COMP_FLD_IDX_RD(parms, tbl->cond_operand))
			rc = 0;
		break;
	case BNXT_ULP_COND_OPCODE_ACTION_BIT_NOT_SET:
		if (!ULP_BITMAP_ISSET(parms->act_bitmap->bits,
				      tbl->cond_operand))
			rc = 0;
		break;
	case BNXT_ULP_COND_OPCODE_HDR_BIT_NOT_SET:
		if (!ULP_BITMAP_ISSET(parms->hdr_bitmap->bits,
				      tbl->cond_operand))
			rc = 0;
		break;
	default:
		BNXT_TF_DBG(ERR,
			    "Invalid arg in mapper tbl for cond opcode\n");
		break;
	}
	return rc;
}

/*
 * Function to process the action template. Iterate through the list
 * action info templates and process it.
 */
static int32_t
ulp_mapper_action_tbls_process(struct bnxt_ulp_mapper_parms *parms)
{
	uint32_t	i;
	int32_t		rc = 0;
	struct bnxt_ulp_mapper_tbl_info *tbl;

	if (!parms->atbls || !parms->num_atbls) {
		BNXT_TF_DBG(ERR, "No action tables for template[%d][%d].\n",
			    parms->dev_id, parms->act_tid);
		return -EINVAL;
	}

	for (i = 0; i < parms->num_atbls; i++) {
		tbl = &parms->atbls[i];
		if (ulp_mapper_tbl_cond_opcode_process(parms, tbl))
			continue;

		switch (tbl->resource_func) {
		case BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE:
			rc = ulp_mapper_index_tbl_process(parms, tbl, false);
			if (rc) {
				BNXT_TF_DBG(ERR, "Resource type %d failed\n",
					    tbl->resource_func);
				return rc;
			}
			break;
		default:
			BNXT_TF_DBG(ERR, "Unexpected action resource %d\n",
				    tbl->resource_func);
			return -EINVAL;
		}
	}

	return rc;
}

/* Create the classifier table entries for a flow. */
static int32_t
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
		struct bnxt_ulp_mapper_tbl_info *tbl = &parms->ctbls[i];

		if (ulp_mapper_tbl_cond_opcode_process(parms, tbl))
			continue;

		switch (tbl->resource_func) {
		case BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE:
			rc = ulp_mapper_tcam_tbl_process(parms, tbl);
			break;
		case BNXT_ULP_RESOURCE_FUNC_EXT_EM_TABLE:
		case BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE:
			rc = ulp_mapper_em_tbl_process(parms, tbl);
			break;
		case BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE:
			rc = ulp_mapper_index_tbl_process(parms, tbl, true);
			break;
		case BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE:
			rc = ulp_mapper_cache_tbl_process(parms, tbl);
			break;
		case BNXT_ULP_RESOURCE_FUNC_IF_TABLE:
			rc = ulp_mapper_if_tbl_process(parms, tbl);
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
	case BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE:
		rc = ulp_mapper_cache_entry_free(ulp, tfp, res);
		break;
	case BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE:
		rc = ulp_mapper_tcam_entry_free(ulp, tfp, res);
		break;
	case BNXT_ULP_RESOURCE_FUNC_EXT_EM_TABLE:
	case BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE:
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
	res_parms.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES;
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

		/* All subsequent call require the non-critical_resource */
		res_parms.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO;

		rc = ulp_flow_db_resource_del(ulp_ctx,
					      tbl_type,
					      fid,
					      &res_parms);
	}

	/* Free the Flow ID since we've removed all resources */
	rc = ulp_flow_db_fid_free(ulp_ctx, tbl_type, fid);

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
			ulp_mapper_resource_free(ulp_ctx, &res);
		}
	}
}

int32_t
ulp_mapper_flow_destroy(struct bnxt_ulp_context	*ulp_ctx, uint32_t fid,
			enum bnxt_ulp_flow_db_tables flow_tbl_type)
{
	if (!ulp_ctx) {
		BNXT_TF_DBG(ERR, "Invalid parms, unable to free flow\n");
		return -EINVAL;
	}

	return ulp_mapper_resources_free(ulp_ctx, fid, flow_tbl_type);
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

		/* Get the class table entry from dev id and class id */
		parms.class_tid = glbl_tmpl_list[idx];
		parms.ctbls = ulp_mapper_class_tbl_list_get(parms.dev_id,
							    parms.class_tid,
							    &parms.num_ctbls,
							    &parms.tbl_idx);
		if (!parms.ctbls || !parms.num_ctbls) {
			BNXT_TF_DBG(ERR, "No class tables for %d:%d\n",
				    parms.dev_id, parms.class_tid);
			return -EINVAL;
		}
		parms.device_params = bnxt_ulp_device_params_get(parms.dev_id);
		if (!parms.device_params) {
			BNXT_TF_DBG(ERR, "No class tables for %d:%d\n",
				    parms.dev_id, parms.class_tid);
			return -EINVAL;
		}
		rc = ulp_mapper_class_tbls_process(&parms);
		if (rc) {
			BNXT_TF_DBG(ERR,
				    "class tables failed creation for %d:%d\n",
				    parms.dev_id, parms.class_tid);
			return rc;
		}
	}
	return rc;
}

/* Function to handle the mapping of the Flow to be compatible
 * with the underlying hardware.
 */
int32_t
ulp_mapper_flow_create(struct bnxt_ulp_context *ulp_ctx,
		       struct bnxt_ulp_mapper_create_parms *cparms,
		       uint32_t *flowid)
{
	struct bnxt_ulp_mapper_parms parms;
	struct ulp_regfile regfile;
	int32_t	 rc, trc;

	if (!ulp_ctx || !cparms)
		return -EINVAL;

	/* Initialize the parms structure */
	memset(&parms, 0, sizeof(parms));
	parms.act_prop = cparms->act_prop;
	parms.act_bitmap = cparms->act;
	parms.hdr_bitmap = cparms->hdr_bitmap;
	parms.regfile = &regfile;
	parms.hdr_field = cparms->hdr_field;
	parms.comp_fld = cparms->comp_fld;
	parms.tfp = bnxt_ulp_cntxt_tfp_get(ulp_ctx);
	parms.ulp_ctx = ulp_ctx;
	parms.tcam_tbl_opc = BNXT_ULP_MAPPER_TCAM_TBL_OPC_NORMAL;

	/* Get the device id from the ulp context */
	if (bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &parms.dev_id)) {
		BNXT_TF_DBG(ERR, "Invalid ulp context\n");
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

	/* Get the action table entry from device id and act context id */
	parms.act_tid = cparms->act_tid;

	/*
	 * Perform the action table get only if act template is not zero
	 * for act template zero like for default rules ignore the action
	 * table processing.
	 */
	if (parms.act_tid) {
		parms.atbls = ulp_mapper_action_tbl_list_get(parms.dev_id,
							     parms.act_tid,
							     &parms.num_atbls);
		if (!parms.atbls || !parms.num_atbls) {
			BNXT_TF_DBG(ERR, "No action tables for %d:%d\n",
				    parms.dev_id, parms.act_tid);
			return -EINVAL;
		}
	}

	/* Get the class table entry from device id and act context id */
	parms.class_tid = cparms->class_tid;
	parms.ctbls = ulp_mapper_class_tbl_list_get(parms.dev_id,
						    parms.class_tid,
						    &parms.num_ctbls,
						    &parms.tbl_idx);
	if (!parms.ctbls || !parms.num_ctbls) {
		BNXT_TF_DBG(ERR, "No class tables for %d:%d\n",
			    parms.dev_id, parms.class_tid);
		return -EINVAL;
	}

	/* Get the device params, it will be used in later processing */
	parms.device_params = bnxt_ulp_device_params_get(parms.dev_id);
	if (!parms.device_params) {
		BNXT_TF_DBG(ERR, "No class tables for %d:%d\n",
			    parms.dev_id, parms.class_tid);
		return -EINVAL;
	}

	/* initialize the registry file for further processing */
	if (!ulp_regfile_init(parms.regfile)) {
		BNXT_TF_DBG(ERR, "regfile initialization failed.\n");
		return -EINVAL;
	}

	rc = ulp_regfile_write(parms.regfile,
			       BNXT_ULP_REGFILE_INDEX_CLASS_TID,
			       tfp_cpu_to_be_64((uint64_t)parms.class_tid));
	if (!rc) {
		BNXT_TF_DBG(ERR, "Unable to write template ID to regfile\n");
		return -EINVAL;
	}

	/* Allocate a Flow ID for attaching all resources for the flow to.
	 * Once allocated, all errors have to walk the list of resources and
	 * free each of them.
	 */
	rc = ulp_flow_db_fid_alloc(ulp_ctx,
				   parms.tbl_idx,
				   cparms->func_id,
				   &parms.fid);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to allocate flow table entry\n");
		return rc;
	}

	/* Process the action template list from the selected action table*/
	if (parms.act_tid) {
		rc = ulp_mapper_action_tbls_process(&parms);
		if (rc) {
			BNXT_TF_DBG(ERR,
				    "action tables failed creation for %d:%d\n",
				    parms.dev_id, parms.act_tid);
			goto flow_error;
		}
	}

	/* All good. Now process the class template */
	rc = ulp_mapper_class_tbls_process(&parms);
	if (rc) {
		BNXT_TF_DBG(ERR, "class tables failed creation for %d:%d\n",
			    parms.dev_id, parms.class_tid);
		goto flow_error;
	}

	*flowid = parms.fid;

	return rc;

flow_error:
	/* Free all resources that were allocated during flow creation */
	trc = ulp_mapper_flow_destroy(ulp_ctx, parms.fid,
				      BNXT_ULP_REGULAR_FLOW_TABLE);
	if (trc)
		BNXT_TF_DBG(ERR, "Failed to free all resources rc=%d\n", trc);

	return rc;
}

int32_t
ulp_mapper_init(struct bnxt_ulp_context *ulp_ctx)
{
	struct bnxt_ulp_cache_tbl_params *tbl;
	struct bnxt_ulp_mapper_data *data;
	uint32_t i;
	struct tf *tfp;
	int32_t rc, csize;

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

	/* Allocate the ulp cache tables. */
	for (i = 0; i < BNXT_ULP_CACHE_TBL_MAX_SZ; i++) {
		tbl = ulp_mapper_cache_tbl_params_get(i);
		if (!tbl) {
			BNXT_TF_DBG(ERR, "Failed to get cache table parms (%d)",
				    i);
			goto error;
		}
		if (tbl->num_entries != 0) {
			csize = sizeof(struct bnxt_ulp_mapper_cache_entry) *
				tbl->num_entries;
			data->cache_tbl[i] = rte_zmalloc("ulp mapper cache tbl",
							 csize, 0);
			if (!data->cache_tbl[i]) {
				BNXT_TF_DBG(ERR, "Failed to allocate Cache "
					    "table %d.\n", i);
				rc = -ENOMEM;
				goto error;
			}
		}
	}

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
	uint32_t i;
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
	/* Free the ulp cache tables */
	for (i = 0; i < BNXT_ULP_CACHE_TBL_MAX_SZ; i++) {
		rte_free(data->cache_tbl[i]);
		data->cache_tbl[i] = NULL;
	}

	rte_free(data);
	/* Reset the data pointer within the ulp_ctx. */
	bnxt_ulp_cntxt_ptr2_mapper_data_set(ulp_ctx, NULL);
}
