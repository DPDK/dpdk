/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Cavium, Inc
 */

#ifndef _RTE_EVENTDEV_PMD_VDEV_H_
#define _RTE_EVENTDEV_PMD_VDEV_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @file
 * RTE Eventdev VDEV PMD APIs
 *
 * @note
 * These API are from event VDEV PMD only and user applications should not call
 * them directly.
 */

#include <string.h>

#include <rte_compat.h>
#include <rte_config.h>
#include <rte_debug.h>
#include <rte_eal.h>
#include <bus_vdev_driver.h>

#include "eventdev_pmd.h"

/**
 * @internal
 * Creates a new virtual event device and returns the pointer to that device.
 *
 * @param name
 *   PMD type name
 * @param dev_private_size
 *   Size of event PMDs private data
 * @param socket_id
 *   Socket to allocate resources on.
 *
 * @return
 *   - Eventdev pointer if device is successfully created.
 *   - NULL if device cannot be created.
 */
__rte_internal
static inline struct rte_eventdev *
rte_event_pmd_vdev_init(const char *name, size_t dev_private_size,
		int socket_id, struct rte_vdev_device *vdev)
{

	struct rte_eventdev *eventdev;

	/* Allocate device structure */
	eventdev = rte_event_pmd_allocate(name, socket_id);
	if (eventdev == NULL)
		return NULL;

	/* Allocate private device structure */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		eventdev->data->dev_private =
				rte_zmalloc_socket("eventdev device private",
						dev_private_size,
						RTE_CACHE_LINE_SIZE,
						socket_id);

		if (eventdev->data->dev_private == NULL)
			rte_panic("Cannot allocate memzone for private device"
					" data");
	}
	eventdev->dev = &vdev->device;

	return eventdev;
}

/**
 * @internal
 * Destroy the given virtual event device
 *
 * @param name
 *   PMD type name
 * @return
 *   - 0 on success, negative on error
 */
__rte_internal
static inline int
rte_event_pmd_vdev_uninit(const char *name)
{
	int ret;
	struct rte_eventdev *eventdev;

	if (name == NULL)
		return -EINVAL;

	eventdev = rte_event_pmd_get_named_dev(name);
	if (eventdev == NULL)
		return -ENODEV;

	ret = rte_event_dev_close(eventdev->data->dev_id);
	if (ret < 0)
		return ret;

	/* Free the event device */
	rte_event_pmd_release(eventdev);

	return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_EVENTDEV_PMD_VDEV_H_ */
