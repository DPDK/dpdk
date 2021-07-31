/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _TF_DEVICE_P58_H_
#define _TF_DEVICE_P58_H_

#include "cfa_resource_types.h"
#include "tf_core.h"
#include "tf_rm.h"
#include "tf_if_tbl.h"
#include "tf_global_cfg.h"

struct tf_rm_element_cfg tf_ident_p58[TF_IDENT_TYPE_MAX] = {
	[TF_IDENT_TYPE_L2_CTXT_HIGH] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_L2_CTXT_REMAP_HIGH,
		0, 0, 0
	},
	[TF_IDENT_TYPE_L2_CTXT_LOW] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_L2_CTXT_REMAP_LOW,
		0, 0, 0
	},
	[TF_IDENT_TYPE_PROF_FUNC] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_PROF_FUNC,
		0, 0, 0
	},
	[TF_IDENT_TYPE_WC_PROF] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_WC_TCAM_PROF_ID,
		0, 0, 0
	},
	[TF_IDENT_TYPE_EM_PROF] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_EM_PROF_ID,
		0, 0, 0
	},
};

struct tf_rm_element_cfg tf_tcam_p58[TF_TCAM_TBL_TYPE_MAX] = {
	[TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_L2_CTXT_TCAM_HIGH,
		0, 0, 0
	},
	[TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_L2_CTXT_TCAM_LOW,
		0, 0, 0
	},
	[TF_TCAM_TBL_TYPE_PROF_TCAM] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_PROF_TCAM,
		0, 0, 0
	},
	[TF_TCAM_TBL_TYPE_WC_TCAM] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_WC_TCAM,
		0, 0, 0
	},
	[TF_TCAM_TBL_TYPE_VEB_TCAM] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_VEB_TCAM,
		0, 0, 0
	},
};

struct tf_rm_element_cfg tf_tbl_p58[TF_TBL_TYPE_MAX] = {
	[TF_TBL_TYPE_EM_FKB] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_EM_FKB,
		0, 0, 0
	},
	[TF_TBL_TYPE_WC_FKB] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_WC_FKB,
		0, 0, 0
	},
	[TF_TBL_TYPE_METER_PROF] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_METER_PROF,
		0, 0, 0
	},
	[TF_TBL_TYPE_METER_INST] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_METER,
		0, 0, 0
	},
	[TF_TBL_TYPE_MIRROR_CONFIG] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_MIRROR,
		0, 0, 0
	},
	/* Policy - ARs in bank 1 */
	[TF_TBL_TYPE_FULL_ACT_RECORD] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_PARENT,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_1,
		.slices          = 1,
	},
	[TF_TBL_TYPE_COMPACT_ACT_RECORD] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_FULL_ACT_RECORD,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_1,
		.slices          = 1,
	},
	/* Policy - Encaps in bank 2 */
	[TF_TBL_TYPE_ACT_ENCAP_8B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_PARENT,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 1,
	},
	[TF_TBL_TYPE_ACT_ENCAP_16B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 1,
	},
	[TF_TBL_TYPE_ACT_ENCAP_32B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 1,
	},
	[TF_TBL_TYPE_ACT_ENCAP_64B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 1,
	},
	/* Policy - Modify in bank 2 with Encaps */
	[TF_TBL_TYPE_ACT_MODIFY_8B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 1,
	},
	[TF_TBL_TYPE_ACT_MODIFY_16B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 1,
	},
	[TF_TBL_TYPE_ACT_MODIFY_32B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 1,
	},
	[TF_TBL_TYPE_ACT_MODIFY_64B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 1,
	},
	/* Policy - SP in bank 0 */
	[TF_TBL_TYPE_ACT_SP_SMAC] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_PARENT,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_0,
		.slices          = 1,
	},
	[TF_TBL_TYPE_ACT_SP_SMAC_IPV4] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_SP_SMAC,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_0,
		.slices          = 1,
	},
	[TF_TBL_TYPE_ACT_SP_SMAC_IPV6] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_SP_SMAC,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_0,
		.slices          = 1,
	},
	/* Policy - Stats in bank 3 */
	[TF_TBL_TYPE_ACT_STATS_64] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_PARENT,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_3,
		.slices          = 1,
	},
};

struct tf_rm_element_cfg tf_em_int_p58[TF_EM_TBL_TYPE_MAX] = {
	[TF_EM_TBL_TYPE_EM_RECORD] = {
		TF_RM_ELEM_CFG_HCAPI, CFA_RESOURCE_TYPE_P58_EM_REC,
		0, 0, 0
	},
};

struct tf_if_tbl_cfg tf_if_tbl_p58[TF_IF_TBL_TYPE_MAX] = {
	[TF_IF_TBL_TYPE_PROF_PARIF_DFLT_ACT_REC_PTR] = {
		TF_IF_TBL_CFG, CFA_P58_TBL_PROF_PARIF_DFLT_ACT_REC_PTR},
	[TF_IF_TBL_TYPE_PROF_PARIF_ERR_ACT_REC_PTR] = {
		TF_IF_TBL_CFG, CFA_P58_TBL_PROF_PARIF_ERR_ACT_REC_PTR},
	[TF_IF_TBL_TYPE_ILT] = {
		TF_IF_TBL_CFG, CFA_P58_TBL_ILT},
	[TF_IF_TBL_TYPE_VSPT] = {
		TF_IF_TBL_CFG, CFA_P58_TBL_VSPT},
};

struct tf_global_cfg_cfg tf_global_cfg_p58[TF_GLOBAL_CFG_TYPE_MAX] = {
	[TF_TUNNEL_ENCAP] = {
		TF_GLOBAL_CFG_CFG_HCAPI, TF_TUNNEL_ENCAP
	},
	[TF_ACTION_BLOCK] = {
		TF_GLOBAL_CFG_CFG_HCAPI, TF_ACTION_BLOCK
	},
	[TF_COUNTER_CFG] = {
		TF_GLOBAL_CFG_CFG_HCAPI, TF_COUNTER_CFG
	},
};
#endif /* _TF_DEVICE_P58_H_ */
