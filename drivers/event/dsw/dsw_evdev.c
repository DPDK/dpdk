/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Ericsson AB
 */

#include <rte_eventdev_pmd.h>
#include <rte_eventdev_pmd_vdev.h>

#include "dsw_evdev.h"

#define EVENTDEV_NAME_DSW_PMD event_dsw

static int
dsw_port_setup(struct rte_eventdev *dev, uint8_t port_id,
	       const struct rte_event_port_conf *conf)
{
	struct dsw_evdev *dsw = dsw_pmd_priv(dev);
	struct dsw_port *port;
	struct rte_event_ring *in_ring;
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

	port->in_ring = in_ring;

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
}

static int
dsw_queue_setup(struct rte_eventdev *dev, uint8_t queue_id,
		const struct rte_event_queue_conf *conf)
{
	struct dsw_evdev *dsw = dsw_pmd_priv(dev);
	struct dsw_queue *queue = &dsw->queues[queue_id];

	if (RTE_EVENT_QUEUE_CFG_ALL_TYPES & conf->event_queue_cfg)
		return -ENOTSUP;

	if (conf->schedule_type == RTE_SCHED_TYPE_ORDERED)
		return -ENOTSUP;

	/* SINGLE_LINK is better off treated as TYPE_ATOMIC, since it
	 * avoid the "fake" TYPE_PARALLEL flow_id assignment. Since
	 * the queue will only have a single serving port, no
	 * migration will ever happen, so the extra TYPE_ATOMIC
	 * migration overhead is avoided.
	 */
	if (RTE_EVENT_QUEUE_CFG_SINGLE_LINK & conf->event_queue_cfg)
		queue->schedule_type = RTE_SCHED_TYPE_ATOMIC;
	else /* atomic or parallel */
		queue->schedule_type = conf->schedule_type;

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
		.event_dev_cap = RTE_EVENT_DEV_CAP_BURST_MODE|
		RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED
	};
}

static int
dsw_configure(const struct rte_eventdev *dev)
{
	struct dsw_evdev *dsw = dsw_pmd_priv(dev);
	const struct rte_event_dev_config *conf = &dev->data->dev_conf;

	dsw->num_ports = conf->nb_event_ports;
	dsw->num_queues = conf->nb_event_queues;

	return 0;
}

static struct rte_eventdev_ops dsw_evdev_ops = {
	.port_setup = dsw_port_setup,
	.port_def_conf = dsw_port_def_conf,
	.port_release = dsw_port_release,
	.queue_setup = dsw_queue_setup,
	.queue_def_conf = dsw_queue_def_conf,
	.queue_release = dsw_queue_release,
	.dev_infos_get = dsw_info_get,
	.dev_configure = dsw_configure,
};

static int
dsw_probe(struct rte_vdev_device *vdev)
{
	const char *name;
	struct rte_eventdev *dev;
	struct dsw_evdev *dsw;

	name = rte_vdev_device_name(vdev);

	dev = rte_event_pmd_vdev_init(name, sizeof(struct dsw_evdev),
				      rte_socket_id());
	if (dev == NULL)
		return -EFAULT;

	dev->dev_ops = &dsw_evdev_ops;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	dsw = dev->data->dev_private;
	dsw->data = dev->data;

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
