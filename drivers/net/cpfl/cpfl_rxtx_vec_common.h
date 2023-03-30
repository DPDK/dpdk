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

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

#define CPFL_SCALAR_PATH		0
#define CPFL_VECTOR_PATH		1
#define CPFL_RX_NO_VECTOR_FLAGS (		\
		RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |	\
		RTE_ETH_RX_OFFLOAD_UDP_CKSUM |	\
		RTE_ETH_RX_OFFLOAD_TCP_CKSUM |	\
		RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM |	\
		RTE_ETH_RX_OFFLOAD_TIMESTAMP)
#define CPFL_TX_NO_VECTOR_FLAGS (		\
		RTE_ETH_TX_OFFLOAD_TCP_TSO |	\
		RTE_ETH_TX_OFFLOAD_MULTI_SEGS)

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
cpfl_tx_vec_queue_default(struct idpf_tx_queue *txq)
{
	if (txq == NULL)
		return CPFL_SCALAR_PATH;

	if (txq->rs_thresh < IDPF_VPMD_TX_MAX_BURST ||
	    (txq->rs_thresh & 3) != 0)
		return CPFL_SCALAR_PATH;

	if ((txq->offloads & CPFL_TX_NO_VECTOR_FLAGS) != 0)
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
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_rx_queue *rxq;
	int i, default_ret, splitq_ret, ret = CPFL_SCALAR_PATH;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		default_ret = cpfl_rx_vec_queue_default(rxq);
		if (vport->rxq_model == VIRTCHNL2_QUEUE_MODEL_SPLIT) {
			splitq_ret = cpfl_rx_splitq_vec_default(rxq);
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
	struct idpf_tx_queue *txq;
	int ret = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		ret = cpfl_tx_vec_queue_default(txq);
		if (ret == CPFL_SCALAR_PATH)
			return CPFL_SCALAR_PATH;
	}

	return CPFL_VECTOR_PATH;
}

#endif /*_CPFL_RXTX_VEC_COMMON_H_*/
