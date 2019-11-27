/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <assert.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <fcntl.h>
#include <stdalign.h>
#include <sys/un.h>
#include <time.h>

#include <rte_atomic.h>
#include <rte_ethdev_driver.h>
#include <rte_bus_pci.h>
#include <rte_mbuf.h>
#include <rte_common.h>
#include <rte_interrupts.h>
#include <rte_malloc.h>
#include <rte_string_fns.h>
#include <rte_rwlock.h>
#include <rte_cycles.h>

#include "mlx5.h"
#include "mlx5_glue.h"
#include "mlx5_rxtx.h"
#include "mlx5_utils.h"

/* Supported speed values found in /usr/include/linux/ethtool.h */
#ifndef HAVE_SUPPORTED_40000baseKR4_Full
#define SUPPORTED_40000baseKR4_Full (1 << 23)
#endif
#ifndef HAVE_SUPPORTED_40000baseCR4_Full
#define SUPPORTED_40000baseCR4_Full (1 << 24)
#endif
#ifndef HAVE_SUPPORTED_40000baseSR4_Full
#define SUPPORTED_40000baseSR4_Full (1 << 25)
#endif
#ifndef HAVE_SUPPORTED_40000baseLR4_Full
#define SUPPORTED_40000baseLR4_Full (1 << 26)
#endif
#ifndef HAVE_SUPPORTED_56000baseKR4_Full
#define SUPPORTED_56000baseKR4_Full (1 << 27)
#endif
#ifndef HAVE_SUPPORTED_56000baseCR4_Full
#define SUPPORTED_56000baseCR4_Full (1 << 28)
#endif
#ifndef HAVE_SUPPORTED_56000baseSR4_Full
#define SUPPORTED_56000baseSR4_Full (1 << 29)
#endif
#ifndef HAVE_SUPPORTED_56000baseLR4_Full
#define SUPPORTED_56000baseLR4_Full (1 << 30)
#endif

/* Add defines in case the running kernel is not the same as user headers. */
#ifndef ETHTOOL_GLINKSETTINGS
struct ethtool_link_settings {
	uint32_t cmd;
	uint32_t speed;
	uint8_t duplex;
	uint8_t port;
	uint8_t phy_address;
	uint8_t autoneg;
	uint8_t mdio_support;
	uint8_t eth_to_mdix;
	uint8_t eth_tp_mdix_ctrl;
	int8_t link_mode_masks_nwords;
	uint32_t reserved[8];
	uint32_t link_mode_masks[];
};

#define ETHTOOL_GLINKSETTINGS 0x0000004c
#define ETHTOOL_LINK_MODE_1000baseT_Full_BIT 5
#define ETHTOOL_LINK_MODE_Autoneg_BIT 6
#define ETHTOOL_LINK_MODE_1000baseKX_Full_BIT 17
#define ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT 18
#define ETHTOOL_LINK_MODE_10000baseKR_Full_BIT 19
#define ETHTOOL_LINK_MODE_10000baseR_FEC_BIT 20
#define ETHTOOL_LINK_MODE_20000baseMLD2_Full_BIT 21
#define ETHTOOL_LINK_MODE_20000baseKR2_Full_BIT 22
#define ETHTOOL_LINK_MODE_40000baseKR4_Full_BIT 23
#define ETHTOOL_LINK_MODE_40000baseCR4_Full_BIT 24
#define ETHTOOL_LINK_MODE_40000baseSR4_Full_BIT 25
#define ETHTOOL_LINK_MODE_40000baseLR4_Full_BIT 26
#define ETHTOOL_LINK_MODE_56000baseKR4_Full_BIT 27
#define ETHTOOL_LINK_MODE_56000baseCR4_Full_BIT 28
#define ETHTOOL_LINK_MODE_56000baseSR4_Full_BIT 29
#define ETHTOOL_LINK_MODE_56000baseLR4_Full_BIT 30
#endif
#ifndef HAVE_ETHTOOL_LINK_MODE_25G
#define ETHTOOL_LINK_MODE_25000baseCR_Full_BIT 31
#define ETHTOOL_LINK_MODE_25000baseKR_Full_BIT 32
#define ETHTOOL_LINK_MODE_25000baseSR_Full_BIT 33
#endif
#ifndef HAVE_ETHTOOL_LINK_MODE_50G
#define ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT 34
#define ETHTOOL_LINK_MODE_50000baseKR2_Full_BIT 35
#endif
#ifndef HAVE_ETHTOOL_LINK_MODE_100G
#define ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT 36
#define ETHTOOL_LINK_MODE_100000baseSR4_Full_BIT 37
#define ETHTOOL_LINK_MODE_100000baseCR4_Full_BIT 38
#define ETHTOOL_LINK_MODE_100000baseLR4_ER4_Full_BIT 39
#endif

/**
 * Get master interface name from private structure.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[out] ifname
 *   Interface name output buffer.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_get_master_ifname(const char *ibdev_path, char (*ifname)[IF_NAMESIZE])
{
	DIR *dir;
	struct dirent *dent;
	unsigned int dev_type = 0;
	unsigned int dev_port_prev = ~0u;
	char match[IF_NAMESIZE] = "";

	assert(ibdev_path);
	{
		MKSTR(path, "%s/device/net", ibdev_path);

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
		      ibdev_path, name,
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
		if (dev_port == 0)
			strlcpy(match, name, sizeof(match));
	}
	closedir(dir);
	if (match[0] == '\0') {
		rte_errno = ENOENT;
		return -rte_errno;
	}
	strncpy(*ifname, match, sizeof(*ifname));
	return 0;
}

/**
 * Get interface name from private structure.
 *
 * This is a port representor-aware version of mlx5_get_master_ifname().
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[out] ifname
 *   Interface name output buffer.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_get_ifname(const struct rte_eth_dev *dev, char (*ifname)[IF_NAMESIZE])
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int ifindex;

	assert(priv);
	assert(priv->sh);
	ifindex = mlx5_ifindex(dev);
	if (!ifindex) {
		if (!priv->representor)
			return mlx5_get_master_ifname(priv->sh->ibdev_path,
						      ifname);
		rte_errno = ENXIO;
		return -rte_errno;
	}
	if (if_indextoname(ifindex, &(*ifname)[0]))
		return 0;
	rte_errno = errno;
	return -rte_errno;
}

/**
 * Get the interface index from device name.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   Nonzero interface index on success, zero otherwise and rte_errno is set.
 */
unsigned int
mlx5_ifindex(const struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int ifindex;

	assert(priv);
	assert(priv->if_index);
	ifindex = priv->if_index;
	if (!ifindex)
		rte_errno = ENXIO;
	return ifindex;
}

/**
 * Perform ifreq ioctl() on associated Ethernet device.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param req
 *   Request number to pass to ioctl().
 * @param[out] ifr
 *   Interface request structure output buffer.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_ifreq(const struct rte_eth_dev *dev, int req, struct ifreq *ifr)
{
	int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	int ret = 0;

	if (sock == -1) {
		rte_errno = errno;
		return -rte_errno;
	}
	ret = mlx5_get_ifname(dev, &ifr->ifr_name);
	if (ret)
		goto error;
	ret = ioctl(sock, req, ifr);
	if (ret == -1) {
		rte_errno = errno;
		goto error;
	}
	close(sock);
	return 0;
error:
	close(sock);
	return -rte_errno;
}

/**
 * Get device MTU.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[out] mtu
 *   MTU value output buffer.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_get_mtu(struct rte_eth_dev *dev, uint16_t *mtu)
{
	struct ifreq request;
	int ret = mlx5_ifreq(dev, SIOCGIFMTU, &request);

	if (ret)
		return ret;
	*mtu = request.ifr_mtu;
	return 0;
}

/**
 * Set device MTU.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param mtu
 *   MTU value to set.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_set_mtu(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct ifreq request = { .ifr_mtu = mtu, };

	return mlx5_ifreq(dev, SIOCSIFMTU, &request);
}

/**
 * Set device flags.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param keep
 *   Bitmask for flags that must remain untouched.
 * @param flags
 *   Bitmask for flags to modify.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_set_flags(struct rte_eth_dev *dev, unsigned int keep, unsigned int flags)
{
	struct ifreq request;
	int ret = mlx5_ifreq(dev, SIOCGIFFLAGS, &request);

	if (ret)
		return ret;
	request.ifr_flags &= keep;
	request.ifr_flags |= flags & ~keep;
	return mlx5_ifreq(dev, SIOCSIFFLAGS, &request);
}

/**
 * DPDK callback for Ethernet device configuration.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_dev_configure(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int rxqs_n = dev->data->nb_rx_queues;
	unsigned int txqs_n = dev->data->nb_tx_queues;
	const uint8_t use_app_rss_key =
		!!dev->data->dev_conf.rx_adv_conf.rss_conf.rss_key;
	int ret = 0;

	if (use_app_rss_key &&
	    (dev->data->dev_conf.rx_adv_conf.rss_conf.rss_key_len !=
	     MLX5_RSS_HASH_KEY_LEN)) {
		DRV_LOG(ERR, "port %u RSS key len must be %s Bytes long",
			dev->data->port_id, RTE_STR(MLX5_RSS_HASH_KEY_LEN));
		rte_errno = EINVAL;
		return -rte_errno;
	}
	priv->rss_conf.rss_key =
		rte_realloc(priv->rss_conf.rss_key,
			    MLX5_RSS_HASH_KEY_LEN, 0);
	if (!priv->rss_conf.rss_key) {
		DRV_LOG(ERR, "port %u cannot allocate RSS hash key memory (%u)",
			dev->data->port_id, rxqs_n);
		rte_errno = ENOMEM;
		return -rte_errno;
	}

	if (dev->data->dev_conf.rxmode.mq_mode & ETH_MQ_RX_RSS_FLAG)
		dev->data->dev_conf.rxmode.offloads |= DEV_RX_OFFLOAD_RSS_HASH;

	memcpy(priv->rss_conf.rss_key,
	       use_app_rss_key ?
	       dev->data->dev_conf.rx_adv_conf.rss_conf.rss_key :
	       rss_hash_default_key,
	       MLX5_RSS_HASH_KEY_LEN);
	priv->rss_conf.rss_key_len = MLX5_RSS_HASH_KEY_LEN;
	priv->rss_conf.rss_hf = dev->data->dev_conf.rx_adv_conf.rss_conf.rss_hf;
	priv->rxqs = (void *)dev->data->rx_queues;
	priv->txqs = (void *)dev->data->tx_queues;
	if (txqs_n != priv->txqs_n) {
		DRV_LOG(INFO, "port %u Tx queues number update: %u -> %u",
			dev->data->port_id, priv->txqs_n, txqs_n);
		priv->txqs_n = txqs_n;
	}
	if (rxqs_n > priv->config.ind_table_max_size) {
		DRV_LOG(ERR, "port %u cannot handle this many Rx queues (%u)",
			dev->data->port_id, rxqs_n);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	if (rxqs_n != priv->rxqs_n) {
		DRV_LOG(INFO, "port %u Rx queues number update: %u -> %u",
			dev->data->port_id, priv->rxqs_n, rxqs_n);
		priv->rxqs_n = rxqs_n;
	}
	priv->skip_default_rss_reta = 0;
	ret = mlx5_proc_priv_init(dev);
	if (ret)
		return ret;
	return 0;
}

/**
 * Configure default RSS reta.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_dev_configure_rss_reta(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int rxqs_n = dev->data->nb_rx_queues;
	unsigned int i;
	unsigned int j;
	unsigned int reta_idx_n;
	int ret = 0;
	unsigned int *rss_queue_arr = NULL;
	unsigned int rss_queue_n = 0;

	if (priv->skip_default_rss_reta)
		return ret;
	rss_queue_arr = rte_malloc("", rxqs_n * sizeof(unsigned int), 0);
	if (!rss_queue_arr) {
		DRV_LOG(ERR, "port %u cannot allocate RSS queue list (%u)",
			dev->data->port_id, rxqs_n);
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	for (i = 0, j = 0; i < rxqs_n; i++) {
		struct mlx5_rxq_data *rxq_data;
		struct mlx5_rxq_ctrl *rxq_ctrl;

		rxq_data = (*priv->rxqs)[i];
		rxq_ctrl = container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
		if (rxq_ctrl && rxq_ctrl->type == MLX5_RXQ_TYPE_STANDARD)
			rss_queue_arr[j++] = i;
	}
	rss_queue_n = j;
	if (rss_queue_n > priv->config.ind_table_max_size) {
		DRV_LOG(ERR, "port %u cannot handle this many Rx queues (%u)",
			dev->data->port_id, rss_queue_n);
		rte_errno = EINVAL;
		rte_free(rss_queue_arr);
		return -rte_errno;
	}
	DRV_LOG(INFO, "port %u Rx queues number update: %u -> %u",
		dev->data->port_id, priv->rxqs_n, rxqs_n);
	priv->rxqs_n = rxqs_n;
	/*
	 * If the requested number of RX queues is not a power of two,
	 * use the maximum indirection table size for better balancing.
	 * The result is always rounded to the next power of two.
	 */
	reta_idx_n = (1 << log2above((rss_queue_n & (rss_queue_n - 1)) ?
				priv->config.ind_table_max_size :
				rss_queue_n));
	ret = mlx5_rss_reta_index_resize(dev, reta_idx_n);
	if (ret) {
		rte_free(rss_queue_arr);
		return ret;
	}
	/*
	 * When the number of RX queues is not a power of two,
	 * the remaining table entries are padded with reused WQs
	 * and hashes are not spread uniformly.
	 */
	for (i = 0, j = 0; (i != reta_idx_n); ++i) {
		(*priv->reta_idx)[i] = rss_queue_arr[j];
		if (++j == rss_queue_n)
			j = 0;
	}
	rte_free(rss_queue_arr);
	return ret;
}

/**
 * Sets default tuning parameters.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[out] info
 *   Info structure output buffer.
 */
static void
mlx5_set_default_params(struct rte_eth_dev *dev, struct rte_eth_dev_info *info)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	/* Minimum CPU utilization. */
	info->default_rxportconf.ring_size = 256;
	info->default_txportconf.ring_size = 256;
	info->default_rxportconf.burst_size = MLX5_RX_DEFAULT_BURST;
	info->default_txportconf.burst_size = MLX5_TX_DEFAULT_BURST;
	if (priv->link_speed_capa & ETH_LINK_SPEED_100G) {
		info->default_rxportconf.nb_queues = 16;
		info->default_txportconf.nb_queues = 16;
		if (dev->data->nb_rx_queues > 2 ||
		    dev->data->nb_tx_queues > 2) {
			/* Max Throughput. */
			info->default_rxportconf.ring_size = 2048;
			info->default_txportconf.ring_size = 2048;
		}
	} else {
		info->default_rxportconf.nb_queues = 8;
		info->default_txportconf.nb_queues = 8;
		if (dev->data->nb_rx_queues > 2 ||
		    dev->data->nb_tx_queues > 2) {
			/* Max Throughput. */
			info->default_rxportconf.ring_size = 4096;
			info->default_txportconf.ring_size = 4096;
		}
	}
}

/**
 * Sets tx mbuf limiting parameters.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[out] info
 *   Info structure output buffer.
 */
static void
mlx5_set_txlimit_params(struct rte_eth_dev *dev, struct rte_eth_dev_info *info)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_config *config = &priv->config;
	unsigned int inlen;
	uint16_t nb_max;

	inlen = (config->txq_inline_max == MLX5_ARG_UNSET) ?
		MLX5_SEND_DEF_INLINE_LEN :
		(unsigned int)config->txq_inline_max;
	assert(config->txq_inline_min >= 0);
	inlen = RTE_MAX(inlen, (unsigned int)config->txq_inline_min);
	inlen = RTE_MIN(inlen, MLX5_WQE_SIZE_MAX +
			       MLX5_ESEG_MIN_INLINE_SIZE -
			       MLX5_WQE_CSEG_SIZE -
			       MLX5_WQE_ESEG_SIZE -
			       MLX5_WQE_DSEG_SIZE * 2);
	nb_max = (MLX5_WQE_SIZE_MAX +
		  MLX5_ESEG_MIN_INLINE_SIZE -
		  MLX5_WQE_CSEG_SIZE -
		  MLX5_WQE_ESEG_SIZE -
		  MLX5_WQE_DSEG_SIZE -
		  inlen) / MLX5_WSEG_SIZE;
	info->tx_desc_lim.nb_seg_max = nb_max;
	info->tx_desc_lim.nb_mtu_seg_max = nb_max;
}

/**
 * DPDK callback to get information about the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] info
 *   Info structure output buffer.
 */
int
mlx5_dev_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *info)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_config *config = &priv->config;
	unsigned int max;

	/* FIXME: we should ask the device for these values. */
	info->min_rx_bufsize = 32;
	info->max_rx_pktlen = 65536;
	info->max_lro_pkt_size = MLX5_MAX_LRO_SIZE;
	/*
	 * Since we need one CQ per QP, the limit is the minimum number
	 * between the two values.
	 */
	max = RTE_MIN(priv->sh->device_attr.orig_attr.max_cq,
		      priv->sh->device_attr.orig_attr.max_qp);
	/* If max >= 65535 then max = 0, max_rx_queues is uint16_t. */
	if (max >= 65535)
		max = 65535;
	info->max_rx_queues = max;
	info->max_tx_queues = max;
	info->max_mac_addrs = MLX5_MAX_UC_MAC_ADDRESSES;
	info->rx_queue_offload_capa = mlx5_get_rx_queue_offloads(dev);
	info->rx_offload_capa = (mlx5_get_rx_port_offloads() |
				 info->rx_queue_offload_capa);
	info->tx_offload_capa = mlx5_get_tx_port_offloads(dev);
	info->if_index = mlx5_ifindex(dev);
	info->reta_size = priv->reta_idx_n ?
		priv->reta_idx_n : config->ind_table_max_size;
	info->hash_key_size = MLX5_RSS_HASH_KEY_LEN;
	info->speed_capa = priv->link_speed_capa;
	info->flow_type_rss_offloads = ~MLX5_RSS_HF_MASK;
	mlx5_set_default_params(dev, info);
	mlx5_set_txlimit_params(dev, info);
	info->switch_info.name = dev->data->name;
	info->switch_info.domain_id = priv->domain_id;
	info->switch_info.port_id = priv->representor_id;
	if (priv->representor) {
		uint16_t port_id;

		if (priv->pf_bond >= 0) {
			/*
			 * Switch port ID is opaque value with driver defined
			 * format. Push the PF index in bonding configurations
			 * in upper four bits of port ID. If we get too many
			 * representors (more than 4K) or PFs (more than 15)
			 * this approach must be reconsidered.
			 */
			if ((info->switch_info.port_id >>
				MLX5_PORT_ID_BONDING_PF_SHIFT) ||
			    priv->pf_bond > MLX5_PORT_ID_BONDING_PF_MASK) {
				DRV_LOG(ERR, "can't update switch port ID"
					     " for bonding device");
				assert(false);
				return -ENODEV;
			}
			info->switch_info.port_id |=
				priv->pf_bond << MLX5_PORT_ID_BONDING_PF_SHIFT;
		}
		MLX5_ETH_FOREACH_DEV(port_id, priv->pci_dev) {
			struct mlx5_priv *opriv =
				rte_eth_devices[port_id].data->dev_private;

			if (!opriv ||
			    opriv->representor ||
			    opriv->sh != priv->sh ||
			    opriv->domain_id != priv->domain_id)
				continue;
			/*
			 * Override switch name with that of the master
			 * device.
			 */
			info->switch_info.name = opriv->dev_data->name;
			break;
		}
	}
	return 0;
}

/**
 * Get device current raw clock counter
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] time
 *   Current raw clock counter of the device.
 *
 * @return
 *   0 if the clock has correctly been read
 *   The value of errno in case of error
 */
int
mlx5_read_clock(struct rte_eth_dev *dev, uint64_t *clock)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct ibv_context *ctx = priv->sh->ctx;
	struct ibv_values_ex values;
	int err = 0;

	values.comp_mask = IBV_VALUES_MASK_RAW_CLOCK;
	err = mlx5_glue->query_rt_values_ex(ctx, &values);
	if (err != 0) {
		DRV_LOG(WARNING, "Could not query the clock !");
		return err;
	}
	*clock = values.raw_clock.tv_nsec;
	return 0;
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
int mlx5_fw_version_get(struct rte_eth_dev *dev, char *fw_ver, size_t fw_size)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct ibv_device_attr *attr = &priv->sh->device_attr.orig_attr;
	size_t size = strnlen(attr->fw_ver, sizeof(attr->fw_ver)) + 1;

	if (fw_size < size)
		return size;
	if (fw_ver != NULL)
		strlcpy(fw_ver, attr->fw_ver, fw_size);
	return 0;
}

/**
 * Get supported packet types.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   A pointer to the supported Packet types array.
 */
const uint32_t *
mlx5_dev_supported_ptypes_get(struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		/* refers to rxq_cq_to_pkt_type() */
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_NONFRAG,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L4_NONFRAG,
		RTE_PTYPE_INNER_L4_FRAG,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_UNKNOWN
	};

	if (dev->rx_pkt_burst == mlx5_rx_burst ||
	    dev->rx_pkt_burst == mlx5_rx_burst_mprq ||
	    dev->rx_pkt_burst == mlx5_rx_burst_vec)
		return ptypes;
	return NULL;
}

/**
 * Retrieve the master device for representor in the same switch domain.
 *
 * @param dev
 *   Pointer to representor Ethernet device structure.
 *
 * @return
 *   Master device structure  on success, NULL otherwise.
 */

static struct rte_eth_dev *
mlx5_find_master_dev(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv;
	uint16_t port_id;
	uint16_t domain_id;

	priv = dev->data->dev_private;
	domain_id = priv->domain_id;
	assert(priv->representor);
	MLX5_ETH_FOREACH_DEV(port_id, priv->pci_dev) {
		struct mlx5_priv *opriv =
			rte_eth_devices[port_id].data->dev_private;
		if (opriv &&
		    opriv->master &&
		    opriv->domain_id == domain_id &&
		    opriv->sh == priv->sh)
			return &rte_eth_devices[port_id];
	}
	return NULL;
}

/**
 * DPDK callback to retrieve physical link information.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] link
 *   Storage for current link status.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_link_update_unlocked_gset(struct rte_eth_dev *dev,
			       struct rte_eth_link *link)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct ethtool_cmd edata = {
		.cmd = ETHTOOL_GSET /* Deprecated since Linux v4.5. */
	};
	struct ifreq ifr;
	struct rte_eth_link dev_link;
	int link_speed = 0;
	int ret;

	ret = mlx5_ifreq(dev, SIOCGIFFLAGS, &ifr);
	if (ret) {
		DRV_LOG(WARNING, "port %u ioctl(SIOCGIFFLAGS) failed: %s",
			dev->data->port_id, strerror(rte_errno));
		return ret;
	}
	dev_link = (struct rte_eth_link) {
		.link_status = ((ifr.ifr_flags & IFF_UP) &&
				(ifr.ifr_flags & IFF_RUNNING)),
	};
	ifr = (struct ifreq) {
		.ifr_data = (void *)&edata,
	};
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		if (ret == -ENOTSUP && priv->representor) {
			struct rte_eth_dev *master;

			/*
			 * For representors we can try to inherit link
			 * settings from the master device. Actually
			 * link settings do not make a lot of sense
			 * for representors due to missing physical
			 * link. The old kernel drivers supported
			 * emulated settings query for representors,
			 * the new ones do not, so we have to add
			 * this code for compatibility issues.
			 */
			master = mlx5_find_master_dev(dev);
			if (master) {
				ifr = (struct ifreq) {
					.ifr_data = (void *)&edata,
				};
				ret = mlx5_ifreq(master, SIOCETHTOOL, &ifr);
			}
		}
		if (ret) {
			DRV_LOG(WARNING,
				"port %u ioctl(SIOCETHTOOL,"
				" ETHTOOL_GSET) failed: %s",
				dev->data->port_id, strerror(rte_errno));
			return ret;
		}
	}
	link_speed = ethtool_cmd_speed(&edata);
	if (link_speed == -1)
		dev_link.link_speed = ETH_SPEED_NUM_NONE;
	else
		dev_link.link_speed = link_speed;
	priv->link_speed_capa = 0;
	if (edata.supported & SUPPORTED_Autoneg)
		priv->link_speed_capa |= ETH_LINK_SPEED_AUTONEG;
	if (edata.supported & (SUPPORTED_1000baseT_Full |
			       SUPPORTED_1000baseKX_Full))
		priv->link_speed_capa |= ETH_LINK_SPEED_1G;
	if (edata.supported & SUPPORTED_10000baseKR_Full)
		priv->link_speed_capa |= ETH_LINK_SPEED_10G;
	if (edata.supported & (SUPPORTED_40000baseKR4_Full |
			       SUPPORTED_40000baseCR4_Full |
			       SUPPORTED_40000baseSR4_Full |
			       SUPPORTED_40000baseLR4_Full))
		priv->link_speed_capa |= ETH_LINK_SPEED_40G;
	dev_link.link_duplex = ((edata.duplex == DUPLEX_HALF) ?
				ETH_LINK_HALF_DUPLEX : ETH_LINK_FULL_DUPLEX);
	dev_link.link_autoneg = !(dev->data->dev_conf.link_speeds &
			ETH_LINK_SPEED_FIXED);
	if (((dev_link.link_speed && !dev_link.link_status) ||
	     (!dev_link.link_speed && dev_link.link_status))) {
		rte_errno = EAGAIN;
		return -rte_errno;
	}
	*link = dev_link;
	return 0;
}

/**
 * Retrieve physical link information (unlocked version using new ioctl).
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] link
 *   Storage for current link status.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_link_update_unlocked_gs(struct rte_eth_dev *dev,
			     struct rte_eth_link *link)

{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct ethtool_link_settings gcmd = { .cmd = ETHTOOL_GLINKSETTINGS };
	struct ifreq ifr;
	struct rte_eth_link dev_link;
	struct rte_eth_dev *master = NULL;
	uint64_t sc;
	int ret;

	ret = mlx5_ifreq(dev, SIOCGIFFLAGS, &ifr);
	if (ret) {
		DRV_LOG(WARNING, "port %u ioctl(SIOCGIFFLAGS) failed: %s",
			dev->data->port_id, strerror(rte_errno));
		return ret;
	}
	dev_link = (struct rte_eth_link) {
		.link_status = ((ifr.ifr_flags & IFF_UP) &&
				(ifr.ifr_flags & IFF_RUNNING)),
	};
	ifr = (struct ifreq) {
		.ifr_data = (void *)&gcmd,
	};
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		if (ret == -ENOTSUP && priv->representor) {
			/*
			 * For representors we can try to inherit link
			 * settings from the master device. Actually
			 * link settings do not make a lot of sense
			 * for representors due to missing physical
			 * link. The old kernel drivers supported
			 * emulated settings query for representors,
			 * the new ones do not, so we have to add
			 * this code for compatibility issues.
			 */
			master = mlx5_find_master_dev(dev);
			if (master) {
				ifr = (struct ifreq) {
					.ifr_data = (void *)&gcmd,
				};
				ret = mlx5_ifreq(master, SIOCETHTOOL, &ifr);
			}
		}
		if (ret) {
			DRV_LOG(DEBUG,
				"port %u ioctl(SIOCETHTOOL,"
				" ETHTOOL_GLINKSETTINGS) failed: %s",
				dev->data->port_id, strerror(rte_errno));
			return ret;
		}

	}
	gcmd.link_mode_masks_nwords = -gcmd.link_mode_masks_nwords;

	alignas(struct ethtool_link_settings)
	uint8_t data[offsetof(struct ethtool_link_settings, link_mode_masks) +
		     sizeof(uint32_t) * gcmd.link_mode_masks_nwords * 3];
	struct ethtool_link_settings *ecmd = (void *)data;

	*ecmd = gcmd;
	ifr.ifr_data = (void *)ecmd;
	ret = mlx5_ifreq(master ? master : dev, SIOCETHTOOL, &ifr);
	if (ret) {
		DRV_LOG(DEBUG,
			"port %u ioctl(SIOCETHTOOL,"
			"ETHTOOL_GLINKSETTINGS) failed: %s",
			dev->data->port_id, strerror(rte_errno));
		return ret;
	}
	dev_link.link_speed = (ecmd->speed == UINT32_MAX) ? ETH_SPEED_NUM_NONE :
							    ecmd->speed;
	sc = ecmd->link_mode_masks[0] |
		((uint64_t)ecmd->link_mode_masks[1] << 32);
	priv->link_speed_capa = 0;
	if (sc & MLX5_BITSHIFT(ETHTOOL_LINK_MODE_Autoneg_BIT))
		priv->link_speed_capa |= ETH_LINK_SPEED_AUTONEG;
	if (sc & (MLX5_BITSHIFT(ETHTOOL_LINK_MODE_1000baseT_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_1000baseKX_Full_BIT)))
		priv->link_speed_capa |= ETH_LINK_SPEED_1G;
	if (sc & (MLX5_BITSHIFT(ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_10000baseKR_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_10000baseR_FEC_BIT)))
		priv->link_speed_capa |= ETH_LINK_SPEED_10G;
	if (sc & (MLX5_BITSHIFT(ETHTOOL_LINK_MODE_20000baseMLD2_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_20000baseKR2_Full_BIT)))
		priv->link_speed_capa |= ETH_LINK_SPEED_20G;
	if (sc & (MLX5_BITSHIFT(ETHTOOL_LINK_MODE_40000baseKR4_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_40000baseCR4_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_40000baseSR4_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_40000baseLR4_Full_BIT)))
		priv->link_speed_capa |= ETH_LINK_SPEED_40G;
	if (sc & (MLX5_BITSHIFT(ETHTOOL_LINK_MODE_56000baseKR4_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_56000baseCR4_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_56000baseSR4_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_56000baseLR4_Full_BIT)))
		priv->link_speed_capa |= ETH_LINK_SPEED_56G;
	if (sc & (MLX5_BITSHIFT(ETHTOOL_LINK_MODE_25000baseCR_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_25000baseKR_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_25000baseSR_Full_BIT)))
		priv->link_speed_capa |= ETH_LINK_SPEED_25G;
	if (sc & (MLX5_BITSHIFT(ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_50000baseKR2_Full_BIT)))
		priv->link_speed_capa |= ETH_LINK_SPEED_50G;
	if (sc & (MLX5_BITSHIFT(ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_100000baseSR4_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_100000baseCR4_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_100000baseLR4_ER4_Full_BIT)))
		priv->link_speed_capa |= ETH_LINK_SPEED_100G;
	dev_link.link_duplex = ((ecmd->duplex == DUPLEX_HALF) ?
				ETH_LINK_HALF_DUPLEX : ETH_LINK_FULL_DUPLEX);
	dev_link.link_autoneg = !(dev->data->dev_conf.link_speeds &
				  ETH_LINK_SPEED_FIXED);
	if (((dev_link.link_speed && !dev_link.link_status) ||
	     (!dev_link.link_speed && dev_link.link_status))) {
		rte_errno = EAGAIN;
		return -rte_errno;
	}
	*link = dev_link;
	return 0;
}

/**
 * DPDK callback to retrieve physical link information.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param wait_to_complete
 *   Wait for request completion.
 *
 * @return
 *   0 if link status was not updated, positive if it was, a negative errno
 *   value otherwise and rte_errno is set.
 */
int
mlx5_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	int ret;
	struct rte_eth_link dev_link;
	time_t start_time = time(NULL);
	int retry = MLX5_GET_LINK_STATUS_RETRY_COUNT;

	do {
		ret = mlx5_link_update_unlocked_gs(dev, &dev_link);
		if (ret == -ENOTSUP)
			ret = mlx5_link_update_unlocked_gset(dev, &dev_link);
		if (ret == 0)
			break;
		/* Handle wait to complete situation. */
		if ((wait_to_complete || retry) && ret == -EAGAIN) {
			if (abs((int)difftime(time(NULL), start_time)) <
			    MLX5_LINK_STATUS_TIMEOUT) {
				usleep(0);
				continue;
			} else {
				rte_errno = EBUSY;
				return -rte_errno;
			}
		} else if (ret < 0) {
			return ret;
		}
	} while (wait_to_complete || retry-- > 0);
	ret = !!memcmp(&dev->data->dev_link, &dev_link,
		       sizeof(struct rte_eth_link));
	dev->data->dev_link = dev_link;
	return ret;
}

/**
 * DPDK callback to change the MTU.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param in_mtu
 *   New MTU.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_dev_set_mtu(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint16_t kern_mtu = 0;
	int ret;

	ret = mlx5_get_mtu(dev, &kern_mtu);
	if (ret)
		return ret;
	/* Set kernel interface MTU first. */
	ret = mlx5_set_mtu(dev, mtu);
	if (ret)
		return ret;
	ret = mlx5_get_mtu(dev, &kern_mtu);
	if (ret)
		return ret;
	if (kern_mtu == mtu) {
		priv->mtu = mtu;
		DRV_LOG(DEBUG, "port %u adapter MTU set to %u",
			dev->data->port_id, mtu);
		return 0;
	}
	rte_errno = EAGAIN;
	return -rte_errno;
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
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_dev_get_flow_ctrl(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct ifreq ifr;
	struct ethtool_pauseparam ethpause = {
		.cmd = ETHTOOL_GPAUSEPARAM
	};
	int ret;

	ifr.ifr_data = (void *)&ethpause;
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		DRV_LOG(WARNING,
			"port %u ioctl(SIOCETHTOOL, ETHTOOL_GPAUSEPARAM) failed:"
			" %s",
			dev->data->port_id, strerror(rte_errno));
		return ret;
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
	return 0;
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
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_dev_set_flow_ctrl(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct ifreq ifr;
	struct ethtool_pauseparam ethpause = {
		.cmd = ETHTOOL_SPAUSEPARAM
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
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		DRV_LOG(WARNING,
			"port %u ioctl(SIOCETHTOOL, ETHTOOL_SPAUSEPARAM)"
			" failed: %s",
			dev->data->port_id, strerror(rte_errno));
		return ret;
	}
	return 0;
}

/**
 * Get PCI information by sysfs device path.
 *
 * @param dev_path
 *   Pointer to device sysfs folder name.
 * @param[out] pci_addr
 *   PCI bus address output buffer.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_dev_to_pci_addr(const char *dev_path,
		     struct rte_pci_addr *pci_addr)
{
	FILE *file;
	char line[32];
	MKSTR(path, "%s/device/uevent", dev_path);

	file = fopen(path, "rb");
	if (file == NULL) {
		rte_errno = errno;
		return -rte_errno;
	}
	while (fgets(line, sizeof(line), file) == line) {
		size_t len = strlen(line);
		int ret;

		/* Truncate long lines. */
		if (len == (sizeof(line) - 1))
			while (line[(len - 1)] != '\n') {
				ret = fgetc(file);
				if (ret == EOF)
					break;
				line[(len - 1)] = ret;
			}
		/* Extract information. */
		if (sscanf(line,
			   "PCI_SLOT_NAME="
			   "%" SCNx32 ":%" SCNx8 ":%" SCNx8 ".%" SCNx8 "\n",
			   &pci_addr->domain,
			   &pci_addr->bus,
			   &pci_addr->devid,
			   &pci_addr->function) == 4) {
			ret = 0;
			break;
		}
	}
	fclose(file);
	return 0;
}

/**
 * Handle asynchronous removal event for entire multiport device.
 *
 * @param sh
 *   Infiniband device shared context.
 */
static void
mlx5_dev_interrupt_device_fatal(struct mlx5_ibv_shared *sh)
{
	uint32_t i;

	for (i = 0; i < sh->max_port; ++i) {
		struct rte_eth_dev *dev;

		if (sh->port[i].ih_port_id >= RTE_MAX_ETHPORTS) {
			/*
			 * Or not existing port either no
			 * handler installed for this port.
			 */
			continue;
		}
		dev = &rte_eth_devices[sh->port[i].ih_port_id];
		assert(dev);
		if (dev->data->dev_conf.intr_conf.rmv)
			_rte_eth_dev_callback_process
				(dev, RTE_ETH_EVENT_INTR_RMV, NULL);
	}
}

/**
 * Handle shared asynchronous events the NIC (removal event
 * and link status change). Supports multiport IB device.
 *
 * @param cb_arg
 *   Callback argument.
 */
void
mlx5_dev_interrupt_handler(void *cb_arg)
{
	struct mlx5_ibv_shared *sh = cb_arg;
	struct ibv_async_event event;

	/* Read all message from the IB device and acknowledge them. */
	for (;;) {
		struct rte_eth_dev *dev;
		uint32_t tmp;

		if (mlx5_glue->get_async_event(sh->ctx, &event))
			break;
		/* Retrieve and check IB port index. */
		tmp = (uint32_t)event.element.port_num;
		if (!tmp && event.event_type == IBV_EVENT_DEVICE_FATAL) {
			/*
			 * The DEVICE_FATAL event is called once for
			 * entire device without port specifying.
			 * We should notify all existing ports.
			 */
			mlx5_glue->ack_async_event(&event);
			mlx5_dev_interrupt_device_fatal(sh);
			continue;
		}
		assert(tmp && (tmp <= sh->max_port));
		if (!tmp) {
			/* Unsupported devive level event. */
			mlx5_glue->ack_async_event(&event);
			DRV_LOG(DEBUG,
				"unsupported common event (type %d)",
				event.event_type);
			continue;
		}
		if (tmp > sh->max_port) {
			/* Invalid IB port index. */
			mlx5_glue->ack_async_event(&event);
			DRV_LOG(DEBUG,
				"cannot handle an event (type %d)"
				"due to invalid IB port index (%u)",
				event.event_type, tmp);
			continue;
		}
		if (sh->port[tmp - 1].ih_port_id >= RTE_MAX_ETHPORTS) {
			/* No handler installed. */
			mlx5_glue->ack_async_event(&event);
			DRV_LOG(DEBUG,
				"cannot handle an event (type %d)"
				"due to no handler installed for port %u",
				event.event_type, tmp);
			continue;
		}
		/* Retrieve ethernet device descriptor. */
		tmp = sh->port[tmp - 1].ih_port_id;
		dev = &rte_eth_devices[tmp];
		assert(dev);
		if ((event.event_type == IBV_EVENT_PORT_ACTIVE ||
		     event.event_type == IBV_EVENT_PORT_ERR) &&
			dev->data->dev_conf.intr_conf.lsc) {
			mlx5_glue->ack_async_event(&event);
			if (mlx5_link_update(dev, 0) == -EAGAIN) {
				usleep(0);
				continue;
			}
			_rte_eth_dev_callback_process
				(dev, RTE_ETH_EVENT_INTR_LSC, NULL);
			continue;
		}
		DRV_LOG(DEBUG,
			"port %u cannot handle an unknown event (type %d)",
			dev->data->port_id, event.event_type);
		mlx5_glue->ack_async_event(&event);
	}
}

/*
 * Unregister callback handler safely. The handler may be active
 * while we are trying to unregister it, in this case code -EAGAIN
 * is returned by rte_intr_callback_unregister(). This routine checks
 * the return code and tries to unregister handler again.
 *
 * @param handle
 *   interrupt handle
 * @param cb_fn
 *   pointer to callback routine
 * @cb_arg
 *   opaque callback parameter
 */
void
mlx5_intr_callback_unregister(const struct rte_intr_handle *handle,
			      rte_intr_callback_fn cb_fn, void *cb_arg)
{
	/*
	 * Try to reduce timeout management overhead by not calling
	 * the timer related routines on the first iteration. If the
	 * unregistering succeeds on first call there will be no
	 * timer calls at all.
	 */
	uint64_t twait = 0;
	uint64_t start = 0;

	do {
		int ret;

		ret = rte_intr_callback_unregister(handle, cb_fn, cb_arg);
		if (ret >= 0)
			return;
		if (ret != -EAGAIN) {
			DRV_LOG(INFO, "failed to unregister interrupt"
				      " handler (error: %d)", ret);
			assert(false);
			return;
		}
		if (twait) {
			struct timespec onems;

			/* Wait one millisecond and try again. */
			onems.tv_sec = 0;
			onems.tv_nsec = NS_PER_S / MS_PER_S;
			nanosleep(&onems, 0);
			/* Check whether one second elapsed. */
			if ((rte_get_timer_cycles() - start) <= twait)
				continue;
		} else {
			/*
			 * We get the amount of timer ticks for one second.
			 * If this amount elapsed it means we spent one
			 * second in waiting. This branch is executed once
			 * on first iteration.
			 */
			twait = rte_get_timer_hz();
			assert(twait);
		}
		/*
		 * Timeout elapsed, show message (once a second) and retry.
		 * We have no other acceptable option here, if we ignore
		 * the unregistering return code the handler will not
		 * be unregistered, fd will be closed and we may get the
		 * crush. Hanging and messaging in the loop seems not to be
		 * the worst choice.
		 */
		DRV_LOG(INFO, "Retrying to unregister interrupt handler");
		start = rte_get_timer_cycles();
	} while (true);
}

/**
 * Handle DEVX interrupts from the NIC.
 * This function is probably called from the DPDK host thread.
 *
 * @param cb_arg
 *   Callback argument.
 */
void
mlx5_dev_interrupt_handler_devx(void *cb_arg)
{
#ifndef HAVE_IBV_DEVX_ASYNC
	(void)cb_arg;
	return;
#else
	struct mlx5_ibv_shared *sh = cb_arg;
	union {
		struct mlx5dv_devx_async_cmd_hdr cmd_resp;
		uint8_t buf[MLX5_ST_SZ_BYTES(query_flow_counter_out) +
			    MLX5_ST_SZ_BYTES(traffic_counter) +
			    sizeof(struct mlx5dv_devx_async_cmd_hdr)];
	} out;
	uint8_t *buf = out.buf + sizeof(out.cmd_resp);

	while (!mlx5_glue->devx_get_async_cmd_comp(sh->devx_comp,
						   &out.cmd_resp,
						   sizeof(out.buf)))
		mlx5_flow_async_pool_query_handle
			(sh, (uint64_t)out.cmd_resp.wr_id,
			 mlx5_devx_get_out_command_status(buf));
#endif /* HAVE_IBV_DEVX_ASYNC */
}

/**
 * Uninstall shared asynchronous device events handler.
 * This function is implemented to support event sharing
 * between multiple ports of single IB device.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
static void
mlx5_dev_shared_handler_uninstall(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ibv_shared *sh = priv->sh;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;
	pthread_mutex_lock(&sh->intr_mutex);
	assert(priv->ibv_port);
	assert(priv->ibv_port <= sh->max_port);
	assert(dev->data->port_id < RTE_MAX_ETHPORTS);
	if (sh->port[priv->ibv_port - 1].ih_port_id >= RTE_MAX_ETHPORTS)
		goto exit;
	assert(sh->port[priv->ibv_port - 1].ih_port_id ==
					(uint32_t)dev->data->port_id);
	assert(sh->intr_cnt);
	sh->port[priv->ibv_port - 1].ih_port_id = RTE_MAX_ETHPORTS;
	if (!sh->intr_cnt || --sh->intr_cnt)
		goto exit;
	mlx5_intr_callback_unregister(&sh->intr_handle,
				     mlx5_dev_interrupt_handler, sh);
	sh->intr_handle.fd = 0;
	sh->intr_handle.type = RTE_INTR_HANDLE_UNKNOWN;
exit:
	pthread_mutex_unlock(&sh->intr_mutex);
}

/**
 * Uninstall devx shared asynchronous device events handler.
 * This function is implemeted to support event sharing
 * between multiple ports of single IB device.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
static void
mlx5_dev_shared_handler_devx_uninstall(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ibv_shared *sh = priv->sh;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;
	pthread_mutex_lock(&sh->intr_mutex);
	assert(priv->ibv_port);
	assert(priv->ibv_port <= sh->max_port);
	assert(dev->data->port_id < RTE_MAX_ETHPORTS);
	if (sh->port[priv->ibv_port - 1].devx_ih_port_id >= RTE_MAX_ETHPORTS)
		goto exit;
	assert(sh->port[priv->ibv_port - 1].devx_ih_port_id ==
					(uint32_t)dev->data->port_id);
	sh->port[priv->ibv_port - 1].devx_ih_port_id = RTE_MAX_ETHPORTS;
	if (!sh->devx_intr_cnt || --sh->devx_intr_cnt)
		goto exit;
	if (sh->intr_handle_devx.fd) {
		rte_intr_callback_unregister(&sh->intr_handle_devx,
					     mlx5_dev_interrupt_handler_devx,
					     sh);
		sh->intr_handle_devx.fd = 0;
		sh->intr_handle_devx.type = RTE_INTR_HANDLE_UNKNOWN;
	}
	if (sh->devx_comp) {
		mlx5_glue->devx_destroy_cmd_comp(sh->devx_comp);
		sh->devx_comp = NULL;
	}
exit:
	pthread_mutex_unlock(&sh->intr_mutex);
}

/**
 * Install shared asynchronous device events handler.
 * This function is implemented to support event sharing
 * between multiple ports of single IB device.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
static void
mlx5_dev_shared_handler_install(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ibv_shared *sh = priv->sh;
	int ret;
	int flags;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;
	pthread_mutex_lock(&sh->intr_mutex);
	assert(priv->ibv_port);
	assert(priv->ibv_port <= sh->max_port);
	assert(dev->data->port_id < RTE_MAX_ETHPORTS);
	if (sh->port[priv->ibv_port - 1].ih_port_id < RTE_MAX_ETHPORTS) {
		/* The handler is already installed for this port. */
		assert(sh->intr_cnt);
		goto exit;
	}
	if (sh->intr_cnt) {
		sh->port[priv->ibv_port - 1].ih_port_id =
						(uint32_t)dev->data->port_id;
		sh->intr_cnt++;
		goto exit;
	}
	/* No shared handler installed. */
	assert(sh->ctx->async_fd > 0);
	flags = fcntl(sh->ctx->async_fd, F_GETFL);
	ret = fcntl(sh->ctx->async_fd, F_SETFL, flags | O_NONBLOCK);
	if (ret) {
		DRV_LOG(INFO, "failed to change file descriptor async event"
			" queue");
		/* Indicate there will be no interrupts. */
		dev->data->dev_conf.intr_conf.lsc = 0;
		dev->data->dev_conf.intr_conf.rmv = 0;
	} else {
		sh->intr_handle.fd = sh->ctx->async_fd;
		sh->intr_handle.type = RTE_INTR_HANDLE_EXT;
		rte_intr_callback_register(&sh->intr_handle,
					   mlx5_dev_interrupt_handler, sh);
		sh->intr_cnt++;
		sh->port[priv->ibv_port - 1].ih_port_id =
						(uint32_t)dev->data->port_id;
	}
exit:
	pthread_mutex_unlock(&sh->intr_mutex);
}

/**
 * Install devx shared asyncronous device events handler.
 * This function is implemeted to support event sharing
 * between multiple ports of single IB device.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
static void
mlx5_dev_shared_handler_devx_install(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ibv_shared *sh = priv->sh;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;
	pthread_mutex_lock(&sh->intr_mutex);
	assert(priv->ibv_port);
	assert(priv->ibv_port <= sh->max_port);
	assert(dev->data->port_id < RTE_MAX_ETHPORTS);
	if (sh->port[priv->ibv_port - 1].devx_ih_port_id < RTE_MAX_ETHPORTS) {
		/* The handler is already installed for this port. */
		assert(sh->devx_intr_cnt);
		goto exit;
	}
	if (sh->devx_intr_cnt) {
		sh->devx_intr_cnt++;
		sh->port[priv->ibv_port - 1].devx_ih_port_id =
					(uint32_t)dev->data->port_id;
		goto exit;
	}
	if (priv->config.devx) {
#ifndef HAVE_IBV_DEVX_ASYNC
		goto exit;
#else
		sh->devx_comp = mlx5_glue->devx_create_cmd_comp(sh->ctx);
		if (sh->devx_comp) {
			int flags = fcntl(sh->devx_comp->fd, F_GETFL);
			int ret = fcntl(sh->devx_comp->fd, F_SETFL,
				    flags | O_NONBLOCK);

			if (ret) {
				DRV_LOG(INFO, "failed to change file descriptor"
					" devx async event queue");
			} else {
				sh->intr_handle_devx.fd = sh->devx_comp->fd;
				sh->intr_handle_devx.type = RTE_INTR_HANDLE_EXT;
				rte_intr_callback_register
					(&sh->intr_handle_devx,
					 mlx5_dev_interrupt_handler_devx, sh);
				sh->devx_intr_cnt++;
				sh->port[priv->ibv_port - 1].devx_ih_port_id =
						(uint32_t)dev->data->port_id;
			}
		}
#endif /* HAVE_IBV_DEVX_ASYNC */
	}
exit:
	pthread_mutex_unlock(&sh->intr_mutex);
}

/**
 * Uninstall interrupt handler.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
void
mlx5_dev_interrupt_handler_uninstall(struct rte_eth_dev *dev)
{
	mlx5_dev_shared_handler_uninstall(dev);
}

/**
 * Install interrupt handler.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
void
mlx5_dev_interrupt_handler_install(struct rte_eth_dev *dev)
{
	mlx5_dev_shared_handler_install(dev);
}

/**
 * Devx uninstall interrupt handler.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
void
mlx5_dev_interrupt_handler_devx_uninstall(struct rte_eth_dev *dev)
{
	mlx5_dev_shared_handler_devx_uninstall(dev);
}

/**
 * Devx install interrupt handler.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
void
mlx5_dev_interrupt_handler_devx_install(struct rte_eth_dev *dev)
{
	mlx5_dev_shared_handler_devx_install(dev);
}

/**
 * DPDK callback to bring the link DOWN.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_set_link_down(struct rte_eth_dev *dev)
{
	return mlx5_set_flags(dev, ~IFF_UP, ~IFF_UP);
}

/**
 * DPDK callback to bring the link UP.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_set_link_up(struct rte_eth_dev *dev)
{
	return mlx5_set_flags(dev, ~IFF_UP, IFF_UP);
}

/**
 * Configure the RX function to use.
 *
 * @param dev
 *   Pointer to private data structure.
 *
 * @return
 *   Pointer to selected Rx burst function.
 */
eth_rx_burst_t
mlx5_select_rx_function(struct rte_eth_dev *dev)
{
	eth_rx_burst_t rx_pkt_burst = mlx5_rx_burst;

	assert(dev != NULL);
	if (mlx5_check_vec_rx_support(dev) > 0) {
		rx_pkt_burst = mlx5_rx_burst_vec;
		DRV_LOG(DEBUG, "port %u selected Rx vectorized function",
			dev->data->port_id);
	} else if (mlx5_mprq_enabled(dev)) {
		rx_pkt_burst = mlx5_rx_burst_mprq;
	}
	return rx_pkt_burst;
}

/**
 * Check if mlx5 device was removed.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   1 when device is removed, otherwise 0.
 */
int
mlx5_is_removed(struct rte_eth_dev *dev)
{
	struct ibv_device_attr device_attr;
	struct mlx5_priv *priv = dev->data->dev_private;

	if (mlx5_glue->query_device(priv->sh->ctx, &device_attr) == EIO)
		return 1;
	return 0;
}

/**
 * Get the E-Switch parameters by port id.
 *
 * @param[in] port
 *   Device port id.
 * @param[in] valid
 *   Device port id is valid, skip check. This flag is useful
 *   when trials are performed from probing and device is not
 *   flagged as valid yet (in attaching process).
 * @param[out] es_domain_id
 *   E-Switch domain id.
 * @param[out] es_port_id
 *   The port id of the port in the E-Switch.
 *
 * @return
 *   pointer to device private data structure containing data needed
 *   on success, NULL otherwise and rte_errno is set.
 */
struct mlx5_priv *
mlx5_port_to_eswitch_info(uint16_t port, bool valid)
{
	struct rte_eth_dev *dev;
	struct mlx5_priv *priv;

	if (port >= RTE_MAX_ETHPORTS) {
		rte_errno = EINVAL;
		return NULL;
	}
	if (!valid && !rte_eth_dev_is_valid_port(port)) {
		rte_errno = ENODEV;
		return NULL;
	}
	dev = &rte_eth_devices[port];
	priv = dev->data->dev_private;
	if (!(priv->representor || priv->master)) {
		rte_errno = EINVAL;
		return NULL;
	}
	return priv;
}

/**
 * Get the E-Switch parameters by device instance.
 *
 * @param[in] port
 *   Device port id.
 * @param[out] es_domain_id
 *   E-Switch domain id.
 * @param[out] es_port_id
 *   The port id of the port in the E-Switch.
 *
 * @return
 *   pointer to device private data structure containing data needed
 *   on success, NULL otherwise and rte_errno is set.
 */
struct mlx5_priv *
mlx5_dev_to_eswitch_info(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv;

	priv = dev->data->dev_private;
	if (!(priv->representor || priv->master)) {
		rte_errno = EINVAL;
		return NULL;
	}
	return priv;
}

/**
 * Get switch information associated with network interface.
 *
 * @param ifindex
 *   Network interface index.
 * @param[out] info
 *   Switch information object, populated in case of success.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_sysfs_switch_info(unsigned int ifindex, struct mlx5_switch_info *info)
{
	char ifname[IF_NAMESIZE];
	char port_name[IF_NAMESIZE];
	FILE *file;
	struct mlx5_switch_info data = {
		.master = 0,
		.representor = 0,
		.name_type = MLX5_PHYS_PORT_NAME_TYPE_NOTSET,
		.port_name = 0,
		.switch_id = 0,
	};
	DIR *dir;
	bool port_switch_id_set = false;
	bool device_dir = false;
	char c;
	int ret;

	if (!if_indextoname(ifindex, ifname)) {
		rte_errno = errno;
		return -rte_errno;
	}

	MKSTR(phys_port_name, "/sys/class/net/%s/phys_port_name",
	      ifname);
	MKSTR(phys_switch_id, "/sys/class/net/%s/phys_switch_id",
	      ifname);
	MKSTR(pci_device, "/sys/class/net/%s/device",
	      ifname);

	file = fopen(phys_port_name, "rb");
	if (file != NULL) {
		ret = fscanf(file, "%s", port_name);
		fclose(file);
		if (ret == 1)
			mlx5_translate_port_name(port_name, &data);
	}
	file = fopen(phys_switch_id, "rb");
	if (file == NULL) {
		rte_errno = errno;
		return -rte_errno;
	}
	port_switch_id_set =
		fscanf(file, "%" SCNx64 "%c", &data.switch_id, &c) == 2 &&
		c == '\n';
	fclose(file);
	dir = opendir(pci_device);
	if (dir != NULL) {
		closedir(dir);
		device_dir = true;
	}
	if (port_switch_id_set) {
		/* We have some E-Switch configuration. */
		mlx5_sysfs_check_switch_info(device_dir, &data);
	}
	*info = data;
	assert(!(data.master && data.representor));
	if (data.master && data.representor) {
		DRV_LOG(ERR, "ifindex %u device is recognized as master"
			     " and as representor", ifindex);
		rte_errno = ENODEV;
		return -rte_errno;
	}
	return 0;
}

/**
 * Analyze gathered port parameters via Netlink to recognize master
 * and representor devices for E-Switch configuration.
 *
 * @param[in] num_vf_set
 *   flag of presence of number of VFs port attribute.
 * @param[inout] switch_info
 *   Port information, including port name as a number and port name
 *   type if recognized
 *
 * @return
 *   master and representor flags are set in switch_info according to
 *   recognized parameters (if any).
 */
void
mlx5_nl_check_switch_info(bool num_vf_set,
			  struct mlx5_switch_info *switch_info)
{
	switch (switch_info->name_type) {
	case MLX5_PHYS_PORT_NAME_TYPE_UNKNOWN:
		/*
		 * Name is not recognized, assume the master,
		 * check the number of VFs key presence.
		 */
		switch_info->master = num_vf_set;
		break;
	case MLX5_PHYS_PORT_NAME_TYPE_NOTSET:
		/*
		 * Name is not set, this assumes the legacy naming
		 * schema for master, just check if there is a
		 * number of VFs key.
		 */
		switch_info->master = num_vf_set;
		break;
	case MLX5_PHYS_PORT_NAME_TYPE_UPLINK:
		/* New uplink naming schema recognized. */
		switch_info->master = 1;
		break;
	case MLX5_PHYS_PORT_NAME_TYPE_LEGACY:
		/* Legacy representors naming schema. */
		switch_info->representor = !num_vf_set;
		break;
	case MLX5_PHYS_PORT_NAME_TYPE_PFVF:
		/* New representors naming schema. */
		switch_info->representor = 1;
		break;
	}
}

/**
 * Analyze gathered port parameters via sysfs to recognize master
 * and representor devices for E-Switch configuration.
 *
 * @param[in] device_dir
 *   flag of presence of "device" directory under port device key.
 * @param[inout] switch_info
 *   Port information, including port name as a number and port name
 *   type if recognized
 *
 * @return
 *   master and representor flags are set in switch_info according to
 *   recognized parameters (if any).
 */
void
mlx5_sysfs_check_switch_info(bool device_dir,
			     struct mlx5_switch_info *switch_info)
{
	switch (switch_info->name_type) {
	case MLX5_PHYS_PORT_NAME_TYPE_UNKNOWN:
		/*
		 * Name is not recognized, assume the master,
		 * check the device directory presence.
		 */
		switch_info->master = device_dir;
		break;
	case MLX5_PHYS_PORT_NAME_TYPE_NOTSET:
		/*
		 * Name is not set, this assumes the legacy naming
		 * schema for master, just check if there is
		 * a device directory.
		 */
		switch_info->master = device_dir;
		break;
	case MLX5_PHYS_PORT_NAME_TYPE_UPLINK:
		/* New uplink naming schema recognized. */
		switch_info->master = 1;
		break;
	case MLX5_PHYS_PORT_NAME_TYPE_LEGACY:
		/* Legacy representors naming schema. */
		switch_info->representor = !device_dir;
		break;
	case MLX5_PHYS_PORT_NAME_TYPE_PFVF:
		/* New representors naming schema. */
		switch_info->representor = 1;
		break;
	}
}

/**
 * Extract port name, as a number, from sysfs or netlink information.
 *
 * @param[in] port_name_in
 *   String representing the port name.
 * @param[out] port_info_out
 *   Port information, including port name as a number and port name
 *   type if recognized
 *
 * @return
 *   port_name field set according to recognized name format.
 */
void
mlx5_translate_port_name(const char *port_name_in,
			 struct mlx5_switch_info *port_info_out)
{
	char pf_c1, pf_c2, vf_c1, vf_c2;
	char *end;
	int sc_items;

	/*
	 * Check for port-name as a string of the form pf0vf0
	 * (support kernel ver >= 5.0 or OFED ver >= 4.6).
	 */
	sc_items = sscanf(port_name_in, "%c%c%d%c%c%d",
			  &pf_c1, &pf_c2, &port_info_out->pf_num,
			  &vf_c1, &vf_c2, &port_info_out->port_name);
	if (sc_items == 6 &&
	    pf_c1 == 'p' && pf_c2 == 'f' &&
	    vf_c1 == 'v' && vf_c2 == 'f') {
		port_info_out->name_type = MLX5_PHYS_PORT_NAME_TYPE_PFVF;
		return;
	}
	/*
	 * Check for port-name as a string of the form p0
	 * (support kernel ver >= 5.0, or OFED ver >= 4.6).
	 */
	sc_items = sscanf(port_name_in, "%c%d",
			  &pf_c1, &port_info_out->port_name);
	if (sc_items == 2 && pf_c1 == 'p') {
		port_info_out->name_type = MLX5_PHYS_PORT_NAME_TYPE_UPLINK;
		return;
	}
	/* Check for port-name as a number (support kernel ver < 5.0 */
	errno = 0;
	port_info_out->port_name = strtol(port_name_in, &end, 0);
	if (!errno &&
	    (size_t)(end - port_name_in) == strlen(port_name_in)) {
		port_info_out->name_type = MLX5_PHYS_PORT_NAME_TYPE_LEGACY;
		return;
	}
	port_info_out->name_type = MLX5_PHYS_PORT_NAME_TYPE_UNKNOWN;
	return;
}

/**
 * DPDK callback to retrieve plug-in module EEPROM information (type and size).
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] modinfo
 *   Storage for plug-in module EEPROM information.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_get_module_info(struct rte_eth_dev *dev,
		     struct rte_eth_dev_module_info *modinfo)
{
	struct ethtool_modinfo info = {
		.cmd = ETHTOOL_GMODULEINFO,
	};
	struct ifreq ifr = (struct ifreq) {
		.ifr_data = (void *)&info,
	};
	int ret = 0;

	if (!dev || !modinfo) {
		DRV_LOG(WARNING, "missing argument, cannot get module info");
		rte_errno = EINVAL;
		return -rte_errno;
	}
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		DRV_LOG(WARNING, "port %u ioctl(SIOCETHTOOL) failed: %s",
			dev->data->port_id, strerror(rte_errno));
		return ret;
	}
	modinfo->type = info.type;
	modinfo->eeprom_len = info.eeprom_len;
	return ret;
}

/**
 * DPDK callback to retrieve plug-in module EEPROM data.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] info
 *   Storage for plug-in module EEPROM data.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int mlx5_get_module_eeprom(struct rte_eth_dev *dev,
			   struct rte_dev_eeprom_info *info)
{
	struct ethtool_eeprom *eeprom;
	struct ifreq ifr;
	int ret = 0;

	if (!dev || !info) {
		DRV_LOG(WARNING, "missing argument, cannot get module eeprom");
		rte_errno = EINVAL;
		return -rte_errno;
	}
	eeprom = rte_calloc(__func__, 1,
			    (sizeof(struct ethtool_eeprom) + info->length), 0);
	if (!eeprom) {
		DRV_LOG(WARNING, "port %u cannot allocate memory for "
			"eeprom data", dev->data->port_id);
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	eeprom->cmd = ETHTOOL_GMODULEEEPROM;
	eeprom->offset = info->offset;
	eeprom->len = info->length;
	ifr = (struct ifreq) {
		.ifr_data = (void *)eeprom,
	};
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret)
		DRV_LOG(WARNING, "port %u ioctl(SIOCETHTOOL) failed: %s",
			dev->data->port_id, strerror(rte_errno));
	else
		rte_memcpy(info->data, eeprom->data, info->length);
	rte_free(eeprom);
	return ret;
}

/**
 * DPDK callback to retrieve hairpin capabilities.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] cap
 *   Storage for hairpin capability data.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int mlx5_hairpin_cap_get(struct rte_eth_dev *dev,
			 struct rte_eth_hairpin_cap *cap)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (priv->sh->devx == 0) {
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	cap->max_nb_queues = UINT16_MAX;
	cap->max_rx_2_tx = 1;
	cap->max_tx_2_rx = 1;
	cap->max_nb_desc = 8192;
	return 0;
}
