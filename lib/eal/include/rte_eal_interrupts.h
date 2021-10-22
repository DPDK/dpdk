/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_INTERRUPTS_H_
#error "don't include this file directly, please include generic <rte_interrupts.h>"
#endif

/**
 * @file rte_eal_interrupts.h
 * @internal
 *
 * Contains function prototypes exposed by the EAL for interrupt handling by
 * drivers and other DPDK internal consumers.
 */

#ifndef _RTE_EAL_INTERRUPTS_H_
#define _RTE_EAL_INTERRUPTS_H_

#define RTE_MAX_RXTX_INTR_VEC_ID      512
#define RTE_INTR_VEC_ZERO_OFFSET      0
#define RTE_INTR_VEC_RXTX_OFFSET      1

/**
 * The interrupt source type, e.g. UIO, VFIO, ALARM etc.
 */
enum rte_intr_handle_type {
	RTE_INTR_HANDLE_UNKNOWN = 0,  /**< generic unknown handle */
	RTE_INTR_HANDLE_UIO,          /**< uio device handle */
	RTE_INTR_HANDLE_UIO_INTX,     /**< uio generic handle */
	RTE_INTR_HANDLE_VFIO_LEGACY,  /**< vfio device handle (legacy) */
	RTE_INTR_HANDLE_VFIO_MSI,     /**< vfio device handle (MSI) */
	RTE_INTR_HANDLE_VFIO_MSIX,    /**< vfio device handle (MSIX) */
	RTE_INTR_HANDLE_ALARM,        /**< alarm handle */
	RTE_INTR_HANDLE_EXT,          /**< external handler */
	RTE_INTR_HANDLE_VDEV,         /**< virtual device */
	RTE_INTR_HANDLE_DEV_EVENT,    /**< device event handle */
	RTE_INTR_HANDLE_VFIO_REQ,     /**< VFIO request handle */
	RTE_INTR_HANDLE_MAX           /**< count of elements */
};

/** Handle for interrupts. */
struct rte_intr_handle {
	RTE_STD_C11
	union {
		struct {
			RTE_STD_C11
			union {
				/** VFIO device file descriptor */
				int vfio_dev_fd;
				/** UIO cfg file desc for uio_pci_generic */
				int uio_cfg_fd;
			};
			int fd;	/**< interrupt event file descriptor */
		};
		void *windows_handle; /**< device driver handle */
	};
	uint32_t alloc_flags;	/**< flags passed at allocation */
	enum rte_intr_handle_type type;  /**< handle type */
	uint32_t max_intr;             /**< max interrupt requested */
	uint32_t nb_efd;               /**< number of available efd(event fd) */
	uint8_t efd_counter_size;      /**< size of efd counter, used for vdev */
	uint16_t nb_intr;
		/**< Max vector count, default RTE_MAX_RXTX_INTR_VEC_ID */
	int efds[RTE_MAX_RXTX_INTR_VEC_ID];  /**< intr vectors/efds mapping */
	struct rte_epoll_event elist[RTE_MAX_RXTX_INTR_VEC_ID];
				       /**< intr vector epoll event */
	uint16_t vec_list_size;
	int *intr_vec;                 /**< intr vector number array */
};

#endif /* _RTE_EAL_INTERRUPTS_H_ */
