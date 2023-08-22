/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 * Copyright(c) 2019 Arm Limited
 */

#ifndef _RTE_PAUSE_ARM64_H_
#define _RTE_PAUSE_ARM64_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>

#ifdef RTE_ARM_USE_WFE
#define RTE_WAIT_UNTIL_EQUAL_ARCH_DEFINED
#endif

#include "generic/rte_pause.h"

static inline void rte_pause(void)
{
	asm volatile("yield" ::: "memory");
}

#ifdef RTE_WAIT_UNTIL_EQUAL_ARCH_DEFINED

/* Send a local event to quit WFE. */
#define __RTE_ARM_SEVL() { asm volatile("sevl" : : : "memory"); }

/* Send a global event to quit WFE for all cores. */
#define __RTE_ARM_SEV() { asm volatile("sev" : : : "memory"); }

/* Put processor into low power WFE(Wait For Event) state. */
#define __RTE_ARM_WFE() { asm volatile("wfe" : : : "memory"); }

/*
 * Atomic exclusive load from addr, it returns the 8-bit content of
 * *addr while making it 'monitored', when it is written by someone
 * else, the 'monitored' state is cleared and an event is generated
 * implicitly to exit WFE.
 */
#define __RTE_ARM_LOAD_EXC_8(src, dst, memorder) {       \
	if (memorder == rte_memory_order_relaxed) {       \
		asm volatile("ldxrb %w[tmp], [%x[addr]]"  \
			: [tmp] "=&r" (dst)               \
			: [addr] "r" (src)                \
			: "memory");                      \
	} else {                                          \
		asm volatile("ldaxrb %w[tmp], [%x[addr]]" \
			: [tmp] "=&r" (dst)               \
			: [addr] "r" (src)                \
			: "memory");                      \
	} }

/*
 * Atomic exclusive load from addr, it returns the 16-bit content of
 * *addr while making it 'monitored', when it is written by someone
 * else, the 'monitored' state is cleared and an event is generated
 * implicitly to exit WFE.
 */
#define __RTE_ARM_LOAD_EXC_16(src, dst, memorder) {       \
	if (memorder == rte_memory_order_relaxed) {       \
		asm volatile("ldxrh %w[tmp], [%x[addr]]"  \
			: [tmp] "=&r" (dst)               \
			: [addr] "r" (src)                \
			: "memory");                      \
	} else {                                          \
		asm volatile("ldaxrh %w[tmp], [%x[addr]]" \
			: [tmp] "=&r" (dst)               \
			: [addr] "r" (src)                \
			: "memory");                      \
	} }

/*
 * Atomic exclusive load from addr, it returns the 32-bit content of
 * *addr while making it 'monitored', when it is written by someone
 * else, the 'monitored' state is cleared and an event is generated
 * implicitly to exit WFE.
 */
#define __RTE_ARM_LOAD_EXC_32(src, dst, memorder) {      \
	if (memorder == rte_memory_order_relaxed) {      \
		asm volatile("ldxr %w[tmp], [%x[addr]]"  \
			: [tmp] "=&r" (dst)              \
			: [addr] "r" (src)               \
			: "memory");                     \
	} else {                                         \
		asm volatile("ldaxr %w[tmp], [%x[addr]]" \
			: [tmp] "=&r" (dst)              \
			: [addr] "r" (src)               \
			: "memory");                     \
	} }

/*
 * Atomic exclusive load from addr, it returns the 64-bit content of
 * *addr while making it 'monitored', when it is written by someone
 * else, the 'monitored' state is cleared and an event is generated
 * implicitly to exit WFE.
 */
#define __RTE_ARM_LOAD_EXC_64(src, dst, memorder) {      \
	if (memorder == rte_memory_order_relaxed) {      \
		asm volatile("ldxr %x[tmp], [%x[addr]]"  \
			: [tmp] "=&r" (dst)              \
			: [addr] "r" (src)               \
			: "memory");                     \
	} else {                                         \
		asm volatile("ldaxr %x[tmp], [%x[addr]]" \
			: [tmp] "=&r" (dst)              \
			: [addr] "r" (src)               \
			: "memory");                     \
	} }

/*
 * Atomic exclusive load from addr, it returns the 128-bit content of
 * *addr while making it 'monitored', when it is written by someone
 * else, the 'monitored' state is cleared and an event is generated
 * implicitly to exit WFE.
 */
#define __RTE_ARM_LOAD_EXC_128(src, dst, memorder) {                    \
	volatile rte_int128_t *dst_128 = (volatile rte_int128_t *)&dst; \
	if (memorder == rte_memory_order_relaxed) {                     \
		asm volatile("ldxp %x[tmp0], %x[tmp1], [%x[addr]]"      \
			: [tmp0] "=&r" (dst_128->val[0]),               \
			  [tmp1] "=&r" (dst_128->val[1])                \
			: [addr] "r" (src)                              \
			: "memory");                                    \
	} else {                                                        \
		asm volatile("ldaxp %x[tmp0], %x[tmp1], [%x[addr]]"     \
			: [tmp0] "=&r" (dst_128->val[0]),               \
			  [tmp1] "=&r" (dst_128->val[1])                \
			: [addr] "r" (src)                              \
			: "memory");                                    \
	} }                                                             \

#define __RTE_ARM_LOAD_EXC(src, dst, memorder, size) {     \
	RTE_BUILD_BUG_ON(size != 8 && size != 16 &&        \
		size != 32 && size != 64 && size != 128);  \
	if (size == 8)                                    \
		__RTE_ARM_LOAD_EXC_8(src, dst, memorder)   \
	else if (size == 16)                               \
		__RTE_ARM_LOAD_EXC_16(src, dst, memorder)  \
	else if (size == 32)                               \
		__RTE_ARM_LOAD_EXC_32(src, dst, memorder)  \
	else if (size == 64)                               \
		__RTE_ARM_LOAD_EXC_64(src, dst, memorder)  \
	else if (size == 128)                              \
		__RTE_ARM_LOAD_EXC_128(src, dst, memorder) \
}

static __rte_always_inline void
rte_wait_until_equal_16(volatile uint16_t *addr, uint16_t expected,
		int memorder)
{
	uint16_t value;

	RTE_BUILD_BUG_ON(memorder != rte_memory_order_acquire &&
		memorder != rte_memory_order_relaxed);

	__RTE_ARM_LOAD_EXC_16(addr, value, memorder)
	if (value != expected) {
		__RTE_ARM_SEVL()
		do {
			__RTE_ARM_WFE()
			__RTE_ARM_LOAD_EXC_16(addr, value, memorder)
		} while (value != expected);
	}
}

static __rte_always_inline void
rte_wait_until_equal_32(volatile uint32_t *addr, uint32_t expected,
		int memorder)
{
	uint32_t value;

	RTE_BUILD_BUG_ON(memorder != rte_memory_order_acquire &&
		memorder != rte_memory_order_relaxed);

	__RTE_ARM_LOAD_EXC_32(addr, value, memorder)
	if (value != expected) {
		__RTE_ARM_SEVL()
		do {
			__RTE_ARM_WFE()
			__RTE_ARM_LOAD_EXC_32(addr, value, memorder)
		} while (value != expected);
	}
}

static __rte_always_inline void
rte_wait_until_equal_64(volatile uint64_t *addr, uint64_t expected,
		int memorder)
{
	uint64_t value;

	RTE_BUILD_BUG_ON(memorder != rte_memory_order_acquire &&
		memorder != rte_memory_order_relaxed);

	__RTE_ARM_LOAD_EXC_64(addr, value, memorder)
	if (value != expected) {
		__RTE_ARM_SEVL()
		do {
			__RTE_ARM_WFE()
			__RTE_ARM_LOAD_EXC_64(addr, value, memorder)
		} while (value != expected);
	}
}

#define RTE_WAIT_UNTIL_MASKED(addr, mask, cond, expected, memorder) do {  \
	RTE_BUILD_BUG_ON(!__builtin_constant_p(memorder));                \
	RTE_BUILD_BUG_ON(memorder != rte_memory_order_acquire &&          \
		memorder != rte_memory_order_relaxed);                    \
	const uint32_t size = sizeof(*(addr)) << 3;                       \
	typeof(*(addr)) expected_value = (expected);                      \
	typeof(*(addr)) value;                                            \
	__RTE_ARM_LOAD_EXC((addr), value, memorder, size)                 \
	if (!((value & (mask)) cond expected_value)) {                    \
		__RTE_ARM_SEVL()                                          \
		do {                                                      \
			__RTE_ARM_WFE()                                   \
			__RTE_ARM_LOAD_EXC((addr), value, memorder, size) \
		} while (!((value & (mask)) cond expected_value));        \
	}                                                                 \
} while (0)

#endif /* RTE_WAIT_UNTIL_EQUAL_ARCH_DEFINED */

#ifdef __cplusplus
}
#endif

#endif /* _RTE_PAUSE_ARM64_H_ */
