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
#include "hinic3_nic_io.h"
#include "hinic3_tx.h"
#include "hinic3_rx.h"
#include "hinic3_ethdev.h"

#define HINIC3_MIN_RX_BUF_SIZE 1024

#define HINIC3_DEFAULT_BURST_SIZE 32
#define HINIC3_DEFAULT_NB_QUEUES  1
#define HINIC3_DEFAULT_RING_SIZE  1024
#define HINIC3_MAX_LRO_SIZE	  65536

#define HINIC3_DEFAULT_RX_FREE_THRESH 32u
#define HINIC3_DEFAULT_TX_FREE_THRESH 32u

#define HINIC3_RX_WAIT_CYCLE_THRESH 500

/**
 * Get the 32-bit VFTA bit mask for the lower 5 bits of the VLAN ID.
 *
 * Vlan_id is a 12 bit number. The VFTA array is actually a 4096 bit array,
 * 128 of 32bit elements. 2^5 = 32. The val of lower 5 bits specifies the bit
 * in the 32bit element. The higher 7 bit val specifies VFTA array index.
 */
#define HINIC3_VFTA_BIT(vlan_id) (1 << ((vlan_id) & 0x1F))
/**
 * Get the VFTA index from the upper 7 bits of the VLAN ID.
 */
#define HINIC3_VFTA_IDX(vlan_id) ((vlan_id) >> 5)

#define HINIC3_LRO_DEFAULT_TIME_LIMIT 16
#define HINIC3_LRO_UNIT_WQE_SIZE      1024 /**< Bytes. */

#define HINIC3_MAX_RX_PKT_LEN(rxmod) ((rxmod).mtu)
int hinic3_logtype; /**< Driver-specific log messages type. */

/**
 * The different receive modes for the NIC.
 *
 * The receive modes are represented as bit flags that control how the
 * NIC handles various types of network traffic.
 */
enum hinic3_rx_mod {
	/* Enable unicast receive mode. */
	HINIC3_RX_MODE_UC = 1 << 0,
	/* Enable multicast receive mode. */
	HINIC3_RX_MODE_MC = 1 << 1,
	/* Enable broadcast receive mode. */
	HINIC3_RX_MODE_BC = 1 << 2,
	/* Enable receive mode for all multicast addresses. */
	HINIC3_RX_MODE_MC_ALL = 1 << 3,
	/* Enable promiscuous mode, receiving all packets. */
	HINIC3_RX_MODE_PROMISC = 1 << 4,
};

#define HINIC3_DEFAULT_RX_MODE \
	(HINIC3_RX_MODE_UC | HINIC3_RX_MODE_MC | HINIC3_RX_MODE_BC)

struct hinic3_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	uint32_t offset;
};

#define HINIC3_FUNC_STAT(_stat_item)                                       \
	{                                                                  \
		.name = #_stat_item,                                       \
		.offset = offsetof(struct hinic3_vport_stats, _stat_item), \
	}

static const struct hinic3_xstats_name_off hinic3_vport_stats_strings[] = {
	HINIC3_FUNC_STAT(tx_unicast_pkts_vport),
	HINIC3_FUNC_STAT(tx_unicast_bytes_vport),
	HINIC3_FUNC_STAT(tx_multicast_pkts_vport),
	HINIC3_FUNC_STAT(tx_multicast_bytes_vport),
	HINIC3_FUNC_STAT(tx_broadcast_pkts_vport),
	HINIC3_FUNC_STAT(tx_broadcast_bytes_vport),

	HINIC3_FUNC_STAT(rx_unicast_pkts_vport),
	HINIC3_FUNC_STAT(rx_unicast_bytes_vport),
	HINIC3_FUNC_STAT(rx_multicast_pkts_vport),
	HINIC3_FUNC_STAT(rx_multicast_bytes_vport),
	HINIC3_FUNC_STAT(rx_broadcast_pkts_vport),
	HINIC3_FUNC_STAT(rx_broadcast_bytes_vport),

	HINIC3_FUNC_STAT(tx_discard_vport),
	HINIC3_FUNC_STAT(rx_discard_vport),
	HINIC3_FUNC_STAT(tx_err_vport),
	HINIC3_FUNC_STAT(rx_err_vport),
};

#define HINIC3_VPORT_XSTATS_NUM RTE_DIM(hinic3_vport_stats_strings)

#define HINIC3_PORT_STAT(_stat_item)                                       \
	{                                                                  \
		.name = #_stat_item,                                       \
		.offset = offsetof(struct mag_phy_port_stats, _stat_item), \
	}

static const struct hinic3_xstats_name_off hinic3_phyport_stats_strings[] = {
	HINIC3_PORT_STAT(mac_tx_fragment_pkt_num),
	HINIC3_PORT_STAT(mac_tx_undersize_pkt_num),
	HINIC3_PORT_STAT(mac_tx_undermin_pkt_num),
	HINIC3_PORT_STAT(mac_tx_64_oct_pkt_num),
	HINIC3_PORT_STAT(mac_tx_65_127_oct_pkt_num),
	HINIC3_PORT_STAT(mac_tx_128_255_oct_pkt_num),
	HINIC3_PORT_STAT(mac_tx_256_511_oct_pkt_num),
	HINIC3_PORT_STAT(mac_tx_512_1023_oct_pkt_num),
	HINIC3_PORT_STAT(mac_tx_1024_1518_oct_pkt_num),
	HINIC3_PORT_STAT(mac_tx_1519_2047_oct_pkt_num),
	HINIC3_PORT_STAT(mac_tx_2048_4095_oct_pkt_num),
	HINIC3_PORT_STAT(mac_tx_4096_8191_oct_pkt_num),
	HINIC3_PORT_STAT(mac_tx_8192_9216_oct_pkt_num),
	HINIC3_PORT_STAT(mac_tx_9217_12287_oct_pkt_num),
	HINIC3_PORT_STAT(mac_tx_12288_16383_oct_pkt_num),
	HINIC3_PORT_STAT(mac_tx_1519_max_bad_pkt_num),
	HINIC3_PORT_STAT(mac_tx_1519_max_good_pkt_num),
	HINIC3_PORT_STAT(mac_tx_oversize_pkt_num),
	HINIC3_PORT_STAT(mac_tx_jabber_pkt_num),
	HINIC3_PORT_STAT(mac_tx_bad_pkt_num),
	HINIC3_PORT_STAT(mac_tx_bad_oct_num),
	HINIC3_PORT_STAT(mac_tx_good_pkt_num),
	HINIC3_PORT_STAT(mac_tx_good_oct_num),
	HINIC3_PORT_STAT(mac_tx_total_pkt_num),
	HINIC3_PORT_STAT(mac_tx_total_oct_num),
	HINIC3_PORT_STAT(mac_tx_uni_pkt_num),
	HINIC3_PORT_STAT(mac_tx_multi_pkt_num),
	HINIC3_PORT_STAT(mac_tx_broad_pkt_num),
	HINIC3_PORT_STAT(mac_tx_pause_num),
	HINIC3_PORT_STAT(mac_tx_pfc_pkt_num),
	HINIC3_PORT_STAT(mac_tx_pfc_pri0_pkt_num),
	HINIC3_PORT_STAT(mac_tx_pfc_pri1_pkt_num),
	HINIC3_PORT_STAT(mac_tx_pfc_pri2_pkt_num),
	HINIC3_PORT_STAT(mac_tx_pfc_pri3_pkt_num),
	HINIC3_PORT_STAT(mac_tx_pfc_pri4_pkt_num),
	HINIC3_PORT_STAT(mac_tx_pfc_pri5_pkt_num),
	HINIC3_PORT_STAT(mac_tx_pfc_pri6_pkt_num),
	HINIC3_PORT_STAT(mac_tx_pfc_pri7_pkt_num),
	HINIC3_PORT_STAT(mac_tx_control_pkt_num),
	HINIC3_PORT_STAT(mac_tx_err_all_pkt_num),
	HINIC3_PORT_STAT(mac_tx_from_app_good_pkt_num),
	HINIC3_PORT_STAT(mac_tx_from_app_bad_pkt_num),

	HINIC3_PORT_STAT(mac_rx_fragment_pkt_num),
	HINIC3_PORT_STAT(mac_rx_undersize_pkt_num),
	HINIC3_PORT_STAT(mac_rx_undermin_pkt_num),
	HINIC3_PORT_STAT(mac_rx_64_oct_pkt_num),
	HINIC3_PORT_STAT(mac_rx_65_127_oct_pkt_num),
	HINIC3_PORT_STAT(mac_rx_128_255_oct_pkt_num),
	HINIC3_PORT_STAT(mac_rx_256_511_oct_pkt_num),
	HINIC3_PORT_STAT(mac_rx_512_1023_oct_pkt_num),
	HINIC3_PORT_STAT(mac_rx_1024_1518_oct_pkt_num),
	HINIC3_PORT_STAT(mac_rx_1519_2047_oct_pkt_num),
	HINIC3_PORT_STAT(mac_rx_2048_4095_oct_pkt_num),
	HINIC3_PORT_STAT(mac_rx_4096_8191_oct_pkt_num),
	HINIC3_PORT_STAT(mac_rx_8192_9216_oct_pkt_num),
	HINIC3_PORT_STAT(mac_rx_9217_12287_oct_pkt_num),
	HINIC3_PORT_STAT(mac_rx_12288_16383_oct_pkt_num),
	HINIC3_PORT_STAT(mac_rx_1519_max_bad_pkt_num),
	HINIC3_PORT_STAT(mac_rx_1519_max_good_pkt_num),
	HINIC3_PORT_STAT(mac_rx_oversize_pkt_num),
	HINIC3_PORT_STAT(mac_rx_jabber_pkt_num),
	HINIC3_PORT_STAT(mac_rx_bad_pkt_num),
	HINIC3_PORT_STAT(mac_rx_bad_oct_num),
	HINIC3_PORT_STAT(mac_rx_good_pkt_num),
	HINIC3_PORT_STAT(mac_rx_good_oct_num),
	HINIC3_PORT_STAT(mac_rx_total_pkt_num),
	HINIC3_PORT_STAT(mac_rx_total_oct_num),
	HINIC3_PORT_STAT(mac_rx_uni_pkt_num),
	HINIC3_PORT_STAT(mac_rx_multi_pkt_num),
	HINIC3_PORT_STAT(mac_rx_broad_pkt_num),
	HINIC3_PORT_STAT(mac_rx_pause_num),
	HINIC3_PORT_STAT(mac_rx_pfc_pkt_num),
	HINIC3_PORT_STAT(mac_rx_pfc_pri0_pkt_num),
	HINIC3_PORT_STAT(mac_rx_pfc_pri1_pkt_num),
	HINIC3_PORT_STAT(mac_rx_pfc_pri2_pkt_num),
	HINIC3_PORT_STAT(mac_rx_pfc_pri3_pkt_num),
	HINIC3_PORT_STAT(mac_rx_pfc_pri4_pkt_num),
	HINIC3_PORT_STAT(mac_rx_pfc_pri5_pkt_num),
	HINIC3_PORT_STAT(mac_rx_pfc_pri6_pkt_num),
	HINIC3_PORT_STAT(mac_rx_pfc_pri7_pkt_num),
	HINIC3_PORT_STAT(mac_rx_control_pkt_num),
	HINIC3_PORT_STAT(mac_rx_sym_err_pkt_num),
	HINIC3_PORT_STAT(mac_rx_fcs_err_pkt_num),
	HINIC3_PORT_STAT(mac_rx_send_app_good_pkt_num),
	HINIC3_PORT_STAT(mac_rx_send_app_bad_pkt_num),
	HINIC3_PORT_STAT(mac_rx_unfilter_pkt_num),
};

#define HINIC3_PHYPORT_XSTATS_NUM RTE_DIM(hinic3_phyport_stats_strings)

#define HINIC3_RXQ_STAT(_stat_item)                                      \
	{                                                                \
		.name = #_stat_item,                                     \
		.offset = offsetof(struct hinic3_rxq_stats, _stat_item), \
	}

/**
 * The name and offset field of RXQ statistic items.
 *
 * The inclusion of additional statistics depends on the compilation flags:
 * - `HINIC3_XSTAT_RXBUF_INFO` enables buffer-related stats.
 * - `HINIC3_XSTAT_PROF_RX` enables performance timing stats.
 * - `HINIC3_XSTAT_MBUF_USE` enables memory buffer usage stats.
 */
static const struct hinic3_xstats_name_off hinic3_rxq_stats_strings[] = {
	HINIC3_RXQ_STAT(rx_nombuf),
	HINIC3_RXQ_STAT(burst_pkts),
	HINIC3_RXQ_STAT(errors),
	HINIC3_RXQ_STAT(csum_errors),
	HINIC3_RXQ_STAT(other_errors),
	HINIC3_RXQ_STAT(empty),

#ifdef HINIC3_XSTAT_RXBUF_INFO
	HINIC3_RXQ_STAT(rx_mbuf),
	HINIC3_RXQ_STAT(rx_avail),
	HINIC3_RXQ_STAT(rx_hole),
#endif

#ifdef HINIC3_XSTAT_PROF_RX
	HINIC3_RXQ_STAT(app_tsc),
	HINIC3_RXQ_STAT(pmd_tsc),
#endif

#ifdef HINIC3_XSTAT_MBUF_USE
	HINIC3_RXQ_STAT(rx_alloc_mbuf_bytes),
	HINIC3_RXQ_STAT(rx_free_mbuf_bytes),
	HINIC3_RXQ_STAT(rx_left_mbuf_bytes),
#endif
};

#define HINIC3_RXQ_XSTATS_NUM RTE_DIM(hinic3_rxq_stats_strings)

#define HINIC3_TXQ_STAT(_stat_item)                                      \
	{                                                                \
		.name = #_stat_item,                                     \
		.offset = offsetof(struct hinic3_txq_stats, _stat_item), \
	}

/**
 * The name and offset field of TXQ statistic items.
 *
 * The inclusion of additional statistics depends on the compilation flags:
 * - `HINIC3_XSTAT_PROF_TX` enables performance timing stats.
 * - `HINIC3_XSTAT_MBUF_USE` enables memory buffer usage stats.
 */
static const struct hinic3_xstats_name_off hinic3_txq_stats_strings[] = {
	HINIC3_TXQ_STAT(tx_busy),
	HINIC3_TXQ_STAT(offload_errors),
	HINIC3_TXQ_STAT(burst_pkts),
	HINIC3_TXQ_STAT(sge_len0),
	HINIC3_TXQ_STAT(mbuf_null),

#ifdef HINIC3_XSTAT_PROF_TX
	HINIC3_TXQ_STAT(app_tsc),
	HINIC3_TXQ_STAT(pmd_tsc),
#endif

#ifdef HINIC3_XSTAT_MBUF_USE
	HINIC3_TXQ_STAT(tx_left_mbuf_bytes),
#endif
};

#define HINIC3_TXQ_XSTATS_NUM RTE_DIM(hinic3_txq_stats_strings)

static uint32_t
hinic3_xstats_calc_num(struct hinic3_nic_dev *nic_dev)
{
	if (HINIC3_IS_VF(nic_dev->hwdev)) {
		return (HINIC3_VPORT_XSTATS_NUM +
			HINIC3_RXQ_XSTATS_NUM * nic_dev->num_rqs +
			HINIC3_TXQ_XSTATS_NUM * nic_dev->num_sqs);
	} else {
		return (HINIC3_VPORT_XSTATS_NUM + HINIC3_PHYPORT_XSTATS_NUM +
			HINIC3_RXQ_XSTATS_NUM * nic_dev->num_rqs +
			HINIC3_TXQ_XSTATS_NUM * nic_dev->num_sqs);
	}
}

#define HINIC3_MAX_QUEUE_DEPTH 16384
#define HINIC3_MIN_QUEUE_DEPTH 128
#define HINIC3_TXD_ALIGN       1
#define HINIC3_RXD_ALIGN       1

static const struct rte_eth_desc_lim hinic3_rx_desc_lim = {
	.nb_max = HINIC3_MAX_QUEUE_DEPTH,
	.nb_min = HINIC3_MIN_QUEUE_DEPTH,
	.nb_align = HINIC3_RXD_ALIGN,
};

static const struct rte_eth_desc_lim hinic3_tx_desc_lim = {
	.nb_max = HINIC3_MAX_QUEUE_DEPTH,
	.nb_min = HINIC3_MIN_QUEUE_DEPTH,
	.nb_align = HINIC3_TXD_ALIGN,
};

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

/**
 * Look up or creates a memory pool for storing packet buffers used in copy
 * operations.
 *
 * @param[in] nic_dev
 * Pointer to NIC device structure.
 *
 * @return
 * 0 on success, non-zero on failure.
 * `-ENOMEM`: Memory pool creation fails.
 */
static int
hinic3_copy_mempool_init(struct hinic3_nic_dev *nic_dev)
{
	nic_dev->cpy_mpool = rte_mempool_lookup(HINCI3_CPY_MEMPOOL_NAME);
	if (nic_dev->cpy_mpool == NULL) {
		nic_dev->cpy_mpool = rte_pktmbuf_pool_create(HINCI3_CPY_MEMPOOL_NAME,
			HINIC3_COPY_MEMPOOL_DEPTH, HINIC3_COPY_MEMPOOL_CACHE,
			0, HINIC3_COPY_MBUF_SIZE, rte_socket_id());
		if (nic_dev->cpy_mpool == NULL) {
			PMD_DRV_LOG(ERR,
				    "Create copy mempool failed, errno: %d, dev_name: %s",
				    rte_errno, HINCI3_CPY_MEMPOOL_NAME);
			return -ENOMEM;
		}
	}

	return 0;
}

/**
 * Clear the reference to the copy memory pool without freeing it.
 *
 * @param[in] nic_dev
 * Pointer to NIC device structure.
 */
static void
hinic3_copy_mempool_uninit(struct hinic3_nic_dev *nic_dev)
{
	nic_dev->cpy_mpool = NULL;
}

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
			    "Intr is disabled, ignore intr event, dev_name: %s, port_id: %d",
			    nic_dev->dev_name, dev->data->port_id);
		return;
	}

	/* Aeq0 msg handler. */
	hinic3_dev_handle_aeq_event(nic_dev->hwdev, param);
}

/**
 * Do the config for TX/Rx queues, include queue number, mtu size and RSS.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_dev_configure(struct rte_eth_dev *dev)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	nic_dev->num_sqs = dev->data->nb_tx_queues;
	nic_dev->num_rqs = dev->data->nb_rx_queues;

	nic_dev->mtu_size =
		(uint16_t)HINIC3_PKTLEN_TO_MTU(HINIC3_MAX_RX_PKT_LEN(dev->data->dev_conf.rxmode));
	if (dev->data->dev_conf.rxmode.mq_mode & RTE_ETH_MQ_RX_RSS_FLAG)
		dev->data->dev_conf.rxmode.offloads |=
			RTE_ETH_RX_OFFLOAD_RSS_HASH;

	/* Clear fdir filter. */
	hinic3_free_fdir_filter(dev);

	return 0;
}

/**
 * Get information about the device.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[out] info
 * Info structure for ethernet device.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_dev_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *info)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	info->max_rx_queues = nic_dev->max_rqs;
	info->max_tx_queues = nic_dev->max_sqs;
	info->min_rx_bufsize = HINIC3_MIN_RX_BUF_SIZE;
	info->max_rx_pktlen = HINIC3_MAX_JUMBO_FRAME_SIZE;
	info->max_mac_addrs = HINIC3_MAX_UC_MAC_ADDRS;
	info->min_mtu = HINIC3_MIN_MTU_SIZE;
	info->max_mtu = HINIC3_MAX_MTU_SIZE;
	info->max_lro_pkt_size = HINIC3_MAX_LRO_SIZE;

	info->rx_queue_offload_capa = 0;
	info->rx_offload_capa =
		RTE_ETH_RX_OFFLOAD_VLAN_STRIP | RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
		RTE_ETH_RX_OFFLOAD_UDP_CKSUM | RTE_ETH_RX_OFFLOAD_TCP_CKSUM |
		RTE_ETH_RX_OFFLOAD_SCTP_CKSUM | RTE_ETH_RX_OFFLOAD_VLAN_FILTER |
		RTE_ETH_RX_OFFLOAD_SCATTER | RTE_ETH_RX_OFFLOAD_TCP_LRO |
		RTE_ETH_RX_OFFLOAD_RSS_HASH;

	info->tx_queue_offload_capa = 0;
	info->tx_offload_capa =
		RTE_ETH_TX_OFFLOAD_VLAN_INSERT | RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
		RTE_ETH_TX_OFFLOAD_UDP_CKSUM | RTE_ETH_TX_OFFLOAD_TCP_CKSUM |
		RTE_ETH_TX_OFFLOAD_SCTP_CKSUM |
		RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM |
		RTE_ETH_TX_OFFLOAD_TCP_TSO | RTE_ETH_TX_OFFLOAD_MULTI_SEGS;

	info->hash_key_size = HINIC3_RSS_KEY_SIZE;
	info->reta_size = HINIC3_RSS_INDIR_SIZE;
	info->flow_type_rss_offloads = HINIC3_RSS_OFFLOAD_ALL;

	info->rx_desc_lim = hinic3_rx_desc_lim;
	info->tx_desc_lim = hinic3_tx_desc_lim;

	/* Driver-preferred rx/tx parameters. */
	info->default_rxportconf.burst_size = HINIC3_DEFAULT_BURST_SIZE;
	info->default_txportconf.burst_size = HINIC3_DEFAULT_BURST_SIZE;
	info->default_rxportconf.nb_queues = HINIC3_DEFAULT_NB_QUEUES;
	info->default_txportconf.nb_queues = HINIC3_DEFAULT_NB_QUEUES;
	info->default_rxportconf.ring_size = HINIC3_DEFAULT_RING_SIZE;
	info->default_txportconf.ring_size = HINIC3_DEFAULT_RING_SIZE;

	return 0;
}

static int
hinic3_fw_version_get(struct rte_eth_dev *dev, char *fw_version, size_t fw_size)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	char mgmt_ver[MGMT_VERSION_MAX_LEN] = {0};
	int err;

	err = hinic3_get_mgmt_version(nic_dev->hwdev, mgmt_ver,
				      HINIC3_MGMT_VERSION_MAX_LEN);
	if (err) {
		PMD_DRV_LOG(ERR, "Get fw version failed");
		return -EIO;
	}

	if (fw_size < strlen(mgmt_ver) + 1)
		return (strlen(mgmt_ver) + 1);

	strlcpy(fw_version, mgmt_ver, fw_size);

	return 0;
}

/**
 * Set ethernet device link state up.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_dev_set_link_up(struct rte_eth_dev *dev)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	int err;

	/*
	 * Vport enable will set function valid in mpu.
	 * So dev start status need to be checked before vport enable.
	 */
	if (hinic3_get_bit(HINIC3_DEV_START, &nic_dev->dev_status)) {
		err = hinic3_set_vport_enable(nic_dev->hwdev, true);
		if (err) {
			PMD_DRV_LOG(ERR, "Enable vport failed, dev_name: %s",
				    nic_dev->dev_name);
			return err;
		}
	}

	/* Link status follow phy port status, mpu will open pma. */
	err = hinic3_set_port_enable(nic_dev->hwdev, true);
	if (err) {
		PMD_DRV_LOG(ERR,
			    "Set MAC link up failed, dev_name: %s, port_id: %d",
			    nic_dev->dev_name, dev->data->port_id);
		return err;
	}

	return 0;
}

/**
 * Set ethernet device link state down.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_dev_set_link_down(struct rte_eth_dev *dev)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	int err;

	err = hinic3_set_vport_enable(nic_dev->hwdev, false);
	if (err) {
		PMD_DRV_LOG(ERR, "Disable vport failed, dev_name: %s",
			    nic_dev->dev_name);
		return err;
	}

	/* Link status follow phy port status, mpu will close pma. */
	err = hinic3_set_port_enable(nic_dev->hwdev, false);
	if (err) {
		PMD_DRV_LOG(ERR,
			"Set MAC link down failed, dev_name: %s, port_id: %d",
			nic_dev->dev_name, dev->data->port_id);
		return err;
	}

	return 0;
}

/**
 * Get device physical link information.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] wait_to_complete
 * Wait for request completion.
 *
 * @return
 * 0 : Link status changed
 * -1 : Link status not changed.
 */
static int
hinic3_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
#define CHECK_INTERVAL	10  /**< 10ms. */
#define MAX_REPEAT_TIME 100 /**< 1s (100 * 10ms) in total. */
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct rte_eth_link link;
	uint8_t link_state;
	unsigned int rep_cnt = MAX_REPEAT_TIME;
	int ret;

	memset(&link, 0, sizeof(link));
	do {
		/* Get link status information from hardware. */
		ret = hinic3_get_link_state(nic_dev->hwdev, &link_state);
		if (ret) {
			link.link_status = RTE_ETH_LINK_DOWN;
			link.link_speed = RTE_ETH_SPEED_NUM_NONE;
			link.link_duplex = RTE_ETH_LINK_HALF_DUPLEX;
			link.link_autoneg = RTE_ETH_LINK_FIXED;
			goto out;
		}

		hinic3_get_link_port_info(nic_dev->hwdev, link_state, &link);

		if (!wait_to_complete || link.link_status)
			break;

		rte_delay_ms(CHECK_INTERVAL);
	} while (rep_cnt--);

out:
	return rte_eth_linkstatus_set(dev, &link);
}

/**
 * Reset all RX queues (RXQs).
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 */
static void
hinic3_reset_rx_queue(struct rte_eth_dev *dev)
{
	struct hinic3_rxq *rxq = NULL;
	struct hinic3_nic_dev *nic_dev;
	int q_id = 0;

	nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	for (q_id = 0; q_id < nic_dev->num_rqs; q_id++) {
		rxq = nic_dev->rxqs[q_id];

		rxq->cons_idx = 0;
		rxq->prod_idx = 0;
		rxq->delta = rxq->q_depth;
		rxq->next_to_update = 0;
	}
}

/**
 * Reset all TX queues (TXQs).
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 */
static void
hinic3_reset_tx_queue(struct rte_eth_dev *dev)
{
	struct hinic3_nic_dev *nic_dev;
	struct hinic3_txq *txq = NULL;
	int q_id = 0;

	nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	for (q_id = 0; q_id < nic_dev->num_sqs; q_id++) {
		txq = nic_dev->txqs[q_id];

		txq->cons_idx = 0;
		txq->prod_idx = 0;
		txq->owner = 1;

		/* Clear hardware ci. */
		*txq->ci_vaddr_base = 0;
	}
}

/**
 * Create the receive queue.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] qid
 * Receive queue index.
 * @param[in] nb_desc
 * Number of descriptors for receive queue.
 * @param[in] socket_id
 * Socket index on which memory must be allocated.
 * @param[in] rx_conf
 * Thresholds parameters (unused_).
 * @param[in] mp
 * Memory pool for buffer allocations.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_rx_queue_setup(struct rte_eth_dev *dev, uint16_t qid, uint16_t nb_desc,
		      unsigned int socket_id, const struct rte_eth_rxconf *rx_conf,
		      struct rte_mempool *mp)
{
	struct hinic3_nic_dev *nic_dev;
	struct hinic3_rxq *rxq = NULL;
	const struct rte_memzone *rq_mz = NULL;
	const struct rte_memzone *cqe_mz = NULL;
	const struct rte_memzone *pi_mz = NULL;
	uint16_t rq_depth, rx_free_thresh;
	uint32_t queue_buf_size;
	void *db_addr = NULL;
	int wqe_count;
	uint32_t buf_size;
	int err;

	nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	/* Queue depth must be power of 2, otherwise will be aligned up. */
	rq_depth = (nb_desc & (nb_desc - 1))
			? ((uint16_t)(1U << (rte_log2_u32(nb_desc) + 1))) : nb_desc;

	/*
	 * Validate number of receive descriptors.
	 * It must not exceed hardware maximum and minimum.
	 */
	if (rq_depth > HINIC3_MAX_QUEUE_DEPTH ||
	    rq_depth < HINIC3_MIN_QUEUE_DEPTH) {
		PMD_DRV_LOG(ERR,
			    "RX queue depth is out of range from %d to %d",
			    HINIC3_MIN_QUEUE_DEPTH, HINIC3_MAX_QUEUE_DEPTH);
		PMD_DRV_LOG(ERR,
				"nb_desc: %d, q_depth: %d, port: %d queue: %d",
				nb_desc, rq_depth, dev->data->port_id, qid);
		return -EINVAL;
	}

	/*
	 * The RX descriptor ring will be cleaned after rxq->rx_free_thresh
	 * descriptors are used or if the number of descriptors required
	 * to transmit a packet is greater than the number of free RX
	 * descriptors.
	 * The following constraints must be satisfied:
	 * - rx_free_thresh must be greater than 0.
	 * - rx_free_thresh must be less than the size of the ring minus 1.
	 * When set to zero use default values.
	 */
	rx_free_thresh = rx_conf->rx_free_thresh
				? rx_conf->rx_free_thresh : HINIC3_DEFAULT_RX_FREE_THRESH;
	if (rx_free_thresh >= (rq_depth - 1)) {
		PMD_DRV_LOG(ERR,
			    "rx_free_thresh must be less than the number of RX descriptors minus 1, rx_free_thresh: %d port: %d queue: %d)",
			    rx_free_thresh, dev->data->port_id, qid);
		return -EINVAL;
	}

	rxq = rte_zmalloc_socket("hinic3_rq", sizeof(struct hinic3_rxq),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (!rxq) {
		PMD_DRV_LOG(ERR, "Allocate rxq[%d] failed, dev_name: %s", qid,
			    dev->data->name);

		return -ENOMEM;
	}

	/* Init rq parameters. */
	rxq->nic_dev = nic_dev;
	nic_dev->rxqs[qid] = rxq;
	rxq->mb_pool = mp;
	rxq->q_id = qid;
	rxq->q_depth = rq_depth;
	rxq->q_mask = rq_depth - 1;
	rxq->delta = rq_depth;
	rxq->rx_free_thresh = rx_free_thresh;
	rxq->rxinfo_align_end = rxq->q_depth - rxq->rx_free_thresh;
	rxq->port_id = dev->data->port_id;
	rxq->wait_time_cycle = HINIC3_RX_WAIT_CYCLE_THRESH;
	rxq->rx_deferred_start = rx_conf->rx_deferred_start;
	/* If buf_len used for function table, need to translated. */
	uint16_t rx_buf_size =
		rte_pktmbuf_data_room_size(rxq->mb_pool) - RTE_PKTMBUF_HEADROOM;
	err = hinic3_convert_rx_buf_size(rx_buf_size, &buf_size);
	if (err) {
		PMD_DRV_LOG(ERR, "Adjust buf size failed, dev_name: %s",
			    dev->data->name);
		goto adjust_bufsize_fail;
	}

	if (buf_size >= HINIC3_RX_BUF_SIZE_4K &&
	    buf_size < HINIC3_RX_BUF_SIZE_16K)
		rxq->wqe_type = HINIC3_EXTEND_RQ_WQE;
	else
		rxq->wqe_type = HINIC3_NORMAL_RQ_WQE;

	rxq->wqebb_shift = HINIC3_RQ_WQEBB_SHIFT + rxq->wqe_type;
	rxq->wqebb_size = (uint16_t)RTE_BIT32(rxq->wqebb_shift);

	rxq->buf_len = (uint16_t)buf_size;
	rxq->rx_buff_shift = rte_log2_u32(rxq->buf_len);

	pi_mz = hinic3_dma_zone_reserve(dev, "hinic3_rq_pi", qid, RTE_PGSIZE_4K,
					RTE_CACHE_LINE_SIZE, socket_id);
	if (!pi_mz) {
		PMD_DRV_LOG(ERR, "Allocate rxq[%d] pi_mz failed, dev_name: %s",
			    qid, dev->data->name);
		err = -ENOMEM;
		goto alloc_pi_mz_fail;
	}
	rxq->pi_mz = pi_mz;
	rxq->pi_dma_addr = pi_mz->iova;
	rxq->pi_virt_addr = pi_mz->addr;

	err = hinic3_alloc_db_addr(nic_dev->hwdev, &db_addr, HINIC3_DB_TYPE_RQ);
	if (err) {
		PMD_DRV_LOG(ERR, "Alloc rq doorbell addr failed");
		goto alloc_db_err_fail;
	}
	rxq->db_addr = db_addr;

	queue_buf_size = RTE_BIT32(rxq->wqebb_shift) * rq_depth;
	rq_mz = hinic3_dma_zone_reserve(dev, "hinic3_rq_mz", qid,
					queue_buf_size, RTE_PGSIZE_256K, socket_id);
	if (!rq_mz) {
		PMD_DRV_LOG(ERR, "Allocate rxq[%d] rq_mz failed, dev_name: %s",
			    qid, dev->data->name);
		err = -ENOMEM;
		goto alloc_rq_mz_fail;
	}

	memset(rq_mz->addr, 0, queue_buf_size);
	rxq->rq_mz = rq_mz;
	rxq->queue_buf_paddr = rq_mz->iova;
	rxq->queue_buf_vaddr = rq_mz->addr;

	rxq->rx_info = rte_zmalloc_socket("rx_info",
					  rq_depth * sizeof(*rxq->rx_info),
					  RTE_CACHE_LINE_SIZE, socket_id);
	if (!rxq->rx_info) {
		PMD_DRV_LOG(ERR, "Allocate rx_info failed, dev_name: %s",
			    dev->data->name);
		err = -ENOMEM;
		goto alloc_rx_info_fail;
	}

	cqe_mz = hinic3_dma_zone_reserve(dev, "hinic3_cqe_mz", qid,
					 rq_depth * sizeof(*rxq->rx_cqe),
					 RTE_CACHE_LINE_SIZE, socket_id);
	if (!cqe_mz) {
		PMD_DRV_LOG(ERR, "Allocate cqe mem zone failed, dev_name: %s",
			    dev->data->name);
		err = -ENOMEM;
		goto alloc_cqe_mz_fail;
	}
	memset(cqe_mz->addr, 0, rq_depth * sizeof(*rxq->rx_cqe));
	rxq->cqe_mz = cqe_mz;
	rxq->cqe_start_paddr = cqe_mz->iova;
	rxq->cqe_start_vaddr = cqe_mz->addr;
	rxq->rx_cqe = (struct hinic3_rq_cqe *)rxq->cqe_start_vaddr;

	wqe_count = hinic3_rx_fill_wqe(rxq);
	if (wqe_count != rq_depth) {
		PMD_DRV_LOG(ERR, "Fill rx wqe failed, wqe_count: %d, dev_name: %s",
				wqe_count, dev->data->name);
		err = -ENOMEM;
		goto fill_rx_wqe_fail;
	}
	/* Record rxq pointer in rte_eth rx_queues. */
	dev->data->rx_queues[qid] = rxq;

	return 0;

fill_rx_wqe_fail:
	hinic3_memzone_free(rxq->cqe_mz);
alloc_cqe_mz_fail:
	rte_free(rxq->rx_info);

alloc_rx_info_fail:
	hinic3_memzone_free(rxq->rq_mz);

alloc_rq_mz_fail:
alloc_db_err_fail:
	hinic3_memzone_free(rxq->pi_mz);

alloc_pi_mz_fail:
adjust_bufsize_fail:
	rte_free(rxq);
	nic_dev->rxqs[qid] = NULL;

	return err;
}

/**
 * Create the transmit queue.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] queue_idx
 * Transmit queue index.
 * @param[in] nb_desc
 * Number of descriptors for transmit queue.
 * @param[in] socket_id
 * Socket index on which memory must be allocated.
 * @param[in] tx_conf
 * Tx queue configuration parameters (unused_).
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_tx_queue_setup(struct rte_eth_dev *dev, uint16_t qid, uint16_t nb_desc,
		      unsigned int socket_id, const struct rte_eth_txconf *tx_conf)
{
	struct hinic3_nic_dev *nic_dev;
	struct hinic3_hwdev *hwdev;
	struct hinic3_txq *txq = NULL;
	const struct rte_memzone *sq_mz = NULL;
	const struct rte_memzone *ci_mz = NULL;
	void *db_addr = NULL;
	uint16_t sq_depth, tx_free_thresh;
	uint32_t queue_buf_size;
	int err;

	nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	hwdev = nic_dev->hwdev;

	/* Queue depth must be power of 2, otherwise will be aligned up. */
	sq_depth = (nb_desc & (nb_desc - 1))
				? ((uint16_t)(1U << (rte_log2_u32(nb_desc) + 1))) : nb_desc;

	/*
	 * Validate number of transmit descriptors.
	 * It must not exceed hardware maximum and minimum.
	 */
	if (sq_depth > HINIC3_MAX_QUEUE_DEPTH ||
	    sq_depth < HINIC3_MIN_QUEUE_DEPTH) {
		PMD_DRV_LOG(ERR,
			    "TX queue depth is out of range from %d to %d",
			    HINIC3_MIN_QUEUE_DEPTH, HINIC3_MAX_QUEUE_DEPTH);
		PMD_DRV_LOG(ERR,
			    "nb_desc: %d, q_depth: %d, port: %d queue: %d",
			    nb_desc, sq_depth, dev->data->port_id, qid);
		return -EINVAL;
	}

	/*
	 * The TX descriptor ring will be cleaned after txq->tx_free_thresh
	 * descriptors are used or if the number of descriptors required
	 * to transmit a packet is greater than the number of free TX
	 * descriptors.
	 * The following constraints must be satisfied:
	 * - tx_free_thresh must be greater than 0.
	 * - tx_free_thresh must be less than the size of the ring minus 1.
	 * When set to zero use default values.
	 */
	tx_free_thresh = tx_conf->tx_free_thresh
				 ? tx_conf->tx_free_thresh : HINIC3_DEFAULT_TX_FREE_THRESH;
	if (tx_free_thresh >= (sq_depth - 1)) {
		PMD_DRV_LOG(ERR,
			    "tx_free_thresh must be less than the number of tx descriptors minus 1, tx_free_thresh: %d port: %d queue: %d",
			    tx_free_thresh, dev->data->port_id, qid);
		return -EINVAL;
	}

	txq = rte_zmalloc_socket("hinic3_tx_queue", sizeof(struct hinic3_txq),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (!txq) {
		PMD_DRV_LOG(ERR, "Allocate txq[%d] failed, dev_name: %s", qid,
			    dev->data->name);
		return -ENOMEM;
	}
	nic_dev->txqs[qid] = txq;
	txq->nic_dev = nic_dev;
	txq->q_id = qid;
	txq->q_depth = sq_depth;
	txq->q_mask = sq_depth - 1;
	txq->wqebb_shift = HINIC3_SQ_WQEBB_SHIFT;
	txq->wqebb_size = (uint16_t)RTE_BIT32(txq->wqebb_shift);
	txq->tx_free_thresh = tx_free_thresh;
	txq->owner = 1;
	txq->cos = nic_dev->default_cos;
	txq->tx_deferred_start = tx_conf->tx_deferred_start;

	ci_mz = hinic3_dma_zone_reserve(dev, "hinic3_sq_ci", qid,
					HINIC3_CI_Q_ADDR_SIZE,
					HINIC3_CI_Q_ADDR_SIZE, socket_id);
	if (!ci_mz) {
		PMD_DRV_LOG(ERR, "Allocate txq[%d] ci_mz failed, dev_name: %s",
			    qid, dev->data->name);
		err = -ENOMEM;
		goto alloc_ci_mz_fail;
	}
	txq->ci_mz = ci_mz;
	txq->ci_dma_base = ci_mz->iova;
	txq->ci_vaddr_base = (volatile uint16_t *)ci_mz->addr;

	queue_buf_size = RTE_BIT32(txq->wqebb_shift) * sq_depth;
	sq_mz = hinic3_dma_zone_reserve(dev, "hinic3_sq_mz", qid,
					queue_buf_size, RTE_PGSIZE_256K, socket_id);
	if (!sq_mz) {
		PMD_DRV_LOG(ERR, "Allocate txq[%d] sq_mz failed, dev_name: %s",
			    qid, dev->data->name);
		err = -ENOMEM;
		goto alloc_sq_mz_fail;
	}
	memset(sq_mz->addr, 0, queue_buf_size);
	txq->sq_mz = sq_mz;
	txq->queue_buf_paddr = sq_mz->iova;
	txq->queue_buf_vaddr = sq_mz->addr;
	txq->sq_head_addr = (uint64_t)txq->queue_buf_vaddr;
	txq->sq_bot_sge_addr = txq->sq_head_addr + queue_buf_size;

	err = hinic3_alloc_db_addr(hwdev, &db_addr, HINIC3_DB_TYPE_SQ);
	if (err) {
		PMD_DRV_LOG(ERR, "Alloc sq doorbell addr failed");
		goto alloc_db_err_fail;
	}
	txq->db_addr = db_addr;

	txq->tx_info = rte_zmalloc_socket("tx_info",
					  sq_depth * sizeof(*txq->tx_info),
					  RTE_CACHE_LINE_SIZE, socket_id);
	if (!txq->tx_info) {
		PMD_DRV_LOG(ERR, "Allocate tx_info failed, dev_name: %s",
			    dev->data->name);
		err = -ENOMEM;
		goto alloc_tx_info_fail;
	}

	/* Record txq pointer in rte_eth tx_queues. */
	dev->data->tx_queues[qid] = txq;

	return 0;

alloc_tx_info_fail:
alloc_db_err_fail:
	hinic3_memzone_free(txq->sq_mz);

alloc_sq_mz_fail:
	hinic3_memzone_free(txq->ci_mz);

alloc_ci_mz_fail:
	rte_free(txq);
	return err;
}

static void
hinic3_rx_queue_release(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct hinic3_rxq *rxq = dev->data->rx_queues[queue_id];
	struct hinic3_nic_dev *nic_dev = rxq->nic_dev;

	PMD_DRV_LOG(DEBUG, "%s rxq_idx:%d queue release.",
		    rxq->nic_dev->dev_name, rxq->q_id);

	hinic3_free_rxq_mbufs(rxq);

	hinic3_memzone_free(rxq->cqe_mz);

	rte_free(rxq->rx_info);
	rxq->rx_info = NULL;

	hinic3_memzone_free(rxq->rq_mz);

	hinic3_memzone_free(rxq->pi_mz);

	nic_dev->rxqs[rxq->q_id] = NULL;
	rte_free(rxq);
}

static void
hinic3_tx_queue_release(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct hinic3_txq *txq = dev->data->tx_queues[queue_id];
	struct hinic3_nic_dev *nic_dev = txq->nic_dev;

	PMD_DRV_LOG(DEBUG, "%s txq_idx:%d queue release.",
		    txq->nic_dev->dev_name, txq->q_id);

	hinic3_free_txq_mbufs(txq);

	rte_free(txq->tx_info);
	txq->tx_info = NULL;

	hinic3_memzone_free(txq->sq_mz);

	hinic3_memzone_free(txq->ci_mz);

	nic_dev->txqs[txq->q_id] = NULL;
	rte_free(txq);
}

/**
 * Start RXQ and enables flow director (fdir) filter for RXQ.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] rq_id
 * RX queue ID to be started.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_dev_rx_queue_start(struct rte_eth_dev *dev, uint16_t rq_id)
{
	struct hinic3_rxq *rxq = dev->data->rx_queues[rq_id];
	int rc;

	rc = hinic3_start_rq(dev, rxq);
	if (rc) {
		PMD_DRV_LOG(ERR,
			    "Start rx queue failed, eth_dev:%s, queue_idx:%d",
			    dev->data->name, rq_id);
		return rc;
	}
	dev->data->rx_queue_state[rq_id] = RTE_ETH_QUEUE_STATE_STARTED;

	rc = hinic3_enable_rxq_fdir_filter(dev, rq_id, true);
	if (rc) {
		PMD_DRV_LOG(ERR, "Failed to enable rq : %d fdir filter.",
			    rq_id);
		return rc;
	}
	return 0;
}

/**
 * Stop RXQ and disable flow director (fdir) filter for RXQ.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] rq_id
 * RX queue ID to be stopped.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_dev_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rq_id)
{
	struct hinic3_rxq *rxq = dev->data->rx_queues[rq_id];
	int rc;

	rc = hinic3_enable_rxq_fdir_filter(dev, rq_id, false);
	if (rc) {
		PMD_DRV_LOG(ERR, "Failed to disable rq : %d fdir filter.", rq_id);
		return rc;
	}
	rc = hinic3_stop_rq(dev, rxq);
	if (rc) {
		PMD_DRV_LOG(ERR,
			    "Stop rx queue failed, eth_dev:%s, queue_idx:%d",
			    dev->data->name, rq_id);
		return rc;
	}
	dev->data->rx_queue_state[rq_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

static int
hinic3_dev_tx_queue_start(struct rte_eth_dev *dev, uint16_t sq_id)
{
	struct hinic3_txq *txq = dev->data->tx_queues[sq_id];

	PMD_DRV_LOG(DEBUG, "Start tx queue, eth_dev:%s, queue_idx:%d",
		    dev->data->name, sq_id);

	HINIC3_SET_TXQ_STARTED(txq);
	dev->data->tx_queue_state[sq_id] = RTE_ETH_QUEUE_STATE_STARTED;
	return 0;
}

static int
hinic3_dev_tx_queue_stop(struct rte_eth_dev *dev, uint16_t sq_id)
{
	struct hinic3_txq *txq = dev->data->tx_queues[sq_id];
	int rc;

	rc = hinic3_stop_sq(txq);
	if (rc) {
		PMD_DRV_LOG(ERR,
			    "Stop tx queue failed, eth_dev:%s, queue_idx:%d",
			    dev->data->name, sq_id);
		return rc;
	}
	HINIC3_SET_TXQ_STOPPED(txq);
	dev->data->tx_queue_state[sq_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

int
hinic3_dev_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = PCI_DEV_TO_INTR_HANDLE(pci_dev);
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	uint16_t msix_intr;

	if (!rte_intr_dp_is_en(intr_handle) || !intr_handle->intr_vec)
		return 0;

	msix_intr = (uint16_t)intr_handle->intr_vec[queue_id];
	hinic3_set_msix_auto_mask_state(nic_dev->hwdev, msix_intr,
					HINIC3_SET_MSIX_AUTO_MASK);
	hinic3_set_msix_state(nic_dev->hwdev, msix_intr, HINIC3_MSIX_ENABLE);

	return 0;
}

int
hinic3_dev_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = PCI_DEV_TO_INTR_HANDLE(pci_dev);
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	uint16_t msix_intr;

	if (!rte_intr_dp_is_en(intr_handle) || !intr_handle->intr_vec)
		return 0;

	msix_intr = (uint16_t)intr_handle->intr_vec[queue_id];
	hinic3_set_msix_auto_mask_state(nic_dev->hwdev, msix_intr,
					HINIC3_CLR_MSIX_AUTO_MASK);
	hinic3_set_msix_state(nic_dev->hwdev, msix_intr, HINIC3_MSIX_DISABLE);
	hinic3_misx_intr_clear_resend_bit(nic_dev->hwdev, msix_intr,
					  MSIX_RESEND_TIMER_CLEAR);

	return 0;
}

static int
hinic3_set_lro(struct hinic3_nic_dev *nic_dev, struct rte_eth_conf *dev_conf)
{
	bool lro_en;
	int max_lro_size, lro_max_pkt_len;
	int err;

	/* Config lro. */
	lro_en = dev_conf->rxmode.offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO ? true
									: false;
	max_lro_size = (int)(dev_conf->rxmode.max_lro_pkt_size);
	/* `max_lro_size` is divisible by `HINIC3_LRO_UNIT_WQE_SIZE`. */
	lro_max_pkt_len = max_lro_size / HINIC3_LRO_UNIT_WQE_SIZE
				  ? max_lro_size / HINIC3_LRO_UNIT_WQE_SIZE : 1;

	PMD_DRV_LOG(DEBUG,
		    "max_lro_size: %d, rx_buff_len: %d, lro_max_pkt_len: %d",
		    max_lro_size, nic_dev->rx_buff_len, lro_max_pkt_len);
	PMD_DRV_LOG(DEBUG, "max_rx_pkt_len: %d",
		    HINIC3_MAX_RX_PKT_LEN(dev_conf->rxmode));
	err = hinic3_set_rx_lro_state(nic_dev->hwdev, lro_en,
				      HINIC3_LRO_DEFAULT_TIME_LIMIT, lro_max_pkt_len);
	if (err)
		PMD_DRV_LOG(ERR, "Set lro state failed, err: %d", err);
	return err;
}

static int
hinic3_set_vlan(struct rte_eth_dev *dev, struct rte_eth_conf *dev_conf)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	bool vlan_filter, vlan_strip;
	int err;

	/* Config vlan filter. */
	vlan_filter = dev_conf->rxmode.offloads &
		      RTE_ETH_RX_OFFLOAD_VLAN_FILTER;

	err = hinic3_set_vlan_filter(nic_dev->hwdev, vlan_filter);
	if (err) {
		PMD_DRV_LOG(ERR,
			    "Config vlan filter failed, device: %s, port_id: %d, err: %d",
			    nic_dev->dev_name, dev->data->port_id, err);
		return err;
	}

	/* Config vlan stripping. */
	vlan_strip = dev_conf->rxmode.offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP;

	err = hinic3_set_rx_vlan_offload(nic_dev->hwdev, vlan_strip);
	if (err) {
		PMD_DRV_LOG(ERR,
			    "Config vlan strip failed, device: %s, port_id: %d, err: %d",
			    nic_dev->dev_name, dev->data->port_id, err);
	}

	return err;
}

/**
 * Configure RX mode, checksum offload, LRO, RSS, VLAN and initialize the RXQ
 * list.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_set_rxtx_configure(struct rte_eth_dev *dev)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct rte_eth_conf *dev_conf = &dev->data->dev_conf;
	struct rte_eth_rss_conf *rss_conf = NULL;
	int err;

	/* Config rx mode. */
	err = hinic3_set_rx_mode(nic_dev->hwdev, HINIC3_DEFAULT_RX_MODE);
	if (err) {
		PMD_DRV_LOG(ERR, "Set rx_mode: 0x%x failed",
			    HINIC3_DEFAULT_RX_MODE);
		return err;
	}
	nic_dev->rx_mode = HINIC3_DEFAULT_RX_MODE;

	/* Config rx checksum offload. */
	if (dev_conf->rxmode.offloads & RTE_ETH_RX_OFFLOAD_CHECKSUM)
		nic_dev->rx_csum_en = HINIC3_DEFAULT_RX_CSUM_OFFLOAD;

	err = hinic3_set_lro(nic_dev, dev_conf);
	if (err) {
		PMD_DRV_LOG(ERR, "Set lro failed");
		return err;
	}
	/* Config RSS. */
	if ((dev_conf->rxmode.mq_mode & RTE_ETH_MQ_RX_RSS_FLAG) &&
	    nic_dev->num_rqs > 1) {
		rss_conf = &dev_conf->rx_adv_conf.rss_conf;
		err = hinic3_update_rss_config(dev, rss_conf);
		if (err) {
			PMD_DRV_LOG(ERR, "Set rss config failed, err: %d", err);
			return err;
		}
	}

	err = hinic3_set_vlan(dev, dev_conf);
	if (err) {
		PMD_DRV_LOG(ERR, "Set vlan failed, err: %d", err);
		return err;
	}

	hinic3_init_rx_queue_list(nic_dev);

	return 0;
}

/**
 * Disable RX mode and RSS, and free associated resources.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 */
static void
hinic3_remove_rxtx_configure(struct rte_eth_dev *dev)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	uint8_t prio_tc[HINIC3_DCB_UP_MAX] = {0};

	hinic3_set_rx_mode(nic_dev->hwdev, 0);

	if (nic_dev->rss_state == HINIC3_RSS_ENABLE) {
		hinic3_rss_cfg(nic_dev->hwdev, HINIC3_RSS_DISABLE, 0, prio_tc);
		hinic3_rss_template_free(nic_dev->hwdev);
		nic_dev->rss_state = HINIC3_RSS_DISABLE;
	}
}

static bool
hinic3_find_vlan_filter(struct hinic3_nic_dev *nic_dev, uint16_t vlan_id)
{
	uint32_t vid_idx, vid_bit;

	vid_idx = HINIC3_VFTA_IDX(vlan_id);
	vid_bit = HINIC3_VFTA_BIT(vlan_id);

	return (nic_dev->vfta[vid_idx] & vid_bit) ? true : false;
}

static void
hinic3_store_vlan_filter(struct hinic3_nic_dev *nic_dev, uint16_t vlan_id, bool on)
{
	uint32_t vid_idx, vid_bit;

	vid_idx = HINIC3_VFTA_IDX(vlan_id);
	vid_bit = HINIC3_VFTA_BIT(vlan_id);

	if (on)
		nic_dev->vfta[vid_idx] |= vid_bit;
	else
		nic_dev->vfta[vid_idx] &= ~vid_bit;
}

static void
hinic3_remove_all_vlanid(struct rte_eth_dev *dev)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	int vlan_id;
	uint16_t func_id;

	func_id = hinic3_global_func_id(nic_dev->hwdev);

	for (vlan_id = 1; vlan_id < RTE_ETHER_MAX_VLAN_ID; vlan_id++) {
		if (hinic3_find_vlan_filter(nic_dev, vlan_id)) {
			hinic3_del_vlan(nic_dev->hwdev, vlan_id, func_id);
			hinic3_store_vlan_filter(nic_dev, vlan_id, false);
		}
	}
}

static void
hinic3_disable_interrupt(struct rte_eth_dev *dev)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	if (!hinic3_get_bit(HINIC3_DEV_INIT, &nic_dev->dev_status))
		return;

	/* Disable rte interrupt. */
	rte_intr_disable(PCI_DEV_TO_INTR_HANDLE(pci_dev));
	rte_intr_callback_unregister(PCI_DEV_TO_INTR_HANDLE(pci_dev),
				     hinic3_dev_interrupt_handler, (void *)dev);
}

static void
hinic3_enable_interrupt(struct rte_eth_dev *dev)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	if (!hinic3_get_bit(HINIC3_DEV_INIT, &nic_dev->dev_status))
		return;

	/* Enable rte interrupt. */
	rte_intr_enable(PCI_DEV_TO_INTR_HANDLE(pci_dev));
	rte_intr_callback_register(PCI_DEV_TO_INTR_HANDLE(pci_dev),
				   hinic3_dev_interrupt_handler, (void *)dev);
}

#define HINIC3_RX_VEC_START RTE_INTR_VEC_RXTX_OFFSET

/** Dp interrupt msix attribute. */
#define HINIC3_TXRX_MSIX_PENDING_LIMIT	  2
#define HINIC3_TXRX_MSIX_COALESCE_TIMER	  2
#define HINIC3_TXRX_MSIX_RESEND_TIMER_CFG 7

static int
hinic3_init_rxq_msix_attr(void *hwdev, uint16_t msix_index)
{
	struct interrupt_info info = {0};
	int err;

	info.lli_set = 0;
	info.interrupt_coalesce_set = 1;
	info.pending_limt = HINIC3_TXRX_MSIX_PENDING_LIMIT;
	info.coalesce_timer_cfg = HINIC3_TXRX_MSIX_COALESCE_TIMER;
	info.resend_timer_cfg = HINIC3_TXRX_MSIX_RESEND_TIMER_CFG;
	info.msix_index = msix_index;
	err = hinic3_set_interrupt_cfg(hwdev, info);
	if (err) {
		PMD_DRV_LOG(ERR, "Set msix attr failed, msix_index %d",
			    msix_index);
		return -EFAULT;
	}

	return 0;
}

static void
hinic3_deinit_rxq_intr(struct rte_eth_dev *dev)
{
	struct rte_intr_handle *intr_handle = dev->intr_handle;

	rte_intr_efd_disable(intr_handle);
	if (intr_handle->intr_vec) {
		rte_free(intr_handle->intr_vec);
		intr_handle->intr_vec = NULL;
	}
}

/**
 * Initialize RX queue interrupts by enabling MSI-X, allocate interrupt vectors,
 * and configure interrupt attributes for each RX queue.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 *
 * @return
 * 0 on success, negative error code on failure.
 * - -ENOTSUP if MSI-X interrupts are not supported.
 * - Error code if enabling event file descriptors fails.
 * - -ENOMEM if allocating interrupt vectors fails.
 */
static int
hinic3_init_rxq_intr(struct rte_eth_dev *dev)
{
	struct rte_intr_handle *intr_handle = NULL;
	struct hinic3_nic_dev *nic_dev = NULL;
	struct hinic3_rxq *rxq = NULL;
	uint32_t nb_rx_queues, i;
	int err;

	intr_handle = dev->intr_handle;
	nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	if (!dev->data->dev_conf.intr_conf.rxq)
		return 0;

	if (!rte_intr_cap_multiple(intr_handle)) {
		PMD_DRV_LOG(ERR, "Rx queue interrupts require MSI-X interrupts (vfio-pci driver)");
		return -ENOTSUP;
	}

	nb_rx_queues = dev->data->nb_rx_queues;
	err = rte_intr_efd_enable(intr_handle, nb_rx_queues);
	if (err) {
		PMD_DRV_LOG(ERR,
			"Failed to enable event fds for Rx queue interrupts");
		return err;
	}

	intr_handle->intr_vec =
		rte_zmalloc("hinic_intr_vec", nb_rx_queues * sizeof(int), 0);
	if (intr_handle->intr_vec == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate intr_vec");
		rte_intr_efd_disable(intr_handle);
		return -ENOMEM;
	}
	intr_handle->vec_list_size = nb_rx_queues;
	for (i = 0; i < nb_rx_queues; i++)
		intr_handle->intr_vec[i] = (int)(i + HINIC3_RX_VEC_START);

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		rxq->dp_intr_en = 1;
		rxq->msix_entry_idx = (uint16_t)intr_handle->intr_vec[i];

		err = hinic3_init_rxq_msix_attr(nic_dev->hwdev,
						rxq->msix_entry_idx);
		if (err) {
			hinic3_deinit_rxq_intr(dev);
			return err;
		}
	}

	return 0;
}

static void
hinic3_disable_queue_intr(struct rte_eth_dev *dev)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct rte_intr_handle *intr_handle = dev->intr_handle;
	int msix_intr;
	int i;

	if (intr_handle->intr_vec == NULL)
		return;

	for (i = 0; i < nic_dev->num_rqs; i++) {
		msix_intr = intr_handle->intr_vec[i];
		hinic3_set_msix_state(nic_dev->hwdev, msix_intr,
				      HINIC3_MSIX_DISABLE);
		hinic3_misx_intr_clear_resend_bit(nic_dev->hwdev,
						  msix_intr,
						  MSIX_RESEND_TIMER_CLEAR);
	}
}

/**
 * Start the device.
 *
 * Initialize function table, TXQ and TXQ context, configure RX offload, and
 * enable vport and port to prepare receiving packets.
 *
 * @param[in] eth_dev
 * Pointer to ethernet device structure.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_dev_start(struct rte_eth_dev *eth_dev)
{
	struct hinic3_nic_dev *nic_dev = NULL;
	uint64_t nic_features;
	struct hinic3_rxq *rxq = NULL;
	int i;
	int err;

	nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);
	err = hinic3_copy_mempool_init(nic_dev);
	if (err) {
		PMD_DRV_LOG(ERR, "Create copy mempool failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_mpool_fail;
	}
	hinic3_update_msix_info(nic_dev->hwdev->hwif);
	hinic3_disable_interrupt(eth_dev);
	err = hinic3_init_rxq_intr(eth_dev);
	if (err) {
		PMD_DRV_LOG(ERR, "Init rxq intr fail, eth_dev:%s",
			    eth_dev->data->name);
		goto init_rxq_intr_fail;
	}

	hinic3_get_func_rx_buf_size(nic_dev);
	err = hinic3_init_function_table(nic_dev->hwdev, nic_dev->rx_buff_len);
	if (err) {
		PMD_DRV_LOG(ERR, "Init function table failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_func_tbl_fail;
	}

	nic_features = hinic3_get_driver_feature(nic_dev);
	/*
	 * You can update the features supported by the driver according to the
	 * scenario here.
	 */
	nic_features &= DEFAULT_DRV_FEATURE;
	hinic3_update_driver_feature(nic_dev, nic_features);

	err = hinic3_set_feature_to_hw(nic_dev->hwdev, &nic_dev->feature_cap, 1);
	if (err) {
		PMD_DRV_LOG(ERR,
			    "Failed to set nic features to hardware, err %d",
			    err);
		goto get_feature_err;
	}

	/* Reset rx and tx queue. */
	hinic3_reset_rx_queue(eth_dev);
	hinic3_reset_tx_queue(eth_dev);

	/* Init txq and rxq context. */
	err = hinic3_init_qp_ctxts(nic_dev);
	if (err) {
		PMD_DRV_LOG(ERR, "Init qp context failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_qp_fail;
	}

	/* Set default mtu. */
	err = hinic3_set_port_mtu(nic_dev->hwdev, nic_dev->mtu_size);
	if (err) {
		PMD_DRV_LOG(ERR, "Set mtu_size[%d] failed, dev_name: %s",
			    nic_dev->mtu_size, eth_dev->data->name);
		goto set_mtu_fail;
	}
	eth_dev->data->mtu = nic_dev->mtu_size;

	/* Set rx configuration: rss/checksum/rxmode/lro. */
	err = hinic3_set_rxtx_configure(eth_dev);
	if (err) {
		PMD_DRV_LOG(ERR, "Set rx config failed, dev_name: %s",
			    eth_dev->data->name);
		goto set_rxtx_config_fail;
	}

	/* Enable dev interrupt. */
	hinic3_enable_interrupt(eth_dev);
	err = hinic3_start_all_rqs(eth_dev);
	if (err) {
		PMD_DRV_LOG(ERR, "Set rx config failed, dev_name: %s",
			    eth_dev->data->name);
		goto start_rqs_fail;
	}

	hinic3_start_all_sqs(eth_dev);

	/* Open virtual port and ready to start packet receiving. */
	err = hinic3_set_vport_enable(nic_dev->hwdev, true);
	if (err) {
		PMD_DRV_LOG(ERR, "Enable vport failed, dev_name: %s",
			    eth_dev->data->name);
		goto en_vport_fail;
	}

	/* Open physical port and start packet receiving. */
	err = hinic3_set_port_enable(nic_dev->hwdev, true);
	if (err) {
		PMD_DRV_LOG(ERR, "Enable physical port failed, dev_name: %s",
			    eth_dev->data->name);
		goto en_port_fail;
	}

	/* Update eth_dev link status. */
	if (eth_dev->data->dev_conf.intr_conf.lsc != 0)
		hinic3_link_update(eth_dev, 0);

	hinic3_set_bit(HINIC3_DEV_START, &nic_dev->dev_status);

	return 0;

en_port_fail:
	hinic3_set_vport_enable(nic_dev->hwdev, false);

en_vport_fail:
	/* Flush tx && rx chip resources in case of setting vport fake fail. */
	hinic3_flush_qps_res(nic_dev->hwdev);
	rte_delay_ms(DEV_START_DELAY_MS);
	for (i = 0; i < nic_dev->num_rqs; i++) {
		rxq = nic_dev->rxqs[i];
		hinic3_remove_rq_from_rx_queue_list(nic_dev, rxq->q_id);
		hinic3_free_rxq_mbufs(rxq);
		hinic3_dev_rx_queue_intr_disable(eth_dev, rxq->q_id);
		eth_dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
		eth_dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	}
start_rqs_fail:
	hinic3_remove_rxtx_configure(eth_dev);

set_rxtx_config_fail:
set_mtu_fail:
	hinic3_free_qp_ctxts(nic_dev->hwdev);

init_qp_fail:
get_feature_err:
init_func_tbl_fail:
	hinic3_deinit_rxq_intr(eth_dev);
init_rxq_intr_fail:
	hinic3_copy_mempool_uninit(nic_dev);
init_mpool_fail:
	return err;
}

/**
 * Stop the device.
 *
 * Stop phy port and vport, flush pending io request, clean context configure
 * and free io resource.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 */
static int
hinic3_dev_stop(struct rte_eth_dev *dev)
{
	struct hinic3_nic_dev *nic_dev;
	struct rte_eth_link link;
	int err;

	nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	if (!hinic3_test_and_clear_bit(HINIC3_DEV_START,
				       &nic_dev->dev_status)) {
		PMD_DRV_LOG(INFO, "Device %s already stopped",
			    nic_dev->dev_name);
		return 0;
	}

	/* Stop phy port and vport. */
	err = hinic3_set_port_enable(nic_dev->hwdev, false);
	if (err)
		PMD_DRV_LOG(WARNING,
			    "Disable phy port failed, error: %d, dev_name: %s, port_id: %d",
			    err, dev->data->name, dev->data->port_id);

	err = hinic3_set_vport_enable(nic_dev->hwdev, false);
	if (err)
		PMD_DRV_LOG(WARNING,
			    "Disable vport failed, error: %d, dev_name: %s, port_id: %d",
			    err, dev->data->name, dev->data->port_id);

	/* Clear recorded link status. */
	memset(&link, 0, sizeof(link));
	rte_eth_linkstatus_set(dev, &link);

	/* Disable dp interrupt. */
	hinic3_disable_queue_intr(dev);
	hinic3_deinit_rxq_intr(dev);

	/* Flush pending io request. */
	hinic3_flush_txqs(nic_dev);

	/* After set vport disable 100ms, no packets will be send to host. */
	rte_delay_ms(DEV_STOP_DELAY_MS);

	hinic3_flush_qps_res(nic_dev->hwdev);

	/* Clean RSS table and rx_mode. */
	hinic3_remove_rxtx_configure(dev);

	/* Clean root context. */
	hinic3_free_qp_ctxts(nic_dev->hwdev);

	/* Free all tx and rx mbufs. */
	hinic3_free_all_txq_mbufs(nic_dev);
	hinic3_free_all_rxq_mbufs(nic_dev);

	/* Free mempool. */
	hinic3_copy_mempool_uninit(nic_dev);
	return 0;
}

static void
hinic3_dev_release(struct rte_eth_dev *eth_dev)
{
	struct hinic3_nic_dev *nic_dev =
		HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	int qid;

	/* Release io resource. */
	for (qid = 0; qid < nic_dev->num_sqs; qid++)
		hinic3_tx_queue_release(eth_dev, qid);

	for (qid = 0; qid < nic_dev->num_rqs; qid++)
		hinic3_rx_queue_release(eth_dev, qid);

	hinic3_deinit_sw_rxtxqs(nic_dev);

	hinic3_deinit_mac_addr(eth_dev);
	rte_free(nic_dev->mc_list);

	hinic3_remove_all_vlanid(eth_dev);

	hinic3_clear_bit(HINIC3_DEV_INTR_EN, &nic_dev->dev_status);
	hinic3_set_msix_state(nic_dev->hwdev, 0, HINIC3_MSIX_DISABLE);
	rte_intr_disable(PCI_DEV_TO_INTR_HANDLE(pci_dev));
	rte_intr_callback_unregister(PCI_DEV_TO_INTR_HANDLE(pci_dev),
				     hinic3_dev_interrupt_handler,
				     (void *)eth_dev);

	hinic3_free_nic_hwdev(nic_dev->hwdev);
	hinic3_free_hwdev(nic_dev->hwdev);

	eth_dev->rx_pkt_burst = NULL;
	eth_dev->tx_pkt_burst = NULL;
	eth_dev->dev_ops = NULL;

	rte_free(nic_dev->hwdev);
	nic_dev->hwdev = NULL;
}

/**
 * Close the device.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_dev_close(struct rte_eth_dev *eth_dev)
{
	struct hinic3_nic_dev *nic_dev =
		HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);
	int ret;

	if (hinic3_test_and_set_bit(HINIC3_DEV_CLOSE, &nic_dev->dev_status)) {
		PMD_DRV_LOG(WARNING, "Device %s already closed",
			    nic_dev->dev_name);
		return 0;
	}

	ret = hinic3_dev_stop(eth_dev);

	hinic3_dev_release(eth_dev);
	return ret;
}

#define MIN_RX_BUFFER_SIZE	      256
#define MIN_RX_BUFFER_SIZE_SMALL_MODE 1518

static int
hinic3_dev_set_mtu(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	int err = 0;

	PMD_DRV_LOG(DEBUG, "Set port mtu, port_id: %d, mtu: %d, max_pkt_len: %d",
		    dev->data->port_id, mtu, HINIC3_MTU_TO_PKTLEN(mtu));

	err = hinic3_set_port_mtu(nic_dev->hwdev, mtu);
	if (err) {
		PMD_DRV_LOG(ERR, "Set port mtu failed, err: %d", err);
		return err;
	}

	/* Update max frame size. */
	HINIC3_MAX_RX_PKT_LEN(dev->data->dev_conf.rxmode) =
		HINIC3_MTU_TO_PKTLEN(mtu);
	nic_dev->mtu_size = mtu;
	return err;
}

/**
 * Add or delete vlan id.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] vlan_id
 * Vlan id is used to filter vlan packets.
 * @param[in] enable
 * Disable or enable vlan filter function.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int enable)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	int err = 0;
	uint16_t func_id;

	if (vlan_id == 0)
		return 0;

	func_id = hinic3_global_func_id(nic_dev->hwdev);

	if (enable) {
		/* If vlanid is already set, just return. */
		if (hinic3_find_vlan_filter(nic_dev, vlan_id)) {
			PMD_DRV_LOG(WARNING, "Vlan %u has been added, device: %s",
				    vlan_id, nic_dev->dev_name);
			return 0;
		}

		err = hinic3_add_vlan(nic_dev->hwdev, vlan_id, func_id);
	} else {
		/* If vlanid can't be found, just return. */
		if (!hinic3_find_vlan_filter(nic_dev, vlan_id)) {
			PMD_DRV_LOG(WARNING,
				    "Vlan %u is not in the vlan filter list, device: %s",
				    vlan_id, nic_dev->dev_name);
			return 0;
		}

		err = hinic3_del_vlan(nic_dev->hwdev, vlan_id, func_id);
	}

	if (err) {
		PMD_DRV_LOG(ERR,
			    "%s vlan failed, func_id: %d, vlan_id: %d, err: %d",
			    enable ? "Add" : "Remove", func_id, vlan_id, err);
		return err;
	}

	hinic3_store_vlan_filter(nic_dev, vlan_id, enable);

	PMD_DRV_LOG(DEBUG, "%s vlan %u succeed, device: %s",
		    enable ? "Add" : "Remove", vlan_id, nic_dev->dev_name);

	return 0;
}

/**
 * Enable or disable vlan offload.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] mask
 * Definitions used for VLAN setting, vlan filter of vlan strip.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;
	bool on;
	int err;

	/* Enable or disable VLAN filter. */
	if (mask & RTE_ETH_VLAN_FILTER_MASK) {
		on = (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_FILTER) ? true : false;
		err = hinic3_set_vlan_filter(nic_dev->hwdev, on);
		if (err) {
			PMD_DRV_LOG(ERR,
				    "%s vlan filter failed, device: %s, port_id: %d",
				    on ? "Enable" : "Disable", nic_dev->dev_name,
				    dev->data->port_id);
			return err;
		}
		PMD_DRV_LOG(DEBUG,
			    "%s vlan filter succeed, device: %s, port_id: %d",
			    on ? "Enable" : "Disable", nic_dev->dev_name,
			    dev->data->port_id);
	}

	/* Enable or disable VLAN stripping. */
	if (mask & RTE_ETH_VLAN_STRIP_MASK) {
		on = (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP) ? true : false;
		err = hinic3_set_rx_vlan_offload(nic_dev->hwdev, on);
		if (err) {
			PMD_DRV_LOG(ERR,
				    "%s vlan strip failed, device: %s, port_id: %d",
				    on ? "Enable" : "Disable", nic_dev->dev_name,
				    dev->data->port_id);
			return err;
		}

		PMD_DRV_LOG(DEBUG,
			    "%s vlan strip succeed, device: %s, port_id: %d",
			    on ? "Enable" : "Disable", nic_dev->dev_name,
			    dev->data->port_id);
	}
	return 0;
}

/**
 * Enable allmulticast mode.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	uint32_t rx_mode = nic_dev->rx_mode | HINIC3_RX_MODE_MC_ALL;
	int err;

	err = hinic3_set_rx_mode(nic_dev->hwdev, rx_mode);
	if (err) {
		PMD_DRV_LOG(ERR, "Enable allmulticast failed, error: %d", err);
		return err;
	}

	nic_dev->rx_mode = rx_mode;

	PMD_DRV_LOG(DEBUG,
		    "Enable allmulticast succeed, nic_dev: %s, port_id: %d",
		    nic_dev->dev_name, dev->data->port_id);
	return 0;
}

/**
 * Disable allmulticast mode.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	uint32_t rx_mode = nic_dev->rx_mode & (~HINIC3_RX_MODE_MC_ALL);
	int err;

	err = hinic3_set_rx_mode(nic_dev->hwdev, rx_mode);
	if (err) {
		PMD_DRV_LOG(ERR, "Disable allmulticast failed, error: %d", err);
		return err;
	}

	nic_dev->rx_mode = rx_mode;

	PMD_DRV_LOG(DEBUG,
		    "Disable allmulticast succeed, nic_dev: %s, port_id: %d",
		    nic_dev->dev_name, dev->data->port_id);
	return 0;
}

/**
 * Get device generic statistics.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[out] stats
 * Stats structure output buffer.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct hinic3_vport_stats vport_stats;
	uint64_t rx_discards_pmd = 0;
	int err;

	err = hinic3_get_vport_stats(nic_dev->hwdev, &vport_stats);
	if (err) {
		PMD_DRV_LOG(ERR, "Get vport stats from fw failed, nic_dev: %s",
			    nic_dev->dev_name);
		return err;
	}

	/* Rx queue stats. */
	for (uint32_t i = 0; i < nic_dev->num_rqs; i++) {
		struct hinic3_rxq *rxq = nic_dev->rxqs[i];
#ifdef HINIC3_XSTAT_MBUF_USE
		rxq->rxq_stats.rx_left_mbuf_bytes =
			rxq->rxq_stats.rx_alloc_mbuf_bytes -
			rxq->rxq_stats.rx_free_mbuf_bytes;
#endif
		rxq->rxq_stats.errors = rxq->rxq_stats.csum_errors +
					rxq->rxq_stats.other_errors;
		if (i < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			stats->q_ipackets[i] = rxq->rxq_stats.packets;
			stats->q_ibytes[i] = rxq->rxq_stats.bytes;
			stats->q_errors[i] = rxq->rxq_stats.errors;
		}
		stats->ierrors += rxq->rxq_stats.errors;
		rx_discards_pmd += rxq->rxq_stats.dropped;
		dev->data->rx_mbuf_alloc_failed += rxq->rxq_stats.rx_nombuf;
	}

	/* Tx queue stats. */
	for (uint32_t i = 0; i < nic_dev->num_sqs; i++) {
		struct hinic3_txq *txq = nic_dev->txqs[i];
		stats->oerrors += (txq->txq_stats.tx_busy + txq->txq_stats.offload_errors);
		if (i < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			stats->q_opackets[i] = txq->txq_stats.packets;
			stats->q_obytes[i] = txq->txq_stats.bytes;
		}
	}

	/* Vport stats. */
	stats->oerrors += vport_stats.tx_discard_vport;

	stats->imissed = vport_stats.rx_discard_vport + rx_discards_pmd;

	stats->ipackets = vport_stats.rx_unicast_pkts_vport +
				vport_stats.rx_multicast_pkts_vport +
				vport_stats.rx_broadcast_pkts_vport;

	stats->opackets = vport_stats.tx_unicast_pkts_vport +
				vport_stats.tx_multicast_pkts_vport +
				vport_stats.tx_broadcast_pkts_vport;

	stats->ibytes = vport_stats.rx_unicast_bytes_vport +
				vport_stats.rx_multicast_bytes_vport +
				vport_stats.rx_broadcast_bytes_vport;

	stats->obytes = vport_stats.tx_unicast_bytes_vport +
				vport_stats.tx_multicast_bytes_vport +
				vport_stats.tx_broadcast_bytes_vport;
	return 0;
}

/**
 * Clear device generic statistics.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_dev_stats_reset(struct rte_eth_dev *dev)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct hinic3_rxq *rxq = NULL;
	struct hinic3_txq *txq = NULL;
	int qid;
	int err;

	dev->data->rx_mbuf_alloc_failed = 0;

	err = hinic3_clear_vport_stats(nic_dev->hwdev);
	if (err)
		return err;

	for (qid = 0; qid < nic_dev->num_rqs; qid++) {
		rxq = nic_dev->rxqs[qid];
		memset(&rxq->rxq_stats, 0, sizeof(struct hinic3_rxq_stats));
	}

	for (qid = 0; qid < nic_dev->num_sqs; qid++) {
		txq = nic_dev->txqs[qid];
		memset(&txq->txq_stats, 0, sizeof(struct hinic3_txq_stats));
	}

	return 0;
}

/**
 * Get device extended statistics.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[out] xstats
 * Pointer to rte extended stats table.
 * @param[in] n
 * The size of the stats table.
 *
 * @return
 * positive: Number of extended stats on success and stats is filled.
 * negative: Failure.
 */
static int
hinic3_dev_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
		      unsigned int n)
{
	struct hinic3_nic_dev *nic_dev;
	struct mag_phy_port_stats port_stats;
	struct hinic3_vport_stats vport_stats;
	struct hinic3_rxq *rxq = NULL;
	struct hinic3_rxq_stats rxq_stats;
	struct hinic3_txq *txq = NULL;
	struct hinic3_txq_stats txq_stats;
	uint16_t qid;
	uint32_t i, count;
	int err;

	nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	count = hinic3_xstats_calc_num(nic_dev);
	if (n < count)
		return count;

	count = 0;

	/* Get stats from rxq stats structure. */
	for (qid = 0; qid < nic_dev->num_rqs; qid++) {
		rxq = nic_dev->rxqs[qid];

#ifdef HINIC3_XSTAT_RXBUF_INFO
		hinic3_get_stats(rxq);
#endif

#ifdef HINIC3_XSTAT_MBUF_USE
		rxq->rxq_stats.rx_left_mbuf_bytes =
			rxq->rxq_stats.rx_alloc_mbuf_bytes -
			rxq->rxq_stats.rx_free_mbuf_bytes;
#endif
		rxq->rxq_stats.errors = rxq->rxq_stats.csum_errors +
					rxq->rxq_stats.other_errors;

		rxq_stats = rxq->rxq_stats;

		for (i = 0; i < HINIC3_RXQ_XSTATS_NUM; i++) {
			xstats[count].value = *(uint64_t *)(((char *)&rxq_stats) +
					    hinic3_rxq_stats_strings[i].offset);
			xstats[count].id = count;
			count++;
		}
	}

	/* Get stats from txq stats structure. */
	for (qid = 0; qid < nic_dev->num_sqs; qid++) {
		txq = nic_dev->txqs[qid];
		txq_stats = txq->txq_stats;

		for (i = 0; i < HINIC3_TXQ_XSTATS_NUM; i++) {
			xstats[count].value = *(uint64_t *)(((char *)&txq_stats) +
					    hinic3_txq_stats_strings[i].offset);
			xstats[count].id = count;
			count++;
		}
	}

	/* Get stats from vport stats structure. */
	err = hinic3_get_vport_stats(nic_dev->hwdev, &vport_stats);
	if (err)
		return err;

	for (i = 0; i < HINIC3_VPORT_XSTATS_NUM; i++) {
		xstats[count].value =
			*(uint64_t *)(((char *)&vport_stats) +
				      hinic3_vport_stats_strings[i].offset);
		xstats[count].id = count;
		count++;
	}

	if (HINIC3_IS_VF(nic_dev->hwdev))
		return count;

	/* Get stats from phy port stats structure. */
	err = hinic3_get_phy_port_stats(nic_dev->hwdev, &port_stats);
	if (err)
		return err;

	for (i = 0; i < HINIC3_PHYPORT_XSTATS_NUM; i++) {
		xstats[count].value =
			*(uint64_t *)(((char *)&port_stats) +
				      hinic3_phyport_stats_strings[i].offset);
		xstats[count].id = count;
		count++;
	}

	return count;
}

/**
 * Clear device extended statistics.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_dev_xstats_reset(struct rte_eth_dev *dev)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	int err;

	err = hinic3_dev_stats_reset(dev);
	if (err)
		return err;

	if (hinic3_func_type(nic_dev->hwdev) != TYPE_VF) {
		err = hinic3_clear_phy_port_stats(nic_dev->hwdev);
		if (err)
			return err;
	}

	return 0;
}

/**
 * Retrieve names of extended device statistics.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[out] xstats_names
 * Buffer to insert names into.
 *
 * @return
 * Number of xstats names.
 */
static int
hinic3_dev_xstats_get_names(struct rte_eth_dev *dev,
			    struct rte_eth_xstat_name *xstats_names,
			    __rte_unused unsigned int limit)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	int count = 0;
	uint16_t i, q_num;

	if (xstats_names == NULL)
		return hinic3_xstats_calc_num(nic_dev);

	/* Get pmd rxq stats name. */
	for (q_num = 0; q_num < nic_dev->num_rqs; q_num++) {
		for (i = 0; i < HINIC3_RXQ_XSTATS_NUM; i++) {
			snprintf(xstats_names[count].name,
				 sizeof(xstats_names[count].name),
				 "rxq%d_%s_pmd", q_num,
				 hinic3_rxq_stats_strings[i].name);
			count++;
		}
	}

	/* Get pmd txq stats name. */
	for (q_num = 0; q_num < nic_dev->num_sqs; q_num++) {
		for (i = 0; i < HINIC3_TXQ_XSTATS_NUM; i++) {
			snprintf(xstats_names[count].name,
				 sizeof(xstats_names[count].name),
				 "txq%d_%s_pmd", q_num,
				 hinic3_txq_stats_strings[i].name);
			count++;
		}
	}

	/* Get vport stats name. */
	for (i = 0; i < HINIC3_VPORT_XSTATS_NUM; i++) {
		strlcpy(xstats_names[count].name,
			hinic3_vport_stats_strings[i].name,
			sizeof(xstats_names[count].name));
		count++;
	}

	if (HINIC3_IS_VF(nic_dev->hwdev))
		return count;

	/* Get phy port stats name. */
	for (i = 0; i < HINIC3_PHYPORT_XSTATS_NUM; i++) {
		strlcpy(xstats_names[count].name,
			hinic3_phyport_stats_strings[i].name,
			sizeof(xstats_names[count].name));
		count++;
	}

	return count;
}

static void
hinic3_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		    struct rte_eth_rxq_info *qinfo)
{
	struct hinic3_rxq *rxq = dev->data->rx_queues[queue_id];

	qinfo->mp = rxq->mb_pool;
	qinfo->nb_desc = rxq->q_depth;
	qinfo->rx_buf_size = rxq->buf_len;
	qinfo->conf.offloads = dev->data->dev_conf.rxmode.offloads;
	qinfo->conf.rx_free_thresh = rxq->rx_free_thresh;
	qinfo->conf.rx_deferred_start = rxq->rx_deferred_start;
}

static void
hinic3_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		    struct rte_eth_txq_info *qinfo)
{
	struct hinic3_txq *txq = dev->data->tx_queues[queue_id];

	qinfo->nb_desc = txq->q_depth;
	qinfo->conf.offloads = dev->data->dev_conf.txmode.offloads;
	qinfo->conf.tx_free_thresh = txq->tx_free_thresh;
	qinfo->conf.tx_deferred_start = txq->tx_deferred_start;
}

/**
 * Update MAC address.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] addr
 * Pointer to MAC address.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_set_mac_addr(struct rte_eth_dev *dev, struct rte_ether_addr *addr)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	char mac_addr[RTE_ETHER_ADDR_FMT_SIZE];
	uint16_t func_id;
	int err;

	if (!rte_is_valid_assigned_ether_addr(addr)) {
		rte_ether_format_addr(mac_addr, RTE_ETHER_ADDR_FMT_SIZE, addr);
		PMD_DRV_LOG(ERR, "Set invalid MAC address %s", mac_addr);
		return -EINVAL;
	}

	func_id = hinic3_global_func_id(nic_dev->hwdev);
	err = hinic3_update_mac(nic_dev->hwdev,
				nic_dev->default_addr.addr_bytes,
				addr->addr_bytes, 0, func_id);
	if (err)
		return err;

	rte_ether_addr_copy(addr, &nic_dev->default_addr);
	rte_ether_format_addr(mac_addr, RTE_ETHER_ADDR_FMT_SIZE,
			      &nic_dev->default_addr);

	PMD_DRV_LOG(DEBUG, "Set new MAC address %s", mac_addr);
	return 0;
}

/**
 * Remove a MAC address.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] index
 * MAC address index.
 */
static void
hinic3_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	uint16_t func_id;
	int err;

	if (index >= HINIC3_MAX_UC_MAC_ADDRS) {
		PMD_DRV_LOG(ERR, "Remove MAC index(%u) is out of range", index);
		return;
	}

	func_id = hinic3_global_func_id(nic_dev->hwdev);
	err = hinic3_del_mac(nic_dev->hwdev,
			     dev->data->mac_addrs[index].addr_bytes, 0, func_id);
	if (err)
		PMD_DRV_LOG(ERR, "Remove MAC index(%u) failed", index);
}

/**
 * Add a MAC address.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] mac_addr
 * MAC address to register.
 * @param[in] index
 * MAC address index.
 * @param[in] vmdq
 * VMDq pool index to associate address with (unused_).
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr,
		    uint32_t index, __rte_unused uint32_t vmdq)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	unsigned int i;
	uint16_t func_id;
	int err;

	if (!rte_is_valid_assigned_ether_addr(mac_addr)) {
		PMD_DRV_LOG(ERR, "Add invalid MAC address");
		return -EINVAL;
	}

	if (index >= HINIC3_MAX_UC_MAC_ADDRS) {
		PMD_DRV_LOG(ERR, "Add MAC index(%u) is out of range", index);
		return -EINVAL;
	}

	/* Make sure this address doesn't already be configured. */
	for (i = 0; i < HINIC3_MAX_UC_MAC_ADDRS; i++) {
		if (rte_is_same_ether_addr(mac_addr,
					   &dev->data->mac_addrs[i])) {
			PMD_DRV_LOG(ERR, "MAC address is already configured");
			return -EADDRINUSE;
		}
	}

	func_id = hinic3_global_func_id(nic_dev->hwdev);
	err = hinic3_set_mac(nic_dev->hwdev, mac_addr->addr_bytes, 0, func_id);
	if (err)
		return err;

	return 0;
}

/**
 * Set multicast MAC address.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] mc_addr_set
 * Pointer to multicast MAC address.
 * @param[in] nb_mc_addr
 * The number of multicast MAC address to set.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_set_mc_addr_list(struct rte_eth_dev *dev,
			struct rte_ether_addr *mc_addr_set, uint32_t nb_mc_addr)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	char mac_addr[RTE_ETHER_ADDR_FMT_SIZE];
	uint16_t func_id;
	int err;
	uint32_t i;

	func_id = hinic3_global_func_id(nic_dev->hwdev);

	/* Delete old multi_cast addrs firstly. */
	hinic3_delete_mc_addr_list(nic_dev);

	if (nb_mc_addr > HINIC3_MAX_MC_MAC_ADDRS)
		return -EINVAL;

	for (i = 0; i < nb_mc_addr; i++) {
		if (!rte_is_multicast_ether_addr(&mc_addr_set[i])) {
			rte_ether_format_addr(mac_addr, RTE_ETHER_ADDR_FMT_SIZE, &mc_addr_set[i]);
			PMD_DRV_LOG(ERR, "Set mc MAC addr failed, addr(%s) invalid", mac_addr);
			return -EINVAL;
		}
	}

	for (i = 0; i < nb_mc_addr; i++) {
		err = hinic3_set_mac(nic_dev->hwdev, mc_addr_set[i].addr_bytes, 0, func_id);
		if (err) {
			hinic3_delete_mc_addr_list(nic_dev);
			return err;
		}

		rte_ether_addr_copy(&mc_addr_set[i], &nic_dev->mc_list[i]);
	}

	return 0;
}

static const struct eth_dev_ops hinic3_pmd_ops = {
	.dev_configure                 = hinic3_dev_configure,
	.dev_infos_get                 = hinic3_dev_infos_get,
	.fw_version_get                = hinic3_fw_version_get,
	.dev_set_link_up               = hinic3_dev_set_link_up,
	.dev_set_link_down             = hinic3_dev_set_link_down,
	.link_update                   = hinic3_link_update,
	.rx_queue_setup                = hinic3_rx_queue_setup,
	.tx_queue_setup                = hinic3_tx_queue_setup,
	.rx_queue_release              = hinic3_rx_queue_release,
	.tx_queue_release              = hinic3_tx_queue_release,
	.rx_queue_start                = hinic3_dev_rx_queue_start,
	.rx_queue_stop                 = hinic3_dev_rx_queue_stop,
	.tx_queue_start                = hinic3_dev_tx_queue_start,
	.tx_queue_stop                 = hinic3_dev_tx_queue_stop,
	.rx_queue_intr_enable          = hinic3_dev_rx_queue_intr_enable,
	.rx_queue_intr_disable         = hinic3_dev_rx_queue_intr_disable,
	.dev_start                     = hinic3_dev_start,
	.dev_stop                      = hinic3_dev_stop,
	.dev_close                     = hinic3_dev_close,
	.mtu_set                       = hinic3_dev_set_mtu,
	.vlan_filter_set               = hinic3_vlan_filter_set,
	.vlan_offload_set              = hinic3_vlan_offload_set,
	.allmulticast_enable           = hinic3_dev_allmulticast_enable,
	.allmulticast_disable          = hinic3_dev_allmulticast_disable,
	.stats_get                     = hinic3_dev_stats_get,
	.stats_reset                   = hinic3_dev_stats_reset,
	.xstats_get                    = hinic3_dev_xstats_get,
	.xstats_reset                  = hinic3_dev_xstats_reset,
	.xstats_get_names              = hinic3_dev_xstats_get_names,
	.rxq_info_get                  = hinic3_rxq_info_get,
	.txq_info_get                  = hinic3_txq_info_get,
	.mac_addr_set                  = hinic3_set_mac_addr,
	.mac_addr_remove               = hinic3_mac_addr_remove,
	.mac_addr_add                  = hinic3_mac_addr_add,
	.set_mc_addr_list              = hinic3_set_mc_addr_list,
};

static const struct eth_dev_ops hinic3_pmd_vf_ops = {
	.dev_configure                 = hinic3_dev_configure,
	.dev_infos_get                 = hinic3_dev_infos_get,
	.fw_version_get                = hinic3_fw_version_get,
	.rx_queue_setup                = hinic3_rx_queue_setup,
	.tx_queue_setup                = hinic3_tx_queue_setup,
	.rx_queue_intr_enable          = hinic3_dev_rx_queue_intr_enable,
	.rx_queue_intr_disable         = hinic3_dev_rx_queue_intr_disable,

	.rx_queue_start                = hinic3_dev_rx_queue_start,
	.rx_queue_stop                 = hinic3_dev_rx_queue_stop,
	.tx_queue_start                = hinic3_dev_tx_queue_start,
	.tx_queue_stop                 = hinic3_dev_tx_queue_stop,

	.dev_start                     = hinic3_dev_start,
	.link_update                   = hinic3_link_update,
	.rx_queue_release              = hinic3_rx_queue_release,
	.tx_queue_release              = hinic3_tx_queue_release,
	.dev_stop                      = hinic3_dev_stop,
	.dev_close                     = hinic3_dev_close,
	.mtu_set                       = hinic3_dev_set_mtu,
	.vlan_filter_set               = hinic3_vlan_filter_set,
	.vlan_offload_set              = hinic3_vlan_offload_set,
	.allmulticast_enable           = hinic3_dev_allmulticast_enable,
	.allmulticast_disable          = hinic3_dev_allmulticast_disable,
	.stats_get                     = hinic3_dev_stats_get,
	.stats_reset                   = hinic3_dev_stats_reset,
	.xstats_get                    = hinic3_dev_xstats_get,
	.xstats_reset                  = hinic3_dev_xstats_reset,
	.xstats_get_names              = hinic3_dev_xstats_get_names,
	.rxq_info_get                  = hinic3_rxq_info_get,
	.txq_info_get                  = hinic3_txq_info_get,
	.mac_addr_set                  = hinic3_set_mac_addr,
	.mac_addr_remove               = hinic3_mac_addr_remove,
	.mac_addr_add                  = hinic3_mac_addr_add,
	.set_mc_addr_list              = hinic3_set_mc_addr_list,
};

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
		PMD_DRV_LOG(ERR, "Allocate  MAC addresses failed, dev_name: %s",
			    eth_dev->data->name);
		err = -ENOMEM;
		goto alloc_eth_addr_fail;
	}

	nic_dev->mc_list = rte_zmalloc("hinic3_mc",
		HINIC3_MAX_MC_MAC_ADDRS * sizeof(struct rte_ether_addr), 0);
	if (!nic_dev->mc_list) {
		PMD_DRV_LOG(ERR, "Allocate  MAC addresses failed, dev_name: %s",
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

	if (HINIC3_FUNC_TYPE(nic_dev->hwdev) == TYPE_VF)
		eth_dev->dev_ops = &hinic3_pmd_vf_ops;
	else
		eth_dev->dev_ops = &hinic3_pmd_ops;

	err = hinic3_init_nic_hwdev(nic_dev->hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Init nic hwdev failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_nic_hwdev_fail;
	}

	err = hinic3_get_feature_from_hw(nic_dev->hwdev, &nic_dev->feature_cap, 1);
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

	hinic3_set_bit(HINIC3_DEV_INTR_EN, &nic_dev->dev_status);

	hinic3_set_bit(HINIC3_DEV_INIT, &nic_dev->dev_status);
	PMD_DRV_LOG(DEBUG, "Initialize %s in primary succeed",
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

	PMD_DRV_LOG(DEBUG, "Network Interface pmd driver version: %s",
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
