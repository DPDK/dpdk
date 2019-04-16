/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _OSDEP_RAW_GENERIC_H
#define _OSDEP_RAW_GENERIC_H

#define	compiler_barrier() (asm volatile ("" : : : "memory"))

#define io_wmb() compiler_barrier()
#define io_rmb() compiler_barrier()

static inline uint8_t opae_readb(const volatile void *addr)
{
	uint8_t val;

	val = *(const volatile uint8_t *)addr;
	io_rmb();
	return val;
}

static inline uint16_t opae_readw(const volatile void *addr)
{
	uint16_t val;

	val = *(const volatile uint16_t *)addr;
	io_rmb();
	return val;
}

static inline uint32_t opae_readl(const volatile void *addr)
{
	uint32_t val;

	val = *(const volatile uint32_t *)addr;
	io_rmb();
	return val;
}

static inline uint64_t opae_readq(const volatile void *addr)
{
	uint64_t val;

	val = *(const volatile uint64_t *)addr;
	io_rmb();
	return val;
}

static inline void opae_writeb(uint8_t value, volatile void *addr)
{
	io_wmb();
	*(volatile uint8_t *)addr = value;
}

static inline void opae_writew(uint16_t value, volatile void *addr)
{
	io_wmb();
	*(volatile uint16_t *)addr = value;
}

static inline void opae_writel(uint32_t value, volatile void *addr)
{
	io_wmb();
	*(volatile uint32_t *)addr = value;
}

static inline void opae_writeq(uint64_t value, volatile void *addr)
{
	io_wmb();
	*(volatile uint64_t *)addr = value;
}

#define opae_free(addr) free(addr)
#define opae_memcpy(a, b, c) memcpy((a), (b), (c))

#endif
