/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <errno.h>
#include <stdint.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>

#include "bpf_impl.h"

#define GET_BPF_OP(op)	(BPF_OP(op) >> 4)

enum {
	RAX = 0,  /* scratch, return value */
	RCX = 1,  /* scratch, 4th arg */
	RDX = 2,  /* scratch, 3rd arg */
	RBX = 3,  /* callee saved */
	RSP = 4,  /* stack pointer */
	RBP = 5,  /* frame pointer, callee saved */
	RSI = 6,  /* scratch, 2nd arg */
	RDI = 7,  /* scratch, 1st arg */
	R8  = 8,  /* scratch, 5th arg */
	R9  = 9,  /* scratch, 6th arg */
	R10 = 10, /* scratch */
	R11 = 11, /* scratch */
	R12 = 12, /* callee saved */
	R13 = 13, /* callee saved */
	R14 = 14, /* callee saved */
	R15 = 15, /* callee saved */
};

#define IS_EXT_REG(r)	((r) >= R8)

enum {
	REX_PREFIX = 0x40, /* fixed value 0100 */
	REX_W = 0x8,       /* 64bit operand size */
	REX_R = 0x4,       /* extension of the ModRM.reg field */
	REX_X = 0x2,       /* extension of the SIB.index field */
	REX_B = 0x1,       /* extension of the ModRM.rm field */
};

enum {
	MOD_INDIRECT = 0,
	MOD_IDISP8 = 1,
	MOD_IDISP32 = 2,
	MOD_DIRECT = 3,
};

enum {
	SIB_SCALE_1 = 0,
	SIB_SCALE_2 = 1,
	SIB_SCALE_4 = 2,
	SIB_SCALE_8 = 3,
};

/*
 * eBPF to x86_64 register mappings.
 */
static const uint32_t ebpf2x86[] = {
	[EBPF_REG_0] = RAX,
	[EBPF_REG_1] = RDI,
	[EBPF_REG_2] = RSI,
	[EBPF_REG_3] = RDX,
	[EBPF_REG_4] = RCX,
	[EBPF_REG_5] = R8,
	[EBPF_REG_6] = RBX,
	[EBPF_REG_7] = R13,
	[EBPF_REG_8] = R14,
	[EBPF_REG_9] = R15,
	[EBPF_REG_10] = RBP,
};

/*
 * r10 and r11 are used as a scratch temporary registers.
 */
enum {
	REG_DIV_IMM = R9,
	REG_TMP0 = R11,
	REG_TMP1 = R10,
};

/* LD_ABS/LD_IMM offsets */
enum {
	LDMB_FSP_OFS, /* fast-path */
	LDMB_SLP_OFS, /* slow-path */
	LDMB_FIN_OFS, /* final part */
	LDMB_OFS_NUM
};

/*
 * callee saved registers list.
 * keep RBP as the last one.
 */
static const uint32_t save_regs[] = {RBX, R12, R13, R14, R15, RBP};

struct bpf_jit_state {
	uint32_t idx;
	size_t sz;
	struct {
		uint32_t num;
		int32_t off;
	} exit;
	struct {
		uint32_t stack_ofs;
	} ldmb;
	uint32_t reguse;
	int32_t *off;
	uint8_t *ins;
};

#define	INUSE(v, r)	(((v) >> (r)) & 1)
#define	USED(v, r)	((v) |= 1 << (r))

union bpf_jit_imm {
	uint32_t u32;
	uint8_t u8[4];
};

/*
 * In many cases for imm8 we can produce shorter code.
 */
static size_t
imm_size(int32_t v)
{
	if (v == (int8_t)v)
		return sizeof(int8_t);
	return sizeof(int32_t);
}

static void
emit_bytes(struct bpf_jit_state *st, const uint8_t ins[], uint32_t sz)
{
	uint32_t i;

	if (st->ins != NULL) {
		for (i = 0; i != sz; i++)
			st->ins[st->sz + i] = ins[i];
	}
	st->sz += sz;
}

static void
emit_imm(struct bpf_jit_state *st, const uint32_t imm, uint32_t sz)
{
	union bpf_jit_imm v;

	v.u32 = imm;
	emit_bytes(st, v.u8, sz);
}

/*
 * emit REX byte
 */
static void
emit_rex(struct bpf_jit_state *st, uint32_t op, uint32_t reg, uint32_t rm)
{
	uint8_t rex;

	/* mark operand registers as used*/
	USED(st->reguse, reg);
	USED(st->reguse, rm);

	rex = 0;
	if (BPF_CLASS(op) == EBPF_ALU64 ||
			op == (BPF_ST | BPF_MEM | EBPF_DW) ||
			op == (BPF_STX | BPF_MEM | EBPF_DW) ||
			op == (BPF_STX | EBPF_XADD | EBPF_DW) ||
			op == (BPF_LD | BPF_IMM | EBPF_DW) ||
			(BPF_CLASS(op) == BPF_LDX &&
			BPF_MODE(op) == BPF_MEM &&
			BPF_SIZE(op) != BPF_W))
		rex |= REX_W;

	if (IS_EXT_REG(reg))
		rex |= REX_R;

	if (IS_EXT_REG(rm))
		rex |= REX_B;

	/* store using SIL, DIL */
	if (op == (BPF_STX | BPF_MEM | BPF_B) && (reg == RDI || reg == RSI))
		rex |= REX_PREFIX;

	if (rex != 0) {
		rex |= REX_PREFIX;
		emit_bytes(st, &rex, sizeof(rex));
	}
}

/*
 * emit MODRegRM byte
 */
static void
emit_modregrm(struct bpf_jit_state *st, uint32_t mod, uint32_t reg, uint32_t rm)
{
	uint8_t v;

	v = mod << 6 | (reg & 7) << 3 | (rm & 7);
	emit_bytes(st, &v, sizeof(v));
}

/*
 * emit SIB byte
 */
static void
emit_sib(struct bpf_jit_state *st, uint32_t scale, uint32_t idx, uint32_t base)
{
	uint8_t v;

	v = scale << 6 | (idx & 7) << 3 | (base & 7);
	emit_bytes(st, &v, sizeof(v));
}

/*
 * emit OPCODE+REGIDX byte
 */
static void
emit_opcode(struct bpf_jit_state *st, uint8_t ops, uint32_t reg)
{
	uint8_t v;

	v = ops | (reg & 7);
	emit_bytes(st, &v, sizeof(v));
}


/*
 * emit xchg %<sreg>, %<dreg>
 */
static void
emit_xchg_reg(struct bpf_jit_state *st, uint32_t sreg, uint32_t dreg)
{
	const uint8_t ops = 0x87;

	emit_rex(st, EBPF_ALU64, sreg, dreg);
	emit_bytes(st, &ops, sizeof(ops));
	emit_modregrm(st, MOD_DIRECT, sreg, dreg);
}

/*
 * emit neg %<dreg>
 */
static void
emit_neg(struct bpf_jit_state *st, uint32_t op, uint32_t dreg)
{
	const uint8_t ops = 0xF7;
	const uint8_t mods = 3;

	emit_rex(st, op, 0, dreg);
	emit_bytes(st, &ops, sizeof(ops));
	emit_modregrm(st, MOD_DIRECT, mods, dreg);
}

/*
 * emit mov %<sreg>, %<dreg>
 */
static void
emit_mov_reg(struct bpf_jit_state *st, uint32_t op, uint32_t sreg,
	uint32_t dreg)
{
	const uint8_t ops = 0x89;

	/* if operands are 32-bit, then it can be used to clear upper 32-bit */
	if (sreg != dreg || BPF_CLASS(op) == BPF_ALU) {
		emit_rex(st, op, sreg, dreg);
		emit_bytes(st, &ops, sizeof(ops));
		emit_modregrm(st, MOD_DIRECT, sreg, dreg);
	}
}

/*
 * emit movzwl %<sreg>, %<dreg>
 */
static void
emit_movzwl(struct bpf_jit_state *st, uint32_t sreg, uint32_t dreg)
{
	static const uint8_t ops[] = {0x0F, 0xB7};

	emit_rex(st, BPF_ALU, sreg, dreg);
	emit_bytes(st, ops, sizeof(ops));
	emit_modregrm(st, MOD_DIRECT, sreg, dreg);
}

/*
 * emit ror <imm8>, %<dreg>
 */
static void
emit_ror_imm(struct bpf_jit_state *st, uint32_t dreg, uint32_t imm)
{
	const uint8_t prfx = 0x66;
	const uint8_t ops = 0xC1;
	const uint8_t mods = 1;

	emit_bytes(st, &prfx, sizeof(prfx));
	emit_rex(st, BPF_ALU, 0, dreg);
	emit_bytes(st, &ops, sizeof(ops));
	emit_modregrm(st, MOD_DIRECT, mods, dreg);
	emit_imm(st, imm, imm_size(imm));
}

/*
 * emit bswap %<dreg>
 */
static void
emit_be2le_48(struct bpf_jit_state *st, uint32_t dreg, uint32_t imm)
{
	uint32_t rop;

	const uint8_t ops = 0x0F;
	const uint8_t mods = 1;

	rop = (imm == 64) ? EBPF_ALU64 : BPF_ALU;
	emit_rex(st, rop, 0, dreg);
	emit_bytes(st, &ops, sizeof(ops));
	emit_modregrm(st, MOD_DIRECT, mods, dreg);
}

static void
emit_be2le(struct bpf_jit_state *st, uint32_t dreg, uint32_t imm)
{
	if (imm == 16) {
		emit_ror_imm(st, dreg, 8);
		emit_movzwl(st, dreg, dreg);
	} else
		emit_be2le_48(st, dreg, imm);
}

/*
 * In general it is NOP for x86.
 * Just clear the upper bits.
 */
static void
emit_le2be(struct bpf_jit_state *st, uint32_t dreg, uint32_t imm)
{
	if (imm == 16)
		emit_movzwl(st, dreg, dreg);
	else if (imm == 32)
		emit_mov_reg(st, BPF_ALU | EBPF_MOV | BPF_X, dreg, dreg);
}

/*
 * emit one of:
 *   add <imm>, %<dreg>
 *   and <imm>, %<dreg>
 *   or  <imm>, %<dreg>
 *   sub <imm>, %<dreg>
 *   xor <imm>, %<dreg>
 */
static void
emit_alu_imm(struct bpf_jit_state *st, uint32_t op, uint32_t dreg, uint32_t imm)
{
	uint8_t mod, opcode;
	uint32_t bop, imsz;

	const uint8_t op8 = 0x83;
	const uint8_t op32 = 0x81;
	static const uint8_t mods[] = {
		[GET_BPF_OP(BPF_ADD)] = 0,
		[GET_BPF_OP(BPF_AND)] = 4,
		[GET_BPF_OP(BPF_OR)] =  1,
		[GET_BPF_OP(BPF_SUB)] = 5,
		[GET_BPF_OP(BPF_XOR)] = 6,
	};

	bop = GET_BPF_OP(op);
	mod = mods[bop];

	imsz = imm_size(imm);
	opcode = (imsz == 1) ? op8 : op32;

	emit_rex(st, op, 0, dreg);
	emit_bytes(st, &opcode, sizeof(opcode));
	emit_modregrm(st, MOD_DIRECT, mod, dreg);
	emit_imm(st, imm, imsz);
}

/*
 * emit one of:
 *   add %<sreg>, %<dreg>
 *   and %<sreg>, %<dreg>
 *   or  %<sreg>, %<dreg>
 *   sub %<sreg>, %<dreg>
 *   xor %<sreg>, %<dreg>
 */
static void
emit_alu_reg(struct bpf_jit_state *st, uint32_t op, uint32_t sreg,
	uint32_t dreg)
{
	uint32_t bop;

	static const uint8_t ops[] = {
		[GET_BPF_OP(BPF_ADD)] = 0x01,
		[GET_BPF_OP(BPF_AND)] = 0x21,
		[GET_BPF_OP(BPF_OR)] =  0x09,
		[GET_BPF_OP(BPF_SUB)] = 0x29,
		[GET_BPF_OP(BPF_XOR)] = 0x31,
	};

	bop = GET_BPF_OP(op);

	emit_rex(st, op, sreg, dreg);
	emit_bytes(st, &ops[bop], sizeof(ops[bop]));
	emit_modregrm(st, MOD_DIRECT, sreg, dreg);
}

static void
emit_shift(struct bpf_jit_state *st, uint32_t op, uint32_t dreg)
{
	uint8_t mod;
	uint32_t bop, opx;

	static const uint8_t ops[] = {0xC1, 0xD3};
	static const uint8_t mods[] = {
		[GET_BPF_OP(BPF_LSH)] = 4,
		[GET_BPF_OP(BPF_RSH)] = 5,
		[GET_BPF_OP(EBPF_ARSH)] = 7,
	};

	bop = GET_BPF_OP(op);
	mod = mods[bop];
	opx = (BPF_SRC(op) == BPF_X);

	emit_rex(st, op, 0, dreg);
	emit_bytes(st, &ops[opx], sizeof(ops[opx]));
	emit_modregrm(st, MOD_DIRECT, mod, dreg);
}

/*
 * emit one of:
 *   shl <imm>, %<dreg>
 *   shr <imm>, %<dreg>
 *   sar <imm>, %<dreg>
 */
static void
emit_shift_imm(struct bpf_jit_state *st, uint32_t op, uint32_t dreg,
	uint32_t imm)
{
	emit_shift(st, op, dreg);
	emit_imm(st, imm, imm_size(imm));
}

/*
 * emit one of:
 *   shl %<dreg>
 *   shr %<dreg>
 *   sar %<dreg>
 * note that rcx is implicitly used as a source register, so few extra
 * instructions for register spillage might be necessary.
 */
static void
emit_shift_reg(struct bpf_jit_state *st, uint32_t op, uint32_t sreg,
	uint32_t dreg)
{
	if (sreg != RCX)
		emit_xchg_reg(st, RCX, sreg);

	emit_shift(st, op, (dreg == RCX) ? sreg : dreg);

	if (sreg != RCX)
		emit_xchg_reg(st, RCX, sreg);
}

/*
 * emit mov <imm>, %<dreg>
 */
static void
emit_mov_imm(struct bpf_jit_state *st, uint32_t op, uint32_t dreg, uint32_t imm)
{
	const uint8_t ops = 0xC7;

	if (imm == 0) {
		/* replace 'mov 0, %<dst>' with 'xor %<dst>, %<dst>' */
		op = BPF_CLASS(op) | BPF_XOR | BPF_X;
		emit_alu_reg(st, op, dreg, dreg);
		return;
	}

	emit_rex(st, op, 0, dreg);
	emit_bytes(st, &ops, sizeof(ops));
	emit_modregrm(st, MOD_DIRECT, 0, dreg);
	emit_imm(st, imm, sizeof(imm));
}

/*
 * emit mov <imm64>, %<dreg>
 */
static void
emit_ld_imm64(struct bpf_jit_state *st, uint32_t dreg, uint32_t imm0,
	uint32_t imm1)
{
	uint32_t op;

	const uint8_t ops = 0xB8;

	op = (imm1 == 0) ? BPF_ALU : EBPF_ALU64;

	emit_rex(st, op, 0, dreg);
	emit_opcode(st, ops, dreg);

	emit_imm(st, imm0, sizeof(imm0));
	if (imm1 != 0)
		emit_imm(st, imm1, sizeof(imm1));
}

/*
 * note that rax:rdx are implicitly used as source/destination registers,
 * so some reg spillage is necessary.
 * emit:
 * mov %rax, %r11
 * mov %rdx, %r10
 * mov %<dreg>, %rax
 * either:
 *   mov %<sreg>, %rdx
 * OR
 *   mov <imm>, %rdx
 * mul %rdx
 * mov %r10, %rdx
 * mov %rax, %<dreg>
 * mov %r11, %rax
 */
static void
emit_mul(struct bpf_jit_state *st, uint32_t op, uint32_t sreg, uint32_t dreg,
	uint32_t imm)
{
	const uint8_t ops = 0xF7;
	const uint8_t mods = 4;

	/* save rax & rdx */
	emit_mov_reg(st, EBPF_ALU64 | EBPF_MOV | BPF_X, RAX, REG_TMP0);
	emit_mov_reg(st, EBPF_ALU64 | EBPF_MOV | BPF_X, RDX, REG_TMP1);

	/* rax = dreg */
	emit_mov_reg(st, EBPF_ALU64 | EBPF_MOV | BPF_X, dreg, RAX);

	if (BPF_SRC(op) == BPF_X)
		/* rdx = sreg */
		emit_mov_reg(st, EBPF_ALU64 | EBPF_MOV | BPF_X,
			sreg == RAX ? REG_TMP0 : sreg, RDX);
	else
		/* rdx = imm */
		emit_mov_imm(st, EBPF_ALU64 | EBPF_MOV | BPF_K, RDX, imm);

	emit_rex(st, op, RAX, RDX);
	emit_bytes(st, &ops, sizeof(ops));
	emit_modregrm(st, MOD_DIRECT, mods, RDX);

	if (dreg != RDX)
		/* restore rdx */
		emit_mov_reg(st, EBPF_ALU64 | EBPF_MOV | BPF_X, REG_TMP1, RDX);

	if (dreg != RAX) {
		/* dreg = rax */
		emit_mov_reg(st, EBPF_ALU64 | EBPF_MOV | BPF_X, RAX, dreg);
		/* restore rax */
		emit_mov_reg(st, EBPF_ALU64 | EBPF_MOV | BPF_X, REG_TMP0, RAX);
	}
}

/*
 * emit mov <ofs>(%<sreg>), %<dreg>
 * note that for non 64-bit ops, higher bits have to be cleared.
 */
static void
emit_ld_reg(struct bpf_jit_state *st, uint32_t op, uint32_t sreg, uint32_t dreg,
	int32_t ofs)
{
	uint32_t mods, opsz;
	const uint8_t op32 = 0x8B;
	const uint8_t op16[] = {0x0F, 0xB7};
	const uint8_t op8[] = {0x0F, 0xB6};

	emit_rex(st, op, dreg, sreg);

	opsz = BPF_SIZE(op);
	if (opsz == BPF_B)
		emit_bytes(st, op8, sizeof(op8));
	else if (opsz == BPF_H)
		emit_bytes(st, op16, sizeof(op16));
	else
		emit_bytes(st, &op32, sizeof(op32));

	mods = (imm_size(ofs) == 1) ? MOD_IDISP8 : MOD_IDISP32;

	emit_modregrm(st, mods, dreg, sreg);
	if (sreg == RSP || sreg == R12)
		emit_sib(st, SIB_SCALE_1, sreg, sreg);
	emit_imm(st, ofs, imm_size(ofs));
}

/*
 * emit one of:
 *   mov %<sreg>, <ofs>(%<dreg>)
 *   mov <imm>, <ofs>(%<dreg>)
 */
static void
emit_st_common(struct bpf_jit_state *st, uint32_t op, uint32_t sreg,
	uint32_t dreg, uint32_t imm, int32_t ofs)
{
	uint32_t mods, imsz, opsz, opx;
	const uint8_t prfx16 = 0x66;

	/* 8 bit instruction opcodes */
	static const uint8_t op8[] = {0xC6, 0x88};

	/* 16/32/64 bit instruction opcodes */
	static const uint8_t ops[] = {0xC7, 0x89};

	/* is the instruction has immediate value or src reg? */
	opx = (BPF_CLASS(op) == BPF_STX);

	opsz = BPF_SIZE(op);
	if (opsz == BPF_H)
		emit_bytes(st, &prfx16, sizeof(prfx16));

	emit_rex(st, op, sreg, dreg);

	if (opsz == BPF_B)
		emit_bytes(st, &op8[opx], sizeof(op8[opx]));
	else
		emit_bytes(st, &ops[opx], sizeof(ops[opx]));

	imsz = imm_size(ofs);
	mods = (imsz == 1) ? MOD_IDISP8 : MOD_IDISP32;

	emit_modregrm(st, mods, sreg, dreg);

	if (dreg == RSP || dreg == R12)
		emit_sib(st, SIB_SCALE_1, dreg, dreg);

	emit_imm(st, ofs, imsz);

	if (opx == 0) {
		imsz = RTE_MIN(bpf_size(opsz), sizeof(imm));
		emit_imm(st, imm, imsz);
	}
}

static void
emit_st_imm(struct bpf_jit_state *st, uint32_t op, uint32_t dreg, uint32_t imm,
	int32_t ofs)
{
	emit_st_common(st, op, 0, dreg, imm, ofs);
}

static void
emit_st_reg(struct bpf_jit_state *st, uint32_t op, uint32_t sreg, uint32_t dreg,
	int32_t ofs)
{
	emit_st_common(st, op, sreg, dreg, 0, ofs);
}

/*
 * emit lock add %<sreg>, <ofs>(%<dreg>)
 */
static void
emit_st_xadd(struct bpf_jit_state *st, uint32_t op, uint32_t sreg,
	uint32_t dreg, int32_t ofs)
{
	uint32_t imsz, mods;

	const uint8_t lck = 0xF0; /* lock prefix */
	const uint8_t ops = 0x01; /* add opcode */

	imsz = imm_size(ofs);
	mods = (imsz == 1) ? MOD_IDISP8 : MOD_IDISP32;

	emit_bytes(st, &lck, sizeof(lck));
	emit_rex(st, op, sreg, dreg);
	emit_bytes(st, &ops, sizeof(ops));
	emit_modregrm(st, mods, sreg, dreg);
	emit_imm(st, ofs, imsz);
}

/*
 * emit:
 *    mov <imm64>, (%rax)
 *    call *%rax
 */
static void
emit_call(struct bpf_jit_state *st, uintptr_t trg)
{
	const uint8_t ops = 0xFF;
	const uint8_t mods = 2;

	emit_ld_imm64(st, RAX, trg, trg >> 32);
	emit_bytes(st, &ops, sizeof(ops));
	emit_modregrm(st, MOD_DIRECT, mods, RAX);
}

/*
 * emit jmp <ofs>
 * where 'ofs' is the target offset for the native code.
 */
static void
emit_abs_jmp(struct bpf_jit_state *st, int32_t ofs)
{
	int32_t joff;
	uint32_t imsz;

	const uint8_t op8 = 0xEB;
	const uint8_t op32 = 0xE9;

	const int32_t sz8 = sizeof(op8) + sizeof(uint8_t);
	const int32_t sz32 = sizeof(op32) + sizeof(uint32_t);

	/* max possible jmp instruction size */
	const int32_t iszm = RTE_MAX(sz8, sz32);

	joff = ofs - st->sz;
	imsz = RTE_MAX(imm_size(joff), imm_size(joff + iszm));

	if (imsz == 1) {
		emit_bytes(st, &op8, sizeof(op8));
		joff -= sz8;
	} else {
		emit_bytes(st, &op32, sizeof(op32));
		joff -= sz32;
	}

	emit_imm(st, joff, imsz);
}

/*
 * emit jmp <ofs>
 * where 'ofs' is the target offset for the BPF bytecode.
 */
static void
emit_jmp(struct bpf_jit_state *st, int32_t ofs)
{
	emit_abs_jmp(st, st->off[st->idx + ofs]);
}

/*
 * emit one of:
 *    cmovz %<sreg>, <%dreg>
 *    cmovne %<sreg>, <%dreg>
 *    cmova %<sreg>, <%dreg>
 *    cmovb %<sreg>, <%dreg>
 *    cmovae %<sreg>, <%dreg>
 *    cmovbe %<sreg>, <%dreg>
 *    cmovg %<sreg>, <%dreg>
 *    cmovl %<sreg>, <%dreg>
 *    cmovge %<sreg>, <%dreg>
 *    cmovle %<sreg>, <%dreg>
 */
static void
emit_movcc_reg(struct bpf_jit_state *st, uint32_t op, uint32_t sreg,
	uint32_t dreg)
{
	uint32_t bop;

	static const uint8_t ops[][2] = {
		[GET_BPF_OP(BPF_JEQ)] = {0x0F, 0x44},  /* CMOVZ */
		[GET_BPF_OP(EBPF_JNE)] = {0x0F, 0x45},  /* CMOVNE */
		[GET_BPF_OP(BPF_JGT)] = {0x0F, 0x47},  /* CMOVA */
		[GET_BPF_OP(EBPF_JLT)] = {0x0F, 0x42},  /* CMOVB */
		[GET_BPF_OP(BPF_JGE)] = {0x0F, 0x43},  /* CMOVAE */
		[GET_BPF_OP(EBPF_JLE)] = {0x0F, 0x46},  /* CMOVBE */
		[GET_BPF_OP(EBPF_JSGT)] = {0x0F, 0x4F}, /* CMOVG */
		[GET_BPF_OP(EBPF_JSLT)] = {0x0F, 0x4C}, /* CMOVL */
		[GET_BPF_OP(EBPF_JSGE)] = {0x0F, 0x4D}, /* CMOVGE */
		[GET_BPF_OP(EBPF_JSLE)] = {0x0F, 0x4E}, /* CMOVLE */
		[GET_BPF_OP(BPF_JSET)] = {0x0F, 0x45}, /* CMOVNE */
	};

	bop = GET_BPF_OP(op);

	emit_rex(st, op, dreg, sreg);
	emit_bytes(st, ops[bop], sizeof(ops[bop]));
	emit_modregrm(st, MOD_DIRECT, dreg, sreg);
}

/*
 * emit one of:
 * je <ofs>
 * jne <ofs>
 * ja <ofs>
 * jb <ofs>
 * jae <ofs>
 * jbe <ofs>
 * jg <ofs>
 * jl <ofs>
 * jge <ofs>
 * jle <ofs>
 * where 'ofs' is the target offset for the native code.
 */
static void
emit_abs_jcc(struct bpf_jit_state *st, uint32_t op, int32_t ofs)
{
	uint32_t bop, imsz;
	int32_t joff;

	static const uint8_t op8[] = {
		[GET_BPF_OP(BPF_JEQ)] = 0x74,  /* JE */
		[GET_BPF_OP(EBPF_JNE)] = 0x75,  /* JNE */
		[GET_BPF_OP(BPF_JGT)] = 0x77,  /* JA */
		[GET_BPF_OP(EBPF_JLT)] = 0x72,  /* JB */
		[GET_BPF_OP(BPF_JGE)] = 0x73,  /* JAE */
		[GET_BPF_OP(EBPF_JLE)] = 0x76,  /* JBE */
		[GET_BPF_OP(EBPF_JSGT)] = 0x7F, /* JG */
		[GET_BPF_OP(EBPF_JSLT)] = 0x7C, /* JL */
		[GET_BPF_OP(EBPF_JSGE)] = 0x7D, /*JGE */
		[GET_BPF_OP(EBPF_JSLE)] = 0x7E, /* JLE */
		[GET_BPF_OP(BPF_JSET)] = 0x75, /*JNE */
	};

	static const uint8_t op32[][2] = {
		[GET_BPF_OP(BPF_JEQ)] = {0x0F, 0x84},  /* JE */
		[GET_BPF_OP(EBPF_JNE)] = {0x0F, 0x85},  /* JNE */
		[GET_BPF_OP(BPF_JGT)] = {0x0F, 0x87},  /* JA */
		[GET_BPF_OP(EBPF_JLT)] = {0x0F, 0x82},  /* JB */
		[GET_BPF_OP(BPF_JGE)] = {0x0F, 0x83},  /* JAE */
		[GET_BPF_OP(EBPF_JLE)] = {0x0F, 0x86},  /* JBE */
		[GET_BPF_OP(EBPF_JSGT)] = {0x0F, 0x8F}, /* JG */
		[GET_BPF_OP(EBPF_JSLT)] = {0x0F, 0x8C}, /* JL */
		[GET_BPF_OP(EBPF_JSGE)] = {0x0F, 0x8D}, /*JGE */
		[GET_BPF_OP(EBPF_JSLE)] = {0x0F, 0x8E}, /* JLE */
		[GET_BPF_OP(BPF_JSET)] = {0x0F, 0x85}, /*JNE */
	};

	const int32_t sz8 = sizeof(op8[0]) + sizeof(uint8_t);
	const int32_t sz32 = sizeof(op32[0]) + sizeof(uint32_t);

	/* max possible jcc instruction size */
	const int32_t iszm = RTE_MAX(sz8, sz32);

	joff = ofs - st->sz;
	imsz = RTE_MAX(imm_size(joff), imm_size(joff + iszm));

	bop = GET_BPF_OP(op);

	if (imsz == 1) {
		emit_bytes(st, &op8[bop], sizeof(op8[bop]));
		joff -= sz8;
	} else {
		emit_bytes(st, op32[bop], sizeof(op32[bop]));
		joff -= sz32;
	}

	emit_imm(st, joff, imsz);
}

/*
 * emit one of:
 * je <ofs>
 * jne <ofs>
 * ja <ofs>
 * jb <ofs>
 * jae <ofs>
 * jbe <ofs>
 * jg <ofs>
 * jl <ofs>
 * jge <ofs>
 * jle <ofs>
 * where 'ofs' is the target offset for the BPF bytecode.
 */
static void
emit_jcc(struct bpf_jit_state *st, uint32_t op, int32_t ofs)
{
	emit_abs_jcc(st, op, st->off[st->idx + ofs]);
}


/*
 * emit cmp <imm>, %<dreg>
 */
static void
emit_cmp_imm(struct bpf_jit_state *st, uint32_t op, uint32_t dreg, uint32_t imm)
{
	uint8_t ops;
	uint32_t imsz;

	const uint8_t op8 = 0x83;
	const uint8_t op32 = 0x81;
	const uint8_t mods = 7;

	imsz = imm_size(imm);
	ops = (imsz == 1) ? op8 : op32;

	emit_rex(st, op, 0, dreg);
	emit_bytes(st, &ops, sizeof(ops));
	emit_modregrm(st, MOD_DIRECT, mods, dreg);
	emit_imm(st, imm, imsz);
}

/*
 * emit test <imm>, %<dreg>
 */
static void
emit_tst_imm(struct bpf_jit_state *st, uint32_t op, uint32_t dreg, uint32_t imm)
{
	const uint8_t ops = 0xF7;
	const uint8_t mods = 0;

	emit_rex(st, op, 0, dreg);
	emit_bytes(st, &ops, sizeof(ops));
	emit_modregrm(st, MOD_DIRECT, mods, dreg);
	emit_imm(st, imm, imm_size(imm));
}

static void
emit_jcc_imm(struct bpf_jit_state *st, uint32_t op, uint32_t dreg,
	uint32_t imm, int32_t ofs)
{
	if (BPF_OP(op) == BPF_JSET)
		emit_tst_imm(st, EBPF_ALU64, dreg, imm);
	else
		emit_cmp_imm(st, EBPF_ALU64, dreg, imm);

	emit_jcc(st, op, ofs);
}

/*
 * emit test %<sreg>, %<dreg>
 */
static void
emit_tst_reg(struct bpf_jit_state *st, uint32_t op, uint32_t sreg,
	uint32_t dreg)
{
	const uint8_t ops = 0x85;

	emit_rex(st, op, sreg, dreg);
	emit_bytes(st, &ops, sizeof(ops));
	emit_modregrm(st, MOD_DIRECT, sreg, dreg);
}

/*
 * emit cmp %<sreg>, %<dreg>
 */
static void
emit_cmp_reg(struct bpf_jit_state *st, uint32_t op, uint32_t sreg,
	uint32_t dreg)
{
	const uint8_t ops = 0x39;

	emit_rex(st, op, sreg, dreg);
	emit_bytes(st, &ops, sizeof(ops));
	emit_modregrm(st, MOD_DIRECT, sreg, dreg);

}

static void
emit_jcc_reg(struct bpf_jit_state *st, uint32_t op, uint32_t sreg,
	uint32_t dreg, int32_t ofs)
{
	if (BPF_OP(op) == BPF_JSET)
		emit_tst_reg(st, EBPF_ALU64, sreg, dreg);
	else
		emit_cmp_reg(st, EBPF_ALU64, sreg, dreg);

	emit_jcc(st, op, ofs);
}

/*
 * note that rax:rdx are implicitly used as source/destination registers,
 * so some reg spillage is necessary.
 * emit:
 * mov %rax, %r11
 * mov %rdx, %r10
 * mov %<dreg>, %rax
 * xor %rdx, %rdx
 * for divisor as immediate value:
 *   mov <imm>, %r9
 * div %<divisor_reg>
 * mov %r10, %rdx
 * mov %rax, %<dreg>
 * mov %r11, %rax
 * either:
 *   mov %rax, %<dreg>
 * OR
 *   mov %rdx, %<dreg>
 * mov %r11, %rax
 * mov %r10, %rdx
 */
static void
emit_div(struct bpf_jit_state *st, uint32_t op, uint32_t sreg, uint32_t dreg,
	uint32_t imm)
{
	uint32_t sr;

	const uint8_t ops = 0xF7;
	const uint8_t mods = 6;

	if (BPF_SRC(op) == BPF_X) {

		/* check that src divisor is not zero */
		emit_tst_reg(st, BPF_CLASS(op), sreg, sreg);

		/* exit with return value zero */
		emit_movcc_reg(st, BPF_CLASS(op) | BPF_JEQ | BPF_X, sreg, RAX);
		emit_abs_jcc(st, BPF_JMP | BPF_JEQ | BPF_K, st->exit.off);
	}

	/* save rax & rdx */
	if (dreg != RAX)
		emit_mov_reg(st, EBPF_ALU64 | EBPF_MOV | BPF_X, RAX, REG_TMP0);
	if (dreg != RDX)
		emit_mov_reg(st, EBPF_ALU64 | EBPF_MOV | BPF_X, RDX, REG_TMP1);

	/* fill rax & rdx */
	emit_mov_reg(st, EBPF_ALU64 | EBPF_MOV | BPF_X, dreg, RAX);
	emit_mov_imm(st, EBPF_ALU64 | EBPF_MOV | BPF_K, RDX, 0);

	if (BPF_SRC(op) == BPF_X) {
		sr = sreg;
		if (sr == RAX)
			sr = REG_TMP0;
		else if (sr == RDX)
			sr = REG_TMP1;
	} else {
		sr = REG_DIV_IMM;
		emit_mov_imm(st, EBPF_ALU64 | EBPF_MOV | BPF_K, sr, imm);
	}

	emit_rex(st, op, 0, sr);
	emit_bytes(st, &ops, sizeof(ops));
	emit_modregrm(st, MOD_DIRECT, mods, sr);

	if (BPF_OP(op) == BPF_DIV)
		emit_mov_reg(st, EBPF_ALU64 | EBPF_MOV | BPF_X, RAX, dreg);
	else
		emit_mov_reg(st, EBPF_ALU64 | EBPF_MOV | BPF_X, RDX, dreg);

	if (dreg != RAX)
		emit_mov_reg(st, EBPF_ALU64 | EBPF_MOV | BPF_X, REG_TMP0, RAX);
	if (dreg != RDX)
		emit_mov_reg(st, EBPF_ALU64 | EBPF_MOV | BPF_X, REG_TMP1, RDX);
}

/*
 * helper function, used by emit_ld_mbuf().
 * generates code for 'fast_path':
 * calculate load offset and check is it inside first packet segment.
 */
static void
emit_ldmb_fast_path(struct bpf_jit_state *st, const uint32_t rg[EBPF_REG_7],
	uint32_t sreg, uint32_t mode, uint32_t sz, uint32_t imm,
	const int32_t ofs[LDMB_OFS_NUM])
{
	/* make R2 contain *off* value */

	if (sreg != rg[EBPF_REG_2]) {
		emit_mov_imm(st, EBPF_ALU64 | EBPF_MOV | BPF_K,
			rg[EBPF_REG_2], imm);
		if (mode == BPF_IND)
			emit_alu_reg(st, EBPF_ALU64 | BPF_ADD | BPF_X,
				sreg, rg[EBPF_REG_2]);
	} else
		/* BPF_IND with sreg == R2 */
		emit_alu_imm(st, EBPF_ALU64 | BPF_ADD | BPF_K,
			rg[EBPF_REG_2], imm);

	/* R3 = mbuf->data_len */
	emit_ld_reg(st, BPF_LDX | BPF_MEM | BPF_H,
		rg[EBPF_REG_6], rg[EBPF_REG_3],
		offsetof(struct rte_mbuf, data_len));

	/* R3 = R3 - R2 */
	emit_alu_reg(st, EBPF_ALU64 | BPF_SUB | BPF_X,
		rg[EBPF_REG_2], rg[EBPF_REG_3]);

	/* JSLT R3, <sz> <slow_path> */
	emit_cmp_imm(st, EBPF_ALU64, rg[EBPF_REG_3], sz);
	emit_abs_jcc(st, BPF_JMP | EBPF_JSLT | BPF_K, ofs[LDMB_SLP_OFS]);

	/* R3 = mbuf->data_off */
	emit_ld_reg(st, BPF_LDX | BPF_MEM | BPF_H,
		rg[EBPF_REG_6], rg[EBPF_REG_3],
		offsetof(struct rte_mbuf, data_off));

	/* R0 = mbuf->buf_addr */
	emit_ld_reg(st, BPF_LDX | BPF_MEM | EBPF_DW,
		rg[EBPF_REG_6], rg[EBPF_REG_0],
		offsetof(struct rte_mbuf, buf_addr));

	/* R0 = R0 + R3 */
	emit_alu_reg(st, EBPF_ALU64 | BPF_ADD | BPF_X,
		rg[EBPF_REG_3], rg[EBPF_REG_0]);

	/* R0 = R0 + R2 */
	emit_alu_reg(st, EBPF_ALU64 | BPF_ADD | BPF_X,
		rg[EBPF_REG_2], rg[EBPF_REG_0]);

	/* JMP <fin_part> */
	emit_abs_jmp(st, ofs[LDMB_FIN_OFS]);
}

/*
 * helper function, used by emit_ld_mbuf().
 * generates code for 'slow_path':
 * call __rte_pktmbuf_read() and check return value.
 */
static void
emit_ldmb_slow_path(struct bpf_jit_state *st, const uint32_t rg[EBPF_REG_7],
	uint32_t sz)
{
	/* make R3 contain *len* value (1/2/4) */

	emit_mov_imm(st, EBPF_ALU64 | EBPF_MOV | BPF_K, rg[EBPF_REG_3], sz);

	/* make R4 contain (RBP - ldmb.stack_ofs) */

	emit_mov_reg(st, EBPF_ALU64 | EBPF_MOV | BPF_X, RBP, rg[EBPF_REG_4]);
	emit_alu_imm(st, EBPF_ALU64 | BPF_SUB | BPF_K, rg[EBPF_REG_4],
		st->ldmb.stack_ofs);

	/* make R1 contain mbuf ptr */

	emit_mov_reg(st, EBPF_ALU64 | EBPF_MOV | BPF_X,
		rg[EBPF_REG_6], rg[EBPF_REG_1]);

	/* call rte_pktmbuf_read */
	emit_call(st, (uintptr_t)__rte_pktmbuf_read);

	/* check that return value (R0) is not zero */
	emit_tst_reg(st, EBPF_ALU64, rg[EBPF_REG_0], rg[EBPF_REG_0]);
	emit_abs_jcc(st, BPF_JMP | BPF_JEQ | BPF_K, st->exit.off);
}

/*
 * helper function, used by emit_ld_mbuf().
 * generates final part of code for BPF_ABS/BPF_IND load:
 * perform data load and endianness conversion.
 * expects dreg to contain valid data pointer.
 */
static void
emit_ldmb_fin(struct bpf_jit_state *st, uint32_t dreg, uint32_t opsz,
	uint32_t sz)
{
	emit_ld_reg(st, BPF_LDX | BPF_MEM | opsz, dreg, dreg, 0);
	if (sz != sizeof(uint8_t))
		emit_be2le(st, dreg, sz * CHAR_BIT);
}

/*
 * emit code for BPF_ABS/BPF_IND load.
 * generates the following construction:
 * fast_path:
 *   off = ins->sreg + ins->imm
 *   if (mbuf->data_len - off < ins->opsz)
 *      goto slow_path;
 *   ptr = mbuf->buf_addr + mbuf->data_off + off;
 *   goto fin_part;
 * slow_path:
 *   typeof(ins->opsz) buf; //allocate space on the stack
 *   ptr = __rte_pktmbuf_read(mbuf, off, ins->opsz, &buf);
 *   if (ptr == NULL)
 *      goto exit_label;
 * fin_part:
 *   res = *(typeof(ins->opsz))ptr;
 *   res = bswap(res);
 */
static void
emit_ld_mbuf(struct bpf_jit_state *st, uint32_t op, uint32_t sreg, uint32_t imm)
{
	uint32_t i, mode, opsz, sz;
	uint32_t rg[EBPF_REG_7];
	int32_t ofs[LDMB_OFS_NUM];

	mode = BPF_MODE(op);
	opsz = BPF_SIZE(op);
	sz = bpf_size(opsz);

	for (i = 0; i != RTE_DIM(rg); i++)
		rg[i] = ebpf2x86[i];

	/* fill with fake offsets */
	for (i = 0; i != RTE_DIM(ofs); i++)
		ofs[i] = st->sz + INT8_MAX;

	/* dry run first to calculate jump offsets */

	ofs[LDMB_FSP_OFS] = st->sz;
	emit_ldmb_fast_path(st, rg, sreg, mode, sz, imm, ofs);
	ofs[LDMB_SLP_OFS] = st->sz;
	emit_ldmb_slow_path(st, rg, sz);
	ofs[LDMB_FIN_OFS] = st->sz;
	emit_ldmb_fin(st, rg[EBPF_REG_0], opsz, sz);

	RTE_VERIFY(ofs[LDMB_FIN_OFS] - ofs[LDMB_FSP_OFS] <= INT8_MAX);

	/* reset dry-run code and do a proper run */

	st->sz = ofs[LDMB_FSP_OFS];
	emit_ldmb_fast_path(st, rg, sreg, mode, sz, imm, ofs);
	emit_ldmb_slow_path(st, rg, sz);
	emit_ldmb_fin(st, rg[EBPF_REG_0], opsz, sz);
}

static void
emit_prolog(struct bpf_jit_state *st, int32_t stack_size)
{
	uint32_t i;
	int32_t spil, ofs;

	spil = 0;
	for (i = 0; i != RTE_DIM(save_regs); i++)
		spil += INUSE(st->reguse, save_regs[i]);

	/* we can avoid touching the stack at all */
	if (spil == 0)
		return;


	emit_alu_imm(st, EBPF_ALU64 | BPF_SUB | BPF_K, RSP,
		spil * sizeof(uint64_t));

	ofs = 0;
	for (i = 0; i != RTE_DIM(save_regs); i++) {
		if (INUSE(st->reguse, save_regs[i]) != 0) {
			emit_st_reg(st, BPF_STX | BPF_MEM | EBPF_DW,
				save_regs[i], RSP, ofs);
			ofs += sizeof(uint64_t);
		}
	}

	if (INUSE(st->reguse, RBP) != 0) {
		emit_mov_reg(st, EBPF_ALU64 | EBPF_MOV | BPF_X, RSP, RBP);
		emit_alu_imm(st, EBPF_ALU64 | BPF_SUB | BPF_K, RSP, stack_size);
	}
}

/*
 * emit ret
 */
static void
emit_ret(struct bpf_jit_state *st)
{
	const uint8_t ops = 0xC3;

	emit_bytes(st, &ops, sizeof(ops));
}

static void
emit_epilog(struct bpf_jit_state *st)
{
	uint32_t i;
	int32_t spil, ofs;

	/* if we already have an epilog generate a jump to it */
	if (st->exit.num++ != 0) {
		emit_abs_jmp(st, st->exit.off);
		return;
	}

	/* store offset of epilog block */
	st->exit.off = st->sz;

	spil = 0;
	for (i = 0; i != RTE_DIM(save_regs); i++)
		spil += INUSE(st->reguse, save_regs[i]);

	if (spil != 0) {

		if (INUSE(st->reguse, RBP) != 0)
			emit_mov_reg(st, EBPF_ALU64 | EBPF_MOV | BPF_X,
				RBP, RSP);

		ofs = 0;
		for (i = 0; i != RTE_DIM(save_regs); i++) {
			if (INUSE(st->reguse, save_regs[i]) != 0) {
				emit_ld_reg(st, BPF_LDX | BPF_MEM | EBPF_DW,
					RSP, save_regs[i], ofs);
				ofs += sizeof(uint64_t);
			}
		}

		emit_alu_imm(st, EBPF_ALU64 | BPF_ADD | BPF_K, RSP,
			spil * sizeof(uint64_t));
	}

	emit_ret(st);
}

/*
 * walk through bpf code and translate them x86_64 one.
 */
static int
emit(struct bpf_jit_state *st, const struct rte_bpf *bpf)
{
	uint32_t i, dr, op, sr;
	const struct ebpf_insn *ins;

	/* reset state fields */
	st->sz = 0;
	st->exit.num = 0;
	st->ldmb.stack_ofs = bpf->stack_sz;

	emit_prolog(st, bpf->stack_sz);

	for (i = 0; i != bpf->prm.nb_ins; i++) {

		st->idx = i;
		st->off[i] = st->sz;

		ins = bpf->prm.ins + i;

		dr = ebpf2x86[ins->dst_reg];
		sr = ebpf2x86[ins->src_reg];
		op = ins->code;

		switch (op) {
		/* 32 bit ALU IMM operations */
		case (BPF_ALU | BPF_ADD | BPF_K):
		case (BPF_ALU | BPF_SUB | BPF_K):
		case (BPF_ALU | BPF_AND | BPF_K):
		case (BPF_ALU | BPF_OR | BPF_K):
		case (BPF_ALU | BPF_XOR | BPF_K):
			emit_alu_imm(st, op, dr, ins->imm);
			break;
		case (BPF_ALU | BPF_LSH | BPF_K):
		case (BPF_ALU | BPF_RSH | BPF_K):
			emit_shift_imm(st, op, dr, ins->imm);
			break;
		case (BPF_ALU | EBPF_MOV | BPF_K):
			emit_mov_imm(st, op, dr, ins->imm);
			break;
		/* 32 bit ALU REG operations */
		case (BPF_ALU | BPF_ADD | BPF_X):
		case (BPF_ALU | BPF_SUB | BPF_X):
		case (BPF_ALU | BPF_AND | BPF_X):
		case (BPF_ALU | BPF_OR | BPF_X):
		case (BPF_ALU | BPF_XOR | BPF_X):
			emit_alu_reg(st, op, sr, dr);
			break;
		case (BPF_ALU | BPF_LSH | BPF_X):
		case (BPF_ALU | BPF_RSH | BPF_X):
			emit_shift_reg(st, op, sr, dr);
			break;
		case (BPF_ALU | EBPF_MOV | BPF_X):
			emit_mov_reg(st, op, sr, dr);
			break;
		case (BPF_ALU | BPF_NEG):
			emit_neg(st, op, dr);
			break;
		case (BPF_ALU | EBPF_END | EBPF_TO_BE):
			emit_be2le(st, dr, ins->imm);
			break;
		case (BPF_ALU | EBPF_END | EBPF_TO_LE):
			emit_le2be(st, dr, ins->imm);
			break;
		/* 64 bit ALU IMM operations */
		case (EBPF_ALU64 | BPF_ADD | BPF_K):
		case (EBPF_ALU64 | BPF_SUB | BPF_K):
		case (EBPF_ALU64 | BPF_AND | BPF_K):
		case (EBPF_ALU64 | BPF_OR | BPF_K):
		case (EBPF_ALU64 | BPF_XOR | BPF_K):
			emit_alu_imm(st, op, dr, ins->imm);
			break;
		case (EBPF_ALU64 | BPF_LSH | BPF_K):
		case (EBPF_ALU64 | BPF_RSH | BPF_K):
		case (EBPF_ALU64 | EBPF_ARSH | BPF_K):
			emit_shift_imm(st, op, dr, ins->imm);
			break;
		case (EBPF_ALU64 | EBPF_MOV | BPF_K):
			emit_mov_imm(st, op, dr, ins->imm);
			break;
		/* 64 bit ALU REG operations */
		case (EBPF_ALU64 | BPF_ADD | BPF_X):
		case (EBPF_ALU64 | BPF_SUB | BPF_X):
		case (EBPF_ALU64 | BPF_AND | BPF_X):
		case (EBPF_ALU64 | BPF_OR | BPF_X):
		case (EBPF_ALU64 | BPF_XOR | BPF_X):
			emit_alu_reg(st, op, sr, dr);
			break;
		case (EBPF_ALU64 | BPF_LSH | BPF_X):
		case (EBPF_ALU64 | BPF_RSH | BPF_X):
		case (EBPF_ALU64 | EBPF_ARSH | BPF_X):
			emit_shift_reg(st, op, sr, dr);
			break;
		case (EBPF_ALU64 | EBPF_MOV | BPF_X):
			emit_mov_reg(st, op, sr, dr);
			break;
		case (EBPF_ALU64 | BPF_NEG):
			emit_neg(st, op, dr);
			break;
		/* multiply instructions */
		case (BPF_ALU | BPF_MUL | BPF_K):
		case (BPF_ALU | BPF_MUL | BPF_X):
		case (EBPF_ALU64 | BPF_MUL | BPF_K):
		case (EBPF_ALU64 | BPF_MUL | BPF_X):
			emit_mul(st, op, sr, dr, ins->imm);
			break;
		/* divide instructions */
		case (BPF_ALU | BPF_DIV | BPF_K):
		case (BPF_ALU | BPF_MOD | BPF_K):
		case (BPF_ALU | BPF_DIV | BPF_X):
		case (BPF_ALU | BPF_MOD | BPF_X):
		case (EBPF_ALU64 | BPF_DIV | BPF_K):
		case (EBPF_ALU64 | BPF_MOD | BPF_K):
		case (EBPF_ALU64 | BPF_DIV | BPF_X):
		case (EBPF_ALU64 | BPF_MOD | BPF_X):
			emit_div(st, op, sr, dr, ins->imm);
			break;
		/* load instructions */
		case (BPF_LDX | BPF_MEM | BPF_B):
		case (BPF_LDX | BPF_MEM | BPF_H):
		case (BPF_LDX | BPF_MEM | BPF_W):
		case (BPF_LDX | BPF_MEM | EBPF_DW):
			emit_ld_reg(st, op, sr, dr, ins->off);
			break;
		/* load 64 bit immediate value */
		case (BPF_LD | BPF_IMM | EBPF_DW):
			emit_ld_imm64(st, dr, ins[0].imm, ins[1].imm);
			i++;
			break;
		/* load absolute/indirect instructions */
		case (BPF_LD | BPF_ABS | BPF_B):
		case (BPF_LD | BPF_ABS | BPF_H):
		case (BPF_LD | BPF_ABS | BPF_W):
		case (BPF_LD | BPF_IND | BPF_B):
		case (BPF_LD | BPF_IND | BPF_H):
		case (BPF_LD | BPF_IND | BPF_W):
			emit_ld_mbuf(st, op, sr, ins->imm);
			break;
		/* store instructions */
		case (BPF_STX | BPF_MEM | BPF_B):
		case (BPF_STX | BPF_MEM | BPF_H):
		case (BPF_STX | BPF_MEM | BPF_W):
		case (BPF_STX | BPF_MEM | EBPF_DW):
			emit_st_reg(st, op, sr, dr, ins->off);
			break;
		case (BPF_ST | BPF_MEM | BPF_B):
		case (BPF_ST | BPF_MEM | BPF_H):
		case (BPF_ST | BPF_MEM | BPF_W):
		case (BPF_ST | BPF_MEM | EBPF_DW):
			emit_st_imm(st, op, dr, ins->imm, ins->off);
			break;
		/* atomic add instructions */
		case (BPF_STX | EBPF_XADD | BPF_W):
		case (BPF_STX | EBPF_XADD | EBPF_DW):
			emit_st_xadd(st, op, sr, dr, ins->off);
			break;
		/* jump instructions */
		case (BPF_JMP | BPF_JA):
			emit_jmp(st, ins->off + 1);
			break;
		/* jump IMM instructions */
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
			emit_jcc_imm(st, op, dr, ins->imm, ins->off + 1);
			break;
		/* jump REG instructions */
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
			emit_jcc_reg(st, op, sr, dr, ins->off + 1);
			break;
		/* call instructions */
		case (BPF_JMP | EBPF_CALL):
			emit_call(st,
				(uintptr_t)bpf->prm.xsym[ins->imm].func.val);
			break;
		/* return instruction */
		case (BPF_JMP | EBPF_EXIT):
			emit_epilog(st);
			break;
		default:
			RTE_BPF_LOG_LINE(ERR,
				"%s(%p): invalid opcode %#x at pc: %u;",
				__func__, bpf, ins->code, i);
			return -EINVAL;
		}
	}

	return 0;
}

/*
 * produce a native ISA version of the given BPF code.
 */
int
__rte_bpf_jit_x86(struct rte_bpf *bpf)
{
	int32_t rc;
	uint32_t i;
	size_t sz;
	struct bpf_jit_state st;

	/* init state */
	memset(&st, 0, sizeof(st));
	st.off = malloc(bpf->prm.nb_ins * sizeof(st.off[0]));
	if (st.off == NULL)
		return -ENOMEM;

	/* fill with fake offsets */
	st.exit.off = INT32_MAX;
	for (i = 0; i != bpf->prm.nb_ins; i++)
		st.off[i] = INT32_MAX;

	/*
	 * dry runs, used to calculate total code size and valid jump offsets.
	 * stop when we get minimal possible size
	 */
	do {
		sz = st.sz;
		rc = emit(&st, bpf);
	} while (rc == 0 && sz != st.sz);

	if (rc == 0) {

		/* allocate memory needed */
		st.ins = mmap(NULL, st.sz, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (st.ins == MAP_FAILED)
			rc = -ENOMEM;
		else
			/* generate code */
			rc = emit(&st, bpf);
	}

	if (rc == 0 && mprotect(st.ins, st.sz, PROT_READ | PROT_EXEC) != 0)
		rc = -ENOMEM;

	if (rc != 0)
		munmap(st.ins, st.sz);
	else {
		bpf->jit.func = (void *)st.ins;
		bpf->jit.sz = st.sz;
	}

	free(st.off);
	return rc;
}
