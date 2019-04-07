/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Cesnet
 * Copyright(c) 2019 Netcope Technologies, a.s. <info@netcope.com>
 * All rights reserved.
 */

#include "nfb_tx.h"
#include "nfb.h"

int
nfb_eth_tx_queue_start(struct rte_eth_dev *dev, uint16_t txq_id)
{
	struct ndp_tx_queue *txq = dev->data->tx_queues[txq_id];
	int ret;

	if (txq->queue == NULL) {
		RTE_LOG(ERR, PMD, "RX NDP queue is NULL!\n");
		return -EINVAL;
	}

	ret = ndp_queue_start(txq->queue);
	if (ret != 0)
		goto err;
	dev->data->tx_queue_state[txq_id] = RTE_ETH_QUEUE_STATE_STARTED;
	return 0;

err:
	return -EINVAL;
}

int
nfb_eth_tx_queue_stop(struct rte_eth_dev *dev, uint16_t txq_id)
{
	struct ndp_tx_queue *txq = dev->data->tx_queues[txq_id];
	int ret;

	if (txq->queue == NULL) {
		RTE_LOG(ERR, PMD, "TX NDP queue is NULL!\n");
		return -EINVAL;
	}

	ret = ndp_queue_stop(txq->queue);
	if (ret != 0)
		return -EINVAL;
	dev->data->tx_queue_state[txq_id] = RTE_ETH_QUEUE_STATE_STOPPED;
	return 0;
}

int
nfb_eth_tx_queue_setup(struct rte_eth_dev *dev,
	uint16_t tx_queue_id,
	uint16_t nb_tx_desc __rte_unused,
	unsigned int socket_id,
	const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct pmd_internals *internals = dev->data->dev_private;
	int ret;
	struct ndp_tx_queue *txq;

	txq = rte_zmalloc_socket("ndp tx queue",
		sizeof(struct ndp_tx_queue),
		RTE_CACHE_LINE_SIZE, socket_id);

	if (txq == NULL) {
		RTE_LOG(ERR, PMD, "rte_zmalloc_socket() failed for tx queue id "
			"%" PRIu16 "!\n", tx_queue_id);
		return -ENOMEM;
	}

	ret = nfb_eth_tx_queue_init(internals->nfb,
		tx_queue_id,
		txq);

	if (ret == 0)
		dev->data->tx_queues[tx_queue_id] = txq;
	else
		rte_free(txq);

	return ret;
}

int
nfb_eth_tx_queue_init(struct nfb_device *nfb,
	uint16_t tx_queue_id,
	struct ndp_tx_queue *txq)
{
	if (nfb == NULL)
		return -EINVAL;

	txq->queue = ndp_open_tx_queue(nfb, tx_queue_id);
	if (txq->queue == NULL)
		return -EINVAL;

	txq->nfb = nfb;
	txq->tx_queue_id = tx_queue_id;

	txq->tx_pkts = 0;
	txq->tx_bytes = 0;
	txq->err_pkts = 0;

	return 0;
}

void
nfb_eth_tx_queue_release(void *q)
{
	struct ndp_tx_queue *txq = (struct ndp_tx_queue *)q;
	if (txq->queue != NULL) {
		ndp_close_tx_queue(txq->queue);
		rte_free(txq);
		txq->queue = NULL;
	}
}
