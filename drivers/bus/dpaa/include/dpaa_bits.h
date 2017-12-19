/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2017 NXP
 *
 */

#ifndef __DPAA_BITS_H
#define __DPAA_BITS_H

/* Bitfield stuff. */
#define BITS_PER_ULONG	(sizeof(unsigned long) << 3)
#define SHIFT_PER_ULONG	(((1 << 5) == BITS_PER_ULONG) ? 5 : 6)
#define BITS_MASK(idx)	(1UL << ((idx) & (BITS_PER_ULONG - 1)))
#define BITS_IDX(idx)	((idx) >> SHIFT_PER_ULONG)

static inline void dpaa_set_bits(unsigned long mask,
				 volatile unsigned long *p)
{
	*p |= mask;
}

static inline void dpaa_set_bit(int idx, volatile unsigned long *bits)
{
	dpaa_set_bits(BITS_MASK(idx), bits + BITS_IDX(idx));
}

static inline void dpaa_clear_bits(unsigned long mask,
				   volatile unsigned long *p)
{
	*p &= ~mask;
}

static inline void dpaa_clear_bit(int idx,
				  volatile unsigned long *bits)
{
	dpaa_clear_bits(BITS_MASK(idx), bits + BITS_IDX(idx));
}

#endif /* __DPAA_BITS_H */
