/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#include "txgbe_type.h"
#include "txgbe_hw.h"
#include "txgbe_dcb.h"
#include "txgbe_dcb_hw.h"

/**
 * txgbe_dcb_calculate_tc_credits_cee - Calculates traffic class credits
 * @hw: pointer to hardware structure
 * @dcb_config: Struct containing DCB settings
 * @max_frame_size: Maximum frame size
 * @direction: Configuring either Tx or Rx
 *
 * This function calculates the credits allocated to each traffic class.
 * It should be called only after the rules are checked by
 * txgbe_dcb_check_config_cee().
 */
s32 txgbe_dcb_calculate_tc_credits_cee(struct txgbe_hw *hw,
				   struct txgbe_dcb_config *dcb_config,
				   u32 max_frame_size, u8 direction)
{
	struct txgbe_dcb_tc_path *p;
	u32 min_multiplier	= 0;
	u16 min_percent		= 100;
	s32 ret_val =		0;
	/* Initialization values default for Tx settings */
	u32 min_credit		= 0;
	u32 credit_refill	= 0;
	u32 credit_max		= 0;
	u16 link_percentage	= 0;
	u8  bw_percent		= 0;
	u8  i;

	UNREFERENCED_PARAMETER(hw);

	if (dcb_config == NULL) {
		ret_val = TXGBE_ERR_CONFIG;
		goto out;
	}

	min_credit = ((max_frame_size / 2) + TXGBE_DCB_CREDIT_QUANTUM - 1) /
		     TXGBE_DCB_CREDIT_QUANTUM;

	/* Find smallest link percentage */
	for (i = 0; i < TXGBE_DCB_TC_MAX; i++) {
		p = &dcb_config->tc_config[i].path[direction];
		bw_percent = dcb_config->bw_percentage[p->bwg_id][direction];
		link_percentage = p->bwg_percent;

		link_percentage = (link_percentage * bw_percent) / 100;

		if (link_percentage && link_percentage < min_percent)
			min_percent = link_percentage;
	}

	/*
	 * The ratio between traffic classes will control the bandwidth
	 * percentages seen on the wire. To calculate this ratio we use
	 * a multiplier. It is required that the refill credits must be
	 * larger than the max frame size so here we find the smallest
	 * multiplier that will allow all bandwidth percentages to be
	 * greater than the max frame size.
	 */
	min_multiplier = (min_credit / min_percent) + 1;

	/* Find out the link percentage for each TC first */
	for (i = 0; i < TXGBE_DCB_TC_MAX; i++) {
		p = &dcb_config->tc_config[i].path[direction];
		bw_percent = dcb_config->bw_percentage[p->bwg_id][direction];

		link_percentage = p->bwg_percent;
		/* Must be careful of integer division for very small nums */
		link_percentage = (link_percentage * bw_percent) / 100;
		if (p->bwg_percent > 0 && link_percentage == 0)
			link_percentage = 1;

		/* Save link_percentage for reference */
		p->link_percent = (u8)link_percentage;

		/* Calculate credit refill ratio using multiplier */
		credit_refill = min(link_percentage * min_multiplier,
				    (u32)TXGBE_DCB_MAX_CREDIT_REFILL);

		/* Refill at least minimum credit */
		if (credit_refill < min_credit)
			credit_refill = min_credit;

		p->data_credits_refill = (u16)credit_refill;

		/* Calculate maximum credit for the TC */
		credit_max = (link_percentage * TXGBE_DCB_MAX_CREDIT) / 100;

		/*
		 * Adjustment based on rule checking, if the percentage
		 * of a TC is too small, the maximum credit may not be
		 * enough to send out a jumbo frame in data plane arbitration.
		 */
		if (credit_max < min_credit)
			credit_max = min_credit;

		if (direction == TXGBE_DCB_TX_CONFIG) {
			dcb_config->tc_config[i].desc_credits_max =
								(u16)credit_max;
		}

		p->data_credits_max = (u16)credit_max;
	}

out:
	return ret_val;
}

/**
 * txgbe_dcb_unpack_pfc_cee - Unpack dcb_config PFC info
 * @cfg: dcb configuration to unpack into hardware consumable fields
 * @map: user priority to traffic class map
 * @pfc_up: u8 to store user priority PFC bitmask
 *
 * This unpacks the dcb configuration PFC info which is stored per
 * traffic class into a 8bit user priority bitmask that can be
 * consumed by hardware routines. The priority to tc map must be
 * updated before calling this routine to use current up-to maps.
 */
void txgbe_dcb_unpack_pfc_cee(struct txgbe_dcb_config *cfg, u8 *map, u8 *pfc_up)
{
	struct txgbe_dcb_tc_config *tc_config = &cfg->tc_config[0];
	int up;

	/*
	 * If the TC for this user priority has PFC enabled then set the
	 * matching bit in 'pfc_up' to reflect that PFC is enabled.
	 */
	for (*pfc_up = 0, up = 0; up < TXGBE_DCB_UP_MAX; up++) {
		if (tc_config[map[up]].pfc != txgbe_dcb_pfc_disabled)
			*pfc_up |= 1 << up;
	}
}

void txgbe_dcb_unpack_refill_cee(struct txgbe_dcb_config *cfg, int direction,
			     u16 *refill)
{
	struct txgbe_dcb_tc_config *tc_config = &cfg->tc_config[0];
	int tc;

	for (tc = 0; tc < TXGBE_DCB_TC_MAX; tc++)
		refill[tc] = tc_config[tc].path[direction].data_credits_refill;
}

void txgbe_dcb_unpack_max_cee(struct txgbe_dcb_config *cfg, u16 *max)
{
	struct txgbe_dcb_tc_config *tc_config = &cfg->tc_config[0];
	int tc;

	for (tc = 0; tc < TXGBE_DCB_TC_MAX; tc++)
		max[tc] = tc_config[tc].desc_credits_max;
}

void txgbe_dcb_unpack_bwgid_cee(struct txgbe_dcb_config *cfg, int direction,
			    u8 *bwgid)
{
	struct txgbe_dcb_tc_config *tc_config = &cfg->tc_config[0];
	int tc;

	for (tc = 0; tc < TXGBE_DCB_TC_MAX; tc++)
		bwgid[tc] = tc_config[tc].path[direction].bwg_id;
}

void txgbe_dcb_unpack_tsa_cee(struct txgbe_dcb_config *cfg, int direction,
			   u8 *tsa)
{
	struct txgbe_dcb_tc_config *tc_config = &cfg->tc_config[0];
	int tc;

	for (tc = 0; tc < TXGBE_DCB_TC_MAX; tc++)
		tsa[tc] = tc_config[tc].path[direction].tsa;
}

u8 txgbe_dcb_get_tc_from_up(struct txgbe_dcb_config *cfg, int direction, u8 up)
{
	struct txgbe_dcb_tc_config *tc_config = &cfg->tc_config[0];
	u8 prio_mask = 1 << up;
	u8 tc = cfg->num_tcs.pg_tcs;

	/* If tc is 0 then DCB is likely not enabled or supported */
	if (!tc)
		goto out;

	/*
	 * Test from maximum TC to 1 and report the first match we find.  If
	 * we find no match we can assume that the TC is 0 since the TC must
	 * be set for all user priorities
	 */
	for (tc--; tc; tc--) {
		if (prio_mask & tc_config[tc].path[direction].up_to_tc_bitmap)
			break;
	}
out:
	return tc;
}

void txgbe_dcb_unpack_map_cee(struct txgbe_dcb_config *cfg, int direction,
			      u8 *map)
{
	u8 up;

	for (up = 0; up < TXGBE_DCB_UP_MAX; up++)
		map[up] = txgbe_dcb_get_tc_from_up(cfg, direction, up);
}

