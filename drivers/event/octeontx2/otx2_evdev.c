/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <inttypes.h>

#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_eventdev_pmd_pci.h>
#include <rte_pci.h>

#include "otx2_evdev.h"

static int
otx2_sso_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_probe(pci_drv, pci_dev,
				       sizeof(struct otx2_sso_evdev),
				       otx2_sso_init);
}

static int
otx2_sso_remove(struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_remove(pci_dev, otx2_sso_fini);
}

static const struct rte_pci_id pci_sso_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
			       PCI_DEVID_OCTEONTX2_RVU_SSO_TIM_PF)
	},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver pci_sso = {
	.id_table = pci_sso_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_IOVA_AS_VA,
	.probe = otx2_sso_probe,
	.remove = otx2_sso_remove,
};

int
otx2_sso_init(struct rte_eventdev *event_dev)
{
	RTE_SET_USED(event_dev);
	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	return 0;
}

int
otx2_sso_fini(struct rte_eventdev *event_dev)
{
	RTE_SET_USED(event_dev);
	/* For secondary processes, nothing to be done */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	return 0;
}

RTE_PMD_REGISTER_PCI(event_octeontx2, pci_sso);
RTE_PMD_REGISTER_PCI_TABLE(event_octeontx2, pci_sso_map);
RTE_PMD_REGISTER_KMOD_DEP(event_octeontx2, "vfio-pci");
