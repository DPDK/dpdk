/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _HCAPI_CFA_H_
#define _HCAPI_CFA_H_

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "hcapi_cfa_defs.h"

#if CHIP_CFG == SR_A
#define SUPPORT_CFA_HW_P45  1
#undef SUPPORT_CFA_HW_P4
#define SUPPORT_CFA_HW_P4   0
#elif CHIP_CFG == CMB_A
#define SUPPORT_CFA_HW_P4  1
#else
#error "Chip not supported"
#endif

#if SUPPORT_CFA_HW_P4 && SUPPORT_CFA_HW_P58 && SUPPORT_CFA_HW_P59
#define SUPPORT_CFA_HW_ALL  1
#endif

/**
 * Index used for the sram_entries field
 */
enum hcapi_cfa_resc_type_sram {
	HCAPI_CFA_RESC_TYPE_SRAM_FULL_ACTION,
	HCAPI_CFA_RESC_TYPE_SRAM_MCG,
	HCAPI_CFA_RESC_TYPE_SRAM_ENCAP_8B,
	HCAPI_CFA_RESC_TYPE_SRAM_ENCAP_16B,
	HCAPI_CFA_RESC_TYPE_SRAM_ENCAP_64B,
	HCAPI_CFA_RESC_TYPE_SRAM_SP_SMAC,
	HCAPI_CFA_RESC_TYPE_SRAM_SP_SMAC_IPV4,
	HCAPI_CFA_RESC_TYPE_SRAM_SP_SMAC_IPV6,
	HCAPI_CFA_RESC_TYPE_SRAM_COUNTER_64B,
	HCAPI_CFA_RESC_TYPE_SRAM_NAT_SPORT,
	HCAPI_CFA_RESC_TYPE_SRAM_NAT_DPORT,
	HCAPI_CFA_RESC_TYPE_SRAM_NAT_S_IPV4,
	HCAPI_CFA_RESC_TYPE_SRAM_NAT_D_IPV4,
	HCAPI_CFA_RESC_TYPE_SRAM_MAX
};

/**
 * Index used for the hw_entries field in struct cfa_rm_db
 */
enum hcapi_cfa_resc_type_hw {
	/* common HW resources for all chip variants */
	HCAPI_CFA_RESC_TYPE_HW_L2_CTXT_TCAM,
	HCAPI_CFA_RESC_TYPE_HW_PROF_FUNC,
	HCAPI_CFA_RESC_TYPE_HW_PROF_TCAM,
	HCAPI_CFA_RESC_TYPE_HW_EM_PROF_ID,
	HCAPI_CFA_RESC_TYPE_HW_EM_REC,
	HCAPI_CFA_RESC_TYPE_HW_WC_TCAM_PROF_ID,
	HCAPI_CFA_RESC_TYPE_HW_WC_TCAM,
	HCAPI_CFA_RESC_TYPE_HW_METER_PROF,
	HCAPI_CFA_RESC_TYPE_HW_METER_INST,
	HCAPI_CFA_RESC_TYPE_HW_MIRROR,
	HCAPI_CFA_RESC_TYPE_HW_UPAR,
	/* Wh+/SR specific HW resources */
	HCAPI_CFA_RESC_TYPE_HW_SP_TCAM,
	/* Thor, SR2 common HW resources */
	HCAPI_CFA_RESC_TYPE_HW_FKB,
	/* SR specific HW resources */
	HCAPI_CFA_RESC_TYPE_HW_TBL_SCOPE,
	HCAPI_CFA_RESC_TYPE_HW_L2_FUNC,
	HCAPI_CFA_RESC_TYPE_HW_EPOCH0,
	HCAPI_CFA_RESC_TYPE_HW_EPOCH1,
	HCAPI_CFA_RESC_TYPE_HW_METADATA,
	HCAPI_CFA_RESC_TYPE_HW_CT_STATE,
	HCAPI_CFA_RESC_TYPE_HW_RANGE_PROF,
	HCAPI_CFA_RESC_TYPE_HW_RANGE_ENTRY,
	HCAPI_CFA_RESC_TYPE_HW_LAG_ENTRY,
	HCAPI_CFA_RESC_TYPE_HW_MAX
};

struct hcapi_cfa_key_result {
	uint64_t bucket_mem_ptr;
	uint8_t bucket_idx;
};

/* common CFA register access macros */
#define CFA_REG(x)		OFFSETOF(cfa_reg_t, cfa_##x)

#ifndef TF_REG_WR
#define TF_REG_WR(_p, x, y)  (*((uint32_t volatile *)(x)) = (y))
#endif
#ifndef TF_REG_RD
#define TF_REG_RD(_p, x)  (*((uint32_t volatile *)(x)))
#endif
#ifndef TF_CFA_REG_RD
#define TF_CFA_REG_RD(_p, x)	\
	TF_REG_RD(0, (uint32_t)(_p)->base_addr + CFA_REG(x))
#endif
#ifndef TF_CFA_REG_WR
#define TF_CFA_REG_WR(_p, x, y)	\
	TF_REG_WR(0, (uint32_t)(_p)->base_addr + CFA_REG(x), y)
#endif

/* Constants used by Resource Manager Registration*/
#define RM_CLIENT_NAME_MAX_LEN          32

/**
 *  Resource Manager Data Structures used for resource requests
 */
struct hcapi_cfa_resc_req_entry {
	uint16_t min;
	uint16_t max;
};

struct hcapi_cfa_resc_req {
	/* Wh+/SR specific onchip Action SRAM resources */
	/* Validity of each sram type is indicated by the
	 * corresponding sram type bit in the sram_resc_flags. When
	 * set to 1, the CFA sram resource type is valid and amount of
	 * resources for this type is reserved. Each sram resource
	 * pool is identified by the starting index and number of
	 * resources in the pool.
	 */
	uint32_t sram_resc_flags;
	struct hcapi_cfa_resc_req_entry sram_resc[HCAPI_CFA_RESC_TYPE_SRAM_MAX];

	/* Validity of each resource type is indicated by the
	 * corresponding resource type bit in the hw_resc_flags. When
	 * set to 1, the CFA resource type is valid and amount of
	 * resource of this type is reserved. Each resource pool is
	 * identified by the starting index and the number of
	 * resources in the pool.
	 */
	uint32_t hw_resc_flags;
	struct hcapi_cfa_resc_req_entry hw_resc[HCAPI_CFA_RESC_TYPE_HW_MAX];
};

struct hcapi_cfa_resc_req_db {
	struct hcapi_cfa_resc_req rx;
	struct hcapi_cfa_resc_req tx;
};

struct hcapi_cfa_resc_entry {
	uint16_t start;
	uint16_t stride;
	uint16_t tag;
};

struct hcapi_cfa_resc {
	/* Wh+/SR specific onchip Action SRAM resources */
	/* Validity of each sram type is indicated by the
	 * corresponding sram type bit in the sram_resc_flags. When
	 * set to 1, the CFA sram resource type is valid and amount of
	 * resources for this type is reserved. Each sram resource
	 * pool is identified by the starting index and number of
	 * resources in the pool.
	 */
	uint32_t sram_resc_flags;
	struct hcapi_cfa_resc_entry sram_resc[HCAPI_CFA_RESC_TYPE_SRAM_MAX];

	/* Validity of each resource type is indicated by the
	 * corresponding resource type bit in the hw_resc_flags. When
	 * set to 1, the CFA resource type is valid and amount of
	 * resource of this type is reserved. Each resource pool is
	 * identified by the starting index and the number of resources
	 * in the pool.
	 */
	uint32_t hw_resc_flags;
	struct hcapi_cfa_resc_entry hw_resc[HCAPI_CFA_RESC_TYPE_HW_MAX];
};

struct hcapi_cfa_resc_db {
	struct hcapi_cfa_resc rx;
	struct hcapi_cfa_resc tx;
};

/**
 * This is the main data structure used by the CFA Resource
 * Manager.  This data structure holds all the state and table
 * management information.
 */
typedef struct hcapi_cfa_rm_data {
	uint32_t dummy_data;
} hcapi_cfa_rm_data_t;

/* End RM support */

struct hcapi_cfa_devops;

struct hcapi_cfa_devinfo {
	uint8_t global_cfg_data[CFA_GLOBAL_CFG_DATA_SZ];
	struct hcapi_cfa_layout_tbl layouts;
	struct hcapi_cfa_devops *devops;
};

int hcapi_cfa_dev_bind(enum hcapi_cfa_ver hw_ver,
		       struct hcapi_cfa_devinfo *dev_info);

int hcapi_cfa_key_compile_layout(struct hcapi_cfa_key_template *key_template,
				 struct hcapi_cfa_key_layout *key_layout);
uint64_t hcapi_cfa_key_hash(uint64_t *key_data, uint16_t bitlen);
int
hcapi_cfa_action_compile_layout(struct hcapi_cfa_action_template *act_template,
				struct hcapi_cfa_action_layout *act_layout);
int hcapi_cfa_action_init_obj(uint64_t *act_obj,
			      struct hcapi_cfa_action_layout *act_layout);
int hcapi_cfa_action_compute_ptr(uint64_t *act_obj,
				 struct hcapi_cfa_action_layout *act_layout,
				 uint32_t base_ptr);

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
	 * @param[in] obj_data
	 *   A pointer to the HW data object for the hardware operation
	 *
	 * @return
	 *   0 for SUCCESS, negative value for FAILURE
	 */
	int (*hwop_cb)(struct hcapi_cfa_hwop *op,
		       struct hcapi_cfa_data *obj_data);
};

#endif  /* HCAPI_CFA_H_ */
