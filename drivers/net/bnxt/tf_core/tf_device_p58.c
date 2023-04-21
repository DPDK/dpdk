/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include <rte_common.h>

#include "cfa_resource_types.h"
#include "tf_device.h"
#include "tf_identifier.h"
#include "tf_tbl.h"
#include "tf_tcam.h"
#include "tf_tcam_shared.h"
#include "tf_em.h"
#include "tf_if_tbl.h"
#include "tfp.h"
#include "tf_msg_common.h"
#include "tf_tbl_sram.h"
#include "tf_util.h"

#define TF_DEV_P58_PARIF_MAX 16
#define TF_DEV_P58_PF_MASK 0xfUL

/* For print alignment, make all entries 8 chars in this table */
const char *tf_resource_str_p58[CFA_RESOURCE_TYPE_P58_LAST + 1] = {
	[CFA_RESOURCE_TYPE_P58_METER]              = "meter   ",
	[CFA_RESOURCE_TYPE_P58_SRAM_BANK_0]        = "sram_bk0",
	[CFA_RESOURCE_TYPE_P58_SRAM_BANK_1]        = "sram_bk1",
	[CFA_RESOURCE_TYPE_P58_SRAM_BANK_2]        = "sram_bk2",
	[CFA_RESOURCE_TYPE_P58_SRAM_BANK_3]        = "sram_bk3",
	[CFA_RESOURCE_TYPE_P58_L2_CTXT_TCAM_HIGH]  = "l2ctx_hi",
	[CFA_RESOURCE_TYPE_P58_L2_CTXT_TCAM_LOW]   = "l2ctx_lo",
	[CFA_RESOURCE_TYPE_P58_L2_CTXT_REMAP_HIGH] = "l2ctr_hi",
	[CFA_RESOURCE_TYPE_P58_L2_CTXT_REMAP_LOW]  = "l2ctr_lo",
	[CFA_RESOURCE_TYPE_P58_PROF_FUNC]          = "prf_func",
	[CFA_RESOURCE_TYPE_P58_PROF_TCAM]          = "prf_tcam",
	[CFA_RESOURCE_TYPE_P58_EM_PROF_ID]         = "em_prof ",
	[CFA_RESOURCE_TYPE_P58_WC_TCAM_PROF_ID]    = "wc_prof ",
	[CFA_RESOURCE_TYPE_P58_EM_REC]             = "em_rec  ",
	[CFA_RESOURCE_TYPE_P58_WC_TCAM]            = "wc_tcam ",
	[CFA_RESOURCE_TYPE_P58_METER_PROF]         = "mtr_prof",
	[CFA_RESOURCE_TYPE_P58_MIRROR]             = "mirror  ",
	[CFA_RESOURCE_TYPE_P58_EM_FKB]             = "em_fkb  ",
	[CFA_RESOURCE_TYPE_P58_WC_FKB]             = "wc_fkb  ",
	[CFA_RESOURCE_TYPE_P58_VEB_TCAM]           = "veb     ",
	[CFA_RESOURCE_TYPE_P58_METADATA]           = "metadata",
	[CFA_RESOURCE_TYPE_P58_METER_DROP_CNT]     = "meter_dc",
};

struct tf_rm_element_cfg tf_tbl_p58[TF_DIR_MAX][TF_TBL_TYPE_MAX] = {
	[TF_DIR_RX][TF_TBL_TYPE_EM_FKB] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_EM_FKB,
		0, 0
	},
	[TF_DIR_RX][TF_TBL_TYPE_WC_FKB] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_WC_FKB,
		0, 0
	},
	[TF_DIR_RX][TF_TBL_TYPE_METER_PROF] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_METER_PROF,
		0, 0
	},
	[TF_DIR_RX][TF_TBL_TYPE_METER_INST] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_METER,
		0, 0
	},
	[TF_DIR_RX][TF_TBL_TYPE_METER_DROP_CNT] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_METER_DROP_CNT,
		0, 0
	},
	[TF_DIR_RX][TF_TBL_TYPE_MIRROR_CONFIG] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_MIRROR,
		0, 0
	},
	[TF_DIR_RX][TF_TBL_TYPE_METADATA] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_METADATA,
		0, 0
	},
	/* Policy - ARs in bank 1 */
	[TF_DIR_RX][TF_TBL_TYPE_FULL_ACT_RECORD] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_PARENT,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_1,
		.slices          = 8,
	},
	[TF_DIR_RX][TF_TBL_TYPE_COMPACT_ACT_RECORD] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_FULL_ACT_RECORD,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_1,
		.slices          = 16,
	},
	/* Policy - Encaps in bank 2 */
	[TF_DIR_RX][TF_TBL_TYPE_ACT_ENCAP_8B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_PARENT,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 16,
	},
	[TF_DIR_RX][TF_TBL_TYPE_ACT_ENCAP_16B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 8,
	},
	[TF_DIR_RX][TF_TBL_TYPE_ACT_ENCAP_32B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 4,
	},
	[TF_DIR_RX][TF_TBL_TYPE_ACT_ENCAP_64B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 2,
	},
	[TF_DIR_RX][TF_TBL_TYPE_ACT_ENCAP_128B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 1,
	},
	/* Policy - Modify in bank 2 with Encaps */
	[TF_DIR_RX][TF_TBL_TYPE_ACT_MODIFY_8B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 16,
	},
	[TF_DIR_RX][TF_TBL_TYPE_ACT_MODIFY_16B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 8,
	},
	[TF_DIR_RX][TF_TBL_TYPE_ACT_MODIFY_32B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 4,
	},
	[TF_DIR_RX][TF_TBL_TYPE_ACT_MODIFY_64B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 2,
	},
	/* Policy - SP in bank 0 */
	[TF_DIR_RX][TF_TBL_TYPE_ACT_SP_SMAC] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_PARENT,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_0,
		.slices          = 16,
	},
	[TF_DIR_RX][TF_TBL_TYPE_ACT_SP_SMAC_IPV4] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_SP_SMAC,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_0,
		.slices          = 8,
	},
	[TF_DIR_RX][TF_TBL_TYPE_ACT_SP_SMAC_IPV6] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_SP_SMAC,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_0,
		.slices          = 4,
	},
	/* Policy - Stats in bank 3 */
	[TF_DIR_RX][TF_TBL_TYPE_ACT_STATS_64] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_PARENT,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_3,
		.slices          = 16,
	},
	[TF_DIR_TX][TF_TBL_TYPE_EM_FKB] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_EM_FKB,
		0, 0
	},
	[TF_DIR_TX][TF_TBL_TYPE_WC_FKB] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_WC_FKB,
		0, 0
	},
	[TF_DIR_TX][TF_TBL_TYPE_METER_PROF] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_METER_PROF,
		0, 0
	},
	[TF_DIR_TX][TF_TBL_TYPE_METER_INST] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_METER,
		0, 0
	},
	[TF_DIR_TX][TF_TBL_TYPE_METER_DROP_CNT] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_METER_DROP_CNT,
		0, 0
	},
	[TF_DIR_TX][TF_TBL_TYPE_MIRROR_CONFIG] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_MIRROR,
		0, 0
	},
	[TF_DIR_TX][TF_TBL_TYPE_METADATA] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P58_METADATA,
		0, 0
	},
	/* Policy - ARs in bank 1 */
	[TF_DIR_TX][TF_TBL_TYPE_FULL_ACT_RECORD] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_PARENT,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_1,
		.slices          = 8,
	},
	[TF_DIR_TX][TF_TBL_TYPE_COMPACT_ACT_RECORD] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_FULL_ACT_RECORD,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_1,
		.slices          = 16,
	},
	/* Policy - Encaps in bank 2 */
	[TF_DIR_TX][TF_TBL_TYPE_ACT_ENCAP_8B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_PARENT,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 16,
	},
	[TF_DIR_TX][TF_TBL_TYPE_ACT_ENCAP_16B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 8,
	},
	[TF_DIR_TX][TF_TBL_TYPE_ACT_ENCAP_32B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 4,
	},
	[TF_DIR_TX][TF_TBL_TYPE_ACT_ENCAP_64B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 2,
	},
	[TF_DIR_TX][TF_TBL_TYPE_ACT_ENCAP_128B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 1,
	},
	/* Policy - Modify in bank 2 with Encaps */
	[TF_DIR_TX][TF_TBL_TYPE_ACT_MODIFY_8B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 16,
	},
	[TF_DIR_TX][TF_TBL_TYPE_ACT_MODIFY_16B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 8,
	},
	[TF_DIR_TX][TF_TBL_TYPE_ACT_MODIFY_32B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 4,
	},
	[TF_DIR_TX][TF_TBL_TYPE_ACT_MODIFY_64B] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_ENCAP_8B,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
		.slices          = 2,
	},
	/* Policy - SP in bank 0 */
	[TF_DIR_TX][TF_TBL_TYPE_ACT_SP_SMAC] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_PARENT,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_0,
		.slices	         = 16,
	},
	[TF_DIR_TX][TF_TBL_TYPE_ACT_SP_SMAC_IPV4] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_SP_SMAC,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_0,
		.slices	         = 8,
	},
	[TF_DIR_TX][TF_TBL_TYPE_ACT_SP_SMAC_IPV6] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
		.parent_subtype  = TF_TBL_TYPE_ACT_SP_SMAC,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_0,
		.slices	         = 4,
	},
	/* Policy - Stats in bank 3 */
	[TF_DIR_TX][TF_TBL_TYPE_ACT_STATS_64] = {
		.cfg_type        = TF_RM_ELEM_CFG_HCAPI_BA_PARENT,
		.hcapi_type      = CFA_RESOURCE_TYPE_P58_SRAM_BANK_3,
		.slices          = 16,
	},
};

/**
 * Device specific function that retrieves the MAX number of HCAPI
 * types the device supports.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [out] max_types
 *   Pointer to the MAX number of HCAPI types supported
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
static int
tf_dev_p58_get_max_types(struct tf *tfp,
			 uint16_t *max_types)
{
	if (max_types == NULL || tfp == NULL)
		return -EINVAL;

	*max_types = CFA_RESOURCE_TYPE_P58_LAST + 1;

	return 0;
}
/**
 * Device specific function that retrieves a human readable
 * string to identify a CFA resource type.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] resource_id
 *   HCAPI CFA resource id
 *
 * [out] resource_str
 *   Resource string
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
static int
tf_dev_p58_get_resource_str(struct tf *tfp __rte_unused,
			    uint16_t resource_id,
			    const char **resource_str)
{
	if (resource_str == NULL)
		return -EINVAL;

	if (resource_id > CFA_RESOURCE_TYPE_P58_LAST)
		return -EINVAL;

	*resource_str = tf_resource_str_p58[resource_id];

	return 0;
}

/**
 * Device specific function that set the WC TCAM slices the
 * device supports.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] num_slices_per_row
 *   The WC TCAM row slice configuration
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
static int
tf_dev_p58_set_tcam_slice_info(struct tf *tfp,
			       enum tf_wc_num_slice num_slices_per_row)
{
	int rc;
	struct tf_session *tfs = NULL;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	switch (num_slices_per_row) {
	case TF_WC_TCAM_1_SLICE_PER_ROW:
	case TF_WC_TCAM_2_SLICE_PER_ROW:
	case TF_WC_TCAM_4_SLICE_PER_ROW:
		tfs->wc_num_slices_per_row = num_slices_per_row;
	break;
	default:
		return -EINVAL;
	}

	return 0;
}

/**
 * Device specific function that retrieves the TCAM slices the
 * device supports.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] type
 *   TF TCAM type
 *
 * [in] key_sz
 *   The key size
 *
 * [out] num_slices_per_row
 *   Pointer to the WC TCAM row slice configuration
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
static int
tf_dev_p58_get_tcam_slice_info(struct tf *tfp,
			       enum tf_tcam_tbl_type type,
			       uint16_t key_sz,
			       uint16_t *num_slices_per_row)
{
	int rc;
	struct tf_session *tfs = NULL;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

#define CFA_P58_WC_TCAM_SLICE_SIZE (24)
	if (type == TF_TCAM_TBL_TYPE_WC_TCAM) {
		if (key_sz <= 1 * CFA_P58_WC_TCAM_SLICE_SIZE)
			*num_slices_per_row = TF_WC_TCAM_1_SLICE_PER_ROW;
		else if (key_sz <= 2 * CFA_P58_WC_TCAM_SLICE_SIZE)
			*num_slices_per_row = TF_WC_TCAM_2_SLICE_PER_ROW;
		else if (key_sz <= 4 * CFA_P58_WC_TCAM_SLICE_SIZE)
			*num_slices_per_row = TF_WC_TCAM_4_SLICE_PER_ROW;
		else
			return -ENOTSUP;
	} else { /* for other type of tcam */
		*num_slices_per_row = 1;
	}

	return 0;
}

static int tf_dev_p58_get_mailbox(void)
{
	return TF_CHIMP_MB;
}

static int tf_dev_p58_word_align(uint16_t size)
{
	return ((((size) + 63) >> 6) * 8);
}

/**
 * Device specific function that retrieves the increment
 * required for certain table types in a shared session
 *
 * [in] tfp
 * tf handle
 *
 * [in/out] parms
 *   pointer to parms structure
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
static int tf_dev_p58_get_shared_tbl_increment(struct tf *tfp __rte_unused,
				struct tf_get_shared_tbl_increment_parms *parms)
{
	switch (parms->type) {
	case TF_TBL_TYPE_FULL_ACT_RECORD:
	case TF_TBL_TYPE_COMPACT_ACT_RECORD:
	case TF_TBL_TYPE_ACT_ENCAP_8B:
	case TF_TBL_TYPE_ACT_ENCAP_16B:
	case TF_TBL_TYPE_ACT_ENCAP_32B:
	case TF_TBL_TYPE_ACT_ENCAP_64B:
	case TF_TBL_TYPE_ACT_ENCAP_128B:
	case TF_TBL_TYPE_ACT_SP_SMAC:
	case TF_TBL_TYPE_ACT_SP_SMAC_IPV4:
	case TF_TBL_TYPE_ACT_SP_SMAC_IPV6:
	case TF_TBL_TYPE_ACT_STATS_64:
	case TF_TBL_TYPE_ACT_MODIFY_IPV4:
	case TF_TBL_TYPE_ACT_MODIFY_8B:
	case TF_TBL_TYPE_ACT_MODIFY_16B:
	case TF_TBL_TYPE_ACT_MODIFY_32B:
	case TF_TBL_TYPE_ACT_MODIFY_64B:
		parms->increment_cnt = 16;
		break;
	default:
		parms->increment_cnt = 1;
		break;
	}
	return 0;
}

/**
 * Indicates whether the index table type is SRAM managed
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] type
 *   Truflow index table type, e.g. TF_TYPE_FULL_ACT_RECORD
 *
 * Returns
 *   - (0) if the table is not managed by the SRAM manager
 *   - (1) if the table is managed by the SRAM manager
 */
static bool tf_dev_p58_is_sram_managed(struct tf *tfp __rte_unused,
				       enum tf_tbl_type type)
{
	switch (type) {
	case TF_TBL_TYPE_FULL_ACT_RECORD:
	case TF_TBL_TYPE_COMPACT_ACT_RECORD:
	case TF_TBL_TYPE_ACT_ENCAP_8B:
	case TF_TBL_TYPE_ACT_ENCAP_16B:
	case TF_TBL_TYPE_ACT_ENCAP_32B:
	case TF_TBL_TYPE_ACT_ENCAP_64B:
	case TF_TBL_TYPE_ACT_ENCAP_128B:
	case TF_TBL_TYPE_ACT_SP_SMAC:
	case TF_TBL_TYPE_ACT_SP_SMAC_IPV4:
	case TF_TBL_TYPE_ACT_SP_SMAC_IPV6:
	case TF_TBL_TYPE_ACT_STATS_64:
	case TF_TBL_TYPE_ACT_MODIFY_IPV4:
	case TF_TBL_TYPE_ACT_MODIFY_8B:
	case TF_TBL_TYPE_ACT_MODIFY_16B:
	case TF_TBL_TYPE_ACT_MODIFY_32B:
	case TF_TBL_TYPE_ACT_MODIFY_64B:
		return true;
	default:
		return false;
	}
}

#define TF_DEV_P58_BANK_SZ_64B 2048
/**
 * Get SRAM table information.
 *
 * Converts an internal RM allocated element offset to
 * a user address and vice versa.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] type
 *   Truflow index table type, e.g. TF_TYPE_FULL_ACT_RECORD
 *
 * [in/out] base
 *   Pointer to the Base address of the associated SRAM bank used for
 *   the type of record allocated.
 *
 * [in/out] shift
 *   Pointer to the factor to be used as a multiplier to translate
 *   between the RM units to the user address.  SRAM manages 128B entries
 *   Addresses must be shifted to an 8B address.
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
static int tf_dev_p58_get_sram_tbl_info(struct tf *tfp __rte_unused,
					void *db,
					enum tf_tbl_type type,
					uint16_t *base,
					uint16_t *shift)
{
	uint16_t hcapi_type;
	struct tf_rm_get_hcapi_parms parms;
	int rc;

	parms.rm_db = db;
	parms.subtype = type;
	parms.hcapi_type = &hcapi_type;

	rc = tf_rm_get_hcapi_type(&parms);
	if (rc) {
		*base = 0;
		*shift = 0;
		return 0;
	}

	switch (hcapi_type) {
	case CFA_RESOURCE_TYPE_P58_SRAM_BANK_0:
		*base = 0;
		*shift = 3;
		break;
	case CFA_RESOURCE_TYPE_P58_SRAM_BANK_1:
		*base = TF_DEV_P58_BANK_SZ_64B;
		*shift = 3;
		break;
	case CFA_RESOURCE_TYPE_P58_SRAM_BANK_2:
		*base = TF_DEV_P58_BANK_SZ_64B * 2;
		*shift = 3;
		break;
	case CFA_RESOURCE_TYPE_P58_SRAM_BANK_3:
		*base = TF_DEV_P58_BANK_SZ_64B * 3;
		*shift = 3;
		break;
	default:
		*base = 0;
		*shift = 0;
		break;
	}
	return 0;
}

/**
 * Device specific function that maps the hcapi resource types
 * to Truflow type.
 *
 * [in] hcapi_caps
 *   CFA resource type bitmap
 *
 * [out] ident_caps
 *   Pointer to identifier type bitmap
 *
 * [out] tcam_caps
 *   Pointer to tcam type bitmap
 *
 * [out] tbl_caps
 *   Pointer to table type bitmap
 *
 * [out] em_caps
 *   Pointer to em type bitmap
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
static int tf_dev_p58_map_hcapi_caps(uint64_t hcapi_caps,
				     uint32_t *ident_caps,
				     uint32_t *tcam_caps,
				     uint32_t *tbl_caps,
				     uint32_t *em_caps)
{
	uint32_t i;

	*ident_caps = 0;
	*tcam_caps = 0;
	*tbl_caps = 0;
	*em_caps = 0;

	for (i = 0; i <= CFA_RESOURCE_TYPE_P58_LAST; i++) {
		if (hcapi_caps & 1ULL << i) {
			switch (tf_hcapi_res_map_p58[i].module_type) {
			case TF_MODULE_TYPE_IDENTIFIER:
				*ident_caps |= tf_hcapi_res_map_p58[i].type_caps;
				break;
			case TF_MODULE_TYPE_TABLE:
				*tbl_caps |= tf_hcapi_res_map_p58[i].type_caps;
				break;
			case TF_MODULE_TYPE_TCAM:
				*tcam_caps |= tf_hcapi_res_map_p58[i].type_caps;
				break;
			case TF_MODULE_TYPE_EM:
				*em_caps |= tf_hcapi_res_map_p58[i].type_caps;
				break;
			default:
				return -EINVAL;
			}
		}
	}

	return 0;
}

/**
 * Device specific function that retrieve the sram resource
 *
 * [in] query
 *   Point to resources query result
 *
 * [out] sram_bank_caps
 *   Pointer to SRAM bank capabilities
 *
 * [out] dynamic_sram_capable
 *   Pointer to dynamic sram capable
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
static int tf_dev_p58_get_sram_resources(void *q,
					 uint32_t *sram_bank_caps,
					 bool *dynamic_sram_capable)
{
	uint32_t i;
	struct tf_rm_resc_req_entry *query = q;

	for (i = 0; i < CFA_RESOURCE_TYPE_P58_LAST + 1; i++) {
		switch (query[i].type) {
		case CFA_RESOURCE_TYPE_P58_SRAM_BANK_0:
			sram_bank_caps[0] = query[i].max;
			break;
		case CFA_RESOURCE_TYPE_P58_SRAM_BANK_1:
			sram_bank_caps[1] = query[i].max;
			break;
		case CFA_RESOURCE_TYPE_P58_SRAM_BANK_2:
			sram_bank_caps[2] = query[i].max;
			break;
		case CFA_RESOURCE_TYPE_P58_SRAM_BANK_3:
			sram_bank_caps[3] = query[i].max;
			break;
		default:
			break;
		}
	}

	*dynamic_sram_capable = false;

	return 0;
}

static int sram_bank_hcapi_type[] = {
	CFA_RESOURCE_TYPE_P58_SRAM_BANK_0,
	CFA_RESOURCE_TYPE_P58_SRAM_BANK_1,
	CFA_RESOURCE_TYPE_P58_SRAM_BANK_2,
	CFA_RESOURCE_TYPE_P58_SRAM_BANK_3
};

/**
 * Device specific function that set the sram policy
 *
 * [in] dir
 *   Receive or transmit direction
 *
 * [in] band_id
 *   SRAM bank id
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
static int tf_dev_p58_set_sram_policy(enum tf_dir dir,
				      enum tf_sram_bank_id *bank_id)
{
	struct tf_rm_element_cfg *rm_cfg = tf_tbl_p58[dir];
	uint8_t type;
	uint8_t parent[TF_SRAM_BANK_ID_MAX] = { 0xFF, 0xFF, 0xFF, 0xFF };

	for (type = TF_TBL_TYPE_FULL_ACT_RECORD;
			type <= TF_TBL_TYPE_ACT_MODIFY_64B; type++) {
		if (bank_id[type] >= TF_SRAM_BANK_ID_MAX)
			return -EINVAL;

		rm_cfg[type].hcapi_type = sram_bank_hcapi_type[bank_id[type]];
		if (parent[bank_id[type]] == 0xFF) {
			parent[bank_id[type]] = type;
			rm_cfg[type].cfg_type = TF_RM_ELEM_CFG_HCAPI_BA_PARENT;
			rm_cfg[type].parent_subtype = 0;
			if (rm_cfg[type].slices == 0)
				rm_cfg[type].slices = 1;
		} else {
			rm_cfg[type].cfg_type = TF_RM_ELEM_CFG_HCAPI_BA_CHILD;
			rm_cfg[type].parent_subtype = parent[bank_id[type]];
		}
	}

	return 0;
}

/**
 * Device specific function that get the sram policy
 *
 * [in] dir
 *   Receive or transmit direction
 *
 * [out] band_id
 *   pointer to SRAM bank id
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
static int tf_dev_p58_get_sram_policy(enum tf_dir dir,
				      enum tf_sram_bank_id *bank_id)
{
	struct tf_rm_element_cfg *rm_cfg = tf_tbl_p58[dir];
	uint8_t type;

	for (type = TF_TBL_TYPE_FULL_ACT_RECORD;
			type < TF_TBL_TYPE_ACT_MODIFY_64B + 1; type++)
		bank_id[type] = rm_cfg[type].hcapi_type - CFA_RESOURCE_TYPE_P58_SRAM_BANK_0;

	return 0;
}

/**
 * Truflow P58 device specific functions
 */
const struct tf_dev_ops tf_dev_ops_p58_init = {
	.tf_dev_get_max_types = tf_dev_p58_get_max_types,
	.tf_dev_get_resource_str = tf_dev_p58_get_resource_str,
	.tf_dev_set_tcam_slice_info = tf_dev_p58_set_tcam_slice_info,
	.tf_dev_get_tcam_slice_info = tf_dev_p58_get_tcam_slice_info,
	.tf_dev_alloc_ident = NULL,
	.tf_dev_free_ident = NULL,
	.tf_dev_search_ident = NULL,
	.tf_dev_get_ident_resc_info = NULL,
	.tf_dev_get_tbl_info = NULL,
	.tf_dev_is_sram_managed = tf_dev_p58_is_sram_managed,
	.tf_dev_alloc_ext_tbl = NULL,
	.tf_dev_alloc_tbl = NULL,
	.tf_dev_alloc_sram_tbl = NULL,
	.tf_dev_free_ext_tbl = NULL,
	.tf_dev_free_tbl = NULL,
	.tf_dev_free_sram_tbl = NULL,
	.tf_dev_set_tbl = NULL,
	.tf_dev_set_ext_tbl = NULL,
	.tf_dev_set_sram_tbl = NULL,
	.tf_dev_get_tbl = NULL,
	.tf_dev_get_sram_tbl = NULL,
	.tf_dev_get_bulk_tbl = NULL,
	.tf_dev_get_bulk_sram_tbl = NULL,
	.tf_dev_get_shared_tbl_increment = tf_dev_p58_get_shared_tbl_increment,
	.tf_dev_get_tbl_resc_info = NULL,
	.tf_dev_alloc_tcam = NULL,
	.tf_dev_free_tcam = NULL,
	.tf_dev_alloc_search_tcam = NULL,
	.tf_dev_set_tcam = NULL,
	.tf_dev_get_tcam = NULL,
	.tf_dev_get_tcam_resc_info = NULL,
	.tf_dev_insert_int_em_entry = NULL,
	.tf_dev_delete_int_em_entry = NULL,
	.tf_dev_insert_ext_em_entry = NULL,
	.tf_dev_delete_ext_em_entry = NULL,
	.tf_dev_get_em_resc_info = NULL,
	.tf_dev_alloc_tbl_scope = NULL,
	.tf_dev_map_tbl_scope = NULL,
	.tf_dev_map_parif = NULL,
	.tf_dev_free_tbl_scope = NULL,
	.tf_dev_set_if_tbl = NULL,
	.tf_dev_get_if_tbl = NULL,
	.tf_dev_set_global_cfg = NULL,
	.tf_dev_get_global_cfg = NULL,
	.tf_dev_get_mailbox = tf_dev_p58_get_mailbox,
	.tf_dev_word_align = NULL,
	.tf_dev_map_hcapi_caps = tf_dev_p58_map_hcapi_caps,
	.tf_dev_get_sram_resources = tf_dev_p58_get_sram_resources,
	.tf_dev_set_sram_policy = tf_dev_p58_set_sram_policy,
	.tf_dev_get_sram_policy = tf_dev_p58_get_sram_policy,
};

/**
 * Truflow P58 device specific functions
 */
const struct tf_dev_ops tf_dev_ops_p58 = {
	.tf_dev_get_max_types = tf_dev_p58_get_max_types,
	.tf_dev_get_resource_str = tf_dev_p58_get_resource_str,
	.tf_dev_set_tcam_slice_info = tf_dev_p58_set_tcam_slice_info,
	.tf_dev_get_tcam_slice_info = tf_dev_p58_get_tcam_slice_info,
	.tf_dev_alloc_ident = tf_ident_alloc,
	.tf_dev_free_ident = tf_ident_free,
	.tf_dev_search_ident = tf_ident_search,
	.tf_dev_get_ident_resc_info = tf_ident_get_resc_info,
	.tf_dev_is_sram_managed = tf_dev_p58_is_sram_managed,
	.tf_dev_get_tbl_info = tf_dev_p58_get_sram_tbl_info,
	.tf_dev_alloc_tbl = tf_tbl_alloc,
	.tf_dev_alloc_sram_tbl = tf_tbl_sram_alloc,
	.tf_dev_alloc_ext_tbl = tf_tbl_ext_alloc,
	.tf_dev_free_tbl = tf_tbl_free,
	.tf_dev_free_ext_tbl = tf_tbl_ext_free,
	.tf_dev_free_sram_tbl = tf_tbl_sram_free,
	.tf_dev_set_tbl = tf_tbl_set,
	.tf_dev_set_ext_tbl = tf_tbl_ext_common_set,
	.tf_dev_set_sram_tbl = tf_tbl_sram_set,
	.tf_dev_get_tbl = tf_tbl_get,
	.tf_dev_get_sram_tbl = tf_tbl_sram_get,
	.tf_dev_get_bulk_tbl = tf_tbl_bulk_get,
	.tf_dev_get_bulk_sram_tbl = tf_tbl_sram_bulk_get,
	.tf_dev_get_shared_tbl_increment = tf_dev_p58_get_shared_tbl_increment,
	.tf_dev_get_tbl_resc_info = tf_tbl_get_resc_info,
	.tf_dev_alloc_tcam = tf_tcam_shared_alloc,
	.tf_dev_free_tcam = tf_tcam_shared_free,
	.tf_dev_set_tcam = tf_tcam_shared_set,
	.tf_dev_get_tcam = tf_tcam_shared_get,
	.tf_dev_move_tcam = tf_tcam_shared_move_p58,
	.tf_dev_clear_tcam = tf_tcam_shared_clear,
	.tf_dev_get_tcam_resc_info = tf_tcam_get_resc_info,
	.tf_dev_insert_int_em_entry = tf_em_hash_insert_int_entry,
	.tf_dev_delete_int_em_entry = tf_em_hash_delete_int_entry,
	.tf_dev_move_int_em_entry = tf_em_move_int_entry,
	.tf_dev_insert_ext_em_entry = NULL,
	.tf_dev_delete_ext_em_entry = NULL,
	.tf_dev_get_em_resc_info = tf_em_get_resc_info,
	.tf_dev_alloc_tbl_scope = NULL,
	.tf_dev_map_tbl_scope = NULL,
	.tf_dev_map_parif = NULL,
	.tf_dev_free_tbl_scope = NULL,
	.tf_dev_set_if_tbl = tf_if_tbl_set,
	.tf_dev_get_if_tbl = tf_if_tbl_get,
	.tf_dev_set_global_cfg = tf_global_cfg_set,
	.tf_dev_get_global_cfg = tf_global_cfg_get,
	.tf_dev_get_mailbox = tf_dev_p58_get_mailbox,
	.tf_dev_word_align = tf_dev_p58_word_align,
	.tf_dev_cfa_key_hash = hcapi_cfa_p58_key_hash,
	.tf_dev_map_hcapi_caps = tf_dev_p58_map_hcapi_caps,
	.tf_dev_get_sram_resources = tf_dev_p58_get_sram_resources,
	.tf_dev_set_sram_policy = tf_dev_p58_set_sram_policy,
	.tf_dev_get_sram_policy = tf_dev_p58_get_sram_policy,
};
