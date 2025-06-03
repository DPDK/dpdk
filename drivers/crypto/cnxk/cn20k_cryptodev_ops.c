/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell.
 */

#include <cryptodev_pmd.h>
#include <rte_cryptodev.h>
#include <rte_hexdump.h>

#include "roc_cpt.h"
#include "roc_idev.h"

#include "cn20k_cryptodev.h"
#include "cn20k_cryptodev_ops.h"
#include "cnxk_ae.h"
#include "cnxk_cryptodev.h"
#include "cnxk_cryptodev_ops.h"
#include "cnxk_se.h"

#include "rte_pmd_cnxk_crypto.h"

static int
cn20k_cpt_crypto_adapter_ev_mdata_set(struct rte_cryptodev *dev __rte_unused, void *sess,
				      enum rte_crypto_op_type op_type,
				      enum rte_crypto_op_sess_type sess_type, void *mdata)
{
	(void)dev;
	(void)sess;
	(void)op_type;
	(void)sess_type;
	(void)mdata;

	return 0;
}

static inline struct cnxk_se_sess *
cn20k_cpt_sym_temp_sess_create(struct cnxk_cpt_qp *qp, struct rte_crypto_op *op)
{
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct rte_cryptodev_sym_session *sess;
	struct cnxk_se_sess *priv;
	int ret;

	/* Create temporary session */
	if (rte_mempool_get(qp->sess_mp, (void **)&sess) < 0)
		return NULL;

	ret = sym_session_configure(qp->lf.roc_cpt, sym_op->xform, sess, true);
	if (ret) {
		rte_mempool_put(qp->sess_mp, (void *)sess);
		goto sess_put;
	}

	priv = (void *)sess;
	sym_op->session = sess;

	return priv;

sess_put:
	rte_mempool_put(qp->sess_mp, sess);
	return NULL;
}

static inline int
cn20k_cpt_fill_inst(struct cnxk_cpt_qp *qp, struct rte_crypto_op *ops[], struct cpt_inst_s inst[],
		    struct cpt_inflight_req *infl_req)
{
	struct rte_crypto_asym_op *asym_op;
	struct rte_crypto_sym_op *sym_op;
	struct cnxk_ae_sess *ae_sess;
	struct cnxk_se_sess *sess;
	struct rte_crypto_op *op;
	uint64_t w7;
	int ret;

	const union cpt_res_s res = {
		.cn20k.compcode = CPT_COMP_NOT_DONE,
	};

	op = ops[0];

	inst[0].w0.u64 = 0;
	inst[0].w2.u64 = 0;
	inst[0].w3.u64 = 0;

	sym_op = op->sym;

	if (op->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
		if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
			sess = (struct cnxk_se_sess *)(sym_op->session);
			ret = cpt_sym_inst_fill(qp, op, sess, infl_req, &inst[0], true);
			if (unlikely(ret))
				return 0;
			w7 = sess->cpt_inst_w7;
		} else {
			sess = cn20k_cpt_sym_temp_sess_create(qp, op);
			if (unlikely(sess == NULL)) {
				plt_dp_err("Could not create temp session");
				return 0;
			}

			ret = cpt_sym_inst_fill(qp, op, sess, infl_req, &inst[0], true);
			if (unlikely(ret)) {
				sym_session_clear(op->sym->session, true);
				rte_mempool_put(qp->sess_mp, op->sym->session);
				return 0;
			}
			w7 = sess->cpt_inst_w7;
		}
	} else if (op->type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC) {

		if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
			asym_op = op->asym;
			ae_sess = (struct cnxk_ae_sess *)asym_op->session;
			ret = cnxk_ae_enqueue(qp, op, infl_req, &inst[0], ae_sess);
			if (unlikely(ret))
				return 0;
			w7 = ae_sess->cpt_inst_w7;
		} else {
			plt_dp_err("Not supported Asym op without session");
			return 0;
		}
	} else {
		plt_dp_err("Unsupported op type");
		return 0;
	}

	inst[0].res_addr = (uint64_t)&infl_req->res;
	rte_atomic_store_explicit((RTE_ATOMIC(uint64_t) *)(&infl_req->res.u64[0]),
				res.u64[0], rte_memory_order_relaxed);
	infl_req->cop = op;

	inst[0].w7.u64 = w7;

#ifdef CPT_INST_DEBUG_ENABLE
	infl_req->dptr = (uint8_t *)inst[0].dptr;
	infl_req->rptr = (uint8_t *)inst[0].rptr;
	infl_req->scatter_sz = inst[0].w6.s.scatter_sz;
	infl_req->opcode_major = inst[0].w4.s.opcode_major;

	rte_hexdump(rte_log_get_stream(), "cptr", (void *)(uint64_t)inst[0].w7.s.cptr, 128);
	plt_err("major opcode:%d", inst[0].w4.s.opcode_major);
	plt_err("minor opcode:%d", inst[0].w4.s.opcode_minor);
	plt_err("param1:%d", inst[0].w4.s.param1);
	plt_err("param2:%d", inst[0].w4.s.param2);
	plt_err("dlen:%d", inst[0].w4.s.dlen);

	cnxk_cpt_request_data_sgv2_mode_dump((void *)inst[0].dptr, 1, inst[0].w5.s.gather_sz);
	cnxk_cpt_request_data_sgv2_mode_dump((void *)inst[0].rptr, 0, inst[0].w6.s.scatter_sz);
#endif

	return 1;
}

static uint16_t
cn20k_cpt_enqueue_burst(void *qptr, struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct cpt_inflight_req *infl_req;
	uint64_t head, lmt_base, io_addr;
	uint16_t nb_allowed, count = 0;
	struct cnxk_cpt_qp *qp = qptr;
	struct pending_queue *pend_q;
	struct cpt_inst_s *inst;
	union cpt_fc_write_s fc;
	uint64_t *fc_addr;
	uint16_t lmt_id;
	int ret, i;

	pend_q = &qp->pend_q;

	const uint64_t pq_mask = pend_q->pq_mask;

	head = pend_q->head;
	nb_allowed = pending_queue_free_cnt(head, pend_q->tail, pq_mask);
	nb_ops = RTE_MIN(nb_ops, nb_allowed);

	if (unlikely(nb_ops == 0))
		return 0;

	lmt_base = qp->lmtline.lmt_base;
	io_addr = qp->lmtline.io_addr;
	fc_addr = qp->lmtline.fc_addr;

	const uint32_t fc_thresh = qp->lmtline.fc_thresh;

	ROC_LMT_BASE_ID_GET(lmt_base, lmt_id);
	inst = (struct cpt_inst_s *)lmt_base;

again:
	fc.u64[0] =
		rte_atomic_load_explicit((RTE_ATOMIC(uint64_t) *)fc_addr, rte_memory_order_relaxed);
	if (unlikely(fc.s.qsize > fc_thresh)) {
		i = 0;
		goto pend_q_commit;
	}

	for (i = 0; i < RTE_MIN(CN20K_CPT_PKTS_PER_LOOP, nb_ops); i++) {
		infl_req = &pend_q->req_queue[head];
		infl_req->op_flags = 0;

		ret = cn20k_cpt_fill_inst(qp, ops + i, &inst[i], infl_req);
		if (unlikely(ret != 1)) {
			plt_dp_err("Could not process op: %p", ops + i);
			if (i == 0)
				goto pend_q_commit;
			break;
		}

		pending_queue_advance(&head, pq_mask);
	}

	cn20k_cpt_lmtst_dual_submit(&io_addr, lmt_id, &i);

	if (nb_ops - i > 0 && i == CN20K_CPT_PKTS_PER_LOOP) {
		nb_ops -= CN20K_CPT_PKTS_PER_LOOP;
		ops += CN20K_CPT_PKTS_PER_LOOP;
		count += CN20K_CPT_PKTS_PER_LOOP;
		goto again;
	}

pend_q_commit:
	rte_atomic_thread_fence(rte_memory_order_release);

	pend_q->head = head;
	pend_q->time_out = rte_get_timer_cycles() + DEFAULT_COMMAND_TIMEOUT * rte_get_timer_hz();

	return count + i;
}

static inline void
cn20k_cpt_dequeue_post_process(struct cnxk_cpt_qp *qp, struct rte_crypto_op *cop,
			       struct cpt_inflight_req *infl_req, struct cpt_cn20k_res_s *res)
{
	const uint8_t uc_compcode = res->uc_compcode;
	const uint8_t compcode = res->compcode;

	cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;

	if (cop->type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC &&
	    cop->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
		struct cnxk_ae_sess *sess;

		sess = (struct cnxk_ae_sess *)cop->asym->session;
		if (sess->xfrm_type == RTE_CRYPTO_ASYM_XFORM_ECDH &&
		    cop->asym->ecdh.ke_type == RTE_CRYPTO_ASYM_KE_PUB_KEY_VERIFY) {
			if (likely(compcode == CPT_COMP_GOOD)) {
				if (uc_compcode == ROC_AE_ERR_ECC_POINT_NOT_ON_CURVE) {
					cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
					return;
				} else if (uc_compcode == ROC_AE_ERR_ECC_PAI) {
					cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
					return;
				}
			}
		}
	}

	if (likely(compcode == CPT_COMP_GOOD)) {
#ifdef CPT_INST_DEBUG_ENABLE
		cnxk_cpt_request_data_sgv2_mode_dump(infl_req->rptr, 0, infl_req->scatter_sz);
#endif

		if (unlikely(uc_compcode)) {
			if (uc_compcode == ROC_SE_ERR_GC_ICV_MISCOMPARE)
				cop->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
			else
				cop->status = RTE_CRYPTO_OP_STATUS_ERROR;

			plt_dp_info("Request failed with microcode error");
			plt_dp_info("MC completion code 0x%x", res->uc_compcode);
			cop->aux_flags = uc_compcode;
			goto temp_sess_free;
		}

		if (cop->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
			/* Verify authentication data if required */
			if (unlikely(infl_req->op_flags & CPT_OP_FLAGS_AUTH_VERIFY)) {
				uintptr_t *rsp = infl_req->mdata;

				compl_auth_verify(cop, (uint8_t *)rsp[0], rsp[1]);
			}
		} else if (cop->type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC) {
			struct rte_crypto_asym_op *op = cop->asym;
			uintptr_t *mdata = infl_req->mdata;
			struct cnxk_ae_sess *sess = (struct cnxk_ae_sess *)op->session;

			cnxk_ae_post_process(cop, sess, (uint8_t *)mdata[0]);
		}
	} else {
		cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
		plt_dp_info("HW completion code 0x%x", res->compcode);

		switch (compcode) {
		case CPT_COMP_INSTERR:
			plt_dp_err("Request failed with instruction error");
			break;
		case CPT_COMP_FAULT:
			plt_dp_err("Request failed with DMA fault");
			break;
		case CPT_COMP_HWERR:
			plt_dp_err("Request failed with hardware error");
			break;
		default:
			plt_dp_err("Request failed with unknown completion code");
		}
	}

temp_sess_free:
	if (unlikely(cop->sess_type == RTE_CRYPTO_OP_SESSIONLESS)) {
		if (cop->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
			sym_session_clear(cop->sym->session, true);
			rte_mempool_put(qp->sess_mp, cop->sym->session);
			cop->sym->session = NULL;
		}
	}
}

static uint16_t
cn20k_cpt_dequeue_burst(void *qptr, struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct cpt_inflight_req *infl_req;
	struct cnxk_cpt_qp *qp = qptr;
	struct pending_queue *pend_q;
	uint64_t infl_cnt, pq_tail;
	struct rte_crypto_op *cop;
	union cpt_res_s res;
	int i;

	pend_q = &qp->pend_q;

	const uint64_t pq_mask = pend_q->pq_mask;

	pq_tail = pend_q->tail;
	infl_cnt = pending_queue_infl_cnt(pend_q->head, pq_tail, pq_mask);
	nb_ops = RTE_MIN(nb_ops, infl_cnt);

	/* Ensure infl_cnt isn't read before data lands */
	rte_atomic_thread_fence(rte_memory_order_acquire);

	for (i = 0; i < nb_ops; i++) {
		infl_req = &pend_q->req_queue[pq_tail];

		res.u64[0] = rte_atomic_load_explicit(
			(RTE_ATOMIC(uint64_t) *)(&infl_req->res.u64[0]), rte_memory_order_relaxed);

		if (unlikely(res.cn20k.compcode == CPT_COMP_NOT_DONE)) {
			if (unlikely(rte_get_timer_cycles() > pend_q->time_out)) {
				plt_err("Request timed out");
				cnxk_cpt_dump_on_err(qp);
				pend_q->time_out = rte_get_timer_cycles() +
						   DEFAULT_COMMAND_TIMEOUT * rte_get_timer_hz();
			}
			break;
		}

		pending_queue_advance(&pq_tail, pq_mask);

		cop = infl_req->cop;

		ops[i] = cop;

		cn20k_cpt_dequeue_post_process(qp, cop, infl_req, &res.cn20k);

		if (unlikely(infl_req->op_flags & CPT_OP_FLAGS_METABUF))
			rte_mempool_put(qp->meta_info.pool, infl_req->mdata);
	}

	pend_q->tail = pq_tail;

	return i;
}

void
cn20k_cpt_set_enqdeq_fns(struct rte_cryptodev *dev)
{
	dev->enqueue_burst = cn20k_cpt_enqueue_burst;
	dev->dequeue_burst = cn20k_cpt_dequeue_burst;

	rte_mb();
}

static void
cn20k_cpt_dev_info_get(struct rte_cryptodev *dev, struct rte_cryptodev_info *info)
{
	if (info != NULL) {
		cnxk_cpt_dev_info_get(dev, info);
		info->driver_id = cn20k_cryptodev_driver_id;
	}
}

static int
cn20k_sym_get_raw_dp_ctx_size(struct rte_cryptodev *dev __rte_unused)
{
	return 0;
}

static int
cn20k_sym_configure_raw_dp_ctx(struct rte_cryptodev *dev, uint16_t qp_id,
			       struct rte_crypto_raw_dp_ctx *raw_dp_ctx,
			       enum rte_crypto_op_sess_type sess_type,
			       union rte_cryptodev_session_ctx session_ctx, uint8_t is_update)
{
	(void)dev;
	(void)qp_id;
	(void)raw_dp_ctx;
	(void)sess_type;
	(void)session_ctx;
	(void)is_update;
	return 0;
}

struct rte_cryptodev_ops cn20k_cpt_ops = {
	/* Device control ops */
	.dev_configure = cnxk_cpt_dev_config,
	.dev_start = cnxk_cpt_dev_start,
	.dev_stop = cnxk_cpt_dev_stop,
	.dev_close = cnxk_cpt_dev_close,
	.dev_infos_get = cn20k_cpt_dev_info_get,

	.stats_get = NULL,
	.stats_reset = NULL,
	.queue_pair_setup = cnxk_cpt_queue_pair_setup,
	.queue_pair_release = cnxk_cpt_queue_pair_release,
	.queue_pair_reset = cnxk_cpt_queue_pair_reset,

	/* Symmetric crypto ops */
	.sym_session_get_size = cnxk_cpt_sym_session_get_size,
	.sym_session_configure = cnxk_cpt_sym_session_configure,
	.sym_session_clear = cnxk_cpt_sym_session_clear,

	/* Asymmetric crypto ops */
	.asym_session_get_size = cnxk_ae_session_size_get,
	.asym_session_configure = cnxk_ae_session_cfg,
	.asym_session_clear = cnxk_ae_session_clear,

	/* Event crypto ops */
	.session_ev_mdata_set = cn20k_cpt_crypto_adapter_ev_mdata_set,
	.queue_pair_event_error_query = cnxk_cpt_queue_pair_event_error_query,

	/* Raw data-path API related operations */
	.sym_get_raw_dp_ctx_size = cn20k_sym_get_raw_dp_ctx_size,
	.sym_configure_raw_dp_ctx = cn20k_sym_configure_raw_dp_ctx,
};
