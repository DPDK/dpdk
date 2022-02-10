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
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_byteorder.h>
#include <rte_errno.h>
#include "test.h"

#if !defined(RTE_LIB_BPF)

static int
test_bpf(void)
{
	printf("BPF not supported, skipping test\n");
	return TEST_SKIPPED;
}

#else

#include <rte_bpf.h>
#include <rte_ether.h>
#include <rte_ip.h>


/*
 * Basic functional tests for librte_bpf.
 * The main procedure - load eBPF program, execute it and
 * compare results with expected values.
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

struct dummy_net {
	struct rte_ether_hdr eth_hdr;
	struct rte_vlan_hdr vlan_hdr;
	struct rte_ipv4_hdr ip_hdr;
};

#define	DUMMY_MBUF_NUM	2

/* first mbuf in the packet, should always be at offset 0 */
struct dummy_mbuf {
	struct rte_mbuf mb[DUMMY_MBUF_NUM];
	uint8_t buf[DUMMY_MBUF_NUM][RTE_MBUF_DEFAULT_BUF_SIZE];
};

#define	TEST_FILL_1	0xDEADBEEF

#define	TEST_MUL_1	21
#define TEST_MUL_2	-100

#define TEST_SHIFT_1	15
#define TEST_SHIFT_2	33

#define TEST_SHIFT32_MASK	(CHAR_BIT * sizeof(uint32_t) - 1)
#define TEST_SHIFT64_MASK	(CHAR_BIT * sizeof(uint64_t) - 1)

#define TEST_JCC_1	0
#define TEST_JCC_2	-123
#define TEST_JCC_3	5678
#define TEST_JCC_4	TEST_FILL_1

#define TEST_IMM_1	UINT64_MAX
#define TEST_IMM_2	((uint64_t)INT64_MIN)
#define TEST_IMM_3	((uint64_t)INT64_MAX + INT32_MAX)
#define TEST_IMM_4	((uint64_t)UINT32_MAX)
#define TEST_IMM_5	((uint64_t)UINT32_MAX + 1)

#define TEST_MEMFROB	0x2a2a2a2a

#define STRING_GEEK	0x6B656567
#define STRING_WEEK	0x6B656577

#define TEST_NETMASK 0xffffff00
#define TEST_SUBNET  0xaca80200

uint8_t src_mac[] = { 0x00, 0xFF, 0xAA, 0xFF, 0xAA, 0xFF };
uint8_t dst_mac[] = { 0x00, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA };

uint32_t ip_src_addr = (172U << 24) | (168U << 16) | (2 << 8) | 1;
uint32_t ip_dst_addr = (172U << 24) | (168U << 16) | (2 << 8) | 2;

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

/* load immediate test-cases */
static const struct ebpf_insn test_ldimm1_prog[] = {

	{
		.code = (BPF_LD | BPF_IMM | EBPF_DW),
		.dst_reg = EBPF_REG_0,
		.imm = (uint32_t)TEST_IMM_1,
	},
	{
		.imm = TEST_IMM_1 >> 32,
	},
	{
		.code = (BPF_LD | BPF_IMM | EBPF_DW),
		.dst_reg = EBPF_REG_3,
		.imm = (uint32_t)TEST_IMM_2,
	},
	{
		.imm = TEST_IMM_2 >> 32,
	},
	{
		.code = (BPF_LD | BPF_IMM | EBPF_DW),
		.dst_reg = EBPF_REG_5,
		.imm = (uint32_t)TEST_IMM_3,
	},
	{
		.imm = TEST_IMM_3 >> 32,
	},
	{
		.code = (BPF_LD | BPF_IMM | EBPF_DW),
		.dst_reg = EBPF_REG_7,
		.imm = (uint32_t)TEST_IMM_4,
	},
	{
		.imm = TEST_IMM_4 >> 32,
	},
	{
		.code = (BPF_LD | BPF_IMM | EBPF_DW),
		.dst_reg = EBPF_REG_9,
		.imm = (uint32_t)TEST_IMM_5,
	},
	{
		.imm = TEST_IMM_5 >> 32,
	},
	/* return sum */
	{
		.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_3,
	},
	{
		.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_5,
	},
	{
		.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_7,
	},
	{
		.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_9,
	},
	{
		.code = (BPF_JMP | EBPF_EXIT),
	},
};

static int
test_ldimm1_check(uint64_t rc, const void *arg)
{
	uint64_t v1, v2;

	v1 = TEST_IMM_1;
	v2 = TEST_IMM_2;
	v1 += v2;
	v2 = TEST_IMM_3;
	v1 += v2;
	v2 = TEST_IMM_4;
	v1 += v2;
	v2 = TEST_IMM_5;
	v1 += v2;

	return cmp_res(__func__, v1, rc, arg, arg, 0);
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
		.code = (BPF_ALU | BPF_AND | BPF_K),
		.dst_reg = EBPF_REG_4,
		.imm = TEST_SHIFT64_MASK,
	},
	{
		.code = (EBPF_ALU64 | BPF_LSH | BPF_X),
		.dst_reg = EBPF_REG_3,
		.src_reg = EBPF_REG_4,
	},
	{
		.code = (BPF_ALU | BPF_AND | BPF_K),
		.dst_reg = EBPF_REG_4,
		.imm = TEST_SHIFT32_MASK,
	},
	{
		.code = (BPF_ALU | BPF_RSH | BPF_X),
		.dst_reg = EBPF_REG_2,
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
		.imm = TEST_SHIFT64_MASK,
	},
	{
		.code = (EBPF_ALU64 | EBPF_ARSH | BPF_X),
		.dst_reg = EBPF_REG_3,
		.src_reg = EBPF_REG_2,
	},
	{
		.code = (BPF_ALU | BPF_AND | BPF_K),
		.dst_reg = EBPF_REG_2,
		.imm = TEST_SHIFT32_MASK,
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

	r4 &= TEST_SHIFT64_MASK;
	r3 <<= r4;
	r4 &= TEST_SHIFT32_MASK;
	r2 = (uint32_t)r2 >> r4;

	dve.out[2].u64 = r2;
	dve.out[3].u64 = r3;

	r2 = dvt->in[0].u32;
	r3 = dvt->in[1].u64;
	r4 = dvt->in[2].u32;

	r2 &= TEST_SHIFT64_MASK;
	r3 = (int64_t)r3 >> r2;
	r2 &= TEST_SHIFT32_MASK;
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

/* Jump test case - check ip4_dest in particular subnet */
static const struct ebpf_insn test_jump2_prog[] = {

	[0] = {
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_2,
		.imm = 0xe,
	},
	[1] = {
		.code = (BPF_LDX | BPF_MEM | BPF_H),
		.dst_reg = EBPF_REG_3,
		.src_reg = EBPF_REG_1,
		.off = 12,
	},
	[2] = {
		.code = (BPF_JMP | EBPF_JNE | BPF_K),
		.dst_reg = EBPF_REG_3,
		.off = 2,
		.imm = 0x81,
	},
	[3] = {
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_2,
		.imm = 0x12,
	},
	[4] = {
		.code = (BPF_LDX | BPF_MEM | BPF_H),
		.dst_reg = EBPF_REG_3,
		.src_reg = EBPF_REG_1,
		.off = 16,
	},
	[5] = {
		.code = (EBPF_ALU64 | BPF_AND | BPF_K),
		.dst_reg = EBPF_REG_3,
		.imm = 0xffff,
	},
	[6] = {
		.code = (BPF_JMP | EBPF_JNE | BPF_K),
		.dst_reg = EBPF_REG_3,
		.off = 9,
		.imm = 0x8,
	},
	[7] = {
		.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_2,
	},
	[8] = {
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = 0,
	},
	[9] = {
		.code = (BPF_LDX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_1,
		.off = 16,
	},
	[10] = {
		.code = (BPF_ALU | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_3,
		.imm = TEST_NETMASK,
	},
	[11] = {
		.code = (BPF_ALU | EBPF_END | EBPF_TO_BE),
		.dst_reg = EBPF_REG_3,
		.imm = sizeof(uint32_t) * CHAR_BIT,
	},
	[12] = {
		.code = (BPF_ALU | BPF_AND | BPF_X),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_3,
	},
	[13] = {
		.code = (BPF_ALU | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_3,
		.imm = TEST_SUBNET,
	},
	[14] = {
		.code = (BPF_ALU | EBPF_END | EBPF_TO_BE),
		.dst_reg = EBPF_REG_3,
		.imm = sizeof(uint32_t) * CHAR_BIT,
	},
	[15] = {
		.code = (BPF_JMP | BPF_JEQ | BPF_X),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_3,
		.off = 1,
	},
	[16] = {
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = -1,
	},
	[17] = {
		.code = (BPF_JMP | EBPF_EXIT),
	},
};

/* Preparing a vlan packet */
static void
test_jump2_prepare(void *arg)
{
	struct dummy_net *dn;

	dn = arg;
	memset(dn, 0, sizeof(*dn));

	/*
	 * Initialize ether header.
	 */
	rte_ether_addr_copy((struct rte_ether_addr *)dst_mac,
			    &dn->eth_hdr.dst_addr);
	rte_ether_addr_copy((struct rte_ether_addr *)src_mac,
			    &dn->eth_hdr.src_addr);
	dn->eth_hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);

	/*
	 * Initialize vlan header.
	 */
	dn->vlan_hdr.eth_proto =  rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
	dn->vlan_hdr.vlan_tci = 32;

	/*
	 * Initialize IP header.
	 */
	dn->ip_hdr.version_ihl   = 0x45;    /*IP_VERSION | IP_HDRLEN*/
	dn->ip_hdr.time_to_live   = 64;   /* IP_DEFTTL */
	dn->ip_hdr.next_proto_id = IPPROTO_TCP;
	dn->ip_hdr.packet_id = rte_cpu_to_be_16(0x463c);
	dn->ip_hdr.total_length   = rte_cpu_to_be_16(60);
	dn->ip_hdr.src_addr = rte_cpu_to_be_32(ip_src_addr);
	dn->ip_hdr.dst_addr = rte_cpu_to_be_32(ip_dst_addr);
}

static int
test_jump2_check(uint64_t rc, const void *arg)
{
	const struct rte_ether_hdr *eth_hdr = arg;
	const struct rte_ipv4_hdr *ipv4_hdr;
	const void *next = eth_hdr;
	uint16_t eth_type;
	uint64_t v = -1;

	if (eth_hdr->ether_type == htons(0x8100)) {
		const struct rte_vlan_hdr *vlan_hdr =
			(const void *)(eth_hdr + 1);
		eth_type = vlan_hdr->eth_proto;
		next = vlan_hdr + 1;
	} else {
		eth_type = eth_hdr->ether_type;
		next = eth_hdr + 1;
	}

	if (eth_type == htons(0x0800)) {
		ipv4_hdr = next;
		if ((ipv4_hdr->dst_addr & rte_cpu_to_be_32(TEST_NETMASK)) ==
		    rte_cpu_to_be_32(TEST_SUBNET)) {
			v = 0;
		}
	}

	return cmp_res(__func__, v, rc, arg, arg, sizeof(arg));
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
	__atomic_fetch_add(&dfe.u32, rv, __ATOMIC_RELAXED);
	__atomic_fetch_add(&dfe.u64, rv, __ATOMIC_RELAXED);

	rv = -1;
	__atomic_fetch_add(&dfe.u32, rv, __ATOMIC_RELAXED);
	__atomic_fetch_add(&dfe.u64, rv, __ATOMIC_RELAXED);

	rv = (int32_t)TEST_FILL_1;
	__atomic_fetch_add(&dfe.u32, rv, __ATOMIC_RELAXED);
	__atomic_fetch_add(&dfe.u64, rv, __ATOMIC_RELAXED);

	rv = TEST_MUL_1;
	__atomic_fetch_add(&dfe.u32, rv, __ATOMIC_RELAXED);
	__atomic_fetch_add(&dfe.u64, rv, __ATOMIC_RELAXED);

	rv = TEST_MUL_2;
	__atomic_fetch_add(&dfe.u32, rv, __ATOMIC_RELAXED);
	__atomic_fetch_add(&dfe.u64, rv, __ATOMIC_RELAXED);

	rv = TEST_JCC_2;
	__atomic_fetch_add(&dfe.u32, rv, __ATOMIC_RELAXED);
	__atomic_fetch_add(&dfe.u64, rv, __ATOMIC_RELAXED);

	rv = TEST_JCC_3;
	__atomic_fetch_add(&dfe.u32, rv, __ATOMIC_RELAXED);
	__atomic_fetch_add(&dfe.u64, rv, __ATOMIC_RELAXED);

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

	return cmp_res(__func__, v64, rc, dv, dv, sizeof(*dv));
}

static const struct rte_bpf_xsym test_call1_xsym[] = {
	{
		.name = RTE_STR(dummy_func1),
		.type = RTE_BPF_XTYPE_FUNC,
		.func = {
			.val = (void *)dummy_func1,
			.nb_args = 3,
			.args = {
				[0] = {
					.type = RTE_BPF_ARG_PTR,
					.size = sizeof(struct dummy_offset),
				},
				[1] = {
					.type = RTE_BPF_ARG_PTR,
					.size = sizeof(uint32_t),
				},
				[2] = {
					.type = RTE_BPF_ARG_PTR,
					.size = sizeof(uint64_t),
				},
			},
		},
	},
};

static const struct ebpf_insn test_call2_prog[] = {

	{
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_X),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_10,
	},
	{
		.code = (EBPF_ALU64 | BPF_ADD | BPF_K),
		.dst_reg = EBPF_REG_1,
		.imm = -(int32_t)sizeof(struct dummy_offset),
	},
	{
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_X),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_10,
	},
	{
		.code = (EBPF_ALU64 | BPF_ADD | BPF_K),
		.dst_reg = EBPF_REG_2,
		.imm = -2 * (int32_t)sizeof(struct dummy_offset),
	},
	{
		.code = (BPF_JMP | EBPF_CALL),
		.imm = 0,
	},
	{
		.code = (BPF_LDX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_10,
		.off = -(int32_t)(sizeof(struct dummy_offset) -
			offsetof(struct dummy_offset, u64)),
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_10,
		.off = -(int32_t)(sizeof(struct dummy_offset) -
			offsetof(struct dummy_offset, u32)),
	},
	{
		.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_1,
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_H),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_10,
		.off = -(int32_t)(2 * sizeof(struct dummy_offset) -
			offsetof(struct dummy_offset, u16)),
	},
	{
		.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_1,
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_B),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_10,
		.off = -(int32_t)(2 * sizeof(struct dummy_offset) -
			offsetof(struct dummy_offset, u8)),
	},
	{
		.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_1,
	},
	{
		.code = (BPF_JMP | EBPF_EXIT),
	},

};

static void
dummy_func2(struct dummy_offset *a, struct dummy_offset *b)
{
	uint64_t v;

	v = 0;
	a->u64 = v++;
	a->u32 = v++;
	a->u16 = v++;
	a->u8 = v++;
	b->u64 = v++;
	b->u32 = v++;
	b->u16 = v++;
	b->u8 = v++;
}

static int
test_call2_check(uint64_t rc, const void *arg)
{
	uint64_t v;
	struct dummy_offset a, b;

	RTE_SET_USED(arg);

	dummy_func2(&a, &b);
	v = a.u64 + a.u32 + b.u16 + b.u8;

	return cmp_res(__func__, v, rc, arg, arg, 0);
}

static const struct rte_bpf_xsym test_call2_xsym[] = {
	{
		.name = RTE_STR(dummy_func2),
		.type = RTE_BPF_XTYPE_FUNC,
		.func = {
			.val = (void *)dummy_func2,
			.nb_args = 2,
			.args = {
				[0] = {
					.type = RTE_BPF_ARG_PTR,
					.size = sizeof(struct dummy_offset),
				},
				[1] = {
					.type = RTE_BPF_ARG_PTR,
					.size = sizeof(struct dummy_offset),
				},
			},
		},
	},
};

static const struct ebpf_insn test_call3_prog[] = {

	{
		.code = (BPF_JMP | EBPF_CALL),
		.imm = 0,
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_B),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_0,
		.off = offsetof(struct dummy_offset, u8),
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_H),
		.dst_reg = EBPF_REG_3,
		.src_reg = EBPF_REG_0,
		.off = offsetof(struct dummy_offset, u16),
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_4,
		.src_reg = EBPF_REG_0,
		.off = offsetof(struct dummy_offset, u32),
	},
	{
		.code = (BPF_LDX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_0,
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

static const struct dummy_offset *
dummy_func3(const struct dummy_vect8 *p)
{
	return &p->in[RTE_DIM(p->in) - 1];
}

static void
test_call3_prepare(void *arg)
{
	struct dummy_vect8 *pv;
	struct dummy_offset *df;

	pv = arg;
	df = (struct dummy_offset *)(uintptr_t)dummy_func3(pv);

	memset(pv, 0, sizeof(*pv));
	df->u64 = (int32_t)TEST_FILL_1;
	df->u32 = df->u64;
	df->u16 = df->u64;
	df->u8 = df->u64;
}

static int
test_call3_check(uint64_t rc, const void *arg)
{
	uint64_t v;
	const struct dummy_vect8 *pv;
	const struct dummy_offset *dft;

	pv = arg;
	dft = dummy_func3(pv);

	v = dft->u64;
	v += dft->u32;
	v += dft->u16;
	v += dft->u8;

	return cmp_res(__func__, v, rc, pv, pv, sizeof(*pv));
}

static const struct rte_bpf_xsym test_call3_xsym[] = {
	{
		.name = RTE_STR(dummy_func3),
		.type = RTE_BPF_XTYPE_FUNC,
		.func = {
			.val = (void *)dummy_func3,
			.nb_args = 1,
			.args = {
				[0] = {
					.type = RTE_BPF_ARG_PTR,
					.size = sizeof(struct dummy_vect8),
				},
			},
			.ret = {
				.type = RTE_BPF_ARG_PTR,
				.size = sizeof(struct dummy_offset),
			},
		},
	},
};

/* Test for stack corruption in multiple function calls */
static const struct ebpf_insn test_call4_prog[] = {
	{
		.code = (BPF_ST | BPF_MEM | BPF_B),
		.dst_reg = EBPF_REG_10,
		.off = -4,
		.imm = 1,
	},
	{
		.code = (BPF_ST | BPF_MEM | BPF_B),
		.dst_reg = EBPF_REG_10,
		.off = -3,
		.imm = 2,
	},
	{
		.code = (BPF_ST | BPF_MEM | BPF_B),
		.dst_reg = EBPF_REG_10,
		.off = -2,
		.imm = 3,
	},
	{
		.code = (BPF_ST | BPF_MEM | BPF_B),
		.dst_reg = EBPF_REG_10,
		.off = -1,
		.imm = 4,
	},
	{
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_X),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_10,
	},
	{
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_2,
		.imm = 4,
	},
	{
		.code = (EBPF_ALU64 | BPF_SUB | BPF_X),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_2,
	},
	{
		.code = (BPF_JMP | EBPF_CALL),
		.imm = 0,
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_B),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_10,
		.off = -4,
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_B),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_10,
		.off = -3,
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_B),
		.dst_reg = EBPF_REG_3,
		.src_reg = EBPF_REG_10,
		.off = -2,
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_B),
		.dst_reg = EBPF_REG_4,
		.src_reg = EBPF_REG_10,
		.off = -1,
	},
	{
		.code = (BPF_JMP | EBPF_CALL),
		.imm = 1,
	},
	{
		.code = (EBPF_ALU64 | BPF_XOR | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = TEST_MEMFROB,
	},
	{
		.code = (BPF_JMP | EBPF_EXIT),
	},
};

/* Gathering the bytes together */
static uint32_t
dummy_func4_1(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
	return (a << 24) | (b << 16) | (c << 8) | (d << 0);
}

/* Implementation of memfrob */
static uint32_t
dummy_func4_0(uint32_t *s, uint8_t n)
{
	char *p = (char *) s;
	while (n-- > 0)
		*p++ ^= 42;
	return *s;
}


static int
test_call4_check(uint64_t rc, const void *arg)
{
	uint8_t a[4] = {1, 2, 3, 4};
	uint32_t s, v = 0;

	RTE_SET_USED(arg);

	s = dummy_func4_0((uint32_t *)a, 4);

	s = dummy_func4_1(a[0], a[1], a[2], a[3]);

	v = s ^ TEST_MEMFROB;

	return cmp_res(__func__, v, rc, &v, &rc, sizeof(v));
}

static const struct rte_bpf_xsym test_call4_xsym[] = {
	[0] = {
		.name = RTE_STR(dummy_func4_0),
		.type = RTE_BPF_XTYPE_FUNC,
		.func = {
			.val = (void *)dummy_func4_0,
			.nb_args = 2,
			.args = {
				[0] = {
					.type = RTE_BPF_ARG_PTR,
					.size = 4 * sizeof(uint8_t),
				},
				[1] = {
					.type = RTE_BPF_ARG_RAW,
					.size = sizeof(uint8_t),
				},
			},
			.ret = {
				.type = RTE_BPF_ARG_RAW,
				.size = sizeof(uint32_t),
			},
		},
	},
	[1] = {
		.name = RTE_STR(dummy_func4_1),
		.type = RTE_BPF_XTYPE_FUNC,
		.func = {
			.val = (void *)dummy_func4_1,
			.nb_args = 4,
			.args = {
				[0] = {
					.type = RTE_BPF_ARG_RAW,
					.size = sizeof(uint8_t),
				},
				[1] = {
					.type = RTE_BPF_ARG_RAW,
					.size = sizeof(uint8_t),
				},
				[2] = {
					.type = RTE_BPF_ARG_RAW,
					.size = sizeof(uint8_t),
				},
				[3] = {
					.type = RTE_BPF_ARG_RAW,
					.size = sizeof(uint8_t),
				},
			},
			.ret = {
				.type = RTE_BPF_ARG_RAW,
				.size = sizeof(uint32_t),
			},
		},
	},
};

/* string compare test case */
static const struct ebpf_insn test_call5_prog[] = {

	[0] = {
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_1,
		.imm = STRING_GEEK,
	},
	[1] = {
		.code = (BPF_STX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_10,
		.src_reg = EBPF_REG_1,
		.off = -8,
	},
	[2] = {
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_6,
		.imm = 0,
	},
	[3] = {
		.code = (BPF_STX | BPF_MEM | BPF_B),
		.dst_reg = EBPF_REG_10,
		.src_reg = EBPF_REG_6,
		.off = -4,
	},
	[4] = {
		.code = (BPF_STX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_10,
		.src_reg = EBPF_REG_6,
		.off = -12,
	},
	[5] = {
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_1,
		.imm = STRING_WEEK,
	},
	[6] = {
		.code = (BPF_STX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_10,
		.src_reg = EBPF_REG_1,
		.off = -16,
	},
	[7] = {
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_X),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_10,
	},
	[8] = {
		.code = (EBPF_ALU64 | BPF_ADD | BPF_K),
		.dst_reg = EBPF_REG_1,
		.imm = -8,
	},
	[9] = {
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_X),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_1,
	},
	[10] = {
		.code = (BPF_JMP | EBPF_CALL),
		.imm = 0,
	},
	[11] = {
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_X),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_0,
	},
	[12] = {
		.code = (BPF_ALU | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = -1,
	},
	[13] = {
		.code = (EBPF_ALU64 | BPF_LSH | BPF_K),
		.dst_reg = EBPF_REG_1,
		.imm = 0x20,
	},
	[14] = {
		.code = (EBPF_ALU64 | BPF_RSH | BPF_K),
		.dst_reg = EBPF_REG_1,
		.imm = 0x20,
	},
	[15] = {
		.code = (BPF_JMP | EBPF_JNE | BPF_K),
		.dst_reg = EBPF_REG_1,
		.off = 11,
		.imm = 0,
	},
	[16] = {
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_X),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_10,
	},
	[17] = {
		.code = (EBPF_ALU64 | BPF_ADD | BPF_K),
		.dst_reg = EBPF_REG_1,
		.imm = -8,
	},
	[18] = {
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_X),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_10,
	},
	[19] = {
		.code = (EBPF_ALU64 | BPF_ADD | BPF_K),
		.dst_reg = EBPF_REG_2,
		.imm = -16,
	},
	[20] = {
		.code = (BPF_JMP | EBPF_CALL),
		.imm = 0,
	},
	[21] = {
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_X),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_0,
	},
	[22] = {
		.code = (EBPF_ALU64 | BPF_LSH | BPF_K),
		.dst_reg = EBPF_REG_1,
		.imm = 0x20,
	},
	[23] = {
		.code = (EBPF_ALU64 | BPF_RSH | BPF_K),
		.dst_reg = EBPF_REG_1,
		.imm = 0x20,
	},
	[24] = {
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_X),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_1,
	},
	[25] = {
		.code = (BPF_JMP | BPF_JEQ | BPF_X),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_6,
		.off = 1,
	},
	[26] = {
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = 0,
	},
	[27] = {
		.code = (BPF_JMP | EBPF_EXIT),
	},
};

/* String comparison implementation, return 0 if equal else difference */
static uint32_t
dummy_func5(const char *s1, const char *s2)
{
	while (*s1 && (*s1 == *s2)) {
		s1++;
		s2++;
	}
	return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

static int
test_call5_check(uint64_t rc, const void *arg)
{
	char a[] = "geek";
	char b[] = "week";
	uint32_t v;

	RTE_SET_USED(arg);

	v = dummy_func5(a, a);
	if (v != 0) {
		v = -1;
		goto fail;
	}

	v = dummy_func5(a, b);
	if (v == 0)
		goto fail;

	v = 0;

fail:
	return cmp_res(__func__, v, rc, &v, &rc, sizeof(v));
}

static const struct rte_bpf_xsym test_call5_xsym[] = {
	[0] = {
		.name = RTE_STR(dummy_func5),
		.type = RTE_BPF_XTYPE_FUNC,
		.func = {
			.val = (void *)dummy_func5,
			.nb_args = 2,
			.args = {
				[0] = {
					.type = RTE_BPF_ARG_PTR,
					.size = sizeof(char),
				},
				[1] = {
					.type = RTE_BPF_ARG_PTR,
					.size = sizeof(char),
				},
			},
			.ret = {
				.type = RTE_BPF_ARG_RAW,
				.size = sizeof(uint32_t),
			},
		},
	},
};

/* load mbuf (BPF_ABS/BPF_IND) test-cases */
static const struct ebpf_insn test_ld_mbuf1_prog[] = {

	/* BPF_ABS/BPF_IND implicitly expect mbuf ptr in R6 */
	{
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_X),
		.dst_reg = EBPF_REG_6,
		.src_reg = EBPF_REG_1,
	},
	/* load IPv4 version and IHL */
	{
		.code = (BPF_LD | BPF_ABS | BPF_B),
		.imm = offsetof(struct rte_ipv4_hdr, version_ihl),
	},
	/* check IP version */
	{
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_X),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_0,
	},
	{
		.code = (BPF_ALU | BPF_AND | BPF_K),
		.dst_reg = EBPF_REG_2,
		.imm = 0xf0,
	},
	{
		.code = (BPF_JMP | BPF_JEQ | BPF_K),
		.dst_reg = EBPF_REG_2,
		.imm = IPVERSION << 4,
		.off = 2,
	},
	/* invalid IP version, return 0 */
	{
		.code = (EBPF_ALU64 | BPF_XOR | BPF_X),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_0,
	},
	{
		.code = (BPF_JMP | EBPF_EXIT),
	},
	/* load 3-rd byte of IP data */
	{
		.code = (BPF_ALU | BPF_AND | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = RTE_IPV4_HDR_IHL_MASK,
	},
	{
		.code = (BPF_ALU | BPF_LSH | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = 2,
	},
	{
		.code = (BPF_LD | BPF_IND | BPF_B),
		.src_reg = EBPF_REG_0,
		.imm = 3,
	},
	{
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_X),
		.dst_reg = EBPF_REG_7,
		.src_reg = EBPF_REG_0,
	},
	/* load IPv4 src addr */
	{
		.code = (BPF_LD | BPF_ABS | BPF_W),
		.imm = offsetof(struct rte_ipv4_hdr, src_addr),
	},
	{
		.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		.dst_reg = EBPF_REG_7,
		.src_reg = EBPF_REG_0,
	},
	/* load IPv4 total length */
	{
		.code = (BPF_LD | BPF_ABS | BPF_H),
		.imm = offsetof(struct rte_ipv4_hdr, total_length),
	},
	{
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_X),
		.dst_reg = EBPF_REG_8,
		.src_reg = EBPF_REG_0,
	},
	/* load last 4 bytes of IP data */
	{
		.code = (BPF_LD | BPF_IND | BPF_W),
		.src_reg = EBPF_REG_8,
		.imm = -(int32_t)sizeof(uint32_t),
	},
	{
		.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		.dst_reg = EBPF_REG_7,
		.src_reg = EBPF_REG_0,
	},
	/* load 2 bytes from the middle of IP data */
	{
		.code = (EBPF_ALU64 | BPF_RSH | BPF_K),
		.dst_reg = EBPF_REG_8,
		.imm = 1,
	},
	{
		.code = (BPF_LD | BPF_IND | BPF_H),
		.src_reg = EBPF_REG_8,
	},
	{
		.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_7,
	},
	{
		.code = (BPF_JMP | EBPF_EXIT),
	},
};

static void
dummy_mbuf_prep(struct rte_mbuf *mb, uint8_t buf[], uint32_t buf_len,
	uint32_t data_len)
{
	uint32_t i;
	uint8_t *db;

	mb->buf_addr = buf;
	mb->buf_iova = (uintptr_t)buf;
	mb->buf_len = buf_len;
	rte_mbuf_refcnt_set(mb, 1);

	/* set pool pointer to dummy value, test doesn't use it */
	mb->pool = (void *)buf;

	rte_pktmbuf_reset(mb);
	db = (uint8_t *)rte_pktmbuf_append(mb, data_len);

	for (i = 0; i != data_len; i++)
		db[i] = i;
}

static void
test_ld_mbuf1_prepare(void *arg)
{
	struct dummy_mbuf *dm;
	struct rte_ipv4_hdr *ph;

	const uint32_t plen = 400;
	const struct rte_ipv4_hdr iph = {
		.version_ihl = RTE_IPV4_VHL_DEF,
		.total_length = rte_cpu_to_be_16(plen),
		.time_to_live = IPDEFTTL,
		.next_proto_id = IPPROTO_RAW,
		.src_addr = rte_cpu_to_be_32(RTE_IPV4_LOOPBACK),
		.dst_addr = rte_cpu_to_be_32(RTE_IPV4_BROADCAST),
	};

	dm = arg;
	memset(dm, 0, sizeof(*dm));

	dummy_mbuf_prep(&dm->mb[0], dm->buf[0], sizeof(dm->buf[0]),
		plen / 2 + 1);
	dummy_mbuf_prep(&dm->mb[1], dm->buf[1], sizeof(dm->buf[0]),
		plen / 2 - 1);

	rte_pktmbuf_chain(&dm->mb[0], &dm->mb[1]);

	ph = rte_pktmbuf_mtod(dm->mb, typeof(ph));
	memcpy(ph, &iph, sizeof(iph));
}

static uint64_t
test_ld_mbuf1(const struct rte_mbuf *pkt)
{
	uint64_t n, v;
	const uint8_t *p8;
	const uint16_t *p16;
	const uint32_t *p32;
	struct dummy_offset dof;

	/* load IPv4 version and IHL */
	p8 = rte_pktmbuf_read(pkt,
		offsetof(struct rte_ipv4_hdr, version_ihl), sizeof(*p8),
		&dof);
	if (p8 == NULL)
		return 0;

	/* check IP version */
	if ((p8[0] & 0xf0) != IPVERSION << 4)
		return 0;

	n = (p8[0] & RTE_IPV4_HDR_IHL_MASK) * RTE_IPV4_IHL_MULTIPLIER;

	/* load 3-rd byte of IP data */
	p8 = rte_pktmbuf_read(pkt, n + 3, sizeof(*p8), &dof);
	if (p8 == NULL)
		return 0;

	v = p8[0];

	/* load IPv4 src addr */
	p32 = rte_pktmbuf_read(pkt,
		offsetof(struct rte_ipv4_hdr, src_addr), sizeof(*p32),
		&dof);
	if (p32 == NULL)
		return 0;

	v += rte_be_to_cpu_32(p32[0]);

	/* load IPv4 total length */
	p16 = rte_pktmbuf_read(pkt,
		offsetof(struct rte_ipv4_hdr, total_length), sizeof(*p16),
		&dof);
	if (p16 == NULL)
		return 0;

	n = rte_be_to_cpu_16(p16[0]);

	/* load last 4 bytes of IP data */
	p32 = rte_pktmbuf_read(pkt, n - sizeof(*p32), sizeof(*p32), &dof);
	if (p32 == NULL)
		return 0;

	v += rte_be_to_cpu_32(p32[0]);

	/* load 2 bytes from the middle of IP data */
	p16 = rte_pktmbuf_read(pkt, n / 2, sizeof(*p16), &dof);
	if (p16 == NULL)
		return 0;

	v += rte_be_to_cpu_16(p16[0]);
	return v;
}

static int
test_ld_mbuf1_check(uint64_t rc, const void *arg)
{
	const struct dummy_mbuf *dm;
	uint64_t v;

	dm = arg;
	v = test_ld_mbuf1(dm->mb);
	return cmp_res(__func__, v, rc, arg, arg, 0);
}

/*
 * same as ld_mbuf1, but then truncate the mbuf by 1B,
 * so load of last 4B fail.
 */
static void
test_ld_mbuf2_prepare(void *arg)
{
	struct dummy_mbuf *dm;

	test_ld_mbuf1_prepare(arg);
	dm = arg;
	rte_pktmbuf_trim(dm->mb, 1);
}

static int
test_ld_mbuf2_check(uint64_t rc, const void *arg)
{
	return cmp_res(__func__, 0, rc, arg, arg, 0);
}

/* same as test_ld_mbuf1, but now store intermediate results on the stack */
static const struct ebpf_insn test_ld_mbuf3_prog[] = {

	/* BPF_ABS/BPF_IND implicitly expect mbuf ptr in R6 */
	{
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_X),
		.dst_reg = EBPF_REG_6,
		.src_reg = EBPF_REG_1,
	},
	/* load IPv4 version and IHL */
	{
		.code = (BPF_LD | BPF_ABS | BPF_B),
		.imm = offsetof(struct rte_ipv4_hdr, version_ihl),
	},
	/* check IP version */
	{
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_X),
		.dst_reg = EBPF_REG_2,
		.src_reg = EBPF_REG_0,
	},
	{
		.code = (BPF_ALU | BPF_AND | BPF_K),
		.dst_reg = EBPF_REG_2,
		.imm = 0xf0,
	},
	{
		.code = (BPF_JMP | BPF_JEQ | BPF_K),
		.dst_reg = EBPF_REG_2,
		.imm = IPVERSION << 4,
		.off = 2,
	},
	/* invalid IP version, return 0 */
	{
		.code = (EBPF_ALU64 | BPF_XOR | BPF_X),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_0,
	},
	{
		.code = (BPF_JMP | EBPF_EXIT),
	},
	/* load 3-rd byte of IP data */
	{
		.code = (BPF_ALU | BPF_AND | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = RTE_IPV4_HDR_IHL_MASK,
	},
	{
		.code = (BPF_ALU | BPF_LSH | BPF_K),
		.dst_reg = EBPF_REG_0,
		.imm = 2,
	},
	{
		.code = (BPF_LD | BPF_IND | BPF_B),
		.src_reg = EBPF_REG_0,
		.imm = 3,
	},
	{
		.code = (BPF_STX | BPF_MEM | BPF_B),
		.dst_reg = EBPF_REG_10,
		.src_reg = EBPF_REG_0,
		.off = (int16_t)(offsetof(struct dummy_offset, u8) -
			sizeof(struct dummy_offset)),
	},
	/* load IPv4 src addr */
	{
		.code = (BPF_LD | BPF_ABS | BPF_W),
		.imm = offsetof(struct rte_ipv4_hdr, src_addr),
	},
	{
		.code = (BPF_STX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_10,
		.src_reg = EBPF_REG_0,
		.off = (int16_t)(offsetof(struct dummy_offset, u32) -
			sizeof(struct dummy_offset)),
	},
	/* load IPv4 total length */
	{
		.code = (BPF_LD | BPF_ABS | BPF_H),
		.imm = offsetof(struct rte_ipv4_hdr, total_length),
	},
	{
		.code = (EBPF_ALU64 | EBPF_MOV | BPF_X),
		.dst_reg = EBPF_REG_8,
		.src_reg = EBPF_REG_0,
	},
	/* load last 4 bytes of IP data */
	{
		.code = (BPF_LD | BPF_IND | BPF_W),
		.src_reg = EBPF_REG_8,
		.imm = -(int32_t)sizeof(uint32_t),
	},
	{
		.code = (BPF_STX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_10,
		.src_reg = EBPF_REG_0,
		.off = (int16_t)(offsetof(struct dummy_offset, u64) -
			sizeof(struct dummy_offset)),
	},
	/* load 2 bytes from the middle of IP data */
	{
		.code = (EBPF_ALU64 | BPF_RSH | BPF_K),
		.dst_reg = EBPF_REG_8,
		.imm = 1,
	},
	{
		.code = (BPF_LD | BPF_IND | BPF_H),
		.src_reg = EBPF_REG_8,
	},
	{
		.code = (BPF_LDX | BPF_MEM | EBPF_DW),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_10,
		.off = (int16_t)(offsetof(struct dummy_offset, u64) -
			sizeof(struct dummy_offset)),
	},
	{
		.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_1,
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_W),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_10,
		.off = (int16_t)(offsetof(struct dummy_offset, u32) -
			sizeof(struct dummy_offset)),
	},
	{
		.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_1,
	},
	{
		.code = (BPF_LDX | BPF_MEM | BPF_B),
		.dst_reg = EBPF_REG_1,
		.src_reg = EBPF_REG_10,
		.off = (int16_t)(offsetof(struct dummy_offset, u8) -
			sizeof(struct dummy_offset)),
	},
	{
		.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		.dst_reg = EBPF_REG_0,
		.src_reg = EBPF_REG_1,
	},
	{
		.code = (BPF_JMP | EBPF_EXIT),
	},
};

/* all bpf test cases */
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
		.name = "test_ldimm1",
		.arg_sz = sizeof(struct dummy_offset),
		.prm = {
			.ins = test_ldimm1_prog,
			.nb_ins = RTE_DIM(test_ldimm1_prog),
			.prog_arg = {
				.type = RTE_BPF_ARG_PTR,
				.size = sizeof(struct dummy_offset),
			},
		},
		.prepare = test_store1_prepare,
		.check_result = test_ldimm1_check,
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
		.name = "test_jump2",
		.arg_sz = sizeof(struct dummy_net),
		.prm = {
			.ins = test_jump2_prog,
			.nb_ins = RTE_DIM(test_jump2_prog),
			.prog_arg = {
				.type = RTE_BPF_ARG_PTR,
				.size = sizeof(struct dummy_net),
			},
		},
		.prepare = test_jump2_prepare,
		.check_result = test_jump2_check,
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
	{
		.name = "test_call2",
		.arg_sz = sizeof(struct dummy_offset),
		.prm = {
			.ins = test_call2_prog,
			.nb_ins = RTE_DIM(test_call2_prog),
			.prog_arg = {
				.type = RTE_BPF_ARG_PTR,
				.size = sizeof(struct dummy_offset),
			},
			.xsym = test_call2_xsym,
			.nb_xsym = RTE_DIM(test_call2_xsym),
		},
		.prepare = test_store1_prepare,
		.check_result = test_call2_check,
		/* for now don't support function calls on 32 bit platform */
		.allow_fail = (sizeof(uint64_t) != sizeof(uintptr_t)),
	},
	{
		.name = "test_call3",
		.arg_sz = sizeof(struct dummy_vect8),
		.prm = {
			.ins = test_call3_prog,
			.nb_ins = RTE_DIM(test_call3_prog),
			.prog_arg = {
				.type = RTE_BPF_ARG_PTR,
				.size = sizeof(struct dummy_vect8),
			},
			.xsym = test_call3_xsym,
			.nb_xsym = RTE_DIM(test_call3_xsym),
		},
		.prepare = test_call3_prepare,
		.check_result = test_call3_check,
		/* for now don't support function calls on 32 bit platform */
		.allow_fail = (sizeof(uint64_t) != sizeof(uintptr_t)),
	},
	{
		.name = "test_call4",
		.arg_sz = sizeof(struct dummy_offset),
		.prm = {
			.ins = test_call4_prog,
			.nb_ins = RTE_DIM(test_call4_prog),
			.prog_arg = {
				.type = RTE_BPF_ARG_PTR,
				.size = 2 * sizeof(struct dummy_offset),
			},
			.xsym = test_call4_xsym,
			.nb_xsym = RTE_DIM(test_call4_xsym),
		},
		.prepare = test_store1_prepare,
		.check_result = test_call4_check,
		/* for now don't support function calls on 32 bit platform */
		.allow_fail = (sizeof(uint64_t) != sizeof(uintptr_t)),
	},
	{
		.name = "test_call5",
		.arg_sz = sizeof(struct dummy_offset),
		.prm = {
			.ins = test_call5_prog,
			.nb_ins = RTE_DIM(test_call5_prog),
			.prog_arg = {
				.type = RTE_BPF_ARG_PTR,
				.size = sizeof(struct dummy_offset),
			},
			.xsym = test_call5_xsym,
			.nb_xsym = RTE_DIM(test_call5_xsym),
		},
		.prepare = test_store1_prepare,
		.check_result = test_call5_check,
		/* for now don't support function calls on 32 bit platform */
		.allow_fail = (sizeof(uint64_t) != sizeof(uintptr_t)),
	},
	{
		.name = "test_ld_mbuf1",
		.arg_sz = sizeof(struct dummy_mbuf),
		.prm = {
			.ins = test_ld_mbuf1_prog,
			.nb_ins = RTE_DIM(test_ld_mbuf1_prog),
			.prog_arg = {
				.type = RTE_BPF_ARG_PTR_MBUF,
				.buf_size = sizeof(struct dummy_mbuf),
			},
		},
		.prepare = test_ld_mbuf1_prepare,
		.check_result = test_ld_mbuf1_check,
		/* mbuf as input argument is not supported on 32 bit platform */
		.allow_fail = (sizeof(uint64_t) != sizeof(uintptr_t)),
	},
	{
		.name = "test_ld_mbuf2",
		.arg_sz = sizeof(struct dummy_mbuf),
		.prm = {
			.ins = test_ld_mbuf1_prog,
			.nb_ins = RTE_DIM(test_ld_mbuf1_prog),
			.prog_arg = {
				.type = RTE_BPF_ARG_PTR_MBUF,
				.buf_size = sizeof(struct dummy_mbuf),
			},
		},
		.prepare = test_ld_mbuf2_prepare,
		.check_result = test_ld_mbuf2_check,
		/* mbuf as input argument is not supported on 32 bit platform */
		.allow_fail = (sizeof(uint64_t) != sizeof(uintptr_t)),
	},
	{
		.name = "test_ld_mbuf3",
		.arg_sz = sizeof(struct dummy_mbuf),
		.prm = {
			.ins = test_ld_mbuf3_prog,
			.nb_ins = RTE_DIM(test_ld_mbuf3_prog),
			.prog_arg = {
				.type = RTE_BPF_ARG_PTR_MBUF,
				.buf_size = sizeof(struct dummy_mbuf),
			},
		},
		.prepare = test_ld_mbuf1_prepare,
		.check_result = test_ld_mbuf1_check,
		/* mbuf as input argument is not supported on 32 bit platform */
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

	/* repeat the same test with jit, when possible */
	rte_bpf_get_jit(bpf, &jit);
	if (jit.func != NULL) {

		tst->prepare(tbuf);
		rc = jit.func(tbuf);
		rv = tst->check_result(rc, tbuf);
		ret |= rv;
		if (rv != 0) {
			printf("%s@%d: check_result(%s) failed, "
				"error: %d(%s);\n",
				__func__, __LINE__, tst->name,
				rv, strerror(rv));
		}
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

#endif /* !RTE_LIB_BPF */

REGISTER_TEST_COMMAND(bpf_autotest, test_bpf);

#ifdef RTE_HAS_LIBPCAP
#include <pcap/pcap.h>

static void
test_bpf_dump(struct bpf_program *cbf, const struct rte_bpf_prm *prm)
{
	printf("cBPF program (%u insns)\n", cbf->bf_len);
	bpf_dump(cbf, 1);

	if (prm != NULL) {
		printf("\neBPF program (%u insns)\n", prm->nb_ins);
		rte_bpf_dump(stdout, prm->ins, prm->nb_ins);
	}
}

static int
test_bpf_match(pcap_t *pcap, const char *str,
	       struct rte_mbuf *mb)
{
	struct bpf_program fcode;
	struct rte_bpf_prm *prm = NULL;
	struct rte_bpf *bpf = NULL;
	int ret = -1;
	uint64_t rc;

	if (pcap_compile(pcap, &fcode, str, 1, PCAP_NETMASK_UNKNOWN)) {
		printf("%s@%d: pcap_compile(\"%s\") failed: %s;\n",
		       __func__, __LINE__,  str, pcap_geterr(pcap));
		return -1;
	}

	prm = rte_bpf_convert(&fcode);
	if (prm == NULL) {
		printf("%s@%d: bpf_convert('%s') failed,, error=%d(%s);\n",
		       __func__, __LINE__, str, rte_errno, strerror(rte_errno));
		goto error;
	}

	bpf = rte_bpf_load(prm);
	if (bpf == NULL) {
		printf("%s@%d: failed to load bpf code, error=%d(%s);\n",
			__func__, __LINE__, rte_errno, strerror(rte_errno));
		goto error;
	}

	rc = rte_bpf_exec(bpf, mb);
	/* The return code from bpf capture filter is non-zero if matched */
	ret = (rc == 0);
error:
	if (bpf)
		rte_bpf_destroy(bpf);
	rte_free(prm);
	pcap_freecode(&fcode);
	return ret;
}

/* Basic sanity test can we match a IP packet */
static int
test_bpf_filter_sanity(pcap_t *pcap)
{
	const uint32_t plen = 100;
	struct rte_mbuf mb, *m;
	uint8_t tbuf[RTE_MBUF_DEFAULT_BUF_SIZE];
	struct {
		struct rte_ether_hdr eth_hdr;
		struct rte_ipv4_hdr ip_hdr;
	} *hdr;

	dummy_mbuf_prep(&mb, tbuf, sizeof(tbuf), plen);
	m = &mb;

	hdr = rte_pktmbuf_mtod(m, typeof(hdr));
	hdr->eth_hdr = (struct rte_ether_hdr) {
		.dst_addr.addr_bytes = "\xff\xff\xff\xff\xff\xff",
		.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4),
	};
	hdr->ip_hdr = (struct rte_ipv4_hdr) {
		.version_ihl = RTE_IPV4_VHL_DEF,
		.total_length = rte_cpu_to_be_16(plen),
		.time_to_live = IPDEFTTL,
		.next_proto_id = IPPROTO_RAW,
		.src_addr = rte_cpu_to_be_32(RTE_IPV4_LOOPBACK),
		.dst_addr = rte_cpu_to_be_32(RTE_IPV4_BROADCAST),
	};

	if (test_bpf_match(pcap, "ip", m) != 0) {
		printf("%s@%d: filter \"ip\" doesn't match test data\n",
		       __func__, __LINE__);
		return -1;
	}
	if (test_bpf_match(pcap, "not ip", m) == 0) {
		printf("%s@%d: filter \"not ip\" does match test data\n",
		       __func__, __LINE__);
		return -1;
	}

	return 0;
}

/*
 * Some sample pcap filter strings from
 * https://wiki.wireshark.org/CaptureFilters
 */
static const char * const sample_filters[] = {
	"host 172.18.5.4",
	"net 192.168.0.0/24",
	"src net 192.168.0.0/24",
	"src net 192.168.0.0 mask 255.255.255.0",
	"dst net 192.168.0.0/24",
	"dst net 192.168.0.0 mask 255.255.255.0",
	"port 53",
	"host 192.0.2.1 and not (port 80 or port 25)",
	"host 2001:4b98:db0::8 and not port 80 and not port 25",
	"port not 53 and not arp",
	"(tcp[0:2] > 1500 and tcp[0:2] < 1550) or (tcp[2:2] > 1500 and tcp[2:2] < 1550)",
	"ether proto 0x888e",
	"ether[0] & 1 = 0 and ip[16] >= 224",
	"icmp[icmptype] != icmp-echo and icmp[icmptype] != icmp-echoreply",
	"tcp[tcpflags] & (tcp-syn|tcp-fin) != 0 and not src and dst net 127.0.0.1",
	"not ether dst 01:80:c2:00:00:0e",
	"not broadcast and not multicast",
	"dst host ff02::1",
	"port 80 and tcp[((tcp[12:1] & 0xf0) >> 2):4] = 0x47455420",
	/* Worms */
	"dst port 135 and tcp port 135 and ip[2:2]==48",
	"icmp[icmptype]==icmp-echo and ip[2:2]==92 and icmp[8:4]==0xAAAAAAAA",
	"dst port 135 or dst port 445 or dst port 1433"
	" and tcp[tcpflags] & (tcp-syn) != 0"
	" and tcp[tcpflags] & (tcp-ack) = 0 and src net 192.168.0.0/24",
	"tcp src port 443 and (tcp[((tcp[12] & 0xF0) >> 4 ) * 4] = 0x18)"
	" and (tcp[((tcp[12] & 0xF0) >> 4 ) * 4 + 1] = 0x03)"
	" and (tcp[((tcp[12] & 0xF0) >> 4 ) * 4 + 2] < 0x04)"
	" and ((ip[2:2] - 4 * (ip[0] & 0x0F) - 4 * ((tcp[12] & 0xF0) >> 4) > 69))",
	/* Other */
	"len = 128",
};

static int
test_bpf_filter(pcap_t *pcap, const char *s)
{
	struct bpf_program fcode;
	struct rte_bpf_prm *prm = NULL;
	struct rte_bpf *bpf = NULL;

	if (pcap_compile(pcap, &fcode, s, 1, PCAP_NETMASK_UNKNOWN)) {
		printf("%s@%d: pcap_compile('%s') failed: %s;\n",
		       __func__, __LINE__, s, pcap_geterr(pcap));
		return -1;
	}

	prm = rte_bpf_convert(&fcode);
	if (prm == NULL) {
		printf("%s@%d: bpf_convert('%s') failed,, error=%d(%s);\n",
		       __func__, __LINE__, s, rte_errno, strerror(rte_errno));
		goto error;
	}

	bpf = rte_bpf_load(prm);
	if (bpf == NULL) {
		printf("%s@%d: failed to load bpf code, error=%d(%s);\n",
			__func__, __LINE__, rte_errno, strerror(rte_errno));
		goto error;
	}

error:
	if (bpf)
		rte_bpf_destroy(bpf);
	else {
		printf("%s \"%s\"\n", __func__, s);
		test_bpf_dump(&fcode, prm);
	}

	rte_free(prm);
	pcap_freecode(&fcode);
	return (bpf == NULL) ? -1 : 0;
}

static int
test_bpf_convert(void)
{
	unsigned int i;
	pcap_t *pcap;
	int rc;

	pcap = pcap_open_dead(DLT_EN10MB, 262144);
	if (!pcap) {
		printf("pcap_open_dead failed\n");
		return -1;
	}

	rc = test_bpf_filter_sanity(pcap);
	for (i = 0; i < RTE_DIM(sample_filters); i++)
		rc |= test_bpf_filter(pcap, sample_filters[i]);

	pcap_close(pcap);
	return rc;
}

REGISTER_TEST_COMMAND(bpf_convert_autotest, test_bpf_convert);
#endif /* RTE_HAS_LIBPCAP */
