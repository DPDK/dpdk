/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _VNIC_RQ_H_
#define _VNIC_RQ_H_

#include <stdbool.h>

#include "vnic_dev.h"
#include "vnic_cq.h"

/* Receive queue control */
struct vnic_rq_ctrl {
	uint64_t ring_base;			/* 0x00 */
	uint32_t ring_size;			/* 0x08 */
	uint32_t pad0;
	uint32_t posted_index;			/* 0x10 */
	uint32_t pad1;
	uint32_t cq_index;			/* 0x18 */
	uint32_t pad2;
	uint32_t enable;			/* 0x20 */
	uint32_t pad3;
	uint32_t running;			/* 0x28 */
	uint32_t pad4;
	uint32_t fetch_index;			/* 0x30 */
	uint32_t pad5;
	uint32_t error_interrupt_enable;	/* 0x38 */
	uint32_t pad6;
	uint32_t error_interrupt_offset;	/* 0x40 */
	uint32_t pad7;
	uint32_t error_status;			/* 0x48 */
	uint32_t pad8;
	uint32_t tcp_sn;			/* 0x50 */
	uint32_t pad9;
	uint32_t unused;			/* 0x58 */
	uint32_t pad10;
	uint32_t dca_select;			/* 0x60 */
	uint32_t pad11;
	uint32_t dca_value;			/* 0x68 */
	uint32_t pad12;
	uint32_t data_ring;			/* 0x70 */
	uint32_t pad13;
	uint32_t header_split;			/* 0x78 */
	uint32_t pad14;
};

struct vnic_rq {
	unsigned int index;
	unsigned int posted_index;
	struct vnic_dev *vdev;
	struct vnic_rq_ctrl __iomem *ctrl;	/* memory-mapped */
	struct vnic_dev_ring ring;
	struct rte_mbuf **free_mbufs;		/* reserve of free mbufs */
	int num_free_mbufs;
	struct rte_mbuf **mbuf_ring;		/* array of allocated mbufs */
	unsigned int mbuf_next_idx;		/* next mb to consume */
	void *os_buf_head;
	unsigned int pkts_outstanding;
	uint16_t rx_nb_hold;
	uint16_t rx_free_thresh;
	unsigned int socket_id;
	struct rte_mempool *mp;
	uint16_t rxst_idx;
	uint32_t tot_pkts;
	uint16_t data_queue_idx;
	uint8_t data_queue_enable;
	uint8_t is_sop;
	uint8_t in_use;
	struct rte_mbuf *pkt_first_seg;
	struct rte_mbuf *pkt_last_seg;
	unsigned int max_mbufs_per_pkt;
	uint16_t tot_nb_desc;
	bool need_initial_post;
};

static inline unsigned int vnic_rq_desc_avail(struct vnic_rq *rq)
{
	/* how many does SW own? */
	return rq->ring.desc_avail;
}

static inline unsigned int vnic_rq_desc_used(struct vnic_rq *rq)
{
	/* how many does HW own? */
	return rq->ring.desc_count - rq->ring.desc_avail - 1;
}



enum desc_return_options {
	VNIC_RQ_RETURN_DESC,
	VNIC_RQ_DEFER_RETURN_DESC,
};

static inline int vnic_rq_fill(struct vnic_rq *rq,
	int (*buf_fill)(struct vnic_rq *rq))
{
	int err;

	while (vnic_rq_desc_avail(rq) > 0) {

		err = (*buf_fill)(rq);
		if (err)
			return err;
	}

	return 0;
}

static inline int vnic_rq_fill_count(struct vnic_rq *rq,
	int (*buf_fill)(struct vnic_rq *rq), unsigned int count)
{
	int err;

	while ((vnic_rq_desc_avail(rq) > 0) && (count--)) {

		err = (*buf_fill)(rq);
		if (err)
			return err;
	}

	return 0;
}

void vnic_rq_free(struct vnic_rq *rq);
int vnic_rq_alloc(struct vnic_dev *vdev, struct vnic_rq *rq, unsigned int index,
	unsigned int desc_count, unsigned int desc_size);
void vnic_rq_init_start(struct vnic_rq *rq, unsigned int cq_index,
	unsigned int fetch_index, unsigned int posted_index,
	unsigned int error_interrupt_enable,
	unsigned int error_interrupt_offset);
void vnic_rq_init(struct vnic_rq *rq, unsigned int cq_index,
	unsigned int error_interrupt_enable,
	unsigned int error_interrupt_offset);
void vnic_rq_error_out(struct vnic_rq *rq, unsigned int error);
unsigned int vnic_rq_error_status(struct vnic_rq *rq);
void vnic_rq_enable(struct vnic_rq *rq);
int vnic_rq_disable(struct vnic_rq *rq);
void vnic_rq_clean(struct vnic_rq *rq,
	void (*buf_clean)(struct rte_mbuf **buf));
#endif /* _VNIC_RQ_H_ */
