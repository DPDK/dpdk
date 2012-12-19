/*******************************************************************************

  Intel(R) Gigabit Ethernet Linux driver
  Copyright(c) 2007-2012 Intel Corporation.

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

#include "igb.h"
#include "e1000_82575.h"
#include "e1000_hw.h"
#ifdef IGB_SYSFS
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/netdevice.h>

static struct net_device_stats *sysfs_get_stats(struct net_device *netdev)
{
#ifndef HAVE_NETDEV_STATS_IN_NETDEV
	struct igb_adapter *adapter;
#endif
	if (netdev == NULL)
		return NULL;

#ifdef HAVE_NETDEV_STATS_IN_NETDEV
	/* only return the current stats */
	return &netdev->stats;
#else
	adapter = netdev_priv(netdev);

	/* only return the current stats */
	return &adapter->net_stats;
#endif /* HAVE_NETDEV_STATS_IN_NETDEV */
}

struct net_device *igb_get_netdev(struct kobject *kobj)
{
	struct net_device *netdev;
	struct kobject *parent = kobj->parent;
	struct device *device_info_kobj;

	if (kobj == NULL)
	        return NULL;

	device_info_kobj = container_of(parent, struct device, kobj);
	if (device_info_kobj == NULL)
		return NULL;

	netdev = container_of(device_info_kobj, struct net_device, dev);
	return netdev;
}
struct igb_adapter *igb_get_adapter(struct kobject *kobj)
{
	struct igb_adapter *adapter;
	struct net_device *netdev = igb_get_netdev(kobj);
	if (netdev == NULL)
		return NULL;
	adapter = netdev_priv(netdev);
	return adapter;
}

bool igb_thermal_present(struct kobject *kobj)
{
	s32 status;
	struct igb_adapter *adapter = igb_get_adapter(kobj);

	if (adapter == NULL)
		return false;

	/*
	 * Only set I2C bit-bang mode if an external thermal sensor is
	 * supported on this device.
	 */
	if (adapter->ets) {
		status = e1000_set_i2c_bb(&(adapter->hw));
		if (status != E1000_SUCCESS)
			return false;
	}

	status = e1000_init_thermal_sensor_thresh(&(adapter->hw));
	if (status != E1000_SUCCESS)
		return false; 

	return true;
}

/*
 * Convert the directory to the sensor offset.
 *
 * Note: We know the name will be in the form of 'sensor_n' where 'n' is 0
 * - 'IGB_MAX_SENSORS'.  E1000_MAX_SENSORS < 10.  
 */
static int igb_name_to_idx(const char *c) {

	/* find first digit */
	while (*c < '0' || *c > '9') {
		if (*c == '\n')
			return -1;
		c++;
	}
	
	return ((int)(*c - '0'));
}

/* 
 * We are a statistics entry; we do not take in data-this should be the
 * same for all attributes
 */
static ssize_t igb_store(struct kobject *kobj,
			 struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	return -1;
}

static ssize_t igb_fwbanner(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	struct igb_adapter *adapter = igb_get_adapter(kobj);	
	u16 nvm_ver;

	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");
	nvm_ver = adapter->fw_version;

	return snprintf(buf, PAGE_SIZE, "0x%08x\n", nvm_ver);
}

static ssize_t igb_numeports(struct kobject *kobj,
			     struct kobj_attribute *attr, char *buf)
{
	struct e1000_hw *hw;
	int ports = 0;
	struct igb_adapter *adapter = igb_get_adapter(kobj);	
	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no hw data\n");

 	/* CMW taking the original out so and assigning ports generally
	 * by mac type for now.  Want to have the daemon handle this some
	 * other way due to the variability of the 1GB parts.
	 */
	switch (hw->mac.type) {
		case e1000_82575:
			ports = 2;
			break;
		case e1000_82576:
			ports = 2;
			break;
		case e1000_82580:
		case e1000_i350:
			ports = 4;
			break;
		default:
			break;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", ports);
}

static ssize_t igb_porttype(struct kobject *kobj,
		            struct kobj_attribute *attr, char *buf)
{
	struct igb_adapter *adapter = igb_get_adapter(kobj);	
	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");

	return snprintf(buf, PAGE_SIZE, "%d\n", 
			test_bit(__IGB_DOWN, &adapter->state));	
}

static ssize_t igb_portspeed(struct kobject *kobj,
			     struct kobj_attribute *attr, char *buf)
{
	struct igb_adapter *adapter = igb_get_adapter(kobj);
	int speed = 0;

	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");
	
	switch (adapter->link_speed) {
	case E1000_STATUS_SPEED_10:
		speed = 10;
		break;;
	case E1000_STATUS_SPEED_100:
		speed = 100;
		break;
	case E1000_STATUS_SPEED_1000:
		speed = 1000;
		break;
	}	
	return snprintf(buf, PAGE_SIZE, "%d\n", speed);
}

static ssize_t igb_wqlflag(struct kobject *kobj,
			   struct kobj_attribute *attr, char *buf)
{
	struct igb_adapter *adapter = igb_get_adapter(kobj);	
	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");

	return snprintf(buf, PAGE_SIZE, "%d\n", adapter->wol);
}

static ssize_t igb_xflowctl(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	struct e1000_hw *hw;
	struct igb_adapter *adapter = igb_get_adapter(kobj);	
	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no hw data\n");

	return snprintf(buf, PAGE_SIZE, "%d\n", hw->fc.current_mode);
}

static ssize_t igb_rxdrops(struct kobject *kobj,
			   struct kobj_attribute *attr, char *buf)
{
	struct net_device_stats *net_stats;
	struct net_device *netdev = igb_get_netdev(kobj);	
	if (netdev == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no net device\n");

	net_stats  = sysfs_get_stats(netdev);
	if (net_stats == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no net stats\n");

	return snprintf(buf, PAGE_SIZE, "%lu\n", 
			net_stats->rx_dropped);
}

static ssize_t igb_rxerrors(struct kobject *kobj,
                            struct kobj_attribute *attr, char *buf)
{
	struct net_device_stats *net_stats;
	struct net_device *netdev = igb_get_netdev(kobj);	
	if (netdev == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no net device\n");

	net_stats  = sysfs_get_stats(netdev);
	if (net_stats == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no net stats\n");
	return snprintf(buf, PAGE_SIZE, "%lu\n", net_stats->rx_errors);
}

static ssize_t igb_rxupacks(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	struct e1000_hw *hw;
	struct igb_adapter *adapter = igb_get_adapter(kobj);	
	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no hw data\n");

	return snprintf(buf, PAGE_SIZE, "%d\n", E1000_READ_REG(hw, E1000_TPR));
}

static ssize_t igb_rxmpacks(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	struct e1000_hw *hw;
	struct igb_adapter *adapter = igb_get_adapter(kobj);	
	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no hw data\n");

	return snprintf(buf, PAGE_SIZE, "%d\n", E1000_READ_REG(hw, E1000_MPRC));
}

static ssize_t igb_rxbpacks(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	struct e1000_hw *hw;
	struct igb_adapter *adapter = igb_get_adapter(kobj);	
	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no hw data\n");

	return snprintf(buf, PAGE_SIZE, "%d\n", E1000_READ_REG(hw, E1000_BPRC));
}

static ssize_t igb_txupacks(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	struct e1000_hw *hw;
	struct igb_adapter *adapter = igb_get_adapter(kobj);	
	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no hw data\n");

	return snprintf(buf, PAGE_SIZE, "%d\n", E1000_READ_REG(hw, E1000_TPT));
}

static ssize_t igb_txmpacks(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	struct e1000_hw *hw;
	struct igb_adapter *adapter = igb_get_adapter(kobj);	
	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no hw data\n");

	return snprintf(buf, PAGE_SIZE, "%d\n", E1000_READ_REG(hw, E1000_MPTC));
}

static ssize_t igb_txbpacks(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	struct e1000_hw *hw;
	struct igb_adapter *adapter = igb_get_adapter(kobj);	
	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no hw data\n");

	return snprintf(buf, PAGE_SIZE, "%d\n", E1000_READ_REG(hw, E1000_BPTC));

}

static ssize_t igb_txerrors(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	struct net_device_stats *net_stats;
	struct net_device *netdev = igb_get_netdev(kobj);	
	if (netdev == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no net device\n");

	net_stats  = sysfs_get_stats(netdev);
	if (net_stats == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no net stats\n");

	return snprintf(buf, PAGE_SIZE, "%lu\n", 
			net_stats->tx_errors);
}

static ssize_t igb_txdrops(struct kobject *kobj,
			   struct kobj_attribute *attr, char *buf)
{
	struct net_device_stats *net_stats;
	struct net_device *netdev = igb_get_netdev(kobj);	
	if (netdev == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no net device\n");

	net_stats  = sysfs_get_stats(netdev);
	if (net_stats == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no net stats\n");
	return snprintf(buf, PAGE_SIZE, "%lu\n", 
			net_stats->tx_dropped);
}

static ssize_t igb_rxframes(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	struct net_device_stats *net_stats;
	struct net_device *netdev = igb_get_netdev(kobj);	
	if (netdev == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no net device\n");

	net_stats  = sysfs_get_stats(netdev);
	if (net_stats == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no net stats\n");

	return snprintf(buf, PAGE_SIZE, "%lu\n", 
			net_stats->rx_packets);
}

static ssize_t igb_rxbytes(struct kobject *kobj,
			   struct kobj_attribute *attr, char *buf)
{
	struct net_device_stats *net_stats;
	struct net_device *netdev = igb_get_netdev(kobj);	
	if (netdev == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no net device\n");

	net_stats  = sysfs_get_stats(netdev);
	if (net_stats == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no net stats\n");

	return snprintf(buf, PAGE_SIZE, "%lu\n", 
			net_stats->rx_bytes);
}

static ssize_t igb_txframes(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	struct net_device_stats *net_stats;
	struct net_device *netdev = igb_get_netdev(kobj);	
	if (netdev == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no net device\n");

	net_stats  = sysfs_get_stats(netdev);
	if (net_stats == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no net stats\n");

	return snprintf(buf, PAGE_SIZE, "%lu\n", 
			net_stats->tx_packets);
}

static ssize_t igb_txbytes(struct kobject *kobj,
			   struct kobj_attribute *attr, char *buf)
{
	struct net_device_stats *net_stats;
	struct net_device *netdev = igb_get_netdev(kobj);	
	if (netdev == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no net device\n");

	net_stats  = sysfs_get_stats(netdev);
	if (net_stats == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no net stats\n");

	return snprintf(buf, PAGE_SIZE, "%lu\n", 
			net_stats->tx_bytes);
}

static ssize_t igb_linkstat(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	bool link_up = false;
	int bitmask = 0;
	struct e1000_hw *hw;
	struct igb_adapter *adapter = igb_get_adapter(kobj);
	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no hw data\n");

	if (test_bit(__IGB_DOWN, &adapter->state)) 
		bitmask |= 1;

	if (hw->mac.ops.check_for_link) {
		hw->mac.ops.check_for_link(hw);
	}
	else {
		/* always assume link is up, if no check link function */
		link_up = true;
	}
	if (link_up)
		bitmask |= 2;
	return snprintf(buf, PAGE_SIZE, "0x%X\n", bitmask);
}

static ssize_t igb_funcid(struct kobject *kobj,
			  struct kobj_attribute *attr, char *buf)
{
	struct net_device *netdev = igb_get_netdev(kobj);	
	if (netdev == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no net device\n");

	return snprintf(buf, PAGE_SIZE, "0x%lX\n", netdev->base_addr);
}

static ssize_t igb_funcvers(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", igb_driver_version);
}

static ssize_t igb_macburn(struct kobject *kobj,
			   struct kobj_attribute *attr, char *buf)
{
	struct e1000_hw *hw;
	struct igb_adapter *adapter = igb_get_adapter(kobj);
	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no hw data\n");

	return snprintf(buf, PAGE_SIZE, "0x%X%X%X%X%X%X\n",
		       (unsigned int)hw->mac.perm_addr[0],
		       (unsigned int)hw->mac.perm_addr[1],
		       (unsigned int)hw->mac.perm_addr[2],
		       (unsigned int)hw->mac.perm_addr[3],
		       (unsigned int)hw->mac.perm_addr[4],
		       (unsigned int)hw->mac.perm_addr[5]);
}

static ssize_t igb_macadmn(struct kobject *kobj,
			   struct kobj_attribute *attr, char *buf)
{
	struct igb_adapter *adapter = igb_get_adapter(kobj);
	struct e1000_hw *hw;

	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no hw data\n");

	return snprintf(buf, PAGE_SIZE, "0x%X%X%X%X%X%X\n",
		       (unsigned int)hw->mac.addr[0],
		       (unsigned int)hw->mac.addr[1],
		       (unsigned int)hw->mac.addr[2],
		       (unsigned int)hw->mac.addr[3],
		       (unsigned int)hw->mac.addr[4],
		       (unsigned int)hw->mac.addr[5]);
}

static ssize_t igb_maclla1(struct kobject *kobj,
			   struct kobj_attribute *attr, char *buf)
{
	struct e1000_hw *hw;
	u16 eeprom_buff[6];
	int first_word = 0x37;
	int word_count = 6;
	int rc;

	struct igb_adapter *adapter = igb_get_adapter(kobj);
	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no hw data\n");

	return 0;

	rc = e1000_read_nvm(hw, first_word, word_count, 
					   eeprom_buff);
	if (rc != E1000_SUCCESS)
		return 0;

	switch (hw->bus.func) {
	case 0:
		return snprintf(buf, PAGE_SIZE, "0x%04X%04X%04X\n", 
				eeprom_buff[0], eeprom_buff[1], eeprom_buff[2]);
	case 1:
		return snprintf(buf, PAGE_SIZE, "0x%04X%04X%04X\n", 
				eeprom_buff[3], eeprom_buff[4], eeprom_buff[5]);
	}
	return snprintf(buf, PAGE_SIZE, "unexpected port %d\n", hw->bus.func);
}

static ssize_t igb_mtusize(struct kobject *kobj,
			      struct kobj_attribute *attr, char *buf)
{
	struct igb_adapter *adapter = igb_get_adapter(kobj);	
	struct net_device *netdev = igb_get_netdev(kobj);	
	if (netdev == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no net device\n");

	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");

	return snprintf(buf, PAGE_SIZE, "%d\n", netdev->mtu);
}

static ssize_t igb_featflag(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
#ifdef HAVE_NDO_SET_FEATURES
	int bitmask = 0;
#endif
	struct net_device *netdev = igb_get_netdev(kobj);	
	struct igb_adapter *adapter = igb_get_adapter(kobj);

	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");
	if (netdev == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no net device\n");

#ifndef HAVE_NDO_SET_FEATURES
	/* igb_get_rx_csum(netdev) doesn't compile so hard code */
	return snprintf(buf, PAGE_SIZE, "%d\n", 
			test_bit(IGB_RING_FLAG_RX_CSUM, 
				 &adapter->rx_ring[0]->flags));
#else
	if (netdev->features & NETIF_F_RXCSUM)
		bitmask |= 1;
	return snprintf(buf, PAGE_SIZE, "%d\n", bitmask);
#endif
}

static ssize_t igb_lsominct(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 1);
}

static ssize_t igb_prommode(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	struct net_device *netdev = igb_get_netdev(kobj);	
	if (netdev == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no net device\n");

	return snprintf(buf, PAGE_SIZE, "%d\n", 
			netdev->flags & IFF_PROMISC);
}

static ssize_t igb_txdscqsz(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	struct igb_adapter *adapter = igb_get_adapter(kobj);
	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");

	return snprintf(buf, PAGE_SIZE, "%d\n", adapter->tx_ring[0]->count);
}

static ssize_t igb_rxdscqsz(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	struct igb_adapter *adapter = igb_get_adapter(kobj);
	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");

	return snprintf(buf, PAGE_SIZE, "%d\n", adapter->rx_ring[0]->count);
}

static ssize_t igb_rxqavg(struct kobject *kobj,
			  struct kobj_attribute *attr, char *buf)
{
	int index;
	int diff = 0;
	u16 ntc;
	u16 ntu;
	struct igb_adapter *adapter = igb_get_adapter(kobj);
	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");
	
	for (index = 0; index < adapter->num_rx_queues; index++) {
		ntc = adapter->rx_ring[index]->next_to_clean;
		ntu = adapter->rx_ring[index]->next_to_use;

		if (ntc >= ntu)
			diff += (ntc - ntu);
		else
			diff += (adapter->rx_ring[index]->count - ntu + ntc);
	}
	if (adapter->num_rx_queues <= 0)
		return snprintf(buf, PAGE_SIZE, 
				"can't calculate, number of queues %d\n", 
				adapter->num_rx_queues);		
	return snprintf(buf, PAGE_SIZE, "%d\n", diff/adapter->num_rx_queues);
}

static ssize_t igb_txqavg(struct kobject *kobj,
			  struct kobj_attribute *attr, char *buf)
{
	int index;
	int diff = 0;
	u16 ntc;
	u16 ntu;
	struct igb_adapter *adapter = igb_get_adapter(kobj);
	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");

	for (index = 0; index < adapter->num_tx_queues; index++) {
		ntc = adapter->tx_ring[index]->next_to_clean;
		ntu = adapter->tx_ring[index]->next_to_use;

		if (ntc >= ntu)
			diff += (ntc - ntu);
		else
			diff += (adapter->tx_ring[index]->count - ntu + ntc);
	}
	if (adapter->num_tx_queues <= 0)
		return snprintf(buf, PAGE_SIZE, 
				"can't calculate, number of queues %d\n", 
				adapter->num_tx_queues);		
	return snprintf(buf, PAGE_SIZE, "%d\n", 
			diff/adapter->num_tx_queues);
}

static ssize_t igb_iovotype(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "2\n");
}

static ssize_t igb_funcnbr(struct kobject *kobj,
			   struct kobj_attribute *attr, char *buf)
{
	struct igb_adapter *adapter = igb_get_adapter(kobj);
	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");

	return snprintf(buf, PAGE_SIZE, "%d\n", adapter->vfs_allocated_count);
}

s32 igb_sysfs_get_thermal_data(struct kobject *kobj, char *buf)
{
	s32 status;
	struct igb_adapter *adapter = igb_get_adapter(kobj->parent);

	if (adapter == NULL) {
		snprintf(buf, PAGE_SIZE, "error: missing adapter\n");
		return 0;
	}

	if (&adapter->hw == NULL) {
		snprintf(buf, PAGE_SIZE, "error: missing hw\n");
		return 0;
	}

	status = e1000_get_thermal_sensor_data_generic(&(adapter->hw));
	if (status != E1000_SUCCESS)
		snprintf(buf, PAGE_SIZE, "error: status %d returned\n", status);

	return status;
}

static ssize_t igb_sysfs_location(struct kobject *kobj, 
				     struct kobj_attribute *attr,
				     char *buf) 
{
	struct igb_adapter *adapter = igb_get_adapter(kobj->parent);
	int idx;

	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");

	idx = igb_name_to_idx(kobj->name);
	if (idx == -1)
		return snprintf(buf, PAGE_SIZE,
			"error: invalid sensor name %s\n", kobj->name);

	return snprintf(buf, PAGE_SIZE, "%d\n", 
			adapter->hw.mac.thermal_sensor_data.sensor[idx].location);
}

static ssize_t igb_sysfs_temp(struct kobject *kobj,
				 struct kobj_attribute *attr,
				 char *buf)
{
	struct igb_adapter *adapter = igb_get_adapter(kobj->parent);
	int idx;

	s32 status = igb_sysfs_get_thermal_data(kobj, buf);

	if (status != E1000_SUCCESS)
	        return snprintf(buf, PAGE_SIZE, "error: status %d returned", 
				status);

	idx = igb_name_to_idx(kobj->name);
	if (idx == -1)
		return snprintf(buf, PAGE_SIZE,
			"error: invalid sensor name %s\n", kobj->name);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			adapter->hw.mac.thermal_sensor_data.sensor[idx].temp);
}

static ssize_t igb_sysfs_maxopthresh(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf) 
{
	struct igb_adapter *adapter = igb_get_adapter(kobj->parent);
	int idx;
	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");

	idx = igb_name_to_idx(kobj->name);
	if (idx == -1)
		return snprintf(buf, PAGE_SIZE,
			"error: invalid sensor name %s\n", kobj->name);

	return snprintf(buf, PAGE_SIZE, "%d\n", 
		adapter->hw.mac.thermal_sensor_data.sensor[idx].max_op_thresh);
}

static ssize_t igb_sysfs_cautionthresh(struct kobject *kobj,
					  struct kobj_attribute *attr,
					  char *buf) 
{
	struct igb_adapter *adapter = igb_get_adapter(kobj->parent);
	int idx;

	if (adapter == NULL)
		return snprintf(buf, PAGE_SIZE, "error: no adapter\n");

	idx = igb_name_to_idx(kobj->name);
	if (idx == -1)
		return snprintf(buf, PAGE_SIZE,
			"error: invalid sensor name %s\n", kobj->name);

	return snprintf(buf, PAGE_SIZE, "%d\n", 
		adapter->hw.mac.thermal_sensor_data.sensor[0].caution_thresh);
}

/* Initialize the attributes */
static struct kobj_attribute igb_sysfs_location_attr = 
	__ATTR(location, 0444, igb_sysfs_location, igb_store);
static struct kobj_attribute igb_sysfs_temp_attr = 
	__ATTR(temp, 0444, igb_sysfs_temp, igb_store);
static struct kobj_attribute igb_sysfs_cautionthresh_attr = 
	__ATTR(cautionthresh, 0444, igb_sysfs_cautionthresh, igb_store);
static struct kobj_attribute igb_sysfs_maxopthresh_attr = 
	__ATTR(maxopthresh, 0444, igb_sysfs_maxopthresh, igb_store);

static struct kobj_attribute igb_sysfs_fwbanner_attr = 
	__ATTR(fwbanner, 0444, igb_fwbanner, igb_store);
static struct kobj_attribute igb_sysfs_numeports_attr = 
	__ATTR(numeports, 0444, igb_numeports, igb_store);
static struct kobj_attribute igb_sysfs_porttype_attr = 
	__ATTR(porttype, 0444, igb_porttype, igb_store);
static struct kobj_attribute igb_sysfs_portspeed_attr = 
	__ATTR(portspeed, 0444, igb_portspeed, igb_store);
static struct kobj_attribute igb_sysfs_wqlflag_attr = 
	__ATTR(wqlflag, 0444, igb_wqlflag, igb_store);
static struct kobj_attribute igb_sysfs_xflowctl_attr = 
	__ATTR(xflowctl, 0444, igb_xflowctl, igb_store);
static struct kobj_attribute igb_sysfs_rxdrops_attr = 
	__ATTR(rxdrops, 0444, igb_rxdrops, igb_store);
static struct kobj_attribute igb_sysfs_rxerrors_attr = 
	__ATTR(rxerrors, 0444, igb_rxerrors, igb_store);
static struct kobj_attribute igb_sysfs_rxupacks_attr = 
	__ATTR(rxupacks, 0444, igb_rxupacks, igb_store);
static struct kobj_attribute igb_sysfs_rxmpacks_attr = 
	__ATTR(rxmpacks, 0444, igb_rxmpacks, igb_store);
static struct kobj_attribute igb_sysfs_rxbpacks_attr = 
	__ATTR(rxbpacks, 0444, igb_rxbpacks, igb_store);
static struct kobj_attribute igb_sysfs_txupacks_attr = 
	__ATTR(txupacks, 0444, igb_txupacks, igb_store);
static struct kobj_attribute igb_sysfs_txmpacks_attr = 
	__ATTR(txmpacks, 0444, igb_txmpacks, igb_store);
static struct kobj_attribute igb_sysfs_txbpacks_attr = 
	__ATTR(txbpacks, 0444, igb_txbpacks, igb_store);
static struct kobj_attribute igb_sysfs_txerrors_attr = 
	__ATTR(txerrors, 0444, igb_txerrors, igb_store);
static struct kobj_attribute igb_sysfs_txdrops_attr = 
	__ATTR(txdrops, 0444, igb_txdrops, igb_store);
static struct kobj_attribute igb_sysfs_rxframes_attr = 
	__ATTR(rxframes, 0444, igb_rxframes, igb_store);
static struct kobj_attribute igb_sysfs_rxbytes_attr = 
	__ATTR(rxbytes, 0444, igb_rxbytes, igb_store);
static struct kobj_attribute igb_sysfs_txframes_attr = 
	__ATTR(txframes, 0444, igb_txframes, igb_store);
static struct kobj_attribute igb_sysfs_txbytes_attr = 
	__ATTR(txbytes, 0444, igb_txbytes, igb_store);
static struct kobj_attribute igb_sysfs_linkstat_attr = 
	__ATTR(linkstat, 0444, igb_linkstat, igb_store);
static struct kobj_attribute igb_sysfs_funcid_attr = 
	__ATTR(funcid, 0444, igb_funcid, igb_store);
static struct kobj_attribute igb_sysfs_funvers_attr = 
	__ATTR(funcvers, 0444, igb_funcvers, igb_store);
static struct kobj_attribute igb_sysfs_macburn_attr = 
	__ATTR(macburn, 0444, igb_macburn, igb_store);
static struct kobj_attribute igb_sysfs_macadmn_attr = 
	__ATTR(macadmn, 0444, igb_macadmn, igb_store);
static struct kobj_attribute igb_sysfs_maclla1_attr = 
	__ATTR(maclla1, 0444, igb_maclla1, igb_store);
static struct kobj_attribute igb_sysfs_mtusize_attr = 
	__ATTR(mtusize, 0444, igb_mtusize, igb_store);
static struct kobj_attribute igb_sysfs_featflag_attr = 
	__ATTR(featflag, 0444, igb_featflag, igb_store);
static struct kobj_attribute igb_sysfs_lsominct_attr = 
	__ATTR(lsominct, 0444, igb_lsominct, igb_store);
static struct kobj_attribute igb_sysfs_prommode_attr = 
	__ATTR(prommode, 0444, igb_prommode, igb_store);
static struct kobj_attribute igb_sysfs_txdscqsz_attr = 
	__ATTR(txdscqsz, 0444, igb_txdscqsz, igb_store);
static struct kobj_attribute igb_sysfs_rxdscqsz_attr = 
	__ATTR(rxdscqsz, 0444, igb_rxdscqsz, igb_store);
static struct kobj_attribute igb_sysfs_txqavg_attr = 
	__ATTR(txqavg, 0444, igb_txqavg, igb_store);
static struct kobj_attribute igb_sysfs_rxqavg_attr = 
	__ATTR(rxqavg, 0444, igb_rxqavg, igb_store);
static struct kobj_attribute igb_sysfs_iovotype_attr = 
	__ATTR(iovotype, 0444, igb_iovotype, igb_store);
static struct kobj_attribute igb_sysfs_funcnbr_attr = 
	__ATTR(funcnbr, 0444, igb_funcnbr, igb_store);

/* Add the attributes into an array, to be added to a group */
static struct attribute *therm_attrs[] = {
	&igb_sysfs_location_attr.attr,
	&igb_sysfs_temp_attr.attr,
	&igb_sysfs_cautionthresh_attr.attr,
	&igb_sysfs_maxopthresh_attr.attr,
	NULL
};

static struct attribute *attrs[] = {
	&igb_sysfs_fwbanner_attr.attr,
	&igb_sysfs_numeports_attr.attr,
	&igb_sysfs_porttype_attr.attr,
	&igb_sysfs_portspeed_attr.attr,
	&igb_sysfs_wqlflag_attr.attr,
	&igb_sysfs_xflowctl_attr.attr,
	&igb_sysfs_rxdrops_attr.attr,
	&igb_sysfs_rxerrors_attr.attr,
	&igb_sysfs_rxupacks_attr.attr,
	&igb_sysfs_rxmpacks_attr.attr,
	&igb_sysfs_rxbpacks_attr.attr,
	&igb_sysfs_txdrops_attr.attr,
	&igb_sysfs_txerrors_attr.attr,
	&igb_sysfs_txupacks_attr.attr,
	&igb_sysfs_txmpacks_attr.attr,
	&igb_sysfs_txbpacks_attr.attr,
	&igb_sysfs_rxframes_attr.attr,
	&igb_sysfs_rxbytes_attr.attr,
	&igb_sysfs_txframes_attr.attr,
	&igb_sysfs_txbytes_attr.attr,
	&igb_sysfs_linkstat_attr.attr,
	&igb_sysfs_funcid_attr.attr,
	&igb_sysfs_funvers_attr.attr,
	&igb_sysfs_macburn_attr.attr,
	&igb_sysfs_macadmn_attr.attr,
	&igb_sysfs_maclla1_attr.attr,
	&igb_sysfs_mtusize_attr.attr,
	&igb_sysfs_featflag_attr.attr,
	&igb_sysfs_lsominct_attr.attr,
	&igb_sysfs_prommode_attr.attr,
	&igb_sysfs_txdscqsz_attr.attr,
	&igb_sysfs_rxdscqsz_attr.attr,
	&igb_sysfs_txqavg_attr.attr,
	&igb_sysfs_rxqavg_attr.attr,
	&igb_sysfs_iovotype_attr.attr,
	&igb_sysfs_funcnbr_attr.attr,
	NULL
};

/* add attributes to a group */
static struct attribute_group therm_attr_group = {
	.attrs = therm_attrs,
};

/* add attributes to a group */
static struct attribute_group attr_group = {
	.attrs = attrs,
};

void igb_del_adapter(struct igb_adapter *adapter)
{
	int i;

	for (i = 0; i < E1000_MAX_SENSORS; i++) {
		if (adapter->therm_kobj[i] == NULL)
			continue;
		sysfs_remove_group(adapter->therm_kobj[i], &therm_attr_group);
		kobject_put(adapter->therm_kobj[i]);
	}
	if (adapter->info_kobj != NULL) {
		sysfs_remove_group(adapter->info_kobj, &attr_group);
		kobject_put(adapter->info_kobj);
	}
}

/* cleanup goes here */
void igb_sysfs_exit(struct igb_adapter *adapter) 
{
	igb_del_adapter(adapter);
}

int igb_sysfs_init(struct igb_adapter *adapter) 
{
	struct net_device *netdev;
	int rc = 0;
	int i;
	char buf[16];

	if ( adapter == NULL )
		goto del_adapter;
	netdev = adapter->netdev;	
	if (netdev == NULL)
		goto del_adapter;

	adapter->info_kobj = NULL;
	for (i = 0; i < E1000_MAX_SENSORS; i++)
		adapter->therm_kobj[i] = NULL;
	
	/* create stats kobj and attribute listings in kobj */
	adapter->info_kobj = kobject_create_and_add("info", 
					&(netdev->dev.kobj));
	if (adapter->info_kobj == NULL)
		goto del_adapter;
	if (sysfs_create_group(adapter->info_kobj, &attr_group))
		goto del_adapter;

	/* Don't create thermal subkobjs if no data present */
	if (igb_thermal_present(adapter->info_kobj) != true)
		goto exit;

	for (i = 0; i < E1000_MAX_SENSORS; i++) {

		/*
		 * Likewise only create individual kobjs that have
		 * meaningful data.
		 */
		if (adapter->hw.mac.thermal_sensor_data.sensor[i].location == 0)
			continue;
		
		/* directory named after sensor offset */
		snprintf(buf, sizeof(buf), "sensor_%d", i);
		adapter->therm_kobj[i] = 
			kobject_create_and_add(buf, adapter->info_kobj);
		if (adapter->therm_kobj[i] == NULL)
			goto del_adapter;
		if (sysfs_create_group(adapter->therm_kobj[i],
				       &therm_attr_group))
			goto del_adapter;
	}

	goto exit;

del_adapter:
	igb_del_adapter(adapter);
	rc = -1;
exit:
	return rc;
}

#endif /* IGB_SYSFS */
