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
#ident "$Id: vnic_intr.h 171146 2014-05-02 07:08:20Z ssujith $"

#ifndef _VNIC_INTR_H_
#define _VNIC_INTR_H_


#include "vnic_dev.h"

#define VNIC_INTR_TIMER_TYPE_ABS	0
#define VNIC_INTR_TIMER_TYPE_QUIET	1

/* Interrupt control */
struct vnic_intr_ctrl {
	u32 coalescing_timer;		/* 0x00 */
	u32 pad0;
	u32 coalescing_value;		/* 0x08 */
	u32 pad1;
	u32 coalescing_type;		/* 0x10 */
	u32 pad2;
	u32 mask_on_assertion;		/* 0x18 */
	u32 pad3;
	u32 mask;			/* 0x20 */
	u32 pad4;
	u32 int_credits;		/* 0x28 */
	u32 pad5;
	u32 int_credit_return;		/* 0x30 */
	u32 pad6;
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

	u32 int_credit_return = (credits & 0xffff) |
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

static inline u32 vnic_intr_legacy_pba(u32 __iomem *legacy_pba)
{
	/* read PBA without clearing */
	return ioread32(legacy_pba);
}

void vnic_intr_free(struct vnic_intr *intr);
int vnic_intr_alloc(struct vnic_dev *vdev, struct vnic_intr *intr,
	unsigned int index);
void vnic_intr_init(struct vnic_intr *intr, u32 coalescing_timer,
	unsigned int coalescing_type, unsigned int mask_on_assertion);
void vnic_intr_coalescing_timer_set(struct vnic_intr *intr,
	u32 coalescing_timer);
void vnic_intr_clean(struct vnic_intr *intr);

#endif /* _VNIC_INTR_H_ */
