/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>

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
	RTE_ATOMIC(uint64_t) u64;
	RTE_ATOMIC(uint32_t) u32;
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
	rte_atomic_fetch_add_explicit((uint32_t __rte_atomic *)&dfe.u32, rv,
			rte_memory_order_relaxed);
	rte_atomic_fetch_add_explicit((uint64_t __rte_atomic *)&dfe.u64, rv,
			rte_memory_order_relaxed);

	rv = -1;
	rte_atomic_fetch_add_explicit((uint32_t __rte_atomic *)&dfe.u32, rv,
			rte_memory_order_relaxed);
	rte_atomic_fetch_add_explicit((uint64_t __rte_atomic *)&dfe.u64, rv,
			rte_memory_order_relaxed);

	rv = (int32_t)TEST_FILL_1;
	rte_atomic_fetch_add_explicit((uint32_t __rte_atomic *)&dfe.u32, rv,
			rte_memory_order_relaxed);
	rte_atomic_fetch_add_explicit((uint64_t __rte_atomic *)&dfe.u64, rv,
			rte_memory_order_relaxed);

	rv = TEST_MUL_1;
	rte_atomic_fetch_add_explicit((uint32_t __rte_atomic *)&dfe.u32, rv,
			rte_memory_order_relaxed);
	rte_atomic_fetch_add_explicit((uint64_t __rte_atomic *)&dfe.u64, rv,
			rte_memory_order_relaxed);

	rv = TEST_MUL_2;
	rte_atomic_fetch_add_explicit((uint32_t __rte_atomic *)&dfe.u32, rv,
			rte_memory_order_relaxed);
	rte_atomic_fetch_add_explicit((uint64_t __rte_atomic *)&dfe.u64, rv,
			rte_memory_order_relaxed);

	rv = TEST_JCC_2;
	rte_atomic_fetch_add_explicit((uint32_t __rte_atomic *)&dfe.u32, rv,
			rte_memory_order_relaxed);
	rte_atomic_fetch_add_explicit((uint64_t __rte_atomic *)&dfe.u64, rv,
			rte_memory_order_relaxed);

	rv = TEST_JCC_3;
	rte_atomic_fetch_add_explicit((uint32_t __rte_atomic *)&dfe.u32, rv,
			rte_memory_order_relaxed);
	rte_atomic_fetch_add_explicit((uint64_t __rte_atomic *)&dfe.u64, rv,
			rte_memory_order_relaxed);

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
	rte_mbuf_iova_set(mb, (uintptr_t)buf);
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

REGISTER_FAST_TEST(bpf_autotest, NOHUGE_OK, ASAN_OK, test_bpf);

/* Tests of BPF JIT stack alignment when calling external functions (xfuncs). */

/* Function called from the BPF program in a test. */
typedef uint64_t (*text_xfunc_t)(uint64_t argument);

/* Call function from BPF program, verify that it incremented its argument. */
static int
call_from_bpf_test(text_xfunc_t xfunc)
{
	static const struct ebpf_insn ins[] = {
		{
			.code = (BPF_JMP | EBPF_CALL),
			.imm = 0,  /* xsym #0 */
		},
		{
			.code = (BPF_JMP | EBPF_EXIT),
		},
	};
	const struct rte_bpf_xsym xsym[] = {
		{
			.name = "xfunc",
			.type = RTE_BPF_XTYPE_FUNC,
			.func = {
				.val = (void *)xfunc,
				.nb_args = 1,
				.args = {
					{
						.type = RTE_BPF_ARG_RAW,
						.size = sizeof(uint64_t),
					},
				},
				.ret = {
					.type = RTE_BPF_ARG_RAW,
					.size = sizeof(uint64_t),
				},
			},
		},
	};
	const struct rte_bpf_prm prm = {
		.ins = ins,
		.nb_ins = RTE_DIM(ins),
		.xsym = xsym,
		.nb_xsym = RTE_DIM(xsym),
		.prog_arg = {
			.type = RTE_BPF_ARG_RAW,
			.size = sizeof(uint64_t),
		},
	};

	struct rte_bpf_jit jit;

	struct rte_bpf *const bpf = rte_bpf_load(&prm);
	RTE_TEST_ASSERT_NOT_EQUAL(bpf, NULL,
		"expect rte_bpf_load() != NULL");

	RTE_TEST_ASSERT_SUCCESS(rte_bpf_get_jit(bpf, &jit),
		"expect rte_bpf_get_jit() to succeed");

	const text_xfunc_t jit_function = (void *)jit.func;
	if (jit_function == NULL) {
		rte_bpf_destroy(bpf);
		return TEST_SKIPPED;
	}

	const uint64_t argument = 42;
	const uint64_t result = jit_function(argument);
	rte_bpf_destroy(bpf);

	RTE_TEST_ASSERT_EQUAL(result, argument + 1,
		"expect result == %ju, found %ju",
		(uintmax_t)(argument + 1), (uintmax_t)result);

	return TEST_SUCCESS;
}

/*
 * Test alignment of a local variable.
 *
 * NOTE: May produce false negatives with sanitizers if they replace the stack.
 */

/* Copy of the pointer to max_align stack variable, volatile to thwart optimization. */
static volatile uintptr_t stack_alignment_test_pointer;

static uint64_t
stack_alignment_xfunc(uint64_t argument)
{
	max_align_t max_align;
	stack_alignment_test_pointer = (uintptr_t)&max_align;
	return argument + 1;
}

static int
test_stack_alignment(void)
{
	const int test_rc = call_from_bpf_test(stack_alignment_xfunc);
	if (test_rc == TEST_SKIPPED)
		return TEST_SKIPPED;

	RTE_TEST_ASSERT_SUCCESS(test_rc,
		"expect call_from_bpf_test(stack_alignment_xfunc) to succeed");

	const uintptr_t test_offset = stack_alignment_test_pointer;
	RTE_TEST_ASSERT_NOT_EQUAL(test_offset, 0, "expect test_pointer != 0");

	const size_t test_alignment = test_offset % alignof(max_align_t);
	RTE_TEST_ASSERT_EQUAL(test_alignment, 0,
		"expect test_alignment == 0, found %zu", test_alignment);

	return TEST_SUCCESS;
}

REGISTER_FAST_TEST(bpf_stack_alignment_autotest, NOHUGE_OK, ASAN_OK, test_stack_alignment);

/*
 * Test copying `__uint128_t`.
 *
 * This operation is used by some variations of `rte_memcpy`;
 * it can also be produced by vectorizer in the compiler.
 */

#if defined(__SIZEOF_INT128__)

static uint64_t
stack_copy_uint128_xfunc(uint64_t argument)
{
	/* Pass addresses through volatiles to prevent compiler from optimizing it all out. */
	char alignas(16) src_buffer[16];
	char alignas(16) dst_buffer[16];
	void *const src = (char *volatile)src_buffer;
	void *const dst = (char *volatile)dst_buffer;
	const size_t size = 16;

	memset(src, 0x2a, size);
	memset(dst, 0x55, size);
	const int initial_memcmp_rc = memcmp(dst, src, size);

	const __uint128_t *const src128 = (const __uint128_t *)src;
	__uint128_t *const dst128 = (__uint128_t *)dst;
	*dst128 = *src128;
	const int memcmp_rc = memcmp(dst, src, size);

	return argument + 1 + !initial_memcmp_rc + memcmp_rc;
}

static int
test_stack_copy_uint128(void)
{
	const int test_rc = call_from_bpf_test(stack_copy_uint128_xfunc);
	if (test_rc == TEST_SKIPPED)
		return TEST_SKIPPED;

	RTE_TEST_ASSERT_SUCCESS(test_rc,
		"expect call_from_bpf_test(stack_copy_uint128_xfunc) to succeed");

	return TEST_SUCCESS;
}

#else

static int
test_stack_copy_uint128(void)
{
	return TEST_SKIPPED;
}

#endif

REGISTER_FAST_TEST(bpf_stack_copy_uint128_autotest, NOHUGE_OK, ASAN_OK, test_stack_copy_uint128);

/*
 * Test SSE2 load and store intrinsics.
 *
 * These intrinsics are used by e.g. lib/hash.
 *
 * Test both aligned and unaligned versions. Unaligned intrinsics may still fail
 * when the stack is misaligned, since they only treat memory address as
 * unaligned, not stack.
 */

#if defined(__SSE2__)

static uint64_t
stack_sse2_aligned_xfunc(uint64_t argument)
{
	/* Pass addresses through volatiles to prevent compiler from optimizing it all out. */
	char alignas(16) src_buffer[16];
	char alignas(16) dst_buffer[16];
	void *const src = (char *volatile)src_buffer;
	void *const dst = (char *volatile)dst_buffer;
	const size_t size = 16;

	memset(src, 0x2a, size);
	memset(dst, 0x55, size);
	const int initial_memcmp_rc = memcmp(dst, src, size);

	const __m128i tmp = _mm_load_si128((const __m128i *)src);
	_mm_store_si128((__m128i *)dst, tmp);
	const int memcmp_rc = memcmp(dst, src, size);

	return argument + 1 + !initial_memcmp_rc + memcmp_rc;
}

static uint64_t
stack_sse2_unaligned_xfunc(uint64_t argument)
{
	/* Pass addresses through volatiles to prevent compiler from optimizing it all out. */
	char alignas(16) src_buffer[17];
	char alignas(16) dst_buffer[17];
	void *const src = (char *volatile)src_buffer + 1;
	void *const dst = (char *volatile)dst_buffer + 1;
	const size_t size = 16;

	memset(src, 0x2a, size);
	memset(dst, 0x55, size);
	const int initial_memcmp_rc = memcmp(dst, src, size);

	const __m128i tmp = _mm_loadu_si128((const __m128i *)src);
	_mm_storeu_si128((__m128i *)dst, tmp);
	const int memcmp_rc = memcmp(dst, src, size);

	return argument + 1 + !initial_memcmp_rc + memcmp_rc;
}

static int
test_stack_sse2(void)
{
	int test_rc;

	test_rc = call_from_bpf_test(stack_sse2_aligned_xfunc);
	if (test_rc == TEST_SKIPPED)
		return test_rc;
	RTE_TEST_ASSERT_SUCCESS(test_rc,
		"expect call_from_bpf_test(stack_sse2_aligned_xfunc) to succeed");

	test_rc = call_from_bpf_test(stack_sse2_unaligned_xfunc);
	if (test_rc == TEST_SKIPPED)
		return test_rc;
	RTE_TEST_ASSERT_SUCCESS(test_rc,
		"expect call_from_bpf_test(stack_sse2_unaligned_xfunc) to succeed");

	return TEST_SUCCESS;
}

#else

static int
test_stack_sse2(void)
{
	return TEST_SKIPPED;
}

#endif

REGISTER_FAST_TEST(bpf_stack_sse2_autotest, NOHUGE_OK, ASAN_OK, test_stack_sse2);

/*
 * Run memcpy and rte_memcpy with various data sizes and offsets (unaligned and aligned).
 *
 * May produce false negatives even if BPF breaks stack alignment since
 * compilers may realign the stack in the beginning of the function to use
 * vector instructions with width larger than the default stack alignment.
 * However, represents very important use case that was broken in practice.
 *
 * For the reason specified above test 16-byte fixed-width memcpy explicitly.
 */

static void *volatile stack_memcpy_dst;
static const void *volatile stack_memcpy_src;
static size_t volatile stack_memcpy_size;

static uint64_t
stack_memcpy16_xfunc(uint64_t argument)
{
	RTE_ASSERT(stack_memcpy_size == 16);
	memcpy(stack_memcpy_dst, stack_memcpy_src, 16);
	return argument + 1;
}

static uint64_t
stack_rte_memcpy16_xfunc(uint64_t argument)
{
	RTE_ASSERT(stack_memcpy_size == 16);
	rte_memcpy(stack_memcpy_dst, stack_memcpy_src, 16);
	return argument + 1;
}

static uint64_t
stack_memcpy_xfunc(uint64_t argument)
{
	memcpy(stack_memcpy_dst, stack_memcpy_src, stack_memcpy_size);
	return argument + 1;
}

static uint64_t
stack_rte_memcpy_xfunc(uint64_t argument)
{
	rte_memcpy(stack_memcpy_dst, stack_memcpy_src, stack_memcpy_size);
	return argument + 1;
}

static int
stack_memcpy_subtest(text_xfunc_t xfunc, size_t size, size_t src_offset, size_t dst_offset)
{
	stack_memcpy_size = size;

	char *const src_buffer = malloc(size + src_offset);
	char *const dst_buffer = malloc(size + dst_offset);

	if (src_buffer == NULL || dst_buffer == NULL) {
		free(dst_buffer);
		free(src_buffer);
		return TEST_FAILED;
	}

	memset(src_buffer + src_offset, 0x2a, size);
	stack_memcpy_src = src_buffer + src_offset;

	memset(dst_buffer + dst_offset, 0x55, size);
	stack_memcpy_dst = dst_buffer + dst_offset;

	const int initial_memcmp_rc = memcmp(stack_memcpy_dst, stack_memcpy_src, size);
	const int test_rc = call_from_bpf_test(xfunc);
	const int memcmp_rc = memcmp(stack_memcpy_dst, stack_memcpy_src, size);

	free(dst_buffer);
	free(src_buffer);

	if (test_rc == TEST_SKIPPED)
		return TEST_SKIPPED;

	RTE_TEST_ASSERT_FAIL(initial_memcmp_rc, "expect memcmp() to fail initially");
	RTE_TEST_ASSERT_SUCCESS(test_rc, "expect call_from_bpf_test(xfunc) to succeed");
	RTE_TEST_ASSERT_SUCCESS(memcmp_rc, "expect memcmp() to succeed");

	return TEST_SUCCESS;
}

static int
test_stack_memcpy(void)
{
	for (int offsets = 0; offsets < 4; ++offsets) {
		const bool src_offset = offsets & 1;
		const bool dst_offset = offsets & 2;
		int test_rc;

		test_rc = stack_memcpy_subtest(stack_memcpy16_xfunc,
			16, src_offset, dst_offset);
		if (test_rc == TEST_SKIPPED)
			return test_rc;
		RTE_TEST_ASSERT_SUCCESS(test_rc,
			"expect stack_memcpy_subtest(stack_memcpy16_xfunc, "
				"16, %i, %i) to succeed",
			src_offset, dst_offset);

		test_rc = stack_memcpy_subtest(stack_rte_memcpy16_xfunc,
			16, src_offset, dst_offset);
		if (test_rc == TEST_SKIPPED)
			return test_rc;
		RTE_TEST_ASSERT_SUCCESS(test_rc,
			"expect stack_memcpy_subtest(stack_rte_memcpy16_xfunc, "
				"16, %i, %i) to succeed",
			src_offset, dst_offset);

		for (size_t size = 1; size <= 1024; size <<= 1) {
			test_rc = stack_memcpy_subtest(stack_memcpy_xfunc,
				size, src_offset, dst_offset);
			if (test_rc == TEST_SKIPPED)
				return test_rc;
			RTE_TEST_ASSERT_SUCCESS(test_rc,
				"expect stack_memcpy_subtest(stack_memcpy_xfunc, "
					"%zu, %i, %i) to succeed",
				size, src_offset, dst_offset);

			test_rc = stack_memcpy_subtest(stack_rte_memcpy_xfunc,
				size, src_offset, dst_offset);
			if (test_rc == TEST_SKIPPED)
				return test_rc;
			RTE_TEST_ASSERT_SUCCESS(test_rc,
				"expect stack_memcpy_subtest(stack_rte_memcpy_xfunc, "
					"%zu, %i, %i) to succeed",
				size, src_offset, dst_offset);
		}
	}
	return TEST_SUCCESS;
}

REGISTER_FAST_TEST(bpf_stack_memcpy_autotest, NOHUGE_OK, ASAN_OK, test_stack_memcpy);

#ifdef TEST_BPF_ELF_LOAD

/*
 * Helper function to write BPF object data to temporary file.
 * Returns temp file path on success, NULL on failure.
 * Caller must free the returned path and unlink the file.
 */
static char *
create_temp_bpf_file(const uint8_t *data, size_t size, const char *name)
{
	char *tmpfile = NULL;
	int fd;
	ssize_t written;

	if (asprintf(&tmpfile, "/tmp/dpdk_bpf_%s_XXXXXX.o", name) < 0) {
		printf("%s@%d: asprintf failed: %s\n",
		       __func__, __LINE__, strerror(errno));
		return NULL;
	}

	/* Create and open temp file */
	fd = mkstemps(tmpfile, strlen(".o"));
	if (fd < 0) {
		printf("%s@%d: mkstemps(%s) failed: %s\n",
		       __func__, __LINE__, tmpfile, strerror(errno));
		free(tmpfile);
		return NULL;
	}

	/* Write BPF object data */
	written = write(fd, data, size);
	close(fd);

	if (written != (ssize_t)size) {
		printf("%s@%d: write failed: %s\n",
		       __func__, __LINE__, strerror(errno));
		unlink(tmpfile);
		free(tmpfile);
		return NULL;
	}

	return tmpfile;
}

#include "test_bpf_load.h"

/*
 * Test loading BPF program from an object file.
 * This test uses same arguments as previous test_call1 example.
 */
static int
test_bpf_elf_load(void)
{
	static const char test_section[] = "call1";
	uint8_t tbuf[sizeof(struct dummy_vect8)];
	const struct rte_bpf_xsym xsym[] = {
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
	int ret;

	/* Create temp file from embedded BPF object */
	char *tmpfile = create_temp_bpf_file(app_test_bpf_load_o,
					     app_test_bpf_load_o_len,
					     "load");
	if (tmpfile == NULL)
		return -1;

	/* Try to load BPF program from temp file */
	const struct rte_bpf_prm prm = {
		.xsym = xsym,
		.nb_xsym = RTE_DIM(xsym),
		.prog_arg = {
			.type = RTE_BPF_ARG_PTR,
			.size = sizeof(tbuf),
		},
	};

	struct rte_bpf *bpf = rte_bpf_elf_load(&prm, tmpfile, test_section);
	unlink(tmpfile);
	free(tmpfile);

	/* If libelf support is not available */
	if (bpf == NULL && rte_errno == ENOTSUP)
		return TEST_SKIPPED;

	TEST_ASSERT(bpf != NULL, "failed to load BPF %d:%s", rte_errno, strerror(rte_errno));

	/* Prepare test data */
	struct dummy_vect8 *dv = (struct dummy_vect8 *)tbuf;

	memset(dv, 0, sizeof(*dv));
	dv->in[0].u64 = (int32_t)TEST_FILL_1;
	dv->in[0].u32 = dv->in[0].u64;
	dv->in[0].u16 = dv->in[0].u64;
	dv->in[0].u8 = dv->in[0].u64;

	/* Execute loaded BPF program */
	uint64_t rc = rte_bpf_exec(bpf, tbuf);
	ret = test_call1_check(rc, tbuf);
	TEST_ASSERT(ret == 0, "test_call1_check failed: %d", ret);

	/* Test JIT if available */
	struct rte_bpf_jit jit;
	ret = rte_bpf_get_jit(bpf, &jit);
	TEST_ASSERT(ret == 0, "rte_bpf_get_jit failed: %d", ret);

	if (jit.func != NULL) {
		memset(dv, 0, sizeof(*dv));
		dv->in[0].u64 = (int32_t)TEST_FILL_1;
		dv->in[0].u32 = dv->in[0].u64;
		dv->in[0].u16 = dv->in[0].u64;
		dv->in[0].u8 = dv->in[0].u64;

		rc = jit.func(tbuf);
		ret = test_call1_check(rc, tbuf);
		TEST_ASSERT(ret == 0, "jit test_call1_check failed: %d", ret);
	}

	rte_bpf_destroy(bpf);

	printf("%s: ELF load test passed\n", __func__);
	return TEST_SUCCESS;
}

#include <rte_ethdev.h>
#include <rte_bpf_ethdev.h>
#include <rte_bus_vdev.h>

#include "test_bpf_filter.h"

#define BPF_TEST_BURST		128
#define BPF_TEST_POOLSIZE	256 /* at least 2x burst */
#define BPF_TEST_PKT_LEN	64 /* Ether + IP + TCP */

static int null_vdev_setup(const char *name, uint16_t *port, struct rte_mempool *pool)
{
	int ret;

	/* Make a null device */
	ret = rte_vdev_init(name, NULL);
	TEST_ASSERT(ret == 0, "rte_vdev_init(%s) failed: %d", name, ret);

	ret = rte_eth_dev_get_port_by_name(name, port);
	TEST_ASSERT(ret == 0, "failed to get port id for %s: %d", name, ret);

	struct rte_eth_conf conf = { };
	ret = rte_eth_dev_configure(*port, 1, 1, &conf);
	TEST_ASSERT(ret == 0, "failed to configure port %u: %d", *port, ret);

	struct rte_eth_txconf txconf = { };
	ret = rte_eth_tx_queue_setup(*port, 0, BPF_TEST_BURST, SOCKET_ID_ANY, &txconf);
	TEST_ASSERT(ret == 0, "failed to setup tx queue port %u: %d", *port, ret);

	struct rte_eth_rxconf rxconf = { };
	ret = rte_eth_rx_queue_setup(*port, 0, BPF_TEST_BURST, SOCKET_ID_ANY,
				     &rxconf, pool);
	TEST_ASSERT(ret == 0, "failed to setup rx queue port %u: %d", *port, ret);

	ret = rte_eth_dev_start(*port);
	TEST_ASSERT(ret == 0, "failed to start port %u: %d", *port, ret);

	return 0;
}

static unsigned int
setup_mbufs(struct rte_mbuf *burst[], unsigned int n)
{
	struct rte_ether_hdr eh = {
		.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4),
	};
	const struct rte_ipv4_hdr iph = {
		.version_ihl = RTE_IPV4_VHL_DEF,
		.total_length = rte_cpu_to_be_16(BPF_TEST_PKT_LEN - sizeof(eh)),
		.time_to_live = IPDEFTTL,
		.src_addr = rte_cpu_to_be_32(ip_src_addr),
		.dst_addr = rte_cpu_to_be_32(ip_dst_addr),
	};
	unsigned int tcp_count = 0;

	rte_eth_random_addr(eh.dst_addr.addr_bytes);

	for (unsigned int i = 0; i < n; i++) {
		struct rte_mbuf *mb = burst[i];

		/* Setup Ethernet header */
		*rte_pktmbuf_mtod(mb, struct rte_ether_hdr *) = eh;

		/* Setup IP header */
		struct rte_ipv4_hdr *ip
			= rte_pktmbuf_mtod_offset(mb, struct rte_ipv4_hdr *, sizeof(eh));
		*ip = iph;

		if (rte_rand() & 1) {
			struct rte_udp_hdr *udp
				= rte_pktmbuf_mtod_offset(mb, struct rte_udp_hdr *,
							  sizeof(eh) + sizeof(iph));

			ip->next_proto_id = IPPROTO_UDP;
			*udp = (struct rte_udp_hdr) {
				.src_port = rte_cpu_to_be_16(9),	/* discard */
				.dst_port = rte_cpu_to_be_16(9),	/* discard */
				.dgram_len = BPF_TEST_PKT_LEN - sizeof(eh) - sizeof(iph),
			};

		} else {
			struct rte_tcp_hdr *tcp
				= rte_pktmbuf_mtod_offset(mb, struct rte_tcp_hdr *,
							  sizeof(eh) + sizeof(iph));

			ip->next_proto_id = IPPROTO_TCP;
			*tcp = (struct rte_tcp_hdr) {
				.src_port = rte_cpu_to_be_16(9),	/* discard */
				.dst_port = rte_cpu_to_be_16(9),	/* discard */
				.tcp_flags = RTE_TCP_RST_FLAG,
			};
			++tcp_count;
		}
	}

	return tcp_count;
}

static int bpf_tx_test(uint16_t port, const char *tmpfile, struct rte_mempool *pool,
		       const char *section, uint32_t flags)
{
	const struct rte_bpf_prm prm = {
		.prog_arg = {
			.type = RTE_BPF_ARG_PTR,
			.size = sizeof(struct rte_mbuf),
		},
	};
	int ret;

	/* Try to load BPF TX program from temp file */
	ret = rte_bpf_eth_tx_elf_load(port, 0, &prm, tmpfile, section, flags);
	if (ret != 0) {
		printf("%s@%d: failed to load BPF filter from file=%s error=%d:(%s)\n",
		       __func__, __LINE__, tmpfile, rte_errno, rte_strerror(rte_errno));
		return ret;
	}

	struct rte_mbuf *pkts[BPF_TEST_BURST] = { };
	ret = rte_pktmbuf_alloc_bulk(pool, pkts, BPF_TEST_BURST);
	TEST_ASSERT(ret == 0, "failed to allocate mbufs");

	uint16_t expect = setup_mbufs(pkts, BPF_TEST_BURST);

	uint16_t sent = rte_eth_tx_burst(port, 0, pkts, BPF_TEST_BURST);
	TEST_ASSERT_EQUAL(sent, expect, "rte_eth_tx_burst returned: %u expected %u",
			  sent, expect);

	/* The unsent packets should be dropped */
	rte_pktmbuf_free_bulk(pkts + sent, BPF_TEST_BURST - sent);

	/* Pool should have same number of packets avail */
	unsigned int avail = rte_mempool_avail_count(pool);
	TEST_ASSERT_EQUAL(avail, BPF_TEST_POOLSIZE,
			  "Mempool available %u != %u leaks?", avail, BPF_TEST_POOLSIZE);

	rte_bpf_eth_tx_unload(port, 0);
	return TEST_SUCCESS;
}

/* Test loading a transmit filter which only allows IPv4 packets */
static int
test_bpf_elf_tx_load(void)
{
	static const char null_dev[] = "net_null_bpf0";
	char *tmpfile = NULL;
	struct rte_mempool *mb_pool = NULL;
	uint16_t port = UINT16_MAX;
	int ret;

	printf("%s start\n", __func__);

	/* Make a pool for packets */
	mb_pool = rte_pktmbuf_pool_create("bpf_tx_test_pool", BPF_TEST_POOLSIZE,
					  0, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
					  SOCKET_ID_ANY);

	ret = null_vdev_setup(null_dev, &port, mb_pool);
	if (ret != 0)
		goto fail;

	/* Create temp file from embedded BPF object */
	tmpfile = create_temp_bpf_file(app_test_bpf_filter_o, app_test_bpf_filter_o_len, "tx");
	if (tmpfile == NULL)
		goto fail;

	/* Do test with VM */
	ret = bpf_tx_test(port, tmpfile, mb_pool, "filter", 0);
	if (ret != 0)
		goto fail;

	/* Repeat with JIT */
	ret = bpf_tx_test(port, tmpfile, mb_pool, "filter", RTE_BPF_ETH_F_JIT);
	if (ret == 0)
		printf("%s: TX ELF load test passed\n", __func__);

fail:
	if (tmpfile) {
		unlink(tmpfile);
		free(tmpfile);
	}

	if (port != UINT16_MAX)
		rte_vdev_uninit(null_dev);

	rte_mempool_free(mb_pool);

	if (ret == 0)
		return TEST_SUCCESS;
	else if (ret == -ENOTSUP)
		return TEST_SKIPPED;
	else
		return TEST_FAILED;
}

/* Test loading a receive filter */
static int bpf_rx_test(uint16_t port, const char *tmpfile, struct rte_mempool *pool,
		       const char *section, uint32_t flags, uint16_t expected)
{
	struct rte_mbuf *pkts[BPF_TEST_BURST];
	const struct rte_bpf_prm prm = {
		.prog_arg = {
			.type = RTE_BPF_ARG_PTR,
			.size = sizeof(struct rte_mbuf),
		},
	};
	int ret;

	/* Load BPF program to drop all packets */
	ret = rte_bpf_eth_rx_elf_load(port, 0, &prm, tmpfile, section, flags);
	if (ret != 0) {
		printf("%s@%d: failed to load BPF filter from file=%s error=%d:(%s)\n",
		       __func__, __LINE__, tmpfile, rte_errno, rte_strerror(rte_errno));
		return ret;
	}

	uint16_t rcvd = rte_eth_rx_burst(port, 0, pkts, BPF_TEST_BURST);
	TEST_ASSERT_EQUAL(rcvd, expected,
			  "rte_eth_rx_burst returned: %u expect: %u", rcvd, expected);

	/* Drop the received packets */
	rte_pktmbuf_free_bulk(pkts, rcvd);

	rte_bpf_eth_rx_unload(port, 0);

	/* Pool should now be full */
	unsigned int avail = rte_mempool_avail_count(pool);
	TEST_ASSERT_EQUAL(avail, BPF_TEST_POOLSIZE,
			  "Mempool available %u != %u leaks?", avail, BPF_TEST_POOLSIZE);

	return TEST_SUCCESS;
}

/* Test loading a receive filters, first with drop all and then with allow all packets */
static int
test_bpf_elf_rx_load(void)
{
	static const char null_dev[] = "net_null_bpf0";
	struct rte_mempool *pool = NULL;
	char *tmpfile = NULL;
	uint16_t port;
	int ret;

	printf("%s start\n", __func__);

	/* Make a pool for packets */
	pool = rte_pktmbuf_pool_create("bpf_rx_test_pool", 2 * BPF_TEST_BURST,
					  0, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
					  SOCKET_ID_ANY);
	TEST_ASSERT(pool != NULL, "failed to create mempool");

	ret = null_vdev_setup(null_dev, &port, pool);
	if (ret != 0)
		goto fail;

	/* Create temp file from embedded BPF object */
	tmpfile = create_temp_bpf_file(app_test_bpf_filter_o, app_test_bpf_filter_o_len, "rx");
	if (tmpfile == NULL)
		goto fail;

	/* Do test with VM */
	ret = bpf_rx_test(port, tmpfile, pool, "drop", 0, 0);
	if (ret != 0)
		goto fail;

	/* Repeat with JIT */
	ret = bpf_rx_test(port, tmpfile, pool, "drop", RTE_BPF_ETH_F_JIT, 0);
	if (ret != 0)
		goto fail;

	/* Repeat with allow all */
	ret = bpf_rx_test(port, tmpfile, pool, "allow", 0, BPF_TEST_BURST);
	if (ret != 0)
		goto fail;

	/* Repeat with JIT */
	ret = bpf_rx_test(port, tmpfile, pool, "allow", RTE_BPF_ETH_F_JIT, BPF_TEST_BURST);
	if (ret != 0)
		goto fail;

	printf("%s: RX ELF load test passed\n", __func__);

	/* The filter should free the mbufs */
	unsigned int avail = rte_mempool_avail_count(pool);
	TEST_ASSERT_EQUAL(avail, BPF_TEST_POOLSIZE,
			  "Mempool available %u != %u leaks?", avail, BPF_TEST_POOLSIZE);

fail:
	if (tmpfile) {
		unlink(tmpfile);
		free(tmpfile);
	}

	if (port != UINT16_MAX)
		rte_vdev_uninit(null_dev);

	rte_mempool_free(pool);

	return ret == 0 ? TEST_SUCCESS : TEST_FAILED;
}


static int
test_bpf_elf(void)
{
	int ret;

	ret = test_bpf_elf_load();
	if (ret == TEST_SUCCESS)
		ret = test_bpf_elf_tx_load();
	if (ret == TEST_SUCCESS)
		ret = test_bpf_elf_rx_load();

	return ret;
}

#else

static int
test_bpf_elf(void)
{
	printf("BPF compile not supported, skipping test\n");
	return TEST_SKIPPED;
}

#endif /* !TEST_BPF_ELF_LOAD */

REGISTER_FAST_TEST(bpf_elf_autotest, NOHUGE_OK, ASAN_OK, test_bpf_elf);

#ifndef RTE_HAS_LIBPCAP

static int
test_bpf_convert(void)
{
	printf("BPF convert RTE_HAS_LIBPCAP is undefined, skipping test\n");
	return TEST_SKIPPED;
}

#else
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

	memset(&mb, 0, sizeof(mb));
	dummy_mbuf_prep(&mb, tbuf, sizeof(tbuf), plen);
	m = &mb;

	hdr = rte_pktmbuf_mtod(m, typeof(hdr));
	hdr->eth_hdr = (struct rte_ether_hdr) {
		.dst_addr.addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
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
	"host 1::1 or host 1::1 or host 1::1 or host 1::1 or host 1::1 or host 1::1",
	("host 1::1 or host 1::2 or host 1::3 or host 1::4 or host 1::5 "
	"or host 192.0.2.1 or host 192.0.2.100 or host 192.0.2.200"),
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

	printf("bpf convert for \"%s\" produced:\n", s);
	rte_bpf_dump(stdout, prm->ins, prm->nb_ins);

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

#endif /* RTE_HAS_LIBPCAP */

REGISTER_FAST_TEST(bpf_convert_autotest, NOHUGE_OK, ASAN_OK, test_bpf_convert);

/*
 * Tests of BPF atomic instructions.
 */

/* Value that should be returned by the xchg test programs. */
#define XCHG_RETURN_VALUE 0xdeadbeefcafebabe

/* Operand of XADD, should overflow both 32-bit and 64-bit parts of initial value. */
#define XADD_OPERAND 0xc1c3c5c7c9cbcdcf

/* Argument type of the xchg test program. */
struct xchg_arg {
	uint64_t value0;
	uint64_t value1;
};

/* Initial value of the data area passed to the xchg test program. */
static const struct xchg_arg xchg_input = {
	.value0 = 0xa0a1a2a3a4a5a6a7,
	.value1 = 0xb0b1b2b3b4b5b6b7,
};

/* Run program against xchg_input and compare output value with expected. */
static int
run_xchg_test(uint32_t nb_ins, const struct ebpf_insn *ins, struct xchg_arg expected)
{
	const struct rte_bpf_prm prm = {
		.ins = ins,
		.nb_ins = nb_ins,
		.prog_arg = {
			.type = RTE_BPF_ARG_PTR,
			.size = sizeof(struct xchg_arg),
		},
	};

	for (int use_jit = false; use_jit <= true; ++use_jit) {
		struct xchg_arg argument = xchg_input;
		uint64_t return_value;

		struct rte_bpf *const bpf = rte_bpf_load(&prm);
		RTE_TEST_ASSERT_NOT_NULL(bpf, "expect rte_bpf_load() != NULL");

		if (use_jit) {
			struct rte_bpf_jit jit;
			RTE_TEST_ASSERT_SUCCESS(rte_bpf_get_jit(bpf, &jit),
				"expect rte_bpf_get_jit() to succeed");
			if (jit.func == NULL) {
				/* No JIT on this platform. */
				rte_bpf_destroy(bpf);
				continue;
			}

			return_value = jit.func(&argument);
		} else
			return_value = rte_bpf_exec(bpf, &argument);

		rte_bpf_destroy(bpf);

		RTE_TEST_ASSERT_EQUAL(return_value, XCHG_RETURN_VALUE,
			"expect return_value == %#jx, found %#jx, use_jit=%d",
			(uintmax_t)XCHG_RETURN_VALUE, (uintmax_t)return_value,
			use_jit);

		RTE_TEST_ASSERT_EQUAL(argument.value0, expected.value0,
			"expect value0 == %#jx, found %#jx, use_jit=%d",
			(uintmax_t)expected.value0, (uintmax_t)argument.value0,
			use_jit);

		RTE_TEST_ASSERT_EQUAL(argument.value1, expected.value1,
			"expect value1 == %#jx, found %#jx, use_jit=%d",
			(uintmax_t)expected.value1, (uintmax_t)argument.value1,
			use_jit);
	}

	return TEST_SUCCESS;
}

/*
 * Test 32-bit XADD.
 *
 * - Pre-fill r0 with return value.
 * - Fill r2 with XADD_OPERAND.
 * - Add (uint32_t)XADD_OPERAND to *(uint32_t *)&value0.
 * - Negate r2 and use it in the next operation to verify it was not corrupted.
 * - Add (uint32_t)-XADD_OPERAND to *(uint32_t *)&value1.
 * - Return r0 which should remain unchanged.
 */

static int
test_xadd32(void)
{
	static const struct ebpf_insn ins[] = {
		{
			/* Set r0 to return value. */
			.code = (BPF_LD | BPF_IMM | EBPF_DW),
			.dst_reg = EBPF_REG_0,
			.imm = (uint32_t)XCHG_RETURN_VALUE,
		},
		{
			/* Second part of 128-bit instruction. */
			.imm = XCHG_RETURN_VALUE >> 32,
		},
		{
			/* Set r2 to XADD operand. */
			.code = (BPF_LD | BPF_IMM | EBPF_DW),
			.dst_reg = EBPF_REG_2,
			.imm = (uint32_t)XADD_OPERAND,
		},
		{
			/* Second part of 128-bit instruction. */
			.imm = XADD_OPERAND >> 32,
		},
		{
			/* Atomically add r2 to value0, 32-bit. */
			.code = (BPF_STX | EBPF_ATOMIC | BPF_W),
			.src_reg = EBPF_REG_2,
			.dst_reg = EBPF_REG_1,
			.off = offsetof(struct xchg_arg, value0),
			.imm = BPF_ATOMIC_ADD,
		},
		{
			/* Negate r2. */
			.code = (EBPF_ALU64 | BPF_NEG | BPF_K),
			.dst_reg = EBPF_REG_2,
		},
		{
			/* Atomically add r2 to value1, 32-bit. */
			.code = (BPF_STX | EBPF_ATOMIC | BPF_W),
			.src_reg = EBPF_REG_2,
			.dst_reg = EBPF_REG_1,
			.off = offsetof(struct xchg_arg, value1),
			.imm = BPF_ATOMIC_ADD,
		},
		{
			.code = (BPF_JMP | EBPF_EXIT),
		},
	};
	const struct xchg_arg expected = {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		/* Only high 32 bits should be added. */
		.value0 = xchg_input.value0 + (XADD_OPERAND & RTE_GENMASK64(63, 32)),
		.value1 = xchg_input.value1 - (XADD_OPERAND & RTE_GENMASK64(63, 32)),
#elif RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
		/* Only low 32 bits should be added, without carry. */
		.value0 = (xchg_input.value0 & RTE_GENMASK64(63, 32)) |
			((xchg_input.value0 + XADD_OPERAND) & RTE_GENMASK64(31, 0)),
		.value1 = (xchg_input.value1 & RTE_GENMASK64(63, 32)) |
			((xchg_input.value1 - XADD_OPERAND) & RTE_GENMASK64(31, 0)),
#else
#error Unsupported endianness.
#endif
	};
	return run_xchg_test(RTE_DIM(ins), ins, expected);
}

REGISTER_FAST_TEST(bpf_xadd32_autotest, NOHUGE_OK, ASAN_OK, test_xadd32);

/*
 * Test 64-bit XADD.
 *
 * - Pre-fill r0 with return value.
 * - Fill r2 with XADD_OPERAND.
 * - Add XADD_OPERAND to value0.
 * - Negate r2 and use it in the next operation to verify it was not corrupted.
 * - Add -XADD_OPERAND to value1.
 * - Return r0 which should remain unchanged.
 */

static int
test_xadd64(void)
{
	static const struct ebpf_insn ins[] = {
		{
			/* Set r0 to return value. */
			.code = (BPF_LD | BPF_IMM | EBPF_DW),
			.dst_reg = EBPF_REG_0,
			.imm = (uint32_t)XCHG_RETURN_VALUE,
		},
		{
			/* Second part of 128-bit instruction. */
			.imm = XCHG_RETURN_VALUE >> 32,
		},
		{
			/* Set r2 to XADD operand. */
			.code = (BPF_LD | BPF_IMM | EBPF_DW),
			.dst_reg = EBPF_REG_2,
			.imm = (uint32_t)XADD_OPERAND,
		},
		{
			/* Second part of 128-bit instruction. */
			.imm = XADD_OPERAND >> 32,
		},
		{
			/* Atomically add r2 to value0. */
			.code = (BPF_STX | EBPF_ATOMIC | EBPF_DW),
			.src_reg = EBPF_REG_2,
			.dst_reg = EBPF_REG_1,
			.off = offsetof(struct xchg_arg, value0),
			.imm = BPF_ATOMIC_ADD,
		},
		{
			/* Negate r2. */
			.code = (EBPF_ALU64 | BPF_NEG | BPF_K),
			.dst_reg = EBPF_REG_2,
		},
		{
			/* Atomically add r2 to value1. */
			.code = (BPF_STX | EBPF_ATOMIC | EBPF_DW),
			.src_reg = EBPF_REG_2,
			.dst_reg = EBPF_REG_1,
			.off = offsetof(struct xchg_arg, value1),
			.imm = BPF_ATOMIC_ADD,
		},
		{
			.code = (BPF_JMP | EBPF_EXIT),
		},
	};
	const struct xchg_arg expected = {
		.value0 = xchg_input.value0 + XADD_OPERAND,
		.value1 = xchg_input.value1 - XADD_OPERAND,
	};
	return run_xchg_test(RTE_DIM(ins), ins, expected);
}

REGISTER_FAST_TEST(bpf_xadd64_autotest, NOHUGE_OK, ASAN_OK, test_xadd64);

/*
 * Test 32-bit XCHG.
 *
 * - Pre-fill r2 with return value.
 * - Exchange *(uint32_t *)&value0 and *(uint32_t *)&value1 via r2.
 * - Upper half of r2 should get cleared, so add it back before returning.
 */

static int
test_xchg32(void)
{
	static const struct ebpf_insn ins[] = {
		{
			/* Set r2 to return value. */
			.code = (BPF_LD | BPF_IMM | EBPF_DW),
			.dst_reg = EBPF_REG_2,
			.imm = (uint32_t)XCHG_RETURN_VALUE,
		},
		{
			/* Second part of 128-bit instruction. */
			.imm = XCHG_RETURN_VALUE >> 32,
		},
		{
			/* Atomically exchange r2 with value0, 32-bit. */
			.code = (BPF_STX | EBPF_ATOMIC | BPF_W),
			.src_reg = EBPF_REG_2,
			.dst_reg = EBPF_REG_1,
			.off = offsetof(struct xchg_arg, value0),
			.imm = BPF_ATOMIC_XCHG,
		},
		{
			/* Atomically exchange r2 with value1, 32-bit. */
			.code = (BPF_STX | EBPF_ATOMIC | BPF_W),
			.src_reg = EBPF_REG_2,
			.dst_reg = EBPF_REG_1,
			.off = offsetof(struct xchg_arg, value1),
			.imm = BPF_ATOMIC_XCHG,
		},
		{
			/* Atomically exchange r2 with value0, 32-bit. */
			.code = (BPF_STX | EBPF_ATOMIC | BPF_W),
			.src_reg = EBPF_REG_2,
			.dst_reg = EBPF_REG_1,
			.off = offsetof(struct xchg_arg, value0),
			.imm = BPF_ATOMIC_XCHG,
		},
		{
			/* Set upper half of r0 to return value. */
			.code = (BPF_LD | BPF_IMM | EBPF_DW),
			.dst_reg = EBPF_REG_0,
			.imm = 0,
		},
		{
			/* Second part of 128-bit instruction. */
			.imm = XCHG_RETURN_VALUE >> 32,
		},
		{
			/*
			 * Add r2 (should have upper half cleared by this time)
			 * to r0 to use as a return value.
			 */
			.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
			.src_reg = EBPF_REG_2,
			.dst_reg = EBPF_REG_0,
		},
		{
			.code = (BPF_JMP | EBPF_EXIT),
		},
	};
	struct xchg_arg expected = {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	/* Only high 32 bits should be exchanged. */
		.value0 =
			(xchg_input.value0 & RTE_GENMASK64(31, 0)) |
			(xchg_input.value1 & RTE_GENMASK64(63, 32)),
		.value1 =
			(xchg_input.value1 & RTE_GENMASK64(31, 0)) |
			(xchg_input.value0 & RTE_GENMASK64(63, 32)),
#elif RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	/* Only low 32 bits should be exchanged. */
		.value0 =
			(xchg_input.value1 & RTE_GENMASK64(31, 0)) |
			(xchg_input.value0 & RTE_GENMASK64(63, 32)),
		.value1 =
			(xchg_input.value0 & RTE_GENMASK64(31, 0)) |
			(xchg_input.value1 & RTE_GENMASK64(63, 32)),
#else
#error Unsupported endianness.
#endif
	};
	return run_xchg_test(RTE_DIM(ins), ins, expected);
}

REGISTER_FAST_TEST(bpf_xchg32_autotest, NOHUGE_OK, ASAN_OK, test_xchg32);

/*
 * Test 64-bit XCHG.
 *
 * - Pre-fill r2 with return value.
 * - Exchange value0 and value1 via r2.
 * - Return r2, which should remain unchanged.
 */

static int
test_xchg64(void)
{
	static const struct ebpf_insn ins[] = {
		{
			/* Set r2 to return value. */
			.code = (BPF_LD | BPF_IMM | EBPF_DW),
			.dst_reg = EBPF_REG_2,
			.imm = (uint32_t)XCHG_RETURN_VALUE,
		},
		{
			/* Second part of 128-bit instruction. */
			.imm = XCHG_RETURN_VALUE >> 32,
		},
		{
			/* Atomically exchange r2 with value0. */
			.code = (BPF_STX | EBPF_ATOMIC | EBPF_DW),
			.src_reg = EBPF_REG_2,
			.dst_reg = EBPF_REG_1,
			.off = offsetof(struct xchg_arg, value0),
			.imm = BPF_ATOMIC_XCHG,
		},
		{
			/* Atomically exchange r2 with value1. */
			.code = (BPF_STX | EBPF_ATOMIC | EBPF_DW),
			.src_reg = EBPF_REG_2,
			.dst_reg = EBPF_REG_1,
			.off = offsetof(struct xchg_arg, value1),
			.imm = BPF_ATOMIC_XCHG,
		},
		{
			/* Atomically exchange r2 with value0. */
			.code = (BPF_STX | EBPF_ATOMIC | EBPF_DW),
			.src_reg = EBPF_REG_2,
			.dst_reg = EBPF_REG_1,
			.off = offsetof(struct xchg_arg, value0),
			.imm = BPF_ATOMIC_XCHG,
		},
		{
			/* Copy r2 to r0 to use as a return value. */
			.code = (EBPF_ALU64 | EBPF_MOV | BPF_X),
			.src_reg = EBPF_REG_2,
			.dst_reg = EBPF_REG_0,
		},
		{
			.code = (BPF_JMP | EBPF_EXIT),
		},
	};
	const struct xchg_arg expected = {
		.value0 = xchg_input.value1,
		.value1 = xchg_input.value0,
	};
	return run_xchg_test(RTE_DIM(ins), ins, expected);
}

REGISTER_FAST_TEST(bpf_xchg64_autotest, NOHUGE_OK, ASAN_OK, test_xchg64);

/*
 * Test invalid and unsupported atomic imm values (also valid ones for control).
 *
 * For realism use a meaningful subset of the test_xchg64 program.
 */

static int
test_atomic_imm(int32_t imm, bool is_valid)
{
	const struct ebpf_insn ins[] = {
		{
			/* Set r2 to return value. */
			.code = (BPF_LD | BPF_IMM | EBPF_DW),
			.dst_reg = EBPF_REG_2,
			.imm = (uint32_t)XCHG_RETURN_VALUE,
		},
		{
			/* Second part of 128-bit instruction. */
			.imm = XCHG_RETURN_VALUE >> 32,
		},
		{
			/* Atomically exchange r2 with value0. */
			.code = (BPF_STX | EBPF_ATOMIC | EBPF_DW),
			.src_reg = EBPF_REG_2,
			.dst_reg = EBPF_REG_1,
			.off = offsetof(struct xchg_arg, value0),
			.imm = imm,
		},
		{
			/* Copy r2 to r0 to use as a return value. */
			.code = (EBPF_ALU64 | EBPF_MOV | BPF_X),
			.src_reg = EBPF_REG_2,
			.dst_reg = EBPF_REG_0,
		},
		{
			.code = (BPF_JMP | EBPF_EXIT),
		},
	};
	const struct rte_bpf_prm prm = {
		.ins = ins,
		.nb_ins = RTE_DIM(ins),
		.prog_arg = {
			.type = RTE_BPF_ARG_PTR,
			.size = sizeof(struct xchg_arg),
		},
	};

	struct rte_bpf *const bpf = rte_bpf_load(&prm);
	rte_bpf_destroy(bpf);

	if (is_valid)
		RTE_TEST_ASSERT_NOT_NULL(bpf, "expect rte_bpf_load() != NULL, imm=%#x", imm);
	else
		RTE_TEST_ASSERT_NULL(bpf, "expect rte_bpf_load() == NULL, imm=%#x", imm);

	return TEST_SUCCESS;
}

static int
test_atomic_imms(void)
{
	RTE_TEST_ASSERT_SUCCESS(test_atomic_imm(INT32_MIN, false), "expect success");
	for (int32_t imm = BPF_ATOMIC_ADD - 1; imm <= BPF_ATOMIC_XCHG + 1; ++imm) {
		const bool is_valid = imm == BPF_ATOMIC_ADD || imm == BPF_ATOMIC_XCHG;
		RTE_TEST_ASSERT_SUCCESS(test_atomic_imm(imm, is_valid), "expect success");
	}
	RTE_TEST_ASSERT_SUCCESS(test_atomic_imm(INT32_MAX, false), "expect success");

	return TEST_SUCCESS;
}

REGISTER_FAST_TEST(bpf_atomic_imms_autotest, NOHUGE_OK, ASAN_OK, test_atomic_imms);
