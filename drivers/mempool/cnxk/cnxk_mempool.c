/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <rte_atomic.h>
#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_devargs.h>
#include <rte_eal.h>
#include <rte_io.h>
#include <rte_kvargs.h>
#include <rte_malloc.h>
#include <rte_mbuf_pool_ops.h>
#include <rte_pci.h>

#include "roc_api.h"

static int
npa_remove(struct rte_pci_device *pci_dev)
{
	RTE_SET_USED(pci_dev);

	return 0;
}

static int
npa_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	RTE_SET_USED(pci_drv);
	RTE_SET_USED(pci_dev);

	return 0;
}

static const struct rte_pci_id npa_pci_map[] = {
	{
		.class_id = RTE_CLASS_ANY_ID,
		.vendor_id = PCI_VENDOR_ID_CAVIUM,
		.device_id = PCI_DEVID_CNXK_RVU_NPA_PF,
		.subsystem_vendor_id = PCI_VENDOR_ID_CAVIUM,
		.subsystem_device_id = PCI_SUBSYSTEM_DEVID_CN10KA,
	},
	{
		.class_id = RTE_CLASS_ANY_ID,
		.vendor_id = PCI_VENDOR_ID_CAVIUM,
		.device_id = PCI_DEVID_CNXK_RVU_NPA_PF,
		.subsystem_vendor_id = PCI_VENDOR_ID_CAVIUM,
		.subsystem_device_id = PCI_SUBSYSTEM_DEVID_CN10KAS,
	},
	{
		.class_id = RTE_CLASS_ANY_ID,
		.vendor_id = PCI_VENDOR_ID_CAVIUM,
		.device_id = PCI_DEVID_CNXK_RVU_NPA_VF,
		.subsystem_vendor_id = PCI_VENDOR_ID_CAVIUM,
		.subsystem_device_id = PCI_SUBSYSTEM_DEVID_CN10KA,
	},
	{
		.class_id = RTE_CLASS_ANY_ID,
		.vendor_id = PCI_VENDOR_ID_CAVIUM,
		.device_id = PCI_DEVID_CNXK_RVU_NPA_VF,
		.subsystem_vendor_id = PCI_VENDOR_ID_CAVIUM,
		.subsystem_device_id = PCI_SUBSYSTEM_DEVID_CN10KAS,
	},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver npa_pci = {
	.id_table = npa_pci_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA,
	.probe = npa_probe,
	.remove = npa_remove,
};

RTE_PMD_REGISTER_PCI(mempool_cnxk, npa_pci);
RTE_PMD_REGISTER_PCI_TABLE(mempool_cnxk, npa_pci_map);
RTE_PMD_REGISTER_KMOD_DEP(mempool_cnxk, "vfio-pci");
