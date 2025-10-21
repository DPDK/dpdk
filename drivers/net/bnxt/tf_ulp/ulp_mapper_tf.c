/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#include "ulp_mapper.h"
#include "ulp_flow_db.h"
#include "ulp_ha_mgr.h"
#include "tfp.h"
#include "tf_util.h"
#include "bnxt_ulp_utils.h"
#include "bnxt_ulp_tf.h"

#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#include "ulp_template_debug_proto.h"
#include "ulp_tf_debug.h"
#endif

/* Internal function to write the tcam entry */
static int32_t
ulp_mapper_tf_tcam_tbl_entry_write(struct bnxt_ulp_mapper_parms *parms,
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
		BNXT_DRV_DBG(ERR, "Failed to get truflow pointer\n");
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
		BNXT_DRV_DBG(ERR, "tcam[%s][%s][%x] write failed.\n",
			     tf_tcam_tbl_2_str(sparms.tcam_tbl_type),
			     tf_dir_2_str(sparms.dir), sparms.idx);
		return -EIO;
	}
	BNXT_DRV_INF("tcam[%s][%s][%x] write success.\n",
		     tf_tcam_tbl_2_str(sparms.tcam_tbl_type),
		     tf_dir_2_str(sparms.dir), sparms.idx);

	/* Mark action */
	rc = ulp_mapper_mark_act_ptr_process(parms, tbl);
	if (rc) {
		BNXT_DRV_DBG(ERR, "failed mark action processing\n");
		return rc;
	}

#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG_MAPPER
	ulp_mapper_tcam_entry_dump("TCAM", idx, tbl, key, mask, data);
#endif
#endif
	return rc;
}

static
int32_t ulp_mapper_tf_tcam_is_wc_tcam(struct bnxt_ulp_mapper_tbl_info *tbl)
{
	if (tbl->resource_type == TF_TCAM_TBL_TYPE_WC_TCAM ||
	    tbl->resource_type == TF_TCAM_TBL_TYPE_WC_TCAM_HIGH ||
	    tbl->resource_type == TF_TCAM_TBL_TYPE_WC_TCAM_LOW)
		return 1;
	return 0;
}

static int32_t
ulp_mapper_tf_tcam_tbl_process(struct bnxt_ulp_mapper_parms *parms,
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
	struct ulp_flow_db_res_params	fid_parms	= { 0 };
	struct tf_free_tcam_entry_parms free_parms	= { 0 };
	uint16_t tmplen = 0;
	uint16_t idx = 0;
	enum bnxt_ulp_byte_order key_byte_order;

	/* Set the key and mask to the original key and mask. */
	key = &okey;
	mask = &omask;

	/* Skip this tcam table opcode is NOP */
	if (tbl->tbl_opcode == BNXT_ULP_TCAM_TBL_OPC_NOT_USED ||
	    tbl->tbl_opcode >= BNXT_ULP_TCAM_TBL_OPC_LAST) {
		BNXT_DRV_DBG(ERR, "Invalid tcam table opcode %d\n",
			     tbl->tbl_opcode);
		return 0;
	}

	tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx, tbl->session_type);
	if (!tfp) {
		BNXT_DRV_DBG(ERR, "Failed to get truflow pointer\n");
		return -EINVAL;
	}

	/* If only allocation of identifier then perform and exit */
	if (tbl->tbl_opcode == BNXT_ULP_TCAM_TBL_OPC_ALLOC_IDENT) {
		rc = ulp_mapper_tcam_tbl_ident_alloc(parms, tbl);
		return rc;
	}

	if (tbl->key_recipe_opcode == BNXT_ULP_KEY_RECIPE_OPC_DYN_KEY)
		kflds = ulp_mapper_key_recipe_fields_get(parms, tbl, &num_kflds);
	else
		kflds = ulp_mapper_key_fields_get(parms, tbl, &num_kflds);
	if (!kflds || !num_kflds) {
		BNXT_DRV_DBG(ERR, "Failed to get key fields\n");
		return -EINVAL;
	}

	if (ulp_mapper_tf_tcam_is_wc_tcam(tbl))
		key_byte_order = dparms->wc_key_byte_order;
	else
		key_byte_order = dparms->key_byte_order;

	if (ulp_blob_init(key, tbl->blob_key_bit_size, key_byte_order) ||
	    ulp_blob_init(mask, tbl->blob_key_bit_size, key_byte_order) ||
	    ulp_blob_init(&data, tbl->result_bit_size,
			  dparms->result_byte_order) ||
	    ulp_blob_init(&update_data, tbl->result_bit_size,
			  dparms->result_byte_order)) {
		BNXT_DRV_DBG(ERR, "blob inits failed.\n");
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
			BNXT_DRV_DBG(ERR, "Key field set failed %s\n",
				     kflds[i].field_info_spec.description);
			return rc;
		}

		/* Setup the mask */
		rc = ulp_mapper_field_opc_process(parms, tbl->direction,
						  &kflds[i].field_info_mask,
						  mask, 0, "TCAM Mask");
		if (rc) {
			BNXT_DRV_DBG(ERR, "Mask field set failed %s\n",
				     kflds[i].field_info_mask.description);
			return rc;
		}
	}

	/* For wild card tcam perform the post process to swap the blob */
	if (ulp_mapper_tf_tcam_is_wc_tcam(tbl)) {
		if (dparms->wc_dynamic_pad_en) {
			/* Sets up the slices for writing to the WC TCAM */
			rc = ulp_mapper_wc_tcam_tbl_dyn_post_process(dparms,
								     key, mask,
								     &tkey,
								     &tmask);
			if (rc) {
				BNXT_DRV_DBG(ERR,
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
			BNXT_DRV_DBG(ERR, "entry priority process failed\n");
			return rc;
		}

		rc = tf_alloc_tcam_entry(tfp, &aparms);
		if (rc) {
			BNXT_DRV_DBG(ERR, "tcam alloc failed rc=%d.\n", rc);
			return rc;
		}
		idx = aparms.idx;
	}

	/* Write the tcam index into the regfile*/
	if (ulp_regfile_write(parms->regfile, tbl->tbl_operand,
			      (uint64_t)tfp_cpu_to_be_64(idx))) {
		BNXT_DRV_DBG(ERR, "Regfile[%d] write failed.\n",
			     tbl->tbl_operand);
		rc = -EINVAL;
		/* Need to free the tcam idx, so goto error */
		goto error;
	}

	/* if it is miss then it is same as no search before alloc */
	if (tbl->tbl_opcode == BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE) {
		/*Scan identifier list, allocate identifier and update regfile*/
		rc = ulp_mapper_tcam_tbl_ident_alloc(parms, tbl);
		/* Create the result blob */
		if (!rc)
			rc = ulp_mapper_tbl_result_build(parms, tbl, &data,
							 "TCAM Result");
		/* write the tcam entry */
		if (!rc)
			rc = ulp_mapper_tf_tcam_tbl_entry_write(parms, tbl, key,
								mask, &data,
								idx);
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
		BNXT_DRV_DBG(ERR, "Failed to link resource to flow rc = %d\n",
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
		BNXT_DRV_DBG(ERR, "Failed to free tcam[%d][%d][%d] on failure\n",
			     tbl->resource_type, tbl->direction, idx);
	return rc;
}

static int32_t
ulp_mapper_tf_em_tbl_process(struct bnxt_ulp_mapper_parms *parms,
			     struct bnxt_ulp_mapper_tbl_info *tbl,
			     void *error)
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
		BNXT_DRV_DBG(ERR, "Failed to get the mem type for EM\n");
		return -EINVAL;
	}

	if (tbl->key_recipe_opcode == BNXT_ULP_KEY_RECIPE_OPC_DYN_KEY)
		kflds = ulp_mapper_key_recipe_fields_get(parms, tbl, &num_kflds);
	else
		kflds = ulp_mapper_key_fields_get(parms, tbl, &num_kflds);
	if (!kflds || !num_kflds) {
		BNXT_DRV_DBG(ERR, "Failed to get key fields\n");
		return -EINVAL;
	}

	key_order = dparms->em_byte_order;
	res_order = dparms->em_byte_order;

	/* Initialize the key/result blobs */
	if (ulp_blob_init(&key, tbl->blob_key_bit_size, key_order) ||
	    ulp_blob_init(&data, tbl->result_bit_size, res_order)) {
		BNXT_DRV_DBG(ERR, "blob inits failed.\n");
		return -EINVAL;
	}

	/* create the key */
	for (i = 0; i < num_kflds; i++) {
		/* Setup the key */
		rc = ulp_mapper_field_opc_process(parms, tbl->direction,
						  &kflds[i].field_info_spec,
						  &key, 1, "EM Key");
		if (rc) {
			BNXT_DRV_DBG(ERR, "Key field set failed.\n");
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
			BNXT_DRV_DBG(ERR, "Invalid em blk size and align\n");
			return -EINVAL;
		}
		ulp_blob_pad_push(&data, (uint32_t)pad);
	}

	/* Create the result data blob */
	rc = ulp_mapper_tbl_result_build(parms, tbl, &data, "EM Result");
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to build the result blob\n");
		return rc;
	}
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG_MAPPER
	ulp_mapper_result_dump("EM Result", tbl, &data);
#endif
#endif
	if (dparms->em_dynamic_pad_en) {
		uint32_t abits = dparms->em_blk_align_bits;

		/* when dynamic padding is enabled merge result + key */
		rc = ulp_blob_block_merge(&data, &key, abits, pad);
		if (rc) {
			BNXT_DRV_DBG(ERR, "Failed to merge the result blob\n");
			return rc;
		}

		/* add padding to make sure merged result is at slice boundary*/
		ulp_blob_pad_align(&data, abits);

		ulp_blob_perform_byte_reverse(&data, ULP_BITS_2_BYTE(abits));
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG_MAPPER
	ulp_mapper_result_dump("EM Merged Result", tbl, &data);
#endif
#endif
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
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG_MAPPER
	ulp_mapper_result_dump("EM Key Transpose", tbl, &key);
#endif
#endif
	}

	rc = bnxt_ulp_cntxt_tbl_scope_id_get(parms->ulp_ctx,
					     &iparms.tbl_scope_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to get table scope rc=%d\n", rc);
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
		/* Set the error flag in reg file */
		if (tbl->tbl_opcode == BNXT_ULP_EM_TBL_OPC_WR_REGFILE) {
			uint64_t val = 0;

			/* over max flows or hash collision */
			if (rc == -EIO || rc == -ENOMEM) {
				val = 1;
				rc = 0;
				BNXT_DRV_DBG(DEBUG,
					     "Fail to insert EM, shall add to wc\n");
			}
			ulp_regfile_write(parms->regfile, tbl->tbl_operand,
					  tfp_cpu_to_be_64(val));
		}
		if (rc)
			BNXT_DRV_DBG(ERR,
				     "Failed to insert em entry rc=%d.\n", rc);
		if (rc && error != NULL)
			rte_flow_error_set((struct rte_flow_error *)error, EIO,
					   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					   "error adding EM entry");
		return rc;
	}

#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG_MAPPER
	ulp_mapper_em_dump("EM", &key, &data, &iparms);
	/* tf_dump_tables(tfp, iparms.tbl_scope_id); */
#endif
#endif
	/* Mark action process */
	if (mtype == BNXT_ULP_FLOW_MEM_TYPE_EXT &&
	    tbl->resource_type == TF_MEM_EXTERNAL)
		rc = ulp_mapper_mark_gfid_process(parms, tbl, iparms.flow_id);
	else if (mtype == BNXT_ULP_FLOW_MEM_TYPE_INT &&
		 tbl->resource_type == TF_MEM_INTERNAL)
		rc = ulp_mapper_mark_act_ptr_process(parms, tbl);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to add mark to flow\n");
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
		BNXT_DRV_DBG(ERR, "Fail to link res to flow rc = %d\n",
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
		BNXT_DRV_DBG(ERR, "Failed to delete EM entry on failed add\n");

	if (error != NULL)
		rte_flow_error_set((struct rte_flow_error *)error, EIO,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "error adding EM entry");
	return rc;
}

static uint16_t
ulp_mapper_tf_dyn_blob_size_get(struct bnxt_ulp_mapper_parms *mparms,
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

static int32_t
ulp_mapper_tf_em_entry_free(struct bnxt_ulp_context *ulp,
			    struct ulp_flow_db_res_params *res,
			    __rte_unused void *error)
{
	struct tf_delete_em_entry_parms fparms = { 0 };
	struct tf *tfp;
	uint32_t session_type;
	int32_t rc;

	session_type = ulp_flow_db_shared_session_get(res);
	tfp = bnxt_ulp_cntxt_tfp_get(ulp, session_type);
	if (!tfp) {
		BNXT_DRV_DBG(ERR, "Failed to get tf pointer\n");
		return -EINVAL;
	}

	fparms.dir = res->direction;
	fparms.flow_handle = res->resource_hndl;

	rc = bnxt_ulp_cntxt_tbl_scope_id_get(ulp, &fparms.tbl_scope_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to get table scope\n");
		return -EINVAL;
	}

	return tf_delete_em_entry(tfp, &fparms);
}

static uint32_t
ulp_mapper_tf_dyn_tbl_type_get(struct bnxt_ulp_mapper_parms *mparms,
			       struct bnxt_ulp_mapper_tbl_info *tbl,
			       uint16_t blob_len,
			       uint16_t *out_len)
{
	struct bnxt_ulp_device_params *d_params = mparms->device_params;
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

static int32_t
ulp_mapper_tf_index_tbl_process(struct bnxt_ulp_mapper_parms *parms,
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
	uint16_t blob_len;

	tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx, tbl->session_type);
	/* compute the blob size */
	bit_size = ulp_mapper_tf_dyn_blob_size_get(parms, tbl);

	/* Initialize the blob data */
	if (ulp_blob_init(&data, bit_size,
			  parms->device_params->result_byte_order)) {
		BNXT_DRV_DBG(ERR, "Failed to initialize index table blob\n");
		return -EINVAL;
	}

	/* Get the scope id first */
	rc = bnxt_ulp_cntxt_tbl_scope_id_get(parms->ulp_ctx, &tbl_scope_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to get table scope rc=%d\n", rc);
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
		if (ulp_regfile_read(parms->regfile,
				    tbl->tbl_operand,
				    &regval)) {
			BNXT_DRV_DBG(ERR,
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
			BNXT_DRV_DBG(ERR, "Template error, wrong fdb opcode\n");
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
			BNXT_DRV_DBG(ERR,
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
			BNXT_DRV_DBG(ERR,
				     "Ext Table Read Opcode not supported.\n");
			return -EINVAL;
		}
		if (ulp_regfile_read(parms->regfile,
				     tbl->tbl_operand, &regval)) {
			BNXT_DRV_DBG(ERR,
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
			BNXT_DRV_DBG(ERR,
				     "Failed to read the tbl entry %d:%d\n",
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
			BNXT_DRV_DBG(ERR,
				     "Failed to get flds on tbl read rc=%d\n",
				     rc);
			return rc;
		}
		return 0;
	default:
		BNXT_DRV_DBG(ERR, "Invalid index table opcode %d\n",
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
			BNXT_DRV_DBG(ERR, "Failed to build the result blob\n");
			return rc;
		}
	}

	if (alloc) {
		aparms.dir = tbl->direction;
		blob_len = ulp_blob_data_len_get(&data);
		tbl_type = ulp_mapper_tf_dyn_tbl_type_get(parms, tbl,
							  blob_len,
							  &tmplen);
		aparms.type = tbl_type;
		aparms.tbl_scope_id	= tbl_scope_id;

		/* All failures after the alloc succeeds require a free */
		rc = tf_alloc_tbl_entry(tfp, &aparms);
		if (rc) {
			BNXT_DRV_DBG(ERR, "Alloc table[%s][%s] failed rc=%d\n",
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
			BNXT_DRV_DBG(ERR,
				     "Failed to write %s regfile[%d] rc=%d\n",
				     (global) ? "global" : "reg",
				     tbl->tbl_operand, rc);
			goto error;
		}
	}

	if (write) {
		sparms.dir = tbl->direction;
		sparms.data = ulp_blob_data_get(&data, &tmplen);
		blob_len = ulp_blob_data_len_get(&data);
		tbl_type = ulp_mapper_tf_dyn_tbl_type_get(parms, tbl,
							  blob_len,
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
			BNXT_DRV_DBG(ERR,
				   "Index table[%s][%s][%x] write fail rc=%d\n",
				     tf_tbl_type_2_str(sparms.type),
				     tf_dir_2_str(sparms.dir),
				     sparms.idx, rc);
			goto error;
		}
		BNXT_DRV_INF("Index table[%s][%s][%x] write successful.\n",
			     tf_tbl_type_2_str(sparms.type),
			     tf_dir_2_str(sparms.dir), sparms.idx);

		/* Calculate action record size */
		if (tbl->resource_type == TF_TBL_TYPE_EXT) {
			act_rec_size = (ULP_BITS_2_BYTE_NR(tmplen) + 15) / 16;
			act_rec_size--;
			if (ulp_regfile_write(parms->regfile,
					      BNXT_ULP_RF_IDX_ACTION_REC_SIZE,
					      tfp_cpu_to_be_64(act_rec_size)))
				BNXT_DRV_DBG(ERR,
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
		BNXT_DRV_DBG(ERR, "Failed to link resource to flow rc = %d\n",
			     rc);
		goto error;
	}

	/* Perform the VF rep action */
	rc = ulp_mapper_mark_vfr_idx_process(parms, tbl);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to add vfr mark rc = %d\n", rc);
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
		BNXT_DRV_DBG(ERR, "Failed to free tbl entry on failure\n");

	return rc;
}

static int32_t
ulp_mapper_tf_cmm_tbl_process(struct bnxt_ulp_mapper_parms *parms __rte_unused,
			      struct bnxt_ulp_mapper_tbl_info *tbl __rte_unused,
			      void *error __rte_unused)
{
	/* CMM does not exist in TF library*/
	BNXT_DRV_DBG(ERR, "Invalid resource func,CMM is not supported on TF\n");
	return 0;
}

static int32_t
ulp_mapper_tf_cmm_entry_free(struct bnxt_ulp_context *ulp __rte_unused,
			     struct ulp_flow_db_res_params *res __rte_unused,
			     void *error __rte_unused)
{
	/* CMM does not exist in TF library*/
	BNXT_DRV_DBG(ERR, "Invalid resource func,CMM is not supported on TF\n");
	return 0;
}

static int32_t
ulp_mapper_tf_if_tbl_process(struct bnxt_ulp_mapper_parms *parms,
			     struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct ulp_blob	data, res_blob;
	uint64_t idx = 0;
	uint16_t tmplen;
	int32_t rc = 0;
	struct tf_set_if_tbl_entry_parms iftbl_params = { 0 };
	struct tf_get_if_tbl_entry_parms get_parms = { 0 };
	struct tf *tfp;
	enum bnxt_ulp_if_tbl_opc if_opc = tbl->tbl_opcode;
	uint32_t res_size;

	tfp = bnxt_ulp_cntxt_tfp_get(parms->ulp_ctx, tbl->session_type);
	/* Initialize the blob data */
	if (ulp_blob_init(&data, tbl->result_bit_size,
			  parms->device_params->result_byte_order)) {
		BNXT_DRV_DBG(ERR, "Failed initial index table blob\n");
		return -EINVAL;
	}

	/* create the result blob */
	rc = ulp_mapper_tbl_result_build(parms, tbl, &data, "IFtable Result");
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to build the result blob\n");
		return rc;
	}

	/* Get the index details */
	switch (if_opc) {
	case BNXT_ULP_IF_TBL_OPC_WR_COMP_FIELD:
		idx = ULP_COMP_FLD_IDX_RD(parms, tbl->tbl_operand);
		break;
	case BNXT_ULP_IF_TBL_OPC_WR_REGFILE:
		if (ulp_regfile_read(parms->regfile, tbl->tbl_operand, &idx)) {
			BNXT_DRV_DBG(ERR, "regfile[%d] read oob\n",
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
		if (ulp_blob_init(&res_blob, tbl->result_bit_size,
				  parms->device_params->result_byte_order)) {
			BNXT_DRV_DBG(ERR, "Failed initial result blob\n");
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
			BNXT_DRV_DBG(ERR, "Get table[%d][%s][%x] failed rc=%d\n",
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
			BNXT_DRV_DBG(ERR, "Scan and extract failed rc=%d\n",
				     rc);
		return rc;
	case BNXT_ULP_IF_TBL_OPC_NOT_USED:
		return rc; /* skip it */
	default:
		BNXT_DRV_DBG(ERR, "Invalid tbl index opcode\n");
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
		BNXT_DRV_DBG(ERR, "Set table[%d][%s][%x] failed rc=%d\n",
			     iftbl_params.type,/* TBD: add tf_if_tbl_2_str */
			     tf_dir_2_str(iftbl_params.dir),
			     iftbl_params.idx, rc);
		return rc;
	}
	BNXT_DRV_INF("Set table[%s][%s][%x] success.\n",
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
ulp_mapper_tf_ident_alloc(struct bnxt_ulp_context *ulp_ctx,
			  uint32_t session_type,
			  uint16_t ident_type,
			  uint8_t direction,
			  enum cfa_track_type tt __rte_unused,
			  uint64_t *identifier_id)
{
	struct tf_alloc_identifier_parms iparms = {0};
	struct tf *tfp;
	int32_t rc = 0;

	tfp = bnxt_ulp_cntxt_tfp_get(ulp_ctx, session_type);
	if (!tfp) {
		BNXT_DRV_DBG(ERR, "Failed to get tf pointer\n");
		return -EINVAL;
	}

	iparms.ident_type = ident_type;
	iparms.dir = direction;

	rc = tf_alloc_identifier(tfp, &iparms);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Alloc ident %s:%s failed.\n",
			     tf_dir_2_str(iparms.dir),
			     tf_ident_2_str(iparms.ident_type));
		return rc;
	}
	*identifier_id = iparms.id;
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG_MAPPER
	BNXT_DRV_INF("Allocated Identifier [%s]:[%s] = 0x%X\n",
		     tf_dir_2_str(iparms.dir),
		     tf_ident_2_str(iparms.ident_type), iparms.id);
#endif
#endif
	return rc;
}

static int32_t
ulp_mapper_tf_ident_free(struct bnxt_ulp_context *ulp_ctx,
			 struct ulp_flow_db_res_params *res)
{
	struct tf_free_identifier_parms free_parms = { 0 };
	uint32_t session_type;
	struct tf *tfp;
	int32_t rc = 0;

	session_type = ulp_flow_db_shared_session_get(res);
	tfp = bnxt_ulp_cntxt_tfp_get(ulp_ctx, session_type);
	if (!tfp) {
		BNXT_DRV_DBG(ERR, "Failed to get tf pointer\n");
		return -EINVAL;
	}

	free_parms.ident_type = res->resource_type;
	free_parms.dir = res->direction;
	free_parms.id = res->resource_hndl;

	(void)tf_free_identifier(tfp, &free_parms);
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG_MAPPER
	BNXT_DRV_INF("Freed Identifier [%s]:[%s] = 0x%X\n",
		     tf_dir_2_str(free_parms.dir),
		     tf_ident_2_str(free_parms.ident_type),
		     (uint32_t)free_parms.id);
#endif
#endif
	return rc;
}

static inline int32_t
ulp_mapper_tf_tcam_entry_free(struct bnxt_ulp_context *ulp,
			      struct ulp_flow_db_res_params *res)
{
	struct tf *tfp;
	struct tf_free_tcam_entry_parms fparms = {
		.dir		= res->direction,
		.tcam_tbl_type	= res->resource_type,
		.idx		= (uint16_t)res->resource_hndl
	};

	tfp = bnxt_ulp_cntxt_tfp_get(ulp, ulp_flow_db_shared_session_get(res));
	if (!tfp) {
		BNXT_DRV_DBG(ERR, "Unable to free resource failed to get tfp\n");
		return -EINVAL;
	}

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
				BNXT_DRV_DBG(ERR,
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
	static uint8_t fld_zeros[16] = { 0 };
	uint32_t dev_id = BNXT_ULP_DEVICE_ID_LAST;
	int32_t rc = 0;

	rc = bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to get the dev id from ulp.\n");
		return rc;
	}

	if (dev_id == BNXT_ULP_DEVICE_ID_THOR) {
		sparms.dir = fparms->dir;
		sparms.data = fld_zeros;
		sparms.type = fparms->type;
		sparms.data_sz_in_bytes = 16; /* FULL ACT REC SIZE - THOR */
		sparms.idx = fparms->idx;
		sparms.tbl_scope_id = fparms->tbl_scope_id;
		rc = tf_set_tbl_entry(tfp, &sparms);
		if (rc) {
			BNXT_DRV_DBG(ERR,
				     "Index table[%s][%s][%x] write fail %d\n",
				     tf_tbl_type_2_str(sparms.type),
				     tf_dir_2_str(sparms.dir),
				     sparms.idx, rc);
			return rc;
		}
	}
	return 0;
}

static inline int32_t
ulp_mapper_tf_index_entry_free(struct bnxt_ulp_context *ulp,
			       struct ulp_flow_db_res_params *res)
{
	struct tf *tfp;
	struct tf_free_tbl_entry_parms fparms = {
		.dir	= res->direction,
		.type	= res->resource_type,
		.idx	= (uint32_t)res->resource_hndl
	};

	tfp = bnxt_ulp_cntxt_tfp_get(ulp, ulp_flow_db_shared_session_get(res));
	if (!tfp) {
		BNXT_DRV_DBG(ERR,
			     "Unable to free resource failed to get tfp\n");
		return -EINVAL;
	}

	/* Get the table scope, it may be ignored */
	(void)bnxt_ulp_cntxt_tbl_scope_id_get(ulp, &fparms.tbl_scope_id);

	if (fparms.type == TF_TBL_TYPE_FULL_ACT_RECORD)
		(void)ulp_mapper_clear_full_action_record(tfp, ulp, &fparms);

#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG_MAPPER
	BNXT_DRV_INF("Free index table [%s]:[%s] = 0x%X\n",
		     tf_dir_2_str(fparms.dir),
		     tf_tbl_type_2_str(fparms.type),
		     (uint32_t)fparms.idx);
#endif
#endif
	return tf_free_tbl_entry(tfp, &fparms);
}

static int32_t
ulp_mapper_tf_index_tbl_alloc_process(struct bnxt_ulp_context *ulp,
				      uint32_t session_type,
				      uint16_t table_type,
				      uint8_t direction,
				      uint64_t *index)
{
	struct tf_alloc_tbl_entry_parms	aparms = { 0 };
	struct tf *tfp;
	uint32_t tbl_scope_id;
	int32_t rc = 0;

	tfp = bnxt_ulp_cntxt_tfp_get(ulp, session_type);
	if (!tfp)
		return -EINVAL;

	/* Get the scope id */
	rc = bnxt_ulp_cntxt_tbl_scope_id_get(ulp, &tbl_scope_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to get table scope rc=%d\n", rc);
		return rc;
	}

	aparms.type = table_type;
	aparms.dir = direction;
	aparms.tbl_scope_id = tbl_scope_id;

	/* Allocate the index tbl using tf api */
	rc = tf_alloc_tbl_entry(tfp, &aparms);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to alloc index table [%s][%d]\n",
			    tf_dir_2_str(aparms.dir), aparms.type);
		return rc;
	}

	*index = aparms.idx;

#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG_MAPPER
		BNXT_DRV_DBG(DEBUG, "Allocated Table Index [%s][%s] = 0x%04x\n",
			     tf_tbl_type_2_str(aparms.type),
			     tf_dir_2_str(aparms.dir),
			     aparms.idx);
#endif
#endif
	return rc;
}

static int32_t
ulp_mapper_tf_app_glb_resource_info_init(struct bnxt_ulp_context *ulp_ctx,
					 struct bnxt_ulp_mapper_data *mapper_data)
{
	struct bnxt_ulp_glb_resource_info *glb_res;
	uint32_t num_entries, idx, dev_id;
	uint8_t app_id;
	int32_t rc = 0;

	glb_res = bnxt_ulp_app_glb_resource_info_list_get(&num_entries);
	/* Check if there are no resources */
	if (!num_entries)
		return 0;

	rc = bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to get device id for glb init (%d)\n",
			    rc);
		return rc;
	}

	rc = bnxt_ulp_cntxt_app_id_get(ulp_ctx, &app_id);
	if (rc) {
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
								true);
			break;
		case BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE:
			rc = ulp_mapper_resource_index_tbl_alloc(ulp_ctx,
								 mapper_data,
								 &glb_res[idx],
								 true);
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

static int32_t
ulp_mapper_tf_handle_to_offset(struct bnxt_ulp_mapper_parms *parms __rte_unused,
			       uint64_t handle __rte_unused,
			       uint32_t offset __rte_unused,
			       uint64_t *result __rte_unused)
{
	BNXT_DRV_DBG(ERR, "handle to offset not supported in tf\n");
	return -EINVAL;
}

static int
ulp_mapper_tf_mpc_batch_start(struct tfc_mpc_batch_info_t *batch_info __rte_unused)
{
	return 0;
}

static int
ulp_mapper_tf_mpc_batch_end(struct tfc *tfcp __rte_unused,
			    struct tfc_mpc_batch_info_t *batch_info __rte_unused)
{
	return 0;
}

static bool
ulp_mapper_tf_mpc_batch_started(struct tfc_mpc_batch_info_t *batch_info __rte_unused)
{
	return false;
}

const struct ulp_mapper_core_ops ulp_mapper_tf_core_ops = {
	.ulp_mapper_core_tcam_tbl_process = ulp_mapper_tf_tcam_tbl_process,
	.ulp_mapper_core_tcam_entry_free = ulp_mapper_tf_tcam_entry_free,
	.ulp_mapper_core_em_tbl_process = ulp_mapper_tf_em_tbl_process,
	.ulp_mapper_core_em_entry_free = ulp_mapper_tf_em_entry_free,
	.ulp_mapper_core_index_tbl_process = ulp_mapper_tf_index_tbl_process,
	.ulp_mapper_core_index_entry_free = ulp_mapper_tf_index_entry_free,
	.ulp_mapper_core_cmm_tbl_process = ulp_mapper_tf_cmm_tbl_process,
	.ulp_mapper_core_cmm_entry_free = ulp_mapper_tf_cmm_entry_free,
	.ulp_mapper_core_if_tbl_process = ulp_mapper_tf_if_tbl_process,
	.ulp_mapper_core_ident_alloc_process = ulp_mapper_tf_ident_alloc,
	.ulp_mapper_core_ident_free = ulp_mapper_tf_ident_free,
	.ulp_mapper_core_dyn_tbl_type_get = ulp_mapper_tf_dyn_tbl_type_get,
	.ulp_mapper_core_index_tbl_alloc_process =
		ulp_mapper_tf_index_tbl_alloc_process,
	.ulp_mapper_core_app_glb_res_info_init =
		ulp_mapper_tf_app_glb_resource_info_init,
	.ulp_mapper_core_handle_to_offset = ulp_mapper_tf_handle_to_offset,
	.ulp_mapper_mpc_batch_started = ulp_mapper_tf_mpc_batch_started,
	.ulp_mapper_mpc_batch_start = ulp_mapper_tf_mpc_batch_start,
	.ulp_mapper_mpc_batch_end = ulp_mapper_tf_mpc_batch_end,
	.ulp_mapper_mtr_stats_hndl_set = NULL,
	.ulp_mapper_mtr_stats_hndl_get = NULL,
	.ulp_mapper_mtr_stats_hndl_del = NULL
};
