/*   SPDX-License-Identifier:        BSD-3-Clause
 *   Copyright 2017-2019 NXP
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
#include <dev_driver.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_pci.h>
#include <rte_eventdev.h>
#include <eventdev_pmd_vdev.h>
#include <rte_ethdev.h>
#include <rte_event_crypto_adapter.h>
#include <rte_event_eth_rx_adapter.h>
#include <rte_event_eth_tx_adapter.h>
#include <cryptodev_pmd.h>
#include <bus_dpaa_driver.h>
#include <rte_dpaa_logs.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>

#include <dpaa_ethdev.h>
#include <dpaa_sec_event.h>
#include "dpaa_eventdev.h"
#include <dpaa_mempool.h>

/*
 * Clarifications
 * Evendev = Virtual Instance for SoC
 * Eventport = Portal Instance
 * Eventqueue = Channel Instance
 * 1 Eventdev can have N Eventqueue
 */
RTE_LOG_REGISTER_DEFAULT(dpaa_logtype_eventdev, NOTICE);

#define DISABLE_INTR_MODE "disable_intr"

static int
dpaa_event_dequeue_timeout_ticks(struct rte_eventdev *dev, uint64_t ns,
				 uint64_t *timeout_ticks)
{
	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);

	uint64_t cycles_per_second;

	cycles_per_second = rte_get_timer_hz();
	*timeout_ticks = (ns * cycles_per_second) / NS_PER_S;

	return 0;
}

static int
dpaa_event_dequeue_timeout_ticks_intr(struct rte_eventdev *dev, uint64_t ns,
				 uint64_t *timeout_ticks)
{
	RTE_SET_USED(dev);

	*timeout_ticks = ns/1000;
	return 0;
}

static void
dpaa_eventq_portal_add(u16 ch_id)
{
	uint32_t sdqcr;

	sdqcr = QM_SDQCR_CHANNELS_POOL_CONV(ch_id);
	qman_static_dequeue_add(sdqcr, NULL);
}

static uint16_t
dpaa_event_enqueue_burst(void *port, const struct rte_event ev[],
			 uint16_t nb_events)
{
	uint16_t i;
	struct rte_mbuf *mbuf;

	RTE_SET_USED(port);
	/*Release all the contexts saved previously*/
	for (i = 0; i < nb_events; i++) {
		switch (ev[i].op) {
		case RTE_EVENT_OP_RELEASE:
			qman_dca_index(ev[i].impl_opaque, 0);
			mbuf = DPAA_PER_LCORE_DQRR_MBUF(i);
			*dpaa_seqn(mbuf) = DPAA_INVALID_MBUF_SEQN;
			DPAA_PER_LCORE_DQRR_HELD &= ~(1 << i);
			DPAA_PER_LCORE_DQRR_SIZE--;
			break;
		default:
			break;
		}
	}

	return nb_events;
}

static uint16_t
dpaa_event_enqueue(void *port, const struct rte_event *ev)
{
	return dpaa_event_enqueue_burst(port, ev, 1);
}

static void drain_4_bytes(int fd, fd_set *fdset)
{
	if (FD_ISSET(fd, fdset)) {
		/* drain 4 bytes */
		uint32_t junk;
		ssize_t sjunk = read(qman_thread_fd(), &junk, sizeof(junk));
		if (sjunk != sizeof(junk))
			DPAA_EVENTDEV_ERR("UIO irq read error");
	}
}

static inline int
dpaa_event_dequeue_wait(uint64_t timeout_ticks)
{
	int fd_qman, nfds;
	int ret;
	fd_set readset;

	/* Go into (and back out of) IRQ mode for each select,
	 * it simplifies exit-path considerations and other
	 * potential nastiness.
	 */
	struct timeval tv = {
		.tv_sec = timeout_ticks / 1000000,
		.tv_usec = timeout_ticks % 1000000
	};

	fd_qman = qman_thread_fd();
	nfds = fd_qman + 1;
	FD_ZERO(&readset);
	FD_SET(fd_qman, &readset);

	qman_irqsource_add(QM_PIRQ_DQRI);

	ret = select(nfds, &readset, NULL, NULL, &tv);
	if (ret < 0)
		return ret;
	/* Calling irqsource_remove() prior to thread_irq()
	 * means thread_irq() will not process whatever caused
	 * the interrupts, however it does ensure that, once
	 * thread_irq() re-enables interrupts, they won't fire
	 * again immediately.
	 */
	qman_irqsource_remove(~0);
	drain_4_bytes(fd_qman, &readset);
	qman_thread_irq();

	return ret;
}

static uint16_t
dpaa_event_dequeue_burst(void *port, struct rte_event ev[],
			 uint16_t nb_events, uint64_t timeout_ticks)
{
	int ret;
	u16 ch_id;
	void *buffers[8];
	u32 num_frames, i;
	uint64_t cur_ticks = 0, wait_time_ticks = 0;
	struct dpaa_port *portal = (struct dpaa_port *)port;
	struct rte_mbuf *mbuf;

	if (unlikely(!DPAA_PER_LCORE_PORTAL)) {
		/* Affine current thread context to a qman portal */
		ret = rte_dpaa_portal_init((void *)0);
		if (ret) {
			DPAA_EVENTDEV_ERR("Unable to initialize portal");
			return ret;
		}
	}

	if (unlikely(!portal->is_port_linked)) {
		/*
		 * Affine event queue for current thread context
		 * to a qman portal.
		 */
		for (i = 0; i < portal->num_linked_evq; i++) {
			ch_id = portal->evq_info[i].ch_id;
			dpaa_eventq_portal_add(ch_id);
		}
		portal->is_port_linked = true;
	}

	/* Check if there are atomic contexts to be released */
	i = 0;
	while (DPAA_PER_LCORE_DQRR_SIZE) {
		if (DPAA_PER_LCORE_DQRR_HELD & (1 << i)) {
			qman_dca_index(i, 0);
			mbuf = DPAA_PER_LCORE_DQRR_MBUF(i);
			*dpaa_seqn(mbuf) = DPAA_INVALID_MBUF_SEQN;
			DPAA_PER_LCORE_DQRR_HELD &= ~(1 << i);
			DPAA_PER_LCORE_DQRR_SIZE--;
		}
		i++;
	}
	DPAA_PER_LCORE_DQRR_HELD = 0;

	if (timeout_ticks)
		wait_time_ticks = timeout_ticks;
	else
		wait_time_ticks = portal->timeout_us;

	wait_time_ticks += rte_get_timer_cycles();
	do {
		/* Lets dequeue the frames */
		num_frames = qman_portal_dequeue(ev, nb_events, buffers);
		if (num_frames)
			break;
		cur_ticks = rte_get_timer_cycles();
	} while (cur_ticks < wait_time_ticks);

	return num_frames;
}

static uint16_t
dpaa_event_dequeue(void *port, struct rte_event *ev, uint64_t timeout_ticks)
{
	return dpaa_event_dequeue_burst(port, ev, 1, timeout_ticks);
}

static uint16_t
dpaa_event_dequeue_burst_intr(void *port, struct rte_event ev[],
			      uint16_t nb_events, uint64_t timeout_ticks)
{
	int ret;
	u16 ch_id;
	void *buffers[8];
	u32 num_frames, i, irq = 0;
	uint64_t cur_ticks = 0, wait_time_ticks = 0;
	struct dpaa_port *portal = (struct dpaa_port *)port;
	struct rte_mbuf *mbuf;

	if (unlikely(!DPAA_PER_LCORE_PORTAL)) {
		/* Affine current thread context to a qman portal */
		ret = rte_dpaa_portal_init((void *)0);
		if (ret) {
			DPAA_EVENTDEV_ERR("Unable to initialize portal");
			return ret;
		}
	}

	if (unlikely(!portal->is_port_linked)) {
		/*
		 * Affine event queue for current thread context
		 * to a qman portal.
		 */
		for (i = 0; i < portal->num_linked_evq; i++) {
			ch_id = portal->evq_info[i].ch_id;
			dpaa_eventq_portal_add(ch_id);
		}
		portal->is_port_linked = true;
	}

	/* Check if there are atomic contexts to be released */
	i = 0;
	while (DPAA_PER_LCORE_DQRR_SIZE) {
		if (DPAA_PER_LCORE_DQRR_HELD & (1 << i)) {
			qman_dca_index(i, 0);
			mbuf = DPAA_PER_LCORE_DQRR_MBUF(i);
			*dpaa_seqn(mbuf) = DPAA_INVALID_MBUF_SEQN;
			DPAA_PER_LCORE_DQRR_HELD &= ~(1 << i);
			DPAA_PER_LCORE_DQRR_SIZE--;
		}
		i++;
	}
	DPAA_PER_LCORE_DQRR_HELD = 0;

	if (timeout_ticks)
		wait_time_ticks = timeout_ticks;
	else
		wait_time_ticks = portal->timeout_us;

	do {
		/* Lets dequeue the frames */
		num_frames = qman_portal_dequeue(ev, nb_events, buffers);
		if (irq)
			irq = 0;
		if (num_frames)
			break;
		if (wait_time_ticks) { /* wait for time */
			if (dpaa_event_dequeue_wait(wait_time_ticks) > 0) {
				irq = 1;
				continue;
			}
			break; /* no event after waiting */
		}
		cur_ticks = rte_get_timer_cycles();
	} while (cur_ticks < wait_time_ticks);

	return num_frames;
}

static uint16_t
dpaa_event_dequeue_intr(void *port,
			struct rte_event *ev,
			uint64_t timeout_ticks)
{
	return dpaa_event_dequeue_burst_intr(port, ev, 1, timeout_ticks);
}

static void
dpaa_event_dev_info_get(struct rte_eventdev *dev,
			struct rte_event_dev_info *dev_info)
{
	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);
	dev_info->driver_name = "event_dpaa1";
	dev_info->min_dequeue_timeout_ns =
		DPAA_EVENT_MIN_DEQUEUE_TIMEOUT;
	dev_info->max_dequeue_timeout_ns =
		DPAA_EVENT_MAX_DEQUEUE_TIMEOUT;
	dev_info->dequeue_timeout_ns =
		DPAA_EVENT_PORT_DEQUEUE_TIMEOUT_NS;
	dev_info->max_event_queues =
		DPAA_EVENT_MAX_QUEUES;
	dev_info->max_event_queue_flows =
		DPAA_EVENT_MAX_QUEUE_FLOWS;
	dev_info->max_event_queue_priority_levels =
		DPAA_EVENT_MAX_QUEUE_PRIORITY_LEVELS;
	dev_info->max_event_priority_levels =
		DPAA_EVENT_MAX_EVENT_PRIORITY_LEVELS;
	dev_info->max_event_ports =
		DPAA_EVENT_MAX_EVENT_PORT;
	dev_info->max_event_port_dequeue_depth =
		DPAA_EVENT_MAX_PORT_DEQUEUE_DEPTH;
	dev_info->max_event_port_enqueue_depth =
		DPAA_EVENT_MAX_PORT_ENQUEUE_DEPTH;
	/*
	 * TODO: Need to find out that how to fetch this info
	 * from kernel or somewhere else.
	 */
	dev_info->max_num_events =
		DPAA_EVENT_MAX_NUM_EVENTS;
	dev_info->event_dev_cap =
		RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED |
		RTE_EVENT_DEV_CAP_BURST_MODE |
		RTE_EVENT_DEV_CAP_MULTIPLE_QUEUE_PORT |
		RTE_EVENT_DEV_CAP_NONSEQ_MODE |
		RTE_EVENT_DEV_CAP_CARRY_FLOW_ID |
		RTE_EVENT_DEV_CAP_MAINTENANCE_FREE;
	dev_info->max_profiles_per_port = 1;
}

static int
dpaa_event_dev_configure(const struct rte_eventdev *dev)
{
	struct dpaa_eventdev *priv = dev->data->dev_private;
	struct rte_event_dev_config *conf = &dev->data->dev_conf;
	int ret, i;
	uint32_t *ch_id;

	EVENTDEV_INIT_FUNC_TRACE();
	priv->dequeue_timeout_ns = conf->dequeue_timeout_ns;
	priv->nb_events_limit = conf->nb_events_limit;
	priv->nb_event_queues = conf->nb_event_queues;
	priv->nb_event_ports = conf->nb_event_ports;
	priv->nb_event_queue_flows = conf->nb_event_queue_flows;
	priv->nb_event_port_dequeue_depth = conf->nb_event_port_dequeue_depth;
	priv->nb_event_port_enqueue_depth = conf->nb_event_port_enqueue_depth;
	priv->event_dev_cfg = conf->event_dev_cfg;

	ch_id = rte_malloc("dpaa-channels",
			  sizeof(uint32_t) * priv->nb_event_queues,
			  RTE_CACHE_LINE_SIZE);
	if (ch_id == NULL) {
		DPAA_EVENTDEV_ERR("Fail to allocate memory for dpaa channels\n");
		return -ENOMEM;
	}
	/* Create requested event queues within the given event device */
	ret = qman_alloc_pool_range(ch_id, priv->nb_event_queues, 1, 0);
	if (ret < 0) {
		DPAA_EVENTDEV_ERR("qman_alloc_pool_range %u, err =%d\n",
				 priv->nb_event_queues, ret);
		rte_free(ch_id);
		return ret;
	}
	for (i = 0; i < priv->nb_event_queues; i++)
		priv->evq_info[i].ch_id = (u16)ch_id[i];

	/* Lets prepare event ports */
	memset(&priv->ports[0], 0,
	      sizeof(struct dpaa_port) * priv->nb_event_ports);

	/* Check dequeue timeout method is per dequeue or global */
	if (priv->event_dev_cfg & RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT) {
		/*
		 * Use timeout value as given in dequeue operation.
		 * So invalidating this timeout value.
		 */
		priv->dequeue_timeout_ns = 0;

	} else if (conf->dequeue_timeout_ns == 0) {
		priv->dequeue_timeout_ns = DPAA_EVENT_PORT_DEQUEUE_TIMEOUT_NS;
	} else {
		priv->dequeue_timeout_ns = conf->dequeue_timeout_ns;
	}

	for (i = 0; i < priv->nb_event_ports; i++) {
		if (priv->intr_mode) {
			priv->ports[i].timeout_us =
				priv->dequeue_timeout_ns/1000;
		} else {
			uint64_t cycles_per_second;

			cycles_per_second = rte_get_timer_hz();
			priv->ports[i].timeout_us =
				(priv->dequeue_timeout_ns * cycles_per_second)
					/ NS_PER_S;
		}
	}

	/*
	 * TODO: Currently portals are affined with threads. Maximum threads
	 * can be created equals to number of lcore.
	 */
	rte_free(ch_id);
	DPAA_EVENTDEV_INFO("Configured eventdev devid=%d", dev->data->dev_id);

	return 0;
}

static int
dpaa_event_dev_start(struct rte_eventdev *dev)
{
	EVENTDEV_INIT_FUNC_TRACE();
	RTE_SET_USED(dev);

	return 0;
}

static void
dpaa_event_dev_stop(struct rte_eventdev *dev)
{
	EVENTDEV_INIT_FUNC_TRACE();
	RTE_SET_USED(dev);
}

static int
dpaa_event_dev_close(struct rte_eventdev *dev)
{
	EVENTDEV_INIT_FUNC_TRACE();
	RTE_SET_USED(dev);

	return 0;
}

static void
dpaa_event_queue_def_conf(struct rte_eventdev *dev, uint8_t queue_id,
			  struct rte_event_queue_conf *queue_conf)
{
	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);
	RTE_SET_USED(queue_id);

	memset(queue_conf, 0, sizeof(struct rte_event_queue_conf));
	queue_conf->nb_atomic_flows = DPAA_EVENT_QUEUE_ATOMIC_FLOWS;
	queue_conf->schedule_type = RTE_SCHED_TYPE_PARALLEL;
	queue_conf->priority = RTE_EVENT_DEV_PRIORITY_HIGHEST;
}

static int
dpaa_event_queue_setup(struct rte_eventdev *dev, uint8_t queue_id,
		       const struct rte_event_queue_conf *queue_conf)
{
	struct dpaa_eventdev *priv = dev->data->dev_private;
	struct dpaa_eventq *evq_info = &priv->evq_info[queue_id];

	EVENTDEV_INIT_FUNC_TRACE();

	switch (queue_conf->schedule_type) {
	case RTE_SCHED_TYPE_PARALLEL:
	case RTE_SCHED_TYPE_ATOMIC:
		break;
	case RTE_SCHED_TYPE_ORDERED:
		DPAA_EVENTDEV_ERR("Schedule type is not supported.");
		return -1;
	}
	evq_info->event_queue_cfg = queue_conf->event_queue_cfg;
	evq_info->event_queue_id = queue_id;

	return 0;
}

static void
dpaa_event_queue_release(struct rte_eventdev *dev, uint8_t queue_id)
{
	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);
	RTE_SET_USED(queue_id);
}

static void
dpaa_event_port_default_conf_get(struct rte_eventdev *dev, uint8_t port_id,
				 struct rte_event_port_conf *port_conf)
{
	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);
	RTE_SET_USED(port_id);

	port_conf->new_event_threshold = DPAA_EVENT_MAX_NUM_EVENTS;
	port_conf->dequeue_depth = DPAA_EVENT_MAX_PORT_DEQUEUE_DEPTH;
	port_conf->enqueue_depth = DPAA_EVENT_MAX_PORT_ENQUEUE_DEPTH;
}

static int
dpaa_event_port_setup(struct rte_eventdev *dev, uint8_t port_id,
		      const struct rte_event_port_conf *port_conf)
{
	struct dpaa_eventdev *eventdev = dev->data->dev_private;

	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(port_conf);
	dev->data->ports[port_id] = &eventdev->ports[port_id];

	return 0;
}

static void
dpaa_event_port_release(void *port)
{
	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(port);
}

static int
dpaa_event_port_link(struct rte_eventdev *dev, void *port,
		     const uint8_t queues[], const uint8_t priorities[],
		     uint16_t nb_links)
{
	struct dpaa_eventdev *priv = dev->data->dev_private;
	struct dpaa_port *event_port = (struct dpaa_port *)port;
	struct dpaa_eventq *event_queue;
	uint8_t eventq_id;
	int i;

	RTE_SET_USED(dev);
	RTE_SET_USED(priorities);

	/* First check that input configuration are valid */
	for (i = 0; i < nb_links; i++) {
		eventq_id = queues[i];
		event_queue = &priv->evq_info[eventq_id];
		if ((event_queue->event_queue_cfg
			& RTE_EVENT_QUEUE_CFG_SINGLE_LINK)
			&& (event_queue->event_port)) {
			return -EINVAL;
		}
	}

	for (i = 0; i < nb_links; i++) {
		eventq_id = queues[i];
		event_queue = &priv->evq_info[eventq_id];
		event_port->evq_info[i].event_queue_id = eventq_id;
		event_port->evq_info[i].ch_id = event_queue->ch_id;
		event_queue->event_port = port;
	}

	event_port->num_linked_evq = event_port->num_linked_evq + i;

	return (int)i;
}

static int
dpaa_event_port_unlink(struct rte_eventdev *dev, void *port,
		       uint8_t queues[], uint16_t nb_links)
{
	int i;
	uint8_t eventq_id;
	struct dpaa_eventq *event_queue;
	struct dpaa_eventdev *priv = dev->data->dev_private;
	struct dpaa_port *event_port = (struct dpaa_port *)port;

	if (!event_port->num_linked_evq)
		return nb_links;

	for (i = 0; i < nb_links; i++) {
		eventq_id = queues[i];
		event_port->evq_info[eventq_id].event_queue_id = -1;
		event_port->evq_info[eventq_id].ch_id = 0;
		event_queue = &priv->evq_info[eventq_id];
		event_queue->event_port = NULL;
	}

	if (event_port->num_linked_evq)
		event_port->num_linked_evq = event_port->num_linked_evq - i;

	return (int)i;
}

static int
dpaa_event_eth_rx_adapter_caps_get(const struct rte_eventdev *dev,
				   const struct rte_eth_dev *eth_dev,
				   uint32_t *caps)
{
	const char *ethdev_driver = eth_dev->device->driver->name;

	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);

	if (!strcmp(ethdev_driver, "net_dpaa"))
		*caps = RTE_EVENT_ETH_RX_ADAPTER_DPAA_CAP;
	else
		*caps = RTE_EVENT_ETH_RX_ADAPTER_SW_CAP;

	return 0;
}

static int
dpaa_event_eth_rx_adapter_queue_add(
		const struct rte_eventdev *dev,
		const struct rte_eth_dev *eth_dev,
		int32_t rx_queue_id,
		const struct rte_event_eth_rx_adapter_queue_conf *queue_conf)
{
	struct dpaa_eventdev *eventdev = dev->data->dev_private;
	uint8_t ev_qid = queue_conf->ev.queue_id;
	u16 ch_id = eventdev->evq_info[ev_qid].ch_id;
	struct dpaa_if *dpaa_intf = eth_dev->data->dev_private;
	int ret, i;

	EVENTDEV_INIT_FUNC_TRACE();

	if (rx_queue_id == -1) {
		for (i = 0; i < dpaa_intf->nb_rx_queues; i++) {
			ret = dpaa_eth_eventq_attach(eth_dev, i, ch_id,
						     queue_conf);
			if (ret) {
				DPAA_EVENTDEV_ERR(
					"Event Queue attach failed:%d\n", ret);
				goto detach_configured_queues;
			}
		}
		return 0;
	}

	ret = dpaa_eth_eventq_attach(eth_dev, rx_queue_id, ch_id, queue_conf);
	if (ret)
		DPAA_EVENTDEV_ERR("dpaa_eth_eventq_attach failed:%d\n", ret);
	return ret;

detach_configured_queues:

	for (i = (i - 1); i >= 0 ; i--)
		dpaa_eth_eventq_detach(eth_dev, i);

	return ret;
}

static int
dpaa_event_eth_rx_adapter_queue_del(const struct rte_eventdev *dev,
				    const struct rte_eth_dev *eth_dev,
				    int32_t rx_queue_id)
{
	int ret, i;
	struct dpaa_if *dpaa_intf = eth_dev->data->dev_private;

	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);
	if (rx_queue_id == -1) {
		for (i = 0; i < dpaa_intf->nb_rx_queues; i++) {
			ret = dpaa_eth_eventq_detach(eth_dev, i);
			if (ret)
				DPAA_EVENTDEV_ERR(
					"Event Queue detach failed:%d\n", ret);
		}

		return 0;
	}

	ret = dpaa_eth_eventq_detach(eth_dev, rx_queue_id);
	if (ret)
		DPAA_EVENTDEV_ERR("dpaa_eth_eventq_detach failed:%d\n", ret);
	return ret;
}

static int
dpaa_event_eth_rx_adapter_start(const struct rte_eventdev *dev,
				const struct rte_eth_dev *eth_dev)
{
	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);
	RTE_SET_USED(eth_dev);

	return 0;
}

static int
dpaa_event_eth_rx_adapter_stop(const struct rte_eventdev *dev,
			       const struct rte_eth_dev *eth_dev)
{
	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);
	RTE_SET_USED(eth_dev);

	return 0;
}

static int
dpaa_eventdev_crypto_caps_get(const struct rte_eventdev *dev,
			    const struct rte_cryptodev *cdev,
			    uint32_t *caps)
{
	const char *name = cdev->data->name;

	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);

	if (!strncmp(name, "dpaa_sec-", 9))
		*caps = RTE_EVENT_CRYPTO_ADAPTER_DPAA_CAP;
	else
		return -1;

	return 0;
}

static int
dpaa_eventdev_crypto_queue_add_all(const struct rte_eventdev *dev,
		const struct rte_cryptodev *cryptodev,
		const struct rte_event *ev)
{
	struct dpaa_eventdev *priv = dev->data->dev_private;
	uint8_t ev_qid = ev->queue_id;
	u16 ch_id = priv->evq_info[ev_qid].ch_id;
	int i, ret;

	EVENTDEV_INIT_FUNC_TRACE();

	for (i = 0; i < cryptodev->data->nb_queue_pairs; i++) {
		ret = dpaa_sec_eventq_attach(cryptodev, i,
				ch_id, ev);
		if (ret) {
			DPAA_EVENTDEV_ERR("dpaa_sec_eventq_attach failed: ret %d\n",
				    ret);
			goto fail;
		}
	}
	return 0;
fail:
	for (i = (i - 1); i >= 0 ; i--)
		dpaa_sec_eventq_detach(cryptodev, i);

	return ret;
}

static int
dpaa_eventdev_crypto_queue_add(const struct rte_eventdev *dev,
		const struct rte_cryptodev *cryptodev,
		int32_t rx_queue_id,
		const struct rte_event_crypto_adapter_queue_conf *conf)
{
	struct dpaa_eventdev *priv = dev->data->dev_private;
	uint8_t ev_qid = conf->ev.queue_id;
	u16 ch_id = priv->evq_info[ev_qid].ch_id;
	int ret;

	EVENTDEV_INIT_FUNC_TRACE();

	if (rx_queue_id == -1)
		return dpaa_eventdev_crypto_queue_add_all(dev,
				cryptodev, &conf->ev);

	ret = dpaa_sec_eventq_attach(cryptodev, rx_queue_id,
			ch_id, &conf->ev);
	if (ret) {
		DPAA_EVENTDEV_ERR(
			"dpaa_sec_eventq_attach failed: ret: %d\n", ret);
		return ret;
	}
	return 0;
}

static int
dpaa_eventdev_crypto_queue_del_all(const struct rte_eventdev *dev,
			     const struct rte_cryptodev *cdev)
{
	int i, ret;

	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);

	for (i = 0; i < cdev->data->nb_queue_pairs; i++) {
		ret = dpaa_sec_eventq_detach(cdev, i);
		if (ret) {
			DPAA_EVENTDEV_ERR(
				"dpaa_sec_eventq_detach failed:ret %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static int
dpaa_eventdev_crypto_queue_del(const struct rte_eventdev *dev,
			     const struct rte_cryptodev *cryptodev,
			     int32_t rx_queue_id)
{
	int ret;

	EVENTDEV_INIT_FUNC_TRACE();

	if (rx_queue_id == -1)
		return dpaa_eventdev_crypto_queue_del_all(dev, cryptodev);

	ret = dpaa_sec_eventq_detach(cryptodev, rx_queue_id);
	if (ret) {
		DPAA_EVENTDEV_ERR(
			"dpaa_sec_eventq_detach failed: ret: %d\n", ret);
		return ret;
	}

	return 0;
}

static int
dpaa_eventdev_crypto_start(const struct rte_eventdev *dev,
			   const struct rte_cryptodev *cryptodev)
{
	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);
	RTE_SET_USED(cryptodev);

	return 0;
}

static int
dpaa_eventdev_crypto_stop(const struct rte_eventdev *dev,
			  const struct rte_cryptodev *cryptodev)
{
	EVENTDEV_INIT_FUNC_TRACE();

	RTE_SET_USED(dev);
	RTE_SET_USED(cryptodev);

	return 0;
}

static int
dpaa_eventdev_tx_adapter_create(uint8_t id,
				 const struct rte_eventdev *dev)
{
	RTE_SET_USED(id);
	RTE_SET_USED(dev);

	/* Nothing to do. Simply return. */
	return 0;
}

static int
dpaa_eventdev_tx_adapter_caps(const struct rte_eventdev *dev,
			       const struct rte_eth_dev *eth_dev,
			       uint32_t *caps)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(eth_dev);

	*caps = RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT;
	return 0;
}

static uint16_t
dpaa_eventdev_txa_enqueue_same_dest(void *port,
				     struct rte_event ev[],
				     uint16_t nb_events)
{
	struct rte_mbuf *m[DPAA_EVENT_MAX_PORT_ENQUEUE_DEPTH], *m0;
	uint8_t qid, i;

	RTE_SET_USED(port);

	m0 = (struct rte_mbuf *)ev[0].mbuf;
	qid = rte_event_eth_tx_adapter_txq_get(m0);

	for (i = 0; i < nb_events; i++)
		m[i] = (struct rte_mbuf *)ev[i].mbuf;

	return rte_eth_tx_burst(m0->port, qid, m, nb_events);
}

static uint16_t
dpaa_eventdev_txa_enqueue(void *port,
			   struct rte_event ev[],
			   uint16_t nb_events)
{
	struct rte_mbuf *m = (struct rte_mbuf *)ev[0].mbuf;
	uint8_t qid, i;

	RTE_SET_USED(port);

	for (i = 0; i < nb_events; i++) {
		qid = rte_event_eth_tx_adapter_txq_get(m);
		rte_eth_tx_burst(m->port, qid, &m, 1);
	}

	return nb_events;
}

static struct eventdev_ops dpaa_eventdev_ops = {
	.dev_infos_get    = dpaa_event_dev_info_get,
	.dev_configure    = dpaa_event_dev_configure,
	.dev_start        = dpaa_event_dev_start,
	.dev_stop         = dpaa_event_dev_stop,
	.dev_close        = dpaa_event_dev_close,
	.queue_def_conf   = dpaa_event_queue_def_conf,
	.queue_setup      = dpaa_event_queue_setup,
	.queue_release    = dpaa_event_queue_release,
	.port_def_conf    = dpaa_event_port_default_conf_get,
	.port_setup       = dpaa_event_port_setup,
	.port_release       = dpaa_event_port_release,
	.port_link        = dpaa_event_port_link,
	.port_unlink      = dpaa_event_port_unlink,
	.timeout_ticks    = dpaa_event_dequeue_timeout_ticks,
	.eth_rx_adapter_caps_get	= dpaa_event_eth_rx_adapter_caps_get,
	.eth_rx_adapter_queue_add	= dpaa_event_eth_rx_adapter_queue_add,
	.eth_rx_adapter_queue_del	= dpaa_event_eth_rx_adapter_queue_del,
	.eth_rx_adapter_start		= dpaa_event_eth_rx_adapter_start,
	.eth_rx_adapter_stop		= dpaa_event_eth_rx_adapter_stop,
	.eth_tx_adapter_caps_get	= dpaa_eventdev_tx_adapter_caps,
	.eth_tx_adapter_create		= dpaa_eventdev_tx_adapter_create,
	.crypto_adapter_caps_get	= dpaa_eventdev_crypto_caps_get,
	.crypto_adapter_queue_pair_add	= dpaa_eventdev_crypto_queue_add,
	.crypto_adapter_queue_pair_del	= dpaa_eventdev_crypto_queue_del,
	.crypto_adapter_start		= dpaa_eventdev_crypto_start,
	.crypto_adapter_stop		= dpaa_eventdev_crypto_stop,
};

static int flag_check_handler(__rte_unused const char *key,
		const char *value, __rte_unused void *opaque)
{
	if (strcmp(value, "1"))
		return -1;

	return 0;
}

static int
dpaa_event_check_flags(const char *params)
{
	struct rte_kvargs *kvlist;

	if (params == NULL || params[0] == '\0')
		return 0;

	kvlist = rte_kvargs_parse(params, NULL);
	if (kvlist == NULL)
		return 0;

	if (!rte_kvargs_count(kvlist, DISABLE_INTR_MODE)) {
		rte_kvargs_free(kvlist);
		return 0;
	}
	/* INTR MODE is disabled when there's key-value pair: disable_intr = 1*/
	if (rte_kvargs_process(kvlist, DISABLE_INTR_MODE,
				flag_check_handler, NULL) < 0) {
		rte_kvargs_free(kvlist);
		return 0;
	}
	rte_kvargs_free(kvlist);

	return 1;
}

static int
dpaa_event_dev_create(const char *name, const char *params)
{
	struct rte_eventdev *eventdev;
	struct dpaa_eventdev *priv;

	eventdev = rte_event_pmd_vdev_init(name,
					   sizeof(struct dpaa_eventdev),
					   rte_socket_id());
	if (eventdev == NULL) {
		DPAA_EVENTDEV_ERR("Failed to create eventdev vdev %s", name);
		goto fail;
	}
	priv = eventdev->data->dev_private;

	eventdev->dev_ops       = &dpaa_eventdev_ops;
	eventdev->enqueue       = dpaa_event_enqueue;
	eventdev->enqueue_burst = dpaa_event_enqueue_burst;

	if (dpaa_event_check_flags(params)) {
		eventdev->dequeue	= dpaa_event_dequeue;
		eventdev->dequeue_burst = dpaa_event_dequeue_burst;
	} else {
		priv->intr_mode = 1;
		eventdev->dev_ops->timeout_ticks =
				dpaa_event_dequeue_timeout_ticks_intr;
		eventdev->dequeue	= dpaa_event_dequeue_intr;
		eventdev->dequeue_burst = dpaa_event_dequeue_burst_intr;
	}
	eventdev->txa_enqueue = dpaa_eventdev_txa_enqueue;
	eventdev->txa_enqueue_same_dest	= dpaa_eventdev_txa_enqueue_same_dest;

	RTE_LOG(INFO, PMD, "%s eventdev added", name);

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		goto done;

	priv->max_event_queues = DPAA_EVENT_MAX_QUEUES;

done:
	event_dev_probing_finish(eventdev);
	return 0;
fail:
	return -EFAULT;
}

static int
dpaa_event_dev_probe(struct rte_vdev_device *vdev)
{
	const char *name;
	const char *params;

	name = rte_vdev_device_name(vdev);
	DPAA_EVENTDEV_INFO("Initializing %s", name);

	params = rte_vdev_device_args(vdev);

	return dpaa_event_dev_create(name, params);
}

static int
dpaa_event_dev_remove(struct rte_vdev_device *vdev)
{
	const char *name;

	name = rte_vdev_device_name(vdev);
	DPAA_EVENTDEV_INFO("Closing %s", name);

	return rte_event_pmd_vdev_uninit(name);
}

static struct rte_vdev_driver vdev_eventdev_dpaa_pmd = {
	.probe = dpaa_event_dev_probe,
	.remove = dpaa_event_dev_remove
};

RTE_PMD_REGISTER_VDEV(EVENTDEV_NAME_DPAA_PMD, vdev_eventdev_dpaa_pmd);
RTE_PMD_REGISTER_PARAM_STRING(EVENTDEV_NAME_DPAA_PMD,
		DISABLE_INTR_MODE "=<int>");
