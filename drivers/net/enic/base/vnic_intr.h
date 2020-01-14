/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _VNIC_INTR_H_
#define _VNIC_INTR_H_


#include "vnic_dev.h"

#define VNIC_INTR_TIMER_TYPE_ABS	0
#define VNIC_INTR_TIMER_TYPE_QUIET	1

/* Interrupt control */
struct vnic_intr_ctrl {
	uint32_t coalescing_timer;		/* 0x00 */
	uint32_t pad0;
	uint32_t coalescing_value;		/* 0x08 */
	uint32_t pad1;
	uint32_t coalescing_type;		/* 0x10 */
	uint32_t pad2;
	uint32_t mask_on_assertion;		/* 0x18 */
	uint32_t pad3;
	uint32_t mask;				/* 0x20 */
	uint32_t pad4;
	uint32_t int_credits;			/* 0x28 */
	uint32_t pad5;
	uint32_t int_credit_return;		/* 0x30 */
	uint32_t pad6;
};

struct vnic_intr {
	unsigned int index;
	struct vnic_dev *vdev;
	struct vnic_intr_ctrl __iomem *ctrl;		/* memory-mapped */
};

static inline void vnic_intr_unmask(struct vnic_intr *intr)
{
	iowrite32(0, &intr->ctrl->mask);
}

static inline void vnic_intr_mask(struct vnic_intr *intr)
{
	iowrite32(1, &intr->ctrl->mask);
}

static inline int vnic_intr_masked(struct vnic_intr *intr)
{
	return ioread32(&intr->ctrl->mask);
}

static inline void vnic_intr_return_credits(struct vnic_intr *intr,
	unsigned int credits, int unmask, int reset_timer)
{
#define VNIC_INTR_UNMASK_SHIFT		16
#define VNIC_INTR_RESET_TIMER_SHIFT	17

	uint32_t int_credit_return = (credits & 0xffff) |
		(unmask ? (1 << VNIC_INTR_UNMASK_SHIFT) : 0) |
		(reset_timer ? (1 << VNIC_INTR_RESET_TIMER_SHIFT) : 0);

	iowrite32(int_credit_return, &intr->ctrl->int_credit_return);
}

static inline unsigned int vnic_intr_credits(struct vnic_intr *intr)
{
	return ioread32(&intr->ctrl->int_credits);
}

static inline void vnic_intr_return_all_credits(struct vnic_intr *intr)
{
	unsigned int credits = vnic_intr_credits(intr);
	int unmask = 1;
	int reset_timer = 1;

	vnic_intr_return_credits(intr, credits, unmask, reset_timer);
}

static inline uint32_t vnic_intr_legacy_pba(uint32_t __iomem *legacy_pba)
{
	/* read PBA without clearing */
	return ioread32(legacy_pba);
}

void vnic_intr_free(struct vnic_intr *intr);
int vnic_intr_alloc(struct vnic_dev *vdev, struct vnic_intr *intr,
	unsigned int index);
void vnic_intr_init(struct vnic_intr *intr, uint32_t coalescing_timer,
	unsigned int coalescing_type, unsigned int mask_on_assertion);
void vnic_intr_coalescing_timer_set(struct vnic_intr *intr,
	uint32_t coalescing_timer);
void vnic_intr_clean(struct vnic_intr *intr);

#endif /* _VNIC_INTR_H_ */
