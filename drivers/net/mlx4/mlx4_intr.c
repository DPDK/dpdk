/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 6WIND S.A.
 *   Copyright 2017 Mellanox
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

/**
 * @file
 * Interrupts handling for mlx4 driver.
 */

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

/* Verbs headers do not support -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_alarm.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_interrupts.h>

#include "mlx4.h"
#include "mlx4_rxtx.h"
#include "mlx4_utils.h"

static void mlx4_link_status_alarm(struct priv *priv);

/**
 * Clean up Rx interrupts handler.
 *
 * @param priv
 *   Pointer to private structure.
 */
static void
mlx4_rx_intr_vec_disable(struct priv *priv)
{
	struct rte_intr_handle *intr_handle = &priv->intr_handle;

	rte_intr_free_epoll_fd(intr_handle);
	free(intr_handle->intr_vec);
	intr_handle->nb_efd = 0;
	intr_handle->intr_vec = NULL;
}

/**
 * Allocate queue vector and fill epoll fd list for Rx interrupts.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_rx_intr_vec_enable(struct priv *priv)
{
	unsigned int i;
	unsigned int rxqs_n = priv->dev->data->nb_rx_queues;
	unsigned int n = RTE_MIN(rxqs_n, (uint32_t)RTE_MAX_RXTX_INTR_VEC_ID);
	unsigned int count = 0;
	struct rte_intr_handle *intr_handle = &priv->intr_handle;

	mlx4_rx_intr_vec_disable(priv);
	intr_handle->intr_vec = malloc(sizeof(intr_handle->intr_vec[rxqs_n]));
	if (intr_handle->intr_vec == NULL) {
		rte_errno = ENOMEM;
		ERROR("failed to allocate memory for interrupt vector,"
		      " Rx interrupts will not be supported");
		return -rte_errno;
	}
	for (i = 0; i != n; ++i) {
		struct rxq *rxq = priv->dev->data->rx_queues[i];

		/* Skip queues that cannot request interrupts. */
		if (!rxq || !rxq->channel) {
			/* Use invalid intr_vec[] index to disable entry. */
			intr_handle->intr_vec[i] =
				RTE_INTR_VEC_RXTX_OFFSET +
				RTE_MAX_RXTX_INTR_VEC_ID;
			continue;
		}
		if (count >= RTE_MAX_RXTX_INTR_VEC_ID) {
			rte_errno = E2BIG;
			ERROR("too many Rx queues for interrupt vector size"
			      " (%d), Rx interrupts cannot be enabled",
			      RTE_MAX_RXTX_INTR_VEC_ID);
			mlx4_rx_intr_vec_disable(priv);
			return -rte_errno;
		}
		intr_handle->intr_vec[i] = RTE_INTR_VEC_RXTX_OFFSET + count;
		intr_handle->efds[count] = rxq->channel->fd;
		count++;
	}
	if (!count)
		mlx4_rx_intr_vec_disable(priv);
	else
		intr_handle->nb_efd = count;
	return 0;
}

/**
 * Collect interrupt events.
 *
 * @param priv
 *   Pointer to private structure.
 * @param events
 *   Pointer to event flags holder.
 *
 * @return
 *   Number of events.
 */
static int
mlx4_collect_interrupt_events(struct priv *priv, uint32_t *events)
{
	struct ibv_async_event event;
	int port_change = 0;
	struct rte_eth_link *link = &priv->dev->data->dev_link;
	const struct rte_intr_conf *const intr_conf =
		&priv->dev->data->dev_conf.intr_conf;
	int ret = 0;

	*events = 0;
	/* Read all message and acknowledge them. */
	for (;;) {
		if (ibv_get_async_event(priv->ctx, &event))
			break;
		if ((event.event_type == IBV_EVENT_PORT_ACTIVE ||
		     event.event_type == IBV_EVENT_PORT_ERR) &&
		    intr_conf->lsc) {
			port_change = 1;
			ret++;
		} else if (event.event_type == IBV_EVENT_DEVICE_FATAL &&
			   intr_conf->rmv) {
			*events |= (1 << RTE_ETH_EVENT_INTR_RMV);
			ret++;
		} else {
			DEBUG("event type %d on port %d not handled",
			      event.event_type, event.element.port_num);
		}
		ibv_ack_async_event(&event);
	}
	if (!port_change)
		return ret;
	mlx4_link_update(priv->dev, 0);
	if (((link->link_speed == 0) && link->link_status) ||
	    ((link->link_speed != 0) && !link->link_status)) {
		if (!priv->intr_alarm) {
			/* Inconsistent status, check again later. */
			priv->intr_alarm = 1;
			rte_eal_alarm_set(MLX4_INTR_ALARM_TIMEOUT,
					  (void (*)(void *))
					  mlx4_link_status_alarm,
					  priv);
		}
	} else {
		*events |= (1 << RTE_ETH_EVENT_INTR_LSC);
	}
	return ret;
}

/**
 * Process scheduled link status check.
 *
 * @param priv
 *   Pointer to private structure.
 */
static void
mlx4_link_status_alarm(struct priv *priv)
{
	uint32_t events;
	int ret;

	assert(priv->intr_alarm == 1);
	priv->intr_alarm = 0;
	ret = mlx4_collect_interrupt_events(priv, &events);
	if (ret > 0 && events & (1 << RTE_ETH_EVENT_INTR_LSC))
		_rte_eth_dev_callback_process(priv->dev,
					      RTE_ETH_EVENT_INTR_LSC,
					      NULL, NULL);
}

/**
 * Handle interrupts from the NIC.
 *
 * @param priv
 *   Pointer to private structure.
 */
static void
mlx4_interrupt_handler(struct priv *priv)
{
	int ret;
	uint32_t ev;
	int i;

	ret = mlx4_collect_interrupt_events(priv, &ev);
	if (ret > 0) {
		for (i = RTE_ETH_EVENT_UNKNOWN;
		     i < RTE_ETH_EVENT_MAX;
		     i++) {
			if (ev & (1 << i)) {
				ev &= ~(1 << i);
				_rte_eth_dev_callback_process(priv->dev, i,
							      NULL, NULL);
				ret--;
			}
		}
		if (ret)
			WARN("%d event%s not processed", ret,
			     (ret > 1 ? "s were" : " was"));
	}
}

/**
 * Uninstall interrupt handler.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
mlx4_intr_uninstall(struct priv *priv)
{
	int err = rte_errno; /* Make sure rte_errno remains unchanged. */

	if (priv->intr_handle.fd != -1) {
		rte_intr_callback_unregister(&priv->intr_handle,
					     (void (*)(void *))
					     mlx4_interrupt_handler,
					     priv);
		priv->intr_handle.fd = -1;
	}
	rte_eal_alarm_cancel((void (*)(void *))mlx4_link_status_alarm, priv);
	priv->intr_alarm = 0;
	mlx4_rx_intr_vec_disable(priv);
	rte_errno = err;
	return 0;
}

/**
 * Install interrupt handler.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
mlx4_intr_install(struct priv *priv)
{
	const struct rte_intr_conf *const intr_conf =
		&priv->dev->data->dev_conf.intr_conf;
	int rc;

	mlx4_intr_uninstall(priv);
	if (intr_conf->rxq && mlx4_rx_intr_vec_enable(priv) < 0)
		goto error;
	if (intr_conf->lsc | intr_conf->rmv) {
		priv->intr_handle.fd = priv->ctx->async_fd;
		rc = rte_intr_callback_register(&priv->intr_handle,
						(void (*)(void *))
						mlx4_interrupt_handler,
						priv);
		if (rc < 0) {
			rte_errno = -rc;
			goto error;
		}
	}
	return 0;
error:
	mlx4_intr_uninstall(priv);
	return -rte_errno;
}

/**
 * DPDK callback for Rx queue interrupt disable.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   Rx queue index.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
mlx4_rx_intr_disable(struct rte_eth_dev *dev, uint16_t idx)
{
	struct rxq *rxq = dev->data->rx_queues[idx];
	struct ibv_cq *ev_cq;
	void *ev_ctx;
	int ret;

	if (!rxq || !rxq->channel) {
		ret = EINVAL;
	} else {
		ret = ibv_get_cq_event(rxq->cq->channel, &ev_cq, &ev_ctx);
		if (ret || ev_cq != rxq->cq)
			ret = EINVAL;
	}
	if (ret) {
		rte_errno = ret;
		WARN("unable to disable interrupt on rx queue %d",
		     idx);
	} else {
		ibv_ack_cq_events(rxq->cq, 1);
	}
	return -ret;
}

/**
 * DPDK callback for Rx queue interrupt enable.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   Rx queue index.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
mlx4_rx_intr_enable(struct rte_eth_dev *dev, uint16_t idx)
{
	struct rxq *rxq = dev->data->rx_queues[idx];
	int ret;

	if (!rxq || !rxq->channel)
		ret = EINVAL;
	else
		ret = ibv_req_notify_cq(rxq->cq, 0);
	if (ret) {
		rte_errno = ret;
		WARN("unable to arm interrupt on rx queue %d", idx);
	}
	return -ret;
}
