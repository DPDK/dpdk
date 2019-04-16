/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#include "opae_osdep.h"
#include "opae_eth_group.h"

#define DATA_VAL_INVL		1 /* us */
#define DATA_VAL_POLL_TIMEOUT	10 /* us */

static const char *eth_type_to_string(u8 type)
{
	switch (type) {
	case ETH_GROUP_PHY:
		return "phy";
	case ETH_GROUP_MAC:
		return "mac";
	case ETH_GROUP_ETHER:
		return "ethernet wrapper";
	}

	return "unknown";
}

static int eth_group_get_select(struct eth_group_device *dev,
		u8 type, u8 index, u8 *select)
{
	/*
	 * in different speed configuration, the index of
	 * PHY and MAC are different.
	 *
	 * 1 ethernet wrapper -> Device Select 0x0 - fixed value
	 * n PHYs             -> Device Select 0x2,4,6,8,A,C,E,10,...
	 * n MACs             -> Device Select 0x3,5,7,9,B,D,F,11,...
	 */

	if (type == ETH_GROUP_PHY && index < dev->phy_num)
		*select = index * 2 + 2;
	else if (type == ETH_GROUP_MAC && index < dev->mac_num)
		*select = index * 2 + 3;
	else if (type == ETH_GROUP_ETHER && index == 0)
		*select = 0;
	else
		return -EINVAL;

	return 0;
}

int eth_group_write_reg(struct eth_group_device *dev,
		u8 type, u8 index, u16 addr, u32 data)
{
	u8 dev_select = 0;
	u64 v = 0;
	int ret;

	dev_debug(dev, "%s type %s index %u addr 0x%x\n",
			__func__, eth_type_to_string(type), index, addr);

	/* find device select */
	ret = eth_group_get_select(dev, type, index, &dev_select);
	if (ret)
		return ret;

	v = CMD_WR << CTRL_CMD_SHIT |
		(u64)dev_select << CTRL_DS_SHIFT |
		(u64)addr << CTRL_ADDR_SHIFT |
		(data & CTRL_WR_DATA);

	/* only PHY has additional feature bit */
	if (type == ETH_GROUP_PHY)
		v |= CTRL_FEAT_SELECT;

	opae_writeq(v, dev->base + ETH_GROUP_CTRL);

	return 0;
}

int eth_group_read_reg(struct eth_group_device *dev,
		u8 type, u8 index, u16 addr, u32 *data)
{
	u8 dev_select = 0;
	u64 v = 0;
	int ret;

	dev_debug(dev, "%s type %s index %u addr 0x%x\n",
			__func__, eth_type_to_string(type), index,
			addr);

	/* find device select */
	ret = eth_group_get_select(dev, type, index, &dev_select);
	if (ret)
		return ret;

	v = CMD_RD << CTRL_CMD_SHIT |
		(u64)dev_select << CTRL_DS_SHIFT |
		(u64)addr << CTRL_ADDR_SHIFT;

	/* only PHY has additional feature bit */
	if (type == ETH_GROUP_PHY)
		v |= CTRL_FEAT_SELECT;

	opae_writeq(v, dev->base + ETH_GROUP_CTRL);

	if (opae_readq_poll_timeout(dev->base + ETH_GROUP_STAT,
			v, v & STAT_DATA_VAL, DATA_VAL_INVL,
			DATA_VAL_POLL_TIMEOUT))
		return -ETIMEDOUT;

	*data = (v & STAT_RD_DATA);

	dev_debug(dev, "%s data 0x%x\n", __func__, *data);

	return 0;
}

struct eth_group_device *eth_group_probe(void *base)
{
	struct eth_group_device *dev;

	dev = opae_malloc(sizeof(*dev));
	if (!dev)
		return NULL;

	dev->base = (u8 *)base;

	dev->info.info = opae_readq(dev->base + ETH_GROUP_INFO);
	dev->group_id = dev->info.group_id;
	dev->phy_num = dev->mac_num = dev->info.num_phys;
	dev->speed = dev->info.speed;

	dev->status = ETH_GROUP_DEV_ATTACHED;

	dev_info(dev, "eth group device %d probe done: phy_num=mac_num:%d, speed=%d\n",
			dev->group_id, dev->phy_num, dev->speed);

	return dev;
}

void eth_group_release(struct eth_group_device *dev)
{
	if (dev) {
		dev->status = ETH_GROUP_DEV_NOUSED;
		opae_free(dev);
	}
}
