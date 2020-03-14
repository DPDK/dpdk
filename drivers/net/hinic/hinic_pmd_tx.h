/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC_PMD_TX_H_
#define _HINIC_PMD_TX_H_

#define HINIC_DEFAULT_TX_FREE_THRESH	32
#define HINIC_MAX_TX_FREE_BULK		64

#define HINIC_GET_WQ_HEAD(txq)		((txq)->wq->queue_buf_vaddr)

#define HINIC_GET_WQ_TAIL(txq)		\
		((txq)->wq->queue_buf_vaddr + (txq)->wq->wq_buf_size)

#define HINIC_TX_CKSUM_OFFLOAD_MASK (	\
		PKT_TX_IP_CKSUM |	\
		PKT_TX_TCP_CKSUM |	\
		PKT_TX_UDP_CKSUM |      \
		PKT_TX_SCTP_CKSUM |	\
		PKT_TX_OUTER_IP_CKSUM |	\
		PKT_TX_TCP_SEG)

enum sq_wqe_type {
	SQ_NORMAL_WQE = 0,
};

/* tx offload info */
struct hinic_tx_offload_info {
	u8 outer_l2_len;
	u8 outer_l3_type;
	u16 outer_l3_len;

	u8 inner_l2_len;
	u8 inner_l3_type;
	u16 inner_l3_len;

	u8 tunnel_length;
	u8 tunnel_type;
	u8 inner_l4_type;
	u8 inner_l4_len;

	u16 payload_offset;
	u8 inner_l4_tcp_udp;
	u8 rsvd0;
};

/* tx sge info */
struct hinic_wqe_info {
	u16 pi;
	u16 owner;
	u16 around;
	u16 seq_wqebbs;
	u16 sge_cnt;
	u16 cpy_mbuf_cnt;
};

struct hinic_sq_ctrl {
	u32	ctrl_fmt;
	u32	queue_info;
};

struct hinic_sq_task {
	u32		pkt_info0;
	u32		pkt_info1;
	u32		pkt_info2;
	u32		ufo_v6_identify;
	u32		pkt_info4;
	u32		rsvd5;
};

struct hinic_sq_bufdesc {
	struct hinic_sge sge;
	u32	rsvd;
};

struct hinic_sq_wqe {
	/* sq wqe control section */
	struct hinic_sq_ctrl		ctrl;

	/* sq task control section */
	struct hinic_sq_task		task;

	/* sq sge section start address, 1~127 sges */
	struct hinic_sq_bufdesc     buf_descs[0];
};

struct hinic_txq_stats {
	u64 packets;
	u64 bytes;
	u64 rl_drop;
	u64 tx_busy;
	u64 off_errs;
	u64 cpy_pkts;
	u64 burst_pkts;
};

struct hinic_tx_info {
	struct rte_mbuf *mbuf;
	int wqebb_cnt;
	struct rte_mbuf *cpy_mbuf;
};

struct hinic_txq {
	/* cacheline0 */
	struct hinic_nic_dev *nic_dev;
	struct hinic_wq *wq;
	struct hinic_sq *sq;
	volatile u16 *cons_idx_addr;
	struct hinic_tx_info *tx_info;

	u16 tx_free_thresh;
	u16 port_id;
	u16 q_id;
	u16 q_depth;
	u32 cos;
	u32 socket_id;

	/* cacheline1 */
	struct hinic_txq_stats txq_stats;
	u64 sq_head_addr;
	u64 sq_bot_sge_addr;
};

int hinic_setup_tx_resources(struct hinic_txq *txq);

void hinic_free_all_tx_resources(struct rte_eth_dev *eth_dev);

void hinic_free_all_tx_mbuf(struct rte_eth_dev *eth_dev);

void hinic_free_tx_resources(struct hinic_txq *txq);

u16 hinic_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, u16 nb_pkts);

void hinic_free_all_tx_mbufs(struct hinic_txq *txq);

void hinic_txq_get_stats(struct hinic_txq *txq, struct hinic_txq_stats *stats);

void hinic_txq_stats_reset(struct hinic_txq *txq);

int hinic_create_sq(struct hinic_hwdev *hwdev, u16 q_id,
			u16 sq_depth, unsigned int socket_id);

void hinic_destroy_sq(struct hinic_hwdev *hwdev, u16 q_id);

#endif /* _HINIC_PMD_TX_H_ */
