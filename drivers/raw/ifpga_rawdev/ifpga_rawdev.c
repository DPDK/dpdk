/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <rte_log.h>
#include <rte_bus.h>
#include <rte_eal_memconfig.h>
#include <rte_malloc.h>
#include <rte_devargs.h>
#include <rte_memcpy.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_kvargs.h>
#include <rte_alarm.h>

#include <rte_errno.h>
#include <rte_per_lcore.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_common.h>
#include <rte_bus_vdev.h>

#include "base/opae_hw_api.h"
#include "rte_rawdev.h"
#include "rte_rawdev_pmd.h"
#include "rte_bus_ifpga.h"
#include "ifpga_common.h"
#include "ifpga_logs.h"
#include "ifpga_rawdev.h"

int ifpga_rawdev_logtype;

#define PCI_VENDOR_ID_INTEL          0x8086
/* PCI Device ID */
#define PCIE_DEVICE_ID_PF_INT_5_X    0xBCBD
#define PCIE_DEVICE_ID_PF_INT_6_X    0xBCC0
#define PCIE_DEVICE_ID_PF_DSC_1_X    0x09C4
/* VF Device */
#define PCIE_DEVICE_ID_VF_INT_5_X    0xBCBF
#define PCIE_DEVICE_ID_VF_INT_6_X    0xBCC1
#define PCIE_DEVICE_ID_VF_DSC_1_X    0x09C5
#define RTE_MAX_RAW_DEVICE           10

static const struct rte_pci_id pci_ifpga_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCIE_DEVICE_ID_PF_INT_5_X) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCIE_DEVICE_ID_VF_INT_5_X) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCIE_DEVICE_ID_PF_INT_6_X) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCIE_DEVICE_ID_VF_INT_6_X) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCIE_DEVICE_ID_PF_DSC_1_X) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCIE_DEVICE_ID_VF_DSC_1_X) },
	{ .vendor_id = 0, /* sentinel */ },
};

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

static void
ifpga_rawdev_info_get(struct rte_rawdev *dev,
				     rte_rawdev_obj_t dev_info)
{
	struct opae_adapter *adapter;
	struct opae_accelerator *acc;
	struct rte_afu_device *afu_dev;

	IFPGA_RAWDEV_PMD_FUNC_TRACE();

	if (!dev_info) {
		IFPGA_RAWDEV_PMD_ERR("Invalid request");
		return;
	}

	adapter = ifpga_rawdev_get_priv(dev);
	if (!adapter)
		return;

	afu_dev = dev_info;
	afu_dev->rawdev = dev;

	/* find opae_accelerator and fill info into afu_device */
	opae_adapter_for_each_acc(adapter, acc) {
		if (acc->index != afu_dev->id.port)
			continue;

		if (ifpga_fill_afu_dev(acc, afu_dev)) {
			IFPGA_RAWDEV_PMD_ERR("cannot get info\n");
			return;
		}
	}
}

static int
ifpga_rawdev_configure(const struct rte_rawdev *dev,
		rte_rawdev_obj_t config)
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
	return dev ? 0:1;
}

static int
ifpga_rawdev_reset(struct rte_rawdev *dev)
{
	return dev ? 0:1;
}

static int
fpga_pr(struct rte_rawdev *raw_dev, u32 port_id, u64 *buffer, u32 size,
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
	void *buffer;
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
		return -EINVAL;
	}
	buffer_size = file_stat.st_size;
	IFPGA_RAWDEV_PMD_INFO("bitstream file size: %zu\n", buffer_size);
	buffer = rte_malloc(NULL, buffer_size, 0);
	if (!buffer) {
		ret = -ENOMEM;
		goto close_fd;
	}

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
	if (buffer)
		rte_free(buffer);
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

	acc = opae_adapter_get_acc(adapter, afu_pr_conf->afu_id.port);
	if (!acc)
		return -ENODEV;

	ret = opae_acc_get_uuid(acc, &uuid);
	if (ret)
		return ret;

	memcpy(&afu_pr_conf->afu_id.uuid.uuid_low, uuid.b, sizeof(u64));
	memcpy(&afu_pr_conf->afu_id.uuid.uuid_high, uuid.b + 8, sizeof(u64));

	IFPGA_RAWDEV_PMD_INFO("%s: uuid_l=0x%lx, uuid_h=0x%lx\n", __func__,
		(unsigned long)afu_pr_conf->afu_id.uuid.uuid_low,
		(unsigned long)afu_pr_conf->afu_id.uuid.uuid_high);

	return 0;
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

	.attr_get = NULL,
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
ifpga_rawdev_create(struct rte_pci_device *pci_dev,
			int socket_id)
{
	int ret = 0;
	struct rte_rawdev *rawdev = NULL;
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
	snprintf(name, RTE_RAWDEV_NAME_MAX_LEN, "IFPGA:%x:%02x.%x",
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

	/* create a opae_adapter based on above device data */
	adapter = opae_adapter_alloc(pci_dev->device.name, data);
	if (!adapter) {
		ret = -ENOMEM;
		goto free_adapter_data;
	}

	rawdev->dev_ops = &ifpga_rawdev_ops;
	rawdev->device = &pci_dev->device;
	rawdev->driver_name = pci_dev->device.driver->name;

	rawdev->dev_private = adapter;

	/* must enumerate the adapter before use it */
	ret = opae_adapter_enumerate(adapter);
	if (ret)
		goto free_adapter;

	/* get opae_manager to rawdev */
	mgr = opae_adapter_get_mgr(adapter);
	if (mgr) {
		/* PF function */
		IFPGA_RAWDEV_PMD_INFO("this is a PF function");
	}

	return ret;

free_adapter:
	if (adapter)
		opae_adapter_free(adapter);
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

	if (!pci_dev) {
		IFPGA_RAWDEV_PMD_ERR("Invalid pci_dev of the device!");
		ret = -EINVAL;
		return ret;
	}

	memset(name, 0, sizeof(name));
	snprintf(name, RTE_RAWDEV_NAME_MAX_LEN, "IFPGA:%x:%02x.%x",
		pci_dev->addr.bus, pci_dev->addr.devid, pci_dev->addr.function);

	IFPGA_RAWDEV_PMD_INFO("Closing %s on NUMA node %d",
		name, rte_socket_id());

	rawdev = rte_rawdev_pmd_get_named_dev(name);
	if (!rawdev) {
		IFPGA_RAWDEV_PMD_ERR("Invalid device name (%s)", name);
		return -EINVAL;
	}

	adapter = ifpga_rawdev_get_priv(rawdev);
	if (!adapter)
		return -ENODEV;

	opae_adapter_data_free(adapter->data);
	opae_adapter_free(adapter);

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

RTE_INIT(ifpga_rawdev_init_log);
static void
ifpga_rawdev_init_log(void)
{
	ifpga_rawdev_logtype = rte_log_register("driver.raw.init");
	if (ifpga_rawdev_logtype >= 0)
		rte_log_set_level(ifpga_rawdev_logtype, RTE_LOG_NOTICE);
}

static const char * const valid_args[] = {
#define IFPGA_ARG_NAME         "ifpga"
	IFPGA_ARG_NAME,
#define IFPGA_ARG_PORT         "port"
	IFPGA_ARG_PORT,
#define IFPGA_AFU_BTS          "afu_bts"
	IFPGA_AFU_BTS,
	NULL
};

static int
ifpga_cfg_probe(struct rte_vdev_device *dev)
{
	struct rte_devargs *devargs;
	struct rte_kvargs *kvlist = NULL;
	int port;
	char *name = NULL;
	char dev_name[RTE_RAWDEV_NAME_MAX_LEN];

	devargs = dev->device.devargs;

	kvlist = rte_kvargs_parse(devargs->args, valid_args);
	if (!kvlist) {
		IFPGA_RAWDEV_PMD_LOG(ERR, "error when parsing param");
		goto end;
	}

	if (rte_kvargs_count(kvlist, IFPGA_ARG_NAME) == 1) {
		if (rte_kvargs_process(kvlist, IFPGA_ARG_NAME,
				       &rte_ifpga_get_string_arg, &name) < 0) {
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
	snprintf(dev_name, RTE_RAWDEV_NAME_MAX_LEN, "%d|%s",
	port, name);

	rte_eal_hotplug_add(RTE_STR(IFPGA_BUS_NAME),
			dev_name, devargs->args);
end:
	if (kvlist)
		rte_kvargs_free(kvlist);
	if (name)
		free(name);

	return 0;
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

