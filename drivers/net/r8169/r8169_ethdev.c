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

static int rtl_dev_configure(struct rte_eth_dev *dev);
static int rtl_dev_start(struct rte_eth_dev *dev);
static int rtl_dev_stop(struct rte_eth_dev *dev);
static int rtl_dev_reset(struct rte_eth_dev *dev);
static int rtl_dev_close(struct rte_eth_dev *dev);

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_r8169_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0x8125) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0x8162) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0x8126) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0x5000) },
	{.vendor_id = 0, /* sentinel */ },
};

static const struct eth_dev_ops rtl_eth_dev_ops = {
	.dev_configure	      = rtl_dev_configure,
	.dev_start	      = rtl_dev_start,
	.dev_stop	      = rtl_dev_stop,
	.dev_close	      = rtl_dev_close,
	.dev_reset	      = rtl_dev_reset,
};

static int
rtl_dev_configure(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
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
	int err;

	rtl_powerup_pll(hw);

	rtl_hw_ephy_config(hw);

	rtl_hw_phy_config(hw);

	rtl_hw_config(hw);

	/* Initialize transmission unit */
	rtl_tx_init(dev);

	/* This can fail when allocating mbufs for descriptor rings */
	err = rtl_rx_init(dev);
	if (err) {
		PMD_INIT_LOG(ERR, "Unable to initialize RX hardware");
		goto error;
	}

	rtl_mdio_write(hw, 0x1F, 0x0000);

	return 0;
error:
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

	rtl_nic_reset(hw);

	switch (hw->mcfg) {
	case CFG_METHOD_48 ... CFG_METHOD_57:
	case CFG_METHOD_69 ... CFG_METHOD_71:
		rtl_mac_ocp_write(hw, 0xE00A, hw->mcu_pme_setting);
		break;
	}

	rtl_powerdown_pll(hw);

	return 0;
}

/*
 * Reset and stop device.
 */
static int
rtl_dev_close(struct rte_eth_dev *dev)
{
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;
	int ret_stp;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	ret_stp = rtl_dev_stop(dev);

	/* Reprogram the RAR[0] in case user changed it. */
	rtl_rar_set(hw, hw->mac_addr);

	return ret_stp;
}

static int
rtl_dev_init(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;
	struct rte_ether_addr *perm_addr = (struct rte_ether_addr *)hw->mac_addr;
	char buf[RTE_ETHER_ADDR_FMT_SIZE];

	dev->dev_ops = &rtl_eth_dev_ops;
	dev->tx_pkt_burst = &rtl_xmit_pkts;
	dev->rx_pkt_burst = &rtl_recv_pkts;

	/* For secondary processes, the primary process has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	hw->mmio_addr = (u8 *)pci_dev->mem_resource[2].addr; /* RTL8169 uses BAR2 */

	rtl_get_mac_version(hw, pci_dev);

	if (rtl_set_hw_ops(hw))
		return -ENOTSUP;

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
