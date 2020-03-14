/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC_PMD_RX_H_
#define _HINIC_PMD_RX_H_

#define HINIC_DEFAULT_RX_FREE_THRESH	32

#define HINIC_RSS_OFFLOAD_ALL ( \
	ETH_RSS_IPV4 | \
	ETH_RSS_FRAG_IPV4 |\
	ETH_RSS_NONFRAG_IPV4_TCP | \
	ETH_RSS_NONFRAG_IPV4_UDP | \
	ETH_RSS_IPV6 | \
	ETH_RSS_FRAG_IPV6 | \
	ETH_RSS_NONFRAG_IPV6_TCP | \
	ETH_RSS_NONFRAG_IPV6_UDP | \
	ETH_RSS_IPV6_EX | \
	ETH_RSS_IPV6_TCP_EX | \
	ETH_RSS_IPV6_UDP_EX)

enum rq_completion_fmt {
	RQ_COMPLETE_SGE = 1
};

struct hinic_rq_ctrl {
	u32	ctrl_fmt;
};

struct hinic_rq_cqe {
	u32 status;
	u32 vlan_len;
	u32 offload_type;
	u32 rss_hash;

	u32 rsvd[4];
} __rte_cache_aligned;

struct hinic_rq_cqe_sect {
	struct hinic_sge	sge;
	u32			rsvd;
};

struct hinic_rq_bufdesc {
	u32	addr_high;
	u32	addr_low;
};

struct hinic_rq_wqe {
	struct hinic_rq_ctrl		ctrl;
	u32				rsvd;
	struct hinic_rq_cqe_sect	cqe_sect;
	struct hinic_rq_bufdesc		buf_desc;
};

struct hinic_rxq_stats {
	u64 packets;
	u64 bytes;
	u64 rx_nombuf;
	u64 errors;
	u64 rx_discards;
	u64 burst_pkts;
};

/* Attention, Do not add any member in hinic_rx_info
 * as rxq bulk rearm mode will write mbuf in rx_info
 */
struct hinic_rx_info {
	struct rte_mbuf *mbuf;
};

struct hinic_rxq {
	struct hinic_wq *wq;
	volatile u16 *pi_virt_addr;

	u16 port_id;
	u16 q_id;
	u16 q_depth;
	u16 buf_len;

	u16 rx_free_thresh;
	u16 rxinfo_align_end;

	u32 socket_id;

	unsigned long status;
	struct hinic_rxq_stats rxq_stats;

	struct hinic_nic_dev *nic_dev;

	struct hinic_rx_info	*rx_info;
	volatile struct hinic_rq_cqe *rx_cqe;

	dma_addr_t cqe_start_paddr;
	void *cqe_start_vaddr;
	struct rte_mempool *mb_pool;
};

int hinic_setup_rx_resources(struct hinic_rxq *rxq);

void hinic_free_all_rx_resources(struct rte_eth_dev *eth_dev);

void hinic_free_all_rx_mbuf(struct rte_eth_dev *eth_dev);

void hinic_free_rx_resources(struct hinic_rxq *rxq);

u16 hinic_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, u16 nb_pkts);

void hinic_free_all_rx_mbufs(struct hinic_rxq *rxq);

void hinic_rx_alloc_pkts(struct hinic_rxq *rxq);

void hinic_rxq_get_stats(struct hinic_rxq *rxq, struct hinic_rxq_stats *stats);

void hinic_rxq_stats_reset(struct hinic_rxq *rxq);

int hinic_config_mq_mode(struct rte_eth_dev *dev, bool on);

int hinic_rx_configure(struct rte_eth_dev *dev);

void hinic_rx_remove_configure(struct rte_eth_dev *dev);

void hinic_get_func_rx_buf_size(struct hinic_nic_dev *nic_dev);

int hinic_create_rq(struct hinic_hwdev *hwdev, u16 q_id,
			u16 rq_depth, unsigned int socket_id);

void hinic_destroy_rq(struct hinic_hwdev *hwdev, u16 q_id);

#endif /* _HINIC_PMD_RX_H_ */
