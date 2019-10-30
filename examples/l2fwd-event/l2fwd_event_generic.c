/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <stdbool.h>
#include <getopt.h>

#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_eventdev.h>
#include <rte_event_eth_rx_adapter.h>
#include <rte_event_eth_tx_adapter.h>
#include <rte_lcore.h>
#include <rte_spinlock.h>

#include "l2fwd_common.h"
#include "l2fwd_event.h"

static uint32_t
l2fwd_event_device_setup_generic(struct l2fwd_resources *rsrc)
{
	struct l2fwd_event_resources *evt_rsrc = rsrc->evt_rsrc;
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
		if ((rsrc->enabled_port_mask & (1 << port_id)) == 0)
			continue;
		ethdev_count++;
	}

	/* Event device configurtion */
	rte_event_dev_info_get(event_d_id, &dev_info);
	evt_rsrc->disable_implicit_release = !!(dev_info.event_dev_cap &
				    RTE_EVENT_DEV_CAP_IMPLICIT_RELEASE_DISABLE);

	if (dev_info.event_dev_cap & RTE_EVENT_DEV_CAP_QUEUE_ALL_TYPES)
		event_queue_cfg |= RTE_EVENT_QUEUE_CFG_ALL_TYPES;

	/* One queue for each ethdev port + one Tx adapter Single link queue. */
	event_d_conf.nb_event_queues = ethdev_count + 1;
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

	num_workers = rte_lcore_count() - rte_service_lcore_count();
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

void
l2fwd_event_set_generic_ops(struct event_setup_ops *ops)
{
	ops->event_device_setup = l2fwd_event_device_setup_generic;
}
