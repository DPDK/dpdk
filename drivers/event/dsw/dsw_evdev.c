/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Ericsson AB
 */

#include <stdbool.h>

#include <rte_cycles.h>
#include <eventdev_pmd.h>
#include <eventdev_pmd_vdev.h>
#include <rte_random.h>
#include <rte_ring_elem.h>

#include "dsw_evdev.h"

#define EVENTDEV_NAME_DSW_PMD event_dsw

static int
dsw_port_setup(struct rte_eventdev *dev, uint8_t port_id,
	       const struct rte_event_port_conf *conf)
{
	struct dsw_evdev *dsw = dsw_pmd_priv(dev);
	struct dsw_port *port;
	struct rte_event_ring *in_ring;
	struct rte_ring *ctl_in_ring;
	char ring_name[RTE_RING_NAMESIZE];

	port = &dsw->ports[port_id];

	*port = (struct dsw_port) {
		.id = port_id,
		.dsw = dsw,
		.dequeue_depth = conf->dequeue_depth,
		.enqueue_depth = conf->enqueue_depth,
		.new_event_threshold = conf->new_event_threshold
	};

	snprintf(ring_name, sizeof(ring_name), "dsw%d_p%u", dev->data->dev_id,
		 port_id);

	in_ring = rte_event_ring_create(ring_name, DSW_IN_RING_SIZE,
					dev->data->socket_id,
					RING_F_SC_DEQ|RING_F_EXACT_SZ);

	if (in_ring == NULL)
		return -ENOMEM;

	snprintf(ring_name, sizeof(ring_name), "dswctl%d_p%u",
		 dev->data->dev_id, port_id);

	ctl_in_ring = rte_ring_create_elem(ring_name,
					   sizeof(struct dsw_ctl_msg),
					   DSW_CTL_IN_RING_SIZE,
					   dev->data->socket_id,
					   RING_F_SC_DEQ|RING_F_EXACT_SZ);

	if (ctl_in_ring == NULL) {
		rte_event_ring_free(in_ring);
		return -ENOMEM;
	}

	port->in_ring = in_ring;
	port->ctl_in_ring = ctl_in_ring;

	port->load_update_interval =
		(DSW_LOAD_UPDATE_INTERVAL * rte_get_timer_hz()) / US_PER_S;

	port->migration_interval =
		(DSW_MIGRATION_INTERVAL * rte_get_timer_hz()) / US_PER_S;

	dev->data->ports[port_id] = port;

	return 0;
}

static void
dsw_port_def_conf(struct rte_eventdev *dev __rte_unused,
		  uint8_t port_id __rte_unused,
		  struct rte_event_port_conf *port_conf)
{
	*port_conf = (struct rte_event_port_conf) {
		.new_event_threshold = 1024,
		.dequeue_depth = DSW_MAX_PORT_DEQUEUE_DEPTH / 4,
		.enqueue_depth = DSW_MAX_PORT_ENQUEUE_DEPTH / 4
	};
}

static void
dsw_port_release(void *p)
{
	struct dsw_port *port = p;

	rte_event_ring_free(port->in_ring);
	rte_ring_free(port->ctl_in_ring);
}

static int
dsw_queue_setup(struct rte_eventdev *dev, uint8_t queue_id,
		const struct rte_event_queue_conf *conf)
{
	struct dsw_evdev *dsw = dsw_pmd_priv(dev);
	struct dsw_queue *queue = &dsw->queues[queue_id];

	if (RTE_EVENT_QUEUE_CFG_ALL_TYPES & conf->event_queue_cfg)
		return -ENOTSUP;

	/* SINGLE_LINK is better off treated as TYPE_ATOMIC, since it
	 * avoid the "fake" TYPE_PARALLEL flow_id assignment. Since
	 * the queue will only have a single serving port, no
	 * migration will ever happen, so the extra TYPE_ATOMIC
	 * migration overhead is avoided.
	 */
	if (RTE_EVENT_QUEUE_CFG_SINGLE_LINK & conf->event_queue_cfg)
		queue->schedule_type = RTE_SCHED_TYPE_ATOMIC;
	else {
		if (conf->schedule_type == RTE_SCHED_TYPE_ORDERED)
			return -ENOTSUP;
		/* atomic or parallel */
		queue->schedule_type = conf->schedule_type;
	}

	queue->num_serving_ports = 0;

	return 0;
}

static void
dsw_queue_def_conf(struct rte_eventdev *dev __rte_unused,
		   uint8_t queue_id __rte_unused,
		   struct rte_event_queue_conf *queue_conf)
{
	*queue_conf = (struct rte_event_queue_conf) {
		.nb_atomic_flows = 4096,
		.schedule_type = RTE_SCHED_TYPE_ATOMIC,
		.priority = RTE_EVENT_DEV_PRIORITY_NORMAL
	};
}

static void
dsw_queue_release(struct rte_eventdev *dev __rte_unused,
		  uint8_t queue_id __rte_unused)
{
}

static void
queue_add_port(struct dsw_queue *queue, uint16_t port_id)
{
	queue->serving_ports[queue->num_serving_ports] = port_id;
	queue->num_serving_ports++;
}

static bool
queue_remove_port(struct dsw_queue *queue, uint16_t port_id)
{
	uint16_t i;

	for (i = 0; i < queue->num_serving_ports; i++)
		if (queue->serving_ports[i] == port_id) {
			uint16_t last_idx = queue->num_serving_ports - 1;
			if (i != last_idx)
				queue->serving_ports[i] =
					queue->serving_ports[last_idx];
			queue->num_serving_ports--;
			return true;
		}
	return false;
}

static int
dsw_port_link_unlink(struct rte_eventdev *dev, void *port,
		     const uint8_t queues[], uint16_t num, bool link)
{
	struct dsw_evdev *dsw = dsw_pmd_priv(dev);
	struct dsw_port *p = port;
	uint16_t i;
	uint16_t count = 0;

	for (i = 0; i < num; i++) {
		uint8_t qid = queues[i];
		struct dsw_queue *q = &dsw->queues[qid];
		if (link) {
			queue_add_port(q, p->id);
			count++;
		} else {
			bool removed = queue_remove_port(q, p->id);
			if (removed)
				count++;
		}
	}

	return count;
}

static int
dsw_port_link(struct rte_eventdev *dev, void *port, const uint8_t queues[],
	      const uint8_t priorities[] __rte_unused, uint16_t num)
{
	return dsw_port_link_unlink(dev, port, queues, num, true);
}

static int
dsw_port_unlink(struct rte_eventdev *dev, void *port, uint8_t queues[],
		uint16_t num)
{
	return dsw_port_link_unlink(dev, port, queues, num, false);
}

static void
dsw_info_get(struct rte_eventdev *dev __rte_unused,
	     struct rte_event_dev_info *info)
{
	*info = (struct rte_event_dev_info) {
		.driver_name = DSW_PMD_NAME,
		.max_event_queues = DSW_MAX_QUEUES,
		.max_event_queue_flows = DSW_MAX_FLOWS,
		.max_event_queue_priority_levels = 1,
		.max_event_priority_levels = 1,
		.max_event_ports = DSW_MAX_PORTS,
		.max_event_port_dequeue_depth = DSW_MAX_PORT_DEQUEUE_DEPTH,
		.max_event_port_enqueue_depth = DSW_MAX_PORT_ENQUEUE_DEPTH,
		.max_num_events = DSW_MAX_EVENTS,
		.max_profiles_per_port = 1,
		.event_dev_cap = RTE_EVENT_DEV_CAP_BURST_MODE|
		RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED|
		RTE_EVENT_DEV_CAP_NONSEQ_MODE|
		RTE_EVENT_DEV_CAP_MULTIPLE_QUEUE_PORT|
		RTE_EVENT_DEV_CAP_CARRY_FLOW_ID
	};
}

static int
dsw_configure(const struct rte_eventdev *dev)
{
	struct dsw_evdev *dsw = dsw_pmd_priv(dev);
	const struct rte_event_dev_config *conf = &dev->data->dev_conf;
	int32_t min_max_in_flight;

	dsw->num_ports = conf->nb_event_ports;
	dsw->num_queues = conf->nb_event_queues;

	/* Avoid a situation where consumer ports are holding all the
	 * credits, without making use of them.
	 */
	min_max_in_flight = conf->nb_event_ports * DSW_PORT_MAX_CREDITS;

	dsw->max_inflight = RTE_MAX(conf->nb_events_limit, min_max_in_flight);

	return 0;
}


static void
initial_flow_to_port_assignment(struct dsw_evdev *dsw)
{
	uint8_t queue_id;
	for (queue_id = 0; queue_id < dsw->num_queues; queue_id++) {
		struct dsw_queue *queue = &dsw->queues[queue_id];
		uint16_t flow_hash;
		for (flow_hash = 0; flow_hash < DSW_MAX_FLOWS; flow_hash++) {
			uint8_t port_idx =
				rte_rand() % queue->num_serving_ports;
			uint8_t port_id =
				queue->serving_ports[port_idx];
			dsw->queues[queue_id].flow_to_port_map[flow_hash] =
				port_id;
		}
	}
}

static int
dsw_start(struct rte_eventdev *dev)
{
	struct dsw_evdev *dsw = dsw_pmd_priv(dev);
	uint16_t i;
	uint64_t now;

	dsw->credits_on_loan = 0;

	initial_flow_to_port_assignment(dsw);

	now = rte_get_timer_cycles();
	for (i = 0; i < dsw->num_ports; i++) {
		dsw->ports[i].measurement_start = now;
		dsw->ports[i].busy_start = now;
	}

	return 0;
}

static void
dsw_port_drain_buf(uint8_t dev_id, struct rte_event *buf, uint16_t buf_len,
		   eventdev_stop_flush_t flush, void *flush_arg)
{
	uint16_t i;

	for (i = 0; i < buf_len; i++)
		flush(dev_id, buf[i], flush_arg);
}

static void
dsw_port_drain_paused(uint8_t dev_id, struct dsw_port *port,
		      eventdev_stop_flush_t flush, void *flush_arg)
{
	dsw_port_drain_buf(dev_id, port->paused_events, port->paused_events_len,
			   flush, flush_arg);
}

static void
dsw_port_drain_out(uint8_t dev_id, struct dsw_evdev *dsw, struct dsw_port *port,
		   eventdev_stop_flush_t flush, void *flush_arg)
{
	uint16_t dport_id;

	for (dport_id = 0; dport_id < dsw->num_ports; dport_id++)
		if (dport_id != port->id)
			dsw_port_drain_buf(dev_id, port->out_buffer[dport_id],
					   port->out_buffer_len[dport_id],
					   flush, flush_arg);
}

static void
dsw_port_drain_in_ring(uint8_t dev_id, struct dsw_port *port,
		       eventdev_stop_flush_t flush, void *flush_arg)
{
	struct rte_event ev;

	while (rte_event_ring_dequeue_burst(port->in_ring, &ev, 1, NULL))
		flush(dev_id, ev, flush_arg);
}

static void
dsw_drain(uint8_t dev_id, struct dsw_evdev *dsw,
	  eventdev_stop_flush_t flush, void *flush_arg)
{
	uint16_t port_id;

	if (flush == NULL)
		return;

	for (port_id = 0; port_id < dsw->num_ports; port_id++) {
		struct dsw_port *port = &dsw->ports[port_id];

		dsw_port_drain_out(dev_id, dsw, port, flush, flush_arg);
		dsw_port_drain_paused(dev_id, port, flush, flush_arg);
		dsw_port_drain_in_ring(dev_id, port, flush, flush_arg);
	}
}

static void
dsw_stop(struct rte_eventdev *dev)
{
	struct dsw_evdev *dsw = dsw_pmd_priv(dev);
	uint8_t dev_id;
	eventdev_stop_flush_t flush;
	void *flush_arg;

	dev_id = dev->data->dev_id;
	flush = dev->dev_ops->dev_stop_flush;
	flush_arg = dev->data->dev_stop_flush_arg;

	dsw_drain(dev_id, dsw, flush, flush_arg);
}

static int
dsw_close(struct rte_eventdev *dev)
{
	struct dsw_evdev *dsw = dsw_pmd_priv(dev);
	uint16_t port_id;

	for (port_id = 0; port_id < dsw->num_ports; port_id++)
		dsw_port_release(&dsw->ports[port_id]);

	dsw->num_ports = 0;
	dsw->num_queues = 0;

	return 0;
}

static int
dsw_eth_rx_adapter_caps_get(const struct rte_eventdev *dev __rte_unused,
			    const struct rte_eth_dev *eth_dev __rte_unused,
			    uint32_t *caps)
{
	*caps = RTE_EVENT_ETH_RX_ADAPTER_SW_CAP;
	return 0;
}

static int
dsw_timer_adapter_caps_get(const struct rte_eventdev *dev __rte_unused,
			   uint64_t flags __rte_unused, uint32_t *caps,
			   const struct event_timer_adapter_ops **ops)
{
	*caps = 0;
	*ops = NULL;
	return 0;
}

static int
dsw_crypto_adapter_caps_get(const struct rte_eventdev *dev  __rte_unused,
			    const struct rte_cryptodev *cdev  __rte_unused,
			    uint32_t *caps)
{
	*caps = RTE_EVENT_CRYPTO_ADAPTER_SW_CAP;
	return 0;
}

static struct eventdev_ops dsw_evdev_ops = {
	.port_setup = dsw_port_setup,
	.port_def_conf = dsw_port_def_conf,
	.port_release = dsw_port_release,
	.queue_setup = dsw_queue_setup,
	.queue_def_conf = dsw_queue_def_conf,
	.queue_release = dsw_queue_release,
	.port_link = dsw_port_link,
	.port_unlink = dsw_port_unlink,
	.dev_infos_get = dsw_info_get,
	.dev_configure = dsw_configure,
	.dev_start = dsw_start,
	.dev_stop = dsw_stop,
	.dev_close = dsw_close,
	.eth_rx_adapter_caps_get = dsw_eth_rx_adapter_caps_get,
	.timer_adapter_caps_get = dsw_timer_adapter_caps_get,
	.crypto_adapter_caps_get = dsw_crypto_adapter_caps_get,
	.xstats_get = dsw_xstats_get,
	.xstats_get_names = dsw_xstats_get_names,
	.xstats_get_by_name = dsw_xstats_get_by_name
};

static int
dsw_probe(struct rte_vdev_device *vdev)
{
	const char *name;
	struct rte_eventdev *dev;
	struct dsw_evdev *dsw;

	name = rte_vdev_device_name(vdev);

	dev = rte_event_pmd_vdev_init(name, sizeof(struct dsw_evdev),
				      rte_socket_id(), vdev);
	if (dev == NULL)
		return -EFAULT;

	dev->dev_ops = &dsw_evdev_ops;
	dev->enqueue = dsw_event_enqueue;
	dev->enqueue_burst = dsw_event_enqueue_burst;
	dev->enqueue_new_burst = dsw_event_enqueue_new_burst;
	dev->enqueue_forward_burst = dsw_event_enqueue_forward_burst;
	dev->dequeue = dsw_event_dequeue;
	dev->dequeue_burst = dsw_event_dequeue_burst;
	dev->maintain = dsw_event_maintain;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	dsw = dev->data->dev_private;
	dsw->data = dev->data;

	event_dev_probing_finish(dev);
	return 0;
}

static int
dsw_remove(struct rte_vdev_device *vdev)
{
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	return rte_event_pmd_vdev_uninit(name);
}

static struct rte_vdev_driver evdev_dsw_pmd_drv = {
	.probe = dsw_probe,
	.remove = dsw_remove
};

RTE_PMD_REGISTER_VDEV(EVENTDEV_NAME_DSW_PMD, evdev_dsw_pmd_drv);
