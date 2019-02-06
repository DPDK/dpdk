/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 */

#include "compat.h"
#include <fsl_qbman_debug.h>
#include "qbman_portal.h"

/* QBMan portal management command code */
#define QBMAN_BP_QUERY            0x32
#define QBMAN_FQ_QUERY            0x44
#define QBMAN_FQ_QUERY_NP         0x45
#define QBMAN_WQ_QUERY            0x47
#define QBMAN_CGR_QUERY           0x51
#define QBMAN_WRED_QUERY          0x54
#define QBMAN_CGR_STAT_QUERY      0x55
#define QBMAN_CGR_STAT_QUERY_CLR  0x56

struct qbman_fq_query_desc {
	uint8_t verb;
	uint8_t reserved[3];
	uint32_t fqid;
	uint8_t reserved2[57];
};

int qbman_fq_query_state(struct qbman_swp *s, uint32_t fqid,
			 struct qbman_fq_query_np_rslt *r)
{
	struct qbman_fq_query_desc *p;

	p = (struct qbman_fq_query_desc *)qbman_swp_mc_start(s);
	if (!p)
		return -EBUSY;

	p->fqid = fqid;
	*r = *(struct qbman_fq_query_np_rslt *)qbman_swp_mc_complete(s, p,
						QBMAN_FQ_QUERY_NP);
	if (!r) {
		pr_err("qbman: Query FQID %d NP fields failed, no response\n",
		       fqid);
		return -EIO;
	}

	/* Decode the outcome */
	QBMAN_BUG_ON((r->verb & QBMAN_RESPONSE_VERB_MASK) != QBMAN_FQ_QUERY_NP);

	/* Determine success or failure */
	if (r->rslt != QBMAN_MC_RSLT_OK) {
		pr_err("Query NP fields of FQID 0x%x failed, code=0x%02x\n",
		       fqid, r->rslt);
		return -EIO;
	}

	return 0;
}

uint32_t qbman_fq_state_frame_count(const struct qbman_fq_query_np_rslt *r)
{
	return (r->frm_cnt & 0x00FFFFFF);
}

uint32_t qbman_fq_state_byte_count(const struct qbman_fq_query_np_rslt *r)
{
	return r->byte_cnt;
}
