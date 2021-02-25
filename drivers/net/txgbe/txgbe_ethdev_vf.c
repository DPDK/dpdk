/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <rte_log.h>
#include <ethdev_pci.h>

#include "txgbe_logs.h"
#include "base/txgbe.h"
#include "txgbe_ethdev.h"
#include "txgbe_rxtx.h"

static int txgbevf_dev_close(struct rte_eth_dev *dev);
static void txgbevf_intr_disable(struct rte_eth_dev *dev);
static void txgbevf_intr_enable(struct rte_eth_dev *dev);

/*
 * The set of PCI devices this driver supports (for VF)
 */
static const struct rte_pci_id pci_id_txgbevf_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, TXGBE_DEV_ID_RAPTOR_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, TXGBE_DEV_ID_RAPTOR_VF_HV) },
	{ .vendor_id = 0, /* sentinel */ },
};

static const struct eth_dev_ops txgbevf_eth_dev_ops;

/*
 * Negotiate mailbox API version with the PF.
 * After reset API version is always set to the basic one (txgbe_mbox_api_10).
 * Then we try to negotiate starting with the most recent one.
 * If all negotiation attempts fail, then we will proceed with
 * the default one (txgbe_mbox_api_10).
 */
static void
txgbevf_negotiate_api(struct txgbe_hw *hw)
{
	int32_t i;

	/* start with highest supported, proceed down */
	static const int sup_ver[] = {
		txgbe_mbox_api_13,
		txgbe_mbox_api_12,
		txgbe_mbox_api_11,
		txgbe_mbox_api_10,
	};

	for (i = 0; i < ARRAY_SIZE(sup_ver); i++) {
		if (txgbevf_negotiate_api_version(hw, sup_ver[i]) == 0)
			break;
	}
}

/*
 * Virtual Function device init
 */
static int
eth_txgbevf_dev_init(struct rte_eth_dev *eth_dev)
{
	int err;
	uint32_t tc, tcs;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);

	PMD_INIT_FUNC_TRACE();

	eth_dev->dev_ops = &txgbevf_eth_dev_ops;

	/* for secondary processes, we don't initialise any further as primary
	 * has already done this work. Only check we don't need a different
	 * RX function
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		struct txgbe_tx_queue *txq;
		uint16_t nb_tx_queues = eth_dev->data->nb_tx_queues;
		/* TX queue function in primary, set by last queue initialized
		 * Tx queue may not initialized by primary process
		 */
		if (eth_dev->data->tx_queues) {
			txq = eth_dev->data->tx_queues[nb_tx_queues - 1];
			txgbe_set_tx_function(eth_dev, txq);
		} else {
			/* Use default TX function if we get here */
			PMD_INIT_LOG(NOTICE,
				     "No TX queues configured yet. Using default TX function.");
		}

		txgbe_set_rx_function(eth_dev);

		return 0;
	}

	rte_eth_copy_pci_info(eth_dev, pci_dev);

	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->subsystem_device_id = pci_dev->id.subsystem_device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;
	hw->hw_addr = (void *)pci_dev->mem_resource[0].addr;

	/* Initialize the shared code (base driver) */
	err = txgbe_init_shared_code(hw);
	if (err != 0) {
		PMD_INIT_LOG(ERR,
			"Shared code init failed for txgbevf: %d", err);
		return -EIO;
	}

	/* init_mailbox_params */
	hw->mbx.init_params(hw);

	/* Disable the interrupts for VF */
	txgbevf_intr_disable(eth_dev);

	hw->mac.num_rar_entries = 128; /* The MAX of the underlying PF */
	err = hw->mac.reset_hw(hw);

	/*
	 * The VF reset operation returns the TXGBE_ERR_INVALID_MAC_ADDR when
	 * the underlying PF driver has not assigned a MAC address to the VF.
	 * In this case, assign a random MAC address.
	 */
	if (err != 0 && err != TXGBE_ERR_INVALID_MAC_ADDR) {
		PMD_INIT_LOG(ERR, "VF Initialization Failure: %d", err);
		/*
		 * This error code will be propagated to the app by
		 * rte_eth_dev_reset, so use a public error code rather than
		 * the internal-only TXGBE_ERR_RESET_FAILED
		 */
		return -EAGAIN;
	}

	/* negotiate mailbox API version to use with the PF. */
	txgbevf_negotiate_api(hw);

	/* Get Rx/Tx queue count via mailbox, which is ready after reset_hw */
	txgbevf_get_queues(hw, &tcs, &tc);

	txgbevf_intr_enable(eth_dev);

	return 0;
}

/* Virtual Function device uninit */
static int
eth_txgbevf_dev_uninit(struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	txgbevf_dev_close(eth_dev);

	return 0;
}

static int eth_txgbevf_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct txgbe_adapter), eth_txgbevf_dev_init);
}

static int eth_txgbevf_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, eth_txgbevf_dev_uninit);
}

/*
 * virtual function driver struct
 */
static struct rte_pci_driver rte_txgbevf_pmd = {
	.id_table = pci_id_txgbevf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = eth_txgbevf_pci_probe,
	.remove = eth_txgbevf_pci_remove,
};

/*
 * Virtual Function operations
 */
static void
txgbevf_intr_disable(struct rte_eth_dev *dev)
{
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	PMD_INIT_FUNC_TRACE();

	/* Clear interrupt mask to stop from interrupts being generated */
	wr32(hw, TXGBE_VFIMS, TXGBE_VFIMS_MASK);

	txgbe_flush(hw);

	/* Clear mask value. */
	intr->mask_misc = TXGBE_VFIMS_MASK;
}

static void
txgbevf_intr_enable(struct rte_eth_dev *dev)
{
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	PMD_INIT_FUNC_TRACE();

	/* VF enable interrupt autoclean */
	wr32(hw, TXGBE_VFIMC, TXGBE_VFIMC_MASK);

	txgbe_flush(hw);

	intr->mask_misc = 0;
}

static int
txgbevf_dev_close(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	PMD_INIT_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	hw->mac.reset_hw(hw);

	txgbe_dev_free_queues(dev);

	/* Disable the interrupts for VF */
	txgbevf_intr_disable(dev);

	return 0;
}

/*
 * dev_ops for virtual function, bare necessities for basic vf
 * operation have been implemented
 */
static const struct eth_dev_ops txgbevf_eth_dev_ops = {
};

RTE_PMD_REGISTER_PCI(net_txgbe_vf, rte_txgbevf_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_txgbe_vf, pci_id_txgbevf_map);
RTE_PMD_REGISTER_KMOD_DEP(net_txgbe_vf, "* igb_uio | vfio-pci");
