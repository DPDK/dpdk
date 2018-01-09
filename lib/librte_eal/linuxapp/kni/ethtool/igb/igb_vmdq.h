/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************

  Intel(R) Gigabit Ethernet Linux driver
  Copyright(c) 2007-2013 Intel Corporation.

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#ifndef _IGB_VMDQ_H_
#define _IGB_VMDQ_H_

#ifdef CONFIG_IGB_VMDQ_NETDEV
int igb_vmdq_open(struct net_device *dev);
int igb_vmdq_close(struct net_device *dev);
netdev_tx_t igb_vmdq_xmit_frame(struct sk_buff *skb, struct net_device *dev);
struct net_device_stats *igb_vmdq_get_stats(struct net_device *dev);
void igb_vmdq_set_rx_mode(struct net_device *dev);
int igb_vmdq_set_mac(struct net_device *dev, void *addr);
int igb_vmdq_change_mtu(struct net_device *dev, int new_mtu);
void igb_vmdq_tx_timeout(struct net_device *dev);
void igb_vmdq_vlan_rx_register(struct net_device *dev,
				 struct vlan_group *grp);
void igb_vmdq_vlan_rx_add_vid(struct net_device *dev, unsigned short vid);
void igb_vmdq_vlan_rx_kill_vid(struct net_device *dev, unsigned short vid);
void igb_vmdq_set_ethtool_ops(struct net_device *netdev);
#endif /* CONFIG_IGB_VMDQ_NETDEV */
#endif /* _IGB_VMDQ_H_ */
