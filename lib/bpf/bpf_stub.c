/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Intel Corporation
 */

#include "bpf_impl.h"
#include <eal_export.h>
#include <rte_errno.h>

/**
 * Contains stubs for unimplemented public API functions
 */

#ifndef RTE_HAS_LIBPCAP
RTE_EXPORT_SYMBOL(rte_bpf_convert)
struct rte_bpf_prm *
rte_bpf_convert(const struct bpf_program *prog)
{
	if (prog == NULL) {
		rte_errno = EINVAL;
		return NULL;
	}

	RTE_BPF_LOG_FUNC_LINE(ERR, "not supported, rebuild with libpcap installed");
	rte_errno = ENOTSUP;
	return NULL;
}
#endif
