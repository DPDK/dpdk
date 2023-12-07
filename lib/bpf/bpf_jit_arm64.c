/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

#include <rte_common.h>
#include <rte_byteorder.h>

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
check_immr_imms(bool is64, uint8_t immr, uint8_t imms)
{
	const unsigned int width = is64 ? 64 : 32;

	if (immr >= width || imms >= width)
		return 1;

	return 0;
}

static int
check_mov_hw(bool is64, const uint8_t val)
{
	if (val == 16 || val == 0)
		return 0;
	else if (is64 && val != 64 && val != 48 && val != 32)
		return 1;

	return 0;
}

static int
check_ls_sz(uint8_t sz)
{
	if (sz == BPF_B || sz == BPF_H || sz == BPF_W || sz == EBPF_DW)
		return 0;

	return 1;
}

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
			RTE_BPF_LOG_LINE(ERR,
				"%s: invalid opcode at %u;", __func__, idx);
			return -EINVAL;
		}
	}
	return 0;
}

static int
jump_offset_init(struct a64_jit_ctx *ctx, struct rte_bpf *bpf)
{
	uint32_t i;

	ctx->map = malloc(bpf->prm.nb_ins * sizeof(ctx->map[0]));
	if (ctx->map == NULL)
		return -ENOMEM;

	/* Fill with fake offsets */
	for (i = 0; i != bpf->prm.nb_ins; i++) {
		ctx->map[i].off = INT32_MAX;
		ctx->map[i].off_to_b = 0;
	}
	return 0;
}

static void
jump_offset_fini(struct a64_jit_ctx *ctx)
{
	free(ctx->map);
}

static void
jump_offset_update(struct a64_jit_ctx *ctx, uint32_t ebpf_idx)
{
	if (is_first_pass(ctx))
		ctx->map[ebpf_idx].off = ctx->idx;
}

static void
jump_offset_to_branch_update(struct a64_jit_ctx *ctx, uint32_t ebpf_idx)
{
	if (is_first_pass(ctx))
		ctx->map[ebpf_idx].off_to_b = ctx->idx - ctx->map[ebpf_idx].off;

}

static int32_t
jump_offset_get(struct a64_jit_ctx *ctx, uint32_t from, int16_t offset)
{
	int32_t a64_from, a64_to;

	a64_from = ctx->map[from].off +  ctx->map[from].off_to_b;
	a64_to = ctx->map[from + offset + 1].off;

	if (a64_to == INT32_MAX)
		return a64_to;

	return a64_to - a64_from;
}

enum a64_cond_e {
	A64_EQ = 0x0, /* == */
	A64_NE = 0x1, /* != */
	A64_CS = 0x2, /* Unsigned >= */
	A64_CC = 0x3, /* Unsigned < */
	A64_MI = 0x4, /* < 0 */
	A64_PL = 0x5, /* >= 0 */
	A64_VS = 0x6, /* Overflow */
	A64_VC = 0x7, /* No overflow */
	A64_HI = 0x8, /* Unsigned > */
	A64_LS = 0x9, /* Unsigned <= */
	A64_GE = 0xa, /* Signed >= */
	A64_LT = 0xb, /* Signed < */
	A64_GT = 0xc, /* Signed > */
	A64_LE = 0xd, /* Signed <= */
	A64_AL = 0xe, /* Always */
};

static int
check_cond(uint8_t cond)
{
	return (cond >= A64_AL) ? 1 : 0;
}

static uint8_t
ebpf_to_a64_cond(uint8_t op)
{
	switch (BPF_OP(op)) {
	case BPF_JEQ:
		return A64_EQ;
	case BPF_JGT:
		return A64_HI;
	case EBPF_JLT:
		return A64_CC;
	case BPF_JGE:
		return A64_CS;
	case EBPF_JLE:
		return A64_LS;
	case BPF_JSET:
	case EBPF_JNE:
		return A64_NE;
	case EBPF_JSGT:
		return A64_GT;
	case EBPF_JSLT:
		return A64_LT;
	case EBPF_JSGE:
		return A64_GE;
	case EBPF_JSLE:
		return A64_LE;
	default:
		return UINT8_MAX;
	}
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

#define A64_MOVN 0
#define A64_MOVZ 2
#define A64_MOVK 3
static void
mov_imm(struct a64_jit_ctx *ctx, bool is64, uint8_t rd, uint8_t type,
	uint16_t imm16, uint8_t shift)
{
	uint32_t insn;

	insn = (!!is64) << 31;
	insn |= type << 29;
	insn |= 0x25 << 23;
	insn |= (shift/16) << 21;
	insn |= imm16 << 5;
	insn |= rd;

	emit_insn(ctx, insn, check_reg(rd) || check_mov_hw(is64, shift));
}

static void
emit_mov_imm32(struct a64_jit_ctx *ctx, bool is64, uint8_t rd, uint32_t val)
{
	uint16_t upper = val >> 16;
	uint16_t lower = val & 0xffff;

	/* Positive number */
	if ((val & 1UL << 31) == 0) {
		mov_imm(ctx, is64, rd, A64_MOVZ, lower, 0);
		if (upper)
			mov_imm(ctx, is64, rd, A64_MOVK, upper, 16);
	} else { /* Negative number */
		if (upper == 0xffff) {
			mov_imm(ctx, is64, rd, A64_MOVN, ~lower, 0);
		} else {
			mov_imm(ctx, is64, rd, A64_MOVN, ~upper, 16);
			if (lower != 0xffff)
				mov_imm(ctx, is64, rd, A64_MOVK, lower, 0);
		}
	}
}

static int
u16_blocks_weight(const uint64_t val, bool one)
{
	return (((val >>  0) & 0xffff) == (one ? 0xffff : 0x0000)) +
	       (((val >> 16) & 0xffff) == (one ? 0xffff : 0x0000)) +
	       (((val >> 32) & 0xffff) == (one ? 0xffff : 0x0000)) +
	       (((val >> 48) & 0xffff) == (one ? 0xffff : 0x0000));
}

static void
emit_mov_imm(struct a64_jit_ctx *ctx, bool is64, uint8_t rd, uint64_t val)
{
	uint64_t nval = ~val;
	int movn, sr;

	if (is64 == 0)
		return emit_mov_imm32(ctx, 0, rd, (uint32_t)(val & 0xffffffff));

	/* Find MOVN or MOVZ first */
	movn = u16_blocks_weight(val, true) > u16_blocks_weight(val, false);
	/* Find shift right value */
	sr = movn ? rte_fls_u64(nval) - 1 : rte_fls_u64(val) - 1;
	sr = RTE_ALIGN_FLOOR(sr, 16);
	sr = RTE_MAX(sr, 0);

	if (movn)
		mov_imm(ctx, 1, rd, A64_MOVN, (nval >> sr) & 0xffff, sr);
	else
		mov_imm(ctx, 1, rd, A64_MOVZ, (val >> sr) & 0xffff, sr);

	sr -= 16;
	while (sr >= 0) {
		if (((val >> sr) & 0xffff) != (movn ? 0xffff : 0x0000))
			mov_imm(ctx, 1, rd, A64_MOVK, (val >> sr) & 0xffff, sr);
		sr -= 16;
	}
}

static void
emit_ls(struct a64_jit_ctx *ctx, uint8_t sz, uint8_t rt, uint8_t rn, uint8_t rm,
	bool load)
{
	uint32_t insn;

	insn = 0x1c1 << 21;
	if (load)
		insn |= 1 << 22;
	if (sz == BPF_B)
		insn |= 0 << 30;
	else if (sz == BPF_H)
		insn |= 1 << 30;
	else if (sz == BPF_W)
		insn |= 2 << 30;
	else if (sz == EBPF_DW)
		insn |= 3 << 30;

	insn |= rm << 16;
	insn |= 0x1a << 10; /* LSL and S = 0 */
	insn |= rn << 5;
	insn |= rt;

	emit_insn(ctx, insn, check_reg(rt) || check_reg(rn) || check_reg(rm) ||
		  check_ls_sz(sz));
}

static void
emit_str(struct a64_jit_ctx *ctx, uint8_t sz, uint8_t rt, uint8_t rn,
	 uint8_t rm)
{
	emit_ls(ctx, sz, rt, rn, rm, 0);
}

static void
emit_ldr(struct a64_jit_ctx *ctx, uint8_t sz, uint8_t rt, uint8_t rn,
	 uint8_t rm)
{
	emit_ls(ctx, sz, rt, rn, rm, 1);
}

#define A64_ADD 0x58
#define A64_SUB 0x258
static void
emit_add_sub(struct a64_jit_ctx *ctx, bool is64, uint8_t rd, uint8_t rn,
	     uint8_t rm, uint16_t op)
{
	uint32_t insn;

	insn = (!!is64) << 31;
	insn |= op << 21; /* shift == 0 */
	insn |= rm << 16;
	insn |= rn << 5;
	insn |= rd;

	emit_insn(ctx, insn, check_reg(rd) || check_reg(rm));
}

static void
emit_add(struct a64_jit_ctx *ctx, bool is64, uint8_t rd, uint8_t rm)
{
	emit_add_sub(ctx, is64, rd, rd, rm, A64_ADD);
}

static void
emit_sub(struct a64_jit_ctx *ctx, bool is64, uint8_t rd, uint8_t rm)
{
	emit_add_sub(ctx, is64, rd, rd, rm, A64_SUB);
}

static void
emit_neg(struct a64_jit_ctx *ctx, bool is64, uint8_t rd)
{
	emit_add_sub(ctx, is64, rd, A64_ZR, rd, A64_SUB);
}

static void
emit_mul(struct a64_jit_ctx *ctx, bool is64, uint8_t rd, uint8_t rm)
{
	uint32_t insn;

	insn = (!!is64) << 31;
	insn |= 0xd8 << 21;
	insn |= rm << 16;
	insn |= A64_ZR << 10;
	insn |= rd << 5;
	insn |= rd;

	emit_insn(ctx, insn, check_reg(rd) || check_reg(rm));
}

#define A64_UDIV 0x2
#define A64_LSLV 0x8
#define A64_LSRV 0x9
#define A64_ASRV 0xA
static void
emit_data_process_two_src(struct a64_jit_ctx *ctx, bool is64, uint8_t rd,
			  uint8_t rn, uint8_t rm, uint16_t op)

{
	uint32_t insn;

	insn = (!!is64) << 31;
	insn |= 0xd6 << 21;
	insn |= rm << 16;
	insn |= op << 10;
	insn |= rn << 5;
	insn |= rd;

	emit_insn(ctx, insn, check_reg(rd) || check_reg(rm));
}

static void
emit_div(struct a64_jit_ctx *ctx, bool is64, uint8_t rd, uint8_t rm)
{
	emit_data_process_two_src(ctx, is64, rd, rd, rm, A64_UDIV);
}

static void
emit_lslv(struct a64_jit_ctx *ctx, bool is64, uint8_t rd, uint8_t rm)
{
	emit_data_process_two_src(ctx, is64, rd, rd, rm, A64_LSLV);
}

static void
emit_lsrv(struct a64_jit_ctx *ctx, bool is64, uint8_t rd, uint8_t rm)
{
	emit_data_process_two_src(ctx, is64, rd, rd, rm, A64_LSRV);
}

static void
emit_asrv(struct a64_jit_ctx *ctx, bool is64, uint8_t rd, uint8_t rm)
{
	emit_data_process_two_src(ctx, is64, rd, rd, rm, A64_ASRV);
}

#define A64_UBFM 0x2
#define A64_SBFM 0x0
static void
emit_bitfield(struct a64_jit_ctx *ctx, bool is64, uint8_t rd, uint8_t rn,
	      uint8_t immr, uint8_t imms, uint16_t op)

{
	uint32_t insn;

	insn = (!!is64) << 31;
	if (insn)
		insn |= 1 << 22; /* Set N bit when is64 is set */
	insn |= op << 29;
	insn |= 0x26 << 23;
	insn |= immr << 16;
	insn |= imms << 10;
	insn |= rn << 5;
	insn |= rd;

	emit_insn(ctx, insn, check_reg(rd) || check_reg(rn) ||
		  check_immr_imms(is64, immr, imms));
}
static void
emit_lsl(struct a64_jit_ctx *ctx, bool is64, uint8_t rd, uint8_t imm)
{
	const unsigned int width = is64 ? 64 : 32;
	uint8_t imms, immr;

	immr = (width - imm) & (width - 1);
	imms = width - 1 - imm;

	emit_bitfield(ctx, is64, rd, rd, immr, imms, A64_UBFM);
}

static void
emit_lsr(struct a64_jit_ctx *ctx, bool is64, uint8_t rd, uint8_t imm)
{
	emit_bitfield(ctx, is64, rd, rd, imm, is64 ? 63 : 31, A64_UBFM);
}

static void
emit_asr(struct a64_jit_ctx *ctx, bool is64, uint8_t rd, uint8_t imm)
{
	emit_bitfield(ctx, is64, rd, rd, imm, is64 ? 63 : 31, A64_SBFM);
}

#define A64_AND 0
#define A64_OR 1
#define A64_XOR 2
static void
emit_logical(struct a64_jit_ctx *ctx, bool is64, uint8_t rd,
	     uint8_t rm, uint16_t op)
{
	uint32_t insn;

	insn = (!!is64) << 31;
	insn |= op << 29;
	insn |= 0x50 << 21;
	insn |= rm << 16;
	insn |= rd << 5;
	insn |= rd;

	emit_insn(ctx, insn, check_reg(rd) || check_reg(rm));
}

static void
emit_or(struct a64_jit_ctx *ctx, bool is64, uint8_t rd, uint8_t rm)
{
	emit_logical(ctx, is64, rd, rm, A64_OR);
}

static void
emit_and(struct a64_jit_ctx *ctx, bool is64, uint8_t rd, uint8_t rm)
{
	emit_logical(ctx, is64, rd, rm, A64_AND);
}

static void
emit_xor(struct a64_jit_ctx *ctx, bool is64, uint8_t rd, uint8_t rm)
{
	emit_logical(ctx, is64, rd, rm, A64_XOR);
}

static void
emit_msub(struct a64_jit_ctx *ctx, bool is64, uint8_t rd, uint8_t rn,
	  uint8_t rm, uint8_t ra)
{
	uint32_t insn;

	insn = (!!is64) << 31;
	insn |= 0xd8 << 21;
	insn |= rm << 16;
	insn |= 0x1 << 15;
	insn |= ra << 10;
	insn |= rn << 5;
	insn |= rd;

	emit_insn(ctx, insn, check_reg(rd) || check_reg(rn) || check_reg(rm) ||
		  check_reg(ra));
}

static void
emit_mod(struct a64_jit_ctx *ctx, bool is64, uint8_t tmp, uint8_t rd,
	 uint8_t rm)
{
	emit_data_process_two_src(ctx, is64, tmp, rd, rm, A64_UDIV);
	emit_msub(ctx, is64, rd, tmp, rm, rd);
}

static void
emit_blr(struct a64_jit_ctx *ctx, uint8_t rn)
{
	uint32_t insn;

	insn = 0xd63f0000;
	insn |= rn << 5;

	emit_insn(ctx, insn, check_reg(rn));
}

static void
emit_zero_extend(struct a64_jit_ctx *ctx, uint8_t rd, int32_t imm)
{
	switch (imm) {
	case 16:
		/* Zero-extend 16 bits into 64 bits */
		emit_bitfield(ctx, 1, rd, rd, 0, 15, A64_UBFM);
		break;
	case 32:
		/* Zero-extend 32 bits into 64 bits */
		emit_bitfield(ctx, 1, rd, rd, 0, 31, A64_UBFM);
		break;
	case 64:
		break;
	default:
		/* Generate error */
		emit_insn(ctx, 0, 1);
	}
}

static void
emit_rev(struct a64_jit_ctx *ctx, uint8_t rd, int32_t imm)
{
	uint32_t insn;

	insn = 0xdac00000;
	insn |= rd << 5;
	insn |= rd;

	switch (imm) {
	case 16:
		insn |= 1 << 10;
		emit_insn(ctx, insn, check_reg(rd));
		emit_zero_extend(ctx, rd, 16);
		break;
	case 32:
		insn |= 2 << 10;
		emit_insn(ctx, insn, check_reg(rd));
		/* Upper 32 bits already cleared */
		break;
	case 64:
		insn |= 3 << 10;
		emit_insn(ctx, insn, check_reg(rd));
		break;
	default:
		/* Generate error */
		emit_insn(ctx, insn, 1);
	}
}

static int
is_be(void)
{
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	return 1;
#else
	return 0;
#endif
}

static void
emit_be(struct a64_jit_ctx *ctx, uint8_t rd, int32_t imm)
{
	if (is_be())
		emit_zero_extend(ctx, rd, imm);
	else
		emit_rev(ctx, rd, imm);
}

static void
emit_le(struct a64_jit_ctx *ctx, uint8_t rd, int32_t imm)
{
	if (is_be())
		emit_rev(ctx, rd, imm);
	else
		emit_zero_extend(ctx, rd, imm);
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
emit_call(struct a64_jit_ctx *ctx, uint8_t tmp, void *func)
{
	uint8_t r0 = ebpf_to_a64_reg(ctx, EBPF_REG_0);

	emit_mov_imm(ctx, 1, tmp, (uint64_t)func);
	emit_blr(ctx, tmp);
	emit_mov_64(ctx, r0, A64_R(0));
}

static void
emit_cbnz(struct a64_jit_ctx *ctx, bool is64, uint8_t rt, int32_t imm19)
{
	uint32_t insn, imm;

	imm = mask_imm(19, imm19);
	insn = (!!is64) << 31;
	insn |= 0x35 << 24;
	insn |= imm << 5;
	insn |= rt;

	emit_insn(ctx, insn, check_reg(rt) || check_imm(19, imm19));
}

static void
emit_b(struct a64_jit_ctx *ctx, int32_t imm26)
{
	uint32_t insn, imm;

	imm = mask_imm(26, imm26);
	insn = 0x5 << 26;
	insn |= imm;

	emit_insn(ctx, insn, check_imm(26, imm26));
}

static void
emit_return_zero_if_src_zero(struct a64_jit_ctx *ctx, bool is64, uint8_t src)
{
	uint8_t r0 = ebpf_to_a64_reg(ctx, EBPF_REG_0);
	uint16_t jump_to_epilogue;

	emit_cbnz(ctx, is64, src, 3);
	emit_mov_imm(ctx, is64, r0, 0);
	jump_to_epilogue = (ctx->program_start + ctx->program_sz) - ctx->idx;
	emit_b(ctx, jump_to_epilogue);
}

static void
emit_stadd(struct a64_jit_ctx *ctx, bool is64, uint8_t rs, uint8_t rn)
{
	uint32_t insn;

	insn = 0xb820001f;
	insn |= (!!is64) << 30;
	insn |= rs << 16;
	insn |= rn << 5;

	emit_insn(ctx, insn, check_reg(rs) || check_reg(rn));
}

static void
emit_ldxr(struct a64_jit_ctx *ctx, bool is64, uint8_t rt, uint8_t rn)
{
	uint32_t insn;

	insn = 0x885f7c00;
	insn |= (!!is64) << 30;
	insn |= rn << 5;
	insn |= rt;

	emit_insn(ctx, insn, check_reg(rt) || check_reg(rn));
}

static void
emit_stxr(struct a64_jit_ctx *ctx, bool is64, uint8_t rs, uint8_t rt,
	  uint8_t rn)
{
	uint32_t insn;

	insn = 0x88007c00;
	insn |= (!!is64) << 30;
	insn |= rs << 16;
	insn |= rn << 5;
	insn |= rt;

	emit_insn(ctx, insn, check_reg(rs) || check_reg(rt) || check_reg(rn));
}

static int
has_atomics(void)
{
	int rc = 0;

#if defined(__ARM_FEATURE_ATOMICS) || defined(RTE_ARM_FEATURE_ATOMICS)
	rc = 1;
#endif
	return rc;
}

static void
emit_xadd(struct a64_jit_ctx *ctx, uint8_t op, uint8_t tmp1, uint8_t tmp2,
	  uint8_t tmp3, uint8_t dst, int16_t off, uint8_t src)
{
	bool is64 = (BPF_SIZE(op) == EBPF_DW);
	uint8_t rn;

	if (off) {
		emit_mov_imm(ctx, 1, tmp1, off);
		emit_add(ctx, 1, tmp1, dst);
		rn = tmp1;
	} else {
		rn = dst;
	}

	if (has_atomics()) {
		emit_stadd(ctx, is64, src, rn);
	} else {
		emit_ldxr(ctx, is64, tmp2, rn);
		emit_add(ctx, is64, tmp2, src);
		emit_stxr(ctx, is64, tmp3, tmp2, rn);
		emit_cbnz(ctx, is64, tmp3, -3);
	}
}

#define A64_CMP 0x6b00000f
#define A64_TST 0x6a00000f
static void
emit_cmp_tst(struct a64_jit_ctx *ctx, bool is64, uint8_t rn, uint8_t rm,
	     uint32_t opc)
{
	uint32_t insn;

	insn = opc;
	insn |= (!!is64) << 31;
	insn |= rm << 16;
	insn |= rn << 5;

	emit_insn(ctx, insn, check_reg(rn) || check_reg(rm));
}

static void
emit_cmp(struct a64_jit_ctx *ctx, bool is64, uint8_t rn, uint8_t rm)
{
	emit_cmp_tst(ctx, is64, rn, rm, A64_CMP);
}

static void
emit_tst(struct a64_jit_ctx *ctx, bool is64, uint8_t rn, uint8_t rm)
{
	emit_cmp_tst(ctx, is64, rn, rm, A64_TST);
}

static void
emit_b_cond(struct a64_jit_ctx *ctx, uint8_t cond, int32_t imm19)
{
	uint32_t insn, imm;

	imm = mask_imm(19, imm19);
	insn = 0x15 << 26;
	insn |= imm << 5;
	insn |= cond;

	emit_insn(ctx, insn, check_cond(cond) || check_imm(19, imm19));
}

static void
emit_branch(struct a64_jit_ctx *ctx, uint8_t op, uint32_t i, int16_t off)
{
	jump_offset_to_branch_update(ctx, i);
	emit_b_cond(ctx, ebpf_to_a64_cond(op), jump_offset_get(ctx, i, off));
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
	uint8_t op, dst, src, tmp1, tmp2, tmp3;
	const struct ebpf_insn *ins;
	uint64_t u64;
	int16_t off;
	int32_t imm;
	uint32_t i;
	bool is64;
	int rc;

	/* Reset context fields */
	ctx->idx = 0;
	/* arm64 SP must be aligned to 16 */
	ctx->stack_sz = RTE_ALIGN_MUL_CEIL(bpf->stack_sz, 16);
	tmp1 = ebpf_to_a64_reg(ctx, TMP_REG_1);
	tmp2 = ebpf_to_a64_reg(ctx, TMP_REG_2);
	tmp3 = ebpf_to_a64_reg(ctx, TMP_REG_3);

	emit_prologue(ctx);

	for (i = 0; i != bpf->prm.nb_ins; i++) {

		jump_offset_update(ctx, i);
		ins = bpf->prm.ins + i;
		op = ins->code;
		off = ins->off;
		imm = ins->imm;

		dst = ebpf_to_a64_reg(ctx, ins->dst_reg);
		src = ebpf_to_a64_reg(ctx, ins->src_reg);
		is64 = (BPF_CLASS(op) == EBPF_ALU64);

		switch (op) {
		/* dst = src */
		case (BPF_ALU | EBPF_MOV | BPF_X):
		case (EBPF_ALU64 | EBPF_MOV | BPF_X):
			emit_mov(ctx, is64, dst, src);
			break;
		/* dst = imm */
		case (BPF_ALU | EBPF_MOV | BPF_K):
		case (EBPF_ALU64 | EBPF_MOV | BPF_K):
			emit_mov_imm(ctx, is64, dst, imm);
			break;
		/* dst += src */
		case (BPF_ALU | BPF_ADD | BPF_X):
		case (EBPF_ALU64 | BPF_ADD | BPF_X):
			emit_add(ctx, is64, dst, src);
			break;
		/* dst += imm */
		case (BPF_ALU | BPF_ADD | BPF_K):
		case (EBPF_ALU64 | BPF_ADD | BPF_K):
			emit_mov_imm(ctx, is64, tmp1, imm);
			emit_add(ctx, is64, dst, tmp1);
			break;
		/* dst -= src */
		case (BPF_ALU | BPF_SUB | BPF_X):
		case (EBPF_ALU64 | BPF_SUB | BPF_X):
			emit_sub(ctx, is64, dst, src);
			break;
		/* dst -= imm */
		case (BPF_ALU | BPF_SUB | BPF_K):
		case (EBPF_ALU64 | BPF_SUB | BPF_K):
			emit_mov_imm(ctx, is64, tmp1, imm);
			emit_sub(ctx, is64, dst, tmp1);
			break;
		/* dst *= src */
		case (BPF_ALU | BPF_MUL | BPF_X):
		case (EBPF_ALU64 | BPF_MUL | BPF_X):
			emit_mul(ctx, is64, dst, src);
			break;
		/* dst *= imm */
		case (BPF_ALU | BPF_MUL | BPF_K):
		case (EBPF_ALU64 | BPF_MUL | BPF_K):
			emit_mov_imm(ctx, is64, tmp1, imm);
			emit_mul(ctx, is64, dst, tmp1);
			break;
		/* dst /= src */
		case (BPF_ALU | BPF_DIV | BPF_X):
		case (EBPF_ALU64 | BPF_DIV | BPF_X):
			emit_return_zero_if_src_zero(ctx, is64, src);
			emit_div(ctx, is64, dst, src);
			break;
		/* dst /= imm */
		case (BPF_ALU | BPF_DIV | BPF_K):
		case (EBPF_ALU64 | BPF_DIV | BPF_K):
			emit_mov_imm(ctx, is64, tmp1, imm);
			emit_div(ctx, is64, dst, tmp1);
			break;
		/* dst %= src */
		case (BPF_ALU | BPF_MOD | BPF_X):
		case (EBPF_ALU64 | BPF_MOD | BPF_X):
			emit_return_zero_if_src_zero(ctx, is64, src);
			emit_mod(ctx, is64, tmp1, dst, src);
			break;
		/* dst %= imm */
		case (BPF_ALU | BPF_MOD | BPF_K):
		case (EBPF_ALU64 | BPF_MOD | BPF_K):
			emit_mov_imm(ctx, is64, tmp1, imm);
			emit_mod(ctx, is64, tmp2, dst, tmp1);
			break;
		/* dst |= src */
		case (BPF_ALU | BPF_OR | BPF_X):
		case (EBPF_ALU64 | BPF_OR | BPF_X):
			emit_or(ctx, is64, dst, src);
			break;
		/* dst |= imm */
		case (BPF_ALU | BPF_OR | BPF_K):
		case (EBPF_ALU64 | BPF_OR | BPF_K):
			emit_mov_imm(ctx, is64, tmp1, imm);
			emit_or(ctx, is64, dst, tmp1);
			break;
		/* dst &= src */
		case (BPF_ALU | BPF_AND | BPF_X):
		case (EBPF_ALU64 | BPF_AND | BPF_X):
			emit_and(ctx, is64, dst, src);
			break;
		/* dst &= imm */
		case (BPF_ALU | BPF_AND | BPF_K):
		case (EBPF_ALU64 | BPF_AND | BPF_K):
			emit_mov_imm(ctx, is64, tmp1, imm);
			emit_and(ctx, is64, dst, tmp1);
			break;
		/* dst ^= src */
		case (BPF_ALU | BPF_XOR | BPF_X):
		case (EBPF_ALU64 | BPF_XOR | BPF_X):
			emit_xor(ctx, is64, dst, src);
			break;
		/* dst ^= imm */
		case (BPF_ALU | BPF_XOR | BPF_K):
		case (EBPF_ALU64 | BPF_XOR | BPF_K):
			emit_mov_imm(ctx, is64, tmp1, imm);
			emit_xor(ctx, is64, dst, tmp1);
			break;
		/* dst = -dst */
		case (BPF_ALU | BPF_NEG):
		case (EBPF_ALU64 | BPF_NEG):
			emit_neg(ctx, is64, dst);
			break;
		/* dst <<= src */
		case BPF_ALU | BPF_LSH | BPF_X:
		case EBPF_ALU64 | BPF_LSH | BPF_X:
			emit_lslv(ctx, is64, dst, src);
			break;
		/* dst <<= imm */
		case BPF_ALU | BPF_LSH | BPF_K:
		case EBPF_ALU64 | BPF_LSH | BPF_K:
			emit_lsl(ctx, is64, dst, imm);
			break;
		/* dst >>= src */
		case BPF_ALU | BPF_RSH | BPF_X:
		case EBPF_ALU64 | BPF_RSH | BPF_X:
			emit_lsrv(ctx, is64, dst, src);
			break;
		/* dst >>= imm */
		case BPF_ALU | BPF_RSH | BPF_K:
		case EBPF_ALU64 | BPF_RSH | BPF_K:
			emit_lsr(ctx, is64, dst, imm);
			break;
		/* dst >>= src (arithmetic) */
		case BPF_ALU | EBPF_ARSH | BPF_X:
		case EBPF_ALU64 | EBPF_ARSH | BPF_X:
			emit_asrv(ctx, is64, dst, src);
			break;
		/* dst >>= imm (arithmetic) */
		case BPF_ALU | EBPF_ARSH | BPF_K:
		case EBPF_ALU64 | EBPF_ARSH | BPF_K:
			emit_asr(ctx, is64, dst, imm);
			break;
		/* dst = be##imm(dst) */
		case (BPF_ALU | EBPF_END | EBPF_TO_BE):
			emit_be(ctx, dst, imm);
			break;
		/* dst = le##imm(dst) */
		case (BPF_ALU | EBPF_END | EBPF_TO_LE):
			emit_le(ctx, dst, imm);
			break;
		/* dst = *(size *) (src + off) */
		case (BPF_LDX | BPF_MEM | BPF_B):
		case (BPF_LDX | BPF_MEM | BPF_H):
		case (BPF_LDX | BPF_MEM | BPF_W):
		case (BPF_LDX | BPF_MEM | EBPF_DW):
			emit_mov_imm(ctx, 1, tmp1, off);
			emit_ldr(ctx, BPF_SIZE(op), dst, src, tmp1);
			break;
		/* dst = imm64 */
		case (BPF_LD | BPF_IMM | EBPF_DW):
			u64 = ((uint64_t)ins[1].imm << 32) | (uint32_t)imm;
			emit_mov_imm(ctx, 1, dst, u64);
			i++;
			break;
		/* *(size *)(dst + off) = src */
		case (BPF_STX | BPF_MEM | BPF_B):
		case (BPF_STX | BPF_MEM | BPF_H):
		case (BPF_STX | BPF_MEM | BPF_W):
		case (BPF_STX | BPF_MEM | EBPF_DW):
			emit_mov_imm(ctx, 1, tmp1, off);
			emit_str(ctx, BPF_SIZE(op), src, dst, tmp1);
			break;
		/* *(size *)(dst + off) = imm */
		case (BPF_ST | BPF_MEM | BPF_B):
		case (BPF_ST | BPF_MEM | BPF_H):
		case (BPF_ST | BPF_MEM | BPF_W):
		case (BPF_ST | BPF_MEM | EBPF_DW):
			emit_mov_imm(ctx, 1, tmp1, imm);
			emit_mov_imm(ctx, 1, tmp2, off);
			emit_str(ctx, BPF_SIZE(op), tmp1, dst, tmp2);
			break;
		/* STX XADD: lock *(size *)(dst + off) += src */
		case (BPF_STX | EBPF_XADD | BPF_W):
		case (BPF_STX | EBPF_XADD | EBPF_DW):
			emit_xadd(ctx, op, tmp1, tmp2, tmp3, dst, off, src);
			break;
		/* PC += off */
		case (BPF_JMP | BPF_JA):
			emit_b(ctx, jump_offset_get(ctx, i, off));
			break;
		/* PC += off if dst COND imm */
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
			emit_mov_imm(ctx, 1, tmp1, imm);
			emit_cmp(ctx, 1, dst, tmp1);
			emit_branch(ctx, op, i, off);
			break;
		case (BPF_JMP | BPF_JSET | BPF_K):
			emit_mov_imm(ctx, 1, tmp1, imm);
			emit_tst(ctx, 1, dst, tmp1);
			emit_branch(ctx, op, i, off);
			break;
		/* PC += off if dst COND src */
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
			emit_cmp(ctx, 1, dst, src);
			emit_branch(ctx, op, i, off);
			break;
		case (BPF_JMP | BPF_JSET | BPF_X):
			emit_tst(ctx, 1, dst, src);
			emit_branch(ctx, op, i, off);
			break;
		/* Call imm */
		case (BPF_JMP | EBPF_CALL):
			emit_call(ctx, tmp1, bpf->prm.xsym[ins->imm].func.val);
			break;
		/* Return r0 */
		case (BPF_JMP | EBPF_EXIT):
			emit_epilogue(ctx);
			break;
		default:
			RTE_BPF_LOG_LINE(ERR,
				"%s(%p): invalid opcode %#x at pc: %u;",
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
__rte_bpf_jit_arm64(struct rte_bpf *bpf)
{
	struct a64_jit_ctx ctx;
	size_t size;
	int rc;

	/* Init JIT context */
	memset(&ctx, 0, sizeof(ctx));

	/* Initialize the memory for eBPF to a64 insn offset map for jump */
	rc = jump_offset_init(&ctx, bpf);
	if (rc)
		goto error;

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
	__builtin___clear_cache((char *)ctx.ins, (char *)(ctx.ins + ctx.idx));

	bpf->jit.func = (void *)ctx.ins;
	bpf->jit.sz = size;

	goto finish;

munmap:
	munmap(ctx.ins, size);
finish:
	jump_offset_fini(&ctx);
error:
	return rc;
}
