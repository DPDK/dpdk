/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014 6WIND S.A.
 */

#ifndef _RTE_DEV_H_
#define _RTE_DEV_H_

/**
 * @file
 *
 * RTE PMD Driver Registration Interface
 *
 * This file manages the list of device drivers.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/queue.h>

#include <rte_config.h>
#include <rte_compat.h>
#include <rte_log.h>

/**
 * The device event type.
 */
enum rte_dev_event_type {
	RTE_DEV_EVENT_ADD,	/**< device being added */
	RTE_DEV_EVENT_REMOVE,	/**< device being removed */
	RTE_DEV_EVENT_MAX	/**< max value of this enum */
};

struct rte_dev_event {
	enum rte_dev_event_type type;	/**< device event type */
	int subsystem;			/**< subsystem id */
	char *devname;			/**< device name */
};

typedef void (*rte_dev_event_cb_fn)(char *device_name,
					enum rte_dev_event_type event,
					void *cb_arg);

__attribute__((format(printf, 2, 0)))
static inline void
rte_pmd_debug_trace(const char *func_name, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	{
		char buffer[vsnprintf(NULL, 0, fmt, ap) + 1];

		va_end(ap);

		va_start(ap, fmt);
		vsnprintf(buffer, sizeof(buffer), fmt, ap);
		va_end(ap);

		rte_log(RTE_LOG_ERR, RTE_LOGTYPE_PMD, "%s: %s",
			func_name, buffer);
	}
}

/*
 * Enable RTE_PMD_DEBUG_TRACE() when at least one component relying on the
 * RTE_*_RET() macros defined below is compiled in debug mode.
 */
#if defined(RTE_LIBRTE_ETHDEV_DEBUG) || \
	defined(RTE_LIBRTE_CRYPTODEV_DEBUG) || \
	defined(RTE_LIBRTE_EVENTDEV_DEBUG)
#define RTE_PMD_DEBUG_TRACE(...) \
	rte_pmd_debug_trace(__func__, __VA_ARGS__)
#else
#define RTE_PMD_DEBUG_TRACE(...) (void)0
#endif

/* Macros for checking for restricting functions to primary instance only */
#define RTE_PROC_PRIMARY_OR_ERR_RET(retval) do { \
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) { \
		RTE_PMD_DEBUG_TRACE("Cannot run in secondary processes\n"); \
		return retval; \
	} \
} while (0)

#define RTE_PROC_PRIMARY_OR_RET() do { \
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) { \
		RTE_PMD_DEBUG_TRACE("Cannot run in secondary processes\n"); \
		return; \
	} \
} while (0)

/* Macros to check for invalid function pointers */
#define RTE_FUNC_PTR_OR_ERR_RET(func, retval) do { \
	if ((func) == NULL) { \
		RTE_PMD_DEBUG_TRACE("Function not supported\n"); \
		return retval; \
	} \
} while (0)

#define RTE_FUNC_PTR_OR_RET(func) do { \
	if ((func) == NULL) { \
		RTE_PMD_DEBUG_TRACE("Function not supported\n"); \
		return; \
	} \
} while (0)

/**
 * Device driver.
 */
enum rte_kernel_driver {
	RTE_KDRV_UNKNOWN = 0,
	RTE_KDRV_IGB_UIO,
	RTE_KDRV_VFIO,
	RTE_KDRV_UIO_GENERIC,
	RTE_KDRV_NIC_UIO,
	RTE_KDRV_NONE,
};

/**
 * Device policies.
 */
enum rte_dev_policy {
	RTE_DEV_WHITELISTED,
	RTE_DEV_BLACKLISTED,
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
 * A structure describing a device driver.
 */
struct rte_driver {
	TAILQ_ENTRY(rte_driver) next;  /**< Next in list. */
	const char *name;                   /**< Driver name. */
	const char *alias;              /**< Driver alias. */
};

/*
 * Internal identifier length
 * Sufficiently large to allow for UUID or PCI address
 */
#define RTE_DEV_NAME_MAX_LEN 64

/**
 * A structure describing a generic device.
 */
struct rte_device {
	TAILQ_ENTRY(rte_device) next; /**< Next device */
	const char *name;             /**< Device name */
	const struct rte_driver *driver;/**< Associated driver */
	int numa_node;                /**< NUMA node connection */
	struct rte_devargs *devargs;  /**< Device user arguments */
};

/**
 * Attach a device to a registered driver.
 *
 * @param name
 *   The device name, that refers to a pci device (or some private
 *   way of designating a vdev device). Based on this device name, eal
 *   will identify a driver capable of handling it and pass it to the
 *   driver probing function.
 * @param devargs
 *   Device arguments to be passed to the driver.
 * @return
 *   0 on success, negative on error.
 */
int rte_eal_dev_attach(const char *name, const char *devargs);

/**
 * Detach a device from its driver.
 *
 * @param dev
 *   A pointer to a rte_device structure.
 * @return
 *   0 on success, negative on error.
 */
int rte_eal_dev_detach(struct rte_device *dev);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Hotplug add a given device to a specific bus.
 *
 * @param busname
 *   The bus name the device is added to.
 * @param devname
 *   The device name. Based on this device name, eal will identify a driver
 *   capable of handling it and pass it to the driver probing function.
 * @param devargs
 *   Device arguments to be passed to the driver.
 * @return
 *   0 on success, negative on error.
 */
int __rte_experimental rte_eal_hotplug_add(const char *busname, const char *devname,
			const char *devargs);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Hotplug remove a given device from a specific bus.
 *
 * @param busname
 *   The bus name the device is removed from.
 * @param devname
 *   The device name being removed.
 * @return
 *   0 on success, negative on error.
 */
int __rte_experimental rte_eal_hotplug_remove(const char *busname,
					  const char *devname);

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

#define RTE_PMD_EXPORT_NAME_ARRAY(n, idx) n##idx[]

#define RTE_PMD_EXPORT_NAME(name, idx) \
static const char RTE_PMD_EXPORT_NAME_ARRAY(this_pmd_name, idx) \
__attribute__((used)) = RTE_STR(name)

#define DRV_EXP_TAG(name, tag) __##name##_##tag

#define RTE_PMD_REGISTER_PCI_TABLE(name, table) \
static const char DRV_EXP_TAG(name, pci_tbl_export)[] __attribute__((used)) = \
RTE_STR(table)

#define RTE_PMD_REGISTER_PARAM_STRING(name, str) \
static const char DRV_EXP_TAG(name, param_string_export)[] \
__attribute__((used)) = str

/**
 * Advertise the list of kernel modules required to run this driver
 *
 * This string lists the kernel modules required for the devices
 * associated to a PMD. The format of each line of the string is:
 * "<device-pattern> <kmod-expression>".
 *
 * The possible formats for the device pattern are:
 *   "*"                     all devices supported by this driver
 *   "pci:*"                 all PCI devices supported by this driver
 *   "pci:v8086:d*:sv*:sd*"  all PCI devices supported by this driver
 *                           whose vendor id is 0x8086.
 *
 * The format of the kernel modules list is a parenthesed expression
 * containing logical-and (&) and logical-or (|).
 *
 * The device pattern and the kmod expression are separated by a space.
 *
 * Example:
 * - "* igb_uio | uio_pci_generic | vfio"
 */
#define RTE_PMD_REGISTER_KMOD_DEP(name, str) \
static const char DRV_EXP_TAG(name, kmod_dep_export)[] \
__attribute__((used)) = str

#ifdef __cplusplus
}
#endif

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * It registers the callback for the specific device.
 * Multiple callbacks cal be registered at the same time.
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
int __rte_experimental
rte_dev_event_callback_register(const char *device_name,
				rte_dev_event_cb_fn cb_fn,
				void *cb_arg);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
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
int __rte_experimental
rte_dev_event_callback_unregister(const char *device_name,
				  rte_dev_event_cb_fn cb_fn,
				  void *cb_arg);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Start the device event monitoring.
 *
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
int __rte_experimental
rte_dev_event_monitor_start(void);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Stop the device event monitoring.
 *
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
int __rte_experimental
rte_dev_event_monitor_stop(void);

#endif /* _RTE_DEV_H_ */
