/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Cesnet
 * Copyright(c) 2019 Netcope Technologies, a.s. <info@netcope.com>
 * All rights reserved.
 */

#include <ctype.h>
#include <sys/queue.h>
#include <eal_export.h>
#include <rte_tailq.h>

#include <nfb/nfb.h>
#include <nfb/ndp.h>
#include <netcope/rxmac.h>
#include <netcope/txmac.h>

#include <ethdev_pci.h>
#include <rte_kvargs.h>

#include "nfb_stats.h"
#include "nfb_rx.h"
#include "nfb_tx.h"
#include "nfb_rxmode.h"
#include "nfb.h"

static const char * const VALID_KEYS[] = {
	NFB_ARG_PORT,
	NULL
};

struct nfb_ifc_create_params {
	struct nfb_probe_params *probe_params;
	struct nc_ifc_map_info map_info;
	struct nc_ifc_info *ifc_info;

	int basename_len;       /* Cached real length of original probe_params->name */
	/* Return value of failed nfb_eth_dev_create_for_ifc when rte_kvargs_process is used */
	int ret;
};

/* The TAILQ entries are used for cleanup of allocated resources
 * in situations, where dev_close is not called.
 */
TAILQ_HEAD(nfb_pmd_internals_head, pmd_internals);
static struct nfb_pmd_internals_head nfb_eth_dev_list =
	TAILQ_HEAD_INITIALIZER(nfb_eth_dev_list);

static int nfb_eth_dev_uninit(struct rte_eth_dev *dev);

/**
 * Default MAC addr
 */
static const struct rte_ether_addr eth_addr = {
	.addr_bytes = { 0x00, 0x11, 0x17, 0x00, 0x00, 0x00 }
};

/**
 * Open Rx and Tx MAC components associated with current ifc
 *
 * @param intl
 *   Pointer to driver internal structure
 * @param params
 *   Pointer to init parameters structure
 */
static int
nfb_nc_eth_init(struct pmd_internals *intl, struct nfb_ifc_create_params *params)
{
	int ret;
	int i, rxm, txm;
	struct nc_ifc_info *ifc = params->ifc_info;
	struct nc_ifc_map_info *mi = &params->map_info;

	ret = -ENOMEM;
	if (ifc->eth_cnt == 0)
		return 0;

	intl->rxmac = calloc(ifc->eth_cnt, sizeof(*intl->rxmac));
	if (intl->rxmac == NULL)
		goto err_alloc_rxmac;

	intl->txmac = calloc(ifc->eth_cnt, sizeof(*intl->txmac));
	if (intl->txmac == NULL)
		goto err_alloc_txmac;

	/* Some eths may not have assigned MAC nodes, hence use separate var for indexing */
	rxm = 0;
	txm = 0;
	for (i = 0; i < mi->eth_cnt; i++) {
		if (mi->eth[i].ifc != ifc->id)
			continue;

		if (rxm >= ifc->eth_cnt || txm >= ifc->eth_cnt) {
			NFB_LOG(ERR, "MAC mapping inconsistent");
			ret = -EINVAL;
			goto err_map_inconsistent;
		}

		intl->rxmac[rxm] = nc_rxmac_open(intl->nfb, mi->eth[i].node_rxmac);
		if (intl->rxmac[rxm])
			rxm++;

		intl->txmac[txm] = nc_txmac_open(intl->nfb, mi->eth[i].node_txmac);
		if (intl->txmac[txm])
			txm++;
	}

	intl->max_rxmac = rxm;
	intl->max_txmac = txm;
	return 0;

err_map_inconsistent:
	for (i = 0; i < txm; i++)
		nc_txmac_close(intl->txmac[i]);
	for (i = 0; i < rxm; i++)
		nc_rxmac_close(intl->rxmac[i]);
	free(intl->txmac);
err_alloc_txmac:
	free(intl->rxmac);
err_alloc_rxmac:
	return ret;
}

/**
 * Close all Rx+Tx MAC components
 * @param intl
 *   Pointer to internal driver structure
 */
static void
nfb_nc_eth_deinit(struct pmd_internals *intl)
{
	uint16_t i;
	for (i = 0; i < intl->max_txmac; i++)
		nc_txmac_close(intl->txmac[i]);
	for (i = 0; i < intl->max_rxmac; i++)
		nc_rxmac_close(intl->rxmac[i]);

	free(intl->txmac);
	free(intl->rxmac);
}

/**
 * DPDK callback to start the device.
 *
 * Start device by starting all configured queues.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
static int
nfb_eth_dev_start(struct rte_eth_dev *dev)
{
	int ret;
	uint16_t i;
	uint16_t nb_rx = dev->data->nb_rx_queues;
	uint16_t nb_tx = dev->data->nb_tx_queues;

	for (i = 0; i < nb_rx; i++) {
		ret = nfb_eth_rx_queue_start(dev, i);
		if (ret != 0)
			goto err_rx;
	}

	for (i = 0; i < nb_tx; i++) {
		ret = nfb_eth_tx_queue_start(dev, i);
		if (ret != 0)
			goto err_tx;
	}

	return 0;

err_tx:
	for (; i > 0; i--)
		nfb_eth_tx_queue_stop(dev, i - 1);
	i = nb_rx;
err_rx:
	for (; i > 0; i--)
		nfb_eth_rx_queue_stop(dev, i - 1);
	return ret;
}

/**
 * DPDK callback to stop the device.
 *
 * Stop device by stopping all configured queues.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static int
nfb_eth_dev_stop(struct rte_eth_dev *dev)
{
	uint16_t i;
	uint16_t nb_rx = dev->data->nb_rx_queues;
	uint16_t nb_tx = dev->data->nb_tx_queues;

	for (i = 0; i < nb_tx; i++)
		nfb_eth_tx_queue_stop(dev, i);

	for (i = 0; i < nb_rx; i++)
		nfb_eth_rx_queue_stop(dev, i);

	return 0;
}

/**
 * DPDK callback for Ethernet device configuration.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
static int
nfb_eth_dev_configure(struct rte_eth_dev *dev)
{
	int ret;
	struct rte_eth_conf *dev_conf = &dev->data->dev_conf;

	if (dev_conf->rxmode.offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP) {
		ret = rte_mbuf_dyn_rx_timestamp_register
				(&nfb_timestamp_dynfield_offset,
				&nfb_timestamp_rx_dynflag);
		if (ret != 0) {
			NFB_LOG(ERR, "Cannot register Rx timestamp field/flag %d", ret);
			ret = -ENOMEM;
			goto err_ts_register;
		}
	}

	return 0;

err_ts_register:
	nfb_eth_dev_uninit(dev);
	return ret;
}

static uint32_t
nfb_eth_get_max_mac_address_count(struct rte_eth_dev *dev)
{
	uint16_t i;
	uint32_t c;
	uint32_t ret = (uint32_t)-1;
	struct pmd_internals *internals = dev->process_private;

	/*
	 * Go through all RX MAC components in firmware and find
	 * the minimal indicated space size for MAC addresses.
	 */
	for (i = 0; i < internals->max_rxmac; i++) {
		c = nc_rxmac_mac_address_count(internals->rxmac[i]);
		ret = RTE_MIN(c, ret);
	}

	/* The driver must support at least 1 MAC address, pretend that */
	if (internals->max_rxmac == 0 || ret == 0)
		ret = 1;

	return ret;
}

/**
 * DPDK callback to get information about the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] info
 *   Info structure output buffer.
 */
static int
nfb_eth_dev_info(struct rte_eth_dev *dev,
	struct rte_eth_dev_info *dev_info)
{
	struct pmd_priv *priv = dev->data->dev_private;

	dev_info->max_mac_addrs = nfb_eth_get_max_mac_address_count(dev);

	dev_info->max_rx_pktlen = (uint32_t)-1;
	dev_info->max_rx_queues = priv->max_rx_queues;
	dev_info->max_tx_queues = priv->max_tx_queues;
	dev_info->speed_capa = RTE_ETH_LINK_SPEED_100G;
	dev_info->rx_offload_capa =
		RTE_ETH_RX_OFFLOAD_TIMESTAMP;

	return 0;
}

/**
 * DPDK callback to close the device.
 *
 * Destroy all queues and objects, free memory.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static int
nfb_eth_dev_close(struct rte_eth_dev *dev)
{
	uint16_t i;
	uint16_t nb_rx = dev->data->nb_rx_queues;
	uint16_t nb_tx = dev->data->nb_tx_queues;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		for (i = 0; i < nb_rx; i++) {
			if (dev->data->rx_queues[i]) {
				nfb_eth_rx_queue_release(dev, i);
				dev->data->rx_queues[i] = NULL;
			}
		}
		for (i = 0; i < nb_tx; i++) {
			if (dev->data->tx_queues[i]) {
				nfb_eth_tx_queue_release(dev, i);
				dev->data->tx_queues[i] = NULL;
			}
		}
	}

	nfb_eth_dev_uninit(dev);

	return 0;
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
 *   0 on success, a negative errno value otherwise.
 */
static int
nfb_eth_link_update(struct rte_eth_dev *dev,
	int wait_to_complete __rte_unused)
{
	uint16_t i;
	struct nc_rxmac_status status;
	struct rte_eth_link link;
	memset(&link, 0, sizeof(link));

	struct pmd_internals *internals = dev->process_private;

	status.speed = MAC_SPEED_UNKNOWN;

	link.link_speed = RTE_ETH_SPEED_NUM_NONE;
	link.link_status = RTE_ETH_LINK_DOWN;
	link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	link.link_autoneg = RTE_ETH_LINK_SPEED_FIXED;

	if (internals->max_rxmac) {
		nc_rxmac_read_status(internals->rxmac[0], &status);

		switch (status.speed) {
		case MAC_SPEED_10G:
			link.link_speed = RTE_ETH_SPEED_NUM_10G;
			break;
		case MAC_SPEED_40G:
			link.link_speed = RTE_ETH_SPEED_NUM_40G;
			break;
		case MAC_SPEED_100G:
			link.link_speed = RTE_ETH_SPEED_NUM_100G;
			break;
		default:
			link.link_speed = RTE_ETH_SPEED_NUM_NONE;
			break;
		}
	}

	for (i = 0; i < internals->max_rxmac; ++i) {
		nc_rxmac_read_status(internals->rxmac[i], &status);

		if (status.enabled && status.link_up) {
			link.link_status = RTE_ETH_LINK_UP;
			break;
		}
	}

	rte_eth_linkstatus_set(dev, &link);

	return 0;
}

/**
 * DPDK callback to bring the link UP.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
static int
nfb_eth_dev_set_link_up(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = (struct pmd_internals *)
		dev->process_private;

	uint16_t i;
	for (i = 0; i < internals->max_rxmac; ++i)
		nc_rxmac_enable(internals->rxmac[i]);

	for (i = 0; i < internals->max_txmac; ++i)
		nc_txmac_enable(internals->txmac[i]);

	return 0;
}

/**
 * DPDK callback to bring the link DOWN.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
static int
nfb_eth_dev_set_link_down(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = (struct pmd_internals *)
		dev->process_private;

	uint16_t i;
	for (i = 0; i < internals->max_rxmac; ++i)
		nc_rxmac_disable(internals->rxmac[i]);

	for (i = 0; i < internals->max_txmac; ++i)
		nc_txmac_disable(internals->txmac[i]);

	return 0;
}

static uint64_t
nfb_eth_mac_addr_conv(struct rte_ether_addr *mac_addr)
{
	int i;
	uint64_t res = 0;
	for (i = 0; i < RTE_ETHER_ADDR_LEN; i++) {
		res <<= 8;
		res |= mac_addr->addr_bytes[i] & 0xFF;
	}
	return res;
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
 *   0 on success, a negative errno value otherwise.
 */
static int
nfb_eth_mac_addr_set(struct rte_eth_dev *dev,
	struct rte_ether_addr *mac_addr)
{
	unsigned int i;
	uint64_t mac;
	struct pmd_internals *internals = (struct pmd_internals *)
		dev->process_private;

	mac = nfb_eth_mac_addr_conv(mac_addr);
	/* Until no real multi-port support, configure all RX MACs the same */
	for (i = 0; i < internals->max_rxmac; ++i)
		nc_rxmac_set_mac(internals->rxmac[i], 0, mac, 1);

	return 0;
}

static int
nfb_eth_mac_addr_add(struct rte_eth_dev *dev,
	struct rte_ether_addr *mac_addr, uint32_t index, uint32_t pool __rte_unused)
{
	unsigned int i;
	uint64_t mac;
	struct pmd_internals *internals = (struct pmd_internals *)
		dev->process_private;

	mac = nfb_eth_mac_addr_conv(mac_addr);
	for (i = 0; i < internals->max_rxmac; ++i)
		nc_rxmac_set_mac(internals->rxmac[i], index, mac, 1);

	return 0;
}

static void
nfb_eth_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	unsigned int i;
	struct pmd_internals *internals = (struct pmd_internals *)
		dev->process_private;

	for (i = 0; i < internals->max_rxmac; ++i)
		nc_rxmac_set_mac(internals->rxmac[i], index, 0, 0);
}

static const struct eth_dev_ops ops = {
	.dev_start = nfb_eth_dev_start,
	.dev_stop = nfb_eth_dev_stop,
	.dev_set_link_up = nfb_eth_dev_set_link_up,
	.dev_set_link_down = nfb_eth_dev_set_link_down,
	.dev_close = nfb_eth_dev_close,
	.dev_configure = nfb_eth_dev_configure,
	.dev_infos_get = nfb_eth_dev_info,
	.promiscuous_enable = nfb_eth_promiscuous_enable,
	.promiscuous_disable = nfb_eth_promiscuous_disable,
	.allmulticast_enable = nfb_eth_allmulticast_enable,
	.allmulticast_disable = nfb_eth_allmulticast_disable,
	.rx_queue_start = nfb_eth_rx_queue_start,
	.rx_queue_stop = nfb_eth_rx_queue_stop,
	.tx_queue_start = nfb_eth_tx_queue_start,
	.tx_queue_stop = nfb_eth_tx_queue_stop,
	.rx_queue_setup = nfb_eth_rx_queue_setup,
	.tx_queue_setup = nfb_eth_tx_queue_setup,
	.rx_queue_release = nfb_eth_rx_queue_release,
	.tx_queue_release = nfb_eth_tx_queue_release,
	.link_update = nfb_eth_link_update,
	.stats_get = nfb_eth_stats_get,
	.stats_reset = nfb_eth_stats_reset,
	.mac_addr_set = nfb_eth_mac_addr_set,
	.mac_addr_add = nfb_eth_mac_addr_add,
	.mac_addr_remove = nfb_eth_mac_addr_remove,
};

/**
 * DPDK callback to initialize an ethernet device
 *
 * @param dev
 *   Pointer to ethernet device structure
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
static int
nfb_eth_dev_init(struct rte_eth_dev *dev, void *init_data)
{
	int i;
	int cnt;
	int ret;
	uint32_t mac_count;
	struct rte_eth_dev_data *data = dev->data;
	struct pmd_internals *internals;
	struct nfb_ifc_create_params *params = init_data;
	struct nc_ifc_info *ifc = params->ifc_info;
	struct nc_ifc_map_info *mi = &params->map_info;
	struct pmd_priv *priv = data->dev_private;
	struct rte_ether_addr eth_addr_init;
	uint16_t max_rx_queues, max_tx_queues;

	internals = rte_zmalloc_socket("nfb_internals",
			sizeof(struct pmd_internals), RTE_CACHE_LINE_SIZE,
			dev->device->numa_node);
	if (internals == NULL) {
		ret = -ENOMEM;
		goto err_alloc_internals;
	}

	dev->process_private = internals;

	/* Open device handle */
	internals->nfb = nfb_open(params->probe_params->path);
	if (internals->nfb == NULL) {
		NFB_LOG(ERR, "nfb_open(): failed to open %s", params->probe_params->path);
		ret = -EINVAL;
		goto err_nfb_open;
	}

	/* Get number of available DMA RX and TX queues */
	max_rx_queues = ifc->rxq_cnt;
	max_tx_queues = ifc->txq_cnt;

	ret = nfb_nc_eth_init(internals, params);
	if (ret)
		goto err_nc_eth_init;

	/* Set rx, tx burst functions */
	dev->rx_pkt_burst = nfb_eth_ndp_rx;
	dev->tx_pkt_burst = nfb_eth_ndp_tx;

	/* Set function callbacks for Ethernet API */
	dev->dev_ops = &ops;

	/* Get link state */
	nfb_eth_link_update(dev, 0);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		priv->max_rx_queues = max_rx_queues;
		priv->max_tx_queues = max_tx_queues;

		priv->queue_map_rx = rte_calloc("NFB queue map", max_rx_queues + max_tx_queues,
				sizeof(*priv->queue_map_rx), 0);
		if (priv->queue_map_rx == NULL) {
			ret = -ENOMEM;
			goto err_alloc_queue_map;
		}
		priv->queue_map_tx = priv->queue_map_rx + max_rx_queues;

		/* Use queue mapping provided by libnfb */
		cnt = 0;
		for (i = 0; i < mi->rxq_cnt; i++) {
			if (mi->rxq[i].ifc == ifc->id) {
				if (cnt >= max_rx_queues) {
					NFB_LOG(ERR, "RX queue mapping inconsistent");
					ret = -EINVAL;
					goto err_queue_map;
				}
				priv->queue_map_rx[cnt++] = mi->rxq[i].id;
			}
		}

		cnt = 0;
		for (i = 0; i < mi->txq_cnt; i++) {
			if (mi->txq[i].ifc == ifc->id) {
				if (cnt >= max_tx_queues) {
					NFB_LOG(ERR, "TX queue mapping inconsistent");
					ret = -EINVAL;
					goto err_queue_map;
				}
				priv->queue_map_tx[cnt++] = mi->txq[i].id;
			}
		}

		/* Allocate space for MAC addresses */
		mac_count = nfb_eth_get_max_mac_address_count(dev);
		data->mac_addrs = rte_zmalloc(data->name,
			sizeof(struct rte_ether_addr) * mac_count, RTE_CACHE_LINE_SIZE);
		if (data->mac_addrs == NULL) {
			NFB_LOG(ERR, "Could not alloc space for MAC address");
			ret = -ENOMEM;
			goto err_malloc_mac_addrs;
		}

		rte_eth_random_addr(eth_addr_init.addr_bytes);
		eth_addr_init.addr_bytes[0] = eth_addr.addr_bytes[0];
		eth_addr_init.addr_bytes[1] = eth_addr.addr_bytes[1];
		eth_addr_init.addr_bytes[2] = eth_addr.addr_bytes[2];

		nfb_eth_mac_addr_set(dev, &eth_addr_init);
		rte_ether_addr_copy(&eth_addr_init, &data->mac_addrs[0]);

		data->promiscuous = nfb_eth_promiscuous_get(dev);
		data->all_multicast = nfb_eth_allmulticast_get(dev);

		data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;
		priv->ready = true;
	} else {
		if (!priv->ready) {
			ret = -EBADFD;
			goto err_secondary_not_ready;
		}
	}

	return 0;

err_malloc_mac_addrs:
err_queue_map:
	rte_free(priv->queue_map_rx);
err_alloc_queue_map:
err_secondary_not_ready:
	nfb_nc_eth_deinit(internals);
err_nc_eth_init:
	nfb_close(internals->nfb);
err_nfb_open:
	rte_free(internals);
err_alloc_internals:
	return ret;
}

/**
 * DPDK callback to uninitialize an ethernet device
 *
 * @param dev
 *   Pointer to ethernet device structure
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
static int
nfb_eth_dev_uninit(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = dev->process_private;
	struct pmd_priv *priv = dev->data->dev_private;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(priv->queue_map_rx);

	TAILQ_REMOVE(&nfb_eth_dev_list, internals, eth_dev_list);

	nfb_nc_eth_deinit(internals);
	nfb_close(internals->nfb);

	rte_free(internals);

	return 0;
}

static int
nfb_eth_dev_create_for_ifc(struct nfb_ifc_create_params *cp)
{
	int ret;
	struct nc_ifc_info *ifc;
	struct nfb_probe_params *pp;
	struct rte_eth_dev *eth_dev;
	struct pmd_internals *p;

	ifc = cp->ifc_info;
	pp = cp->probe_params;

	/* Skip interfaces which doesn't belong to the probed PCI device */
	if ((pp->ep_index != -1 && ifc->ep != pp->ep_index) ||
			(ifc->flags & NC_IFC_INFO_FLAG_ACTIVE) == 0)
		return 0;

	ret = snprintf(pp->name + cp->basename_len, sizeof(pp->name) - cp->basename_len,
			"_eth%d", ifc->id);
	if (ret < 0 || ret >= (signed int)sizeof(pp->name) - cp->basename_len)
		return -EINVAL;

	ret = rte_eth_dev_create(pp->device, pp->name,
			sizeof(struct pmd_priv),
			pp->specific_init, pp->specific_device,
			nfb_eth_dev_init, cp);
	if (ret)
		goto out;

	eth_dev = rte_eth_dev_get_by_name(pp->name);
	if (eth_dev == NULL) {
		NFB_LOG(ERR, "Failed to get created device %s", pp->name);
		ret = -ENODEV;
		goto out;
	}
	p = eth_dev->process_private;
	p->eth_dev = eth_dev;
	TAILQ_INSERT_TAIL(&nfb_eth_dev_list, p, eth_dev_list);

out:
	/* return to original name (just for clarity) */
	pp->name[cp->basename_len] = '\0';
	return ret;
}

static int nfb_eth_dev_create_for_ifc_by_port(const char *key __rte_unused,
		const char *value, void *opaque)
{
	int ret = -EINVAL;
	char *end;
	unsigned long port;
	struct nfb_ifc_create_params *ifc_params = opaque;

	if (value == NULL || strlen(value) == 0 || !isdigit(*value))
		goto out;

	errno = 0;
	port = strtoul(value, &end, 10);
	if (errno != 0 || *end != '\0')
		goto out;

	if (port >= LONG_MAX || port >= (unsigned long)ifc_params->map_info.ifc_cnt)
		goto out;

	ifc_params->ifc_info = &ifc_params->map_info.ifc[port];
	ret = nfb_eth_dev_create_for_ifc(ifc_params);

out:
	ifc_params->ret = ret;
	return ret;
}

int
nfb_eth_common_probe(struct nfb_probe_params *params)
{
	int i;
	int ret;

	struct nfb_device *nfb_dev;
	struct nfb_ifc_create_params ifc_params;
	struct rte_kvargs *kvlist = NULL;

	/* Open the device handle just for parsing ifc_params.
	 * A separate handle is used later for each netdev.
	 */
	nfb_dev = nfb_open(params->path);
	if (nfb_dev == NULL) {
		NFB_LOG(ERR, "nfb_open(): failed to open %s", params->path);
		return -EINVAL;
	}

	ret = nc_ifc_map_info_create_ordinary(nfb_dev, &ifc_params.map_info);
	if (ret)
		goto err_map_info_create;

	if (params->args != NULL && strlen(params->args) > 0) {
		kvlist = rte_kvargs_parse(params->args, VALID_KEYS);
		if (kvlist == NULL) {
			NFB_LOG(ERR, "Failed to parse device arguments %s", params->args);
			ret = -EINVAL;
			goto err_parse_args;
		}
	}

	ifc_params.ret = 0;
	ifc_params.probe_params = params;
	ifc_params.basename_len = strlen(params->name);

	/* When at least one port argument is specified, create only selected ports */
	if (kvlist && rte_kvargs_count(kvlist, NFB_ARG_PORT)) {
		ret = rte_kvargs_process(kvlist, NFB_ARG_PORT,
				nfb_eth_dev_create_for_ifc_by_port, (void *)&ifc_params);
		if (ret) {
			ret = ifc_params.ret;
			goto err_dev_create;
		}
	} else {
		for (i = 0; i < ifc_params.map_info.ifc_cnt; i++) {
			ifc_params.ifc_info = &ifc_params.map_info.ifc[i];
			ret = nfb_eth_dev_create_for_ifc(&ifc_params);
			if (ret)
				goto err_dev_create;
		}
	}

	rte_kvargs_free(kvlist);

	nc_map_info_destroy(&ifc_params.map_info);
	nfb_close(nfb_dev);

	return 0;

err_dev_create:
	nfb_eth_common_remove(params->device);
	rte_kvargs_free(kvlist);
err_parse_args:
	nc_map_info_destroy(&ifc_params.map_info);
err_map_info_create:
	nfb_close(nfb_dev);
	return ret;
}

static const struct rte_pci_id nfb_pci_id_table[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_NETCOPE, PCI_DEVICE_ID_NFB_40G2) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_NETCOPE, PCI_DEVICE_ID_NFB_100G2) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_NETCOPE, PCI_DEVICE_ID_NFB_200G2QL) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_SILICOM, PCI_DEVICE_ID_FB2CGG3) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_SILICOM, PCI_DEVICE_ID_FB2CGG3D) },
	{ .vendor_id = 0, }
};

/**
 * DPDK callback to register a PCI device.
 *
 * This function spawns Ethernet devices out of a given PCI device.
 *
 * @param[in] pci_drv
 *   PCI driver structure (nfb_driver).
 * @param[in] pci_dev
 *   PCI device information.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
static int
nfb_eth_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	int ret;
	char path[PATH_MAX];

	struct nc_composed_device_info comp_dev_info;
	struct nfb_probe_params params = {0};

	rte_pci_device_name(&pci_dev->addr, params.name, sizeof(params.name));

	/*
	 * NFB device can be composed from multiple PCI devices,
	 * find the base char device ID for the current PCI device
	 */
	ret = nc_get_composed_device_info_by_pci(NULL, params.name, &comp_dev_info);
	if (ret) {
		NFB_LOG(ERR, "Could not find NFB device for %s", params.name);
		return -ENODEV;
	}

	ret = snprintf(path, sizeof(path), NFB_BASE_DEV_PATH "%d", comp_dev_info.nfb_id);
	if (ret < 0 || ret >= (signed int)sizeof(path))
		return -EINVAL;

	params.args = pci_dev->device.devargs ? pci_dev->device.devargs->args : NULL;
	params.path = path;
	params.nfb_id = comp_dev_info.nfb_id;
	params.ep_index = comp_dev_info.ep_index;

	params.device = &pci_dev->device;
	params.specific_init = eth_dev_pci_specific_init;
	params.specific_device = pci_dev;

	return nfb_eth_common_probe(&params);
}

int
nfb_eth_common_remove(struct rte_device *dev)
{
	struct pmd_internals *entry, *temp;

	RTE_TAILQ_FOREACH_SAFE(entry, &nfb_eth_dev_list, eth_dev_list, temp) {
		if (dev == entry->eth_dev->device)
			rte_eth_dev_destroy(entry->eth_dev, nfb_eth_dev_uninit);
	}
	return 0;
}

/**
 * DPDK callback to remove a PCI device.
 *
 * This function removes all Ethernet devices belong to a given PCI device.
 *
 * @param[in] pci_dev
 *   Pointer to the PCI device.
 *
 * @return
 *   0 on success, the function cannot fail.
 */
static int
nfb_eth_pci_remove(struct rte_pci_device *pci_dev)
{
	return nfb_eth_common_remove(&pci_dev->device);
}

static struct rte_pci_driver nfb_eth_driver = {
	.id_table = nfb_pci_id_table,
	.probe = nfb_eth_pci_probe,
	.remove = nfb_eth_pci_remove,
};

RTE_PMD_REGISTER_PCI(RTE_NFB_DRIVER_NAME, nfb_eth_driver);
RTE_PMD_REGISTER_PCI_TABLE(RTE_NFB_DRIVER_NAME, nfb_pci_id_table);
RTE_PMD_REGISTER_KMOD_DEP(RTE_NFB_DRIVER_NAME, "* nfb");
RTE_LOG_REGISTER_DEFAULT(nfb_logtype, NOTICE);
RTE_PMD_REGISTER_PARAM_STRING(RTE_NFB_DRIVER_NAME,
		NFB_COMMON_ARGS
		);
