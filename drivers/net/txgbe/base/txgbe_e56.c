/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024-2026 Beijing WangXun Technology Co., Ltd.
 */

#include "txgbe_e56.h"
#include "txgbe_e56_bp.h"
#include "../txgbe_logs.h"

/*
 * compare function for qsort()
 */
static inline
int txgbe_e56_int_cmp(const void *a, const void *b)
{
	const int *num1 = (const int *)a;
	const int *num2 = (const int *)b;

	if (*num1 < *num2)
		return -1;

	else if (*num1 > *num2)
		return 1;

	else
		return 0;
}

s32 txgbe_e56_check_phy_link(struct txgbe_hw *hw, u32 *speed,
				bool *link_up)
{
	u32 rdata = 0;
	u32 links_reg = 0;

	/* must read it twice because the state may
	 * not be correct the first time you read it
	 */
	rdata = rd32_epcs(hw, 0x30001);
	rdata = rd32_epcs(hw, 0x30001);

	if (rdata & TXGBE_E56_PHY_LINK_UP)
		*link_up = true;
	else
		*link_up = false;

	if (!hw->link_valid)
		*link_up = false;

	links_reg = rd32(hw, TXGBE_PORTSTAT);
	if (*link_up) {
		if ((links_reg & TXGBE_CFG_PORT_ST_AML_LINK_40G) ==
				TXGBE_CFG_PORT_ST_AML_LINK_40G)
			*speed = TXGBE_LINK_SPEED_40GB_FULL;
		else if ((links_reg & TXGBE_CFG_PORT_ST_AML_LINK_25G) ==
				TXGBE_CFG_PORT_ST_AML_LINK_25G)
			*speed = TXGBE_LINK_SPEED_25GB_FULL;
		else if ((links_reg & TXGBE_CFG_PORT_ST_AML_LINK_10G) ==
				TXGBE_CFG_PORT_ST_AML_LINK_10G)
			*speed = TXGBE_LINK_SPEED_10GB_FULL;
	} else {
		*speed = TXGBE_LINK_SPEED_UNKNOWN;
	}

	return 0;
}

u32 txgbe_e56_tx_ffe_cfg(struct txgbe_hw *hw, u32 speed)
{
	u32 ffe_main = 0, pre1 = 0, pre2 = 0, post = 0;

	if (speed == TXGBE_LINK_SPEED_10GB_FULL) {
		ffe_main = S10G_TX_FFE_CFG_MAIN;
		pre1 = S10G_TX_FFE_CFG_PRE1;
		pre2 = S10G_TX_FFE_CFG_PRE2;
		post = S10G_TX_FFE_CFG_POST;
	} else if (speed == TXGBE_LINK_SPEED_25GB_FULL) {
		if (hw->phy.sfp_type == txgbe_sfp_type_da_cu_core0 ||
		    hw->phy.sfp_type == txgbe_sfp_type_da_cu_core1) {
			ffe_main = S25G_TX_FFE_CFG_DAC_MAIN;
			pre1 = S25G_TX_FFE_CFG_DAC_PRE1;
			pre2 = S25G_TX_FFE_CFG_DAC_PRE2;
			post = S25G_TX_FFE_CFG_DAC_POST;
		} else {
			ffe_main = S25G_TX_FFE_CFG_MAIN;
			pre1 = S25G_TX_FFE_CFG_PRE1;
			pre2 = S25G_TX_FFE_CFG_PRE2;
			post = S25G_TX_FFE_CFG_POST;
		}
	} else if (speed == TXGBE_LINK_SPEED_40GB_FULL) {
		ffe_main = S10G_TX_FFE_CFG_MAIN;
		pre1 = S10G_TX_FFE_CFG_PRE1;
		pre2 = S10G_TX_FFE_CFG_PRE2;
		post = S10G_TX_FFE_CFG_POST;

		if (hw->phy.sfp_type == txgbe_qsfp_type_40g_cu_core0 ||
		    hw->phy.sfp_type == txgbe_qsfp_type_40g_cu_core1) {
			ffe_main = S40G_TX_FFE_CFG_MAIN;
			pre1 = S40G_TX_FFE_CFG_PRE1;
			pre2 = S40G_TX_FFE_CFG_PRE2;
			post = S40G_TX_FFE_CFG_POST;
		}
	}

	if (hw->phy.ffe_set) {
		ffe_main = hw->phy.ffe_main;
		pre1 = hw->phy.ffe_pre;
		pre2 = hw->phy.ffe_pre2;
		post = hw->phy.ffe_post;
	}

	DEBUGOUT("main = 0x%x, pre1 = 0x%x, pre2 = 0x%x, post = 0x%x",
		  ffe_main, pre1, pre2, post);

	wr32_ephy(hw, E56G__PMD_TX_FFE_CFG_1_ADDR, ffe_main);
	wr32_ephy(hw, E56G__PMD_TX_FFE_CFG_2_ADDR, pre1);
	wr32_ephy(hw, E56G__PMD_TX_FFE_CFG_3_ADDR, pre2);
	wr32_ephy(hw, E56G__PMD_TX_FFE_CFG_4_ADDR, post);

	return 0;
}

int
txgbe_e56_get_temp(struct txgbe_hw *hw, int *temp)
{
	int data_code, temp_data, temp_fraction;
	u32 rdata;
	u32 timer = 0;

	while (1) {
		rdata = rd32(hw, 0x1033c);
		if (((rdata >> 12) & 0x1) != 0)
			break;
		if (timer++ > PHYINIT_TIMEOUT)
			return -1;
	}

	data_code = rdata & 0xFFF;
	temp_data = 419400 + 2205 * (data_code * 1000 / 4094 - 500);

	/* Change double Temperature to int */
	*temp = temp_data / 10000;
	temp_fraction = temp_data - (*temp * 10000);
	if (temp_fraction >= 5000)
		*temp += 1;

	return 0;
}

u32 txgbe_e56_cfg_40g(struct txgbe_hw *hw)
{
	u32 addr;
	u32 rdata = 0;
	int i;

	/* CMS config */
	addr  = E56G_CMS_ANA_OVRDVAL_7_ADDR;
	rdata = rd32_ephy(hw, addr);
	((E56G_CMS_ANA_OVRDVAL_7 *)&rdata)->ana_lcpll_lf_vco_swing_ctrl_i = 0xf;
	wr32_ephy(hw, addr, rdata);

	addr  = E56G_CMS_ANA_OVRDEN_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	((E56G_CMS_ANA_OVRDEN_1 *)&rdata)->ovrd_en_ana_lcpll_lf_vco_swing_ctrl_i = 0x1;
	wr32_ephy(hw, addr, rdata);

	addr  = E56G_CMS_ANA_OVRDVAL_9_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 23, 0, 0x260000);
	wr32_ephy(hw, addr, rdata);

	addr  = E56G_CMS_ANA_OVRDEN_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	((E56G_CMS_ANA_OVRDEN_1 *)&rdata)->ovrd_en_ana_lcpll_lf_test_in_i = 0x1;
	wr32_ephy(hw, addr, rdata);

	/* TXS config */
	for (i = 0; i < 4; i++) {
		addr  = E56PHY_TXS_TXS_CFG_1_ADDR + (E56PHY_TXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_TXS_TXS_CFG_1_ADAPTATION_WAIT_CNT_X256, 0xf);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_TXS_WKUP_CNT_ADDR + (E56PHY_TXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_TXS_WKUP_CNTLDO_WKUP_CNT_X32, 0xff);
		set_fields_e56(&rdata, E56PHY_TXS_WKUP_CNTDCC_WKUP_CNT_X32, 0xff);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_TXS_PIN_OVRDVAL_6_ADDR + (E56PHY_TXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 19, 16, 0x6);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_TXS_PIN_OVRDEN_0_ADDR + (E56PHY_TXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_TXS_PIN_OVRDEN_0_OVRD_EN_TX0_EFUSE_BITS_I, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_TXS_ANA_OVRDVAL_1_ADDR + (E56PHY_TXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_TXS_ANA_OVRDVAL_1_ANA_TEST_DAC_I, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_TXS_ANA_OVRDEN_0_ADDR + (E56PHY_TXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_TXS_ANA_OVRDEN_0_OVRD_EN_ANA_TEST_DAC_I, 0x1);
		wr32_ephy(hw, addr, rdata);
	}
	/* Setting TX FFE */
	txgbe_e56_tx_ffe_cfg(hw, TXGBE_LINK_SPEED_40GB_FULL);

	/* RXS config */
	for (i = 0; i < 4; i++) {
		addr  = E56PHY_RXS_RXS_CFG_0_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_RXS_CFG_0_DSER_DATA_SEL, 0x0);
		set_fields_e56(&rdata, E56PHY_RXS_RXS_CFG_0_TRAIN_CLK_GATE_BYPASS_EN, 0x1fff);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS_OSC_CAL_N_CDR_1_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		((E56G_RXS0_OSC_CAL_N_CDR_0 *)&rdata)->prediv0 = 0xfa0;
		((E56G_RXS0_OSC_CAL_N_CDR_0 *)&rdata)->target_cnt0 = 0x203a;
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS_OSC_CAL_N_CDR_4_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		((E56G_RXS0_OSC_CAL_N_CDR_4 *)&rdata)->osc_range_sel0 = 0x2;
		((E56G_RXS0_OSC_CAL_N_CDR_4 *)&rdata)->vco_code_init = 0x7ff;
		((E56G_RXS0_OSC_CAL_N_CDR_4 *)&rdata)->osc_current_boost_en0 = 0x1;
		((E56G_RXS0_OSC_CAL_N_CDR_4 *)&rdata)->bbcdr_current_boost0 = 0x0;
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS_OSC_CAL_N_CDR_5_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_5_SDM_WIDTH, 0x3);
		set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_5_BB_CDR_PROP_STEP_PRELOCK, 0xf);
		set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_5_BB_CDR_PROP_STEP_POSTLOCK, 0xf);
		set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_5_BB_CDR_GAIN_CTRL_POSTLOCK, 0xc);
		set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_5_BB_CDR_GAIN_CTRL_PRELOCK, 0xf);
		set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_5_BBCDR_RDY_CNT, 0x3);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS_OSC_CAL_N_CDR_6_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_6_PI_GAIN_CTRL_PRELOCK, 0x7);
		set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_6_PI_GAIN_CTRL_POSTLOCK, 0x5);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS_INTL_CONFIG_0_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		((E56G_RXS0_INTL_CONFIG_0 *)&rdata)->adc_intl2slice_delay0 = 0x5555;
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS_INTL_CONFIG_2_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		((E56G_RXS0_INTL_CONFIG_2 *)&rdata)->interleaver_hbw_disable0 = 0x1;
		wr32_ephy(hw, addr, rdata);

		rdata = 0x0000;
		addr  = E56PHY_RXS_TXFFE_TRAINING_0_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_0_ADC_DATA_PEAK_LTH, 0x56);
		set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_0_ADC_DATA_PEAK_UTH, 0x6a);
		wr32_ephy(hw, addr, rdata);

		rdata = 0x0000;
		addr  = E56PHY_RXS_TXFFE_TRAINING_1_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_1_C1_LTH, 0x1e8);
		set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_1_C1_UTH, 0x78);
		wr32_ephy(hw, addr, rdata);

		rdata = 0x0000;
		addr  = E56PHY_RXS_TXFFE_TRAINING_2_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_2_CM1_LTH, 0x100);
		set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_2_CM1_UTH, 0xff);
		wr32_ephy(hw, addr, rdata);

		rdata = 0x0000;
		addr  = E56PHY_RXS_TXFFE_TRAINING_3_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_3_CM2_LTH, 0x4);
		set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_3_CM2_UTH, 0x37);
		set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_3_TXFFE_TRAIN_MOD_TYPE, 0x38);
		wr32_ephy(hw, addr, rdata);

		rdata = 0x0000;
		addr  = E56PHY_RXS_VGA_TRAINING_0_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_VGA_TRAINING_0_VGA_TARGET, 0x34);
		wr32_ephy(hw, addr, rdata);

		rdata = 0x0000;
		addr  = E56PHY_RXS_VGA_TRAINING_1_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_VGA_TRAINING_1_VGA1_CODE_INIT0, 0xa);
		set_fields_e56(&rdata, E56PHY_RXS_VGA_TRAINING_1_VGA2_CODE_INIT0, 0xa);
		set_fields_e56(&rdata, E56PHY_RXS_VGA_TRAINING_1_VGA1_CODE_INIT123, 0xa);
		set_fields_e56(&rdata, E56PHY_RXS_VGA_TRAINING_1_VGA2_CODE_INIT123, 0xa);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS_CTLE_TRAINING_0_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_0_CTLE_CODE_INIT0, 0x9);
		set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_0_CTLE_CODE_INIT123, 0x9);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS_CTLE_TRAINING_1_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_1_LFEQ_LUT, 0x1ffffea);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS_CTLE_TRAINING_2_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_2_ISI_TH_FRAC_P1,
			       S10G_PHY_RX_CTLE_TAP_FRACP1);
		set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_2_ISI_TH_FRAC_P2,
			       S10G_PHY_RX_CTLE_TAP_FRACP2);
		set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_2_ISI_TH_FRAC_P3,
			       S10G_PHY_RX_CTLE_TAP_FRACP3);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS_CTLE_TRAINING_3_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_3_TAP_WEIGHT_P1,
			       S10G_PHY_RX_CTLE_TAPWT_WEIGHT1);
		set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_3_TAP_WEIGHT_P2,
			       S10G_PHY_RX_CTLE_TAPWT_WEIGHT2);
		set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_3_TAP_WEIGHT_P3,
			       S10G_PHY_RX_CTLE_TAPWT_WEIGHT3);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS_OFFSET_N_GAIN_CAL_0_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_OFFSET_N_GAIN_CAL_0_ADC_SLICE_DATA_AVG_CNT, 0x3);
		set_fields_e56(&rdata, E56PHY_RXS_OFFSET_N_GAIN_CAL_0_ADC_DATA_AVG_CNT, 0x3);
		set_fields_e56(&rdata, E56PHY_RXS_OFFSET_N_GAIN_CAL_0_FE_OFFSET_DAC_CLK_CNT_X8,
			       0xc);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS_OFFSET_N_GAIN_CAL_1_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_OFFSET_N_GAIN_CAL_1_SAMP_ADAPT_CFG, 0x5);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS_FFE_TRAINING_0_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_FFE_TRAINING_0_FFE_TAP_EN, 0xf9ff);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS_IDLE_DETECT_1_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_IDLE_DETECT_1_IDLE_TH_ADC_PEAK_MAX, 0xa);
		set_fields_e56(&rdata, E56PHY_RXS_IDLE_DETECT_1_IDLE_TH_ADC_PEAK_MIN, 0x5);
		wr32_ephy(hw, addr, rdata);

		addr = E56G__RXS3_ANA_OVRDVAL_11_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		((E56G__RXS3_ANA_OVRDVAL_11 *)&rdata)->ana_test_adc_clkgen_i = 0x0;
		wr32_ephy(hw, addr, rdata);

		addr = E56G__RXS0_ANA_OVRDEN_2_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		((E56G__RXS0_ANA_OVRDEN_2 *)&rdata)->ovrd_en_ana_test_adc_clkgen_i = 0x0;
		wr32_ephy(hw, addr, rdata);

		addr = E56PHY_RXS_ANA_OVRDVAL_0_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDVAL_0_ANA_EN_RTERM_I, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr = E56PHY_RXS_ANA_OVRDEN_0_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_0_OVRD_EN_ANA_EN_RTERM_I, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr = E56PHY_RXS_ANA_OVRDVAL_6_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 4, 0, 0x6);
		set_fields_e56(&rdata, 14, 13, 0x2);
		wr32_ephy(hw, addr, rdata);

		addr = E56PHY_RXS_ANA_OVRDEN_1_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_1_OVRD_EN_ANA_BBCDR_VCOFILT_BYP_I,
			       0x1);
		set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_1_OVRD_EN_ANA_TEST_BBCDR_I, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr = E56PHY_RXS_ANA_OVRDVAL_15_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 2, 0, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr = E56PHY_RXS_ANA_OVRDVAL_17_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDVAL_17_ANA_VGA2_BOOST_CSTM_I, 0x0);
		wr32_ephy(hw, addr, rdata);

		addr = E56PHY_RXS_ANA_OVRDEN_3_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_3_OVRD_EN_ANA_ANABS_CONFIG_I, 0x1);
		set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_3_OVRD_EN_ANA_VGA2_BOOST_CSTM_I, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr = E56PHY_RXS_ANA_OVRDVAL_14_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 13, 13, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr = E56PHY_RXS_ANA_OVRDEN_4_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 13, 13, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS_EYE_SCAN_1_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS_EYE_SCAN_1_EYE_SCAN_REF_TIMER, 0x400);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS_RINGO_0_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 9, 4, 0x366);
		wr32_ephy(hw, addr, rdata);
	}

	/* PDIG config */
	addr  = E56PHY_PMD_CFG_3_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_PMD_CFG_3_CTRL_FSM_TIMEOUT_X64K, 0x80);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_PMD_CFG_4_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_PMD_CFG_4_TRAIN_DC_ON_PERIOD_X64K, 0x18);
	set_fields_e56(&rdata, E56PHY_PMD_CFG_4_TRAIN_DC_PERIOD_X512K, 0x3e);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_PMD_CFG_5_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_PMD_CFG_5_USE_RECENT_MARKER_OFFSET, 0x1);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_CTRL_FSM_CFG_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_0_CONT_ON_ADC_GAIN_CAL_ERR, 0x1);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_0_DO_RX_ADC_OFST_CAL, 0x3);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_0_RX_ERR_ACTION_EN, 0x40);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_CTRL_FSM_CFG_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_1_TRAIN_ST0_WAIT_CNT_X4096, 0xff);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_1_TRAIN_ST1_WAIT_CNT_X4096, 0xff);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_1_TRAIN_ST2_WAIT_CNT_X4096, 0xff);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_1_TRAIN_ST3_WAIT_CNT_X4096, 0xff);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_CTRL_FSM_CFG_2_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_2_TRAIN_ST4_WAIT_CNT_X4096, 0x1);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_2_TRAIN_ST5_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_2_TRAIN_ST6_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_2_TRAIN_ST7_WAIT_CNT_X4096, 0x4);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_CTRL_FSM_CFG_3_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_3_TRAIN_ST8_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_3_TRAIN_ST9_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_3_TRAIN_ST10_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_3_TRAIN_ST11_WAIT_CNT_X4096, 0x4);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_CTRL_FSM_CFG_4_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_4_TRAIN_ST12_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_4_TRAIN_ST13_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_4_TRAIN_ST14_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_4_TRAIN_ST15_WAIT_CNT_X4096, 0x4);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_CTRL_FSM_CFG_7_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_7_TRAIN_ST4_EN, 0x4bf);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_7_TRAIN_ST5_EN, 0xc4bf);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_CTRL_FSM_CFG_8_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_8_TRAIN_ST7_EN, 0x47ff);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_CTRL_FSM_CFG_12_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_12_TRAIN_ST15_EN, 0x67ff);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_CTRL_FSM_CFG_13_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_13_TRAIN_ST0_DONE_EN, 0x8001);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_13_TRAIN_ST1_DONE_EN, 0x8002);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_CTRL_FSM_CFG_14_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_14_TRAIN_ST3_DONE_EN, 0x8008);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_CTRL_FSM_CFG_15_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_15_TRAIN_ST4_DONE_EN, 0x8004);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_CTRL_FSM_CFG_17_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_17_TRAIN_ST8_DONE_EN, 0x20c0);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_CTRL_FSM_CFG_18_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_18_TRAIN_ST10_DONE_EN, 0x0);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_CTRL_FSM_CFG_29_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_29_TRAIN_ST15_DC_EN, 0x3f6d);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_CTRL_FSM_CFG_33_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_33_TRAIN0_RATE_SEL, 0x8000);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_33_TRAIN1_RATE_SEL, 0x8000);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_CTRL_FSM_CFG_34_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_34_TRAIN2_RATE_SEL, 0x8000);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_34_TRAIN3_RATE_SEL, 0x8000);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_KRT_TFSM_CFG_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_KRT_TFSM_CFGKRT_TFSM_MAX_WAIT_TIMER_X1000K, 0x49);
	set_fields_e56(&rdata, E56PHY_KRT_TFSM_CFGKRT_TFSM_MAX_WAIT_TIMER_X8000K, 0x37);
	set_fields_e56(&rdata, E56PHY_KRT_TFSM_CFGKRT_TFSM_HOLDOFF_TIMER_X256K, 0x2f);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_FETX_FFE_TRAIN_CFG_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_FETX_FFE_TRAIN_CFG_0_KRT_FETX_INIT_FFE_CFG_2, 0x2);
	wr32_ephy(hw, addr, rdata);

	return 0;
}

u32
txgbe_e56_cfg_25g(struct txgbe_hw *hw)
{
	u32 addr;
	u32 rdata = 0;

	addr = E56PHY_CMS_PIN_OVRDVAL_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CMS_PIN_OVRDVAL_0_INT_PLL0_TX_SIGNAL_TYPE_I, 0x0);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CMS_PIN_OVRDEN_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CMS_PIN_OVRDEN_0_OVRD_EN_PLL0_TX_SIGNAL_TYPE_I,
		       0x0);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CMS_ANA_OVRDVAL_2_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CMS_ANA_OVRDVAL_2_ANA_LCPLL_HF_VCO_SWING_CTRL_I,
		       0xf);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CMS_ANA_OVRDEN_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata,
		       E56PHY_CMS_ANA_OVRDEN_0_OVRD_EN_ANA_LCPLL_HF_VCO_SWING_CTRL_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CMS_ANA_OVRDVAL_4_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 23, 0, 0x260000);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_CMS_ANA_OVRDEN_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CMS_ANA_OVRDEN_1_OVRD_EN_ANA_LCPLL_HF_TEST_IN_I,
		       0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_TXS_TXS_CFG_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_TXS_TXS_CFG_1_ADAPTATION_WAIT_CNT_X256, 0xf);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_TXS_WKUP_CNT_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_TXS_WKUP_CNTLDO_WKUP_CNT_X32, 0xff);
	set_fields_e56(&rdata, E56PHY_TXS_WKUP_CNTDCC_WKUP_CNT_X32, 0xff);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_TXS_PIN_OVRDVAL_6_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 27, 24, 0x5);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_TXS_PIN_OVRDEN_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_TXS_PIN_OVRDEN_0_OVRD_EN_TX0_EFUSE_BITS_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_TXS_ANA_OVRDVAL_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_TXS_ANA_OVRDVAL_1_ANA_TEST_DAC_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_TXS_ANA_OVRDEN_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_TXS_ANA_OVRDEN_0_OVRD_EN_ANA_TEST_DAC_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	txgbe_e56_tx_ffe_cfg(hw, TXGBE_LINK_SPEED_25GB_FULL);

	addr = E56PHY_RXS_RXS_CFG_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_RXS_CFG_0_DSER_DATA_SEL, 0x0);
	set_fields_e56(&rdata, E56PHY_RXS_RXS_CFG_0_TRAIN_CLK_GATE_BYPASS_EN, 0x1fff);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_RXS_OSC_CAL_N_CDR_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_1_PREDIV1, 0x700);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_1_TARGET_CNT1, 0x2418);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_OSC_CAL_N_CDR_4_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_4_OSC_RANGE_SEL1, 0x1);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_4_VCO_CODE_INIT, 0x7fb);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_4_OSC_CURRENT_BOOST_EN1, 0x0);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_4_BBCDR_CURRENT_BOOST1, 0x0);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_OSC_CAL_N_CDR_5_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_5_SDM_WIDTH, 0x3);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_5_BB_CDR_PROP_STEP_PRELOCK,
		       0xf);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_5_BB_CDR_PROP_STEP_POSTLOCK,
		       0x3);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_5_BB_CDR_GAIN_CTRL_POSTLOCK,
		       0xa);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_5_BB_CDR_GAIN_CTRL_PRELOCK,
		       0xf);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_5_BBCDR_RDY_CNT, 0x3);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_OSC_CAL_N_CDR_6_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_6_PI_GAIN_CTRL_PRELOCK, 0x7);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_6_PI_GAIN_CTRL_POSTLOCK, 0x5);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_INTL_CONFIG_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_INTL_CONFIG_0_ADC_INTL2SLICE_DELAY1, 0x3333);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_INTL_CONFIG_2_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_INTL_CONFIG_2_INTERLEAVER_HBW_DISABLE1, 0x0);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_TXFFE_TRAINING_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_0_ADC_DATA_PEAK_LTH, 0x56);
	set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_0_ADC_DATA_PEAK_UTH, 0x6a);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_TXFFE_TRAINING_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_1_C1_LTH, 0x1f8);
	set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_1_C1_UTH, 0xf0);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_TXFFE_TRAINING_2_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_2_CM1_LTH, 0x100);
	set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_2_CM1_UTH, 0xff);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_TXFFE_TRAINING_3_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_3_CM2_LTH, 0x4);
	set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_3_CM2_UTH, 0x37);
	set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_3_TXFFE_TRAIN_MOD_TYPE, 0x38);
	wr32_ephy(hw, addr, rdata);

	addr = E56G__RXS0_FOM_18__ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56G__RXS0_FOM_18__DFE_COEFFL_HINT__MSB,
		       E56G__RXS0_FOM_18__DFE_COEFFL_HINT__LSB, 0x0);
	/* change 0x90 to 0x0 to fix 25G link up keep when cable unplugged */
	set_fields_e56(&rdata, E56G__RXS0_FOM_18__DFE_COEFFH_HINT__MSB,
		       E56G__RXS0_FOM_18__DFE_COEFFH_HINT__LSB, 0x0);
	set_fields_e56(&rdata, E56G__RXS0_FOM_18__DFE_COEFF_HINT_LOAD__MSB,
		       E56G__RXS0_FOM_18__DFE_COEFF_HINT_LOAD__LSB, 0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_VGA_TRAINING_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_VGA_TRAINING_0_VGA_TARGET, 0x34);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_VGA_TRAINING_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_VGA_TRAINING_1_VGA1_CODE_INIT0, 0xa);
	set_fields_e56(&rdata, E56PHY_RXS_VGA_TRAINING_1_VGA2_CODE_INIT0, 0xa);
	set_fields_e56(&rdata, E56PHY_RXS_VGA_TRAINING_1_VGA1_CODE_INIT123, 0xa);
	set_fields_e56(&rdata, E56PHY_RXS_VGA_TRAINING_1_VGA2_CODE_INIT123, 0xa);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_CTLE_TRAINING_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_0_CTLE_CODE_INIT0, 0x9);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_0_CTLE_CODE_INIT123, 0x9);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_CTLE_TRAINING_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_1_LFEQ_LUT, 0x1ffffea);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_CTLE_TRAINING_2_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_2_ISI_TH_FRAC_P1,
		       S25G_PHY_RX_CTLE_TAP_FRACP1);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_2_ISI_TH_FRAC_P2,
		       S25G_PHY_RX_CTLE_TAP_FRACP2);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_2_ISI_TH_FRAC_P3,
		       S25G_PHY_RX_CTLE_TAP_FRACP3);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_CTLE_TRAINING_3_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_3_TAP_WEIGHT_P1,
		       S25G_PHY_RX_CTLE_TAPWT_WEIGHT1);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_3_TAP_WEIGHT_P2,
		       S25G_PHY_RX_CTLE_TAPWT_WEIGHT2);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_3_TAP_WEIGHT_P3,
		       S25G_PHY_RX_CTLE_TAPWT_WEIGHT3);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_OFFSET_N_GAIN_CAL_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_OFFSET_N_GAIN_CAL_0_ADC_SLICE_DATA_AVG_CNT,
		       0x3);
	set_fields_e56(&rdata, E56PHY_RXS_OFFSET_N_GAIN_CAL_0_ADC_DATA_AVG_CNT, 0x3);
	set_fields_e56(&rdata, E56PHY_RXS_OFFSET_N_GAIN_CAL_0_FE_OFFSET_DAC_CLK_CNT_X8,
		       0xc);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_OFFSET_N_GAIN_CAL_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_OFFSET_N_GAIN_CAL_1_SAMP_ADAPT_CFG, 0x5);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_FFE_TRAINING_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_FFE_TRAINING_0_FFE_TAP_EN, 0xf9ff);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_IDLE_DETECT_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_IDLE_DETECT_1_IDLE_TH_ADC_PEAK_MAX, 0xa);
	set_fields_e56(&rdata, E56PHY_RXS_IDLE_DETECT_1_IDLE_TH_ADC_PEAK_MIN, 0x5);
	wr32_ephy(hw, addr, rdata);

	txgbe_e56_ephy_config(E56G__RXS3_ANA_OVRDVAL_11, ana_test_adc_clkgen_i, 0x0);
	txgbe_e56_ephy_config(E56G__RXS0_ANA_OVRDEN_2, ovrd_en_ana_test_adc_clkgen_i,
			      0x0);

	addr = E56PHY_RXS_ANA_OVRDVAL_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDVAL_0_ANA_EN_RTERM_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_ANA_OVRDEN_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_0_OVRD_EN_ANA_EN_RTERM_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_ANA_OVRDVAL_6_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 4, 0, 0x0);
	set_fields_e56(&rdata, 14, 13, 0x0);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_ANA_OVRDEN_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_1_OVRD_EN_ANA_BBCDR_VCOFILT_BYP_I,
		       0x1);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_1_OVRD_EN_ANA_TEST_BBCDR_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_ANA_OVRDVAL_15_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 2, 0, 0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_ANA_OVRDVAL_17_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDVAL_17_ANA_VGA2_BOOST_CSTM_I, 0x0);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_ANA_OVRDEN_3_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_3_OVRD_EN_ANA_ANABS_CONFIG_I, 0x1);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_3_OVRD_EN_ANA_VGA2_BOOST_CSTM_I,
		       0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_ANA_OVRDVAL_14_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 13, 13, 0x0);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_ANA_OVRDEN_4_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 13, 13, 0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_EYE_SCAN_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_EYE_SCAN_1_EYE_SCAN_REF_TIMER, 0x400);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_RINGO_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 21, 12, 0x366);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_PMD_CFG_3_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_PMD_CFG_3_CTRL_FSM_TIMEOUT_X64K, 0x80);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_PMD_CFG_4_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_PMD_CFG_4_TRAIN_DC_ON_PERIOD_X64K, 0x18);
	set_fields_e56(&rdata, E56PHY_PMD_CFG_4_TRAIN_DC_PERIOD_X512K, 0x3e);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_PMD_CFG_5_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_PMD_CFG_5_USE_RECENT_MARKER_OFFSET, 0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_0_CONT_ON_ADC_GAIN_CAL_ERR, 0x1);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_0_DO_RX_ADC_OFST_CAL, 0x3);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_0_RX_ERR_ACTION_EN, 0x40);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_1_TRAIN_ST0_WAIT_CNT_X4096, 0xff);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_1_TRAIN_ST1_WAIT_CNT_X4096, 0xff);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_1_TRAIN_ST2_WAIT_CNT_X4096, 0xff);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_1_TRAIN_ST3_WAIT_CNT_X4096, 0xff);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_2_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_2_TRAIN_ST4_WAIT_CNT_X4096, 0x1);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_2_TRAIN_ST5_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_2_TRAIN_ST6_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_2_TRAIN_ST7_WAIT_CNT_X4096, 0x4);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_3_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_3_TRAIN_ST8_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_3_TRAIN_ST9_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_3_TRAIN_ST10_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_3_TRAIN_ST11_WAIT_CNT_X4096, 0x4);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_4_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_4_TRAIN_ST12_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_4_TRAIN_ST13_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_4_TRAIN_ST14_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_4_TRAIN_ST15_WAIT_CNT_X4096, 0x4);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_7_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_7_TRAIN_ST4_EN, 0x4bf);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_7_TRAIN_ST5_EN, 0xc4bf);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_8_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_8_TRAIN_ST7_EN, 0x47ff);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_12_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_12_TRAIN_ST15_EN, 0x67ff);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_13_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_13_TRAIN_ST0_DONE_EN, 0x8001);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_13_TRAIN_ST1_DONE_EN, 0x8002);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_14_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_14_TRAIN_ST3_DONE_EN, 0x8008);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_15_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_15_TRAIN_ST4_DONE_EN, 0x8004);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_17_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_17_TRAIN_ST8_DONE_EN, 0x20c0);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_18_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_18_TRAIN_ST10_DONE_EN, 0x0);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_29_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_29_TRAIN_ST15_DC_EN, 0x3f6d);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_33_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_33_TRAIN0_RATE_SEL, 0x8000);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_33_TRAIN1_RATE_SEL, 0x8000);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_34_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_34_TRAIN2_RATE_SEL, 0x8000);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_34_TRAIN3_RATE_SEL, 0x8000);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_KRT_TFSM_CFG_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_KRT_TFSM_CFGKRT_TFSM_MAX_WAIT_TIMER_X1000K, 0x49);
	set_fields_e56(&rdata, E56PHY_KRT_TFSM_CFGKRT_TFSM_MAX_WAIT_TIMER_X8000K, 0x37);
	set_fields_e56(&rdata, E56PHY_KRT_TFSM_CFGKRT_TFSM_HOLDOFF_TIMER_X256K, 0x2f);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_FETX_FFE_TRAIN_CFG_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_FETX_FFE_TRAIN_CFG_0_KRT_FETX_INIT_FFE_CFG_2,
		       0x2);
	wr32_ephy(hw, addr, rdata);

	return 0;
}

u32
txgbe_e56_cfg_10g(struct txgbe_hw *hw)
{
	u32 addr;
	u32 rdata = 0;

	addr = E56G_CMS_ANA_OVRDVAL_7_ADDR;
	rdata = rd32_ephy(hw, addr);
	((E56G_CMS_ANA_OVRDVAL_7 *)&rdata)->ana_lcpll_lf_vco_swing_ctrl_i = 0xf;
	wr32_ephy(hw, addr, rdata);

	addr = E56G_CMS_ANA_OVRDEN_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	((E56G_CMS_ANA_OVRDEN_1 *)&rdata)->ovrd_en_ana_lcpll_lf_vco_swing_ctrl_i = 0x1;
	wr32_ephy(hw, addr, rdata);

	addr = E56G_CMS_ANA_OVRDVAL_9_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 23, 0, 0x260000);
	wr32_ephy(hw, addr, rdata);

	addr  = E56G_CMS_ANA_OVRDEN_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	((E56G_CMS_ANA_OVRDEN_1 *)&rdata)->ovrd_en_ana_lcpll_lf_test_in_i = 0x1;
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_TXS_TXS_CFG_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_TXS_TXS_CFG_1_ADAPTATION_WAIT_CNT_X256, 0xf);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_TXS_WKUP_CNT_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_TXS_WKUP_CNTLDO_WKUP_CNT_X32, 0xff);
	set_fields_e56(&rdata, E56PHY_TXS_WKUP_CNTDCC_WKUP_CNT_X32, 0xff);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_TXS_PIN_OVRDVAL_6_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 19, 16, 0x6);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_TXS_PIN_OVRDEN_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_TXS_PIN_OVRDEN_0_OVRD_EN_TX0_EFUSE_BITS_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_TXS_ANA_OVRDVAL_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_TXS_ANA_OVRDVAL_1_ANA_TEST_DAC_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_TXS_ANA_OVRDEN_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_TXS_ANA_OVRDEN_0_OVRD_EN_ANA_TEST_DAC_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	/* Setting TX FFE */
	txgbe_e56_tx_ffe_cfg(hw, TXGBE_LINK_SPEED_10GB_FULL);

	addr = E56PHY_RXS_RXS_CFG_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_RXS_CFG_0_DSER_DATA_SEL, 0x0);
	set_fields_e56(&rdata, E56PHY_RXS_RXS_CFG_0_TRAIN_CLK_GATE_BYPASS_EN, 0x1fff);
	wr32_ephy(hw, addr, rdata);

	addr  = E56PHY_RXS_OSC_CAL_N_CDR_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	((E56G_RXS0_OSC_CAL_N_CDR_0 *)&rdata)->prediv0 = 0xfa0;
	((E56G_RXS0_OSC_CAL_N_CDR_0 *)&rdata)->target_cnt0 = 0x203a;
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_OSC_CAL_N_CDR_4_ADDR;
	rdata = rd32_ephy(hw, addr);
	((E56G_RXS0_OSC_CAL_N_CDR_4 *)&rdata)->osc_range_sel0 = 0x2;
	((E56G_RXS0_OSC_CAL_N_CDR_4 *)&rdata)->vco_code_init = 0x7ff;
	((E56G_RXS0_OSC_CAL_N_CDR_4 *)&rdata)->osc_current_boost_en0 = 0x1;
	((E56G_RXS0_OSC_CAL_N_CDR_4 *)&rdata)->bbcdr_current_boost0 = 0x0;
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_OSC_CAL_N_CDR_5_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_5_SDM_WIDTH, 0x3);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_5_BB_CDR_PROP_STEP_PRELOCK,
		       0xf);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_5_BB_CDR_PROP_STEP_POSTLOCK,
		       0xf);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_5_BB_CDR_GAIN_CTRL_POSTLOCK,
		       0xc);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_5_BB_CDR_GAIN_CTRL_PRELOCK,
		       0xf);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_5_BBCDR_RDY_CNT, 0x3);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_OSC_CAL_N_CDR_6_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_6_PI_GAIN_CTRL_PRELOCK, 0x7);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_6_PI_GAIN_CTRL_POSTLOCK, 0x5);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_INTL_CONFIG_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	((E56G_RXS0_INTL_CONFIG_0 *)&rdata)->adc_intl2slice_delay0 = 0x5555;
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_INTL_CONFIG_2_ADDR;
	rdata = rd32_ephy(hw, addr);
	((E56G_RXS0_INTL_CONFIG_2 *)&rdata)->interleaver_hbw_disable0 = 0x1;
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_TXFFE_TRAINING_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_0_ADC_DATA_PEAK_LTH, 0x56);
	set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_0_ADC_DATA_PEAK_UTH, 0x6a);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_TXFFE_TRAINING_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_1_C1_LTH, 0x1e8);
	set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_1_C1_UTH, 0x78);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_TXFFE_TRAINING_2_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_2_CM1_LTH, 0x100);
	set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_2_CM1_UTH, 0xff);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_TXFFE_TRAINING_3_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_3_CM2_LTH, 0x4);
	set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_3_CM2_UTH, 0x37);
	set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_3_TXFFE_TRAIN_MOD_TYPE, 0x38);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_VGA_TRAINING_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_VGA_TRAINING_0_VGA_TARGET, 0x34);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_VGA_TRAINING_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_VGA_TRAINING_1_VGA1_CODE_INIT0, 0xa);
	set_fields_e56(&rdata, E56PHY_RXS_VGA_TRAINING_1_VGA2_CODE_INIT0, 0xa);
	set_fields_e56(&rdata, E56PHY_RXS_VGA_TRAINING_1_VGA1_CODE_INIT123, 0xa);
	set_fields_e56(&rdata, E56PHY_RXS_VGA_TRAINING_1_VGA2_CODE_INIT123, 0xa);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_CTLE_TRAINING_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_0_CTLE_CODE_INIT0, 0x9);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_0_CTLE_CODE_INIT123, 0x9);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_CTLE_TRAINING_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_1_LFEQ_LUT, 0x1ffffea);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_CTLE_TRAINING_2_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_2_ISI_TH_FRAC_P1,
		       S10G_PHY_RX_CTLE_TAP_FRACP1);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_2_ISI_TH_FRAC_P2,
		       S10G_PHY_RX_CTLE_TAP_FRACP2);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_2_ISI_TH_FRAC_P3,
		       S10G_PHY_RX_CTLE_TAP_FRACP3);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_CTLE_TRAINING_3_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_3_TAP_WEIGHT_P1,
		       S10G_PHY_RX_CTLE_TAPWT_WEIGHT1);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_3_TAP_WEIGHT_P2,
		       S10G_PHY_RX_CTLE_TAPWT_WEIGHT2);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_3_TAP_WEIGHT_P3,
		       S10G_PHY_RX_CTLE_TAPWT_WEIGHT3);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_OFFSET_N_GAIN_CAL_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_OFFSET_N_GAIN_CAL_0_ADC_SLICE_DATA_AVG_CNT,
		       0x3);
	set_fields_e56(&rdata, E56PHY_RXS_OFFSET_N_GAIN_CAL_0_ADC_DATA_AVG_CNT, 0x3);
	set_fields_e56(&rdata, E56PHY_RXS_OFFSET_N_GAIN_CAL_0_FE_OFFSET_DAC_CLK_CNT_X8,
		       0xc);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_OFFSET_N_GAIN_CAL_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_OFFSET_N_GAIN_CAL_1_SAMP_ADAPT_CFG, 0x5);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_FFE_TRAINING_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_FFE_TRAINING_0_FFE_TAP_EN, 0xf9ff);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_IDLE_DETECT_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_IDLE_DETECT_1_IDLE_TH_ADC_PEAK_MAX, 0xa);
	set_fields_e56(&rdata, E56PHY_RXS_IDLE_DETECT_1_IDLE_TH_ADC_PEAK_MIN, 0x5);
	wr32_ephy(hw, addr, rdata);

	txgbe_e56_ephy_config(E56G__RXS3_ANA_OVRDVAL_11, ana_test_adc_clkgen_i, 0x0);
	txgbe_e56_ephy_config(E56G__RXS0_ANA_OVRDEN_2, ovrd_en_ana_test_adc_clkgen_i,
			      0x0);

	addr = E56PHY_RXS_ANA_OVRDVAL_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDVAL_0_ANA_EN_RTERM_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_ANA_OVRDEN_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_0_OVRD_EN_ANA_EN_RTERM_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_ANA_OVRDVAL_6_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 4, 0, 0x6);
	set_fields_e56(&rdata, 14, 13, 0x2);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_ANA_OVRDEN_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_1_OVRD_EN_ANA_BBCDR_VCOFILT_BYP_I,
		       0x1);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_1_OVRD_EN_ANA_TEST_BBCDR_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_ANA_OVRDVAL_15_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 2, 0, 0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_ANA_OVRDVAL_17_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDVAL_17_ANA_VGA2_BOOST_CSTM_I, 0x0);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_ANA_OVRDEN_3_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_3_OVRD_EN_ANA_ANABS_CONFIG_I, 0x1);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_3_OVRD_EN_ANA_VGA2_BOOST_CSTM_I,
		       0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_ANA_OVRDVAL_14_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 13, 13, 0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_ANA_OVRDEN_4_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 13, 13, 0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_EYE_SCAN_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_EYE_SCAN_1_EYE_SCAN_REF_TIMER, 0x400);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS_RINGO_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 9, 4, 0x366);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_PMD_CFG_3_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_PMD_CFG_3_CTRL_FSM_TIMEOUT_X64K, 0x80);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_PMD_CFG_4_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_PMD_CFG_4_TRAIN_DC_ON_PERIOD_X64K, 0x18);
	set_fields_e56(&rdata, E56PHY_PMD_CFG_4_TRAIN_DC_PERIOD_X512K, 0x3e);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_PMD_CFG_5_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_PMD_CFG_5_USE_RECENT_MARKER_OFFSET, 0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_0_CONT_ON_ADC_GAIN_CAL_ERR, 0x1);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_0_DO_RX_ADC_OFST_CAL, 0x3);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_0_RX_ERR_ACTION_EN, 0x40);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_1_TRAIN_ST0_WAIT_CNT_X4096, 0xff);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_1_TRAIN_ST1_WAIT_CNT_X4096, 0xff);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_1_TRAIN_ST2_WAIT_CNT_X4096, 0xff);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_1_TRAIN_ST3_WAIT_CNT_X4096, 0xff);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_2_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_2_TRAIN_ST4_WAIT_CNT_X4096, 0x1);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_2_TRAIN_ST5_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_2_TRAIN_ST6_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_2_TRAIN_ST7_WAIT_CNT_X4096, 0x4);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_3_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_3_TRAIN_ST8_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_3_TRAIN_ST9_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_3_TRAIN_ST10_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_3_TRAIN_ST11_WAIT_CNT_X4096, 0x4);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_4_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_4_TRAIN_ST12_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_4_TRAIN_ST13_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_4_TRAIN_ST14_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_4_TRAIN_ST15_WAIT_CNT_X4096, 0x4);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_7_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_7_TRAIN_ST4_EN, 0x4bf);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_7_TRAIN_ST5_EN, 0xc4bf);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_8_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_8_TRAIN_ST7_EN, 0x47ff);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_12_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_12_TRAIN_ST15_EN, 0x67ff);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_13_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_13_TRAIN_ST0_DONE_EN, 0x8001);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_13_TRAIN_ST1_DONE_EN, 0x8002);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_14_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_14_TRAIN_ST3_DONE_EN, 0x8008);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_15_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_15_TRAIN_ST4_DONE_EN, 0x8004);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_17_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_17_TRAIN_ST8_DONE_EN, 0x20c0);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_18_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_18_TRAIN_ST10_DONE_EN, 0x0);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_29_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_29_TRAIN_ST15_DC_EN, 0x3f6d);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_33_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_33_TRAIN0_RATE_SEL, 0x8000);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_33_TRAIN1_RATE_SEL, 0x8000);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_CTRL_FSM_CFG_34_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_34_TRAIN2_RATE_SEL, 0x8000);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_34_TRAIN3_RATE_SEL, 0x8000);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_KRT_TFSM_CFG_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_KRT_TFSM_CFGKRT_TFSM_MAX_WAIT_TIMER_X1000K, 0x49);
	set_fields_e56(&rdata, E56PHY_KRT_TFSM_CFGKRT_TFSM_MAX_WAIT_TIMER_X8000K, 0x37);
	set_fields_e56(&rdata, E56PHY_KRT_TFSM_CFGKRT_TFSM_HOLDOFF_TIMER_X256K, 0x2f);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_FETX_FFE_TRAIN_CFG_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_FETX_FFE_TRAIN_CFG_0_KRT_FETX_INIT_FFE_CFG_2,
		       0x2);
	wr32_ephy(hw, addr, rdata);

	return 0;
}

static inline int
txgbe_e56_rxs_osc_init_for_temp_track_range(struct txgbe_hw *hw, u32 speed)
{
	int status = 0;
	unsigned int addr, rdata, timer;
	int T = 40;
	int RX_COARSE_MID_TD, CMVAR_RANGE_H = 0, CMVAR_RANGE_L = 0;
	int OFFSET_CENTRE_RANGE_H, OFFSET_CENTRE_RANGE_L, RANGE_FINAL;
	int i = 0;
	int lane_num = 1;
	/* 1. Read the temperature T just before RXS is enabled. */
	txgbe_e56_get_temp(hw, &T);

	/*
	 * 2. Define software variable RX_COARSE_MID_TD
	 * (RX Coarse Code mid value dependent upon temperature)
	 */
	if (T < -5)
		RX_COARSE_MID_TD = 10;
	else if (T < 30)
		RX_COARSE_MID_TD = 9;
	else if (T < 65)
		RX_COARSE_MID_TD = 8;
	else if (T < 100)
		RX_COARSE_MID_TD = 7;
	else
		RX_COARSE_MID_TD = 6;

	/* Set CMVAR_RANGE_H/L based on the link speed mode */
	if (speed == TXGBE_LINK_SPEED_10GB_FULL || speed == TXGBE_LINK_SPEED_40GB_FULL) {
		CMVAR_RANGE_H = S10G_CMVAR_RANGE_H;
		CMVAR_RANGE_L = S10G_CMVAR_RANGE_L;
	} else if (speed == TXGBE_LINK_SPEED_25GB_FULL) {
		CMVAR_RANGE_H = S25G_CMVAR_RANGE_H;
		CMVAR_RANGE_L = S25G_CMVAR_RANGE_L;
	}

	if (speed == TXGBE_LINK_SPEED_40GB_FULL)
		lane_num = 4;

	/* 3. Program ALIAS::RXS::RANGE_SEL = CMVAR::RANGE_H */
	for (i = 0; i < lane_num; i++) {
		rdata = 0x0000;
		addr  = E56PHY_RXS_ANA_OVRDVAL_5_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata,
			       E56PHY_RXS_ANA_OVRDVAL_5_ANA_BBCDR_OSC_RANGE_SEL_I, CMVAR_RANGE_H);
		wr32_ephy(hw, addr, rdata);

		rdata = 0x0000;
		addr  = E56PHY_RXS_ANA_OVRDEN_0_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata,
			       E56PHY_RXS_ANA_OVRDEN_0_OVRD_EN_ANA_BBCDR_OSC_RANGE_SEL_I, 0x1);
		wr32_ephy(hw, addr, rdata);

		/*
		 * 4. Do SEQ::RX_ENABLE to enable RXS, and let it stop after oscillator calibration.
		 * This needs to be done by blocking the RX power-up fsm at the state following
		 * the oscillator calibration state.
		 * Follow below steps to do the same before SEQ::RX_ENABLE.
		 * a. ALIAS::PDIG::CTRL_FSM_RX_ST can be stopped at RX_SAMP_CAL_ST which is the
		 * state after RX_OSC_CAL_ST by configuring ALIAS::RXS::SAMP_CAL_DONE=0b0
		 */
		rdata = 0x0000;
		addr = E56PHY_RXS0_OVRDVAL_0_ADDR + (E56PHY_PMD_RX_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_0_RXS0_RX0_SAMP_CAL_DONE_O, 0x0);
		wr32_ephy(hw, addr, rdata);

		rdata = 0x0000;
		addr  = E56PHY_RXS0_OVRDEN_0_ADDR + (E56PHY_PMD_RX_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS0_OVRDEN_0_OVRD_EN_RXS0_RX0_SAMP_CAL_DONE_O, 0x1);
		wr32_ephy(hw, addr, rdata);

		/* Do SEQ::RX_ENABLE to enable RXS */
		rdata = 0;
		addr  = E56PHY_PMD_CFG_0_ADDR;
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_PMD_CFG_0_RX_EN_CFG, (0x1 << i));
		wr32_ephy(hw, addr, rdata);

		/* b. Poll ALIAS::PDIG::CTRL_FSM_RX_ST and confirm its value is RX_SAMP_CAL_ST */
		rdata = 0;
		timer = 0;
		while ((rdata >> (i * 8) & 0x3f) != 0x9) {
			usec_delay(500);
			rdata = 0;
			addr  = E56PHY_INTR_0_ADDR;
			rdata = rd32_ephy(hw, addr);
			if (rdata & (0x100 << i))
				break;

			rdata = 0;
			addr  = E56PHY_CTRL_FSM_RX_STAT_0_ADDR;
			rdata = rd32_ephy(hw, addr);

			if (timer++ > PHYINIT_TIMEOUT) {
				DEBUGOUT("ERROR: Wait E56PHY_CTRL_FSM_RX_STAT_0_ADDR Timeout!\n");
				return -1;
			}
		}

		/* 5/6.Define software variable as OFFSET_CENTRE_RANGE_H = ALIAS::RXS::COARSE */
		rdata = 0;
		addr  = E56PHY_RXS_ANA_OVRDVAL_5_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		OFFSET_CENTRE_RANGE_H = (rdata >> 4) & 0xf;
		if (OFFSET_CENTRE_RANGE_H > RX_COARSE_MID_TD)
			OFFSET_CENTRE_RANGE_H = OFFSET_CENTRE_RANGE_H - RX_COARSE_MID_TD;
		else
			OFFSET_CENTRE_RANGE_H = RX_COARSE_MID_TD - OFFSET_CENTRE_RANGE_H;

		/*
		 * 7. Do SEQ::RX_DISABLE to disable RXS.
		 * Poll ALIAS::PDIG::CTRL_FSM_RX_ST and confirm.
		 */
		rdata = 0;
		addr  = E56PHY_PMD_CFG_0_ADDR;
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_PMD_CFG_0_RX_EN_CFG, 0x0);
		wr32_ephy(hw, addr, rdata);

		timer = 0;
		while (1) {
			usec_delay(500);
			rdata = 0;
			addr  = E56PHY_CTRL_FSM_RX_STAT_0_ADDR;
			rdata = rd32_ephy(hw, addr);
			if (((rdata >> (i * 8)) & 0x3f) == 0x21)
				break;
			if (timer++ > PHYINIT_TIMEOUT) {
				DEBUGOUT("ERROR: Wait E56PHY_CTRL_FSM_RX_STAT_0_ADDR Timeout!\n");
				return -1;
			}
		}

		/*
		 * 8. Since RX power-up fsm is stopped in RX_SAMP_CAL_ST,
		 * it is possible the timeout interrupt is set.
		 * Clear the same by clearing ALIAS::PDIG::INTR_CTRL_FSM_RX_ERR.
		 * Also clear ALIAS::PDIG::INTR_RX_OSC_FREQ_ERR which could also be set.
		 */
		usec_delay(500);
		rdata = 0;
		addr  = E56PHY_INTR_0_ADDR;
		rdata = rd32_ephy(hw, addr);

		usec_delay(500);
		addr  = E56PHY_INTR_0_ADDR;
		wr32_ephy(hw, addr, rdata);

		usec_delay(500);
		rdata = 0;
		addr  = E56PHY_INTR_0_ADDR;
		rdata = rd32_ephy(hw, addr);

		/* 9. Program ALIAS::RXS::RANGE_SEL = CMVAR::RANGE_L */
		rdata = 0x0000;
		addr  = E56PHY_RXS_ANA_OVRDVAL_5_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata,
			       E56PHY_RXS_ANA_OVRDVAL_5_ANA_BBCDR_OSC_RANGE_SEL_I, CMVAR_RANGE_L);
		wr32_ephy(hw, addr, rdata);

		rdata = 0x0000;
		addr  = E56PHY_RXS_ANA_OVRDEN_0_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata,
			       E56PHY_RXS_ANA_OVRDEN_0_OVRD_EN_ANA_BBCDR_OSC_RANGE_SEL_I, 0x1);
		wr32_ephy(hw, addr, rdata);

		/*
		 * 10. Do SEQ::RX_ENABLE to enable RXS,
		 * and let it stop after oscillator calibration.
		 */
		rdata = 0x0000;
		addr  = E56PHY_RXS0_OVRDVAL_0_ADDR + (E56PHY_PMD_RX_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_0_RXS0_RX0_SAMP_CAL_DONE_O, 0x0);
		wr32_ephy(hw, addr, rdata);

		rdata = 0x0000;
		addr  = E56PHY_RXS0_OVRDEN_0_ADDR + (E56PHY_PMD_RX_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS0_OVRDEN_0_OVRD_EN_RXS0_RX0_SAMP_CAL_DONE_O, 0x1);
		wr32_ephy(hw, addr, rdata);

		rdata = 0;
		addr  = E56PHY_PMD_CFG_0_ADDR;
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_PMD_CFG_0_RX_EN_CFG, (0x1 << i));
		wr32_ephy(hw, addr, rdata);

		/* poll CTRL_FSM_RX_ST */
		timer = 0;
		while (((rdata >> (i * 8)) & 0x3f) != 0x9) {
			usec_delay(500);
			rdata = 0;
			addr  = E56PHY_INTR_0_ADDR;
			rdata = rd32_ephy(hw, addr);
			if ((rdata & 0x100) == 0x100)
				break;

			rdata = 0;
			addr  = E56PHY_CTRL_FSM_RX_STAT_0_ADDR;
			rdata = rd32_ephy(hw, addr);
			if (timer++ > PHYINIT_TIMEOUT) {
				DEBUGOUT("ERROR: Wait E56PHY_CTRL_FSM_RX_STAT_0_ADDR Timeout!\n");
				return -1;
			}
		}

		/*
		 * 11/12.Define software variable as OFFSET_CENTRE_RANGE_L = ALIAS::RXS::COARSE -
		 * RX_COARSE_MID_TD. Clear the INTR.
		 */
		rdata = 0;
		addr  = E56PHY_RXS_ANA_OVRDVAL_5_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		OFFSET_CENTRE_RANGE_L = (rdata >> 4) & 0xf;
		if (OFFSET_CENTRE_RANGE_L > RX_COARSE_MID_TD)
			OFFSET_CENTRE_RANGE_L = OFFSET_CENTRE_RANGE_L - RX_COARSE_MID_TD;
		else
			OFFSET_CENTRE_RANGE_L = RX_COARSE_MID_TD - OFFSET_CENTRE_RANGE_L;

		/*
		 * 13. Perform below calculation in software. Goal is to pick range value
		 * which is closer to RX_COARSE_MID_TD.
		 */
		if (OFFSET_CENTRE_RANGE_L < OFFSET_CENTRE_RANGE_H)
			RANGE_FINAL = CMVAR_RANGE_L;
		else
			RANGE_FINAL = CMVAR_RANGE_H;

		/*
		 * 14. Do SEQ::RX_DISABLE to disable RXS. Poll ALIAS::PDIG::CTRL_FSM_RX_ST
		 * and confirm its value is POWERDN_ST
		 */
		rdata = 0;
		addr  = E56PHY_PMD_CFG_0_ADDR;
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_PMD_CFG_0_RX_EN_CFG, 0x0);
		wr32_ephy(hw, addr, rdata);

		timer = 0;
		while (1) {
			usec_delay(500);
			rdata = 0;
			addr  = E56PHY_CTRL_FSM_RX_STAT_0_ADDR;
			rdata = rd32_ephy(hw, addr);
			if (((rdata  >> (i * 8)) & 0x3f) == 0x21)
				break;
			if (timer++ > PHYINIT_TIMEOUT) {
				DEBUGOUT("ERROR: Wait E56PHY_CTRL_FSM_RX_STAT_0_ADDR Timeout!\n");
				return -1;
			}
		}

		/*
		 * 15. Since RX power-up fsm is stopped in RX_SAMP_CAL_ST,
		 * it is possible the timeout interrupt is set. Clear the same by clearing
		 * ALIAS::PDIG::INTR_CTRL_FSM_RX_ERR. Also clear ALIAS::PDIG::INTR_RX_OSC_FREQ_ERR
		 * which could also be set.
		 */
		usec_delay(500);
		rdata = 0;
		addr  = E56PHY_INTR_0_ADDR;
		rdata = rd32_ephy(hw, addr);
		usec_delay(500);
		wr32_ephy(hw, addr, rdata);

		usec_delay(500);
		rdata = 0;
		addr  = E56PHY_INTR_0_ADDR;
		rdata = rd32_ephy(hw, addr);

		/* 16. Program ALIAS::RXS::RANGE_SEL = RANGE_FINAL */
		rdata = 0x0000;
		addr  = E56PHY_RXS_ANA_OVRDVAL_5_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata,
			       E56PHY_RXS_ANA_OVRDVAL_5_ANA_BBCDR_OSC_RANGE_SEL_I, RANGE_FINAL);
		wr32_ephy(hw, addr, rdata);

		/*
		 * 17. Program following before enabling RXS. Purpose is to disable power-up
		 * FSM control on ADC offset adaptation.
		 * Note: this step will be done in 2.3.3 RXS calibration and adaptation sequence
		 * 18. After this SEQ::RX_ENABLE can be done at any time. Note to ensure that
		 * ALIAS::RXS::RANGE_SEL = RANGE_FINAL configuration is retained.
		 * Rmove the OVRDEN on rxs0_rx0_samp_cal_done_o
		 */
		rdata = 0x0000;
		addr  = E56PHY_RXS0_OVRDEN_0_ADDR + (E56PHY_PMD_RX_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS0_OVRDEN_0_OVRD_EN_RXS0_RX0_SAMP_CAL_DONE_O, 0x0);
		wr32_ephy(hw, addr, rdata);
	}

	rdata = 0;
	addr  = E56PHY_PMD_CFG_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	if (speed == TXGBE_LINK_SPEED_40GB_FULL)
		set_fields_e56(&rdata, E56PHY_PMD_CFG_0_RX_EN_CFG, 0xf);
	else
		set_fields_e56(&rdata, E56PHY_PMD_CFG_0_RX_EN_CFG, 0x1);
	wr32_ephy(hw, addr, rdata);

	return status;
}

static int txgbe_e56_set_rxs_ufine_le_max_40g(struct txgbe_hw *hw, u32 speed)
{
	int status = 0;
	unsigned int rdata;
	unsigned int ULTRAFINE_CODE;
	int i = 0;
	unsigned int CMVAR_UFINE_MAX = 0;
	u32 addr;

	for (i = 0; i < 4; i++) {
		if (speed == TXGBE_LINK_SPEED_10GB_FULL || speed == TXGBE_LINK_SPEED_40GB_FULL)
			CMVAR_UFINE_MAX = S10G_CMVAR_UFINE_MAX;
		else if (speed == TXGBE_LINK_SPEED_25GB_FULL)
			CMVAR_UFINE_MAX = S25G_CMVAR_UFINE_MAX;

		/* a. Assign software defined variables as below */
		addr = E56G__RXS0_ANA_OVRDVAL_5_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		ULTRAFINE_CODE = EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5, ana_bbcdr_ultrafine_i);

		/* b. Perform the below logic sequence */
		while (ULTRAFINE_CODE > CMVAR_UFINE_MAX) {
			ULTRAFINE_CODE = ULTRAFINE_CODE - 1;
			addr = E56G__RXS0_ANA_OVRDVAL_5_ADDR + (E56PHY_RXS_OFFSET * i);
			rdata = rd32_ephy(hw, addr);
			EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5, ana_bbcdr_ultrafine_i) = ULTRAFINE_CODE;
			wr32_ephy(hw, addr, rdata);

			addr = E56G__RXS0_ANA_OVRDEN_1_ADDR + (E56PHY_RXS_OFFSET * i);
			rdata = rd32_ephy(hw, addr);
			EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_ultrafine_i) = 1;
			wr32_ephy(hw, addr, rdata);

			/* Wait until 1milliseconds or greater */
			msleep(10);
		}
	}
	return status;
}

static inline
int txgbe_e56_set_rxs_ufine_le_max(struct txgbe_hw *hw, u32 speed)
{
	int status = 0;
	unsigned int rdata;
	unsigned int ULTRAFINE_CODE;

	unsigned int CMVAR_UFINE_MAX = 0;

	if (speed == TXGBE_LINK_SPEED_10GB_FULL)
		CMVAR_UFINE_MAX = S10G_CMVAR_UFINE_MAX;
	else if (speed == TXGBE_LINK_SPEED_25GB_FULL)
		CMVAR_UFINE_MAX = S25G_CMVAR_UFINE_MAX;

	/* a. Assign software defined variables as below */
	/* ii. ULTRAFINE_CODE = ALIAS::RXS::ULTRAFINE */
	EPHY_RREG(E56G__RXS0_ANA_OVRDVAL_5);
	ULTRAFINE_CODE = EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5, ana_bbcdr_ultrafine_i);

	/* b. Perform the below logic sequence */
	while (ULTRAFINE_CODE > CMVAR_UFINE_MAX) {
		ULTRAFINE_CODE = ULTRAFINE_CODE - 1;
		txgbe_e56_ephy_config(E56G__RXS0_ANA_OVRDVAL_5, ana_bbcdr_ultrafine_i,
				      ULTRAFINE_CODE);
		/* Set ovrd_en=1 to override ASIC value */
		txgbe_e56_ephy_config(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_ultrafine_i,
				      1);
		/*  Wait until 1milliseconds or greater */
		msleep(10);
	}

	return status;
}

#define RXS_READ_COUNT 5

void txgbe_e56_rx_rd_second_code_40g(struct txgbe_hw *hw, int *SECOND_CODE, int lane)
{
	int i, median;
	unsigned int rdata;
	u32 addr;
	int RXS_BBCDR_SECOND_ORDER_ST[RXS_READ_COUNT];

	/* Set ovrd_en=0 to read ASIC value */
	addr = E56G__RXS0_ANA_OVRDEN_1_ADDR + (lane *  E56PHY_RXS_OFFSET);
	rdata = rd32_ephy(hw, addr);
	EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_int_cstm_i) = 0;
	wr32_ephy(hw, addr, rdata);

	/*
	 * As status update from RXS hardware is asynchronous to read status of SECOND_ORDER,
	 * follow sequence mentioned below.
	 */
	for (i = 0; i < RXS_READ_COUNT; i = i + 1) {
		addr = E56G__RXS0_ANA_OVRDVAL_5_ADDR + (lane *  E56PHY_RXS_OFFSET);
		rdata = rd32_ephy(hw, addr);
		RXS_BBCDR_SECOND_ORDER_ST[i] = EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5,
							 ana_bbcdr_int_cstm_i);
		usec_delay(100);
	}

	/* sort array RXS_BBCDR_SECOND_ORDER_ST[i] */
	qsort(RXS_BBCDR_SECOND_ORDER_ST, RXS_READ_COUNT, sizeof(int), txgbe_e56_int_cmp);

	median = ((RXS_READ_COUNT + 1) / 2) - 1;
	*SECOND_CODE = RXS_BBCDR_SECOND_ORDER_ST[median];

	return;
}

void txgbe_e56_rx_rd_second_code(struct txgbe_hw *hw, int *SECOND_CODE)
{
	int i, median;
	unsigned int rdata;
	int RXS_BBCDR_SECOND_ORDER_ST[RXS_READ_COUNT];

	/* Set ovrd_en=0 to read ASIC value */
	txgbe_e56_ephy_config(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_int_cstm_i, 0);

	/*
	 * As status update from RXS hardware is asynchronous to read status
	 * of SECOND_ORDER, follow sequence mentioned below.
	 */
	for (i = 0; i < RXS_READ_COUNT; i = i + 1) {
		EPHY_RREG(E56G__RXS0_ANA_OVRDVAL_5);
		RXS_BBCDR_SECOND_ORDER_ST[i] = EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5,
					       ana_bbcdr_int_cstm_i);
		usec_delay(100);
	}

	/* sort array RXS_BBCDR_SECOND_ORDER_ST[i] */
	qsort(RXS_BBCDR_SECOND_ORDER_ST, RXS_READ_COUNT, sizeof(int), txgbe_e56_int_cmp);

	median = ((RXS_READ_COUNT + 1) / 2) - 1;
	*SECOND_CODE = RXS_BBCDR_SECOND_ORDER_ST[median];

	return;
}

/*
 * 2.3.4 RXS post CDR lock temperature tracking sequence
 *
 * Below sequence must be run before the temperature drifts by >5degC
 * after the CDR locks for the first time or after the ious time this
 * sequence was run. It is recommended to call this sequence periodically
 * (eg: once every 100ms) or trigger sequence if the temperature drifts
 * by >=5degC. Temperature must be read from an on-die temperature sensor.
 */
int txgbe_temp_track_seq_40g(struct txgbe_hw *hw, u32 speed)
{
	int status = 0;
	unsigned int rdata;
	int SECOND_CODE;
	int COARSE_CODE;
	int FINE_CODE;
	int ULTRAFINE_CODE;

	int CMVAR_SEC_LOW_TH;
	int CMVAR_UFINE_MAX = 0;
	int CMVAR_FINE_MAX;
	int CMVAR_UFINE_UMAX_WRAP = 0;
	int CMVAR_COARSE_MAX;
	int CMVAR_UFINE_FMAX_WRAP = 0;
	int CMVAR_FINE_FMAX_WRAP = 0;
	int CMVAR_SEC_HIGH_TH;
	int CMVAR_UFINE_MIN;
	int CMVAR_FINE_MIN;
	int CMVAR_UFINE_UMIN_WRAP;
	int CMVAR_COARSE_MIN;
	int CMVAR_UFINE_FMIN_WRAP;
	int CMVAR_FINE_FMIN_WRAP;
	int i;
	u32 addr;
	int temperature;

	for (i = 0; i < 4; i++) {
		if (speed == TXGBE_LINK_SPEED_10GB_FULL || speed == TXGBE_LINK_SPEED_40GB_FULL) {
			CMVAR_SEC_LOW_TH = S10G_CMVAR_SEC_LOW_TH;
			CMVAR_UFINE_MAX = S10G_CMVAR_UFINE_MAX;
			CMVAR_FINE_MAX = S10G_CMVAR_FINE_MAX;
			CMVAR_UFINE_UMAX_WRAP = S10G_CMVAR_UFINE_UMAX_WRAP;
			CMVAR_COARSE_MAX = S10G_CMVAR_COARSE_MAX;
			CMVAR_UFINE_FMAX_WRAP = S10G_CMVAR_UFINE_FMAX_WRAP;
			CMVAR_FINE_FMAX_WRAP = S10G_CMVAR_FINE_FMAX_WRAP;
			CMVAR_SEC_HIGH_TH = S10G_CMVAR_SEC_HIGH_TH;
			CMVAR_UFINE_MIN = S10G_CMVAR_UFINE_MIN;
			CMVAR_FINE_MIN = S10G_CMVAR_FINE_MIN;
			CMVAR_UFINE_UMIN_WRAP = S10G_CMVAR_UFINE_UMIN_WRAP;
			CMVAR_COARSE_MIN = S10G_CMVAR_COARSE_MIN;
			CMVAR_UFINE_FMIN_WRAP = S10G_CMVAR_UFINE_FMIN_WRAP;
			CMVAR_FINE_FMIN_WRAP = S10G_CMVAR_FINE_FMIN_WRAP;
		} else if (speed == TXGBE_LINK_SPEED_25GB_FULL) {
			CMVAR_SEC_LOW_TH = S25G_CMVAR_SEC_LOW_TH;
			CMVAR_UFINE_MAX = S25G_CMVAR_UFINE_MAX;
			CMVAR_FINE_MAX = S25G_CMVAR_FINE_MAX;
			CMVAR_UFINE_UMAX_WRAP = S25G_CMVAR_UFINE_UMAX_WRAP;
			CMVAR_COARSE_MAX = S25G_CMVAR_COARSE_MAX;
			CMVAR_UFINE_FMAX_WRAP = S25G_CMVAR_UFINE_FMAX_WRAP;
			CMVAR_FINE_FMAX_WRAP = S25G_CMVAR_FINE_FMAX_WRAP;
			CMVAR_SEC_HIGH_TH = S25G_CMVAR_SEC_HIGH_TH;
			CMVAR_UFINE_MIN = S25G_CMVAR_UFINE_MIN;
			CMVAR_FINE_MIN = S25G_CMVAR_FINE_MIN;
			CMVAR_UFINE_UMIN_WRAP = S25G_CMVAR_UFINE_UMIN_WRAP;
			CMVAR_COARSE_MIN = S25G_CMVAR_COARSE_MIN;
			CMVAR_UFINE_FMIN_WRAP = S25G_CMVAR_UFINE_FMIN_WRAP;
			CMVAR_FINE_FMIN_WRAP = S25G_CMVAR_FINE_FMIN_WRAP;
		} else {
			DEBUGOUT("Error Speed\n");
			return 0;
		}

		status = txgbe_e56_get_temp(hw, &temperature);
		if (status)
			return 0;

		hw->temperature = temperature;

		/* Assign software defined variables as below */
		/* a. SECOND_CODE = ALIAS::RXS::SECOND_ORDER */
		txgbe_e56_rx_rd_second_code_40g(hw, &SECOND_CODE, i);

		/*
		 * b. COARSE_CODE = ALIAS::RXS::COARSE
		 * c. FINE_CODE = ALIAS::RXS::FINE
		 * d. ULTRAFINE_CODE = ALIAS::RXS::ULTRAFINE
		 */
		addr = E56G__RXS0_ANA_OVRDVAL_5_ADDR + (E56PHY_RXS_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		COARSE_CODE = EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5, ana_bbcdr_coarse_i);
		FINE_CODE = EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5, ana_bbcdr_fine_i);
		ULTRAFINE_CODE = EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5, ana_bbcdr_ultrafine_i);

		if (SECOND_CODE <= CMVAR_SEC_LOW_TH) {
			if (ULTRAFINE_CODE < CMVAR_UFINE_MAX) {
				addr = E56G__RXS0_ANA_OVRDVAL_5_ADDR + (E56PHY_RXS_OFFSET * i);
				rdata = rd32_ephy(hw, addr);
				EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5,
					  ana_bbcdr_ultrafine_i) = ULTRAFINE_CODE + 1;
				wr32_ephy(hw, addr, rdata);

				/* Set ovrd_en=1 to override ASIC value */
				addr = E56G__RXS0_ANA_OVRDEN_1_ADDR + (E56PHY_RXS_OFFSET * i);
				rdata = rd32_ephy(hw, addr);
				EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1,
					  ovrd_en_ana_bbcdr_ultrafine_i) = 1;
				wr32_ephy(hw, addr, rdata);
			} else if (FINE_CODE < CMVAR_FINE_MAX) {
				addr = E56G__RXS0_ANA_OVRDVAL_5_ADDR + (E56PHY_RXS_OFFSET * i);
				rdata = rd32_ephy(hw, addr);
				EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5,
					  ana_bbcdr_ultrafine_i) = CMVAR_UFINE_UMAX_WRAP;
				EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5,
					  ana_bbcdr_fine_i) = FINE_CODE + 1;
				wr32_ephy(hw, addr, rdata);
				addr = E56G__RXS0_ANA_OVRDEN_1_ADDR + (E56PHY_RXS_OFFSET * i);
				rdata = rd32_ephy(hw, addr);
				EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_fine_i) = 1;
				EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1,
					  ovrd_en_ana_bbcdr_ultrafine_i) = 1;
				wr32_ephy(hw, addr, rdata);
			} else if (COARSE_CODE < CMVAR_COARSE_MAX) {
				addr = E56G__RXS0_ANA_OVRDVAL_5_ADDR + (E56PHY_RXS_OFFSET * i);
				rdata = rd32_ephy(hw, addr);
				EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5,
					  ana_bbcdr_ultrafine_i) = CMVAR_UFINE_FMAX_WRAP;
				EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5,
					  ana_bbcdr_fine_i) = CMVAR_FINE_FMAX_WRAP;
				EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5,
					  ana_bbcdr_coarse_i) = COARSE_CODE + 1;
				wr32_ephy(hw, addr, rdata);

				addr = E56G__RXS0_ANA_OVRDEN_1_ADDR + (E56PHY_RXS_OFFSET * i);
				rdata = rd32_ephy(hw, addr);
				EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_coarse_i) = 1;
				EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_fine_i) = 1;
				EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1,
					  ovrd_en_ana_bbcdr_ultrafine_i) = 1;
				wr32_ephy(hw, addr, rdata);
			} else {
				DEBUGOUT("ERROR: (SECOND_CODE <= CMVAR_SEC_LOW_TH) temperature "
					 "tracking occurs Error condition");
			}
		} else if (SECOND_CODE >= CMVAR_SEC_HIGH_TH) {
			if (ULTRAFINE_CODE > CMVAR_UFINE_MIN) {
				addr = E56G__RXS0_ANA_OVRDVAL_5_ADDR + (E56PHY_RXS_OFFSET * i);
				rdata = rd32_ephy(hw, addr);
				EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5,
					  ana_bbcdr_ultrafine_i) = ULTRAFINE_CODE - 1;
				wr32_ephy(hw, addr, rdata);

				addr = E56G__RXS0_ANA_OVRDEN_1_ADDR + (E56PHY_RXS_OFFSET * i);
				rdata = rd32_ephy(hw, addr);
				EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1,
					  ovrd_en_ana_bbcdr_ultrafine_i) = 1;
				wr32_ephy(hw, addr, rdata);
			} else if (FINE_CODE > CMVAR_FINE_MIN) {
				addr = E56G__RXS0_ANA_OVRDVAL_5_ADDR + (E56PHY_RXS_OFFSET * i);
				rdata = rd32_ephy(hw, addr);
				EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5,
					  ana_bbcdr_ultrafine_i) = CMVAR_UFINE_UMIN_WRAP;
				EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5,
					  ana_bbcdr_fine_i) = FINE_CODE - 1;
				wr32_ephy(hw, addr, rdata);

				addr = E56G__RXS0_ANA_OVRDEN_1_ADDR + (E56PHY_RXS_OFFSET * i);
				rdata = rd32_ephy(hw, addr);
				EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_fine_i) = 1;
				EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1,
					  ovrd_en_ana_bbcdr_ultrafine_i) = 1;
				wr32_ephy(hw, addr, rdata);
			} else if (COARSE_CODE > CMVAR_COARSE_MIN) {
				addr = E56G__RXS0_ANA_OVRDVAL_5_ADDR + (E56PHY_RXS_OFFSET * i);
				rdata = rd32_ephy(hw, addr);
				EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5,
					  ana_bbcdr_ultrafine_i) = CMVAR_UFINE_FMIN_WRAP;
				EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5,
					  ana_bbcdr_fine_i) = CMVAR_FINE_FMIN_WRAP;
				EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5,
					  ana_bbcdr_coarse_i) = COARSE_CODE - 1;
				wr32_ephy(hw, addr, rdata);

				addr = E56G__RXS0_ANA_OVRDEN_1_ADDR + (E56PHY_RXS_OFFSET * i);
				rdata = rd32_ephy(hw, addr);
				EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_coarse_i) = 1;
				EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_fine_i) = 1;
				EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1,
					  ovrd_en_ana_bbcdr_ultrafine_i) = 1;
				wr32_ephy(hw, addr, rdata);
			} else {
				DEBUGOUT("ERROR: (SECOND_CODE >= CMVAR_SEC_HIGH_TH) temperature "
					 "tracking occurs Error condition");
			}
		}
	}

	return status;
}

/*
 * 2.3.4 RXS post CDR lock temperature tracking sequence
 *
 * Below sequence must be run before the temperature drifts by >5degC
 * after the CDR locks for the first time or after the ious time this
 * sequence was run. It is recommended to call this sequence periodically
 * (eg: once every 100ms) or trigger sequence if the temperature drifts
 * by >=5degC. Temperature must be read from an on-die temperature sensor.
 */

int txgbe_temp_track_seq(struct txgbe_hw *hw, u32 speed)
{
	int status = 0;
	unsigned int rdata;
	int SECOND_CODE;
	int COARSE_CODE;
	int FINE_CODE;
	int ULTRAFINE_CODE;

	int CMVAR_SEC_LOW_TH;
	int CMVAR_UFINE_MAX = 0;
	int CMVAR_FINE_MAX;
	int CMVAR_UFINE_UMAX_WRAP = 0;
	int CMVAR_COARSE_MAX;
	int CMVAR_UFINE_FMAX_WRAP = 0;
	int CMVAR_FINE_FMAX_WRAP = 0;
	int CMVAR_SEC_HIGH_TH;
	int CMVAR_UFINE_MIN;
	int CMVAR_FINE_MIN;
	int CMVAR_UFINE_UMIN_WRAP;
	int CMVAR_COARSE_MIN;
	int CMVAR_UFINE_FMIN_WRAP;
	int CMVAR_FINE_FMIN_WRAP;
	int temperature;

	if (speed == TXGBE_LINK_SPEED_10GB_FULL) {
		CMVAR_SEC_LOW_TH = S10G_CMVAR_SEC_LOW_TH;
		CMVAR_UFINE_MAX = S10G_CMVAR_UFINE_MAX;
		CMVAR_FINE_MAX = S10G_CMVAR_FINE_MAX;
		CMVAR_UFINE_UMAX_WRAP = S10G_CMVAR_UFINE_UMAX_WRAP;
		CMVAR_COARSE_MAX = S10G_CMVAR_COARSE_MAX;
		CMVAR_UFINE_FMAX_WRAP = S10G_CMVAR_UFINE_FMAX_WRAP;
		CMVAR_FINE_FMAX_WRAP = S10G_CMVAR_FINE_FMAX_WRAP;
		CMVAR_SEC_HIGH_TH = S10G_CMVAR_SEC_HIGH_TH;
		CMVAR_UFINE_MIN = S10G_CMVAR_UFINE_MIN;
		CMVAR_FINE_MIN = S10G_CMVAR_FINE_MIN;
		CMVAR_UFINE_UMIN_WRAP = S10G_CMVAR_UFINE_UMIN_WRAP;
		CMVAR_COARSE_MIN = S10G_CMVAR_COARSE_MIN;
		CMVAR_UFINE_FMIN_WRAP = S10G_CMVAR_UFINE_FMIN_WRAP;
		CMVAR_FINE_FMIN_WRAP = S10G_CMVAR_FINE_FMIN_WRAP;
	} else if (speed == TXGBE_LINK_SPEED_25GB_FULL) {
		CMVAR_SEC_LOW_TH = S25G_CMVAR_SEC_LOW_TH;
		CMVAR_UFINE_MAX = S25G_CMVAR_UFINE_MAX;
		CMVAR_FINE_MAX = S25G_CMVAR_FINE_MAX;
		CMVAR_UFINE_UMAX_WRAP = S25G_CMVAR_UFINE_UMAX_WRAP;
		CMVAR_COARSE_MAX = S25G_CMVAR_COARSE_MAX;
		CMVAR_UFINE_FMAX_WRAP = S25G_CMVAR_UFINE_FMAX_WRAP;
		CMVAR_FINE_FMAX_WRAP = S25G_CMVAR_FINE_FMAX_WRAP;
		CMVAR_SEC_HIGH_TH = S25G_CMVAR_SEC_HIGH_TH;
		CMVAR_UFINE_MIN = S25G_CMVAR_UFINE_MIN;
		CMVAR_FINE_MIN = S25G_CMVAR_FINE_MIN;
		CMVAR_UFINE_UMIN_WRAP = S25G_CMVAR_UFINE_UMIN_WRAP;
		CMVAR_COARSE_MIN = S25G_CMVAR_COARSE_MIN;
		CMVAR_UFINE_FMIN_WRAP = S25G_CMVAR_UFINE_FMIN_WRAP;
		CMVAR_FINE_FMIN_WRAP = S25G_CMVAR_FINE_FMIN_WRAP;
	} else {
		PMD_DRV_LOG(ERR, "Error Speed");
		return 0;
	}

	status = txgbe_e56_get_temp(hw, &temperature);
	if (status)
		return 0;

	hw->temperature = temperature;

	/*
	 * Assign software defined variables as below
	 * a. SECOND_CODE = ALIAS::RXS::SECOND_ORDER
	 */
	txgbe_e56_rx_rd_second_code(hw, &SECOND_CODE);

	/*
	 * b. COARSE_CODE = ALIAS::RXS::COARSE
	 * c. FINE_CODE = ALIAS::RXS::FINE
	 * d. ULTRAFINE_CODE = ALIAS::RXS::ULTRAFINE
	 */
	EPHY_RREG(E56G__RXS0_ANA_OVRDVAL_5);
	COARSE_CODE = EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5, ana_bbcdr_coarse_i);
	FINE_CODE = EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5, ana_bbcdr_fine_i);
	ULTRAFINE_CODE = EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5, ana_bbcdr_ultrafine_i);

	if (SECOND_CODE <= CMVAR_SEC_LOW_TH) {
		if (ULTRAFINE_CODE < CMVAR_UFINE_MAX) {
			txgbe_e56_ephy_config(E56G__RXS0_ANA_OVRDVAL_5, ana_bbcdr_ultrafine_i,
					      ULTRAFINE_CODE + 1);
			/* Set ovrd_en=1 to override ASIC value */
			EPHY_RREG(E56G__RXS0_ANA_OVRDEN_1);
			EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_ultrafine_i) = 1;
			EPHY_WREG(E56G__RXS0_ANA_OVRDEN_1);
		} else if (FINE_CODE < CMVAR_FINE_MAX) {
			EPHY_RREG(E56G__RXS0_ANA_OVRDVAL_5);
			EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5,
				  ana_bbcdr_ultrafine_i) = CMVAR_UFINE_UMAX_WRAP;
			EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5, ana_bbcdr_fine_i) = FINE_CODE + 1;
			EPHY_WREG(E56G__RXS0_ANA_OVRDVAL_5);
			/*
			 * Note: All two of above code updates should be written
			 * in a single register write
			 * Set ovrd_en=1 to override ASIC value
			 */
			EPHY_RREG(E56G__RXS0_ANA_OVRDEN_1);
			EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_fine_i) = 1;
			EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_ultrafine_i) = 1;
			EPHY_WREG(E56G__RXS0_ANA_OVRDEN_1);
		} else if (COARSE_CODE < CMVAR_COARSE_MAX) {
			EPHY_RREG(E56G__RXS0_ANA_OVRDVAL_5);
			EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5,
				  ana_bbcdr_ultrafine_i) = CMVAR_UFINE_FMAX_WRAP;
			EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5,
				  ana_bbcdr_fine_i) = CMVAR_FINE_FMAX_WRAP;
			EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5, ana_bbcdr_coarse_i) = COARSE_CODE + 1;
			EPHY_WREG(E56G__RXS0_ANA_OVRDVAL_5);
			/*
			 * Note: All three of above code updates should be written
			 * in a single register write
			 * Set ovrd_en=1 to override ASIC value
			 */
			EPHY_RREG(E56G__RXS0_ANA_OVRDEN_1);
			EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_coarse_i) = 1;
			EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_fine_i) = 1;
			EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_ultrafine_i) = 1;
			EPHY_WREG(E56G__RXS0_ANA_OVRDEN_1);
		} else {
			PMD_DRV_LOG(ERR, "ERROR: (SECOND_CODE <= CMVAR_SEC_LOW_TH) temperature "
				    "tracking occurs Error condition");
		}
	} else if (SECOND_CODE >= CMVAR_SEC_HIGH_TH) {
		if (ULTRAFINE_CODE > CMVAR_UFINE_MIN) {
			txgbe_e56_ephy_config(E56G__RXS0_ANA_OVRDVAL_5, ana_bbcdr_ultrafine_i,
					      ULTRAFINE_CODE - 1);
			EPHY_RREG(E56G__RXS0_ANA_OVRDEN_1);
			EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_ultrafine_i) = 1;
			EPHY_WREG(E56G__RXS0_ANA_OVRDEN_1);
		} else if (FINE_CODE > CMVAR_FINE_MIN) {
			EPHY_RREG(E56G__RXS0_ANA_OVRDVAL_5);
			EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5,
				  ana_bbcdr_ultrafine_i) = CMVAR_UFINE_UMIN_WRAP;
			EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5, ana_bbcdr_fine_i) = FINE_CODE - 1;
			EPHY_WREG(E56G__RXS0_ANA_OVRDVAL_5);
			EPHY_RREG(E56G__RXS0_ANA_OVRDEN_1);
			EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_fine_i) = 1;
			EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_ultrafine_i) = 1;
			EPHY_WREG(E56G__RXS0_ANA_OVRDEN_1);
		} else if (COARSE_CODE > CMVAR_COARSE_MIN) {
			EPHY_RREG(E56G__RXS0_ANA_OVRDVAL_5);
			EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5,
				  ana_bbcdr_ultrafine_i) = CMVAR_UFINE_FMIN_WRAP;
			EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5,
				  ana_bbcdr_fine_i) = CMVAR_FINE_FMIN_WRAP;
			EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5, ana_bbcdr_coarse_i) = COARSE_CODE - 1;
			EPHY_WREG(E56G__RXS0_ANA_OVRDVAL_5);
			EPHY_RREG(E56G__RXS0_ANA_OVRDEN_1);
			EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_coarse_i) = 1;
			EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_fine_i) = 1;
			EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_ultrafine_i) = 1;
			EPHY_WREG(E56G__RXS0_ANA_OVRDEN_1);
		} else {
			PMD_DRV_LOG(ERR, "ERROR: (SECOND_CODE >= CMVAR_SEC_HIGH_TH) "
				    "temperature tracking occurs Error condition");
		}
	}

	return status;
}

static inline int
txgbe_e56_ctle_bypass_seq(struct txgbe_hw *hw, u32 speed)
{
	unsigned int rdata;

	/* 1. Program the following RXS registers as mentioned below. */
	txgbe_e56_ephy_config(E56G__RXS0_ANA_OVRDVAL_0, ana_ctle_bypass_i, 1);
	txgbe_e56_ephy_config(E56G__RXS0_ANA_OVRDEN_0, ovrd_en_ana_ctle_bypass_i, 1);

	txgbe_e56_ephy_config(E56G__RXS0_ANA_OVRDVAL_3, ana_ctle_cz_cstm_i, 0);
	txgbe_e56_ephy_config(E56G__RXS0_ANA_OVRDEN_0, ovrd_en_ana_ctle_cz_cstm_i, 1);

	/* 2. Program the following PDIG registers as mentioned below. */
	EPHY_RREG(E56G__PMD_RXS0_OVRDVAL_1);
	EPHY_XFLD(E56G__PMD_RXS0_OVRDVAL_1, rxs0_rx0_ctle_train_en_i) = 0;
	EPHY_XFLD(E56G__PMD_RXS0_OVRDVAL_1, rxs0_rx0_ctle_train_done_o) = 1;
	EPHY_WREG(E56G__PMD_RXS0_OVRDVAL_1);

	EPHY_RREG(E56G__PMD_RXS0_OVRDEN_1);
	EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_ctle_train_en_i) = 1;
	EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_ctle_train_done_o) = 1;
	EPHY_WREG(E56G__PMD_RXS0_OVRDEN_1);

	if (speed == TXGBE_LINK_SPEED_40GB_FULL) {
		/* 1. Program the following RXS registers as mentioned below. */
		txgbe_e56_ephy_config(E56G__RXS1_ANA_OVRDVAL_0, ana_ctle_bypass_i, 1);
		txgbe_e56_ephy_config(E56G__RXS1_ANA_OVRDEN_0, ovrd_en_ana_ctle_bypass_i, 1);
		txgbe_e56_ephy_config(E56G__RXS2_ANA_OVRDVAL_0, ana_ctle_bypass_i, 1);
		txgbe_e56_ephy_config(E56G__RXS2_ANA_OVRDEN_0, ovrd_en_ana_ctle_bypass_i, 1);
		txgbe_e56_ephy_config(E56G__RXS3_ANA_OVRDVAL_0, ana_ctle_bypass_i, 1);
		txgbe_e56_ephy_config(E56G__RXS3_ANA_OVRDEN_0, ovrd_en_ana_ctle_bypass_i, 1);

		txgbe_e56_ephy_config(E56G__RXS1_ANA_OVRDVAL_3, ana_ctle_cz_cstm_i, 0);
		txgbe_e56_ephy_config(E56G__RXS1_ANA_OVRDEN_0, ovrd_en_ana_ctle_cz_cstm_i, 1);
		txgbe_e56_ephy_config(E56G__RXS2_ANA_OVRDVAL_3, ana_ctle_cz_cstm_i, 0);
		txgbe_e56_ephy_config(E56G__RXS2_ANA_OVRDEN_0, ovrd_en_ana_ctle_cz_cstm_i, 1);
		txgbe_e56_ephy_config(E56G__RXS3_ANA_OVRDVAL_3, ana_ctle_cz_cstm_i, 0);
		txgbe_e56_ephy_config(E56G__RXS3_ANA_OVRDEN_0, ovrd_en_ana_ctle_cz_cstm_i, 1);

		/* 2. Program the following PDIG registers as mentioned below. */
		EPHY_RREG(E56G__PMD_RXS1_OVRDVAL_1);
		EPHY_XFLD(E56G__PMD_RXS1_OVRDVAL_1, rxs1_rx0_ctle_train_en_i) = 0;
		EPHY_XFLD(E56G__PMD_RXS1_OVRDVAL_1, rxs1_rx0_ctle_train_done_o) = 1;
		EPHY_WREG(E56G__PMD_RXS1_OVRDVAL_1);
		EPHY_RREG(E56G__PMD_RXS2_OVRDVAL_1);
		EPHY_XFLD(E56G__PMD_RXS2_OVRDVAL_1, rxs2_rx0_ctle_train_en_i) = 0;
		EPHY_XFLD(E56G__PMD_RXS2_OVRDVAL_1, rxs2_rx0_ctle_train_done_o) = 1;
		EPHY_WREG(E56G__PMD_RXS2_OVRDVAL_1);
		EPHY_RREG(E56G__PMD_RXS3_OVRDVAL_1);
		EPHY_XFLD(E56G__PMD_RXS3_OVRDVAL_1, rxs3_rx0_ctle_train_en_i) = 0;
		EPHY_XFLD(E56G__PMD_RXS3_OVRDVAL_1, rxs3_rx0_ctle_train_done_o) = 1;
		EPHY_WREG(E56G__PMD_RXS3_OVRDVAL_1);

		EPHY_RREG(E56G__PMD_RXS1_OVRDEN_1);
		EPHY_XFLD(E56G__PMD_RXS1_OVRDEN_1, ovrd_en_rxs1_rx0_ctle_train_en_i) = 1;
		EPHY_XFLD(E56G__PMD_RXS1_OVRDEN_1, ovrd_en_rxs1_rx0_ctle_train_done_o) = 1;
		EPHY_WREG(E56G__PMD_RXS1_OVRDEN_1);
		EPHY_RREG(E56G__PMD_RXS2_OVRDEN_1);
		EPHY_XFLD(E56G__PMD_RXS2_OVRDEN_1, ovrd_en_rxs2_rx0_ctle_train_en_i) = 1;
		EPHY_XFLD(E56G__PMD_RXS2_OVRDEN_1, ovrd_en_rxs2_rx0_ctle_train_done_o) = 1;
		EPHY_WREG(E56G__PMD_RXS2_OVRDEN_1);
		EPHY_RREG(E56G__PMD_RXS3_OVRDEN_1);
		EPHY_XFLD(E56G__PMD_RXS3_OVRDEN_1, ovrd_en_rxs3_rx0_ctle_train_en_i) = 1;
		EPHY_XFLD(E56G__PMD_RXS3_OVRDEN_1, ovrd_en_rxs3_rx0_ctle_train_done_o) = 1;
		EPHY_WREG(E56G__PMD_RXS3_OVRDEN_1);
	}
	return 0;
}

static int txgbe_e56_rxs_calib_adapt_seq_40G(struct txgbe_hw *hw, u32 speed)
{
	int status = 0, i, j;
	u32 addr, timer;
	u32 rdata = 0x0;
	const bool bypass_ctle = true;

	for (i = 0; i < 4; i++) {
		rdata = 0x0000;
		addr  = E56PHY_RXS0_OVRDVAL_1_ADDR + (i * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_OFST_ADAPT_EN_I, 0x0);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS0_OVRDEN_2_ADDR + (i * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata,
			       E56PHY_RXS0_OVRDEN_2_OVRD_EN_RXS0_RX0_ADC_OFST_ADAPT_EN_I, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS0_OVRDVAL_1_ADDR + (i * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_GAIN_ADAPT_EN_I, 0x0);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS0_OVRDEN_2_ADDR + (i * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata,
			       E56PHY_RXS0_OVRDEN_2_OVRD_EN_RXS0_RX0_ADC_GAIN_ADAPT_EN_I, 0x1);
		wr32_ephy(hw, addr, rdata);

		rdata = 0x0000;
		addr  = E56PHY_RXS0_OVRDVAL_1_ADDR + (i * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_INTL_CAL_EN_I, 0x0);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS0_OVRDEN_1_ADDR + (i * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata,
			       E56PHY_RXS0_OVRDEN_1_OVRD_EN_RXS0_RX0_ADC_INTL_CAL_EN_I, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS0_OVRDVAL_1_ADDR + (i * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_INTL_CAL_DONE_O, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS0_OVRDEN_1_ADDR + (i * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata,
			       E56PHY_RXS0_OVRDEN_1_OVRD_EN_RXS0_RX0_ADC_INTL_CAL_DONE_O, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS0_OVRDVAL_1_ADDR + (i * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_INTL_ADAPT_EN_I, 0x0);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS0_OVRDEN_2_ADDR + (i * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata,
			       E56PHY_RXS0_OVRDEN_2_OVRD_EN_RXS0_RX0_ADC_INTL_ADAPT_EN_I, 0x1);
		wr32_ephy(hw, addr, rdata);
	}

	if (bypass_ctle == 1)
		txgbe_e56_ctle_bypass_seq(hw, speed);

	/*
	 * 2. Follow sequence described in 2.3.2 RXS Osc Initialization for temperature tracking
	 * range here. RXS would be enabled at the end of this sequence. For the case when PAM4 KR
	 * training is not enabled (including PAM4 mode without KR training), wait until
	 * ALIAS::PDIG::CTRL_FSM_RX_ST would return RX_TRAIN_15_ST (RX_RDY_ST).
	 */
	txgbe_e56_rxs_osc_init_for_temp_track_range(hw, speed);

	addr  = E56PHY_CTRL_FSM_RX_STAT_0_ADDR;
	timer = 0;
	rdata = 0;
	while (EPHY_XFLD(E56G__PMD_CTRL_FSM_RX_STAT_0, ctrl_fsm_rx0_st) != E56PHY_RX_RDY_ST ||
	       EPHY_XFLD(E56G__PMD_CTRL_FSM_RX_STAT_0, ctrl_fsm_rx1_st) != E56PHY_RX_RDY_ST ||
	       EPHY_XFLD(E56G__PMD_CTRL_FSM_RX_STAT_0, ctrl_fsm_rx2_st) != E56PHY_RX_RDY_ST ||
	       EPHY_XFLD(E56G__PMD_CTRL_FSM_RX_STAT_0, ctrl_fsm_rx3_st) != E56PHY_RX_RDY_ST) {
		rdata = rd32_ephy(hw, addr);
		usec_delay(500);
		if (timer++ > PHYINIT_TIMEOUT) {
			rdata = 0;
			addr  = E56PHY_PMD_CFG_0_ADDR;
			rdata = rd32_ephy(hw, addr);
			set_fields_e56(&rdata, E56PHY_PMD_CFG_0_RX_EN_CFG, 0x0);
			wr32_ephy(hw, addr, rdata);
			return TXGBE_ERR_TIMEOUT;
		}
	}

	rdata = 0;
	timer = 0;
	while (EPHY_XFLD(E56G__PMD_RXS0_OVRDVAL_1, rxs0_rx0_cdr_rdy_o) != 1) {
		EPHY_RREG(E56G__PMD_RXS0_OVRDVAL_1);
		usec_delay(500);
		if (timer++ > PHYINIT_TIMEOUT)
			return TXGBE_ERR_TIMEOUT;
	}

	rdata = 0;
	timer = 0;
	while (EPHY_XFLD(E56G__PMD_RXS1_OVRDVAL_1, rxs1_rx0_cdr_rdy_o) != 1) {
		EPHY_RREG(E56G__PMD_RXS1_OVRDVAL_1);
		usec_delay(500);
		if (timer++ > PHYINIT_TIMEOUT)
			return TXGBE_ERR_TIMEOUT;
	}

	rdata = 0;
	timer = 0;
	while (EPHY_XFLD(E56G__PMD_RXS2_OVRDVAL_1, rxs2_rx0_cdr_rdy_o) != 1) {
		EPHY_RREG(E56G__PMD_RXS2_OVRDVAL_1);
		usec_delay(500);
		if (timer++ > PHYINIT_TIMEOUT)
			return TXGBE_ERR_TIMEOUT;
	}

	rdata = 0;
	timer = 0;
	while (EPHY_XFLD(E56G__PMD_RXS3_OVRDVAL_1, rxs3_rx0_cdr_rdy_o) != 1) {
		EPHY_RREG(E56G__PMD_RXS3_OVRDVAL_1);
		usec_delay(500);
		if (timer++ > PHYINIT_TIMEOUT)
			return TXGBE_ERR_TIMEOUT;
	}

	for (i = 0; i < 4; i++) {
		/* 4. Disable VGA and CTLE training so they don't interfere with ADC calibration */
		/* a. Set ALIAS::RXS::VGA_TRAIN_EN = 0b0 */
		addr  = E56PHY_RXS0_OVRDVAL_1_ADDR + (E56PHY_PMD_RX_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_VGA_TRAIN_EN_I, 0x0);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS0_OVRDEN_1_ADDR + (E56PHY_PMD_RX_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS0_OVRDEN_1_OVRD_EN_RXS0_RX0_VGA_TRAIN_EN_I, 0x1);
		wr32_ephy(hw, addr, rdata);

		/* b. Set ALIAS::RXS::CTLE_TRAIN_EN = 0b0 */
		addr  = E56PHY_RXS0_OVRDVAL_1_ADDR + (E56PHY_PMD_RX_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_CTLE_TRAIN_EN_I, 0x0);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS0_OVRDEN_1_ADDR + (E56PHY_PMD_RX_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS0_OVRDEN_1_OVRD_EN_RXS0_RX0_CTLE_TRAIN_EN_I, 0x1);
		wr32_ephy(hw, addr, rdata);

		/* 5. Perform ADC interleaver calibration */
		/* a. Remove the OVERRIDE on ALIAS::RXS::ADC_INTL_CAL_DONE */
		addr  = E56PHY_RXS0_OVRDEN_1_ADDR + (E56PHY_PMD_RX_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata,
			       E56PHY_RXS0_OVRDEN_1_OVRD_EN_RXS0_RX0_ADC_INTL_CAL_DONE_O, 0x0);
		wr32_ephy(hw, addr, rdata);

		addr  = E56PHY_RXS0_OVRDVAL_1_ADDR + (E56PHY_PMD_RX_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_INTL_CAL_EN_I, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr = E56PHY_RXS0_OVRDVAL_1_ADDR + (E56PHY_PMD_RX_OFFSET * i);
		timer = 0;
		while (((rdata >> E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_INTL_CAL_DONE_O_LSB)
			 & 1) != 1) {
			rdata = rd32_ephy(hw, addr);
			usec_delay(1000);

			if (timer++ > PHYINIT_TIMEOUT)
				break;
		}

		/*
		 * 6. Perform ADC offset adaptation and ADC gain adaptation, repeat them a few
		 * times and after that keep it disabled.
		 */
		for (j = 0; j < 16; j++) {
			/* a. ALIAS::RXS::ADC_OFST_ADAPT_EN = 0b1 */
			addr  = E56PHY_RXS0_OVRDVAL_1_ADDR + (E56PHY_PMD_RX_OFFSET * i);
			rdata = rd32_ephy(hw, addr);
			set_fields_e56(&rdata,
				       E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_OFST_ADAPT_EN_I, 0x1);
			wr32_ephy(hw, addr, rdata);

			/* b. Wait for 1ms or greater */
			addr = E56G__PMD_RXS0_OVRDEN_2_ADDR + (E56PHY_PMD_RX_OFFSET * i);
			rdata = rd32_ephy(hw, addr);
			EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_2,
				  ovrd_en_rxs0_rx0_adc_ofst_adapt_done_o) = 0;
			wr32_ephy(hw, addr, rdata);

			rdata = 0;
			addr = E56G__PMD_RXS0_OVRDVAL_1_ADDR + (E56PHY_PMD_RX_OFFSET * i);
			timer = 0;
			while (EPHY_XFLD(E56G__PMD_RXS0_OVRDVAL_1,
					 rxs0_rx0_adc_ofst_adapt_done_o) != 1) {
				rdata = rd32_ephy(hw, addr);
				usec_delay(500);
				if (timer++ > PHYINIT_TIMEOUT)
					break;
			}

			/* c. ALIAS::RXS::ADC_OFST_ADAPT_EN = 0b0 */
			rdata = 0x0000;
			addr  = E56PHY_RXS0_OVRDVAL_1_ADDR + (E56PHY_PMD_RX_OFFSET * i);
			rdata = rd32_ephy(hw, addr);
			set_fields_e56(&rdata,
				       E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_OFST_ADAPT_EN_I, 0x0);
			wr32_ephy(hw, addr, rdata);

			/* d. ALIAS::RXS::ADC_GAIN_ADAPT_EN = 0b1 */
			rdata = 0x0000;
			addr  = E56PHY_RXS0_OVRDVAL_1_ADDR + (E56PHY_PMD_RX_OFFSET * i);
			rdata = rd32_ephy(hw, addr);
			set_fields_e56(&rdata,
				       E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_GAIN_ADAPT_EN_I, 0x1);
			wr32_ephy(hw, addr, rdata);

			/* e. Wait for 1ms or greater */
			addr = E56G__PMD_RXS0_OVRDEN_2_ADDR + (E56PHY_PMD_RX_OFFSET * i);
			rdata = rd32_ephy(hw, addr);
			EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_2,
				  ovrd_en_rxs0_rx0_adc_ofst_adapt_done_o) = 0;
			wr32_ephy(hw, addr, rdata);

			rdata = 0;
			timer = 0;
			addr = E56G__PMD_RXS0_OVRDVAL_1_ADDR + (E56PHY_PMD_RX_OFFSET * i);
			while (EPHY_XFLD(E56G__PMD_RXS0_OVRDVAL_1,
					 rxs0_rx0_adc_gain_adapt_done_o) != 1) {
				rdata = rd32_ephy(hw, addr);
				usec_delay(500);

				if (timer++ > PHYINIT_TIMEOUT)
					break;
			}

			/* f. ALIAS::RXS::ADC_GAIN_ADAPT_EN = 0b0 */
			addr  = E56PHY_RXS0_OVRDVAL_1_ADDR + (E56PHY_PMD_RX_OFFSET * i);
			rdata = rd32_ephy(hw, addr);
			set_fields_e56(&rdata,
				       E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_GAIN_ADAPT_EN_I, 0x0);
			wr32_ephy(hw, addr, rdata);
			/* g. Repeat #a to #f total 16 times */
		}

		/*
		 * 7. Perform ADC interleaver adaptation for 10ms or greater,
		 * and after that disable it
		 * a. ALIAS::RXS::ADC_INTL_ADAPT_EN = 0b1
		 */
		addr  = E56PHY_RXS0_OVRDVAL_1_ADDR + (E56PHY_PMD_RX_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_INTL_ADAPT_EN_I, 0x1);
		wr32_ephy(hw, addr, rdata);
		/* b. Wait for 10ms or greater */
		msleep(10);

		/* c. ALIAS::RXS::ADC_INTL_ADAPT_EN = 0b0 */
		addr = E56G__PMD_RXS0_OVRDEN_2_ADDR + (E56PHY_PMD_RX_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_2, ovrd_en_rxs0_rx0_adc_intl_adapt_en_i) = 0;
		wr32_ephy(hw, addr, rdata);

		/*
		 * 8. Now re-enable VGA and CTLE trainings, so that it continues to adapt tracking
		 * changes in temperature or voltage
		 */
		addr = E56G__PMD_RXS0_OVRDVAL_1_ADDR + (E56PHY_PMD_RX_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		EPHY_XFLD(E56G__PMD_RXS0_OVRDVAL_1, rxs0_rx0_vga_train_en_i) = 1;
		if (bypass_ctle == 0)
			EPHY_XFLD(E56G__PMD_RXS0_OVRDVAL_1, rxs0_rx0_ctle_train_en_i) = 1;
		wr32_ephy(hw, addr, rdata);

		addr = E56G__PMD_RXS0_OVRDEN_1_ADDR + (E56PHY_PMD_RX_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_vga_train_done_o) = 0;
		wr32_ephy(hw, addr, rdata);

		rdata = 0;
		timer = 0;
		addr = E56G__PMD_RXS0_OVRDVAL_1_ADDR + (E56PHY_PMD_RX_OFFSET * i);
		while (EPHY_XFLD(E56G__PMD_RXS0_OVRDVAL_1, rxs0_rx0_vga_train_done_o) != 1) {
			rdata = rd32_ephy(hw, addr);
			usec_delay(500);

			if (timer++ > PHYINIT_TIMEOUT)
				break;
		}

		if (bypass_ctle == 0) {
			addr = E56G__PMD_RXS0_OVRDEN_1_ADDR + (E56PHY_PMD_RX_OFFSET * i);
			rdata = rd32_ephy(hw, addr);
			EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_ctle_train_done_o) = 0;
			wr32_ephy(hw, addr, rdata);

			rdata = 0;
			timer = 0;
			addr = E56G__PMD_RXS0_OVRDVAL_1_ADDR + (E56PHY_PMD_RX_OFFSET * i);
			while (EPHY_XFLD(E56G__PMD_RXS0_OVRDVAL_1,
					 rxs0_rx0_ctle_train_done_o) != 1) {
				rdata = rd32_ephy(hw, addr);
				usec_delay(500);

				if (timer++ > PHYINIT_TIMEOUT)
					break;
			}
		}

		/* a. Remove the OVERRIDE on ALIAS::RXS::VGA_TRAIN_EN */
		addr = E56G__PMD_RXS0_OVRDEN_1_ADDR + (E56PHY_PMD_RX_OFFSET * i);
		rdata = rd32_ephy(hw, addr);
		EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_vga_train_en_i) = 0;
		/* b. Remove the OVERRIDE on ALIAS::RXS::CTLE_TRAIN_EN */
		if (bypass_ctle == 0)
			EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_ctle_train_en_i) = 0;
		wr32_ephy(hw, addr, rdata);

	}
	return status;
}

static inline int
txgbe_e56_rxs_calib_adapt_seq(struct txgbe_hw *hw, u32 speed)
{
	int status = 0, i;
	u32 addr, timer;
	u32 rdata = 0x0;
	bool bypass_ctle = true;

	if (hw->phy.sfp_type == txgbe_sfp_type_da_cu_core0 ||
	    hw->phy.sfp_type == txgbe_sfp_type_da_cu_core1)
		bypass_ctle = 0;

	if (hw->mac.type == txgbe_mac_aml) {
		msleep(350);
		rdata = rd32(hw, TXGBE_GPIOEXT);
		if (rdata & (TXGBE_SFP1_MOD_ABS_LS | TXGBE_SFP1_RX_LOS_LS)) {
			if (rdata & TXGBE_SFP1_MOD_ABS_LS)
				DEBUGOUT("E56phyRxsCalibAdaptSeq TXGBE_SFP1_MOD_ABS_LS");
			else if (rdata & TXGBE_SFP1_RX_LOS_LS)
				DEBUGOUT("E56phyRxsCalibAdaptSeq TXGBE_SFP1_RX_LOS_LS");
			return TXGBE_ERR_PHY_INIT_NOT_DONE;
		}
	}

	rdata = 0;
	addr = E56PHY_RXS0_OVRDVAL_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_OFST_ADAPT_EN_I, 0x0);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS0_OVRDEN_2_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS0_OVRDEN_2_OVRD_EN_RXS0_RX0_ADC_OFST_ADAPT_EN_I
		       , 0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS0_OVRDVAL_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_GAIN_ADAPT_EN_I, 0x0);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS0_OVRDEN_2_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS0_OVRDEN_2_OVRD_EN_RXS0_RX0_ADC_GAIN_ADAPT_EN_I
		       , 0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0;
	addr = E56PHY_RXS0_OVRDVAL_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_INTL_CAL_EN_I, 0x0);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS0_OVRDEN_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS0_OVRDEN_1_OVRD_EN_RXS0_RX0_ADC_INTL_CAL_EN_I,
		       0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS0_OVRDVAL_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_INTL_CAL_DONE_O, 0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS0_OVRDEN_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS0_OVRDEN_1_OVRD_EN_RXS0_RX0_ADC_INTL_CAL_DONE_O
		       , 0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS0_OVRDVAL_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_INTL_ADAPT_EN_I, 0x0);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS0_OVRDEN_2_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS0_OVRDEN_2_OVRD_EN_RXS0_RX0_ADC_INTL_ADAPT_EN_I
		       , 0x1);
	wr32_ephy(hw, addr, rdata);

	if (bypass_ctle == 1)
		txgbe_e56_ctle_bypass_seq(hw, speed);

	/*
	 * 2. Follow sequence described in 2.3.2 RXS Osc Initialization for temperature
	 * tracking range here. RXS would be enabled at the end of this sequence. For the case
	 * when PAM4 KR training is not enabled (including PAM4 mode without KR training),
	 * wait until ALIAS::PDIG::CTRL_FSM_RX_ST would return RX_TRAIN_15_ST (RX_RDY_ST).
	 */
	status |= txgbe_e56_rxs_osc_init_for_temp_track_range(hw, speed);

	addr = E56PHY_CTRL_FSM_RX_STAT_0_ADDR;
	timer = 0;
	rdata = 0;
	while (EPHY_XFLD(E56G__PMD_CTRL_FSM_RX_STAT_0,
			 ctrl_fsm_rx0_st) != E56PHY_RX_RDY_ST) {
		rdata = rd32_ephy(hw, addr);
		usec_delay(500);
		EPHY_RREG(E56G__PMD_CTRL_FSM_RX_STAT_0);
		if (timer++ > PHYINIT_TIMEOUT) {
			rdata = 0;
			addr  = E56PHY_PMD_CFG_0_ADDR;
			rdata = rd32_ephy(hw, addr);
			set_fields_e56(&rdata, E56PHY_PMD_CFG_0_RX_EN_CFG, 0x0);
			wr32_ephy(hw, addr, rdata);
			return TXGBE_ERR_TIMEOUT;
		}
	}

	rdata = 0;
	timer = 0;
	while (EPHY_XFLD(E56G__PMD_RXS0_OVRDVAL_1, rxs0_rx0_cdr_rdy_o) != 1) {
		EPHY_RREG(E56G__PMD_RXS0_OVRDVAL_1);
		usec_delay(500);
		if (timer++ > PHYINIT_TIMEOUT)
			return TXGBE_ERR_TIMEOUT;
	}

	/*
	 * 4. Disable VGA and CTLE training so that they don't interfere with ADC calibration
	 * a. Set ALIAS::RXS::VGA_TRAIN_EN = 0b0
	 */
	addr = E56PHY_RXS0_OVRDVAL_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_VGA_TRAIN_EN_I, 0x0);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS0_OVRDEN_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS0_OVRDEN_1_OVRD_EN_RXS0_RX0_VGA_TRAIN_EN_I,
		       0x1);
	wr32_ephy(hw, addr, rdata);

	/* b. Set ALIAS::RXS::CTLE_TRAIN_EN = 0b0 */
	addr = E56PHY_RXS0_OVRDVAL_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_CTLE_TRAIN_EN_I, 0x0);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS0_OVRDEN_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS0_OVRDEN_1_OVRD_EN_RXS0_RX0_CTLE_TRAIN_EN_I,
		       0x1);
	wr32_ephy(hw, addr, rdata);

	/*
	 * 5. Perform ADC interleaver calibration
	 * a. Remove the OVERRIDE on ALIAS::RXS::ADC_INTL_CAL_DONE
	 */
	addr = E56PHY_RXS0_OVRDEN_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS0_OVRDEN_1_OVRD_EN_RXS0_RX0_ADC_INTL_CAL_DONE_O
		       , 0x0);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS0_OVRDVAL_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_INTL_CAL_EN_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	addr = E56PHY_RXS0_OVRDVAL_1_ADDR;
	timer = 0;
	while (((rdata >> E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_INTL_CAL_DONE_O_LSB) & 1)
	       != 1) {
		rdata = rd32_ephy(hw, addr);
		usec_delay(1000);
		if (timer++ > PHYINIT_TIMEOUT)
			break;
	}

	/*
	 * 6. Perform ADC offset adaptation and ADC gain adaptation,
	 * repeat them a few times and after that keep it disabled.
	 */
	for (i = 0; i < 16; i++) {
		/* a. ALIAS::RXS::ADC_OFST_ADAPT_EN = 0b1 */
		addr = E56PHY_RXS0_OVRDVAL_1_ADDR;
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_OFST_ADAPT_EN_I, 0x1);
		wr32_ephy(hw, addr, rdata);

		/* b. Wait for 1ms or greater */
		txgbe_e56_ephy_config(E56G__PMD_RXS0_OVRDEN_2,
				      ovrd_en_rxs0_rx0_adc_ofst_adapt_done_o, 0);
		rdata = 0;
		timer = 0;
		while (EPHY_XFLD(E56G__PMD_RXS0_OVRDVAL_1,
				 rxs0_rx0_adc_ofst_adapt_done_o) != 1) {
			EPHY_RREG(E56G__PMD_RXS0_OVRDVAL_1);
			usec_delay(500);
			if (timer++ > PHYINIT_TIMEOUT)
				break;
		}

		/* c. ALIAS::RXS::ADC_OFST_ADAPT_EN = 0b0 */
		rdata = 0x0000;
		addr = E56PHY_RXS0_OVRDVAL_1_ADDR;
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_OFST_ADAPT_EN_I, 0x0);
		wr32_ephy(hw, addr, rdata);

		/* d. ALIAS::RXS::ADC_GAIN_ADAPT_EN = 0b1 */
		rdata = 0x0000;
		addr = E56PHY_RXS0_OVRDVAL_1_ADDR;
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_GAIN_ADAPT_EN_I, 0x1);
		wr32_ephy(hw, addr, rdata);

		/* e. Wait for 1ms or greater */
		txgbe_e56_ephy_config(E56G__PMD_RXS0_OVRDEN_2,
				      ovrd_en_rxs0_rx0_adc_ofst_adapt_done_o, 0);
		rdata = 0;
		timer = 0;
		while (EPHY_XFLD(E56G__PMD_RXS0_OVRDVAL_1,
				 rxs0_rx0_adc_gain_adapt_done_o) != 1) {
			EPHY_RREG(E56G__PMD_RXS0_OVRDVAL_1);
			usec_delay(500);
			if (timer++ > PHYINIT_TIMEOUT)
				break;
		}

		/* f. ALIAS::RXS::ADC_GAIN_ADAPT_EN = 0b0 */
		addr = E56PHY_RXS0_OVRDVAL_1_ADDR;
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_GAIN_ADAPT_EN_I, 0x0);
		wr32_ephy(hw, addr, rdata);
		/* g. Repeat #a to #f total 16 times */
	}

	/*
	 * 7. Perform ADC interleaver adaptation for 10ms or greater, and after that disable it
	 * a. ALIAS::RXS::ADC_INTL_ADAPT_EN = 0b1
	 */
	addr = E56PHY_RXS0_OVRDVAL_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS0_OVRDVAL_1_RXS0_RX0_ADC_INTL_ADAPT_EN_I, 0x1);
	wr32_ephy(hw, addr, rdata);
	/* b. Wait for 10ms or greater */
	msleep(10);

	/* c. ALIAS::RXS::ADC_INTL_ADAPT_EN = 0b0 */
	txgbe_e56_ephy_config(E56G__PMD_RXS0_OVRDEN_2,
			      ovrd_en_rxs0_rx0_adc_intl_adapt_en_i, 0);

	/*
	 * 8. Now re-enable VGA and CTLE trainings, so that it continues to adapt tracking
	 * changes in temperature or voltage
	 * <1> Set ALIAS::RXS::VGA_TRAIN_EN = 0b1
	 *     Set ALIAS::RXS::CTLE_TRAIN_EN = 0b1
	 */
	EPHY_RREG(E56G__PMD_RXS0_OVRDVAL_1);
	EPHY_XFLD(E56G__PMD_RXS0_OVRDVAL_1, rxs0_rx0_vga_train_en_i) = 1;
	if (bypass_ctle == 0)
		EPHY_XFLD(E56G__PMD_RXS0_OVRDVAL_1, rxs0_rx0_ctle_train_en_i) = 1;
	EPHY_WREG(E56G__PMD_RXS0_OVRDVAL_1);

	/*
	 * <2> wait for ALIAS::RXS::VGA_TRAIN_DONE = 1
	 *     wait for ALIAS::RXS::CTLE_TRAIN_DONE = 1
	 */
	txgbe_e56_ephy_config(E56G__PMD_RXS0_OVRDEN_1,
			      ovrd_en_rxs0_rx0_vga_train_done_o, 0);
	rdata = 0;
	timer = 0;
	while (EPHY_XFLD(E56G__PMD_RXS0_OVRDVAL_1, rxs0_rx0_vga_train_done_o) != 1) {
		EPHY_RREG(E56G__PMD_RXS0_OVRDVAL_1);
		usec_delay(500);
		if (timer++ > PHYINIT_TIMEOUT)
			break;
	}

	if (bypass_ctle == 0) {
		txgbe_e56_ephy_config(E56G__PMD_RXS0_OVRDEN_1,
				      ovrd_en_rxs0_rx0_ctle_train_done_o, 0);
		rdata = 0;
		timer = 0;
		while (EPHY_XFLD(E56G__PMD_RXS0_OVRDVAL_1, rxs0_rx0_ctle_train_done_o) != 1) {
			EPHY_RREG(E56G__PMD_RXS0_OVRDVAL_1);
			usec_delay(500);
			if (timer++ > PHYINIT_TIMEOUT)
				break;
		}
	}

	/* a. Remove the OVERRIDE on ALIAS::RXS::VGA_TRAIN_EN */
	EPHY_RREG(E56G__PMD_RXS0_OVRDEN_1);
	EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_vga_train_en_i) = 0;
	/* b. Remove the OVERRIDE on ALIAS::RXS::CTLE_TRAIN_EN */
	if (bypass_ctle == 0)
		EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_ctle_train_en_i) = 0;
	EPHY_WREG(E56G__PMD_RXS0_OVRDEN_1);

	return status;
}

static inline u32
txgbe_e56_cfg_temp(struct txgbe_hw *hw)
{
	u32 status;
	u32 value;
	int temp;

	status = txgbe_e56_get_temp(hw, &temp);
	if (status)
		temp = DEFAULT_TEMP;

	if (temp < DEFAULT_TEMP) {
		value = rd32_ephy(hw, CMS_ANA_OVRDEN0);
		set_fields_e56(&value, 25, 25, 0x1);
		wr32_ephy(hw, CMS_ANA_OVRDEN0, value);

		value = rd32_ephy(hw, CMS_ANA_OVRDVAL2);
		set_fields_e56(&value, 20, 16, 0x1);
		wr32_ephy(hw, CMS_ANA_OVRDVAL2, value);

		value = rd32_ephy(hw, CMS_ANA_OVRDEN1);
		set_fields_e56(&value, 12, 12, 0x1);
		wr32_ephy(hw, CMS_ANA_OVRDEN1, value);

		value = rd32_ephy(hw, CMS_ANA_OVRDVAL7);
		set_fields_e56(&value, 8, 4, 0x1);
		wr32_ephy(hw, CMS_ANA_OVRDVAL7, value);
	} else if (temp > HIGH_TEMP) {
		value = rd32_ephy(hw, CMS_ANA_OVRDEN0);
		set_fields_e56(&value, 25, 25, 0x1);
		wr32_ephy(hw, CMS_ANA_OVRDEN0, value);

		value = rd32_ephy(hw, CMS_ANA_OVRDVAL2);
		set_fields_e56(&value, 20, 16, 0x3);
		wr32_ephy(hw, CMS_ANA_OVRDVAL2, value);

		value = rd32_ephy(hw, CMS_ANA_OVRDEN1);
		set_fields_e56(&value, 12, 12, 0x1);
		wr32_ephy(hw, CMS_ANA_OVRDEN1, value);

		value = rd32_ephy(hw, CMS_ANA_OVRDVAL7);
		set_fields_e56(&value, 8, 4, 0x3);
		wr32_ephy(hw, CMS_ANA_OVRDVAL7, value);
	} else {
		value = rd32_ephy(hw, CMS_ANA_OVRDEN1);
		set_fields_e56(&value, 4, 4, 0x1);
		wr32_ephy(hw, CMS_ANA_OVRDEN1, value);

		value = rd32_ephy(hw, CMS_ANA_OVRDVAL4);
		set_fields_e56(&value, 24, 24, 0x1);
		set_fields_e56(&value, 31, 29, 0x4);
		wr32_ephy(hw, CMS_ANA_OVRDVAL4, value);

		value = rd32_ephy(hw, CMS_ANA_OVRDVAL5);
		set_fields_e56(&value, 1, 0, 0x0);
		wr32_ephy(hw, CMS_ANA_OVRDVAL5, value);

		value = rd32_ephy(hw, CMS_ANA_OVRDEN1);
		set_fields_e56(&value, 23, 23, 0x1);
		wr32_ephy(hw, CMS_ANA_OVRDEN1, value);

		value = rd32_ephy(hw, CMS_ANA_OVRDVAL9);
		set_fields_e56(&value, 24, 24, 0x1);
		set_fields_e56(&value, 31, 29, 0x4);
		wr32_ephy(hw, CMS_ANA_OVRDVAL9, value);

		value = rd32_ephy(hw, CMS_ANA_OVRDVAL10);
		set_fields_e56(&value, 1, 0, 0x0);
		wr32_ephy(hw, CMS_ANA_OVRDVAL10, value);
	}

	return 0;
}

static int txgbe_e56_config_rx_40G(struct txgbe_hw *hw, u32 speed)
{
	s32 status;

	status = txgbe_e56_rxs_calib_adapt_seq_40G(hw, speed);
	if (status)
		return status;

	/* Step 2 of 2.3.4 */
	txgbe_e56_set_rxs_ufine_le_max_40g(hw, speed);

	/* 2.3.4 RXS post CDR lock temperature tracking sequence */
	txgbe_temp_track_seq_40g(hw, speed);

	hw->link_valid = true;

	return 0;
}

static int txgbe_e56_config_rx(struct txgbe_hw *hw, u32 speed)
{
	s32 status;

	if (speed == TXGBE_LINK_SPEED_40GB_FULL) {
		status = txgbe_e56_config_rx_40G(hw, speed);
		if (status)
			return status;
	} else  {
		status = txgbe_e56_rxs_calib_adapt_seq(hw, speed);
		if (status)
			return status;

		/* Step 2 of 2.3.4 */
		txgbe_e56_set_rxs_ufine_le_max(hw, speed);

		/* 2.3.4 RXS post CDR lock temperature tracking sequence */
		txgbe_temp_track_seq(hw, speed);
	}
	return 0;
}

/*
 * 2.2.10 SEQ::RX_DISABLE
 * Use PDIG::PMD_CFG[0]::rx_en_cfg[<lane no.>] = 0b0 to powerdown specific RXS lanes.
 * Completion of RXS powerdown can be confirmed by
 * observing ALIAS::PDIG::CTRL_FSM_RX_ST = POWERDN_ST
 */
static int txgbe_e56_disable_rx40G(struct txgbe_hw *hw)
{
	int status = 0;
	unsigned int rdata, timer;
	unsigned int addr, temp;
	int i;

	for (i = 0; i < 4; i++) {
		/* 1. Disable OVERRIDE on below aliases */
		/* a. ALIAS::RXS::RANGE_SEL */
		rdata = 0x0000;
		addr = E56G__RXS0_ANA_OVRDEN_0_ADDR + (i * E56PHY_RXS_OFFSET);
		rdata = rd32_ephy(hw, addr);
		EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_0, ovrd_en_ana_bbcdr_osc_range_sel_i) = 0;
		wr32_ephy(hw, addr, rdata);

		addr = E56G__RXS0_ANA_OVRDEN_1_ADDR + (i * E56PHY_RXS_OFFSET);
		rdata = rd32_ephy(hw, addr);
		/* b. ALIAS::RXS::COARSE */
		EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_coarse_i) = 0;
		/* c. ALIAS::RXS::FINE */
		EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_fine_i) = 0;
		/* d. ALIAS::RXS::ULTRAFINE */
		EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_ultrafine_i) = 0;
		wr32_ephy(hw, addr, rdata);

		/* e. ALIAS::RXS::SAMP_CAL_DONE */
		addr  = E56G__PMD_RXS0_OVRDEN_0_ADDR + (i * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_0, ovrd_en_rxs0_rx0_samp_cal_done_o) = 0;
		wr32_ephy(hw, addr, rdata);

		addr = E56G__PMD_RXS0_OVRDEN_2_ADDR + (i * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		/* f. ALIAS::RXS::ADC_OFST_ADAPT_EN */
		EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_2, ovrd_en_rxs0_rx0_adc_ofst_adapt_en_i) = 0;
		/* g. ALIAS::RXS::ADC_GAIN_ADAPT_EN */
		EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_2, ovrd_en_rxs0_rx0_adc_gain_adapt_en_i) = 0;
		/* j. ALIAS::RXS::ADC_INTL_ADAPT_EN */
		EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_2, ovrd_en_rxs0_rx0_adc_intl_adapt_en_i) = 0;
		wr32_ephy(hw, addr, rdata);

		addr = E56G__PMD_RXS0_OVRDEN_1_ADDR + (i * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		/* h. ALIAS::RXS::ADC_INTL_CAL_EN */
		EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_adc_intl_cal_en_i) = 0;
		/* i. ALIAS::RXS::ADC_INTL_CAL_DONE */
		EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_adc_intl_cal_done_o) = 0;
		/* k. ALIAS::RXS::CDR_EN */
		EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_cdr_en_i) = 0;
		/* l. ALIAS::RXS::VGA_TRAIN_EN */
		EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_vga_train_en_i) = 0;
		/* m. ALIAS::RXS::CTLE_TRAIN_EN */
		EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_ctle_train_en_i) = 0;
		/* p. ALIAS::RXS::RX_FETX_TRAIN_DONE */
		EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_txffe_train_done_o) = 0;
		/* r. ALIAS::RXS::RX_TXFFE_COEFF_CHANGE */
		EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_txffe_coeff_change_o) = 0;
		/* s. ALIAS::RXS::RX_TXFFE_TRAIN_ENACK */
		EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_txffe_train_enack_o) = 0;
		wr32_ephy(hw, addr, rdata);

		addr = E56G__PMD_RXS0_OVRDEN_3_ADDR + (i * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		/* n. ALIAS::RXS::RX_FETX_MOD_TYPE */
		/* o. ALIAS::RXS::RX_FETX_MOD_TYPE_UPDATE */
		temp = EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_3, ovrd_en_rxs0_rx0_spareout_o);
		EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_3, ovrd_en_rxs0_rx0_spareout_o) = temp & 0x8F;
		wr32_ephy(hw, addr, rdata);

		addr = E56G__RXS0_DIG_OVRDEN_1_ADDR + (i * E56PHY_RXS_OFFSET);
		rdata = rd32_ephy(hw, addr);
		/* q. ALIAS::RXS::SLICER_THRESHOLD_OVRD_EN */
		EPHY_XFLD(E56G__RXS0_DIG_OVRDEN_1, top_comp_th_ovrd_en) = 0;
		EPHY_XFLD(E56G__RXS0_DIG_OVRDEN_1, mid_comp_th_ovrd_en) = 0;
		EPHY_XFLD(E56G__RXS0_DIG_OVRDEN_1, bot_comp_th_ovrd_en) = 0;
		wr32_ephy(hw, addr, rdata);

		/* 2. Disable pattern checker */
		addr = E56G__RXS0_DFT_1_ADDR + (i * E56PHY_RXS_OFFSET);
		rdata = rd32_ephy(hw, addr);
		EPHY_XFLD(E56G__RXS0_DFT_1, ber_en) = 0;
		wr32_ephy(hw, addr, rdata);

		/* 3. Disable internal serial loopback mode */
		addr = E56G__RXS0_ANA_OVRDEN_3_ADDR + (i * E56PHY_RXS_OFFSET);
		rdata = rd32_ephy(hw, addr);
		EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_3, ovrd_en_ana_sel_lpbk_i) = 0;
		wr32_ephy(hw, addr, rdata);

		addr = E56G__RXS0_ANA_OVRDEN_2_ADDR + (i * E56PHY_RXS_OFFSET);
		rdata = rd32_ephy(hw, addr);
		EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_2, ovrd_en_ana_en_adccal_lpbk_i) = 0;
		wr32_ephy(hw, addr, rdata);

		/* 4. Enable bypass of clock gates in RXS - */
		addr = E56G__RXS0_RXS_CFG_0_ADDR + (i * E56PHY_RXS_OFFSET);
		rdata = rd32_ephy(hw, addr);
		EPHY_XFLD(E56G__RXS0_RXS_CFG_0, train_clk_gate_bypass_en) = 0x1FFF;
		wr32_ephy(hw, addr, rdata);
	}

	/* 5. Disable KR training mode */
	/* a. ALIAS::PDIG::KR_TRAINING_MODE = 0b0 */
	addr = E56G__PMD_BASER_PMD_CONTROL_ADDR;
	rdata = rd32_ephy(hw, addr);
	EPHY_XFLD(E56G__PMD_BASER_PMD_CONTROL, training_enable_ln0) = 0;
	EPHY_XFLD(E56G__PMD_BASER_PMD_CONTROL, training_enable_ln1) = 0;
	EPHY_XFLD(E56G__PMD_BASER_PMD_CONTROL, training_enable_ln2) = 0;
	EPHY_XFLD(E56G__PMD_BASER_PMD_CONTROL, training_enable_ln3) = 0;
	wr32_ephy(hw, addr, rdata);

	/* 6. Disable RX to TX parallel loopback */
	/* a. ALIAS::PDIG::RX_TO_TX_LPBK_EN = 0b0 */
	addr = E56G__PMD_PMD_CFG_5_ADDR;
	rdata = rd32_ephy(hw, addr);
	EPHY_XFLD(E56G__PMD_PMD_CFG_5, rx_to_tx_lpbk_en) = 0x0;
	wr32_ephy(hw, addr, rdata);

	/*
	 * The FSM to disable RXS is present in PDIG. The FSM disables the RXS when
	 * PDIG::PMD_CFG[0]::rx_en_cfg[<lane no.>] = 0b0
	 */
	txgbe_e56_ephy_config(E56G__PMD_PMD_CFG_0, rx_en_cfg, 0);

	/* Wait RX FSM to be POWERDN_ST */
	timer = 0;

	while (EPHY_XFLD(E56G__PMD_CTRL_FSM_RX_STAT_0, ctrl_fsm_rx0_st) != 0x21 ||
		EPHY_XFLD(E56G__PMD_CTRL_FSM_RX_STAT_0, ctrl_fsm_rx1_st) != 0x21 ||
		EPHY_XFLD(E56G__PMD_CTRL_FSM_RX_STAT_0, ctrl_fsm_rx2_st) != 0x21 ||
		EPHY_XFLD(E56G__PMD_CTRL_FSM_RX_STAT_0, ctrl_fsm_rx3_st) != 0x21) {
		rdata = 0;
		addr  = E56PHY_CTRL_FSM_RX_STAT_0_ADDR;
		rdata = rd32_ephy(hw, addr);
		usec_delay(100);
		if (timer++ > PHYINIT_TIMEOUT) {
			DEBUGOUT("ERROR: Wait E56PHY_CTRL_FSM_RX_STAT_0_ADDR Timeout!\n");
			break;
		}
	}

	return status;
}

static int txgbe_e56_disable_rx(struct txgbe_hw *hw)
{
	int status = 0;
	unsigned int rdata, timer;
	unsigned int addr, temp;

	/* 1. Disable OVERRIDE on below aliases */
	/* a. ALIAS::RXS::RANGE_SEL */
	txgbe_e56_ephy_config(E56G__RXS0_ANA_OVRDEN_0, ovrd_en_ana_bbcdr_osc_range_sel_i, 0);

	EPHY_RREG(E56G__RXS0_ANA_OVRDEN_1);
	/* b. ALIAS::RXS::COARSE */
	EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_coarse_i) = 0;
	/* c. ALIAS::RXS::FINE */
	EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_fine_i) = 0;
	/* d. ALIAS::RXS::ULTRAFINE */
	EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_ultrafine_i) = 0;
	EPHY_WREG(E56G__RXS0_ANA_OVRDEN_1);

	/* e. ALIAS::RXS::SAMP_CAL_DONE */
	txgbe_e56_ephy_config(E56G__PMD_RXS0_OVRDEN_0, ovrd_en_rxs0_rx0_samp_cal_done_o, 0);

	EPHY_RREG(E56G__PMD_RXS0_OVRDEN_2);
	/* f. ALIAS::RXS::ADC_OFST_ADAPT_EN */
	EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_2, ovrd_en_rxs0_rx0_adc_ofst_adapt_en_i) = 0;
	/* g. ALIAS::RXS::ADC_GAIN_ADAPT_EN */
	EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_2, ovrd_en_rxs0_rx0_adc_gain_adapt_en_i) = 0;
	/* j. ALIAS::RXS::ADC_INTL_ADAPT_EN */
	EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_2, ovrd_en_rxs0_rx0_adc_intl_adapt_en_i) = 0;
	EPHY_WREG(E56G__PMD_RXS0_OVRDEN_2);

	EPHY_RREG(E56G__PMD_RXS0_OVRDEN_1);
	/* h. ALIAS::RXS::ADC_INTL_CAL_EN */
	EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_adc_intl_cal_en_i) = 0;
	/* i. ALIAS::RXS::ADC_INTL_CAL_DONE */
	EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_adc_intl_cal_done_o) = 0;
	/* k. ALIAS::RXS::CDR_EN */
	EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_cdr_en_i) = 0;
	/* l. ALIAS::RXS::VGA_TRAIN_EN */
	EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_vga_train_en_i) = 0;
	/* m. ALIAS::RXS::CTLE_TRAIN_EN */
	EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_ctle_train_en_i) = 0;
	/* p. ALIAS::RXS::RX_FETX_TRAIN_DONE */
	EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_txffe_train_done_o) = 0;
	/* r. ALIAS::RXS::RX_TXFFE_COEFF_CHANGE */
	EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_txffe_coeff_change_o) = 0;
	/* s. ALIAS::RXS::RX_TXFFE_TRAIN_ENACK */
	EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_txffe_train_enack_o) = 0;
	EPHY_WREG(E56G__PMD_RXS0_OVRDEN_1);

	EPHY_RREG(E56G__PMD_RXS0_OVRDEN_3);
	/* n. ALIAS::RXS::RX_FETX_MOD_TYPE */
	/* o. ALIAS::RXS::RX_FETX_MOD_TYPE_UPDATE */
	temp = EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_3, ovrd_en_rxs0_rx0_spareout_o);
	EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_3, ovrd_en_rxs0_rx0_spareout_o) = temp & 0x8F;
	EPHY_WREG(E56G__PMD_RXS0_OVRDEN_3);

	/* q. ALIAS::RXS::SLICER_THRESHOLD_OVRD_EN */
	EPHY_RREG(E56G__RXS0_DIG_OVRDEN_1);
	EPHY_XFLD(E56G__RXS0_DIG_OVRDEN_1, top_comp_th_ovrd_en) = 0;
	EPHY_XFLD(E56G__RXS0_DIG_OVRDEN_1, mid_comp_th_ovrd_en) = 0;
	EPHY_XFLD(E56G__RXS0_DIG_OVRDEN_1, bot_comp_th_ovrd_en) = 0;
	EPHY_WREG(E56G__RXS0_DIG_OVRDEN_1);

	/* 2. Disable pattern checker */
	txgbe_e56_ephy_config(E56G__RXS0_DFT_1, ber_en, 0);

	/* 3. Disable internal serial loopback mode */
	txgbe_e56_ephy_config(E56G__RXS0_ANA_OVRDEN_3, ovrd_en_ana_sel_lpbk_i, 0);
	txgbe_e56_ephy_config(E56G__RXS0_ANA_OVRDEN_2, ovrd_en_ana_en_adccal_lpbk_i, 0);

	/* 4. Enable bypass of clock gates in RXS */
	txgbe_e56_ephy_config(E56G__RXS0_RXS_CFG_0, train_clk_gate_bypass_en, 0x1FFF);

	/* 5. Disable KR training mode */
	/* a. ALIAS::PDIG::KR_TRAINING_MODE = 0b0 */
	txgbe_e56_ephy_config(E56G__PMD_BASER_PMD_CONTROL, training_enable_ln0, 0);

	/* 6. Disable RX to TX parallel loopback */
	/* a. ALIAS::PDIG::RX_TO_TX_LPBK_EN = 0b0 */
	txgbe_e56_ephy_config(E56G__PMD_PMD_CFG_5, rx_to_tx_lpbk_en, 0);

	/*
	 * The FSM to disable RXS is present in PDIG. The FSM disables the RXS when
	 * PDIG::PMD_CFG[0]::rx_en_cfg[<lane no.>] = 0b0
	 */
	txgbe_e56_ephy_config(E56G__PMD_PMD_CFG_0, rx_en_cfg, 0);

	/* Wait RX FSM to be POWERDN_ST */
	timer = 0;
	while (1) {
		rdata = 0;
		addr  = E56PHY_CTRL_FSM_RX_STAT_0_ADDR;
		rdata = rd32_ephy(hw, addr);
		if ((rdata & 0x3f) == 0x21)
			break;
		usec_delay(100);
		if (timer++ > PHYINIT_TIMEOUT) {
			DEBUGOUT("ERROR: Wait E56PHY_CTRL_FSM_RX_STAT_0_ADDR Timeout!\n");
			break;
		}
	}

	return status;
}

int txgbe_e56_reconfig_rx(struct txgbe_hw *hw, u32 speed)
{
	u32 addr;
	u32 rdata;
	int status = 0;

	wr32m(hw, TXGBE_MACTXCFG, TXGBE_MACTXCFG_TXE,
	      ~TXGBE_MACTXCFG_TXE);
	wr32m(hw, TXGBE_MACRXCFG, TXGBE_MACRXCFG_ENA,
	      ~TXGBE_MACRXCFG_ENA);

	hw->mac.disable_sec_tx_path(hw);

	if (hw->mac.type == txgbe_mac_aml) {
		rdata = rd32(hw, TXGBE_GPIOEXT);
		if (rdata & (TXGBE_SFP1_MOD_ABS_LS | TXGBE_SFP1_RX_LOS_LS))
			return TXGBE_ERR_TIMEOUT;
	}

	wr32_ephy(hw, E56PHY_INTR_0_ENABLE_ADDR, 0x0);
	wr32_ephy(hw, E56PHY_INTR_1_ENABLE_ADDR, 0x0);

	/*
	 * 14. Do SEQ::RX_DISABLE to disable RXS. Poll ALIAS::PDIG::CTRL_FSM_RX_ST
	 * and confirm its value is POWERDN_ST
	 */
	if (hw->mac.type == txgbe_mac_aml40) {
		txgbe_e56_disable_rx40G(hw);
		status = txgbe_e56_config_rx_40G(hw, speed);
	} else {
		txgbe_e56_disable_rx(hw);
		status = txgbe_e56_config_rx(hw, speed);
	}

	addr = E56PHY_INTR_0_ADDR;
	wr32_ephy(hw, addr, E56PHY_INTR_0_IDLE_ENTRY1);

	addr = E56PHY_INTR_1_ADDR;
	wr32_ephy(hw, addr, E56PHY_INTR_1_IDLE_EXIT1);

	wr32_ephy(hw, E56PHY_INTR_0_ENABLE_ADDR, E56PHY_INTR_0_IDLE_ENTRY1);
	wr32_ephy(hw, E56PHY_INTR_1_ENABLE_ADDR, E56PHY_INTR_1_IDLE_EXIT1);

	hw->mac.enable_sec_tx_path(hw);

	return status;
}

int txgbe_set_link_to_amlite(struct txgbe_hw *hw, u32 speed)
{
	u32 value = 0;
	u32 ppl_lock = false;
	int status = 0;
	u32 reset = 0;

	DEBUGOUT("port[%d] force set speed: 0x%x", hw->bus.lan_id, speed);

	if ((rd32(hw, TXGBE_EPHY_STAT) & TXGBE_EPHY_STAT_PPL_LOCK) ==
	     TXGBE_EPHY_STAT_PPL_LOCK) {
		ppl_lock = true;
		wr32m(hw, TXGBE_MACTXCFG, TXGBE_MACTXCFG_TXE,
		      ~TXGBE_MACTXCFG_TXE);
		wr32m(hw, TXGBE_MACRXCFG, TXGBE_MACRXCFG_ENA,
		      ~TXGBE_MACRXCFG_ENA);

		hw->mac.disable_sec_tx_path(hw);
	}

	if (hw->mac.type == txgbe_mac_aml)
		hw->mac.disable_tx_laser(hw);

	if (hw->bus.lan_id == 0)
		reset = TXGBE_RST_EPHY_LAN_0;

	else
		reset = TXGBE_RST_EPHY_LAN_1;

	wr32(hw, TXGBE_RST,
	     reset | rd32(hw, TXGBE_RST));
	txgbe_flush(hw);
	usec_delay(10);

	/* XLGPCS REGS Start */
	value = rd32_epcs(hw, VR_PCS_DIG_CTRL1);
	value |= 0x8000;
	wr32_epcs(hw, VR_PCS_DIG_CTRL1, value);

	usec_delay(1000);
	value = rd32_epcs(hw, VR_PCS_DIG_CTRL1);
	if ((value & 0x8000)) {
		status = TXGBE_ERR_PHY_INIT_NOT_DONE;
		hw->mac.enable_tx_laser(hw);
		goto out;
	}

	value = rd32_epcs(hw, SR_AN_CTRL);
	set_fields_e56(&value, 12, 12, 0);
	wr32_epcs(hw, SR_AN_CTRL, value);

	if (speed == TXGBE_LINK_SPEED_40GB_FULL) {
		value = rd32_epcs(hw, SR_PCS_CTRL1);
		set_fields_e56(&value, 5, 2, 0x3);
		wr32_epcs(hw, SR_PCS_CTRL1, value);

		value = rd32_epcs(hw, SR_PCS_CTRL2);
		set_fields_e56(&value, 3, 0, 0x4);
		wr32_epcs(hw, SR_PCS_CTRL2, value);

		value = rd32_ephy(hw, ANA_OVRDVAL0);
		set_fields_e56(&value, 29, 29, 0x1);
		set_fields_e56(&value, 1, 1, 0x1);
		wr32_ephy(hw, ANA_OVRDVAL0, value);

		value = rd32_ephy(hw, ANA_OVRDVAL5);
		set_fields_e56(&value, 24, 24, 0x1);
		wr32_ephy(hw, ANA_OVRDVAL5, value);

		value = rd32_ephy(hw, ANA_OVRDEN0);
		set_fields_e56(&value, 1, 1, 0x1);
		wr32_ephy(hw, ANA_OVRDEN0, value);

		value = rd32_ephy(hw, ANA_OVRDEN1);
		set_fields_e56(&value, 30, 30, 0x1);
		set_fields_e56(&value, 25, 25, 0x1);
		wr32_ephy(hw, ANA_OVRDEN1, value);

		value = rd32_ephy(hw, PLL0_CFG0);
		set_fields_e56(&value, 25, 24, 0x1);
		set_fields_e56(&value, 17, 16, 0x3);
		wr32_ephy(hw, PLL0_CFG0, value);

		value = rd32_ephy(hw, PLL0_CFG2);
		set_fields_e56(&value, 12, 8, 0x4);
		wr32_ephy(hw, PLL0_CFG2, value);

		value = rd32_ephy(hw, PLL1_CFG0);
		set_fields_e56(&value, 25, 24, 0x1);
		set_fields_e56(&value, 17, 16, 0x3);
		wr32_ephy(hw, PLL1_CFG0, value);

		value = rd32_ephy(hw, PLL1_CFG2);
		set_fields_e56(&value, 12, 8, 0x8);
		wr32_ephy(hw, PLL1_CFG2, value);

		value = rd32_ephy(hw, PLL0_DIV_CFG0);
		set_fields_e56(&value, 18, 8, 0x294);
		set_fields_e56(&value, 4, 0, 0x8);
		wr32_ephy(hw, PLL0_DIV_CFG0, value);

		value = rd32_ephy(hw, DATAPATH_CFG0);
		set_fields_e56(&value, 30, 28, 0x7);
		set_fields_e56(&value, 26, 24, 0x5);
		set_fields_e56(&value, 18, 16, 0x5);
		set_fields_e56(&value, 14, 12, 0x5);
		set_fields_e56(&value, 10, 8, 0x5);
		wr32_ephy(hw, DATAPATH_CFG0, value);

		value = rd32_ephy(hw, DATAPATH_CFG1);
		set_fields_e56(&value, 26, 24, 0x5);
		set_fields_e56(&value, 10, 8, 0x5);
		set_fields_e56(&value, 18, 16, 0x5);
		set_fields_e56(&value, 2, 0, 0x5);
		wr32_ephy(hw, DATAPATH_CFG1, value);

		value = rd32_ephy(hw, AN_CFG1);
		set_fields_e56(&value, 4, 0, 0x2);
		wr32_ephy(hw, AN_CFG1, value);

		txgbe_e56_cfg_temp(hw);
		txgbe_e56_cfg_40g(hw);

		value = rd32_ephy(hw, PMD_CFG0);
		set_fields_e56(&value, 21, 20, 0x3);
		set_fields_e56(&value, 19, 12, 0xf);
		set_fields_e56(&value, 8, 8, 0x0);
		set_fields_e56(&value, 1, 1, 0x1);
		wr32_ephy(hw, PMD_CFG0, value);
	}

	if (speed == TXGBE_LINK_SPEED_25GB_FULL) {
		value = rd32_epcs(hw, SR_PCS_CTRL1);
		set_fields_e56(&value, 5, 2, 5);
		wr32_epcs(hw, SR_PCS_CTRL1, value);

		value = rd32_epcs(hw, SR_PCS_CTRL2);
		set_fields_e56(&value, 3, 0, 7);
		wr32_epcs(hw, SR_PCS_CTRL2, value);

		value = rd32_epcs(hw, SR_PMA_CTRL2);
		set_fields_e56(&value, 6, 0, 0x39);
		wr32_epcs(hw, SR_PMA_CTRL2, value);

		value = rd32_ephy(hw, ANA_OVRDVAL0);
		set_fields_e56(&value, 29, 29, 0x1);
		set_fields_e56(&value, 1, 1, 0x1);
		wr32_ephy(hw, ANA_OVRDVAL0, value);

		value = rd32_ephy(hw, ANA_OVRDVAL5);
		/* Update to 0 for PIN CLKP/N: Enable the termination of the input buffer */
		set_fields_e56(&value, 24, 24, 0x1);
		wr32_ephy(hw, ANA_OVRDVAL5, value);

		value = rd32_ephy(hw, ANA_OVRDEN0);
		set_fields_e56(&value, 1, 1, 0x1);
		wr32_ephy(hw, ANA_OVRDEN0, value);

		value = rd32_ephy(hw, ANA_OVRDEN1);
		set_fields_e56(&value, 30, 30, 0x1);
		set_fields_e56(&value, 25, 25, 0x1);
		wr32_ephy(hw, ANA_OVRDEN1, value);

		value = rd32_ephy(hw, PLL0_CFG0);
		set_fields_e56(&value, 25, 24, 0x1);
		set_fields_e56(&value, 17, 16, 0x3);
		wr32_ephy(hw, PLL0_CFG0, value);

		value = rd32_ephy(hw, PLL0_CFG2);
		set_fields_e56(&value, 12, 8, 0x4);
		wr32_ephy(hw, PLL0_CFG2, value);

		value = rd32_ephy(hw, PLL1_CFG0);
		set_fields_e56(&value, 25, 24, 0x1);
		set_fields_e56(&value, 17, 16, 0x3);
		wr32_ephy(hw, PLL1_CFG0, value);

		value = rd32_ephy(hw, PLL1_CFG2);
		set_fields_e56(&value, 12, 8, 0x8);
		wr32_ephy(hw, PLL1_CFG2, value);

		value = rd32_ephy(hw, PLL0_DIV_CFG0);
		set_fields_e56(&value, 18, 8, 0x294);
		set_fields_e56(&value, 4, 0, 0x8);
		wr32_ephy(hw, PLL0_DIV_CFG0, value);

		value = rd32_ephy(hw, DATAPATH_CFG0);
		set_fields_e56(&value, 30, 28, 0x7);
		set_fields_e56(&value, 26, 24, 0x5);
		set_fields_e56(&value, 18, 16, 0x3);
		set_fields_e56(&value, 14, 12, 0x5);
		set_fields_e56(&value, 10, 8, 0x5);
		wr32_ephy(hw, DATAPATH_CFG0, value);

		value = rd32_ephy(hw, DATAPATH_CFG1);
		set_fields_e56(&value, 26, 24, 0x5);
		set_fields_e56(&value, 10, 8, 0x5);
		set_fields_e56(&value, 18, 16, 0x3);
		set_fields_e56(&value, 2, 0, 0x3);
		wr32_ephy(hw, DATAPATH_CFG1, value);

		value = rd32_ephy(hw, AN_CFG1);
		set_fields_e56(&value, 4, 0, 0x9);
		wr32_ephy(hw, AN_CFG1, value);

		txgbe_e56_cfg_temp(hw);
		txgbe_e56_cfg_25g(hw);

		value = rd32_ephy(hw, PMD_CFG0);
		set_fields_e56(&value, 21, 20, 0x3);
		set_fields_e56(&value, 19, 12, 0x1); /* TX_EN set */
		set_fields_e56(&value, 8, 8, 0x0);
		set_fields_e56(&value, 1, 1, 0x1);
		wr32_ephy(hw, PMD_CFG0, value);
	}

	if (speed == TXGBE_LINK_SPEED_10GB_FULL) {
		value = rd32_epcs(hw, SR_PCS_CTRL1);
		set_fields_e56(&value, 5, 2, 0);
		wr32_epcs(hw, SR_PCS_CTRL1, value);

		value = rd32_epcs(hw, SR_PCS_CTRL2);
		set_fields_e56(&value, 3, 0, 0);
		wr32_epcs(hw, SR_PCS_CTRL2, value);

		value = rd32_epcs(hw, SR_PMA_CTRL2);
		set_fields_e56(&value, 6, 0, 0xb);
		wr32_epcs(hw, SR_PMA_CTRL2, value);

		value = rd32_ephy(hw, ANA_OVRDVAL0);
		set_fields_e56(&value, 29, 29, 0x1);
		set_fields_e56(&value, 1, 1, 0x1);
		wr32_ephy(hw, ANA_OVRDVAL0, value);

		value = rd32_ephy(hw, ANA_OVRDVAL5);
		set_fields_e56(&value, 24, 24, 0x1);
		wr32_ephy(hw, ANA_OVRDVAL5, value);

		value = rd32_ephy(hw, ANA_OVRDEN0);
		set_fields_e56(&value, 1, 1, 0x1);
		wr32_ephy(hw, ANA_OVRDEN0, value);

		value = rd32_ephy(hw, ANA_OVRDEN1);
		set_fields_e56(&value, 30, 30, 0x1);
		set_fields_e56(&value, 25, 25, 0x1);
		wr32_ephy(hw, ANA_OVRDEN1, value);

		value = rd32_ephy(hw, PLL0_CFG0);
		set_fields_e56(&value, 25, 24, 0x1);
		set_fields_e56(&value, 17, 16, 0x3);
		wr32_ephy(hw, PLL0_CFG0, value);

		value = rd32_ephy(hw, PLL0_CFG2);
		set_fields_e56(&value, 12, 8, 0x4);
		wr32_ephy(hw, PLL0_CFG2, value);

		value = rd32_ephy(hw, PLL1_CFG0);
		set_fields_e56(&value, 25, 24, 0x1);
		set_fields_e56(&value, 17, 16, 0x3);
		wr32_ephy(hw, PLL1_CFG0, value);

		value = rd32_ephy(hw, PLL1_CFG2);
		set_fields_e56(&value, 12, 8, 0x8);
		wr32_ephy(hw, PLL1_CFG2, value);

		value = rd32_ephy(hw, PLL0_DIV_CFG0);
		set_fields_e56(&value, 18, 8, 0x294);
		set_fields_e56(&value, 4, 0, 0x8);
		wr32_ephy(hw, PLL0_DIV_CFG0, value);

		value = rd32_ephy(hw, DATAPATH_CFG0);
		set_fields_e56(&value, 30, 28, 0x7);
		set_fields_e56(&value, 26, 24, 0x5);
		set_fields_e56(&value, 18, 16, 0x5);
		set_fields_e56(&value, 14, 12, 0x5);
		set_fields_e56(&value, 10, 8, 0x5);
		wr32_ephy(hw, DATAPATH_CFG0, value);

		value = rd32_ephy(hw, DATAPATH_CFG1);
		set_fields_e56(&value, 26, 24, 0x5);
		set_fields_e56(&value, 10, 8, 0x5);
		set_fields_e56(&value, 18, 16, 0x5);
		set_fields_e56(&value, 2, 0, 0x5);
		wr32_ephy(hw, DATAPATH_CFG1, value);

		value = rd32_ephy(hw, AN_CFG1);
		set_fields_e56(&value, 4, 0, 0x2);
		wr32_ephy(hw, AN_CFG1, value);

		txgbe_e56_cfg_temp(hw);
		txgbe_e56_cfg_10g(hw);

		value = rd32_ephy(hw, PMD_CFG0);
		set_fields_e56(&value, 21, 20, 0x3);
		set_fields_e56(&value, 19, 12, 0x1); /* TX_EN set */
		set_fields_e56(&value, 8, 8, 0x0);
		set_fields_e56(&value, 1, 1, 0x1);
		wr32_ephy(hw, PMD_CFG0, value);
	}

	if (hw->mac.type == txgbe_mac_aml)
		hw->mac.enable_tx_laser(hw);

	status = txgbe_e56_config_rx(hw, speed);

	value = rd32_ephy(hw, E56PHY_RXS_IDLE_DETECT_1_ADDR);
	set_fields_e56(&value, E56PHY_RXS_IDLE_DETECT_1_IDLE_TH_ADC_PEAK_MAX, 0x28);
	set_fields_e56(&value, E56PHY_RXS_IDLE_DETECT_1_IDLE_TH_ADC_PEAK_MIN, 0xa);
	wr32_ephy(hw, E56PHY_RXS_IDLE_DETECT_1_ADDR, value);

	wr32_ephy(hw, E56PHY_INTR_0_ADDR, E56PHY_INTR_0_IDLE_ENTRY1);
	wr32_ephy(hw, E56PHY_INTR_1_ADDR, E56PHY_INTR_1_IDLE_EXIT1);
	wr32_ephy(hw, E56PHY_INTR_0_ENABLE_ADDR, E56PHY_INTR_0_IDLE_ENTRY1);
	wr32_ephy(hw, E56PHY_INTR_1_ENABLE_ADDR, E56PHY_INTR_1_IDLE_EXIT1);

	if (hw->fec_mode != TXGBE_PHY_FEC_AUTO) {
		hw->cur_fec_link = hw->fec_mode;
		txgbe_e56_fec_set(hw);
	}

out:
	if (ppl_lock) {
		hw->mac.enable_sec_tx_path(hw);
		wr32m(hw, TXGBE_MACRXCFG, TXGBE_MACRXCFG_ENA,
		      TXGBE_MACRXCFG_ENA);
	}

	return status;
}

s32 txgbe_e56_fec_set(struct txgbe_hw *hw)
{
	u32 value;

	if (hw->cur_fec_link  & TXGBE_PHY_FEC_RS) {
		/* disable BASER FEC */
		value = rd32_epcs(hw, SR_PMA_KR_FEC_CTRL);
		set_fields_e56(&value, 0, 0, 0);
		wr32_epcs(hw, SR_PMA_KR_FEC_CTRL, value);

		/* enable RS FEC */
		wr32_epcs(hw, 0x180a3, 0x68c1);
		wr32_epcs(hw, 0x180a4, 0x3321);
		wr32_epcs(hw, 0x180a5, 0x973e);
		wr32_epcs(hw, 0x180a6, 0xccde);

		wr32_epcs(hw, 0x38018, 1024);
		value = rd32_epcs(hw, 0x100c8);
		set_fields_e56(&value, 2, 2, 1);
		wr32_epcs(hw, 0x100c8, value);
	} else if (hw->cur_fec_link & TXGBE_PHY_FEC_BASER) {
		/* disable RS FEC */
		wr32_epcs(hw, 0x180a3, 0x7690);
		wr32_epcs(hw, 0x180a4, 0x3347);
		wr32_epcs(hw, 0x180a5, 0x896f);
		wr32_epcs(hw, 0x180a6, 0xccb8);
		wr32_epcs(hw, 0x38018, 0x3fff);
		value = rd32_epcs(hw, 0x100c8);
		set_fields_e56(&value, 2, 2, 0);
		wr32_epcs(hw, 0x100c8, value);

		/* enable BASER FEC */
		value = rd32_epcs(hw, SR_PMA_KR_FEC_CTRL);
		set_fields_e56(&value, 0, 0, 1);
		wr32_epcs(hw, SR_PMA_KR_FEC_CTRL, value);
	} else {
		/* disable RS FEC */
		wr32_epcs(hw, 0x180a3, 0x7690);
		wr32_epcs(hw, 0x180a4, 0x3347);
		wr32_epcs(hw, 0x180a5, 0x896f);
		wr32_epcs(hw, 0x180a6, 0xccb8);
		wr32_epcs(hw, 0x38018, 0x3fff);
		value = rd32_epcs(hw, 0x100c8);
		set_fields_e56(&value, 2, 2, 0);
		wr32_epcs(hw, 0x100c8, value);

		/* disable BASER FEC */
		value = rd32_epcs(hw, SR_PMA_KR_FEC_CTRL);
		set_fields_e56(&value, 0, 0, 0);
		wr32_epcs(hw, SR_PMA_KR_FEC_CTRL, value);
	}

	return 0;
}

s32 txgbe_e56_fec_polling(struct txgbe_hw *hw, bool *link_up)
{
	u32 link_speed = TXGBE_LINK_SPEED_UNKNOWN;
	s32 i = 0, j = 0;

	do {
		if (!(hw->fec_mode & BIT(j))) {
			j += 1;
			continue;
		}

		hw->cur_fec_link = hw->fec_mode & BIT(j);

		/*
		 * If in fec auto mode, try another fec mode after no link in 1s
		 * for lr sfp, enable KR-FEC to link up with mellonax and intel
		 */
		rte_spinlock_lock(&hw->phy_lock);
		txgbe_e56_fec_set(hw);
		rte_spinlock_unlock(&hw->phy_lock);

		for (i = 0; i < 4; i++) {
			msleep(250);
			txgbe_e56_check_phy_link(hw, &link_speed, link_up);
			if (*link_up)
				return 0;
		}
		j += 1;
	} while (j < 3);

	return 0;
}
