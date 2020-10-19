/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#include "txgbe_type.h"

#include "txgbe_dcb.h"

/**
 * txgbe_dcb_config_rx_arbiter_raptor - Config Rx Data arbiter
 * @hw: pointer to hardware structure
 * @refill: refill credits index by traffic class
 * @max: max credits index by traffic class
 * @bwg_id: bandwidth grouping indexed by traffic class
 * @tsa: transmission selection algorithm indexed by traffic class
 * @map: priority to tc assignments indexed by priority
 *
 * Configure Rx Packet Arbiter and credits for each traffic class.
 */
s32 txgbe_dcb_config_rx_arbiter_raptor(struct txgbe_hw *hw, u16 *refill,
				      u16 *max, u8 *bwg_id, u8 *tsa,
				      u8 *map)
{
	u32 reg = 0;
	u32 credit_refill = 0;
	u32 credit_max = 0;
	u8  i = 0;

	/*
	 * Disable the arbiter before changing parameters
	 * (always enable recycle mode; WSP)
	 */
	reg = TXGBE_ARBRXCTL_RRM | TXGBE_ARBRXCTL_WSP |
	      TXGBE_ARBRXCTL_DIA;
	wr32(hw, TXGBE_ARBRXCTL, reg);

	/*
	 * map all UPs to TCs. up_to_tc_bitmap for each TC has corresponding
	 * bits sets for the UPs that needs to be mappped to that TC.
	 * e.g if priorities 6 and 7 are to be mapped to a TC then the
	 * up_to_tc_bitmap value for that TC will be 11000000 in binary.
	 */
	reg = 0;
	for (i = 0; i < TXGBE_DCB_UP_MAX; i++)
		reg |= (map[i] << (i * TXGBE_RPUP2TC_UP_SHIFT));

	wr32(hw, TXGBE_RPUP2TC, reg);

	/* Configure traffic class credits and priority */
	for (i = 0; i < TXGBE_DCB_TC_MAX; i++) {
		credit_refill = refill[i];
		credit_max = max[i];
		reg = TXGBE_QARBRXCFG_CRQ(credit_refill) |
		      TXGBE_QARBRXCFG_MCL(credit_max) |
		      TXGBE_QARBRXCFG_BWG(bwg_id[i]);

		if (tsa[i] == txgbe_dcb_tsa_strict)
			reg |= TXGBE_QARBRXCFG_LSP;

		wr32(hw, TXGBE_QARBRXCFG(i), reg);
	}

	/*
	 * Configure Rx packet plane (recycle mode; WSP) and
	 * enable arbiter
	 */
	reg = TXGBE_ARBRXCTL_RRM | TXGBE_ARBRXCTL_WSP;
	wr32(hw, TXGBE_ARBRXCTL, reg);

	return 0;
}

/**
 * txgbe_dcb_config_tx_desc_arbiter_raptor - Config Tx Desc. arbiter
 * @hw: pointer to hardware structure
 * @refill: refill credits index by traffic class
 * @max: max credits index by traffic class
 * @bwg_id: bandwidth grouping indexed by traffic class
 * @tsa: transmission selection algorithm indexed by traffic class
 *
 * Configure Tx Descriptor Arbiter and credits for each traffic class.
 */
s32 txgbe_dcb_config_tx_desc_arbiter_raptor(struct txgbe_hw *hw, u16 *refill,
					   u16 *max, u8 *bwg_id, u8 *tsa)
{
	u32 reg, max_credits;
	u8  i;

	/* Clear the per-Tx queue credits; we use per-TC instead */
	for (i = 0; i < 128; i++)
		wr32(hw, TXGBE_QARBTXCRED(i), 0);

	/* Configure traffic class credits and priority */
	for (i = 0; i < TXGBE_DCB_TC_MAX; i++) {
		max_credits = max[i];
		reg = TXGBE_QARBTXCFG_MCL(max_credits) |
		      TXGBE_QARBTXCFG_CRQ(refill[i]) |
		      TXGBE_QARBTXCFG_BWG(bwg_id[i]);

		if (tsa[i] == txgbe_dcb_tsa_group_strict_cee)
			reg |= TXGBE_QARBTXCFG_GSP;

		if (tsa[i] == txgbe_dcb_tsa_strict)
			reg |= TXGBE_QARBTXCFG_LSP;

		wr32(hw, TXGBE_QARBTXCFG(i), reg);
	}

	/*
	 * Configure Tx descriptor plane (recycle mode; WSP) and
	 * enable arbiter
	 */
	reg = TXGBE_ARBTXCTL_WSP | TXGBE_ARBTXCTL_RRM;
	wr32(hw, TXGBE_ARBTXCTL, reg);

	return 0;
}

/**
 * txgbe_dcb_config_tx_data_arbiter_raptor - Config Tx Data arbiter
 * @hw: pointer to hardware structure
 * @refill: refill credits index by traffic class
 * @max: max credits index by traffic class
 * @bwg_id: bandwidth grouping indexed by traffic class
 * @tsa: transmission selection algorithm indexed by traffic class
 * @map: priority to tc assignments indexed by priority
 *
 * Configure Tx Packet Arbiter and credits for each traffic class.
 */
s32 txgbe_dcb_config_tx_data_arbiter_raptor(struct txgbe_hw *hw, u16 *refill,
					   u16 *max, u8 *bwg_id, u8 *tsa,
					   u8 *map)
{
	u32 reg;
	u8 i;

	/*
	 * Disable the arbiter before changing parameters
	 * (always enable recycle mode; SP; arb delay)
	 */
	reg = TXGBE_PARBTXCTL_SP |
	      TXGBE_PARBTXCTL_RECYC |
	      TXGBE_PARBTXCTL_DA;
	wr32(hw, TXGBE_PARBTXCTL, reg);

	/*
	 * map all UPs to TCs. up_to_tc_bitmap for each TC has corresponding
	 * bits sets for the UPs that needs to be mappped to that TC.
	 * e.g if priorities 6 and 7 are to be mapped to a TC then the
	 * up_to_tc_bitmap value for that TC will be 11000000 in binary.
	 */
	reg = 0;
	for (i = 0; i < TXGBE_DCB_UP_MAX; i++)
		reg |= TXGBE_DCBUP2TC_MAP(i, map[i]);

	wr32(hw, TXGBE_PBRXUP2TC, reg);

	/* Configure traffic class credits and priority */
	for (i = 0; i < TXGBE_DCB_TC_MAX; i++) {
		reg = TXGBE_PARBTXCFG_CRQ(refill[i]) |
		      TXGBE_PARBTXCFG_MCL(max[i]) |
		      TXGBE_PARBTXCFG_BWG(bwg_id[i]);

		if (tsa[i] == txgbe_dcb_tsa_group_strict_cee)
			reg |= TXGBE_PARBTXCFG_GSP;

		if (tsa[i] == txgbe_dcb_tsa_strict)
			reg |= TXGBE_PARBTXCFG_LSP;

		wr32(hw, TXGBE_PARBTXCFG(i), reg);
	}

	/*
	 * Configure Tx packet plane (recycle mode; SP; arb delay) and
	 * enable arbiter
	 */
	reg = TXGBE_PARBTXCTL_SP | TXGBE_PARBTXCTL_RECYC;
	wr32(hw, TXGBE_PARBTXCTL, reg);

	return 0;
}

/**
 * txgbe_dcb_config_tc_stats_raptor - Config traffic class statistics
 * @hw: pointer to hardware structure
 * @dcb_config: pointer to txgbe_dcb_config structure
 *
 * Configure queue statistics registers, all queues belonging to same traffic
 * class uses a single set of queue statistics counters.
 */
s32 txgbe_dcb_config_tc_stats_raptor(struct txgbe_hw *hw,
				    struct txgbe_dcb_config *dcb_config)
{
	u8 tc_count = 8;
	bool vt_mode = false;

	UNREFERENCED_PARAMETER(hw);

	if (dcb_config != NULL) {
		tc_count = dcb_config->num_tcs.pg_tcs;
		vt_mode = dcb_config->vt_mode;
	}

	if (!((tc_count == 8 && !vt_mode) || tc_count == 4))
		return TXGBE_ERR_PARAM;

	return 0;
}

