/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Cavium, Inc
 */

#ifndef _RTE_EVENTDEV_PMD_PCI_H_
#define _RTE_EVENTDEV_PMD_PCI_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @file
 * RTE Eventdev PCI PMD APIs
 *
 * @note
 * These API are from event PCI PMD only and user applications should not call
 * them directly.
 */

#include <string.h>

#include <rte_compat.h>
#include <rte_config.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_pci.h>
#include <bus_pci_driver.h>

#include "eventdev_pmd.h"

typedef int (*eventdev_pmd_pci_callback_t)(struct rte_eventdev *dev);

/**
 * @internal
 * Wrapper for use by pci drivers as a .probe function to attach to an event
 * interface.  Same as rte_event_pmd_pci_probe, except caller can specify
 * the name.
 */
__rte_internal
static inline int
rte_event_pmd_pci_probe_named(struct rte_pci_driver *pci_drv,
			      struct rte_pci_device *pci_dev,
			      size_t private_data_size,
			      eventdev_pmd_pci_callback_t devinit,
			      const char *name)
{
	struct rte_eventdev *eventdev;
	int retval;

	if (devinit == NULL)
		return -EINVAL;

	eventdev = rte_event_pmd_allocate(name,
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
	if (retval == 0) {
		event_dev_probing_finish(eventdev);
		return 0;
	}

	RTE_EDEV_LOG_ERR("driver %s: (vendor_id=0x%x device_id=0x%x)"
			" failed", pci_drv->driver.name,
			(unsigned int) pci_dev->id.vendor_id,
			(unsigned int) pci_dev->id.device_id);

	rte_event_pmd_release(eventdev);

	return -ENXIO;
}

/**
 * @internal
 * Wrapper for use by pci drivers as a .probe function to attach to a event
 * interface.
 */
__rte_internal
static inline int
rte_event_pmd_pci_probe(struct rte_pci_driver *pci_drv,
			    struct rte_pci_device *pci_dev,
			    size_t private_data_size,
			    eventdev_pmd_pci_callback_t devinit)
{
	char eventdev_name[RTE_EVENTDEV_NAME_MAX_LEN];

	rte_pci_device_name(&pci_dev->addr, eventdev_name,
			sizeof(eventdev_name));

	return rte_event_pmd_pci_probe_named(pci_drv,
					     pci_dev,
					     private_data_size,
					     devinit,
					     eventdev_name);
}

/**
 * @internal
 * Wrapper for use by pci drivers as a .remove function to detach a event
 * interface.
 */
__rte_internal
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

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		ret = rte_event_dev_close(eventdev->data->dev_id);
		if (ret < 0)
			return ret;
	}

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
