/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024-2026 Beijing WangXun Technology Co., Ltd.
 */

#ifndef _TXGBE_E56_H_
#define _TXGBE_E56_H_

#include "txgbe_type.h"
#include "txgbe_hw.h"
#include "txgbe_osdep.h"
#include "txgbe_phy.h"

#define EPHY_RREG(REG) \
	do {\
		rdata = 0; \
		rdata = rd32_ephy(hw, REG##_ADDR); \
	} while (0)

#define EPCS_RREG(REG) \
	do {\
		rdata = 0; \
		rdata = rd32_epcs(hw, REG##_ADDR); \
	} while (0)

#define EPHY_WREG(REG)	wr32_ephy(hw, REG##_ADDR, rdata)
#define EPCS_WREG(REG)	wr32_epcs(hw, REG##_ADDR, rdata)

#define txgbe_e56_ephy_config(reg, field, val) \
	do { \
		EPHY_RREG(reg); \
		EPHY_XFLD(reg, field) = (val); \
		EPHY_WREG(reg); \
	} while (0)

#define txgbe_e56_epcs_config(reg, field, val) \
	do { \
		EPCS_RREG(reg); \
		EPCS_XFLD(reg, field) = (val); \
		EPCS_WREG(reg); \
	} while (0)

/*
 * LAN GPIO define for SFP+ module
 * -- Fields
 */
#define SFP1_RS0		5, 5
#define SFP1_RS1		4, 4
#define SFP1_RX_LOS		3, 3
#define SFP1_MOD_ABS		2, 2
#define SFP1_TX_DISABLE		1, 1
#define SFP1_TX_FAULT		0, 0

#define EPHY_XFLD(REG, FLD)	(((REG *)&rdata)->FLD)
#define EPCS_XFLD(REG, FLD)	(((REG *)&rdata)->FLD)

typedef union {
	struct {
		u32 ana_refclk_buf_daisy_en_i : 1;
		u32 ana_refclk_buf_pad_en_i : 1;
		u32 ana_vddinoff_dcore_dig_o : 1;
		u32 ana_lcpll_en_clkout_hf_left_top_i : 1;
		u32 ana_lcpll_en_clkout_hf_right_top_i : 1;
		u32 ana_lcpll_en_clkout_hf_left_bot_i : 1;
		u32 ana_lcpll_en_clkout_hf_right_bot_i : 1;
		u32 ana_lcpll_en_clkout_lf_left_top_i : 1;
		u32 ana_lcpll_en_clkout_lf_right_top_i : 1;
		u32 ana_lcpll_en_clkout_lf_left_bot_i : 1;
		u32 ana_lcpll_en_clkout_lf_right_bot_i : 1;
		u32 ana_bg_en_i : 1;
		u32 ana_en_rescal_i : 1;
		u32 ana_rescal_comp_o : 1;
		u32 ana_en_ldo_core_i : 1;
		u32 ana_lcpll_hf_en_bias_i : 1;
		u32 ana_lcpll_hf_en_loop_i : 1;
		u32 ana_lcpll_hf_en_cp_i : 1;
		u32 ana_lcpll_hf_set_lpf_i : 1;
		u32 ana_lcpll_hf_en_vco_i : 1;
		u32 ana_lcpll_hf_vco_amp_status_o : 1;
		u32 ana_lcpll_hf_en_odiv_i : 1;
		u32 ana_lcpll_lf_en_bias_i : 1;
		u32 ana_lcpll_lf_en_loop_i : 1;
		u32 ana_lcpll_lf_en_cp_i : 1;
		u32 ana_lcpll_lf_set_lpf_i : 1;
		u32 ana_lcpll_lf_en_vco_i : 1;
		u32 ana_lcpll_lf_vco_amp_status_o : 1;
		u32 ana_lcpll_lf_en_odiv_i : 1;
		u32 ana_lcpll_hf_refclk_select_i : 1;
		u32 ana_lcpll_lf_refclk_select_i : 1;
		u32 rsvd0 : 1;
	};
	u32 reg;
} E56G_CMS_ANA_OVRDVAL_0;

/* AMLITE ETH PHY Registers */
#define VR_PCS_DIG_CTRL1                        0x38000
#define SR_PCS_CTRL1                            0x30000
#define SR_PCS_CTRL2                            0x30007
#define SR_PMA_CTRL2                            0x10007
#define VR_PCS_DIG_CTRL3                        0x38003
#define VR_PMA_CTRL3                            0x180a8
#define VR_PMA_CTRL4                            0x180a9
#define SR_PMA_RS_FEC_CTRL                      0x100c8
#define CMS_ANA_OVRDEN0                         0xca4
#define ANA_OVRDEN0                             0xca4
#define ANA_OVRDEN1                             0xca8
#define ANA_OVRDVAL0                            0xcb0
#define ANA_OVRDVAL5                            0xcc4
#define OSC_CAL_N_CDR4                          0x14
#define PLL0_CFG0                               0xc10
#define PLL0_CFG2                               0xc18
#define PLL0_DIV_CFG0                           0xc1c
#define PLL1_CFG0                               0xc48
#define PLL1_CFG2                               0xc50
#define CMS_PIN_OVRDEN0                         0xc8c
#define CMS_PIN_OVRDVAL0                        0xc94
#define DATAPATH_CFG0                           0x142c
#define DATAPATH_CFG1                           0x1430
#define AN_CFG1                                 0x1438
#define SPARE52                                 0x16fc
#define RXS_CFG0                                0x000
#define PMD_CFG0                                0x1400
#define SR_PCS_STS1                             0x30001
#define PMD_CTRL_FSM_TX_STAT0                   0x14dc
#define CMS_ANA_OVRDEN0                         0xca4
#define CMS_ANA_OVRDEN1                         0xca8
#define CMS_ANA_OVRDVAL2                        0xcb8
#define CMS_ANA_OVRDVAL4                        0xcc0
#define CMS_ANA_OVRDVAL5                        0xcc4
#define CMS_ANA_OVRDVAL7                        0xccc
#define CMS_ANA_OVRDVAL9                        0xcd4
#define CMS_ANA_OVRDVAL10                       0xcd8

#define TXS_TXS_CFG1                            0x804
#define TXS_WKUP_CNT                            0x808
#define TXS_PIN_OVRDEN0                         0x80c
#define TXS_PIN_OVRDVAL6                        0x82c
#define TXS_ANA_OVRDVAL1                        0x854

#define E56PHY_CMS_BASE_ADDR                    0x0C00

#define E56PHY_CMS_PIN_OVRDEN_0_ADDR   (E56PHY_CMS_BASE_ADDR + 0x8C)
#define E56PHY_CMS_PIN_OVRDEN_0_OVRD_EN_PLL0_TX_SIGNAL_TYPE_I 12, 12

#define E56PHY_CMS_PIN_OVRDVAL_0_ADDR   (E56PHY_CMS_BASE_ADDR + 0x94)
#define E56PHY_CMS_PIN_OVRDVAL_0_INT_PLL0_TX_SIGNAL_TYPE_I 10, 10

#define E56PHY_CMS_ANA_OVRDEN_0_ADDR   (E56PHY_CMS_BASE_ADDR + 0xA4)

#define E56PHY_CMS_ANA_OVRDEN_0_OVRD_EN_ANA_LCPLL_HF_VCO_SWING_CTRL_I 29, 29

#define E56PHY_CMS_ANA_OVRDEN_1_ADDR   (E56PHY_CMS_BASE_ADDR + 0xA8)
#define E56PHY_CMS_ANA_OVRDEN_1_OVRD_EN_ANA_LCPLL_HF_TEST_IN_I 4, 4

#define E56PHY_CMS_ANA_OVRDVAL_2_ADDR   (E56PHY_CMS_BASE_ADDR + 0xB8)

#define E56PHY_CMS_ANA_OVRDVAL_2_ANA_LCPLL_HF_VCO_SWING_CTRL_I 31, 28

#define E56PHY_CMS_ANA_OVRDVAL_4_ADDR   (E56PHY_CMS_BASE_ADDR + 0xC0)

#define E56PHY_TXS_BASE_ADDR                    0x0800
#define E56PHY_TXS1_BASE_ADDR                   0x0900
#define E56PHY_TXS2_BASE_ADDR                   0x0A00
#define E56PHY_TXS3_BASE_ADDR                   0x0B00
#define E56PHY_TXS_OFFSET                       0x0100

#define E56PHY_PMD_RX_OFFSET                    0x02C

#define E56PHY_TXS_TXS_CFG_1_ADDR   (E56PHY_TXS_BASE_ADDR + 0x04)
#define E56PHY_TXS_TXS_CFG_1_ADAPTATION_WAIT_CNT_X256 7, 4
#define E56PHY_TXS_WKUP_CNT_ADDR   (E56PHY_TXS_BASE_ADDR + 0x08)
#define E56PHY_TXS_WKUP_CNTLDO_WKUP_CNT_X32 7, 0
#define E56PHY_TXS_WKUP_CNTDCC_WKUP_CNT_X32 15, 8

#define E56PHY_TXS_PIN_OVRDEN_0_ADDR   (E56PHY_TXS_BASE_ADDR + 0x0C)
#define E56PHY_TXS_PIN_OVRDEN_0_OVRD_EN_TX0_EFUSE_BITS_I 28, 28

#define E56PHY_TXS_PIN_OVRDVAL_6_ADDR   (E56PHY_TXS_BASE_ADDR + 0x2C)

#define E56PHY_TXS_ANA_OVRDVAL_1_ADDR   (E56PHY_TXS_BASE_ADDR + 0x54)
#define E56PHY_TXS_ANA_OVRDVAL_1_ANA_TEST_DAC_I 23, 8

#define E56PHY_TXS_ANA_OVRDEN_0_ADDR   (E56PHY_TXS_BASE_ADDR + 0x44)
#define E56PHY_TXS_ANA_OVRDEN_0_OVRD_EN_ANA_TEST_DAC_I 13, 13

#define E56PHY_RXS_BASE_ADDR   0x0000
#define E56PHY_RXS1_BASE_ADDR  0x0200
#define E56PHY_RXS2_BASE_ADDR  0x0400
#define E56PHY_RXS3_BASE_ADDR  0x0600
#define E56PHY_RXS_OFFSET      0x0200

#define E56PHY_RXS_RXS_CFG_0_ADDR   (E56PHY_RXS_BASE_ADDR + 0x000)
#define E56PHY_RXS_RXS_CFG_0_DSER_DATA_SEL 1, 1
#define E56PHY_RXS_RXS_CFG_0_TRAIN_CLK_GATE_BYPASS_EN 17, 4

#define E56PHY_RXS_OSC_CAL_N_CDR_1_ADDR   (E56PHY_RXS_BASE_ADDR + 0x008)
#define E56PHY_RXS_OSC_CAL_N_CDR_1_PREDIV1 15, 0
#define E56PHY_RXS_OSC_CAL_N_CDR_1_PREDIV1_LSB 0
#define E56PHY_RXS_OSC_CAL_N_CDR_1_TARGET_CNT1 31, 16
#define E56PHY_RXS_OSC_CAL_N_CDR_1_TARGET_CNT1_LSB 16

#define E56PHY_RXS_OSC_CAL_N_CDR_4_ADDR   (E56PHY_RXS_BASE_ADDR + 0x014)
#define E56PHY_RXS_OSC_CAL_N_CDR_4_OSC_RANGE_SEL1 3, 2
#define E56PHY_RXS_OSC_CAL_N_CDR_4_VCO_CODE_INIT 18, 8
#define E56PHY_RXS_OSC_CAL_N_CDR_4_OSC_CURRENT_BOOST_EN1 21, 21
#define E56PHY_RXS_OSC_CAL_N_CDR_4_BBCDR_CURRENT_BOOST1 27, 26

#define E56PHY_RXS_OSC_CAL_N_CDR_5_ADDR   (E56PHY_RXS_BASE_ADDR + 0x018)
#define E56PHY_RXS_OSC_CAL_N_CDR_5_SDM_WIDTH 3, 2
#define E56PHY_RXS_OSC_CAL_N_CDR_5_BB_CDR_PROP_STEP_PRELOCK 15, 12
#define E56PHY_RXS_OSC_CAL_N_CDR_5_BB_CDR_PROP_STEP_POSTLOCK 19, 16
#define E56PHY_RXS_OSC_CAL_N_CDR_5_BB_CDR_GAIN_CTRL_POSTLOCK 23, 20
#define E56PHY_RXS_OSC_CAL_N_CDR_5_BB_CDR_GAIN_CTRL_PRELOCK 27, 24
#define E56PHY_RXS_OSC_CAL_N_CDR_5_BBCDR_RDY_CNT 30, 28

#define E56PHY_RXS_OSC_CAL_N_CDR_6_ADDR   (E56PHY_RXS_BASE_ADDR + 0x01C)
#define E56PHY_RXS_OSC_CAL_N_CDR_6_PI_GAIN_CTRL_PRELOCK 3, 0
#define E56PHY_RXS_OSC_CAL_N_CDR_6_PI_GAIN_CTRL_POSTLOCK 7, 4

#define E56PHY_RXS_INTL_CONFIG_0_ADDR   (E56PHY_RXS_BASE_ADDR + 0x020)
#define E56PHY_RXS_INTL_CONFIG_0_ADC_INTL2SLICE_DELAY1 31, 16

#define E56PHY_RXS_INTL_CONFIG_2_ADDR   (E56PHY_RXS_BASE_ADDR + 0x028)
#define E56PHY_RXS_INTL_CONFIG_2_INTERLEAVER_HBW_DISABLE1 1, 1

#define E56PHY_RXS_TXFFE_TRAINING_0_ADDR   (E56PHY_RXS_BASE_ADDR + 0x02C)
#define E56PHY_RXS_TXFFE_TRAINING_0_ADC_DATA_PEAK_LTH 18, 12
#define E56PHY_RXS_TXFFE_TRAINING_0_ADC_DATA_PEAK_UTH 26, 20

#define E56PHY_RXS_TXFFE_TRAINING_1_ADDR   (E56PHY_RXS_BASE_ADDR + 0x030)
#define E56PHY_RXS_TXFFE_TRAINING_1_C1_LTH 8, 0
#define E56PHY_RXS_TXFFE_TRAINING_1_C1_UTH 20, 12

#define E56PHY_RXS_TXFFE_TRAINING_2_ADDR   (E56PHY_RXS_BASE_ADDR + 0x034)
#define E56PHY_RXS_TXFFE_TRAINING_2_CM1_LTH 8, 0
#define E56PHY_RXS_TXFFE_TRAINING_2_CM1_UTH 20, 12

#define E56PHY_RXS_TXFFE_TRAINING_3_ADDR   (E56PHY_RXS_BASE_ADDR + 0x038)
#define E56PHY_RXS_TXFFE_TRAINING_3_CM2_LTH 8, 0
#define E56PHY_RXS_TXFFE_TRAINING_3_CM2_UTH 20, 12
#define E56PHY_RXS_TXFFE_TRAINING_3_TXFFE_TRAIN_MOD_TYPE 26, 21

#define E56PHY_RXS_VGA_TRAINING_0_ADDR   (E56PHY_RXS_BASE_ADDR + 0x04C)
#define E56PHY_RXS_VGA_TRAINING_0_VGA_TARGET 18, 12

#define E56PHY_RXS_VGA_TRAINING_1_ADDR   (E56PHY_RXS_BASE_ADDR + 0x050)
#define E56PHY_RXS_VGA_TRAINING_1_VGA1_CODE_INIT0 4, 0
#define E56PHY_RXS_VGA_TRAINING_1_VGA2_CODE_INIT0 12, 8
#define E56PHY_RXS_VGA_TRAINING_1_VGA1_CODE_INIT123 20, 16
#define E56PHY_RXS_VGA_TRAINING_1_VGA2_CODE_INIT123 28, 24

#define E56PHY_RXS_CTLE_TRAINING_0_ADDR   (E56PHY_RXS_BASE_ADDR + 0x054)
#define E56PHY_RXS_CTLE_TRAINING_0_CTLE_CODE_INIT0 24, 20
#define E56PHY_RXS_CTLE_TRAINING_0_CTLE_CODE_INIT123 31, 27

#define E56PHY_RXS_CTLE_TRAINING_1_ADDR   (E56PHY_RXS_BASE_ADDR + 0x058)
#define E56PHY_RXS_CTLE_TRAINING_1_LFEQ_LUT 24, 0

#define E56PHY_RXS_CTLE_TRAINING_2_ADDR   (E56PHY_RXS_BASE_ADDR + 0x05C)
#define E56PHY_RXS_CTLE_TRAINING_2_ISI_TH_FRAC_P1 5, 0
#define E56PHY_RXS_CTLE_TRAINING_2_ISI_TH_FRAC_P2 13, 8
#define E56PHY_RXS_CTLE_TRAINING_2_ISI_TH_FRAC_P3 21, 16

#define E56PHY_RXS_CTLE_TRAINING_3_ADDR   (E56PHY_RXS_BASE_ADDR + 0x060)
#define E56PHY_RXS_CTLE_TRAINING_3_TAP_WEIGHT_P1 9, 8
#define E56PHY_RXS_CTLE_TRAINING_3_TAP_WEIGHT_P2 11, 10
#define E56PHY_RXS_CTLE_TRAINING_3_TAP_WEIGHT_P3 13, 12

#define E56PHY_RXS_OFFSET_N_GAIN_CAL_0_ADDR   (E56PHY_RXS_BASE_ADDR + 0x064)
#define E56PHY_RXS_OFFSET_N_GAIN_CAL_0_ADC_SLICE_DATA_AVG_CNT 5, 4
#define E56PHY_RXS_OFFSET_N_GAIN_CAL_0_ADC_DATA_AVG_CNT 9, 8
#define E56PHY_RXS_OFFSET_N_GAIN_CAL_0_FE_OFFSET_DAC_CLK_CNT_X8 31, 28

#define E56PHY_RXS_OFFSET_N_GAIN_CAL_1_ADDR   (E56PHY_RXS_BASE_ADDR + 0x068)
#define E56PHY_RXS_OFFSET_N_GAIN_CAL_1_SAMP_ADAPT_CFG 31, 28

#define E56PHY_RXS_FFE_TRAINING_0_ADDR   (E56PHY_RXS_BASE_ADDR + 0x070)
#define E56PHY_RXS_FFE_TRAINING_0_FFE_TAP_EN 23, 8

#define E56PHY_RXS_IDLE_DETECT_1_ADDR   (E56PHY_RXS_BASE_ADDR + 0x088)
#define E56PHY_RXS_IDLE_DETECT_1_IDLE_TH_ADC_PEAK_MAX 22, 16
#define E56PHY_RXS_IDLE_DETECT_1_IDLE_TH_ADC_PEAK_MIN 30, 24

#define E56PHY_RXS_ANA_OVRDEN_0_ADDR   (E56PHY_RXS_BASE_ADDR + 0x08C)
#define E56PHY_RXS_ANA_OVRDEN_0_OVRD_EN_ANA_EN_RTERM_I 0, 0
#define E56PHY_RXS_ANA_OVRDEN_0_OVRD_EN_ANA_TRIM_RTERM_I 1, 1
#define E56PHY_RXS_ANA_OVRDEN_0_OVRD_EN_ANA_BBCDR_OSC_RANGE_SEL_I 29, 29

#define E56PHY_RXS_ANA_OVRDEN_1_ADDR   (E56PHY_RXS_BASE_ADDR + 0x090)
#define E56PHY_RXS_ANA_OVRDEN_1_OVRD_EN_ANA_BBCDR_VCOFILT_BYP_I 0, 0
#define E56PHY_RXS_ANA_OVRDEN_1_OVRD_EN_ANA_TEST_BBCDR_I 9, 9

#define E56PHY_RXS_ANA_OVRDEN_3_ADDR   (E56PHY_RXS_BASE_ADDR + 0x098)
#define E56PHY_RXS_ANA_OVRDEN_3_OVRD_EN_ANA_ANABS_CONFIG_I 15, 15
#define E56PHY_RXS_ANA_OVRDEN_3_OVRD_EN_ANA_VGA2_BOOST_CSTM_I 25, 25

#define E56PHY_RXS_ANA_OVRDEN_4_ADDR   (E56PHY_RXS_BASE_ADDR + 0x09C)
#define E56PHY_RXS_ANA_OVRDVAL_0_ADDR   (E56PHY_RXS_BASE_ADDR + 0x0A0)
#define E56PHY_RXS_ANA_OVRDVAL_0_ANA_EN_RTERM_I 0, 0

#define E56PHY_RXS_ANA_OVRDVAL_6_ADDR   (E56PHY_RXS_BASE_ADDR + 0x0B8)
#define E56PHY_RXS_ANA_OVRDVAL_14_ADDR   (E56PHY_RXS_BASE_ADDR + 0x0D8)
#define E56PHY_RXS_ANA_OVRDVAL_15_ADDR   (E56PHY_RXS_BASE_ADDR + 0x0DC)
#define E56PHY_RXS_ANA_OVRDVAL_17_ADDR   (E56PHY_RXS_BASE_ADDR + 0x0E4)
#define E56PHY_RXS_ANA_OVRDVAL_17_ANA_VGA2_BOOST_CSTM_I 18, 16

#define E56PHY_RXS_EYE_SCAN_1_ADDR   (E56PHY_RXS_BASE_ADDR + 0x1A4)
#define E56PHY_RXS_EYE_SCAN_1_EYE_SCAN_REF_TIMER 31, 0

#define E56PHY_RXS_ANA_OVRDVAL_5_ADDR   (E56PHY_RXS_BASE_ADDR + 0x0B4)
#define E56PHY_RXS_ANA_OVRDVAL_5_ANA_BBCDR_OSC_RANGE_SEL_I 1, 0

#define E56PHY_RXS_RINGO_0_ADDR   (E56PHY_RXS_BASE_ADDR + 0x1FC)

#define E56PHY_PMD_BASE_ADDR  0x1400
#define E56PHY_PMD_CFG_0_ADDR   (E56PHY_PMD_BASE_ADDR + 0x000)
#define E56PHY_PMD_CFG_0_RX_EN_CFG 19, 16

#define E56PHY_PMD_CFG_3_ADDR   (E56PHY_PMD_BASE_ADDR + 0x00C)
#define E56PHY_PMD_CFG_3_CTRL_FSM_TIMEOUT_X64K 31, 24
#define E56PHY_PMD_CFG_4_ADDR   (E56PHY_PMD_BASE_ADDR + 0x010)
#define E56PHY_PMD_CFG_4_TRAIN_DC_ON_PERIOD_X64K 7, 0
#define E56PHY_PMD_CFG_4_TRAIN_DC_PERIOD_X512K 15, 8
#define E56PHY_PMD_CFG_5_ADDR   (E56PHY_PMD_BASE_ADDR + 0x014)
#define E56PHY_PMD_CFG_5_USE_RECENT_MARKER_OFFSET 12, 12
#define E56PHY_CTRL_FSM_CFG_0_ADDR   (E56PHY_PMD_BASE_ADDR + 0x040)
#define E56PHY_CTRL_FSM_CFG_0_CONT_ON_ADC_OFST_CAL_ERR 4, 4
#define E56PHY_CTRL_FSM_CFG_0_CONT_ON_ADC_GAIN_CAL_ERR 5, 5
#define E56PHY_CTRL_FSM_CFG_0_DO_RX_ADC_OFST_CAL 9, 8
#define E56PHY_CTRL_FSM_CFG_0_RX_ERR_ACTION_EN 31, 24

#define E56PHY_CTRL_FSM_CFG_1_ADDR   (E56PHY_PMD_BASE_ADDR + 0x044)
#define E56PHY_CTRL_FSM_CFG_1_TRAIN_ST0_WAIT_CNT_X4096 7, 0
#define E56PHY_CTRL_FSM_CFG_1_TRAIN_ST1_WAIT_CNT_X4096 15, 8
#define E56PHY_CTRL_FSM_CFG_1_TRAIN_ST2_WAIT_CNT_X4096 23, 16
#define E56PHY_CTRL_FSM_CFG_1_TRAIN_ST3_WAIT_CNT_X4096 31, 24

#define E56PHY_CTRL_FSM_CFG_2_ADDR   (E56PHY_PMD_BASE_ADDR + 0x048)
#define E56PHY_CTRL_FSM_CFG_2_TRAIN_ST4_WAIT_CNT_X4096 7, 0
#define E56PHY_CTRL_FSM_CFG_2_TRAIN_ST5_WAIT_CNT_X4096 15, 8
#define E56PHY_CTRL_FSM_CFG_2_TRAIN_ST6_WAIT_CNT_X4096 23, 16
#define E56PHY_CTRL_FSM_CFG_2_TRAIN_ST7_WAIT_CNT_X4096 31, 24

#define E56PHY_CTRL_FSM_CFG_3_ADDR   (E56PHY_PMD_BASE_ADDR + 0x04C)
#define E56PHY_CTRL_FSM_CFG_3_TRAIN_ST8_WAIT_CNT_X4096 7, 0

#define E56PHY_CTRL_FSM_CFG_3_TRAIN_ST9_WAIT_CNT_X4096 15, 8
#define E56PHY_CTRL_FSM_CFG_3_TRAIN_ST10_WAIT_CNT_X4096 23, 16
#define E56PHY_CTRL_FSM_CFG_3_TRAIN_ST11_WAIT_CNT_X4096 31, 24

#define E56PHY_CTRL_FSM_CFG_4_ADDR   (E56PHY_PMD_BASE_ADDR + 0x050)
#define E56PHY_CTRL_FSM_CFG_4_TRAIN_ST12_WAIT_CNT_X4096 7, 0
#define E56PHY_CTRL_FSM_CFG_4_TRAIN_ST13_WAIT_CNT_X4096 15, 8
#define E56PHY_CTRL_FSM_CFG_4_TRAIN_ST14_WAIT_CNT_X4096 23, 16
#define E56PHY_CTRL_FSM_CFG_4_TRAIN_ST15_WAIT_CNT_X4096 31, 24

#define E56PHY_CTRL_FSM_CFG_7_ADDR   (E56PHY_PMD_BASE_ADDR + 0x05C)
#define E56PHY_CTRL_FSM_CFG_7_TRAIN_ST4_EN 15, 0
#define E56PHY_CTRL_FSM_CFG_7_TRAIN_ST5_EN 31, 16

#define E56PHY_CTRL_FSM_CFG_8_ADDR   (E56PHY_PMD_BASE_ADDR + 0x060)
#define E56PHY_CTRL_FSM_CFG_8_TRAIN_ST7_EN 31, 16

#define E56PHY_CTRL_FSM_CFG_12_ADDR   (E56PHY_PMD_BASE_ADDR + 0x070)
#define E56PHY_CTRL_FSM_CFG_12_TRAIN_ST15_EN 31, 16

#define E56PHY_CTRL_FSM_CFG_13_ADDR   (E56PHY_PMD_BASE_ADDR + 0x074)
#define E56PHY_CTRL_FSM_CFG_13_TRAIN_ST0_DONE_EN 15, 0
#define E56PHY_CTRL_FSM_CFG_13_TRAIN_ST1_DONE_EN 31, 16

#define E56PHY_CTRL_FSM_CFG_14_ADDR   (E56PHY_PMD_BASE_ADDR + 0x078)
#define E56PHY_CTRL_FSM_CFG_14_TRAIN_ST3_DONE_EN 31, 16

#define E56PHY_CTRL_FSM_CFG_15_ADDR   (E56PHY_PMD_BASE_ADDR + 0x07C)
#define E56PHY_CTRL_FSM_CFG_15_TRAIN_ST4_DONE_EN 15, 0

#define E56PHY_CTRL_FSM_CFG_17_ADDR   (E56PHY_PMD_BASE_ADDR + 0x084)
#define E56PHY_CTRL_FSM_CFG_17_TRAIN_ST8_DONE_EN 15, 0

#define E56PHY_CTRL_FSM_CFG_18_ADDR   (E56PHY_PMD_BASE_ADDR + 0x088)
#define E56PHY_CTRL_FSM_CFG_18_TRAIN_ST10_DONE_EN 15, 0

#define E56PHY_CTRL_FSM_CFG_29_ADDR   (E56PHY_PMD_BASE_ADDR + 0x0B4)
#define E56PHY_CTRL_FSM_CFG_29_TRAIN_ST15_DC_EN 31, 16

#define E56PHY_CTRL_FSM_CFG_33_ADDR   (E56PHY_PMD_BASE_ADDR + 0x0C4)
#define E56PHY_CTRL_FSM_CFG_33_TRAIN0_RATE_SEL 15, 0
#define E56PHY_CTRL_FSM_CFG_33_TRAIN1_RATE_SEL 31, 16

#define E56PHY_CTRL_FSM_CFG_34_ADDR   (E56PHY_PMD_BASE_ADDR + 0x0C8)
#define E56PHY_CTRL_FSM_CFG_34_TRAIN2_RATE_SEL 15, 0
#define E56PHY_CTRL_FSM_CFG_34_TRAIN3_RATE_SEL 31, 16

#define E56PHY_CTRL_FSM_RX_STAT_0_ADDR   (E56PHY_PMD_BASE_ADDR + 0x0FC)
#define E56PHY_RXS0_OVRDEN_0_ADDR   (E56PHY_PMD_BASE_ADDR + 0x130)
#define E56PHY_RXS0_OVRDEN_0_OVRD_EN_RXS0_RX0_SAMP_CAL_DONE_O 27, 27

#define E56PHY_RXS0_OVRDEN_1_ADDR   (E56PHY_PMD_BASE_ADDR + 0x134)
#define E56PHY_RXS0_OVRDEN_1_OVRD_EN_RXS0_RX0_VGA_TRAIN_EN_I 14, 14
#define E56PHY_RXS0_OVRDEN_1_OVRD_EN_RXS0_RX0_CTLE_TRAIN_EN_I 16, 16
#define E56PHY_RXS0_OVRDEN_1_OVRD_EN_RXS0_RX0_CDR_EN_I 18, 18
#define E56PHY_RXS0_OVRDEN_1_OVRD_EN_RXS0_RX0_ADC_INTL_CAL_EN_I 23, 23
#define E56PHY_RXS0_OVRDEN_1_OVRD_EN_RXS0_RX0_ADC_INTL_CAL_DONE_O 24, 24
#define E56PHY_RXS0_OVRDEN_1_OVRD_EN_RXS0_RX0_ADC_INTL_CAL_DONE_O_LSB 24

#define E56PHY_RXS0_OVRDEN_2_ADDR   (E56PHY_PMD_BASE_ADDR + 0x138)
#define E56PHY_RXS0_OVRDEN_2_OVRD_EN_RXS0_RX0_ADC_OFST_ADAPT_EN_I 0, 0
#define E56PHY_RXS0_OVRDEN_2_OVRD_EN_RXS0_RX0_ADC_GAIN_ADAPT_EN_I 3, 3
#define E56PHY_RXS0_OVRDEN_2_OVRD_EN_RXS0_RX0_ADC_INTL_ADAPT_EN_I 6, 6

#define E56PHY_RXS0_OVRDVAL_0_ADDR   (E56PHY_PMD_BASE_ADDR + 0x140)
#define E56PHY_RXS0_OVRDVAL_0_RXS0_RX0_SAMP_CAL_DONE_O 22, 22

#define E56PHY_RXS0_OVRDVAL_1_ADDR   (E56PHY_PMD_BASE_ADDR + 0x144)
#define E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_VGA_TRAIN_EN_I 7, 7
#define E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_CTLE_TRAIN_EN_I 9, 9
#define E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_CDR_EN_I 11, 11
#define E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_INTL_CAL_EN_I 16, 16
#define E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_INTL_CAL_DONE_O 17, 17
#define E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_INTL_CAL_DONE_O_LSB 17
#define E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_OFST_ADAPT_EN_I 25, 25
#define E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_GAIN_ADAPT_EN_I 28, 28
#define E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_INTL_ADAPT_EN_I 31, 31

#define E56PHY_INTR_0_IDLE_ENTRY1              0x10000000
#define E56PHY_INTR_0_ADDR   (E56PHY_PMD_BASE_ADDR + 0x1EC)
#define E56PHY_INTR_0_ENABLE_ADDR   (E56PHY_PMD_BASE_ADDR + 0x1E0)

#define E56PHY_INTR_1_IDLE_EXIT1               0x1
#define E56PHY_INTR_1_ADDR   (E56PHY_PMD_BASE_ADDR + 0x1F0)
#define E56PHY_INTR_1_ENABLE_ADDR   (E56PHY_PMD_BASE_ADDR + 0x1E4)

#define E56PHY_KRT_TFSM_CFG_ADDR   (E56PHY_PMD_BASE_ADDR + 0x2B8)
#define E56PHY_KRT_TFSM_CFGKRT_TFSM_MAX_WAIT_TIMER_X1000K 7, 0
#define E56PHY_KRT_TFSM_CFGKRT_TFSM_MAX_WAIT_TIMER_X8000K 15, 8
#define E56PHY_KRT_TFSM_CFGKRT_TFSM_HOLDOFF_TIMER_X256K 23, 16

#define E56PHY_FETX_FFE_TRAIN_CFG_0_ADDR   (E56PHY_PMD_BASE_ADDR + 0x2BC)
#define E56PHY_FETX_FFE_TRAIN_CFG_0_KRT_FETX_INIT_FFE_CFG_2 9, 8
#define E56PHY_FETX_FFE_TRAIN_CFG_0_KRT_FETX_INIT_FFE_CFG_3 13, 12

/* PHYINIT_TIMEOUT, a loop iteration counter, not a fixed time unit.
 * The actual timeout duration depends on the usec_delay() or msleep()
 * value used inside the polling loop. For example, when usec_delay(500)
 * is used, the total timeout is approximately PHYINIT_TIMEOUT * 500 us.
 */
#define PHYINIT_TIMEOUT 1000

#define E56G__BASEADDR 0x0

typedef union {
	struct {
		u32 ana_lcpll_lf_vco_swing_ctrl_i : 4;
		u32 ana_lcpll_lf_lpf_setcode_calib_i : 5;
		u32 rsvd0 : 3;
		u32 ana_lcpll_lf_vco_coarse_bin_i : 5;
		u32 rsvd1 : 3;
		u32 ana_lcpll_lf_vco_fine_therm_i : 8;
		u32 ana_lcpll_lf_clkout_fb_ctrl_i : 2;
		u32 rsvd2 : 2;
	};
	u32 reg;
} E56G_CMS_ANA_OVRDVAL_7;
#define E56G_CMS_ANA_OVRDVAL_7_ADDR                   (E56G__BASEADDR + 0xccc)

typedef union {
	struct {
		u32 ovrd_en_ana_lcpll_hf_vco_amp_status_o : 1;
		u32 ovrd_en_ana_lcpll_hf_clkout_fb_ctrl_i : 1;
		u32 ovrd_en_ana_lcpll_hf_clkdiv_ctrl_i : 1;
		u32 ovrd_en_ana_lcpll_hf_en_odiv_i : 1;
		u32 ovrd_en_ana_lcpll_hf_test_in_i : 1;
		u32 ovrd_en_ana_lcpll_hf_test_out_o : 1;
		u32 ovrd_en_ana_lcpll_lf_en_bias_i : 1;
		u32 ovrd_en_ana_lcpll_lf_en_loop_i : 1;
		u32 ovrd_en_ana_lcpll_lf_en_cp_i : 1;
		u32 ovrd_en_ana_lcpll_lf_icp_base_i : 1;
		u32 ovrd_en_ana_lcpll_lf_icp_fine_i : 1;
		u32 ovrd_en_ana_lcpll_lf_lpf_ctrl_i : 1;
		u32 ovrd_en_ana_lcpll_lf_lpf_setcode_calib_i : 1;
		u32 ovrd_en_ana_lcpll_lf_set_lpf_i : 1;
		u32 ovrd_en_ana_lcpll_lf_en_vco_i : 1;
		u32 ovrd_en_ana_lcpll_lf_vco_sel_i : 1;
		u32 ovrd_en_ana_lcpll_lf_vco_swing_ctrl_i : 1;
		u32 ovrd_en_ana_lcpll_lf_vco_coarse_bin_i : 1;
		u32 ovrd_en_ana_lcpll_lf_vco_fine_therm_i : 1;
		u32 ovrd_en_ana_lcpll_lf_vco_amp_status_o : 1;
		u32 ovrd_en_ana_lcpll_lf_clkout_fb_ctrl_i : 1;
		u32 ovrd_en_ana_lcpll_lf_clkdiv_ctrl_i : 1;
		u32 ovrd_en_ana_lcpll_lf_en_odiv_i : 1;
		u32 ovrd_en_ana_lcpll_lf_test_in_i : 1;
		u32 ovrd_en_ana_lcpll_lf_test_out_o : 1;
		u32 ovrd_en_ana_lcpll_hf_refclk_select_i : 1;
		u32 ovrd_en_ana_lcpll_lf_refclk_select_i : 1;
		u32 ovrd_en_ana_lcpll_hf_clk_ref_sel_i : 1;
		u32 ovrd_en_ana_lcpll_lf_clk_ref_sel_i : 1;
		u32 ovrd_en_ana_test_bias_i : 1;
		u32 ovrd_en_ana_test_slicer_i : 1;
		u32 ovrd_en_ana_test_sampler_i : 1;
	};
	u32 reg;
} E56G_CMS_ANA_OVRDEN_1;

#define E56G_CMS_ANA_OVRDEN_1_ADDR                    (E56G__BASEADDR + 0xca8)

typedef union {
	struct {
		u32 ana_lcpll_lf_test_in_i : 32;
	};
	u32 reg;
} E56G_CMS_ANA_OVRDVAL_9;

#define E56G_CMS_ANA_OVRDVAL_9_ADDR                   (E56G__BASEADDR + 0xcd4)

typedef union {
	struct {
		u32 ovrd_en_ana_bbcdr_vcofilt_byp_i : 1;
		u32 ovrd_en_ana_bbcdr_coarse_i : 1;
		u32 ovrd_en_ana_bbcdr_fine_i : 1;
		u32 ovrd_en_ana_bbcdr_ultrafine_i : 1;
		u32 ovrd_en_ana_en_bbcdr_i : 1;
		u32 ovrd_en_ana_bbcdr_divctrl_i : 1;
		u32 ovrd_en_ana_bbcdr_int_cstm_i : 1;
		u32 ovrd_en_ana_bbcdr_prop_step_i : 1;
		u32 ovrd_en_ana_en_bbcdr_clk_i : 1;
		u32 ovrd_en_ana_test_bbcdr_i : 1;
		u32 ovrd_en_ana_bbcdr_en_elv_cnt_ping0_pong1_i : 1;
		u32 ovrd_en_ana_bbcdr_clrz_elv_cnt_ping_i : 1;
		u32 ovrd_en_ana_bbcdr_clrz_elv_cnt_pong_i : 1;
		u32 ovrd_en_ana_bbcdr_clrz_cnt_sync_i : 1;
		u32 ovrd_en_ana_bbcdr_en_elv_cnt_rd_i : 1;
		u32 ovrd_en_ana_bbcdr_elv_cnt_rdout_0_o : 1;
		u32 ovrd_en_ana_bbcdr_elv_cnt_rdout_90_o : 1;
		u32 ovrd_en_ana_bbcdr_elv_cnt_rdout_180_o : 1;
		u32 ovrd_en_ana_bbcdr_elv_cnt_rdout_270_o : 1;
		u32 ovrd_en_ana_bbcdr_elv_cnt_ping_0_o : 1;
		u32 ovrd_en_ana_bbcdr_elv_cnt_ping_90_o : 1;
		u32 ovrd_en_ana_bbcdr_elv_cnt_ping_180_o : 1;
		u32 ovrd_en_ana_bbcdr_elv_cnt_ping_270_o : 1;
		u32 ovrd_en_ana_bbcdr_elv_cnt_pong_0_o : 1;
		u32 ovrd_en_ana_bbcdr_elv_cnt_pong_90_o : 1;
		u32 ovrd_en_ana_bbcdr_elv_cnt_pong_180_o : 1;
		u32 ovrd_en_ana_bbcdr_elv_cnt_pong_270_o : 1;
		u32 ovrd_en_ana_en_bbcdr_samp_dac_i : 1;
		u32 ovrd_en_ana_bbcdr_dac0_i : 1;
		u32 ovrd_en_ana_bbcdr_dac90_i : 1;
		u32 ovrd_en_ana_vga2_cload_in_cstm_i : 1;
		u32 ovrd_en_ana_intlvr_cut_bw_i : 1;
	};
	u32 reg;
} E56G__RXS0_ANA_OVRDEN_1;

#define E56G__RXS0_ANA_OVRDEN_1_ADDR                    (E56G__BASEADDR + 0x90)

typedef union {
	struct {
		u32 prediv0 : 16;
		u32 target_cnt0 : 16;
	};
	u32 reg;
} E56G_RXS0_OSC_CAL_N_CDR_0;

#define E56G_RXS0_OSC_CAL_N_CDR_0_ADDR                  (E56G__BASEADDR + 0x4)

typedef union {
	struct {
		u32 osc_range_sel0 : 2;
		u32 osc_range_sel1 : 2;
		u32 osc_range_sel2 : 2;
		u32 osc_range_sel3 : 2;
		u32 vco_code_init : 11;
		u32 calibrate_range_sel : 1;
		u32 osc_current_boost_en0 : 1;
		u32 osc_current_boost_en1 : 1;
		u32 osc_current_boost_en2 : 1;
		u32 osc_current_boost_en3 : 1;
		u32 bbcdr_current_boost0 : 2;
		u32 bbcdr_current_boost1 : 2;
		u32 bbcdr_current_boost2 : 2;
		u32 bbcdr_current_boost3 : 2;
	};
	u32 reg;
} E56G_RXS0_OSC_CAL_N_CDR_4;

#define E56G_RXS0_OSC_CAL_N_CDR_4_ADDR                 (E56G__BASEADDR + 0x14)

typedef union {
	struct {
		u32 adc_intl2slice_delay0 : 16;
		u32 adc_intl2slice_delay1 : 16;
	};
	u32 reg;
} E56G_RXS0_INTL_CONFIG_0;

#define E56G_RXS0_INTL_CONFIG_0_ADDR                   (E56G__BASEADDR + 0x20)

typedef union {
	struct {
		u32 interleaver_hbw_disable0 : 1;
		u32 interleaver_hbw_disable1 : 1;
		u32 interleaver_hbw_disable2 : 1;
		u32 interleaver_hbw_disable3 : 1;
		u32 rsvd0 : 28;
	};
	u32 reg;
} E56G_RXS0_INTL_CONFIG_2;

#define E56G_RXS0_INTL_CONFIG_2_ADDR                   (E56G__BASEADDR + 0x28)

typedef union {
	struct {
		u32 ovrd_en_ana_bbcdr_dac180_i : 1;
		u32 ovrd_en_ana_bbcdr_dac270_i : 1;
		u32 ovrd_en_ana_bbcdr_en_samp_cal_cnt_i : 1;
		u32 ovrd_en_ana_bbcdr_clrz_samp_cal_cnt_i : 1;
		u32 ovrd_en_ana_bbcdr_samp_cnt_0_o : 1;
		u32 ovrd_en_ana_bbcdr_samp_cnt_90_o : 1;
		u32 ovrd_en_ana_bbcdr_samp_cnt_180_o : 1;
		u32 ovrd_en_ana_bbcdr_samp_cnt_270_o : 1;
		u32 ovrd_en_ana_en_adcbuf1_i : 1;
		u32 ovrd_en_ana_test_adcbuf1_i : 1;
		u32 ovrd_en_ana_en_adc_clk4ui_i : 1;
		u32 ovrd_en_ana_adc_clk_skew0_i : 1;
		u32 ovrd_en_ana_adc_clk_skew90_i : 1;
		u32 ovrd_en_ana_adc_clk_skew180_i : 1;
		u32 ovrd_en_ana_adc_clk_skew270_i : 1;
		u32 ovrd_en_ana_adc_update_skew_i : 1;
		u32 ovrd_en_ana_en_adc_pi_i : 1;
		u32 ovrd_en_ana_adc_pictrl_quad_i : 1;
		u32 ovrd_en_ana_adc_pctrl_code_i : 1;
		u32 ovrd_en_ana_adc_clkdiv_i : 1;
		u32 ovrd_en_ana_test_adc_clkgen_i : 1;
		u32 ovrd_en_ana_en_adc_i : 1;
		u32 ovrd_en_ana_en_adc_vref_i : 1;
		u32 ovrd_en_ana_vref_cnfg_i : 1;
		u32 ovrd_en_ana_adc_data_cstm_o : 1;
		u32 ovrd_en_ana_en_adccal_lpbk_i : 1;
		u32 ovrd_en_ana_sel_adcoffset_cal_i : 1;
		u32 ovrd_en_ana_sel_adcgain_cal_i : 1;
		u32 ovrd_en_ana_adcgain_cal_swing_ctrl_i : 1;
		u32 ovrd_en_ana_adc_gain_i : 1;
		u32 ovrd_en_ana_vga_cload_out_cstm_i : 1;
		u32 ovrd_en_ana_vga2_cload_out_cstm_i : 1;
	};
	u32 reg;
} E56G__RXS0_ANA_OVRDEN_2;

#define E56G__RXS0_ANA_OVRDEN_2_ADDR                    (E56G__BASEADDR + 0x94)

typedef union {
	struct {
		u32 ovrd_en_ana_adc_offset_i         : 1;
		u32 ovrd_en_ana_adc_slice_addr_i     : 1;
		u32 ovrd_en_ana_slice_wr_i           : 1;
		u32 ovrd_en_ana_test_adc_i           : 1;
		u32 ovrd_en_ana_test_adc_o           : 1;
		u32 ovrd_en_ana_spare_o              : 8;
		u32 ovrd_en_ana_sel_lpbk_i           : 1;
		u32 ovrd_en_ana_ana_debug_sel_i      : 1;
		u32 ovrd_en_ana_anabs_config_i       : 1;
		u32 ovrd_en_ana_en_anabs_i           : 1;
		u32 ovrd_en_ana_anabs_rxn_o          : 1;
		u32 ovrd_en_ana_anabs_rxp_o          : 1;
		u32 ovrd_en_ana_dser_clk_en_i        : 1;
		u32 ovrd_en_ana_dser_clk_config_i    : 1;
		u32 ovrd_en_ana_en_mmcdr_clk_obs_i   : 1;
		u32 ovrd_en_ana_skew_coarse0_fine1_i : 1;
		u32 ovrd_en_ana_vddinoff_acore_dig_o : 1;
		u32 ovrd_en_ana_vddinoff_dcore_dig_o : 1;
		u32 ovrd_en_ana_vga2_boost_cstm_i    : 1;
		u32 ovrd_en_ana_adc_sel_vbgr_bias_i  : 1;
		u32 ovrd_en_ana_adc_nbuf_cnfg_i      : 1;
		u32 ovrd_en_ana_adc_pbuf_cnfg_i      : 1;
		u32 rsvd0                            : 3;
	};
	u32 reg;
} E56G__RXS0_ANA_OVRDEN_3;

#define E56G__RXS0_ANA_OVRDEN_3_NUM                                         1
#define E56G__RXS0_ANA_OVRDEN_3_ADDR                    (E56G__BASEADDR + 0x98)

typedef union {
	struct {
		u32 pam4_ab_swap_en          : 1;
		u32 dser_data_sel            : 1;
		u32 signal_type              : 1;
		u32 precode_en               : 1;
		u32 train_clk_gate_bypass_en : 14;
		u32 rsvd0                    : 14;
	};
	u32 reg;
} E56G__RXS0_RXS_CFG_0;

#define E56G__RXS0_RXS_CFG_0_NUM                                            1
#define E56G__RXS0_RXS_CFG_0_ADDR                        (E56G__BASEADDR + 0x0)

typedef union {
	struct {
		u32 restart_training_ln0 : 1;
		u32 training_enable_ln0  : 1;
		u32 restart_training_ln1 : 1;
		u32 training_enable_ln1  : 1;
		u32 restart_training_ln2 : 1;
		u32 training_enable_ln2  : 1;
		u32 restart_training_ln3 : 1;
		u32 training_enable_ln3  : 1;
		u32 rsvd0                : 24;
	};
	u32 reg;
} E56G__PMD_BASER_PMD_CONTROL;

#define E56G__PMD_BASER_PMD_CONTROL_NUM                                     1
#define E56G__PMD_BASER_PMD_CONTROL_ADDR              (E56G__BASEADDR + 0x1640)

typedef union {
	struct {
		u32 rx_to_tx_lpbk_en         : 4;
		u32 sel_wp_pmt_out           : 4;
		u32 sel_wp_pmt_clkout        : 4;
		u32 use_recent_marker_offset : 1;
		u32 interrupt_debug_mode     : 1;
		u32 rsvd0                    : 2;
		u32 tx_ffe_coeff_update      : 4;
		u32 rsvd1                    : 12;
	};
	u32 reg;
} E56G__PMD_PMD_CFG_5;

#define E56G__PMD_PMD_CFG_5_NUM                                             1
#define E56G__PMD_PMD_CFG_5_ADDR                      (E56G__BASEADDR + 0x1414)

typedef union {
	struct {
		u32 soft_reset             : 1;
		u32 pmd_en                 : 1;
		u32 rsvd0                  : 2;
		u32 pll_refclk_sel         : 2;
		u32 rsvd1                  : 2;
		u32 pmd_mode               : 1;
		u32 rsvd2                  : 3;
		u32 tx_en_cfg              : 4;
		u32 rx_en_cfg              : 4;
		u32 pll_en_cfg             : 2;
		u32 rsvd3                  : 2;
		u32 pam4_precode_no_krt_en : 4;
		u32 rsvd4                  : 4;
	};
	u32 reg;
} E56G__PMD_PMD_CFG_0;

#define E56G__PMD_PMD_CFG_0_NUM                                             1
#define E56G__PMD_PMD_CFG_0_ADDR                      (E56G__BASEADDR + 0x1400)

typedef union {
	struct {
		u32 ovrd_en_rxs0_rx0_rstn_i               : 1;
		u32 ovrd_en_rxs0_rx0_bitclk_divctrl_i     : 1;
		u32 ovrd_en_rxs0_rx0_bitclk_rate_i        : 1;
		u32 ovrd_en_rxs0_rx0_symdata_width_i      : 1;
		u32 ovrd_en_rxs0_rx0_symdata_o            : 1;
		u32 ovrd_en_rxs0_rx0_precode_en_i         : 1;
		u32 ovrd_en_rxs0_rx0_signal_type_i        : 1;
		u32 ovrd_en_rxs0_rx0_sync_detect_en_i     : 1;
		u32 ovrd_en_rxs0_rx0_sync_o               : 1;
		u32 ovrd_en_rxs0_rx0_rate_select_i        : 1;
		u32 ovrd_en_rxs0_rx0_rterm_en_i           : 1;
		u32 ovrd_en_rxs0_rx0_bias_en_i            : 1;
		u32 ovrd_en_rxs0_rx0_ldo_en_i             : 1;
		u32 ovrd_en_rxs0_rx0_ldo_rdy_i            : 1;
		u32 ovrd_en_rxs0_rx0_blwc_en_i            : 1;
		u32 ovrd_en_rxs0_rx0_ctle_en_i            : 1;
		u32 ovrd_en_rxs0_rx0_vga_en_i             : 1;
		u32 ovrd_en_rxs0_rx0_osc_sel_i            : 1;
		u32 ovrd_en_rxs0_rx0_osc_en_i             : 1;
		u32 ovrd_en_rxs0_rx0_clkgencdr_en_i       : 1;
		u32 ovrd_en_rxs0_rx0_ctlecdr_en_i         : 1;
		u32 ovrd_en_rxs0_rx0_samp_en_i            : 1;
		u32 ovrd_en_rxs0_rx0_adc_en_i             : 1;
		u32 ovrd_en_rxs0_rx0_osc_cal_en_i         : 1;
		u32 ovrd_en_rxs0_rx0_osc_cal_done_o       : 1;
		u32 ovrd_en_rxs0_rx0_osc_freq_error_o     : 1;
		u32 ovrd_en_rxs0_rx0_samp_cal_en_i        : 1;
		u32 ovrd_en_rxs0_rx0_samp_cal_done_o      : 1;
		u32 ovrd_en_rxs0_rx0_samp_cal_err_o       : 1;
		u32 ovrd_en_rxs0_rx0_adc_ofst_cal_en_i    : 1;
		u32 ovrd_en_rxs0_rx0_adc_ofst_cal_done_o  : 1;
		u32 ovrd_en_rxs0_rx0_adc_ofst_cal_error_o : 1;
	};
	u32 reg;
} E56G__PMD_RXS0_OVRDEN_0;

#define E56G__PMD_RXS0_OVRDEN_0_NUM                                         1
#define E56G__PMD_RXS0_OVRDEN_0_ADDR                  (E56G__BASEADDR + 0x1530)

typedef union {
	struct {
		u32 ovrd_en_rxs0_rx0_sparein_i  : 8;
		u32 ovrd_en_rxs0_rx0_spareout_o : 8;
		u32 rsvd0                       : 16;
	};
	u32 reg;
} E56G__PMD_RXS0_OVRDEN_3;

#define E56G__PMD_RXS0_OVRDEN_3_NUM                                         1
#define E56G__PMD_RXS0_OVRDEN_3_ADDR                  (E56G__BASEADDR + 0x153c)

typedef union {
	struct {
		u32 vco_code_cont_adj_done_ovrd_en : 1;
		u32 dfe_coeffl_ovrd_en             : 1;
		u32 dfe_coeffh_ovrd_en             : 1;
		u32 rsvd0                          : 1;
		u32 top_comp_th_ovrd_en            : 1;
		u32 mid_comp_th_ovrd_en            : 1;
		u32 bot_comp_th_ovrd_en            : 1;
		u32 rsvd1                          : 1;
		u32 level_target_ovrd_en           : 4;
		u32 ffe_coeff_c0to3_ovrd_en        : 4;
		u32 ffe_coeff_c4to7_ovrd_en        : 4;
		u32 ffe_coeff_c8to11_ovrd_en       : 4;
		u32 ffe_coeff_c12to15_ovrd_en      : 4;
		u32 ffe_coeff_update_ovrd_en       : 1;
		u32 rsvd2                          : 3;
	};
	u32 reg;
} E56G__RXS0_DIG_OVRDEN_1;

#define E56G__RXS0_DIG_OVRDEN_1_NUM                                         1
#define E56G__RXS0_DIG_OVRDEN_1_ADDR                   (E56G__BASEADDR + 0x160)

typedef union {
	struct {
		u32 ber_en                            : 1;
		u32 rsvd0                             : 3;
		u32 read_mode_en                      : 1;
		u32 rsvd1                             : 3;
		u32 err_cnt_mode_all0_one1            : 1;
		u32 rsvd2                             : 3;
		u32 init_lfsr_mode_continue0_restart1 : 1;
		u32 rsvd3                             : 3;
		u32 pattern_sel                       : 4;
		u32 rsvd4                             : 12;
	};
	u32 reg;
} E56G__RXS0_DFT_1;

#define E56G__RXS0_DFT_1_NUM                                                1
#define E56G__RXS0_DFT_1_ADDR                   (E56G__BASEADDR + 0xec)

typedef union {
	struct {
		u32 ovrd_en_rxs0_rx0_adc_ofst_adapt_en_i : 1;
		u32 ovrd_en_rxs0_rx0_adc_ofst_adapt_done_o : 1;
		u32 ovrd_en_rxs0_rx0_adc_ofst_adapt_error_o : 1;
		u32 ovrd_en_rxs0_rx0_adc_gain_adapt_en_i : 1;
		u32 ovrd_en_rxs0_rx0_adc_gain_adapt_done_o : 1;
		u32 ovrd_en_rxs0_rx0_adc_gain_adapt_error_o : 1;
		u32 ovrd_en_rxs0_rx0_adc_intl_adapt_en_i : 1;
		u32 ovrd_en_rxs0_rx0_adc_intl_adapt_done_o : 1;
		u32 ovrd_en_rxs0_rx0_adc_intl_adapt_error_o : 1;
		u32 ovrd_en_rxs0_rx0_fe_ofst_adapt_en_i : 1;
		u32 ovrd_en_rxs0_rx0_fe_ofst_adapt_done_o : 1;
		u32 ovrd_en_rxs0_rx0_fe_ofst_adapt_error_o : 1;
		u32 ovrd_en_rxs0_rx0_samp_th_adapt_en_i : 1;
		u32 ovrd_en_rxs0_rx0_samp_th_adapt_done_o : 1;
		u32 ovrd_en_rxs0_rx0_efuse_bits_i : 1;
		u32 ovrd_en_rxs0_rx0_wp_pmt_in_i : 1;
		u32 ovrd_en_rxs0_rx0_wp_pmt_out_o : 1;
		u32 rsvd0 : 15;
	};
	u32 reg;
} E56G__PMD_RXS0_OVRDEN_2;

#define E56G__PMD_RXS0_OVRDEN_2_ADDR                  (E56G__BASEADDR + 0x1538)

typedef union {
	struct {
		u32 ana_bbcdr_osc_range_sel_i : 2;
		u32 rsvd0 : 2;
		u32 ana_bbcdr_coarse_i : 4;
		u32 ana_bbcdr_fine_i : 3;
		u32 rsvd1 : 1;
		u32 ana_bbcdr_ultrafine_i : 3;
		u32 rsvd2 : 1;
		u32 ana_bbcdr_divctrl_i : 2;
		u32 rsvd3 : 2;
		u32 ana_bbcdr_int_cstm_i : 5;
		u32 rsvd4 : 3;
		u32 ana_bbcdr_prop_step_i : 4;
	};
	u32 reg;
} E56G__RXS0_ANA_OVRDVAL_5;

#define E56G__RXS0_ANA_OVRDVAL_5_ADDR                   (E56G__BASEADDR + 0xb4)

typedef union {
	struct {
		u32 ana_adc_pictrl_quad_i : 2;
		u32 rsvd0 : 2;
		u32 ana_adc_clkdiv_i : 2;
		u32 rsvd1 : 2;
		u32 ana_test_adc_clkgen_i : 4;
		u32 ana_vref_cnfg_i : 4;
		u32 ana_adcgain_cal_swing_ctrl_i : 4;
		u32 ana_adc_gain_i : 4;
		u32 ana_adc_offset_i : 4;
		u32 ana_ana_debug_sel_i : 4;
	};
	u32 reg;
} E56G__RXS3_ANA_OVRDVAL_11;

#define E56G__RXS3_ANA_OVRDVAL_11_ADDR                 (E56G__BASEADDR + 0x6cc)

typedef union {
	struct {
		u32 rxs0_rx0_fe_ofst_cal_error_o : 1;
		u32 rxs0_rx0_fom_en_i : 1;
		u32 rxs0_rx0_idle_detect_en_i : 1;
		u32 rxs0_rx0_idle_o : 1;
		u32 rxs0_rx0_txffe_train_en_i : 1;
		u32 rxs0_rx0_txffe_train_enack_o : 1;
		u32 rxs0_rx0_txffe_train_done_o : 1;
		u32 rxs0_rx0_vga_train_en_i : 1;
		u32 rxs0_rx0_vga_train_done_o : 1;
		u32 rxs0_rx0_ctle_train_en_i : 1;
		u32 rxs0_rx0_ctle_train_done_o : 1;
		u32 rxs0_rx0_cdr_en_i : 1;
		u32 rxs0_rx0_cdr_rdy_o : 1;
		u32 rxs0_rx0_ffe_train_en_i : 1;
		u32 rxs0_rx0_ffe_train_done_o : 1;
		u32 rxs0_rx0_mmpd_en_i : 1;
		u32 rxs0_rx0_adc_intl_cal_en_i : 1;
		u32 rxs0_rx0_adc_intl_cal_done_o : 1;
		u32 rxs0_rx0_adc_intl_cal_error_o : 1;
		u32 rxs0_rx0_dfe_train_en_i : 1;
		u32 rxs0_rx0_dfe_train_done_o : 1;
		u32 rxs0_rx0_vga_adapt_en_i : 1;
		u32 rxs0_rx0_vga_adapt_done_o : 1;
		u32 rxs0_rx0_ctle_adapt_en_i : 1;
		u32 rxs0_rx0_ctle_adapt_done_o : 1;
		u32 rxs0_rx0_adc_ofst_adapt_en_i : 1;
		u32 rxs0_rx0_adc_ofst_adapt_done_o : 1;
		u32 rxs0_rx0_adc_ofst_adapt_error_o : 1;
		u32 rxs0_rx0_adc_gain_adapt_en_i : 1;
		u32 rxs0_rx0_adc_gain_adapt_done_o : 1;
		u32 rxs0_rx0_adc_gain_adapt_error_o : 1;
		u32 rxs0_rx0_adc_intl_adapt_en_i : 1;
	};
	u32 reg;
} E56G__PMD_RXS0_OVRDVAL_1;
#define E56G__PMD_RXS0_OVRDVAL_1_ADDR                 (E56G__BASEADDR + 0x1544)

typedef union {
	struct {
		u32 rxs1_rx0_fe_ofst_cal_error_o    : 1;
		u32 rxs1_rx0_fom_en_i               : 1;
		u32 rxs1_rx0_idle_detect_en_i       : 1;
		u32 rxs1_rx0_idle_o                 : 1;
		u32 rxs1_rx0_txffe_train_en_i       : 1;
		u32 rxs1_rx0_txffe_train_enack_o    : 1;
		u32 rxs1_rx0_txffe_train_done_o     : 1;
		u32 rxs1_rx0_vga_train_en_i         : 1;
		u32 rxs1_rx0_vga_train_done_o       : 1;
		u32 rxs1_rx0_ctle_train_en_i        : 1;
		u32 rxs1_rx0_ctle_train_done_o      : 1;
		u32 rxs1_rx0_cdr_en_i               : 1;
		u32 rxs1_rx0_cdr_rdy_o              : 1;
		u32 rxs1_rx0_ffe_train_en_i         : 1;
		u32 rxs1_rx0_ffe_train_done_o       : 1;
		u32 rxs1_rx0_mmpd_en_i              : 1;
		u32 rxs1_rx0_adc_intl_cal_en_i      : 1;
		u32 rxs1_rx0_adc_intl_cal_done_o    : 1;
		u32 rxs1_rx0_adc_intl_cal_error_o   : 1;
		u32 rxs1_rx0_dfe_train_en_i         : 1;
		u32 rxs1_rx0_dfe_train_done_o       : 1;
		u32 rxs1_rx0_vga_adapt_en_i         : 1;
		u32 rxs1_rx0_vga_adapt_done_o       : 1;
		u32 rxs1_rx0_ctle_adapt_en_i        : 1;
		u32 rxs1_rx0_ctle_adapt_done_o      : 1;
		u32 rxs1_rx0_adc_ofst_adapt_en_i    : 1;
		u32 rxs1_rx0_adc_ofst_adapt_done_o  : 1;
		u32 rxs1_rx0_adc_ofst_adapt_error_o : 1;
		u32 rxs1_rx0_adc_gain_adapt_en_i    : 1;
		u32 rxs1_rx0_adc_gain_adapt_done_o  : 1;
		u32 rxs1_rx0_adc_gain_adapt_error_o : 1;
		u32 rxs1_rx0_adc_intl_adapt_en_i    : 1;
	};
	u32 reg;
} E56G__PMD_RXS1_OVRDVAL_1;

#define E56G__PMD_RXS1_OVRDVAL_1_ADDR                 (E56G__BASEADDR + 0x1570)

typedef union {
	struct {
		u32 rxs2_rx0_fe_ofst_cal_error_o    : 1;
		u32 rxs2_rx0_fom_en_i               : 1;
		u32 rxs2_rx0_idle_detect_en_i       : 1;
		u32 rxs2_rx0_idle_o                 : 1;
		u32 rxs2_rx0_txffe_train_en_i       : 1;
		u32 rxs2_rx0_txffe_train_enack_o    : 1;
		u32 rxs2_rx0_txffe_train_done_o     : 1;
		u32 rxs2_rx0_vga_train_en_i         : 1;
		u32 rxs2_rx0_vga_train_done_o       : 1;
		u32 rxs2_rx0_ctle_train_en_i        : 1;
		u32 rxs2_rx0_ctle_train_done_o      : 1;
		u32 rxs2_rx0_cdr_en_i               : 1;
		u32 rxs2_rx0_cdr_rdy_o              : 1;
		u32 rxs2_rx0_ffe_train_en_i         : 1;
		u32 rxs2_rx0_ffe_train_done_o       : 1;
		u32 rxs2_rx0_mmpd_en_i              : 1;
		u32 rxs2_rx0_adc_intl_cal_en_i      : 1;
		u32 rxs2_rx0_adc_intl_cal_done_o    : 1;
		u32 rxs2_rx0_adc_intl_cal_error_o   : 1;
		u32 rxs2_rx0_dfe_train_en_i         : 1;
		u32 rxs2_rx0_dfe_train_done_o       : 1;
		u32 rxs2_rx0_vga_adapt_en_i         : 1;
		u32 rxs2_rx0_vga_adapt_done_o       : 1;
		u32 rxs2_rx0_ctle_adapt_en_i        : 1;
		u32 rxs2_rx0_ctle_adapt_done_o      : 1;
		u32 rxs2_rx0_adc_ofst_adapt_en_i    : 1;
		u32 rxs2_rx0_adc_ofst_adapt_done_o  : 1;
		u32 rxs2_rx0_adc_ofst_adapt_error_o : 1;
		u32 rxs2_rx0_adc_gain_adapt_en_i    : 1;
		u32 rxs2_rx0_adc_gain_adapt_done_o  : 1;
		u32 rxs2_rx0_adc_gain_adapt_error_o : 1;
		u32 rxs2_rx0_adc_intl_adapt_en_i    : 1;
	};
	u32 reg;
} E56G__PMD_RXS2_OVRDVAL_1;

#define E56G__PMD_RXS2_OVRDVAL_1_ADDR                 (E56G__BASEADDR + 0x159c)

typedef union {
	struct {
		u32 rxs3_rx0_fe_ofst_cal_error_o    : 1;
		u32 rxs3_rx0_fom_en_i               : 1;
		u32 rxs3_rx0_idle_detect_en_i       : 1;
		u32 rxs3_rx0_idle_o                 : 1;
		u32 rxs3_rx0_txffe_train_en_i       : 1;
		u32 rxs3_rx0_txffe_train_enack_o    : 1;
		u32 rxs3_rx0_txffe_train_done_o     : 1;
		u32 rxs3_rx0_vga_train_en_i         : 1;
		u32 rxs3_rx0_vga_train_done_o       : 1;
		u32 rxs3_rx0_ctle_train_en_i        : 1;
		u32 rxs3_rx0_ctle_train_done_o      : 1;
		u32 rxs3_rx0_cdr_en_i               : 1;
		u32 rxs3_rx0_cdr_rdy_o              : 1;
		u32 rxs3_rx0_ffe_train_en_i         : 1;
		u32 rxs3_rx0_ffe_train_done_o       : 1;
		u32 rxs3_rx0_mmpd_en_i              : 1;
		u32 rxs3_rx0_adc_intl_cal_en_i      : 1;
		u32 rxs3_rx0_adc_intl_cal_done_o    : 1;
		u32 rxs3_rx0_adc_intl_cal_error_o   : 1;
		u32 rxs3_rx0_dfe_train_en_i         : 1;
		u32 rxs3_rx0_dfe_train_done_o       : 1;
		u32 rxs3_rx0_vga_adapt_en_i         : 1;
		u32 rxs3_rx0_vga_adapt_done_o       : 1;
		u32 rxs3_rx0_ctle_adapt_en_i        : 1;
		u32 rxs3_rx0_ctle_adapt_done_o      : 1;
		u32 rxs3_rx0_adc_ofst_adapt_en_i    : 1;
		u32 rxs3_rx0_adc_ofst_adapt_done_o  : 1;
		u32 rxs3_rx0_adc_ofst_adapt_error_o : 1;
		u32 rxs3_rx0_adc_gain_adapt_en_i    : 1;
		u32 rxs3_rx0_adc_gain_adapt_done_o  : 1;
		u32 rxs3_rx0_adc_gain_adapt_error_o : 1;
		u32 rxs3_rx0_adc_intl_adapt_en_i    : 1;
	};
	u32 reg;
} E56G__PMD_RXS3_OVRDVAL_1;

#define E56G__PMD_RXS3_OVRDVAL_1_ADDR                 (E56G__BASEADDR + 0x15c8)

typedef union {
	struct {
		u32 ctrl_fsm_rx0_st : 6;
		u32 rsvd0 : 2;
		u32 ctrl_fsm_rx1_st : 6;
		u32 rsvd1 : 2;
		u32 ctrl_fsm_rx2_st : 6;
		u32 rsvd2 : 2;
		u32 ctrl_fsm_rx3_st : 6;
		u32 rsvd3 : 2;
	};
	u32 reg;
} E56G__PMD_CTRL_FSM_RX_STAT_0;

#define E56G__PMD_CTRL_FSM_RX_STAT_0_ADDR             (E56G__BASEADDR + 0x14fc)

typedef union {
	struct {
		u32 ana_en_rterm_i : 1;
		u32 ana_en_bias_i : 1;
		u32 ana_en_ldo_i : 1;
		u32 ana_rstn_i : 1;
		u32 ana_en_blwc_i : 1;
		u32 ana_en_acc_amp_i : 1;
		u32 ana_en_acc_dac_i : 1;
		u32 ana_en_afe_offset_cal_i : 1;
		u32 ana_clk_offsetcal_i : 1;
		u32 ana_acc_os_comp_o : 1;
		u32 ana_en_ctle_i : 1;
		u32 ana_ctle_bypass_i : 1;
		u32 ana_en_ctlecdr_i : 1;
		u32 ana_cdr_ctle_boost_i : 1;
		u32 ana_en_vga_i : 1;
		u32 ana_en_bbcdr_vco_i : 1;
		u32 ana_bbcdr_vcofilt_byp_i : 1;
		u32 ana_en_bbcdr_i : 1;
		u32 ana_en_bbcdr_clk_i : 1;
		u32 ana_bbcdr_en_elv_cnt_ping0_pong1_i : 1;
		u32 ana_bbcdr_clrz_elv_cnt_ping_i : 1;
		u32 ana_bbcdr_clrz_elv_cnt_pong_i : 1;
		u32 ana_bbcdr_clrz_cnt_sync_i : 1;
		u32 ana_bbcdr_en_elv_cnt_rd_i : 1;
		u32 ana_bbcdr_elv_cnt_ping_0_o : 1;
		u32 ana_bbcdr_elv_cnt_ping_90_o : 1;
		u32 ana_bbcdr_elv_cnt_ping_180_o : 1;
		u32 ana_bbcdr_elv_cnt_ping_270_o : 1;
		u32 ana_bbcdr_elv_cnt_pong_0_o : 1;
		u32 ana_bbcdr_elv_cnt_pong_90_o : 1;
		u32 ana_bbcdr_elv_cnt_pong_180_o : 1;
		u32 ana_bbcdr_elv_cnt_pong_270_o : 1;
	};
	u32 reg;
} E56G__RXS0_ANA_OVRDVAL_0;
#define E56G__RXS0_ANA_OVRDVAL_0_ADDR                   (E56G__BASEADDR + 0xa0)

typedef union {
	struct {
		u32 ana_en_rterm_i                     : 1;
		u32 ana_en_bias_i                      : 1;
		u32 ana_en_ldo_i                       : 1;
		u32 ana_rstn_i                         : 1;
		u32 ana_en_blwc_i                      : 1;
		u32 ana_en_acc_amp_i                   : 1;
		u32 ana_en_acc_dac_i                   : 1;
		u32 ana_en_afe_offset_cal_i            : 1;
		u32 ana_clk_offsetcal_i                : 1;
		u32 ana_acc_os_comp_o                  : 1;
		u32 ana_en_ctle_i                      : 1;
		u32 ana_ctle_bypass_i                  : 1;
		u32 ana_en_ctlecdr_i                   : 1;
		u32 ana_cdr_ctle_boost_i               : 1;
		u32 ana_en_vga_i                       : 1;
		u32 ana_en_bbcdr_vco_i                 : 1;
		u32 ana_bbcdr_vcofilt_byp_i            : 1;
		u32 ana_en_bbcdr_i                     : 1;
		u32 ana_en_bbcdr_clk_i                 : 1;
		u32 ana_bbcdr_en_elv_cnt_ping0_pong1_i : 1;
		u32 ana_bbcdr_clrz_elv_cnt_ping_i      : 1;
		u32 ana_bbcdr_clrz_elv_cnt_pong_i      : 1;
		u32 ana_bbcdr_clrz_cnt_sync_i          : 1;
		u32 ana_bbcdr_en_elv_cnt_rd_i          : 1;
		u32 ana_bbcdr_elv_cnt_ping_0_o         : 1;
		u32 ana_bbcdr_elv_cnt_ping_90_o        : 1;
		u32 ana_bbcdr_elv_cnt_ping_180_o       : 1;
		u32 ana_bbcdr_elv_cnt_ping_270_o       : 1;
		u32 ana_bbcdr_elv_cnt_pong_0_o         : 1;
		u32 ana_bbcdr_elv_cnt_pong_90_o        : 1;
		u32 ana_bbcdr_elv_cnt_pong_180_o       : 1;
		u32 ana_bbcdr_elv_cnt_pong_270_o       : 1;
	};
	u32 reg;
} E56G__RXS1_ANA_OVRDVAL_0;

#define E56G__RXS1_ANA_OVRDVAL_0_ADDR                  (E56G__BASEADDR + 0x2a0)

typedef union {
	struct {
		u32 ana_en_rterm_i                     : 1;
		u32 ana_en_bias_i                      : 1;
		u32 ana_en_ldo_i                       : 1;
		u32 ana_rstn_i                         : 1;
		u32 ana_en_blwc_i                      : 1;
		u32 ana_en_acc_amp_i                   : 1;
		u32 ana_en_acc_dac_i                   : 1;
		u32 ana_en_afe_offset_cal_i            : 1;
		u32 ana_clk_offsetcal_i                : 1;
		u32 ana_acc_os_comp_o                  : 1;
		u32 ana_en_ctle_i                      : 1;
		u32 ana_ctle_bypass_i                  : 1;
		u32 ana_en_ctlecdr_i                   : 1;
		u32 ana_cdr_ctle_boost_i               : 1;
		u32 ana_en_vga_i                       : 1;
		u32 ana_en_bbcdr_vco_i                 : 1;
		u32 ana_bbcdr_vcofilt_byp_i            : 1;
		u32 ana_en_bbcdr_i                     : 1;
		u32 ana_en_bbcdr_clk_i                 : 1;
		u32 ana_bbcdr_en_elv_cnt_ping0_pong1_i : 1;
		u32 ana_bbcdr_clrz_elv_cnt_ping_i      : 1;
		u32 ana_bbcdr_clrz_elv_cnt_pong_i      : 1;
		u32 ana_bbcdr_clrz_cnt_sync_i          : 1;
		u32 ana_bbcdr_en_elv_cnt_rd_i          : 1;
		u32 ana_bbcdr_elv_cnt_ping_0_o         : 1;
		u32 ana_bbcdr_elv_cnt_ping_90_o        : 1;
		u32 ana_bbcdr_elv_cnt_ping_180_o       : 1;
		u32 ana_bbcdr_elv_cnt_ping_270_o       : 1;
		u32 ana_bbcdr_elv_cnt_pong_0_o         : 1;
		u32 ana_bbcdr_elv_cnt_pong_90_o        : 1;
		u32 ana_bbcdr_elv_cnt_pong_180_o       : 1;
		u32 ana_bbcdr_elv_cnt_pong_270_o       : 1;
	};
	u32 reg;
} E56G__RXS2_ANA_OVRDVAL_0;

#define E56G__RXS2_ANA_OVRDVAL_0_ADDR                  (E56G__BASEADDR + 0x4a0)

typedef union {
	struct {
		u32 ana_en_rterm_i                     : 1;
		u32 ana_en_bias_i                      : 1;
		u32 ana_en_ldo_i                       : 1;
		u32 ana_rstn_i                         : 1;
		u32 ana_en_blwc_i                      : 1;
		u32 ana_en_acc_amp_i                   : 1;
		u32 ana_en_acc_dac_i                   : 1;
		u32 ana_en_afe_offset_cal_i            : 1;
		u32 ana_clk_offsetcal_i                : 1;
		u32 ana_acc_os_comp_o                  : 1;
		u32 ana_en_ctle_i                      : 1;
		u32 ana_ctle_bypass_i                  : 1;
		u32 ana_en_ctlecdr_i                   : 1;
		u32 ana_cdr_ctle_boost_i               : 1;
		u32 ana_en_vga_i                       : 1;
		u32 ana_en_bbcdr_vco_i                 : 1;
		u32 ana_bbcdr_vcofilt_byp_i            : 1;
		u32 ana_en_bbcdr_i                     : 1;
		u32 ana_en_bbcdr_clk_i                 : 1;
		u32 ana_bbcdr_en_elv_cnt_ping0_pong1_i : 1;
		u32 ana_bbcdr_clrz_elv_cnt_ping_i      : 1;
		u32 ana_bbcdr_clrz_elv_cnt_pong_i      : 1;
		u32 ana_bbcdr_clrz_cnt_sync_i          : 1;
		u32 ana_bbcdr_en_elv_cnt_rd_i          : 1;
		u32 ana_bbcdr_elv_cnt_ping_0_o         : 1;
		u32 ana_bbcdr_elv_cnt_ping_90_o        : 1;
		u32 ana_bbcdr_elv_cnt_ping_180_o       : 1;
		u32 ana_bbcdr_elv_cnt_ping_270_o       : 1;
		u32 ana_bbcdr_elv_cnt_pong_0_o         : 1;
		u32 ana_bbcdr_elv_cnt_pong_90_o        : 1;
		u32 ana_bbcdr_elv_cnt_pong_180_o       : 1;
		u32 ana_bbcdr_elv_cnt_pong_270_o       : 1;
	};
	u32 reg;
} E56G__RXS3_ANA_OVRDVAL_0;

#define E56G__RXS3_ANA_OVRDVAL_0_ADDR                  (E56G__BASEADDR + 0x6a0)

typedef union {
	struct {
		u32 ovrd_en_ana_en_rterm_i : 1;
		u32 ovrd_en_ana_trim_rterm_i : 1;
		u32 ovrd_en_ana_en_bias_i : 1;
		u32 ovrd_en_ana_test_bias_i : 1;
		u32 ovrd_en_ana_en_ldo_i : 1;
		u32 ovrd_en_ana_test_ldo_i : 1;
		u32 ovrd_en_ana_rstn_i : 1;
		u32 ovrd_en_ana_en_blwc_i : 1;
		u32 ovrd_en_ana_en_acc_amp_i : 1;
		u32 ovrd_en_ana_en_acc_dac_i : 1;
		u32 ovrd_en_ana_en_afe_offset_cal_i : 1;
		u32 ovrd_en_ana_clk_offsetcal_i : 1;
		u32 ovrd_en_ana_acc_os_code_i : 1;
		u32 ovrd_en_ana_acc_os_comp_o : 1;
		u32 ovrd_en_ana_test_acc_i : 1;
		u32 ovrd_en_ana_en_ctle_i : 1;
		u32 ovrd_en_ana_ctle_bypass_i : 1;
		u32 ovrd_en_ana_ctle_cz_cstm_i : 1;
		u32 ovrd_en_ana_ctle_cload_cstm_i : 1;
		u32 ovrd_en_ana_test_ctle_i : 1;
		u32 ovrd_en_ana_lfeq_ctrl_cstm_i : 1;
		u32 ovrd_en_ana_en_ctlecdr_i : 1;
		u32 ovrd_en_ana_cdr_ctle_boost_i : 1;
		u32 ovrd_en_ana_test_ctlecdr_i : 1;
		u32 ovrd_en_ana_en_vga_i : 1;
		u32 ovrd_en_ana_vga_gain_cstm_i : 1;
		u32 ovrd_en_ana_vga_cload_in_cstm_i : 1;
		u32 ovrd_en_ana_test_vga_i : 1;
		u32 ovrd_en_ana_en_bbcdr_vco_i : 1;
		u32 ovrd_en_ana_bbcdr_osc_range_sel_i : 1;
		u32 ovrd_en_ana_sel_vga_gain_byp_i : 1;
		u32 ovrd_en_ana_vga2_gain_cstm_i : 1;
	};
	u32 reg;
} E56G__RXS0_ANA_OVRDEN_0;

#define E56G__RXS0_ANA_OVRDEN_0_ADDR                    (E56G__BASEADDR + 0x8c)

typedef union {
	struct {
		u32 ovrd_en_ana_en_rterm_i            : 1;
		u32 ovrd_en_ana_trim_rterm_i          : 1;
		u32 ovrd_en_ana_en_bias_i             : 1;
		u32 ovrd_en_ana_test_bias_i           : 1;
		u32 ovrd_en_ana_en_ldo_i              : 1;
		u32 ovrd_en_ana_test_ldo_i            : 1;
		u32 ovrd_en_ana_rstn_i                : 1;
		u32 ovrd_en_ana_en_blwc_i             : 1;
		u32 ovrd_en_ana_en_acc_amp_i          : 1;
		u32 ovrd_en_ana_en_acc_dac_i          : 1;
		u32 ovrd_en_ana_en_afe_offset_cal_i   : 1;
		u32 ovrd_en_ana_clk_offsetcal_i       : 1;
		u32 ovrd_en_ana_acc_os_code_i         : 1;
		u32 ovrd_en_ana_acc_os_comp_o         : 1;
		u32 ovrd_en_ana_test_acc_i            : 1;
		u32 ovrd_en_ana_en_ctle_i             : 1;
		u32 ovrd_en_ana_ctle_bypass_i         : 1;
		u32 ovrd_en_ana_ctle_cz_cstm_i        : 1;
		u32 ovrd_en_ana_ctle_cload_cstm_i     : 1;
		u32 ovrd_en_ana_test_ctle_i           : 1;
		u32 ovrd_en_ana_lfeq_ctrl_cstm_i      : 1;
		u32 ovrd_en_ana_en_ctlecdr_i          : 1;
		u32 ovrd_en_ana_cdr_ctle_boost_i      : 1;
		u32 ovrd_en_ana_test_ctlecdr_i        : 1;
		u32 ovrd_en_ana_en_vga_i              : 1;
		u32 ovrd_en_ana_vga_gain_cstm_i       : 1;
		u32 ovrd_en_ana_vga_cload_in_cstm_i   : 1;
		u32 ovrd_en_ana_test_vga_i            : 1;
		u32 ovrd_en_ana_en_bbcdr_vco_i        : 1;
		u32 ovrd_en_ana_bbcdr_osc_range_sel_i : 1;
		u32 ovrd_en_ana_sel_vga_gain_byp_i    : 1;
		u32 ovrd_en_ana_vga2_gain_cstm_i      : 1;
	};
	u32 reg;
} E56G__RXS1_ANA_OVRDEN_0;

#define E56G__RXS1_ANA_OVRDEN_0_ADDR                   (E56G__BASEADDR + 0x28c)

typedef union {
	struct {
		u32 ovrd_en_ana_en_rterm_i            : 1;
		u32 ovrd_en_ana_trim_rterm_i          : 1;
		u32 ovrd_en_ana_en_bias_i             : 1;
		u32 ovrd_en_ana_test_bias_i           : 1;
		u32 ovrd_en_ana_en_ldo_i              : 1;
		u32 ovrd_en_ana_test_ldo_i            : 1;
		u32 ovrd_en_ana_rstn_i                : 1;
		u32 ovrd_en_ana_en_blwc_i             : 1;
		u32 ovrd_en_ana_en_acc_amp_i          : 1;
		u32 ovrd_en_ana_en_acc_dac_i          : 1;
		u32 ovrd_en_ana_en_afe_offset_cal_i   : 1;
		u32 ovrd_en_ana_clk_offsetcal_i       : 1;
		u32 ovrd_en_ana_acc_os_code_i         : 1;
		u32 ovrd_en_ana_acc_os_comp_o         : 1;
		u32 ovrd_en_ana_test_acc_i            : 1;
		u32 ovrd_en_ana_en_ctle_i             : 1;
		u32 ovrd_en_ana_ctle_bypass_i         : 1;
		u32 ovrd_en_ana_ctle_cz_cstm_i        : 1;
		u32 ovrd_en_ana_ctle_cload_cstm_i     : 1;
		u32 ovrd_en_ana_test_ctle_i           : 1;
		u32 ovrd_en_ana_lfeq_ctrl_cstm_i      : 1;
		u32 ovrd_en_ana_en_ctlecdr_i          : 1;
		u32 ovrd_en_ana_cdr_ctle_boost_i      : 1;
		u32 ovrd_en_ana_test_ctlecdr_i        : 1;
		u32 ovrd_en_ana_en_vga_i              : 1;
		u32 ovrd_en_ana_vga_gain_cstm_i       : 1;
		u32 ovrd_en_ana_vga_cload_in_cstm_i   : 1;
		u32 ovrd_en_ana_test_vga_i            : 1;
		u32 ovrd_en_ana_en_bbcdr_vco_i        : 1;
		u32 ovrd_en_ana_bbcdr_osc_range_sel_i : 1;
		u32 ovrd_en_ana_sel_vga_gain_byp_i    : 1;
		u32 ovrd_en_ana_vga2_gain_cstm_i      : 1;
	};
	u32 reg;
} E56G__RXS2_ANA_OVRDEN_0;

#define E56G__RXS2_ANA_OVRDEN_0_ADDR                   (E56G__BASEADDR + 0x48c)

typedef union {
	struct {
		u32 ovrd_en_ana_en_rterm_i            : 1;
		u32 ovrd_en_ana_trim_rterm_i          : 1;
		u32 ovrd_en_ana_en_bias_i             : 1;
		u32 ovrd_en_ana_test_bias_i           : 1;
		u32 ovrd_en_ana_en_ldo_i              : 1;
		u32 ovrd_en_ana_test_ldo_i            : 1;
		u32 ovrd_en_ana_rstn_i                : 1;
		u32 ovrd_en_ana_en_blwc_i             : 1;
		u32 ovrd_en_ana_en_acc_amp_i          : 1;
		u32 ovrd_en_ana_en_acc_dac_i          : 1;
		u32 ovrd_en_ana_en_afe_offset_cal_i   : 1;
		u32 ovrd_en_ana_clk_offsetcal_i       : 1;
		u32 ovrd_en_ana_acc_os_code_i         : 1;
		u32 ovrd_en_ana_acc_os_comp_o         : 1;
		u32 ovrd_en_ana_test_acc_i            : 1;
		u32 ovrd_en_ana_en_ctle_i             : 1;
		u32 ovrd_en_ana_ctle_bypass_i         : 1;
		u32 ovrd_en_ana_ctle_cz_cstm_i        : 1;
		u32 ovrd_en_ana_ctle_cload_cstm_i     : 1;
		u32 ovrd_en_ana_test_ctle_i           : 1;
		u32 ovrd_en_ana_lfeq_ctrl_cstm_i      : 1;
		u32 ovrd_en_ana_en_ctlecdr_i          : 1;
		u32 ovrd_en_ana_cdr_ctle_boost_i      : 1;
		u32 ovrd_en_ana_test_ctlecdr_i        : 1;
		u32 ovrd_en_ana_en_vga_i              : 1;
		u32 ovrd_en_ana_vga_gain_cstm_i       : 1;
		u32 ovrd_en_ana_vga_cload_in_cstm_i   : 1;
		u32 ovrd_en_ana_test_vga_i            : 1;
		u32 ovrd_en_ana_en_bbcdr_vco_i        : 1;
		u32 ovrd_en_ana_bbcdr_osc_range_sel_i : 1;
		u32 ovrd_en_ana_sel_vga_gain_byp_i    : 1;
		u32 ovrd_en_ana_vga2_gain_cstm_i      : 1;
	};
	u32 reg;
} E56G__RXS3_ANA_OVRDEN_0;

#define E56G__RXS3_ANA_OVRDEN_0_NUM                                         1
#define E56G__RXS3_ANA_OVRDEN_0_ADDR                   (E56G__BASEADDR + 0x68c)

typedef union {
	struct {
		u32 ana_ctle_cz_cstm_i : 5;
		u32 rsvd0 : 3;
		u32 ana_ctle_cload_cstm_i : 5;
		u32 rsvd1 : 3;
		u32 ana_test_ctle_i : 2;
		u32 rsvd2 : 2;
		u32 ana_lfeq_ctrl_cstm_i : 4;
		u32 ana_test_ctlecdr_i : 2;
		u32 rsvd3 : 2;
		u32 ana_vga_cload_in_cstm_i : 3;
		u32 rsvd4 : 1;
	};
	u32 reg;
} E56G__RXS0_ANA_OVRDVAL_3;

#define E56G__RXS0_ANA_OVRDVAL_3_NUM                                        1
#define E56G__RXS0_ANA_OVRDVAL_3_ADDR                   (E56G__BASEADDR + 0xac)

typedef union {
	struct {
		u32 ana_ctle_cz_cstm_i      : 5;
		u32 rsvd0                   : 3;
		u32 ana_ctle_cload_cstm_i   : 5;
		u32 rsvd1                   : 3;
		u32 ana_test_ctle_i         : 2;
		u32 rsvd2                   : 2;
		u32 ana_lfeq_ctrl_cstm_i    : 4;
		u32 ana_test_ctlecdr_i      : 2;
		u32 rsvd3                   : 2;
		u32 ana_vga_cload_in_cstm_i : 3;
		u32 rsvd4                   : 1;
	};
	u32 reg;
} E56G__RXS1_ANA_OVRDVAL_3;

#define E56G__RXS1_ANA_OVRDVAL_3_ADDR                  (E56G__BASEADDR + 0x2ac)

typedef union {
	struct {
		u32 ana_ctle_cz_cstm_i      : 5;
		u32 rsvd0                   : 3;
		u32 ana_ctle_cload_cstm_i   : 5;
		u32 rsvd1                   : 3;
		u32 ana_test_ctle_i         : 2;
		u32 rsvd2                   : 2;
		u32 ana_lfeq_ctrl_cstm_i    : 4;
		u32 ana_test_ctlecdr_i      : 2;
		u32 rsvd3                   : 2;
		u32 ana_vga_cload_in_cstm_i : 3;
		u32 rsvd4                   : 1;
	};
	u32 reg;
} E56G__RXS2_ANA_OVRDVAL_3;

#define E56G__RXS2_ANA_OVRDVAL_3_ADDR                  (E56G__BASEADDR + 0x4ac)

typedef union {
	struct {
		u32 ana_ctle_cz_cstm_i      : 5;
		u32 rsvd0                   : 3;
		u32 ana_ctle_cload_cstm_i   : 5;
		u32 rsvd1                   : 3;
		u32 ana_test_ctle_i         : 2;
		u32 rsvd2                   : 2;
		u32 ana_lfeq_ctrl_cstm_i    : 4;
		u32 ana_test_ctlecdr_i      : 2;
		u32 rsvd3                   : 2;
		u32 ana_vga_cload_in_cstm_i : 3;
		u32 rsvd4                   : 1;
	};
	u32 reg;
} E56G__RXS3_ANA_OVRDVAL_3;

#define E56G__RXS3_ANA_OVRDVAL_3_ADDR                  (E56G__BASEADDR + 0x6ac)

typedef union {
	struct {
		u32 ovrd_en_rxs0_rx0_adc_gain_cal_en_i : 1;
		u32 ovrd_en_rxs0_rx0_adc_gain_cal_done_o : 1;
		u32 ovrd_en_rxs0_rx0_adc_gain_cal_error_o : 1;
		u32 ovrd_en_rxs0_rx0_fe_ofst_cal_en_i : 1;
		u32 ovrd_en_rxs0_rx0_fe_ofst_cal_done_o : 1;
		u32 ovrd_en_rxs0_rx0_fe_ofst_cal_error_o : 1;
		u32 ovrd_en_rxs0_rx0_fom_en_i : 1;
		u32 ovrd_en_rxs0_rx0_idle_detect_en_i : 1;
		u32 ovrd_en_rxs0_rx0_idle_o : 1;
		u32 ovrd_en_rxs0_rx0_txffe_train_en_i : 1;
		u32 ovrd_en_rxs0_rx0_txffe_coeff_rst_i : 1;
		u32 ovrd_en_rxs0_rx0_txffe_train_enack_o : 1;
		u32 ovrd_en_rxs0_rx0_txffe_train_done_o : 1;
		u32 ovrd_en_rxs0_rx0_txffe_coeff_change_o : 1;
		u32 ovrd_en_rxs0_rx0_vga_train_en_i : 1;
		u32 ovrd_en_rxs0_rx0_vga_train_done_o : 1;
		u32 ovrd_en_rxs0_rx0_ctle_train_en_i : 1;
		u32 ovrd_en_rxs0_rx0_ctle_train_done_o : 1;
		u32 ovrd_en_rxs0_rx0_cdr_en_i : 1;
		u32 ovrd_en_rxs0_rx0_cdr_rdy_o : 1;
		u32 ovrd_en_rxs0_rx0_ffe_train_en_i : 1;
		u32 ovrd_en_rxs0_rx0_ffe_train_done_o : 1;
		u32 ovrd_en_rxs0_rx0_mmpd_en_i : 1;
		u32 ovrd_en_rxs0_rx0_adc_intl_cal_en_i : 1;
		u32 ovrd_en_rxs0_rx0_adc_intl_cal_done_o : 1;
		u32 ovrd_en_rxs0_rx0_adc_intl_cal_error_o : 1;
		u32 ovrd_en_rxs0_rx0_dfe_train_en_i : 1;
		u32 ovrd_en_rxs0_rx0_dfe_train_done_o : 1;
		u32 ovrd_en_rxs0_rx0_vga_adapt_en_i : 1;
		u32 ovrd_en_rxs0_rx0_vga_adapt_done_o : 1;
		u32 ovrd_en_rxs0_rx0_ctle_adapt_en_i : 1;
		u32 ovrd_en_rxs0_rx0_ctle_adapt_done_o : 1;
	};
	u32 reg;
} E56G__PMD_RXS0_OVRDEN_1;

#define E56G__PMD_RXS0_OVRDEN_1_NUM                                         1
#define E56G__PMD_RXS0_OVRDEN_1_ADDR                  (E56G__BASEADDR + 0x1534)

typedef union {
	struct {
		u32 ovrd_en_rxs1_rx0_adc_gain_cal_en_i    : 1;
		u32 ovrd_en_rxs1_rx0_adc_gain_cal_done_o  : 1;
		u32 ovrd_en_rxs1_rx0_adc_gain_cal_error_o : 1;
		u32 ovrd_en_rxs1_rx0_fe_ofst_cal_en_i     : 1;
		u32 ovrd_en_rxs1_rx0_fe_ofst_cal_done_o   : 1;
		u32 ovrd_en_rxs1_rx0_fe_ofst_cal_error_o  : 1;
		u32 ovrd_en_rxs1_rx0_fom_en_i             : 1;
		u32 ovrd_en_rxs1_rx0_idle_detect_en_i     : 1;
		u32 ovrd_en_rxs1_rx0_idle_o               : 1;
		u32 ovrd_en_rxs1_rx0_txffe_train_en_i     : 1;
		u32 ovrd_en_rxs1_rx0_txffe_coeff_rst_i    : 1;
		u32 ovrd_en_rxs1_rx0_txffe_train_enack_o  : 1;
		u32 ovrd_en_rxs1_rx0_txffe_train_done_o   : 1;
		u32 ovrd_en_rxs1_rx0_txffe_coeff_change_o : 1;
		u32 ovrd_en_rxs1_rx0_vga_train_en_i       : 1;
		u32 ovrd_en_rxs1_rx0_vga_train_done_o     : 1;
		u32 ovrd_en_rxs1_rx0_ctle_train_en_i      : 1;
		u32 ovrd_en_rxs1_rx0_ctle_train_done_o    : 1;
		u32 ovrd_en_rxs1_rx0_cdr_en_i             : 1;
		u32 ovrd_en_rxs1_rx0_cdr_rdy_o            : 1;
		u32 ovrd_en_rxs1_rx0_ffe_train_en_i       : 1;
		u32 ovrd_en_rxs1_rx0_ffe_train_done_o     : 1;
		u32 ovrd_en_rxs1_rx0_mmpd_en_i            : 1;
		u32 ovrd_en_rxs1_rx0_adc_intl_cal_en_i    : 1;
		u32 ovrd_en_rxs1_rx0_adc_intl_cal_done_o  : 1;
		u32 ovrd_en_rxs1_rx0_adc_intl_cal_error_o : 1;
		u32 ovrd_en_rxs1_rx0_dfe_train_en_i       : 1;
		u32 ovrd_en_rxs1_rx0_dfe_train_done_o     : 1;
		u32 ovrd_en_rxs1_rx0_vga_adapt_en_i       : 1;
		u32 ovrd_en_rxs1_rx0_vga_adapt_done_o     : 1;
		u32 ovrd_en_rxs1_rx0_ctle_adapt_en_i      : 1;
		u32 ovrd_en_rxs1_rx0_ctle_adapt_done_o    : 1;
	};
	u32 reg;
} E56G__PMD_RXS1_OVRDEN_1;

#define E56G__PMD_RXS1_OVRDEN_1_ADDR                  (E56G__BASEADDR + 0x1560)

typedef union {
	struct {
		u32 ovrd_en_rxs2_rx0_adc_gain_cal_en_i    : 1;
		u32 ovrd_en_rxs2_rx0_adc_gain_cal_done_o  : 1;
		u32 ovrd_en_rxs2_rx0_adc_gain_cal_error_o : 1;
		u32 ovrd_en_rxs2_rx0_fe_ofst_cal_en_i     : 1;
		u32 ovrd_en_rxs2_rx0_fe_ofst_cal_done_o   : 1;
		u32 ovrd_en_rxs2_rx0_fe_ofst_cal_error_o  : 1;
		u32 ovrd_en_rxs2_rx0_fom_en_i             : 1;
		u32 ovrd_en_rxs2_rx0_idle_detect_en_i     : 1;
		u32 ovrd_en_rxs2_rx0_idle_o               : 1;
		u32 ovrd_en_rxs2_rx0_txffe_train_en_i     : 1;
		u32 ovrd_en_rxs2_rx0_txffe_coeff_rst_i    : 1;
		u32 ovrd_en_rxs2_rx0_txffe_train_enack_o  : 1;
		u32 ovrd_en_rxs2_rx0_txffe_train_done_o   : 1;
		u32 ovrd_en_rxs2_rx0_txffe_coeff_change_o : 1;
		u32 ovrd_en_rxs2_rx0_vga_train_en_i       : 1;
		u32 ovrd_en_rxs2_rx0_vga_train_done_o     : 1;
		u32 ovrd_en_rxs2_rx0_ctle_train_en_i      : 1;
		u32 ovrd_en_rxs2_rx0_ctle_train_done_o    : 1;
		u32 ovrd_en_rxs2_rx0_cdr_en_i             : 1;
		u32 ovrd_en_rxs2_rx0_cdr_rdy_o            : 1;
		u32 ovrd_en_rxs2_rx0_ffe_train_en_i       : 1;
		u32 ovrd_en_rxs2_rx0_ffe_train_done_o     : 1;
		u32 ovrd_en_rxs2_rx0_mmpd_en_i            : 1;
		u32 ovrd_en_rxs2_rx0_adc_intl_cal_en_i    : 1;
		u32 ovrd_en_rxs2_rx0_adc_intl_cal_done_o  : 1;
		u32 ovrd_en_rxs2_rx0_adc_intl_cal_error_o : 1;
		u32 ovrd_en_rxs2_rx0_dfe_train_en_i       : 1;
		u32 ovrd_en_rxs2_rx0_dfe_train_done_o     : 1;
		u32 ovrd_en_rxs2_rx0_vga_adapt_en_i       : 1;
		u32 ovrd_en_rxs2_rx0_vga_adapt_done_o     : 1;
		u32 ovrd_en_rxs2_rx0_ctle_adapt_en_i      : 1;
		u32 ovrd_en_rxs2_rx0_ctle_adapt_done_o    : 1;
	};
	u32 reg;
} E56G__PMD_RXS2_OVRDEN_1;

#define E56G__PMD_RXS2_OVRDEN_1_ADDR                  (E56G__BASEADDR + 0x158c)

typedef union {
	struct {
		u32 ovrd_en_rxs3_rx0_adc_gain_cal_en_i    : 1;
		u32 ovrd_en_rxs3_rx0_adc_gain_cal_done_o  : 1;
		u32 ovrd_en_rxs3_rx0_adc_gain_cal_error_o : 1;
		u32 ovrd_en_rxs3_rx0_fe_ofst_cal_en_i     : 1;
		u32 ovrd_en_rxs3_rx0_fe_ofst_cal_done_o   : 1;
		u32 ovrd_en_rxs3_rx0_fe_ofst_cal_error_o  : 1;
		u32 ovrd_en_rxs3_rx0_fom_en_i             : 1;
		u32 ovrd_en_rxs3_rx0_idle_detect_en_i     : 1;
		u32 ovrd_en_rxs3_rx0_idle_o               : 1;
		u32 ovrd_en_rxs3_rx0_txffe_train_en_i     : 1;
		u32 ovrd_en_rxs3_rx0_txffe_coeff_rst_i    : 1;
		u32 ovrd_en_rxs3_rx0_txffe_train_enack_o  : 1;
		u32 ovrd_en_rxs3_rx0_txffe_train_done_o   : 1;
		u32 ovrd_en_rxs3_rx0_txffe_coeff_change_o : 1;
		u32 ovrd_en_rxs3_rx0_vga_train_en_i       : 1;
		u32 ovrd_en_rxs3_rx0_vga_train_done_o     : 1;
		u32 ovrd_en_rxs3_rx0_ctle_train_en_i      : 1;
		u32 ovrd_en_rxs3_rx0_ctle_train_done_o    : 1;
		u32 ovrd_en_rxs3_rx0_cdr_en_i             : 1;
		u32 ovrd_en_rxs3_rx0_cdr_rdy_o            : 1;
		u32 ovrd_en_rxs3_rx0_ffe_train_en_i       : 1;
		u32 ovrd_en_rxs3_rx0_ffe_train_done_o     : 1;
		u32 ovrd_en_rxs3_rx0_mmpd_en_i            : 1;
		u32 ovrd_en_rxs3_rx0_adc_intl_cal_en_i    : 1;
		u32 ovrd_en_rxs3_rx0_adc_intl_cal_done_o  : 1;
		u32 ovrd_en_rxs3_rx0_adc_intl_cal_error_o : 1;
		u32 ovrd_en_rxs3_rx0_dfe_train_en_i       : 1;
		u32 ovrd_en_rxs3_rx0_dfe_train_done_o     : 1;
		u32 ovrd_en_rxs3_rx0_vga_adapt_en_i       : 1;
		u32 ovrd_en_rxs3_rx0_vga_adapt_done_o     : 1;
		u32 ovrd_en_rxs3_rx0_ctle_adapt_en_i      : 1;
		u32 ovrd_en_rxs3_rx0_ctle_adapt_done_o    : 1;
	};
	u32 reg;
} E56G__PMD_RXS3_OVRDEN_1;

#define E56G__PMD_RXS3_OVRDEN_1_ADDR                  (E56G__BASEADDR + 0x15b8)

#define E56G__RXS0_FOM_18__ADDR                       (E56G__BASEADDR + 0x1f8)
#define E56G__RXS0_FOM_18__DFE_COEFFL_HINT__MSB                             11
#define E56G__RXS0_FOM_18__DFE_COEFFL_HINT__LSB                              0
#define E56G__RXS0_FOM_18__DFE_COEFFH_HINT__MSB                             23
#define E56G__RXS0_FOM_18__DFE_COEFFH_HINT__LSB                             12
#define E56G__RXS0_FOM_18__DFE_COEFF_HINT_LOAD__MSB                         25
#define E56G__RXS0_FOM_18__DFE_COEFF_HINT_LOAD__LSB                         25

#define DEFAULT_TEMP                            40
#define HIGH_TEMP                               70

#define E56PHY_RX_RDY_ST    0x1B

#define S10G_CMVAR_RANGE_H          0x3
#define S10G_CMVAR_RANGE_L          0x2
#define S25G_CMVAR_RANGE_H          0x1
#define S25G_CMVAR_RANGE_L          0x0

#define S25G_CMVAR_RANGE_H          0x1
#define S25G_CMVAR_RANGE_L          0x0
#define S25G_CMVAR_SEC_LOW_TH       0x1A
#define S25G_CMVAR_SEC_HIGH_TH      0x1D
#define S25G_CMVAR_UFINE_MAX        0x2
#define S25G_CMVAR_FINE_MAX         0x7
#define S25G_CMVAR_COARSE_MAX       0xF
#define S25G_CMVAR_UFINE_UMAX_WRAP  0x0
#define S25G_CMVAR_UFINE_FMAX_WRAP  0x0
#define S25G_CMVAR_FINE_FMAX_WRAP   0x2
#define S25G_CMVAR_UFINE_MIN        0x0
#define S25G_CMVAR_FINE_MIN         0x0
#define S25G_CMVAR_COARSE_MIN       0x1
#define S25G_CMVAR_UFINE_UMIN_WRAP  0x2
#define S25G_CMVAR_UFINE_FMIN_WRAP  0x2
#define S25G_CMVAR_FINE_FMIN_WRAP   0x5

#define S10G_CMVAR_RANGE_H          0x3
#define S10G_CMVAR_RANGE_L          0x2
#define S10G_CMVAR_SEC_LOW_TH       0x1A
#define S10G_CMVAR_SEC_HIGH_TH      0x1D
#define S10G_CMVAR_UFINE_MAX        0x7
#define S10G_CMVAR_FINE_MAX         0x7
#define S10G_CMVAR_COARSE_MAX       0xF
#define S10G_CMVAR_UFINE_UMAX_WRAP  0x6
#define S10G_CMVAR_UFINE_FMAX_WRAP  0x7
#define S10G_CMVAR_FINE_FMAX_WRAP   0x1
#define S10G_CMVAR_UFINE_MIN        0x0
#define S10G_CMVAR_FINE_MIN         0x0
#define S10G_CMVAR_COARSE_MIN       0x1
#define S10G_CMVAR_UFINE_UMIN_WRAP  0x2
#define S10G_CMVAR_UFINE_FMIN_WRAP  0x2
#define S10G_CMVAR_FINE_FMIN_WRAP   0x5

#define S10G_TX_FFE_CFG_MAIN        0x2c2c2c2c
#define S10G_TX_FFE_CFG_PRE1        0x0
#define S10G_TX_FFE_CFG_PRE2        0x0
#define S10G_TX_FFE_CFG_POST        0x06060606
#define S25G_TX_FFE_CFG_MAIN        0x31
#define S25G_TX_FFE_CFG_PRE1        0x4
#define S25G_TX_FFE_CFG_PRE2        0x1
#define S25G_TX_FFE_CFG_POST        0x9

#define S25G_TX_FFE_CFG_DAC_MAIN    0x2a
#define S25G_TX_FFE_CFG_DAC_PRE1    0x03
#define S25G_TX_FFE_CFG_DAC_PRE2    0x0
#define S25G_TX_FFE_CFG_DAC_POST    0x11

#define S40G_TX_FFE_CFG_MAIN        0x2b2b2b2b
#define S40G_TX_FFE_CFG_PRE1        0x03030303
#define S40G_TX_FFE_CFG_PRE2        0x0
#define S40G_TX_FFE_CFG_POST        0x11111111

#define BYPASS_CTLE_TAG             0x0

#define S10G_PHY_RX_CTLE_TAPWT_WEIGHT1      0x1
#define S10G_PHY_RX_CTLE_TAPWT_WEIGHT2      0x0
#define S10G_PHY_RX_CTLE_TAPWT_WEIGHT3      0x0
#define S10G_PHY_RX_CTLE_TAP_FRACP1         0x18
#define S10G_PHY_RX_CTLE_TAP_FRACP2         0x0
#define S10G_PHY_RX_CTLE_TAP_FRACP3         0x0

#define S25G_PHY_RX_CTLE_TAPWT_WEIGHT1      0x1
#define S25G_PHY_RX_CTLE_TAPWT_WEIGHT2      0x0
#define S25G_PHY_RX_CTLE_TAPWT_WEIGHT3      0x0
#define S25G_PHY_RX_CTLE_TAP_FRACP1         0x18
#define S25G_PHY_RX_CTLE_TAP_FRACP2         0x0
#define S25G_PHY_RX_CTLE_TAP_FRACP3         0x0

#define TXGBE_E56_PHY_LINK_UP            0x4

static inline void
set_fields_e56(unsigned int *src_data, unsigned int bit_high,
	       unsigned int bit_low, unsigned int set_value)
{
	unsigned int i;

	/* Single bit field handling */
	if (bit_high == bit_low) {
		if (set_value == 0) {
			/* clear single bit */
			*src_data &= ~(1U << bit_low);
		} else {
			/* set single bit */
			*src_data |= (1U << bit_low);
		}
	} else {
		/* first, clear the bit fields */
		for (i = bit_low; i <= bit_high; i++) {
			/* clear single bit */
			*src_data &= ~(1U << i);
		}

		/* second, or the bit fields with set value */
		*src_data |= (set_value << bit_low);
	}
}

void txgbe_e56_rx_rd_second_code_40g(struct txgbe_hw *hw, int *SECOND_CODE, int lane);
void txgbe_e56_rx_rd_second_code(struct txgbe_hw *hw, int *SECOND_CODE);
u32 txgbe_e56_cfg_40g(struct txgbe_hw *hw);
u32 txgbe_e56_cfg_25g(struct txgbe_hw *hw);
u32 txgbe_e56_cfg_10g(struct txgbe_hw *hw);
int txgbe_temp_track_seq_40g(struct txgbe_hw *hw, u32 speed);
int txgbe_temp_track_seq(struct txgbe_hw *hw, u32 speed);
int txgbe_e56_get_temp(struct txgbe_hw *hw, int *temp);
int txgbe_set_link_to_amlite(struct txgbe_hw *hw, u32 speed);
int txgbe_e56_reconfig_rx(struct txgbe_hw *hw, u32 speed);
s32 txgbe_e56_fec_set(struct txgbe_hw *hw);
s32 txgbe_e56_fec_polling(struct txgbe_hw *hw, bool *link_up);
u32 txgbe_e56_tx_ffe_cfg(struct txgbe_hw *hw, u32 speed);

#endif /* _TXGBE_E56_H_ */
