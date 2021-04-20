/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) Mellanox 2020.
 */

#ifndef _RTE_ALTIVEC_H_
#define _RTE_ALTIVEC_H_

/* To include altivec.h, GCC version must be >= 4.8 */
#include <altivec.h>

/*
 * Compilation workaround for PPC64 when AltiVec is fully enabled, e.g. std=c11.
 * Otherwise there would be a type conflict between stdbool and altivec.
 */
#if defined(__PPC64__) && !defined(__APPLE_ALTIVEC__)
#undef bool
/* redefine as in stdbool.h */
#define bool _Bool
#endif

#endif /* _RTE_ALTIVEC_H_ */
