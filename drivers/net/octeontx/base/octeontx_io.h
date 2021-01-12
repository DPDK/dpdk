/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef __OCTEONTX_IO_H__
#define __OCTEONTX_IO_H__

#include <stddef.h>
#include <stdint.h>

#include <rte_io.h>

/* In Cavium OCTEON TX SoC, all accesses to the device registers are
 * implicitly strongly ordered. So, The relaxed version of IO operation is
 * safe to use with out any IO memory barriers.
 */
#define octeontx_read64 rte_read64_relaxed
#define octeontx_write64 rte_write64_relaxed

/* ARM64 specific functions */
#if defined(RTE_ARCH_ARM64)
#define octeontx_prefetch_store_keep(_ptr) ({\
	asm volatile("prfm pstl1keep, %a0\n" : : "p" (_ptr)); })

#define octeontx_load_pair(val0, val1, addr) ({		\
			asm volatile(			\
			"ldp %x[x0], %x[x1], [%x[p1]]"	\
			:[x0]"=r"(val0), [x1]"=r"(val1) \
			:[p1]"r"(addr)			\
			); })

#define octeontx_store_pair(val0, val1, addr) ({		\
			asm volatile(			\
			"stp %x[x0], %x[x1], [%x[p1]]"	\
			::[x0]"r"(val0), [x1]"r"(val1), [p1]"r"(addr) \
			); })
#else /* Un optimized functions for building on non arm64 arch */

#define octeontx_prefetch_store_keep(_ptr) do {} while (0)

#define octeontx_load_pair(val0, val1, addr)		\
do {							\
	val0 = rte_read64(addr);			\
	val1 = rte_read64(((uint8_t *)addr) + 8);	\
} while (0)

#define octeontx_store_pair(val0, val1, addr)		\
do {							\
	rte_write64(val0, addr);			\
	rte_write64(val1, (((uint8_t *)addr) + 8));	\
} while (0)
#endif

#if defined(RTE_ARCH_ARM64)
#if defined(__ARM_FEATURE_SVE)
#define __LSE_PREAMBLE " .cpu	generic+lse+sve\n"
#else
#define __LSE_PREAMBLE " .cpu	generic+lse\n"
#endif
/**
 * Perform an atomic fetch-and-add operation.
 */
static inline uint64_t
octeontx_reg_ldadd_u64(void *addr, int64_t off)
{
	uint64_t old_val;

	__asm__ volatile(
		__LSE_PREAMBLE
		" ldadd	%1, %0, [%2]\n"
		: "=r" (old_val) : "r" (off), "r" (addr) : "memory");

	return old_val;
}

/**
 * Perform a LMTST operation - an atomic write of up to 128 byte to
 * an I/O block that supports this operation type.
 *
 * @param lmtline_va is the address where LMTLINE is mapped
 * @param ioreg_va is the virtual address of the device register
 * @param cmdbuf is the array of peripheral commands to execute
 * @param cmdsize is the number of 64-bit words in 'cmdbuf'
 *
 * @return N/A
 */
static inline void
octeontx_reg_lmtst(void *lmtline_va, void *ioreg_va, const uint64_t cmdbuf[],
		   uint64_t cmdsize)
{
	uint64_t result;
	uint64_t word_count;
	uint64_t *lmtline = lmtline_va;

	word_count = cmdsize;

	do {
		/* Copy commands to LMTLINE */
		for (result = 0; result < word_count; result += 2) {
			lmtline[result + 0] = cmdbuf[result + 0];
			lmtline[result + 1] = cmdbuf[result + 1];
		}

		/* LDEOR initiates atomic transfer to I/O device */
		__asm__ volatile(
			__LSE_PREAMBLE
			" ldeor	xzr, %0, [%1]\n"
			: "=r" (result) : "r" (ioreg_va) : "memory");
	} while (!result);
}

#undef __LSE_PREAMBLE
#else

static inline uint64_t
octeontx_reg_ldadd_u64(void *addr, int64_t off)
{
	RTE_SET_USED(addr);
	RTE_SET_USED(off);
	return 0;
}

static inline void
octeontx_reg_lmtst(void *lmtline_va, void *ioreg_va, const uint64_t cmdbuf[],
		   uint64_t cmdsize)
{
	RTE_SET_USED(lmtline_va);
	RTE_SET_USED(ioreg_va);
	RTE_SET_USED(cmdbuf);
	RTE_SET_USED(cmdsize);
}

#endif
#endif /* __OCTEONTX_IO_H__ */
