/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2025 NVIDIA Corporation & Affiliates
 */

#include "mlx5_driver_event.h"

#include <sys/queue.h>

#include <eal_export.h>

#include "mlx5.h"
#include "mlx5_rx.h"
#include "mlx5_tx.h"
#include "rte_pmd_mlx5.h"

/*
 * Macro serving as a longest possible "queue_info" string generated as part of driver event
 * callback. Used to derive the correct size of the static buffer, so the dynamic allocation
 * can be skipped during callback.
 */
#define MAX_QUEUE_INFO ( \
	"lro_timeout=" RTE_STR(UINT32_MAX) "," \
	"max_lro_msg_size=" RTE_STR(UINT32_MAX) "," \
	"td=" RTE_STR(UINT32_MAX) "," \
	"lpbk=1")

static char queue_info_buf[sizeof(MAX_QUEUE_INFO)];

struct registered_cb {
	LIST_ENTRY(registered_cb) list;
	rte_pmd_mlx5_driver_event_callback_t cb;
	const void *opaque;
};

LIST_HEAD(, registered_cb) cb_list_head = LIST_HEAD_INITIALIZER(cb_list_head);

static const char *
generate_rx_queue_info(struct mlx5_rxq_priv *rxq)
{
	struct mlx5_priv *priv = rxq->priv;
	uint32_t max_lro_msg_size = 0;
	uint32_t lro_timeout = 0;
	uint32_t lpbk = 0;
	uint32_t td = 0;
	int ret __rte_unused;

	if (rxq->ctrl->rxq.lro) {
		lro_timeout = priv->config.lro_timeout;
		max_lro_msg_size = priv->max_lro_msg_size / MLX5_LRO_SEG_CHUNK_SIZE;
	}

	if (rxq->ctrl->is_hairpin)
		td = priv->sh->td->id;
	else
		td = priv->sh->tdn;

	lpbk = !!priv->dev_data->dev_conf.lpbk_mode;

	ret = snprintf(queue_info_buf, sizeof(queue_info_buf),
		 "lro_timeout=%u,max_lro_msg_size=%u,td=%u,lpbk=%u",
		 lro_timeout, max_lro_msg_size, td, lpbk);
	/*
	 * queue_info_buf is set up to accommodate maximum possible values.
	 * As a result, snprintf should always succeed here.
	 */
	MLX5_ASSERT(ret >= 0);

	return queue_info_buf;
}

static void
fill_rxq_info(struct mlx5_rxq_priv *rxq,
	      struct rte_pmd_mlx5_driver_event_cb_queue_info *queue,
	      enum rte_pmd_mlx5_driver_event_cb_type event)
{
	/* It is assumed that port is started so all control structs should be initialized. */
	MLX5_ASSERT(rxq != NULL);
	MLX5_ASSERT(rxq->ctrl != NULL);

	queue->dpdk_queue_id = rxq->idx;
	if (rxq->ctrl->is_hairpin) {
		MLX5_ASSERT(rxq->ctrl->obj != NULL && rxq->ctrl->obj->rq != NULL);
		queue->hw_queue_id = rxq->ctrl->obj->rq->id;
	} else {
		MLX5_ASSERT(rxq->devx_rq.rq != NULL);
		queue->hw_queue_id = rxq->devx_rq.rq->id;
	}
	if (event == RTE_PMD_MLX5_DRIVER_EVENT_CB_TYPE_RXQ_CREATE)
		queue->queue_info = generate_rx_queue_info(rxq);
}

static void
notify_rxq_event(struct mlx5_rxq_priv *rxq,
		 enum rte_pmd_mlx5_driver_event_cb_type event)
{
	struct rte_pmd_mlx5_driver_event_cb_info cb_info = {
		.event = event,
	};
	struct registered_cb *r;
	uint16_t port_id;

	MLX5_ASSERT(rxq != NULL);
	MLX5_ASSERT(event == RTE_PMD_MLX5_DRIVER_EVENT_CB_TYPE_RXQ_CREATE ||
		    event == RTE_PMD_MLX5_DRIVER_EVENT_CB_TYPE_RXQ_DESTROY);

	if (LIST_EMPTY(&cb_list_head))
		return;

	port_id = rxq->priv->dev_data->port_id;
	fill_rxq_info(rxq, &cb_info.queue, event);

	LIST_FOREACH(r, &cb_list_head, list)
		r->cb(port_id, &cb_info, r->opaque);
}

void
mlx5_driver_event_notify_rxq_create(struct mlx5_rxq_priv *rxq)
{
	notify_rxq_event(rxq, RTE_PMD_MLX5_DRIVER_EVENT_CB_TYPE_RXQ_CREATE);
}

void
mlx5_driver_event_notify_rxq_destroy(struct mlx5_rxq_priv *rxq)
{
	notify_rxq_event(rxq, RTE_PMD_MLX5_DRIVER_EVENT_CB_TYPE_RXQ_DESTROY);
}

static void
fill_txq_info(struct mlx5_txq_ctrl *txq_ctrl,
	      struct rte_pmd_mlx5_driver_event_cb_queue_info *queue)
{
	/* It is assumed that port is started so all control structs should be initialized. */
	MLX5_ASSERT(txq_ctrl != NULL);
	MLX5_ASSERT(txq_ctrl->obj != NULL);

	queue->dpdk_queue_id = txq_ctrl->txq.idx;
	if (txq_ctrl->is_hairpin) {
		MLX5_ASSERT(txq_ctrl->obj->sq != NULL);
		queue->hw_queue_id = txq_ctrl->obj->sq->id;
	} else {
		MLX5_ASSERT(txq_ctrl->obj->sq_obj.sq != NULL);
		queue->hw_queue_id = txq_ctrl->obj->sq_obj.sq->id;
	}
}

static void
notify_txq_event(struct mlx5_txq_ctrl *txq_ctrl,
		 enum rte_pmd_mlx5_driver_event_cb_type event)
{
	struct rte_pmd_mlx5_driver_event_cb_info cb_info = {
		.event = event,
	};
	struct registered_cb *r;
	uint16_t port_id;

	MLX5_ASSERT(txq_ctrl != NULL);
	MLX5_ASSERT(event == RTE_PMD_MLX5_DRIVER_EVENT_CB_TYPE_TXQ_CREATE ||
		    event == RTE_PMD_MLX5_DRIVER_EVENT_CB_TYPE_TXQ_DESTROY);

	if (LIST_EMPTY(&cb_list_head))
		return;

	port_id = txq_ctrl->priv->dev_data->port_id;
	fill_txq_info(txq_ctrl, &cb_info.queue);

	LIST_FOREACH(r, &cb_list_head, list)
		r->cb(port_id, &cb_info, r->opaque);
}

void
mlx5_driver_event_notify_txq_create(struct mlx5_txq_ctrl *txq_ctrl)
{
	notify_txq_event(txq_ctrl, RTE_PMD_MLX5_DRIVER_EVENT_CB_TYPE_TXQ_CREATE);
}

void
mlx5_driver_event_notify_txq_destroy(struct mlx5_txq_ctrl *txq_ctrl)
{
	notify_txq_event(txq_ctrl, RTE_PMD_MLX5_DRIVER_EVENT_CB_TYPE_TXQ_DESTROY);
}

static void
notify_existing_queues(uint16_t port_id,
		       rte_pmd_mlx5_driver_event_callback_t cb,
		       void *opaque)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct mlx5_priv *priv = (struct mlx5_priv *)dev->data->dev_private;
	unsigned int i;

	/* Stopped port does not have any queues. */
	if (!dev->data->dev_started)
		return;

	for (i = 0; i < priv->rxqs_n; ++i) {
		struct mlx5_rxq_priv *rxq = mlx5_rxq_get(dev, i);
		struct rte_pmd_mlx5_driver_event_cb_info cb_info = {
			.event = RTE_PMD_MLX5_DRIVER_EVENT_CB_TYPE_RXQ_CREATE,
		};

		/* Port is started and only known queues are iterated on. All should be there. */
		MLX5_ASSERT(rxq != NULL);

		fill_rxq_info(rxq, &cb_info.queue, cb_info.event);
		cb(port_id, &cb_info, opaque);
	}

	for (i = 0; i < priv->txqs_n; ++i) {
		struct mlx5_txq_ctrl *txq_ctrl = mlx5_txq_get(dev, i);
		struct rte_pmd_mlx5_driver_event_cb_info cb_info = {
			.event = RTE_PMD_MLX5_DRIVER_EVENT_CB_TYPE_TXQ_CREATE,
		};

		/* Port is started and only known queues are iterated on. All should be there. */
		MLX5_ASSERT(txq_ctrl != NULL);

		fill_txq_info(txq_ctrl, &cb_info.queue);
		cb(port_id, &cb_info, opaque);

		/* mlx5_txq_get() increments a ref count on Tx queue. Need to decrement. */
		mlx5_txq_release(dev, i);
	}
}

static void
notify_existing_devices(rte_pmd_mlx5_driver_event_callback_t cb, void *opaque)
{
	uint16_t port_id;

	/*
	 * Whenever there is at least one available port,
	 * it means that EAL was initialized and ports were probed.
	 * Logging library should be available, so it is safe to use DRV_LOG.
	 */
	MLX5_ETH_FOREACH_DEV(port_id, NULL)
		notify_existing_queues(port_id, cb, opaque);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_mlx5_driver_event_cb_register, 25.11)
int
rte_pmd_mlx5_driver_event_cb_register(rte_pmd_mlx5_driver_event_callback_t cb, void *opaque)
{
	struct registered_cb *r;

	if (cb == NULL)
		return -EINVAL;

	LIST_FOREACH(r, &cb_list_head, list) {
		if (r->cb == cb)
			return -EEXIST;
	}

	r = calloc(1, sizeof(*r));
	if (r == NULL)
		return -ENOMEM;

	r->cb = cb;
	r->opaque = opaque;

	notify_existing_devices(cb, opaque);

	LIST_INSERT_HEAD(&cb_list_head, r, list);

	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_mlx5_driver_event_cb_unregister, 25.11)
int
rte_pmd_mlx5_driver_event_cb_unregister(rte_pmd_mlx5_driver_event_callback_t cb)
{
	struct registered_cb *r;
	bool found = false;

	if (cb == NULL)
		return -EINVAL;

	LIST_FOREACH(r, &cb_list_head, list) {
		if (r->cb == cb) {
			found = true;
			break;
		}
	}
	if (!found)
		return 0;

	LIST_REMOVE(r, list);
	free(r);

	return 0;
}

RTE_FINI(rte_pmd_mlx5_driver_event_cb_cleanup) {
	struct registered_cb *r;

	while (!LIST_EMPTY(&cb_list_head)) {
		r = LIST_FIRST(&cb_list_head);
		LIST_REMOVE(r, list);
		free(r);
	}
}
