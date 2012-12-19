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

#ifdef IGB_PROCFS
#ifndef IGB_SYSFS

#include <linux/module.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/netdevice.h>

static struct proc_dir_entry *igb_top_dir = NULL;

static struct net_device_stats *procfs_get_stats(struct net_device *netdev)
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

bool igb_thermal_present(struct igb_adapter *adapter)
{
	s32 status;
	struct e1000_hw *hw;

	if (adapter == NULL)
		return false;
	hw = &adapter->hw;

	/*
	 * Only set I2C bit-bang mode if an external thermal sensor is
	 * supported on this device.
	 */
	if (adapter->ets) {
		status = e1000_set_i2c_bb(hw);
		if (status != E1000_SUCCESS)
			return false;
	}

	status = hw->mac.ops.init_thermal_sensor_thresh(hw);
	if (status != E1000_SUCCESS)
		return false;
	
	return true;
}

static int igb_fwbanner(char *page, char **start, off_t off, int count, 
			 int *eof, void *data)
{
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");

	return snprintf(page, count, "%d.%d-%d\n", 
			(adapter->fw_version & 0xF000) >> 12,
			(adapter->fw_version & 0x0FF0) >> 4,
			adapter->fw_version & 0x000F);
}

static int igb_numeports(char *page, char **start, off_t off, int count, 
			  int *eof, void *data)
{
	struct e1000_hw *hw;
	int ports;
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(page, count, "error: no hw data\n");

	ports = 4;

	return snprintf(page, count, "%d\n", ports);
}

static int igb_porttype(char *page, char **start, off_t off, int count, 
			 int *eof, void *data)
{
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");

	return snprintf(page, count, "%d\n", 
			test_bit(__IGB_DOWN, &adapter->state));	
}

static int igb_portspeed(char *page, char **start, off_t off, 
			 int count, int *eof, void *data)
{
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	int speed = 0;	
	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");
	
	switch (adapter->link_speed) {
	case E1000_STATUS_SPEED_10:
		speed = 10;
		break;
	case E1000_STATUS_SPEED_100:
		speed = 100;
		break;
	case E1000_STATUS_SPEED_1000:
		speed = 1000;
		break;
	}	
	return snprintf(page, count, "%d\n", speed);
}

static int igb_wqlflag(char *page, char **start, off_t off, int count, 
			int *eof, void *data)
{
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");

	return snprintf(page, count, "%d\n", adapter->wol);
}

static int igb_xflowctl(char *page, char **start, off_t off, int count, 
			 int *eof, void *data)
{
	struct e1000_hw *hw;
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(page, count, "error: no hw data\n");

	return snprintf(page, count, "%d\n", hw->fc.current_mode);
}

static int igb_rxdrops(char *page, char **start, off_t off, int count, 
			int *eof, void *data)
{
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	struct net_device_stats *net_stats;

	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");
	net_stats  = procfs_get_stats(adapter->netdev);
	if (net_stats == NULL)
		return snprintf(page, count, "error: no net stats\n");

	return snprintf(page, count, "%lu\n", 
			net_stats->rx_dropped);
}

static int igb_rxerrors(char *page, char **start, off_t off, int count, 
			 int *eof, void *data)
{
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	struct net_device_stats *net_stats;

	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");
	net_stats  = procfs_get_stats(adapter->netdev);
	if (net_stats == NULL)
		return snprintf(page, count, "error: no net stats\n");

	return snprintf(page, count, "%lu\n", net_stats->rx_errors);
}

static int igb_rxupacks(char *page, char **start, off_t off, int count, 
			 int *eof, void *data)
{
	struct e1000_hw *hw;
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(page, count, "error: no hw data\n");

	return snprintf(page, count, "%d\n", E1000_READ_REG(hw, E1000_TPR));
}

static int igb_rxmpacks(char *page, char **start, off_t off, int count, 
			 int *eof, void *data)
{
	struct e1000_hw *hw;
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(page, count, "error: no hw data\n");

	return snprintf(page, count, "%d\n", 
			E1000_READ_REG(hw, E1000_MPRC));
}

static int igb_rxbpacks(char *page, char **start, off_t off, int count, 
			 int *eof, void *data)
{
	struct e1000_hw *hw;
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(page, count, "error: no hw data\n");

	return snprintf(page, count, "%d\n", 
			E1000_READ_REG(hw, E1000_BPRC));
}

static int igb_txupacks(char *page, char **start, off_t off, int count, 
			 int *eof, void *data)
{
	struct e1000_hw *hw;
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(page, count, "error: no hw data\n");

	return snprintf(page, count, "%d\n", E1000_READ_REG(hw, E1000_TPT));
}

static int igb_txmpacks(char *page, char **start, off_t off, int count, 
			 int *eof, void *data)
{
	struct e1000_hw *hw;
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(page, count, "error: no hw data\n");

	return snprintf(page, count, "%d\n", 
			E1000_READ_REG(hw, E1000_MPTC));
}

static int igb_txbpacks(char *page, char **start, off_t off, int count, 
			 int *eof, void *data)
{
	struct e1000_hw *hw;
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(page, count, "error: no hw data\n");

	return snprintf(page, count, "%d\n", 
			E1000_READ_REG(hw, E1000_BPTC));

}

static int igb_txerrors(char *page, char **start, off_t off, int count, 
			 int *eof, void *data)
{
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	struct net_device_stats *net_stats;

	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");
	net_stats  = procfs_get_stats(adapter->netdev);
	if (net_stats == NULL)
		return snprintf(page, count, "error: no net stats\n");

	return snprintf(page, count, "%lu\n", 
			net_stats->tx_errors);
}

static int igb_txdrops(char *page, char **start, off_t off, int count, 
			int *eof, void *data)
{
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	struct net_device_stats *net_stats;

	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");
	net_stats  = procfs_get_stats(adapter->netdev);
	if (net_stats == NULL)
		return snprintf(page, count, "error: no net stats\n");

	return snprintf(page, count, "%lu\n", 
			net_stats->tx_dropped);
}

static int igb_rxframes(char *page, char **start, off_t off, int count, 
			 int *eof, void *data)
{
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	struct net_device_stats *net_stats;

	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");
	net_stats  = procfs_get_stats(adapter->netdev);
	if (net_stats == NULL)
		return snprintf(page, count, "error: no net stats\n");

	return snprintf(page, count, "%lu\n", 
			net_stats->rx_packets);
}

static int igb_rxbytes(char *page, char **start, off_t off, int count, 
			int *eof, void *data)
{
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	struct net_device_stats *net_stats;

	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");
	net_stats  = procfs_get_stats(adapter->netdev);
	if (net_stats == NULL)
		return snprintf(page, count, "error: no net stats\n");

	return snprintf(page, count, "%lu\n", 
			net_stats->rx_bytes);
}

static int igb_txframes(char *page, char **start, off_t off, int count, 
			 int *eof, void *data)
{
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	struct net_device_stats *net_stats;

	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");
	net_stats  = procfs_get_stats(adapter->netdev);
	if (net_stats == NULL)
		return snprintf(page, count, "error: no net stats\n");

	return snprintf(page, count, "%lu\n", 
			net_stats->tx_packets);
}

static int igb_txbytes(char *page, char **start, off_t off, int count, 
			int *eof, void *data)
{
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	struct net_device_stats *net_stats;

	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");
	net_stats  = procfs_get_stats(adapter->netdev);
	if (net_stats == NULL)
		return snprintf(page, count, "error: no net stats\n");

	return snprintf(page, count, "%lu\n", 
			net_stats->tx_bytes);
}

static int igb_linkstat(char *page, char **start, off_t off, int count, 
			 int *eof, void *data)
{
	int bitmask = 0;
	struct e1000_hw *hw;
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(page, count, "error: no hw data\n");

	if (test_bit(__IGB_DOWN, &adapter->state)) 
		bitmask |= 1;

	if (igb_has_link(adapter))
		bitmask |= 2;
	return snprintf(page, count, "0x%X\n", bitmask);
}

static int igb_funcid(char *page, char **start, off_t off, 
		      int count, int *eof, void *data)
{
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	struct net_device* netdev;

	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");
	netdev = adapter->netdev;
	if (netdev == NULL)
		return snprintf(page, count, "error: no net device\n");

	return snprintf(page, count, "0x%lX\n", netdev->base_addr);
}

static int igb_funcvers(char *page, char **start, off_t off, 
			int count, int *eof, void *data)
{
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	struct net_device* netdev;

	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");
	netdev = adapter->netdev;
	if (netdev == NULL)
		return snprintf(page, count, "error: no net device\n");

	return snprintf(page, count, "%s\n", igb_driver_version);
}

static int igb_macburn(char *page, char **start, off_t off, int count, 
			int *eof, void *data)
{
	struct e1000_hw *hw;
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(page, count, "error: no hw data\n");

	return snprintf(page, count, "0x%X%X%X%X%X%X\n",
		       (unsigned int)hw->mac.perm_addr[0],
		       (unsigned int)hw->mac.perm_addr[1],
		       (unsigned int)hw->mac.perm_addr[2],
		       (unsigned int)hw->mac.perm_addr[3],
		       (unsigned int)hw->mac.perm_addr[4],
		       (unsigned int)hw->mac.perm_addr[5]);
}

static int igb_macadmn(char *page, char **start, off_t off, 
		       int count, int *eof, void *data)
{
	struct e1000_hw *hw;
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(page, count, "error: no hw data\n");

	return snprintf(page, count, "0x%X%X%X%X%X%X\n",
		       (unsigned int)hw->mac.addr[0],
		       (unsigned int)hw->mac.addr[1],
		       (unsigned int)hw->mac.addr[2],
		       (unsigned int)hw->mac.addr[3],
		       (unsigned int)hw->mac.addr[4],
		       (unsigned int)hw->mac.addr[5]);
}

static int igb_maclla1(char *page, char **start, off_t off, int count, 
		       int *eof, void *data)
{
	struct e1000_hw *hw;
	u16 eeprom_buff[6];
	int first_word = 0x37;
	int word_count = 6;
	int rc;

	struct igb_adapter *adapter = (struct igb_adapter *)data;
	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");

	hw = &adapter->hw;
	if (hw == NULL)
		return snprintf(page, count, "error: no hw data\n");

	rc = e1000_read_nvm(hw, first_word, word_count, 
					   eeprom_buff);
	if (rc != E1000_SUCCESS)
		return 0;

	switch (hw->bus.func) {
	case 0:
		return snprintf(page, count, "0x%04X%04X%04X\n", 
				eeprom_buff[0],
				eeprom_buff[1],
				eeprom_buff[2]);
	case 1:
		return snprintf(page, count, "0x%04X%04X%04X\n", 
				eeprom_buff[3],
				eeprom_buff[4],
				eeprom_buff[5]);
	}
	return snprintf(page, count, "unexpected port %d\n", hw->bus.func);
}

static int igb_mtusize(char *page, char **start, off_t off, 
		       int count, int *eof, void *data)
{
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	struct net_device* netdev;

	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");
	netdev = adapter->netdev;
	if (netdev == NULL)
		return snprintf(page, count, "error: no net device\n");

	return snprintf(page, count, "%d\n", netdev->mtu);
}

static int igb_featflag(char *page, char **start, off_t off, int count, 
			 int *eof, void *data)
{
	int bitmask = 0;
#ifndef HAVE_NDO_SET_FEATURES
	struct igb_ring *ring;
#endif
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	struct net_device *netdev;	

	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");
	netdev = adapter->netdev;
	if (netdev == NULL)
		return snprintf(page, count, "error: no net device\n");

#ifndef HAVE_NDO_SET_FEATURES
	/* igb_get_rx_csum(netdev) doesn't compile so hard code */
	ring = adapter->rx_ring[0];
	bitmask = test_bit(IGB_RING_FLAG_RX_CSUM, &ring->flags);	
	return snprintf(page, count, "%d\n", bitmask);
#else
	if (netdev->features & NETIF_F_RXCSUM)
		bitmask |= 1;
	return snprintf(page, count, "%d\n", bitmask);
#endif
}

static int igb_lsominct(char *page, char **start, off_t off, int count, 
			 int *eof, void *data)
{
	return snprintf(page, count, "%d\n", 1);
}

static int igb_prommode(char *page, char **start, off_t off, int count, 
			 int *eof, void *data)
{
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	struct net_device *netdev;	

	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");
	netdev = adapter->netdev;
	if (netdev == NULL)
		return snprintf(page, count, "error: no net device\n");

	return snprintf(page, count, "%d\n", 
			netdev->flags & IFF_PROMISC);
}

static int igb_txdscqsz(char *page, char **start, off_t off, int count, 
			    int *eof, void *data)
{
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");

	return snprintf(page, count, "%d\n", adapter->tx_ring[0]->count);
}

static int igb_rxdscqsz(char *page, char **start, off_t off, int count, 
			    int *eof, void *data)
{
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");

	return snprintf(page, count, "%d\n", adapter->rx_ring[0]->count);
}

static int igb_rxqavg(char *page, char **start, off_t off, int count, 
		       int *eof, void *data)
{
	int index;
	int totaldiff = 0;
	u16 ntc;
	u16 ntu;
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");

	if (adapter->num_rx_queues <= 0)
		return snprintf(page, count,
				"can't calculate, number of queues %d\n",
				adapter->num_rx_queues);

	for (index = 0; index < adapter->num_rx_queues; index++) {
		ntc = adapter->rx_ring[index]->next_to_clean;
		ntu = adapter->rx_ring[index]->next_to_use;

		if (ntc >= ntu)
			totaldiff += (ntc - ntu);
		else
			totaldiff += (adapter->rx_ring[index]->count 
				      - ntu + ntc);
	}
	if (adapter->num_rx_queues <= 0)
		return snprintf(page, count, 
				"can't calculate, number of queues %d\n", 
				adapter->num_rx_queues);		
	return snprintf(page, count, "%d\n", totaldiff/adapter->num_rx_queues);
}

static int igb_txqavg(char *page, char **start, off_t off, int count, 
		       int *eof, void *data)
{
	int index;
	int totaldiff = 0;
	u16 ntc;
	u16 ntu;
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");

	if (adapter->num_tx_queues <= 0)
		return snprintf(page, count,
				"can't calculate, number of queues %d\n",
				adapter->num_tx_queues);

	for (index = 0; index < adapter->num_tx_queues; index++) {
		ntc = adapter->tx_ring[index]->next_to_clean;
		ntu = adapter->tx_ring[index]->next_to_use;

		if (ntc >= ntu)
			totaldiff += (ntc - ntu);
		else
			totaldiff += (adapter->tx_ring[index]->count 
				      - ntu + ntc);
	}
	if (adapter->num_tx_queues <= 0)
		return snprintf(page, count, 
				"can't calculate, number of queues %d\n", 
				adapter->num_tx_queues);		
	return snprintf(page, count, "%d\n", 
			totaldiff/adapter->num_tx_queues);
}

static int igb_iovotype(char *page, char **start, off_t off, int count, 
			  int *eof, void *data)
{
	return snprintf(page, count, "2\n");
}

static int igb_funcnbr(char *page, char **start, off_t off, int count, 
			 int *eof, void *data)
{
	struct igb_adapter *adapter = (struct igb_adapter *)data;
	if (adapter == NULL)
		return snprintf(page, count, "error: no adapter\n");

	return snprintf(page, count, "%d\n", adapter->vfs_allocated_count);
}

static int igb_therm_location(char *page, char **start, off_t off, 
				     int count, int *eof, void *data)
{
	struct igb_therm_proc_data *therm_data =
		(struct igb_therm_proc_data *)data;

	if (therm_data == NULL)
		return snprintf(page, count, "error: no therm_data\n");

	return snprintf(page, count, "%d\n", therm_data->sensor_data->location);
}

static int igb_therm_maxopthresh(char *page, char **start, off_t off, 
				    int count, int *eof, void *data)
{
	struct igb_therm_proc_data *therm_data =
		(struct igb_therm_proc_data *)data;

	if (therm_data == NULL)
		return snprintf(page, count, "error: no therm_data\n");

	return snprintf(page, count, "%d\n",
			therm_data->sensor_data->max_op_thresh);
}

static int igb_therm_cautionthresh(char *page, char **start, off_t off, 
				      int count, int *eof, void *data)
{
	struct igb_therm_proc_data *therm_data =
		(struct igb_therm_proc_data *)data;

	if (therm_data == NULL)
		return snprintf(page, count, "error: no therm_data\n");

	return snprintf(page, count, "%d\n",
			therm_data->sensor_data->caution_thresh);
}

static int igb_therm_temp(char *page, char **start, off_t off, 
			     int count, int *eof, void *data)
{
	s32 status;
	struct igb_therm_proc_data *therm_data =
		(struct igb_therm_proc_data *)data;

	if (therm_data == NULL)
		return snprintf(page, count, "error: no therm_data\n");

	status = e1000_get_thermal_sensor_data(therm_data->hw);
 	if (status != E1000_SUCCESS)
		snprintf(page, count, "error: status %d returned\n", status);

	return snprintf(page, count, "%d\n", therm_data->sensor_data->temp);
}

struct igb_proc_type{
	char name[32];
	int (*read)(char*, char**, off_t, int, int*, void*);
};

struct igb_proc_type igb_proc_entries[] = {
	{"fwbanner", &igb_fwbanner},
	{"numeports", &igb_numeports},
	{"porttype", &igb_porttype},
	{"portspeed", &igb_portspeed},
	{"wqlflag", &igb_wqlflag},
	{"xflowctl", &igb_xflowctl},
	{"rxdrops", &igb_rxdrops},
	{"rxerrors", &igb_rxerrors},
	{"rxupacks", &igb_rxupacks},
	{"rxmpacks", &igb_rxmpacks},
	{"rxbpacks", &igb_rxbpacks}, 
	{"txdrops", &igb_txdrops},
	{"txerrors", &igb_txerrors},
	{"txupacks", &igb_txupacks},
	{"txmpacks", &igb_txmpacks},
	{"txbpacks", &igb_txbpacks},
	{"rxframes", &igb_rxframes},
	{"rxbytes", &igb_rxbytes},
	{"txframes", &igb_txframes},
	{"txbytes", &igb_txbytes},
	{"linkstat", &igb_linkstat},
	{"funcid", &igb_funcid},
	{"funcvers", &igb_funcvers},
	{"macburn", &igb_macburn},
	{"macadmn", &igb_macadmn},
	{"maclla1", &igb_maclla1},
	{"mtusize", &igb_mtusize},
	{"featflag", &igb_featflag},
	{"lsominct", &igb_lsominct},
	{"prommode", &igb_prommode},
	{"txdscqsz", &igb_txdscqsz},
	{"rxdscqsz", &igb_rxdscqsz},
	{"txqavg", &igb_txqavg},
	{"rxqavg", &igb_rxqavg},
	{"iovotype", &igb_iovotype},
	{"funcnbr", &igb_funcnbr},
	{"", NULL}
};

struct igb_proc_type igb_internal_entries[] = {
	{"location", &igb_therm_location},
	{"temp", &igb_therm_temp},
	{"cautionthresh", &igb_therm_cautionthresh},
	{"maxopthresh", &igb_therm_maxopthresh},	
	{"", NULL}
};

void igb_del_proc_entries(struct igb_adapter *adapter)
{
	int index, i;
	char buf[16];	/* much larger than the sensor number will ever be */

	if (igb_top_dir == NULL)
		return;

	for (i = 0; i < E1000_MAX_SENSORS; i++) {
		if (adapter->therm_dir[i] == NULL)
			continue;

		for (index = 0; ; index++) {
			if (igb_internal_entries[index].read == NULL)
				break;

			 remove_proc_entry(igb_internal_entries[index].name,
					   adapter->therm_dir[i]);
		}
		snprintf(buf, sizeof(buf), "sensor_%d", i);
		remove_proc_entry(buf, adapter->info_dir);
	}

	if (adapter->info_dir != NULL) {
		for (index = 0; ; index++) {
			if (igb_proc_entries[index].read == NULL)
				break;
		        remove_proc_entry(igb_proc_entries[index].name,
					  adapter->info_dir); 
		}
		remove_proc_entry("info", adapter->eth_dir);
	}

	if (adapter->eth_dir != NULL)
		remove_proc_entry(pci_name(adapter->pdev), igb_top_dir);
}

/* called from igb_main.c */
void igb_procfs_exit(struct igb_adapter *adapter) 
{
	igb_del_proc_entries(adapter);
}

int igb_procfs_topdir_init(void) 
{
	igb_top_dir = proc_mkdir("driver/igb", NULL);
	if (igb_top_dir == NULL)
		return (-ENOMEM);

	return 0;
}

void igb_procfs_topdir_exit(void) 
{
//	remove_proc_entry("driver", proc_root_driver);
	remove_proc_entry("driver/igb", NULL);
}

/* called from igb_main.c */
int igb_procfs_init(struct igb_adapter *adapter) 
{
	int rc = 0;
	int i;
	int index;
	char buf[16];	/* much larger than the sensor number will ever be */

	adapter->eth_dir = NULL;
	adapter->info_dir = NULL;
	for (i = 0; i < E1000_MAX_SENSORS; i++)
		adapter->therm_dir[i] = NULL;

	if ( igb_top_dir == NULL ) {
		rc = -ENOMEM;
		goto fail;
	}

	adapter->eth_dir = proc_mkdir(pci_name(adapter->pdev), igb_top_dir);
	if (adapter->eth_dir == NULL) {
		rc = -ENOMEM;
		goto fail;
	}

	adapter->info_dir = proc_mkdir("info", adapter->eth_dir);
	if (adapter->info_dir == NULL) {
		rc = -ENOMEM;
		goto fail;
	}
	for (index = 0; ; index++) {
		if (igb_proc_entries[index].read == NULL) {
			break;
		}
		if (!(create_proc_read_entry(igb_proc_entries[index].name, 
					   0444, 
					   adapter->info_dir, 
					   igb_proc_entries[index].read, 
					   adapter))) {

			rc = -ENOMEM;
			goto fail;
		}
	}
	if (igb_thermal_present(adapter) == false)
		goto exit;

	for (i = 0; i < E1000_MAX_SENSORS; i++) {

		 if (adapter->hw.mac.thermal_sensor_data.sensor[i].location== 0)
			continue;

		snprintf(buf, sizeof(buf), "sensor_%d", i);
		adapter->therm_dir[i] = proc_mkdir(buf, adapter->info_dir);
		if (adapter->therm_dir[i] == NULL) {
			rc = -ENOMEM;
			goto fail;
		}
		for (index = 0; ; index++) {
			if (igb_internal_entries[index].read == NULL)
				break;
			/*
			 * therm_data struct contains pointer the read func
			 * will be needing
			 */
			adapter->therm_data[i].hw = &adapter->hw;
			adapter->therm_data[i].sensor_data = 
				&adapter->hw.mac.thermal_sensor_data.sensor[i];

			if (!(create_proc_read_entry(
					   igb_internal_entries[index].name, 
					   0444, 
					   adapter->therm_dir[i], 
					   igb_internal_entries[index].read, 
					   &adapter->therm_data[i]))) {
				rc = -ENOMEM;
				goto fail;
			}
		}
	}
	goto exit;

fail:
	igb_del_proc_entries(adapter);
exit:
	return rc;
}

#endif /* !IGB_SYSFS */
#endif /* IGB_PROCFS */
