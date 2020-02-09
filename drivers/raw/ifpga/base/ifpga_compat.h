/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _IFPGA_COMPAT_H_
#define _IFPGA_COMPAT_H_

#include "opae_osdep.h"

#undef container_of
#define container_of(ptr, type, member) ({ \
		typeof(((type *)0)->member)(*__mptr) = (ptr); \
		(type *)((char *)__mptr - offsetof(type, member)); })

#define IFPGA_PAGE_SHIFT       12
#define IFPGA_PAGE_SIZE        (1 << IFPGA_PAGE_SHIFT)
#define IFPGA_PAGE_MASK        (~(IFPGA_PAGE_SIZE - 1))
#define IFPGA_PAGE_ALIGN(addr) (((addr) + IFPGA_PAGE_SIZE - 1)\
		& IFPGA_PAGE_MASK)
#define IFPGA_ALIGN(x, a)  (((x) + (a) - 1) & ~((a) - 1))

#define IS_ALIGNED(x, a)		(((x) & ((typeof(x))(a) - 1)) == 0)
#define PAGE_ALIGNED(addr) IS_ALIGNED((unsigned long)(addr), IFPGA_PAGE_SIZE)

#define readl(addr) opae_readl(addr)
#define readq(addr) opae_readq(addr)
#define writel(value, addr) opae_writel(value, addr)
#define writeq(value, addr) opae_writeq(value, addr)

#define malloc(size) opae_malloc(size)
#define zmalloc(size) opae_zmalloc(size)
#define free(size) opae_free(size)

/*
 * Wait register's _field to be changed to the given value (_expect's _field)
 * by polling with given interval and timeout.
 */
#define fpga_wait_register_field(_field, _expect, _reg_addr, _timeout, _invl)\
({									     \
	int wait = 0;							     \
	int ret = -ETIMEDOUT;						     \
	typeof(_expect) value;						     \
	for (; wait <= _timeout; wait += _invl) {			     \
		value.csr = readq(_reg_addr);				     \
		if (_expect._field == value._field) {			     \
			ret = 0;					     \
			break;						     \
		}							     \
		udelay(_invl);						     \
	}								     \
	ret;								     \
})

#define __maybe_unused __rte_unused

#define UNUSED(x)	(void)(x)

#endif /* _IFPGA_COMPAT_H_ */
