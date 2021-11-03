/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#include <rte_log.h>
#include <rte_malloc.h>
#include "bnxt_tf_common.h"
#include "ulp_gen_hash.h"
#include "ulp_utils.h"
#include "tf_hash.h"

static
int32_t ulp_bit_alloc_list_alloc(struct bit_alloc_list *blist,
				 uint32_t *index)
{
	uint64_t bentry;
	uint32_t idx = 0, jdx = 0;
	uint32_t bsize_64 = blist->bsize / ULP_64B_IN_BYTES;

	/* Iterate all numbers that have all 1's */
	do {
		bentry = blist->bdata[idx++];
	} while (bentry == -1UL && idx <= bsize_64);

	if (idx <= bsize_64) {
		if (bentry)
			jdx = __builtin_clzl(~bentry);
		*index = ((idx - 1) * ULP_INDEX_BITMAP_SIZE) + jdx;
		ULP_INDEX_BITMAP_SET(blist->bdata[(idx - 1)], jdx);
		return 0;
	}
	jdx = (uint32_t)(bsize_64 * ULP_INDEX_BITMAP_SIZE);
	BNXT_TF_DBG(ERR, "bit allocator is full reached max:%x\n", jdx);
	return -1;
}

static
int32_t ulp_bit_alloc_list_dealloc(struct bit_alloc_list *blist,
				   uint32_t index)
{
	uint32_t idx = 0, jdx;
	uint32_t bsize_64 = blist->bsize / ULP_64B_IN_BYTES;

	idx = index / ULP_INDEX_BITMAP_SIZE;
	if (idx >= bsize_64) {
		BNXT_TF_DBG(ERR, "invalid bit index %x:%x\n", idx,
			    blist->bsize);
		return -EINVAL;
	}
	jdx = index % ULP_INDEX_BITMAP_SIZE;
	ULP_INDEX_BITMAP_RESET(blist->bdata[idx], jdx);
	return 0;
}

/*
 * Initialize the Generic Hash table
 *
 * cparams [in] Pointer to hash create params list
 * hash_tbl [out] the pointer to created hash table
 *
 * returns 0 on success
 */
int32_t
ulp_gen_hash_tbl_list_init(struct ulp_hash_create_params *cparams,
			   struct ulp_gen_hash_tbl **hash_table)
{
	struct ulp_gen_hash_tbl *hash_tbl = NULL;
	int32_t rc = 0;
	uint32_t size = 0;

	/* validate the arguments */
	if (!hash_table || !cparams) {
		BNXT_TF_DBG(ERR, "invalid arguments\n");
		return -EINVAL;
	}

	/* validate the size parameters */
	if (ulp_util_is_power_of_2(cparams->num_hash_tbl_entries) ||
	    ulp_util_is_power_of_2(cparams->num_key_entries) ||
	    (cparams->num_buckets % ULP_HASH_BUCKET_ROW_SZ)) {
		BNXT_TF_DBG(ERR, "invalid arguments for hash tbl\n");
		return -EINVAL;
	}

	/* validate the size of the hash table size */
	if (cparams->num_hash_tbl_entries >= ULP_GEN_HASH_MAX_TBL_SIZE) {
		BNXT_TF_DBG(ERR, "invalid size for hash tbl\n");
		return -EINVAL;
	}

	hash_tbl = rte_zmalloc("Generic hash table",
			       sizeof(struct ulp_gen_hash_tbl), 0);
	if (!hash_tbl) {
		BNXT_TF_DBG(ERR, "failed to alloc mem for hash tbl\n");
		return -ENOMEM;
	}
	*hash_table = hash_tbl;
	/* allocate the memory for the hash key table */
	hash_tbl->num_key_entries = cparams->num_key_entries;
	hash_tbl->key_tbl.data_size = cparams->key_size;
	hash_tbl->key_tbl.mem_size = cparams->key_size *
		(cparams->num_key_entries + 1);
	hash_tbl->key_tbl.key_data = rte_zmalloc("Generic hash keys",
						 hash_tbl->key_tbl.mem_size, 0);
	if (!hash_tbl->key_tbl.key_data) {
		BNXT_TF_DBG(ERR, "failed to alloc mem for hash key\n");
		rc = -ENOMEM;
		goto init_error;
	}

	/* allocate the memory for the hash table */
	hash_tbl->hash_bkt_num = cparams->num_buckets / ULP_HASH_BUCKET_ROW_SZ;
	hash_tbl->hash_tbl_size = cparams->num_hash_tbl_entries;
	size = hash_tbl->hash_tbl_size * hash_tbl->hash_bkt_num *
		sizeof(struct ulp_hash_bucket_entry);
	hash_tbl->hash_list = rte_zmalloc("Generic hash table list", size,
					  ULP_BUFFER_ALIGN_64_BYTE);
	if (!hash_tbl->hash_list) {
		BNXT_TF_DBG(ERR, "failed to alloc mem for hash tbl\n");
		rc = -ENOMEM;
		goto init_error;
	}

	/* calculate the hash_mask based on the tbl size */
	size = 1;
	while (size < hash_tbl->hash_tbl_size)
		size = size << 1;
	hash_tbl->hash_mask = size - 1;

	/* allocate the memory for the bit allocator */
	size = (cparams->num_key_entries / sizeof(uint64_t));
	size = ULP_BYTE_ROUND_OFF_8(size);
	hash_tbl->bit_list.bsize = size;
	hash_tbl->bit_list.bdata = rte_zmalloc("Generic hash bit alloc", size,
					       ULP_BUFFER_ALIGN_64_BYTE);
	if (!hash_tbl->bit_list.bdata) {
		BNXT_TF_DBG(ERR, "failed to alloc mem for hash bit list\n");
		rc = -ENOMEM;
		goto init_error;
	}
	return rc;

init_error:
	if (hash_tbl)
		ulp_gen_hash_tbl_list_deinit(hash_tbl);
	return rc;
}

/*
 * Free the generic hash table
 *
 * hash_tbl [in] the pointer to hash table
 *
 * returns 0 on success
 */
int32_t
ulp_gen_hash_tbl_list_deinit(struct ulp_gen_hash_tbl *hash_tbl)
{
	if (!hash_tbl)
		return -EINVAL;

	if (hash_tbl->key_tbl.key_data) {
		rte_free(hash_tbl->key_tbl.key_data);
		hash_tbl->key_tbl.key_data = NULL;
	}

	if (hash_tbl->hash_list) {
		rte_free(hash_tbl->hash_list);
		hash_tbl->hash_list = NULL;
	}

	if (hash_tbl->bit_list.bdata) {
		rte_free(hash_tbl->bit_list.bdata);
		hash_tbl->bit_list.bdata = NULL;
	}

	rte_free(hash_tbl);
	return 0;
}

/*
 * Search the generic hash table using key data
 *
 * hash_tbl [in] the pointer to hash table
 * entry [in/out] pointer to hash entry details.
 *
 * returns 0 on success and marks search flag as found.
 */
int32_t
ulp_gen_hash_tbl_list_key_search(struct ulp_gen_hash_tbl *hash_tbl,
				 struct ulp_gen_hash_entry_params *entry)
{
	uint32_t hash_id, key_idx, idx;
	uint16_t *bucket;
	int32_t miss_idx = ULP_HASH_BUCKET_INVAL;

	/* validate the arguments */
	if (!hash_tbl || !entry || !entry->key_data || entry->key_length !=
	    hash_tbl->key_tbl.data_size) {
		BNXT_TF_DBG(ERR, "invalid arguments\n");
		return -EINVAL;
	}

	/* calculate the hash */
	hash_id = tf_hash_calc_crc32(entry->key_data,
				     hash_tbl->key_tbl.data_size);
	hash_id = (uint16_t)(((hash_id >> 16) & 0xffff) ^ (hash_id & 0xffff));
	hash_id &= hash_tbl->hash_mask;
	hash_id = hash_id * hash_tbl->hash_bkt_num;

	/* Iterate the bucket list */
	bucket = (uint16_t *)&hash_tbl->hash_list[hash_id];
	for (idx = 0; idx < (hash_tbl->hash_bkt_num * ULP_HASH_BUCKET_ROW_SZ);
	      idx++, bucket++) {
		if (ULP_HASH_BUCKET_INUSE(bucket)) {
			/* compare the key contents */
			key_idx = ULP_HASH_BUCKET_INDEX(bucket);
			if (key_idx >= hash_tbl->num_key_entries) {
				BNXT_TF_DBG(ERR, "Hash table corruption\n");
				return -EINVAL;
			}
			if (!memcmp(entry->key_data,
				    &hash_tbl->key_tbl.key_data[key_idx *
				    hash_tbl->key_tbl.data_size],
				    hash_tbl->key_tbl.data_size)) {
				/* Found the entry */
				entry->search_flag = ULP_GEN_HASH_SEARCH_FOUND;
				entry->hash_index = ULP_HASH_INDEX_CALC(hash_id,
									idx);
				entry->key_idx = key_idx;
				return 0;
			}
		} else if (miss_idx == ULP_HASH_BUCKET_INVAL) {
			miss_idx = idx;
		}
	}

	if (miss_idx == ULP_HASH_BUCKET_INVAL) {
		entry->search_flag = ULP_GEN_HASH_SEARCH_FULL;
	} else {
		entry->search_flag = ULP_GEN_HASH_SEARCH_MISSED;
		entry->hash_index = ULP_HASH_INDEX_CALC(hash_id, miss_idx);
	}
	return 0;
}

/*
 * Search the generic hash table using hash index
 *
 * hash_tbl [in] the pointer to hash table
 * entry [in/out] pointer to hash entry details.
 *
 * returns 0 on success and marks search flag as found.
 */
int32_t
ulp_gen_hash_tbl_list_index_search(struct ulp_gen_hash_tbl *hash_tbl,
				   struct ulp_gen_hash_entry_params *entry)
{
	uint32_t idx;
	uint16_t *bucket;

	/* validate the arguments */
	if (!hash_tbl || !entry) {
		BNXT_TF_DBG(ERR, "invalid arguments\n");
		return -EINVAL;
	}

	idx = ULP_HASH_GET_H_INDEX(entry->hash_index);
	if (idx > (hash_tbl->hash_tbl_size * hash_tbl->hash_bkt_num)) {
		BNXT_TF_DBG(ERR, "invalid hash index %x\n", idx);
		return -EINVAL;
	}
	bucket = (uint16_t *)&hash_tbl->hash_list[idx];
	idx  = ULP_HASH_GET_B_INDEX(entry->hash_index);
	if (idx >= (hash_tbl->hash_bkt_num * ULP_HASH_BUCKET_ROW_SZ)) {
		BNXT_TF_DBG(ERR, "invalid bucket index %x\n", idx);
		return -EINVAL;
	}
	bucket += idx;
	if (ULP_HASH_BUCKET_INUSE(bucket)) {
		entry->key_idx = ULP_HASH_BUCKET_INDEX(bucket);
		entry->search_flag = ULP_GEN_HASH_SEARCH_FOUND;
	} else {
		entry->search_flag = ULP_GEN_HASH_SEARCH_MISSED;
		return -ENOENT;
	}
	return 0;
}

/*
 * Add the entry to the generic hash table
 *
 * hash_tbl [in] the pointer to hash table
 * entry [in/out] pointer to hash entry details. Fill the hash index and
 * key data details to be added.
 *
 * returns 0 on success
 *
 */
int32_t
ulp_gen_hash_tbl_list_add(struct ulp_gen_hash_tbl *hash_tbl,
			  struct ulp_gen_hash_entry_params *entry)
{
	int32_t rc = 0;
	uint16_t *bucket;
	uint32_t idx, key_index;

	/* add the entry */
	idx = ULP_HASH_GET_H_INDEX(entry->hash_index);
	bucket = (uint16_t *)&hash_tbl->hash_list[idx];
	bucket += ULP_HASH_GET_B_INDEX(entry->hash_index);
	if (ulp_bit_alloc_list_alloc(&hash_tbl->bit_list, &key_index)) {
		BNXT_TF_DBG(ERR, "Error in bit list alloc\n");
		return -ENOMEM;
	}
	if (key_index > hash_tbl->num_key_entries) {
		BNXT_TF_DBG(ERR, "reached max size %u:%u\n", key_index,
			    hash_tbl->num_key_entries);
		ulp_bit_alloc_list_dealloc(&hash_tbl->bit_list, key_index);
		return -ENOMEM;
	}
	/* Update the hash entry */
	ULP_HASH_BUCKET_MARK_INUSE(bucket, (uint16_t)key_index);

	/* update the hash key and key index */
	entry->key_idx = key_index;
	key_index = key_index * hash_tbl->key_tbl.data_size;
	memcpy(&hash_tbl->key_tbl.key_data[key_index], entry->key_data,
	       hash_tbl->key_tbl.data_size);

	return rc;
}

/*
 * Delete the entry in the generic hash table
 *
 * hash_tbl [in] the pointer to hash table
 * entry [in] pointer to hash entry details. Fill the hash index details to be
 * deleted.
 *
 * returns 0 on success
 */
int32_t
ulp_gen_hash_tbl_list_del(struct ulp_gen_hash_tbl *hash_tbl,
			  struct ulp_gen_hash_entry_params *entry)
{
	uint16_t *bucket;
	uint32_t idx, key_index;

	/* delete the entry */
	idx = ULP_HASH_GET_H_INDEX(entry->hash_index);
	bucket = (uint16_t *)&hash_tbl->hash_list[idx];
	bucket += ULP_HASH_GET_B_INDEX(entry->hash_index);

	/* Get the hash entry */
	key_index = ULP_HASH_BUCKET_INDEX(bucket);
	if (key_index >= hash_tbl->num_key_entries) {
		BNXT_TF_DBG(ERR, "Hash table corruption\n");
		return -EINVAL;
	}

	/* reset the bit in the bit allocator */
	if (ulp_bit_alloc_list_dealloc(&hash_tbl->bit_list,
				       key_index)) {
		BNXT_TF_DBG(ERR, "Error is bit list dealloc\n");
		return -EINVAL;
	}

	/* erase key details and bucket details */
	key_index = key_index * hash_tbl->key_tbl.data_size;
	memset(&hash_tbl->key_tbl.key_data[key_index], 0,
	       hash_tbl->key_tbl.data_size);
	ULP_HASH_BUCKET_CLEAR(bucket);

	return 0;
}
