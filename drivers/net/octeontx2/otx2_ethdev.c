/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_ethdev_pci.h>
#include <rte_io.h>
#include <rte_malloc.h>

#include "otx2_ethdev.h"

static int
otx2_eth_dev_init(struct rte_eth_dev *eth_dev)
{
	RTE_SET_USED(eth_dev);

	return -ENODEV;
}

static int
otx2_eth_dev_uninit(struct rte_eth_dev *eth_dev, bool mbox_close)
{
	RTE_SET_USED(eth_dev);
	RTE_SET_USED(mbox_close);

	return -ENODEV;
}

static int
nix_remove(struct rte_pci_device *pci_dev)
{
	struct rte_eth_dev *eth_dev;
	int rc;

	eth_dev = rte_eth_dev_allocated(pci_dev->device.name);
	if (eth_dev) {
		/* Cleanup eth dev */
		rc = otx2_eth_dev_uninit(eth_dev, true);
		if (rc)
			return rc;

		rte_eth_dev_pci_release(eth_dev);
	}

	/* Nothing to be done for secondary processes */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	return 0;
}

static int
nix_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	int rc;

	RTE_SET_USED(pci_drv);

	rc = rte_eth_dev_pci_generic_probe(pci_dev, sizeof(struct otx2_eth_dev),
					   otx2_eth_dev_init);

	/* On error on secondary, recheck if port exists in primary or
	 * in mid of detach state.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY && rc)
		if (!rte_eth_dev_allocated(pci_dev->device.name))
			return 0;
	return rc;
}

static const struct rte_pci_id pci_nix_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_OCTEONTX2_RVU_PF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_OCTEONTX2_RVU_VF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
			       PCI_DEVID_OCTEONTX2_RVU_AF_VF)
	},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver pci_nix = {
	.id_table = pci_nix_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_IOVA_AS_VA |
			RTE_PCI_DRV_INTR_LSC,
	.probe = nix_probe,
	.remove = nix_remove,
};

RTE_PMD_REGISTER_PCI(net_octeontx2, pci_nix);
RTE_PMD_REGISTER_PCI_TABLE(net_octeontx2, pci_nix_map);
RTE_PMD_REGISTER_KMOD_DEP(net_octeontx2, "vfio-pci");
