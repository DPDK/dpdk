/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 HiSilicon Limited
 */

#ifndef RTE_DMADEV_PMD_H
#define RTE_DMADEV_PMD_H

/**
 * @file
 *
 * DMA Device PMD interface
 *
 * Driver facing interface for a DMA device. These are not to be called directly
 * by any application.
 */

#include "rte_dmadev.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Possible states of a DMA device.
 *
 * @see struct rte_dma_dev::state
 */
enum rte_dma_dev_state {
	RTE_DMA_DEV_UNUSED = 0, /**< Device is unused. */
	/** Device is registered, but not ready to be used. */
	RTE_DMA_DEV_REGISTERED,
	/** Device is ready for use. This is set by the PMD. */
	RTE_DMA_DEV_READY,
};

/**
 * @internal
 * The generic data structure associated with each DMA device.
 */
struct rte_dma_dev {
	char dev_name[RTE_DEV_NAME_MAX_LEN]; /**< Unique identifier name */
	int16_t dev_id; /**< Device [external] identifier. */
	int16_t numa_node; /**< Local NUMA memory ID. -1 if unknown. */
	void *dev_private; /**< PMD-specific private data. */
	/** Device info which supplied during device initialization. */
	struct rte_device *device;
	enum rte_dma_dev_state state; /**< Flag indicating the device state. */
	uint64_t reserved[2]; /**< Reserved for future fields. */
} __rte_cache_aligned;

extern struct rte_dma_dev *rte_dma_devices;

/**
 * @internal
 * Allocate a new dmadev slot for an DMA device and return the pointer to that
 * slot for the driver to use.
 *
 * @param name
 *   DMA device name.
 * @param numa_node
 *   Driver's private data's NUMA node.
 * @param private_data_size
 *   Driver's private data size.
 *
 * @return
 *   A pointer to the DMA device slot case of success,
 *   NULL otherwise.
 */
__rte_internal
struct rte_dma_dev *rte_dma_pmd_allocate(const char *name, int numa_node,
					 size_t private_data_size);

/**
 * @internal
 * Release the specified dmadev.
 *
 * @param name
 *   DMA device name.
 *
 * @return
 *   - 0 on success, negative on error.
 */
__rte_internal
int rte_dma_pmd_release(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* RTE_DMADEV_PMD_H */
