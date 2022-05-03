/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) IBM Corporation 2016.
 */

#ifndef _RTE_VECT_PPC_64_H_
#define _RTE_VECT_PPC_64_H_

#include "rte_altivec.h"

#include "generic/rte_vect.h"
#include "rte_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_VECT_DEFAULT_SIMD_BITWIDTH RTE_VECT_SIMD_256

typedef __vector signed int xmm_t;

#define	XMM_SIZE	(sizeof(xmm_t))
#define	XMM_MASK	(XMM_SIZE - 1)

typedef union rte_xmm {
	xmm_t    x;
	uint8_t  u8[XMM_SIZE / sizeof(uint8_t)];
	uint16_t u16[XMM_SIZE / sizeof(uint16_t)];
	uint32_t u32[XMM_SIZE / sizeof(uint32_t)];
	uint64_t u64[XMM_SIZE / sizeof(uint64_t)];
	double   pd[XMM_SIZE / sizeof(double)];
} __rte_aligned(16) rte_xmm_t;

#ifdef __cplusplus
}
#endif

#endif /* _RTE_VECT_PPC_64_H_ */
