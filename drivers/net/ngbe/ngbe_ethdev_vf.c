/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2025 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <ethdev_pci.h>

#include "ngbe_logs.h"
#include "base/ngbe.h"
#include "ngbe_ethdev.h"
#include "ngbe_rxtx.h"
#include "ngbe_regs_group.h"

#define NGBEVF_PMD_NAME "rte_ngbevf_pmd" /* PMD name */
static int ngbevf_dev_close(struct rte_eth_dev *dev);

/*
 * The set of PCI devices this driver supports (for VF)
 */
static const struct rte_pci_id pci_id_ngbevf_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860AL_W_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A2_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A2S_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A4_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A4S_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860AL2_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860AL2S_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860AL4_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860AL4S_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860NCSI_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A1_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A1L_VF) },
	{ .vendor_id = 0, /* sentinel */ },
};

static const struct eth_dev_ops ngbevf_eth_dev_ops;

/*
 * Virtual Function device init
 */
static int
eth_ngbevf_dev_init(struct rte_eth_dev *eth_dev)
{
	int err;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct ngbe_hw *hw = ngbe_dev_hw(eth_dev);

	PMD_INIT_FUNC_TRACE();

	eth_dev->dev_ops = &ngbevf_eth_dev_ops;

	rte_eth_copy_pci_info(eth_dev, pci_dev);

	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->sub_system_id = pci_dev->id.subsystem_device_id;
	ngbe_map_device_id(hw);
	hw->hw_addr = (void *)pci_dev->mem_resource[0].addr;

	/* Initialize the shared code (base driver) */
	err = ngbe_init_shared_code(hw);
	if (err != 0) {
		PMD_INIT_LOG(ERR,
			"Shared code init failed for ngbevf: %d", err);
		return -EIO;
	}

	hw->mac.num_rar_entries = 32; /* The MAX of the underlying PF */

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("ngbevf", RTE_ETHER_ADDR_LEN *
					       hw->mac.num_rar_entries, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate %u bytes needed to store MAC addresses",
			     RTE_ETHER_ADDR_LEN * hw->mac.num_rar_entries);
		return -ENOMEM;
	}

	PMD_INIT_LOG(DEBUG, "port %d vendorID=0x%x deviceID=0x%x mac.type=%s",
		     eth_dev->data->port_id, pci_dev->id.vendor_id,
		     pci_dev->id.device_id, "ngbe_mac_sp_vf");

	return 0;
}

/* Virtual Function device uninit */
static int
eth_ngbevf_dev_uninit(struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	ngbevf_dev_close(eth_dev);

	return 0;
}

static int eth_ngbevf_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct ngbe_adapter), eth_ngbevf_dev_init);
}

static int eth_ngbevf_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, eth_ngbevf_dev_uninit);
}

/*
 * virtual function driver struct
 */
static struct rte_pci_driver rte_ngbevf_pmd = {
	.id_table = pci_id_ngbevf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = eth_ngbevf_pci_probe,
	.remove = eth_ngbevf_pci_remove,
};

static int
ngbevf_dev_info_get(struct rte_eth_dev *dev,
		     struct rte_eth_dev_info *dev_info)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);

	dev_info->max_rx_queues = (uint16_t)hw->mac.max_rx_queues;
	dev_info->max_tx_queues = (uint16_t)hw->mac.max_tx_queues;

	return 0;
}

static int
ngbevf_dev_close(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;

	return 0;
}

/*
 * dev_ops for virtual function, bare necessities for basic vf
 * operation have been implemented
 */
static const struct eth_dev_ops ngbevf_eth_dev_ops = {
	.dev_infos_get        = ngbevf_dev_info_get,
};

RTE_PMD_REGISTER_PCI(net_ngbe_vf, rte_ngbevf_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_ngbe_vf, pci_id_ngbevf_map);
RTE_PMD_REGISTER_KMOD_DEP(net_ngbe_vf, "* igb_uio | vfio-pci");
