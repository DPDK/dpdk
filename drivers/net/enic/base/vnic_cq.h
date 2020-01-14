/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _VNIC_CQ_H_
#define _VNIC_CQ_H_

#include <rte_mbuf.h>

#include "cq_desc.h"
#include "vnic_dev.h"

/* Completion queue control */
struct vnic_cq_ctrl {
	uint64_t ring_base;			/* 0x00 */
	uint32_t ring_size;			/* 0x08 */
	uint32_t pad0;
	uint32_t flow_control_enable;		/* 0x10 */
	uint32_t pad1;
	uint32_t color_enable;			/* 0x18 */
	uint32_t pad2;
	uint32_t cq_head;			/* 0x20 */
	uint32_t pad3;
	uint32_t cq_tail;			/* 0x28 */
	uint32_t pad4;
	uint32_t cq_tail_color;			/* 0x30 */
	uint32_t pad5;
	uint32_t interrupt_enable;		/* 0x38 */
	uint32_t pad6;
	uint32_t cq_entry_enable;		/* 0x40 */
	uint32_t pad7;
	uint32_t cq_message_enable;		/* 0x48 */
	uint32_t pad8;
	uint32_t interrupt_offset;		/* 0x50 */
	uint32_t pad9;
	uint64_t cq_message_addr;		/* 0x58 */
	uint32_t pad10;
};

#ifdef ENIC_AIC
struct vnic_rx_bytes_counter {
	unsigned int small_pkt_bytes_cnt;
	unsigned int large_pkt_bytes_cnt;
};
#endif

struct vnic_cq {
	unsigned int index;
	struct vnic_dev *vdev;
	struct vnic_cq_ctrl __iomem *ctrl;              /* memory-mapped */
	struct vnic_dev_ring ring;
	unsigned int to_clean;
	unsigned int last_color;
	unsigned int interrupt_offset;
#ifdef ENIC_AIC
	struct vnic_rx_bytes_counter pkt_size_counter;
	unsigned int cur_rx_coal_timeval;
	unsigned int tobe_rx_coal_timeval;
	ktime_t prev_ts;
#endif
};

void vnic_cq_free(struct vnic_cq *cq);
int vnic_cq_alloc(struct vnic_dev *vdev, struct vnic_cq *cq, unsigned int index,
	unsigned int socket_id,
	unsigned int desc_count, unsigned int desc_size);
void vnic_cq_init(struct vnic_cq *cq, unsigned int flow_control_enable,
	unsigned int color_enable, unsigned int cq_head, unsigned int cq_tail,
	unsigned int cq_tail_color, unsigned int interrupt_enable,
	unsigned int cq_entry_enable, unsigned int message_enable,
	unsigned int interrupt_offset, uint64_t message_addr);
void vnic_cq_clean(struct vnic_cq *cq);
int vnic_cq_mem_size(struct vnic_cq *cq, unsigned int desc_count,
	unsigned int desc_size);

#endif /* _VNIC_CQ_H_ */
