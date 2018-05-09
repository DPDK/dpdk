/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2017 NXP
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/epoll.h>

#include <rte_atomic.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_debug.h>
#include <rte_dev.h>
#include <rte_eal.h>
#include <rte_fslmc.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_memory.h>
#include <rte_pci.h>
#include <rte_bus_vdev.h>
#include <rte_ethdev_driver.h>
#include <rte_event_eth_rx_adapter.h>

#include <fslmc_vfio.h>
#include <dpaa2_hw_pvt.h>
#include <dpaa2_hw_mempool.h>
#include <dpaa2_hw_dpio.h>
#include <dpaa2_ethdev.h>
#include "dpaa2_eventdev.h"
#include "dpaa2_eventdev_logs.h"
#include <portal/dpaa2_hw_pvt.h>
#include <mc/fsl_dpci.h>

/* Clarifications
 * Evendev = SoC Instance
 * Eventport = DPIO Instance
 * Eventqueue = DPCON Instance
 * 1 Eventdev can have N Eventqueue
 * Soft Event Flow is DPCI Instance
 */

/* Dynamic logging identified for mempool */
int dpaa2_logtype_event;

static uint16_t
dpaa2_eventdev_enqueue_burst(void *port, const struct rte_event ev[],
			     uint16_t nb_events)
{
	struct rte_eventdev *ev_dev =
			((struct dpaa2_io_portal_t *)port)->eventdev;
	struct dpaa2_eventdev *priv = ev_dev->data->dev_private;
	uint32_t queue_id = ev[0].queue_id;
	struct evq_info_t *evq_info = &priv->evq_info[queue_id];
	uint32_t fqid;
	struct qbman_swp *swp;
	struct qbman_fd fd_arr[MAX_TX_RING_SLOTS];
	uint32_t loop, frames_to_send;
	struct qbman_eq_desc eqdesc[MAX_TX_RING_SLOTS];
	uint16_t num_tx = 0;
	int ret;

	RTE_SET_USED(port);

	if (unlikely(!DPAA2_PER_LCORE_DPIO)) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			DPAA2_EVENTDEV_ERR("Failure in affining portal");
			return 0;
		}
	}

	swp = DPAA2_PER_LCORE_PORTAL;

	while (nb_events) {
		frames_to_send = (nb_events >> 3) ?
			MAX_TX_RING_SLOTS : nb_events;

		for (loop = 0; loop < frames_to_send; loop++) {
			const struct rte_event *event = &ev[num_tx + loop];

			if (event->sched_type != RTE_SCHED_TYPE_ATOMIC)
				fqid = evq_info->dpci->rx_queue[
					DPAA2_EVENT_DPCI_PARALLEL_QUEUE].fqid;
			else
				fqid = evq_info->dpci->rx_queue[
					DPAA2_EVENT_DPCI_ATOMIC_QUEUE].fqid;

			/* Prepare enqueue descriptor */
			qbman_eq_desc_clear(&eqdesc[loop]);
			qbman_eq_desc_set_fq(&eqdesc[loop], fqid);
			qbman_eq_desc_set_no_orp(&eqdesc[loop], 0);
			qbman_eq_desc_set_response(&eqdesc[loop], 0, 0);

			if (event->mbuf->seqn) {
				uint8_t dqrr_index = event->mbuf->seqn - 1;

				qbman_eq_desc_set_dca(&eqdesc[loop], 1,
						      dqrr_index, 0);
				DPAA2_PER_LCORE_DQRR_SIZE--;
				DPAA2_PER_LCORE_DQRR_HELD &=
					~(1 << dqrr_index);
			}

			memset(&fd_arr[loop], 0, sizeof(struct qbman_fd));

			/*
			 * todo - need to align with hw context data
			 * to avoid copy
			 */
			struct rte_event *ev_temp = rte_malloc(NULL,
				sizeof(struct rte_event), 0);

			if (!ev_temp) {
				if (!loop)
					return num_tx;
				frames_to_send = loop;
				DPAA2_EVENTDEV_ERR(
					"Unable to allocate event object");
				goto send_partial;
			}
			rte_memcpy(ev_temp, event, sizeof(struct rte_event));
			DPAA2_SET_FD_ADDR((&fd_arr[loop]), (size_t)ev_temp);
			DPAA2_SET_FD_LEN((&fd_arr[loop]),
					 sizeof(struct rte_event));
		}
send_partial:
		loop = 0;
		while (loop < frames_to_send) {
			loop += qbman_swp_enqueue_multiple_desc(swp,
					&eqdesc[loop], &fd_arr[loop],
					frames_to_send - loop);
		}
		num_tx += frames_to_send;
		nb_events -= frames_to_send;
	}

	return num_tx;
}

static uint16_t
dpaa2_eventdev_enqueue(void *port, const struct rte_event *ev)
{
	return dpaa2_eventdev_enqueue_burst(port, ev, 1);
}

static void dpaa2_eventdev_dequeue_wait(uint64_t timeout_ticks)
{
	struct epoll_event epoll_ev;

	qbman_swp_interrupt_clear_status(DPAA2_PER_LCORE_PORTAL,
					 QBMAN_SWP_INTERRUPT_DQRI);

	epoll_wait(DPAA2_PER_LCORE_DPIO->epoll_fd,
			 &epoll_ev, 1, timeout_ticks);
}

static void dpaa2_eventdev_process_parallel(struct qbman_swp *swp,
					    const struct qbman_fd *fd,
					    const struct qbman_result *dq,
					    struct dpaa2_queue *rxq,
					    struct rte_event *ev)
{
	struct rte_event *ev_temp =
		(struct rte_event *)(size_t)DPAA2_GET_FD_ADDR(fd);

	RTE_SET_USED(rxq);

	rte_memcpy(ev, ev_temp, sizeof(struct rte_event));
	rte_free(ev_temp);

	qbman_swp_dqrr_consume(swp, dq);
}

static void dpaa2_eventdev_process_atomic(struct qbman_swp *swp,
					  const struct qbman_fd *fd,
					  const struct qbman_result *dq,
					  struct dpaa2_queue *rxq,
					  struct rte_event *ev)
{
	struct rte_event *ev_temp =
		(struct rte_event *)(size_t)DPAA2_GET_FD_ADDR(fd);
	uint8_t dqrr_index = qbman_get_dqrr_idx(dq);

	RTE_SET_USED(swp);
	RTE_SET_USED(rxq);

	rte_memcpy(ev, ev_temp, sizeof(struct rte_event));
	rte_free(ev_temp);
	ev->mbuf->seqn = dqrr_index + 1;
	DPAA2_PER_LCORE_DQRR_SIZE++;
	DPAA2_PER_LCORE_DQRR_HELD |= 1 << dqrr_index;
}

static uint16_t
dpaa2_eventdev_dequeue_burst(void *port, struct rte_event ev[],
			     uint16_t nb_events, uint64_t timeout_ticks)
{
	const struct qbman_result *dq;
	struct qbman_swp *swp;
	const struct qbman_fd *fd;
	struct dpaa2_queue *rxq;
	int num_pkts = 0, ret, i = 0;

	RTE_SET_USED(port);

	if (unlikely(!DPAA2_PER_LCORE_DPIO)) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			DPAA2_EVENTDEV_ERR("Failure in affining portal");
			return 0;
		}
	}
	swp = DPAA2_PER_LCORE_PORTAL;

	/* Check if there are atomic contexts to be released */
	while (DPAA2_PER_LCORE_DQRR_SIZE) {
		if (DPAA2_PER_LCORE_DQRR_HELD & (1 << i)) {
			qbman_swp_dqrr_idx_consume(swp, i);
			DPAA2_PER_LCORE_DQRR_SIZE--;
			DPAA2_PER_LCORE_DQRR_MBUF(i)->seqn =
				DPAA2_INVALID_MBUF_SEQN;
		}
		i++;
	}
	DPAA2_PER_LCORE_DQRR_HELD = 0;

	do {
		dq = qbman_swp_dqrr_next(swp);
		if (!dq) {
			if (!num_pkts && timeout_ticks) {
				dpaa2_eventdev_dequeue_wait(timeout_ticks);
				timeout_ticks = 0;
				continue;
			}
			return num_pkts;
		}
		qbman_swp_prefetch_dqrr_next(swp);

		fd = qbman_result_DQ_fd(dq);
		rxq = (struct dpaa2_queue *)(size_t)qbman_result_DQ_fqd_ctx(dq);
		if (rxq) {
			rxq->cb(swp, fd, dq, rxq, &ev[num_pkts]);
		} else {
			qbman_swp_dqrr_consume(swp, dq);
			DPAA2_EVENTDEV_ERR("Null Return VQ received");
			return 0;
		}

		num_pkts++;
	} while (num_pkts < nb_events);

	return num_pkts;
}

static uint16_t
dpaa2_eventdev_dequeue(void *port, struct rte_event *ev,
		       uint64_t timeout_ticks)
{
	return dpaa2_eventdev_dequeue_burst(port, ev, 1, timeout_ticks);
}

static void
dpaa2_eventdev_info_get(struct rte_eventdev *dev,
			struct rte_event_dev_info *dev_info)
{
	struct dpaa2_eventdev *priv = dev->data->dev_private;

	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);

	memset(dev_info, 0, sizeof(struct rte_event_dev_info));
	dev_info->min_dequeue_timeout_ns =
		DPAA2_EVENT_MIN_DEQUEUE_TIMEOUT;
	dev_info->max_dequeue_timeout_ns =
		DPAA2_EVENT_MAX_DEQUEUE_TIMEOUT;
	dev_info->dequeue_timeout_ns =
		DPAA2_EVENT_MIN_DEQUEUE_TIMEOUT;
	dev_info->max_event_queues = priv->max_event_queues;
	dev_info->max_event_queue_flows =
		DPAA2_EVENT_MAX_QUEUE_FLOWS;
	dev_info->max_event_queue_priority_levels =
		DPAA2_EVENT_MAX_QUEUE_PRIORITY_LEVELS;
	dev_info->max_event_priority_levels =
		DPAA2_EVENT_MAX_EVENT_PRIORITY_LEVELS;
	dev_info->max_event_ports = rte_fslmc_get_device_count(DPAA2_IO);
	dev_info->max_event_port_dequeue_depth =
		DPAA2_EVENT_MAX_PORT_DEQUEUE_DEPTH;
	dev_info->max_event_port_enqueue_depth =
		DPAA2_EVENT_MAX_PORT_ENQUEUE_DEPTH;
	dev_info->max_num_events = DPAA2_EVENT_MAX_NUM_EVENTS;
	dev_info->event_dev_cap = RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED |
		RTE_EVENT_DEV_CAP_BURST_MODE|
		RTE_EVENT_DEV_CAP_RUNTIME_PORT_LINK |
		RTE_EVENT_DEV_CAP_MULTIPLE_QUEUE_PORT |
		RTE_EVENT_DEV_CAP_NONSEQ_MODE;

}

static int
dpaa2_eventdev_configure(const struct rte_eventdev *dev)
{
	struct dpaa2_eventdev *priv = dev->data->dev_private;
	struct rte_event_dev_config *conf = &dev->data->dev_conf;

	EVENTDEV_INIT_FUNC_TRACE();

	priv->dequeue_timeout_ns = conf->dequeue_timeout_ns;
	priv->nb_event_queues = conf->nb_event_queues;
	priv->nb_event_ports = conf->nb_event_ports;
	priv->nb_event_queue_flows = conf->nb_event_queue_flows;
	priv->nb_event_port_dequeue_depth = conf->nb_event_port_dequeue_depth;
	priv->nb_event_port_enqueue_depth = conf->nb_event_port_enqueue_depth;
	priv->event_dev_cfg = conf->event_dev_cfg;

	DPAA2_EVENTDEV_DEBUG("Configured eventdev devid=%d",
			     dev->data->dev_id);
	return 0;
}

static int
dpaa2_eventdev_start(struct rte_eventdev *dev)
{
	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);

	return 0;
}

static void
dpaa2_eventdev_stop(struct rte_eventdev *dev)
{
	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);
}

static int
dpaa2_eventdev_close(struct rte_eventdev *dev)
{
	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);

	return 0;
}

static void
dpaa2_eventdev_queue_def_conf(struct rte_eventdev *dev, uint8_t queue_id,
			      struct rte_event_queue_conf *queue_conf)
{
	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);
	RTE_SET_USED(queue_id);
	RTE_SET_USED(queue_conf);

	queue_conf->nb_atomic_flows = DPAA2_EVENT_QUEUE_ATOMIC_FLOWS;
	queue_conf->schedule_type = RTE_SCHED_TYPE_ATOMIC |
				      RTE_SCHED_TYPE_PARALLEL;
	queue_conf->priority = RTE_EVENT_DEV_PRIORITY_NORMAL;
}

static void
dpaa2_eventdev_queue_release(struct rte_eventdev *dev, uint8_t queue_id)
{
	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);
	RTE_SET_USED(queue_id);
}

static int
dpaa2_eventdev_queue_setup(struct rte_eventdev *dev, uint8_t queue_id,
			   const struct rte_event_queue_conf *queue_conf)
{
	struct dpaa2_eventdev *priv = dev->data->dev_private;
	struct evq_info_t *evq_info =
		&priv->evq_info[queue_id];

	EVENTDEV_INIT_FUNC_TRACE();

	evq_info->event_queue_cfg = queue_conf->event_queue_cfg;

	return 0;
}

static void
dpaa2_eventdev_port_def_conf(struct rte_eventdev *dev, uint8_t port_id,
			     struct rte_event_port_conf *port_conf)
{
	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);
	RTE_SET_USED(port_id);
	RTE_SET_USED(port_conf);

	port_conf->new_event_threshold =
		DPAA2_EVENT_MAX_NUM_EVENTS;
	port_conf->dequeue_depth =
		DPAA2_EVENT_MAX_PORT_DEQUEUE_DEPTH;
	port_conf->enqueue_depth =
		DPAA2_EVENT_MAX_PORT_ENQUEUE_DEPTH;
	port_conf->disable_implicit_release = 0;
}

static void
dpaa2_eventdev_port_release(void *port)
{
	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(port);
}

static int
dpaa2_eventdev_port_setup(struct rte_eventdev *dev, uint8_t port_id,
			  const struct rte_event_port_conf *port_conf)
{
	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(port_conf);

	if (!dpaa2_io_portal[port_id].dpio_dev) {
		dpaa2_io_portal[port_id].dpio_dev =
				dpaa2_get_qbman_swp(port_id);
		rte_atomic16_inc(&dpaa2_io_portal[port_id].dpio_dev->ref_count);
		if (!dpaa2_io_portal[port_id].dpio_dev)
			return -1;
	}

	dpaa2_io_portal[port_id].eventdev = dev;
	dev->data->ports[port_id] = &dpaa2_io_portal[port_id];
	return 0;
}

static int
dpaa2_eventdev_port_unlink(struct rte_eventdev *dev, void *port,
			   uint8_t queues[], uint16_t nb_unlinks)
{
	struct dpaa2_eventdev *priv = dev->data->dev_private;
	struct dpaa2_io_portal_t *dpaa2_portal = port;
	struct evq_info_t *evq_info;
	int i;

	EVENTDEV_INIT_FUNC_TRACE();

	for (i = 0; i < nb_unlinks; i++) {
		evq_info = &priv->evq_info[queues[i]];
		qbman_swp_push_set(dpaa2_portal->dpio_dev->sw_portal,
				   evq_info->dpcon->channel_index, 0);
		dpio_remove_static_dequeue_channel(dpaa2_portal->dpio_dev->dpio,
					0, dpaa2_portal->dpio_dev->token,
			evq_info->dpcon->dpcon_id);
	}

	return (int)nb_unlinks;
}

static int
dpaa2_eventdev_port_link(struct rte_eventdev *dev, void *port,
			 const uint8_t queues[], const uint8_t priorities[],
			uint16_t nb_links)
{
	struct dpaa2_eventdev *priv = dev->data->dev_private;
	struct dpaa2_io_portal_t *dpaa2_portal = port;
	struct evq_info_t *evq_info;
	uint8_t channel_index;
	int ret, i, n;

	EVENTDEV_INIT_FUNC_TRACE();

	for (i = 0; i < nb_links; i++) {
		evq_info = &priv->evq_info[queues[i]];

		ret = dpio_add_static_dequeue_channel(
			dpaa2_portal->dpio_dev->dpio,
			CMD_PRI_LOW, dpaa2_portal->dpio_dev->token,
			evq_info->dpcon->dpcon_id, &channel_index);
		if (ret < 0) {
			DPAA2_EVENTDEV_ERR(
				"Static dequeue config failed: err(%d)", ret);
			goto err;
		}

		qbman_swp_push_set(dpaa2_portal->dpio_dev->sw_portal,
				   channel_index, 1);
		evq_info->dpcon->channel_index = channel_index;
	}

	RTE_SET_USED(priorities);

	return (int)nb_links;
err:
	for (n = 0; n < i; n++) {
		evq_info = &priv->evq_info[queues[n]];
		qbman_swp_push_set(dpaa2_portal->dpio_dev->sw_portal,
				   evq_info->dpcon->channel_index, 0);
		dpio_remove_static_dequeue_channel(dpaa2_portal->dpio_dev->dpio,
					0, dpaa2_portal->dpio_dev->token,
			evq_info->dpcon->dpcon_id);
	}
	return ret;
}

static int
dpaa2_eventdev_timeout_ticks(struct rte_eventdev *dev, uint64_t ns,
			     uint64_t *timeout_ticks)
{
	uint32_t scale = 1;

	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);
	*timeout_ticks = ns * scale;

	return 0;
}

static void
dpaa2_eventdev_dump(struct rte_eventdev *dev, FILE *f)
{
	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);
	RTE_SET_USED(f);
}

static int
dpaa2_eventdev_eth_caps_get(const struct rte_eventdev *dev,
			    const struct rte_eth_dev *eth_dev,
			    uint32_t *caps)
{
	const char *ethdev_driver = eth_dev->device->driver->name;

	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);

	if (!strcmp(ethdev_driver, "net_dpaa2"))
		*caps = RTE_EVENT_ETH_RX_ADAPTER_DPAA2_CAP;
	else
		*caps = RTE_EVENT_ETH_RX_ADAPTER_SW_CAP;

	return 0;
}

static int
dpaa2_eventdev_eth_queue_add_all(const struct rte_eventdev *dev,
		const struct rte_eth_dev *eth_dev,
		const struct rte_event_eth_rx_adapter_queue_conf *queue_conf)
{
	struct dpaa2_eventdev *priv = dev->data->dev_private;
	uint8_t ev_qid = queue_conf->ev.queue_id;
	uint16_t dpcon_id = priv->evq_info[ev_qid].dpcon->dpcon_id;
	int i, ret;

	EVENTDEV_INIT_FUNC_TRACE();

	for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
		ret = dpaa2_eth_eventq_attach(eth_dev, i,
				dpcon_id, queue_conf);
		if (ret) {
			DPAA2_EVENTDEV_ERR(
				"Event queue attach failed: err(%d)", ret);
			goto fail;
		}
	}
	return 0;
fail:
	for (i = (i - 1); i >= 0 ; i--)
		dpaa2_eth_eventq_detach(eth_dev, i);

	return ret;
}

static int
dpaa2_eventdev_eth_queue_add(const struct rte_eventdev *dev,
		const struct rte_eth_dev *eth_dev,
		int32_t rx_queue_id,
		const struct rte_event_eth_rx_adapter_queue_conf *queue_conf)
{
	struct dpaa2_eventdev *priv = dev->data->dev_private;
	uint8_t ev_qid = queue_conf->ev.queue_id;
	uint16_t dpcon_id = priv->evq_info[ev_qid].dpcon->dpcon_id;
	int ret;

	EVENTDEV_INIT_FUNC_TRACE();

	if (rx_queue_id == -1)
		return dpaa2_eventdev_eth_queue_add_all(dev,
				eth_dev, queue_conf);

	ret = dpaa2_eth_eventq_attach(eth_dev, rx_queue_id,
			dpcon_id, queue_conf);
	if (ret) {
		DPAA2_EVENTDEV_ERR(
			"Event queue attach failed: err(%d)", ret);
		return ret;
	}
	return 0;
}

static int
dpaa2_eventdev_eth_queue_del_all(const struct rte_eventdev *dev,
			     const struct rte_eth_dev *eth_dev)
{
	int i, ret;

	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);

	for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
		ret = dpaa2_eth_eventq_detach(eth_dev, i);
		if (ret) {
			DPAA2_EVENTDEV_ERR(
				"Event queue detach failed: err(%d)", ret);
			return ret;
		}
	}

	return 0;
}

static int
dpaa2_eventdev_eth_queue_del(const struct rte_eventdev *dev,
			     const struct rte_eth_dev *eth_dev,
			     int32_t rx_queue_id)
{
	int ret;

	EVENTDEV_INIT_FUNC_TRACE();

	if (rx_queue_id == -1)
		return dpaa2_eventdev_eth_queue_del_all(dev, eth_dev);

	ret = dpaa2_eth_eventq_detach(eth_dev, rx_queue_id);
	if (ret) {
		DPAA2_EVENTDEV_ERR(
			"Event queue detach failed: err(%d)", ret);
		return ret;
	}

	return 0;
}

static int
dpaa2_eventdev_eth_start(const struct rte_eventdev *dev,
			 const struct rte_eth_dev *eth_dev)
{
	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);
	RTE_SET_USED(eth_dev);

	return 0;
}

static int
dpaa2_eventdev_eth_stop(const struct rte_eventdev *dev,
			const struct rte_eth_dev *eth_dev)
{
	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);
	RTE_SET_USED(eth_dev);

	return 0;
}

static struct rte_eventdev_ops dpaa2_eventdev_ops = {
	.dev_infos_get    = dpaa2_eventdev_info_get,
	.dev_configure    = dpaa2_eventdev_configure,
	.dev_start        = dpaa2_eventdev_start,
	.dev_stop         = dpaa2_eventdev_stop,
	.dev_close        = dpaa2_eventdev_close,
	.queue_def_conf   = dpaa2_eventdev_queue_def_conf,
	.queue_setup      = dpaa2_eventdev_queue_setup,
	.queue_release    = dpaa2_eventdev_queue_release,
	.port_def_conf    = dpaa2_eventdev_port_def_conf,
	.port_setup       = dpaa2_eventdev_port_setup,
	.port_release     = dpaa2_eventdev_port_release,
	.port_link        = dpaa2_eventdev_port_link,
	.port_unlink      = dpaa2_eventdev_port_unlink,
	.timeout_ticks    = dpaa2_eventdev_timeout_ticks,
	.dump             = dpaa2_eventdev_dump,
	.eth_rx_adapter_caps_get = dpaa2_eventdev_eth_caps_get,
	.eth_rx_adapter_queue_add = dpaa2_eventdev_eth_queue_add,
	.eth_rx_adapter_queue_del = dpaa2_eventdev_eth_queue_del,
	.eth_rx_adapter_start = dpaa2_eventdev_eth_start,
	.eth_rx_adapter_stop = dpaa2_eventdev_eth_stop,
};

static int
dpaa2_eventdev_setup_dpci(struct dpaa2_dpci_dev *dpci_dev,
			  struct dpaa2_dpcon_dev *dpcon_dev)
{
	struct dpci_rx_queue_cfg rx_queue_cfg;
	int ret, i;

	/*Do settings to get the frame on a DPCON object*/
	rx_queue_cfg.options = DPCI_QUEUE_OPT_DEST |
		  DPCI_QUEUE_OPT_USER_CTX;
	rx_queue_cfg.dest_cfg.dest_type = DPCI_DEST_DPCON;
	rx_queue_cfg.dest_cfg.dest_id = dpcon_dev->dpcon_id;
	rx_queue_cfg.dest_cfg.priority = DPAA2_EVENT_DEFAULT_DPCI_PRIO;

	dpci_dev->rx_queue[DPAA2_EVENT_DPCI_PARALLEL_QUEUE].cb =
		dpaa2_eventdev_process_parallel;
	dpci_dev->rx_queue[DPAA2_EVENT_DPCI_ATOMIC_QUEUE].cb =
		dpaa2_eventdev_process_atomic;

	for (i = 0 ; i < DPAA2_EVENT_DPCI_MAX_QUEUES; i++) {
		rx_queue_cfg.user_ctx = (size_t)(&dpci_dev->rx_queue[i]);
		ret = dpci_set_rx_queue(&dpci_dev->dpci,
					CMD_PRI_LOW,
					dpci_dev->token, i,
					&rx_queue_cfg);
		if (ret) {
			DPAA2_EVENTDEV_ERR(
				"DPCI Rx queue setup failed: err(%d)",
				ret);
			return ret;
		}
	}
	return 0;
}

static int
dpaa2_eventdev_create(const char *name)
{
	struct rte_eventdev *eventdev;
	struct dpaa2_eventdev *priv;
	struct dpaa2_dpcon_dev *dpcon_dev = NULL;
	struct dpaa2_dpci_dev *dpci_dev = NULL;
	int ret;

	eventdev = rte_event_pmd_vdev_init(name,
					   sizeof(struct dpaa2_eventdev),
					   rte_socket_id());
	if (eventdev == NULL) {
		DPAA2_EVENTDEV_ERR("Failed to create Event device %s", name);
		goto fail;
	}

	eventdev->dev_ops       = &dpaa2_eventdev_ops;
	eventdev->enqueue       = dpaa2_eventdev_enqueue;
	eventdev->enqueue_burst = dpaa2_eventdev_enqueue_burst;
	eventdev->enqueue_new_burst = dpaa2_eventdev_enqueue_burst;
	eventdev->enqueue_forward_burst = dpaa2_eventdev_enqueue_burst;
	eventdev->dequeue       = dpaa2_eventdev_dequeue;
	eventdev->dequeue_burst = dpaa2_eventdev_dequeue_burst;

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	priv = eventdev->data->dev_private;
	priv->max_event_queues = 0;

	do {
		dpcon_dev = rte_dpaa2_alloc_dpcon_dev();
		if (!dpcon_dev)
			break;
		priv->evq_info[priv->max_event_queues].dpcon = dpcon_dev;

		dpci_dev = rte_dpaa2_alloc_dpci_dev();
		if (!dpci_dev) {
			rte_dpaa2_free_dpcon_dev(dpcon_dev);
			break;
		}
		priv->evq_info[priv->max_event_queues].dpci = dpci_dev;

		ret = dpaa2_eventdev_setup_dpci(dpci_dev, dpcon_dev);
		if (ret) {
			DPAA2_EVENTDEV_ERR(
				    "DPCI setup failed: err(%d)", ret);
			return ret;
		}
		priv->max_event_queues++;
	} while (dpcon_dev && dpci_dev);

	return 0;
fail:
	return -EFAULT;
}

static int
dpaa2_eventdev_probe(struct rte_vdev_device *vdev)
{
	const char *name;

	name = rte_vdev_device_name(vdev);
	DPAA2_EVENTDEV_INFO("Initializing %s", name);
	return dpaa2_eventdev_create(name);
}

static int
dpaa2_eventdev_remove(struct rte_vdev_device *vdev)
{
	const char *name;

	name = rte_vdev_device_name(vdev);
	DPAA2_EVENTDEV_INFO("Closing %s", name);

	return rte_event_pmd_vdev_uninit(name);
}

static struct rte_vdev_driver vdev_eventdev_dpaa2_pmd = {
	.probe = dpaa2_eventdev_probe,
	.remove = dpaa2_eventdev_remove
};

RTE_PMD_REGISTER_VDEV(EVENTDEV_NAME_DPAA2_PMD, vdev_eventdev_dpaa2_pmd);

RTE_INIT(dpaa2_eventdev_init_log);
static void
dpaa2_eventdev_init_log(void)
{
	dpaa2_logtype_event = rte_log_register("pmd.event.dpaa2");
	if (dpaa2_logtype_event >= 0)
		rte_log_set_level(dpaa2_logtype_event, RTE_LOG_NOTICE);
}
