/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 1982, 1986, 1990, 1993
 *      The Regents of the University of California.
 * Copyright(c) 2018 Intel Corporation.
 */

#ifndef _RTE_BPF_DEF_H_
#define _RTE_BPF_DEF_H_

/**
 * @file
 *
 * classic BPF (cBPF) and extended BPF (eBPF) related defines.
 * For more information regarding cBPF and eBPF ISA and their differences,
 * please refer to:
 * https://www.kernel.org/doc/Documentation/networking/filter.txt.
 * As a rule of thumb for that file:
 * all definitions used by both cBPF and eBPF start with bpf(BPF)_ prefix,
 * while eBPF only ones start with ebpf(EBPF)) prefix.
 */

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

/*
 * The instruction encodings.
 */

/* Instruction classes */
#define BPF_CLASS(code) ((code) & 0x07)
#define	BPF_LD		0x00
#define	BPF_LDX		0x01
#define	BPF_ST		0x02
#define	BPF_STX		0x03
#define	BPF_ALU		0x04
#define	BPF_JMP		0x05
#define	BPF_RET		0x06
#define	BPF_MISC        0x07

#define EBPF_ALU64	0x07

/* ld/ldx fields */
#define BPF_SIZE(code)  ((code) & 0x18)
#define	BPF_W		0x00
#define	BPF_H		0x08
#define	BPF_B		0x10
#define	EBPF_DW		0x18

#define BPF_MODE(code)  ((code) & 0xe0)
#define	BPF_IMM		0x00
#define	BPF_ABS		0x20
#define	BPF_IND		0x40
#define	BPF_MEM		0x60
#define	BPF_LEN		0x80
#define	BPF_MSH		0xa0

#define EBPF_XADD	0xc0

/* alu/jmp fields */
#define BPF_OP(code)    ((code) & 0xf0)
#define	BPF_ADD		0x00
#define	BPF_SUB		0x10
#define	BPF_MUL		0x20
#define	BPF_DIV		0x30
#define	BPF_OR		0x40
#define	BPF_AND		0x50
#define	BPF_LSH		0x60
#define	BPF_RSH		0x70
#define	BPF_NEG		0x80
#define	BPF_MOD		0x90
#define	BPF_XOR		0xa0

#define EBPF_MOV	0xb0
#define EBPF_ARSH	0xc0
#define EBPF_END	0xd0

#define	BPF_JA		0x00
#define	BPF_JEQ		0x10
#define	BPF_JGT		0x20
#define	BPF_JGE		0x30
#define	BPF_JSET        0x40

#define EBPF_JNE	0x50
#define EBPF_JSGT	0x60
#define EBPF_JSGE	0x70
#define EBPF_CALL	0x80
#define EBPF_EXIT	0x90
#define EBPF_JLT	0xa0
#define EBPF_JLE	0xb0
#define EBPF_JSLT	0xc0
#define EBPF_JSLE	0xd0

#define BPF_SRC(code)   ((code) & 0x08)
#define	BPF_K		0x00
#define	BPF_X		0x08

/* if BPF_OP(code) == EBPF_END */
#define EBPF_TO_LE	0x00  /* convert to little-endian */
#define EBPF_TO_BE	0x08  /* convert to big-endian */

/*
 * eBPF registers
 */
enum {
	EBPF_REG_0,  /* return value from internal function/for eBPF program */
	EBPF_REG_1,  /* 0-th argument to internal function */
	EBPF_REG_2,  /* 1-th argument to internal function */
	EBPF_REG_3,  /* 2-th argument to internal function */
	EBPF_REG_4,  /* 3-th argument to internal function */
	EBPF_REG_5,  /* 4-th argument to internal function */
	EBPF_REG_6,  /* callee saved register */
	EBPF_REG_7,  /* callee saved register */
	EBPF_REG_8,  /* callee saved register */
	EBPF_REG_9,  /* callee saved register */
	EBPF_REG_10, /* stack pointer (read-only) */
	EBPF_REG_NUM,
};

/*
 * eBPF instruction format
 */
struct ebpf_insn {
	uint8_t code;
	uint8_t dst_reg:4;
	uint8_t src_reg:4;
	int16_t off;
	int32_t imm;
};

/*
 * eBPF allows functions with R1-R5 as arguments.
 */
#define	EBPF_FUNC_MAX_ARGS	(EBPF_REG_6 - EBPF_REG_1)

#ifdef __cplusplus
}
#endif

#endif /* RTE_BPF_DEF_H_ */
