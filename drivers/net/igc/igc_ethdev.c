/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Intel Corporation
 */

#include <stdint.h>
#include <string.h>

#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_malloc.h>

#include "igc_logs.h"
#include "igc_txrx.h"

#define IGC_INTEL_VENDOR_ID		0x8086

/*
 * The overhead from MTU to max frame size.
 * Considering VLAN so tag needs to be counted.
 */
#define IGC_ETH_OVERHEAD		(RTE_ETHER_HDR_LEN + \
					RTE_ETHER_CRC_LEN + VLAN_TAG_SIZE)

#define IGC_FC_PAUSE_TIME		0x0680
#define IGC_LINK_UPDATE_CHECK_TIMEOUT	90  /* 9s */
#define IGC_LINK_UPDATE_CHECK_INTERVAL	100 /* ms */

#define IGC_MISC_VEC_ID			RTE_INTR_VEC_ZERO_OFFSET
#define IGC_RX_VEC_START		RTE_INTR_VEC_RXTX_OFFSET
#define IGC_MSIX_OTHER_INTR_VEC		0   /* MSI-X other interrupt vector */
#define IGC_FLAG_NEED_LINK_UPDATE	(1u << 0)	/* need update link */

#define IGC_DEFAULT_RX_FREE_THRESH	32

#define IGC_DEFAULT_RX_PTHRESH		8
#define IGC_DEFAULT_RX_HTHRESH		8
#define IGC_DEFAULT_RX_WTHRESH		4

#define IGC_DEFAULT_TX_PTHRESH		8
#define IGC_DEFAULT_TX_HTHRESH		1
#define IGC_DEFAULT_TX_WTHRESH		16

/* MSI-X other interrupt vector */
#define IGC_MSIX_OTHER_INTR_VEC		0

/* External VLAN Enable bit mask */
#define IGC_CTRL_EXT_EXT_VLAN		(1u << 26)

static const struct rte_eth_desc_lim rx_desc_lim = {
	.nb_max = IGC_MAX_RXD,
	.nb_min = IGC_MIN_RXD,
	.nb_align = IGC_RXD_ALIGN,
};

static const struct rte_eth_desc_lim tx_desc_lim = {
	.nb_max = IGC_MAX_TXD,
	.nb_min = IGC_MIN_TXD,
	.nb_align = IGC_TXD_ALIGN,
	.nb_seg_max = IGC_TX_MAX_SEG,
	.nb_mtu_seg_max = IGC_TX_MAX_MTU_SEG,
};

static const struct rte_pci_id pci_id_igc_map[] = {
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I225_LM) },
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I225_V)  },
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I225_I)  },
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I225_K)  },
	{ .vendor_id = 0, /* sentinel */ },
};

static int eth_igc_configure(struct rte_eth_dev *dev);
static int eth_igc_link_update(struct rte_eth_dev *dev, int wait_to_complete);
static void eth_igc_stop(struct rte_eth_dev *dev);
static int eth_igc_start(struct rte_eth_dev *dev);
static int eth_igc_set_link_up(struct rte_eth_dev *dev);
static int eth_igc_set_link_down(struct rte_eth_dev *dev);
static void eth_igc_close(struct rte_eth_dev *dev);
static int eth_igc_reset(struct rte_eth_dev *dev);
static int eth_igc_promiscuous_enable(struct rte_eth_dev *dev);
static int eth_igc_promiscuous_disable(struct rte_eth_dev *dev);
static int eth_igc_fw_version_get(struct rte_eth_dev *dev,
				char *fw_version, size_t fw_size);
static int eth_igc_infos_get(struct rte_eth_dev *dev,
			struct rte_eth_dev_info *dev_info);
static int eth_igc_led_on(struct rte_eth_dev *dev);
static int eth_igc_led_off(struct rte_eth_dev *dev);
static const uint32_t *eth_igc_supported_ptypes_get(struct rte_eth_dev *dev);
static int eth_igc_rar_set(struct rte_eth_dev *dev,
		struct rte_ether_addr *mac_addr, uint32_t index, uint32_t pool);
static void eth_igc_rar_clear(struct rte_eth_dev *dev, uint32_t index);
static int eth_igc_default_mac_addr_set(struct rte_eth_dev *dev,
			struct rte_ether_addr *addr);
static int eth_igc_set_mc_addr_list(struct rte_eth_dev *dev,
			 struct rte_ether_addr *mc_addr_set,
			 uint32_t nb_mc_addr);
static int eth_igc_allmulticast_enable(struct rte_eth_dev *dev);
static int eth_igc_allmulticast_disable(struct rte_eth_dev *dev);
static int eth_igc_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);

static const struct eth_dev_ops eth_igc_ops = {
	.dev_configure		= eth_igc_configure,
	.link_update		= eth_igc_link_update,
	.dev_stop		= eth_igc_stop,
	.dev_start		= eth_igc_start,
	.dev_close		= eth_igc_close,
	.dev_reset		= eth_igc_reset,
	.dev_set_link_up	= eth_igc_set_link_up,
	.dev_set_link_down	= eth_igc_set_link_down,
	.promiscuous_enable	= eth_igc_promiscuous_enable,
	.promiscuous_disable	= eth_igc_promiscuous_disable,
	.allmulticast_enable	= eth_igc_allmulticast_enable,
	.allmulticast_disable	= eth_igc_allmulticast_disable,
	.fw_version_get		= eth_igc_fw_version_get,
	.dev_infos_get		= eth_igc_infos_get,
	.dev_led_on		= eth_igc_led_on,
	.dev_led_off		= eth_igc_led_off,
	.dev_supported_ptypes_get = eth_igc_supported_ptypes_get,
	.mtu_set		= eth_igc_mtu_set,
	.mac_addr_add		= eth_igc_rar_set,
	.mac_addr_remove	= eth_igc_rar_clear,
	.mac_addr_set		= eth_igc_default_mac_addr_set,
	.set_mc_addr_list	= eth_igc_set_mc_addr_list,

	.rx_queue_setup		= eth_igc_rx_queue_setup,
	.rx_queue_release	= eth_igc_rx_queue_release,
	.rx_queue_count		= eth_igc_rx_queue_count,
	.rx_descriptor_done	= eth_igc_rx_descriptor_done,
	.rx_descriptor_status	= eth_igc_rx_descriptor_status,
	.tx_descriptor_status	= eth_igc_tx_descriptor_status,
	.tx_queue_setup		= eth_igc_tx_queue_setup,
	.tx_queue_release	= eth_igc_tx_queue_release,
	.tx_done_cleanup	= eth_igc_tx_done_cleanup,
	.rxq_info_get		= eth_igc_rxq_info_get,
	.txq_info_get		= eth_igc_txq_info_get,
};

/*
 * multiple queue mode checking
 */
static int
igc_check_mq_mode(struct rte_eth_dev *dev)
{
	enum rte_eth_rx_mq_mode rx_mq_mode = dev->data->dev_conf.rxmode.mq_mode;
	enum rte_eth_tx_mq_mode tx_mq_mode = dev->data->dev_conf.txmode.mq_mode;

	if (RTE_ETH_DEV_SRIOV(dev).active != 0) {
		PMD_INIT_LOG(ERR, "SRIOV is not supported.");
		return -EINVAL;
	}

	if (rx_mq_mode != ETH_MQ_RX_NONE &&
		rx_mq_mode != ETH_MQ_RX_RSS) {
		/* RSS together with VMDq not supported*/
		PMD_INIT_LOG(ERR, "RX mode %d is not supported.",
				rx_mq_mode);
		return -EINVAL;
	}

	/* To no break software that set invalid mode, only display
	 * warning if invalid mode is used.
	 */
	if (tx_mq_mode != ETH_MQ_TX_NONE)
		PMD_INIT_LOG(WARNING,
			"TX mode %d is not supported. Due to meaningless in this driver, just ignore",
			tx_mq_mode);

	return 0;
}

static int
eth_igc_configure(struct rte_eth_dev *dev)
{
	struct igc_interrupt *intr = IGC_DEV_PRIVATE_INTR(dev);
	int ret;

	PMD_INIT_FUNC_TRACE();

	ret  = igc_check_mq_mode(dev);
	if (ret != 0)
		return ret;

	intr->flags |= IGC_FLAG_NEED_LINK_UPDATE;
	return 0;
}

static int
eth_igc_set_link_up(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	if (hw->phy.media_type == igc_media_type_copper)
		igc_power_up_phy(hw);
	else
		igc_power_up_fiber_serdes_link(hw);
	return 0;
}

static int
eth_igc_set_link_down(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	if (hw->phy.media_type == igc_media_type_copper)
		igc_power_down_phy(hw);
	else
		igc_shutdown_fiber_serdes_link(hw);
	return 0;
}

/*
 * disable other interrupt
 */
static void
igc_intr_other_disable(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;

	if (rte_intr_allow_others(intr_handle) &&
		dev->data->dev_conf.intr_conf.lsc) {
		IGC_WRITE_REG(hw, IGC_EIMC, 1u << IGC_MSIX_OTHER_INTR_VEC);
	}

	IGC_WRITE_REG(hw, IGC_IMC, ~0);
	IGC_WRITE_FLUSH(hw);
}

/*
 * enable other interrupt
 */
static inline void
igc_intr_other_enable(struct rte_eth_dev *dev)
{
	struct igc_interrupt *intr = IGC_DEV_PRIVATE_INTR(dev);
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;

	if (rte_intr_allow_others(intr_handle) &&
		dev->data->dev_conf.intr_conf.lsc) {
		IGC_WRITE_REG(hw, IGC_EIMS, 1u << IGC_MSIX_OTHER_INTR_VEC);
	}

	IGC_WRITE_REG(hw, IGC_IMS, intr->mask);
	IGC_WRITE_FLUSH(hw);
}

/*
 * It reads ICR and gets interrupt causes, check it and set a bit flag
 * to update link status.
 */
static void
eth_igc_interrupt_get_status(struct rte_eth_dev *dev)
{
	uint32_t icr;
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct igc_interrupt *intr = IGC_DEV_PRIVATE_INTR(dev);

	/* read-on-clear nic registers here */
	icr = IGC_READ_REG(hw, IGC_ICR);

	intr->flags = 0;
	if (icr & IGC_ICR_LSC)
		intr->flags |= IGC_FLAG_NEED_LINK_UPDATE;
}

/* return 0 means link status changed, -1 means not changed */
static int
eth_igc_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct rte_eth_link link;
	int link_check, count;

	link_check = 0;
	hw->mac.get_link_status = 1;

	/* possible wait-to-complete in up to 9 seconds */
	for (count = 0; count < IGC_LINK_UPDATE_CHECK_TIMEOUT; count++) {
		/* Read the real link status */
		switch (hw->phy.media_type) {
		case igc_media_type_copper:
			/* Do the work to read phy */
			igc_check_for_link(hw);
			link_check = !hw->mac.get_link_status;
			break;

		case igc_media_type_fiber:
			igc_check_for_link(hw);
			link_check = (IGC_READ_REG(hw, IGC_STATUS) &
				      IGC_STATUS_LU);
			break;

		case igc_media_type_internal_serdes:
			igc_check_for_link(hw);
			link_check = hw->mac.serdes_has_link;
			break;

		default:
			break;
		}
		if (link_check || wait_to_complete == 0)
			break;
		rte_delay_ms(IGC_LINK_UPDATE_CHECK_INTERVAL);
	}
	memset(&link, 0, sizeof(link));

	/* Now we check if a transition has happened */
	if (link_check) {
		uint16_t duplex, speed;
		hw->mac.ops.get_link_up_info(hw, &speed, &duplex);
		link.link_duplex = (duplex == FULL_DUPLEX) ?
				ETH_LINK_FULL_DUPLEX :
				ETH_LINK_HALF_DUPLEX;
		link.link_speed = speed;
		link.link_status = ETH_LINK_UP;
		link.link_autoneg = !(dev->data->dev_conf.link_speeds &
				ETH_LINK_SPEED_FIXED);

		if (speed == SPEED_2500) {
			uint32_t tipg = IGC_READ_REG(hw, IGC_TIPG);
			if ((tipg & IGC_TIPG_IPGT_MASK) != 0x0b) {
				tipg &= ~IGC_TIPG_IPGT_MASK;
				tipg |= 0x0b;
				IGC_WRITE_REG(hw, IGC_TIPG, tipg);
			}
		}
	} else {
		link.link_speed = 0;
		link.link_duplex = ETH_LINK_HALF_DUPLEX;
		link.link_status = ETH_LINK_DOWN;
		link.link_autoneg = ETH_LINK_FIXED;
	}

	return rte_eth_linkstatus_set(dev, &link);
}

/*
 * It executes link_update after knowing an interrupt is present.
 */
static void
eth_igc_interrupt_action(struct rte_eth_dev *dev)
{
	struct igc_interrupt *intr = IGC_DEV_PRIVATE_INTR(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_eth_link link;
	int ret;

	if (intr->flags & IGC_FLAG_NEED_LINK_UPDATE) {
		intr->flags &= ~IGC_FLAG_NEED_LINK_UPDATE;

		/* set get_link_status to check register later */
		ret = eth_igc_link_update(dev, 0);

		/* check if link has changed */
		if (ret < 0)
			return;

		rte_eth_linkstatus_get(dev, &link);
		if (link.link_status)
			PMD_DRV_LOG(INFO,
				" Port %d: Link Up - speed %u Mbps - %s",
				dev->data->port_id,
				(unsigned int)link.link_speed,
				link.link_duplex == ETH_LINK_FULL_DUPLEX ?
				"full-duplex" : "half-duplex");
		else
			PMD_DRV_LOG(INFO, " Port %d: Link Down",
				dev->data->port_id);

		PMD_DRV_LOG(DEBUG, "PCI Address: " PCI_PRI_FMT,
				pci_dev->addr.domain,
				pci_dev->addr.bus,
				pci_dev->addr.devid,
				pci_dev->addr.function);
		_rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC,
				NULL);
	}
}

/*
 * Interrupt handler which shall be registered at first.
 *
 * @handle
 *  Pointer to interrupt handle.
 * @param
 *  The address of parameter (struct rte_eth_dev *) registered before.
 */
static void
eth_igc_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;

	eth_igc_interrupt_get_status(dev);
	eth_igc_interrupt_action(dev);
}

/*
 * rx,tx enable/disable
 */
static void
eth_igc_rxtx_control(struct rte_eth_dev *dev, bool enable)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint32_t tctl, rctl;

	tctl = IGC_READ_REG(hw, IGC_TCTL);
	rctl = IGC_READ_REG(hw, IGC_RCTL);

	if (enable) {
		/* enable Tx/Rx */
		tctl |= IGC_TCTL_EN;
		rctl |= IGC_RCTL_EN;
	} else {
		/* disable Tx/Rx */
		tctl &= ~IGC_TCTL_EN;
		rctl &= ~IGC_RCTL_EN;
	}
	IGC_WRITE_REG(hw, IGC_TCTL, tctl);
	IGC_WRITE_REG(hw, IGC_RCTL, rctl);
	IGC_WRITE_FLUSH(hw);
}

/*
 *  This routine disables all traffic on the adapter by issuing a
 *  global reset on the MAC.
 */
static void
eth_igc_stop(struct rte_eth_dev *dev)
{
	struct igc_adapter *adapter = IGC_DEV_PRIVATE(dev);
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct rte_eth_link link;

	adapter->stopped = 1;

	/* disable receive and transmit */
	eth_igc_rxtx_control(dev, false);

	/* disable all MSI-X interrupts */
	IGC_WRITE_REG(hw, IGC_EIMC, 0x1f);
	IGC_WRITE_FLUSH(hw);

	/* clear all MSI-X interrupts */
	IGC_WRITE_REG(hw, IGC_EICR, 0x1f);

	igc_intr_other_disable(dev);

	/* disable intr eventfd mapping */
	rte_intr_disable(intr_handle);

	igc_reset_hw(hw);

	/* disable all wake up */
	IGC_WRITE_REG(hw, IGC_WUC, 0);

	/* Set bit for Go Link disconnect */
	igc_read_reg_check_set_bits(hw, IGC_82580_PHY_POWER_MGMT,
			IGC_82580_PM_GO_LINKD);

	/* Power down the phy. Needed to make the link go Down */
	eth_igc_set_link_down(dev);

	igc_dev_clear_queues(dev);

	/* clear the recorded link status */
	memset(&link, 0, sizeof(link));
	rte_eth_linkstatus_set(dev, &link);

	if (!rte_intr_allow_others(intr_handle))
		/* resume to the default handler */
		rte_intr_callback_register(intr_handle,
					   eth_igc_interrupt_handler,
					   (void *)dev);

	/* Clean datapath event and queue/vec mapping */
	rte_intr_efd_disable(intr_handle);
}

/* Sets up the hardware to generate MSI-X interrupts properly
 * @hw
 *  board private structure
 */
static void
igc_configure_msix_intr(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;

	uint32_t intr_mask;

	/* won't configure msix register if no mapping is done
	 * between intr vector and event fd
	 */
	if (!rte_intr_dp_is_en(intr_handle) ||
		!dev->data->dev_conf.intr_conf.lsc)
		return;

	/* turn on MSI-X capability first */
	IGC_WRITE_REG(hw, IGC_GPIE, IGC_GPIE_MSIX_MODE |
				IGC_GPIE_PBA | IGC_GPIE_EIAME |
				IGC_GPIE_NSICR);

	intr_mask = (1u << IGC_MSIX_OTHER_INTR_VEC);

	/* enable msix auto-clear */
	igc_read_reg_check_set_bits(hw, IGC_EIAC, intr_mask);

	/* set other cause interrupt vector */
	igc_read_reg_check_set_bits(hw, IGC_IVAR_MISC,
		(uint32_t)(IGC_MSIX_OTHER_INTR_VEC | IGC_IVAR_VALID) << 8);

	/* enable auto-mask */
	igc_read_reg_check_set_bits(hw, IGC_EIAM, intr_mask);

	IGC_WRITE_FLUSH(hw);
}

/**
 * It enables the interrupt mask and then enable the interrupt.
 *
 * @dev
 *  Pointer to struct rte_eth_dev.
 * @on
 *  Enable or Disable
 */
static void
igc_lsc_interrupt_setup(struct rte_eth_dev *dev, uint8_t on)
{
	struct igc_interrupt *intr = IGC_DEV_PRIVATE_INTR(dev);

	if (on)
		intr->mask |= IGC_ICR_LSC;
	else
		intr->mask &= ~IGC_ICR_LSC;
}

/*
 *  Get hardware rx-buffer size.
 */
static inline int
igc_get_rx_buffer_size(struct igc_hw *hw)
{
	return (IGC_READ_REG(hw, IGC_RXPBS) & 0x3f) << 10;
}

/*
 * igc_hw_control_acquire sets CTRL_EXT:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means
 * that the driver is loaded.
 */
static void
igc_hw_control_acquire(struct igc_hw *hw)
{
	uint32_t ctrl_ext;

	/* Let firmware know the driver has taken over */
	ctrl_ext = IGC_READ_REG(hw, IGC_CTRL_EXT);
	IGC_WRITE_REG(hw, IGC_CTRL_EXT, ctrl_ext | IGC_CTRL_EXT_DRV_LOAD);
}

/*
 * igc_hw_control_release resets CTRL_EXT:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means that the
 * driver is no longer loaded.
 */
static void
igc_hw_control_release(struct igc_hw *hw)
{
	uint32_t ctrl_ext;

	/* Let firmware taken over control of h/w */
	ctrl_ext = IGC_READ_REG(hw, IGC_CTRL_EXT);
	IGC_WRITE_REG(hw, IGC_CTRL_EXT,
			ctrl_ext & ~IGC_CTRL_EXT_DRV_LOAD);
}

static int
igc_hardware_init(struct igc_hw *hw)
{
	uint32_t rx_buf_size;
	int diag;

	/* Let the firmware know the OS is in control */
	igc_hw_control_acquire(hw);

	/* Issue a global reset */
	igc_reset_hw(hw);

	/* disable all wake up */
	IGC_WRITE_REG(hw, IGC_WUC, 0);

	/*
	 * Hardware flow control
	 * - High water mark should allow for at least two standard size (1518)
	 *   frames to be received after sending an XOFF.
	 * - Low water mark works best when it is very near the high water mark.
	 *   This allows the receiver to restart by sending XON when it has
	 *   drained a bit. Here we use an arbitrary value of 1500 which will
	 *   restart after one full frame is pulled from the buffer. There
	 *   could be several smaller frames in the buffer and if so they will
	 *   not trigger the XON until their total number reduces the buffer
	 *   by 1500.
	 */
	rx_buf_size = igc_get_rx_buffer_size(hw);
	hw->fc.high_water = rx_buf_size - (RTE_ETHER_MAX_LEN * 2);
	hw->fc.low_water = hw->fc.high_water - 1500;
	hw->fc.pause_time = IGC_FC_PAUSE_TIME;
	hw->fc.send_xon = 1;
	hw->fc.requested_mode = igc_fc_full;

	diag = igc_init_hw(hw);
	if (diag < 0)
		return diag;

	igc_get_phy_info(hw);
	igc_check_for_link(hw);

	return 0;
}

static int
eth_igc_start(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct igc_adapter *adapter = IGC_DEV_PRIVATE(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	uint32_t *speeds;
	int ret;

	PMD_INIT_FUNC_TRACE();

	/* disable all MSI-X interrupts */
	IGC_WRITE_REG(hw, IGC_EIMC, 0x1f);
	IGC_WRITE_FLUSH(hw);

	/* clear all MSI-X interrupts */
	IGC_WRITE_REG(hw, IGC_EICR, 0x1f);

	/* disable uio/vfio intr/eventfd mapping */
	if (!adapter->stopped)
		rte_intr_disable(intr_handle);

	/* Power up the phy. Needed to make the link go Up */
	eth_igc_set_link_up(dev);

	/* Put the address into the Receive Address Array */
	igc_rar_set(hw, hw->mac.addr, 0);

	/* Initialize the hardware */
	if (igc_hardware_init(hw)) {
		PMD_DRV_LOG(ERR, "Unable to initialize the hardware");
		return -EIO;
	}
	adapter->stopped = 0;

	/* confiugre msix for rx interrupt */
	igc_configure_msix_intr(dev);

	igc_tx_init(dev);

	/* This can fail when allocating mbufs for descriptor rings */
	ret = igc_rx_init(dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "Unable to initialize RX hardware");
		igc_dev_clear_queues(dev);
		return ret;
	}

	igc_clear_hw_cntrs_base_generic(hw);

	/* Setup link speed and duplex */
	speeds = &dev->data->dev_conf.link_speeds;
	if (*speeds == ETH_LINK_SPEED_AUTONEG) {
		hw->phy.autoneg_advertised = IGC_ALL_SPEED_DUPLEX_2500;
		hw->mac.autoneg = 1;
	} else {
		int num_speeds = 0;
		bool autoneg = (*speeds & ETH_LINK_SPEED_FIXED) == 0;

		/* Reset */
		hw->phy.autoneg_advertised = 0;

		if (*speeds & ~(ETH_LINK_SPEED_10M_HD | ETH_LINK_SPEED_10M |
				ETH_LINK_SPEED_100M_HD | ETH_LINK_SPEED_100M |
				ETH_LINK_SPEED_1G | ETH_LINK_SPEED_2_5G |
				ETH_LINK_SPEED_FIXED)) {
			num_speeds = -1;
			goto error_invalid_config;
		}
		if (*speeds & ETH_LINK_SPEED_10M_HD) {
			hw->phy.autoneg_advertised |= ADVERTISE_10_HALF;
			num_speeds++;
		}
		if (*speeds & ETH_LINK_SPEED_10M) {
			hw->phy.autoneg_advertised |= ADVERTISE_10_FULL;
			num_speeds++;
		}
		if (*speeds & ETH_LINK_SPEED_100M_HD) {
			hw->phy.autoneg_advertised |= ADVERTISE_100_HALF;
			num_speeds++;
		}
		if (*speeds & ETH_LINK_SPEED_100M) {
			hw->phy.autoneg_advertised |= ADVERTISE_100_FULL;
			num_speeds++;
		}
		if (*speeds & ETH_LINK_SPEED_1G) {
			hw->phy.autoneg_advertised |= ADVERTISE_1000_FULL;
			num_speeds++;
		}
		if (*speeds & ETH_LINK_SPEED_2_5G) {
			hw->phy.autoneg_advertised |= ADVERTISE_2500_FULL;
			num_speeds++;
		}
		if (num_speeds == 0 || (!autoneg && num_speeds > 1))
			goto error_invalid_config;

		/* Set/reset the mac.autoneg based on the link speed,
		 * fixed or not
		 */
		if (!autoneg) {
			hw->mac.autoneg = 0;
			hw->mac.forced_speed_duplex =
					hw->phy.autoneg_advertised;
		} else {
			hw->mac.autoneg = 1;
		}
	}

	igc_setup_link(hw);

	if (rte_intr_allow_others(intr_handle)) {
		/* check if lsc interrupt is enabled */
		if (dev->data->dev_conf.intr_conf.lsc)
			igc_lsc_interrupt_setup(dev, 1);
		else
			igc_lsc_interrupt_setup(dev, 0);
	} else {
		rte_intr_callback_unregister(intr_handle,
					     eth_igc_interrupt_handler,
					     (void *)dev);
		if (dev->data->dev_conf.intr_conf.lsc)
			PMD_DRV_LOG(INFO,
				"LSC won't enable because of no intr multiplex");
	}

	/* enable uio/vfio intr/eventfd mapping */
	rte_intr_enable(intr_handle);

	/* resume enabled intr since hw reset */
	igc_intr_other_enable(dev);

	eth_igc_rxtx_control(dev, true);
	eth_igc_link_update(dev, 0);

	return 0;

error_invalid_config:
	PMD_DRV_LOG(ERR, "Invalid advertised speeds (%u) for port %u",
		     dev->data->dev_conf.link_speeds, dev->data->port_id);
	igc_dev_clear_queues(dev);
	return -EINVAL;
}

static int
igc_reset_swfw_lock(struct igc_hw *hw)
{
	int ret_val;

	/*
	 * Do mac ops initialization manually here, since we will need
	 * some function pointers set by this call.
	 */
	ret_val = igc_init_mac_params(hw);
	if (ret_val)
		return ret_val;

	/*
	 * SMBI lock should not fail in this early stage. If this is the case,
	 * it is due to an improper exit of the application.
	 * So force the release of the faulty lock.
	 */
	if (igc_get_hw_semaphore_generic(hw) < 0)
		PMD_DRV_LOG(DEBUG, "SMBI lock released");

	igc_put_hw_semaphore_generic(hw);

	if (hw->mac.ops.acquire_swfw_sync != NULL) {
		uint16_t mask;

		/*
		 * Phy lock should not fail in this early stage.
		 * If this is the case, it is due to an improper exit of the
		 * application. So force the release of the faulty lock.
		 */
		mask = IGC_SWFW_PHY0_SM;
		if (hw->mac.ops.acquire_swfw_sync(hw, mask) < 0) {
			PMD_DRV_LOG(DEBUG, "SWFW phy%d lock released",
				    hw->bus.func);
		}
		hw->mac.ops.release_swfw_sync(hw, mask);

		/*
		 * This one is more tricky since it is common to all ports; but
		 * swfw_sync retries last long enough (1s) to be almost sure
		 * that if lock can not be taken it is due to an improper lock
		 * of the semaphore.
		 */
		mask = IGC_SWFW_EEP_SM;
		if (hw->mac.ops.acquire_swfw_sync(hw, mask) < 0)
			PMD_DRV_LOG(DEBUG, "SWFW common locks released");

		hw->mac.ops.release_swfw_sync(hw, mask);
	}

	return IGC_SUCCESS;
}

/*
 * free all rx/tx queues.
 */
static void
igc_dev_free_queues(struct rte_eth_dev *dev)
{
	uint16_t i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		eth_igc_rx_queue_release(dev->data->rx_queues[i]);
		dev->data->rx_queues[i] = NULL;
	}
	dev->data->nb_rx_queues = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		eth_igc_tx_queue_release(dev->data->tx_queues[i]);
		dev->data->tx_queues[i] = NULL;
	}
	dev->data->nb_tx_queues = 0;
}

static void
eth_igc_close(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct igc_adapter *adapter = IGC_DEV_PRIVATE(dev);
	int retry = 0;

	PMD_INIT_FUNC_TRACE();

	if (!adapter->stopped)
		eth_igc_stop(dev);

	igc_intr_other_disable(dev);
	do {
		int ret = rte_intr_callback_unregister(intr_handle,
				eth_igc_interrupt_handler, dev);
		if (ret >= 0 || ret == -ENOENT || ret == -EINVAL)
			break;

		PMD_DRV_LOG(ERR, "intr callback unregister failed: %d", ret);
		DELAY(200 * 1000); /* delay 200ms */
	} while (retry++ < 5);

	igc_phy_hw_reset(hw);
	igc_hw_control_release(hw);
	igc_dev_free_queues(dev);

	/* Reset any pending lock */
	igc_reset_swfw_lock(hw);
}

static void
igc_identify_hardware(struct rte_eth_dev *dev, struct rte_pci_device *pci_dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	hw->vendor_id = pci_dev->id.vendor_id;
	hw->device_id = pci_dev->id.device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;
	hw->subsystem_device_id = pci_dev->id.subsystem_device_id;
}

static int
eth_igc_dev_init(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct igc_adapter *igc = IGC_DEV_PRIVATE(dev);
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	int error = 0;

	PMD_INIT_FUNC_TRACE();
	dev->dev_ops = &eth_igc_ops;

	/*
	 * for secondary processes, we don't initialize any further as primary
	 * has already done this work. Only check we don't need a different
	 * RX function.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	rte_eth_copy_pci_info(dev, pci_dev);

	hw->back = pci_dev;
	hw->hw_addr = (void *)pci_dev->mem_resource[0].addr;

	igc_identify_hardware(dev, pci_dev);
	if (igc_setup_init_funcs(hw, false) != IGC_SUCCESS) {
		error = -EIO;
		goto err_late;
	}

	igc_get_bus_info(hw);

	/* Reset any pending lock */
	if (igc_reset_swfw_lock(hw) != IGC_SUCCESS) {
		error = -EIO;
		goto err_late;
	}

	/* Finish initialization */
	if (igc_setup_init_funcs(hw, true) != IGC_SUCCESS) {
		error = -EIO;
		goto err_late;
	}

	hw->mac.autoneg = 1;
	hw->phy.autoneg_wait_to_complete = 0;
	hw->phy.autoneg_advertised = IGC_ALL_SPEED_DUPLEX_2500;

	/* Copper options */
	if (hw->phy.media_type == igc_media_type_copper) {
		hw->phy.mdix = 0; /* AUTO_ALL_MODES */
		hw->phy.disable_polarity_correction = 0;
		hw->phy.ms_type = igc_ms_hw_default;
	}

	/*
	 * Start from a known state, this is important in reading the nvm
	 * and mac from that.
	 */
	igc_reset_hw(hw);

	/* Make sure we have a good EEPROM before we read from it */
	if (igc_validate_nvm_checksum(hw) < 0) {
		/*
		 * Some PCI-E parts fail the first check due to
		 * the link being in sleep state, call it again,
		 * if it fails a second time its a real issue.
		 */
		if (igc_validate_nvm_checksum(hw) < 0) {
			PMD_INIT_LOG(ERR, "EEPROM checksum invalid");
			error = -EIO;
			goto err_late;
		}
	}

	/* Read the permanent MAC address out of the EEPROM */
	if (igc_read_mac_addr(hw) != 0) {
		PMD_INIT_LOG(ERR, "EEPROM error while reading MAC address");
		error = -EIO;
		goto err_late;
	}

	/* Allocate memory for storing MAC addresses */
	dev->data->mac_addrs = rte_zmalloc("igc",
		RTE_ETHER_ADDR_LEN * hw->mac.rar_entry_count, 0);
	if (dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate %d bytes for storing MAC",
				RTE_ETHER_ADDR_LEN * hw->mac.rar_entry_count);
		error = -ENOMEM;
		goto err_late;
	}

	/* Copy the permanent MAC address */
	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac.addr,
			&dev->data->mac_addrs[0]);

	/* Now initialize the hardware */
	if (igc_hardware_init(hw) != 0) {
		PMD_INIT_LOG(ERR, "Hardware initialization failed");
		rte_free(dev->data->mac_addrs);
		dev->data->mac_addrs = NULL;
		error = -ENODEV;
		goto err_late;
	}

	/* Pass the information to the rte_eth_dev_close() that it should also
	 * release the private port resources.
	 */
	dev->data->dev_flags |= RTE_ETH_DEV_CLOSE_REMOVE;

	hw->mac.get_link_status = 1;
	igc->stopped = 0;

	/* Indicate SOL/IDER usage */
	if (igc_check_reset_block(hw) < 0)
		PMD_INIT_LOG(ERR,
			"PHY reset is blocked due to SOL/IDER session.");

	PMD_INIT_LOG(DEBUG, "port_id %d vendorID=0x%x deviceID=0x%x",
			dev->data->port_id, pci_dev->id.vendor_id,
			pci_dev->id.device_id);

	rte_intr_callback_register(&pci_dev->intr_handle,
			eth_igc_interrupt_handler, (void *)dev);

	/* enable uio/vfio intr/eventfd mapping */
	rte_intr_enable(&pci_dev->intr_handle);

	/* enable support intr */
	igc_intr_other_enable(dev);

	return 0;

err_late:
	igc_hw_control_release(hw);
	return error;
}

static int
eth_igc_dev_uninit(__rte_unused struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	eth_igc_close(eth_dev);
	return 0;
}

static int
eth_igc_reset(struct rte_eth_dev *dev)
{
	int ret;

	PMD_INIT_FUNC_TRACE();

	ret = eth_igc_dev_uninit(dev);
	if (ret)
		return ret;

	return eth_igc_dev_init(dev);
}

static int
eth_igc_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint32_t rctl;

	rctl = IGC_READ_REG(hw, IGC_RCTL);
	rctl |= (IGC_RCTL_UPE | IGC_RCTL_MPE);
	IGC_WRITE_REG(hw, IGC_RCTL, rctl);
	return 0;
}

static int
eth_igc_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint32_t rctl;

	rctl = IGC_READ_REG(hw, IGC_RCTL);
	rctl &= (~IGC_RCTL_UPE);
	if (dev->data->all_multicast == 1)
		rctl |= IGC_RCTL_MPE;
	else
		rctl &= (~IGC_RCTL_MPE);
	IGC_WRITE_REG(hw, IGC_RCTL, rctl);
	return 0;
}

static int
eth_igc_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint32_t rctl;

	rctl = IGC_READ_REG(hw, IGC_RCTL);
	rctl |= IGC_RCTL_MPE;
	IGC_WRITE_REG(hw, IGC_RCTL, rctl);
	return 0;
}

static int
eth_igc_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint32_t rctl;

	if (dev->data->promiscuous == 1)
		return 0;	/* must remain in all_multicast mode */

	rctl = IGC_READ_REG(hw, IGC_RCTL);
	rctl &= (~IGC_RCTL_MPE);
	IGC_WRITE_REG(hw, IGC_RCTL, rctl);
	return 0;
}

static int
eth_igc_fw_version_get(struct rte_eth_dev *dev, char *fw_version,
		       size_t fw_size)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct igc_fw_version fw;
	int ret;

	igc_get_fw_version(hw, &fw);

	/* if option rom is valid, display its version too */
	if (fw.or_valid) {
		ret = snprintf(fw_version, fw_size,
			 "%d.%d, 0x%08x, %d.%d.%d",
			 fw.eep_major, fw.eep_minor, fw.etrack_id,
			 fw.or_major, fw.or_build, fw.or_patch);
	/* no option rom */
	} else {
		if (fw.etrack_id != 0X0000) {
			ret = snprintf(fw_version, fw_size,
				 "%d.%d, 0x%08x",
				 fw.eep_major, fw.eep_minor,
				 fw.etrack_id);
		} else {
			ret = snprintf(fw_version, fw_size,
				 "%d.%d.%d",
				 fw.eep_major, fw.eep_minor,
				 fw.eep_build);
		}
	}

	ret += 1; /* add the size of '\0' */
	if (fw_size < (u32)ret)
		return ret;
	else
		return 0;
}

static int
eth_igc_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	dev_info->min_rx_bufsize = 256; /* See BSIZE field of RCTL register. */
	dev_info->max_rx_pktlen = MAX_RX_JUMBO_FRAME_SIZE;
	dev_info->max_mac_addrs = hw->mac.rar_entry_count;
	dev_info->rx_offload_capa = IGC_RX_OFFLOAD_ALL;
	dev_info->tx_offload_capa = IGC_TX_OFFLOAD_ALL;

	dev_info->max_rx_queues = IGC_QUEUE_PAIRS_NUM;
	dev_info->max_tx_queues = IGC_QUEUE_PAIRS_NUM;
	dev_info->max_vmdq_pools = 0;

	dev_info->hash_key_size = IGC_HKEY_MAX_INDEX * sizeof(uint32_t);
	dev_info->reta_size = ETH_RSS_RETA_SIZE_128;
	dev_info->flow_type_rss_offloads = IGC_RSS_OFFLOAD_ALL;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_thresh = {
			.pthresh = IGC_DEFAULT_RX_PTHRESH,
			.hthresh = IGC_DEFAULT_RX_HTHRESH,
			.wthresh = IGC_DEFAULT_RX_WTHRESH,
		},
		.rx_free_thresh = IGC_DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
		.offloads = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = IGC_DEFAULT_TX_PTHRESH,
			.hthresh = IGC_DEFAULT_TX_HTHRESH,
			.wthresh = IGC_DEFAULT_TX_WTHRESH,
		},
		.offloads = 0,
	};

	dev_info->rx_desc_lim = rx_desc_lim;
	dev_info->tx_desc_lim = tx_desc_lim;

	dev_info->speed_capa = ETH_LINK_SPEED_10M_HD | ETH_LINK_SPEED_10M |
			ETH_LINK_SPEED_100M_HD | ETH_LINK_SPEED_100M |
			ETH_LINK_SPEED_1G | ETH_LINK_SPEED_2_5G;

	dev_info->max_mtu = dev_info->max_rx_pktlen - IGC_ETH_OVERHEAD;
	dev_info->min_mtu = RTE_ETHER_MIN_MTU;
	return 0;
}

static int
eth_igc_led_on(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	return igc_led_on(hw) == IGC_SUCCESS ? 0 : -ENOTSUP;
}

static int
eth_igc_led_off(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	return igc_led_off(hw) == IGC_SUCCESS ? 0 : -ENOTSUP;
}

static const uint32_t *
eth_igc_supported_ptypes_get(__rte_unused struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		/* refers to rx_desc_pkt_info_to_pkt_type() */
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_L3_IPV4_EXT,
		RTE_PTYPE_L3_IPV6,
		RTE_PTYPE_L3_IPV6_EXT,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_TUNNEL_IP,
		RTE_PTYPE_INNER_L3_IPV6,
		RTE_PTYPE_INNER_L3_IPV6_EXT,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_UNKNOWN
	};

	return ptypes;
}

static int
eth_igc_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint32_t frame_size = mtu + IGC_ETH_OVERHEAD;
	uint32_t rctl;

	/* if extend vlan has been enabled */
	if (IGC_READ_REG(hw, IGC_CTRL_EXT) & IGC_CTRL_EXT_EXT_VLAN)
		frame_size += VLAN_TAG_SIZE;

	/* check that mtu is within the allowed range */
	if (mtu < RTE_ETHER_MIN_MTU ||
		frame_size > MAX_RX_JUMBO_FRAME_SIZE)
		return -EINVAL;

	/*
	 * refuse mtu that requires the support of scattered packets when
	 * this feature has not been enabled before.
	 */
	if (!dev->data->scattered_rx &&
	    frame_size > dev->data->min_rx_buf_size - RTE_PKTMBUF_HEADROOM)
		return -EINVAL;

	rctl = IGC_READ_REG(hw, IGC_RCTL);

	/* switch to jumbo mode if needed */
	if (mtu > RTE_ETHER_MTU) {
		dev->data->dev_conf.rxmode.offloads |=
			DEV_RX_OFFLOAD_JUMBO_FRAME;
		rctl |= IGC_RCTL_LPE;
	} else {
		dev->data->dev_conf.rxmode.offloads &=
			~DEV_RX_OFFLOAD_JUMBO_FRAME;
		rctl &= ~IGC_RCTL_LPE;
	}
	IGC_WRITE_REG(hw, IGC_RCTL, rctl);

	/* update max frame size */
	dev->data->dev_conf.rxmode.max_rx_pkt_len = frame_size;

	IGC_WRITE_REG(hw, IGC_RLPML,
			dev->data->dev_conf.rxmode.max_rx_pkt_len);

	return 0;
}

static int
eth_igc_rar_set(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr,
		uint32_t index, uint32_t pool)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	igc_rar_set(hw, mac_addr->addr_bytes, index);
	RTE_SET_USED(pool);
	return 0;
}

static void
eth_igc_rar_clear(struct rte_eth_dev *dev, uint32_t index)
{
	uint8_t addr[RTE_ETHER_ADDR_LEN];
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	memset(addr, 0, sizeof(addr));
	igc_rar_set(hw, addr, index);
}

static int
eth_igc_default_mac_addr_set(struct rte_eth_dev *dev,
			struct rte_ether_addr *addr)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	igc_rar_set(hw, addr->addr_bytes, 0);
	return 0;
}

static int
eth_igc_set_mc_addr_list(struct rte_eth_dev *dev,
			 struct rte_ether_addr *mc_addr_set,
			 uint32_t nb_mc_addr)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	igc_update_mc_addr_list(hw, (u8 *)mc_addr_set, nb_mc_addr);
	return 0;
}

static int
eth_igc_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	PMD_INIT_FUNC_TRACE();
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct igc_adapter), eth_igc_dev_init);
}

static int
eth_igc_pci_remove(struct rte_pci_device *pci_dev)
{
	PMD_INIT_FUNC_TRACE();
	return rte_eth_dev_pci_generic_remove(pci_dev, eth_igc_dev_uninit);
}

static struct rte_pci_driver rte_igc_pmd = {
	.id_table = pci_id_igc_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = eth_igc_pci_probe,
	.remove = eth_igc_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_igc, rte_igc_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_igc, pci_id_igc_map);
RTE_PMD_REGISTER_KMOD_DEP(net_igc, "* igb_uio | uio_pci_generic | vfio-pci");
