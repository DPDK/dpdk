/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>
#include "rte_pmd_ifpga.h"
#include "ifpga_rawdev.h"
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

	snprintf(rdev_name, RTE_RAWDEV_NAME_MAX_LEN, "IFPGA:%02x:%02x.%x",
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
