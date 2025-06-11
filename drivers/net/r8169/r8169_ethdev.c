/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>

#include <rte_eal.h>

#include <rte_common.h>
#include <rte_interrupts.h>
#include <rte_byteorder.h>
#include <rte_pci.h>
#include <bus_pci_driver.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <rte_memory.h>
#include <rte_malloc.h>
#include <dev_driver.h>

#include "r8169_ethdev.h"
#include "r8169_compat.h"
#include "r8169_logs.h"
#include "r8169_hw.h"
#include "r8169_dash.h"

static int rtl_dev_configure(struct rte_eth_dev *dev __rte_unused);
static int rtl_dev_start(struct rte_eth_dev *dev);
static int rtl_dev_stop(struct rte_eth_dev *dev);
static int rtl_dev_reset(struct rte_eth_dev *dev);
static int rtl_dev_close(struct rte_eth_dev *dev);
static int rtl_dev_link_update(struct rte_eth_dev *dev, int wait __rte_unused);
static int rtl_dev_set_link_up(struct rte_eth_dev *dev);
static int rtl_dev_set_link_down(struct rte_eth_dev *dev);
static int rtl_dev_infos_get(struct rte_eth_dev *dev,
			     struct rte_eth_dev_info *dev_info);
static int rtl_dev_stats_get(struct rte_eth_dev *dev,
			     struct rte_eth_stats *rte_stats);
static int rtl_dev_stats_reset(struct rte_eth_dev *dev);
static int rtl_promiscuous_enable(struct rte_eth_dev *dev);
static int rtl_promiscuous_disable(struct rte_eth_dev *dev);
static int rtl_allmulticast_enable(struct rte_eth_dev *dev);
static int rtl_allmulticast_disable(struct rte_eth_dev *dev);
static int rtl_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);
static int rtl_fw_version_get(struct rte_eth_dev *dev, char *fw_version,
			      size_t fw_size);

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_r8169_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0x8125) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0x8162) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0x8126) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0x5000) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0x8168) },
	{.vendor_id = 0, /* sentinel */ },
};

static const struct rte_eth_desc_lim rx_desc_lim = {
	.nb_max   = RTL_MAX_RX_DESC,
	.nb_min   = RTL_MIN_RX_DESC,
	.nb_align = RTL_DESC_ALIGN,
};

static const struct rte_eth_desc_lim tx_desc_lim = {
	.nb_max         = RTL_MAX_TX_DESC,
	.nb_min         = RTL_MIN_TX_DESC,
	.nb_align       = RTL_DESC_ALIGN,
	.nb_seg_max     = RTL_MAX_TX_SEG,
	.nb_mtu_seg_max = RTL_MAX_TX_SEG,
};

static const struct eth_dev_ops rtl_eth_dev_ops = {
	.dev_configure	      = rtl_dev_configure,
	.dev_start	      = rtl_dev_start,
	.dev_stop	      = rtl_dev_stop,
	.dev_close	      = rtl_dev_close,
	.dev_reset	      = rtl_dev_reset,
	.dev_set_link_up      = rtl_dev_set_link_up,
	.dev_set_link_down    = rtl_dev_set_link_down,
	.dev_infos_get        = rtl_dev_infos_get,

	.promiscuous_enable   = rtl_promiscuous_enable,
	.promiscuous_disable  = rtl_promiscuous_disable,
	.allmulticast_enable  = rtl_allmulticast_enable,
	.allmulticast_disable = rtl_allmulticast_disable,

	.link_update          = rtl_dev_link_update,

	.stats_get            = rtl_dev_stats_get,
	.stats_reset          = rtl_dev_stats_reset,

	.mtu_set              = rtl_dev_mtu_set,

	.fw_version_get       = rtl_fw_version_get,

	.rx_queue_setup       = rtl_rx_queue_setup,
	.rx_queue_release     = rtl_rx_queue_release,
	.rxq_info_get         = rtl_rxq_info_get,

	.tx_queue_setup       = rtl_tx_queue_setup,
	.tx_queue_release     = rtl_tx_queue_release,
	.tx_done_cleanup      = rtl_tx_done_cleanup,
	.txq_info_get         = rtl_txq_info_get,
};

static int
rtl_dev_configure(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

static void
rtl_disable_intr(struct rtl_hw *hw)
{
	PMD_INIT_FUNC_TRACE();
	if (rtl_is_8125(hw)) {
		RTL_W32(hw, IMR0_8125, 0x0000);
		RTL_W32(hw, ISR0_8125, RTL_R32(hw, ISR0_8125));
	} else {
		RTL_W16(hw, IntrMask, 0x0000);
		RTL_W16(hw, IntrStatus, RTL_R16(hw, IntrStatus));
	}
}

static void
rtl_enable_intr(struct rtl_hw *hw)
{
	PMD_INIT_FUNC_TRACE();
	if (rtl_is_8125(hw))
		RTL_W32(hw, IMR0_8125, LinkChg);
	else
		RTL_W16(hw, IntrMask, LinkChg);
}

static int
_rtl_setup_link(struct rte_eth_dev *dev)
{
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;
	u64 adv = 0;
	u32 *link_speeds = &dev->data->dev_conf.link_speeds;
	unsigned int speed_mode;

	/* Setup link speed and duplex */
	if (*link_speeds == RTE_ETH_LINK_SPEED_AUTONEG) {
		switch (hw->mcfg) {
		case CFG_METHOD_48:
		case CFG_METHOD_49:
		case CFG_METHOD_50:
		case CFG_METHOD_51:
		case CFG_METHOD_52:
		case CFG_METHOD_53:
		case CFG_METHOD_54:
		case CFG_METHOD_55:
		case CFG_METHOD_56:
		case CFG_METHOD_57:
			speed_mode = SPEED_2500;
			break;
		case CFG_METHOD_69:
		case CFG_METHOD_70:
		case CFG_METHOD_71:
			speed_mode = SPEED_5000;
			break;
		default:
			speed_mode = SPEED_1000;
			break;
		}

		rtl_set_link_option(hw, AUTONEG_ENABLE, speed_mode, DUPLEX_FULL,
				    rtl_fc_full);
	} else if (*link_speeds != 0) {
		if (*link_speeds & ~(RTE_ETH_LINK_SPEED_10M_HD | RTE_ETH_LINK_SPEED_10M |
				     RTE_ETH_LINK_SPEED_100M_HD | RTE_ETH_LINK_SPEED_100M |
				     RTE_ETH_LINK_SPEED_1G | RTE_ETH_LINK_SPEED_2_5G |
				     RTE_ETH_LINK_SPEED_5G | RTE_ETH_LINK_SPEED_FIXED))
			goto error_invalid_config;

		if (*link_speeds & RTE_ETH_LINK_SPEED_10M_HD) {
			hw->speed = SPEED_10;
			hw->duplex = DUPLEX_HALF;
			adv |= ADVERTISE_10_HALF;
		}
		if (*link_speeds & RTE_ETH_LINK_SPEED_10M) {
			hw->speed = SPEED_10;
			hw->duplex = DUPLEX_FULL;
			adv |= ADVERTISE_10_FULL;
		}
		if (*link_speeds & RTE_ETH_LINK_SPEED_100M_HD) {
			hw->speed = SPEED_100;
			hw->duplex = DUPLEX_HALF;
			adv |= ADVERTISE_100_HALF;
		}
		if (*link_speeds & RTE_ETH_LINK_SPEED_100M) {
			hw->speed = SPEED_100;
			hw->duplex = DUPLEX_FULL;
			adv |= ADVERTISE_100_FULL;
		}
		if (*link_speeds & RTE_ETH_LINK_SPEED_1G) {
			hw->speed = SPEED_1000;
			hw->duplex = DUPLEX_FULL;
			adv |= ADVERTISE_1000_FULL;
		}
		if (*link_speeds & RTE_ETH_LINK_SPEED_2_5G) {
			hw->speed = SPEED_2500;
			hw->duplex = DUPLEX_FULL;
			adv |= ADVERTISE_2500_FULL;
		}
		if (*link_speeds & RTE_ETH_LINK_SPEED_5G) {
			hw->speed = SPEED_5000;
			hw->duplex = DUPLEX_FULL;
			adv |= ADVERTISE_5000_FULL;
		}

		hw->autoneg = AUTONEG_ENABLE;
		hw->advertising = adv;
	}

	rtl_set_speed(hw);

	return 0;

error_invalid_config:
	PMD_INIT_LOG(ERR, "Invalid advertised speeds (%u) for port %u",
		     dev->data->dev_conf.link_speeds, dev->data->port_id);
	rtl_stop_queues(dev);
	return -EINVAL;
}

static int
rtl_setup_link(struct rte_eth_dev *dev)
{
#ifdef RTE_EXEC_ENV_FREEBSD
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;
	struct rte_eth_link link;
	int count;
#endif

	_rtl_setup_link(dev);

#ifdef RTE_EXEC_ENV_FREEBSD
	for (count = 0; count < R8169_LINK_CHECK_TIMEOUT; count++) {
		if (!(RTL_R16(hw, PHYstatus) & LinkStatus)) {
			rte_delay_ms(R8169_LINK_CHECK_INTERVAL);
			continue;
		}

		rtl_dev_link_update(dev, 0);

		rte_eth_linkstatus_get(dev, &link);

		return 0;
	}
#endif
	return 0;
}

/* Set PCI configuration space offset 0x79 to setting */
static void
set_offset79(struct rte_pci_device *pdev, u8 setting)
{
	u8 device_control;

	PCI_READ_CONFIG_BYTE(pdev, &device_control, 0x79);
	device_control &= ~0x70;
	device_control |= setting;
	PCI_WRITE_CONFIG_BYTE(pdev, &device_control, 0x79);
}

/*
 * Configure device link speed and setup link.
 * It returns 0 on success.
 */
static int
rtl_dev_start(struct rte_eth_dev *dev)
{
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	int err;

	/* Disable uio/vfio intr/eventfd mapping */
	rte_intr_disable(intr_handle);

	rtl_powerup_pll(hw);

	rtl_hw_ephy_config(hw);

	rtl_hw_phy_config(hw);

	rtl_hw_config(hw);

	switch (hw->mcfg) {
	case CFG_METHOD_21:
	case CFG_METHOD_22:
	case CFG_METHOD_23:
	case CFG_METHOD_24:
	case CFG_METHOD_25:
	case CFG_METHOD_26:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
	case CFG_METHOD_37:
		set_offset79(pci_dev, 0x40);
		break;
	}

	/* Initialize transmission unit */
	rtl_tx_init(dev);

	/* This can fail when allocating mbufs for descriptor rings */
	err = rtl_rx_init(dev);
	if (err) {
		PMD_INIT_LOG(ERR, "Unable to initialize RX hardware");
		goto error;
	}

	/* This can fail when allocating mem for tally counters */
	err = rtl_tally_init(dev);
	if (err)
		goto error;

	/* Enable uio/vfio intr/eventfd mapping */
	rte_intr_enable(intr_handle);

	/* Resume enabled intr since hw reset */
	rtl_enable_intr(hw);

	rtl_setup_link(dev);

	rtl_mdio_write(hw, 0x1F, 0x0000);

	return 0;
error:
	rtl_stop_queues(dev);
	return -EIO;
}

/*
 * Stop device: disable RX and TX functions to allow for reconfiguring.
 */
static int
rtl_dev_stop(struct rte_eth_dev *dev)
{
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;
	struct rte_eth_link link;

	rtl_disable_intr(hw);

	rtl_nic_reset(hw);

	switch (hw->mcfg) {
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_52:
	case CFG_METHOD_53:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_56:
	case CFG_METHOD_57:
	case CFG_METHOD_69:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
		rtl_mac_ocp_write(hw, 0xE00A, hw->mcu_pme_setting);
		break;
	}

	rtl_powerdown_pll(hw);

	rtl_stop_queues(dev);

	rtl_tally_free(dev);

	/* Clear the recorded link status */
	memset(&link, 0, sizeof(link));
	rte_eth_linkstatus_set(dev, &link);

	return 0;
}

static int
rtl_dev_set_link_up(struct rte_eth_dev *dev)
{
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;

	rtl_powerup_pll(hw);

	return 0;
}

static int
rtl_dev_set_link_down(struct rte_eth_dev *dev)
{
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;

	/* mcu pme intr masks */
	switch (hw->mcfg) {
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_52:
	case CFG_METHOD_53:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_56:
	case CFG_METHOD_57:
	case CFG_METHOD_69:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
		rtl_mac_ocp_write(hw, 0xE00A, hw->mcu_pme_setting & ~(BIT_11 | BIT_14));
		break;
	}

	rtl_powerdown_pll(hw);

	return 0;
}

static int
rtl_dev_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;

	dev_info->min_rx_bufsize = 1024;
	dev_info->max_rx_pktlen = JUMBO_FRAME_9K;
	dev_info->max_mac_addrs = 1;

	dev_info->max_rx_queues = 1;
	dev_info->max_tx_queues = 1;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_free_thresh = RTL_RX_FREE_THRESH,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_free_thresh = RTL_TX_FREE_THRESH,
	};

	dev_info->rx_desc_lim = rx_desc_lim;
	dev_info->tx_desc_lim = tx_desc_lim;

	dev_info->speed_capa = RTE_ETH_LINK_SPEED_10M_HD | RTE_ETH_LINK_SPEED_10M |
			       RTE_ETH_LINK_SPEED_100M_HD | RTE_ETH_LINK_SPEED_100M |
			       RTE_ETH_LINK_SPEED_1G;

	switch (hw->chipset_name) {
	case RTL8126A:
		dev_info->speed_capa |= RTE_ETH_LINK_SPEED_5G;
	/* fallthrough */
	case RTL8125A:
	case RTL8125B:
	case RTL8125BP:
	case RTL8125D:
		dev_info->speed_capa |= RTE_ETH_LINK_SPEED_2_5G;
		break;
	}

	dev_info->min_mtu = RTE_ETHER_MIN_MTU;
	dev_info->max_mtu = dev_info->max_rx_pktlen - RTL_ETH_OVERHEAD;

	dev_info->rx_offload_capa = (rtl_get_rx_port_offloads() |
				     dev_info->rx_queue_offload_capa);
	dev_info->tx_offload_capa = rtl_get_tx_port_offloads();

	return 0;
}

static int
rtl_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;

	int rx_mode = AcceptBroadcast | AcceptMulticast | AcceptMyPhys | AcceptAllPhys;

	RTL_W32(hw, RxConfig, rx_mode | (RTL_R32(hw, RxConfig)));
	rtl_allmulticast_enable(dev);

	return 0;
}

static int
rtl_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;
	int rx_mode = ~AcceptAllPhys;

	RTL_W32(hw, RxConfig, rx_mode & (RTL_R32(hw, RxConfig)));

	if (dev->data->all_multicast == 1)
		rtl_allmulticast_enable(dev);
	else
		rtl_allmulticast_disable(dev);

	return 0;
}

static int
rtl_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;

	RTL_W32(hw, MAR0 + 0, 0xffffffff);
	RTL_W32(hw, MAR0 + 4, 0xffffffff);

	return 0;
}

static int
rtl_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;

	if (dev->data->promiscuous == 1)
		return 0; /* Must remain in all_multicast mode */

	RTL_W32(hw, MAR0 + 0, 0);
	RTL_W32(hw, MAR0 + 4, 0);

	return 0;
}

static int
rtl_dev_stats_reset(struct rte_eth_dev *dev)
{
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;

	rtl_clear_tally_stats(hw);

	memset(&adapter->sw_stats, 0, sizeof(adapter->sw_stats));

	return 0;
}

static void
rtl_sw_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *rte_stats)
{
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_sw_stats *sw_stats = &adapter->sw_stats;

	rte_stats->ibytes = sw_stats->rx_bytes;
	rte_stats->obytes = sw_stats->tx_bytes;
}

static int
rtl_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *rte_stats)
{
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;

	rtl_get_tally_stats(hw, rte_stats);
	rtl_sw_stats_get(dev, rte_stats);

	return 0;
}

/* Return 0 means link status changed, -1 means not changed */
static int
rtl_dev_link_update(struct rte_eth_dev *dev, int wait __rte_unused)
{
	struct rte_eth_link link, old;
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;
	u32 speed;
	u16 status;

	link.link_status = RTE_ETH_LINK_DOWN;
	link.link_speed = 0;
	link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	link.link_autoneg = RTE_ETH_LINK_AUTONEG;

	memset(&old, 0, sizeof(old));

	/* Load old link status */
	rte_eth_linkstatus_get(dev, &old);

	/* Read current link status */
	status = RTL_R16(hw, PHYstatus);

	if (status & LinkStatus) {
		link.link_status = RTE_ETH_LINK_UP;

		if (status & FullDup) {
			link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
			switch (hw->mcfg) {
			case CFG_METHOD_21:
			case CFG_METHOD_22:
			case CFG_METHOD_23:
			case CFG_METHOD_24:
			case CFG_METHOD_25:
			case CFG_METHOD_26:
			case CFG_METHOD_27:
			case CFG_METHOD_28:
			case CFG_METHOD_29:
			case CFG_METHOD_30:
			case CFG_METHOD_31:
			case CFG_METHOD_32:
			case CFG_METHOD_33:
			case CFG_METHOD_34:
			case CFG_METHOD_35:
			case CFG_METHOD_36:
			case CFG_METHOD_37:
			case CFG_METHOD_48:
				RTL_W32(hw, TxConfig, (RTL_R32(hw, TxConfig) |
						      (BIT_24 | BIT_25)) & ~BIT_19);
				break;
			}
		} else {
			link.link_duplex = RTE_ETH_LINK_HALF_DUPLEX;
			switch (hw->mcfg) {
			case CFG_METHOD_21:
			case CFG_METHOD_22:
			case CFG_METHOD_23:
			case CFG_METHOD_24:
			case CFG_METHOD_25:
			case CFG_METHOD_26:
			case CFG_METHOD_27:
			case CFG_METHOD_28:
			case CFG_METHOD_29:
			case CFG_METHOD_30:
			case CFG_METHOD_31:
			case CFG_METHOD_32:
			case CFG_METHOD_33:
			case CFG_METHOD_34:
			case CFG_METHOD_35:
			case CFG_METHOD_36:
			case CFG_METHOD_37:
			case CFG_METHOD_48:
				RTL_W32(hw, TxConfig, (RTL_R32(hw, TxConfig) | BIT_25) &
						      ~(BIT_19 | BIT_24));
				break;
			}
		}

		/*
		 * The PHYstatus register for the RTL8168 is 8 bits,
		 * while for the RTL8125 and RTL8126, it is 16 bits.
		 */
		if (status & _5000bpsF && rtl_is_8125(hw))
			speed = 5000;
		else if (status & _2500bpsF && rtl_is_8125(hw))
			speed = 2500;
		else if (status & _1000bpsF)
			speed = 1000;
		else if (status & _100bps)
			speed = 100;
		else
			speed = 10;

		link.link_speed = speed;
	}

	if (link.link_status == old.link_status)
		return -1;

	rte_eth_linkstatus_set(dev, &link);

	return 0;
}

static void
rtl_dev_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;
	uint32_t intr;

	if (rtl_is_8125(hw))
		intr = RTL_R32(hw, ISR0_8125);
	else
		intr = RTL_R16(hw, IntrStatus);

	/* Clear all cause mask */
	rtl_disable_intr(hw);

	if (intr & LinkChg)
		rtl_dev_link_update(dev, 0);
	else
		PMD_DRV_LOG(ERR, "r8169: interrupt unhandled.");

	rtl_enable_intr(hw);
}

/*
 * Reset and stop device.
 */
static int
rtl_dev_close(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;
	int retries = 0;
	int ret_unreg, ret_stp;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (HW_DASH_SUPPORT_DASH(hw))
		rtl_driver_stop(hw);

	ret_stp = rtl_dev_stop(dev);

	rtl_free_queues(dev);

	/* Reprogram the RAR[0] in case user changed it. */
	rtl_rar_set(hw, hw->mac_addr);

	/* Disable uio intr before callback unregister */
	rte_intr_disable(intr_handle);

	do {
		ret_unreg = rte_intr_callback_unregister(intr_handle, rtl_dev_interrupt_handler,
							 dev);
		if (ret_unreg >= 0 || ret_unreg == -ENOENT)
			break;
		else if (ret_unreg != -EAGAIN)
			PMD_DRV_LOG(ERR, "r8169: intr callback unregister failed: %d", ret_unreg);

		rte_delay_ms(100);
	} while (retries++ < (10 + 90));

	return ret_stp;
}

static int
rtl_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;
	uint32_t frame_size = mtu + RTL_ETH_OVERHEAD;

	hw->mtu = mtu;

	RTL_W16(hw, RxMaxSize, frame_size);

	return 0;
}

static int
rtl_fw_version_get(struct rte_eth_dev *dev, char *fw_version, size_t fw_size)
{
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;
	int ret;

	ret = snprintf(fw_version, fw_size, "0x%08x", hw->hw_ram_code_ver);

	ret += 1; /* Add the size of '\0' */
	if (fw_size < (u32)ret)
		return ret;
	else
		return 0;
}

static int
rtl_dev_init(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;
	struct rte_ether_addr *perm_addr = (struct rte_ether_addr *)hw->mac_addr;
	char buf[RTE_ETHER_ADDR_FMT_SIZE];

	dev->dev_ops = &rtl_eth_dev_ops;
	dev->tx_pkt_burst = &rtl_xmit_pkts;
	dev->rx_pkt_burst = &rtl_recv_pkts;

	/* For secondary processes, the primary process has done all the work. */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		if (dev->data->scattered_rx)
			dev->rx_pkt_burst = &rtl_recv_scattered_pkts;
		return 0;
	}

	/* R8169 uses BAR2 */
	hw->mmio_addr = (u8 *)pci_dev->mem_resource[2].addr;

	rtl_get_mac_version(hw, pci_dev);

	if (rtl_set_hw_ops(hw))
		return -ENOTSUP;

	rtl_disable_intr(hw);

	rtl_hw_initialize(hw);

	/* Read the permanent MAC address out of ROM */
	rtl_get_mac_address(hw, perm_addr);

	if (!rte_is_valid_assigned_ether_addr(perm_addr)) {
		rte_eth_random_addr(&perm_addr->addr_bytes[0]);

		rte_ether_format_addr(buf, sizeof(buf), perm_addr);

		PMD_INIT_LOG(NOTICE, "r8169: Assign randomly generated MAC address %s", buf);
	}

	/* Allocate memory for storing MAC addresses */
	dev->data->mac_addrs = rte_zmalloc("r8169", RTE_ETHER_ADDR_LEN, 0);

	if (dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "MAC Malloc failed");
		return -ENOMEM;
	}

	/* Copy the permanent MAC address */
	rte_ether_addr_copy(perm_addr, &dev->data->mac_addrs[0]);

	rtl_rar_set(hw, &perm_addr->addr_bytes[0]);

	rte_intr_callback_register(intr_handle, rtl_dev_interrupt_handler, dev);

	/* Enable uio/vfio intr/eventfd mapping */
	rte_intr_enable(intr_handle);

	return 0;
}

static int
rtl_dev_uninit(struct rte_eth_dev *dev)
{
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -EPERM;

	rtl_dev_close(dev);

	return 0;
}

static int
rtl_dev_reset(struct rte_eth_dev *dev)
{
	int ret;

	ret = rtl_dev_uninit(dev);
	if (ret)
		return ret;

	ret = rtl_dev_init(dev);

	return ret;
}

static int
rtl_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	      struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev, sizeof(struct rtl_adapter),
					     rtl_dev_init);
}

static int
rtl_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, rtl_dev_uninit);
}

static struct rte_pci_driver rte_r8169_pmd = {
	.id_table  = pci_id_r8169_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe     = rtl_pci_probe,
	.remove    = rtl_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_r8169, rte_r8169_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_r8169, pci_id_r8169_map);
RTE_PMD_REGISTER_KMOD_DEP(net_r8169, "* igb_uio | uio_pci_generic | vfio-pci");

RTE_LOG_REGISTER_SUFFIX(r8169_logtype_init, init, NOTICE)
RTE_LOG_REGISTER_SUFFIX(r8169_logtype_driver, driver, NOTICE)
#ifdef RTE_ETHDEV_DEBUG_RX
RTE_LOG_REGISTER_SUFFIX(r8169_logtype_rx, rx, DEBUG)
#endif
#ifdef RTE_ETHDEV_DEBUG_TX
RTE_LOG_REGISTER_SUFFIX(r8169_logtype_tx, tx, DEBUG)
#endif
