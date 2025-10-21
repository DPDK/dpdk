/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */
#include <rte_byteorder.h>
#include "ulp_mapper.h"
#include "ulp_flow_db.h"
#include "cfa_resources.h"
#include "cfa_bld.h"
#include "tfc_util.h"
#include "bnxt_ulp_tfc.h"
#include "bnxt_ulp_utils.h"
#include "tfc_action_handle.h"

#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#include "ulp_template_debug_proto.h"
#include "ulp_tf_debug.h"
#include "tfc_debug.h"
#endif

#define BNXT_METER_MAX_NUM 1024
static struct bnxt_mtr_stats_id_map mtr_stats[BNXT_METER_MAX_NUM];

static uint32_t CFA_RESTYPE_TO_BLKT(uint8_t idx_tbl_restype)
{
	return (idx_tbl_restype > CFA_RSUBTYPE_IDX_TBL_MAX) ?
			CFA_IDX_TBL_BLKTYPE_RXP : CFA_IDX_TBL_BLKTYPE_CFA;
}

/* Internal function to write the tcam entry */
static int32_t
ulp_mapper_tfc_tcam_tbl_entry_write(struct bnxt_ulp_mapper_parms *parms,
				    struct bnxt_ulp_mapper_tbl_info *tbl,
				    struct ulp_blob *key,
				    struct ulp_blob *mask,
				    struct ulp_blob *remap,
				    uint16_t idx)
{
	struct tfc_tcam_info tfc_info = {0};
	struct tfc_tcam_data tfc_data = {0};
	struct tfc *tfcp = NULL;
	uint16_t key_size = 0, mask_size = 0, remap_size = 0;
	int32_t rc;
	uint16_t fw_fid;

	tfcp = bnxt_ulp_cntxt_tfcp_get(parms->ulp_ctx);
	if (unlikely(tfcp == NULL)) {
		BNXT_DRV_DBG(ERR, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_fid_get(parms->ulp_ctx, &fw_fid);
	if (unlikely(rc))
		return rc;

	tfc_info.dir = tbl->direction;
	tfc_info.rsubtype = tbl->resource_type;
	tfc_info.id = idx;
	tfc_data.key = ulp_blob_data_get(key, &key_size);
	tfc_data.key_sz_in_bytes = ULP_BITS_2_BYTE(key_size);
	tfc_data.mask = ulp_blob_data_get(mask, &mask_size);
	tfc_data.remap = ulp_blob_data_get(remap, &remap_size);
	remap_size = ULP_BITS_2_BYTE(remap_size);
	tfc_data.remap_sz_in_bytes = remap_size;

	if (unlikely(tfc_tcam_set(tfcp, fw_fid, &tfc_info, &tfc_data))) {
		BNXT_DRV_DBG(ERR, "tcam[%s][%s][%x] write failed.\n",
			     tfc_tcam_2_str(tfc_info.rsubtype),
			     tfc_dir_2_str(tfc_info.dir), tfc_info.id);
		return -EIO;
	}
	PMD_DRV_LOG_LINE(INFO, "tcam[%s][%s][%x] write success.",
		    tfc_tcam_2_str(tfc_info.rsubtype),
		    tfc_dir_2_str(tfc_info.dir), tfc_info.id);

	/* Mark action */
	rc = ulp_mapper_mark_act_ptr_process(parms, tbl);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "failed mark action processing\n");
		return rc;
	}

#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG_MAPPER
	ulp_mapper_tcam_entry_dump("TCAM", idx, tbl, key, mask, remap);
#endif
#endif
	return rc;
}

static uint32_t
ulp_mapper_tfc_wc_tcam_post_process(struct bnxt_ulp_device_params *dparms,
				    struct ulp_blob *key,
				    struct ulp_blob *tkey)
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
	if (unlikely(ulp_blob_pad_push(key, pad) < 0)) {
		BNXT_DRV_DBG(ERR, "Unable to pad key/mask\n");
		return -EINVAL;
	}

	/* The new length accounts for the ctrl word length and num slices */
	tlen = tlen + (clen + 1) * num_slices;
	if (unlikely(ulp_blob_init(tkey, tlen, key->byte_order))) {
		BNXT_DRV_DBG(ERR, "Unable to post process wc tcam entry\n");
		return -EINVAL;
	}

	/* pad any remaining bits to do byte alignment */
	pad = (slice_width + clen) * num_slices;
	pad = ULP_BYTE_ROUND_OFF_8(pad) - pad;
	if (unlikely(ulp_blob_pad_push(tkey, pad) < 0)) {
		BNXT_DRV_DBG(ERR, "Unable to pad key/mask\n");
		return -EINVAL;
	}

	/* Build the transformed key/mask */
	cword = dparms->wc_mode_list[num_slices - 1];
	cword = rte_cpu_to_be_32(cword);
	offset = 0;
	for (i = 0; i < num_slices; i++) {
		val = ulp_blob_push_32(tkey, &cword, clen);
		if (unlikely(!val)) {
			BNXT_DRV_DBG(ERR, "Key ctrl word push failed\n");
			return -EINVAL;
		}
		rc = ulp_blob_append(tkey, key, offset, slice_width);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Key blob append failed\n");
			return rc;
		}
		offset += slice_width;
	}
	blen = ulp_blob_data_len_get(tkey);
	/* reverse the blob byte wise in reverse */
	ulp_blob_perform_byte_reverse(tkey, ULP_BITS_2_BYTE(blen));
	return 0;
}

static int32_t
ulp_mapper_tfc_tcam_tbl_process(struct bnxt_ulp_mapper_parms *parms,
				struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct bnxt_ulp_device_params *dparms = parms->device_params;
	struct ulp_blob okey, omask, *key, *mask, data;
	struct ulp_blob tkey, tmask; /* transform key and mask */
	uint32_t alloc_tcam = 0, alloc_ident = 0, write_tcam = 0;
	struct ulp_flow_db_res_params fid_parms = { 0 };
	enum cfa_track_type tt = tbl->track_type;
	enum bnxt_ulp_byte_order key_byte_order;
	enum bnxt_ulp_byte_order res_byte_order;
	struct bnxt_ulp_mapper_key_info	*kflds;
	struct tfc_tcam_info tfc_inf = {0};
	uint16_t key_sz_in_words = 0, key_sz_in_bits = 0;
	struct tfc *tfcp = NULL;
	uint32_t num_kflds, i;
	uint32_t priority;
	int32_t rc = 0, free_rc = 0;
	uint16_t fw_fid = 0;

	/* Set the key and mask to the original key and mask. */
	key = &okey;
	mask = &omask;

	switch (tbl->tbl_opcode) {
	case BNXT_ULP_TCAM_TBL_OPC_ALLOC_IDENT:
		alloc_ident = 1;
		break;
	case BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE:
		alloc_ident = 1;
		alloc_tcam = 1;
		write_tcam = 1;
		break;
	case BNXT_ULP_TCAM_TBL_OPC_NOT_USED:
	case BNXT_ULP_TCAM_TBL_OPC_LAST:
	default:
		BNXT_DRV_DBG(ERR, "Invalid tcam table opcode %d\n",
			     tbl->tbl_opcode);
		return -EINVAL;
	}

	tfcp = bnxt_ulp_cntxt_tfcp_get(parms->ulp_ctx);
	if (unlikely(tfcp == NULL)) {
		PMD_DRV_LOG_LINE(ERR, "Failed to get tfcp pointer");
		return -EINVAL;
	}

	if (unlikely(bnxt_ulp_cntxt_fid_get(parms->ulp_ctx, &fw_fid))) {
		BNXT_DRV_DBG(ERR, "Failed to get func_id\n");
		return -EINVAL;
	}

	/* Allocate the identifiers */
	if (alloc_ident) {
		rc = ulp_mapper_tcam_tbl_ident_alloc(parms, tbl);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Failed to alloc identifier\n");
			return rc;
		}
	}

	/* If no allocation or write is needed, then just exit */
	if (unlikely(!alloc_tcam && !write_tcam))
		return rc;

	/* Initialize the blobs for write */
	if (tbl->resource_type == CFA_RSUBTYPE_TCAM_WC)
		key_byte_order = dparms->wc_key_byte_order;
	else
		key_byte_order = dparms->key_byte_order;

	res_byte_order = dparms->result_byte_order;
	if (unlikely(ulp_blob_init(key, tbl->blob_key_bit_size, key_byte_order) ||
		     ulp_blob_init(mask, tbl->blob_key_bit_size, key_byte_order) ||
		     ulp_blob_init(&data, tbl->result_bit_size, res_byte_order))) {
		BNXT_DRV_DBG(ERR, "blob inits failed.\n");
		return -EINVAL;
	}

	/* Get the key fields and update the key blob */
	if (tbl->key_recipe_opcode == BNXT_ULP_KEY_RECIPE_OPC_DYN_KEY)
		kflds = ulp_mapper_key_recipe_fields_get(parms, tbl, &num_kflds);
	else
		kflds = ulp_mapper_key_fields_get(parms, tbl, &num_kflds);
	if (unlikely(!kflds || !num_kflds)) {
		BNXT_DRV_DBG(ERR, "Failed to get key fields\n");
		return -EINVAL;
	}

	for (i = 0; i < num_kflds; i++) {
		/* Setup the key */
		rc = ulp_mapper_field_opc_process(parms, tbl->direction,
						  &kflds[i].field_info_spec,
						  key, 1, "TCAM Key");
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Key field set failed %s\n",
				     kflds[i].field_info_spec.description);
			return rc;
		}

		/* Setup the mask */
		rc = ulp_mapper_field_opc_process(parms, tbl->direction,
						  &kflds[i].field_info_mask,
						  mask, 0, "TCAM Mask");
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Mask field set failed %s\n",
				     kflds[i].field_info_mask.description);
			return rc;
		}
	}

	/* For wild card tcam perform the post process to swap the blob */
	if (tbl->resource_type == CFA_RSUBTYPE_TCAM_WC) {
		/* Sets up the slices for writing to the WC TCAM */
		rc = ulp_mapper_tfc_wc_tcam_post_process(dparms, key, &tkey);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR,
				     "Failed to post proc WC key.\n");
			return rc;
		}
		/* Sets up the slices for writing to the WC TCAM */
		rc = ulp_mapper_tfc_wc_tcam_post_process(dparms, mask, &tmask);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR,
				     "Failed to post proc WC mask.\n");
			return rc;
		}
		key = &tkey;
		mask = &tmask;
	}
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG_MAPPER
	ulp_mapper_tcam_entry_dump("TCAM", 0, tbl, key, mask, &data);
#endif
#endif

	if (alloc_tcam) {
		tfcp = bnxt_ulp_cntxt_tfcp_get(parms->ulp_ctx);
		if (unlikely(tfcp == NULL)) {
			PMD_DRV_LOG_LINE(ERR, "Failed to get tfcp pointer");
			return -EINVAL;
		}
		/* calculate the entry priority*/
		rc = ulp_mapper_priority_opc_process(parms, tbl,
						     &priority);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "entry priority process failed\n");
			return rc;
		}

		/* allocate the tcam entry, only need the length */
		(void)ulp_blob_data_get(key, &key_sz_in_bits);
		key_sz_in_words = ULP_BITS_2_BYTE(key_sz_in_bits);
		tfc_inf.dir = tbl->direction;
		tfc_inf.rsubtype = tbl->resource_type;

		rc = tfc_tcam_alloc(tfcp, fw_fid, tt, priority,
				    key_sz_in_words, &tfc_inf);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "TCAM Alloc failed, status:%d\n", rc);
			return rc;
		}

		/* Write the tcam index into the regfile*/
		if (unlikely(ulp_regfile_write(parms->regfile, tbl->tbl_operand,
					       (uint64_t)rte_cpu_to_be_64(tfc_inf.id)))) {
			BNXT_DRV_DBG(ERR, "Regfile[%d] write failed.\n",
				     tbl->tbl_operand);
			/* Need to free the tcam idx, so goto error */
			goto error;
		}
	}

	if (write_tcam) {
		/* Create the result blob */
		rc = ulp_mapper_tbl_result_build(parms, tbl, &data,
						 "TCAM Result");
		/* write the tcam entry */
		if (!rc)
			rc = ulp_mapper_tfc_tcam_tbl_entry_write(parms,
								 tbl, key,
								 mask, &data,
								 tfc_inf.id);
	}
	if (rc)
		goto error;

	/* Add the tcam index to the flow database */
	fid_parms.direction = tbl->direction;
	fid_parms.resource_func	= tbl->resource_func;
	fid_parms.resource_type	= tbl->resource_type;
	fid_parms.critical_resource = tbl->critical_resource;
	fid_parms.resource_hndl	= tfc_inf.id;
	ulp_flow_db_shared_session_set(&fid_parms, tbl->session_type);

	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to link resource to flow rc = %d\n",
			     rc);
		/* Need to free the identifier, so goto error */
		goto error;
	}

	return 0;
error:
	free_rc = tfc_tcam_free(tfcp, fw_fid, &tfc_inf);
	if (free_rc)
		BNXT_DRV_DBG(ERR, "TCAM free failed on error, status:%d\n",
			     free_rc);
	return rc;
}

static const char * const mpc_error_str[] = {
	"OK",
	"Unsupported Opcode",
	"Bad Format",
	"Invalid Scope",
	"Bad Address",
	"Cache Error",
	"EM Miss",
	"Duplicate Entry",
	"No Events",
	"EM Abort"
};

/*
 * TBD: Temporary swap until a more generic solution is designed
 *
 * blob [inout] A byte array that is being edited in-place
 * block_sz [in] The size of the blocks in bytes to swap
 *
 * The length of the blob is assumed to be a multiple of block_sz
 */
static int32_t
ulp_mapper_blob_block_swap(struct ulp_blob *blob, uint32_t block_sz)
{
	uint8_t data[block_sz]; /* size of a block for temp storage */
	uint16_t num_words, data_sz;
	uint8_t *pdata;
	int i;

	/* Shouldn't happen since it is internal function, but check anyway */
	if (unlikely(blob == NULL || !block_sz)) {
		BNXT_DRV_DBG(ERR, "Invalid arguments\n");
		return -EINVAL;
	}

	pdata = ulp_blob_data_get(blob, &data_sz);
	data_sz = ULP_BITS_2_BYTE(data_sz);
	if (unlikely(!data_sz || (data_sz % block_sz) != 0)) {
		BNXT_DRV_DBG(ERR, "length(%d) not a multiple of %d\n",
			     data_sz, block_sz);
		return -EINVAL;
	}

	num_words = data_sz / block_sz;
	for (i = 0; i < num_words / 2; i++) {
		memcpy(data, &pdata[i * block_sz], block_sz);
		memcpy(&pdata[i * block_sz],
		       &pdata[(num_words - 1 - i) * block_sz], block_sz);
		memcpy(&pdata[(num_words - 1 - i) * block_sz],
		       data, block_sz);
	}
	return 0;
}

static int
ulp_mapper_tfc_mpc_batch_end(struct tfc *tfcp,
			     struct tfc_mpc_batch_info_t *batch_info)
{
	uint32_t i;
	int rc;

	rc = tfc_mpc_batch_end(tfcp, batch_info);
	if (unlikely(rc))
		return rc;

	for (i = 0; i < batch_info->count; i++) {
		if (!batch_info->result[i])
			continue;

		switch (batch_info->comp_info[i].type) {
		case TFC_MPC_EM_INSERT:
			batch_info->em_error = batch_info->result[i];
			break;
		default:
			if (batch_info->result[i] && !batch_info->error)
				batch_info->error = batch_info->result[i];
			break;
		}
	}

	return rc;
}

static bool
ulp_mapper_tfc_mpc_batch_started(struct tfc_mpc_batch_info_t *batch_info)
{
	return tfc_mpc_batch_started(batch_info);
}

static int32_t
ulp_mapper_tfc_em_tbl_process(struct bnxt_ulp_mapper_parms *parms,
			      struct bnxt_ulp_mapper_tbl_info *tbl,
			      void *error)
{
	struct bnxt_ulp_device_params *dparms = parms->device_params;
	struct bnxt_ulp_mapper_key_info	*kflds;
	struct tfc_em_delete_parms free_parms = { 0 };
	struct tfc_em_insert_parms iparms = { 0 };
	struct ulp_flow_db_res_params fid_parms = { 0 };
	uint16_t tmplen, key_len, align_len_bits;
	enum bnxt_ulp_byte_order byte_order;
	struct ulp_blob key, data;
	uint32_t i, num_kflds;
	uint64_t handle = 0;
	struct tfc *tfcp;
	int32_t	trc = 0;
	uint8_t tsid = 0;
	int32_t rc = 0;

	tfcp = bnxt_ulp_cntxt_tfcp_get(parms->ulp_ctx);
	if (unlikely(tfcp == NULL)) {
		BNXT_DRV_DBG(ERR, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}

	if (tbl->key_recipe_opcode == BNXT_ULP_KEY_RECIPE_OPC_DYN_KEY)
		kflds = ulp_mapper_key_recipe_fields_get(parms, tbl, &num_kflds);
	else
		kflds = ulp_mapper_key_fields_get(parms, tbl, &num_kflds);
	if (unlikely(!kflds || !num_kflds)) {
		BNXT_DRV_DBG(ERR, "Failed to get key fields\n");
		return -EINVAL;
	}

	byte_order = dparms->em_byte_order;
	/* Initialize the key/result blobs */
	if (unlikely(ulp_blob_init(&key, tbl->blob_key_bit_size, byte_order) ||
		     ulp_blob_init(&data, tbl->result_bit_size, byte_order))) {
		BNXT_DRV_DBG(ERR, "blob inits failed.\n");
		return -EINVAL;
	}

	/* create the key */
	for (i = 0; i < num_kflds; i++) {
		/* Setup the key */
		rc = ulp_mapper_field_opc_process(parms, tbl->direction,
						  &kflds[i].field_info_spec,
						  &key, 1, "EM Key");
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Key field set failed.\n");
			return rc;
		}
	}
	/* add padding to make sure key is at record boundary */
	key_len = ulp_blob_data_len_get(&key);
	if (key_len > dparms->em_blk_align_bits) {
		key_len = key_len - dparms->em_blk_align_bits;
		align_len_bits = dparms->em_blk_size_bits -
			(key_len % dparms->em_blk_size_bits);
	} else {
		align_len_bits = dparms->em_blk_align_bits - key_len;
	}

	ulp_blob_pad_push(&key, align_len_bits);
	key_len = ULP_BITS_2_BYTE(ulp_blob_data_len_get(&key));
	ulp_blob_perform_byte_reverse(&key, key_len);
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG_MAPPER
	ulp_mapper_result_dump("EM Key", tbl, &key);
#endif
#endif
	/* Create the result data blob */
	rc = ulp_mapper_tbl_result_build(parms, tbl, &data, "EM Result");
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to build the result blob\n");
		return rc;
	}
	ulp_blob_pad_align(&data, dparms->em_blk_align_bits);
	key_len = ULP_BITS_2_BYTE(ulp_blob_data_len_get(&data));
	ulp_blob_perform_byte_reverse(&data, key_len);

#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG_MAPPER
	ulp_mapper_result_dump("EM Result", tbl, &data);
#endif
#endif
	rc = ulp_blob_append(&key, &data, 0, dparms->em_blk_align_bits);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "EM Failed to append the result to key(%d)",
			     rc);
		return rc;
	}
	/* TBD: Need to come up with a more generic way to know when to swap,
	 * this is fine for now as this driver only supports this device.
	 */
	rc = ulp_mapper_blob_block_swap(&key,
					ULP_BITS_2_BYTE(dparms->em_blk_size_bits));
	/* Error printed within function, just return on error */
	if (unlikely(rc))
		return rc;

#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG_MAPPER
	ulp_mapper_result_dump("EM Merged Result", tbl, &key);
#endif
#endif
	iparms.dir		 = tbl->direction;
	iparms.lkup_key_data	 = ulp_blob_data_get(&key, &tmplen);
	iparms.lkup_key_sz_words = ULP_BITS_TO_32_BYTE_WORD(tmplen);
	iparms.key_data		 = NULL;
	iparms.key_sz_bits	 = 0;
	iparms.flow_handle	 = &handle;
	iparms.batch_info	 = &parms->batch_info;

	rc = bnxt_ulp_cntxt_tsid_get(parms->ulp_ctx, &tsid);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to get the table scope\n");
		return rc;
	}

	rc = tfc_em_insert(tfcp, tsid, &iparms);

	if (likely(tfc_mpc_batch_started(&parms->batch_info))) {
		int trc;
		int em_index = parms->batch_info.count - 1;

		parms->batch_info.em_hdl[em_index] = *iparms.flow_handle;

		trc = ulp_mapper_tfc_mpc_batch_end(tfcp, &parms->batch_info);
		if (unlikely(trc))
			return trc;

		*iparms.flow_handle = parms->batch_info.em_hdl[em_index];

		/* Has there been an error? */
		if (unlikely(parms->batch_info.error)) {
			/* If there's not an EM error the entry will need to
			 * be deleted
			 */
			if (!parms->batch_info.em_error) {
				rc = parms->batch_info.error;
				goto error;
			}
		}

		rc = parms->batch_info.em_error;
	}

	if (unlikely(rc)) {
		/* Set the error flag in reg file */
		if (tbl->tbl_opcode == BNXT_ULP_EM_TBL_OPC_WR_REGFILE) {
			uint64_t val = 0;

			/* hash collision */
			if (unlikely(rc == -E2BIG))
				BNXT_DRV_DBG(DEBUG, "Duplicate EM entry\n");

			/* over max flows */
			if (rc == -ENOMEM) {
				val = 1;
				rc = 0;
				BNXT_DRV_DBG(DEBUG,
					     "Fail to insert EM, shall add to wc\n");
			}
			ulp_regfile_write(parms->regfile, tbl->tbl_operand,
					  rte_cpu_to_be_64(val));
		}

		if (rc && rc != -E2BIG)
			BNXT_DRV_DBG(ERR,
				     "Failed to insert em entry rc=%d.\n", rc);

		if (rc && error != NULL && tfc_is_mpc_error(rc))
			rte_flow_error_set((struct rte_flow_error *)error, EIO,
					   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					   mpc_error_str[rc * -1]);
		return rc;
	}

#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG_MAPPER
	ulp_mapper_tfc_em_dump("EM", &key, &iparms);
#endif
#endif
	/* Mark action process */
	rc = ulp_mapper_mark_gfid_process(parms, tbl, *iparms.flow_handle);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to add mark to flow\n");
		goto error;
	}

	/* Link the EM resource to the flow in the flow db */
	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.direction = tbl->direction;
	fid_parms.resource_func = tbl->resource_func;
	fid_parms.resource_type	= tbl->resource_type;
	fid_parms.critical_resource = tbl->critical_resource;
	fid_parms.resource_hndl = *iparms.flow_handle;

	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Fail to link res to flow rc = %d\n", rc);
		/* Need to free the identifier, so goto error */
		goto error;
	}

	return 0;
error:
	free_parms.dir = iparms.dir;
	free_parms.flow_handle = *iparms.flow_handle;
	free_parms.batch_info = &parms->batch_info;

	trc = tfc_em_delete(tfcp, &free_parms);
	if (trc)
		BNXT_DRV_DBG(ERR, "Failed to delete EM entry on failed add\n");

	return rc;
}

static int32_t
ulp_mapper_tfc_em_entry_free(struct bnxt_ulp_context *ulp,
			     struct ulp_flow_db_res_params *res,
			     void *error)
{
	struct tfc_em_delete_parms free_parms = { 0 };
	struct tfc *tfcp;
	int32_t rc = 0;
	uint16_t fw_fid = 0;
	struct tfc_mpc_batch_info_t batch_info;

	memset(&batch_info, 0, sizeof(batch_info));

	if (unlikely(bnxt_ulp_cntxt_fid_get(ulp, &fw_fid))) {
		BNXT_DRV_DBG(ERR, "Failed to get func_id\n");
		return -EINVAL;
	}

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp);
	if (unlikely(tfcp == NULL)) {
		BNXT_DRV_DBG(ERR, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}

	free_parms.dir = (enum cfa_dir)res->direction;
	free_parms.flow_handle = res->resource_hndl;
	free_parms.batch_info = &batch_info;

	rc = tfc_em_delete(tfcp, &free_parms);
	if (rc) {
		BNXT_DRV_DBG(ERR,
			     "Failed to delete EM entry, res = 0x%" PRIx64 "\n",
			     res->resource_hndl);
		if (error != NULL && tfc_is_mpc_error(rc)) {
			struct rte_flow_error *fe = (struct rte_flow_error *)error;
			rte_flow_error_set(fe,
					   EIO,
					   RTE_FLOW_ERROR_TYPE_HANDLE,
					   NULL,
					   mpc_error_str[rc * -1]);
		}
	} else {
		BNXT_DRV_DBG(DEBUG, "Deleted EM entry, res = 0x%" PRIx64 "\n",
			     res->resource_hndl);
	}

	return rc;
}

static uint16_t
ulp_mapper_tfc_dyn_blob_size_get(struct bnxt_ulp_mapper_parms *mparms,
				 struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct bnxt_ulp_device_params *d_params = mparms->device_params;

	if (d_params->dynamic_sram_en) {
		switch (tbl->resource_type) {
		/* TBD: add more types here */
		case CFA_BLD_ACT_OBJ_TYPE_STAT:
		case CFA_BLD_ACT_OBJ_TYPE_ENCAP:
		case CFA_BLD_ACT_OBJ_TYPE_MODIFY:
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
ulp_mapper_tfc_index_tbl_process(struct bnxt_ulp_mapper_parms *parms,
				 struct bnxt_ulp_mapper_tbl_info *tbl)
{
	bool alloc = false, write = false, global = false, regfile = false;
	struct bnxt_ulp_glb_resource_info glb_res = { 0 };
	uint16_t bit_size, wordlen = 0, tmplen = 0;
	enum cfa_track_type tt = tbl->track_type;
	struct ulp_flow_db_res_params fid_parms;
	struct tfc_idx_tbl_info tbl_info = { 0 };
	struct tfc *tfcp = NULL;
	struct ulp_blob	data;
	uint64_t regval = 0;
	bool shared = false;
	uint32_t index = 0;
	unsigned char *data_p;
	int32_t rc = 0;
	uint16_t fw_fid = 0;

	tfcp = bnxt_ulp_cntxt_tfcp_get(parms->ulp_ctx);
	if (unlikely(tfcp == NULL)) {
		PMD_DRV_LOG_LINE(ERR, "Failed to get tfcp pointer");
		return -EINVAL;
	}

	if (unlikely(bnxt_ulp_cntxt_fid_get(parms->ulp_ctx, &fw_fid))) {
		BNXT_DRV_DBG(ERR, "Failed to get func id\n");
		return -EINVAL;
	}

	/* compute the blob size */
	bit_size = ulp_mapper_tfc_dyn_blob_size_get(parms, tbl);

	/* Initialize the blob data */
	if (unlikely(ulp_blob_init(&data, bit_size,
				   parms->device_params->result_byte_order))) {
		BNXT_DRV_DBG(ERR, "Failed to initialize index table blob\n");
		return -EINVAL;
	}

	switch (tbl->tbl_opcode) {
	case BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE:
		alloc = true;
		regfile = true;
		break;
	case BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE:
		/*
		 * Build the entry, alloc an index, write the table, and store
		 * the data in the regfile.
		 */
		alloc = true;
		write = true;
		regfile = true;
		break;
	case BNXT_ULP_INDEX_TBL_OPC_WR_REGFILE:
		/*
		 * get the index to write to from the regfile and then write
		 * the table entry.
		 */
		regfile = true;
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
		break;
	case BNXT_ULP_INDEX_TBL_OPC_WR_GLB_REGFILE:
		if (unlikely(tbl->fdb_opcode != BNXT_ULP_FDB_OPC_NOP)) {
			BNXT_DRV_DBG(ERR, "Template error, wrong fdb opcode\n");
			return -EINVAL;
		}
		/*
		 * get the index to write to from the global regfile and then
		 * write the table.
		 */
		if (unlikely(ulp_mapper_glb_resource_read(parms->mapper_data,
							  tbl->direction,
							  tbl->tbl_operand,
							  &regval, &shared))) {
			BNXT_DRV_DBG(ERR,
				     "Failed to get tbl idx from Glb RF[%d].\n",
				     tbl->tbl_operand);
			return -EINVAL;
		}
		index = rte_be_to_cpu_64(regval);
		/* check to see if any scope id changes needs to be done*/
		write = true;
		break;
	case BNXT_ULP_INDEX_TBL_OPC_RD_REGFILE:
		/*
		 * The read is different from the rest and can be handled here
		 * instead of trying to use common code.  Simply read the table
		 * with the index from the regfile, scan and store the
		 * identifiers, and return.
		 */
		if (unlikely(ulp_regfile_read(parms->regfile,
					      tbl->tbl_operand, &regval))) {
			BNXT_DRV_DBG(ERR,
				     "Failed to get tbl idx from regfile[%d]\n",
				     tbl->tbl_operand);
			return -EINVAL;
		}
		index = rte_be_to_cpu_64(regval);
		tbl_info.dir = tbl->direction;
		tbl_info.rsubtype = tbl->resource_type;
		tbl_info.blktype = CFA_RESTYPE_TO_BLKT(tbl->resource_type);
		tbl_info.id = index;
		/* Nothing has been pushed to blob, so push bit_size */
		tmplen = ulp_blob_pad_push(&data, bit_size);
		data_p = ulp_blob_data_get(&data, &tmplen);
		wordlen = ULP_BITS_2_BYTE(tmplen);

		rc = tfc_idx_tbl_get(tfcp, fw_fid, &tbl_info, (uint32_t *)data_p,
				     (uint8_t *)&wordlen);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR,
				     "Failed to read the tbl entry %d:%d\n",
				     tbl->resource_type, index);
			return rc;
		}

		/* Scan the fields in the entry and push them into the regfile*/
		rc = ulp_mapper_tbl_ident_scan_ext(parms, tbl, data_p,
						   wordlen, data.byte_order);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR,
				     "Failed to get flds on tbl read rc=%d\n",
				     rc);
			return rc;
		}
		return 0;
	case BNXT_ULP_INDEX_TBL_OPC_NOP_REGFILE:
		/* Special case, where hw table processing is not being done */
		/* but only for writing the regfile into the flow database */
		regfile = true;
		break;
	default:
		BNXT_DRV_DBG(ERR, "Invalid index table opcode %d\n",
			     tbl->tbl_opcode);
		return -EINVAL;
	}

	/* read the CMM identifier from the regfile, it is not allocated */
	if (!alloc && regfile) {
		if (unlikely(ulp_regfile_read(parms->regfile,
					      tbl->tbl_operand,
					      &regval))) {
			BNXT_DRV_DBG(ERR,
				    "Failed to get tbl idx from regfile[%d].\n",
				     tbl->tbl_operand);
			return -EINVAL;
		}
		index = rte_be_to_cpu_64(regval);
	}

	/* Allocate the Action CMM identifier */
	if (alloc) {
		tbl_info.dir = tbl->direction;
		tbl_info.rsubtype = tbl->resource_type;
		tbl_info.blktype = CFA_RESTYPE_TO_BLKT(tbl->resource_type);
		/*
		 * Read back the operand and pass it into the
		 * alloc command if its a Dyn UPAR table.
		 */
		if (tbl_info.blktype == CFA_IDX_TBL_BLKTYPE_RXP) {
			if (ulp_regfile_read(parms->regfile,
					     tbl->tbl_operand, &regval)) {
				BNXT_DRV_DBG(ERR,
					     "Failed to get tbl idx from regfile[%d]\n",
					     tbl->tbl_operand);
				return -EINVAL;
			}
			tbl_info.rsubtype = rte_be_to_cpu_64(regval);
		}

		rc =  tfc_idx_tbl_alloc(tfcp, fw_fid, tt, &tbl_info);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Alloc table[%s][%s] failed rc=%d\n",
				     tfc_idx_tbl_2_str(tbl_info.rsubtype),
				     tfc_dir_2_str(tbl->direction), rc);
			return rc;
		}
		index = tbl_info.id;
	}

	/* update the global register value */
	if (alloc && global) {
		glb_res.direction = tbl->direction;
		glb_res.resource_func = tbl->resource_func;
		glb_res.resource_type = tbl->resource_type;
		glb_res.glb_regfile_index = tbl->tbl_operand;
		regval = rte_cpu_to_be_64(index);

		/*
		 * Shared resources are never allocated through this
		 * method, so the shared flag is always false.
		 */
		rc = ulp_mapper_glb_resource_write(parms->mapper_data,
						   &glb_res, regval,
						   false);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR,
				     "Failed to write %s regfile[%d] rc=%d\n",
				     (global) ? "global" : "reg",
				     tbl->tbl_operand, rc);
			goto error;
		}
	}

	/* update the local register value */
	if (alloc && regfile) {
		regval = rte_cpu_to_be_64(index);
		rc = ulp_regfile_write(parms->regfile,
				       tbl->tbl_operand, regval);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR,
				     "Failed to write %s regfile[%d] rc=%d\n",
				     (global) ? "global" : "reg",
				     tbl->tbl_operand, rc);
			goto error;
		}
	}

	if (write) {
		/* Get the result fields list */
		rc = ulp_mapper_tbl_result_build(parms,
						 tbl,
						 &data,
						 "Indexed Result");
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Failed to build the result blob\n");
			return rc;
		}
		data_p = ulp_blob_data_get(&data, &tmplen);
		tbl_info.dir = tbl->direction;
		tbl_info.rsubtype = tbl->resource_type;
		tbl_info.blktype = CFA_RESTYPE_TO_BLKT(tbl->resource_type);
		tbl_info.id = index;
		wordlen = ULP_BITS_2_BYTE(tmplen);
		rc = tfc_idx_tbl_set(tfcp, fw_fid, &tbl_info,
				     (uint32_t *)data_p, wordlen);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR,
				     "Index table[%s][%s][%x] write fail %d\n",
				     tfc_idx_tbl_2_str(tbl_info.rsubtype),
				     tfc_dir_2_str(tbl_info.dir),
				     tbl_info.id, rc);
			goto error;
		}
		BNXT_DRV_DBG(DEBUG,
			     "Index table[%s][%d][%x] write successful\n",
			     tfc_idx_tbl_2_str(tbl_info.rsubtype),
			     tbl_info.dir, tbl_info.id);
	}
	/* Link the resource to the flow in the flow db */
	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.direction	= tbl->direction;
	fid_parms.resource_func	= tbl->resource_func;
	fid_parms.resource_type	= tbl->resource_type;
	fid_parms.resource_sub_type = tbl->resource_sub_type;
	fid_parms.resource_hndl	= index;
	fid_parms.critical_resource = tbl->critical_resource;
	ulp_flow_db_shared_session_set(&fid_parms, tbl->session_type);

	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to link resource to flow rc = %d\n",
			     rc);
		goto error;
	}

	/* Perform the VF rep action */
	rc = ulp_mapper_mark_vfr_idx_process(parms, tbl);
	if (unlikely(rc)) {
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

	tbl_info.blktype = CFA_RESTYPE_TO_BLKT(tbl->resource_type);
	if (tfc_idx_tbl_free(tfcp, fw_fid, &tbl_info))
		BNXT_DRV_DBG(ERR, "Failed to free index entry on failure\n");
	return rc;
}

static inline int32_t
ulp_mapper_tfc_index_entry_free(struct bnxt_ulp_context *ulp_ctx,
				struct ulp_flow_db_res_params *res)
{
	struct tfc *tfcp = NULL;
	struct tfc_idx_tbl_info tbl_info = { 0 };
	uint16_t fw_fid = 0;
	int32_t rc;

	if (unlikely(bnxt_ulp_cntxt_fid_get(ulp_ctx, &fw_fid))) {
		BNXT_DRV_DBG(ERR, "Failed to get func_id\n");
		return -EINVAL;
	}

#ifndef ULP_MAPPER_TFC_TEST
	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp_ctx);
	if (unlikely(tfcp == NULL)) {
		BNXT_DRV_DBG(ERR, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}
#endif
	tbl_info.dir = (enum cfa_dir)res->direction;
	tbl_info.rsubtype = res->resource_type;
	tbl_info.id = (uint16_t)res->resource_hndl;
	tbl_info.blktype = CFA_RESTYPE_TO_BLKT(res->resource_type);

	/* TBD: check to see if the memory needs to be cleaned as well*/
	rc = tfc_idx_tbl_free(tfcp, fw_fid, &tbl_info);
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG_MAPPER
	if (!rc)
		BNXT_DRV_DBG(DEBUG, "Freed Index [%s]:[%s] = 0x%X\n",
		     tfc_dir_2_str(tbl_info.dir),
		     tfc_idx_tbl_2_str(tbl_info.rsubtype), tbl_info.id);
#endif
#endif
	return rc;
}

static int32_t
ulp_mapper_tfc_cmm_tbl_process(struct bnxt_ulp_mapper_parms *parms,
			       struct bnxt_ulp_mapper_tbl_info *tbl,
			       void *error)
{
	bool alloc = false, write = false, global = false, regfile = false;
	struct bnxt_ulp_glb_resource_info glb_res = { 0 };
	uint16_t bit_size, act_wordlen = 0, tmplen = 0;
	struct ulp_flow_db_res_params fid_parms;
	struct tfc_cmm_info cmm_info = { 0 };
	struct tfc *tfcp = NULL;
	struct ulp_blob	data;
	uint64_t regval = 0;
	bool shared = false;
	uint64_t handle = 0;
	const uint8_t *act_data;
	uint64_t act_rec_size = 0;
	uint8_t tsid = 0;
	int32_t rc = 0;

	tfcp = bnxt_ulp_cntxt_tfcp_get(parms->ulp_ctx);
	if (unlikely(tfcp == NULL)) {
		PMD_DRV_LOG_LINE(ERR, "Failed to get tfcp pointer");
		return -EINVAL;
	}

	/* compute the blob size */
	bit_size = ulp_mapper_tfc_dyn_blob_size_get(parms, tbl);

	/* Initialize the blob data */
	if (unlikely(ulp_blob_init(&data, bit_size,
				   parms->device_params->result_byte_order))) {
		BNXT_DRV_DBG(ERR, "Failed to initialize cmm table blob\n");
		return -EINVAL;
	}

	switch (tbl->tbl_opcode) {
	case BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE:
		regfile = true;
		alloc = true;
		break;
	case BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE:
		/*
		 * Build the entry, alloc an index, write the table, and store
		 * the data in the regfile.
		 */
		alloc = true;
		write = true;
		regfile = true;
		break;
	case BNXT_ULP_INDEX_TBL_OPC_WR_REGFILE:
		/*
		 * get the index to write to from the regfile and then write
		 * the table entry.
		 */
		regfile = true;
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
		break;
	case BNXT_ULP_INDEX_TBL_OPC_WR_GLB_REGFILE:
		if (unlikely(tbl->fdb_opcode != BNXT_ULP_FDB_OPC_NOP)) {
			BNXT_DRV_DBG(ERR, "Template error, wrong fdb opcode\n");
			return -EINVAL;
		}
		/*
		 * get the index to write to from the global regfile and then
		 * write the table.
		 */
		if (unlikely(ulp_mapper_glb_resource_read(parms->mapper_data,
							  tbl->direction,
							  tbl->tbl_operand,
							  &regval, &shared))) {
			BNXT_DRV_DBG(ERR,
				     "Failed to get tbl idx from Glb RF[%d].\n",
				     tbl->tbl_operand);
			return -EINVAL;
		}
		handle = rte_be_to_cpu_64(regval);
		/* check to see if any scope id changes needs to be done*/
		write = true;
		break;
	case BNXT_ULP_INDEX_TBL_OPC_RD_REGFILE:
		/*
		 * The read is different from the rest and can be handled here
		 * instead of trying to use common code.  Simply read the table
		 * with the index from the regfile, scan and store the
		 * identifiers, and return.
		 */
		if (unlikely(ulp_regfile_read(parms->regfile,
					      tbl->tbl_operand, &regval))) {
			BNXT_DRV_DBG(ERR,
				     "Failed to get tbl idx from regfile[%d]\n",
				     tbl->tbl_operand);
			return -EINVAL;
		}
		handle = rte_be_to_cpu_64(regval);
		return 0;
	case BNXT_ULP_INDEX_TBL_OPC_NOP_REGFILE:
		regfile = true;
		alloc = false;
		break;
	default:
		BNXT_DRV_DBG(ERR, "Invalid cmm table opcode %d\n",
			     tbl->tbl_opcode);
		return -EINVAL;
	}

	/* read the CMM handle from the regfile, it is not allocated */
	if (!alloc && regfile) {
		if (unlikely(ulp_regfile_read(parms->regfile,
				     tbl->tbl_operand,
					      &regval))) {
			BNXT_DRV_DBG(ERR,
				    "Failed to get tbl idx from regfile[%d].\n",
				     tbl->tbl_operand);
			return -EINVAL;
		}
		handle = rte_be_to_cpu_64(regval);
	}

	/* Get the result fields list */
	rc = ulp_mapper_tbl_result_build(parms,
					 tbl,
					 &data,
					 "Indexed Result");
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to build the result blob\n");
		return rc;
	}

	/* Allocate the Action CMM identifier */
	if (alloc) {
		cmm_info.dir = tbl->direction;
		cmm_info.rsubtype = tbl->resource_type;
		/* Only need the length for alloc, ignore the returned data */
		act_data = ulp_blob_data_get(&data, &tmplen);
		act_wordlen = ULP_BITS_TO_32_BYTE_WORD(tmplen);

		rc = bnxt_ulp_cntxt_tsid_get(parms->ulp_ctx, &tsid);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Failed to get the table scope\n");
			return rc;
		}
		/* All failures after the alloc succeeds require a free */
		rc =  tfc_act_alloc(tfcp, tsid, &cmm_info, act_wordlen);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Alloc CMM [%d][%s] failed rc=%d\n",
				     cmm_info.rsubtype,
				     tfc_dir_2_str(cmm_info.dir), rc);
			return rc;
		}
		handle = cmm_info.act_handle;

		/* Counters need to be reset when allocated to ensure counter is
		 * zero
		 */
		if (tbl->resource_func == BNXT_ULP_RESOURCE_FUNC_CMM_STAT) {
			rc = tfc_act_set(tfcp,
					 &parms->batch_info,
					 &cmm_info, act_data,
					 act_wordlen);
			if (unlikely(rc)) {
				BNXT_DRV_DBG(ERR, "Stat alloc/clear[%d][%s]"
					     "[0x%" PRIx64 "] failed rc=%d\n",
					     cmm_info.rsubtype,
					     tfc_dir_2_str(cmm_info.dir),
					     cmm_info.act_handle, rc);
				if (error != NULL && tfc_is_mpc_error(rc)) {
					struct rte_flow_error *fe =
						(struct rte_flow_error *)error;
					rte_flow_error_set(fe,
							   EIO,
							   RTE_FLOW_ERROR_TYPE_HANDLE,
							   NULL,
							   mpc_error_str[rc * -1]);
				}
				goto error;
			}
		}
	}

	/* update the global register value */
	if (alloc && global) {
		glb_res.direction = tbl->direction;
		glb_res.resource_func = tbl->resource_func;
		glb_res.resource_type = tbl->resource_type;
		glb_res.glb_regfile_index = tbl->tbl_operand;
		regval = rte_cpu_to_be_64(handle);

		/*
		 * Shared resources are never allocated through this
		 * method, so the shared flag is always false.
		 */
		rc = ulp_mapper_glb_resource_write(parms->mapper_data,
						   &glb_res, regval,
						   false);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR,
				     "Failed to write %s regfile[%d] rc=%d\n",
				     (global) ? "global" : "reg",
				     tbl->tbl_operand, rc);
			goto error;
		}
	}

	/* update the local register value */
	if (alloc && regfile) {
		regval = rte_cpu_to_be_64(handle);
		rc = ulp_regfile_write(parms->regfile,
				       tbl->tbl_operand, regval);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR,
				     "Failed to write %s regfile[%d] rc=%d\n",
				     (global) ? "global" : "reg",
				     tbl->tbl_operand, rc);
			goto error;
		}
	}

	if (write) {
		act_data = ulp_blob_data_get(&data, &tmplen);
		cmm_info.dir = tbl->direction;
		cmm_info.rsubtype = tbl->resource_type;
		cmm_info.act_handle = handle;
		act_wordlen = ULP_BITS_TO_32_BYTE_WORD(tmplen);
		rc = tfc_act_set(tfcp, &parms->batch_info, &cmm_info, act_data, act_wordlen);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR,
				     "CMM table[%d][%s][0x%" PRIx64
				      "] write fail %d\n",
				     cmm_info.rsubtype,
				     tfc_dir_2_str(cmm_info.dir),
				     handle, rc);
			if (error != NULL && tfc_is_mpc_error(rc)) {
				struct rte_flow_error *fe =
					(struct rte_flow_error *)error;
				rte_flow_error_set(fe,
						   EIO,
						   RTE_FLOW_ERROR_TYPE_HANDLE,
						   NULL,
						   mpc_error_str[rc * -1]);
			}
			goto error;
		}
		BNXT_DRV_DBG(DEBUG,
			     "CMM table[%d][%s][0x%" PRIx64
			      "] write successful\n",
			     cmm_info.rsubtype, tfc_dir_2_str(cmm_info.dir),
			     handle);

		/* Calculate action record size */
		if (tbl->resource_type == CFA_RSUBTYPE_CMM_ACT) {
			act_rec_size = (ULP_BITS_2_BYTE_NR(tmplen) + 15) / 16;
			act_rec_size--;
			if (ulp_regfile_write(parms->regfile,
					      BNXT_ULP_RF_IDX_ACTION_REC_SIZE,
					      rte_cpu_to_be_64(act_rec_size)))
				BNXT_DRV_DBG(ERR,
					     "Failed write the act rec size\n");
		}
	}
	/* Link the resource to the flow in the flow db */
	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.direction	= tbl->direction;
	fid_parms.resource_func	= tbl->resource_func;
	fid_parms.resource_type	= tbl->resource_type;
	fid_parms.resource_sub_type = tbl->resource_sub_type;
	fid_parms.resource_hndl	= handle;
	fid_parms.critical_resource = tbl->critical_resource;
	ulp_flow_db_shared_session_set(&fid_parms, tbl->session_type);

	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to link resource to flow rc = %d\n",
			     rc);
		goto error;
	}

	/* Perform the VF rep action */
	rc = ulp_mapper_mark_vfr_idx_process(parms, tbl);
	if (unlikely(rc)) {
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

	if (tfc_act_free(tfcp, &cmm_info))
		BNXT_DRV_DBG(ERR, "Failed to free cmm entry on failure\n");

	return rc;
}

static int32_t
ulp_mapper_tfc_cmm_entry_free(struct bnxt_ulp_context *ulp_ctx,
			      struct ulp_flow_db_res_params *res,
			      void *error)
{
	struct tfc *tfcp = NULL;
	struct tfc_cmm_info cmm_info = { 0 };
	uint16_t fw_fid = 0;
	int32_t rc = 0;

	/* skip cmm processing if fdb flag is sw only */
	if (res->fdb_flags & ULP_FDB_FLAG_SW_ONLY)
		return 0;

	if (unlikely(bnxt_ulp_cntxt_fid_get(ulp_ctx, &fw_fid))) {
		BNXT_DRV_DBG(ERR, "Failed to get func_id\n");
		return -EINVAL;
	}

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp_ctx);
	if (unlikely(tfcp == NULL)) {
		BNXT_DRV_DBG(ERR, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}

	cmm_info.dir = (enum cfa_dir)res->direction;
	cmm_info.rsubtype = res->resource_type;
	cmm_info.act_handle = res->resource_hndl;

	/* TBD: check to see if the memory needs to be cleaned as well*/
	rc = tfc_act_free(tfcp, &cmm_info);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR,
			     "Failed to delete CMM entry,res = 0x%" PRIx64 "\n",
			     res->resource_hndl);
		if (error != NULL && tfc_is_mpc_error(rc)) {
			struct rte_flow_error *fe = (struct rte_flow_error *)error;
			rte_flow_error_set(fe,
					   EIO,
					   RTE_FLOW_ERROR_TYPE_HANDLE,
					   NULL,
					   mpc_error_str[rc * -1]);
		}
	} else {
		BNXT_DRV_DBG(DEBUG, "Deleted CMM entry,res = 0x%" PRIx64 "\n",
			     res->resource_hndl);
	}
	return rc;
}

static int32_t
ulp_mapper_tfc_if_tbl_process(struct bnxt_ulp_mapper_parms *parms,
			      struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct ulp_blob	data, res_blob;
	uint64_t idx = 0;
	int32_t rc = 0;
	struct tfc *tfcp;
	enum bnxt_ulp_if_tbl_opc if_opc = tbl->tbl_opcode;
	uint32_t res_size;
	struct tfc_if_tbl_info tbl_info = { 0 };
	unsigned char *data_p;
	uint16_t fw_fid = 0;
	uint8_t data_size;
	uint16_t tmplen;

	if (unlikely(bnxt_ulp_cntxt_fid_get(parms->ulp_ctx, &fw_fid))) {
		BNXT_DRV_DBG(ERR, "Failed to get func_id\n");
		return -EINVAL;
	}

	tfcp = bnxt_ulp_cntxt_tfcp_get(parms->ulp_ctx);
	if (unlikely(tfcp == NULL)) {
		PMD_DRV_LOG_LINE(ERR, "Failed to get tfcp pointer");
		return -EINVAL;
	}

	/* Initialize the blob data */
	if (unlikely(ulp_blob_init(&data, tbl->result_bit_size,
				   parms->device_params->result_byte_order))) {
		BNXT_DRV_DBG(ERR, "Failed initial index table blob\n");
		return -EINVAL;
	}

	/* create the result blob */
	rc = ulp_mapper_tbl_result_build(parms, tbl, &data, "IFtable Result");
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to build the result blob\n");
		return rc;
	}

	/* Get the index details */
	switch (if_opc) {
	case BNXT_ULP_IF_TBL_OPC_WR_COMP_FIELD:
		idx = ULP_COMP_FLD_IDX_RD(parms, tbl->tbl_operand);
		break;
	case BNXT_ULP_IF_TBL_OPC_WR_REGFILE:
		if (unlikely(ulp_regfile_read(parms->regfile, tbl->tbl_operand, &idx))) {
			BNXT_DRV_DBG(ERR, "regfile[%d] read oob\n",
				     tbl->tbl_operand);
			return -EINVAL;
		}
		idx = rte_be_to_cpu_64(idx);
		break;
	case BNXT_ULP_IF_TBL_OPC_WR_CONST:
		idx = tbl->tbl_operand;
		break;
	case BNXT_ULP_IF_TBL_OPC_RD_COMP_FIELD:
		/* Initialize the result blob */
		if (unlikely(ulp_blob_init(&res_blob, tbl->result_bit_size,
					   parms->device_params->result_byte_order))) {
			BNXT_DRV_DBG(ERR, "Failed initial result blob\n");
			return -EINVAL;
		}

		/* read the interface table */
		idx = ULP_COMP_FLD_IDX_RD(parms, tbl->tbl_operand);
		res_size = ULP_BITS_2_BYTE(tbl->result_bit_size);
		rc = ulp_mapper_tbl_ident_scan_ext(parms, tbl,
						   res_blob.data,
						   res_size,
						   res_blob.byte_order);
		if (unlikely(rc))
			BNXT_DRV_DBG(ERR, "Scan and extract failed rc=%d\n",
				     rc);
		return rc;
	case BNXT_ULP_IF_TBL_OPC_NOT_USED:
		return rc; /* skip it */
	default:
		BNXT_DRV_DBG(ERR, "Invalid tbl index opcode\n");
		return -EINVAL;
	}

	tbl_info.dir = tbl->direction;
	tbl_info.rsubtype = tbl->resource_type;
	tbl_info.id = (uint32_t)idx;
	data_p = ulp_blob_data_get(&data, &tmplen);
	data_size = ULP_BITS_2_BYTE(tmplen);

	rc = tfc_if_tbl_set(tfcp, fw_fid, &tbl_info, (uint8_t *)data_p,
			    data_size);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR,
			     "Failed to write the if tbl entry %d:%d\n",
			     tbl->resource_type, (uint32_t)idx);
		return rc;
	}

	return rc;
}

static int32_t
ulp_mapper_tfc_ident_alloc(struct bnxt_ulp_context *ulp_ctx,
			   uint32_t session_type __rte_unused,
			   uint16_t ident_type,
			   uint8_t direction,
			   enum cfa_track_type tt,
			   uint64_t *identifier_id)
{
	struct tfc *tfcp = NULL;
	struct tfc_identifier_info ident_info = { 0 };
	int32_t rc = 0;
	uint16_t fw_fid = 0;

	if (unlikely(bnxt_ulp_cntxt_fid_get(ulp_ctx, &fw_fid))) {
		BNXT_DRV_DBG(ERR, "Failed to get func_id\n");
		return -EINVAL;
	}

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp_ctx);
	if (unlikely(tfcp == NULL)) {
		BNXT_DRV_DBG(ERR, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}

	ident_info.dir = direction;
	ident_info.rsubtype = ident_type;

	rc = tfc_identifier_alloc(tfcp, fw_fid, tt, &ident_info);
	if (unlikely(rc != 0)) {
		BNXT_DRV_DBG(ERR, "alloc failed %d\n", rc);
		return rc;
	}
	*identifier_id = ident_info.id;
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG_MAPPER
	BNXT_DRV_INF("Allocated Identifier [%s]:[%s] = 0x%X\n",
		     tfc_dir_2_str(direction),
		     tfc_ident_2_str(ident_info.rsubtype), ident_info.id);
#endif
#endif

	return rc;
}

static int32_t
ulp_mapper_tfc_ident_free(struct bnxt_ulp_context *ulp_ctx,
			  struct ulp_flow_db_res_params *res)
{
	struct tfc *tfcp = NULL;
	struct tfc_identifier_info ident_info = { 0 };
	int32_t rc = 0;
	uint16_t fw_fid = 0;

	if (unlikely(bnxt_ulp_cntxt_fid_get(ulp_ctx, &fw_fid))) {
		BNXT_DRV_DBG(ERR, "Failed to get func_id\n");
		return -EINVAL;
	}

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp_ctx);
	if (unlikely(tfcp == NULL)) {
		BNXT_DRV_DBG(ERR, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}

	ident_info.dir = (enum cfa_dir)res->direction;
	ident_info.rsubtype = res->resource_type;
	ident_info.id = res->resource_hndl;

	rc = tfc_identifier_free(tfcp, fw_fid, &ident_info);
	if (unlikely(rc != 0)) {
		BNXT_DRV_DBG(ERR, "free failed %d\n", rc);
		return rc;
	}
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG_MAPPER
	BNXT_DRV_DBG(DEBUG, "Freed Identifier [%s]:[%s] = 0x%X\n",
		     tfc_dir_2_str(ident_info.dir),
		     tfc_ident_2_str(ident_info.rsubtype), ident_info.id);
#endif
#endif

	return rc;
}

static int32_t
ulp_mapper_tfc_global_ident_alloc(struct bnxt_ulp_context *ulp_ctx,
				  uint16_t ident_type,
				  uint8_t direction,
				  uint8_t *context_id,
				  uint16_t context_len,
				  uint64_t *identifier_id)
{
	struct tfc *tfcp = NULL;
	struct tfc_global_id_req glb_req = { 0 };
	struct tfc_global_id glb_rsp = { 0 };
	uint16_t fw_fid = 0;
	int32_t rc = 0;
	bool first = false;

	if (unlikely(bnxt_ulp_cntxt_fid_get(ulp_ctx, &fw_fid))) {
		BNXT_DRV_DBG(ERR, "Failed to get func_id\n");
		return -EINVAL;
	}

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp_ctx);
	if (unlikely(tfcp == NULL)) {
		BNXT_DRV_DBG(ERR, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}

	glb_req.rtype = CFA_RTYPE_IDENT;
	glb_req.dir = direction;
	glb_req.rsubtype = ident_type;
	glb_req.context_len = context_len;
	glb_req.context_id = context_id;

	rc = tfc_global_id_alloc(tfcp, fw_fid, &glb_req, &glb_rsp, &first);
	if (unlikely(rc != 0)) {
		BNXT_DRV_DBG(ERR, "alloc failed %d\n", rc);
		return rc;
	}
	*identifier_id = glb_rsp.id;

	return rc;
}

static int32_t
ulp_mapper_tfc_global_ident_free(struct bnxt_ulp_context *ulp_ctx,
				 struct ulp_flow_db_res_params *res)
{
	struct tfc_global_id_req glb_req = { 0 };
	struct tfc *tfcp = NULL;
	int32_t rc = 0;
	uint16_t fw_fid = 0;

	if (unlikely(bnxt_ulp_cntxt_fid_get(ulp_ctx, &fw_fid))) {
		BNXT_DRV_DBG(ERR, "Failed to get func_id\n");
		return -EINVAL;
	}

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp_ctx);
	if (unlikely(tfcp == NULL)) {
		BNXT_DRV_DBG(ERR, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}

	glb_req.rtype = CFA_RTYPE_IDENT;
	glb_req.dir = (enum cfa_dir)res->direction;
	glb_req.rsubtype = res->resource_type;
	glb_req.resource_id = (uint16_t)res->resource_hndl;

	rc = tfc_global_id_free(tfcp, fw_fid, &glb_req);
	if (unlikely(rc != 0)) {
		BNXT_DRV_DBG(ERR, "free failed %d\n", rc);
		return rc;
	}

	return rc;
}

static inline int32_t
ulp_mapper_tfc_tcam_entry_free(struct bnxt_ulp_context *ulp,
			       struct ulp_flow_db_res_params *res)
{
	struct tfc *tfcp = NULL;
	struct tfc_tcam_info tcam_info = { 0 };
	uint16_t fw_fid = 0;

	if (unlikely(bnxt_ulp_cntxt_fid_get(ulp, &fw_fid))) {
		BNXT_DRV_DBG(ERR, "Failed to get func_id\n");
		return -EINVAL;
	}

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp);
	if (unlikely(tfcp == NULL)) {
		PMD_DRV_LOG_LINE(ERR, "Failed to get tfcp pointer");
		return -EINVAL;
	}
	tcam_info.dir = (enum cfa_dir)res->direction;
	tcam_info.rsubtype = res->resource_type;
	tcam_info.id = (uint16_t)res->resource_hndl;

	if (unlikely(!tfcp || tfc_tcam_free(tfcp, fw_fid, &tcam_info))) {
		BNXT_DRV_DBG(ERR, "Unable to free tcam resource %u\n",
			    tcam_info.id);
		return -EINVAL;
	}
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG_MAPPER
	BNXT_DRV_DBG(DEBUG, "Freed TCAM [%s]:[%s] = 0x%X\n",
		     tfc_dir_2_str(tcam_info.dir),
		     tfc_tcam_2_str(tcam_info.rsubtype), tcam_info.id);
#endif
#endif
	return 0;
}

static uint32_t
ulp_mapper_tfc_dyn_tbl_type_get(struct bnxt_ulp_mapper_parms *parm __rte_unused,
				struct bnxt_ulp_mapper_tbl_info *tbl,
				uint16_t blob_len,
				uint16_t *out_len)
{
	switch (tbl->resource_type) {
	case CFA_RSUBTYPE_CMM_ACT:
		*out_len = ULP_BITS_TO_32_BYTE_WORD(blob_len);
		*out_len = *out_len * 256;
		break;
	default:
		BNXT_DRV_DBG(ERR, "Not a dynamic table %d\n", tbl->resource_type);
		*out_len = blob_len;
		break;
	}

	return tbl->resource_type;
}

static int32_t
ulp_mapper_tfc_index_tbl_alloc_process(struct bnxt_ulp_context *ulp,
				       uint32_t session_type __rte_unused,
				       uint16_t table_type,
				       uint8_t direction,
				       uint64_t *index)
{
	enum cfa_track_type tt = CFA_TRACK_TYPE_SID;
	struct tfc_idx_tbl_info tbl_info = { 0 };
	struct tfc *tfcp = NULL;
	uint16_t fw_fid = 0;
	int32_t rc = 0;

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp);
	if (unlikely(tfcp == NULL)) {
		BNXT_DRV_DBG(ERR, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}

	if (unlikely(bnxt_ulp_cntxt_fid_get(ulp, &fw_fid))) {
		BNXT_DRV_DBG(ERR, "Failed to get func id\n");
		return -EINVAL;
	}

	tbl_info.rsubtype = table_type;
	tbl_info.dir = direction;
	rc = tfc_idx_tbl_alloc(tfcp, fw_fid, tt, &tbl_info);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Alloc table[%s][%s] failed rc=%d\n",
			     tfc_idx_tbl_2_str(tbl_info.rsubtype),
			     tfc_dir_2_str(direction), rc);
		return rc;
	}

	*index = tbl_info.id;
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG_MAPPER
	BNXT_DRV_DBG(DEBUG, "Allocated Table Index [%s][%s] = 0x%04x\n",
		     tfc_idx_tbl_2_str(table_type), tfc_dir_2_str(direction),
		     tbl_info.id);
#endif
#endif
	return rc;
}

static int32_t
ulp_mapper_tfc_app_glb_resource_info_init(struct bnxt_ulp_context
					  *ulp_ctx __rte_unused,
					  struct bnxt_ulp_mapper_data
					  *mapper_data __rte_unused)
{
	/* Not supported Shared Apps yet on TFC API */
	return 0;
}

static int32_t
ulp_mapper_tfc_handle_to_offset(struct bnxt_ulp_mapper_parms *parms __rte_unused,
				uint64_t handle,
				uint32_t offset,
				uint64_t *result)
{
	uint32_t val = 0;
	int32_t rc = 0;

	TFC_GET_32B_OFFSET_ACT_HANDLE(val, &handle);

	switch (offset) {
	case 0:
		val = val << 5;
		break;
	case 4:
		val = val << 3;
		break;
	case 8:
		val = val << 2;
		break;
	case 16:
		val = val << 1;
		break;
	case 32:
		break;
	default:
		return -EINVAL;
	}

	*result = val;
	return rc;
}

static int
ulp_mapper_tfc_mpc_batch_start(struct tfc_mpc_batch_info_t *batch_info)
{
	return tfc_mpc_batch_start(batch_info);
}

static int32_t
ulp_mapper_tfc_mtr_stats_hndl_set(struct bnxt_ulp_mapper_parms *parms __rte_unused,
				  uint32_t mtr_id, uint64_t stats_hndl)
{
	int32_t i, rc = -ENOMEM;

	for (i = 0; i < BNXT_METER_MAX_NUM; i++)
		if (!mtr_stats[i].valid) {
			mtr_stats[i].mtr_id = mtr_id;
			mtr_stats[i].stats_hndl = stats_hndl;
			mtr_stats[i].valid = true;
			rc = 0;
			break;
		}

	return rc;
}

static int32_t
ulp_mapper_tfc_mtr_stats_hndl_get(uint32_t mtr_id, uint64_t *stats_hndl)
{
	int32_t i, rc = -EINVAL;

	for (i = 0; i < BNXT_METER_MAX_NUM; i++) {
		if (mtr_stats[i].valid && mtr_stats[i].mtr_id == mtr_id) {
			*stats_hndl = mtr_stats[i].stats_hndl;
			rc = 0;
			break;
		}
	}

	return rc;
}

static int32_t
ulp_mapper_tfc_mtr_stats_hndl_del(uint32_t mtr_id)
{
	int32_t i, rc = -EINVAL;

	for (i = 0; i < BNXT_METER_MAX_NUM; i++)
		if (mtr_stats[i].valid && mtr_stats[i].mtr_id == mtr_id) {
			mtr_stats[i].valid = false;
			rc = 0;
			break;
		}

	return rc;
}

static int32_t
ulp_mapper_tfc_glb_idx_tbl_alloc(struct bnxt_ulp_context *ulp_ctx,
				 uint16_t sub_type, uint8_t direction,
				 uint8_t *context_id, uint16_t context_len,
				 uint64_t *idx_id)
{
	struct tfc *tfcp = NULL;
	struct tfc_global_id_req glb_req = { 0 };
	struct tfc_global_id glb_rsp = { 0 };
	uint16_t fw_fid = 0;
	int32_t rc = 0;
	bool first = false;

	if (unlikely(bnxt_ulp_cntxt_fid_get(ulp_ctx, &fw_fid))) {
		BNXT_DRV_DBG(ERR, "Failed to get func_id");
		return -EINVAL;
	}

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp_ctx);
	if (unlikely(tfcp == NULL)) {
		BNXT_DRV_DBG(ERR, "Failed to get tfcp pointer");
		return -EINVAL;
	}

	glb_req.rtype = CFA_RTYPE_IDX_TBL;
	glb_req.dir = direction;
	glb_req.rsubtype = sub_type;
	glb_req.context_len = context_len;
	glb_req.context_id = context_id;

	rc = tfc_global_id_alloc(tfcp, fw_fid, &glb_req, &glb_rsp, &first);
	if (unlikely(rc != 0)) {
		BNXT_DRV_DBG(ERR, "alloc failed %d", rc);
		return rc;
	}
	*idx_id = glb_rsp.id;

	return rc;
}

static int32_t
ulp_mapper_tfc_glb_idx_tbl_free(struct bnxt_ulp_context *ulp_ctx,
				struct ulp_flow_db_res_params *res)
{
	struct tfc_global_id_req glb_req = { 0 };
	struct tfc *tfcp = NULL;
	int32_t rc = 0;
	uint16_t fw_fid = 0;

	if (unlikely(bnxt_ulp_cntxt_fid_get(ulp_ctx, &fw_fid))) {
		BNXT_DRV_DBG(ERR, "Failed to get func_id");
		return -EINVAL;
	}

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp_ctx);
	if (unlikely(tfcp == NULL)) {
		BNXT_DRV_DBG(ERR, "Failed to get tfcp pointer");
		return -EINVAL;
	}

	glb_req.rtype = CFA_RTYPE_IDX_TBL;
	glb_req.dir = (enum cfa_dir)res->direction;
	glb_req.rsubtype = res->resource_type;
	glb_req.resource_id = (uint16_t)res->resource_hndl;

	rc = tfc_global_id_free(tfcp, fw_fid, &glb_req);
	if (unlikely(rc != 0)) {
		BNXT_DRV_DBG(ERR, "free failed %d", rc);
		return rc;
	}

	return rc;
}

static inline int32_t
ulp_mapper_tfc_tcam_prio_update(struct bnxt_ulp_mapper_parms *parms,
				uint8_t dir,
				enum cfa_track_type tt,
				enum cfa_resource_subtype_tcam rtype,
				uint32_t tcam_id,
				uint16_t priority)
{
	struct tfc *tfcp = NULL;
	struct tfc_tcam_info tcam_info = { 0 };
	uint16_t fw_fid = 0;

	if (unlikely(bnxt_ulp_cntxt_fid_get(parms->ulp_ctx, &fw_fid))) {
		BNXT_DRV_DBG(ERR, "Failed to get func_id\n");
		return -EINVAL;
	}

	tfcp = bnxt_ulp_cntxt_tfcp_get(parms->ulp_ctx);
	if (unlikely(tfcp == NULL)) {
		PMD_DRV_LOG_LINE(ERR, "Failed to get tfcp pointer");
		return -EINVAL;
	}
	tcam_info.dir = (enum cfa_dir)dir;
	tcam_info.rsubtype = rtype;
	tcam_info.id = (uint16_t)tcam_id;

	if (unlikely(!tfcp || tfc_tcam_priority_update(tfcp, fw_fid, tt,
						       &tcam_info,
						       priority))) {
		BNXT_DRV_DBG(ERR, "Unable to update tcam priority %u\n",
			     tcam_info.id);
		return -EINVAL;
	}
	return 0;
}

const struct ulp_mapper_core_ops ulp_mapper_tfc_core_ops = {
	.ulp_mapper_core_tcam_tbl_process = ulp_mapper_tfc_tcam_tbl_process,
	.ulp_mapper_core_tcam_entry_free = ulp_mapper_tfc_tcam_entry_free,
	.ulp_mapper_core_tcam_prio_update = ulp_mapper_tfc_tcam_prio_update,
	.ulp_mapper_core_em_tbl_process = ulp_mapper_tfc_em_tbl_process,
	.ulp_mapper_core_em_entry_free = ulp_mapper_tfc_em_entry_free,
	.ulp_mapper_core_index_tbl_process = ulp_mapper_tfc_index_tbl_process,
	.ulp_mapper_core_index_entry_free = ulp_mapper_tfc_index_entry_free,
	.ulp_mapper_core_cmm_tbl_process = ulp_mapper_tfc_cmm_tbl_process,
	.ulp_mapper_core_cmm_entry_free = ulp_mapper_tfc_cmm_entry_free,
	.ulp_mapper_core_if_tbl_process = ulp_mapper_tfc_if_tbl_process,
	.ulp_mapper_core_ident_alloc_process = ulp_mapper_tfc_ident_alloc,
	.ulp_mapper_core_ident_free = ulp_mapper_tfc_ident_free,
	.ulp_mapper_core_global_ident_alloc = ulp_mapper_tfc_global_ident_alloc,
	.ulp_mapper_core_global_ident_free = ulp_mapper_tfc_global_ident_free,
	.ulp_mapper_core_glb_idx_tbl_alloc = ulp_mapper_tfc_glb_idx_tbl_alloc,
	.ulp_mapper_core_glb_idx_tbl_free = ulp_mapper_tfc_glb_idx_tbl_free,
	.ulp_mapper_core_dyn_tbl_type_get = ulp_mapper_tfc_dyn_tbl_type_get,
	.ulp_mapper_core_index_tbl_alloc_process =
		ulp_mapper_tfc_index_tbl_alloc_process,
	.ulp_mapper_core_app_glb_res_info_init =
		ulp_mapper_tfc_app_glb_resource_info_init,
	.ulp_mapper_core_handle_to_offset = ulp_mapper_tfc_handle_to_offset,
	.ulp_mapper_mpc_batch_start = ulp_mapper_tfc_mpc_batch_start,
	.ulp_mapper_mpc_batch_started = ulp_mapper_tfc_mpc_batch_started,
	.ulp_mapper_mpc_batch_end = ulp_mapper_tfc_mpc_batch_end,
	.ulp_mapper_mtr_stats_hndl_set = ulp_mapper_tfc_mtr_stats_hndl_set,
	.ulp_mapper_mtr_stats_hndl_get = ulp_mapper_tfc_mtr_stats_hndl_get,
	.ulp_mapper_mtr_stats_hndl_del = ulp_mapper_tfc_mtr_stats_hndl_del
};
