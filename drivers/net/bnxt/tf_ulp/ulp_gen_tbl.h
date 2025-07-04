/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_GEN_TBL_H_
#define _ULP_GEN_TBL_H_

#include "ulp_gen_hash.h"

/* Macros for reference count manipulation */
#define ULP_GEN_TBL_REF_CNT_INC(entry) {*(entry)->ref_count += 1; }
#define ULP_GEN_TBL_REF_CNT_DEC(entry) {*(entry)->ref_count -= 1; }
#define ULP_GEN_TBL_REF_CNT(entry) (*(entry)->ref_count)

#define ULP_GEN_TBL_FID_OFFSET		0
#define ULP_GEN_TBL_FID_SIZE_BITS	32

enum ulp_gen_list_search_flag {
	ULP_GEN_LIST_SEARCH_MISSED = 1,
	ULP_GEN_LIST_SEARCH_FOUND = 2,
	ULP_GEN_LIST_SEARCH_FOUND_SUBSET = 3,
	ULP_GEN_LIST_SEARCH_FOUND_SUPERSET = 4,
	ULP_GEN_LIST_SEARCH_FULL = 5
};

/* Structure to pass the generic table values across APIs */
struct ulp_mapper_gen_tbl_entry {
	uint32_t			*ref_count;
	uint32_t			byte_data_size;
	uint8_t				*byte_data;
	enum bnxt_ulp_byte_order	byte_order;
	uint32_t			byte_key_size;
	uint8_t				*byte_key;
};

/*
 * Structure to store the generic tbl container
 * The ref count and byte data contain list of "num_elem" elements.
 * The size of each entry in byte_data is of size byte_data_size.
 */
struct ulp_mapper_gen_tbl_cont {
	uint32_t			num_elem;
	uint32_t			byte_data_size;
	enum bnxt_ulp_byte_order	byte_order;
	/* Reference count to track number of users*/
	uint32_t			*ref_count;
	/* First 4 bytes is either tcam_idx or fid and rest are identities */
	uint8_t				*byte_data;
	uint8_t				*byte_key;
	uint32_t			byte_key_ex_size;/* exact match size */
	uint32_t			byte_key_par_size; /*partial match */
	uint32_t			seq_cnt;
};

/* Structure to store the generic tbl container */
struct ulp_mapper_gen_tbl_list {
	const char			*gen_tbl_name;
	enum bnxt_ulp_gen_tbl_type	tbl_type;
	struct ulp_mapper_gen_tbl_cont	container;
	uint32_t			mem_data_size;
	uint8_t				*mem_data;
	struct ulp_gen_hash_tbl		*hash_tbl;
};

/* Forward declaration */
struct bnxt_ulp_mapper_data;
struct ulp_flow_db_res_params;

/*
 * Initialize the generic table list
 *
 * ulp_ctx [in] - Pointer to the ulp context
 * mapper_data [in] Pointer to the mapper data and the generic table is
 * part of it
 *
 * returns 0 on success
 */
int32_t
ulp_mapper_generic_tbl_list_init(struct bnxt_ulp_context *ulp_ctx,
				 struct bnxt_ulp_mapper_data *mapper_data);

/*
 * Free the generic table list
 *
 * mapper_data [in] Pointer to the mapper data and the generic table is
 * part of it
 *
 * returns 0 on success
 */
int32_t
ulp_mapper_generic_tbl_list_deinit(struct bnxt_ulp_mapper_data *mapper_data);

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
			     struct ulp_mapper_gen_tbl_entry *entry);

/*
 * utility function to calculate the table idx
 *
 * res_sub_type [in] - Resource sub type
 * dir [in] - direction
 *
 * returns None
 */
int32_t
ulp_mapper_gen_tbl_idx_calculate(uint32_t res_sub_type, uint32_t dir);

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
				  uint8_t *data, uint32_t data_size);

/*
 * Get the data in the generic table entry
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
				  uint32_t data_size);

/*
 * Free the generic table list resource
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
			    struct ulp_flow_db_res_params *res);

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
			      uint32_t tbl_idx, uint32_t ckey);

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
				  struct ulp_mapper_gen_tbl_entry *gen_tbl_ent);

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
				  struct ulp_mapper_gen_tbl_entry *ent);
/*
 * Perform simple list search
 *
 * tbl_list [in] - pointer to the generic table list
 * match_key [in] -  Key data that needs to be matched
 * key_idx [out] - returns key index .
 *
 * returns 0 on success.
 */
uint32_t
ulp_gen_tbl_simple_list_search(struct ulp_mapper_gen_tbl_list *tbl_list,
			       uint8_t *match_key,
			       uint32_t *key_idx);
#endif /* _ULP_EN_TBL_H_ */
