/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#include <rte_common.h>
#include <rte_ethdev_pci.h>
#include <rte_pci.h>

#include "txgbe_logs.h"
#include "base/txgbe.h"
#include "txgbe_ethdev.h"

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_txgbe_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, TXGBE_DEV_ID_RAPTOR_SFP) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, TXGBE_DEV_ID_WX1820_SFP) },
	{ .vendor_id = 0, /* sentinel */ },
};

static int
eth_txgbe_dev_init(struct rte_eth_dev *eth_dev, void *init_params __rte_unused)
{
	RTE_SET_USED(eth_dev);

	return 0;
}

static int
eth_txgbe_dev_uninit(struct rte_eth_dev *eth_dev)
{
	RTE_SET_USED(eth_dev);

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
