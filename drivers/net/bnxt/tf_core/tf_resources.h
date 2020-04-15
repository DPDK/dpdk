/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _TF_RESOURCES_H_
#define _TF_RESOURCES_H_

/*
 * Hardware specific MAX values
 * NOTE: Should really come from the chip_cfg.h in some MAX form or HCAPI
 */

/** HW Resource types
 */
enum tf_resource_type_hw {
	/* Common HW resources for all chip variants */
	TF_RESC_TYPE_HW_L2_CTXT_TCAM,
	TF_RESC_TYPE_HW_PROF_FUNC,
	TF_RESC_TYPE_HW_PROF_TCAM,
	TF_RESC_TYPE_HW_EM_PROF_ID,
	TF_RESC_TYPE_HW_EM_REC,
	TF_RESC_TYPE_HW_WC_TCAM_PROF_ID,
	TF_RESC_TYPE_HW_WC_TCAM,
	TF_RESC_TYPE_HW_METER_PROF,
	TF_RESC_TYPE_HW_METER_INST,
	TF_RESC_TYPE_HW_MIRROR,
	TF_RESC_TYPE_HW_UPAR,
	/* Wh+/Brd2 specific HW resources */
	TF_RESC_TYPE_HW_SP_TCAM,
	/* Brd2/Brd4 specific HW resources */
	TF_RESC_TYPE_HW_L2_FUNC,
	/* Brd3, Brd4 common HW resources */
	TF_RESC_TYPE_HW_FKB,
	/* Brd4 specific HW resources */
	TF_RESC_TYPE_HW_TBL_SCOPE,
	TF_RESC_TYPE_HW_EPOCH0,
	TF_RESC_TYPE_HW_EPOCH1,
	TF_RESC_TYPE_HW_METADATA,
	TF_RESC_TYPE_HW_CT_STATE,
	TF_RESC_TYPE_HW_RANGE_PROF,
	TF_RESC_TYPE_HW_RANGE_ENTRY,
	TF_RESC_TYPE_HW_LAG_ENTRY,
	TF_RESC_TYPE_HW_MAX
};
#endif /* _TF_RESOURCES_H_ */
