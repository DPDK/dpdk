/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2018-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_EF100_H
#define _SFC_EF100_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Prime event queue to allow processed events to be reused.
 *
 * @param evq_prime	Global address of the prime register
 * @param evq_hw_index	Event queue index
 * @param evq_read_ptr	Masked event qeueu read pointer
 */
static inline void
sfc_ef100_evq_prime(volatile void *evq_prime, unsigned int evq_hw_index,
		    unsigned int evq_read_ptr)
{
	efx_dword_t dword;

	EFX_POPULATE_DWORD_2(dword,
			     ERF_GZ_EVQ_ID, evq_hw_index,
			     ERF_GZ_IDX, evq_read_ptr);

	/*
	 * EvQ prime on EF100 allows HW to reuse descriptors. So we
	 * should be sure that event descriptor reads are done.
	 * However, there is implicit data dependency here since we
	 * move past event if we have found out that the event has
	 * come (i.e. we read it) and we have processed it.
	 * So, no extra barriers are required here.
	 */
	rte_write32_relaxed(dword.ed_u32[0], evq_prime);
}

static inline bool
sfc_ef100_ev_present(const efx_qword_t *ev, bool phase_bit)
{
	return !((ev->eq_u64[0] &
		  EFX_INPLACE_MASK64(0, 63, ESF_GZ_EV_EVQ_PHASE)) ^
		 ((uint64_t)phase_bit << ESF_GZ_EV_EVQ_PHASE_LBN));
}

static inline bool
sfc_ef100_ev_type_is(const efx_qword_t *ev, unsigned int type)
{
	return (ev->eq_u64[0] & EFX_INPLACE_MASK64(0, 63, ESF_GZ_E_TYPE)) ==
		EFX_INSERT_FIELD64(0, 63, ESF_GZ_E_TYPE, type);
}

#ifdef __cplusplus
}
#endif
#endif /* _SFC_EF100_H */
