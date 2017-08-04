/*
 *
 *   Copyright(c) 2016-2017 Cavium, Inc. All rights reserved.
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
 *     * Neither the name of Cavium, Inc nor the names of its
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

#ifndef _RTE_EVENTDEV_PMD_PCI_H_
#define _RTE_EVENTDEV_PMD_PCI_H_

/** @file
 * RTE Eventdev PCI PMD APIs
 *
 * @note
 * These API are from event PCI PMD only and user applications should not call
 * them directly.
 */


#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_pci.h>

#include "rte_eventdev_pmd.h"

typedef int (*eventdev_pmd_pci_callback_t)(struct rte_eventdev *dev);

/**
 * @internal
 * Wrapper for use by pci drivers as a .probe function to attach to a event
 * interface.
 */
static inline int
rte_event_pmd_pci_probe(struct rte_pci_driver *pci_drv,
			    struct rte_pci_device *pci_dev,
			    size_t private_data_size,
			    eventdev_pmd_pci_callback_t devinit)
{
	struct rte_eventdev *eventdev;

	char eventdev_name[RTE_EVENTDEV_NAME_MAX_LEN];

	int retval;

	if (devinit == NULL)
		return -EINVAL;

	rte_pci_device_name(&pci_dev->addr, eventdev_name,
			sizeof(eventdev_name));

	eventdev = rte_event_pmd_allocate(eventdev_name,
			 pci_dev->device.numa_node);
	if (eventdev == NULL)
		return -ENOMEM;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		eventdev->data->dev_private =
				rte_zmalloc_socket(
						"eventdev private structure",
						private_data_size,
						RTE_CACHE_LINE_SIZE,
						rte_socket_id());

		if (eventdev->data->dev_private == NULL)
			rte_panic("Cannot allocate memzone for private "
					"device data");
	}

	eventdev->dev = &pci_dev->device;

	/* Invoke PMD device initialization function */
	retval = devinit(eventdev);
	if (retval == 0)
		return 0;

	RTE_EDEV_LOG_ERR("driver %s: (vendor_id=0x%x device_id=0x%x)"
			" failed", pci_drv->driver.name,
			(unsigned int) pci_dev->id.vendor_id,
			(unsigned int) pci_dev->id.device_id);

	rte_event_pmd_release(eventdev);

	return -ENXIO;
}


/**
 * @internal
 * Wrapper for use by pci drivers as a .remove function to detach a event
 * interface.
 */
static inline int
rte_event_pmd_pci_remove(struct rte_pci_device *pci_dev,
			     eventdev_pmd_pci_callback_t devuninit)
{
	struct rte_eventdev *eventdev;
	char eventdev_name[RTE_EVENTDEV_NAME_MAX_LEN];
	int ret = 0;

	if (pci_dev == NULL)
		return -EINVAL;

	rte_pci_device_name(&pci_dev->addr, eventdev_name,
			sizeof(eventdev_name));

	eventdev = rte_event_pmd_get_named_dev(eventdev_name);
	if (eventdev == NULL)
		return -ENODEV;

	ret = rte_event_dev_close(eventdev->data->dev_id);
	if (ret < 0)
		return ret;

	/* Invoke PMD device un-init function */
	if (devuninit)
		ret = devuninit(eventdev);
	if (ret)
		return ret;

	/* Free event device */
	rte_event_pmd_release(eventdev);

	eventdev->dev = NULL;

	return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_EVENTDEV_PMD_PCI_H_ */
