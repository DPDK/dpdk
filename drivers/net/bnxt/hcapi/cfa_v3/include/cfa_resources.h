/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_RESOURCES_H_
#define _CFA_RESOURCES_H_

/**
 *  @addtogroup CFA_RESC_TYPES CFA Resource Types
 *  \ingroup CFA_V3
 *  CFA HW resource types and sub types definition
 *  @{
 */

/**
 * CFA hardware Resource Type
 *
 * Depending on the type of CFA hardware resource, the resources are divided
 * into multiple groups. This group is identified by resource type. The
 * following enum defines all the CFA resource types
 */
enum cfa_resource_type {
	/** CFA resources using fixed identifiers (IDM)
	 */
	CFA_RTYPE_IDENT = 0,
	/** CFA resources accessed by fixed indices (TBM)
	 */
	CFA_RTYPE_IDX_TBL,
	/** CFA TCAM resources
	 */
	CFA_RTYPE_TCAM,
	/** CFA interface tables (IFM)
	 */
	CFA_RTYPE_IF_TBL,
	/** CFA resources accessed using CFA memory manager index
	 */
	CFA_RTYPE_CMM,
	/** CFA Global fields (e.g. registers which configure global settings)
	 */
	CFA_RTYPE_GLB_FLD,

	CFA_RTYPE_HW_MAX = 12,

	/** FIrmware only types
	 */
	/** CFA Firmware Session Manager
	 */
	CFA_RTYPE_SM = CFA_RTYPE_HW_MAX,
	/** CFA Firmware Table Scope Manager
	 */
	CFA_RTYPE_TSM,
	/** CFA Firmware Table Scope Instance Manager
	 */
	CFA_RTYPE_TIM,
	/** CFA Firmware Global Id Manager
	 */
	CFA_RTYPE_GIM,

	CFA_RTYPE_MAX
};

/**
 * Resource sub-types for CFA_RTYPE_IDENT
 */
enum cfa_resource_subtype_ident {
	CFA_RSUBTYPE_IDENT_L2CTX = 0, /**< Remapped L2 contexts */
	CFA_RSUBTYPE_IDENT_PROF_FUNC, /**< Profile functions    */
	CFA_RSUBTYPE_IDENT_WC_PROF,   /**< WC TCAM profile IDs  */
	CFA_RSUBTYPE_IDENT_EM_PROF,   /**< EM profile IDs       */
	CFA_RSUBTYPE_IDENT_L2_FUNC,   /**< L2 functions         */
	CFA_RSUBTYPE_IDENT_LAG_ID,    /**< LAG IDs              */
	CFA_RSUBTYPE_IDENT_MAX
};

/**
 * Resource sub-types for CFA_RTYPE_IDX
 */
enum cfa_resource_subtype_idx_tbl {
	CFA_RSUBTYPE_IDX_TBL_STAT64 = 0,     /**< Statistics          */
	CFA_RSUBTYPE_IDX_TBL_METER_PROF,     /**< Meter profile       */
	CFA_RSUBTYPE_IDX_TBL_METER_INST,     /**< Meter instances     */
	CFA_RSUBTYPE_IDX_TBL_METER_DROP_CNT, /**< Meter Drop Count     */
	CFA_RSUBTYPE_IDX_TBL_MIRROR,	 /**< Mirror table        */
	/* Metadata mask for profiler block */
	CFA_RSUBTYPE_IDX_TBL_METADATA_PROF,
	/* Metadata mask for lookup block (for recycling) */
	CFA_RSUBTYPE_IDX_TBL_METADATA_LKUP,
	/* Metadata mask for action block */
	CFA_RSUBTYPE_IDX_TBL_METADATA_ACT,
	CFA_RSUBTYPE_IDX_TBL_CT_STATE,     /**< Connection tracking */
	CFA_RSUBTYPE_IDX_TBL_RANGE_PROF,   /**< Range profile       */
	CFA_RSUBTYPE_IDX_TBL_RANGE_ENTRY,  /**< Range entry         */
	CFA_RSUBTYPE_IDX_TBL_EM_FKB,       /**< EM FKB table        */
	CFA_RSUBTYPE_IDX_TBL_WC_FKB,       /**< WC TCAM FKB table   */
	CFA_RSUBTYPE_IDX_TBL_EM_FKB_MASK,  /**< EM FKB Mask table   */
	CFA_RSUBTYPE_IDX_TBL_MAX
};

/**
 * Resource sub-types for CFA_RTYPE_TCAM
 */
enum cfa_resource_subtype_tcam {
	CFA_RSUBTYPE_TCAM_L2CTX = 0,     /**< L2 contexts TCAM         */
	CFA_RSUBTYPE_TCAM_PROF_TCAM,     /**< Profile TCAM             */
	CFA_RSUBTYPE_TCAM_WC,		 /**< WC lookup TCAM           */
	CFA_RSUBTYPE_TCAM_CT_RULE,       /**< Connection tracking TCAM */
	CFA_RSUBTYPE_TCAM_VEB,		 /**< VEB TCAM                 */
	CFA_RSUBTYPE_TCAM_FEATURE_CHAIN, /**< Feature chain TCAM       */
	CFA_RSUBTYPE_TCAM_MAX
};

/**
 * Resource sub-types for CFA_RTYPE_IF_TBL
 */
enum cfa_resource_subtype_if_tbl {
	/** ILT table indexed by SVIF
	 */
	CFA_RSUBTYPE_IF_TBL_ILT = 0,
	/** VSPT table
	 */
	CFA_RSUBTYPE_IF_TBL_VSPT,
	/** Profiler partition default action record pointer
	 */
	CFA_RSUBTYPE_IF_TBL_PROF_PARIF_DFLT_ACT_PTR,
	/** Profiler partition error action record pointer
	 */
	CFA_RSUBTYPE_IF_TBL_PROF_PARIF_ERR_ACT_PTR,
	CFA_RSUBTYPE_IF_TBL_EPOCH0, /**< Epoch0 mask table   */
	CFA_RSUBTYPE_IF_TBL_EPOCH1, /**< Epoch1 mask table   */
	CFA_RSUBTYPE_IF_TBL_LAG,    /**< LAG Table           */
	CFA_RSUBTYPE_IF_TBL_MAX
};

/**
 * Resource sub-types for CFA_RTYPE_CMM
 */
enum cfa_resource_subtype_cmm {
	CFA_RSUBTYPE_CMM_INT_ACT_B0 = 0, /**< SRAM Bank 0    */
	CFA_RSUBTYPE_CMM_INT_ACT_B1,     /**< SRAM Bank 0    */
	CFA_RSUBTYPE_CMM_INT_ACT_B2,     /**< SRAM Bank 0    */
	CFA_RSUBTYPE_CMM_INT_ACT_B3,     /**< SRAM Bank 0    */
	CFA_RSUBTYPE_CMM_ACT,		 /**< Action table    */
	CFA_RSUBTYPE_CMM_LKUP,		 /**< EM lookup table */
	CFA_RSUBTYPE_CMM_MAX
};

#define CFA_RSUBTYPE_GLB_FLD_MAX 1
#define CFA_RSUBTYPE_SM_MAX 1
#define CFA_RSUBTYPE_TSM_MAX 1
#define CFA_RSUBTYPE_TIM_MAX 1

/**
 * Resource sub-types for CFA_RTYPE_GIM
 */
enum cfa_resource_subtype_gim {
	CFA_RSUBTYPE_GIM_DOMAIN_0 = 0, /**< Domain 0       */
	CFA_RSUBTYPE_GIM_DOMAIN_1,     /**< Domain 1       */
	CFA_RSUBTYPE_GIM_DOMAIN_2,     /**< Domain 2       */
	CFA_RSUBTYPE_GIM_DOMAIN_3,     /**< Domain 3       */
	CFA_RSUBTYPE_GIM_MAX
};

/**
 * Total number of resource subtypes
 */
#define CFA_NUM_RSUBTYPES                                                      \
	(CFA_RSUBTYPE_IDENT_MAX + CFA_RSUBTYPE_IDX_TBL_MAX +                   \
	 CFA_RSUBTYPE_TCAM_MAX + CFA_RSUBTYPE_IF_TBL_MAX +                     \
	 CFA_RSUBTYPE_CMM_MAX + CFA_RSUBTYPE_GLB_FLD_MAX +                     \
	 CFA_RSUBTYPE_SM_MAX + CFA_RSUBTYPE_TSM_MAX + CFA_RSUBTYPE_TIM_MAX +   \
	 CFA_RSUBTYPE_GIM_MAX)

/**
 * @}
 */

#endif /* _CFA_RESOURCES_H_ */
