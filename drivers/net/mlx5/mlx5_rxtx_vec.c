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
