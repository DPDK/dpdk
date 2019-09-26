/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2019 Hisilicon Limited.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_dev.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_io.h>
#include <rte_log.h>
#include <rte_pci.h>

#include "hns3_ethdev.h"
#include "hns3_logs.h"
#include "hns3_regs.h"

int hns3_logtype_init;
int hns3_logtype_driver;

static void
hns3_dev_close(struct rte_eth_dev *eth_dev)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	hw->adapter_state = HNS3_NIC_CLOSED;
}

static const struct eth_dev_ops hns3_eth_dev_ops = {
	.dev_close          = hns3_dev_close,
};

static int
hns3_dev_init(struct rte_eth_dev *eth_dev)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	PMD_INIT_FUNC_TRACE();

	eth_dev->dev_ops = &hns3_eth_dev_ops;
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	hns->is_vf = false;
	hw->data = eth_dev->data;
	hw->adapter_state = HNS3_NIC_INITIALIZED;
	/*
	 * Pass the information to the rte_eth_dev_close() that it should also
	 * release the private port resources.
	 */
	eth_dev->data->dev_flags |= RTE_ETH_DEV_CLOSE_REMOVE;

	return 0;
}

static int
hns3_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -EPERM;

	eth_dev->dev_ops = NULL;
	eth_dev->rx_pkt_burst = NULL;
	eth_dev->tx_pkt_burst = NULL;
	eth_dev->tx_pkt_prepare = NULL;
	if (hw->adapter_state < HNS3_NIC_CLOSING)
		hns3_dev_close(eth_dev);

	hw->adapter_state = HNS3_NIC_REMOVED;
	return 0;
}

static int
eth_hns3_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		   struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
					     sizeof(struct hns3_adapter),
					     hns3_dev_init);
}

static int
eth_hns3_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, hns3_dev_uninit);
}

static const struct rte_pci_id pci_id_hns3_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HNS3_DEV_ID_GE) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HNS3_DEV_ID_25GE) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HNS3_DEV_ID_25GE_RDMA) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HNS3_DEV_ID_50GE_RDMA) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HNS3_DEV_ID_100G_RDMA_MACSEC) },
	{ .vendor_id = 0, /* sentinel */ },
};

static struct rte_pci_driver rte_hns3_pmd = {
	.id_table = pci_id_hns3_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = eth_hns3_pci_probe,
	.remove = eth_hns3_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_hns3, rte_hns3_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_hns3, pci_id_hns3_map);
RTE_PMD_REGISTER_KMOD_DEP(net_hns3, "* igb_uio | vfio-pci");

RTE_INIT(hns3_init_log)
{
	hns3_logtype_init = rte_log_register("pmd.net.hns3.init");
	if (hns3_logtype_init >= 0)
		rte_log_set_level(hns3_logtype_init, RTE_LOG_NOTICE);
	hns3_logtype_driver = rte_log_register("pmd.net.hns3.driver");
	if (hns3_logtype_driver >= 0)
		rte_log_set_level(hns3_logtype_driver, RTE_LOG_NOTICE);
}
