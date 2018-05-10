/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include <rte_memory.h>
#include <rte_debug.h>
#include <rte_hexdump.h>
#include <rte_random.h>
#include <rte_byteorder.h>
#include <rte_errno.h>
#include <rte_bpf.h>

#include "test.h"

/*
 * Basic functional tests for librte_bpf.
 * The main procedure - load eBPF program, execute it and
 * compare restuls with expected values.
 */

struct dummy_offset {
	uint64_t u64;
	uint32_t u32;
	uint16_t u16;
	uint8_t  u8;
};

struct dummy_vect8 {
	struct dummy_offset in[8];
	struct dummy_offset out[8];
};

#define	TEST_FILL_1	0xDEADBEEF

#define	TEST_MUL_1	21
#define TEST_MUL_2	-100

#define TEST_SHIFT_1	15
#define TEST_SHIFT_2	33

#define TEST_JCC_1	0
#define TEST_JCC_2	-123
#define TEST_JCC_3	5678
#define TEST_JCC_4	TEST_FILL_1

struct bpf_test {
	const char *name;
	size_t arg_sz;
	struct rte_bpf_prm prm;
	void (*prepare)(void *);
	int (*check_result)(uint64_t, const void *);
	uint32_t allow_fail;
};

/*
 * Compare return value and result data with expected ones.
 * Report a failure if they don't match.
 */
static int
cmp_res(const char *func, uint64_t exp_rc, uint64_t ret_rc,
	const void *exp_res, const void *ret_res, size_t res_sz)
{
	int32_t ret;

	ret = 0;
	if (exp_rc != ret_rc) {
		printf("%s@%d: invalid return value, expected: 0x%" PRIx64
			",result: 0x%" PRIx64 "\n",
			func, __LINE__, exp_rc, ret_rc);
		ret |= -1;
	}

	if (memcmp(exp_res, ret_res, res_sz) != 0) {
		printf("%s: invalid value\n", func);
		rte_memdump(stdout, "expected", exp_res, res_sz);
		rte_memdump(stdout, "result", ret_res, res_sz);
		ret |= -1;
	}

	return ret;
}

/* store immediate test-cases */
static const struct ebpf_insn test_store1_prog[] = {
	{
		.code = (BPF_ST | BPF_MEM | BPF_B),
		.dst_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_offset, u8),
		.imm = TEST_FILL_1,
	},
	{
		.code = (BPF_ST | BPF_MEM | BPF_H),
		.dst_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_offset, u16),
		.imm = TEST_FILL_1,
	},
	{
		.code = (BPF_ST | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_offset, u32),
		.imm = TEST_FILL_1,
	},
	{
		.code = (BPF_ST | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_offset, u64),
		.imm = TEST_FILL_1,
	},
	/* return 1 */
	{
		.code = (BPF_ALU | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = 1,
	},
	{
		.code = (BPF_JMP | EBPF_EXIT),
	},
};

static void
test_store1_prepare(void *arg)
{
	struct dummy_offset *df;

	df = arg;
	memset(df, 0, sizeof(*df));
}

static int
test_store1_check(uint64_t rc, const void *arg)
{
	const struct dummy_offset *dft;
	struct dummy_offset dfe;

	dft = arg;

	memset(&dfe, 0, sizeof(dfe));
	dfe.u64 = (int32_t)TEST_FILL_1;
	dfe.u32 = dfe.u64;
	dfe.u16 = dfe.u64;
	dfe.u8 = dfe.u64;

	return cmp_res(__func__, 1, rc, &dfe, dft, sizeof(dfe));
}

/* store register test-cases */
static const struct ebpf_insn test_store2_prog[] = {

	{
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_2,
		.imm = TEST_FILL_1,
	},
	{
		.code = (BPF_STX | BPF_MEM | BPF_B),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_2,
		.off = offsetof(struct dummy_offset, u8),
	},
	{
		.code = (BPF_STX | BPF_MEM | BPF_H),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_2,
		.off = offsetof(struct dummy_offset, u16),
	},
	{
		.code = (BPF_STX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_2,
		.off = offsetof(struct dummy_offset, u32),
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_2,
		.off = offsetof(struct dummy_offset, u64),
	},
	/* return 1 */
	{
		.code = (BPF_ALU | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = 1,
	},
	{
		.code = (BPF_JMP | EBPF_EXIT),
	},
};

/* load test-cases */
static const struct ebpf_insn test_load1_prog[] = {

	{
		.code = (BPF_LDX | BPF_MEM | BPF_B),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_offset, u8),
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_H),
		.dst_reg = EBPF_REG_3,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_offset, u16),
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_4,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_offset, u32),
	},
	{
		.code = (BPF_LDX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_offset, u64),
	},
	/* return sum */
	{
		.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_4,
	},
	{
		.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_3,
	},
	{
		.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_2,
	},
	{
		.code = (BPF_JMP | EBPF_EXIT),
	},
};

static void
test_load1_prepare(void *arg)
{
	struct dummy_offset *df;

	df = arg;

	memset(df, 0, sizeof(*df));
	df->u64 = (int32_t)TEST_FILL_1;
	df->u32 = df->u64;
	df->u16 = df->u64;
	df->u8 = df->u64;
}

static int
test_load1_check(uint64_t rc, const void *arg)
{
	uint64_t v;
	const struct dummy_offset *dft;

	dft = arg;
	v = dft->u64;
	v += dft->u32;
	v += dft->u16;
	v += dft->u8;

	return cmp_res(__func__, v, rc, dft, dft, sizeof(*dft));
}

/* alu mul test-cases */
static const struct ebpf_insn test_mul1_prog[] = {

	{
		.code = (BPF_LDX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[0].u32),
	},
	{
		.code = (BPF_LDX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_3,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[1].u64),
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_4,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[2].u32),
	},
	{
		.code = (BPF_ALU | BPF_MUL | BPF_K),
		.dst_reg = EBPF_REG_2,
		.imm = TEST_MUL_1,
	},
	{
		.code = (EBPF_ALU64 | BPF_MUL | BPF_K),
		.dst_reg = EBPF_REG_3,
		.imm = TEST_MUL_2,
	},
	{
		.code = (BPF_ALU | BPF_MUL | BPF_X),
		.dst_reg = EBPF_REG_4,
		.src_reg = EBPF_REG_2,
	},
	{
		.code = (EBPF_ALU64 | BPF_MUL | BPF_X),
		.dst_reg = EBPF_REG_4,
		.src_reg = EBPF_REG_3,
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_2,
		.off = offsetof(struct dummy_vect8, out[0].u64),
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_3,
		.off = offsetof(struct dummy_vect8, out[1].u64),
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_4,
		.off = offsetof(struct dummy_vect8, out[2].u64),
	},
	/* return 1 */
	{
		.code = (BPF_ALU | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = 1,
	},
	{
		.code = (BPF_JMP | EBPF_EXIT),
	},
};

static void
test_mul1_prepare(void *arg)
{
	struct dummy_vect8 *dv;
	uint64_t v;

	dv = arg;

	v = rte_rand();

	memset(dv, 0, sizeof(*dv));
	dv->in[0].u32 = v;
	dv->in[1].u64 = v << 12 | v >> 6;
	dv->in[2].u32 = -v;
}

static int
test_mul1_check(uint64_t rc, const void *arg)
{
	uint64_t r2, r3, r4;
	const struct dummy_vect8 *dvt;
	struct dummy_vect8 dve;

	dvt = arg;
	memset(&dve, 0, sizeof(dve));

	r2 = dvt->in[0].u32;
	r3 = dvt->in[1].u64;
	r4 = dvt->in[2].u32;

	r2 = (uint32_t)r2 * TEST_MUL_1;
	r3 *= TEST_MUL_2;
	r4 = (uint32_t)(r4 * r2);
	r4 *= r3;

	dve.out[0].u64 = r2;
	dve.out[1].u64 = r3;
	dve.out[2].u64 = r4;

	return cmp_res(__func__, 1, rc, dve.out, dvt->out, sizeof(dve.out));
}

/* alu shift test-cases */
static const struct ebpf_insn test_shift1_prog[] = {

	{
		.code = (BPF_LDX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[0].u32),
	},
	{
		.code = (BPF_LDX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_3,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[1].u64),
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_4,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[2].u32),
	},
	{
		.code = (BPF_ALU | BPF_LSH | BPF_K),
		.dst_reg = EBPF_REG_2,
		.imm = TEST_SHIFT_1,
	},
	{
		.code = (EBPF_ALU64 | EBPF_ARSH | BPF_K),
		.dst_reg = EBPF_REG_3,
		.imm = TEST_SHIFT_2,
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_2,
		.off = offsetof(struct dummy_vect8, out[0].u64),
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_3,
		.off = offsetof(struct dummy_vect8, out[1].u64),
	},
	{
		.code = (BPF_ALU | BPF_RSH | BPF_X),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_4,
	},
	{
		.code = (EBPF_ALU64 | BPF_LSH | BPF_X),
		.dst_reg = EBPF_REG_3,
		.src_reg = EBPF_REG_4,
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_2,
		.off = offsetof(struct dummy_vect8, out[2].u64),
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_3,
		.off = offsetof(struct dummy_vect8, out[3].u64),
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[0].u32),
	},
	{
		.code = (BPF_LDX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_3,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[1].u64),
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_4,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[2].u32),
	},
	{
		.code = (BPF_ALU | BPF_AND | BPF_K),
		.dst_reg = EBPF_REG_2,
		.imm = sizeof(uint64_t) * CHAR_BIT - 1,
	},
	{
		.code = (EBPF_ALU64 | EBPF_ARSH | BPF_X),
		.dst_reg = EBPF_REG_3,
		.src_reg = EBPF_REG_2,
	},
	{
		.code = (BPF_ALU | BPF_AND | BPF_K),
		.dst_reg = EBPF_REG_2,
		.imm = sizeof(uint32_t) * CHAR_BIT - 1,
	},
	{
		.code = (BPF_ALU | BPF_LSH | BPF_X),
		.dst_reg = EBPF_REG_4,
		.src_reg = EBPF_REG_2,
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_4,
		.off = offsetof(struct dummy_vect8, out[4].u64),
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_3,
		.off = offsetof(struct dummy_vect8, out[5].u64),
	},
	/* return 1 */
	{
		.code = (BPF_ALU | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = 1,
	},
	{
		.code = (BPF_JMP | EBPF_EXIT),
	},
};

static void
test_shift1_prepare(void *arg)
{
	struct dummy_vect8 *dv;
	uint64_t v;

	dv = arg;

	v = rte_rand();

	memset(dv, 0, sizeof(*dv));
	dv->in[0].u32 = v;
	dv->in[1].u64 = v << 12 | v >> 6;
	dv->in[2].u32 = (-v ^ 5);
}

static int
test_shift1_check(uint64_t rc, const void *arg)
{
	uint64_t r2, r3, r4;
	const struct dummy_vect8 *dvt;
	struct dummy_vect8 dve;

	dvt = arg;
	memset(&dve, 0, sizeof(dve));

	r2 = dvt->in[0].u32;
	r3 = dvt->in[1].u64;
	r4 = dvt->in[2].u32;

	r2 = (uint32_t)r2 << TEST_SHIFT_1;
	r3 = (int64_t)r3 >> TEST_SHIFT_2;

	dve.out[0].u64 = r2;
	dve.out[1].u64 = r3;

	r2 = (uint32_t)r2 >> r4;
	r3 <<= r4;

	dve.out[2].u64 = r2;
	dve.out[3].u64 = r3;

	r2 = dvt->in[0].u32;
	r3 = dvt->in[1].u64;
	r4 = dvt->in[2].u32;

	r2 &= sizeof(uint64_t) * CHAR_BIT - 1;
	r3 = (int64_t)r3 >> r2;
	r2 &= sizeof(uint32_t) * CHAR_BIT - 1;
	r4 = (uint32_t)r4 << r2;

	dve.out[4].u64 = r4;
	dve.out[5].u64 = r3;

	return cmp_res(__func__, 1, rc, dve.out, dvt->out, sizeof(dve.out));
}

/* jmp test-cases */
static const struct ebpf_insn test_jump1_prog[] = {

	[0] = {
		.code = (BPF_ALU | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = 0,
	},
	[1] = {
		.code = (BPF_LDX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[0].u32),
	},
	[2] = {
		.code = (BPF_LDX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_3,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[0].u64),
	},
	[3] = {
		.code = (BPF_LDX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_4,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[1].u32),
	},
	[4] = {
		.code = (BPF_LDX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_5,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[1].u64),
	},
	[5] = {
		.code = (BPF_JMP | BPF_JEQ | BPF_K),
		.dst_reg = EBPF_REG_2,
		.imm = TEST_JCC_1,
		.off = 8,
	},
	[6] = {
		.code = (BPF_JMP | EBPF_JSLE | BPF_K),
		.dst_reg = EBPF_REG_3,
		.imm = TEST_JCC_2,
		.off = 9,
	},
	[7] = {
		.code = (BPF_JMP | BPF_JGT | BPF_K),
		.dst_reg = EBPF_REG_4,
		.imm = TEST_JCC_3,
		.off = 10,
	},
	[8] = {
		.code = (BPF_JMP | BPF_JSET | BPF_K),
		.dst_reg = EBPF_REG_5,
		.imm = TEST_JCC_4,
		.off = 11,
	},
	[9] = {
		.code = (BPF_JMP | EBPF_JNE | BPF_X),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_3,
		.off = 12,
	},
	[10] = {
		.code = (BPF_JMP | EBPF_JSGT | BPF_X),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_4,
		.off = 13,
	},
	[11] = {
		.code = (BPF_JMP | EBPF_JLE | BPF_X),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_5,
		.off = 14,
	},
	[12] = {
		.code = (BPF_JMP | BPF_JSET | BPF_X),
		.dst_reg = EBPF_REG_3,
		.src_reg = EBPF_REG_5,
		.off = 15,
	},
	[13] = {
		.code = (BPF_JMP | EBPF_EXIT),
	},
	[14] = {
		.code = (EBPF_ALU64 | BPF_OR | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = 0x1,
	},
	[15] = {
		.code = (BPF_JMP | BPF_JA),
		.off = -10,
	},
	[16] = {
		.code = (EBPF_ALU64 | BPF_OR | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = 0x2,
	},
	[17] = {
		.code = (BPF_JMP | BPF_JA),
		.off = -11,
	},
	[18] = {
		.code = (EBPF_ALU64 | BPF_OR | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = 0x4,
	},
	[19] = {
		.code = (BPF_JMP | BPF_JA),
		.off = -12,
	},
	[20] = {
		.code = (EBPF_ALU64 | BPF_OR | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = 0x8,
	},
	[21] = {
		.code = (BPF_JMP | BPF_JA),
		.off = -13,
	},
	[22] = {
		.code = (EBPF_ALU64 | BPF_OR | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = 0x10,
	},
	[23] = {
		.code = (BPF_JMP | BPF_JA),
		.off = -14,
	},
	[24] = {
		.code = (EBPF_ALU64 | BPF_OR | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = 0x20,
	},
	[25] = {
		.code = (BPF_JMP | BPF_JA),
		.off = -15,
	},
	[26] = {
		.code = (EBPF_ALU64 | BPF_OR | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = 0x40,
	},
	[27] = {
		.code = (BPF_JMP | BPF_JA),
		.off = -16,
	},
	[28] = {
		.code = (EBPF_ALU64 | BPF_OR | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = 0x80,
	},
	[29] = {
		.code = (BPF_JMP | BPF_JA),
		.off = -17,
	},
};

static void
test_jump1_prepare(void *arg)
{
	struct dummy_vect8 *dv;
	uint64_t v1, v2;

	dv = arg;

	v1 = rte_rand();
	v2 = rte_rand();

	memset(dv, 0, sizeof(*dv));
	dv->in[0].u64 = v1;
	dv->in[1].u64 = v2;
	dv->in[0].u32 = (v1 << 12) + (v2 >> 6);
	dv->in[1].u32 = (v2 << 12) - (v1 >> 6);
}

static int
test_jump1_check(uint64_t rc, const void *arg)
{
	uint64_t r2, r3, r4, r5, rv;
	const struct dummy_vect8 *dvt;

	dvt = arg;

	rv = 0;
	r2 = dvt->in[0].u32;
	r3 = dvt->in[0].u64;
	r4 = dvt->in[1].u32;
	r5 = dvt->in[1].u64;

	if (r2 == TEST_JCC_1)
		rv |= 0x1;
	if ((int64_t)r3 <= TEST_JCC_2)
		rv |= 0x2;
	if (r4 > TEST_JCC_3)
		rv |= 0x4;
	if (r5 & TEST_JCC_4)
		rv |= 0x8;
	if (r2 != r3)
		rv |= 0x10;
	if ((int64_t)r2 > (int64_t)r4)
		rv |= 0x20;
	if (r2 <= r5)
		rv |= 0x40;
	if (r3 & r5)
		rv |= 0x80;

	return cmp_res(__func__, rv, rc, &rv, &rc, sizeof(rv));
}

/* alu (add, sub, and, or, xor, neg)  test-cases */
static const struct ebpf_insn test_alu1_prog[] = {

	{
		.code = (BPF_LDX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[0].u32),
	},
	{
		.code = (BPF_LDX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_3,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[0].u64),
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_4,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[1].u32),
	},
	{
		.code = (BPF_LDX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_5,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[1].u64),
	},
	{
		.code = (BPF_ALU | BPF_AND | BPF_K),
		.dst_reg = EBPF_REG_2,
		.imm = TEST_FILL_1,
	},
	{
		.code = (EBPF_ALU64 | BPF_OR | BPF_K),
		.dst_reg = EBPF_REG_3,
		.imm = TEST_FILL_1,
	},
	{
		.code = (BPF_ALU | BPF_XOR | BPF_K),
		.dst_reg = EBPF_REG_4,
		.imm = TEST_FILL_1,
	},
	{
		.code = (EBPF_ALU64 | BPF_ADD | BPF_K),
		.dst_reg = EBPF_REG_5,
		.imm = TEST_FILL_1,
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_2,
		.off = offsetof(struct dummy_vect8, out[0].u64),
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_3,
		.off = offsetof(struct dummy_vect8, out[1].u64),
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_4,
		.off = offsetof(struct dummy_vect8, out[2].u64),
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_5,
		.off = offsetof(struct dummy_vect8, out[3].u64),
	},
	{
		.code = (BPF_ALU | BPF_OR | BPF_X),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_3,
	},
	{
		.code = (EBPF_ALU64 | BPF_XOR | BPF_X),
		.dst_reg = EBPF_REG_3,
		.src_reg = EBPF_REG_4,
	},
	{
		.code = (BPF_ALU | BPF_SUB | BPF_X),
		.dst_reg = EBPF_REG_4,
		.src_reg = EBPF_REG_5,
	},
	{
		.code = (EBPF_ALU64 | BPF_AND | BPF_X),
		.dst_reg = EBPF_REG_5,
		.src_reg = EBPF_REG_2,
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_2,
		.off = offsetof(struct dummy_vect8, out[4].u64),
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_3,
		.off = offsetof(struct dummy_vect8, out[5].u64),
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_4,
		.off = offsetof(struct dummy_vect8, out[6].u64),
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_5,
		.off = offsetof(struct dummy_vect8, out[7].u64),
	},
	/* return (-r2 + (-r3)) */
	{
		.code = (BPF_ALU | BPF_NEG),
		.dst_reg = EBPF_REG_2,
	},
	{
		.code = (EBPF_ALU64 | BPF_NEG),
		.dst_reg = EBPF_REG_3,
	},
	{
		.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_3,
	},
	{
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_X),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_2,
	},
	{
		.code = (BPF_JMP | EBPF_EXIT),
	},
};

static int
test_alu1_check(uint64_t rc, const void *arg)
{
	uint64_t r2, r3, r4, r5, rv;
	const struct dummy_vect8 *dvt;
	struct dummy_vect8 dve;

	dvt = arg;
	memset(&dve, 0, sizeof(dve));

	r2 = dvt->in[0].u32;
	r3 = dvt->in[0].u64;
	r4 = dvt->in[1].u32;
	r5 = dvt->in[1].u64;

	r2 = (uint32_t)r2 & TEST_FILL_1;
	r3 |= (int32_t) TEST_FILL_1;
	r4 = (uint32_t)r4 ^ TEST_FILL_1;
	r5 += (int32_t)TEST_FILL_1;

	dve.out[0].u64 = r2;
	dve.out[1].u64 = r3;
	dve.out[2].u64 = r4;
	dve.out[3].u64 = r5;

	r2 = (uint32_t)r2 | (uint32_t)r3;
	r3 ^= r4;
	r4 = (uint32_t)r4 - (uint32_t)r5;
	r5 &= r2;

	dve.out[4].u64 = r2;
	dve.out[5].u64 = r3;
	dve.out[6].u64 = r4;
	dve.out[7].u64 = r5;

	r2 = -(int32_t)r2;
	rv = (uint32_t)r2;
	r3 = -r3;
	rv += r3;

	return cmp_res(__func__, rv, rc, dve.out, dvt->out, sizeof(dve.out));
}

/* endianness conversions (BE->LE/LE->BE)  test-cases */
static const struct ebpf_insn test_bele1_prog[] = {

	{
		.code = (BPF_LDX | BPF_MEM | BPF_H),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[0].u16),
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_3,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[0].u32),
	},
	{
		.code = (BPF_LDX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_4,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[0].u64),
	},
	{
		.code = (BPF_ALU | EBPF_END | EBPF_TO_BE),
		.dst_reg = EBPF_REG_2,
		.imm = sizeof(uint16_t) * CHAR_BIT,
	},
	{
		.code = (BPF_ALU | EBPF_END | EBPF_TO_BE),
		.dst_reg = EBPF_REG_3,
		.imm = sizeof(uint32_t) * CHAR_BIT,
	},
	{
		.code = (BPF_ALU | EBPF_END | EBPF_TO_BE),
		.dst_reg = EBPF_REG_4,
		.imm = sizeof(uint64_t) * CHAR_BIT,
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_2,
		.off = offsetof(struct dummy_vect8, out[0].u64),
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_3,
		.off = offsetof(struct dummy_vect8, out[1].u64),
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_4,
		.off = offsetof(struct dummy_vect8, out[2].u64),
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_H),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[0].u16),
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_3,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[0].u32),
	},
	{
		.code = (BPF_LDX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_4,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[0].u64),
	},
	{
		.code = (BPF_ALU | EBPF_END | EBPF_TO_LE),
		.dst_reg = EBPF_REG_2,
		.imm = sizeof(uint16_t) * CHAR_BIT,
	},
	{
		.code = (BPF_ALU | EBPF_END | EBPF_TO_LE),
		.dst_reg = EBPF_REG_3,
		.imm = sizeof(uint32_t) * CHAR_BIT,
	},
	{
		.code = (BPF_ALU | EBPF_END | EBPF_TO_LE),
		.dst_reg = EBPF_REG_4,
		.imm = sizeof(uint64_t) * CHAR_BIT,
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_2,
		.off = offsetof(struct dummy_vect8, out[3].u64),
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_3,
		.off = offsetof(struct dummy_vect8, out[4].u64),
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_4,
		.off = offsetof(struct dummy_vect8, out[5].u64),
	},
	/* return 1 */
	{
		.code = (BPF_ALU | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = 1,
	},
	{
		.code = (BPF_JMP | EBPF_EXIT),
	},
};

static void
test_bele1_prepare(void *arg)
{
	struct dummy_vect8 *dv;

	dv = arg;

	memset(dv, 0, sizeof(*dv));
	dv->in[0].u64 = rte_rand();
	dv->in[0].u32 = dv->in[0].u64;
	dv->in[0].u16 = dv->in[0].u64;
}

static int
test_bele1_check(uint64_t rc, const void *arg)
{
	uint64_t r2, r3, r4;
	const struct dummy_vect8 *dvt;
	struct dummy_vect8 dve;

	dvt = arg;
	memset(&dve, 0, sizeof(dve));

	r2 = dvt->in[0].u16;
	r3 = dvt->in[0].u32;
	r4 = dvt->in[0].u64;

	r2 =  rte_cpu_to_be_16(r2);
	r3 =  rte_cpu_to_be_32(r3);
	r4 =  rte_cpu_to_be_64(r4);

	dve.out[0].u64 = r2;
	dve.out[1].u64 = r3;
	dve.out[2].u64 = r4;

	r2 = dvt->in[0].u16;
	r3 = dvt->in[0].u32;
	r4 = dvt->in[0].u64;

	r2 =  rte_cpu_to_le_16(r2);
	r3 =  rte_cpu_to_le_32(r3);
	r4 =  rte_cpu_to_le_64(r4);

	dve.out[3].u64 = r2;
	dve.out[4].u64 = r3;
	dve.out[5].u64 = r4;

	return cmp_res(__func__, 1, rc, dve.out, dvt->out, sizeof(dve.out));
}

/* atomic add test-cases */
static const struct ebpf_insn test_xadd1_prog[] = {

	{
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_2,
		.imm = 1,
	},
	{
		.code = (BPF_STX | EBPF_XADD | BPF_W),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_2,
		.off = offsetof(struct dummy_offset, u32),
	},
	{
		.code = (BPF_STX | EBPF_XADD | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_2,
		.off = offsetof(struct dummy_offset, u64),
	},
	{
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_3,
		.imm = -1,
	},
	{
		.code = (BPF_STX | EBPF_XADD | BPF_W),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_3,
		.off = offsetof(struct dummy_offset, u32),
	},
	{
		.code = (BPF_STX | EBPF_XADD | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_3,
		.off = offsetof(struct dummy_offset, u64),
	},
	{
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_4,
		.imm = TEST_FILL_1,
	},
	{
		.code = (BPF_STX | EBPF_XADD | BPF_W),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_4,
		.off = offsetof(struct dummy_offset, u32),
	},
	{
		.code = (BPF_STX | EBPF_XADD | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_4,
		.off = offsetof(struct dummy_offset, u64),
	},
	{
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_5,
		.imm = TEST_MUL_1,
	},
	{
		.code = (BPF_STX | EBPF_XADD | BPF_W),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_5,
		.off = offsetof(struct dummy_offset, u32),
	},
	{
		.code = (BPF_STX | EBPF_XADD | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_5,
		.off = offsetof(struct dummy_offset, u64),
	},
	{
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_6,
		.imm = TEST_MUL_2,
	},
	{
		.code = (BPF_STX | EBPF_XADD | BPF_W),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_6,
		.off = offsetof(struct dummy_offset, u32),
	},
	{
		.code = (BPF_STX | EBPF_XADD | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_6,
		.off = offsetof(struct dummy_offset, u64),
	},
	{
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_7,
		.imm = TEST_JCC_2,
	},
	{
		.code = (BPF_STX | EBPF_XADD | BPF_W),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_7,
		.off = offsetof(struct dummy_offset, u32),
	},
	{
		.code = (BPF_STX | EBPF_XADD | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_7,
		.off = offsetof(struct dummy_offset, u64),
	},
	{
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_8,
		.imm = TEST_JCC_3,
	},
	{
		.code = (BPF_STX | EBPF_XADD | BPF_W),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_8,
		.off = offsetof(struct dummy_offset, u32),
	},
	{
		.code = (BPF_STX | EBPF_XADD | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_8,
		.off = offsetof(struct dummy_offset, u64),
	},
	/* return 1 */
	{
		.code = (BPF_ALU | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = 1,
	},
	{
		.code = (BPF_JMP | EBPF_EXIT),
	},
};

static int
test_xadd1_check(uint64_t rc, const void *arg)
{
	uint64_t rv;
	const struct dummy_offset *dft;
	struct dummy_offset dfe;

	dft = arg;
	memset(&dfe, 0, sizeof(dfe));

	rv = 1;
	rte_atomic32_add((rte_atomic32_t *)&dfe.u32, rv);
	rte_atomic64_add((rte_atomic64_t *)&dfe.u64, rv);

	rv = -1;
	rte_atomic32_add((rte_atomic32_t *)&dfe.u32, rv);
	rte_atomic64_add((rte_atomic64_t *)&dfe.u64, rv);

	rv = (int32_t)TEST_FILL_1;
	rte_atomic32_add((rte_atomic32_t *)&dfe.u32, rv);
	rte_atomic64_add((rte_atomic64_t *)&dfe.u64, rv);

	rv = TEST_MUL_1;
	rte_atomic32_add((rte_atomic32_t *)&dfe.u32, rv);
	rte_atomic64_add((rte_atomic64_t *)&dfe.u64, rv);

	rv = TEST_MUL_2;
	rte_atomic32_add((rte_atomic32_t *)&dfe.u32, rv);
	rte_atomic64_add((rte_atomic64_t *)&dfe.u64, rv);

	rv = TEST_JCC_2;
	rte_atomic32_add((rte_atomic32_t *)&dfe.u32, rv);
	rte_atomic64_add((rte_atomic64_t *)&dfe.u64, rv);

	rv = TEST_JCC_3;
	rte_atomic32_add((rte_atomic32_t *)&dfe.u32, rv);
	rte_atomic64_add((rte_atomic64_t *)&dfe.u64, rv);

	return cmp_res(__func__, 1, rc, &dfe, dft, sizeof(dfe));
}

/* alu div test-cases */
static const struct ebpf_insn test_div1_prog[] = {

	{
		.code = (BPF_LDX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[0].u32),
	},
	{
		.code = (BPF_LDX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_3,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[1].u64),
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_4,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[2].u32),
	},
	{
		.code = (BPF_ALU | BPF_DIV | BPF_K),
		.dst_reg = EBPF_REG_2,
		.imm = TEST_MUL_1,
	},
	{
		.code = (EBPF_ALU64 | BPF_MOD | BPF_K),
		.dst_reg = EBPF_REG_3,
		.imm = TEST_MUL_2,
	},
	{
		.code = (EBPF_ALU64 | BPF_OR | BPF_K),
		.dst_reg = EBPF_REG_2,
		.imm = 1,
	},
	{
		.code = (EBPF_ALU64 | BPF_OR | BPF_K),
		.dst_reg = EBPF_REG_3,
		.imm = 1,
	},
	{
		.code = (BPF_ALU | BPF_MOD | BPF_X),
		.dst_reg = EBPF_REG_4,
		.src_reg = EBPF_REG_2,
	},
	{
		.code = (EBPF_ALU64 | BPF_DIV | BPF_X),
		.dst_reg = EBPF_REG_4,
		.src_reg = EBPF_REG_3,
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_2,
		.off = offsetof(struct dummy_vect8, out[0].u64),
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_3,
		.off = offsetof(struct dummy_vect8, out[1].u64),
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_4,
		.off = offsetof(struct dummy_vect8, out[2].u64),
	},
	/* check that we can handle division by zero gracefully. */
	{
		.code = (BPF_LDX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_vect8, in[3].u32),
	},
	{
		.code = (BPF_ALU | BPF_DIV | BPF_X),
		.dst_reg = EBPF_REG_4,
		.src_reg = EBPF_REG_2,
	},
	/* return 1 */
	{
		.code = (BPF_ALU | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = 1,
	},
	{
		.code = (BPF_JMP | EBPF_EXIT),
	},
};

static int
test_div1_check(uint64_t rc, const void *arg)
{
	uint64_t r2, r3, r4;
	const struct dummy_vect8 *dvt;
	struct dummy_vect8 dve;

	dvt = arg;
	memset(&dve, 0, sizeof(dve));

	r2 = dvt->in[0].u32;
	r3 = dvt->in[1].u64;
	r4 = dvt->in[2].u32;

	r2 = (uint32_t)r2 / TEST_MUL_1;
	r3 %= TEST_MUL_2;
	r2 |= 1;
	r3 |= 1;
	r4 = (uint32_t)(r4 % r2);
	r4 /= r3;

	dve.out[0].u64 = r2;
	dve.out[1].u64 = r3;
	dve.out[2].u64 = r4;

	/*
	 * in the test prog we attempted to divide by zero.
	 * so return value should return 0.
	 */
	return cmp_res(__func__, 0, rc, dve.out, dvt->out, sizeof(dve.out));
}

/* call test-cases */
static const struct ebpf_insn test_call1_prog[] = {

	{
		.code = (BPF_LDX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_offset, u32),
	},
	{
		.code = (BPF_LDX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_3,
		.src_reg = EBPF_REG_1,
		.off = offsetof(struct dummy_offset, u64),
	},
	{
		.code = (BPF_STX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_10,
		.src_reg = EBPF_REG_2,
		.off = -4,
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_10,
		.src_reg = EBPF_REG_3,
		.off = -16,
	},
	{
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_X),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_10,
	},
	{
		.code = (EBPF_ALU64 | BPF_SUB | BPF_K),
		.dst_reg = EBPF_REG_2,
		.imm = 4,
	},
	{
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_X),
		.dst_reg = EBPF_REG_3,
		.src_reg = EBPF_REG_10,
	},
	{
		.code = (EBPF_ALU64 | BPF_SUB | BPF_K),
		.dst_reg = EBPF_REG_3,
		.imm = 16,
	},
	{
		.code = (BPF_JMP | EBPF_CALL),
		.imm = 0,
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_10,
		.off = -4,
	},
	{
		.code = (BPF_LDX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_10,
		.off = -16
	},
	{
		.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_2,
	},
	{
		.code = (BPF_JMP | EBPF_EXIT),
	},
};

static void
dummy_func1(const void *p, uint32_t *v32, uint64_t *v64)
{
	const struct dummy_offset *dv;

	dv = p;

	v32[0] += dv->u16;
	v64[0] += dv->u8;
}

static int
test_call1_check(uint64_t rc, const void *arg)
{
	uint32_t v32;
	uint64_t v64;
	const struct dummy_offset *dv;

	dv = arg;

	v32 = dv->u32;
	v64 = dv->u64;
	dummy_func1(arg, &v32, &v64);
	v64 += v32;

	if (v64 != rc) {
		printf("%s@%d: invalid return value "
			"expected=0x%" PRIx64 ", actual=0x%" PRIx64 "\n",
			__func__, __LINE__, v64, rc);
		return -1;
	}
	return 0;
	return cmp_res(__func__, v64, rc, dv, dv, sizeof(*dv));
}

static const struct rte_bpf_xsym test_call1_xsym[] = {
	{
		.name = RTE_STR(dummy_func1),
		.type = RTE_BPF_XTYPE_FUNC,
		.func = (void *)dummy_func1,
	},
};

static const struct bpf_test tests[] = {
	{
		.name = "test_store1",
		.arg_sz = sizeof(struct dummy_offset),
		.prm = {
			.ins = test_store1_prog,
			.nb_ins = RTE_DIM(test_store1_prog),
			.prog_arg = {
				.type = RTE_BPF_ARG_PTR,
				.size = sizeof(struct dummy_offset),
			},
		},
		.prepare = test_store1_prepare,
		.check_result = test_store1_check,
	},
	{
		.name = "test_store2",
		.arg_sz = sizeof(struct dummy_offset),
		.prm = {
			.ins = test_store2_prog,
			.nb_ins = RTE_DIM(test_store2_prog),
			.prog_arg = {
				.type = RTE_BPF_ARG_PTR,
				.size = sizeof(struct dummy_offset),
			},
		},
		.prepare = test_store1_prepare,
		.check_result = test_store1_check,
	},
	{
		.name = "test_load1",
		.arg_sz = sizeof(struct dummy_offset),
		.prm = {
			.ins = test_load1_prog,
			.nb_ins = RTE_DIM(test_load1_prog),
			.prog_arg = {
				.type = RTE_BPF_ARG_PTR,
				.size = sizeof(struct dummy_offset),
			},
		},
		.prepare = test_load1_prepare,
		.check_result = test_load1_check,
	},
	{
		.name = "test_mul1",
		.arg_sz = sizeof(struct dummy_vect8),
		.prm = {
			.ins = test_mul1_prog,
			.nb_ins = RTE_DIM(test_mul1_prog),
			.prog_arg = {
				.type = RTE_BPF_ARG_PTR,
				.size = sizeof(struct dummy_vect8),
			},
		},
		.prepare = test_mul1_prepare,
		.check_result = test_mul1_check,
	},
	{
		.name = "test_shift1",
		.arg_sz = sizeof(struct dummy_vect8),
		.prm = {
			.ins = test_shift1_prog,
			.nb_ins = RTE_DIM(test_shift1_prog),
			.prog_arg = {
				.type = RTE_BPF_ARG_PTR,
				.size = sizeof(struct dummy_vect8),
			},
		},
		.prepare = test_shift1_prepare,
		.check_result = test_shift1_check,
	},
	{
		.name = "test_jump1",
		.arg_sz = sizeof(struct dummy_vect8),
		.prm = {
			.ins = test_jump1_prog,
			.nb_ins = RTE_DIM(test_jump1_prog),
			.prog_arg = {
				.type = RTE_BPF_ARG_PTR,
				.size = sizeof(struct dummy_vect8),
			},
		},
		.prepare = test_jump1_prepare,
		.check_result = test_jump1_check,
	},
	{
		.name = "test_alu1",
		.arg_sz = sizeof(struct dummy_vect8),
		.prm = {
			.ins = test_alu1_prog,
			.nb_ins = RTE_DIM(test_alu1_prog),
			.prog_arg = {
				.type = RTE_BPF_ARG_PTR,
				.size = sizeof(struct dummy_vect8),
			},
		},
		.prepare = test_jump1_prepare,
		.check_result = test_alu1_check,
	},
	{
		.name = "test_bele1",
		.arg_sz = sizeof(struct dummy_vect8),
		.prm = {
			.ins = test_bele1_prog,
			.nb_ins = RTE_DIM(test_bele1_prog),
			.prog_arg = {
				.type = RTE_BPF_ARG_PTR,
				.size = sizeof(struct dummy_vect8),
			},
		},
		.prepare = test_bele1_prepare,
		.check_result = test_bele1_check,
	},
	{
		.name = "test_xadd1",
		.arg_sz = sizeof(struct dummy_offset),
		.prm = {
			.ins = test_xadd1_prog,
			.nb_ins = RTE_DIM(test_xadd1_prog),
			.prog_arg = {
				.type = RTE_BPF_ARG_PTR,
				.size = sizeof(struct dummy_offset),
			},
		},
		.prepare = test_store1_prepare,
		.check_result = test_xadd1_check,
	},
	{
		.name = "test_div1",
		.arg_sz = sizeof(struct dummy_vect8),
		.prm = {
			.ins = test_div1_prog,
			.nb_ins = RTE_DIM(test_div1_prog),
			.prog_arg = {
				.type = RTE_BPF_ARG_PTR,
				.size = sizeof(struct dummy_vect8),
			},
		},
		.prepare = test_mul1_prepare,
		.check_result = test_div1_check,
	},
	{
		.name = "test_call1",
		.arg_sz = sizeof(struct dummy_offset),
		.prm = {
			.ins = test_call1_prog,
			.nb_ins = RTE_DIM(test_call1_prog),
			.prog_arg = {
				.type = RTE_BPF_ARG_PTR,
				.size = sizeof(struct dummy_offset),
			},
			.xsym = test_call1_xsym,
			.nb_xsym = RTE_DIM(test_call1_xsym),
		},
		.prepare = test_load1_prepare,
		.check_result = test_call1_check,
		/* for now don't support function calls on 32 bit platform */
		.allow_fail = (sizeof(uint64_t) != sizeof(uintptr_t)),
	},
};

static int
run_test(const struct bpf_test *tst)
{
	int32_t ret, rv;
	int64_t rc;
	struct rte_bpf *bpf;
	struct rte_bpf_jit jit;
	uint8_t tbuf[tst->arg_sz];

	printf("%s(%s) start\n", __func__, tst->name);

	bpf = rte_bpf_load(&tst->prm);
	if (bpf == NULL) {
		printf("%s@%d: failed to load bpf code, error=%d(%s);\n",
			__func__, __LINE__, rte_errno, strerror(rte_errno));
		return -1;
	}

	tst->prepare(tbuf);

	rc = rte_bpf_exec(bpf, tbuf);
	ret = tst->check_result(rc, tbuf);
	if (ret != 0) {
		printf("%s@%d: check_result(%s) failed, error: %d(%s);\n",
			__func__, __LINE__, tst->name, ret, strerror(ret));
	}

	rte_bpf_get_jit(bpf, &jit);
	if (jit.func == NULL)
		return 0;

	tst->prepare(tbuf);
	rc = jit.func(tbuf);
	rv = tst->check_result(rc, tbuf);
	ret |= rv;
	if (rv != 0) {
		printf("%s@%d: check_result(%s) failed, error: %d(%s);\n",
			__func__, __LINE__, tst->name, rv, strerror(ret));
	}

	rte_bpf_destroy(bpf);
	return ret;

}

static int
test_bpf(void)
{
	int32_t rc, rv;
	uint32_t i;

	rc = 0;
	for (i = 0; i != RTE_DIM(tests); i++) {
		rv = run_test(tests + i);
		if (tests[i].allow_fail == 0)
			rc |= rv;
	}

	return rc;
}

REGISTER_TEST_COMMAND(bpf_autotest, test_bpf);
