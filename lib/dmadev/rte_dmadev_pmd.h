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

#include <dev_driver.h>

#include <rte_compat.h>
#include "rte_dmadev.h"

#ifdef __cplusplus
extern "C" {
#endif

struct rte_dma_dev;

/** @internal Used to get device information of a device. */
typedef int (*rte_dma_info_get_t)(const struct rte_dma_dev *dev,
				  struct rte_dma_info *dev_info,
				  uint32_t info_sz);

/** @internal Used to configure a device. */
typedef int (*rte_dma_configure_t)(struct rte_dma_dev *dev,
				   const struct rte_dma_conf *dev_conf,
				   uint32_t conf_sz);

/** @internal Used to start a configured device. */
typedef int (*rte_dma_start_t)(struct rte_dma_dev *dev);

/** @internal Used to stop a configured device. */
typedef int (*rte_dma_stop_t)(struct rte_dma_dev *dev);

/** @internal Used to close a configured device. */
typedef int (*rte_dma_close_t)(struct rte_dma_dev *dev);

/** @internal Used to allocate and set up a virtual DMA channel. */
typedef int (*rte_dma_vchan_setup_t)(struct rte_dma_dev *dev, uint16_t vchan,
				const struct rte_dma_vchan_conf *conf,
				uint32_t conf_sz);

/** @internal Used to retrieve basic statistics. */
typedef int (*rte_dma_stats_get_t)(const struct rte_dma_dev *dev,
			uint16_t vchan, struct rte_dma_stats *stats,
			uint32_t stats_sz);

/** @internal Used to reset basic statistics. */
typedef int (*rte_dma_stats_reset_t)(struct rte_dma_dev *dev, uint16_t vchan);

/** @internal Used to check if a virtual channel has finished all jobs. */
typedef int (*rte_dma_vchan_status_t)(const struct rte_dma_dev *dev, uint16_t vchan,
		enum rte_dma_vchan_status *status);

/** @internal Used to dump internal information. */
typedef int (*rte_dma_dump_t)(const struct rte_dma_dev *dev, FILE *f);

/** @internal Used to create an access pair group for inter-process or inter-OS DMA transfers. */
typedef int (*rte_dma_access_pair_group_create_t)(const struct rte_dma_dev *dev,
						  rte_uuid_t domain_id, rte_uuid_t token,
						  int16_t *group_id,
						  rte_dma_access_pair_group_event_cb_t cb);

/** @internal Used to destroy an access pair group when all other devices have exited. */
typedef int (*rte_dma_access_pair_group_destroy_t)(const struct rte_dma_dev *dev,
						   int16_t group_id);

/** @internal Used to join an access pair group for inter-process or inter-OS DMA transfers. */
typedef int (*rte_dma_access_pair_group_join_t)(const struct rte_dma_dev *dev, rte_uuid_t domain_id,
						rte_uuid_t token, int16_t group_id,
						rte_dma_access_pair_group_event_cb_t cb);

/** @internal Used to leave an access pair group, removing the device from the group. */
typedef int (*rte_dma_access_pair_group_leave_t)(const struct rte_dma_dev *dev, int16_t group_id);

/** @internal Used to retrieve handler information of the domain_id present in the group. */
typedef int (*rte_dma_access_pair_group_handler_get_t)(const struct rte_dma_dev *dev,
						       int16_t group_id, rte_uuid_t domain_id,
						       uint16_t *handler);

/**
 * DMA device operations function pointer table.
 *
 * @see struct rte_dma_dev:dev_ops
 */
struct rte_dma_dev_ops {
	rte_dma_info_get_t         dev_info_get;
	rte_dma_configure_t        dev_configure;
	rte_dma_start_t            dev_start;
	rte_dma_stop_t             dev_stop;
	rte_dma_close_t            dev_close;

	rte_dma_vchan_setup_t      vchan_setup;

	rte_dma_stats_get_t        stats_get;
	rte_dma_stats_reset_t      stats_reset;

	rte_dma_vchan_status_t     vchan_status;
	rte_dma_dump_t             dev_dump;

	rte_dma_access_pair_group_create_t	access_pair_group_create;
	rte_dma_access_pair_group_destroy_t	access_pair_group_destroy;
	rte_dma_access_pair_group_join_t	access_pair_group_join;
	rte_dma_access_pair_group_leave_t	access_pair_group_leave;
	rte_dma_access_pair_group_handler_get_t	access_pair_group_handler_get;
};

/**
 * @internal
 * The data part, with no function pointers, associated with each DMA device.
 *
 * This structure is safe to place in shared memory to be common among different
 * processes in a multi-process configuration.
 *
 * @see struct rte_dma_dev::data
 */
struct __rte_cache_aligned rte_dma_dev_data {
	char dev_name[RTE_DEV_NAME_MAX_LEN]; /**< Unique identifier name */
	int16_t dev_id; /**< Device [external] identifier. */
	int16_t numa_node; /**< Local NUMA memory ID. -1 if unknown. */
	void *dev_private; /**< PMD-specific private data. */
	struct rte_dma_conf dev_conf; /**< DMA device configuration. */
	__extension__
	uint8_t dev_started : 1; /**< Device state: STARTED(1)/STOPPED(0). */
	uint64_t reserved[2]; /**< Reserved for future fields */
};

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
struct __rte_cache_aligned rte_dma_dev {
	/** Device info which supplied during device initialization. */
	struct rte_device *device;
	struct rte_dma_dev_data *data; /**< Pointer to shared device data. */
	/**< Fast-path functions and related data. */
	struct rte_dma_fp_object *fp_obj;
	/** Functions implemented by PMD. */
	const struct rte_dma_dev_ops *dev_ops;
	enum rte_dma_dev_state state; /**< Flag indicating the device state. */
	uint64_t reserved[2]; /**< Reserved for future fields. */
};

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

/**
 * @internal
 * Get the rte_dma_dev structure device pointer for the device ID.
 *
 * @param dev_id
 *   DMA device index in dmadev library.
 *
 * @return
 *   rte_dma_dev structure pointer on success, NULL otherwise.
 */
__rte_internal
struct rte_dma_dev *rte_dma_pmd_get_dev_by_id(int16_t dev_id);

#ifdef __cplusplus
}
#endif

#endif /* RTE_DMADEV_PMD_H */
