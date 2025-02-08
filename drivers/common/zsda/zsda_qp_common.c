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
