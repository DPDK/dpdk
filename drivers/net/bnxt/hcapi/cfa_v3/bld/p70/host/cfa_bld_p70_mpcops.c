/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#define COMP_ID BLD

#include <errno.h>
#include <string.h>
#include "cfa_trace.h"
#include "cfa_bld.h"
#include "host/cfa_bld_mpcops.h"
#include "cfa_bld_p70_host_mpc_wrapper.h"
#include "cfa_bld_p70_mpcops.h"

const struct cfa_bld_mpcops cfa_bld_p70_mpcops = {
	/* Build command apis */
	.cfa_bld_mpc_build_cache_read = cfa_bld_p70_mpc_build_cache_read,
	.cfa_bld_mpc_build_cache_write = cfa_bld_p70_mpc_build_cache_write,
	.cfa_bld_mpc_build_cache_evict = cfa_bld_p70_mpc_build_cache_evict,
	.cfa_bld_mpc_build_cache_read_clr = cfa_bld_p70_mpc_build_cache_rdclr,
	.cfa_bld_mpc_build_em_search = cfa_bld_p70_mpc_build_em_search,
	.cfa_bld_mpc_build_em_insert = cfa_bld_p70_mpc_build_em_insert,
	.cfa_bld_mpc_build_em_delete = cfa_bld_p70_mpc_build_em_delete,
	.cfa_bld_mpc_build_em_chain = cfa_bld_p70_mpc_build_em_chain,
	/* Parse response apis */
	.cfa_bld_mpc_parse_cache_read = cfa_bld_p70_mpc_parse_cache_read,
	.cfa_bld_mpc_parse_cache_write = cfa_bld_p70_mpc_parse_cache_write,
	.cfa_bld_mpc_parse_cache_evict = cfa_bld_p70_mpc_parse_cache_evict,
	.cfa_bld_mpc_parse_cache_read_clr = cfa_bld_p70_mpc_parse_cache_rdclr,
	.cfa_bld_mpc_parse_em_search = cfa_bld_p70_mpc_parse_em_search,
	.cfa_bld_mpc_parse_em_insert = cfa_bld_p70_mpc_parse_em_insert,
	.cfa_bld_mpc_parse_em_delete = cfa_bld_p70_mpc_parse_em_delete,
	.cfa_bld_mpc_parse_em_chain = cfa_bld_p70_mpc_parse_em_chain,
};

int cfa_bld_p70_mpc_bind(enum cfa_ver hw_ver, struct cfa_bld_mpcinfo *mpcinfo)
{
	if (hw_ver != CFA_P70)
		return -EINVAL;

	if (!mpcinfo)
		return -EINVAL;

	mpcinfo->mpcops = &cfa_bld_p70_mpcops;

	return 0;
}
