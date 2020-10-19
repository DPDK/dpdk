/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <rte_common.h>
#include <rte_ethdev_pci.h>
#include <rte_pci.h>
#include <rte_memory.h>

#include "txgbe_logs.h"
#include "base/txgbe.h"
#include "txgbe_ethdev.h"

static int txgbe_dev_close(struct rte_eth_dev *dev);

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_txgbe_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, TXGBE_DEV_ID_RAPTOR_SFP) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, TXGBE_DEV_ID_WX1820_SFP) },
	{ .vendor_id = 0, /* sentinel */ },
};

static const struct eth_dev_ops txgbe_eth_dev_ops;

static int
eth_txgbe_dev_init(struct rte_eth_dev *eth_dev, void *init_params __rte_unused)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	const struct rte_memzone *mz;

	PMD_INIT_FUNC_TRACE();

	eth_dev->dev_ops = &txgbe_eth_dev_ops;

	rte_eth_copy_pci_info(eth_dev, pci_dev);

	/* Vendor and Device ID need to be set before init of shared code */
	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->hw_addr = (void *)pci_dev->mem_resource[0].addr;
	hw->allow_unsupported_sfp = 1;

	/* Reserve memory for interrupt status block */
	mz = rte_eth_dma_zone_reserve(eth_dev, "txgbe_driver", -1,
		16, TXGBE_ALIGN, SOCKET_ID_ANY);
	if (mz == NULL)
		return -ENOMEM;

	hw->isb_dma = TMZ_PADDR(mz);
	hw->isb_mem = TMZ_VADDR(mz);

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("txgbe", RTE_ETHER_ADDR_LEN *
					       hw->mac.num_rar_entries, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate %u bytes needed to store "
			     "MAC addresses",
			     RTE_ETHER_ADDR_LEN * hw->mac.num_rar_entries);
		return -ENOMEM;
	}

	/* Copy the permanent MAC address */
	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac.perm_addr,
			&eth_dev->data->mac_addrs[0]);

	/* Allocate memory for storing hash filter MAC addresses */
	eth_dev->data->hash_mac_addrs = rte_zmalloc("txgbe",
			RTE_ETHER_ADDR_LEN * TXGBE_VMDQ_NUM_UC_MAC, 0);
	if (eth_dev->data->hash_mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate %d bytes needed to store MAC addresses",
			     RTE_ETHER_ADDR_LEN * TXGBE_VMDQ_NUM_UC_MAC);
		return -ENOMEM;
	}

	PMD_INIT_LOG(DEBUG, "port %d vendorID=0x%x deviceID=0x%x",
		     eth_dev->data->port_id, pci_dev->id.vendor_id,
		     pci_dev->id.device_id);

	/* enable uio/vfio intr/eventfd mapping */
	rte_intr_enable(intr_handle);

	return 0;
}

static int
eth_txgbe_dev_uninit(struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	txgbe_dev_close(eth_dev);

	return 0;
}

static int
eth_txgbe_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	struct rte_eth_dev *pf_ethdev;
	struct rte_eth_devargs eth_da;
	int retval;

	if (pci_dev->device.devargs) {
		retval = rte_eth_devargs_parse(pci_dev->device.devargs->args,
				&eth_da);
		if (retval)
			return retval;
	} else {
		memset(&eth_da, 0, sizeof(eth_da));
	}

	retval = rte_eth_dev_create(&pci_dev->device, pci_dev->device.name,
			sizeof(struct txgbe_adapter),
			eth_dev_pci_specific_init, pci_dev,
			eth_txgbe_dev_init, NULL);

	if (retval || eth_da.nb_representor_ports < 1)
		return retval;

	pf_ethdev = rte_eth_dev_allocated(pci_dev->device.name);
	if (pf_ethdev == NULL)
		return -ENODEV;

	return 0;
}

static int eth_txgbe_pci_remove(struct rte_pci_device *pci_dev)
{
	struct rte_eth_dev *ethdev;

	ethdev = rte_eth_dev_allocated(pci_dev->device.name);
	if (!ethdev)
		return -ENODEV;

	return rte_eth_dev_destroy(ethdev, eth_txgbe_dev_uninit);
}

static struct rte_pci_driver rte_txgbe_pmd = {
	.id_table = pci_id_txgbe_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING |
		     RTE_PCI_DRV_INTR_LSC,
	.probe = eth_txgbe_pci_probe,
	.remove = eth_txgbe_pci_remove,
};

/*
 * Reset and stop device.
 */
static int
txgbe_dev_close(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;

	PMD_INIT_FUNC_TRACE();

	/* disable uio intr before callback unregister */
	rte_intr_disable(intr_handle);

	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;

	rte_free(dev->data->hash_mac_addrs);
	dev->data->hash_mac_addrs = NULL;

	return 0;
}

static const struct eth_dev_ops txgbe_eth_dev_ops = {
};

RTE_PMD_REGISTER_PCI(net_txgbe, rte_txgbe_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_txgbe, pci_id_txgbe_map);
RTE_PMD_REGISTER_KMOD_DEP(net_txgbe, "* igb_uio | uio_pci_generic | vfio-pci");

RTE_LOG_REGISTER(txgbe_logtype_init, pmd.net.txgbe.init, NOTICE);
RTE_LOG_REGISTER(txgbe_logtype_driver, pmd.net.txgbe.driver, NOTICE);

#ifdef RTE_LIBRTE_TXGBE_DEBUG_RX
	RTE_LOG_REGISTER(txgbe_logtype_rx, pmd.net.txgbe.rx, DEBUG);
#endif
#ifdef RTE_LIBRTE_TXGBE_DEBUG_TX
	RTE_LOG_REGISTER(txgbe_logtype_tx, pmd.net.txgbe.tx, DEBUG);
#endif

#ifdef RTE_LIBRTE_TXGBE_DEBUG_TX_FREE
	RTE_LOG_REGISTER(txgbe_logtype_tx_free, pmd.net.txgbe.tx_free, DEBUG);
#endif
