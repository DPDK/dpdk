/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#ifndef _IONIC_REGS_H_
#define _IONIC_REGS_H_

/** struct ionic_intr - interrupt control register set.
 * @coal_init:			coalesce timer initial value.
 * @mask:			interrupt mask value.
 * @credits:			interrupt credit count and return.
 * @mask_assert:		interrupt mask value on assert.
 * @coal:			coalesce timer time remaining.
 */
struct ionic_intr {
	uint32_t coal_init;
	uint32_t mask;
	uint32_t credits;
	uint32_t mask_assert;
	uint32_t coal;
	uint32_t rsvd[3];
};

/** enum ionic_intr_mask_vals - valid values for mask and mask_assert.
 * @IONIC_INTR_MASK_CLEAR:	unmask interrupt.
 * @IONIC_INTR_MASK_SET:	mask interrupt.
 */
enum ionic_intr_mask_vals {
	IONIC_INTR_MASK_CLEAR		= 0,
	IONIC_INTR_MASK_SET		= 1,
};

/** enum ionic_intr_credits_bits - bitwise composition of credits values.
 * @IONIC_INTR_CRED_COUNT:	bit mask of credit count, no shift needed.
 * @IONIC_INTR_CRED_COUNT_SIGNED: bit mask of credit count, including sign bit.
 * @IONIC_INTR_CRED_UNMASK:	unmask the interrupt.
 * @IONIC_INTR_CRED_RESET_COALESCE: reset the coalesce timer.
 * @IONIC_INTR_CRED_REARM:	unmask the and reset the timer.
 */
enum ionic_intr_credits_bits {
	IONIC_INTR_CRED_COUNT		= 0x7fffu,
	IONIC_INTR_CRED_COUNT_SIGNED	= 0xffffu,
	IONIC_INTR_CRED_UNMASK		= 0x10000u,
	IONIC_INTR_CRED_RESET_COALESCE	= 0x20000u,
	IONIC_INTR_CRED_REARM		= (IONIC_INTR_CRED_UNMASK |
					   IONIC_INTR_CRED_RESET_COALESCE),
};

static inline void
ionic_intr_coal_init(struct ionic_intr __iomem *intr_ctrl,
		int intr_idx, uint32_t coal)
{
	iowrite32(coal, &intr_ctrl[intr_idx].coal_init);
}

static inline void
ionic_intr_mask(struct ionic_intr __iomem *intr_ctrl,
		int intr_idx, uint32_t mask)
{
	iowrite32(mask, &intr_ctrl[intr_idx].mask);
}

static inline void
ionic_intr_credits(struct ionic_intr __iomem *intr_ctrl,
		int intr_idx, uint32_t cred, uint32_t flags)
{
	if (cred > IONIC_INTR_CRED_COUNT) {
		IONIC_WARN_ON(cred > IONIC_INTR_CRED_COUNT);
		cred = ioread32(&intr_ctrl[intr_idx].credits);
		cred &= IONIC_INTR_CRED_COUNT_SIGNED;
	}

	iowrite32(cred | flags, &intr_ctrl[intr_idx].credits);
}

static inline void
ionic_intr_clean(struct ionic_intr __iomem *intr_ctrl,
		int intr_idx)
{
	uint32_t cred;

	cred = ioread32(&intr_ctrl[intr_idx].credits);
	cred &= IONIC_INTR_CRED_COUNT_SIGNED;
	cred |= IONIC_INTR_CRED_RESET_COALESCE;
	iowrite32(cred, &intr_ctrl[intr_idx].credits);
}

static inline void
ionic_intr_mask_assert(struct ionic_intr __iomem *intr_ctrl,
		int intr_idx, uint32_t mask)
{
	iowrite32(mask, &intr_ctrl[intr_idx].mask_assert);
}

/** enum ionic_dbell_bits - bitwise composition of dbell values.
 *
 * @IONIC_DBELL_QID_MASK:	unshifted mask of valid queue id bits.
 * @IONIC_DBELL_QID_SHIFT:	queue id shift amount in dbell value.
 * @IONIC_DBELL_QID:		macro to build QID component of dbell value.
 *
 * @IONIC_DBELL_RING_MASK:	unshifted mask of valid ring bits.
 * @IONIC_DBELL_RING_SHIFT:	ring shift amount in dbell value.
 * @IONIC_DBELL_RING:		macro to build ring component of dbell value.
 *
 * @IONIC_DBELL_RING_0:		ring zero dbell component value.
 * @IONIC_DBELL_RING_1:		ring one dbell component value.
 * @IONIC_DBELL_RING_2:		ring two dbell component value.
 * @IONIC_DBELL_RING_3:		ring three dbell component value.
 *
 * @IONIC_DBELL_INDEX_MASK:	bit mask of valid index bits, no shift needed.
 */
enum ionic_dbell_bits {
	IONIC_DBELL_QID_MASK		= 0xffffff,
	IONIC_DBELL_QID_SHIFT		= 24,

#define IONIC_DBELL_QID(n) \
	(((u64)(n) & IONIC_DBELL_QID_MASK) << IONIC_DBELL_QID_SHIFT)

	IONIC_DBELL_RING_MASK		= 0x7,
	IONIC_DBELL_RING_SHIFT		= 16,

#define IONIC_DBELL_RING(n) \
	(((u64)(n) & IONIC_DBELL_RING_MASK) << IONIC_DBELL_RING_SHIFT)

	IONIC_DBELL_RING_0		= 0,
	IONIC_DBELL_RING_1		= IONIC_DBELL_RING(1),
	IONIC_DBELL_RING_2		= IONIC_DBELL_RING(2),
	IONIC_DBELL_RING_3		= IONIC_DBELL_RING(3),

	IONIC_DBELL_INDEX_MASK		= 0xffff,
};

#endif /* _IONIC_REGS_H_ */
