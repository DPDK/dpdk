/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>

#include <rte_string_fns.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_dev.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>

#include "rte_eventdev.h"
#include "rte_eventdev_pmd.h"

static struct rte_eventdev rte_event_devices[RTE_EVENT_MAX_DEVS];

struct rte_eventdev *rte_eventdevs = rte_event_devices;

static struct rte_eventdev_global eventdev_globals = {
	.nb_devs		= 0
};

/* Event dev north bound API implementation */

uint8_t
rte_event_dev_count(void)
{
	return eventdev_globals.nb_devs;
}

int
rte_event_dev_get_dev_id(const char *name)
{
	int i;
	uint8_t cmp;

	if (!name)
		return -EINVAL;

	for (i = 0; i < eventdev_globals.nb_devs; i++) {
		cmp = (strncmp(rte_event_devices[i].data->name, name,
				RTE_EVENTDEV_NAME_MAX_LEN) == 0) ||
			(rte_event_devices[i].dev ? (strncmp(
				rte_event_devices[i].dev->driver->name, name,
					 RTE_EVENTDEV_NAME_MAX_LEN) == 0) : 0);
		if (cmp && (rte_event_devices[i].attached ==
					RTE_EVENTDEV_ATTACHED))
			return i;
	}
	return -ENODEV;
}

int
rte_event_dev_socket_id(uint8_t dev_id)
{
	struct rte_eventdev *dev;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];

	return dev->data->socket_id;
}

int
rte_event_dev_info_get(uint8_t dev_id, struct rte_event_dev_info *dev_info)
{
	struct rte_eventdev *dev;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];

	if (dev_info == NULL)
		return -EINVAL;

	memset(dev_info, 0, sizeof(struct rte_event_dev_info));

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_infos_get, -ENOTSUP);
	(*dev->dev_ops->dev_infos_get)(dev, dev_info);

	dev_info->dequeue_timeout_ns = dev->data->dev_conf.dequeue_timeout_ns;

	dev_info->dev = dev->dev;
	return 0;
}

int
rte_event_eth_rx_adapter_caps_get(uint8_t dev_id, uint16_t eth_port_id,
				uint32_t *caps)
{
	struct rte_eventdev *dev;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	RTE_ETH_VALID_PORTID_OR_ERR_RET(eth_port_id, -EINVAL);

	dev = &rte_eventdevs[dev_id];

	if (caps == NULL)
		return -EINVAL;
	*caps = 0;

	return dev->dev_ops->eth_rx_adapter_caps_get ?
				(*dev->dev_ops->eth_rx_adapter_caps_get)(dev,
						&rte_eth_devices[eth_port_id],
						caps)
				: 0;
}

int
rte_event_timer_adapter_caps_get(uint8_t dev_id, uint32_t *caps)
{
	struct rte_eventdev *dev;
	const struct rte_event_timer_adapter_ops *ops;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);

	dev = &rte_eventdevs[dev_id];

	if (caps == NULL)
		return -EINVAL;
	*caps = 0;

	return dev->dev_ops->timer_adapter_caps_get ?
				(*dev->dev_ops->timer_adapter_caps_get)(dev,
									0,
									caps,
									&ops)
				: 0;
}

int
rte_event_crypto_adapter_caps_get(uint8_t dev_id, uint8_t cdev_id,
				  uint32_t *caps)
{
	struct rte_eventdev *dev;
	struct rte_cryptodev *cdev;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	if (!rte_cryptodev_pmd_is_valid_dev(cdev_id))
		return -EINVAL;

	dev = &rte_eventdevs[dev_id];
	cdev = rte_cryptodev_pmd_get_dev(cdev_id);

	if (caps == NULL)
		return -EINVAL;
	*caps = 0;

	return dev->dev_ops->crypto_adapter_caps_get ?
		(*dev->dev_ops->crypto_adapter_caps_get)
		(dev, cdev, caps) : -ENOTSUP;
}

int
rte_event_eth_tx_adapter_caps_get(uint8_t dev_id, uint16_t eth_port_id,
				uint32_t *caps)
{
	struct rte_eventdev *dev;
	struct rte_eth_dev *eth_dev;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	RTE_ETH_VALID_PORTID_OR_ERR_RET(eth_port_id, -EINVAL);

	dev = &rte_eventdevs[dev_id];
	eth_dev = &rte_eth_devices[eth_port_id];

	if (caps == NULL)
		return -EINVAL;

	*caps = 0;

	return dev->dev_ops->eth_tx_adapter_caps_get ?
			(*dev->dev_ops->eth_tx_adapter_caps_get)(dev,
								eth_dev,
								caps)
			: 0;
}

static inline int
rte_event_dev_queue_config(struct rte_eventdev *dev, uint8_t nb_queues)
{
	uint8_t old_nb_queues = dev->data->nb_queues;
	struct rte_event_queue_conf *queues_cfg;
	unsigned int i;

	RTE_EDEV_LOG_DEBUG("Setup %d queues on device %u", nb_queues,
			 dev->data->dev_id);

	/* First time configuration */
	if (dev->data->queues_cfg == NULL && nb_queues != 0) {
		/* Allocate memory to store queue configuration */
		dev->data->queues_cfg = rte_zmalloc_socket(
				"eventdev->data->queues_cfg",
				sizeof(dev->data->queues_cfg[0]) * nb_queues,
				RTE_CACHE_LINE_SIZE, dev->data->socket_id);
		if (dev->data->queues_cfg == NULL) {
			dev->data->nb_queues = 0;
			RTE_EDEV_LOG_ERR("failed to get mem for queue cfg,"
					"nb_queues %u", nb_queues);
			return -(ENOMEM);
		}
	/* Re-configure */
	} else if (dev->data->queues_cfg != NULL && nb_queues != 0) {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->queue_release, -ENOTSUP);

		for (i = nb_queues; i < old_nb_queues; i++)
			(*dev->dev_ops->queue_release)(dev, i);

		/* Re allocate memory to store queue configuration */
		queues_cfg = dev->data->queues_cfg;
		queues_cfg = rte_realloc(queues_cfg,
				sizeof(queues_cfg[0]) * nb_queues,
				RTE_CACHE_LINE_SIZE);
		if (queues_cfg == NULL) {
			RTE_EDEV_LOG_ERR("failed to realloc queue cfg memory,"
						" nb_queues %u", nb_queues);
			return -(ENOMEM);
		}
		dev->data->queues_cfg = queues_cfg;

		if (nb_queues > old_nb_queues) {
			uint8_t new_qs = nb_queues - old_nb_queues;

			memset(queues_cfg + old_nb_queues, 0,
				sizeof(queues_cfg[0]) * new_qs);
		}
	} else if (dev->data->queues_cfg != NULL && nb_queues == 0) {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->queue_release, -ENOTSUP);

		for (i = nb_queues; i < old_nb_queues; i++)
			(*dev->dev_ops->queue_release)(dev, i);
	}

	dev->data->nb_queues = nb_queues;
	return 0;
}

#define EVENT_QUEUE_SERVICE_PRIORITY_INVALID (0xdead)

static inline int
rte_event_dev_port_config(struct rte_eventdev *dev, uint8_t nb_ports)
{
	uint8_t old_nb_ports = dev->data->nb_ports;
	void **ports;
	uint16_t *links_map;
	struct rte_event_port_conf *ports_cfg;
	unsigned int i;

	RTE_EDEV_LOG_DEBUG("Setup %d ports on device %u", nb_ports,
			 dev->data->dev_id);

	/* First time configuration */
	if (dev->data->ports == NULL && nb_ports != 0) {
		dev->data->ports = rte_zmalloc_socket("eventdev->data->ports",
				sizeof(dev->data->ports[0]) * nb_ports,
				RTE_CACHE_LINE_SIZE, dev->data->socket_id);
		if (dev->data->ports == NULL) {
			dev->data->nb_ports = 0;
			RTE_EDEV_LOG_ERR("failed to get mem for port meta data,"
					"nb_ports %u", nb_ports);
			return -(ENOMEM);
		}

		/* Allocate memory to store port configurations */
		dev->data->ports_cfg =
			rte_zmalloc_socket("eventdev->ports_cfg",
			sizeof(dev->data->ports_cfg[0]) * nb_ports,
			RTE_CACHE_LINE_SIZE, dev->data->socket_id);
		if (dev->data->ports_cfg == NULL) {
			dev->data->nb_ports = 0;
			RTE_EDEV_LOG_ERR("failed to get mem for port cfg,"
					"nb_ports %u", nb_ports);
			return -(ENOMEM);
		}

		/* Allocate memory to store queue to port link connection */
		dev->data->links_map =
			rte_zmalloc_socket("eventdev->links_map",
			sizeof(dev->data->links_map[0]) * nb_ports *
			RTE_EVENT_MAX_QUEUES_PER_DEV,
			RTE_CACHE_LINE_SIZE, dev->data->socket_id);
		if (dev->data->links_map == NULL) {
			dev->data->nb_ports = 0;
			RTE_EDEV_LOG_ERR("failed to get mem for port_map area,"
					"nb_ports %u", nb_ports);
			return -(ENOMEM);
		}
		for (i = 0; i < nb_ports * RTE_EVENT_MAX_QUEUES_PER_DEV; i++)
			dev->data->links_map[i] =
				EVENT_QUEUE_SERVICE_PRIORITY_INVALID;
	} else if (dev->data->ports != NULL && nb_ports != 0) {/* re-config */
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->port_release, -ENOTSUP);

		ports = dev->data->ports;
		ports_cfg = dev->data->ports_cfg;
		links_map = dev->data->links_map;

		for (i = nb_ports; i < old_nb_ports; i++)
			(*dev->dev_ops->port_release)(ports[i]);

		/* Realloc memory for ports */
		ports = rte_realloc(ports, sizeof(ports[0]) * nb_ports,
				RTE_CACHE_LINE_SIZE);
		if (ports == NULL) {
			RTE_EDEV_LOG_ERR("failed to realloc port meta data,"
						" nb_ports %u", nb_ports);
			return -(ENOMEM);
		}

		/* Realloc memory for ports_cfg */
		ports_cfg = rte_realloc(ports_cfg,
			sizeof(ports_cfg[0]) * nb_ports,
			RTE_CACHE_LINE_SIZE);
		if (ports_cfg == NULL) {
			RTE_EDEV_LOG_ERR("failed to realloc port cfg mem,"
						" nb_ports %u", nb_ports);
			return -(ENOMEM);
		}

		/* Realloc memory to store queue to port link connection */
		links_map = rte_realloc(links_map,
			sizeof(dev->data->links_map[0]) * nb_ports *
			RTE_EVENT_MAX_QUEUES_PER_DEV,
			RTE_CACHE_LINE_SIZE);
		if (links_map == NULL) {
			dev->data->nb_ports = 0;
			RTE_EDEV_LOG_ERR("failed to realloc mem for port_map,"
					"nb_ports %u", nb_ports);
			return -(ENOMEM);
		}

		if (nb_ports > old_nb_ports) {
			uint8_t new_ps = nb_ports - old_nb_ports;
			unsigned int old_links_map_end =
				old_nb_ports * RTE_EVENT_MAX_QUEUES_PER_DEV;
			unsigned int links_map_end =
				nb_ports * RTE_EVENT_MAX_QUEUES_PER_DEV;

			memset(ports + old_nb_ports, 0,
				sizeof(ports[0]) * new_ps);
			memset(ports_cfg + old_nb_ports, 0,
				sizeof(ports_cfg[0]) * new_ps);
			for (i = old_links_map_end; i < links_map_end; i++)
				links_map[i] =
					EVENT_QUEUE_SERVICE_PRIORITY_INVALID;
		}

		dev->data->ports = ports;
		dev->data->ports_cfg = ports_cfg;
		dev->data->links_map = links_map;
	} else if (dev->data->ports != NULL && nb_ports == 0) {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->port_release, -ENOTSUP);

		ports = dev->data->ports;
		for (i = nb_ports; i < old_nb_ports; i++)
			(*dev->dev_ops->port_release)(ports[i]);
	}

	dev->data->nb_ports = nb_ports;
	return 0;
}

int
rte_event_dev_configure(uint8_t dev_id,
			const struct rte_event_dev_config *dev_conf)
{
	struct rte_eventdev *dev;
	struct rte_event_dev_info info;
	int diag;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_infos_get, -ENOTSUP);
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_configure, -ENOTSUP);

	if (dev->data->dev_started) {
		RTE_EDEV_LOG_ERR(
		    "device %d must be stopped to allow configuration", dev_id);
		return -EBUSY;
	}

	if (dev_conf == NULL)
		return -EINVAL;

	(*dev->dev_ops->dev_infos_get)(dev, &info);

	/* Check dequeue_timeout_ns value is in limit */
	if (!(dev_conf->event_dev_cfg & RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT)) {
		if (dev_conf->dequeue_timeout_ns &&
		    (dev_conf->dequeue_timeout_ns < info.min_dequeue_timeout_ns
			|| dev_conf->dequeue_timeout_ns >
				 info.max_dequeue_timeout_ns)) {
			RTE_EDEV_LOG_ERR("dev%d invalid dequeue_timeout_ns=%d"
			" min_dequeue_timeout_ns=%d max_dequeue_timeout_ns=%d",
			dev_id, dev_conf->dequeue_timeout_ns,
			info.min_dequeue_timeout_ns,
			info.max_dequeue_timeout_ns);
			return -EINVAL;
		}
	}

	/* Check nb_events_limit is in limit */
	if (dev_conf->nb_events_limit > info.max_num_events) {
		RTE_EDEV_LOG_ERR("dev%d nb_events_limit=%d > max_num_events=%d",
		dev_id, dev_conf->nb_events_limit, info.max_num_events);
		return -EINVAL;
	}

	/* Check nb_event_queues is in limit */
	if (!dev_conf->nb_event_queues) {
		RTE_EDEV_LOG_ERR("dev%d nb_event_queues cannot be zero",
					dev_id);
		return -EINVAL;
	}
	if (dev_conf->nb_event_queues > info.max_event_queues) {
		RTE_EDEV_LOG_ERR("%d nb_event_queues=%d > max_event_queues=%d",
		dev_id, dev_conf->nb_event_queues, info.max_event_queues);
		return -EINVAL;
	}

	/* Check nb_event_ports is in limit */
	if (!dev_conf->nb_event_ports) {
		RTE_EDEV_LOG_ERR("dev%d nb_event_ports cannot be zero", dev_id);
		return -EINVAL;
	}
	if (dev_conf->nb_event_ports > info.max_event_ports) {
		RTE_EDEV_LOG_ERR("id%d nb_event_ports=%d > max_event_ports= %d",
		dev_id, dev_conf->nb_event_ports, info.max_event_ports);
		return -EINVAL;
	}

	/* Check nb_event_queue_flows is in limit */
	if (!dev_conf->nb_event_queue_flows) {
		RTE_EDEV_LOG_ERR("dev%d nb_flows cannot be zero", dev_id);
		return -EINVAL;
	}
	if (dev_conf->nb_event_queue_flows > info.max_event_queue_flows) {
		RTE_EDEV_LOG_ERR("dev%d nb_flows=%x > max_flows=%x",
		dev_id, dev_conf->nb_event_queue_flows,
		info.max_event_queue_flows);
		return -EINVAL;
	}

	/* Check nb_event_port_dequeue_depth is in limit */
	if (!dev_conf->nb_event_port_dequeue_depth) {
		RTE_EDEV_LOG_ERR("dev%d nb_dequeue_depth cannot be zero",
					dev_id);
		return -EINVAL;
	}
	if ((info.event_dev_cap & RTE_EVENT_DEV_CAP_BURST_MODE) &&
		 (dev_conf->nb_event_port_dequeue_depth >
			 info.max_event_port_dequeue_depth)) {
		RTE_EDEV_LOG_ERR("dev%d nb_dq_depth=%d > max_dq_depth=%d",
		dev_id, dev_conf->nb_event_port_dequeue_depth,
		info.max_event_port_dequeue_depth);
		return -EINVAL;
	}

	/* Check nb_event_port_enqueue_depth is in limit */
	if (!dev_conf->nb_event_port_enqueue_depth) {
		RTE_EDEV_LOG_ERR("dev%d nb_enqueue_depth cannot be zero",
					dev_id);
		return -EINVAL;
	}
	if ((info.event_dev_cap & RTE_EVENT_DEV_CAP_BURST_MODE) &&
		(dev_conf->nb_event_port_enqueue_depth >
			 info.max_event_port_enqueue_depth)) {
		RTE_EDEV_LOG_ERR("dev%d nb_enq_depth=%d > max_enq_depth=%d",
		dev_id, dev_conf->nb_event_port_enqueue_depth,
		info.max_event_port_enqueue_depth);
		return -EINVAL;
	}

	/* Copy the dev_conf parameter into the dev structure */
	memcpy(&dev->data->dev_conf, dev_conf, sizeof(dev->data->dev_conf));

	/* Setup new number of queues and reconfigure device. */
	diag = rte_event_dev_queue_config(dev, dev_conf->nb_event_queues);
	if (diag != 0) {
		RTE_EDEV_LOG_ERR("dev%d rte_event_dev_queue_config = %d",
				dev_id, diag);
		return diag;
	}

	/* Setup new number of ports and reconfigure device. */
	diag = rte_event_dev_port_config(dev, dev_conf->nb_event_ports);
	if (diag != 0) {
		rte_event_dev_queue_config(dev, 0);
		RTE_EDEV_LOG_ERR("dev%d rte_event_dev_port_config = %d",
				dev_id, diag);
		return diag;
	}

	/* Configure the device */
	diag = (*dev->dev_ops->dev_configure)(dev);
	if (diag != 0) {
		RTE_EDEV_LOG_ERR("dev%d dev_configure = %d", dev_id, diag);
		rte_event_dev_queue_config(dev, 0);
		rte_event_dev_port_config(dev, 0);
	}

	dev->data->event_dev_cap = info.event_dev_cap;
	return diag;
}

static inline int
is_valid_queue(struct rte_eventdev *dev, uint8_t queue_id)
{
	if (queue_id < dev->data->nb_queues && queue_id <
				RTE_EVENT_MAX_QUEUES_PER_DEV)
		return 1;
	else
		return 0;
}

int
rte_event_queue_default_conf_get(uint8_t dev_id, uint8_t queue_id,
				 struct rte_event_queue_conf *queue_conf)
{
	struct rte_eventdev *dev;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];

	if (queue_conf == NULL)
		return -EINVAL;

	if (!is_valid_queue(dev, queue_id)) {
		RTE_EDEV_LOG_ERR("Invalid queue_id=%" PRIu8, queue_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->queue_def_conf, -ENOTSUP);
	memset(queue_conf, 0, sizeof(struct rte_event_queue_conf));
	(*dev->dev_ops->queue_def_conf)(dev, queue_id, queue_conf);
	return 0;
}

static inline int
is_valid_atomic_queue_conf(const struct rte_event_queue_conf *queue_conf)
{
	if (queue_conf &&
		!(queue_conf->event_queue_cfg &
		  RTE_EVENT_QUEUE_CFG_SINGLE_LINK) &&
		((queue_conf->event_queue_cfg &
			 RTE_EVENT_QUEUE_CFG_ALL_TYPES) ||
		(queue_conf->schedule_type
			== RTE_SCHED_TYPE_ATOMIC)
		))
		return 1;
	else
		return 0;
}

static inline int
is_valid_ordered_queue_conf(const struct rte_event_queue_conf *queue_conf)
{
	if (queue_conf &&
		!(queue_conf->event_queue_cfg &
		  RTE_EVENT_QUEUE_CFG_SINGLE_LINK) &&
		((queue_conf->event_queue_cfg &
			 RTE_EVENT_QUEUE_CFG_ALL_TYPES) ||
		(queue_conf->schedule_type
			== RTE_SCHED_TYPE_ORDERED)
		))
		return 1;
	else
		return 0;
}


int
rte_event_queue_setup(uint8_t dev_id, uint8_t queue_id,
		      const struct rte_event_queue_conf *queue_conf)
{
	struct rte_eventdev *dev;
	struct rte_event_queue_conf def_conf;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];

	if (!is_valid_queue(dev, queue_id)) {
		RTE_EDEV_LOG_ERR("Invalid queue_id=%" PRIu8, queue_id);
		return -EINVAL;
	}

	/* Check nb_atomic_flows limit */
	if (is_valid_atomic_queue_conf(queue_conf)) {
		if (queue_conf->nb_atomic_flows == 0 ||
		    queue_conf->nb_atomic_flows >
			dev->data->dev_conf.nb_event_queue_flows) {
			RTE_EDEV_LOG_ERR(
		"dev%d queue%d Invalid nb_atomic_flows=%d max_flows=%d",
			dev_id, queue_id, queue_conf->nb_atomic_flows,
			dev->data->dev_conf.nb_event_queue_flows);
			return -EINVAL;
		}
	}

	/* Check nb_atomic_order_sequences limit */
	if (is_valid_ordered_queue_conf(queue_conf)) {
		if (queue_conf->nb_atomic_order_sequences == 0 ||
		    queue_conf->nb_atomic_order_sequences >
			dev->data->dev_conf.nb_event_queue_flows) {
			RTE_EDEV_LOG_ERR(
		"dev%d queue%d Invalid nb_atomic_order_seq=%d max_flows=%d",
			dev_id, queue_id, queue_conf->nb_atomic_order_sequences,
			dev->data->dev_conf.nb_event_queue_flows);
			return -EINVAL;
		}
	}

	if (dev->data->dev_started) {
		RTE_EDEV_LOG_ERR(
		    "device %d must be stopped to allow queue setup", dev_id);
		return -EBUSY;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->queue_setup, -ENOTSUP);

	if (queue_conf == NULL) {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->queue_def_conf,
					-ENOTSUP);
		(*dev->dev_ops->queue_def_conf)(dev, queue_id, &def_conf);
		queue_conf = &def_conf;
	}

	dev->data->queues_cfg[queue_id] = *queue_conf;
	return (*dev->dev_ops->queue_setup)(dev, queue_id, queue_conf);
}

static inline int
is_valid_port(struct rte_eventdev *dev, uint8_t port_id)
{
	if (port_id < dev->data->nb_ports)
		return 1;
	else
		return 0;
}

int
rte_event_port_default_conf_get(uint8_t dev_id, uint8_t port_id,
				 struct rte_event_port_conf *port_conf)
{
	struct rte_eventdev *dev;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];

	if (port_conf == NULL)
		return -EINVAL;

	if (!is_valid_port(dev, port_id)) {
		RTE_EDEV_LOG_ERR("Invalid port_id=%" PRIu8, port_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->port_def_conf, -ENOTSUP);
	memset(port_conf, 0, sizeof(struct rte_event_port_conf));
	(*dev->dev_ops->port_def_conf)(dev, port_id, port_conf);
	return 0;
}

int
rte_event_port_setup(uint8_t dev_id, uint8_t port_id,
		     const struct rte_event_port_conf *port_conf)
{
	struct rte_eventdev *dev;
	struct rte_event_port_conf def_conf;
	int diag;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];

	if (!is_valid_port(dev, port_id)) {
		RTE_EDEV_LOG_ERR("Invalid port_id=%" PRIu8, port_id);
		return -EINVAL;
	}

	/* Check new_event_threshold limit */
	if ((port_conf && !port_conf->new_event_threshold) ||
			(port_conf && port_conf->new_event_threshold >
				 dev->data->dev_conf.nb_events_limit)) {
		RTE_EDEV_LOG_ERR(
		   "dev%d port%d Invalid event_threshold=%d nb_events_limit=%d",
			dev_id, port_id, port_conf->new_event_threshold,
			dev->data->dev_conf.nb_events_limit);
		return -EINVAL;
	}

	/* Check dequeue_depth limit */
	if ((port_conf && !port_conf->dequeue_depth) ||
			(port_conf && port_conf->dequeue_depth >
		dev->data->dev_conf.nb_event_port_dequeue_depth)) {
		RTE_EDEV_LOG_ERR(
		   "dev%d port%d Invalid dequeue depth=%d max_dequeue_depth=%d",
			dev_id, port_id, port_conf->dequeue_depth,
			dev->data->dev_conf.nb_event_port_dequeue_depth);
		return -EINVAL;
	}

	/* Check enqueue_depth limit */
	if ((port_conf && !port_conf->enqueue_depth) ||
			(port_conf && port_conf->enqueue_depth >
		dev->data->dev_conf.nb_event_port_enqueue_depth)) {
		RTE_EDEV_LOG_ERR(
		   "dev%d port%d Invalid enqueue depth=%d max_enqueue_depth=%d",
			dev_id, port_id, port_conf->enqueue_depth,
			dev->data->dev_conf.nb_event_port_enqueue_depth);
		return -EINVAL;
	}

	if (port_conf && port_conf->disable_implicit_release &&
	    !(dev->data->event_dev_cap &
	      RTE_EVENT_DEV_CAP_IMPLICIT_RELEASE_DISABLE)) {
		RTE_EDEV_LOG_ERR(
		   "dev%d port%d Implicit release disable not supported",
			dev_id, port_id);
		return -EINVAL;
	}

	if (dev->data->dev_started) {
		RTE_EDEV_LOG_ERR(
		    "device %d must be stopped to allow port setup", dev_id);
		return -EBUSY;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->port_setup, -ENOTSUP);

	if (port_conf == NULL) {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->port_def_conf,
					-ENOTSUP);
		(*dev->dev_ops->port_def_conf)(dev, port_id, &def_conf);
		port_conf = &def_conf;
	}

	dev->data->ports_cfg[port_id] = *port_conf;

	diag = (*dev->dev_ops->port_setup)(dev, port_id, port_conf);

	/* Unlink all the queues from this port(default state after setup) */
	if (!diag)
		diag = rte_event_port_unlink(dev_id, port_id, NULL, 0);

	if (diag < 0)
		return diag;

	return 0;
}

int
rte_event_dev_attr_get(uint8_t dev_id, uint32_t attr_id,
		       uint32_t *attr_value)
{
	struct rte_eventdev *dev;

	if (!attr_value)
		return -EINVAL;
	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];

	switch (attr_id) {
	case RTE_EVENT_DEV_ATTR_PORT_COUNT:
		*attr_value = dev->data->nb_ports;
		break;
	case RTE_EVENT_DEV_ATTR_QUEUE_COUNT:
		*attr_value = dev->data->nb_queues;
		break;
	case RTE_EVENT_DEV_ATTR_STARTED:
		*attr_value = dev->data->dev_started;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int
rte_event_port_attr_get(uint8_t dev_id, uint8_t port_id, uint32_t attr_id,
			uint32_t *attr_value)
{
	struct rte_eventdev *dev;

	if (!attr_value)
		return -EINVAL;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];
	if (!is_valid_port(dev, port_id)) {
		RTE_EDEV_LOG_ERR("Invalid port_id=%" PRIu8, port_id);
		return -EINVAL;
	}

	switch (attr_id) {
	case RTE_EVENT_PORT_ATTR_ENQ_DEPTH:
		*attr_value = dev->data->ports_cfg[port_id].enqueue_depth;
		break;
	case RTE_EVENT_PORT_ATTR_DEQ_DEPTH:
		*attr_value = dev->data->ports_cfg[port_id].dequeue_depth;
		break;
	case RTE_EVENT_PORT_ATTR_NEW_EVENT_THRESHOLD:
		*attr_value = dev->data->ports_cfg[port_id].new_event_threshold;
		break;
	default:
		return -EINVAL;
	};
	return 0;
}

int
rte_event_queue_attr_get(uint8_t dev_id, uint8_t queue_id, uint32_t attr_id,
			uint32_t *attr_value)
{
	struct rte_event_queue_conf *conf;
	struct rte_eventdev *dev;

	if (!attr_value)
		return -EINVAL;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];
	if (!is_valid_queue(dev, queue_id)) {
		RTE_EDEV_LOG_ERR("Invalid queue_id=%" PRIu8, queue_id);
		return -EINVAL;
	}

	conf = &dev->data->queues_cfg[queue_id];

	switch (attr_id) {
	case RTE_EVENT_QUEUE_ATTR_PRIORITY:
		*attr_value = RTE_EVENT_DEV_PRIORITY_NORMAL;
		if (dev->data->event_dev_cap & RTE_EVENT_DEV_CAP_QUEUE_QOS)
			*attr_value = conf->priority;
		break;
	case RTE_EVENT_QUEUE_ATTR_NB_ATOMIC_FLOWS:
		*attr_value = conf->nb_atomic_flows;
		break;
	case RTE_EVENT_QUEUE_ATTR_NB_ATOMIC_ORDER_SEQUENCES:
		*attr_value = conf->nb_atomic_order_sequences;
		break;
	case RTE_EVENT_QUEUE_ATTR_EVENT_QUEUE_CFG:
		*attr_value = conf->event_queue_cfg;
		break;
	case RTE_EVENT_QUEUE_ATTR_SCHEDULE_TYPE:
		if (conf->event_queue_cfg & RTE_EVENT_QUEUE_CFG_ALL_TYPES)
			return -EOVERFLOW;

		*attr_value = conf->schedule_type;
		break;
	default:
		return -EINVAL;
	};
	return 0;
}

int
rte_event_port_link(uint8_t dev_id, uint8_t port_id,
		    const uint8_t queues[], const uint8_t priorities[],
		    uint16_t nb_links)
{
	struct rte_eventdev *dev;
	uint8_t queues_list[RTE_EVENT_MAX_QUEUES_PER_DEV];
	uint8_t priorities_list[RTE_EVENT_MAX_QUEUES_PER_DEV];
	uint16_t *links_map;
	int i, diag;

	RTE_EVENTDEV_VALID_DEVID_OR_ERRNO_RET(dev_id, -EINVAL, 0);
	dev = &rte_eventdevs[dev_id];

	if (*dev->dev_ops->port_link == NULL) {
		RTE_EDEV_LOG_ERR("Function not supported\n");
		rte_errno = -ENOTSUP;
		return 0;
	}

	if (!is_valid_port(dev, port_id)) {
		RTE_EDEV_LOG_ERR("Invalid port_id=%" PRIu8, port_id);
		rte_errno = -EINVAL;
		return 0;
	}

	if (queues == NULL) {
		for (i = 0; i < dev->data->nb_queues; i++)
			queues_list[i] = i;

		queues = queues_list;
		nb_links = dev->data->nb_queues;
	}

	if (priorities == NULL) {
		for (i = 0; i < nb_links; i++)
			priorities_list[i] = RTE_EVENT_DEV_PRIORITY_NORMAL;

		priorities = priorities_list;
	}

	for (i = 0; i < nb_links; i++)
		if (queues[i] >= dev->data->nb_queues) {
			rte_errno = -EINVAL;
			return 0;
		}

	diag = (*dev->dev_ops->port_link)(dev, dev->data->ports[port_id],
						queues, priorities, nb_links);
	if (diag < 0)
		return diag;

	links_map = dev->data->links_map;
	/* Point links_map to this port specific area */
	links_map += (port_id * RTE_EVENT_MAX_QUEUES_PER_DEV);
	for (i = 0; i < diag; i++)
		links_map[queues[i]] = (uint8_t)priorities[i];

	return diag;
}

int
rte_event_port_unlink(uint8_t dev_id, uint8_t port_id,
		      uint8_t queues[], uint16_t nb_unlinks)
{
	struct rte_eventdev *dev;
	uint8_t all_queues[RTE_EVENT_MAX_QUEUES_PER_DEV];
	int i, diag, j;
	uint16_t *links_map;

	RTE_EVENTDEV_VALID_DEVID_OR_ERRNO_RET(dev_id, -EINVAL, 0);
	dev = &rte_eventdevs[dev_id];

	if (*dev->dev_ops->port_unlink == NULL) {
		RTE_EDEV_LOG_ERR("Function not supported");
		rte_errno = -ENOTSUP;
		return 0;
	}

	if (!is_valid_port(dev, port_id)) {
		RTE_EDEV_LOG_ERR("Invalid port_id=%" PRIu8, port_id);
		rte_errno = -EINVAL;
		return 0;
	}

	links_map = dev->data->links_map;
	/* Point links_map to this port specific area */
	links_map += (port_id * RTE_EVENT_MAX_QUEUES_PER_DEV);

	if (queues == NULL) {
		j = 0;
		for (i = 0; i < dev->data->nb_queues; i++) {
			if (links_map[i] !=
					EVENT_QUEUE_SERVICE_PRIORITY_INVALID) {
				all_queues[j] = i;
				j++;
			}
		}
		queues = all_queues;
	} else {
		for (j = 0; j < nb_unlinks; j++) {
			if (links_map[queues[j]] ==
					EVENT_QUEUE_SERVICE_PRIORITY_INVALID)
				break;
		}
	}

	nb_unlinks = j;
	for (i = 0; i < nb_unlinks; i++)
		if (queues[i] >= dev->data->nb_queues) {
			rte_errno = -EINVAL;
			return 0;
		}

	diag = (*dev->dev_ops->port_unlink)(dev, dev->data->ports[port_id],
					queues, nb_unlinks);

	if (diag < 0)
		return diag;

	for (i = 0; i < diag; i++)
		links_map[queues[i]] = EVENT_QUEUE_SERVICE_PRIORITY_INVALID;

	return diag;
}

int
rte_event_port_unlinks_in_progress(uint8_t dev_id, uint8_t port_id)
{
	struct rte_eventdev *dev;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];
	if (!is_valid_port(dev, port_id)) {
		RTE_EDEV_LOG_ERR("Invalid port_id=%" PRIu8, port_id);
		return -EINVAL;
	}

	/* Return 0 if the PMD does not implement unlinks in progress.
	 * This allows PMDs which handle unlink synchronously to not implement
	 * this function at all.
	 */
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->port_unlinks_in_progress, 0);

	return (*dev->dev_ops->port_unlinks_in_progress)(dev,
			dev->data->ports[port_id]);
}

int
rte_event_port_links_get(uint8_t dev_id, uint8_t port_id,
			 uint8_t queues[], uint8_t priorities[])
{
	struct rte_eventdev *dev;
	uint16_t *links_map;
	int i, count = 0;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];
	if (!is_valid_port(dev, port_id)) {
		RTE_EDEV_LOG_ERR("Invalid port_id=%" PRIu8, port_id);
		return -EINVAL;
	}

	links_map = dev->data->links_map;
	/* Point links_map to this port specific area */
	links_map += (port_id * RTE_EVENT_MAX_QUEUES_PER_DEV);
	for (i = 0; i < dev->data->nb_queues; i++) {
		if (links_map[i] != EVENT_QUEUE_SERVICE_PRIORITY_INVALID) {
			queues[count] = i;
			priorities[count] = (uint8_t)links_map[i];
			++count;
		}
	}
	return count;
}

int
rte_event_dequeue_timeout_ticks(uint8_t dev_id, uint64_t ns,
				 uint64_t *timeout_ticks)
{
	struct rte_eventdev *dev;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->timeout_ticks, -ENOTSUP);

	if (timeout_ticks == NULL)
		return -EINVAL;

	return (*dev->dev_ops->timeout_ticks)(dev, ns, timeout_ticks);
}

int
rte_event_dev_service_id_get(uint8_t dev_id, uint32_t *service_id)
{
	struct rte_eventdev *dev;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];

	if (service_id == NULL)
		return -EINVAL;

	if (dev->data->service_inited)
		*service_id = dev->data->service_id;

	return dev->data->service_inited ? 0 : -ESRCH;
}

int
rte_event_dev_dump(uint8_t dev_id, FILE *f)
{
	struct rte_eventdev *dev;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dump, -ENOTSUP);

	(*dev->dev_ops->dump)(dev, f);
	return 0;

}

static int
xstats_get_count(uint8_t dev_id, enum rte_event_dev_xstats_mode mode,
		uint8_t queue_port_id)
{
	struct rte_eventdev *dev = &rte_eventdevs[dev_id];
	if (dev->dev_ops->xstats_get_names != NULL)
		return (*dev->dev_ops->xstats_get_names)(dev, mode,
							queue_port_id,
							NULL, NULL, 0);
	return 0;
}

int
rte_event_dev_xstats_names_get(uint8_t dev_id,
		enum rte_event_dev_xstats_mode mode, uint8_t queue_port_id,
		struct rte_event_dev_xstats_name *xstats_names,
		unsigned int *ids, unsigned int size)
{
	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -ENODEV);
	const int cnt_expected_entries = xstats_get_count(dev_id, mode,
							  queue_port_id);
	if (xstats_names == NULL || cnt_expected_entries < 0 ||
			(int)size < cnt_expected_entries)
		return cnt_expected_entries;

	/* dev_id checked above */
	const struct rte_eventdev *dev = &rte_eventdevs[dev_id];

	if (dev->dev_ops->xstats_get_names != NULL)
		return (*dev->dev_ops->xstats_get_names)(dev, mode,
				queue_port_id, xstats_names, ids, size);

	return -ENOTSUP;
}

/* retrieve eventdev extended statistics */
int
rte_event_dev_xstats_get(uint8_t dev_id, enum rte_event_dev_xstats_mode mode,
		uint8_t queue_port_id, const unsigned int ids[],
		uint64_t values[], unsigned int n)
{
	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -ENODEV);
	const struct rte_eventdev *dev = &rte_eventdevs[dev_id];

	/* implemented by the driver */
	if (dev->dev_ops->xstats_get != NULL)
		return (*dev->dev_ops->xstats_get)(dev, mode, queue_port_id,
				ids, values, n);
	return -ENOTSUP;
}

uint64_t
rte_event_dev_xstats_by_name_get(uint8_t dev_id, const char *name,
		unsigned int *id)
{
	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, 0);
	const struct rte_eventdev *dev = &rte_eventdevs[dev_id];
	unsigned int temp = -1;

	if (id != NULL)
		*id = (unsigned int)-1;
	else
		id = &temp; /* ensure driver never gets a NULL value */

	/* implemented by driver */
	if (dev->dev_ops->xstats_get_by_name != NULL)
		return (*dev->dev_ops->xstats_get_by_name)(dev, name, id);
	return -ENOTSUP;
}

int rte_event_dev_xstats_reset(uint8_t dev_id,
		enum rte_event_dev_xstats_mode mode, int16_t queue_port_id,
		const uint32_t ids[], uint32_t nb_ids)
{
	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	struct rte_eventdev *dev = &rte_eventdevs[dev_id];

	if (dev->dev_ops->xstats_reset != NULL)
		return (*dev->dev_ops->xstats_reset)(dev, mode, queue_port_id,
							ids, nb_ids);
	return -ENOTSUP;
}

int rte_event_dev_selftest(uint8_t dev_id)
{
	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	struct rte_eventdev *dev = &rte_eventdevs[dev_id];

	if (dev->dev_ops->dev_selftest != NULL)
		return (*dev->dev_ops->dev_selftest)();
	return -ENOTSUP;
}

int
rte_event_dev_start(uint8_t dev_id)
{
	struct rte_eventdev *dev;
	int diag;

	RTE_EDEV_LOG_DEBUG("Start dev_id=%" PRIu8, dev_id);

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_start, -ENOTSUP);

	if (dev->data->dev_started != 0) {
		RTE_EDEV_LOG_ERR("Device with dev_id=%" PRIu8 "already started",
			dev_id);
		return 0;
	}

	diag = (*dev->dev_ops->dev_start)(dev);
	if (diag == 0)
		dev->data->dev_started = 1;
	else
		return diag;

	return 0;
}

int
rte_event_dev_stop_flush_callback_register(uint8_t dev_id,
		eventdev_stop_flush_t callback, void *userdata)
{
	struct rte_eventdev *dev;

	RTE_EDEV_LOG_DEBUG("Stop flush register dev_id=%" PRIu8, dev_id);

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];

	dev->dev_ops->dev_stop_flush = callback;
	dev->data->dev_stop_flush_arg = userdata;

	return 0;
}

void
rte_event_dev_stop(uint8_t dev_id)
{
	struct rte_eventdev *dev;

	RTE_EDEV_LOG_DEBUG("Stop dev_id=%" PRIu8, dev_id);

	RTE_EVENTDEV_VALID_DEVID_OR_RET(dev_id);
	dev = &rte_eventdevs[dev_id];
	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->dev_stop);

	if (dev->data->dev_started == 0) {
		RTE_EDEV_LOG_ERR("Device with dev_id=%" PRIu8 "already stopped",
			dev_id);
		return;
	}

	dev->data->dev_started = 0;
	(*dev->dev_ops->dev_stop)(dev);
}

int
rte_event_dev_close(uint8_t dev_id)
{
	struct rte_eventdev *dev;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_close, -ENOTSUP);

	/* Device must be stopped before it can be closed */
	if (dev->data->dev_started == 1) {
		RTE_EDEV_LOG_ERR("Device %u must be stopped before closing",
				dev_id);
		return -EBUSY;
	}

	return (*dev->dev_ops->dev_close)(dev);
}

static inline int
rte_eventdev_data_alloc(uint8_t dev_id, struct rte_eventdev_data **data,
		int socket_id)
{
	char mz_name[RTE_EVENTDEV_NAME_MAX_LEN];
	const struct rte_memzone *mz;
	int n;

	/* Generate memzone name */
	n = snprintf(mz_name, sizeof(mz_name), "rte_eventdev_data_%u", dev_id);
	if (n >= (int)sizeof(mz_name))
		return -EINVAL;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		mz = rte_memzone_reserve(mz_name,
				sizeof(struct rte_eventdev_data),
				socket_id, 0);
	} else
		mz = rte_memzone_lookup(mz_name);

	if (mz == NULL)
		return -ENOMEM;

	*data = mz->addr;
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		memset(*data, 0, sizeof(struct rte_eventdev_data));

	return 0;
}

static inline uint8_t
rte_eventdev_find_free_device_index(void)
{
	uint8_t dev_id;

	for (dev_id = 0; dev_id < RTE_EVENT_MAX_DEVS; dev_id++) {
		if (rte_eventdevs[dev_id].attached ==
				RTE_EVENTDEV_DETACHED)
			return dev_id;
	}
	return RTE_EVENT_MAX_DEVS;
}

static uint16_t
rte_event_tx_adapter_enqueue(__rte_unused void *port,
			__rte_unused struct rte_event ev[],
			__rte_unused uint16_t nb_events)
{
	rte_errno = ENOTSUP;
	return 0;
}

struct rte_eventdev *
rte_event_pmd_allocate(const char *name, int socket_id)
{
	struct rte_eventdev *eventdev;
	uint8_t dev_id;

	if (rte_event_pmd_get_named_dev(name) != NULL) {
		RTE_EDEV_LOG_ERR("Event device with name %s already "
				"allocated!", name);
		return NULL;
	}

	dev_id = rte_eventdev_find_free_device_index();
	if (dev_id == RTE_EVENT_MAX_DEVS) {
		RTE_EDEV_LOG_ERR("Reached maximum number of event devices");
		return NULL;
	}

	eventdev = &rte_eventdevs[dev_id];

	eventdev->txa_enqueue = rte_event_tx_adapter_enqueue;

	if (eventdev->data == NULL) {
		struct rte_eventdev_data *eventdev_data = NULL;

		int retval = rte_eventdev_data_alloc(dev_id, &eventdev_data,
				socket_id);

		if (retval < 0 || eventdev_data == NULL)
			return NULL;

		eventdev->data = eventdev_data;

		strlcpy(eventdev->data->name, name, RTE_EVENTDEV_NAME_MAX_LEN);

		eventdev->data->dev_id = dev_id;
		eventdev->data->socket_id = socket_id;
		eventdev->data->dev_started = 0;

		eventdev->attached = RTE_EVENTDEV_ATTACHED;

		eventdev_globals.nb_devs++;
	}

	return eventdev;
}

int
rte_event_pmd_release(struct rte_eventdev *eventdev)
{
	int ret;
	char mz_name[RTE_EVENTDEV_NAME_MAX_LEN];
	const struct rte_memzone *mz;

	if (eventdev == NULL)
		return -EINVAL;

	eventdev->attached = RTE_EVENTDEV_DETACHED;
	eventdev_globals.nb_devs--;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		rte_free(eventdev->data->dev_private);

		/* Generate memzone name */
		ret = snprintf(mz_name, sizeof(mz_name), "rte_eventdev_data_%u",
				eventdev->data->dev_id);
		if (ret >= (int)sizeof(mz_name))
			return -EINVAL;

		mz = rte_memzone_lookup(mz_name);
		if (mz == NULL)
			return -ENOMEM;

		ret = rte_memzone_free(mz);
		if (ret)
			return ret;
	}

	eventdev->data = NULL;
	return 0;
}
