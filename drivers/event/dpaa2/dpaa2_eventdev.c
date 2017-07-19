/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 NXP.
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
 *     * Neither the name of NXP nor the names of its
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
#include <rte_memzone.h>
#include <rte_pci.h>
#include <rte_vdev.h>

#include <fslmc_vfio.h>
#include <dpaa2_hw_pvt.h>
#include <dpaa2_hw_mempool.h>
#include <dpaa2_hw_dpio.h>
#include "dpaa2_eventdev.h"
#include <portal/dpaa2_hw_pvt.h>
#include <mc/fsl_dpci.h>

/* Clarifications
 * Evendev = SoC Instance
 * Eventport = DPIO Instance
 * Eventqueue = DPCON Instance
 * 1 Eventdev can have N Eventqueue
 * Soft Event Flow is DPCI Instance
 */

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
			PMD_DRV_LOG(ERR, "Failure in affining portal\n");
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
				fqid = evq_info->dpci->queue[
					DPAA2_EVENT_DPCI_PARALLEL_QUEUE].fqid;
			else
				fqid = evq_info->dpci->queue[
					DPAA2_EVENT_DPCI_ATOMIC_QUEUE].fqid;

			/* Prepare enqueue descriptor */
			qbman_eq_desc_clear(&eqdesc[loop]);
			qbman_eq_desc_set_fq(&eqdesc[loop], fqid);
			qbman_eq_desc_set_no_orp(&eqdesc[loop], 0);
			qbman_eq_desc_set_response(&eqdesc[loop], 0, 0);

			if (event->impl_opaque) {
				uint8_t dqrr_index = event->impl_opaque - 1;

				qbman_eq_desc_set_dca(&eqdesc[loop], 1,
						      dqrr_index, 0);
				DPAA2_PER_LCORE_DPIO->dqrr_size--;
				DPAA2_PER_LCORE_DPIO->dqrr_held &=
					~(1 << dqrr_index);
			}

			memset(&fd_arr[loop], 0, sizeof(struct qbman_fd));

			/*
			 * todo - need to align with hw context data
			 * to avoid copy
			 */
			struct rte_event *ev_temp = rte_malloc(NULL,
				sizeof(struct rte_event), 0);
			rte_memcpy(ev_temp, event, sizeof(struct rte_event));
			DPAA2_SET_FD_ADDR((&fd_arr[loop]), ev_temp);
			DPAA2_SET_FD_LEN((&fd_arr[loop]),
					 sizeof(struct rte_event));
		}
		loop = 0;
		while (loop < frames_to_send) {
			loop += qbman_swp_enqueue_multiple_eqdesc(swp,
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
	int ret, i = 0;

	qbman_swp_interrupt_clear_status(DPAA2_PER_LCORE_PORTAL,
					 QBMAN_SWP_INTERRUPT_DQRI);

RETRY:
	ret = epoll_wait(DPAA2_PER_LCORE_DPIO->epoll_fd,
			 &epoll_ev, 1, timeout_ticks);
	if (ret < 1) {
		/* sometimes due to some spurious interrupts epoll_wait fails
		 * with errno EINTR. so here we are retrying epoll_wait in such
		 * case to avoid the problem.
		 */
		if (errno == EINTR) {
			PMD_DRV_LOG(DEBUG, "epoll_wait fails\n");
			if (i++ > 10)
				PMD_DRV_LOG(DEBUG, "Dequeue burst Failed\n");
		goto RETRY;
		}
	}
}

static void dpaa2_eventdev_process_parallel(struct qbman_swp *swp,
					    const struct qbman_fd *fd,
					    const struct qbman_result *dq,
					    struct rte_event *ev)
{
	struct rte_event *ev_temp =
		(struct rte_event *)DPAA2_GET_FD_ADDR(fd);
	rte_memcpy(ev, ev_temp, sizeof(struct rte_event));
	rte_free(ev_temp);

	qbman_swp_dqrr_consume(swp, dq);
}

static void dpaa2_eventdev_process_atomic(struct qbman_swp *swp,
					  const struct qbman_fd *fd,
					  const struct qbman_result *dq,
					  struct rte_event *ev)
{
	struct rte_event *ev_temp =
		(struct rte_event *)DPAA2_GET_FD_ADDR(fd);
	uint8_t dqrr_index = qbman_get_dqrr_idx(dq);

	RTE_SET_USED(swp);

	rte_memcpy(ev, ev_temp, sizeof(struct rte_event));
	rte_free(ev_temp);
	ev->impl_opaque = dqrr_index + 1;
	DPAA2_PER_LCORE_DPIO->dqrr_size++;
	DPAA2_PER_LCORE_DPIO->dqrr_held |= 1 << dqrr_index;
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
			PMD_DRV_LOG(ERR, "Failure in affining portal\n");
			return 0;
		}
	}

	swp = DPAA2_PER_LCORE_PORTAL;

	/* Check if there are atomic contexts to be released */
	while (DPAA2_PER_LCORE_DPIO->dqrr_size) {
		if (DPAA2_PER_LCORE_DPIO->dqrr_held & (1 << i)) {
			dq = qbman_get_dqrr_from_idx(swp, i);
			qbman_swp_dqrr_consume(swp, dq);
			DPAA2_PER_LCORE_DPIO->dqrr_size--;
		}
		i++;
	}
	DPAA2_PER_LCORE_DPIO->dqrr_held = 0;

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

		fd = qbman_result_DQ_fd(dq);

		rxq = (struct dpaa2_queue *)qbman_result_DQ_fqd_ctx(dq);
		if (rxq) {
			rxq->cb(swp, fd, dq, &ev[num_pkts]);
		} else {
			qbman_swp_dqrr_consume(swp, dq);
			PMD_DRV_LOG(ERR, "Null Return VQ received\n");
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

	PMD_DRV_FUNC_TRACE();

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
	dev_info->max_event_ports = RTE_MAX_LCORE;
	dev_info->max_event_port_dequeue_depth =
		DPAA2_EVENT_MAX_PORT_DEQUEUE_DEPTH;
	dev_info->max_event_port_enqueue_depth =
		DPAA2_EVENT_MAX_PORT_ENQUEUE_DEPTH;
	dev_info->max_num_events = DPAA2_EVENT_MAX_NUM_EVENTS;
	dev_info->event_dev_cap = RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED |
		RTE_EVENT_DEV_CAP_BURST_MODE;
}

static int
dpaa2_eventdev_configure(const struct rte_eventdev *dev)
{
	struct dpaa2_eventdev *priv = dev->data->dev_private;
	struct rte_event_dev_config *conf = &dev->data->dev_conf;

	PMD_DRV_FUNC_TRACE();

	priv->dequeue_timeout_ns = conf->dequeue_timeout_ns;
	priv->nb_event_queues = conf->nb_event_queues;
	priv->nb_event_ports = conf->nb_event_ports;
	priv->nb_event_queue_flows = conf->nb_event_queue_flows;
	priv->nb_event_port_dequeue_depth = conf->nb_event_port_dequeue_depth;
	priv->nb_event_port_enqueue_depth = conf->nb_event_port_enqueue_depth;
	priv->event_dev_cfg = conf->event_dev_cfg;

	PMD_DRV_LOG(DEBUG, "Configured eventdev devid=%d", dev->data->dev_id);
	return 0;
}

static int
dpaa2_eventdev_start(struct rte_eventdev *dev)
{
	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(dev);

	return 0;
}

static void
dpaa2_eventdev_stop(struct rte_eventdev *dev)
{
	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(dev);
}

static int
dpaa2_eventdev_close(struct rte_eventdev *dev)
{
	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(dev);

	return 0;
}

static void
dpaa2_eventdev_queue_def_conf(struct rte_eventdev *dev, uint8_t queue_id,
			      struct rte_event_queue_conf *queue_conf)
{
	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(dev);
	RTE_SET_USED(queue_id);
	RTE_SET_USED(queue_conf);

	queue_conf->nb_atomic_flows = DPAA2_EVENT_QUEUE_ATOMIC_FLOWS;
	queue_conf->event_queue_cfg = RTE_EVENT_QUEUE_CFG_ATOMIC_ONLY |
				      RTE_EVENT_QUEUE_CFG_PARALLEL_ONLY;
	queue_conf->priority = RTE_EVENT_DEV_PRIORITY_NORMAL;
}

static void
dpaa2_eventdev_queue_release(struct rte_eventdev *dev, uint8_t queue_id)
{
	PMD_DRV_FUNC_TRACE();

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

	PMD_DRV_FUNC_TRACE();

	evq_info->event_queue_cfg = queue_conf->event_queue_cfg;

	return 0;
}

static void
dpaa2_eventdev_port_def_conf(struct rte_eventdev *dev, uint8_t port_id,
			     struct rte_event_port_conf *port_conf)
{
	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(dev);
	RTE_SET_USED(port_id);
	RTE_SET_USED(port_conf);

	port_conf->new_event_threshold =
		DPAA2_EVENT_MAX_NUM_EVENTS;
	port_conf->dequeue_depth =
		DPAA2_EVENT_MAX_PORT_DEQUEUE_DEPTH;
	port_conf->enqueue_depth =
		DPAA2_EVENT_MAX_PORT_ENQUEUE_DEPTH;
}

static void
dpaa2_eventdev_port_release(void *port)
{
	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(port);
}

static int
dpaa2_eventdev_port_setup(struct rte_eventdev *dev, uint8_t port_id,
			  const struct rte_event_port_conf *port_conf)
{
	PMD_DRV_FUNC_TRACE();

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

	PMD_DRV_FUNC_TRACE();

	for (i = 0; i < nb_unlinks; i++) {
		evq_info = &priv->evq_info[queues[i]];
		qbman_swp_push_set(dpaa2_portal->dpio_dev->sw_portal,
				   evq_info->dpcon->channel_index, 0);
		dpio_remove_static_dequeue_channel(dpaa2_portal->dpio_dev->dpio,
					0, dpaa2_portal->dpio_dev->token,
			evq_info->dpcon->dpcon_id);
		evq_info->link = 0;
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

	PMD_DRV_FUNC_TRACE();

	for (i = 0; i < nb_links; i++) {
		evq_info = &priv->evq_info[queues[i]];
		if (evq_info->link)
			continue;

		ret = dpio_add_static_dequeue_channel(
			dpaa2_portal->dpio_dev->dpio,
			CMD_PRI_LOW, dpaa2_portal->dpio_dev->token,
			evq_info->dpcon->dpcon_id, &channel_index);
		if (ret < 0) {
			PMD_DRV_ERR("Static dequeue cfg failed with ret: %d\n",
				    ret);
			goto err;
		}

		qbman_swp_push_set(dpaa2_portal->dpio_dev->sw_portal,
				   channel_index, 1);
		evq_info->dpcon->channel_index = channel_index;
		evq_info->link = 1;
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
		evq_info->link = 0;
	}
	return ret;
}

static int
dpaa2_eventdev_timeout_ticks(struct rte_eventdev *dev, uint64_t ns,
			     uint64_t *timeout_ticks)
{
	uint32_t scale = 1;

	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(dev);
	*timeout_ticks = ns * scale;

	return 0;
}

static void
dpaa2_eventdev_dump(struct rte_eventdev *dev, FILE *f)
{
	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(dev);
	RTE_SET_USED(f);
}

static const struct rte_eventdev_ops dpaa2_eventdev_ops = {
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
	.dump             = dpaa2_eventdev_dump
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

	dpci_dev->queue[DPAA2_EVENT_DPCI_PARALLEL_QUEUE].cb =
		dpaa2_eventdev_process_parallel;
	dpci_dev->queue[DPAA2_EVENT_DPCI_ATOMIC_QUEUE].cb =
		dpaa2_eventdev_process_atomic;

	for (i = 0 ; i < DPAA2_EVENT_DPCI_MAX_QUEUES; i++) {
		rx_queue_cfg.user_ctx = (uint64_t)(&dpci_dev->queue[i]);
		ret = dpci_set_rx_queue(&dpci_dev->dpci,
					CMD_PRI_LOW,
					dpci_dev->token, i,
					&rx_queue_cfg);
		if (ret) {
			PMD_DRV_LOG(ERR,
				    "set_rx_q failed with err code: %d", ret);
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
		PMD_DRV_ERR("Failed to create eventdev vdev %s", name);
		goto fail;
	}

	eventdev->dev_ops       = &dpaa2_eventdev_ops;
	eventdev->schedule      = NULL;
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
			PMD_DRV_LOG(ERR,
				    "dpci setup failed with err code: %d", ret);
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
	PMD_DRV_LOG(INFO, "Initializing %s", name);
	return dpaa2_eventdev_create(name);
}

static int
dpaa2_eventdev_remove(struct rte_vdev_device *vdev)
{
	const char *name;

	name = rte_vdev_device_name(vdev);
	PMD_DRV_LOG(INFO, "Closing %s", name);

	return rte_event_pmd_vdev_uninit(name);
}

static struct rte_vdev_driver vdev_eventdev_dpaa2_pmd = {
	.probe = dpaa2_eventdev_probe,
	.remove = dpaa2_eventdev_remove
};

RTE_PMD_REGISTER_VDEV(EVENTDEV_NAME_DPAA2_PMD, vdev_eventdev_dpaa2_pmd);
