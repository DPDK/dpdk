/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2014 6WIND S.A.
 *   All rights reserved.
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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

#include <rte_log.h>

__attribute__((format(printf, 2, 0)))
static inline void
rte_pmd_debug_trace(const char *func_name, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	char buffer[vsnprintf(NULL, 0, fmt, ap) + 1];

	va_end(ap);

	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);

	rte_log(RTE_LOG_ERR, RTE_LOGTYPE_PMD, "%s: %s", func_name, buffer);
}

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
 * A generic memory resource representation.
 */
struct rte_mem_resource {
	uint64_t phys_addr; /**< Physical address, 0 if not resource. */
	uint64_t len;       /**< Length of the resource. */
	void *addr;         /**< Virtual address, NULL when not mapped. */
};

/** Double linked list of device drivers. */
TAILQ_HEAD(rte_driver_list, rte_driver);
/** Double linked list of devices. */
TAILQ_HEAD(rte_device_list, rte_device);

/* Forward declaration */
struct rte_driver;

/**
 * A structure describing a generic device.
 */
struct rte_device {
	TAILQ_ENTRY(rte_device) next; /**< Next device */
	struct rte_driver *driver;    /**< Associated driver */
	int numa_node;                /**< NUMA node connection */
	struct rte_devargs *devargs;  /**< Device user arguments */
};

/**
 * Insert a device detected by a bus scanning.
 *
 * @param dev
 *   A pointer to a rte_device structure describing the detected device.
 */
void rte_eal_device_insert(struct rte_device *dev);

/**
 * Remove a device (e.g. when being unplugged).
 *
 * @param dev
 *   A pointer to a rte_device structure describing the device to be removed.
 */
void rte_eal_device_remove(struct rte_device *dev);

/**
 * A structure describing a device driver.
 */
struct rte_driver {
	TAILQ_ENTRY(rte_driver) next;  /**< Next in list. */
	const char *name;                   /**< Driver name. */
	const char *alias;              /**< Driver alias. */
};

/**
 * Register a device driver.
 *
 * @param driver
 *   A pointer to a rte_dev structure describing the driver
 *   to be registered.
 */
void rte_eal_driver_register(struct rte_driver *driver);

/**
 * Unregister a device driver.
 *
 * @param driver
 *   A pointer to a rte_dev structure describing the driver
 *   to be unregistered.
 */
void rte_eal_driver_unregister(struct rte_driver *driver);

/**
 * Initalize all the registered drivers in this process
 */
int rte_eal_dev_init(void);

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
int rte_eal_vdev_init(const char *name, const char *args);

/**
 * Uninitalize a driver specified by name.
 *
 * @param name
 *   The pointer to a driver name to be initialized.
 * @return
 *  0 on success, negative on error
 */
int rte_eal_vdev_uninit(const char *name);

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
 * @param name
 *   Same description as for rte_eal_dev_attach().
 *   Here, eal will call the driver detaching function.
 * @return
 *   0 on success, negative on error.
 */
int rte_eal_dev_detach(const char *name);

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

#ifdef __cplusplus
}
#endif

#endif /* _RTE_VDEV_H_ */
