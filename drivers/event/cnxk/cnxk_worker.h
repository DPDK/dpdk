/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CNXK_WORKER_H__
#define __CNXK_WORKER_H__

#if defined(__aarch64__)
#include "roc_io.h"
#else
#include "roc_io_generic.h"
#endif

#include "cnxk_eventdev_dp.h"
#include "hw/ssow.h"

/* SSO Operations */

static __rte_always_inline void
cnxk_sso_hws_add_work(const uint64_t event_ptr, const uint32_t tag,
		      const uint8_t new_tt, const uintptr_t grp_base)
{
	uint64_t add_work0;

	add_work0 = tag | ((uint64_t)(new_tt) << 32);
	roc_store_pair(add_work0, event_ptr, grp_base);
}

static __rte_always_inline void
cnxk_sso_hws_swtag_desched(uint32_t tag, uint8_t new_tt, uint16_t grp,
			   uintptr_t swtag_desched_op)
{
	uint64_t val;

	val = tag | ((uint64_t)(new_tt & 0x3) << 32) | ((uint64_t)grp << 34);
	__atomic_store_n((uint64_t *)swtag_desched_op, val, __ATOMIC_RELEASE);
}

static __rte_always_inline void
cnxk_sso_hws_swtag_norm(uint32_t tag, uint8_t new_tt, uintptr_t swtag_norm_op)
{
	uint64_t val;

	val = tag | ((uint64_t)(new_tt & 0x3) << 32);
	plt_write64(val, swtag_norm_op);
}

static __rte_always_inline void
cnxk_sso_hws_swtag_untag(uintptr_t swtag_untag_op)
{
	plt_write64(0, swtag_untag_op);
}

static __rte_always_inline void
cnxk_sso_hws_swtag_flush(uint64_t base)
{
	/* Ensure that there is no previous flush is pending. */
	while (plt_read64(base + SSOW_LF_GWS_PENDSTATE) & BIT_ULL(56))
		;
	if (CNXK_TT_FROM_TAG(plt_read64(base + SSOW_LF_GWS_TAG)) ==
	    SSO_TT_EMPTY)
		return;
	plt_write64(0, base + SSOW_LF_GWS_OP_SWTAG_FLUSH);
}

static __rte_always_inline uint64_t
cnxk_sso_hws_swtag_wait(uintptr_t tag_op)
{
	uint64_t swtp;
#ifdef RTE_ARCH_ARM64

	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "		ldr %[swtb], [%[swtp_loc]]	\n"
		     "		tbz %[swtb], 62, .Ldone%=	\n"
		     "		sevl				\n"
		     ".Lrty%=:	wfe				\n"
		     "		ldr %[swtb], [%[swtp_loc]]	\n"
		     "		tbnz %[swtb], 62, .Lrty%=	\n"
		     ".Ldone%=:					\n"
		     : [swtb] "=&r"(swtp)
		     : [swtp_loc] "r"(tag_op));
#else
	/* Wait for the SWTAG/SWTAG_FULL operation */
	do {
		swtp = plt_read64(tag_op);
	} while (swtp & BIT_ULL(62));
#endif

	return swtp;
}

static __rte_always_inline void
cnxk_sso_hws_desched(uint64_t u64, uint64_t base)
{
	plt_write64(u64, base + SSOW_LF_GWS_OP_UPD_WQP_GRP1);
	plt_write64(0, base + SSOW_LF_GWS_OP_DESCHED);
}

#endif
