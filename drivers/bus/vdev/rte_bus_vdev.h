/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 RehiveTech. All rights reserved.
 */

#ifndef RTE_VDEV_H
#define RTE_VDEV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/queue.h>
#include <rte_dev.h>
#include <rte_devargs.h>

struct rte_vdev_device {
	TAILQ_ENTRY(rte_vdev_device) next;      /**< Next attached vdev */
	struct rte_device device;               /**< Inherit core device */
};

/**
 * @internal
 * Helper macro for drivers that need to convert to struct rte_vdev_device.
 */
#define RTE_DEV_TO_VDEV(ptr) \
	container_of(ptr, struct rte_vdev_device, device)

#define RTE_DEV_TO_VDEV_CONST(ptr) \
	container_of(ptr, const struct rte_vdev_device, device)

static inline const char *
rte_vdev_device_name(const struct rte_vdev_device *dev)
{
	if (dev && dev->device.name)
		return dev->device.name;
	return NULL;
}

static inline const char *
rte_vdev_device_args(const struct rte_vdev_device *dev)
{
	if (dev && dev->device.devargs)
		return dev->device.devargs->args;
	return "";
}

/** Double linked list of virtual device drivers. */
TAILQ_HEAD(vdev_driver_list, rte_vdev_driver);

/**
 * Probe function called for each virtual device driver once.
 */
typedef int (rte_vdev_probe_t)(struct rte_vdev_device *dev);

/**
 * Remove function called for each virtual device driver once.
 */
typedef int (rte_vdev_remove_t)(struct rte_vdev_device *dev);

/**
 * A virtual device driver abstraction.
 */
struct rte_vdev_driver {
	TAILQ_ENTRY(rte_vdev_driver) next; /**< Next in list. */
	struct rte_driver driver;      /**< Inherited general driver. */
	rte_vdev_probe_t *probe;       /**< Virtual device probe function. */
	rte_vdev_remove_t *remove;     /**< Virtual device remove function. */
};

/**
 * Register a virtual device driver.
 *
 * @param driver
 *   A pointer to a rte_vdev_driver structure describing the driver
 *   to be registered.
 */
void rte_vdev_register(struct rte_vdev_driver *driver);

/**
 * Unregister a virtual device driver.
 *
 * @param driver
 *   A pointer to a rte_vdev_driver structure describing the driver
 *   to be unregistered.
 */
void rte_vdev_unregister(struct rte_vdev_driver *driver);

#define RTE_PMD_REGISTER_VDEV(nm, vdrv)\
RTE_INIT(vdrvinitfn_ ##vdrv);\
static const char *vdrvinit_ ## nm ## _alias;\
static void vdrvinitfn_ ##vdrv(void)\
{\
	(vdrv).driver.name = RTE_STR(nm);\
	(vdrv).driver.alias = vdrvinit_ ## nm ## _alias;\
	rte_vdev_register(&vdrv);\
} \
RTE_PMD_EXPORT_NAME(nm, __COUNTER__)

#define RTE_PMD_REGISTER_ALIAS(nm, alias)\
static const char *vdrvinit_ ## nm ## _alias = RTE_STR(alias)

typedef void (*rte_vdev_scan_callback)(void *user_arg);

/**
 * Add a callback to be called on vdev scan
 * before reading the devargs list.
 *
 * This function cannot be called in a scan callback
 * because of deadlock.
 *
 * @param callback
 *   The function to be called which can update the devargs list.
 * @param user_arg
 *   An opaque pointer passed to callback.
 * @return
 *   0 on success, negative on error
 */
int
rte_vdev_add_custom_scan(rte_vdev_scan_callback callback, void *user_arg);

/**
 * Remove a registered scan callback.
 *
 * This function cannot be called in a scan callback
 * because of deadlock.
 *
 * @param callback
 *   The registered function to be removed.
 * @param user_arg
 *   The associated opaque pointer or (void*)-1 for any.
 * @return
 *   0 on success
 */
int
rte_vdev_remove_custom_scan(rte_vdev_scan_callback callback, void *user_arg);

/**
 * Initialize a driver specified by name.
 *
 * @param name
 *   The pointer to a driver name to be initialized.
 * @param args
 *   The pointer to arguments used by driver initialization.
 * @return
 *  0 on success, negative on error
 */
int rte_vdev_init(const char *name, const char *args);

/**
 * Uninitalize a driver specified by name.
 *
 * @param name
 *   The pointer to a driver name to be initialized.
 * @return
 *  0 on success, negative on error
 */
int rte_vdev_uninit(const char *name);

#ifdef __cplusplus
}
#endif

#endif
