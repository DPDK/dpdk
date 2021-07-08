/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <errno.h>
#include <rte_common.h>
#include <ethdev_pci.h>

static int
eth_ngbe_pci_probe(struct rte_pci_driver *pci_drv,
		struct rte_pci_device *pci_dev)
{
	RTE_SET_USED(pci_drv);
	RTE_SET_USED(pci_dev);
	return -EINVAL;
}

static int eth_ngbe_pci_remove(struct rte_pci_device *pci_dev)
{
	RTE_SET_USED(pci_dev);
	return -EINVAL;
}

static struct rte_pci_driver rte_ngbe_pmd = {
	.probe = eth_ngbe_pci_probe,
	.remove = eth_ngbe_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_ngbe, rte_ngbe_pmd);
