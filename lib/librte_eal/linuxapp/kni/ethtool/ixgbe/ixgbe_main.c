// SPDX-License-Identifier: GPL-2.0
/*******************************************************************************

  Intel 10 Gigabit PCI Express Linux driver
  Copyright(c) 1999 - 2012 Intel Corporation.

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

/******************************************************************************
 Copyright (c)2006 - 2007 Myricom, Inc. for some LRO specific code
******************************************************************************/
#include <linux/types.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/vmalloc.h>
#include <linux/highmem.h>
#include <linux/string.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#ifdef HAVE_SCTP
#include <linux/sctp.h>
#endif
#include <linux/pkt_sched.h>
#include <linux/ipv6.h>
#ifdef NETIF_F_TSO
#include <net/checksum.h>
#ifdef NETIF_F_TSO6
#include <net/ip6_checksum.h>
#endif
#endif
#ifdef SIOCETHTOOL
#include <linux/ethtool.h>
#endif

#include "ixgbe.h"

#undef CONFIG_DCA
#undef CONFIG_DCA_MODULE

char ixgbe_driver_name[] = "ixgbe";
#define DRV_HW_PERF

#ifndef CONFIG_IXGBE_NAPI
#define DRIVERNAPI
#else
#define DRIVERNAPI "-NAPI"
#endif

#define FPGA

#define VMDQ_TAG

#define MAJ 3
#define MIN 9
#define BUILD 17
#define DRV_VERSION	__stringify(MAJ) "." __stringify(MIN) "." \
			__stringify(BUILD) DRIVERNAPI DRV_HW_PERF FPGA VMDQ_TAG
const char ixgbe_driver_version[] = DRV_VERSION;

/* ixgbe_pci_tbl - PCI Device ID Table
 *
 * Wildcard entries (PCI_ANY_ID) should come last
 * Last entry must be all 0s
 *
 * { Vendor ID, Device ID, SubVendor ID, SubDevice ID,
 *   Class, Class Mask, private data (not used) }
 */
const struct pci_device_id ixgbe_pci_tbl[] = {
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598AF_DUAL_PORT)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598AF_SINGLE_PORT)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598AT)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598AT2)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598EB_CX4)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598_CX4_DUAL_PORT)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598_DA_DUAL_PORT)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598_SR_DUAL_PORT_EM)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598EB_XF_LR)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598EB_SFP_LOM)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598_BX)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_KX4)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_XAUI_LOM)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_KR)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_SFP)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_SFP_EM)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_KX4_MEZZ)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_CX4)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_BACKPLANE_FCOE)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_SFP_FCOE)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_T3_LOM)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_COMBO_BACKPLANE)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X540T)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_SFP_SF2)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_LS)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599EN_SFP)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_QSFP_SF_QP)},
	/* required last entry */
	{0, }
};

#if defined(CONFIG_DCA) || defined(CONFIG_DCA_MODULE)
static int ixgbe_notify_dca(struct notifier_block *, unsigned long event,
			    void *p);
static struct notifier_block dca_notifier = {
	.notifier_call	= ixgbe_notify_dca,
	.next		= NULL,
	.priority	= 0
};

#endif
MODULE_AUTHOR("Intel Corporation, <linux.nics@intel.com>");
MODULE_DESCRIPTION("Intel(R) 10 Gigabit PCI Express Network Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

#define DEFAULT_DEBUG_LEVEL_SHIFT 3


static void ixgbe_release_hw_control(struct ixgbe_adapter *adapter)
{
	u32 ctrl_ext;

	/* Let firmware take over control of h/w */
	ctrl_ext = IXGBE_READ_REG(&adapter->hw, IXGBE_CTRL_EXT);
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_CTRL_EXT,
			ctrl_ext & ~IXGBE_CTRL_EXT_DRV_LOAD);
}

#ifdef NO_VNIC
static void ixgbe_get_hw_control(struct ixgbe_adapter *adapter)
{
	u32 ctrl_ext;

	/* Let firmware know the driver has taken over */
	ctrl_ext = IXGBE_READ_REG(&adapter->hw, IXGBE_CTRL_EXT);
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_CTRL_EXT,
			ctrl_ext | IXGBE_CTRL_EXT_DRV_LOAD);
}
#endif


static void ixgbe_update_xoff_rx_lfc(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	struct ixgbe_hw_stats *hwstats = &adapter->stats;
	int i;
	u32 data;

	if ((hw->fc.current_mode != ixgbe_fc_full) &&
	    (hw->fc.current_mode != ixgbe_fc_rx_pause))
		return;

	switch (hw->mac.type) {
	case ixgbe_mac_82598EB:
		data = IXGBE_READ_REG(hw, IXGBE_LXOFFRXC);
		break;
	default:
		data = IXGBE_READ_REG(hw, IXGBE_LXOFFRXCNT);
	}
	hwstats->lxoffrxc += data;

	/* refill credits (no tx hang) if we received xoff */
	if (!data)
		return;

	for (i = 0; i < adapter->num_tx_queues; i++)
		clear_bit(__IXGBE_HANG_CHECK_ARMED,
			  &adapter->tx_ring[i]->state);
}

static void ixgbe_update_xoff_received(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	struct ixgbe_hw_stats *hwstats = &adapter->stats;
	u32 xoff[8] = {0};
	int i;
	bool pfc_en = adapter->dcb_cfg.pfc_mode_enable;

#ifdef HAVE_DCBNL_IEEE
	if (adapter->ixgbe_ieee_pfc)
		pfc_en |= !!(adapter->ixgbe_ieee_pfc->pfc_en);

#endif
	if (!(adapter->flags & IXGBE_FLAG_DCB_ENABLED) || !pfc_en) {
		ixgbe_update_xoff_rx_lfc(adapter);
		return;
	}

	/* update stats for each tc, only valid with PFC enabled */
	for (i = 0; i < MAX_TX_PACKET_BUFFERS; i++) {
		switch (hw->mac.type) {
		case ixgbe_mac_82598EB:
			xoff[i] = IXGBE_READ_REG(hw, IXGBE_PXOFFRXC(i));
			break;
		default:
			xoff[i] = IXGBE_READ_REG(hw, IXGBE_PXOFFRXCNT(i));
		}
		hwstats->pxoffrxc[i] += xoff[i];
	}

	/* disarm tx queues that have received xoff frames */
	for (i = 0; i < adapter->num_tx_queues; i++) {
		struct ixgbe_ring *tx_ring = adapter->tx_ring[i];
		u8 tc = tx_ring->dcb_tc;

		if ((tc <= 7) && (xoff[tc]))
			clear_bit(__IXGBE_HANG_CHECK_ARMED, &tx_ring->state);
	}
}




#define IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT 2




#ifdef HAVE_8021P_SUPPORT
/**
 * ixgbe_vlan_stripping_disable - helper to disable vlan tag stripping
 * @adapter: driver data
 */
void ixgbe_vlan_stripping_disable(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 vlnctrl;
	int i;

	/* leave vlan tag stripping enabled for DCB */
	if (adapter->flags & IXGBE_FLAG_DCB_ENABLED)
		return;

	switch (hw->mac.type) {
	case ixgbe_mac_82598EB:
		vlnctrl = IXGBE_READ_REG(hw, IXGBE_VLNCTRL);
		vlnctrl &= ~IXGBE_VLNCTRL_VME;
		IXGBE_WRITE_REG(hw, IXGBE_VLNCTRL, vlnctrl);
		break;
	case ixgbe_mac_82599EB:
	case ixgbe_mac_X540:
		for (i = 0; i < adapter->num_rx_queues; i++) {
			u8 reg_idx = adapter->rx_ring[i]->reg_idx;
			vlnctrl = IXGBE_READ_REG(hw, IXGBE_RXDCTL(reg_idx));
			vlnctrl &= ~IXGBE_RXDCTL_VME;
			IXGBE_WRITE_REG(hw, IXGBE_RXDCTL(reg_idx), vlnctrl);
		}
		break;
	default:
		break;
	}
}

#endif
/**
 * ixgbe_vlan_stripping_enable - helper to enable vlan tag stripping
 * @adapter: driver data
 */
void ixgbe_vlan_stripping_enable(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 vlnctrl;
	int i;

	switch (hw->mac.type) {
	case ixgbe_mac_82598EB:
		vlnctrl = IXGBE_READ_REG(hw, IXGBE_VLNCTRL);
		vlnctrl |= IXGBE_VLNCTRL_VME;
		IXGBE_WRITE_REG(hw, IXGBE_VLNCTRL, vlnctrl);
		break;
	case ixgbe_mac_82599EB:
	case ixgbe_mac_X540:
		for (i = 0; i < adapter->num_rx_queues; i++) {
			u8 reg_idx = adapter->rx_ring[i]->reg_idx;
			vlnctrl = IXGBE_READ_REG(hw, IXGBE_RXDCTL(reg_idx));
			vlnctrl |= IXGBE_RXDCTL_VME;
			IXGBE_WRITE_REG(hw, IXGBE_RXDCTL(reg_idx), vlnctrl);
		}
		break;
	default:
		break;
	}
}

#ifdef HAVE_VLAN_RX_REGISTER
void ixgbe_vlan_mode(struct net_device *netdev, struct vlan_group *grp)
#else
void ixgbe_vlan_mode(struct net_device *netdev, u32 features)
#endif
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
#ifdef HAVE_8021P_SUPPORT
	bool enable;
#endif
#ifdef HAVE_VLAN_RX_REGISTER

	//if (!test_bit(__IXGBE_DOWN, &adapter->state))
	//	ixgbe_irq_disable(adapter);

	adapter->vlgrp = grp;

	//if (!test_bit(__IXGBE_DOWN, &adapter->state))
	//	ixgbe_irq_enable(adapter, true, true);
#endif
#ifdef HAVE_8021P_SUPPORT
#ifdef HAVE_VLAN_RX_REGISTER
	enable = (grp || (adapter->flags & IXGBE_FLAG_DCB_ENABLED));
#else
	enable = !!(features & NETIF_F_HW_VLAN_RX);
#endif
	if (enable)
		/* enable VLAN tag insert/strip */
		ixgbe_vlan_stripping_enable(adapter);
	else
		/* disable VLAN tag insert/strip */
		ixgbe_vlan_stripping_disable(adapter);

#endif
}

static u8 *ixgbe_addr_list_itr(struct ixgbe_hw *hw, u8 **mc_addr_ptr, u32 *vmdq)
{
#ifdef NETDEV_HW_ADDR_T_MULTICAST
	struct netdev_hw_addr *mc_ptr;
#else
	struct dev_mc_list *mc_ptr;
#endif
	struct ixgbe_adapter *adapter = hw->back;
	u8 *addr = *mc_addr_ptr;

	*vmdq = adapter->num_vfs;

#ifdef NETDEV_HW_ADDR_T_MULTICAST
	mc_ptr = container_of(addr, struct netdev_hw_addr, addr[0]);
	if (mc_ptr->list.next) {
		struct netdev_hw_addr *ha;

		ha = list_entry(mc_ptr->list.next, struct netdev_hw_addr, list);
		*mc_addr_ptr = ha->addr;
	}
#else
	mc_ptr = container_of(addr, struct dev_mc_list, dmi_addr[0]);
	if (mc_ptr->next)
		*mc_addr_ptr = mc_ptr->next->dmi_addr;
#endif
	else
		*mc_addr_ptr = NULL;

	return addr;
}

/**
 * ixgbe_write_mc_addr_list - write multicast addresses to MTA
 * @netdev: network interface device structure
 *
 * Writes multicast address list to the MTA hash table.
 * Returns: -ENOMEM on failure
 *                0 on no addresses written
 *                X on writing X addresses to MTA
 **/
int ixgbe_write_mc_addr_list(struct net_device *netdev)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_hw *hw = &adapter->hw;
#ifdef NETDEV_HW_ADDR_T_MULTICAST
	struct netdev_hw_addr *ha;
#endif
	u8  *addr_list = NULL;
	int addr_count = 0;

	if (!hw->mac.ops.update_mc_addr_list)
		return -ENOMEM;

	if (!netif_running(netdev))
		return 0;


	hw->mac.ops.update_mc_addr_list(hw, NULL, 0,
					ixgbe_addr_list_itr, true);

	if (!netdev_mc_empty(netdev)) {
#ifdef NETDEV_HW_ADDR_T_MULTICAST
		ha = list_first_entry(&netdev->mc.list,
				      struct netdev_hw_addr, list);
		addr_list = ha->addr;
#else
		addr_list = netdev->mc_list->dmi_addr;
#endif
		addr_count = netdev_mc_count(netdev);

		hw->mac.ops.update_mc_addr_list(hw, addr_list, addr_count,
						ixgbe_addr_list_itr, false);
	}

#ifdef CONFIG_PCI_IOV
	//ixgbe_restore_vf_multicasts(adapter);
#endif
	return addr_count;
}


void ixgbe_full_sync_mac_table(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	int i;
	for (i = 0; i < hw->mac.num_rar_entries; i++) {
		if (adapter->mac_table[i].state & IXGBE_MAC_STATE_IN_USE) {
			hw->mac.ops.set_rar(hw, i, adapter->mac_table[i].addr,
						adapter->mac_table[i].queue,
						IXGBE_RAH_AV);
		} else {
			hw->mac.ops.clear_rar(hw, i);
		}
	}
}

void ixgbe_sync_mac_table(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	int i;
	for (i = 0; i < hw->mac.num_rar_entries; i++) {
		if (adapter->mac_table[i].state & IXGBE_MAC_STATE_MODIFIED) {
			if (adapter->mac_table[i].state &
					IXGBE_MAC_STATE_IN_USE) {
				hw->mac.ops.set_rar(hw, i,
						adapter->mac_table[i].addr,
						adapter->mac_table[i].queue,
						IXGBE_RAH_AV);
			} else {
				hw->mac.ops.clear_rar(hw, i);
			}
			adapter->mac_table[i].state &=
				~(IXGBE_MAC_STATE_MODIFIED);
		}
	}
}

int ixgbe_available_rars(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	int i, count = 0;

	for (i = 0; i < hw->mac.num_rar_entries; i++) {
		if (adapter->mac_table[i].state == 0)
			count++;
	}
	return count;
}

int ixgbe_add_mac_filter(struct ixgbe_adapter *adapter, u8 *addr, u16 queue)
{
	struct ixgbe_hw *hw = &adapter->hw;
	int i;

	if (is_zero_ether_addr(addr))
		return 0;

	for (i = 0; i < hw->mac.num_rar_entries; i++) {
		if (adapter->mac_table[i].state & IXGBE_MAC_STATE_IN_USE)
			continue;
		adapter->mac_table[i].state |= (IXGBE_MAC_STATE_MODIFIED |
						IXGBE_MAC_STATE_IN_USE);
		memcpy(adapter->mac_table[i].addr, addr, ETH_ALEN);
		adapter->mac_table[i].queue = queue;
		ixgbe_sync_mac_table(adapter);
		return i;
	}
	return -ENOMEM;
}

void ixgbe_flush_sw_mac_table(struct ixgbe_adapter *adapter)
{
	int i;
	struct ixgbe_hw *hw = &adapter->hw;

	for (i = 0; i < hw->mac.num_rar_entries; i++) {
		adapter->mac_table[i].state |= IXGBE_MAC_STATE_MODIFIED;
		adapter->mac_table[i].state &= ~IXGBE_MAC_STATE_IN_USE;
		memset(adapter->mac_table[i].addr, 0, ETH_ALEN);
		adapter->mac_table[i].queue = 0;
	}
	ixgbe_sync_mac_table(adapter);
}

void ixgbe_del_mac_filter_by_index(struct ixgbe_adapter *adapter, int index)
{
	adapter->mac_table[index].state |= IXGBE_MAC_STATE_MODIFIED;
	adapter->mac_table[index].state &= ~IXGBE_MAC_STATE_IN_USE;
	memset(adapter->mac_table[index].addr, 0, ETH_ALEN);
	adapter->mac_table[index].queue = 0;
	ixgbe_sync_mac_table(adapter);
}

int ixgbe_del_mac_filter(struct ixgbe_adapter *adapter, u8* addr, u16 queue)
{
	/* search table for addr, if found, set to 0 and sync */
	int i;
	struct ixgbe_hw *hw = &adapter->hw;

	if (is_zero_ether_addr(addr))
		return 0;
	for (i = 0; i < hw->mac.num_rar_entries; i++) {
		if (ether_addr_equal(addr, adapter->mac_table[i].addr) &&
		    adapter->mac_table[i].queue == queue) {
			adapter->mac_table[i].state |= IXGBE_MAC_STATE_MODIFIED;
			adapter->mac_table[i].state &= ~IXGBE_MAC_STATE_IN_USE;
			memset(adapter->mac_table[i].addr, 0, ETH_ALEN);
			adapter->mac_table[i].queue = 0;
			ixgbe_sync_mac_table(adapter);
			return 0;
		}
	}
	return -ENOMEM;
}
#ifdef HAVE_SET_RX_MODE
/**
 * ixgbe_write_uc_addr_list - write unicast addresses to RAR table
 * @netdev: network interface device structure
 *
 * Writes unicast address list to the RAR table.
 * Returns: -ENOMEM on failure/insufficient address space
 *                0 on no addresses written
 *                X on writing X addresses to the RAR table
 **/
int ixgbe_write_uc_addr_list(struct ixgbe_adapter *adapter,
			     struct net_device *netdev, unsigned int vfn)
{
	int count = 0;

	/* return ENOMEM indicating insufficient memory for addresses */
	if (netdev_uc_count(netdev) > ixgbe_available_rars(adapter))
		return -ENOMEM;

	if (!netdev_uc_empty(netdev)) {
#ifdef NETDEV_HW_ADDR_T_UNICAST
		struct netdev_hw_addr *ha;
#else
		struct dev_mc_list *ha;
#endif
		netdev_for_each_uc_addr(ha, netdev) {
#ifdef NETDEV_HW_ADDR_T_UNICAST
			ixgbe_del_mac_filter(adapter, ha->addr, (u16)vfn);
			ixgbe_add_mac_filter(adapter, ha->addr, (u16)vfn);
#else
			ixgbe_del_mac_filter(adapter, ha->da_addr, (u16)vfn);
			ixgbe_add_mac_filter(adapter, ha->da_addr, (u16)vfn);
#endif
			count++;
		}
	}
	return count;
}

#endif
/**
 * ixgbe_set_rx_mode - Unicast, Multicast and Promiscuous mode set
 * @netdev: network interface device structure
 *
 * The set_rx_method entry point is called whenever the unicast/multicast
 * address list or the network interface flags are updated.  This routine is
 * responsible for configuring the hardware for proper unicast, multicast and
 * promiscuous mode.
 **/
void ixgbe_set_rx_mode(struct net_device *netdev)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_hw *hw = &adapter->hw;
	u32 fctrl, vmolr = IXGBE_VMOLR_BAM | IXGBE_VMOLR_AUPE;
	u32 vlnctrl;
	int count;

	/* Check for Promiscuous and All Multicast modes */
	fctrl = IXGBE_READ_REG(hw, IXGBE_FCTRL);
	vlnctrl = IXGBE_READ_REG(hw, IXGBE_VLNCTRL);

	/* set all bits that we expect to always be set */
	fctrl |= IXGBE_FCTRL_BAM;
	fctrl |= IXGBE_FCTRL_DPF; /* discard pause frames when FC enabled */
	fctrl |= IXGBE_FCTRL_PMCF;

	/* clear the bits we are changing the status of */
	fctrl &= ~(IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
	vlnctrl  &= ~(IXGBE_VLNCTRL_VFE | IXGBE_VLNCTRL_CFIEN);

	if (netdev->flags & IFF_PROMISC) {
		hw->addr_ctrl.user_set_promisc = true;
		fctrl |= (IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
		vmolr |= IXGBE_VMOLR_MPE;
	} else {
		if (netdev->flags & IFF_ALLMULTI) {
			fctrl |= IXGBE_FCTRL_MPE;
			vmolr |= IXGBE_VMOLR_MPE;
		} else {
			/*
			 * Write addresses to the MTA, if the attempt fails
			 * then we should just turn on promiscuous mode so
			 * that we can at least receive multicast traffic
			 */
			count = ixgbe_write_mc_addr_list(netdev);
			if (count < 0) {
				fctrl |= IXGBE_FCTRL_MPE;
				vmolr |= IXGBE_VMOLR_MPE;
			} else if (count) {
				vmolr |= IXGBE_VMOLR_ROMPE;
			}
		}
#ifdef NETIF_F_HW_VLAN_TX
		/* enable hardware vlan filtering */
		vlnctrl |= IXGBE_VLNCTRL_VFE;
#endif
		hw->addr_ctrl.user_set_promisc = false;
#ifdef HAVE_SET_RX_MODE
		/*
		 * Write addresses to available RAR registers, if there is not
		 * sufficient space to store all the addresses then enable
		 * unicast promiscuous mode
		 */
		count = ixgbe_write_uc_addr_list(adapter, netdev,
						 adapter->num_vfs);
		if (count < 0) {
			fctrl |= IXGBE_FCTRL_UPE;
			vmolr |= IXGBE_VMOLR_ROPE;
		}
#endif
	}

	if (hw->mac.type != ixgbe_mac_82598EB) {
		vmolr |= IXGBE_READ_REG(hw, IXGBE_VMOLR(adapter->num_vfs)) &
			 ~(IXGBE_VMOLR_MPE | IXGBE_VMOLR_ROMPE |
			   IXGBE_VMOLR_ROPE);
		IXGBE_WRITE_REG(hw, IXGBE_VMOLR(adapter->num_vfs), vmolr);
	}

	IXGBE_WRITE_REG(hw, IXGBE_VLNCTRL, vlnctrl);
	IXGBE_WRITE_REG(hw, IXGBE_FCTRL, fctrl);
}








/* Additional bittime to account for IXGBE framing */
#define IXGBE_ETH_FRAMING 20

/*
 * ixgbe_hpbthresh - calculate high water mark for flow control
 *
 * @adapter: board private structure to calculate for
 * @pb - packet buffer to calculate
 */
static int ixgbe_hpbthresh(struct ixgbe_adapter *adapter, int pb)
{
	struct ixgbe_hw *hw = &adapter->hw;
	struct net_device *dev = adapter->netdev;
	int link, tc, kb, marker;
	u32 dv_id, rx_pba;

	/* Calculate max LAN frame size */
	tc = link = dev->mtu + ETH_HLEN + ETH_FCS_LEN + IXGBE_ETH_FRAMING;

#ifdef IXGBE_FCOE
	/* FCoE traffic class uses FCOE jumbo frames */
	if (dev->features & NETIF_F_FCOE_MTU) {
		int fcoe_pb = 0;

		fcoe_pb = netdev_get_prio_tc_map(dev, adapter->fcoe.up);

		if (fcoe_pb == pb && tc < IXGBE_FCOE_JUMBO_FRAME_SIZE)
			tc = IXGBE_FCOE_JUMBO_FRAME_SIZE;
	}
#endif

	/* Calculate delay value for device */
	switch (hw->mac.type) {
	case ixgbe_mac_X540:
		dv_id = IXGBE_DV_X540(link, tc);
		break;
	default:
		dv_id = IXGBE_DV(link, tc);
		break;
	}

	/* Loopback switch introduces additional latency */
	if (adapter->flags & IXGBE_FLAG_SRIOV_ENABLED)
		dv_id += IXGBE_B2BT(tc);

	/* Delay value is calculated in bit times convert to KB */
	kb = IXGBE_BT2KB(dv_id);
	rx_pba = IXGBE_READ_REG(hw, IXGBE_RXPBSIZE(pb)) >> 10;

	marker = rx_pba - kb;

	/* It is possible that the packet buffer is not large enough
	 * to provide required headroom. In this case throw an error
	 * to user and a do the best we can.
	 */
	if (marker < 0) {
		e_warn(drv, "Packet Buffer(%i) can not provide enough"
			    "headroom to suppport flow control."
			    "Decrease MTU or number of traffic classes\n", pb);
		marker = tc + 1;
	}

	return marker;
}

/*
 * ixgbe_lpbthresh - calculate low water mark for for flow control
 *
 * @adapter: board private structure to calculate for
 * @pb - packet buffer to calculate
 */
static int ixgbe_lpbthresh(struct ixgbe_adapter *adapter, int pb)
{
	struct ixgbe_hw *hw = &adapter->hw;
	struct net_device *dev = adapter->netdev;
	int tc;
	u32 dv_id;

	/* Calculate max LAN frame size */
	tc = dev->mtu + ETH_HLEN + ETH_FCS_LEN;

#ifdef IXGBE_FCOE
	/* FCoE traffic class uses FCOE jumbo frames */
	if (dev->features & NETIF_F_FCOE_MTU) {
		int fcoe_pb = 0;

		fcoe_pb = netdev_get_prio_tc_map(dev, adapter->fcoe.up);

		if (fcoe_pb == pb && tc < IXGBE_FCOE_JUMBO_FRAME_SIZE)
			tc = IXGBE_FCOE_JUMBO_FRAME_SIZE;
	}
#endif

	/* Calculate delay value for device */
	switch (hw->mac.type) {
	case ixgbe_mac_X540:
		dv_id = IXGBE_LOW_DV_X540(tc);
		break;
	default:
		dv_id = IXGBE_LOW_DV(tc);
		break;
	}

	/* Delay value is calculated in bit times convert to KB */
	return IXGBE_BT2KB(dv_id);
}

/*
 * ixgbe_pbthresh_setup - calculate and setup high low water marks
 */
static void ixgbe_pbthresh_setup(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	int num_tc = netdev_get_num_tc(adapter->netdev);
	int i;

	if (!num_tc)
		num_tc = 1;
	if (num_tc > IXGBE_DCB_MAX_TRAFFIC_CLASS)
		num_tc = IXGBE_DCB_MAX_TRAFFIC_CLASS;

	for (i = 0; i < num_tc; i++) {
		hw->fc.high_water[i] = ixgbe_hpbthresh(adapter, i);
		hw->fc.low_water[i] = ixgbe_lpbthresh(adapter, i);

		/* Low water marks must not be larger than high water marks */
		if (hw->fc.low_water[i] > hw->fc.high_water[i])
			hw->fc.low_water[i] = 0;
	}

	for (; i < IXGBE_DCB_MAX_TRAFFIC_CLASS; i++)
		hw->fc.high_water[i] = 0;
}



#ifdef NO_VNIC
static void ixgbe_configure(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;

	ixgbe_configure_pb(adapter);
	ixgbe_configure_dcb(adapter);

	ixgbe_set_rx_mode(adapter->netdev);
#ifdef NETIF_F_HW_VLAN_TX
	ixgbe_restore_vlan(adapter);
#endif

#ifdef IXGBE_FCOE
	if (adapter->flags & IXGBE_FLAG_FCOE_ENABLED)
		ixgbe_configure_fcoe(adapter);

#endif /* IXGBE_FCOE */

	if (adapter->hw.mac.type != ixgbe_mac_82598EB)
		hw->mac.ops.disable_sec_rx_path(hw);

	if (adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE) {
		ixgbe_init_fdir_signature_82599(&adapter->hw,
						adapter->fdir_pballoc);
	} else if (adapter->flags & IXGBE_FLAG_FDIR_PERFECT_CAPABLE) {
		ixgbe_init_fdir_perfect_82599(&adapter->hw,
					      adapter->fdir_pballoc);
		ixgbe_fdir_filter_restore(adapter);
	}

	if (adapter->hw.mac.type != ixgbe_mac_82598EB)
		hw->mac.ops.enable_sec_rx_path(hw);

	ixgbe_configure_virtualization(adapter);

	ixgbe_configure_tx(adapter);
	ixgbe_configure_rx(adapter);
}
#endif

static bool ixgbe_is_sfp(struct ixgbe_hw *hw)
{
	switch (hw->phy.type) {
	case ixgbe_phy_sfp_avago:
	case ixgbe_phy_sfp_ftl:
	case ixgbe_phy_sfp_intel:
	case ixgbe_phy_sfp_unknown:
	case ixgbe_phy_sfp_passive_tyco:
	case ixgbe_phy_sfp_passive_unknown:
	case ixgbe_phy_sfp_active_unknown:
	case ixgbe_phy_sfp_ftl_active:
		return true;
	case ixgbe_phy_nl:
		if (hw->mac.type == ixgbe_mac_82598EB)
			return true;
	default:
		return false;
	}
}


/**
 * ixgbe_clear_vf_stats_counters - Clear out VF stats after reset
 * @adapter: board private structure
 *
 * On a reset we need to clear out the VF stats or accounting gets
 * messed up because they're not clear on read.
 **/
void ixgbe_clear_vf_stats_counters(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	int i;

	for (i = 0; i < adapter->num_vfs; i++) {
		adapter->vfinfo[i].last_vfstats.gprc =
			IXGBE_READ_REG(hw, IXGBE_PVFGPRC(i));
		adapter->vfinfo[i].saved_rst_vfstats.gprc +=
			adapter->vfinfo[i].vfstats.gprc;
		adapter->vfinfo[i].vfstats.gprc = 0;
		adapter->vfinfo[i].last_vfstats.gptc =
			IXGBE_READ_REG(hw, IXGBE_PVFGPTC(i));
		adapter->vfinfo[i].saved_rst_vfstats.gptc +=
			adapter->vfinfo[i].vfstats.gptc;
		adapter->vfinfo[i].vfstats.gptc = 0;
		adapter->vfinfo[i].last_vfstats.gorc =
			IXGBE_READ_REG(hw, IXGBE_PVFGORC_LSB(i));
		adapter->vfinfo[i].saved_rst_vfstats.gorc +=
			adapter->vfinfo[i].vfstats.gorc;
		adapter->vfinfo[i].vfstats.gorc = 0;
		adapter->vfinfo[i].last_vfstats.gotc =
			IXGBE_READ_REG(hw, IXGBE_PVFGOTC_LSB(i));
		adapter->vfinfo[i].saved_rst_vfstats.gotc +=
			adapter->vfinfo[i].vfstats.gotc;
		adapter->vfinfo[i].vfstats.gotc = 0;
		adapter->vfinfo[i].last_vfstats.mprc =
			IXGBE_READ_REG(hw, IXGBE_PVFMPRC(i));
		adapter->vfinfo[i].saved_rst_vfstats.mprc +=
			adapter->vfinfo[i].vfstats.mprc;
		adapter->vfinfo[i].vfstats.mprc = 0;
	}
}



void ixgbe_reinit_locked(struct ixgbe_adapter *adapter)
{
#ifdef NO_VNIC
	WARN_ON(in_interrupt());
	/* put off any impending NetWatchDogTimeout */
	adapter->netdev->trans_start = jiffies;

	while (test_and_set_bit(__IXGBE_RESETTING, &adapter->state))
		usleep_range(1000, 2000);
	ixgbe_down(adapter);
	/*
	 * If SR-IOV enabled then wait a bit before bringing the adapter
	 * back up to give the VFs time to respond to the reset.  The
	 * two second wait is based upon the watchdog timer cycle in
	 * the VF driver.
	 */
	if (adapter->flags & IXGBE_FLAG_SRIOV_ENABLED)
		msleep(2000);
	ixgbe_up(adapter);
	clear_bit(__IXGBE_RESETTING, &adapter->state);
#endif
}

void ixgbe_up(struct ixgbe_adapter *adapter)
{
	/* hardware has been reset, we need to reload some things */
	//ixgbe_configure(adapter);

	//ixgbe_up_complete(adapter);
}

void ixgbe_reset(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	struct net_device *netdev = adapter->netdev;
	int err;

	/* lock SFP init bit to prevent race conditions with the watchdog */
	while (test_and_set_bit(__IXGBE_IN_SFP_INIT, &adapter->state))
		usleep_range(1000, 2000);

	/* clear all SFP and link config related flags while holding SFP_INIT */
	adapter->flags2 &= ~(IXGBE_FLAG2_SEARCH_FOR_SFP |
			     IXGBE_FLAG2_SFP_NEEDS_RESET);
	adapter->flags &= ~IXGBE_FLAG_NEED_LINK_CONFIG;

	err = hw->mac.ops.init_hw(hw);
	switch (err) {
	case 0:
	case IXGBE_ERR_SFP_NOT_PRESENT:
	case IXGBE_ERR_SFP_NOT_SUPPORTED:
		break;
	case IXGBE_ERR_MASTER_REQUESTS_PENDING:
		e_dev_err("master disable timed out\n");
		break;
	case IXGBE_ERR_EEPROM_VERSION:
		/* We are running on a pre-production device, log a warning */
		e_dev_warn("This device is a pre-production adapter/LOM. "
			   "Please be aware there may be issues associated "
			   "with your hardware.  If you are experiencing "
			   "problems please contact your Intel or hardware "
			   "representative who provided you with this "
			   "hardware.\n");
		break;
	default:
		e_dev_err("Hardware Error: %d\n", err);
	}

	clear_bit(__IXGBE_IN_SFP_INIT, &adapter->state);

	ixgbe_flush_sw_mac_table(adapter);
	memcpy(&adapter->mac_table[0].addr, hw->mac.perm_addr,
	       netdev->addr_len);
	adapter->mac_table[0].queue = adapter->num_vfs;
	adapter->mac_table[0].state = (IXGBE_MAC_STATE_DEFAULT |
					IXGBE_MAC_STATE_IN_USE);
	hw->mac.ops.set_rar(hw, 0, adapter->mac_table[0].addr,
				adapter->mac_table[0].queue,
				IXGBE_RAH_AV);
}






void ixgbe_down(struct ixgbe_adapter *adapter)
{
#ifdef NO_VNIC
	struct net_device *netdev = adapter->netdev;
	struct ixgbe_hw *hw = &adapter->hw;
	u32 rxctrl;
	int i;

	/* signal that we are down to the interrupt handler */
	set_bit(__IXGBE_DOWN, &adapter->state);

	/* disable receives */
	rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
	IXGBE_WRITE_REG(hw, IXGBE_RXCTRL, rxctrl & ~IXGBE_RXCTRL_RXEN);

	/* disable all enabled rx queues */
	for (i = 0; i < adapter->num_rx_queues; i++)
		/* this call also flushes the previous write */
		ixgbe_disable_rx_queue(adapter, adapter->rx_ring[i]);

	usleep_range(10000, 20000);

	netif_tx_stop_all_queues(netdev);

	/* call carrier off first to avoid false dev_watchdog timeouts */
	netif_carrier_off(netdev);
	netif_tx_disable(netdev);

	ixgbe_irq_disable(adapter);

	ixgbe_napi_disable_all(adapter);

	adapter->flags2 &= ~(IXGBE_FLAG2_FDIR_REQUIRES_REINIT |
			     IXGBE_FLAG2_RESET_REQUESTED);
	adapter->flags &= ~IXGBE_FLAG_NEED_LINK_UPDATE;

	del_timer_sync(&adapter->service_timer);

	if (adapter->num_vfs) {
		/* Clear EITR Select mapping */
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EITRSEL, 0);

		/* Mark all the VFs as inactive */
		for (i = 0 ; i < adapter->num_vfs; i++)
			adapter->vfinfo[i].clear_to_send = 0;

		/* ping all the active vfs to let them know we are going down */
		ixgbe_ping_all_vfs(adapter);

		/* Disable all VFTE/VFRE TX/RX */
		ixgbe_disable_tx_rx(adapter);
	}

	/* disable transmits in the hardware now that interrupts are off */
	for (i = 0; i < adapter->num_tx_queues; i++) {
		u8 reg_idx = adapter->tx_ring[i]->reg_idx;
		IXGBE_WRITE_REG(hw, IXGBE_TXDCTL(reg_idx), IXGBE_TXDCTL_SWFLSH);
	}

	/* Disable the Tx DMA engine on 82599 and X540 */
	switch (hw->mac.type) {
	case ixgbe_mac_82599EB:
	case ixgbe_mac_X540:
		IXGBE_WRITE_REG(hw, IXGBE_DMATXCTL,
				(IXGBE_READ_REG(hw, IXGBE_DMATXCTL) &
				 ~IXGBE_DMATXCTL_TE));
		break;
	default:
		break;
	}

#ifdef HAVE_PCI_ERS
	if (!pci_channel_offline(adapter->pdev))
#endif
		ixgbe_reset(adapter);
	/* power down the optics */
	if ((hw->phy.multispeed_fiber) ||
	    ((hw->mac.ops.get_media_type(hw) == ixgbe_media_type_fiber) &&
	     (hw->mac.type == ixgbe_mac_82599EB)))
		ixgbe_disable_tx_laser(hw);

	ixgbe_clean_all_tx_rings(adapter);
	ixgbe_clean_all_rx_rings(adapter);

#if defined(CONFIG_DCA) || defined(CONFIG_DCA_MODULE)
	/* since we reset the hardware DCA settings were cleared */
	ixgbe_setup_dca(adapter);
#endif

#endif /* NO_VNIC */
}

#ifndef NO_VNIC

#undef IXGBE_FCOE

/* Artificial max queue cap per traffic class in DCB mode */
#define DCB_QUEUE_CAP 8

/**
 * ixgbe_set_dcb_queues: Allocate queues for a DCB-enabled device
 * @adapter: board private structure to initialize
 *
 * When DCB (Data Center Bridging) is enabled, allocate queues for
 * each traffic class.  If multiqueue isn't available,then abort DCB
 * initialization.
 *
 * This function handles all combinations of DCB, RSS, and FCoE.
 *
 **/
static bool ixgbe_set_dcb_queues(struct ixgbe_adapter *adapter)
{
       int tcs;
#ifdef HAVE_MQPRIO
       int rss_i, i, offset = 0;
       struct net_device *dev = adapter->netdev;

       /* Map queue offset and counts onto allocated tx queues */
       tcs = netdev_get_num_tc(dev);

       if (!tcs)
              return false;

       rss_i = min_t(int, dev->num_tx_queues / tcs, num_online_cpus());

       if (rss_i > DCB_QUEUE_CAP)
              rss_i = DCB_QUEUE_CAP;

       for (i = 0; i < tcs; i++) {
              netdev_set_tc_queue(dev, i, rss_i, offset);
              offset += rss_i;
       }

       adapter->num_tx_queues = rss_i * tcs;
       adapter->num_rx_queues = rss_i * tcs;

#ifdef IXGBE_FCOE
       /* FCoE enabled queues require special configuration indexed
        * by feature specific indices and mask. Here we map FCoE
        * indices onto the DCB queue pairs allowing FCoE to own
        * configuration later.
        */

       if (adapter->flags & IXGBE_FLAG_FCOE_ENABLED) {
              struct ixgbe_ring_feature *f;
              int tc;
              u8 prio_tc[IXGBE_DCB_MAX_USER_PRIORITY] = {0};

              ixgbe_dcb_unpack_map_cee(&adapter->dcb_cfg,
                                    IXGBE_DCB_TX_CONFIG,
                                    prio_tc);
              tc = prio_tc[adapter->fcoe.up];

              f = &adapter->ring_feature[RING_F_FCOE];
              f->indices = min_t(int, rss_i, f->indices);
              f->mask = rss_i * tc;
       }
#endif /* IXGBE_FCOE */
#else
       if (!(adapter->flags & IXGBE_FLAG_DCB_ENABLED))
              return false;

       /* Enable one Queue per traffic class */
       tcs = adapter->tc;
       if (!tcs)
              return false;

#ifdef IXGBE_FCOE
       if (adapter->flags & IXGBE_FLAG_FCOE_ENABLED) {
              struct ixgbe_ring_feature *f;
              int tc = netdev_get_prio_tc_map(adapter->netdev,
                                          adapter->fcoe.up);

              f = &adapter->ring_feature[RING_F_FCOE];

              /*
               * We have max 8 queues for FCoE, where 8 the is
               * FCoE redirection table size.  We must also share
               * ring resources with network traffic so if FCoE TC is
               * 4 or greater and we are in 8 TC mode we can only use
               * 7 queues.
               */
              if ((tcs > 4) && (tc >= 4) && (f->indices > 7))
                     f->indices = 7;

              f->indices = min_t(int, num_online_cpus(), f->indices);
              f->mask = tcs;

              adapter->num_rx_queues = f->indices + tcs;
              adapter->num_tx_queues = f->indices + tcs;

              return true;
       }

#endif /* IXGBE_FCOE */
       adapter->num_rx_queues = tcs;
       adapter->num_tx_queues = tcs;
#endif /* HAVE_MQ */

       return true;
}

/**
 * ixgbe_set_vmdq_queues: Allocate queues for VMDq devices
 * @adapter: board private structure to initialize
 *
 * When VMDq (Virtual Machine Devices queue) is enabled, allocate queues
 * and VM pools where appropriate.  If RSS is available, then also try and
 * enable RSS and map accordingly.
 *
 **/
static bool ixgbe_set_vmdq_queues(struct ixgbe_adapter *adapter)
{
       int vmdq_i = adapter->ring_feature[RING_F_VMDQ].indices;
       int vmdq_m = 0;
       int rss_i = adapter->ring_feature[RING_F_RSS].indices;
       unsigned long i;
       int rss_shift;
       bool ret = false;


       switch (adapter->flags & (IXGBE_FLAG_RSS_ENABLED
                               | IXGBE_FLAG_DCB_ENABLED
                               | IXGBE_FLAG_VMDQ_ENABLED)) {

       case (IXGBE_FLAG_RSS_ENABLED | IXGBE_FLAG_VMDQ_ENABLED):
              switch (adapter->hw.mac.type) {
              case ixgbe_mac_82599EB:
              case ixgbe_mac_X540:
                     vmdq_i = min((int)IXGBE_MAX_VMDQ_INDICES, vmdq_i);
                     if (vmdq_i > 32)
                            rss_i = 2;
                     else
                            rss_i = 4;
                     i = rss_i;
                     rss_shift = find_first_bit(&i, sizeof(i) * 8);
                     vmdq_m = ((IXGBE_MAX_VMDQ_INDICES - 1) <<
                               rss_shift) & (MAX_RX_QUEUES - 1);
                     break;
              default:
                     break;
              }
              adapter->num_rx_queues = vmdq_i * rss_i;
              adapter->num_tx_queues = min((int)MAX_TX_QUEUES, vmdq_i * rss_i);
              ret = true;
              break;

       case (IXGBE_FLAG_VMDQ_ENABLED):
              switch (adapter->hw.mac.type) {
              case ixgbe_mac_82598EB:
                     vmdq_m = (IXGBE_MAX_VMDQ_INDICES - 1);
                     break;
              case ixgbe_mac_82599EB:
              case ixgbe_mac_X540:
                     vmdq_m = (IXGBE_MAX_VMDQ_INDICES - 1) << 1;
                     break;
              default:
                     break;
              }
              adapter->num_rx_queues = vmdq_i;
              adapter->num_tx_queues = vmdq_i;
              ret = true;
              break;

       default:
              ret = false;
              goto vmdq_queues_out;
       }

       if (adapter->flags & IXGBE_FLAG_VMDQ_ENABLED) {
              adapter->num_rx_pools = vmdq_i;
              adapter->num_rx_queues_per_pool = adapter->num_rx_queues /
                                            vmdq_i;
       } else {
              adapter->num_rx_pools = adapter->num_rx_queues;
              adapter->num_rx_queues_per_pool = 1;
       }
       /* save the mask for later use */
       adapter->ring_feature[RING_F_VMDQ].mask = vmdq_m;
vmdq_queues_out:
       return ret;
}

/**
 * ixgbe_set_rss_queues: Allocate queues for RSS
 * @adapter: board private structure to initialize
 *
 * This is our "base" multiqueue mode.  RSS (Receive Side Scaling) will try
 * to allocate one Rx queue per CPU, and if available, one Tx queue per CPU.
 *
 **/
static bool ixgbe_set_rss_queues(struct ixgbe_adapter *adapter)
{
       struct ixgbe_ring_feature *f;

       if (!(adapter->flags & IXGBE_FLAG_RSS_ENABLED)) {
              adapter->flags &= ~IXGBE_FLAG_FDIR_HASH_CAPABLE;
              return false;
       }

       /* set mask for 16 queue limit of RSS */
       f = &adapter->ring_feature[RING_F_RSS];
       f->mask = 0xF;

       /*
        * Use Flow Director in addition to RSS to ensure the best
        * distribution of flows across cores, even when an FDIR flow
        * isn't matched.
        */
       if (adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE) {
              f = &adapter->ring_feature[RING_F_FDIR];

              f->indices = min_t(int, num_online_cpus(), f->indices);
              f->mask = 0;
       }

       adapter->num_rx_queues = f->indices;
#ifdef HAVE_TX_MQ
       adapter->num_tx_queues = f->indices;
#endif

       return true;
}

#ifdef IXGBE_FCOE
/**
 * ixgbe_set_fcoe_queues: Allocate queues for Fiber Channel over Ethernet (FCoE)
 * @adapter: board private structure to initialize
 *
 * FCoE RX FCRETA can use up to 8 rx queues for up to 8 different exchanges.
 * The ring feature mask is not used as a mask for FCoE, as it can take any 8
 * rx queues out of the max number of rx queues, instead, it is used as the
 * index of the first rx queue used by FCoE.
 *
 **/
static bool ixgbe_set_fcoe_queues(struct ixgbe_adapter *adapter)
{
       struct ixgbe_ring_feature *f;

       if (!(adapter->flags & IXGBE_FLAG_FCOE_ENABLED))
              return false;

       ixgbe_set_rss_queues(adapter);

       f = &adapter->ring_feature[RING_F_FCOE];
       f->indices = min_t(int, num_online_cpus(), f->indices);

       /* adding FCoE queues */
       f->mask = adapter->num_rx_queues;
       adapter->num_rx_queues += f->indices;
       adapter->num_tx_queues += f->indices;

       return true;
}

#endif /* IXGBE_FCOE */
/*
 * ixgbe_set_num_queues: Allocate queues for device, feature dependent
 * @adapter: board private structure to initialize
 *
 * This is the top level queue allocation routine.  The order here is very
 * important, starting with the "most" number of features turned on at once,
 * and ending with the smallest set of features.  This way large combinations
 * can be allocated if they're turned on, and smaller combinations are the
 * fallthrough conditions.
 *
 **/
static void ixgbe_set_num_queues(struct ixgbe_adapter *adapter)
{
       /* Start with base case */
       adapter->num_rx_queues = 1;
       adapter->num_tx_queues = 1;
       adapter->num_rx_pools = adapter->num_rx_queues;
       adapter->num_rx_queues_per_pool = 1;

       if (ixgbe_set_vmdq_queues(adapter))
              return;

       if (ixgbe_set_dcb_queues(adapter))
              return;

#ifdef IXGBE_FCOE
       if (ixgbe_set_fcoe_queues(adapter))
              return;

#endif /* IXGBE_FCOE */
       ixgbe_set_rss_queues(adapter);
}

#endif


/**
 * ixgbe_sw_init - Initialize general software structures (struct ixgbe_adapter)
 * @adapter: board private structure to initialize
 *
 * ixgbe_sw_init initializes the Adapter private data structure.
 * Fields are initialized based on PCI device information and
 * OS network device settings (MTU size).
 **/
static int ixgbe_sw_init(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	struct pci_dev *pdev = adapter->pdev;
	int err;

	/* PCI config space info */

	hw->vendor_id = pdev->vendor;
	hw->device_id = pdev->device;
	pci_read_config_byte(pdev, PCI_REVISION_ID, &hw->revision_id);
	hw->subsystem_vendor_id = pdev->subsystem_vendor;
	hw->subsystem_device_id = pdev->subsystem_device;

	err = ixgbe_init_shared_code(hw);
	if (err) {
		e_err(probe, "init_shared_code failed: %d\n", err);
		goto out;
	}
	adapter->mac_table = kzalloc(sizeof(struct ixgbe_mac_addr) *
				     hw->mac.num_rar_entries,
				     GFP_ATOMIC);
	/* Set capability flags */
	switch (hw->mac.type) {
	case ixgbe_mac_82598EB:
		adapter->flags |= IXGBE_FLAG_MSI_CAPABLE |
				  IXGBE_FLAG_MSIX_CAPABLE |
				  IXGBE_FLAG_MQ_CAPABLE |
				  IXGBE_FLAG_RSS_CAPABLE;
		adapter->flags |= IXGBE_FLAG_DCB_CAPABLE;
#if defined(CONFIG_DCA) || defined(CONFIG_DCA_MODULE)
		adapter->flags |= IXGBE_FLAG_DCA_CAPABLE;
#endif
		adapter->flags &= ~IXGBE_FLAG_SRIOV_CAPABLE;
		adapter->flags2 &= ~IXGBE_FLAG2_RSC_CAPABLE;

		if (hw->device_id == IXGBE_DEV_ID_82598AT)
			adapter->flags |= IXGBE_FLAG_FAN_FAIL_CAPABLE;

		adapter->max_msix_q_vectors = IXGBE_MAX_MSIX_Q_VECTORS_82598;
		break;
	case ixgbe_mac_X540:
		adapter->flags2 |= IXGBE_FLAG2_TEMP_SENSOR_CAPABLE;
	case ixgbe_mac_82599EB:
		adapter->flags |= IXGBE_FLAG_MSI_CAPABLE |
				  IXGBE_FLAG_MSIX_CAPABLE |
				  IXGBE_FLAG_MQ_CAPABLE |
				  IXGBE_FLAG_RSS_CAPABLE;
		adapter->flags |= IXGBE_FLAG_DCB_CAPABLE;
#if defined(CONFIG_DCA) || defined(CONFIG_DCA_MODULE)
		adapter->flags |= IXGBE_FLAG_DCA_CAPABLE;
#endif
		adapter->flags |= IXGBE_FLAG_SRIOV_CAPABLE;
		adapter->flags2 |= IXGBE_FLAG2_RSC_CAPABLE;
#ifdef IXGBE_FCOE
		adapter->flags |= IXGBE_FLAG_FCOE_CAPABLE;
		adapter->flags &= ~IXGBE_FLAG_FCOE_ENABLED;
		adapter->ring_feature[RING_F_FCOE].indices = 0;
#ifdef CONFIG_DCB
		/* Default traffic class to use for FCoE */
		adapter->fcoe.tc = IXGBE_FCOE_DEFTC;
		adapter->fcoe.up = IXGBE_FCOE_DEFTC;
		adapter->fcoe.up_set = IXGBE_FCOE_DEFTC;
#endif
#endif
		if (hw->device_id == IXGBE_DEV_ID_82599_T3_LOM)
			adapter->flags2 |= IXGBE_FLAG2_TEMP_SENSOR_CAPABLE;
#ifndef IXGBE_NO_SMART_SPEED
		hw->phy.smart_speed = ixgbe_smart_speed_on;
#else
		hw->phy.smart_speed = ixgbe_smart_speed_off;
#endif
		adapter->max_msix_q_vectors = IXGBE_MAX_MSIX_Q_VECTORS_82599;
	default:
		break;
	}

	/* n-tuple support exists, always init our spinlock */
	//spin_lock_init(&adapter->fdir_perfect_lock);

	if (adapter->flags & IXGBE_FLAG_DCB_CAPABLE) {
		int j;
		struct ixgbe_dcb_tc_config *tc;
		int dcb_i = IXGBE_DCB_MAX_TRAFFIC_CLASS;


		adapter->dcb_cfg.num_tcs.pg_tcs = dcb_i;
		adapter->dcb_cfg.num_tcs.pfc_tcs = dcb_i;
		for (j = 0; j < dcb_i; j++) {
			tc = &adapter->dcb_cfg.tc_config[j];
			tc->path[IXGBE_DCB_TX_CONFIG].bwg_id = 0;
			tc->path[IXGBE_DCB_TX_CONFIG].bwg_percent = 100 / dcb_i;
			tc->path[IXGBE_DCB_RX_CONFIG].bwg_id = 0;
			tc->path[IXGBE_DCB_RX_CONFIG].bwg_percent = 100 / dcb_i;
			tc->pfc = ixgbe_dcb_pfc_disabled;
			if (j == 0) {
				/* total of all TCs bandwidth needs to be 100 */
				tc->path[IXGBE_DCB_TX_CONFIG].bwg_percent +=
								 100 % dcb_i;
				tc->path[IXGBE_DCB_RX_CONFIG].bwg_percent +=
								 100 % dcb_i;
			}
		}

		/* Initialize default user to priority mapping, UPx->TC0 */
		tc = &adapter->dcb_cfg.tc_config[0];
		tc->path[IXGBE_DCB_TX_CONFIG].up_to_tc_bitmap = 0xFF;
		tc->path[IXGBE_DCB_RX_CONFIG].up_to_tc_bitmap = 0xFF;

		adapter->dcb_cfg.bw_percentage[IXGBE_DCB_TX_CONFIG][0] = 100;
		adapter->dcb_cfg.bw_percentage[IXGBE_DCB_RX_CONFIG][0] = 100;
		adapter->dcb_cfg.rx_pba_cfg = ixgbe_dcb_pba_equal;
		adapter->dcb_cfg.pfc_mode_enable = false;
		adapter->dcb_cfg.round_robin_enable = false;
		adapter->dcb_set_bitmap = 0x00;
#ifdef CONFIG_DCB
		adapter->dcbx_cap = DCB_CAP_DCBX_HOST | DCB_CAP_DCBX_VER_CEE;
#endif /* CONFIG_DCB */

		if (hw->mac.type == ixgbe_mac_X540) {
			adapter->dcb_cfg.num_tcs.pg_tcs = 4;
			adapter->dcb_cfg.num_tcs.pfc_tcs = 4;
		}
	}
#ifdef CONFIG_DCB
	/* XXX does this need to be initialized even w/o DCB? */
	//memcpy(&adapter->temp_dcb_cfg, &adapter->dcb_cfg,
	//       sizeof(adapter->temp_dcb_cfg));

#endif
	//if (hw->mac.type == ixgbe_mac_82599EB ||
	//    hw->mac.type == ixgbe_mac_X540)
	//	hw->mbx.ops.init_params(hw);

	/* default flow control settings */
	hw->fc.requested_mode = ixgbe_fc_full;
	hw->fc.current_mode = ixgbe_fc_full;	/* init for ethtool output */

	adapter->last_lfc_mode = hw->fc.current_mode;
	ixgbe_pbthresh_setup(adapter);
	hw->fc.pause_time = IXGBE_DEFAULT_FCPAUSE;
	hw->fc.send_xon = true;
	hw->fc.disable_fc_autoneg = false;

	/* set default ring sizes */
	adapter->tx_ring_count = IXGBE_DEFAULT_TXD;
	adapter->rx_ring_count = IXGBE_DEFAULT_RXD;

	/* set default work limits */
	adapter->tx_work_limit = IXGBE_DEFAULT_TX_WORK;
	adapter->rx_work_limit = IXGBE_DEFAULT_RX_WORK;

	set_bit(__IXGBE_DOWN, &adapter->state);
out:
	return err;
}

/**
 * ixgbe_setup_tx_resources - allocate Tx resources (Descriptors)
 * @tx_ring:    tx descriptor ring (for a specific queue) to setup
 *
 * Return 0 on success, negative on failure
 **/
int ixgbe_setup_tx_resources(struct ixgbe_ring *tx_ring)
{
	struct device *dev = tx_ring->dev;
	//int orig_node = dev_to_node(dev);
	int numa_node = -1;
	int size;

	size = sizeof(struct ixgbe_tx_buffer) * tx_ring->count;

	if (tx_ring->q_vector)
		numa_node = tx_ring->q_vector->numa_node;

	tx_ring->tx_buffer_info = vzalloc_node(size, numa_node);
	if (!tx_ring->tx_buffer_info)
		tx_ring->tx_buffer_info = vzalloc(size);
	if (!tx_ring->tx_buffer_info)
		goto err;

	/* round up to nearest 4K */
	tx_ring->size = tx_ring->count * sizeof(union ixgbe_adv_tx_desc);
	tx_ring->size = ALIGN(tx_ring->size, 4096);

	//set_dev_node(dev, numa_node);
	//tx_ring->desc = dma_alloc_coherent(dev,
	//				   tx_ring->size,
	//				   &tx_ring->dma,
	//				   GFP_KERNEL);
	//set_dev_node(dev, orig_node);
	//if (!tx_ring->desc)
	//	tx_ring->desc = dma_alloc_coherent(dev, tx_ring->size,
	//					   &tx_ring->dma, GFP_KERNEL);
	//if (!tx_ring->desc)
	//	goto err;

	return 0;

err:
	vfree(tx_ring->tx_buffer_info);
	tx_ring->tx_buffer_info = NULL;
	dev_err(dev, "Unable to allocate memory for the Tx descriptor ring\n");
	return -ENOMEM;
}

/**
 * ixgbe_setup_all_tx_resources - allocate all queues Tx resources
 * @adapter: board private structure
 *
 * If this function returns with an error, then it's possible one or
 * more of the rings is populated (while the rest are not).  It is the
 * callers duty to clean those orphaned rings.
 *
 * Return 0 on success, negative on failure
 **/
static int ixgbe_setup_all_tx_resources(struct ixgbe_adapter *adapter)
{
	int i, err = 0;

	for (i = 0; i < adapter->num_tx_queues; i++) {
		err = ixgbe_setup_tx_resources(adapter->tx_ring[i]);
		if (!err)
			continue;
		e_err(probe, "Allocation for Tx Queue %u failed\n", i);
		break;
	}

	return err;
}

/**
 * ixgbe_setup_rx_resources - allocate Rx resources (Descriptors)
 * @rx_ring:    rx descriptor ring (for a specific queue) to setup
 *
 * Returns 0 on success, negative on failure
 **/
int ixgbe_setup_rx_resources(struct ixgbe_ring *rx_ring)
{
	struct device *dev = rx_ring->dev;
	//int orig_node = dev_to_node(dev);
	int numa_node = -1;
	int size;

	size = sizeof(struct ixgbe_rx_buffer) * rx_ring->count;

	if (rx_ring->q_vector)
		numa_node = rx_ring->q_vector->numa_node;

	rx_ring->rx_buffer_info = vzalloc_node(size, numa_node);
	if (!rx_ring->rx_buffer_info)
		rx_ring->rx_buffer_info = vzalloc(size);
	if (!rx_ring->rx_buffer_info)
		goto err;

	/* Round up to nearest 4K */
	rx_ring->size = rx_ring->count * sizeof(union ixgbe_adv_rx_desc);
	rx_ring->size = ALIGN(rx_ring->size, 4096);

#ifdef NO_VNIC
	set_dev_node(dev, numa_node);
	rx_ring->desc = dma_alloc_coherent(dev,
					   rx_ring->size,
					   &rx_ring->dma,
					   GFP_KERNEL);
	set_dev_node(dev, orig_node);
	if (!rx_ring->desc)
		rx_ring->desc = dma_alloc_coherent(dev, rx_ring->size,
						   &rx_ring->dma, GFP_KERNEL);
	if (!rx_ring->desc)
		goto err;

#ifndef CONFIG_IXGBE_DISABLE_PACKET_SPLIT
	ixgbe_init_rx_page_offset(rx_ring);

#endif

#endif /* NO_VNIC */
	return 0;
err:
	vfree(rx_ring->rx_buffer_info);
	rx_ring->rx_buffer_info = NULL;
	dev_err(dev, "Unable to allocate memory for the Rx descriptor ring\n");
	return -ENOMEM;
}

/**
 * ixgbe_setup_all_rx_resources - allocate all queues Rx resources
 * @adapter: board private structure
 *
 * If this function returns with an error, then it's possible one or
 * more of the rings is populated (while the rest are not).  It is the
 * callers duty to clean those orphaned rings.
 *
 * Return 0 on success, negative on failure
 **/
static int ixgbe_setup_all_rx_resources(struct ixgbe_adapter *adapter)
{
	int i, err = 0;

	for (i = 0; i < adapter->num_rx_queues; i++) {
		err = ixgbe_setup_rx_resources(adapter->rx_ring[i]);
		if (!err)
			continue;
		e_err(probe, "Allocation for Rx Queue %u failed\n", i);
		break;
	}

	return err;
}

/**
 * ixgbe_free_tx_resources - Free Tx Resources per Queue
 * @tx_ring: Tx descriptor ring for a specific queue
 *
 * Free all transmit software resources
 **/
void ixgbe_free_tx_resources(struct ixgbe_ring *tx_ring)
{
	//ixgbe_clean_tx_ring(tx_ring);

	vfree(tx_ring->tx_buffer_info);
	tx_ring->tx_buffer_info = NULL;

	/* if not set, then don't free */
	if (!tx_ring->desc)
		return;

	//dma_free_coherent(tx_ring->dev, tx_ring->size,
	//		  tx_ring->desc, tx_ring->dma);

	tx_ring->desc = NULL;
}

/**
 * ixgbe_free_all_tx_resources - Free Tx Resources for All Queues
 * @adapter: board private structure
 *
 * Free all transmit software resources
 **/
static void ixgbe_free_all_tx_resources(struct ixgbe_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_tx_queues; i++)
		if (adapter->tx_ring[i]->desc)
			ixgbe_free_tx_resources(adapter->tx_ring[i]);
}

/**
 * ixgbe_free_rx_resources - Free Rx Resources
 * @rx_ring: ring to clean the resources from
 *
 * Free all receive software resources
 **/
void ixgbe_free_rx_resources(struct ixgbe_ring *rx_ring)
{
	//ixgbe_clean_rx_ring(rx_ring);

	vfree(rx_ring->rx_buffer_info);
	rx_ring->rx_buffer_info = NULL;

	/* if not set, then don't free */
	if (!rx_ring->desc)
		return;

	//dma_free_coherent(rx_ring->dev, rx_ring->size,
	//		  rx_ring->desc, rx_ring->dma);

	rx_ring->desc = NULL;
}

/**
 * ixgbe_free_all_rx_resources - Free Rx Resources for All Queues
 * @adapter: board private structure
 *
 * Free all receive software resources
 **/
static void ixgbe_free_all_rx_resources(struct ixgbe_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_rx_queues; i++)
		if (adapter->rx_ring[i]->desc)
			ixgbe_free_rx_resources(adapter->rx_ring[i]);
}


/**
 * ixgbe_open - Called when a network interface is made active
 * @netdev: network interface device structure
 *
 * Returns 0 on success, negative value on failure
 *
 * The open entry point is called when a network interface is made
 * active by the system (IFF_UP).  At this point all resources needed
 * for transmit and receive operations are allocated, the interrupt
 * handler is registered with the OS, the watchdog timer is started,
 * and the stack is notified that the interface is ready.
 **/
//static
int ixgbe_open(struct net_device *netdev)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	int err;

	/* disallow open during test */
	if (test_bit(__IXGBE_TESTING, &adapter->state))
		return -EBUSY;

	netif_carrier_off(netdev);

	/* allocate transmit descriptors */
	err = ixgbe_setup_all_tx_resources(adapter);
	if (err)
		goto err_setup_tx;

	/* allocate receive descriptors */
	err = ixgbe_setup_all_rx_resources(adapter);
	if (err)
		goto err_setup_rx;

#ifdef NO_VNIC
	ixgbe_configure(adapter);

	err = ixgbe_request_irq(adapter);
	if (err)
		goto err_req_irq;

	ixgbe_up_complete(adapter);

err_req_irq:
#else
	return 0;
#endif
err_setup_rx:
	ixgbe_free_all_rx_resources(adapter);
err_setup_tx:
	ixgbe_free_all_tx_resources(adapter);
	ixgbe_reset(adapter);

	return err;
}

/**
 * ixgbe_close - Disables a network interface
 * @netdev: network interface device structure
 *
 * Returns 0, this is not allowed to fail
 *
 * The close entry point is called when an interface is de-activated
 * by the OS.  The hardware is still under the drivers control, but
 * needs to be disabled.  A global MAC reset is issued to stop the
 * hardware, and all transmit and receive resources are freed.
 **/
//static
int ixgbe_close(struct net_device *netdev)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	//ixgbe_down(adapter);
	//ixgbe_free_irq(adapter);

	//ixgbe_fdir_filter_exit(adapter);

	//ixgbe_free_all_tx_resources(adapter);
	//ixgbe_free_all_rx_resources(adapter);

	ixgbe_release_hw_control(adapter);

	return 0;
}





/**
 * ixgbe_get_stats - Get System Network Statistics
 * @netdev: network interface device structure
 *
 * Returns the address of the device statistics structure.
 * The statistics are actually updated from the timer callback.
 **/
//static
struct net_device_stats *ixgbe_get_stats(struct net_device *netdev)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	/* update the stats data */
	ixgbe_update_stats(adapter);

#ifdef HAVE_NETDEV_STATS_IN_NETDEV
	/* only return the current stats */
	return &netdev->stats;
#else
	/* only return the current stats */
	return &adapter->net_stats;
#endif /* HAVE_NETDEV_STATS_IN_NETDEV */
}

/**
 * ixgbe_update_stats - Update the board statistics counters.
 * @adapter: board private structure
 **/
void ixgbe_update_stats(struct ixgbe_adapter *adapter)
{
#ifdef HAVE_NETDEV_STATS_IN_NETDEV
	struct net_device_stats *net_stats = &adapter->netdev->stats;
#else
	struct net_device_stats *net_stats = &adapter->net_stats;
#endif /* HAVE_NETDEV_STATS_IN_NETDEV */
	struct ixgbe_hw *hw = &adapter->hw;
	struct ixgbe_hw_stats *hwstats = &adapter->stats;
	u64 total_mpc = 0;
	u32 i, missed_rx = 0, mpc, bprc, lxon, lxoff, xon_off_tot;
	u64 non_eop_descs = 0, restart_queue = 0, tx_busy = 0;
	u64 alloc_rx_page_failed = 0, alloc_rx_buff_failed = 0;
	u64 bytes = 0, packets = 0, hw_csum_rx_error = 0;
#ifndef IXGBE_NO_LRO
	u32 flushed = 0, coal = 0;
	int num_q_vectors = 1;
#endif
#ifdef IXGBE_FCOE
	struct ixgbe_fcoe *fcoe = &adapter->fcoe;
	unsigned int cpu;
	u64 fcoe_noddp_counts_sum = 0, fcoe_noddp_ext_buff_counts_sum = 0;
#endif /* IXGBE_FCOE */

	printk(KERN_DEBUG "ixgbe_update_stats, tx_queues=%d, rx_queues=%d\n",
			adapter->num_tx_queues, adapter->num_rx_queues);

	if (test_bit(__IXGBE_DOWN, &adapter->state) ||
	    test_bit(__IXGBE_RESETTING, &adapter->state))
		return;

#ifndef IXGBE_NO_LRO
	if (adapter->flags & IXGBE_FLAG_MSIX_ENABLED)
		num_q_vectors = adapter->num_msix_vectors - NON_Q_VECTORS;

#endif
	if (adapter->flags2 & IXGBE_FLAG2_RSC_ENABLED) {
		u64 rsc_count = 0;
		u64 rsc_flush = 0;
		for (i = 0; i < adapter->num_rx_queues; i++) {
			rsc_count += adapter->rx_ring[i]->rx_stats.rsc_count;
			rsc_flush += adapter->rx_ring[i]->rx_stats.rsc_flush;
		}
		adapter->rsc_total_count = rsc_count;
		adapter->rsc_total_flush = rsc_flush;
	}

#ifndef IXGBE_NO_LRO
	for (i = 0; i < num_q_vectors; i++) {
		struct ixgbe_q_vector *q_vector = adapter->q_vector[i];
		if (!q_vector)
			continue;
		flushed += q_vector->lrolist.stats.flushed;
		coal += q_vector->lrolist.stats.coal;
	}
	adapter->lro_stats.flushed = flushed;
	adapter->lro_stats.coal = coal;

#endif
	for (i = 0; i < adapter->num_rx_queues; i++) {
		struct ixgbe_ring *rx_ring = adapter->rx_ring[i];
		non_eop_descs += rx_ring->rx_stats.non_eop_descs;
		alloc_rx_page_failed += rx_ring->rx_stats.alloc_rx_page_failed;
		alloc_rx_buff_failed += rx_ring->rx_stats.alloc_rx_buff_failed;
		hw_csum_rx_error += rx_ring->rx_stats.csum_err;
		bytes += rx_ring->stats.bytes;
		packets += rx_ring->stats.packets;

	}
	adapter->non_eop_descs = non_eop_descs;
	adapter->alloc_rx_page_failed = alloc_rx_page_failed;
	adapter->alloc_rx_buff_failed = alloc_rx_buff_failed;
	adapter->hw_csum_rx_error = hw_csum_rx_error;
	net_stats->rx_bytes = bytes;
	net_stats->rx_packets = packets;

	bytes = 0;
	packets = 0;
	/* gather some stats to the adapter struct that are per queue */
	for (i = 0; i < adapter->num_tx_queues; i++) {
		struct ixgbe_ring *tx_ring = adapter->tx_ring[i];
		restart_queue += tx_ring->tx_stats.restart_queue;
		tx_busy += tx_ring->tx_stats.tx_busy;
		bytes += tx_ring->stats.bytes;
		packets += tx_ring->stats.packets;
	}
	adapter->restart_queue = restart_queue;
	adapter->tx_busy = tx_busy;
	net_stats->tx_bytes = bytes;
	net_stats->tx_packets = packets;

	hwstats->crcerrs += IXGBE_READ_REG(hw, IXGBE_CRCERRS);

	/* 8 register reads */
	for (i = 0; i < 8; i++) {
		/* for packet buffers not used, the register should read 0 */
		mpc = IXGBE_READ_REG(hw, IXGBE_MPC(i));
		missed_rx += mpc;
		hwstats->mpc[i] += mpc;
		total_mpc += hwstats->mpc[i];
		hwstats->pxontxc[i] += IXGBE_READ_REG(hw, IXGBE_PXONTXC(i));
		hwstats->pxofftxc[i] += IXGBE_READ_REG(hw, IXGBE_PXOFFTXC(i));
		switch (hw->mac.type) {
		case ixgbe_mac_82598EB:
			hwstats->rnbc[i] += IXGBE_READ_REG(hw, IXGBE_RNBC(i));
			hwstats->qbtc[i] += IXGBE_READ_REG(hw, IXGBE_QBTC(i));
			hwstats->qbrc[i] += IXGBE_READ_REG(hw, IXGBE_QBRC(i));
			hwstats->pxonrxc[i] +=
				IXGBE_READ_REG(hw, IXGBE_PXONRXC(i));
			break;
		case ixgbe_mac_82599EB:
		case ixgbe_mac_X540:
			hwstats->pxonrxc[i] +=
				IXGBE_READ_REG(hw, IXGBE_PXONRXCNT(i));
			break;
		default:
			break;
		}
	}

	/*16 register reads */
	for (i = 0; i < 16; i++) {
		hwstats->qptc[i] += IXGBE_READ_REG(hw, IXGBE_QPTC(i));
		hwstats->qprc[i] += IXGBE_READ_REG(hw, IXGBE_QPRC(i));
		if ((hw->mac.type == ixgbe_mac_82599EB) ||
		    (hw->mac.type == ixgbe_mac_X540)) {
			hwstats->qbtc[i] += IXGBE_READ_REG(hw, IXGBE_QBTC_L(i));
			IXGBE_READ_REG(hw, IXGBE_QBTC_H(i)); /* to clear */
			hwstats->qbrc[i] += IXGBE_READ_REG(hw, IXGBE_QBRC_L(i));
			IXGBE_READ_REG(hw, IXGBE_QBRC_H(i)); /* to clear */
		}
	}

	hwstats->gprc += IXGBE_READ_REG(hw, IXGBE_GPRC);
	/* work around hardware counting issue */
	hwstats->gprc -= missed_rx;

	ixgbe_update_xoff_received(adapter);

	/* 82598 hardware only has a 32 bit counter in the high register */
	switch (hw->mac.type) {
	case ixgbe_mac_82598EB:
		hwstats->lxonrxc += IXGBE_READ_REG(hw, IXGBE_LXONRXC);
		hwstats->gorc += IXGBE_READ_REG(hw, IXGBE_GORCH);
		hwstats->gotc += IXGBE_READ_REG(hw, IXGBE_GOTCH);
		hwstats->tor += IXGBE_READ_REG(hw, IXGBE_TORH);
		break;
	case ixgbe_mac_X540:
		/* OS2BMC stats are X540 only*/
		hwstats->o2bgptc += IXGBE_READ_REG(hw, IXGBE_O2BGPTC);
		hwstats->o2bspc += IXGBE_READ_REG(hw, IXGBE_O2BSPC);
		hwstats->b2ospc += IXGBE_READ_REG(hw, IXGBE_B2OSPC);
		hwstats->b2ogprc += IXGBE_READ_REG(hw, IXGBE_B2OGPRC);
	case ixgbe_mac_82599EB:
		for (i = 0; i < 16; i++)
			adapter->hw_rx_no_dma_resources +=
					     IXGBE_READ_REG(hw, IXGBE_QPRDC(i));
		hwstats->gorc += IXGBE_READ_REG(hw, IXGBE_GORCL);
		IXGBE_READ_REG(hw, IXGBE_GORCH); /* to clear */
		hwstats->gotc += IXGBE_READ_REG(hw, IXGBE_GOTCL);
		IXGBE_READ_REG(hw, IXGBE_GOTCH); /* to clear */
		hwstats->tor += IXGBE_READ_REG(hw, IXGBE_TORL);
		IXGBE_READ_REG(hw, IXGBE_TORH); /* to clear */
		hwstats->lxonrxc += IXGBE_READ_REG(hw, IXGBE_LXONRXCNT);
#ifdef HAVE_TX_MQ
		hwstats->fdirmatch += IXGBE_READ_REG(hw, IXGBE_FDIRMATCH);
		hwstats->fdirmiss += IXGBE_READ_REG(hw, IXGBE_FDIRMISS);
#endif /* HAVE_TX_MQ */
#ifdef IXGBE_FCOE
		hwstats->fccrc += IXGBE_READ_REG(hw, IXGBE_FCCRC);
		hwstats->fclast += IXGBE_READ_REG(hw, IXGBE_FCLAST);
		hwstats->fcoerpdc += IXGBE_READ_REG(hw, IXGBE_FCOERPDC);
		hwstats->fcoeprc += IXGBE_READ_REG(hw, IXGBE_FCOEPRC);
		hwstats->fcoeptc += IXGBE_READ_REG(hw, IXGBE_FCOEPTC);
		hwstats->fcoedwrc += IXGBE_READ_REG(hw, IXGBE_FCOEDWRC);
		hwstats->fcoedwtc += IXGBE_READ_REG(hw, IXGBE_FCOEDWTC);
		/* Add up per cpu counters for total ddp aloc fail */
		if (fcoe && fcoe->pcpu_noddp && fcoe->pcpu_noddp_ext_buff) {
			for_each_possible_cpu(cpu) {
				fcoe_noddp_counts_sum +=
					*per_cpu_ptr(fcoe->pcpu_noddp, cpu);
				fcoe_noddp_ext_buff_counts_sum +=
					*per_cpu_ptr(fcoe->
						pcpu_noddp_ext_buff, cpu);
			}
		}
		hwstats->fcoe_noddp = fcoe_noddp_counts_sum;
		hwstats->fcoe_noddp_ext_buff = fcoe_noddp_ext_buff_counts_sum;

#endif /* IXGBE_FCOE */
		break;
	default:
		break;
	}
	bprc = IXGBE_READ_REG(hw, IXGBE_BPRC);
	hwstats->bprc += bprc;
	hwstats->mprc += IXGBE_READ_REG(hw, IXGBE_MPRC);
	if (hw->mac.type == ixgbe_mac_82598EB)
		hwstats->mprc -= bprc;
	hwstats->roc += IXGBE_READ_REG(hw, IXGBE_ROC);
	hwstats->prc64 += IXGBE_READ_REG(hw, IXGBE_PRC64);
	hwstats->prc127 += IXGBE_READ_REG(hw, IXGBE_PRC127);
	hwstats->prc255 += IXGBE_READ_REG(hw, IXGBE_PRC255);
	hwstats->prc511 += IXGBE_READ_REG(hw, IXGBE_PRC511);
	hwstats->prc1023 += IXGBE_READ_REG(hw, IXGBE_PRC1023);
	hwstats->prc1522 += IXGBE_READ_REG(hw, IXGBE_PRC1522);
	hwstats->rlec += IXGBE_READ_REG(hw, IXGBE_RLEC);
	lxon = IXGBE_READ_REG(hw, IXGBE_LXONTXC);
	hwstats->lxontxc += lxon;
	lxoff = IXGBE_READ_REG(hw, IXGBE_LXOFFTXC);
	hwstats->lxofftxc += lxoff;
	hwstats->gptc += IXGBE_READ_REG(hw, IXGBE_GPTC);
	hwstats->mptc += IXGBE_READ_REG(hw, IXGBE_MPTC);
	/*
	 * 82598 errata - tx of flow control packets is included in tx counters
	 */
	xon_off_tot = lxon + lxoff;
	hwstats->gptc -= xon_off_tot;
	hwstats->mptc -= xon_off_tot;
	hwstats->gotc -= (xon_off_tot * (ETH_ZLEN + ETH_FCS_LEN));
	hwstats->ruc += IXGBE_READ_REG(hw, IXGBE_RUC);
	hwstats->rfc += IXGBE_READ_REG(hw, IXGBE_RFC);
	hwstats->rjc += IXGBE_READ_REG(hw, IXGBE_RJC);
	hwstats->tpr += IXGBE_READ_REG(hw, IXGBE_TPR);
	hwstats->ptc64 += IXGBE_READ_REG(hw, IXGBE_PTC64);
	hwstats->ptc64 -= xon_off_tot;
	hwstats->ptc127 += IXGBE_READ_REG(hw, IXGBE_PTC127);
	hwstats->ptc255 += IXGBE_READ_REG(hw, IXGBE_PTC255);
	hwstats->ptc511 += IXGBE_READ_REG(hw, IXGBE_PTC511);
	hwstats->ptc1023 += IXGBE_READ_REG(hw, IXGBE_PTC1023);
	hwstats->ptc1522 += IXGBE_READ_REG(hw, IXGBE_PTC1522);
	hwstats->bptc += IXGBE_READ_REG(hw, IXGBE_BPTC);
	/* Fill out the OS statistics structure */
	net_stats->multicast = hwstats->mprc;

	/* Rx Errors */
	net_stats->rx_errors = hwstats->crcerrs +
				       hwstats->rlec;
	net_stats->rx_dropped = 0;
	net_stats->rx_length_errors = hwstats->rlec;
	net_stats->rx_crc_errors = hwstats->crcerrs;
	net_stats->rx_missed_errors = total_mpc;

	/*
	 * VF Stats Collection - skip while resetting because these
	 * are not clear on read and otherwise you'll sometimes get
	 * crazy values.
	 */
	if (!test_bit(__IXGBE_RESETTING, &adapter->state)) {
		for (i = 0; i < adapter->num_vfs; i++) {
			UPDATE_VF_COUNTER_32bit(IXGBE_PVFGPRC(i),	      \
					adapter->vfinfo[i].last_vfstats.gprc, \
					adapter->vfinfo[i].vfstats.gprc);
			UPDATE_VF_COUNTER_32bit(IXGBE_PVFGPTC(i),	      \
					adapter->vfinfo[i].last_vfstats.gptc, \
					adapter->vfinfo[i].vfstats.gptc);
			UPDATE_VF_COUNTER_36bit(IXGBE_PVFGORC_LSB(i),	      \
					IXGBE_PVFGORC_MSB(i),		      \
					adapter->vfinfo[i].last_vfstats.gorc, \
					adapter->vfinfo[i].vfstats.gorc);
			UPDATE_VF_COUNTER_36bit(IXGBE_PVFGOTC_LSB(i),	      \
					IXGBE_PVFGOTC_MSB(i),		      \
					adapter->vfinfo[i].last_vfstats.gotc, \
					adapter->vfinfo[i].vfstats.gotc);
			UPDATE_VF_COUNTER_32bit(IXGBE_PVFMPRC(i),	      \
					adapter->vfinfo[i].last_vfstats.mprc, \
					adapter->vfinfo[i].vfstats.mprc);
		}
	}
}


#ifdef NO_VNIC

/**
 * ixgbe_watchdog_update_link - update the link status
 * @adapter - pointer to the device adapter structure
 * @link_speed - pointer to a u32 to store the link_speed
 **/
static void ixgbe_watchdog_update_link(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 link_speed = adapter->link_speed;
	bool link_up = adapter->link_up;
	bool pfc_en = adapter->dcb_cfg.pfc_mode_enable;

	if (!(adapter->flags & IXGBE_FLAG_NEED_LINK_UPDATE))
		return;

	if (hw->mac.ops.check_link) {
		hw->mac.ops.check_link(hw, &link_speed, &link_up, false);
	} else {
		/* always assume link is up, if no check link function */
		link_speed = IXGBE_LINK_SPEED_10GB_FULL;
		link_up = true;
	}

#ifdef HAVE_DCBNL_IEEE
	if (adapter->ixgbe_ieee_pfc)
		pfc_en |= !!(adapter->ixgbe_ieee_pfc->pfc_en);

#endif
	if (link_up && !((adapter->flags & IXGBE_FLAG_DCB_ENABLED) && pfc_en)) {
		hw->mac.ops.fc_enable(hw);
		//ixgbe_set_rx_drop_en(adapter);
	}

	if (link_up ||
	    time_after(jiffies, (adapter->link_check_timeout +
				 IXGBE_TRY_LINK_TIMEOUT))) {
		adapter->flags &= ~IXGBE_FLAG_NEED_LINK_UPDATE;
		IXGBE_WRITE_REG(hw, IXGBE_EIMS, IXGBE_EIMC_LSC);
		IXGBE_WRITE_FLUSH(hw);
	}

	adapter->link_up = link_up;
	adapter->link_speed = link_speed;
}
#endif



#ifdef NO_VNIC

/**
 * ixgbe_service_task - manages and runs subtasks
 * @work: pointer to work_struct containing our data
 **/
static void ixgbe_service_task(struct work_struct *work)
{
	//struct ixgbe_adapter *adapter = container_of(work,
	//					     struct ixgbe_adapter,
	//					     service_task);

	//ixgbe_reset_subtask(adapter);
	//ixgbe_sfp_detection_subtask(adapter);
	//ixgbe_sfp_link_config_subtask(adapter);
	//ixgbe_check_overtemp_subtask(adapter);
	//ixgbe_watchdog_subtask(adapter);
#ifdef HAVE_TX_MQ
	//ixgbe_fdir_reinit_subtask(adapter);
#endif
	//ixgbe_check_hang_subtask(adapter);

	//ixgbe_service_event_complete(adapter);
}




#define IXGBE_TXD_CMD (IXGBE_TXD_CMD_EOP | \
		       IXGBE_TXD_CMD_RS)


/**
 * ixgbe_set_mac - Change the Ethernet Address of the NIC
 * @netdev: network interface device structure
 * @p: pointer to an address structure
 *
 * Returns 0 on success, negative on failure
 **/
static int ixgbe_set_mac(struct net_device *netdev, void *p)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_hw *hw = &adapter->hw;
	struct sockaddr *addr = p;
	int ret;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	ixgbe_del_mac_filter(adapter, hw->mac.addr,
			     adapter->num_vfs);
	memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);
	memcpy(hw->mac.addr, addr->sa_data, netdev->addr_len);


	/* set the correct pool for the new PF MAC address in entry 0 */
	ret = ixgbe_add_mac_filter(adapter, hw->mac.addr,
				    adapter->num_vfs);
	return ret > 0 ? 0 : ret;
}


/**
 * ixgbe_ioctl -
 * @netdev:
 * @ifreq:
 * @cmd:
 **/
static int ixgbe_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
	switch (cmd) {
#ifdef ETHTOOL_OPS_COMPAT
	case SIOCETHTOOL:
		return ethtool_ioctl(ifr);
#endif
	default:
		return -EOPNOTSUPP;
	}
}
#endif /* NO_VNIC */


void ixgbe_do_reset(struct net_device *netdev)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	if (netif_running(netdev))
		ixgbe_reinit_locked(adapter);
	else
		ixgbe_reset(adapter);
}






/**
 * ixgbe_probe - Device Initialization Routine
 * @pdev: PCI device information struct
 * @ent: entry in ixgbe_pci_tbl
 *
 * Returns 0 on success, negative on failure
 *
 * ixgbe_probe initializes an adapter identified by a pci_dev structure.
 * The OS initialization, configuring of the adapter private structure,
 * and a hardware reset occur.
 **/
//static
int ixgbe_kni_probe(struct pci_dev *pdev,
				 struct net_device **lad_dev)
{
	size_t count;
	struct net_device *netdev;
	struct ixgbe_adapter *adapter = NULL;
	struct ixgbe_hw *hw = NULL;
	static int cards_found;
	int i, err;
	u16 offset;
	u16 eeprom_verh, eeprom_verl, eeprom_cfg_blkh, eeprom_cfg_blkl;
	u32 etrack_id;
	u16 build, major, patch;
	char *info_string, *i_s_var;
	u8 part_str[IXGBE_PBANUM_LENGTH];
	enum ixgbe_mac_type mac_type = ixgbe_mac_unknown;
#ifdef HAVE_TX_MQ
	unsigned int indices = num_possible_cpus();
#endif /* HAVE_TX_MQ */
#ifdef IXGBE_FCOE
	u16 device_caps;
#endif
	u16 wol_cap;

	err = pci_enable_device_mem(pdev);
	if (err)
		return err;


#ifdef NO_VNIC
	err = pci_request_selected_regions(pdev, pci_select_bars(pdev,
					   IORESOURCE_MEM), ixgbe_driver_name);
	if (err) {
		dev_err(pci_dev_to_dev(pdev),
			"pci_request_selected_regions failed 0x%x\n", err);
		goto err_pci_reg;
	}
#endif

	/*
	 * The mac_type is needed before we have the adapter is  set up
	 * so rather than maintain two devID -> MAC tables we dummy up
	 * an ixgbe_hw stuct and use ixgbe_set_mac_type.
	 */
	hw = vmalloc(sizeof(struct ixgbe_hw));
	if (!hw) {
		pr_info("Unable to allocate memory for early mac "
			"check\n");
	} else {
		hw->vendor_id = pdev->vendor;
		hw->device_id = pdev->device;
		ixgbe_set_mac_type(hw);
		mac_type = hw->mac.type;
		vfree(hw);
	}

#ifdef NO_VNIC
	/*
	 * Workaround of Silicon errata on 82598. Disable LOs in the PCI switch
	 * port to which the 82598 is connected to prevent duplicate
	 * completions caused by LOs.  We need the mac type so that we only
	 * do this on 82598 devices, ixgbe_set_mac_type does this for us if
	 * we set it's device ID.
	 */
	if (mac_type == ixgbe_mac_82598EB)
		pci_disable_link_state(pdev, PCIE_LINK_STATE_L0S);

	pci_enable_pcie_error_reporting(pdev);

	pci_set_master(pdev);
#endif

#ifdef HAVE_TX_MQ
#ifdef CONFIG_DCB
#ifdef HAVE_MQPRIO
	indices *= IXGBE_DCB_MAX_TRAFFIC_CLASS;
#else
	indices = max_t(unsigned int, indices, IXGBE_MAX_DCB_INDICES);
#endif /* HAVE_MQPRIO */
#endif /* CONFIG_DCB */

	if (mac_type == ixgbe_mac_82598EB)
		indices = min_t(unsigned int, indices, IXGBE_MAX_RSS_INDICES);
	else
		indices = min_t(unsigned int, indices, IXGBE_MAX_FDIR_INDICES);

#ifdef IXGBE_FCOE
	indices += min_t(unsigned int, num_possible_cpus(),
			 IXGBE_MAX_FCOE_INDICES);
#endif
	netdev = alloc_etherdev_mq(sizeof(struct ixgbe_adapter), indices);
#else /* HAVE_TX_MQ */
	netdev = alloc_etherdev(sizeof(struct ixgbe_adapter));
#endif /* HAVE_TX_MQ */
	if (!netdev) {
		err = -ENOMEM;
		goto err_alloc_etherdev;
	}

	SET_NETDEV_DEV(netdev, &pdev->dev);

	adapter = netdev_priv(netdev);
	//pci_set_drvdata(pdev, adapter);

	adapter->netdev = netdev;
	adapter->pdev = pdev;
	hw = &adapter->hw;
	hw->back = adapter;
	adapter->msg_enable = (1 << DEFAULT_DEBUG_LEVEL_SHIFT) - 1;

#ifdef HAVE_PCI_ERS
	/*
	 * call save state here in standalone driver because it relies on
	 * adapter struct to exist, and needs to call netdev_priv
	 */
	pci_save_state(pdev);

#endif
	hw->hw_addr = ioremap(pci_resource_start(pdev, 0),
			      pci_resource_len(pdev, 0));
	if (!hw->hw_addr) {
		err = -EIO;
		goto err_ioremap;
	}
	//ixgbe_assign_netdev_ops(netdev);
	ixgbe_set_ethtool_ops(netdev);

	strlcpy(netdev->name, pci_name(pdev), sizeof(netdev->name));

	adapter->bd_number = cards_found;

	/* setup the private structure */
	err = ixgbe_sw_init(adapter);
	if (err)
		goto err_sw_init;

	/* Make it possible the adapter to be woken up via WOL */
	switch (adapter->hw.mac.type) {
	case ixgbe_mac_82599EB:
	case ixgbe_mac_X540:
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_WUS, ~0);
		break;
	default:
		break;
	}

	/*
	 * check_options must be called before setup_link to set up
	 * hw->fc completely
	 */
	//ixgbe_check_options(adapter);

#ifndef NO_VNIC
	/* reset_hw fills in the perm_addr as well */
	hw->phy.reset_if_overtemp = true;
	err = hw->mac.ops.reset_hw(hw);
	hw->phy.reset_if_overtemp = false;
	if (err == IXGBE_ERR_SFP_NOT_PRESENT &&
	    hw->mac.type == ixgbe_mac_82598EB) {
		err = 0;
	} else if (err == IXGBE_ERR_SFP_NOT_SUPPORTED) {
		e_dev_err("failed to load because an unsupported SFP+ "
			  "module type was detected.\n");
		e_dev_err("Reload the driver after installing a supported "
			  "module.\n");
		goto err_sw_init;
	} else if (err) {
		e_dev_err("HW Init failed: %d\n", err);
		goto err_sw_init;
	}
#endif

	//if (adapter->flags & IXGBE_FLAG_SRIOV_ENABLED)
	//	ixgbe_probe_vf(adapter);


#ifdef MAX_SKB_FRAGS
	netdev->features |= NETIF_F_SG |
			    NETIF_F_IP_CSUM;

#ifdef NETIF_F_IPV6_CSUM
	netdev->features |= NETIF_F_IPV6_CSUM;
#endif

#ifdef NETIF_F_HW_VLAN_TX
	netdev->features |= NETIF_F_HW_VLAN_TX |
			    NETIF_F_HW_VLAN_RX;
#endif
#ifdef NETIF_F_TSO
	netdev->features |= NETIF_F_TSO;
#endif /* NETIF_F_TSO */
#ifdef NETIF_F_TSO6
	netdev->features |= NETIF_F_TSO6;
#endif /* NETIF_F_TSO6 */
#ifdef NETIF_F_RXHASH
	netdev->features |= NETIF_F_RXHASH;
#endif /* NETIF_F_RXHASH */

#ifdef HAVE_NDO_SET_FEATURES
	netdev->features |= NETIF_F_RXCSUM;

	/* copy netdev features into list of user selectable features */
	netdev->hw_features |= netdev->features;

	/* give us the option of enabling RSC/LRO later */
#ifdef IXGBE_NO_LRO
	if (adapter->flags2 & IXGBE_FLAG2_RSC_CAPABLE)
#endif
		netdev->hw_features |= NETIF_F_LRO;

#else
#ifdef NETIF_F_GRO

	/* this is only needed on kernels prior to 2.6.39 */
	netdev->features |= NETIF_F_GRO;
#endif /* NETIF_F_GRO */
#endif

#ifdef NETIF_F_HW_VLAN_TX
	/* set this bit last since it cannot be part of hw_features */
	netdev->features |= NETIF_F_HW_VLAN_FILTER;
#endif
	switch (adapter->hw.mac.type) {
	case ixgbe_mac_82599EB:
	case ixgbe_mac_X540:
		netdev->features |= NETIF_F_SCTP_CSUM;
#ifdef HAVE_NDO_SET_FEATURES
		netdev->hw_features |= NETIF_F_SCTP_CSUM |
				       NETIF_F_NTUPLE;
#endif
		break;
	default:
		break;
	}

#ifdef HAVE_NETDEV_VLAN_FEATURES
	netdev->vlan_features |= NETIF_F_SG |
				 NETIF_F_IP_CSUM |
				 NETIF_F_IPV6_CSUM |
				 NETIF_F_TSO |
				 NETIF_F_TSO6;

#endif /* HAVE_NETDEV_VLAN_FEATURES */
	/*
	 * If perfect filters were enabled in check_options(), enable them
	 * on the netdevice too.
	 */
	if (adapter->flags & IXGBE_FLAG_FDIR_PERFECT_CAPABLE)
		netdev->features |= NETIF_F_NTUPLE;
	if (adapter->flags & IXGBE_FLAG_VMDQ_ENABLED)
		adapter->flags &= ~IXGBE_FLAG_RSS_ENABLED;
	if (adapter->flags & IXGBE_FLAG_DCB_ENABLED)
		adapter->flags &= ~IXGBE_FLAG_RSS_ENABLED;
	if (adapter->flags & IXGBE_FLAG_VMDQ_ENABLED) {
		adapter->flags &= ~IXGBE_FLAG_FDIR_HASH_CAPABLE;
		/* clear n-tuple support in the netdev unconditionally */
		netdev->features &= ~NETIF_F_NTUPLE;
	}

#ifdef NETIF_F_RXHASH
	if (!(adapter->flags & IXGBE_FLAG_RSS_ENABLED))
		netdev->features &= ~NETIF_F_RXHASH;

#endif /* NETIF_F_RXHASH */
	if (netdev->features & NETIF_F_LRO) {
		if ((adapter->flags2 & IXGBE_FLAG2_RSC_CAPABLE) &&
		    ((adapter->rx_itr_setting == 1) ||
		     (adapter->rx_itr_setting > IXGBE_MIN_RSC_ITR))) {
			adapter->flags2 |= IXGBE_FLAG2_RSC_ENABLED;
		} else if (adapter->flags2 & IXGBE_FLAG2_RSC_CAPABLE) {
#ifdef IXGBE_NO_LRO
			e_info(probe, "InterruptThrottleRate set too high, "
			       "disabling RSC\n");
#else
			e_info(probe, "InterruptThrottleRate set too high, "
			       "falling back to software LRO\n");
#endif
		}
	}
#ifdef CONFIG_DCB
	//netdev->dcbnl_ops = &dcbnl_ops;
#endif

#ifdef IXGBE_FCOE
#ifdef NETIF_F_FSO
	if (adapter->flags & IXGBE_FLAG_FCOE_CAPABLE) {
		ixgbe_get_device_caps(hw, &device_caps);
		if (device_caps & IXGBE_DEVICE_CAPS_FCOE_OFFLOADS) {
			adapter->flags &= ~IXGBE_FLAG_FCOE_ENABLED;
			adapter->flags &= ~IXGBE_FLAG_FCOE_CAPABLE;
			e_info(probe, "FCoE offload feature is not available. "
			       "Disabling FCoE offload feature\n");
		}
#ifndef HAVE_NETDEV_OPS_FCOE_ENABLE
		else {
			adapter->flags |= IXGBE_FLAG_FCOE_ENABLED;
			adapter->ring_feature[RING_F_FCOE].indices =
				IXGBE_FCRETA_SIZE;
			netdev->features |= NETIF_F_FSO |
					    NETIF_F_FCOE_CRC |
					    NETIF_F_FCOE_MTU;
			netdev->fcoe_ddp_xid = IXGBE_FCOE_DDP_MAX - 1;
		}
#endif /* HAVE_NETDEV_OPS_FCOE_ENABLE */
#ifdef HAVE_NETDEV_VLAN_FEATURES
		netdev->vlan_features |= NETIF_F_FSO |
					 NETIF_F_FCOE_CRC |
					 NETIF_F_FCOE_MTU;
#endif /* HAVE_NETDEV_VLAN_FEATURES */
	}
#endif /* NETIF_F_FSO */
#endif /* IXGBE_FCOE */

#endif /* MAX_SKB_FRAGS */
	/* make sure the EEPROM is good */
	if (hw->eeprom.ops.validate_checksum &&
	    (hw->eeprom.ops.validate_checksum(hw, NULL) < 0)) {
		e_dev_err("The EEPROM Checksum Is Not Valid\n");
		err = -EIO;
		goto err_sw_init;
	}

	memcpy(netdev->dev_addr, hw->mac.perm_addr, netdev->addr_len);
#ifdef ETHTOOL_GPERMADDR
	memcpy(netdev->perm_addr, hw->mac.perm_addr, netdev->addr_len);

	if (ixgbe_validate_mac_addr(netdev->perm_addr)) {
		e_dev_err("invalid MAC address\n");
		err = -EIO;
		goto err_sw_init;
	}
#else
	if (ixgbe_validate_mac_addr(netdev->dev_addr)) {
		e_dev_err("invalid MAC address\n");
		err = -EIO;
		goto err_sw_init;
	}
#endif
	memcpy(&adapter->mac_table[0].addr, hw->mac.perm_addr,
	       netdev->addr_len);
	adapter->mac_table[0].queue = adapter->num_vfs;
	adapter->mac_table[0].state = (IXGBE_MAC_STATE_DEFAULT |
				       IXGBE_MAC_STATE_IN_USE);
	hw->mac.ops.set_rar(hw, 0, adapter->mac_table[0].addr,
			    adapter->mac_table[0].queue,
			    IXGBE_RAH_AV);

	//setup_timer(&adapter->service_timer, &ixgbe_service_timer,
	//	    (unsigned long) adapter);

	//INIT_WORK(&adapter->service_task, ixgbe_service_task);
	//clear_bit(__IXGBE_SERVICE_SCHED, &adapter->state);

	//err = ixgbe_init_interrupt_scheme(adapter);
	//if (err)
	//	goto err_sw_init;

	//adapter->flags &= ~IXGBE_FLAG_RSS_ENABLED;
	ixgbe_set_num_queues(adapter);

	adapter->wol = 0;
	/* WOL not supported for all but the following */
	switch (pdev->device) {
	case IXGBE_DEV_ID_82599_SFP:
		/* Only these subdevice supports WOL */
		switch (pdev->subsystem_device) {
		case IXGBE_SUBDEV_ID_82599_560FLR:
			/* only support first port */
			if (hw->bus.func != 0)
				break;
		case IXGBE_SUBDEV_ID_82599_SFP:
			adapter->wol = IXGBE_WUFC_MAG;
			break;
		}
		break;
	case IXGBE_DEV_ID_82599_COMBO_BACKPLANE:
		/* All except this subdevice support WOL */
		if (pdev->subsystem_device != IXGBE_SUBDEV_ID_82599_KX4_KR_MEZZ)
			adapter->wol = IXGBE_WUFC_MAG;
		break;
	case IXGBE_DEV_ID_82599_KX4:
		adapter->wol = IXGBE_WUFC_MAG;
		break;
	case IXGBE_DEV_ID_X540T:
		/* Check eeprom to see if it is enabled */
		ixgbe_read_eeprom(hw, 0x2c, &adapter->eeprom_cap);
		wol_cap = adapter->eeprom_cap & IXGBE_DEVICE_CAPS_WOL_MASK;

		if ((wol_cap == IXGBE_DEVICE_CAPS_WOL_PORT0_1) ||
		    ((wol_cap == IXGBE_DEVICE_CAPS_WOL_PORT0) &&
		     (hw->bus.func == 0)))
			adapter->wol = IXGBE_WUFC_MAG;
		break;
	}
	//device_set_wakeup_enable(&adapter->pdev->dev, adapter->wol);


	/*
	 * Save off EEPROM version number and Option Rom version which
	 * together make a unique identify for the eeprom
	 */
	ixgbe_read_eeprom(hw, 0x2e, &eeprom_verh);
	ixgbe_read_eeprom(hw, 0x2d, &eeprom_verl);

	etrack_id = (eeprom_verh << 16) | eeprom_verl;

	ixgbe_read_eeprom(hw, 0x17, &offset);

	/* Make sure offset to SCSI block is valid */
	if (!(offset == 0x0) && !(offset == 0xffff)) {
		ixgbe_read_eeprom(hw, offset + 0x84, &eeprom_cfg_blkh);
		ixgbe_read_eeprom(hw, offset + 0x83, &eeprom_cfg_blkl);

		/* Only display Option Rom if exist */
		if (eeprom_cfg_blkl && eeprom_cfg_blkh) {
			major = eeprom_cfg_blkl >> 8;
			build = (eeprom_cfg_blkl << 8) | (eeprom_cfg_blkh >> 8);
			patch = eeprom_cfg_blkh & 0x00ff;

			snprintf(adapter->eeprom_id, sizeof(adapter->eeprom_id),
				 "0x%08x, %d.%d.%d", etrack_id, major, build,
				 patch);
		} else {
			snprintf(adapter->eeprom_id, sizeof(adapter->eeprom_id),
				 "0x%08x", etrack_id);
		}
	} else {
		snprintf(adapter->eeprom_id, sizeof(adapter->eeprom_id),
			 "0x%08x", etrack_id);
	}

	/* reset the hardware with the new settings */
	err = hw->mac.ops.start_hw(hw);
	if (err == IXGBE_ERR_EEPROM_VERSION) {
		/* We are running on a pre-production device, log a warning */
		e_dev_warn("This device is a pre-production adapter/LOM. "
			   "Please be aware there may be issues associated "
			   "with your hardware.  If you are experiencing "
			   "problems please contact your Intel or hardware "
			   "representative who provided you with this "
			   "hardware.\n");
	}
	/* pick up the PCI bus settings for reporting later */
	if (hw->mac.ops.get_bus_info)
		hw->mac.ops.get_bus_info(hw);

	strlcpy(netdev->name, "eth%d", sizeof(netdev->name));
	*lad_dev = netdev;

	adapter->netdev_registered = true;
#ifdef NO_VNIC
	/* power down the optics */
	if ((hw->phy.multispeed_fiber) ||
	    ((hw->mac.ops.get_media_type(hw) == ixgbe_media_type_fiber) &&
	     (hw->mac.type == ixgbe_mac_82599EB)))
		ixgbe_disable_tx_laser(hw);

	/* carrier off reporting is important to ethtool even BEFORE open */
	netif_carrier_off(netdev);
	/* keep stopping all the transmit queues for older kernels */
	netif_tx_stop_all_queues(netdev);
#endif

	/* print all messages at the end so that we use our eth%d name */
	/* print bus type/speed/width info */
	e_dev_info("(PCI Express:%s:%s) ",
		   (hw->bus.speed == ixgbe_bus_speed_5000 ? "5.0GT/s" :
		   hw->bus.speed == ixgbe_bus_speed_2500 ? "2.5GT/s" :
		   "Unknown"),
		   (hw->bus.width == ixgbe_bus_width_pcie_x8 ? "Width x8" :
		   hw->bus.width == ixgbe_bus_width_pcie_x4 ? "Width x4" :
		   hw->bus.width == ixgbe_bus_width_pcie_x1 ? "Width x1" :
		   "Unknown"));

	/* print the MAC address */
	for (i = 0; i < 6; i++)
		pr_cont("%2.2x%c", netdev->dev_addr[i], i == 5 ? '\n' : ':');

	/* First try to read PBA as a string */
	err = ixgbe_read_pba_string(hw, part_str, IXGBE_PBANUM_LENGTH);
	if (err)
		strlcpy(part_str, "Unknown", sizeof(part_str));
	if (ixgbe_is_sfp(hw) && hw->phy.sfp_type != ixgbe_sfp_type_not_present)
		e_info(probe, "MAC: %d, PHY: %d, SFP+: %d, PBA No: %s\n",
		       hw->mac.type, hw->phy.type, hw->phy.sfp_type, part_str);
	else
		e_info(probe, "MAC: %d, PHY: %d, PBA No: %s\n",
		      hw->mac.type, hw->phy.type, part_str);

	if (((hw->bus.speed == ixgbe_bus_speed_2500) &&
	     (hw->bus.width <= ixgbe_bus_width_pcie_x4)) ||
	    (hw->bus.width <= ixgbe_bus_width_pcie_x2)) {
		e_dev_warn("PCI-Express bandwidth available for this "
			   "card is not sufficient for optimal "
			   "performance.\n");
		e_dev_warn("For optimal performance a x8 PCI-Express "
			   "slot is required.\n");
	}

#define INFO_STRING_LEN 255
	info_string = kzalloc(INFO_STRING_LEN, GFP_KERNEL);
	if (!info_string) {
		e_err(probe, "allocation for info string failed\n");
		goto no_info_string;
	}
	count = 0;
	i_s_var = info_string;
	count += snprintf(i_s_var, INFO_STRING_LEN, "Enabled Features: ");

	i_s_var = info_string + count;
	count += snprintf(i_s_var, (INFO_STRING_LEN - count),
			"RxQ: %d TxQ: %d ", adapter->num_rx_queues,
					adapter->num_tx_queues);
	i_s_var = info_string + count;
#ifdef IXGBE_FCOE
	if (adapter->flags & IXGBE_FLAG_FCOE_ENABLED) {
		count += snprintf(i_s_var, INFO_STRING_LEN - count, "FCoE ");
		i_s_var = info_string + count;
	}
#endif
	if (adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE) {
		count += snprintf(i_s_var, INFO_STRING_LEN - count,
							"FdirHash ");
		i_s_var = info_string + count;
	}
	if (adapter->flags & IXGBE_FLAG_FDIR_PERFECT_CAPABLE) {
		count += snprintf(i_s_var, INFO_STRING_LEN - count,
						"FdirPerfect ");
		i_s_var = info_string + count;
	}
	if (adapter->flags & IXGBE_FLAG_DCB_ENABLED) {
		count += snprintf(i_s_var, INFO_STRING_LEN - count, "DCB ");
		i_s_var = info_string + count;
	}
	if (adapter->flags & IXGBE_FLAG_RSS_ENABLED) {
		count += snprintf(i_s_var, INFO_STRING_LEN - count, "RSS ");
		i_s_var = info_string + count;
	}
	if (adapter->flags & IXGBE_FLAG_DCA_ENABLED) {
		count += snprintf(i_s_var, INFO_STRING_LEN - count, "DCA ");
		i_s_var = info_string + count;
	}
	if (adapter->flags2 & IXGBE_FLAG2_RSC_ENABLED) {
		count += snprintf(i_s_var, INFO_STRING_LEN - count, "RSC ");
		i_s_var = info_string + count;
	}
#ifndef IXGBE_NO_LRO
	else if (netdev->features & NETIF_F_LRO) {
		count += snprintf(i_s_var, INFO_STRING_LEN - count, "LRO ");
		i_s_var = info_string + count;
	}
#endif

	BUG_ON(i_s_var > (info_string + INFO_STRING_LEN));
	/* end features printing */
	e_info(probe, "%s\n", info_string);
	kfree(info_string);
no_info_string:

	/* firmware requires blank driver version */
	ixgbe_set_fw_drv_ver(hw, 0xFF, 0xFF, 0xFF, 0xFF);

#if defined(HAVE_NETDEV_STORAGE_ADDRESS) && defined(NETDEV_HW_ADDR_T_SAN)
	/* add san mac addr to netdev */
	//ixgbe_add_sanmac_netdev(netdev);

#endif /* (HAVE_NETDEV_STORAGE_ADDRESS) && (NETDEV_HW_ADDR_T_SAN) */
	e_info(probe, "Intel(R) 10 Gigabit Network Connection\n");
	cards_found++;

#ifdef IXGBE_SYSFS
	//if (ixgbe_sysfs_init(adapter))
	//	e_err(probe, "failed to allocate sysfs resources\n");
#else
#ifdef IXGBE_PROCFS
	//if (ixgbe_procfs_init(adapter))
	//	e_err(probe, "failed to allocate procfs resources\n");
#endif /* IXGBE_PROCFS */
#endif /* IXGBE_SYSFS */

	return 0;

//err_register:
	//ixgbe_clear_interrupt_scheme(adapter);
	//ixgbe_release_hw_control(adapter);
err_sw_init:
	adapter->flags2 &= ~IXGBE_FLAG2_SEARCH_FOR_SFP;
	if (adapter->mac_table)
		kfree(adapter->mac_table);
	iounmap(hw->hw_addr);
err_ioremap:
	free_netdev(netdev);
err_alloc_etherdev:
	//pci_release_selected_regions(pdev,
	//			     pci_select_bars(pdev, IORESOURCE_MEM));
//err_pci_reg:
//err_dma:
	pci_disable_device(pdev);
	return err;
}

/**
 * ixgbe_remove - Device Removal Routine
 * @pdev: PCI device information struct
 *
 * ixgbe_remove is called by the PCI subsystem to alert the driver
 * that it should release a PCI device.  The could be caused by a
 * Hot-Plug event, or because the driver is going to be removed from
 * memory.
 **/
void ixgbe_kni_remove(struct pci_dev *pdev)
{
	pci_disable_device(pdev);
}


u16 ixgbe_read_pci_cfg_word(struct ixgbe_hw *hw, u32 reg)
{
	u16 value;
	struct ixgbe_adapter *adapter = hw->back;

	pci_read_config_word(adapter->pdev, reg, &value);
	return value;
}

void ixgbe_write_pci_cfg_word(struct ixgbe_hw *hw, u32 reg, u16 value)
{
	struct ixgbe_adapter *adapter = hw->back;

	pci_write_config_word(adapter->pdev, reg, value);
}

void ewarn(struct ixgbe_hw *hw, const char *st, u32 status)
{
	struct ixgbe_adapter *adapter = hw->back;

	netif_warn(adapter, drv, adapter->netdev,  "%s", st);
}
