/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_pci.h>

#include "otx2_common.h"

static int
npa_remove(struct rte_pci_device *pci_dev)
{
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	RTE_SET_USED(pci_dev);
	return 0;
}

static int
npa_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	RTE_SET_USED(pci_drv);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	RTE_SET_USED(pci_dev);
	return 0;
}

static const struct rte_pci_id pci_npa_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
					PCI_DEVID_OCTEONTX2_RVU_NPA_PF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
					PCI_DEVID_OCTEONTX2_RVU_NPA_VF)
	},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver pci_npa = {
	.id_table = pci_npa_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_IOVA_AS_VA,
	.probe = npa_probe,
	.remove = npa_remove,
};

RTE_PMD_REGISTER_PCI(mempool_octeontx2, pci_npa);
RTE_PMD_REGISTER_PCI_TABLE(mempool_octeontx2, pci_npa_map);
RTE_PMD_REGISTER_KMOD_DEP(mempool_octeontx2, "vfio-pci");
