/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 Stephen Hemminger
 * Based on filter2xdp
 * Copyright (C) 2017 Tobias Klauser
 */

#include <stdio.h>
#include <stdint.h>

#include <eal_export.h>
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
	[EBPF_JLT >> 4] = "jlt",   [EBPF_JLE >> 4] = "jle",
	[EBPF_JSLT >> 4] = "jslt", [EBPF_JSLE >> 4] = "jsle",
};

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_bpf_insn_is_wide, 26.07)
bool
rte_bpf_insn_is_wide(const struct ebpf_insn *ins)
{
	return ins->code == (BPF_LD | BPF_IMM | EBPF_DW);
}


/* Format one (possibly wide) eBPF command as hexadecimal in objdump format. */
static int
format_hexadecimal(char *buffer, size_t bufsz, const struct ebpf_insn *ins,
	uint32_t flags)
{
	const char *const b = (const char *)ins;

	RTE_ASSERT((flags & RTE_BPF_FORMAT_FLAG_HEXADECIMAL) != 0);

	RTE_BUILD_BUG_ON(sizeof(*ins) != 8);

	if ((flags & RTE_BPF_FORMAT_FLAG_NEVER_WIDE) == 0 && rte_bpf_insn_is_wide(ins))
		return snprintf(buffer, bufsz,
			"%02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx "
			"%02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx",
			b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
			b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
	else
		return snprintf(buffer, bufsz,
			"%02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx",
			b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
}

/* Return atomic subcommand mnemonic based on BPF_STX immediate. */
static inline const char *
atomic_op(int32_t imm)
{
	switch (imm) {
	case BPF_ATOMIC_ADD:
		return "xadd";
	case BPF_ATOMIC_XCHG:
		return "xchg";
	default:
		return NULL;
	}
}

/* Format one (possibly wide) eBPF command as assembler. */
static int
format_disassembly(char *buffer, size_t bufsz, const struct ebpf_insn *ins,
	uint32_t pc, uint32_t flags)
{
	uint8_t cls = BPF_CLASS(ins->code);
	const char *op, *postfix = "", *warning = "";
	char jump[16];

	RTE_ASSERT((flags & RTE_BPF_FORMAT_FLAG_HEXADECIMAL) == 0);

	switch (cls) {
	default:
		return snprintf(buffer, bufsz, "unimp 0x%x // class: %s",
			ins->code, class_tbl[cls]);
	case BPF_ALU:
		postfix = "32";
		/* fall through */
	case EBPF_ALU64:
		op = alu_op_tbl[BPF_OP_INDEX(ins->code)];
		if (ins->off != 0)
			/* Not yet supported variation with non-zero offset. */
			warning = ", off != 0";
		if (BPF_SRC(ins->code) == BPF_X)
			return snprintf(buffer, bufsz, "%s%s r%u, r%u%s", op, postfix, ins->dst_reg,
				ins->src_reg, warning);
		else
			return snprintf(buffer, bufsz, "%s%s r%u, #0x%x%s", op, postfix,
				ins->dst_reg, ins->imm, warning);
	case BPF_LD:
		op = "ld";
		postfix = size_tbl[BPF_SIZE_INDEX(ins->code)];
		if (ins->code == (BPF_LD | BPF_IMM | EBPF_DW)) {
			uint64_t val;

			if (ins->src_reg != 0)
				/* Not yet supported variation with non-zero src. */
				warning = ", src != 0";
			val = (uint32_t)ins[0].imm |
				(uint64_t)(uint32_t)ins[1].imm << 32;
			return snprintf(buffer, bufsz, "%s%s r%d, #0x%"PRIx64"%s",
				op, postfix, ins->dst_reg, val, warning);
		}
		switch (BPF_MODE(ins->code)) {
		case BPF_IMM:
			return snprintf(buffer, bufsz, "%s%s r%d, #0x%x", op, postfix,
				ins->dst_reg, ins->imm);
		case BPF_ABS:
			return snprintf(buffer, bufsz, "%s%s r%d, [%d]", op, postfix,
				ins->dst_reg, ins->imm);
		case BPF_IND:
			return snprintf(buffer, bufsz, "%s%s r%d, [r%u + %d]", op, postfix,
				ins->dst_reg, ins->src_reg, ins->imm);
		default:
			return snprintf(buffer, bufsz, "// BUG: LD opcode 0x%02x in eBPF insns",
				ins->code);
		}
	case BPF_LDX:
		op = "ldx";
		postfix = size_tbl[BPF_SIZE_INDEX(ins->code)];
		if (BPF_MODE(ins->code) == BPF_MEM)
			return snprintf(buffer, bufsz, "%s%s r%d, [r%u + %d]", op, postfix,
				ins->dst_reg, ins->src_reg, ins->off);
		else
			return snprintf(buffer, bufsz, "// BUG: LDX opcode 0x%02x in eBPF insns",
				ins->code);
	case BPF_ST:
		op = "st";
		postfix = size_tbl[BPF_SIZE_INDEX(ins->code)];
		if (BPF_MODE(ins->code) == BPF_MEM)
			return snprintf(buffer, bufsz, "%s%s [r%d + %d], #0x%x", op, postfix,
				ins->dst_reg, ins->off, ins->imm);
		else
			return snprintf(buffer, bufsz, "// BUG: ST opcode 0x%02x in eBPF insns",
				ins->code);
	case BPF_STX:
		switch (BPF_MODE(ins->code)) {
		case BPF_MEM:
			op = "stx";
			break;
		case EBPF_ATOMIC:
			op = atomic_op(ins->imm);
			if (op == NULL)
				return snprintf(buffer, bufsz,
					"// BUG: ATOMIC operation 0x%x in eBPF insns", ins->imm);
			break;
		default:
			return snprintf(buffer, bufsz, "// BUG: STX opcode 0x%02x in eBPF insns",
				ins->code);
		}
		postfix = size_tbl[BPF_SIZE_INDEX(ins->code)];
		return snprintf(buffer, bufsz, "%s%s [r%d + %d], r%u", op, postfix,
			ins->dst_reg, ins->off, ins->src_reg);
	case BPF_JMP:
		op = jump_tbl[BPF_OP_INDEX(ins->code)];
		if (op == NULL)
			return snprintf(buffer, bufsz, "invalid jump opcode: %#x", ins->code);

		if ((flags & RTE_BPF_FORMAT_FLAG_ABSOLUTE_JUMPS) != 0)
			snprintf(jump, sizeof(jump), "L%d", pc + 1 + ins->off);
		else
			snprintf(jump, sizeof(jump), "%+d", (int)ins->off);

		if (ins->src_reg != 0)
			/* Not yet supported variation with non-zero src w/o condition. */
			warning = ", src != 0";
		switch (BPF_OP(ins->code)) {
		case BPF_JA:
			return snprintf(buffer, bufsz, "%s %s%s", op, jump, warning);
		case EBPF_CALL:
			/* Call of helper function with index in immediate. */
			return snprintf(buffer, bufsz, "%s #%u%s", op, ins->imm, warning);
		case EBPF_EXIT:
			return snprintf(buffer, bufsz, "%s%s", op, warning);
		}

		if (BPF_SRC(ins->code) == BPF_X)
			return snprintf(buffer, bufsz, "%s r%u, r%u, %s", op, ins->dst_reg,
				ins->src_reg, jump);
		else
			return snprintf(buffer, bufsz, "%s r%u, #0x%x, %s", op, ins->dst_reg,
				ins->imm, jump);
	case BPF_RET:
		return snprintf(buffer, bufsz, "// BUG: RET opcode 0x%02x in eBPF insns",
			ins->code);
	}
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_bpf_format, 26.07)
int
rte_bpf_format(char *buffer, size_t bufsz, const struct ebpf_insn *ins,
	uint32_t pc, uint32_t flags)
{
	if ((flags & RTE_BPF_FORMAT_FLAG_HEXADECIMAL) != 0)
		return format_hexadecimal(buffer, bufsz, ins, flags);
	else
		return format_disassembly(buffer, bufsz, ins, pc, flags);
}

RTE_EXPORT_SYMBOL(rte_bpf_dump)
void rte_bpf_dump(FILE *f, const struct ebpf_insn *buf, uint32_t len)
{
	uint32_t i;
	char buffer[256];

	for (i = 0; i < len; ++i) {
		const struct ebpf_insn *ins = buf + i;

		format_disassembly(buffer, sizeof(buffer), ins, i,
			RTE_BPF_FORMAT_FLAG_DISASSEMBLY	|
			RTE_BPF_FORMAT_FLAG_ABSOLUTE_JUMPS);
		fprintf(f, " L%u:\t%s\n", i, buffer);
		i += rte_bpf_insn_is_wide(ins);
	}
}
