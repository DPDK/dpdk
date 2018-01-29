/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox.
 */

#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_common.h>

#include "mlx5.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_defs.h"

/**
 * Get MAC address by querying netdevice.
 *
 * @param[in] priv
 *   struct priv for the requested device.
 * @param[out] mac
 *   MAC address output buffer.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
int
priv_get_mac(struct priv *priv, uint8_t (*mac)[ETHER_ADDR_LEN])
{
	struct ifreq request;

	if (priv_ifreq(priv, SIOCGIFHWADDR, &request))
		return -1;
	memcpy(mac, request.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);
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
	assert(index < MLX5_MAX_MAC_ADDRESSES);
	memset(&dev->data->mac_addrs[index], 0, sizeof(struct ether_addr));
	if (!dev->data->promiscuous)
		mlx5_traffic_restart(dev);
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
 *   0 on success.
 */
int
mlx5_mac_addr_add(struct rte_eth_dev *dev, struct ether_addr *mac,
		  uint32_t index, uint32_t vmdq)
{
	unsigned int i;
	int ret = 0;

	(void)vmdq;
	assert(index < MLX5_MAX_MAC_ADDRESSES);
	/* First, make sure this address isn't already configured. */
	for (i = 0; (i != MLX5_MAX_MAC_ADDRESSES); ++i) {
		/* Skip this index, it's going to be reconfigured. */
		if (i == index)
			continue;
		if (memcmp(&dev->data->mac_addrs[i], mac, sizeof(*mac)))
			continue;
		/* Address already configured elsewhere, return with error. */
		return EADDRINUSE;
	}
	dev->data->mac_addrs[index] = *mac;
	if (!dev->data->promiscuous)
		mlx5_traffic_restart(dev);
	return ret;
}

/**
 * DPDK callback to set primary MAC address.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mac_addr
 *   MAC address to register.
 */
void
mlx5_mac_addr_set(struct rte_eth_dev *dev, struct ether_addr *mac_addr)
{
	DEBUG("%p: setting primary MAC address", (void *)dev);
	mlx5_mac_addr_add(dev, mac_addr, 0, 0);
}
