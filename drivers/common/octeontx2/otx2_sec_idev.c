/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <rte_bus_pci.h>
#include <rte_ethdev.h>

#include "otx2_common.h"
#include "otx2_sec_idev.h"

/**
 * @internal
 * Check if rte_eth_dev is security offload capable otx2_eth_dev
 */
uint8_t
otx2_eth_dev_is_sec_capable(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	if (pci_dev->id.device_id == PCI_DEVID_OCTEONTX2_RVU_PF ||
	    pci_dev->id.device_id == PCI_DEVID_OCTEONTX2_RVU_VF ||
	    pci_dev->id.device_id == PCI_DEVID_OCTEONTX2_RVU_AF_VF)
		return 1;

	return 0;
}
