/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>

#include <rte_string_fns.h>
#include <rte_log.h>
#include <dev_driver.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_errno.h>
#include <ethdev_driver.h>
#include <rte_cryptodev.h>
#include <rte_dmadev.h>
#include <cryptodev_pmd.h>
#include <rte_telemetry.h>

#include "rte_eventdev.h"
#include "eventdev_pmd.h"
#include "eventdev_trace.h"

static struct rte_eventdev rte_event_devices[RTE_EVENT_MAX_DEVS];

struct rte_eventdev *rte_eventdevs = rte_event_devices;

static struct rte_eventdev_global eventdev_globals = {
	.nb_devs		= 0
};

/* Public fastpath APIs. */
struct rte_event_fp_ops rte_event_fp_ops[RTE_EVENT_MAX_DEVS];

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
					RTE_EVENTDEV_ATTACHED)) {
			rte_eventdev_trace_get_dev_id(name, i);
			return i;
		}
	}
	return -ENODEV;
}

int
rte_event_dev_socket_id(uint8_t dev_id)
{
	struct rte_eventdev *dev;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];

	rte_eventdev_trace_socket_id(dev_id, dev, dev->data->socket_id);

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

	if (*dev->dev_ops->dev_infos_get == NULL)
		return -ENOTSUP;
	(*dev->dev_ops->dev_infos_get)(dev, dev_info);

	dev_info->dequeue_timeout_ns = dev->data->dev_conf.dequeue_timeout_ns;

	dev_info->dev = dev->dev;
	if (dev->dev != NULL && dev->dev->driver != NULL)
		dev_info->driver_name = dev->dev->driver->name;

	rte_eventdev_trace_info_get(dev_id, dev_info, dev_info->dev);

	return 0;
}

int
rte_event_eth_rx_adapter_caps_get(uint8_t dev_id, uint16_t eth_port_id,
				uint32_t *caps)
{
	struct rte_eventdev *dev;

	rte_eventdev_trace_eth_rx_adapter_caps_get(dev_id, eth_port_id);

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	RTE_ETH_VALID_PORTID_OR_ERR_RET(eth_port_id, -EINVAL);

	dev = &rte_eventdevs[dev_id];

	if (caps == NULL)
		return -EINVAL;

	if (dev->dev_ops->eth_rx_adapter_caps_get == NULL)
		*caps = RTE_EVENT_ETH_RX_ADAPTER_SW_CAP;
	else
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
	const struct event_timer_adapter_ops *ops;

	rte_eventdev_trace_timer_adapter_caps_get(dev_id);

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);

	dev = &rte_eventdevs[dev_id];

	if (caps == NULL)
		return -EINVAL;

	if (dev->dev_ops->timer_adapter_caps_get == NULL)
		*caps = RTE_EVENT_TIMER_ADAPTER_SW_CAP;
	else
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
	if (!rte_cryptodev_is_valid_dev(cdev_id))
		return -EINVAL;

	dev = &rte_eventdevs[dev_id];
	cdev = rte_cryptodev_pmd_get_dev(cdev_id);

	rte_eventdev_trace_crypto_adapter_caps_get(dev_id, dev, cdev_id, cdev);

	if (caps == NULL)
		return -EINVAL;

	if (dev->dev_ops->crypto_adapter_caps_get == NULL)
		*caps = RTE_EVENT_CRYPTO_ADAPTER_SW_CAP;
	else
		*caps = 0;

	return dev->dev_ops->crypto_adapter_caps_get ?
		(*dev->dev_ops->crypto_adapter_caps_get)
		(dev, cdev, caps) : 0;
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

	rte_eventdev_trace_eth_tx_adapter_caps_get(dev_id, dev, eth_port_id, eth_dev);

	if (caps == NULL)
		return -EINVAL;

	if (dev->dev_ops->eth_tx_adapter_caps_get == NULL)
		*caps = RTE_EVENT_ETH_TX_ADAPTER_CAP_EVENT_VECTOR;
	else
		*caps = 0;

	return dev->dev_ops->eth_tx_adapter_caps_get ?
			(*dev->dev_ops->eth_tx_adapter_caps_get)(dev,
								eth_dev,
								caps)
			: 0;
}

int
rte_event_dma_adapter_caps_get(uint8_t dev_id, uint8_t dma_dev_id, uint32_t *caps)
{
	struct rte_eventdev *dev;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	if (!rte_dma_is_valid(dma_dev_id))
		return -EINVAL;

	dev = &rte_eventdevs[dev_id];

	if (caps == NULL)
		return -EINVAL;

	*caps = 0;

	if (dev->dev_ops->dma_adapter_caps_get)
		return (*dev->dev_ops->dma_adapter_caps_get)(dev, dma_dev_id, caps);

	return 0;
}

static inline int
event_dev_queue_config(struct rte_eventdev *dev, uint8_t nb_queues)
{
	uint8_t old_nb_queues = dev->data->nb_queues;
	struct rte_event_queue_conf *queues_cfg;
	unsigned int i;

	RTE_EDEV_LOG_DEBUG("Setup %d queues on device %u", nb_queues,
			 dev->data->dev_id);

	if (nb_queues != 0) {
		queues_cfg = dev->data->queues_cfg;
		if (*dev->dev_ops->queue_release == NULL)
			return -ENOTSUP;

		for (i = nb_queues; i < old_nb_queues; i++)
			(*dev->dev_ops->queue_release)(dev, i);


		if (nb_queues > old_nb_queues) {
			uint8_t new_qs = nb_queues - old_nb_queues;

			memset(queues_cfg + old_nb_queues, 0,
				sizeof(queues_cfg[0]) * new_qs);
		}
	} else {
		if (*dev->dev_ops->queue_release == NULL)
			return -ENOTSUP;

		for (i = nb_queues; i < old_nb_queues; i++)
			(*dev->dev_ops->queue_release)(dev, i);
	}

	dev->data->nb_queues = nb_queues;
	return 0;
}

#define EVENT_QUEUE_SERVICE_PRIORITY_INVALID (0xdead)

static inline int
event_dev_port_config(struct rte_eventdev *dev, uint8_t nb_ports)
{
	uint8_t old_nb_ports = dev->data->nb_ports;
	void **ports;
	uint16_t *links_map;
	struct rte_event_port_conf *ports_cfg;
	unsigned int i, j;

	RTE_EDEV_LOG_DEBUG("Setup %d ports on device %u", nb_ports,
			 dev->data->dev_id);

	if (nb_ports != 0) { /* re-config */
		if (*dev->dev_ops->port_release == NULL)
			return -ENOTSUP;

		ports = dev->data->ports;
		ports_cfg = dev->data->ports_cfg;

		for (i = nb_ports; i < old_nb_ports; i++)
			(*dev->dev_ops->port_release)(ports[i]);

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
			for (i = 0; i < RTE_EVENT_MAX_PROFILES_PER_PORT; i++) {
				links_map = dev->data->links_map[i];
				for (j = old_links_map_end; j < links_map_end; j++)
					links_map[j] = EVENT_QUEUE_SERVICE_PRIORITY_INVALID;
			}
		}
	} else {
		if (*dev->dev_ops->port_release == NULL)
			return -ENOTSUP;

		ports = dev->data->ports;
		for (i = nb_ports; i < old_nb_ports; i++) {
			(*dev->dev_ops->port_release)(ports[i]);
			ports[i] = NULL;
		}
	}

	dev->data->nb_ports = nb_ports;
	return 0;
}

int
rte_event_dev_configure(uint8_t dev_id,
			const struct rte_event_dev_config *dev_conf)
{
	struct rte_event_dev_info info;
	struct rte_eventdev *dev;
	int diag;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];

	if (*dev->dev_ops->dev_infos_get == NULL)
		return -ENOTSUP;
	if (*dev->dev_ops->dev_configure == NULL)
		return -ENOTSUP;

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
	if (dev_conf->nb_event_queues > info.max_event_queues +
			info.max_single_link_event_port_queue_pairs) {
		RTE_EDEV_LOG_ERR("%d nb_event_queues=%d > max_event_queues=%d + max_single_link_event_port_queue_pairs=%d",
				 dev_id, dev_conf->nb_event_queues,
				 info.max_event_queues,
				 info.max_single_link_event_port_queue_pairs);
		return -EINVAL;
	}
	if (dev_conf->nb_event_queues -
			dev_conf->nb_single_link_event_port_queues >
			info.max_event_queues) {
		RTE_EDEV_LOG_ERR("id%d nb_event_queues=%d - nb_single_link_event_port_queues=%d > max_event_queues=%d",
				 dev_id, dev_conf->nb_event_queues,
				 dev_conf->nb_single_link_event_port_queues,
				 info.max_event_queues);
		return -EINVAL;
	}
	if (dev_conf->nb_single_link_event_port_queues >
			dev_conf->nb_event_queues) {
		RTE_EDEV_LOG_ERR("dev%d nb_single_link_event_port_queues=%d > nb_event_queues=%d",
				 dev_id,
				 dev_conf->nb_single_link_event_port_queues,
				 dev_conf->nb_event_queues);
		return -EINVAL;
	}

	/* Check nb_event_ports is in limit */
	if (!dev_conf->nb_event_ports) {
		RTE_EDEV_LOG_ERR("dev%d nb_event_ports cannot be zero", dev_id);
		return -EINVAL;
	}
	if (dev_conf->nb_event_ports > info.max_event_ports +
			info.max_single_link_event_port_queue_pairs) {
		RTE_EDEV_LOG_ERR("id%d nb_event_ports=%d > max_event_ports=%d + max_single_link_event_port_queue_pairs=%d",
				 dev_id, dev_conf->nb_event_ports,
				 info.max_event_ports,
				 info.max_single_link_event_port_queue_pairs);
		return -EINVAL;
	}
	if (dev_conf->nb_event_ports -
			dev_conf->nb_single_link_event_port_queues
			> info.max_event_ports) {
		RTE_EDEV_LOG_ERR("id%d nb_event_ports=%d - nb_single_link_event_port_queues=%d > max_event_ports=%d",
				 dev_id, dev_conf->nb_event_ports,
				 dev_conf->nb_single_link_event_port_queues,
				 info.max_event_ports);
		return -EINVAL;
	}

	if (dev_conf->nb_single_link_event_port_queues >
	    dev_conf->nb_event_ports) {
		RTE_EDEV_LOG_ERR(
				 "dev%d nb_single_link_event_port_queues=%d > nb_event_ports=%d",
				 dev_id,
				 dev_conf->nb_single_link_event_port_queues,
				 dev_conf->nb_event_ports);
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
	diag = event_dev_queue_config(dev, dev_conf->nb_event_queues);
	if (diag != 0) {
		RTE_EDEV_LOG_ERR("dev%d event_dev_queue_config = %d", dev_id,
				 diag);
		return diag;
	}

	/* Setup new number of ports and reconfigure device. */
	diag = event_dev_port_config(dev, dev_conf->nb_event_ports);
	if (diag != 0) {
		event_dev_queue_config(dev, 0);
		RTE_EDEV_LOG_ERR("dev%d event_dev_port_config = %d", dev_id,
				 diag);
		return diag;
	}

	event_dev_fp_ops_reset(rte_event_fp_ops + dev_id);

	/* Configure the device */
	diag = (*dev->dev_ops->dev_configure)(dev);
	if (diag != 0) {
		RTE_EDEV_LOG_ERR("dev%d dev_configure = %d", dev_id, diag);
		event_dev_fp_ops_reset(rte_event_fp_ops + dev_id);
		event_dev_queue_config(dev, 0);
		event_dev_port_config(dev, 0);
	}

	dev->data->event_dev_cap = info.event_dev_cap;
	rte_eventdev_trace_configure(dev_id, dev_conf, diag);
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

	if (*dev->dev_ops->queue_def_conf == NULL)
		return -ENOTSUP;
	memset(queue_conf, 0, sizeof(struct rte_event_queue_conf));
	(*dev->dev_ops->queue_def_conf)(dev, queue_id, queue_conf);

	rte_eventdev_trace_queue_default_conf_get(dev_id, dev, queue_id, queue_conf);

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

	if (*dev->dev_ops->queue_setup == NULL)
		return -ENOTSUP;

	if (queue_conf == NULL) {
		if (*dev->dev_ops->queue_def_conf == NULL)
			return -ENOTSUP;
		(*dev->dev_ops->queue_def_conf)(dev, queue_id, &def_conf);
		queue_conf = &def_conf;
	}

	dev->data->queues_cfg[queue_id] = *queue_conf;
	rte_eventdev_trace_queue_setup(dev_id, queue_id, queue_conf);
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

	if (*dev->dev_ops->port_def_conf == NULL)
		return -ENOTSUP;
	memset(port_conf, 0, sizeof(struct rte_event_port_conf));
	(*dev->dev_ops->port_def_conf)(dev, port_id, port_conf);

	rte_eventdev_trace_port_default_conf_get(dev_id, dev, port_id, port_conf);

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

	if (port_conf &&
	    (port_conf->event_port_cfg & RTE_EVENT_PORT_CFG_DISABLE_IMPL_REL) &&
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

	if (*dev->dev_ops->port_setup == NULL)
		return -ENOTSUP;

	if (port_conf == NULL) {
		if (*dev->dev_ops->port_def_conf == NULL)
			return -ENOTSUP;
		(*dev->dev_ops->port_def_conf)(dev, port_id, &def_conf);
		port_conf = &def_conf;
	}

	dev->data->ports_cfg[port_id] = *port_conf;

	diag = (*dev->dev_ops->port_setup)(dev, port_id, port_conf);

	/* Unlink all the queues from this port(default state after setup) */
	if (!diag)
		diag = rte_event_port_unlink(dev_id, port_id, NULL, 0);

	rte_eventdev_trace_port_setup(dev_id, port_id, port_conf, diag);
	if (diag < 0)
		return diag;

	return 0;
}

void
rte_event_port_quiesce(uint8_t dev_id, uint8_t port_id,
		       rte_eventdev_port_flush_t release_cb, void *args)
{
	struct rte_eventdev *dev;

	RTE_EVENTDEV_VALID_DEVID_OR_RET(dev_id);
	dev = &rte_eventdevs[dev_id];

	rte_eventdev_trace_port_quiesce(dev_id, dev, port_id, args);

	if (!is_valid_port(dev, port_id)) {
		RTE_EDEV_LOG_ERR("Invalid port_id=%" PRIu8, port_id);
		return;
	}

	if (dev->dev_ops->port_quiesce)
		(*dev->dev_ops->port_quiesce)(dev, dev->data->ports[port_id],
					      release_cb, args);
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

	rte_eventdev_trace_attr_get(dev_id, dev, attr_id, *attr_value);

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
	case RTE_EVENT_PORT_ATTR_IMPLICIT_RELEASE_DISABLE:
	{
		uint32_t config;

		config = dev->data->ports_cfg[port_id].event_port_cfg;
		*attr_value = !!(config & RTE_EVENT_PORT_CFG_DISABLE_IMPL_REL);
		break;
	}
	default:
		return -EINVAL;
	};

	rte_eventdev_trace_port_attr_get(dev_id, dev, port_id, attr_id, *attr_value);

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
	case RTE_EVENT_QUEUE_ATTR_WEIGHT:
		*attr_value = RTE_EVENT_QUEUE_WEIGHT_LOWEST;
		if (dev->data->event_dev_cap & RTE_EVENT_DEV_CAP_QUEUE_QOS)
			*attr_value = conf->weight;
		break;
	case RTE_EVENT_QUEUE_ATTR_AFFINITY:
		*attr_value = RTE_EVENT_QUEUE_AFFINITY_LOWEST;
		if (dev->data->event_dev_cap & RTE_EVENT_DEV_CAP_QUEUE_QOS)
			*attr_value = conf->affinity;
		break;
	default:
		return -EINVAL;
	};

	rte_eventdev_trace_queue_attr_get(dev_id, dev, queue_id, attr_id, *attr_value);

	return 0;
}

int
rte_event_queue_attr_set(uint8_t dev_id, uint8_t queue_id, uint32_t attr_id,
			 uint64_t attr_value)
{
	struct rte_eventdev *dev;

	rte_eventdev_trace_queue_attr_set(dev_id, queue_id, attr_id, attr_value);

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];
	if (!is_valid_queue(dev, queue_id)) {
		RTE_EDEV_LOG_ERR("Invalid queue_id=%" PRIu8, queue_id);
		return -EINVAL;
	}

	if (!(dev->data->event_dev_cap &
	      RTE_EVENT_DEV_CAP_RUNTIME_QUEUE_ATTR)) {
		RTE_EDEV_LOG_ERR(
			"Device %" PRIu8 "does not support changing queue attributes at runtime",
			dev_id);
		return -ENOTSUP;
	}

	if (*dev->dev_ops->queue_attr_set == NULL)
		return -ENOTSUP;
	return (*dev->dev_ops->queue_attr_set)(dev, queue_id, attr_id,
					       attr_value);
}

int
rte_event_port_link(uint8_t dev_id, uint8_t port_id,
		    const uint8_t queues[], const uint8_t priorities[],
		    uint16_t nb_links)
{
	return rte_event_port_profile_links_set(dev_id, port_id, queues, priorities, nb_links, 0);
}

int
rte_event_port_profile_links_set(uint8_t dev_id, uint8_t port_id, const uint8_t queues[],
				 const uint8_t priorities[], uint16_t nb_links, uint8_t profile_id)
{
	uint8_t priorities_list[RTE_EVENT_MAX_QUEUES_PER_DEV];
	uint8_t queues_list[RTE_EVENT_MAX_QUEUES_PER_DEV];
	struct rte_event_dev_info info;
	struct rte_eventdev *dev;
	uint16_t *links_map;
	int i, diag;

	RTE_EVENTDEV_VALID_DEVID_OR_ERRNO_RET(dev_id, EINVAL, 0);
	dev = &rte_eventdevs[dev_id];

	if (*dev->dev_ops->dev_infos_get == NULL)
		return -ENOTSUP;

	(*dev->dev_ops->dev_infos_get)(dev, &info);
	if (profile_id >= RTE_EVENT_MAX_PROFILES_PER_PORT ||
	    profile_id >= info.max_profiles_per_port) {
		RTE_EDEV_LOG_ERR("Invalid profile_id=%" PRIu8, profile_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->port_link == NULL) {
		RTE_EDEV_LOG_ERR("Function not supported");
		rte_errno = ENOTSUP;
		return 0;
	}

	if (profile_id && *dev->dev_ops->port_link_profile == NULL) {
		RTE_EDEV_LOG_ERR("Function not supported");
		rte_errno = ENOTSUP;
		return 0;
	}

	if (!is_valid_port(dev, port_id)) {
		RTE_EDEV_LOG_ERR("Invalid port_id=%" PRIu8, port_id);
		rte_errno = EINVAL;
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
			rte_errno = EINVAL;
			return 0;
		}

	if (profile_id)
		diag = (*dev->dev_ops->port_link_profile)(dev, dev->data->ports[port_id], queues,
							  priorities, nb_links, profile_id);
	else
		diag = (*dev->dev_ops->port_link)(dev, dev->data->ports[port_id], queues,
						  priorities, nb_links);
	if (diag < 0)
		return diag;

	links_map = dev->data->links_map[profile_id];
	/* Point links_map to this port specific area */
	links_map += (port_id * RTE_EVENT_MAX_QUEUES_PER_DEV);
	for (i = 0; i < diag; i++)
		links_map[queues[i]] = (uint8_t)priorities[i];

	rte_eventdev_trace_port_profile_links_set(dev_id, port_id, nb_links, profile_id, diag);
	return diag;
}

int
rte_event_port_unlink(uint8_t dev_id, uint8_t port_id,
		      uint8_t queues[], uint16_t nb_unlinks)
{
	return rte_event_port_profile_unlink(dev_id, port_id, queues, nb_unlinks, 0);
}

int
rte_event_port_profile_unlink(uint8_t dev_id, uint8_t port_id, uint8_t queues[],
			      uint16_t nb_unlinks, uint8_t profile_id)
{
	uint8_t all_queues[RTE_EVENT_MAX_QUEUES_PER_DEV];
	struct rte_event_dev_info info;
	struct rte_eventdev *dev;
	uint16_t *links_map;
	int i, diag, j;

	RTE_EVENTDEV_VALID_DEVID_OR_ERRNO_RET(dev_id, EINVAL, 0);
	dev = &rte_eventdevs[dev_id];

	if (*dev->dev_ops->dev_infos_get == NULL)
		return -ENOTSUP;

	(*dev->dev_ops->dev_infos_get)(dev, &info);
	if (profile_id >= RTE_EVENT_MAX_PROFILES_PER_PORT ||
	    profile_id >= info.max_profiles_per_port) {
		RTE_EDEV_LOG_ERR("Invalid profile_id=%" PRIu8, profile_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->port_unlink == NULL) {
		RTE_EDEV_LOG_ERR("Function not supported");
		rte_errno = ENOTSUP;
		return 0;
	}

	if (profile_id && *dev->dev_ops->port_unlink_profile == NULL) {
		RTE_EDEV_LOG_ERR("Function not supported");
		rte_errno = ENOTSUP;
		return 0;
	}

	if (!is_valid_port(dev, port_id)) {
		RTE_EDEV_LOG_ERR("Invalid port_id=%" PRIu8, port_id);
		rte_errno = EINVAL;
		return 0;
	}

	links_map = dev->data->links_map[profile_id];
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
			rte_errno = EINVAL;
			return 0;
		}

	if (profile_id)
		diag = (*dev->dev_ops->port_unlink_profile)(dev, dev->data->ports[port_id], queues,
							    nb_unlinks, profile_id);
	else
		diag = (*dev->dev_ops->port_unlink)(dev, dev->data->ports[port_id], queues,
						    nb_unlinks);
	if (diag < 0)
		return diag;

	for (i = 0; i < diag; i++)
		links_map[queues[i]] = EVENT_QUEUE_SERVICE_PRIORITY_INVALID;

	rte_eventdev_trace_port_profile_unlink(dev_id, port_id, nb_unlinks, profile_id, diag);
	return diag;
}

int
rte_event_port_unlinks_in_progress(uint8_t dev_id, uint8_t port_id)
{
	struct rte_eventdev *dev;

	rte_eventdev_trace_port_unlinks_in_progress(dev_id, port_id);

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
	if (*dev->dev_ops->port_unlinks_in_progress == NULL)
		return 0;

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

	/* Use the default profile_id. */
	links_map = dev->data->links_map[0];
	/* Point links_map to this port specific area */
	links_map += (port_id * RTE_EVENT_MAX_QUEUES_PER_DEV);
	for (i = 0; i < dev->data->nb_queues; i++) {
		if (links_map[i] != EVENT_QUEUE_SERVICE_PRIORITY_INVALID) {
			queues[count] = i;
			priorities[count] = (uint8_t)links_map[i];
			++count;
		}
	}

	rte_eventdev_trace_port_links_get(dev_id, port_id, count);

	return count;
}

int
rte_event_port_profile_links_get(uint8_t dev_id, uint8_t port_id, uint8_t queues[],
				 uint8_t priorities[], uint8_t profile_id)
{
	struct rte_event_dev_info info;
	struct rte_eventdev *dev;
	uint16_t *links_map;
	int i, count = 0;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);

	dev = &rte_eventdevs[dev_id];
	if (*dev->dev_ops->dev_infos_get == NULL)
		return -ENOTSUP;

	(*dev->dev_ops->dev_infos_get)(dev, &info);
	if (profile_id >= RTE_EVENT_MAX_PROFILES_PER_PORT ||
	    profile_id >= info.max_profiles_per_port) {
		RTE_EDEV_LOG_ERR("Invalid profile_id=%" PRIu8, profile_id);
		return -EINVAL;
	}

	if (!is_valid_port(dev, port_id)) {
		RTE_EDEV_LOG_ERR("Invalid port_id=%" PRIu8, port_id);
		return -EINVAL;
	}

	links_map = dev->data->links_map[profile_id];
	/* Point links_map to this port specific area */
	links_map += (port_id * RTE_EVENT_MAX_QUEUES_PER_DEV);
	for (i = 0; i < dev->data->nb_queues; i++) {
		if (links_map[i] != EVENT_QUEUE_SERVICE_PRIORITY_INVALID) {
			queues[count] = i;
			priorities[count] = (uint8_t)links_map[i];
			++count;
		}
	}

	rte_eventdev_trace_port_profile_links_get(dev_id, port_id, profile_id, count);

	return count;
}

int
rte_event_dequeue_timeout_ticks(uint8_t dev_id, uint64_t ns,
				 uint64_t *timeout_ticks)
{
	struct rte_eventdev *dev;

	rte_eventdev_trace_dequeue_timeout_ticks(dev_id, ns, timeout_ticks);

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];
	if (*dev->dev_ops->timeout_ticks == NULL)
		return -ENOTSUP;

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

	rte_eventdev_trace_service_id_get(dev_id, *service_id);

	return dev->data->service_inited ? 0 : -ESRCH;
}

int
rte_event_dev_dump(uint8_t dev_id, FILE *f)
{
	struct rte_eventdev *dev;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];
	if (*dev->dev_ops->dump == NULL)
		return -ENOTSUP;
	if (f == NULL)
		return -EINVAL;

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
		uint64_t *ids, unsigned int size)
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
		uint8_t queue_port_id, const uint64_t ids[],
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
		uint64_t *id)
{
	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, 0);
	const struct rte_eventdev *dev = &rte_eventdevs[dev_id];
	uint64_t temp = -1;

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
		const uint64_t ids[], uint32_t nb_ids)
{
	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	struct rte_eventdev *dev = &rte_eventdevs[dev_id];

	if (dev->dev_ops->xstats_reset != NULL)
		return (*dev->dev_ops->xstats_reset)(dev, mode, queue_port_id,
							ids, nb_ids);
	return -ENOTSUP;
}

int rte_event_pmd_selftest_seqn_dynfield_offset = -1;

int rte_event_dev_selftest(uint8_t dev_id)
{
	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	static const struct rte_mbuf_dynfield test_seqn_dynfield_desc = {
		.name = "rte_event_pmd_selftest_seqn_dynfield",
		.size = sizeof(rte_event_pmd_selftest_seqn_t),
		.align = __alignof__(rte_event_pmd_selftest_seqn_t),
	};
	struct rte_eventdev *dev = &rte_eventdevs[dev_id];

	if (dev->dev_ops->dev_selftest != NULL) {
		rte_event_pmd_selftest_seqn_dynfield_offset =
			rte_mbuf_dynfield_register(&test_seqn_dynfield_desc);
		if (rte_event_pmd_selftest_seqn_dynfield_offset < 0)
			return -ENOMEM;
		return (*dev->dev_ops->dev_selftest)();
	}
	return -ENOTSUP;
}

struct rte_mempool *
rte_event_vector_pool_create(const char *name, unsigned int n,
			     unsigned int cache_size, uint16_t nb_elem,
			     int socket_id)
{
	const char *mp_ops_name;
	struct rte_mempool *mp;
	unsigned int elt_sz;
	int ret;

	if (!nb_elem) {
		RTE_EDEV_LOG_ERR("Invalid number of elements=%d requested",
			nb_elem);
		rte_errno = EINVAL;
		return NULL;
	}

	elt_sz =
		sizeof(struct rte_event_vector) + (nb_elem * sizeof(uintptr_t));
	mp = rte_mempool_create_empty(name, n, elt_sz, cache_size, 0, socket_id,
				      0);
	if (mp == NULL)
		return NULL;

	mp_ops_name = rte_mbuf_best_mempool_ops();
	ret = rte_mempool_set_ops_byname(mp, mp_ops_name, NULL);
	if (ret != 0) {
		RTE_EDEV_LOG_ERR("error setting mempool handler");
		goto err;
	}

	ret = rte_mempool_populate_default(mp);
	if (ret < 0)
		goto err;

	rte_eventdev_trace_vector_pool_create(mp, mp->name, mp->socket_id,
		mp->size, mp->cache_size, mp->elt_size);

	return mp;
err:
	rte_mempool_free(mp);
	rte_errno = -ret;
	return NULL;
}

int
rte_event_dev_start(uint8_t dev_id)
{
	struct rte_eventdev *dev;
	int diag;

	RTE_EDEV_LOG_DEBUG("Start dev_id=%" PRIu8, dev_id);

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];
	if (*dev->dev_ops->dev_start == NULL)
		return -ENOTSUP;

	if (dev->data->dev_started != 0) {
		RTE_EDEV_LOG_ERR("Device with dev_id=%" PRIu8 "already started",
			dev_id);
		return 0;
	}

	diag = (*dev->dev_ops->dev_start)(dev);
	rte_eventdev_trace_start(dev_id, diag);
	if (diag == 0)
		dev->data->dev_started = 1;
	else
		return diag;

	event_dev_fp_ops_set(rte_event_fp_ops + dev_id, dev);

	return 0;
}

int
rte_event_dev_stop_flush_callback_register(uint8_t dev_id,
					   rte_eventdev_stop_flush_t callback,
					   void *userdata)
{
	struct rte_eventdev *dev;

	RTE_EDEV_LOG_DEBUG("Stop flush register dev_id=%" PRIu8, dev_id);

	rte_eventdev_trace_stop_flush_callback_register(dev_id, callback, userdata);

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
	if (*dev->dev_ops->dev_stop == NULL)
		return;

	if (dev->data->dev_started == 0) {
		RTE_EDEV_LOG_ERR("Device with dev_id=%" PRIu8 "already stopped",
			dev_id);
		return;
	}

	dev->data->dev_started = 0;
	(*dev->dev_ops->dev_stop)(dev);
	rte_eventdev_trace_stop(dev_id);
	event_dev_fp_ops_reset(rte_event_fp_ops + dev_id);
}

int
rte_event_dev_close(uint8_t dev_id)
{
	struct rte_eventdev *dev;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];
	if (*dev->dev_ops->dev_close == NULL)
		return -ENOTSUP;

	/* Device must be stopped before it can be closed */
	if (dev->data->dev_started == 1) {
		RTE_EDEV_LOG_ERR("Device %u must be stopped before closing",
				dev_id);
		return -EBUSY;
	}

	event_dev_fp_ops_reset(rte_event_fp_ops + dev_id);
	rte_eventdev_trace_close(dev_id);
	return (*dev->dev_ops->dev_close)(dev);
}

static inline int
eventdev_data_alloc(uint8_t dev_id, struct rte_eventdev_data **data,
		    int socket_id)
{
	char mz_name[RTE_EVENTDEV_NAME_MAX_LEN];
	const struct rte_memzone *mz;
	int i, n;

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
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		memset(*data, 0, sizeof(struct rte_eventdev_data));
		for (i = 0; i < RTE_EVENT_MAX_PROFILES_PER_PORT; i++)
			for (n = 0; n < RTE_EVENT_MAX_PORTS_PER_DEV * RTE_EVENT_MAX_QUEUES_PER_DEV;
			     n++)
				(*data)->links_map[i][n] = EVENT_QUEUE_SERVICE_PRIORITY_INVALID;
	}

	return 0;
}

static inline uint8_t
eventdev_find_free_device_index(void)
{
	uint8_t dev_id;

	for (dev_id = 0; dev_id < RTE_EVENT_MAX_DEVS; dev_id++) {
		if (rte_eventdevs[dev_id].attached ==
				RTE_EVENTDEV_DETACHED)
			return dev_id;
	}
	return RTE_EVENT_MAX_DEVS;
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

	dev_id = eventdev_find_free_device_index();
	if (dev_id == RTE_EVENT_MAX_DEVS) {
		RTE_EDEV_LOG_ERR("Reached maximum number of event devices");
		return NULL;
	}

	eventdev = &rte_eventdevs[dev_id];

	if (eventdev->data == NULL) {
		struct rte_eventdev_data *eventdev_data = NULL;

		int retval =
			eventdev_data_alloc(dev_id, &eventdev_data, socket_id);

		if (retval < 0 || eventdev_data == NULL)
			return NULL;

		eventdev->data = eventdev_data;

		if (rte_eal_process_type() == RTE_PROC_PRIMARY) {

			strlcpy(eventdev->data->name, name,
				RTE_EVENTDEV_NAME_MAX_LEN);

			eventdev->data->dev_id = dev_id;
			eventdev->data->socket_id = socket_id;
			eventdev->data->dev_started = 0;
		}

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

	event_dev_fp_ops_reset(rte_event_fp_ops + eventdev->data->dev_id);
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

void
event_dev_probing_finish(struct rte_eventdev *eventdev)
{
	if (eventdev == NULL)
		return;

	event_dev_fp_ops_set(rte_event_fp_ops + eventdev->data->dev_id,
			     eventdev);
}

static int
handle_dev_list(const char *cmd __rte_unused,
		const char *params __rte_unused,
		struct rte_tel_data *d)
{
	uint8_t dev_id;
	int ndev = rte_event_dev_count();

	if (ndev < 1)
		return -1;

	rte_tel_data_start_array(d, RTE_TEL_INT_VAL);
	for (dev_id = 0; dev_id < RTE_EVENT_MAX_DEVS; dev_id++) {
		if (rte_eventdevs[dev_id].attached ==
				RTE_EVENTDEV_ATTACHED)
			rte_tel_data_add_array_int(d, dev_id);
	}

	return 0;
}

static int
handle_port_list(const char *cmd __rte_unused,
		 const char *params,
		 struct rte_tel_data *d)
{
	int i;
	uint8_t dev_id;
	struct rte_eventdev *dev;
	char *end_param;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -1;

	dev_id = strtoul(params, &end_param, 10);
	if (*end_param != '\0')
		RTE_EDEV_LOG_DEBUG(
			"Extra parameters passed to eventdev telemetry command, ignoring");

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];

	rte_tel_data_start_array(d, RTE_TEL_INT_VAL);
	for (i = 0; i < dev->data->nb_ports; i++)
		rte_tel_data_add_array_int(d, i);

	return 0;
}

static int
handle_queue_list(const char *cmd __rte_unused,
		  const char *params,
		  struct rte_tel_data *d)
{
	int i;
	uint8_t dev_id;
	struct rte_eventdev *dev;
	char *end_param;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -1;

	dev_id = strtoul(params, &end_param, 10);
	if (*end_param != '\0')
		RTE_EDEV_LOG_DEBUG(
			"Extra parameters passed to eventdev telemetry command, ignoring");

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];

	rte_tel_data_start_array(d, RTE_TEL_INT_VAL);
	for (i = 0; i < dev->data->nb_queues; i++)
		rte_tel_data_add_array_int(d, i);

	return 0;
}

static int
handle_queue_links(const char *cmd __rte_unused,
		   const char *params,
		   struct rte_tel_data *d)
{
	int i, ret, port_id = 0;
	char *end_param;
	uint8_t dev_id;
	uint8_t queues[RTE_EVENT_MAX_QUEUES_PER_DEV];
	uint8_t priorities[RTE_EVENT_MAX_QUEUES_PER_DEV];
	const char *p_param;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -1;

	/* Get dev ID from parameter string */
	dev_id = strtoul(params, &end_param, 10);
	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);

	p_param = strtok(end_param, ",");
	if (p_param == NULL || strlen(p_param) == 0 || !isdigit(*p_param))
		return -1;

	port_id = strtoul(p_param, &end_param, 10);
	p_param = strtok(NULL, "\0");
	if (p_param != NULL)
		RTE_EDEV_LOG_DEBUG(
			"Extra parameters passed to eventdev telemetry command, ignoring");

	ret = rte_event_port_links_get(dev_id, port_id, queues, priorities);
	if (ret < 0)
		return -1;

	rte_tel_data_start_dict(d);
	for (i = 0; i < ret; i++) {
		char qid_name[32];

		snprintf(qid_name, 31, "qid_%u", queues[i]);
		rte_tel_data_add_dict_uint(d, qid_name, priorities[i]);
	}

	return 0;
}

static int
eventdev_build_telemetry_data(int dev_id,
			      enum rte_event_dev_xstats_mode mode,
			      int port_queue_id,
			      struct rte_tel_data *d)
{
	struct rte_event_dev_xstats_name *xstat_names;
	uint64_t *ids;
	uint64_t *values;
	int i, ret, num_xstats;

	num_xstats = rte_event_dev_xstats_names_get(dev_id,
						    mode,
						    port_queue_id,
						    NULL,
						    NULL,
						    0);

	if (num_xstats < 0)
		return -1;

	/* use one malloc for names */
	xstat_names = malloc((sizeof(struct rte_event_dev_xstats_name))
			     * num_xstats);
	if (xstat_names == NULL)
		return -1;

	ids = malloc((sizeof(uint64_t)) * num_xstats);
	if (ids == NULL) {
		free(xstat_names);
		return -1;
	}

	values = malloc((sizeof(uint64_t)) * num_xstats);
	if (values == NULL) {
		free(xstat_names);
		free(ids);
		return -1;
	}

	ret = rte_event_dev_xstats_names_get(dev_id, mode, port_queue_id,
					     xstat_names, ids, num_xstats);
	if (ret < 0 || ret > num_xstats) {
		free(xstat_names);
		free(ids);
		free(values);
		return -1;
	}

	ret = rte_event_dev_xstats_get(dev_id, mode, port_queue_id,
				       ids, values, num_xstats);
	if (ret < 0 || ret > num_xstats) {
		free(xstat_names);
		free(ids);
		free(values);
		return -1;
	}

	rte_tel_data_start_dict(d);
	for (i = 0; i < num_xstats; i++)
		rte_tel_data_add_dict_uint(d, xstat_names[i].name, values[i]);

	free(xstat_names);
	free(ids);
	free(values);
	return 0;
}

static int
handle_dev_xstats(const char *cmd __rte_unused,
		  const char *params,
		  struct rte_tel_data *d)
{
	int dev_id;
	enum rte_event_dev_xstats_mode mode;
	char *end_param;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -1;

	/* Get dev ID from parameter string */
	dev_id = strtoul(params, &end_param, 10);
	if (*end_param != '\0')
		RTE_EDEV_LOG_DEBUG(
			"Extra parameters passed to eventdev telemetry command, ignoring");

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);

	mode = RTE_EVENT_DEV_XSTATS_DEVICE;
	return eventdev_build_telemetry_data(dev_id, mode, 0, d);
}

static int
handle_port_xstats(const char *cmd __rte_unused,
		   const char *params,
		   struct rte_tel_data *d)
{
	int dev_id;
	int port_queue_id = 0;
	enum rte_event_dev_xstats_mode mode;
	char *end_param;
	const char *p_param;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -1;

	/* Get dev ID from parameter string */
	dev_id = strtoul(params, &end_param, 10);
	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);

	p_param = strtok(end_param, ",");
	mode = RTE_EVENT_DEV_XSTATS_PORT;

	if (p_param == NULL || strlen(p_param) == 0 || !isdigit(*p_param))
		return -1;

	port_queue_id = strtoul(p_param, &end_param, 10);

	p_param = strtok(NULL, "\0");
	if (p_param != NULL)
		RTE_EDEV_LOG_DEBUG(
			"Extra parameters passed to eventdev telemetry command, ignoring");

	return eventdev_build_telemetry_data(dev_id, mode, port_queue_id, d);
}

static int
handle_queue_xstats(const char *cmd __rte_unused,
		    const char *params,
		    struct rte_tel_data *d)
{
	int dev_id;
	int port_queue_id = 0;
	enum rte_event_dev_xstats_mode mode;
	char *end_param;
	const char *p_param;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -1;

	/* Get dev ID from parameter string */
	dev_id = strtoul(params, &end_param, 10);
	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);

	p_param = strtok(end_param, ",");
	mode = RTE_EVENT_DEV_XSTATS_QUEUE;

	if (p_param == NULL || strlen(p_param) == 0 || !isdigit(*p_param))
		return -1;

	port_queue_id = strtoul(p_param, &end_param, 10);

	p_param = strtok(NULL, "\0");
	if (p_param != NULL)
		RTE_EDEV_LOG_DEBUG(
			"Extra parameters passed to eventdev telemetry command, ignoring");

	return eventdev_build_telemetry_data(dev_id, mode, port_queue_id, d);
}

static int
handle_dev_dump(const char *cmd __rte_unused,
		const char *params,
		struct rte_tel_data *d)
{
	char *buf, *end_param;
	int dev_id, ret;
	FILE *f;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -1;

	/* Get dev ID from parameter string */
	dev_id = strtoul(params, &end_param, 10);
	if (*end_param != '\0')
		RTE_EDEV_LOG_DEBUG(
			"Extra parameters passed to eventdev telemetry command, ignoring");

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);

	buf = calloc(sizeof(char), RTE_TEL_MAX_SINGLE_STRING_LEN);
	if (buf == NULL)
		return -ENOMEM;

	f = fmemopen(buf, RTE_TEL_MAX_SINGLE_STRING_LEN - 1, "w+");
	if (f == NULL) {
		free(buf);
		return -EINVAL;
	}

	ret = rte_event_dev_dump(dev_id, f);
	fclose(f);
	if (ret == 0) {
		rte_tel_data_start_dict(d);
		rte_tel_data_string(d, buf);
	}

	free(buf);
	return ret;
}

RTE_INIT(eventdev_init_telemetry)
{
	rte_telemetry_register_cmd("/eventdev/dev_list", handle_dev_list,
			"Returns list of available eventdevs. Takes no parameters");
	rte_telemetry_register_cmd("/eventdev/port_list", handle_port_list,
			"Returns list of available ports. Parameter: DevID");
	rte_telemetry_register_cmd("/eventdev/queue_list", handle_queue_list,
			"Returns list of available queues. Parameter: DevID");

	rte_telemetry_register_cmd("/eventdev/dev_xstats", handle_dev_xstats,
			"Returns stats for an eventdev. Parameter: DevID");
	rte_telemetry_register_cmd("/eventdev/port_xstats", handle_port_xstats,
			"Returns stats for an eventdev port. Params: DevID,PortID");
	rte_telemetry_register_cmd("/eventdev/queue_xstats",
			handle_queue_xstats,
			"Returns stats for an eventdev queue. Params: DevID,QueueID");
	rte_telemetry_register_cmd("/eventdev/dev_dump", handle_dev_dump,
			"Returns dump information for an eventdev. Parameter: DevID");
	rte_telemetry_register_cmd("/eventdev/queue_links", handle_queue_links,
			"Returns links for an eventdev port. Params: DevID,QueueID");
}
