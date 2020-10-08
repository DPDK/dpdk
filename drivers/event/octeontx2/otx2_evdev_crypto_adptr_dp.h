/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#ifndef _OTX2_EVDEV_CRYPTO_ADPTR_DP_H_
#define _OTX2_EVDEV_CRYPTO_ADPTR_DP_H_

#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_eventdev.h>

#include "cpt_pmd_logs.h"
#include "cpt_ucode.h"

#include "otx2_cryptodev.h"
#include "otx2_cryptodev_hw_access.h"
#include "otx2_cryptodev_ops_helper.h"
#include "otx2_cryptodev_qp.h"

static inline void
otx2_ca_deq_post_process(const struct otx2_cpt_qp *qp,
			 struct rte_crypto_op *cop, uintptr_t *rsp,
			 uint8_t cc)
{
	if (cop->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
		if (likely(cc == NO_ERR)) {
			/* Verify authentication data if required */
			if (unlikely(rsp[2]))
				compl_auth_verify(cop, (uint8_t *)rsp[2],
						 rsp[3]);
			else
				cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
		} else {
			if (cc == ERR_GC_ICV_MISCOMPARE)
				cop->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
			else
				cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
		}

		if (unlikely(cop->sess_type == RTE_CRYPTO_OP_SESSIONLESS)) {
			sym_session_clear(otx2_cryptodev_driver_id,
					  cop->sym->session);
			memset(cop->sym->session, 0,
			rte_cryptodev_sym_get_existing_header_session_size(
				cop->sym->session));
			rte_mempool_put(qp->sess_mp, cop->sym->session);
			cop->sym->session = NULL;
		}
	}

}

static inline uint64_t
otx2_handle_crypto_event(uint64_t get_work1)
{
	struct cpt_request_info *req;
	struct rte_crypto_op *cop;
	uintptr_t *rsp;
	void *metabuf;
	uint8_t cc;

	req = (struct cpt_request_info *)(get_work1);
	cc = otx2_cpt_compcode_get(req);

	rsp = req->op;
	metabuf = (void *)rsp[0];
	cop = (void *)rsp[1];

	otx2_ca_deq_post_process(req->qp, cop, rsp, cc);

	rte_mempool_put(req->qp->meta_info.pool, metabuf);

	return (uint64_t)(cop);
}
#endif /* _OTX2_EVDEV_CRYPTO_ADPTR_DP_H_ */
