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

static int eth_group_reset_mac(struct eth_group_device *dev, u8 index,
			       bool enable)
{
	u32 val;
	int ret;

	/*
	 * only support 25G & 40G mac reset for now. It uses internal reset.
	 * as PHY and MAC are integrated together, below action will trigger
	 * PHY reset too.
	 */
	if (dev->speed != 25 && dev->speed != 40)
		return 0;

	ret = eth_group_read_reg(dev, ETH_GROUP_MAC, index, MAC_CONFIG,
				 &val);
	if (ret) {
		dev_err(dev, "fail to read PHY_CONFIG: %d\n", ret);
		return ret;
	}

	/* skip if mac is in expected state already */
	if ((((val & MAC_RESET_MASK) == MAC_RESET_MASK) && enable) ||
	    (((val & MAC_RESET_MASK) == 0) && !enable))
		return 0;

	if (enable)
		val |= MAC_RESET_MASK;
	else
		val &= ~MAC_RESET_MASK;

	ret = eth_group_write_reg(dev, ETH_GROUP_MAC, index, MAC_CONFIG,
				  val);
	if (ret)
		dev_err(dev, "fail to write PHY_CONFIG: %d\n", ret);

	return ret;
}

static void eth_group_mac_uinit(struct eth_group_device *dev)
{
	u8 i;

	for (i = 0; i < dev->mac_num; i++) {
		if (eth_group_reset_mac(dev, i, true))
			dev_err(dev, "fail to disable mac %d\n", i);
	}
}

static int eth_group_mac_init(struct eth_group_device *dev)
{
	int ret;
	u8 i;

	for (i = 0; i < dev->mac_num; i++) {
		ret = eth_group_reset_mac(dev, i, false);
		if (ret) {
			dev_err(dev, "fail to enable mac %d\n", i);
			goto exit;
		}
	}

	return 0;

exit:
	while (i--)
		eth_group_reset_mac(dev, i, true);

	return ret;
}

static int eth_group_reset_phy(struct eth_group_device *dev, u8 index,
		bool enable)
{
	u32 val;
	int ret;

	/* only support 10G PHY reset for now. It uses external reset. */
	if (dev->speed != 10)
		return 0;

	ret = eth_group_read_reg(dev, ETH_GROUP_PHY, index,
			ADD_PHY_CTRL, &val);
	if (ret) {
		dev_err(dev, "fail to read ADD_PHY_CTRL reg: %d\n", ret);
		return ret;
	}

	/* return if PHY is already in expected state */
	if ((val & PHY_RESET && enable) || (!(val & PHY_RESET) && !enable))
		return 0;

	if (enable)
		val |= PHY_RESET;
	else
		val &= ~PHY_RESET;

	ret = eth_group_write_reg(dev, ETH_GROUP_PHY, index,
			ADD_PHY_CTRL, val);
	if (ret)
		dev_err(dev, "fail to write ADD_PHY_CTRL reg: %d\n", ret);

	return ret;
}

static int eth_group_phy_init(struct eth_group_device *dev)
{
	int ret;
	int i;

	for (i = 0; i < dev->phy_num; i++) {
		ret = eth_group_reset_phy(dev, i, false);
		if (ret) {
			dev_err(dev, "fail to enable phy %d\n", i);
			goto exit;
		}
	}

	return 0;
exit:
	while (i--)
		eth_group_reset_phy(dev, i, true);

	return ret;
}

static void eth_group_phy_uinit(struct eth_group_device *dev)
{
	int i;

	for (i = 0; i < dev->phy_num; i++) {
		if (eth_group_reset_phy(dev, i, true))
			dev_err(dev, "fail to disable phy %d\n", i);
	}
}

static int eth_group_hw_init(struct eth_group_device *dev)
{
	int ret;

	ret = eth_group_phy_init(dev);
	if (ret) {
		dev_err(dev, "fail to init eth group phys\n");
		return ret;
	}

	ret = eth_group_mac_init(dev);
	if (ret) {
		dev_err(priv->dev, "fail to init eth group macs\n");
		goto phy_exit;
	}

	return 0;

phy_exit:
	eth_group_phy_uinit(dev);
	return ret;
}

static void eth_group_hw_uinit(struct eth_group_device *dev)
{
	eth_group_mac_uinit(dev);
	eth_group_phy_uinit(dev);
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

	if (eth_group_hw_init(dev)) {
		dev_err(dev, "eth group hw init fail\n");
		return NULL;
	}

	dev_info(dev, "eth group device %d probe done: phy_num=mac_num:%d, speed=%d\n",
			dev->group_id, dev->phy_num, dev->speed);

	return dev;
}

void eth_group_release(struct eth_group_device *dev)
{
	if (dev) {
		eth_group_hw_uinit(dev);
		dev->status = ETH_GROUP_DEV_NOUSED;
		opae_free(dev);
	}
}
