/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#ifndef _ROC_SSO_DP_H_
#define _ROC_SSO_DP_H_

static __plt_always_inline uint64_t
roc_sso_hws_head_wait(uintptr_t base)
{
	uintptr_t tag_op = base + SSOW_LF_GWS_TAG;
	uint64_t tag;

#if defined(__aarch64__)
	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "		ldr %[tag], [%[tag_op]]		\n"
		     "		tbnz %[tag], 35, .Ldone%=	\n"
		     "		sevl				\n"
		     ".Lrty%=:	wfe				\n"
		     "		ldr %[tag], [%[tag_op]]		\n"
		     "		tbz %[tag], 35, .Lrty%=		\n"
		     ".Ldone%=:					\n"
		     : [tag] "=&r"(tag)
		     : [tag_op] "r"(tag_op));
#else
	do {
		tag = plt_read64(tag_op);
	} while (!(tag & BIT_ULL(35)));
#endif
	return tag;
}

static __plt_always_inline uint8_t
roc_sso_hws_is_head(uintptr_t base)
{
	uintptr_t tag_op = base + SSOW_LF_GWS_TAG;

	return !!(plt_read64(tag_op) & BIT_ULL(35));
}

#endif /* _ROC_SSO_DP_H_ */
