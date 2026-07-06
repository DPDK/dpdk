/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024-2026 Beijing WangXun Technology Co., Ltd.
 */

#include "txgbe_e56.h"
#include "txgbe_hw.h"
#include "txgbe_osdep.h"
#include "txgbe_phy.h"
#include "txgbe_e56_bp.h"
#include "txgbe.h"
#include "../txgbe_logs.h"

static int
txgbe_e56_set_rxs_ufine_le_max(struct txgbe_hw *hw, u32 speed)
{
	u32 rdata, addr;
	u32 ULTRAFINE_CODE[4] = {0};
	int lane_num = 0, lane_idx = 0;
	u32 CMVAR_UFINE_MAX = 0;

	switch (speed) {
	case 10:
		CMVAR_UFINE_MAX = S10G_CMVAR_UFINE_MAX;
		lane_num = 1;
		break;
	case 40:
		CMVAR_UFINE_MAX = S10G_CMVAR_UFINE_MAX;
		lane_num = 4;
		break;
	case 25:
		CMVAR_UFINE_MAX = S25G_CMVAR_UFINE_MAX;
		lane_num = 1;
		break;
	default:
		BP_LOG("%s %d :Invalid speed\n", __func__, __LINE__);
		break;
	}

	for (lane_idx = 0; lane_idx < lane_num; lane_idx++) {
		/* ii get rx ana_bbcdr_ultrafine_i[14, 12] per lane */
		addr = E56G__RXS0_ANA_OVRDVAL_5_ADDR + (E56PHY_RXS_OFFSET * lane_idx);
		rdata = rd32_ephy(hw, addr);
		ULTRAFINE_CODE[lane_idx] = FIELD_GET_M(GENMASK(14, 12), rdata);
		BP_LOG("ULTRAFINE_CODE[%d] = %d, CMVAR_UFINE_MAX: %x\n",
		       lane_idx, ULTRAFINE_CODE[lane_idx], CMVAR_UFINE_MAX);
	}

	for (lane_idx = 0; lane_idx < lane_num; lane_idx++) {
		/* b. Perform the below logic sequence */
		while (ULTRAFINE_CODE[lane_idx] > CMVAR_UFINE_MAX) {
			ULTRAFINE_CODE[lane_idx] -= 1;
			addr = E56G__RXS0_ANA_OVRDVAL_5_ADDR +
			       (E56PHY_RXS_OFFSET * lane_idx);
			rdata = rd32_ephy(hw, addr);
			set_fields_e56(&rdata, 14, 12, ULTRAFINE_CODE[lane_idx]);
			wr32_ephy(hw, addr, rdata);

			/* ovrd_en_ana_bbcdr_ultrafine=1 override ASIC value */
			addr = E56G__RXS0_ANA_OVRDEN_1_ADDR +
			       (E56PHY_RXS_OFFSET * lane_idx);
			rdata = rd32_ephy(hw, addr);
			wr32_ephy(hw, addr, rdata | BIT(3));

			/* Wait until 1milliseconds or greater */
			usec_delay(1000);
		}
	}
	return 0;
}

static int txgbe_e56_rxs_osc_init_for_temp_track_range(struct txgbe_hw *hw,
		u32 speed)
{
	int OFFSET_CENTRE_RANGE_H[4] = {0}, OFFSET_CENTRE_RANGE_L[4] = {}, RANGE_FINAL[4] = {};
	int RX_COARSE_MID_TD, CMVAR_RANGE_H = 0, CMVAR_RANGE_L = 0;
	int status = 0, lane_num = 0;
	int T = 40, lane_id = 0;
	u32 addr, rdata;

	/* Set CMVAR_RANGE_H/L based on the link speed mode */
	switch (speed) {
	case 10:
		CMVAR_RANGE_H = S10G_CMVAR_RANGE_H;
		CMVAR_RANGE_L = S10G_CMVAR_RANGE_L;
		lane_num = 1;
		break;
	case 40:
		CMVAR_RANGE_H = S10G_CMVAR_RANGE_H;
		CMVAR_RANGE_L = S10G_CMVAR_RANGE_L;
		lane_num = 4;
		break;
	case 25:
		CMVAR_RANGE_H = S25G_CMVAR_RANGE_H;
		CMVAR_RANGE_L = S25G_CMVAR_RANGE_L;
		lane_num = 1;
		break;
	default:
		BP_LOG("%s %d :Invalid speed\n", __func__, __LINE__);
		break;
	}

	/* 1. Read the temperature T just before RXS is enabled. */
	txgbe_e56_get_temp(hw, &T);

	/* 2. Define software variable RX_COARSE_MID_TD */
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

	for (lane_id = 0; lane_id < lane_num; lane_id++) {
		addr  = 0x0b4 + (0x200 * lane_id);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 1, 0, CMVAR_RANGE_H);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x08c + (0x200 * lane_id);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 29, 29, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr = 0x1540 + (0x02c * lane_id);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 22, 22, 0x0);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x1530 + (0x02c * lane_id);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 27, 27, 0x1);
		wr32_ephy(hw, addr, rdata);
	}
	rdata = rd32_ephy(hw, 0x1400);
	set_fields_e56(&rdata, 19, 16, GENMASK(lane_num - 1, 0));
	wr32_ephy(hw, 0x1400, rdata);
	status |= kr_read_poll(rd32_ephy, rdata,
		  (((rdata & 0x3f3f3f3f) & GENMASK(8 * lane_num - 1, 0))
		  == (0x09090909 & GENMASK(8 * lane_num - 1, 0))),
		  100, 2000, hw,
		  E56PHY_CTRL_FSM_RX_STAT_0_ADDR);
	if (status)
		BP_LOG("Wait fsm_rx_sts 1 = %x : %d, Wait rx_sts %s.\n",
		       rdata, status, status ? "FAILED" : "SUCCESS");

	for (lane_id = 0; lane_id < lane_num; lane_id++) {
		addr  = 0x0b4 + (0x0200 * lane_id);
		rdata = rd32_ephy(hw, addr);
		OFFSET_CENTRE_RANGE_H[lane_id] = (rdata >> 4) & 0xf;
		if (OFFSET_CENTRE_RANGE_H[lane_id] > RX_COARSE_MID_TD)
			OFFSET_CENTRE_RANGE_H[lane_id] = OFFSET_CENTRE_RANGE_H[lane_id] -
							 RX_COARSE_MID_TD;
		else
			OFFSET_CENTRE_RANGE_H[lane_id] = RX_COARSE_MID_TD -
							 OFFSET_CENTRE_RANGE_H[lane_id];
	}

	/* 7. Do SEQ::RX_DISABLE to disable RXS. */
	rdata = rd32_ephy(hw, 0x1400);
	set_fields_e56(&rdata, 19, 16, 0x0);
	wr32_ephy(hw, 0x1400, rdata);
	status |= kr_read_poll(rd32_ephy, rdata,
		  (((rdata & 0x3f3f3f3f) & GENMASK(8 * lane_num - 1, 0))
		  == (0x21212121 & GENMASK(8 * lane_num - 1, 0))),
		  100, 2000, hw,
		  E56PHY_CTRL_FSM_RX_STAT_0_ADDR);
	if (status)
		BP_LOG("Wait fsm_rx_sts 2 = %x : %d, Wait rx_sts %s.\n",
		       rdata, status, status ? "FAILED" : "SUCCESS");
	rdata = rd32_ephy(hw, 0x15ec);
	wr32_ephy(hw, 0x15ec, rdata);

	for (lane_id = 0; lane_id < lane_num; lane_id++) {
		addr  = 0x0b4 + (0x200 * lane_id);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 1, 0, CMVAR_RANGE_L);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x08c + (0x200 * lane_id);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 29, 29, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr = 0x1540 + (0x02c * lane_id);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 22, 22, 0x0);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x1530 + (0x02c * lane_id);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 27, 27, 0x1);
		wr32_ephy(hw, addr, rdata);
	}
	rdata = rd32_ephy(hw, 0x1400);
	set_fields_e56(&rdata, 19, 16, 0xf);
	wr32_ephy(hw, 0x1400, rdata);
	status |= kr_read_poll(rd32_ephy, rdata,
		  (((rdata & 0x3f3f3f3f) & GENMASK(8 * lane_num - 1, 0))
		  == (0x09090909 & GENMASK(8 * lane_num - 1, 0))),
		  100, 2000, hw,
		  E56PHY_CTRL_FSM_RX_STAT_0_ADDR);
	if (status)
		BP_LOG("Wait fsm_rx_sts 3 = %x : %d, Wait rx_sts %s.\n",
		       rdata, status, status ? "FAILED" : "SUCCESS");
	for (lane_id = 0; lane_id < lane_num; lane_id++) {
		addr  = 0x0b4 + (0x0200 * lane_id);
		rdata = rd32_ephy(hw, addr);
		OFFSET_CENTRE_RANGE_L[lane_id] = (rdata >> 4) & 0xf;
		if (OFFSET_CENTRE_RANGE_L[lane_id] > RX_COARSE_MID_TD)
			OFFSET_CENTRE_RANGE_L[lane_id] = OFFSET_CENTRE_RANGE_L[lane_id] -
							 RX_COARSE_MID_TD;
		else
			OFFSET_CENTRE_RANGE_L[lane_id] = RX_COARSE_MID_TD -
							 OFFSET_CENTRE_RANGE_L[lane_id];
		}
	for (lane_id = 0; lane_id < lane_num; lane_id++) {
		RANGE_FINAL[lane_id] = OFFSET_CENTRE_RANGE_L[lane_id] <
				       OFFSET_CENTRE_RANGE_H[lane_id] ?
				       CMVAR_RANGE_L : CMVAR_RANGE_H;
		BP_LOG("lane_id:%d-RANGE_L:%x-RANGE_H:%x-RANGE_FINAL:%x\n",
		       lane_id, OFFSET_CENTRE_RANGE_L[lane_id],
		       OFFSET_CENTRE_RANGE_H[lane_id], RANGE_FINAL[lane_id]);
	}

	/* 7. Do SEQ::RX_DISABLE to disable RXS. */
	rdata = rd32_ephy(hw, 0x1400);
	set_fields_e56(&rdata, 19, 16, 0x0);
	wr32_ephy(hw, 0x1400, rdata);
	status |= kr_read_poll(rd32_ephy, rdata,
		  (((rdata & 0x3f3f3f3f) & GENMASK(8 * lane_num - 1, 0))
		  == (0x21212121 & GENMASK(8 * lane_num - 1, 0))),
		  100, 2000, hw,
		  E56PHY_CTRL_FSM_RX_STAT_0_ADDR);
	if (status)
		BP_LOG("Wait fsm_rx_sts 4 = %x : %d, Wait rx_sts %s.\n",
		       rdata, status, status ? "FAILED" : "SUCCESS");
	rdata = rd32_ephy(hw, 0x15ec);
	wr32_ephy(hw, 0x15ec, rdata);

	for (lane_id = 0; lane_id < lane_num; lane_id++) {
		addr  = 0x0b4 + (0x0200 * lane_id);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 1, 0, RANGE_FINAL[lane_id]);
		wr32_ephy(hw, addr, rdata);

		rdata = 0x0000;
		addr  = 0x1544 + (lane_id * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 25, 25, 0x0);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x1538 + (lane_id * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 0, 0, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x1544 + (lane_id * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 28, 28, 0x0);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x1538 + (lane_id * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 3, 3, 0x1);
		wr32_ephy(hw, addr, rdata);

		rdata = 0x0000;
		addr  = 0x1544 + (lane_id * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 16, 16, 0x0);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x1534 + (lane_id * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 23, 23, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x1544 + (lane_id * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 17, 17, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x1534 + (lane_id * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 24, 24, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x1544 + (lane_id * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 31, 31, 0x0);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x1538 + (lane_id * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 6, 6, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x1530 + (0x02c * lane_id);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 27, 27, 0x0);
		wr32_ephy(hw, addr, rdata);
	}

	/* Do SEQ::RX_ENABLE */
	rdata = rd32_ephy(hw, 0x1400);
	set_fields_e56(&rdata, E56PHY_PMD_CFG_0_RX_EN_CFG, GENMASK(lane_num - 1, 0));
	wr32_ephy(hw, 0x1400, rdata);

	return status;
}

static int txgbe_e56_rxs_post_cdr_lock_temp_track_seq(struct txgbe_hw *hw,
		u32 speed)
{
	int status = 0;
	u32 rdata;
	int SECOND_CODE;
	int COARSE_CODE;
	int FINE_CODE;
	int ULTRAFINE_CODE;

	int CMVAR_SEC_LOW_TH = 0;
	int CMVAR_UFINE_MAX = 0;
	int CMVAR_FINE_MAX = 0;
	int CMVAR_UFINE_UMAX_WRAP = 0;
	int CMVAR_COARSE_MAX = 0;
	int CMVAR_UFINE_FMAX_WRAP = 0;
	int CMVAR_FINE_FMAX_WRAP = 0;
	int CMVAR_SEC_HIGH_TH = 0;
	int CMVAR_UFINE_MIN = 0;
	int CMVAR_FINE_MIN = 0;
	int CMVAR_UFINE_UMIN_WRAP = 0;
	int CMVAR_COARSE_MIN = 0;
	int CMVAR_UFINE_FMIN_WRAP = 0;
	int CMVAR_FINE_FMIN_WRAP = 0;

	if (speed == 10) {
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
	} else if (speed == 25) {
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
	}

	txgbe_e56_rx_rd_second_code(hw, &SECOND_CODE);

	EPHY_RREG(E56G__RXS0_ANA_OVRDVAL_5);
	COARSE_CODE = EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5, ana_bbcdr_coarse_i);
	FINE_CODE = EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5, ana_bbcdr_fine_i);
	ULTRAFINE_CODE = EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5, ana_bbcdr_ultrafine_i);

	if (SECOND_CODE <= CMVAR_SEC_LOW_TH) {
		if (ULTRAFINE_CODE < CMVAR_UFINE_MAX) {
			txgbe_e56_ephy_config(E56G__RXS0_ANA_OVRDVAL_5, ana_bbcdr_ultrafine_i,
					      ULTRAFINE_CODE + 1);
			EPHY_RREG(E56G__RXS0_ANA_OVRDEN_1);
			EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_ultrafine_i) = 1;
			EPHY_WREG(E56G__RXS0_ANA_OVRDEN_1);
		} else if (FINE_CODE < CMVAR_FINE_MAX) {
			EPHY_RREG(E56G__RXS0_ANA_OVRDVAL_5);
			EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5,
				  ana_bbcdr_ultrafine_i) = CMVAR_UFINE_UMAX_WRAP;
			EPHY_XFLD(E56G__RXS0_ANA_OVRDVAL_5, ana_bbcdr_fine_i) = FINE_CODE + 1;
			EPHY_WREG(E56G__RXS0_ANA_OVRDVAL_5);
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
			EPHY_RREG(E56G__RXS0_ANA_OVRDEN_1);
			EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_coarse_i) = 1;
			EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_fine_i) = 1;
			EPHY_XFLD(E56G__RXS0_ANA_OVRDEN_1, ovrd_en_ana_bbcdr_ultrafine_i) = 1;
			EPHY_WREG(E56G__RXS0_ANA_OVRDEN_1);
		} else {
			BP_LOG("ERROR: (SECOND_CODE <= CMVAR_SEC_LOW_TH) temperature tracking occurs Error condition\n");
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
			BP_LOG("ERROR: (SECOND_CODE >= CMVAR_SEC_HIGH_TH) temperature tracking occurs Error condition\n");
		}
	}

	return status;
}

static int txgbe_e56_ctle_bypass_seq(struct txgbe_hw *hw, u8 bp_link_mode)
{
	u32 rdata;

	txgbe_e56_ephy_config(E56G__RXS0_ANA_OVRDVAL_0, ana_ctle_bypass_i, 1);
	txgbe_e56_ephy_config(E56G__RXS0_ANA_OVRDEN_0, ovrd_en_ana_ctle_bypass_i, 1);

	txgbe_e56_ephy_config(E56G__RXS0_ANA_OVRDVAL_3, ana_ctle_cz_cstm_i, 0);
	txgbe_e56_ephy_config(E56G__RXS0_ANA_OVRDEN_0, ovrd_en_ana_ctle_cz_cstm_i, 1);

	EPHY_RREG(E56G__PMD_RXS0_OVRDVAL_1);
	EPHY_XFLD(E56G__PMD_RXS0_OVRDVAL_1, rxs0_rx0_ctle_train_en_i) = 0;
	EPHY_XFLD(E56G__PMD_RXS0_OVRDVAL_1, rxs0_rx0_ctle_train_done_o) = 1;
	EPHY_WREG(E56G__PMD_RXS0_OVRDVAL_1);

	EPHY_RREG(E56G__PMD_RXS0_OVRDEN_1);
	EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_ctle_train_en_i) = 1;
	EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1, ovrd_en_rxs0_rx0_ctle_train_done_o) = 1;
	EPHY_WREG(E56G__PMD_RXS0_OVRDEN_1);

	if (bp_link_mode == 40) {
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

static int txgbe_e56_rxs_adc_adapt_seq(struct txgbe_hw *hw, u32 bypass_ctle)
{
	int lane_num = 0, lane_idx = 0;
	u32 rdata = 0, addr = 0;
	int status = 0;

	int timer = 0, j = 0;

	switch (hw->bp_link_mode) {
	case 10:
		lane_num = 1;
		break;
	case 40:
		lane_num = 4;
		break;
	case 25:
		lane_num = 1;
		break;
	default:
		BP_LOG("%s %d :Invalid speed\n", __func__, __LINE__);
		break;
	}

	for (lane_idx = 0; lane_idx < lane_num; lane_idx++) {
		addr = 0x1544 + (E56PHY_PMD_RX_OFFSET * lane_idx);
		/* Wait RXS0-3_OVRDVAL[1]::rxs0-3_rx0_cdr_rdy_o = 1 */
		status = kr_read_poll(rd32_ephy, rdata, (rdata & BIT(12)),
				      100, 2000, hw, 0x1544);
		if (status)
			BP_LOG("rxs%d_rx0_cdr_rdy_o = %x, %s.\n",
			       lane_idx, rdata,
			       status ? "FAILED" : "SUCCESS");
	}

	for (lane_idx = 0; lane_idx < lane_num; lane_idx++) {
		/* 4. Disable VGA and CTLE training so they don't interfere with ADC calibration */
		/* a. Set ALIAS::RXS::VGA_TRAIN_EN = 0b0 */
		addr  = 0x1544 + (E56PHY_PMD_RX_OFFSET * lane_idx);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 7, 7, 0x0);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x1534 + (E56PHY_PMD_RX_OFFSET * lane_idx);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 14, 14, 0x1);
		wr32_ephy(hw, addr, rdata);

		/* b. Set ALIAS::RXS::CTLE_TRAIN_EN = 0b0 */
		addr  = 0x1544 + (E56PHY_PMD_RX_OFFSET * lane_idx);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 9, 9, 0x0);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x1534 + (E56PHY_PMD_RX_OFFSET * lane_idx);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 16, 16, 0x1);
		wr32_ephy(hw, addr, rdata);

		/* 5. Perform ADC interleaver calibration */
		/* a. Remove the OVERRIDE on ALIAS::RXS::ADC_INTL_CAL_DONE */
		addr  = 0x1534 + (E56PHY_PMD_RX_OFFSET * lane_idx);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 24, 24, 0x0);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x1544 + (E56PHY_PMD_RX_OFFSET * lane_idx);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 16, 16, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr = 0x1544 + (E56PHY_PMD_RX_OFFSET * lane_idx);
		/* Wait rxs0_rx0_adc_intl_cal_done_o bit17 = 1 */
		status = kr_read_poll(rd32_ephy, rdata, (rdata & BIT(17)),
				      100, 2000, hw, addr);
		if (status)
			BP_LOG("rxs0_rx0_adc_intl_cal_done_o = %x, %s.\n", rdata,
				status ? "FAILED" : "SUCCESS");

		/* 6. Perform ADC offset adaptation and ADC gain adaptation,
		 * repeat them a few times and after that keep it disabled.
		 */
		for (j = 0; j < 16; j++) {
			/* a. ALIAS::RXS::ADC_OFST_ADAPT_EN = 0b1 */
			addr  = 0x1544 + (E56PHY_PMD_RX_OFFSET * lane_idx);
			rdata = rd32_ephy(hw, addr);
			set_fields_e56(&rdata, 25, 25, 0x1);
			wr32_ephy(hw, addr, rdata);

			/* b. Wait for 1ms or greater */
			/* usec_delay(1000); */
			/* set ovrd_en_rxs0_rx0_adc_ofst_adapt_done_o bit1=0 */
			addr = 0x1538 + (E56PHY_PMD_RX_OFFSET * lane_idx);
			rdata = rd32_ephy(hw, addr);
			set_fields_e56(&rdata, 1, 1, 0);
			wr32_ephy(hw, addr, rdata);

			addr = 0x1544 + (E56PHY_PMD_RX_OFFSET * lane_idx);
			/* Wait rxs0_rx0_adc_ofst_adapt_done_o bit26 = 0 */
			status = kr_read_poll(rd32_ephy, rdata,
						   !(rdata & BIT(26)),
						   100, 2000, hw, addr);
			if (status)
				BP_LOG("rxs0_rx0_adc_ofst_adapt_done_o %d = %x, %s.\n",
				       j, rdata, status ? "FAILED" : "SUCCESS");

			/* c. ALIAS::RXS::ADC_OFST_ADAPT_EN = 0b0 */
			rdata = 0x0000;
			addr  = 0x1544 + (E56PHY_PMD_RX_OFFSET * lane_idx);
			rdata = rd32_ephy(hw, addr);
			set_fields_e56(&rdata, 25, 25, 0x0);
			wr32_ephy(hw, addr, rdata);

			/* d. ALIAS::RXS::ADC_GAIN_ADAPT_EN = 0b1 */
			rdata = 0x0000;
			addr  = 0x1544 + (E56PHY_PMD_RX_OFFSET * lane_idx);
			rdata = rd32_ephy(hw, addr);
			set_fields_e56(&rdata, 28, 28, 0x1);
			wr32_ephy(hw, addr, rdata);

			/* e. Wait for 1ms or greater */
			/* usec_delay(1000); */
			/* set ovrd_en_rxs0_rx0_adc_ofst_adapt_done_o bit1=0 */
			addr = 0x1538 + (E56PHY_PMD_RX_OFFSET * lane_idx);
			rdata = rd32_ephy(hw, addr);
			set_fields_e56(&rdata, 1, 1, 0);
			wr32_ephy(hw, addr, rdata);

			addr = 0x1544 + (E56PHY_PMD_RX_OFFSET * lane_idx);
			/* Wait rxs0_rx0_adc_gain_adapt_done_o bit29 = 0 */
			status = kr_read_poll(rd32_ephy, rdata, !(rdata & BIT(29)),
					      100, 2000, hw, addr);
			if (status)
				BP_LOG("rxs0_rx0_adc_gain_adapt_done_o %d = %x, %s.\n",
				       j, rdata, status ? "FAILED" : "SUCCESS");

			/* f. ALIAS::RXS::ADC_GAIN_ADAPT_EN = 0b0 */
			addr  = 0x1544 + (E56PHY_PMD_RX_OFFSET * lane_idx);
			rdata = rd32_ephy(hw, addr);
			set_fields_e56(&rdata, 28, 28, 0x0);
			wr32_ephy(hw, addr, rdata);
		}
		/* g. Repeat #a to #f total 16 times */

		/* 7. Perform ADC interleaver adaptation for 10ms or greater,
		 * and after that disable it
		 */
		/* a. ALIAS::RXS::ADC_INTL_ADAPT_EN = 0b1 */
		addr  = 0x1544 + (E56PHY_PMD_RX_OFFSET * lane_idx);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 31, 31, 0x1);
		wr32_ephy(hw, addr, rdata);
		/* b. Wait for 10ms or greater */
		msleep(20);

		/* c. ALIAS::RXS::ADC_INTL_ADAPT_EN = 0b0 */
		/* set ovrd_en_rxs0_rx0_adc_intl_adapt_en_i=0 */
		addr = 0x1538 + (E56PHY_PMD_RX_OFFSET * lane_idx);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 6, 6, 0);
		wr32_ephy(hw, addr, rdata);

		/* 8. Now re-enable VGA and CTLE trainings, so that it continues
		 * to adapt tracking changes in temperature or voltage
		 * <1>Set ALIAS::RXS::VGA_TRAIN_EN = 0b1
		 */
		/* set rxs0_rx0_vga_train_en_i=1 */
		addr = 0x1544 + (E56PHY_PMD_RX_OFFSET * lane_idx);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 7, 7, 0x1);
		if (bypass_ctle == 0)
			EPHY_XFLD(E56G__PMD_RXS0_OVRDVAL_1, rxs0_rx0_ctle_train_en_i) = 1;
		wr32_ephy(hw, addr, rdata);

		/* <2>wait for ALIAS::RXS::VGA_TRAIN_DONE = 1 */
		/* set ovrd_en_rxs0_rx0_vga_train_done_o = 0 */
		addr = 0x1534 + (E56PHY_PMD_RX_OFFSET * lane_idx);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 15, 15, 0x0);
		wr32_ephy(hw, addr, rdata);

		/* Wait rxs0_rx0_vga_train_done_o bit8 = 0 */
		addr = 0x1544 + (E56PHY_PMD_RX_OFFSET * lane_idx);
		status = kr_read_poll(rd32_ephy, rdata, (rdata & BIT(8)),
					   100, 3000, hw, addr);
		if (status)
			BP_LOG("rxs0_rx0_vga_train_done_o = %x, %s.\n", rdata,
			       status ? "FAILED" : "SUCCESS");

		if (bypass_ctle == 0) {
			addr = 0x1534 + (E56PHY_PMD_RX_OFFSET * lane_idx);
			rdata = rd32_ephy(hw, addr);
			EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1,
				  ovrd_en_rxs0_rx0_ctle_train_done_o) = 0;
			wr32_ephy(hw, addr, rdata);

			rdata = 0;
			timer = 0;
			addr = 0x1544 + (E56PHY_PMD_RX_OFFSET * lane_idx);
			while (EPHY_XFLD(E56G__PMD_RXS0_OVRDVAL_1,
			       rxs0_rx0_ctle_train_done_o) != 1) {
				rdata = rd32_ephy(hw, addr);
				usec_delay(500);

				if (timer++ > PHYINIT_TIMEOUT)
					break;
			}
		}

		/* a. Remove the OVERRIDE on ALIAS::RXS::VGA_TRAIN_EN */
		addr = 0x1534 + (E56PHY_PMD_RX_OFFSET * lane_idx);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 15, 15, 0);
		/* b. Remove the OVERRIDE on ALIAS::RXS::CTLE_TRAIN_EN */
		if (bypass_ctle == 0)
			EPHY_XFLD(E56G__PMD_RXS0_OVRDEN_1,
				  ovrd_en_rxs0_rx0_ctle_train_en_i) = 0;
		wr32_ephy(hw, addr, rdata);
	}

	return status;
}

static int txgbe_e56_phy_rxs_calib_adapt_seq(struct txgbe_hw *hw,
		u8 bp_link_mode, u32 bypass_ctle)
{
	int lane_num = 0, lane_idx = 0;
	int status = 0;
	u32 rdata, addr;

	switch (bp_link_mode) {
	case 10:
		lane_num = 1;
		break;
	case 40:
		lane_num = 4;
		break;
	case 25:
		lane_num = 1;
		break;
	default:
		BP_LOG("%s %d :Invalid speed\n", __func__, __LINE__);
		break;
	}

	for (lane_idx = 0; lane_idx < lane_num; lane_idx++) {
		rdata = 0x0000;
		addr  = 0x1544 + (lane_idx * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 25, 25, 0x0);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x1538 + (lane_idx * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 0, 0, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x1544 + (lane_idx * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 28, 28, 0x0);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x1538 + (lane_idx * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 3, 3, 0x1);
		wr32_ephy(hw, addr, rdata);

		rdata = 0x0000;
		addr  = 0x1544 + (lane_idx * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 16, 16, 0x0);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x1534 + (lane_idx * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 23, 23, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x1544 + (lane_idx * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 17, 17, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x1534 + (lane_idx * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 24, 24, 0x1);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x1544 + (lane_idx * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 31, 31, 0x0);
		wr32_ephy(hw, addr, rdata);

		addr  = 0x1538 + (lane_idx * E56PHY_PMD_RX_OFFSET);
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 6, 6, 0x1);
		wr32_ephy(hw, addr, rdata);
	}
	if (bypass_ctle != 0)
		status |= txgbe_e56_ctle_bypass_seq(hw, bp_link_mode);

	status |= txgbe_e56_rxs_osc_init_for_temp_track_range(hw, bp_link_mode);

	/* Wait an fsm_rx_sts 25G */
	BP_LOG("Wait CTRL_FSM_RX_STAT[0]::ctrl_fsm_rx0_st to be ready ...\n");

	status |= kr_read_poll(rd32_ephy, rdata,
		  (((rdata & 0x3f3f3f3f) & GENMASK(8 * lane_num - 1, 0))
		  == (0x1b1b1b1b & GENMASK(8 * lane_num - 1, 0))),
		  1000, 300, hw,
		  E56PHY_CTRL_FSM_RX_STAT_0_ADDR);
	BP_LOG("wait ctrl_fsm_rx0_st = %x, %s.\n",
	       rdata, status ? "FAILED" : "SUCCESS");

	return status;
}

static int txgbe_e56_cms_cfg_for_temp_track_range(struct txgbe_hw *hw)
{
	int status = 0, T = 40;
	u32 addr, rdata;

	status = txgbe_e56_get_temp(hw, &T);
	if (T < 40) {
		rdata = 0x0000;
		addr = E56PHY_CMS_ANA_OVRDEN_0_ADDR;
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata,
			E56PHY_CMS_ANA_OVRDEN_0_OVRD_EN_ANA_LCPLL_HF_LPF_SETCODE_CALIB_I, 0x1);
		wr32_ephy(hw, addr, rdata);
		rdata = 0x0000;
		addr = E56PHY_CMS_ANA_OVRDVAL_2_ADDR;
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata,
			       E56PHY_CMS_ANA_OVRDVAL_2_ANA_LCPLL_HF_LPF_SETCODE_CALIB_I, 0x1);
		wr32_ephy(hw, addr, rdata);
		rdata = 0x0000;
		addr = E56PHY_CMS_ANA_OVRDEN_1_ADDR;
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata,
			E56PHY_CMS_ANA_OVRDEN_1_OVRD_EN_ANA_LCPLL_LF_LPF_SETCODE_CALIB_I, 0x1);
		wr32_ephy(hw, addr, rdata);
		rdata = 0x0000;
		addr = E56PHY_CMS_ANA_OVRDVAL_7_ADDR;
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata,
			E56PHY_CMS_ANA_OVRDVAL_7_ANA_LCPLL_LF_LPF_SETCODE_CALIB_I, 0x1);
		wr32_ephy(hw, addr, rdata);
	} else if (T > 70) {
		rdata = 0x0000;
		addr = E56PHY_CMS_ANA_OVRDEN_0_ADDR;
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata,
			E56PHY_CMS_ANA_OVRDEN_0_OVRD_EN_ANA_LCPLL_HF_LPF_SETCODE_CALIB_I, 0x1);
		wr32_ephy(hw, addr, rdata);

		rdata = 0x0000;
		addr = E56PHY_CMS_ANA_OVRDVAL_2_ADDR;
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata,
			       E56PHY_CMS_ANA_OVRDVAL_2_ANA_LCPLL_HF_LPF_SETCODE_CALIB_I, 0x3);
		wr32_ephy(hw, addr, rdata);
		rdata = 0x0000;
		addr = E56PHY_CMS_ANA_OVRDEN_1_ADDR;
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata,
			E56PHY_CMS_ANA_OVRDEN_1_OVRD_EN_ANA_LCPLL_LF_LPF_SETCODE_CALIB_I, 0x1);
		wr32_ephy(hw, addr, rdata);
		rdata = 0x0000;
		addr = E56PHY_CMS_ANA_OVRDVAL_7_ADDR;
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata,
			       E56PHY_CMS_ANA_OVRDVAL_7_ANA_LCPLL_LF_LPF_SETCODE_CALIB_I, 0x3);
		wr32_ephy(hw, addr, rdata);
	} else {
		rdata = 0x0000;
		addr = E56PHY_CMS_ANA_OVRDEN_1_ADDR;
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_CMS_ANA_OVRDEN_1_OVRD_EN_ANA_LCPLL_HF_TEST_IN_I,
			       0x1);
		wr32_ephy(hw, addr, rdata);

		rdata = 0x0000;
		addr = E56PHY_CMS_ANA_OVRDVAL_4_ADDR;
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 24, 24, 0x1);
		set_fields_e56(&rdata, 31, 29, 0x4);
		wr32_ephy(hw, addr, rdata);

		rdata = 0x0000;
		addr = E56PHY_CMS_ANA_OVRDVAL_5_ADDR;
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 1, 0, 0x0);
		wr32_ephy(hw, addr, rdata);
		rdata = 0x0000;
		addr = E56PHY_CMS_ANA_OVRDEN_1_ADDR;
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, E56PHY_CMS_ANA_OVRDEN_1_OVRD_EN_ANA_LCPLL_LF_TEST_IN_I,
			       0x1);
		wr32_ephy(hw, addr, rdata);

		rdata = 0x0000;
		addr = E56PHY_CMS_ANA_OVRDVAL_9_ADDR;
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 24, 24, 0x1);
		set_fields_e56(&rdata, 31, 29, 0x4);
		wr32_ephy(hw, addr, rdata);

		rdata = 0x0000;
		addr = E56PHY_CMS_ANA_OVRDVAL_10_ADDR;
		rdata = rd32_ephy(hw, addr);
		set_fields_e56(&rdata, 1, 0, 0x0);
		wr32_ephy(hw, addr, rdata);
	}
	return status;
}

static int txgbe_e56_bp_cfg_25g(struct txgbe_hw *hw)
{
	u32 addr, rdata;

	rdata = 0x0000;
	addr = E56PHY_CMS_PIN_OVRDVAL_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CMS_PIN_OVRDVAL_0_INT_PLL0_TX_SIGNAL_TYPE_I, 0x0);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CMS_PIN_OVRDEN_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CMS_PIN_OVRDEN_0_OVRD_EN_PLL0_TX_SIGNAL_TYPE_I,
		       0x0);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CMS_ANA_OVRDVAL_2_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CMS_ANA_OVRDVAL_2_ANA_LCPLL_HF_VCO_SWING_CTRL_I,
		       0xf);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CMS_ANA_OVRDEN_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata,
		       E56PHY_CMS_ANA_OVRDEN_0_OVRD_EN_ANA_LCPLL_HF_VCO_SWING_CTRL_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CMS_ANA_OVRDVAL_4_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 23, 0, 0x260000);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr  = E56PHY_CMS_ANA_OVRDEN_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CMS_ANA_OVRDEN_1_OVRD_EN_ANA_LCPLL_HF_TEST_IN_I,
		       0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_TXS_TXS_CFG_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_TXS_TXS_CFG_1_ADAPTATION_WAIT_CNT_X256, 0xf);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_TXS_WKUP_CNT_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_TXS_WKUP_CNTLDO_WKUP_CNT_X32, 0xff);
	set_fields_e56(&rdata, E56PHY_TXS_WKUP_CNTDCC_WKUP_CNT_X32, 0xff);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_TXS_PIN_OVRDVAL_6_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 27, 24, 0x5);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_TXS_PIN_OVRDEN_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_TXS_PIN_OVRDEN_0_OVRD_EN_TX0_EFUSE_BITS_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_TXS_ANA_OVRDVAL_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_TXS_ANA_OVRDVAL_1_ANA_TEST_DAC_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_TXS_ANA_OVRDEN_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_TXS_ANA_OVRDEN_0_OVRD_EN_ANA_TEST_DAC_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	txgbe_e56_tx_ffe_cfg(hw, TXGBE_LINK_SPEED_25GB_FULL);

	rdata = 0x0000;
	addr = E56PHY_RXS_RXS_CFG_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_RXS_CFG_0_DSER_DATA_SEL, 0x0);
	set_fields_e56(&rdata, E56PHY_RXS_RXS_CFG_0_TRAIN_CLK_GATE_BYPASS_EN, 0x1fff);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr  = E56PHY_RXS_OSC_CAL_N_CDR_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_1_PREDIV1, 0x700);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_1_TARGET_CNT1, 0x2418);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_OSC_CAL_N_CDR_4_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_4_OSC_RANGE_SEL1, 0x1);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_4_VCO_CODE_INIT, 0x7fb);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_4_OSC_CURRENT_BOOST_EN1, 0x0);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_4_BBCDR_CURRENT_BOOST1, 0x0);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
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

	rdata = 0x0000;
	addr = E56PHY_RXS_OSC_CAL_N_CDR_6_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_6_PI_GAIN_CTRL_PRELOCK, 0x7);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_6_PI_GAIN_CTRL_POSTLOCK, 0x5);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_INTL_CONFIG_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_INTL_CONFIG_0_ADC_INTL2SLICE_DELAY1, 0x3333);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_INTL_CONFIG_2_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_INTL_CONFIG_2_INTERLEAVER_HBW_DISABLE1, 0x0);
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
	set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_1_C1_LTH, 0x1f8);
	set_fields_e56(&rdata, E56PHY_RXS_TXFFE_TRAINING_1_C1_UTH, 0xf0);
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
	addr = E56G__RXS0_FOM_18__ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56G__RXS0_FOM_18__DFE_COEFFL_HINT__MSB,
		       E56G__RXS0_FOM_18__DFE_COEFFL_HINT__LSB, 0x0);
	set_fields_e56(&rdata, E56G__RXS0_FOM_18__DFE_COEFFH_HINT__MSB,
		       E56G__RXS0_FOM_18__DFE_COEFFH_HINT__LSB, 0x0);
	set_fields_e56(&rdata, E56G__RXS0_FOM_18__DFE_COEFF_HINT_LOAD__MSB,
		       E56G__RXS0_FOM_18__DFE_COEFF_HINT_LOAD__LSB, 0x1);
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

	rdata = 0x0000;
	addr = E56PHY_RXS_CTLE_TRAINING_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_0_CTLE_CODE_INIT0, 0x9);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_0_CTLE_CODE_INIT123, 0x9);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_CTLE_TRAINING_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_1_LFEQ_LUT, 0x1ffffea);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_CTLE_TRAINING_2_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_2_ISI_TH_FRAC_P1, 18);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_2_ISI_TH_FRAC_P2, 0);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_2_ISI_TH_FRAC_P3, 0);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_CTLE_TRAINING_3_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_3_TAP_WEIGHT_P1, 1);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_3_TAP_WEIGHT_P2, 0);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_3_TAP_WEIGHT_P3, 0);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_OFFSET_N_GAIN_CAL_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_OFFSET_N_GAIN_CAL_0_ADC_SLICE_DATA_AVG_CNT,
		       0x3);
	set_fields_e56(&rdata, E56PHY_RXS_OFFSET_N_GAIN_CAL_0_ADC_DATA_AVG_CNT, 0x3);
	set_fields_e56(&rdata, E56PHY_RXS_OFFSET_N_GAIN_CAL_0_FE_OFFSET_DAC_CLK_CNT_X8,
		       0xc);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_OFFSET_N_GAIN_CAL_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_OFFSET_N_GAIN_CAL_1_SAMP_ADAPT_CFG, 0x5);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_FFE_TRAINING_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_FFE_TRAINING_0_FFE_TAP_EN, 0xf9ff);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_IDLE_DETECT_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_IDLE_DETECT_1_IDLE_TH_ADC_PEAK_MAX, 0xa);
	set_fields_e56(&rdata, E56PHY_RXS_IDLE_DETECT_1_IDLE_TH_ADC_PEAK_MIN, 0x5);
	wr32_ephy(hw, addr, rdata);

	addr = 0x6cc;
	rdata = 0x8020000;
	wr32_ephy(hw, addr, rdata);
	addr = 0x94;
	rdata = 0;
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_ANA_OVRDVAL_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDVAL_0_ANA_EN_RTERM_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_ANA_OVRDEN_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_0_OVRD_EN_ANA_EN_RTERM_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_ANA_OVRDVAL_6_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 4, 0, 0x0);
	set_fields_e56(&rdata, 14, 13, 0x0);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_ANA_OVRDEN_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_1_OVRD_EN_ANA_BBCDR_VCOFILT_BYP_I,
		       0x1);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_1_OVRD_EN_ANA_TEST_BBCDR_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_ANA_OVRDVAL_15_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 2, 0, 0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_ANA_OVRDVAL_17_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDVAL_17_ANA_VGA2_BOOST_CSTM_I, 0x0);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_ANA_OVRDEN_3_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_3_OVRD_EN_ANA_ANABS_CONFIG_I, 0x1);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_3_OVRD_EN_ANA_VGA2_BOOST_CSTM_I,
		       0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_ANA_OVRDVAL_14_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 13, 13, 0x0);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_ANA_OVRDEN_4_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 13, 13, 0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_EYE_SCAN_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_EYE_SCAN_1_EYE_SCAN_REF_TIMER, 0x400);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_RINGO_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 21, 12, 0x366);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_PMD_CFG_3_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_PMD_CFG_3_CTRL_FSM_TIMEOUT_X64K, 0x80);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_PMD_CFG_4_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_PMD_CFG_4_TRAIN_DC_ON_PERIOD_X64K, 0x18);
	set_fields_e56(&rdata, E56PHY_PMD_CFG_4_TRAIN_DC_PERIOD_X512K, 0x3e);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_PMD_CFG_5_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_PMD_CFG_5_USE_RECENT_MARKER_OFFSET, 0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_0_CONT_ON_ADC_GAIN_CAL_ERR, 0x1);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_0_DO_RX_ADC_OFST_CAL, 0x3);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_0_RX_ERR_ACTION_EN, 0x40);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_1_TRAIN_ST0_WAIT_CNT_X4096, 0xff);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_1_TRAIN_ST1_WAIT_CNT_X4096, 0xff);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_1_TRAIN_ST2_WAIT_CNT_X4096, 0xff);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_1_TRAIN_ST3_WAIT_CNT_X4096, 0xff);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_2_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_2_TRAIN_ST4_WAIT_CNT_X4096, 0x1);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_2_TRAIN_ST5_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_2_TRAIN_ST6_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_2_TRAIN_ST7_WAIT_CNT_X4096, 0x4);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_3_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_3_TRAIN_ST8_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_3_TRAIN_ST9_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_3_TRAIN_ST10_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_3_TRAIN_ST11_WAIT_CNT_X4096, 0x4);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_4_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_4_TRAIN_ST12_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_4_TRAIN_ST13_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_4_TRAIN_ST14_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_4_TRAIN_ST15_WAIT_CNT_X4096, 0x4);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_7_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_7_TRAIN_ST4_EN, 0x4bf);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_7_TRAIN_ST5_EN, 0xc4bf);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_8_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_8_TRAIN_ST7_EN, 0x47ff);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_12_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_12_TRAIN_ST15_EN, 0x67ff);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_13_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_13_TRAIN_ST0_DONE_EN, 0x8001);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_13_TRAIN_ST1_DONE_EN, 0x8002);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_14_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_14_TRAIN_ST3_DONE_EN, 0x8008);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_15_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_15_TRAIN_ST4_DONE_EN, 0x8004);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_17_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_17_TRAIN_ST8_DONE_EN, 0x20c0);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_18_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_18_TRAIN_ST10_DONE_EN, 0x0);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_29_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_29_TRAIN_ST15_DC_EN, 0x3f6d);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_33_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_33_TRAIN0_RATE_SEL, 0x8000);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_33_TRAIN1_RATE_SEL, 0x8000);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_34_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_34_TRAIN2_RATE_SEL, 0x8000);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_34_TRAIN3_RATE_SEL, 0x8000);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_KRT_TFSM_CFG_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_KRT_TFSM_CFGKRT_TFSM_MAX_WAIT_TIMER_X1000K, 0x49);
	set_fields_e56(&rdata, E56PHY_KRT_TFSM_CFGKRT_TFSM_MAX_WAIT_TIMER_X8000K, 0x37);
	set_fields_e56(&rdata, E56PHY_KRT_TFSM_CFGKRT_TFSM_HOLDOFF_TIMER_X256K, 0x2f);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_FETX_FFE_TRAIN_CFG_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_FETX_FFE_TRAIN_CFG_0_KRT_FETX_INIT_FFE_CFG_2,
		       0x2);
	wr32_ephy(hw, addr, rdata);

	return 0;
}

static int txgbe_e56_bp_cfg_10g(struct txgbe_hw *hw)
{
	u32 addr, rdata;

	rdata = 0x0000;
	addr = E56G__CMS_ANA_OVRDVAL_7_ADDR;
	rdata = rd32_ephy(hw, addr);
	((E56G__CMS_ANA_OVRDVAL_7 *)&rdata)->ana_lcpll_lf_vco_swing_ctrl_i = 0xf;
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56G__CMS_ANA_OVRDEN_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	((E56G__CMS_ANA_OVRDEN_1 *)&rdata)->ovrd_en_ana_lcpll_lf_vco_swing_ctrl_i = 0x1;
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56G__CMS_ANA_OVRDVAL_9_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 23, 0, 0x260000);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr  = E56G__CMS_ANA_OVRDEN_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	((E56G__CMS_ANA_OVRDEN_1 *)&rdata)->ovrd_en_ana_lcpll_lf_test_in_i = 0x1;
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_TXS_TXS_CFG_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_TXS_TXS_CFG_1_ADAPTATION_WAIT_CNT_X256, 0xf);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_TXS_WKUP_CNT_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_TXS_WKUP_CNTLDO_WKUP_CNT_X32, 0xff);
	set_fields_e56(&rdata, E56PHY_TXS_WKUP_CNTDCC_WKUP_CNT_X32, 0xff);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_TXS_PIN_OVRDVAL_6_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 19, 16, 0x6);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_TXS_PIN_OVRDEN_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_TXS_PIN_OVRDEN_0_OVRD_EN_TX0_EFUSE_BITS_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_TXS_ANA_OVRDVAL_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_TXS_ANA_OVRDVAL_1_ANA_TEST_DAC_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_TXS_ANA_OVRDEN_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_TXS_ANA_OVRDEN_0_OVRD_EN_ANA_TEST_DAC_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	txgbe_e56_tx_ffe_cfg(hw, TXGBE_LINK_SPEED_10GB_FULL);

	rdata = 0x0000;
	addr = E56PHY_RXS_RXS_CFG_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_RXS_CFG_0_DSER_DATA_SEL, 0x0);
	set_fields_e56(&rdata, E56PHY_RXS_RXS_CFG_0_TRAIN_CLK_GATE_BYPASS_EN, 0x1fff);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr  = E56PHY_RXS_OSC_CAL_N_CDR_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	((E56G_RXS0_OSC_CAL_N_CDR_0 *)&rdata)->prediv0 = 0xfa0;
	((E56G_RXS0_OSC_CAL_N_CDR_0 *)&rdata)->target_cnt0 = 0x203a;
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_OSC_CAL_N_CDR_4_ADDR;
	rdata = rd32_ephy(hw, addr);
	((E56G_RXS0_OSC_CAL_N_CDR_4 *)&rdata)->osc_range_sel0 = 0x2;
	((E56G_RXS0_OSC_CAL_N_CDR_4 *)&rdata)->vco_code_init = 0x7ff;
	((E56G_RXS0_OSC_CAL_N_CDR_4 *)&rdata)->osc_current_boost_en0 = 0x1;
	((E56G_RXS0_OSC_CAL_N_CDR_4 *)&rdata)->bbcdr_current_boost0 = 0x0;
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
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

	rdata = 0x0000;
	addr = E56PHY_RXS_OSC_CAL_N_CDR_6_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_6_PI_GAIN_CTRL_PRELOCK, 0x7);
	set_fields_e56(&rdata, E56PHY_RXS_OSC_CAL_N_CDR_6_PI_GAIN_CTRL_POSTLOCK, 0x5);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_INTL_CONFIG_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	((E56G_RXS0_INTL_CONFIG_0 *)&rdata)->adc_intl2slice_delay0 = 0x5555;
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
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

	rdata = 0x0000;
	addr = E56PHY_RXS_CTLE_TRAINING_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_0_CTLE_CODE_INIT0, 0x9);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_0_CTLE_CODE_INIT123, 0x9);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_CTLE_TRAINING_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_1_LFEQ_LUT, 0x1ffffea);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_CTLE_TRAINING_2_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_2_ISI_TH_FRAC_P1, 0x18);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_2_ISI_TH_FRAC_P2, 0);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_2_ISI_TH_FRAC_P3, 0);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_CTLE_TRAINING_3_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_3_TAP_WEIGHT_P1, 1);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_3_TAP_WEIGHT_P2, 0);
	set_fields_e56(&rdata, E56PHY_RXS_CTLE_TRAINING_3_TAP_WEIGHT_P3, 0);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_OFFSET_N_GAIN_CAL_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_OFFSET_N_GAIN_CAL_0_ADC_SLICE_DATA_AVG_CNT,
		       0x3);
	set_fields_e56(&rdata, E56PHY_RXS_OFFSET_N_GAIN_CAL_0_ADC_DATA_AVG_CNT, 0x3);
	set_fields_e56(&rdata, E56PHY_RXS_OFFSET_N_GAIN_CAL_0_FE_OFFSET_DAC_CLK_CNT_X8,
		       0xc);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_OFFSET_N_GAIN_CAL_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_OFFSET_N_GAIN_CAL_1_SAMP_ADAPT_CFG, 0x5);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_FFE_TRAINING_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_FFE_TRAINING_0_FFE_TAP_EN, 0xf9ff);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_IDLE_DETECT_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_IDLE_DETECT_1_IDLE_TH_ADC_PEAK_MAX, 0xa);
	set_fields_e56(&rdata, E56PHY_RXS_IDLE_DETECT_1_IDLE_TH_ADC_PEAK_MIN, 0x5);
	wr32_ephy(hw, addr, rdata);

	addr = 0x6cc;
	rdata = 0x8020000;
	wr32_ephy(hw, addr, rdata);
	addr = 0x94;
	rdata = 0;
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_ANA_OVRDVAL_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDVAL_0_ANA_EN_RTERM_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_ANA_OVRDEN_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_0_OVRD_EN_ANA_EN_RTERM_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_ANA_OVRDVAL_6_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 4, 0, 0x6);
	set_fields_e56(&rdata, 14, 13, 0x2);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_ANA_OVRDEN_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_1_OVRD_EN_ANA_BBCDR_VCOFILT_BYP_I,
		       0x1);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_1_OVRD_EN_ANA_TEST_BBCDR_I, 0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_ANA_OVRDVAL_15_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 2, 0, 0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_ANA_OVRDVAL_17_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDVAL_17_ANA_VGA2_BOOST_CSTM_I, 0x0);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_ANA_OVRDEN_3_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_3_OVRD_EN_ANA_ANABS_CONFIG_I, 0x1);
	set_fields_e56(&rdata, E56PHY_RXS_ANA_OVRDEN_3_OVRD_EN_ANA_VGA2_BOOST_CSTM_I,
		       0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_ANA_OVRDVAL_14_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 13, 13, 0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_ANA_OVRDEN_4_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 13, 13, 0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_EYE_SCAN_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_RXS_EYE_SCAN_1_EYE_SCAN_REF_TIMER, 0x400);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_RXS_RINGO_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, 21, 12, 0x366);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_PMD_CFG_3_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_PMD_CFG_3_CTRL_FSM_TIMEOUT_X64K, 0x80);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_PMD_CFG_4_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_PMD_CFG_4_TRAIN_DC_ON_PERIOD_X64K, 0x18);
	set_fields_e56(&rdata, E56PHY_PMD_CFG_4_TRAIN_DC_PERIOD_X512K, 0x3e);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_PMD_CFG_5_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_PMD_CFG_5_USE_RECENT_MARKER_OFFSET, 0x1);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_0_ADDR;
	rdata = rd32_ephy(hw, addr);

	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_0_CONT_ON_ADC_GAIN_CAL_ERR, 0x1);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_0_DO_RX_ADC_OFST_CAL, 0x3);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_0_RX_ERR_ACTION_EN, 0x40);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_1_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_1_TRAIN_ST0_WAIT_CNT_X4096, 0xff);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_1_TRAIN_ST1_WAIT_CNT_X4096, 0xff);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_1_TRAIN_ST2_WAIT_CNT_X4096, 0xff);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_1_TRAIN_ST3_WAIT_CNT_X4096, 0xff);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_2_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_2_TRAIN_ST4_WAIT_CNT_X4096, 0x1);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_2_TRAIN_ST5_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_2_TRAIN_ST6_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_2_TRAIN_ST7_WAIT_CNT_X4096, 0x4);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_3_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_3_TRAIN_ST8_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_3_TRAIN_ST9_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_3_TRAIN_ST10_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_3_TRAIN_ST11_WAIT_CNT_X4096, 0x4);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_4_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_4_TRAIN_ST12_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_4_TRAIN_ST13_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_4_TRAIN_ST14_WAIT_CNT_X4096, 0x4);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_4_TRAIN_ST15_WAIT_CNT_X4096, 0x4);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_7_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_7_TRAIN_ST4_EN, 0x4bf);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_7_TRAIN_ST5_EN, 0xc4bf);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_8_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_8_TRAIN_ST7_EN, 0x47ff);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_12_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_12_TRAIN_ST15_EN, 0x67ff);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_13_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_13_TRAIN_ST0_DONE_EN, 0x8001);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_13_TRAIN_ST1_DONE_EN, 0x8002);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_14_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_14_TRAIN_ST3_DONE_EN, 0x8008);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_15_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_15_TRAIN_ST4_DONE_EN, 0x8004);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_17_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_17_TRAIN_ST8_DONE_EN, 0x20c0);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_18_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_18_TRAIN_ST10_DONE_EN, 0x0);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_29_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_29_TRAIN_ST15_DC_EN, 0x3f6d);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_33_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_33_TRAIN0_RATE_SEL, 0x8000);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_33_TRAIN1_RATE_SEL, 0x8000);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_CTRL_FSM_CFG_34_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_34_TRAIN2_RATE_SEL, 0x8000);
	set_fields_e56(&rdata, E56PHY_CTRL_FSM_CFG_34_TRAIN3_RATE_SEL, 0x8000);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_KRT_TFSM_CFG_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_KRT_TFSM_CFGKRT_TFSM_MAX_WAIT_TIMER_X1000K, 0x49);
	set_fields_e56(&rdata, E56PHY_KRT_TFSM_CFGKRT_TFSM_MAX_WAIT_TIMER_X8000K, 0x37);
	set_fields_e56(&rdata, E56PHY_KRT_TFSM_CFGKRT_TFSM_HOLDOFF_TIMER_X256K, 0x2f);
	wr32_ephy(hw, addr, rdata);

	rdata = 0x0000;
	addr = E56PHY_FETX_FFE_TRAIN_CFG_0_ADDR;
	rdata = rd32_ephy(hw, addr);
	set_fields_e56(&rdata, E56PHY_FETX_FFE_TRAIN_CFG_0_KRT_FETX_INIT_FFE_CFG_2,
		       0x2);
	wr32_ephy(hw, addr, rdata);

	return 0;
}

static int txgbe_set_phy_link_mode(struct txgbe_hw *hw,
				   u8 bp_link_mode)
{
	int status = 0;
	u32 rdata = 0;

	u32 speed_select = 0;
	u32 pcs_type_sel = 0;
	u32 cns_en = 0;
	u32 rsfec_en = 0;
	u32 pma_type = 0;
	u32 an0_rate_select = 0;

	switch (bp_link_mode) {
	case 10:
		bp_link_mode = 10;
		speed_select = 0; /* 10 Gb/s */
		pcs_type_sel = 0; /* 10GBASE-R PCS Type */
		cns_en = 0; /* CNS_EN disable */
		rsfec_en = 0; /* RS-FEC disable */
		pma_type = 0xb; /* 10GBASE-KR PMA/PMD type */
		an0_rate_select = 2; /* 10G-KR */
		break;
	case 40:
		bp_link_mode = 40;
		speed_select = 3; /* 40 Gb/s */
		pcs_type_sel = 4; /* 40GBASE-R PCS Type */
		cns_en = 0; /* CNS_EN disable */
		rsfec_en = 0; /* RS-FEC disable */
		pma_type = 0b0100001; /* 40GBASE-CR PMA/PMD type */
		an0_rate_select = 4; /* 40G-KR: 3 40G-CR: 4 */
		break;
	case 25:
		bp_link_mode = 25;
		speed_select = 5; /* 25 Gb/s */
		pcs_type_sel = 7; /* 25GBASE-R PCS Type */
		cns_en = 1; /* CNS_EN */
		rsfec_en = 1; /* RS-FEC enable*/
		pma_type = 0b0111001; /* 25GBASE-KR PMA/PMD type */
		an0_rate_select = 9; /* 9/10/17 25GK/CR-S or 25GK/CR */
		break;
	default:
		BP_LOG("%s %d :Invalid bp_link_mode\n", __func__, __LINE__);
		break;
	}

	hw->curbp_link_mode = bp_link_mode;
	/* To switch to the 40G mode Ethernet operation, complete the following steps:*/
	/* 1. Initiate the vendor-specific software reset by programming
	 * the VR_RST field (bit [15]) of the VR_PCS_DIG_CTRL1 register to 1.
	 */
	rdata = rd32_epcs(hw, 0x038000);
	wr32_epcs(hw, 0x038000, rdata | BIT(15));

	/* 2. Wait for the hardware to clear the value for the VR_RST
	 * field (bit [15]) of the VR_PCS_DIG_CTRL1 register.
	 */
	BP_LOG("Wait for the bit [15] (VR_RST) to get cleared.\n");
	status = kr_read_poll(rd32_ephy, rdata,
				  FIELD_GET_M(BIT(15), rdata) == 0, 100,
				   2000, hw, 0x038000);
	BP_LOG("Wait PHY VR_RST = %x, Wait VR_RST %s.\n",
	       rdata, status ? "FAILED" : "SUCCESS");

	/* wait rx/tx/cm powerdn_st  according pmd 50   2.0.5 */
	status = kr_read_poll(rd32_ephy, rdata,
			     (rdata & GENMASK(3, 0)) == 0x9, 100,
			      2000, hw, 0x14d4);
	BP_LOG("wait ctrl_fsm_cm_st = %x, %s.\n",
	       rdata, status ? "FAILED" : "SUCCESS");

	/* 3. Write 4'b0011 to bits [5:2] of the SR_PCS_CTRL1 register.
	 * 10G: 0 25G: 5 40G: 3
	 */
	rdata = rd32_epcs(hw, 0x030000);
	set_fields_e56(&rdata, 5, 2, speed_select);
	wr32_epcs(hw, 0x030000, rdata);

	/* 4. Write pcs mode sel to bits [3:0] of the SR_PCS_CTRL2 register.
	 * 10G: 0 25G: 4'b0111 40G: 4'b0100
	 */
	rdata = rd32_epcs(hw, 0x030007);
	set_fields_e56(&rdata, 3, 0, pcs_type_sel);
	wr32_epcs(hw, 0x030007, rdata);

	/* 0 1 1 1 0 0 1 : 25GBASE-KR or 25GBASE-KR-S PMA/PMD type
	 * 0 1 1 1 0 0 0 : 25GBASE-CR or 25GBASE-CR-S PMA/PMD type
	 * 0 1 0 0 0 0 1 : 40GBASE-CR4 PMA/PMD type
	 * 0 1 0 0 0 0 0 : 40GBASE-KR4 PMA/PMD type
	 * 0 0 0 1 0 1 1 : 10GBASE-KR PMA/PMD type
	 */
	rdata = rd32_epcs(hw, 0x010007);
	set_fields_e56(&rdata, 6, 0, pma_type);
	wr32_epcs(hw, 0x010007, rdata);

	/* 5. Write only 25g en to Bits [1:0] of VR_PCS_DIG_CTRL3 register. */
	rdata = rd32_epcs(hw, 0x38003);
	set_fields_e56(&rdata, 1, 0, cns_en);
	wr32_epcs(hw, 0x38003, rdata);

	/* 6. Program PCS_AM_CNT field of VR_PCS_AM_CNT register to 'd16383 to
	 * configure the alignment marker interval. To speed-up simulation,
	 * program a smaller value to this field.
	 */
	if (bp_link_mode == 40)
		wr32_epcs(hw, 0x38018, 16383);

	/* 7. Program bit [2] of SR_PMA_RS_FEC_CTRL register to 0
	 * if previously 1 (as RS-FEC is supported in 25G Mode).
	 */

	rdata = rd32_epcs(hw, 0x100c8);
	set_fields_e56(&rdata, 2, 2, rsfec_en);
	wr32_epcs(hw, 0x100c8, rdata);

	/* 8. To enable BASE-R FEC (if desired), set bit [0].
	 * in SR_PMA_KR_FEC_CTRL register
	 */

	/* 4. set phy an status to 0 */
	rdata = rd32_ephy(hw, 0x1434);
	set_fields_e56(&rdata, 7, 4, 0xe);
	wr32_ephy(hw, 0x1434, rdata);

	/* 9. Program Enterprise 56G PHY regs through its own APB interface:
	 * a. Program PHY registers as mentioned in Table 6-6 on page 1197 to
	 *    configure the PHY to 40G
	 *    Mode. For fast-simulation mode, additionally program,
	 *    the registers shown in the Table 6-7 on page 1199
	 * b. Enable the PMD by setting pmd_en field in PMD_CFG[0] (0x1400)
	 *    register
	 */

	rdata = 0x0000;
	rdata = rd32_ephy(hw, ANA_OVRDVAL0);
	set_fields_e56(&rdata, 29, 29, 0x1);
	set_fields_e56(&rdata, 1, 1, 0x1);
	wr32_ephy(hw, ANA_OVRDVAL0, rdata);

	rdata = 0x0000;
	rdata =  rd32_ephy(hw, ANA_OVRDVAL5);
	set_fields_e56(&rdata, 24, 24, 0x1);
	wr32_ephy(hw, ANA_OVRDVAL5, rdata);

	rdata = 0x0000;
	rdata =  rd32_ephy(hw, ANA_OVRDEN0);
	set_fields_e56(&rdata, 1, 1, 0x1);
	wr32_ephy(hw, ANA_OVRDEN0, rdata);

	rdata = 0x0000;
	rdata =  rd32_ephy(hw, ANA_OVRDEN1);
	set_fields_e56(&rdata, 30, 30, 0x1);
	set_fields_e56(&rdata, 25, 25, 0x1);
	wr32_ephy(hw, ANA_OVRDEN1, rdata);

	rdata = 0x0000;
	rdata =  rd32_ephy(hw, PLL0_CFG0);
	set_fields_e56(&rdata, 25, 24, 0x1);
	set_fields_e56(&rdata, 17, 16, 0x3);
	wr32_ephy(hw, PLL0_CFG0, rdata);

	rdata = 0x0000;
	rdata =  rd32_ephy(hw, PLL0_CFG2);
	set_fields_e56(&rdata, 12, 8, 0x4);
	wr32_ephy(hw, PLL0_CFG2, rdata);

	rdata = 0x0000;
	rdata =  rd32_ephy(hw, PLL1_CFG0);
	set_fields_e56(&rdata, 25, 24, 0x1);
	set_fields_e56(&rdata, 17, 16, 0x3);
	wr32_ephy(hw, PLL1_CFG0, rdata);

	rdata = 0x0000;
	rdata =  rd32_ephy(hw, PLL1_CFG2);
	set_fields_e56(&rdata, 12, 8, 0x8);
	wr32_ephy(hw, PLL1_CFG2, rdata);

	rdata = 0x0000;
	rdata = rd32_ephy(hw, PLL0_DIV_CFG0);
	set_fields_e56(&rdata, 18, 8, 0x294);
	set_fields_e56(&rdata, 4, 0, 0x8);
	wr32_ephy(hw, PLL0_DIV_CFG0, rdata);

	rdata = 0x0000;
	rdata = rd32_ephy(hw, DATAPATH_CFG0);
	set_fields_e56(&rdata, 30, 28, 0x7);
	set_fields_e56(&rdata, 26, 24, 0x5);
	if (bp_link_mode == 10 || bp_link_mode == 40)
		set_fields_e56(&rdata, 18, 16, 0x5);
	else if (bp_link_mode == 25)
		set_fields_e56(&rdata, 18, 16, 0x3);
	set_fields_e56(&rdata, 14, 12, 0x5);
	set_fields_e56(&rdata, 10, 8, 0x5);
	wr32_ephy(hw, DATAPATH_CFG0, rdata);

	rdata = 0x0000;
	rdata = rd32_ephy(hw, DATAPATH_CFG1);
	set_fields_e56(&rdata, 26, 24, 0x5);
	set_fields_e56(&rdata, 10, 8, 0x5);
	if (bp_link_mode == 10 || bp_link_mode == 40) {
		set_fields_e56(&rdata, 18, 16, 0x5);
		set_fields_e56(&rdata, 2, 0, 0x5);
	} else if (bp_link_mode == 25) {
		set_fields_e56(&rdata, 18, 16, 0x3);
		set_fields_e56(&rdata, 2, 0, 0x3);
	}
	wr32_ephy(hw, DATAPATH_CFG1, rdata);

	rdata = rd32_ephy(hw, AN_CFG1);
	set_fields_e56(&rdata, 4, 0, an0_rate_select);
	wr32_ephy(hw, AN_CFG1, rdata);

	status = txgbe_e56_cms_cfg_for_temp_track_range(hw);

	if (bp_link_mode == 10)
		txgbe_e56_bp_cfg_10g(hw);
	else if (bp_link_mode == 25)
		txgbe_e56_bp_cfg_25g(hw);
	else if (bp_link_mode == 40)
		txgbe_e56_cfg_40g(hw);

	return status;
}

int txgbe_e56_set_phy_link_mode(struct txgbe_hw *hw,
			     u8 bp_link_mode, u32 need_restart)
{
	int status = 0;
	u32 rdata;

	UNREFERENCED_PARAMETER(bp_link_mode);

	hw->an_done = false;
	if (hw->curbp_link_mode == 10 && !need_restart)
		return 0;
	BP_LOG("Setup to backplane mode ==========\n");

	u32 backplane_mode = 0;
	u32 fec_advertise = 0;

	hw->an_done = false;
	/* pcs + phy rst */
	rdata = rd32(hw, 0x1000c);
	if (hw->bus.lan_id == 1)
		rdata |= BIT(16);
	else
		rdata |= BIT(19);
	wr32(hw, 0x1000c, rdata);
	msleep(20);

	/* clear interrupt */
	wr32_epcs(hw, 0x070000, 0);
	wr32_epcs(hw, 0x030000, 0x8000);
	rdata = rd32_epcs(hw, 0x070000);
	set_fields_e56(&rdata, 12, 12, 0x1);
	wr32_epcs(hw, 0x070000, rdata);
	wr32_epcs(hw, 0x078002, 0x0000);
	/* pcs case fec en to work around first */
	wr32_epcs(hw, 0x100ab, 1);

	if (txgbe_is_backplane(hw)) {
		/* backplane 10G/25G/40G */
		/* 10GKR:7-25KR:14/15-40GKR:8-40GCR:9 */
		/* default all speed */
		if ((hw->device_id & 0xFF) == 0x10) {
			backplane_mode |= BIT(7);
			fec_advertise |= TXGBE_10G_FEC_ABL;
		} else if ((hw->device_id & 0xFF) == 0x25) {
			backplane_mode |= BIT(14) | BIT(15);
			fec_advertise |= TXGBE_25G_RS_FEC_REQ |
					 TXGBE_25G_BASE_FEC_REQ;
		} else if ((hw->device_id & 0xFF) == 0x40) {
			if (hw->phy.bp_capa == 0)
				/* original configure: KR4 + CR4 */
				backplane_mode |= BIT(9) | BIT(8);
			else if (hw->phy.bp_capa == 1)
				/* only 40GBASE-KR4 */
				backplane_mode |= BIT(8);
			else if (hw->phy.bp_capa == 2)
				/* only 40GBASE-CR4 */
				backplane_mode |= BIT(9);
			fec_advertise |= TXGBE_10G_FEC_ABL;
			BP_LOG("Advertised abilities: %d\n", backplane_mode);
		}
	} else {
		if ((hw->phy.fiber_suppport_speed & TXGBE_LINK_SPEED_10GB_FULL)
		     == TXGBE_LINK_SPEED_10GB_FULL) {
			backplane_mode |= 0x80;
			fec_advertise |= TXGBE_10G_FEC_ABL;
		}

		if ((hw->phy.fiber_suppport_speed & TXGBE_LINK_SPEED_25GB_FULL)
		    == TXGBE_LINK_SPEED_25GB_FULL) {
			backplane_mode |= 0xc000;
			fec_advertise |= TXGBE_25G_RS_FEC_REQ |
					 TXGBE_25G_BASE_FEC_REQ;
		}

		if ((hw->phy.fiber_suppport_speed & TXGBE_LINK_SPEED_40GB_FULL)
		    == TXGBE_LINK_SPEED_40GB_FULL) {
			backplane_mode |= BIT(9) | BIT(8);
			fec_advertise |= TXGBE_10G_FEC_ABL;
		}
	}

	wr32_epcs(hw, 0x070010, 0x0001);

	/* 10GKR:7-25KR:14/15-40GKR:8-40GCR:9 */
	wr32_epcs(hw, 0x070011, backplane_mode | 0x11);

	/* BASE-R FEC */
	rdata = rd32_epcs(hw, 0x70012);
	wr32_epcs(hw, 0x70012, fec_advertise);

	wr32_epcs(hw, 0x070016, 0x0000);
	wr32_epcs(hw, 0x070017, 0x0);
	wr32_epcs(hw, 0x070018, 0x0);

	/* config timer */
	wr32_epcs(hw, 0x078004, 0x003c);
	wr32_epcs(hw, 0x078005, CL74_KRTR_TRAINING_TIMEOUT);
	wr32_epcs(hw, 0x078006, 25);
	wr32_epcs(hw, 0x078000, 0x0008 | BIT(2));

	BP_LOG("1.2 Wait 10G KR phy/pcs mode init ....\n");
	status = txgbe_set_phy_link_mode(hw, 10);
	BP_LOG("Wait 10g phy/pcs mode init = %x, %s.\n", rdata,
	/* wait rx/tx/cm powerdn_st  according pmd 50   2.0.5 */
	       status ? "FAILED" : "SUCCESS");

	/* 5. CM_ENABLE */
	rdata = rd32_ephy(hw, 0x1400);
	set_fields_e56(&rdata, 21, 20, 0x3);	/* pll en */
	set_fields_e56(&rdata, 19, 12, 0x0);	/* tx disable */
	set_fields_e56(&rdata, 8, 8, 0x0);	/* pmd mode */
	set_fields_e56(&rdata, 1, 1, 0x1);	/* pmd en */
	wr32_ephy(hw, 0x1400, rdata);

	/* 6, TX_ENABLE */
	rdata = rd32_ephy(hw, 0x1400);
	set_fields_e56(&rdata, 19, 12, 0x1);	/* tx en */
	wr32_ephy(hw, 0x1400, rdata);

	BP_LOG("1.3 Wait 10G PHY RXS....\n");
	status = txgbe_e56_rxs_osc_init_for_temp_track_range(hw, 10);
	BP_LOG("Wait 10G PHY/RXS mode init = %x, %s.\n", rdata,
	       status ? "FAILED" : "SUCCESS");

	/* Wait an 10g fsm_rx_sts */
	status = kr_read_poll(rd32_ephy, rdata,
				((rdata & 0x3f) == 0xb), 1000,
				200, hw,
				E56PHY_CTRL_FSM_RX_STAT_0_ADDR);
	BP_LOG("Wait 10g fsm_rx_sts = %x, Wait rx_sts %s.\n", rdata,
		status ? "FAILED" : "SUCCESS");
	rdata = rd32_epcs(hw, 0x070000);
	set_fields_e56(&rdata, 12, 12, 0x1);
	wr32_epcs(hw, 0x070000, rdata);
	BP_LOG("Setup the backplane mode========end ==\n");

	return status;
}

static void txgbe_e56_print_page_status(struct txgbe_hw *hw,
	struct txgbe_backplane_ability *local_ability,
	struct txgbe_backplane_ability *lp_ability)
{
	u32 rdata = 0;

	/* Read the local AN73 Base Page Ability Registers */
	BP_LOG("Read the local Base Page Ability Registers\n");
	rdata = rd32_epcs(hw, SR_AN_MMD_ADV_REG1);
	local_ability->next_page = (rdata & BIT(15)) ? 1 : 0;
	BP_LOG("  read 70010 data %0x\n", rdata);
	rdata = rd32_epcs(hw, SR_AN_MMD_ADV_REG2);
	BP_LOG("  read 70011 data %0x\n", rdata);
	local_ability->link_ability = (rdata >> 5) & GENMASK(10, 0);
	/* amber-lite only support 10GKR - 25GKR/CR - 25GKR-S/CR-S */
	BP_LOG("  10GKR : %x       25GKR-S/CR-S: %x 25GKR/CR : %x\n",
	       local_ability->link_ability & BIT(ABILITY_10GBASE_KR) ? 1 : 0,
	       local_ability->link_ability & BIT(ABILITY_25GBASE_KRCR_S) ? 1 : 0,
	       local_ability->link_ability & BIT(ABILITY_25GBASE_KRCR) ? 1 : 0);
	BP_LOG("  40GCR4 : %x      40GKR4 : %x\n",
	       local_ability->link_ability & BIT(ABILITY_40GBASE_CR4) ? 1 : 0,
	       local_ability->link_ability & BIT(ABILITY_40GBASE_KR4) ? 1 : 0);
	rdata = rd32_epcs(hw, SR_AN_MMD_ADV_REG3);
	BP_LOG("  F1:FEC Req      F0:FEC Sup      F3:25GFEC       F2:25GRS\n");
	BP_LOG("  F1: %d           F0: %d           F3: %d           F2: %d\n",
	      ((rdata >> 15) & 0x01), ((rdata >> 14) & 0x01),
	      ((rdata >> 13) & 0x01), ((rdata >> 12) & 0x01));
	local_ability->fec_ability = rdata;
	BP_LOG("  read 70012 data %0x\n", rdata);

	/* Read the link partner AN73 Base Page Ability Registers */
	BP_LOG("Read the link partner Base Page Ability Registers\n");
	rdata = rd32_epcs(hw, SR_AN_MMD_LP_ABL1);
	lp_ability->next_page = (rdata & BIT(15)) ? 1 : 0;
	BP_LOG("  read 70013 data %0x\n", rdata);
	rdata = rd32_epcs(hw, SR_AN_MMD_LP_ABL2);
	lp_ability->link_ability = (rdata >> 5) & GENMASK(10, 0);
	BP_LOG("  read 70014 data %0x\n", rdata);
	BP_LOG("  KX : %x  KX4 : %x\n",
	       lp_ability->link_ability & BIT(ABILITY_1000BASE_KX) ? 1 : 0,
	       lp_ability->link_ability & BIT(ABILITY_10GBASE_KX4) ? 1 : 0);
	BP_LOG("  10GKR : %x       25GKR-S/CR-S: %x 25GKR/CR : %x\n",
	       lp_ability->link_ability & BIT(ABILITY_10GBASE_KR) ? 1 : 0,
	       lp_ability->link_ability & BIT(ABILITY_25GBASE_KRCR_S) ? 1 : 0,
	       lp_ability->link_ability & BIT(ABILITY_25GBASE_KRCR) ? 1 : 0);
	BP_LOG("  40GCR4 : %x      40GKR4 : %x\n",
	       lp_ability->link_ability & BIT(ABILITY_40GBASE_CR4) ? 1 : 0,
	       lp_ability->link_ability & BIT(ABILITY_40GBASE_KR4) ? 1 : 0);
	rdata = rd32_epcs(hw, SR_AN_MMD_LP_ABL3);
	BP_LOG("  F1:FEC Req      F0:FEC Sup      F3:25GFEC       F2:25GRS\n");
	BP_LOG("  F1: %d           F0: %d           F3: %d           F2: %d\n",
	      ((rdata >> 15) & 0x01), ((rdata >> 14) & 0x01),
	      ((rdata >> 13) & 0x01), ((rdata >> 12) & 0x01));
	lp_ability->fec_ability = rdata;

	hw->phy.fec_mode = 0;
	if (rdata & TXGBE_25G_RS_FEC_REQ)
		hw->phy.fec_mode |= TXGBE_25G_RS_FEC_REQ;
	if (rdata & TXGBE_25G_BASE_FEC_REQ)
		hw->phy.fec_mode |= TXGBE_25G_BASE_FEC_REQ;
	if (rdata & TXGBE_10G_FEC_ABL)
		hw->phy.fec_mode |= TXGBE_10G_FEC_ABL;
	if (rdata & TXGBE_10G_FEC_REQ)
		hw->phy.fec_mode |= TXGBE_10G_FEC_REQ;
	BP_LOG("  read 70015 data %0x\n", rdata);

	BP_LOG("  read 70016 data %0x\n", rd32_epcs(hw, 0x70016));
	BP_LOG("  read 70017 data %0x\n", rd32_epcs(hw, 0x70017));
	BP_LOG("  read 70018 data %0x\n", rd32_epcs(hw, 0x70018));
	BP_LOG("  read 70019 data %0x\n", rd32_epcs(hw, 0x70019));
	BP_LOG("  read 7001a data %0x\n", rd32_epcs(hw, 0x7001a));
	BP_LOG("  read 7001b data %0x\n", rd32_epcs(hw, 0x7001b));
}

static int chk_bkp_ability(struct txgbe_hw *hw,
	struct txgbe_backplane_ability local_ability,
	struct txgbe_backplane_ability lp_ability)
{
	unsigned int com_link_ability;

	BP_LOG("CheckBkpAn73Ability():\n");
	/* Check the common link ability and take action based on the result*/
	com_link_ability = local_ability.link_ability &
			 lp_ability.link_ability;
	BP_LOG("comAbility= 0x%x, Ability= 0x%x, lpAbility= 0x%x\n",
		com_link_ability, local_ability.link_ability,
		lp_ability.link_ability);

	if (com_link_ability == 0) {
		hw->bp_link_mode = 0;
		BP_LOG("Do not support any compatible speed mode!\n");
		return -EINVAL;
	} else if (com_link_ability & BIT(ABILITY_40GBASE_KR4)) {
		BP_LOG("Link mode is [ABILITY_40GBASE_KR4].\n");
		hw->bp_link_mode = 40;
	} else if (com_link_ability & BIT(ABILITY_40GBASE_CR4)) {
		BP_LOG("Link mode is [ABILITY_40GBASE_CR4].\n");
		hw->bp_link_mode = 40;
	} else if (com_link_ability & BIT(ABILITY_25GBASE_KRCR_S)) {
		BP_LOG("Link mode is [ABILITY_25GBASE_KRCR_S].\n");
		hw->fec_mode = TXGBE_25G_RS_FEC_REQ;
		hw->bp_link_mode = 25;
	} else if (com_link_ability & BIT(ABILITY_25GBASE_KRCR)) {
		BP_LOG("Link mode is [ABILITY_25GBASE_KRCR].\n");
		hw->bp_link_mode = 25;
	} else if (com_link_ability & BIT(ABILITY_10GBASE_KR)) {
		BP_LOG("Link mode is [ABILITY_10GBASE_KR].\n");
		hw->bp_link_mode = 10;
	} else if (com_link_ability & BIT(ABILITY_10GBASE_KX4)) {
		BP_LOG("Link mode is [ABILITY_10GBASE_KX4].\n");
		hw->bp_link_mode = 10;
	} else if (com_link_ability & BIT(ABILITY_1000BASE_KX)) {
		BP_LOG("Link mode is [ABILITY_1000BASE_KX].\n");
		hw->bp_link_mode = 1;
	} else {
		BP_LOG("No compatible link mode found!\n");
		return -EINVAL;
	}

	return 0;
}

static int txgbe_e56_exchange_page(struct txgbe_hw *hw)
{
	struct txgbe_backplane_ability local_ability = {0}, lp_ability = {0};
	u32 an_int, base_page = 0;
	int count = 0;

	an_int = rd32_epcs(hw, 0x78002);
	/* 500ms timeout */
	if (!(an_int & VR_AN_INTR_PG_RCV))
		return -EINVAL;

	for (count = 0; count < 500; count++) {
		u32 fsm = rd32_epcs(hw, 0x78010);
		u32 rdata = rd32_epcs(hw, 0x78002);

		BP_LOG("-----count----- %d - fsm: %x\n", count, fsm);
		BP_LOG("read 78002 data %0x and clear pacv\n", rdata);
		an_int = rdata;
		set_fields_e56(&rdata, 2, 2, 0x0);
		wr32_epcs(hw, 0x78002, rdata);
		if (an_int & VR_AN_INTR_PG_RCV) {
			u32 addr;

			txgbe_e56_print_page_status(hw, &local_ability, &lp_ability);
			addr = base_page == 0 ? 0x70013 : 0x70019;
			rdata = rd32_epcs(hw, addr);
			if (rdata & BIT(14)) {
				if (rdata & BIT(15)) {
					/* always set null message */
					wr32_epcs(hw, 0x70016, 0x2001);
					BP_LOG("write 70016 0x%0x\n",
					       0x2001);
				}
				base_page = 1;
			}
		}
		if ((fsm & 0x8) == 0x8) {
			hw->fsm = 0x8;
			goto check_ability;
		}
		usec_delay(100);
	}

check_ability:
	return chk_bkp_ability(hw, local_ability, lp_ability);
}

static int txgbe_e56_cl72_training(struct txgbe_hw *hw)
{
	u32 bylinkmode = hw->bp_link_mode;
	u8 bypass_ctle = hw->bypass_ctle;
	int status = 0, temp_data = 0;
	u32 lane_num = 0, lane_idx = 0;
	u32 __rte_unused pmd_ctrl = 0, txffe = 0;
	int ret = 0;
	u32 rdata;

	u8 pll_en_cfg = 0;
	u8 pmd_mode = 0;

	switch (bylinkmode) {
	case 10:
		bylinkmode = 10;
		lane_num = 1;
		pll_en_cfg = 3;
		pmd_mode = 0;
		break;
	case 40:
		bylinkmode = 40;
		lane_num = 4;
		pll_en_cfg = 0; /* pll_en_cfg : single link to 0 */
		pmd_mode = 1; /* pmd mode : 1 - single link */
		break;
	case 25:
		bylinkmode = 25;
		lane_num = 1;
		pll_en_cfg = 3;
		pmd_mode = 0;
		break;
	default:
		BP_LOG("%s %d :Invalid speed\n", __func__, __LINE__);
		return -EINVAL;
	}

	BP_LOG("2.3 Wait %dG KR phy mode init ....\n", bylinkmode);
	status = txgbe_set_phy_link_mode(hw, bylinkmode);

	/* 13. set phy an status to 1 - AN_CFG[0]: 4-7 lane0-lane3 */
	rdata = rd32_ephy(hw, 0x1434);
	set_fields_e56(&rdata, 7, 4, GENMASK(lane_num - 1, 0));
	wr32_ephy(hw, 0x1434, rdata);

	/* 14 and 15. kr training: set BASER_PMD_CONTROL[0, 7] for lane0-4 */
	rdata = rd32_ephy(hw, 0x1640);
	set_fields_e56(&rdata, 7, 0, GENMASK(2 * lane_num - 1, 0));
	wr32_ephy(hw, 0x1640, rdata);

	/* 16. enable CMS and its internal PLL */
	rdata = rd32_ephy(hw, 0x1400);
	set_fields_e56(&rdata, 21, 20, pll_en_cfg);
	set_fields_e56(&rdata, 19, 12, 0); /* tx/rx off */
	set_fields_e56(&rdata, 8, 8, pmd_mode);
	set_fields_e56(&rdata, 1, 1, 0x1); /* pmd en */
	wr32_ephy(hw, 0x1400, rdata);

	/* 17. tx enable PMD_CFG[0] */
	rdata = rd32_ephy(hw, 0x1400);
	set_fields_e56(&rdata, 15, 12, GENMASK(lane_num - 1, 0)); /* tx en */
	wr32_ephy(hw, 0x1400, rdata);

	/* 18 */
	/* 19. rxs calibration and adaptation sequence */
	BP_LOG("2.4 Wait %dG RXS.... fsm: %x\n",
	       bylinkmode, rd32_epcs(hw, 0x78010));
	status = txgbe_e56_phy_rxs_calib_adapt_seq(hw, bylinkmode, bypass_ctle);
	ret |= status;
	/* 20 */
	BP_LOG("2.5 Wait %dG phy calibration.... fsm: %x\n",
	       bylinkmode, rd32_epcs(hw, 0x78010));
	txgbe_e56_set_rxs_ufine_le_max(hw, bylinkmode);
	status = txgbe_e56_get_temp(hw, &temp_data);
	if (bylinkmode == 40)
		status = txgbe_temp_track_seq_40g(hw, TXGBE_LINK_SPEED_40GB_FULL);
	else
		status = txgbe_e56_rxs_post_cdr_lock_temp_track_seq(hw, bylinkmode);
	/* 21 */
	BP_LOG("2.6 Wait %dG phy kr training check.... fsm: %x\n",
	       bylinkmode, rd32_epcs(hw, 0x78010));
	status = kr_read_poll(rd32_ephy, rdata,
				  ((rdata & 0xe) & GENMASK(lane_num, 1)) ==
				  (0xe & GENMASK(lane_num, 1)), 100,
				   10000, hw, 0x163c);
	pmd_ctrl = rd32_ephy(hw, 0x1644);
	BP_LOG("KR TRAINING CHECK = %x, %s. pmd_ctrl:%lx-%lx-%lx-%lx\n",
	       rdata, status ? "FAILED" : "SUCCESS",
	       FIELD_GET_M(GENMASK(3, 0), pmd_ctrl),
	       FIELD_GET_M(GENMASK(7, 4), pmd_ctrl),
	       FIELD_GET_M(GENMASK(11, 8), pmd_ctrl),
	       FIELD_GET_M(GENMASK(15, 12), pmd_ctrl));
	ret |= status;
	BP_LOG("before: %x-%x-%x-%x\n",
	       rd32_ephy(hw, 0x141c), rd32_ephy(hw, 0x1420),
	       rd32_ephy(hw, 0x1424), rd32_ephy(hw, 0x1428));

	for (lane_idx = 0; lane_idx < lane_num; lane_idx++) {
		txffe = rd32_ephy(hw, 0x828 + lane_idx * 0x100);
		BP_LOG("after[%x]: %lx-%lx-%lx-%lx\n", lane_idx,
		       FIELD_GET_M(GENMASK(6, 0), txffe),
		       FIELD_GET_M(GENMASK(21, 16), txffe),
		       FIELD_GET_M(GENMASK(29, 24), txffe),
		       FIELD_GET_M(GENMASK(13, 8), txffe));
	}

	/* 22 */
	BP_LOG("2.7 Wait %dG phy Rx adc.... fsm:%x\n",
	       bylinkmode, rd32_epcs(hw, 0x78010));
	status = txgbe_e56_rxs_adc_adapt_seq(hw, bypass_ctle);

	return ret;
}

int txgbe_handle_e56_bkp_an73_flow(struct txgbe_hw *hw)
{
	int status = 0;
	u32 rdata;

	BP_LOG("2.1 Wait page changed ....\n");
	status = txgbe_e56_exchange_page(hw);
	if (status) {
		BP_LOG("Exchange page failed\n");
		return status;
	}

	BP_LOG("2.2 Wait page changed ..done..\n");
	wr32_epcs(hw, 0x100ab, 0);
	if (AN_TRAINING_MODE) {
		rdata = rd32_epcs(hw, 0x70000);
		BP_LOG("read 0x70000 data %0x\n", rdata);
		wr32_epcs(hw, 0x70000, 0);
		BP_LOG("write 0x70000 0x%0x\n", 0);
	}

	rdata = rd32_epcs(hw, 0x78002);
	BP_LOG("read 78002 data %0x and clear page int\n", rdata);
	set_fields_e56(&rdata, 2, 2, 0x0);
	wr32_epcs(hw, 0x78002, rdata);

	/* dis phy tx/rx lane */
	rdata = rd32_ephy(hw, 0x1400);
	set_fields_e56(&rdata, 19, 16, 0x0);
	set_fields_e56(&rdata, 15, 12, 0x0);
	set_fields_e56(&rdata, 1, 1, 0x0);
	wr32_ephy(hw, 0x1400, rdata);
	BP_LOG("Ephy Write A: 0x%x, D: 0x%x\n", 0x1400, rdata);

	/* wait rx/tx/cm powerdn_st */
	status = kr_read_poll(rd32_ephy, rdata,
				   (rdata & GENMASK(3, 0)) == 0x9, 100,
				   2000, hw, 0x14d4);
	BP_LOG("wait ctrl_fsm_cm_st = %x, %s.\n",
	       rdata, status ? "FAILED" : "SUCCESS");

	if (hw->phy.fec_mode & TXGBE_25G_RS_FEC_REQ) {
		wr32_epcs(hw, 0x180a3, 0x68c1);
		wr32_epcs(hw, 0x180a4, 0x3321);
		wr32_epcs(hw, 0x180a5, 0x973e);
		wr32_epcs(hw, 0x180a6, 0xccde);

		wr32_epcs(hw, 0x38018, 1024);
		rdata = rd32_epcs(hw, 0x100c8);
		set_fields_e56(&rdata, 2, 2, 1);
		wr32_epcs(hw, 0x100c8, rdata);
		BP_LOG("Advertised FEC modes : %s\n", "RS-FEC");
		hw->cur_fec_link = TXGBE_PHY_FEC_RS;
	} else if (hw->phy.fec_mode & TXGBE_25G_BASE_FEC_REQ) {
		/* FEC: FC-FEC/BASE-R */
		wr32_epcs(hw, 0x100ab, BIT(0));
		BP_LOG("Epcs Write A: 0x%x,  D: 0x%x\n", 0x100ab, 1);
		PMD_DRV_LOG(INFO, "Advertised FEC modes : %s", "25GBASE-R");
		hw->cur_fec_link = TXGBE_PHY_FEC_BASER;
	} else if (hw->fec_mode & (TXGBE_10G_FEC_REQ)) {
		/* FEC: FC-FEC/BASE-R */
		wr32_epcs(hw, 0x100ab, BIT(0));
		BP_LOG("Epcs Write A: 0x%x,  D: 0x%x\n", 0x100ab, 1);
		PMD_DRV_LOG(INFO, "Advertised FEC modes : %s", "BASE-R");
		hw->cur_fec_link = TXGBE_PHY_FEC_BASER;
	} else {
		PMD_DRV_LOG(INFO, "Advertised FEC modes : %s", "NONE");
		hw->cur_fec_link = TXGBE_PHY_FEC_OFF;
	}

	status = txgbe_e56_cl72_training(hw);
	if (status) {
		BP_LOG("CL72 training failed, status = %d\n", status);
		return status;
	}

	rdata = rd32_ephy(hw, E56PHY_RXS_IDLE_DETECT_1_ADDR);
	set_fields_e56(&rdata, E56PHY_RXS_IDLE_DETECT_1_IDLE_TH_ADC_PEAK_MAX, 0x28);
	set_fields_e56(&rdata, E56PHY_RXS_IDLE_DETECT_1_IDLE_TH_ADC_PEAK_MIN, 0xa);
	wr32_ephy(hw, E56PHY_RXS_IDLE_DETECT_1_ADDR, rdata);
	wr32_ephy(hw, E56PHY_INTR_0_ADDR, E56PHY_INTR_0_IDLE_ENTRY1);
	wr32_ephy(hw, E56PHY_INTR_1_ADDR, E56PHY_INTR_1_IDLE_EXIT1);
	wr32_ephy(hw, E56PHY_INTR_0_ENABLE_ADDR, E56PHY_INTR_0_IDLE_ENTRY1);
	wr32_ephy(hw, E56PHY_INTR_1_ENABLE_ADDR, E56PHY_INTR_1_IDLE_EXIT1);

	return status;
}
