/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2020 Amazon.com, Inc. or its affiliates.
 * All rights reserved.
 */

#ifndef _ENA_ETHDEV_H_
#define _ENA_ETHDEV_H_

#include <rte_atomic.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <rte_cycles.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_timer.h>
#include <rte_dev.h>
#include <rte_net.h>

#include "ena_com.h"

#define ENA_REGS_BAR	0
#define ENA_MEM_BAR	2

#define ENA_MAX_NUM_QUEUES	128
#define ENA_MIN_FRAME_LEN	64
#define ENA_NAME_MAX_LEN	20
#define ENA_PKT_MAX_BUFS	17
#define ENA_RX_BUF_MIN_SIZE	1400
#define ENA_DEFAULT_RING_SIZE	1024

#define ENA_MIN_MTU		128

#define ENA_MMIO_DISABLE_REG_READ	BIT(0)

#define ENA_WD_TIMEOUT_SEC	3
#define ENA_DEVICE_KALIVE_TIMEOUT (ENA_WD_TIMEOUT_SEC * rte_get_timer_hz())

#define ENA_TX_TIMEOUT			(5 * rte_get_timer_hz())
#define ENA_MONITORED_TX_QUEUES		3
#define ENA_DEFAULT_MISSING_COMP	256U

/* While processing submitted and completed descriptors (rx and tx path
 * respectively) in a loop it is desired to:
 *  - perform batch submissions while populating submission queue
 *  - avoid blocking transmission of other packets during cleanup phase
 * Hence the utilization ratio of 1/8 of a queue size or max value if the size
 * of the ring is very big - like 8k Rx rings.
 */
#define ENA_REFILL_THRESH_DIVIDER      8
#define ENA_REFILL_THRESH_PACKET       256

#define ENA_IDX_NEXT_MASKED(idx, mask) (((idx) + 1) & (mask))
#define ENA_IDX_ADD_MASKED(idx, n, mask) (((idx) + (n)) & (mask))

#define ENA_RX_RSS_TABLE_LOG_SIZE	7
#define ENA_RX_RSS_TABLE_SIZE		(1 << ENA_RX_RSS_TABLE_LOG_SIZE)

#define ENA_HASH_KEY_SIZE		40

#define ENA_ALL_RSS_HF (RTE_ETH_RSS_NONFRAG_IPV4_TCP | RTE_ETH_RSS_NONFRAG_IPV4_UDP | \
			RTE_ETH_RSS_NONFRAG_IPV6_TCP | RTE_ETH_RSS_NONFRAG_IPV6_UDP)

#define ENA_IO_TXQ_IDX(q)		(2 * (q))
#define ENA_IO_RXQ_IDX(q)		(2 * (q) + 1)
/* Reversed version of ENA_IO_RXQ_IDX */
#define ENA_IO_RXQ_IDX_REV(q)		(((q) - 1) / 2)

extern struct ena_shared_data *ena_shared_data;

struct ena_adapter;

enum ena_ring_type {
	ENA_RING_TYPE_RX = 1,
	ENA_RING_TYPE_TX = 2,
};

struct ena_tx_buffer {
	struct rte_mbuf *mbuf;
	unsigned int tx_descs;
	unsigned int num_of_bufs;
	uint64_t timestamp;
	bool print_once;
	struct ena_com_buf bufs[ENA_PKT_MAX_BUFS];
};

/* Rx buffer holds only pointer to the mbuf - may be expanded in the future */
struct ena_rx_buffer {
	struct rte_mbuf *mbuf;
	struct ena_com_buf ena_buf;
};

struct ena_calc_queue_size_ctx {
	struct ena_com_dev_get_features_ctx *get_feat_ctx;
	struct ena_com_dev *ena_dev;
	u32 max_rx_queue_size;
	u32 max_tx_queue_size;
	u16 max_tx_sgl_size;
	u16 max_rx_sgl_size;
};

struct ena_stats_tx {
	u64 cnt;
	u64 bytes;
	u64 prepare_ctx_err;
	u64 linearize;
	u64 linearize_failed;
	u64 tx_poll;
	u64 doorbells;
	u64 bad_req_id;
	u64 available_desc;
	u64 missed_tx;
};

struct ena_stats_rx {
	u64 cnt;
	u64 bytes;
	u64 refill_partial;
	u64 bad_csum;
	u64 mbuf_alloc_fail;
	u64 bad_desc_num;
	u64 bad_req_id;
};

struct ena_ring {
	u16 next_to_use;
	u16 next_to_clean;
	uint64_t last_cleanup_ticks;

	enum ena_ring_type type;
	enum ena_admin_placement_policy_type tx_mem_queue_type;

	/* Indicate there are Tx packets pushed to the device and wait for db */
	bool pkts_without_db;

	/* Holds the empty requests for TX/RX OOO completions */
	union {
		uint16_t *empty_tx_reqs;
		uint16_t *empty_rx_reqs;
	};

	union {
		struct ena_tx_buffer *tx_buffer_info; /* contex of tx packet */
		struct ena_rx_buffer *rx_buffer_info; /* contex of rx packet */
	};
	struct rte_mbuf **rx_refill_buffer;
	unsigned int ring_size; /* number of tx/rx_buffer_info's entries */
	unsigned int size_mask;

	struct ena_com_io_cq *ena_com_io_cq;
	struct ena_com_io_sq *ena_com_io_sq;

	union {
		uint16_t tx_free_thresh;
		uint16_t rx_free_thresh;
	};

	struct ena_com_rx_buf_info ena_bufs[ENA_PKT_MAX_BUFS]
						__rte_cache_aligned;

	struct rte_mempool *mb_pool;
	unsigned int port_id;
	unsigned int id;
	/* Max length PMD can push to device for LLQ */
	uint8_t tx_max_header_size;
	int configured;

	uint8_t *push_buf_intermediate_buf;

	struct ena_adapter *adapter;
	uint64_t offloads;
	u16 sgl_size;

	bool disable_meta_caching;

	union {
		struct ena_stats_rx rx_stats;
		struct ena_stats_tx tx_stats;
	};

	unsigned int numa_socket_id;

	uint32_t missing_tx_completion_threshold;
} __rte_cache_aligned;

enum ena_adapter_state {
	ENA_ADAPTER_STATE_FREE    = 0,
	ENA_ADAPTER_STATE_INIT    = 1,
	ENA_ADAPTER_STATE_RUNNING = 2,
	ENA_ADAPTER_STATE_STOPPED = 3,
	ENA_ADAPTER_STATE_CONFIG  = 4,
	ENA_ADAPTER_STATE_CLOSED  = 5,
};

struct ena_driver_stats {
	rte_atomic64_t ierrors;
	rte_atomic64_t oerrors;
	rte_atomic64_t rx_nombuf;
	u64 rx_drops;
};

struct ena_stats_dev {
	u64 wd_expired;
	u64 dev_start;
	u64 dev_stop;
	/*
	 * Tx drops cannot be reported as the driver statistic, because DPDK
	 * rte_eth_stats structure isn't providing appropriate field for that.
	 * As a workaround it is being published as an extended statistic.
	 */
	u64 tx_drops;
};

struct ena_stats_eni {
	/*
	 * The number of packets shaped due to inbound aggregate BW
	 * allowance being exceeded
	 */
	uint64_t bw_in_allowance_exceeded;
	/*
	 * The number of packets shaped due to outbound aggregate BW
	 * allowance being exceeded
	 */
	uint64_t bw_out_allowance_exceeded;
	/* The number of packets shaped due to PPS allowance being exceeded */
	uint64_t pps_allowance_exceeded;
	/*
	 * The number of packets shaped due to connection tracking
	 * allowance being exceeded and leading to failure in establishment
	 * of new connections
	 */
	uint64_t conntrack_allowance_exceeded;
	/*
	 * The number of packets shaped due to linklocal packet rate
	 * allowance being exceeded
	 */
	uint64_t linklocal_allowance_exceeded;
};

struct ena_offloads {
	uint32_t tx_offloads;
	uint32_t rx_offloads;
};

/* board specific private data structure */
struct ena_adapter {
	/* OS defined structs */
	struct rte_eth_dev_data *edev_data;

	struct ena_com_dev ena_dev __rte_cache_aligned;

	/* TX */
	struct ena_ring tx_ring[ENA_MAX_NUM_QUEUES] __rte_cache_aligned;
	u32 max_tx_ring_size;
	u16 max_tx_sgl_size;

	/* RX */
	struct ena_ring rx_ring[ENA_MAX_NUM_QUEUES] __rte_cache_aligned;
	u32 max_rx_ring_size;
	u16 max_rx_sgl_size;

	u32 max_num_io_queues;
	u16 max_mtu;
	struct ena_offloads offloads;

	/* The admin queue isn't protected by the lock and is used to
	 * retrieve statistics from the device. As there is no guarantee that
	 * application won't try to get statistics from multiple threads, it is
	 * safer to lock the queue to avoid admin queue failure.
	 */
	rte_spinlock_t admin_lock;

	int id_number;
	char name[ENA_NAME_MAX_LEN];
	u8 mac_addr[RTE_ETHER_ADDR_LEN];

	void *regs;
	void *dev_mem_base;

	struct ena_driver_stats *drv_stats;
	enum ena_adapter_state state;

	bool link_status;

	enum ena_regs_reset_reason_types reset_reason;

	struct rte_timer timer_wd;
	uint64_t timestamp_wd;
	uint64_t keep_alive_timeout;

	struct ena_stats_dev dev_stats;
	struct ena_stats_eni eni_stats;

	bool trigger_reset;

	bool wd_state;

	bool use_large_llq_hdr;

	uint32_t last_tx_comp_qid;
	uint64_t missing_tx_completion_to;
	uint64_t missing_tx_completion_budget;
	uint64_t tx_cleanup_stall_delay;
};

int ena_rss_reta_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size);
int ena_rss_reta_query(struct rte_eth_dev *dev,
		       struct rte_eth_rss_reta_entry64 *reta_conf,
		       uint16_t reta_size);
int ena_rss_hash_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_conf *rss_conf);
int ena_rss_hash_conf_get(struct rte_eth_dev *dev,
			  struct rte_eth_rss_conf *rss_conf);
int ena_rss_configure(struct ena_adapter *adapter);

#endif /* _ENA_ETHDEV_H_ */
