/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Cavium, Inc
 */

#ifndef _RTE_CPUFLAGS_ARM64_H_
#define _RTE_CPUFLAGS_ARM64_H_

/**
 * Enumeration of all CPU features supported
 */
enum rte_cpu_flag_t {
	/* Floating point capability */
	RTE_CPUFLAG_FP = 0,

	/* Arm Neon extension */
	RTE_CPUFLAG_NEON,

	/* Generic timer event stream */
	RTE_CPUFLAG_EVTSTRM,

	/* AES instructions */
	RTE_CPUFLAG_AES,

	/* Polynomial multiply long instruction */
	RTE_CPUFLAG_PMULL,

	/* SHA1 instructions */
	RTE_CPUFLAG_SHA1,

	/* SHA2 instructions */
	RTE_CPUFLAG_SHA2,

	/* CRC32 instruction */
	RTE_CPUFLAG_CRC32,

	/*
	 * LDADD, LDCLR, LDEOR, LDSET, LDSMAX, LDSMIN, LDUMAX, LDUMIN, CAS,
	 * CASP, and SWP instructions
	 */
	RTE_CPUFLAG_ATOMICS,

	/* Arm SVE extension */
	RTE_CPUFLAG_SVE,

	/* Arm SVE2 extension */
	RTE_CPUFLAG_SVE2,

	/* SVE-AES instructions */
	RTE_CPUFLAG_SVEAES,

	/* SVE-PMULL instruction */
	RTE_CPUFLAG_SVEPMULL,

	/* SVE bit permute instructions */
	RTE_CPUFLAG_SVEBITPERM,

	/* SVE-SHA3 instructions */
	RTE_CPUFLAG_SVESHA3,

	/* SVE-SM4 instructions */
	RTE_CPUFLAG_SVESM4,

	/* CFINV, RMIF, SETF16, SETF8, AXFLAG, and XAFLAG instructions */
	RTE_CPUFLAG_FLAGM2,

	/* FRINT32Z, FRINT32X, FRINT64Z, and FRINT64X instructions */
	RTE_CPUFLAG_FRINT,

	/* SVE Int8 matrix multiplication instructions */
	RTE_CPUFLAG_SVEI8MM,

	/* SVE FP32 floating-point matrix multiplication instructions */
	RTE_CPUFLAG_SVEF32MM,

	/* SVE FP64 floating-point matrix multiplication instructions */
	RTE_CPUFLAG_SVEF64MM,

	/* SVE BFloat16 instructions */
	RTE_CPUFLAG_SVEBF16,

	/* 64 bit execution state of the Arm architecture */
	RTE_CPUFLAG_AARCH64,

	/* WFET and WFIT instructions */
	RTE_CPUFLAG_WFXT,
};

#include "generic/rte_cpuflags.h"

#endif /* _RTE_CPUFLAGS_ARM64_H_ */
