/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <errno.h>
#include <rte_common.h>
#include <ethdev_pci.h>

#include <rte_alarm.h>

#include "ngbe_logs.h"
#include "ngbe.h"
#include "ngbe_ethdev.h"
#include "ngbe_rxtx.h"

static int ngbe_dev_close(struct rte_eth_dev *dev);

static void ngbe_dev_interrupt_handler(void *param);
static void ngbe_dev_interrupt_delayed_handler(void *param);

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

static const struct rte_eth_desc_lim rx_desc_lim = {
	.nb_max = NGBE_RING_DESC_MAX,
	.nb_min = NGBE_RING_DESC_MIN,
	.nb_align = NGBE_RXD_ALIGN,
};

static const struct rte_eth_desc_lim tx_desc_lim = {
	.nb_max = NGBE_RING_DESC_MAX,
	.nb_min = NGBE_RING_DESC_MIN,
	.nb_align = NGBE_TXD_ALIGN,
	.nb_seg_max = NGBE_TX_MAX_SEG,
	.nb_mtu_seg_max = NGBE_TX_MAX_SEG,
};

static const struct eth_dev_ops ngbe_eth_dev_ops;

static inline void
ngbe_enable_intr(struct rte_eth_dev *dev)
{
	struct ngbe_interrupt *intr = ngbe_dev_intr(dev);
	struct ngbe_hw *hw = ngbe_dev_hw(dev);

	wr32(hw, NGBE_IENMISC, intr->mask_misc);
	wr32(hw, NGBE_IMC(0), intr->mask & BIT_MASK32);
	ngbe_flush(hw);
}

static void
ngbe_disable_intr(struct ngbe_hw *hw)
{
	PMD_INIT_FUNC_TRACE();

	wr32(hw, NGBE_IMS(0), NGBE_IMS_MASK);
	ngbe_flush(hw);
}

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
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	const struct rte_memzone *mz;
	uint32_t ctrl_ext;
	int err;

	PMD_INIT_FUNC_TRACE();

	eth_dev->dev_ops = &ngbe_eth_dev_ops;

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

	/* disable interrupt */
	ngbe_disable_intr(hw);

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

	ctrl_ext = rd32(hw, NGBE_PORTCTL);
	/* let hardware know driver is loaded */
	ctrl_ext |= NGBE_PORTCTL_DRVLOAD;
	/* Set PF Reset Done bit so PF/VF Mail Ops can work */
	ctrl_ext |= NGBE_PORTCTL_RSTDONE;
	wr32(hw, NGBE_PORTCTL, ctrl_ext);
	ngbe_flush(hw);

	rte_intr_callback_register(intr_handle,
				   ngbe_dev_interrupt_handler, eth_dev);

	/* enable uio/vfio intr/eventfd mapping */
	rte_intr_enable(intr_handle);

	/* enable support intr */
	ngbe_enable_intr(eth_dev);

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
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING |
		     RTE_PCI_DRV_INTR_LSC,
	.probe = eth_ngbe_pci_probe,
	.remove = eth_ngbe_pci_remove,
};

static int
ngbe_dev_configure(struct rte_eth_dev *dev)
{
	struct ngbe_interrupt *intr = ngbe_dev_intr(dev);
	struct ngbe_adapter *adapter = ngbe_dev_adapter(dev);

	PMD_INIT_FUNC_TRACE();

	/* set flag to update link status after init */
	intr->flags |= NGBE_FLAG_NEED_LINK_UPDATE;

	/*
	 * Initialize to TRUE. If any of Rx queues doesn't meet the bulk
	 * allocation Rx preconditions we will reset it.
	 */
	adapter->rx_bulk_alloc_allowed = true;

	return 0;
}

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

static int
ngbe_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);

	dev_info->max_rx_queues = (uint16_t)hw->mac.max_rx_queues;
	dev_info->max_tx_queues = (uint16_t)hw->mac.max_tx_queues;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_thresh = {
			.pthresh = NGBE_DEFAULT_RX_PTHRESH,
			.hthresh = NGBE_DEFAULT_RX_HTHRESH,
			.wthresh = NGBE_DEFAULT_RX_WTHRESH,
		},
		.rx_free_thresh = NGBE_DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
		.offloads = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = NGBE_DEFAULT_TX_PTHRESH,
			.hthresh = NGBE_DEFAULT_TX_HTHRESH,
			.wthresh = NGBE_DEFAULT_TX_WTHRESH,
		},
		.tx_free_thresh = NGBE_DEFAULT_TX_FREE_THRESH,
		.offloads = 0,
	};

	dev_info->rx_desc_lim = rx_desc_lim;
	dev_info->tx_desc_lim = tx_desc_lim;

	dev_info->speed_capa = ETH_LINK_SPEED_1G | ETH_LINK_SPEED_100M |
				ETH_LINK_SPEED_10M;

	/* Driver-preferred Rx/Tx parameters */
	dev_info->default_rxportconf.nb_queues = 1;
	dev_info->default_txportconf.nb_queues = 1;
	dev_info->default_rxportconf.ring_size = 256;
	dev_info->default_txportconf.ring_size = 256;

	return 0;
}

/* return 0 means link status changed, -1 means not changed */
int
ngbe_dev_link_update_share(struct rte_eth_dev *dev,
			    int wait_to_complete)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct rte_eth_link link;
	u32 link_speed = NGBE_LINK_SPEED_UNKNOWN;
	u32 lan_speed = 0;
	struct ngbe_interrupt *intr = ngbe_dev_intr(dev);
	bool link_up;
	int err;
	int wait = 1;

	memset(&link, 0, sizeof(link));
	link.link_status = ETH_LINK_DOWN;
	link.link_speed = ETH_SPEED_NUM_NONE;
	link.link_duplex = ETH_LINK_HALF_DUPLEX;
	link.link_autoneg = !(dev->data->dev_conf.link_speeds &
			~ETH_LINK_SPEED_AUTONEG);

	hw->mac.get_link_status = true;

	if (intr->flags & NGBE_FLAG_NEED_LINK_CONFIG)
		return rte_eth_linkstatus_set(dev, &link);

	/* check if it needs to wait to complete, if lsc interrupt is enabled */
	if (wait_to_complete == 0 || dev->data->dev_conf.intr_conf.lsc != 0)
		wait = 0;

	err = hw->mac.check_link(hw, &link_speed, &link_up, wait);
	if (err != 0) {
		link.link_speed = ETH_SPEED_NUM_NONE;
		link.link_duplex = ETH_LINK_FULL_DUPLEX;
		return rte_eth_linkstatus_set(dev, &link);
	}

	if (!link_up)
		return rte_eth_linkstatus_set(dev, &link);

	intr->flags &= ~NGBE_FLAG_NEED_LINK_CONFIG;
	link.link_status = ETH_LINK_UP;
	link.link_duplex = ETH_LINK_FULL_DUPLEX;

	switch (link_speed) {
	default:
	case NGBE_LINK_SPEED_UNKNOWN:
		link.link_speed = ETH_SPEED_NUM_NONE;
		break;

	case NGBE_LINK_SPEED_10M_FULL:
		link.link_speed = ETH_SPEED_NUM_10M;
		lan_speed = 0;
		break;

	case NGBE_LINK_SPEED_100M_FULL:
		link.link_speed = ETH_SPEED_NUM_100M;
		lan_speed = 1;
		break;

	case NGBE_LINK_SPEED_1GB_FULL:
		link.link_speed = ETH_SPEED_NUM_1G;
		lan_speed = 2;
		break;
	}

	if (hw->is_pf) {
		wr32m(hw, NGBE_LAN_SPEED, NGBE_LAN_SPEED_MASK, lan_speed);
		if (link_speed & (NGBE_LINK_SPEED_1GB_FULL |
				NGBE_LINK_SPEED_100M_FULL |
				NGBE_LINK_SPEED_10M_FULL)) {
			wr32m(hw, NGBE_MACTXCFG, NGBE_MACTXCFG_SPEED_MASK,
				NGBE_MACTXCFG_SPEED_1G | NGBE_MACTXCFG_TE);
		}
	}

	return rte_eth_linkstatus_set(dev, &link);
}

static int
ngbe_dev_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	return ngbe_dev_link_update_share(dev, wait_to_complete);
}

/*
 * It reads ICR and sets flag for the link_update.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
ngbe_dev_interrupt_get_status(struct rte_eth_dev *dev)
{
	uint32_t eicr;
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_interrupt *intr = ngbe_dev_intr(dev);

	/* clear all cause mask */
	ngbe_disable_intr(hw);

	/* read-on-clear nic registers here */
	eicr = ((u32 *)hw->isb_mem)[NGBE_ISB_MISC];
	PMD_DRV_LOG(DEBUG, "eicr %x", eicr);

	intr->flags = 0;

	/* set flag for async link update */
	if (eicr & NGBE_ICRMISC_PHY)
		intr->flags |= NGBE_FLAG_NEED_LINK_UPDATE;

	if (eicr & NGBE_ICRMISC_VFMBX)
		intr->flags |= NGBE_FLAG_MAILBOX;

	if (eicr & NGBE_ICRMISC_LNKSEC)
		intr->flags |= NGBE_FLAG_MACSEC;

	if (eicr & NGBE_ICRMISC_GPIO)
		intr->flags |= NGBE_FLAG_NEED_LINK_UPDATE;

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
ngbe_dev_link_status_print(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_eth_link link;

	rte_eth_linkstatus_get(dev, &link);

	if (link.link_status == ETH_LINK_UP) {
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
ngbe_dev_interrupt_action(struct rte_eth_dev *dev)
{
	struct ngbe_interrupt *intr = ngbe_dev_intr(dev);
	int64_t timeout;

	PMD_DRV_LOG(DEBUG, "intr action type %d", intr->flags);

	if (intr->flags & NGBE_FLAG_NEED_LINK_UPDATE) {
		struct rte_eth_link link;

		/*get the link status before link update, for predicting later*/
		rte_eth_linkstatus_get(dev, &link);

		ngbe_dev_link_update(dev, 0);

		/* likely to up */
		if (link.link_status != ETH_LINK_UP)
			/* handle it 1 sec later, wait it being stable */
			timeout = NGBE_LINK_UP_CHECK_TIMEOUT;
		/* likely to down */
		else
			/* handle it 4 sec later, wait it being stable */
			timeout = NGBE_LINK_DOWN_CHECK_TIMEOUT;

		ngbe_dev_link_status_print(dev);
		if (rte_eal_alarm_set(timeout * 1000,
				      ngbe_dev_interrupt_delayed_handler,
				      (void *)dev) < 0) {
			PMD_DRV_LOG(ERR, "Error setting alarm");
		} else {
			/* remember original mask */
			intr->mask_misc_orig = intr->mask_misc;
			/* only disable lsc interrupt */
			intr->mask_misc &= ~NGBE_ICRMISC_PHY;

			intr->mask_orig = intr->mask;
			/* only disable all misc interrupts */
			intr->mask &= ~(1ULL << NGBE_MISC_VEC_ID);
		}
	}

	PMD_DRV_LOG(DEBUG, "enable intr immediately");
	ngbe_enable_intr(dev);

	return 0;
}

/**
 * Interrupt handler which shall be registered for alarm callback for delayed
 * handling specific interrupt to wait for the stable nic state. As the
 * NIC interrupt state is not stable for ngbe after link is just down,
 * it needs to wait 4 seconds to get the stable status.
 *
 * @param param
 *  The address of parameter (struct rte_eth_dev *) registered before.
 */
static void
ngbe_dev_interrupt_delayed_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct ngbe_interrupt *intr = ngbe_dev_intr(dev);
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint32_t eicr;

	ngbe_disable_intr(hw);

	eicr = ((u32 *)hw->isb_mem)[NGBE_ISB_MISC];

	if (intr->flags & NGBE_FLAG_NEED_LINK_UPDATE) {
		ngbe_dev_link_update(dev, 0);
		intr->flags &= ~NGBE_FLAG_NEED_LINK_UPDATE;
		ngbe_dev_link_status_print(dev);
		rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC,
					      NULL);
	}

	if (intr->flags & NGBE_FLAG_MACSEC) {
		rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_MACSEC,
					      NULL);
		intr->flags &= ~NGBE_FLAG_MACSEC;
	}

	/* restore original mask */
	intr->mask_misc = intr->mask_misc_orig;
	intr->mask_misc_orig = 0;
	intr->mask = intr->mask_orig;
	intr->mask_orig = 0;

	PMD_DRV_LOG(DEBUG, "enable intr in delayed handler S[%08x]", eicr);
	ngbe_enable_intr(dev);
}

/**
 * Interrupt handler triggered by NIC  for handling
 * specific interrupt.
 *
 * @param param
 *  The address of parameter (struct rte_eth_dev *) registered before.
 */
static void
ngbe_dev_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;

	ngbe_dev_interrupt_get_status(dev);
	ngbe_dev_interrupt_action(dev);
}

static const struct eth_dev_ops ngbe_eth_dev_ops = {
	.dev_configure              = ngbe_dev_configure,
	.dev_infos_get              = ngbe_dev_info_get,
	.link_update                = ngbe_dev_link_update,
	.rx_queue_setup             = ngbe_dev_rx_queue_setup,
	.rx_queue_release           = ngbe_dev_rx_queue_release,
	.tx_queue_setup             = ngbe_dev_tx_queue_setup,
	.tx_queue_release           = ngbe_dev_tx_queue_release,
};

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
