/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Microsoft Corporation
 *
 * Based on bpf_convert_filter() in the Linux kernel sources
 * and filter2xdp.
 *
 * Licensed as BSD with permission original authors.
 * Copyright (C) 2017 Tobias Klauser
 * Copyright (c) 2011 - 2014 PLUMgrid, http://plumgrid.com
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_common.h>
#include <rte_bpf.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_errno.h>

#include <pcap/pcap.h>
#include <pcap/bpf.h>

#include "bpf_impl.h"
#include "bpf_def.h"

#ifndef BPF_MAXINSNS
#define BPF_MAXINSNS 4096
#endif

/*
 * Linux socket filter uses negative absolute offsets to
 * reference ancillary data.
 */
#define SKF_AD_OFF    (-0x1000)
#define SKF_AD_PROTOCOL 0
#define SKF_AD_PKTTYPE	4
#define SKF_AD_IFINDEX	8
#define SKF_AD_NLATTR	12
#define SKF_AD_NLATTR_NEST	16
#define SKF_AD_MARK	20
#define SKF_AD_QUEUE	24
#define SKF_AD_HATYPE	28
#define SKF_AD_RXHASH	32
#define SKF_AD_CPU	36
#define SKF_AD_ALU_XOR_X	40
#define SKF_AD_VLAN_TAG	44
#define SKF_AD_VLAN_TAG_PRESENT 48
#define SKF_AD_PAY_OFFSET	52
#define SKF_AD_RANDOM	56
#define SKF_AD_VLAN_TPID	60
#define SKF_AD_MAX	64

/* ArgX, context and stack frame pointer register positions. Note,
 * Arg1, Arg2, Arg3, etc are used as argument mappings of function
 * calls in BPF_CALL instruction.
 */
#define BPF_REG_ARG1	EBPF_REG_1
#define BPF_REG_ARG2	EBPF_REG_2
#define BPF_REG_ARG3	EBPF_REG_3
#define BPF_REG_ARG4	EBPF_REG_4
#define BPF_REG_ARG5	EBPF_REG_5
#define BPF_REG_CTX	EBPF_REG_6
#define BPF_REG_FP	EBPF_REG_10

/* Additional register mappings for converted user programs. */
#define BPF_REG_A	EBPF_REG_0
#define BPF_REG_X	EBPF_REG_7
#define BPF_REG_TMP	EBPF_REG_8

/* Helper macros for filter block array initializers. */

/* ALU ops on registers, bpf_add|sub|...: dst_reg += src_reg */

#define EBPF_ALU64_REG(OP, DST, SRC)				\
	((struct ebpf_insn) {					\
		.code  = EBPF_ALU64 | BPF_OP(OP) | BPF_X,	\
		.dst_reg = DST,					\
		.src_reg = SRC,					\
		.off   = 0,					\
		.imm   = 0 })

#define BPF_ALU32_REG(OP, DST, SRC)				\
	((struct ebpf_insn) {					\
		.code  = BPF_ALU | BPF_OP(OP) | BPF_X,		\
		.dst_reg = DST,					\
		.src_reg = SRC,					\
		.off   = 0,					\
		.imm   = 0 })

/* ALU ops on immediates, bpf_add|sub|...: dst_reg += imm32 */

#define BPF_ALU32_IMM(OP, DST, IMM)				\
	((struct ebpf_insn) {					\
		.code  = BPF_ALU | BPF_OP(OP) | BPF_K,		\
		.dst_reg = DST,					\
		.src_reg = 0,					\
		.off   = 0,					\
		.imm   = IMM })

/* Short form of mov, dst_reg = src_reg */

#define BPF_MOV64_REG(DST, SRC)					\
	((struct ebpf_insn) {					\
		.code  = EBPF_ALU64 | EBPF_MOV | BPF_X,		\
		.dst_reg = DST,					\
		.src_reg = SRC,					\
		.off   = 0,					\
		.imm   = 0 })

#define BPF_MOV32_REG(DST, SRC)					\
	((struct ebpf_insn) {					\
		.code  = BPF_ALU | EBPF_MOV | BPF_X,		\
		.dst_reg = DST,					\
		.src_reg = SRC,					\
		.off   = 0,					\
		.imm   = 0 })

/* Short form of mov, dst_reg = imm32 */

#define BPF_MOV32_IMM(DST, IMM)					\
	((struct ebpf_insn) {					\
		.code  = BPF_ALU | EBPF_MOV | BPF_K,		\
		.dst_reg = DST,					\
		.src_reg = 0,					\
		.off   = 0,					\
		.imm   = IMM })

/* Short form of mov based on type, BPF_X: dst_reg = src_reg, BPF_K: dst_reg = imm32 */

#define BPF_MOV32_RAW(TYPE, DST, SRC, IMM)			\
	((struct ebpf_insn) {					\
		.code  = BPF_ALU | EBPF_MOV | BPF_SRC(TYPE),	\
		.dst_reg = DST,					\
		.src_reg = SRC,					\
		.off   = 0,					\
		.imm   = IMM })

/* Direct packet access, R0 = *(uint *) (skb->data + imm32) */

#define BPF_LD_ABS(SIZE, IMM)					\
	((struct ebpf_insn) {					\
		.code  = BPF_LD | BPF_SIZE(SIZE) | BPF_ABS,	\
		.dst_reg = 0,					\
		.src_reg = 0,					\
		.off   = 0,					\
		.imm   = IMM })

/* Memory load, dst_reg = *(uint *) (src_reg + off16) */

#define BPF_LDX_MEM(SIZE, DST, SRC, OFF)			\
	((struct ebpf_insn) {					\
		.code  = BPF_LDX | BPF_SIZE(SIZE) | BPF_MEM,	\
		.dst_reg = DST,					\
		.src_reg = SRC,					\
		.off   = OFF,					\
		.imm   = 0 })

/* Memory store, *(uint *) (dst_reg + off16) = src_reg */

#define BPF_STX_MEM(SIZE, DST, SRC, OFF)			\
	((struct ebpf_insn) {					\
		.code  = BPF_STX | BPF_SIZE(SIZE) | BPF_MEM,	\
		.dst_reg = DST,					\
		.src_reg = SRC,					\
		.off   = OFF,					\
		.imm   = 0 })

/* Conditional jumps against immediates, if (dst_reg 'op' imm32) goto pc + off16 */

#define BPF_JMP_IMM(OP, DST, IMM, OFF)				\
	((struct ebpf_insn) {					\
		.code  = BPF_JMP | BPF_OP(OP) | BPF_K,		\
		.dst_reg = DST,					\
		.src_reg = 0,					\
		.off   = OFF,					\
		.imm   = IMM })

/* Raw code statement block */

#define BPF_RAW_INSN(CODE, DST, SRC, OFF, IMM)			\
	((struct ebpf_insn) {					\
		.code  = CODE,					\
		.dst_reg = DST,					\
		.src_reg = SRC,					\
		.off   = OFF,					\
		.imm   = IMM })

/* Program exit */

#define BPF_EXIT_INSN()						\
	((struct ebpf_insn) {					\
		.code  = BPF_JMP | EBPF_EXIT,			\
		.dst_reg = 0,					\
		.src_reg = 0,					\
		.off   = 0,					\
		.imm   = 0 })

/*
 * Placeholder to convert BPF extensions like length and VLAN tag
 * If and when DPDK BPF supports them.
 */
static bool convert_bpf_load(const struct bpf_insn *fp,
			     struct ebpf_insn **new_insnp __rte_unused)
{
	switch (fp->k) {
	case SKF_AD_OFF + SKF_AD_PROTOCOL:
	case SKF_AD_OFF + SKF_AD_PKTTYPE:
	case SKF_AD_OFF + SKF_AD_IFINDEX:
	case SKF_AD_OFF + SKF_AD_HATYPE:
	case SKF_AD_OFF + SKF_AD_MARK:
	case SKF_AD_OFF + SKF_AD_RXHASH:
	case SKF_AD_OFF + SKF_AD_QUEUE:
	case SKF_AD_OFF + SKF_AD_VLAN_TAG:
	case SKF_AD_OFF + SKF_AD_VLAN_TAG_PRESENT:
	case SKF_AD_OFF + SKF_AD_VLAN_TPID:
	case SKF_AD_OFF + SKF_AD_PAY_OFFSET:
	case SKF_AD_OFF + SKF_AD_NLATTR:
	case SKF_AD_OFF + SKF_AD_NLATTR_NEST:
	case SKF_AD_OFF + SKF_AD_CPU:
	case SKF_AD_OFF + SKF_AD_RANDOM:
	case SKF_AD_OFF + SKF_AD_ALU_XOR_X:
		/* Linux has special negative offsets to access meta-data. */
		RTE_BPF_LOG_LINE(ERR,
			    "rte_bpf_convert: socket offset %d not supported",
			    fp->k - SKF_AD_OFF);
		return true;
	default:
		return false;
	}
}

static int bpf_convert_filter(const struct bpf_insn *prog, size_t len,
			      struct ebpf_insn *new_prog, uint32_t *new_len)
{
	unsigned int pass = 0;
	size_t new_flen = 0, target, i;
	struct ebpf_insn *new_insn;
	const struct bpf_insn *fp;
	int *addrs = NULL;
	uint8_t bpf_src;

	if (len > BPF_MAXINSNS) {
		RTE_BPF_LOG_LINE(ERR, "%s: cBPF program too long (%zu insns)",
			    __func__, len);
		return -EINVAL;
	}

	/* On second pass, allocate the new program */
	if (new_prog) {
		addrs = calloc(len, sizeof(*addrs));
		if (addrs == NULL)
			return -ENOMEM;
	}

do_pass:
	new_insn = new_prog;
	fp = prog;

	/* Classic BPF related prologue emission. */
	if (new_insn) {
		/* Classic BPF expects A and X to be reset first. These need
		 * to be guaranteed to be the first two instructions.
		 */
		*new_insn++ = EBPF_ALU64_REG(BPF_XOR, BPF_REG_A, BPF_REG_A);
		*new_insn++ = EBPF_ALU64_REG(BPF_XOR, BPF_REG_X, BPF_REG_X);

		/* All programs must keep CTX in callee saved BPF_REG_CTX.
		 * In eBPF case it's done by the compiler, here we need to
		 * do this ourself. Initial CTX is present in BPF_REG_ARG1.
		 */
		*new_insn++ = BPF_MOV64_REG(BPF_REG_CTX, BPF_REG_ARG1);
	} else {
		new_insn += 3;
	}

	for (i = 0; i < len; fp++, i++) {
		struct ebpf_insn tmp_insns[6] = { };
		struct ebpf_insn *insn = tmp_insns;

		if (addrs)
			addrs[i] = new_insn - new_prog;

		switch (fp->code) {
			/* Absolute loads are how classic BPF accesses skb */
		case BPF_LD | BPF_ABS | BPF_W:
		case BPF_LD | BPF_ABS | BPF_H:
		case BPF_LD | BPF_ABS | BPF_B:
			if (convert_bpf_load(fp, &insn))
				goto err;

			*insn = BPF_RAW_INSN(fp->code, 0, 0, 0, fp->k);
			break;

		case BPF_ALU | BPF_DIV | BPF_X:
		case BPF_ALU | BPF_MOD | BPF_X:
			/* For cBPF, don't cause floating point exception */
			*insn++ = BPF_MOV32_REG(BPF_REG_X, BPF_REG_X);
			*insn++ = BPF_JMP_IMM(EBPF_JNE, BPF_REG_X, 0, 2);
			*insn++ = BPF_ALU32_REG(BPF_XOR, BPF_REG_A, BPF_REG_A);
			*insn++ = BPF_EXIT_INSN();
			/* fallthrough */
		case BPF_ALU | BPF_ADD | BPF_X:
		case BPF_ALU | BPF_ADD | BPF_K:
		case BPF_ALU | BPF_SUB | BPF_X:
		case BPF_ALU | BPF_SUB | BPF_K:
		case BPF_ALU | BPF_AND | BPF_X:
		case BPF_ALU | BPF_AND | BPF_K:
		case BPF_ALU | BPF_OR | BPF_X:
		case BPF_ALU | BPF_OR | BPF_K:
		case BPF_ALU | BPF_LSH | BPF_X:
		case BPF_ALU | BPF_LSH | BPF_K:
		case BPF_ALU | BPF_RSH | BPF_X:
		case BPF_ALU | BPF_RSH | BPF_K:
		case BPF_ALU | BPF_XOR | BPF_X:
		case BPF_ALU | BPF_XOR | BPF_K:
		case BPF_ALU | BPF_MUL | BPF_X:
		case BPF_ALU | BPF_MUL | BPF_K:
		case BPF_ALU | BPF_DIV | BPF_K:
		case BPF_ALU | BPF_MOD | BPF_K:
		case BPF_ALU | BPF_NEG:
		case BPF_LD | BPF_IND | BPF_W:
		case BPF_LD | BPF_IND | BPF_H:
		case BPF_LD | BPF_IND | BPF_B:
			/* All arithmetic insns map as-is. */
			insn->code = fp->code;
			insn->dst_reg = BPF_REG_A;
			bpf_src = BPF_SRC(fp->code);
			insn->src_reg = bpf_src == BPF_X ? BPF_REG_X : 0;
			insn->off = 0;
			insn->imm = fp->k;
			break;

			/* Jump transformation cannot use BPF block macros
			 * everywhere as offset calculation and target updates
			 * require a bit more work than the rest, i.e. jump
			 * opcodes map as-is, but offsets need adjustment.
			 */

#define BPF_EMIT_JMP							\
			do {						\
				if (target >= len)			\
					goto err;			\
				insn->off = addrs ? addrs[target] - addrs[i] - 1 : 0; \
				/* Adjust pc relative offset for 2nd or 3rd insn. */ \
				insn->off -= insn - tmp_insns;		\
			} while (0)

		case BPF_JMP | BPF_JA:
			target = i + fp->k + 1;
			insn->code = fp->code;
			BPF_EMIT_JMP;
			break;

		case BPF_JMP | BPF_JEQ | BPF_K:
		case BPF_JMP | BPF_JEQ | BPF_X:
		case BPF_JMP | BPF_JSET | BPF_K:
		case BPF_JMP | BPF_JSET | BPF_X:
		case BPF_JMP | BPF_JGT | BPF_K:
		case BPF_JMP | BPF_JGT | BPF_X:
		case BPF_JMP | BPF_JGE | BPF_K:
		case BPF_JMP | BPF_JGE | BPF_X:
			if (BPF_SRC(fp->code) == BPF_K && (int) fp->k < 0) {
				/* BPF immediates are signed, zero extend
				 * immediate into tmp register and use it
				 * in compare insn.
				 */
				*insn++ = BPF_MOV32_IMM(BPF_REG_TMP, fp->k);

				insn->dst_reg = BPF_REG_A;
				insn->src_reg = BPF_REG_TMP;
				bpf_src = BPF_X;
			} else {
				insn->dst_reg = BPF_REG_A;
				insn->imm = fp->k;
				bpf_src = BPF_SRC(fp->code);
				insn->src_reg = bpf_src == BPF_X ? BPF_REG_X : 0;
			}

			/* Common case where 'jump_false' is next insn. */
			if (fp->jf == 0) {
				insn->code = BPF_JMP | BPF_OP(fp->code) | bpf_src;
				target = i + fp->jt + 1;
				BPF_EMIT_JMP;
				break;
			}

			/* Convert JEQ into JNE when 'jump_true' is next insn. */
			if (fp->jt == 0 && BPF_OP(fp->code) == BPF_JEQ) {
				insn->code = BPF_JMP | EBPF_JNE | bpf_src;
				target = i + fp->jf + 1;
				BPF_EMIT_JMP;
				break;
			}

			/* Other jumps are mapped into two insns: Jxx and JA. */
			target = i + fp->jt + 1;
			insn->code = BPF_JMP | BPF_OP(fp->code) | bpf_src;
			BPF_EMIT_JMP;
			insn++;

			insn->code = BPF_JMP | BPF_JA;
			target = i + fp->jf + 1;
			BPF_EMIT_JMP;
			break;

			/* ldxb 4 * ([14] & 0xf) is remapped into 6 insns. */
		case BPF_LDX | BPF_MSH | BPF_B:
			/* tmp = A */
			*insn++ = BPF_MOV64_REG(BPF_REG_TMP, BPF_REG_A);
			/* A = BPF_R0 = *(u8 *) (skb->data + K) */
			*insn++ = BPF_LD_ABS(BPF_B, fp->k);
			/* A &= 0xf */
			*insn++ = BPF_ALU32_IMM(BPF_AND, BPF_REG_A, 0xf);
			/* A <<= 2 */
			*insn++ = BPF_ALU32_IMM(BPF_LSH, BPF_REG_A, 2);
			/* X = A */
			*insn++ = BPF_MOV64_REG(BPF_REG_X, BPF_REG_A);
			/* A = tmp */
			*insn = BPF_MOV64_REG(BPF_REG_A, BPF_REG_TMP);
			break;

			/* RET_K is remapped into 2 insns. RET_A case doesn't need an
			 * extra mov as EBPF_REG_0 is already mapped into BPF_REG_A.
			 */
		case BPF_RET | BPF_A:
		case BPF_RET | BPF_K:
			if (BPF_RVAL(fp->code) == BPF_K) {
				*insn++ = BPF_MOV32_RAW(BPF_K, EBPF_REG_0,
							0, fp->k);
			}
			*insn = BPF_EXIT_INSN();
			break;

			/* Store to stack. */
		case BPF_ST:
		case BPF_STX:
			*insn = BPF_STX_MEM(BPF_W, BPF_REG_FP, BPF_CLASS(fp->code) ==
					    BPF_ST ? BPF_REG_A : BPF_REG_X,
					    -(BPF_MEMWORDS - fp->k) * 4);
			break;

			/* Load from stack. */
		case BPF_LD | BPF_MEM:
		case BPF_LDX | BPF_MEM:
			*insn = BPF_LDX_MEM(BPF_W, BPF_CLASS(fp->code) == BPF_LD  ?
					    BPF_REG_A : BPF_REG_X, BPF_REG_FP,
					    -(BPF_MEMWORDS - fp->k) * 4);
			break;

			/* A = K or X = K */
		case BPF_LD | BPF_IMM:
		case BPF_LDX | BPF_IMM:
			*insn = BPF_MOV32_IMM(BPF_CLASS(fp->code) == BPF_LD ?
					      BPF_REG_A : BPF_REG_X, fp->k);
			break;

			/* X = A */
		case BPF_MISC | BPF_TAX:
			*insn = BPF_MOV64_REG(BPF_REG_X, BPF_REG_A);
			break;

			/* A = X */
		case BPF_MISC | BPF_TXA:
			*insn = BPF_MOV64_REG(BPF_REG_A, BPF_REG_X);
			break;

			/* A = mbuf->len or X = mbuf->len */
		case BPF_LD | BPF_W | BPF_LEN:
		case BPF_LDX | BPF_W | BPF_LEN:
			/* BPF_ABS/BPF_IND implicitly expect mbuf ptr in R6 */

			*insn = BPF_LDX_MEM(BPF_W, BPF_CLASS(fp->code) == BPF_LD ?
					    BPF_REG_A : BPF_REG_X, BPF_REG_CTX,
					    offsetof(struct rte_mbuf, pkt_len));
			break;

			/* Unknown instruction. */
		default:
			RTE_BPF_LOG_LINE(ERR, "%s: Unknown instruction!: %#x",
				    __func__, fp->code);
			goto err;
		}

		insn++;
		if (new_prog)
			memcpy(new_insn, tmp_insns,
			       sizeof(*insn) * (insn - tmp_insns));
		new_insn += insn - tmp_insns;
	}

	if (!new_prog) {
		/* Only calculating new length. */
		*new_len = new_insn - new_prog;
		return 0;
	}

	pass++;
	if ((ptrdiff_t)new_flen != new_insn - new_prog) {
		new_flen = new_insn - new_prog;
		if (pass > 2)
			goto err;
		goto do_pass;
	}

	free(addrs);
	assert(*new_len == new_flen);

	return 0;
err:
	free(addrs);
	return -1;
}

struct rte_bpf_prm *
rte_bpf_convert(const struct bpf_program *prog)
{
	struct rte_bpf_prm *prm = NULL;
	struct ebpf_insn *ebpf = NULL;
	uint32_t ebpf_len = 0;
	int ret;

	if (prog == NULL) {
		RTE_BPF_LOG_LINE(ERR, "%s: NULL program", __func__);
		rte_errno = EINVAL;
		return NULL;
	}

	/* 1st pass: calculate the eBPF program length */
	ret = bpf_convert_filter(prog->bf_insns, prog->bf_len, NULL, &ebpf_len);
	if (ret < 0) {
		RTE_BPF_LOG_LINE(ERR, "%s: cannot get eBPF length", __func__);
		rte_errno = -ret;
		return NULL;
	}

	RTE_BPF_LOG_LINE(DEBUG, "%s: prog len cBPF=%u -> eBPF=%u",
		    __func__, prog->bf_len, ebpf_len);

	prm = rte_zmalloc("bpf_filter",
			  sizeof(*prm) + ebpf_len * sizeof(*ebpf), 0);
	if (prm == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}

	/* The EPBF instructions in this case are right after the header */
	ebpf = (void *)(prm + 1);

	/* 2nd pass: remap cBPF to eBPF instructions  */
	ret = bpf_convert_filter(prog->bf_insns, prog->bf_len, ebpf, &ebpf_len);
	if (ret < 0) {
		RTE_BPF_LOG_LINE(ERR, "%s: cannot convert cBPF to eBPF", __func__);
		free(prm);
		rte_errno = -ret;
		return NULL;
	}

	prm->ins = ebpf;
	prm->nb_ins = ebpf_len;

	/* Classic BPF programs use mbufs */
	prm->prog_arg.type = RTE_BPF_ARG_PTR_MBUF;
	prm->prog_arg.size = sizeof(struct rte_mbuf);

	return prm;
}
