/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#include "zsda_qp_common.h"

void
zsda_stats_get(void **queue_pairs, const uint32_t nb_queue_pairs,
	      struct zsda_qp_stat *stats)
{
	enum zsda_service_type type;
	uint32_t i;
	struct zsda_qp *qp;

	if ((stats == NULL) || (queue_pairs == NULL)) {
		ZSDA_LOG(ERR, "Failed! stats or queue_pairs is NULL");
		return;
	}

	for (i = 0; i < nb_queue_pairs; i++) {
		qp = queue_pairs[i];

		if (qp == NULL) {
			ZSDA_LOG(ERR, "Failed! queue_pairs[i] is NULL");
			break;
		}

		for (type = 0; type < ZSDA_SERVICE_INVALID; type++) {
			if (qp->srv[type].used) {
				stats->enqueued_count +=
					qp->srv[type].stats.enqueued_count;
				stats->dequeued_count +=
					qp->srv[type].stats.dequeued_count;
				stats->enqueue_err_count +=
					qp->srv[type].stats.enqueue_err_count;
				stats->dequeue_err_count +=
					qp->srv[type].stats.dequeue_err_count;
			}
		}
	}
}

void
zsda_stats_reset(void **queue_pairs, const uint32_t nb_queue_pairs)
{
	enum zsda_service_type type;
	uint32_t i;
	struct zsda_qp *qp;

	if (queue_pairs == NULL) {
		ZSDA_LOG(ERR, "Failed! queue_pairs is NULL");
		return;
	}

	for (i = 0; i < nb_queue_pairs; i++) {
		qp = queue_pairs[i];

		if (qp == NULL) {
			ZSDA_LOG(ERR, "Failed! queue_pairs[i] is NULL");
			break;
		}
		for (type = 0; type < ZSDA_MAX_SERVICES; type++) {
			if (qp->srv[type].used)
				memset(&(qp->srv[type].stats), 0,
				       sizeof(struct zsda_qp_stat));
		}
	}
}

void
zsda_queue_delete(const struct zsda_queue *queue)
{
	const struct rte_memzone *mz;

	if (queue == NULL) {
		ZSDA_LOG(DEBUG, "Invalid queue");
		return;
	}

	mz = rte_memzone_lookup(queue->memz_name);
	if (mz != NULL) {
		memset(queue->base_addr, 0x0,
		       (uint16_t)(queue->queue_size * queue->msg_size));
		rte_memzone_free(mz);
	} else
		ZSDA_LOG(DEBUG, "queue %s doesn't exist", queue->memz_name);
}

int
zsda_queue_pair_release(struct zsda_qp **qp_addr)
{
	struct zsda_qp *qp = *qp_addr;
	uint32_t i;
	enum zsda_service_type type;

	if (qp == NULL) {
		ZSDA_LOG(DEBUG, "qp already freed");
		return 0;
	}

	for (type = 0; type < ZSDA_SERVICE_INVALID; type++) {
		if (!qp->srv[type].used)
			continue;

		zsda_queue_delete(&(qp->srv[type].tx_q));
		zsda_queue_delete(&(qp->srv[type].rx_q));
		qp->srv[type].used = false;
		for (i = 0; i < qp->srv[type].nb_descriptors; i++)
			rte_mempool_put(qp->srv[type].op_cookie_pool,
					qp->srv[type].op_cookies[i]);

		rte_mempool_free(qp->srv[type].op_cookie_pool);
		rte_free(qp->srv[type].op_cookies);
	}

	rte_free(qp);
	*qp_addr = NULL;

	return ZSDA_SUCCESS;
}
