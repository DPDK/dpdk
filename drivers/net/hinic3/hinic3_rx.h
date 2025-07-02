/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC3_RX_H_
#define _HINIC3_RX_H_

#include "hinic3_wq.h"
#include "hinic3_nic_io.h"

#define RQ_CQE_OFFOLAD_TYPE_PKT_TYPE_SHIFT    0
#define RQ_CQE_OFFOLAD_TYPE_PKT_UMBCAST_SHIFT 19
#define RQ_CQE_OFFOLAD_TYPE_VLAN_EN_SHIFT     21
#define RQ_CQE_OFFOLAD_TYPE_RSS_TYPE_SHIFT    24

#define RQ_CQE_OFFOLAD_TYPE_PKT_TYPE_MASK    0xFFFU
#define RQ_CQE_OFFOLAD_TYPE_PKT_UMBCAST_MASK 0x3U
#define RQ_CQE_OFFOLAD_TYPE_VLAN_EN_MASK     0x1U
#define RQ_CQE_OFFOLAD_TYPE_RSS_TYPE_MASK    0xFFU

#define DPI_EXT_ACTION_FILED (1ULL << 32)

#define RQ_CQE_OFFOLAD_TYPE_GET(val, member)               \
	(((val) >> RQ_CQE_OFFOLAD_TYPE_##member##_SHIFT) & \
	 RQ_CQE_OFFOLAD_TYPE_##member##_MASK)

#define HINIC3_GET_RX_PKT_TYPE(offload_type) \
	RQ_CQE_OFFOLAD_TYPE_GET(offload_type, PKT_TYPE)

#define HINIC3_GET_RX_PKT_UMBCAST(offload_type) \
	RQ_CQE_OFFOLAD_TYPE_GET(offload_type, PKT_UMBCAST)

#define HINIC3_GET_RX_VLAN_OFFLOAD_EN(offload_type) \
	RQ_CQE_OFFOLAD_TYPE_GET(offload_type, VLAN_EN)

#define HINIC3_GET_RSS_TYPES(offload_type) \
	RQ_CQE_OFFOLAD_TYPE_GET(offload_type, RSS_TYPE)

#define RQ_CQE_SGE_VLAN_SHIFT 0
#define RQ_CQE_SGE_LEN_SHIFT  16

#define RQ_CQE_SGE_VLAN_MASK 0xFFFFU
#define RQ_CQE_SGE_LEN_MASK  0xFFFFU

#define RQ_CQE_SGE_GET(val, member) \
	(((val) >> RQ_CQE_SGE_##member##_SHIFT) & RQ_CQE_SGE_##member##_MASK)

#define HINIC3_GET_RX_VLAN_TAG(vlan_len) RQ_CQE_SGE_GET(vlan_len, VLAN)

#define HINIC3_GET_RX_PKT_LEN(vlan_len) RQ_CQE_SGE_GET(vlan_len, LEN)

#define RQ_CQE_STATUS_CSUM_ERR_SHIFT  0
#define RQ_CQE_STATUS_NUM_LRO_SHIFT   16
#define RQ_CQE_STATUS_LRO_PUSH_SHIFT  25
#define RQ_CQE_STATUS_LRO_ENTER_SHIFT 26
#define RQ_CQE_STATUS_LRO_INTR_SHIFT  27

#define RQ_CQE_STATUS_BP_EN_SHIFT     30
#define RQ_CQE_STATUS_RXDONE_SHIFT    31
#define RQ_CQE_STATUS_DECRY_PKT_SHIFT 29
#define RQ_CQE_STATUS_FLUSH_SHIFT     28

#define RQ_CQE_STATUS_CSUM_ERR_MASK  0xFFFFU
#define RQ_CQE_STATUS_NUM_LRO_MASK   0xFFU
#define RQ_CQE_STATUS_LRO_PUSH_MASK  0X1U
#define RQ_CQE_STATUS_LRO_ENTER_MASK 0X1U
#define RQ_CQE_STATUS_LRO_INTR_MASK  0X1U
#define RQ_CQE_STATUS_BP_EN_MASK     0X1U
#define RQ_CQE_STATUS_RXDONE_MASK    0x1U
#define RQ_CQE_STATUS_FLUSH_MASK     0x1U
#define RQ_CQE_STATUS_DECRY_PKT_MASK 0x1U

#define RQ_CQE_STATUS_GET(val, member)               \
	(((val) >> RQ_CQE_STATUS_##member##_SHIFT) & \
	 RQ_CQE_STATUS_##member##_MASK)

#define HINIC3_GET_RX_CSUM_ERR(status) RQ_CQE_STATUS_GET(status, CSUM_ERR)

#define HINIC3_GET_RX_DONE(status) RQ_CQE_STATUS_GET(status, RXDONE)

#define HINIC3_GET_RX_FLUSH(status) RQ_CQE_STATUS_GET(status, FLUSH)

#define HINIC3_GET_RX_BP_EN(status) RQ_CQE_STATUS_GET(status, BP_EN)

#define HINIC3_GET_RX_NUM_LRO(status) RQ_CQE_STATUS_GET(status, NUM_LRO)

#define HINIC3_RX_IS_DECRY_PKT(status) RQ_CQE_STATUS_GET(status, DECRY_PKT)

#define RQ_CQE_SUPER_CQE_EN_SHIFT  0
#define RQ_CQE_PKT_NUM_SHIFT	   1
#define RQ_CQE_PKT_LAST_LEN_SHIFT  6
#define RQ_CQE_PKT_FIRST_LEN_SHIFT 19

#define RQ_CQE_SUPER_CQE_EN_MASK  0x1
#define RQ_CQE_PKT_NUM_MASK	  0x1FU
#define RQ_CQE_PKT_FIRST_LEN_MASK 0x1FFFU
#define RQ_CQE_PKT_LAST_LEN_MASK  0x1FFFU

#define RQ_CQE_PKT_NUM_GET(val, member) \
	(((val) >> RQ_CQE_PKT_##member##_SHIFT) & RQ_CQE_PKT_##member##_MASK)
#define HINIC3_GET_RQ_CQE_PKT_NUM(pkt_info) RQ_CQE_PKT_NUM_GET(pkt_info, NUM)

#define RQ_CQE_SUPER_CQE_EN_GET(val, member) \
	(((val) >> RQ_CQE_##member##_SHIFT) & RQ_CQE_##member##_MASK)

#define HINIC3_GET_SUPER_CQE_EN(pkt_info) \
	RQ_CQE_SUPER_CQE_EN_GET(pkt_info, SUPER_CQE_EN)

#define RQ_CQE_PKT_LEN_GET(val, member) \
	(((val) >> RQ_CQE_PKT_##member##_SHIFT) & RQ_CQE_PKT_##member##_MASK)

#define RQ_CQE_DECRY_INFO_DECRY_STATUS_SHIFT  8
#define RQ_CQE_DECRY_INFO_ESP_NEXT_HEAD_SHIFT 0

#define RQ_CQE_DECRY_INFO_DECRY_STATUS_MASK  0xFFU
#define RQ_CQE_DECRY_INFO_ESP_NEXT_HEAD_MASK 0xFFU

#define RQ_CQE_DECRY_INFO_GET(val, member)               \
	(((val) >> RQ_CQE_DECRY_INFO_##member##_SHIFT) & \
	 RQ_CQE_DECRY_INFO_##member##_MASK)

#define HINIC3_GET_DECRYPT_STATUS(decry_info) \
	RQ_CQE_DECRY_INFO_GET(decry_info, DECRY_STATUS)

#define HINIC3_GET_ESP_NEXT_HEAD(decry_info) \
	RQ_CQE_DECRY_INFO_GET(decry_info, ESP_NEXT_HEAD)

/* Rx cqe checksum err */
#define HINIC3_RX_CSUM_IP_CSUM_ERR      BIT(0)
#define HINIC3_RX_CSUM_TCP_CSUM_ERR     BIT(1)
#define HINIC3_RX_CSUM_UDP_CSUM_ERR     BIT(2)
#define HINIC3_RX_CSUM_IGMP_CSUM_ERR    BIT(3)
#define HINIC3_RX_CSUM_ICMP_V4_CSUM_ERR BIT(4)
#define HINIC3_RX_CSUM_ICMP_V6_CSUM_ERR BIT(5)
#define HINIC3_RX_CSUM_SCTP_CRC_ERR     BIT(6)
#define HINIC3_RX_CSUM_HW_CHECK_NONE    BIT(7)
#define HINIC3_RX_CSUM_IPSU_OTHER_ERR   BIT(8)

#define HINIC3_DEFAULT_RX_CSUM_OFFLOAD 0xFFF
#define HINIC3_CQE_LEN		       32

#define HINIC3_RSS_OFFLOAD_ALL (         \
	RTE_ETH_RSS_IPV4 |               \
	RTE_ETH_RSS_FRAG_IPV4 |          \
	RTE_ETH_RSS_NONFRAG_IPV4_TCP |   \
	RTE_ETH_RSS_NONFRAG_IPV4_UDP |   \
	RTE_ETH_RSS_NONFRAG_IPV4_OTHER | \
	RTE_ETH_RSS_IPV6 |               \
	RTE_ETH_RSS_FRAG_IPV6 |          \
	RTE_ETH_RSS_NONFRAG_IPV6_TCP |   \
	RTE_ETH_RSS_NONFRAG_IPV6_UDP |   \
	RTE_ETH_RSS_NONFRAG_IPV6_OTHER | \
	RTE_ETH_RSS_IPV6_EX |            \
	RTE_ETH_RSS_IPV6_TCP_EX |        \
	RTE_ETH_RSS_IPV6_UDP_EX)

struct hinic3_rxq_stats {
	u64 packets;
	u64 bytes;
	u64 errors;
	u64 csum_errors;
	u64 other_errors;
	u64 unlock_bp;
	u64 dropped;

	u64 rx_nombuf;
	u64 rx_discards;
	u64 burst_pkts;
	u64 empty;
	u64 tsc;
#ifdef HINIC3_XSTAT_MBUF_USE
	u64 rx_alloc_mbuf_bytes;
	u64 rx_free_mbuf_bytes;
	u64 rx_left_mbuf_bytes;
#endif

#ifdef HINIC3_XSTAT_RXBUF_INFO
	u64 rx_mbuf;
	u64 rx_avail;
	u64 rx_hole;
#endif

#ifdef HINIC3_XSTAT_PROF_RX
	u64 app_tsc;
	u64 pmd_tsc;
#endif
};

struct __rte_cache_aligned hinic3_rq_cqe {
	RTE_ATOMIC(u32) status;
	u32 vlan_len;

	u32 offload_type;
	u32 hash_val;
	u32 mark_id_0;
	u32 mark_id_1;
	u32 mark_id_2;
	u32 pkt_info;
};

/**
 * Attention: please do not add any member in hinic3_rx_info
 * because rxq bulk rearm mode will write mbuf in rx_info.
 */
struct hinic3_rx_info {
	struct rte_mbuf *mbuf;
};

struct hinic3_sge_sect {
	struct hinic3_sge sge;
	u32 rsvd;
};

struct hinic3_rq_extend_wqe {
	struct hinic3_sge_sect buf_desc;
	struct hinic3_sge_sect cqe_sect;
};

struct hinic3_rq_normal_wqe {
	u32 buf_hi_addr;
	u32 buf_lo_addr;
	u32 cqe_hi_addr;
	u32 cqe_lo_addr;
};

struct hinic3_rq_wqe {
	union {
		struct hinic3_rq_normal_wqe normal_wqe;
		struct hinic3_rq_extend_wqe extend_wqe;
	};
};

struct __rte_cache_aligned hinic3_rxq {
	struct hinic3_nic_dev *nic_dev;

	u16 q_id;
	u16 q_depth;
	u16 q_mask;
	u16 buf_len;

	u32 rx_buff_shift;

	u16 rx_free_thresh;
	u16 rxinfo_align_end;
	u16 wqebb_shift;
	u16 wqebb_size;

	u16 wqe_type;
	u16 cons_idx;
	u16 prod_idx;
	u16 delta;

	u16 next_to_update;
	u16 port_id;

	const struct rte_memzone *rq_mz;
	void *queue_buf_vaddr; /**< rxq dma info */
	rte_iova_t queue_buf_paddr;

	const struct rte_memzone *pi_mz;
	u16 *pi_virt_addr;
	void *db_addr;
	rte_iova_t pi_dma_addr;

	struct hinic3_rx_info *rx_info;
	struct hinic3_rq_cqe *rx_cqe;
	struct rte_mempool *mb_pool;

	const struct rte_memzone *cqe_mz;
	rte_iova_t cqe_start_paddr;
	void *cqe_start_vaddr;
	u8 dp_intr_en;
	u16 msix_entry_idx;

	unsigned long status;
	u64 wait_time_cycle;

	struct hinic3_rxq_stats rxq_stats;
#ifdef HINIC3_XSTAT_PROF_RX
	uint64_t prof_rx_end_tsc; /**< Performance profiling. */
#endif
};

u16 hinic3_rx_fill_wqe(struct hinic3_rxq *rxq);

u16 hinic3_rx_fill_buffers(struct hinic3_rxq *rxq);

void hinic3_free_rxq_mbufs(struct hinic3_rxq *rxq);

void hinic3_free_all_rxq_mbufs(struct hinic3_nic_dev *nic_dev);

int hinic3_update_rss_config(struct rte_eth_dev *dev,
			     struct rte_eth_rss_conf *rss_conf);

int hinic3_poll_rq_empty(struct hinic3_rxq *rxq);

void hinic3_dump_cqe_status(struct hinic3_rxq *rxq, u32 *cqe_done_cnt,
			    u32 *cqe_hole_cnt, u32 *head_ci, u32 *head_done);

int hinic3_stop_rq(struct rte_eth_dev *eth_dev, struct hinic3_rxq *rxq);

int hinic3_start_rq(struct rte_eth_dev *eth_dev, struct hinic3_rxq *rxq);

u16 hinic3_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, u16 nb_pkts);

void hinic3_add_rq_to_rx_queue_list(struct hinic3_nic_dev *nic_dev,
				    u16 queue_id);

int hinic3_refill_indir_rqid(struct hinic3_rxq *rxq);

void hinic3_init_rx_queue_list(struct hinic3_nic_dev *nic_dev);

void hinic3_remove_rq_from_rx_queue_list(struct hinic3_nic_dev *nic_dev,
					 u16 queue_id);
int hinic3_start_all_rqs(struct rte_eth_dev *eth_dev);

#ifdef HINIC3_XSTAT_RXBUF_INFO
void hinic3_get_stats(struct hinic3_rxq *rxq);
#endif

/**
 * Get receive queue local ci.
 *
 * @param[in] rxq
 * Pointer to receive queue structure.
 * @return
 * Receive queue local ci.
 */
static inline u16
hinic3_get_rq_local_ci(struct hinic3_rxq *rxq)
{
	return MASKED_QUEUE_IDX(rxq, rxq->cons_idx);
}

static inline u16
hinic3_get_rq_free_wqebb(struct hinic3_rxq *rxq)
{
	return rxq->delta - 1;
}

/**
 * Update receive queue local ci.
 *
 * @param[in] rxq
 * Pointer to receive queue structure.
 * @param[out] wqe_cnt
 * Wqebb counters.
 */
static inline void
hinic3_update_rq_local_ci(struct hinic3_rxq *rxq, u16 wqe_cnt)
{
	rxq->cons_idx += wqe_cnt;
	rxq->delta += wqe_cnt;
}

#endif /* _HINIC3_RX_H_ */
