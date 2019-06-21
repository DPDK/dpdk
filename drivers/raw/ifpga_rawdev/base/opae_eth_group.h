/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#ifndef _OPAE_PHY_MAC_H
#define _OPAE_PHY_MAC_H

#include "opae_osdep.h"

#define MAX_ETH_GROUP_DEVICES 2

#define LINE_SIDE_GROUP_ID 0
#define HOST_SIDE_GROUP_ID 1

#define ETH_GROUP_SELECT_FEAT 1

#define ETH_GROUP_PHY 1
#define ETH_GROUP_MAC 2
#define ETH_GROUP_ETHER 3

#define ETH_GROUP_INFO		0x8
#define INFO_SPEED		GENMASK_ULL(23, 16)
#define ETH_SPEED_10G		10
#define ETH_SPEED_25G		25
#define INFO_PHY_NUM		GENMASK_ULL(15, 8)
#define INFO_GROUP_NUM		GENMASK_ULL(7, 0)

#define ETH_GROUP_CTRL		0x10
#define CTRL_CMD		GENMASK_ULL(63, 62)
#define CTRL_CMD_SHIT           62
#define CMD_NOP			0ULL
#define CMD_RD			1ULL
#define CMD_WR			2ULL
#define CTRL_DEV_SELECT		GENMASK_ULL(53, 49)
#define CTRL_DS_SHIFT   49
#define CTRL_FEAT_SELECT	BIT_ULL(48)
#define SELECT_IP		0
#define SELECT_FEAT		1
#define CTRL_ADDR		GENMASK_ULL(47, 32)
#define CTRL_ADDR_SHIFT         32
#define CTRL_WR_DATA		GENMASK_ULL(31, 0)

#define ETH_GROUP_STAT		0x18
#define STAT_DATA_VAL		BIT_ULL(32)
#define STAT_RD_DATA		GENMASK_ULL(31, 0)

/* Additional Feature Register */
#define ADD_PHY_CTRL            0x0
#define PHY_RESET               BIT(0)
#define MAC_CONFIG      0x310
#define MAC_RESET_MASK  GENMASK(2, 0)

struct opae_eth_group_info {
	u8 group_id;
	u8 speed;
	u8 nums_of_phy;
	u8 nums_of_mac;
};

struct opae_eth_group_region_info {
	u8 group_id;
	u64 phys_addr;
	u64 len;
	u8 *addr;
	u8 mem_idx;
};

struct eth_group_info_reg {
	union {
		u64 info;
		struct {
			u8 group_id:8;
			u8 num_phys:8;
			u8 speed:8;
			u8 direction:1;
			u64 resvd:39;
		};
	};
};

enum eth_group_status {
	ETH_GROUP_DEV_NOUSED = 0,
	ETH_GROUP_DEV_ATTACHED,
};

struct eth_group_device {
	u8 *base;
	struct eth_group_info_reg info;
	enum eth_group_status status;
	u8 speed;
	u8 group_id;
	u8 phy_num;
	u8 mac_num;
};

struct eth_group_device *eth_group_probe(void *base);
void eth_group_release(struct eth_group_device *dev);
int eth_group_read_reg(struct eth_group_device *dev,
		u8 type, u8 index, u16 addr, u32 *data);
int eth_group_write_reg(struct eth_group_device *dev,
		u8 type, u8 index, u16 addr, u32 data);
#endif
