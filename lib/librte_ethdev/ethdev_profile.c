/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include "ethdev_profile.h"

/**
 * This conditional block enables RX queues profiling by tracking wasted
 * iterations, i.e. iterations which yielded no RX packets. Profiling is
 * performed using the Instrumentation and Tracing Technology (ITT) API,
 * employed by the Intel (R) VTune (TM) Amplifier.
 */
#ifdef RTE_ETHDEV_PROFILE_ITT_WASTED_RX_ITERATIONS

#include <ittnotify.h>

#define ITT_MAX_NAME_LEN (100)

/**
 * Auxiliary ITT structure belonging to Ethernet device and using to:
 *   -  track RX queue state to determine whether it is wasting loop iterations
 *   -  begin or end ITT task using task domain and task name (handle)
 */
struct itt_profile_rx_data {
	/**
	 * ITT domains for each queue.
	 */
	__itt_domain *domains[RTE_MAX_QUEUES_PER_PORT];
	/**
	 * ITT task names for each queue.
	 */
	__itt_string_handle *handles[RTE_MAX_QUEUES_PER_PORT];
	/**
	 * Flags indicating the queues state. Possible values:
	 *   1 - queue is wasting iterations,
	 *   0 - otherwise.
	 */
	uint8_t queue_state[RTE_MAX_QUEUES_PER_PORT];
};

/**
 * The pool of *itt_profile_rx_data* structures.
 */
struct itt_profile_rx_data itt_rx_data[RTE_MAX_ETHPORTS];


/**
 * This callback function manages ITT tasks collection on given port and queue.
 * It must be registered with rte_eth_add_rx_callback() to be called from
 * rte_eth_rx_burst(). To find more comments see rte_rx_callback_fn function
 * type declaration.
 */
static uint16_t
collect_itt_rx_burst_cb(uint16_t port_id, uint16_t queue_id,
	__rte_unused struct rte_mbuf *pkts[], uint16_t nb_pkts,
	__rte_unused uint16_t max_pkts, __rte_unused void *user_param)
{
	if (unlikely(nb_pkts == 0)) {
		if (!itt_rx_data[port_id].queue_state[queue_id]) {
			__itt_task_begin(
				itt_rx_data[port_id].domains[queue_id],
				__itt_null, __itt_null,
				itt_rx_data[port_id].handles[queue_id]);
			itt_rx_data[port_id].queue_state[queue_id] = 1;
		}
	} else {
		if (unlikely(itt_rx_data[port_id].queue_state[queue_id])) {
			__itt_task_end(
				itt_rx_data[port_id].domains[queue_id]);
			itt_rx_data[port_id].queue_state[queue_id] = 0;
		}
	}
	return nb_pkts;
}

/**
 * Initialization of itt_profile_rx_data for a given Ethernet device.
 * This function must be invoked when ethernet device is being configured.
 * Result will be stored in the global array *itt_rx_data*.
 *
 * @param port_id
 *  The port identifier of the Ethernet device.
 * @param port_name
 *  The name of the Ethernet device.
 * @param rx_queue_num
 *  The number of RX queues on specified port.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static inline int
itt_profile_rx_init(uint16_t port_id, char *port_name, uint8_t rx_queue_num)
{
	uint16_t q_id;

	for (q_id = 0; q_id < rx_queue_num; ++q_id) {
		char domain_name[ITT_MAX_NAME_LEN];

		snprintf(domain_name, sizeof(domain_name),
			"RXBurst.WastedIterations.Port_%s.Queue_%d",
			port_name, q_id);
		itt_rx_data[port_id].domains[q_id]
			= __itt_domain_create(domain_name);

		char task_name[ITT_MAX_NAME_LEN];

		snprintf(task_name, sizeof(task_name),
			"port id: %d; queue id: %d",
			port_id, q_id);
		itt_rx_data[port_id].handles[q_id]
			= __itt_string_handle_create(task_name);

		itt_rx_data[port_id].queue_state[q_id] = 0;

		if (!rte_eth_add_rx_callback(
			port_id, q_id, collect_itt_rx_burst_cb, NULL)) {
			return -rte_errno;
		}
	}

	return 0;
}
#endif /* RTE_ETHDEV_PROFILE_ITT_WASTED_RX_ITERATIONS */

int
__rte_eth_profile_rx_init(__rte_unused uint16_t port_id,
	__rte_unused struct rte_eth_dev *dev)
{
#ifdef RTE_ETHDEV_PROFILE_ITT_WASTED_RX_ITERATIONS
	return itt_profile_rx_init(
		port_id, dev->data->name, dev->data->nb_rx_queues);
#endif
	return 0;
}
