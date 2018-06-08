/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_byteorder.h>

#include "bpf_impl.h"

#define BPF_JMP_UNC(ins)	((ins) += (ins)->off)

#define BPF_JMP_CND_REG(reg, ins, op, type)	\
	((ins) += \
		((type)(reg)[(ins)->dst_reg] op (type)(reg)[(ins)->src_reg]) ? \
		(ins)->off : 0)

#define BPF_JMP_CND_IMM(reg, ins, op, type)	\
	((ins) += \
		((type)(reg)[(ins)->dst_reg] op (type)(ins)->imm) ? \
		(ins)->off : 0)

#define BPF_NEG_ALU(reg, ins, type)	\
	((reg)[(ins)->dst_reg] = (type)(-(reg)[(ins)->dst_reg]))

#define EBPF_MOV_ALU_REG(reg, ins, type)	\
	((reg)[(ins)->dst_reg] = (type)(reg)[(ins)->src_reg])

#define BPF_OP_ALU_REG(reg, ins, op, type)	\
	((reg)[(ins)->dst_reg] = \
		(type)(reg)[(ins)->dst_reg] op (type)(reg)[(ins)->src_reg])

#define EBPF_MOV_ALU_IMM(reg, ins, type)	\
	((reg)[(ins)->dst_reg] = (type)(ins)->imm)

#define BPF_OP_ALU_IMM(reg, ins, op, type)	\
	((reg)[(ins)->dst_reg] = \
		(type)(reg)[(ins)->dst_reg] op (type)(ins)->imm)

#define BPF_DIV_ZERO_CHECK(bpf, reg, ins, type) do { \
	if ((type)(reg)[(ins)->src_reg] == 0) { \
		RTE_BPF_LOG(ERR, \
			"%s(%p): division by 0 at pc: %#zx;\n", \
			__func__, bpf, \
			(uintptr_t)(ins) - (uintptr_t)(bpf)->prm.ins); \
		return 0; \
	} \
} while (0)

#define BPF_LD_REG(reg, ins, type)	\
	((reg)[(ins)->dst_reg] = \
		*(type *)(uintptr_t)((reg)[(ins)->src_reg] + (ins)->off))

#define BPF_ST_IMM(reg, ins, type)	\
	(*(type *)(uintptr_t)((reg)[(ins)->dst_reg] + (ins)->off) = \
		(type)(ins)->imm)

#define BPF_ST_REG(reg, ins, type)	\
	(*(type *)(uintptr_t)((reg)[(ins)->dst_reg] + (ins)->off) = \
		(type)(reg)[(ins)->src_reg])

#define BPF_ST_XADD_REG(reg, ins, tp)	\
	(rte_atomic##tp##_add((rte_atomic##tp##_t *) \
		(uintptr_t)((reg)[(ins)->dst_reg] + (ins)->off), \
		reg[ins->src_reg]))

static inline void
bpf_alu_be(uint64_t reg[EBPF_REG_NUM], const struct ebpf_insn *ins)
{
	uint64_t *v;

	v = reg + ins->dst_reg;
	switch (ins->imm) {
	case 16:
		*v = rte_cpu_to_be_16(*v);
		break;
	case 32:
		*v = rte_cpu_to_be_32(*v);
		break;
	case 64:
		*v = rte_cpu_to_be_64(*v);
		break;
	}
}

static inline void
bpf_alu_le(uint64_t reg[EBPF_REG_NUM], const struct ebpf_insn *ins)
{
	uint64_t *v;

	v = reg + ins->dst_reg;
	switch (ins->imm) {
	case 16:
		*v = rte_cpu_to_le_16(*v);
		break;
	case 32:
		*v = rte_cpu_to_le_32(*v);
		break;
	case 64:
		*v = rte_cpu_to_le_64(*v);
		break;
	}
}

static inline uint64_t
bpf_exec(const struct rte_bpf *bpf, uint64_t reg[EBPF_REG_NUM])
{
	const struct ebpf_insn *ins;

	for (ins = bpf->prm.ins; ; ins++) {
		switch (ins->code) {
		/* 32 bit ALU IMM operations */
		case (BPF_ALU | BPF_ADD | BPF_K):
			BPF_OP_ALU_IMM(reg, ins, +, uint32_t);
			break;
		case (BPF_ALU | BPF_SUB | BPF_K):
			BPF_OP_ALU_IMM(reg, ins, -, uint32_t);
			break;
		case (BPF_ALU | BPF_AND | BPF_K):
			BPF_OP_ALU_IMM(reg, ins, &, uint32_t);
			break;
		case (BPF_ALU | BPF_OR | BPF_K):
			BPF_OP_ALU_IMM(reg, ins, |, uint32_t);
			break;
		case (BPF_ALU | BPF_LSH | BPF_K):
			BPF_OP_ALU_IMM(reg, ins, <<, uint32_t);
			break;
		case (BPF_ALU | BPF_RSH | BPF_K):
			BPF_OP_ALU_IMM(reg, ins, >>, uint32_t);
			break;
		case (BPF_ALU | BPF_XOR | BPF_K):
			BPF_OP_ALU_IMM(reg, ins, ^, uint32_t);
			break;
		case (BPF_ALU | BPF_MUL | BPF_K):
			BPF_OP_ALU_IMM(reg, ins, *, uint32_t);
			break;
		case (BPF_ALU | BPF_DIV | BPF_K):
			BPF_OP_ALU_IMM(reg, ins, /, uint32_t);
			break;
		case (BPF_ALU | BPF_MOD | BPF_K):
			BPF_OP_ALU_IMM(reg, ins, %, uint32_t);
			break;
		case (BPF_ALU | EBPF_MOV | BPF_K):
			EBPF_MOV_ALU_IMM(reg, ins, uint32_t);
			break;
		/* 32 bit ALU REG operations */
		case (BPF_ALU | BPF_ADD | BPF_X):
			BPF_OP_ALU_REG(reg, ins, +, uint32_t);
			break;
		case (BPF_ALU | BPF_SUB | BPF_X):
			BPF_OP_ALU_REG(reg, ins, -, uint32_t);
			break;
		case (BPF_ALU | BPF_AND | BPF_X):
			BPF_OP_ALU_REG(reg, ins, &, uint32_t);
			break;
		case (BPF_ALU | BPF_OR | BPF_X):
			BPF_OP_ALU_REG(reg, ins, |, uint32_t);
			break;
		case (BPF_ALU | BPF_LSH | BPF_X):
			BPF_OP_ALU_REG(reg, ins, <<, uint32_t);
			break;
		case (BPF_ALU | BPF_RSH | BPF_X):
			BPF_OP_ALU_REG(reg, ins, >>, uint32_t);
			break;
		case (BPF_ALU | BPF_XOR | BPF_X):
			BPF_OP_ALU_REG(reg, ins, ^, uint32_t);
			break;
		case (BPF_ALU | BPF_MUL | BPF_X):
			BPF_OP_ALU_REG(reg, ins, *, uint32_t);
			break;
		case (BPF_ALU | BPF_DIV | BPF_X):
			BPF_DIV_ZERO_CHECK(bpf, reg, ins, uint32_t);
			BPF_OP_ALU_REG(reg, ins, /, uint32_t);
			break;
		case (BPF_ALU | BPF_MOD | BPF_X):
			BPF_DIV_ZERO_CHECK(bpf, reg, ins, uint32_t);
			BPF_OP_ALU_REG(reg, ins, %, uint32_t);
			break;
		case (BPF_ALU | EBPF_MOV | BPF_X):
			EBPF_MOV_ALU_REG(reg, ins, uint32_t);
			break;
		case (BPF_ALU | BPF_NEG):
			BPF_NEG_ALU(reg, ins, uint32_t);
			break;
		case (BPF_ALU | EBPF_END | EBPF_TO_BE):
			bpf_alu_be(reg, ins);
			break;
		case (BPF_ALU | EBPF_END | EBPF_TO_LE):
			bpf_alu_le(reg, ins);
			break;
		/* 64 bit ALU IMM operations */
		case (EBPF_ALU64 | BPF_ADD | BPF_K):
			BPF_OP_ALU_IMM(reg, ins, +, uint64_t);
			break;
		case (EBPF_ALU64 | BPF_SUB | BPF_K):
			BPF_OP_ALU_IMM(reg, ins, -, uint64_t);
			break;
		case (EBPF_ALU64 | BPF_AND | BPF_K):
			BPF_OP_ALU_IMM(reg, ins, &, uint64_t);
			break;
		case (EBPF_ALU64 | BPF_OR | BPF_K):
			BPF_OP_ALU_IMM(reg, ins, |, uint64_t);
			break;
		case (EBPF_ALU64 | BPF_LSH | BPF_K):
			BPF_OP_ALU_IMM(reg, ins, <<, uint64_t);
			break;
		case (EBPF_ALU64 | BPF_RSH | BPF_K):
			BPF_OP_ALU_IMM(reg, ins, >>, uint64_t);
			break;
		case (EBPF_ALU64 | EBPF_ARSH | BPF_K):
			BPF_OP_ALU_IMM(reg, ins, >>, int64_t);
			break;
		case (EBPF_ALU64 | BPF_XOR | BPF_K):
			BPF_OP_ALU_IMM(reg, ins, ^, uint64_t);
			break;
		case (EBPF_ALU64 | BPF_MUL | BPF_K):
			BPF_OP_ALU_IMM(reg, ins, *, uint64_t);
			break;
		case (EBPF_ALU64 | BPF_DIV | BPF_K):
			BPF_OP_ALU_IMM(reg, ins, /, uint64_t);
			break;
		case (EBPF_ALU64 | BPF_MOD | BPF_K):
			BPF_OP_ALU_IMM(reg, ins, %, uint64_t);
			break;
		case (EBPF_ALU64 | EBPF_MOV | BPF_K):
			EBPF_MOV_ALU_IMM(reg, ins, uint64_t);
			break;
		/* 64 bit ALU REG operations */
		case (EBPF_ALU64 | BPF_ADD | BPF_X):
			BPF_OP_ALU_REG(reg, ins, +, uint64_t);
			break;
		case (EBPF_ALU64 | BPF_SUB | BPF_X):
			BPF_OP_ALU_REG(reg, ins, -, uint64_t);
			break;
		case (EBPF_ALU64 | BPF_AND | BPF_X):
			BPF_OP_ALU_REG(reg, ins, &, uint64_t);
			break;
		case (EBPF_ALU64 | BPF_OR | BPF_X):
			BPF_OP_ALU_REG(reg, ins, |, uint64_t);
			break;
		case (EBPF_ALU64 | BPF_LSH | BPF_X):
			BPF_OP_ALU_REG(reg, ins, <<, uint64_t);
			break;
		case (EBPF_ALU64 | BPF_RSH | BPF_X):
			BPF_OP_ALU_REG(reg, ins, >>, uint64_t);
			break;
		case (EBPF_ALU64 | EBPF_ARSH | BPF_X):
			BPF_OP_ALU_REG(reg, ins, >>, int64_t);
			break;
		case (EBPF_ALU64 | BPF_XOR | BPF_X):
			BPF_OP_ALU_REG(reg, ins, ^, uint64_t);
			break;
		case (EBPF_ALU64 | BPF_MUL | BPF_X):
			BPF_OP_ALU_REG(reg, ins, *, uint64_t);
			break;
		case (EBPF_ALU64 | BPF_DIV | BPF_X):
			BPF_DIV_ZERO_CHECK(bpf, reg, ins, uint64_t);
			BPF_OP_ALU_REG(reg, ins, /, uint64_t);
			break;
		case (EBPF_ALU64 | BPF_MOD | BPF_X):
			BPF_DIV_ZERO_CHECK(bpf, reg, ins, uint64_t);
			BPF_OP_ALU_REG(reg, ins, %, uint64_t);
			break;
		case (EBPF_ALU64 | EBPF_MOV | BPF_X):
			EBPF_MOV_ALU_REG(reg, ins, uint64_t);
			break;
		case (EBPF_ALU64 | BPF_NEG):
			BPF_NEG_ALU(reg, ins, uint64_t);
			break;
		/* load instructions */
		case (BPF_LDX | BPF_MEM | BPF_B):
			BPF_LD_REG(reg, ins, uint8_t);
			break;
		case (BPF_LDX | BPF_MEM | BPF_H):
			BPF_LD_REG(reg, ins, uint16_t);
			break;
		case (BPF_LDX | BPF_MEM | BPF_W):
			BPF_LD_REG(reg, ins, uint32_t);
			break;
		case (BPF_LDX | BPF_MEM | EBPF_DW):
			BPF_LD_REG(reg, ins, uint64_t);
			break;
		/* load 64 bit immediate value */
		case (BPF_LD | BPF_IMM | EBPF_DW):
			reg[ins->dst_reg] = (uint32_t)ins[0].imm |
				(uint64_t)(uint32_t)ins[1].imm << 32;
			ins++;
			break;
		/* store instructions */
		case (BPF_STX | BPF_MEM | BPF_B):
			BPF_ST_REG(reg, ins, uint8_t);
			break;
		case (BPF_STX | BPF_MEM | BPF_H):
			BPF_ST_REG(reg, ins, uint16_t);
			break;
		case (BPF_STX | BPF_MEM | BPF_W):
			BPF_ST_REG(reg, ins, uint32_t);
			break;
		case (BPF_STX | BPF_MEM | EBPF_DW):
			BPF_ST_REG(reg, ins, uint64_t);
			break;
		case (BPF_ST | BPF_MEM | BPF_B):
			BPF_ST_IMM(reg, ins, uint8_t);
			break;
		case (BPF_ST | BPF_MEM | BPF_H):
			BPF_ST_IMM(reg, ins, uint16_t);
			break;
		case (BPF_ST | BPF_MEM | BPF_W):
			BPF_ST_IMM(reg, ins, uint32_t);
			break;
		case (BPF_ST | BPF_MEM | EBPF_DW):
			BPF_ST_IMM(reg, ins, uint64_t);
			break;
		/* atomic add instructions */
		case (BPF_STX | EBPF_XADD | BPF_W):
			BPF_ST_XADD_REG(reg, ins, 32);
			break;
		case (BPF_STX | EBPF_XADD | EBPF_DW):
			BPF_ST_XADD_REG(reg, ins, 64);
			break;
		/* jump instructions */
		case (BPF_JMP | BPF_JA):
			BPF_JMP_UNC(ins);
			break;
		/* jump IMM instructions */
		case (BPF_JMP | BPF_JEQ | BPF_K):
			BPF_JMP_CND_IMM(reg, ins, ==, uint64_t);
			break;
		case (BPF_JMP | EBPF_JNE | BPF_K):
			BPF_JMP_CND_IMM(reg, ins, !=, uint64_t);
			break;
		case (BPF_JMP | BPF_JGT | BPF_K):
			BPF_JMP_CND_IMM(reg, ins, >, uint64_t);
			break;
		case (BPF_JMP | EBPF_JLT | BPF_K):
			BPF_JMP_CND_IMM(reg, ins, <, uint64_t);
			break;
		case (BPF_JMP | BPF_JGE | BPF_K):
			BPF_JMP_CND_IMM(reg, ins, >=, uint64_t);
			break;
		case (BPF_JMP | EBPF_JLE | BPF_K):
			BPF_JMP_CND_IMM(reg, ins, <=, uint64_t);
			break;
		case (BPF_JMP | EBPF_JSGT | BPF_K):
			BPF_JMP_CND_IMM(reg, ins, >, int64_t);
			break;
		case (BPF_JMP | EBPF_JSLT | BPF_K):
			BPF_JMP_CND_IMM(reg, ins, <, int64_t);
			break;
		case (BPF_JMP | EBPF_JSGE | BPF_K):
			BPF_JMP_CND_IMM(reg, ins, >=, int64_t);
			break;
		case (BPF_JMP | EBPF_JSLE | BPF_K):
			BPF_JMP_CND_IMM(reg, ins, <=, int64_t);
			break;
		case (BPF_JMP | BPF_JSET | BPF_K):
			BPF_JMP_CND_IMM(reg, ins, &, uint64_t);
			break;
		/* jump REG instructions */
		case (BPF_JMP | BPF_JEQ | BPF_X):
			BPF_JMP_CND_REG(reg, ins, ==, uint64_t);
			break;
		case (BPF_JMP | EBPF_JNE | BPF_X):
			BPF_JMP_CND_REG(reg, ins, !=, uint64_t);
			break;
		case (BPF_JMP | BPF_JGT | BPF_X):
			BPF_JMP_CND_REG(reg, ins, >, uint64_t);
			break;
		case (BPF_JMP | EBPF_JLT | BPF_X):
			BPF_JMP_CND_REG(reg, ins, <, uint64_t);
			break;
		case (BPF_JMP | BPF_JGE | BPF_X):
			BPF_JMP_CND_REG(reg, ins, >=, uint64_t);
			break;
		case (BPF_JMP | EBPF_JLE | BPF_X):
			BPF_JMP_CND_REG(reg, ins, <=, uint64_t);
			break;
		case (BPF_JMP | EBPF_JSGT | BPF_X):
			BPF_JMP_CND_REG(reg, ins, >, int64_t);
			break;
		case (BPF_JMP | EBPF_JSLT | BPF_X):
			BPF_JMP_CND_REG(reg, ins, <, int64_t);
			break;
		case (BPF_JMP | EBPF_JSGE | BPF_X):
			BPF_JMP_CND_REG(reg, ins, >=, int64_t);
			break;
		case (BPF_JMP | EBPF_JSLE | BPF_X):
			BPF_JMP_CND_REG(reg, ins, <=, int64_t);
			break;
		case (BPF_JMP | BPF_JSET | BPF_X):
			BPF_JMP_CND_REG(reg, ins, &, uint64_t);
			break;
		/* call instructions */
		case (BPF_JMP | EBPF_CALL):
			reg[EBPF_REG_0] = bpf->prm.xsym[ins->imm].func.val(
				reg[EBPF_REG_1], reg[EBPF_REG_2],
				reg[EBPF_REG_3], reg[EBPF_REG_4],
				reg[EBPF_REG_5]);
			break;
		/* return instruction */
		case (BPF_JMP | EBPF_EXIT):
			return reg[EBPF_REG_0];
		default:
			RTE_BPF_LOG(ERR,
				"%s(%p): invalid opcode %#x at pc: %#zx;\n",
				__func__, bpf, ins->code,
				(uintptr_t)ins - (uintptr_t)bpf->prm.ins);
			return 0;
		}
	}

	/* should never be reached */
	RTE_VERIFY(0);
	return 0;
}

__rte_experimental uint32_t
rte_bpf_exec_burst(const struct rte_bpf *bpf, void *ctx[], uint64_t rc[],
	uint32_t num)
{
	uint32_t i;
	uint64_t reg[EBPF_REG_NUM];
	uint64_t stack[MAX_BPF_STACK_SIZE / sizeof(uint64_t)];

	for (i = 0; i != num; i++) {

		reg[EBPF_REG_1] = (uintptr_t)ctx[i];
		reg[EBPF_REG_10] = (uintptr_t)(stack + RTE_DIM(stack));

		rc[i] = bpf_exec(bpf, reg);
	}

	return i;
}

__rte_experimental uint64_t
rte_bpf_exec(const struct rte_bpf *bpf, void *ctx)
{
	uint64_t rc;

	rte_bpf_exec_burst(bpf, &ctx, &rc, 1);
	return rc;
}
