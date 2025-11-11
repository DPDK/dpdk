/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#ifndef _NBL_RESOURCE_H_
#define _NBL_RESOURCE_H_

#include "nbl_ethdev.h"
#include "nbl_include.h"
#include <stdint.h>

#define NBL_RES_MGT_TO_HW_OPS_TBL(res_mgt)	((res_mgt)->hw_ops_tbl)
#define NBL_RES_MGT_TO_HW_OPS(res_mgt)		(NBL_RES_MGT_TO_HW_OPS_TBL(res_mgt)->ops)
#define NBL_RES_MGT_TO_HW_PRIV(res_mgt)		(NBL_RES_MGT_TO_HW_OPS_TBL(res_mgt)->priv)
#define NBL_RES_MGT_TO_CHAN_OPS_TBL(res_mgt)	((res_mgt)->chan_ops_tbl)
#define NBL_RES_MGT_TO_CHAN_OPS(res_mgt)	(NBL_RES_MGT_TO_CHAN_OPS_TBL(res_mgt)->ops)
#define NBL_RES_MGT_TO_CHAN_PRIV(res_mgt)	(NBL_RES_MGT_TO_CHAN_OPS_TBL(res_mgt)->priv)
#define NBL_RES_MGT_TO_ETH_DEV(res_mgt)		((res_mgt)->eth_dev)
#define NBL_RES_MGT_TO_COMMON(res_mgt)		((res_mgt)->common)
#define NBL_RES_MGT_TO_TXRX_MGT(res_mgt)	((res_mgt)->txrx_mgt)
#define NBL_RES_MGT_TO_TX_RING(res_mgt, index)	\
	(NBL_RES_MGT_TO_TXRX_MGT(res_mgt)->tx_rings[(index)])
#define NBL_RES_MGT_TO_RX_RING(res_mgt, index)	\
	(NBL_RES_MGT_TO_TXRX_MGT(res_mgt)->rx_rings[(index)])

union nbl_tx_extend_head {
	struct nbl_tx_ehdr_leonis {
		/* DW0 */
		u32 mac_len :5;
		u32 ip_len :5;
		u32 l4_len :4;
		u32 l4_type :2;
		u32 inner_ip_type :2;
		u32 external_ip_type :2;
		u32 external_ip_len :5;
		u32 l4_tunnel_type :2;
		u32 l4_tunnel_len :5;
		/* DW1 */
		u32 l4s_sid :10;
		u32 l4s_sync_ind :1;
		u32 l4s_redun_ind :1;
		u32 l4s_redun_head_ind :1;
		u32 l4s_hdl_ind :1;
		u32 l4s_pbrac_mode :1;
		u32 rsv0 :2;
		u32 mss :14;
		u32 tso :1;
		/* DW2 */
		/* if dport = NBL_TX_DPORT_ETH; dport_info = 0
		 * if dport = NBL_TX_DPORT_HOST; dport_info = host queue id
		 * if dport = NBL_TX_DPORT_ECPU; dport_info = ecpu queue_id
		 */
		u32 dport_info :11;
		/* if dport = NBL_TX_DPORT_ETH; dport_id[3:0] = eth port id, dport_id[9:4] = lag id
		 * if dport = NBL_TX_DPORT_HOST; dport_id[9:0] = host vsi_id
		 * if dport = NBL_TX_DPORT_ECPU; dport_id[9:0] = ecpu vsi_id
		 */
		u32 dport_id :10;
#define NBL_TX_DPORT_ID_LAG_OFT_LEONIS	(4)
		u32 dport :3;
#define NBL_TX_DPORT_ETH		(0)
#define NBL_TX_DPORT_HOST		(1)
#define NBL_TX_DPORT_ECPU		(2)
#define NBL_TX_DPORT_EMP		(3)
#define NBL_TX_DPORT_BMC		(4)
#define NBL_TX_DPORT_EMP_DRACO		(2)
#define NBL_TX_DPORT_BMC_DRACO		(3)
		u32 fwd :2;
#define NBL_TX_FWD_TYPE_DROP		(0)
#define NBL_TX_FWD_TYPE_NORMAL		(1)
#define NBL_TX_FWD_TYPE_RSV		(2)
#define NBL_TX_FWD_TYPE_CPU_ASSIGNED	(3)
		u32 rss_lag_en :1;
		u32 l4_csum_en :1;
		u32 l3_csum_en :1;
		u32 rsv1 :3;
	} leonis;
};

union nbl_rx_extend_head {
	struct nbl_rx_ehdr_leonis {
		/* DW0 */
		/* 0x0:eth, 0x1:host, 0x2:ecpu, 0x3:emp, 0x4:bcm */
		u32 sport :3;
		u32 dport_info :11;
		/* sport = 0, sport_id[3:0] = eth id,
		 * sport = 1, sport_id[9:0] = host vsi_id,
		 * sport = 2, sport_id[9:0] = ecpu vsi_id,
		 */
		u32 sport_id :10;
		/* 0x0:drop, 0x1:normal, 0x2:cpu upcall */
		u32 fwd :2;
		u32 rsv0 :6;
		/* DW1 */
		u32 error_code :6;
		u32 ptype :10;
		u32 profile_id :4;
		u32 checksum_status :1;
		u32 rsv1 :1;
		u32 l4s_sid :10;
		/* DW2 */
		u32 rsv3 :2;
		u32 l4s_hdl_ind :1;
		u32 l4s_tcp_offset :14;
		u32 l4s_resync_ind :1;
		u32 l4s_check_ind :1;
		u32 l4s_dec_ind :1;
		u32 rsv2 :4;
		u32 num_buffers :8;
	} leonis;
	struct nbl_rx_ehdr_common {
		u32 dw0;
		u32 dw1;
		u32 dw2:24;
		u32 num_buffers:8;
		u32 dw3;
	} common;
};

struct nbl_packed_desc {
	rte_le64_t addr;
	rte_le32_t len;
	rte_le16_t id;
	rte_le16_t flags;
};

struct nbl_tx_entry {
	struct rte_mbuf *mbuf;
	uint16_t first_id;
};

struct nbl_rx_entry {
	struct rte_mbuf *mbuf;
};

struct nbl_res_tx_ring {
	volatile struct nbl_packed_desc *desc;
	struct nbl_tx_entry *tx_entry;
	const struct rte_memzone *net_hdr_mz;
	volatile uint8_t *notify;
	const struct rte_eth_dev *eth_dev;
	struct nbl_common_info *common;
	struct nbl_txq_stats txq_stats;
	u64 default_hdr[2];

	enum nbl_product_type product;
	int dma_limit_msb;
	bool dma_set_msb;
	u16 nb_desc;
	u16 next_to_clean;
	u16 next_to_use;

	u16 avail_used_flags;
	bool used_wrap_counter;
	u16 notify_qid;
	u16 exthdr_len;

	u16 vlan_proto;
	u16 vlan_tci;
	u16 lag_id;
	u16 vq_free_cnt;
	/* Start freeing TX buffers if there are less free descriptors than this value */
	u16 tx_free_thresh;
	/* Number of Tx descriptors to use before RS bit is set */
	u16 tx_rs_thresh;

	unsigned int size;

	u16 queue_id;

	u64 offloads;
	u64 ring_phys_addr;

	u16 (*prep_tx_ehdr)(union nbl_tx_extend_head *head, struct rte_mbuf *mbuf);
};

struct nbl_res_rx_ring {
	volatile struct nbl_packed_desc *desc;
	struct nbl_rx_entry *rx_entry;
	struct rte_mempool *mempool;
	volatile uint8_t *notify;
	const struct rte_eth_dev *eth_dev;
	struct nbl_common_info *common;
	struct nbl_rxq_stats rxq_stats;
	uint64_t mbuf_initializer; /**< value to init mbufs */
	struct rte_mbuf fake_mbuf;

	enum nbl_product_type product;
	int dma_limit_msb;
	unsigned int size;
	bool dma_set_msb;
	u16 nb_desc;
	u16 next_to_clean;
	u16 next_to_use;

	u16 avail_used_flags;
	bool used_wrap_counter;
	u16 notify_qid;
	u16 exthdr_len;

	u16 vlan_proto;
	u16 vlan_tci;
	u16 vq_free_cnt;
	u16 port_id;

	u16 queue_id;
	u16 buf_length;

	u64 offloads;
	u64 ring_phys_addr;
};

struct nbl_txrx_mgt {
	rte_spinlock_t tx_lock;
	struct nbl_res_tx_ring **tx_rings;
	struct nbl_res_rx_ring **rx_rings;
	u16 queue_offset;
	u8 tx_ring_num;
	u8 rx_ring_num;
};

struct nbl_res_info {
	u16 base_qid;
	u16 lcore_max;
	u16 *pf_qid_to_lcore_id;
	rte_atomic16_t tx_current_queue;
};

struct nbl_resource_mgt {
	struct rte_eth_dev *eth_dev;
	struct nbl_channel_ops_tbl *chan_ops_tbl;
	struct nbl_hw_ops_tbl *hw_ops_tbl;
	struct nbl_txrx_mgt *txrx_mgt;
	struct nbl_common_info *common;
	struct nbl_res_info res_info;
};

struct nbl_resource_mgt_leonis {
	struct nbl_resource_mgt res_mgt;
};

int nbl_txrx_mgt_start(struct nbl_resource_mgt *res_mgt);
void nbl_txrx_mgt_stop(struct nbl_resource_mgt *res_mgt);
int nbl_txrx_setup_ops(struct nbl_resource_ops *resource_ops);
void nbl_txrx_remove_ops(struct nbl_resource_ops *resource_ops);

#endif
