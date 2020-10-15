/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _OPAE_OSDEP_H
#define _OPAE_OSDEP_H

#include <string.h>
#include <stdbool.h>

#ifdef RTE_LIB_EAL
#include "osdep_rte/osdep_generic.h"
#else
#include "osdep_raw/osdep_generic.h"
#endif

#define __iomem

typedef uint8_t		u8;
typedef int8_t		s8;
typedef uint16_t	u16;
typedef uint32_t	u32;
typedef int32_t		s32;
typedef uint64_t	u64;
typedef uint64_t	dma_addr_t;

struct uuid {
	u8 b[16];
};

#ifndef LINUX_MACROS
#ifndef BITS_PER_LONG
#define BITS_PER_LONG	(__SIZEOF_LONG__ * 8)
#endif
#ifndef BITS_PER_LONG_LONG
#define BITS_PER_LONG_LONG  (__SIZEOF_LONG_LONG__ * 8)
#endif
#ifndef BIT
#define BIT(a) (1UL << (a))
#endif /* BIT */
#ifndef BIT_ULL
#define BIT_ULL(a) (1ULL << (a))
#endif /* BIT_ULL */
#ifndef GENMASK
#define GENMASK(h, l)	(((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
#endif /* GENMASK */
#ifndef GENMASK_ULL
#define GENMASK_ULL(h, l) \
	(((~0ULL) << (l)) & (~0ULL >> (BITS_PER_LONG_LONG - 1 - (h))))
#endif /* GENMASK_ULL */
#endif /* LINUX_MACROS */

#define SET_FIELD(m, v) (((v) << (__builtin_ffsll(m) - 1)) & (m))
#define GET_FIELD(m, v) (((v) & (m)) >> (__builtin_ffsll(m) - 1))

#define dev_err(x, args...) dev_printf(ERR, args)
#define dev_info(x, args...) dev_printf(INFO, args)
#define dev_warn(x, args...) dev_printf(WARNING, args)
#define dev_debug(x, args...) dev_printf(DEBUG, args)

#define pr_err(y, args...) dev_err(0, y, ##args)
#define pr_warn(y, args...) dev_warn(0, y, ##args)
#define pr_info(y, args...) dev_info(0, y, ##args)

#ifndef WARN_ON
#define WARN_ON(x) do { \
	int ret = !!(x); \
	if (unlikely(ret)) \
		pr_warn("WARN_ON: \"" #x "\" at %s:%d\n", __func__, __LINE__); \
} while (0)
#endif

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#define udelay(x) opae_udelay(x)
#define msleep(x) opae_udelay(1000 * (x))
#define usleep_range(min, max) msleep(DIV_ROUND_UP(min, 1000))

#define time_after(a, b)	((long)((b) - (a)) < 0)
#define time_before(a, b)	time_after(b, a)
#define opae_memset(a, b, c)    memset((a), (b), (c))

#define opae_readq_poll_timeout(addr, val, cond, invl, timeout)\
({									     \
	int wait = 0;							     \
	for (; wait <= timeout; wait += invl) {			     \
		(val) = opae_readq(addr);				     \
		if (cond)                  \
			break;						     \
		udelay(invl);						     \
	}								     \
	(cond) ? 0 : -ETIMEDOUT;	  \
})
#endif
