/* SPDX-License-Identifier: BSD-3-Clause
 *
 * BPF program for testing rte_bpf_elf_load
 */

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

/* Match the structures from test_bpf.c */
struct dummy_offset {
	uint64_t u64;
	uint32_t u32;
	uint16_t u16;
	uint8_t  u8;
} __attribute__((packed));

struct dummy_vect8 {
	struct dummy_offset in[8];
	struct dummy_offset out[8];
};

/* External function declaration - provided by test via xsym */
extern void dummy_func1(const void *p, uint32_t *v32, uint64_t *v64);

/*
 * Test BPF function that will be loaded from ELF
 * This function is compiled version of code used in test_call1
 */
__attribute__((section("call1"), used))
uint64_t
test_call1(struct dummy_vect8 *arg)
{
	uint32_t v32;
	uint64_t v64;

	/* Load input values */
	v32 = arg->in[0].u32;
	v64 = arg->in[0].u64;

	/* Call external function */
	dummy_func1(arg, &v32, &v64);

	/* Store results */
	arg->out[0].u32 = v32;
	arg->out[0].u64 = v64;

	v64 += v32;
	return v64;
}
