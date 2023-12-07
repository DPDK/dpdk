/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Intel Corporation
 */

#include "bpf_impl.h"
#include <rte_errno.h>

/**
 * Contains stubs for unimplemented public API functions
 */

#ifndef RTE_LIBRTE_BPF_ELF
struct rte_bpf *
rte_bpf_elf_load(const struct rte_bpf_prm *prm, const char *fname,
	const char *sname)
{
	if (prm == NULL || fname == NULL || sname == NULL) {
		rte_errno = EINVAL;
		return NULL;
	}

	RTE_BPF_LOG_LINE(ERR, "%s() is not supported, rebuild with libelf installed",
		__func__);
	rte_errno = ENOTSUP;
	return NULL;
}
#endif

#ifndef RTE_HAS_LIBPCAP
struct rte_bpf_prm *
rte_bpf_convert(const struct bpf_program *prog)
{
	if (prog == NULL) {
		rte_errno = EINVAL;
		return NULL;
	}

	RTE_BPF_LOG_LINE(ERR, "%s() is not supported, rebuild with libpcap installed",
		__func__);
	rte_errno = ENOTSUP;
	return NULL;
}
#endif
