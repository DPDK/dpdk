/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_QUEUE_H
#define SXE2_QUEUE_H

#include <rte_ethdev.h>
#include <rte_io.h>
#include <ethdev_driver.h>

#include "sxe2_drv_cmd.h"
#include "sxe2_txrx_common.h"

#define SXE2_PCI_REG_READ(reg)			\
		rte_read32(reg)
#define SXE2_PCI_REG_WRITE_WC(reg, value)			\
		rte_write32_wc((rte_cpu_to_le_32(value)), reg)
#define SXE2_PCI_REG_WRITE_WC_RELAXED(reg, value)		\
		rte_write32_wc_relaxed((rte_cpu_to_le_32(value)), reg)

struct sxe2_queue_context {
	uint16_t qp_cnt_assign;
	uint16_t base_idx_in_pf;

	uint32_t tx_mode_flags;
	uint32_t rx_mode_flags;
};

struct sxe2_tx_buffer {
	struct rte_mbuf *mbuf;

	uint16_t next_id;
	uint16_t last_id;
};

struct sxe2_tx_buffer_vec {
	struct rte_mbuf *mbuf;
};

struct sxe2_txq_stats {
	uint64_t tx_restart;
	uint64_t tx_busy;

	uint64_t tx_linearize;
	uint64_t tx_tso_linearize_chk;
	uint64_t tx_vlan_insert;
	uint64_t tx_tso_packets;
	uint64_t tx_tso_bytes;
	uint64_t tx_csum_none;
	uint64_t tx_csum_partial;
	uint64_t tx_csum_partial_inner;
	uint64_t tx_queue_dropped;
	uint64_t tx_xmit_more;
	uint64_t tx_pkts_num;
	uint64_t tx_desc_not_done;
};

struct sxe2_tx_queue;
struct sxe2_txq_ops {
	void (*queue_reset)(struct sxe2_tx_queue *txq);
	void (*mbufs_release)(struct sxe2_tx_queue *txq);
	void (*buffer_ring_free)(struct sxe2_tx_queue *txq);
};
struct sxe2_tx_queue {
	volatile union sxe2_tx_data_desc *desc_ring;
	struct sxe2_tx_buffer *buffer_ring;
	volatile uint32_t *tdt_reg_addr;

	uint64_t offloads;
	uint16_t ring_depth;
	uint16_t desc_free_num;

	uint16_t free_thresh;

	uint16_t rs_thresh;
	uint16_t next_use;
	uint16_t next_clean;

	uint16_t desc_used_num;
	uint16_t next_dd;
	uint16_t next_rs;
	uint16_t ipsec_pkt_md_offset;

	uint16_t port_id;
	uint16_t queue_id;
	uint16_t idx_in_func;
	bool tx_deferred_start;
	uint8_t pthresh;
	uint8_t hthresh;
	uint8_t wthresh;
	uint16_t reg_idx;
	uint64_t base_addr;
	struct sxe2_vsi *vsi;
	const struct rte_memzone *mz;
	struct sxe2_txq_ops ops;
	uint8_t  vlan_flag;
	uint8_t  use_ctx:1,
		res:7;
};
struct sxe2_rx_queue;
struct sxe2_rxq_ops {
	void (*queue_reset)(struct sxe2_rx_queue *rxq);
	void (*mbufs_release)(struct sxe2_rx_queue *txq);
};
struct sxe2_rxq_stats {
	uint64_t rx_pkts_num;
	uint64_t rx_rss_pkt_num;
	uint64_t rx_fnav_pkt_num;
	uint64_t rx_ptp_pkt_num;
	uint32_t rx_vec_align_drop;

	uint32_t rxdid_1588_err;
	uint32_t ip_csum_err;
	uint32_t l4_csum_err;
	uint32_t outer_ip_csum_err;
	uint32_t outer_l4_csum_err;
	uint32_t macsec_err;
	uint32_t ipsec_err;

	uint64_t ptype_pkts[SXE2_MAX_PTYPE_NUM];
};

struct sxe2_rxq_sw_stats {
	uint64_t pkts;
	uint64_t bytes;
	uint64_t drop_pkts;
	uint64_t drop_bytes;
	uint64_t unicast_pkts;
	uint64_t multicast_pkts;
	uint64_t broadcast_pkts;
};

struct sxe2_rx_queue {
	volatile union sxe2_rx_desc *desc_ring;
	volatile uint32_t *rdt_reg_addr;
	struct rte_mempool *mb_pool;
	struct rte_mbuf **buffer_ring;
	struct sxe2_vsi *vsi;

	uint64_t offloads;
	uint16_t ring_depth;
	uint16_t rx_free_thresh;
	uint16_t processing_idx;
	uint16_t hold_num;
	uint16_t next_ret_pkt;
	uint16_t batch_alloc_trigger;
	uint16_t completed_pkts_num;
	uint64_t update_time;
	uint32_t desc_ts;
	uint64_t ts_high;
	uint32_t ts_low;
	uint32_t ts_need_update;
	uint8_t  crc_len;
	bool fnav_enable;

	struct rte_eth_rxseg_split rx_seg[SXE2_RX_SEG_NUM];

	struct rte_mbuf *completed_buf[SXE2_RX_PKTS_BURST_BATCH_NUM * 2];
	struct rte_mbuf *pkt_first_seg;
	struct rte_mbuf *pkt_last_seg;
	uint64_t mbuf_init_value;
	uint16_t realloc_num;
	uint16_t realloc_start;
	struct rte_mbuf fake_mbuf;

	const struct rte_memzone *mz;
	struct sxe2_rxq_ops ops;
	rte_iova_t base_addr;
	uint16_t reg_idx;
	uint32_t low_desc_waterline : 16;
	uint32_t ldw_event_pending : 1;
	struct sxe2_rxq_sw_stats sw_stats;
	uint16_t port_id;
	uint16_t queue_id;
	uint16_t idx_in_func;
	uint16_t rx_buf_len;
	uint16_t rx_hdr_len;
	uint16_t max_pkt_len;
	bool rx_deferred_start;
	uint8_t drop_en;
};

struct sxe2_adapter;

void sxe2_sw_queue_ctx_hw_cap_set(struct sxe2_adapter *adapter,
		struct sxe2_drv_queue_caps *q_caps);

int32_t sxe2_queues_init(struct rte_eth_dev *dev);

int32_t sxe2_queues_start(struct rte_eth_dev *dev);

void sxe2_queues_release(struct rte_eth_dev *dev);

#endif /* SXE2_QUEUE_H */
