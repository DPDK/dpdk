/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <rte_log.h>
#include <rte_bus.h>
#include <rte_malloc.h>
#include <rte_devargs.h>
#include <rte_memcpy.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_kvargs.h>
#include <rte_alarm.h>
#include <rte_interrupts.h>
#include <rte_errno.h>
#include <rte_per_lcore.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_common.h>
#include <rte_bus_vdev.h>
#include <rte_string_fns.h>
#include <rte_pmd_i40e.h>

#include "base/opae_hw_api.h"
#include "base/opae_ifpga_hw_api.h"
#include "base/ifpga_api.h"
#include "rte_rawdev.h"
#include "rte_rawdev_pmd.h"
#include "rte_bus_ifpga.h"
#include "ifpga_common.h"
#include "ifpga_logs.h"
#include "ifpga_rawdev.h"
#include "ipn3ke_rawdev_api.h"

#define PCI_VENDOR_ID_INTEL          0x8086
/* PCI Device ID */
#define PCIE_DEVICE_ID_PF_INT_5_X    0xBCBD
#define PCIE_DEVICE_ID_PF_INT_6_X    0xBCC0
#define PCIE_DEVICE_ID_PF_DSC_1_X    0x09C4
#define PCIE_DEVICE_ID_PAC_N3000     0x0B30
/* VF Device */
#define PCIE_DEVICE_ID_VF_INT_5_X    0xBCBF
#define PCIE_DEVICE_ID_VF_INT_6_X    0xBCC1
#define PCIE_DEVICE_ID_VF_DSC_1_X    0x09C5
#define PCIE_DEVICE_ID_VF_PAC_N3000  0x0B31
#define RTE_MAX_RAW_DEVICE           10

static const struct rte_pci_id pci_ifpga_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCIE_DEVICE_ID_PF_INT_5_X) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCIE_DEVICE_ID_VF_INT_5_X) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCIE_DEVICE_ID_PF_INT_6_X) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCIE_DEVICE_ID_VF_INT_6_X) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCIE_DEVICE_ID_PF_DSC_1_X) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCIE_DEVICE_ID_VF_DSC_1_X) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCIE_DEVICE_ID_PAC_N3000),},
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCIE_DEVICE_ID_VF_PAC_N3000),},
	{ .vendor_id = 0, /* sentinel */ },
};

static struct ifpga_rawdev ifpga_rawdevices[IFPGA_RAWDEV_NUM];

static int ifpga_monitor_refcnt;
static pthread_t ifpga_monitor_start_thread;

static struct ifpga_rawdev *
ifpga_rawdev_allocate(struct rte_rawdev *rawdev);
static int set_surprise_link_check_aer(
		struct ifpga_rawdev *ifpga_rdev, int force_disable);
static int ifpga_pci_find_next_ext_capability(unsigned int fd,
					      int start, uint32_t cap);
static int ifpga_pci_find_ext_capability(unsigned int fd, uint32_t cap);

struct ifpga_rawdev *
ifpga_rawdev_get(const struct rte_rawdev *rawdev)
{
	struct ifpga_rawdev *dev;
	unsigned int i;

	if (rawdev == NULL)
		return NULL;

	for (i = 0; i < IFPGA_RAWDEV_NUM; i++) {
		dev = &ifpga_rawdevices[i];
		if (dev->rawdev == rawdev)
			return dev;
	}

	return NULL;
}

static inline uint8_t
ifpga_rawdev_find_free_device_index(void)
{
	uint16_t dev_id;

	for (dev_id = 0; dev_id < IFPGA_RAWDEV_NUM; dev_id++) {
		if (ifpga_rawdevices[dev_id].rawdev == NULL)
			return dev_id;
	}

	return IFPGA_RAWDEV_NUM;
}
static struct ifpga_rawdev *
ifpga_rawdev_allocate(struct rte_rawdev *rawdev)
{
	struct ifpga_rawdev *dev;
	uint16_t dev_id;
	int i = 0;

	dev = ifpga_rawdev_get(rawdev);
	if (dev != NULL) {
		IFPGA_RAWDEV_PMD_ERR("Event device already allocated!");
		return NULL;
	}

	dev_id = ifpga_rawdev_find_free_device_index();
	if (dev_id == IFPGA_RAWDEV_NUM) {
		IFPGA_RAWDEV_PMD_ERR("Reached maximum number of raw devices");
		return NULL;
	}

	dev = &ifpga_rawdevices[dev_id];
	dev->rawdev = rawdev;
	dev->dev_id = dev_id;
	for (i = 0; i < IFPGA_MAX_IRQ; i++)
		dev->intr_handle[i] = NULL;
	dev->poll_enabled = 0;

	return dev;
}

static int
ifpga_pci_find_next_ext_capability(unsigned int fd, int start, uint32_t cap)
{
	uint32_t header;
	int ttl;
	int pos = RTE_PCI_CFG_SPACE_SIZE;
	int ret;

	/* minimum 8 bytes per capability */
	ttl = (RTE_PCI_CFG_SPACE_EXP_SIZE - RTE_PCI_CFG_SPACE_SIZE) / 8;

	if (start)
		pos = start;
	ret = pread(fd, &header, sizeof(header), pos);
	if (ret == -1)
		return -1;

	/*
	 * If we have no capabilities, this is indicated by cap ID,
	 * cap version and next pointer all being 0.
	 */
	if (header == 0)
		return 0;

	while (ttl-- > 0) {
		if (RTE_PCI_EXT_CAP_ID(header) == cap && pos != start)
			return pos;

		pos = RTE_PCI_EXT_CAP_NEXT(header);
		if (pos < RTE_PCI_CFG_SPACE_SIZE)
			break;
		ret = pread(fd, &header, sizeof(header), pos);
		if (ret == -1)
			return -1;
	}

	return 0;
}

static int
ifpga_pci_find_ext_capability(unsigned int fd, uint32_t cap)
{
	return ifpga_pci_find_next_ext_capability(fd, 0, cap);
}

static int ifpga_get_dev_vendor_id(const char *bdf,
	uint32_t *dev_id, uint32_t *vendor_id)
{
	int fd;
	char path[1024];
	int ret;
	uint32_t header;

	strlcpy(path, "/sys/bus/pci/devices/", sizeof(path));
	strlcat(path, bdf, sizeof(path));
	strlcat(path, "/config", sizeof(path));
	fd = open(path, O_RDWR);
	if (fd < 0)
		return -1;
	ret = pread(fd, &header, sizeof(header), 0);
	if (ret == -1) {
		close(fd);
		return -1;
	}
	(*vendor_id) = header & 0xffff;
	(*dev_id) = (header >> 16) & 0xffff;
	close(fd);

	return 0;
}

static int ifpga_rawdev_fill_info(struct ifpga_rawdev *ifpga_dev)
{
	struct opae_adapter *adapter = NULL;
	char path[1024] = "/sys/bus/pci/devices/";
	char link[1024], link1[1024];
	char dir[1024] = "/sys/devices/";
	char *c;
	int ret;
	char sub_brg_bdf[4][16] = {{0}};
	int point;
	DIR *dp = NULL;
	struct dirent *entry;
	int i, j;

	unsigned int dom, bus, dev;
	int func;
	uint32_t dev_id = 0;
	uint32_t vendor_id = 0;

	adapter = ifpga_dev ? ifpga_rawdev_get_priv(ifpga_dev->rawdev) : NULL;
	if (!adapter)
		return -ENODEV;

	strlcat(path, adapter->name, sizeof(path));
	memset(link, 0, sizeof(link));
	memset(link1, 0, sizeof(link1));
	ret = readlink(path, link, (sizeof(link)-1));
	if ((ret < 0) || ((unsigned int)ret > (sizeof(link)-1)))
		return -1;
	link[ret] = 0;   /* terminate string with null character */
	strlcpy(link1, link, sizeof(link1));
	memset(ifpga_dev->parent_bdf, 0, 16);
	point = strlen(link);
	if (point < 39)
		return -1;
	point -= 39;
	link[point] = 0;
	if (point < 12)
		return -1;
	point -= 12;
	rte_memcpy(ifpga_dev->parent_bdf, &link[point], 12);

	point = strlen(link1);
	if (point < 26)
		return -1;
	point -= 26;
	link1[point] = 0;
	if (point < 12)
		return -1;
	point -= 12;
	c = strchr(link1, 'p');
	if (!c)
		return -1;
	strlcat(dir, c, sizeof(dir));

	/* scan folder */
	dp = opendir(dir);
	if (dp == NULL)
		return -1;
	i = 0;
	while ((entry = readdir(dp)) != NULL) {
		if (i >= 4)
			break;
		if (entry->d_name[0] == '.')
			continue;
		if (strlen(entry->d_name) > 12)
			continue;
		if (sscanf(entry->d_name, "%x:%x:%x.%d",
			&dom, &bus, &dev, &func) < 4)
			continue;
		else {
			strlcpy(sub_brg_bdf[i],
				entry->d_name,
				sizeof(sub_brg_bdf[i]));
			i++;
		}
	}
	closedir(dp);

	/* get fpga and fvl */
	j = 0;
	for (i = 0; i < 4; i++) {
		strlcpy(link, dir, sizeof(link));
		strlcat(link, "/", sizeof(link));
		strlcat(link, sub_brg_bdf[i], sizeof(link));
		dp = opendir(link);
		if (dp == NULL)
			return -1;
		while ((entry = readdir(dp)) != NULL) {
			if (j >= 8)
				break;
			if (entry->d_name[0] == '.')
				continue;

			if (strlen(entry->d_name) > 12)
				continue;
			if (sscanf(entry->d_name, "%x:%x:%x.%d",
				&dom, &bus, &dev, &func) < 4)
				continue;
			else {
				if (ifpga_get_dev_vendor_id(entry->d_name,
					&dev_id, &vendor_id))
					continue;
				if (vendor_id == 0x8086 &&
					(dev_id == 0x0CF8 ||
					dev_id == 0x0D58 ||
					dev_id == 0x1580)) {
					strlcpy(ifpga_dev->fvl_bdf[j],
						entry->d_name,
						sizeof(ifpga_dev->fvl_bdf[j]));
					j++;
				}
			}
		}
		closedir(dp);
	}

	return 0;
}

#define HIGH_FATAL(_sens, value)\
	(((_sens)->flags & OPAE_SENSOR_HIGH_FATAL_VALID) &&\
	 (value > (_sens)->high_fatal))

#define HIGH_WARN(_sens, value)\
	(((_sens)->flags & OPAE_SENSOR_HIGH_WARN_VALID) &&\
	 (value > (_sens)->high_warn))

#define LOW_FATAL(_sens, value)\
	(((_sens)->flags & OPAE_SENSOR_LOW_FATAL_VALID) &&\
	 (value > (_sens)->low_fatal))

#define LOW_WARN(_sens, value)\
	(((_sens)->flags & OPAE_SENSOR_LOW_WARN_VALID) &&\
	 (value > (_sens)->low_warn))

#define AUX_VOLTAGE_WARN 11400

static int
ifpga_monitor_sensor(struct rte_rawdev *raw_dev,
	       bool *gsd_start)
{
	struct opae_adapter *adapter;
	struct opae_manager *mgr;
	struct opae_sensor_info *sensor;
	unsigned int value;
	int ret;

	adapter = ifpga_rawdev_get_priv(raw_dev);
	if (!adapter)
		return -ENODEV;

	mgr = opae_adapter_get_mgr(adapter);
	if (!mgr)
		return -ENODEV;

	opae_mgr_for_each_sensor(mgr, sensor) {
		if (!(sensor->flags & OPAE_SENSOR_VALID))
			goto fail;

		ret = opae_mgr_get_sensor_value(mgr, sensor, &value);
		if (ret)
			goto fail;

		if (value == 0xdeadbeef) {
			IFPGA_RAWDEV_PMD_ERR("dev_id %d sensor %s value %x\n",
					raw_dev->dev_id, sensor->name, value);
			continue;
		}

		/* monitor temperature sensors */
		if (!strcmp(sensor->name, "Board Temperature") ||
				!strcmp(sensor->name, "FPGA Die Temperature")) {
			IFPGA_RAWDEV_PMD_DEBUG("read sensor %s %d %d %d\n",
					sensor->name, value, sensor->high_warn,
					sensor->high_fatal);

			if (HIGH_WARN(sensor, value) ||
				LOW_WARN(sensor, value)) {
				IFPGA_RAWDEV_PMD_INFO("%s reach threshold %d\n",
					sensor->name, value);
				*gsd_start = true;
				break;
			}
		}

		/* monitor 12V AUX sensor */
		if (!strcmp(sensor->name, "12V AUX Voltage")) {
			if (value < AUX_VOLTAGE_WARN) {
				IFPGA_RAWDEV_PMD_INFO(
					"%s reach threshold %d mV\n",
					sensor->name, value);
				*gsd_start = true;
				break;
			}
		}
	}

	return 0;
fail:
	return -EFAULT;
}

static int set_surprise_link_check_aer(
	struct ifpga_rawdev *ifpga_rdev, int force_disable)
{
	struct rte_rawdev *rdev;
	int fd = -1;
	char path[1024];
	int pos;
	int ret;
	uint32_t data;
	bool enable = 0;
	uint32_t aer_new0, aer_new1;

	if (!ifpga_rdev || !ifpga_rdev->rawdev) {
		printf("\n device does not exist\n");
		return -EFAULT;
	}

	rdev = ifpga_rdev->rawdev;
	if (ifpga_rdev->aer_enable)
		return -EFAULT;
	if (ifpga_monitor_sensor(rdev, &enable))
		return -EFAULT;
	if (enable || force_disable) {
		IFPGA_RAWDEV_PMD_ERR("Set AER, pls graceful shutdown\n");
		ifpga_rdev->aer_enable = 1;
		/* get bridge fd */
		strlcpy(path, "/sys/bus/pci/devices/", sizeof(path));
		strlcat(path, ifpga_rdev->parent_bdf, sizeof(path));
		strlcat(path, "/config", sizeof(path));
		fd = open(path, O_RDWR);
		if (fd < 0)
			goto end;
		pos = ifpga_pci_find_ext_capability(fd, RTE_PCI_EXT_CAP_ID_ERR);
		if (!pos)
			goto end;
		/* save previous ECAP_AER+0x08 */
		ret = pread(fd, &data, sizeof(data), pos+0x08);
		if (ret == -1)
			goto end;
		ifpga_rdev->aer_old[0] = data;
		/* save previous ECAP_AER+0x14 */
		ret = pread(fd, &data, sizeof(data), pos+0x14);
		if (ret == -1)
			goto end;
		ifpga_rdev->aer_old[1] = data;

		/* set ECAP_AER+0x08 to 0xFFFFFFFF */
		data = 0xffffffff;
		ret = pwrite(fd, &data, 4, pos+0x08);
		if (ret == -1)
			goto end;
		/* set ECAP_AER+0x14 to 0xFFFFFFFF */
		ret = pwrite(fd, &data, 4, pos+0x14);
		if (ret == -1)
			goto end;

		/* read current ECAP_AER+0x08 */
		ret = pread(fd, &data, sizeof(data), pos+0x08);
		if (ret == -1)
			goto end;
		aer_new0 = data;
		/* read current ECAP_AER+0x14 */
		ret = pread(fd, &data, sizeof(data), pos+0x14);
		if (ret == -1)
			goto end;
		aer_new1 = data;

		if (fd != -1)
			close(fd);

		printf(">>>>>>Set AER %x,%x %x,%x\n",
			ifpga_rdev->aer_old[0], ifpga_rdev->aer_old[1],
			aer_new0, aer_new1);

		return 1;
		}

end:
	if (fd != -1)
		close(fd);
	return -EFAULT;
}

static void *
ifpga_rawdev_gsd_handle(__rte_unused void *param)
{
	struct ifpga_rawdev *ifpga_rdev;
	int i;
	int gsd_enable, ret;
#define MS 1000

	while (__atomic_load_n(&ifpga_monitor_refcnt, __ATOMIC_RELAXED)) {
		gsd_enable = 0;
		for (i = 0; i < IFPGA_RAWDEV_NUM; i++) {
			ifpga_rdev = &ifpga_rawdevices[i];
			if (ifpga_rdev->poll_enabled) {
				ret = set_surprise_link_check_aer(ifpga_rdev,
					gsd_enable);
				if (ret == 1 && !gsd_enable) {
					gsd_enable = 1;
					i = -1;
				}
			}
		}

		if (gsd_enable)
			printf(">>>>>>Pls Shutdown APP\n");

		rte_delay_us(100 * MS);
	}

	return NULL;
}

static int
ifpga_monitor_start_func(struct ifpga_rawdev *dev)
{
	int ret;

	if (!dev)
		return -ENODEV;

	ret = ifpga_rawdev_fill_info(dev);
	if (ret)
		return ret;

	dev->poll_enabled = 1;

	if (!__atomic_fetch_add(&ifpga_monitor_refcnt, 1, __ATOMIC_RELAXED)) {
		ret = rte_ctrl_thread_create(&ifpga_monitor_start_thread,
					     "ifpga-monitor", NULL,
					     ifpga_rawdev_gsd_handle, NULL);
		if (ret != 0) {
			ifpga_monitor_start_thread = 0;
			IFPGA_RAWDEV_PMD_ERR(
				"Fail to create ifpga monitor thread");
			return -1;
		}
	}

	return 0;
}

static int
ifpga_monitor_stop_func(struct ifpga_rawdev *dev)
{
	int ret;

	if (!dev || !dev->poll_enabled)
		return 0;

	dev->poll_enabled = 0;

	if (!__atomic_sub_fetch(&ifpga_monitor_refcnt, 1, __ATOMIC_RELAXED) &&
		ifpga_monitor_start_thread) {
		ret = pthread_cancel(ifpga_monitor_start_thread);
		if (ret)
			IFPGA_RAWDEV_PMD_ERR("Can't cancel the thread");

		ret = pthread_join(ifpga_monitor_start_thread, NULL);
		if (ret)
			IFPGA_RAWDEV_PMD_ERR("Can't join the thread");

		return ret;
	}

	return 0;
}

static int
ifpga_fill_afu_dev(struct opae_accelerator *acc,
		struct rte_afu_device *afu_dev)
{
	struct rte_mem_resource *res = afu_dev->mem_resource;
	struct opae_acc_region_info region_info;
	struct opae_acc_info info;
	unsigned long i;
	int ret;

	ret = opae_acc_get_info(acc, &info);
	if (ret)
		return ret;

	if (info.num_regions > PCI_MAX_RESOURCE)
		return -EFAULT;

	afu_dev->num_region = info.num_regions;

	for (i = 0; i < info.num_regions; i++) {
		region_info.index = i;
		ret = opae_acc_get_region_info(acc, &region_info);
		if (ret)
			return ret;

		if ((region_info.flags & ACC_REGION_MMIO) &&
		    (region_info.flags & ACC_REGION_READ) &&
		    (region_info.flags & ACC_REGION_WRITE)) {
			res[i].phys_addr = region_info.phys_addr;
			res[i].len = region_info.len;
			res[i].addr = region_info.addr;
		} else
			return -EFAULT;
	}

	return 0;
}

static int
ifpga_rawdev_info_get(struct rte_rawdev *dev,
		      rte_rawdev_obj_t dev_info,
		      size_t dev_info_size)
{
	struct opae_adapter *adapter;
	struct opae_accelerator *acc;
	struct rte_afu_device *afu_dev;
	struct opae_manager *mgr = NULL;
	struct opae_eth_group_region_info opae_lside_eth_info;
	struct opae_eth_group_region_info opae_nside_eth_info;
	int lside_bar_idx, nside_bar_idx;

	IFPGA_RAWDEV_PMD_FUNC_TRACE();

	if (!dev_info || dev_info_size != sizeof(*afu_dev)) {
		IFPGA_RAWDEV_PMD_ERR("Invalid request");
		return -EINVAL;
	}

	adapter = ifpga_rawdev_get_priv(dev);
	if (!adapter)
		return -ENOENT;

	afu_dev = dev_info;
	afu_dev->rawdev = dev;

	/* find opae_accelerator and fill info into afu_device */
	opae_adapter_for_each_acc(adapter, acc) {
		if (acc->index != afu_dev->id.port)
			continue;

		if (ifpga_fill_afu_dev(acc, afu_dev)) {
			IFPGA_RAWDEV_PMD_ERR("cannot get info\n");
			return -ENOENT;
		}
	}

	/* get opae_manager to rawdev */
	mgr = opae_adapter_get_mgr(adapter);
	if (mgr) {
		/* get LineSide BAR Index */
		if (opae_manager_get_eth_group_region_info(mgr, 0,
			&opae_lside_eth_info)) {
			return -ENOENT;
		}
		lside_bar_idx = opae_lside_eth_info.mem_idx;

		/* get NICSide BAR Index */
		if (opae_manager_get_eth_group_region_info(mgr, 1,
			&opae_nside_eth_info)) {
			return -ENOENT;
		}
		nside_bar_idx = opae_nside_eth_info.mem_idx;

		if (lside_bar_idx >= PCI_MAX_RESOURCE ||
			nside_bar_idx >= PCI_MAX_RESOURCE ||
			lside_bar_idx == nside_bar_idx)
			return -ENOENT;

		/* fill LineSide BAR Index */
		afu_dev->mem_resource[lside_bar_idx].phys_addr =
			opae_lside_eth_info.phys_addr;
		afu_dev->mem_resource[lside_bar_idx].len =
			opae_lside_eth_info.len;
		afu_dev->mem_resource[lside_bar_idx].addr =
			opae_lside_eth_info.addr;

		/* fill NICSide BAR Index */
		afu_dev->mem_resource[nside_bar_idx].phys_addr =
			opae_nside_eth_info.phys_addr;
		afu_dev->mem_resource[nside_bar_idx].len =
			opae_nside_eth_info.len;
		afu_dev->mem_resource[nside_bar_idx].addr =
			opae_nside_eth_info.addr;
	}
	return 0;
}

static int
ifpga_rawdev_configure(const struct rte_rawdev *dev,
		rte_rawdev_obj_t config,
		size_t config_size __rte_unused)
{
	IFPGA_RAWDEV_PMD_FUNC_TRACE();

	RTE_FUNC_PTR_OR_ERR_RET(dev, -EINVAL);

	return config ? 0 : 1;
}

static int
ifpga_rawdev_start(struct rte_rawdev *dev)
{
	int ret = 0;
	struct opae_adapter *adapter;

	IFPGA_RAWDEV_PMD_FUNC_TRACE();

	RTE_FUNC_PTR_OR_ERR_RET(dev, -EINVAL);

	adapter = ifpga_rawdev_get_priv(dev);
	if (!adapter)
		return -ENODEV;

	return ret;
}

static void
ifpga_rawdev_stop(struct rte_rawdev *dev)
{
	dev->started = 0;
}

static int
ifpga_rawdev_close(struct rte_rawdev *dev)
{
	struct opae_adapter *adapter;

	if (dev) {
		ifpga_monitor_stop_func(ifpga_rawdev_get(dev));
		adapter = ifpga_rawdev_get_priv(dev);
		if (adapter) {
			opae_adapter_destroy(adapter);
			opae_adapter_data_free(adapter->data);
		}
	}

	return dev ? 0:1;
}

static int
ifpga_rawdev_reset(struct rte_rawdev *dev)
{
	return dev ? 0:1;
}

static int
fpga_pr(struct rte_rawdev *raw_dev, u32 port_id, const char *buffer, u32 size,
			u64 *status)
{

	struct opae_adapter *adapter;
	struct opae_manager *mgr;
	struct opae_accelerator *acc;
	struct opae_bridge *br;
	int ret;

	adapter = ifpga_rawdev_get_priv(raw_dev);
	if (!adapter)
		return -ENODEV;

	mgr = opae_adapter_get_mgr(adapter);
	if (!mgr)
		return -ENODEV;

	acc = opae_adapter_get_acc(adapter, port_id);
	if (!acc)
		return -ENODEV;

	br = opae_acc_get_br(acc);
	if (!br)
		return -ENODEV;

	ret = opae_manager_flash(mgr, port_id, buffer, size, status);
	if (ret) {
		IFPGA_RAWDEV_PMD_ERR("%s pr error %d\n", __func__, ret);
		return ret;
	}

	ret = opae_bridge_reset(br);
	if (ret) {
		IFPGA_RAWDEV_PMD_ERR("%s reset port:%d error %d\n",
				__func__, port_id, ret);
		return ret;
	}

	return ret;
}

static int
rte_fpga_do_pr(struct rte_rawdev *rawdev, int port_id,
		const char *file_name)
{
	struct stat file_stat;
	int file_fd;
	int ret = 0;
	ssize_t buffer_size;
	void *buffer, *buf_to_free;
	u64 pr_error;

	if (!file_name)
		return -EINVAL;

	file_fd = open(file_name, O_RDONLY);
	if (file_fd < 0) {
		IFPGA_RAWDEV_PMD_ERR("%s: open file error: %s\n",
				__func__, file_name);
		IFPGA_RAWDEV_PMD_ERR("Message : %s\n", strerror(errno));
		return -EINVAL;
	}
	ret = stat(file_name, &file_stat);
	if (ret) {
		IFPGA_RAWDEV_PMD_ERR("stat on bitstream file failed: %s\n",
				file_name);
		ret = -EINVAL;
		goto close_fd;
	}
	buffer_size = file_stat.st_size;
	if (buffer_size <= 0) {
		ret = -EINVAL;
		goto close_fd;
	}

	IFPGA_RAWDEV_PMD_INFO("bitstream file size: %zu\n", buffer_size);
	buffer = rte_malloc(NULL, buffer_size, 0);
	if (!buffer) {
		ret = -ENOMEM;
		goto close_fd;
	}
	buf_to_free = buffer;

	/*read the raw data*/
	if (buffer_size != read(file_fd, (void *)buffer, buffer_size)) {
		ret = -EINVAL;
		goto free_buffer;
	}

	/*do PR now*/
	ret = fpga_pr(rawdev, port_id, buffer, buffer_size, &pr_error);
	IFPGA_RAWDEV_PMD_INFO("downloading to device port %d....%s.\n", port_id,
		ret ? "failed" : "success");
	if (ret) {
		ret = -EINVAL;
		goto free_buffer;
	}

free_buffer:
	rte_free(buf_to_free);
close_fd:
	close(file_fd);
	file_fd = 0;
	return ret;
}

static int
ifpga_rawdev_pr(struct rte_rawdev *dev,
	rte_rawdev_obj_t pr_conf)
{
	struct opae_adapter *adapter;
	struct opae_manager *mgr;
	struct opae_board_info *info;
	struct rte_afu_pr_conf *afu_pr_conf;
	int ret;
	struct uuid uuid;
	struct opae_accelerator *acc;

	IFPGA_RAWDEV_PMD_FUNC_TRACE();

	adapter = ifpga_rawdev_get_priv(dev);
	if (!adapter)
		return -ENODEV;

	if (!pr_conf)
		return -EINVAL;

	afu_pr_conf = pr_conf;

	if (afu_pr_conf->pr_enable) {
		ret = rte_fpga_do_pr(dev,
				afu_pr_conf->afu_id.port,
				afu_pr_conf->bs_path);
		if (ret) {
			IFPGA_RAWDEV_PMD_ERR("do pr error %d\n", ret);
			return ret;
		}
	}

	mgr = opae_adapter_get_mgr(adapter);
	if (!mgr) {
		IFPGA_RAWDEV_PMD_ERR("opae_manager of opae_adapter is NULL");
		return -1;
	}

	if (ifpga_mgr_ops.get_board_info(mgr, &info)) {
		IFPGA_RAWDEV_PMD_ERR("ifpga manager get_board_info fail!");
		return -1;
	}

	if (info->lightweight) {
		/* set uuid to all 0, when fpga is lightweight image */
		memset(&afu_pr_conf->afu_id.uuid.uuid_low, 0, sizeof(u64));
		memset(&afu_pr_conf->afu_id.uuid.uuid_high, 0, sizeof(u64));
	} else {
		acc = opae_adapter_get_acc(adapter, afu_pr_conf->afu_id.port);
		if (!acc)
			return -ENODEV;

		ret = opae_acc_get_uuid(acc, &uuid);
		if (ret)
			return ret;

		rte_memcpy(&afu_pr_conf->afu_id.uuid.uuid_low, uuid.b,
			sizeof(u64));
		rte_memcpy(&afu_pr_conf->afu_id.uuid.uuid_high, uuid.b + 8,
			sizeof(u64));

		IFPGA_RAWDEV_PMD_INFO("%s: uuid_l=0x%lx, uuid_h=0x%lx\n",
			__func__,
			(unsigned long)afu_pr_conf->afu_id.uuid.uuid_low,
			(unsigned long)afu_pr_conf->afu_id.uuid.uuid_high);
		}
	return 0;
}

static int
ifpga_rawdev_get_attr(struct rte_rawdev *dev,
	const char *attr_name, uint64_t *attr_value)
{
	struct opae_adapter *adapter;
	struct opae_manager *mgr;
	struct opae_retimer_info opae_rtm_info;
	struct opae_retimer_status opae_rtm_status;
	struct opae_eth_group_info opae_eth_grp_info;
	struct opae_eth_group_region_info opae_eth_grp_reg_info;
	int eth_group_num = 0;
	uint64_t port_link_bitmap = 0, port_link_bit;
	uint32_t i, j, p, q;

#define MAX_PORT_PER_RETIMER    4

	IFPGA_RAWDEV_PMD_FUNC_TRACE();

	if (!dev || !attr_name || !attr_value) {
		IFPGA_RAWDEV_PMD_ERR("Invalid arguments for getting attributes");
		return -1;
	}

	adapter = ifpga_rawdev_get_priv(dev);
	if (!adapter) {
		IFPGA_RAWDEV_PMD_ERR("Adapter of dev %s is NULL", dev->name);
		return -1;
	}

	mgr = opae_adapter_get_mgr(adapter);
	if (!mgr) {
		IFPGA_RAWDEV_PMD_ERR("opae_manager of opae_adapter is NULL");
		return -1;
	}

	/* currently, eth_group_num is always 2 */
	eth_group_num = opae_manager_get_eth_group_nums(mgr);
	if (eth_group_num < 0)
		return -1;

	if (!strcmp(attr_name, "LineSideBaseMAC")) {
		/* Currently FPGA not implement, so just set all zeros*/
		*attr_value = (uint64_t)0;
		return 0;
	}
	if (!strcmp(attr_name, "LineSideMACType")) {
		/* eth_group 0 on FPGA connect to LineSide */
		if (opae_manager_get_eth_group_info(mgr, 0,
			&opae_eth_grp_info))
			return -1;
		switch (opae_eth_grp_info.speed) {
		case ETH_SPEED_10G:
			*attr_value =
			(uint64_t)(IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI);
			break;
		case ETH_SPEED_25G:
			*attr_value =
			(uint64_t)(IFPGA_RAWDEV_RETIMER_MAC_TYPE_25GE_25GAUI);
			break;
		default:
			*attr_value =
			(uint64_t)(IFPGA_RAWDEV_RETIMER_MAC_TYPE_UNKNOWN);
			break;
		}
		return 0;
	}
	if (!strcmp(attr_name, "LineSideLinkSpeed")) {
		if (opae_manager_get_retimer_status(mgr, &opae_rtm_status))
			return -1;
		switch (opae_rtm_status.speed) {
		case MXD_1GB:
			*attr_value =
				(uint64_t)(IFPGA_RAWDEV_LINK_SPEED_UNKNOWN);
			break;
		case MXD_2_5GB:
			*attr_value =
				(uint64_t)(IFPGA_RAWDEV_LINK_SPEED_UNKNOWN);
			break;
		case MXD_5GB:
			*attr_value =
				(uint64_t)(IFPGA_RAWDEV_LINK_SPEED_UNKNOWN);
			break;
		case MXD_10GB:
			*attr_value =
				(uint64_t)(IFPGA_RAWDEV_LINK_SPEED_10GB);
			break;
		case MXD_25GB:
			*attr_value =
				(uint64_t)(IFPGA_RAWDEV_LINK_SPEED_25GB);
			break;
		case MXD_40GB:
			*attr_value =
				(uint64_t)(IFPGA_RAWDEV_LINK_SPEED_40GB);
			break;
		case MXD_100GB:
			*attr_value =
				(uint64_t)(IFPGA_RAWDEV_LINK_SPEED_UNKNOWN);
			break;
		case MXD_SPEED_UNKNOWN:
			*attr_value =
				(uint64_t)(IFPGA_RAWDEV_LINK_SPEED_UNKNOWN);
			break;
		default:
			*attr_value =
				(uint64_t)(IFPGA_RAWDEV_LINK_SPEED_UNKNOWN);
			break;
		}
		return 0;
	}
	if (!strcmp(attr_name, "LineSideLinkRetimerNum")) {
		if (opae_manager_get_retimer_info(mgr, &opae_rtm_info))
			return -1;
		*attr_value = (uint64_t)(opae_rtm_info.nums_retimer);
		return 0;
	}
	if (!strcmp(attr_name, "LineSideLinkPortNum")) {
		if (opae_manager_get_retimer_info(mgr, &opae_rtm_info))
			return -1;
		uint64_t tmp = (uint64_t)opae_rtm_info.ports_per_retimer *
					(uint64_t)opae_rtm_info.nums_retimer;
		*attr_value = tmp;
		return 0;
	}
	if (!strcmp(attr_name, "LineSideLinkStatus")) {
		if (opae_manager_get_retimer_info(mgr, &opae_rtm_info))
			return -1;
		if (opae_manager_get_retimer_status(mgr, &opae_rtm_status))
			return -1;
		(*attr_value) = 0;
		q = 0;
		port_link_bitmap = (uint64_t)(opae_rtm_status.line_link_bitmap);
		for (i = 0; i < opae_rtm_info.nums_retimer; i++) {
			p = i * MAX_PORT_PER_RETIMER;
			for (j = 0; j < opae_rtm_info.ports_per_retimer; j++) {
				port_link_bit = 0;
				IFPGA_BIT_SET(port_link_bit, (p+j));
				port_link_bit &= port_link_bitmap;
				if (port_link_bit)
					IFPGA_BIT_SET((*attr_value), q);
				q++;
			}
		}
		return 0;
	}
	if (!strcmp(attr_name, "LineSideBARIndex")) {
		/* eth_group 0 on FPGA connect to LineSide */
		if (opae_manager_get_eth_group_region_info(mgr, 0,
			&opae_eth_grp_reg_info))
			return -1;
		*attr_value = (uint64_t)opae_eth_grp_reg_info.mem_idx;
		return 0;
	}
	if (!strcmp(attr_name, "NICSideMACType")) {
		/* eth_group 1 on FPGA connect to NicSide */
		if (opae_manager_get_eth_group_info(mgr, 1,
			&opae_eth_grp_info))
			return -1;
		*attr_value = (uint64_t)(opae_eth_grp_info.speed);
		return 0;
	}
	if (!strcmp(attr_name, "NICSideLinkSpeed")) {
		/* eth_group 1 on FPGA connect to NicSide */
		if (opae_manager_get_eth_group_info(mgr, 1,
			&opae_eth_grp_info))
			return -1;
		*attr_value = (uint64_t)(opae_eth_grp_info.speed);
		return 0;
	}
	if (!strcmp(attr_name, "NICSideLinkPortNum")) {
		if (opae_manager_get_retimer_info(mgr, &opae_rtm_info))
			return -1;
		uint64_t tmp = (uint64_t)opae_rtm_info.nums_fvl *
					(uint64_t)opae_rtm_info.ports_per_fvl;
		*attr_value = tmp;
		return 0;
	}
	if (!strcmp(attr_name, "NICSideLinkStatus"))
		return 0;
	if (!strcmp(attr_name, "NICSideBARIndex")) {
		/* eth_group 1 on FPGA connect to NicSide */
		if (opae_manager_get_eth_group_region_info(mgr, 1,
			&opae_eth_grp_reg_info))
			return -1;
		*attr_value = (uint64_t)opae_eth_grp_reg_info.mem_idx;
		return 0;
	}

	IFPGA_RAWDEV_PMD_ERR("%s not support", attr_name);
	return -1;
}

static const struct rte_rawdev_ops ifpga_rawdev_ops = {
	.dev_info_get = ifpga_rawdev_info_get,
	.dev_configure = ifpga_rawdev_configure,
	.dev_start = ifpga_rawdev_start,
	.dev_stop = ifpga_rawdev_stop,
	.dev_close = ifpga_rawdev_close,
	.dev_reset = ifpga_rawdev_reset,

	.queue_def_conf = NULL,
	.queue_setup = NULL,
	.queue_release = NULL,

	.attr_get = ifpga_rawdev_get_attr,
	.attr_set = NULL,

	.enqueue_bufs = NULL,
	.dequeue_bufs = NULL,

	.dump = NULL,

	.xstats_get = NULL,
	.xstats_get_names = NULL,
	.xstats_get_by_name = NULL,
	.xstats_reset = NULL,

	.firmware_status_get = NULL,
	.firmware_version_get = NULL,
	.firmware_load = ifpga_rawdev_pr,
	.firmware_unload = NULL,

	.dev_selftest = NULL,
};

static int
ifpga_get_fme_error_prop(struct opae_manager *mgr,
		u64 prop_id, u64 *val)
{
	struct feature_prop prop;

	prop.feature_id = IFPGA_FME_FEATURE_ID_GLOBAL_ERR;
	prop.prop_id = prop_id;

	if (opae_manager_ifpga_get_prop(mgr, &prop))
		return -EINVAL;

	*val = prop.data;

	return 0;
}

static int
ifpga_set_fme_error_prop(struct opae_manager *mgr,
		u64 prop_id, u64 val)
{
	struct feature_prop prop;

	prop.feature_id = IFPGA_FME_FEATURE_ID_GLOBAL_ERR;
	prop.prop_id = prop_id;

	prop.data = val;

	if (opae_manager_ifpga_set_prop(mgr, &prop))
		return -EINVAL;

	return 0;
}

static int
fme_err_read_seu_emr(struct opae_manager *mgr)
{
	u64 val;
	int ret;

	ret = ifpga_get_fme_error_prop(mgr, FME_ERR_PROP_SEU_EMR_LOW, &val);
	if (ret)
		return -EINVAL;

	IFPGA_RAWDEV_PMD_INFO("seu emr low: 0x%" PRIx64 "\n", val);

	ret = ifpga_get_fme_error_prop(mgr, FME_ERR_PROP_SEU_EMR_HIGH, &val);
	if (ret)
		return -EINVAL;

	IFPGA_RAWDEV_PMD_INFO("seu emr high: 0x%" PRIx64 "\n", val);

	return 0;
}

static int fme_clear_warning_intr(struct opae_manager *mgr)
{
	u64 val;

	if (ifpga_set_fme_error_prop(mgr, FME_ERR_PROP_INJECT_ERRORS, 0))
		return -EINVAL;

	if (ifpga_get_fme_error_prop(mgr, FME_ERR_PROP_NONFATAL_ERRORS, &val))
		return -EINVAL;
	if ((val & 0x40) != 0)
		IFPGA_RAWDEV_PMD_INFO("clean not done\n");

	return 0;
}

static int fme_clean_fme_error(struct opae_manager *mgr)
{
	u64 val;

	if (ifpga_get_fme_error_prop(mgr, FME_ERR_PROP_ERRORS, &val))
		return -EINVAL;

	IFPGA_RAWDEV_PMD_DEBUG("before clean 0x%" PRIx64 "\n", val);

	ifpga_set_fme_error_prop(mgr, FME_ERR_PROP_CLEAR, val);

	if (ifpga_get_fme_error_prop(mgr, FME_ERR_PROP_ERRORS, &val))
		return -EINVAL;

	IFPGA_RAWDEV_PMD_DEBUG("after clean 0x%" PRIx64 "\n", val);

	return 0;
}

static int
fme_err_handle_error0(struct opae_manager *mgr)
{
	struct feature_fme_error0 fme_error0;
	u64 val;

	if (ifpga_get_fme_error_prop(mgr, FME_ERR_PROP_ERRORS, &val))
		return -EINVAL;

	if (fme_clean_fme_error(mgr))
		return -EINVAL;

	fme_error0.csr = val;

	if (fme_error0.fabric_err)
		IFPGA_RAWDEV_PMD_ERR("Fabric error\n");
	else if (fme_error0.fabfifo_overflow)
		IFPGA_RAWDEV_PMD_ERR("Fabric fifo under/overflow error\n");
	else if (fme_error0.afu_acc_mode_err)
		IFPGA_RAWDEV_PMD_ERR("AFU PF/VF access mismatch detected\n");
	else if (fme_error0.pcie0cdc_parity_err)
		IFPGA_RAWDEV_PMD_ERR("PCIe0 CDC Parity Error\n");
	else if (fme_error0.cvlcdc_parity_err)
		IFPGA_RAWDEV_PMD_ERR("CVL CDC Parity Error\n");
	else if (fme_error0.fpgaseuerr)
		fme_err_read_seu_emr(mgr);

	/* clean the errors */
	if (ifpga_set_fme_error_prop(mgr, FME_ERR_PROP_ERRORS, val))
		return -EINVAL;

	return 0;
}

static int
fme_err_handle_catfatal_error(struct opae_manager *mgr)
{
	struct feature_fme_ras_catfaterror fme_catfatal;
	u64 val;

	if (ifpga_get_fme_error_prop(mgr, FME_ERR_PROP_CATFATAL_ERRORS, &val))
		return -EINVAL;

	fme_catfatal.csr = val;

	if (fme_catfatal.cci_fatal_err)
		IFPGA_RAWDEV_PMD_ERR("CCI error detected\n");
	else if (fme_catfatal.fabric_fatal_err)
		IFPGA_RAWDEV_PMD_ERR("Fabric fatal error detected\n");
	else if (fme_catfatal.pcie_poison_err)
		IFPGA_RAWDEV_PMD_ERR("Poison error from PCIe ports\n");
	else if (fme_catfatal.inject_fata_err)
		IFPGA_RAWDEV_PMD_ERR("Injected Fatal Error\n");
	else if (fme_catfatal.crc_catast_err)
		IFPGA_RAWDEV_PMD_ERR("a catastrophic EDCRC error\n");
	else if (fme_catfatal.injected_catast_err)
		IFPGA_RAWDEV_PMD_ERR("Injected Catastrophic Error\n");
	else if (fme_catfatal.bmc_seu_catast_err)
		fme_err_read_seu_emr(mgr);

	return 0;
}

static int
fme_err_handle_nonfaterror(struct opae_manager *mgr)
{
	struct feature_fme_ras_nonfaterror nonfaterr;
	u64 val;

	if (ifpga_get_fme_error_prop(mgr, FME_ERR_PROP_NONFATAL_ERRORS, &val))
		return -EINVAL;

	nonfaterr.csr = val;

	if (nonfaterr.temp_thresh_ap1)
		IFPGA_RAWDEV_PMD_INFO("Temperature threshold triggered AP1\n");
	else if (nonfaterr.temp_thresh_ap2)
		IFPGA_RAWDEV_PMD_INFO("Temperature threshold triggered AP2\n");
	else if (nonfaterr.pcie_error)
		IFPGA_RAWDEV_PMD_INFO("an error has occurred in pcie\n");
	else if (nonfaterr.portfatal_error)
		IFPGA_RAWDEV_PMD_INFO("fatal error occurred in AFU port.\n");
	else if (nonfaterr.proc_hot)
		IFPGA_RAWDEV_PMD_INFO("a ProcHot event\n");
	else if (nonfaterr.afu_acc_mode_err)
		IFPGA_RAWDEV_PMD_INFO("an AFU PF/VF access mismatch\n");
	else if (nonfaterr.injected_nonfata_err) {
		IFPGA_RAWDEV_PMD_INFO("Injected Warning Error\n");
		fme_clear_warning_intr(mgr);
	} else if (nonfaterr.temp_thresh_AP6)
		IFPGA_RAWDEV_PMD_INFO("Temperature threshold triggered AP6\n");
	else if (nonfaterr.power_thresh_AP1)
		IFPGA_RAWDEV_PMD_INFO("Power threshold triggered AP1\n");
	else if (nonfaterr.power_thresh_AP2)
		IFPGA_RAWDEV_PMD_INFO("Power threshold triggered AP2\n");
	else if (nonfaterr.mbp_err)
		IFPGA_RAWDEV_PMD_INFO("an MBP event\n");

	return 0;
}

static void
fme_interrupt_handler(void *param)
{
	struct opae_manager *mgr = (struct opae_manager *)param;

	IFPGA_RAWDEV_PMD_INFO("%s interrupt occurred\n", __func__);

	fme_err_handle_error0(mgr);
	fme_err_handle_nonfaterror(mgr);
	fme_err_handle_catfatal_error(mgr);
}

int
ifpga_unregister_msix_irq(struct ifpga_rawdev *dev, enum ifpga_irq_type type,
		int vec_start, rte_intr_callback_fn handler, void *arg)
{
	struct rte_intr_handle **intr_handle;
	int rc = 0;
	int i = vec_start + 1;

	if (!dev)
		return -ENODEV;

	if (type == IFPGA_FME_IRQ)
		intr_handle = (struct rte_intr_handle **)&dev->intr_handle[0];
	else if (type == IFPGA_AFU_IRQ)
		intr_handle = (struct rte_intr_handle **)&dev->intr_handle[i];
	else
		return -EINVAL;

	if ((*intr_handle) == NULL) {
		IFPGA_RAWDEV_PMD_ERR("%s interrupt %d not registered\n",
			type == IFPGA_FME_IRQ ? "FME" : "AFU",
			type == IFPGA_FME_IRQ ? 0 : vec_start);
		return -ENOENT;
	}

	rte_intr_efd_disable(*intr_handle);

	rc = rte_intr_callback_unregister(*intr_handle, handler, arg);
	if (rc < 0) {
		IFPGA_RAWDEV_PMD_ERR("Failed to unregister %s interrupt %d\n",
			type == IFPGA_FME_IRQ ? "FME" : "AFU",
			type == IFPGA_FME_IRQ ? 0 : vec_start);
	} else {
		rte_intr_instance_free(*intr_handle);
		*intr_handle = NULL;
	}

	return rc;
}

int
ifpga_register_msix_irq(struct ifpga_rawdev *dev, int port_id,
		enum ifpga_irq_type type, int vec_start, int count,
		rte_intr_callback_fn handler, const char *name,
		void *arg)
{
	int ret;
	struct rte_intr_handle **intr_handle;
	struct opae_adapter *adapter;
	struct opae_manager *mgr;
	struct opae_accelerator *acc;
	int *intr_efds = NULL, nb_intr, i;

	if (!dev || !dev->rawdev)
		return -ENODEV;

	adapter = ifpga_rawdev_get_priv(dev->rawdev);
	if (!adapter)
		return -ENODEV;

	mgr = opae_adapter_get_mgr(adapter);
	if (!mgr)
		return -ENODEV;

	if (type == IFPGA_FME_IRQ) {
		intr_handle = (struct rte_intr_handle **)&dev->intr_handle[0];
		count = 1;
	} else if (type == IFPGA_AFU_IRQ) {
		i = vec_start + 1;
		intr_handle = (struct rte_intr_handle **)&dev->intr_handle[i];
	} else {
		return -EINVAL;
	}

	if (*intr_handle)
		return -EBUSY;

	*intr_handle = rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_PRIVATE);
	if (!(*intr_handle))
		return -ENOMEM;

	if (rte_intr_type_set(*intr_handle, RTE_INTR_HANDLE_VFIO_MSIX))
		return -rte_errno;

	ret = rte_intr_efd_enable(*intr_handle, count);
	if (ret)
		return -ENODEV;

	if (rte_intr_fd_set(*intr_handle,
			rte_intr_efds_index_get(*intr_handle, 0)))
		return -rte_errno;

	IFPGA_RAWDEV_PMD_DEBUG("register %s irq, vfio_fd=%d, fd=%d\n",
			name, rte_intr_dev_fd_get(*intr_handle),
			rte_intr_fd_get(*intr_handle));

	if (type == IFPGA_FME_IRQ) {
		struct fpga_fme_err_irq_set err_irq_set;
		err_irq_set.evtfd = rte_intr_efds_index_get(*intr_handle,
								   0);

		ret = opae_manager_ifpga_set_err_irq(mgr, &err_irq_set);
		if (ret)
			return -EINVAL;
	} else if (type == IFPGA_AFU_IRQ) {
		acc = opae_adapter_get_acc(adapter, port_id);
		if (!acc)
			return -EINVAL;

		nb_intr = rte_intr_nb_intr_get(*intr_handle);

		intr_efds = calloc(nb_intr, sizeof(int));
		if (!intr_efds)
			return -ENOMEM;

		for (i = 0; i < nb_intr; i++)
			intr_efds[i] = rte_intr_efds_index_get(*intr_handle, i);

		ret = opae_acc_set_irq(acc, vec_start, count, intr_efds);
		if (ret) {
			free(intr_efds);
			return -EINVAL;
		}
	}

	/* register interrupt handler using DPDK API */
	ret = rte_intr_callback_register(*intr_handle,
			handler, (void *)arg);
	if (ret) {
		free(intr_efds);
		return -EINVAL;
	}

	IFPGA_RAWDEV_PMD_INFO("success register %s interrupt\n", name);

	free(intr_efds);
	return 0;
}

static int
ifpga_rawdev_create(struct rte_pci_device *pci_dev,
			int socket_id)
{
	int ret = 0;
	struct rte_rawdev *rawdev = NULL;
	struct ifpga_rawdev *dev = NULL;
	struct opae_adapter *adapter = NULL;
	struct opae_manager *mgr = NULL;
	struct opae_adapter_data_pci *data = NULL;
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	int i;

	if (!pci_dev) {
		IFPGA_RAWDEV_PMD_ERR("Invalid pci_dev of the device!");
		ret = -EINVAL;
		goto cleanup;
	}

	memset(name, 0, sizeof(name));
	snprintf(name, RTE_RAWDEV_NAME_MAX_LEN, IFPGA_RAWDEV_NAME_FMT,
		pci_dev->addr.bus, pci_dev->addr.devid, pci_dev->addr.function);

	IFPGA_RAWDEV_PMD_INFO("Init %s on NUMA node %d", name, rte_socket_id());

	/* Allocate device structure */
	rawdev = rte_rawdev_pmd_allocate(name, sizeof(struct opae_adapter),
					 socket_id);
	if (rawdev == NULL) {
		IFPGA_RAWDEV_PMD_ERR("Unable to allocate rawdevice");
		ret = -EINVAL;
		goto cleanup;
	}

	ipn3ke_bridge_func.get_ifpga_rawdev = ifpga_rawdev_get;
	ipn3ke_bridge_func.set_i40e_sw_dev = rte_pmd_i40e_set_switch_dev;

	dev = ifpga_rawdev_allocate(rawdev);
	if (dev == NULL) {
		IFPGA_RAWDEV_PMD_ERR("Unable to allocate ifpga_rawdevice");
		ret = -EINVAL;
		goto cleanup;
	}
	dev->aer_enable = 0;

	/* alloc OPAE_FPGA_PCI data to register to OPAE hardware level API */
	data = opae_adapter_data_alloc(OPAE_FPGA_PCI);
	if (!data) {
		ret = -ENOMEM;
		goto cleanup;
	}

	/* init opae_adapter_data_pci for device specific information */
	for (i = 0; i < PCI_MAX_RESOURCE; i++) {
		data->region[i].phys_addr = pci_dev->mem_resource[i].phys_addr;
		data->region[i].len = pci_dev->mem_resource[i].len;
		data->region[i].addr = pci_dev->mem_resource[i].addr;
	}
	data->device_id = pci_dev->id.device_id;
	data->vendor_id = pci_dev->id.vendor_id;
	data->bus = pci_dev->addr.bus;
	data->devid = pci_dev->addr.devid;
	data->function = pci_dev->addr.function;
	data->vfio_dev_fd = rte_intr_dev_fd_get(pci_dev->intr_handle);

	adapter = rawdev->dev_private;
	/* create a opae_adapter based on above device data */
	ret = opae_adapter_init(adapter, pci_dev->device.name, data);
	if (ret) {
		ret = -ENOMEM;
		goto free_adapter_data;
	}

	rawdev->dev_ops = &ifpga_rawdev_ops;
	rawdev->device = &pci_dev->device;
	rawdev->driver_name = pci_dev->driver->driver.name;

	/* must enumerate the adapter before use it */
	ret = opae_adapter_enumerate(adapter);
	if (ret)
		goto free_adapter_data;

	/* get opae_manager to rawdev */
	mgr = opae_adapter_get_mgr(adapter);
	if (mgr) {
		/* PF function */
		IFPGA_RAWDEV_PMD_INFO("this is a PF function");
	}

	ret = ifpga_register_msix_irq(dev, 0, IFPGA_FME_IRQ, 0, 0,
			fme_interrupt_handler, "fme_irq", mgr);
	if (ret)
		goto free_adapter_data;

	ret = ifpga_monitor_start_func(dev);
	if (ret)
		goto free_adapter_data;

	return ret;

free_adapter_data:
	if (data)
		opae_adapter_data_free(data);
cleanup:
	if (rawdev)
		rte_rawdev_pmd_release(rawdev);

	return ret;
}

static int
ifpga_rawdev_destroy(struct rte_pci_device *pci_dev)
{
	int ret;
	struct rte_rawdev *rawdev;
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct opae_adapter *adapter;
	struct opae_manager *mgr;
	struct ifpga_rawdev *dev;

	if (!pci_dev) {
		IFPGA_RAWDEV_PMD_ERR("Invalid pci_dev of the device!");
		ret = -EINVAL;
		return ret;
	}

	memset(name, 0, sizeof(name));
	snprintf(name, RTE_RAWDEV_NAME_MAX_LEN, IFPGA_RAWDEV_NAME_FMT,
		pci_dev->addr.bus, pci_dev->addr.devid, pci_dev->addr.function);

	IFPGA_RAWDEV_PMD_INFO("Closing %s on NUMA node %d",
		name, rte_socket_id());

	rawdev = rte_rawdev_pmd_get_named_dev(name);
	if (!rawdev) {
		IFPGA_RAWDEV_PMD_ERR("Invalid device name (%s)", name);
		return -EINVAL;
	}
	dev = ifpga_rawdev_get(rawdev);
	if (dev)
		dev->rawdev = NULL;

	adapter = ifpga_rawdev_get_priv(rawdev);
	if (!adapter)
		return -ENODEV;

	mgr = opae_adapter_get_mgr(adapter);
	if (!mgr)
		return -ENODEV;

	if (ifpga_unregister_msix_irq(dev, IFPGA_FME_IRQ, 0,
				fme_interrupt_handler, mgr) < 0)
		return -EINVAL;

	/* rte_rawdev_close is called by pmd_release */
	ret = rte_rawdev_pmd_release(rawdev);
	if (ret)
		IFPGA_RAWDEV_PMD_DEBUG("Device cleanup failed");

	return ret;
}

static int
ifpga_rawdev_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	IFPGA_RAWDEV_PMD_FUNC_TRACE();
	return ifpga_rawdev_create(pci_dev, rte_socket_id());
}

static int
ifpga_rawdev_pci_remove(struct rte_pci_device *pci_dev)
{
	IFPGA_RAWDEV_PMD_INFO("remove pci_dev %s", pci_dev->device.name);
	return ifpga_rawdev_destroy(pci_dev);
}

static struct rte_pci_driver rte_ifpga_rawdev_pmd = {
	.id_table  = pci_ifpga_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe     = ifpga_rawdev_pci_probe,
	.remove    = ifpga_rawdev_pci_remove,
};

RTE_PMD_REGISTER_PCI(ifpga_rawdev_pci_driver, rte_ifpga_rawdev_pmd);
RTE_PMD_REGISTER_PCI_TABLE(ifpga_rawdev_pci_driver, rte_ifpga_rawdev_pmd);
RTE_PMD_REGISTER_KMOD_DEP(ifpga_rawdev_pci_driver, "* igb_uio | uio_pci_generic | vfio-pci");
RTE_LOG_REGISTER_DEFAULT(ifpga_rawdev_logtype, NOTICE);

static const char * const valid_args[] = {
#define IFPGA_ARG_NAME         "ifpga"
	IFPGA_ARG_NAME,
#define IFPGA_ARG_PORT         "port"
	IFPGA_ARG_PORT,
#define IFPGA_AFU_BTS          "afu_bts"
	IFPGA_AFU_BTS,
	NULL
};

static int ifpga_rawdev_get_string_arg(const char *key __rte_unused,
	const char *value, void *extra_args)
{
	int size;
	if (!value || !extra_args)
		return -EINVAL;

	size = strlen(value) + 1;
	*(char **)extra_args = rte_malloc(NULL, size, RTE_CACHE_LINE_SIZE);
	if (!*(char **)extra_args)
		return -ENOMEM;

	strlcpy(*(char **)extra_args, value, size);

	return 0;
}
static int
ifpga_cfg_probe(struct rte_vdev_device *dev)
{
	struct rte_devargs *devargs;
	struct rte_kvargs *kvlist = NULL;
	struct rte_rawdev *rawdev = NULL;
	struct ifpga_rawdev *ifpga_dev;
	int port;
	char *name = NULL;
	char dev_name[RTE_RAWDEV_NAME_MAX_LEN];
	int ret = -1;

	devargs = dev->device.devargs;

	kvlist = rte_kvargs_parse(devargs->args, valid_args);
	if (!kvlist) {
		IFPGA_RAWDEV_PMD_LOG(ERR, "error when parsing param");
		goto end;
	}

	if (rte_kvargs_count(kvlist, IFPGA_ARG_NAME) == 1) {
		if (rte_kvargs_process(kvlist, IFPGA_ARG_NAME,
				       &ifpga_rawdev_get_string_arg,
				       &name) < 0) {
			IFPGA_RAWDEV_PMD_ERR("error to parse %s",
				     IFPGA_ARG_NAME);
			goto end;
		}
	} else {
		IFPGA_RAWDEV_PMD_ERR("arg %s is mandatory for ifpga bus",
			  IFPGA_ARG_NAME);
		goto end;
	}

	if (rte_kvargs_count(kvlist, IFPGA_ARG_PORT) == 1) {
		if (rte_kvargs_process(kvlist,
			IFPGA_ARG_PORT,
			&rte_ifpga_get_integer32_arg,
			&port) < 0) {
			IFPGA_RAWDEV_PMD_ERR("error to parse %s",
				IFPGA_ARG_PORT);
			goto end;
		}
	} else {
		IFPGA_RAWDEV_PMD_ERR("arg %s is mandatory for ifpga bus",
			  IFPGA_ARG_PORT);
		goto end;
	}

	memset(dev_name, 0, sizeof(dev_name));
	snprintf(dev_name, RTE_RAWDEV_NAME_MAX_LEN, "IFPGA:%s", name);
	rawdev = rte_rawdev_pmd_get_named_dev(dev_name);
	if (!rawdev)
		goto end;
	ifpga_dev = ifpga_rawdev_get(rawdev);
	if (!ifpga_dev)
		goto end;

	memset(dev_name, 0, sizeof(dev_name));
	snprintf(dev_name, RTE_RAWDEV_NAME_MAX_LEN, "%d|%s",
	port, name);

	ret = rte_eal_hotplug_add(RTE_STR(IFPGA_BUS_NAME),
			dev_name, devargs->args);
end:
	rte_kvargs_free(kvlist);
	free(name);

	return ret;
}

static int
ifpga_cfg_remove(struct rte_vdev_device *vdev)
{
	IFPGA_RAWDEV_PMD_INFO("Remove ifpga_cfg %p",
		vdev);

	return 0;
}

static struct rte_vdev_driver ifpga_cfg_driver = {
	.probe = ifpga_cfg_probe,
	.remove = ifpga_cfg_remove,
};

RTE_PMD_REGISTER_VDEV(ifpga_rawdev_cfg, ifpga_cfg_driver);
RTE_PMD_REGISTER_ALIAS(ifpga_rawdev_cfg, ifpga_cfg);
RTE_PMD_REGISTER_PARAM_STRING(ifpga_rawdev_cfg,
	"ifpga=<string> "
	"port=<int> "
	"afu_bts=<path>");

struct rte_pci_bus *ifpga_get_pci_bus(void)
{
	return rte_ifpga_rawdev_pmd.bus;
}

int ifpga_rawdev_partial_reconfigure(struct rte_rawdev *dev, int port,
	const char *file)
{
	if (!dev) {
		IFPGA_RAWDEV_PMD_ERR("Input parameter is invalid");
		return -EINVAL;
	}

	return rte_fpga_do_pr(dev, port, file);
}

void ifpga_rawdev_cleanup(void)
{
	struct ifpga_rawdev *dev;
	unsigned int i;

	for (i = 0; i < IFPGA_RAWDEV_NUM; i++) {
		dev = &ifpga_rawdevices[i];
		if (dev->rawdev) {
			rte_rawdev_pmd_release(dev->rawdev);
			dev->rawdev = NULL;
		}
	}
}
