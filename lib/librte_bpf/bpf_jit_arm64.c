/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <errno.h>

#include <rte_common.h>

#include "bpf_impl.h"
/*
 * Produce a native ISA version of the given BPF code.
 */
int
bpf_jit_arm64(struct rte_bpf *bpf)
{
	RTE_SET_USED(bpf);

	return -ENOTSUP;
}
