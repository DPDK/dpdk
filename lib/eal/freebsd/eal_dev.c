/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_log.h>
#include <rte_dev.h>

#include <eal_export.h>
#include "eal_private.h"

RTE_EXPORT_SYMBOL(rte_dev_event_monitor_start)
int
rte_dev_event_monitor_start(void)
{
	EAL_LOG(ERR, "Device event is not supported for FreeBSD");
	return -1;
}

RTE_EXPORT_SYMBOL(rte_dev_event_monitor_stop)
int
rte_dev_event_monitor_stop(void)
{
	EAL_LOG(ERR, "Device event is not supported for FreeBSD");
	return -1;
}

RTE_EXPORT_SYMBOL(rte_dev_hotplug_handle_enable)
int
rte_dev_hotplug_handle_enable(void)
{
	EAL_LOG(ERR, "Device event is not supported for FreeBSD");
	return -1;
}

RTE_EXPORT_SYMBOL(rte_dev_hotplug_handle_disable)
int
rte_dev_hotplug_handle_disable(void)
{
	EAL_LOG(ERR, "Device event is not supported for FreeBSD");
	return -1;
}
