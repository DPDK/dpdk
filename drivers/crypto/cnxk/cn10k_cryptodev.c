/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_crypto.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_dev.h>
#include <rte_pci.h>

#include "cn10k_cryptodev.h"
#include "roc_api.h"

uint8_t cn10k_cryptodev_driver_id;

static struct rte_pci_id pci_id_cpt_table[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
			       PCI_DEVID_CN10K_RVU_CPT_VF)
	},
	/* sentinel */
	{
		.device_id = 0
	},
};

static struct rte_pci_driver cn10k_cryptodev_pmd = {
	.id_table = pci_id_cpt_table,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA,
	.probe = NULL,
	.remove = NULL,
};

static struct cryptodev_driver cn10k_cryptodev_drv;

RTE_PMD_REGISTER_PCI(CRYPTODEV_NAME_CN10K_PMD, cn10k_cryptodev_pmd);
RTE_PMD_REGISTER_PCI_TABLE(CRYPTODEV_NAME_CN10K_PMD, pci_id_cpt_table);
RTE_PMD_REGISTER_KMOD_DEP(CRYPTODEV_NAME_CN10K_PMD, "vfio-pci");
RTE_PMD_REGISTER_CRYPTO_DRIVER(cn10k_cryptodev_drv, cn10k_cryptodev_pmd.driver,
			       cn10k_cryptodev_driver_id);
