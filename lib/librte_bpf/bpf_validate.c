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

#include "bpf_impl.h"

/*
 * dummy one for now, need more work.
 */
int
bpf_validate(struct rte_bpf *bpf)
{
	int32_t rc, ofs, stack_sz;
	uint32_t i, op, dr;
	const struct ebpf_insn *ins;

	rc = 0;
	stack_sz = 0;
	for (i = 0; i != bpf->prm.nb_ins; i++) {

		ins = bpf->prm.ins + i;
		op = ins->code;
		dr = ins->dst_reg;
		ofs = ins->off;

		if ((BPF_CLASS(op) == BPF_STX || BPF_CLASS(op) == BPF_ST) &&
				dr == EBPF_REG_10) {
			ofs -= sizeof(uint64_t);
			stack_sz = RTE_MIN(ofs, stack_sz);
		}
	}

	if (stack_sz != 0) {
		stack_sz = -stack_sz;
		if (stack_sz > MAX_BPF_STACK_SIZE)
			rc = -ERANGE;
		else
			bpf->stack_sz = stack_sz;
	}

	if (rc != 0)
		RTE_BPF_LOG(ERR, "%s(%p) failed, error code: %d;\n",
			__func__, bpf, rc);
	return rc;
}
