/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef EAL_INTERRUPTS_H
#define EAL_INTERRUPTS_H

struct rte_intr_handle {
	RTE_STD_C11
	union {
		struct {
			int dev_fd; /**< VFIO/UIO cfg device file descriptor */
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
	int *efds;  /**< intr vectors/efds mapping */
	struct rte_epoll_event *elist; /**< intr vector epoll event */
	uint16_t vec_list_size;
	int *intr_vec;                 /**< intr vector number array */
};

#endif /* EAL_INTERRUPTS_H */
