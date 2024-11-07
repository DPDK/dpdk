/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#include <rte_malloc.h>
#include "ulp_matcher.h"
#include "ulp_mapper.h"
#include "ulp_utils.h"
#include "bnxt_ulp_utils.h"

#ifndef RTE_HASH_BUCKET_ENTRIES
/* it is defined in lib/hash/rte_cuckoo_hash.h */
#define RTE_HASH_BUCKET_ENTRIES     8
#endif /* RTE_HASH_BUCKET_ENTRIES */

static int32_t
ulp_matcher_class_list_lookup(struct ulp_rte_parser_params *params,
			      uint32_t *class_match_idx)
{
	struct bnxt_ulp_class_match_info *class_list = ulp_class_match_list;
	uint32_t idx = 0;

	while (++idx < BNXT_ULP_CLASS_MATCH_LIST_MAX_SZ) {
		/* iterate the list of class matches to find header match */
		if (class_list[idx].app_id == params->app_id &&
		    !ULP_BITMAP_CMP(&class_list[idx].hdr_bitmap,
				    &params->hdr_bitmap)) {
			/* Found the match */
			*class_match_idx = idx;
			return 0;
		}
	}
	BNXT_DRV_DBG(DEBUG, "Did not find any matching protocol hdr\n");
	return -1;
}

static int32_t
ulp_matcher_action_list_lookup(struct ulp_rte_parser_params *params,
			       uint32_t *act_tmpl_idx)
{
	struct bnxt_ulp_act_match_info *act_list = ulp_act_match_list;
	uint64_t act_bits = params->act_bitmap.bits;
	uint32_t idx = 0;

	while (++idx < BNXT_ULP_ACT_MATCH_LIST_MAX_SZ) {
		/* iterate the list of action matches to find header match */
		if ((act_bits & act_list[idx].act_bitmap.bits) == act_bits) {
			/* Found the match */
			*act_tmpl_idx = act_list[idx].act_tid;
			/* set the comp field to enable action reject cond */
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_ACT_REJ_COND_EN, 1);
			return 0;
		}
	}
	return -1;
}

static int32_t
ulp_matcher_class_hdr_field_validate(struct ulp_rte_parser_params *params,
				     uint32_t idx)
{
	struct bnxt_ulp_class_match_info *info = &ulp_class_match_list[idx];
	uint64_t bitmap;

	/* manadatory fields should be enabled */
	if ((params->fld_s_bitmap.bits & info->field_man_bitmap) !=
	    info->field_man_bitmap){
		BNXT_DRV_DBG(DEBUG, "mismatch in manadatory hdr fields.\n");
		return -EINVAL;
	}

	/* optional fields may be enabled or not so ignore them */
	bitmap = params->fld_s_bitmap.bits & (~info->field_man_bitmap);
	if ((bitmap && (bitmap & info->field_opt_bitmap) != bitmap)) {
		BNXT_DRV_DBG(DEBUG, "mismatch in optional hdr fields.\n");
		return -EINVAL;
	}

	return 0;
}

static uint64_t
ulp_matcher_class_hdr_field_signature(struct ulp_rte_parser_params *params,
				      uint32_t idx)
{
	struct bnxt_ulp_class_match_info *info = &ulp_class_match_list[idx];

	/* remove the exclude bits */
	return params->fld_s_bitmap.bits & ~info->field_exclude_bitmap;
}

static uint64_t
ulp_matcher_class_wc_fld_get(uint32_t idx)
{
	struct bnxt_ulp_class_match_info *info = &ulp_class_match_list[idx];
	uint64_t bits;

	bits = info->field_opt_bitmap | info->field_man_bitmap;
	bits &= ~info->field_exclude_bitmap;
	return bits;
}

static int32_t
ulp_matcher_class_hash_lookup(struct bnxt_ulp_matcher_data *matcher_data,
			      struct ulp_rte_parser_params *params,
			      uint32_t *class_hash_idx)
{
	struct ulp_matcher_hash_db_key key = { {0} };
	struct ulp_matcher_class_db_node *node;
	int32_t idx;
	int32_t rc = -ENOENT;

	/* populate the key for the search */
	key.app_id = params->app_id;
	key.hdr_bitmap = params->hdr_bitmap;

	/* search the hash table for the hdr bit match */
	idx  = rte_hash_lookup(matcher_data->class_matcher_db,
			       (const void *)&key);
	if (idx < 0 || idx >= matcher_data->class_list_size)
		return rc; /* No Entry */

	node = &matcher_data->class_list[idx];
	if (!node->in_use) {
		BNXT_DRV_DBG(ERR, "PANIC: Matcher database is corrupt %d\n",
			     idx);
		return rc;
	}
	*class_hash_idx = idx;
	return 0; /* Success */
}

static int32_t
ulp_matcher_class_hash_add(struct bnxt_ulp_matcher_data *matcher_data,
			   struct ulp_rte_parser_params *params,
			   uint32_t class_match_idx,
			   uint32_t *class_hash_idx)
{
	struct ulp_matcher_hash_db_key key = { {0} };
	struct ulp_matcher_class_db_node *node;
	int32_t hash_idx;
	int32_t rc = -EINVAL;

	/* populate the key for the search */
	key.app_id = params->app_id;
	key.hdr_bitmap = params->hdr_bitmap;

	/* add to the hash table for the hdr bit match */
	hash_idx = rte_hash_add_key(matcher_data->class_matcher_db,
				    (const void *)&key);
	if (hash_idx < 0 || hash_idx >= matcher_data->class_list_size) {
		BNXT_DRV_DBG(ERR, "unable to add entry to matcher hash %d\n",
			     hash_idx);
		return rc;
	}
	/* Initialize the class db node with default values */
	node = &matcher_data->class_list[hash_idx];
	node->in_use = 1;
	node->match_info_idx = class_match_idx;
	*class_hash_idx = hash_idx;
	return 0;
}

/*
 * Function to handle the matching of RTE Flows and validating
 * the pattern masks against the flow templates.
 */
int32_t
ulp_matcher_pattern_match(struct ulp_rte_parser_params *params,
			  uint32_t *class_id)
{
	struct bnxt_ulp_class_match_info *class_match;
	struct ulp_matcher_class_db_node *class_node;
	struct bnxt_ulp_matcher_data *matcher_data;
	uint32_t class_match_idx = 0;
	uint32_t hash_idx;
	uint64_t bits = 0;

	/* Get the matcher data for hash lookup  */
	matcher_data = (struct bnxt_ulp_matcher_data *)
		bnxt_ulp_cntxt_ptr2_matcher_data_get(params->ulp_ctx);
	if (!matcher_data) {
		BNXT_DRV_DBG(ERR, "Failed to get the ulp matcher data\n");
		return -EINVAL;
	}

	bits = bnxt_ulp_cntxt_ptr2_default_class_bits_get(params->ulp_ctx);
	params->hdr_bitmap.bits |= bits;

	/* search the matcher hash db for the entry  */
	if (ulp_matcher_class_hash_lookup(matcher_data, params,
					  &hash_idx) == -ENOENT) {
		/* find  the class list entry */
		if (ulp_matcher_class_list_lookup(params, &class_match_idx))
			goto error;

		/* add it to the hash */
		if (ulp_matcher_class_hash_add(matcher_data, params,
					       class_match_idx, &hash_idx))
			goto error;
	}
	class_node = &matcher_data->class_list[hash_idx];
	class_match = &ulp_class_match_list[class_node->match_info_idx];
	class_match_idx = class_node->match_info_idx;

	/* perform the field bitmap validation */
	if (ulp_matcher_class_hdr_field_validate(params,
						 class_node->match_info_idx))
		goto error;

	/* Update the fields for further processing */
	*class_id = class_match->class_tid;
	params->class_info_idx = class_node->match_info_idx;
	params->flow_sig_id =
		ulp_matcher_class_hdr_field_signature(params, class_match_idx);
	params->flow_pattern_id = class_match->flow_pattern_id;
	params->wc_field_bitmap = ulp_matcher_class_wc_fld_get(class_match_idx);
	params->exclude_field_bitmap = class_match->field_exclude_bitmap;

	BNXT_DRV_DBG(DEBUG, "Found matching pattern template %u:%d\n",
		     class_match_idx, class_match->class_tid);
	return BNXT_TF_RC_SUCCESS;

error:
	BNXT_DRV_DBG(DEBUG, "Did not find any matching template\n");
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
	BNXT_DRV_DBG(DEBUG,
		     "hid:%x,Hdr:%" PRIX64 " Fld:%" PRIX64 " SFl:%" PRIX64 "\n",
		     class_match_idx, params->hdr_bitmap.bits,
		     params->fld_bitmap.bits, params->fld_s_bitmap.bits);
#endif
	*class_id = 0;
	return BNXT_TF_RC_ERROR;
}

static int32_t
ulp_matcher_action_hash_lookup(struct bnxt_ulp_matcher_data *matcher_data,
			       struct ulp_rte_parser_params *params,
			       uint32_t *act_tmpl_idx)
{
	struct ulp_matcher_action_hash_db_key key = { {0} };
	struct ulp_matcher_act_db_node *node;
	int32_t idx;

	/* populate the key for the search */
	key.act_bitmap = params->act_bitmap;

	/* search the hash table for the hdr bit match */
	idx  = rte_hash_lookup(matcher_data->action_matcher_db,
			       (const void *)&key);
	if (idx < 0 || idx >= BNXT_ULP_ACT_HASH_LIST_SIZE)
		return -ENOENT; /* No Entry */

	node = &matcher_data->act_list[idx];
	*act_tmpl_idx = node->act_tmpl_idx;
	return 0; /* Success */
}

static int32_t
ulp_matcher_action_hash_add(struct bnxt_ulp_matcher_data *matcher_data,
			    struct ulp_rte_parser_params *params,
			    uint32_t match_idx)
{
	struct ulp_matcher_action_hash_db_key key = { {0} };
	struct ulp_matcher_act_db_node *node;
	int32_t hash_idx;
	int32_t rc = -EINVAL;

	/* populate the key for the search */
	key.act_bitmap = params->act_bitmap;

	/* add to the hash table for the hdr bit match */
	hash_idx = rte_hash_add_key(matcher_data->action_matcher_db,
				    (const void *)&key);
	if (hash_idx < 0 || hash_idx >= BNXT_ULP_ACT_HASH_LIST_SIZE) {
		BNXT_DRV_DBG(ERR, "unable to add entry to action hash %d\n",
			     hash_idx);
		return rc;
	}
	/* Initialize the class db node with default values */
	node = &matcher_data->act_list[hash_idx];
	node->act_tmpl_idx = match_idx;
	return 0;
}

/*
 * Function to handle the matching of RTE Flows and validating
 * the action against the flow templates.
 */
int32_t
ulp_matcher_action_match(struct ulp_rte_parser_params *params,
			 uint32_t *act_id)
{
	struct bnxt_ulp_matcher_data *matcher_data;
	uint32_t act_tmpl_idx = 0;
	uint64_t bits = 0;

	/* Get the matcher data for hash lookup  */
	matcher_data = (struct bnxt_ulp_matcher_data *)
		bnxt_ulp_cntxt_ptr2_matcher_data_get(params->ulp_ctx);
	if (!matcher_data) {
		BNXT_DRV_DBG(ERR, "Failed to get the ulp matcher data\n");
		return -EINVAL;
	}

	bits = bnxt_ulp_cntxt_ptr2_default_act_bits_get(params->ulp_ctx);
	params->act_bitmap.bits |= bits;

	/* search the matcher hash db for the entry  */
	if (ulp_matcher_action_hash_lookup(matcher_data, params,
					   &act_tmpl_idx) == -ENOENT) {
		/* find the action entry */
		if (ulp_matcher_action_list_lookup(params, &act_tmpl_idx))
			goto error;

		/* add it to the hash */
		if (ulp_matcher_action_hash_add(matcher_data, params,
						act_tmpl_idx))
			goto error;
	}

	BNXT_DRV_DBG(DEBUG, "Found matching action template %u\n", act_tmpl_idx);
	*act_id = act_tmpl_idx;
	return BNXT_TF_RC_SUCCESS;
error:
	BNXT_DRV_DBG(DEBUG, "Did not find any matching action template\n");
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
	BNXT_DRV_DBG(DEBUG, "Hdr:%" PRIX64 "\n", params->act_bitmap.bits);
#endif
	*act_id = 0;
	return BNXT_TF_RC_ERROR;
}

int32_t ulp_matcher_init(struct bnxt_ulp_context *ulp_ctx)
{
	struct rte_hash_parameters hash_tbl_params = {0};
	char hash_class_tbl_name[64] = {0};
	char hash_act_tbl_name[64] = {0};
	struct bnxt_ulp_matcher_data *data;
	uint16_t port_id;

	/* append port_id to the buffer name */
	port_id = ulp_ctx->bp->eth_dev->data->port_id;
	snprintf(hash_class_tbl_name, sizeof(hash_class_tbl_name),
		 "bnxt_ulp_class_matcher_%d", port_id);
	snprintf(hash_act_tbl_name, sizeof(hash_act_tbl_name),
		 "bnxt_ulp_act_matcher_%d", port_id);

	data = rte_zmalloc("bnxt_ulp_matcher_data",
			   sizeof(struct bnxt_ulp_matcher_data), 0);
	if (!data) {
		BNXT_DRV_DBG(ERR, "Failed to allocate the matcher data\n");
		return -ENOMEM;
	}

	if (bnxt_ulp_cntxt_ptr2_matcher_data_set(ulp_ctx, data)) {
		BNXT_DRV_DBG(ERR, "Failed to set matcher data in context\n");
		rte_free(data);
		return -ENOMEM;
	}

	/* create the hash table for the matcher entries */
	hash_tbl_params.name = hash_class_tbl_name;
	hash_tbl_params.entries = BNXT_ULP_CLASS_MATCH_LIST_MAX_SZ +
				  RTE_HASH_BUCKET_ENTRIES;

	hash_tbl_params.key_len = sizeof(struct ulp_matcher_hash_db_key);
	hash_tbl_params.socket_id = rte_socket_id();
	data->class_matcher_db = rte_hash_create(&hash_tbl_params);
	if (data->class_matcher_db == NULL) {
		BNXT_DRV_DBG(ERR, "Failed to create class matcher hash tbl\n");
		goto error;
	}

	/* allocate memorry for the class list */
	data->class_list_size = hash_tbl_params.entries;
	data->class_list = rte_zmalloc("bnxt_ulp_matcher_class_list",
				       sizeof(struct ulp_matcher_class_db_node)
				       * data->class_list_size, 0);
	if (data->class_list == NULL) {
		BNXT_DRV_DBG(ERR, "Failed to create matcher class list\n");
		goto error;
	}

	/* create the hash table for the action entries */
	hash_tbl_params.name = hash_act_tbl_name;
	/* The hash list size set to max support and not dependent on template*/
	hash_tbl_params.entries = BNXT_ULP_ACT_HASH_LIST_SIZE;
	hash_tbl_params.key_len = sizeof(struct ulp_matcher_action_hash_db_key);
	hash_tbl_params.socket_id = rte_socket_id();
	data->action_matcher_db = rte_hash_create(&hash_tbl_params);
	if (data->action_matcher_db == NULL) {
		BNXT_DRV_DBG(ERR, "Failed to create action matcher hash tbl\n");
		goto error;
	}

	/* allocate memorry for the action list */
	data->act_list = rte_zmalloc("bnxt_ulp_matcher_act_list",
				     sizeof(struct ulp_matcher_act_db_node)
				       * BNXT_ULP_ACT_HASH_LIST_SIZE, 0);
	if (data->act_list == NULL) {
		BNXT_DRV_DBG(ERR, "Failed to create matcher act list\n");
		goto error;
	}
	return 0;
error:
	ulp_matcher_deinit(ulp_ctx);
	return -ENOMEM;
}

void ulp_matcher_deinit(struct bnxt_ulp_context *ulp_ctx)
{
	struct bnxt_ulp_matcher_data *data;

	if (!ulp_ctx) {
		BNXT_DRV_DBG(ERR, "Failed to acquire ulp context\n");
		return;
	}

	data = (struct bnxt_ulp_matcher_data *)
		bnxt_ulp_cntxt_ptr2_matcher_data_get(ulp_ctx);
	if (!data) {
		/* Go ahead and return since there is no allocated data. */
		BNXT_DRV_DBG(ERR, "No data appears to have been allocated.\n");
		return;
	}

	/* Delete all the hash nodes and the hash list */
	rte_hash_free(data->class_matcher_db);
	data->class_matcher_db = NULL;

	/* free matcher class list */
	rte_free(data->class_list);
	data->class_list = NULL;

	/* Delete all the hash nodes and the hash list */
	rte_hash_free(data->action_matcher_db);
	data->action_matcher_db = NULL;

	/* free matcher act list */
	rte_free(data->act_list);
	data->act_list = NULL;

	/* free the matcher data */
	rte_free(data);

	/* Reset the data pointer within the ulp_ctx. */
	bnxt_ulp_cntxt_ptr2_matcher_data_set(ulp_ctx, NULL);
}
