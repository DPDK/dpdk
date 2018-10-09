/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#ifndef _CPT_REQUEST_MGR_H_
#define _CPT_REQUEST_MGR_H_

#include <rte_branch_prediction.h>
#include <rte_cycles.h>

#include "cpt_common.h"
#include "cpt_mcode_defines.h"

#if CPT_MODEL == CRYPTO_OCTEONTX
#include "../../crypto/octeontx/otx_cryptodev_hw_access.h"
#endif

/*
 * This file defines the agreement between the common layer and the individual
 * crypto drivers for OCTEON TX series. Datapath in otx* directory include this
 * file and all these functions are static inlined for better performance.
 *
 */

/*
 * Get the session size
 *
 * This function is used in the data path.
 *
 * @return
 *   - session size
 */
static __rte_always_inline unsigned int
cpt_get_session_size(void)
{
	unsigned int ctx_len = sizeof(struct cpt_ctx);
	return (sizeof(struct cpt_sess_misc) + RTE_ALIGN_CEIL(ctx_len, 8));
}

static __rte_always_inline int32_t __hot
cpt_enqueue_req(struct cpt_instance *instance, struct pending_queue *pqueue,
		void *req)
{
	struct cpt_request_info *user_req = (struct cpt_request_info *)req;
	int32_t ret = 0;

	if (unlikely(!req))
		return 0;

	if (unlikely(pqueue->pending_count >= DEFAULT_CMD_QLEN))
		return -EAGAIN;

	fill_cpt_inst(instance, req);

	CPT_LOG_DP_DEBUG("req: %p op: %p ", req, user_req->op);

	/* Fill time_out cycles */
	user_req->time_out = rte_get_timer_cycles() +
			DEFAULT_COMMAND_TIMEOUT * rte_get_timer_hz();
	user_req->extra_time = 0;

	/* Default mode of software queue */
	mark_cpt_inst(instance);

	pqueue->rid_queue[pqueue->enq_tail].rid =
		(uintptr_t)user_req;
	/* We will use soft queue length here to limit
	 * requests
	 */
	MOD_INC(pqueue->enq_tail, DEFAULT_CMD_QLEN);
	pqueue->pending_count += 1;

	CPT_LOG_DP_DEBUG("Submitted NB cmd with request: %p "
			 "op: %p", user_req, user_req->op);

	return ret;
}

static __rte_always_inline int __hot
cpt_pmd_crypto_operation(struct cpt_instance *instance,
		struct rte_crypto_op *op, struct pending_queue *pqueue,
		uint8_t cpt_driver_id)
{
	struct cpt_sess_misc *sess = NULL;
	struct rte_crypto_sym_op *sym_op = op->sym;
	void *prep_req = NULL, *mdata = NULL;
	int ret = 0;
	uint64_t cpt_op;
	struct cpt_vf *cptvf = (struct cpt_vf *)instance;

	if (unlikely(op->sess_type == RTE_CRYPTO_OP_SESSIONLESS)) {
		int sess_len;

		sess_len = cpt_get_session_size();

		sess = rte_calloc(__func__, 1, sess_len, 8);
		if (!sess)
			return -ENOMEM;

		sess->ctx_dma_addr =  rte_malloc_virt2iova(sess) +
			sizeof(struct cpt_sess_misc);

		ret = instance_session_cfg(sym_op->xform, (void *)sess);
		if (unlikely(ret))
			return -EINVAL;
	} else {
		sess = (struct cpt_sess_misc *)
		get_sym_session_private_data(sym_op->session,
		cpt_driver_id);
	}

	cpt_op = sess->cpt_op;

	mdata = &(cptvf->meta_info);

	if (likely(cpt_op & CPT_OP_CIPHER_MASK))
		prep_req = fill_fc_params(op, sess, &mdata, &ret);
	else
		prep_req = fill_digest_params(op, sess, &mdata, &ret);

	if (unlikely(!prep_req)) {
		CPT_LOG_DP_ERR("prep cryto req : op %p, cpt_op 0x%x "
			       "ret 0x%x", op, (unsigned int)cpt_op, ret);
		goto req_fail;
	}

	/* Enqueue prepared instruction to HW */
	ret = cpt_enqueue_req(instance, pqueue, prep_req);

	if (unlikely(ret)) {
		if (unlikely(ret == -EAGAIN))
			goto req_fail;
		CPT_LOG_DP_ERR("Error enqueing crypto request : error "
			       "code %d", ret);
		goto req_fail;
	}

	return 0;

req_fail:
	if (mdata)
		free_op_meta(mdata, cptvf->meta_info.cptvf_meta_pool);
	return ret;
}

static __rte_always_inline int32_t __hot
cpt_dequeue_burst(struct cpt_instance *instance, uint16_t cnt,
		  void *resp[], uint8_t cc[], struct pending_queue *pqueue)
{
	struct cpt_request_info *user_req;
	struct rid *rid_e;
	int i, count, pcount;
	uint8_t ret;

	pcount = pqueue->pending_count;
	count = (cnt > pcount) ? pcount : cnt;

	for (i = 0; i < count; i++) {
		rid_e = &pqueue->rid_queue[pqueue->deq_head];
		user_req = (struct cpt_request_info *)(rid_e->rid);

		if (likely((i+1) < count))
			rte_prefetch_non_temporal((void *)rid_e[1].rid);

		ret = check_nb_command_id(user_req, instance);

		if (unlikely(ret == ERR_REQ_PENDING)) {
			/* Stop checking for completions */
			break;
		}

		/* Return completion code and op handle */
		cc[i] = (uint8_t)ret;
		resp[i] = user_req->op;
		CPT_LOG_DP_DEBUG("Request %p Op %p completed with code %d",
			   user_req, user_req->op, ret);

		MOD_INC(pqueue->deq_head, DEFAULT_CMD_QLEN);
		pqueue->pending_count -= 1;
	}

	return i;
}

#endif /* _CPT_REQUEST_MGR_H_ */
