/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#include "vnic_dev.h"
#include "vnic_wq.h"

static inline
int vnic_wq_get_ctrl(struct vnic_dev *vdev, struct vnic_wq *wq,
				unsigned int index, enum vnic_res_type res_type)
{
	wq->ctrl = vnic_dev_get_res(vdev, res_type, index);
	if (!wq->ctrl)
		return -EINVAL;
	return 0;
}

static inline
int vnic_wq_alloc_ring(struct vnic_dev *vdev, struct vnic_wq *wq,
				unsigned int desc_count, unsigned int desc_size)
{
	char res_name[NAME_MAX];
	static int instance;

	snprintf(res_name, sizeof(res_name), "%d-wq-%u", instance++, wq->index);
	return vnic_dev_alloc_desc_ring(vdev, &wq->ring, desc_count, desc_size,
		wq->socket_id, res_name);
}

static int vnic_wq_alloc_bufs(struct vnic_wq *wq)
{
	unsigned int count = wq->ring.desc_count;
       /* Allocate the mbuf ring */
	wq->bufs = (struct rte_mbuf **)rte_zmalloc_socket("wq->bufs",
		    sizeof(struct rte_mbuf *) * count,
		    RTE_CACHE_LINE_SIZE, wq->socket_id);
	wq->head_idx = 0;
	wq->tail_idx = 0;
	if (wq->bufs == NULL)
		return -ENOMEM;
	return 0;
}

void vnic_wq_free(struct vnic_wq *wq)
{
	struct vnic_dev *vdev;

	vdev = wq->vdev;

	vnic_dev_free_desc_ring(vdev, &wq->ring);

	rte_free(wq->bufs);
	wq->ctrl = NULL;
}


int vnic_wq_alloc(struct vnic_dev *vdev, struct vnic_wq *wq, unsigned int index,
	unsigned int desc_count, unsigned int desc_size)
{
	int err;

	wq->index = index;
	wq->vdev = vdev;

	err = vnic_wq_get_ctrl(vdev, wq, index, RES_TYPE_WQ);
	if (err) {
		pr_err("Failed to hook WQ[%d] resource, err %d\n", index, err);
		return err;
	}

	vnic_wq_disable(wq);

	err = vnic_wq_alloc_ring(vdev, wq, desc_count, desc_size);
	if (err)
		return err;

	err = vnic_wq_alloc_bufs(wq);
	if (err) {
		vnic_wq_free(wq);
		return err;
	}

	return 0;
}

void vnic_wq_init_start(struct vnic_wq *wq, unsigned int cq_index,
	unsigned int fetch_index, unsigned int posted_index,
	unsigned int error_interrupt_enable,
	unsigned int error_interrupt_offset)
{
	u64 paddr;
	unsigned int count = wq->ring.desc_count;

	paddr = (u64)wq->ring.base_addr | VNIC_PADDR_TARGET;
	writeq(paddr, &wq->ctrl->ring_base);
	iowrite32(count, &wq->ctrl->ring_size);
	iowrite32(fetch_index, &wq->ctrl->fetch_index);
	iowrite32(posted_index, &wq->ctrl->posted_index);
	iowrite32(cq_index, &wq->ctrl->cq_index);
	iowrite32(error_interrupt_enable, &wq->ctrl->error_interrupt_enable);
	iowrite32(error_interrupt_offset, &wq->ctrl->error_interrupt_offset);
	iowrite32(0, &wq->ctrl->error_status);

	wq->head_idx = fetch_index;
	wq->tail_idx = wq->head_idx;
}

void vnic_wq_init(struct vnic_wq *wq, unsigned int cq_index,
	unsigned int error_interrupt_enable,
	unsigned int error_interrupt_offset)
{
	vnic_wq_init_start(wq, cq_index, 0, 0,
		error_interrupt_enable,
		error_interrupt_offset);
	wq->cq_pend = 0;
	wq->last_completed_index = 0;
}

unsigned int vnic_wq_error_status(struct vnic_wq *wq)
{
	return ioread32(&wq->ctrl->error_status);
}

void vnic_wq_enable(struct vnic_wq *wq)
{
	iowrite32(1, &wq->ctrl->enable);
}

int vnic_wq_disable(struct vnic_wq *wq)
{
	unsigned int wait;

	iowrite32(0, &wq->ctrl->enable);

	/* Wait for HW to ACK disable request */
	for (wait = 0; wait < 1000; wait++) {
		if (!(ioread32(&wq->ctrl->running)))
			return 0;
		udelay(10);
	}

	pr_err("Failed to disable WQ[%d]\n", wq->index);

	return -ETIMEDOUT;
}

void vnic_wq_clean(struct vnic_wq *wq,
		   void (*buf_clean)(struct rte_mbuf **buf))
{
	struct rte_mbuf **buf;
	unsigned int  to_clean = wq->tail_idx;

	buf = &wq->bufs[to_clean];

	while (vnic_wq_desc_used(wq) > 0) {

		(*buf_clean)(buf);
		to_clean = buf_idx_incr(wq->ring.desc_count, to_clean);

		buf = &wq->bufs[to_clean];
		wq->ring.desc_avail++;
	}

	wq->head_idx = 0;
	wq->tail_idx = 0;
	wq->last_completed_index = 0;
	*((uint32_t *)wq->cqmsg_rz->addr) = 0;

	iowrite32(0, &wq->ctrl->fetch_index);
	iowrite32(0, &wq->ctrl->posted_index);
	iowrite32(0, &wq->ctrl->error_status);

	vnic_dev_clear_desc_ring(&wq->ring);
}
