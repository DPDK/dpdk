/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include <rte_event_crypto_adapter.h>
#include <rte_ip.h>

#include "cn10k_cryptodev.h"
#include "cn10k_cryptodev_ops.h"
#include "cn10k_ipsec_la_ops.h"
#include "cn10k_ipsec.h"
#include "cnxk_ae.h"
#include "cnxk_cryptodev.h"
#include "cnxk_cryptodev_ops.h"
#include "cnxk_se.h"

#include "roc_api.h"

static inline struct cnxk_se_sess *
cn10k_cpt_sym_temp_sess_create(struct cnxk_cpt_qp *qp, struct rte_crypto_op *op)
{
	const int driver_id = cn10k_cryptodev_driver_id;
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct rte_cryptodev_sym_session *sess;
	struct cnxk_se_sess *priv;
	int ret;

	/* Create temporary session */
	sess = rte_cryptodev_sym_session_create(qp->sess_mp);
	if (sess == NULL)
		return NULL;

	ret = sym_session_configure(qp->lf.roc_cpt, driver_id, sym_op->xform,
				    sess, qp->sess_mp_priv);
	if (ret)
		goto sess_put;

	priv = get_sym_session_private_data(sess, driver_id);

	sym_op->session = sess;

	return priv;

sess_put:
	rte_mempool_put(qp->sess_mp, sess);
	return NULL;
}

static __rte_always_inline int __rte_hot
cpt_sec_inst_fill(struct rte_crypto_op *op, struct cn10k_sec_session *sess,
		  struct cpt_inst_s *inst)
{
	struct rte_crypto_sym_op *sym_op = op->sym;
	union roc_ot_ipsec_sa_word2 *w2;
	struct cn10k_ipsec_sa *sa;
	int ret;

	if (unlikely(sym_op->m_dst && sym_op->m_dst != sym_op->m_src)) {
		plt_dp_err("Out of place is not supported");
		return -ENOTSUP;
	}

	if (unlikely(!rte_pktmbuf_is_contiguous(sym_op->m_src))) {
		plt_dp_err("Scatter Gather mode is not supported");
		return -ENOTSUP;
	}

	sa = &sess->sa;
	w2 = (union roc_ot_ipsec_sa_word2 *)&sa->in_sa.w2;

	if (w2->s.dir == ROC_IE_SA_DIR_OUTBOUND)
		ret = process_outb_sa(op, sa, inst);
	else
		ret = process_inb_sa(op, sa, inst);

	return ret;
}

static __rte_always_inline int __rte_hot
cpt_sym_inst_fill(struct cnxk_cpt_qp *qp, struct rte_crypto_op *op,
		  struct cnxk_se_sess *sess, struct cpt_inflight_req *infl_req,
		  struct cpt_inst_s *inst)
{
	uint64_t cpt_op;
	int ret;

	cpt_op = sess->cpt_op;

	if (cpt_op & ROC_SE_OP_CIPHER_MASK)
		ret = fill_fc_params(op, sess, &qp->meta_info, infl_req, inst);
	else
		ret = fill_digest_params(op, sess, &qp->meta_info, infl_req,
					 inst);

	return ret;
}

static inline int
cn10k_cpt_fill_inst(struct cnxk_cpt_qp *qp, struct rte_crypto_op *ops[],
		    struct cpt_inst_s inst[], struct cpt_inflight_req *infl_req)
{
	struct cn10k_sec_session *sec_sess;
	struct rte_crypto_asym_op *asym_op;
	struct rte_crypto_sym_op *sym_op;
	struct cnxk_ae_sess *ae_sess;
	struct cnxk_se_sess *sess;
	struct rte_crypto_op *op;
	uint64_t w7;
	int ret;

	op = ops[0];

	inst[0].w0.u64 = 0;
	inst[0].w2.u64 = 0;
	inst[0].w3.u64 = 0;

	sym_op = op->sym;

	if (op->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
		if (op->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION) {
			sec_sess = get_sec_session_private_data(
				sym_op->sec_session);
			ret = cpt_sec_inst_fill(op, sec_sess, &inst[0]);
			if (unlikely(ret))
				return 0;
			w7 = sec_sess->sa.inst.w7;
		} else if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
			sess = get_sym_session_private_data(
				sym_op->session, cn10k_cryptodev_driver_id);
			ret = cpt_sym_inst_fill(qp, op, sess, infl_req,
						&inst[0]);
			if (unlikely(ret))
				return 0;
			w7 = sess->cpt_inst_w7;
		} else {
			sess = cn10k_cpt_sym_temp_sess_create(qp, op);
			if (unlikely(sess == NULL)) {
				plt_dp_err("Could not create temp session");
				return 0;
			}

			ret = cpt_sym_inst_fill(qp, op, sess, infl_req,
						&inst[0]);
			if (unlikely(ret)) {
				sym_session_clear(cn10k_cryptodev_driver_id,
						  op->sym->session);
				rte_mempool_put(qp->sess_mp, op->sym->session);
				return 0;
			}
			w7 = sess->cpt_inst_w7;
		}
	} else if (op->type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC) {

		if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
			asym_op = op->asym;
			ae_sess = get_asym_session_private_data(
				asym_op->session, cn10k_cryptodev_driver_id);
			ret = cnxk_ae_enqueue(qp, op, infl_req, &inst[0],
					      ae_sess);
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
	infl_req->res.cn10k.compcode = CPT_COMP_NOT_DONE;
	infl_req->cop = op;

	inst[0].w7.u64 = w7;

	return 1;
}

#define PKTS_PER_LOOP	32
#define PKTS_PER_STEORL 16

static uint16_t
cn10k_cpt_enqueue_burst(void *qptr, struct rte_crypto_op **ops, uint16_t nb_ops)
{
	uint64_t lmt_base, lmt_arg, io_addr;
	struct cpt_inflight_req *infl_req;
	uint16_t nb_allowed, count = 0;
	struct cnxk_cpt_qp *qp = qptr;
	struct pending_queue *pend_q;
	struct cpt_inst_s *inst;
	uint16_t lmt_id;
	int ret, i;

	pend_q = &qp->pend_q;

	nb_allowed = qp->lf.nb_desc - pend_q->pending_count;
	nb_ops = RTE_MIN(nb_ops, nb_allowed);

	if (unlikely(nb_ops == 0))
		return 0;

	lmt_base = qp->lmtline.lmt_base;
	io_addr = qp->lmtline.io_addr;

	ROC_LMT_BASE_ID_GET(lmt_base, lmt_id);
	inst = (struct cpt_inst_s *)lmt_base;

again:
	for (i = 0; i < RTE_MIN(PKTS_PER_LOOP, nb_ops); i++) {
		infl_req = &pend_q->req_queue[pend_q->enq_tail];
		infl_req->op_flags = 0;

		ret = cn10k_cpt_fill_inst(qp, ops + i, &inst[2 * i], infl_req);
		if (unlikely(ret != 1)) {
			plt_dp_err("Could not process op: %p", ops + i);
			if (i == 0)
				goto update_pending;
			break;
		}

		MOD_INC(pend_q->enq_tail, qp->lf.nb_desc);
	}

	if (i > PKTS_PER_STEORL) {
		lmt_arg = ROC_CN10K_CPT_LMT_ARG | (PKTS_PER_STEORL - 1) << 12 |
			  (uint64_t)lmt_id;
		roc_lmt_submit_steorl(lmt_arg, io_addr);
		lmt_arg = ROC_CN10K_CPT_LMT_ARG |
			  (i - PKTS_PER_STEORL - 1) << 12 |
			  (uint64_t)(lmt_id + PKTS_PER_STEORL);
		roc_lmt_submit_steorl(lmt_arg, io_addr);
	} else {
		lmt_arg = ROC_CN10K_CPT_LMT_ARG | (i - 1) << 12 |
			  (uint64_t)lmt_id;
		roc_lmt_submit_steorl(lmt_arg, io_addr);
	}

	rte_io_wmb();

	if (nb_ops - i > 0 && i == PKTS_PER_LOOP) {
		nb_ops -= i;
		ops += i;
		count += i;
		goto again;
	}

update_pending:
	pend_q->pending_count += count + i;

	pend_q->time_out = rte_get_timer_cycles() +
			   DEFAULT_COMMAND_TIMEOUT * rte_get_timer_hz();

	return count + i;
}

uint16_t
cn10k_cpt_crypto_adapter_enqueue(uintptr_t tag_op, struct rte_crypto_op *op)
{
	union rte_event_crypto_metadata *ec_mdata;
	struct cpt_inflight_req *infl_req;
	struct rte_event *rsp_info;
	uint64_t lmt_base, lmt_arg;
	struct cpt_inst_s *inst;
	struct cnxk_cpt_qp *qp;
	uint8_t cdev_id;
	uint16_t lmt_id;
	uint16_t qp_id;
	int ret;

	ec_mdata = cnxk_event_crypto_mdata_get(op);
	if (!ec_mdata) {
		rte_errno = EINVAL;
		return 0;
	}

	cdev_id = ec_mdata->request_info.cdev_id;
	qp_id = ec_mdata->request_info.queue_pair_id;
	qp = rte_cryptodevs[cdev_id].data->queue_pairs[qp_id];
	rsp_info = &ec_mdata->response_info;

	if (unlikely(!qp->ca.enabled)) {
		rte_errno = EINVAL;
		return 0;
	}

	if (unlikely(rte_mempool_get(qp->ca.req_mp, (void **)&infl_req))) {
		rte_errno = ENOMEM;
		return 0;
	}
	infl_req->op_flags = 0;

	lmt_base = qp->lmtline.lmt_base;
	ROC_LMT_BASE_ID_GET(lmt_base, lmt_id);
	inst = (struct cpt_inst_s *)lmt_base;

	ret = cn10k_cpt_fill_inst(qp, &op, inst, infl_req);
	if (unlikely(ret != 1)) {
		plt_dp_err("Could not process op: %p", op);
		rte_mempool_put(qp->ca.req_mp, infl_req);
		return 0;
	}

	infl_req->cop = op;
	infl_req->res.cn10k.compcode = CPT_COMP_NOT_DONE;
	infl_req->qp = qp;
	inst->w0.u64 = 0;
	inst->res_addr = (uint64_t)&infl_req->res;
	inst->w2.u64 = CNXK_CPT_INST_W2(
		(RTE_EVENT_TYPE_CRYPTODEV << 28) | rsp_info->flow_id,
		rsp_info->sched_type, rsp_info->queue_id, 0);
	inst->w3.u64 = CNXK_CPT_INST_W3(1, infl_req);

	if (roc_cpt_is_iq_full(&qp->lf)) {
		rte_mempool_put(qp->ca.req_mp, infl_req);
		rte_errno = EAGAIN;
		return 0;
	}

	if (!rsp_info->sched_type)
		roc_sso_hws_head_wait(tag_op);

	lmt_arg = ROC_CN10K_CPT_LMT_ARG | (uint64_t)lmt_id;
	roc_lmt_submit_steorl(lmt_arg, qp->lmtline.io_addr);

	rte_io_wmb();

	return 1;
}

static inline void
cn10k_cpt_sec_post_process(struct rte_crypto_op *cop,
			   struct cpt_cn10k_res_s *res)
{
	struct rte_mbuf *m = cop->sym->m_src;
	const uint16_t m_len = res->rlen;

	m->data_len = m_len;
	m->pkt_len = m_len;
}

static inline void
cn10k_cpt_dequeue_post_process(struct cnxk_cpt_qp *qp,
			       struct rte_crypto_op *cop,
			       struct cpt_inflight_req *infl_req)
{
	struct cpt_cn10k_res_s *res = (struct cpt_cn10k_res_s *)&infl_req->res;
	unsigned int sz;

	if (likely(res->compcode == CPT_COMP_GOOD ||
		   res->compcode == CPT_COMP_WARN)) {
		if (unlikely(res->uc_compcode)) {
			if (res->uc_compcode == ROC_SE_ERR_GC_ICV_MISCOMPARE)
				cop->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
			else
				cop->status = RTE_CRYPTO_OP_STATUS_ERROR;

			plt_dp_info("Request failed with microcode error");
			plt_dp_info("MC completion code 0x%x",
				    res->uc_compcode);
			goto temp_sess_free;
		}

		cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
		if (cop->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
			if (cop->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION) {
				cn10k_cpt_sec_post_process(cop, res);
				return;
			}

			/* Verify authentication data if required */
			if (unlikely(infl_req->op_flags &
				     CPT_OP_FLAGS_AUTH_VERIFY)) {
				uintptr_t *rsp = infl_req->mdata;
				compl_auth_verify(cop, (uint8_t *)rsp[0],
						  rsp[1]);
			}
		} else if (cop->type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC) {
			struct rte_crypto_asym_op *op = cop->asym;
			uintptr_t *mdata = infl_req->mdata;
			struct cnxk_ae_sess *sess;

			sess = get_asym_session_private_data(
				op->session, cn10k_cryptodev_driver_id);

			cnxk_ae_post_process(cop, sess, (uint8_t *)mdata[0]);
		}
	} else {
		cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
		plt_dp_info("HW completion code 0x%x", res->compcode);

		switch (res->compcode) {
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
			plt_dp_err(
				"Request failed with unknown completion code");
		}
	}

temp_sess_free:
	if (unlikely(cop->sess_type == RTE_CRYPTO_OP_SESSIONLESS)) {
		if (cop->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
			sym_session_clear(cn10k_cryptodev_driver_id,
					  cop->sym->session);
			sz = rte_cryptodev_sym_get_existing_header_session_size(
				cop->sym->session);
			memset(cop->sym->session, 0, sz);
			rte_mempool_put(qp->sess_mp, cop->sym->session);
			cop->sym->session = NULL;
		}
	}
}

uintptr_t
cn10k_cpt_crypto_adapter_dequeue(uintptr_t get_work1)
{
	struct cpt_inflight_req *infl_req;
	struct rte_crypto_op *cop;
	struct cnxk_cpt_qp *qp;

	infl_req = (struct cpt_inflight_req *)(get_work1);
	cop = infl_req->cop;
	qp = infl_req->qp;

	cn10k_cpt_dequeue_post_process(qp, infl_req->cop, infl_req);

	if (unlikely(infl_req->op_flags & CPT_OP_FLAGS_METABUF))
		rte_mempool_put(qp->meta_info.pool, infl_req->mdata);

	rte_mempool_put(qp->ca.req_mp, infl_req);
	return (uintptr_t)cop;
}

static uint16_t
cn10k_cpt_dequeue_burst(void *qptr, struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct cpt_inflight_req *infl_req;
	struct cnxk_cpt_qp *qp = qptr;
	struct pending_queue *pend_q;
	struct cpt_cn10k_res_s *res;
	struct rte_crypto_op *cop;
	int i, nb_pending;

	pend_q = &qp->pend_q;

	nb_pending = pend_q->pending_count;

	if (nb_ops > nb_pending)
		nb_ops = nb_pending;

	for (i = 0; i < nb_ops; i++) {
		infl_req = &pend_q->req_queue[pend_q->deq_head];

		res = (struct cpt_cn10k_res_s *)&infl_req->res;

		if (unlikely(res->compcode == CPT_COMP_NOT_DONE)) {
			if (unlikely(rte_get_timer_cycles() >
				     pend_q->time_out)) {
				plt_err("Request timed out");
				pend_q->time_out = rte_get_timer_cycles() +
						   DEFAULT_COMMAND_TIMEOUT *
							   rte_get_timer_hz();
			}
			break;
		}

		MOD_INC(pend_q->deq_head, qp->lf.nb_desc);

		cop = infl_req->cop;

		ops[i] = cop;

		cn10k_cpt_dequeue_post_process(qp, cop, infl_req);

		if (unlikely(infl_req->op_flags & CPT_OP_FLAGS_METABUF))
			rte_mempool_put(qp->meta_info.pool, infl_req->mdata);
	}

	pend_q->pending_count -= i;

	return i;
}

void
cn10k_cpt_set_enqdeq_fns(struct rte_cryptodev *dev)
{
	dev->enqueue_burst = cn10k_cpt_enqueue_burst;
	dev->dequeue_burst = cn10k_cpt_dequeue_burst;

	rte_mb();
}

static void
cn10k_cpt_dev_info_get(struct rte_cryptodev *dev,
		       struct rte_cryptodev_info *info)
{
	if (info != NULL) {
		cnxk_cpt_dev_info_get(dev, info);
		info->driver_id = cn10k_cryptodev_driver_id;
	}
}

struct rte_cryptodev_ops cn10k_cpt_ops = {
	/* Device control ops */
	.dev_configure = cnxk_cpt_dev_config,
	.dev_start = cnxk_cpt_dev_start,
	.dev_stop = cnxk_cpt_dev_stop,
	.dev_close = cnxk_cpt_dev_close,
	.dev_infos_get = cn10k_cpt_dev_info_get,

	.stats_get = NULL,
	.stats_reset = NULL,
	.queue_pair_setup = cnxk_cpt_queue_pair_setup,
	.queue_pair_release = cnxk_cpt_queue_pair_release,

	/* Symmetric crypto ops */
	.sym_session_get_size = cnxk_cpt_sym_session_get_size,
	.sym_session_configure = cnxk_cpt_sym_session_configure,
	.sym_session_clear = cnxk_cpt_sym_session_clear,

	/* Asymmetric crypto ops */
	.asym_session_get_size = cnxk_ae_session_size_get,
	.asym_session_configure = cnxk_ae_session_cfg,
	.asym_session_clear = cnxk_ae_session_clear,

};
