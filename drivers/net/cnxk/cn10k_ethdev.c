/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include "cn10k_ethdev.h"

static int
cn10k_nix_remove(struct rte_pci_device *pci_dev)
{
	return cnxk_nix_remove(pci_dev);
}

static int
cn10k_nix_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	struct rte_eth_dev *eth_dev;
	int rc;

	if (RTE_CACHE_LINE_SIZE != 64) {
		plt_err("Driver not compiled for CN10K");
		return -EFAULT;
	}

	rc = roc_plt_init();
	if (rc) {
		plt_err("Failed to initialize platform model, rc=%d", rc);
		return rc;
	}

	/* Common probe */
	rc = cnxk_nix_probe(pci_drv, pci_dev);
	if (rc)
		return rc;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		eth_dev = rte_eth_dev_allocated(pci_dev->device.name);
		if (!eth_dev)
			return -ENOENT;
	}
	return 0;
}

static const struct rte_pci_id cn10k_pci_nix_map[] = {
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KA, PCI_DEVID_CNXK_RVU_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KAS, PCI_DEVID_CNXK_RVU_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KA, PCI_DEVID_CNXK_RVU_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KAS, PCI_DEVID_CNXK_RVU_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KA, PCI_DEVID_CNXK_RVU_AF_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KAS, PCI_DEVID_CNXK_RVU_AF_VF),
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver cn10k_pci_nix = {
	.id_table = cn10k_pci_nix_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA |
		     RTE_PCI_DRV_INTR_LSC,
	.probe = cn10k_nix_probe,
	.remove = cn10k_nix_remove,
};

RTE_PMD_REGISTER_PCI(net_cn10k, cn10k_pci_nix);
RTE_PMD_REGISTER_PCI_TABLE(net_cn10k, cn10k_pci_nix_map);
RTE_PMD_REGISTER_KMOD_DEP(net_cn10k, "vfio-pci");
