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
#include "base/hinic3_mbox.h"
#include "base/hinic3_hwif.h"
#include "base/hinic3_hw_cfg.h"
#include "base/hinic3_hw_comm.h"
#include "base/hinic3_nic_cfg.h"
#include "base/hinic3_nic_event.h"
#include "htn_adapt/hinic3_htn_cmdq.h"
#include "stn_adapt/hinic3_stn_cmdq.h"
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

#define HINIC3_RX_WAIT_CYCLE_THRESH 150

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

#define HINIC3_DEFAULT_COS_MASK		0x7
#define HINIC3_DEFAULT_COS_MASK_BITMAP	0xff

struct hinic3_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	uint32_t offset;
};

#define HINIC3_CIR_DROP_STAT(_stat_item)                                   \
	{                                                                  \
		.name = #_stat_item,                                       \
		.offset = offsetof(struct hinic3_cir_drop, _stat_item),    \
	}

#define HINIC3_FUNC_STAT(_stat_item)                                       \
	{                                                                  \
		.name = #_stat_item,                                       \
		.offset = offsetof(struct hinic3_vport_stats, _stat_item), \
	}


static struct hinic3_xstats_name_off hinic3_cir_drop_stats_strings[] = {
	HINIC3_CIR_DROP_STAT(rx_discard_phy),
};

#define HINIC3_CIR_DROP_XSTATS_NUM  RTE_DIM(hinic3_cir_drop_stats_strings)

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
	HINIC3_FUNC_STAT(rx_mtu_err_vport),
	HINIC3_FUNC_STAT(rx_out_of_buffer),
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
	uint32_t cir_drop_num = HINIC3_IS_SP620_NIC(nic_dev) ?
				HINIC3_CIR_DROP_XSTATS_NUM : 0;

	if (HINIC3_IS_VF(nic_dev->hwdev)) {
		return (HINIC3_VPORT_XSTATS_NUM + cir_drop_num +
			HINIC3_RXQ_XSTATS_NUM * nic_dev->num_rqs +
			HINIC3_TXQ_XSTATS_NUM * nic_dev->num_sqs);
	} else {
		return (HINIC3_VPORT_XSTATS_NUM + HINIC3_PHYPORT_XSTATS_NUM +
			cir_drop_num +
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

static void
hinic3_get_cos_mask(struct hinic3_hwdev *hwdev, uint8_t *cos_mask)
{
	uint8_t cos_mask_mode;

	cos_mask_mode = hwdev->cfg_mgmt->svc_cap.cos_mask_mode;
	if (!cos_mask_mode) {
		*cos_mask = HINIC3_DEFAULT_COS_MASK; /* default: 8 COS*/
		PMD_DRV_LOG(INFO,
			"cos mask not provided by firmware, use default 0x%x",
			HINIC3_DEFAULT_COS_MASK);
		return;
	}

	*cos_mask = cos_mask_mode;
}

static void
hinic3_get_cos_mask_bitmap(struct hinic3_nic_dev *nic_dev)
{
	int i;
	uint8_t default_cos = 0;
	uint8_t cos_mask_bitmap = nic_dev->hwdev->cfg_mgmt->svc_cap.cos_mask_bitmap == 0
			? HINIC3_DEFAULT_COS_MASK_BITMAP
			: nic_dev->hwdev->cfg_mgmt->svc_cap.cos_mask_bitmap;

	PMD_DRV_LOG(INFO, "cos_mask_bitmap: 0x%x", cos_mask_bitmap);
	for (i = HINIC3_COS_NUM_MAX - 1; i >= 0; i--) {
		if (cos_mask_bitmap & RTE_BIT32(i)) {
			default_cos = i;
			break;
		}
	}

	for (i = 0; i < HINIC3_COS_NUM_MAX; i++)
		nic_dev->cos_map[i] = ((RTE_BIT32(i) & cos_mask_bitmap) == 0) ? default_cos : i;
}

static int
hinic3_init_default_cos(struct hinic3_nic_dev *nic_dev)
{
	uint8_t cos_mask = 0;
	uint8_t cos_id = 0;
	int err;

	hinic3_get_cos_mask(nic_dev->hwdev, &cos_mask);

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

	nic_dev->cos_mask = cos_mask;
	nic_dev->default_cos = cos_id;
	PMD_DRV_LOG(DEBUG, "Default cos %d, cos mask %d", nic_dev->default_cos, nic_dev->cos_mask);
	return 0;
}

static enum nic_type
hinic3_get_nic_type(struct rte_pci_device *pci_dev)
{
	switch (pci_dev->id.device_id) {
	case HINIC3_DEV_ID_SP620:
	case HINIC3_DEV_ID_VF_SP620:
	case HINIC3_DEV_ID_SP920: /* SP920 is the DPU card of SP620. */
		return NIC_SP620;
	case HINIC3_DEV_ID_SP230:
	case HINIC3_DEV_ID_VF_SP230:
		return NIC_SP230;
	case HINIC3_DEV_ID_SP560:
	case HINIC3_DEV_ID_VF_SP560:
	case HINIC3_DEV_ID_HYPER_VF_SP560:
		return NIC_SP560;
	default:
		return NIC_UNKNOWN;
	}
}

/**
 * Get cmdq ops for the given NIC device.
 *
 * @param[in] nic_dev
 * Pointer to NIC device structure.
 *
 * @return
 * Pointer to cmdq ops structure.
 */
const struct hinic3_nic_cmdq_ops *
hinic3_cmdq_get_ops(struct hinic3_nic_dev *nic_dev)
{
	static const struct hinic3_nic_cmdq_ops *cmdq_ops[] = {
		[NIC_SP620]   = &hinic3_stn_cmdq_ops,
		[NIC_SP230]   = &hinic3_htn_cmdq_ops,
		[NIC_SP560]   = &hinic3_stn_cmdq_ops,
		[NIC_UNKNOWN] = &hinic3_stn_cmdq_ops,
	};

	return cmdq_ops[nic_dev->nic_type];
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

	hinic3_get_cos_mask_bitmap(nic_dev);
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

static void
hinic3_dev_tnl_tso_support(struct rte_eth_dev_info *info, struct hinic3_nic_dev *nic_dev)
{
	if (HINIC3_SUPPORT_GENEVE_OFFLOAD(nic_dev))
		info->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO;
	if (HINIC3_SUPPORT_IPXIP_OFFLOAD(nic_dev))
		info->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_IPIP_TNL_TSO;
	if (HINIC3_SUPPORT_VXLAN_OFFLOAD(nic_dev))
		info->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO;
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
		RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM |
		RTE_ETH_TX_OFFLOAD_TCP_TSO | RTE_ETH_TX_OFFLOAD_MULTI_SEGS;

	hinic3_dev_tnl_tso_support(info, nic_dev);

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
 * Get the VF link status reported to the application.
 *
 * @param[in] nic_dev
 * Pointer to the hinic3 NIC device.
 *
 * @return
 * Link status of the VF, RTE_ETH_LINK_UP or RTE_ETH_LINK_DOWN.
 */
static uint8_t
hinic3_vf_link_status(struct hinic3_nic_dev *nic_dev)
{
	struct hinic3_hwdev *hwdev = nic_dev->hwdev;

	if (nic_dev->admin_down)
		return RTE_ETH_LINK_DOWN;

	return hwdev->link_status & hwdev->vf_valid_status;
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
	struct rte_eth_link link = { 0 };
	int err;

	nic_dev->admin_down = 0;

	/*
	 * Vport enable will set function valid in mpu.
	 * So dev start status need to be checked before vport enable.
	 */
	if (hinic3_get_bit(HINIC3_DEV_START, &nic_dev->dev_status)) {
		err = hinic3_set_vport_enable(nic_dev->hwdev, true);
		if (err) {
			PMD_DRV_LOG(ERR, "Enable vport failed, dev_name: %s",
				    nic_dev->dev_name);
			nic_dev->admin_down = 1;
			return err;
		}
	}

	/* Link status follow phy port status, mpu will open pma. */
	err = hinic3_set_port_enable(nic_dev->hwdev, true);
	if (err) {
		PMD_DRV_LOG(ERR,
			    "Set MAC link up failed, dev_name: %s, port_id: %d",
			    nic_dev->dev_name, dev->data->port_id);
		nic_dev->admin_down = 1;
		return err;
	}

	if (HINIC3_IS_VF(nic_dev->hwdev)) {
		link = dev->data->dev_link;
		link.link_status = hinic3_vf_link_status(nic_dev);

		(void)rte_eth_linkstatus_set(dev, &link);
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
	struct rte_eth_link link = { 0 };
	int err;

	nic_dev->admin_down = 1;

	err = hinic3_set_vport_enable(nic_dev->hwdev, false);
	if (err) {
		nic_dev->admin_down = 0;
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

	if (HINIC3_IS_VF(nic_dev->hwdev)) {
		link = dev->data->dev_link;
		link.link_status = hinic3_vf_link_status(nic_dev);

		(void)rte_eth_linkstatus_set(dev, &link);
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
	if (HINIC3_IS_VF(nic_dev->hwdev)) {
		nic_dev->hwdev->link_status = link.link_status;
		link.link_status = hinic3_vf_link_status(nic_dev);
	}

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
		rte_atomic_store_explicit(&rxq->rss_en, 0,
					  rte_memory_order_relaxed);

		/*
		 * Hardware does not clear the CI write-back memory on reset, so
		 * the stale hw_ci must be cleared as well, otherwise the driver
		 * may treat queued mbufs as completed after restart.
		 */
		if (rxq->rq_ci != NULL)
			memset(rxq->rq_ci, 0, sizeof(*rxq->rq_ci));
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
	const struct rte_memzone *ci_mz = NULL;
	const struct rte_memzone *pi_mz = NULL;
	uint16_t rq_depth, rx_free_thresh;
	uint32_t queue_buf_size;
	void *db_addr = NULL;
	int wqe_count;
	uint32_t buf_size;
	uint32_t rx_buf_size;
	int err;

	nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	/* Queue depth must be equal to queue 0 */
	if (qid != 0 && nb_desc != nic_dev->rxqs[0]->q_depth) {
		PMD_DRV_LOG(WARNING, "rxq%u depth:%u is not equal to queue0 depth:%u.",
			    qid, nb_desc, nic_dev->rxqs[0]->q_depth);
		nb_desc = nic_dev->rxqs[0]->q_depth;
	}

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
	rx_buf_size = rte_pktmbuf_data_room_size(rxq->mb_pool) - RTE_PKTMBUF_HEADROOM;
	err = hinic3_convert_rx_buf_size(rx_buf_size, &buf_size);
	if (err) {
		PMD_DRV_LOG(ERR, "Adjust buf size failed, dev_name: %s",
			    dev->data->name);
		goto adjust_bufsize_fail;
	}

	/* If NIC support compact CQE, use compact wqe as default. */
	if (nic_dev->config.rx_cqe_compact_en) {
		rxq->wqe_type = HINIC3_COMPACT_RQ_WQE;
	} else {
		if (buf_size >= HINIC3_RX_BUF_SIZE_4K &&
			buf_size < HINIC3_RX_BUF_SIZE_16K)
			rxq->wqe_type = HINIC3_EXTEND_RQ_WQE;
		else
			rxq->wqe_type = HINIC3_NORMAL_RQ_WQE;
	}

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

	if (nic_dev->config.rx_cqe_compact_en) {
		ci_mz = hinic3_dma_zone_reserve(dev, "hinic3_ci_mz", qid,
						sizeof(*rxq->rq_ci),
						RTE_CACHE_LINE_SIZE, (int)socket_id);

		if (!ci_mz) {
			PMD_DRV_LOG(ERR, "Allocate ci mem zone failed, dev_name: %s",
				    dev->data->name);
			err = -ENOMEM;
			goto alloc_cqe_ci_mz_fail;
		}

		memset(ci_mz->addr, 0, sizeof(*rxq->rq_ci));
		rxq->ci_mz = ci_mz;
		rxq->rq_ci = (struct hinic3_rq_ci_wb *)ci_mz->addr;
		rxq->rq_ci_paddr = ci_mz->iova;
	} else {
		cqe_mz = hinic3_dma_zone_reserve(dev, "hinic3_cqe_mz", qid,
						 rq_depth * sizeof(*rxq->rx_cqe),
						 RTE_CACHE_LINE_SIZE, socket_id);
		if (!cqe_mz) {
			PMD_DRV_LOG(ERR, "Allocate cqe mem zone failed, dev_name: %s",
			dev->data->name);
			err = -ENOMEM;
			goto alloc_cqe_ci_mz_fail;
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
			hinic3_memzone_free(cqe_mz);
			goto alloc_cqe_ci_mz_fail;
		}
	}
	dev->data->rx_queues[qid] = rxq;

	return 0;

alloc_cqe_ci_mz_fail:
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

	if (!ODD_NUMBER_QUEUE_ID(qid) &&
		hinic3_cmd_vf_lag(nic_dev->hwdev, hinic3_global_func_id(nic_dev->hwdev),
				  HINIC3_CMD_OPCODE_GET))
		txq->cos = SELECT_OTHER_COS_ID(nic_dev->default_cos);
	else
		txq->cos = nic_dev->default_cos;

	if (HINIC3_IS_SP230_NIC(nic_dev))
		txq->cos = nic_dev->cos_map[txq->cos &
			(HINIC3_COS_NUM_MAX - 1) & nic_dev->cos_mask];

	txq->tx_deferred_start = tx_conf->tx_deferred_start;
	txq->tx_wqe_compact_task = HINIC3_SUPPORT_TX_WQE_COMPACT_TASK(nic_dev);

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

	if (nic_dev->config.rx_cqe_compact_en)
		hinic3_memzone_free(rxq->ci_mz);
	else
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
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct hinic3_rxq *rxq = dev->data->rx_queues[rq_id];
	int rc;

	rxq = dev->data->rx_queues[rq_id];

	rc = hinic3_start_rq(dev, rxq);
	if (rc) {
		PMD_DRV_LOG(ERR,
			    "Start rx queue failed, eth_dev:%s, queue_idx:%d",
			    dev->data->name, rq_id);
		return rc;
	}

	if ((hinic3_get_driver_feature(nic_dev) & NIC_F_HTN_FDIR) == 0) {
		rc = hinic3_enable_rxq_fdir_filter(dev, rq_id, true);
		if (rc) {
			PMD_DRV_LOG(ERR, "Failed to enable rq : %d fdir filter.", rq_id);
			return rc;
		}
	}

	dev->data->rx_queue_state[rq_id] = RTE_ETH_QUEUE_STATE_STARTED;

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
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct hinic3_rxq *rxq = dev->data->rx_queues[rq_id];
	int rc;

	rc = hinic3_stop_rq(dev, rxq);
	if (rc) {
		PMD_DRV_LOG(ERR,
			    "Stop rx queue failed, eth_dev:%s, queue_idx:%d",
			    dev->data->name, rq_id);
		return rc;
	}

	if ((hinic3_get_driver_feature(nic_dev) & NIC_F_HTN_FDIR) == 0) {
		rc = hinic3_enable_rxq_fdir_filter(dev, rq_id, false);
		if (rc) {
			PMD_DRV_LOG(ERR, "Failed to disable rq : %d fdir filter.", rq_id);
			return rc;
		}
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
	struct rte_pci_device *pci_dev = RTE_CLASS_TO_BUS_DEVICE(dev, *pci_dev);
	struct rte_intr_handle *intr_handle = PCI_DEV_TO_INTR_HANDLE(pci_dev);
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	uint16_t msix_intr;

	if (HINIC3_IS_SP230_NIC(nic_dev) && !dev->data->dev_conf.intr_conf.rxq)
		return 0;

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
	struct rte_pci_device *pci_dev = RTE_CLASS_TO_BUS_DEVICE(dev, *pci_dev);
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
		nic_dev->rss_type.val = 0;
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
	struct rte_pci_device *pci_dev = RTE_CLASS_TO_BUS_DEVICE(dev, *pci_dev);

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
	struct rte_pci_device *pci_dev = RTE_CLASS_TO_BUS_DEVICE(dev, *pci_dev);

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
	if (err)
		return -EFAULT;

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
 * Check that the Rx queue vectors fit in the device MSI-X vector table.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] nb_rx_queues
 * Number of Rx queues to be given a vector.
 *
 * @return
 * 0 if the configuration fits, -EINVAL otherwise.
 */
static int
hinic3_check_rxq_intr_vec(struct rte_eth_dev *dev, uint32_t nb_rx_queues)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	uint16_t num_irqs = HINIC3_HWIF_NUM_IRQS(nic_dev->hwdev->hwif);
	uint32_t last_vec;

	if (nb_rx_queues == 0)
		return 0;

	last_vec = (uint32_t)HINIC3_RX_VEC_START + nb_rx_queues - 1;
	if (last_vec >= (uint32_t)num_irqs) {
		PMD_DRV_LOG(ERR,
			    "Not enough MSI-X vectors for %u Rx queues, need %u vectors, have %u",
			    nb_rx_queues, last_vec + 1, num_irqs);
		return -EINVAL;
	}

	return 0;
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
 * - -EINVAL if the Rx queues do not fit in the MSI-X vector table.
 *
 * On SP230 with per-queue MSI-X vectors, when the application does not
 * request Rx interrupts, the queue vector is named in each RQ context but
 * never enabled, and no interrupt resources are allocated.
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

	if (!dev->data->dev_conf.intr_conf.rxq) {
		/*
		 * SP230 raises the Rx MSI-X vector even without Rx interrupts.
		 * Name a valid vector in the RQ context; the vector is never
		 * enabled, so no interrupt is reported.
		 */
		if (!HINIC3_IS_SP230_NIC(nic_dev) ||
		    !rte_intr_cap_multiple(intr_handle))
			return 0;

		err = hinic3_check_rxq_intr_vec(dev, dev->data->nb_rx_queues);
		if (err)
			return err;

		for (i = 0; i < dev->data->nb_rx_queues; i++) {
			rxq = dev->data->rx_queues[i];
			rxq->dp_intr_en = 1;
			rxq->msix_entry_idx = (uint16_t)(i + HINIC3_RX_VEC_START);
		}

		return 0;
	}

	if (!rte_intr_cap_multiple(intr_handle)) {
		PMD_DRV_LOG(ERR, "Rx queue interrupts require MSI-X interrupts (vfio-pci driver)");
		return -ENOTSUP;
	}

	nb_rx_queues = dev->data->nb_rx_queues;
	err = hinic3_check_rxq_intr_vec(dev, nb_rx_queues);
	if (err)
		return err;

	err = rte_intr_efd_enable(intr_handle, nb_rx_queues);
	if (err) {
		PMD_DRV_LOG(ERR,
			"Failed to enable event fds for Rx queue interrupts");
		return err;
	}

	intr_handle->intr_vec =
		rte_zmalloc("hinic_intr_vec", nb_rx_queues * sizeof(int), 0);
	if (intr_handle->intr_vec == NULL) {
		rte_intr_efd_disable(intr_handle);
		PMD_DRV_LOG(ERR, "Failed to allocate intr_vec");
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
			PMD_DRV_LOG(ERR,
				    "Set msix attr failed, msix_index %d",
				    rxq->msix_entry_idx);
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

	if (!nic_dev->admin_down) {
		err = hinic3_set_vport_enable(nic_dev->hwdev, true);
		if (err) {
			PMD_DRV_LOG(ERR, "Enable vport failed, dev_name: %s",
				    eth_dev->data->name);
			goto en_vport_fail;
		}
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
	struct rte_pci_device *pci_dev = RTE_CLASS_TO_BUS_DEVICE(eth_dev, *pci_dev);
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

	rte_free(nic_dev->hwdev);
	nic_dev->hwdev = NULL;
	rte_free(nic_dev->ptype_tbl);
	nic_dev->ptype_tbl = NULL;
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

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

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
	HINIC3_MAX_RX_PKT_LEN(dev->data->dev_conf.rxmode) = HINIC3_MTU_TO_PKTLEN(mtu);
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
 * Enable promiscuous mode.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	uint32_t rx_mode;
	int err;

	if (!(nic_dev->feature_cap & NIC_F_PROMISC)) {
		PMD_DRV_LOG(ERR, "nic_dev: %s, port_id: %d, do not support vf promisc: %" PRIu64 "",
			    nic_dev->dev_name, dev->data->port_id, nic_dev->feature_cap);
		return -ENOTSUP;
	}

	rx_mode = nic_dev->rx_mode | HINIC3_RX_MODE_PROMISC;

	err = hinic3_set_rx_mode(nic_dev->hwdev, rx_mode);
	if (err) {
		PMD_DRV_LOG(ERR, "Enable promiscuous failed");
		return err;
	}

	nic_dev->rx_mode = rx_mode;

	PMD_DRV_LOG(DEBUG,
		    "Enable promiscuous, nic_dev: %s, port_id: %d, promisc: %d",
		    nic_dev->dev_name, dev->data->port_id,
		    dev->data->promiscuous);
	return 0;
}

/**
 * Disable promiscuous mode.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	uint32_t rx_mode;
	int err;

	rx_mode = nic_dev->rx_mode & (~HINIC3_RX_MODE_PROMISC);

	err = hinic3_set_rx_mode(nic_dev->hwdev, rx_mode);
	if (err) {
		PMD_DRV_LOG(ERR, "Disable promiscuous failed");
		return err;
	}

	nic_dev->rx_mode = rx_mode;

	PMD_DRV_LOG(DEBUG,
		"Disable promiscuous, nic_dev: %s, port_id: %d, promisc: %d",
		nic_dev->dev_name, dev->data->port_id, dev->data->promiscuous);
	return 0;
}

/**
 * Get flow control configuration, including auto-negotiation and RX/TX pause
 * settings.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 *
 * @param[out] fc_conf
 * The flow control configuration to be filled.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_dev_flow_ctrl_get(struct rte_eth_dev *dev,
			 struct rte_eth_fc_conf *fc_conf)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct nic_pause_config nic_pause;
	int err;

	memset(&nic_pause, 0, sizeof(nic_pause));
	err = hinic3_get_pause_info(nic_dev->hwdev, &nic_pause);
	if (err)
		return err;
	if (nic_dev->pause_set || !nic_pause.auto_neg) {
		nic_pause.rx_pause = nic_dev->nic_pause.rx_pause;
		nic_pause.tx_pause = nic_dev->nic_pause.tx_pause;
	}

	fc_conf->autoneg = nic_pause.auto_neg;

	if (nic_pause.tx_pause && nic_pause.rx_pause)
		fc_conf->mode = RTE_ETH_FC_FULL;
	else if (nic_pause.tx_pause)
		fc_conf->mode = RTE_ETH_FC_TX_PAUSE;
	else if (nic_pause.rx_pause)
		fc_conf->mode = RTE_ETH_FC_RX_PAUSE;
	else
		fc_conf->mode = RTE_ETH_FC_NONE;

	return 0;
}

static int
hinic3_dev_flow_ctrl_set(struct rte_eth_dev *dev,
			 struct rte_eth_fc_conf *fc_conf)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct nic_pause_config nic_pause;
	int err;

	memset(&nic_pause, 0, sizeof(nic_pause));
	if ((fc_conf->mode & RTE_ETH_FC_FULL) == RTE_ETH_FC_FULL ||
	    (fc_conf->mode & RTE_ETH_FC_TX_PAUSE))
		nic_pause.tx_pause = true;

	if ((fc_conf->mode & RTE_ETH_FC_FULL) == RTE_ETH_FC_FULL ||
	    (fc_conf->mode & RTE_ETH_FC_RX_PAUSE))
		nic_pause.rx_pause = true;

	err = hinic3_set_pause_info(nic_dev->hwdev, nic_pause);
	if (err)
		return err;
	nic_dev->pause_set = true;
	nic_dev->nic_pause.rx_pause = nic_pause.rx_pause;
	nic_dev->nic_pause.tx_pause = nic_pause.tx_pause;

	PMD_DRV_LOG(INFO,
		    "Just support set tx or rx pause info, tx: %s, rx: %s",
		    nic_pause.tx_pause ? "on" : "off",
		    nic_pause.rx_pause ? "on" : "off");
	return 0;
}

/**
 * Update the RSS hash key and RSS hash type.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] rss_conf
 * RSS configuration data.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_rss_hash_update(struct rte_eth_dev *dev,
		       struct rte_eth_rss_conf *rss_conf)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct hinic3_rss_type rss_type = {0};
	uint64_t rss_hf = rss_conf->rss_hf;
	int err = 0;

	if (nic_dev->rss_state == HINIC3_RSS_DISABLE) {
		if (rss_hf != 0)
			return -EINVAL;

		PMD_DRV_LOG(INFO, "RSS is not enabled");
		return 0;
	}

	if (rss_conf->rss_key_len > HINIC3_RSS_KEY_SIZE) {
		PMD_DRV_LOG(ERR, "Invalid RSS key, rss_key_len: %d",
			    rss_conf->rss_key_len);
		return -EINVAL;
	}

	if (rss_conf->rss_key) {
		err = hinic3_rss_set_hash_key(nic_dev->hwdev, rss_conf->rss_key,
					      HINIC3_RSS_KEY_SIZE);
		if (err) {
			PMD_DRV_LOG(ERR, "Set RSS hash key failed");
			return err;
		}
		memcpy((void *)nic_dev->rss_key, (void *)rss_conf->rss_key,
		       (size_t)rss_conf->rss_key_len);
	}

	rss_type.ipv4 = (rss_hf & (RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_FRAG_IPV4 |
				   RTE_ETH_RSS_NONFRAG_IPV4_OTHER)) ? 1 : 0;
	rss_type.tcp_ipv4 = (rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_TCP) ? 1 : 0;
	rss_type.ipv6 = (rss_hf & (RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_FRAG_IPV6 |
				   RTE_ETH_RSS_NONFRAG_IPV6_OTHER)) ? 1 : 0;
	rss_type.tcp_ipv6 = (rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_TCP) ? 1 : 0;
	rss_type.udp_ipv4 = (rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_UDP) ? 1 : 0;
	rss_type.udp_ipv6 = (rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_UDP) ? 1 : 0;
	rss_type.ipv6_ext = (rss_hf & RTE_ETH_RSS_IPV6_EX) ? 1 : 0;
	rss_type.tcp_ipv6_ext = (rss_hf & RTE_ETH_RSS_IPV6_TCP_EX) ? 1 : 0;

	err = hinic3_set_rss_type(nic_dev->hwdev, rss_type);
	if (err) {
		PMD_DRV_LOG(ERR, "Set RSS type failed");
		return err;
	}

	nic_dev->rss_type = rss_type;
	hinic3_rxq_rss_en_update(dev);
	return 0;
}

/**
 * Get the RSS hash configuration.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[out] rss_conf
 * RSS configuration data.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_rss_conf_get(struct rte_eth_dev *dev, struct rte_eth_rss_conf *rss_conf)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct hinic3_rss_type rss_type = {0};
	int err;

	if (!rss_conf)
		return -EINVAL;

	if (nic_dev->rss_state == HINIC3_RSS_DISABLE) {
		rss_conf->rss_hf = 0;
		PMD_DRV_LOG(INFO, "RSS is not enabled");
		return 0;
	}

	if (rss_conf->rss_key && rss_conf->rss_key_len >= HINIC3_RSS_KEY_SIZE) {
		/*
		 * Get RSS key from driver to reduce the frequency of the MPU
		 * accessing the RSS memory.
		 */
		rss_conf->rss_key_len = sizeof(nic_dev->rss_key);
		memcpy((void *)rss_conf->rss_key, (void *)nic_dev->rss_key,
		       (size_t)rss_conf->rss_key_len);
	}

	err = hinic3_get_rss_type(nic_dev->hwdev, &rss_type);
	if (err)
		return err;

	rss_conf->rss_hf = 0;
	rss_conf->rss_hf |=
		rss_type.ipv4 ? (RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_FRAG_IPV4 |
				 RTE_ETH_RSS_NONFRAG_IPV4_OTHER) : 0;
	rss_conf->rss_hf |= rss_type.tcp_ipv4 ? RTE_ETH_RSS_NONFRAG_IPV4_TCP : 0;
	rss_conf->rss_hf |=
		rss_type.ipv6 ? (RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_FRAG_IPV6 |
				 RTE_ETH_RSS_NONFRAG_IPV6_OTHER) : 0;
	rss_conf->rss_hf |= rss_type.tcp_ipv6 ? RTE_ETH_RSS_NONFRAG_IPV6_TCP : 0;
	rss_conf->rss_hf |= rss_type.udp_ipv4 ? RTE_ETH_RSS_NONFRAG_IPV4_UDP : 0;
	rss_conf->rss_hf |= rss_type.udp_ipv6 ? RTE_ETH_RSS_NONFRAG_IPV6_UDP : 0;
	rss_conf->rss_hf |= rss_type.ipv6_ext ? RTE_ETH_RSS_IPV6_EX : 0;
	rss_conf->rss_hf |= rss_type.tcp_ipv6_ext ? RTE_ETH_RSS_IPV6_TCP_EX : 0;

	return 0;
}

/**
 * Get the RETA indirection table.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[out] reta_conf
 * Pointer to RETA configuration structure array.
 * @param[in] reta_size
 * Size of the RETA table.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_rss_reta_query(struct rte_eth_dev *dev,
		      struct rte_eth_rss_reta_entry64 *reta_conf,
		      uint16_t reta_size)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	uint32_t indirtbl[HINIC3_RSS_INDIR_SIZE] = {0};
	uint16_t idx, shift;
	uint16_t i;
	int err;

	if (nic_dev->rss_state == HINIC3_RSS_DISABLE) {
		PMD_DRV_LOG(INFO, "RSS is not enabled");
		return 0;
	}

	if (reta_size != HINIC3_RSS_INDIR_SIZE) {
		PMD_DRV_LOG(ERR, "Invalid reta size, reta_size: %d", reta_size);
		return -EINVAL;
	}

	err = hinic3_rss_get_indir_tbl(nic_dev->hwdev, indirtbl);
	if (err) {
		PMD_DRV_LOG(ERR, "Get RSS retas table failed, error: %d", err);
		return err;
	}

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			reta_conf[idx].reta[shift] = (uint16_t)indirtbl[i];
	}

	return 0;
}

/**
 * Update the RETA indirection table.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] reta_conf
 * Pointer to RETA configuration structure array.
 * @param[in] reta_size
 * Size of the RETA table.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_rss_reta_update(struct rte_eth_dev *dev,
		       struct rte_eth_rss_reta_entry64 *reta_conf,
		       uint16_t reta_size)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	uint32_t indirtbl[HINIC3_RSS_INDIR_SIZE] = {0};
	uint16_t idx, shift;
	uint16_t i;
	int err;

	if (nic_dev->rss_state == HINIC3_RSS_DISABLE)
		return 0;

	if (reta_size != HINIC3_RSS_INDIR_SIZE) {
		PMD_DRV_LOG(ERR, "Invalid reta size, reta_size: %d", reta_size);
		return -EINVAL;
	}

	err = hinic3_rss_get_indir_tbl(nic_dev->hwdev, indirtbl);
	if (err)
		return err;

	/* Update RSS reta table. */
	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			indirtbl[i] = reta_conf[idx].reta[shift];
	}

	for (i = 0; i < reta_size; i++) {
		if (indirtbl[i] >= nic_dev->num_rqs) {
			PMD_DRV_LOG(ERR,
				    "Invalid reta entry, index: %d, num_rqs: %d",
				    indirtbl[i], nic_dev->num_rqs);
			return -EFAULT;
		}
	}

	err = hinic3_rss_set_indir_tbl(nic_dev->hwdev, indirtbl);
	if (err)
		PMD_DRV_LOG(ERR, "Set RSS reta table failed");

	return err;
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
hinic3_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats,
		     struct eth_queue_stats *qstats)
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

		if (qstats && i < dev->data->nb_rx_queues) {
			qstats[i].q_ipackets = rxq->rxq_stats.packets;
			qstats[i].q_ibytes = rxq->rxq_stats.bytes;
		}
		stats->ierrors += rxq->rxq_stats.errors;
		rx_discards_pmd += rxq->rxq_stats.dropped;
		dev->data->rx_mbuf_alloc_failed += rxq->rxq_stats.rx_nombuf;
	}

	/* Tx queue stats. */
	for (uint32_t i = 0; i < nic_dev->num_sqs; i++) {
		struct hinic3_txq *txq = nic_dev->txqs[i];
		stats->oerrors += (txq->txq_stats.tx_busy + txq->txq_stats.offload_errors);
		if (qstats && i < dev->data->nb_tx_queues) {
			qstats[i].q_opackets = txq->txq_stats.packets;
			qstats[i].q_obytes = txq->txq_stats.bytes;
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

static uint16_t
get_port_cir_drop(struct hinic3_nic_dev *nic_dev,
		  struct rte_eth_xstat *xstats, uint32_t base)
{
	struct hinic3_cir_drop port_stats;
	uint16_t i;
	int err;

	memset(&port_stats, 0, sizeof(port_stats));

	err = hinic3_get_cir_drop(nic_dev->hwdev, &port_stats);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to get CPB cir drops from fw.");

		for (i = 0; i < RTE_DIM(hinic3_cir_drop_stats_strings); i++) {
			xstats[i].value = 0;
			xstats[i].id = base + i;
		}

		return RTE_DIM(hinic3_cir_drop_stats_strings);
	}

	for (i = 0; i < RTE_DIM(hinic3_cir_drop_stats_strings); i++) {
		memcpy(&xstats[i].value,
		       (const char *)&port_stats + hinic3_cir_drop_stats_strings[i].offset,
		       sizeof(uint64_t));
		xstats[i].id = base + i;
	}

	return RTE_DIM(hinic3_cir_drop_stats_strings);
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

	/* Get stats from cir drop stats structure */
	if (HINIC3_IS_SP620_NIC(nic_dev))
		count += get_port_cir_drop(nic_dev, &xstats[count], count);

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

	/* Get cir drop stats name. */
	if (HINIC3_IS_SP620_NIC(nic_dev)) {
		for (i = 0; i < HINIC3_CIR_DROP_XSTATS_NUM; i++) {
			strlcpy(xstats_names[count].name,
				hinic3_cir_drop_stats_strings[i].name,
				sizeof(xstats_names[count].name));
			count++;
		}
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

static const uint32_t *
hinic3_dev_supported_ptypes_get(__rte_unused struct rte_eth_dev *dev, size_t *no_of_elements)
{
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L2_ETHER_FCOE,
		RTE_PTYPE_L2_ETHER_NSH,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_L4_NONFRAG,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_ICMP,
		RTE_PTYPE_L4_IGMP,
		RTE_PTYPE_L4_ESP,
		RTE_PTYPE_TUNNEL_IP,
		RTE_PTYPE_TUNNEL_GRE,
		RTE_PTYPE_TUNNEL_VXLAN,
		RTE_PTYPE_TUNNEL_VXLAN_GPE,
		RTE_PTYPE_TUNNEL_GENEVE,
		RTE_PTYPE_INNER_L2_ETHER,
		RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_INNER_L4_SCTP,
		RTE_PTYPE_INNER_L4_NONFRAG,
	};
	*no_of_elements = RTE_DIM(ptypes);
	return ptypes;
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

/**
 * Manage flow director filter operations.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] filter_type
 * Filter type.
 * @param[in] filter_op
 * Operation to perform.
 * @param[in] arg
 * Pointer to operation-specific structure.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_dev_filter_ctrl(struct rte_eth_dev *dev, const struct rte_flow_ops **arg)
{
	RTE_SET_USED(dev);
	*arg = &hinic3_flow_ops;
	return 0;
}

static const uint32_t HINIC3_FEC_CAPA_NUM_PER_SPEED = 1;

static const uint32_t hinic3_speed_capa[] = {
	RTE_ETH_SPEED_NUM_NONE, RTE_ETH_SPEED_NUM_10M,
	RTE_ETH_SPEED_NUM_100M, RTE_ETH_SPEED_NUM_1G,
	RTE_ETH_SPEED_NUM_10G, RTE_ETH_SPEED_NUM_25G,
	RTE_ETH_SPEED_NUM_40G, RTE_ETH_SPEED_NUM_50G,
	RTE_ETH_SPEED_NUM_100G, RTE_ETH_SPEED_NUM_200G,
	RTE_ETH_SPEED_NUM_400G, RTE_ETH_SPEED_NUM_800G
};

static bool
hinic3_fec_param_valid(uint32_t fec_param)
{
	if (fec_param == RTE_ETH_FEC_MODE_CAPA_MASK(LLRS) ||
	    fec_param == RTE_ETH_FEC_MODE_CAPA_MASK(RS) ||
	    fec_param == RTE_ETH_FEC_MODE_CAPA_MASK(BASER) ||
	    fec_param == RTE_ETH_FEC_MODE_CAPA_MASK(NOFEC)) {
		return true;
	}

	return false;
}

static int
hinic3_fec_set(struct rte_eth_dev *dev, uint32_t fec_capa)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	int err;

	if (!hinic3_fec_param_valid(fec_capa)) {
		if (fec_capa & RTE_ETH_FEC_MODE_CAPA_MASK(AUTO))
			PMD_DRV_LOG(ERR,
				    "Unsupported fec param 0x%x, AUTO is configured by hinicadm5.",
				    fec_capa);
		else
			PMD_DRV_LOG(ERR, "Unsupported fec param 0x%x.", fec_capa);
		return -EINVAL;
	}

	err = hinic3_set_fec_mode(nic_dev->hwdev, (uint8_t)fec_capa);
	if (err) {
		/*
		 * A forced FEC mode can only be configured after
		 * auto-negotiation is disabled with the hinicadm5 tool.
		 * While auto-negotiation is on, the firmware rejects the SET
		 * request (management status 0x10), and the PMD has no way to
		 * toggle it. The firmware also reports the negotiated mode on
		 * GET, so the AUTO state cannot be detected before the attempt.
		 */
		PMD_DRV_LOG(ERR,
			    "Failed to set fec param 0x%x, err %d (AUTO is managed by hinicadm5).",
			    fec_capa, err);
		return err;
	}

	return 0;
}

static int
hinic3_fec_get(struct rte_eth_dev *dev, uint32_t *fec_capa)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	uint8_t advertised_fec = 0;
	int err;

	err = hinic3_get_fec_mode(nic_dev->hwdev, &advertised_fec, NULL);
	if (err) {
		PMD_DRV_LOG(ERR, "Get fec param failed: %d.", err);
		return err;
	}

	if (advertised_fec == 0) {
		PMD_DRV_LOG(ERR, "Firmware reported an unknown FEC mode.");
		return -EINVAL;
	}

	*fec_capa = (uint32_t)advertised_fec;

	return 0;
}

static int
hinic3_fec_capability_get(struct rte_eth_dev *dev,
			  struct rte_eth_fec_capa *speed_fec_capa,
			  unsigned int num)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct nic_port_info port_info = { 0 };
	uint8_t supported_fec = 0;
	int err;

	if (speed_fec_capa == NULL)
		return HINIC3_FEC_CAPA_NUM_PER_SPEED;

	if (num < HINIC3_FEC_CAPA_NUM_PER_SPEED) {
		PMD_DRV_LOG(ERR, "Not enough array size(%u) to store FEC capabilities, should not be less than %u.",
			    num, HINIC3_FEC_CAPA_NUM_PER_SPEED);
		return HINIC3_FEC_CAPA_NUM_PER_SPEED;
	}

	err = hinic3_get_fec_mode(nic_dev->hwdev, NULL, &supported_fec);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to get fec capability, err: %d.", err);
		return err;
	}

	err = hinic3_get_port_info(nic_dev->hwdev, &port_info);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to get port info, err: %d.", err);
		return err;
	}

	if (port_info.speed >= RTE_DIM(hinic3_speed_capa))
		return -EINVAL;

	speed_fec_capa->speed = hinic3_speed_capa[port_info.speed];
	/* AUTO is managed by hinicadm5 and cannot be set through the PMD. */
	speed_fec_capa->capa = (uint32_t)supported_fec &
			       ~RTE_ETH_FEC_MODE_CAPA_MASK(AUTO);

	if (speed_fec_capa->capa == 0)
		return -ENOTSUP;

	return HINIC3_FEC_CAPA_NUM_PER_SPEED;
}

static int
hinic3_eth_dev_priv_dump(struct rte_eth_dev *dev, FILE *file)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct rte_pci_device *pci_dev = RTE_CLASS_TO_BUS_DEVICE(dev, *pci_dev);
	struct hinic3_hwdev *hwdev;

	if (file == NULL)
		return -EINVAL;

	hwdev = nic_dev->hwdev;
	if (hwdev == NULL || hwdev->func_to_func == NULL)
		return 0;

	fprintf(file, "mbox_send_cnt: %" PRIu64 "\n",
		hwdev->func_to_func->mbox_send_cnt);
	fprintf(file, "mbox_ack_cnt: %" PRIu64 "\n",
		hwdev->func_to_func->mbox_ack_cnt);

	hinic3_dump_aeq_info(hwdev, file);
	if (hwdev->func_to_func->mbox_error)
		hinic3_dump_aeq_mbox_info(hwdev, pci_dev, file);

	return 0;
}

static const struct eth_dev_ops hinic3_pmd_ops = {
	.dev_configure                 = hinic3_dev_configure,
	.dev_infos_get                 = hinic3_dev_infos_get,
	.dev_supported_ptypes_get      = hinic3_dev_supported_ptypes_get,
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
	.promiscuous_enable            = hinic3_dev_promiscuous_enable,
	.promiscuous_disable           = hinic3_dev_promiscuous_disable,
	.flow_ctrl_get                 = hinic3_dev_flow_ctrl_get,
	.flow_ctrl_set                 = hinic3_dev_flow_ctrl_set,
	.rss_hash_update               = hinic3_rss_hash_update,
	.rss_hash_conf_get             = hinic3_rss_conf_get,
	.reta_update                   = hinic3_rss_reta_update,
	.reta_query                    = hinic3_rss_reta_query,

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
	.flow_ops_get                  = hinic3_dev_filter_ctrl,
	.fec_get_capability            = hinic3_fec_capability_get,
	.fec_get                       = hinic3_fec_get,
	.fec_set                       = hinic3_fec_set,
	.eth_dev_priv_dump             = hinic3_eth_dev_priv_dump,
};

static const struct eth_dev_ops hinic3_pmd_vf_ops = {
	.dev_configure                 = hinic3_dev_configure,
	.dev_infos_get                 = hinic3_dev_infos_get,
	.dev_supported_ptypes_get      = hinic3_dev_supported_ptypes_get,
	.fw_version_get                = hinic3_fw_version_get,
	.dev_set_link_up               = hinic3_dev_set_link_up,
	.dev_set_link_down             = hinic3_dev_set_link_down,
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
	.promiscuous_enable            = hinic3_dev_promiscuous_enable,
	.promiscuous_disable           = hinic3_dev_promiscuous_disable,
	.rss_hash_update               = hinic3_rss_hash_update,
	.rss_hash_conf_get             = hinic3_rss_conf_get,
	.reta_update                   = hinic3_rss_reta_update,
	.reta_query                    = hinic3_rss_reta_query,

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
	.flow_ops_get                  = hinic3_dev_filter_ctrl,
	.eth_dev_priv_dump             = hinic3_eth_dev_priv_dump,
};

static void hinic3_nic_tx_rx_ops_init(struct hinic3_nic_dev *nic_dev)
{
	if (HINIC3_SUPPORT_TX_WQE_COMPACT_TASK(nic_dev))
		nic_dev->tx_ops->nic_tx_set_wqe_offload = hinic3_tx_set_compact_task_offload;
	else
		nic_dev->tx_ops->nic_tx_set_wqe_offload = hinic3_tx_set_normal_task_offload;
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

	pci_dev = RTE_CLASS_TO_BUS_DEVICE(eth_dev, *pci_dev);

	/* EAL is secondary and eth_dev is already created. */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		PMD_DRV_LOG(INFO, "Initialize %s in secondary process",
			    eth_dev->data->name);

		nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);

		if (HINIC3_FUNC_TYPE(nic_dev->hwdev) == TYPE_VF)
			eth_dev->dev_ops = &hinic3_pmd_vf_ops;
		else
			eth_dev->dev_ops = &hinic3_pmd_ops;

		return 0;
	}

	nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);
	memset(nic_dev, 0, sizeof(*nic_dev));
	nic_dev->nic_type = hinic3_get_nic_type(pci_dev);
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

	nic_dev->tx_ops = rte_zmalloc("tx_ops", sizeof(struct hinic3_nic_tx_ops), 0);
	if (!nic_dev->tx_ops) {
		PMD_DRV_LOG(ERR, "Allocate tx_ops memory failed");
		err = -ENOMEM;
		goto alloc_tx_ops_fail;
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
	nic_dev->hwdev->pci_dev = RTE_CLASS_TO_BUS_DEVICE(eth_dev, *nic_dev->hwdev->pci_dev);
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

	hinic3_nic_tx_rx_ops_init(nic_dev);

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
	rte_free(nic_dev->tx_ops);
	nic_dev->tx_ops = NULL;

alloc_tx_ops_fail:
	rte_free(eth_dev->data->mac_addrs);
	eth_dev->data->mac_addrs = NULL;

alloc_eth_addr_fail:
	PMD_DRV_LOG(ERR, "Initialize %s in primary failed",
		    eth_dev->data->name);
	return err;
}

static int
hinic3_nic_common_args_check_handler(const char *key, const char *val, void *opaque)
{
	struct hinic3_nic_common_dev_config *config = opaque;
	char *end;
	signed long tmp;

	if (val == NULL || *val == '\0') {
		PMD_DRV_LOG(ERR, "Key %s is missing value.", key);
		return -EINVAL;
	}

	errno = 0;
	tmp = strtol(val, &end, 10);
	if (errno != 0 || *end != '\0' || tmp < 0) {
		PMD_DRV_LOG(ERR, "%s: \"%s\" is an invalid integer.", key, val);
		return -EINVAL;
	}

	if (strcmp(key, "tx_pending_limit") == 0) {
		if (tmp > UINT8_MAX)
			goto out_of_range;
		config->tx_pending_limit = tmp;
	} else if (strcmp(key, "tx_coalescing_time") == 0) {
		if (tmp > UINT8_MAX)
			goto out_of_range;
		config->tx_coalescing_time = tmp;
	} else if (strcmp(key, "rx_cqe_compact_en") == 0) {
		config->rx_cqe_compact_en = !!tmp;
	} else if (strcmp(key, "rx_cqe_coalesce_num") == 0) {
		if (tmp > UINT8_MAX)
			goto out_of_range;
		config->rx_cqe_coalesce_num = tmp;
	} else if (strcmp(key, "rx_cqe_timer_loop") == 0) {
		if (tmp > UINT8_MAX)
			goto out_of_range;
		config->rx_cqe_timer_loop = tmp;
	} else {
		PMD_DRV_LOG(ERR, "Unknown parameter: %s", key);
		return -EINVAL;
	}

	return 0;

out_of_range:
	PMD_DRV_LOG(ERR, "%s: value exceeds the maximum allowed value.", key);
	return -EINVAL;
}

static const char *const hinic3_nic_common_valid_args[] = {
	"tx_pending_limit",
	"tx_coalescing_time",
	"rx_cqe_compact_en",
	"rx_cqe_coalesce_num",
	"rx_cqe_timer_loop",
	NULL
};

static int
hinic3_nic_common_config_get(struct rte_pci_device *pci_dev,
			     struct hinic3_nic_common_dev_config *config)
{
	int ret = 0;
	struct rte_kvargs *kvlist;
	struct rte_device *eal_dev = &pci_dev->device;

	/* Set private param defaults. */
	config->tx_pending_limit = HINIC3_DEFAULT_TX_CI_PENDING_LIMIT;
	config->tx_coalescing_time = HINIC3_DEFAULT_TX_CI_COALESCING_TIME;
	config->rx_cqe_compact_en = HINIC3_RX_CQE_COMPACT_EN;
	config->rx_cqe_coalesce_num = HINIC3_RX_CQE_COALESCE_NUM;
	config->rx_cqe_timer_loop = HINIC3_RX_CQE_TIMER_LOOP;

	if (eal_dev->devargs == NULL || eal_dev->devargs->args == NULL)
		return 0;

	kvlist = rte_kvargs_parse(eal_dev->devargs->args,
				  hinic3_nic_common_valid_args);
	if (kvlist == NULL) {
		PMD_DRV_LOG(ERR, "nic private parameter err, the format must be '-a dev,[key]=[value]'.");
		return -EINVAL;
	}

	ret = rte_kvargs_process(kvlist, NULL, hinic3_nic_common_args_check_handler, config);
	if (ret < 0)
		ret = -EINVAL;

	rte_kvargs_free(kvlist);

	PMD_DRV_LOG(INFO, "tx_pending_limit:%upkt, tx_coalescing_time:%uus, "
		"rx_cqe_coalesce_num:%upkt, rx_cqe_timer_loop:%uus.",
		config->tx_pending_limit * HINIC3_COALESCING_UNIT_PKTS,
		config->tx_coalescing_time,
		config->rx_cqe_coalesce_num * HINIC3_COALESCING_UNIT_PKTS,
		config->rx_cqe_timer_loop);
	return ret;
}

static void hinic3_nic_feature_init(struct hinic3_nic_dev *nic_dev)
{
	/*
	 * The SP230 reports the HW compact CQE feature bit
	 * (NIC_F_RX_HW_COMPACT_CQE, bit 20) and always uses the compact
	 * Rx mode. The SP560 reports the SW compact CQE feature bit
	 * (NIC_F_RX_SW_COMPACT_CQE, bit 27) and uses either mode. The
	 * SP620 reports neither.
	 */
	if (HINIC3_SUPPORT_RX_HW_COMPACT_CQE(nic_dev)) {
		nic_dev->config.rx_cqe_compact_en = HINIC3_RX_CQE_COMPACT_EN;
	} else if (HINIC3_SUPPORT_RX_SW_COMPACT_CQE(nic_dev)) {
		if (nic_dev->config.rx_cqe_compact_en == 0) {
			/*
			 * Drop the SW capability too, so the RQ context setup
			 * in hinic3_nic_io.c builds the separate-CQE layout.
			 */
			nic_dev->feature_cap &= ~NIC_F_RX_SW_COMPACT_CQE;
		}
	} else {
		nic_dev->config.rx_cqe_compact_en = 0;
	}

	PMD_DRV_LOG(INFO, "nic rx burst mode is %s.",
		    nic_dev->config.rx_cqe_compact_en ? "compact" : "separate");
}

static int hinic3_dev_uninit(struct rte_eth_dev *dev);

static int
hinic3_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);
	struct hinic3_nic_common_dev_config config;
	int err = 0;

	pci_dev = RTE_CLASS_TO_BUS_DEVICE(eth_dev, *pci_dev);

	PMD_DRV_LOG(INFO, "Initializing %.4x:%.2x:%.2x.%x in %s process",
		    pci_dev->addr.domain, pci_dev->addr.bus,
		    pci_dev->addr.devid, pci_dev->addr.function,
		    (rte_eal_process_type() == RTE_PROC_PRIMARY) ? "primary"
								 : "secondary");

	PMD_DRV_LOG(DEBUG, "Network Interface pmd driver version: %s",
		    HINIC3_PMD_DRV_VERSION);

	/*
	 * Parse device arguments before allocating any resources so that an
	 * invalid argument fails the probe without leaking memory. Secondary
	 * processes re-use the primary's configuration and must not re-parse
	 * devargs or overwrite the primary's settings.
	 */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		err = hinic3_nic_common_config_get(pci_dev, &config);
		if (err < 0) {
			PMD_DRV_LOG(ERR, "Failed to get nic device arguments.");
			return err;
		}
	}

	err = hinic3_func_init(eth_dev);
	if (err)
		return err;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		nic_dev->config = config;
		hinic3_nic_feature_init(nic_dev);

		err = hinic3_init_rx_ptype_table(eth_dev);
		if (err) {
			PMD_DRV_LOG(ERR, "Failed to create ptype_table.");
			hinic3_dev_uninit(eth_dev);
			return err;
		}
	}

	eth_dev->tx_pkt_prepare = hinic3_prep_pkts;
	eth_dev->tx_pkt_burst = hinic3_xmit_pkts;

	/*
	 * The Rx burst follows the same per-port CQE mode that drives the
	 * RQ WQE type, the CI/CQE memzone and the ptype table.
	 */
	if (nic_dev->config.rx_cqe_compact_en)
		eth_dev->rx_pkt_burst = hinic3_recv_pkts_compact_cqe;
	else
		eth_dev->rx_pkt_burst = hinic3_recv_pkts;

	return 0;
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
	{RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HINIC3_DEV_ID_SP620)},
	{RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HINIC3_DEV_ID_VF_SP620)},

	{RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HINIC3_DEV_ID_SP230)},
	{RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HINIC3_DEV_ID_VF_SP230)},

	{RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HINIC3_DEV_ID_SP920)},
	{RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HINIC3_DEV_ID_SP560)},
	{RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HINIC3_DEV_ID_VF_SP560)},
	{RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HINIC3_DEV_ID_HYPER_VF_SP560)},

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
RTE_PMD_REGISTER_PARAM_STRING(net_hinic3,
	"tx_pending_limit=<int> "
	"tx_coalescing_time=<int> "
	"rx_cqe_compact_en=<int> "
	"rx_cqe_coalesce_num=<int> "
	"rx_cqe_timer_loop=<int>");

RTE_INIT(hinic3_init_log)
{
	hinic3_logtype = rte_log_register("pmd.net.hinic3");
	if (hinic3_logtype >= 0)
		rte_log_set_level(hinic3_logtype, RTE_LOG_INFO);
}
