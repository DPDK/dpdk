/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2021 Intel Corporation
 */

#ifndef _ICP_QAT_HW_GEN4_COMP_H_
#define _ICP_QAT_HW_GEN4_COMP_H_

#include "icp_qat_fw.h"
#include "icp_qat_hw_gen4_comp_defs.h"

struct icp_qat_hw_comp_20_config_csr_lower {
	icp_qat_hw_comp_20_extended_delay_match_mode_t edmm;
	icp_qat_hw_comp_20_hw_comp_format_t algo;
	icp_qat_hw_comp_20_search_depth_t sd;
	icp_qat_hw_comp_20_hbs_control_t hbs;
	icp_qat_hw_comp_20_abd_t abd;
	icp_qat_hw_comp_20_lllbd_ctrl_t lllbd;
	icp_qat_hw_comp_20_min_match_control_t mmctrl;
	icp_qat_hw_comp_20_skip_hash_collision_t hash_col;
	icp_qat_hw_comp_20_skip_hash_update_t hash_update;
	icp_qat_hw_comp_20_byte_skip_t skip_ctrl;
};

static inline uint32_t ICP_QAT_FW_COMP_20_BUILD_CONFIG_LOWER(
		struct icp_qat_hw_comp_20_config_csr_lower csr)
{
	uint32_t val32 = 0;

	QAT_FIELD_SET(val32, csr.algo,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_HW_COMP_FORMAT_BITPOS,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_HW_COMP_FORMAT_MASK);

	QAT_FIELD_SET(val32, csr.sd,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_SEARCH_DEPTH_BITPOS,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_SEARCH_DEPTH_MASK);

	QAT_FIELD_SET(val32, csr.edmm,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_EXTENDED_DELAY_MATCH_MODE_BITPOS,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_EXTENDED_DELAY_MATCH_MODE_MASK);

	QAT_FIELD_SET(val32, csr.hbs,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_HBS_CONTROL_BITPOS,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_HBS_CONTROL_MASK);

	QAT_FIELD_SET(val32, csr.lllbd,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_LLLBD_CTRL_BITPOS,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_LLLBD_CTRL_MASK);

	QAT_FIELD_SET(val32, csr.mmctrl,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_MIN_MATCH_CONTROL_BITPOS,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_MIN_MATCH_CONTROL_MASK);

	QAT_FIELD_SET(val32, csr.hash_col,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_SKIP_HASH_COLLISION_BITPOS,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_SKIP_HASH_COLLISION_MASK);

	QAT_FIELD_SET(val32, csr.hash_update,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_SKIP_HASH_UPDATE_BITPOS,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_SKIP_HASH_UPDATE_MASK);

	QAT_FIELD_SET(val32, csr.skip_ctrl,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_BYTE_SKIP_BITPOS,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_BYTE_SKIP_MASK);

	QAT_FIELD_SET(val32, csr.abd,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_ABD_BITPOS,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_ABD_MASK);

	QAT_FIELD_SET(val32, csr.lllbd,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_LLLBD_CTRL_BITPOS,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_LLLBD_CTRL_MASK);

	return rte_bswap32(val32);
}

struct icp_qat_hw_comp_20_config_csr_upper {
	icp_qat_hw_comp_20_scb_control_t scb_ctrl;
	icp_qat_hw_comp_20_rmb_control_t rmb_ctrl;
	icp_qat_hw_comp_20_som_control_t som_ctrl;
	icp_qat_hw_comp_20_skip_hash_rd_control_t skip_hash_ctrl;
	icp_qat_hw_comp_20_scb_unload_control_t scb_unload_ctrl;
	icp_qat_hw_comp_20_disable_token_fusion_control_t
			disable_token_fusion_ctrl;
	icp_qat_hw_comp_20_lbms_t lbms;
	icp_qat_hw_comp_20_scb_mode_reset_mask_t scb_mode_reset;
	uint16_t lazy;
	uint16_t nice;
};

static inline uint32_t ICP_QAT_FW_COMP_20_BUILD_CONFIG_UPPER(
		struct icp_qat_hw_comp_20_config_csr_upper csr)
{
	uint32_t val32 = 0;

	QAT_FIELD_SET(val32, csr.scb_ctrl,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_SCB_CONTROL_BITPOS,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_SCB_CONTROL_MASK);

	QAT_FIELD_SET(val32, csr.rmb_ctrl,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_RMB_CONTROL_BITPOS,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_RMB_CONTROL_MASK);

	QAT_FIELD_SET(val32, csr.som_ctrl,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_SOM_CONTROL_BITPOS,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_SOM_CONTROL_MASK);

	QAT_FIELD_SET(val32, csr.skip_hash_ctrl,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_SKIP_HASH_RD_CONTROL_BITPOS,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_SKIP_HASH_RD_CONTROL_MASK);

	QAT_FIELD_SET(val32, csr.scb_unload_ctrl,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_SCB_UNLOAD_CONTROL_BITPOS,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_SCB_UNLOAD_CONTROL_MASK);

	QAT_FIELD_SET(val32, csr.disable_token_fusion_ctrl,
	ICP_QAT_HW_COMP_20_CONFIG_CSR_DISABLE_TOKEN_FUSION_CONTROL_BITPOS,
	ICP_QAT_HW_COMP_20_CONFIG_CSR_DISABLE_TOKEN_FUSION_CONTROL_MASK);

	QAT_FIELD_SET(val32, csr.lbms,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_LBMS_BITPOS,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_LBMS_MASK);

	QAT_FIELD_SET(val32, csr.scb_mode_reset,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_SCB_MODE_RESET_MASK_BITPOS,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_SCB_MODE_RESET_MASK_MASK);

	QAT_FIELD_SET(val32, csr.lazy,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_LAZY_PARAM_BITPOS,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_LAZY_PARAM_MASK);

	QAT_FIELD_SET(val32, csr.nice,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_NICE_PARAM_BITPOS,
		ICP_QAT_HW_COMP_20_CONFIG_CSR_NICE_PARAM_MASK);

	return rte_bswap32(val32);
}

struct icp_qat_hw_decomp_20_config_csr_lower {
	icp_qat_hw_decomp_20_hbs_control_t hbs;
	icp_qat_hw_decomp_20_lbms_t lbms;
	icp_qat_hw_decomp_20_hw_comp_format_t algo;
	icp_qat_hw_decomp_20_min_match_control_t mmctrl;
	icp_qat_hw_decomp_20_lz4_block_checksum_present_t lbc;
};

static inline uint32_t ICP_QAT_FW_DECOMP_20_BUILD_CONFIG_LOWER(
		struct icp_qat_hw_decomp_20_config_csr_lower csr)
{
	uint32_t val32 = 0;

	QAT_FIELD_SET(val32, csr.hbs,
		ICP_QAT_HW_DECOMP_20_CONFIG_CSR_HBS_CONTROL_BITPOS,
		ICP_QAT_HW_DECOMP_20_CONFIG_CSR_HBS_CONTROL_MASK);

	QAT_FIELD_SET(val32, csr.lbms,
		ICP_QAT_HW_DECOMP_20_CONFIG_CSR_LBMS_BITPOS,
		ICP_QAT_HW_DECOMP_20_CONFIG_CSR_LBMS_MASK);

	QAT_FIELD_SET(val32, csr.algo,
		ICP_QAT_HW_DECOMP_20_CONFIG_CSR_HW_DECOMP_FORMAT_BITPOS,
		ICP_QAT_HW_DECOMP_20_CONFIG_CSR_HW_DECOMP_FORMAT_MASK);

	QAT_FIELD_SET(val32, csr.mmctrl,
		ICP_QAT_HW_DECOMP_20_CONFIG_CSR_MIN_MATCH_CONTROL_BITPOS,
		ICP_QAT_HW_DECOMP_20_CONFIG_CSR_MIN_MATCH_CONTROL_MASK);

	QAT_FIELD_SET(val32, csr.lbc,
	ICP_QAT_HW_DECOMP_20_CONFIG_CSR_LZ4_BLOCK_CHECKSUM_PRESENT_BITPOS,
	ICP_QAT_HW_DECOMP_20_CONFIG_CSR_LZ4_BLOCK_CHECKSUM_PRESENT_MASK);

	return rte_bswap32(val32);
}

struct icp_qat_hw_decomp_20_config_csr_upper {
	icp_qat_hw_decomp_20_speculative_decoder_control_t sdc;
	icp_qat_hw_decomp_20_mini_cam_control_t mcc;
};

static inline uint32_t ICP_QAT_FW_DECOMP_20_BUILD_CONFIG_UPPER(
		struct icp_qat_hw_decomp_20_config_csr_upper csr)
{
	uint32_t val32 = 0;

	QAT_FIELD_SET(val32, csr.sdc,
	ICP_QAT_HW_DECOMP_20_CONFIG_CSR_SPECULATIVE_DECODER_CONTROL_BITPOS,
	ICP_QAT_HW_DECOMP_20_CONFIG_CSR_SPECULATIVE_DECODER_CONTROL_MASK);

	QAT_FIELD_SET(val32, csr.mcc,
		ICP_QAT_HW_DECOMP_20_CONFIG_CSR_MINI_CAM_CONTROL_BITPOS,
		ICP_QAT_HW_DECOMP_20_CONFIG_CSR_MINI_CAM_CONTROL_MASK);

	return rte_bswap32(val32);
}

#endif /* _ICP_QAT_HW_GEN4_COMP_H_ */
