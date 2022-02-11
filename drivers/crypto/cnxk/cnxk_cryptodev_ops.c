/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include <rte_errno.h>

#include "roc_cpt.h"

#include "cnxk_ae.h"
#include "cnxk_cryptodev.h"
#include "cnxk_cryptodev_ops.h"
#include "cnxk_cryptodev_capabilities.h"
#include "cnxk_se.h"

#define CNXK_CPT_MAX_ASYM_OP_NUM_PARAMS 5
#define CNXK_CPT_MAX_ASYM_OP_MOD_LEN	1024

static int
cnxk_cpt_get_mlen(void)
{
	uint32_t len;

	/* For MAC */
	len = 2 * sizeof(uint64_t);
	len += ROC_SE_MAX_MAC_LEN * sizeof(uint8_t);

	len += ROC_SE_OFF_CTRL_LEN + ROC_CPT_AES_CBC_IV_LEN;
	len += RTE_ALIGN_CEIL((ROC_SE_SG_LIST_HDR_SIZE +
			       (RTE_ALIGN_CEIL(ROC_SE_MAX_SG_IN_OUT_CNT, 4) >>
				2) * ROC_SE_SG_ENTRY_SIZE),
			      8);

	return len;
}

static int
cnxk_cpt_asym_get_mlen(void)
{
	uint32_t len;

	/* To hold RPTR */
	len = sizeof(uint64_t);

	/* Get meta len for asymmetric operations */
	len += CNXK_CPT_MAX_ASYM_OP_NUM_PARAMS * CNXK_CPT_MAX_ASYM_OP_MOD_LEN;

	return len;
}

int
cnxk_cpt_dev_config(struct rte_cryptodev *dev,
		    struct rte_cryptodev_config *conf)
{
	struct cnxk_cpt_vf *vf = dev->data->dev_private;
	struct roc_cpt *roc_cpt = &vf->cpt;
	uint16_t nb_lf_avail, nb_lf;
	int ret;

	dev->feature_flags = cnxk_cpt_default_ff_get() & ~conf->ff_disable;

	nb_lf_avail = roc_cpt->nb_lf_avail;
	nb_lf = conf->nb_queue_pairs;

	if (nb_lf > nb_lf_avail)
		return -ENOTSUP;

	ret = roc_cpt_dev_configure(roc_cpt, nb_lf);
	if (ret) {
		plt_err("Could not configure device");
		return ret;
	}

	if (dev->feature_flags & RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO) {
		/* Initialize shared FPM table */
		ret = roc_ae_fpm_get(vf->cnxk_fpm_iova);
		if (ret) {
			plt_err("Could not get FPM table");
			return ret;
		}

		/* Init EC grp table */
		ret = roc_ae_ec_grp_get(vf->ec_grp);
		if (ret) {
			plt_err("Could not get EC grp table");
			roc_ae_fpm_put();
			return ret;
		}
	}

	return 0;
}

int
cnxk_cpt_dev_start(struct rte_cryptodev *dev)
{
	struct cnxk_cpt_vf *vf = dev->data->dev_private;
	struct roc_cpt *roc_cpt = &vf->cpt;
	uint16_t nb_lf = roc_cpt->nb_lf;
	uint16_t qp_id;

	for (qp_id = 0; qp_id < nb_lf; qp_id++) {
		/* Application may not setup all queue pair */
		if (roc_cpt->lf[qp_id] == NULL)
			continue;

		roc_cpt_iq_enable(roc_cpt->lf[qp_id]);
	}

	return 0;
}

void
cnxk_cpt_dev_stop(struct rte_cryptodev *dev)
{
	struct cnxk_cpt_vf *vf = dev->data->dev_private;
	struct roc_cpt *roc_cpt = &vf->cpt;
	uint16_t nb_lf = roc_cpt->nb_lf;
	uint16_t qp_id;

	for (qp_id = 0; qp_id < nb_lf; qp_id++) {
		if (roc_cpt->lf[qp_id] == NULL)
			continue;

		roc_cpt_iq_disable(roc_cpt->lf[qp_id]);
	}
}

int
cnxk_cpt_dev_close(struct rte_cryptodev *dev)
{
	struct cnxk_cpt_vf *vf = dev->data->dev_private;
	uint16_t i;
	int ret;

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		ret = cnxk_cpt_queue_pair_release(dev, i);
		if (ret < 0) {
			plt_err("Could not release queue pair %u", i);
			return ret;
		}
	}

	if (dev->feature_flags & RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO) {
		roc_ae_fpm_put();
		roc_ae_ec_grp_put();
	}

	roc_cpt_dev_clear(&vf->cpt);

	return 0;
}

void
cnxk_cpt_dev_info_get(struct rte_cryptodev *dev,
		      struct rte_cryptodev_info *info)
{
	struct cnxk_cpt_vf *vf = dev->data->dev_private;
	struct roc_cpt *roc_cpt = &vf->cpt;

	info->max_nb_queue_pairs =
		RTE_MIN(roc_cpt->nb_lf_avail, vf->max_qps_limit);
	plt_cpt_dbg("max_nb_queue_pairs %u", info->max_nb_queue_pairs);

	info->feature_flags = cnxk_cpt_default_ff_get();
	info->capabilities = cnxk_crypto_capabilities_get(vf);
	info->sym.max_nb_sessions = 0;
	info->min_mbuf_headroom_req = CNXK_CPT_MIN_HEADROOM_REQ;
	info->min_mbuf_tailroom_req = CNXK_CPT_MIN_TAILROOM_REQ;
}

static void
qp_memzone_name_get(char *name, int size, int dev_id, int qp_id)
{
	snprintf(name, size, "cnxk_cpt_pq_mem_%u:%u", dev_id, qp_id);
}

static int
cnxk_cpt_metabuf_mempool_create(const struct rte_cryptodev *dev,
				struct cnxk_cpt_qp *qp, uint8_t qp_id,
				uint32_t nb_elements)
{
	char mempool_name[RTE_MEMPOOL_NAMESIZE];
	struct cpt_qp_meta_info *meta_info;
	int lcore_cnt = rte_lcore_count();
	struct rte_mempool *pool;
	int mb_pool_sz, mlen = 8;
	uint32_t cache_sz;

	if (dev->feature_flags & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO) {
		/* Get meta len */
		mlen = cnxk_cpt_get_mlen();
	}

	if (dev->feature_flags & RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO) {

		/* Get meta len required for asymmetric operations */
		mlen = RTE_MAX(mlen, cnxk_cpt_asym_get_mlen());
	}

	mb_pool_sz = nb_elements;
	cache_sz = RTE_MIN(RTE_MEMPOOL_CACHE_MAX_SIZE, nb_elements / 1.5);

	/* For poll mode, core that enqueues and core that dequeues can be
	 * different. For event mode, all cores are allowed to use same crypto
	 * queue pair.
	 */

	mb_pool_sz += (RTE_MAX(2, lcore_cnt) * cache_sz);

	/* Allocate mempool */

	snprintf(mempool_name, RTE_MEMPOOL_NAMESIZE, "cnxk_cpt_mb_%u:%u",
		 dev->data->dev_id, qp_id);

	pool = rte_mempool_create(mempool_name, mb_pool_sz, mlen, cache_sz, 0,
				  NULL, NULL, NULL, NULL, rte_socket_id(), 0);

	if (pool == NULL) {
		plt_err("Could not create mempool for metabuf");
		return rte_errno;
	}

	meta_info = &qp->meta_info;

	meta_info->pool = pool;
	meta_info->mlen = mlen;

	return 0;
}

static void
cnxk_cpt_metabuf_mempool_destroy(struct cnxk_cpt_qp *qp)
{
	struct cpt_qp_meta_info *meta_info = &qp->meta_info;

	rte_mempool_free(meta_info->pool);

	meta_info->pool = NULL;
	meta_info->mlen = 0;
}

static struct cnxk_cpt_qp *
cnxk_cpt_qp_create(const struct rte_cryptodev *dev, uint16_t qp_id,
		   uint32_t iq_len)
{
	const struct rte_memzone *pq_mem;
	char name[RTE_MEMZONE_NAMESIZE];
	struct cnxk_cpt_qp *qp;
	uint32_t len;
	uint8_t *va;
	int ret;

	/* Allocate queue pair */
	qp = rte_zmalloc_socket("CNXK Crypto PMD Queue Pair", sizeof(*qp),
				ROC_ALIGN, 0);
	if (qp == NULL) {
		plt_err("Could not allocate queue pair");
		return NULL;
	}

	/* For pending queue */
	len = iq_len * sizeof(struct cpt_inflight_req);

	qp_memzone_name_get(name, RTE_MEMZONE_NAMESIZE, dev->data->dev_id,
			    qp_id);

	pq_mem = rte_memzone_reserve_aligned(name, len, rte_socket_id(),
					     RTE_MEMZONE_SIZE_HINT_ONLY |
						     RTE_MEMZONE_256MB,
					     RTE_CACHE_LINE_SIZE);
	if (pq_mem == NULL) {
		plt_err("Could not allocate reserved memzone");
		goto qp_free;
	}

	va = pq_mem->addr;

	memset(va, 0, len);

	ret = cnxk_cpt_metabuf_mempool_create(dev, qp, qp_id, iq_len);
	if (ret) {
		plt_err("Could not create mempool for metabuf");
		goto pq_mem_free;
	}

	/* Initialize pending queue */
	qp->pend_q.req_queue = pq_mem->addr;
	qp->pend_q.head = 0;
	qp->pend_q.tail = 0;

	return qp;

pq_mem_free:
	rte_memzone_free(pq_mem);
qp_free:
	rte_free(qp);
	return NULL;
}

static int
cnxk_cpt_qp_destroy(const struct rte_cryptodev *dev, struct cnxk_cpt_qp *qp)
{
	const struct rte_memzone *pq_mem;
	char name[RTE_MEMZONE_NAMESIZE];
	int ret;

	cnxk_cpt_metabuf_mempool_destroy(qp);

	qp_memzone_name_get(name, RTE_MEMZONE_NAMESIZE, dev->data->dev_id,
			    qp->lf.lf_id);

	pq_mem = rte_memzone_lookup(name);

	ret = rte_memzone_free(pq_mem);
	if (ret)
		return ret;

	rte_free(qp);

	return 0;
}

int
cnxk_cpt_queue_pair_release(struct rte_cryptodev *dev, uint16_t qp_id)
{
	struct cnxk_cpt_qp *qp = dev->data->queue_pairs[qp_id];
	struct cnxk_cpt_vf *vf = dev->data->dev_private;
	struct roc_cpt *roc_cpt = &vf->cpt;
	struct roc_cpt_lf *lf;
	int ret;

	if (qp == NULL)
		return -EINVAL;

	lf = roc_cpt->lf[qp_id];
	if (lf == NULL)
		return -ENOTSUP;

	roc_cpt_lf_fini(lf);

	ret = cnxk_cpt_qp_destroy(dev, qp);
	if (ret) {
		plt_err("Could not destroy queue pair %d", qp_id);
		return ret;
	}

	roc_cpt->lf[qp_id] = NULL;
	dev->data->queue_pairs[qp_id] = NULL;

	return 0;
}

int
cnxk_cpt_queue_pair_setup(struct rte_cryptodev *dev, uint16_t qp_id,
			  const struct rte_cryptodev_qp_conf *conf,
			  int socket_id __rte_unused)
{
	struct cnxk_cpt_vf *vf = dev->data->dev_private;
	struct roc_cpt *roc_cpt = &vf->cpt;
	struct rte_pci_device *pci_dev;
	struct cnxk_cpt_qp *qp;
	uint32_t nb_desc;
	int ret;

	if (dev->data->queue_pairs[qp_id] != NULL)
		cnxk_cpt_queue_pair_release(dev, qp_id);

	pci_dev = RTE_DEV_TO_PCI(dev->device);

	if (pci_dev->mem_resource[2].addr == NULL) {
		plt_err("Invalid PCI mem address");
		return -EIO;
	}

	/* Update nb_desc to next power of 2 to aid in pending queue checks */
	nb_desc = plt_align32pow2(conf->nb_descriptors);

	qp = cnxk_cpt_qp_create(dev, qp_id, nb_desc);
	if (qp == NULL) {
		plt_err("Could not create queue pair %d", qp_id);
		return -ENOMEM;
	}

	qp->lf.lf_id = qp_id;
	qp->lf.nb_desc = nb_desc;

	ret = roc_cpt_lf_init(roc_cpt, &qp->lf);
	if (ret < 0) {
		plt_err("Could not initialize queue pair %d", qp_id);
		ret = -EINVAL;
		goto exit;
	}

	qp->pend_q.pq_mask = qp->lf.nb_desc - 1;

	roc_cpt->lf[qp_id] = &qp->lf;

	ret = roc_cpt_lmtline_init(roc_cpt, &qp->lmtline, qp_id);
	if (ret < 0) {
		roc_cpt->lf[qp_id] = NULL;
		plt_err("Could not init lmtline for queue pair %d", qp_id);
		goto exit;
	}

	qp->sess_mp = conf->mp_session;
	qp->sess_mp_priv = conf->mp_session_private;
	dev->data->queue_pairs[qp_id] = qp;

	return 0;

exit:
	cnxk_cpt_qp_destroy(dev, qp);
	return ret;
}

unsigned int
cnxk_cpt_sym_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct cnxk_se_sess);
}

static int
cnxk_sess_fill(struct rte_crypto_sym_xform *xform, struct cnxk_se_sess *sess)
{
	struct rte_crypto_sym_xform *aead_xfrm = NULL;
	struct rte_crypto_sym_xform *c_xfrm = NULL;
	struct rte_crypto_sym_xform *a_xfrm = NULL;
	bool ciph_then_auth = false;

	if (xform == NULL)
		return -EINVAL;

	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		c_xfrm = xform;
		a_xfrm = xform->next;
		ciph_then_auth = true;
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		c_xfrm = xform->next;
		a_xfrm = xform;
		ciph_then_auth = false;
	} else {
		aead_xfrm = xform;
	}

	if (c_xfrm != NULL && c_xfrm->type != RTE_CRYPTO_SYM_XFORM_CIPHER) {
		plt_dp_err("Invalid type in cipher xform");
		return -EINVAL;
	}

	if (a_xfrm != NULL && a_xfrm->type != RTE_CRYPTO_SYM_XFORM_AUTH) {
		plt_dp_err("Invalid type in auth xform");
		return -EINVAL;
	}

	if (aead_xfrm != NULL && aead_xfrm->type != RTE_CRYPTO_SYM_XFORM_AEAD) {
		plt_dp_err("Invalid type in AEAD xform");
		return -EINVAL;
	}

	if ((c_xfrm == NULL || c_xfrm->cipher.algo == RTE_CRYPTO_CIPHER_NULL) &&
	    a_xfrm != NULL && a_xfrm->auth.algo == RTE_CRYPTO_AUTH_NULL &&
	    a_xfrm->auth.op == RTE_CRYPTO_AUTH_OP_VERIFY) {
		plt_dp_err("Null cipher + null auth verify is not supported");
		return -ENOTSUP;
	}

	/* Cipher only */
	if (c_xfrm != NULL &&
	    (a_xfrm == NULL || a_xfrm->auth.algo == RTE_CRYPTO_AUTH_NULL)) {
		if (fill_sess_cipher(c_xfrm, sess))
			return -ENOTSUP;
		else
			return 0;
	}

	/* Auth only */
	if (a_xfrm != NULL &&
	    (c_xfrm == NULL || c_xfrm->cipher.algo == RTE_CRYPTO_CIPHER_NULL)) {
		if (fill_sess_auth(a_xfrm, sess))
			return -ENOTSUP;
		else
			return 0;
	}

	/* AEAD */
	if (aead_xfrm != NULL) {
		if (fill_sess_aead(aead_xfrm, sess))
			return -ENOTSUP;
		else
			return 0;
	}

	/* Chained ops */
	if (c_xfrm == NULL || a_xfrm == NULL) {
		plt_dp_err("Invalid xforms");
		return -EINVAL;
	}

	if (c_xfrm->cipher.algo == RTE_CRYPTO_CIPHER_3DES_CBC &&
	    a_xfrm->auth.algo == RTE_CRYPTO_AUTH_SHA1) {
		plt_dp_err("3DES-CBC + SHA1 is not supported");
		return -ENOTSUP;
	}

	/* Cipher then auth */
	if (ciph_then_auth) {
		if (fill_sess_cipher(c_xfrm, sess))
			return -ENOTSUP;
		if (fill_sess_auth(a_xfrm, sess))
			return -ENOTSUP;
		else
			return 0;
	}

	/* else */

	if (c_xfrm->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
		switch (a_xfrm->auth.algo) {
		case RTE_CRYPTO_AUTH_SHA1_HMAC:
			switch (c_xfrm->cipher.algo) {
			case RTE_CRYPTO_CIPHER_AES_CBC:
				break;
			default:
				return -ENOTSUP;
			}
			break;
		default:
			return -ENOTSUP;
		}
	}

	if (fill_sess_auth(a_xfrm, sess))
		return -ENOTSUP;
	if (fill_sess_cipher(c_xfrm, sess))
		return -ENOTSUP;
	else
		return 0;
}

static uint64_t
cnxk_cpt_inst_w7_get(struct cnxk_se_sess *sess, struct roc_cpt *roc_cpt)
{
	union cpt_inst_w7 inst_w7;

	inst_w7.s.cptr = (uint64_t)&sess->roc_se_ctx.se_ctx;

	/* Set the engine group */
	if (sess->zsk_flag || sess->chacha_poly)
		inst_w7.s.egrp = roc_cpt->eng_grp[CPT_ENG_TYPE_SE];
	else
		inst_w7.s.egrp = roc_cpt->eng_grp[CPT_ENG_TYPE_IE];

	return inst_w7.u64;
}

int
sym_session_configure(struct roc_cpt *roc_cpt, int driver_id,
		      struct rte_crypto_sym_xform *xform,
		      struct rte_cryptodev_sym_session *sess,
		      struct rte_mempool *pool)
{
	struct cnxk_se_sess *sess_priv;
	void *priv;
	int ret;

	if (unlikely(rte_mempool_get(pool, &priv))) {
		plt_dp_err("Could not allocate session private data");
		return -ENOMEM;
	}

	memset(priv, 0, sizeof(struct cnxk_se_sess));

	sess_priv = priv;

	ret = cnxk_sess_fill(xform, sess_priv);
	if (ret)
		goto priv_put;

	if ((sess_priv->roc_se_ctx.fc_type == ROC_SE_HASH_HMAC) &&
	    cpt_mac_len_verify(&xform->auth)) {
		plt_dp_err("MAC length is not supported");
		if (sess_priv->roc_se_ctx.auth_key != NULL) {
			plt_free(sess_priv->roc_se_ctx.auth_key);
			sess_priv->roc_se_ctx.auth_key = NULL;
		}

		ret = -ENOTSUP;
		goto priv_put;
	}

	sess_priv->cpt_inst_w7 = cnxk_cpt_inst_w7_get(sess_priv, roc_cpt);

	set_sym_session_private_data(sess, driver_id, sess_priv);

	return 0;

priv_put:
	rte_mempool_put(pool, priv);

	return ret;
}

int
cnxk_cpt_sym_session_configure(struct rte_cryptodev *dev,
			       struct rte_crypto_sym_xform *xform,
			       struct rte_cryptodev_sym_session *sess,
			       struct rte_mempool *pool)
{
	struct cnxk_cpt_vf *vf = dev->data->dev_private;
	struct roc_cpt *roc_cpt = &vf->cpt;
	uint8_t driver_id;

	driver_id = dev->driver_id;

	return sym_session_configure(roc_cpt, driver_id, xform, sess, pool);
}

void
sym_session_clear(int driver_id, struct rte_cryptodev_sym_session *sess)
{
	void *priv = get_sym_session_private_data(sess, driver_id);
	struct cnxk_se_sess *sess_priv;
	struct rte_mempool *pool;

	if (priv == NULL)
		return;

	sess_priv = priv;

	if (sess_priv->roc_se_ctx.auth_key != NULL)
		plt_free(sess_priv->roc_se_ctx.auth_key);

	memset(priv, 0, cnxk_cpt_sym_session_get_size(NULL));

	pool = rte_mempool_from_obj(priv);

	set_sym_session_private_data(sess, driver_id, NULL);

	rte_mempool_put(pool, priv);
}

void
cnxk_cpt_sym_session_clear(struct rte_cryptodev *dev,
			   struct rte_cryptodev_sym_session *sess)
{
	return sym_session_clear(dev->driver_id, sess);
}

unsigned int
cnxk_ae_session_size_get(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct cnxk_ae_sess);
}

void
cnxk_ae_session_clear(struct rte_cryptodev *dev,
		      struct rte_cryptodev_asym_session *sess)
{
	struct cnxk_ae_sess *priv;

	priv = (struct cnxk_ae_sess *) sess->sess_private_data;
	if (priv == NULL)
		return;

	/* Free resources allocated in session_cfg */
	cnxk_ae_free_session_parameters(priv);

	/* Reset and free object back to pool */
	memset(priv, 0, cnxk_ae_session_size_get(dev));
}

int
cnxk_ae_session_cfg(struct rte_cryptodev *dev,
		    struct rte_crypto_asym_xform *xform,
		    struct rte_cryptodev_asym_session *sess)
{
	struct cnxk_ae_sess *priv =
			(struct cnxk_ae_sess *) sess->sess_private_data;
	struct cnxk_cpt_vf *vf = dev->data->dev_private;
	struct roc_cpt *roc_cpt = &vf->cpt;
	union cpt_inst_w7 w7;
	int ret;

	ret = cnxk_ae_fill_session_parameters(priv, xform);
	if (ret)
		return ret;

	w7.u64 = 0;
	w7.s.egrp = roc_cpt->eng_grp[CPT_ENG_TYPE_AE];
	priv->cpt_inst_w7 = w7.u64;
	priv->cnxk_fpm_iova = vf->cnxk_fpm_iova;
	priv->ec_grp = vf->ec_grp;

	return 0;
}

void
cnxk_cpt_dump_on_err(struct cnxk_cpt_qp *qp)
{
	struct pending_queue *pend_q = &qp->pend_q;
	uint64_t inflight, enq_ptr, deq_ptr, insts;
	union cpt_lf_q_inst_ptr inst_ptr;
	union cpt_lf_inprog lf_inprog;

	plt_print("Lcore ID: %d, LF/QP ID: %d", rte_lcore_id(), qp->lf.lf_id);
	plt_print("");
	plt_print("S/w pending queue:");
	plt_print("\tHead: %"PRIu64"", pend_q->head);
	plt_print("\tTail: %"PRIu64"", pend_q->tail);
	plt_print("\tMask: 0x%"PRIx64"", pend_q->pq_mask);
	plt_print("\tInflight count: %"PRIu64"",
		  pending_queue_infl_cnt(pend_q->head, pend_q->tail,
					 pend_q->pq_mask));

	plt_print("");
	plt_print("H/w pending queue:");

	lf_inprog.u = plt_read64(qp->lf.rbase + CPT_LF_INPROG);
	inflight = lf_inprog.s.inflight;
	plt_print("\tInflight in engines: %"PRIu64"", inflight);

	inst_ptr.u = plt_read64(qp->lf.rbase + CPT_LF_Q_INST_PTR);

	enq_ptr = inst_ptr.s.nq_ptr;
	deq_ptr = inst_ptr.s.dq_ptr;

	if (enq_ptr >= deq_ptr)
		insts = enq_ptr - deq_ptr;
	else
		insts = (enq_ptr + pend_q->pq_mask + 1 + 320 + 40) - deq_ptr;

	plt_print("\tNQ ptr: 0x%"PRIx64"", enq_ptr);
	plt_print("\tDQ ptr: 0x%"PRIx64"", deq_ptr);
	plt_print("Insts waiting in CPT: %"PRIu64"", insts);

	plt_print("");
	roc_cpt_afs_print(qp->lf.roc_cpt);
}
