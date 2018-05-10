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

int rte_bpf_logtype;

__rte_experimental void
rte_bpf_destroy(struct rte_bpf *bpf)
{
	if (bpf != NULL) {
		if (bpf->jit.func != NULL)
			munmap(bpf->jit.func, bpf->jit.sz);
		munmap(bpf, bpf->sz);
	}
}

__rte_experimental int
rte_bpf_get_jit(const struct rte_bpf *bpf, struct rte_bpf_jit *jit)
{
	if (bpf == NULL || jit == NULL)
		return -EINVAL;

	jit[0] = bpf->jit;
	return 0;
}

int
bpf_jit(struct rte_bpf *bpf)
{
	int32_t rc;

#ifdef RTE_ARCH_X86_64
	rc = bpf_jit_x86(bpf);
#else
	rc = -ENOTSUP;
#endif

	if (rc != 0)
		RTE_BPF_LOG(WARNING, "%s(%p) failed, error code: %d;\n",
			__func__, bpf, rc);
	return rc;
}

RTE_INIT(rte_bpf_init_log);

static void
rte_bpf_init_log(void)
{
	rte_bpf_logtype = rte_log_register("lib.bpf");
	if (rte_bpf_logtype >= 0)
		rte_log_set_level(rte_bpf_logtype, RTE_LOG_INFO);
}
