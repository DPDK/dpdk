/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <rte_bus_pci.h>

#include "ioat_private.h"

#define IDXD_VENDOR_ID		0x8086
#define IDXD_DEVICE_ID_SPR	0x0B25

#define IDXD_PMD_RAWDEV_NAME_PCI rawdev_idxd_pci

const struct rte_pci_id pci_id_idxd_map[] = {
	{ RTE_PCI_DEVICE(IDXD_VENDOR_ID, IDXD_DEVICE_ID_SPR) },
	{ .vendor_id = 0, /* sentinel */ },
};

static int
idxd_rawdev_probe_pci(struct rte_pci_driver *drv, struct rte_pci_device *dev)
{
	int ret = 0;
	char name[PCI_PRI_STR_SIZE];

	rte_pci_device_name(&dev->addr, name, sizeof(name));
	IOAT_PMD_INFO("Init %s on NUMA node %d", name, dev->device.numa_node);
	dev->device.driver = &drv->driver;

	return ret;
}

static int
idxd_rawdev_remove_pci(struct rte_pci_device *dev)
{
	char name[PCI_PRI_STR_SIZE];
	int ret = 0;

	rte_pci_device_name(&dev->addr, name, sizeof(name));

	IOAT_PMD_INFO("Closing %s on NUMA node %d",
			name, dev->device.numa_node);

	return ret;
}

struct rte_pci_driver idxd_pmd_drv_pci = {
	.id_table = pci_id_idxd_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = idxd_rawdev_probe_pci,
	.remove = idxd_rawdev_remove_pci,
};

RTE_PMD_REGISTER_PCI(IDXD_PMD_RAWDEV_NAME_PCI, idxd_pmd_drv_pci);
RTE_PMD_REGISTER_PCI_TABLE(IDXD_PMD_RAWDEV_NAME_PCI, pci_id_idxd_map);
RTE_PMD_REGISTER_KMOD_DEP(IDXD_PMD_RAWDEV_NAME_PCI,
			  "* igb_uio | uio_pci_generic | vfio-pci");
