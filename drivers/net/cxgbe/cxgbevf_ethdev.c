/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Chelsio Communications.
 * All rights reserved.
 */

#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>

#include "cxgbe.h"
#include "cxgbe_pfvf.h"

/*
 * Macros needed to support the PCI Device ID Table ...
 */
#define CH_PCI_DEVICE_ID_TABLE_DEFINE_BEGIN \
	static const struct rte_pci_id cxgb4vf_pci_tbl[] = {
#define CH_PCI_DEVICE_ID_FUNCTION 0x8

#define PCI_VENDOR_ID_CHELSIO 0x1425

#define CH_PCI_ID_TABLE_ENTRY(devid) \
		{ RTE_PCI_DEVICE(PCI_VENDOR_ID_CHELSIO, (devid)) }

#define CH_PCI_DEVICE_ID_TABLE_DEFINE_END \
		{ .vendor_id = 0, } \
	}

/*
 *... and the PCI ID Table itself ...
 */
#include "t4_pci_id_tbl.h"

static const struct eth_dev_ops cxgbevf_eth_dev_ops = {
	.dev_start              = cxgbe_dev_start,
	.dev_stop               = cxgbe_dev_stop,
	.dev_close              = cxgbe_dev_close,
	.promiscuous_enable     = cxgbe_dev_promiscuous_enable,
	.promiscuous_disable    = cxgbe_dev_promiscuous_disable,
	.allmulticast_enable    = cxgbe_dev_allmulticast_enable,
	.allmulticast_disable   = cxgbe_dev_allmulticast_disable,
	.dev_configure          = cxgbe_dev_configure,
	.dev_infos_get          = cxgbe_dev_info_get,
	.dev_supported_ptypes_get = cxgbe_dev_supported_ptypes_get,
	.link_update            = cxgbe_dev_link_update,
	.mtu_set                = cxgbe_dev_mtu_set,
	.tx_queue_setup         = cxgbe_dev_tx_queue_setup,
	.tx_queue_start         = cxgbe_dev_tx_queue_start,
	.tx_queue_stop          = cxgbe_dev_tx_queue_stop,
	.tx_queue_release       = cxgbe_dev_tx_queue_release,
	.rx_queue_setup         = cxgbe_dev_rx_queue_setup,
	.rx_queue_start         = cxgbe_dev_rx_queue_start,
	.rx_queue_stop          = cxgbe_dev_rx_queue_stop,
	.rx_queue_release       = cxgbe_dev_rx_queue_release,
};

/*
 * Initialize driver
 * It returns 0 on success.
 */
static int eth_cxgbevf_dev_init(struct rte_eth_dev *eth_dev)
{
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	struct rte_pci_device *pci_dev;
	char name[RTE_ETH_NAME_MAX_LEN];
	struct adapter *adapter = NULL;
	int err = 0;

	CXGBE_FUNC_TRACE();

	eth_dev->dev_ops = &cxgbevf_eth_dev_ops;
	eth_dev->rx_pkt_burst = &cxgbe_recv_pkts;
	eth_dev->tx_pkt_burst = &cxgbe_xmit_pkts;
	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	/* for secondary processes, we attach to ethdevs allocated by primary
	 * and do minimal initialization.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		int i;

		for (i = 1; i < MAX_NPORTS; i++) {
			struct rte_eth_dev *rest_eth_dev;
			char namei[RTE_ETH_NAME_MAX_LEN];

			snprintf(namei, sizeof(namei), "%s_%d",
				 pci_dev->device.name, i);
			rest_eth_dev = rte_eth_dev_attach_secondary(namei);
			if (rest_eth_dev) {
				rest_eth_dev->device = &pci_dev->device;
				rest_eth_dev->dev_ops =
					eth_dev->dev_ops;
				rest_eth_dev->rx_pkt_burst =
					eth_dev->rx_pkt_burst;
				rest_eth_dev->tx_pkt_burst =
					eth_dev->tx_pkt_burst;
			}
		}
		return 0;
	}

	snprintf(name, sizeof(name), "cxgbevfadapter%d",
		 eth_dev->data->port_id);
	adapter = rte_zmalloc(name, sizeof(*adapter), 0);
	if (!adapter)
		return -1;

	adapter->use_unpacked_mode = 1;
	adapter->regs = (void *)pci_dev->mem_resource[0].addr;
	if (!adapter->regs) {
		dev_err(adapter, "%s: cannot map device registers\n", __func__);
		err = -ENOMEM;
		goto out_free_adapter;
	}
	adapter->pdev = pci_dev;
	adapter->eth_dev = eth_dev;
	pi->adapter = adapter;
	err = cxgbevf_probe(adapter);
	if (err) {
		dev_err(adapter, "%s: cxgbevf probe failed with err %d\n",
			__func__, err);
		goto out_free_adapter;
	}

	return 0;

out_free_adapter:
	rte_free(adapter);
	return err;
}

static int eth_cxgbevf_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
				 struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev, sizeof(struct port_info),
					     eth_cxgbevf_dev_init);
}

static int eth_cxgbevf_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, NULL);
}

static struct rte_pci_driver rte_cxgbevf_pmd = {
	.id_table = cxgb4vf_pci_tbl,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = eth_cxgbevf_pci_probe,
	.remove = eth_cxgbevf_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_cxgbevf, rte_cxgbevf_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_cxgbevf, cxgb4vf_pci_tbl);
RTE_PMD_REGISTER_KMOD_DEP(net_cxgbevf, "* igb_uio | vfio-pci");
