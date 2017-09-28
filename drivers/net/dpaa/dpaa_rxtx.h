/*-
 *   BSD LICENSE
 *
 *   Copyright 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2017 NXP.
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
 *     * Neither the name of  Freescale Semiconductor, Inc nor the names of its
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

#ifndef __DPDK_RXTX_H__
#define __DPDK_RXTX_H__

/* internal offset from where IC is copied to packet buffer*/
#define DEFAULT_ICIOF          32
/* IC transfer size */
#define DEFAULT_ICSZ	48

/* IC offsets from buffer header address */
#define DEFAULT_RX_ICEOF	16

#define DPAA_MAX_DEQUEUE_NUM_FRAMES    63
	/** <Maximum number of frames to be dequeued in a single rx call*/
/* FD structure masks and offset */
#define DPAA_FD_FORMAT_MASK 0xE0000000
#define DPAA_FD_OFFSET_MASK 0x1FF00000
#define DPAA_FD_LENGTH_MASK 0xFFFFF
#define DPAA_FD_FORMAT_SHIFT 29
#define DPAA_FD_OFFSET_SHIFT 20

uint16_t dpaa_eth_queue_rx(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs);

uint16_t dpaa_eth_queue_tx(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs);

uint16_t dpaa_eth_tx_drop_all(void *q  __rte_unused,
			      struct rte_mbuf **bufs __rte_unused,
			      uint16_t nb_bufs __rte_unused);
#endif
