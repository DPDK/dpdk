/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_IO_H_
#define _ROC_IO_H_

#define ROC_LMT_BASE_ID_GET(lmt_addr, lmt_id)                                  \
	do {                                                                   \
		/* 32 Lines per core */                                        \
		lmt_id = plt_lcore_id() << ROC_LMT_LINES_PER_CORE_LOG2;        \
		/* Each line is of 128B */                                     \
		(lmt_addr) += ((uint64_t)lmt_id << ROC_LMT_LINE_SIZE_LOG2);    \
	} while (0)

#define ROC_LMT_CPT_BASE_ID_GET(lmt_addr, lmt_id)                              \
	do {                                                                   \
		/* 16 Lines per core */                                        \
		lmt_id = ROC_LMT_CPT_BASE_ID_OFF;                              \
		lmt_id += (plt_lcore_id() << ROC_LMT_CPT_LINES_PER_CORE_LOG2); \
		/* Each line is of 128B */                                     \
		(lmt_addr) += ((uint64_t)lmt_id << ROC_LMT_LINE_SIZE_LOG2);    \
	} while (0)

#define roc_load_pair(val0, val1, addr)                                        \
	({                                                                     \
		asm volatile("ldp %x[x0], %x[x1], [%x[p1]]"                    \
			     : [x0] "=r"(val0), [x1] "=r"(val1)                \
			     : [p1] "r"(addr));                                \
	})

#define roc_store_pair(val0, val1, addr)                                       \
	({                                                                     \
		asm volatile(                                                  \
			"stp %x[x0], %x[x1], [%x[p1], #0]!" ::[x0] "r"(val0),  \
			[x1] "r"(val1), [p1] "r"(addr));                       \
	})

#define roc_prefetch_store_keep(ptr)                                           \
	({ asm volatile("prfm pstl1keep, [%x0]\n" : : "r"(ptr)); })

#if defined(__clang__)
static __plt_always_inline void
roc_atomic128_cas_noreturn(uint64_t swap0, uint64_t swap1, int64_t *ptr)
{
	register uint64_t x0 __asm("x0") = swap0;
	register uint64_t x1 __asm("x1") = swap1;

	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "casp %[x0], %[x1], %[x0], %[x1], [%[ptr]]\n"
		     : [x0] "+r"(x0), [x1] "+r"(x1)
		     : [ptr] "r"(ptr)
		     : "memory");
}
#else
static __plt_always_inline void
roc_atomic128_cas_noreturn(uint64_t swap0, uint64_t swap1, uint64_t ptr)
{
	__uint128_t wdata = swap0 | ((__uint128_t)swap1 << 64);

	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "casp %[wdata], %H[wdata], %[wdata], %H[wdata], [%[ptr]]\n"
		     : [wdata] "+r"(wdata)
		     : [ptr] "r"(ptr)
		     : "memory");
}
#endif

static __plt_always_inline uint64_t
roc_atomic64_cas(uint64_t compare, uint64_t swap, int64_t *ptr)
{
	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "cas %[compare], %[swap], [%[ptr]]\n"
		     : [compare] "+r"(compare)
		     : [swap] "r"(swap), [ptr] "r"(ptr)
		     : "memory");

	return compare;
}

static __plt_always_inline uint64_t
roc_atomic64_add_nosync(int64_t incr, int64_t *ptr)
{
	uint64_t result;

	/* Atomic add with no ordering */
	asm volatile(PLT_CPU_FEATURE_PREAMBLE "ldadd %x[i], %x[r], [%[b]]"
		     : [r] "=r"(result), "+m"(*ptr)
		     : [i] "r"(incr), [b] "r"(ptr)
		     : "memory");
	return result;
}

static __plt_always_inline uint64_t
roc_atomic64_add_sync(int64_t incr, int64_t *ptr)
{
	uint64_t result;

	/* Atomic add with ordering */
	asm volatile(PLT_CPU_FEATURE_PREAMBLE "ldadda %x[i], %x[r], [%[b]]"
		     : [r] "=r"(result), "+m"(*ptr)
		     : [i] "r"(incr), [b] "r"(ptr)
		     : "memory");
	return result;
}

static __plt_always_inline uint64_t
roc_lmt_submit_ldeor(plt_iova_t io_address)
{
	uint64_t result;

	asm volatile(PLT_CPU_FEATURE_PREAMBLE "ldeor xzr, %x[rf], [%[rs]]"
		     : [rf] "=r"(result)
		     : [rs] "r"(io_address));
	return result;
}

static __plt_always_inline uint64_t
roc_lmt_submit_ldeorl(plt_iova_t io_address)
{
	uint64_t result;

	asm volatile(PLT_CPU_FEATURE_PREAMBLE "ldeorl xzr,%x[rf],[%[rs]]"
		     : [rf] "=r"(result)
		     : [rs] "r"(io_address));
	return result;
}

static __plt_always_inline void
roc_lmt_submit_steor(uint64_t data, plt_iova_t io_address)
{
	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "steor %x[d], [%[rs]]" ::[d] "r"(data),
		     [rs] "r"(io_address));
}

static __plt_always_inline void
roc_lmt_submit_steorl(uint64_t data, plt_iova_t io_address)
{
	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "steorl %x[d], [%[rs]]" ::[d] "r"(data),
		     [rs] "r"(io_address));
}

static __plt_always_inline void
roc_lmt_mov(void *out, const void *in, const uint32_t lmtext)
{
	volatile const __uint128_t *src128 = (const __uint128_t *)in;
	volatile __uint128_t *dst128 = (__uint128_t *)out;

	dst128[0] = src128[0];
	dst128[1] = src128[1];
	/* lmtext receives following value:
	 * 1: NIX_SUBDC_EXT needed i.e. tx vlan case
	 * 2: NIX_SUBDC_EXT + NIX_SUBDC_MEM i.e. tstamp case
	 */
	if (lmtext) {
		dst128[2] = src128[2];
		if (lmtext > 1)
			dst128[3] = src128[3];
	}
}

static __plt_always_inline void
roc_lmt_mov_seg(void *out, const void *in, const uint16_t segdw)
{
	volatile const __uint128_t *src128 = (const __uint128_t *)in;
	volatile __uint128_t *dst128 = (__uint128_t *)out;
	uint8_t i;

	for (i = 0; i < segdw; i++)
		dst128[i] = src128[i];
}

static __plt_always_inline void
roc_lmt_mov_one(void *out, const void *in)
{
	volatile const __uint128_t *src128 = (const __uint128_t *)in;
	volatile __uint128_t *dst128 = (__uint128_t *)out;

	*dst128 = *src128;
}

/* Non volatile version of roc_lmt_mov_seg() */
static __plt_always_inline void
roc_lmt_mov_seg_nv(void *out, const void *in, const uint16_t segdw)
{
	const __uint128_t *src128 = (const __uint128_t *)in;
	__uint128_t *dst128 = (__uint128_t *)out;
	uint8_t i;

	for (i = 0; i < segdw; i++)
		dst128[i] = src128[i];
}

static __plt_always_inline void
roc_atf_ret(void)
{
	/* This will allow wfi in EL0 to cause async exception to EL3
	 * which will optionally perform necessary actions.
	 */
	__asm("wfi");
}

#endif /* _ROC_IO_H_ */
