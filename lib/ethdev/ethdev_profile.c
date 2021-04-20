/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include "ethdev_profile.h"

/**
 * This conditional block enables Ethernet device profiling with
 * Intel (R) VTune (TM) Amplifier.
 */
#ifdef RTE_ETHDEV_PROFILE_WITH_VTUNE

/**
 * Hook callback to trace rte_eth_rx_burst() calls.
 */
uint16_t
profile_hook_rx_burst_cb(
	__rte_unused uint16_t port_id, __rte_unused uint16_t queue_id,
	__rte_unused struct rte_mbuf *pkts[], uint16_t nb_pkts,
	__rte_unused uint16_t max_pkts, __rte_unused void *user_param)
{
	return nb_pkts;
}

/**
 * Setting profiling rx callback for a given Ethernet device.
 * This function must be invoked when ethernet device is being configured.
 *
 * @param port_id
 *  The port identifier of the Ethernet device.
 * @param rx_queue_num
 *  The number of RX queues on specified port.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static inline int
vtune_profile_rx_init(uint16_t port_id, uint8_t rx_queue_num)
{
	uint16_t q_id;

	for (q_id = 0; q_id < rx_queue_num; ++q_id) {
		if (!rte_eth_add_rx_callback(
			port_id, q_id, profile_hook_rx_burst_cb, NULL)) {
			return -rte_errno;
		}
	}

	return 0;
}
#endif /* RTE_ETHDEV_PROFILE_WITH_VTUNE */

int
__rte_eth_dev_profile_init(__rte_unused uint16_t port_id,
	__rte_unused struct rte_eth_dev *dev)
{
#ifdef RTE_ETHDEV_PROFILE_WITH_VTUNE
	return vtune_profile_rx_init(port_id, dev->data->nb_rx_queues);
#endif
	return 0;
}
