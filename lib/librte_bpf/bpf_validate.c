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
#include <rte_byteorder.h>

#include "bpf_impl.h"

struct bpf_reg_val {
	struct rte_bpf_arg v;
	uint64_t mask;
	struct {
		int64_t min;
		int64_t max;
	} s;
	struct {
		uint64_t min;
		uint64_t max;
	} u;
};

struct bpf_eval_state {
	struct bpf_reg_val rv[EBPF_REG_NUM];
	struct bpf_reg_val sv[MAX_BPF_STACK_SIZE / sizeof(uint64_t)];
};

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
	uint64_t stack_sz;
	uint32_t nb_nodes;
	uint32_t nb_jcc_nodes;
	uint32_t node_colour[MAX_NODE_COLOUR];
	uint32_t edge_type[MAX_EDGE_TYPE];
	struct bpf_eval_state *evst;
	struct inst_node *evin;
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
eval_exit(struct bpf_verifier *bvf, const struct ebpf_insn *ins)
{
	RTE_SET_USED(ins);
	if (bvf->evst->rv[EBPF_REG_0].v.type == RTE_BPF_ARG_UNDEF)
		return "undefined return value";
	return NULL;
}

/* setup max possible with this mask bounds */
static void
eval_umax_bound(struct bpf_reg_val *rv, uint64_t mask)
{
	rv->u.max = mask;
	rv->u.min = 0;
}

static void
eval_smax_bound(struct bpf_reg_val *rv, uint64_t mask)
{
	rv->s.max = mask >> 1;
	rv->s.min = rv->s.max ^ UINT64_MAX;
}

static void
eval_max_bound(struct bpf_reg_val *rv, uint64_t mask)
{
	eval_umax_bound(rv, mask);
	eval_smax_bound(rv, mask);
}

static void
eval_fill_max_bound(struct bpf_reg_val *rv, uint64_t mask)
{
	eval_max_bound(rv, mask);
	rv->v.type = RTE_BPF_ARG_RAW;
	rv->mask = mask;
}

static void
eval_fill_imm64(struct bpf_reg_val *rv, uint64_t mask, uint64_t val)
{
	rv->mask = mask;
	rv->s.min = val;
	rv->s.max = val;
	rv->u.min = val;
	rv->u.max = val;
}

static void
eval_fill_imm(struct bpf_reg_val *rv, uint64_t mask, int32_t imm)
{
	uint64_t v;

	v = (uint64_t)imm & mask;

	rv->v.type = RTE_BPF_ARG_RAW;
	eval_fill_imm64(rv, mask, v);
}

static const char *
eval_ld_imm64(struct bpf_verifier *bvf, const struct ebpf_insn *ins)
{
	uint32_t i;
	uint64_t val;
	struct bpf_reg_val *rd;

	val = (uint32_t)ins[0].imm | (uint64_t)(uint32_t)ins[1].imm << 32;

	rd = bvf->evst->rv + ins->dst_reg;
	rd->v.type = RTE_BPF_ARG_RAW;
	eval_fill_imm64(rd, UINT64_MAX, val);

	for (i = 0; i != bvf->prm->nb_xsym; i++) {

		/* load of external variable */
		if (bvf->prm->xsym[i].type == RTE_BPF_XTYPE_VAR &&
				(uintptr_t)bvf->prm->xsym[i].var.val == val) {
			rd->v = bvf->prm->xsym[i].var.desc;
			eval_fill_imm64(rd, UINT64_MAX, 0);
			break;
		}
	}

	return NULL;
}

static void
eval_apply_mask(struct bpf_reg_val *rv, uint64_t mask)
{
	struct bpf_reg_val rt;

	rt.u.min = rv->u.min & mask;
	rt.u.max = rv->u.max & mask;
	if (rt.u.min != rv->u.min || rt.u.max != rv->u.max) {
		rv->u.max = RTE_MAX(rt.u.max, mask);
		rv->u.min = 0;
	}

	eval_smax_bound(&rt, mask);
	rv->s.max = RTE_MIN(rt.s.max, rv->s.max);
	rv->s.min = RTE_MAX(rt.s.min, rv->s.min);

	rv->mask = mask;
}

static void
eval_add(struct bpf_reg_val *rd, const struct bpf_reg_val *rs, uint64_t msk)
{
	struct bpf_reg_val rv;

	rv.u.min = (rd->u.min + rs->u.min) & msk;
	rv.u.max = (rd->u.min + rs->u.max) & msk;
	rv.s.min = (rd->s.min + rs->s.min) & msk;
	rv.s.max = (rd->s.max + rs->s.max) & msk;

	/*
	 * if at least one of the operands is not constant,
	 * then check for overflow
	 */
	if ((rd->u.min != rd->u.max || rs->u.min != rs->u.max) &&
			(rv.u.min < rd->u.min || rv.u.max < rd->u.max))
		eval_umax_bound(&rv, msk);

	if ((rd->s.min != rd->s.max || rs->s.min != rs->s.max) &&
			(((rs->s.min < 0 && rv.s.min > rd->s.min) ||
			rv.s.min < rd->s.min) ||
			((rs->s.max < 0 && rv.s.max > rd->s.max) ||
				rv.s.max < rd->s.max)))
		eval_smax_bound(&rv, msk);

	rd->s = rv.s;
	rd->u = rv.u;
}

static void
eval_sub(struct bpf_reg_val *rd, const struct bpf_reg_val *rs, uint64_t msk)
{
	struct bpf_reg_val rv;

	rv.u.min = (rd->u.min - rs->u.min) & msk;
	rv.u.max = (rd->u.min - rs->u.max) & msk;
	rv.s.min = (rd->s.min - rs->s.min) & msk;
	rv.s.max = (rd->s.max - rs->s.max) & msk;

	/*
	 * if at least one of the operands is not constant,
	 * then check for overflow
	 */
	if ((rd->u.min != rd->u.max || rs->u.min != rs->u.max) &&
			(rv.u.min > rd->u.min || rv.u.max > rd->u.max))
		eval_umax_bound(&rv, msk);

	if ((rd->s.min != rd->s.max || rs->s.min != rs->s.max) &&
			(((rs->s.min < 0 && rv.s.min < rd->s.min) ||
			rv.s.min > rd->s.min) ||
			((rs->s.max < 0 && rv.s.max < rd->s.max) ||
			rv.s.max > rd->s.max)))
		eval_smax_bound(&rv, msk);

	rd->s = rv.s;
	rd->u = rv.u;
}

static void
eval_lsh(struct bpf_reg_val *rd, const struct bpf_reg_val *rs, size_t opsz,
	uint64_t msk)
{
	/* check if shift value is less then max result bits */
	if (rs->u.max >= opsz) {
		eval_max_bound(rd, msk);
		return;
	}

	/* check for overflow */
	if (rd->u.max > RTE_LEN2MASK(opsz - rs->u.max, uint64_t))
		eval_umax_bound(rd, msk);
	else {
		rd->u.max <<= rs->u.max;
		rd->u.min <<= rs->u.min;
	}

	/* check that dreg values are and would remain always positive */
	if ((uint64_t)rd->s.min >> (opsz - 1) != 0 || rd->s.max >=
			RTE_LEN2MASK(opsz - rs->u.max - 1, int64_t))
		eval_smax_bound(rd, msk);
	else {
		rd->s.max <<= rs->u.max;
		rd->s.min <<= rs->u.min;
	}
}

static void
eval_rsh(struct bpf_reg_val *rd, const struct bpf_reg_val *rs, size_t opsz,
	uint64_t msk)
{
	/* check if shift value is less then max result bits */
	if (rs->u.max >= opsz) {
		eval_max_bound(rd, msk);
		return;
	}

	rd->u.max >>= rs->u.min;
	rd->u.min >>= rs->u.max;

	/* check that dreg values are always positive */
	if ((uint64_t)rd->s.min >> (opsz - 1) != 0)
		eval_smax_bound(rd, msk);
	else {
		rd->s.max >>= rs->u.min;
		rd->s.min >>= rs->u.max;
	}
}

static void
eval_arsh(struct bpf_reg_val *rd, const struct bpf_reg_val *rs, size_t opsz,
	uint64_t msk)
{
	uint32_t shv;

	/* check if shift value is less then max result bits */
	if (rs->u.max >= opsz) {
		eval_max_bound(rd, msk);
		return;
	}

	rd->u.max = (int64_t)rd->u.max >> rs->u.min;
	rd->u.min = (int64_t)rd->u.min >> rs->u.max;

	/* if we have 32-bit values - extend them to 64-bit */
	if (opsz == sizeof(uint32_t) * CHAR_BIT) {
		rd->s.min <<= opsz;
		rd->s.max <<= opsz;
		shv = opsz;
	} else
		shv = 0;

	if (rd->s.min < 0)
		rd->s.min = (rd->s.min >> (rs->u.min + shv)) & msk;
	else
		rd->s.min = (rd->s.min >> (rs->u.max + shv)) & msk;

	if (rd->s.max < 0)
		rd->s.max = (rd->s.max >> (rs->u.max + shv)) & msk;
	else
		rd->s.max = (rd->s.max >> (rs->u.min + shv)) & msk;
}

static uint64_t
eval_umax_bits(uint64_t v, size_t opsz)
{
	if (v == 0)
		return 0;

	v = __builtin_clzll(v);
	return RTE_LEN2MASK(opsz - v, uint64_t);
}

/* estimate max possible value for (v1 & v2) */
static uint64_t
eval_uand_max(uint64_t v1, uint64_t v2, size_t opsz)
{
	v1 = eval_umax_bits(v1, opsz);
	v2 = eval_umax_bits(v2, opsz);
	return (v1 & v2);
}

/* estimate max possible value for (v1 | v2) */
static uint64_t
eval_uor_max(uint64_t v1, uint64_t v2, size_t opsz)
{
	v1 = eval_umax_bits(v1, opsz);
	v2 = eval_umax_bits(v2, opsz);
	return (v1 | v2);
}

static void
eval_and(struct bpf_reg_val *rd, const struct bpf_reg_val *rs, size_t opsz,
	uint64_t msk)
{
	/* both operands are constants */
	if (rd->u.min == rd->u.max && rs->u.min == rs->u.max) {
		rd->u.min &= rs->u.min;
		rd->u.max &= rs->u.max;
	} else {
		rd->u.max = eval_uand_max(rd->u.max, rs->u.max, opsz);
		rd->u.min &= rs->u.min;
	}

	/* both operands are constants */
	if (rd->s.min == rd->s.max && rs->s.min == rs->s.max) {
		rd->s.min &= rs->s.min;
		rd->s.max &= rs->s.max;
	/* at least one of operand is non-negative */
	} else if (rd->s.min >= 0 || rs->s.min >= 0) {
		rd->s.max = eval_uand_max(rd->s.max & (msk >> 1),
			rs->s.max & (msk >> 1), opsz);
		rd->s.min &= rs->s.min;
	} else
		eval_smax_bound(rd, msk);
}

static void
eval_or(struct bpf_reg_val *rd, const struct bpf_reg_val *rs, size_t opsz,
	uint64_t msk)
{
	/* both operands are constants */
	if (rd->u.min == rd->u.max && rs->u.min == rs->u.max) {
		rd->u.min |= rs->u.min;
		rd->u.max |= rs->u.max;
	} else {
		rd->u.max = eval_uor_max(rd->u.max, rs->u.max, opsz);
		rd->u.min |= rs->u.min;
	}

	/* both operands are constants */
	if (rd->s.min == rd->s.max && rs->s.min == rs->s.max) {
		rd->s.min |= rs->s.min;
		rd->s.max |= rs->s.max;

	/* both operands are non-negative */
	} else if (rd->s.min >= 0 || rs->s.min >= 0) {
		rd->s.max = eval_uor_max(rd->s.max, rs->s.max, opsz);
		rd->s.min |= rs->s.min;
	} else
		eval_smax_bound(rd, msk);
}

static void
eval_xor(struct bpf_reg_val *rd, const struct bpf_reg_val *rs, size_t opsz,
	uint64_t msk)
{
	/* both operands are constants */
	if (rd->u.min == rd->u.max && rs->u.min == rs->u.max) {
		rd->u.min ^= rs->u.min;
		rd->u.max ^= rs->u.max;
	} else {
		rd->u.max = eval_uor_max(rd->u.max, rs->u.max, opsz);
		rd->u.min = 0;
	}

	/* both operands are constants */
	if (rd->s.min == rd->s.max && rs->s.min == rs->s.max) {
		rd->s.min ^= rs->s.min;
		rd->s.max ^= rs->s.max;

	/* both operands are non-negative */
	} else if (rd->s.min >= 0 || rs->s.min >= 0) {
		rd->s.max = eval_uor_max(rd->s.max, rs->s.max, opsz);
		rd->s.min = 0;
	} else
		eval_smax_bound(rd, msk);
}

static void
eval_mul(struct bpf_reg_val *rd, const struct bpf_reg_val *rs, size_t opsz,
	uint64_t msk)
{
	/* both operands are constants */
	if (rd->u.min == rd->u.max && rs->u.min == rs->u.max) {
		rd->u.min = (rd->u.min * rs->u.min) & msk;
		rd->u.max = (rd->u.max * rs->u.max) & msk;
	/* check for overflow */
	} else if (rd->u.max <= msk >> opsz / 2 && rs->u.max <= msk >> opsz) {
		rd->u.max *= rs->u.max;
		rd->u.min *= rd->u.min;
	} else
		eval_umax_bound(rd, msk);

	/* both operands are constants */
	if (rd->s.min == rd->s.max && rs->s.min == rs->s.max) {
		rd->s.min = (rd->s.min * rs->s.min) & msk;
		rd->s.max = (rd->s.max * rs->s.max) & msk;
	/* check that both operands are positive and no overflow */
	} else if (rd->s.min >= 0 && rs->s.min >= 0) {
		rd->s.max *= rs->s.max;
		rd->s.min *= rd->s.min;
	} else
		eval_smax_bound(rd, msk);
}

static const char *
eval_divmod(uint32_t op, struct bpf_reg_val *rd, struct bpf_reg_val *rs,
	size_t opsz, uint64_t msk)
{
	/* both operands are constants */
	if (rd->u.min == rd->u.max && rs->u.min == rs->u.max) {
		if (rs->u.max == 0)
			return "division by 0";
		if (op == BPF_DIV) {
			rd->u.min /= rs->u.min;
			rd->u.max /= rs->u.max;
		} else {
			rd->u.min %= rs->u.min;
			rd->u.max %= rs->u.max;
		}
	} else {
		if (op == BPF_MOD)
			rd->u.max = RTE_MIN(rd->u.max, rs->u.max - 1);
		else
			rd->u.max = rd->u.max;
		rd->u.min = 0;
	}

	/* if we have 32-bit values - extend them to 64-bit */
	if (opsz == sizeof(uint32_t) * CHAR_BIT) {
		rd->s.min = (int32_t)rd->s.min;
		rd->s.max = (int32_t)rd->s.max;
		rs->s.min = (int32_t)rs->s.min;
		rs->s.max = (int32_t)rs->s.max;
	}

	/* both operands are constants */
	if (rd->s.min == rd->s.max && rs->s.min == rs->s.max) {
		if (rs->s.max == 0)
			return "division by 0";
		if (op == BPF_DIV) {
			rd->s.min /= rs->s.min;
			rd->s.max /= rs->s.max;
		} else {
			rd->s.min %= rs->s.min;
			rd->s.max %= rs->s.max;
		}
	} else if (op == BPF_MOD) {
		rd->s.min = RTE_MAX(rd->s.max, 0);
		rd->s.min = RTE_MIN(rd->s.min, 0);
	} else
		eval_smax_bound(rd, msk);

	rd->s.max &= msk;
	rd->s.min &= msk;

	return NULL;
}

static void
eval_neg(struct bpf_reg_val *rd, size_t opsz, uint64_t msk)
{
	uint64_t ux, uy;
	int64_t sx, sy;

	/* if we have 32-bit values - extend them to 64-bit */
	if (opsz == sizeof(uint32_t) * CHAR_BIT) {
		rd->u.min = (int32_t)rd->u.min;
		rd->u.max = (int32_t)rd->u.max;
	}

	ux = -(int64_t)rd->u.min & msk;
	uy = -(int64_t)rd->u.max & msk;

	rd->u.max = RTE_MAX(ux, uy);
	rd->u.min = RTE_MIN(ux, uy);

	/* if we have 32-bit values - extend them to 64-bit */
	if (opsz == sizeof(uint32_t) * CHAR_BIT) {
		rd->s.min = (int32_t)rd->s.min;
		rd->s.max = (int32_t)rd->s.max;
	}

	sx = -rd->s.min & msk;
	sy = -rd->s.max & msk;

	rd->s.max = RTE_MAX(sx, sy);
	rd->s.min = RTE_MIN(sx, sy);
}

/*
 * check that destination and source operand are in defined state.
 */
static const char *
eval_defined(const struct bpf_reg_val *dst, const struct bpf_reg_val *src)
{
	if (dst != NULL && dst->v.type == RTE_BPF_ARG_UNDEF)
		return "dest reg value is undefined";
	if (src != NULL && src->v.type == RTE_BPF_ARG_UNDEF)
		return "src reg value is undefined";
	return NULL;
}

static const char *
eval_alu(struct bpf_verifier *bvf, const struct ebpf_insn *ins)
{
	uint64_t msk;
	uint32_t op;
	size_t opsz;
	const char *err;
	struct bpf_eval_state *st;
	struct bpf_reg_val *rd, rs;

	opsz = (BPF_CLASS(ins->code) == BPF_ALU) ?
		sizeof(uint32_t) : sizeof(uint64_t);
	opsz = opsz * CHAR_BIT;
	msk = RTE_LEN2MASK(opsz, uint64_t);

	st = bvf->evst;
	rd = st->rv + ins->dst_reg;

	if (BPF_SRC(ins->code) == BPF_X) {
		rs = st->rv[ins->src_reg];
		eval_apply_mask(&rs, msk);
	} else
		eval_fill_imm(&rs, msk, ins->imm);

	eval_apply_mask(rd, msk);

	op = BPF_OP(ins->code);

	err = eval_defined((op != EBPF_MOV) ? rd : NULL,
			(op != BPF_NEG) ? &rs : NULL);
	if (err != NULL)
		return err;

	if (op == BPF_ADD)
		eval_add(rd, &rs, msk);
	else if (op == BPF_SUB)
		eval_sub(rd, &rs, msk);
	else if (op == BPF_LSH)
		eval_lsh(rd, &rs, opsz, msk);
	else if (op == BPF_RSH)
		eval_rsh(rd, &rs, opsz, msk);
	else if (op == EBPF_ARSH)
		eval_arsh(rd, &rs, opsz, msk);
	else if (op == BPF_AND)
		eval_and(rd, &rs, opsz, msk);
	else if (op == BPF_OR)
		eval_or(rd, &rs, opsz, msk);
	else if (op == BPF_XOR)
		eval_xor(rd, &rs, opsz, msk);
	else if (op == BPF_MUL)
		eval_mul(rd, &rs, opsz, msk);
	else if (op == BPF_DIV || op == BPF_MOD)
		err = eval_divmod(op, rd, &rs, opsz, msk);
	else if (op == BPF_NEG)
		eval_neg(rd, opsz, msk);
	else if (op == EBPF_MOV)
		*rd = rs;
	else
		eval_max_bound(rd, msk);

	return err;
}

static const char *
eval_bele(struct bpf_verifier *bvf, const struct ebpf_insn *ins)
{
	uint64_t msk;
	struct bpf_eval_state *st;
	struct bpf_reg_val *rd;
	const char *err;

	msk = RTE_LEN2MASK(ins->imm, uint64_t);

	st = bvf->evst;
	rd = st->rv + ins->dst_reg;

	err = eval_defined(rd, NULL);
	if (err != NULL)
		return err;

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	if (ins->code == (BPF_ALU | EBPF_END | EBPF_TO_BE))
		eval_max_bound(rd, msk);
	else
		eval_apply_mask(rd, msk);
#else
	if (ins->code == (BPF_ALU | EBPF_END | EBPF_TO_LE))
		eval_max_bound(rd, msk);
	else
		eval_apply_mask(rd, msk);
#endif

	return NULL;
}

static const char *
eval_ptr(struct bpf_verifier *bvf, struct bpf_reg_val *rm, uint32_t opsz,
	uint32_t align, int16_t off)
{
	struct bpf_reg_val rv;

	/* calculate reg + offset */
	eval_fill_imm(&rv, rm->mask, off);
	eval_add(rm, &rv, rm->mask);

	if (RTE_BPF_ARG_PTR_TYPE(rm->v.type) == 0)
		return "destination is not a pointer";

	if (rm->mask != UINT64_MAX)
		return "pointer truncation";

	if (rm->u.max + opsz > rm->v.size ||
			(uint64_t)rm->s.max + opsz > rm->v.size ||
			rm->s.min < 0)
		return "memory boundary violation";

	if (rm->u.max % align !=  0)
		return "unaligned memory access";

	if (rm->v.type == RTE_BPF_ARG_PTR_STACK) {

		if (rm->u.max != rm->u.min || rm->s.max != rm->s.min ||
				rm->u.max != (uint64_t)rm->s.max)
			return "stack access with variable offset";

		bvf->stack_sz = RTE_MAX(bvf->stack_sz, rm->v.size - rm->u.max);

	/* pointer to mbuf */
	} else if (rm->v.type == RTE_BPF_ARG_PTR_MBUF) {

		if (rm->u.max != rm->u.min || rm->s.max != rm->s.min ||
				rm->u.max != (uint64_t)rm->s.max)
			return "mbuf access with variable offset";
	}

	return NULL;
}

static void
eval_max_load(struct bpf_reg_val *rv, uint64_t mask)
{
	eval_umax_bound(rv, mask);

	/* full 64-bit load */
	if (mask == UINT64_MAX)
		eval_smax_bound(rv, mask);

	/* zero-extend load */
	rv->s.min = rv->u.min;
	rv->s.max = rv->u.max;
}


static const char *
eval_load(struct bpf_verifier *bvf, const struct ebpf_insn *ins)
{
	uint32_t opsz;
	uint64_t msk;
	const char *err;
	struct bpf_eval_state *st;
	struct bpf_reg_val *rd, rs;
	const struct bpf_reg_val *sv;

	st = bvf->evst;
	rd = st->rv + ins->dst_reg;
	rs = st->rv[ins->src_reg];
	opsz = bpf_size(BPF_SIZE(ins->code));
	msk = RTE_LEN2MASK(opsz * CHAR_BIT, uint64_t);

	err = eval_ptr(bvf, &rs, opsz, 1, ins->off);
	if (err != NULL)
		return err;

	if (rs.v.type == RTE_BPF_ARG_PTR_STACK) {

		sv = st->sv + rs.u.max / sizeof(uint64_t);
		if (sv->v.type == RTE_BPF_ARG_UNDEF || sv->mask < msk)
			return "undefined value on the stack";

		*rd = *sv;

	/* pointer to mbuf */
	} else if (rs.v.type == RTE_BPF_ARG_PTR_MBUF) {

		if (rs.u.max == offsetof(struct rte_mbuf, next)) {
			eval_fill_imm(rd, msk, 0);
			rd->v = rs.v;
		} else if (rs.u.max == offsetof(struct rte_mbuf, buf_addr)) {
			eval_fill_imm(rd, msk, 0);
			rd->v.type = RTE_BPF_ARG_PTR;
			rd->v.size = rs.v.buf_size;
		} else if (rs.u.max == offsetof(struct rte_mbuf, data_off)) {
			eval_fill_imm(rd, msk, RTE_PKTMBUF_HEADROOM);
			rd->v.type = RTE_BPF_ARG_RAW;
		} else {
			eval_max_load(rd, msk);
			rd->v.type = RTE_BPF_ARG_RAW;
		}

	/* pointer to raw data */
	} else {
		eval_max_load(rd, msk);
		rd->v.type = RTE_BPF_ARG_RAW;
	}

	return NULL;
}

static const char *
eval_mbuf_store(const struct bpf_reg_val *rv, uint32_t opsz)
{
	uint32_t i;

	static const struct {
		size_t off;
		size_t sz;
	} mbuf_ro_fileds[] = {
		{ .off = offsetof(struct rte_mbuf, buf_addr), },
		{ .off = offsetof(struct rte_mbuf, refcnt), },
		{ .off = offsetof(struct rte_mbuf, nb_segs), },
		{ .off = offsetof(struct rte_mbuf, buf_len), },
		{ .off = offsetof(struct rte_mbuf, pool), },
		{ .off = offsetof(struct rte_mbuf, next), },
		{ .off = offsetof(struct rte_mbuf, priv_size), },
	};

	for (i = 0; i != RTE_DIM(mbuf_ro_fileds) &&
			(mbuf_ro_fileds[i].off + mbuf_ro_fileds[i].sz <=
			rv->u.max || rv->u.max + opsz <= mbuf_ro_fileds[i].off);
			i++)
		;

	if (i != RTE_DIM(mbuf_ro_fileds))
		return "store to the read-only mbuf field";

	return NULL;

}

static const char *
eval_store(struct bpf_verifier *bvf, const struct ebpf_insn *ins)
{
	uint32_t opsz;
	uint64_t msk;
	const char *err;
	struct bpf_eval_state *st;
	struct bpf_reg_val rd, rs, *sv;

	opsz = bpf_size(BPF_SIZE(ins->code));
	msk = RTE_LEN2MASK(opsz * CHAR_BIT, uint64_t);

	st = bvf->evst;
	rd = st->rv[ins->dst_reg];

	if (BPF_CLASS(ins->code) == BPF_STX) {
		rs = st->rv[ins->src_reg];
		eval_apply_mask(&rs, msk);
	} else
		eval_fill_imm(&rs, msk, ins->imm);

	err = eval_defined(NULL, &rs);
	if (err != NULL)
		return err;

	err = eval_ptr(bvf, &rd, opsz, 1, ins->off);
	if (err != NULL)
		return err;

	if (rd.v.type == RTE_BPF_ARG_PTR_STACK) {

		sv = st->sv + rd.u.max / sizeof(uint64_t);
		if (BPF_CLASS(ins->code) == BPF_STX &&
				BPF_MODE(ins->code) == EBPF_XADD)
			eval_max_bound(sv, msk);
		else
			*sv = rs;

	/* pointer to mbuf */
	} else if (rd.v.type == RTE_BPF_ARG_PTR_MBUF) {
		err = eval_mbuf_store(&rd, opsz);
		if (err != NULL)
			return err;
	}

	return NULL;
}

static const char *
eval_func_arg(struct bpf_verifier *bvf, const struct rte_bpf_arg *arg,
	struct bpf_reg_val *rv)
{
	uint32_t i, n;
	struct bpf_eval_state *st;
	const char *err;

	st = bvf->evst;

	if (rv->v.type == RTE_BPF_ARG_UNDEF)
		return "Undefined argument type";

	if (arg->type != rv->v.type &&
			arg->type != RTE_BPF_ARG_RAW &&
			(arg->type != RTE_BPF_ARG_PTR ||
			RTE_BPF_ARG_PTR_TYPE(rv->v.type) == 0))
		return "Invalid argument type";

	err = NULL;

	/* argument is a pointer */
	if (RTE_BPF_ARG_PTR_TYPE(arg->type) != 0) {

		err = eval_ptr(bvf, rv, arg->size, 1, 0);

		/*
		 * pointer to the variable on the stack is passed
		 * as an argument, mark stack space it occupies as initialized.
		 */
		if (err == NULL && rv->v.type == RTE_BPF_ARG_PTR_STACK) {

			i = rv->u.max / sizeof(uint64_t);
			n = i + arg->size / sizeof(uint64_t);
			while (i != n) {
				eval_fill_max_bound(st->sv + i, UINT64_MAX);
				i++;
			};
		}
	}

	return err;
}

static const char *
eval_call(struct bpf_verifier *bvf, const struct ebpf_insn *ins)
{
	uint64_t msk;
	uint32_t i, idx;
	struct bpf_reg_val *rv;
	const struct rte_bpf_xsym *xsym;
	const char *err;

	idx = ins->imm;

	if (idx >= bvf->prm->nb_xsym ||
			bvf->prm->xsym[idx].type != RTE_BPF_XTYPE_FUNC)
		return "invalid external function index";

	/* for now don't support function calls on 32 bit platform */
	if (sizeof(uint64_t) != sizeof(uintptr_t))
		return "function calls are supported only for 64 bit apps";

	xsym = bvf->prm->xsym + idx;

	/* evaluate function arguments */
	err = NULL;
	for (i = 0; i != xsym->func.nb_args && err == NULL; i++) {
		err = eval_func_arg(bvf, xsym->func.args + i,
			bvf->evst->rv + EBPF_REG_1 + i);
	}

	/* R1-R5 argument/scratch registers */
	for (i = EBPF_REG_1; i != EBPF_REG_6; i++)
		bvf->evst->rv[i].v.type = RTE_BPF_ARG_UNDEF;

	/* update return value */

	rv = bvf->evst->rv + EBPF_REG_0;
	rv->v = xsym->func.ret;
	msk = (rv->v.type == RTE_BPF_ARG_RAW) ?
		RTE_LEN2MASK(rv->v.size * CHAR_BIT, uint64_t) : UINTPTR_MAX;
	eval_max_bound(rv, msk);
	rv->mask = msk;

	return err;
}

static void
eval_jeq_jne(struct bpf_reg_val *trd, struct bpf_reg_val *trs)
{
	/* sreg is constant */
	if (trs->u.min == trs->u.max) {
		trd->u = trs->u;
	/* dreg is constant */
	} else if (trd->u.min == trd->u.max) {
		trs->u = trd->u;
	} else {
		trd->u.max = RTE_MIN(trd->u.max, trs->u.max);
		trd->u.min = RTE_MAX(trd->u.min, trs->u.min);
		trs->u = trd->u;
	}

	/* sreg is constant */
	if (trs->s.min == trs->s.max) {
		trd->s = trs->s;
	/* dreg is constant */
	} else if (trd->s.min == trd->s.max) {
		trs->s = trd->s;
	} else {
		trd->s.max = RTE_MIN(trd->s.max, trs->s.max);
		trd->s.min = RTE_MAX(trd->s.min, trs->s.min);
		trs->s = trd->s;
	}
}

static void
eval_jgt_jle(struct bpf_reg_val *trd, struct bpf_reg_val *trs,
	struct bpf_reg_val *frd, struct bpf_reg_val *frs)
{
	frd->u.max = RTE_MIN(frd->u.max, frs->u.min);
	trd->u.min = RTE_MAX(trd->u.min, trs->u.min + 1);
}

static void
eval_jlt_jge(struct bpf_reg_val *trd, struct bpf_reg_val *trs,
	struct bpf_reg_val *frd, struct bpf_reg_val *frs)
{
	frd->u.min = RTE_MAX(frd->u.min, frs->u.min);
	trd->u.max = RTE_MIN(trd->u.max, trs->u.max - 1);
}

static void
eval_jsgt_jsle(struct bpf_reg_val *trd, struct bpf_reg_val *trs,
	struct bpf_reg_val *frd, struct bpf_reg_val *frs)
{
	frd->s.max = RTE_MIN(frd->s.max, frs->s.min);
	trd->s.min = RTE_MAX(trd->s.min, trs->s.min + 1);
}

static void
eval_jslt_jsge(struct bpf_reg_val *trd, struct bpf_reg_val *trs,
	struct bpf_reg_val *frd, struct bpf_reg_val *frs)
{
	frd->s.min = RTE_MAX(frd->s.min, frs->s.min);
	trd->s.max = RTE_MIN(trd->s.max, trs->s.max - 1);
}

static const char *
eval_jcc(struct bpf_verifier *bvf, const struct ebpf_insn *ins)
{
	uint32_t op;
	const char *err;
	struct bpf_eval_state *fst, *tst;
	struct bpf_reg_val *frd, *frs, *trd, *trs;
	struct bpf_reg_val rvf, rvt;

	tst = bvf->evst;
	fst = bvf->evin->evst;

	frd = fst->rv + ins->dst_reg;
	trd = tst->rv + ins->dst_reg;

	if (BPF_SRC(ins->code) == BPF_X) {
		frs = fst->rv + ins->src_reg;
		trs = tst->rv + ins->src_reg;
	} else {
		frs = &rvf;
		trs = &rvt;
		eval_fill_imm(frs, UINT64_MAX, ins->imm);
		eval_fill_imm(trs, UINT64_MAX, ins->imm);
	}

	err = eval_defined(trd, trs);
	if (err != NULL)
		return err;

	op = BPF_OP(ins->code);

	if (op == BPF_JEQ)
		eval_jeq_jne(trd, trs);
	else if (op == EBPF_JNE)
		eval_jeq_jne(frd, frs);
	else if (op == BPF_JGT)
		eval_jgt_jle(trd, trs, frd, frs);
	else if (op == EBPF_JLE)
		eval_jgt_jle(frd, frs, trd, trs);
	else if (op == EBPF_JLT)
		eval_jlt_jge(trd, trs, frd, frs);
	else if (op == BPF_JGE)
		eval_jlt_jge(frd, frs, trd, trs);
	else if (op == EBPF_JSGT)
		eval_jsgt_jsle(trd, trs, frd, frs);
	else if (op == EBPF_JSLE)
		eval_jsgt_jsle(frd, frs, trd, trs);
	else if (op == EBPF_JLT)
		eval_jslt_jsge(trd, trs, frd, frs);
	else if (op == EBPF_JSGE)
		eval_jslt_jsge(frd, frs, trd, trs);

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
		.eval = eval_alu,
	},
	[(BPF_ALU | BPF_SUB | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
		.eval = eval_alu,
	},
	[(BPF_ALU | BPF_AND | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
		.eval = eval_alu,
	},
	[(BPF_ALU | BPF_OR | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
		.eval = eval_alu,
	},
	[(BPF_ALU | BPF_LSH | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
		.eval = eval_alu,
	},
	[(BPF_ALU | BPF_RSH | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
		.eval = eval_alu,
	},
	[(BPF_ALU | BPF_XOR | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
		.eval = eval_alu,
	},
	[(BPF_ALU | BPF_MUL | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
		.eval = eval_alu,
	},
	[(BPF_ALU | EBPF_MOV | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
		.eval = eval_alu,
	},
	[(BPF_ALU | BPF_DIV | BPF_K)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 1, .max = UINT32_MAX},
		.eval = eval_alu,
	},
	[(BPF_ALU | BPF_MOD | BPF_K)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 1, .max = UINT32_MAX},
		.eval = eval_alu,
	},
	/* ALU IMM 64-bit instructions */
	[(EBPF_ALU64 | BPF_ADD | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | BPF_SUB | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | BPF_AND | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | BPF_OR | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | BPF_LSH | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | BPF_RSH | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | EBPF_ARSH | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | BPF_XOR | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | BPF_MUL | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | EBPF_MOV | BPF_K)] = {
		.mask = {.dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = UINT32_MAX,},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | BPF_DIV | BPF_K)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 1, .max = UINT32_MAX},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | BPF_MOD | BPF_K)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 1, .max = UINT32_MAX},
		.eval = eval_alu,
	},
	/* ALU REG 32-bit instructions */
	[(BPF_ALU | BPF_ADD | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(BPF_ALU | BPF_SUB | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(BPF_ALU | BPF_AND | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(BPF_ALU | BPF_OR | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(BPF_ALU | BPF_LSH | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(BPF_ALU | BPF_RSH | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(BPF_ALU | BPF_XOR | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(BPF_ALU | BPF_MUL | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(BPF_ALU | BPF_DIV | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(BPF_ALU | BPF_MOD | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(BPF_ALU | EBPF_MOV | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(BPF_ALU | BPF_NEG)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(BPF_ALU | EBPF_END | EBPF_TO_BE)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 16, .max = 64},
		.check = check_alu_bele,
		.eval = eval_bele,
	},
	[(BPF_ALU | EBPF_END | EBPF_TO_LE)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 16, .max = 64},
		.check = check_alu_bele,
		.eval = eval_bele,
	},
	/* ALU REG 64-bit instructions */
	[(EBPF_ALU64 | BPF_ADD | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | BPF_SUB | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | BPF_AND | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | BPF_OR | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | BPF_LSH | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | BPF_RSH | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | EBPF_ARSH | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | BPF_XOR | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | BPF_MUL | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | BPF_DIV | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | BPF_MOD | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | EBPF_MOV | BPF_X)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
	},
	[(EBPF_ALU64 | BPF_NEG)] = {
		.mask = { .dreg = WRT_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = 0},
		.imm = { .min = 0, .max = 0},
		.eval = eval_alu,
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
		.eval = eval_ld_imm64,
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
		.eval = eval_jcc,
	},
	[(BPF_JMP | EBPF_JNE | BPF_K)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
		.eval = eval_jcc,
	},
	[(BPF_JMP | BPF_JGT | BPF_K)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
		.eval = eval_jcc,
	},
	[(BPF_JMP | EBPF_JLT | BPF_K)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
		.eval = eval_jcc,
	},
	[(BPF_JMP | BPF_JGE | BPF_K)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
		.eval = eval_jcc,
	},
	[(BPF_JMP | EBPF_JLE | BPF_K)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
		.eval = eval_jcc,
	},
	[(BPF_JMP | EBPF_JSGT | BPF_K)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
		.eval = eval_jcc,
	},
	[(BPF_JMP | EBPF_JSLT | BPF_K)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
		.eval = eval_jcc,
	},
	[(BPF_JMP | EBPF_JSGE | BPF_K)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
		.eval = eval_jcc,
	},
	[(BPF_JMP | EBPF_JSLE | BPF_K)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
		.eval = eval_jcc,
	},
	[(BPF_JMP | BPF_JSET | BPF_K)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ZERO_REG},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = UINT32_MAX},
		.eval = eval_jcc,
	},
	/* jcc REG instructions */
	[(BPF_JMP | BPF_JEQ | BPF_X)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
		.eval = eval_jcc,
	},
	[(BPF_JMP | EBPF_JNE | BPF_X)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
		.eval = eval_jcc,
	},
	[(BPF_JMP | BPF_JGT | BPF_X)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
		.eval = eval_jcc,
	},
	[(BPF_JMP | EBPF_JLT | BPF_X)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
		.eval = eval_jcc,
	},
	[(BPF_JMP | BPF_JGE | BPF_X)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
		.eval = eval_jcc,
	},
	[(BPF_JMP | EBPF_JLE | BPF_X)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
		.eval = eval_jcc,
	},
	[(BPF_JMP | EBPF_JSGT | BPF_X)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
		.eval = eval_jcc,
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
		.eval = eval_jcc,
	},
	[(BPF_JMP | EBPF_JSLE | BPF_X)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
		.eval = eval_jcc,
	},
	[(BPF_JMP | BPF_JSET | BPF_X)] = {
		.mask = { .dreg = ALL_REGS, .sreg = ALL_REGS},
		.off = { .min = 0, .max = UINT16_MAX},
		.imm = { .min = 0, .max = 0},
		.eval = eval_jcc,
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
		.eval = eval_exit,
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
			"%s: internal error (out of space) at pc: %u\n",
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

static void
log_eval_state(const struct bpf_verifier *bvf, const struct ebpf_insn *ins,
	uint32_t pc, int32_t loglvl)
{
	const struct bpf_eval_state *st;
	const struct bpf_reg_val *rv;

	rte_log(loglvl, rte_bpf_logtype, "%s(pc=%u):\n", __func__, pc);

	st = bvf->evst;
	rv = st->rv + ins->dst_reg;

	rte_log(loglvl, rte_bpf_logtype,
		"r%u={\n"
		"\tv={type=%u, size=%zu},\n"
		"\tmask=0x%" PRIx64 ",\n"
		"\tu={min=0x%" PRIx64 ", max=0x%" PRIx64 "},\n"
		"\ts={min=%" PRId64 ", max=%" PRId64 "},\n"
		"};\n",
		ins->dst_reg,
		rv->v.type, rv->v.size,
		rv->mask,
		rv->u.min, rv->u.max,
		rv->s.min, rv->s.max);
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

	/* initial state of frame pointer */
	static const struct bpf_reg_val rvfp = {
		.v = {
			.type = RTE_BPF_ARG_PTR_STACK,
			.size = MAX_BPF_STACK_SIZE,
		},
		.mask = UINT64_MAX,
		.u = {.min = MAX_BPF_STACK_SIZE, .max = MAX_BPF_STACK_SIZE},
		.s = {.min = MAX_BPF_STACK_SIZE, .max = MAX_BPF_STACK_SIZE},
	};

	bvf->evst->rv[EBPF_REG_1].v = bvf->prm->prog_arg;
	bvf->evst->rv[EBPF_REG_1].mask = UINT64_MAX;
	if (bvf->prm->prog_arg.type == RTE_BPF_ARG_RAW)
		eval_max_bound(bvf->evst->rv + EBPF_REG_1, UINT64_MAX);

	bvf->evst->rv[EBPF_REG_10] = rvfp;

	ins = bvf->prm->ins;
	node = bvf->in;
	next = node;
	rc = 0;

	while (node != NULL && rc == 0) {

		/*
		 * current node evaluation, make sure we evaluate
		 * each node only once.
		 */
		if (next != NULL) {

			bvf->evin = node;
			idx = get_node_idx(bvf, node);
			op = ins[idx].code;

			/* for jcc node make a copy of evaluatoion state */
			if (node->nb_edge > 1)
				rc |= save_eval_state(bvf, node);

			if (ins_chk[op].eval != NULL && rc == 0) {
				err = ins_chk[op].eval(bvf, ins + idx);
				if (err != NULL) {
					RTE_BPF_LOG(ERR, "%s: %s at pc: %u\n",
						__func__, err, idx);
					rc = -EINVAL;
				}
			}

			log_eval_state(bvf, ins + idx, idx, RTE_LOG_DEBUG);
			bvf->evin = NULL;
		}

		/* proceed through CFG */
		next = get_next_node(bvf, node);
		if (next != NULL) {

			/* proceed with next child */
			if (node->cur_edge == node->nb_edge &&
					node->evst != NULL)
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
