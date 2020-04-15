/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Intel Corporation
 */

#include <stdint.h>

#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_malloc.h>

#include "igc_logs.h"
#include "igc_ethdev.h"

#define IGC_INTEL_VENDOR_ID		0x8086
#define IGC_DEV_ID_I225_LM		0x15F2
#define IGC_DEV_ID_I225_V		0x15F3
#define IGC_DEV_ID_I225_K		0x3100
#define IGC_DEV_ID_I225_I		0x15F8
#define IGC_DEV_ID_I220_V		0x15F7

static const struct rte_pci_id pci_id_igc_map[] = {
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I225_LM) },
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I225_V)  },
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I225_I)  },
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I225_K)  },
	{ .vendor_id = 0, /* sentinel */ },
};

static int eth_igc_configure(struct rte_eth_dev *dev);
static int eth_igc_link_update(struct rte_eth_dev *dev, int wait_to_complete);
static void eth_igc_stop(struct rte_eth_dev *dev);
static int eth_igc_start(struct rte_eth_dev *dev);
static void eth_igc_close(struct rte_eth_dev *dev);
static int eth_igc_reset(struct rte_eth_dev *dev);
static int eth_igc_promiscuous_enable(struct rte_eth_dev *dev);
static int eth_igc_promiscuous_disable(struct rte_eth_dev *dev);
static int eth_igc_infos_get(struct rte_eth_dev *dev,
			struct rte_eth_dev_info *dev_info);
static int
eth_igc_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		uint16_t nb_rx_desc, unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mb_pool);
static int
eth_igc_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		uint16_t nb_desc, unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);

static const struct eth_dev_ops eth_igc_ops = {
	.dev_configure		= eth_igc_configure,
	.link_update		= eth_igc_link_update,
	.dev_stop		= eth_igc_stop,
	.dev_start		= eth_igc_start,
	.dev_close		= eth_igc_close,
	.dev_reset		= eth_igc_reset,
	.promiscuous_enable	= eth_igc_promiscuous_enable,
	.promiscuous_disable	= eth_igc_promiscuous_disable,
	.dev_infos_get		= eth_igc_infos_get,
	.rx_queue_setup		= eth_igc_rx_queue_setup,
	.tx_queue_setup		= eth_igc_tx_queue_setup,
};

static int
eth_igc_configure(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	RTE_SET_USED(dev);
	return 0;
}

static int
eth_igc_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	PMD_INIT_FUNC_TRACE();
	RTE_SET_USED(dev);
	RTE_SET_USED(wait_to_complete);
	return 0;
}

static void
eth_igc_stop(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	RTE_SET_USED(dev);
}

static int
eth_igc_start(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	RTE_SET_USED(dev);
	return 0;
}

static void
eth_igc_close(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	 RTE_SET_USED(dev);
}

static int
eth_igc_dev_init(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	PMD_INIT_FUNC_TRACE();
	dev->dev_ops = &eth_igc_ops;

	/*
	 * for secondary processes, we don't initialize any further as primary
	 * has already done this work. Only check we don't need a different
	 * RX function.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	dev->data->mac_addrs = rte_zmalloc("igc",
		RTE_ETHER_ADDR_LEN, 0);
	if (dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate %d bytes for storing MAC",
				RTE_ETHER_ADDR_LEN);
		return -ENOMEM;
	}

	/* Pass the information to the rte_eth_dev_close() that it should also
	 * release the private port resources.
	 */
	dev->data->dev_flags |= RTE_ETH_DEV_CLOSE_REMOVE;

	PMD_INIT_LOG(DEBUG, "port_id %d vendorID=0x%x deviceID=0x%x",
			dev->data->port_id, pci_dev->id.vendor_id,
			pci_dev->id.device_id);

	return 0;
}

static int
eth_igc_dev_uninit(__rte_unused struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	eth_igc_close(eth_dev);
	return 0;
}

static int
eth_igc_reset(struct rte_eth_dev *dev)
{
	int ret;

	PMD_INIT_FUNC_TRACE();

	ret = eth_igc_dev_uninit(dev);
	if (ret)
		return ret;

	return eth_igc_dev_init(dev);
}

static int
eth_igc_promiscuous_enable(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	RTE_SET_USED(dev);
	return 0;
}

static int
eth_igc_promiscuous_disable(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	RTE_SET_USED(dev);
	return 0;
}

static int
eth_igc_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	PMD_INIT_FUNC_TRACE();
	RTE_SET_USED(dev);
	dev_info->max_rx_queues = IGC_QUEUE_PAIRS_NUM;
	dev_info->max_tx_queues = IGC_QUEUE_PAIRS_NUM;
	return 0;
}

static int
eth_igc_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		uint16_t nb_rx_desc, unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mb_pool)
{
	PMD_INIT_FUNC_TRACE();
	RTE_SET_USED(dev);
	RTE_SET_USED(rx_queue_id);
	RTE_SET_USED(nb_rx_desc);
	RTE_SET_USED(socket_id);
	RTE_SET_USED(rx_conf);
	RTE_SET_USED(mb_pool);
	return 0;
}

static int
eth_igc_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		uint16_t nb_desc, unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf)
{
	PMD_INIT_FUNC_TRACE();
	RTE_SET_USED(dev);
	RTE_SET_USED(queue_idx);
	RTE_SET_USED(nb_desc);
	RTE_SET_USED(socket_id);
	RTE_SET_USED(tx_conf);
	return 0;
}

static int
eth_igc_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	PMD_INIT_FUNC_TRACE();
	return rte_eth_dev_pci_generic_probe(pci_dev, 0, eth_igc_dev_init);
}

static int
eth_igc_pci_remove(struct rte_pci_device *pci_dev)
{
	PMD_INIT_FUNC_TRACE();
	return rte_eth_dev_pci_generic_remove(pci_dev, eth_igc_dev_uninit);
}

static struct rte_pci_driver rte_igc_pmd = {
	.id_table = pci_id_igc_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = eth_igc_pci_probe,
	.remove = eth_igc_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_igc, rte_igc_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_igc, pci_id_igc_map);
RTE_PMD_REGISTER_KMOD_DEP(net_igc, "* igb_uio | uio_pci_generic | vfio-pci");
