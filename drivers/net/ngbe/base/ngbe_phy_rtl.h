/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 */

#include "ngbe_phy.h"

#ifndef _NGBE_PHY_RTL_H_
#define _NGBE_PHY_RTL_H_

#define NGBE_PHYID_RTL			0x001CC800U

/* Page 0 */
#define RTL_DEV_ZERO			0
#define RTL_BMCR			0x0
#define   RTL_BMCR_RESET		MS16(15, 0x1)
#define	  RTL_BMCR_SPEED_SELECT0	MS16(13, 0x1)
#define   RTL_BMCR_ANE			MS16(12, 0x1)
#define   RTL_BMCR_PWDN			MS16(11, 0x1)
#define   RTL_BMCR_RESTART_AN		MS16(9, 0x1)
#define   RTL_BMCR_DUPLEX		MS16(8, 0x1)
#define   RTL_BMCR_SPEED_SELECT1	MS16(6, 0x1)
#define RTL_BMSR			0x1
#define   RTL_BMSR_ANC			MS16(5, 0x1)
#define RTL_ID1_OFFSET			0x2
#define RTL_ID2_OFFSET			0x3
#define RTL_ID_MASK			0xFFFFFC00U
#define RTL_ANAR			0x4
#define   RTL_ANAR_APAUSE		MS16(11, 0x1)
#define   RTL_ANAR_PAUSE		MS16(10, 0x1)
#define   RTL_ANAR_100F			MS16(8, 0x1)
#define   RTL_ANAR_100H			MS16(7, 0x1)
#define   RTL_ANAR_10F			MS16(6, 0x1)
#define   RTL_ANAR_10H			MS16(5, 0x1)
#define RTL_ANLPAR			0x5
#define   RTL_ANLPAR_LP			MS16(10, 0x3)
#define RTL_GBCR			0x9
#define   RTL_GBCR_1000F		MS16(9, 0x1)
#define RTL_GBSR			0xA
#define   RTL_GBSR_LRS			MS16(13, 0x1)
/* Page 0xa42*/
#define RTL_GSR				0x10
#define   RTL_GSR_ST			MS16(0, 0x7)
#define   RTL_GSR_ST_LANON		MS16(0, 0x3)
#define RTL_INER			0x12
#define   RTL_INER_LSC			MS16(4, 0x1)
#define   RTL_INER_ANC			MS16(3, 0x1)
/* Page 0xa43*/
#define RTL_PHYSR			0x1A
#define   RTL_PHYSR_SPEED_MASK		MS16(4, 0x3)
#define     RTL_PHYSR_SPEED_RES		LS16(3, 4, 0x3)
#define     RTL_PHYSR_SPEED_1000M	LS16(2, 4, 0x3)
#define     RTL_PHYSR_SPEED_100M	LS16(1, 4, 0x3)
#define     RTL_PHYSR_SPEED_10M		LS16(0, 4, 0x3)
#define   RTL_PHYSR_DP			MS16(3, 0x1)
#define   RTL_PHYSR_RTLS		MS16(2, 0x1)
#define RTL_INSR			0x1D
#define   RTL_INSR_ACCESS		MS16(5, 0x1)
#define   RTL_INSR_LSC			MS16(4, 0x1)
#define   RTL_INSR_ANC			MS16(3, 0x1)
/* Page 0xa46*/
#define RTL_SCR				0x14
#define   RTL_SCR_EXTINI		MS16(1, 0x1)
#define   RTL_SCR_EFUSE			MS16(0, 0x1)
/* Page 0xa47*/
/* Page 0xd04*/
#define RTL_LCR				0x10
#define RTL_EEELCR			0x11
#define RTL_LPCR			0x12

/* INTERNAL PHY CONTROL */
#define RTL_PAGE_SELECT			31
#define NGBE_INTERNAL_PHY_OFFSET_MAX	32
#define NGBE_INTERNAL_PHY_ID		0x000732

#define NGBE_INTPHY_LED0		0x0010
#define NGBE_INTPHY_LED1		0x0040
#define NGBE_INTPHY_LED2		0x2000

s32 ngbe_read_phy_reg_rtl(struct ngbe_hw *hw, u32 reg_addr, u32 device_type,
			u16 *phy_data);
s32 ngbe_write_phy_reg_rtl(struct ngbe_hw *hw, u32 reg_addr, u32 device_type,
			u16 phy_data);

s32 ngbe_setup_phy_link_rtl(struct ngbe_hw *hw,
		u32 speed, bool autoneg_wait_to_complete);

s32 ngbe_init_phy_rtl(struct ngbe_hw *hw);
s32 ngbe_reset_phy_rtl(struct ngbe_hw *hw);
s32 ngbe_get_phy_advertised_pause_rtl(struct ngbe_hw *hw, u8 *pause_bit);
s32 ngbe_get_phy_lp_advertised_pause_rtl(struct ngbe_hw *hw, u8 *pause_bit);
s32 ngbe_set_phy_pause_adv_rtl(struct ngbe_hw *hw, u16 pause_bit);
s32 ngbe_check_phy_link_rtl(struct ngbe_hw *hw,
			u32 *speed, bool *link_up);
s32 ngbe_set_phy_power_rtl(struct ngbe_hw *hw, bool on);

#endif /* _NGBE_PHY_RTL_H_ */
