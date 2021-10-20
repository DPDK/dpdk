/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 Stephen Hemminger
 * Based on filter2xdp
 * Copyright (C) 2017 Tobias Klauser
 */

#include <stdio.h>
#include <stdint.h>

#include "rte_bpf.h"

#define BPF_OP_INDEX(x) (BPF_OP(x) >> 4)
#define BPF_SIZE_INDEX(x) (BPF_SIZE(x) >> 3)

static const char *const class_tbl[] = {
	[BPF_LD] = "ld",   [BPF_LDX] = "ldx",	 [BPF_ST] = "st",
	[BPF_STX] = "stx", [BPF_ALU] = "alu",	 [BPF_JMP] = "jmp",
	[BPF_RET] = "ret", [BPF_MISC] = "alu64",
};

static const char *const alu_op_tbl[16] = {
	[BPF_ADD >> 4] = "add",	   [BPF_SUB >> 4] = "sub",
	[BPF_MUL >> 4] = "mul",	   [BPF_DIV >> 4] = "div",
	[BPF_OR >> 4] = "or",	   [BPF_AND >> 4] = "and",
	[BPF_LSH >> 4] = "lsh",	   [BPF_RSH >> 4] = "rsh",
	[BPF_NEG >> 4] = "neg",	   [BPF_MOD >> 4] = "mod",
	[BPF_XOR >> 4] = "xor",	   [EBPF_MOV >> 4] = "mov",
	[EBPF_ARSH >> 4] = "arsh", [EBPF_END >> 4] = "endian",
};

static const char *const size_tbl[] = {
	[BPF_W >> 3] = "w",
	[BPF_H >> 3] = "h",
	[BPF_B >> 3] = "b",
	[EBPF_DW >> 3] = "dw",
};

static const char *const jump_tbl[16] = {
	[BPF_JA >> 4] = "ja",	   [BPF_JEQ >> 4] = "jeq",
	[BPF_JGT >> 4] = "jgt",	   [BPF_JGE >> 4] = "jge",
	[BPF_JSET >> 4] = "jset",  [EBPF_JNE >> 4] = "jne",
	[EBPF_JSGT >> 4] = "jsgt", [EBPF_JSGE >> 4] = "jsge",
	[EBPF_CALL >> 4] = "call", [EBPF_EXIT >> 4] = "exit",
};

void rte_bpf_dump(FILE *f, const struct ebpf_insn *buf, uint32_t len)
{
	uint32_t i;

	for (i = 0; i < len; ++i) {
		const struct ebpf_insn *ins = buf + i;
		uint8_t cls = BPF_CLASS(ins->code);
		const char *op, *postfix = "";

		fprintf(f, " L%u:\t", i);

		switch (cls) {
		default:
			fprintf(f, "unimp 0x%x // class: %s\n",
				ins->code, class_tbl[cls]);
			break;
		case BPF_ALU:
			postfix = "32";
			/* fall through */
		case EBPF_ALU64:
			op = alu_op_tbl[BPF_OP_INDEX(ins->code)];
			if (BPF_SRC(ins->code) == BPF_X)
				fprintf(f, "%s%s r%u, r%u\n", op, postfix, ins->dst_reg,
					ins->src_reg);
			else
				fprintf(f, "%s%s r%u, #0x%x\n", op, postfix,
					ins->dst_reg, ins->imm);
			break;
		case BPF_LD:
			op = "ld";
			postfix = size_tbl[BPF_SIZE_INDEX(ins->code)];
			if (ins->code == (BPF_LD | BPF_IMM | EBPF_DW)) {
				uint64_t val;

				val = (uint32_t)ins[0].imm |
					(uint64_t)(uint32_t)ins[1].imm << 32;
				fprintf(f, "%s%s r%d, #0x%"PRIx64"\n",
					op, postfix, ins->dst_reg, val);
				i++;
			} else if (BPF_MODE(ins->code) == BPF_IMM)
				fprintf(f, "%s%s r%d, #0x%x\n", op, postfix,
					ins->dst_reg, ins->imm);
			else if (BPF_MODE(ins->code) == BPF_ABS)
				fprintf(f, "%s%s r%d, [%d]\n", op, postfix,
					ins->dst_reg, ins->imm);
			else if (BPF_MODE(ins->code) == BPF_IND)
				fprintf(f, "%s%s r%d, [r%u + %d]\n", op, postfix,
					ins->dst_reg, ins->src_reg, ins->imm);
			else
				fprintf(f, "// BUG: LD opcode 0x%02x in eBPF insns\n",
					ins->code);
			break;
		case BPF_LDX:
			op = "ldx";
			postfix = size_tbl[BPF_SIZE_INDEX(ins->code)];
			fprintf(f, "%s%s r%d, [r%u + %d]\n", op, postfix, ins->dst_reg,
				ins->src_reg, ins->off);
			break;
		case BPF_ST:
			op = "st";
			postfix = size_tbl[BPF_SIZE_INDEX(ins->code)];
			if (BPF_MODE(ins->code) == BPF_MEM)
				fprintf(f, "%s%s [r%d + %d], #0x%x\n", op, postfix,
					ins->dst_reg, ins->off, ins->imm);
			else
				fprintf(f, "// BUG: ST opcode 0x%02x in eBPF insns\n",
					ins->code);
			break;
		case BPF_STX:
			op = "stx";
			postfix = size_tbl[BPF_SIZE_INDEX(ins->code)];
			fprintf(f, "%s%s [r%d + %d], r%u\n", op, postfix,
				ins->dst_reg, ins->off, ins->src_reg);
			break;
#define L(pc, off) ((int)(pc) + 1 + (off))
		case BPF_JMP:
			op = jump_tbl[BPF_OP_INDEX(ins->code)];
			if (op == NULL)
				fprintf(f, "invalid jump opcode: %#x\n", ins->code);
			else if (BPF_OP(ins->code) == BPF_JA)
				fprintf(f, "%s L%d\n", op, L(i, ins->off));
			else if (BPF_OP(ins->code) == EBPF_EXIT)
				fprintf(f, "%s\n", op);
			else
				fprintf(f, "%s r%u, #0x%x, L%d\n", op, ins->dst_reg,
					ins->imm, L(i, ins->off));
			break;
		case BPF_RET:
			fprintf(f, "// BUG: RET opcode 0x%02x in eBPF insns\n",
				ins->code);
			break;
		}
	}
}
