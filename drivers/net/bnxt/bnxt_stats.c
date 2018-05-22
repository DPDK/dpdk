/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
 */

#include <inttypes.h>

#include <rte_byteorder.h>

#include "bnxt.h"
#include "bnxt_cpr.h"
#include "bnxt_hwrm.h"
#include "bnxt_rxq.h"
#include "bnxt_stats.h"
#include "bnxt_txq.h"
#include "hsi_struct_def_dpdk.h"

static const struct bnxt_xstats_name_off bnxt_rx_stats_strings[] = {
	{"rx_64b_frames", offsetof(struct rx_port_stats,
				rx_64b_frames)},
	{"rx_65b_127b_frames", offsetof(struct rx_port_stats,
				rx_65b_127b_frames)},
	{"rx_128b_255b_frames", offsetof(struct rx_port_stats,
				rx_128b_255b_frames)},
	{"rx_256b_511b_frames", offsetof(struct rx_port_stats,
				rx_256b_511b_frames)},
	{"rx_512b_1023b_frames", offsetof(struct rx_port_stats,
				rx_512b_1023b_frames)},
	{"rx_1024b_1518_frames", offsetof(struct rx_port_stats,
				rx_1024b_1518_frames)},
	{"rx_good_vlan_frames", offsetof(struct rx_port_stats,
				rx_good_vlan_frames)},
	{"rx_1519b_2047b_frames", offsetof(struct rx_port_stats,
				rx_1519b_2047b_frames)},
	{"rx_2048b_4095b_frames", offsetof(struct rx_port_stats,
				rx_2048b_4095b_frames)},
	{"rx_4096b_9216b_frames", offsetof(struct rx_port_stats,
				rx_4096b_9216b_frames)},
	{"rx_9217b_16383b_frames", offsetof(struct rx_port_stats,
				rx_9217b_16383b_frames)},
	{"rx_total_frames", offsetof(struct rx_port_stats,
				rx_total_frames)},
	{"rx_ucast_frames", offsetof(struct rx_port_stats,
				rx_ucast_frames)},
	{"rx_mcast_frames", offsetof(struct rx_port_stats,
				rx_mcast_frames)},
	{"rx_bcast_frames", offsetof(struct rx_port_stats,
				rx_bcast_frames)},
	{"rx_fcs_err_frames", offsetof(struct rx_port_stats,
				rx_fcs_err_frames)},
	{"rx_ctrl_frames", offsetof(struct rx_port_stats,
				rx_ctrl_frames)},
	{"rx_pause_frames", offsetof(struct rx_port_stats,
				rx_pause_frames)},
	{"rx_pfc_frames", offsetof(struct rx_port_stats,
				rx_pfc_frames)},
	{"rx_align_err_frames", offsetof(struct rx_port_stats,
				rx_align_err_frames)},
	{"rx_ovrsz_frames", offsetof(struct rx_port_stats,
				rx_ovrsz_frames)},
	{"rx_jbr_frames", offsetof(struct rx_port_stats,
				rx_jbr_frames)},
	{"rx_mtu_err_frames", offsetof(struct rx_port_stats,
				rx_mtu_err_frames)},
	{"rx_tagged_frames", offsetof(struct rx_port_stats,
				rx_tagged_frames)},
	{"rx_double_tagged_frames", offsetof(struct rx_port_stats,
				rx_double_tagged_frames)},
	{"rx_good_frames", offsetof(struct rx_port_stats,
				rx_good_frames)},
	{"rx_undrsz_frames", offsetof(struct rx_port_stats,
				rx_undrsz_frames)},
	{"rx_eee_lpi_events", offsetof(struct rx_port_stats,
				rx_eee_lpi_events)},
	{"rx_eee_lpi_duration", offsetof(struct rx_port_stats,
				rx_eee_lpi_duration)},
	{"rx_bytes", offsetof(struct rx_port_stats,
				rx_bytes)},
	{"rx_runt_bytes", offsetof(struct rx_port_stats,
				rx_runt_bytes)},
	{"rx_runt_frames", offsetof(struct rx_port_stats,
				rx_runt_frames)},
};

static const struct bnxt_xstats_name_off bnxt_tx_stats_strings[] = {
	{"tx_64b_frames", offsetof(struct tx_port_stats,
				tx_64b_frames)},
	{"tx_65b_127b_frames", offsetof(struct tx_port_stats,
				tx_65b_127b_frames)},
	{"tx_128b_255b_frames", offsetof(struct tx_port_stats,
				tx_128b_255b_frames)},
	{"tx_256b_511b_frames", offsetof(struct tx_port_stats,
				tx_256b_511b_frames)},
	{"tx_512b_1023b_frames", offsetof(struct tx_port_stats,
				tx_512b_1023b_frames)},
	{"tx_1024b_1518_frames", offsetof(struct tx_port_stats,
				tx_1024b_1518_frames)},
	{"tx_good_vlan_frames", offsetof(struct tx_port_stats,
				tx_good_vlan_frames)},
	{"tx_1519b_2047_frames", offsetof(struct tx_port_stats,
				tx_1519b_2047_frames)},
	{"tx_2048b_4095b_frames", offsetof(struct tx_port_stats,
				tx_2048b_4095b_frames)},
	{"tx_4096b_9216b_frames", offsetof(struct tx_port_stats,
				tx_4096b_9216b_frames)},
	{"tx_9217b_16383b_frames", offsetof(struct tx_port_stats,
				tx_9217b_16383b_frames)},
	{"tx_good_frames", offsetof(struct tx_port_stats,
				tx_good_frames)},
	{"tx_total_frames", offsetof(struct tx_port_stats,
				tx_total_frames)},
	{"tx_ucast_frames", offsetof(struct tx_port_stats,
				tx_ucast_frames)},
	{"tx_mcast_frames", offsetof(struct tx_port_stats,
				tx_mcast_frames)},
	{"tx_bcast_frames", offsetof(struct tx_port_stats,
				tx_bcast_frames)},
	{"tx_pause_frames", offsetof(struct tx_port_stats,
				tx_pause_frames)},
	{"tx_pfc_frames", offsetof(struct tx_port_stats,
				tx_pfc_frames)},
	{"tx_jabber_frames", offsetof(struct tx_port_stats,
				tx_jabber_frames)},
	{"tx_fcs_err_frames", offsetof(struct tx_port_stats,
				tx_fcs_err_frames)},
	{"tx_err", offsetof(struct tx_port_stats,
				tx_err)},
	{"tx_fifo_underruns", offsetof(struct tx_port_stats,
				tx_fifo_underruns)},
	{"tx_eee_lpi_events", offsetof(struct tx_port_stats,
				tx_eee_lpi_events)},
	{"tx_eee_lpi_duration", offsetof(struct tx_port_stats,
				tx_eee_lpi_duration)},
	{"tx_total_collisions", offsetof(struct tx_port_stats,
				tx_total_collisions)},
	{"tx_bytes", offsetof(struct tx_port_stats,
				tx_bytes)},
};

static const struct bnxt_xstats_name_off bnxt_func_stats_strings[] = {
	{"tx_ucast_pkts", offsetof(struct hwrm_func_qstats_output,
				tx_ucast_pkts)},
	{"tx_mcast_pkts", offsetof(struct hwrm_func_qstats_output,
				tx_mcast_pkts)},
	{"tx_bcast_pkts", offsetof(struct hwrm_func_qstats_output,
				tx_bcast_pkts)},
	{"tx_discard_pkts", offsetof(struct hwrm_func_qstats_output,
				tx_discard_pkts)},
	{"tx_drop_pkts", offsetof(struct hwrm_func_qstats_output,
				tx_drop_pkts)},
	{"tx_ucast_bytes", offsetof(struct hwrm_func_qstats_output,
				tx_ucast_bytes)},
	{"tx_mcast_bytes", offsetof(struct hwrm_func_qstats_output,
				tx_mcast_bytes)},
	{"tx_bcast_bytes", offsetof(struct hwrm_func_qstats_output,
				tx_bcast_bytes)},
	{"rx_ucast_pkts", offsetof(struct hwrm_func_qstats_output,
				rx_ucast_pkts)},
	{"rx_mcast_pkts", offsetof(struct hwrm_func_qstats_output,
				rx_mcast_pkts)},
	{"rx_bcast_pkts", offsetof(struct hwrm_func_qstats_output,
				rx_bcast_pkts)},
	{"rx_discard_pkts", offsetof(struct hwrm_func_qstats_output,
				rx_discard_pkts)},
	{"rx_drop_pkts", offsetof(struct hwrm_func_qstats_output,
				rx_drop_pkts)},
	{"rx_ucast_bytes", offsetof(struct hwrm_func_qstats_output,
				rx_ucast_bytes)},
	{"rx_mcast_bytes", offsetof(struct hwrm_func_qstats_output,
				rx_mcast_bytes)},
	{"rx_bcast_bytes", offsetof(struct hwrm_func_qstats_output,
				rx_bcast_bytes)},
	{"rx_agg_pkts", offsetof(struct hwrm_func_qstats_output,
				rx_agg_pkts)},
	{"rx_agg_bytes", offsetof(struct hwrm_func_qstats_output,
				rx_agg_bytes)},
	{"rx_agg_events", offsetof(struct hwrm_func_qstats_output,
				rx_agg_events)},
	{"rx_agg_aborts", offsetof(struct hwrm_func_qstats_output,
				rx_agg_aborts)},
};

/*
 * Statistics functions
 */

void bnxt_free_stats(struct bnxt *bp)
{
	int i;

	for (i = 0; i < (int)bp->tx_cp_nr_rings; i++) {
		struct bnxt_tx_queue *txq = bp->tx_queues[i];

		bnxt_free_txq_stats(txq);
	}
	for (i = 0; i < (int)bp->rx_cp_nr_rings; i++) {
		struct bnxt_rx_queue *rxq = bp->rx_queues[i];

		bnxt_free_rxq_stats(rxq);
	}
}

int bnxt_stats_get_op(struct rte_eth_dev *eth_dev,
			   struct rte_eth_stats *bnxt_stats)
{
	int rc = 0;
	unsigned int i;
	struct bnxt *bp = eth_dev->data->dev_private;

	memset(bnxt_stats, 0, sizeof(*bnxt_stats));
	if (!(bp->flags & BNXT_FLAG_INIT_DONE)) {
		PMD_DRV_LOG(ERR, "Device Initialization not complete!\n");
		return -1;
	}

	for (i = 0; i < bp->rx_cp_nr_rings; i++) {
		struct bnxt_rx_queue *rxq = bp->rx_queues[i];
		struct bnxt_cp_ring_info *cpr = rxq->cp_ring;

		rc = bnxt_hwrm_ctx_qstats(bp, cpr->hw_stats_ctx_id, i,
				     bnxt_stats, 1);
		if (unlikely(rc))
			return rc;
		bnxt_stats->rx_nombuf +=
				rte_atomic64_read(&rxq->rx_mbuf_alloc_fail);
	}

	for (i = 0; i < bp->tx_cp_nr_rings; i++) {
		struct bnxt_tx_queue *txq = bp->tx_queues[i];
		struct bnxt_cp_ring_info *cpr = txq->cp_ring;

		rc = bnxt_hwrm_ctx_qstats(bp, cpr->hw_stats_ctx_id, i,
				     bnxt_stats, 0);
		if (unlikely(rc))
			return rc;
	}
	rc = bnxt_hwrm_func_qstats(bp, 0xffff, bnxt_stats);
	if (unlikely(rc))
		return rc;
	return rc;
}

void bnxt_stats_reset_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	unsigned int i;

	if (!(bp->flags & BNXT_FLAG_INIT_DONE)) {
		PMD_DRV_LOG(ERR, "Device Initialization not complete!\n");
		return;
	}

	bnxt_clear_all_hwrm_stat_ctxs(bp);
	for (i = 0; i < bp->rx_cp_nr_rings; i++) {
		struct bnxt_rx_queue *rxq = bp->rx_queues[i];

		rte_atomic64_clear(&rxq->rx_mbuf_alloc_fail);
	}
}

int bnxt_dev_xstats_get_op(struct rte_eth_dev *eth_dev,
			   struct rte_eth_xstat *xstats, unsigned int n)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;

	unsigned int count, i;
	uint64_t tx_drop_pkts;

	bnxt_hwrm_port_qstats(bp);
	bnxt_hwrm_func_qstats_tx_drop(bp, 0xffff, &tx_drop_pkts);

	count = RTE_DIM(bnxt_rx_stats_strings) +
		RTE_DIM(bnxt_tx_stats_strings) + 1; /* For tx_drop_pkts */

	if (n < count)
		return count;

	count = 0;
	for (i = 0; i < RTE_DIM(bnxt_rx_stats_strings); i++) {
		uint64_t *rx_stats = (uint64_t *)bp->hw_rx_port_stats;
		xstats[count].value = rte_le_to_cpu_64(
				*(uint64_t *)((char *)rx_stats +
				bnxt_rx_stats_strings[i].offset));
		count++;
	}

	for (i = 0; i < RTE_DIM(bnxt_tx_stats_strings); i++) {
		uint64_t *tx_stats = (uint64_t *)bp->hw_tx_port_stats;
		xstats[count].value = rte_le_to_cpu_64(
				 *(uint64_t *)((char *)tx_stats +
				bnxt_tx_stats_strings[i].offset));
		count++;
	}

	/* The Tx drop pkts aka the Anti spoof coounter */
	xstats[count].value = rte_le_to_cpu_64(tx_drop_pkts);
	count++;

	return count;
}

int bnxt_dev_xstats_get_names_op(__rte_unused struct rte_eth_dev *eth_dev,
	struct rte_eth_xstat_name *xstats_names,
	__rte_unused unsigned int limit)
{
	/* Account for the Tx drop pkts aka the Anti spoof counter */
	const unsigned int stat_cnt = RTE_DIM(bnxt_rx_stats_strings) +
				RTE_DIM(bnxt_tx_stats_strings) + 1;
	unsigned int i, count;

	if (xstats_names != NULL) {
		count = 0;

		for (i = 0; i < RTE_DIM(bnxt_rx_stats_strings); i++) {
			snprintf(xstats_names[count].name,
				sizeof(xstats_names[count].name),
				"%s",
				bnxt_rx_stats_strings[i].name);
			count++;
		}

		for (i = 0; i < RTE_DIM(bnxt_tx_stats_strings); i++) {
			snprintf(xstats_names[count].name,
				sizeof(xstats_names[count].name),
				"%s",
				bnxt_tx_stats_strings[i].name);
			count++;
		}

		snprintf(xstats_names[count].name,
				sizeof(xstats_names[count].name),
				"%s",
				bnxt_func_stats_strings[4].name);
		count++;
	}
	return stat_cnt;
}

void bnxt_dev_xstats_reset_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;

	if (bp->flags & BNXT_FLAG_PORT_STATS && BNXT_SINGLE_PF(bp))
		bnxt_hwrm_port_clr_stats(bp);

	if (BNXT_VF(bp))
		PMD_DRV_LOG(ERR, "Operation not supported on a VF device\n");
	if (!BNXT_SINGLE_PF(bp))
		PMD_DRV_LOG(ERR, "Operation not supported on a MF device\n");
	if (!(bp->flags & BNXT_FLAG_PORT_STATS))
		PMD_DRV_LOG(ERR, "Operation not supported\n");
}

int bnxt_dev_xstats_get_by_id_op(struct rte_eth_dev *dev, const uint64_t *ids,
		uint64_t *values, unsigned int limit)
{
	/* Account for the Tx drop pkts aka the Anti spoof counter */
	const unsigned int stat_cnt = RTE_DIM(bnxt_rx_stats_strings) +
				RTE_DIM(bnxt_tx_stats_strings) + 1;
	struct rte_eth_xstat xstats[stat_cnt];
	uint64_t values_copy[stat_cnt];
	uint16_t i;

	if (!ids)
		return bnxt_dev_xstats_get_op(dev, xstats, stat_cnt);

	bnxt_dev_xstats_get_by_id_op(dev, NULL, values_copy, stat_cnt);
	for (i = 0; i < limit; i++) {
		if (ids[i] >= stat_cnt) {
			PMD_DRV_LOG(ERR, "id value isn't valid");
			return -1;
		}
		values[i] = values_copy[ids[i]];
	}
	return stat_cnt;
}

int bnxt_dev_xstats_get_names_by_id_op(struct rte_eth_dev *dev,
				struct rte_eth_xstat_name *xstats_names,
				const uint64_t *ids, unsigned int limit)
{
	/* Account for the Tx drop pkts aka the Anti spoof counter */
	const unsigned int stat_cnt = RTE_DIM(bnxt_rx_stats_strings) +
				RTE_DIM(bnxt_tx_stats_strings) + 1;
	struct rte_eth_xstat_name xstats_names_copy[stat_cnt];
	uint16_t i;

	if (!ids)
		return bnxt_dev_xstats_get_names_op(dev, xstats_names,
						    stat_cnt);
	bnxt_dev_xstats_get_names_by_id_op(dev, xstats_names_copy, NULL,
			stat_cnt);

	for (i = 0; i < limit; i++) {
		if (ids[i] >= stat_cnt) {
			PMD_DRV_LOG(ERR, "id value isn't valid");
			return -1;
		}
		strcpy(xstats_names[i].name,
				xstats_names_copy[ids[i]].name);
	}
	return stat_cnt;
}
