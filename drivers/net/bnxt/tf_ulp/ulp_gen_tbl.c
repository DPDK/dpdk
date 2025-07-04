/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#include <rte_log.h>
#include <rte_malloc.h>
#include "bnxt_ulp_utils.h"
#include "tf_core.h"
#include "tfp.h"
#include "ulp_mapper.h"
#include "ulp_flow_db.h"

/* Retrieve the generic table  initialization parameters for the tbl_idx */
static const struct bnxt_ulp_generic_tbl_params*
ulp_mapper_gen_tbl_params_get(struct bnxt_ulp_context *ulp_ctx,
			      uint32_t tbl_idx)
{
	struct bnxt_ulp_device_params *dparms;
	const struct bnxt_ulp_generic_tbl_params *gen_tbl;
	uint32_t dev_id;

	if (tbl_idx >= BNXT_ULP_GEN_TBL_MAX_SZ) {
		BNXT_DRV_DBG(ERR, "Gen table out of bounds %d\n", tbl_idx);
		return NULL;
	}

	if (bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id))
		return NULL;

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		BNXT_DRV_DBG(ERR, "Failed to get device parms\n");
		return NULL;
	}

	gen_tbl = &dparms->gen_tbl_params[tbl_idx];
	return gen_tbl;
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
ulp_mapper_generic_tbl_list_init(struct bnxt_ulp_context *ulp_ctx,
				 struct bnxt_ulp_mapper_data *mapper_data)
{
	const struct bnxt_ulp_generic_tbl_params *tbl;
	struct ulp_mapper_gen_tbl_list *entry;
	struct ulp_hash_create_params cparams;
	uint32_t idx, size, key_sz;

	/* Allocate the generic tables. */
	for (idx = 0; idx < BNXT_ULP_GEN_TBL_MAX_SZ; idx++) {
		tbl = ulp_mapper_gen_tbl_params_get(ulp_ctx, idx);
		if (!tbl) {
			BNXT_DRV_DBG(ERR, "Failed to get gen table parms %d\n",
				     idx);
			return -EINVAL;
		}
		entry = &mapper_data->gen_tbl_list[idx];

		/* For simple list allocate memory for key storage*/
		if (tbl->gen_tbl_type == BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST &&
		    tbl->key_num_bytes) {
			key_sz = tbl->key_num_bytes +
				tbl->partial_key_num_bytes;
			entry->container.byte_key_ex_size = tbl->key_num_bytes;
			entry->container.byte_key_par_size =
				tbl->partial_key_num_bytes;
		} else {
			key_sz = 0;
		}

		/* Allocate memory for result data and key data */
		if (tbl->result_num_entries != 0) {
			/* assign the name */
			entry->gen_tbl_name = tbl->name;
			entry->tbl_type = tbl->gen_tbl_type;
			/* add 4 bytes for reference count */
			entry->mem_data_size = (tbl->result_num_entries + 1) *
				(tbl->result_num_bytes + sizeof(uint32_t) +
				 key_sz);

			/* allocate the big chunk of memory */
			entry->mem_data = rte_zmalloc("ulp mapper gen tbl",
						      entry->mem_data_size, 0);
			if (!entry->mem_data) {
				BNXT_DRV_DBG(ERR,
					    "%s:Failed to alloc gen table %d\n",
					     tbl->name, idx);
				return -ENOMEM;
			}
			/* Populate the generic table container */
			entry->container.num_elem = tbl->result_num_entries;
			entry->container.byte_data_size = tbl->result_num_bytes;
			entry->container.ref_count =
				(uint32_t *)entry->mem_data;
			size = sizeof(uint32_t) * (tbl->result_num_entries + 1);
			entry->container.byte_data = &entry->mem_data[size];
			entry->container.byte_order = tbl->result_byte_order;
		} else {
			BNXT_DRV_DBG(DEBUG, "%s: Unused Gen tbl entry is %d\n",
				     tbl->name, idx);
			continue;
		}

		/* assign the memory for key data */
		if (tbl->gen_tbl_type == BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST &&
		    key_sz) {
			size += tbl->result_num_bytes *
				(tbl->result_num_entries + 1);
			entry->container.byte_key =
				&entry->mem_data[size];
		}

		/* Initialize Hash list for hash based generic table */
		if (tbl->gen_tbl_type == BNXT_ULP_GEN_TBL_TYPE_HASH_LIST &&
		    tbl->hash_tbl_entries) {
			cparams.key_size = tbl->key_num_bytes;
			cparams.num_buckets = tbl->num_buckets;
			cparams.num_hash_tbl_entries = tbl->hash_tbl_entries;
			cparams.num_key_entries = tbl->result_num_entries;
			if (ulp_gen_hash_tbl_list_init(&cparams,
						       &entry->hash_tbl)) {
				BNXT_DRV_DBG(ERR,
					    "%s: Failed to alloc hash tbl %d\n",
					     tbl->name, idx);
				return -ENOMEM;
			}
		}
	}
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
			tbl_list->container.byte_data = NULL;
			tbl_list->container.byte_key = NULL;
			tbl_list->container.ref_count = NULL;
		}
		if (tbl_list->hash_tbl) {
			ulp_gen_hash_tbl_list_deinit(tbl_list->hash_tbl);
			tbl_list->hash_tbl = NULL;
		}
	}
	/* success */
	return 0;
}

/*
 * Get the generic table list entry
 *
 * tbl_list [in] - Ptr to generic table
 * key [in] - Key index to the table
 * entry [out] - output will include the entry if found
 *
 * returns 0 on success.
 */
int32_t
ulp_mapper_gen_tbl_entry_get(struct ulp_mapper_gen_tbl_list *tbl_list,
			     uint32_t key,
			     struct ulp_mapper_gen_tbl_entry *entry)
{
	/* populate the output and return the values */
	if (key > tbl_list->container.num_elem) {
		BNXT_DRV_DBG(ERR, "%s: invalid key %x:%x\n",
			     tbl_list->gen_tbl_name, key,
			     tbl_list->container.num_elem);
		return -EINVAL;
	}
	entry->ref_count = &tbl_list->container.ref_count[key];
	entry->byte_data_size = tbl_list->container.byte_data_size;
	entry->byte_data = &tbl_list->container.byte_data[key *
		entry->byte_data_size];
	entry->byte_order = tbl_list->container.byte_order;
	if (tbl_list->tbl_type == BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST) {
		entry->byte_key_size = tbl_list->container.byte_key_ex_size +
			tbl_list->container.byte_key_par_size;
		entry->byte_key = &tbl_list->container.byte_key[key *
			entry->byte_key_size];
	} else {
		entry->byte_key = NULL;
		entry->byte_key_size = 0;
	}
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
		BNXT_DRV_DBG(ERR, "invalid argument %x\n", dir);
		return -EINVAL;
	}
	tbl_idx = (res_sub_type << 1) | (dir & 0x1);
	if (tbl_idx >= BNXT_ULP_GEN_TBL_MAX_SZ) {
		BNXT_DRV_DBG(ERR, "invalid table index %x\n", tbl_idx);
		return -EINVAL;
	}
	return tbl_idx;
}

/*
 * Set the data in the generic table entry, Data is in Big endian format
 *
 * entry [in] - generic table entry
 * key [in] - pointer to the key to be used for setting the value.
 * key_size [in] - The length of the key in bytess to be set
 * data [in] - pointer to the data to be used for setting the value.
 * data_size [in] - length of the data pointer in bytes.
 *
 * returns 0 on success
 */
int32_t
ulp_mapper_gen_tbl_entry_data_set(struct ulp_mapper_gen_tbl_list *tbl_list,
				  struct ulp_mapper_gen_tbl_entry *entry,
				  uint8_t *key, uint32_t key_size,
				  uint8_t *data, uint32_t data_size)
{
	/* validate the null arguments */
	if (!entry || !key || !data) {
		BNXT_DRV_DBG(ERR, "invalid argument\n");
		return -EINVAL;
	}

	/* check the size of the buffer for validation */
	if (data_size > entry->byte_data_size) {
		BNXT_DRV_DBG(ERR, "invalid offset or length %x:%x\n",
			     data_size, entry->byte_data_size);
		return -EINVAL;
	}
	memcpy(entry->byte_data, data, data_size);
	if (tbl_list->tbl_type == BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST) {
		if (key_size > entry->byte_key_size) {
			BNXT_DRV_DBG(ERR, "invalid offset or length %x:%x\n",
				     key_size, entry->byte_key_size);
		return -EINVAL;
		}
		memcpy(entry->byte_key, key, key_size);
	}
	tbl_list->container.seq_cnt++;
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
		BNXT_DRV_DBG(ERR, "invalid argument\n");
		return -EINVAL;
	}

	/* check the size of the buffer for validation */
	if ((offset + len) > ULP_BYTE_2_BITS(entry->byte_data_size) ||
	    len > ULP_BYTE_2_BITS(data_size)) {
		BNXT_DRV_DBG(ERR, "invalid offset or length %x:%x:%x\n",
			     offset, len, entry->byte_data_size);
		return -EINVAL;
	}
	if (entry->byte_order == BNXT_ULP_BYTE_ORDER_LE)
		ulp_bs_pull_lsb(entry->byte_data, data, data_size, offset, len);
	else
		ulp_bs_pull_msb(entry->byte_data, data, offset, len);

	return 0;
}

/* Free the generic table list entry
 *
 * ulp_ctx [in] - Pointer to the ulp context
 * tbl_idx [in] - Index of the generic table
 * ckey [in] - Key for the entry in the table
 *
 * returns 0 on success
 */
int32_t
ulp_mapper_gen_tbl_entry_free(struct bnxt_ulp_context *ulp_ctx,
			      uint32_t tbl_idx, uint32_t ckey)
{
	struct ulp_flow_db_res_params res;
	uint32_t fid = 0; /* not using for this case */

	res.direction = tbl_idx & 0x1;
	res.resource_sub_type = tbl_idx >> 1;
	res.resource_hndl = ckey;

	return ulp_mapper_gen_tbl_res_free(ulp_ctx, fid, &res);
}

/* Free the generic table list resource
 *
 * ulp_ctx [in] - Pointer to the ulp context
 * fid [in] - The fid the generic table is associated with
 * res [in] - Pointer to flow db resource entry
 *
 * returns 0 on success
 */
int32_t
ulp_mapper_gen_tbl_res_free(struct bnxt_ulp_context *ulp_ctx,
			    uint32_t fid,
			    struct ulp_flow_db_res_params *res)
{
	struct bnxt_ulp_mapper_data *mapper_data;
	struct ulp_mapper_gen_tbl_list *gen_tbl_list;
	struct ulp_mapper_gen_tbl_entry entry;
	struct ulp_gen_hash_entry_params hash_entry;
	int32_t tbl_idx;
	uint32_t rid = 0;
	uint32_t key_idx;

	/* Extract the resource sub type and direction */
	tbl_idx = ulp_mapper_gen_tbl_idx_calculate(res->resource_sub_type,
						   res->direction);
	if (tbl_idx < 0) {
		BNXT_DRV_DBG(ERR, "invalid argument %x:%x\n",
			     res->resource_sub_type, res->direction);
		return -EINVAL;
	}

	mapper_data = bnxt_ulp_cntxt_ptr2_mapper_data_get(ulp_ctx);
	if (!mapper_data) {
		BNXT_DRV_DBG(ERR, "invalid ulp context %x\n", tbl_idx);
		return -EINVAL;
	}
	/* get the generic table  */
	gen_tbl_list = &mapper_data->gen_tbl_list[tbl_idx];

	/* Get the generic table entry*/
	if (gen_tbl_list->hash_tbl) {
		/* use the hash index to get the value */
		hash_entry.hash_index = (uint32_t)res->resource_hndl;
		if (ulp_gen_hash_tbl_list_index_search(gen_tbl_list->hash_tbl,
						       &hash_entry)) {
			BNXT_DRV_DBG(ERR, "Unable to find has entry %x:%x\n",
				     tbl_idx, hash_entry.hash_index);
			return -EINVAL;
		}
		key_idx = hash_entry.key_idx;

	} else {
		key_idx =  (uint32_t)res->resource_hndl;
	}
	if (ulp_mapper_gen_tbl_entry_get(gen_tbl_list, key_idx, &entry)) {
		BNXT_DRV_DBG(ERR, "Gen tbl entry get failed %x:%" PRIX64 "\n",
			     tbl_idx, res->resource_hndl);
		return -EINVAL;
	}

	/* Decrement the reference count */
	if (!ULP_GEN_TBL_REF_CNT(&entry)) {
		BNXT_DRV_DBG(DEBUG,
			    "generic table entry already free %x:%" PRIX64 "\n",
			     tbl_idx, res->resource_hndl);
		return 0;
	}
	ULP_GEN_TBL_REF_CNT_DEC(&entry);

	/* retain the details since there are other users */
	if (ULP_GEN_TBL_REF_CNT(&entry))
		return 0;

	/* Delete the generic table entry. First extract the rid */
	if (ulp_mapper_gen_tbl_entry_data_get(&entry, ULP_GEN_TBL_FID_OFFSET,
					      ULP_GEN_TBL_FID_SIZE_BITS,
					      (uint8_t *)&rid,
					      sizeof(rid))) {
		BNXT_DRV_DBG(ERR, "Unable to get rid %x:%" PRIX64 "\n",
			     tbl_idx, res->resource_hndl);
		return -EINVAL;
	}
	rid = tfp_be_to_cpu_32(rid);
	/* no need to del if rid is 0 since there is no associated resource
	 * if rid from the entry is equal to the incoming fid, then we have a
	 * recursive delete, so don't follow the rid.
	 */
	if (rid && rid != fid) {
		/* Destroy the flow associated with the shared flow id */
		if (ulp_mapper_flow_destroy(ulp_ctx, BNXT_ULP_FDB_TYPE_RID,
					    rid, NULL))
			BNXT_DRV_DBG(ERR,
				    "Error in deleting shared resource id %x\n",
				     rid);
	}

	/* Delete the entry from the hash table */
	if (gen_tbl_list->hash_tbl)
		ulp_gen_hash_tbl_list_del(gen_tbl_list->hash_tbl, &hash_entry);

	/* decrement the count */
	if (gen_tbl_list->tbl_type == BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST &&
	    gen_tbl_list->container.seq_cnt > 0)
		gen_tbl_list->container.seq_cnt--;

	/* clear the byte data of the generic table entry */
	memset(entry.byte_data, 0, entry.byte_data_size);

	return 0;
}

/*
 * Write the generic table list hash entry
 *
 * tbl_list [in] - pointer to the generic table list
 * hash_entry [in] -  Hash table entry
 * gen_tbl_ent [out] - generic table entry
 *
 * returns 0 on success.
 */
int32_t
ulp_mapper_gen_tbl_hash_entry_add(struct ulp_mapper_gen_tbl_list *tbl_list,
				  struct ulp_gen_hash_entry_params *hash_entry,
				  struct ulp_mapper_gen_tbl_entry *gen_tbl_ent)
{
	uint32_t key;
	int32_t rc = 0;

	switch (hash_entry->search_flag) {
	case ULP_GEN_HASH_SEARCH_FOUND:
		BNXT_DRV_DBG(ERR, "%s: gen hash entry already present\n",
			     tbl_list->gen_tbl_name);
		return -EINVAL;
	case ULP_GEN_HASH_SEARCH_FULL:
		BNXT_DRV_DBG(ERR, "%s: gen hash table is full\n",
			     tbl_list->gen_tbl_name);
		return -EINVAL;
	case ULP_GEN_HASH_SEARCH_MISSED:
		rc = ulp_gen_hash_tbl_list_add(tbl_list->hash_tbl, hash_entry);
		if (rc) {
			BNXT_DRV_DBG(ERR, "%s: gen hash table add failed\n",
				     tbl_list->gen_tbl_name);
			return -EINVAL;
		}
		key = hash_entry->key_idx;
		gen_tbl_ent->ref_count = &tbl_list->container.ref_count[key];
		gen_tbl_ent->byte_data_size =
			tbl_list->container.byte_data_size;
		gen_tbl_ent->byte_data = &tbl_list->container.byte_data[key *
			gen_tbl_ent->byte_data_size];
		gen_tbl_ent->byte_order = tbl_list->container.byte_order;
		break;
	default:
		BNXT_DRV_DBG(ERR, "%s: invalid search flag\n",
			     tbl_list->gen_tbl_name);
		return -EINVAL;
	}

	return rc;
}

/*
 * Perform add entry in the simple list
 *
 * tbl_list [in] - pointer to the generic table list
 * key [in] -  Key added as index
 * data [in] -  data added as result
 * key_index [out] - index to the entry
 * gen_tbl_ent [out] - write the output to the entry
 *
 * returns 0 on success.
 */
int32_t
ulp_gen_tbl_simple_list_add_entry(struct ulp_mapper_gen_tbl_list *tbl_list,
				  uint8_t *key,
				  uint8_t *data,
				  uint32_t *key_index,
				  struct ulp_mapper_gen_tbl_entry *ent)
{
	struct ulp_mapper_gen_tbl_cont	*cont;
	uint32_t key_size, idx;
	uint8_t *entry_key;

	/* sequentially search for the matching key */
	cont = &tbl_list->container;
	for (idx = 0; idx < cont->num_elem; idx++) {
		ent->ref_count = &cont->ref_count[idx];
		if (ULP_GEN_TBL_REF_CNT(ent) == 0) {
			/* add the entry */
			key_size = cont->byte_key_ex_size +
				cont->byte_key_par_size;
			entry_key = &cont->byte_key[idx * key_size];
			ent->byte_data_size = cont->byte_data_size;
			ent->byte_data = &cont->byte_data[idx *
				cont->byte_data_size];
			memcpy(entry_key, key, key_size);
			memcpy(ent->byte_data, data, ent->byte_data_size);
			ent->byte_order = cont->byte_order;
			*key_index = idx;
			cont->seq_cnt++;
			return 0;
		}
	}
	/* No more memory */
	return -ENOMEM;
}

/* perform the subset and superset. len should be 64bit multiple*/
static enum ulp_gen_list_search_flag
ulp_gen_tbl_overlap_check(uint8_t *key1, uint8_t *key2, uint32_t len)
{
	uint32_t sz = 0, superset = 0, subset = 0;
	uint64_t src, dst;

	while (sz  < len) {
		memcpy(&dst, key2, sizeof(dst));
		memcpy(&src, key1, sizeof(src));
		sz += sizeof(src);
		if (dst == src)
			continue;
		else if (dst == (dst | src))
			superset = 1;
		else if (src == (dst | src))
			subset = 1;
		else
			return ULP_GEN_LIST_SEARCH_MISSED;
	}
	if (superset && !subset)
		return ULP_GEN_LIST_SEARCH_FOUND_SUPERSET;
	if (!superset && subset)
		return ULP_GEN_LIST_SEARCH_FOUND_SUBSET;
	return ULP_GEN_LIST_SEARCH_FOUND;
}

uint32_t
ulp_gen_tbl_simple_list_search(struct ulp_mapper_gen_tbl_list *tbl_list,
			       uint8_t *match_key,
			       uint32_t *key_idx)
{
	enum ulp_gen_list_search_flag rc = ULP_GEN_LIST_SEARCH_FULL;
	uint32_t idx = 0, key_idx_set = 0, sz = 0, key_size = 0;
	struct ulp_mapper_gen_tbl_entry ent = { 0 };
	struct ulp_mapper_gen_tbl_cont	*cont = &tbl_list->container;
	uint8_t *k1 = NULL, *k2, *entry_key;
	uint32_t valid_ent = 0;

	key_size = cont->byte_key_ex_size + cont->byte_key_par_size;
	if (cont->byte_key_par_size)
		k1 = match_key + cont->byte_key_ex_size;

	/* sequentially search for the matching key */
	while (idx < cont->num_elem) {
		ent.ref_count = &cont->ref_count[idx];
		entry_key = &cont->byte_key[idx * key_size];
		/* check ref count not zero and exact key matches */
		if (ULP_GEN_TBL_REF_CNT(&ent)) {
			/* compare the exact match */
			if (!memcmp(match_key, entry_key,
				    cont->byte_key_ex_size)) {
				/* Match the partial key*/
				if (cont->byte_key_par_size) {
					k2 = entry_key + cont->byte_key_ex_size;
					sz = cont->byte_key_par_size;
					rc = ulp_gen_tbl_overlap_check(k1, k2,
								       sz);
					if (rc != ULP_GEN_LIST_SEARCH_MISSED) {
						*key_idx = idx;
						return rc;
					}
				} else {
					/* found the entry return */
					rc = ULP_GEN_LIST_SEARCH_FOUND;
					*key_idx = idx;
					return rc;
				}
			}
			++valid_ent;
		} else {
			/* empty slot */
			if (!key_idx_set) {
				*key_idx = idx;
				key_idx_set = 1;
				rc = ULP_GEN_LIST_SEARCH_MISSED;
			}
			if (valid_ent >= cont->seq_cnt)
				return rc;
		}
		idx++;
	}
	return rc;
}
