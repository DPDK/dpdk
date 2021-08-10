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

	for (qp_id = 0; qp_id < nb_lf; qp_id++)
		roc_cpt_iq_enable(roc_cpt->lf[qp_id]);

	return 0;
}

void
cnxk_cpt_dev_stop(struct rte_cryptodev *dev)
{
	struct cnxk_cpt_vf *vf = dev->data->dev_private;
	struct roc_cpt *roc_cpt = &vf->cpt;
	uint16_t nb_lf = roc_cpt->nb_lf;
	uint16_t qp_id;

	for (qp_id = 0; qp_id < nb_lf; qp_id++)
		roc_cpt_iq_disable(roc_cpt->lf[qp_id]);
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

	info->max_nb_queue_pairs = roc_cpt->nb_lf_avail;
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
	struct rte_mempool *pool;
	uint32_t cache_sz;
	int mlen = 8;

	if (dev->feature_flags & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO) {
		/* Get meta len */
		mlen = cnxk_cpt_get_mlen();
	}

	if (dev->feature_flags & RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO) {

		/* Get meta len required for asymmetric operations */
		mlen = RTE_MAX(mlen, cnxk_cpt_asym_get_mlen());
	}

	cache_sz = RTE_MIN(RTE_MEMPOOL_CACHE_MAX_SIZE, nb_elements / 1.5);

	/* Allocate mempool */

	snprintf(mempool_name, RTE_MEMPOOL_NAMESIZE, "cnxk_cpt_mb_%u:%u",
		 dev->data->dev_id, qp_id);

	pool = rte_mempool_create(mempool_name, nb_elements, mlen, cache_sz, 0,
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
	qp->pend_q.enq_tail = 0;
	qp->pend_q.deq_head = 0;
	qp->pend_q.pending_count = 0;

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
	int ret;

	if (dev->data->queue_pairs[qp_id] != NULL)
		cnxk_cpt_queue_pair_release(dev, qp_id);

	pci_dev = RTE_DEV_TO_PCI(dev->device);

	if (pci_dev->mem_resource[2].addr == NULL) {
		plt_err("Invalid PCI mem address");
		return -EIO;
	}

	qp = cnxk_cpt_qp_create(dev, qp_id, conf->nb_descriptors);
	if (qp == NULL) {
		plt_err("Could not create queue pair %d", qp_id);
		return -ENOMEM;
	}

	qp->lf.lf_id = qp_id;
	qp->lf.nb_desc = conf->nb_descriptors;

	ret = roc_cpt_lf_init(roc_cpt, &qp->lf);
	if (ret < 0) {
		plt_err("Could not initialize queue pair %d", qp_id);
		ret = -EINVAL;
		goto exit;
	}

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
sym_xform_verify(struct rte_crypto_sym_xform *xform)
{
	if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
	    xform->auth.algo == RTE_CRYPTO_AUTH_NULL &&
	    xform->auth.op == RTE_CRYPTO_AUTH_OP_VERIFY)
		return -ENOTSUP;

	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER && xform->next == NULL)
		return CNXK_CPT_CIPHER;

	if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH && xform->next == NULL)
		return CNXK_CPT_AUTH;

	if (xform->type == RTE_CRYPTO_SYM_XFORM_AEAD && xform->next == NULL)
		return CNXK_CPT_AEAD;

	if (xform->next == NULL)
		return -EIO;

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

	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
	    xform->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT &&
	    xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
	    xform->next->auth.op == RTE_CRYPTO_AUTH_OP_GENERATE)
		return CNXK_CPT_CIPHER_ENC_AUTH_GEN;

	if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
	    xform->auth.op == RTE_CRYPTO_AUTH_OP_VERIFY &&
	    xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
	    xform->next->cipher.op == RTE_CRYPTO_CIPHER_OP_DECRYPT)
		return CNXK_CPT_AUTH_VRFY_CIPHER_DEC;

	if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
	    xform->auth.op == RTE_CRYPTO_AUTH_OP_GENERATE &&
	    xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
	    xform->next->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
		switch (xform->auth.algo) {
		case RTE_CRYPTO_AUTH_SHA1_HMAC:
			switch (xform->next->cipher.algo) {
			case RTE_CRYPTO_CIPHER_AES_CBC:
				return CNXK_CPT_AUTH_GEN_CIPHER_ENC;
			default:
				return -ENOTSUP;
			}
		default:
			return -ENOTSUP;
		}
	}

	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
	    xform->cipher.op == RTE_CRYPTO_CIPHER_OP_DECRYPT &&
	    xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
	    xform->next->auth.op == RTE_CRYPTO_AUTH_OP_VERIFY) {
		switch (xform->cipher.algo) {
		case RTE_CRYPTO_CIPHER_AES_CBC:
			switch (xform->next->auth.algo) {
			case RTE_CRYPTO_AUTH_SHA1_HMAC:
				return CNXK_CPT_CIPHER_DEC_AUTH_VRFY;
			default:
				return -ENOTSUP;
			}
		default:
			return -ENOTSUP;
		}
	}

	return -ENOTSUP;
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

	ret = sym_xform_verify(xform);
	if (unlikely(ret < 0))
		return ret;

	if (unlikely(rte_mempool_get(pool, &priv))) {
		plt_dp_err("Could not allocate session private data");
		return -ENOMEM;
	}

	memset(priv, 0, sizeof(struct cnxk_se_sess));

	sess_priv = priv;

	switch (ret) {
	case CNXK_CPT_CIPHER:
		ret = fill_sess_cipher(xform, sess_priv);
		break;
	case CNXK_CPT_AUTH:
		if (xform->auth.algo == RTE_CRYPTO_AUTH_AES_GMAC)
			ret = fill_sess_gmac(xform, sess_priv);
		else
			ret = fill_sess_auth(xform, sess_priv);
		break;
	case CNXK_CPT_AEAD:
		ret = fill_sess_aead(xform, sess_priv);
		break;
	case CNXK_CPT_CIPHER_ENC_AUTH_GEN:
	case CNXK_CPT_CIPHER_DEC_AUTH_VRFY:
		ret = fill_sess_cipher(xform, sess_priv);
		if (ret < 0)
			break;
		ret = fill_sess_auth(xform->next, sess_priv);
		break;
	case CNXK_CPT_AUTH_VRFY_CIPHER_DEC:
	case CNXK_CPT_AUTH_GEN_CIPHER_ENC:
		ret = fill_sess_auth(xform, sess_priv);
		if (ret < 0)
			break;
		ret = fill_sess_cipher(xform->next, sess_priv);
		break;
	default:
		ret = -1;
	}

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

	return -ENOTSUP;
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
	struct rte_mempool *sess_mp;
	struct cnxk_ae_sess *priv;

	priv = get_asym_session_private_data(sess, dev->driver_id);
	if (priv == NULL)
		return;

	/* Free resources allocated in session_cfg */
	cnxk_ae_free_session_parameters(priv);

	/* Reset and free object back to pool */
	memset(priv, 0, cnxk_ae_session_size_get(dev));
	sess_mp = rte_mempool_from_obj(priv);
	set_asym_session_private_data(sess, dev->driver_id, NULL);
	rte_mempool_put(sess_mp, priv);
}

int
cnxk_ae_session_cfg(struct rte_cryptodev *dev,
		    struct rte_crypto_asym_xform *xform,
		    struct rte_cryptodev_asym_session *sess,
		    struct rte_mempool *pool)
{
	struct cnxk_cpt_vf *vf = dev->data->dev_private;
	struct roc_cpt *roc_cpt = &vf->cpt;
	struct cnxk_ae_sess *priv;
	union cpt_inst_w7 w7;
	int ret;

	if (rte_mempool_get(pool, (void **)&priv))
		return -ENOMEM;

	memset(priv, 0, sizeof(struct cnxk_ae_sess));

	ret = cnxk_ae_fill_session_parameters(priv, xform);
	if (ret) {
		rte_mempool_put(pool, priv);
		return ret;
	}

	w7.u64 = 0;
	w7.s.egrp = roc_cpt->eng_grp[CPT_ENG_TYPE_AE];
	priv->cpt_inst_w7 = w7.u64;
	priv->cnxk_fpm_iova = vf->cnxk_fpm_iova;
	priv->ec_grp = vf->ec_grp;
	set_asym_session_private_data(sess, dev->driver_id, priv);

	return 0;
}
