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

#ifndef _VNIC_WQ_H_
#define _VNIC_WQ_H_


#include "vnic_dev.h"
#include "vnic_cq.h"
#include <rte_memzone.h>

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

/* 16 bytes */
struct vnic_wq_buf {
	struct rte_mempool *pool;
	void *mb;
};

struct vnic_wq {
	unsigned int index;
	struct vnic_dev *vdev;
	struct vnic_wq_ctrl __iomem *ctrl;              /* memory-mapped */
	struct vnic_dev_ring ring;
	struct vnic_wq_buf *bufs;
	unsigned int head_idx;
	unsigned int tail_idx;
	unsigned int socket_id;
	const struct rte_memzone *cqmsg_rz;
	uint16_t last_completed_index;
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

static inline uint32_t
buf_idx_incr(uint32_t n_descriptors, uint32_t idx)
{
	idx++;
	if (unlikely(idx == n_descriptors))
		idx = 0;
	return idx;
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
		   void (*buf_clean)(struct vnic_wq_buf *buf));
#endif /* _VNIC_WQ_H_ */
