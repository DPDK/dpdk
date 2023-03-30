/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Marvell International Ltd.
 * Copyright(c) 2018 Semihalf.
 * All rights reserved.
 */

#include <rte_string_fns.h>
#include <ethdev_driver.h>
#include <rte_kvargs.h>
#include <bus_vdev_driver.h>

#include <stdio.h>
#include <fcntl.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <rte_mvep_common.h>

#include "mvneta_rxtx.h"


#define MVNETA_IFACE_NAME_ARG "iface"

#define MVNETA_PKT_SIZE_MAX (16382 - MV_MH_SIZE) /* 9700B */
#define MVNETA_DEFAULT_MTU 1500

#define MVNETA_MAC_ADDRS_MAX 256 /*16 UC, 256 IP, 256 MC/BC */
/** Maximum length of a match string */
#define MVNETA_MATCH_LEN 16

static const char * const valid_args[] = {
	MVNETA_IFACE_NAME_ARG,
	NULL
};

struct mvneta_ifnames {
	const char *names[NETA_NUM_ETH_PPIO];
	int idx;
};

static int mvneta_dev_num;

static int mvneta_stats_reset(struct rte_eth_dev *dev);
static int rte_pmd_mvneta_remove(struct rte_vdev_device *vdev);


/**
 * Deinitialize packet processor.
 */
static void
mvneta_neta_deinit(void)
{
	neta_deinit();
}

/**
 * Initialize packet processor.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int
mvneta_neta_init(void)
{
	return neta_init();
}

/**
 * Callback used by rte_kvargs_process() during argument parsing.
 *
 * @param key
 *   Pointer to the parsed key (unused).
 * @param value
 *   Pointer to the parsed value.
 * @param extra_args
 *   Pointer to the extra arguments which contains address of the
 *   table of pointers to parsed interface names.
 *
 * @return
 *   Always 0.
 */
static int
mvneta_ifnames_get(const char *key __rte_unused, const char *value,
		 void *extra_args)
{
	struct mvneta_ifnames *ifnames = extra_args;

	ifnames->names[ifnames->idx++] = value;

	return 0;
}

/**
 * Ethernet device configuration.
 *
 * Prepare the driver for a given number of TX and RX queues and
 * configure RSS if supported.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int
mvneta_dev_configure(struct rte_eth_dev *dev)
{
	struct mvneta_priv *priv = dev->data->dev_private;
	struct neta_ppio_params *ppio_params;

	if (dev->data->dev_conf.rxmode.mq_mode != RTE_ETH_MQ_RX_NONE) {
		MVNETA_LOG(INFO, "Unsupported RSS and rx multi queue mode %d",
			dev->data->dev_conf.rxmode.mq_mode);
		if (dev->data->nb_rx_queues > 1)
			return -EINVAL;
	}

	if (dev->data->dev_conf.txmode.offloads & RTE_ETH_TX_OFFLOAD_MULTI_SEGS)
		priv->multiseg = 1;

	ppio_params = &priv->ppio_params;
	ppio_params->outqs_params.num_outqs = dev->data->nb_tx_queues;
	/* Default: 1 TC, no QoS supported. */
	ppio_params->inqs_params.num_tcs = 1;
	ppio_params->inqs_params.tcs_params[0].pkt_offset = MRVL_NETA_PKT_OFFS;
	priv->ppio_id = dev->data->port_id;

	return 0;
}

/**
 * DPDK callback to get information about the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure (unused).
 * @param info
 *   Info structure output buffer.
 */
static int
mvneta_dev_infos_get(struct rte_eth_dev *dev __rte_unused,
		   struct rte_eth_dev_info *info)
{
	info->speed_capa = RTE_ETH_LINK_SPEED_10M |
			   RTE_ETH_LINK_SPEED_100M |
			   RTE_ETH_LINK_SPEED_1G |
			   RTE_ETH_LINK_SPEED_2_5G;

	info->max_rx_queues = MRVL_NETA_RXQ_MAX;
	info->max_tx_queues = MRVL_NETA_TXQ_MAX;
	info->max_mac_addrs = MVNETA_MAC_ADDRS_MAX;

	info->rx_desc_lim.nb_max = MRVL_NETA_RXD_MAX;
	info->rx_desc_lim.nb_min = MRVL_NETA_RXD_MIN;
	info->rx_desc_lim.nb_align = MRVL_NETA_RXD_ALIGN;

	info->tx_desc_lim.nb_max = MRVL_NETA_TXD_MAX;
	info->tx_desc_lim.nb_min = MRVL_NETA_TXD_MIN;
	info->tx_desc_lim.nb_align = MRVL_NETA_TXD_ALIGN;

	info->rx_offload_capa = MVNETA_RX_OFFLOADS;
	info->rx_queue_offload_capa = MVNETA_RX_OFFLOADS;

	info->tx_offload_capa =  MVNETA_TX_OFFLOADS;
	info->tx_queue_offload_capa =  MVNETA_TX_OFFLOADS;

	/* By default packets are dropped if no descriptors are available */
	info->default_rxconf.rx_drop_en = 1;
	/* Deferred tx queue start is not supported */
	info->default_txconf.tx_deferred_start = 0;
	info->default_txconf.offloads = 0;

	info->max_rx_pktlen = MVNETA_PKT_SIZE_MAX;

	return 0;
}

/**
 * Return supported packet types.
 *
 * @param dev
 *   Pointer to Ethernet device structure (unused).
 *
 * @return
 *   Const pointer to the table with supported packet types.
 */
static const uint32_t *
mvneta_dev_supported_ptypes_get(struct rte_eth_dev *dev __rte_unused)
{
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L2_ETHER_VLAN,
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_L3_IPV6,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP
	};

	return ptypes;
}

/**
 * DPDK callback to change the MTU.
 *
 * Setting the MTU affects hardware MRU (packets larger than the MRU
 * will be dropped).
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mtu
 *   New MTU.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int
mvneta_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct mvneta_priv *priv = dev->data->dev_private;
	uint16_t mbuf_data_size = 0; /* SW buffer size */
	uint16_t mru;
	int ret;

	mru = MRVL_NETA_MTU_TO_MRU(mtu);
	/*
	 * min_rx_buf_size is equal to mbuf data size
	 * if pmd didn't set it differently
	 */
	mbuf_data_size = dev->data->min_rx_buf_size - RTE_PKTMBUF_HEADROOM;
	/* Prevent PMD from:
	 * - setting mru greater than the mbuf size resulting in
	 * hw and sw buffer size mismatch
	 * - setting mtu that requires the support of scattered packets
	 * when this feature has not been enabled/supported so far.
	 */
	if (!dev->data->scattered_rx &&
	    (mru + MRVL_NETA_PKT_OFFS > mbuf_data_size)) {
		mru = mbuf_data_size - MRVL_NETA_PKT_OFFS;
		mtu = MRVL_NETA_MRU_TO_MTU(mru);
		MVNETA_LOG(WARNING, "MTU too big, max MTU possible limited by"
			" current mbuf size: %u. Set MTU to %u, MRU to %u",
			mbuf_data_size, mtu, mru);
	}

	if (mtu < RTE_ETHER_MIN_MTU || mru > MVNETA_PKT_SIZE_MAX) {
		MVNETA_LOG(ERR, "Invalid MTU [%u] or MRU [%u]", mtu, mru);
		return -EINVAL;
	}

	if (!priv->ppio)
		/* It is OK. New MTU will be set later on mvneta_dev_start */
		return 0;

	ret = neta_ppio_set_mru(priv->ppio, mru);
	if (ret) {
		MVNETA_LOG(ERR, "Failed to change MRU");
		return ret;
	}

	ret = neta_ppio_set_mtu(priv->ppio, mtu);
	if (ret) {
		MVNETA_LOG(ERR, "Failed to change MTU");
		return ret;
	}
	MVNETA_LOG(INFO, "MTU changed to %u, MRU = %u", mtu, mru);

	return 0;
}

/**
 * DPDK callback to bring the link up.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int
mvneta_dev_set_link_up(struct rte_eth_dev *dev)
{
	struct mvneta_priv *priv = dev->data->dev_private;

	if (!priv->ppio)
		return 0;

	return neta_ppio_enable(priv->ppio);
}

/**
 * DPDK callback to bring the link down.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int
mvneta_dev_set_link_down(struct rte_eth_dev *dev)
{
	struct mvneta_priv *priv = dev->data->dev_private;

	if (!priv->ppio)
		return 0;

	return neta_ppio_disable(priv->ppio);
}

/**
 * DPDK callback to start the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
static int
mvneta_dev_start(struct rte_eth_dev *dev)
{
	struct mvneta_priv *priv = dev->data->dev_private;
	char match[MVNETA_MATCH_LEN];
	int ret = 0, i;

	if (priv->ppio)
		return mvneta_dev_set_link_up(dev);

	strlcpy(match, dev->data->name, sizeof(match));
	priv->ppio_params.match = match;
	priv->ppio_params.inqs_params.mtu = dev->data->mtu;

	ret = neta_ppio_init(&priv->ppio_params, &priv->ppio);
	if (ret) {
		MVNETA_LOG(ERR, "Failed to init ppio");
		return ret;
	}
	priv->ppio_id = priv->ppio->port_id;

	mvneta_stats_reset(dev);

	/*
	 * In case there are some stale uc/mc mac addresses flush them
	 * here. It cannot be done during mvneta_dev_close() as port information
	 * is already gone at that point (due to neta_ppio_deinit() in
	 * mvneta_dev_stop()).
	 */
	if (!priv->uc_mc_flushed) {
		ret = neta_ppio_flush_mac_addrs(priv->ppio, 0, 1);
		if (ret) {
			MVNETA_LOG(ERR,
				"Failed to flush uc/mc filter list");
			goto out;
		}
		priv->uc_mc_flushed = 1;
	}

	ret = mvneta_alloc_rx_bufs(dev);
	if (ret)
		goto out;

	ret = mvneta_mtu_set(dev, dev->data->mtu);
	if (ret) {
		MVNETA_LOG(ERR, "Failed to set MTU %d", dev->data->mtu);
		goto out;
	}

	ret = mvneta_dev_set_link_up(dev);
	if (ret) {
		MVNETA_LOG(ERR, "Failed to set link up");
		goto out;
	}

	/* start tx queues */
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;

	mvneta_set_tx_function(dev);

	return 0;

out:
	MVNETA_LOG(ERR, "Failed to start device");
	neta_ppio_deinit(priv->ppio);
	return ret;
}

/**
 * DPDK callback to stop the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static int
mvneta_dev_stop(struct rte_eth_dev *dev)
{
	struct mvneta_priv *priv = dev->data->dev_private;

	dev->data->dev_started = 0;

	if (!priv->ppio)
		return 0;

	mvneta_dev_set_link_down(dev);
	mvneta_flush_queues(dev);
	neta_ppio_deinit(priv->ppio);

	priv->ppio = NULL;

	return 0;
}

/**
 * DPDK callback to close the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static int
mvneta_dev_close(struct rte_eth_dev *dev)
{
	struct mvneta_priv *priv = dev->data->dev_private;
	int i, ret = 0;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (priv->ppio)
		ret = mvneta_dev_stop(dev);

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		mvneta_rx_queue_release(dev, i);
		dev->data->rx_queues[i] = NULL;
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		mvneta_tx_queue_release(dev, i);
		dev->data->tx_queues[i] = NULL;
	}

	mvneta_dev_num--;

	if (mvneta_dev_num == 0) {
		MVNETA_LOG(INFO, "Perform MUSDK deinit");
		mvneta_neta_deinit();
		rte_mvep_deinit(MVEP_MOD_T_NETA);
	}

	return ret;
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
 *   0 on success, negative error value otherwise.
 */
static int
mvneta_link_update(struct rte_eth_dev *dev, int wait_to_complete __rte_unused)
{
	/*
	 * TODO
	 * once MUSDK provides necessary API use it here
	 */
	struct mvneta_priv *priv = dev->data->dev_private;
	struct ethtool_cmd edata;
	struct ifreq req;
	int ret, fd, link_up;

	if (!priv->ppio)
		return -EPERM;

	edata.cmd = ETHTOOL_GSET;

	strcpy(req.ifr_name, dev->data->name);
	req.ifr_data = (void *)&edata;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1)
		return -EFAULT;
	ret = ioctl(fd, SIOCETHTOOL, &req);
	if (ret == -1) {
		close(fd);
		return -EFAULT;
	}

	close(fd);

	switch (ethtool_cmd_speed(&edata)) {
	case SPEED_10:
		dev->data->dev_link.link_speed = RTE_ETH_SPEED_NUM_10M;
		break;
	case SPEED_100:
		dev->data->dev_link.link_speed = RTE_ETH_SPEED_NUM_100M;
		break;
	case SPEED_1000:
		dev->data->dev_link.link_speed = RTE_ETH_SPEED_NUM_1G;
		break;
	case SPEED_2500:
		dev->data->dev_link.link_speed = RTE_ETH_SPEED_NUM_2_5G;
		break;
	default:
		dev->data->dev_link.link_speed = RTE_ETH_SPEED_NUM_NONE;
	}

	dev->data->dev_link.link_duplex = edata.duplex ? RTE_ETH_LINK_FULL_DUPLEX :
							 RTE_ETH_LINK_HALF_DUPLEX;
	dev->data->dev_link.link_autoneg = edata.autoneg ? RTE_ETH_LINK_AUTONEG :
							   RTE_ETH_LINK_FIXED;

	neta_ppio_get_link_state(priv->ppio, &link_up);
	dev->data->dev_link.link_status = link_up ? RTE_ETH_LINK_UP : RTE_ETH_LINK_DOWN;

	return 0;
}

/**
 * DPDK callback to enable promiscuous mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   always 0
 */
static int
mvneta_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct mvneta_priv *priv = dev->data->dev_private;
	int ret, en;

	if (!priv->ppio)
		return 0;

	neta_ppio_get_promisc(priv->ppio, &en);
	if (en) {
		MVNETA_LOG(INFO, "Promiscuous already enabled");
		return 0;
	}

	ret = neta_ppio_set_promisc(priv->ppio, 1);
	if (ret)
		MVNETA_LOG(ERR, "Failed to enable promiscuous mode");

	return 0;
}

/**
 * DPDK callback to disable allmulticast mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   always 0
 */
static int
mvneta_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct mvneta_priv *priv = dev->data->dev_private;
	int ret, en;

	if (!priv->ppio)
		return 0;

	neta_ppio_get_promisc(priv->ppio, &en);
	if (!en) {
		MVNETA_LOG(INFO, "Promiscuous already disabled");
		return 0;
	}

	ret = neta_ppio_set_promisc(priv->ppio, 0);
	if (ret)
		MVNETA_LOG(ERR, "Failed to disable promiscuous mode");

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
static void
mvneta_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	struct mvneta_priv *priv = dev->data->dev_private;
	char buf[RTE_ETHER_ADDR_FMT_SIZE];
	int ret;

	if (!priv->ppio)
		return;

	ret = neta_ppio_remove_mac_addr(priv->ppio,
				       dev->data->mac_addrs[index].addr_bytes);
	if (ret) {
		rte_ether_format_addr(buf, sizeof(buf),
				  &dev->data->mac_addrs[index]);
		MVNETA_LOG(ERR, "Failed to remove mac %s", buf);
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
 *   VMDq pool index to associate address with (unused).
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int
mvneta_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr,
		  uint32_t index, uint32_t vmdq __rte_unused)
{
	struct mvneta_priv *priv = dev->data->dev_private;
	char buf[RTE_ETHER_ADDR_FMT_SIZE];
	int ret;

	if (index == 0)
		/* For setting index 0, mrvl_mac_addr_set() should be used.*/
		return -1;

	if (!priv->ppio)
		return 0;

	ret = neta_ppio_add_mac_addr(priv->ppio, mac_addr->addr_bytes);
	if (ret) {
		rte_ether_format_addr(buf, sizeof(buf), mac_addr);
		MVNETA_LOG(ERR, "Failed to add mac %s", buf);
		return -1;
	}

	return 0;
}

/**
 * DPDK callback to set the primary MAC address.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mac_addr
 *   MAC address to register.
 */
static int
mvneta_mac_addr_set(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr)
{
	struct mvneta_priv *priv = dev->data->dev_private;
	int ret;

	if (!priv->ppio)
		return -EINVAL;

	ret = neta_ppio_set_mac_addr(priv->ppio, mac_addr->addr_bytes);
	if (ret) {
		char buf[RTE_ETHER_ADDR_FMT_SIZE];
		rte_ether_format_addr(buf, sizeof(buf), mac_addr);
		MVNETA_LOG(ERR, "Failed to set mac to %s", buf);
	}
	return 0;
}

/**
 * DPDK callback to get device statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param stats
 *   Stats structure output buffer.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int
mvneta_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct mvneta_priv *priv = dev->data->dev_private;
	struct neta_ppio_statistics ppio_stats;
	unsigned int ret;

	if (!priv->ppio)
		return -EPERM;

	ret = neta_ppio_get_statistics(priv->ppio, &ppio_stats);
	if (unlikely(ret)) {
		MVNETA_LOG(ERR, "Failed to update port statistics");
		return ret;
	}

	stats->ipackets += ppio_stats.rx_packets +
			ppio_stats.rx_broadcast_packets +
			ppio_stats.rx_multicast_packets -
			priv->prev_stats.ipackets;
	stats->opackets += ppio_stats.tx_packets +
			ppio_stats.tx_broadcast_packets +
			ppio_stats.tx_multicast_packets -
			priv->prev_stats.opackets;
	stats->ibytes += ppio_stats.rx_bytes - priv->prev_stats.ibytes;
	stats->obytes += ppio_stats.tx_bytes - priv->prev_stats.obytes;
	stats->imissed += ppio_stats.rx_discard +
			  ppio_stats.rx_overrun -
			  priv->prev_stats.imissed;
	stats->ierrors = ppio_stats.rx_packets_err -
			priv->prev_stats.ierrors;
	stats->oerrors = ppio_stats.tx_errors - priv->prev_stats.oerrors;

	return 0;
}

/**
 * DPDK callback to clear device statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int
mvneta_stats_reset(struct rte_eth_dev *dev)
{
	struct mvneta_priv *priv = dev->data->dev_private;
	unsigned int ret;

	if (!priv->ppio)
		return 0;

	ret = mvneta_stats_get(dev, &priv->prev_stats);
	if (unlikely(ret))
		MVNETA_LOG(ERR, "Failed to reset port statistics");

	return ret;
}


static const struct eth_dev_ops mvneta_ops = {
	.dev_configure = mvneta_dev_configure,
	.dev_start = mvneta_dev_start,
	.dev_stop = mvneta_dev_stop,
	.dev_set_link_up = mvneta_dev_set_link_up,
	.dev_set_link_down = mvneta_dev_set_link_down,
	.dev_close = mvneta_dev_close,
	.link_update = mvneta_link_update,
	.promiscuous_enable = mvneta_promiscuous_enable,
	.promiscuous_disable = mvneta_promiscuous_disable,
	.mac_addr_remove = mvneta_mac_addr_remove,
	.mac_addr_add = mvneta_mac_addr_add,
	.mac_addr_set = mvneta_mac_addr_set,
	.mtu_set = mvneta_mtu_set,
	.stats_get = mvneta_stats_get,
	.stats_reset = mvneta_stats_reset,
	.dev_infos_get = mvneta_dev_infos_get,
	.dev_supported_ptypes_get = mvneta_dev_supported_ptypes_get,
	.rxq_info_get = mvneta_rxq_info_get,
	.txq_info_get = mvneta_txq_info_get,
	.rx_queue_setup = mvneta_rx_queue_setup,
	.rx_queue_release = mvneta_rx_queue_release,
	.tx_queue_setup = mvneta_tx_queue_setup,
	.tx_queue_release = mvneta_tx_queue_release,
};

/**
 * Create device representing Ethernet port.
 *
 * @param name
 *   Pointer to the port's name.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int
mvneta_eth_dev_create(struct rte_vdev_device *vdev, const char *name)
{
	int ret, fd = socket(AF_INET, SOCK_DGRAM, 0);
	struct rte_eth_dev *eth_dev;
	struct mvneta_priv *priv;
	struct ifreq req;

	eth_dev = rte_eth_dev_allocate(name);
	if (!eth_dev)
		return -ENOMEM;

	priv = rte_zmalloc_socket(name, sizeof(*priv), 0, rte_socket_id());
	if (!priv) {
		ret = -ENOMEM;
		goto out_free;
	}
	eth_dev->data->dev_private = priv;

	eth_dev->data->mac_addrs =
		rte_zmalloc("mac_addrs",
			    RTE_ETHER_ADDR_LEN * MVNETA_MAC_ADDRS_MAX, 0);
	if (!eth_dev->data->mac_addrs) {
		MVNETA_LOG(ERR, "Failed to allocate space for eth addrs");
		ret = -ENOMEM;
		goto out_free;
	}

	memset(&req, 0, sizeof(req));
	strcpy(req.ifr_name, name);
	ret = ioctl(fd, SIOCGIFHWADDR, &req);
	if (ret)
		goto out_free;

	memcpy(eth_dev->data->mac_addrs[0].addr_bytes,
	       req.ifr_addr.sa_data, RTE_ETHER_ADDR_LEN);

	eth_dev->device = &vdev->device;
	eth_dev->rx_pkt_burst = mvneta_rx_pkt_burst;
	mvneta_set_tx_function(eth_dev);
	eth_dev->dev_ops = &mvneta_ops;

	rte_eth_dev_probing_finish(eth_dev);
	return 0;
out_free:
	rte_eth_dev_release_port(eth_dev);

	return ret;
}

/**
 * Cleanup previously created device representing Ethernet port.
 *
 * @param eth_dev
 *   Pointer to the corresponding rte_eth_dev structure.
 */
static void
mvneta_eth_dev_destroy(struct rte_eth_dev *eth_dev)
{
	rte_eth_dev_release_port(eth_dev);
}

/**
 * Cleanup previously created device representing Ethernet port.
 *
 * @param name
 *   Pointer to the port name.
 */
static void
mvneta_eth_dev_destroy_name(const char *name)
{
	struct rte_eth_dev *eth_dev;

	eth_dev = rte_eth_dev_allocated(name);
	if (!eth_dev)
		return;

	mvneta_eth_dev_destroy(eth_dev);
}

/**
 * DPDK callback to register the virtual device.
 *
 * @param vdev
 *   Pointer to the virtual device.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int
rte_pmd_mvneta_probe(struct rte_vdev_device *vdev)
{
	struct rte_kvargs *kvlist;
	struct mvneta_ifnames ifnames;
	int ret = -EINVAL;
	uint32_t i, ifnum;
	const char *params;

	params = rte_vdev_device_args(vdev);
	if (!params)
		return -EINVAL;

	kvlist = rte_kvargs_parse(params, valid_args);
	if (!kvlist)
		return -EINVAL;

	ifnum = rte_kvargs_count(kvlist, MVNETA_IFACE_NAME_ARG);
	if (ifnum > RTE_DIM(ifnames.names))
		goto out_free_kvlist;

	ifnames.idx = 0;
	rte_kvargs_process(kvlist, MVNETA_IFACE_NAME_ARG,
			   mvneta_ifnames_get, &ifnames);

	/*
	 * The below system initialization should be done only once,
	 * on the first provided configuration file
	 */
	if (mvneta_dev_num)
		goto init_devices;

	MVNETA_LOG(INFO, "Perform MUSDK initializations");

	ret = rte_mvep_init(MVEP_MOD_T_NETA, kvlist);
	if (ret)
		goto out_free_kvlist;

	ret = mvneta_neta_init();
	if (ret) {
		MVNETA_LOG(ERR, "Failed to init NETA!");
		rte_mvep_deinit(MVEP_MOD_T_NETA);
		goto out_free_kvlist;
	}

init_devices:
	for (i = 0; i < ifnum; i++) {
		MVNETA_LOG(INFO, "Creating %s", ifnames.names[i]);
		ret = mvneta_eth_dev_create(vdev, ifnames.names[i]);
		if (ret)
			goto out_cleanup;

		mvneta_dev_num++;
	}

	rte_kvargs_free(kvlist);

	return 0;
out_cleanup:
	rte_pmd_mvneta_remove(vdev);

out_free_kvlist:
	rte_kvargs_free(kvlist);

	return ret;
}

/**
 * DPDK callback to remove virtual device.
 *
 * @param vdev
 *   Pointer to the removed virtual device.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int
rte_pmd_mvneta_remove(struct rte_vdev_device *vdev)
{
	uint16_t port_id;
	int ret = 0;

	RTE_ETH_FOREACH_DEV(port_id) {
		if (rte_eth_devices[port_id].device != &vdev->device)
			continue;
		ret |= rte_eth_dev_close(port_id);
	}

	return ret == 0 ? 0 : -EIO;
}

static struct rte_vdev_driver pmd_mvneta_drv = {
	.probe = rte_pmd_mvneta_probe,
	.remove = rte_pmd_mvneta_remove,
};

RTE_PMD_REGISTER_VDEV(net_mvneta, pmd_mvneta_drv);
RTE_PMD_REGISTER_PARAM_STRING(net_mvneta, "iface=<ifc>");
RTE_LOG_REGISTER_DEFAULT(mvneta_logtype, NOTICE);
