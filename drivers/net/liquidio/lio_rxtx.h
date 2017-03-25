/*
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Cavium, Inc.. All rights reserved.
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
 *     * Neither the name of Cavium, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER(S) OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LIO_RXTX_H_
#define _LIO_RXTX_H_

#include <stdio.h>
#include <stdint.h>

#include <rte_spinlock.h>
#include <rte_memory.h>

#include "lio_struct.h"

#define LIO_STQUEUE_FIRST_ENTRY(ptr, type, elem)	\
	(type *)((char *)((ptr)->stqh_first) - offsetof(type, elem))

#define lio_check_timeout(cur_time, chk_time) ((cur_time) > (chk_time))

#define lio_uptime		\
	(size_t)(rte_get_timer_cycles() / rte_get_timer_hz())

struct lio_request_list {
	uint32_t reqtype;
	void *buf;
};

/** The size of each buffer in soft command buffer pool */
#define LIO_SOFT_COMMAND_BUFFER_SIZE	1536

/** Maximum number of buffers to allocate into soft command buffer pool */
#define LIO_MAX_SOFT_COMMAND_BUFFERS	255

struct lio_soft_command {
	/** Soft command buffer info. */
	struct lio_stailq_node node;
	uint64_t dma_addr;
	uint32_t size;

#define LIO_COMPLETION_WORD_INIT	0xffffffffffffffffULL
	uint64_t *status_word;

	/** Data buffer info */
	void *virtdptr;
	uint64_t dmadptr;
	uint32_t datasize;

	/** Return buffer info */
	void *virtrptr;
	uint64_t dmarptr;
	uint32_t rdatasize;

	/** Context buffer info */
	void *ctxptr;
	uint32_t ctxsize;

	/** Time out and callback */
	size_t wait_time;
	size_t timeout;
	uint32_t iq_no;
	void (*callback)(uint32_t, void *);
	void *callback_arg;
	struct rte_mbuf *mbuf;
};

int lio_setup_sc_buffer_pool(struct lio_device *lio_dev);
void lio_free_sc_buffer_pool(struct lio_device *lio_dev);

struct lio_soft_command *
lio_alloc_soft_command(struct lio_device *lio_dev,
		       uint32_t datasize, uint32_t rdatasize,
		       uint32_t ctxsize);
void lio_free_soft_command(struct lio_soft_command *sc);

/** Maximum ordered requests to process in every invocation of
 *  lio_process_ordered_list(). The function will continue to process requests
 *  as long as it can find one that has finished processing. If it keeps
 *  finding requests that have completed, the function can run for ever. The
 *  value defined here sets an upper limit on the number of requests it can
 *  process before it returns control to the poll thread.
 */
#define LIO_MAX_ORD_REQS_TO_PROCESS	4096

/** Error codes used in Octeon Host-Core communication.
 *
 *   31		16 15		0
 *   ----------------------------
 * |		|		|
 *   ----------------------------
 *   Error codes are 32-bit wide. The upper 16-bits, called Major Error Number,
 *   are reserved to identify the group to which the error code belongs. The
 *   lower 16-bits, called Minor Error Number, carry the actual code.
 *
 *   So error codes are (MAJOR NUMBER << 16)| MINOR_NUMBER.
 */
/** Status for a request.
 *  If the request is successfully queued, the driver will return
 *  a LIO_REQUEST_PENDING status. LIO_REQUEST_TIMEOUT is only returned by
 *  the driver if the response for request failed to arrive before a
 *  time-out period or if the request processing * got interrupted due to
 *  a signal respectively.
 */
enum {
	/** A value of 0x00000000 indicates no error i.e. success */
	LIO_REQUEST_DONE	= 0x00000000,
	/** (Major number: 0x0000; Minor Number: 0x0001) */
	LIO_REQUEST_PENDING	= 0x00000001,
	LIO_REQUEST_TIMEOUT	= 0x00000003,

};

/*------ Error codes used by firmware (bits 15..0 set by firmware */
#define LIO_FIRMWARE_MAJOR_ERROR_CODE	 0x0001
#define LIO_FIRMWARE_STATUS_CODE(status) \
	((LIO_FIRMWARE_MAJOR_ERROR_CODE << 16) | (status))

/** Initialize the response lists. The number of response lists to create is
 *  given by count.
 *  @param lio_dev - the lio device structure.
 */
void lio_setup_response_list(struct lio_device *lio_dev);

/** Check the status of first entry in the ordered list. If the instruction at
 *  that entry finished processing or has timed-out, the entry is cleaned.
 *  @param lio_dev - the lio device structure.
 *  @return 1 if the ordered list is empty, 0 otherwise.
 */
int lio_process_ordered_list(struct lio_device *lio_dev);

static inline void
lio_swap_8B_data(uint64_t *data, uint32_t blocks)
{
	while (blocks) {
		*data = rte_cpu_to_be_64(*data);
		blocks--;
		data++;
	}
}

/** Setup instruction queue zero for the device
 *  @param lio_dev which lio device to setup
 *
 *  @return 0 if success. -1 if fails
 */
int lio_setup_instr_queue0(struct lio_device *lio_dev);
void lio_free_instr_queue0(struct lio_device *lio_dev);
#endif	/* _LIO_RXTX_H_ */
