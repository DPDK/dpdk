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
#include <rte_eal.h>

#include "bpf_impl.h"

/* possible instruction node colour */
enum {
	WHITE,
	GREY,
	BLACK,
	MAX_NODE_COLOUR
};

/* possible edge types */
enum {
	UNKNOWN_EDGE,
	TREE_EDGE,
	BACK_EDGE,
	CROSS_EDGE,
	MAX_EDGE_TYPE
};

struct bpf_reg_state {
	uint64_t val;
};

struct bpf_eval_state {
	struct bpf_reg_state rs[EBPF_REG_NUM];
};

#define	MAX_EDGES	2

struct inst_node {
	uint8_t colour;
	uint8_t nb_edge:4;
	uint8_t cur_edge:4;
	uint8_t edge_type[MAX_EDGES];
	uint32_t edge_dest[MAX_EDGES];
	uint32_t prev_node;
	struct bpf_eval_state *evst;
};

struct bpf_verifier {
	const struct rte_bpf_prm *prm;
	struct inst_node *in;
	int32_t stack_sz;
	uint32_t nb_nodes;
	uint32_t nb_jcc_nodes;
	uint32_t node_colour[MAX_NODE_COLOUR];
	uint32_t edge_type[MAX_EDGE_TYPE];
	struct bpf_eval_state *evst;
	struct {
		uint32_t num;
		uint32_t cur;
		struct bpf_eval_state *ent;
	} evst_pool;
};

struct bpf_ins_check {
	struct {
		uint16_t dreg;
		uint16_t sreg;
	} mask;
	struct {
		uint16_t min;
		uint16_t max;
	} off;
	struct {
		uint32_t min;
		uint32_t max;
	} imm;
	const char * (*check)(const struct ebpf_insn *);
	const char * (*eval)(struct bpf_verifier *, const struct ebpf_insn *);
};

#define	ALL_REGS	RTE_LEN2MASK(EBPF_REG_NUM, uint16_t)
#define	WRT_REGS	RTE_LEN2MASK(EBPF_REG_10, uint16_t)
#define	ZERO_REG	RTE_LEN2MASK(EBPF_REG_1, uint16_t)

/*
 * check and evaluate functions for particular instruction types.
 */

static const char *
check_alu_bele(const struct ebpf_insn *ins)
{
	if (ins->imm != 16 && ins->imm != 32 && ins->imm != 64)
		return "invalid imm field";
	return NULL;
}

static const char *
eval_stack(struct bpf_verifier *bvf, const struct ebpf_insn *ins)
{
	int32_t ofs;

	ofs = ins->off;

	if (ofs >= 0 || ofs < -MAX_BPF_STACK_SIZE)
		return "stack boundary violation";

	ofs = -ofs;
	bvf->stack_sz = RTE_MAX(bvf->stack_sz, ofs);
	return NULL;
}

static const char *
eval_store(struct bpf_verifier *bvf, const struct ebpf_insn *ins)
{
	if (ins->dst_reg == EBPF_REG_10)
		return eval_stack(bvf, ins);
	return NULL;
}

static const char *
eval_load(struct bpf_verifier *bvf, const struct ebpf_insn *ins)
{
	if (ins->src_reg == EBPF_REG_10)
		return eval_stack(bvf, ins);
	return NULL;
}

static const char *
eval_call(struct bpf_verifier *bvf, const struct ebpf_insn *ins)
{
	uint32_t idx;

	idx = ins->imm;

	if (idx >= bvf->prm->nb_xsym ||
			bvf->prm->xsym[idx].type != RTE_BPF_XTYPE_FUNC)
		return "invalid external function index";

	/* for now don't support function calls on 32 bit platform */
	if (sizeof(uint64_t) != sizeof(uintptr_t))
		return "function calls are supported only for 64 bit apps";
	return NULL;
}

/*
 * validate parameters for each instruction type.
 */
static const struct bpf_ins_check ins_chk[UINT8_MAX] = {
	/* ALU IMM 32-bit instructions */
	[(BPF_ALU | BPF_ADD | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
	},
	[(BPF_ALU | BPF_SUB | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
	},
	[(BPF_ALU | BPF_AND | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
	},
	[(BPF_ALU | BPF_OR | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
	},
	[(BPF_ALU | BPF_LSH | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
	},
	[(BPF_ALU | BPF_RSH | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
	},
	[(BPF_ALU | BPF_XOR | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
	},
	[(BPF_ALU | BPF_MUL | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
	},
	[(BPF_ALU | EBPF_MOV | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
	},
	[(BPF_ALU | BPF_DIV | BPF_K)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 1, .max = UINT32_MAX},
	},
	[(BPF_ALU | BPF_MOD | BPF_K)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 1, .max = UINT32_MAX},
	},
	/* ALU IMM 64-bit instructions */
	[(EBPF_ALU64 | BPF_ADD | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
	},
	[(EBPF_ALU64 | BPF_SUB | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
	},
	[(EBPF_ALU64 | BPF_AND | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
	},
	[(EBPF_ALU64 | BPF_OR | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
	},
	[(EBPF_ALU64 | BPF_LSH | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
	},
	[(EBPF_ALU64 | BPF_RSH | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
	},
	[(EBPF_ALU64 | EBPF_ARSH | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
	},
	[(EBPF_ALU64 | BPF_XOR | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
	},
	[(EBPF_ALU64 | BPF_MUL | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
	},
	[(EBPF_ALU64 | EBPF_MOV | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
	},
	[(EBPF_ALU64 | BPF_DIV | BPF_K)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 1, .max = UINT32_MAX},
	},
	[(EBPF_ALU64 | BPF_MOD | BPF_K)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 1, .max = UINT32_MAX},
	},
	/* ALU REG 32-bit instructions */
	[(BPF_ALU | BPF_ADD | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(BPF_ALU | BPF_SUB | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(BPF_ALU | BPF_AND | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(BPF_ALU | BPF_OR | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(BPF_ALU | BPF_LSH | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(BPF_ALU | BPF_RSH | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(BPF_ALU | BPF_XOR | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(BPF_ALU | BPF_MUL | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(BPF_ALU | BPF_DIV | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(BPF_ALU | BPF_MOD | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(BPF_ALU | EBPF_MOV | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(BPF_ALU | BPF_NEG)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(BPF_ALU | EBPF_END | EBPF_TO_BE)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 16, .max = 64},
		.check = check_alu_bele,
	},
	[(BPF_ALU | EBPF_END | EBPF_TO_LE)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 16, .max = 64},
		.check = check_alu_bele,
	},
	/* ALU REG 64-bit instructions */
	[(EBPF_ALU64 | BPF_ADD | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(EBPF_ALU64 | BPF_SUB | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(EBPF_ALU64 | BPF_AND | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(EBPF_ALU64 | BPF_OR | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(EBPF_ALU64 | BPF_LSH | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(EBPF_ALU64 | BPF_RSH | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(EBPF_ALU64 | EBPF_ARSH | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(EBPF_ALU64 | BPF_XOR | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(EBPF_ALU64 | BPF_MUL | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(EBPF_ALU64 | BPF_DIV | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(EBPF_ALU64 | BPF_MOD | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(EBPF_ALU64 | EBPF_MOV | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	[(EBPF_ALU64 | BPF_NEG)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
	/* load instructions */
	[(BPF_LDX | BPF_MEM | BPF_B)] = {
		.mask = {. dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
		.eval = eval_load,
	},
	[(BPF_LDX | BPF_MEM | BPF_H)] = {
		.mask = {. dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
		.eval = eval_load,
	},
	[(BPF_LDX | BPF_MEM | BPF_W)] = {
		.mask = {. dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
		.eval = eval_load,
	},
	[(BPF_LDX | BPF_MEM | EBPF_DW)] = {
		.mask = {. dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
		.eval = eval_load,
	},
	/* load 64 bit immediate value */
	[(BPF_LD | BPF_IMM | EBPF_DW)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX},
	},
	/* store REG instructions */
	[(BPF_STX | BPF_MEM | BPF_B)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
		.eval = eval_store,
	},
	[(BPF_STX | BPF_MEM | BPF_H)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
		.eval = eval_store,
	},
	[(BPF_STX | BPF_MEM | BPF_W)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
		.eval = eval_store,
	},
	[(BPF_STX | BPF_MEM | EBPF_DW)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
		.eval = eval_store,
	},
	/* atomic add instructions */
	[(BPF_STX | EBPF_XADD | BPF_W)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
		.eval = eval_store,
	},
	[(BPF_STX | EBPF_XADD | EBPF_DW)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
		.eval = eval_store,
	},
	/* store IMM instructions */
	[(BPF_ST | BPF_MEM | BPF_B)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
		.eval = eval_store,
	},
	[(BPF_ST | BPF_MEM | BPF_H)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
		.eval = eval_store,
	},
	[(BPF_ST | BPF_MEM | BPF_W)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
		.eval = eval_store,
	},
	[(BPF_ST | BPF_MEM | EBPF_DW)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
		.eval = eval_store,
	},
	/* jump instruction */
	[(BPF_JMP | BPF_JA)] = {
		.mask = { .dreg = ZERO_REG, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
	},
	/* jcc IMM instructions */
	[(BPF_JMP | BPF_JEQ | BPF_K)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
	},
	[(BPF_JMP | EBPF_JNE | BPF_K)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
	},
	[(BPF_JMP | BPF_JGT | BPF_K)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
	},
	[(BPF_JMP | EBPF_JLT | BPF_K)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
	},
	[(BPF_JMP | BPF_JGE | BPF_K)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
	},
	[(BPF_JMP | EBPF_JLE | BPF_K)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
	},
	[(BPF_JMP | EBPF_JSGT | BPF_K)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
	},
	[(BPF_JMP | EBPF_JSLT | BPF_K)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
	},
	[(BPF_JMP | EBPF_JSGE | BPF_K)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
	},
	[(BPF_JMP | EBPF_JSLE | BPF_K)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
	},
	[(BPF_JMP | BPF_JSET | BPF_K)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
	},
	/* jcc REG instructions */
	[(BPF_JMP | BPF_JEQ | BPF_X)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
	},
	[(BPF_JMP | EBPF_JNE | BPF_X)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
	},
	[(BPF_JMP | BPF_JGT | BPF_X)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
	},
	[(BPF_JMP | EBPF_JLT | BPF_X)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
	},
	[(BPF_JMP | BPF_JGE | BPF_X)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
	},
	[(BPF_JMP | EBPF_JLE | BPF_X)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
	},
	[(BPF_JMP | EBPF_JSGT | BPF_X)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
	},
	[(BPF_JMP | EBPF_JSLT | BPF_X)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
	},
	[(BPF_JMP | EBPF_JSGE | BPF_X)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
	},
	[(BPF_JMP | EBPF_JSLE | BPF_X)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
	},
	[(BPF_JMP | BPF_JSET | BPF_X)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
	},
	/* call instruction */
	[(BPF_JMP | EBPF_CALL)] = {
		.mask = { .dreg = ZERO_REG, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX},
		.eval = eval_call,
	},
	/* ret instruction */
	[(BPF_JMP | EBPF_EXIT)] = {
		.mask = { .dreg = ZERO_REG, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
	},
};

/*
 * make sure that instruction syntax is valid,
 * and it fields don't violate partciular instrcution type restrictions.
 */
static const char *
check_syntax(const struct ebpf_insn *ins)
{

	uint8_t op;
	uint16_t off;
	uint32_t imm;

	op = ins->code;

	if (ins_chk[op].mask.dreg == 0)
		return "invalid opcode";

	if ((ins_chk[op].mask.dreg & 1 << ins->dst_reg) == 0)
		return "invalid dst-reg field";

	if ((ins_chk[op].mask.sreg & 1 << ins->src_reg) == 0)
		return "invalid src-reg field";

	off = ins->off;
	if (ins_chk[op].off.min > off || ins_chk[op].off.max < off)
		return "invalid off field";

	imm = ins->imm;
	if (ins_chk[op].imm.min > imm || ins_chk[op].imm.max < imm)
		return "invalid imm field";

	if (ins_chk[op].check != NULL)
		return ins_chk[op].check(ins);

	return NULL;
}

/*
 * helper function, return instruction index for the given node.
 */
static uint32_t
get_node_idx(const struct bpf_verifier *bvf, const struct inst_node *node)
{
	return node - bvf->in;
}

/*
 * helper function, used to walk through constructed CFG.
 */
static struct inst_node *
get_next_node(struct bpf_verifier *bvf, struct inst_node *node)
{
	uint32_t ce, ne, dst;

	ne = node->nb_edge;
	ce = node->cur_edge;
	if (ce == ne)
		return NULL;

	node->cur_edge++;
	dst = node->edge_dest[ce];
	return bvf->in + dst;
}

static void
set_node_colour(struct bpf_verifier *bvf, struct inst_node *node,
	uint32_t new)
{
	uint32_t prev;

	prev = node->colour;
	node->colour = new;

	bvf->node_colour[prev]--;
	bvf->node_colour[new]++;
}

/*
 * helper function, add new edge between two nodes.
 */
static int
add_edge(struct bpf_verifier *bvf, struct inst_node *node, uint32_t nidx)
{
	uint32_t ne;

	if (nidx > bvf->prm->nb_ins) {
		RTE_BPF_LOG(ERR, "%s: program boundary violation at pc: %u, "
			"next pc: %u\n",
			__func__, get_node_idx(bvf, node), nidx);
		return -EINVAL;
	}

	ne = node->nb_edge;
	if (ne >= RTE_DIM(node->edge_dest)) {
		RTE_BPF_LOG(ERR, "%s: internal error at pc: %u\n",
			__func__, get_node_idx(bvf, node));
		return -EINVAL;
	}

	node->edge_dest[ne] = nidx;
	node->nb_edge = ne + 1;
	return 0;
}

/*
 * helper function, determine type of edge between two nodes.
 */
static void
set_edge_type(struct bpf_verifier *bvf, struct inst_node *node,
	const struct inst_node *next)
{
	uint32_t ce, clr, type;

	ce = node->cur_edge - 1;
	clr = next->colour;

	type = UNKNOWN_EDGE;

	if (clr == WHITE)
		type = TREE_EDGE;
	else if (clr == GREY)
		type = BACK_EDGE;
	else if (clr == BLACK)
		/*
		 * in fact it could be either direct or cross edge,
		 * but for now, we don't need to distinguish between them.
		 */
		type = CROSS_EDGE;

	node->edge_type[ce] = type;
	bvf->edge_type[type]++;
}

static struct inst_node *
get_prev_node(struct bpf_verifier *bvf, struct inst_node *node)
{
	return  bvf->in + node->prev_node;
}

/*
 * Depth-First Search (DFS) through previously constructed
 * Control Flow Graph (CFG).
 * Information collected at this path would be used later
 * to determine is there any loops, and/or unreachable instructions.
 */
static void
dfs(struct bpf_verifier *bvf)
{
	struct inst_node *next, *node;

	node = bvf->in;
	while (node != NULL) {

		if (node->colour == WHITE)
			set_node_colour(bvf, node, GREY);

		if (node->colour == GREY) {

			/* find next unprocessed child node */
			do {
				next = get_next_node(bvf, node);
				if (next == NULL)
					break;
				set_edge_type(bvf, node, next);
			} while (next->colour != WHITE);

			if (next != NULL) {
				/* proceed with next child */
				next->prev_node = get_node_idx(bvf, node);
				node = next;
			} else {
				/*
				 * finished with current node and all it's kids,
				 * proceed with parent
				 */
				set_node_colour(bvf, node, BLACK);
				node->cur_edge = 0;
				node = get_prev_node(bvf, node);
			}
		} else
			node = NULL;
	}
}

/*
 * report unreachable instructions.
 */
static void
log_unreachable(const struct bpf_verifier *bvf)
{
	uint32_t i;
	struct inst_node *node;
	const struct ebpf_insn *ins;

	for (i = 0; i != bvf->prm->nb_ins; i++) {

		node = bvf->in + i;
		ins = bvf->prm->ins + i;

		if (node->colour == WHITE &&
				ins->code != (BPF_LD | BPF_IMM | EBPF_DW))
			RTE_BPF_LOG(ERR, "unreachable code at pc: %u;\n", i);
	}
}

/*
 * report loops detected.
 */
static void
log_loop(const struct bpf_verifier *bvf)
{
	uint32_t i, j;
	struct inst_node *node;

	for (i = 0; i != bvf->prm->nb_ins; i++) {

		node = bvf->in + i;
		if (node->colour != BLACK)
			continue;

		for (j = 0; j != node->nb_edge; j++) {
			if (node->edge_type[j] == BACK_EDGE)
				RTE_BPF_LOG(ERR,
					"loop at pc:%u --> pc:%u;\n",
					i, node->edge_dest[j]);
		}
	}
}

/*
 * First pass goes though all instructions in the set, checks that each
 * instruction is a valid one (correct syntax, valid field values, etc.)
 * and constructs control flow graph (CFG).
 * Then deapth-first search is performed over the constructed graph.
 * Programs with unreachable instructions and/or loops will be rejected.
 */
static int
validate(struct bpf_verifier *bvf)
{
	int32_t rc;
	uint32_t i;
	struct inst_node *node;
	const struct ebpf_insn *ins;
	const char *err;

	rc = 0;
	for (i = 0; i < bvf->prm->nb_ins; i++) {

		ins = bvf->prm->ins + i;
		node = bvf->in + i;

		err = check_syntax(ins);
		if (err != 0) {
			RTE_BPF_LOG(ERR, "%s: %s at pc: %u\n",
				__func__, err, i);
			rc |= -EINVAL;
		}

		/*
		 * construct CFG, jcc nodes have to outgoing edges,
		 * 'exit' nodes - none, all others nodes have exaclty one
		 * outgoing edge.
		 */
		switch (ins->code) {
		case (BPF_JMP | EBPF_EXIT):
			break;
		case (BPF_JMP | BPF_JEQ | BPF_K):
		case (BPF_JMP | EBPF_JNE | BPF_K):
		case (BPF_JMP | BPF_JGT | BPF_K):
		case (BPF_JMP | EBPF_JLT | BPF_K):
		case (BPF_JMP | BPF_JGE | BPF_K):
		case (BPF_JMP | EBPF_JLE | BPF_K):
		case (BPF_JMP | EBPF_JSGT | BPF_K):
		case (BPF_JMP | EBPF_JSLT | BPF_K):
		case (BPF_JMP | EBPF_JSGE | BPF_K):
		case (BPF_JMP | EBPF_JSLE | BPF_K):
		case (BPF_JMP | BPF_JSET | BPF_K):
		case (BPF_JMP | BPF_JEQ | BPF_X):
		case (BPF_JMP | EBPF_JNE | BPF_X):
		case (BPF_JMP | BPF_JGT | BPF_X):
		case (BPF_JMP | EBPF_JLT | BPF_X):
		case (BPF_JMP | BPF_JGE | BPF_X):
		case (BPF_JMP | EBPF_JLE | BPF_X):
		case (BPF_JMP | EBPF_JSGT | BPF_X):
		case (BPF_JMP | EBPF_JSLT | BPF_X):
		case (BPF_JMP | EBPF_JSGE | BPF_X):
		case (BPF_JMP | EBPF_JSLE | BPF_X):
		case (BPF_JMP | BPF_JSET | BPF_X):
			rc |= add_edge(bvf, node, i + ins->off + 1);
			rc |= add_edge(bvf, node, i + 1);
			bvf->nb_jcc_nodes++;
			break;
		case (BPF_JMP | BPF_JA):
			rc |= add_edge(bvf, node, i + ins->off + 1);
			break;
		/* load 64 bit immediate value */
		case (BPF_LD | BPF_IMM | EBPF_DW):
			rc |= add_edge(bvf, node, i + 2);
			i++;
			break;
		default:
			rc |= add_edge(bvf, node, i + 1);
			break;
		}

		bvf->nb_nodes++;
		bvf->node_colour[WHITE]++;
	}

	if (rc != 0)
		return rc;

	dfs(bvf);

	RTE_BPF_LOG(DEBUG, "%s(%p) stats:\n"
		"nb_nodes=%u;\n"
		"nb_jcc_nodes=%u;\n"
		"node_color={[WHITE]=%u, [GREY]=%u,, [BLACK]=%u};\n"
		"edge_type={[UNKNOWN]=%u, [TREE]=%u, [BACK]=%u, [CROSS]=%u};\n",
		__func__, bvf,
		bvf->nb_nodes,
		bvf->nb_jcc_nodes,
		bvf->node_colour[WHITE], bvf->node_colour[GREY],
			bvf->node_colour[BLACK],
		bvf->edge_type[UNKNOWN_EDGE], bvf->edge_type[TREE_EDGE],
		bvf->edge_type[BACK_EDGE], bvf->edge_type[CROSS_EDGE]);

	if (bvf->node_colour[BLACK] != bvf->nb_nodes) {
		RTE_BPF_LOG(ERR, "%s(%p) unreachable instructions;\n",
			__func__, bvf);
		log_unreachable(bvf);
		return -EINVAL;
	}

	if (bvf->node_colour[GREY] != 0 || bvf->node_colour[WHITE] != 0 ||
			bvf->edge_type[UNKNOWN_EDGE] != 0) {
		RTE_BPF_LOG(ERR, "%s(%p) DFS internal error;\n",
			__func__, bvf);
		return -EINVAL;
	}

	if (bvf->edge_type[BACK_EDGE] != 0) {
		RTE_BPF_LOG(ERR, "%s(%p) loops detected;\n",
			__func__, bvf);
		log_loop(bvf);
		return -EINVAL;
	}

	return 0;
}

/*
 * helper functions get/free eval states.
 */
static struct bpf_eval_state *
pull_eval_state(struct bpf_verifier *bvf)
{
	uint32_t n;

	n = bvf->evst_pool.cur;
	if (n == bvf->evst_pool.num)
		return NULL;

	bvf->evst_pool.cur = n + 1;
	return bvf->evst_pool.ent + n;
}

static void
push_eval_state(struct bpf_verifier *bvf)
{
	bvf->evst_pool.cur--;
}

static void
evst_pool_fini(struct bpf_verifier *bvf)
{
	bvf->evst = NULL;
	free(bvf->evst_pool.ent);
	memset(&bvf->evst_pool, 0, sizeof(bvf->evst_pool));
}

static int
evst_pool_init(struct bpf_verifier *bvf)
{
	uint32_t n;

	n = bvf->nb_jcc_nodes + 1;

	bvf->evst_pool.ent = calloc(n, sizeof(bvf->evst_pool.ent[0]));
	if (bvf->evst_pool.ent == NULL)
		return -ENOMEM;

	bvf->evst_pool.num = n;
	bvf->evst_pool.cur = 0;

	bvf->evst = pull_eval_state(bvf);
	return 0;
}

/*
 * Save current eval state.
 */
static int
save_eval_state(struct bpf_verifier *bvf, struct inst_node *node)
{
	struct bpf_eval_state *st;

	/* get new eval_state for this node */
	st = pull_eval_state(bvf);
	if (st == NULL) {
		RTE_BPF_LOG(ERR,
			"%s: internal error (out of space) at pc: %u",
			__func__, get_node_idx(bvf, node));
		return -ENOMEM;
	}

	/* make a copy of current state */
	memcpy(st, bvf->evst, sizeof(*st));

	/* swap current state with new one */
	node->evst = bvf->evst;
	bvf->evst = st;

	RTE_BPF_LOG(DEBUG, "%s(bvf=%p,node=%u) old/new states: %p/%p;\n",
		__func__, bvf, get_node_idx(bvf, node), node->evst, bvf->evst);

	return 0;
}

/*
 * Restore previous eval state and mark current eval state as free.
 */
static void
restore_eval_state(struct bpf_verifier *bvf, struct inst_node *node)
{
	RTE_BPF_LOG(DEBUG, "%s(bvf=%p,node=%u) old/new states: %p/%p;\n",
		__func__, bvf, get_node_idx(bvf, node), bvf->evst, node->evst);

	bvf->evst = node->evst;
	node->evst = NULL;
	push_eval_state(bvf);
}

/*
 * Do second pass through CFG and try to evaluate instructions
 * via each possible path.
 * Right now evaluation functionality is quite limited.
 * Still need to add extra checks for:
 * - use/return uninitialized registers.
 * - use uninitialized data from the stack.
 * - memory boundaries violation.
 */
static int
evaluate(struct bpf_verifier *bvf)
{
	int32_t rc;
	uint32_t idx, op;
	const char *err;
	const struct ebpf_insn *ins;
	struct inst_node *next, *node;

	node = bvf->in;
	ins = bvf->prm->ins;
	rc = 0;

	while (node != NULL && rc == 0) {

		/* current node evaluation */
		idx = get_node_idx(bvf, node);
		op = ins[idx].code;

		if (ins_chk[op].eval != NULL) {
			err = ins_chk[op].eval(bvf, ins + idx);
			if (err != NULL) {
				RTE_BPF_LOG(ERR, "%s: %s at pc: %u\n",
					__func__, err, idx);
				rc = -EINVAL;
			}
		}

		/* proceed through CFG */
		next = get_next_node(bvf, node);
		if (next != NULL) {

			/* proceed with next child */
			if (node->cur_edge != node->nb_edge)
				rc |= save_eval_state(bvf, node);
			else if (node->evst != NULL)
				restore_eval_state(bvf, node);

			next->prev_node = get_node_idx(bvf, node);
			node = next;
		} else {
			/*
			 * finished with current node and all it's kids,
			 * proceed with parent
			 */
			node->cur_edge = 0;
			node = get_prev_node(bvf, node);

			/* finished */
			if (node == bvf->in)
				node = NULL;
		}
	}

	return rc;
}

int
bpf_validate(struct rte_bpf *bpf)
{
	int32_t rc;
	struct bpf_verifier bvf;

	/* check input argument type, don't allow mbuf ptr on 32-bit */
	if (bpf->prm.prog_arg.type != RTE_BPF_ARG_RAW &&
			bpf->prm.prog_arg.type != RTE_BPF_ARG_PTR &&
			(sizeof(uint64_t) != sizeof(uintptr_t) ||
			bpf->prm.prog_arg.type != RTE_BPF_ARG_PTR_MBUF)) {
		RTE_BPF_LOG(ERR, "%s: unsupported argument type\n", __func__);
		return -ENOTSUP;
	}

	memset(&bvf, 0, sizeof(bvf));
	bvf.prm = &bpf->prm;
	bvf.in = calloc(bpf->prm.nb_ins, sizeof(bvf.in[0]));
	if (bvf.in == NULL)
		return -ENOMEM;

	rc = validate(&bvf);

	if (rc == 0) {
		rc = evst_pool_init(&bvf);
		if (rc == 0)
			rc = evaluate(&bvf);
		evst_pool_fini(&bvf);
	}

	free(bvf.in);

	/* copy collected info */
	if (rc == 0)
		bpf->stack_sz = bvf.stack_sz;

	return rc;
}
