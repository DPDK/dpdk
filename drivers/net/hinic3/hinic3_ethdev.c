/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */
#include <ethdev_pci.h>
#include <eal_interrupts.h>

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

/*
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
	uint8_t addr_bytes[RTE_ETHER_ADDR_LEN];
	uint16_t func_id = 0;
	int err = 0;

	err = hinic3_get_default_mac(nic_dev->hwdev, addr_bytes,
				     RTE_ETHER_ADDR_LEN);
	if (err)
		return err;

	rte_ether_addr_copy((struct rte_ether_addr *)addr_bytes,
			    &eth_dev->data->mac_addrs[0]);
	if (rte_is_zero_ether_addr(&eth_dev->data->mac_addrs[0]) ||
	   rte_is_broadcast_ether_addr(&eth_dev->data->mac_addrs[0]))
		rte_eth_random_addr(eth_dev->data->mac_addrs[0].addr_bytes);

	func_id = hinic3_global_func_id(nic_dev->hwdev);
	err = hinic3_set_mac(nic_dev->hwdev,
			     eth_dev->data->mac_addrs[0].addr_bytes, 0, func_id);
	if (err && err != HINIC3_PF_SET_VF_ALREADY)
		return err;

	rte_ether_addr_copy(&eth_dev->data->mac_addrs[0],
			    &nic_dev->default_addr);

	return 0;
}

/**
 * Delete all multicast MAC addresses from the NIC device.
 *
 * This function iterates over the list of multicast MAC addresses and removes
 * each address from the NIC device by calling `hinic3_del_mac`. After each
 * deletion, the address is reset to zero.
 *
 * @param[in] nic_dev
 * Pointer to NIC device structure.
 */
static void
hinic3_delete_mc_addr_list(struct hinic3_nic_dev *nic_dev)
{
	uint16_t func_id;
	uint32_t i;

	func_id = hinic3_global_func_id(nic_dev->hwdev);

	for (i = 0; i < HINIC3_MAX_MC_MAC_ADDRS; i++) {
		if (rte_is_zero_ether_addr(&nic_dev->mc_list[i]))
			break;

		hinic3_del_mac(nic_dev->hwdev, nic_dev->mc_list[i].addr_bytes, 0, func_id);
		memset(&nic_dev->mc_list[i], 0, sizeof(struct rte_ether_addr));
	}
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
	uint16_t func_id = 0;
	int err;
	int i;

	func_id = hinic3_global_func_id(nic_dev->hwdev);

	for (i = 0; i < HINIC3_MAX_UC_MAC_ADDRS; i++) {
		if (rte_is_zero_ether_addr(&eth_dev->data->mac_addrs[i]))
			continue;

		err = hinic3_del_mac(nic_dev->hwdev,
				     eth_dev->data->mac_addrs[i].addr_bytes, 0, func_id);
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
hinic3_pf_get_default_cos(struct hinic3_hwdev *hwdev, uint8_t *cos_id)
{
	uint8_t default_cos = 0;
	uint8_t valid_cos_bitmap;
	uint8_t i;

	valid_cos_bitmap = hwdev->cfg_mgmt->svc_cap.cos_valid_bitmap;
	if (!valid_cos_bitmap) {
		PMD_DRV_LOG(ERR, "PF has none cos to support");
		return -EFAULT;
	}

	for (i = 0; i < HINIC3_COS_NUM_MAX; i++) {
		if (valid_cos_bitmap & RTE_BIT32(i))
			/* Find max cos id as default cos. */
			default_cos = i;
	}

	*cos_id = default_cos;

	return 0;
}

static int
hinic3_init_default_cos(struct hinic3_nic_dev *nic_dev)
{
	uint8_t cos_id = 0;
	int err;

	if (!HINIC3_IS_VF(nic_dev->hwdev)) {
		err = hinic3_pf_get_default_cos(nic_dev->hwdev, &cos_id);
		if (err) {
			PMD_DRV_LOG(ERR, "Get PF default cos failed, err: %d", err);
			return err;
		}
	} else {
		err = hinic3_vf_get_default_cos(nic_dev->hwdev, &cos_id);
		if (err) {
			PMD_DRV_LOG(ERR, "Get VF default cos failed, err: %d", err);
			return err;
		}
	}

	nic_dev->default_cos = cos_id;
	PMD_DRV_LOG(DEBUG, "Default cos %d", nic_dev->default_cos);
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
		PMD_DRV_LOG(WARNING, "Don't support to set link status follow phy port status");
	else if (err)
		return err;

	return 0;
}

static int
hinic3_init_sw_rxtxqs(struct hinic3_nic_dev *nic_dev)
{
	uint32_t txq_size;
	uint32_t rxq_size;

	/* Allocate software txq array. */
	txq_size = nic_dev->max_sqs * sizeof(*nic_dev->txqs);
	nic_dev->txqs =
		rte_zmalloc("hinic3_txqs", txq_size, RTE_CACHE_LINE_SIZE);
	if (!nic_dev->txqs) {
		PMD_DRV_LOG(ERR, "Allocate txqs failed");
		return -ENOMEM;
	}

	/* Allocate software rxq array. */
	rxq_size = nic_dev->max_rqs * sizeof(*nic_dev->rxqs);
	nic_dev->rxqs =
		rte_zmalloc("hinic3_rxqs", rxq_size, RTE_CACHE_LINE_SIZE);
	if (!nic_dev->rxqs) {
		/* Free txqs. */
		rte_free(nic_dev->txqs);
		nic_dev->txqs = NULL;

		PMD_DRV_LOG(ERR, "Allocate rxqs failed");
		return -ENOMEM;
	}

	return 0;
}

static void
hinic3_deinit_sw_rxtxqs(struct hinic3_nic_dev *nic_dev)
{
	rte_free(nic_dev->txqs);
	nic_dev->txqs = NULL;

	rte_free(nic_dev->rxqs);
	nic_dev->rxqs = NULL;
}
