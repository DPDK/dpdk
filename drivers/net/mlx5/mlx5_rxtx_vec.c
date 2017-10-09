/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 6WIND S.A.
 *   Copyright 2017 Mellanox.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of 6WIND S.A. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#include <infiniband/mlx5dv.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_prefetch.h>

#include "mlx5.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_rxtx_vec.h"
#include "mlx5_autoconf.h"
#include "mlx5_defs.h"
#include "mlx5_prm.h"

#if defined RTE_ARCH_X86_64
#include "mlx5_rxtx_vec_sse.h"
#elif defined RTE_ARCH_ARM64
#include "mlx5_rxtx_vec_neon.h"
#else
#error "This should not be compiled if SIMD instructions are not supported."
#endif

/**
 * Count the number of continuous single segment packets.
 *
 * @param pkts
 *   Pointer to array of packets.
 * @param pkts_n
 *   Number of packets.
 *
 * @return
 *   Number of continuous single segment packets.
 */
static inline unsigned int
txq_check_multiseg(struct rte_mbuf **pkts, uint16_t pkts_n)
{
	unsigned int pos;

	if (!pkts_n)
		return 0;
	/* Count the number of continuous single segment packets. */
	for (pos = 0; pos < pkts_n; ++pos)
		if (NB_SEGS(pkts[pos]) > 1)
			break;
	return pos;
}

/**
 * Count the number of packets having same ol_flags and calculate cs_flags.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param pkts
 *   Pointer to array of packets.
 * @param pkts_n
 *   Number of packets.
 * @param cs_flags
 *   Pointer of flags to be returned.
 *
 * @return
 *   Number of packets having same ol_flags.
 */
static inline unsigned int
txq_calc_offload(struct mlx5_txq_data *txq, struct rte_mbuf **pkts,
		 uint16_t pkts_n, uint8_t *cs_flags)
{
	unsigned int pos;
	const uint64_t ol_mask =
		PKT_TX_IP_CKSUM | PKT_TX_TCP_CKSUM |
		PKT_TX_UDP_CKSUM | PKT_TX_TUNNEL_GRE |
		PKT_TX_TUNNEL_VXLAN | PKT_TX_OUTER_IP_CKSUM;

	if (!pkts_n)
		return 0;
	/* Count the number of packets having same ol_flags. */
	for (pos = 1; pos < pkts_n; ++pos)
		if ((pkts[pos]->ol_flags ^ pkts[0]->ol_flags) & ol_mask)
			break;
	/* Should open another MPW session for the rest. */
	if (pkts[0]->ol_flags &
	    (PKT_TX_IP_CKSUM | PKT_TX_TCP_CKSUM | PKT_TX_UDP_CKSUM)) {
		const uint64_t is_tunneled =
			pkts[0]->ol_flags &
			(PKT_TX_TUNNEL_GRE |
			 PKT_TX_TUNNEL_VXLAN);

		if (is_tunneled && txq->tunnel_en) {
			*cs_flags = MLX5_ETH_WQE_L3_INNER_CSUM |
				    MLX5_ETH_WQE_L4_INNER_CSUM;
			if (pkts[0]->ol_flags & PKT_TX_OUTER_IP_CKSUM)
				*cs_flags |= MLX5_ETH_WQE_L3_CSUM;
		} else {
			*cs_flags = MLX5_ETH_WQE_L3_CSUM |
				    MLX5_ETH_WQE_L4_CSUM;
		}
	}
	return pos;
}

/**
 * DPDK callback for vectorized TX.
 *
 * @param dpdk_txq
 *   Generic pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
uint16_t
mlx5_tx_burst_raw_vec(void *dpdk_txq, struct rte_mbuf **pkts,
		      uint16_t pkts_n)
{
	struct mlx5_txq_data *txq = (struct mlx5_txq_data *)dpdk_txq;
	uint16_t nb_tx = 0;

	while (pkts_n > nb_tx) {
		uint16_t n;
		uint16_t ret;

		n = RTE_MIN((uint16_t)(pkts_n - nb_tx), MLX5_VPMD_TX_MAX_BURST);
		ret = txq_burst_v(txq, &pkts[nb_tx], n, 0);
		nb_tx += ret;
		if (!ret)
			break;
	}
	return nb_tx;
}

/**
 * DPDK callback for vectorized TX with multi-seg packets and offload.
 *
 * @param dpdk_txq
 *   Generic pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
uint16_t
mlx5_tx_burst_vec(void *dpdk_txq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct mlx5_txq_data *txq = (struct mlx5_txq_data *)dpdk_txq;
	uint16_t nb_tx = 0;

	while (pkts_n > nb_tx) {
		uint8_t cs_flags = 0;
		uint16_t n;
		uint16_t ret;

		/* Transmit multi-seg packets in the head of pkts list. */
		if (!(txq->flags & ETH_TXQ_FLAGS_NOMULTSEGS) &&
		    NB_SEGS(pkts[nb_tx]) > 1)
			nb_tx += txq_scatter_v(txq,
					       &pkts[nb_tx],
					       pkts_n - nb_tx);
		n = RTE_MIN((uint16_t)(pkts_n - nb_tx), MLX5_VPMD_TX_MAX_BURST);
		if (!(txq->flags & ETH_TXQ_FLAGS_NOMULTSEGS))
			n = txq_check_multiseg(&pkts[nb_tx], n);
		if (!(txq->flags & ETH_TXQ_FLAGS_NOOFFLOADS))
			n = txq_calc_offload(txq, &pkts[nb_tx], n, &cs_flags);
		ret = txq_burst_v(txq, &pkts[nb_tx], n, cs_flags);
		nb_tx += ret;
		if (!ret)
			break;
	}
	return nb_tx;
}

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

		if (pkt->packet_type == RTE_PTYPE_ALL_MASK) {
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
	rxq->pending_err = 0;
	return n;
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
	uint16_t nb_rx;

	nb_rx = rxq_burst_v(rxq, pkts, pkts_n);
	if (unlikely(rxq->pending_err))
		nb_rx = rxq_handle_pending_error(rxq, pkts, nb_rx);
	return nb_rx;
}

/**
 * Check Tx queue flags are set for raw vectorized Tx.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   1 if supported, negative errno value if not.
 */
int __attribute__((cold))
priv_check_raw_vec_tx_support(struct priv *priv)
{
	uint16_t i;

	/* All the configured queues should support. */
	for (i = 0; i < priv->txqs_n; ++i) {
		struct mlx5_txq_data *txq = (*priv->txqs)[i];

		if (!(txq->flags & ETH_TXQ_FLAGS_NOMULTSEGS) ||
		    !(txq->flags & ETH_TXQ_FLAGS_NOOFFLOADS))
			break;
	}
	if (i != priv->txqs_n)
		return -ENOTSUP;
	return 1;
}

/**
 * Check a device can support vectorized TX.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   1 if supported, negative errno value if not.
 */
int __attribute__((cold))
priv_check_vec_tx_support(struct priv *priv)
{
	if (!priv->tx_vec_en ||
	    priv->txqs_n > MLX5_VPMD_MIN_TXQS ||
	    priv->mps != MLX5_MPW_ENHANCED ||
	    priv->tso)
		return -ENOTSUP;
	return 1;
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
int __attribute__((cold))
rxq_check_vec_support(struct mlx5_rxq_data *rxq)
{
	struct mlx5_rxq_ctrl *ctrl =
		container_of(rxq, struct mlx5_rxq_ctrl, rxq);

	if (!ctrl->priv->rx_vec_en || rxq->sges_n != 0)
		return -ENOTSUP;
	return 1;
}

/**
 * Check a device can support vectorized RX.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   1 if supported, negative errno value if not.
 */
int __attribute__((cold))
priv_check_vec_rx_support(struct priv *priv)
{
	uint16_t i;

	if (!priv->rx_vec_en)
		return -ENOTSUP;
	/* All the configured queues should support. */
	for (i = 0; i < priv->rxqs_n; ++i) {
		struct mlx5_rxq_data *rxq = (*priv->rxqs)[i];

		if (!rxq)
			continue;
		if (rxq_check_vec_support(rxq) < 0)
			break;
	}
	if (i != priv->rxqs_n)
		return -ENOTSUP;
	return 1;
}
