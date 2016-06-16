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
#include "vnic_rq.h"

void vnic_rq_free(struct vnic_rq *rq)
{
	struct vnic_dev *vdev;

	vdev = rq->vdev;

	vnic_dev_free_desc_ring(vdev, &rq->ring);

	rq->ctrl = NULL;
}

int vnic_rq_alloc(struct vnic_dev *vdev, struct vnic_rq *rq, unsigned int index,
	unsigned int desc_count, unsigned int desc_size)
{
	int rc;
	char res_name[NAME_MAX];
	static int instance;

	rq->index = index;
	rq->vdev = vdev;

	rq->ctrl = vnic_dev_get_res(vdev, RES_TYPE_RQ, index);
	if (!rq->ctrl) {
		pr_err("Failed to hook RQ[%d] resource\n", index);
		return -EINVAL;
	}

	vnic_rq_disable(rq);

	snprintf(res_name, sizeof(res_name), "%d-rq-%d", instance++, index);
	rc = vnic_dev_alloc_desc_ring(vdev, &rq->ring, desc_count, desc_size,
		rq->socket_id, res_name);
	return rc;
}

void vnic_rq_init_start(struct vnic_rq *rq, unsigned int cq_index,
	unsigned int fetch_index, unsigned int posted_index,
	unsigned int error_interrupt_enable,
	unsigned int error_interrupt_offset)
{
	u64 paddr;
	unsigned int count = rq->ring.desc_count;

	paddr = (u64)rq->ring.base_addr | VNIC_PADDR_TARGET;
	writeq(paddr, &rq->ctrl->ring_base);
	iowrite32(count, &rq->ctrl->ring_size);
	iowrite32(cq_index, &rq->ctrl->cq_index);
	iowrite32(error_interrupt_enable, &rq->ctrl->error_interrupt_enable);
	iowrite32(error_interrupt_offset, &rq->ctrl->error_interrupt_offset);
	iowrite32(0, &rq->ctrl->error_status);
	iowrite32(fetch_index, &rq->ctrl->fetch_index);
	iowrite32(posted_index, &rq->ctrl->posted_index);
	if (rq->is_sop)
		iowrite32(((rq->is_sop << 10) | rq->data_queue_idx),
			  &rq->ctrl->data_ring);
}

void vnic_rq_init(struct vnic_rq *rq, unsigned int cq_index,
	unsigned int error_interrupt_enable,
	unsigned int error_interrupt_offset)
{
	u32 fetch_index = 0;

	/* Use current fetch_index as the ring starting point */
	fetch_index = ioread32(&rq->ctrl->fetch_index);

	if (fetch_index == 0xFFFFFFFF) { /* check for hardware gone  */
		/* Hardware surprise removal: reset fetch_index */
		fetch_index = 0;
	}

	vnic_rq_init_start(rq, cq_index,
		fetch_index, fetch_index,
		error_interrupt_enable,
		error_interrupt_offset);
	rq->rxst_idx = 0;
	rq->tot_pkts = 0;
	rq->pkt_first_seg = NULL;
	rq->pkt_last_seg = NULL;
}

void vnic_rq_error_out(struct vnic_rq *rq, unsigned int error)
{
	iowrite32(error, &rq->ctrl->error_status);
}

unsigned int vnic_rq_error_status(struct vnic_rq *rq)
{
	return ioread32(&rq->ctrl->error_status);
}

void vnic_rq_enable(struct vnic_rq *rq)
{
	iowrite32(1, &rq->ctrl->enable);
}

int vnic_rq_disable(struct vnic_rq *rq)
{
	unsigned int wait;

	iowrite32(0, &rq->ctrl->enable);

	/* Wait for HW to ACK disable request */
	for (wait = 0; wait < 1000; wait++) {
		if (!(ioread32(&rq->ctrl->running)))
			return 0;
		udelay(10);
	}

	pr_err("Failed to disable RQ[%d]\n", rq->index);

	return -ETIMEDOUT;
}

void vnic_rq_clean(struct vnic_rq *rq,
	void (*buf_clean)(struct rte_mbuf **buf))
{
	struct rte_mbuf **buf;
	u32 fetch_index, i;
	unsigned int count = rq->ring.desc_count;

	buf = &rq->mbuf_ring[0];

	for (i = 0; i < count; i++) {
		(*buf_clean)(buf);
		buf++;
	}
	rq->ring.desc_avail = count - 1;
	rq->rx_nb_hold = 0;

	/* Use current fetch_index as the ring starting point */
	fetch_index = ioread32(&rq->ctrl->fetch_index);

	if (fetch_index == 0xFFFFFFFF) { /* check for hardware gone  */
		/* Hardware surprise removal: reset fetch_index */
		fetch_index = 0;
	}

	iowrite32(fetch_index, &rq->ctrl->posted_index);

	vnic_dev_clear_desc_ring(&rq->ring);
}
