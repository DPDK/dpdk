/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _HCAPI_CFA_H_
#define _HCAPI_CFA_H_

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>

#include "hcapi_cfa_defs.h"

#define INVALID_U64 (0xFFFFFFFFFFFFFFFFULL)
#define INVALID_U32 (0xFFFFFFFFUL)
#define INVALID_U16 (0xFFFFUL)
#define INVALID_U8 (0xFFUL)

struct hcapi_cfa_devops;

/**
 * CFA device information
 */
struct hcapi_cfa_devinfo {
	/** [out] CFA device ops function pointer table */
	const struct hcapi_cfa_devops *devops;
};

/**
 *  \defgroup CFA_HCAPI_DEVICE_API
 *  HCAPI used for writing to the hardware
 *  @{
 */

/** CFA device specific function hooks structure
 *
 * The following device hooks can be defined; unless noted otherwise, they are
 * optional and can be filled with a null pointer. The pupose of these hooks
 * to support CFA device operations for different device variants.
 */
struct hcapi_cfa_devops {
	/** calculate a key hash for the provided key_data
	 *
	 * This API computes hash for a key.
	 *
	 * @param[in] key_data
	 *   A pointer of the key data buffer
	 *
	 * @param[in] bitlen
	 *   Number of bits of the key data
	 *
	 * @return
	 *   0 for SUCCESS, negative value for FAILURE
	 */
	uint64_t (*hcapi_cfa_key_hash)(uint64_t *key_data, uint16_t bitlen);

int hcapi_cfa_action_hw_op(struct hcapi_cfa_hwop *op,
			   uint8_t *act_tbl,
			   struct hcapi_cfa_data *act_obj);
int hcapi_cfa_dev_hw_op(struct hcapi_cfa_hwop *op, uint16_t tbl_id,
			struct hcapi_cfa_data *obj_data);
int hcapi_cfa_rm_register_client(hcapi_cfa_rm_data_t *data,
				 const char *client_name,
				 int *client_id);
int hcapi_cfa_rm_unregister_client(hcapi_cfa_rm_data_t *data,
				   int client_id);
int hcapi_cfa_rm_query_resources(hcapi_cfa_rm_data_t *data,
				 int client_id,
				 uint16_t chnl_id,
				 struct hcapi_cfa_resc_req_db *req_db);
int hcapi_cfa_rm_query_resources_one(hcapi_cfa_rm_data_t *data,
				     int clien_id,
				     struct hcapi_cfa_resc_db *resc_db);
int hcapi_cfa_rm_reserve_resources(hcapi_cfa_rm_data_t *data,
				   int client_id,
				   struct hcapi_cfa_resc_req_db *resc_req,
				   struct hcapi_cfa_resc_db *resc_db);
int hcapi_cfa_rm_release_resources(hcapi_cfa_rm_data_t *data,
				   int client_id,
				   struct hcapi_cfa_resc_req_db *resc_req,
				   struct hcapi_cfa_resc_db *resc_db);
int hcapi_cfa_rm_initialize(hcapi_cfa_rm_data_t *data);

#if SUPPORT_CFA_HW_P4

int hcapi_cfa_p4_dev_hw_op(struct hcapi_cfa_hwop *op, uint16_t tbl_id,
			    struct hcapi_cfa_data *obj_data);
int hcapi_cfa_p4_prof_l2ctxt_hwop(struct hcapi_cfa_hwop *op,
				   struct hcapi_cfa_data *obj_data);
int hcapi_cfa_p4_prof_l2ctxtrmp_hwop(struct hcapi_cfa_hwop *op,
				      struct hcapi_cfa_data *obj_data);
int hcapi_cfa_p4_prof_tcam_hwop(struct hcapi_cfa_hwop *op,
				 struct hcapi_cfa_data *obj_data);
int hcapi_cfa_p4_prof_tcamrmp_hwop(struct hcapi_cfa_hwop *op,
				    struct hcapi_cfa_data *obj_data);
int hcapi_cfa_p4_wc_tcam_hwop(struct hcapi_cfa_hwop *op,
			       struct hcapi_cfa_data *obj_data);
int hcapi_cfa_p4_wc_tcam_rec_hwop(struct hcapi_cfa_hwop *op,
				   struct hcapi_cfa_data *obj_data);
int hcapi_cfa_p4_mirror_hwop(struct hcapi_cfa_hwop *op,
			     struct hcapi_cfa_data *mirror);
int hcapi_cfa_p4_global_cfg_hwop(struct hcapi_cfa_hwop *op,
				 uint32_t type,
				 struct hcapi_cfa_data *config);
/* SUPPORT_CFA_HW_P4 */
#elif SUPPORT_CFA_HW_P45
int hcapi_cfa_p45_mirror_hwop(struct hcapi_cfa_hwop *op,
			      struct hcapi_cfa_data *mirror);
int hcapi_cfa_p45_global_cfg_hwop(struct hcapi_cfa_hwop *op,
				  uint32_t type,
				  struct hcapi_cfa_data *config);
/* SUPPORT_CFA_HW_P45 */
#endif

/**
 *  HCAPI CFA device HW operation function callback definition
 *  This is standardized function callback hook to install different
 *  CFA HW table programming function callback.
 */

struct hcapi_cfa_tbl_cb {
	/**
	 * This function callback provides the functionality to read/write
	 * HW table entry from a HW table.
	 *
	 * @param[in] op
	 *   A pointer to the Hardware operation parameter
	 *
	 * @param[in] key_tbl
	 *   A pointer to the off-chip EM key table (applicable to EEM and
	 *   SR2 EM only), set to NULL for on-chip EM key table or WC
	 *   TCAM table.
	 *
	 * @param[in/out] key_obj
	 *   A pointer to the key data object for the hardware operation which
	 *   has the following contents:
	 *     1. key record memory offset (index to WC TCAM or EM key hash
	 *        value)
	 *     2. key data
	 *   When using the HWOP PUT, the key_obj holds the LREC and key to
	 *   be written.
	 *   When using the HWOP GET, the key_obj be populated with the LREC
	 *   and key which was specified by the key location object.
	 *
	 * @param[in/out] key_loc
	 *   When using the HWOP PUT, this is a pointer to the key location
	 *   data structure which holds the information of where the EM key
	 *   is stored.  It holds the bucket index and the data pointer of
	 *   a dynamic bucket that is chained to static bucket
	 *   When using the HWOP GET, this is a pointer to the key location
	 *   which should be retreved.
	 *
	 *   (valid for SR2 only).
	 * @return
	 *   0 for SUCCESS, negative value for FAILURE
	 */
	int (*hcapi_cfa_key_hw_op)(struct hcapi_cfa_hwop *op,
				   struct hcapi_cfa_key_tbl *key_tbl,
				   struct hcapi_cfa_key_data *key_data,
				   struct hcapi_cfa_key_loc *key_loc);
};

/*@}*/

extern const size_t CFA_RM_HANDLE_DATA_SIZE;

#if SUPPORT_CFA_HW_ALL
extern const struct hcapi_cfa_devops cfa_p4_devops;
extern const struct hcapi_cfa_devops cfa_p58_devops;

#elif defined(SUPPORT_CFA_HW_P4) && SUPPORT_CFA_HW_P4
extern const struct hcapi_cfa_devops cfa_p4_devops;
uint64_t hcapi_cfa_p4_key_hash(uint64_t *key_data, uint16_t bitlen);
/* SUPPORT_CFA_HW_P4 */
#elif defined(SUPPORT_CFA_HW_P58) && SUPPORT_CFA_HW_P58
extern const struct hcapi_cfa_devops cfa_p58_devops;
uint64_t hcapi_cfa_p58_key_hash(uint64_t *key_data, uint16_t bitlen);
/* SUPPORT_CFA_HW_P58 */
#endif

#endif /* HCAPI_CFA_H_ */
