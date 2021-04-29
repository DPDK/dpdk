/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>
#include "rte_pmd_ifpga.h"
#include "ifpga_rawdev.h"
#include "base/ifpga_api.h"
#include "base/ifpga_sec_mgr.h"


int
rte_pmd_ifpga_get_dev_id(const char *pci_addr, uint16_t *dev_id)
{
	struct rte_pci_addr addr;
	struct rte_rawdev *rdev = NULL;
	char rdev_name[RTE_RAWDEV_NAME_MAX_LEN] = {0};

	if (!pci_addr || !dev_id) {
		IFPGA_RAWDEV_PMD_ERR("Input parameter is invalid.");
		return -EINVAL;
	}

	if (strnlen(pci_addr, PCI_PRI_STR_SIZE) == PCI_PRI_STR_SIZE) {
		IFPGA_RAWDEV_PMD_ERR("PCI address is too long.");
		return -EINVAL;
	}

	if (rte_pci_addr_parse(pci_addr, &addr)) {
		IFPGA_RAWDEV_PMD_ERR("PCI address %s is invalid.", pci_addr);
		return -EINVAL;
	}

	snprintf(rdev_name, RTE_RAWDEV_NAME_MAX_LEN, IFPGA_RAWDEV_NAME_FMT,
		addr.bus, addr.devid, addr.function);
	rdev = rte_rawdev_pmd_get_named_dev(rdev_name);
	if (!rdev) {
		IFPGA_RAWDEV_PMD_DEBUG("%s is not probed by ifpga driver.",
			pci_addr);
		return -ENODEV;
	}
	*dev_id = rdev->dev_id;

	return 0;
}

static struct rte_rawdev *
get_rte_rawdev(uint16_t dev_id)
{
	struct rte_rawdev *dev = NULL;

	if (dev_id >= RTE_RAWDEV_MAX_DEVS)
		return NULL;

	dev = &rte_rawdevs[dev_id];
	if (dev->attached == RTE_RAWDEV_ATTACHED)
		return dev;

	return NULL;
}

static struct opae_adapter *
get_opae_adapter(uint16_t dev_id)
{
	struct rte_rawdev *dev = NULL;
	struct opae_adapter *adapter = NULL;

	dev = get_rte_rawdev(dev_id);
	if (!dev) {
		IFPGA_RAWDEV_PMD_ERR("Device ID %u is invalid.", dev_id);
		return NULL;
	}

	adapter = ifpga_rawdev_get_priv(dev);
	if (!adapter) {
		IFPGA_RAWDEV_PMD_ERR("Adapter is not registered.");
		return NULL;
	}

	return adapter;
}

static opae_share_data *
get_share_data(struct opae_adapter *adapter)
{
	opae_share_data *sd = NULL;

	if (!adapter)
		return NULL;

	sd = (opae_share_data *)adapter->shm.ptr;
	if (!sd) {
		IFPGA_RAWDEV_PMD_ERR("Share data is not initialized.");
		return NULL;
	}

	return sd;
}

int
rte_pmd_ifpga_get_rsu_status(uint16_t dev_id, uint32_t *stat, uint32_t *prog)
{
	struct opae_adapter *adapter = NULL;
	opae_share_data *sd = NULL;

	adapter = get_opae_adapter(dev_id);
	if (!adapter)
		return -ENODEV;

	sd = get_share_data(adapter);
	if (!sd)
		return -ENOMEM;

	if (stat)
		*stat = IFPGA_RSU_GET_STAT(sd->rsu_stat);
	if (prog)
		*prog = IFPGA_RSU_GET_PROG(sd->rsu_stat);

	return 0;
}

int
rte_pmd_ifpga_set_rsu_status(uint16_t dev_id, uint32_t stat, uint32_t prog)
{
	struct opae_adapter *adapter = NULL;
	opae_share_data *sd = NULL;

	adapter = get_opae_adapter(dev_id);
	if (!adapter)
		return -ENODEV;

	sd = get_share_data(adapter);
	if (!sd)
		return -ENOMEM;

	sd->rsu_stat = IFPGA_RSU_STATUS(stat, prog);

	return 0;
}

static int
ifpga_is_rebooting(struct opae_adapter *adapter)
{
	opae_share_data *sd = NULL;

	sd = get_share_data(adapter);
	if (!sd)
		return 1;

	if (IFPGA_RSU_GET_STAT(sd->rsu_stat) == IFPGA_RSU_REBOOT) {
		IFPGA_RAWDEV_PMD_WARN("Reboot is in progress.");
		return 1;
	}

	return 0;
}

static int
get_common_property(struct opae_adapter *adapter,
	rte_pmd_ifpga_common_prop *prop)
{
	struct ifpga_fme_hw *fme = NULL;
	struct opae_board_info *info = NULL;
	struct feature_prop fp;
	struct uuid pr_id;
	int ret = 0;

	if (!adapter || !prop)
		return -EINVAL;

	if (!adapter->mgr || !adapter->mgr->data) {
		IFPGA_RAWDEV_PMD_ERR("Manager is not registered.");
		return -ENODEV;
	}

	fme = adapter->mgr->data;
	fp.feature_id = FME_FEATURE_ID_HEADER;
	fp.prop_id = FME_HDR_PROP_PORTS_NUM;
	ret = ifpga_get_prop(fme->parent, FEATURE_FIU_ID_FME, 0, &fp);
	if (ret) {
		IFPGA_RAWDEV_PMD_ERR("Failed to get port number.");
		return ret;
	}
	prop->num_ports = fp.data;

	fp.prop_id = FME_HDR_PROP_BITSTREAM_ID;
	ret = ifpga_get_prop(fme->parent, FEATURE_FIU_ID_FME, 0, &fp);
	if (ret) {
		IFPGA_RAWDEV_PMD_ERR("Failed to get bitstream ID.");
		return ret;
	}
	prop->bitstream_id = fp.data;

	fp.prop_id = FME_HDR_PROP_BITSTREAM_METADATA;
	ret = ifpga_get_prop(fme->parent, FEATURE_FIU_ID_FME, 0, &fp);
	if (ret) {
		IFPGA_RAWDEV_PMD_ERR("Failed to get bitstream metadata.");
		return ret;
	}
	prop->bitstream_metadata = fp.data;

	ret = opae_mgr_get_uuid(adapter->mgr, &pr_id);
	if (ret) {
		IFPGA_RAWDEV_PMD_ERR("Failed to get PR ID.");
		return ret;
	}
	memcpy(prop->pr_id.b, pr_id.b, sizeof(rte_pmd_ifpga_uuid));

	ret = opae_mgr_get_board_info(adapter->mgr, &info);
	if (ret) {
		IFPGA_RAWDEV_PMD_ERR("Failed to get board info.");
		return ret;
	}
	prop->boot_page = info->boot_page;
	prop->bmc_version = info->max10_version;
	prop->bmc_nios_version = info->nios_fw_version;

	return 0;
}

static int
get_port_property(struct opae_adapter *adapter, uint16_t port,
	rte_pmd_ifpga_port_prop *prop)
{
	struct ifpga_fme_hw *fme = NULL;
	struct feature_prop fp;
	struct opae_accelerator *acc = NULL;
	struct uuid afu_id;
	int ret = 0;

	if (!adapter || !prop)
		return -EINVAL;

	if (!adapter->mgr || !adapter->mgr->data) {
		IFPGA_RAWDEV_PMD_ERR("Manager is not registered.");
		return -ENODEV;
	}

	fme = adapter->mgr->data;
	fp.feature_id = FME_FEATURE_ID_HEADER;
	fp.prop_id = FME_HDR_PROP_PORT_TYPE;
	fp.data = port;
	fp.data <<= 32;
	ret = ifpga_get_prop(fme->parent, FEATURE_FIU_ID_FME, 0, &fp);
	if (ret)
		return ret;
	prop->type = fp.data & 0xffffffff;

	if (prop->type == 0) {
		acc = opae_adapter_get_acc(adapter, port);
		ret = opae_acc_get_uuid(acc, &afu_id);
		if (ret) {
			IFPGA_RAWDEV_PMD_ERR("Failed to get port%u AFU ID.",
				port);
			return ret;
		}
		memcpy(prop->afu_id.b, afu_id.b, sizeof(rte_pmd_ifpga_uuid));
	}

	return 0;
}

int
rte_pmd_ifpga_get_property(uint16_t dev_id, rte_pmd_ifpga_prop *prop)
{
	struct opae_adapter *adapter = NULL;
	uint32_t i = 0;
	int ret = 0;

	adapter = get_opae_adapter(dev_id);
	if (!adapter)
		return -ENODEV;

	opae_adapter_lock(adapter, -1);
	if (ifpga_is_rebooting(adapter)) {
		ret = -EBUSY;
		goto unlock_dev;
	}

	ret = get_common_property(adapter, &prop->common);
	if (ret) {
		ret = -EIO;
		goto unlock_dev;
	}

	for (i = 0; i < prop->common.num_ports; i++) {
		ret = get_port_property(adapter, i, &prop->port[i]);
		if (ret) {
			ret = -EIO;
			break;
		}
	}

unlock_dev:
	opae_adapter_unlock(adapter);
	return ret;
}

int
rte_pmd_ifpga_get_phy_info(uint16_t dev_id, rte_pmd_ifpga_phy_info *info)
{
	struct opae_adapter *adapter = NULL;
	struct opae_retimer_info rtm_info;
	struct opae_retimer_status rtm_status;
	int ret = 0;

	adapter = get_opae_adapter(dev_id);
	if (!adapter)
		return -ENODEV;

	opae_adapter_lock(adapter, -1);
	if (ifpga_is_rebooting(adapter)) {
		ret = -EBUSY;
		goto unlock_dev;
	}

	ret = opae_manager_get_retimer_info(adapter->mgr, &rtm_info);
	if (ret) {
		IFPGA_RAWDEV_PMD_ERR("Failed to get retimer info.");
		ret = -EIO;
		goto unlock_dev;
	}
	info->num_retimers = rtm_info.nums_retimer;

	ret = opae_manager_get_retimer_status(adapter->mgr, &rtm_status);
	if (ret) {
		IFPGA_RAWDEV_PMD_ERR("Failed to get retimer status.");
		ret = -EIO;
		goto unlock_dev;
	}
	info->link_speed = rtm_status.speed;
	info->link_status = rtm_status.line_link_bitmap;

unlock_dev:
	opae_adapter_unlock(adapter);
	return ret;
}

int
rte_pmd_ifpga_update_flash(uint16_t dev_id, const char *image,
	uint64_t *status)
{
	struct opae_adapter *adapter = NULL;

	adapter = get_opae_adapter(dev_id);
	if (!adapter)
		return -ENODEV;

	return opae_mgr_update_flash(adapter->mgr, image, status);
}

int
rte_pmd_ifpga_stop_update(uint16_t dev_id, int force)
{
	struct opae_adapter *adapter = NULL;

	adapter = get_opae_adapter(dev_id);
	if (!adapter)
		return -ENODEV;

	return opae_mgr_stop_flash_update(adapter->mgr, force);
}

int
rte_pmd_ifpga_reboot_try(uint16_t dev_id)
{
	struct opae_adapter *adapter = NULL;
	opae_share_data *sd = NULL;

	adapter = get_opae_adapter(dev_id);
	if (!adapter)
		return -ENODEV;

	sd = get_share_data(adapter);
	if (!sd)
		return -ENOMEM;

	opae_adapter_lock(adapter, -1);
	if (IFPGA_RSU_GET_STAT(sd->rsu_stat) != IFPGA_RSU_IDLE) {
		opae_adapter_unlock(adapter);
		IFPGA_RAWDEV_PMD_WARN("Update or reboot is in progress.");
		return -EBUSY;
	}
	sd->rsu_stat = IFPGA_RSU_STATUS(IFPGA_RSU_REBOOT, 0);
	opae_adapter_unlock(adapter);

	return 0;
}

int
rte_pmd_ifpga_reload(uint16_t dev_id, int type, int page)
{
	struct opae_adapter *adapter = NULL;

	adapter = get_opae_adapter(dev_id);
	if (!adapter)
		return -ENODEV;

	return opae_mgr_reload(adapter->mgr, type, page);
}

const struct rte_pci_bus *
rte_pmd_ifpga_get_pci_bus(void)
{
	return ifpga_get_pci_bus();
}

int
rte_pmd_ifpga_partial_reconfigure(uint16_t dev_id, int port, const char *file)
{
	struct rte_rawdev *dev = NULL;

	dev = get_rte_rawdev(dev_id);
	if (!dev) {
		IFPGA_RAWDEV_PMD_ERR("Device ID %u is invalid.", dev_id);
		return -EINVAL;
	}

	return ifpga_rawdev_partial_reconfigure(dev, port, file);
}

void
rte_pmd_ifpga_cleanup(void)
{
	ifpga_rawdev_cleanup();
}
