/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Microsoft Corporation
 */

#include <rte_dev.h>

#include "eal_private.h"

int
rte_dev_event_monitor_start(void)
{
	EAL_LOG(ERR, "Device event is not supported for Windows");
	return -1;
}

int
rte_dev_event_monitor_stop(void)
{
	EAL_LOG(ERR, "Device event is not supported for Windows");
	return -1;
}

int
rte_dev_hotplug_handle_enable(void)
{
	EAL_LOG(ERR, "Device event is not supported for Windows");
	return -1;
}

int
rte_dev_hotplug_handle_disable(void)
{
	EAL_LOG(ERR, "Device event is not supported for Windows");
	return -1;
}
