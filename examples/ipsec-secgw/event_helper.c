/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */
#include <rte_bitmap.h>
#include <rte_ethdev.h>
#include <rte_eventdev.h>
#include <rte_event_eth_rx_adapter.h>
#include <rte_event_eth_tx_adapter.h>
#include <rte_malloc.h>
#include <stdbool.h>

#include "event_helper.h"

static volatile bool eth_core_running;

static int
eh_get_enabled_cores(struct rte_bitmap *eth_core_mask)
{
	int i, count = 0;

	RTE_LCORE_FOREACH(i) {
		/* Check if this core is enabled in core mask*/
		if (rte_bitmap_get(eth_core_mask, i)) {
			/* Found enabled core */
			count++;
		}
	}
	return count;
}

static inline unsigned int
eh_get_next_eth_core(struct eventmode_conf *em_conf)
{
	static unsigned int prev_core = -1;
	unsigned int next_core;

	/*
	 * Make sure we have at least one eth core running, else the following
	 * logic would lead to an infinite loop.
	 */
	if (eh_get_enabled_cores(em_conf->eth_core_mask) == 0) {
		EH_LOG_ERR("No enabled eth core found");
		return RTE_MAX_LCORE;
	}

	/* Only some cores are marked as eth cores, skip others */
	do {
		/* Get the next core */
		next_core = rte_get_next_lcore(prev_core, 0, 1);

		/* Check if we have reached max lcores */
		if (next_core == RTE_MAX_LCORE)
			return next_core;

		/* Update prev_core */
		prev_core = next_core;
	} while (!(rte_bitmap_get(em_conf->eth_core_mask, next_core)));

	return next_core;
}

static inline unsigned int
eh_get_next_active_core(struct eventmode_conf *em_conf, unsigned int prev_core)
{
	unsigned int next_core;

	/* Get next active core skipping cores reserved as eth cores */
	do {
		/* Get the next core */
		next_core = rte_get_next_lcore(prev_core, 0, 0);

		/* Check if we have reached max lcores */
		if (next_core == RTE_MAX_LCORE)
			return next_core;

		prev_core = next_core;
	} while (rte_bitmap_get(em_conf->eth_core_mask, next_core));

	return next_core;
}

static struct eventdev_params *
eh_get_eventdev_params(struct eventmode_conf *em_conf, uint8_t eventdev_id)
{
	int i;

	for (i = 0; i < em_conf->nb_eventdev; i++) {
		if (em_conf->eventdev_config[i].eventdev_id == eventdev_id)
			break;
	}

	/* No match */
	if (i == em_conf->nb_eventdev)
		return NULL;

	return &(em_conf->eventdev_config[i]);
}

static inline bool
eh_dev_has_rx_internal_port(uint8_t eventdev_id)
{
	bool flag = true;
	int j, ret;

	RTE_ETH_FOREACH_DEV(j) {
		uint32_t caps = 0;

		ret = rte_event_eth_rx_adapter_caps_get(eventdev_id, j, &caps);
		if (ret < 0)
			return false;

		if (!(caps & RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT))
			flag = false;
	}
	return flag;
}

static inline bool
eh_dev_has_tx_internal_port(uint8_t eventdev_id)
{
	bool flag = true;
	int j, ret;

	RTE_ETH_FOREACH_DEV(j) {
		uint32_t caps = 0;

		ret = rte_event_eth_tx_adapter_caps_get(eventdev_id, j, &caps);
		if (ret < 0)
			return false;

		if (!(caps & RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT))
			flag = false;
	}
	return flag;
}

static inline bool
eh_dev_has_burst_mode(uint8_t dev_id)
{
	struct rte_event_dev_info dev_info;

	rte_event_dev_info_get(dev_id, &dev_info);
	return (dev_info.event_dev_cap & RTE_EVENT_DEV_CAP_BURST_MODE) ?
			true : false;
}

static int
eh_set_default_conf_eventdev(struct eventmode_conf *em_conf)
{
	int lcore_count, nb_eventdev, nb_eth_dev, ret;
	struct eventdev_params *eventdev_config;
	struct rte_event_dev_info dev_info;

	/* Get the number of event devices */
	nb_eventdev = rte_event_dev_count();
	if (nb_eventdev == 0) {
		EH_LOG_ERR("No event devices detected");
		return -EINVAL;
	}

	if (nb_eventdev != 1) {
		EH_LOG_ERR("Event mode does not support multiple event devices. "
			   "Please provide only one event device.");
		return -EINVAL;
	}

	/* Get the number of eth devs */
	nb_eth_dev = rte_eth_dev_count_avail();
	if (nb_eth_dev == 0) {
		EH_LOG_ERR("No eth devices detected");
		return -EINVAL;
	}

	/* Get the number of lcores */
	lcore_count = rte_lcore_count();

	/* Read event device info */
	ret = rte_event_dev_info_get(0, &dev_info);
	if (ret < 0) {
		EH_LOG_ERR("Failed to read event device info %d", ret);
		return ret;
	}

	/* Check if enough ports are available */
	if (dev_info.max_event_ports < 2) {
		EH_LOG_ERR("Not enough event ports available");
		return -EINVAL;
	}

	/* Get the first event dev conf */
	eventdev_config = &(em_conf->eventdev_config[0]);

	/* Save number of queues & ports available */
	eventdev_config->eventdev_id = 0;
	eventdev_config->nb_eventqueue = dev_info.max_event_queues;
	eventdev_config->nb_eventport = dev_info.max_event_ports;
	eventdev_config->ev_queue_mode = RTE_EVENT_QUEUE_CFG_ALL_TYPES;

	/* Check if there are more queues than required */
	if (eventdev_config->nb_eventqueue > nb_eth_dev + 1) {
		/* One queue is reserved for Tx */
		eventdev_config->nb_eventqueue = nb_eth_dev + 1;
	}

	/* Check if there are more ports than required */
	if (eventdev_config->nb_eventport > lcore_count) {
		/* One port per lcore is enough */
		eventdev_config->nb_eventport = lcore_count;
	}

	/* Update the number of event devices */
	em_conf->nb_eventdev++;

	return 0;
}

static void
eh_do_capability_check(struct eventmode_conf *em_conf)
{
	struct eventdev_params *eventdev_config;
	int all_internal_ports = 1;
	uint32_t eventdev_id;
	int i;

	for (i = 0; i < em_conf->nb_eventdev; i++) {

		/* Get the event dev conf */
		eventdev_config = &(em_conf->eventdev_config[i]);
		eventdev_id = eventdev_config->eventdev_id;

		/* Check if event device has internal port for Rx & Tx */
		if (eh_dev_has_rx_internal_port(eventdev_id) &&
		    eh_dev_has_tx_internal_port(eventdev_id)) {
			eventdev_config->all_internal_ports = 1;
		} else {
			all_internal_ports = 0;
		}
	}

	/*
	 * If Rx & Tx internal ports are supported by all event devices then
	 * eth cores won't be required. Override the eth core mask requested
	 * and decrement number of event queues by one as it won't be needed
	 * for Tx.
	 */
	if (all_internal_ports) {
		rte_bitmap_reset(em_conf->eth_core_mask);
		for (i = 0; i < em_conf->nb_eventdev; i++)
			em_conf->eventdev_config[i].nb_eventqueue--;
	}
}

static int
eh_set_default_conf_link(struct eventmode_conf *em_conf)
{
	struct eventdev_params *eventdev_config;
	struct eh_event_link_info *link;
	unsigned int lcore_id = -1;
	int i, link_index;

	/*
	 * Create a 1:1 mapping from event ports to cores. If the number
	 * of event ports is lesser than the cores, some cores won't
	 * execute worker. If there are more event ports, then some ports
	 * won't be used.
	 *
	 */

	/*
	 * The event queue-port mapping is done according to the link. Since
	 * we are falling back to the default link config, enabling
	 * "all_ev_queue_to_ev_port" mode flag. This will map all queues
	 * to the port.
	 */
	em_conf->ext_params.all_ev_queue_to_ev_port = 1;

	/* Get first event dev conf */
	eventdev_config = &(em_conf->eventdev_config[0]);

	/* Loop through the ports */
	for (i = 0; i < eventdev_config->nb_eventport; i++) {

		/* Get next active core id */
		lcore_id = eh_get_next_active_core(em_conf,
				lcore_id);

		if (lcore_id == RTE_MAX_LCORE) {
			/* Reached max cores */
			return 0;
		}

		/* Save the current combination as one link */

		/* Get the index */
		link_index = em_conf->nb_link;

		/* Get the corresponding link */
		link = &(em_conf->link[link_index]);

		/* Save link */
		link->eventdev_id = eventdev_config->eventdev_id;
		link->event_port_id = i;
		link->lcore_id = lcore_id;

		/*
		 * Don't set eventq_id as by default all queues
		 * need to be mapped to the port, which is controlled
		 * by the operating mode.
		 */

		/* Update number of links */
		em_conf->nb_link++;
	}

	return 0;
}

static int
eh_set_default_conf_rx_adapter(struct eventmode_conf *em_conf)
{
	struct rx_adapter_connection_info *conn;
	struct eventdev_params *eventdev_config;
	struct rx_adapter_conf *adapter;
	bool rx_internal_port = true;
	bool single_ev_queue = false;
	int nb_eventqueue;
	uint32_t caps = 0;
	int eventdev_id;
	int nb_eth_dev;
	int adapter_id;
	int conn_id;
	int ret;
	int i;

	/* Create one adapter with eth queues mapped to event queue(s) */

	if (em_conf->nb_eventdev == 0) {
		EH_LOG_ERR("No event devs registered");
		return -EINVAL;
	}

	/* Get the number of eth devs */
	nb_eth_dev = rte_eth_dev_count_avail();

	/* Use the first event dev */
	eventdev_config = &(em_conf->eventdev_config[0]);

	/* Get eventdev ID */
	eventdev_id = eventdev_config->eventdev_id;
	adapter_id = 0;

	/* Get adapter conf */
	adapter = &(em_conf->rx_adapter[adapter_id]);

	/* Set adapter conf */
	adapter->eventdev_id = eventdev_id;
	adapter->adapter_id = adapter_id;

	/*
	 * If event device does not have internal ports for passing
	 * packets then reserved one queue for Tx path
	 */
	nb_eventqueue = eventdev_config->all_internal_ports ?
			eventdev_config->nb_eventqueue :
			eventdev_config->nb_eventqueue - 1;

	/*
	 * Map all queues of eth device (port) to an event queue. If there
	 * are more event queues than eth ports then create 1:1 mapping.
	 * Otherwise map all eth ports to a single event queue.
	 */
	if (nb_eth_dev > nb_eventqueue)
		single_ev_queue = true;

	for (i = 0; i < nb_eth_dev; i++) {

		/* Use only the ports enabled */
		if ((em_conf->eth_portmask & (1 << i)) == 0)
			continue;

		/* Get the connection id */
		conn_id = adapter->nb_connections;

		/* Get the connection */
		conn = &(adapter->conn[conn_id]);

		/* Set mapping between eth ports & event queues*/
		conn->ethdev_id = i;
		conn->eventq_id = single_ev_queue ? 0 : i;

		/* Add all eth queues eth port to event queue */
		conn->ethdev_rx_qid = -1;

		/* Get Rx adapter capabilities */
		ret = rte_event_eth_rx_adapter_caps_get(eventdev_id, i, &caps);
		if (ret < 0) {
			EH_LOG_ERR("Failed to get event device %d eth rx adapter"
				   " capabilities for port %d", eventdev_id, i);
			return ret;
		}
		if (!(caps & RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT))
			rx_internal_port = false;

		/* Update no of connections */
		adapter->nb_connections++;

	}

	if (rx_internal_port) {
		/* Rx core is not required */
		adapter->rx_core_id = -1;
	} else {
		/* Rx core is required */
		adapter->rx_core_id = eh_get_next_eth_core(em_conf);
	}

	/* We have setup one adapter */
	em_conf->nb_rx_adapter = 1;

	return 0;
}

static int
eh_set_default_conf_tx_adapter(struct eventmode_conf *em_conf)
{
	struct tx_adapter_connection_info *conn;
	struct eventdev_params *eventdev_config;
	struct tx_adapter_conf *tx_adapter;
	bool tx_internal_port = true;
	uint32_t caps = 0;
	int eventdev_id;
	int adapter_id;
	int nb_eth_dev;
	int conn_id;
	int ret;
	int i;

	/*
	 * Create one Tx adapter with all eth queues mapped to event queues
	 * 1:1.
	 */

	if (em_conf->nb_eventdev == 0) {
		EH_LOG_ERR("No event devs registered");
		return -EINVAL;
	}

	/* Get the number of eth devs */
	nb_eth_dev = rte_eth_dev_count_avail();

	/* Use the first event dev */
	eventdev_config = &(em_conf->eventdev_config[0]);

	/* Get eventdev ID */
	eventdev_id = eventdev_config->eventdev_id;
	adapter_id = 0;

	/* Get adapter conf */
	tx_adapter = &(em_conf->tx_adapter[adapter_id]);

	/* Set adapter conf */
	tx_adapter->eventdev_id = eventdev_id;
	tx_adapter->adapter_id = adapter_id;

	/*
	 * Map all Tx queues of the eth device (port) to the event device.
	 */

	/* Set defaults for connections */

	/*
	 * One eth device (port) is one connection. Map all Tx queues
	 * of the device to the Tx adapter.
	 */

	for (i = 0; i < nb_eth_dev; i++) {

		/* Use only the ports enabled */
		if ((em_conf->eth_portmask & (1 << i)) == 0)
			continue;

		/* Get the connection id */
		conn_id = tx_adapter->nb_connections;

		/* Get the connection */
		conn = &(tx_adapter->conn[conn_id]);

		/* Add ethdev to connections */
		conn->ethdev_id = i;

		/* Add all eth tx queues to adapter */
		conn->ethdev_tx_qid = -1;

		/* Get Tx adapter capabilities */
		ret = rte_event_eth_tx_adapter_caps_get(eventdev_id, i, &caps);
		if (ret < 0) {
			EH_LOG_ERR("Failed to get event device %d eth tx adapter"
				   " capabilities for port %d", eventdev_id, i);
			return ret;
		}
		if (!(caps & RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT))
			tx_internal_port = false;

		/* Update no of connections */
		tx_adapter->nb_connections++;
	}

	if (tx_internal_port) {
		/* Tx core is not required */
		tx_adapter->tx_core_id = -1;
	} else {
		/* Tx core is required */
		tx_adapter->tx_core_id = eh_get_next_eth_core(em_conf);

		/*
		 * Use one event queue per adapter for submitting packets
		 * for Tx. Reserving the last queue available
		 */
		/* Queue numbers start at 0 */
		tx_adapter->tx_ev_queue = eventdev_config->nb_eventqueue - 1;
	}

	/* We have setup one adapter */
	em_conf->nb_tx_adapter = 1;
	return 0;
}

static int
eh_validate_conf(struct eventmode_conf *em_conf)
{
	int ret;

	/*
	 * Check if event devs are specified. Else probe the event devices
	 * and initialize the config with all ports & queues available
	 */
	if (em_conf->nb_eventdev == 0) {
		ret = eh_set_default_conf_eventdev(em_conf);
		if (ret != 0)
			return ret;
	}

	/* Perform capability check for the selected event devices */
	eh_do_capability_check(em_conf);

	/*
	 * Check if links are specified. Else generate a default config for
	 * the event ports used.
	 */
	if (em_conf->nb_link == 0) {
		ret = eh_set_default_conf_link(em_conf);
		if (ret != 0)
			return ret;
	}

	/*
	 * Check if rx adapters are specified. Else generate a default config
	 * with one rx adapter and all eth queues - event queue mapped.
	 */
	if (em_conf->nb_rx_adapter == 0) {
		ret = eh_set_default_conf_rx_adapter(em_conf);
		if (ret != 0)
			return ret;
	}

	/*
	 * Check if tx adapters are specified. Else generate a default config
	 * with one tx adapter.
	 */
	if (em_conf->nb_tx_adapter == 0) {
		ret = eh_set_default_conf_tx_adapter(em_conf);
		if (ret != 0)
			return ret;
	}

	return 0;
}

static int
eh_initialize_eventdev(struct eventmode_conf *em_conf)
{
	struct rte_event_queue_conf eventq_conf = {0};
	struct rte_event_dev_info evdev_default_conf;
	struct rte_event_dev_config eventdev_conf;
	struct eventdev_params *eventdev_config;
	int nb_eventdev = em_conf->nb_eventdev;
	struct eh_event_link_info *link;
	uint8_t *queue = NULL;
	uint8_t eventdev_id;
	int nb_eventqueue;
	uint8_t i, j;
	int ret;

	for (i = 0; i < nb_eventdev; i++) {

		/* Get eventdev config */
		eventdev_config = &(em_conf->eventdev_config[i]);

		/* Get event dev ID */
		eventdev_id = eventdev_config->eventdev_id;

		/* Get the number of queues */
		nb_eventqueue = eventdev_config->nb_eventqueue;

		/* Reset the default conf */
		memset(&evdev_default_conf, 0,
			sizeof(struct rte_event_dev_info));

		/* Get default conf of eventdev */
		ret = rte_event_dev_info_get(eventdev_id, &evdev_default_conf);
		if (ret < 0) {
			EH_LOG_ERR(
				"Error in getting event device info[devID:%d]",
				eventdev_id);
			return ret;
		}

		memset(&eventdev_conf, 0, sizeof(struct rte_event_dev_config));
		eventdev_conf.nb_events_limit =
				evdev_default_conf.max_num_events;
		eventdev_conf.nb_event_queues = nb_eventqueue;
		eventdev_conf.nb_event_ports =
				eventdev_config->nb_eventport;
		eventdev_conf.nb_event_queue_flows =
				evdev_default_conf.max_event_queue_flows;
		eventdev_conf.nb_event_port_dequeue_depth =
				evdev_default_conf.max_event_port_dequeue_depth;
		eventdev_conf.nb_event_port_enqueue_depth =
				evdev_default_conf.max_event_port_enqueue_depth;

		/* Configure event device */
		ret = rte_event_dev_configure(eventdev_id, &eventdev_conf);
		if (ret < 0) {
			EH_LOG_ERR("Error in configuring event device");
			return ret;
		}

		/* Configure event queues */
		for (j = 0; j < nb_eventqueue; j++) {

			memset(&eventq_conf, 0,
					sizeof(struct rte_event_queue_conf));

			/* Per event dev queues can be ATQ or SINGLE LINK */
			eventq_conf.event_queue_cfg =
					eventdev_config->ev_queue_mode;
			/*
			 * All queues need to be set with sched_type as
			 * schedule type for the application stage. One
			 * queue would be reserved for the final eth tx
			 * stage if event device does not have internal
			 * ports. This will be an atomic queue.
			 */
			if (!eventdev_config->all_internal_ports &&
			    j == nb_eventqueue-1) {
				eventq_conf.schedule_type =
					RTE_SCHED_TYPE_ATOMIC;
			} else {
				eventq_conf.schedule_type =
					em_conf->ext_params.sched_type;
			}

			/* Set max atomic flows to 1024 */
			eventq_conf.nb_atomic_flows = 1024;
			eventq_conf.nb_atomic_order_sequences = 1024;

			/* Setup the queue */
			ret = rte_event_queue_setup(eventdev_id, j,
					&eventq_conf);
			if (ret < 0) {
				EH_LOG_ERR("Failed to setup event queue %d",
					   ret);
				return ret;
			}
		}

		/* Configure event ports */
		for (j = 0; j <  eventdev_config->nb_eventport; j++) {
			ret = rte_event_port_setup(eventdev_id, j, NULL);
			if (ret < 0) {
				EH_LOG_ERR("Failed to setup event port %d",
					   ret);
				return ret;
			}
		}
	}

	/* Make event queue - event port link */
	for (j = 0; j <  em_conf->nb_link; j++) {

		/* Get link info */
		link = &(em_conf->link[j]);

		/* Get event dev ID */
		eventdev_id = link->eventdev_id;

		/*
		 * If "all_ev_queue_to_ev_port" params flag is selected, all
		 * queues need to be mapped to the port.
		 */
		if (em_conf->ext_params.all_ev_queue_to_ev_port)
			queue = NULL;
		else
			queue = &(link->eventq_id);

		/* Link queue to port */
		ret = rte_event_port_link(eventdev_id, link->event_port_id,
				queue, NULL, 1);
		if (ret < 0) {
			EH_LOG_ERR("Failed to link event port %d", ret);
			return ret;
		}
	}

	/* Start event devices */
	for (i = 0; i < nb_eventdev; i++) {

		/* Get eventdev config */
		eventdev_config = &(em_conf->eventdev_config[i]);

		ret = rte_event_dev_start(eventdev_config->eventdev_id);
		if (ret < 0) {
			EH_LOG_ERR("Failed to start event device %d, %d",
				   i, ret);
			return ret;
		}
	}
	return 0;
}

static int
eh_rx_adapter_configure(struct eventmode_conf *em_conf,
		struct rx_adapter_conf *adapter)
{
	struct rte_event_eth_rx_adapter_queue_conf queue_conf = {0};
	struct rte_event_dev_info evdev_default_conf = {0};
	struct rte_event_port_conf port_conf = {0};
	struct rx_adapter_connection_info *conn;
	uint8_t eventdev_id;
	uint32_t service_id;
	int ret;
	int j;

	/* Get event dev ID */
	eventdev_id = adapter->eventdev_id;

	/* Get default configuration of event dev */
	ret = rte_event_dev_info_get(eventdev_id, &evdev_default_conf);
	if (ret < 0) {
		EH_LOG_ERR("Failed to get event dev info %d", ret);
		return ret;
	}

	/* Setup port conf */
	port_conf.new_event_threshold = 1200;
	port_conf.dequeue_depth =
			evdev_default_conf.max_event_port_dequeue_depth;
	port_conf.enqueue_depth =
			evdev_default_conf.max_event_port_enqueue_depth;

	/* Create Rx adapter */
	ret = rte_event_eth_rx_adapter_create(adapter->adapter_id,
			adapter->eventdev_id, &port_conf);
	if (ret < 0) {
		EH_LOG_ERR("Failed to create rx adapter %d", ret);
		return ret;
	}

	/* Setup various connections in the adapter */
	for (j = 0; j < adapter->nb_connections; j++) {
		/* Get connection */
		conn = &(adapter->conn[j]);

		/* Setup queue conf */
		queue_conf.ev.queue_id = conn->eventq_id;
		queue_conf.ev.sched_type = em_conf->ext_params.sched_type;
		queue_conf.ev.event_type = RTE_EVENT_TYPE_ETHDEV;

		/* Add queue to the adapter */
		ret = rte_event_eth_rx_adapter_queue_add(adapter->adapter_id,
				conn->ethdev_id, conn->ethdev_rx_qid,
				&queue_conf);
		if (ret < 0) {
			EH_LOG_ERR("Failed to add eth queue to rx adapter %d",
				   ret);
			return ret;
		}
	}

	/* Get the service ID used by rx adapter */
	ret = rte_event_eth_rx_adapter_service_id_get(adapter->adapter_id,
						      &service_id);
	if (ret != -ESRCH && ret < 0) {
		EH_LOG_ERR("Failed to get service id used by rx adapter %d",
			   ret);
		return ret;
	}

	rte_service_set_runstate_mapped_check(service_id, 0);

	/* Start adapter */
	ret = rte_event_eth_rx_adapter_start(adapter->adapter_id);
	if (ret < 0) {
		EH_LOG_ERR("Failed to start rx adapter %d", ret);
		return ret;
	}

	return 0;
}

static int
eh_initialize_rx_adapter(struct eventmode_conf *em_conf)
{
	struct rx_adapter_conf *adapter;
	int i, ret;

	/* Configure rx adapters */
	for (i = 0; i < em_conf->nb_rx_adapter; i++) {
		adapter = &(em_conf->rx_adapter[i]);
		ret = eh_rx_adapter_configure(em_conf, adapter);
		if (ret < 0) {
			EH_LOG_ERR("Failed to configure rx adapter %d", ret);
			return ret;
		}
	}
	return 0;
}

static int32_t
eh_start_worker_eth_core(struct eventmode_conf *conf, uint32_t lcore_id)
{
	uint32_t service_id[EVENT_MODE_MAX_ADAPTERS_PER_RX_CORE];
	struct rx_adapter_conf *rx_adapter;
	struct tx_adapter_conf *tx_adapter;
	int service_count = 0;
	int adapter_id;
	int32_t ret;
	int i;

	EH_LOG_INFO("Entering eth_core processing on lcore %u", lcore_id);

	/*
	 * Parse adapter config to check which of all Rx adapters need
	 * to be handled by this core.
	 */
	for (i = 0; i < conf->nb_rx_adapter; i++) {
		/* Check if we have exceeded the max allowed */
		if (service_count > EVENT_MODE_MAX_ADAPTERS_PER_RX_CORE) {
			EH_LOG_ERR(
			      "Exceeded the max allowed adapters per rx core");
			break;
		}

		rx_adapter = &(conf->rx_adapter[i]);
		if (rx_adapter->rx_core_id != lcore_id)
			continue;

		/* Adapter is handled by this core */
		adapter_id = rx_adapter->adapter_id;

		/* Get the service ID for the adapters */
		ret = rte_event_eth_rx_adapter_service_id_get(adapter_id,
				&(service_id[service_count]));

		if (ret != -ESRCH && ret < 0) {
			EH_LOG_ERR(
				"Failed to get service id used by rx adapter");
			return ret;
		}

		/* Update service count */
		service_count++;
	}

	/*
	 * Parse adapter config to see which of all Tx adapters need
	 * to be handled by this core.
	 */
	for (i = 0; i < conf->nb_tx_adapter; i++) {
		/* Check if we have exceeded the max allowed */
		if (service_count > EVENT_MODE_MAX_ADAPTERS_PER_TX_CORE) {
			EH_LOG_ERR(
				"Exceeded the max allowed adapters per tx core");
			break;
		}

		tx_adapter = &conf->tx_adapter[i];
		if (tx_adapter->tx_core_id != lcore_id)
			continue;

		/* Adapter is handled by this core */
		adapter_id = tx_adapter->adapter_id;

		/* Get the service ID for the adapters */
		ret = rte_event_eth_tx_adapter_service_id_get(adapter_id,
				&(service_id[service_count]));

		if (ret != -ESRCH && ret < 0) {
			EH_LOG_ERR(
				"Failed to get service id used by tx adapter");
			return ret;
		}

		/* Update service count */
		service_count++;
	}

	eth_core_running = true;

	while (eth_core_running) {
		for (i = 0; i < service_count; i++) {
			/* Initiate adapter service */
			rte_service_run_iter_on_app_lcore(service_id[i], 0);
		}
	}

	return 0;
}

static int32_t
eh_stop_worker_eth_core(void)
{
	if (eth_core_running) {
		EH_LOG_INFO("Stopping eth cores");
		eth_core_running = false;
	}
	return 0;
}

static struct eh_app_worker_params *
eh_find_worker(uint32_t lcore_id, struct eh_conf *conf,
		struct eh_app_worker_params *app_wrkrs, uint8_t nb_wrkr_param)
{
	struct eh_app_worker_params curr_conf = { {{0} }, NULL};
	struct eh_event_link_info *link = NULL;
	struct eh_app_worker_params *tmp_wrkr;
	struct eventmode_conf *em_conf;
	uint8_t eventdev_id;
	int i;

	/* Get eventmode config */
	em_conf = conf->mode_params;

	/*
	 * Use event device from the first lcore-event link.
	 *
	 * Assumption: All lcore-event links tied to a core are using the
	 * same event device. In other words, one core would be polling on
	 * queues of a single event device only.
	 */

	/* Get a link for this lcore */
	for (i = 0; i < em_conf->nb_link; i++) {
		link = &(em_conf->link[i]);
		if (link->lcore_id == lcore_id)
			break;
	}

	if (link == NULL) {
		EH_LOG_ERR("No valid link found for lcore %d", lcore_id);
		return NULL;
	}

	/* Get event dev ID */
	eventdev_id = link->eventdev_id;

	/* Populate the curr_conf with the capabilities */

	/* Check for Tx internal port */
	if (eh_dev_has_tx_internal_port(eventdev_id))
		curr_conf.cap.tx_internal_port = EH_TX_TYPE_INTERNAL_PORT;
	else
		curr_conf.cap.tx_internal_port = EH_TX_TYPE_NO_INTERNAL_PORT;

	/* Check for burst mode */
	if (eh_dev_has_burst_mode(eventdev_id))
		curr_conf.cap.burst = EH_RX_TYPE_BURST;
	else
		curr_conf.cap.burst = EH_RX_TYPE_NON_BURST;

	curr_conf.cap.ipsec_mode = conf->ipsec_mode;

	/* Parse the passed list and see if we have matching capabilities */

	/* Initialize the pointer used to traverse the list */
	tmp_wrkr = app_wrkrs;

	for (i = 0; i < nb_wrkr_param; i++, tmp_wrkr++) {

		/* Skip this if capabilities are not matching */
		if (tmp_wrkr->cap.u64 != curr_conf.cap.u64)
			continue;

		/* If the checks pass, we have a match */
		return tmp_wrkr;
	}

	return NULL;
}

static int
eh_verify_match_worker(struct eh_app_worker_params *match_wrkr)
{
	/* Verify registered worker */
	if (match_wrkr->worker_thread == NULL) {
		EH_LOG_ERR("No worker registered");
		return 0;
	}

	/* Success */
	return 1;
}

static uint8_t
eh_get_event_lcore_links(uint32_t lcore_id, struct eh_conf *conf,
		struct eh_event_link_info **links)
{
	struct eh_event_link_info *link_cache;
	struct eventmode_conf *em_conf = NULL;
	struct eh_event_link_info *link;
	uint8_t lcore_nb_link = 0;
	size_t single_link_size;
	size_t cache_size;
	int index = 0;
	int i;

	if (conf == NULL || links == NULL) {
		EH_LOG_ERR("Invalid args");
		return -EINVAL;
	}

	/* Get eventmode conf */
	em_conf = conf->mode_params;

	if (em_conf == NULL) {
		EH_LOG_ERR("Invalid event mode parameters");
		return -EINVAL;
	}

	/* Get the number of links registered */
	for (i = 0; i < em_conf->nb_link; i++) {

		/* Get link */
		link = &(em_conf->link[i]);

		/* Check if we have link intended for this lcore */
		if (link->lcore_id == lcore_id) {

			/* Update the number of links for this core */
			lcore_nb_link++;

		}
	}

	/* Compute size of one entry to be copied */
	single_link_size = sizeof(struct eh_event_link_info);

	/* Compute size of the buffer required */
	cache_size = lcore_nb_link * sizeof(struct eh_event_link_info);

	/* Compute size of the buffer required */
	link_cache = calloc(1, cache_size);

	/* Get the number of links registered */
	for (i = 0; i < em_conf->nb_link; i++) {

		/* Get link */
		link = &(em_conf->link[i]);

		/* Check if we have link intended for this lcore */
		if (link->lcore_id == lcore_id) {

			/* Cache the link */
			memcpy(&link_cache[index], link, single_link_size);

			/* Update index */
			index++;
		}
	}

	/* Update the links for application to use the cached links */
	*links = link_cache;

	/* Return the number of cached links */
	return lcore_nb_link;
}

static int
eh_tx_adapter_configure(struct eventmode_conf *em_conf,
		struct tx_adapter_conf *adapter)
{
	struct rte_event_dev_info evdev_default_conf = {0};
	struct rte_event_port_conf port_conf = {0};
	struct tx_adapter_connection_info *conn;
	struct eventdev_params *eventdev_config;
	uint8_t tx_port_id = 0;
	uint8_t eventdev_id;
	uint32_t service_id;
	int ret, j;

	/* Get event dev ID */
	eventdev_id = adapter->eventdev_id;

	/* Get event device conf */
	eventdev_config = eh_get_eventdev_params(em_conf, eventdev_id);

	/* Create Tx adapter */

	/* Get default configuration of event dev */
	ret = rte_event_dev_info_get(eventdev_id, &evdev_default_conf);
	if (ret < 0) {
		EH_LOG_ERR("Failed to get event dev info %d", ret);
		return ret;
	}

	/* Setup port conf */
	port_conf.new_event_threshold =
			evdev_default_conf.max_num_events;
	port_conf.dequeue_depth =
			evdev_default_conf.max_event_port_dequeue_depth;
	port_conf.enqueue_depth =
			evdev_default_conf.max_event_port_enqueue_depth;

	/* Create adapter */
	ret = rte_event_eth_tx_adapter_create(adapter->adapter_id,
			adapter->eventdev_id, &port_conf);
	if (ret < 0) {
		EH_LOG_ERR("Failed to create tx adapter %d", ret);
		return ret;
	}

	/* Setup various connections in the adapter */
	for (j = 0; j < adapter->nb_connections; j++) {

		/* Get connection */
		conn = &(adapter->conn[j]);

		/* Add queue to the adapter */
		ret = rte_event_eth_tx_adapter_queue_add(adapter->adapter_id,
				conn->ethdev_id, conn->ethdev_tx_qid);
		if (ret < 0) {
			EH_LOG_ERR("Failed to add eth queue to tx adapter %d",
				   ret);
			return ret;
		}
	}

	/*
	 * Check if Tx core is assigned. If Tx core is not assigned then
	 * the adapter has internal port for submitting Tx packets and
	 * Tx event queue & port setup is not required
	 */
	if (adapter->tx_core_id == (uint32_t) (-1)) {
		/* Internal port is present */
		goto skip_tx_queue_port_setup;
	}

	/* Setup Tx queue & port */

	/* Get event port used by the adapter */
	ret = rte_event_eth_tx_adapter_event_port_get(
			adapter->adapter_id, &tx_port_id);
	if (ret) {
		EH_LOG_ERR("Failed to get tx adapter port id %d", ret);
		return ret;
	}

	/*
	 * Tx event queue is reserved for Tx adapter. Unlink this queue
	 * from all other ports
	 *
	 */
	for (j = 0; j < eventdev_config->nb_eventport; j++) {
		rte_event_port_unlink(eventdev_id, j,
				      &(adapter->tx_ev_queue), 1);
	}

	/* Link Tx event queue to Tx port */
	ret = rte_event_port_link(eventdev_id, tx_port_id,
			&(adapter->tx_ev_queue), NULL, 1);
	if (ret != 1) {
		EH_LOG_ERR("Failed to link event queue to port");
		return ret;
	}

	/* Get the service ID used by Tx adapter */
	ret = rte_event_eth_tx_adapter_service_id_get(adapter->adapter_id,
						      &service_id);
	if (ret != -ESRCH && ret < 0) {
		EH_LOG_ERR("Failed to get service id used by tx adapter %d",
			   ret);
		return ret;
	}

	rte_service_set_runstate_mapped_check(service_id, 0);

skip_tx_queue_port_setup:
	/* Start adapter */
	ret = rte_event_eth_tx_adapter_start(adapter->adapter_id);
	if (ret < 0) {
		EH_LOG_ERR("Failed to start tx adapter %d", ret);
		return ret;
	}

	return 0;
}

static int
eh_initialize_tx_adapter(struct eventmode_conf *em_conf)
{
	struct tx_adapter_conf *adapter;
	int i, ret;

	/* Configure Tx adapters */
	for (i = 0; i < em_conf->nb_tx_adapter; i++) {
		adapter = &(em_conf->tx_adapter[i]);
		ret = eh_tx_adapter_configure(em_conf, adapter);
		if (ret < 0) {
			EH_LOG_ERR("Failed to configure tx adapter %d", ret);
			return ret;
		}
	}
	return 0;
}

static void
eh_display_operating_mode(struct eventmode_conf *em_conf)
{
	char sched_types[][32] = {
		"RTE_SCHED_TYPE_ORDERED",
		"RTE_SCHED_TYPE_ATOMIC",
		"RTE_SCHED_TYPE_PARALLEL",
	};
	EH_LOG_INFO("Operating mode:");

	EH_LOG_INFO("\tScheduling type: \t%s",
		sched_types[em_conf->ext_params.sched_type]);

	EH_LOG_INFO("");
}

static void
eh_display_event_dev_conf(struct eventmode_conf *em_conf)
{
	char queue_mode[][32] = {
		"",
		"ATQ (ALL TYPE QUEUE)",
		"SINGLE LINK",
	};
	char print_buf[256] = { 0 };
	int i;

	EH_LOG_INFO("Event Device Configuration:");

	for (i = 0; i < em_conf->nb_eventdev; i++) {
		sprintf(print_buf,
			"\tDev ID: %-2d \tQueues: %-2d \tPorts: %-2d",
			em_conf->eventdev_config[i].eventdev_id,
			em_conf->eventdev_config[i].nb_eventqueue,
			em_conf->eventdev_config[i].nb_eventport);
		sprintf(print_buf + strlen(print_buf),
			"\tQueue mode: %s",
			queue_mode[em_conf->eventdev_config[i].ev_queue_mode]);
		EH_LOG_INFO("%s", print_buf);
	}
	EH_LOG_INFO("");
}

static void
eh_display_rx_adapter_conf(struct eventmode_conf *em_conf)
{
	int nb_rx_adapter = em_conf->nb_rx_adapter;
	struct rx_adapter_connection_info *conn;
	struct rx_adapter_conf *adapter;
	char print_buf[256] = { 0 };
	int i, j;

	EH_LOG_INFO("Rx adapters configured: %d", nb_rx_adapter);

	for (i = 0; i < nb_rx_adapter; i++) {
		adapter = &(em_conf->rx_adapter[i]);
		sprintf(print_buf,
			"\tRx adaper ID: %-2d\tConnections: %-2d\tEvent dev ID: %-2d",
			adapter->adapter_id,
			adapter->nb_connections,
			adapter->eventdev_id);
		if (adapter->rx_core_id == (uint32_t)-1)
			sprintf(print_buf + strlen(print_buf),
				"\tRx core: %-2s", "[INTERNAL PORT]");
		else if (adapter->rx_core_id == RTE_MAX_LCORE)
			sprintf(print_buf + strlen(print_buf),
				"\tRx core: %-2s", "[NONE]");
		else
			sprintf(print_buf + strlen(print_buf),
				"\tRx core: %-2d", adapter->rx_core_id);

		EH_LOG_INFO("%s", print_buf);

		for (j = 0; j < adapter->nb_connections; j++) {
			conn = &(adapter->conn[j]);

			sprintf(print_buf,
				"\t\tEthdev ID: %-2d", conn->ethdev_id);

			if (conn->ethdev_rx_qid == -1)
				sprintf(print_buf + strlen(print_buf),
					"\tEth rx queue: %-2s", "ALL");
			else
				sprintf(print_buf + strlen(print_buf),
					"\tEth rx queue: %-2d",
					conn->ethdev_rx_qid);

			sprintf(print_buf + strlen(print_buf),
				"\tEvent queue: %-2d", conn->eventq_id);
			EH_LOG_INFO("%s", print_buf);
		}
	}
	EH_LOG_INFO("");
}

static void
eh_display_tx_adapter_conf(struct eventmode_conf *em_conf)
{
	int nb_tx_adapter = em_conf->nb_tx_adapter;
	struct tx_adapter_connection_info *conn;
	struct tx_adapter_conf *adapter;
	char print_buf[256] = { 0 };
	int i, j;

	EH_LOG_INFO("Tx adapters configured: %d", nb_tx_adapter);

	for (i = 0; i < nb_tx_adapter; i++) {
		adapter = &(em_conf->tx_adapter[i]);
		sprintf(print_buf,
			"\tTx adapter ID: %-2d\tConnections: %-2d\tEvent dev ID: %-2d",
			adapter->adapter_id,
			adapter->nb_connections,
			adapter->eventdev_id);
		if (adapter->tx_core_id == (uint32_t)-1)
			sprintf(print_buf + strlen(print_buf),
				"\tTx core: %-2s", "[INTERNAL PORT]");
		else if (adapter->tx_core_id == RTE_MAX_LCORE)
			sprintf(print_buf + strlen(print_buf),
				"\tTx core: %-2s", "[NONE]");
		else
			sprintf(print_buf + strlen(print_buf),
				"\tTx core: %-2d,\tInput event queue: %-2d",
				adapter->tx_core_id, adapter->tx_ev_queue);

		EH_LOG_INFO("%s", print_buf);

		for (j = 0; j < adapter->nb_connections; j++) {
			conn = &(adapter->conn[j]);

			sprintf(print_buf,
				"\t\tEthdev ID: %-2d", conn->ethdev_id);

			if (conn->ethdev_tx_qid == -1)
				sprintf(print_buf + strlen(print_buf),
					"\tEth tx queue: %-2s", "ALL");
			else
				sprintf(print_buf + strlen(print_buf),
					"\tEth tx queue: %-2d",
					conn->ethdev_tx_qid);
			EH_LOG_INFO("%s", print_buf);
		}
	}
	EH_LOG_INFO("");
}

static void
eh_display_link_conf(struct eventmode_conf *em_conf)
{
	struct eh_event_link_info *link;
	char print_buf[256] = { 0 };
	int i;

	EH_LOG_INFO("Links configured: %d", em_conf->nb_link);

	for (i = 0; i < em_conf->nb_link; i++) {
		link = &(em_conf->link[i]);

		sprintf(print_buf,
			"\tEvent dev ID: %-2d\tEvent port: %-2d",
			link->eventdev_id,
			link->event_port_id);

		if (em_conf->ext_params.all_ev_queue_to_ev_port)
			sprintf(print_buf + strlen(print_buf),
				"Event queue: %-2s\t", "ALL");
		else
			sprintf(print_buf + strlen(print_buf),
				"Event queue: %-2d\t", link->eventq_id);

		sprintf(print_buf + strlen(print_buf),
			"Lcore: %-2d", link->lcore_id);
		EH_LOG_INFO("%s", print_buf);
	}
	EH_LOG_INFO("");
}

struct eh_conf *
eh_conf_init(void)
{
	struct eventmode_conf *em_conf = NULL;
	struct eh_conf *conf = NULL;
	unsigned int eth_core_id;
	void *bitmap = NULL;
	uint32_t nb_bytes;

	/* Allocate memory for config */
	conf = calloc(1, sizeof(struct eh_conf));
	if (conf == NULL) {
		EH_LOG_ERR("Failed to allocate memory for eventmode helper "
			   "config");
		return NULL;
	}

	/* Set default conf */

	/* Packet transfer mode: poll */
	conf->mode = EH_PKT_TRANSFER_MODE_POLL;
	conf->ipsec_mode = EH_IPSEC_MODE_TYPE_APP;

	/* Keep all ethernet ports enabled by default */
	conf->eth_portmask = -1;

	/* Allocate memory for event mode params */
	conf->mode_params = calloc(1, sizeof(struct eventmode_conf));
	if (conf->mode_params == NULL) {
		EH_LOG_ERR("Failed to allocate memory for event mode params");
		goto free_conf;
	}

	/* Get eventmode conf */
	em_conf = conf->mode_params;

	/* Allocate and initialize bitmap for eth cores */
	nb_bytes = rte_bitmap_get_memory_footprint(RTE_MAX_LCORE);
	if (!nb_bytes) {
		EH_LOG_ERR("Failed to get bitmap footprint");
		goto free_em_conf;
	}

	bitmap = rte_zmalloc("event-helper-ethcore-bitmap", nb_bytes,
			     RTE_CACHE_LINE_SIZE);
	if (!bitmap) {
		EH_LOG_ERR("Failed to allocate memory for eth cores bitmap\n");
		goto free_em_conf;
	}

	em_conf->eth_core_mask = rte_bitmap_init(RTE_MAX_LCORE, bitmap,
						 nb_bytes);
	if (!em_conf->eth_core_mask) {
		EH_LOG_ERR("Failed to initialize bitmap");
		goto free_bitmap;
	}

	/* Set schedule type as not set */
	em_conf->ext_params.sched_type = SCHED_TYPE_NOT_SET;

	/* Set two cores as eth cores for Rx & Tx */

	/* Use first core other than main core as Rx core */
	eth_core_id = rte_get_next_lcore(0,	/* curr core */
					 1,	/* skip main core */
					 0	/* wrap */);

	rte_bitmap_set(em_conf->eth_core_mask, eth_core_id);

	/* Use next core as Tx core */
	eth_core_id = rte_get_next_lcore(eth_core_id,	/* curr core */
					 1,		/* skip main core */
					 0		/* wrap */);

	rte_bitmap_set(em_conf->eth_core_mask, eth_core_id);

	return conf;

free_bitmap:
	rte_free(bitmap);
free_em_conf:
	free(em_conf);
free_conf:
	free(conf);
	return NULL;
}

void
eh_conf_uninit(struct eh_conf *conf)
{
	struct eventmode_conf *em_conf = NULL;

	if (!conf || !conf->mode_params)
		return;

	/* Get eventmode conf */
	em_conf = conf->mode_params;

	/* Free evenmode configuration memory */
	rte_free(em_conf->eth_core_mask);
	free(em_conf);
	free(conf);
}

void
eh_display_conf(struct eh_conf *conf)
{
	struct eventmode_conf *em_conf;

	if (conf == NULL) {
		EH_LOG_ERR("Invalid event helper configuration");
		return;
	}

	if (conf->mode != EH_PKT_TRANSFER_MODE_EVENT)
		return;

	if (conf->mode_params == NULL) {
		EH_LOG_ERR("Invalid event mode parameters");
		return;
	}

	/* Get eventmode conf */
	em_conf = (struct eventmode_conf *)(conf->mode_params);

	/* Display user exposed operating modes */
	eh_display_operating_mode(em_conf);

	/* Display event device conf */
	eh_display_event_dev_conf(em_conf);

	/* Display Rx adapter conf */
	eh_display_rx_adapter_conf(em_conf);

	/* Display Tx adapter conf */
	eh_display_tx_adapter_conf(em_conf);

	/* Display event-lcore link */
	eh_display_link_conf(em_conf);
}

int32_t
eh_devs_init(struct eh_conf *conf)
{
	struct eventmode_conf *em_conf;
	uint16_t port_id;
	int ret;

	if (conf == NULL) {
		EH_LOG_ERR("Invalid event helper configuration");
		return -EINVAL;
	}

	if (conf->mode != EH_PKT_TRANSFER_MODE_EVENT)
		return 0;

	if (conf->mode_params == NULL) {
		EH_LOG_ERR("Invalid event mode parameters");
		return -EINVAL;
	}

	/* Get eventmode conf */
	em_conf = conf->mode_params;

	/* Eventmode conf would need eth portmask */
	em_conf->eth_portmask = conf->eth_portmask;

	/* Validate the requested config */
	ret = eh_validate_conf(em_conf);
	if (ret < 0) {
		EH_LOG_ERR("Failed to validate the requested config %d", ret);
		return ret;
	}

	/* Display the current configuration */
	eh_display_conf(conf);

	/* Stop eth devices before setting up adapter */
	RTE_ETH_FOREACH_DEV(port_id) {

		/* Use only the ports enabled */
		if ((conf->eth_portmask & (1 << port_id)) == 0)
			continue;

		ret = rte_eth_dev_stop(port_id);
		if (ret != 0) {
			EH_LOG_ERR("Failed to stop port %u, err: %d",
					port_id, ret);
			return ret;
		}
	}

	/* Setup eventdev */
	ret = eh_initialize_eventdev(em_conf);
	if (ret < 0) {
		EH_LOG_ERR("Failed to initialize event dev %d", ret);
		return ret;
	}

	/* Setup Rx adapter */
	ret = eh_initialize_rx_adapter(em_conf);
	if (ret < 0) {
		EH_LOG_ERR("Failed to initialize rx adapter %d", ret);
		return ret;
	}

	/* Setup Tx adapter */
	ret = eh_initialize_tx_adapter(em_conf);
	if (ret < 0) {
		EH_LOG_ERR("Failed to initialize tx adapter %d", ret);
		return ret;
	}

	/* Start eth devices after setting up adapter */
	RTE_ETH_FOREACH_DEV(port_id) {

		/* Use only the ports enabled */
		if ((conf->eth_portmask & (1 << port_id)) == 0)
			continue;

		ret = rte_eth_dev_start(port_id);
		if (ret < 0) {
			EH_LOG_ERR("Failed to start eth dev %d, %d",
				   port_id, ret);
			return ret;
		}
	}

	return 0;
}

int32_t
eh_devs_uninit(struct eh_conf *conf)
{
	struct eventmode_conf *em_conf;
	int ret, i, j;
	uint16_t id;

	if (conf == NULL) {
		EH_LOG_ERR("Invalid event helper configuration");
		return -EINVAL;
	}

	if (conf->mode != EH_PKT_TRANSFER_MODE_EVENT)
		return 0;

	if (conf->mode_params == NULL) {
		EH_LOG_ERR("Invalid event mode parameters");
		return -EINVAL;
	}

	/* Get eventmode conf */
	em_conf = conf->mode_params;

	/* Stop and release rx adapters */
	for (i = 0; i < em_conf->nb_rx_adapter; i++) {

		id = em_conf->rx_adapter[i].adapter_id;
		ret = rte_event_eth_rx_adapter_stop(id);
		if (ret < 0) {
			EH_LOG_ERR("Failed to stop rx adapter %d", ret);
			return ret;
		}

		for (j = 0; j < em_conf->rx_adapter[i].nb_connections; j++) {

			ret = rte_event_eth_rx_adapter_queue_del(id,
				em_conf->rx_adapter[i].conn[j].ethdev_id, -1);
			if (ret < 0) {
				EH_LOG_ERR(
				       "Failed to remove rx adapter queues %d",
				       ret);
				return ret;
			}
		}

		ret = rte_event_eth_rx_adapter_free(id);
		if (ret < 0) {
			EH_LOG_ERR("Failed to free rx adapter %d", ret);
			return ret;
		}
	}

	/* Stop and release event devices */
	for (i = 0; i < em_conf->nb_eventdev; i++) {

		id = em_conf->eventdev_config[i].eventdev_id;
		rte_event_dev_stop(id);

		ret = rte_event_dev_close(id);
		if (ret < 0) {
			EH_LOG_ERR("Failed to close event dev %d, %d", id, ret);
			return ret;
		}
	}

	/* Stop and release tx adapters */
	for (i = 0; i < em_conf->nb_tx_adapter; i++) {

		id = em_conf->tx_adapter[i].adapter_id;
		ret = rte_event_eth_tx_adapter_stop(id);
		if (ret < 0) {
			EH_LOG_ERR("Failed to stop tx adapter %d", ret);
			return ret;
		}

		for (j = 0; j < em_conf->tx_adapter[i].nb_connections; j++) {

			ret = rte_event_eth_tx_adapter_queue_del(id,
				em_conf->tx_adapter[i].conn[j].ethdev_id, -1);
			if (ret < 0) {
				EH_LOG_ERR(
					"Failed to remove tx adapter queues %d",
					ret);
				return ret;
			}
		}

		ret = rte_event_eth_tx_adapter_free(id);
		if (ret < 0) {
			EH_LOG_ERR("Failed to free tx adapter %d", ret);
			return ret;
		}
	}

	return 0;
}

void
eh_launch_worker(struct eh_conf *conf, struct eh_app_worker_params *app_wrkr,
		uint8_t nb_wrkr_param)
{
	struct eh_app_worker_params *match_wrkr;
	struct eh_event_link_info *links = NULL;
	struct eventmode_conf *em_conf;
	uint32_t lcore_id;
	uint8_t nb_links;

	if (conf == NULL) {
		EH_LOG_ERR("Invalid event helper configuration");
		return;
	}

	if (conf->mode_params == NULL) {
		EH_LOG_ERR("Invalid event mode parameters");
		return;
	}

	/* Get eventmode conf */
	em_conf = conf->mode_params;

	/* Get core ID */
	lcore_id = rte_lcore_id();

	/* Check if this is eth core */
	if (rte_bitmap_get(em_conf->eth_core_mask, lcore_id)) {
		eh_start_worker_eth_core(em_conf, lcore_id);
		return;
	}

	if (app_wrkr == NULL || nb_wrkr_param == 0) {
		EH_LOG_ERR("Invalid args");
		return;
	}

	/*
	 * This is a regular worker thread. The application registers
	 * multiple workers with various capabilities. Run worker
	 * based on the selected capabilities of the event
	 * device configured.
	 */

	/* Get the first matching worker for the event device */
	match_wrkr = eh_find_worker(lcore_id, conf, app_wrkr, nb_wrkr_param);
	if (match_wrkr == NULL) {
		EH_LOG_ERR("Failed to match worker registered for lcore %d",
			   lcore_id);
		goto clean_and_exit;
	}

	/* Verify sanity of the matched worker */
	if (eh_verify_match_worker(match_wrkr) != 1) {
		EH_LOG_ERR("Failed to validate the matched worker");
		goto clean_and_exit;
	}

	/* Get worker links */
	nb_links = eh_get_event_lcore_links(lcore_id, conf, &links);

	/* Launch the worker thread */
	match_wrkr->worker_thread(links, nb_links);

	/* Free links info memory */
	free(links);

clean_and_exit:

	/* Flag eth_cores to stop, if started */
	eh_stop_worker_eth_core();
}

uint8_t
eh_get_tx_queue(struct eh_conf *conf, uint8_t eventdev_id)
{
	struct eventdev_params *eventdev_config;
	struct eventmode_conf *em_conf;

	if (conf == NULL) {
		EH_LOG_ERR("Invalid event helper configuration");
		return -EINVAL;
	}

	if (conf->mode_params == NULL) {
		EH_LOG_ERR("Invalid event mode parameters");
		return -EINVAL;
	}

	/* Get eventmode conf */
	em_conf = conf->mode_params;

	/* Get event device conf */
	eventdev_config = eh_get_eventdev_params(em_conf, eventdev_id);

	if (eventdev_config == NULL) {
		EH_LOG_ERR("Failed to read eventdev config");
		return -EINVAL;
	}

	/*
	 * The last queue is reserved to be used as atomic queue for the
	 * last stage (eth packet tx stage)
	 */
	return eventdev_config->nb_eventqueue - 1;
}
