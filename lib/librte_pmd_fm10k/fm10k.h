/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2013-2015 Intel Corporation. All rights reserved.
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

#ifndef _FM10K_H_
#define _FM10K_H_

#include <stdint.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_spinlock.h>
#include "fm10k_logs.h"
#include "base/fm10k_type.h"

/* descriptor ring base addresses must be aligned to the following */
#define FM10K_ALIGN_RX_DESC  128
#define FM10K_ALIGN_TX_DESC  128

/* The maximum packet size that FM10K supports */
#define FM10K_MAX_PKT_SIZE  (15 * 1024)

/* Minimum size of RX buffer FM10K supported */
#define FM10K_MIN_RX_BUF_SIZE  256

/* The maximum of SRIOV VFs per port supported */
#define FM10K_MAX_VF_NUM    64

/* number of descriptors must be a multiple of the following */
#define FM10K_MULT_RX_DESC  FM10K_REQ_RX_DESCRIPTOR_MULTIPLE
#define FM10K_MULT_TX_DESC  FM10K_REQ_TX_DESCRIPTOR_MULTIPLE

/* maximum size of descriptor rings */
#define FM10K_MAX_RX_RING_SZ  (512 * 1024)
#define FM10K_MAX_TX_RING_SZ  (512 * 1024)

/* minimum and maximum number of descriptors in a ring */
#define FM10K_MIN_RX_DESC  32
#define FM10K_MIN_TX_DESC  32
#define FM10K_MAX_RX_DESC  (FM10K_MAX_RX_RING_SZ / sizeof(union fm10k_rx_desc))
#define FM10K_MAX_TX_DESC  (FM10K_MAX_TX_RING_SZ / sizeof(struct fm10k_tx_desc))

/*
 * byte aligment for HW RX data buffer
 * Datasheet requires RX buffer addresses shall either be 512-byte aligned or
 * be 8-byte aligned but without crossing host memory pages (4KB alignment
 * boundaries). Satisfy first option.
 */
#define FM10K_RX_DATABUF_ALIGN 512

/*
 * threshold default, min, max, and divisor constraints
 * the configured values must satisfy the following:
 *   MIN <= value <= MAX
 *   DIV % value == 0
 */
#define FM10K_RX_FREE_THRESH_DEFAULT(rxq)  32
#define FM10K_RX_FREE_THRESH_MIN(rxq)      1
#define FM10K_RX_FREE_THRESH_MAX(rxq)      ((rxq)->nb_desc - 1)
#define FM10K_RX_FREE_THRESH_DIV(rxq)      ((rxq)->nb_desc)

#define FM10K_TX_FREE_THRESH_DEFAULT(txq)  32
#define FM10K_TX_FREE_THRESH_MIN(txq)      1
#define FM10K_TX_FREE_THRESH_MAX(txq)      ((txq)->nb_desc - 3)
#define FM10K_TX_FREE_THRESH_DIV(txq)      0

#define FM10K_DEFAULT_RX_PTHRESH      8
#define FM10K_DEFAULT_RX_HTHRESH      8
#define FM10K_DEFAULT_RX_WTHRESH      0

#define FM10K_DEFAULT_TX_PTHRESH      32
#define FM10K_DEFAULT_TX_HTHRESH      0
#define FM10K_DEFAULT_TX_WTHRESH      0

#define FM10K_TX_RS_THRESH_DEFAULT(txq)    32
#define FM10K_TX_RS_THRESH_MIN(txq)        1
#define FM10K_TX_RS_THRESH_MAX(txq)        \
	RTE_MIN(((txq)->nb_desc - 2), (txq)->free_thresh)
#define FM10K_TX_RS_THRESH_DIV(txq)        ((txq)->nb_desc)

#define FM10K_VLAN_TAG_SIZE 4

struct fm10k_dev_info {
	volatile uint32_t enable;
	volatile uint32_t glort;
	/* Protect the mailbox to avoid race condition */
	rte_spinlock_t    mbx_lock;
};

/*
 * Structure to store private data for each driver instance.
 */
struct fm10k_adapter {
	struct fm10k_hw             hw;
	struct fm10k_hw_stats       stats;
	struct fm10k_dev_info       info;
};

#define FM10K_DEV_PRIVATE_TO_HW(adapter) \
	(&((struct fm10k_adapter *)adapter)->hw)

#define FM10K_DEV_PRIVATE_TO_STATS(adapter) \
	(&((struct fm10k_adapter *)adapter)->stats)

#define FM10K_DEV_PRIVATE_TO_INFO(adapter) \
	(&((struct fm10k_adapter *)adapter)->info)

#define FM10K_DEV_PRIVATE_TO_MBXLOCK(adapter) \
	(&(((struct fm10k_adapter *)adapter)->info.mbx_lock))

struct fm10k_rx_queue {
	struct rte_mempool *mp;
	struct rte_mbuf **sw_ring;
	volatile union fm10k_rx_desc *hw_ring;
	struct rte_mbuf *pkt_first_seg; /**< First segment of current packet. */
	struct rte_mbuf *pkt_last_seg;  /**< Last segment of current packet. */
	uint64_t hw_ring_phys_addr;
	uint16_t next_dd;
	uint16_t next_alloc;
	uint16_t next_trigger;
	uint16_t alloc_thresh;
	volatile uint32_t *tail_ptr;
	uint16_t nb_desc;
	uint16_t queue_id;
	uint8_t port_id;
	uint8_t drop_en;
	uint8_t rx_deferred_start; /**< don't start this queue in dev start. */
};

/*
 * a FIFO is used to track which descriptors have their RS bit set for Tx
 * queues which are configured to allow multiple descriptors per packet
 */
struct fifo {
	uint16_t *list;
	uint16_t *head;
	uint16_t *tail;
	uint16_t *endp;
};

struct fm10k_tx_queue {
	struct rte_mbuf **sw_ring;
	struct fm10k_tx_desc *hw_ring;
	uint64_t hw_ring_phys_addr;
	struct fifo rs_tracker;
	uint16_t last_free;
	uint16_t next_free;
	uint16_t nb_free;
	uint16_t nb_used;
	uint16_t free_trigger;
	uint16_t free_thresh;
	uint16_t rs_thresh;
	volatile uint32_t *tail_ptr;
	uint16_t nb_desc;
	uint8_t port_id;
	uint8_t tx_deferred_start; /** < don't start this queue in dev start. */
	uint16_t queue_id;
};

#define MBUF_DMA_ADDR(mb) \
	((uint64_t) ((mb)->buf_physaddr + (mb)->data_off))

/* enforce 512B alignment on default Rx DMA addresses */
#define MBUF_DMA_ADDR_DEFAULT(mb) \
	((uint64_t) RTE_ALIGN(((mb)->buf_physaddr + RTE_PKTMBUF_HEADROOM), 512))

static inline void fifo_reset(struct fifo *fifo, uint32_t len)
{
	fifo->head = fifo->tail = fifo->list;
	fifo->endp = fifo->list + len;
}

static inline void fifo_insert(struct fifo *fifo, uint16_t val)
{
	*fifo->head = val;
	if (++fifo->head == fifo->endp)
		fifo->head = fifo->list;
}

/* do not worry about list being empty since we only check it once we know
 * we have used enough descriptors to set the RS bit at least once */
static inline uint16_t fifo_peek(struct fifo *fifo)
{
	return *fifo->tail;
}

static inline uint16_t fifo_remove(struct fifo *fifo)
{
	uint16_t val;
	val = *fifo->tail;
	if (++fifo->tail == fifo->endp)
		fifo->tail = fifo->list;
	return val;
}

static inline void
fm10k_pktmbuf_reset(struct rte_mbuf *mb, uint8_t in_port)
{
	rte_mbuf_refcnt_set(mb, 1);
	mb->next = NULL;
	mb->nb_segs = 1;

	/* enforce 512B alignment on default Rx virtual addresses */
	mb->data_off = (uint16_t)(RTE_PTR_ALIGN((char *)mb->buf_addr +
			RTE_PKTMBUF_HEADROOM, FM10K_RX_DATABUF_ALIGN)
			- (char *)mb->buf_addr);
	mb->port = in_port;
}

/*
 * Verify Rx packet buffer alignment is valid.
 *
 * Hardware requires specific alignment for Rx packet buffers. At
 * least one of the following two conditions must be satisfied.
 *  1. Address is 512B aligned
 *  2. Address is 8B aligned and buffer does not cross 4K boundary.
 *
 * Return 1 if buffer alignment satisfies at least one condition,
 * otherwise return 0.
 *
 * Note: Alignment is checked by the driver when the Rx queue is reset. It
 *       is assumed that if an entire descriptor ring can be filled with
 *       buffers containing valid alignment, then all buffers in that mempool
 *       have valid address alignment. It is the responsibility of the user
 *       to ensure all buffers have valid alignment, as it is the user who
 *       creates the mempool.
 * Note: It is assumed the buffer needs only to store a maximum size Ethernet
 *       frame.
 */
static inline int
fm10k_addr_alignment_valid(struct rte_mbuf *mb)
{
	uint64_t addr = MBUF_DMA_ADDR_DEFAULT(mb);
	uint64_t boundary1, boundary2;

	/* 512B aligned? */
	if (RTE_ALIGN(addr, 512) == addr)
		return 1;

	/* 8B aligned, and max Ethernet frame would not cross a 4KB boundary? */
	if (RTE_ALIGN(addr, 8) == addr) {
		boundary1 = RTE_ALIGN_FLOOR(addr, 4096);
		boundary2 = RTE_ALIGN_FLOOR(addr + ETHER_MAX_VLAN_FRAME_LEN,
						4096);
		if (boundary1 == boundary2)
			return 1;
	}

	PMD_INIT_LOG(ERR, "Error: Invalid buffer alignment!");

	return 0;
}

/* Rx and Tx prototypes */
uint16_t fm10k_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
	uint16_t nb_pkts);

uint16_t fm10k_recv_scattered_pkts(void *rx_queue,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts);

uint16_t fm10k_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
	uint16_t nb_pkts);
#endif
