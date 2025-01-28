/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Yunsilicon Technology Co., Ltd.
 */

#include <ethdev_pci.h>

#include "xsc_log.h"
#include "xsc_defs.h"
#include "xsc_ethdev.h"

static int
xsc_ethdev_init(struct rte_eth_dev *eth_dev)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(eth_dev);

	PMD_INIT_FUNC_TRACE();

	priv->eth_dev = eth_dev;
	priv->pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	return 0;
}

static int
xsc_ethdev_uninit(struct rte_eth_dev *eth_dev)
{
	RTE_SET_USED(eth_dev);

	PMD_INIT_FUNC_TRACE();

	return 0;
}

static int
xsc_ethdev_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		     struct rte_pci_device *pci_dev)
{
	int ret;

	PMD_INIT_FUNC_TRACE();

	ret = rte_eth_dev_pci_generic_probe(pci_dev,
					    sizeof(struct xsc_ethdev_priv),
					    xsc_ethdev_init);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to probe ethdev: %s", pci_dev->name);
		return ret;
	}

	return 0;
}

static int
xsc_ethdev_pci_remove(struct rte_pci_device *pci_dev)
{
	int ret;

	PMD_INIT_FUNC_TRACE();

	ret = rte_eth_dev_pci_generic_remove(pci_dev, xsc_ethdev_uninit);
	if (ret) {
		PMD_DRV_LOG(ERR, "Could not remove ethdev: %s", pci_dev->name);
		return ret;
	}

	return 0;
}

static const struct rte_pci_id xsc_ethdev_pci_id_map[] = {
	{ RTE_PCI_DEVICE(XSC_PCI_VENDOR_ID, XSC_PCI_DEV_ID_MS) },
	{ RTE_PCI_DEVICE(XSC_PCI_VENDOR_ID, XSC_PCI_DEV_ID_MSVF) },
	{ RTE_PCI_DEVICE(XSC_PCI_VENDOR_ID, XSC_PCI_DEV_ID_MVH) },
	{ RTE_PCI_DEVICE(XSC_PCI_VENDOR_ID, XSC_PCI_DEV_ID_MVHVF) },
	{ RTE_PCI_DEVICE(XSC_PCI_VENDOR_ID, XSC_PCI_DEV_ID_MVS) },
	{ RTE_PCI_DEVICE(0, 0) },
};

static struct rte_pci_driver xsc_ethdev_pci_driver = {
	.id_table  = xsc_ethdev_pci_id_map,
	.probe = xsc_ethdev_pci_probe,
	.remove = xsc_ethdev_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_xsc, xsc_ethdev_pci_driver);
RTE_PMD_REGISTER_PCI_TABLE(net_xsc, xsc_ethdev_pci_id_map);

RTE_LOG_REGISTER_SUFFIX(xsc_logtype_init, init, NOTICE);
RTE_LOG_REGISTER_SUFFIX(xsc_logtype_driver, driver, NOTICE);
