/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell.
 */

#include <cryptodev_pmd.h>
#include <eal_export.h>
#include <ethdev_driver.h>
#include <rte_cryptodev.h>
#include <rte_hexdump.h>
#include <rte_vect.h>

#include "hw/nix.h"
#include "hw/npa.h"
#include "hw/tim.h"
#include "roc_cpt.h"
#include "roc_idev.h"
#include "roc_mbox.h"
#include "roc_sso.h"
#include "roc_sso_dp.h"
#include "roc_tim.h"

#include "cn20k_cryptodev.h"
#include "cn20k_cryptodev_event_dp.h"
#include "cn20k_cryptodev_ops.h"
#include "cn20k_cryptodev_sec.h"
#include "cn20k_eventdev.h"
#include "cn20k_ipsec_la_ops.h"
#include "cn20k_tls_ops.h"
#include "cnxk_ae.h"
#include "cnxk_cryptodev.h"
#include "cnxk_cryptodev_ops.h"
#include "cnxk_eventdev.h"
#include "cnxk_se.h"

#include "rte_pmd_cnxk_crypto.h"

/* Holds information required to send crypto operations in one burst */
struct ops_burst {
	struct rte_crypto_op *op[CN20K_CPT_PKTS_PER_LOOP];
	uint64_t w2[CN20K_CPT_PKTS_PER_LOOP];
	struct cn20k_sso_hws *ws;
	struct cnxk_cpt_qp *qp;
	uint16_t nb_ops;
};

/* Holds information required to send vector of operations */
struct vec_request {
	struct cpt_inflight_req *req;
	struct rte_event_vector *vec;
	union cpt_inst_w7 w7;
	uint64_t w2;
};

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

static inline struct cnxk_ae_sess *
cn20k_cpt_asym_temp_sess_create(struct cnxk_cpt_qp *qp, struct rte_crypto_op *op)
{
	struct rte_crypto_asym_op *asym_op = op->asym;
	struct roc_cpt *roc_cpt = qp->lf.roc_cpt;
	struct rte_cryptodev_asym_session *sess;
	struct cnxk_ae_sess *priv;
	struct cnxk_cpt_vf *vf;
	union cpt_inst_w7 w7;
	struct hw_ctx_s *hwc;

	/* Create temporary session */
	if (rte_mempool_get(qp->sess_mp, (void **)&sess) < 0)
		return NULL;

	priv = (struct cnxk_ae_sess *)sess;
	if (cnxk_ae_fill_session_parameters(priv, asym_op->xform))
		goto sess_put;

	priv->lf = &qp->lf;

	if (roc_errata_cpt_hang_on_mixed_ctx_val()) {
		hwc = &priv->hw_ctx;
		hwc->w0.s.aop_valid = 1;
		hwc->w0.s.ctx_hdr_size = 0;
		hwc->w0.s.ctx_size = 1;
		hwc->w0.s.ctx_push_size = 1;

		w7.s.ctx_val = 1;
		w7.s.cptr = (uint64_t)hwc;
	}

	w7.u64 = 0;
	w7.s.egrp = roc_cpt->eng_grp[CPT_ENG_TYPE_AE];

	vf = container_of(roc_cpt, struct cnxk_cpt_vf, cpt);
	priv->cpt_inst_w7 = w7.u64;
	priv->cnxk_fpm_iova = vf->cnxk_fpm_iova;
	priv->ec_grp = vf->ec_grp;

	asym_op->session = sess;

	return priv;

sess_put:
	rte_mempool_put(qp->sess_mp, sess);
	return NULL;
}

static __rte_always_inline int __rte_hot
cpt_sec_ipsec_inst_fill(struct cnxk_cpt_qp *qp, struct rte_crypto_op *op,
			struct cn20k_sec_session *sess, struct cpt_inst_s *inst,
			struct cpt_inflight_req *infl_req)
{
	struct rte_crypto_sym_op *sym_op = op->sym;
	int ret;

	if (unlikely(sym_op->m_dst && sym_op->m_dst != sym_op->m_src)) {
		plt_dp_err("Out of place is not supported");
		return -ENOTSUP;
	}

	if (sess->ipsec.is_outbound)
		ret = process_outb_sa(&qp->lf, op, sess, &qp->meta_info, infl_req, inst);
	else
		ret = process_inb_sa(op, sess, inst, &qp->meta_info, infl_req);

	return ret;
}

static __rte_always_inline int __rte_hot
cpt_sec_tls_inst_fill(struct cnxk_cpt_qp *qp, struct rte_crypto_op *op,
		      struct cn20k_sec_session *sess, struct cpt_inst_s *inst,
		      struct cpt_inflight_req *infl_req)
{
	if (sess->tls_opt.is_write)
		return process_tls_write(&qp->lf, op, sess, &qp->meta_info, infl_req, inst);
	else
		return process_tls_read(op, sess, &qp->meta_info, infl_req, inst);
}

static __rte_always_inline int __rte_hot
cpt_sec_inst_fill(struct cnxk_cpt_qp *qp, struct rte_crypto_op *op, struct cn20k_sec_session *sess,
		  struct cpt_inst_s *inst, struct cpt_inflight_req *infl_req)
{

	if (sess->proto == RTE_SECURITY_PROTOCOL_IPSEC)
		return cpt_sec_ipsec_inst_fill(qp, op, sess, &inst[0], infl_req);
	else if (sess->proto == RTE_SECURITY_PROTOCOL_TLS_RECORD)
		return cpt_sec_tls_inst_fill(qp, op, sess, &inst[0], infl_req);

	return 0;
}

static inline int
cn20k_cpt_fill_inst(struct cnxk_cpt_qp *qp, struct rte_crypto_op *ops[], struct cpt_inst_s inst[],
		    struct cpt_inflight_req *infl_req)
{
	struct cn20k_sec_session *sec_sess;
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

	inst[0].meta_sz = 0;
	inst[0].cq_ena = 0;
	inst[0].res_addr = 0;

	sym_op = op->sym;

	if (op->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
		if (op->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION) {
			sec_sess = (struct cn20k_sec_session *)sym_op->session;
			ret = cpt_sec_inst_fill(qp, op, sec_sess, &inst[0], infl_req);
			if (unlikely(ret))
				return 0;
			w7 = sec_sess->inst.w7;
		} else if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
			sess = (struct cnxk_se_sess *)(sym_op->session);
			ret = cpt_sym_inst_fill(qp, op, sess, infl_req, &inst[0], true, true);
			if (unlikely(ret))
				return 0;
			w7 = sess->cpt_inst_w7;
		} else {
			sess = cn20k_cpt_sym_temp_sess_create(qp, op);
			if (unlikely(sess == NULL)) {
				plt_dp_err("Could not create temp session");
				return 0;
			}

			ret = cpt_sym_inst_fill(qp, op, sess, infl_req, &inst[0], true, true);
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
			ae_sess = cn20k_cpt_asym_temp_sess_create(qp, op);
			if (unlikely(ae_sess == NULL)) {
				plt_dp_err("Could not create temp session");
				return 0;
			}

			ret = cnxk_ae_enqueue(qp, op, infl_req, &inst[0], ae_sess);
			if (unlikely(ret)) {
				cnxk_ae_session_clear(NULL,
						      (struct rte_cryptodev_asym_session *)ae_sess);
				rte_mempool_put(qp->sess_mp, ae_sess);
				return 0;
			}
			w7 = ae_sess->cpt_inst_w7;
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

static int
cn20k_cpt_crypto_adapter_ev_mdata_set(struct rte_cryptodev *dev __rte_unused, void *sess,
				      enum rte_crypto_op_type op_type,
				      enum rte_crypto_op_sess_type sess_type, void *mdata)
{
	union rte_event_crypto_metadata *ec_mdata = mdata;
	struct rte_event *rsp_info;
	struct cnxk_cpt_qp *qp;
	uint64_t w2, tag_type;
	uint8_t cdev_id;
	int16_t qp_id;

	/* Get queue pair */
	cdev_id = ec_mdata->request_info.cdev_id;
	qp_id = ec_mdata->request_info.queue_pair_id;
	qp = rte_cryptodevs[cdev_id].data->queue_pairs[qp_id];

	if (!qp->ca.enabled)
		return -EINVAL;

	/* Prepare w2 */
	tag_type = qp->ca.vector_sz ? RTE_EVENT_TYPE_CRYPTODEV_VECTOR : RTE_EVENT_TYPE_CRYPTODEV;
	rsp_info = &ec_mdata->response_info;
	w2 = CNXK_CPT_INST_W2((tag_type << 28) | (rsp_info->sub_event_type << 20) |
				      rsp_info->flow_id,
			      rsp_info->sched_type, rsp_info->queue_id, 0);

	/* Set meta according to session type */
	if (op_type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
		if (sess_type == RTE_CRYPTO_OP_SECURITY_SESSION) {
			struct cn20k_sec_session *sec_sess = (struct cn20k_sec_session *)sess;

			sec_sess->qp = qp;
			sec_sess->inst.w2 = w2;
		} else if (sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
			struct cnxk_se_sess *priv;

			priv = (struct cnxk_se_sess *)sess;
			priv->qp = qp;
			priv->cpt_inst_w2 = w2;
		} else
			return -EINVAL;
	} else if (op_type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC) {
		if (sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
			struct cnxk_ae_sess *priv;

			priv = (struct cnxk_ae_sess *)sess;
			priv->qp = qp;
			priv->cpt_inst_w2 = w2;
		} else
			return -EINVAL;
	} else
		return -EINVAL;

	return 0;
}

static inline int
cn20k_ca_meta_info_extract(struct rte_crypto_op *op, struct cnxk_cpt_qp **qp, uint64_t *w2)
{
	if (op->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
		if (op->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION) {
			struct cn20k_sec_session *sec_sess;

			sec_sess = (struct cn20k_sec_session *)op->sym->session;

			*qp = sec_sess->qp;
			*w2 = sec_sess->inst.w2;
		} else if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
			struct cnxk_se_sess *priv;

			priv = (struct cnxk_se_sess *)op->sym->session;
			*qp = priv->qp;
			*w2 = priv->cpt_inst_w2;
		} else {
			union rte_event_crypto_metadata *ec_mdata;
			struct rte_event *rsp_info;
			uint8_t cdev_id;
			uint16_t qp_id;

			if (unlikely(op->private_data_offset == 0))
				return -EINVAL;
			ec_mdata = (union rte_event_crypto_metadata *)((uint8_t *)op +
								       op->private_data_offset);
			rsp_info = &ec_mdata->response_info;
			cdev_id = ec_mdata->request_info.cdev_id;
			qp_id = ec_mdata->request_info.queue_pair_id;
			*qp = rte_cryptodevs[cdev_id].data->queue_pairs[qp_id];
			*w2 = CNXK_CPT_INST_W2((RTE_EVENT_TYPE_CRYPTODEV << 28) | rsp_info->flow_id,
					       rsp_info->sched_type, rsp_info->queue_id, 0);
		}
	} else if (op->type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC) {
		if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
			struct cnxk_ae_sess *priv;

			priv = (struct cnxk_ae_sess *)op->asym->session;
			*qp = priv->qp;
			*w2 = priv->cpt_inst_w2;
		} else {
			union rte_event_crypto_metadata *ec_mdata;
			struct rte_event *rsp_info;
			uint8_t cdev_id;
			uint16_t qp_id;

			if (unlikely(op->private_data_offset == 0))
				return -EINVAL;
			ec_mdata = (union rte_event_crypto_metadata *)((uint8_t *)op +
								       op->private_data_offset);
			rsp_info = &ec_mdata->response_info;
			cdev_id = ec_mdata->request_info.cdev_id;
			qp_id = ec_mdata->request_info.queue_pair_id;
			*qp = rte_cryptodevs[cdev_id].data->queue_pairs[qp_id];
			*w2 = CNXK_CPT_INST_W2((RTE_EVENT_TYPE_CRYPTODEV << 28) | rsp_info->flow_id,
					       rsp_info->sched_type, rsp_info->queue_id, 0);
		}
	} else
		return -EINVAL;

	return 0;
}

static inline void
cn20k_cpt_vec_inst_fill(struct vec_request *vec_req, struct cpt_inst_s *inst,
			struct cnxk_cpt_qp *qp, union cpt_inst_w7 w7)
{
	const union cpt_res_s res = {.cn20k.compcode = CPT_COMP_NOT_DONE};
	struct cpt_inflight_req *infl_req = vec_req->req;

	const union cpt_inst_w4 w4 = {
		.s.opcode_major = ROC_SE_MAJOR_OP_MISC,
		.s.opcode_minor = ROC_SE_MISC_MINOR_OP_PASSTHROUGH,
		.s.param1 = 1,
		.s.param2 = 1,
		.s.dlen = 0,
	};

	w7.s.egrp = ROC_CPT_DFLT_ENG_GRP_SE;

	infl_req->vec = vec_req->vec;
	infl_req->qp = qp;

	inst->res_addr = (uint64_t)&infl_req->res;
	rte_atomic_store_explicit((RTE_ATOMIC(uint64_t) *)(&infl_req->res.u64[0]), res.u64[0],
				  rte_memory_order_relaxed);

	inst->w0.u64 = 0;
	inst->w2.u64 = vec_req->w2;
	inst->w3.u64 = CNXK_CPT_INST_W3(1, infl_req);
	inst->w4.u64 = w4.u64;
	inst->w5.u64 = 0;
	inst->w6.u64 = 0;
	inst->w7.u64 = w7.u64;
}

static void
cn20k_cpt_vec_pkt_submission_timeout_handle(void)
{
	plt_dp_err("Vector packet submission timedout");
	abort();
}

static inline void
cn20k_cpt_vec_submit(struct vec_request vec_tbl[], uint16_t vec_tbl_len, struct cnxk_cpt_qp *qp)
{
	uint64_t lmt_base, lmt_id, io_addr;
	union cpt_fc_write_s fc;
	struct cpt_inst_s *inst;
	uint16_t burst_size;
	uint64_t *fc_addr;
	int i;

	if (vec_tbl_len == 0)
		return;

	const uint32_t fc_thresh = qp->lmtline.fc_thresh;
	/*
	 * Use 10 mins timeout for the poll. It is not possible to recover from partial submission
	 * of vector packet. Actual packets for processing are submitted to CPT prior to this
	 * routine. Hence, any failure for submission of vector packet would indicate an
	 * unrecoverable error for the application.
	 */
	const uint64_t timeout = rte_get_timer_cycles() + 10 * 60 * rte_get_timer_hz();

	lmt_base = qp->lmtline.lmt_base;
	io_addr = qp->lmtline.io_addr;
	fc_addr = qp->lmtline.fc_addr;
	ROC_LMT_BASE_ID_GET(lmt_base, lmt_id);
	inst = (struct cpt_inst_s *)lmt_base;

again:
	burst_size = RTE_MIN(CN20K_PKTS_PER_STEORL, vec_tbl_len);
	for (i = 0; i < burst_size; i++)
		cn20k_cpt_vec_inst_fill(&vec_tbl[i], &inst[i], qp, vec_tbl[0].w7);

	do {
		fc.u64[0] = rte_atomic_load_explicit((RTE_ATOMIC(uint64_t) *)fc_addr,
						     rte_memory_order_relaxed);
		if (likely(fc.s.qsize < fc_thresh))
			break;
		if (unlikely(rte_get_timer_cycles() > timeout))
			cn20k_cpt_vec_pkt_submission_timeout_handle();
	} while (true);

	cn20k_cpt_lmtst_dual_submit(&io_addr, lmt_id, &i);

	vec_tbl_len -= i;

	if (vec_tbl_len > 0) {
		vec_tbl += i;
		goto again;
	}
}

static inline int
ca_lmtst_vec_submit(struct ops_burst *burst, struct vec_request vec_tbl[], uint16_t *vec_tbl_len)
{
	struct cpt_inflight_req *infl_reqs[CN20K_CPT_PKTS_PER_LOOP];
	uint16_t lmt_id, len = *vec_tbl_len;
	struct cpt_inst_s *inst, *inst_base;
	struct cpt_inflight_req *infl_req;
	struct rte_event_vector *vec;
	uint64_t lmt_base, io_addr;
	union cpt_fc_write_s fc;
	struct cnxk_cpt_qp *qp;
	uint64_t *fc_addr;
	int ret, i, vi;

	qp = burst->qp;

	lmt_base = qp->lmtline.lmt_base;
	io_addr = qp->lmtline.io_addr;
	fc_addr = qp->lmtline.fc_addr;

	const uint32_t fc_thresh = qp->lmtline.fc_thresh;

	ROC_LMT_BASE_ID_GET(lmt_base, lmt_id);
	inst_base = (struct cpt_inst_s *)lmt_base;

#ifdef CNXK_CRYPTODEV_DEBUG
	if (unlikely(!qp->ca.enabled)) {
		rte_errno = EINVAL;
		return 0;
	}
#endif

	/* Perform fc check before putting packets into vectors */
	fc.u64[0] =
		rte_atomic_load_explicit((RTE_ATOMIC(uint64_t) *)fc_addr, rte_memory_order_relaxed);
	if (unlikely(fc.s.qsize > fc_thresh)) {
		rte_errno = EAGAIN;
		return 0;
	}

	if (unlikely(rte_mempool_get_bulk(qp->ca.req_mp, (void **)infl_reqs, burst->nb_ops))) {
		rte_errno = ENOMEM;
		return 0;
	}

	for (i = 0; i < burst->nb_ops; i++) {
		inst = &inst_base[i];
		infl_req = infl_reqs[i];
		infl_req->op_flags = 0;

		ret = cn20k_cpt_fill_inst(qp, &burst->op[i], inst, infl_req);
		if (unlikely(ret != 1)) {
			plt_cpt_dbg("Could not process op: %p", burst->op[i]);
			if (i != 0)
				goto submit;
			else
				goto put;
		}

		infl_req->res.cn20k.compcode = CPT_COMP_NOT_DONE;
		infl_req->qp = qp;
		inst->w3.u64 = 0x1;

		/* Lookup for existing vector by w2 */
		for (vi = len - 1; vi >= 0; vi--) {
			if (vec_tbl[vi].w2 != burst->w2[i])
				continue;
			vec = vec_tbl[vi].vec;
			if (unlikely(vec->nb_elem == qp->ca.vector_sz))
				continue;
			vec->ptrs[vec->nb_elem++] = infl_req;
			goto next_op; /* continue outer loop */
		}

		/* No available vectors found, allocate a new one */
		if (unlikely(rte_mempool_get(qp->ca.vector_mp, (void **)&vec_tbl[len].vec))) {
			rte_errno = ENOMEM;
			if (i != 0)
				goto submit;
			else
				goto put;
		}
		/* Also preallocate in-flight request, that will be used to
		 * submit misc passthrough instruction
		 */
		if (unlikely(rte_mempool_get(qp->ca.req_mp, (void **)&vec_tbl[len].req))) {
			rte_mempool_put(qp->ca.vector_mp, vec_tbl[len].vec);
			rte_errno = ENOMEM;
			if (i != 0)
				goto submit;
			else
				goto put;
		}
		vec_tbl[len].w2 = burst->w2[i];
		vec_tbl[len].vec->ptrs[0] = infl_req;
		vec_tbl[len].vec->nb_elem = 1;
		len++;

next_op:;
	}

	/* Submit operations in burst */
submit:
	if (CNXK_TT_FROM_TAG(burst->ws->gw_rdata) == SSO_TT_ORDERED)
		roc_sso_hws_head_wait(burst->ws->base);

	cn20k_cpt_lmtst_dual_submit(&io_addr, lmt_id, &i);

	/* Store w7 of last successfully filled instruction */
	inst = &inst_base[2 * (i - 1)];
	vec_tbl[0].w7 = inst->w7;

put:
	if (i != burst->nb_ops)
		rte_mempool_put_bulk(qp->ca.req_mp, (void *)&infl_reqs[i], burst->nb_ops - i);

	*vec_tbl_len = len;

	return i;
}

static inline uint16_t
ca_lmtst_burst_submit(struct ops_burst *burst)
{
	struct cpt_inflight_req *infl_reqs[CN20K_CPT_PKTS_PER_LOOP];
	struct cpt_inst_s *inst, *inst_base;
	struct cpt_inflight_req *infl_req;
	uint64_t lmt_base, io_addr;
	union cpt_fc_write_s fc;
	struct cnxk_cpt_qp *qp;
	uint64_t *fc_addr;
	uint16_t lmt_id;
	int ret, i, j;

	qp = burst->qp;

	lmt_base = qp->lmtline.lmt_base;
	io_addr = qp->lmtline.io_addr;
	fc_addr = qp->lmtline.fc_addr;

	const uint32_t fc_thresh = qp->lmtline.fc_thresh;

	ROC_LMT_BASE_ID_GET(lmt_base, lmt_id);
	inst_base = (struct cpt_inst_s *)lmt_base;

#ifdef CNXK_CRYPTODEV_DEBUG
	if (unlikely(!qp->ca.enabled)) {
		rte_errno = EINVAL;
		return 0;
	}
#endif

	if (unlikely(rte_mempool_get_bulk(qp->ca.req_mp, (void **)infl_reqs, burst->nb_ops))) {
		rte_errno = ENOMEM;
		return 0;
	}

	for (i = 0; i < burst->nb_ops; i++) {
		inst = &inst_base[i];
		infl_req = infl_reqs[i];
		infl_req->op_flags = 0;

		ret = cn20k_cpt_fill_inst(qp, &burst->op[i], inst, infl_req);
		if (unlikely(ret != 1)) {
			plt_dp_dbg("Could not process op: %p", burst->op[i]);
			if (i != 0)
				goto submit;
			else
				goto put;
		}

		infl_req->res.cn20k.compcode = CPT_COMP_NOT_DONE;
		infl_req->qp = qp;
		inst->w0.u64 = 0;
		inst->res_addr = (uint64_t)&infl_req->res;
		inst->w2.u64 = burst->w2[i];
		inst->w3.u64 = CNXK_CPT_INST_W3(1, infl_req);
	}

	fc.u64[0] =
		rte_atomic_load_explicit((RTE_ATOMIC(uint64_t) *)fc_addr, rte_memory_order_relaxed);
	if (unlikely(fc.s.qsize > fc_thresh)) {
		rte_errno = EAGAIN;
		for (j = 0; j < i; j++) {
			infl_req = infl_reqs[j];
			if (unlikely(infl_req->op_flags & CPT_OP_FLAGS_METABUF))
				rte_mempool_put(qp->meta_info.pool, infl_req->mdata);
		}
		i = 0;
		goto put;
	}

submit:
	if (CNXK_TT_FROM_TAG(burst->ws->gw_rdata) == SSO_TT_ORDERED)
		roc_sso_hws_head_wait(burst->ws->base);

	cn20k_cpt_lmtst_dual_submit(&io_addr, lmt_id, &i);

put:
	if (unlikely(i != burst->nb_ops))
		rte_mempool_put_bulk(qp->ca.req_mp, (void *)&infl_reqs[i], burst->nb_ops - i);

	return i;
}

RTE_EXPORT_INTERNAL_SYMBOL(cn20k_cpt_crypto_adapter_enqueue)
uint16_t __rte_hot
cn20k_cpt_crypto_adapter_enqueue(void *ws, struct rte_event ev[], uint16_t nb_events)
{
	uint16_t submitted, count = 0, vec_tbl_len = 0;
	struct vec_request vec_tbl[nb_events];
	struct rte_crypto_op *op;
	struct ops_burst burst;
	struct cnxk_cpt_qp *qp;
	bool is_vector = false;
	uint64_t w2;
	int ret, i;

	burst.ws = ws;
	burst.qp = NULL;
	burst.nb_ops = 0;

	for (i = 0; i < nb_events; i++) {
		op = ev[i].event_ptr;
		ret = cn20k_ca_meta_info_extract(op, &qp, &w2);
		if (unlikely(ret)) {
			rte_errno = EINVAL;
			goto vec_submit;
		}

		/* Queue pair change check */
		if (qp != burst.qp) {
			if (burst.nb_ops) {
				if (is_vector) {
					submitted =
						ca_lmtst_vec_submit(&burst, vec_tbl, &vec_tbl_len);
					/*
					 * Vector submission is required on qp change, but not in
					 * other cases, since we could send several vectors per
					 * lmtst instruction only for same qp
					 */
					cn20k_cpt_vec_submit(vec_tbl, vec_tbl_len, burst.qp);
					vec_tbl_len = 0;
				} else {
					submitted = ca_lmtst_burst_submit(&burst);
				}
				count += submitted;
				if (unlikely(submitted != burst.nb_ops))
					goto vec_submit;
				burst.nb_ops = 0;
			}
			is_vector = qp->ca.vector_sz;
			burst.qp = qp;
		}
		burst.w2[burst.nb_ops] = w2;
		burst.op[burst.nb_ops] = op;

		/* Max nb_ops per burst check */
		if (++burst.nb_ops == CN20K_CPT_PKTS_PER_LOOP) {
			if (is_vector)
				submitted = ca_lmtst_vec_submit(&burst, vec_tbl, &vec_tbl_len);
			else
				submitted = ca_lmtst_burst_submit(&burst);
			count += submitted;
			if (unlikely(submitted != burst.nb_ops))
				goto vec_submit;
			burst.nb_ops = 0;
		}
	}
	/* Submit the rest of crypto operations */
	if (burst.nb_ops) {
		if (is_vector)
			count += ca_lmtst_vec_submit(&burst, vec_tbl, &vec_tbl_len);
		else
			count += ca_lmtst_burst_submit(&burst);
	}

vec_submit:
	cn20k_cpt_vec_submit(vec_tbl, vec_tbl_len, burst.qp);
	return count;
}

static inline void
cn20k_cpt_ipsec_post_process(struct rte_crypto_op *cop, struct cpt_cn20k_res_s *res)
{
	struct rte_mbuf *mbuf = cop->sym->m_src;
	const uint16_t m_len = res->rlen;

	switch (res->uc_compcode) {
	case ROC_IE_OW_UCC_SUCCESS_PKT_IP_BADCSUM:
		mbuf->ol_flags &= ~RTE_MBUF_F_RX_IP_CKSUM_GOOD;
		mbuf->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
		break;
	case ROC_IE_OW_UCC_SUCCESS_PKT_L4_GOODCSUM:
		mbuf->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_GOOD;
		break;
	case ROC_IE_OW_UCC_SUCCESS_PKT_L4_BADCSUM:
		mbuf->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_GOOD;
		break;
	case ROC_IE_OW_UCC_SUCCESS_PKT_IP_GOODCSUM:
		break;
	case ROC_IE_OW_UCC_SUCCESS_SA_SOFTEXP_FIRST:
	case ROC_IE_OW_UCC_SUCCESS_SA_SOFTEXP_AGAIN:
		cop->aux_flags = RTE_CRYPTO_OP_AUX_FLAGS_IPSEC_SOFT_EXPIRY;
		break;
	default:
		cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
		cop->aux_flags = res->uc_compcode;
		return;
	}

	if (mbuf->next == NULL)
		mbuf->data_len = m_len;

	mbuf->pkt_len = m_len;
}

static inline void
cn20k_cpt_tls12_trim_mac(struct rte_crypto_op *cop, struct cpt_cn20k_res_s *res, uint8_t mac_len)
{
	uint32_t pad_len, trim_len, mac_offset, pad_offset;
	struct rte_mbuf *mbuf = cop->sym->m_src;
	uint16_t m_len = res->rlen, len_to_trim;
	struct rte_mbuf *seg;
	uint8_t pad_res = 0;
	uint8_t pad_val;
	uint32_t i;

	pad_val = ((res->spi >> 16) & 0xff);
	pad_len = pad_val + 1;
	trim_len = pad_len + mac_len;
	mac_offset = m_len - trim_len;
	pad_offset = mac_offset + mac_len;

	/* Handle Direct Mode */
	if (mbuf->next == NULL) {
		uint8_t *ptr = rte_pktmbuf_mtod_offset(mbuf, uint8_t *, pad_offset);

		for (i = 0; i < pad_len; i++)
			pad_res |= ptr[i] ^ pad_val;

		if (pad_res) {
			cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
			cop->aux_flags = res->uc_compcode;
		}
		mbuf->pkt_len = m_len - trim_len;
		mbuf->data_len = m_len - trim_len;

		return;
	}

	/* Handle SG mode */
	seg = mbuf;
	while (mac_offset >= seg->data_len) {
		mac_offset -= seg->data_len;
		seg = seg->next;
	}

	pad_offset = mac_offset + mac_len;
	while (pad_offset >= seg->data_len) {
		pad_offset -= seg->data_len;
		seg = seg->next;
	}

	while (pad_len != 0) {
		uint8_t *ptr = rte_pktmbuf_mtod_offset(seg, uint8_t *, pad_offset);
		uint8_t len = RTE_MIN(seg->data_len - pad_offset, pad_len);

		for (i = 0; i < len; i++)
			pad_res |= ptr[i] ^ pad_val;

		pad_offset = 0;
		pad_len -= len;
		seg = seg->next;
	}

	if (pad_res) {
		cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
		cop->aux_flags = res->uc_compcode;
	}

	len_to_trim = mbuf->pkt_len - (m_len - trim_len);

	pktmbuf_trim_chain(mbuf, len_to_trim);
}

/* TLS-1.3:
 * Read from last until a non-zero value is encountered.
 * Return the non zero value as the content type.
 * Remove the MAC and content type and padding bytes.
 */
static inline void
cn20k_cpt_tls13_trim_mac(struct rte_crypto_op *cop, struct cpt_cn20k_res_s *res)
{
	uint16_t m_len = res->rlen, len_to_trim;
	struct rte_mbuf *mbuf = cop->sym->m_src;
	struct rte_mbuf *seg = mbuf;
	uint8_t *ptr, type = 0x0;
	int len, i;

	while (m_len && !type) {
		len = m_len;
		seg = mbuf;

		/* get the last seg */
		while (len > seg->data_len) {
			len -= seg->data_len;
			seg = seg->next;
		}

		/* walkthrough from last until a non zero value is found */
		ptr = rte_pktmbuf_mtod(seg, uint8_t *);
		i = len;
		while (i && (ptr[--i] == 0))
			;

		type = ptr[i];
		m_len -= len;
	}

	len_to_trim = mbuf->pkt_len - i;

	if (type) {
		pktmbuf_trim_chain(mbuf, len_to_trim);
		cop->param1.tls_record.content_type = type;
	} else
		cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
}

static inline void
cn20k_cpt_tls_post_process(struct rte_crypto_op *cop, struct cpt_cn20k_res_s *res,
			   struct cn20k_sec_session *sess)
{
	struct cn20k_tls_opt tls_opt = sess->tls_opt;
	struct rte_mbuf *mbuf = cop->sym->m_src;
	uint16_t m_len = res->rlen;

	if (!res->uc_compcode) {
		if (mbuf->next == NULL)
			mbuf->data_len = m_len;
		mbuf->pkt_len = m_len;
		cop->param1.tls_record.content_type = (res->spi >> 24) & 0xff;
		return;
	}

	/* Any error other than post process */
	if (res->uc_compcode != ROC_SE_ERR_SSL_POST_PROCESS) {
		cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
		cop->aux_flags = res->uc_compcode;
		plt_err("crypto op failed with UC compcode: 0x%x", res->uc_compcode);
		return;
	}

	/* Extra padding scenario: Verify padding. Remove padding and MAC */
	if (tls_opt.tls_ver != RTE_SECURITY_VERSION_TLS_1_3)
		cn20k_cpt_tls12_trim_mac(cop, res, (uint8_t)tls_opt.mac_len);
	else
		cn20k_cpt_tls13_trim_mac(cop, res);
}

static inline void
cn20k_cpt_sec_post_process(struct rte_crypto_op *cop, struct cpt_cn20k_res_s *res)
{
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct cn20k_sec_session *sess;

	sess = sym_op->session;
	if (sess->proto == RTE_SECURITY_PROTOCOL_IPSEC)
		cn20k_cpt_ipsec_post_process(cop, res);
	else if (sess->proto == RTE_SECURITY_PROTOCOL_TLS_RECORD)
		cn20k_cpt_tls_post_process(cop, res, sess);
}

static inline void
cn20k_cpt_dequeue_post_process(struct cnxk_cpt_qp *qp, struct rte_crypto_op *cop,
			       struct cpt_inflight_req *infl_req, struct cpt_cn20k_res_s *res)
{
	const uint8_t uc_compcode = res->uc_compcode;
	const uint8_t compcode = res->compcode;

	cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;

	if (cop->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC &&
	    cop->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION) {
		if (likely(compcode == CPT_COMP_GOOD || compcode == CPT_COMP_WARN)) {
			/* Success with additional info */
			cn20k_cpt_sec_post_process(cop, res);
		} else {
			cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
			plt_dp_info("HW completion code 0x%x", res->compcode);
			if (compcode == CPT_COMP_GOOD) {
				plt_dp_info("Request failed with microcode error");
				plt_dp_info("MC completion code 0x%x", uc_compcode);
			}
		}

		return;
	} else if (cop->type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC &&
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
		if (cop->type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC) {
			cnxk_ae_session_clear(NULL, cop->asym->session);
			rte_mempool_put(qp->sess_mp, cop->asym->session);
			cop->asym->session = NULL;
		}
	}
}

RTE_EXPORT_INTERNAL_SYMBOL(cn20k_cpt_crypto_adapter_dequeue)
uintptr_t
cn20k_cpt_crypto_adapter_dequeue(uintptr_t get_work1)
{
	struct cpt_inflight_req *infl_req;
	struct rte_crypto_op *cop;
	struct cnxk_cpt_qp *qp;
	union cpt_res_s res;

	infl_req = (struct cpt_inflight_req *)(get_work1);
	cop = infl_req->cop;
	qp = infl_req->qp;

	res.u64[0] = rte_atomic_load_explicit((RTE_ATOMIC(uint64_t) *)(&infl_req->res.u64[0]),
					      rte_memory_order_relaxed);

	cn20k_cpt_dequeue_post_process(qp, infl_req->cop, infl_req, &res.cn20k);

	if (unlikely(infl_req->op_flags & CPT_OP_FLAGS_METABUF))
		rte_mempool_put(qp->meta_info.pool, infl_req->mdata);

	rte_mempool_put(qp->ca.req_mp, infl_req);
	return (uintptr_t)cop;
}

RTE_EXPORT_INTERNAL_SYMBOL(cn20k_cpt_crypto_adapter_vector_dequeue)
uintptr_t
cn20k_cpt_crypto_adapter_vector_dequeue(uintptr_t get_work1)
{
	struct cpt_inflight_req *infl_req, *vec_infl_req;
	struct rte_mempool *meta_mp, *req_mp;
	struct rte_event_vector *vec;
	struct rte_crypto_op *cop;
	struct cnxk_cpt_qp *qp;
	union cpt_res_s res;
	int i;

	vec_infl_req = (struct cpt_inflight_req *)(get_work1);

	vec = vec_infl_req->vec;
	qp = vec_infl_req->qp;
	meta_mp = qp->meta_info.pool;
	req_mp = qp->ca.req_mp;

#ifdef CNXK_CRYPTODEV_DEBUG
	res.u64[0] = rte_atomic_load_explicit((RTE_ATOMIC(uint64_t) *)(&vec_infl_req->res.u64[0]),
					      rte_memory_order_relaxed);
	PLT_ASSERT(res.cn20k.compcode == CPT_COMP_GOOD);
	PLT_ASSERT(res.cn20k.uc_compcode == 0);
#endif

	for (i = 0; i < vec->nb_elem; i++) {
		infl_req = vec->ptrs[i];
		cop = infl_req->cop;

		res.u64[0] = rte_atomic_load_explicit(
			(RTE_ATOMIC(uint64_t) *)(&infl_req->res.u64[0]), rte_memory_order_relaxed);
		cn20k_cpt_dequeue_post_process(qp, cop, infl_req, &res.cn20k);

		vec->ptrs[i] = cop;
		if (unlikely(infl_req->op_flags & CPT_OP_FLAGS_METABUF))
			rte_mempool_put(meta_mp, infl_req->mdata);

		rte_mempool_put(req_mp, infl_req);
	}

	rte_mempool_put(req_mp, vec_infl_req);

	return (uintptr_t)vec;
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

static inline int
cn20k_cpt_raw_fill_inst(struct cnxk_iov *iov, struct cnxk_cpt_qp *qp,
			struct cnxk_sym_dp_ctx *dp_ctx, struct cpt_inst_s inst[],
			struct cpt_inflight_req *infl_req, void *opaque)
{
	struct cnxk_se_sess *sess;
	int ret;

	const union cpt_res_s res = {
		.cn20k.compcode = CPT_COMP_NOT_DONE,
	};

	inst[0].w0.u64 = 0;
	inst[0].w2.u64 = 0;
	inst[0].w3.u64 = 0;

	sess = dp_ctx->sess;

	switch (sess->dp_thr_type) {
	case CPT_DP_THREAD_TYPE_PT:
		ret = fill_raw_passthrough_params(iov, inst);
		break;
	case CPT_DP_THREAD_TYPE_FC_CHAIN:
		ret = fill_raw_fc_params(iov, sess, &qp->meta_info, infl_req, &inst[0], false,
					 false, true);
		break;
	case CPT_DP_THREAD_TYPE_FC_AEAD:
		ret = fill_raw_fc_params(iov, sess, &qp->meta_info, infl_req, &inst[0], false, true,
					 true);
		break;
	case CPT_DP_THREAD_AUTH_ONLY:
		ret = fill_raw_digest_params(iov, sess, &qp->meta_info, infl_req, &inst[0], true,
					     true);
		break;
	default:
		ret = -EINVAL;
	}

	if (unlikely(ret))
		return 0;

	inst[0].res_addr = (uint64_t)&infl_req->res;
	rte_atomic_store_explicit((RTE_ATOMIC(uint64_t) *)(&infl_req->res.u64[0]), res.u64[0],
			rte_memory_order_relaxed);
	infl_req->opaque = opaque;

	inst[0].w7.u64 = sess->cpt_inst_w7;

	return 1;
}

static uint32_t
cn20k_cpt_raw_enqueue_burst(void *qpair, uint8_t *drv_ctx, struct rte_crypto_sym_vec *vec,
			    union rte_crypto_sym_ofs ofs, void *user_data[], int *enqueue_status)
{
	uint16_t lmt_id, nb_allowed, nb_ops = vec->num;
	struct cpt_inflight_req *infl_req;
	uint64_t lmt_base, io_addr, head;
	struct cnxk_cpt_qp *qp = qpair;
	struct cnxk_sym_dp_ctx *dp_ctx;
	struct pending_queue *pend_q;
	uint32_t count = 0, index;
	union cpt_fc_write_s fc;
	struct cpt_inst_s *inst;
	uint64_t *fc_addr;
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

	dp_ctx = (struct cnxk_sym_dp_ctx *)drv_ctx;
again:
	fc.u64[0] =
		rte_atomic_load_explicit((RTE_ATOMIC(uint64_t) *)fc_addr, rte_memory_order_relaxed);
	if (unlikely(fc.s.qsize > fc_thresh)) {
		i = 0;
		goto pend_q_commit;
	}

	for (i = 0; i < RTE_MIN(CN20K_CPT_PKTS_PER_LOOP, nb_ops); i++) {
		struct cnxk_iov iov;

		index = count + i;
		infl_req = &pend_q->req_queue[head];
		infl_req->op_flags = 0;

		cnxk_raw_burst_to_iov(vec, &ofs, index, &iov);
		ret = cn20k_cpt_raw_fill_inst(&iov, qp, dp_ctx, &inst[i], infl_req,
					      user_data[index]);
		if (unlikely(ret != 1)) {
			plt_dp_err("Could not process vec: %d", index);
			if (i == 0 && count == 0)
				return -1;
			else if (i == 0)
				goto pend_q_commit;
			else
				break;
		}
		pending_queue_advance(&head, pq_mask);
	}

	cn20k_cpt_lmtst_dual_submit(&io_addr, lmt_id, &i);

	if (nb_ops - i > 0 && i == CN20K_CPT_PKTS_PER_LOOP) {
		nb_ops -= i;
		count += i;
		goto again;
	}

pend_q_commit:
	rte_atomic_thread_fence(rte_memory_order_release);

	pend_q->head = head;
	pend_q->time_out = rte_get_timer_cycles() + DEFAULT_COMMAND_TIMEOUT * rte_get_timer_hz();

	*enqueue_status = 1;
	return count + i;
}

static int
cn20k_cpt_raw_enqueue(void *qpair, uint8_t *drv_ctx, struct rte_crypto_vec *data_vec,
		      uint16_t n_data_vecs, union rte_crypto_sym_ofs ofs,
		      struct rte_crypto_va_iova_ptr *iv, struct rte_crypto_va_iova_ptr *digest,
		      struct rte_crypto_va_iova_ptr *aad_or_auth_iv, void *user_data)
{
	struct cpt_inflight_req *infl_req;
	uint64_t lmt_base, io_addr, head;
	struct cnxk_cpt_qp *qp = qpair;
	struct cnxk_sym_dp_ctx *dp_ctx;
	uint16_t lmt_id, nb_allowed;
	struct cpt_inst_s *inst;
	union cpt_fc_write_s fc;
	struct cnxk_iov iov;
	uint64_t *fc_addr;
	int ret, i = 1;

	struct pending_queue *pend_q = &qp->pend_q;
	const uint64_t pq_mask = pend_q->pq_mask;
	const uint32_t fc_thresh = qp->lmtline.fc_thresh;

	head = pend_q->head;
	nb_allowed = pending_queue_free_cnt(head, pend_q->tail, pq_mask);

	if (unlikely(nb_allowed == 0))
		return -1;

	cnxk_raw_to_iov(data_vec, n_data_vecs, &ofs, iv, digest, aad_or_auth_iv, &iov);

	lmt_base = qp->lmtline.lmt_base;
	io_addr = qp->lmtline.io_addr;
	fc_addr = qp->lmtline.fc_addr;

	ROC_LMT_BASE_ID_GET(lmt_base, lmt_id);
	inst = (struct cpt_inst_s *)lmt_base;

	fc.u64[0] =
		rte_atomic_load_explicit((RTE_ATOMIC(uint64_t) *)fc_addr, rte_memory_order_relaxed);
	if (unlikely(fc.s.qsize > fc_thresh))
		return -1;

	dp_ctx = (struct cnxk_sym_dp_ctx *)drv_ctx;
	infl_req = &pend_q->req_queue[head];
	infl_req->op_flags = 0;

	ret = cn20k_cpt_raw_fill_inst(&iov, qp, dp_ctx, &inst[0], infl_req, user_data);
	if (unlikely(ret != 1)) {
		plt_dp_err("Could not process vec");
		return -1;
	}

	pending_queue_advance(&head, pq_mask);

	cn20k_cpt_lmtst_dual_submit(&io_addr, lmt_id, &i);

	pend_q->head = head;
	pend_q->time_out = rte_get_timer_cycles() + DEFAULT_COMMAND_TIMEOUT * rte_get_timer_hz();

	return 1;
}

static inline int
cn20k_cpt_raw_dequeue_post_process(struct cpt_cn20k_res_s *res)
{
	const uint8_t uc_compcode = res->uc_compcode;
	const uint8_t compcode = res->compcode;
	int ret = 1;

	if (likely(compcode == CPT_COMP_GOOD)) {
		if (unlikely(uc_compcode))
			plt_dp_info("Request failed with microcode error: 0x%x", res->uc_compcode);
		else
			ret = 0;
	}

	return ret;
}

static uint32_t
cn20k_cpt_sym_raw_dequeue_burst(void *qptr, uint8_t *drv_ctx,
				rte_cryptodev_raw_get_dequeue_count_t get_dequeue_count,
				uint32_t max_nb_to_dequeue,
				rte_cryptodev_raw_post_dequeue_t post_dequeue, void **out_user_data,
				uint8_t is_user_data_array, uint32_t *n_success,
				int *dequeue_status)
{
	struct cpt_inflight_req *infl_req;
	struct cnxk_cpt_qp *qp = qptr;
	struct pending_queue *pend_q;
	uint64_t infl_cnt, pq_tail;
	union cpt_res_s res;
	int is_op_success;
	uint16_t nb_ops;
	void *opaque;
	int i = 0;

	pend_q = &qp->pend_q;

	const uint64_t pq_mask = pend_q->pq_mask;

	RTE_SET_USED(drv_ctx);
	pq_tail = pend_q->tail;
	infl_cnt = pending_queue_infl_cnt(pend_q->head, pq_tail, pq_mask);

	/* Ensure infl_cnt isn't read before data lands */
	rte_atomic_thread_fence(rte_memory_order_acquire);

	infl_req = &pend_q->req_queue[pq_tail];

	opaque = infl_req->opaque;
	if (get_dequeue_count)
		nb_ops = get_dequeue_count(opaque);
	else
		nb_ops = max_nb_to_dequeue;
	nb_ops = RTE_MIN(nb_ops, infl_cnt);

	for (i = 0; i < nb_ops; i++) {
		is_op_success = 0;
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

		if (!cn20k_cpt_raw_dequeue_post_process(&res.cn20k)) {
			is_op_success = 1;
			*n_success += 1;
		}

		if (is_user_data_array) {
			out_user_data[i] = infl_req->opaque;
			post_dequeue(out_user_data[i], i, is_op_success);
		} else {
			if (i == 0)
				out_user_data[0] = opaque;
			post_dequeue(out_user_data[0], i, is_op_success);
		}

		if (unlikely(infl_req->op_flags & CPT_OP_FLAGS_METABUF))
			rte_mempool_put(qp->meta_info.pool, infl_req->mdata);
	}

	pend_q->tail = pq_tail;
	*dequeue_status = 1;

	return i;
}

static void *
cn20k_cpt_sym_raw_dequeue(void *qptr, uint8_t *drv_ctx, int *dequeue_status,
			  enum rte_crypto_op_status *op_status)
{
	struct cpt_inflight_req *infl_req;
	struct cnxk_cpt_qp *qp = qptr;
	struct pending_queue *pend_q;
	uint64_t pq_tail;
	union cpt_res_s res;
	void *opaque = NULL;

	pend_q = &qp->pend_q;

	const uint64_t pq_mask = pend_q->pq_mask;

	RTE_SET_USED(drv_ctx);

	pq_tail = pend_q->tail;

	rte_atomic_thread_fence(rte_memory_order_acquire);

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
		goto exit;
	}

	pending_queue_advance(&pq_tail, pq_mask);

	opaque = infl_req->opaque;

	if (!cn20k_cpt_raw_dequeue_post_process(&res.cn20k))
		*op_status = RTE_CRYPTO_OP_STATUS_SUCCESS;
	else
		*op_status = RTE_CRYPTO_OP_STATUS_ERROR;

	if (unlikely(infl_req->op_flags & CPT_OP_FLAGS_METABUF))
		rte_mempool_put(qp->meta_info.pool, infl_req->mdata);

	*dequeue_status = 1;
exit:
	return opaque;
}

static int
cn20k_sym_get_raw_dp_ctx_size(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct cnxk_sym_dp_ctx);
}

static int
cn20k_sym_configure_raw_dp_ctx(struct rte_cryptodev *dev, uint16_t qp_id,
			       struct rte_crypto_raw_dp_ctx *raw_dp_ctx,
			       enum rte_crypto_op_sess_type sess_type,
			       union rte_cryptodev_session_ctx session_ctx, uint8_t is_update)
{
	struct cnxk_se_sess *sess = (struct cnxk_se_sess *)session_ctx.crypto_sess;
	struct cnxk_sym_dp_ctx *dp_ctx;

	if (sess_type != RTE_CRYPTO_OP_WITH_SESSION)
		return -ENOTSUP;

	if (sess == NULL || sess->roc_se_ctx == NULL)
		return -EINVAL;

	if ((sess->dp_thr_type == CPT_DP_THREAD_TYPE_PDCP) ||
	    (sess->dp_thr_type == CPT_DP_THREAD_TYPE_PDCP_CHAIN) ||
	    (sess->dp_thr_type == CPT_DP_THREAD_TYPE_KASUMI) ||
	    (sess->dp_thr_type == CPT_DP_THREAD_TYPE_SM))
		return -ENOTSUP;

	if ((sess->dp_thr_type == CPT_DP_THREAD_AUTH_ONLY) &&
	    ((sess->roc_se_ctx->fc_type == ROC_SE_KASUMI) ||
	     (sess->roc_se_ctx->fc_type == ROC_SE_PDCP)))
		return -ENOTSUP;

	if (sess->roc_se_ctx->hash_type == ROC_SE_SHA1_TYPE)
		return -ENOTSUP;

	dp_ctx = (struct cnxk_sym_dp_ctx *)raw_dp_ctx->drv_ctx_data;
	dp_ctx->sess = sess;

	if (!is_update) {
		raw_dp_ctx->qp_data = (struct cnxk_cpt_qp *)dev->data->queue_pairs[qp_id];
		raw_dp_ctx->dequeue = cn20k_cpt_sym_raw_dequeue;
		raw_dp_ctx->dequeue_burst = cn20k_cpt_sym_raw_dequeue_burst;
		raw_dp_ctx->enqueue = cn20k_cpt_raw_enqueue;
		raw_dp_ctx->enqueue_burst = cn20k_cpt_raw_enqueue_burst;
	}

	return 0;
}

#if defined(RTE_ARCH_ARM64)
RTE_EXPORT_INTERNAL_SYMBOL(cn20k_cryptodev_sec_inb_rx_inject)
uint16_t __rte_hot
cn20k_cryptodev_sec_inb_rx_inject(void *dev, struct rte_mbuf **pkts,
				  struct rte_security_session **sess, uint16_t nb_pkts)
{
	uint64_t lmt_base, io_addr, u64_0, u64_1, l2_len, pf_func;
	uint64x2_t inst_01, inst_23, inst_45, inst_67;
	struct cn20k_sec_session *sec_sess;
	struct rte_cryptodev *cdev = dev;
	union cpt_res_s *hw_res = NULL;
	uint16_t lmt_id, count = 0;
	struct cpt_inst_s *inst;
	union cpt_fc_write_s fc;
	struct cnxk_cpt_vf *vf;
	struct rte_mbuf *m;
	uint64_t u64_dptr;
	uint64_t *fc_addr;
	int i;

	vf = cdev->data->dev_private;

	lmt_base = vf->rx_inj_lmtline.lmt_base;
	io_addr = vf->rx_inj_lmtline.io_addr;
	fc_addr = vf->rx_inj_lmtline.fc_addr;

	ROC_LMT_BASE_ID_GET(lmt_base, lmt_id);
	pf_func = vf->rx_inj_sso_pf_func;

	const uint32_t fc_thresh = vf->rx_inj_lmtline.fc_thresh;

again:
	fc.u64[0] =
		rte_atomic_load_explicit((RTE_ATOMIC(uint64_t) *)fc_addr, rte_memory_order_relaxed);
	inst = (struct cpt_inst_s *)lmt_base;

	i = 0;

	if (unlikely(fc.s.qsize > fc_thresh))
		goto exit;

	for (; i < RTE_MIN(CN20K_CPT_PKTS_PER_LOOP, nb_pkts); i++) {

		m = pkts[i];
		sec_sess = (struct cn20k_sec_session *)sess[i];

		if (unlikely(rte_pktmbuf_headroom(m) < 32)) {
			plt_dp_err("No space for CPT res_s");
			break;
		}

		l2_len = m->l2_len;

		*rte_security_dynfield(m) = (uint64_t)sec_sess->userdata;

		hw_res = rte_pktmbuf_mtod(m, void *);
		hw_res = RTE_PTR_SUB(hw_res, 32);
		hw_res = RTE_PTR_ALIGN_CEIL(hw_res, 16);

		/* Prepare CPT instruction */
		if (m->nb_segs > 1) {
			struct rte_mbuf *last = rte_pktmbuf_lastseg(m);
			uintptr_t dptr, rxphdr, wqe_hdr;
			uint16_t i;

			if ((m->nb_segs > CNXK_CPT_MAX_SG_SEGS) ||
			    (rte_pktmbuf_tailroom(m) < CNXK_CPT_MIN_TAILROOM_REQ))
				goto exit;

			wqe_hdr = rte_pktmbuf_mtod_offset(last, uintptr_t, last->data_len);
			wqe_hdr += BIT_ULL(7);
			wqe_hdr = (wqe_hdr - 1) & ~(BIT_ULL(7) - 1);

			/* Pointer to WQE header */
			*(uint64_t *)(m + 1) = wqe_hdr;

			/* Reserve SG list after end of last mbuf data location. */
			rxphdr = wqe_hdr + 8;
			dptr = rxphdr + 7 * 8;

			/* Prepare Multiseg SG list */
			i = fill_sg2_comp_from_pkt((struct roc_sg2list_comp *)dptr, 0, m);
			u64_dptr = dptr | ((uint64_t)(i) << 60);
		} else {
			struct roc_sg2list_comp *sg2;
			uintptr_t dptr, wqe_hdr;

			/* Reserve space for WQE, NIX_RX_PARSE_S and SG_S.
			 * Populate SG_S with num segs and seg length
			 */
			wqe_hdr = (uintptr_t)(m + 1);
			*(uint64_t *)(m + 1) = wqe_hdr;

			sg2 = (struct roc_sg2list_comp *)(wqe_hdr + 8 * 8);
			sg2->u.s.len[0] = rte_pktmbuf_pkt_len(m);
			sg2->u.s.valid_segs = 1;

			dptr = (uint64_t)rte_pktmbuf_iova(m);
			u64_dptr = dptr;
		}

		/* Word 0 and 1 */
		inst_01 = vdupq_n_u64(0);
		u64_0 = pf_func << 48 | *(vf->rx_chan_base + m->port) << 4 | (l2_len - 2) << 24 |
			l2_len << 16;
		inst_01 = vsetq_lane_u64(u64_0, inst_01, 0);
		inst_01 = vsetq_lane_u64((uint64_t)hw_res, inst_01, 1);
		vst1q_u64(&inst->w0.u64, inst_01);

		/* Word 2 and 3 */
		inst_23 = vdupq_n_u64(0);
		u64_1 = (((uint64_t)m + sizeof(struct rte_mbuf)) >> 3) << 3 | 1;
		inst_23 = vsetq_lane_u64(u64_1, inst_23, 1);
		vst1q_u64(&inst->w2.u64, inst_23);

		/* Word 4 and 5 */
		inst_45 = vdupq_n_u64(0);
		u64_0 = sec_sess->inst.w4 | (rte_pktmbuf_pkt_len(m));
		inst_45 = vsetq_lane_u64(u64_0, inst_45, 0);
		inst_45 = vsetq_lane_u64(u64_dptr, inst_45, 1);
		vst1q_u64(&inst->w4.u64, inst_45);

		/* Word 6 and 7 */
		inst_67 = vdupq_n_u64(0);
		u64_1 = sec_sess->inst.w7;
		inst_67 = vsetq_lane_u64(u64_1, inst_67, 1);
		vst1q_u64(&inst->w6.u64, inst_67);

		inst++;
	}

	cn20k_cpt_lmtst_dual_submit(&io_addr, lmt_id, &i);

	if (nb_pkts - i > 0 && i == CN20K_CPT_PKTS_PER_LOOP) {
		nb_pkts -= CN20K_CPT_PKTS_PER_LOOP;
		pkts += CN20K_CPT_PKTS_PER_LOOP;
		count += CN20K_CPT_PKTS_PER_LOOP;
		sess += CN20K_CPT_PKTS_PER_LOOP;
		goto again;
	}

exit:
	return count + i;
}
#else
RTE_EXPORT_INTERNAL_SYMBOL(cn20k_cryptodev_sec_inb_rx_inject)
uint16_t __rte_hot
cn20k_cryptodev_sec_inb_rx_inject(void *dev, struct rte_mbuf **pkts,
				  struct rte_security_session **sess, uint16_t nb_pkts)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(pkts);
	RTE_SET_USED(sess);
	RTE_SET_USED(nb_pkts);
	return 0;
}
#endif

RTE_EXPORT_INTERNAL_SYMBOL(cn20k_cryptodev_sec_rx_inject_configure)
int
cn20k_cryptodev_sec_rx_inject_configure(void *device, uint16_t port_id, bool enable)
{
	struct rte_cryptodev *crypto_dev = device;
	struct rte_eth_dev *eth_dev;
	int ret;

	if (!rte_eth_dev_is_valid_port(port_id))
		return -EINVAL;

	if (!(crypto_dev->feature_flags & RTE_CRYPTODEV_FF_SECURITY_RX_INJECT))
		return -ENOTSUP;

	eth_dev = &rte_eth_devices[port_id];

	ret = strncmp(eth_dev->device->driver->name, "net_cn20k", 8);
	if (ret)
		return -ENOTSUP;

	roc_idev_nix_rx_inject_set(port_id, enable);

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
