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
		if (bpf->jit.raw != NULL)
			munmap(bpf->jit.raw, bpf->jit.sz);
		munmap(bpf, bpf->sz);
	}
}

RTE_EXPORT_SYMBOL(rte_bpf_get_jit)
int
rte_bpf_get_jit(const struct rte_bpf *bpf, struct rte_bpf_jit *jit)
{
	if (bpf == NULL || jit == NULL)
		return -EINVAL;

	if (bpf->prm.nb_prog_arg != 1) {
		RTE_BPF_LOG_LINE(ERR,
			"this program takes %d arguments, use rte_bpf_get_jit_ex",
			bpf->prm.nb_prog_arg);
		return -EINVAL;
	}

	*jit = (struct rte_bpf_jit) {
		.func = bpf->jit.raw,
		.sz = bpf->jit.sz,
	};
	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_bpf_get_jit_ex, 26.07)
int
rte_bpf_get_jit_ex(const struct rte_bpf *bpf, struct rte_bpf_jit_ex *jit)
{
	if (bpf == NULL || jit == NULL)
		return -EINVAL;

	if (bpf->jit.raw == NULL) {
		RTE_BPF_LOG_LINE(ERR, "no JIT-compiled version");
		return -ENOENT;
	}

	*jit = bpf->jit;
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
