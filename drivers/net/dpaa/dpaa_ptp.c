/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2022-2024 NXP
 */

/* System headers */
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>

#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_eth_ctrl.h>
#include <rte_malloc.h>
#include <rte_time.h>

#include <dpaa_ethdev.h>
#include <dpaa_rxtx.h>

int dpaa_timesync_read_tx_timestamp(struct rte_eth_dev *dev,
						struct timespec *timestamp)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;

	if (dpaa_intf->next_tx_conf_queue) {
		while (!dpaa_intf->tx_timestamp)
			dpaa_eth_tx_conf(dpaa_intf->next_tx_conf_queue);
	} else {
		return -1;
	}
	*timestamp = rte_ns_to_timespec(dpaa_intf->tx_timestamp);

	return 0;
}

int dpaa_timesync_read_rx_timestamp(struct rte_eth_dev *dev,
						struct timespec *timestamp,
						uint32_t flags __rte_unused)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;
	*timestamp = rte_ns_to_timespec(dpaa_intf->rx_timestamp);
	return 0;
}
