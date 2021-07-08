/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <errno.h>
#include <rte_common.h>
#include <ethdev_pci.h>

#include "ngbe_logs.h"
#include "ngbe.h"
#include "ngbe_ethdev.h"

static int ngbe_dev_close(struct rte_eth_dev *dev);

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_ngbe_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A2) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A2S) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A4) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A4S) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860AL2) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860AL2S) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860AL4) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860AL4S) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860NCSI) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A1) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A1L) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860AL_W) },
	{ .vendor_id = 0, /* sentinel */ },
};

/*
 * Ensure that all locks are released before first NVM or PHY access
 */
static void
ngbe_swfw_lock_reset(struct ngbe_hw *hw)
{
	uint16_t mask;

	/*
	 * These ones are more tricky since they are common to all ports; but
	 * swfw_sync retries last long enough (1s) to be almost sure that if
	 * lock can not be taken it is due to an improper lock of the
	 * semaphore.
	 */
	mask = NGBE_MNGSEM_SWPHY |
	       NGBE_MNGSEM_SWMBX |
	       NGBE_MNGSEM_SWFLASH;
	if (hw->mac.acquire_swfw_sync(hw, mask) < 0)
		PMD_DRV_LOG(DEBUG, "SWFW common locks released");

	hw->mac.release_swfw_sync(hw, mask);
}

static int
eth_ngbe_dev_init(struct rte_eth_dev *eth_dev, void *init_params __rte_unused)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct ngbe_hw *hw = ngbe_dev_hw(eth_dev);
	const struct rte_memzone *mz;
	int err;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	rte_eth_copy_pci_info(eth_dev, pci_dev);

	/* Vendor and Device ID need to be set before init of shared code */
	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->sub_system_id = pci_dev->id.subsystem_device_id;
	ngbe_map_device_id(hw);
	hw->hw_addr = (void *)pci_dev->mem_resource[0].addr;

	/* Reserve memory for interrupt status block */
	mz = rte_eth_dma_zone_reserve(eth_dev, "ngbe_driver", -1,
		NGBE_ISB_SIZE, NGBE_ALIGN, SOCKET_ID_ANY);
	if (mz == NULL)
		return -ENOMEM;

	hw->isb_dma = TMZ_PADDR(mz);
	hw->isb_mem = TMZ_VADDR(mz);

	/* Initialize the shared code (base driver) */
	err = ngbe_init_shared_code(hw);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "Shared code init failed: %d", err);
		return -EIO;
	}

	/* Unlock any pending hardware semaphore */
	ngbe_swfw_lock_reset(hw);

	err = hw->rom.init_params(hw);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "The EEPROM init failed: %d", err);
		return -EIO;
	}

	/* Make sure we have a good EEPROM before we read from it */
	err = hw->rom.validate_checksum(hw, NULL);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "The EEPROM checksum is not valid: %d", err);
		return -EIO;
	}

	err = hw->mac.init_hw(hw);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "Hardware Initialization Failure: %d", err);
		return -EIO;
	}

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("ngbe", RTE_ETHER_ADDR_LEN *
					       hw->mac.num_rar_entries, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate %u bytes needed to store MAC addresses",
			     RTE_ETHER_ADDR_LEN * hw->mac.num_rar_entries);
		return -ENOMEM;
	}

	/* Copy the permanent MAC address */
	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac.perm_addr,
			&eth_dev->data->mac_addrs[0]);

	/* Allocate memory for storing hash filter MAC addresses */
	eth_dev->data->hash_mac_addrs = rte_zmalloc("ngbe",
			RTE_ETHER_ADDR_LEN * NGBE_VMDQ_NUM_UC_MAC, 0);
	if (eth_dev->data->hash_mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate %d bytes needed to store MAC addresses",
			     RTE_ETHER_ADDR_LEN * NGBE_VMDQ_NUM_UC_MAC);
		rte_free(eth_dev->data->mac_addrs);
		eth_dev->data->mac_addrs = NULL;
		return -ENOMEM;
	}

	return 0;
}

static int
eth_ngbe_dev_uninit(struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	ngbe_dev_close(eth_dev);

	return -EINVAL;
}

static int
eth_ngbe_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_create(&pci_dev->device, pci_dev->device.name,
			sizeof(struct ngbe_adapter),
			eth_dev_pci_specific_init, pci_dev,
			eth_ngbe_dev_init, NULL);
}

static int eth_ngbe_pci_remove(struct rte_pci_device *pci_dev)
{
	struct rte_eth_dev *ethdev;

	ethdev = rte_eth_dev_allocated(pci_dev->device.name);
	if (ethdev == NULL)
		return 0;

	return rte_eth_dev_destroy(ethdev, eth_ngbe_dev_uninit);
}

static struct rte_pci_driver rte_ngbe_pmd = {
	.id_table = pci_id_ngbe_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = eth_ngbe_pci_probe,
	.remove = eth_ngbe_pci_remove,
};

/*
 * Reset and stop device.
 */
static int
ngbe_dev_close(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);

	return -EINVAL;
}

RTE_PMD_REGISTER_PCI(net_ngbe, rte_ngbe_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_ngbe, pci_id_ngbe_map);
RTE_PMD_REGISTER_KMOD_DEP(net_ngbe, "* igb_uio | uio_pci_generic | vfio-pci");

RTE_LOG_REGISTER_SUFFIX(ngbe_logtype_init, init, NOTICE);
RTE_LOG_REGISTER_SUFFIX(ngbe_logtype_driver, driver, NOTICE);

#ifdef RTE_ETHDEV_DEBUG_RX
	RTE_LOG_REGISTER_SUFFIX(ngbe_logtype_rx, rx, DEBUG);
#endif
#ifdef RTE_ETHDEV_DEBUG_TX
	RTE_LOG_REGISTER_SUFFIX(ngbe_logtype_tx, tx, DEBUG);
#endif
