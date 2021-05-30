/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_GEN_HASH_H_
#define _ULP_GEN_HASH_H_

#include "bnxt.h"

#define ULP_GEN_HASH_MAX_TBL_SIZE	BIT(15)

/* Structure to store the hash key details */
struct ulp_gen_hash_key_entry {
	uint32_t		mem_size;
	uint32_t		data_size;
	uint8_t			*key_data;
};

/* Macros for bucket entries */
#define ULP_HASH_BUCKET_VALID	0x8000
#define ULP_HASH_BUCKET_IDX_MSK	0x7FFF
#define ULP_HASH_BUCKET_ROW_SZ	4
#define ULP_HASH_BUCKET_INUSE(x) ((*(x)) & (ULP_HASH_BUCKET_VALID))
#define ULP_HASH_BUCKET_MARK_INUSE(x, y)	\
	((*(x)) = ((y) & ULP_HASH_BUCKET_IDX_MSK) | (ULP_HASH_BUCKET_VALID))
#define ULP_HASH_BUCKET_CLEAR(x) ((*(x)) = 0)
#define ULP_HASH_BUCKET_INDEX(x) ((*(x)) & (ULP_HASH_BUCKET_IDX_MSK))
#define ULP_HASH_INDEX_CALC(id1, id2) (((id1) << 16) | ((id2) & 0xFFFF))
#define ULP_HASH_GET_H_INDEX(x) (((x) >> 16) & 0xFFFF)
#define ULP_HASH_GET_B_INDEX(x) ((x) & 0xFFFF)
#define ULP_HASH_BUCKET_INVAL -1

/* Structure for the hash bucket details */
struct ulp_hash_bucket_entry {
	uint64_t		*bucket;
};

/* Structure for the hash bucket details */
struct bit_alloc_list {
	uint32_t		bsize;
	uint64_t		*bdata;
};

/*
 * Structure to store the generic tbl container
 * The ref count and byte data contain list of "num_elem" elements.
 * The size of each entry in byte_data is of size byte_data_size.
 */
struct ulp_gen_hash_tbl {
	/* memory to store hash key */
	uint32_t			num_key_entries;
	struct ulp_gen_hash_key_entry	key_tbl;

	/* Hash table memory */
	uint32_t			hash_tbl_size;
	uint32_t			hash_bkt_num;
	struct ulp_hash_bucket_entry	*hash_list;
	uint32_t			hash_mask;

	/* Bit allocator - to allocate key_res index */
	struct bit_alloc_list		bit_list;
};

/* structure to pass hash creation params */
struct ulp_hash_create_params {
	/* this is size of the hash tbl - try to keep it to power of 2.*/
	uint32_t			num_hash_tbl_entries;
	/* Bucket size must be multiple of 4 */
	uint32_t			num_buckets;
	/* This is size of hash key and data - try to keep it to power of 2 */
	/* This value has to be less than 2^15 */
	uint32_t			num_key_entries;
	/* the size of the hash key in bytes */
	uint32_t			key_size;
};

enum ulp_gen_hash_search_flag {
	ULP_GEN_HASH_SEARCH_MISSED = 1,
	ULP_GEN_HASH_SEARCH_FOUND = 2,
	ULP_GEN_HASH_SEARCH_FULL = 3
};

/* structure to pass hash entry */
struct ulp_gen_hash_entry_params {
	uint8_t				*key_data;
	uint32_t			key_length;
	enum ulp_gen_hash_search_flag	search_flag;
	uint32_t			hash_index;
	uint32_t			key_idx;
};

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
			   struct ulp_gen_hash_tbl **hash_tbl);

/*
 * Free the generic hash table
 *
 * hash_tbl [in] the pointer to hash table
 *
 * returns 0 on success
 */
int32_t
ulp_gen_hash_tbl_list_deinit(struct ulp_gen_hash_tbl *hash_tbl);

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
				 struct ulp_gen_hash_entry_params *entry);

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
				   struct ulp_gen_hash_entry_params *entry);

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
			  struct ulp_gen_hash_entry_params *entry);

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
			  struct ulp_gen_hash_entry_params *entry);

#endif /* _ULP_GEN_HASH_H_ */
