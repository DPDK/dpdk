/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_ethdev.h>
#include <rte_ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_ethdev_pci.h>

#include "ionic_logs.h"
#include "ionic.h"
#include "ionic_dev.h"
#include "ionic_mac_api.h"
#include "ionic_lif.h"
#include "ionic_ethdev.h"

static int  eth_ionic_dev_init(struct rte_eth_dev *eth_dev, void *init_params);
static int  eth_ionic_dev_uninit(struct rte_eth_dev *eth_dev);

int ionic_logtype;

static const struct rte_pci_id pci_id_ionic_map[] = {
	{ RTE_PCI_DEVICE(IONIC_PENSANDO_VENDOR_ID, IONIC_DEV_ID_ETH_PF) },
	{ RTE_PCI_DEVICE(IONIC_PENSANDO_VENDOR_ID, IONIC_DEV_ID_ETH_VF) },
	{ RTE_PCI_DEVICE(IONIC_PENSANDO_VENDOR_ID, IONIC_DEV_ID_ETH_MGMT) },
	{ .vendor_id = 0, /* sentinel */ },
};

static const struct eth_dev_ops ionic_eth_dev_ops = {
};

/**
 * Interrupt handler triggered by NIC for handling
 * specific interrupt.
 *
 * @param param
 *  The address of parameter registered before.
 *
 * @return
 *  void
 */
static void
ionic_dev_interrupt_handler(void *param)
{
	struct ionic_adapter *adapter = (struct ionic_adapter *)param;
	uint32_t i;

	IONIC_PRINT(DEBUG, "->");

	for (i = 0; i < adapter->nlifs; i++) {
		if (adapter->lifs[i])
			ionic_notifyq_handler(adapter->lifs[i], -1);
	}
}

static int
eth_ionic_dev_init(struct rte_eth_dev *eth_dev, void *init_params)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	struct ionic_adapter *adapter = (struct ionic_adapter *)init_params;
	int err;

	IONIC_PRINT_CALL();

	eth_dev->dev_ops = &ionic_eth_dev_ops;

	/* Multi-process not supported, primary does initialization anyway */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	rte_eth_copy_pci_info(eth_dev, pci_dev);

	lif->index = adapter->nlifs;
	lif->eth_dev = eth_dev;
	lif->adapter = adapter;
	adapter->lifs[adapter->nlifs] = lif;

	err = ionic_lif_alloc(lif);
	if (err) {
		IONIC_PRINT(ERR, "Cannot allocate LIFs: %d, aborting",
			err);
		goto err;
	}

	err = ionic_lif_init(lif);
	if (err) {
		IONIC_PRINT(ERR, "Cannot init LIFs: %d, aborting", err);
		goto err_free_lif;
	}

	IONIC_PRINT(DEBUG, "Port %u initialized", eth_dev->data->port_id);

	return 0;

err_free_lif:
	ionic_lif_free(lif);
err:
	return err;
}

static int
eth_ionic_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	struct ionic_adapter *adapter = lif->adapter;

	IONIC_PRINT_CALL();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	adapter->lifs[lif->index] = NULL;

	ionic_lif_deinit(lif);
	ionic_lif_free(lif);

	eth_dev->dev_ops = NULL;

	return 0;
}

static int
ionic_configure_intr(struct ionic_adapter *adapter)
{
	struct rte_pci_device *pci_dev = adapter->pci_dev;
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	int err;

	IONIC_PRINT(DEBUG, "Configuring %u intrs", adapter->nintrs);

	if (rte_intr_efd_enable(intr_handle, adapter->nintrs)) {
		IONIC_PRINT(ERR, "Fail to create eventfd");
		return -1;
	}

	if (rte_intr_dp_is_en(intr_handle))
		IONIC_PRINT(DEBUG,
			"Packet I/O interrupt on datapath is enabled");

	if (!intr_handle->intr_vec) {
		intr_handle->intr_vec = rte_zmalloc("intr_vec",
			adapter->nintrs * sizeof(int), 0);

		if (!intr_handle->intr_vec) {
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
			"Failure registering interrupts handler (%d)",
			err);
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
ionic_unconfigure_intr(struct ionic_adapter *adapter)
{
	struct rte_pci_device *pci_dev = adapter->pci_dev;
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;

	rte_intr_disable(intr_handle);

	rte_intr_callback_unregister(intr_handle,
		ionic_dev_interrupt_handler,
		adapter);
}

static int
eth_ionic_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	char name[RTE_ETH_NAME_MAX_LEN];
	struct rte_mem_resource *resource;
	struct ionic_adapter *adapter;
	struct ionic_hw *hw;
	unsigned long i;
	int err;

	/* Check structs (trigger error at compilation time) */
	ionic_struct_size_checks();

	/* Multi-process not supported */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		err = -EPERM;
		goto err;
	}

	IONIC_PRINT(DEBUG, "Initializing device %s",
		pci_dev->device.name);

	adapter = rte_zmalloc("ionic", sizeof(*adapter), 0);
	if (!adapter) {
		IONIC_PRINT(ERR, "OOM");
		err = -ENOMEM;
		goto err;
	}

	adapter->pci_dev = pci_dev;
	hw = &adapter->hw;

	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;

	err = ionic_init_mac(hw);
	if (err != 0) {
		IONIC_PRINT(ERR, "Mac init failed: %d", err);
		err = -EIO;
		goto err_free_adapter;
	}

	adapter->is_mgmt_nic = (pci_dev->id.device_id == IONIC_DEV_ID_ETH_MGMT);

	adapter->num_bars = 0;
	for (i = 0; i < PCI_MAX_RESOURCE && i < IONIC_BARS_MAX; i++) {
		resource = &pci_dev->mem_resource[i];
		if (resource->phys_addr == 0 || resource->len == 0)
			continue;
		adapter->bars[adapter->num_bars].vaddr = resource->addr;
		adapter->bars[adapter->num_bars].bus_addr = resource->phys_addr;
		adapter->bars[adapter->num_bars].len = resource->len;
		adapter->num_bars++;
	}

	/* Discover ionic dev resources */

	err = ionic_setup(adapter);
	if (err) {
		IONIC_PRINT(ERR, "Cannot setup device: %d, aborting", err);
		goto err_free_adapter;
	}

	err = ionic_identify(adapter);
	if (err) {
		IONIC_PRINT(ERR, "Cannot identify device: %d, aborting",
			err);
		goto err_free_adapter;
	}

	err = ionic_init(adapter);
	if (err) {
		IONIC_PRINT(ERR, "Cannot init device: %d, aborting", err);
		goto err_free_adapter;
	}

	/* Configure the ports */
	err = ionic_port_identify(adapter);
	if (err) {
		IONIC_PRINT(ERR, "Cannot identify port: %d, aborting",
			err);
		goto err_free_adapter;
	}

	err = ionic_port_init(adapter);
	if (err) {
		IONIC_PRINT(ERR, "Cannot init port: %d, aborting", err);
		goto err_free_adapter;
	}

	/* Configure LIFs */
	err = ionic_lif_identify(adapter);
	if (err) {
		IONIC_PRINT(ERR, "Cannot identify lif: %d, aborting", err);
		goto err_free_adapter;
	}

	/* Allocate and init LIFs */
	err = ionic_lifs_size(adapter);
	if (err) {
		IONIC_PRINT(ERR, "Cannot size LIFs: %d, aborting", err);
		goto err_free_adapter;
	}

	adapter->nlifs = 0;
	for (i = 0; i < adapter->ident.dev.nlifs; i++) {
		snprintf(name, sizeof(name), "net_%s_lif_%lu",
			pci_dev->device.name, i);

		err = rte_eth_dev_create(&pci_dev->device, name,
			sizeof(struct ionic_lif),
			NULL, NULL,
			eth_ionic_dev_init, adapter);
		if (err) {
			IONIC_PRINT(ERR, "Cannot create eth device for "
				"ionic lif %s", name);
			break;
		}

		adapter->nlifs++;
	}

	err = ionic_configure_intr(adapter);

	if (err) {
		IONIC_PRINT(ERR, "Failed to configure interrupts");
		goto err_free_adapter;
	}

	return 0;

err_free_adapter:
	rte_free(adapter);
err:
	return err;
}

static int
eth_ionic_pci_remove(struct rte_pci_device *pci_dev __rte_unused)
{
	char name[RTE_ETH_NAME_MAX_LEN];
	struct ionic_adapter *adapter = NULL;
	struct rte_eth_dev *eth_dev;
	struct ionic_lif *lif;
	uint32_t i;

	/* Adapter lookup is using (the first) eth_dev name */
	snprintf(name, sizeof(name), "net_%s_lif_0",
		pci_dev->device.name);

	eth_dev = rte_eth_dev_allocated(name);
	if (eth_dev) {
		lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
		adapter = lif->adapter;
	}

	if (adapter) {
		ionic_unconfigure_intr(adapter);

		for (i = 0; i < adapter->nlifs; i++) {
			lif = adapter->lifs[i];
			rte_eth_dev_destroy(lif->eth_dev, eth_ionic_dev_uninit);
		}

		rte_free(adapter);
	}

	return 0;
}

static struct rte_pci_driver rte_ionic_pmd = {
	.id_table = pci_id_ionic_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = eth_ionic_pci_probe,
	.remove = eth_ionic_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_ionic, rte_ionic_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_ionic, pci_id_ionic_map);
RTE_PMD_REGISTER_KMOD_DEP(net_ionic, "* igb_uio | uio_pci_generic | vfio-pci");

RTE_INIT(ionic_init_log)
{
	ionic_logtype = rte_log_register("pmd.net.ionic");
	if (ionic_logtype >= 0)
		rte_log_set_level(ionic_logtype, RTE_LOG_NOTICE);
}
