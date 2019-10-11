/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#include <rte_common.h>

#include "cpt_common.h"
#include "cpt_hw_types.h"
#include "cpt_mcode_defines.h"
#include "cpt_pmd_ops_helper.h"

#define CPT_MAX_IV_LEN 16
#define CPT_OFFSET_CONTROL_BYTES 8
#define CPT_MAX_ASYM_OP_NUM_PARAMS 5
#define CPT_MAX_ASYM_OP_MOD_LEN 1024

int32_t
cpt_pmd_ops_helper_get_mlen_direct_mode(void)
{
	uint32_t len = 0;

	/* Request structure */
	len = sizeof(struct cpt_request_info);

	/* CPT HW result structure plus extra as it is aligned */
	len += 2*sizeof(cpt_res_s_t);

	return len;
}

int
cpt_pmd_ops_helper_get_mlen_sg_mode(void)
{
	uint32_t len = 0;

	len += sizeof(struct cpt_request_info);
	len += CPT_OFFSET_CONTROL_BYTES + CPT_MAX_IV_LEN;
	len += ROUNDUP8(SG_LIST_HDR_SIZE +
			(ROUNDUP4(CPT_MAX_SG_IN_OUT_CNT) >> 2) * SG_ENTRY_SIZE);
	len += 2 * COMPLETION_CODE_SIZE;
	len += 2 * sizeof(cpt_res_s_t);
	return len;
}

int
cpt_pmd_ops_helper_asym_get_mlen(void)
{
	uint32_t len;

	/* Get meta len for linear buffer (direct) mode */
	len = cpt_pmd_ops_helper_get_mlen_direct_mode();

	/* Get meta len for asymmetric operations */
	len += CPT_MAX_ASYM_OP_NUM_PARAMS * CPT_MAX_ASYM_OP_MOD_LEN;
	return len;
}
