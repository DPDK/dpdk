/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#include <ethdev_pci.h>
#include <rte_io.h>

#include "rnp.h"

static int
rnp_eth_dev_init(struct rte_eth_dev *eth_dev)
{
	RTE_SET_USED(eth_dev);

	return -ENODEV;
}

static int
rnp_eth_dev_uninit(struct rte_eth_dev *eth_dev)
{
	RTE_SET_USED(eth_dev);

	return -ENODEV;
}

static int
rnp_pci_remove(struct rte_pci_device *pci_dev)
{
	struct rte_eth_dev *eth_dev;

	eth_dev = rte_eth_dev_allocated(pci_dev->device.name);

	if (eth_dev)
		/* Cleanup eth dev */
		return rte_eth_dev_pci_generic_remove(pci_dev,
				rnp_eth_dev_uninit);
	return -ENODEV;
}

static int
rnp_pci_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	int rc;

	RTE_SET_USED(pci_drv);

	rc = rte_eth_dev_pci_generic_probe(pci_dev, sizeof(struct rnp_eth_port),
			rnp_eth_dev_init);

	return rc;
}

static const struct rte_pci_id pci_id_rnp_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_MUCSE, RNP_DEV_ID_N10G_X2) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_MUCSE, RNP_DEV_ID_N10G_X4) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_MUCSE, RNP_DEV_ID_N10G_X8) },
	{ .vendor_id = 0, },
};

static struct rte_pci_driver rte_rnp_pmd = {
	.id_table = pci_id_rnp_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = rnp_pci_probe,
	.remove = rnp_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_rnp, rte_rnp_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_rnp, pci_id_rnp_map);
RTE_PMD_REGISTER_KMOD_DEP(net_rnp, "igb_uio | uio_pci_generic | vfio-pci");
