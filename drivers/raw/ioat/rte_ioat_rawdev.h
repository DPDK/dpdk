/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _RTE_IOAT_RAWDEV_H_
#define _RTE_IOAT_RAWDEV_H_

/**
 * @file rte_ioat_rawdev.h
 *
 * Definitions for using the ioat rawdev device driver
 *
 * @warning
 * @b EXPERIMENTAL: these structures and APIs may change without prior notice
 */

#include <x86intrin.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_ioat_spec.h>

/** Name of the device driver */
#define IOAT_PMD_RAWDEV_NAME rawdev_ioat
/** String reported as the device driver name by rte_rawdev_info_get() */
#define IOAT_PMD_RAWDEV_NAME_STR "rawdev_ioat"
/** Name used to adjust the log level for this driver */
#define IOAT_PMD_LOG_NAME "rawdev.ioat"

/**
 * Configuration structure for an ioat rawdev instance
 *
 * This structure is to be passed as the ".dev_private" parameter when
 * calling the rte_rawdev_get_info() and rte_rawdev_configure() APIs on
 * an ioat rawdev instance.
 */
struct rte_ioat_rawdev_config {
	unsigned short ring_size;
};

/**
 * @internal
 * Structure representing a device instance
 */
struct rte_ioat_rawdev {
	struct rte_rawdev *rawdev;
	const struct rte_memzone *mz;
	const struct rte_memzone *desc_mz;

	volatile struct rte_ioat_registers *regs;
	phys_addr_t status_addr;
	phys_addr_t ring_addr;

	unsigned short ring_size;
	struct rte_ioat_generic_hw_desc *desc_ring;
	__m128i *hdls; /* completion handles for returning to user */

	/* some statistics for tracking, if added/changed update xstats fns*/
	uint64_t enqueue_failed __rte_cache_aligned;
	uint64_t enqueued;
	uint64_t started;
	uint64_t completed;

	/* to report completions, the device will write status back here */
	volatile uint64_t status __rte_cache_aligned;
};

#endif
