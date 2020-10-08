/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _RTE_IOAT_RAWDEV_H_
#define _RTE_IOAT_RAWDEV_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file rte_ioat_rawdev.h
 *
 * Definitions for using the ioat rawdev device driver
 *
 * @warning
 * @b EXPERIMENTAL: these structures and APIs may change without prior notice
 */

#include <rte_common.h>

/** Name of the device driver */
#define IOAT_PMD_RAWDEV_NAME rawdev_ioat
/** String reported as the device driver name by rte_rawdev_info_get() */
#define IOAT_PMD_RAWDEV_NAME_STR "rawdev_ioat"

/**
 * Configuration structure for an ioat rawdev instance
 *
 * This structure is to be passed as the ".dev_private" parameter when
 * calling the rte_rawdev_get_info() and rte_rawdev_configure() APIs on
 * an ioat rawdev instance.
 */
struct rte_ioat_rawdev_config {
	unsigned short ring_size; /**< size of job submission descriptor ring */
	bool hdls_disable;    /**< if set, ignore user-supplied handle params */
};

/**
 * Enqueue a fill operation onto the ioat device
 *
 * This queues up a fill operation to be performed by hardware, but does not
 * trigger hardware to begin that operation.
 *
 * @param dev_id
 *   The rawdev device id of the ioat instance
 * @param pattern
 *   The pattern to populate the destination buffer with
 * @param dst
 *   The physical address of the destination buffer
 * @param length
 *   The length of the destination buffer
 * @param dst_hdl
 *   An opaque handle for the destination data, to be returned when this
 *   operation has been completed and the user polls for the completion details.
 *   NOTE: If hdls_disable configuration option for the device is set, this
 *   parameter is ignored.
 * @return
 *   Number of operations enqueued, either 0 or 1
 */
static inline int
__rte_experimental
rte_ioat_enqueue_fill(int dev_id, uint64_t pattern, phys_addr_t dst,
		unsigned int length, uintptr_t dst_hdl);

/**
 * Enqueue a copy operation onto the ioat device
 *
 * This queues up a copy operation to be performed by hardware, but does not
 * trigger hardware to begin that operation.
 *
 * @param dev_id
 *   The rawdev device id of the ioat instance
 * @param src
 *   The physical address of the source buffer
 * @param dst
 *   The physical address of the destination buffer
 * @param length
 *   The length of the data to be copied
 * @param src_hdl
 *   An opaque handle for the source data, to be returned when this operation
 *   has been completed and the user polls for the completion details.
 *   NOTE: If hdls_disable configuration option for the device is set, this
 *   parameter is ignored.
 * @param dst_hdl
 *   An opaque handle for the destination data, to be returned when this
 *   operation has been completed and the user polls for the completion details.
 *   NOTE: If hdls_disable configuration option for the device is set, this
 *   parameter is ignored.
 * @return
 *   Number of operations enqueued, either 0 or 1
 */
static inline int
__rte_experimental
rte_ioat_enqueue_copy(int dev_id, phys_addr_t src, phys_addr_t dst,
		unsigned int length, uintptr_t src_hdl, uintptr_t dst_hdl);

/**
 * Add a fence to force ordering between operations
 *
 * This adds a fence to a sequence of operations to enforce ordering, such that
 * all operations enqueued before the fence must be completed before operations
 * after the fence.
 * NOTE: Since this fence may be added as a flag to the last operation enqueued,
 * this API may not function correctly when called immediately after an
 * "rte_ioat_perform_ops" call i.e. before any new operations are enqueued.
 *
 * @param dev_id
 *   The rawdev device id of the ioat instance
 * @return
 *   Number of fences enqueued, either 0 or 1
 */
static inline int
__rte_experimental
rte_ioat_fence(int dev_id);


/**
 * Trigger hardware to begin performing enqueued operations
 *
 * This API is used to write the "doorbell" to the hardware to trigger it
 * to begin the operations previously enqueued by rte_ioat_enqueue_copy()
 *
 * @param dev_id
 *   The rawdev device id of the ioat instance
 */
static inline void
__rte_experimental
rte_ioat_perform_ops(int dev_id);

/**
 * Returns details of operations that have been completed
 *
 * If the hdls_disable option was not set when the device was configured,
 * the function will return to the caller the user-provided "handles" for
 * the copy operations which have been completed by the hardware, and not
 * already returned by a previous call to this API.
 * If the hdls_disable option for the device was set on configure, the
 * max_copies, src_hdls and dst_hdls parameters will be ignored, and the
 * function returns the number of newly-completed operations.
 *
 * @param dev_id
 *   The rawdev device id of the ioat instance
 * @param max_copies
 *   The number of entries which can fit in the src_hdls and dst_hdls
 *   arrays, i.e. max number of completed operations to report.
 *   NOTE: If hdls_disable configuration option for the device is set, this
 *   parameter is ignored.
 * @param src_hdls
 *   Array to hold the source handle parameters of the completed ops.
 *   NOTE: If hdls_disable configuration option for the device is set, this
 *   parameter is ignored.
 * @param dst_hdls
 *   Array to hold the destination handle parameters of the completed ops.
 *   NOTE: If hdls_disable configuration option for the device is set, this
 *   parameter is ignored.
 * @return
 *   -1 on error, with rte_errno set appropriately.
 *   Otherwise number of completed operations i.e. number of entries written
 *   to the src_hdls and dst_hdls array parameters.
 */
static inline int
__rte_experimental
rte_ioat_completed_ops(int dev_id, uint8_t max_copies,
		uintptr_t *src_hdls, uintptr_t *dst_hdls);

/* include the implementation details from a separate file */
#include "rte_ioat_rawdev_fns.h"

#ifdef __cplusplus
}
#endif

#endif /* _RTE_IOAT_RAWDEV_H_ */
