/*
 * Copyright 2008-2015 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 *
 * Copyright (c) 2015, Cisco Systems, Inc.
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

#ifndef _ENIC_VNIC_WQ_H_
#define _ENIC_VNIC_WQ_H_

#include "vnic_dev.h"
#include "vnic_cq.h"

static inline void enic_vnic_post_wq_index(struct vnic_wq *wq)
{
	struct vnic_wq_buf *buf = wq->to_use;

	/* Adding write memory barrier prevents compiler and/or CPU
	 * reordering, thus avoiding descriptor posting before
	 * descriptor is initialized. Otherwise, hardware can read
	 * stale descriptor fields.
	*/
	wmb();
	iowrite32(buf->index, &wq->ctrl->posted_index);
}

static inline void enic_vnic_post_wq(struct vnic_wq *wq,
				     void *os_buf, dma_addr_t dma_addr,
				     unsigned int len, int sop,
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
	if (cq_entry)
		enic_vnic_post_wq_index(wq);
	wq->to_use = buf;

	wq->ring.desc_avail -= desc_skip_cnt;
}

#endif /* _ENIC_VNIC_WQ_H_ */
