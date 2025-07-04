/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014 6WIND S.A.
 */

#ifndef _RTE_DEV_H_
#define _RTE_DEV_H_

/**
 * @file
 *
 * RTE PMD Registration Interface
 *
 * This file manages the list of device drivers.
 */

#include <stdio.h>

#include <rte_config.h>
#include <rte_common.h>
#include <rte_log.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rte_bus;
struct rte_devargs;
struct rte_device;
struct rte_driver;

/**
 * The device event type.
 */
enum rte_dev_event_type {
	RTE_DEV_EVENT_ADD,	/**< device being added */
	RTE_DEV_EVENT_REMOVE,	/**< device being removed */
	RTE_DEV_EVENT_MAX	/**< max value of this enum */
};

typedef void (*rte_dev_event_cb_fn)(const char *device_name,
					enum rte_dev_event_type event,
					void *cb_arg);

/**
 * Device policies.
 */
enum rte_dev_policy {
	RTE_DEV_ALLOWED,
	RTE_DEV_BLOCKED,
};

/**
 * A generic memory resource representation.
 */
struct rte_mem_resource {
	uint64_t phys_addr; /**< Physical address, 0 if not resource. */
	uint64_t len;       /**< Length of the resource. */
	void *addr;         /**< Virtual address, NULL when not mapped. */
};

/**
 * Retrieve a driver name.
 *
 * @param driver
 *   A pointer to a driver structure.
 * @return
 *   A pointer to the driver name string.
 */
const char *
rte_driver_name(const struct rte_driver *driver);

/**
 * Retrieve a device bus.
 *
 * @param dev
 *   A pointer to a device structure.
 * @return
 *   A pointer to this device bus.
 */
const struct rte_bus *
rte_dev_bus(const struct rte_device *dev);

/**
 * Retrieve bus specific information for a device.
 *
 * @param dev
 *   A pointer to a device structure.
 * @return
 *   A string describing this device or NULL if none is available.
 */
const char *
rte_dev_bus_info(const struct rte_device *dev);

/**
 * Retrieve a device arguments.
 *
 * @param dev
 *   A pointer to a device structure.
 * @return
 *   A pointer to this device devargs.
 */
const struct rte_devargs *
rte_dev_devargs(const struct rte_device *dev);

/**
 * Retrieve a device driver.
 *
 * @param dev
 *   A pointer to a device structure.
 * @return
 *   A pointer to this device driver.
 */
const struct rte_driver *
rte_dev_driver(const struct rte_device *dev);

/**
 * Retrieve a device name.
 *
 * @param dev
 *   A pointer to a device structure.
 * @return
 *   A pointer to this device name.
 */
const char *
rte_dev_name(const struct rte_device *dev);

/**
 * Retrieve a device numa node.
 *
 * @param dev
 *   A pointer to a device structure.
 * @return
 *   A pointer to this device numa node.
 */
int
rte_dev_numa_node(const struct rte_device *dev);

/*
 * Internal identifier length
 * Sufficiently large to allow for UUID or PCI address
 */
#define RTE_DEV_NAME_MAX_LEN 64

/**
 * Query status of a device.
 *
 * @param dev
 *   Generic device pointer.
 * @return
 *   (int)true if already probed successfully, 0 otherwise.
 */
int rte_dev_is_probed(const struct rte_device *dev);

/**
 * Hotplug add a given device to a specific bus.
 *
 * In multi-process, it will request other processes to add the same device.
 * A failure, in any process, will rollback the action
 *
 * @param busname
 *   The bus name the device is added to.
 * @param devname
 *   The device name. Based on this device name, eal will identify a driver
 *   capable of handling it and pass it to the driver probing function.
 * @param drvargs
 *   Device arguments to be passed to the driver.
 * @return
 *   0 on success, negative on error.
 */
int rte_eal_hotplug_add(const char *busname, const char *devname,
			const char *drvargs);

/**
 * Add matching devices.
 *
 * In multi-process, it will request other processes to add the same device.
 * A failure, in any process, will rollback the action
 *
 * @param devargs
 *   Device arguments including bus, class and driver properties.
 * @return
 *   0 on success, negative on error.
 */
int rte_dev_probe(const char *devargs);

/**
 * Hotplug remove a given device from a specific bus.
 *
 * In multi-process, it will request other processes to remove the same device.
 * A failure, in any process, will rollback the action
 *
 * @param busname
 *   The bus name the device is removed from.
 * @param devname
 *   The device name being removed.
 * @return
 *   0 on success, negative on error.
 */
int rte_eal_hotplug_remove(const char *busname, const char *devname);

/**
 * Remove one device.
 *
 * In multi-process, it will request other processes to remove the same device.
 * A failure, in any process, will rollback the action
 *
 * @param dev
 *   Data structure of the device to remove.
 * @return
 *   0 on success, negative on error.
 */
int rte_dev_remove(struct rte_device *dev);

/**
 * Device comparison function.
 *
 * This type of function is used to compare an rte_device with arbitrary
 * data.
 *
 * @param dev
 *   Device handle.
 *
 * @param data
 *   Data to compare against. The type of this parameter is determined by
 *   the kind of comparison performed by the function.
 *
 * @return
 *   0 if the device matches the data.
 *   !0 if the device does not match.
 *   <0 if ordering is possible and the device is lower than the data.
 *   >0 if ordering is possible and the device is greater than the data.
 */
typedef int (*rte_dev_cmp_t)(const struct rte_device *dev, const void *data);

/**
 * Iteration context.
 *
 * This context carries over the current iteration state.
 */
struct rte_dev_iterator {
	const char *dev_str; /**< device string. */
	const char *bus_str; /**< bus-related part of device string. */
	const char *cls_str; /**< class-related part of device string. */
	struct rte_bus *bus; /**< bus handle. */
	struct rte_class *cls; /**< class handle. */
	struct rte_device *device; /**< current position. */
	void *class_device; /**< additional specialized context. */
};

/**
 * Device iteration function.
 *
 * Find the next device matching properties passed in parameters.
 * The function takes an additional ``start`` parameter, that is
 * used as starting context when relevant.
 *
 * The function returns the current element in the iteration.
 * This return value will potentially be used as a start parameter
 * in subsequent calls to the function.
 *
 * The additional iterator parameter is only there if a specific
 * implementation needs additional context. It must not be modified by
 * the iteration function itself.
 *
 * @param start
 *   Starting iteration context.
 *
 * @param devstr
 *   Device description string.
 *
 * @param it
 *   Device iterator.
 *
 * @return
 *   The address of the current element matching the device description
 *   string.
 */
typedef void *(*rte_dev_iterate_t)(const void *start,
				   const char *devstr,
				   const struct rte_dev_iterator *it);

/**
 * Initializes a device iterator.
 *
 * This iterator allows accessing a list of devices matching a criteria.
 * The device matching is made among all buses and classes currently registered,
 * filtered by the device description given as parameter.
 *
 * This function will not allocate any memory. It is safe to stop the
 * iteration at any moment and let the iterator go out of context.
 *
 * @param it
 *   Device iterator handle.
 *
 * @param str
 *   Device description string.
 *
 * @return
 *   0 on successful initialization.
 *   <0 on error.
 */
int
rte_dev_iterator_init(struct rte_dev_iterator *it, const char *str);

/**
 * Iterates on a device iterator.
 *
 * Generates a new rte_device handle corresponding to the next element
 * in the list described in comprehension by the iterator.
 *
 * The next object is returned, and the iterator is updated.
 *
 * @param it
 *   Device iterator handle.
 *
 * @return
 *   An rte_device handle if found.
 *   NULL if an error occurred (rte_errno is set).
 *   NULL if no device could be found (rte_errno is not set).
 */
struct rte_device *
rte_dev_iterator_next(struct rte_dev_iterator *it);

#define RTE_DEV_FOREACH(dev, devstr, it) \
	for (rte_dev_iterator_init(it, devstr), \
	     dev = rte_dev_iterator_next(it); \
	     dev != NULL; \
	     dev = rte_dev_iterator_next(it))

/**
 * It registers the callback for the specific device.
 * Multiple callbacks can be registered at the same time.
 *
 * @param device_name
 *  The device name, that is the param name of the struct rte_device,
 *  null value means for all devices.
 * @param cb_fn
 *  callback address.
 * @param cb_arg
 *  address of parameter for callback.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
int
rte_dev_event_callback_register(const char *device_name,
				rte_dev_event_cb_fn cb_fn,
				void *cb_arg);

/**
 * It unregisters the callback according to the specified device.
 *
 * @param device_name
 *  The device name, that is the param name of the struct rte_device,
 *  null value means for all devices and their callbacks.
 * @param cb_fn
 *  callback address.
 * @param cb_arg
 *  address of parameter for callback, (void *)-1 means to remove all
 *  registered which has the same callback address.
 *
 * @return
 *  - On success, return the number of callback entities removed.
 *  - On failure, a negative value.
 */
int
rte_dev_event_callback_unregister(const char *device_name,
				  rte_dev_event_cb_fn cb_fn,
				  void *cb_arg);

/**
 * Executes all the user application registered callbacks for
 * the specific device.
 *
 * @param device_name
 *  The device name.
 * @param event
 *  the device event type.
 */
void
rte_dev_event_callback_process(const char *device_name,
			       enum rte_dev_event_type event);

/**
 * Start the device event monitoring.
 *
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
int
rte_dev_event_monitor_start(void);

/**
 * Stop the device event monitoring.
 *
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
int
rte_dev_event_monitor_stop(void);

/**
 * Enable hotplug handling for devices.
 *
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
int
rte_dev_hotplug_handle_enable(void);

/**
 * Disable hotplug handling for devices.
 *
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
int
rte_dev_hotplug_handle_disable(void);

/**
 * Device level DMA map function.
 * After a successful call, the memory segment will be mapped to the
 * given device.
 *
 * @note: Memory must be registered in advance using rte_extmem_* APIs.
 *
 * @param dev
 *	Device pointer.
 * @param addr
 *	Virtual address to map.
 * @param iova
 *	IOVA address to map.
 * @param len
 *	Length of the memory segment being mapped.
 *
 * @return
 *	0 if mapping was successful.
 *	Negative value and rte_errno is set otherwise.
 */
int
rte_dev_dma_map(struct rte_device *dev, void *addr, uint64_t iova, size_t len);

/**
 * Device level DMA unmap function.
 * After a successful call, the memory segment will no longer be
 * accessible by the given device.
 *
 * @note: Memory must be registered in advance using rte_extmem_* APIs.
 *
 * @param dev
 *	Device pointer.
 * @param addr
 *	Virtual address to unmap.
 * @param iova
 *	IOVA address to unmap.
 * @param len
 *	Length of the memory segment being mapped.
 *
 * @return
 *	0 if un-mapping was successful.
 *	Negative value and rte_errno is set otherwise.
 */
int
rte_dev_dma_unmap(struct rte_device *dev, void *addr, uint64_t iova,
		  size_t len);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_DEV_H_ */
