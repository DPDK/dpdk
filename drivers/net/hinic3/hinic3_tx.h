/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC3_TX_H_
#define _HINIC3_TX_H_

#define MAX_SINGLE_SGE_SIZE		 65536
#define HINIC3_NONTSO_PKT_MAX_SGE	 38 /**< non-tso max sge 38. */
#define HINIC3_NONTSO_SEG_NUM_VALID(num) ((num) <= HINIC3_NONTSO_PKT_MAX_SGE)

#define HINIC3_TSO_PKT_MAX_SGE		127 /**< tso max sge 127. */
#define HINIC3_TSO_SEG_NUM_INVALID(num) ((num) > HINIC3_TSO_PKT_MAX_SGE)

/* Tx offload info. */
struct hinic3_tx_offload_info {
	uint8_t outer_l2_len;
	uint8_t outer_l3_type;
	uint16_t outer_l3_len;

	uint8_t inner_l2_len;
	uint8_t inner_l3_type;
	uint16_t inner_l3_len;

	uint8_t tunnel_length;
	uint8_t tunnel_type;
	uint8_t inner_l4_type;
	uint8_t inner_l4_len;

	uint16_t payload_offset;
	uint8_t inner_l4_tcp_udp;
	uint8_t rsvd0; /**< Reserved field. */
};

/* Tx wqe ctx. */
struct hinic3_wqe_info {
	uint8_t around; /**< Indicates whether the WQE is bypassed. */
	uint8_t cpy_mbuf_cnt;
	uint16_t sge_cnt;

	uint8_t offload;
	uint8_t rsvd0; /**< Reserved field 0. */
	uint16_t payload_offset;

	uint8_t wrapped;
	uint8_t owner;
	uint16_t pi;

	uint16_t wqebb_cnt;
	uint16_t rsvd1; /**< Reserved field 1. */

	uint32_t queue_info;
};

/* Descriptor for the send queue of wqe. */
struct hinic3_sq_wqe_desc {
	uint32_t ctrl_len;
	uint32_t queue_info;
	uint32_t hi_addr;
	uint32_t lo_addr;
};

/* Describes the send queue task. */
struct hinic3_sq_task {
	uint32_t pkt_info0;
	uint32_t ip_identify;
	uint32_t pkt_info2;
	uint32_t vlan_offload;
};

/* Descriptor that describes the transmit queue buffer. */
struct hinic3_sq_bufdesc {
	uint32_t len;     /**< 31-bits Length, L2NIC only use length[17:0]. */
	uint32_t rsvd;    /**< Reserved field. */
	uint32_t hi_addr; /**< Upper address. */
	uint32_t lo_addr; /**< Lower address. */
};

/* Compact work queue entry that describes the send queue (SQ). */
struct hinic3_sq_compact_wqe {
	struct hinic3_sq_wqe_desc wqe_desc;
};

/* Extend work queue entry that describes the send queue (SQ). */
struct hinic3_sq_extend_wqe {
	struct hinic3_sq_wqe_desc wqe_desc;
	struct hinic3_sq_task task;
	struct hinic3_sq_bufdesc buf_desc[];
};

struct hinic3_sq_wqe {
	union {
		struct hinic3_sq_compact_wqe compact_wqe;
		struct hinic3_sq_extend_wqe extend_wqe;
	};
};

struct hinic3_sq_wqe_combo {
	struct hinic3_sq_wqe_desc *hdr;
	struct hinic3_sq_task *task;
	struct hinic3_sq_bufdesc *bds_head;
	uint32_t wqe_type;
	uint32_t task_type;
};

enum sq_wqe_data_format {
	SQ_NORMAL_WQE = 0,
};

/* Indicates the type of a WQE. */
enum sq_wqe_ec_type {
	SQ_WQE_COMPACT_TYPE = 0,
	SQ_WQE_EXTENDED_TYPE = 1,
};

#define COMPACT_WQE_MAX_CTRL_LEN 0x3FFF

/* Indicates the type of tasks with different lengths. */
enum sq_wqe_tasksect_len_type {
	SQ_WQE_TASKSECT_46BITS = 0,
	SQ_WQE_TASKSECT_16BYTES = 1,
};

/** Setting and obtaining queue information */
#define SQ_CTRL_BD0_LEN_SHIFT	   0
#define SQ_CTRL_RSVD_SHIFT	   18
#define SQ_CTRL_BUFDESC_NUM_SHIFT  19
#define SQ_CTRL_TASKSECT_LEN_SHIFT 27
#define SQ_CTRL_DATA_FORMAT_SHIFT  28
#define SQ_CTRL_DIRECT_SHIFT	   29
#define SQ_CTRL_EXTENDED_SHIFT	   30
#define SQ_CTRL_OWNER_SHIFT	   31

#define SQ_CTRL_BD0_LEN_MASK	  0x3FFFFU
#define SQ_CTRL_RSVD_MASK	  0x1U
#define SQ_CTRL_BUFDESC_NUM_MASK  0xFFU
#define SQ_CTRL_TASKSECT_LEN_MASK 0x1U
#define SQ_CTRL_DATA_FORMAT_MASK  0x1U
#define SQ_CTRL_DIRECT_MASK	  0x1U
#define SQ_CTRL_EXTENDED_MASK	  0x1U
#define SQ_CTRL_OWNER_MASK	  0x1U

#define SQ_CTRL_SET(val, member) \
	(((uint32_t)(val) & SQ_CTRL_##member##_MASK) << SQ_CTRL_##member##_SHIFT)
#define SQ_CTRL_GET(val, member) \
	(((val) >> SQ_CTRL_##member##_SHIFT) & SQ_CTRL_##member##_MASK)
#define SQ_CTRL_CLEAR(val, member) \
	((val) & (~(SQ_CTRL_##member##_MASK << SQ_CTRL_##member##_SHIFT)))

#define SQ_CTRL_QUEUE_INFO_PKT_TYPE_SHIFT  0
#define SQ_CTRL_QUEUE_INFO_PLDOFF_SHIFT	   2
#define SQ_CTRL_QUEUE_INFO_UFO_SHIFT	   10
#define SQ_CTRL_QUEUE_INFO_TSO_SHIFT	   11
#define SQ_CTRL_QUEUE_INFO_TCPUDP_CS_SHIFT 12
#define SQ_CTRL_QUEUE_INFO_MSS_SHIFT	   13
#define SQ_CTRL_QUEUE_INFO_SCTP_SHIFT	   27
#define SQ_CTRL_QUEUE_INFO_UC_SHIFT	   28
#define SQ_CTRL_QUEUE_INFO_PRI_SHIFT	   29

#define SQ_CTRL_QUEUE_INFO_PKT_TYPE_MASK  0x3U
#define SQ_CTRL_QUEUE_INFO_PLDOFF_MASK	  0xFFU
#define SQ_CTRL_QUEUE_INFO_UFO_MASK	  0x1U
#define SQ_CTRL_QUEUE_INFO_TSO_MASK	  0x1U
#define SQ_CTRL_QUEUE_INFO_TCPUDP_CS_MASK 0x1U
#define SQ_CTRL_QUEUE_INFO_MSS_MASK	  0x3FFFU
#define SQ_CTRL_QUEUE_INFO_SCTP_MASK	  0x1U
#define SQ_CTRL_QUEUE_INFO_UC_MASK	  0x1U
#define SQ_CTRL_QUEUE_INFO_PRI_MASK	  0x7U

#define SQ_CTRL_QUEUE_INFO_SET(val, member)                \
	(((uint32_t)(val) & SQ_CTRL_QUEUE_INFO_##member##_MASK) \
	 << SQ_CTRL_QUEUE_INFO_##member##_SHIFT)
#define SQ_CTRL_QUEUE_INFO_GET(val, member)               \
	(((val) >> SQ_CTRL_QUEUE_INFO_##member##_SHIFT) & \
	 SQ_CTRL_QUEUE_INFO_##member##_MASK)
#define SQ_CTRL_QUEUE_INFO_CLEAR(val, member)          \
	((val) & (~(SQ_CTRL_QUEUE_INFO_##member##_MASK \
		    << SQ_CTRL_QUEUE_INFO_##member##_SHIFT)))

/* Setting and obtaining task information */
#define SQ_TASK_INFO0_TUNNEL_FLAG_SHIFT	    19
#define SQ_TASK_INFO0_ESP_NEXT_PROTO_SHIFT  22
#define SQ_TASK_INFO0_INNER_L4_EN_SHIFT	    24
#define SQ_TASK_INFO0_INNER_L3_EN_SHIFT	    25
#define SQ_TASK_INFO0_INNER_L4_PSEUDO_SHIFT 26
#define SQ_TASK_INFO0_OUT_L4_EN_SHIFT	    27
#define SQ_TASK_INFO0_OUT_L3_EN_SHIFT	    28
#define SQ_TASK_INFO0_OUT_L4_PSEUDO_SHIFT   29
#define SQ_TASK_INFO0_ESP_OFFLOAD_SHIFT	    30
#define SQ_TASK_INFO0_IPSEC_PROTO_SHIFT	    31

#define SQ_TASK_INFO0_TUNNEL_FLAG_MASK	   0x1U
#define SQ_TASK_INFO0_ESP_NEXT_PROTO_MASK  0x3U
#define SQ_TASK_INFO0_INNER_L4_EN_MASK	   0x1U
#define SQ_TASK_INFO0_INNER_L3_EN_MASK	   0x1U
#define SQ_TASK_INFO0_INNER_L4_PSEUDO_MASK 0x1U
#define SQ_TASK_INFO0_OUT_L4_EN_MASK	   0x1U
#define SQ_TASK_INFO0_OUT_L3_EN_MASK	   0x1U
#define SQ_TASK_INFO0_OUT_L4_PSEUDO_MASK   0x1U
#define SQ_TASK_INFO0_ESP_OFFLOAD_MASK	   0x1U
#define SQ_TASK_INFO0_IPSEC_PROTO_MASK	   0x1U

#define SQ_TASK_INFO0_SET(val, member)                \
	(((uint32_t)(val) & SQ_TASK_INFO0_##member##_MASK) \
	 << SQ_TASK_INFO0_##member##_SHIFT)
#define SQ_TASK_INFO0_GET(val, member)               \
	(((val) >> SQ_TASK_INFO0_##member##_SHIFT) & \
	 SQ_TASK_INFO0_##member##_MASK)

#define SQ_TASK_INFO1_SET(val, member)           \
	(((val) & SQ_TASK_INFO1_##member##_MASK) \
	 << SQ_TASK_INFO1_##member##_SHIFT)
#define SQ_TASK_INFO1_GET(val, member)               \
	(((val) >> SQ_TASK_INFO1_##member##_SHIFT) & \
	 SQ_TASK_INFO1_##member##_MASK)

#define SQ_TASK_INFO3_VLAN_TAG_SHIFT	   0
#define SQ_TASK_INFO3_VLAN_TYPE_SHIFT	   16
#define SQ_TASK_INFO3_VLAN_TAG_VALID_SHIFT 19

#define SQ_TASK_INFO3_VLAN_TAG_MASK	  0xFFFFU
#define SQ_TASK_INFO3_VLAN_TYPE_MASK	  0x7U
#define SQ_TASK_INFO3_VLAN_TAG_VALID_MASK 0x1U

#define SQ_TASK_INFO3_SET(val, member)           \
	(((val) & SQ_TASK_INFO3_##member##_MASK) \
	 << SQ_TASK_INFO3_##member##_SHIFT)
#define SQ_TASK_INFO3_GET(val, member)               \
	(((val) >> SQ_TASK_INFO3_##member##_SHIFT) & \
	 SQ_TASK_INFO3_##member##_MASK)

/* Defines the TX queue status. */
enum hinic3_txq_status {
	HINIC3_TXQ_STATUS_START = 0,
	HINIC3_TXQ_STATUS_STOP,
};

/* Setting and obtaining status information. */
#define HINIC3_TXQ_IS_STARTED(txq)  ((txq)->status == HINIC3_TXQ_STATUS_START)
#define HINIC3_TXQ_IS_STOPPED(txq)  ((txq)->status == HINIC3_TXQ_STATUS_STOP)
#define HINIC3_SET_TXQ_STARTED(txq) ((txq)->status = HINIC3_TXQ_STATUS_START)
#define HINIC3_SET_TXQ_STOPPED(txq) ((txq)->status = HINIC3_TXQ_STATUS_STOP)

/* Txq info. */
struct hinic3_txq_stats {
	uint64_t packets;
	uint64_t bytes;
	uint64_t tx_busy;
	uint64_t offload_errors;
	uint64_t burst_pkts;
	uint64_t sge_len0;
	uint64_t mbuf_null;
	uint64_t cpy_pkts;
	uint64_t sge_len_too_large;

#ifdef HINIC3_XSTAT_PROF_TX
	uint64_t app_tsc;
	uint64_t pmd_tsc;
#endif

#ifdef HINIC3_XSTAT_MBUF_USE
	uint64_t tx_left_mbuf_bytes;
#endif
};

/* Structure for storing the information sent. */
struct hinic3_tx_info {
	struct rte_mbuf *mbuf;
	struct rte_mbuf *cpy_mbuf;
	int wqebb_cnt;
};

/* Indicates the sending queue of information. */
struct __rte_cache_aligned hinic3_txq {
	struct hinic3_nic_dev *nic_dev;
	uint16_t q_id;
	uint16_t q_depth;
	uint16_t q_mask;
	uint16_t wqebb_size;
	uint16_t wqebb_shift;
	uint16_t cons_idx;
	uint16_t prod_idx;
	uint16_t status;

	uint16_t tx_free_thresh;
	uint16_t owner;
	bool tx_deferred_start; /* don't start this queue in dev start */
	void *db_addr;
	struct hinic3_tx_info *tx_info;

	const struct rte_memzone *sq_mz;
	void *queue_buf_vaddr;
	rte_iova_t queue_buf_paddr;

	const struct rte_memzone *ci_mz;
	volatile uint16_t *ci_vaddr_base;
	rte_iova_t ci_dma_base;
	uint64_t sq_head_addr;
	uint64_t sq_bot_sge_addr;
	uint32_t cos;
	struct hinic3_txq_stats txq_stats;
#ifdef HINIC3_XSTAT_PROF_TX
	uint64_t prof_tx_end_tsc;
#endif
};

void hinic3_flush_txqs(struct hinic3_nic_dev *nic_dev);
void hinic3_free_txq_mbufs(struct hinic3_txq *txq);
void hinic3_free_all_txq_mbufs(struct hinic3_nic_dev *nic_dev);
int hinic3_stop_sq(struct hinic3_txq *txq);
int hinic3_start_all_sqs(struct rte_eth_dev *eth_dev);
int hinic3_tx_done_cleanup(void *txq, uint32_t free_cnt);
#endif /**< _HINIC3_TX_H_ */
