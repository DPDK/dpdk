/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <errno.h>
#include <stdbool.h>

#include <rte_common.h>

#include "bpf_impl.h"

#define A64_REG_MASK(r)		((r) & 0x1f)
#define A64_INVALID_OP_CODE	(0xffffffff)

#define TMP_REG_1		(EBPF_REG_10 + 1)
#define TMP_REG_2		(EBPF_REG_10 + 2)
#define TMP_REG_3		(EBPF_REG_10 + 3)

#define EBPF_FP			(EBPF_REG_10)
#define EBPF_OP_GET(op)		(BPF_OP(op) >> 4)

#define A64_R(x)		x
#define A64_FP			29
#define A64_LR			30
#define A64_SP			31
#define A64_ZR			31

#define check_imm(n, val) (((val) >= 0) ? !!((val) >> (n)) : !!((~val) >> (n)))
#define mask_imm(n, val) ((val) & ((1 << (n)) - 1))

struct ebpf_a64_map {
	uint32_t off; /* eBPF to arm64 insn offset mapping for jump */
	uint8_t off_to_b; /* Offset to branch instruction delta */
};

struct a64_jit_ctx {
	size_t stack_sz;          /* Stack size */
	uint32_t *ins;            /* ARM64 instructions. NULL if first pass */
	struct ebpf_a64_map *map; /* eBPF to arm64 insn mapping for jump */
	uint32_t idx;             /* Current instruction index */
	uint32_t program_start;   /* Program index, Just after prologue */
	uint32_t program_sz;      /* Program size. Found in first pass */
	uint8_t foundcall;        /* Found EBPF_CALL class code in eBPF pgm */
};

static int
check_reg(uint8_t r)
{
	return (r > 31) ? 1 : 0;
}

static int
is_first_pass(struct a64_jit_ctx *ctx)
{
	return (ctx->ins == NULL);
}

static int
check_invalid_args(struct a64_jit_ctx *ctx, uint32_t limit)
{
	uint32_t idx;

	if (is_first_pass(ctx))
		return 0;

	for (idx = 0; idx < limit; idx++) {
		if (rte_le_to_cpu_32(ctx->ins[idx]) == A64_INVALID_OP_CODE) {
			RTE_BPF_LOG(ERR,
				"%s: invalid opcode at %u;\n", __func__, idx);
			return -EINVAL;
		}
	}
	return 0;
}

/* Emit an instruction */
static inline void
emit_insn(struct a64_jit_ctx *ctx, uint32_t insn, int error)
{
	if (error)
		insn = A64_INVALID_OP_CODE;

	if (ctx->ins)
		ctx->ins[ctx->idx] = rte_cpu_to_le_32(insn);

	ctx->idx++;
}

static void
emit_ret(struct a64_jit_ctx *ctx)
{
	emit_insn(ctx, 0xd65f03c0, 0);
}

static void
emit_add_sub_imm(struct a64_jit_ctx *ctx, bool is64, bool sub, uint8_t rd,
		 uint8_t rn, int16_t imm12)
{
	uint32_t insn, imm;

	imm = mask_imm(12, imm12);
	insn = (!!is64) << 31;
	insn |= (!!sub) << 30;
	insn |= 0x11000000;
	insn |= rd;
	insn |= rn << 5;
	insn |= imm << 10;

	emit_insn(ctx, insn,
		  check_reg(rd) || check_reg(rn) || check_imm(12, imm12));
}

static void
emit_add_imm_64(struct a64_jit_ctx *ctx, uint8_t rd, uint8_t rn, uint16_t imm12)
{
	emit_add_sub_imm(ctx, 1, 0, rd, rn, imm12);
}

static void
emit_sub_imm_64(struct a64_jit_ctx *ctx, uint8_t rd, uint8_t rn, uint16_t imm12)
{
	emit_add_sub_imm(ctx, 1, 1, rd, rn, imm12);
}

static void
emit_mov(struct a64_jit_ctx *ctx, bool is64, uint8_t rd, uint8_t rn)
{
	emit_add_sub_imm(ctx, is64, 0, rd, rn, 0);
}

static void
emit_mov_64(struct a64_jit_ctx *ctx, uint8_t rd, uint8_t rn)
{
	emit_mov(ctx, 1, rd, rn);
}

static void
emit_ls_pair_64(struct a64_jit_ctx *ctx, uint8_t rt, uint8_t rt2, uint8_t rn,
		bool push, bool load, bool pre_index)
{
	uint32_t insn;

	insn = (!!load) << 22;
	insn |= (!!pre_index) << 24;
	insn |= 0xa8800000;
	insn |= rt;
	insn |= rn << 5;
	insn |= rt2 << 10;
	if (push)
		insn |= 0x7e << 15; /* 0x7e means -2 with imm7 */
	else
		insn |= 0x2 << 15;

	emit_insn(ctx, insn, check_reg(rn) || check_reg(rt) || check_reg(rt2));

}

/* Emit stp rt, rt2, [sp, #-16]! */
static void
emit_stack_push(struct a64_jit_ctx *ctx, uint8_t rt, uint8_t rt2)
{
	emit_ls_pair_64(ctx, rt, rt2, A64_SP, 1, 0, 1);
}

/* Emit ldp rt, rt2, [sp, #16] */
static void
emit_stack_pop(struct a64_jit_ctx *ctx, uint8_t rt, uint8_t rt2)
{
	emit_ls_pair_64(ctx, rt, rt2, A64_SP, 0, 1, 0);
}

static uint8_t
ebpf_to_a64_reg(struct a64_jit_ctx *ctx, uint8_t reg)
{
	const uint32_t ebpf2a64_has_call[] = {
		/* Map A64 R7 register as EBPF return register */
		[EBPF_REG_0] = A64_R(7),
		/* Map A64 arguments register as EBPF arguments register */
		[EBPF_REG_1] = A64_R(0),
		[EBPF_REG_2] = A64_R(1),
		[EBPF_REG_3] = A64_R(2),
		[EBPF_REG_4] = A64_R(3),
		[EBPF_REG_5] = A64_R(4),
		/* Map A64 callee save register as EBPF callee save register */
		[EBPF_REG_6] = A64_R(19),
		[EBPF_REG_7] = A64_R(20),
		[EBPF_REG_8] = A64_R(21),
		[EBPF_REG_9] = A64_R(22),
		[EBPF_FP]    = A64_R(25),
		/* Map A64 scratch registers as temporary storage */
		[TMP_REG_1] = A64_R(9),
		[TMP_REG_2] = A64_R(10),
		[TMP_REG_3] = A64_R(11),
	};

	const uint32_t ebpf2a64_no_call[] = {
		/* Map A64 R7 register as EBPF return register */
		[EBPF_REG_0] = A64_R(7),
		/* Map A64 arguments register as EBPF arguments register */
		[EBPF_REG_1] = A64_R(0),
		[EBPF_REG_2] = A64_R(1),
		[EBPF_REG_3] = A64_R(2),
		[EBPF_REG_4] = A64_R(3),
		[EBPF_REG_5] = A64_R(4),
		/*
		 * EBPF program does not have EBPF_CALL op code,
		 * Map A64 scratch registers as EBPF callee save registers.
		 */
		[EBPF_REG_6] = A64_R(9),
		[EBPF_REG_7] = A64_R(10),
		[EBPF_REG_8] = A64_R(11),
		[EBPF_REG_9] = A64_R(12),
		/* Map A64 FP register as EBPF FP register */
		[EBPF_FP]    = A64_FP,
		/* Map remaining A64 scratch registers as temporary storage */
		[TMP_REG_1] = A64_R(13),
		[TMP_REG_2] = A64_R(14),
		[TMP_REG_3] = A64_R(15),
	};

	if (ctx->foundcall)
		return ebpf2a64_has_call[reg];
	else
		return ebpf2a64_no_call[reg];
}

/*
 * Procedure call standard for the arm64
 * -------------------------------------
 * R0..R7  - Parameter/result registers
 * R8      - Indirect result location register
 * R9..R15 - Scratch registers
 * R15     - Platform Register
 * R16     - First intra-procedure-call scratch register
 * R17     - Second intra-procedure-call temporary register
 * R19-R28 - Callee saved registers
 * R29     - Frame pointer
 * R30     - Link register
 * R31     - Stack pointer
 */
static void
emit_prologue_has_call(struct a64_jit_ctx *ctx)
{
	uint8_t r6, r7, r8, r9, fp;

	r6 = ebpf_to_a64_reg(ctx, EBPF_REG_6);
	r7 = ebpf_to_a64_reg(ctx, EBPF_REG_7);
	r8 = ebpf_to_a64_reg(ctx, EBPF_REG_8);
	r9 = ebpf_to_a64_reg(ctx, EBPF_REG_9);
	fp = ebpf_to_a64_reg(ctx, EBPF_FP);

	/*
	 * eBPF prog stack layout
	 *
	 *                               high
	 *       eBPF prologue       0:+-----+ <= original A64_SP
	 *                             |FP/LR|
	 *                         -16:+-----+ <= current A64_FP
	 *    Callee saved registers   | ... |
	 *             EBPF_FP =>  -64:+-----+
	 *                             |     |
	 *       eBPF prog stack       | ... |
	 *                             |     |
	 * (EBPF_FP - bpf->stack_sz)=> +-----+
	 * Pad for A64_SP 16B alignment| PAD |
	 * (EBPF_FP - ctx->stack_sz)=> +-----+ <= current A64_SP
	 *                             |     |
	 *                             | ... | Function call stack
	 *                             |     |
	 *                             +-----+
	 *                              low
	 */
	emit_stack_push(ctx, A64_FP, A64_LR);
	emit_mov_64(ctx, A64_FP, A64_SP);
	emit_stack_push(ctx, r6, r7);
	emit_stack_push(ctx, r8, r9);
	/*
	 * There is no requirement to save A64_R(28) in stack. Doing it here,
	 * because, A64_SP needs be to 16B aligned and STR vs STP
	 * takes same number of cycles(typically).
	 */
	emit_stack_push(ctx, fp, A64_R(28));
	emit_mov_64(ctx, fp, A64_SP);
	if (ctx->stack_sz)
		emit_sub_imm_64(ctx, A64_SP, A64_SP, ctx->stack_sz);
}

static void
emit_epilogue_has_call(struct a64_jit_ctx *ctx)
{
	uint8_t r6, r7, r8, r9, fp, r0;

	r6 = ebpf_to_a64_reg(ctx, EBPF_REG_6);
	r7 = ebpf_to_a64_reg(ctx, EBPF_REG_7);
	r8 = ebpf_to_a64_reg(ctx, EBPF_REG_8);
	r9 = ebpf_to_a64_reg(ctx, EBPF_REG_9);
	fp = ebpf_to_a64_reg(ctx, EBPF_FP);
	r0 = ebpf_to_a64_reg(ctx, EBPF_REG_0);

	if (ctx->stack_sz)
		emit_add_imm_64(ctx, A64_SP, A64_SP, ctx->stack_sz);
	emit_stack_pop(ctx, fp, A64_R(28));
	emit_stack_pop(ctx, r8, r9);
	emit_stack_pop(ctx, r6, r7);
	emit_stack_pop(ctx, A64_FP, A64_LR);
	emit_mov_64(ctx, A64_R(0), r0);
	emit_ret(ctx);
}

static void
emit_prologue_no_call(struct a64_jit_ctx *ctx)
{
	/*
	 * eBPF prog stack layout without EBPF_CALL opcode
	 *
	 *                               high
	 *    eBPF prologue(EBPF_FP) 0:+-----+ <= original A64_SP/current A64_FP
	 *                             |     |
	 *                             | ... |
	 *            eBPF prog stack  |     |
	 *                             |     |
	 * (EBPF_FP - bpf->stack_sz)=> +-----+
	 * Pad for A64_SP 16B alignment| PAD |
	 * (EBPF_FP - ctx->stack_sz)=> +-----+ <= current A64_SP
	 *                             |     |
	 *                             | ... | Function call stack
	 *                             |     |
	 *                             +-----+
	 *                              low
	 */
	if (ctx->stack_sz) {
		emit_mov_64(ctx, A64_FP, A64_SP);
		emit_sub_imm_64(ctx, A64_SP, A64_SP, ctx->stack_sz);
	}
}

static void
emit_epilogue_no_call(struct a64_jit_ctx *ctx)
{
	if (ctx->stack_sz)
		emit_add_imm_64(ctx, A64_SP, A64_SP, ctx->stack_sz);
	emit_mov_64(ctx, A64_R(0), ebpf_to_a64_reg(ctx, EBPF_REG_0));
	emit_ret(ctx);
}

static void
emit_prologue(struct a64_jit_ctx *ctx)
{
	if (ctx->foundcall)
		emit_prologue_has_call(ctx);
	else
		emit_prologue_no_call(ctx);

	ctx->program_start = ctx->idx;
}

static void
emit_epilogue(struct a64_jit_ctx *ctx)
{
	ctx->program_sz = ctx->idx - ctx->program_start;

	if (ctx->foundcall)
		emit_epilogue_has_call(ctx);
	else
		emit_epilogue_no_call(ctx);
}

static void
check_program_has_call(struct a64_jit_ctx *ctx, struct rte_bpf *bpf)
{
	const struct ebpf_insn *ins;
	uint8_t op;
	uint32_t i;

	for (i = 0; i != bpf->prm.nb_ins; i++) {
		ins = bpf->prm.ins + i;
		op = ins->code;

		switch (op) {
		/* Call imm */
		case (BPF_JMP | EBPF_CALL):
			ctx->foundcall = 1;
			return;
		}
	}
}

/*
 * Walk through eBPF code and translate them to arm64 one.
 */
static int
emit(struct a64_jit_ctx *ctx, struct rte_bpf *bpf)
{
	uint8_t op;
	const struct ebpf_insn *ins;
	uint32_t i;
	int rc;

	/* Reset context fields */
	ctx->idx = 0;
	/* arm64 SP must be aligned to 16 */
	ctx->stack_sz = RTE_ALIGN_MUL_CEIL(bpf->stack_sz, 16);

	emit_prologue(ctx);

	for (i = 0; i != bpf->prm.nb_ins; i++) {

		ins = bpf->prm.ins + i;
		op = ins->code;

		switch (op) {
		/* Return r0 */
		case (BPF_JMP | EBPF_EXIT):
			emit_epilogue(ctx);
			break;
		default:
			RTE_BPF_LOG(ERR,
				"%s(%p): invalid opcode %#x at pc: %u;\n",
				__func__, bpf, ins->code, i);
			return -EINVAL;
		}
	}
	rc = check_invalid_args(ctx, ctx->idx);

	return rc;
}

/*
 * Produce a native ISA version of the given BPF code.
 */
int
bpf_jit_arm64(struct rte_bpf *bpf)
{
	struct a64_jit_ctx ctx;
	size_t size;
	int rc;

	/* Init JIT context */
	memset(&ctx, 0, sizeof(ctx));

	/* Find eBPF program has call class or not */
	check_program_has_call(&ctx, bpf);

	/* First pass to calculate total code size and valid jump offsets */
	rc = emit(&ctx, bpf);
	if (rc)
		goto finish;

	size = ctx.idx * sizeof(uint32_t);
	/* Allocate JIT program memory */
	ctx.ins = mmap(NULL, size, PROT_READ | PROT_WRITE,
			       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (ctx.ins == MAP_FAILED) {
		rc = -ENOMEM;
		goto finish;
	}

	/* Second pass to generate code */
	rc = emit(&ctx, bpf);
	if (rc)
		goto munmap;

	rc = mprotect(ctx.ins, size, PROT_READ | PROT_EXEC) != 0;
	if (rc) {
		rc = -errno;
		goto munmap;
	}

	/* Flush the icache */
	__builtin___clear_cache(ctx.ins, ctx.ins + ctx.idx);

	bpf->jit.func = (void *)ctx.ins;
	bpf->jit.sz = size;

	goto finish;

munmap:
	munmap(ctx.ins, size);
finish:
	return rc;
}
