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

/** Setup instruction queue zero for the device
 *  @param lio_dev which lio device to setup
 *
 *  @return 0 if success. -1 if fails
 */
int lio_setup_instr_queue0(struct lio_device *lio_dev);
void lio_free_instr_queue0(struct lio_device *lio_dev);
#endif	/* _LIO_RXTX_H_ */
