/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _NGBE_PHY_H_
#define _NGBE_PHY_H_

#include "ngbe_type.h"
#include "ngbe_phy_rtl.h"
#include "ngbe_phy_mvl.h"
#include "ngbe_phy_yt.h"

/******************************************************************************
 * PHY MDIO Registers:
 ******************************************************************************/
#define NGBE_MAX_PHY_ADDR		32

/* (dev_type = 1) */
#define NGBE_MD_DEV_PMA_PMD		0x1
#define NGBE_MD_PHY_ID_HIGH		0x2 /* PHY ID High Reg*/
#define NGBE_MD_PHY_ID_LOW		0x3 /* PHY ID Low Reg*/
#define   NGBE_PHY_REVISION_MASK	0xFFFFFFF0

#define NGBE_MII_AUTONEG_REG			0x0

/* IEEE 802.3 Clause 22 */
struct mdi_reg_22 {
	u16 page;
	u16 addr;
	u16 device_type;
};
typedef struct mdi_reg_22 mdi_reg_22_t;

/* IEEE 802.3ae Clause 45 */
struct mdi_reg {
	u16 device_type;
	u16 addr;
};
typedef struct mdi_reg mdi_reg_t;

#define NGBE_MD22_PHY_ID_HIGH		0x2 /* PHY ID High Reg*/
#define NGBE_MD22_PHY_ID_LOW		0x3 /* PHY ID Low Reg*/

#define NGBE_TAF_SYM_PAUSE		0x1
#define NGBE_TAF_ASM_PAUSE		0x2

s32 ngbe_mdi_map_register(mdi_reg_t *reg, mdi_reg_22_t *reg22);

bool ngbe_validate_phy_addr(struct ngbe_hw *hw, u32 phy_addr);
s32 ngbe_get_phy_type_from_id(struct ngbe_hw *hw);
s32 ngbe_get_phy_id(struct ngbe_hw *hw);
s32 ngbe_identify_phy(struct ngbe_hw *hw);
s32 ngbe_reset_phy(struct ngbe_hw *hw);
s32 ngbe_read_phy_reg_mdi(struct ngbe_hw *hw, u32 reg_addr, u32 device_type,
			   u16 *phy_data);
s32 ngbe_write_phy_reg_mdi(struct ngbe_hw *hw, u32 reg_addr, u32 device_type,
			    u16 phy_data);
s32 ngbe_read_phy_reg(struct ngbe_hw *hw, u32 reg_addr,
			       u32 device_type, u16 *phy_data);
s32 ngbe_write_phy_reg(struct ngbe_hw *hw, u32 reg_addr,
				u32 device_type, u16 phy_data);
s32 ngbe_check_reset_blocked(struct ngbe_hw *hw);

#endif /* _NGBE_PHY_H_ */
