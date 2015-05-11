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

#include "igb.h"
#include "e1000_82575.h"
#include "e1000_hw.h"

#ifdef IGB_PROCFS
#ifndef IGB_HWMON

#include <linux/module.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/netdevice.h>

static struct proc_dir_entry *igb_top_dir = NULL;


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

	return snprintf(page, count, "0x%02X%02X%02X%02X%02X%02X\n",
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

	return snprintf(page, count, "0x%02X%02X%02X%02X%02X%02X\n",
		       (unsigned int)hw->mac.addr[0],
		       (unsigned int)hw->mac.addr[1],
		       (unsigned int)hw->mac.addr[2],
		       (unsigned int)hw->mac.addr[3],
		       (unsigned int)hw->mac.addr[4],
		       (unsigned int)hw->mac.addr[5]);
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
	{"numeports", &igb_numeports},
	{"porttype", &igb_porttype},
	{"macburn", &igb_macburn},
	{"macadmn", &igb_macadmn},
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
		return -ENOMEM;

	return 0;
}

void igb_procfs_topdir_exit(void)
{
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

#endif /* !IGB_HWMON */
#endif /* IGB_PROCFS */
