/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 */

#include "ngbe_phy.h"
#include "ngbe_hw.h"

#ifndef _NGBE_PHY_MVL_H_
#define _NGBE_PHY_MVL_H_

#define NGBE_PHYID_MVL			0x01410DD0U

/* Page 0 for Copper, Page 1 for Fiber */
#define MVL_CTRL			0x0
#define   MVL_CTRL_RESET		MS16(15, 0x1)
#define	  MVL_CTRL_SPEED_SELECT0	MS16(13, 0x1)
#define   MVL_CTRL_ANE			MS16(12, 0x1)
#define   MVL_CTRL_PWDN			MS16(11, 0x1)
#define   MVL_CTRL_RESTART_AN		MS16(9, 0x1)
#define   MVL_CTRL_DUPLEX		MS16(8, 0x1)
#define	  MVL_CTRL_SPEED_SELECT1	MS16(6, 0x1)
#define MVL_ANA				0x4
/* copper */
#define   MVL_CANA_ASM_PAUSE		MS16(11, 0x1)
#define   MVL_CANA_PAUSE		MS16(10, 0x1)
#define   MVL_PHY_100BASET_FULL		MS16(8, 0x1)
#define   MVL_PHY_100BASET_HALF		MS16(7, 0x1)
#define   MVL_PHY_10BASET_FULL		MS16(6, 0x1)
#define   MVL_PHY_10BASET_HALF		MS16(5, 0x1)
/* fiber */
#define   MVL_FANA_PAUSE_MASK		MS16(7, 0x3)
#define     MVL_FANA_SYM_PAUSE		LS16(1, 7, 0x3)
#define     MVL_FANA_ASM_PAUSE		LS16(2, 7, 0x3)
#define   MVL_PHY_1000BASEX_HALF	MS16(6, 0x1)
#define   MVL_PHY_1000BASEX_FULL	MS16(5, 0x1)
#define MVL_LPAR			0x5
#define   MVL_CLPAR_ASM_PAUSE		MS(11, 0x1)
#define   MVL_CLPAR_PAUSE		MS(10, 0x1)
#define   MVL_FLPAR_PAUSE_MASK		MS(7, 0x3)
#define MVL_PHY_1000BASET		0x9
#define   MVL_PHY_1000BASET_FULL	MS16(9, 0x1)
#define   MVL_PHY_1000BASET_HALF	MS16(8, 0x1)
#define MVL_CTRL1			0x10
#define   MVL_CTRL1_INTR_POL		MS16(2, 0x1)
#define MVL_PHYSR			0x11
#define   MVL_PHYSR_SPEED_MASK		MS16(14, 0x3)
#define     MVL_PHYSR_SPEED_1000M	LS16(2, 14, 0x3)
#define     MVL_PHYSR_SPEED_100M	LS16(1, 14, 0x3)
#define     MVL_PHYSR_SPEED_10M		LS16(0, 14, 0x3)
#define   MVL_PHYSR_LINK		MS16(10, 0x1)
#define MVL_INTR_EN			0x12
#define   MVL_INTR_EN_ANC		MS16(11, 0x1)
#define   MVL_INTR_EN_LSC		MS16(10, 0x1)
#define MVL_INTR			0x13
#define   MVL_INTR_ANC			MS16(11, 0x1)
#define   MVL_INTR_LSC			MS16(10, 0x1)

/* Page 2 */
#define MVL_RGM_CTL2			0x15
#define   MVL_RGM_CTL2_TTC		MS16(4, 0x1)
#define   MVL_RGM_CTL2_RTC		MS16(5, 0x1)
/* Page 3 */
#define MVL_LEDFCR			0x10
#define   MVL_LEDFCR_CTL1		MS16(4, 0xF)
#define     MVL_LEDFCR_CTL1_CONF	LS16(6, 4, 0xF)
#define   MVL_LEDFCR_CTL0		MS16(0, 0xF)
#define     MVL_LEDFCR_CTL0_CONF	LS16(1, 0, 0xF)
#define MVL_LEDPCR			0x11
#define   MVL_LEDPCR_CTL1		MS16(2, 0x3)
#define     MVL_LEDPCR_CTL1_CONF	LS16(1, 2, 0x3)
#define   MVL_LEDPCR_CTL0		MS16(0, 0x3)
#define     MVL_LEDPCR_CTL0_CONF	LS16(1, 0, 0x3)
#define MVL_LEDTCR			0x12
#define   MVL_LEDTCR_INTR_POL		MS16(11, 0x1)
#define   MVL_LEDTCR_INTR_EN		MS16(7, 0x1)
/* Page 18 */
#define MVL_GEN_CTL			0x14
#define   MVL_GEN_CTL_RESET		MS16(15, 0x1)
#define   MVL_GEN_CTL_MODE(v)		LS16(v, 0, 0x7)
#define     MVL_GEN_CTL_MODE_COPPER	LS16(0, 0, 0x7)
#define     MVL_GEN_CTL_MODE_FIBER	LS16(2, 0, 0x7)

/* reg 22 */
#define MVL_PAGE_SEL			22

/* reg 19_0 INT status*/
#define MVL_PHY_ANC                      0x0800
#define MVL_PHY_LSC                      0x0400

s32 ngbe_read_phy_reg_mvl(struct ngbe_hw *hw, u32 reg_addr, u32 device_type,
			u16 *phy_data);
s32 ngbe_write_phy_reg_mvl(struct ngbe_hw *hw, u32 reg_addr, u32 device_type,
			u16 phy_data);
s32 ngbe_check_phy_mode_mvl(struct ngbe_hw *hw);
s32 ngbe_init_phy_mvl(struct ngbe_hw *hw);

s32 ngbe_reset_phy_mvl(struct ngbe_hw *hw);

s32 ngbe_check_phy_link_mvl(struct ngbe_hw *hw,
		u32 *speed, bool *link_up);
s32 ngbe_set_phy_power_mvl(struct ngbe_hw *hw, bool on);
s32 ngbe_setup_phy_link_mvl(struct ngbe_hw *hw,
			u32 speed, bool autoneg_wait_to_complete);
s32 ngbe_get_phy_advertised_pause_mvl(struct ngbe_hw *hw, u8 *pause_bit);
s32 ngbe_get_phy_lp_advertised_pause_mvl(struct ngbe_hw *hw, u8 *pause_bit);
s32 ngbe_set_phy_pause_adv_mvl(struct ngbe_hw *hw, u16 pause_bit);

#endif /* _NGBE_PHY_MVL_H_ */
