/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#include "nbl_ethdev.h"
#include "nbl_dev.h"

RTE_LOG_REGISTER_SUFFIX(nbl_logtype_init, init, INFO);
RTE_LOG_REGISTER_SUFFIX(nbl_logtype_driver, driver, INFO);

static int nbl_dev_release_pf(struct rte_eth_dev *eth_dev)
{
	struct nbl_adapter *adapter = ETH_DEV_TO_NBL_DEV_PF_PRIV(eth_dev);

	if (!adapter)
		return -EINVAL;
	NBL_LOG(DEBUG, "start to close device %s", eth_dev->device->name);
	nbl_dev_port_close(eth_dev);
	nbl_core_stop(adapter);
	nbl_core_remove(adapter);
	return 0;
}

static int nbl_dev_close(struct rte_eth_dev *eth_dev)
{
	return nbl_dev_release_pf(eth_dev);
}

const struct eth_dev_ops nbl_eth_dev_ops = {
	.dev_configure = nbl_dev_configure,
	.dev_start = nbl_dev_port_start,
	.dev_stop = nbl_dev_port_stop,
	.dev_close = nbl_dev_close,
	.tx_queue_setup = nbl_tx_queue_setup,
	.rx_queue_setup = nbl_rx_queue_setup,
	.tx_queue_release = nbl_tx_queues_release,
	.rx_queue_release = nbl_rx_queues_release,
	.dev_infos_get = nbl_dev_infos_get,
	.link_update = nbl_link_update,
	.stats_get = nbl_stats_get,
};

static int nbl_eth_dev_init(struct rte_eth_dev *eth_dev)
{
	struct nbl_adapter *adapter = ETH_DEV_TO_NBL_DEV_PF_PRIV(eth_dev);
	int ret;

	PMD_INIT_FUNC_TRACE();
	ret = nbl_core_init(adapter, eth_dev);
	if (ret) {
		NBL_LOG(ERR, "core init failed ret %d", ret);
		goto eth_init_failed;
	}

	ret = nbl_core_start(adapter);
	if (ret) {
		NBL_LOG(ERR, "core start failed ret %d", ret);
		nbl_core_remove(adapter);
		goto eth_init_failed;
	}

	eth_dev->dev_ops = &nbl_eth_dev_ops;
	return 0;

eth_init_failed:
	return ret;
}

/**
 * @brief: nbl device pci probe
 * @param[in]: {rte_pci_driver} *pci_drv
 * @param[in]: {rte_pci_device} *pci_dev
 * @return: {0-success,negative-fail}
 */
static int nbl_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
			 struct rte_pci_device *pci_dev)
{
	NBL_LOG(DEBUG, "nbl probe device %s", pci_dev->name);
	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		NBL_LOG(ERR, "Secondary process is not supported.");
		return -ENOTSUP;
	}
	return rte_eth_dev_pci_generic_probe(pci_dev, sizeof(struct nbl_adapter),
					     nbl_eth_dev_init);
}

static int nbl_eth_dev_uninit(struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();
	return nbl_dev_close(eth_dev);
}

static int nbl_pci_remove(struct rte_pci_device *pci_dev)
{
	PMD_INIT_FUNC_TRACE();
	return rte_eth_dev_pci_generic_remove(pci_dev, nbl_eth_dev_uninit);
}

static const struct rte_pci_id pci_id_nbl_map[] = {
	{ RTE_PCI_DEVICE(NBL_VENDOR_ID, NBL_DEVICE_ID_M18110) },
	{ RTE_PCI_DEVICE(NBL_VENDOR_ID, NBL_DEVICE_ID_M18110_LX) },
	{ RTE_PCI_DEVICE(NBL_VENDOR_ID, NBL_DEVICE_ID_M18110_BASE_T) },
	{ RTE_PCI_DEVICE(NBL_VENDOR_ID, NBL_DEVICE_ID_M18110_LX_BASE_T) },
	{ RTE_PCI_DEVICE(NBL_VENDOR_ID, NBL_DEVICE_ID_M18110_OCP) },
	{ RTE_PCI_DEVICE(NBL_VENDOR_ID, NBL_DEVICE_ID_M18110_LX_OCP) },
	{ RTE_PCI_DEVICE(NBL_VENDOR_ID, NBL_DEVICE_ID_M18110_BASE_T_OCP) },
	{ RTE_PCI_DEVICE(NBL_VENDOR_ID, NBL_DEVICE_ID_M18110_LX_BASE_T_OCP) },
	{ RTE_PCI_DEVICE(NBL_VENDOR_ID, NBL_DEVICE_ID_M18120) },
	{ RTE_PCI_DEVICE(NBL_VENDOR_ID, NBL_DEVICE_ID_M18120_LX) },
	{ RTE_PCI_DEVICE(NBL_VENDOR_ID, NBL_DEVICE_ID_M18120_BASE_T) },
	{ RTE_PCI_DEVICE(NBL_VENDOR_ID, NBL_DEVICE_ID_M18120_LX_BASE_T) },
	{ RTE_PCI_DEVICE(NBL_VENDOR_ID, NBL_DEVICE_ID_M18120_OCP) },
	{ RTE_PCI_DEVICE(NBL_VENDOR_ID, NBL_DEVICE_ID_M18120_LX_OCP) },
	{ RTE_PCI_DEVICE(NBL_VENDOR_ID, NBL_DEVICE_ID_M18120_BASE_T_OCP) },
	{ RTE_PCI_DEVICE(NBL_VENDOR_ID, NBL_DEVICE_ID_M18120_LX_BASE_T_OCP) },
	{ RTE_PCI_DEVICE(NBL_VENDOR_ID, NBL_DEVICE_ID_M18100_VF) },
	{ .vendor_id = 0, /* sentinel */ },
};

static struct rte_pci_driver nbl_pmd = {
	.id_table = pci_id_nbl_map,
	.drv_flags =
		RTE_PCI_DRV_INTR_LSC |
		RTE_PCI_DRV_PROBE_AGAIN,
	.probe = nbl_pci_probe,
	.remove = nbl_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_nbl, nbl_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_nbl, pci_id_nbl_map);
RTE_PMD_REGISTER_KMOD_DEP(net_nbl, "* nbl_core");
