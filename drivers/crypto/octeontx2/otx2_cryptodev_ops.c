/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019 Marvell International Ltd.
 */

#include <unistd.h>

#include <cryptodev_pmd.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_event_crypto_adapter.h>

#include "otx2_cryptodev.h"
#include "otx2_cryptodev_capabilities.h"
#include "otx2_cryptodev_hw_access.h"
#include "otx2_cryptodev_mbox.h"
#include "otx2_cryptodev_ops.h"
#include "otx2_cryptodev_ops_helper.h"
#include "otx2_ipsec_anti_replay.h"
#include "otx2_ipsec_po_ops.h"
#include "otx2_mbox.h"
#include "otx2_sec_idev.h"
#include "otx2_security.h"

#include "cpt_hw_types.h"
#include "cpt_pmd_logs.h"
#include "cpt_pmd_ops_helper.h"
#include "cpt_ucode.h"
#include "cpt_ucode_asym.h"

#define METABUF_POOL_CACHE_SIZE	512

static uint64_t otx2_fpm_iova[CPT_EC_ID_PMAX];

/* Forward declarations */

static int
otx2_cpt_queue_pair_release(struct rte_cryptodev *dev, uint16_t qp_id);

static void
qp_memzone_name_get(char *name, int size, int dev_id, int qp_id)
{
	snprintf(name, size, "otx2_cpt_lf_mem_%u:%u", dev_id, qp_id);
}

static int
otx2_cpt_metabuf_mempool_create(const struct rte_cryptodev *dev,
				struct otx2_cpt_qp *qp, uint8_t qp_id,
				unsigned int nb_elements)
{
	char mempool_name[RTE_MEMPOOL_NAMESIZE];
	struct cpt_qp_meta_info *meta_info;
	int ret, max_mlen, mb_pool_sz;
	struct rte_mempool *pool;
	int asym_mlen = 0;
	int lb_mlen = 0;
	int sg_mlen = 0;

	if (dev->feature_flags & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO) {

		/* Get meta len for scatter gather mode */
		sg_mlen = cpt_pmd_ops_helper_get_mlen_sg_mode();

		/* Extra 32B saved for future considerations */
		sg_mlen += 4 * sizeof(uint64_t);

		/* Get meta len for linear buffer (direct) mode */
		lb_mlen = cpt_pmd_ops_helper_get_mlen_direct_mode();

		/* Extra 32B saved for future considerations */
		lb_mlen += 4 * sizeof(uint64_t);
	}

	if (dev->feature_flags & RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO) {

		/* Get meta len required for asymmetric operations */
		asym_mlen = cpt_pmd_ops_helper_asym_get_mlen();
	}

	/*
	 * Check max requirement for meta buffer to
	 * support crypto op of any type (sym/asym).
	 */
	max_mlen = RTE_MAX(RTE_MAX(lb_mlen, sg_mlen), asym_mlen);

	/* Allocate mempool */

	snprintf(mempool_name, RTE_MEMPOOL_NAMESIZE, "otx2_cpt_mb_%u:%u",
		 dev->data->dev_id, qp_id);

	mb_pool_sz = RTE_MAX(nb_elements, (METABUF_POOL_CACHE_SIZE * rte_lcore_count()));

	pool = rte_mempool_create_empty(mempool_name, mb_pool_sz, max_mlen,
					METABUF_POOL_CACHE_SIZE, 0,
					rte_socket_id(), 0);

	if (pool == NULL) {
		CPT_LOG_ERR("Could not create mempool for metabuf");
		return rte_errno;
	}

	ret = rte_mempool_set_ops_byname(pool, RTE_MBUF_DEFAULT_MEMPOOL_OPS,
					 NULL);
	if (ret) {
		CPT_LOG_ERR("Could not set mempool ops");
		goto mempool_free;
	}

	ret = rte_mempool_populate_default(pool);
	if (ret <= 0) {
		CPT_LOG_ERR("Could not populate metabuf pool");
		goto mempool_free;
	}

	meta_info = &qp->meta_info;

	meta_info->pool = pool;
	meta_info->lb_mlen = lb_mlen;
	meta_info->sg_mlen = sg_mlen;

	return 0;

mempool_free:
	rte_mempool_free(pool);
	return ret;
}

static void
otx2_cpt_metabuf_mempool_destroy(struct otx2_cpt_qp *qp)
{
	struct cpt_qp_meta_info *meta_info = &qp->meta_info;

	rte_mempool_free(meta_info->pool);

	meta_info->pool = NULL;
	meta_info->lb_mlen = 0;
	meta_info->sg_mlen = 0;
}

static int
otx2_cpt_qp_inline_cfg(const struct rte_cryptodev *dev, struct otx2_cpt_qp *qp)
{
	static rte_atomic16_t port_offset = RTE_ATOMIC16_INIT(-1);
	uint16_t port_id, nb_ethport = rte_eth_dev_count_avail();
	int i, ret;

	for (i = 0; i < nb_ethport; i++) {
		port_id = rte_atomic16_add_return(&port_offset, 1) % nb_ethport;
		if (otx2_eth_dev_is_sec_capable(&rte_eth_devices[port_id]))
			break;
	}

	if (i >= nb_ethport)
		return 0;

	ret = otx2_cpt_qp_ethdev_bind(dev, qp, port_id);
	if (ret)
		return ret;

	/* Publish inline Tx QP to eth dev security */
	ret = otx2_sec_idev_tx_cpt_qp_add(port_id, qp);
	if (ret)
		return ret;

	return 0;
}

static struct otx2_cpt_qp *
otx2_cpt_qp_create(const struct rte_cryptodev *dev, uint16_t qp_id,
		   uint8_t group)
{
	struct otx2_cpt_vf *vf = dev->data->dev_private;
	uint64_t pg_sz = sysconf(_SC_PAGESIZE);
	const struct rte_memzone *lf_mem;
	uint32_t len, iq_len, size_div40;
	char name[RTE_MEMZONE_NAMESIZE];
	uint64_t used_len, iova;
	struct otx2_cpt_qp *qp;
	uint64_t lmtline;
	uint8_t *va;
	int ret;

	/* Allocate queue pair */
	qp = rte_zmalloc_socket("OCTEON TX2 Crypto PMD Queue Pair", sizeof(*qp),
				OTX2_ALIGN, 0);
	if (qp == NULL) {
		CPT_LOG_ERR("Could not allocate queue pair");
		return NULL;
	}

	iq_len = OTX2_CPT_IQ_LEN;

	/*
	 * Queue size must be a multiple of 40 and effective queue size to
	 * software is (size_div40 - 1) * 40
	 */
	size_div40 = (iq_len + 40 - 1) / 40 + 1;

	/* For pending queue */
	len = iq_len * sizeof(uintptr_t);

	/* Space for instruction group memory */
	len += size_div40 * 16;

	/* So that instruction queues start as pg size aligned */
	len = RTE_ALIGN(len, pg_sz);

	/* For instruction queues */
	len += OTX2_CPT_IQ_LEN * sizeof(union cpt_inst_s);

	/* Wastage after instruction queues */
	len = RTE_ALIGN(len, pg_sz);

	qp_memzone_name_get(name, RTE_MEMZONE_NAMESIZE, dev->data->dev_id,
			    qp_id);

	lf_mem = rte_memzone_reserve_aligned(name, len, vf->otx2_dev.node,
			RTE_MEMZONE_SIZE_HINT_ONLY | RTE_MEMZONE_256MB,
			RTE_CACHE_LINE_SIZE);
	if (lf_mem == NULL) {
		CPT_LOG_ERR("Could not allocate reserved memzone");
		goto qp_free;
	}

	va = lf_mem->addr;
	iova = lf_mem->iova;

	memset(va, 0, len);

	ret = otx2_cpt_metabuf_mempool_create(dev, qp, qp_id, iq_len);
	if (ret) {
		CPT_LOG_ERR("Could not create mempool for metabuf");
		goto lf_mem_free;
	}

	/* Initialize pending queue */
	qp->pend_q.req_queue = (uintptr_t *)va;
	qp->pend_q.enq_tail = 0;
	qp->pend_q.deq_head = 0;
	qp->pend_q.pending_count = 0;

	used_len = iq_len * sizeof(uintptr_t);
	used_len += size_div40 * 16;
	used_len = RTE_ALIGN(used_len, pg_sz);
	iova += used_len;

	qp->iq_dma_addr = iova;
	qp->id = qp_id;
	qp->blkaddr = vf->lf_blkaddr[qp_id];
	qp->base = OTX2_CPT_LF_BAR2(vf, qp->blkaddr, qp_id);

	lmtline = vf->otx2_dev.bar2 +
		  (RVU_BLOCK_ADDR_LMT << 20 | qp_id << 12) +
		  OTX2_LMT_LF_LMTLINE(0);

	qp->lmtline = (void *)lmtline;

	qp->lf_nq_reg = qp->base + OTX2_CPT_LF_NQ(0);

	ret = otx2_sec_idev_tx_cpt_qp_remove(qp);
	if (ret && (ret != -ENOENT)) {
		CPT_LOG_ERR("Could not delete inline configuration");
		goto mempool_destroy;
	}

	otx2_cpt_iq_disable(qp);

	ret = otx2_cpt_qp_inline_cfg(dev, qp);
	if (ret) {
		CPT_LOG_ERR("Could not configure queue for inline IPsec");
		goto mempool_destroy;
	}

	ret = otx2_cpt_iq_enable(dev, qp, group, OTX2_CPT_QUEUE_HI_PRIO,
				 size_div40);
	if (ret) {
		CPT_LOG_ERR("Could not enable instruction queue");
		goto mempool_destroy;
	}

	return qp;

mempool_destroy:
	otx2_cpt_metabuf_mempool_destroy(qp);
lf_mem_free:
	rte_memzone_free(lf_mem);
qp_free:
	rte_free(qp);
	return NULL;
}

static int
otx2_cpt_qp_destroy(const struct rte_cryptodev *dev, struct otx2_cpt_qp *qp)
{
	const struct rte_memzone *lf_mem;
	char name[RTE_MEMZONE_NAMESIZE];
	int ret;

	ret = otx2_sec_idev_tx_cpt_qp_remove(qp);
	if (ret && (ret != -ENOENT)) {
		CPT_LOG_ERR("Could not delete inline configuration");
		return ret;
	}

	otx2_cpt_iq_disable(qp);

	otx2_cpt_metabuf_mempool_destroy(qp);

	qp_memzone_name_get(name, RTE_MEMZONE_NAMESIZE, dev->data->dev_id,
			    qp->id);

	lf_mem = rte_memzone_lookup(name);

	ret = rte_memzone_free(lf_mem);
	if (ret)
		return ret;

	rte_free(qp);

	return 0;
}

static int
sym_xform_verify(struct rte_crypto_sym_xform *xform)
{
	if (xform->next) {
		if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
		    xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
		    xform->next->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT &&
		    (xform->auth.algo != RTE_CRYPTO_AUTH_SHA1_HMAC ||
		     xform->next->cipher.algo != RTE_CRYPTO_CIPHER_AES_CBC))
			return -ENOTSUP;

		if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
		    xform->cipher.op == RTE_CRYPTO_CIPHER_OP_DECRYPT &&
		    xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
		    (xform->cipher.algo != RTE_CRYPTO_CIPHER_AES_CBC ||
		     xform->next->auth.algo != RTE_CRYPTO_AUTH_SHA1_HMAC))
			return -ENOTSUP;

		if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
		    xform->cipher.algo == RTE_CRYPTO_CIPHER_3DES_CBC &&
		    xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
		    xform->next->auth.algo == RTE_CRYPTO_AUTH_SHA1)
			return -ENOTSUP;

		if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
		    xform->auth.algo == RTE_CRYPTO_AUTH_SHA1 &&
		    xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
		    xform->next->cipher.algo == RTE_CRYPTO_CIPHER_3DES_CBC)
			return -ENOTSUP;

	} else {
		if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
		    xform->auth.algo == RTE_CRYPTO_AUTH_NULL &&
		    xform->auth.op == RTE_CRYPTO_AUTH_OP_VERIFY)
			return -ENOTSUP;
	}
	return 0;
}

static int
sym_session_configure(int driver_id, struct rte_crypto_sym_xform *xform,
		      struct rte_cryptodev_sym_session *sess,
		      struct rte_mempool *pool)
{
	struct rte_crypto_sym_xform *temp_xform = xform;
	struct cpt_sess_misc *misc;
	vq_cmd_word3_t vq_cmd_w3;
	void *priv;
	int ret;

	ret = sym_xform_verify(xform);
	if (unlikely(ret))
		return ret;

	if (unlikely(rte_mempool_get(pool, &priv))) {
		CPT_LOG_ERR("Could not allocate session private data");
		return -ENOMEM;
	}

	memset(priv, 0, sizeof(struct cpt_sess_misc) +
			offsetof(struct cpt_ctx, mc_ctx));

	misc = priv;

	for ( ; xform != NULL; xform = xform->next) {
		switch (xform->type) {
		case RTE_CRYPTO_SYM_XFORM_AEAD:
			ret = fill_sess_aead(xform, misc);
			break;
		case RTE_CRYPTO_SYM_XFORM_CIPHER:
			ret = fill_sess_cipher(xform, misc);
			break;
		case RTE_CRYPTO_SYM_XFORM_AUTH:
			if (xform->auth.algo == RTE_CRYPTO_AUTH_AES_GMAC)
				ret = fill_sess_gmac(xform, misc);
			else
				ret = fill_sess_auth(xform, misc);
			break;
		default:
			ret = -1;
		}

		if (ret)
			goto priv_put;
	}

	if ((GET_SESS_FC_TYPE(misc) == HASH_HMAC) &&
			cpt_mac_len_verify(&temp_xform->auth)) {
		CPT_LOG_ERR("MAC length is not supported");
		struct cpt_ctx *ctx = SESS_PRIV(misc);
		if (ctx->auth_key != NULL) {
			rte_free(ctx->auth_key);
			ctx->auth_key = NULL;
		}
		ret = -ENOTSUP;
		goto priv_put;
	}

	set_sym_session_private_data(sess, driver_id, misc);

	misc->ctx_dma_addr = rte_mempool_virt2iova(misc) +
			     sizeof(struct cpt_sess_misc);

	vq_cmd_w3.u64 = 0;
	vq_cmd_w3.s.cptr = misc->ctx_dma_addr + offsetof(struct cpt_ctx,
							 mc_ctx);

	/*
	 * IE engines support IPsec operations
	 * SE engines support IPsec operations, Chacha-Poly and
	 * Air-Crypto operations
	 */
	if (misc->zsk_flag || misc->chacha_poly)
		vq_cmd_w3.s.grp = OTX2_CPT_EGRP_SE;
	else
		vq_cmd_w3.s.grp = OTX2_CPT_EGRP_SE_IE;

	misc->cpt_inst_w7 = vq_cmd_w3.u64;

	return 0;

priv_put:
	rte_mempool_put(pool, priv);

	return -ENOTSUP;
}

static __rte_always_inline int32_t __rte_hot
otx2_ca_enqueue_req(const struct otx2_cpt_qp *qp,
		    struct cpt_request_info *req,
		    void *lmtline,
		    struct rte_crypto_op *op,
		    uint64_t cpt_inst_w7)
{
	union rte_event_crypto_metadata *m_data;
	union cpt_inst_s inst;
	uint64_t lmt_status;

	if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
		m_data = rte_cryptodev_sym_session_get_user_data(
						op->sym->session);
		if (m_data == NULL) {
			rte_pktmbuf_free(op->sym->m_src);
			rte_crypto_op_free(op);
			rte_errno = EINVAL;
			return -EINVAL;
		}
	} else if (op->sess_type == RTE_CRYPTO_OP_SESSIONLESS &&
		   op->private_data_offset) {
		m_data = (union rte_event_crypto_metadata *)
			 ((uint8_t *)op +
			  op->private_data_offset);
	} else {
		return -EINVAL;
	}

	inst.u[0] = 0;
	inst.s9x.res_addr = req->comp_baddr;
	inst.u[2] = 0;
	inst.u[3] = 0;

	inst.s9x.ei0 = req->ist.ei0;
	inst.s9x.ei1 = req->ist.ei1;
	inst.s9x.ei2 = req->ist.ei2;
	inst.s9x.ei3 = cpt_inst_w7;

	inst.u[2] = (((RTE_EVENT_TYPE_CRYPTODEV << 28) |
		      m_data->response_info.flow_id) |
		     ((uint64_t)m_data->response_info.sched_type << 32) |
		     ((uint64_t)m_data->response_info.queue_id << 34));
	inst.u[3] = 1 | (((uint64_t)req >> 3) << 3);
	req->qp = qp;

	do {
		/* Copy CPT command to LMTLINE */
		memcpy(lmtline, &inst, sizeof(inst));

		/*
		 * Make sure compiler does not reorder memcpy and ldeor.
		 * LMTST transactions are always flushed from the write
		 * buffer immediately, a DMB is not required to push out
		 * LMTSTs.
		 */
		rte_io_wmb();
		lmt_status = otx2_lmt_submit(qp->lf_nq_reg);
	} while (lmt_status == 0);

	return 0;
}

static __rte_always_inline int32_t __rte_hot
otx2_cpt_enqueue_req(const struct otx2_cpt_qp *qp,
		     struct pending_queue *pend_q,
		     struct cpt_request_info *req,
		     struct rte_crypto_op *op,
		     uint64_t cpt_inst_w7)
{
	void *lmtline = qp->lmtline;
	union cpt_inst_s inst;
	uint64_t lmt_status;

	if (qp->ca_enable)
		return otx2_ca_enqueue_req(qp, req, lmtline, op, cpt_inst_w7);

	if (unlikely(pend_q->pending_count >= OTX2_CPT_DEFAULT_CMD_QLEN))
		return -EAGAIN;

	inst.u[0] = 0;
	inst.s9x.res_addr = req->comp_baddr;
	inst.u[2] = 0;
	inst.u[3] = 0;

	inst.s9x.ei0 = req->ist.ei0;
	inst.s9x.ei1 = req->ist.ei1;
	inst.s9x.ei2 = req->ist.ei2;
	inst.s9x.ei3 = cpt_inst_w7;

	req->time_out = rte_get_timer_cycles() +
			DEFAULT_COMMAND_TIMEOUT * rte_get_timer_hz();

	do {
		/* Copy CPT command to LMTLINE */
		memcpy(lmtline, &inst, sizeof(inst));

		/*
		 * Make sure compiler does not reorder memcpy and ldeor.
		 * LMTST transactions are always flushed from the write
		 * buffer immediately, a DMB is not required to push out
		 * LMTSTs.
		 */
		rte_io_wmb();
		lmt_status = otx2_lmt_submit(qp->lf_nq_reg);
	} while (lmt_status == 0);

	pend_q->req_queue[pend_q->enq_tail] = (uintptr_t)req;

	/* We will use soft queue length here to limit requests */
	MOD_INC(pend_q->enq_tail, OTX2_CPT_DEFAULT_CMD_QLEN);
	pend_q->pending_count += 1;

	return 0;
}

static __rte_always_inline int32_t __rte_hot
otx2_cpt_enqueue_asym(struct otx2_cpt_qp *qp,
		      struct rte_crypto_op *op,
		      struct pending_queue *pend_q)
{
	struct cpt_qp_meta_info *minfo = &qp->meta_info;
	struct rte_crypto_asym_op *asym_op = op->asym;
	struct asym_op_params params = {0};
	struct cpt_asym_sess_misc *sess;
	uintptr_t *cop;
	void *mdata;
	int ret;

	if (unlikely(rte_mempool_get(minfo->pool, &mdata) < 0)) {
		CPT_LOG_ERR("Could not allocate meta buffer for request");
		return -ENOMEM;
	}

	sess = get_asym_session_private_data(asym_op->session,
					     otx2_cryptodev_driver_id);

	/* Store IO address of the mdata to meta_buf */
	params.meta_buf = rte_mempool_virt2iova(mdata);

	cop = mdata;
	cop[0] = (uintptr_t)mdata;
	cop[1] = (uintptr_t)op;
	cop[2] = cop[3] = 0ULL;

	params.req = RTE_PTR_ADD(cop, 4 * sizeof(uintptr_t));
	params.req->op = cop;

	/* Adjust meta_buf to point to end of cpt_request_info structure */
	params.meta_buf += (4 * sizeof(uintptr_t)) +
			    sizeof(struct cpt_request_info);
	switch (sess->xfrm_type) {
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		ret = cpt_modex_prep(&params, &sess->mod_ctx);
		if (unlikely(ret))
			goto req_fail;
		break;
	case RTE_CRYPTO_ASYM_XFORM_RSA:
		ret = cpt_enqueue_rsa_op(op, &params, sess);
		if (unlikely(ret))
			goto req_fail;
		break;
	case RTE_CRYPTO_ASYM_XFORM_ECDSA:
		ret = cpt_enqueue_ecdsa_op(op, &params, sess, otx2_fpm_iova);
		if (unlikely(ret))
			goto req_fail;
		break;
	case RTE_CRYPTO_ASYM_XFORM_ECPM:
		ret = cpt_ecpm_prep(&asym_op->ecpm, &params,
				    sess->ec_ctx.curveid);
		if (unlikely(ret))
			goto req_fail;
		break;
	default:
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		ret = -EINVAL;
		goto req_fail;
	}

	ret = otx2_cpt_enqueue_req(qp, pend_q, params.req, op,
				   sess->cpt_inst_w7);

	if (unlikely(ret)) {
		CPT_LOG_DP_ERR("Could not enqueue crypto req");
		goto req_fail;
	}

	return 0;

req_fail:
	free_op_meta(mdata, minfo->pool);

	return ret;
}

static __rte_always_inline int __rte_hot
otx2_cpt_enqueue_sym(struct otx2_cpt_qp *qp, struct rte_crypto_op *op,
		     struct pending_queue *pend_q)
{
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct cpt_request_info *req;
	struct cpt_sess_misc *sess;
	uint64_t cpt_op;
	void *mdata;
	int ret;

	sess = get_sym_session_private_data(sym_op->session,
					    otx2_cryptodev_driver_id);

	cpt_op = sess->cpt_op;

	if (cpt_op & CPT_OP_CIPHER_MASK)
		ret = fill_fc_params(op, sess, &qp->meta_info, &mdata,
				     (void **)&req);
	else
		ret = fill_digest_params(op, sess, &qp->meta_info, &mdata,
					 (void **)&req);

	if (unlikely(ret)) {
		CPT_LOG_DP_ERR("Crypto req : op %p, cpt_op 0x%x ret 0x%x",
				op, (unsigned int)cpt_op, ret);
		return ret;
	}

	ret = otx2_cpt_enqueue_req(qp, pend_q, req, op, sess->cpt_inst_w7);

	if (unlikely(ret)) {
		/* Free buffer allocated by fill params routines */
		free_op_meta(mdata, qp->meta_info.pool);
	}

	return ret;
}

static __rte_always_inline int __rte_hot
otx2_cpt_enqueue_sec(struct otx2_cpt_qp *qp, struct rte_crypto_op *op,
		     struct pending_queue *pend_q)
{
	uint32_t winsz, esn_low = 0, esn_hi = 0, seql = 0, seqh = 0;
	struct rte_mbuf *m_src = op->sym->m_src;
	struct otx2_sec_session_ipsec_lp *sess;
	struct otx2_ipsec_po_sa_ctl *ctl_wrd;
	struct otx2_ipsec_po_in_sa *sa;
	struct otx2_sec_session *priv;
	struct cpt_request_info *req;
	uint64_t seq_in_sa, seq = 0;
	uint8_t esn;
	int ret;

	priv = get_sec_session_private_data(op->sym->sec_session);
	sess = &priv->ipsec.lp;
	sa = &sess->in_sa;

	ctl_wrd = &sa->ctl;
	esn = ctl_wrd->esn_en;
	winsz = sa->replay_win_sz;

	if (ctl_wrd->direction == OTX2_IPSEC_PO_SA_DIRECTION_OUTBOUND)
		ret = process_outb_sa(op, sess, &qp->meta_info, (void **)&req);
	else {
		if (winsz) {
			esn_low = rte_be_to_cpu_32(sa->esn_low);
			esn_hi = rte_be_to_cpu_32(sa->esn_hi);
			seql = *rte_pktmbuf_mtod_offset(m_src, uint32_t *,
				sizeof(struct rte_ipv4_hdr) + 4);
			seql = rte_be_to_cpu_32(seql);

			if (!esn)
				seq = (uint64_t)seql;
			else {
				seqh = anti_replay_get_seqh(winsz, seql, esn_hi,
						esn_low);
				seq = ((uint64_t)seqh << 32) | seql;
			}

			if (unlikely(seq == 0))
				return IPSEC_ANTI_REPLAY_FAILED;

			ret = anti_replay_check(sa->replay, seq, winsz);
			if (unlikely(ret)) {
				otx2_err("Anti replay check failed");
				return IPSEC_ANTI_REPLAY_FAILED;
			}
		}

		ret = process_inb_sa(op, sess, &qp->meta_info, (void **)&req);
	}

	if (unlikely(ret)) {
		otx2_err("Crypto req : op %p, ret 0x%x", op, ret);
		return ret;
	}

	ret = otx2_cpt_enqueue_req(qp, pend_q, req, op, sess->cpt_inst_w7);

	if (winsz && esn) {
		seq_in_sa = ((uint64_t)esn_hi << 32) | esn_low;
		if (seq > seq_in_sa) {
			sa->esn_low = rte_cpu_to_be_32(seql);
			sa->esn_hi = rte_cpu_to_be_32(seqh);
		}
	}

	return ret;
}

static __rte_always_inline int __rte_hot
otx2_cpt_enqueue_sym_sessless(struct otx2_cpt_qp *qp, struct rte_crypto_op *op,
			      struct pending_queue *pend_q)
{
	const int driver_id = otx2_cryptodev_driver_id;
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct rte_cryptodev_sym_session *sess;
	int ret;

	/* Create temporary session */
	sess = rte_cryptodev_sym_session_create(qp->sess_mp);
	if (sess == NULL)
		return -ENOMEM;

	ret = sym_session_configure(driver_id, sym_op->xform, sess,
				    qp->sess_mp_priv);
	if (ret)
		goto sess_put;

	sym_op->session = sess;

	ret = otx2_cpt_enqueue_sym(qp, op, pend_q);

	if (unlikely(ret))
		goto priv_put;

	return 0;

priv_put:
	sym_session_clear(driver_id, sess);
sess_put:
	rte_mempool_put(qp->sess_mp, sess);
	return ret;
}

static uint16_t
otx2_cpt_enqueue_burst(void *qptr, struct rte_crypto_op **ops, uint16_t nb_ops)
{
	uint16_t nb_allowed, count = 0;
	struct otx2_cpt_qp *qp = qptr;
	struct pending_queue *pend_q;
	struct rte_crypto_op *op;
	int ret;

	pend_q = &qp->pend_q;

	nb_allowed = OTX2_CPT_DEFAULT_CMD_QLEN - pend_q->pending_count;
	if (nb_ops > nb_allowed)
		nb_ops = nb_allowed;

	for (count = 0; count < nb_ops; count++) {
		op = ops[count];
		if (op->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
			if (op->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION)
				ret = otx2_cpt_enqueue_sec(qp, op, pend_q);
			else if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION)
				ret = otx2_cpt_enqueue_sym(qp, op, pend_q);
			else
				ret = otx2_cpt_enqueue_sym_sessless(qp, op,
								    pend_q);
		} else if (op->type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC) {
			if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION)
				ret = otx2_cpt_enqueue_asym(qp, op, pend_q);
			else
				break;
		} else
			break;

		if (unlikely(ret))
			break;
	}

	return count;
}

static __rte_always_inline void
otx2_cpt_asym_rsa_op(struct rte_crypto_op *cop, struct cpt_request_info *req,
		     struct rte_crypto_rsa_xform *rsa_ctx)
{
	struct rte_crypto_rsa_op_param *rsa = &cop->asym->rsa;

	switch (rsa->op_type) {
	case RTE_CRYPTO_ASYM_OP_ENCRYPT:
		rsa->cipher.length = rsa_ctx->n.length;
		memcpy(rsa->cipher.data, req->rptr, rsa->cipher.length);
		break;
	case RTE_CRYPTO_ASYM_OP_DECRYPT:
		if (rsa->pad == RTE_CRYPTO_RSA_PADDING_NONE) {
			rsa->message.length = rsa_ctx->n.length;
			memcpy(rsa->message.data, req->rptr,
			       rsa->message.length);
		} else {
			/* Get length of decrypted output */
			rsa->message.length = rte_cpu_to_be_16
					     (*((uint16_t *)req->rptr));
			/*
			 * Offset output data pointer by length field
			 * (2 bytes) and copy decrypted data.
			 */
			memcpy(rsa->message.data, req->rptr + 2,
			       rsa->message.length);
		}
		break;
	case RTE_CRYPTO_ASYM_OP_SIGN:
		rsa->sign.length = rsa_ctx->n.length;
		memcpy(rsa->sign.data, req->rptr, rsa->sign.length);
		break;
	case RTE_CRYPTO_ASYM_OP_VERIFY:
		if (rsa->pad == RTE_CRYPTO_RSA_PADDING_NONE) {
			rsa->sign.length = rsa_ctx->n.length;
			memcpy(rsa->sign.data, req->rptr, rsa->sign.length);
		} else {
			/* Get length of signed output */
			rsa->sign.length = rte_cpu_to_be_16
					  (*((uint16_t *)req->rptr));
			/*
			 * Offset output data pointer by length field
			 * (2 bytes) and copy signed data.
			 */
			memcpy(rsa->sign.data, req->rptr + 2,
			       rsa->sign.length);
		}
		if (memcmp(rsa->sign.data, rsa->message.data,
			   rsa->message.length)) {
			CPT_LOG_DP_ERR("RSA verification failed");
			cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
		}
		break;
	default:
		CPT_LOG_DP_DEBUG("Invalid RSA operation type");
		cop->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		break;
	}
}

static __rte_always_inline void
otx2_cpt_asym_dequeue_ecdsa_op(struct rte_crypto_ecdsa_op_param *ecdsa,
			       struct cpt_request_info *req,
			       struct cpt_asym_ec_ctx *ec)
{
	int prime_len = ec_grp[ec->curveid].prime.length;

	if (ecdsa->op_type == RTE_CRYPTO_ASYM_OP_VERIFY)
		return;

	/* Separate out sign r and s components */
	memcpy(ecdsa->r.data, req->rptr, prime_len);
	memcpy(ecdsa->s.data, req->rptr + RTE_ALIGN_CEIL(prime_len, 8),
	       prime_len);
	ecdsa->r.length = prime_len;
	ecdsa->s.length = prime_len;
}

static __rte_always_inline void
otx2_cpt_asym_dequeue_ecpm_op(struct rte_crypto_ecpm_op_param *ecpm,
			     struct cpt_request_info *req,
			     struct cpt_asym_ec_ctx *ec)
{
	int prime_len = ec_grp[ec->curveid].prime.length;

	memcpy(ecpm->r.x.data, req->rptr, prime_len);
	memcpy(ecpm->r.y.data, req->rptr + RTE_ALIGN_CEIL(prime_len, 8),
	       prime_len);
	ecpm->r.x.length = prime_len;
	ecpm->r.y.length = prime_len;
}

static void
otx2_cpt_asym_post_process(struct rte_crypto_op *cop,
			   struct cpt_request_info *req)
{
	struct rte_crypto_asym_op *op = cop->asym;
	struct cpt_asym_sess_misc *sess;

	sess = get_asym_session_private_data(op->session,
					     otx2_cryptodev_driver_id);

	switch (sess->xfrm_type) {
	case RTE_CRYPTO_ASYM_XFORM_RSA:
		otx2_cpt_asym_rsa_op(cop, req, &sess->rsa_ctx);
		break;
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		op->modex.result.length = sess->mod_ctx.modulus.length;
		memcpy(op->modex.result.data, req->rptr,
		       op->modex.result.length);
		break;
	case RTE_CRYPTO_ASYM_XFORM_ECDSA:
		otx2_cpt_asym_dequeue_ecdsa_op(&op->ecdsa, req, &sess->ec_ctx);
		break;
	case RTE_CRYPTO_ASYM_XFORM_ECPM:
		otx2_cpt_asym_dequeue_ecpm_op(&op->ecpm, req, &sess->ec_ctx);
		break;
	default:
		CPT_LOG_DP_DEBUG("Invalid crypto xform type");
		cop->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		break;
	}
}

static void
otx2_cpt_sec_post_process(struct rte_crypto_op *cop, uintptr_t *rsp)
{
	struct cpt_request_info *req = (struct cpt_request_info *)rsp[2];
	vq_cmd_word0_t *word0 = (vq_cmd_word0_t *)&req->ist.ei0;
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct rte_mbuf *m = sym_op->m_src;
	struct rte_ipv6_hdr *ip6;
	struct rte_ipv4_hdr *ip;
	uint16_t m_len = 0;
	int mdata_len;
	char *data;

	mdata_len = (int)rsp[3];
	rte_pktmbuf_trim(m, mdata_len);

	if (word0->s.opcode.major == OTX2_IPSEC_PO_PROCESS_IPSEC_INB) {
		data = rte_pktmbuf_mtod(m, char *);

		if (rsp[4] == OTX2_IPSEC_PO_TRANSPORT ||
		    rsp[4] == OTX2_IPSEC_PO_TUNNEL_IPV4) {
			ip = (struct rte_ipv4_hdr *)(data +
				OTX2_IPSEC_PO_INB_RPTR_HDR);
			m_len = rte_be_to_cpu_16(ip->total_length);
		} else if (rsp[4] == OTX2_IPSEC_PO_TUNNEL_IPV6) {
			ip6 = (struct rte_ipv6_hdr *)(data +
				OTX2_IPSEC_PO_INB_RPTR_HDR);
			m_len = rte_be_to_cpu_16(ip6->payload_len) +
				sizeof(struct rte_ipv6_hdr);
		}

		m->data_len = m_len;
		m->pkt_len = m_len;
		m->data_off += OTX2_IPSEC_PO_INB_RPTR_HDR;
	}
}

static inline void
otx2_cpt_dequeue_post_process(struct otx2_cpt_qp *qp, struct rte_crypto_op *cop,
			      uintptr_t *rsp, uint8_t cc)
{
	unsigned int sz;

	if (cop->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
		if (cop->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION) {
			if (likely(cc == OTX2_IPSEC_PO_CC_SUCCESS)) {
				otx2_cpt_sec_post_process(cop, rsp);
				cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
			} else
				cop->status = RTE_CRYPTO_OP_STATUS_ERROR;

			return;
		}

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
			sz = rte_cryptodev_sym_get_existing_header_session_size(
					cop->sym->session);
			memset(cop->sym->session, 0, sz);
			rte_mempool_put(qp->sess_mp, cop->sym->session);
			cop->sym->session = NULL;
		}
	}

	if (cop->type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC) {
		if (likely(cc == NO_ERR)) {
			cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
			/*
			 * Pass cpt_req_info stored in metabuf during
			 * enqueue.
			 */
			rsp = RTE_PTR_ADD(rsp, 4 * sizeof(uintptr_t));
			otx2_cpt_asym_post_process(cop,
					(struct cpt_request_info *)rsp);
		} else
			cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
	}
}

static uint16_t
otx2_cpt_dequeue_burst(void *qptr, struct rte_crypto_op **ops, uint16_t nb_ops)
{
	int i, nb_pending, nb_completed;
	struct otx2_cpt_qp *qp = qptr;
	struct pending_queue *pend_q;
	struct cpt_request_info *req;
	struct rte_crypto_op *cop;
	uint8_t cc[nb_ops];
	uintptr_t *rsp;
	void *metabuf;

	pend_q = &qp->pend_q;

	nb_pending = pend_q->pending_count;

	if (nb_ops > nb_pending)
		nb_ops = nb_pending;

	for (i = 0; i < nb_ops; i++) {
		req = (struct cpt_request_info *)
				pend_q->req_queue[pend_q->deq_head];

		cc[i] = otx2_cpt_compcode_get(req);

		if (unlikely(cc[i] == ERR_REQ_PENDING))
			break;

		ops[i] = req->op;

		MOD_INC(pend_q->deq_head, OTX2_CPT_DEFAULT_CMD_QLEN);
		pend_q->pending_count -= 1;
	}

	nb_completed = i;

	for (i = 0; i < nb_completed; i++) {
		rsp = (void *)ops[i];

		metabuf = (void *)rsp[0];
		cop = (void *)rsp[1];

		ops[i] = cop;

		otx2_cpt_dequeue_post_process(qp, cop, rsp, cc[i]);

		free_op_meta(metabuf, qp->meta_info.pool);
	}

	return nb_completed;
}

void
otx2_cpt_set_enqdeq_fns(struct rte_cryptodev *dev)
{
	dev->enqueue_burst = otx2_cpt_enqueue_burst;
	dev->dequeue_burst = otx2_cpt_dequeue_burst;

	rte_mb();
}

/* PMD ops */

static int
otx2_cpt_dev_config(struct rte_cryptodev *dev,
		    struct rte_cryptodev_config *conf)
{
	struct otx2_cpt_vf *vf = dev->data->dev_private;
	int ret;

	if (conf->nb_queue_pairs > vf->max_queues) {
		CPT_LOG_ERR("Invalid number of queue pairs requested");
		return -EINVAL;
	}

	dev->feature_flags = otx2_cpt_default_ff_get() & ~conf->ff_disable;

	if (dev->feature_flags & RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO) {
		/* Initialize shared FPM table */
		ret = cpt_fpm_init(otx2_fpm_iova);
		if (ret)
			return ret;
	}

	/* Unregister error interrupts */
	if (vf->err_intr_registered)
		otx2_cpt_err_intr_unregister(dev);

	/* Detach queues */
	if (vf->nb_queues) {
		ret = otx2_cpt_queues_detach(dev);
		if (ret) {
			CPT_LOG_ERR("Could not detach CPT queues");
			return ret;
		}
	}

	/* Attach queues */
	ret = otx2_cpt_queues_attach(dev, conf->nb_queue_pairs);
	if (ret) {
		CPT_LOG_ERR("Could not attach CPT queues");
		return -ENODEV;
	}

	ret = otx2_cpt_msix_offsets_get(dev);
	if (ret) {
		CPT_LOG_ERR("Could not get MSI-X offsets");
		goto queues_detach;
	}

	/* Register error interrupts */
	ret = otx2_cpt_err_intr_register(dev);
	if (ret) {
		CPT_LOG_ERR("Could not register error interrupts");
		goto queues_detach;
	}

	ret = otx2_cpt_inline_init(dev);
	if (ret) {
		CPT_LOG_ERR("Could not enable inline IPsec");
		goto intr_unregister;
	}

	otx2_cpt_set_enqdeq_fns(dev);

	return 0;

intr_unregister:
	otx2_cpt_err_intr_unregister(dev);
queues_detach:
	otx2_cpt_queues_detach(dev);
	return ret;
}

static int
otx2_cpt_dev_start(struct rte_cryptodev *dev)
{
	RTE_SET_USED(dev);

	CPT_PMD_INIT_FUNC_TRACE();

	return 0;
}

static void
otx2_cpt_dev_stop(struct rte_cryptodev *dev)
{
	CPT_PMD_INIT_FUNC_TRACE();

	if (dev->feature_flags & RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO)
		cpt_fpm_clear();
}

static int
otx2_cpt_dev_close(struct rte_cryptodev *dev)
{
	struct otx2_cpt_vf *vf = dev->data->dev_private;
	int i, ret = 0;

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		ret = otx2_cpt_queue_pair_release(dev, i);
		if (ret)
			return ret;
	}

	/* Unregister error interrupts */
	if (vf->err_intr_registered)
		otx2_cpt_err_intr_unregister(dev);

	/* Detach queues */
	if (vf->nb_queues) {
		ret = otx2_cpt_queues_detach(dev);
		if (ret)
			CPT_LOG_ERR("Could not detach CPT queues");
	}

	return ret;
}

static void
otx2_cpt_dev_info_get(struct rte_cryptodev *dev,
		      struct rte_cryptodev_info *info)
{
	struct otx2_cpt_vf *vf = dev->data->dev_private;

	if (info != NULL) {
		info->max_nb_queue_pairs = vf->max_queues;
		info->feature_flags = otx2_cpt_default_ff_get();
		info->capabilities = otx2_cpt_capabilities_get();
		info->sym.max_nb_sessions = 0;
		info->driver_id = otx2_cryptodev_driver_id;
		info->min_mbuf_headroom_req = OTX2_CPT_MIN_HEADROOM_REQ;
		info->min_mbuf_tailroom_req = OTX2_CPT_MIN_TAILROOM_REQ;
	}
}

static int
otx2_cpt_queue_pair_setup(struct rte_cryptodev *dev, uint16_t qp_id,
			  const struct rte_cryptodev_qp_conf *conf,
			  int socket_id __rte_unused)
{
	uint8_t grp_mask = OTX2_CPT_ENG_GRPS_MASK;
	struct rte_pci_device *pci_dev;
	struct otx2_cpt_qp *qp;

	CPT_PMD_INIT_FUNC_TRACE();

	if (dev->data->queue_pairs[qp_id] != NULL)
		otx2_cpt_queue_pair_release(dev, qp_id);

	if (conf->nb_descriptors > OTX2_CPT_DEFAULT_CMD_QLEN) {
		CPT_LOG_ERR("Could not setup queue pair for %u descriptors",
			    conf->nb_descriptors);
		return -EINVAL;
	}

	pci_dev = RTE_DEV_TO_PCI(dev->device);

	if (pci_dev->mem_resource[2].addr == NULL) {
		CPT_LOG_ERR("Invalid PCI mem address");
		return -EIO;
	}

	qp = otx2_cpt_qp_create(dev, qp_id, grp_mask);
	if (qp == NULL) {
		CPT_LOG_ERR("Could not create queue pair %d", qp_id);
		return -ENOMEM;
	}

	qp->sess_mp = conf->mp_session;
	qp->sess_mp_priv = conf->mp_session_private;
	dev->data->queue_pairs[qp_id] = qp;

	return 0;
}

static int
otx2_cpt_queue_pair_release(struct rte_cryptodev *dev, uint16_t qp_id)
{
	struct otx2_cpt_qp *qp = dev->data->queue_pairs[qp_id];
	int ret;

	CPT_PMD_INIT_FUNC_TRACE();

	if (qp == NULL)
		return -EINVAL;

	CPT_LOG_INFO("Releasing queue pair %d", qp_id);

	ret = otx2_cpt_qp_destroy(dev, qp);
	if (ret) {
		CPT_LOG_ERR("Could not destroy queue pair %d", qp_id);
		return ret;
	}

	dev->data->queue_pairs[qp_id] = NULL;

	return 0;
}

static unsigned int
otx2_cpt_sym_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	return cpt_get_session_size();
}

static int
otx2_cpt_sym_session_configure(struct rte_cryptodev *dev,
			       struct rte_crypto_sym_xform *xform,
			       struct rte_cryptodev_sym_session *sess,
			       struct rte_mempool *pool)
{
	CPT_PMD_INIT_FUNC_TRACE();

	return sym_session_configure(dev->driver_id, xform, sess, pool);
}

static void
otx2_cpt_sym_session_clear(struct rte_cryptodev *dev,
			   struct rte_cryptodev_sym_session *sess)
{
	CPT_PMD_INIT_FUNC_TRACE();

	return sym_session_clear(dev->driver_id, sess);
}

static unsigned int
otx2_cpt_asym_session_size_get(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct cpt_asym_sess_misc);
}

static int
otx2_cpt_asym_session_cfg(struct rte_cryptodev *dev,
			  struct rte_crypto_asym_xform *xform,
			  struct rte_cryptodev_asym_session *sess,
			  struct rte_mempool *pool)
{
	struct cpt_asym_sess_misc *priv;
	vq_cmd_word3_t vq_cmd_w3;
	int ret;

	CPT_PMD_INIT_FUNC_TRACE();

	if (rte_mempool_get(pool, (void **)&priv)) {
		CPT_LOG_ERR("Could not allocate session_private_data");
		return -ENOMEM;
	}

	memset(priv, 0, sizeof(struct cpt_asym_sess_misc));

	ret = cpt_fill_asym_session_parameters(priv, xform);
	if (ret) {
		CPT_LOG_ERR("Could not configure session parameters");

		/* Return session to mempool */
		rte_mempool_put(pool, priv);
		return ret;
	}

	vq_cmd_w3.u64 = 0;
	vq_cmd_w3.s.grp = OTX2_CPT_EGRP_AE;
	priv->cpt_inst_w7 = vq_cmd_w3.u64;

	set_asym_session_private_data(sess, dev->driver_id, priv);

	return 0;
}

static void
otx2_cpt_asym_session_clear(struct rte_cryptodev *dev,
			    struct rte_cryptodev_asym_session *sess)
{
	struct cpt_asym_sess_misc *priv;
	struct rte_mempool *sess_mp;

	CPT_PMD_INIT_FUNC_TRACE();

	priv = get_asym_session_private_data(sess, dev->driver_id);
	if (priv == NULL)
		return;

	/* Free resources allocated in session_cfg */
	cpt_free_asym_session_parameters(priv);

	/* Reset and free object back to pool */
	memset(priv, 0, otx2_cpt_asym_session_size_get(dev));
	sess_mp = rte_mempool_from_obj(priv);
	set_asym_session_private_data(sess, dev->driver_id, NULL);
	rte_mempool_put(sess_mp, priv);
}

struct rte_cryptodev_ops otx2_cpt_ops = {
	/* Device control ops */
	.dev_configure = otx2_cpt_dev_config,
	.dev_start = otx2_cpt_dev_start,
	.dev_stop = otx2_cpt_dev_stop,
	.dev_close = otx2_cpt_dev_close,
	.dev_infos_get = otx2_cpt_dev_info_get,

	.stats_get = NULL,
	.stats_reset = NULL,
	.queue_pair_setup = otx2_cpt_queue_pair_setup,
	.queue_pair_release = otx2_cpt_queue_pair_release,

	/* Symmetric crypto ops */
	.sym_session_get_size = otx2_cpt_sym_session_get_size,
	.sym_session_configure = otx2_cpt_sym_session_configure,
	.sym_session_clear = otx2_cpt_sym_session_clear,

	/* Asymmetric crypto ops */
	.asym_session_get_size = otx2_cpt_asym_session_size_get,
	.asym_session_configure = otx2_cpt_asym_session_cfg,
	.asym_session_clear = otx2_cpt_asym_session_clear,

};
