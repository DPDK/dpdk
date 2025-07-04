/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>

#include <rte_ether.h>
#include <ethdev_driver.h>
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
 * @param addr
 *   If MAC address is actually removed, it will be stored here if pointer is not a NULL.
 *
 * @return
 *   True if there was a MAC address under given index.
 */
static bool
mlx5_internal_mac_addr_remove(struct rte_eth_dev *dev,
			      uint32_t index,
			      struct rte_ether_addr *addr)
{
	MLX5_ASSERT(index < MLX5_MAX_MAC_ADDRESSES);
	if (rte_is_zero_ether_addr(&dev->data->mac_addrs[index]))
		return false;
	mlx5_os_mac_addr_remove(dev, index);
	if (addr != NULL)
		*addr = dev->data->mac_addrs[index];
	memset(&dev->data->mac_addrs[index], 0, sizeof(struct rte_ether_addr));
	return true;
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
	struct rte_ether_addr addr = { 0 };
	int ret;

	if (index >= MLX5_MAX_UC_MAC_ADDRESSES)
		return;
	if (mlx5_internal_mac_addr_remove(dev, index, &addr)) {
		ret = mlx5_traffic_mac_remove(dev, &addr);
		if (ret)
			DRV_LOG(ERR, "port %u cannot update control flow rules: %s",
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
	return mlx5_traffic_mac_add(dev, mac);
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
	struct mlx5_priv *pf_priv;
	struct rte_ether_addr old_mac_addr = dev->data->mac_addrs[0];
	int ret;

	/* ethdev does not check if new default address is the same as the old one. */
	if (rte_is_same_ether_addr(mac_addr, &old_mac_addr))
		return 0;

	/*
	 * Configuring the VF instead of its representor,
	 * need to skip the special cases:
	 * - HPF on BlueField,
	 * - SF representors,
	 * - uplink ports when running in MPESW mode.
	 */
	if (priv->representor && !mlx5_is_hpf(dev) && !mlx5_is_sf_repr(dev) &&
	    !priv->mpesw_uplink) {
		DRV_LOG(DEBUG, "VF represented by port %u setting primary MAC address",
			dev->data->port_id);
		if (priv->pf_bond >= 0) {
			/* Bonding, get owner PF ifindex from shared data. */
			return mlx5_os_vf_mac_addr_modify
			       (priv,
				priv->sh->bond.ports[priv->pf_bond].ifindex,
				mac_addr,
				MLX5_REPRESENTOR_REPR(priv->representor_id));
		}
		RTE_ETH_FOREACH_DEV_SIBLING(port_id, dev->data->port_id) {
			pf_priv = rte_eth_devices[port_id].data->dev_private;
			if (pf_priv->master == 1)
				return mlx5_os_vf_mac_addr_modify
				       (priv, pf_priv->if_index, mac_addr,
					MLX5_REPRESENTOR_REPR
						(priv->representor_id));
		}
		rte_errno = -ENOTSUP;
		return rte_errno;
	}

	DRV_LOG(DEBUG, "port %u setting primary MAC address",
		dev->data->port_id);
	ret = mlx5_mac_addr_add(dev, mac_addr, 0, 0);
	if (ret)
		return ret;
	return mlx5_traffic_mac_remove(dev, &old_mac_addr);
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
		mlx5_internal_mac_addr_remove(dev, i, NULL);
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
