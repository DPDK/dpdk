/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef BPF_IMPL_H
#define BPF_IMPL_H

#include <rte_bpf.h>
#include <sys/mman.h>

#define MAX_BPF_STACK_SIZE	0x200

struct rte_bpf {
	struct rte_bpf_prm prm;
	struct rte_bpf_jit jit;
	size_t sz;
	uint32_t stack_sz;
};

/*
 * Use '__rte' prefix for non-static internal functions
 * to avoid potential name conflict with other libraries.
 */
int __rte_bpf_validate(struct rte_bpf *bpf);
int __rte_bpf_jit(struct rte_bpf *bpf);
int __rte_bpf_jit_x86(struct rte_bpf *bpf);
int __rte_bpf_jit_arm64(struct rte_bpf *bpf);

extern int rte_bpf_logtype;
#define RTE_LOGTYPE_BPF rte_bpf_logtype

#define RTE_BPF_LOG_LINE(lvl, ...) \
	RTE_LOG_LINE(lvl, BPF, __VA_ARGS__)

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
