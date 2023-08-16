/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifdef RTE_LIB_EVENTDEV
#include <stdbool.h>

#include "l3fwd.h"
#include "l3fwd_event.h"

static uint32_t
l3fwd_event_device_setup_internal_port(void)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();
	struct rte_event_dev_config event_d_conf = {
		.nb_events_limit  = 4096,
		.nb_event_queue_flows = 1024,
		.nb_event_port_dequeue_depth = 128,
		.nb_event_port_enqueue_depth = 128
	};
	struct rte_event_dev_info dev_info;
	const uint8_t event_d_id = 0; /* Always use first event device only */
	uint32_t event_queue_cfg = 0;
	uint16_t ethdev_count = 0;
	uint16_t num_workers = 0;
	uint16_t port_id;
	int ret;

	RTE_ETH_FOREACH_DEV(port_id) {
		if ((evt_rsrc->port_mask & (1 << port_id)) == 0)
			continue;
		ethdev_count++;
	}

	/* Event device configuration */
	rte_event_dev_info_get(event_d_id, &dev_info);

	/* Enable implicit release */
	if (dev_info.event_dev_cap & RTE_EVENT_DEV_CAP_IMPLICIT_RELEASE_DISABLE)
		evt_rsrc->disable_implicit_release = 0;

	if (dev_info.event_dev_cap & RTE_EVENT_DEV_CAP_QUEUE_ALL_TYPES)
		event_queue_cfg |= RTE_EVENT_QUEUE_CFG_ALL_TYPES;

	event_d_conf.nb_event_queues = ethdev_count;
	if (dev_info.max_event_queues < event_d_conf.nb_event_queues)
		event_d_conf.nb_event_queues = dev_info.max_event_queues;

	if (dev_info.max_num_events < event_d_conf.nb_events_limit)
		event_d_conf.nb_events_limit = dev_info.max_num_events;

	if (dev_info.max_event_queue_flows < event_d_conf.nb_event_queue_flows)
		event_d_conf.nb_event_queue_flows =
						dev_info.max_event_queue_flows;

	if (dev_info.max_event_port_dequeue_depth <
				event_d_conf.nb_event_port_dequeue_depth)
		event_d_conf.nb_event_port_dequeue_depth =
				dev_info.max_event_port_dequeue_depth;

	if (dev_info.max_event_port_enqueue_depth <
				event_d_conf.nb_event_port_enqueue_depth)
		event_d_conf.nb_event_port_enqueue_depth =
				dev_info.max_event_port_enqueue_depth;

	num_workers = rte_lcore_count();
	if (dev_info.max_event_ports < num_workers)
		num_workers = dev_info.max_event_ports;

	event_d_conf.nb_event_ports = num_workers;
	evt_rsrc->evp.nb_ports = num_workers;
	evt_rsrc->evq.nb_queues = event_d_conf.nb_event_queues;
	evt_rsrc->has_burst = !!(dev_info.event_dev_cap &
				    RTE_EVENT_DEV_CAP_BURST_MODE);

	ret = rte_event_dev_configure(event_d_id, &event_d_conf);
	if (ret < 0)
		rte_panic("Error in configuring event device\n");

	evt_rsrc->event_d_id = event_d_id;
	return event_queue_cfg;
}

static void
l3fwd_event_port_setup_internal_port(void)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();
	uint8_t event_d_id = evt_rsrc->event_d_id;
	struct rte_event_port_conf event_p_conf = {
		.dequeue_depth = 32,
		.enqueue_depth = 32,
		.new_event_threshold = 4096
	};
	struct rte_event_port_conf def_p_conf;
	uint8_t event_p_id;
	int32_t ret;

	evt_rsrc->evp.event_p_id = (uint8_t *)malloc(sizeof(uint8_t) *
					evt_rsrc->evp.nb_ports);
	if (!evt_rsrc->evp.event_p_id)
		rte_panic("Failed to allocate memory for Event Ports\n");

	ret = rte_event_port_default_conf_get(event_d_id, 0, &def_p_conf);
	if (ret < 0)
		rte_panic("Error to get default configuration of event port\n");

	if (def_p_conf.new_event_threshold < event_p_conf.new_event_threshold)
		event_p_conf.new_event_threshold =
						def_p_conf.new_event_threshold;

	if (def_p_conf.dequeue_depth < event_p_conf.dequeue_depth)
		event_p_conf.dequeue_depth = def_p_conf.dequeue_depth;

	if (def_p_conf.enqueue_depth < event_p_conf.enqueue_depth)
		event_p_conf.enqueue_depth = def_p_conf.enqueue_depth;

	event_p_conf.event_port_cfg = 0;
	if (evt_rsrc->disable_implicit_release)
		event_p_conf.event_port_cfg |=
			RTE_EVENT_PORT_CFG_DISABLE_IMPL_REL;

	evt_rsrc->deq_depth = def_p_conf.dequeue_depth;

	for (event_p_id = 0; event_p_id < evt_rsrc->evp.nb_ports;
								event_p_id++) {
		ret = rte_event_port_setup(event_d_id, event_p_id,
					   &event_p_conf);
		if (ret < 0)
			rte_panic("Error in configuring event port %d\n",
				  event_p_id);

		ret = rte_event_port_link(event_d_id, event_p_id, NULL,
					  NULL, 0);
		if (ret < 0)
			rte_panic("Error in linking event port %d to queue\n",
				  event_p_id);
		evt_rsrc->evp.event_p_id[event_p_id] = event_p_id;

		/* init spinlock */
		rte_spinlock_init(&evt_rsrc->evp.lock);
	}

	evt_rsrc->def_p_conf = event_p_conf;
}

static void
l3fwd_event_queue_setup_internal_port(uint32_t event_queue_cfg)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();
	uint8_t event_d_id = evt_rsrc->event_d_id;
	struct rte_event_queue_conf event_q_conf = {
		.nb_atomic_flows = 1024,
		.nb_atomic_order_sequences = 1024,
		.event_queue_cfg = event_queue_cfg,
		.priority = RTE_EVENT_DEV_PRIORITY_NORMAL
	};
	struct rte_event_queue_conf def_q_conf;
	uint8_t event_q_id = 0;
	int32_t ret;

	ret = rte_event_queue_default_conf_get(event_d_id, event_q_id,
					       &def_q_conf);
	if (ret < 0)
		rte_panic("Error to get default config of event queue\n");

	if (def_q_conf.nb_atomic_flows < event_q_conf.nb_atomic_flows)
		event_q_conf.nb_atomic_flows = def_q_conf.nb_atomic_flows;

	if (def_q_conf.nb_atomic_order_sequences <
					event_q_conf.nb_atomic_order_sequences)
		event_q_conf.nb_atomic_order_sequences =
					def_q_conf.nb_atomic_order_sequences;

	event_q_conf.event_queue_cfg = event_queue_cfg;
	event_q_conf.schedule_type = evt_rsrc->sched_type;
	evt_rsrc->evq.event_q_id = (uint8_t *)malloc(sizeof(uint8_t) *
					evt_rsrc->evq.nb_queues);
	if (!evt_rsrc->evq.event_q_id)
		rte_panic("Memory allocation failure\n");

	for (event_q_id = 0; event_q_id < evt_rsrc->evq.nb_queues;
								event_q_id++) {
		ret = rte_event_queue_setup(event_d_id, event_q_id,
					    &event_q_conf);
		if (ret < 0)
			rte_panic("Error in configuring event queue\n");
		evt_rsrc->evq.event_q_id[event_q_id] = event_q_id;
	}
}

static void
l3fwd_rx_tx_adapter_setup_internal_port(void)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();
	struct rte_event_eth_rx_adapter_queue_conf eth_q_conf;
	uint8_t event_d_id = evt_rsrc->event_d_id;
	uint16_t adapter_id = 0;
	uint16_t nb_adapter = 0;
	uint16_t port_id;
	uint8_t q_id = 0;
	int ret;

	memset(&eth_q_conf, 0, sizeof(eth_q_conf));
	eth_q_conf.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL;

	RTE_ETH_FOREACH_DEV(port_id) {
		if ((evt_rsrc->port_mask & (1 << port_id)) == 0)
			continue;
		nb_adapter++;
	}

	evt_rsrc->rx_adptr.nb_rx_adptr = nb_adapter;
	evt_rsrc->rx_adptr.rx_adptr = (uint8_t *)malloc(sizeof(uint8_t) *
					evt_rsrc->rx_adptr.nb_rx_adptr);
	if (!evt_rsrc->rx_adptr.rx_adptr) {
		free(evt_rsrc->evp.event_p_id);
		free(evt_rsrc->evq.event_q_id);
		rte_panic("Failed to allocate memory for Rx adapter\n");
	}

	RTE_ETH_FOREACH_DEV(port_id) {
		if ((evt_rsrc->port_mask & (1 << port_id)) == 0)
			continue;

		if (evt_rsrc->vector_enabled) {
			uint32_t cap;

			if (rte_event_eth_rx_adapter_caps_get(event_d_id,
							      port_id, &cap))
				rte_panic(
					"Failed to get event rx adapter capability");

			if (cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_EVENT_VECTOR) {
				eth_q_conf.vector_sz = evt_rsrc->vector_size;
				eth_q_conf.vector_timeout_ns =
					evt_rsrc->vector_tmo_ns;
				eth_q_conf.vector_mp =
					evt_rsrc->per_port_pool ?
						evt_rsrc->vec_pool[port_id] :
						evt_rsrc->vec_pool[0];
				eth_q_conf.rx_queue_flags |=
				RTE_EVENT_ETH_RX_ADAPTER_QUEUE_EVENT_VECTOR;
			} else {
				rte_panic(
					"Rx adapter doesn't support event vector");
			}
		}

		ret = rte_event_eth_rx_adapter_create(adapter_id, event_d_id,
						      &evt_rsrc->def_p_conf);
		if (ret)
			rte_panic("Failed to create rx adapter[%d]\n",
				  adapter_id);

		/* Configure user requested sched type*/
		eth_q_conf.ev.sched_type = evt_rsrc->sched_type;
		eth_q_conf.ev.queue_id = evt_rsrc->evq.event_q_id[q_id];
		ret = rte_event_eth_rx_adapter_queue_add(adapter_id, port_id,
							 -1, &eth_q_conf);
		if (ret)
			rte_panic("Failed to add queues to Rx adapter\n");

		ret = rte_event_eth_rx_adapter_start(adapter_id);
		if (ret)
			rte_panic("Rx adapter[%d] start Failed\n", adapter_id);

		evt_rsrc->rx_adptr.rx_adptr[adapter_id] = adapter_id;
		adapter_id++;
		if (q_id < evt_rsrc->evq.nb_queues)
			q_id++;
	}

	evt_rsrc->tx_adptr.nb_tx_adptr = nb_adapter;
	evt_rsrc->tx_adptr.tx_adptr = (uint8_t *)malloc(sizeof(uint8_t) *
					evt_rsrc->tx_adptr.nb_tx_adptr);
	if (!evt_rsrc->tx_adptr.tx_adptr) {
		free(evt_rsrc->rx_adptr.rx_adptr);
		free(evt_rsrc->evp.event_p_id);
		free(evt_rsrc->evq.event_q_id);
		rte_panic("Failed to allocate memory for Rx adapter\n");
	}

	adapter_id = 0;
	RTE_ETH_FOREACH_DEV(port_id) {
		if ((evt_rsrc->port_mask & (1 << port_id)) == 0)
			continue;
		ret = rte_event_eth_tx_adapter_create(adapter_id, event_d_id,
						&evt_rsrc->def_p_conf);
		if (ret)
			rte_panic("Failed to create tx adapter[%d]\n",
				  adapter_id);

		ret = rte_event_eth_tx_adapter_queue_add(adapter_id, port_id,
							 -1);
		if (ret)
			rte_panic("Failed to add queues to Tx adapter\n");

		ret = rte_event_eth_tx_adapter_start(adapter_id);
		if (ret)
			rte_panic("Tx adapter[%d] start Failed\n", adapter_id);

		evt_rsrc->tx_adptr.tx_adptr[adapter_id] = adapter_id;
		adapter_id++;
	}
}

void
l3fwd_event_set_internal_port_ops(struct l3fwd_event_setup_ops *ops)
{
	ops->event_device_setup = l3fwd_event_device_setup_internal_port;
	ops->event_queue_setup = l3fwd_event_queue_setup_internal_port;
	ops->event_port_setup = l3fwd_event_port_setup_internal_port;
	ops->adapter_setup = l3fwd_rx_tx_adapter_setup_internal_port;
}
#endif /* RTE_LIB_EVENTDEV */
