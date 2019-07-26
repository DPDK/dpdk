/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _OTX2_IO_ARM64_H_
#define _OTX2_IO_ARM64_H_

#define otx2_load_pair(val0, val1, addr) ({		\
	asm volatile(					\
	"ldp %x[x0], %x[x1], [%x[p1]]"			\
	:[x0]"=r"(val0), [x1]"=r"(val1)			\
	:[p1]"r"(addr)					\
	); })

#define otx2_store_pair(val0, val1, addr) ({		\
	asm volatile(					\
	"stp %x[x0], %x[x1], [%x[p1],#0]!"		\
	::[x0]"r"(val0), [x1]"r"(val1), [p1]"r"(addr)	\
	); })

#define otx2_prefetch_store_keep(ptr) ({\
	asm volatile("prfm pstl1keep, [%x0]\n" : : "r" (ptr)); })

static __rte_always_inline uint64_t
otx2_atomic64_add_nosync(int64_t incr, int64_t *ptr)
{
	uint64_t result;

	/* Atomic add with no ordering */
	asm volatile (
		".cpu  generic+lse\n"
		"ldadd %x[i], %x[r], [%[b]]"
		: [r] "=r" (result), "+m" (*ptr)
		: [i] "r" (incr), [b] "r" (ptr)
		: "memory");
	return result;
}

static __rte_always_inline uint64_t
otx2_atomic64_add_sync(int64_t incr, int64_t *ptr)
{
	uint64_t result;

	/* Atomic add with ordering */
	asm volatile (
		".cpu  generic+lse\n"
		"ldadda %x[i], %x[r], [%[b]]"
		: [r] "=r" (result), "+m" (*ptr)
		: [i] "r" (incr), [b] "r" (ptr)
		: "memory");
	return result;
}

static __rte_always_inline uint64_t
otx2_lmt_submit(rte_iova_t io_address)
{
	uint64_t result;

	asm volatile (
		".cpu  generic+lse\n"
		"ldeor xzr,%x[rf],[%[rs]]" :
		 [rf] "=r"(result): [rs] "r"(io_address));
	return result;
}

static __rte_always_inline void
otx2_lmt_mov(void *out, const void *in, const uint32_t lmtext)
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

static __rte_always_inline void
otx2_lmt_mov_seg(void *out, const void *in, const uint16_t segdw)
{
	volatile const __uint128_t *src128 = (const __uint128_t *)in;
	volatile __uint128_t *dst128 = (__uint128_t *)out;
	uint8_t i;

	for (i = 0; i < segdw; i++)
		dst128[i] = src128[i];
}

#endif /* _OTX2_IO_ARM64_H_ */
