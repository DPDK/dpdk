/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Cesnet
 * Copyright(c) 2019 Netcope Technologies, a.s. <info@netcope.com>
 * All rights reserved.
 */

#include <rte_kvargs.h>

#include "nfb_rx.h"
#include "nfb.h"

uint64_t nfb_timestamp_rx_dynflag;
int nfb_timestamp_dynfield_offset = -1;

int
nfb_eth_rx_queue_start(struct rte_eth_dev *dev, uint16_t rxq_id)
{
	struct ndp_rx_queue *rxq = dev->data->rx_queues[rxq_id];
	int ret;

	if (rxq->queue == NULL) {
		RTE_LOG(ERR, PMD, "RX NDP queue is NULL!\n");
		return -EINVAL;
	}

	ret = ndp_queue_start(rxq->queue);
	if (ret != 0)
		goto err;
	dev->data->rx_queue_state[rxq_id] = RTE_ETH_QUEUE_STATE_STARTED;
	return 0;

err:
	return -EINVAL;
}

int
nfb_eth_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rxq_id)
{
	struct ndp_rx_queue *rxq = dev->data->rx_queues[rxq_id];
	int ret;

	if (rxq->queue == NULL) {
		RTE_LOG(ERR, PMD, "RX NDP queue is NULL!\n");
		return -EINVAL;
	}

	ret = ndp_queue_stop(rxq->queue);
	if (ret != 0)
		return -EINVAL;

	dev->data->rx_queue_state[rxq_id] = RTE_ETH_QUEUE_STATE_STOPPED;
	return 0;
}

int
nfb_eth_rx_queue_setup(struct rte_eth_dev *dev,
		uint16_t rx_queue_id,
		uint16_t nb_rx_desc __rte_unused,
		unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf __rte_unused,
		struct rte_mempool *mb_pool)
{
	struct pmd_internals *internals = dev->data->dev_private;

	struct ndp_rx_queue *rxq;
	int ret;

	rxq = rte_zmalloc_socket("ndp rx queue",
			sizeof(struct ndp_rx_queue),
			RTE_CACHE_LINE_SIZE, socket_id);

	if (rxq == NULL) {
		RTE_LOG(ERR, PMD, "rte_zmalloc_socket() failed for rx queue id "
				"%" PRIu16 "!\n", rx_queue_id);
		return -ENOMEM;
	}

	rxq->flags = 0;

	ret = nfb_eth_rx_queue_init(internals->nfb,
		rx_queue_id,
		dev->data->port_id,
		mb_pool,
		rxq);

	if (ret == 0)
		dev->data->rx_queues[rx_queue_id] = rxq;
	else
		rte_free(rxq);

	return ret;
}

int
nfb_eth_rx_queue_init(struct nfb_device *nfb,
		uint16_t rx_queue_id,
		uint16_t port_id,
		struct rte_mempool *mb_pool,
		struct ndp_rx_queue *rxq)
{
	const struct rte_pktmbuf_pool_private *mbp_priv =
		rte_mempool_get_priv(mb_pool);

	if (nfb == NULL)
		return -EINVAL;

	rxq->queue = ndp_open_rx_queue(nfb, rx_queue_id);
	if (rxq->queue == NULL)
		return -EINVAL;

	rxq->nfb = nfb;
	rxq->rx_queue_id = rx_queue_id;
	rxq->in_port = port_id;
	rxq->mb_pool = mb_pool;
	rxq->buf_size = (uint16_t)(mbp_priv->mbuf_data_room_size -
		RTE_PKTMBUF_HEADROOM);

	rxq->rx_pkts = 0;
	rxq->rx_bytes = 0;
	rxq->err_pkts = 0;

	return 0;
}

void
nfb_eth_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	struct ndp_rx_queue *rxq = dev->data->rx_queues[qid];

	if (rxq->queue != NULL) {
		ndp_close_rx_queue(rxq->queue);
		rte_free(rxq);
		rxq->queue = NULL;
	}
}
