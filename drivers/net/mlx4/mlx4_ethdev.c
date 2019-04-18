/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

/**
 * @file
 * Miscellaneous control operations for mlx4 driver.
 */

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

/* Verbs headers do not support -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_bus_pci.h>
#include <rte_errno.h>
#include <rte_ethdev_driver.h>
#include <rte_ether.h>
#include <rte_flow.h>
#include <rte_pci.h>
#include <rte_string_fns.h>

#include "mlx4.h"
#include "mlx4_flow.h"
#include "mlx4_glue.h"
#include "mlx4_rxtx.h"
#include "mlx4_utils.h"

/**
 * Get interface name from private structure.
 *
 * @param[in] priv
 *   Pointer to private structure.
 * @param[out] ifname
 *   Interface name output buffer.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
mlx4_get_ifname(const struct mlx4_priv *priv, char (*ifname)[IF_NAMESIZE])
{
	DIR *dir;
	struct dirent *dent;
	unsigned int dev_type = 0;
	unsigned int dev_port_prev = ~0u;
	char match[IF_NAMESIZE] = "";

	{
		MKSTR(path, "%s/device/net", priv->ctx->device->ibdev_path);

		dir = opendir(path);
		if (dir == NULL) {
			rte_errno = errno;
			return -rte_errno;
		}
	}
	while ((dent = readdir(dir)) != NULL) {
		char *name = dent->d_name;
		FILE *file;
		unsigned int dev_port;
		int r;

		if ((name[0] == '.') &&
		    ((name[1] == '\0') ||
		     ((name[1] == '.') && (name[2] == '\0'))))
			continue;

		MKSTR(path, "%s/device/net/%s/%s",
		      priv->ctx->device->ibdev_path, name,
		      (dev_type ? "dev_id" : "dev_port"));

		file = fopen(path, "rb");
		if (file == NULL) {
			if (errno != ENOENT)
				continue;
			/*
			 * Switch to dev_id when dev_port does not exist as
			 * is the case with Linux kernel versions < 3.15.
			 */
try_dev_id:
			match[0] = '\0';
			if (dev_type)
				break;
			dev_type = 1;
			dev_port_prev = ~0u;
			rewinddir(dir);
			continue;
		}
		r = fscanf(file, (dev_type ? "%x" : "%u"), &dev_port);
		fclose(file);
		if (r != 1)
			continue;
		/*
		 * Switch to dev_id when dev_port returns the same value for
		 * all ports. May happen when using a MOFED release older than
		 * 3.0 with a Linux kernel >= 3.15.
		 */
		if (dev_port == dev_port_prev)
			goto try_dev_id;
		dev_port_prev = dev_port;
		if (dev_port == (priv->port - 1u))
			strlcpy(match, name, sizeof(match));
	}
	closedir(dir);
	if (match[0] == '\0') {
		rte_errno = ENODEV;
		return -rte_errno;
	}
	strncpy(*ifname, match, sizeof(*ifname));
	return 0;
}

/**
 * Perform ifreq ioctl() on associated Ethernet device.
 *
 * @param[in] priv
 *   Pointer to private structure.
 * @param req
 *   Request number to pass to ioctl().
 * @param[out] ifr
 *   Interface request structure output buffer.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_ifreq(const struct mlx4_priv *priv, int req, struct ifreq *ifr)
{
	int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	int ret;

	if (sock == -1) {
		rte_errno = errno;
		return -rte_errno;
	}
	ret = mlx4_get_ifname(priv, &ifr->ifr_name);
	if (!ret && ioctl(sock, req, ifr) == -1) {
		rte_errno = errno;
		ret = -rte_errno;
	}
	close(sock);
	return ret;
}

/**
 * Get MAC address by querying netdevice.
 *
 * @param[in] priv
 *   Pointer to private structure.
 * @param[out] mac
 *   MAC address output buffer.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
mlx4_get_mac(struct mlx4_priv *priv, uint8_t (*mac)[ETHER_ADDR_LEN])
{
	struct ifreq request;
	int ret = mlx4_ifreq(priv, SIOCGIFHWADDR, &request);

	if (ret)
		return ret;
	memcpy(mac, request.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);
	return 0;
}

/**
 * Get device MTU.
 *
 * @param priv
 *   Pointer to private structure.
 * @param[out] mtu
 *   MTU value output buffer.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
mlx4_mtu_get(struct mlx4_priv *priv, uint16_t *mtu)
{
	struct ifreq request;
	int ret = mlx4_ifreq(priv, SIOCGIFMTU, &request);

	if (ret)
		return ret;
	*mtu = request.ifr_mtu;
	return 0;
}

/**
 * DPDK callback to change the MTU.
 *
 * @param priv
 *   Pointer to Ethernet device structure.
 * @param mtu
 *   MTU value to set.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
mlx4_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	struct ifreq request = { .ifr_mtu = mtu, };
	int ret = mlx4_ifreq(priv, SIOCSIFMTU, &request);

	if (ret)
		return ret;
	priv->mtu = mtu;
	return 0;
}

/**
 * Set device flags.
 *
 * @param priv
 *   Pointer to private structure.
 * @param keep
 *   Bitmask for flags that must remain untouched.
 * @param flags
 *   Bitmask for flags to modify.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_set_flags(struct mlx4_priv *priv, unsigned int keep, unsigned int flags)
{
	struct ifreq request;
	int ret = mlx4_ifreq(priv, SIOCGIFFLAGS, &request);

	if (ret)
		return ret;
	request.ifr_flags &= keep;
	request.ifr_flags |= flags & ~keep;
	return mlx4_ifreq(priv, SIOCSIFFLAGS, &request);
}

/**
 * Change the link state (UP / DOWN).
 *
 * @param priv
 *   Pointer to Ethernet device private data.
 * @param up
 *   Nonzero for link up, otherwise link down.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_dev_set_link(struct mlx4_priv *priv, int up)
{
	int err;

	if (up) {
		err = mlx4_set_flags(priv, ~IFF_UP, IFF_UP);
		if (err)
			return err;
	} else {
		err = mlx4_set_flags(priv, ~IFF_UP, ~IFF_UP);
		if (err)
			return err;
	}
	return 0;
}

/**
 * DPDK callback to bring the link DOWN.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
mlx4_dev_set_link_down(struct rte_eth_dev *dev)
{
	struct mlx4_priv *priv = dev->data->dev_private;

	return mlx4_dev_set_link(priv, 0);
}

/**
 * DPDK callback to bring the link UP.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
mlx4_dev_set_link_up(struct rte_eth_dev *dev)
{
	struct mlx4_priv *priv = dev->data->dev_private;

	return mlx4_dev_set_link(priv, 1);
}

/**
 * Supported Rx mode toggles.
 *
 * Even and odd values respectively stand for off and on.
 */
enum rxmode_toggle {
	RXMODE_TOGGLE_PROMISC_OFF,
	RXMODE_TOGGLE_PROMISC_ON,
	RXMODE_TOGGLE_ALLMULTI_OFF,
	RXMODE_TOGGLE_ALLMULTI_ON,
};

/**
 * Helper function to toggle promiscuous and all multicast modes.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param toggle
 *   Toggle to set.
 */
static void
mlx4_rxmode_toggle(struct rte_eth_dev *dev, enum rxmode_toggle toggle)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	const char *mode;
	struct rte_flow_error error;

	switch (toggle) {
	case RXMODE_TOGGLE_PROMISC_OFF:
	case RXMODE_TOGGLE_PROMISC_ON:
		mode = "promiscuous";
		dev->data->promiscuous = toggle & 1;
		break;
	case RXMODE_TOGGLE_ALLMULTI_OFF:
	case RXMODE_TOGGLE_ALLMULTI_ON:
		mode = "all multicast";
		dev->data->all_multicast = toggle & 1;
		break;
	default:
		mode = "undefined";
	}
	if (!mlx4_flow_sync(priv, &error))
		return;
	ERROR("cannot toggle %s mode (code %d, \"%s\"),"
	      " flow error type %d, cause %p, message: %s",
	      mode, rte_errno, strerror(rte_errno), error.type, error.cause,
	      error.message ? error.message : "(unspecified)");
}

/**
 * DPDK callback to enable promiscuous mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx4_promiscuous_enable(struct rte_eth_dev *dev)
{
	mlx4_rxmode_toggle(dev, RXMODE_TOGGLE_PROMISC_ON);
}

/**
 * DPDK callback to disable promiscuous mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx4_promiscuous_disable(struct rte_eth_dev *dev)
{
	mlx4_rxmode_toggle(dev, RXMODE_TOGGLE_PROMISC_OFF);
}

/**
 * DPDK callback to enable all multicast mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx4_allmulticast_enable(struct rte_eth_dev *dev)
{
	mlx4_rxmode_toggle(dev, RXMODE_TOGGLE_ALLMULTI_ON);
}

/**
 * DPDK callback to disable all multicast mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx4_allmulticast_disable(struct rte_eth_dev *dev)
{
	mlx4_rxmode_toggle(dev, RXMODE_TOGGLE_ALLMULTI_OFF);
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
mlx4_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	struct rte_flow_error error;

	if (index >= RTE_DIM(priv->mac) - priv->mac_mc) {
		rte_errno = EINVAL;
		return;
	}
	memset(&priv->mac[index], 0, sizeof(priv->mac[index]));
	if (!mlx4_flow_sync(priv, &error))
		return;
	ERROR("failed to synchronize flow rules after removing MAC address"
	      " at index %d (code %d, \"%s\"),"
	      " flow error type %d, cause %p, message: %s",
	      index, rte_errno, strerror(rte_errno), error.type, error.cause,
	      error.message ? error.message : "(unspecified)");
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
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
mlx4_mac_addr_add(struct rte_eth_dev *dev, struct ether_addr *mac_addr,
		  uint32_t index, uint32_t vmdq)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	struct rte_flow_error error;
	int ret;

	(void)vmdq;
	if (index >= RTE_DIM(priv->mac) - priv->mac_mc) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	memcpy(&priv->mac[index], mac_addr, sizeof(priv->mac[index]));
	ret = mlx4_flow_sync(priv, &error);
	if (!ret)
		return 0;
	ERROR("failed to synchronize flow rules after adding MAC address"
	      " at index %d (code %d, \"%s\"),"
	      " flow error type %d, cause %p, message: %s",
	      index, rte_errno, strerror(rte_errno), error.type, error.cause,
	      error.message ? error.message : "(unspecified)");
	return ret;
}

/**
 * DPDK callback to configure multicast addresses.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param list
 *   List of MAC addresses to register.
 * @param num
 *   Number of entries in list.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
mlx4_set_mc_addr_list(struct rte_eth_dev *dev, struct ether_addr *list,
		      uint32_t num)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	struct rte_flow_error error;
	int ret;

	if (num > RTE_DIM(priv->mac)) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	/*
	 * Make sure there is enough room to increase the number of
	 * multicast entries without overwriting standard entries.
	 */
	if (num > priv->mac_mc) {
		unsigned int i;

		for (i = RTE_DIM(priv->mac) - num;
		     i != RTE_DIM(priv->mac) - priv->mac_mc;
		     ++i)
			if (!is_zero_ether_addr(&priv->mac[i])) {
				rte_errno = EBUSY;
				return -rte_errno;
			}
	} else if (num < priv->mac_mc) {
		/* Clear unused entries. */
		memset(priv->mac + RTE_DIM(priv->mac) - priv->mac_mc,
		       0,
		       sizeof(priv->mac[0]) * (priv->mac_mc - num));
	}
	memcpy(priv->mac + RTE_DIM(priv->mac) - num, list, sizeof(*list) * num);
	priv->mac_mc = num;
	ret = mlx4_flow_sync(priv, &error);
	if (!ret)
		return 0;
	ERROR("failed to synchronize flow rules after modifying MC list,"
	      " (code %d, \"%s\"), flow error type %d, cause %p, message: %s",
	      rte_errno, strerror(rte_errno), error.type, error.cause,
	      error.message ? error.message : "(unspecified)");
	return ret;
}

/**
 * DPDK callback to configure a VLAN filter.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param vlan_id
 *   VLAN ID to filter.
 * @param on
 *   Toggle filter.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
mlx4_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	struct rte_flow_error error;
	unsigned int vidx = vlan_id / 64;
	unsigned int vbit = vlan_id % 64;
	uint64_t *v;
	int ret;

	if (vidx >= RTE_DIM(dev->data->vlan_filter_conf.ids)) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	v = &dev->data->vlan_filter_conf.ids[vidx];
	*v &= ~(UINT64_C(1) << vbit);
	*v |= (uint64_t)!!on << vbit;
	ret = mlx4_flow_sync(priv, &error);
	if (!ret)
		return 0;
	ERROR("failed to synchronize flow rules after %s VLAN filter on ID %u"
	      " (code %d, \"%s\"), "
	      " flow error type %d, cause %p, message: %s",
	      on ? "enabling" : "disabling", vlan_id,
	      rte_errno, strerror(rte_errno), error.type, error.cause,
	      error.message ? error.message : "(unspecified)");
	return ret;
}

/**
 * DPDK callback to set the primary MAC address.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mac_addr
 *   MAC address to register.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
mlx4_mac_addr_set(struct rte_eth_dev *dev, struct ether_addr *mac_addr)
{
	return mlx4_mac_addr_add(dev, mac_addr, 0, 0);
}

/**
 * DPDK callback to get information about the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] info
 *   Info structure output buffer.
 */
void
mlx4_dev_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *info)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	unsigned int max;
	char ifname[IF_NAMESIZE];

	/* FIXME: we should ask the device for these values. */
	info->min_rx_bufsize = 32;
	info->max_rx_pktlen = 65536;
	/*
	 * Since we need one CQ per QP, the limit is the minimum number
	 * between the two values.
	 */
	max = ((priv->device_attr.max_cq > priv->device_attr.max_qp) ?
	       priv->device_attr.max_qp : priv->device_attr.max_cq);
	/* If max >= 65535 then max = 0, max_rx_queues is uint16_t. */
	if (max >= 65535)
		max = 65535;
	info->max_rx_queues = max;
	info->max_tx_queues = max;
	info->max_mac_addrs = RTE_DIM(priv->mac);
	info->tx_offload_capa = mlx4_get_tx_port_offloads(priv);
	info->rx_queue_offload_capa = mlx4_get_rx_queue_offloads(priv);
	info->rx_offload_capa = (mlx4_get_rx_port_offloads(priv) |
				 info->rx_queue_offload_capa);
	if (mlx4_get_ifname(priv, &ifname) == 0)
		info->if_index = if_nametoindex(ifname);
	info->hash_key_size = MLX4_RSS_HASH_KEY_SIZE;
	info->speed_capa =
			ETH_LINK_SPEED_1G |
			ETH_LINK_SPEED_10G |
			ETH_LINK_SPEED_20G |
			ETH_LINK_SPEED_40G |
			ETH_LINK_SPEED_56G;
	info->flow_type_rss_offloads = mlx4_conv_rss_types(priv, 0, 1);
}

/**
 * Get firmware version of a device.
 *
 * @param dev
 *   Ethernet device port.
 * @param fw_ver
 *   String output allocated by caller.
 * @param fw_size
 *   Size of the output string, including terminating null byte.
 *
 * @return
 *   0 on success, or the size of the non truncated string if too big.
 */
int mlx4_fw_version_get(struct rte_eth_dev *dev, char *fw_ver, size_t fw_size)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	struct ibv_device_attr *attr = &priv->device_attr;
	size_t size = strnlen(attr->fw_ver, sizeof(attr->fw_ver)) + 1;

	if (fw_size < size)
		return size;
	if (fw_ver != NULL)
		strlcpy(fw_ver, attr->fw_ver, fw_size);
	return 0;
}

/**
 * DPDK callback to get device statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] stats
 *   Stats structure output buffer.
 */
int
mlx4_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct rte_eth_stats tmp;
	unsigned int i;
	unsigned int idx;

	memset(&tmp, 0, sizeof(tmp));
	/* Add software counters. */
	for (i = 0; i != dev->data->nb_rx_queues; ++i) {
		struct rxq *rxq = dev->data->rx_queues[i];

		if (rxq == NULL)
			continue;
		idx = rxq->stats.idx;
		if (idx < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			tmp.q_ipackets[idx] += rxq->stats.ipackets;
			tmp.q_ibytes[idx] += rxq->stats.ibytes;
			tmp.q_errors[idx] += (rxq->stats.idropped +
					      rxq->stats.rx_nombuf);
		}
		tmp.ipackets += rxq->stats.ipackets;
		tmp.ibytes += rxq->stats.ibytes;
		tmp.ierrors += rxq->stats.idropped;
		tmp.rx_nombuf += rxq->stats.rx_nombuf;
	}
	for (i = 0; i != dev->data->nb_tx_queues; ++i) {
		struct txq *txq = dev->data->tx_queues[i];

		if (txq == NULL)
			continue;
		idx = txq->stats.idx;
		if (idx < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			tmp.q_opackets[idx] += txq->stats.opackets;
			tmp.q_obytes[idx] += txq->stats.obytes;
			tmp.q_errors[idx] += txq->stats.odropped;
		}
		tmp.opackets += txq->stats.opackets;
		tmp.obytes += txq->stats.obytes;
		tmp.oerrors += txq->stats.odropped;
	}
	*stats = tmp;
	return 0;
}

/**
 * DPDK callback to clear device statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx4_stats_reset(struct rte_eth_dev *dev)
{
	unsigned int i;

	for (i = 0; i != dev->data->nb_rx_queues; ++i) {
		struct rxq *rxq = dev->data->rx_queues[i];

		if (rxq)
			rxq->stats = (struct mlx4_rxq_stats){
				.idx = rxq->stats.idx,
			};
	}
	for (i = 0; i != dev->data->nb_tx_queues; ++i) {
		struct txq *txq = dev->data->tx_queues[i];

		if (txq)
			txq->stats = (struct mlx4_txq_stats){
				.idx = txq->stats.idx,
			};
	}
}

/**
 * DPDK callback to retrieve physical link information.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param wait_to_complete
 *   Wait for request completion (ignored).
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
mlx4_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	const struct mlx4_priv *priv = dev->data->dev_private;
	struct ethtool_cmd edata = {
		.cmd = ETHTOOL_GSET,
	};
	struct ifreq ifr;
	struct rte_eth_link dev_link;
	int link_speed = 0;

	if (priv == NULL) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	(void)wait_to_complete;
	if (mlx4_ifreq(priv, SIOCGIFFLAGS, &ifr)) {
		WARN("ioctl(SIOCGIFFLAGS) failed: %s", strerror(rte_errno));
		return -rte_errno;
	}
	memset(&dev_link, 0, sizeof(dev_link));
	dev_link.link_status = ((ifr.ifr_flags & IFF_UP) &&
				(ifr.ifr_flags & IFF_RUNNING));
	ifr.ifr_data = (void *)&edata;
	if (mlx4_ifreq(priv, SIOCETHTOOL, &ifr)) {
		WARN("ioctl(SIOCETHTOOL, ETHTOOL_GSET) failed: %s",
		     strerror(rte_errno));
		return -rte_errno;
	}
	link_speed = ethtool_cmd_speed(&edata);
	if (link_speed == -1)
		dev_link.link_speed = ETH_SPEED_NUM_NONE;
	else
		dev_link.link_speed = link_speed;
	dev_link.link_duplex = ((edata.duplex == DUPLEX_HALF) ?
				ETH_LINK_HALF_DUPLEX : ETH_LINK_FULL_DUPLEX);
	dev_link.link_autoneg = !(dev->data->dev_conf.link_speeds &
				  ETH_LINK_SPEED_FIXED);
	dev->data->dev_link = dev_link;
	return 0;
}

/**
 * DPDK callback to get flow control status.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] fc_conf
 *   Flow control output buffer.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
mlx4_flow_ctrl_get(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	struct ifreq ifr;
	struct ethtool_pauseparam ethpause = {
		.cmd = ETHTOOL_GPAUSEPARAM,
	};
	int ret;

	ifr.ifr_data = (void *)&ethpause;
	if (mlx4_ifreq(priv, SIOCETHTOOL, &ifr)) {
		ret = rte_errno;
		WARN("ioctl(SIOCETHTOOL, ETHTOOL_GPAUSEPARAM)"
		     " failed: %s",
		     strerror(rte_errno));
		goto out;
	}
	fc_conf->autoneg = ethpause.autoneg;
	if (ethpause.rx_pause && ethpause.tx_pause)
		fc_conf->mode = RTE_FC_FULL;
	else if (ethpause.rx_pause)
		fc_conf->mode = RTE_FC_RX_PAUSE;
	else if (ethpause.tx_pause)
		fc_conf->mode = RTE_FC_TX_PAUSE;
	else
		fc_conf->mode = RTE_FC_NONE;
	ret = 0;
out:
	assert(ret >= 0);
	return -ret;
}

/**
 * DPDK callback to modify flow control parameters.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[in] fc_conf
 *   Flow control parameters.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
mlx4_flow_ctrl_set(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	struct ifreq ifr;
	struct ethtool_pauseparam ethpause = {
		.cmd = ETHTOOL_SPAUSEPARAM,
	};
	int ret;

	ifr.ifr_data = (void *)&ethpause;
	ethpause.autoneg = fc_conf->autoneg;
	if (((fc_conf->mode & RTE_FC_FULL) == RTE_FC_FULL) ||
	    (fc_conf->mode & RTE_FC_RX_PAUSE))
		ethpause.rx_pause = 1;
	else
		ethpause.rx_pause = 0;
	if (((fc_conf->mode & RTE_FC_FULL) == RTE_FC_FULL) ||
	    (fc_conf->mode & RTE_FC_TX_PAUSE))
		ethpause.tx_pause = 1;
	else
		ethpause.tx_pause = 0;
	if (mlx4_ifreq(priv, SIOCETHTOOL, &ifr)) {
		ret = rte_errno;
		WARN("ioctl(SIOCETHTOOL, ETHTOOL_SPAUSEPARAM)"
		     " failed: %s",
		     strerror(rte_errno));
		goto out;
	}
	ret = 0;
out:
	assert(ret >= 0);
	return -ret;
}

/**
 * DPDK callback to retrieve the received packet types that are recognized
 * by the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   Pointer to an array of recognized packet types if in Rx burst mode,
 *   NULL otherwise.
 */
const uint32_t *
mlx4_dev_supported_ptypes_get(struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		/* refers to rxq_cq_to_pkt_type() */
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_UNKNOWN
	};
	static const uint32_t ptypes_l2tun[] = {
		/* refers to rxq_cq_to_pkt_type() */
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_UNKNOWN
	};
	struct mlx4_priv *priv = dev->data->dev_private;

	if (dev->rx_pkt_burst == mlx4_rx_burst) {
		if (priv->hw_csum_l2tun)
			return ptypes_l2tun;
		else
			return ptypes;
	}
	return NULL;
}

/**
 * Check if mlx4 device was removed.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   1 when device is removed, otherwise 0.
 */
int
mlx4_is_removed(struct rte_eth_dev *dev)
{
	struct ibv_device_attr device_attr;
	struct mlx4_priv *priv = dev->data->dev_private;

	if (mlx4_glue->query_device(priv->ctx, &device_attr) == EIO)
		return 1;
	return 0;
}
