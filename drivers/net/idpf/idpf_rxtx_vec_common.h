/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef _IDPF_RXTX_VEC_COMMON_H_
#define _IDPF_RXTX_VEC_COMMON_H_
#include <stdint.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>

#include "idpf_ethdev.h"
#include "idpf_rxtx.h"

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

#define IDPF_SCALAR_PATH		0
#define IDPF_VECTOR_PATH		1
#define IDPF_RX_NO_VECTOR_FLAGS (		\
		RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |	\
		RTE_ETH_RX_OFFLOAD_UDP_CKSUM |	\
		RTE_ETH_RX_OFFLOAD_TCP_CKSUM |	\
		RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM |	\
		RTE_ETH_RX_OFFLOAD_TIMESTAMP)
#define IDPF_TX_NO_VECTOR_FLAGS (		\
		RTE_ETH_TX_OFFLOAD_TCP_TSO |	\
		RTE_ETH_TX_OFFLOAD_MULTI_SEGS |	\
		RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |		\
		RTE_ETH_TX_OFFLOAD_SCTP_CKSUM |		\
		RTE_ETH_TX_OFFLOAD_UDP_CKSUM |	\
		RTE_ETH_TX_OFFLOAD_TCP_CKSUM)

static inline int
idpf_rx_vec_queue_default(struct idpf_rx_queue *rxq)
{
	if (rxq == NULL)
		return IDPF_SCALAR_PATH;

	if (rte_is_power_of_2(rxq->nb_rx_desc) == 0)
		return IDPF_SCALAR_PATH;

	if (rxq->rx_free_thresh < IDPF_VPMD_RX_MAX_BURST)
		return IDPF_SCALAR_PATH;

	if ((rxq->nb_rx_desc % rxq->rx_free_thresh) != 0)
		return IDPF_SCALAR_PATH;

	if ((rxq->offloads & IDPF_RX_NO_VECTOR_FLAGS) != 0)
		return IDPF_SCALAR_PATH;

	return IDPF_VECTOR_PATH;
}

static inline int
idpf_tx_vec_queue_default(struct idpf_tx_queue *txq)
{
	if (txq == NULL)
		return IDPF_SCALAR_PATH;

	if (txq->rs_thresh < IDPF_VPMD_TX_MAX_BURST ||
	    (txq->rs_thresh & 3) != 0)
		return IDPF_SCALAR_PATH;

	if ((txq->offloads & IDPF_TX_NO_VECTOR_FLAGS) != 0)
		return IDPF_SCALAR_PATH;

	return IDPF_VECTOR_PATH;
}

static inline int
idpf_rx_splitq_vec_default(struct idpf_rx_queue *rxq)
{
	if (rxq->bufq2->rx_buf_len < rxq->max_pkt_len)
		return IDPF_SCALAR_PATH;

	return IDPF_VECTOR_PATH;
}

static inline int
idpf_rx_vec_dev_check_default(struct rte_eth_dev *dev)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_rx_queue *rxq;
	int i, default_ret, splitq_ret, ret = IDPF_SCALAR_PATH;

	if (dev->data->scattered_rx)
		return IDPF_SCALAR_PATH;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		default_ret = idpf_rx_vec_queue_default(rxq);
		if (vport->rxq_model == VIRTCHNL2_QUEUE_MODEL_SPLIT) {
			splitq_ret = idpf_rx_splitq_vec_default(rxq);
			ret = splitq_ret && default_ret;
		} else {
			ret = default_ret;
		}
		if (ret == IDPF_SCALAR_PATH)
			return IDPF_SCALAR_PATH;
	}

	return IDPF_VECTOR_PATH;
}

static inline int
idpf_tx_vec_dev_check_default(struct rte_eth_dev *dev)
{
	int i;
	struct idpf_tx_queue *txq;
	int ret = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		ret = idpf_tx_vec_queue_default(txq);
		if (ret == IDPF_SCALAR_PATH)
			return IDPF_SCALAR_PATH;
	}

	return IDPF_VECTOR_PATH;
}

#endif /*_IDPF_RXTX_VEC_COMMON_H_*/
