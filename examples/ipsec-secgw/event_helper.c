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
	bool single_ev_queue = false;
	int eventdev_id;
	int nb_eth_dev;
	int adapter_id;
	int conn_id;
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
	adapter->rx_core_id = eh_get_next_eth_core(em_conf);

	/*
	 * Map all queues of eth device (port) to an event queue. If there
	 * are more event queues than eth ports then create 1:1 mapping.
	 * Otherwise map all eth ports to a single event queue.
	 */
	if (nb_eth_dev > eventdev_config->nb_eventqueue)
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

		/* Update no of connections */
		adapter->nb_connections++;

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
	int eventdev_id;
	int adapter_id;
	int nb_eth_dev;
	int conn_id;
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

	/* TODO: Tx core is required only when internal port is not present */
	tx_adapter->tx_core_id = eh_get_next_eth_core(em_conf);

	/*
	 * Application uses one event queue per adapter for submitting
	 * packets for Tx. Reserve the last queue available and decrement
	 * the total available event queues for this
	 */

	/* Queue numbers start at 0 */
	tx_adapter->tx_ev_queue = eventdev_config->nb_eventqueue - 1;

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

		/* Update no of connections */
		tx_adapter->nb_connections++;
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
			 * schedule type for the application stage. One queue
			 * would be reserved for the final eth tx stage. This
			 * will be an atomic queue.
			 */
			if (j == nb_eventqueue-1) {
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
		EH_LOG_INFO(
			"\tRx adaper ID: %-2d\tConnections: %-2d\tEvent dev ID: %-2d"
			"\tRx core: %-2d",
			adapter->adapter_id,
			adapter->nb_connections,
			adapter->eventdev_id,
			adapter->rx_core_id);

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

		rte_eth_dev_stop(port_id);
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
