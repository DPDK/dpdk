/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_errno.h>
#include <rte_ether.h>

#include "base/hinic3_compat.h"
#include "base/hinic3_csr.h"
#include "base/hinic3_wq.h"
#include "base/hinic3_eqs.h"
#include "base/hinic3_cmdq.h"
#include "base/hinic3_hwdev.h"
#include "base/hinic3_hwif.h"
#include "base/hinic3_hw_cfg.h"
#include "base/hinic3_hw_comm.h"
#include "base/hinic3_nic_cfg.h"
#include "base/hinic3_nic_event.h"
#include "hinic3_ethdev.h"

/**
 * Interrupt handler triggered by NIC for handling specific event.
 *
 * @param[in] param
 * The address of parameter (struct rte_eth_dev *) registered before.
 */
static void
hinic3_dev_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = param;
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	if (!hinic3_get_bit(HINIC3_DEV_INTR_EN, &nic_dev->dev_status)) {
		PMD_DRV_LOG(WARNING,
			    "Intr is disabled, ignore intr event, "
			    "dev_name: %s, port_id: %d",
			    nic_dev->dev_name, dev->data->port_id);
		return;
	}

	/* Aeq0 msg handler. */
	hinic3_dev_handle_aeq_event(nic_dev->hwdev, param);
}

static void
hinic3_deinit_sw_rxtxqs(struct hinic3_nic_dev *nic_dev)
{
	rte_free(nic_dev->txqs);
	nic_dev->txqs = NULL;

	rte_free(nic_dev->rxqs);
	nic_dev->rxqs = NULL;
}

/**
 * Init mac_vlan table in hardwares.
 *
 * @param[in] eth_dev
 * Pointer to ethernet device structure.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_init_mac_table(struct rte_eth_dev *eth_dev)
{
	struct hinic3_nic_dev *nic_dev =
		HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);
	u8 addr_bytes[RTE_ETHER_ADDR_LEN];
	u16 func_id = 0;
	int err = 0;

	err = hinic3_get_default_mac(nic_dev->hwdev, addr_bytes,
				     RTE_ETHER_ADDR_LEN);
	if (err)
		return err;

	rte_ether_addr_copy((struct rte_ether_addr *)addr_bytes,
			    &eth_dev->data->mac_addrs[0]);
	if (rte_is_zero_ether_addr(&eth_dev->data->mac_addrs[0]))
		rte_eth_random_addr(eth_dev->data->mac_addrs[0].addr_bytes);

	func_id = hinic3_global_func_id(nic_dev->hwdev);
	err = hinic3_set_mac(nic_dev->hwdev,
			     eth_dev->data->mac_addrs[0].addr_bytes, 0,
			     func_id);
	if (err && err != HINIC3_PF_SET_VF_ALREADY)
		return err;

	rte_ether_addr_copy(&eth_dev->data->mac_addrs[0],
			    &nic_dev->default_addr);

	return 0;
}

/**
 * Deinit mac_vlan table in hardware.
 *
 * @param[in] eth_dev
 * Pointer to ethernet device structure.
 */
static void
hinic3_deinit_mac_addr(struct rte_eth_dev *eth_dev)
{
	struct hinic3_nic_dev *nic_dev =
		HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);
	u16 func_id = 0;
	int err;
	int i;

	func_id = hinic3_global_func_id(nic_dev->hwdev);

	for (i = 0; i < HINIC3_MAX_UC_MAC_ADDRS; i++) {
		if (rte_is_zero_ether_addr(&eth_dev->data->mac_addrs[i]))
			continue;

		err = hinic3_del_mac(nic_dev->hwdev,
				     eth_dev->data->mac_addrs[i].addr_bytes, 0,
				     func_id);
		if (err && err != HINIC3_PF_SET_VF_ALREADY)
			PMD_DRV_LOG(ERR,
				    "Delete mac table failed, dev_name: %s",
				    eth_dev->data->name);

		memset(&eth_dev->data->mac_addrs[i], 0,
		       sizeof(struct rte_ether_addr));
	}

	/* Delete multicast mac addrs. */
	hinic3_delete_mc_addr_list(nic_dev);
}

/**
 * Check the valid CoS bitmap to determine the available CoS IDs and set
 * the default CoS ID to the highest valid one.
 *
 * @param[in] hwdev
 * Pointer to hardware device structure.
 * @param[out] cos_id
 * Pointer to store the default CoS ID.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_pf_get_default_cos(struct hinic3_hwdev *hwdev, u8 *cos_id)
{
	u8 default_cos = 0;
	u8 valid_cos_bitmap;
	u8 i;

	valid_cos_bitmap = hwdev->cfg_mgmt->svc_cap.cos_valid_bitmap;
	if (!valid_cos_bitmap) {
		PMD_DRV_LOG(ERR, "PF has none cos to support");
		return -EFAULT;
	}

	for (i = 0; i < HINIC3_COS_NUM_MAX; i++) {
		if (valid_cos_bitmap & BIT(i))
			/* Find max cos id as default cos. */
			default_cos = i;
	}

	*cos_id = default_cos;

	return 0;
}

static int
hinic3_init_default_cos(struct hinic3_nic_dev *nic_dev)
{
	u8 cos_id = 0;
	int err;

	if (!HINIC3_IS_VF(nic_dev->hwdev)) {
		err = hinic3_pf_get_default_cos(nic_dev->hwdev, &cos_id);
		if (err) {
			PMD_DRV_LOG(ERR, "Get PF default cos failed, err: %d",
				    err);
			return err;
		}
	} else {
		err = hinic3_vf_get_default_cos(nic_dev->hwdev, &cos_id);
		if (err) {
			PMD_DRV_LOG(ERR, "Get VF default cos failed, err: %d",
				    err);
			return err;
		}
	}

	nic_dev->default_cos = cos_id;
	PMD_DRV_LOG(INFO, "Default cos %d", nic_dev->default_cos);
	return 0;
}

/**
 * Initialize Class of Service (CoS). For PF devices, it also sync the link
 * status with the physical port.
 *
 * @param[in] nic_dev
 * Pointer to NIC device structure.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_set_default_hw_feature(struct hinic3_nic_dev *nic_dev)
{
	int err;

	err = hinic3_init_default_cos(nic_dev);
	if (err)
		return err;

	if (hinic3_func_type(nic_dev->hwdev) == TYPE_VF)
		return 0;

	err = hinic3_set_link_status_follow(nic_dev->hwdev,
					    HINIC3_LINK_FOLLOW_PORT);
	if (err == HINIC3_MGMT_CMD_UNSUPPORTED)
		PMD_DRV_LOG(WARNING, "Don't support to set link status follow "
				     "phy port status");
	else if (err)
		return err;

	return 0;
}

/**
 * Initialize the network function, including hardware configuration, memory
 * allocation for data structures, MAC address setup, and interrupt enabling.
 * It also registers interrupt callbacks and sets default hardware features.
 * If any step fails, appropriate cleanup is performed.
 *
 * @param[out] eth_dev
 * Pointer to ethernet device structure.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_func_init(struct rte_eth_dev *eth_dev)
{
	struct hinic3_tcam_info *tcam_info = NULL;
	struct hinic3_nic_dev *nic_dev = NULL;
	struct rte_pci_device *pci_dev = NULL;
	int err;

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	/* EAL is secondary and eth_dev is already created. */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		PMD_DRV_LOG(INFO, "Initialize %s in secondary process",
			    eth_dev->data->name);

		return 0;
	}

	nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);
	memset(nic_dev, 0, sizeof(*nic_dev));
	snprintf(nic_dev->dev_name, sizeof(nic_dev->dev_name),
		     "dbdf-%.4x:%.2x:%.2x.%x", pci_dev->addr.domain,
		     pci_dev->addr.bus, pci_dev->addr.devid,
		     pci_dev->addr.function);

	/* Alloc mac_addrs. */
	eth_dev->data->mac_addrs = rte_zmalloc("hinic3_mac",
		HINIC3_MAX_UC_MAC_ADDRS * sizeof(struct rte_ether_addr), 0);
	if (!eth_dev->data->mac_addrs) {
		PMD_DRV_LOG(ERR,
			    "Allocate %zx bytes to store MAC addresses "
			    "failed, dev_name: %s",
			    HINIC3_MAX_UC_MAC_ADDRS *
				    sizeof(struct rte_ether_addr),
			    eth_dev->data->name);
		err = -ENOMEM;
		goto alloc_eth_addr_fail;
	}

	nic_dev->mc_list = rte_zmalloc("hinic3_mc",
		HINIC3_MAX_MC_MAC_ADDRS * sizeof(struct rte_ether_addr), 0);
	if (!nic_dev->mc_list) {
		PMD_DRV_LOG(ERR,
			    "Allocate %zx bytes to store multicast "
			    "addresses failed, dev_name: %s",
			    HINIC3_MAX_MC_MAC_ADDRS *
				    sizeof(struct rte_ether_addr),
			    eth_dev->data->name);
		err = -ENOMEM;
		goto alloc_mc_list_fail;
	}

	/* Create hardware device. */
	nic_dev->hwdev = rte_zmalloc("hinic3_hwdev", sizeof(*nic_dev->hwdev),
				     RTE_CACHE_LINE_SIZE);
	if (!nic_dev->hwdev) {
		PMD_DRV_LOG(ERR, "Allocate hwdev memory failed, dev_name: %s",
			    eth_dev->data->name);
		err = -ENOMEM;
		goto alloc_hwdev_mem_fail;
	}
	nic_dev->hwdev->pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	nic_dev->hwdev->dev_handle = nic_dev;
	nic_dev->hwdev->eth_dev = eth_dev;
	nic_dev->hwdev->port_id = eth_dev->data->port_id;

	err = hinic3_init_hwdev(nic_dev->hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Init chip hwdev failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_hwdev_fail;
	}

	nic_dev->max_sqs = hinic3_func_max_sqs(nic_dev->hwdev);
	nic_dev->max_rqs = hinic3_func_max_rqs(nic_dev->hwdev);

	err = hinic3_init_nic_hwdev(nic_dev->hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Init nic hwdev failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_nic_hwdev_fail;
	}

	err = hinic3_get_feature_from_hw(nic_dev->hwdev, &nic_dev->feature_cap,
					 1);
	if (err) {
		PMD_DRV_LOG(ERR,
			"Get nic feature from hardware failed, dev_name: %s",
			eth_dev->data->name);
		goto get_cap_fail;
	}

	err = hinic3_init_sw_rxtxqs(nic_dev);
	if (err) {
		PMD_DRV_LOG(ERR, "Init sw rxqs or txqs failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_sw_rxtxqs_fail;
	}

	err = hinic3_init_mac_table(eth_dev);
	if (err) {
		PMD_DRV_LOG(ERR, "Init mac table failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_mac_table_fail;
	}

	/* Set hardware feature to default status. */
	err = hinic3_set_default_hw_feature(nic_dev);
	if (err) {
		PMD_DRV_LOG(ERR, "Set hw default features failed, dev_name: %s",
			    eth_dev->data->name);
		goto set_default_feature_fail;
	}

	/* Register callback func to eal lib. */
	err = rte_intr_callback_register(PCI_DEV_TO_INTR_HANDLE(pci_dev),
					 hinic3_dev_interrupt_handler,
					 (void *)eth_dev);
	if (err) {
		PMD_DRV_LOG(ERR, "Register intr callback failed, dev_name: %s",
			    eth_dev->data->name);
		goto reg_intr_cb_fail;
	}

	/* Enable uio/vfio intr/eventfd mapping. */
	err = rte_intr_enable(PCI_DEV_TO_INTR_HANDLE(pci_dev));
	if (err) {
		PMD_DRV_LOG(ERR, "Enable rte interrupt failed, dev_name: %s",
			    eth_dev->data->name);
		goto enable_intr_fail;
	}
	tcam_info = &nic_dev->tcam;
	memset(tcam_info, 0, sizeof(struct hinic3_tcam_info));
	TAILQ_INIT(&tcam_info->tcam_list);
	TAILQ_INIT(&tcam_info->tcam_dynamic_info.tcam_dynamic_list);
	TAILQ_INIT(&nic_dev->filter_ethertype_list);
	TAILQ_INIT(&nic_dev->filter_fdir_rule_list);

	hinic3_mutex_init(&nic_dev->rx_mode_mutex, NULL);

	hinic3_set_bit(HINIC3_DEV_INTR_EN, &nic_dev->dev_status);

	hinic3_set_bit(HINIC3_DEV_INIT, &nic_dev->dev_status);
	PMD_DRV_LOG(INFO, "Initialize %s in primary succeed",
		    eth_dev->data->name);

	/**
	 * Queue xstats filled automatically by ethdev layer.
	 */
	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	return 0;

enable_intr_fail:
	rte_intr_callback_unregister(PCI_DEV_TO_INTR_HANDLE(pci_dev),
					hinic3_dev_interrupt_handler,
					(void *)eth_dev);

reg_intr_cb_fail:
set_default_feature_fail:
	hinic3_deinit_mac_addr(eth_dev);

init_mac_table_fail:
	hinic3_deinit_sw_rxtxqs(nic_dev);

init_sw_rxtxqs_fail:
	hinic3_free_nic_hwdev(nic_dev->hwdev);

get_cap_fail:
init_nic_hwdev_fail:
	hinic3_free_hwdev(nic_dev->hwdev);
	eth_dev->dev_ops = NULL;
	eth_dev->rx_queue_count = NULL;
	eth_dev->rx_descriptor_status = NULL;
	eth_dev->tx_descriptor_status = NULL;

init_hwdev_fail:
	rte_free(nic_dev->hwdev);
	nic_dev->hwdev = NULL;

alloc_hwdev_mem_fail:
	rte_free(nic_dev->mc_list);
	nic_dev->mc_list = NULL;

alloc_mc_list_fail:
	rte_free(eth_dev->data->mac_addrs);
	eth_dev->data->mac_addrs = NULL;

alloc_eth_addr_fail:
	PMD_DRV_LOG(ERR, "Initialize %s in primary failed",
		    eth_dev->data->name);
	return err;
}

static int
hinic3_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	PMD_DRV_LOG(INFO, "Initializing %.4x:%.2x:%.2x.%x in %s process",
		    pci_dev->addr.domain, pci_dev->addr.bus,
		    pci_dev->addr.devid, pci_dev->addr.function,
		    (rte_eal_process_type() == RTE_PROC_PRIMARY) ? "primary"
								 : "secondary");

	PMD_DRV_LOG(INFO, "Network Interface pmd driver version: %s",
		    HINIC3_PMD_DRV_VERSION);

	return hinic3_func_init(eth_dev);
}

static int
hinic3_dev_uninit(struct rte_eth_dev *dev)
{
	struct hinic3_nic_dev *nic_dev;

	nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	hinic3_clear_bit(HINIC3_DEV_INIT, &nic_dev->dev_status);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	return hinic3_dev_close(dev);
}

static const struct rte_pci_id pci_id_hinic3_map[] = {
#ifdef CONFIG_SP_VID_DID
	{RTE_PCI_DEVICE(PCI_VENDOR_ID_SPNIC, HINIC3_DEV_ID_STANDARD)},
	{RTE_PCI_DEVICE(PCI_VENDOR_ID_SPNIC, HINIC3_DEV_ID_VF)},
#else
	{RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HINIC3_DEV_ID_STANDARD)},
	{RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HINIC3_DEV_ID_VF)},
#endif

	{.vendor_id = 0},
};

static int
hinic3_pci_probe(__rte_unused struct rte_pci_driver *pci_drv,
		 struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct hinic3_nic_dev), hinic3_dev_init);
}

static int
hinic3_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, hinic3_dev_uninit);
}

static struct rte_pci_driver rte_hinic3_pmd = {
	.id_table = pci_id_hinic3_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = hinic3_pci_probe,
	.remove = hinic3_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_hinic3, rte_hinic3_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_hinic3, pci_id_hinic3_map);

RTE_INIT(hinic3_init_log)
{
	hinic3_logtype = rte_log_register("pmd.net.hinic3");
	if (hinic3_logtype >= 0)
		rte_log_set_level(hinic3_logtype, RTE_LOG_INFO);
}
