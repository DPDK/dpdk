/*******************************************************************************

  Intel(R) Gigabit Ethernet Linux driver
  Copyright(c) 2007-2013 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/


#include <linux/tcp.h>

#include "igb.h"
#include "igb_vmdq.h"
#include <linux/if_vlan.h>

#ifdef CONFIG_IGB_VMDQ_NETDEV
int igb_vmdq_open(struct net_device *dev)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(dev);
	struct igb_adapter *adapter = vadapter->real_adapter;
	struct net_device *main_netdev = adapter->netdev;
	int hw_queue = vadapter->rx_ring->queue_index +
		       adapter->vfs_allocated_count;

	if (test_bit(__IGB_DOWN, &adapter->state)) {
		DPRINTK(DRV, WARNING,
			"Open %s before opening this device.\n",
			main_netdev->name);
		return -EAGAIN;
	}
	netif_carrier_off(dev);
	vadapter->tx_ring->vmdq_netdev = dev;
	vadapter->rx_ring->vmdq_netdev = dev;
	if (is_valid_ether_addr(dev->dev_addr)) {
		igb_del_mac_filter(adapter, dev->dev_addr, hw_queue);
		igb_add_mac_filter(adapter, dev->dev_addr, hw_queue);
	}
	netif_carrier_on(dev);
	return 0;
}

int igb_vmdq_close(struct net_device *dev)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(dev);
	struct igb_adapter *adapter = vadapter->real_adapter;
	int hw_queue = vadapter->rx_ring->queue_index +
		       adapter->vfs_allocated_count;

	netif_carrier_off(dev);
	igb_del_mac_filter(adapter, dev->dev_addr, hw_queue);

	vadapter->tx_ring->vmdq_netdev = NULL;
	vadapter->rx_ring->vmdq_netdev = NULL;
	return 0;
}

netdev_tx_t igb_vmdq_xmit_frame(struct sk_buff *skb, struct net_device *dev)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(dev);

	return igb_xmit_frame_ring(skb, vadapter->tx_ring);
}

struct net_device_stats *igb_vmdq_get_stats(struct net_device *dev)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(dev);
        struct igb_adapter *adapter = vadapter->real_adapter;
        struct e1000_hw *hw = &adapter->hw;
	int hw_queue = vadapter->rx_ring->queue_index +
		       adapter->vfs_allocated_count;

	vadapter->net_stats.rx_packets +=
			E1000_READ_REG(hw, E1000_PFVFGPRC(hw_queue));
	E1000_WRITE_REG(hw, E1000_PFVFGPRC(hw_queue), 0);
        vadapter->net_stats.tx_packets +=
			E1000_READ_REG(hw, E1000_PFVFGPTC(hw_queue));
        E1000_WRITE_REG(hw, E1000_PFVFGPTC(hw_queue), 0);
        vadapter->net_stats.rx_bytes +=
			E1000_READ_REG(hw, E1000_PFVFGORC(hw_queue));
        E1000_WRITE_REG(hw, E1000_PFVFGORC(hw_queue), 0);
        vadapter->net_stats.tx_bytes +=
			E1000_READ_REG(hw, E1000_PFVFGOTC(hw_queue));
        E1000_WRITE_REG(hw, E1000_PFVFGOTC(hw_queue), 0);
        vadapter->net_stats.multicast +=
			E1000_READ_REG(hw, E1000_PFVFMPRC(hw_queue));
        E1000_WRITE_REG(hw, E1000_PFVFMPRC(hw_queue), 0);
	/* only return the current stats */
	return &vadapter->net_stats;
}

/**
 * igb_write_vm_addr_list - write unicast addresses to RAR table
 * @netdev: network interface device structure
 *
 * Writes unicast address list to the RAR table.
 * Returns: -ENOMEM on failure/insufficient address space
 *                0 on no addresses written
 *                X on writing X addresses to the RAR table
 **/
static int igb_write_vm_addr_list(struct net_device *netdev)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(netdev);
        struct igb_adapter *adapter = vadapter->real_adapter;
	int count = 0;
	int hw_queue = vadapter->rx_ring->queue_index +
		       adapter->vfs_allocated_count;

	/* return ENOMEM indicating insufficient memory for addresses */
	if (netdev_uc_count(netdev) > igb_available_rars(adapter))
		return -ENOMEM;

	if (!netdev_uc_empty(netdev)) {
#ifdef NETDEV_HW_ADDR_T_UNICAST
		struct netdev_hw_addr *ha;
#else
		struct dev_mc_list *ha;
#endif
		netdev_for_each_uc_addr(ha, netdev) {
#ifdef NETDEV_HW_ADDR_T_UNICAST
			igb_del_mac_filter(adapter, ha->addr, hw_queue);
			igb_add_mac_filter(adapter, ha->addr, hw_queue);
#else
			igb_del_mac_filter(adapter, ha->da_addr, hw_queue);
			igb_add_mac_filter(adapter, ha->da_addr, hw_queue);
#endif
			count++;
		}
	}
	return count;
}


#define E1000_VMOLR_UPE		0x20000000 /* Unicast promiscuous mode */
void igb_vmdq_set_rx_mode(struct net_device *dev)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(dev);
        struct igb_adapter *adapter = vadapter->real_adapter;
        struct e1000_hw *hw = &adapter->hw;
	u32 vmolr, rctl;
	int hw_queue = vadapter->rx_ring->queue_index +
		       adapter->vfs_allocated_count;

	/* Check for Promiscuous and All Multicast modes */
	vmolr = E1000_READ_REG(hw, E1000_VMOLR(hw_queue));

	/* clear the affected bits */
	vmolr &= ~(E1000_VMOLR_UPE | E1000_VMOLR_MPME |
		   E1000_VMOLR_ROPE | E1000_VMOLR_ROMPE);

	if (dev->flags & IFF_PROMISC) {
		vmolr |= E1000_VMOLR_UPE;
		rctl = E1000_READ_REG(hw, E1000_RCTL);
		rctl |= E1000_RCTL_UPE;
		E1000_WRITE_REG(hw, E1000_RCTL, rctl);
	} else {
		rctl = E1000_READ_REG(hw, E1000_RCTL);
		rctl &= ~E1000_RCTL_UPE;
		E1000_WRITE_REG(hw, E1000_RCTL, rctl);
		if (dev->flags & IFF_ALLMULTI) {
			vmolr |= E1000_VMOLR_MPME;
		} else {
			/*
			 * Write addresses to the MTA, if the attempt fails
			 * then we should just turn on promiscuous mode so
			 * that we can at least receive multicast traffic
			 */
			if (igb_write_mc_addr_list(adapter->netdev) != 0)
				vmolr |= E1000_VMOLR_ROMPE;
		}
#ifdef HAVE_SET_RX_MODE
		/*
		 * Write addresses to available RAR registers, if there is not
		 * sufficient space to store all the addresses then enable
		 * unicast promiscuous mode
		 */
		if (igb_write_vm_addr_list(dev) < 0)
			vmolr |= E1000_VMOLR_UPE;
#endif
	}
	E1000_WRITE_REG(hw, E1000_VMOLR(hw_queue), vmolr);

	return;
}

int igb_vmdq_set_mac(struct net_device *dev, void *p)
{
	struct sockaddr *addr = p;
	struct igb_vmdq_adapter *vadapter = netdev_priv(dev);
        struct igb_adapter *adapter = vadapter->real_adapter;
	int hw_queue = vadapter->rx_ring->queue_index +
		       adapter->vfs_allocated_count;

	igb_del_mac_filter(adapter, dev->dev_addr, hw_queue);
	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);
	return igb_add_mac_filter(adapter, dev->dev_addr, hw_queue);
}

int igb_vmdq_change_mtu(struct net_device *dev, int new_mtu)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(dev);
	struct igb_adapter *adapter = vadapter->real_adapter;

	if (adapter->netdev->mtu < new_mtu) {
		DPRINTK(PROBE, INFO,
			"Set MTU on %s to >= %d "
			"before changing MTU on %s\n",
			adapter->netdev->name, new_mtu, dev->name);
		return -EINVAL;
	}
	dev->mtu = new_mtu;
	return 0;
}

void igb_vmdq_tx_timeout(struct net_device *dev)
{
	return;
}

void igb_vmdq_vlan_rx_register(struct net_device *dev, struct vlan_group *grp)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(dev);
	struct igb_adapter *adapter = vadapter->real_adapter;
	struct e1000_hw *hw = &adapter->hw;
	int hw_queue = vadapter->rx_ring->queue_index +
		       adapter->vfs_allocated_count;

	vadapter->vlgrp = grp;

	igb_enable_vlan_tags(adapter);
	E1000_WRITE_REG(hw, E1000_VMVIR(hw_queue), 0);

	return;
}
void igb_vmdq_vlan_rx_add_vid(struct net_device *dev, unsigned short vid)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(dev);
	struct igb_adapter *adapter = vadapter->real_adapter;
#ifndef HAVE_NETDEV_VLAN_FEATURES
	struct net_device *v_netdev;
#endif
	int hw_queue = vadapter->rx_ring->queue_index +
		       adapter->vfs_allocated_count;

	/* attempt to add filter to vlvf array */
	igb_vlvf_set(adapter, vid, TRUE, hw_queue);

#ifndef HAVE_NETDEV_VLAN_FEATURES

	/* Copy feature flags from netdev to the vlan netdev for this vid.
	 * This allows things like TSO to bubble down to our vlan device.
	 */
	v_netdev = vlan_group_get_device(vadapter->vlgrp, vid);
	v_netdev->features |= adapter->netdev->features;
	vlan_group_set_device(vadapter->vlgrp, vid, v_netdev);
#endif

	return;
}
void igb_vmdq_vlan_rx_kill_vid(struct net_device *dev, unsigned short vid)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(dev);
	struct igb_adapter *adapter = vadapter->real_adapter;
	int hw_queue = vadapter->rx_ring->queue_index +
		       adapter->vfs_allocated_count;

	vlan_group_set_device(vadapter->vlgrp, vid, NULL);
	/* remove vlan from VLVF table array */
	igb_vlvf_set(adapter, vid, FALSE, hw_queue);


	return;
}

static int igb_vmdq_get_settings(struct net_device *netdev,
				   struct ethtool_cmd *ecmd)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(netdev);
	struct igb_adapter *adapter = vadapter->real_adapter;
	struct e1000_hw *hw = &adapter->hw;
	u32 status;

	if (hw->phy.media_type == e1000_media_type_copper) {

		ecmd->supported = (SUPPORTED_10baseT_Half |
				   SUPPORTED_10baseT_Full |
				   SUPPORTED_100baseT_Half |
				   SUPPORTED_100baseT_Full |
				   SUPPORTED_1000baseT_Full|
				   SUPPORTED_Autoneg |
				   SUPPORTED_TP);
		ecmd->advertising = ADVERTISED_TP;

		if (hw->mac.autoneg == 1) {
			ecmd->advertising |= ADVERTISED_Autoneg;
			/* the e1000 autoneg seems to match ethtool nicely */
			ecmd->advertising |= hw->phy.autoneg_advertised;
		}

		ecmd->port = PORT_TP;
		ecmd->phy_address = hw->phy.addr;
	} else {
		ecmd->supported   = (SUPPORTED_1000baseT_Full |
				     SUPPORTED_FIBRE |
				     SUPPORTED_Autoneg);

		ecmd->advertising = (ADVERTISED_1000baseT_Full |
				     ADVERTISED_FIBRE |
				     ADVERTISED_Autoneg);

		ecmd->port = PORT_FIBRE;
	}

	ecmd->transceiver = XCVR_INTERNAL;

	status = E1000_READ_REG(hw, E1000_STATUS);

	if (status & E1000_STATUS_LU) {

		if ((status & E1000_STATUS_SPEED_1000) ||
		    hw->phy.media_type != e1000_media_type_copper)
			ecmd->speed = SPEED_1000;
		else if (status & E1000_STATUS_SPEED_100)
			ecmd->speed = SPEED_100;
		else
			ecmd->speed = SPEED_10;

		if ((status & E1000_STATUS_FD) ||
		    hw->phy.media_type != e1000_media_type_copper)
			ecmd->duplex = DUPLEX_FULL;
		else
			ecmd->duplex = DUPLEX_HALF;
	} else {
		ecmd->speed = -1;
		ecmd->duplex = -1;
	}

	ecmd->autoneg = hw->mac.autoneg ? AUTONEG_ENABLE : AUTONEG_DISABLE;
	return 0;
}


static u32 igb_vmdq_get_msglevel(struct net_device *netdev)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(netdev);
	struct igb_adapter *adapter = vadapter->real_adapter;
	return adapter->msg_enable;
}

static void igb_vmdq_get_drvinfo(struct net_device *netdev,
				   struct ethtool_drvinfo *drvinfo)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(netdev);
	struct igb_adapter *adapter = vadapter->real_adapter;
	struct net_device *main_netdev = adapter->netdev;

	strncpy(drvinfo->driver, igb_driver_name, 32);
	strncpy(drvinfo->version, igb_driver_version, 32);

	strncpy(drvinfo->fw_version, "N/A", 4);
	snprintf(drvinfo->bus_info, 32, "%s VMDQ %d", main_netdev->name,
		 vadapter->rx_ring->queue_index);
	drvinfo->n_stats = 0;
	drvinfo->testinfo_len = 0;
	drvinfo->regdump_len = 0;
}

static void igb_vmdq_get_ringparam(struct net_device *netdev,
				     struct ethtool_ringparam *ring)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(netdev);

	struct igb_ring *tx_ring = vadapter->tx_ring;
	struct igb_ring *rx_ring = vadapter->rx_ring;

	ring->rx_max_pending = IGB_MAX_RXD;
	ring->tx_max_pending = IGB_MAX_TXD;
	ring->rx_mini_max_pending = 0;
	ring->rx_jumbo_max_pending = 0;
	ring->rx_pending = rx_ring->count;
	ring->tx_pending = tx_ring->count;
	ring->rx_mini_pending = 0;
	ring->rx_jumbo_pending = 0;
}
static u32 igb_vmdq_get_rx_csum(struct net_device *netdev)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(netdev);
	struct igb_adapter *adapter = vadapter->real_adapter;

	return test_bit(IGB_RING_FLAG_RX_CSUM, &adapter->rx_ring[0]->flags);
}


static struct ethtool_ops igb_vmdq_ethtool_ops = {
	.get_settings           = igb_vmdq_get_settings,
	.get_drvinfo            = igb_vmdq_get_drvinfo,
	.get_link               = ethtool_op_get_link,
	.get_ringparam          = igb_vmdq_get_ringparam,
	.get_rx_csum            = igb_vmdq_get_rx_csum,
	.get_tx_csum            = ethtool_op_get_tx_csum,
	.get_sg                 = ethtool_op_get_sg,
	.set_sg                 = ethtool_op_set_sg,
	.get_msglevel           = igb_vmdq_get_msglevel,
#ifdef NETIF_F_TSO
	.get_tso                = ethtool_op_get_tso,
#endif
#ifdef HAVE_ETHTOOL_GET_PERM_ADDR
	.get_perm_addr          = ethtool_op_get_perm_addr,
#endif
};

void igb_vmdq_set_ethtool_ops(struct net_device *netdev)
{
	SET_ETHTOOL_OPS(netdev, &igb_vmdq_ethtool_ops);
}


#endif /* CONFIG_IGB_VMDQ_NETDEV */

