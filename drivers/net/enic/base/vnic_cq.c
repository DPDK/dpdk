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

#include "vnic_dev.h"
#include "vnic_cq.h"

int vnic_cq_mem_size(struct vnic_cq *cq, unsigned int desc_count,
	unsigned int desc_size)
{
	int mem_size;

	mem_size = vnic_dev_desc_ring_size(&cq->ring, desc_count, desc_size);

	return mem_size;
}

void vnic_cq_free(struct vnic_cq *cq)
{
	vnic_dev_free_desc_ring(cq->vdev, &cq->ring);

	cq->ctrl = NULL;
}

int vnic_cq_alloc(struct vnic_dev *vdev, struct vnic_cq *cq, unsigned int index,
	unsigned int socket_id,
	unsigned int desc_count, unsigned int desc_size)
{
	int err;
	char res_name[NAME_MAX];
	static int instance;

	cq->index = index;
	cq->vdev = vdev;

	cq->ctrl = vnic_dev_get_res(vdev, RES_TYPE_CQ, index);
	if (!cq->ctrl) {
		pr_err("Failed to hook CQ[%d] resource\n", index);
		return -EINVAL;
	}

	snprintf(res_name, sizeof(res_name), "%d-cq-%d", instance++, index);
	err = vnic_dev_alloc_desc_ring(vdev, &cq->ring, desc_count, desc_size,
		socket_id, res_name);
	if (err)
		return err;

	return 0;
}

void vnic_cq_init(struct vnic_cq *cq, unsigned int flow_control_enable,
	unsigned int color_enable, unsigned int cq_head, unsigned int cq_tail,
	unsigned int cq_tail_color, unsigned int interrupt_enable,
	unsigned int cq_entry_enable, unsigned int cq_message_enable,
	unsigned int interrupt_offset, u64 cq_message_addr)
{
	u64 paddr;

	paddr = (u64)cq->ring.base_addr | VNIC_PADDR_TARGET;
	writeq(paddr, &cq->ctrl->ring_base);
	iowrite32(cq->ring.desc_count, &cq->ctrl->ring_size);
	iowrite32(flow_control_enable, &cq->ctrl->flow_control_enable);
	iowrite32(color_enable, &cq->ctrl->color_enable);
	iowrite32(cq_head, &cq->ctrl->cq_head);
	iowrite32(cq_tail, &cq->ctrl->cq_tail);
	iowrite32(cq_tail_color, &cq->ctrl->cq_tail_color);
	iowrite32(interrupt_enable, &cq->ctrl->interrupt_enable);
	iowrite32(cq_entry_enable, &cq->ctrl->cq_entry_enable);
	iowrite32(cq_message_enable, &cq->ctrl->cq_message_enable);
	iowrite32(interrupt_offset, &cq->ctrl->interrupt_offset);
	writeq(cq_message_addr, &cq->ctrl->cq_message_addr);

	cq->interrupt_offset = interrupt_offset;
}

void vnic_cq_clean(struct vnic_cq *cq)
{
	cq->to_clean = 0;
	cq->last_color = 0;

	iowrite32(0, &cq->ctrl->cq_head);
	iowrite32(0, &cq->ctrl->cq_tail);
	iowrite32(1, &cq->ctrl->cq_tail_color);

	vnic_dev_clear_desc_ring(&cq->ring);
}
