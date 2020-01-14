/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _CQ_DESC_H_
#define _CQ_DESC_H_
#include <rte_byteorder.h>

/*
 * Completion queue descriptor types
 */
enum cq_desc_types {
	CQ_DESC_TYPE_WQ_ENET = 0,
	CQ_DESC_TYPE_DESC_COPY = 1,
	CQ_DESC_TYPE_WQ_EXCH = 2,
	CQ_DESC_TYPE_RQ_ENET = 3,
	CQ_DESC_TYPE_RQ_FCP = 4,
	CQ_DESC_TYPE_IOMMU_MISS = 5,
	CQ_DESC_TYPE_SGL = 6,
	CQ_DESC_TYPE_CLASSIFIER = 7,
	CQ_DESC_TYPE_TEST = 127,
};

/* Completion queue descriptor: 16B
 *
 * All completion queues have this basic layout.  The
 * type_specific area is unique for each completion
 * queue type.
 */
struct cq_desc {
	uint16_t completed_index;
	uint16_t q_number;
	uint8_t type_specific[11];
	uint8_t type_color;
};

#define CQ_DESC_TYPE_BITS        4
#define CQ_DESC_TYPE_MASK        ((1 << CQ_DESC_TYPE_BITS) - 1)
#define CQ_DESC_COLOR_MASK       1
#define CQ_DESC_COLOR_SHIFT      7
#define CQ_DESC_COLOR_MASK_NOSHIFT 0x80
#define CQ_DESC_Q_NUM_BITS       10
#define CQ_DESC_Q_NUM_MASK       ((1 << CQ_DESC_Q_NUM_BITS) - 1)
#define CQ_DESC_COMP_NDX_BITS    12
#define CQ_DESC_COMP_NDX_MASK    ((1 << CQ_DESC_COMP_NDX_BITS) - 1)

static inline void cq_color_enc(struct cq_desc *desc, const uint8_t color)
{
	if (color)
		desc->type_color |=  (1 << CQ_DESC_COLOR_SHIFT);
	else
		desc->type_color &= ~(1 << CQ_DESC_COLOR_SHIFT);
}

static inline void cq_desc_enc(struct cq_desc *desc,
	const uint8_t type, const uint8_t color, const uint16_t q_number,
	const uint16_t completed_index)
{
	desc->type_color = (type & CQ_DESC_TYPE_MASK) |
		((color & CQ_DESC_COLOR_MASK) << CQ_DESC_COLOR_SHIFT);
	desc->q_number = rte_cpu_to_le_16(q_number & CQ_DESC_Q_NUM_MASK);
	desc->completed_index = rte_cpu_to_le_16(completed_index &
		CQ_DESC_COMP_NDX_MASK);
}

static inline void cq_desc_dec(const struct cq_desc *desc_arg,
	uint8_t *type, uint8_t *color, uint16_t *q_number,
	uint16_t *completed_index)
{
	const struct cq_desc *desc = desc_arg;
	const uint8_t type_color = desc->type_color;

	*color = (type_color >> CQ_DESC_COLOR_SHIFT) & CQ_DESC_COLOR_MASK;

	/*
	 * Make sure color bit is read from desc *before* other fields
	 * are read from desc.  Hardware guarantees color bit is last
	 * bit (byte) written.  Adding the rmb() prevents the compiler
	 * and/or CPU from reordering the reads which would potentially
	 * result in reading stale values.
	 */

	rte_rmb();

	*type = type_color & CQ_DESC_TYPE_MASK;
	*q_number = rte_le_to_cpu_16(desc->q_number) & CQ_DESC_Q_NUM_MASK;
	*completed_index = rte_le_to_cpu_16(desc->completed_index) &
		CQ_DESC_COMP_NDX_MASK;
}

static inline void cq_color_dec(const struct cq_desc *desc_arg, uint8_t *color)
{
	volatile const struct cq_desc *desc = desc_arg;

	*color = (desc->type_color >> CQ_DESC_COLOR_SHIFT) & CQ_DESC_COLOR_MASK;
}

#endif /* _CQ_DESC_H_ */
