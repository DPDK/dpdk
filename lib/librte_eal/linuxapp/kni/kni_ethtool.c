/*-
 * GPL LICENSE SUMMARY
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of version 2 of the GNU General Public License as
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *   The full GNU General Public License is included in this distribution
 *   in the file called LICENSE.GPL.
 *
 *   Contact Information:
 *   Intel Corporation
 */

#include <linux/device.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include "kni_dev.h"

static int
kni_check_if_running(struct net_device *dev)
{
	struct kni_dev *priv = netdev_priv(dev);

	if (priv->lad_dev)
		return 0;
	else
		return -EOPNOTSUPP;
}

static void
kni_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	struct kni_dev *priv = netdev_priv(dev);

	priv->lad_dev->ethtool_ops->get_drvinfo(priv->lad_dev, info);
}

static int
kni_get_settings(struct net_device *dev, struct ethtool_cmd *ecmd)
{
	struct kni_dev *priv = netdev_priv(dev);

	return priv->lad_dev->ethtool_ops->get_settings(priv->lad_dev, ecmd);
}

static int
kni_set_settings(struct net_device *dev, struct ethtool_cmd *ecmd)
{
	struct kni_dev *priv = netdev_priv(dev);

	return priv->lad_dev->ethtool_ops->set_settings(priv->lad_dev, ecmd);
}

static void
kni_get_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct kni_dev *priv = netdev_priv(dev);

	priv->lad_dev->ethtool_ops->get_wol(priv->lad_dev, wol);
}

static int
kni_set_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct kni_dev *priv = netdev_priv(dev);

	return priv->lad_dev->ethtool_ops->set_wol(priv->lad_dev, wol);
}

static int
kni_nway_reset(struct net_device *dev)
{
	struct kni_dev *priv = netdev_priv(dev);

	return priv->lad_dev->ethtool_ops->nway_reset(priv->lad_dev);
}

static int
kni_get_eeprom_len(struct net_device *dev)
{
	struct kni_dev *priv = netdev_priv(dev);

	return priv->lad_dev->ethtool_ops->get_eeprom_len(priv->lad_dev);
}

static int
kni_get_eeprom(struct net_device *dev, struct ethtool_eeprom *eeprom,
							u8 *bytes)
{
	struct kni_dev *priv = netdev_priv(dev);

	return priv->lad_dev->ethtool_ops->get_eeprom(priv->lad_dev, eeprom,
								bytes);
}

static int
kni_set_eeprom(struct net_device *dev, struct ethtool_eeprom *eeprom,
							u8 *bytes)
{
	struct kni_dev *priv = netdev_priv(dev);

	return priv->lad_dev->ethtool_ops->set_eeprom(priv->lad_dev, eeprom,
								bytes);
}

static void
kni_get_ringparam(struct net_device *dev, struct ethtool_ringparam *ring)
{
	struct kni_dev *priv = netdev_priv(dev);

	priv->lad_dev->ethtool_ops->get_ringparam(priv->lad_dev, ring);
}

static int
kni_set_ringparam(struct net_device *dev, struct ethtool_ringparam *ring)
{
	struct kni_dev *priv = netdev_priv(dev);

	return priv->lad_dev->ethtool_ops->set_ringparam(priv->lad_dev, ring);
}

static void
kni_get_pauseparam(struct net_device *dev, struct ethtool_pauseparam *pause)
{
	struct kni_dev *priv = netdev_priv(dev);

	priv->lad_dev->ethtool_ops->get_pauseparam(priv->lad_dev, pause);
}

static int
kni_set_pauseparam(struct net_device *dev, struct ethtool_pauseparam *pause)
{
	struct kni_dev *priv = netdev_priv(dev);

	return priv->lad_dev->ethtool_ops->set_pauseparam(priv->lad_dev,
								pause);
}

static u32
kni_get_msglevel(struct net_device *dev)
{
	struct kni_dev *priv = netdev_priv(dev);

	return priv->lad_dev->ethtool_ops->get_msglevel(priv->lad_dev);
}

static void
kni_set_msglevel(struct net_device *dev, u32 data)
{
	struct kni_dev *priv = netdev_priv(dev);

	priv->lad_dev->ethtool_ops->set_msglevel(priv->lad_dev, data);
}

static int
kni_get_regs_len(struct net_device *dev)
{
	struct kni_dev *priv = netdev_priv(dev);

	return priv->lad_dev->ethtool_ops->get_regs_len(priv->lad_dev);
}

static void
kni_get_regs(struct net_device *dev, struct ethtool_regs *regs, void *p)
{
	struct kni_dev *priv = netdev_priv(dev);

	priv->lad_dev->ethtool_ops->get_regs(priv->lad_dev, regs, p);
}

static void
kni_get_strings(struct net_device *dev, u32 stringset, u8 *data)
{
	struct kni_dev *priv = netdev_priv(dev);

	priv->lad_dev->ethtool_ops->get_strings(priv->lad_dev, stringset,
								data);
}

static int
kni_get_sset_count(struct net_device *dev, int sset)
{
	struct kni_dev *priv = netdev_priv(dev);

	return priv->lad_dev->ethtool_ops->get_sset_count(priv->lad_dev, sset);
}

static void
kni_get_ethtool_stats(struct net_device *dev, struct ethtool_stats *stats,
								u64 *data)
{
	struct kni_dev *priv = netdev_priv(dev);

	priv->lad_dev->ethtool_ops->get_ethtool_stats(priv->lad_dev, stats,
								data);
}

struct ethtool_ops kni_ethtool_ops = {
	.begin			= kni_check_if_running,
	.get_drvinfo		= kni_get_drvinfo,
	.get_settings		= kni_get_settings,
	.set_settings		= kni_set_settings,
	.get_regs_len		= kni_get_regs_len,
	.get_regs		= kni_get_regs,
	.get_wol		= kni_get_wol,
	.set_wol		= kni_set_wol,
	.nway_reset		= kni_nway_reset,
	.get_link		= ethtool_op_get_link,
	.get_eeprom_len		= kni_get_eeprom_len,
	.get_eeprom		= kni_get_eeprom,
	.set_eeprom		= kni_set_eeprom,
	.get_ringparam		= kni_get_ringparam,
	.set_ringparam		= kni_set_ringparam,
	.get_pauseparam		= kni_get_pauseparam,
	.set_pauseparam		= kni_set_pauseparam,
	.get_msglevel		= kni_get_msglevel,
	.set_msglevel		= kni_set_msglevel,
	.get_strings		= kni_get_strings,
	.get_sset_count		= kni_get_sset_count,
	.get_ethtool_stats	= kni_get_ethtool_stats,
};

void
kni_set_ethtool_ops(struct net_device *netdev)
{
	netdev->ethtool_ops = &kni_ethtool_ops;
}
