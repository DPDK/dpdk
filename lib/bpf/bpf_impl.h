/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef BPF_IMPL_H
#define BPF_IMPL_H

#include <rte_bpf.h>
#include <sys/mman.h>

#define MAX_BPF_STACK_SIZE	0x200

struct rte_bpf {
	struct rte_bpf_prm_ex prm;
	struct rte_bpf_jit_ex jit;
	size_t sz;
	uint32_t stack_sz;
};

/* Temporary copies etc. used by the load process. */
struct __rte_bpf_load {
	struct rte_bpf_prm_ex prm;

	/* Conversion from cBPF. */
	struct ebpf_insn *ins;

	/* Loading ELF and applying relocations. */
	int elf_fd;  /* ELF fd, must be negative (not zero) by default. */
	void *elf;  /* Using void to avoid dependency on libelf. */

	/* Value we are going to return, if any. */
	struct rte_bpf *bpf;
};

/*
 * Use '__rte' prefix for non-static internal functions
 * to avoid potential name conflict with other libraries.
 */

/* Free temporary resources created by converting from cBPF to eBPF. */
void
__rte_bpf_convert_cleanup(struct __rte_bpf_load *load);

/* Convert program from cBPF to eBPF. */
int
__rte_bpf_convert(struct __rte_bpf_load *load);

/* Free temporary resources created by opening ELF. */
void
__rte_bpf_load_elf_cleanup(struct __rte_bpf_load *load);

/* Open the ELF file. */
int
__rte_bpf_load_elf_file(struct __rte_bpf_load *load);

/* Get code from ELF and apply relocations to it. */
int
__rte_bpf_load_elf_code(struct __rte_bpf_load *load);

/* Validate final BPF code and calculate stack size. */
int
__rte_bpf_validate(const struct rte_bpf_prm_ex *prm, uint32_t *stack_sz);

int __rte_bpf_jit(struct rte_bpf *bpf);
int __rte_bpf_jit_x86(struct rte_bpf *bpf);
int __rte_bpf_jit_arm64(struct rte_bpf *bpf);

extern int rte_bpf_logtype;
#define RTE_LOGTYPE_BPF rte_bpf_logtype

#define RTE_BPF_LOG_LINE(lvl, ...) \
	RTE_LOG_LINE(lvl, BPF, __VA_ARGS__)

#define RTE_BPF_LOG_FUNC_LINE(lvl, fmt, ...) \
	RTE_LOG_LINE(lvl, BPF, "%s(): " fmt, __func__, ##__VA_ARGS__)

static inline size_t
bpf_size(uint32_t bpf_op_sz)
{
	if (bpf_op_sz == BPF_B)
		return sizeof(uint8_t);
	else if (bpf_op_sz == BPF_H)
		return sizeof(uint16_t);
	else if (bpf_op_sz == BPF_W)
		return sizeof(uint32_t);
	else if (bpf_op_sz == EBPF_DW)
		return sizeof(uint64_t);
	return 0;
}

#endif /* BPF_IMPL_H */
