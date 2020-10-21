/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_prefetch.h>
#include <rte_vect.h>

#include <mlx5_glue.h>
#include <mlx5_prm.h>

#include "mlx5_defs.h"
#include "mlx5.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_rxtx_vec.h"
#include "mlx5_autoconf.h"

#if defined RTE_ARCH_X86_64
#include "mlx5_rxtx_vec_sse.h"
#elif defined RTE_ARCH_ARM64
#include "mlx5_rxtx_vec_neon.h"
#elif defined RTE_ARCH_PPC_64
#include "mlx5_rxtx_vec_altivec.h"
#else
#error "This should not be compiled if SIMD instructions are not supported."
#endif

/**
 * Skip error packets.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received (<= pkts_n).
 */
static uint16_t
rxq_handle_pending_error(struct mlx5_rxq_data *rxq, struct rte_mbuf **pkts,
			 uint16_t pkts_n)
{
	uint16_t n = 0;
	unsigned int i;
#ifdef MLX5_PMD_SOFT_COUNTERS
	uint32_t err_bytes = 0;
#endif

	for (i = 0; i < pkts_n; ++i) {
		struct rte_mbuf *pkt = pkts[i];

		if (pkt->packet_type == RTE_PTYPE_ALL_MASK || rxq->err_state) {
#ifdef MLX5_PMD_SOFT_COUNTERS
			err_bytes += PKT_LEN(pkt);
#endif
			rte_pktmbuf_free_seg(pkt);
		} else {
			pkts[n++] = pkt;
		}
	}
	rxq->stats.idropped += (pkts_n - n);
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Correct counters of errored completions. */
	rxq->stats.ipackets -= (pkts_n - n);
	rxq->stats.ibytes -= err_bytes;
#endif
	mlx5_rx_err_handle(rxq, 1);
	return n;
}

/**
 * Receive burst of packets. An errored completion also consumes a mbuf, but the
 * packet_type is set to be RTE_PTYPE_ALL_MASK. Marked mbufs should be freed
 * before returning to application.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 * @param[out] err
 *   Pointer to a flag. Set non-zero value if pkts array has at least one error
 *   packet to handle.
 * @param[out] no_cq
 *   Pointer to a boolean. Set true if no new CQE seen.
 *
 * @return
 *   Number of packets received including errors (<= pkts_n).
 */
static inline uint16_t
rxq_burst_v(struct mlx5_rxq_data *rxq, struct rte_mbuf **pkts,
	    uint16_t pkts_n, uint64_t *err, bool *no_cq)
{
	const uint16_t q_n = 1 << rxq->cqe_n;
	const uint16_t q_mask = q_n - 1;
	const uint16_t e_n = 1 << rxq->elts_n;
	const uint16_t e_mask = e_n - 1;
	volatile struct mlx5_cqe *cq;
	struct rte_mbuf **elts;
	uint64_t comp_idx = MLX5_VPMD_DESCS_PER_LOOP;
	uint16_t nocmp_n = 0;
	uint16_t rcvd_pkt = 0;
	unsigned int cq_idx = rxq->cq_ci & q_mask;
	unsigned int elts_idx;

	MLX5_ASSERT(rxq->sges_n == 0);
	MLX5_ASSERT(rxq->cqe_n == rxq->elts_n);
	cq = &(*rxq->cqes)[cq_idx];
	rte_prefetch0(cq);
	rte_prefetch0(cq + 1);
	rte_prefetch0(cq + 2);
	rte_prefetch0(cq + 3);
	pkts_n = RTE_MIN(pkts_n, MLX5_VPMD_RX_MAX_BURST);
	mlx5_rx_replenish_bulk_mbuf(rxq);
	/* See if there're unreturned mbufs from compressed CQE. */
	rcvd_pkt = rxq->decompressed;
	if (rcvd_pkt > 0) {
		rcvd_pkt = RTE_MIN(rcvd_pkt, pkts_n);
		rxq_copy_mbuf_v(&(*rxq->elts)[rxq->rq_pi & e_mask],
				pkts, rcvd_pkt);
		rxq->rq_pi += rcvd_pkt;
		rxq->decompressed -= rcvd_pkt;
		pkts += rcvd_pkt;
	}
	elts_idx = rxq->rq_pi & e_mask;
	elts = &(*rxq->elts)[elts_idx];
	/* Not to overflow pkts array. */
	pkts_n = RTE_ALIGN_FLOOR(pkts_n - rcvd_pkt, MLX5_VPMD_DESCS_PER_LOOP);
	/* Not to cross queue end. */
	pkts_n = RTE_MIN(pkts_n, q_n - elts_idx);
	pkts_n = RTE_MIN(pkts_n, q_n - cq_idx);
	if (!pkts_n) {
		*no_cq = !rcvd_pkt;
		return rcvd_pkt;
	}
	/* At this point, there shouldn't be any remaining packets. */
	MLX5_ASSERT(rxq->decompressed == 0);
	/* Process all the CQEs */
	nocmp_n = rxq_cq_process_v(rxq, cq, elts, pkts, pkts_n, err, &comp_idx);
	/* If no new CQE seen, return without updating cq_db. */
	if (unlikely(!nocmp_n && comp_idx == MLX5_VPMD_DESCS_PER_LOOP)) {
		*no_cq = true;
		return rcvd_pkt;
	}
	/* Update the consumer indexes for non-compressed CQEs. */
	MLX5_ASSERT(nocmp_n <= pkts_n);
	rxq->cq_ci += nocmp_n;
	rxq->rq_pi += nocmp_n;
	rcvd_pkt += nocmp_n;
	/* Decompress the last CQE if compressed. */
	if (comp_idx < MLX5_VPMD_DESCS_PER_LOOP) {
		MLX5_ASSERT(comp_idx == (nocmp_n % MLX5_VPMD_DESCS_PER_LOOP));
		rxq->decompressed = rxq_cq_decompress_v(rxq, &cq[nocmp_n],
							&elts[nocmp_n]);
		rxq->cq_ci += rxq->decompressed;
		/* Return more packets if needed. */
		if (nocmp_n < pkts_n) {
			uint16_t n = rxq->decompressed;

			n = RTE_MIN(n, pkts_n - nocmp_n);
			rxq_copy_mbuf_v(&(*rxq->elts)[rxq->rq_pi & e_mask],
					&pkts[nocmp_n], n);
			rxq->rq_pi += n;
			rcvd_pkt += n;
			rxq->decompressed -= n;
		}
	}
	rte_io_wmb();
	*rxq->cq_db = rte_cpu_to_be_32(rxq->cq_ci);
	*no_cq = !rcvd_pkt;
	return rcvd_pkt;
}

/**
 * DPDK callback for vectorized RX.
 *
 * @param dpdk_rxq
 *   Generic pointer to RX queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received (<= pkts_n).
 */
uint16_t
mlx5_rx_burst_vec(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct mlx5_rxq_data *rxq = dpdk_rxq;
	uint16_t nb_rx = 0;
	uint16_t tn = 0;
	uint64_t err = 0;
	bool no_cq = false;

	do {
		nb_rx = rxq_burst_v(rxq, pkts + tn, pkts_n - tn, &err, &no_cq);
		if (unlikely(err | rxq->err_state))
			nb_rx = rxq_handle_pending_error(rxq, pkts + tn, nb_rx);
		tn += nb_rx;
		if (unlikely(no_cq))
			break;
	} while (tn != pkts_n);
	return tn;
}

/**
 * Check a RX queue can support vectorized RX.
 *
 * @param rxq
 *   Pointer to RX queue.
 *
 * @return
 *   1 if supported, negative errno value if not.
 */
int __rte_cold
mlx5_rxq_check_vec_support(struct mlx5_rxq_data *rxq)
{
	struct mlx5_rxq_ctrl *ctrl =
		container_of(rxq, struct mlx5_rxq_ctrl, rxq);

	if (mlx5_mprq_enabled(ETH_DEV(ctrl->priv)))
		return -ENOTSUP;
	if (!ctrl->priv->config.rx_vec_en || rxq->sges_n != 0)
		return -ENOTSUP;
	if (rxq->lro)
		return -ENOTSUP;
	return 1;
}

/**
 * Check a device can support vectorized RX.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   1 if supported, negative errno value if not.
 */
int __rte_cold
mlx5_check_vec_rx_support(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t i;

	if (rte_vect_get_max_simd_bitwidth() < RTE_VECT_SIMD_128)
		return -ENOTSUP;
	if (!priv->config.rx_vec_en)
		return -ENOTSUP;
	if (mlx5_mprq_enabled(dev))
		return -ENOTSUP;
	/* All the configured queues should support. */
	for (i = 0; i < priv->rxqs_n; ++i) {
		struct mlx5_rxq_data *rxq = (*priv->rxqs)[i];

		if (!rxq)
			continue;
		if (mlx5_rxq_check_vec_support(rxq) < 0)
			break;
	}
	if (i != priv->rxqs_n)
		return -ENOTSUP;
	return 1;
}
