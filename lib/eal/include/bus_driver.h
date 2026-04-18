/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Red Hat, Inc.
 */

#ifndef BUS_DRIVER_H
#define BUS_DRIVER_H

#include <rte_bus.h>
#include <rte_compat.h>
#include <rte_dev.h>
#include <rte_eal.h>
#include <rte_tailq.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rte_devargs;
struct rte_device;

/** Double linked list of buses */
RTE_TAILQ_HEAD(rte_bus_list, rte_bus);

/**
 * Bus specific scan for devices attached on the bus.
 * For each bus object, the scan would be responsible for finding devices and
 * adding them to its private device list.
 *
 * A bus should mandatorily implement this method.
 *
 * @return
 *	0 for successful scan
 *	<0 for unsuccessful scan with error value
 */
typedef int (*rte_bus_scan_t)(void);

/**
 * Implementation specific probe function which is responsible for linking
 * devices on that bus with applicable drivers.
 *
 * This is called while iterating over each registered bus.
 *
 * @return
 *	0 for successful probe
 *	!0 for any error while probing
 */
typedef int (*rte_bus_probe_t)(void);

/**
 * Device iterator to find a device on a bus.
 *
 * This function returns an rte_device if one of those held by the bus
 * matches the data passed as parameter.
 *
 * If the comparison function returns zero this function should stop iterating
 * over any more devices. To continue a search the device of a previous search
 * can be passed via the start parameter.
 *
 * @param cmp
 *	Comparison function.
 *
 * @param data
 *	Data to compare each device against.
 *
 * @param start
 *	starting point for the iteration
 *
 * @return
 *	The first device matching the data, NULL if none exists.
 */
typedef struct rte_device *
(*rte_bus_find_device_t)(const struct rte_bus *bus, const struct rte_device *start,
			 rte_dev_cmp_t cmp, const void *data);

/**
 * Implementation specific probe function which is responsible for linking
 * devices on that bus with applicable drivers.
 *
 * @param dev
 *	Device pointer that was returned by a previous call to find_device.
 *
 * @return
 *	0 on success.
 *	!0 on error.
 */
typedef int (*rte_bus_plug_t)(struct rte_device *dev);

/**
 * Implementation specific remove function which is responsible for unlinking
 * devices on that bus from assigned driver.
 *
 * @param dev
 *	Device pointer that was returned by a previous call to find_device.
 *
 * @return
 *	0 on success.
 *	!0 on error.
 */
typedef int (*rte_bus_unplug_t)(struct rte_device *dev);

/**
 * Bus specific parsing function.
 * Validates the syntax used in the textual representation of a device,
 * If the syntax is valid and ``addr`` is not NULL, writes the bus-specific
 * device representation to ``addr``.
 *
 * @param[in] name
 *	device textual description
 *
 * @param[out] addr
 *	device information location address, into which parsed info
 *	should be written. If NULL, nothing should be written, which
 *	is not an error.
 *
 * @return
 *	0 if parsing was successful.
 *	!0 for any error.
 */
typedef int (*rte_bus_parse_t)(const char *name, void *addr);

/**
 * Bus specific device name comparison function.
 * Bus can normalize the names of devices using an internal representation.
 * This helper makes it possible to check whether two names refer to the same device.
 *
 * @param[in] name1
 *	device name
 * @param[in] name2
 *	device name
 *
 * @return
 *      0 if the two names refer to the same device.
 *      !0 otherwise.
 */
typedef int (*rte_bus_dev_compare_t)(const char *name1, const char *name2);

/**
 * Parse bus part of the device arguments.
 *
 * The field name of the struct rte_devargs will be set.
 *
 * @param da
 *	Pointer to the devargs to parse.
 *
 * @return
 *	0 on successful parsing, otherwise rte_errno is set.
 *	-EINVAL: on parsing error.
 *	-ENODEV: if no key matching a device argument is specified.
 *	-E2BIG: device name is too long.
 */
typedef int (*rte_bus_devargs_parse_t)(struct rte_devargs *da);

/**
 * Device level DMA map function.
 * After a successful call, the memory segment will be mapped to the
 * given device.
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
typedef int (*rte_dev_dma_map_t)(struct rte_device *dev, void *addr,
				  uint64_t iova, size_t len);

/**
 * Device level DMA unmap function.
 * After a successful call, the memory segment will no longer be
 * accessible by the given device.
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
typedef int (*rte_dev_dma_unmap_t)(struct rte_device *dev, void *addr,
				   uint64_t iova, size_t len);

/**
 * Implement a specific hot-unplug handler, which is responsible for
 * handle the failure when device be hot-unplugged. When the event of
 * hot-unplug be detected, it could call this function to handle
 * the hot-unplug failure and avoid app crash.
 * @param dev
 *	Pointer of the device structure.
 *
 * @return
 *	0 on success.
 *	!0 on error.
 */
typedef int (*rte_bus_hot_unplug_handler_t)(struct rte_device *dev);

/**
 * Implement a specific sigbus handler, which is responsible for handling
 * the sigbus error which is either original memory error, or specific memory
 * error that caused of device be hot-unplugged. When sigbus error be captured,
 * it could call this function to handle sigbus error.
 * @param failure_addr
 *	Pointer of the fault address of the sigbus error.
 *
 * @return
 *	0 for success handle the sigbus for hot-unplug.
 *	1 for not process it, because it is a generic sigbus error.
 *	-1 for failed to handle the sigbus for hot-unplug.
 */
typedef int (*rte_bus_sigbus_handler_t)(const void *failure_addr);

/**
 * Implementation specific cleanup function which is responsible for cleaning up
 * devices on that bus with applicable drivers.
 *
 * This is called while iterating over each registered bus.
 *
 * @return
 * 0 for successful cleanup
 * !0 for any error during cleanup
 */
typedef int (*rte_bus_cleanup_t)(void);

/**
 * Check if a driver matches a device.
 *
 * This function checks whether a driver can handle a given device.
 * Matching logic is bus-specific (e.g., PCI uses ID tables, vdev uses
 * name matching, fslmc uses device type).
 *
 * @param drv
 *	Driver to check.
 * @param dev
 *	Device to check against.
 *
 * @return
 *	true if the driver matches the device, false otherwise.
 */
typedef bool (*rte_bus_match_t)(const struct rte_driver *drv,
				 const struct rte_device *dev);

/**
 * Bus scan policies
 */
enum rte_bus_scan_mode {
	RTE_BUS_SCAN_UNDEFINED,
	RTE_BUS_SCAN_ALLOWLIST,
	RTE_BUS_SCAN_BLOCKLIST,
};

/**
 * A structure used to configure bus operations.
 */
struct rte_bus_conf {
	enum rte_bus_scan_mode scan_mode; /**< Scan policy. */
};


/**
 * Get common iommu class of the all the devices on the bus. The bus may
 * check that those devices are attached to iommu driver.
 * If no devices are attached to the bus. The bus may return with don't care
 * (_DC) value.
 * Otherwise, The bus will return appropriate _pa or _va iova mode.
 *
 * @return
 *      enum rte_iova_mode value.
 */
typedef enum rte_iova_mode (*rte_bus_get_iommu_class_t)(void);

/**
 * Per bus, device iteration function.
 *
 * Similar to rte_dev_iterate_t but also pass along the bus pointer.
 *
 * @param bus
 *   A pointer to the bus structure.
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
typedef void *(*rte_bus_dev_iterate_t)(const struct rte_bus *bus,
				       const void *start,
				       const char *devstr,
				       const struct rte_dev_iterator *it);

/**
 * A structure describing a generic bus.
 */
struct rte_bus {
	RTE_TAILQ_ENTRY(rte_bus) next; /**< Next bus object in linked list */
	const char *name;            /**< Name of the bus */
	rte_bus_scan_t scan;         /**< Scan for devices attached to bus */
	rte_bus_probe_t probe;       /**< Probe devices on bus */
	rte_bus_find_device_t find_device; /**< Find a device on the bus */
	rte_bus_match_t match;       /**< Check if driver matches device */
	rte_bus_plug_t plug;         /**< Probe single device for drivers */
	rte_bus_unplug_t unplug;     /**< Remove single device from driver */
	rte_bus_parse_t parse;       /**< Parse a device name */
	rte_bus_dev_compare_t dev_compare; /**< Compare two device names */
	rte_bus_devargs_parse_t devargs_parse; /**< Parse bus devargs */
	rte_dev_dma_map_t dma_map;   /**< DMA map for device in the bus */
	rte_dev_dma_unmap_t dma_unmap; /**< DMA unmap for device in the bus */
	struct rte_bus_conf conf;    /**< Bus configuration */
	rte_bus_get_iommu_class_t get_iommu_class; /**< Get iommu class */
	rte_bus_dev_iterate_t dev_iterate; /**< Bus device iterator. */
	rte_bus_hot_unplug_handler_t hot_unplug_handler;
				/**< handle hot-unplug failure on the bus */
	rte_bus_sigbus_handler_t sigbus_handler;
					/**< handle sigbus error on the bus */
	rte_bus_cleanup_t cleanup;   /**< Cleanup devices on bus */
	RTE_TAILQ_HEAD(, rte_device) device_list; /**< List of devices on the bus */
	RTE_TAILQ_HEAD(, rte_driver) driver_list; /**< List of drivers on the bus */
};

/**
 * Register a Bus handler.
 *
 * @param bus
 *   A pointer to a rte_bus structure describing the bus
 *   to be registered.
 */
__rte_internal
void rte_bus_register(struct rte_bus *bus);

/**
 * Find the devargs associated to a device.
 *
 * @param bus
 *   A pointer to a rte_bus structure describing the bus
 *   to search for devargs on.
 * @param dev_name
 *   A device name.
 *
 * @return
 *   Pointer to the device devargs, or NULL if none found.
 */
__rte_internal
struct rte_devargs *rte_bus_find_devargs(const struct rte_bus *bus, const char *dev_name);

/**
 * Indicate if a device should be skipped during probing of a bus.
 */
__rte_internal
bool rte_bus_device_is_ignored(const struct rte_bus *bus, const char *dev_name);

/**
 * Generic device iterator for buses using name-based matching.
 *
 * This helper implements the standard name-based device iteration pattern
 * using kvargs parsing. Buses that only support "name" parameter matching
 * can use this instead of implementing their own dev_iterate function.
 *
 * @param bus
 *   A pointer to the bus structure.
 * @param start
 *   The starting device (NULL to start from the beginning).
 * @param devstr
 *   The device filter string (e.g., "name=eth0").
 * @param it
 *   Device iterator.
 *
 * @return
 *   Pointer to the matching device, or NULL if not found.
 */
__rte_internal
void *rte_bus_generic_dev_iterate(const struct rte_bus *bus,
				   const void *start,
				   const char *devstr,
				   const struct rte_dev_iterator *it);

/**
 * Helper for Bus registration.
 * The constructor has higher priority than PMD constructors.
 */
#define RTE_REGISTER_BUS(nm, bus) \
RTE_INIT_PRIO(businitfn_ ##nm, BUS) \
{\
	(bus).name = RTE_STR(nm);\
	rte_bus_register(&bus); \
}

/**
 * Unregister a Bus handler.
 *
 * @param bus
 *   A pointer to a rte_bus structure describing the bus
 *   to be unregistered.
 */
__rte_internal
void rte_bus_unregister(struct rte_bus *bus);

/**
 * Helper macro to convert a generic device pointer to a bus-specific device type.
 * Uses typeof to automatically determine the bus-specific type from the second argument.
 *
 * @param dev
 *   Generic rte_device pointer to convert
 * @param bus_dev_type
 *   Type specifier: either a struct type (e.g., struct rte_pci_device) or
 *   a dereferenced pointer variable (e.g., *pdev) to infer the type automatically
 * @return
 *   Pointer to the bus-specific device structure containing this rte_device
 */
#define RTE_BUS_DEVICE(dev, bus_dev_type) \
	container_of(dev, typeof(bus_dev_type), device)

/**
 * Helper macro to convert a generic driver pointer to a bus-specific driver type.
 * Uses typeof to automatically determine the bus-specific type from the second argument.
 *
 * @param drv
 *   Generic rte_driver pointer to convert
 * @param bus_drv_type
 *   Type specifier: either a struct type (e.g., struct rte_pci_driver) or
 *   a dereferenced pointer variable (e.g., *pdrv) to infer the type automatically
 * @return
 *   Pointer to the bus-specific driver structure containing this rte_driver
 */
#define RTE_BUS_DRIVER(drv, bus_drv_type) \
	container_of(drv, typeof(bus_drv_type), driver)

/**
 * Helper macro to iterate over all devices on a bus.
 *
 * @param dev
 *   Variable name for the bus-specific device pointer.
 * @param bus
 *   Pointer to the bus structure.
 *
 * Example:
 *   struct rte_pci_device *pci_dev;
 *   RTE_BUS_FOREACH_DEV(pci_dev, &pci_bus.bus) {
 *       // Use pci_dev here
 *   }
 */
#define RTE_BUS_FOREACH_DEV(dev, bus) \
	for (struct rte_device *__rte_dev = TAILQ_FIRST(&(bus)->device_list), *__rte_dev_tmp; \
			(__rte_dev != NULL && ((dev) = RTE_BUS_DEVICE(__rte_dev, *dev), \
				__rte_dev_tmp = TAILQ_NEXT(__rte_dev, next), 1)) || \
			(dev = NULL, 0); \
			__rte_dev = __rte_dev_tmp)

/**
 * Add a device to the bus device list.
 *
 * @param bus
 *   A pointer to a rte_bus structure.
 * @param dev
 *   A pointer to a rte_device structure to add.
 */
__rte_internal
void rte_bus_add_device(struct rte_bus *bus, struct rte_device *dev);

/**
 * Remove a device from the bus device list.
 *
 * @param bus
 *   A pointer to a rte_bus structure.
 * @param dev
 *   A pointer to a rte_device structure to remove.
 */
__rte_internal
void rte_bus_remove_device(struct rte_bus *bus, struct rte_device *dev);

/**
 * Insert a device before another in the bus device list.
 *
 * @param bus
 *   A pointer to a rte_bus structure.
 * @param exist_dev
 *   Existing device in the list.
 * @param new_dev
 *   New device to insert before exist_dev.
 */
__rte_internal
void rte_bus_insert_device(struct rte_bus *bus,
			   struct rte_device *exist_dev,
			   struct rte_device *new_dev);

/**
 * Find a device on a bus.
 *
 * @param bus
 *   A pointer to a rte_bus structure.
 * @param start
 *   Starting point for the search. If NULL, search from the beginning.
 * @param cmp
 *   Comparison function to match devices.
 * @param data
 *   Data to pass to the comparison function.
 * @return
 *   The first matching device, or NULL if not found.
 */
__rte_internal
struct rte_device *rte_bus_generic_find_device(const struct rte_bus *bus,
					       const struct rte_device *start,
					       rte_dev_cmp_t cmp,
					       const void *data);

/**
 * Helper macro to iterate over all drivers on a bus.
 *
 * @param drv
 *   Variable name for the bus-specific driver pointer.
 * @param bus
 *   Pointer to the bus structure.
 *
 * Example:
 *   struct rte_pci_driver *pci_drv;
 *   RTE_BUS_FOREACH_DRV(pci_drv, &pci_bus.bus) {
 *       // Use pci_drv here
 *   }
 */
#define RTE_BUS_FOREACH_DRV(drv, bus) \
	for (struct rte_driver *__rte_drv = TAILQ_FIRST(&(bus)->driver_list), *__rte_drv_tmp; \
			(__rte_drv != NULL && ((drv) = RTE_BUS_DRIVER(__rte_drv, *drv), \
				__rte_drv_tmp = TAILQ_NEXT(__rte_drv, next), 1)) || \
			(drv = NULL, 0); \
			__rte_drv = __rte_drv_tmp)

/**
 * Add a driver to the bus driver list.
 *
 * @param bus
 *   A pointer to a rte_bus structure.
 * @param driver
 *   A pointer to a rte_driver structure to add.
 */
__rte_internal
void rte_bus_add_driver(struct rte_bus *bus, struct rte_driver *driver);

/**
 * Remove a driver from the bus driver list.
 *
 * @param bus
 *   A pointer to a rte_bus structure.
 * @param driver
 *   A pointer to a rte_driver structure to remove.
 */
__rte_internal
void rte_bus_remove_driver(struct rte_bus *bus, struct rte_driver *driver);

/**
 * Find the first driver that matches a device on a bus.
 *
 * Iterates through all registered drivers on the bus and returns the next
 * one that matches the given device according to the bus's match operation.
 *
 * @param bus
 *   A pointer to a rte_bus structure.
 * @param start
 *   Starting iteration context.
 * @param dev
 *   A pointer to a rte_device structure.
 * @return
 *   Pointer to the matching driver, or NULL if no match found.
 */
__rte_internal
struct rte_driver *rte_bus_find_driver(const struct rte_bus *bus, const struct rte_driver *start,
	const struct rte_device *dev);

#ifdef __cplusplus
}
#endif

#endif /* BUS_DRIVER_H */
