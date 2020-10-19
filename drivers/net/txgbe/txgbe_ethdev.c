/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <rte_common.h>
#include <rte_ethdev_pci.h>

#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_alarm.h>

#include "txgbe_logs.h"
#include "base/txgbe.h"
#include "txgbe_ethdev.h"
#include "txgbe_rxtx.h"

static int txgbe_dev_close(struct rte_eth_dev *dev);

static void txgbe_dev_link_status_print(struct rte_eth_dev *dev);
static int txgbe_dev_lsc_interrupt_setup(struct rte_eth_dev *dev, uint8_t on);
static int txgbe_dev_macsec_interrupt_setup(struct rte_eth_dev *dev);
static int txgbe_dev_rxq_interrupt_setup(struct rte_eth_dev *dev);
static int txgbe_dev_interrupt_get_status(struct rte_eth_dev *dev);
static int txgbe_dev_interrupt_action(struct rte_eth_dev *dev,
				      struct rte_intr_handle *handle);
static void txgbe_dev_interrupt_handler(void *param);
static void txgbe_dev_interrupt_delayed_handler(void *param);
static void txgbe_configure_msix(struct rte_eth_dev *dev);

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_txgbe_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, TXGBE_DEV_ID_RAPTOR_SFP) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, TXGBE_DEV_ID_WX1820_SFP) },
	{ .vendor_id = 0, /* sentinel */ },
};

static const struct rte_eth_desc_lim rx_desc_lim = {
	.nb_max = TXGBE_RING_DESC_MAX,
	.nb_min = TXGBE_RING_DESC_MIN,
	.nb_align = TXGBE_RXD_ALIGN,
};

static const struct rte_eth_desc_lim tx_desc_lim = {
	.nb_max = TXGBE_RING_DESC_MAX,
	.nb_min = TXGBE_RING_DESC_MIN,
	.nb_align = TXGBE_TXD_ALIGN,
	.nb_seg_max = TXGBE_TX_MAX_SEG,
	.nb_mtu_seg_max = TXGBE_TX_MAX_SEG,
};

static const struct eth_dev_ops txgbe_eth_dev_ops;

static inline int
txgbe_is_sfp(struct txgbe_hw *hw)
{
	switch (hw->phy.type) {
	case txgbe_phy_sfp_avago:
	case txgbe_phy_sfp_ftl:
	case txgbe_phy_sfp_intel:
	case txgbe_phy_sfp_unknown:
	case txgbe_phy_sfp_tyco_passive:
	case txgbe_phy_sfp_unknown_passive:
		return 1;
	default:
		return 0;
	}
}

static inline void
txgbe_enable_intr(struct rte_eth_dev *dev)
{
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	wr32(hw, TXGBE_IENMISC, intr->mask_misc);
	wr32(hw, TXGBE_IMC(0), TXGBE_IMC_MASK);
	wr32(hw, TXGBE_IMC(1), TXGBE_IMC_MASK);
	txgbe_flush(hw);
}

static void
txgbe_disable_intr(struct txgbe_hw *hw)
{
	PMD_INIT_FUNC_TRACE();

	wr32(hw, TXGBE_IENMISC, ~BIT_MASK32);
	wr32(hw, TXGBE_IMS(0), TXGBE_IMC_MASK);
	wr32(hw, TXGBE_IMS(1), TXGBE_IMC_MASK);
	txgbe_flush(hw);
}

static int
eth_txgbe_dev_init(struct rte_eth_dev *eth_dev, void *init_params __rte_unused)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	const struct rte_memzone *mz;
	uint16_t csum;
	int err;

	PMD_INIT_FUNC_TRACE();

	eth_dev->dev_ops = &txgbe_eth_dev_ops;

	rte_eth_copy_pci_info(eth_dev, pci_dev);

	/* Vendor and Device ID need to be set before init of shared code */
	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->hw_addr = (void *)pci_dev->mem_resource[0].addr;
	hw->allow_unsupported_sfp = 1;

	/* Reserve memory for interrupt status block */
	mz = rte_eth_dma_zone_reserve(eth_dev, "txgbe_driver", -1,
		16, TXGBE_ALIGN, SOCKET_ID_ANY);
	if (mz == NULL)
		return -ENOMEM;

	hw->isb_dma = TMZ_PADDR(mz);
	hw->isb_mem = TMZ_VADDR(mz);

	/* Initialize the shared code (base driver) */
	err = txgbe_init_shared_code(hw);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "Shared code init failed: %d", err);
		return -EIO;
	}

	err = hw->rom.init_params(hw);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "The EEPROM init failed: %d", err);
		return -EIO;
	}

	/* Make sure we have a good EEPROM before we read from it */
	err = hw->rom.validate_checksum(hw, &csum);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "The EEPROM checksum is not valid: %d", err);
		return -EIO;
	}

	err = hw->mac.init_hw(hw);

	/*
	 * Devices with copper phys will fail to initialise if txgbe_init_hw()
	 * is called too soon after the kernel driver unbinding/binding occurs.
	 * The failure occurs in txgbe_identify_phy() for all devices,
	 * but for non-copper devies, txgbe_identify_sfp_module() is
	 * also called. See txgbe_identify_phy(). The reason for the
	 * failure is not known, and only occuts when virtualisation features
	 * are disabled in the bios. A delay of 200ms  was found to be enough by
	 * trial-and-error, and is doubled to be safe.
	 */
	if (err && hw->phy.media_type == txgbe_media_type_copper) {
		rte_delay_ms(200);
		err = hw->mac.init_hw(hw);
	}

	if (err == TXGBE_ERR_SFP_NOT_PRESENT)
		err = 0;

	if (err == TXGBE_ERR_EEPROM_VERSION) {
		PMD_INIT_LOG(ERR, "This device is a pre-production adapter/"
			     "LOM.  Please be aware there may be issues associated "
			     "with your hardware.");
		PMD_INIT_LOG(ERR, "If you are experiencing problems "
			     "please contact your hardware representative "
			     "who provided you with this hardware.");
	} else if (err == TXGBE_ERR_SFP_NOT_SUPPORTED) {
		PMD_INIT_LOG(ERR, "Unsupported SFP+ Module");
	}
	if (err) {
		PMD_INIT_LOG(ERR, "Hardware Initialization Failure: %d", err);
		return -EIO;
	}

	/* disable interrupt */
	txgbe_disable_intr(hw);

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("txgbe", RTE_ETHER_ADDR_LEN *
					       hw->mac.num_rar_entries, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate %u bytes needed to store "
			     "MAC addresses",
			     RTE_ETHER_ADDR_LEN * hw->mac.num_rar_entries);
		return -ENOMEM;
	}

	/* Copy the permanent MAC address */
	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac.perm_addr,
			&eth_dev->data->mac_addrs[0]);

	/* Allocate memory for storing hash filter MAC addresses */
	eth_dev->data->hash_mac_addrs = rte_zmalloc("txgbe",
			RTE_ETHER_ADDR_LEN * TXGBE_VMDQ_NUM_UC_MAC, 0);
	if (eth_dev->data->hash_mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate %d bytes needed to store MAC addresses",
			     RTE_ETHER_ADDR_LEN * TXGBE_VMDQ_NUM_UC_MAC);
		return -ENOMEM;
	}

	if (txgbe_is_sfp(hw) && hw->phy.sfp_type != txgbe_sfp_type_not_present)
		PMD_INIT_LOG(DEBUG, "MAC: %d, PHY: %d, SFP+: %d",
			     (int)hw->mac.type, (int)hw->phy.type,
			     (int)hw->phy.sfp_type);
	else
		PMD_INIT_LOG(DEBUG, "MAC: %d, PHY: %d",
			     (int)hw->mac.type, (int)hw->phy.type);

	PMD_INIT_LOG(DEBUG, "port %d vendorID=0x%x deviceID=0x%x",
		     eth_dev->data->port_id, pci_dev->id.vendor_id,
		     pci_dev->id.device_id);

	rte_intr_callback_register(intr_handle,
				   txgbe_dev_interrupt_handler, eth_dev);

	/* enable uio/vfio intr/eventfd mapping */
	rte_intr_enable(intr_handle);

	/* enable support intr */
	txgbe_enable_intr(eth_dev);

	return 0;
}

static int
eth_txgbe_dev_uninit(struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	txgbe_dev_close(eth_dev);

	return 0;
}

static int
eth_txgbe_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	struct rte_eth_dev *pf_ethdev;
	struct rte_eth_devargs eth_da;
	int retval;

	if (pci_dev->device.devargs) {
		retval = rte_eth_devargs_parse(pci_dev->device.devargs->args,
				&eth_da);
		if (retval)
			return retval;
	} else {
		memset(&eth_da, 0, sizeof(eth_da));
	}

	retval = rte_eth_dev_create(&pci_dev->device, pci_dev->device.name,
			sizeof(struct txgbe_adapter),
			eth_dev_pci_specific_init, pci_dev,
			eth_txgbe_dev_init, NULL);

	if (retval || eth_da.nb_representor_ports < 1)
		return retval;

	pf_ethdev = rte_eth_dev_allocated(pci_dev->device.name);
	if (pf_ethdev == NULL)
		return -ENODEV;

	return 0;
}

static int eth_txgbe_pci_remove(struct rte_pci_device *pci_dev)
{
	struct rte_eth_dev *ethdev;

	ethdev = rte_eth_dev_allocated(pci_dev->device.name);
	if (!ethdev)
		return -ENODEV;

	return rte_eth_dev_destroy(ethdev, eth_txgbe_dev_uninit);
}

static struct rte_pci_driver rte_txgbe_pmd = {
	.id_table = pci_id_txgbe_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING |
		     RTE_PCI_DRV_INTR_LSC,
	.probe = eth_txgbe_pci_probe,
	.remove = eth_txgbe_pci_remove,
};


static void
txgbe_dev_phy_intr_setup(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	uint32_t gpie;

	gpie = rd32(hw, TXGBE_GPIOINTEN);
	gpie |= TXGBE_GPIOBIT_6;
	wr32(hw, TXGBE_GPIOINTEN, gpie);
	intr->mask_misc |= TXGBE_ICRMISC_GPIO;
}

/*
 * Reset and stop device.
 */
static int
txgbe_dev_close(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	int retries = 0;
	int ret;

	PMD_INIT_FUNC_TRACE();

	/* disable uio intr before callback unregister */
	rte_intr_disable(intr_handle);

	do {
		ret = rte_intr_callback_unregister(intr_handle,
				txgbe_dev_interrupt_handler, dev);
		if (ret >= 0 || ret == -ENOENT) {
			break;
		} else if (ret != -EAGAIN) {
			PMD_INIT_LOG(ERR,
				"intr callback unregister failed: %d",
				ret);
		}
		rte_delay_ms(100);
	} while (retries++ < (10 + TXGBE_LINK_UP_TIME));

	/* cancel the delay handler before remove dev */
	rte_eal_alarm_cancel(txgbe_dev_interrupt_delayed_handler, dev);

	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;

	rte_free(dev->data->hash_mac_addrs);
	dev->data->hash_mac_addrs = NULL;

	return 0;
}

static int
txgbe_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	dev_info->max_rx_queues = (uint16_t)hw->mac.max_rx_queues;
	dev_info->max_tx_queues = (uint16_t)hw->mac.max_tx_queues;
	dev_info->min_rx_bufsize = 1024;
	dev_info->max_rx_pktlen = 15872;
	dev_info->max_mac_addrs = hw->mac.num_rar_entries;
	dev_info->max_hash_mac_addrs = TXGBE_VMDQ_NUM_UC_MAC;
	dev_info->max_vfs = pci_dev->max_vfs;
	dev_info->max_vmdq_pools = ETH_64_POOLS;
	dev_info->vmdq_queue_num = dev_info->max_rx_queues;
	dev_info->rx_queue_offload_capa = txgbe_get_rx_queue_offloads(dev);
	dev_info->rx_offload_capa = (txgbe_get_rx_port_offloads(dev) |
				     dev_info->rx_queue_offload_capa);
	dev_info->tx_queue_offload_capa = txgbe_get_tx_queue_offloads(dev);
	dev_info->tx_offload_capa = txgbe_get_tx_port_offloads(dev);

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_thresh = {
			.pthresh = TXGBE_DEFAULT_RX_PTHRESH,
			.hthresh = TXGBE_DEFAULT_RX_HTHRESH,
			.wthresh = TXGBE_DEFAULT_RX_WTHRESH,
		},
		.rx_free_thresh = TXGBE_DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
		.offloads = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = TXGBE_DEFAULT_TX_PTHRESH,
			.hthresh = TXGBE_DEFAULT_TX_HTHRESH,
			.wthresh = TXGBE_DEFAULT_TX_WTHRESH,
		},
		.tx_free_thresh = TXGBE_DEFAULT_TX_FREE_THRESH,
		.offloads = 0,
	};

	dev_info->rx_desc_lim = rx_desc_lim;
	dev_info->tx_desc_lim = tx_desc_lim;

	dev_info->hash_key_size = TXGBE_HKEY_MAX_INDEX * sizeof(uint32_t);
	dev_info->reta_size = ETH_RSS_RETA_SIZE_128;
	dev_info->flow_type_rss_offloads = TXGBE_RSS_OFFLOAD_ALL;

	dev_info->speed_capa = ETH_LINK_SPEED_1G | ETH_LINK_SPEED_10G;
	dev_info->speed_capa |= ETH_LINK_SPEED_100M;

	/* Driver-preferred Rx/Tx parameters */
	dev_info->default_rxportconf.burst_size = 32;
	dev_info->default_txportconf.burst_size = 32;
	dev_info->default_rxportconf.nb_queues = 1;
	dev_info->default_txportconf.nb_queues = 1;
	dev_info->default_rxportconf.ring_size = 256;
	dev_info->default_txportconf.ring_size = 256;

	return 0;
}

static int
txgbe_dev_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(wait_to_complete);
	return 0;
}

/**
 * It clears the interrupt causes and enables the interrupt.
 * It will be called once only during nic initialized.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 * @param on
 *  Enable or Disable.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
txgbe_dev_lsc_interrupt_setup(struct rte_eth_dev *dev, uint8_t on)
{
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);

	txgbe_dev_link_status_print(dev);
	if (on)
		intr->mask_misc |= TXGBE_ICRMISC_LSC;
	else
		intr->mask_misc &= ~TXGBE_ICRMISC_LSC;

	return 0;
}

/**
 * It clears the interrupt causes and enables the interrupt.
 * It will be called once only during nic initialized.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
txgbe_dev_rxq_interrupt_setup(struct rte_eth_dev *dev)
{
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);

	intr->mask[0] |= TXGBE_ICR_MASK;
	intr->mask[1] |= TXGBE_ICR_MASK;

	return 0;
}

/**
 * It clears the interrupt causes and enables the interrupt.
 * It will be called once only during nic initialized.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
txgbe_dev_macsec_interrupt_setup(struct rte_eth_dev *dev)
{
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);

	intr->mask_misc |= TXGBE_ICRMISC_LNKSEC;

	return 0;
}

/*
 * It reads ICR and sets flag (TXGBE_ICRMISC_LSC) for the link_update.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
txgbe_dev_interrupt_get_status(struct rte_eth_dev *dev)
{
	uint32_t eicr;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);

	/* clear all cause mask */
	txgbe_disable_intr(hw);

	/* read-on-clear nic registers here */
	eicr = ((u32 *)hw->isb_mem)[TXGBE_ISB_MISC];
	PMD_DRV_LOG(DEBUG, "eicr %x", eicr);

	intr->flags = 0;

	/* set flag for async link update */
	if (eicr & TXGBE_ICRMISC_LSC)
		intr->flags |= TXGBE_FLAG_NEED_LINK_UPDATE;

	if (eicr & TXGBE_ICRMISC_VFMBX)
		intr->flags |= TXGBE_FLAG_MAILBOX;

	if (eicr & TXGBE_ICRMISC_LNKSEC)
		intr->flags |= TXGBE_FLAG_MACSEC;

	if (eicr & TXGBE_ICRMISC_GPIO)
		intr->flags |= TXGBE_FLAG_PHY_INTERRUPT;

	return 0;
}

/**
 * It gets and then prints the link status.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static void
txgbe_dev_link_status_print(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_eth_link link;

	rte_eth_linkstatus_get(dev, &link);

	if (link.link_status) {
		PMD_INIT_LOG(INFO, "Port %d: Link Up - speed %u Mbps - %s",
					(int)(dev->data->port_id),
					(unsigned int)link.link_speed,
			link.link_duplex == ETH_LINK_FULL_DUPLEX ?
					"full-duplex" : "half-duplex");
	} else {
		PMD_INIT_LOG(INFO, " Port %d: Link Down",
				(int)(dev->data->port_id));
	}
	PMD_INIT_LOG(DEBUG, "PCI Address: " PCI_PRI_FMT,
				pci_dev->addr.domain,
				pci_dev->addr.bus,
				pci_dev->addr.devid,
				pci_dev->addr.function);
}

/*
 * It executes link_update after knowing an interrupt occurred.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
txgbe_dev_interrupt_action(struct rte_eth_dev *dev,
			   struct rte_intr_handle *intr_handle)
{
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	int64_t timeout;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	PMD_DRV_LOG(DEBUG, "intr action type %d", intr->flags);

	if (intr->flags & TXGBE_FLAG_MAILBOX)
		intr->flags &= ~TXGBE_FLAG_MAILBOX;

	if (intr->flags & TXGBE_FLAG_PHY_INTERRUPT) {
		hw->phy.handle_lasi(hw);
		intr->flags &= ~TXGBE_FLAG_PHY_INTERRUPT;
	}

	if (intr->flags & TXGBE_FLAG_NEED_LINK_UPDATE) {
		struct rte_eth_link link;

		/*get the link status before link update, for predicting later*/
		rte_eth_linkstatus_get(dev, &link);

		txgbe_dev_link_update(dev, 0);

		/* likely to up */
		if (!link.link_status)
			/* handle it 1 sec later, wait it being stable */
			timeout = TXGBE_LINK_UP_CHECK_TIMEOUT;
		/* likely to down */
		else
			/* handle it 4 sec later, wait it being stable */
			timeout = TXGBE_LINK_DOWN_CHECK_TIMEOUT;

		txgbe_dev_link_status_print(dev);
		if (rte_eal_alarm_set(timeout * 1000,
				      txgbe_dev_interrupt_delayed_handler,
				      (void *)dev) < 0) {
			PMD_DRV_LOG(ERR, "Error setting alarm");
		} else {
			/* remember original mask */
			intr->mask_misc_orig = intr->mask_misc;
			/* only disable lsc interrupt */
			intr->mask_misc &= ~TXGBE_ICRMISC_LSC;
		}
	}

	PMD_DRV_LOG(DEBUG, "enable intr immediately");
	txgbe_enable_intr(dev);
	rte_intr_enable(intr_handle);

	return 0;
}

/**
 * Interrupt handler which shall be registered for alarm callback for delayed
 * handling specific interrupt to wait for the stable nic state. As the
 * NIC interrupt state is not stable for txgbe after link is just down,
 * it needs to wait 4 seconds to get the stable status.
 *
 * @param handle
 *  Pointer to interrupt handle.
 * @param param
 *  The address of parameter (struct rte_eth_dev *) registered before.
 *
 * @return
 *  void
 */
static void
txgbe_dev_interrupt_delayed_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t eicr;

	txgbe_disable_intr(hw);

	eicr = ((u32 *)hw->isb_mem)[TXGBE_ISB_MISC];

	if (intr->flags & TXGBE_FLAG_PHY_INTERRUPT) {
		hw->phy.handle_lasi(hw);
		intr->flags &= ~TXGBE_FLAG_PHY_INTERRUPT;
	}

	if (intr->flags & TXGBE_FLAG_NEED_LINK_UPDATE) {
		txgbe_dev_link_update(dev, 0);
		intr->flags &= ~TXGBE_FLAG_NEED_LINK_UPDATE;
		txgbe_dev_link_status_print(dev);
		rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC,
					      NULL);
	}

	if (intr->flags & TXGBE_FLAG_MACSEC) {
		rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_MACSEC,
					      NULL);
		intr->flags &= ~TXGBE_FLAG_MACSEC;
	}

	/* restore original mask */
	intr->mask_misc = intr->mask_misc_orig;
	intr->mask_misc_orig = 0;

	PMD_DRV_LOG(DEBUG, "enable intr in delayed handler S[%08x]", eicr);
	txgbe_enable_intr(dev);
	rte_intr_enable(intr_handle);
}

/**
 * Interrupt handler triggered by NIC  for handling
 * specific interrupt.
 *
 * @param handle
 *  Pointer to interrupt handle.
 * @param param
 *  The address of parameter (struct rte_eth_dev *) registered before.
 *
 * @return
 *  void
 */
static void
txgbe_dev_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;

	txgbe_dev_interrupt_get_status(dev);
	txgbe_dev_interrupt_action(dev, dev->intr_handle);
}

/**
 * set the IVAR registers, mapping interrupt causes to vectors
 * @param hw
 *  pointer to txgbe_hw struct
 * @direction
 *  0 for Rx, 1 for Tx, -1 for other causes
 * @queue
 *  queue to map the corresponding interrupt to
 * @msix_vector
 *  the vector to map to the corresponding queue
 */
void
txgbe_set_ivar_map(struct txgbe_hw *hw, int8_t direction,
		   uint8_t queue, uint8_t msix_vector)
{
	uint32_t tmp, idx;

	if (direction == -1) {
		/* other causes */
		msix_vector |= TXGBE_IVARMISC_VLD;
		idx = 0;
		tmp = rd32(hw, TXGBE_IVARMISC);
		tmp &= ~(0xFF << idx);
		tmp |= (msix_vector << idx);
		wr32(hw, TXGBE_IVARMISC, tmp);
	} else {
		/* rx or tx causes */
		/* Workround for ICR lost */
		idx = ((16 * (queue & 1)) + (8 * direction));
		tmp = rd32(hw, TXGBE_IVAR(queue >> 1));
		tmp &= ~(0xFF << idx);
		tmp |= (msix_vector << idx);
		wr32(hw, TXGBE_IVAR(queue >> 1), tmp);
	}
}

/**
 * Sets up the hardware to properly generate MSI-X interrupts
 * @hw
 *  board private structure
 */
static void
txgbe_configure_msix(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t queue_id, base = TXGBE_MISC_VEC_ID;
	uint32_t vec = TXGBE_MISC_VEC_ID;
	uint32_t gpie;

	/* won't configure msix register if no mapping is done
	 * between intr vector and event fd
	 * but if misx has been enabled already, need to configure
	 * auto clean, auto mask and throttling.
	 */
	gpie = rd32(hw, TXGBE_GPIE);
	if (!rte_intr_dp_is_en(intr_handle) &&
	    !(gpie & TXGBE_GPIE_MSIX))
		return;

	if (rte_intr_allow_others(intr_handle)) {
		base = TXGBE_RX_VEC_START;
		vec = base;
	}

	/* setup GPIE for MSI-x mode */
	gpie = rd32(hw, TXGBE_GPIE);
	gpie |= TXGBE_GPIE_MSIX;
	wr32(hw, TXGBE_GPIE, gpie);

	/* Populate the IVAR table and set the ITR values to the
	 * corresponding register.
	 */
	if (rte_intr_dp_is_en(intr_handle)) {
		for (queue_id = 0; queue_id < dev->data->nb_rx_queues;
			queue_id++) {
			/* by default, 1:1 mapping */
			txgbe_set_ivar_map(hw, 0, queue_id, vec);
			intr_handle->intr_vec[queue_id] = vec;
			if (vec < base + intr_handle->nb_efd - 1)
				vec++;
		}

		txgbe_set_ivar_map(hw, -1, 1, TXGBE_MISC_VEC_ID);
	}
	wr32(hw, TXGBE_ITR(TXGBE_MISC_VEC_ID),
			TXGBE_ITR_IVAL_10G(TXGBE_QUEUE_ITR_INTERVAL_DEFAULT)
			| TXGBE_ITR_WRDSA);
}

static const struct eth_dev_ops txgbe_eth_dev_ops = {
	.dev_infos_get              = txgbe_dev_info_get,
};

RTE_PMD_REGISTER_PCI(net_txgbe, rte_txgbe_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_txgbe, pci_id_txgbe_map);
RTE_PMD_REGISTER_KMOD_DEP(net_txgbe, "* igb_uio | uio_pci_generic | vfio-pci");

RTE_LOG_REGISTER(txgbe_logtype_init, pmd.net.txgbe.init, NOTICE);
RTE_LOG_REGISTER(txgbe_logtype_driver, pmd.net.txgbe.driver, NOTICE);

#ifdef RTE_LIBRTE_TXGBE_DEBUG_RX
	RTE_LOG_REGISTER(txgbe_logtype_rx, pmd.net.txgbe.rx, DEBUG);
#endif
#ifdef RTE_LIBRTE_TXGBE_DEBUG_TX
	RTE_LOG_REGISTER(txgbe_logtype_tx, pmd.net.txgbe.tx, DEBUG);
#endif

#ifdef RTE_LIBRTE_TXGBE_DEBUG_TX_FREE
	RTE_LOG_REGISTER(txgbe_logtype_tx_free, pmd.net.txgbe.tx_free, DEBUG);
#endif
