/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Marvell International Ltd.
 * Copyright(c) 2018 Semihalf.
 * All rights reserved.
 */

#include <rte_ethdev_driver.h>
#include <rte_kvargs.h>
#include <rte_bus_vdev.h>

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

#define MVNETA_RX_OFFLOADS (DEV_RX_OFFLOAD_JUMBO_FRAME | \
			  DEV_RX_OFFLOAD_CHECKSUM)

/** Port Tx offloads capabilities */
#define MVNETA_TX_OFFLOADS (DEV_TX_OFFLOAD_IPV4_CKSUM | \
			  DEV_TX_OFFLOAD_UDP_CKSUM | \
			  DEV_TX_OFFLOAD_TCP_CKSUM | \
			  DEV_TX_OFFLOAD_MULTI_SEGS)

#define MVNETA_PKT_SIZE_MAX (16382 - MV_MH_SIZE) /* 9700B */
#define MVNETA_DEFAULT_MTU 1500

#define MVNETA_MAC_ADDRS_MAX 256 /*16 UC, 256 IP, 256 MC/BC */
/** Maximum length of a match string */
#define MVNETA_MATCH_LEN 16

int mvneta_logtype;

static const char * const valid_args[] = {
	MVNETA_IFACE_NAME_ARG,
	NULL
};

struct mvneta_ifnames {
	const char *names[NETA_NUM_ETH_PPIO];
	int idx;
};

static int mvneta_dev_num;

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

	if (dev->data->dev_conf.rxmode.mq_mode != ETH_MQ_RX_NONE) {
		MVNETA_LOG(INFO, "Unsupported RSS and rx multi queue mode %d",
			dev->data->dev_conf.rxmode.mq_mode);
		if (dev->data->nb_rx_queues > 1)
			return -EINVAL;
	}

	if (dev->data->dev_conf.rxmode.split_hdr_size) {
		MVNETA_LOG(INFO, "Split headers not supported");
		return -EINVAL;
	}

	if (dev->data->dev_conf.rxmode.offloads & DEV_RX_OFFLOAD_JUMBO_FRAME)
		dev->data->mtu = dev->data->dev_conf.rxmode.max_rx_pkt_len -
				 MRVL_NETA_ETH_HDRS_LEN;

	if (dev->data->dev_conf.txmode.offloads & DEV_TX_OFFLOAD_MULTI_SEGS)
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
static void
mvneta_dev_infos_get(struct rte_eth_dev *dev __rte_unused,
		   struct rte_eth_dev_info *info)
{
	info->speed_capa = ETH_LINK_SPEED_10M |
			   ETH_LINK_SPEED_100M |
			   ETH_LINK_SPEED_1G |
			   ETH_LINK_SPEED_2_5G;

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

	snprintf(match, sizeof(match), "%s", dev->data->name);
	priv->ppio_params.match = match;
	priv->ppio_params.inqs_params.mtu = dev->data->mtu;

	ret = neta_ppio_init(&priv->ppio_params, &priv->ppio);
	if (ret) {
		MVNETA_LOG(ERR, "Failed to init ppio");
		return ret;
	}
	priv->ppio_id = priv->ppio->port_id;

	/*
	 * In case there are some some stale uc/mc mac addresses flush them
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
static void
mvneta_dev_stop(struct rte_eth_dev *dev)
{
	struct mvneta_priv *priv = dev->data->dev_private;

	if (!priv->ppio)
		return;

	mvneta_dev_set_link_down(dev);
	mvneta_flush_queues(dev);
	neta_ppio_deinit(priv->ppio);

	priv->ppio = NULL;
}

/**
 * DPDK callback to close the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mvneta_dev_close(struct rte_eth_dev *dev)
{
	struct mvneta_priv *priv = dev->data->dev_private;
	int i;

	if (priv->ppio)
		mvneta_dev_stop(dev);

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		mvneta_rx_queue_release(dev->data->rx_queues[i]);
		dev->data->rx_queues[i] = NULL;
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		mvneta_tx_queue_release(dev->data->tx_queues[i]);
		dev->data->tx_queues[i] = NULL;
	}
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
mvneta_mac_addr_set(struct rte_eth_dev *dev, struct ether_addr *mac_addr)
{
	struct mvneta_priv *priv = dev->data->dev_private;
	int ret;

	if (!priv->ppio)
		return -EINVAL;

	ret = neta_ppio_set_mac_addr(priv->ppio, mac_addr->addr_bytes);
	if (ret) {
		char buf[ETHER_ADDR_FMT_SIZE];
		ether_format_addr(buf, sizeof(buf), mac_addr);
		MVNETA_LOG(ERR, "Failed to set mac to %s", buf);
	}
	return 0;
}

static const struct eth_dev_ops mvneta_ops = {
	.dev_configure = mvneta_dev_configure,
	.dev_start = mvneta_dev_start,
	.dev_stop = mvneta_dev_stop,
	.dev_set_link_up = mvneta_dev_set_link_up,
	.dev_set_link_down = mvneta_dev_set_link_down,
	.dev_close = mvneta_dev_close,
	.mac_addr_set = mvneta_mac_addr_set,
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
		goto out_free_dev;
	}

	eth_dev->data->mac_addrs =
		rte_zmalloc("mac_addrs",
			    ETHER_ADDR_LEN * MVNETA_MAC_ADDRS_MAX, 0);
	if (!eth_dev->data->mac_addrs) {
		MVNETA_LOG(ERR, "Failed to allocate space for eth addrs");
		ret = -ENOMEM;
		goto out_free_priv;
	}

	memset(&req, 0, sizeof(req));
	strcpy(req.ifr_name, name);
	ret = ioctl(fd, SIOCGIFHWADDR, &req);
	if (ret)
		goto out_free_mac;

	memcpy(eth_dev->data->mac_addrs[0].addr_bytes,
	       req.ifr_addr.sa_data, ETHER_ADDR_LEN);

	eth_dev->data->kdrv = RTE_KDRV_NONE;
	eth_dev->data->dev_private = priv;
	eth_dev->device = &vdev->device;
	eth_dev->rx_pkt_burst = mvneta_rx_pkt_burst;
	mvneta_set_tx_function(eth_dev);
	eth_dev->dev_ops = &mvneta_ops;

	rte_eth_dev_probing_finish(eth_dev);
	return 0;
out_free_mac:
	rte_free(eth_dev->data->mac_addrs);
out_free_priv:
	rte_free(priv);
out_free_dev:
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
	rte_free(eth_dev->data->dev_private);
	rte_free(eth_dev->data->mac_addrs);
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
	}
	mvneta_dev_num += ifnum;

	rte_kvargs_free(kvlist);

	return 0;
out_cleanup:
	for (; i > 0; i--)
		mvneta_eth_dev_destroy_name(ifnames.names[i]);

	if (mvneta_dev_num == 0) {
		mvneta_neta_deinit();
		rte_mvep_deinit(MVEP_MOD_T_NETA);
	}
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
	int i;
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (!name)
		return -EINVAL;

	MVNETA_LOG(INFO, "Removing %s", name);

	RTE_ETH_FOREACH_DEV(i) {
		if (rte_eth_devices[i].device != &vdev->device)
			continue;

		mvneta_eth_dev_destroy(&rte_eth_devices[i]);
		mvneta_dev_num--;
	}

	if (mvneta_dev_num == 0) {
		MVNETA_LOG(INFO, "Perform MUSDK deinit");
		mvneta_neta_deinit();
		rte_mvep_deinit(MVEP_MOD_T_NETA);
	}

	return 0;
}

static struct rte_vdev_driver pmd_mvneta_drv = {
	.probe = rte_pmd_mvneta_probe,
	.remove = rte_pmd_mvneta_remove,
};

RTE_PMD_REGISTER_VDEV(net_mvneta, pmd_mvneta_drv);
RTE_PMD_REGISTER_PARAM_STRING(net_mvneta, "iface=<ifc>");

RTE_INIT(mvneta_init_log)
{
	mvneta_logtype = rte_log_register("pmd.net.mvneta");
	if (mvneta_logtype >= 0)
		rte_log_set_level(mvneta_logtype, RTE_LOG_NOTICE);
}
