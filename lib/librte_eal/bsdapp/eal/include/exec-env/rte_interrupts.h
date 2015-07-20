/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#ifndef _RTE_INTERRUPTS_H_
#error "don't include this file directly, please include generic <rte_interrupts.h>"
#endif

#ifndef _RTE_BSDAPP_INTERRUPTS_H_
#define _RTE_BSDAPP_INTERRUPTS_H_

enum rte_intr_handle_type {
	RTE_INTR_HANDLE_UNKNOWN = 0,
	RTE_INTR_HANDLE_UIO,      /**< uio device handle */
	RTE_INTR_HANDLE_ALARM,    /**< alarm handle */
	RTE_INTR_HANDLE_MAX
};

/** Handle for interrupts. */
struct rte_intr_handle {
	int fd;                          /**< file descriptor */
	int uio_cfg_fd;                  /**< UIO config file descriptor */
	enum rte_intr_handle_type type;  /**< handle type */
#ifdef RTE_NEXT_ABI
	int max_intr;                    /**< max interrupt requested */
	uint32_t nb_efd;                 /**< number of available efds */
	int *intr_vec;                   /**< intr vector number array */
#endif
};

/**
 * @param intr_handle
 *   Pointer to the interrupt handle.
 * @param epfd
 *   Epoll instance fd which the intr vector associated to.
 * @param op
 *   The operation be performed for the vector.
 *   Operation type of {ADD, DEL}.
 * @param vec
 *   RX intr vector number added to the epoll instance wait list.
 * @param data
 *   User raw data.
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
int
rte_intr_rx_ctl(struct rte_intr_handle *intr_handle,
		int epfd, int op, unsigned int vec, void *data);

/**
 * It enables the fastpath event fds if it's necessary.
 * It creates event fds when multi-vectors allowed,
 * otherwise it multiplexes the single event fds.
 *
 * @param intr_handle
 *   Pointer to the interrupt handle.
 * @param nb_vec
 *   Number of interrupt vector trying to enable.
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
int
rte_intr_efd_enable(struct rte_intr_handle *intr_handle, uint32_t nb_efd);

/**
 * It disable the fastpath event fds.
 * It deletes registered eventfds and closes the open fds.
 *
 * @param intr_handle
 *   Pointer to the interrupt handle.
 */
void
rte_intr_efd_disable(struct rte_intr_handle *intr_handle);

/**
 * The fastpath interrupt is enabled or not.
 *
 * @param intr_handle
 *   Pointer to the interrupt handle.
 */
int rte_intr_dp_is_en(struct rte_intr_handle *intr_handle);

/**
 * The interrupt handle instance allows other cause or not.
 * Other cause stands for none fastpath interrupt.
 *
 * @param intr_handle
 *   Pointer to the interrupt handle.
 */
int rte_intr_allow_others(struct rte_intr_handle *intr_handle);

#endif /* _RTE_BSDAPP_INTERRUPTS_H_ */
