/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#include <ethdev_pci.h>
#include <bus_pci_driver.h>
#include <rte_ethdev.h>

#include "zxdh_ethdev.h"
#include "zxdh_logs.h"
#include "zxdh_pci.h"
#include "zxdh_msg.h"

struct zxdh_hw_internal zxdh_hw_internal[RTE_MAX_ETHPORTS];

static int
zxdh_dev_close(struct rte_eth_dev *dev __rte_unused)
{
	int ret = 0;

	return ret;
}

static int32_t
zxdh_init_device(struct rte_eth_dev *eth_dev)
{
	struct zxdh_hw *hw = eth_dev->data->dev_private;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	int ret = 0;

	ret = zxdh_read_pci_caps(pci_dev, hw);
	if (ret) {
		PMD_DRV_LOG(ERR, "port 0x%x pci caps read failed .", hw->port_id);
		goto err;
	}

	zxdh_hw_internal[hw->port_id].zxdh_vtpci_ops = &zxdh_dev_pci_ops;
	zxdh_pci_reset(hw);
	zxdh_get_pci_dev_config(hw);

	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac_addr, &eth_dev->data->mac_addrs[0]);

	/* If host does not support both status and MSI-X then disable LSC */
	if (vtpci_with_feature(hw, ZXDH_NET_F_STATUS) && hw->use_msix != ZXDH_MSIX_NONE)
		eth_dev->data->dev_flags |= RTE_ETH_DEV_INTR_LSC;
	else
		eth_dev->data->dev_flags &= ~RTE_ETH_DEV_INTR_LSC;

	return 0;

err:
	PMD_DRV_LOG(ERR, "port %d init device failed", eth_dev->data->port_id);
	return ret;
}

static int
zxdh_eth_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct zxdh_hw *hw = eth_dev->data->dev_private;
	int ret = 0;

	eth_dev->dev_ops = NULL;

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("zxdh_mac",
			ZXDH_MAX_MAC_ADDRS * RTE_ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate %d bytes store MAC addresses",
				ZXDH_MAX_MAC_ADDRS * RTE_ETHER_ADDR_LEN);
		return -ENOMEM;
	}

	memset(hw, 0, sizeof(*hw));
	hw->bar_addr[0] = (uint64_t)pci_dev->mem_resource[0].addr;
	if (hw->bar_addr[0] == 0) {
		PMD_DRV_LOG(ERR, "Bad mem resource.");
		return -EIO;
	}

	hw->device_id = pci_dev->id.device_id;
	hw->port_id = eth_dev->data->port_id;
	hw->eth_dev = eth_dev;
	hw->speed = RTE_ETH_SPEED_NUM_UNKNOWN;
	hw->duplex = RTE_ETH_LINK_FULL_DUPLEX;
	hw->is_pf = 0;

	if (pci_dev->id.device_id == ZXDH_E310_PF_DEVICEID ||
		pci_dev->id.device_id == ZXDH_E312_PF_DEVICEID) {
		hw->is_pf = 1;
	}

	ret = zxdh_init_device(eth_dev);
	if (ret < 0)
		goto err_zxdh_init;

	ret = zxdh_msg_chan_init();
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to init bar msg chan");
		goto err_zxdh_init;
	}
	hw->msg_chan_init = 1;

	ret = zxdh_msg_chan_hwlock_init(eth_dev);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "zxdh_msg_chan_hwlock_init failed ret %d", ret);
		goto err_zxdh_init;
	}

	ret = zxdh_msg_chan_enable(eth_dev);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "zxdh_msg_bar_chan_enable failed ret %d", ret);
		goto err_zxdh_init;
	}

	return ret;

err_zxdh_init:
	zxdh_bar_msg_chan_exit();
	rte_free(eth_dev->data->mac_addrs);
	eth_dev->data->mac_addrs = NULL;
	return ret;
}

static int
zxdh_eth_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
						sizeof(struct zxdh_hw),
						zxdh_eth_dev_init);
}

static int
zxdh_eth_dev_uninit(struct rte_eth_dev *eth_dev)
{
	int ret = 0;

	ret = zxdh_dev_close(eth_dev);

	return ret;
}

static int
zxdh_eth_pci_remove(struct rte_pci_device *pci_dev)
{
	int ret = rte_eth_dev_pci_generic_remove(pci_dev, zxdh_eth_dev_uninit);

	return ret;
}

static const struct rte_pci_id pci_id_zxdh_map[] = {
	{RTE_PCI_DEVICE(ZXDH_PCI_VENDOR_ID, ZXDH_E310_PF_DEVICEID)},
	{RTE_PCI_DEVICE(ZXDH_PCI_VENDOR_ID, ZXDH_E310_VF_DEVICEID)},
	{RTE_PCI_DEVICE(ZXDH_PCI_VENDOR_ID, ZXDH_E312_PF_DEVICEID)},
	{RTE_PCI_DEVICE(ZXDH_PCI_VENDOR_ID, ZXDH_E312_VF_DEVICEID)},
	{.vendor_id = 0, /* sentinel */ },
};
static struct rte_pci_driver zxdh_pmd = {
	.id_table = pci_id_zxdh_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = zxdh_eth_pci_probe,
	.remove = zxdh_eth_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_zxdh, zxdh_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_zxdh, pci_id_zxdh_map);
RTE_PMD_REGISTER_KMOD_DEP(net_zxdh, "* vfio-pci");
RTE_LOG_REGISTER_SUFFIX(zxdh_logtype_driver, driver, NOTICE);
RTE_LOG_REGISTER_SUFFIX(zxdh_logtype_rx, rx, NOTICE);
RTE_LOG_REGISTER_SUFFIX(zxdh_logtype_tx, tx, NOTICE);
RTE_LOG_REGISTER_SUFFIX(zxdh_logtype_msg, msg, NOTICE);
