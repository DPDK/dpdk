/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_common.h>

#include "mlx5_defs.h"
#include "mlx5.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"

/**
 * Remove a MAC address from the internal array.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param index
 *   MAC address index.
 */
static void
mlx5_internal_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	MLX5_ASSERT(index < MLX5_MAX_MAC_ADDRESSES);
	if (rte_is_zero_ether_addr(&dev->data->mac_addrs[index]))
		return;
	mlx5_os_mac_addr_remove(dev, index);
	memset(&dev->data->mac_addrs[index], 0, sizeof(struct rte_ether_addr));
}

/**
 * Adds a MAC address to the internal array.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mac_addr
 *   MAC address to register.
 * @param index
 *   MAC address index.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_internal_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *mac,
			   uint32_t index)
{
	unsigned int i;
	int ret;

	MLX5_ASSERT(index < MLX5_MAX_MAC_ADDRESSES);
	if (rte_is_zero_ether_addr(mac)) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	/* First, make sure this address isn't already configured. */
	for (i = 0; (i != MLX5_MAX_MAC_ADDRESSES); ++i) {
		/* Skip this index, it's going to be reconfigured. */
		if (i == index)
			continue;
		if (memcmp(&dev->data->mac_addrs[i], mac, sizeof(*mac)))
			continue;
		/* Address already configured elsewhere, return with error. */
		rte_errno = EADDRINUSE;
		return -rte_errno;
	}
	ret = mlx5_os_mac_addr_add(dev, mac, index);
	if (ret)
		return ret;

	dev->data->mac_addrs[index] = *mac;
	return 0;
}

/**
 * DPDK callback to remove a MAC address.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param index
 *   MAC address index.
 */
void
mlx5_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	int ret;

	if (index >= MLX5_MAX_UC_MAC_ADDRESSES)
		return;
	mlx5_internal_mac_addr_remove(dev, index);
	if (!dev->data->promiscuous) {
		ret = mlx5_traffic_restart(dev);
		if (ret)
			DRV_LOG(ERR, "port %u cannot restart traffic: %s",
				dev->data->port_id, strerror(rte_errno));
	}
}

/**
 * DPDK callback to add a MAC address.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mac_addr
 *   MAC address to register.
 * @param index
 *   MAC address index.
 * @param vmdq
 *   VMDq pool index to associate address with (ignored).
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *mac,
		  uint32_t index, uint32_t vmdq __rte_unused)
{
	int ret;

	if (index >= MLX5_MAX_UC_MAC_ADDRESSES) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	ret = mlx5_internal_mac_addr_add(dev, mac, index);
	if (ret < 0)
		return ret;
	if (!dev->data->promiscuous)
		return mlx5_traffic_restart(dev);
	return 0;
}

/**
 * DPDK callback to set primary MAC address.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mac_addr
 *   MAC address to register.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_mac_addr_set(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr)
{
	uint16_t port_id;
	struct mlx5_priv *priv = dev->data->dev_private;

	/*
	 * Configuring the VF instead of its representor,
	 * need to skip the special case of HPF on Bluefield.
	 */
	if (priv->representor && priv->representor_id >= 0) {
		DRV_LOG(DEBUG, "VF represented by port %u setting primary MAC address",
			dev->data->port_id);
		RTE_ETH_FOREACH_DEV_SIBLING(port_id, dev->data->port_id) {
			priv = rte_eth_devices[port_id].data->dev_private;
			if (priv->master == 1) {
				priv = dev->data->dev_private;
				return mlx5_os_vf_mac_addr_modify
				       (priv,
					mlx5_ifindex(&rte_eth_devices[port_id]),
					mac_addr, priv->representor_id);
			}
		}
		rte_errno = -ENOTSUP;
		return rte_errno;
	}

	DRV_LOG(DEBUG, "port %u setting primary MAC address",
		dev->data->port_id);
	return mlx5_mac_addr_add(dev, mac_addr, 0, 0);
}

/**
 * DPDK callback to set multicast addresses list.
 *
 * @see rte_eth_dev_set_mc_addr_list()
 */
int
mlx5_set_mc_addr_list(struct rte_eth_dev *dev,
		      struct rte_ether_addr *mc_addr_set, uint32_t nb_mc_addr)
{
	uint32_t i;
	int ret;

	if (nb_mc_addr >= MLX5_MAX_MC_MAC_ADDRESSES) {
		rte_errno = ENOSPC;
		return -rte_errno;
	}
	for (i = MLX5_MAX_UC_MAC_ADDRESSES; i != MLX5_MAX_MAC_ADDRESSES; ++i)
		mlx5_internal_mac_addr_remove(dev, i);
	i = MLX5_MAX_UC_MAC_ADDRESSES;
	while (nb_mc_addr--) {
		ret = mlx5_internal_mac_addr_add(dev, mc_addr_set++, i++);
		if (ret)
			return ret;
	}
	if (!dev->data->promiscuous)
		return mlx5_traffic_restart(dev);
	return 0;
}
