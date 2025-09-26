/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#include "nbl_userdev.h"

int nbl_pci_map_device(struct nbl_adapter *adapter)
{
	struct rte_pci_device *pci_dev = adapter->pci_dev;
	int ret = 0;

	ret = rte_pci_map_device(pci_dev);
	if (ret)
		NBL_LOG(ERR, "device %s uio or vfio map failed", pci_dev->device.name);

	return ret;
}

void nbl_pci_unmap_device(struct nbl_adapter *adapter)
{
	struct rte_pci_device *pci_dev = adapter->pci_dev;

	return rte_pci_unmap_device(pci_dev);
}
