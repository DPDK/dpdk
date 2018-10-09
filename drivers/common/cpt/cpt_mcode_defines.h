/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#ifndef _CPT_MCODE_DEFINES_H_
#define _CPT_MCODE_DEFINES_H_

/*
 * This file defines macros and structures according to microcode spec
 *
 */

#define CPT_BYTE_16		16
#define CPT_BYTE_24		24
#define CPT_BYTE_32		32
#define CPT_MAX_SG_IN_OUT_CNT	32
#define CPT_MAX_SG_CNT		(CPT_MAX_SG_IN_OUT_CNT/2)

#define COMPLETION_CODE_SIZE	8
#define COMPLETION_CODE_INIT	0

#define SG_LIST_HDR_SIZE	(8u)
#define SG_ENTRY_SIZE		sizeof(sg_comp_t)

/* #define CPT_ALWAYS_USE_SG_MODE */
#define CPT_ALWAYS_USE_SEPARATE_BUF

typedef struct sglist_comp {
	union {
		uint64_t len;
		struct {
			uint16_t len[4];
		} s;
	} u;
	uint64_t ptr[4];
} sg_comp_t;

#endif /* _CPT_MCODE_DEFINES_H_ */
