/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <rte_ethdev.h>
#include <rte_bus_pci.h>
#include <ethdev_pci.h>

static const struct rte_pci_id nthw_pci_id_map[] = {
	{
		.vendor_id = 0,
	},	/* sentinel */
};

static int
nthw_pci_dev_init(struct rte_pci_device *pci_dev __rte_unused)
{
	return 0;
}

static int
nthw_pci_dev_deinit(struct rte_eth_dev *eth_dev __rte_unused)
{
	return 0;
}

static int
nthw_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	int ret;
	ret = nthw_pci_dev_init(pci_dev);
	return ret;
}

static int
nthw_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, nthw_pci_dev_deinit);
}

static struct rte_pci_driver rte_nthw_pmd = {
	.id_table = nthw_pci_id_map,
	.probe = nthw_pci_probe,
	.remove = nthw_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_ntnic, rte_nthw_pmd);
