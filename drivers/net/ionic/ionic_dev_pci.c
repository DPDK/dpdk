/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2022 Advanced Micro Devices, Inc.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

#include <rte_common.h>
#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_pci.h>
#include <bus_pci_driver.h>
#include <rte_eal.h>
#include <ethdev_pci.h>
#include <rte_dev.h>
#include <rte_kvargs.h>

#include "ionic.h"
#include "ionic_if.h"
#include "ionic_dev.h"
#include "ionic_ethdev.h"
#include "ionic_logs.h"

static const struct rte_pci_id pci_id_ionic_map[] = {
	{ RTE_PCI_DEVICE(IONIC_PENSANDO_VENDOR_ID, IONIC_DEV_ID_ETH_PF) },
	{ RTE_PCI_DEVICE(IONIC_PENSANDO_VENDOR_ID, IONIC_DEV_ID_ETH_VF) },
	{ RTE_PCI_DEVICE(IONIC_PENSANDO_VENDOR_ID, IONIC_DEV_ID_ETH_MGMT) },
	{ .vendor_id = 0, /* sentinel */ },
};

static int
ionic_pci_setup(struct ionic_adapter *adapter)
{
	struct ionic_dev_bar *bar = adapter->bars.bar;
	unsigned int num_bars = adapter->bars.num_bars;
	struct ionic_dev *idev = &adapter->idev;
	struct rte_pci_device *bus_dev = adapter->bus_dev;
	uint32_t sig;
	u_char *bar0_base;
	unsigned int i;

	/* BAR0: dev_cmd and interrupts */
	if (num_bars < 1) {
		IONIC_PRINT(ERR, "No bars found, aborting\n");
		return -EFAULT;
	}

	if (bar->len < IONIC_BAR0_SIZE) {
		IONIC_PRINT(ERR,
			"Resource bar size %lu too small, aborting\n",
			bar->len);
		return -EFAULT;
	}

	bar0_base = bar->vaddr;
	idev->dev_info = (union ionic_dev_info_regs *)
		&bar0_base[IONIC_BAR0_DEV_INFO_REGS_OFFSET];
	idev->dev_cmd = (union ionic_dev_cmd_regs *)
		&bar0_base[IONIC_BAR0_DEV_CMD_REGS_OFFSET];
	idev->intr_status = (struct ionic_intr_status *)
		&bar0_base[IONIC_BAR0_INTR_STATUS_OFFSET];
	idev->intr_ctrl = (struct ionic_intr *)
		&bar0_base[IONIC_BAR0_INTR_CTRL_OFFSET];

	sig = ioread32(&idev->dev_info->signature);
	if (sig != IONIC_DEV_INFO_SIGNATURE) {
		IONIC_PRINT(ERR, "Incompatible firmware signature %#x",
			sig);
		return -EFAULT;
	}

	for (i = 0; i < IONIC_DEVINFO_FWVERS_BUFLEN; i++)
		adapter->fw_version[i] =
			ioread8(&idev->dev_info->fw_version[i]);
	adapter->fw_version[IONIC_DEVINFO_FWVERS_BUFLEN - 1] = '\0';

	adapter->name = bus_dev->device.name;

	IONIC_PRINT(DEBUG, "%s firmware version: %s",
		adapter->name, adapter->fw_version);

	/* BAR1: doorbells */
	bar++;
	if (num_bars < 2) {
		IONIC_PRINT(ERR, "Doorbell bar missing, aborting\n");
		return -EFAULT;
	}

	idev->db_pages = bar->vaddr;

	return 0;
}

const char *ionic_pci_devargs_arr[] = {
	PMD_IONIC_CMB_KVARG,
	NULL,
};

static int
ionic_pci_devarg_cmb(const char *key __rte_unused, const char *val, void *arg)
{
	struct ionic_adapter *adapter = arg;

	if (!strcmp(val, "1")) {
		IONIC_PRINT(NOTICE, "%s enabled", PMD_IONIC_CMB_KVARG);
		adapter->q_in_cmb = true;
	} else if (!strcmp(val, "0")) {
		IONIC_PRINT(DEBUG, "%s disabled (default)",
			PMD_IONIC_CMB_KVARG);
	} else {
		IONIC_PRINT(ERR, "%s=%s invalid, use 1 or 0",
			PMD_IONIC_CMB_KVARG, val);
		return -ERANGE;
	}

	return 0;
}

static int
ionic_pci_devargs(struct ionic_adapter *adapter, struct rte_devargs *devargs)
{
	struct rte_kvargs *kvlist;
	int err = 0;

	if (!devargs)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, ionic_pci_devargs_arr);
	if (!kvlist) {
		IONIC_PRINT(ERR, "Couldn't parse args '%s'", devargs->args);
		return -EINVAL;
	}

	if (rte_kvargs_count(kvlist, PMD_IONIC_CMB_KVARG) == 1) {
		err = rte_kvargs_process(kvlist, PMD_IONIC_CMB_KVARG,
				ionic_pci_devarg_cmb, adapter);
		if (err < 0)
			goto free_kvlist;
	}

free_kvlist:
	rte_kvargs_free(kvlist);
	return err;
}

static void
ionic_pci_copy_bus_info(struct ionic_adapter *adapter,
	struct rte_eth_dev *eth_dev)
{
	rte_eth_copy_pci_info(eth_dev, adapter->bus_dev);
}

static int
ionic_pci_configure_intr(struct ionic_adapter *adapter)
{
	struct rte_pci_device *pci_dev =
		(struct rte_pci_device *)(adapter->bus_dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	int err;

	IONIC_PRINT(ERR, "Configuring %u intrs", adapter->nintrs);

	if (rte_intr_efd_enable(intr_handle, adapter->nintrs)) {
		IONIC_PRINT(ERR, "Fail to create eventfd");
		return -1;
	}

	if (rte_intr_dp_is_en(intr_handle)) {
		IONIC_PRINT(NOTICE,
			"Packet I/O interrupt on datapath is enabled");
		if (rte_intr_vec_list_alloc(intr_handle, "intr_vec",
						adapter->nintrs)) {
			IONIC_PRINT(ERR, "Failed to allocate %u vectors",
						adapter->nintrs);
			return -ENOMEM;
		}
	}

	err = rte_intr_callback_register(intr_handle,
		ionic_dev_interrupt_handler,
		adapter);
	if (err) {
		IONIC_PRINT(ERR,
			"Failure registering interrupts handler (%d)", err);
		return err;
	}

	/* enable intr mapping */
	err = rte_intr_enable(intr_handle);
	if (err) {
		IONIC_PRINT(ERR, "Failure enabling interrupts (%d)", err);
		return err;
	}

	return 0;
}

static void
ionic_pci_unconfigure_intr(struct ionic_adapter *adapter)
{
	struct rte_pci_device *pci_dev =
		(struct rte_pci_device *)(adapter->bus_dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;

	rte_intr_disable(intr_handle);

	rte_intr_callback_unregister(intr_handle,
		ionic_dev_interrupt_handler,
		adapter);
}

static const struct ionic_dev_intf ionic_pci_intf = {
	.setup = ionic_pci_setup,
	.devargs = ionic_pci_devargs,
	.copy_bus_info = ionic_pci_copy_bus_info,
	.configure_intr = ionic_pci_configure_intr,
	.unconfigure_intr = ionic_pci_unconfigure_intr,
};

static int
eth_ionic_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	struct rte_mem_resource *resource;
	struct ionic_bars bars;
	unsigned long i;

	IONIC_PRINT(NOTICE, "Initializing device %s %s",
		pci_dev->device.name,
		rte_eal_process_type() == RTE_PROC_SECONDARY ?
		"[SECONDARY]" : "");

	bars.num_bars = 0;
	for (i = 0; i < PCI_MAX_RESOURCE && i < IONIC_BARS_MAX; i++) {
		resource = &pci_dev->mem_resource[i];
		if (resource->phys_addr == 0 || resource->len == 0)
			continue;

		bars.bar[bars.num_bars].vaddr = resource->addr;
		bars.bar[bars.num_bars].bus_addr = resource->phys_addr;
		bars.bar[bars.num_bars].len = resource->len;
		bars.num_bars++;
	}

	return eth_ionic_dev_probe((void *)pci_dev,
			&pci_dev->device,
			&bars,
			&ionic_pci_intf,
			pci_dev->id.device_id,
			pci_dev->id.vendor_id);
}

static int
eth_ionic_pci_remove(struct rte_pci_device *pci_dev)
{
	return eth_ionic_dev_remove(&pci_dev->device);
}

static struct rte_pci_driver rte_pci_ionic_pmd = {
	.id_table = pci_id_ionic_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC |
			RTE_PCI_DRV_WC_ACTIVATE,
	.probe = eth_ionic_pci_probe,
	.remove = eth_ionic_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_ionic_pci, rte_pci_ionic_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_ionic_pci, pci_id_ionic_map);
RTE_PMD_REGISTER_KMOD_DEP(net_ionic_pci, "* igb_uio | uio_pci_generic | vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(net_ionic_pci,
	PMD_IONIC_CMB_KVARG "=<0|1>"
);
