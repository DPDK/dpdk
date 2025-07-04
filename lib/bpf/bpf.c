/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>

#include <eal_export.h>
#include <rte_common.h>

#include "bpf_impl.h"

RTE_EXPORT_SYMBOL(rte_bpf_destroy)
void
rte_bpf_destroy(struct rte_bpf *bpf)
{
	if (bpf != NULL) {
		if (bpf->jit.func != NULL)
			munmap(bpf->jit.func, bpf->jit.sz);
		munmap(bpf, bpf->sz);
	}
}

RTE_EXPORT_SYMBOL(rte_bpf_get_jit)
int
rte_bpf_get_jit(const struct rte_bpf *bpf, struct rte_bpf_jit *jit)
{
	if (bpf == NULL || jit == NULL)
		return -EINVAL;

	jit[0] = bpf->jit;
	return 0;
}

int
__rte_bpf_jit(struct rte_bpf *bpf)
{
	int32_t rc;

#if defined(RTE_ARCH_X86_64)
	rc = __rte_bpf_jit_x86(bpf);
#elif defined(RTE_ARCH_ARM64)
	rc = __rte_bpf_jit_arm64(bpf);
#else
	rc = -ENOTSUP;
#endif

	if (rc != 0)
		RTE_BPF_LOG_LINE(WARNING, "%s(%p) failed, error code: %d;",
			__func__, bpf, rc);
	return rc;
}

RTE_LOG_REGISTER_DEFAULT(rte_bpf_logtype, INFO);
