/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#include <rte_common.h>

#include "cfa_resource_types.h"
#include "tf_device.h"
#include "tf_identifier.h"
#include "tf_tbl.h"
#include "tf_tcam.h"
#ifdef TF_TCAM_SHARED
#include "tf_tcam_shared.h"
#endif /* TF_TCAM_SHARED */
#include "tf_em.h"
#include "tf_if_tbl.h"
#include "tfp.h"
#include "tf_msg_common.h"

#define TF_DEV_P4_PARIF_MAX 16
#define TF_DEV_P4_PF_MASK 0xfUL

const char *tf_resource_str_p4[CFA_RESOURCE_TYPE_P4_LAST + 1] = {
	[CFA_RESOURCE_TYPE_P4_MCG] = "mc_group",
	[CFA_RESOURCE_TYPE_P4_ENCAP_8B] = "encap_8 ",
	[CFA_RESOURCE_TYPE_P4_ENCAP_16B] = "encap_16",
	[CFA_RESOURCE_TYPE_P4_ENCAP_64B] = "encap_64",
	[CFA_RESOURCE_TYPE_P4_SP_MAC] =	"sp_mac  ",
	[CFA_RESOURCE_TYPE_P4_SP_MAC_IPV4] = "sp_macv4",
	[CFA_RESOURCE_TYPE_P4_SP_MAC_IPV6] = "sp_macv6",
	[CFA_RESOURCE_TYPE_P4_COUNTER_64B] = "ctr_64b ",
	[CFA_RESOURCE_TYPE_P4_NAT_PORT] = "nat_port",
	[CFA_RESOURCE_TYPE_P4_NAT_IPV4] = "nat_ipv4",
	[CFA_RESOURCE_TYPE_P4_METER] = "meter   ",
	[CFA_RESOURCE_TYPE_P4_FLOW_STATE] = "flow_st ",
	[CFA_RESOURCE_TYPE_P4_FULL_ACTION] = "full_act",
	[CFA_RESOURCE_TYPE_P4_FORMAT_0_ACTION] = "fmt0_act",
	[CFA_RESOURCE_TYPE_P4_EXT_FORMAT_0_ACTION] = "ext0_act",
	[CFA_RESOURCE_TYPE_P4_FORMAT_1_ACTION] = "fmt1_act",
	[CFA_RESOURCE_TYPE_P4_FORMAT_2_ACTION] = "fmt2_act",
	[CFA_RESOURCE_TYPE_P4_FORMAT_3_ACTION] = "fmt3_act",
	[CFA_RESOURCE_TYPE_P4_FORMAT_4_ACTION] = "fmt4_act",
	[CFA_RESOURCE_TYPE_P4_FORMAT_5_ACTION] = "fmt5_act",
	[CFA_RESOURCE_TYPE_P4_FORMAT_6_ACTION] = "fmt6_act",
	[CFA_RESOURCE_TYPE_P4_L2_CTXT_TCAM_HIGH] = "l2ctx_hi",
	[CFA_RESOURCE_TYPE_P4_L2_CTXT_TCAM_LOW] = "l2ctx_lo",
	[CFA_RESOURCE_TYPE_P4_L2_CTXT_REMAP_HIGH] = "l2ctr_hi",
	[CFA_RESOURCE_TYPE_P4_L2_CTXT_REMAP_LOW] = "l2ctr_lo",
	[CFA_RESOURCE_TYPE_P4_PROF_FUNC] = "prf_func",
	[CFA_RESOURCE_TYPE_P4_PROF_TCAM] = "prf_tcam",
	[CFA_RESOURCE_TYPE_P4_EM_PROF_ID] = "em_prof ",
	[CFA_RESOURCE_TYPE_P4_EM_REC] = "em_rec  ",
	[CFA_RESOURCE_TYPE_P4_WC_TCAM_PROF_ID] = "wc_prof ",
	[CFA_RESOURCE_TYPE_P4_WC_TCAM] = "wc_tcam ",
	[CFA_RESOURCE_TYPE_P4_METER_PROF] = "mtr_prof",
	[CFA_RESOURCE_TYPE_P4_MIRROR] = "mirror  ",
	[CFA_RESOURCE_TYPE_P4_SP_TCAM] = "sp_tcam ",
	[CFA_RESOURCE_TYPE_P4_TBL_SCOPE] = "tb_scope",
};

struct tf_rm_element_cfg tf_tbl_p4[TF_DIR_MAX][TF_TBL_TYPE_MAX] = {
	[TF_DIR_RX][TF_TBL_TYPE_FULL_ACT_RECORD] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_FULL_ACTION,
		0, 0
	},
	[TF_DIR_RX][TF_TBL_TYPE_MCAST_GROUPS] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_MCG,
		0, 0
	},
	[TF_DIR_RX][TF_TBL_TYPE_ACT_ENCAP_8B] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_ENCAP_8B,
		0, 0
	},
	[TF_DIR_RX][TF_TBL_TYPE_ACT_ENCAP_16B] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_ENCAP_16B,
		0, 0
	},
	[TF_DIR_RX][TF_TBL_TYPE_ACT_ENCAP_64B] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_ENCAP_64B,
		0, 0
	},
	[TF_DIR_RX][TF_TBL_TYPE_ACT_SP_SMAC] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_SP_MAC,
		0, 0
	},
	[TF_DIR_RX][TF_TBL_TYPE_ACT_SP_SMAC_IPV4] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_SP_MAC_IPV4,
		0, 0
	},
	[TF_DIR_RX][TF_TBL_TYPE_ACT_SP_SMAC_IPV6] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_SP_MAC_IPV6,
		0, 0
	},
	[TF_DIR_RX][TF_TBL_TYPE_ACT_STATS_64] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_COUNTER_64B,
		0, 0
	},
	[TF_DIR_RX][TF_TBL_TYPE_ACT_MODIFY_IPV4] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_NAT_IPV4,
		0, 0
	},
	[TF_DIR_RX][TF_TBL_TYPE_METER_PROF] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_METER_PROF,
		0, 0
	},
	[TF_DIR_RX][TF_TBL_TYPE_METER_INST] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_METER,
		0, 0
	},
	[TF_DIR_RX][TF_TBL_TYPE_MIRROR_CONFIG] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_MIRROR,
		0, 0
	},
	[TF_DIR_TX][TF_TBL_TYPE_FULL_ACT_RECORD] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_FULL_ACTION,
		0, 0
	},
	[TF_DIR_TX][TF_TBL_TYPE_MCAST_GROUPS] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_MCG,
		0, 0
	},
	[TF_DIR_TX][TF_TBL_TYPE_ACT_ENCAP_8B] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_ENCAP_8B,
		0, 0
	},
	[TF_DIR_TX][TF_TBL_TYPE_ACT_ENCAP_16B] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_ENCAP_16B,
		0, 0
	},
	[TF_DIR_TX][TF_TBL_TYPE_ACT_ENCAP_64B] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_ENCAP_64B,
		0, 0
	},
	[TF_DIR_TX][TF_TBL_TYPE_ACT_SP_SMAC] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_SP_MAC,
		0, 0
	},
	[TF_DIR_TX][TF_TBL_TYPE_ACT_SP_SMAC_IPV4] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_SP_MAC_IPV4,
		0, 0
	},
	[TF_DIR_TX][TF_TBL_TYPE_ACT_SP_SMAC_IPV6] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_SP_MAC_IPV6,
		0, 0
	},
	[TF_DIR_TX][TF_TBL_TYPE_ACT_STATS_64] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_COUNTER_64B,
		0, 0
	},
	[TF_DIR_TX][TF_TBL_TYPE_ACT_MODIFY_IPV4] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_NAT_IPV4,
		0, 0
	},
	[TF_DIR_TX][TF_TBL_TYPE_METER_PROF] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_METER_PROF,
		0, 0
	},
	[TF_DIR_TX][TF_TBL_TYPE_METER_INST] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_METER,
		0, 0
	},
	[TF_DIR_TX][TF_TBL_TYPE_MIRROR_CONFIG] = {
		TF_RM_ELEM_CFG_HCAPI_BA, CFA_RESOURCE_TYPE_P4_MIRROR,
		0, 0
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
 *   Pointer to the MAX number of CFA resource types supported
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
static int
tf_dev_p4_get_max_types(struct tf *tfp,
			uint16_t *max_types)
{
	if (max_types == NULL || tfp == NULL)
		return -EINVAL;

	*max_types = CFA_RESOURCE_TYPE_P4_LAST + 1;

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
tf_dev_p4_get_resource_str(struct tf *tfp __rte_unused,
			   uint16_t resource_id,
			   const char **resource_str)
{
	if (resource_str == NULL)
		return -EINVAL;

	if (resource_id > CFA_RESOURCE_TYPE_P4_LAST)
		return -EINVAL;

	*resource_str = tf_resource_str_p4[resource_id];

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
tf_dev_p4_set_tcam_slice_info(struct tf *tfp,
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
tf_dev_p4_get_tcam_slice_info(struct tf *tfp,
			      enum tf_tcam_tbl_type type,
			      uint16_t key_sz,
			      uint16_t *num_slices_per_row)
{
	int rc;
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

/* Single slice support */
#define CFA_P4_WC_TCAM_SLICE_SIZE     12

	if (type == TF_TCAM_TBL_TYPE_WC_TCAM) {
		*num_slices_per_row = tfs->wc_num_slices_per_row;
		if (key_sz > *num_slices_per_row * CFA_P4_WC_TCAM_SLICE_SIZE)
			return -ENOTSUP;
	} else { /* for other type of tcam */
		*num_slices_per_row = 1;
	}

	return 0;
}

static int
tf_dev_p4_map_parif(struct tf *tfp __rte_unused,
		    uint16_t parif_bitmask,
		    uint16_t pf,
		    uint8_t *data,
		    uint8_t *mask,
		    uint16_t sz_in_bytes)
{
	uint32_t parif_pf[2] = { 0 };
	uint32_t parif_pf_mask[2] = { 0 };
	uint32_t parif;
	uint32_t shift;

	if (sz_in_bytes != sizeof(uint64_t))
		return -ENOTSUP;

	for (parif = 0; parif < TF_DEV_P4_PARIF_MAX; parif++) {
		if (parif_bitmask & (1UL << parif)) {
			if (parif < 8) {
				shift = 4 * parif;
				parif_pf_mask[0] |= TF_DEV_P4_PF_MASK << shift;
				parif_pf[0] |= pf << shift;
			} else {
				shift = 4 * (parif - 8);
				parif_pf_mask[1] |= TF_DEV_P4_PF_MASK << shift;
				parif_pf[1] |= pf << shift;
			}
		}
	}
	tfp_memcpy(data, parif_pf, sz_in_bytes);
	tfp_memcpy(mask, parif_pf_mask, sz_in_bytes);

	return 0;
}

/**
 * Device specific function that retrieves the increment
 * required for certain table types in a shared session
 *
 * [in] tfp
 *  tf handle
 *
 * [in/out] parms
 *   pointer to parms structure
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
static int tf_dev_p4_get_shared_tbl_increment(struct tf *tfp __rte_unused,
				struct tf_get_shared_tbl_increment_parms *parms)
{
	parms->increment_cnt = 1;
	return 0;
}
static int tf_dev_p4_get_mailbox(void)
{
	return TF_KONG_MB;
}

static int tf_dev_p4_word_align(uint16_t size)
{
	return ((((size) + 31) >> 5) * 4);
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
static bool tf_dev_p4_is_sram_managed(struct tf *tfp __rte_unused,
				      enum tf_tbl_type type __rte_unused)
{
	return false;
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
static int tf_dev_p4_map_hcapi_caps(uint64_t hcapi_caps,
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

	for (i = 0; i <= CFA_RESOURCE_TYPE_P4_LAST; i++) {
		if (hcapi_caps & 1ULL << i) {
			switch (tf_hcapi_res_map_p4[i].module_type) {
			case TF_MODULE_TYPE_IDENTIFIER:
				*ident_caps |= tf_hcapi_res_map_p4[i].type_caps;
				break;
			case TF_MODULE_TYPE_TABLE:
				*tbl_caps |= tf_hcapi_res_map_p4[i].type_caps;
				break;
			case TF_MODULE_TYPE_TCAM:
				*tcam_caps |= tf_hcapi_res_map_p4[i].type_caps;
				break;
			case TF_MODULE_TYPE_EM:
				*em_caps |= tf_hcapi_res_map_p4[i].type_caps;
				break;
			default:
				return -EINVAL;
			}
		}
	}

	return 0;
}

/**
 * Truflow P4 device specific functions
 */
const struct tf_dev_ops tf_dev_ops_p4_init = {
	.tf_dev_get_max_types = tf_dev_p4_get_max_types,
	.tf_dev_get_resource_str = tf_dev_p4_get_resource_str,
	.tf_dev_set_tcam_slice_info = tf_dev_p4_set_tcam_slice_info,
	.tf_dev_get_tcam_slice_info = tf_dev_p4_get_tcam_slice_info,
	.tf_dev_alloc_ident = NULL,
	.tf_dev_free_ident = NULL,
	.tf_dev_search_ident = NULL,
	.tf_dev_get_ident_resc_info = NULL,
	.tf_dev_get_tbl_info = NULL,
	.tf_dev_is_sram_managed = tf_dev_p4_is_sram_managed,
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
	.tf_dev_get_shared_tbl_increment = tf_dev_p4_get_shared_tbl_increment,
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
	.tf_dev_get_mailbox = tf_dev_p4_get_mailbox,
	.tf_dev_word_align = NULL,
	.tf_dev_map_hcapi_caps = tf_dev_p4_map_hcapi_caps,
	.tf_dev_get_sram_resources = NULL,
	.tf_dev_set_sram_policy = NULL,
	.tf_dev_get_sram_policy = NULL,
};

/**
 * Truflow P4 device specific functions
 */
const struct tf_dev_ops tf_dev_ops_p4 = {
	.tf_dev_get_max_types = tf_dev_p4_get_max_types,
	.tf_dev_get_resource_str = tf_dev_p4_get_resource_str,
	.tf_dev_set_tcam_slice_info = tf_dev_p4_set_tcam_slice_info,
	.tf_dev_get_tcam_slice_info = tf_dev_p4_get_tcam_slice_info,
	.tf_dev_alloc_ident = tf_ident_alloc,
	.tf_dev_free_ident = tf_ident_free,
	.tf_dev_search_ident = tf_ident_search,
	.tf_dev_get_ident_resc_info = tf_ident_get_resc_info,
	.tf_dev_get_tbl_info = NULL,
	.tf_dev_is_sram_managed = tf_dev_p4_is_sram_managed,
	.tf_dev_alloc_tbl = tf_tbl_alloc,
	.tf_dev_alloc_ext_tbl = tf_tbl_ext_alloc,
	.tf_dev_alloc_sram_tbl = tf_tbl_alloc,
	.tf_dev_free_tbl = tf_tbl_free,
	.tf_dev_free_ext_tbl = tf_tbl_ext_free,
	.tf_dev_free_sram_tbl = tf_tbl_free,
	.tf_dev_set_tbl = tf_tbl_set,
	.tf_dev_set_ext_tbl = tf_tbl_ext_common_set,
	.tf_dev_set_sram_tbl = NULL,
	.tf_dev_get_tbl = tf_tbl_get,
	.tf_dev_get_sram_tbl = NULL,
	.tf_dev_get_bulk_tbl = tf_tbl_bulk_get,
	.tf_dev_get_bulk_sram_tbl = NULL,
	.tf_dev_get_shared_tbl_increment = tf_dev_p4_get_shared_tbl_increment,
	.tf_dev_get_tbl_resc_info = tf_tbl_get_resc_info,
#ifdef TF_TCAM_SHARED
	.tf_dev_alloc_tcam = tf_tcam_shared_alloc,
	.tf_dev_free_tcam = tf_tcam_shared_free,
	.tf_dev_set_tcam = tf_tcam_shared_set,
	.tf_dev_get_tcam = tf_tcam_shared_get,
	.tf_dev_move_tcam = tf_tcam_shared_move_p4,
	.tf_dev_clear_tcam = tf_tcam_shared_clear,
#else /* !TF_TCAM_SHARED */
	.tf_dev_alloc_tcam = tf_tcam_alloc,
	.tf_dev_free_tcam = tf_tcam_free,
	.tf_dev_set_tcam = tf_tcam_set,
	.tf_dev_get_tcam = tf_tcam_get,
#endif
	.tf_dev_alloc_search_tcam = tf_tcam_alloc_search,
	.tf_dev_get_tcam_resc_info = tf_tcam_get_resc_info,
	.tf_dev_insert_int_em_entry = tf_em_insert_int_entry,
	.tf_dev_delete_int_em_entry = tf_em_delete_int_entry,
	.tf_dev_insert_ext_em_entry = tf_em_insert_ext_entry,
	.tf_dev_delete_ext_em_entry = tf_em_delete_ext_entry,
	.tf_dev_get_em_resc_info = tf_em_get_resc_info,
	.tf_dev_alloc_tbl_scope = tf_em_ext_common_alloc,
	.tf_dev_map_tbl_scope = tf_em_ext_map_tbl_scope,
	.tf_dev_map_parif = tf_dev_p4_map_parif,
	.tf_dev_free_tbl_scope = tf_em_ext_common_free,
	.tf_dev_set_if_tbl = tf_if_tbl_set,
	.tf_dev_get_if_tbl = tf_if_tbl_get,
	.tf_dev_set_global_cfg = tf_global_cfg_set,
	.tf_dev_get_global_cfg = tf_global_cfg_get,
	.tf_dev_get_mailbox = tf_dev_p4_get_mailbox,
	.tf_dev_word_align = tf_dev_p4_word_align,
	.tf_dev_cfa_key_hash = hcapi_cfa_p4_key_hash,
	.tf_dev_map_hcapi_caps = tf_dev_p4_map_hcapi_caps,
	.tf_dev_get_sram_resources = NULL,
	.tf_dev_set_sram_policy = NULL,
	.tf_dev_get_sram_policy = NULL,
};
