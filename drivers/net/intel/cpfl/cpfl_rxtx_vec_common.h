/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef _CPFL_RXTX_VEC_COMMON_H_
#define _CPFL_RXTX_VEC_COMMON_H_
#include <stdint.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>

#include "cpfl_ethdev.h"
#include "cpfl_rxtx.h"

#ifdef RTE_ARCH_X86
#include "../common/rx_vec_x86.h"
#endif

#define CPFL_SCALAR_PATH		0
#define CPFL_VECTOR_PATH		1
#define CPFL_RX_NO_VECTOR_FLAGS (		\
		RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |	\
		RTE_ETH_RX_OFFLOAD_UDP_CKSUM |	\
		RTE_ETH_RX_OFFLOAD_TCP_CKSUM |	\
		RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM |	\
		RTE_ETH_RX_OFFLOAD_TIMESTAMP)

static inline int
cpfl_rx_vec_queue_default(struct idpf_rx_queue *rxq)
{
	if (rxq == NULL)
		return CPFL_SCALAR_PATH;

	if (rte_is_power_of_2(rxq->nb_rx_desc) == 0)
		return CPFL_SCALAR_PATH;

	if (rxq->rx_free_thresh < IDPF_VPMD_RX_MAX_BURST)
		return CPFL_SCALAR_PATH;

	if ((rxq->nb_rx_desc % rxq->rx_free_thresh) != 0)
		return CPFL_SCALAR_PATH;

	if ((rxq->offloads & CPFL_RX_NO_VECTOR_FLAGS) != 0)
		return CPFL_SCALAR_PATH;

	return CPFL_VECTOR_PATH;
}

static inline int
cpfl_tx_vec_queue_default(struct ci_tx_queue *txq)
{
	if (txq == NULL)
		return CPFL_SCALAR_PATH;

	if (txq->tx_rs_thresh < IDPF_VPMD_TX_MAX_BURST ||
	    (txq->tx_rs_thresh & 3) != 0)
		return CPFL_SCALAR_PATH;

	return CPFL_VECTOR_PATH;
}

static inline int
cpfl_rx_splitq_vec_default(struct idpf_rx_queue *rxq)
{
	if (rxq->bufq2->rx_buf_len < rxq->max_pkt_len)
		return CPFL_SCALAR_PATH;

	return CPFL_VECTOR_PATH;
}

static inline int
cpfl_rx_vec_dev_check_default(struct rte_eth_dev *dev)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct cpfl_rx_queue *cpfl_rxq;
	int i, default_ret, splitq_ret, ret = CPFL_SCALAR_PATH;

	if (dev->data->scattered_rx)
		return CPFL_SCALAR_PATH;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		cpfl_rxq = dev->data->rx_queues[i];
		default_ret = cpfl_rx_vec_queue_default(&cpfl_rxq->base);
		if (vport->rxq_model == VIRTCHNL2_QUEUE_MODEL_SPLIT) {
			if (cpfl_rxq->hairpin_info.hairpin_q)
				continue;
			splitq_ret = cpfl_rx_splitq_vec_default(&cpfl_rxq->base);
			ret = splitq_ret && default_ret;
		} else {
			ret = default_ret;
		}
		if (ret == CPFL_SCALAR_PATH)
			return CPFL_SCALAR_PATH;
	}

	return CPFL_VECTOR_PATH;
}

static inline int
cpfl_tx_vec_dev_check_default(struct rte_eth_dev *dev)
{
	int i;
	struct cpfl_tx_queue *cpfl_txq;
	int ret = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		cpfl_txq = dev->data->tx_queues[i];
		if (cpfl_txq->hairpin_info.hairpin_q)
			continue;
		ret = cpfl_tx_vec_queue_default(&cpfl_txq->base);
		if (ret == CPFL_SCALAR_PATH)
			return CPFL_SCALAR_PATH;
	}

	return CPFL_VECTOR_PATH;
}

#ifdef RTE_ARCH_X86
static inline enum rte_vect_max_simd
cpfl_get_max_simd_bitwidth(void)
{
	return ci_get_x86_max_simd_bitwidth();
}
#endif

#endif /*_CPFL_RXTX_VEC_COMMON_H_*/
