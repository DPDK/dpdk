/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_OSAL_H
#define SXE2_OSAL_H

#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdbool.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_ether.h>
#include <rte_version.h>
#include <rte_bitops.h>

#ifndef __BITS_PER_LONG
#define __BITS_PER_LONG   (__SIZEOF_LONG__ * 8)
#endif
#define BIT_WORD(nr)      ((nr) / __BITS_PER_LONG)
#define BIT_MASK(nr)      (1UL << ((nr) % __BITS_PER_LONG))

#ifndef TAILQ_FOREACH_SAFE
#define TAILQ_FOREACH_SAFE(var, head, field, tvar) \
	for ((var) = TAILQ_FIRST((head)); \
		(var) && ((tvar) = TAILQ_NEXT((var), field), 1); \
		(var) = (tvar))
#endif

#define upper_32_bits(n) ((uint32_t)(((n) >> 16) >> 16))
#define lower_32_bits(n) ((uint32_t)((n) & 0xffffffff))

#ifndef SXE2_DIV_ROUND_UP
#define SXE2_DIV_ROUND_UP(n, d) \
			(((n) + (typeof(n))(d) - (typeof(n))1) / (typeof(n))(d))
#endif

enum sxe2_itr_idx {
	SXE2_ITR_IDX_0 = 0,
	SXE2_ITR_IDX_1,
	SXE2_ITR_IDX_2,
	SXE2_ITR_IDX_NONE,
};

#define SXE2_ETH_ALEN 6

#define SXE2_BITS_PER_BYTE 8
#define BITS_TO_LONGS(nr) SXE2_DIV_ROUND_UP(nr, SXE2_BITS_PER_BYTE * sizeof(unsigned long))
#define BITS_TO_U32(nr) SXE2_DIV_ROUND_UP(nr, 32)

#define BITMAP_LAST_WORD_MASK(nbits) (~0UL >> (-(nbits) & (__BITS_PER_LONG - 1)))

#define DECLARE_BITMAP(name, bits) \
				unsigned long name[BITS_TO_LONGS(bits)]
#define BITMAP_TYPE unsigned long

static inline void sxe2_set_bit(uint32_t nr, unsigned long *addr)
{
	addr[nr / __BITS_PER_LONG] |= 1UL << (nr % __BITS_PER_LONG);
}

static inline void sxe2_clear_bit(uint32_t nr, unsigned long *addr)
{
	addr[nr / __BITS_PER_LONG] &= ~(1UL << (nr % __BITS_PER_LONG));
}

static inline uint32_t sxe2_test_bit(uint32_t nr, const unsigned long *addr)
{
	return 1UL & (addr[BIT_WORD(nr)] >> (nr & (__BITS_PER_LONG-1)));
}

static inline uint32_t sxe2_bitmap_weight(const unsigned long *src, uint32_t nbits)
{
	uint32_t cnt = 0;
	uint16_t i;

	for (i = 0; i < nbits; i++) {
		if (sxe2_test_bit(i, src))
			cnt++;
	}
	return cnt;
}

static inline bool sxe2_bitmap_empty(const unsigned long *src, uint32_t nbits)
{
	uint16_t i;

	for (i = 0; i < nbits; i++) {
		if (sxe2_test_bit(i, src))
			return false;
	}
	return true;
}

static inline void sxe2_bitmap_zero(unsigned long *dst, uint32_t nbits)
{
	uint32_t len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
	memset(dst, 0, len);
}

static inline void sxe2_bitmap_copy(unsigned long *dst, const unsigned long *src,
			uint32_t nbits)
{
	uint32_t len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
	memcpy(dst, src, len);
}

#define sxe2_small_const_nbits(nbits) \
	((nbits) && (nbits) <= __BITS_PER_LONG && (nbits) > 0)

#define SXE2_BITMAP_LAST_WORD_MASK(nbits) \
	(((nbits) % __BITS_PER_LONG) ? \
	 ((1UL << ((nbits) % __BITS_PER_LONG)) - 1UL) : ~0UL)

static inline bool
__sxe2_bitmap_equal_generic(const unsigned long *src1,
			    const unsigned long *src2, uint32_t nbits)
{
	uint32_t i;
	uint32_t words = nbits / __BITS_PER_LONG;
	uint32_t trailing = nbits % __BITS_PER_LONG;

	for (i = 0; i < words; i++) {
		if (src1[i] != src2[i])
			return false;
	}

	if (trailing) {
		unsigned long mask = (1UL << trailing) - 1UL;
		if ((src1[words] & mask) != (src2[words] & mask))
			return false;
	}

	return true;
}

static inline bool
sxe2_bitmap_equal(const unsigned long *src1,
		  const unsigned long *src2, uint32_t nbits)
{
	if (sxe2_small_const_nbits(nbits))
		return !((*src1 ^ *src2) & SXE2_BITMAP_LAST_WORD_MASK(nbits));

	if (__rte_constant(nbits & 7) && (nbits % 8 == 0))
		return !memcmp(src1, src2, nbits / 8);

	return __sxe2_bitmap_equal_generic(src1, src2, nbits);
}

#endif /* SXE2_OSAL_H */
