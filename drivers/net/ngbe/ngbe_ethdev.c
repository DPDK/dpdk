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
static int ngbe_dev_link_update(struct rte_eth_dev *dev,
				int wait_to_complete);

static void ngbe_dev_link_status_print(struct rte_eth_dev *dev);
static int ngbe_dev_lsc_interrupt_setup(struct rte_eth_dev *dev, uint8_t on);
static int ngbe_dev_macsec_interrupt_setup(struct rte_eth_dev *dev);
static int ngbe_dev_misc_interrupt_setup(struct rte_eth_dev *dev);
static int ngbe_dev_rxq_interrupt_setup(struct rte_eth_dev *dev);
static void ngbe_dev_interrupt_handler(void *param);
static void ngbe_dev_interrupt_delayed_handler(void *param);
static void ngbe_configure_msix(struct rte_eth_dev *dev);

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

static inline int32_t
ngbe_pf_reset_hw(struct ngbe_hw *hw)
{
	uint32_t ctrl_ext;
	int32_t status;

	status = hw->mac.reset_hw(hw);

	ctrl_ext = rd32(hw, NGBE_PORTCTL);
	/* Set PF Reset Done bit so PF/VF Mail Ops can work */
	ctrl_ext |= NGBE_PORTCTL_RSTDONE;
	wr32(hw, NGBE_PORTCTL, ctrl_ext);
	ngbe_flush(hw);

	if (status == NGBE_ERR_SFP_NOT_PRESENT)
		status = 0;
	return status;
}

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
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	const struct rte_memzone *mz;
	uint32_t ctrl_ext;
	int err;

	PMD_INIT_FUNC_TRACE();

	eth_dev->dev_ops = &ngbe_eth_dev_ops;
	eth_dev->rx_pkt_burst = &ngbe_recv_pkts;
	eth_dev->tx_pkt_burst = &ngbe_xmit_pkts_simple;

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

	PMD_INIT_LOG(DEBUG, "MAC: %d, PHY: %d",
			(int)hw->mac.type, (int)hw->phy.type);

	PMD_INIT_LOG(DEBUG, "port %d vendorID=0x%x deviceID=0x%x",
		     eth_dev->data->port_id, pci_dev->id.vendor_id,
		     pci_dev->id.device_id);

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

	return 0;
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

static void
ngbe_dev_phy_intr_setup(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_interrupt *intr = ngbe_dev_intr(dev);

	wr32(hw, NGBE_GPIODIR, NGBE_GPIODIR_DDR(1));
	wr32(hw, NGBE_GPIOINTEN, NGBE_GPIOINTEN_INT(3));
	wr32(hw, NGBE_GPIOINTTYPE, NGBE_GPIOINTTYPE_LEVEL(0));
	if (hw->phy.type == ngbe_phy_yt8521s_sfi)
		wr32(hw, NGBE_GPIOINTPOL, NGBE_GPIOINTPOL_ACT(0));
	else
		wr32(hw, NGBE_GPIOINTPOL, NGBE_GPIOINTPOL_ACT(3));

	intr->mask_misc |= NGBE_ICRMISC_GPIO;
}

/*
 * Configure device link speed and setup link.
 * It returns 0 on success.
 */
static int
ngbe_dev_start(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	uint32_t intr_vector = 0;
	int err;
	bool link_up = false, negotiate = false;
	uint32_t speed = 0;
	uint32_t allowed_speeds = 0;
	int status;
	uint32_t *link_speeds;

	PMD_INIT_FUNC_TRACE();

	/* disable uio/vfio intr/eventfd mapping */
	rte_intr_disable(intr_handle);

	/* stop adapter */
	hw->adapter_stopped = 0;
	ngbe_stop_hw(hw);

	/* reinitialize adapter, this calls reset and start */
	hw->nb_rx_queues = dev->data->nb_rx_queues;
	hw->nb_tx_queues = dev->data->nb_tx_queues;
	status = ngbe_pf_reset_hw(hw);
	if (status != 0)
		return -1;
	hw->mac.start_hw(hw);
	hw->mac.get_link_status = true;

	ngbe_dev_phy_intr_setup(dev);

	/* check and configure queue intr-vector mapping */
	if ((rte_intr_cap_multiple(intr_handle) ||
	     !RTE_ETH_DEV_SRIOV(dev).active) &&
	    dev->data->dev_conf.intr_conf.rxq != 0) {
		intr_vector = dev->data->nb_rx_queues;
		if (rte_intr_efd_enable(intr_handle, intr_vector))
			return -1;
	}

	if (rte_intr_dp_is_en(intr_handle)) {
		if (rte_intr_vec_list_alloc(intr_handle, "intr_vec",
						   dev->data->nb_rx_queues)) {
			PMD_INIT_LOG(ERR,
				     "Failed to allocate %d rx_queues intr_vec",
				     dev->data->nb_rx_queues);
			return -ENOMEM;
		}
	}

	/* confiugre MSI-X for sleep until Rx interrupt */
	ngbe_configure_msix(dev);

	/* initialize transmission unit */
	ngbe_dev_tx_init(dev);

	/* This can fail when allocating mbufs for descriptor rings */
	err = ngbe_dev_rx_init(dev);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "Unable to initialize Rx hardware");
		goto error;
	}

	err = ngbe_dev_rxtx_start(dev);
	if (err < 0) {
		PMD_INIT_LOG(ERR, "Unable to start rxtx queues");
		goto error;
	}

	err = hw->mac.check_link(hw, &speed, &link_up, 0);
	if (err != 0)
		goto error;
	dev->data->dev_link.link_status = link_up;

	link_speeds = &dev->data->dev_conf.link_speeds;
	if (*link_speeds == RTE_ETH_LINK_SPEED_AUTONEG)
		negotiate = true;

	err = hw->mac.get_link_capabilities(hw, &speed, &negotiate);
	if (err != 0)
		goto error;

	allowed_speeds = 0;
	if (hw->mac.default_speeds & NGBE_LINK_SPEED_1GB_FULL)
		allowed_speeds |= RTE_ETH_LINK_SPEED_1G;
	if (hw->mac.default_speeds & NGBE_LINK_SPEED_100M_FULL)
		allowed_speeds |= RTE_ETH_LINK_SPEED_100M;
	if (hw->mac.default_speeds & NGBE_LINK_SPEED_10M_FULL)
		allowed_speeds |= RTE_ETH_LINK_SPEED_10M;

	if (*link_speeds & ~allowed_speeds) {
		PMD_INIT_LOG(ERR, "Invalid link setting");
		goto error;
	}

	speed = 0x0;
	if (*link_speeds == RTE_ETH_LINK_SPEED_AUTONEG) {
		speed = hw->mac.default_speeds;
	} else {
		if (*link_speeds & RTE_ETH_LINK_SPEED_1G)
			speed |= NGBE_LINK_SPEED_1GB_FULL;
		if (*link_speeds & RTE_ETH_LINK_SPEED_100M)
			speed |= NGBE_LINK_SPEED_100M_FULL;
		if (*link_speeds & RTE_ETH_LINK_SPEED_10M)
			speed |= NGBE_LINK_SPEED_10M_FULL;
	}

	hw->phy.init_hw(hw);
	err = hw->mac.setup_link(hw, speed, link_up);
	if (err != 0)
		goto error;

	if (rte_intr_allow_others(intr_handle)) {
		ngbe_dev_misc_interrupt_setup(dev);
		/* check if lsc interrupt is enabled */
		if (dev->data->dev_conf.intr_conf.lsc != 0)
			ngbe_dev_lsc_interrupt_setup(dev, TRUE);
		else
			ngbe_dev_lsc_interrupt_setup(dev, FALSE);
		ngbe_dev_macsec_interrupt_setup(dev);
		ngbe_set_ivar_map(hw, -1, 1, NGBE_MISC_VEC_ID);
	} else {
		rte_intr_callback_unregister(intr_handle,
					     ngbe_dev_interrupt_handler, dev);
		if (dev->data->dev_conf.intr_conf.lsc != 0)
			PMD_INIT_LOG(INFO,
				     "LSC won't enable because of no intr multiplex");
	}

	/* check if rxq interrupt is enabled */
	if (dev->data->dev_conf.intr_conf.rxq != 0 &&
	    rte_intr_dp_is_en(intr_handle))
		ngbe_dev_rxq_interrupt_setup(dev);

	/* enable UIO/VFIO intr/eventfd mapping */
	rte_intr_enable(intr_handle);

	/* resume enabled intr since HW reset */
	ngbe_enable_intr(dev);

	if ((hw->sub_system_id & NGBE_OEM_MASK) == NGBE_LY_M88E1512_SFP ||
		(hw->sub_system_id & NGBE_OEM_MASK) == NGBE_LY_YT8521S_SFP) {
		/* gpio0 is used to power on/off control*/
		wr32(hw, NGBE_GPIODATA, 0);
	}

	/*
	 * Update link status right before return, because it may
	 * start link configuration process in a separate thread.
	 */
	ngbe_dev_link_update(dev, 0);

	return 0;

error:
	PMD_INIT_LOG(ERR, "failure in dev start: %d", err);
	ngbe_dev_clear_queues(dev);
	return -EIO;
}

/*
 * Stop device: disable rx and tx functions to allow for reconfiguring.
 */
static int
ngbe_dev_stop(struct rte_eth_dev *dev)
{
	struct rte_eth_link link;
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;

	if (hw->adapter_stopped)
		return 0;

	PMD_INIT_FUNC_TRACE();

	if ((hw->sub_system_id & NGBE_OEM_MASK) == NGBE_LY_M88E1512_SFP ||
		(hw->sub_system_id & NGBE_OEM_MASK) == NGBE_LY_YT8521S_SFP) {
		/* gpio0 is used to power on/off control*/
		wr32(hw, NGBE_GPIODATA, NGBE_GPIOBIT_0);
	}

	/* disable interrupts */
	ngbe_disable_intr(hw);

	/* reset the NIC */
	ngbe_pf_reset_hw(hw);
	hw->adapter_stopped = 0;

	/* stop adapter */
	ngbe_stop_hw(hw);

	ngbe_dev_clear_queues(dev);

	/* Clear recorded link status */
	memset(&link, 0, sizeof(link));
	rte_eth_linkstatus_set(dev, &link);

	if (!rte_intr_allow_others(intr_handle))
		/* resume to the default handler */
		rte_intr_callback_register(intr_handle,
					   ngbe_dev_interrupt_handler,
					   (void *)dev);

	/* Clean datapath event and queue/vec mapping */
	rte_intr_efd_disable(intr_handle);
	rte_intr_vec_list_free(intr_handle);

	hw->adapter_stopped = true;
	dev->data->dev_started = 0;

	return 0;
}

/*
 * Reset and stop device.
 */
static int
ngbe_dev_close(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	int retries = 0;
	int ret;

	PMD_INIT_FUNC_TRACE();

	ngbe_pf_reset_hw(hw);

	ngbe_dev_stop(dev);

	ngbe_dev_free_queues(dev);

	/* reprogram the RAR[0] in case user changed it. */
	ngbe_set_rar(hw, 0, hw->mac.addr, 0, true);

	/* Unlock any pending hardware semaphore */
	ngbe_swfw_lock_reset(hw);

	/* disable uio intr before callback unregister */
	rte_intr_disable(intr_handle);

	do {
		ret = rte_intr_callback_unregister(intr_handle,
				ngbe_dev_interrupt_handler, dev);
		if (ret >= 0 || ret == -ENOENT) {
			break;
		} else if (ret != -EAGAIN) {
			PMD_INIT_LOG(ERR,
				"intr callback unregister failed: %d",
				ret);
		}
		rte_delay_ms(100);
	} while (retries++ < (10 + NGBE_LINK_UP_TIME));

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
ngbe_dev_reset(struct rte_eth_dev *dev)
{
	int ret;

	ret = eth_ngbe_dev_uninit(dev);
	if (ret != 0)
		return ret;

	ret = eth_ngbe_dev_init(dev, NULL);

	return ret;
}

static int
ngbe_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);

	dev_info->max_rx_queues = (uint16_t)hw->mac.max_rx_queues;
	dev_info->max_tx_queues = (uint16_t)hw->mac.max_tx_queues;
	dev_info->min_rx_bufsize = 1024;
	dev_info->max_rx_pktlen = 15872;

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

	dev_info->speed_capa = RTE_ETH_LINK_SPEED_1G | RTE_ETH_LINK_SPEED_100M |
				RTE_ETH_LINK_SPEED_10M;

	/* Driver-preferred Rx/Tx parameters */
	dev_info->default_rxportconf.burst_size = 32;
	dev_info->default_txportconf.burst_size = 32;
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
	link.link_status = RTE_ETH_LINK_DOWN;
	link.link_speed = RTE_ETH_SPEED_NUM_NONE;
	link.link_duplex = RTE_ETH_LINK_HALF_DUPLEX;
	link.link_autoneg = !(dev->data->dev_conf.link_speeds &
			~RTE_ETH_LINK_SPEED_AUTONEG);

	hw->mac.get_link_status = true;

	if (intr->flags & NGBE_FLAG_NEED_LINK_CONFIG)
		return rte_eth_linkstatus_set(dev, &link);

	/* check if it needs to wait to complete, if lsc interrupt is enabled */
	if (wait_to_complete == 0 || dev->data->dev_conf.intr_conf.lsc != 0)
		wait = 0;

	err = hw->mac.check_link(hw, &link_speed, &link_up, wait);
	if (err != 0) {
		link.link_speed = RTE_ETH_SPEED_NUM_NONE;
		link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
		return rte_eth_linkstatus_set(dev, &link);
	}

	if (!link_up)
		return rte_eth_linkstatus_set(dev, &link);

	intr->flags &= ~NGBE_FLAG_NEED_LINK_CONFIG;
	link.link_status = RTE_ETH_LINK_UP;
	link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;

	switch (link_speed) {
	default:
	case NGBE_LINK_SPEED_UNKNOWN:
		link.link_speed = RTE_ETH_SPEED_NUM_NONE;
		break;

	case NGBE_LINK_SPEED_10M_FULL:
		link.link_speed = RTE_ETH_SPEED_NUM_10M;
		lan_speed = 0;
		break;

	case NGBE_LINK_SPEED_100M_FULL:
		link.link_speed = RTE_ETH_SPEED_NUM_100M;
		lan_speed = 1;
		break;

	case NGBE_LINK_SPEED_1GB_FULL:
		link.link_speed = RTE_ETH_SPEED_NUM_1G;
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

/**
 * It clears the interrupt causes and enables the interrupt.
 * It will be called once only during NIC initialized.
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
ngbe_dev_lsc_interrupt_setup(struct rte_eth_dev *dev, uint8_t on)
{
	struct ngbe_interrupt *intr = ngbe_dev_intr(dev);

	ngbe_dev_link_status_print(dev);
	if (on != 0) {
		intr->mask_misc |= NGBE_ICRMISC_PHY;
		intr->mask_misc |= NGBE_ICRMISC_GPIO;
	} else {
		intr->mask_misc &= ~NGBE_ICRMISC_PHY;
		intr->mask_misc &= ~NGBE_ICRMISC_GPIO;
	}

	return 0;
}

/**
 * It clears the interrupt causes and enables the interrupt.
 * It will be called once only during NIC initialized.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
ngbe_dev_misc_interrupt_setup(struct rte_eth_dev *dev)
{
	struct ngbe_interrupt *intr = ngbe_dev_intr(dev);
	u64 mask;

	mask = NGBE_ICR_MASK;
	mask &= (1ULL << NGBE_MISC_VEC_ID);
	intr->mask |= mask;
	intr->mask_misc |= NGBE_ICRMISC_GPIO;

	return 0;
}

/**
 * It clears the interrupt causes and enables the interrupt.
 * It will be called once only during NIC initialized.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
ngbe_dev_rxq_interrupt_setup(struct rte_eth_dev *dev)
{
	struct ngbe_interrupt *intr = ngbe_dev_intr(dev);
	u64 mask;

	mask = NGBE_ICR_MASK;
	mask &= ~((1ULL << NGBE_RX_VEC_START) - 1);
	intr->mask |= mask;

	return 0;
}

/**
 * It clears the interrupt causes and enables the interrupt.
 * It will be called once only during NIC initialized.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
ngbe_dev_macsec_interrupt_setup(struct rte_eth_dev *dev)
{
	struct ngbe_interrupt *intr = ngbe_dev_intr(dev);

	intr->mask_misc |= NGBE_ICRMISC_LNKSEC;

	return 0;
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

	if (link.link_status == RTE_ETH_LINK_UP) {
		PMD_INIT_LOG(INFO, "Port %d: Link Up - speed %u Mbps - %s",
					(int)(dev->data->port_id),
					(unsigned int)link.link_speed,
			link.link_duplex == RTE_ETH_LINK_FULL_DUPLEX ?
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
		if (link.link_status != RTE_ETH_LINK_UP)
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

/**
 * Set the IVAR registers, mapping interrupt causes to vectors
 * @param hw
 *  pointer to ngbe_hw struct
 * @direction
 *  0 for Rx, 1 for Tx, -1 for other causes
 * @queue
 *  queue to map the corresponding interrupt to
 * @msix_vector
 *  the vector to map to the corresponding queue
 */
void
ngbe_set_ivar_map(struct ngbe_hw *hw, int8_t direction,
		   uint8_t queue, uint8_t msix_vector)
{
	uint32_t tmp, idx;

	if (direction == -1) {
		/* other causes */
		msix_vector |= NGBE_IVARMISC_VLD;
		idx = 0;
		tmp = rd32(hw, NGBE_IVARMISC);
		tmp &= ~(0xFF << idx);
		tmp |= (msix_vector << idx);
		wr32(hw, NGBE_IVARMISC, tmp);
	} else {
		/* rx or tx causes */
		/* Workround for ICR lost */
		idx = ((16 * (queue & 1)) + (8 * direction));
		tmp = rd32(hw, NGBE_IVAR(queue >> 1));
		tmp &= ~(0xFF << idx);
		tmp |= (msix_vector << idx);
		wr32(hw, NGBE_IVAR(queue >> 1), tmp);
	}
}

/**
 * Sets up the hardware to properly generate MSI-X interrupts
 * @hw
 *  board private structure
 */
static void
ngbe_configure_msix(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint32_t queue_id, base = NGBE_MISC_VEC_ID;
	uint32_t vec = NGBE_MISC_VEC_ID;
	uint32_t gpie;

	/*
	 * Won't configure MSI-X register if no mapping is done
	 * between intr vector and event fd
	 * but if MSI-X has been enabled already, need to configure
	 * auto clean, auto mask and throttling.
	 */
	gpie = rd32(hw, NGBE_GPIE);
	if (!rte_intr_dp_is_en(intr_handle) &&
	    !(gpie & NGBE_GPIE_MSIX))
		return;

	if (rte_intr_allow_others(intr_handle)) {
		base = NGBE_RX_VEC_START;
		vec = base;
	}

	/* setup GPIE for MSI-X mode */
	gpie = rd32(hw, NGBE_GPIE);
	gpie |= NGBE_GPIE_MSIX;
	wr32(hw, NGBE_GPIE, gpie);

	/* Populate the IVAR table and set the ITR values to the
	 * corresponding register.
	 */
	if (rte_intr_dp_is_en(intr_handle)) {
		for (queue_id = 0; queue_id < dev->data->nb_rx_queues;
			queue_id++) {
			/* by default, 1:1 mapping */
			ngbe_set_ivar_map(hw, 0, queue_id, vec);
			rte_intr_vec_list_index_set(intr_handle,
							   queue_id, vec);
			if (vec < base + rte_intr_nb_efd_get(intr_handle)
			    - 1)
				vec++;
		}

		ngbe_set_ivar_map(hw, -1, 1, NGBE_MISC_VEC_ID);
	}
	wr32(hw, NGBE_ITR(NGBE_MISC_VEC_ID),
			NGBE_ITR_IVAL_1G(NGBE_QUEUE_ITR_INTERVAL_DEFAULT)
			| NGBE_ITR_WRDSA);
}

static const struct eth_dev_ops ngbe_eth_dev_ops = {
	.dev_configure              = ngbe_dev_configure,
	.dev_infos_get              = ngbe_dev_info_get,
	.dev_start                  = ngbe_dev_start,
	.dev_stop                   = ngbe_dev_stop,
	.dev_close                  = ngbe_dev_close,
	.dev_reset                  = ngbe_dev_reset,
	.link_update                = ngbe_dev_link_update,
	.rx_queue_start	            = ngbe_dev_rx_queue_start,
	.rx_queue_stop              = ngbe_dev_rx_queue_stop,
	.tx_queue_start	            = ngbe_dev_tx_queue_start,
	.tx_queue_stop              = ngbe_dev_tx_queue_stop,
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
