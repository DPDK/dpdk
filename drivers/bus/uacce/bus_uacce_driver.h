/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 HiSilicon Limited
 */

#ifndef BUS_UACCE_DRIVER_H
#define BUS_UACCE_DRIVER_H

/**
 * @file
 *
 * HiSilicon UACCE bus interface.
 */

#include <inttypes.h>
#include <stdlib.h>
#include <linux/types.h>

#include <rte_compat.h>
#include <rte_devargs.h>
#include <dev_driver.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define RTE_UACCE_DEV_PATH_SIZE		256
#define RTE_UACCE_API_NAME_SIZE		64
#define RTE_UACCE_ALGS_NAME_SIZE	384
#define RTE_UACCE_ATTR_MAX_SIZE		384

/*
 * Definition for queue file region type.
 */
enum rte_uacce_qfrt {
	RTE_UACCE_QFRT_MMIO = 0, /**< Device mmio region. */
	RTE_UACCE_QFRT_DUS,      /**< Device user share region. */
	RTE_UACCE_QFRT_BUTT
};

struct rte_uacce_driver;

/**
 * A structure describing a UACCE device.
 */
struct rte_uacce_device {
	RTE_TAILQ_ENTRY(rte_uacce_device) next;  /**< Next in device list. */
	struct rte_device device;                /**< Inherit core device. */
	struct rte_uacce_driver *driver;         /**< Driver used in probing. */
	char name[RTE_DEV_NAME_MAX_LEN];         /**< Device name. */
	char dev_root[RTE_UACCE_DEV_PATH_SIZE];  /**< Sysfs path with device name. */
	char cdev_path[RTE_UACCE_DEV_PATH_SIZE]; /**< Device path in devfs. */
	char api[RTE_UACCE_API_NAME_SIZE];       /**< Device context type. */
	char algs[RTE_UACCE_ALGS_NAME_SIZE];     /**< Device supported algorithms. */
	uint32_t flags;                          /**< Device flags. */
	int numa_node;                           /**< NUMA node connection, -1 if unknown. */
	uint32_t qfrt_sz[RTE_UACCE_QFRT_BUTT];   /**< Queue file region type's size. */
};

/**
 * @internal
 * Helper macro for drivers that need to convert to struct rte_uacce_device.
 */
#define RTE_DEV_TO_UACCE_DEV(ptr) \
	container_of(ptr, struct rte_uacce_device, device)

#define RTE_DEV_TO_UACCE_DEV_CONST(ptr) \
	container_of(ptr, const struct rte_uacce_device, device)

/**
 * A structure describing an ID for a UACCE driver. Each driver provides a
 * table of these IDs for each device that it supports.
 */
struct rte_uacce_id {
	const char *dev_api;   /**< Device context type. */
	/** Device algorithm.
	 * If this field is NULL, only dev_api is matched. Otherwise, in
	 * addition to match dev_api, dev_alg must be a subset of device's
	 * algs.
	 */
	const char *dev_alg;
};

/**
 * Initialization function for the driver called during probing.
 */
typedef int (rte_uacce_probe_t)(struct rte_uacce_driver *, struct rte_uacce_device *);

/**
 * Uninitialization function for the driver called during hotplugging.
 */
typedef int (rte_uacce_remove_t)(struct rte_uacce_device *);

/**
 * A structure describing a UACCE driver.
 */
struct rte_uacce_driver {
	RTE_TAILQ_ENTRY(rte_uacce_driver) next;	/**< Next in list. */
	struct rte_driver driver;               /**< Inherit core driver. */
	struct rte_uacce_bus *bus;              /**< UACCE bus reference. */
	rte_uacce_probe_t *probe;               /**< Device probe function. */
	rte_uacce_remove_t *remove;             /**< Device remove function. */
	const struct rte_uacce_id *id_table;    /**< ID table, NULL terminated. */
};

/**
 * Get available queue number.
 *
 * @param dev
 *   A pointer to a rte_uacce_device structure describing the device
 *   to use.
 *
 * @note The available queues on the device may changes dynamically,
 * for examples, other process may alloc or free queues.
 *
 * @return
 *   >=0 on success. Otherwise negative value is returned.
 */
__rte_internal
int rte_uacce_avail_queues(struct rte_uacce_device *dev);

/*
 * The queue context for a UACCE queue.
 */
struct rte_uacce_qcontex {
	int fd;                       /**< The file descriptor associated to the queue. */
	struct rte_uacce_device *dev; /**< The device associated to the queue. */
	void *qfrt_base[RTE_UACCE_QFRT_BUTT]; /**< The qfrt mmap's memory base. */
};

/**
 * Alloc one queue.
 *
 * @param dev
 *   A pointer to a rte_uacce_device structure describing the device to use.
 * @param qctx
 *   Pointer to queue context, which is used to store the queue information
 *   that is successfully applied for.
 *
 * @return
 *   0 on success. Otherwise negative value is returned.
 */
__rte_internal
int rte_uacce_queue_alloc(struct rte_uacce_device *dev, struct rte_uacce_qcontex *qctx);

/**
 * Free one queue.
 *
 * @param qctx
 *   Pointer to queue context, which allocated by @see rte_uacce_alloc_queue.
 *
 * @note Once the queue is freed, any operations on the queue (including
 * control-plane and data-plane, and also read & write mmap region) are not
 * allowed.
 */
__rte_internal
void rte_uacce_queue_free(struct rte_uacce_qcontex *qctx);

/**
 * Start one queue.
 *
 * @param qctx
 *   Pointer to queue context, which allocated by @see rte_uacce_alloc_queue.
 *
 * @return
 *   0 on success. Otherwise negative value is returned.
 */
__rte_internal
int rte_uacce_queue_start(struct rte_uacce_qcontex *qctx);

/**
 * Send ioctl command to one queue.
 *
 * @param qctx
 *   Pointer to queue context, which allocated by @see rte_uacce_alloc_queue.
 * @param cmd
 *   ioctl command.
 *   @note The nr must not conflict with the definition in Linux kerel:
 *   include/uapi/misc/uacce/uacce.h. It is recommended that the driver
 *   custom nr start from 64.
 * @param arg
 *   Command input & output buffer.
 *
 * @return
 *   0 on success. Otherwise negative value is returned.
 */
__rte_internal
int rte_uacce_queue_ioctl(struct rte_uacce_qcontex *qctx, unsigned long cmd, void *arg);

/**
 * Mmap queue file region.
 *
 * @param qctx
 *   Pointer to queue context, which allocated by @see rte_uacce_alloc_queue.
 * @param qfrt
 *   The queue file region type. Must be RTE_UACCE_QFRT_MMIO or
 *   RTE_UACCE_QFRT_DUS.
 *
 * @return
 *   Non-NULL on success. Otherwise NULL is returned.
 */
__rte_internal
void *rte_uacce_queue_mmap(struct rte_uacce_qcontex *qctx, enum rte_uacce_qfrt qfrt);

/**
 * Unmap queue file region.
 *
 * @param qctx
 *   Pointer to queue context, which allocated by @see rte_uacce_alloc_queue.
 * @param qfrt
 *   The queue file region type. Must be RTE_UACCE_QFRT_MMIO or
 *   RTE_UACCE_QFRT_DUS.
 *
 * @return
 *   Non-NULL on success. Otherwise NULL is returned.
 */
__rte_internal
void rte_uacce_queue_unmap(struct rte_uacce_qcontex *qctx, enum rte_uacce_qfrt qfrt);

/**
 * Register a UACCE driver.
 *
 * @param driver
 *   A pointer to a rte_uacce_driver structure describing the driver to be
 *   registered.
 */
__rte_internal
void rte_uacce_register(struct rte_uacce_driver *driver);

/**
 * Unregister a UACCE driver.
 *
 * @param driver
 *   A pointer to a rte_uacce_driver structure describing the driver to be
 *   unregistered.
 */
__rte_internal
void rte_uacce_unregister(struct rte_uacce_driver *driver);

/**
 * Helper for UACCE device registration from driver instance.
 */
#define RTE_PMD_REGISTER_UACCE(nm, uacce_drv) \
		RTE_INIT(uacceinitfn_ ##nm) \
		{\
			(uacce_drv).driver.name = RTE_STR(nm);\
			rte_uacce_register(&uacce_drv); \
		} \
		RTE_PMD_EXPORT_NAME(nm)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* BUS_UACCE_DRIVER_H */
