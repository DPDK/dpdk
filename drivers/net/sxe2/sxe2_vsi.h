/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_VSI_H
#define SXE2_VSI_H

#include <rte_os.h>
#include "sxe2_drv_cmd.h"

#define SXE2_MAX_BOND_MEMBER_CNT   4

enum sxe2_drv_type {
	SXE2_MAX_DRV_TYPE_DPDK = 0,
	SXE2_MAX_DRV_TYPE_KERNEL,
	SXE2_MAX_DRV_TYPE_CNT,
};

#define SXE2_MAX_USER_PRIORITY        (8)

#define SXE2_DFLT_NUM_RX_DESC 512
#define SXE2_DFLT_NUM_TX_DESC 512

#define SXE2_DFLT_Q_NUM_OTHER_VSI 1
#define SXE2_INVALID_VSI_ID    0xFFFF

struct sxe2_adapter;
struct sxe2_drv_vsi_caps;
struct rte_eth_dev;

enum sxe2_vsi_type {
	SXE2_VSI_T_PF = 0,
	SXE2_VSI_T_VF,
	SXE2_VSI_T_CTRL,
	SXE2_VSI_T_LB,
	SXE2_VSI_T_MACVLAN,
	SXE2_VSI_T_ESW,
	SXE2_VSI_T_RDMA,
	SXE2_VSI_T_DPDK_PF,
	SXE2_VSI_T_DPDK_VF,
	SXE2_VSI_T_DPDK_ESW,
	SXE2_VSI_T_NR,
};

struct sxe2_queue_info {
	uint16_t base_idx_in_nic;
	uint16_t base_idx_in_func;
	uint16_t q_cnt;
	uint16_t depth;
	uint16_t rx_buf_len;
	uint16_t max_frame_len;
	struct sxe2_queue **queues;
};

struct sxe2_vsi_irqs {
	uint16_t avail_cnt;
	uint16_t used_cnt;
	uint16_t base_idx_in_pf;
};

enum {
	sxe2_VSI_DOWN = 0,
	sxe2_VSI_CLOSE,
	sxe2_VSI_DISABLE,
	sxe2_VSI_MAX,
};

struct sxe2_stats {
	uint64_t ipackets;

	uint64_t opackets;

	uint64_t ibytes;

	uint64_t obytes;

	uint64_t ierrors;

	uint64_t imissed;

	uint64_t rx_out_of_buffer;
	uint64_t rx_qblock_drop;

	uint64_t tx_frame_good;
	uint64_t rx_frame_good;
	uint64_t rx_crc_errors;
	uint64_t tx_bytes_good;
	uint64_t rx_bytes_good;
	uint64_t tx_multicast_good;
	uint64_t tx_broadcast_good;
	uint64_t rx_multicast_good;
	uint64_t rx_broadcast_good;
	uint64_t rx_len_errors;
	uint64_t rx_out_of_range_errors;
	uint64_t rx_oversize_pkts_phy;
	uint64_t rx_symbol_err;
	uint64_t rx_pause_frame;
	uint64_t tx_pause_frame;

	uint64_t rx_discards_phy;
	uint64_t rx_discards_ips_phy;

	uint64_t tx_dropped_link_down;
	uint64_t rx_undersize_good;
	uint64_t rx_runt_error;
	uint64_t tx_bytes_good_bad;
	uint64_t tx_frame_good_bad;
	uint64_t rx_jabbers;
	uint64_t rx_size_64;
	uint64_t rx_size_65_127;
	uint64_t rx_size_128_255;
	uint64_t rx_size_256_511;
	uint64_t rx_size_512_1023;
	uint64_t rx_size_1024_1522;
	uint64_t rx_size_1523_max;
	uint64_t rx_pcs_symbol_err_phy;
	uint64_t rx_corrected_bits_phy;
	uint64_t rx_err_lane_0_phy;
	uint64_t rx_err_lane_1_phy;
	uint64_t rx_err_lane_2_phy;
	uint64_t rx_err_lane_3_phy;

	uint64_t rx_prio_buf_discard[SXE2_MAX_USER_PRIORITY];
	uint64_t rx_illegal_bytes;
	uint64_t rx_oversize_good;
	uint64_t tx_unicast;
	uint64_t tx_broadcast;
	uint64_t tx_multicast;
	uint64_t tx_vlan_packet_good;
	uint64_t tx_size_64;
	uint64_t tx_size_65_127;
	uint64_t tx_size_128_255;
	uint64_t tx_size_256_511;
	uint64_t tx_size_512_1023;
	uint64_t tx_size_1024_1522;
	uint64_t tx_size_1523_max;
	uint64_t tx_underflow_error;
	uint64_t rx_byte_good_bad;
	uint64_t rx_frame_good_bad;
	uint64_t rx_unicast_good;
	uint64_t rx_vlan_packets;

	uint64_t prio_xoff_rx[SXE2_MAX_USER_PRIORITY];
	uint64_t prio_xon_rx[SXE2_MAX_USER_PRIORITY];
	uint64_t prio_xon_tx[SXE2_MAX_USER_PRIORITY];
	uint64_t prio_xoff_tx[SXE2_MAX_USER_PRIORITY];
	uint64_t prio_xon_2_xoff[SXE2_MAX_USER_PRIORITY];

	uint64_t rx_vsi_unicast_packets;
	uint64_t rx_vsi_bytes;
	uint64_t tx_vsi_unicast_packets;
	uint64_t tx_vsi_bytes;
	uint64_t rx_vsi_multicast_packets;
	uint64_t tx_vsi_multicast_packets;
	uint64_t rx_vsi_broadcast_packets;
	uint64_t tx_vsi_broadcast_packets;

	uint64_t rx_sw_unicast_packets;
	uint64_t rx_sw_broadcast_packets;
	uint64_t rx_sw_multicast_packets;
	uint64_t rx_sw_drop_packets;
	uint64_t rx_sw_drop_bytes;
};

struct sxe2_vsi_stats {
	struct sxe2_stats        vsi_sw_stats;
	struct sxe2_stats        vsi_sw_stats_prev;
	struct sxe2_stats        vsi_hw_stats;
	struct sxe2_stats        stats;
};

struct sxe2_vsi {
	TAILQ_ENTRY(sxe2_vsi) next;
	struct sxe2_adapter *adapter;
	uint16_t vsi_id;
	uint16_t vsi_type;
	struct sxe2_vsi_irqs irqs;
	struct sxe2_queue_info txqs;
	struct sxe2_queue_info rxqs;
	uint16_t budget;
	struct sxe2_vsi_stats vsi_stats;
};

TAILQ_HEAD(sxe2_vsi_list_head, sxe2_vsi);

struct sxe2_vsi_context {
	uint16_t func_id;
	uint16_t dpdk_vsi_id;
	uint16_t kernel_vsi_id;
	uint16_t vsi_type;

	uint16_t bond_member_kernel_vsi_id[SXE2_MAX_BOND_MEMBER_CNT];
	uint16_t bond_member_dpdk_vsi_id[SXE2_MAX_BOND_MEMBER_CNT];

	struct sxe2_vsi *main_vsi;
};

void sxe2_sw_vsi_ctx_hw_cap_set(struct sxe2_adapter *adapter,
		struct sxe2_drv_vsi_caps *vsi_caps);

int32_t sxe2_vsi_init(struct rte_eth_dev *dev);

void sxe2_vsi_uninit(struct rte_eth_dev *dev);

#endif /* SXE2_VSI_H */
