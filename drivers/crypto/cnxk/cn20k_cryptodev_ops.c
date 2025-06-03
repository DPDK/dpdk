/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell.
 */

#include <cryptodev_pmd.h>
#include <eal_export.h>
#include <ethdev_driver.h>
#include <rte_cryptodev.h>
#include <rte_hexdump.h>
#include <rte_vect.h>

#include "roc_cpt.h"
#include "roc_idev.h"

#include "cn20k_cryptodev.h"
#include "cn20k_cryptodev_ops.h"
#include "cn20k_cryptodev_sec.h"
#include "cn20k_ipsec_la_ops.h"
#include "cn20k_tls_ops.h"
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
cn20k_cpt_sec_post_process(struct rte_crypto_op *cop, struct cpt_cn20k_res_s *res)
{
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct cn20k_sec_session *sess;

	sess = sym_op->session;
	if (sess->proto == RTE_SECURITY_PROTOCOL_IPSEC)
		cn20k_cpt_ipsec_post_process(cop, res);
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
