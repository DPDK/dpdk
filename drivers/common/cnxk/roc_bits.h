/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_BITS_H_
#define _ROC_BITS_H_

#ifndef BIT_ULL
#define BIT_ULL(nr) (1ULL << (nr))
#endif

#ifndef BIT
#define BIT(nr) (1UL << (nr))
#endif

#ifndef BITS_PER_LONG
#define BITS_PER_LONG (__SIZEOF_LONG__ * 8)
#endif
#ifndef BITS_PER_LONG_LONG
#define BITS_PER_LONG_LONG (__SIZEOF_LONG_LONG__ * 8)
#endif

#ifndef GENMASK
#define GENMASK(h, l) (((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
#endif
#ifndef GENMASK_ULL
#define GENMASK_ULL(h, l)                                                      \
	(((~0ULL) - (1ULL << (l)) + 1) &                                       \
	 (~0ULL >> (BITS_PER_LONG_LONG - 1 - (h))))
#endif

#endif /* _ROC_BITS_H_ */
