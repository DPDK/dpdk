/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2017-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_EF10_H
#define _SFC_EF10_H

#include "sfc_debug.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of events in one cache line */
#define SFC_EF10_EV_PER_CACHE_LINE \
	(RTE_CACHE_LINE_SIZE / sizeof(efx_qword_t))

#define SFC_EF10_EV_QCLEAR_MASK		(~(SFC_EF10_EV_PER_CACHE_LINE - 1))

/*
 * Use simple libefx-based implementation of the
 * sfc_ef10_ev_qclear_cache_line() if SSE2 is not available
 * since optimized implementation uses __m128i intrinsics.
 */
#ifndef __SSE2__
#define SFC_EF10_EV_QCLEAR_USE_EFX
#endif

#if defined(SFC_EF10_EV_QCLEAR_USE_EFX)
static inline void
sfc_ef10_ev_qclear_cache_line(void *ptr)
{
	efx_qword_t *entry = ptr;
	unsigned int i;

	for (i = 0; i < SFC_EF10_EV_PER_CACHE_LINE; ++i)
		EFX_SET_QWORD(entry[i]);
}
#else
/*
 * It is possible to do it using AVX2 and AVX512F, but it shows less
 * performance.
 */
static inline void
sfc_ef10_ev_qclear_cache_line(void *ptr)
{
	const efsys_uint128_t val = _mm_set1_epi64x(UINT64_MAX);
	efsys_uint128_t *addr = ptr;
	unsigned int i;

	RTE_BUILD_BUG_ON(sizeof(val) > RTE_CACHE_LINE_SIZE);
	RTE_BUILD_BUG_ON(RTE_CACHE_LINE_SIZE % sizeof(val) != 0);

	for (i = 0; i < RTE_CACHE_LINE_SIZE / sizeof(val); ++i)
		_mm_store_si128(&addr[i], val);
}
#endif

static inline void
sfc_ef10_ev_qclear(efx_qword_t *hw_ring, unsigned int ptr_mask,
		   unsigned int old_read_ptr, unsigned int read_ptr)
{
	const unsigned int clear_ptr = read_ptr & SFC_EF10_EV_QCLEAR_MASK;
	unsigned int old_clear_ptr = old_read_ptr & SFC_EF10_EV_QCLEAR_MASK;

	while (old_clear_ptr != clear_ptr) {
		sfc_ef10_ev_qclear_cache_line(
			&hw_ring[old_clear_ptr & ptr_mask]);
		old_clear_ptr += SFC_EF10_EV_PER_CACHE_LINE;
	}

	/*
	 * No barriers here.
	 * Functions which push doorbell should care about correct
	 * ordering: store instructions which fill in EvQ ring should be
	 * retired from CPU and DMA sync before doorbell which will allow
	 * to use these event entries.
	 */
}

static inline bool
sfc_ef10_ev_present(const efx_qword_t ev)
{
	return ~EFX_QWORD_FIELD(ev, EFX_DWORD_0) |
	       ~EFX_QWORD_FIELD(ev, EFX_DWORD_1);
}


/**
 * Alignment requirement for value written to RX WPTR:
 * the WPTR must be aligned to an 8 descriptor boundary.
 */
#define SFC_EF10_RX_WPTR_ALIGN	8u

static inline void
sfc_ef10_rx_qpush(volatile void *doorbell, unsigned int added,
		  unsigned int ptr_mask, uint32_t *dbell_counter)
{
	efx_dword_t dword;

	/* Hardware has alignment restriction for WPTR */
	RTE_BUILD_BUG_ON(SFC_RX_REFILL_BULK % SFC_EF10_RX_WPTR_ALIGN != 0);
	SFC_ASSERT(RTE_ALIGN(added, SFC_EF10_RX_WPTR_ALIGN) == added);

	EFX_POPULATE_DWORD_1(dword, ERF_DZ_RX_DESC_WPTR, added & ptr_mask);

	/* DMA sync to device is not required */

	/*
	 * rte_write32() has rte_io_wmb() which guarantees that the STORE
	 * operations (i.e. Rx and event descriptor updates) that precede
	 * the rte_io_wmb() call are visible to NIC before the STORE
	 * operations that follow it (i.e. doorbell write).
	 */
	rte_write32(dword.ed_u32[0], doorbell);
	(*dbell_counter)++;
}

static inline void
sfc_ef10_ev_qprime(volatile void *qprime, unsigned int read_ptr,
		  unsigned int ptr_mask)
{
	efx_dword_t dword;

	EFX_POPULATE_DWORD_1(dword, ERF_DZ_EVQ_RPTR, read_ptr & ptr_mask);

	rte_write32_relaxed(dword.ed_u32[0], qprime);
	rte_wmb();
}


const uint32_t *sfc_ef10_supported_ptypes_get(uint32_t tunnel_encaps,
					      size_t *no_of_elements);


#ifdef __cplusplus
}
#endif
#endif /* _SFC_EF10_H */
