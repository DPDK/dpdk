/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA Corporation & Affiliates
 */

#ifndef RTE_GPUDEV_H
#define RTE_GPUDEV_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <rte_compat.h>

/**
 * @file
 * Generic library to interact with GPU computing device.
 *
 * The API is not thread-safe.
 * Device management must be done by a single thread.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of devices if rte_gpu_init() is not called. */
#define RTE_GPU_DEFAULT_MAX 32

/** Empty device ID. */
#define RTE_GPU_ID_NONE -1

/** Store device info. */
struct rte_gpu_info {
	/** Unique identifier name. */
	const char *name;
	/** Device ID. */
	int16_t dev_id;
	/** Total processors available on device. */
	uint32_t processor_count;
	/** Total memory available on device. */
	size_t total_memory;
	/* Local NUMA memory ID. -1 if unknown. */
	int16_t numa_node;
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Initialize the device array before probing devices.
 * If not called, the maximum of probed devices is RTE_GPU_DEFAULT_MAX.
 *
 * @param dev_max
 *   Maximum number of devices.
 *
 * @return
 *   0 on success, -rte_errno otherwise:
 *   - ENOMEM if out of memory
 *   - EINVAL if 0 size
 *   - EBUSY if already initialized
 */
__rte_experimental
int rte_gpu_init(size_t dev_max);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Return the number of GPU detected and associated to DPDK.
 *
 * @return
 *   The number of available computing devices.
 */
__rte_experimental
uint16_t rte_gpu_count_avail(void);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Check if the device is valid and initialized in DPDK.
 *
 * @param dev_id
 *   The input device ID.
 *
 * @return
 *   - True if dev_id is a valid and initialized computing device.
 *   - False otherwise.
 */
__rte_experimental
bool rte_gpu_is_valid(int16_t dev_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Get the ID of the next valid GPU initialized in DPDK.
 *
 * @param dev_id
 *   The initial device ID to start the research.
 *
 * @return
 *   Next device ID corresponding to a valid and initialized computing device,
 *   RTE_GPU_ID_NONE if there is none.
 */
__rte_experimental
int16_t rte_gpu_find_next(int16_t dev_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Macro to iterate over all valid GPU devices.
 *
 * @param dev_id
 *   The ID of the next possible valid device, usually 0 to iterate all.
 */
#define RTE_GPU_FOREACH(dev_id) \
	for (dev_id = rte_gpu_find_next(0); \
	     dev_id > 0; \
	     dev_id = rte_gpu_find_next(dev_id + 1))

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Close device.
 * All resources are released.
 *
 * @param dev_id
 *   Device ID to close.
 *
 * @return
 *   0 on success, -rte_errno otherwise:
 *   - ENODEV if invalid dev_id
 *   - EPERM if driver error
 */
__rte_experimental
int rte_gpu_close(int16_t dev_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Return device specific info.
 *
 * @param dev_id
 *   Device ID to get info.
 * @param info
 *   Memory structure to fill with the info.
 *
 * @return
 *   0 on success, -rte_errno otherwise:
 *   - ENODEV if invalid dev_id
 *   - EINVAL if NULL info
 *   - EPERM if driver error
 */
__rte_experimental
int rte_gpu_info_get(int16_t dev_id, struct rte_gpu_info *info);

#ifdef __cplusplus
}
#endif

#endif /* RTE_GPUDEV_H */
