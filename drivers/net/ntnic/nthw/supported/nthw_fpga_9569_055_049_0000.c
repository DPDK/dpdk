/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Napatech A/S
 */

/*
 * Auto-generated file - do *NOT* edit
 */

#include "nthw_register.h"

static nthw_fpga_field_init_s cat_cct_ctrl_fields[] = {
	{ CAT_CCT_CTRL_ADR, 8, 0, 0x0000 },
	{ CAT_CCT_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cat_cct_data_fields[] = {
	{ CAT_CCT_DATA_COLOR, 32, 0, 0x0000 },
	{ CAT_CCT_DATA_KM, 4, 32, 0x0000 },
};

static nthw_fpga_field_init_s cat_cfn_ctrl_fields[] = {
	{ CAT_CFN_CTRL_ADR, 6, 0, 0x0000 },
	{ CAT_CFN_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cat_cfn_data_fields[] = {
	{ CAT_CFN_DATA_ENABLE, 1, 0, 0x0000 },
	{ CAT_CFN_DATA_ERR_CV, 2, 99, 0x0000 },
	{ CAT_CFN_DATA_ERR_FCS, 2, 101, 0x0000 },
	{ CAT_CFN_DATA_ERR_INV, 1, 98, 0x0000 },
	{ CAT_CFN_DATA_ERR_L3_CS, 2, 105, 0x0000 },
	{ CAT_CFN_DATA_ERR_L4_CS, 2, 107, 0x0000 },
	{ CAT_CFN_DATA_ERR_TNL_L3_CS, 2, 109, 0x0000 },
	{ CAT_CFN_DATA_ERR_TNL_L4_CS, 2, 111, 0x0000 },
	{ CAT_CFN_DATA_ERR_TNL_TTL_EXP, 2, 115, 0x0000 },
	{ CAT_CFN_DATA_ERR_TRUNC, 2, 103, 0x0000 },
	{ CAT_CFN_DATA_ERR_TTL_EXP, 2, 113, 0x0000 },
	{ CAT_CFN_DATA_INV, 1, 1, 0x0000 },
	{ CAT_CFN_DATA_KM0_OR, 3, 173, 0x0000 },
	{ CAT_CFN_DATA_KM1_OR, 3, 176, 0x0000 },
	{ CAT_CFN_DATA_LC, 8, 164, 0x0000 },
	{ CAT_CFN_DATA_LC_INV, 1, 172, 0x0000 },
	{ CAT_CFN_DATA_MAC_PORT, 2, 117, 0x0000 },
	{ CAT_CFN_DATA_PM_AND_INV, 1, 161, 0x0000 },
	{ CAT_CFN_DATA_PM_CMB, 4, 157, 0x0000 },
	{ CAT_CFN_DATA_PM_CMP, 32, 119, 0x0000 },
	{ CAT_CFN_DATA_PM_DCT, 2, 151, 0x0000 },
	{ CAT_CFN_DATA_PM_EXT_INV, 4, 153, 0x0000 },
	{ CAT_CFN_DATA_PM_INV, 1, 163, 0x0000 },
	{ CAT_CFN_DATA_PM_OR_INV, 1, 162, 0x0000 },
	{ CAT_CFN_DATA_PTC_CFP, 2, 5, 0x0000 },
	{ CAT_CFN_DATA_PTC_FRAG, 4, 36, 0x0000 },
	{ CAT_CFN_DATA_PTC_INV, 1, 2, 0x0000 },
	{ CAT_CFN_DATA_PTC_IP_PROT, 8, 40, 0x0000 },
	{ CAT_CFN_DATA_PTC_ISL, 2, 3, 0x0000 },
	{ CAT_CFN_DATA_PTC_L2, 7, 12, 0x0000 },
	{ CAT_CFN_DATA_PTC_L3, 3, 33, 0x0000 },
	{ CAT_CFN_DATA_PTC_L4, 5, 48, 0x0000 },
	{ CAT_CFN_DATA_PTC_MAC, 5, 7, 0x0000 },
	{ CAT_CFN_DATA_PTC_MPLS, 8, 25, 0x0000 },
	{ CAT_CFN_DATA_PTC_TNL_FRAG, 4, 81, 0x0000 },
	{ CAT_CFN_DATA_PTC_TNL_IP_PROT, 8, 85, 0x0000 },
	{ CAT_CFN_DATA_PTC_TNL_L2, 2, 64, 0x0000 },
	{ CAT_CFN_DATA_PTC_TNL_L3, 3, 78, 0x0000 },
	{ CAT_CFN_DATA_PTC_TNL_L4, 5, 93, 0x0000 },
	{ CAT_CFN_DATA_PTC_TNL_MPLS, 8, 70, 0x0000 },
	{ CAT_CFN_DATA_PTC_TNL_VLAN, 4, 66, 0x0000 },
	{ CAT_CFN_DATA_PTC_TUNNEL, 11, 53, 0x0000 },
	{ CAT_CFN_DATA_PTC_VLAN, 4, 21, 0x0000 },
	{ CAT_CFN_DATA_PTC_VNTAG, 2, 19, 0x0000 },
};

static nthw_fpga_field_init_s cat_cot_ctrl_fields[] = {
	{ CAT_COT_CTRL_ADR, 6, 0, 0x0000 },
	{ CAT_COT_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cat_cot_data_fields[] = {
	{ CAT_COT_DATA_COLOR, 32, 0, 0x0000 },
	{ CAT_COT_DATA_KM, 4, 32, 0x0000 },
};

static nthw_fpga_field_init_s cat_cte_ctrl_fields[] = {
	{ CAT_CTE_CTRL_ADR, 6, 0, 0x0000 },
	{ CAT_CTE_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cat_cte_data_fields[] = {
	{ CAT_CTE_DATA_COL_ENABLE, 1, 0, 0x0000 }, { CAT_CTE_DATA_COR_ENABLE, 1, 1, 0x0000 },
	{ CAT_CTE_DATA_EPP_ENABLE, 1, 9, 0x0000 }, { CAT_CTE_DATA_HSH_ENABLE, 1, 2, 0x0000 },
	{ CAT_CTE_DATA_HST_ENABLE, 1, 8, 0x0000 }, { CAT_CTE_DATA_IPF_ENABLE, 1, 4, 0x0000 },
	{ CAT_CTE_DATA_MSK_ENABLE, 1, 7, 0x0000 }, { CAT_CTE_DATA_PDB_ENABLE, 1, 6, 0x0000 },
	{ CAT_CTE_DATA_QSL_ENABLE, 1, 3, 0x0000 }, { CAT_CTE_DATA_SLC_ENABLE, 1, 5, 0x0000 },
	{ CAT_CTE_DATA_TPE_ENABLE, 1, 10, 0x0000 },
};

static nthw_fpga_field_init_s cat_cts_ctrl_fields[] = {
	{ CAT_CTS_CTRL_ADR, 9, 0, 0x0000 },
	{ CAT_CTS_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cat_cts_data_fields[] = {
	{ CAT_CTS_DATA_CAT_A, 6, 0, 0x0000 },
	{ CAT_CTS_DATA_CAT_B, 6, 6, 0x0000 },
};

static nthw_fpga_field_init_s cat_dct_ctrl_fields[] = {
	{ CAT_DCT_CTRL_ADR, 13, 0, 0x0000 },
	{ CAT_DCT_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cat_dct_data_fields[] = {
	{ CAT_DCT_DATA_RES, 16, 0, 0x0000 },
};

static nthw_fpga_field_init_s cat_dct_sel_fields[] = {
	{ CAT_DCT_SEL_LU, 2, 0, 0x0000 },
};

static nthw_fpga_field_init_s cat_exo_ctrl_fields[] = {
	{ CAT_EXO_CTRL_ADR, 2, 0, 0x0000 },
	{ CAT_EXO_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cat_exo_data_fields[] = {
	{ CAT_EXO_DATA_DYN, 5, 0, 0x0000 },
	{ CAT_EXO_DATA_OFS, 11, 16, 0x0000 },
};

static nthw_fpga_field_init_s cat_fte0_ctrl_fields[] = {
	{ CAT_FTE0_CTRL_ADR, 9, 0, 0x0000 },
	{ CAT_FTE0_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cat_fte0_data_fields[] = {
	{ CAT_FTE0_DATA_ENABLE, 8, 0, 0x0000 },
};

static nthw_fpga_field_init_s cat_fte1_ctrl_fields[] = {
	{ CAT_FTE1_CTRL_ADR, 9, 0, 0x0000 },
	{ CAT_FTE1_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cat_fte1_data_fields[] = {
	{ CAT_FTE1_DATA_ENABLE, 8, 0, 0x0000 },
};

static nthw_fpga_field_init_s cat_join_fields[] = {
	{ CAT_JOIN_J1, 2, 0, 0x0000 },
	{ CAT_JOIN_J2, 1, 8, 0x0000 },
};

static nthw_fpga_field_init_s cat_kcc_ctrl_fields[] = {
	{ CAT_KCC_CTRL_ADR, 11, 0, 0x0000 },
	{ CAT_KCC_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cat_kcc_data_fields[] = {
	{ CAT_KCC_DATA_CATEGORY, 8, 64, 0x0000 },
	{ CAT_KCC_DATA_ID, 12, 72, 0x0000 },
	{ CAT_KCC_DATA_KEY, 64, 0, 0x0000 },
};

static nthw_fpga_field_init_s cat_kce0_ctrl_fields[] = {
	{ CAT_KCE0_CTRL_ADR, 3, 0, 0x0000 },
	{ CAT_KCE0_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cat_kce0_data_fields[] = {
	{ CAT_KCE0_DATA_ENABLE, 8, 0, 0x0000 },
};

static nthw_fpga_field_init_s cat_kce1_ctrl_fields[] = {
	{ CAT_KCE1_CTRL_ADR, 3, 0, 0x0000 },
	{ CAT_KCE1_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cat_kce1_data_fields[] = {
	{ CAT_KCE1_DATA_ENABLE, 8, 0, 0x0000 },
};

static nthw_fpga_field_init_s cat_kcs0_ctrl_fields[] = {
	{ CAT_KCS0_CTRL_ADR, 6, 0, 0x0000 },
	{ CAT_KCS0_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cat_kcs0_data_fields[] = {
	{ CAT_KCS0_DATA_CATEGORY, 6, 0, 0x0000 },
};

static nthw_fpga_field_init_s cat_kcs1_ctrl_fields[] = {
	{ CAT_KCS1_CTRL_ADR, 6, 0, 0x0000 },
	{ CAT_KCS1_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cat_kcs1_data_fields[] = {
	{ CAT_KCS1_DATA_CATEGORY, 6, 0, 0x0000 },
};

static nthw_fpga_field_init_s cat_len_ctrl_fields[] = {
	{ CAT_LEN_CTRL_ADR, 3, 0, 0x0000 },
	{ CAT_LEN_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cat_len_data_fields[] = {
	{ CAT_LEN_DATA_DYN1, 5, 28, 0x0000 }, { CAT_LEN_DATA_DYN2, 5, 33, 0x0000 },
	{ CAT_LEN_DATA_INV, 1, 38, 0x0000 }, { CAT_LEN_DATA_LOWER, 14, 0, 0x0000 },
	{ CAT_LEN_DATA_UPPER, 14, 14, 0x0000 },
};

static nthw_fpga_field_init_s cat_rck_ctrl_fields[] = {
	{ CAT_RCK_CTRL_ADR, 8, 0, 0x0000 },
	{ CAT_RCK_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cat_rck_data_fields[] = {
	{ CAT_RCK_DATA_CM0U, 1, 1, 0x0000 }, { CAT_RCK_DATA_CM1U, 1, 5, 0x0000 },
	{ CAT_RCK_DATA_CM2U, 1, 9, 0x0000 }, { CAT_RCK_DATA_CM3U, 1, 13, 0x0000 },
	{ CAT_RCK_DATA_CM4U, 1, 17, 0x0000 }, { CAT_RCK_DATA_CM5U, 1, 21, 0x0000 },
	{ CAT_RCK_DATA_CM6U, 1, 25, 0x0000 }, { CAT_RCK_DATA_CM7U, 1, 29, 0x0000 },
	{ CAT_RCK_DATA_CML0, 1, 0, 0x0000 }, { CAT_RCK_DATA_CML1, 1, 4, 0x0000 },
	{ CAT_RCK_DATA_CML2, 1, 8, 0x0000 }, { CAT_RCK_DATA_CML3, 1, 12, 0x0000 },
	{ CAT_RCK_DATA_CML4, 1, 16, 0x0000 }, { CAT_RCK_DATA_CML5, 1, 20, 0x0000 },
	{ CAT_RCK_DATA_CML6, 1, 24, 0x0000 }, { CAT_RCK_DATA_CML7, 1, 28, 0x0000 },
	{ CAT_RCK_DATA_SEL0, 1, 2, 0x0000 }, { CAT_RCK_DATA_SEL1, 1, 6, 0x0000 },
	{ CAT_RCK_DATA_SEL2, 1, 10, 0x0000 }, { CAT_RCK_DATA_SEL3, 1, 14, 0x0000 },
	{ CAT_RCK_DATA_SEL4, 1, 18, 0x0000 }, { CAT_RCK_DATA_SEL5, 1, 22, 0x0000 },
	{ CAT_RCK_DATA_SEL6, 1, 26, 0x0000 }, { CAT_RCK_DATA_SEL7, 1, 30, 0x0000 },
	{ CAT_RCK_DATA_SEU0, 1, 3, 0x0000 }, { CAT_RCK_DATA_SEU1, 1, 7, 0x0000 },
	{ CAT_RCK_DATA_SEU2, 1, 11, 0x0000 }, { CAT_RCK_DATA_SEU3, 1, 15, 0x0000 },
	{ CAT_RCK_DATA_SEU4, 1, 19, 0x0000 }, { CAT_RCK_DATA_SEU5, 1, 23, 0x0000 },
	{ CAT_RCK_DATA_SEU6, 1, 27, 0x0000 }, { CAT_RCK_DATA_SEU7, 1, 31, 0x0000 },
};

static nthw_fpga_register_init_s cat_registers[] = {
	{ CAT_CCT_CTRL, 30, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, cat_cct_ctrl_fields },
	{ CAT_CCT_DATA, 31, 36, NTHW_FPGA_REG_TYPE_WO, 0, 2, cat_cct_data_fields },
	{ CAT_CFN_CTRL, 10, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, cat_cfn_ctrl_fields },
	{ CAT_CFN_DATA, 11, 179, NTHW_FPGA_REG_TYPE_WO, 0, 44, cat_cfn_data_fields },
	{ CAT_COT_CTRL, 28, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, cat_cot_ctrl_fields },
	{ CAT_COT_DATA, 29, 36, NTHW_FPGA_REG_TYPE_WO, 0, 2, cat_cot_data_fields },
	{ CAT_CTE_CTRL, 24, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, cat_cte_ctrl_fields },
	{ CAT_CTE_DATA, 25, 11, NTHW_FPGA_REG_TYPE_WO, 0, 11, cat_cte_data_fields },
	{ CAT_CTS_CTRL, 26, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, cat_cts_ctrl_fields },
	{ CAT_CTS_DATA, 27, 12, NTHW_FPGA_REG_TYPE_WO, 0, 2, cat_cts_data_fields },
	{ CAT_DCT_CTRL, 6, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, cat_dct_ctrl_fields },
	{ CAT_DCT_DATA, 7, 16, NTHW_FPGA_REG_TYPE_WO, 0, 1, cat_dct_data_fields },
	{ CAT_DCT_SEL, 4, 2, NTHW_FPGA_REG_TYPE_WO, 0, 1, cat_dct_sel_fields },
	{ CAT_EXO_CTRL, 0, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, cat_exo_ctrl_fields },
	{ CAT_EXO_DATA, 1, 27, NTHW_FPGA_REG_TYPE_WO, 0, 2, cat_exo_data_fields },
	{ CAT_FTE0_CTRL, 16, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, cat_fte0_ctrl_fields },
	{ CAT_FTE0_DATA, 17, 8, NTHW_FPGA_REG_TYPE_WO, 0, 1, cat_fte0_data_fields },
	{ CAT_FTE1_CTRL, 22, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, cat_fte1_ctrl_fields },
	{ CAT_FTE1_DATA, 23, 8, NTHW_FPGA_REG_TYPE_WO, 0, 1, cat_fte1_data_fields },
	{ CAT_JOIN, 5, 9, NTHW_FPGA_REG_TYPE_WO, 0, 2, cat_join_fields },
	{ CAT_KCC_CTRL, 32, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, cat_kcc_ctrl_fields },
	{ CAT_KCC_DATA, 33, 84, NTHW_FPGA_REG_TYPE_WO, 0, 3, cat_kcc_data_fields },
	{ CAT_KCE0_CTRL, 12, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, cat_kce0_ctrl_fields },
	{ CAT_KCE0_DATA, 13, 8, NTHW_FPGA_REG_TYPE_WO, 0, 1, cat_kce0_data_fields },
	{ CAT_KCE1_CTRL, 18, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, cat_kce1_ctrl_fields },
	{ CAT_KCE1_DATA, 19, 8, NTHW_FPGA_REG_TYPE_WO, 0, 1, cat_kce1_data_fields },
	{ CAT_KCS0_CTRL, 14, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, cat_kcs0_ctrl_fields },
	{ CAT_KCS0_DATA, 15, 6, NTHW_FPGA_REG_TYPE_WO, 0, 1, cat_kcs0_data_fields },
	{ CAT_KCS1_CTRL, 20, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, cat_kcs1_ctrl_fields },
	{ CAT_KCS1_DATA, 21, 6, NTHW_FPGA_REG_TYPE_WO, 0, 1, cat_kcs1_data_fields },
	{ CAT_LEN_CTRL, 8, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, cat_len_ctrl_fields },
	{ CAT_LEN_DATA, 9, 39, NTHW_FPGA_REG_TYPE_WO, 0, 5, cat_len_data_fields },
	{ CAT_RCK_CTRL, 2, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, cat_rck_ctrl_fields },
	{ CAT_RCK_DATA, 3, 32, NTHW_FPGA_REG_TYPE_WO, 0, 32, cat_rck_data_fields },
};

static nthw_fpga_field_init_s cpy_packet_reader0_ctrl_fields[] = {
	{ CPY_PACKET_READER0_CTRL_ADR, 4, 0, 0x0000 },
	{ CPY_PACKET_READER0_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cpy_packet_reader0_data_fields[] = {
	{ CPY_PACKET_READER0_DATA_DYN, 5, 10, 0x0000 },
	{ CPY_PACKET_READER0_DATA_OFS, 10, 0, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer0_ctrl_fields[] = {
	{ CPY_WRITER0_CTRL_ADR, 4, 0, 0x0000 },
	{ CPY_WRITER0_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer0_data_fields[] = {
	{ CPY_WRITER0_DATA_DYN, 5, 17, 0x0000 }, { CPY_WRITER0_DATA_LEN, 5, 22, 0x0000 },
	{ CPY_WRITER0_DATA_MASK_POINTER, 4, 27, 0x0000 }, { CPY_WRITER0_DATA_OFS, 14, 3, 0x0000 },
	{ CPY_WRITER0_DATA_READER_SELECT, 3, 0, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer0_mask_ctrl_fields[] = {
	{ CPY_WRITER0_MASK_CTRL_ADR, 4, 0, 0x0000 },
	{ CPY_WRITER0_MASK_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer0_mask_data_fields[] = {
	{ CPY_WRITER0_MASK_DATA_BYTE_MASK, 16, 0, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer1_ctrl_fields[] = {
	{ CPY_WRITER1_CTRL_ADR, 4, 0, 0x0000 },
	{ CPY_WRITER1_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer1_data_fields[] = {
	{ CPY_WRITER1_DATA_DYN, 5, 17, 0x0000 }, { CPY_WRITER1_DATA_LEN, 5, 22, 0x0000 },
	{ CPY_WRITER1_DATA_MASK_POINTER, 4, 27, 0x0000 }, { CPY_WRITER1_DATA_OFS, 14, 3, 0x0000 },
	{ CPY_WRITER1_DATA_READER_SELECT, 3, 0, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer1_mask_ctrl_fields[] = {
	{ CPY_WRITER1_MASK_CTRL_ADR, 4, 0, 0x0000 },
	{ CPY_WRITER1_MASK_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer1_mask_data_fields[] = {
	{ CPY_WRITER1_MASK_DATA_BYTE_MASK, 16, 0, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer2_ctrl_fields[] = {
	{ CPY_WRITER2_CTRL_ADR, 4, 0, 0x0000 },
	{ CPY_WRITER2_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer2_data_fields[] = {
	{ CPY_WRITER2_DATA_DYN, 5, 17, 0x0000 }, { CPY_WRITER2_DATA_LEN, 5, 22, 0x0000 },
	{ CPY_WRITER2_DATA_MASK_POINTER, 4, 27, 0x0000 }, { CPY_WRITER2_DATA_OFS, 14, 3, 0x0000 },
	{ CPY_WRITER2_DATA_READER_SELECT, 3, 0, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer2_mask_ctrl_fields[] = {
	{ CPY_WRITER2_MASK_CTRL_ADR, 4, 0, 0x0000 },
	{ CPY_WRITER2_MASK_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer2_mask_data_fields[] = {
	{ CPY_WRITER2_MASK_DATA_BYTE_MASK, 16, 0, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer3_ctrl_fields[] = {
	{ CPY_WRITER3_CTRL_ADR, 4, 0, 0x0000 },
	{ CPY_WRITER3_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer3_data_fields[] = {
	{ CPY_WRITER3_DATA_DYN, 5, 17, 0x0000 }, { CPY_WRITER3_DATA_LEN, 5, 22, 0x0000 },
	{ CPY_WRITER3_DATA_MASK_POINTER, 4, 27, 0x0000 }, { CPY_WRITER3_DATA_OFS, 14, 3, 0x0000 },
	{ CPY_WRITER3_DATA_READER_SELECT, 3, 0, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer3_mask_ctrl_fields[] = {
	{ CPY_WRITER3_MASK_CTRL_ADR, 4, 0, 0x0000 },
	{ CPY_WRITER3_MASK_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer3_mask_data_fields[] = {
	{ CPY_WRITER3_MASK_DATA_BYTE_MASK, 16, 0, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer4_ctrl_fields[] = {
	{ CPY_WRITER4_CTRL_ADR, 4, 0, 0x0000 },
	{ CPY_WRITER4_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer4_data_fields[] = {
	{ CPY_WRITER4_DATA_DYN, 5, 17, 0x0000 }, { CPY_WRITER4_DATA_LEN, 5, 22, 0x0000 },
	{ CPY_WRITER4_DATA_MASK_POINTER, 4, 27, 0x0000 }, { CPY_WRITER4_DATA_OFS, 14, 3, 0x0000 },
	{ CPY_WRITER4_DATA_READER_SELECT, 3, 0, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer4_mask_ctrl_fields[] = {
	{ CPY_WRITER4_MASK_CTRL_ADR, 4, 0, 0x0000 },
	{ CPY_WRITER4_MASK_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer4_mask_data_fields[] = {
	{ CPY_WRITER4_MASK_DATA_BYTE_MASK, 16, 0, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer5_ctrl_fields[] = {
	{ CPY_WRITER5_CTRL_ADR, 4, 0, 0x0000 },
	{ CPY_WRITER5_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer5_data_fields[] = {
	{ CPY_WRITER5_DATA_DYN, 5, 17, 0x0000 }, { CPY_WRITER5_DATA_LEN, 5, 22, 0x0000 },
	{ CPY_WRITER5_DATA_MASK_POINTER, 4, 27, 0x0000 }, { CPY_WRITER5_DATA_OFS, 14, 3, 0x0000 },
	{ CPY_WRITER5_DATA_READER_SELECT, 3, 0, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer5_mask_ctrl_fields[] = {
	{ CPY_WRITER5_MASK_CTRL_ADR, 4, 0, 0x0000 },
	{ CPY_WRITER5_MASK_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s cpy_writer5_mask_data_fields[] = {
	{ CPY_WRITER5_MASK_DATA_BYTE_MASK, 16, 0, 0x0000 },
};

static nthw_fpga_register_init_s cpy_registers[] = {
	{
		CPY_PACKET_READER0_CTRL, 24, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2,
		cpy_packet_reader0_ctrl_fields
	},
	{
		CPY_PACKET_READER0_DATA, 25, 15, NTHW_FPGA_REG_TYPE_WO, 0, 2,
		cpy_packet_reader0_data_fields
	},
	{ CPY_WRITER0_CTRL, 0, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, cpy_writer0_ctrl_fields },
	{ CPY_WRITER0_DATA, 1, 31, NTHW_FPGA_REG_TYPE_WO, 0, 5, cpy_writer0_data_fields },
	{
		CPY_WRITER0_MASK_CTRL, 2, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2,
		cpy_writer0_mask_ctrl_fields
	},
	{
		CPY_WRITER0_MASK_DATA, 3, 16, NTHW_FPGA_REG_TYPE_WO, 0, 1,
		cpy_writer0_mask_data_fields
	},
	{ CPY_WRITER1_CTRL, 4, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, cpy_writer1_ctrl_fields },
	{ CPY_WRITER1_DATA, 5, 31, NTHW_FPGA_REG_TYPE_WO, 0, 5, cpy_writer1_data_fields },
	{
		CPY_WRITER1_MASK_CTRL, 6, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2,
		cpy_writer1_mask_ctrl_fields
	},
	{
		CPY_WRITER1_MASK_DATA, 7, 16, NTHW_FPGA_REG_TYPE_WO, 0, 1,
		cpy_writer1_mask_data_fields
	},
	{ CPY_WRITER2_CTRL, 8, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, cpy_writer2_ctrl_fields },
	{ CPY_WRITER2_DATA, 9, 31, NTHW_FPGA_REG_TYPE_WO, 0, 5, cpy_writer2_data_fields },
	{
		CPY_WRITER2_MASK_CTRL, 10, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2,
		cpy_writer2_mask_ctrl_fields
	},
	{
		CPY_WRITER2_MASK_DATA, 11, 16, NTHW_FPGA_REG_TYPE_WO, 0, 1,
		cpy_writer2_mask_data_fields
	},
	{ CPY_WRITER3_CTRL, 12, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, cpy_writer3_ctrl_fields },
	{ CPY_WRITER3_DATA, 13, 31, NTHW_FPGA_REG_TYPE_WO, 0, 5, cpy_writer3_data_fields },
	{
		CPY_WRITER3_MASK_CTRL, 14, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2,
		cpy_writer3_mask_ctrl_fields
	},
	{
		CPY_WRITER3_MASK_DATA, 15, 16, NTHW_FPGA_REG_TYPE_WO, 0, 1,
		cpy_writer3_mask_data_fields
	},
	{ CPY_WRITER4_CTRL, 16, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, cpy_writer4_ctrl_fields },
	{ CPY_WRITER4_DATA, 17, 31, NTHW_FPGA_REG_TYPE_WO, 0, 5, cpy_writer4_data_fields },
	{
		CPY_WRITER4_MASK_CTRL, 18, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2,
		cpy_writer4_mask_ctrl_fields
	},
	{
		CPY_WRITER4_MASK_DATA, 19, 16, NTHW_FPGA_REG_TYPE_WO, 0, 1,
		cpy_writer4_mask_data_fields
	},
	{ CPY_WRITER5_CTRL, 20, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, cpy_writer5_ctrl_fields },
	{ CPY_WRITER5_DATA, 21, 31, NTHW_FPGA_REG_TYPE_WO, 0, 5, cpy_writer5_data_fields },
	{
		CPY_WRITER5_MASK_CTRL, 22, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2,
		cpy_writer5_mask_ctrl_fields
	},
	{
		CPY_WRITER5_MASK_DATA, 23, 16, NTHW_FPGA_REG_TYPE_WO, 0, 1,
		cpy_writer5_mask_data_fields
	},
};

static nthw_fpga_field_init_s csu_rcp_ctrl_fields[] = {
	{ CSU_RCP_CTRL_ADR, 4, 0, 0x0000 },
	{ CSU_RCP_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s csu_rcp_data_fields[] = {
	{ CSU_RCP_DATA_IL3_CMD, 2, 5, 0x0000 },
	{ CSU_RCP_DATA_IL4_CMD, 3, 7, 0x0000 },
	{ CSU_RCP_DATA_OL3_CMD, 2, 0, 0x0000 },
	{ CSU_RCP_DATA_OL4_CMD, 3, 2, 0x0000 },
};

static nthw_fpga_register_init_s csu_registers[] = {
	{ CSU_RCP_CTRL, 1, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, csu_rcp_ctrl_fields },
	{ CSU_RCP_DATA, 2, 10, NTHW_FPGA_REG_TYPE_WO, 0, 4, csu_rcp_data_fields },
};

static nthw_fpga_field_init_s dbs_rx_am_ctrl_fields[] = {
	{ DBS_RX_AM_CTRL_ADR, 7, 0, 0x0000 },
	{ DBS_RX_AM_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s dbs_rx_am_data_fields[] = {
	{ DBS_RX_AM_DATA_ENABLE, 1, 72, 0x0000 }, { DBS_RX_AM_DATA_GPA, 64, 0, 0x0000 },
	{ DBS_RX_AM_DATA_HID, 8, 64, 0x0000 }, { DBS_RX_AM_DATA_INT, 1, 74, 0x0000 },
	{ DBS_RX_AM_DATA_PCKED, 1, 73, 0x0000 },
};

static nthw_fpga_field_init_s dbs_rx_control_fields[] = {
	{ DBS_RX_CONTROL_AME, 1, 7, 0 }, { DBS_RX_CONTROL_AMS, 4, 8, 8 },
	{ DBS_RX_CONTROL_LQ, 7, 0, 0 }, { DBS_RX_CONTROL_QE, 1, 17, 0 },
	{ DBS_RX_CONTROL_UWE, 1, 12, 0 }, { DBS_RX_CONTROL_UWS, 4, 13, 5 },
};

static nthw_fpga_field_init_s dbs_rx_dr_ctrl_fields[] = {
	{ DBS_RX_DR_CTRL_ADR, 7, 0, 0x0000 },
	{ DBS_RX_DR_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s dbs_rx_dr_data_fields[] = {
	{ DBS_RX_DR_DATA_GPA, 64, 0, 0x0000 }, { DBS_RX_DR_DATA_HDR, 1, 88, 0x0000 },
	{ DBS_RX_DR_DATA_HID, 8, 64, 0x0000 }, { DBS_RX_DR_DATA_PCKED, 1, 87, 0x0000 },
	{ DBS_RX_DR_DATA_QS, 15, 72, 0x0000 },
};

static nthw_fpga_field_init_s dbs_rx_idle_fields[] = {
	{ DBS_RX_IDLE_BUSY, 1, 8, 0 },
	{ DBS_RX_IDLE_IDLE, 1, 0, 0x0000 },
	{ DBS_RX_IDLE_QUEUE, 7, 1, 0x0000 },
};

static nthw_fpga_field_init_s dbs_rx_init_fields[] = {
	{ DBS_RX_INIT_BUSY, 1, 8, 0 },
	{ DBS_RX_INIT_INIT, 1, 0, 0x0000 },
	{ DBS_RX_INIT_QUEUE, 7, 1, 0x0000 },
};

static nthw_fpga_field_init_s dbs_rx_init_val_fields[] = {
	{ DBS_RX_INIT_VAL_IDX, 16, 0, 0x0000 },
	{ DBS_RX_INIT_VAL_PTR, 15, 16, 0x0000 },
};

static nthw_fpga_field_init_s dbs_rx_ptr_fields[] = {
	{ DBS_RX_PTR_PTR, 16, 0, 0x0000 },
	{ DBS_RX_PTR_QUEUE, 7, 16, 0x0000 },
	{ DBS_RX_PTR_VALID, 1, 23, 0x0000 },
};

static nthw_fpga_field_init_s dbs_rx_uw_ctrl_fields[] = {
	{ DBS_RX_UW_CTRL_ADR, 7, 0, 0x0000 },
	{ DBS_RX_UW_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s dbs_rx_uw_data_fields[] = {
	{ DBS_RX_UW_DATA_GPA, 64, 0, 0x0000 }, { DBS_RX_UW_DATA_HID, 8, 64, 0x0000 },
	{ DBS_RX_UW_DATA_INT, 1, 88, 0x0000 }, { DBS_RX_UW_DATA_ISTK, 1, 92, 0x0000 },
	{ DBS_RX_UW_DATA_PCKED, 1, 87, 0x0000 }, { DBS_RX_UW_DATA_QS, 15, 72, 0x0000 },
	{ DBS_RX_UW_DATA_VEC, 3, 89, 0x0000 },
};

static nthw_fpga_field_init_s dbs_tx_am_ctrl_fields[] = {
	{ DBS_TX_AM_CTRL_ADR, 7, 0, 0x0000 },
	{ DBS_TX_AM_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s dbs_tx_am_data_fields[] = {
	{ DBS_TX_AM_DATA_ENABLE, 1, 72, 0x0000 }, { DBS_TX_AM_DATA_GPA, 64, 0, 0x0000 },
	{ DBS_TX_AM_DATA_HID, 8, 64, 0x0000 }, { DBS_TX_AM_DATA_INT, 1, 74, 0x0000 },
	{ DBS_TX_AM_DATA_PCKED, 1, 73, 0x0000 },
};

static nthw_fpga_field_init_s dbs_tx_control_fields[] = {
	{ DBS_TX_CONTROL_AME, 1, 7, 0 }, { DBS_TX_CONTROL_AMS, 4, 8, 5 },
	{ DBS_TX_CONTROL_LQ, 7, 0, 0 }, { DBS_TX_CONTROL_QE, 1, 17, 0 },
	{ DBS_TX_CONTROL_UWE, 1, 12, 0 }, { DBS_TX_CONTROL_UWS, 4, 13, 8 },
};

static nthw_fpga_field_init_s dbs_tx_dr_ctrl_fields[] = {
	{ DBS_TX_DR_CTRL_ADR, 7, 0, 0x0000 },
	{ DBS_TX_DR_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s dbs_tx_dr_data_fields[] = {
	{ DBS_TX_DR_DATA_GPA, 64, 0, 0x0000 }, { DBS_TX_DR_DATA_HDR, 1, 88, 0x0000 },
	{ DBS_TX_DR_DATA_HID, 8, 64, 0x0000 }, { DBS_TX_DR_DATA_PCKED, 1, 87, 0x0000 },
	{ DBS_TX_DR_DATA_PORT, 1, 89, 0x0000 }, { DBS_TX_DR_DATA_QS, 15, 72, 0x0000 },
};

static nthw_fpga_field_init_s dbs_tx_idle_fields[] = {
	{ DBS_TX_IDLE_BUSY, 1, 8, 0 },
	{ DBS_TX_IDLE_IDLE, 1, 0, 0x0000 },
	{ DBS_TX_IDLE_QUEUE, 7, 1, 0x0000 },
};

static nthw_fpga_field_init_s dbs_tx_init_fields[] = {
	{ DBS_TX_INIT_BUSY, 1, 8, 0 },
	{ DBS_TX_INIT_INIT, 1, 0, 0x0000 },
	{ DBS_TX_INIT_QUEUE, 7, 1, 0x0000 },
};

static nthw_fpga_field_init_s dbs_tx_init_val_fields[] = {
	{ DBS_TX_INIT_VAL_IDX, 16, 0, 0x0000 },
	{ DBS_TX_INIT_VAL_PTR, 15, 16, 0x0000 },
};

static nthw_fpga_field_init_s dbs_tx_ptr_fields[] = {
	{ DBS_TX_PTR_PTR, 16, 0, 0x0000 },
	{ DBS_TX_PTR_QUEUE, 7, 16, 0x0000 },
	{ DBS_TX_PTR_VALID, 1, 23, 0x0000 },
};

static nthw_fpga_field_init_s dbs_tx_qos_ctrl_fields[] = {
	{ DBS_TX_QOS_CTRL_ADR, 1, 0, 0x0000 },
	{ DBS_TX_QOS_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s dbs_tx_qos_data_fields[] = {
	{ DBS_TX_QOS_DATA_BS, 27, 17, 0x0000 },
	{ DBS_TX_QOS_DATA_EN, 1, 0, 0x0000 },
	{ DBS_TX_QOS_DATA_IR, 16, 1, 0x0000 },
};

static nthw_fpga_field_init_s dbs_tx_qos_rate_fields[] = {
	{ DBS_TX_QOS_RATE_DIV, 19, 16, 2 },
	{ DBS_TX_QOS_RATE_MUL, 16, 0, 1 },
};

static nthw_fpga_field_init_s dbs_tx_qp_ctrl_fields[] = {
	{ DBS_TX_QP_CTRL_ADR, 7, 0, 0x0000 },
	{ DBS_TX_QP_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s dbs_tx_qp_data_fields[] = {
	{ DBS_TX_QP_DATA_VPORT, 1, 0, 0x0000 },
};

static nthw_fpga_field_init_s dbs_tx_uw_ctrl_fields[] = {
	{ DBS_TX_UW_CTRL_ADR, 7, 0, 0x0000 },
	{ DBS_TX_UW_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s dbs_tx_uw_data_fields[] = {
	{ DBS_TX_UW_DATA_GPA, 64, 0, 0x0000 }, { DBS_TX_UW_DATA_HID, 8, 64, 0x0000 },
	{ DBS_TX_UW_DATA_INO, 1, 93, 0x0000 }, { DBS_TX_UW_DATA_INT, 1, 88, 0x0000 },
	{ DBS_TX_UW_DATA_ISTK, 1, 92, 0x0000 }, { DBS_TX_UW_DATA_PCKED, 1, 87, 0x0000 },
	{ DBS_TX_UW_DATA_QS, 15, 72, 0x0000 }, { DBS_TX_UW_DATA_VEC, 3, 89, 0x0000 },
};

static nthw_fpga_register_init_s dbs_registers[] = {
	{ DBS_RX_AM_CTRL, 10, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, dbs_rx_am_ctrl_fields },
	{ DBS_RX_AM_DATA, 11, 75, NTHW_FPGA_REG_TYPE_WO, 0, 5, dbs_rx_am_data_fields },
	{ DBS_RX_CONTROL, 0, 18, NTHW_FPGA_REG_TYPE_RW, 43008, 6, dbs_rx_control_fields },
	{ DBS_RX_DR_CTRL, 18, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, dbs_rx_dr_ctrl_fields },
	{ DBS_RX_DR_DATA, 19, 89, NTHW_FPGA_REG_TYPE_WO, 0, 5, dbs_rx_dr_data_fields },
	{ DBS_RX_IDLE, 8, 9, NTHW_FPGA_REG_TYPE_MIXED, 0, 3, dbs_rx_idle_fields },
	{ DBS_RX_INIT, 2, 9, NTHW_FPGA_REG_TYPE_MIXED, 0, 3, dbs_rx_init_fields },
	{ DBS_RX_INIT_VAL, 3, 31, NTHW_FPGA_REG_TYPE_WO, 0, 2, dbs_rx_init_val_fields },
	{ DBS_RX_PTR, 4, 24, NTHW_FPGA_REG_TYPE_MIXED, 0, 3, dbs_rx_ptr_fields },
	{ DBS_RX_UW_CTRL, 14, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, dbs_rx_uw_ctrl_fields },
	{ DBS_RX_UW_DATA, 15, 93, NTHW_FPGA_REG_TYPE_WO, 0, 7, dbs_rx_uw_data_fields },
	{ DBS_TX_AM_CTRL, 12, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, dbs_tx_am_ctrl_fields },
	{ DBS_TX_AM_DATA, 13, 75, NTHW_FPGA_REG_TYPE_WO, 0, 5, dbs_tx_am_data_fields },
	{ DBS_TX_CONTROL, 1, 18, NTHW_FPGA_REG_TYPE_RW, 66816, 6, dbs_tx_control_fields },
	{ DBS_TX_DR_CTRL, 20, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, dbs_tx_dr_ctrl_fields },
	{ DBS_TX_DR_DATA, 21, 90, NTHW_FPGA_REG_TYPE_WO, 0, 6, dbs_tx_dr_data_fields },
	{ DBS_TX_IDLE, 9, 9, NTHW_FPGA_REG_TYPE_MIXED, 0, 3, dbs_tx_idle_fields },
	{ DBS_TX_INIT, 5, 9, NTHW_FPGA_REG_TYPE_MIXED, 0, 3, dbs_tx_init_fields },
	{ DBS_TX_INIT_VAL, 6, 31, NTHW_FPGA_REG_TYPE_WO, 0, 2, dbs_tx_init_val_fields },
	{ DBS_TX_PTR, 7, 24, NTHW_FPGA_REG_TYPE_MIXED, 0, 3, dbs_tx_ptr_fields },
	{ DBS_TX_QOS_CTRL, 24, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, dbs_tx_qos_ctrl_fields },
	{ DBS_TX_QOS_DATA, 25, 44, NTHW_FPGA_REG_TYPE_WO, 0, 3, dbs_tx_qos_data_fields },
	{ DBS_TX_QOS_RATE, 26, 35, NTHW_FPGA_REG_TYPE_RW, 131073, 2, dbs_tx_qos_rate_fields },
	{ DBS_TX_QP_CTRL, 22, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, dbs_tx_qp_ctrl_fields },
	{ DBS_TX_QP_DATA, 23, 1, NTHW_FPGA_REG_TYPE_WO, 0, 1, dbs_tx_qp_data_fields },
	{ DBS_TX_UW_CTRL, 16, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, dbs_tx_uw_ctrl_fields },
	{ DBS_TX_UW_DATA, 17, 94, NTHW_FPGA_REG_TYPE_WO, 0, 8, dbs_tx_uw_data_fields },
};

static nthw_fpga_field_init_s flm_buf_ctrl_fields[] = {
	{ FLM_BUF_CTRL_INF_AVAIL, 16, 16, 0x0000 },
	{ FLM_BUF_CTRL_LRN_FREE, 16, 0, 0x0000 },
	{ FLM_BUF_CTRL_STA_AVAIL, 16, 32, 0x0000 },
};

static nthw_fpga_field_init_s flm_control_fields[] = {
	{ FLM_CONTROL_CALIB_RECALIBRATE, 3, 28, 0 },
	{ FLM_CONTROL_CRCRD, 1, 12, 0x0000 },
	{ FLM_CONTROL_CRCWR, 1, 11, 0x0000 },
	{ FLM_CONTROL_EAB, 5, 18, 0 },
	{ FLM_CONTROL_ENABLE, 1, 0, 0 },
	{ FLM_CONTROL_INIT, 1, 1, 0x0000 },
	{ FLM_CONTROL_LDS, 1, 2, 0x0000 },
	{ FLM_CONTROL_LFS, 1, 3, 0x0000 },
	{ FLM_CONTROL_LIS, 1, 4, 0x0000 },
	{ FLM_CONTROL_PDS, 1, 9, 0x0000 },
	{ FLM_CONTROL_PIS, 1, 10, 0x0000 },
	{ FLM_CONTROL_RBL, 4, 13, 0 },
	{ FLM_CONTROL_RDS, 1, 7, 0x0000 },
	{ FLM_CONTROL_RIS, 1, 8, 0x0000 },
	{ FLM_CONTROL_SPLIT_SDRAM_USAGE, 5, 23, 16 },
	{ FLM_CONTROL_UDS, 1, 5, 0x0000 },
	{ FLM_CONTROL_UIS, 1, 6, 0x0000 },
	{ FLM_CONTROL_WPD, 1, 17, 0 },
};

static nthw_fpga_field_init_s flm_inf_data_fields[] = {
	{ FLM_INF_DATA_BYTES, 64, 0, 0x0000 }, { FLM_INF_DATA_CAUSE, 3, 224, 0x0000 },
	{ FLM_INF_DATA_EOR, 1, 287, 0x0000 }, { FLM_INF_DATA_ID, 32, 192, 0x0000 },
	{ FLM_INF_DATA_PACKETS, 64, 64, 0x0000 }, { FLM_INF_DATA_TS, 64, 128, 0x0000 },
};

static nthw_fpga_field_init_s flm_load_aps_fields[] = {
	{ FLM_LOAD_APS_APS, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_load_bin_fields[] = {
	{ FLM_LOAD_BIN_BIN, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_load_lps_fields[] = {
	{ FLM_LOAD_LPS_LPS, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_lrn_data_fields[] = {
	{ FLM_LRN_DATA_ADJ, 32, 480, 0x0000 }, { FLM_LRN_DATA_COLOR, 32, 448, 0x0000 },
	{ FLM_LRN_DATA_DSCP, 6, 698, 0x0000 }, { FLM_LRN_DATA_ENT, 1, 693, 0x0000 },
	{ FLM_LRN_DATA_EOR, 1, 767, 0x0000 }, { FLM_LRN_DATA_FILL, 16, 544, 0x0000 },
	{ FLM_LRN_DATA_FT, 4, 560, 0x0000 }, { FLM_LRN_DATA_FT_MBR, 4, 564, 0x0000 },
	{ FLM_LRN_DATA_FT_MISS, 4, 568, 0x0000 }, { FLM_LRN_DATA_ID, 32, 512, 0x0000 },
	{ FLM_LRN_DATA_KID, 8, 328, 0x0000 }, { FLM_LRN_DATA_MBR_ID1, 28, 572, 0x0000 },
	{ FLM_LRN_DATA_MBR_ID2, 28, 600, 0x0000 }, { FLM_LRN_DATA_MBR_ID3, 28, 628, 0x0000 },
	{ FLM_LRN_DATA_MBR_ID4, 28, 656, 0x0000 }, { FLM_LRN_DATA_NAT_EN, 1, 711, 0x0000 },
	{ FLM_LRN_DATA_NAT_IP, 32, 336, 0x0000 }, { FLM_LRN_DATA_NAT_PORT, 16, 400, 0x0000 },
	{ FLM_LRN_DATA_NOFI, 1, 716, 0x0000 }, { FLM_LRN_DATA_OP, 4, 694, 0x0000 },
	{ FLM_LRN_DATA_PRIO, 2, 691, 0x0000 }, { FLM_LRN_DATA_PROT, 8, 320, 0x0000 },
	{ FLM_LRN_DATA_QFI, 6, 704, 0x0000 }, { FLM_LRN_DATA_QW0, 128, 192, 0x0000 },
	{ FLM_LRN_DATA_QW4, 128, 64, 0x0000 }, { FLM_LRN_DATA_RATE, 16, 416, 0x0000 },
	{ FLM_LRN_DATA_RQI, 1, 710, 0x0000 }, { FLM_LRN_DATA_SCRUB_PROF, 4, 712, 0x0000 },
	{ FLM_LRN_DATA_SIZE, 16, 432, 0x0000 }, { FLM_LRN_DATA_STAT_PROF, 4, 687, 0x0000 },
	{ FLM_LRN_DATA_SW8, 32, 32, 0x0000 }, { FLM_LRN_DATA_SW9, 32, 0, 0x0000 },
	{ FLM_LRN_DATA_TEID, 32, 368, 0x0000 }, { FLM_LRN_DATA_VOL_IDX, 3, 684, 0x0000 },
};

static nthw_fpga_field_init_s flm_prio_fields[] = {
	{ FLM_PRIO_FT0, 4, 4, 1 }, { FLM_PRIO_FT1, 4, 12, 1 }, { FLM_PRIO_FT2, 4, 20, 1 },
	{ FLM_PRIO_FT3, 4, 28, 1 }, { FLM_PRIO_LIMIT0, 4, 0, 0 }, { FLM_PRIO_LIMIT1, 4, 8, 0 },
	{ FLM_PRIO_LIMIT2, 4, 16, 0 }, { FLM_PRIO_LIMIT3, 4, 24, 0 },
};

static nthw_fpga_field_init_s flm_pst_ctrl_fields[] = {
	{ FLM_PST_CTRL_ADR, 4, 0, 0x0000 },
	{ FLM_PST_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s flm_pst_data_fields[] = {
	{ FLM_PST_DATA_BP, 5, 0, 0x0000 },
	{ FLM_PST_DATA_PP, 5, 5, 0x0000 },
	{ FLM_PST_DATA_TP, 5, 10, 0x0000 },
};

static nthw_fpga_field_init_s flm_rcp_ctrl_fields[] = {
	{ FLM_RCP_CTRL_ADR, 5, 0, 0x0000 },
	{ FLM_RCP_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s flm_rcp_data_fields[] = {
	{ FLM_RCP_DATA_AUTO_IPV4_MASK, 1, 402, 0x0000 },
	{ FLM_RCP_DATA_BYT_DYN, 5, 387, 0x0000 },
	{ FLM_RCP_DATA_BYT_OFS, 8, 392, 0x0000 },
	{ FLM_RCP_DATA_IPN, 1, 386, 0x0000 },
	{ FLM_RCP_DATA_KID, 8, 377, 0x0000 },
	{ FLM_RCP_DATA_LOOKUP, 1, 0, 0x0000 },
	{ FLM_RCP_DATA_MASK, 320, 57, 0x0000 },
	{ FLM_RCP_DATA_OPN, 1, 385, 0x0000 },
	{ FLM_RCP_DATA_QW0_DYN, 5, 1, 0x0000 },
	{ FLM_RCP_DATA_QW0_OFS, 8, 6, 0x0000 },
	{ FLM_RCP_DATA_QW0_SEL, 2, 14, 0x0000 },
	{ FLM_RCP_DATA_QW4_DYN, 5, 16, 0x0000 },
	{ FLM_RCP_DATA_QW4_OFS, 8, 21, 0x0000 },
	{ FLM_RCP_DATA_SW8_DYN, 5, 29, 0x0000 },
	{ FLM_RCP_DATA_SW8_OFS, 8, 34, 0x0000 },
	{ FLM_RCP_DATA_SW8_SEL, 2, 42, 0x0000 },
	{ FLM_RCP_DATA_SW9_DYN, 5, 44, 0x0000 },
	{ FLM_RCP_DATA_SW9_OFS, 8, 49, 0x0000 },
	{ FLM_RCP_DATA_TXPLM, 2, 400, 0x0000 },
};

static nthw_fpga_field_init_s flm_scan_fields[] = {
	{ FLM_SCAN_I, 16, 0, 0 },
};

static nthw_fpga_field_init_s flm_scrub_ctrl_fields[] = {
	{ FLM_SCRUB_CTRL_ADR, 4, 0, 0x0000 },
	{ FLM_SCRUB_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s flm_scrub_data_fields[] = {
	{ FLM_SCRUB_DATA_DEL, 1, 12, 0 },
	{ FLM_SCRUB_DATA_INF, 1, 13, 0 },
	{ FLM_SCRUB_DATA_R, 4, 8, 0 },
	{ FLM_SCRUB_DATA_T, 8, 0, 0 },
};

static nthw_fpga_field_init_s flm_status_fields[] = {
	{ FLM_STATUS_CACHE_BUFFER_CRITICAL, 1, 12, 0x0000 },
	{ FLM_STATUS_CALIB_FAIL, 3, 3, 0 },
	{ FLM_STATUS_CALIB_SUCCESS, 3, 0, 0 },
	{ FLM_STATUS_CRCERR, 1, 10, 0x0000 },
	{ FLM_STATUS_CRITICAL, 1, 8, 0x0000 },
	{ FLM_STATUS_EFT_BP, 1, 11, 0x0000 },
	{ FLM_STATUS_IDLE, 1, 7, 0x0000 },
	{ FLM_STATUS_INITDONE, 1, 6, 0x0000 },
	{ FLM_STATUS_PANIC, 1, 9, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_aul_done_fields[] = {
	{ FLM_STAT_AUL_DONE_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_aul_fail_fields[] = {
	{ FLM_STAT_AUL_FAIL_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_aul_ignore_fields[] = {
	{ FLM_STAT_AUL_IGNORE_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_csh_hit_fields[] = {
	{ FLM_STAT_CSH_HIT_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_csh_miss_fields[] = {
	{ FLM_STAT_CSH_MISS_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_csh_unh_fields[] = {
	{ FLM_STAT_CSH_UNH_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_cuc_move_fields[] = {
	{ FLM_STAT_CUC_MOVE_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_cuc_start_fields[] = {
	{ FLM_STAT_CUC_START_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_flows_fields[] = {
	{ FLM_STAT_FLOWS_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_inf_done_fields[] = {
	{ FLM_STAT_INF_DONE_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_inf_skip_fields[] = {
	{ FLM_STAT_INF_SKIP_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_lrn_done_fields[] = {
	{ FLM_STAT_LRN_DONE_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_lrn_fail_fields[] = {
	{ FLM_STAT_LRN_FAIL_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_lrn_ignore_fields[] = {
	{ FLM_STAT_LRN_IGNORE_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_pck_dis_fields[] = {
	{ FLM_STAT_PCK_DIS_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_pck_hit_fields[] = {
	{ FLM_STAT_PCK_HIT_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_pck_miss_fields[] = {
	{ FLM_STAT_PCK_MISS_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_pck_unh_fields[] = {
	{ FLM_STAT_PCK_UNH_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_prb_done_fields[] = {
	{ FLM_STAT_PRB_DONE_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_prb_ignore_fields[] = {
	{ FLM_STAT_PRB_IGNORE_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_rel_done_fields[] = {
	{ FLM_STAT_REL_DONE_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_rel_ignore_fields[] = {
	{ FLM_STAT_REL_IGNORE_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_sta_done_fields[] = {
	{ FLM_STAT_STA_DONE_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_tul_done_fields[] = {
	{ FLM_STAT_TUL_DONE_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_unl_done_fields[] = {
	{ FLM_STAT_UNL_DONE_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_stat_unl_ignore_fields[] = {
	{ FLM_STAT_UNL_IGNORE_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s flm_sta_data_fields[] = {
	{ FLM_STA_DATA_EOR, 1, 95, 0x0000 }, { FLM_STA_DATA_ID, 32, 0, 0x0000 },
	{ FLM_STA_DATA_LDS, 1, 32, 0x0000 }, { FLM_STA_DATA_LFS, 1, 33, 0x0000 },
	{ FLM_STA_DATA_LIS, 1, 34, 0x0000 }, { FLM_STA_DATA_PDS, 1, 39, 0x0000 },
	{ FLM_STA_DATA_PIS, 1, 40, 0x0000 }, { FLM_STA_DATA_RDS, 1, 37, 0x0000 },
	{ FLM_STA_DATA_RIS, 1, 38, 0x0000 }, { FLM_STA_DATA_UDS, 1, 35, 0x0000 },
	{ FLM_STA_DATA_UIS, 1, 36, 0x0000 },
};

static nthw_fpga_register_init_s flm_registers[] = {
	{ FLM_BUF_CTRL, 14, 48, NTHW_FPGA_REG_TYPE_RW, 0, 3, flm_buf_ctrl_fields },
	{ FLM_CONTROL, 0, 31, NTHW_FPGA_REG_TYPE_MIXED, 134217728, 18, flm_control_fields },
	{ FLM_INF_DATA, 16, 288, NTHW_FPGA_REG_TYPE_RO, 0, 6, flm_inf_data_fields },
	{ FLM_LOAD_APS, 5, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_load_aps_fields },
	{ FLM_LOAD_BIN, 3, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, flm_load_bin_fields },
	{ FLM_LOAD_LPS, 4, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_load_lps_fields },
	{ FLM_LRN_DATA, 15, 768, NTHW_FPGA_REG_TYPE_WO, 0, 34, flm_lrn_data_fields },
	{ FLM_PRIO, 6, 32, NTHW_FPGA_REG_TYPE_WO, 269488144, 8, flm_prio_fields },
	{ FLM_PST_CTRL, 12, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, flm_pst_ctrl_fields },
	{ FLM_PST_DATA, 13, 15, NTHW_FPGA_REG_TYPE_WO, 0, 3, flm_pst_data_fields },
	{ FLM_RCP_CTRL, 8, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, flm_rcp_ctrl_fields },
	{ FLM_RCP_DATA, 9, 403, NTHW_FPGA_REG_TYPE_WO, 0, 19, flm_rcp_data_fields },
	{ FLM_SCAN, 2, 16, NTHW_FPGA_REG_TYPE_WO, 0, 1, flm_scan_fields },
	{ FLM_SCRUB_CTRL, 10, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, flm_scrub_ctrl_fields },
	{ FLM_SCRUB_DATA, 11, 14, NTHW_FPGA_REG_TYPE_WO, 0, 4, flm_scrub_data_fields },
	{ FLM_STATUS, 1, 17, NTHW_FPGA_REG_TYPE_MIXED, 0, 9, flm_status_fields },
	{ FLM_STAT_AUL_DONE, 41, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_aul_done_fields },
	{ FLM_STAT_AUL_FAIL, 43, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_aul_fail_fields },
	{ FLM_STAT_AUL_IGNORE, 42, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_aul_ignore_fields },
	{ FLM_STAT_CSH_HIT, 52, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_csh_hit_fields },
	{ FLM_STAT_CSH_MISS, 53, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_csh_miss_fields },
	{ FLM_STAT_CSH_UNH, 54, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_csh_unh_fields },
	{ FLM_STAT_CUC_MOVE, 56, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_cuc_move_fields },
	{ FLM_STAT_CUC_START, 55, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_cuc_start_fields },
	{ FLM_STAT_FLOWS, 18, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_flows_fields },
	{ FLM_STAT_INF_DONE, 46, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_inf_done_fields },
	{ FLM_STAT_INF_SKIP, 47, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_inf_skip_fields },
	{ FLM_STAT_LRN_DONE, 32, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_lrn_done_fields },
	{ FLM_STAT_LRN_FAIL, 34, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_lrn_fail_fields },
	{ FLM_STAT_LRN_IGNORE, 33, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_lrn_ignore_fields },
	{ FLM_STAT_PCK_DIS, 51, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_pck_dis_fields },
	{ FLM_STAT_PCK_HIT, 48, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_pck_hit_fields },
	{ FLM_STAT_PCK_MISS, 49, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_pck_miss_fields },
	{ FLM_STAT_PCK_UNH, 50, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_pck_unh_fields },
	{ FLM_STAT_PRB_DONE, 39, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_prb_done_fields },
	{ FLM_STAT_PRB_IGNORE, 40, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_prb_ignore_fields },
	{ FLM_STAT_REL_DONE, 37, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_rel_done_fields },
	{ FLM_STAT_REL_IGNORE, 38, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_rel_ignore_fields },
	{ FLM_STAT_STA_DONE, 45, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_sta_done_fields },
	{ FLM_STAT_TUL_DONE, 44, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_tul_done_fields },
	{ FLM_STAT_UNL_DONE, 35, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_unl_done_fields },
	{ FLM_STAT_UNL_IGNORE, 36, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, flm_stat_unl_ignore_fields },
	{ FLM_STA_DATA, 17, 96, NTHW_FPGA_REG_TYPE_RO, 0, 11, flm_sta_data_fields },
};

static nthw_fpga_field_init_s gfg_burstsize0_fields[] = {
	{ GFG_BURSTSIZE0_VAL, 24, 0, 0 },
};

static nthw_fpga_field_init_s gfg_burstsize1_fields[] = {
	{ GFG_BURSTSIZE1_VAL, 24, 0, 0 },
};

static nthw_fpga_field_init_s gfg_ctrl0_fields[] = {
	{ GFG_CTRL0_ENABLE, 1, 0, 0 },
	{ GFG_CTRL0_MODE, 3, 1, 0 },
	{ GFG_CTRL0_PRBS_EN, 1, 4, 0 },
	{ GFG_CTRL0_SIZE, 14, 16, 64 },
};

static nthw_fpga_field_init_s gfg_ctrl1_fields[] = {
	{ GFG_CTRL1_ENABLE, 1, 0, 0 },
	{ GFG_CTRL1_MODE, 3, 1, 0 },
	{ GFG_CTRL1_PRBS_EN, 1, 4, 0 },
	{ GFG_CTRL1_SIZE, 14, 16, 64 },
};

static nthw_fpga_field_init_s gfg_run0_fields[] = {
	{ GFG_RUN0_RUN, 1, 0, 0 },
};

static nthw_fpga_field_init_s gfg_run1_fields[] = {
	{ GFG_RUN1_RUN, 1, 0, 0 },
};

static nthw_fpga_field_init_s gfg_sizemask0_fields[] = {
	{ GFG_SIZEMASK0_VAL, 14, 0, 0 },
};

static nthw_fpga_field_init_s gfg_sizemask1_fields[] = {
	{ GFG_SIZEMASK1_VAL, 14, 0, 0 },
};

static nthw_fpga_field_init_s gfg_streamid0_fields[] = {
	{ GFG_STREAMID0_VAL, 8, 0, 0 },
};

static nthw_fpga_field_init_s gfg_streamid1_fields[] = {
	{ GFG_STREAMID1_VAL, 8, 0, 1 },
};

static nthw_fpga_register_init_s gfg_registers[] = {
	{ GFG_BURSTSIZE0, 3, 24, NTHW_FPGA_REG_TYPE_WO, 0, 1, gfg_burstsize0_fields },
	{ GFG_BURSTSIZE1, 8, 24, NTHW_FPGA_REG_TYPE_WO, 0, 1, gfg_burstsize1_fields },
	{ GFG_CTRL0, 0, 30, NTHW_FPGA_REG_TYPE_WO, 4194304, 4, gfg_ctrl0_fields },
	{ GFG_CTRL1, 5, 30, NTHW_FPGA_REG_TYPE_WO, 4194304, 4, gfg_ctrl1_fields },
	{ GFG_RUN0, 1, 1, NTHW_FPGA_REG_TYPE_WO, 0, 1, gfg_run0_fields },
	{ GFG_RUN1, 6, 1, NTHW_FPGA_REG_TYPE_WO, 0, 1, gfg_run1_fields },
	{ GFG_SIZEMASK0, 4, 14, NTHW_FPGA_REG_TYPE_WO, 0, 1, gfg_sizemask0_fields },
	{ GFG_SIZEMASK1, 9, 14, NTHW_FPGA_REG_TYPE_WO, 0, 1, gfg_sizemask1_fields },
	{ GFG_STREAMID0, 2, 8, NTHW_FPGA_REG_TYPE_WO, 0, 1, gfg_streamid0_fields },
	{ GFG_STREAMID1, 7, 8, NTHW_FPGA_REG_TYPE_WO, 1, 1, gfg_streamid1_fields },
};

static nthw_fpga_field_init_s gmf_ctrl_fields[] = {
	{ GMF_CTRL_ENABLE, 1, 0, 0 },
	{ GMF_CTRL_FCS_ALWAYS, 1, 1, 0 },
	{ GMF_CTRL_IFG_AUTO_ADJUST_ENABLE, 1, 7, 0 },
	{ GMF_CTRL_IFG_ENABLE, 1, 2, 0 },
	{ GMF_CTRL_IFG_TX_NOW_ALWAYS, 1, 3, 0 },
	{ GMF_CTRL_IFG_TX_NOW_ON_TS_ENABLE, 1, 5, 0 },
	{ GMF_CTRL_IFG_TX_ON_TS_ADJUST_ON_SET_CLOCK, 1, 6, 0 },
	{ GMF_CTRL_IFG_TX_ON_TS_ALWAYS, 1, 4, 0 },
	{ GMF_CTRL_TS_INJECT_ALWAYS, 1, 8, 0 },
	{ GMF_CTRL_TS_INJECT_DUAL_STEP, 1, 9, 0 },
};

static nthw_fpga_field_init_s gmf_debug_lane_marker_fields[] = {
	{ GMF_DEBUG_LANE_MARKER_COMPENSATION, 16, 0, 16384 },
};

static nthw_fpga_field_init_s gmf_ifg_max_adjust_slack_fields[] = {
	{ GMF_IFG_MAX_ADJUST_SLACK_SLACK, 64, 0, 0 },
};

static nthw_fpga_field_init_s gmf_ifg_set_clock_delta_fields[] = {
	{ GMF_IFG_SET_CLOCK_DELTA_DELTA, 64, 0, 0 },
};

static nthw_fpga_field_init_s gmf_ifg_set_clock_delta_adjust_fields[] = {
	{ GMF_IFG_SET_CLOCK_DELTA_ADJUST_DELTA, 64, 0, 0 },
};

static nthw_fpga_field_init_s gmf_ifg_tx_now_on_ts_fields[] = {
	{ GMF_IFG_TX_NOW_ON_TS_TS, 64, 0, 0 },
};

static nthw_fpga_field_init_s gmf_speed_fields[] = {
	{ GMF_SPEED_IFG_SPEED, 64, 0, 0 },
};

static nthw_fpga_field_init_s gmf_stat_data_buffer_fields[] = {
	{ GMF_STAT_DATA_BUFFER_USED, 15, 0, 0x0000 },
};

static nthw_fpga_field_init_s gmf_stat_max_delayed_pkt_fields[] = {
	{ GMF_STAT_MAX_DELAYED_PKT_NS, 64, 0, 0 },
};

static nthw_fpga_field_init_s gmf_stat_next_pkt_fields[] = {
	{ GMF_STAT_NEXT_PKT_NS, 64, 0, 0 },
};

static nthw_fpga_field_init_s gmf_stat_sticky_fields[] = {
	{ GMF_STAT_STICKY_DATA_UNDERFLOWED, 1, 0, 0 },
	{ GMF_STAT_STICKY_IFG_ADJUSTED, 1, 1, 0 },
};

static nthw_fpga_field_init_s gmf_ts_inject_fields[] = {
	{ GMF_TS_INJECT_OFFSET, 14, 0, 0 },
	{ GMF_TS_INJECT_POS, 2, 14, 0 },
};

static nthw_fpga_register_init_s gmf_registers[] = {
	{ GMF_CTRL, 0, 10, NTHW_FPGA_REG_TYPE_WO, 0, 10, gmf_ctrl_fields },
	{
		GMF_DEBUG_LANE_MARKER, 7, 16, NTHW_FPGA_REG_TYPE_WO, 16384, 1,
		gmf_debug_lane_marker_fields
	},
	{
		GMF_IFG_MAX_ADJUST_SLACK, 4, 64, NTHW_FPGA_REG_TYPE_WO, 0, 1,
		gmf_ifg_max_adjust_slack_fields
	},
	{
		GMF_IFG_SET_CLOCK_DELTA, 2, 64, NTHW_FPGA_REG_TYPE_WO, 0, 1,
		gmf_ifg_set_clock_delta_fields
	},
	{
		GMF_IFG_SET_CLOCK_DELTA_ADJUST, 3, 64, NTHW_FPGA_REG_TYPE_WO, 0, 1,
		gmf_ifg_set_clock_delta_adjust_fields
	},
	{ GMF_IFG_TX_NOW_ON_TS, 5, 64, NTHW_FPGA_REG_TYPE_WO, 0, 1, gmf_ifg_tx_now_on_ts_fields },
	{ GMF_SPEED, 1, 64, NTHW_FPGA_REG_TYPE_WO, 0, 1, gmf_speed_fields },
	{ GMF_STAT_DATA_BUFFER, 9, 15, NTHW_FPGA_REG_TYPE_RO, 0, 1, gmf_stat_data_buffer_fields },
	{
		GMF_STAT_MAX_DELAYED_PKT, 11, 64, NTHW_FPGA_REG_TYPE_RC1, 0, 1,
		gmf_stat_max_delayed_pkt_fields
	},
	{ GMF_STAT_NEXT_PKT, 10, 64, NTHW_FPGA_REG_TYPE_RO, 0, 1, gmf_stat_next_pkt_fields },
	{ GMF_STAT_STICKY, 8, 2, NTHW_FPGA_REG_TYPE_RC1, 0, 2, gmf_stat_sticky_fields },
	{ GMF_TS_INJECT, 6, 16, NTHW_FPGA_REG_TYPE_WO, 0, 2, gmf_ts_inject_fields },
};

static nthw_fpga_field_init_s hfu_rcp_ctrl_fields[] = {
	{ HFU_RCP_CTRL_ADR, 6, 0, 0x0000 },
	{ HFU_RCP_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s hfu_rcp_data_fields[] = {
	{ HFU_RCP_DATA_LEN_A_ADD_DYN, 5, 15, 0x0000 },
	{ HFU_RCP_DATA_LEN_A_ADD_OFS, 8, 20, 0x0000 },
	{ HFU_RCP_DATA_LEN_A_OL4LEN, 1, 1, 0x0000 },
	{ HFU_RCP_DATA_LEN_A_POS_DYN, 5, 2, 0x0000 },
	{ HFU_RCP_DATA_LEN_A_POS_OFS, 8, 7, 0x0000 },
	{ HFU_RCP_DATA_LEN_A_SUB_DYN, 5, 28, 0x0000 },
	{ HFU_RCP_DATA_LEN_A_WR, 1, 0, 0x0000 },
	{ HFU_RCP_DATA_LEN_B_ADD_DYN, 5, 47, 0x0000 },
	{ HFU_RCP_DATA_LEN_B_ADD_OFS, 8, 52, 0x0000 },
	{ HFU_RCP_DATA_LEN_B_POS_DYN, 5, 34, 0x0000 },
	{ HFU_RCP_DATA_LEN_B_POS_OFS, 8, 39, 0x0000 },
	{ HFU_RCP_DATA_LEN_B_SUB_DYN, 5, 60, 0x0000 },
	{ HFU_RCP_DATA_LEN_B_WR, 1, 33, 0x0000 },
	{ HFU_RCP_DATA_LEN_C_ADD_DYN, 5, 79, 0x0000 },
	{ HFU_RCP_DATA_LEN_C_ADD_OFS, 8, 84, 0x0000 },
	{ HFU_RCP_DATA_LEN_C_POS_DYN, 5, 66, 0x0000 },
	{ HFU_RCP_DATA_LEN_C_POS_OFS, 8, 71, 0x0000 },
	{ HFU_RCP_DATA_LEN_C_SUB_DYN, 5, 92, 0x0000 },
	{ HFU_RCP_DATA_LEN_C_WR, 1, 65, 0x0000 },
	{ HFU_RCP_DATA_TTL_POS_DYN, 5, 98, 0x0000 },
	{ HFU_RCP_DATA_TTL_POS_OFS, 8, 103, 0x0000 },
	{ HFU_RCP_DATA_TTL_WR, 1, 97, 0x0000 },
};

static nthw_fpga_register_init_s hfu_registers[] = {
	{ HFU_RCP_CTRL, 0, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, hfu_rcp_ctrl_fields },
	{ HFU_RCP_DATA, 1, 111, NTHW_FPGA_REG_TYPE_WO, 0, 22, hfu_rcp_data_fields },
};

static nthw_fpga_field_init_s hif_build_time_fields[] = {
	{ HIF_BUILD_TIME_TIME, 32, 0, 1726787043 },
};

static nthw_fpga_field_init_s hif_config_fields[] = {
	{ HIF_CONFIG_EXT_TAG, 1, 6, 0x0000 },
	{ HIF_CONFIG_MAX_READ, 3, 3, 0x0000 },
	{ HIF_CONFIG_MAX_TLP, 3, 0, 0x0000 },
};

static nthw_fpga_field_init_s hif_control_fields[] = {
	{ HIF_CONTROL_BLESSED, 8, 4, 0 },
	{ HIF_CONTROL_FSR, 1, 12, 1 },
	{ HIF_CONTROL_WRAW, 4, 0, 1 },
};

static nthw_fpga_field_init_s hif_prod_id_ex_fields[] = {
	{ HIF_PROD_ID_EX_LAYOUT, 1, 31, 0 },
	{ HIF_PROD_ID_EX_LAYOUT_VERSION, 8, 0, 1 },
	{ HIF_PROD_ID_EX_RESERVED, 23, 8, 0 },
};

static nthw_fpga_field_init_s hif_prod_id_lsb_fields[] = {
	{ HIF_PROD_ID_LSB_GROUP_ID, 16, 16, 9569 },
	{ HIF_PROD_ID_LSB_REV_ID, 8, 0, 49 },
	{ HIF_PROD_ID_LSB_VER_ID, 8, 8, 55 },
};

static nthw_fpga_field_init_s hif_prod_id_msb_fields[] = {
	{ HIF_PROD_ID_MSB_BUILD_NO, 10, 12, 0 },
	{ HIF_PROD_ID_MSB_TYPE_ID, 12, 0, 200 },
};

static nthw_fpga_field_init_s hif_sample_time_fields[] = {
	{ HIF_SAMPLE_TIME_SAMPLE_TIME, 1, 0, 0x0000 },
};

static nthw_fpga_field_init_s hif_status_fields[] = {
	{ HIF_STATUS_RD_ERR, 1, 9, 0 },
	{ HIF_STATUS_TAGS_IN_USE, 8, 0, 0 },
	{ HIF_STATUS_WR_ERR, 1, 8, 0 },
};

static nthw_fpga_field_init_s hif_stat_ctrl_fields[] = {
	{ HIF_STAT_CTRL_STAT_ENA, 1, 1, 0 },
	{ HIF_STAT_CTRL_STAT_REQ, 1, 0, 0 },
};

static nthw_fpga_field_init_s hif_stat_refclk_fields[] = {
	{ HIF_STAT_REFCLK_REFCLK250, 32, 0, 0 },
};

static nthw_fpga_field_init_s hif_stat_rx_fields[] = {
	{ HIF_STAT_RX_COUNTER, 32, 0, 0 },
};

static nthw_fpga_field_init_s hif_stat_tx_fields[] = {
	{ HIF_STAT_TX_COUNTER, 32, 0, 0 },
};

static nthw_fpga_field_init_s hif_test0_fields[] = {
	{ HIF_TEST0_DATA, 32, 0, 287454020 },
};

static nthw_fpga_field_init_s hif_test1_fields[] = {
	{ HIF_TEST1_DATA, 32, 0, 2864434397 },
};

static nthw_fpga_field_init_s hif_uuid0_fields[] = {
	{ HIF_UUID0_UUID0, 32, 0, 1604494892 },
};

static nthw_fpga_field_init_s hif_uuid1_fields[] = {
	{ HIF_UUID1_UUID1, 32, 0, 3171872420 },
};

static nthw_fpga_field_init_s hif_uuid2_fields[] = {
	{ HIF_UUID2_UUID2, 32, 0, 1942500832 },
};

static nthw_fpga_field_init_s hif_uuid3_fields[] = {
	{ HIF_UUID3_UUID3, 32, 0, 3282253638 },
};

static nthw_fpga_register_init_s hif_registers[] = {
	{ HIF_BUILD_TIME, 16, 32, NTHW_FPGA_REG_TYPE_RO, 1726787043, 1, hif_build_time_fields },
	{ HIF_CONFIG, 24, 7, NTHW_FPGA_REG_TYPE_RW, 0, 3, hif_config_fields },
	{ HIF_CONTROL, 40, 13, NTHW_FPGA_REG_TYPE_MIXED, 4097, 3, hif_control_fields },
	{ HIF_PROD_ID_EX, 112, 32, NTHW_FPGA_REG_TYPE_RO, 1, 3, hif_prod_id_ex_fields },
	{ HIF_PROD_ID_LSB, 0, 32, NTHW_FPGA_REG_TYPE_RO, 627128113, 3, hif_prod_id_lsb_fields },
	{ HIF_PROD_ID_MSB, 8, 22, NTHW_FPGA_REG_TYPE_RO, 200, 2, hif_prod_id_msb_fields },
	{ HIF_SAMPLE_TIME, 96, 1, NTHW_FPGA_REG_TYPE_WO, 0, 1, hif_sample_time_fields },
	{ HIF_STATUS, 32, 10, NTHW_FPGA_REG_TYPE_MIXED, 0, 3, hif_status_fields },
	{ HIF_STAT_CTRL, 64, 2, NTHW_FPGA_REG_TYPE_WO, 0, 2, hif_stat_ctrl_fields },
	{ HIF_STAT_REFCLK, 72, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, hif_stat_refclk_fields },
	{ HIF_STAT_RX, 88, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, hif_stat_rx_fields },
	{ HIF_STAT_TX, 80, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, hif_stat_tx_fields },
	{ HIF_TEST0, 48, 32, NTHW_FPGA_REG_TYPE_RW, 287454020, 1, hif_test0_fields },
	{ HIF_TEST1, 56, 32, NTHW_FPGA_REG_TYPE_RW, 2864434397, 1, hif_test1_fields },
	{ HIF_UUID0, 128, 32, NTHW_FPGA_REG_TYPE_RO, 1604494892, 1, hif_uuid0_fields },
	{ HIF_UUID1, 144, 32, NTHW_FPGA_REG_TYPE_RO, 3171872420, 1, hif_uuid1_fields },
	{ HIF_UUID2, 160, 32, NTHW_FPGA_REG_TYPE_RO, 1942500832, 1, hif_uuid2_fields },
	{ HIF_UUID3, 176, 32, NTHW_FPGA_REG_TYPE_RO, 3282253638, 1, hif_uuid3_fields },
};

static nthw_fpga_field_init_s hsh_rcp_ctrl_fields[] = {
	{ HSH_RCP_CTRL_ADR, 4, 0, 0x0000 },
	{ HSH_RCP_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s hsh_rcp_data_fields[] = {
	{ HSH_RCP_DATA_AUTO_IPV4_MASK, 1, 742, 0x0000 },
	{ HSH_RCP_DATA_HSH_TYPE, 5, 416, 0x0000 },
	{ HSH_RCP_DATA_HSH_VALID, 1, 415, 0x0000 },
	{ HSH_RCP_DATA_K, 320, 422, 0x0000 },
	{ HSH_RCP_DATA_LOAD_DIST_TYPE, 2, 0, 0x0000 },
	{ HSH_RCP_DATA_MAC_PORT_MASK, 2, 2, 0x0000 },
	{ HSH_RCP_DATA_P_MASK, 1, 61, 0x0000 },
	{ HSH_RCP_DATA_QW0_OFS, 8, 11, 0x0000 },
	{ HSH_RCP_DATA_QW0_PE, 5, 6, 0x0000 },
	{ HSH_RCP_DATA_QW4_OFS, 8, 24, 0x0000 },
	{ HSH_RCP_DATA_QW4_PE, 5, 19, 0x0000 },
	{ HSH_RCP_DATA_SEED, 32, 382, 0x0000 },
	{ HSH_RCP_DATA_SORT, 2, 4, 0x0000 },
	{ HSH_RCP_DATA_TNL_P, 1, 414, 0x0000 },
	{ HSH_RCP_DATA_TOEPLITZ, 1, 421, 0x0000 },
	{ HSH_RCP_DATA_W8_OFS, 8, 37, 0x0000 },
	{ HSH_RCP_DATA_W8_PE, 5, 32, 0x0000 },
	{ HSH_RCP_DATA_W8_SORT, 1, 45, 0x0000 },
	{ HSH_RCP_DATA_W9_OFS, 8, 51, 0x0000 },
	{ HSH_RCP_DATA_W9_P, 1, 60, 0x0000 },
	{ HSH_RCP_DATA_W9_PE, 5, 46, 0x0000 },
	{ HSH_RCP_DATA_W9_SORT, 1, 59, 0x0000 },
	{ HSH_RCP_DATA_WORD_MASK, 320, 62, 0x0000 },
};

static nthw_fpga_register_init_s hsh_registers[] = {
	{ HSH_RCP_CTRL, 0, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, hsh_rcp_ctrl_fields },
	{ HSH_RCP_DATA, 1, 743, NTHW_FPGA_REG_TYPE_WO, 0, 23, hsh_rcp_data_fields },
};

static nthw_fpga_field_init_s i2cm_cmd_status_fields[] = {
	{ I2CM_CMD_STATUS_CMD_STATUS, 8, 0, 0 },
};

static nthw_fpga_field_init_s i2cm_ctrl_fields[] = {
	{ I2CM_CTRL_EN, 1, 7, 0 },
	{ I2CM_CTRL_IEN, 1, 6, 0 },
};

static nthw_fpga_field_init_s i2cm_data_fields[] = {
	{ I2CM_DATA_DATA, 8, 0, 0 },
};

static nthw_fpga_field_init_s i2cm_io_exp_fields[] = {
	{ I2CM_IO_EXP_INT_B, 1, 1, 0 },
	{ I2CM_IO_EXP_RST, 1, 0, 0 },
};

static nthw_fpga_field_init_s i2cm_prer_high_fields[] = {
	{ I2CM_PRER_HIGH_PRER_HIGH, 8, 0, 255 },
};

static nthw_fpga_field_init_s i2cm_prer_low_fields[] = {
	{ I2CM_PRER_LOW_PRER_LOW, 8, 0, 255 },
};

static nthw_fpga_field_init_s i2cm_select_fields[] = {
	{ I2CM_SELECT_SELECT, 2, 0, 0 },
};

static nthw_fpga_register_init_s i2cm_registers[] = {
	{ I2CM_CMD_STATUS, 4, 8, NTHW_FPGA_REG_TYPE_RW, 0, 1, i2cm_cmd_status_fields },
	{ I2CM_CTRL, 2, 8, NTHW_FPGA_REG_TYPE_RW, 0, 2, i2cm_ctrl_fields },
	{ I2CM_DATA, 3, 8, NTHW_FPGA_REG_TYPE_RW, 0, 1, i2cm_data_fields },
	{ I2CM_IO_EXP, 6, 2, NTHW_FPGA_REG_TYPE_MIXED, 0, 2, i2cm_io_exp_fields },
	{ I2CM_PRER_HIGH, 1, 8, NTHW_FPGA_REG_TYPE_RW, 255, 1, i2cm_prer_high_fields },
	{ I2CM_PRER_LOW, 0, 8, NTHW_FPGA_REG_TYPE_RW, 255, 1, i2cm_prer_low_fields },
	{ I2CM_SELECT, 5, 2, NTHW_FPGA_REG_TYPE_RW, 0, 1, i2cm_select_fields },
};

static nthw_fpga_field_init_s ifr_counters_ctrl_fields[] = {
	{ IFR_COUNTERS_CTRL_ADR, 4, 0, 0x0000 },
	{ IFR_COUNTERS_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s ifr_counters_data_fields[] = {
	{ IFR_COUNTERS_DATA_DROP, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s ifr_df_buf_ctrl_fields[] = {
	{ IFR_DF_BUF_CTRL_AVAILABLE, 11, 0, 0x0000 },
	{ IFR_DF_BUF_CTRL_MTU_PROFILE, 16, 11, 0x0000 },
};

static nthw_fpga_field_init_s ifr_df_buf_data_fields[] = {
	{ IFR_DF_BUF_DATA_FIFO_DAT, 128, 0, 0x0000 },
};

static nthw_fpga_field_init_s ifr_rcp_ctrl_fields[] = {
	{ IFR_RCP_CTRL_ADR, 4, 0, 0x0000 },
	{ IFR_RCP_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s ifr_rcp_data_fields[] = {
	{ IFR_RCP_DATA_IPV4_DF_DROP, 1, 17, 0x0000 }, { IFR_RCP_DATA_IPV4_EN, 1, 0, 0x0000 },
	{ IFR_RCP_DATA_IPV6_DROP, 1, 16, 0x0000 }, { IFR_RCP_DATA_IPV6_EN, 1, 1, 0x0000 },
	{ IFR_RCP_DATA_MTU, 14, 2, 0x0000 },
};

static nthw_fpga_register_init_s ifr_registers[] = {
	{ IFR_COUNTERS_CTRL, 4, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, ifr_counters_ctrl_fields },
	{ IFR_COUNTERS_DATA, 5, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, ifr_counters_data_fields },
	{ IFR_DF_BUF_CTRL, 2, 27, NTHW_FPGA_REG_TYPE_RO, 0, 2, ifr_df_buf_ctrl_fields },
	{ IFR_DF_BUF_DATA, 3, 128, NTHW_FPGA_REG_TYPE_RO, 0, 1, ifr_df_buf_data_fields },
	{ IFR_RCP_CTRL, 0, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, ifr_rcp_ctrl_fields },
	{ IFR_RCP_DATA, 1, 18, NTHW_FPGA_REG_TYPE_WO, 0, 5, ifr_rcp_data_fields },
};

static nthw_fpga_field_init_s igam_base_fields[] = {
	{ IGAM_BASE_BUSY, 1, 30, 0x0000 },
	{ IGAM_BASE_CMD, 1, 31, 0x0000 },
	{ IGAM_BASE_PTR, 30, 0, 0x0000 },
};

static nthw_fpga_field_init_s igam_ctrl_fields[] = {
	{ IGAM_CTRL_FORCE_RST, 1, 0, 0 },
	{ IGAM_CTRL_FORWARD_RST, 1, 1, 1 },
};

static nthw_fpga_field_init_s igam_data_fields[] = {
	{ IGAM_DATA_DATA, 32, 0, 0x0000 },
};

static nthw_fpga_register_init_s igam_registers[] = {
	{ IGAM_BASE, 0, 32, NTHW_FPGA_REG_TYPE_MIXED, 0, 3, igam_base_fields },
	{ IGAM_CTRL, 2, 2, NTHW_FPGA_REG_TYPE_RW, 2, 2, igam_ctrl_fields },
	{ IGAM_DATA, 1, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1, igam_data_fields },
};

static nthw_fpga_field_init_s ins_rcp_ctrl_fields[] = {
	{ INS_RCP_CTRL_ADR, 4, 0, 0x0000 },
	{ INS_RCP_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s ins_rcp_data_fields[] = {
	{ INS_RCP_DATA_DYN, 5, 0, 0x0000 },
	{ INS_RCP_DATA_LEN, 8, 15, 0x0000 },
	{ INS_RCP_DATA_OFS, 10, 5, 0x0000 },
};

static nthw_fpga_register_init_s ins_registers[] = {
	{ INS_RCP_CTRL, 0, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, ins_rcp_ctrl_fields },
	{ INS_RCP_DATA, 1, 23, NTHW_FPGA_REG_TYPE_WO, 0, 3, ins_rcp_data_fields },
};

static nthw_fpga_field_init_s km_cam_ctrl_fields[] = {
	{ KM_CAM_CTRL_ADR, 13, 0, 0x0000 },
	{ KM_CAM_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s km_cam_data_fields[] = {
	{ KM_CAM_DATA_FT0, 4, 192, 0x0000 }, { KM_CAM_DATA_FT1, 4, 196, 0x0000 },
	{ KM_CAM_DATA_FT2, 4, 200, 0x0000 }, { KM_CAM_DATA_FT3, 4, 204, 0x0000 },
	{ KM_CAM_DATA_FT4, 4, 208, 0x0000 }, { KM_CAM_DATA_FT5, 4, 212, 0x0000 },
	{ KM_CAM_DATA_W0, 32, 0, 0x0000 }, { KM_CAM_DATA_W1, 32, 32, 0x0000 },
	{ KM_CAM_DATA_W2, 32, 64, 0x0000 }, { KM_CAM_DATA_W3, 32, 96, 0x0000 },
	{ KM_CAM_DATA_W4, 32, 128, 0x0000 }, { KM_CAM_DATA_W5, 32, 160, 0x0000 },
};

static nthw_fpga_field_init_s km_rcp_ctrl_fields[] = {
	{ KM_RCP_CTRL_ADR, 5, 0, 0x0000 },
	{ KM_RCP_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s km_rcp_data_fields[] = {
	{ KM_RCP_DATA_BANK_A, 12, 694, 0x0000 }, { KM_RCP_DATA_BANK_B, 12, 706, 0x0000 },
	{ KM_RCP_DATA_DUAL, 1, 651, 0x0000 }, { KM_RCP_DATA_DW0_B_DYN, 5, 729, 0x0000 },
	{ KM_RCP_DATA_DW0_B_OFS, 8, 734, 0x0000 }, { KM_RCP_DATA_DW10_DYN, 5, 55, 0x0000 },
	{ KM_RCP_DATA_DW10_OFS, 8, 60, 0x0000 }, { KM_RCP_DATA_DW10_SEL_A, 2, 68, 0x0000 },
	{ KM_RCP_DATA_DW10_SEL_B, 2, 70, 0x0000 }, { KM_RCP_DATA_DW2_B_DYN, 5, 742, 0x0000 },
	{ KM_RCP_DATA_DW2_B_OFS, 8, 747, 0x0000 }, { KM_RCP_DATA_DW8_DYN, 5, 36, 0x0000 },
	{ KM_RCP_DATA_DW8_OFS, 8, 41, 0x0000 }, { KM_RCP_DATA_DW8_SEL_A, 3, 49, 0x0000 },
	{ KM_RCP_DATA_DW8_SEL_B, 3, 52, 0x0000 }, { KM_RCP_DATA_EL_A, 4, 653, 0x0000 },
	{ KM_RCP_DATA_EL_B, 3, 657, 0x0000 }, { KM_RCP_DATA_FTM_A, 16, 662, 0x0000 },
	{ KM_RCP_DATA_FTM_B, 16, 678, 0x0000 }, { KM_RCP_DATA_INFO_A, 1, 660, 0x0000 },
	{ KM_RCP_DATA_INFO_B, 1, 661, 0x0000 }, { KM_RCP_DATA_KEYWAY_A, 1, 725, 0x0000 },
	{ KM_RCP_DATA_KEYWAY_B, 1, 726, 0x0000 }, { KM_RCP_DATA_KL_A, 4, 718, 0x0000 },
	{ KM_RCP_DATA_KL_B, 3, 722, 0x0000 }, { KM_RCP_DATA_MASK_A, 384, 75, 0x0000 },
	{ KM_RCP_DATA_MASK_B, 192, 459, 0x0000 }, { KM_RCP_DATA_PAIRED, 1, 652, 0x0000 },
	{ KM_RCP_DATA_QW0_DYN, 5, 0, 0x0000 }, { KM_RCP_DATA_QW0_OFS, 8, 5, 0x0000 },
	{ KM_RCP_DATA_QW0_SEL_A, 3, 13, 0x0000 }, { KM_RCP_DATA_QW0_SEL_B, 3, 16, 0x0000 },
	{ KM_RCP_DATA_QW4_DYN, 5, 19, 0x0000 }, { KM_RCP_DATA_QW4_OFS, 8, 24, 0x0000 },
	{ KM_RCP_DATA_QW4_SEL_A, 2, 32, 0x0000 }, { KM_RCP_DATA_QW4_SEL_B, 2, 34, 0x0000 },
	{ KM_RCP_DATA_SW4_B_DYN, 5, 755, 0x0000 }, { KM_RCP_DATA_SW4_B_OFS, 8, 760, 0x0000 },
	{ KM_RCP_DATA_SW5_B_DYN, 5, 768, 0x0000 }, { KM_RCP_DATA_SW5_B_OFS, 8, 773, 0x0000 },
	{ KM_RCP_DATA_SWX_CCH, 1, 72, 0x0000 }, { KM_RCP_DATA_SWX_SEL_A, 1, 73, 0x0000 },
	{ KM_RCP_DATA_SWX_SEL_B, 1, 74, 0x0000 }, { KM_RCP_DATA_SYNERGY_MODE, 2, 727, 0x0000 },
};

static nthw_fpga_field_init_s km_status_fields[] = {
	{ KM_STATUS_TCQ_RDY, 1, 0, 0x0000 },
};

static nthw_fpga_field_init_s km_tcam_ctrl_fields[] = {
	{ KM_TCAM_CTRL_ADR, 14, 0, 0x0000 },
	{ KM_TCAM_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s km_tcam_data_fields[] = {
	{ KM_TCAM_DATA_T, 72, 0, 0x0000 },
};

static nthw_fpga_field_init_s km_tci_ctrl_fields[] = {
	{ KM_TCI_CTRL_ADR, 10, 0, 0x0000 },
	{ KM_TCI_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s km_tci_data_fields[] = {
	{ KM_TCI_DATA_COLOR, 32, 0, 0x0000 },
	{ KM_TCI_DATA_FT, 4, 32, 0x0000 },
};

static nthw_fpga_field_init_s km_tcq_ctrl_fields[] = {
	{ KM_TCQ_CTRL_ADR, 7, 0, 0x0000 },
	{ KM_TCQ_CTRL_CNT, 5, 16, 0x0000 },
};

static nthw_fpga_field_init_s km_tcq_data_fields[] = {
	{ KM_TCQ_DATA_BANK_MASK, 12, 0, 0x0000 },
	{ KM_TCQ_DATA_QUAL, 3, 12, 0x0000 },
};

static nthw_fpga_register_init_s km_registers[] = {
	{ KM_CAM_CTRL, 2, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, km_cam_ctrl_fields },
	{ KM_CAM_DATA, 3, 216, NTHW_FPGA_REG_TYPE_WO, 0, 12, km_cam_data_fields },
	{ KM_RCP_CTRL, 0, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, km_rcp_ctrl_fields },
	{ KM_RCP_DATA, 1, 781, NTHW_FPGA_REG_TYPE_WO, 0, 44, km_rcp_data_fields },
	{ KM_STATUS, 10, 1, NTHW_FPGA_REG_TYPE_RO, 0, 1, km_status_fields },
	{ KM_TCAM_CTRL, 4, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, km_tcam_ctrl_fields },
	{ KM_TCAM_DATA, 5, 72, NTHW_FPGA_REG_TYPE_WO, 0, 1, km_tcam_data_fields },
	{ KM_TCI_CTRL, 6, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, km_tci_ctrl_fields },
	{ KM_TCI_DATA, 7, 36, NTHW_FPGA_REG_TYPE_WO, 0, 2, km_tci_data_fields },
	{ KM_TCQ_CTRL, 8, 21, NTHW_FPGA_REG_TYPE_WO, 0, 2, km_tcq_ctrl_fields },
	{ KM_TCQ_DATA, 9, 15, NTHW_FPGA_REG_TYPE_WO, 0, 2, km_tcq_data_fields },
};

static nthw_fpga_field_init_s pci_rd_tg_tg_ctrl_fields[] = {
	{ PCI_RD_TG_TG_CTRL_TG_RD_RDY, 1, 0, 0 },
};

static nthw_fpga_field_init_s pci_rd_tg_tg_rdaddr_fields[] = {
	{ PCI_RD_TG_TG_RDADDR_RAM_ADDR, 9, 0, 0 },
};

static nthw_fpga_field_init_s pci_rd_tg_tg_rddata0_fields[] = {
	{ PCI_RD_TG_TG_RDDATA0_PHYS_ADDR_LOW, 32, 0, 0 },
};

static nthw_fpga_field_init_s pci_rd_tg_tg_rddata1_fields[] = {
	{ PCI_RD_TG_TG_RDDATA1_PHYS_ADDR_HIGH, 32, 0, 0 },
};

static nthw_fpga_field_init_s pci_rd_tg_tg_rddata2_fields[] = {
	{ PCI_RD_TG_TG_RDDATA2_REQ_HID, 6, 22, 0 },
	{ PCI_RD_TG_TG_RDDATA2_REQ_SIZE, 22, 0, 0 },
	{ PCI_RD_TG_TG_RDDATA2_WAIT, 1, 30, 0 },
	{ PCI_RD_TG_TG_RDDATA2_WRAP, 1, 31, 0 },
};

static nthw_fpga_field_init_s pci_rd_tg_tg_rd_run_fields[] = {
	{ PCI_RD_TG_TG_RD_RUN_RD_ITERATION, 16, 0, 0 },
};

static nthw_fpga_register_init_s pci_rd_tg_registers[] = {
	{ PCI_RD_TG_TG_CTRL, 5, 1, NTHW_FPGA_REG_TYPE_RO, 0, 1, pci_rd_tg_tg_ctrl_fields },
	{ PCI_RD_TG_TG_RDADDR, 3, 9, NTHW_FPGA_REG_TYPE_WO, 0, 1, pci_rd_tg_tg_rdaddr_fields },
	{ PCI_RD_TG_TG_RDDATA0, 0, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, pci_rd_tg_tg_rddata0_fields },
	{ PCI_RD_TG_TG_RDDATA1, 1, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, pci_rd_tg_tg_rddata1_fields },
	{ PCI_RD_TG_TG_RDDATA2, 2, 32, NTHW_FPGA_REG_TYPE_WO, 0, 4, pci_rd_tg_tg_rddata2_fields },
	{ PCI_RD_TG_TG_RD_RUN, 4, 16, NTHW_FPGA_REG_TYPE_WO, 0, 1, pci_rd_tg_tg_rd_run_fields },
};

static nthw_fpga_field_init_s pci_ta_control_fields[] = {
	{ PCI_TA_CONTROL_ENABLE, 1, 0, 0 },
};

static nthw_fpga_field_init_s pci_ta_length_error_fields[] = {
	{ PCI_TA_LENGTH_ERROR_AMOUNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s pci_ta_packet_bad_fields[] = {
	{ PCI_TA_PACKET_BAD_AMOUNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s pci_ta_packet_good_fields[] = {
	{ PCI_TA_PACKET_GOOD_AMOUNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s pci_ta_payload_error_fields[] = {
	{ PCI_TA_PAYLOAD_ERROR_AMOUNT, 32, 0, 0x0000 },
};

static nthw_fpga_register_init_s pci_ta_registers[] = {
	{ PCI_TA_CONTROL, 0, 1, NTHW_FPGA_REG_TYPE_WO, 0, 1, pci_ta_control_fields },
	{ PCI_TA_LENGTH_ERROR, 3, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, pci_ta_length_error_fields },
	{ PCI_TA_PACKET_BAD, 2, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, pci_ta_packet_bad_fields },
	{ PCI_TA_PACKET_GOOD, 1, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, pci_ta_packet_good_fields },
	{ PCI_TA_PAYLOAD_ERROR, 4, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, pci_ta_payload_error_fields },
};

static nthw_fpga_field_init_s pci_wr_tg_tg_ctrl_fields[] = {
	{ PCI_WR_TG_TG_CTRL_TG_WR_RDY, 1, 0, 0 },
};

static nthw_fpga_field_init_s pci_wr_tg_tg_seq_fields[] = {
	{ PCI_WR_TG_TG_SEQ_SEQUENCE, 16, 0, 0 },
};

static nthw_fpga_field_init_s pci_wr_tg_tg_wraddr_fields[] = {
	{ PCI_WR_TG_TG_WRADDR_RAM_ADDR, 9, 0, 0 },
};

static nthw_fpga_field_init_s pci_wr_tg_tg_wrdata0_fields[] = {
	{ PCI_WR_TG_TG_WRDATA0_PHYS_ADDR_LOW, 32, 0, 0 },
};

static nthw_fpga_field_init_s pci_wr_tg_tg_wrdata1_fields[] = {
	{ PCI_WR_TG_TG_WRDATA1_PHYS_ADDR_HIGH, 32, 0, 0 },
};

static nthw_fpga_field_init_s pci_wr_tg_tg_wrdata2_fields[] = {
	{ PCI_WR_TG_TG_WRDATA2_INC_MODE, 1, 29, 0 }, { PCI_WR_TG_TG_WRDATA2_REQ_HID, 6, 22, 0 },
	{ PCI_WR_TG_TG_WRDATA2_REQ_SIZE, 22, 0, 0 }, { PCI_WR_TG_TG_WRDATA2_WAIT, 1, 30, 0 },
	{ PCI_WR_TG_TG_WRDATA2_WRAP, 1, 31, 0 },
};

static nthw_fpga_field_init_s pci_wr_tg_tg_wr_run_fields[] = {
	{ PCI_WR_TG_TG_WR_RUN_WR_ITERATION, 16, 0, 0 },
};

static nthw_fpga_register_init_s pci_wr_tg_registers[] = {
	{ PCI_WR_TG_TG_CTRL, 5, 1, NTHW_FPGA_REG_TYPE_RO, 0, 1, pci_wr_tg_tg_ctrl_fields },
	{ PCI_WR_TG_TG_SEQ, 6, 16, NTHW_FPGA_REG_TYPE_RW, 0, 1, pci_wr_tg_tg_seq_fields },
	{ PCI_WR_TG_TG_WRADDR, 3, 9, NTHW_FPGA_REG_TYPE_WO, 0, 1, pci_wr_tg_tg_wraddr_fields },
	{ PCI_WR_TG_TG_WRDATA0, 0, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, pci_wr_tg_tg_wrdata0_fields },
	{ PCI_WR_TG_TG_WRDATA1, 1, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, pci_wr_tg_tg_wrdata1_fields },
	{ PCI_WR_TG_TG_WRDATA2, 2, 32, NTHW_FPGA_REG_TYPE_WO, 0, 5, pci_wr_tg_tg_wrdata2_fields },
	{ PCI_WR_TG_TG_WR_RUN, 4, 16, NTHW_FPGA_REG_TYPE_WO, 0, 1, pci_wr_tg_tg_wr_run_fields },
};

static nthw_fpga_field_init_s pcm_nt400dxx_ctrl_fields[] = {
	{ PCM_NT400DXX_CTRL_TS_PLL_RECAL, 1, 0, 0 },
};

static nthw_fpga_field_init_s pcm_nt400dxx_latch_fields[] = {
	{ PCM_NT400DXX_LATCH_TS_PLL_LOCKED, 1, 0, 0x0000 },
};

static nthw_fpga_field_init_s pcm_nt400dxx_stat_fields[] = {
	{ PCM_NT400DXX_STAT_TS_PLL_LOCKED, 1, 0, 0x0000 },
};

static nthw_fpga_register_init_s pcm_nt400dxx_registers[] = {
	{ PCM_NT400DXX_CTRL, 0, 1, NTHW_FPGA_REG_TYPE_RW, 0, 1, pcm_nt400dxx_ctrl_fields },
	{ PCM_NT400DXX_LATCH, 2, 1, NTHW_FPGA_REG_TYPE_RW, 0, 1, pcm_nt400dxx_latch_fields },
	{ PCM_NT400DXX_STAT, 1, 1, NTHW_FPGA_REG_TYPE_RO, 0, 1, pcm_nt400dxx_stat_fields },
};

static nthw_fpga_field_init_s pdb_config_fields[] = {
	{ PDB_CONFIG_PORT_OFS, 6, 3, 0 },
	{ PDB_CONFIG_TS_FORMAT, 3, 0, 0 },
};

static nthw_fpga_field_init_s pdb_rcp_ctrl_fields[] = {
	{ PDB_RCP_CTRL_ADR, 4, 0, 0x0000 },
	{ PDB_RCP_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s pdb_rcp_data_fields[] = {
	{ PDB_RCP_DATA_ALIGN, 1, 17, 0x0000 },
	{ PDB_RCP_DATA_CRC_OVERWRITE, 1, 16, 0x0000 },
	{ PDB_RCP_DATA_DESCRIPTOR, 4, 0, 0x0000 },
	{ PDB_RCP_DATA_DESC_LEN, 5, 4, 0 },
	{ PDB_RCP_DATA_DUPLICATE_BIT, 5, 61, 0x0000 },
	{ PDB_RCP_DATA_DUPLICATE_EN, 1, 60, 0x0000 },
	{ PDB_RCP_DATA_IP_PROT_TNL, 1, 57, 0x0000 },
	{ PDB_RCP_DATA_OFS0_DYN, 5, 18, 0x0000 },
	{ PDB_RCP_DATA_OFS0_REL, 8, 23, 0x0000 },
	{ PDB_RCP_DATA_OFS1_DYN, 5, 31, 0x0000 },
	{ PDB_RCP_DATA_OFS1_REL, 8, 36, 0x0000 },
	{ PDB_RCP_DATA_OFS2_DYN, 5, 44, 0x0000 },
	{ PDB_RCP_DATA_OFS2_REL, 8, 49, 0x0000 },
	{ PDB_RCP_DATA_PCAP_KEEP_FCS, 1, 66, 0x0000 },
	{ PDB_RCP_DATA_PPC_HSH, 2, 58, 0x0000 },
	{ PDB_RCP_DATA_TX_IGNORE, 1, 14, 0x0000 },
	{ PDB_RCP_DATA_TX_NOW, 1, 15, 0x0000 },
	{ PDB_RCP_DATA_TX_PORT, 5, 9, 0x0000 },
};

static nthw_fpga_register_init_s pdb_registers[] = {
	{ PDB_CONFIG, 2, 10, NTHW_FPGA_REG_TYPE_WO, 0, 2, pdb_config_fields },
	{ PDB_RCP_CTRL, 0, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, pdb_rcp_ctrl_fields },
	{ PDB_RCP_DATA, 1, 67, NTHW_FPGA_REG_TYPE_WO, 0, 18, pdb_rcp_data_fields },
};

static nthw_fpga_field_init_s pdi_cr_fields[] = {
	{ PDI_CR_EN, 1, 0, 0 }, { PDI_CR_PARITY, 1, 4, 0 }, { PDI_CR_RST, 1, 1, 0 },
	{ PDI_CR_RXRST, 1, 2, 0 }, { PDI_CR_STOP, 1, 5, 0 }, { PDI_CR_TXRST, 1, 3, 0 },
};

static nthw_fpga_field_init_s pdi_drr_fields[] = {
	{ PDI_DRR_DRR, 8, 0, 0 },
};

static nthw_fpga_field_init_s pdi_dtr_fields[] = {
	{ PDI_DTR_DTR, 8, 0, 0 },
};

static nthw_fpga_field_init_s pdi_pre_fields[] = {
	{ PDI_PRE_PRE, 7, 0, 3 },
};

static nthw_fpga_field_init_s pdi_sr_fields[] = {
	{ PDI_SR_DISABLE_BUSY, 1, 2, 0 }, { PDI_SR_DONE, 1, 0, 0 },
	{ PDI_SR_ENABLE_BUSY, 1, 1, 0 }, { PDI_SR_FRAME_ERR, 1, 5, 0 },
	{ PDI_SR_OVERRUN_ERR, 1, 7, 0 }, { PDI_SR_PARITY_ERR, 1, 6, 0 },
	{ PDI_SR_RXLVL, 7, 8, 0 }, { PDI_SR_RX_BUSY, 1, 4, 0 },
	{ PDI_SR_TXLVL, 7, 15, 0 }, { PDI_SR_TX_BUSY, 1, 3, 0 },
};

static nthw_fpga_field_init_s pdi_srr_fields[] = {
	{ PDI_SRR_RST, 4, 0, 0 },
};

static nthw_fpga_register_init_s pdi_registers[] = {
	{ PDI_CR, 1, 6, NTHW_FPGA_REG_TYPE_WO, 0, 6, pdi_cr_fields },
	{ PDI_DRR, 4, 8, NTHW_FPGA_REG_TYPE_RO, 0, 1, pdi_drr_fields },
	{ PDI_DTR, 3, 8, NTHW_FPGA_REG_TYPE_WO, 0, 1, pdi_dtr_fields },
	{ PDI_PRE, 5, 7, NTHW_FPGA_REG_TYPE_WO, 3, 1, pdi_pre_fields },
	{ PDI_SR, 2, 22, NTHW_FPGA_REG_TYPE_RO, 0, 10, pdi_sr_fields },
	{ PDI_SRR, 0, 4, NTHW_FPGA_REG_TYPE_WO, 0, 1, pdi_srr_fields },
};

static nthw_fpga_field_init_s phy_tile_dr_cfg_fields[] = {
	{ PHY_TILE_DR_CFG_DYN_RST, 1, 0, 0 },
	{ PHY_TILE_DR_CFG_FEATURES, 7, 9, 0x0000 },
	{ PHY_TILE_DR_CFG_SCRATCH, 8, 1, 0x0000 },
	{ PHY_TILE_DR_CFG_TX_FLUSH_LEVEL, 6, 16, 23 },
};

static nthw_fpga_field_init_s phy_tile_dr_cfg_status_fields[] = {
	{ PHY_TILE_DR_CFG_STATUS_CURR_PROFILE_ID, 15, 0, 0x0000 },
	{ PHY_TILE_DR_CFG_STATUS_ERROR, 1, 16, 0x0000 },
	{ PHY_TILE_DR_CFG_STATUS_IN_PROGRESS, 1, 15, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_dyn_reconfig_base_fields[] = {
	{ PHY_TILE_DYN_RECONFIG_BASE_BUSY, 1, 30, 0x0000 },
	{ PHY_TILE_DYN_RECONFIG_BASE_CMD, 1, 31, 0x0000 },
	{ PHY_TILE_DYN_RECONFIG_BASE_PTR, 30, 0, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_dyn_reconfig_data_fields[] = {
	{ PHY_TILE_DYN_RECONFIG_DATA_DATA, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_link_summary_0_fields[] = {
	{ PHY_TILE_LINK_SUMMARY_0_LH_RECEIVED_LOCAL_FAULT, 1, 14, 0 },
	{ PHY_TILE_LINK_SUMMARY_0_LH_REMOTE_FAULT, 1, 15, 0 },
	{ PHY_TILE_LINK_SUMMARY_0_LH_RX_HIGH_BIT_ERROR_RATE, 1, 13, 0 },
	{ PHY_TILE_LINK_SUMMARY_0_LINK_DOWN_CNT, 8, 2, 0 },
	{ PHY_TILE_LINK_SUMMARY_0_LL_PHY_LINK_STATE, 1, 1, 1 },
	{ PHY_TILE_LINK_SUMMARY_0_LL_RX_AM_LOCK, 1, 11, 1 },
	{ PHY_TILE_LINK_SUMMARY_0_LL_RX_BLOCK_LOCK, 1, 10, 1 },
	{ PHY_TILE_LINK_SUMMARY_0_NT_PHY_LINK_STATE, 1, 0, 0 },
};

static nthw_fpga_field_init_s phy_tile_link_summary_1_fields[] = {
	{ PHY_TILE_LINK_SUMMARY_1_LH_RECEIVED_LOCAL_FAULT, 1, 14, 0 },
	{ PHY_TILE_LINK_SUMMARY_1_LH_REMOTE_FAULT, 1, 15, 0 },
	{ PHY_TILE_LINK_SUMMARY_1_LH_RX_HIGH_BIT_ERROR_RATE, 1, 13, 0 },
	{ PHY_TILE_LINK_SUMMARY_1_LINK_DOWN_CNT, 8, 2, 0 },
	{ PHY_TILE_LINK_SUMMARY_1_LL_PHY_LINK_STATE, 1, 1, 1 },
	{ PHY_TILE_LINK_SUMMARY_1_LL_RX_AM_LOCK, 1, 11, 1 },
	{ PHY_TILE_LINK_SUMMARY_1_LL_RX_BLOCK_LOCK, 1, 10, 1 },
	{ PHY_TILE_LINK_SUMMARY_1_NT_PHY_LINK_STATE, 1, 0, 0 },
};

static nthw_fpga_field_init_s phy_tile_port_0_eth_0_base_fields[] = {
	{ PHY_TILE_PORT_0_ETH_0_BASE_BUSY, 1, 30, 0x0000 },
	{ PHY_TILE_PORT_0_ETH_0_BASE_CMD, 1, 31, 0x0000 },
	{ PHY_TILE_PORT_0_ETH_0_BASE_PTR, 30, 0, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_port_0_eth_0_data_fields[] = {
	{ PHY_TILE_PORT_0_ETH_0_DATA_DATA, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_port_0_xcvr_0_base_fields[] = {
	{ PHY_TILE_PORT_0_XCVR_0_BASE_BUSY, 1, 30, 0x0000 },
	{ PHY_TILE_PORT_0_XCVR_0_BASE_CMD, 1, 31, 0x0000 },
	{ PHY_TILE_PORT_0_XCVR_0_BASE_PTR, 30, 0, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_port_0_xcvr_0_data_fields[] = {
	{ PHY_TILE_PORT_0_XCVR_0_DATA_DATA, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_port_0_xcvr_1_base_fields[] = {
	{ PHY_TILE_PORT_0_XCVR_1_BASE_BUSY, 1, 30, 0x0000 },
	{ PHY_TILE_PORT_0_XCVR_1_BASE_CMD, 1, 31, 0x0000 },
	{ PHY_TILE_PORT_0_XCVR_1_BASE_PTR, 30, 0, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_port_0_xcvr_1_data_fields[] = {
	{ PHY_TILE_PORT_0_XCVR_1_DATA_DATA, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_port_0_xcvr_2_base_fields[] = {
	{ PHY_TILE_PORT_0_XCVR_2_BASE_BUSY, 1, 30, 0x0000 },
	{ PHY_TILE_PORT_0_XCVR_2_BASE_CMD, 1, 31, 0x0000 },
	{ PHY_TILE_PORT_0_XCVR_2_BASE_PTR, 30, 0, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_port_0_xcvr_2_data_fields[] = {
	{ PHY_TILE_PORT_0_XCVR_2_DATA_DATA, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_port_0_xcvr_3_base_fields[] = {
	{ PHY_TILE_PORT_0_XCVR_3_BASE_BUSY, 1, 30, 0x0000 },
	{ PHY_TILE_PORT_0_XCVR_3_BASE_CMD, 1, 31, 0x0000 },
	{ PHY_TILE_PORT_0_XCVR_3_BASE_PTR, 30, 0, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_port_0_xcvr_3_data_fields[] = {
	{ PHY_TILE_PORT_0_XCVR_3_DATA_DATA, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_port_1_eth_0_base_fields[] = {
	{ PHY_TILE_PORT_1_ETH_0_BASE_BUSY, 1, 30, 0x0000 },
	{ PHY_TILE_PORT_1_ETH_0_BASE_CMD, 1, 31, 0x0000 },
	{ PHY_TILE_PORT_1_ETH_0_BASE_PTR, 30, 0, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_port_1_eth_0_data_fields[] = {
	{ PHY_TILE_PORT_1_ETH_0_DATA_DATA, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_port_1_xcvr_0_base_fields[] = {
	{ PHY_TILE_PORT_1_XCVR_0_BASE_BUSY, 1, 30, 0x0000 },
	{ PHY_TILE_PORT_1_XCVR_0_BASE_CMD, 1, 31, 0x0000 },
	{ PHY_TILE_PORT_1_XCVR_0_BASE_PTR, 30, 0, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_port_1_xcvr_0_data_fields[] = {
	{ PHY_TILE_PORT_1_XCVR_0_DATA_DATA, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_port_1_xcvr_1_base_fields[] = {
	{ PHY_TILE_PORT_1_XCVR_1_BASE_BUSY, 1, 30, 0x0000 },
	{ PHY_TILE_PORT_1_XCVR_1_BASE_CMD, 1, 31, 0x0000 },
	{ PHY_TILE_PORT_1_XCVR_1_BASE_PTR, 30, 0, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_port_1_xcvr_1_data_fields[] = {
	{ PHY_TILE_PORT_1_XCVR_1_DATA_DATA, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_port_1_xcvr_2_base_fields[] = {
	{ PHY_TILE_PORT_1_XCVR_2_BASE_BUSY, 1, 30, 0x0000 },
	{ PHY_TILE_PORT_1_XCVR_2_BASE_CMD, 1, 31, 0x0000 },
	{ PHY_TILE_PORT_1_XCVR_2_BASE_PTR, 30, 0, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_port_1_xcvr_2_data_fields[] = {
	{ PHY_TILE_PORT_1_XCVR_2_DATA_DATA, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_port_1_xcvr_3_base_fields[] = {
	{ PHY_TILE_PORT_1_XCVR_3_BASE_BUSY, 1, 30, 0x0000 },
	{ PHY_TILE_PORT_1_XCVR_3_BASE_CMD, 1, 31, 0x0000 },
	{ PHY_TILE_PORT_1_XCVR_3_BASE_PTR, 30, 0, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_port_1_xcvr_3_data_fields[] = {
	{ PHY_TILE_PORT_1_XCVR_3_DATA_DATA, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_port_comp_0_fields[] = {
	{ PHY_TILE_PORT_COMP_0_RX_COMPENSATION, 16, 0, 0 },
	{ PHY_TILE_PORT_COMP_0_TX_COMPENSATION, 16, 16, 0 },
};

static nthw_fpga_field_init_s phy_tile_port_comp_1_fields[] = {
	{ PHY_TILE_PORT_COMP_1_RX_COMPENSATION, 16, 0, 0 },
	{ PHY_TILE_PORT_COMP_1_TX_COMPENSATION, 16, 16, 0 },
};

static nthw_fpga_field_init_s phy_tile_port_config_0_fields[] = {
	{ PHY_TILE_PORT_CONFIG_0_NT_AUTO_FORCE_LINK_DOWN, 1, 12, 0 },
	{ PHY_TILE_PORT_CONFIG_0_NT_FORCE_LINK_DOWN, 1, 11, 0 },
	{ PHY_TILE_PORT_CONFIG_0_NT_LINKUP_LATENCY, 8, 3, 10 },
	{ PHY_TILE_PORT_CONFIG_0_RST, 1, 0, 1 },
	{ PHY_TILE_PORT_CONFIG_0_RX_RST, 1, 1, 1 },
	{ PHY_TILE_PORT_CONFIG_0_TX_RST, 1, 2, 1 },
};

static nthw_fpga_field_init_s phy_tile_port_config_1_fields[] = {
	{ PHY_TILE_PORT_CONFIG_1_NT_AUTO_FORCE_LINK_DOWN, 1, 12, 0 },
	{ PHY_TILE_PORT_CONFIG_1_NT_FORCE_LINK_DOWN, 1, 11, 0 },
	{ PHY_TILE_PORT_CONFIG_1_NT_LINKUP_LATENCY, 8, 3, 10 },
	{ PHY_TILE_PORT_CONFIG_1_RST, 1, 0, 1 },
	{ PHY_TILE_PORT_CONFIG_1_RX_RST, 1, 1, 1 },
	{ PHY_TILE_PORT_CONFIG_1_TX_RST, 1, 2, 1 },
};

static nthw_fpga_field_init_s phy_tile_port_status_0_fields[] = {
	{ PHY_TILE_PORT_STATUS_0_RESET_ACK_N, 1, 0, 0x0000 },
	{ PHY_TILE_PORT_STATUS_0_RX_AM_LOCK, 1, 10, 0x0000 },
	{ PHY_TILE_PORT_STATUS_0_RX_HI_BER, 1, 7, 0x0000 },
	{ PHY_TILE_PORT_STATUS_0_RX_LOCAL_FAULT, 1, 9, 0x0000 },
	{ PHY_TILE_PORT_STATUS_0_RX_PCS_FULLY_ALIGNED, 1, 6, 0x0000 },
	{ PHY_TILE_PORT_STATUS_0_RX_REMOTE_FAULT, 1, 8, 0x0000 },
	{ PHY_TILE_PORT_STATUS_0_RX_RESET_ACK_N, 1, 2, 0x0000 },
	{ PHY_TILE_PORT_STATUS_0_SYS_PLL_LOCKED, 1, 5, 0x0000 },
	{ PHY_TILE_PORT_STATUS_0_TX_LANES_STABLE, 1, 3, 0x0000 },
	{ PHY_TILE_PORT_STATUS_0_TX_PLL_LOCKED, 1, 4, 0x0000 },
	{ PHY_TILE_PORT_STATUS_0_TX_RESET_ACK_N, 1, 1, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_port_status_1_fields[] = {
	{ PHY_TILE_PORT_STATUS_1_RESET_ACK_N, 1, 0, 0x0000 },
	{ PHY_TILE_PORT_STATUS_1_RX_AM_LOCK, 1, 10, 0x0000 },
	{ PHY_TILE_PORT_STATUS_1_RX_HI_BER, 1, 7, 0x0000 },
	{ PHY_TILE_PORT_STATUS_1_RX_LOCAL_FAULT, 1, 9, 0x0000 },
	{ PHY_TILE_PORT_STATUS_1_RX_PCS_FULLY_ALIGNED, 1, 6, 0x0000 },
	{ PHY_TILE_PORT_STATUS_1_RX_REMOTE_FAULT, 1, 8, 0x0000 },
	{ PHY_TILE_PORT_STATUS_1_RX_RESET_ACK_N, 1, 2, 0x0000 },
	{ PHY_TILE_PORT_STATUS_1_SYS_PLL_LOCKED, 1, 5, 0x0000 },
	{ PHY_TILE_PORT_STATUS_1_TX_LANES_STABLE, 1, 3, 0x0000 },
	{ PHY_TILE_PORT_STATUS_1_TX_PLL_LOCKED, 1, 4, 0x0000 },
	{ PHY_TILE_PORT_STATUS_1_TX_RESET_ACK_N, 1, 1, 0x0000 },
};

static nthw_fpga_field_init_s phy_tile_sys_pll_fields[] = {
	{ PHY_TILE_SYS_PLL_DISABLE_REFCLK_MONITOR, 8, 13, 0 },
	{ PHY_TILE_SYS_PLL_EN_REFCLK_FGT, 8, 5, 0 },
	{ PHY_TILE_SYS_PLL_FORCE_RST, 1, 29, 0 },
	{ PHY_TILE_SYS_PLL_FORWARD_RST, 1, 30, 1 },
	{ PHY_TILE_SYS_PLL_GET_RDY, 1, 3, 0x0000 },
	{ PHY_TILE_SYS_PLL_REFCLK_FGT_ENABLED, 8, 21, 0 },
	{ PHY_TILE_SYS_PLL_SET_RDY, 3, 0, 0 },
	{ PHY_TILE_SYS_PLL_SYSTEMPLL_LOCK, 1, 4, 0x0000 },
};

static nthw_fpga_register_init_s phy_tile_registers[] = {
	{ PHY_TILE_DR_CFG, 32, 22, NTHW_FPGA_REG_TYPE_MIXED, 1507328, 4, phy_tile_dr_cfg_fields },
	{
		PHY_TILE_DR_CFG_STATUS, 31, 19, NTHW_FPGA_REG_TYPE_RO, 0, 3,
		phy_tile_dr_cfg_status_fields
	},
	{
		PHY_TILE_DYN_RECONFIG_BASE, 20, 32, NTHW_FPGA_REG_TYPE_MIXED, 0, 3,
		phy_tile_dyn_reconfig_base_fields
	},
	{
		PHY_TILE_DYN_RECONFIG_DATA, 21, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1,
		phy_tile_dyn_reconfig_data_fields
	},
	{
		PHY_TILE_LINK_SUMMARY_0, 27, 16, NTHW_FPGA_REG_TYPE_RO, 7170, 8,
		phy_tile_link_summary_0_fields
	},
	{
		PHY_TILE_LINK_SUMMARY_1, 28, 16, NTHW_FPGA_REG_TYPE_RO, 7170, 8,
		phy_tile_link_summary_1_fields
	},
	{
		PHY_TILE_PORT_0_ETH_0_BASE, 8, 32, NTHW_FPGA_REG_TYPE_MIXED, 0, 3,
		phy_tile_port_0_eth_0_base_fields
	},
	{
		PHY_TILE_PORT_0_ETH_0_DATA, 9, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1,
		phy_tile_port_0_eth_0_data_fields
	},
	{
		PHY_TILE_PORT_0_XCVR_0_BASE, 0, 32, NTHW_FPGA_REG_TYPE_MIXED, 0, 3,
		phy_tile_port_0_xcvr_0_base_fields
	},
	{
		PHY_TILE_PORT_0_XCVR_0_DATA, 1, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1,
		phy_tile_port_0_xcvr_0_data_fields
	},
	{
		PHY_TILE_PORT_0_XCVR_1_BASE, 2, 32, NTHW_FPGA_REG_TYPE_MIXED, 0, 3,
		phy_tile_port_0_xcvr_1_base_fields
	},
	{
		PHY_TILE_PORT_0_XCVR_1_DATA, 3, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1,
		phy_tile_port_0_xcvr_1_data_fields
	},
	{
		PHY_TILE_PORT_0_XCVR_2_BASE, 4, 32, NTHW_FPGA_REG_TYPE_MIXED, 0, 3,
		phy_tile_port_0_xcvr_2_base_fields
	},
	{
		PHY_TILE_PORT_0_XCVR_2_DATA, 5, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1,
		phy_tile_port_0_xcvr_2_data_fields
	},
	{
		PHY_TILE_PORT_0_XCVR_3_BASE, 6, 32, NTHW_FPGA_REG_TYPE_MIXED, 0, 3,
		phy_tile_port_0_xcvr_3_base_fields
	},
	{
		PHY_TILE_PORT_0_XCVR_3_DATA, 7, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1,
		phy_tile_port_0_xcvr_3_data_fields
	},
	{
		PHY_TILE_PORT_1_ETH_0_BASE, 18, 32, NTHW_FPGA_REG_TYPE_MIXED, 0, 3,
		phy_tile_port_1_eth_0_base_fields
	},
	{
		PHY_TILE_PORT_1_ETH_0_DATA, 19, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1,
		phy_tile_port_1_eth_0_data_fields
	},
	{
		PHY_TILE_PORT_1_XCVR_0_BASE, 10, 32, NTHW_FPGA_REG_TYPE_MIXED, 0, 3,
		phy_tile_port_1_xcvr_0_base_fields
	},
	{
		PHY_TILE_PORT_1_XCVR_0_DATA, 11, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1,
		phy_tile_port_1_xcvr_0_data_fields
	},
	{
		PHY_TILE_PORT_1_XCVR_1_BASE, 12, 32, NTHW_FPGA_REG_TYPE_MIXED, 0, 3,
		phy_tile_port_1_xcvr_1_base_fields
	},
	{
		PHY_TILE_PORT_1_XCVR_1_DATA, 13, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1,
		phy_tile_port_1_xcvr_1_data_fields
	},
	{
		PHY_TILE_PORT_1_XCVR_2_BASE, 14, 32, NTHW_FPGA_REG_TYPE_MIXED, 0, 3,
		phy_tile_port_1_xcvr_2_base_fields
	},
	{
		PHY_TILE_PORT_1_XCVR_2_DATA, 15, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1,
		phy_tile_port_1_xcvr_2_data_fields
	},
	{
		PHY_TILE_PORT_1_XCVR_3_BASE, 16, 32, NTHW_FPGA_REG_TYPE_MIXED, 0, 3,
		phy_tile_port_1_xcvr_3_base_fields
	},
	{
		PHY_TILE_PORT_1_XCVR_3_DATA, 17, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1,
		phy_tile_port_1_xcvr_3_data_fields
	},
	{ PHY_TILE_PORT_COMP_0, 35, 32, NTHW_FPGA_REG_TYPE_RW, 0, 2, phy_tile_port_comp_0_fields },
	{ PHY_TILE_PORT_COMP_1, 36, 32, NTHW_FPGA_REG_TYPE_RW, 0, 2, phy_tile_port_comp_1_fields },
	{
		PHY_TILE_PORT_CONFIG_0, 33, 13, NTHW_FPGA_REG_TYPE_RW, 87, 6,
		phy_tile_port_config_0_fields
	},
	{
		PHY_TILE_PORT_CONFIG_1, 34, 13, NTHW_FPGA_REG_TYPE_RW, 87, 6,
		phy_tile_port_config_1_fields
	},
	{
		PHY_TILE_PORT_STATUS_0, 29, 16, NTHW_FPGA_REG_TYPE_RO, 0, 11,
		phy_tile_port_status_0_fields
	},
	{
		PHY_TILE_PORT_STATUS_1, 30, 16, NTHW_FPGA_REG_TYPE_RO, 0, 11,
		phy_tile_port_status_1_fields
	},
	{
		PHY_TILE_SYS_PLL, 26, 31, NTHW_FPGA_REG_TYPE_MIXED, 1073741824, 8,
		phy_tile_sys_pll_fields
	},
};

static nthw_fpga_field_init_s prm_nt400dxx_rst_fields[] = {
	{ PRM_NT400DXX_RST_PERIPH, 1, 0, 0 },
	{ PRM_NT400DXX_RST_PLATFORM, 1, 1, 1 },
};

static nthw_fpga_register_init_s prm_nt400dxx_registers[] = {
	{ PRM_NT400DXX_RST, 0, 2, NTHW_FPGA_REG_TYPE_RW, 2, 2, prm_nt400dxx_rst_fields },
};

static nthw_fpga_field_init_s qsl_qen_ctrl_fields[] = {
	{ QSL_QEN_CTRL_ADR, 5, 0, 0x0000 },
	{ QSL_QEN_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s qsl_qen_data_fields[] = {
	{ QSL_QEN_DATA_EN, 4, 0, 0x0000 },
};

static nthw_fpga_field_init_s qsl_qst_ctrl_fields[] = {
	{ QSL_QST_CTRL_ADR, 12, 0, 0x0000 },
	{ QSL_QST_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s qsl_qst_data_fields[] = {
	{ QSL_QST_DATA_LRE, 1, 9, 0x0000 }, { QSL_QST_DATA_QEN, 1, 7, 0x0000 },
	{ QSL_QST_DATA_QUEUE, 7, 0, 0x0000 }, { QSL_QST_DATA_TCI, 16, 10, 0x0000 },
	{ QSL_QST_DATA_TX_PORT, 1, 8, 0x0000 }, { QSL_QST_DATA_VEN, 1, 26, 0x0000 },
};

static nthw_fpga_field_init_s qsl_rcp_ctrl_fields[] = {
	{ QSL_RCP_CTRL_ADR, 5, 0, 0x0000 },
	{ QSL_RCP_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s qsl_rcp_data_fields[] = {
	{ QSL_RCP_DATA_DISCARD, 1, 0, 0x0000 }, { QSL_RCP_DATA_DROP, 2, 1, 0x0000 },
	{ QSL_RCP_DATA_LR, 2, 51, 0x0000 }, { QSL_RCP_DATA_TBL_HI, 12, 15, 0x0000 },
	{ QSL_RCP_DATA_TBL_IDX, 12, 27, 0x0000 }, { QSL_RCP_DATA_TBL_LO, 12, 3, 0x0000 },
	{ QSL_RCP_DATA_TBL_MSK, 12, 39, 0x0000 }, { QSL_RCP_DATA_TSA, 1, 53, 0x0000 },
	{ QSL_RCP_DATA_VLI, 2, 54, 0x0000 },
};

static nthw_fpga_field_init_s qsl_unmq_ctrl_fields[] = {
	{ QSL_UNMQ_CTRL_ADR, 1, 0, 0x0000 },
	{ QSL_UNMQ_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s qsl_unmq_data_fields[] = {
	{ QSL_UNMQ_DATA_DEST_QUEUE, 7, 0, 0x0000 },
	{ QSL_UNMQ_DATA_EN, 1, 7, 0x0000 },
};

static nthw_fpga_register_init_s qsl_registers[] = {
	{ QSL_QEN_CTRL, 4, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, qsl_qen_ctrl_fields },
	{ QSL_QEN_DATA, 5, 4, NTHW_FPGA_REG_TYPE_WO, 0, 1, qsl_qen_data_fields },
	{ QSL_QST_CTRL, 2, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, qsl_qst_ctrl_fields },
	{ QSL_QST_DATA, 3, 27, NTHW_FPGA_REG_TYPE_WO, 0, 6, qsl_qst_data_fields },
	{ QSL_RCP_CTRL, 0, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, qsl_rcp_ctrl_fields },
	{ QSL_RCP_DATA, 1, 56, NTHW_FPGA_REG_TYPE_WO, 0, 9, qsl_rcp_data_fields },
	{ QSL_UNMQ_CTRL, 6, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, qsl_unmq_ctrl_fields },
	{ QSL_UNMQ_DATA, 7, 8, NTHW_FPGA_REG_TYPE_WO, 0, 2, qsl_unmq_data_fields },
};

static nthw_fpga_field_init_s rac_dbg_ctrl_fields[] = {
	{ RAC_DBG_CTRL_C, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s rac_dbg_data_fields[] = {
	{ RAC_DBG_DATA_D, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s rac_rab_buf_free_fields[] = {
	{ RAC_RAB_BUF_FREE_IB_FREE, 9, 0, 511 }, { RAC_RAB_BUF_FREE_IB_OVF, 1, 12, 0 },
	{ RAC_RAB_BUF_FREE_OB_FREE, 9, 16, 511 }, { RAC_RAB_BUF_FREE_OB_OVF, 1, 28, 0 },
	{ RAC_RAB_BUF_FREE_TIMEOUT, 1, 31, 0 },
};

static nthw_fpga_field_init_s rac_rab_buf_used_fields[] = {
	{ RAC_RAB_BUF_USED_FLUSH, 1, 31, 0 },
	{ RAC_RAB_BUF_USED_IB_USED, 9, 0, 0 },
	{ RAC_RAB_BUF_USED_OB_USED, 9, 16, 0 },
};

static nthw_fpga_field_init_s rac_rab_dma_ib_hi_fields[] = {
	{ RAC_RAB_DMA_IB_HI_PHYADDR, 32, 0, 0 },
};

static nthw_fpga_field_init_s rac_rab_dma_ib_lo_fields[] = {
	{ RAC_RAB_DMA_IB_LO_PHYADDR, 32, 0, 0 },
};

static nthw_fpga_field_init_s rac_rab_dma_ib_rd_fields[] = {
	{ RAC_RAB_DMA_IB_RD_PTR, 16, 0, 0 },
};

static nthw_fpga_field_init_s rac_rab_dma_ib_wr_fields[] = {
	{ RAC_RAB_DMA_IB_WR_PTR, 16, 0, 0 },
};

static nthw_fpga_field_init_s rac_rab_dma_ob_hi_fields[] = {
	{ RAC_RAB_DMA_OB_HI_PHYADDR, 32, 0, 0 },
};

static nthw_fpga_field_init_s rac_rab_dma_ob_lo_fields[] = {
	{ RAC_RAB_DMA_OB_LO_PHYADDR, 32, 0, 0 },
};

static nthw_fpga_field_init_s rac_rab_dma_ob_wr_fields[] = {
	{ RAC_RAB_DMA_OB_WR_PTR, 16, 0, 0 },
};

static nthw_fpga_field_init_s rac_rab_ib_data_fields[] = {
	{ RAC_RAB_IB_DATA_D, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s rac_rab_init_fields[] = {
	{ RAC_RAB_INIT_RAB, 3, 0, 7 },
};

static nthw_fpga_field_init_s rac_rab_ob_data_fields[] = {
	{ RAC_RAB_OB_DATA_D, 32, 0, 0x0000 },
};

static nthw_fpga_register_init_s rac_registers[] = {
	{ RAC_DBG_CTRL, 4200, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1, rac_dbg_ctrl_fields },
	{ RAC_DBG_DATA, 4208, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1, rac_dbg_data_fields },
	{
		RAC_RAB_BUF_FREE, 4176, 32, NTHW_FPGA_REG_TYPE_MIXED, 33489407, 5,
		rac_rab_buf_free_fields
	},
	{ RAC_RAB_BUF_USED, 4184, 32, NTHW_FPGA_REG_TYPE_MIXED, 0, 3, rac_rab_buf_used_fields },
	{ RAC_RAB_DMA_IB_HI, 4360, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, rac_rab_dma_ib_hi_fields },
	{ RAC_RAB_DMA_IB_LO, 4352, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, rac_rab_dma_ib_lo_fields },
	{ RAC_RAB_DMA_IB_RD, 4424, 16, NTHW_FPGA_REG_TYPE_RO, 0, 1, rac_rab_dma_ib_rd_fields },
	{ RAC_RAB_DMA_IB_WR, 4416, 16, NTHW_FPGA_REG_TYPE_WO, 0, 1, rac_rab_dma_ib_wr_fields },
	{ RAC_RAB_DMA_OB_HI, 4376, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, rac_rab_dma_ob_hi_fields },
	{ RAC_RAB_DMA_OB_LO, 4368, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, rac_rab_dma_ob_lo_fields },
	{ RAC_RAB_DMA_OB_WR, 4480, 16, NTHW_FPGA_REG_TYPE_RO, 0, 1, rac_rab_dma_ob_wr_fields },
	{ RAC_RAB_IB_DATA, 4160, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, rac_rab_ib_data_fields },
	{ RAC_RAB_INIT, 4192, 3, NTHW_FPGA_REG_TYPE_RW, 7, 1, rac_rab_init_fields },
	{ RAC_RAB_OB_DATA, 4168, 32, NTHW_FPGA_REG_TYPE_RC1, 0, 1, rac_rab_ob_data_fields },
};

static nthw_fpga_field_init_s rfd_ctrl_fields[] = {
	{ RFD_CTRL_CFP, 1, 2, 1 },
	{ RFD_CTRL_ISL, 1, 0, 1 },
	{ RFD_CTRL_PWMCW, 1, 1, 1 },
};

static nthw_fpga_field_init_s rfd_max_frame_size_fields[] = {
	{ RFD_MAX_FRAME_SIZE_MAX, 14, 0, 9018 },
};

static nthw_fpga_field_init_s rfd_tnl_vlan_fields[] = {
	{ RFD_TNL_VLAN_TPID0, 16, 0, 33024 },
	{ RFD_TNL_VLAN_TPID1, 16, 16, 33024 },
};

static nthw_fpga_field_init_s rfd_vlan_fields[] = {
	{ RFD_VLAN_TPID0, 16, 0, 33024 },
	{ RFD_VLAN_TPID1, 16, 16, 33024 },
};

static nthw_fpga_field_init_s rfd_vxlan_fields[] = {
	{ RFD_VXLAN_DP0, 16, 0, 4789 },
	{ RFD_VXLAN_DP1, 16, 16, 4789 },
};

static nthw_fpga_register_init_s rfd_registers[] = {
	{ RFD_CTRL, 0, 3, NTHW_FPGA_REG_TYPE_WO, 7, 3, rfd_ctrl_fields },
	{ RFD_MAX_FRAME_SIZE, 1, 14, NTHW_FPGA_REG_TYPE_WO, 9018, 1, rfd_max_frame_size_fields },
	{ RFD_TNL_VLAN, 3, 32, NTHW_FPGA_REG_TYPE_WO, 2164293888, 2, rfd_tnl_vlan_fields },
	{ RFD_VLAN, 2, 32, NTHW_FPGA_REG_TYPE_WO, 2164293888, 2, rfd_vlan_fields },
	{ RFD_VXLAN, 4, 32, NTHW_FPGA_REG_TYPE_WO, 313856693, 2, rfd_vxlan_fields },
};

static nthw_fpga_field_init_s rpf_control_fields[] = {
	{ RPF_CONTROL_KEEP_ALIVE_EN, 1, 7, 0 },
	{ RPF_CONTROL_PEN, 2, 0, 0 },
	{ RPF_CONTROL_RPP_EN, 4, 2, 0 },
	{ RPF_CONTROL_ST_TGL_EN, 1, 6, 0 },
};

static nthw_fpga_field_init_s rpf_ts_sort_prg_fields[] = {
	{ RPF_TS_SORT_PRG_MATURING_DELAY, 19, 0, 524163 },
	{ RPF_TS_SORT_PRG_TS_AT_EOF, 1, 19, 1 },
};

static nthw_fpga_register_init_s rpf_registers[] = {
	{ RPF_CONTROL, 0, 8, NTHW_FPGA_REG_TYPE_RW, 0, 4, rpf_control_fields },
	{ RPF_TS_SORT_PRG, 1, 20, NTHW_FPGA_REG_TYPE_RW, 1048451, 2, rpf_ts_sort_prg_fields },
};

static nthw_fpga_field_init_s rpl_ext_ctrl_fields[] = {
	{ RPL_EXT_CTRL_ADR, 10, 0, 0x0000 },
	{ RPL_EXT_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s rpl_ext_data_fields[] = {
	{ RPL_EXT_DATA_RPL_PTR, 12, 0, 0x0000 },
};

static nthw_fpga_field_init_s rpl_rcp_ctrl_fields[] = {
	{ RPL_RCP_CTRL_ADR, 4, 0, 0x0000 },
	{ RPL_RCP_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s rpl_rcp_data_fields[] = {
	{ RPL_RCP_DATA_DYN, 5, 0, 0x0000 }, { RPL_RCP_DATA_ETH_TYPE_WR, 1, 36, 0x0000 },
	{ RPL_RCP_DATA_EXT_PRIO, 1, 35, 0x0000 }, { RPL_RCP_DATA_LEN, 8, 15, 0x0000 },
	{ RPL_RCP_DATA_OFS, 10, 5, 0x0000 }, { RPL_RCP_DATA_RPL_PTR, 12, 23, 0x0000 },
};

static nthw_fpga_field_init_s rpl_rpl_ctrl_fields[] = {
	{ RPL_RPL_CTRL_ADR, 12, 0, 0x0000 },
	{ RPL_RPL_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s rpl_rpl_data_fields[] = {
	{ RPL_RPL_DATA_VALUE, 128, 0, 0x0000 },
};

static nthw_fpga_register_init_s rpl_registers[] = {
	{ RPL_EXT_CTRL, 2, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, rpl_ext_ctrl_fields },
	{ RPL_EXT_DATA, 3, 12, NTHW_FPGA_REG_TYPE_WO, 0, 1, rpl_ext_data_fields },
	{ RPL_RCP_CTRL, 0, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, rpl_rcp_ctrl_fields },
	{ RPL_RCP_DATA, 1, 37, NTHW_FPGA_REG_TYPE_WO, 0, 6, rpl_rcp_data_fields },
	{ RPL_RPL_CTRL, 4, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, rpl_rpl_ctrl_fields },
	{ RPL_RPL_DATA, 5, 128, NTHW_FPGA_REG_TYPE_WO, 0, 1, rpl_rpl_data_fields },
};

static nthw_fpga_field_init_s rpp_lr_ifr_rcp_ctrl_fields[] = {
	{ RPP_LR_IFR_RCP_CTRL_ADR, 4, 0, 0x0000 },
	{ RPP_LR_IFR_RCP_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s rpp_lr_ifr_rcp_data_fields[] = {
	{ RPP_LR_IFR_RCP_DATA_IPV4_DF_DROP, 1, 17, 0x0000 },
	{ RPP_LR_IFR_RCP_DATA_IPV4_EN, 1, 0, 0x0000 },
	{ RPP_LR_IFR_RCP_DATA_IPV6_DROP, 1, 16, 0x0000 },
	{ RPP_LR_IFR_RCP_DATA_IPV6_EN, 1, 1, 0x0000 },
	{ RPP_LR_IFR_RCP_DATA_MTU, 14, 2, 0x0000 },
};

static nthw_fpga_field_init_s rpp_lr_rcp_ctrl_fields[] = {
	{ RPP_LR_RCP_CTRL_ADR, 4, 0, 0x0000 },
	{ RPP_LR_RCP_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s rpp_lr_rcp_data_fields[] = {
	{ RPP_LR_RCP_DATA_EXP, 14, 0, 0x0000 },
};

static nthw_fpga_register_init_s rpp_lr_registers[] = {
	{ RPP_LR_IFR_RCP_CTRL, 2, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, rpp_lr_ifr_rcp_ctrl_fields },
	{ RPP_LR_IFR_RCP_DATA, 3, 18, NTHW_FPGA_REG_TYPE_WO, 0, 5, rpp_lr_ifr_rcp_data_fields },
	{ RPP_LR_RCP_CTRL, 0, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, rpp_lr_rcp_ctrl_fields },
	{ RPP_LR_RCP_DATA, 1, 14, NTHW_FPGA_REG_TYPE_WO, 0, 1, rpp_lr_rcp_data_fields },
};

static nthw_fpga_field_init_s rst9569_latch_fields[] = {
	{ RST9569_LATCH_DDR4_CALIB_COMPLETE, 1, 0, 0x0000 },
	{ RST9569_LATCH_PHY_FTILE_RDY, 1, 2, 0x0000 },
	{ RST9569_LATCH_PHY_FTILE_RST_DONE, 1, 1, 0x0000 },
};

static nthw_fpga_field_init_s rst9569_rst_fields[] = {
	{ RST9569_RST_DDR4, 1, 1, 1 },
	{ RST9569_RST_PHY_FTILE, 1, 2, 1 },
	{ RST9569_RST_SYS, 1, 0, 1 },
};

static nthw_fpga_field_init_s rst9569_stat_fields[] = {
	{ RST9569_STAT_DDR4_CALIB_COMPLETE, 1, 0, 0x0000 },
	{ RST9569_STAT_PHY_FTILE_RDY, 1, 2, 0x0000 },
	{ RST9569_STAT_PHY_FTILE_RST_DONE, 1, 1, 0x0000 },
};

static nthw_fpga_register_init_s rst9569_registers[] = {
	{ RST9569_LATCH, 2, 3, NTHW_FPGA_REG_TYPE_RW, 0, 3, rst9569_latch_fields },
	{ RST9569_RST, 0, 3, NTHW_FPGA_REG_TYPE_RW, 7, 3, rst9569_rst_fields },
	{ RST9569_STAT, 1, 3, NTHW_FPGA_REG_TYPE_RO, 0, 3, rst9569_stat_fields },
};

static nthw_fpga_field_init_s slc_rcp_ctrl_fields[] = {
	{ SLC_RCP_CTRL_ADR, 6, 0, 0x0000 },
	{ SLC_RCP_CTRL_CNT, 16, 16, 0x0000 },
};

static nthw_fpga_field_init_s slc_rcp_data_fields[] = {
	{ SLC_RCP_DATA_HEAD_DYN, 5, 1, 0x0000 }, { SLC_RCP_DATA_HEAD_OFS, 8, 6, 0x0000 },
	{ SLC_RCP_DATA_HEAD_SLC_EN, 1, 0, 0x0000 }, { SLC_RCP_DATA_PCAP, 1, 35, 0x0000 },
	{ SLC_RCP_DATA_TAIL_DYN, 5, 15, 0x0000 }, { SLC_RCP_DATA_TAIL_OFS, 15, 20, 0x0000 },
	{ SLC_RCP_DATA_TAIL_SLC_EN, 1, 14, 0x0000 },
};

static nthw_fpga_register_init_s slc_registers[] = {
	{ SLC_RCP_CTRL, 0, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, slc_rcp_ctrl_fields },
	{ SLC_RCP_DATA, 1, 36, NTHW_FPGA_REG_TYPE_WO, 0, 7, slc_rcp_data_fields },
};

static nthw_fpga_field_init_s spim_cfg_fields[] = {
	{ SPIM_CFG_PRE, 3, 0, 5 },
};

static nthw_fpga_field_init_s spim_cfg_clk_fields[] = {
	{ SPIM_CFG_CLK_MODE, 2, 0, 0 },
};

static nthw_fpga_field_init_s spim_cr_fields[] = {
	{ SPIM_CR_EN, 1, 1, 0 },
	{ SPIM_CR_LOOP, 1, 0, 0 },
	{ SPIM_CR_RXRST, 1, 3, 0 },
	{ SPIM_CR_TXRST, 1, 2, 0 },
};

static nthw_fpga_field_init_s spim_drr_fields[] = {
	{ SPIM_DRR_DRR, 32, 0, 0 },
};

static nthw_fpga_field_init_s spim_dtr_fields[] = {
	{ SPIM_DTR_DTR, 32, 0, 0 },
};

static nthw_fpga_field_init_s spim_sr_fields[] = {
	{ SPIM_SR_DONE, 1, 0, 0 }, { SPIM_SR_RXEMPTY, 1, 2, 1 }, { SPIM_SR_RXFULL, 1, 4, 0 },
	{ SPIM_SR_RXLVL, 8, 16, 0 }, { SPIM_SR_TXEMPTY, 1, 1, 1 }, { SPIM_SR_TXFULL, 1, 3, 0 },
	{ SPIM_SR_TXLVL, 8, 8, 0 },
};

static nthw_fpga_field_init_s spim_srr_fields[] = {
	{ SPIM_SRR_RST, 4, 0, 0 },
};

static nthw_fpga_register_init_s spim_registers[] = {
	{ SPIM_CFG, 5, 3, NTHW_FPGA_REG_TYPE_WO, 5, 1, spim_cfg_fields },
	{ SPIM_CFG_CLK, 6, 2, NTHW_FPGA_REG_TYPE_WO, 0, 1, spim_cfg_clk_fields },
	{ SPIM_CR, 1, 4, NTHW_FPGA_REG_TYPE_WO, 0, 4, spim_cr_fields },
	{ SPIM_DRR, 4, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, spim_drr_fields },
	{ SPIM_DTR, 3, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, spim_dtr_fields },
	{ SPIM_SR, 2, 24, NTHW_FPGA_REG_TYPE_MIXED, 6, 7, spim_sr_fields },
	{ SPIM_SRR, 0, 4, NTHW_FPGA_REG_TYPE_WO, 0, 1, spim_srr_fields },
};

static nthw_fpga_field_init_s spis_cr_fields[] = {
	{ SPIS_CR_DEBUG, 1, 4, 0 }, { SPIS_CR_EN, 1, 1, 0 }, { SPIS_CR_LOOP, 1, 0, 0 },
	{ SPIS_CR_RXRST, 1, 3, 0 }, { SPIS_CR_TXRST, 1, 2, 0 },
};

static nthw_fpga_field_init_s spis_drr_fields[] = {
	{ SPIS_DRR_DRR, 32, 0, 0 },
};

static nthw_fpga_field_init_s spis_dtr_fields[] = {
	{ SPIS_DTR_DTR, 32, 0, 0 },
};

static nthw_fpga_field_init_s spis_ram_ctrl_fields[] = {
	{ SPIS_RAM_CTRL_ADR, 6, 0, 0 },
	{ SPIS_RAM_CTRL_CNT, 6, 6, 0 },
};

static nthw_fpga_field_init_s spis_ram_data_fields[] = {
	{ SPIS_RAM_DATA_DATA, 32, 0, 0 },
};

static nthw_fpga_field_init_s spis_sr_fields[] = {
	{ SPIS_SR_DONE, 1, 0, 0 }, { SPIS_SR_FRAME_ERR, 1, 24, 0 },
	{ SPIS_SR_READ_ERR, 1, 25, 0 }, { SPIS_SR_RXEMPTY, 1, 2, 1 },
	{ SPIS_SR_RXFULL, 1, 4, 0 }, { SPIS_SR_RXLVL, 8, 16, 0 },
	{ SPIS_SR_TXEMPTY, 1, 1, 1 }, { SPIS_SR_TXFULL, 1, 3, 0 },
	{ SPIS_SR_TXLVL, 8, 8, 0 }, { SPIS_SR_WRITE_ERR, 1, 26, 0 },
};

static nthw_fpga_field_init_s spis_srr_fields[] = {
	{ SPIS_SRR_RST, 4, 0, 0 },
};

static nthw_fpga_register_init_s spis_registers[] = {
	{ SPIS_CR, 1, 5, NTHW_FPGA_REG_TYPE_WO, 0, 5, spis_cr_fields },
	{ SPIS_DRR, 4, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, spis_drr_fields },
	{ SPIS_DTR, 3, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, spis_dtr_fields },
	{ SPIS_RAM_CTRL, 5, 12, NTHW_FPGA_REG_TYPE_RW, 0, 2, spis_ram_ctrl_fields },
	{ SPIS_RAM_DATA, 6, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1, spis_ram_data_fields },
	{ SPIS_SR, 2, 27, NTHW_FPGA_REG_TYPE_RO, 6, 10, spis_sr_fields },
	{ SPIS_SRR, 0, 4, NTHW_FPGA_REG_TYPE_WO, 0, 1, spis_srr_fields },
};

static nthw_fpga_field_init_s sta_byte_fields[] = {
	{ STA_BYTE_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s sta_cfg_fields[] = {
	{ STA_CFG_CNT_CLEAR, 1, 1, 0 },
	{ STA_CFG_DMA_ENA, 1, 0, 0 },
};

static nthw_fpga_field_init_s sta_cv_err_fields[] = {
	{ STA_CV_ERR_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s sta_fcs_err_fields[] = {
	{ STA_FCS_ERR_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s sta_host_adr_lsb_fields[] = {
	{ STA_HOST_ADR_LSB_LSB, 32, 0, 0 },
};

static nthw_fpga_field_init_s sta_host_adr_msb_fields[] = {
	{ STA_HOST_ADR_MSB_MSB, 32, 0, 0 },
};

static nthw_fpga_field_init_s sta_load_bin_fields[] = {
	{ STA_LOAD_BIN_BIN, 32, 0, 8388607 },
};

static nthw_fpga_field_init_s sta_load_bps_rx_0_fields[] = {
	{ STA_LOAD_BPS_RX_0_BPS, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s sta_load_bps_rx_1_fields[] = {
	{ STA_LOAD_BPS_RX_1_BPS, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s sta_load_bps_tx_0_fields[] = {
	{ STA_LOAD_BPS_TX_0_BPS, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s sta_load_bps_tx_1_fields[] = {
	{ STA_LOAD_BPS_TX_1_BPS, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s sta_load_pps_rx_0_fields[] = {
	{ STA_LOAD_PPS_RX_0_PPS, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s sta_load_pps_rx_1_fields[] = {
	{ STA_LOAD_PPS_RX_1_PPS, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s sta_load_pps_tx_0_fields[] = {
	{ STA_LOAD_PPS_TX_0_PPS, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s sta_load_pps_tx_1_fields[] = {
	{ STA_LOAD_PPS_TX_1_PPS, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s sta_pckt_fields[] = {
	{ STA_PCKT_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s sta_status_fields[] = {
	{ STA_STATUS_STAT_TOGGLE_MISSED, 1, 0, 0x0000 },
};

static nthw_fpga_register_init_s sta_registers[] = {
	{ STA_BYTE, 4, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, sta_byte_fields },
	{ STA_CFG, 0, 2, NTHW_FPGA_REG_TYPE_RW, 0, 2, sta_cfg_fields },
	{ STA_CV_ERR, 5, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, sta_cv_err_fields },
	{ STA_FCS_ERR, 6, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, sta_fcs_err_fields },
	{ STA_HOST_ADR_LSB, 1, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, sta_host_adr_lsb_fields },
	{ STA_HOST_ADR_MSB, 2, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, sta_host_adr_msb_fields },
	{ STA_LOAD_BIN, 8, 32, NTHW_FPGA_REG_TYPE_WO, 8388607, 1, sta_load_bin_fields },
	{ STA_LOAD_BPS_RX_0, 11, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, sta_load_bps_rx_0_fields },
	{ STA_LOAD_BPS_RX_1, 13, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, sta_load_bps_rx_1_fields },
	{ STA_LOAD_BPS_TX_0, 15, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, sta_load_bps_tx_0_fields },
	{ STA_LOAD_BPS_TX_1, 17, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, sta_load_bps_tx_1_fields },
	{ STA_LOAD_PPS_RX_0, 10, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, sta_load_pps_rx_0_fields },
	{ STA_LOAD_PPS_RX_1, 12, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, sta_load_pps_rx_1_fields },
	{ STA_LOAD_PPS_TX_0, 14, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, sta_load_pps_tx_0_fields },
	{ STA_LOAD_PPS_TX_1, 16, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, sta_load_pps_tx_1_fields },
	{ STA_PCKT, 3, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, sta_pckt_fields },
	{ STA_STATUS, 7, 1, NTHW_FPGA_REG_TYPE_RC1, 0, 1, sta_status_fields },
};

static nthw_fpga_field_init_s tint_ctrl_fields[] = {
	{ TINT_CTRL_INTERVAL, 32, 0, 0 },
};

static nthw_fpga_field_init_s tint_status_fields[] = {
	{ TINT_STATUS_DELAYED, 8, 8, 0 },
	{ TINT_STATUS_SKIPPED, 8, 0, 0 },
};

static nthw_fpga_register_init_s tint_registers[] = {
	{ TINT_CTRL, 0, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, tint_ctrl_fields },
	{ TINT_STATUS, 1, 16, NTHW_FPGA_REG_TYPE_RC1, 0, 2, tint_status_fields },
};

static nthw_fpga_field_init_s tsm_con0_config_fields[] = {
	{ TSM_CON0_CONFIG_BLIND, 5, 8, 9 }, { TSM_CON0_CONFIG_DC_SRC, 3, 5, 0 },
	{ TSM_CON0_CONFIG_PORT, 3, 0, 0 }, { TSM_CON0_CONFIG_PPSIN_2_5V, 1, 13, 0 },
	{ TSM_CON0_CONFIG_SAMPLE_EDGE, 2, 3, 2 },
};

static nthw_fpga_field_init_s tsm_con0_interface_fields[] = {
	{ TSM_CON0_INTERFACE_EX_TERM, 2, 0, 3 }, { TSM_CON0_INTERFACE_IN_REF_PWM, 8, 12, 128 },
	{ TSM_CON0_INTERFACE_PWM_ENA, 1, 2, 0 }, { TSM_CON0_INTERFACE_RESERVED, 1, 3, 0 },
	{ TSM_CON0_INTERFACE_VTERM_PWM, 8, 4, 0 },
};

static nthw_fpga_field_init_s tsm_con0_sample_hi_fields[] = {
	{ TSM_CON0_SAMPLE_HI_SEC, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_con0_sample_lo_fields[] = {
	{ TSM_CON0_SAMPLE_LO_NS, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_con1_config_fields[] = {
	{ TSM_CON1_CONFIG_BLIND, 5, 8, 9 }, { TSM_CON1_CONFIG_DC_SRC, 3, 5, 0 },
	{ TSM_CON1_CONFIG_PORT, 3, 0, 0 }, { TSM_CON1_CONFIG_PPSIN_2_5V, 1, 13, 0 },
	{ TSM_CON1_CONFIG_SAMPLE_EDGE, 2, 3, 2 },
};

static nthw_fpga_field_init_s tsm_con1_sample_hi_fields[] = {
	{ TSM_CON1_SAMPLE_HI_SEC, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_con1_sample_lo_fields[] = {
	{ TSM_CON1_SAMPLE_LO_NS, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_con2_config_fields[] = {
	{ TSM_CON2_CONFIG_BLIND, 5, 8, 9 }, { TSM_CON2_CONFIG_DC_SRC, 3, 5, 0 },
	{ TSM_CON2_CONFIG_PORT, 3, 0, 0 }, { TSM_CON2_CONFIG_PPSIN_2_5V, 1, 13, 0 },
	{ TSM_CON2_CONFIG_SAMPLE_EDGE, 2, 3, 2 },
};

static nthw_fpga_field_init_s tsm_con2_sample_hi_fields[] = {
	{ TSM_CON2_SAMPLE_HI_SEC, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_con2_sample_lo_fields[] = {
	{ TSM_CON2_SAMPLE_LO_NS, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_con3_config_fields[] = {
	{ TSM_CON3_CONFIG_BLIND, 5, 5, 26 },
	{ TSM_CON3_CONFIG_PORT, 3, 0, 1 },
	{ TSM_CON3_CONFIG_SAMPLE_EDGE, 2, 3, 1 },
};

static nthw_fpga_field_init_s tsm_con3_sample_hi_fields[] = {
	{ TSM_CON3_SAMPLE_HI_SEC, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_con3_sample_lo_fields[] = {
	{ TSM_CON3_SAMPLE_LO_NS, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_con4_config_fields[] = {
	{ TSM_CON4_CONFIG_BLIND, 5, 5, 26 },
	{ TSM_CON4_CONFIG_PORT, 3, 0, 1 },
	{ TSM_CON4_CONFIG_SAMPLE_EDGE, 2, 3, 1 },
};

static nthw_fpga_field_init_s tsm_con4_sample_hi_fields[] = {
	{ TSM_CON4_SAMPLE_HI_SEC, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_con4_sample_lo_fields[] = {
	{ TSM_CON4_SAMPLE_LO_NS, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_con5_config_fields[] = {
	{ TSM_CON5_CONFIG_BLIND, 5, 5, 26 },
	{ TSM_CON5_CONFIG_PORT, 3, 0, 1 },
	{ TSM_CON5_CONFIG_SAMPLE_EDGE, 2, 3, 1 },
};

static nthw_fpga_field_init_s tsm_con5_sample_hi_fields[] = {
	{ TSM_CON5_SAMPLE_HI_SEC, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_con5_sample_lo_fields[] = {
	{ TSM_CON5_SAMPLE_LO_TIME, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_con6_config_fields[] = {
	{ TSM_CON6_CONFIG_BLIND, 5, 5, 26 },
	{ TSM_CON6_CONFIG_PORT, 3, 0, 1 },
	{ TSM_CON6_CONFIG_SAMPLE_EDGE, 2, 3, 1 },
};

static nthw_fpga_field_init_s tsm_con6_sample_hi_fields[] = {
	{ TSM_CON6_SAMPLE_HI_SEC, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_con6_sample_lo_fields[] = {
	{ TSM_CON6_SAMPLE_LO_NS, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_con7_host_sample_hi_fields[] = {
	{ TSM_CON7_HOST_SAMPLE_HI_SEC, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_con7_host_sample_lo_fields[] = {
	{ TSM_CON7_HOST_SAMPLE_LO_NS, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_config_fields[] = {
	{ TSM_CONFIG_NTTS_SRC, 2, 5, 0 }, { TSM_CONFIG_NTTS_SYNC, 1, 4, 0 },
	{ TSM_CONFIG_TIMESET_EDGE, 2, 8, 1 }, { TSM_CONFIG_TIMESET_SRC, 3, 10, 0 },
	{ TSM_CONFIG_TIMESET_UP, 1, 7, 0 }, { TSM_CONFIG_TS_FORMAT, 4, 0, 1 },
};

static nthw_fpga_field_init_s tsm_int_config_fields[] = {
	{ TSM_INT_CONFIG_AUTO_DISABLE, 1, 0, 0 },
	{ TSM_INT_CONFIG_MASK, 19, 1, 0 },
};

static nthw_fpga_field_init_s tsm_int_stat_fields[] = {
	{ TSM_INT_STAT_CAUSE, 19, 1, 0 },
	{ TSM_INT_STAT_ENABLE, 1, 0, 0 },
};

static nthw_fpga_field_init_s tsm_led_fields[] = {
	{ TSM_LED_LED0_BG_COLOR, 2, 3, 0 }, { TSM_LED_LED0_COLOR, 2, 1, 0 },
	{ TSM_LED_LED0_MODE, 1, 0, 0 }, { TSM_LED_LED0_SRC, 4, 5, 0 },
	{ TSM_LED_LED1_BG_COLOR, 2, 12, 0 }, { TSM_LED_LED1_COLOR, 2, 10, 0 },
	{ TSM_LED_LED1_MODE, 1, 9, 0 }, { TSM_LED_LED1_SRC, 4, 14, 1 },
	{ TSM_LED_LED2_BG_COLOR, 2, 21, 0 }, { TSM_LED_LED2_COLOR, 2, 19, 0 },
	{ TSM_LED_LED2_MODE, 1, 18, 0 }, { TSM_LED_LED2_SRC, 4, 23, 2 },
};

static nthw_fpga_field_init_s tsm_ntts_config_fields[] = {
	{ TSM_NTTS_CONFIG_AUTO_HARDSET, 1, 5, 1 },
	{ TSM_NTTS_CONFIG_EXT_CLK_ADJ, 1, 6, 0 },
	{ TSM_NTTS_CONFIG_HIGH_SAMPLE, 1, 4, 0 },
	{ TSM_NTTS_CONFIG_TS_SRC_FORMAT, 4, 0, 0 },
};

static nthw_fpga_field_init_s tsm_ntts_ext_stat_fields[] = {
	{ TSM_NTTS_EXT_STAT_MASTER_ID, 8, 16, 0x0000 },
	{ TSM_NTTS_EXT_STAT_MASTER_REV, 8, 24, 0x0000 },
	{ TSM_NTTS_EXT_STAT_MASTER_STAT, 16, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_ntts_limit_hi_fields[] = {
	{ TSM_NTTS_LIMIT_HI_SEC, 16, 0, 0 },
};

static nthw_fpga_field_init_s tsm_ntts_limit_lo_fields[] = {
	{ TSM_NTTS_LIMIT_LO_NS, 32, 0, 100000 },
};

static nthw_fpga_field_init_s tsm_ntts_offset_fields[] = {
	{ TSM_NTTS_OFFSET_NS, 30, 0, 0 },
};

static nthw_fpga_field_init_s tsm_ntts_sample_hi_fields[] = {
	{ TSM_NTTS_SAMPLE_HI_SEC, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_ntts_sample_lo_fields[] = {
	{ TSM_NTTS_SAMPLE_LO_NS, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_ntts_stat_fields[] = {
	{ TSM_NTTS_STAT_NTTS_VALID, 1, 0, 0 },
	{ TSM_NTTS_STAT_SIGNAL_LOST, 8, 1, 0 },
	{ TSM_NTTS_STAT_SYNC_LOST, 8, 9, 0 },
};

static nthw_fpga_field_init_s tsm_ntts_ts_t0_hi_fields[] = {
	{ TSM_NTTS_TS_T0_HI_TIME, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_ntts_ts_t0_lo_fields[] = {
	{ TSM_NTTS_TS_T0_LO_TIME, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_ntts_ts_t0_offset_fields[] = {
	{ TSM_NTTS_TS_T0_OFFSET_COUNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_pb_ctrl_fields[] = {
	{ TSM_PB_CTRL_INSTMEM_WR, 1, 1, 0 },
	{ TSM_PB_CTRL_RST, 1, 0, 0 },
};

static nthw_fpga_field_init_s tsm_pb_instmem_fields[] = {
	{ TSM_PB_INSTMEM_MEM_ADDR, 14, 0, 0 },
	{ TSM_PB_INSTMEM_MEM_DATA, 18, 14, 0 },
};

static nthw_fpga_field_init_s tsm_pi_ctrl_i_fields[] = {
	{ TSM_PI_CTRL_I_VAL, 32, 0, 0 },
};

static nthw_fpga_field_init_s tsm_pi_ctrl_ki_fields[] = {
	{ TSM_PI_CTRL_KI_GAIN, 24, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_pi_ctrl_kp_fields[] = {
	{ TSM_PI_CTRL_KP_GAIN, 24, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_pi_ctrl_shl_fields[] = {
	{ TSM_PI_CTRL_SHL_VAL, 4, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_stat_fields[] = {
	{ TSM_STAT_HARD_SYNC, 8, 8, 0 }, { TSM_STAT_LINK_CON0, 1, 0, 0 },
	{ TSM_STAT_LINK_CON1, 1, 1, 0 }, { TSM_STAT_LINK_CON2, 1, 2, 0 },
	{ TSM_STAT_LINK_CON3, 1, 3, 0 }, { TSM_STAT_LINK_CON4, 1, 4, 0 },
	{ TSM_STAT_LINK_CON5, 1, 5, 0 }, { TSM_STAT_NTTS_INSYNC, 1, 6, 0 },
	{ TSM_STAT_PTP_MI_PRESENT, 1, 7, 0 },
};

static nthw_fpga_field_init_s tsm_timer_ctrl_fields[] = {
	{ TSM_TIMER_CTRL_TIMER_EN_T0, 1, 0, 0 },
	{ TSM_TIMER_CTRL_TIMER_EN_T1, 1, 1, 0 },
};

static nthw_fpga_field_init_s tsm_timer_t0_fields[] = {
	{ TSM_TIMER_T0_MAX_COUNT, 30, 0, 50000 },
};

static nthw_fpga_field_init_s tsm_timer_t1_fields[] = {
	{ TSM_TIMER_T1_MAX_COUNT, 30, 0, 50000 },
};

static nthw_fpga_field_init_s tsm_time_hardset_hi_fields[] = {
	{ TSM_TIME_HARDSET_HI_TIME, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_time_hardset_lo_fields[] = {
	{ TSM_TIME_HARDSET_LO_TIME, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_time_hi_fields[] = {
	{ TSM_TIME_HI_SEC, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_time_lo_fields[] = {
	{ TSM_TIME_LO_NS, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_time_rate_adj_fields[] = {
	{ TSM_TIME_RATE_ADJ_FRACTION, 29, 0, 0 },
};

static nthw_fpga_field_init_s tsm_ts_hi_fields[] = {
	{ TSM_TS_HI_TIME, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_ts_lo_fields[] = {
	{ TSM_TS_LO_TIME, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s tsm_ts_offset_fields[] = {
	{ TSM_TS_OFFSET_NS, 30, 0, 0 },
};

static nthw_fpga_field_init_s tsm_ts_stat_fields[] = {
	{ TSM_TS_STAT_OVERRUN, 1, 16, 0 },
	{ TSM_TS_STAT_SAMPLES, 16, 0, 0 },
};

static nthw_fpga_field_init_s tsm_ts_stat_hi_offset_fields[] = {
	{ TSM_TS_STAT_HI_OFFSET_NS, 32, 0, 0 },
};

static nthw_fpga_field_init_s tsm_ts_stat_lo_offset_fields[] = {
	{ TSM_TS_STAT_LO_OFFSET_NS, 32, 0, 0 },
};

static nthw_fpga_field_init_s tsm_ts_stat_tar_hi_fields[] = {
	{ TSM_TS_STAT_TAR_HI_SEC, 32, 0, 0 },
};

static nthw_fpga_field_init_s tsm_ts_stat_tar_lo_fields[] = {
	{ TSM_TS_STAT_TAR_LO_NS, 32, 0, 0 },
};

static nthw_fpga_field_init_s tsm_ts_stat_x_fields[] = {
	{ TSM_TS_STAT_X_NS, 32, 0, 0 },
};

static nthw_fpga_field_init_s tsm_ts_stat_x2_hi_fields[] = {
	{ TSM_TS_STAT_X2_HI_NS, 16, 0, 0 },
};

static nthw_fpga_field_init_s tsm_ts_stat_x2_lo_fields[] = {
	{ TSM_TS_STAT_X2_LO_NS, 32, 0, 0 },
};

static nthw_fpga_field_init_s tsm_utc_offset_fields[] = {
	{ TSM_UTC_OFFSET_SEC, 8, 0, 0 },
};

static nthw_fpga_register_init_s tsm_registers[] = {
	{ TSM_CON0_CONFIG, 24, 14, NTHW_FPGA_REG_TYPE_RW, 2320, 5, tsm_con0_config_fields },
	{
		TSM_CON0_INTERFACE, 25, 20, NTHW_FPGA_REG_TYPE_RW, 524291, 5,
		tsm_con0_interface_fields
	},
	{ TSM_CON0_SAMPLE_HI, 27, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_con0_sample_hi_fields },
	{ TSM_CON0_SAMPLE_LO, 26, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_con0_sample_lo_fields },
	{ TSM_CON1_CONFIG, 28, 14, NTHW_FPGA_REG_TYPE_RW, 2320, 5, tsm_con1_config_fields },
	{ TSM_CON1_SAMPLE_HI, 30, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_con1_sample_hi_fields },
	{ TSM_CON1_SAMPLE_LO, 29, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_con1_sample_lo_fields },
	{ TSM_CON2_CONFIG, 31, 14, NTHW_FPGA_REG_TYPE_RW, 2320, 5, tsm_con2_config_fields },
	{ TSM_CON2_SAMPLE_HI, 33, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_con2_sample_hi_fields },
	{ TSM_CON2_SAMPLE_LO, 32, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_con2_sample_lo_fields },
	{ TSM_CON3_CONFIG, 34, 10, NTHW_FPGA_REG_TYPE_RW, 841, 3, tsm_con3_config_fields },
	{ TSM_CON3_SAMPLE_HI, 36, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_con3_sample_hi_fields },
	{ TSM_CON3_SAMPLE_LO, 35, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_con3_sample_lo_fields },
	{ TSM_CON4_CONFIG, 37, 10, NTHW_FPGA_REG_TYPE_RW, 841, 3, tsm_con4_config_fields },
	{ TSM_CON4_SAMPLE_HI, 39, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_con4_sample_hi_fields },
	{ TSM_CON4_SAMPLE_LO, 38, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_con4_sample_lo_fields },
	{ TSM_CON5_CONFIG, 40, 10, NTHW_FPGA_REG_TYPE_RW, 841, 3, tsm_con5_config_fields },
	{ TSM_CON5_SAMPLE_HI, 42, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_con5_sample_hi_fields },
	{ TSM_CON5_SAMPLE_LO, 41, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_con5_sample_lo_fields },
	{ TSM_CON6_CONFIG, 43, 10, NTHW_FPGA_REG_TYPE_RW, 841, 3, tsm_con6_config_fields },
	{ TSM_CON6_SAMPLE_HI, 45, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_con6_sample_hi_fields },
	{ TSM_CON6_SAMPLE_LO, 44, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_con6_sample_lo_fields },
	{
		TSM_CON7_HOST_SAMPLE_HI, 47, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1,
		tsm_con7_host_sample_hi_fields
	},
	{
		TSM_CON7_HOST_SAMPLE_LO, 46, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1,
		tsm_con7_host_sample_lo_fields
	},
	{ TSM_CONFIG, 0, 13, NTHW_FPGA_REG_TYPE_RW, 257, 6, tsm_config_fields },
	{ TSM_INT_CONFIG, 2, 20, NTHW_FPGA_REG_TYPE_RW, 0, 2, tsm_int_config_fields },
	{ TSM_INT_STAT, 3, 20, NTHW_FPGA_REG_TYPE_MIXED, 0, 2, tsm_int_stat_fields },
	{ TSM_LED, 4, 27, NTHW_FPGA_REG_TYPE_RW, 16793600, 12, tsm_led_fields },
	{ TSM_NTTS_CONFIG, 13, 7, NTHW_FPGA_REG_TYPE_RW, 32, 4, tsm_ntts_config_fields },
	{ TSM_NTTS_EXT_STAT, 15, 32, NTHW_FPGA_REG_TYPE_MIXED, 0, 3, tsm_ntts_ext_stat_fields },
	{ TSM_NTTS_LIMIT_HI, 23, 16, NTHW_FPGA_REG_TYPE_RW, 0, 1, tsm_ntts_limit_hi_fields },
	{ TSM_NTTS_LIMIT_LO, 22, 32, NTHW_FPGA_REG_TYPE_RW, 100000, 1, tsm_ntts_limit_lo_fields },
	{ TSM_NTTS_OFFSET, 21, 30, NTHW_FPGA_REG_TYPE_RW, 0, 1, tsm_ntts_offset_fields },
	{ TSM_NTTS_SAMPLE_HI, 19, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_ntts_sample_hi_fields },
	{ TSM_NTTS_SAMPLE_LO, 18, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_ntts_sample_lo_fields },
	{ TSM_NTTS_STAT, 14, 17, NTHW_FPGA_REG_TYPE_RO, 0, 3, tsm_ntts_stat_fields },
	{ TSM_NTTS_TS_T0_HI, 17, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_ntts_ts_t0_hi_fields },
	{ TSM_NTTS_TS_T0_LO, 16, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_ntts_ts_t0_lo_fields },
	{
		TSM_NTTS_TS_T0_OFFSET, 20, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1,
		tsm_ntts_ts_t0_offset_fields
	},
	{ TSM_PB_CTRL, 63, 2, NTHW_FPGA_REG_TYPE_WO, 0, 2, tsm_pb_ctrl_fields },
	{ TSM_PB_INSTMEM, 64, 32, NTHW_FPGA_REG_TYPE_WO, 0, 2, tsm_pb_instmem_fields },
	{ TSM_PI_CTRL_I, 54, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, tsm_pi_ctrl_i_fields },
	{ TSM_PI_CTRL_KI, 52, 24, NTHW_FPGA_REG_TYPE_RW, 0, 1, tsm_pi_ctrl_ki_fields },
	{ TSM_PI_CTRL_KP, 51, 24, NTHW_FPGA_REG_TYPE_RW, 0, 1, tsm_pi_ctrl_kp_fields },
	{ TSM_PI_CTRL_SHL, 53, 4, NTHW_FPGA_REG_TYPE_WO, 0, 1, tsm_pi_ctrl_shl_fields },
	{ TSM_STAT, 1, 16, NTHW_FPGA_REG_TYPE_RO, 0, 9, tsm_stat_fields },
	{ TSM_TIMER_CTRL, 48, 2, NTHW_FPGA_REG_TYPE_RW, 0, 2, tsm_timer_ctrl_fields },
	{ TSM_TIMER_T0, 49, 30, NTHW_FPGA_REG_TYPE_RW, 50000, 1, tsm_timer_t0_fields },
	{ TSM_TIMER_T1, 50, 30, NTHW_FPGA_REG_TYPE_RW, 50000, 1, tsm_timer_t1_fields },
	{ TSM_TIME_HARDSET_HI, 12, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_time_hardset_hi_fields },
	{ TSM_TIME_HARDSET_LO, 11, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_time_hardset_lo_fields },
	{ TSM_TIME_HI, 9, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1, tsm_time_hi_fields },
	{ TSM_TIME_LO, 8, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1, tsm_time_lo_fields },
	{ TSM_TIME_RATE_ADJ, 10, 29, NTHW_FPGA_REG_TYPE_RW, 0, 1, tsm_time_rate_adj_fields },
	{ TSM_TS_HI, 6, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_ts_hi_fields },
	{ TSM_TS_LO, 5, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_ts_lo_fields },
	{ TSM_TS_OFFSET, 7, 30, NTHW_FPGA_REG_TYPE_RW, 0, 1, tsm_ts_offset_fields },
	{ TSM_TS_STAT, 55, 17, NTHW_FPGA_REG_TYPE_RO, 0, 2, tsm_ts_stat_fields },
	{
		TSM_TS_STAT_HI_OFFSET, 62, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1,
		tsm_ts_stat_hi_offset_fields
	},
	{
		TSM_TS_STAT_LO_OFFSET, 61, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1,
		tsm_ts_stat_lo_offset_fields
	},
	{ TSM_TS_STAT_TAR_HI, 57, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_ts_stat_tar_hi_fields },
	{ TSM_TS_STAT_TAR_LO, 56, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_ts_stat_tar_lo_fields },
	{ TSM_TS_STAT_X, 58, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_ts_stat_x_fields },
	{ TSM_TS_STAT_X2_HI, 60, 16, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_ts_stat_x2_hi_fields },
	{ TSM_TS_STAT_X2_LO, 59, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, tsm_ts_stat_x2_lo_fields },
	{ TSM_UTC_OFFSET, 65, 8, NTHW_FPGA_REG_TYPE_RW, 0, 1, tsm_utc_offset_fields },
};

static nthw_fpga_module_init_s fpga_modules[] = {
	{ MOD_CAT, 0, MOD_CAT, 0, 21, NTHW_FPGA_BUS_TYPE_RAB1, 768, 34, cat_registers },
	{ MOD_CSU, 0, MOD_CSU, 0, 0, NTHW_FPGA_BUS_TYPE_RAB1, 9728, 2, csu_registers },
	{ MOD_DBS, 0, MOD_DBS, 0, 11, NTHW_FPGA_BUS_TYPE_RAB2, 1568, 27, dbs_registers },
	{ MOD_FLM, 0, MOD_FLM, 0, 25, NTHW_FPGA_BUS_TYPE_RAB1, 1280, 43, flm_registers },
	{ MOD_GFG, 0, MOD_GFG, 1, 1, NTHW_FPGA_BUS_TYPE_RAB2, 5120, 10, gfg_registers },
	{ MOD_GMF, 0, MOD_GMF, 2, 5, NTHW_FPGA_BUS_TYPE_RAB2, 5632, 12, gmf_registers },
	{ MOD_GMF, 1, MOD_GMF, 2, 5, NTHW_FPGA_BUS_TYPE_RAB2, 6144, 12, gmf_registers },
	{ MOD_HFU, 0, MOD_HFU, 0, 2, NTHW_FPGA_BUS_TYPE_RAB1, 9472, 2, hfu_registers },
	{ MOD_HIF, 0, MOD_HIF, 0, 0, NTHW_FPGA_BUS_TYPE_PCI, 0, 18, hif_registers },
	{ MOD_HSH, 0, MOD_HSH, 0, 5, NTHW_FPGA_BUS_TYPE_RAB1, 1536, 2, hsh_registers },
	{ MOD_I2CM, 0, MOD_I2CM, 0, 1, NTHW_FPGA_BUS_TYPE_RAB0, 640, 7, i2cm_registers },
	{ MOD_IFR, 0, MOD_IFR, 0, 7, NTHW_FPGA_BUS_TYPE_RAB1, 9984, 6, ifr_registers },
	{ MOD_IGAM, 0, MOD_IGAM, 0, 1, NTHW_FPGA_BUS_TYPE_RAB0, 1040, 3, igam_registers },
	{ MOD_KM, 0, MOD_KM, 0, 7, NTHW_FPGA_BUS_TYPE_RAB1, 1024, 11, km_registers },
	{
		MOD_PCI_RD_TG, 0, MOD_PCI_RD_TG, 0, 1, NTHW_FPGA_BUS_TYPE_RAB0, 1312, 6,
		pci_rd_tg_registers
	},
	{ MOD_PCI_TA, 0, MOD_PCI_TA, 0, 0, NTHW_FPGA_BUS_TYPE_RAB0, 1328, 5, pci_ta_registers },
	{
		MOD_PCI_WR_TG, 0, MOD_PCI_WR_TG, 0, 1, NTHW_FPGA_BUS_TYPE_RAB0, 1296, 7,
		pci_wr_tg_registers
	},
	{
		MOD_PCM_NT400DXX, 0, MOD_PCM_NT400DXX, 0, 3, NTHW_FPGA_BUS_TYPE_RAB0, 1024, 3,
		pcm_nt400dxx_registers
	},
	{ MOD_PDB, 0, MOD_PDB, 0, 9, NTHW_FPGA_BUS_TYPE_RAB1, 2560, 3, pdb_registers },
	{ MOD_PDI, 0, MOD_PDI, 1, 1, NTHW_FPGA_BUS_TYPE_RAB0, 64, 6, pdi_registers },
	{
		MOD_PHY_TILE, 0, MOD_PHY_TILE, 0, 10, NTHW_FPGA_BUS_TYPE_RAB2, 7168, 33,
		phy_tile_registers
	},
	{
		MOD_PRM_NT400DXX, 0, MOD_PRM_NT400DXX, 0, 0, NTHW_FPGA_BUS_TYPE_RAB0, 1032, 1,
		prm_nt400dxx_registers
	},
	{ MOD_QSL, 0, MOD_QSL, 0, 7, NTHW_FPGA_BUS_TYPE_RAB1, 1792, 8, qsl_registers },
	{ MOD_RAC, 0, MOD_RAC, 3, 0, NTHW_FPGA_BUS_TYPE_PCI, 8192, 14, rac_registers },
	{ MOD_RFD, 0, MOD_RFD, 0, 4, NTHW_FPGA_BUS_TYPE_RAB1, 256, 5, rfd_registers },
	{ MOD_RPF, 0, MOD_RPF, 0, 2, NTHW_FPGA_BUS_TYPE_RAB2, 4096, 2, rpf_registers },
	{ MOD_RPP_LR, 0, MOD_RPP_LR, 0, 2, NTHW_FPGA_BUS_TYPE_RAB1, 2304, 4, rpp_lr_registers },
	{ MOD_RST9569, 0, MOD_RST9569, 0, 2, NTHW_FPGA_BUS_TYPE_RAB2, 0, 3, rst9569_registers },
	{ MOD_SLC_LR, 0, MOD_SLC, 0, 2, NTHW_FPGA_BUS_TYPE_RAB1, 2048, 2, slc_registers },
	{ MOD_SPIM, 0, MOD_SPIM, 1, 1, NTHW_FPGA_BUS_TYPE_RAB0, 80, 7, spim_registers },
	{ MOD_SPIS, 0, MOD_SPIS, 1, 0, NTHW_FPGA_BUS_TYPE_RAB0, 256, 7, spis_registers },
	{ MOD_STA, 0, MOD_STA, 0, 9, NTHW_FPGA_BUS_TYPE_RAB2, 1536, 17, sta_registers },
	{ MOD_TINT, 0, MOD_TINT, 0, 0, NTHW_FPGA_BUS_TYPE_RAB2, 1024, 2, tint_registers },
	{ MOD_TSM, 0, MOD_TSM, 0, 8, NTHW_FPGA_BUS_TYPE_RAB2, 512, 66, tsm_registers },
	{ MOD_TX_CPY, 0, MOD_CPY, 0, 4, NTHW_FPGA_BUS_TYPE_RAB1, 9216, 26, cpy_registers },
	{ MOD_TX_INS, 0, MOD_INS, 0, 2, NTHW_FPGA_BUS_TYPE_RAB1, 8704, 2, ins_registers },
	{ MOD_TX_RPL, 0, MOD_RPL, 0, 4, NTHW_FPGA_BUS_TYPE_RAB1, 8960, 6, rpl_registers },
};

static nthw_fpga_prod_param_s product_parameters[] = {
	{ NT_BUILD_NUMBER, 0 },
	{ NT_BUILD_TIME, 1726787043 },
	{ NT_CATEGORIES, 64 },
	{ NT_CAT_DCT_PRESENT, 0 },
	{ NT_CAT_END_OFS_SUPPORT, 0 },
	{ NT_CAT_FUNCS, 64 },
	{ NT_CAT_KCC_BANKS, 3 },
	{ NT_CAT_KCC_PRESENT, 0 },
	{ NT_CAT_KCC_SIZE, 1536 },
	{ NT_CAT_KM_IF_CNT, 2 },
	{ NT_CAT_KM_IF_M0, 0 },
	{ NT_CAT_KM_IF_M1, 1 },
	{ NT_CAT_N_CMP, 8 },
	{ NT_CAT_N_EXT, 4 },
	{ NT_CAT_N_LEN, 8 },
	{ NT_CB_DEBUG, 0 },
	{ NT_COR_CATEGORIES, 16 },
	{ NT_COR_PRESENT, 0 },
	{ NT_CSU_PRESENT, 1 },
	{ NT_DBS_PRESENT, 1 },
	{ NT_DBS_RX_QUEUES, 128 },
	{ NT_DBS_TX_PORTS, 2 },
	{ NT_DBS_TX_QUEUES, 128 },
	{ NT_DDP_PRESENT, 0 },
	{ NT_DDP_TBL_DEPTH, 1024 },
	{ NT_EMI_SPLIT_STEPS, 16 },
	{ NT_EOF_TIMESTAMP_ONLY, 1 },
	{ NT_EPP_CATEGORIES, 32 },
	{ NT_FLM_CACHE, 1 },
	{ NT_FLM_CATEGORIES, 32 },
	{ NT_FLM_ENTRY_SIZE, 64 },
	{ NT_FLM_LOAD_APS_MAX, 270000000 },
	{ NT_FLM_LOAD_LPS_MAX, 600000000 },
	{ NT_FLM_PRESENT, 1 },
	{ NT_FLM_PRIOS, 4 },
	{ NT_FLM_PST_PROFILES, 16 },
	{ NT_FLM_SCRUB_PROFILES, 16 },
	{ NT_FLM_SIZE_MB, 12288 },
	{ NT_FLM_STATEFUL, 1 },
	{ NT_FLM_VARIANT, 2 },
	{ NT_GFG_PRESENT, 1 },
	{ NT_GFG_TX_LIVE_RECONFIG_SUPPORT, 1 },
	{ NT_GMF_FCS_PRESENT, 0 },
	{ NT_GMF_IFG_SPEED_DIV, 34 },
	{ NT_GMF_IFG_SPEED_DIV100G, 34 },
	{ NT_GMF_IFG_SPEED_MUL, 16 },
	{ NT_GMF_IFG_SPEED_MUL100G, 16 },
	{ NT_GROUP_ID, 9569 },
	{ NT_HFU_PRESENT, 1 },
	{ NT_HIF_MSIX_BAR, 1 },
	{ NT_HIF_MSIX_PBA_OFS, 8192 },
	{ NT_HIF_MSIX_PRESENT, 1 },
	{ NT_HIF_MSIX_TBL_OFS, 0 },
	{ NT_HIF_MSIX_TBL_SIZE, 8 },
	{ NT_HIF_PER_PS, 2222 },
	{ NT_HIF_SRIOV_PRESENT, 1 },
	{ NT_HIF_VF_OFFSET, 1 },
	{ NT_HSH_CATEGORIES, 16 },
	{ NT_HSH_TOEPLITZ, 1 },
	{ NT_HST_CATEGORIES, 32 },
	{ NT_HST_PRESENT, 0 },
	{ NT_IOA_CATEGORIES, 1024 },
	{ NT_IOA_PRESENT, 0 },
	{ NT_IPF_PRESENT, 0 },
	{ NT_KM_CAM_BANKS, 3 },
	{ NT_KM_CAM_RECORDS, 2048 },
	{ NT_KM_CAM_REC_WORDS, 6 },
	{ NT_KM_CATEGORIES, 32 },
	{ NT_KM_END_OFS_SUPPORT, 0 },
	{ NT_KM_EXT_EXTRACTORS, 1 },
	{ NT_KM_FLOW_TYPES, 16 },
	{ NT_KM_PRESENT, 1 },
	{ NT_KM_SWX_PRESENT, 0 },
	{ NT_KM_SYNERGY_MATCH, 0 },
	{ NT_KM_TCAM_BANKS, 12 },
	{ NT_KM_TCAM_BANK_WIDTH, 72 },
	{ NT_KM_TCAM_HIT_QUAL, 0 },
	{ NT_KM_TCAM_KEYWAY, 1 },
	{ NT_KM_WIDE, 0 },
	{ NT_LR_PRESENT, 1 },
	{ NT_MCU_PRESENT, 0 },
	{ NT_MDG_DEBUG_FLOW_CONTROL, 0 },
	{ NT_MDG_DEBUG_REG_READ_BACK, 0 },
	{ NT_MSK_CATEGORIES, 32 },
	{ NT_MSK_PRESENT, 0 },
	{ NT_NFV_OVS_PRODUCT, 0 },
	{ NT_NIMS, 2 },
	{ NT_PCI_DEVICE_ID, 533 },
	{ NT_PCI_TA_TG_PRESENT, 1 },
	{ NT_PCI_VENDOR_ID, 6388 },
	{ NT_PDB_CATEGORIES, 16 },
	{ NT_PHY_ANEG_PRESENT, 0 },
	{ NT_PHY_KRFEC_PRESENT, 0 },
	{ NT_PHY_PORTS, 2 },
	{ NT_PHY_PORTS_PER_QUAD, 1 },
	{ NT_PHY_QUADS, 2 },
	{ NT_PHY_RSFEC_PRESENT, 0 },
	{ NT_QM_CELLS, 2097152 },
	{ NT_QM_CELL_SIZE, 6144 },
	{ NT_QM_PRESENT, 0 },
	{ NT_QSL_CATEGORIES, 32 },
	{ NT_QSL_COLOR_SEL_BW, 7 },
	{ NT_QSL_QST_SIZE, 4096 },
	{ NT_QUEUES, 128 },
	{ NT_RAC_RAB_INTERFACES, 3 },
	{ NT_RAC_RAB_OB_UPDATE, 0 },
	{ NT_REVISION_ID, 49 },
	{ NT_RMC_LAG_GROUPS, 1 },
	{ NT_RMC_PRESENT, 0 },
	{ NT_ROA_CATEGORIES, 1024 },
	{ NT_ROA_PRESENT, 0 },
	{ NT_RPF_MATURING_DEL_DEFAULT, -125 },
	{ NT_RPF_PRESENT, 1 },
	{ NT_RPP_PER_PS, 1666 },
	{ NT_RTX_PRESENT, 0 },
	{ NT_RX_HOST_BUFFERS, 128 },
	{ NT_RX_PORTS, 2 },
	{ NT_RX_PORT_REPLICATE, 0 },
	{ NT_SLB_PRESENT, 0 },
	{ NT_SLC_LR_PRESENT, 1 },
	{ NT_SLC_PRESENT, 0 },
	{ NT_STA_COLORS, 64 },
	{ NT_STA_LOAD_AVG_RX, 1 },
	{ NT_STA_LOAD_AVG_TX, 1 },
	{ NT_STA_RX_PORTS, 2 },
	{ NT_TBH_DEBUG_DLN, 1 },
	{ NT_TBH_PRESENT, 0 },
	{ NT_TFD_PRESENT, 1 },
	{ NT_TPE_CATEGORIES, 16 },
	{ NT_TSM_OST_ONLY, 1 },
	{ NT_TS_APPEND, 0 },
	{ NT_TS_INJECT_PRESENT, 0 },
	{ NT_TX_CPY_PACKET_READERS, 0 },
	{ NT_TX_CPY_PRESENT, 1 },
	{ NT_TX_CPY_SIDEBAND_READERS, 7 },
	{ NT_TX_CPY_VARIANT, 0 },
	{ NT_TX_CPY_WRITERS, 6 },
	{ NT_TX_HOST_BUFFERS, 128 },
	{ NT_TX_INS_OFS_ZERO, 1 },
	{ NT_TX_INS_PRESENT, 1 },
	{ NT_TX_MTU_PROFILE_IFR, 16 },
	{ NT_TX_ON_TIMESTAMP, 1 },
	{ NT_TX_PORTS, 2 },
	{ NT_TX_PORT_REPLICATE, 1 },
	{ NT_TX_RPL_DEPTH, 4096 },
	{ NT_TX_RPL_EXT_CATEGORIES, 1024 },
	{ NT_TX_RPL_OFS_ZERO, 1 },
	{ NT_TX_RPL_PRESENT, 1 },
	{ NT_TYPE_ID, 200 },
	{ NT_USE_TRIPLE_SPEED, 0 },
	{ NT_VERSION_ID, 55 },
	{ NT_VLI_PRESENT, 0 },
	{ 0, -1 },	/* END */
};

nthw_fpga_prod_init_s nthw_fpga_9569_055_049_0000 = {
	200, 9569, 55, 49, 0, 0, 1726787043, 152, product_parameters, 37, fpga_modules,
};

/*
 * Auto-generated file - do *NOT* edit
 */
