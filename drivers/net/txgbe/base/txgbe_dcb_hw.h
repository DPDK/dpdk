/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#ifndef _TXGBE_DCB_HW_H_
#define _TXGBE_DCB_HW_H_

/* DCB PFC */
s32 txgbe_dcb_config_pfc_raptor(struct txgbe_hw *hw, u8 pfc_en, u8 *map);

/* DCB stats */
s32 txgbe_dcb_config_tc_stats_raptor(struct txgbe_hw *hw,
				    struct txgbe_dcb_config *dcb_config);

/* DCB config arbiters */
s32 txgbe_dcb_config_tx_desc_arbiter_raptor(struct txgbe_hw *hw, u16 *refill,
					   u16 *max, u8 *bwg_id, u8 *tsa);
s32 txgbe_dcb_config_tx_data_arbiter_raptor(struct txgbe_hw *hw, u16 *refill,
					   u16 *max, u8 *bwg_id, u8 *tsa,
					   u8 *map);
s32 txgbe_dcb_config_rx_arbiter_raptor(struct txgbe_hw *hw, u16 *refill,
				      u16 *max, u8 *bwg_id, u8 *tsa, u8 *map);

#endif /* _TXGBE_DCB_HW_H_ */
