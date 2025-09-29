/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <eal_export.h>
#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include <rte_event_crypto_adapter.h>
#include <rte_ip.h>
#include <rte_vect.h>

#include "roc_api.h"

#include "cn9k_cryptodev.h"
#include "cn9k_cryptodev_ops.h"
#include "cn9k_ipsec.h"
#include "cn9k_ipsec_la_ops.h"
#include "cnxk_ae.h"
#include "cnxk_cryptodev.h"
#include "cnxk_cryptodev_ops.h"
#include "cnxk_se.h"

static __rte_always_inline int __rte_hot
cn9k_cpt_sec_inst_fill(struct cnxk_cpt_qp *qp, struct rte_crypto_op *op,
		       struct cpt_inflight_req *infl_req, struct cpt_inst_s *inst)
{
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct cn9k_sec_session *sec_sess;

	sec_sess = (struct cn9k_sec_session *)(op->sym->session);

	if (unlikely(sym_op->m_dst && sym_op->m_dst != sym_op->m_src)) {
		plt_dp_err("Out of place is not supported");
		return -ENOTSUP;
	}

	if (sec_sess->is_outbound)
		return process_outb_sa(&qp->meta_info, op, sec_sess, inst, infl_req);
	else
		return process_inb_sa(&qp->meta_info, op, sec_sess, inst, infl_req);
}

static inline struct cnxk_se_sess *
cn9k_cpt_sym_temp_sess_create(struct cnxk_cpt_qp *qp, struct rte_crypto_op *op)
{
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct rte_cryptodev_sym_session *sess;
	struct cnxk_se_sess *priv;
	int ret;

	/* Create temporary session */
	if (rte_mempool_get(qp->sess_mp, (void **)&sess) < 0)
		return NULL;

	ret = sym_session_configure(qp->lf.roc_cpt, sym_op->xform, sess, true);
	if (ret)
		goto sess_put;

	priv = (struct cnxk_se_sess *)sess;

	sym_op->session = sess;

	return priv;

sess_put:
	rte_mempool_put(qp->sess_mp, sess);
	return NULL;
}

static inline struct cnxk_ae_sess *
cn9k_cpt_asym_temp_sess_create(struct cnxk_cpt_qp *qp, struct rte_crypto_op *op)
{
	struct rte_crypto_asym_op *asym_op = op->asym;
	struct roc_cpt *roc_cpt = qp->lf.roc_cpt;
	struct rte_cryptodev_asym_session *sess;
	struct cnxk_ae_sess *priv;
	struct cnxk_cpt_vf *vf;
	union cpt_inst_w7 w7;

	/* Create temporary session */
	if (rte_mempool_get(qp->sess_mp, (void **)&sess) < 0)
		return NULL;

	priv = (struct cnxk_ae_sess *)sess;
	if (cnxk_ae_fill_session_parameters(priv, asym_op->xform))
		goto sess_put;

	priv->lf = &qp->lf;

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

static inline int
cn9k_cpt_inst_prep(struct cnxk_cpt_qp *qp, struct rte_crypto_op *op,
		   struct cpt_inflight_req *infl_req, struct cpt_inst_s *inst)
{
	int ret;

	if (op->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
		struct rte_crypto_sym_op *sym_op;
		struct cnxk_se_sess *sess;

		if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
			sym_op = op->sym;
			sess = (struct cnxk_se_sess *)sym_op->session;
			ret = cpt_sym_inst_fill(qp, op, sess, infl_req, inst, false, false);
			inst->w7.u64 = sess->cpt_inst_w7;
		} else if (op->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION)
			ret = cn9k_cpt_sec_inst_fill(qp, op, infl_req, inst);
		else {
			sess = cn9k_cpt_sym_temp_sess_create(qp, op);
			if (unlikely(sess == NULL)) {
				plt_dp_err("Could not create temp session");
				return -1;
			}

			ret = cpt_sym_inst_fill(qp, op, sess, infl_req, inst, false, false);
			if (unlikely(ret)) {
				sym_session_clear(op->sym->session, true);
				rte_mempool_put(qp->sess_mp, op->sym->session);
			}
			inst->w7.u64 = sess->cpt_inst_w7;
		}
	} else if (op->type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC) {
		struct cnxk_ae_sess *sess;

		if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
			sess = (struct cnxk_ae_sess *)op->asym->session;
			ret = cnxk_ae_enqueue(qp, op, infl_req, inst, sess);
			inst->w7.u64 = sess->cpt_inst_w7;
		} else {
			sess = cn9k_cpt_asym_temp_sess_create(qp, op);
			if (unlikely(sess == NULL)) {
				plt_dp_err("Could not create temp session");
				return 0;
			}

			ret = cnxk_ae_enqueue(qp, op, infl_req, inst, sess);
			if (unlikely(ret)) {
				cnxk_ae_session_clear(NULL,
						      (struct rte_cryptodev_asym_session *)sess);
				rte_mempool_put(qp->sess_mp, sess);
				return 0;
			}
			inst->w7.u64 = sess->cpt_inst_w7;
		}
	} else {
		ret = -EINVAL;
		plt_dp_err("Unsupported op type");
	}

	return ret;
}

static uint16_t
cn9k_cpt_enqueue_burst(void *qptr, struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct cpt_inflight_req *infl_req_1, *infl_req_2;
	alignas(RTE_CACHE_LINE_SIZE) struct cpt_inst_s inst[2];
	struct rte_crypto_op *op_1, *op_2;
	uint16_t nb_allowed, count = 0;
	struct cnxk_cpt_qp *qp = qptr;
	struct pending_queue *pend_q;
	uint64_t head;
	int ret;

	const union cpt_res_s res = {
		.cn10k.compcode = CPT_COMP_NOT_DONE,
	};

	pend_q = &qp->pend_q;
	rte_prefetch2(pend_q);

	/* Clear w0, w2, w3 of both inst */

#if defined(RTE_ARCH_ARM64)
	uint64x2_t zero = vdupq_n_u64(0);

	vst1q_u64(&inst[0].w0.u64, zero);
	vst1q_u64(&inst[1].w0.u64, zero);
	vst1q_u64(&inst[0].w2.u64, zero);
	vst1q_u64(&inst[1].w2.u64, zero);
#else
	inst[0].w0.u64 = 0;
	inst[0].w2.u64 = 0;
	inst[0].w3.u64 = 0;
	inst[1].w0.u64 = 0;
	inst[1].w2.u64 = 0;
	inst[1].w3.u64 = 0;
#endif

	const uint64_t lmt_base = qp->lf.lmt_base;
	const uint64_t io_addr = qp->lf.io_addr;
	const uint64_t pq_mask = pend_q->pq_mask;

	head = pend_q->head;
	nb_allowed = pending_queue_free_cnt(head, pend_q->tail, pq_mask);
	nb_ops = RTE_MIN(nb_ops, nb_allowed);

	if (unlikely(nb_ops & 1)) {
		op_1 = ops[0];
		infl_req_1 = &pend_q->req_queue[head];
		infl_req_1->op_flags = 0;

		ret = cn9k_cpt_inst_prep(qp, op_1, infl_req_1, &inst[0]);
		if (unlikely(ret)) {
			plt_dp_err("Could not process op: %p", op_1);
			return 0;
		}

		infl_req_1->cop = op_1;
		infl_req_1->res.cn9k.compcode = CPT_COMP_NOT_DONE;
		inst[0].res_addr = (uint64_t)&infl_req_1->res;

		cn9k_cpt_inst_submit(&inst[0], lmt_base, io_addr);
		pending_queue_advance(&head, pq_mask);
		count++;
	}

	while (count < nb_ops) {
		op_1 = ops[count];
		op_2 = ops[count + 1];

		infl_req_1 = &pend_q->req_queue[head];
		pending_queue_advance(&head, pq_mask);
		infl_req_2 = &pend_q->req_queue[head];
		pending_queue_advance(&head, pq_mask);

		infl_req_1->cop = op_1;
		infl_req_2->cop = op_2;
		infl_req_1->op_flags = 0;
		infl_req_2->op_flags = 0;

		rte_atomic_store_explicit((uint64_t __rte_atomic *)&infl_req_1->res.u64[0],
					  res.u64[0], rte_memory_order_relaxed);
		inst[0].res_addr = (uint64_t)&infl_req_1->res;

		rte_atomic_store_explicit((uint64_t __rte_atomic *)&infl_req_2->res.u64[0],
					  res.u64[0], rte_memory_order_relaxed);
		inst[1].res_addr = (uint64_t)&infl_req_2->res;

		ret = cn9k_cpt_inst_prep(qp, op_1, infl_req_1, &inst[0]);
		if (unlikely(ret)) {
			plt_dp_err("Could not process op: %p", op_1);
			pending_queue_retreat(&head, pq_mask, 2);
			break;
		}

		ret = cn9k_cpt_inst_prep(qp, op_2, infl_req_2, &inst[1]);
		if (unlikely(ret)) {
			plt_dp_err("Could not process op: %p", op_2);
			pending_queue_retreat(&head, pq_mask, 1);
			cn9k_cpt_inst_submit(&inst[0], lmt_base, io_addr);
			count++;
			break;
		}

		cn9k_cpt_inst_submit_dual(&inst[0], lmt_base, io_addr);

		count += 2;
	}

	rte_atomic_thread_fence(rte_memory_order_release);

	pend_q->head = head;
	pend_q->time_out = rte_get_timer_cycles() +
			   DEFAULT_COMMAND_TIMEOUT * rte_get_timer_hz();

	return count;
}

static int
cn9k_cpt_crypto_adapter_ev_mdata_set(struct rte_cryptodev *dev __rte_unused,
				     void *sess,
				     enum rte_crypto_op_type op_type,
				     enum rte_crypto_op_sess_type sess_type,
				     void *mdata)
{
	union rte_event_crypto_metadata *ec_mdata = mdata;
	struct rte_event *rsp_info;
	struct cnxk_cpt_qp *qp;
	uint8_t cdev_id;
	uint16_t qp_id;
	uint64_t w2;

	/* Get queue pair */
	cdev_id = ec_mdata->request_info.cdev_id;
	qp_id = ec_mdata->request_info.queue_pair_id;
	qp = rte_cryptodevs[cdev_id].data->queue_pairs[qp_id];

	/* Prepare w2 */
	rsp_info = &ec_mdata->response_info;
	w2 = CNXK_CPT_INST_W2((RTE_EVENT_TYPE_CRYPTODEV << 28) |
				      (rsp_info->sub_event_type << 20) |
				      rsp_info->flow_id,
			      rsp_info->sched_type, rsp_info->queue_id, 0);

	/* Set meta according to session type */
	if (op_type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
		if (sess_type == RTE_CRYPTO_OP_SECURITY_SESSION) {
			struct cn9k_sec_session *priv;

			priv = (struct cn9k_sec_session *)sess;
			priv->qp = qp;
			priv->inst.w2 = w2;
		} else if (sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
			struct cnxk_se_sess *priv;

			priv = (struct cnxk_se_sess *)sess;
			priv->qp = qp;
			priv->cpt_inst_w2 = w2;
		} else
			return -EINVAL;
	} else if (op_type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC) {
		if (sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
			struct rte_cryptodev_asym_session *asym_sess = sess;
			struct cnxk_ae_sess *priv;

			priv = (struct cnxk_ae_sess *)asym_sess;
			priv->qp = qp;
			priv->cpt_inst_w2 = w2;
		} else
			return -EINVAL;
	} else
		return -EINVAL;

	return 0;
}

static inline int
cn9k_ca_meta_info_extract(struct rte_crypto_op *op,
			struct cnxk_cpt_qp **qp, struct cpt_inst_s *inst)
{
	if (op->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
		if (op->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION) {
			struct cn9k_sec_session *priv;

			priv = (struct cn9k_sec_session *)(op->sym->session);
			*qp = priv->qp;
			inst->w2.u64 = priv->inst.w2;
		} else if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
			struct cnxk_se_sess *priv;

			priv = (struct cnxk_se_sess *)op->sym->session;
			*qp = priv->qp;
			inst->w2.u64 = priv->cpt_inst_w2;
		} else {
			union rte_event_crypto_metadata *ec_mdata;
			struct rte_event *rsp_info;
			uint8_t cdev_id;
			uint16_t qp_id;

			if (unlikely(op->private_data_offset == 0))
				return -EINVAL;
			ec_mdata = (union rte_event_crypto_metadata *)
				((uint8_t *)op + op->private_data_offset);
			rsp_info = &ec_mdata->response_info;
			cdev_id = ec_mdata->request_info.cdev_id;
			qp_id = ec_mdata->request_info.queue_pair_id;
			*qp = rte_cryptodevs[cdev_id].data->queue_pairs[qp_id];
			inst->w2.u64 = CNXK_CPT_INST_W2(
				(RTE_EVENT_TYPE_CRYPTODEV << 28) | rsp_info->flow_id,
				rsp_info->sched_type, rsp_info->queue_id, 0);
		}
	} else if (op->type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC) {
		if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
			struct cnxk_ae_sess *priv;

			priv = (struct cnxk_ae_sess *)op->asym->session;
			*qp = priv->qp;
			inst->w2.u64 = priv->cpt_inst_w2;
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
			inst->w2.u64 = CNXK_CPT_INST_W2(
				(RTE_EVENT_TYPE_CRYPTODEV << 28) | rsp_info->flow_id,
				rsp_info->sched_type, rsp_info->queue_id, 0);
		}
	} else
		return -EINVAL;

	return 0;
}

RTE_EXPORT_INTERNAL_SYMBOL(cn9k_cpt_crypto_adapter_enqueue)
uint16_t
cn9k_cpt_crypto_adapter_enqueue(uintptr_t base, struct rte_crypto_op *op)
{
	struct cpt_inflight_req *infl_req;
	uint64_t __rte_atomic *fc_addr;
	union cpt_fc_write_s fc;
	struct cnxk_cpt_qp *qp;
	struct cpt_inst_s inst;
	int ret;

	ret = cn9k_ca_meta_info_extract(op, &qp, &inst);
	if (unlikely(ret)) {
		rte_errno = EINVAL;
		return 0;
	}

	if (unlikely(!qp->ca.enabled)) {
		rte_errno = EINVAL;
		return 0;
	}

	if (unlikely(rte_mempool_get(qp->ca.req_mp, (void **)&infl_req))) {
		rte_errno = ENOMEM;
		return 0;
	}
	infl_req->op_flags = 0;

	ret = cn9k_cpt_inst_prep(qp, op, infl_req, &inst);
	if (unlikely(ret)) {
		plt_dp_err("Could not process op: %p", op);
		rte_mempool_put(qp->ca.req_mp, infl_req);
		return 0;
	}

	infl_req->cop = op;
	infl_req->res.cn9k.compcode = CPT_COMP_NOT_DONE;
	infl_req->qp = qp;
	inst.w0.u64 = 0;
	inst.res_addr = (uint64_t)&infl_req->res;
	inst.w3.u64 = CNXK_CPT_INST_W3(1, infl_req);

	fc_addr = (uint64_t __rte_atomic *)qp->lmtline.fc_addr;

	const uint32_t fc_thresh = qp->lmtline.fc_thresh;

	fc.u64[0] = rte_atomic_load_explicit(fc_addr, rte_memory_order_relaxed);
	if (unlikely(fc.s.qsize > fc_thresh)) {
		rte_mempool_put(qp->ca.req_mp, infl_req);
		rte_errno = EAGAIN;
		return 0;
	}

	if (inst.w2.s.tt == RTE_SCHED_TYPE_ORDERED)
		roc_sso_hws_head_wait(base);

	cn9k_cpt_inst_submit(&inst, qp->lmtline.lmt_base, qp->lmtline.io_addr);

	return 1;
}

static inline int
ipsec_antireplay_check(struct cn9k_sec_session *sess, uint32_t win_sz,
		       struct roc_ie_on_inb_hdr *data)
{
	uint32_t esn_low, esn_hi, seql, seqh = 0;
	struct roc_ie_on_common_sa *common_sa;
	struct roc_ie_on_inb_sa *in_sa;
	uint64_t seq, seq_in_sa;
	uint8_t esn;
	int ret;

	in_sa = &sess->sa.in_sa;
	common_sa = &in_sa->common_sa;

	esn = common_sa->ctl.esn_en;
	seql = rte_be_to_cpu_32(data->seql);

	if (!esn) {
		seq = (uint64_t)seql;
	} else {
		seqh = rte_be_to_cpu_32(data->seqh);
		seq = ((uint64_t)seqh << 32) | seql;
	}

	if (unlikely(seq == 0))
		return IPSEC_ANTI_REPLAY_FAILED;

	rte_spinlock_lock(&sess->ar.lock);
	ret = cnxk_on_anti_replay_check(seq, &sess->ar, win_sz);
	if (esn && !ret) {
		esn_low = rte_be_to_cpu_32(common_sa->seq_t.tl);
		esn_hi = rte_be_to_cpu_32(common_sa->seq_t.th);
		seq_in_sa = ((uint64_t)esn_hi << 32) | esn_low;
		if (seq > seq_in_sa) {
			common_sa->seq_t.tl = rte_cpu_to_be_32(seql);
			common_sa->seq_t.th = rte_cpu_to_be_32(seqh);
		}
	}
	rte_spinlock_unlock(&sess->ar.lock);

	return ret;
}

static inline void
cn9k_cpt_sec_post_process(struct rte_crypto_op *cop,
			  struct cpt_inflight_req *infl_req)
{
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct rte_mbuf *m = sym_op->m_src;
	struct roc_ie_on_inb_hdr *hdr;
	struct cn9k_sec_session *priv;
	struct rte_ipv6_hdr *ip6;
	struct rte_ipv4_hdr *ip;
	uint16_t m_len = 0;

	if (infl_req->op_flags & CPT_OP_FLAGS_IPSEC_DIR_INBOUND) {

		hdr = rte_pktmbuf_mtod(m, struct roc_ie_on_inb_hdr *);

		if (likely(m->next == NULL)) {
			ip = PLT_PTR_ADD(hdr, ROC_IE_ON_INB_RPTR_HDR);
		} else {
			ip = (struct rte_ipv4_hdr *)hdr;
			hdr = infl_req->mdata;
		}

		if (unlikely(infl_req->op_flags & CPT_OP_FLAGS_IPSEC_INB_REPLAY)) {
			int ret;

			priv = (struct cn9k_sec_session *)(sym_op->session);

			ret = ipsec_antireplay_check(priv, priv->replay_win_sz, hdr);
			if (unlikely(ret)) {
				cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
				return;
			}
		}

		if (ip->version == IPVERSION) {
			m_len = rte_be_to_cpu_16(ip->total_length);
		} else {
			PLT_ASSERT((ip->version == 6));
			ip6 = (struct rte_ipv6_hdr *)ip;
			m_len = rte_be_to_cpu_16(ip6->payload_len) + sizeof(struct rte_ipv6_hdr);
		}

		if (likely(m->next == NULL)) {
			m->data_len = m_len;
			m->pkt_len = m_len;

			m->data_off += ROC_IE_ON_INB_RPTR_HDR;
		} else {
			struct rte_mbuf *temp = m;
			uint8_t m_len_s = m_len;

			while (m_len_s - temp->data_len > 0) {
				m_len_s -= temp->data_len;
				temp = temp->next;
			}

			temp->data_len = m_len_s;
			m->pkt_len = m_len;
		}
	}
}

static inline void
cn9k_cpt_dequeue_post_process(struct cnxk_cpt_qp *qp, struct rte_crypto_op *cop,
			      struct cpt_inflight_req *infl_req,
			      struct cpt_cn9k_res_s *res)
{
	if (likely(res->compcode == CPT_COMP_GOOD)) {
		if (unlikely(res->uc_compcode)) {
			if (res->uc_compcode == ROC_SE_ERR_GC_ICV_MISCOMPARE)
				cop->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
			else if (cop->type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC &&
				 cop->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
				struct cnxk_ae_sess *sess;

				sess = (struct cnxk_ae_sess *)cop->asym->session;
				if (sess->xfrm_type == RTE_CRYPTO_ASYM_XFORM_ECDH &&
				    cop->asym->ecdh.ke_type == RTE_CRYPTO_ASYM_KE_PUB_KEY_VERIFY) {
					if (res->uc_compcode == ROC_AE_ERR_ECC_POINT_NOT_ON_CURVE) {
						cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
						return;
					} else if (res->uc_compcode == ROC_AE_ERR_ECC_PAI) {
						cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
						return;
					}
				} else {
					cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
				}
			} else
				cop->status = RTE_CRYPTO_OP_STATUS_ERROR;

			plt_dp_info("Request failed with microcode error");
			plt_dp_info("MC completion code 0x%x",
				    res->uc_compcode);
			goto temp_sess_free;
		}

		cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
		if (cop->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
			if (cop->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION) {
				cn9k_cpt_sec_post_process(cop, infl_req);
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
			struct cnxk_ae_sess *sess = (struct cnxk_ae_sess *)op->session;

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

RTE_EXPORT_INTERNAL_SYMBOL(cn9k_cpt_crypto_adapter_dequeue)
uintptr_t
cn9k_cpt_crypto_adapter_dequeue(uintptr_t get_work1)
{
	struct cpt_inflight_req *infl_req;
	struct rte_crypto_op *cop;
	struct cnxk_cpt_qp *qp;
	union cpt_res_s res;

	infl_req = (struct cpt_inflight_req *)(get_work1);
	cop = infl_req->cop;
	qp = infl_req->qp;

	res.u64[0] = rte_atomic_load_explicit((uint64_t __rte_atomic *)&infl_req->res.u64[0],
					      rte_memory_order_relaxed);

	cn9k_cpt_dequeue_post_process(qp, infl_req->cop, infl_req, &res.cn9k);

	if (unlikely(infl_req->op_flags & CPT_OP_FLAGS_METABUF))
		rte_mempool_put(qp->meta_info.pool, infl_req->mdata);

	rte_mempool_put(qp->ca.req_mp, infl_req);
	return (uintptr_t)cop;
}

static uint16_t
cn9k_cpt_dequeue_burst(void *qptr, struct rte_crypto_op **ops, uint16_t nb_ops)
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
			(uint64_t __rte_atomic *)&infl_req->res.u64[0], rte_memory_order_relaxed);

		if (unlikely(res.cn9k.compcode == CPT_COMP_NOT_DONE)) {
			if (unlikely(rte_get_timer_cycles() >
				     pend_q->time_out)) {
				plt_err("Request timed out");
				cnxk_cpt_dump_on_err(qp);
				pend_q->time_out = rte_get_timer_cycles() +
						   DEFAULT_COMMAND_TIMEOUT *
							   rte_get_timer_hz();
			}
			break;
		}

		pending_queue_advance(&pq_tail, pq_mask);

		cop = infl_req->cop;

		ops[i] = cop;

		cn9k_cpt_dequeue_post_process(qp, cop, infl_req, &res.cn9k);

		if (unlikely(infl_req->op_flags & CPT_OP_FLAGS_METABUF))
			rte_mempool_put(qp->meta_info.pool, infl_req->mdata);
	}

	pend_q->tail = pq_tail;

	return i;
}
void
cn9k_cpt_set_enqdeq_fns(struct rte_cryptodev *dev)
{
	dev->enqueue_burst = cn9k_cpt_enqueue_burst;
	dev->dequeue_burst = cn9k_cpt_dequeue_burst;

	rte_mb();
}

static void
cn9k_cpt_dev_info_get(struct rte_cryptodev *dev,
		      struct rte_cryptodev_info *info)
{
	if (info != NULL) {
		cnxk_cpt_dev_info_get(dev, info);
		info->driver_id = cn9k_cryptodev_driver_id;
	}
}

struct rte_cryptodev_ops cn9k_cpt_ops = {
	/* Device control ops */
	.dev_configure = cnxk_cpt_dev_config,
	.dev_start = cnxk_cpt_dev_start,
	.dev_stop = cnxk_cpt_dev_stop,
	.dev_close = cnxk_cpt_dev_close,
	.dev_infos_get = cn9k_cpt_dev_info_get,

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
	.session_ev_mdata_set = cn9k_cpt_crypto_adapter_ev_mdata_set,
	.queue_pair_event_error_query = cnxk_cpt_queue_pair_event_error_query,

};
