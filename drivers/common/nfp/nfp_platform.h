/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_PLATFORM_H__
#define __NFP_PLATFORM_H__

#include <stdint.h>

#define DIV_ROUND_UP(n, d)             (((n) + (d) - 1) / (d))

#define DMA_BIT_MASK(n)    ((1ULL << (n)) - 1)

#define BITS_PER_LONG      (__SIZEOF_LONG__ * 8)
#define BITS_PER_LONG_LONG (__SIZEOF_LONG_LONG__ * 8)

#define GENMASK(h, l) \
	((~0UL << (l)) & (~0UL >> (BITS_PER_LONG - (h) - 1)))

#define GENMASK_ULL(h, l) \
	((~0ULL << (l)) & (~0ULL >> (BITS_PER_LONG_LONG - (h) - 1)))

#define __bf_shf(x) (__builtin_ffsll(x) - 1)

#define FIELD_GET(_mask, _reg) \
	(__extension__ ({ \
		typeof(_mask) _x = (_mask); \
		(typeof(_x))(((_reg) & (_x)) >> __bf_shf(_x)); \
	}))

#define FIELD_FIT(_mask, _val) \
	(__extension__ ({ \
		typeof(_mask) _x = (_mask); \
		!((((typeof(_x))_val) << __bf_shf(_x)) & ~(_x)); \
	}))

#define FIELD_PREP(_mask, _val) \
	(__extension__ ({ \
		typeof(_mask) _x = (_mask); \
		((typeof(_x))(_val) << __bf_shf(_x)) & (_x); \
	}))

#endif /* __NFP_PLATFORM_H__ */
