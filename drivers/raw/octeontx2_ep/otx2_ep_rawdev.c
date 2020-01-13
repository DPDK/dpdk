/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */
#include <string.h>
#include <unistd.h>

#include <rte_bus.h>
#include <rte_bus_pci.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_mempool.h>
#include <rte_pci.h>

#include <rte_common.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>

#include "otx2_common.h"
#include "otx2_ep_rawdev.h"

static const struct rte_pci_id pci_sdp_vf_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
			       PCI_DEVID_OCTEONTX2_EP_VF)
	},
	{
		.vendor_id = 0,
	},
};

static int
otx2_sdp_rawdev_probe(struct rte_pci_driver *pci_drv __rte_unused,
		      struct rte_pci_device *pci_dev)
{
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct sdp_device *sdpvf = NULL;
	struct rte_rawdev *sdp_rawdev;
	uint16_t vf_id;

	/* Single process support */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (pci_dev->mem_resource[0].addr)
		otx2_info("SDP_EP BAR0 is mapped:");
	else {
		otx2_err("SDP_EP: Failed to map device BARs");
		otx2_err("BAR0 %p\n BAR2 %p",
			pci_dev->mem_resource[0].addr,
			pci_dev->mem_resource[2].addr);
		return -ENODEV;
	}

	memset(name, 0, sizeof(name));
	snprintf(name, RTE_RAWDEV_NAME_MAX_LEN, "SDPEP:%x:%02x.%x",
		 pci_dev->addr.bus, pci_dev->addr.devid,
		 pci_dev->addr.function);

	/* Allocate rawdev pmd */
	sdp_rawdev = rte_rawdev_pmd_allocate(name,
					     sizeof(struct sdp_device),
					     rte_socket_id());

	if (sdp_rawdev == NULL) {
		otx2_err("SDP_EP VF rawdev allocation failed");
		return -ENOMEM;
	}

	sdp_rawdev->device = &pci_dev->device;
	sdp_rawdev->driver_name = pci_dev->driver->driver.name;

	sdpvf = (struct sdp_device *)sdp_rawdev->dev_private;
	sdpvf->hw_addr = pci_dev->mem_resource[0].addr;
	sdpvf->pci_dev = pci_dev;

	/* Discover the VF number being probed */
	vf_id = ((pci_dev->addr.devid & 0x1F) << 3) |
		 (pci_dev->addr.function & 0x7);

	vf_id -= 1;
	sdpvf->vf_num = vf_id;

	otx2_info("SDP_EP VF[%d] probe done", vf_id);

	return 0;
}

static int
otx2_sdp_rawdev_remove(struct rte_pci_device *pci_dev)
{
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct rte_rawdev *rawdev;
	struct sdp_device *sdpvf;

	/* Single process support */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (pci_dev == NULL) {
		otx2_err("SDP_EP:invalid pci_dev!");
		return -EINVAL;
	}


	memset(name, 0, sizeof(name));
	snprintf(name, RTE_RAWDEV_NAME_MAX_LEN, "SDPEP:%x:%02x.%x",
		 pci_dev->addr.bus, pci_dev->addr.devid,
		 pci_dev->addr.function);

	rawdev = rte_rawdev_pmd_get_named_dev(name);
	if (rawdev == NULL) {
		otx2_err("SDP_EP: invalid device name (%s)", name);
		return -EINVAL;
	}

	sdpvf = (struct sdp_device *)rawdev->dev_private;
	otx2_info("Removing SDP_EP VF[%d] ", sdpvf->vf_num);

	/* rte_rawdev_close is called by pmd_release */
	return rte_rawdev_pmd_release(rawdev);
}

static struct rte_pci_driver rte_sdp_rawdev_pmd = {
	.id_table  = pci_sdp_vf_map,
	.drv_flags = (RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA),
	.probe     = otx2_sdp_rawdev_probe,
	.remove    = otx2_sdp_rawdev_remove,
};

RTE_PMD_REGISTER_PCI(sdp_rawdev_pci_driver, rte_sdp_rawdev_pmd);
RTE_PMD_REGISTER_PCI_TABLE(sdp_rawdev_pci_driver, pci_sdp_vf_map);
RTE_PMD_REGISTER_KMOD_DEP(sdp_rawdev_pci_driver, "vfio-pci");
