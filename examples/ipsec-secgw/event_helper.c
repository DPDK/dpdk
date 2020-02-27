/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */
#include <rte_bitmap.h>
#include <rte_ethdev.h>
#include <rte_eventdev.h>
#include <rte_malloc.h>

#include "event_helper.h"

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

	/* Validate the requested config */
	ret = eh_validate_conf(em_conf);
	if (ret < 0) {
		EH_LOG_ERR("Failed to validate the requested config %d", ret);
		return ret;
	}

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
	uint16_t id;
	int ret, i;

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

	return 0;
}
