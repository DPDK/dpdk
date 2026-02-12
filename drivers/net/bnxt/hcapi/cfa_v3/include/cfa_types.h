/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_TYPES_H_
#define _CFA_TYPES_H_

/*!
 * \file
 * \brief Exported CFA data structures shared between host and firmware
 *  @{
 */

/** \defgroup CFA_V3 Common CFA Access Framework
 *
 * The primary goal of the CFA common HW access framework is to unify the CFA
 * resource management and hardware programming design for different CFA
 * applications so the CFA hardware can be properly shared with different
 * entities. This framework is collection of the following CFA resource
 * managers and Libraries listed below:
 *
 * 1. CFA Memory Manager
 * 2. CFA Object Instance Manager
 * 3. CFA Session Manager
 * 4. CFA TCAM Manager
 * 5. CFA Table Scope Manager
 * 6. CFA Hardware Access Library
 * 7. CFA Builder Library
 * 8. CFA Index table manager
 * 9. CFA Utilities Library
 *
 **/

/**
 * CFA HW version definition
 */
enum cfa_ver {
	CFA_P40 = 0, /**< CFA phase 4.0 */
	CFA_P45 = 1, /**< CFA phase 4.5 */
	CFA_P58 = 2, /**< CFA phase 5.8 */
	CFA_P59 = 3, /**< CFA phase 5.9 */
	CFA_P70 = 4, /**< CFA phase 7.0 */
	CFA_PMAX = 5
};

/**
 * CFA direction definition
 */
enum cfa_dir {
	CFA_DIR_RX = 0, /**< Receive */
	CFA_DIR_TX = 1, /**< Transmit */
	CFA_DIR_MAX = 2
};

/**
 * CFA Remap Table Type
 */
enum cfa_remap_tbl_type {
	CFA_REMAP_TBL_TYPE_NORMAL = 0,
	CFA_REMAP_TBL_TYPE_BYPASS,
	CFA_REMAP_TBL_TYPE_MAX
};

/**
 * CFA tracker types
 */
enum cfa_track_type {
	CFA_TRACK_TYPE_INVALID = 0, /** !< Invalid */
	CFA_TRACK_TYPE_SID,	 /** !< Tracked by session id */
	CFA_TRACK_TYPE_FIRST = CFA_TRACK_TYPE_SID,
	CFA_TRACK_TYPE_FID, /** !< Tracked by function id */
	CFA_TRACK_TYPE_MAX
};

/**
 * CFA Region Type
 */
enum cfa_region_type {
	CFA_REGION_TYPE_LKUP = 0,
	CFA_REGION_TYPE_ACT,
	CFA_REGION_TYPE_MAX
};

/**
 * CFA application type
 */
enum cfa_app_type {
	CFA_APP_TYPE_AFM = 0, /** !< AFM firmware */
	CFA_APP_TYPE_TF = 1,  /** !< TruFlow firmware */
	CFA_APP_TYPE_MAX = 2,
	CFA_APP_TYPE_INVALID = CFA_APP_TYPE_MAX,
};

/**
 * CFA table scope types
 */
enum cfa_scope_type {
	CFA_SCOPE_TYPE_NON_SHARED = 0,
	CFA_SCOPE_TYPE_SHARED_APP = 1,
	CFA_SCOPE_TYPE_GLOBAL = 2,
	CFA_SCOPE_TYPE_INVALID = 3
};

/**
 * CFA FID types
 */
enum cfa_fid_type {
	CFA_FID_TYPE_FID = 0,  /**< General */
	CFA_FID_TYPE_RFID = 1, /**< Representor */
	CFA_FID_TYPE_EFID = 2  /**< Endpoint */
};

/**
 * CFA srchm modes
 */
enum cfa_srch_mode {
	CFA_SRCH_MODE_FIRST = 0, /** !< Start new iteration */
	CFA_SRCH_MODE_NEXT,      /** !< Next item in iteration */
	CFA_SRCH_MODE_MAX
};

/** @} */

#endif /* _CFA_TYPES_H_ */
