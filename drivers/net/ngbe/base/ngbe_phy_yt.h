/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 */

#include "ngbe_phy.h"

#ifndef _NGBE_PHY_YT_H_
#define _NGBE_PHY_YT_H_

#define NGBE_PHYID_YT			0x00000110U

/* Common EXT */
#define YT_SMI_PHY			0xA000
#define   YT_SMI_PHY_SW_RST		MS16(15, 0x1)
#define   YT_SMI_PHY_SDS		MS16(1, 0x1) /* 0 for UTP */
#define YT_CHIP				0xA001
#define   YT_CHIP_SW_RST		MS16(15, 0x1)
#define   YT_CHIP_SW_LDO_EN		MS16(6, 0x1)
#define   YT_CHIP_MODE_MASK		MS16(0, 0x7)
#define   YT_CHIP_MODE_SEL(v)		LS16(v, 0, 0x7)
#define YT_RGMII_CONF1			0xA003
#define   YT_RGMII_CONF1_MODE		MS16(15, 0x1)
#define   YT_RGMII_CONF1_RXDELAY	MS16(10, 0xF)
#define   YT_RGMII_CONF1_TXDELAY_FE	MS16(4, 0xF)
#define   YT_RGMII_CONF1_TXDELAY	MS16(0, 0x1)
#define YT_RGMII_CONF2			0xA004
#define   YT_RGMII_CONF2_SPEED_MASK	MS16(6, 0x3)
#define   YT_RGMII_CONF2_SPEED(v)	LS16(v, 6, 0x3)
#define   YT_RGMII_CONF2_DUPLEX		MS16(5, 0x1)
#define   YT_RGMII_CONF2_LINKUP		MS16(4, 0x1)
#define YT_MISC				0xA006
#define   YT_MISC_FIBER_PRIO		MS16(8, 0x1) /* 0 for UTP */

/* MII common registers in UTP and SDS */
#define YT_BCR				0x0
#define   YT_BCR_RESET			MS16(15, 0x1)
#define	  YT_BCR_SPEED_SELECT0		MS16(13, 0x1)
#define   YT_BCR_ANE			MS16(12, 0x1)
#define   YT_BCR_PWDN			MS16(11, 0x1)
#define   YT_BCR_RESTART_AN		MS16(9, 0x1)
#define   YT_BCR_DUPLEX			MS16(8, 0x1)
#define   YT_BCR_SPEED_SELECT1		MS16(6, 0x1)
#define YT_ANA				0x4
/* copper */
#define   YT_ANA_100BASET_FULL		MS16(8, 0x1)
#define   YT_ANA_100BASET_HALF		MS16(7, 0x1)
#define   YT_ANA_10BASET_FULL		MS16(6, 0x1)
#define   YT_ANA_10BASET_HALF		MS16(5, 0x1)
/* fiber */
#define   YT_FANA_PAUSE_MASK		MS16(7, 0x3)

#define YT_LPAR				0x5
#define   YT_CLPAR_ASM_PAUSE		MS(11, 0x1)
#define   YT_CLPAR_PAUSE		MS(10, 0x1)
#define   YT_FLPAR_PAUSE_MASK		MS(7, 0x3)

#define YT_MS_CTRL			0x9
#define   YT_MS_1000BASET_FULL		MS16(9, 0x1)
#define   YT_MS_1000BASET_HALF		MS16(8, 0x1)
#define YT_SPST				0x11
#define   YT_SPST_SPEED_MASK		MS16(14, 0x3)
#define	    YT_SPST_SPEED_1000M		LS16(2, 14, 0x3)
#define	    YT_SPST_SPEED_100M		LS16(1, 14, 0x3)
#define	    YT_SPST_SPEED_10M		LS16(0, 14, 0x3)
#define   YT_SPST_LINK			MS16(10, 0x1)

/* UTP only */
#define YT_INTR				0x12
#define   YT_INTR_ENA_MASK		MS16(10, 0x3)
#define   YT_SDS_INTR_ENA_MASK		MS16(2, 0x3)
#define YT_INTR_STATUS			0x13

s32 ngbe_read_phy_reg_yt(struct ngbe_hw *hw, u32 reg_addr, u32 device_type,
			u16 *phy_data);
s32 ngbe_write_phy_reg_yt(struct ngbe_hw *hw, u32 reg_addr, u32 device_type,
			u16 phy_data);
s32 ngbe_read_phy_reg_ext_yt(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 *phy_data);
s32 ngbe_write_phy_reg_ext_yt(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 phy_data);
s32 ngbe_read_phy_reg_sds_ext_yt(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 *phy_data);
s32 ngbe_write_phy_reg_sds_ext_yt(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 phy_data);
s32 ngbe_init_phy_yt(struct ngbe_hw *hw);

s32 ngbe_reset_phy_yt(struct ngbe_hw *hw);

s32 ngbe_check_phy_link_yt(struct ngbe_hw *hw,
		u32 *speed, bool *link_up);
s32 ngbe_setup_phy_link_yt(struct ngbe_hw *hw,
			u32 speed, bool autoneg_wait_to_complete);
s32 ngbe_get_phy_advertised_pause_yt(struct ngbe_hw *hw,
				u8 *pause_bit);
s32 ngbe_get_phy_lp_advertised_pause_yt(struct ngbe_hw *hw,
						u8 *pause_bit);
s32 ngbe_set_phy_pause_adv_yt(struct ngbe_hw *hw, u16 pause_bit);

#endif /* _NGBE_PHY_YT_H_ */
