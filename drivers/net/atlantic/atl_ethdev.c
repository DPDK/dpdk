/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Aquantia Corporation
 */

#include <rte_ethdev_pci.h>

#include "atl_ethdev.h"
#include "atl_common.h"

static int eth_atl_dev_init(struct rte_eth_dev *eth_dev);
static int eth_atl_dev_uninit(struct rte_eth_dev *eth_dev);

static int  atl_dev_configure(struct rte_eth_dev *dev);
static int  atl_dev_start(struct rte_eth_dev *dev);
static void atl_dev_stop(struct rte_eth_dev *dev);
static void atl_dev_close(struct rte_eth_dev *dev);
static int  atl_dev_reset(struct rte_eth_dev *dev);

static int eth_atl_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev);
static int eth_atl_pci_remove(struct rte_pci_device *pci_dev);

static void atl_dev_info_get(struct rte_eth_dev *dev,
				struct rte_eth_dev_info *dev_info);

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_atl_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_0001) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_D100) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_D107) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_D108) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_D109) },

	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC100) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC107) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC108) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC109) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC111) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC112) },

	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC100S) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC107S) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC108S) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC109S) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC111S) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC112S) },

	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC111E) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC112E) },
	{ .vendor_id = 0, /* sentinel */ },
};

static struct rte_pci_driver rte_atl_pmd = {
	.id_table = pci_id_atl_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING |
		     RTE_PCI_DRV_IOVA_AS_VA,
	.probe = eth_atl_pci_probe,
	.remove = eth_atl_pci_remove,
};

static const struct eth_dev_ops atl_eth_dev_ops = {
	.dev_configure	      = atl_dev_configure,
	.dev_start	      = atl_dev_start,
	.dev_stop	      = atl_dev_stop,
	.dev_close	      = atl_dev_close,
	.dev_reset	      = atl_dev_reset,
	.dev_infos_get        = atl_dev_info_get,
};

static int
eth_atl_dev_init(struct rte_eth_dev *eth_dev)
{
	eth_dev->dev_ops = &atl_eth_dev_ops;

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("atlantic", ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL)
		return -ENOMEM;

	return 0;
}

static int
eth_atl_dev_uninit(struct rte_eth_dev *eth_dev)
{
	rte_free(eth_dev->data->mac_addrs);

	return 0;
}

static int
eth_atl_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct atl_adapter), eth_atl_dev_init);
}

static int
eth_atl_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, eth_atl_dev_uninit);
}

static int
atl_dev_configure(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

/*
 * Configure device link speed and setup link.
 * It returns 0 on success.
 */
static int
atl_dev_start(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

/*
 * Stop device: disable rx and tx functions to allow for reconfiguring.
 */
static void
atl_dev_stop(struct rte_eth_dev *dev __rte_unused)
{
}

/*
 * Reset and stop device.
 */
static void
atl_dev_close(struct rte_eth_dev *dev __rte_unused)
{
}

static int
atl_dev_reset(struct rte_eth_dev *dev)
{
	int ret;

	ret = eth_atl_dev_uninit(dev);
	if (ret)
		return ret;

	ret = eth_atl_dev_init(dev);

	return ret;
}

static void
atl_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	dev_info->max_rx_queues = 0;
	dev_info->max_rx_queues = 0;

	dev_info->max_vfs = pci_dev->max_vfs;

	dev_info->max_hash_mac_addrs = 0;
	dev_info->max_vmdq_pools = 0;
	dev_info->vmdq_queue_num = 0;
}

RTE_PMD_REGISTER_PCI(net_atlantic, rte_atl_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_atlantic, pci_id_atl_map);
RTE_PMD_REGISTER_KMOD_DEP(net_atlantic, "* igb_uio | uio_pci_generic");
