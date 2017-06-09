/*
 *
 *   Copyright(c) 2016-2017 Cavium networks. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Cavium networks nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RTE_EVENTDEV_PMD_VDEV_H_
#define _RTE_EVENTDEV_PMD_VDEV_H_

/** @file
 * RTE Eventdev VDEV PMD APIs
 *
 * @note
 * These API are from event VDEV PMD only and user applications should not call
 * them directly.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_vdev.h>

#include "rte_eventdev_pmd.h"

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
static inline struct rte_eventdev *
rte_event_pmd_vdev_init(const char *name, size_t dev_private_size,
		int socket_id)
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
