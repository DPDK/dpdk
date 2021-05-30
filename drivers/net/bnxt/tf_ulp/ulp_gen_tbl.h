/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
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

/* Structure to pass the generic table values across APIs */
struct ulp_mapper_gen_tbl_entry {
	uint32_t			*ref_count;
	uint32_t			byte_data_size;
	uint8_t				*byte_data;
	enum bnxt_ulp_byte_order	byte_order;
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
};

/* Structure to store the generic tbl container */
struct ulp_mapper_gen_tbl_list {
	const char			*gen_tbl_name;
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
 * mapper_data [in] Pointer to the mapper data and the generic table is
 * part of it
 *
 * returns 0 on success
 */
int32_t
ulp_mapper_generic_tbl_list_init(struct bnxt_ulp_mapper_data *mapper_data);

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
 * Set the data in the generic table entry
 *
 * entry [in] - generic table entry
 * len [in] - The length of the data in bits to be set
 * data [in] - pointer to the data to be used for setting the value.
 * data_size [in] - length of the data pointer in bytes.
 *
 * returns 0 on success
 */
int32_t
ulp_mapper_gen_tbl_entry_data_set(struct ulp_mapper_gen_tbl_entry *entry,
				  uint32_t len, uint8_t *data,
				  uint32_t data_size);

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
 * res [in] - Pointer to flow db resource entry
 *
 * returns 0 on success
 */
int32_t
ulp_mapper_gen_tbl_res_free(struct bnxt_ulp_context *ulp_ctx,
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

#endif /* _ULP_EN_TBL_H_ */
