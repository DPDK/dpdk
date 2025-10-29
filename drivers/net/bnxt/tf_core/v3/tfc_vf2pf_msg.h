/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Broadcom
 * All rights reserved.
 */
#ifndef _TFC_VF2PF_MSG_H_
#define _TFC_VF2PF_MSG_H_

#include <stdint.h>
#include <stdbool.h>
#include "cfa_types.h"
#include "tfc.h"
#include "hsi_struct_def_dpdk.h"

/* HWRM_OEM_CMD is used to transport the vf2pf commands and responses.
 * All commands will have a naming_authority set to PCI_SIG, oem_id set to
 * 0x14e4 and message_family set to TRUFLOW. The maximum size of the oem_data
 * is 104 bytes.  The response maximum size is 88 bytes.
 */

/** Truflow VF2PF message types
 */
enum tfc_vf2pf_type {
	TFC_VF2PF_TYPE_TBL_SCOPE_MEM_ALLOC_CFG_CMD = 1,
	TFC_VF2PF_TYPE_TBL_SCOPE_MEM_FREE_CMD,
	TFC_VF2PF_TYPE_TBL_SCOPE_PFID_QUERY_CMD,
	TFC_VF2PF_TYPE_TBL_SCOPE_POOL_ALLOC_CMD,
	TFC_VF2PF_TYPE_TBL_SCOPE_POOL_FREE_CMD,
};

/** Truflow VF2PF response status
 */
enum tfc_vf2pf_status {
	TFC_VF2PF_STATUS_OK = 0,
	TFC_VF2PF_STATUS_TSID_CFG_ERR = 1,
	TFC_VF2PF_STATUS_TSID_MEM_ALLOC_ERR = 2,
	TFC_VF2PF_STATUS_TSID_INVALID = 3,
	TFC_VF2PF_STATUS_TSID_NOT_CONFIGURED = 4,
	TFC_VF2PF_STATUS_NO_POOLS_AVAIL = 5,
	TFC_VF2PF_STATUS_FID_ERR = 6,
};

/**
 * Truflow VF2PF header used for all Truflow VF2PF cmds/responses
 */
struct tfc_vf2pf_hdr {
	/** use enum tfc_vf2pf_type */
	uint16_t type;
	/** VF fid */
	uint16_t fid;
};

/**
 * Truflow VF2PF Table Scope Memory allocate/config command
 */
struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_cmd {
	/** vf2pf header */
	struct tfc_vf2pf_hdr hdr;
	/** table scope identifier */
	uint8_t tsid;
	/** lkup static bucket count */
	uint8_t static_bucket_cnt_exp[CFA_DIR_MAX];
	/** maximum number of pools requested - 1 for non-shared */
	uint16_t max_pools;
	/** dynamic bucket count */
	uint32_t dynamic_bucket_cnt[CFA_DIR_MAX];
	/** lkup record count */
	uint32_t lkup_rec_cnt[CFA_DIR_MAX];
	/** action record count */
	uint32_t act_rec_cnt[CFA_DIR_MAX];
	/** lkup pool size expressed as log2(max_recs/max_pools) */
	uint8_t lkup_pool_sz_exp[CFA_DIR_MAX];
	/** action pool size expressed as log2(max_recs/max_pools) */
	uint8_t act_pool_sz_exp[CFA_DIR_MAX];
	/** start offset in 32B records of the lkup recs (after buckets) */
	uint32_t lkup_rec_start_offset[CFA_DIR_MAX];
	/** scope type non-shared, shared-app or global */
	enum cfa_scope_type scope_type;
};
/**
 * Truflow VF2PF Table Scope Memory allocate/config response
 */
struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_resp {
	/** vf2pf header copied from cmd */
	struct tfc_vf2pf_hdr hdr;
	/** status of request */
	enum tfc_vf2pf_status status;
	/** tsid allocated */
	uint8_t tsid;
};
/**
 * Truflow VF2PF Table Scope Memory free command
 */
struct tfc_vf2pf_tbl_scope_mem_free_cmd {
	/** vf2pf header */
	struct tfc_vf2pf_hdr hdr;
	/** table scope identifier */
	uint8_t tsid;
};

/**
 * Truflow VF2PF Table Scope Memory free response
 */
struct tfc_vf2pf_tbl_scope_mem_free_resp {
	/** vf2pf header copied from cmd */
	struct tfc_vf2pf_hdr hdr;
	/** status of request */
	enum tfc_vf2pf_status status;
	/** table scope identifier */
	uint8_t tsid;
};

/**
 * Truflow VF2PF Table Scope PFID query command
 */
struct tfc_vf2pf_tbl_scope_pfid_query_cmd {
	/** vf2pf header */
	struct tfc_vf2pf_hdr hdr;
};
/**
 * Truflow VF2PF Table Scope PFID query response
 */
struct tfc_vf2pf_pfid_query_resp {
	/** vf2pf header copied from cmd */
	struct tfc_vf2pf_hdr hdr;
	/** status of AFM/NIC flow tbl scope */
	enum tfc_vf2pf_status status;
	/** tsid used for AFM/NIC flow tbl scope */
	uint8_t tsid;
	/** lookup tbl pool size expressed as log2(max_recs/max_pools) */
	uint8_t lkup_pool_sz_exp[CFA_DIR_MAX];
	/** action tbl pool size expressed as log2(max_recs/max_pools) */
	uint8_t act_pool_sz_exp[CFA_DIR_MAX];
	/** lkup record start offset in 32B records */
	uint32_t lkup_rec_start_offset[CFA_DIR_MAX];
	/** maximum number of pools */
	uint16_t max_pools;
};

/**
 * Truflow VF2PF Table Scope pool alloc command
 */
struct tfc_vf2pf_tbl_scope_pool_alloc_cmd {
	/** vf2pf header */
	struct tfc_vf2pf_hdr hdr;
	/** table scope identifier */
	uint8_t tsid;
	/** direction RX or TX */
	enum cfa_dir dir;
	/** region lkup or action */
	enum cfa_region_type region;
};

/**
 * Truflow VF2PF Table Scope pool alloc response
 */
struct tfc_vf2pf_tbl_scope_pool_alloc_resp {
	/** vf2pf header copied from cmd */
	struct tfc_vf2pf_hdr hdr;
	/** status of pool allocation */
	enum tfc_vf2pf_status status;
	/* tbl scope identifier */
	uint8_t tsid;
	/* pool size expressed as log2(max_recs/max_pools) */
	uint8_t pool_sz_exp;
	/* pool_id allocated */
	uint16_t pool_id;
};

/**
 * Truflow VF2PF Table Scope pool free command
 */
struct tfc_vf2pf_tbl_scope_pool_free_cmd {
	/** vf2pf header */
	struct tfc_vf2pf_hdr hdr;
	/* direction RX or TX */
	enum cfa_dir dir;
	/* region lkup or action */
	enum cfa_region_type region;
	/* table scope id */
	uint8_t tsid;
	/* pool_id */
	uint16_t pool_id;
};

/**
 * Truflow VF2PF Table Scope pool free response
 */
struct tfc_vf2pf_tbl_scope_pool_free_resp {
	/** vf2pf header copied from cmd */
	struct tfc_vf2pf_hdr hdr;
	/** status of pool allocation */
	enum tfc_vf2pf_status status;
	/** table scope id */
	uint8_t tsid;
};

int
tfc_vf2pf_mem_alloc(struct tfc *tfcp,
		    struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_cmd *req,
		    struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_resp *resp);

int
tfc_vf2pf_mem_free(struct tfc *tfcp,
		   struct tfc_vf2pf_tbl_scope_mem_free_cmd *req,
		    struct tfc_vf2pf_tbl_scope_mem_free_resp *resp);

int
tfc_vf2pf_pool_alloc(struct tfc *tfcp,
		    struct tfc_vf2pf_tbl_scope_pool_alloc_cmd *req,
		    struct tfc_vf2pf_tbl_scope_pool_alloc_resp *resp);

int
tfc_vf2pf_pool_free(struct tfc *tfcp,
		    struct tfc_vf2pf_tbl_scope_pool_free_cmd *req,
		    struct tfc_vf2pf_tbl_scope_pool_free_resp *resp);

int
tfc_oem_cmd_process(struct tfc *tfcp,
		    uint32_t *oem_data,
		    uint32_t *resp,
		    uint16_t *resp_len);
#endif /* _TFC_VF2PF_MSG_H */
