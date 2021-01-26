/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_errno.h>

#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_dev.h>
#include <rte_kvargs.h>

#include "virtio_ethdev.h"
#include "virtio_pci.h"
#include "virtio_logs.h"

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_virtio_map[] = {
	{ RTE_PCI_DEVICE(VIRTIO_PCI_VENDORID, VIRTIO_PCI_LEGACY_DEVICEID_NET) },
	{ RTE_PCI_DEVICE(VIRTIO_PCI_VENDORID, VIRTIO_PCI_MODERN_DEVICEID_NET) },
	{ .vendor_id = 0, /* sentinel */ },
};

static int
eth_virtio_pci_init(struct rte_eth_dev *eth_dev)
{
	return eth_virtio_dev_init(eth_dev);
}

static int
eth_virtio_pci_uninit(struct rte_eth_dev *eth_dev)
{
	int ret;
	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		return 0;

	ret = virtio_dev_stop(eth_dev);
	virtio_dev_close(eth_dev);

	PMD_INIT_LOG(DEBUG, "dev_uninit completed");

	return ret;
}

static int vdpa_check_handler(__rte_unused const char *key,
		const char *value, void *ret_val)
{
	if (strcmp(value, "1") == 0)
		*(int *)ret_val = 1;
	else
		*(int *)ret_val = 0;

	return 0;
}

#define VIRTIO_ARG_VDPA       "vdpa"

static int
virtio_pci_devargs_parse(struct rte_devargs *devargs, int *vdpa)
{
	struct rte_kvargs *kvlist;
	int ret = 0;

	if (devargs == NULL)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL) {
		PMD_INIT_LOG(ERR, "error when parsing param");
		return 0;
	}

	if (rte_kvargs_count(kvlist, VIRTIO_ARG_VDPA) == 1) {
		/* vdpa mode selected when there's a key-value pair:
		 * vdpa=1
		 */
		ret = rte_kvargs_process(kvlist, VIRTIO_ARG_VDPA,
				vdpa_check_handler, vdpa);
		if (ret < 0)
			PMD_INIT_LOG(ERR, "Failed to parse %s", VIRTIO_ARG_VDPA);
	}

	rte_kvargs_free(kvlist);

	return ret;
}

static int eth_virtio_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	int vdpa = 0;
	int ret = 0;

	ret = virtio_pci_devargs_parse(pci_dev->device.devargs, &vdpa);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "devargs parsing is failed");
		return ret;
	}
	/* virtio pmd skips probe if device needs to work in vdpa mode */
	if (vdpa == 1)
		return 1;

	return rte_eth_dev_pci_generic_probe(pci_dev, sizeof(struct virtio_pci_dev),
		eth_virtio_pci_init);
}

static int eth_virtio_pci_remove(struct rte_pci_device *pci_dev)
{
	int ret;

	ret = rte_eth_dev_pci_generic_remove(pci_dev, eth_virtio_pci_uninit);
	/* Port has already been released by close. */
	if (ret == -ENODEV)
		ret = 0;
	return ret;
}

static struct rte_pci_driver rte_virtio_net_pci_pmd = {
	.driver = {
		.name = "net_virtio",
	},
	.id_table = pci_id_virtio_map,
	.drv_flags = 0,
	.probe = eth_virtio_pci_probe,
	.remove = eth_virtio_pci_remove,
};

RTE_INIT(rte_virtio_net_pci_pmd_init)
{
	rte_eal_iopl_init();
	rte_pci_register(&rte_virtio_net_pci_pmd);
}

RTE_PMD_REGISTER_PCI_TABLE(net_virtio, pci_id_virtio_map);
RTE_PMD_REGISTER_KMOD_DEP(net_virtio, "* igb_uio | uio_pci_generic | vfio-pci");
RTE_PMD_EXPORT_NAME(net_virtio, __COUNTER__);
