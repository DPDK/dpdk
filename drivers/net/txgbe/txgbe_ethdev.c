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
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_alarm.h>

#include "txgbe_logs.h"
#include "base/txgbe.h"
#include "txgbe_ethdev.h"
#include "txgbe_rxtx.h"

static int  txgbe_dev_set_link_up(struct rte_eth_dev *dev);
static int  txgbe_dev_set_link_down(struct rte_eth_dev *dev);
static int txgbe_dev_close(struct rte_eth_dev *dev);
static int txgbe_dev_link_update(struct rte_eth_dev *dev,
				int wait_to_complete);

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

static inline int32_t
txgbe_pf_reset_hw(struct txgbe_hw *hw)
{
	uint32_t ctrl_ext;
	int32_t status;

	status = hw->mac.reset_hw(hw);

	ctrl_ext = rd32(hw, TXGBE_PORTCTL);
	/* Set PF Reset Done bit so PF/VF Mail Ops can work */
	ctrl_ext |= TXGBE_PORTCTL_RSTDONE;
	wr32(hw, TXGBE_PORTCTL, ctrl_ext);
	txgbe_flush(hw);

	if (status == TXGBE_ERR_SFP_NOT_PRESENT)
		status = 0;
	return status;
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
	eth_dev->rx_pkt_burst = &txgbe_recv_pkts;
	eth_dev->tx_pkt_burst = &txgbe_xmit_pkts;
	eth_dev->tx_pkt_prepare = &txgbe_prep_pkts;

	/*
	 * For secondary processes, we don't initialise any further as primary
	 * has already done this work. Only check we don't need a different
	 * RX and TX function.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		struct txgbe_tx_queue *txq;
		/* TX queue function in primary, set by last queue initialized
		 * Tx queue may not initialized by primary process
		 */
		if (eth_dev->data->tx_queues) {
			uint16_t nb_tx_queues = eth_dev->data->nb_tx_queues;
			txq = eth_dev->data->tx_queues[nb_tx_queues - 1];
			txgbe_set_tx_function(eth_dev, txq);
		} else {
			/* Use default TX function if we get here */
			PMD_INIT_LOG(NOTICE, "No TX queues configured yet. "
				     "Using default TX function.");
		}

		txgbe_set_rx_function(eth_dev);

		return 0;
	}

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

static int
txgbe_check_vf_rss_rxq_num(struct rte_eth_dev *dev, uint16_t nb_rx_q)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	switch (nb_rx_q) {
	case 1:
	case 2:
		RTE_ETH_DEV_SRIOV(dev).active = ETH_64_POOLS;
		break;
	case 4:
		RTE_ETH_DEV_SRIOV(dev).active = ETH_32_POOLS;
		break;
	default:
		return -EINVAL;
	}

	RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool =
		TXGBE_MAX_RX_QUEUE_NUM / RTE_ETH_DEV_SRIOV(dev).active;
	RTE_ETH_DEV_SRIOV(dev).def_pool_q_idx =
		pci_dev->max_vfs * RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool;
	return 0;
}

static int
txgbe_check_mq_mode(struct rte_eth_dev *dev)
{
	struct rte_eth_conf *dev_conf = &dev->data->dev_conf;
	uint16_t nb_rx_q = dev->data->nb_rx_queues;
	uint16_t nb_tx_q = dev->data->nb_tx_queues;

	if (RTE_ETH_DEV_SRIOV(dev).active != 0) {
		/* check multi-queue mode */
		switch (dev_conf->rxmode.mq_mode) {
		case ETH_MQ_RX_VMDQ_DCB:
			PMD_INIT_LOG(INFO, "ETH_MQ_RX_VMDQ_DCB mode supported in SRIOV");
			break;
		case ETH_MQ_RX_VMDQ_DCB_RSS:
			/* DCB/RSS VMDQ in SRIOV mode, not implement yet */
			PMD_INIT_LOG(ERR, "SRIOV active,"
					" unsupported mq_mode rx %d.",
					dev_conf->rxmode.mq_mode);
			return -EINVAL;
		case ETH_MQ_RX_RSS:
		case ETH_MQ_RX_VMDQ_RSS:
			dev->data->dev_conf.rxmode.mq_mode = ETH_MQ_RX_VMDQ_RSS;
			if (nb_rx_q <= RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool)
				if (txgbe_check_vf_rss_rxq_num(dev, nb_rx_q)) {
					PMD_INIT_LOG(ERR, "SRIOV is active,"
						" invalid queue number"
						" for VMDQ RSS, allowed"
						" value are 1, 2 or 4.");
					return -EINVAL;
				}
			break;
		case ETH_MQ_RX_VMDQ_ONLY:
		case ETH_MQ_RX_NONE:
			/* if nothing mq mode configure, use default scheme */
			dev->data->dev_conf.rxmode.mq_mode =
				ETH_MQ_RX_VMDQ_ONLY;
			break;
		default: /* ETH_MQ_RX_DCB, ETH_MQ_RX_DCB_RSS or ETH_MQ_TX_DCB*/
			/* SRIOV only works in VMDq enable mode */
			PMD_INIT_LOG(ERR, "SRIOV is active,"
					" wrong mq_mode rx %d.",
					dev_conf->rxmode.mq_mode);
			return -EINVAL;
		}

		switch (dev_conf->txmode.mq_mode) {
		case ETH_MQ_TX_VMDQ_DCB:
			PMD_INIT_LOG(INFO, "ETH_MQ_TX_VMDQ_DCB mode supported in SRIOV");
			dev->data->dev_conf.txmode.mq_mode = ETH_MQ_TX_VMDQ_DCB;
			break;
		default: /* ETH_MQ_TX_VMDQ_ONLY or ETH_MQ_TX_NONE */
			dev->data->dev_conf.txmode.mq_mode =
				ETH_MQ_TX_VMDQ_ONLY;
			break;
		}

		/* check valid queue number */
		if ((nb_rx_q > RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool) ||
		    (nb_tx_q > RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool)) {
			PMD_INIT_LOG(ERR, "SRIOV is active,"
					" nb_rx_q=%d nb_tx_q=%d queue number"
					" must be less than or equal to %d.",
					nb_rx_q, nb_tx_q,
					RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool);
			return -EINVAL;
		}
	} else {
		if (dev_conf->rxmode.mq_mode == ETH_MQ_RX_VMDQ_DCB_RSS) {
			PMD_INIT_LOG(ERR, "VMDQ+DCB+RSS mq_mode is"
					  " not supported.");
			return -EINVAL;
		}
		/* check configuration for vmdb+dcb mode */
		if (dev_conf->rxmode.mq_mode == ETH_MQ_RX_VMDQ_DCB) {
			const struct rte_eth_vmdq_dcb_conf *conf;

			if (nb_rx_q != TXGBE_VMDQ_DCB_NB_QUEUES) {
				PMD_INIT_LOG(ERR, "VMDQ+DCB, nb_rx_q != %d.",
						TXGBE_VMDQ_DCB_NB_QUEUES);
				return -EINVAL;
			}
			conf = &dev_conf->rx_adv_conf.vmdq_dcb_conf;
			if (!(conf->nb_queue_pools == ETH_16_POOLS ||
			       conf->nb_queue_pools == ETH_32_POOLS)) {
				PMD_INIT_LOG(ERR, "VMDQ+DCB selected,"
						" nb_queue_pools must be %d or %d.",
						ETH_16_POOLS, ETH_32_POOLS);
				return -EINVAL;
			}
		}
		if (dev_conf->txmode.mq_mode == ETH_MQ_TX_VMDQ_DCB) {
			const struct rte_eth_vmdq_dcb_tx_conf *conf;

			if (nb_tx_q != TXGBE_VMDQ_DCB_NB_QUEUES) {
				PMD_INIT_LOG(ERR, "VMDQ+DCB, nb_tx_q != %d",
						 TXGBE_VMDQ_DCB_NB_QUEUES);
				return -EINVAL;
			}
			conf = &dev_conf->tx_adv_conf.vmdq_dcb_tx_conf;
			if (!(conf->nb_queue_pools == ETH_16_POOLS ||
			       conf->nb_queue_pools == ETH_32_POOLS)) {
				PMD_INIT_LOG(ERR, "VMDQ+DCB selected,"
						" nb_queue_pools != %d and"
						" nb_queue_pools != %d.",
						ETH_16_POOLS, ETH_32_POOLS);
				return -EINVAL;
			}
		}

		/* For DCB mode check our configuration before we go further */
		if (dev_conf->rxmode.mq_mode == ETH_MQ_RX_DCB) {
			const struct rte_eth_dcb_rx_conf *conf;

			conf = &dev_conf->rx_adv_conf.dcb_rx_conf;
			if (!(conf->nb_tcs == ETH_4_TCS ||
			       conf->nb_tcs == ETH_8_TCS)) {
				PMD_INIT_LOG(ERR, "DCB selected, nb_tcs != %d"
						" and nb_tcs != %d.",
						ETH_4_TCS, ETH_8_TCS);
				return -EINVAL;
			}
		}

		if (dev_conf->txmode.mq_mode == ETH_MQ_TX_DCB) {
			const struct rte_eth_dcb_tx_conf *conf;

			conf = &dev_conf->tx_adv_conf.dcb_tx_conf;
			if (!(conf->nb_tcs == ETH_4_TCS ||
			       conf->nb_tcs == ETH_8_TCS)) {
				PMD_INIT_LOG(ERR, "DCB selected, nb_tcs != %d"
						" and nb_tcs != %d.",
						ETH_4_TCS, ETH_8_TCS);
				return -EINVAL;
			}
		}
	}
	return 0;
}

static int
txgbe_dev_configure(struct rte_eth_dev *dev)
{
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	struct txgbe_adapter *adapter = TXGBE_DEV_ADAPTER(dev);
	int ret;

	PMD_INIT_FUNC_TRACE();

	if (dev->data->dev_conf.rxmode.mq_mode & ETH_MQ_RX_RSS_FLAG)
		dev->data->dev_conf.rxmode.offloads |= DEV_RX_OFFLOAD_RSS_HASH;

	/* multiple queue mode checking */
	ret  = txgbe_check_mq_mode(dev);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "txgbe_check_mq_mode fails with %d.",
			    ret);
		return ret;
	}

	/* set flag to update link status after init */
	intr->flags |= TXGBE_FLAG_NEED_LINK_UPDATE;

	/*
	 * Initialize to TRUE. If any of Rx queues doesn't meet the bulk
	 * allocation Rx preconditions we will reset it.
	 */
	adapter->rx_bulk_alloc_allowed = true;

	return 0;
}

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
 * Configure device link speed and setup link.
 * It returns 0 on success.
 */
static int
txgbe_dev_start(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	uint32_t intr_vector = 0;
	int err;
	bool link_up = false, negotiate = 0;
	uint32_t speed = 0;
	uint32_t allowed_speeds = 0;
	int status;
	uint32_t *link_speeds;

	PMD_INIT_FUNC_TRACE();

	/* TXGBE devices don't support:
	 *    - half duplex (checked afterwards for valid speeds)
	 *    - fixed speed: TODO implement
	 */
	if (dev->data->dev_conf.link_speeds & ETH_LINK_SPEED_FIXED) {
		PMD_INIT_LOG(ERR,
		"Invalid link_speeds for port %u, fix speed not supported",
				dev->data->port_id);
		return -EINVAL;
	}

	/* Stop the link setup handler before resetting the HW. */
	rte_eal_alarm_cancel(txgbe_dev_setup_link_alarm_handler, dev);

	/* disable uio/vfio intr/eventfd mapping */
	rte_intr_disable(intr_handle);

	/* stop adapter */
	hw->adapter_stopped = 0;
	txgbe_stop_hw(hw);

	/* reinitialize adapter
	 * this calls reset and start
	 */
	hw->nb_rx_queues = dev->data->nb_rx_queues;
	hw->nb_tx_queues = dev->data->nb_tx_queues;
	status = txgbe_pf_reset_hw(hw);
	if (status != 0)
		return -1;
	hw->mac.start_hw(hw);
	hw->mac.get_link_status = true;

	txgbe_dev_phy_intr_setup(dev);

	/* check and configure queue intr-vector mapping */
	if ((rte_intr_cap_multiple(intr_handle) ||
	     !RTE_ETH_DEV_SRIOV(dev).active) &&
	    dev->data->dev_conf.intr_conf.rxq != 0) {
		intr_vector = dev->data->nb_rx_queues;
		if (rte_intr_efd_enable(intr_handle, intr_vector))
			return -1;
	}

	if (rte_intr_dp_is_en(intr_handle) && !intr_handle->intr_vec) {
		intr_handle->intr_vec =
			rte_zmalloc("intr_vec",
				    dev->data->nb_rx_queues * sizeof(int), 0);
		if (intr_handle->intr_vec == NULL) {
			PMD_INIT_LOG(ERR, "Failed to allocate %d rx_queues"
				     " intr_vec", dev->data->nb_rx_queues);
			return -ENOMEM;
		}
	}

	/* confiugre msix for sleep until rx interrupt */
	txgbe_configure_msix(dev);

	/* initialize transmission unit */
	txgbe_dev_tx_init(dev);

	/* This can fail when allocating mbufs for descriptor rings */
	err = txgbe_dev_rx_init(dev);
	if (err) {
		PMD_INIT_LOG(ERR, "Unable to initialize RX hardware");
		goto error;
	}

	err = txgbe_dev_rxtx_start(dev);
	if (err < 0) {
		PMD_INIT_LOG(ERR, "Unable to start rxtx queues");
		goto error;
	}

	/* Skip link setup if loopback mode is enabled. */
	if (hw->mac.type == txgbe_mac_raptor &&
	    dev->data->dev_conf.lpbk_mode)
		goto skip_link_setup;

	if (txgbe_is_sfp(hw) && hw->phy.multispeed_fiber) {
		err = hw->mac.setup_sfp(hw);
		if (err)
			goto error;
	}

	if (hw->phy.media_type == txgbe_media_type_copper) {
		/* Turn on the copper */
		hw->phy.set_phy_power(hw, true);
	} else {
		/* Turn on the laser */
		hw->mac.enable_tx_laser(hw);
	}

	err = hw->mac.check_link(hw, &speed, &link_up, 0);
	if (err)
		goto error;
	dev->data->dev_link.link_status = link_up;

	err = hw->mac.get_link_capabilities(hw, &speed, &negotiate);
	if (err)
		goto error;

	allowed_speeds = ETH_LINK_SPEED_100M | ETH_LINK_SPEED_1G |
			ETH_LINK_SPEED_10G;

	link_speeds = &dev->data->dev_conf.link_speeds;
	if (*link_speeds & ~allowed_speeds) {
		PMD_INIT_LOG(ERR, "Invalid link setting");
		goto error;
	}

	speed = 0x0;
	if (*link_speeds == ETH_LINK_SPEED_AUTONEG) {
		speed = (TXGBE_LINK_SPEED_100M_FULL |
			 TXGBE_LINK_SPEED_1GB_FULL |
			 TXGBE_LINK_SPEED_10GB_FULL);
	} else {
		if (*link_speeds & ETH_LINK_SPEED_10G)
			speed |= TXGBE_LINK_SPEED_10GB_FULL;
		if (*link_speeds & ETH_LINK_SPEED_5G)
			speed |= TXGBE_LINK_SPEED_5GB_FULL;
		if (*link_speeds & ETH_LINK_SPEED_2_5G)
			speed |= TXGBE_LINK_SPEED_2_5GB_FULL;
		if (*link_speeds & ETH_LINK_SPEED_1G)
			speed |= TXGBE_LINK_SPEED_1GB_FULL;
		if (*link_speeds & ETH_LINK_SPEED_100M)
			speed |= TXGBE_LINK_SPEED_100M_FULL;
	}

	err = hw->mac.setup_link(hw, speed, link_up);
	if (err)
		goto error;

skip_link_setup:

	if (rte_intr_allow_others(intr_handle)) {
		/* check if lsc interrupt is enabled */
		if (dev->data->dev_conf.intr_conf.lsc != 0)
			txgbe_dev_lsc_interrupt_setup(dev, TRUE);
		else
			txgbe_dev_lsc_interrupt_setup(dev, FALSE);
		txgbe_dev_macsec_interrupt_setup(dev);
		txgbe_set_ivar_map(hw, -1, 1, TXGBE_MISC_VEC_ID);
	} else {
		rte_intr_callback_unregister(intr_handle,
					     txgbe_dev_interrupt_handler, dev);
		if (dev->data->dev_conf.intr_conf.lsc != 0)
			PMD_INIT_LOG(INFO, "lsc won't enable because of"
				     " no intr multiplex");
	}

	/* check if rxq interrupt is enabled */
	if (dev->data->dev_conf.intr_conf.rxq != 0 &&
	    rte_intr_dp_is_en(intr_handle))
		txgbe_dev_rxq_interrupt_setup(dev);

	/* enable uio/vfio intr/eventfd mapping */
	rte_intr_enable(intr_handle);

	/* resume enabled intr since hw reset */
	txgbe_enable_intr(dev);

	/*
	 * Update link status right before return, because it may
	 * start link configuration process in a separate thread.
	 */
	txgbe_dev_link_update(dev, 0);

	wr32m(hw, TXGBE_LEDCTL, 0xFFFFFFFF, TXGBE_LEDCTL_ORD_MASK);

	return 0;

error:
	PMD_INIT_LOG(ERR, "failure in dev start: %d", err);
	txgbe_dev_clear_queues(dev);
	return -EIO;
}

/*
 * Stop device: disable rx and tx functions to allow for reconfiguring.
 */
static int
txgbe_dev_stop(struct rte_eth_dev *dev)
{
	struct rte_eth_link link;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;

	if (hw->adapter_stopped)
		return 0;

	PMD_INIT_FUNC_TRACE();

	rte_eal_alarm_cancel(txgbe_dev_setup_link_alarm_handler, dev);

	/* disable interrupts */
	txgbe_disable_intr(hw);

	/* reset the NIC */
	txgbe_pf_reset_hw(hw);
	hw->adapter_stopped = 0;

	/* stop adapter */
	txgbe_stop_hw(hw);

	if (hw->phy.media_type == txgbe_media_type_copper) {
		/* Turn off the copper */
		hw->phy.set_phy_power(hw, false);
	} else {
		/* Turn off the laser */
		hw->mac.disable_tx_laser(hw);
	}

	txgbe_dev_clear_queues(dev);

	/* Clear stored conf */
	dev->data->scattered_rx = 0;
	dev->data->lro = 0;

	/* Clear recorded link status */
	memset(&link, 0, sizeof(link));
	rte_eth_linkstatus_set(dev, &link);

	if (!rte_intr_allow_others(intr_handle))
		/* resume to the default handler */
		rte_intr_callback_register(intr_handle,
					   txgbe_dev_interrupt_handler,
					   (void *)dev);

	/* Clean datapath event and queue/vec mapping */
	rte_intr_efd_disable(intr_handle);
	if (intr_handle->intr_vec != NULL) {
		rte_free(intr_handle->intr_vec);
		intr_handle->intr_vec = NULL;
	}

	wr32m(hw, TXGBE_LEDCTL, 0xFFFFFFFF, TXGBE_LEDCTL_SEL_MASK);

	hw->adapter_stopped = true;
	dev->data->dev_started = 0;

	return 0;
}

/*
 * Set device link up: enable tx.
 */
static int
txgbe_dev_set_link_up(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	if (hw->phy.media_type == txgbe_media_type_copper) {
		/* Turn on the copper */
		hw->phy.set_phy_power(hw, true);
	} else {
		/* Turn on the laser */
		hw->mac.enable_tx_laser(hw);
		txgbe_dev_link_update(dev, 0);
	}

	return 0;
}

/*
 * Set device link down: disable tx.
 */
static int
txgbe_dev_set_link_down(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	if (hw->phy.media_type == txgbe_media_type_copper) {
		/* Turn off the copper */
		hw->phy.set_phy_power(hw, false);
	} else {
		/* Turn off the laser */
		hw->mac.disable_tx_laser(hw);
		txgbe_dev_link_update(dev, 0);
	}

	return 0;
}

/*
 * Reset and stop device.
 */
static int
txgbe_dev_close(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	int retries = 0;
	int ret;

	PMD_INIT_FUNC_TRACE();

	txgbe_pf_reset_hw(hw);

	ret = txgbe_dev_stop(dev);

	txgbe_dev_free_queues(dev);

	/* reprogram the RAR[0] in case user changed it. */
	txgbe_set_rar(hw, 0, hw->mac.addr, 0, true);

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

	return ret;
}

/*
 * Reset PF device.
 */
static int
txgbe_dev_reset(struct rte_eth_dev *dev)
{
	int ret;

	/* When a DPDK PMD PF begin to reset PF port, it should notify all
	 * its VF to make them align with it. The detailed notification
	 * mechanism is PMD specific. As to txgbe PF, it is rather complex.
	 * To avoid unexpected behavior in VF, currently reset of PF with
	 * SR-IOV activation is not supported. It might be supported later.
	 */
	if (dev->data->sriov.active)
		return -ENOTSUP;

	ret = eth_txgbe_dev_uninit(dev);
	if (ret)
		return ret;

	ret = eth_txgbe_dev_init(dev, NULL);

	return ret;
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

const uint32_t *
txgbe_dev_supported_ptypes_get(struct rte_eth_dev *dev)
{
	if (dev->rx_pkt_burst == txgbe_recv_pkts ||
	    dev->rx_pkt_burst == txgbe_recv_pkts_lro_single_alloc ||
	    dev->rx_pkt_burst == txgbe_recv_pkts_lro_bulk_alloc ||
	    dev->rx_pkt_burst == txgbe_recv_pkts_bulk_alloc)
		return txgbe_get_supported_ptypes();

	return NULL;
}

void
txgbe_dev_setup_link_alarm_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	u32 speed;
	bool autoneg = false;

	speed = hw->phy.autoneg_advertised;
	if (!speed)
		hw->mac.get_link_capabilities(hw, &speed, &autoneg);

	hw->mac.setup_link(hw, speed, true);

	intr->flags &= ~TXGBE_FLAG_NEED_LINK_CONFIG;
}

/* return 0 means link status changed, -1 means not changed */
int
txgbe_dev_link_update_share(struct rte_eth_dev *dev,
			    int wait_to_complete)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct rte_eth_link link;
	u32 link_speed = TXGBE_LINK_SPEED_UNKNOWN;
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	bool link_up;
	int err;
	int wait = 1;

	memset(&link, 0, sizeof(link));
	link.link_status = ETH_LINK_DOWN;
	link.link_speed = ETH_SPEED_NUM_NONE;
	link.link_duplex = ETH_LINK_HALF_DUPLEX;
	link.link_autoneg = ETH_LINK_AUTONEG;

	hw->mac.get_link_status = true;

	if (intr->flags & TXGBE_FLAG_NEED_LINK_CONFIG)
		return rte_eth_linkstatus_set(dev, &link);

	/* check if it needs to wait to complete, if lsc interrupt is enabled */
	if (wait_to_complete == 0 || dev->data->dev_conf.intr_conf.lsc != 0)
		wait = 0;

	err = hw->mac.check_link(hw, &link_speed, &link_up, wait);

	if (err != 0) {
		link.link_speed = ETH_SPEED_NUM_100M;
		link.link_duplex = ETH_LINK_FULL_DUPLEX;
		return rte_eth_linkstatus_set(dev, &link);
	}

	if (link_up == 0) {
		if (hw->phy.media_type == txgbe_media_type_fiber) {
			intr->flags |= TXGBE_FLAG_NEED_LINK_CONFIG;
			rte_eal_alarm_set(10,
				txgbe_dev_setup_link_alarm_handler, dev);
		}
		return rte_eth_linkstatus_set(dev, &link);
	}

	intr->flags &= ~TXGBE_FLAG_NEED_LINK_CONFIG;
	link.link_status = ETH_LINK_UP;
	link.link_duplex = ETH_LINK_FULL_DUPLEX;

	switch (link_speed) {
	default:
	case TXGBE_LINK_SPEED_UNKNOWN:
		link.link_duplex = ETH_LINK_FULL_DUPLEX;
		link.link_speed = ETH_SPEED_NUM_100M;
		break;

	case TXGBE_LINK_SPEED_100M_FULL:
		link.link_speed = ETH_SPEED_NUM_100M;
		break;

	case TXGBE_LINK_SPEED_1GB_FULL:
		link.link_speed = ETH_SPEED_NUM_1G;
		break;

	case TXGBE_LINK_SPEED_2_5GB_FULL:
		link.link_speed = ETH_SPEED_NUM_2_5G;
		break;

	case TXGBE_LINK_SPEED_5GB_FULL:
		link.link_speed = ETH_SPEED_NUM_5G;
		break;

	case TXGBE_LINK_SPEED_10GB_FULL:
		link.link_speed = ETH_SPEED_NUM_10G;
		break;
	}

	return rte_eth_linkstatus_set(dev, &link);
}

static int
txgbe_dev_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	return txgbe_dev_link_update_share(dev, wait_to_complete);
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

static int
txgbe_add_rar(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr,
				uint32_t index, uint32_t pool)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t enable_addr = 1;

	return txgbe_set_rar(hw, index, mac_addr->addr_bytes,
			     pool, enable_addr);
}

static void
txgbe_remove_rar(struct rte_eth_dev *dev, uint32_t index)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	txgbe_clear_rar(hw, index);
}

static int
txgbe_set_default_mac_addr(struct rte_eth_dev *dev, struct rte_ether_addr *addr)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	txgbe_remove_rar(dev, 0);
	txgbe_add_rar(dev, addr, 0, pci_dev->max_vfs);

	return 0;
}

static uint32_t
txgbe_uta_vector(struct txgbe_hw *hw, struct rte_ether_addr *uc_addr)
{
	uint32_t vector = 0;

	switch (hw->mac.mc_filter_type) {
	case 0:   /* use bits [47:36] of the address */
		vector = ((uc_addr->addr_bytes[4] >> 4) |
			(((uint16_t)uc_addr->addr_bytes[5]) << 4));
		break;
	case 1:   /* use bits [46:35] of the address */
		vector = ((uc_addr->addr_bytes[4] >> 3) |
			(((uint16_t)uc_addr->addr_bytes[5]) << 5));
		break;
	case 2:   /* use bits [45:34] of the address */
		vector = ((uc_addr->addr_bytes[4] >> 2) |
			(((uint16_t)uc_addr->addr_bytes[5]) << 6));
		break;
	case 3:   /* use bits [43:32] of the address */
		vector = ((uc_addr->addr_bytes[4]) |
			(((uint16_t)uc_addr->addr_bytes[5]) << 8));
		break;
	default:  /* Invalid mc_filter_type */
		break;
	}

	/* vector can only be 12-bits or boundary will be exceeded */
	vector &= 0xFFF;
	return vector;
}

static int
txgbe_uc_hash_table_set(struct rte_eth_dev *dev,
			struct rte_ether_addr *mac_addr, uint8_t on)
{
	uint32_t vector;
	uint32_t uta_idx;
	uint32_t reg_val;
	uint32_t uta_mask;
	uint32_t psrctl;

	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_uta_info *uta_info = TXGBE_DEV_UTA_INFO(dev);

	/* The UTA table only exists on pf hardware */
	if (hw->mac.type < txgbe_mac_raptor)
		return -ENOTSUP;

	vector = txgbe_uta_vector(hw, mac_addr);
	uta_idx = (vector >> 5) & 0x7F;
	uta_mask = 0x1UL << (vector & 0x1F);

	if (!!on == !!(uta_info->uta_shadow[uta_idx] & uta_mask))
		return 0;

	reg_val = rd32(hw, TXGBE_UCADDRTBL(uta_idx));
	if (on) {
		uta_info->uta_in_use++;
		reg_val |= uta_mask;
		uta_info->uta_shadow[uta_idx] |= uta_mask;
	} else {
		uta_info->uta_in_use--;
		reg_val &= ~uta_mask;
		uta_info->uta_shadow[uta_idx] &= ~uta_mask;
	}

	wr32(hw, TXGBE_UCADDRTBL(uta_idx), reg_val);

	psrctl = rd32(hw, TXGBE_PSRCTL);
	if (uta_info->uta_in_use > 0)
		psrctl |= TXGBE_PSRCTL_UCHFENA;
	else
		psrctl &= ~TXGBE_PSRCTL_UCHFENA;

	psrctl &= ~TXGBE_PSRCTL_ADHF12_MASK;
	psrctl |= TXGBE_PSRCTL_ADHF12(hw->mac.mc_filter_type);
	wr32(hw, TXGBE_PSRCTL, psrctl);

	return 0;
}

static int
txgbe_uc_all_hash_table_set(struct rte_eth_dev *dev, uint8_t on)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_uta_info *uta_info = TXGBE_DEV_UTA_INFO(dev);
	uint32_t psrctl;
	int i;

	/* The UTA table only exists on pf hardware */
	if (hw->mac.type < txgbe_mac_raptor)
		return -ENOTSUP;

	if (on) {
		for (i = 0; i < ETH_VMDQ_NUM_UC_HASH_ARRAY; i++) {
			uta_info->uta_shadow[i] = ~0;
			wr32(hw, TXGBE_UCADDRTBL(i), ~0);
		}
	} else {
		for (i = 0; i < ETH_VMDQ_NUM_UC_HASH_ARRAY; i++) {
			uta_info->uta_shadow[i] = 0;
			wr32(hw, TXGBE_UCADDRTBL(i), 0);
		}
	}

	psrctl = rd32(hw, TXGBE_PSRCTL);
	if (on)
		psrctl |= TXGBE_PSRCTL_UCHFENA;
	else
		psrctl &= ~TXGBE_PSRCTL_UCHFENA;

	psrctl &= ~TXGBE_PSRCTL_ADHF12_MASK;
	psrctl |= TXGBE_PSRCTL_ADHF12(hw->mac.mc_filter_type);
	wr32(hw, TXGBE_PSRCTL, psrctl);

	return 0;
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

static u8 *
txgbe_dev_addr_list_itr(__rte_unused struct txgbe_hw *hw,
			u8 **mc_addr_ptr, u32 *vmdq)
{
	u8 *mc_addr;

	*vmdq = 0;
	mc_addr = *mc_addr_ptr;
	*mc_addr_ptr = (mc_addr + sizeof(struct rte_ether_addr));
	return mc_addr;
}

int
txgbe_dev_set_mc_addr_list(struct rte_eth_dev *dev,
			  struct rte_ether_addr *mc_addr_set,
			  uint32_t nb_mc_addr)
{
	struct txgbe_hw *hw;
	u8 *mc_addr_list;

	hw = TXGBE_DEV_HW(dev);
	mc_addr_list = (u8 *)mc_addr_set;
	return txgbe_update_mc_addr_list(hw, mc_addr_list, nb_mc_addr,
					 txgbe_dev_addr_list_itr, TRUE);
}

static const struct eth_dev_ops txgbe_eth_dev_ops = {
	.dev_configure              = txgbe_dev_configure,
	.dev_infos_get              = txgbe_dev_info_get,
	.dev_start                  = txgbe_dev_start,
	.dev_stop                   = txgbe_dev_stop,
	.dev_set_link_up            = txgbe_dev_set_link_up,
	.dev_set_link_down          = txgbe_dev_set_link_down,
	.dev_close                  = txgbe_dev_close,
	.dev_reset                  = txgbe_dev_reset,
	.link_update                = txgbe_dev_link_update,
	.dev_supported_ptypes_get   = txgbe_dev_supported_ptypes_get,
	.rx_queue_start	            = txgbe_dev_rx_queue_start,
	.rx_queue_stop              = txgbe_dev_rx_queue_stop,
	.tx_queue_start	            = txgbe_dev_tx_queue_start,
	.tx_queue_stop              = txgbe_dev_tx_queue_stop,
	.rx_queue_setup             = txgbe_dev_rx_queue_setup,
	.rx_queue_release           = txgbe_dev_rx_queue_release,
	.tx_queue_setup             = txgbe_dev_tx_queue_setup,
	.tx_queue_release           = txgbe_dev_tx_queue_release,
	.mac_addr_add               = txgbe_add_rar,
	.mac_addr_remove            = txgbe_remove_rar,
	.mac_addr_set               = txgbe_set_default_mac_addr,
	.uc_hash_table_set          = txgbe_uc_hash_table_set,
	.uc_all_hash_table_set      = txgbe_uc_all_hash_table_set,
	.set_mc_addr_list           = txgbe_dev_set_mc_addr_list,
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
