/*
 * Copyright 2008-2010 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 *
 * Copyright (c) 2014, Cisco Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ident "$Id: vnic_wq.h 183023 2014-07-22 23:47:25Z xuywang $"

#ifndef _VNIC_WQ_H_
#define _VNIC_WQ_H_


#include "vnic_dev.h"
#include "vnic_cq.h"

/* Work queue control */
struct vnic_wq_ctrl {
	u64 ring_base;			/* 0x00 */
	u32 ring_size;			/* 0x08 */
	u32 pad0;
	u32 posted_index;		/* 0x10 */
	u32 pad1;
	u32 cq_index;			/* 0x18 */
	u32 pad2;
	u32 enable;			/* 0x20 */
	u32 pad3;
	u32 running;			/* 0x28 */
	u32 pad4;
	u32 fetch_index;		/* 0x30 */
	u32 pad5;
	u32 dca_value;			/* 0x38 */
	u32 pad6;
	u32 error_interrupt_enable;	/* 0x40 */
	u32 pad7;
	u32 error_interrupt_offset;	/* 0x48 */
	u32 pad8;
	u32 error_status;		/* 0x50 */
	u32 pad9;
};

struct vnic_wq_buf {
	struct vnic_wq_buf *next;
	dma_addr_t dma_addr;
	void *os_buf;
	unsigned int len;
	unsigned int index;
	int sop;
	void *desc;
	uint64_t wr_id; /* Cookie */
	uint8_t cq_entry; /* Gets completion event from hw */
	uint8_t desc_skip_cnt; /* Num descs to occupy */
	uint8_t compressed_send; /* Both hdr and payload in one desc */
};

/* Break the vnic_wq_buf allocations into blocks of 32/64 entries */
#define VNIC_WQ_BUF_MIN_BLK_ENTRIES 32
#define VNIC_WQ_BUF_DFLT_BLK_ENTRIES 64
#define VNIC_WQ_BUF_BLK_ENTRIES(entries) \
	((unsigned int)((entries < VNIC_WQ_BUF_DFLT_BLK_ENTRIES) ? \
	VNIC_WQ_BUF_MIN_BLK_ENTRIES : VNIC_WQ_BUF_DFLT_BLK_ENTRIES))
#define VNIC_WQ_BUF_BLK_SZ(entries) \
	(VNIC_WQ_BUF_BLK_ENTRIES(entries) * sizeof(struct vnic_wq_buf))
#define VNIC_WQ_BUF_BLKS_NEEDED(entries) \
	DIV_ROUND_UP(entries, VNIC_WQ_BUF_BLK_ENTRIES(entries))
#define VNIC_WQ_BUF_BLKS_MAX VNIC_WQ_BUF_BLKS_NEEDED(4096)

struct vnic_wq {
	unsigned int index;
	struct vnic_dev *vdev;
	struct vnic_wq_ctrl __iomem *ctrl;              /* memory-mapped */
	struct vnic_dev_ring ring;
	struct vnic_wq_buf *bufs[VNIC_WQ_BUF_BLKS_MAX];
	struct vnic_wq_buf *to_use;
	struct vnic_wq_buf *to_clean;
	unsigned int pkts_outstanding;
	unsigned int socket_id;
};

static inline unsigned int vnic_wq_desc_avail(struct vnic_wq *wq)
{
	/* how many does SW own? */
	return wq->ring.desc_avail;
}

static inline unsigned int vnic_wq_desc_used(struct vnic_wq *wq)
{
	/* how many does HW own? */
	return wq->ring.desc_count - wq->ring.desc_avail - 1;
}

static inline void *vnic_wq_next_desc(struct vnic_wq *wq)
{
	return wq->to_use->desc;
}

#define PI_LOG2_CACHE_LINE_SIZE        5
#define PI_INDEX_BITS            12
#define PI_INDEX_MASK ((1U << PI_INDEX_BITS) - 1)
#define PI_PREFETCH_LEN_MASK ((1U << PI_LOG2_CACHE_LINE_SIZE) - 1)
#define PI_PREFETCH_LEN_OFF 16
#define PI_PREFETCH_ADDR_BITS 43
#define PI_PREFETCH_ADDR_MASK ((1ULL << PI_PREFETCH_ADDR_BITS) - 1)
#define PI_PREFETCH_ADDR_OFF 21

/** How many cache lines are touched by buffer (addr, len). */
static inline unsigned int num_cache_lines_touched(dma_addr_t addr,
							unsigned int len)
{
	const unsigned long mask = PI_PREFETCH_LEN_MASK;
	const unsigned long laddr = (unsigned long)addr;
	unsigned long lines, equiv_len;
	/* A. If addr is aligned, our solution is just to round up len to the
	next boundary.

	e.g. addr = 0, len = 48
	+--------------------+
	|XXXXXXXXXXXXXXXXXXXX|    32-byte cacheline a
	+--------------------+
	|XXXXXXXXXX          |    cacheline b
	+--------------------+

	B. If addr is not aligned, however, we may use an extra
	cacheline.  e.g. addr = 12, len = 22

	+--------------------+
	|       XXXXXXXXXXXXX|
	+--------------------+
	|XX                  |
	+--------------------+

	Our solution is to make the problem equivalent to case A
	above by adding the empty space in the first cacheline to the length:
	unsigned long len;

	+--------------------+
	|eeeeeeeXXXXXXXXXXXXX|    "e" is empty space, which we add to len
	+--------------------+
	|XX                  |
	+--------------------+

	*/
	equiv_len = len + (laddr & mask);

	/* Now we can just round up this len to the next 32-byte boundary. */
	lines = (equiv_len + mask) & (~mask);

	/* Scale bytes -> cachelines. */
	return lines >> PI_LOG2_CACHE_LINE_SIZE;
}

static inline u64 vnic_cached_posted_index(dma_addr_t addr, unsigned int len,
						unsigned int index)
{
	unsigned int num_cache_lines = num_cache_lines_touched(addr, len);
	/* Wish we could avoid a branch here.  We could have separate
	 * vnic_wq_post() and vinc_wq_post_inline(), the latter
	 * only supporting < 1k (2^5 * 2^5) sends, I suppose.  This would
	 * eliminate the if (eop) branch as well.
	 */
	if (num_cache_lines > PI_PREFETCH_LEN_MASK)
		num_cache_lines = 0;
	return (index & PI_INDEX_MASK) |
	((num_cache_lines & PI_PREFETCH_LEN_MASK) << PI_PREFETCH_LEN_OFF) |
		(((addr >> PI_LOG2_CACHE_LINE_SIZE) &
	PI_PREFETCH_ADDR_MASK) << PI_PREFETCH_ADDR_OFF);
}

static inline void vnic_wq_post(struct vnic_wq *wq,
	void *os_buf, dma_addr_t dma_addr,
	unsigned int len, int sop, int eop,
	uint8_t desc_skip_cnt, uint8_t cq_entry,
	uint8_t compressed_send, uint64_t wrid)
{
	struct vnic_wq_buf *buf = wq->to_use;

	buf->sop = sop;
	buf->cq_entry = cq_entry;
	buf->compressed_send = compressed_send;
	buf->desc_skip_cnt = desc_skip_cnt;
	buf->os_buf = os_buf;
	buf->dma_addr = dma_addr;
	buf->len = len;
	buf->wr_id = wrid;

	buf = buf->next;
	if (eop) {
#ifdef DO_PREFETCH
		uint64_t wr = vnic_cached_posted_index(dma_addr, len,
							buf->index);
#endif
		/* Adding write memory barrier prevents compiler and/or CPU
		 * reordering, thus avoiding descriptor posting before
		 * descriptor is initialized. Otherwise, hardware can read
		 * stale descriptor fields.
		 */
		wmb();
#ifdef DO_PREFETCH
		/* Intel chipsets seem to limit the rate of PIOs that we can
		 * push on the bus.  Thus, it is very important to do a single
		 * 64 bit write here.  With two 32-bit writes, my maximum
		 * pkt/sec rate was cut almost in half. -AJF
		 */
		iowrite64((uint64_t)wr, &wq->ctrl->posted_index);
#else
		iowrite32(buf->index, &wq->ctrl->posted_index);
#endif
	}
	wq->to_use = buf;

	wq->ring.desc_avail -= desc_skip_cnt;
}

static inline void vnic_wq_service(struct vnic_wq *wq,
	struct cq_desc *cq_desc, u16 completed_index,
	void (*buf_service)(struct vnic_wq *wq,
	struct cq_desc *cq_desc, struct vnic_wq_buf *buf, void *opaque),
	void *opaque)
{
	struct vnic_wq_buf *buf;

	buf = wq->to_clean;
	while (1) {

		(*buf_service)(wq, cq_desc, buf, opaque);

		wq->ring.desc_avail++;

		wq->to_clean = buf->next;

		if (buf->index == completed_index)
			break;

		buf = wq->to_clean;
	}
}

void vnic_wq_free(struct vnic_wq *wq);
int vnic_wq_alloc(struct vnic_dev *vdev, struct vnic_wq *wq, unsigned int index,
	unsigned int desc_count, unsigned int desc_size);
void vnic_wq_init_start(struct vnic_wq *wq, unsigned int cq_index,
	unsigned int fetch_index, unsigned int posted_index,
	unsigned int error_interrupt_enable,
	unsigned int error_interrupt_offset);
void vnic_wq_init(struct vnic_wq *wq, unsigned int cq_index,
	unsigned int error_interrupt_enable,
	unsigned int error_interrupt_offset);
void vnic_wq_error_out(struct vnic_wq *wq, unsigned int error);
unsigned int vnic_wq_error_status(struct vnic_wq *wq);
void vnic_wq_enable(struct vnic_wq *wq);
int vnic_wq_disable(struct vnic_wq *wq);
void vnic_wq_clean(struct vnic_wq *wq,
	void (*buf_clean)(struct vnic_wq *wq, struct vnic_wq_buf *buf));
int vnic_wq_mem_size(struct vnic_wq *wq, unsigned int desc_count,
	unsigned int desc_size);

#endif /* _VNIC_WQ_H_ */
