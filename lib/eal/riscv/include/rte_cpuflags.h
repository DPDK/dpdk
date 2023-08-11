/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014 IBM Corporation
 * Copyright(c) 2022 StarFive
 * Copyright(c) 2022 SiFive
 * Copyright(c) 2022 Semihalf
 */

#ifndef RTE_CPUFLAGS_RISCV_H
#define RTE_CPUFLAGS_RISCV_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Enumeration of all CPU features supported
 */
enum rte_cpu_flag_t {
	RTE_CPUFLAG_RISCV_ISA_A, /* Atomic */
	RTE_CPUFLAG_RISCV_ISA_B, /* Bit-Manipulation */
	RTE_CPUFLAG_RISCV_ISA_C, /* Compressed instruction */
	RTE_CPUFLAG_RISCV_ISA_D, /* Double precision floating-point  */
	RTE_CPUFLAG_RISCV_ISA_E, /* RV32E ISA */
	RTE_CPUFLAG_RISCV_ISA_F, /* Single precision floating-point */
	RTE_CPUFLAG_RISCV_ISA_G, /* Extension pack (IMAFD, Zicsr, Zifencei) */
	RTE_CPUFLAG_RISCV_ISA_H, /* Hypervisor */
	RTE_CPUFLAG_RISCV_ISA_I, /* RV32I/RV64I/IRV128I base ISA */
	RTE_CPUFLAG_RISCV_ISA_J, /* Dynamic Translation Language */
	RTE_CPUFLAG_RISCV_ISA_K, /* Reserved */
	RTE_CPUFLAG_RISCV_ISA_L, /* Decimal Floating-Point */
	RTE_CPUFLAG_RISCV_ISA_M, /* Integer Multiply/Divide */
	RTE_CPUFLAG_RISCV_ISA_N, /* User-level interrupts */
	RTE_CPUFLAG_RISCV_ISA_O, /* Reserved */
	RTE_CPUFLAG_RISCV_ISA_P, /* Packed-SIMD */
	RTE_CPUFLAG_RISCV_ISA_Q, /* Quad-precision floating-points */
	RTE_CPUFLAG_RISCV_ISA_R, /* Reserved */
	RTE_CPUFLAG_RISCV_ISA_S, /* Supervisor mode */
	RTE_CPUFLAG_RISCV_ISA_T, /* Transactional memory */
	RTE_CPUFLAG_RISCV_ISA_U, /* User mode */
	RTE_CPUFLAG_RISCV_ISA_V, /* Vector */
	RTE_CPUFLAG_RISCV_ISA_W, /* Reserved */
	RTE_CPUFLAG_RISCV_ISA_X, /* Non-standard extension present */
	RTE_CPUFLAG_RISCV_ISA_Y, /* Reserved */
	RTE_CPUFLAG_RISCV_ISA_Z, /* Reserved */
};

#include "generic/rte_cpuflags.h"

#ifdef __cplusplus
}
#endif

#endif /* RTE_CPUFLAGS_RISCV_H */
