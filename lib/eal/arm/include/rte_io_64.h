/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#ifndef _RTE_IO_ARM64_H_
#define _RTE_IO_ARM64_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define RTE_OVERRIDE_IO_H

#include "generic/rte_io.h"
#include "rte_atomic_64.h"

static __rte_always_inline uint8_t
rte_read8_relaxed(const volatile void *addr)
{
	uint8_t val;

	asm volatile(
		    "ldrb %w[val], [%x[addr]]"
		    : [val] "=r" (val)
		    : [addr] "r" (addr));
	return val;
}

static __rte_always_inline uint16_t
rte_read16_relaxed(const volatile void *addr)
{
	uint16_t val;

	asm volatile(
		    "ldrh %w[val], [%x[addr]]"
		    : [val] "=r" (val)
		    : [addr] "r" (addr));
	return val;
}

static __rte_always_inline uint32_t
rte_read32_relaxed(const volatile void *addr)
{
	uint32_t val;

	asm volatile(
		    "ldr %w[val], [%x[addr]]"
		    : [val] "=r" (val)
		    : [addr] "r" (addr));
	return val;
}

static __rte_always_inline uint64_t
rte_read64_relaxed(const volatile void *addr)
{
	uint64_t val;

	asm volatile(
		    "ldr %x[val], [%x[addr]]"
		    : [val] "=r" (val)
		    : [addr] "r" (addr));
	return val;
}

static __rte_always_inline void
rte_write8_relaxed(uint8_t val, volatile void *addr)
{
	asm volatile(
		    "strb %w[val], [%x[addr]]"
		    :
		    : [val] "r" (val), [addr] "r" (addr));
}

static __rte_always_inline void
rte_write16_relaxed(uint16_t val, volatile void *addr)
{
	asm volatile(
		    "strh %w[val], [%x[addr]]"
		    :
		    : [val] "r" (val), [addr] "r" (addr));
}

static __rte_always_inline void
rte_write32_relaxed(uint32_t val, volatile void *addr)
{
	asm volatile(
		    "str %w[val], [%x[addr]]"
		    :
		    : [val] "r" (val), [addr] "r" (addr));
}

static __rte_always_inline void
rte_write64_relaxed(uint64_t val, volatile void *addr)
{
	asm volatile(
		    "str %x[val], [%x[addr]]"
		    :
		    : [val] "r" (val), [addr] "r" (addr));
}

static __rte_always_inline uint8_t
rte_read8(const volatile void *addr)
{
	uint8_t val;
	val = rte_read8_relaxed(addr);
	rte_io_rmb();
	return val;
}

static __rte_always_inline uint16_t
rte_read16(const volatile void *addr)
{
	uint16_t val;
	val = rte_read16_relaxed(addr);
	rte_io_rmb();
	return val;
}

static __rte_always_inline uint32_t
rte_read32(const volatile void *addr)
{
	uint32_t val;
	val = rte_read32_relaxed(addr);
	rte_io_rmb();
	return val;
}

static __rte_always_inline uint64_t
rte_read64(const volatile void *addr)
{
	uint64_t val;
	val = rte_read64_relaxed(addr);
	rte_io_rmb();
	return val;
}

static __rte_always_inline void
rte_write8(uint8_t value, volatile void *addr)
{
	rte_io_wmb();
	rte_write8_relaxed(value, addr);
}

static __rte_always_inline void
rte_write16(uint16_t value, volatile void *addr)
{
	rte_io_wmb();
	rte_write16_relaxed(value, addr);
}

static __rte_always_inline void
rte_write32(uint32_t value, volatile void *addr)
{
	rte_io_wmb();
	rte_write32_relaxed(value, addr);
}

static __rte_always_inline void
rte_write64(uint64_t value, volatile void *addr)
{
	rte_io_wmb();
	rte_write64_relaxed(value, addr);
}

__rte_experimental
static __rte_always_inline void
rte_write32_wc(uint32_t value, volatile void *addr)
{
	rte_write32(value, addr);
}

__rte_experimental
static __rte_always_inline void
rte_write32_wc_relaxed(uint32_t value, volatile void *addr)
{
	rte_write32_relaxed(value, addr);
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_IO_ARM64_H_ */
