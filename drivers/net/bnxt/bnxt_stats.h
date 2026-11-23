/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_STATS_H_
#define _BNXT_STATS_H_

#include <ethdev_driver.h>

/* Number of software (non-HWRM) xstats appended after the FW-reported ones. */
#define BNXT_NUM_SW_XSTATS		2

#define BNXT_RING_STATS_CNT		20ULL
#define BNXT_RING_STATS_EXT_CNT		22ULL
#define BNXT_STAT_CONSEC_LOW_MAX	2
#define BNXT_DMA_STATS_UPDATE_PERIOD	250

struct bnxt_ring_stats {
	/* Number of transmitted unicast packets */
	uint64_t	tx_ucast_pkts;
	/* Number of transmitted multicast packets */
	uint64_t	tx_mcast_pkts;
	/* Number of transmitted broadcast packets */
	uint64_t	tx_bcast_pkts;
	/* Number of packets discarded in transmit path */
	uint64_t	tx_discard_pkts;
	/* Number of packets in transmit path with error */
	uint64_t	tx_error_pkts;
	/* Number of transmitted bytes for unicast traffic */
	uint64_t	tx_ucast_bytes;
	/* Number of transmitted bytes for multicast traffic */
	uint64_t	tx_mcast_bytes;
	/* Number of transmitted bytes for broadcast traffic */
	uint64_t	tx_bcast_bytes;
	/* Number of received unicast packets */
	uint64_t	rx_ucast_pkts;
	/* Number of received multicast packets */
	uint64_t	rx_mcast_pkts;
	/* Number of received broadcast packets */
	uint64_t	rx_bcast_pkts;
	/* Number of packets discarded in receive path */
	uint64_t	rx_discard_pkts;
	/* Number of packets in receive path with errors */
	uint64_t	rx_error_pkts;
	/* Number of received bytes for unicast traffic */
	uint64_t	rx_ucast_bytes;
	/* Number of received bytes for multicast traffic */
	uint64_t	rx_mcast_bytes;
	/* Number of received bytes for broadcast traffic */
	uint64_t	rx_bcast_bytes;
	/* Number of aggregated unicast packets */
	uint64_t	rx_agg_pkts;
	/* Number of aggregated unicast bytes */
	uint64_t	rx_agg_bytes;
	/* Number of aggregation events */
	uint64_t	rx_agg_events;
	/* Number of aborted aggregations */
	uint64_t	rx_agg_aborts;
};

struct bnxt_ring_stats_ext {
	/* Number of received unicast packets */
	uint64_t	rx_ucast_pkts;
	/* Number of received multicast packets */
	uint64_t	rx_mcast_pkts;
	/* Number of received broadcast packets */
	uint64_t	rx_bcast_pkts;
	/* Number of discarded packets on receive path */
	uint64_t	rx_discard_pkts;
	/* Number of packets on receive path with error */
	uint64_t	rx_error_pkts;
	/* Number of received bytes for unicast traffic */
	uint64_t	rx_ucast_bytes;
	/* Number of received bytes for multicast traffic */
	uint64_t	rx_mcast_bytes;
	/* Number of received bytes for broadcast traffic */
	uint64_t	rx_bcast_bytes;
	/* Number of transmitted unicast packets */
	uint64_t	tx_ucast_pkts;
	/* Number of transmitted multicast packets */
	uint64_t	tx_mcast_pkts;
	/* Number of transmitted broadcast packets */
	uint64_t	tx_bcast_pkts;
	/* Number of packets on transmit path with error */
	uint64_t	tx_error_pkts;
	/* Number of discarded packets on transmit path */
	uint64_t	tx_discard_pkts;
	/* Number of transmitted bytes for unicast traffic */
	uint64_t	tx_ucast_bytes;
	/* Number of transmitted bytes for multicast traffic */
	uint64_t	tx_mcast_bytes;
	/* Number of transmitted bytes for broadcast traffic */
	uint64_t	tx_bcast_bytes;
	/* Number of TPA eligible packets */
	uint64_t	rx_tpa_eligible_pkt;
	/* Number of TPA eligible bytes */
	uint64_t	rx_tpa_eligible_bytes;
	/* Number of TPA packets */
	uint64_t	rx_tpa_pkt;
	/* Number of TPA bytes */
	uint64_t	rx_tpa_bytes;
	/* Number of TPA errors */
	uint64_t	rx_tpa_errors;
	/* Number of TPA events. Not used for P5*/
	uint64_t	rx_tpa_events;
};


struct bnxt_stat_offset_table {
	uint16_t dst_off;
	uint16_t src_off;
	bool     is_rx;
};

#define BNXT_STAT_OFFSET(dst_field, src_field, rxf)			\
	{ offsetof(struct bnxt_ring_stats, dst_field),			\
	  offsetof(struct ctx_hw_stats, src_field), rxf }

#define BNXT_EXT_OFFSET(dst_field, src_field, rxf)			\
	{ offsetof(struct bnxt_ring_stats_ext, dst_field),		\
	  offsetof(struct bnxt_ring_stats_ext, src_field), rxf }

static const struct bnxt_stat_offset_table bnxt_ring_stat_offsets[] = {
	BNXT_STAT_OFFSET(rx_ucast_pkts,		rx_ucast_pkts,		true),
	BNXT_STAT_OFFSET(rx_mcast_pkts,		rx_mcast_pkts,		true),
	BNXT_STAT_OFFSET(rx_bcast_pkts,		rx_bcast_pkts,		true),
	BNXT_STAT_OFFSET(rx_ucast_bytes,	rx_ucast_bytes,		true),
	BNXT_STAT_OFFSET(rx_mcast_bytes,	rx_mcast_bytes,		true),
	BNXT_STAT_OFFSET(rx_bcast_bytes,	rx_bcast_bytes,		true),
	BNXT_STAT_OFFSET(rx_discard_pkts,	rx_discard_pkts,	true),
	BNXT_STAT_OFFSET(rx_error_pkts,		rx_error_pkts,		true),
	BNXT_STAT_OFFSET(rx_agg_pkts,		tpa_pkts,		true),
	BNXT_STAT_OFFSET(rx_agg_bytes,		tpa_bytes,		true),
	BNXT_STAT_OFFSET(rx_agg_events,		tpa_events,		true),
	BNXT_STAT_OFFSET(rx_agg_aborts,		tpa_aborts,		true),
	BNXT_STAT_OFFSET(tx_ucast_pkts,		tx_ucast_pkts,		false),
	BNXT_STAT_OFFSET(tx_mcast_pkts,		tx_mcast_pkts,		false),
	BNXT_STAT_OFFSET(tx_bcast_pkts,		tx_bcast_pkts,		false),
	BNXT_STAT_OFFSET(tx_ucast_bytes,	tx_ucast_bytes,		false),
	BNXT_STAT_OFFSET(tx_mcast_bytes,	tx_mcast_bytes,		false),
	BNXT_STAT_OFFSET(tx_bcast_bytes,	tx_bcast_bytes,		false),
	BNXT_STAT_OFFSET(tx_discard_pkts,	tx_discard_pkts,	false),
	BNXT_STAT_OFFSET(tx_error_pkts,		tx_error_pkts,		false),
};

static const struct bnxt_stat_offset_table bnxt_ring_stat_ext_offsets[] = {
	BNXT_EXT_OFFSET(rx_ucast_pkts,		rx_ucast_pkts,		true),
	BNXT_EXT_OFFSET(rx_mcast_pkts,		rx_mcast_pkts,		true),
	BNXT_EXT_OFFSET(rx_bcast_pkts,		rx_bcast_pkts,		true),
	BNXT_EXT_OFFSET(rx_ucast_bytes,		rx_ucast_bytes,		true),
	BNXT_EXT_OFFSET(rx_mcast_bytes,		rx_mcast_bytes,		true),
	BNXT_EXT_OFFSET(rx_bcast_bytes,		rx_bcast_bytes,		true),
	BNXT_EXT_OFFSET(rx_discard_pkts,	rx_discard_pkts,	true),
	BNXT_EXT_OFFSET(rx_error_pkts,		rx_error_pkts,		true),
	BNXT_EXT_OFFSET(rx_tpa_eligible_pkt,	rx_tpa_eligible_pkt,	true),
	BNXT_EXT_OFFSET(rx_tpa_eligible_bytes,	rx_tpa_eligible_bytes,	true),
	BNXT_EXT_OFFSET(rx_tpa_pkt,		rx_tpa_pkt,		true),
	BNXT_EXT_OFFSET(rx_tpa_bytes,		rx_tpa_bytes,		true),
	BNXT_EXT_OFFSET(rx_tpa_errors,		rx_tpa_errors,		true),
	BNXT_EXT_OFFSET(rx_tpa_events,		rx_tpa_events,		true),
	BNXT_EXT_OFFSET(tx_ucast_pkts,		tx_ucast_pkts,		false),
	BNXT_EXT_OFFSET(tx_mcast_pkts,		tx_mcast_pkts,		false),
	BNXT_EXT_OFFSET(tx_bcast_pkts,		tx_bcast_pkts,		false),
	BNXT_EXT_OFFSET(tx_ucast_bytes,		tx_ucast_bytes,		false),
	BNXT_EXT_OFFSET(tx_mcast_bytes,		tx_mcast_bytes,		false),
	BNXT_EXT_OFFSET(tx_bcast_bytes,		tx_bcast_bytes,		false),
	BNXT_EXT_OFFSET(tx_discard_pkts,	tx_discard_pkts,	false),
	BNXT_EXT_OFFSET(tx_error_pkts,		tx_error_pkts,		false),
};

void bnxt_free_stats(struct bnxt *bp);
int bnxt_stats_get_op(struct rte_eth_dev *eth_dev,
			   struct rte_eth_stats *bnxt_stats, struct eth_queue_stats *qstats);
int bnxt_stats_reset_op(struct rte_eth_dev *eth_dev);
int bnxt_dev_xstats_get_names_op(struct rte_eth_dev *eth_dev,
	struct rte_eth_xstat_name *xstats_names,
	__rte_unused unsigned int limit);
int bnxt_dev_xstats_get_op(struct rte_eth_dev *eth_dev,
			   struct rte_eth_xstat *xstats, unsigned int n);
int bnxt_dev_xstats_reset_op(struct rte_eth_dev *eth_dev);
void bnxt_masks_fill(struct bnxt *bp, uint64_t *dst,
		     const uint64_t *src, uint32_t count);
int bnxt_apply_stats_masks(uint64_t *dst, const uint64_t *src,
			   const uint64_t *masks, uint32_t count);
int bnxt_get_ring_stats_masks(struct bnxt *bp, struct bnxt_cp_ring_info *cpr);
void bnxt_clear_ring_stats_masks(struct bnxt_cp_ring_info *cpr);
int bnxt_get_ring_stats(struct bnxt *bp, struct bnxt_cp_ring_info *cpr, int idx,
			struct bnxt_ring_stats *stats, bool rx);
int bnxt_get_ring_stats_ext(struct bnxt *bp, struct bnxt_cp_ring_info *cpr, int idx,
			    struct bnxt_ring_stats_ext *ring_stats, bool rx);

struct bnxt_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	uint64_t offset;
};
#endif
