/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_log.h>
#include <rte_compat.h>
#include <rte_dev.h>


int __rte_experimental
rte_dev_event_monitor_start(void)
{
	/* TODO: start uevent monitor for linux */
	return 0;
}

int __rte_experimental
rte_dev_event_monitor_stop(void)
{
	/* TODO: stop uevent monitor for linux */
	return 0;
}
