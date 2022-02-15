/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Cesnet
 * Copyright(c) 2019 Netcope Technologies, a.s. <info@netcope.com>
 * All rights reserved.
 */

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

/**
 * Default MAC addr
 */
static const struct rte_ether_addr eth_addr = {
	.addr_bytes = { 0x00, 0x11, 0x17, 0x00, 0x00, 0x00 }
};

/**
 * Open all RX DMA queues
 *
 * @param dev
 *   Pointer to nfb device.
 * @param[out] rxmac
 *   Pointer to output array of nc_rxmac
 * @param[out] max_rxmac
 *   Pointer to output max index of rxmac
 */
static void
nfb_nc_rxmac_init(struct nfb_device *nfb,
	struct nc_rxmac *rxmac[RTE_MAX_NC_RXMAC],
	uint16_t *max_rxmac)
{
	*max_rxmac = 0;
	while ((rxmac[*max_rxmac] = nc_rxmac_open_index(nfb, *max_rxmac)))
		++(*max_rxmac);
}

/**
 * Open all TX DMA queues
 *
 * @param dev
 *   Pointer to nfb device.
 * @param[out] txmac
 *   Pointer to output array of nc_txmac
 * @param[out] max_rxmac
 *   Pointer to output max index of txmac
 */
static void
nfb_nc_txmac_init(struct nfb_device *nfb,
	struct nc_txmac *txmac[RTE_MAX_NC_TXMAC],
	uint16_t *max_txmac)
{
	*max_txmac = 0;
	while ((txmac[*max_txmac] = nc_txmac_open_index(nfb, *max_txmac)))
		++(*max_txmac);
}

/**
 * Close all RX DMA queues
 *
 * @param rxmac
 *   Pointer to array of nc_rxmac
 * @param max_rxmac
 *   Maximum index of rxmac
 */
static void
nfb_nc_rxmac_deinit(struct nc_rxmac *rxmac[RTE_MAX_NC_RXMAC],
	uint16_t max_rxmac)
{
	uint16_t i;
	for (i = 0; i < max_rxmac; i++) {
		nc_rxmac_close(rxmac[i]);
		rxmac[i] = NULL;
	}
}

/**
 * Close all TX DMA queues
 *
 * @param txmac
 *   Pointer to array of nc_txmac
 * @param max_txmac
 *   Maximum index of txmac
 */
static void
nfb_nc_txmac_deinit(struct nc_txmac *txmac[RTE_MAX_NC_TXMAC],
	uint16_t max_txmac)
{
	uint16_t i;
	for (i = 0; i < max_txmac; i++) {
		nc_txmac_close(txmac[i]);
		txmac[i] = NULL;
	}
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
	for (i = 0; i < nb_tx; i++)
		nfb_eth_tx_queue_stop(dev, i);
err_rx:
	for (i = 0; i < nb_rx; i++)
		nfb_eth_rx_queue_stop(dev, i);
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

	dev->data->dev_started = 0;

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
nfb_eth_dev_configure(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
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
	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = (uint32_t)-1;
	dev_info->max_rx_queues = dev->data->nb_rx_queues;
	dev_info->max_tx_queues = dev->data->nb_tx_queues;
	dev_info->speed_capa = RTE_ETH_LINK_SPEED_100G;

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
	struct pmd_internals *internals = dev->data->dev_private;
	uint16_t i;
	uint16_t nb_rx = dev->data->nb_rx_queues;
	uint16_t nb_tx = dev->data->nb_tx_queues;
	int ret;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	ret = nfb_eth_dev_stop(dev);

	nfb_nc_rxmac_deinit(internals->rxmac, internals->max_rxmac);
	nfb_nc_txmac_deinit(internals->txmac, internals->max_txmac);

	for (i = 0; i < nb_rx; i++) {
		nfb_eth_rx_queue_release(dev, i);
		dev->data->rx_queues[i] = NULL;
	}
	dev->data->nb_rx_queues = 0;
	for (i = 0; i < nb_tx; i++) {
		nfb_eth_tx_queue_release(dev, i);
		dev->data->tx_queues[i] = NULL;
	}
	dev->data->nb_tx_queues = 0;

	return ret;
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

	struct pmd_internals *internals = dev->data->dev_private;

	status.speed = MAC_SPEED_UNKNOWN;

	link.link_speed = RTE_ETH_SPEED_NUM_NONE;
	link.link_status = RTE_ETH_LINK_DOWN;
	link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	link.link_autoneg = RTE_ETH_LINK_SPEED_FIXED;

	if (internals->rxmac[0] != NULL) {
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
		dev->data->dev_private;

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
		dev->data->dev_private;

	uint16_t i;
	for (i = 0; i < internals->max_rxmac; ++i)
		nc_rxmac_disable(internals->rxmac[i]);

	for (i = 0; i < internals->max_txmac; ++i)
		nc_txmac_disable(internals->txmac[i]);

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
 *   0 on success, a negative errno value otherwise.
 */
static int
nfb_eth_mac_addr_set(struct rte_eth_dev *dev,
	struct rte_ether_addr *mac_addr)
{
	unsigned int i;
	uint64_t mac = 0;
	struct rte_eth_dev_data *data = dev->data;
	struct pmd_internals *internals = (struct pmd_internals *)
		data->dev_private;

	if (!rte_is_valid_assigned_ether_addr(mac_addr))
		return -EINVAL;

	for (i = 0; i < RTE_ETHER_ADDR_LEN; i++) {
		mac <<= 8;
		mac |= mac_addr->addr_bytes[i] & 0xFF;
	}

	for (i = 0; i < internals->max_rxmac; ++i)
		nc_rxmac_set_mac(internals->rxmac[i], 0, mac, 1);

	rte_ether_addr_copy(mac_addr, data->mac_addrs);
	return 0;
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
nfb_eth_dev_init(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	struct pmd_internals *internals = (struct pmd_internals *)
		data->dev_private;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_pci_addr *pci_addr = &pci_dev->addr;
	struct rte_ether_addr eth_addr_init;
	struct rte_kvargs *kvlist;

	RTE_LOG(INFO, PMD, "Initializing NFB device (" PCI_PRI_FMT ")\n",
		pci_addr->domain, pci_addr->bus, pci_addr->devid,
		pci_addr->function);

	snprintf(internals->nfb_dev, PATH_MAX,
		"/dev/nfb/by-pci-slot/" PCI_PRI_FMT,
		pci_addr->domain, pci_addr->bus, pci_addr->devid,
		pci_addr->function);

	/* Check validity of device args */
	if (dev->device->devargs != NULL &&
			dev->device->devargs->args != NULL &&
			strlen(dev->device->devargs->args) > 0) {
		kvlist = rte_kvargs_parse(dev->device->devargs->args,
						VALID_KEYS);
		if (kvlist == NULL) {
			RTE_LOG(ERR, PMD, "Failed to parse device arguments %s",
				dev->device->devargs->args);
			rte_kvargs_free(kvlist);
			return -EINVAL;
		}
		rte_kvargs_free(kvlist);
	}

	/*
	 * Get number of available DMA RX and TX queues, which is maximum
	 * number of queues that can be created and store it in private device
	 * data structure.
	 */
	internals->nfb = nfb_open(internals->nfb_dev);
	if (internals->nfb == NULL) {
		RTE_LOG(ERR, PMD, "nfb_open(): failed to open %s",
			internals->nfb_dev);
		return -EINVAL;
	}
	data->nb_rx_queues = ndp_get_rx_queue_available_count(internals->nfb);
	data->nb_tx_queues = ndp_get_tx_queue_available_count(internals->nfb);

	RTE_LOG(INFO, PMD, "Available NDP queues RX: %u TX: %u\n",
		data->nb_rx_queues, data->nb_tx_queues);

	nfb_nc_rxmac_init(internals->nfb,
		internals->rxmac,
		&internals->max_rxmac);
	nfb_nc_txmac_init(internals->nfb,
		internals->txmac,
		&internals->max_txmac);

	/* Set rx, tx burst functions */
	dev->rx_pkt_burst = nfb_eth_ndp_rx;
	dev->tx_pkt_burst = nfb_eth_ndp_tx;

	/* Set function callbacks for Ethernet API */
	dev->dev_ops = &ops;

	/* Get link state */
	nfb_eth_link_update(dev, 0);

	/* Allocate space for one mac address */
	data->mac_addrs = rte_zmalloc(data->name, sizeof(struct rte_ether_addr),
		RTE_CACHE_LINE_SIZE);
	if (data->mac_addrs == NULL) {
		RTE_LOG(ERR, PMD, "Could not alloc space for MAC address!\n");
		nfb_close(internals->nfb);
		return -EINVAL;
	}

	rte_eth_random_addr(eth_addr_init.addr_bytes);
	eth_addr_init.addr_bytes[0] = eth_addr.addr_bytes[0];
	eth_addr_init.addr_bytes[1] = eth_addr.addr_bytes[1];
	eth_addr_init.addr_bytes[2] = eth_addr.addr_bytes[2];

	nfb_eth_mac_addr_set(dev, &eth_addr_init);

	data->promiscuous = nfb_eth_promiscuous_get(dev);
	data->all_multicast = nfb_eth_allmulticast_get(dev);

	dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	RTE_LOG(INFO, PMD, "NFB device ("
		PCI_PRI_FMT ") successfully initialized\n",
		pci_addr->domain, pci_addr->bus, pci_addr->devid,
		pci_addr->function);

	return 0;
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
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_pci_addr *pci_addr = &pci_dev->addr;

	nfb_eth_dev_close(dev);

	RTE_LOG(INFO, PMD, "NFB device ("
		PCI_PRI_FMT ") successfully uninitialized\n",
		pci_addr->domain, pci_addr->bus, pci_addr->devid,
		pci_addr->function);

	return 0;
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
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct pmd_internals), nfb_eth_dev_init);
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
	return rte_eth_dev_pci_generic_remove(pci_dev, nfb_eth_dev_uninit);
}

static struct rte_pci_driver nfb_eth_driver = {
	.id_table = nfb_pci_id_table,
	.probe = nfb_eth_pci_probe,
	.remove = nfb_eth_pci_remove,
};

RTE_PMD_REGISTER_PCI(RTE_NFB_DRIVER_NAME, nfb_eth_driver);
RTE_PMD_REGISTER_PCI_TABLE(RTE_NFB_DRIVER_NAME, nfb_pci_id_table);
RTE_PMD_REGISTER_KMOD_DEP(RTE_NFB_DRIVER_NAME, "* nfb");
RTE_PMD_REGISTER_PARAM_STRING(RTE_NFB_DRIVER_NAME, TIMESTAMP_ARG "=<0|1>");
