/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <eal_export.h>
#include <rte_atomic.h>
#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include <rte_errno.h>
#ifdef CPT_INST_DEBUG_ENABLE
#include <rte_hexdump.h>
#endif
#include <rte_security_driver.h>

#include "roc_ae.h"
#include "roc_ae_fpm_tables.h"
#include "roc_cpt.h"
#include "roc_errata.h"
#include "roc_idev.h"
#include "roc_ie_on.h"
#if defined(__aarch64__)
#include "roc_io.h"
#else
#include "roc_io_generic.h"
#endif

#include "cnxk_ae.h"
#include "cnxk_cryptodev.h"
#include "cnxk_cryptodev_capabilities.h"
#include "cnxk_cryptodev_ops.h"
#include "cnxk_se.h"

#include "cn10k_cryptodev_ops.h"
#include "cn10k_cryptodev_sec.h"
#include "cn20k_cryptodev_ops.h"
#include "cn20k_cryptodev_sec.h"
#include "cn9k_cryptodev_ops.h"
#include "cn9k_ipsec.h"

#include "rte_pmd_cnxk_crypto.h"

#define CNXK_CPT_MAX_ASYM_OP_NUM_PARAMS	 5
#define CNXK_CPT_MAX_ASYM_OP_MOD_LEN	 1024
#define CNXK_CPT_META_BUF_MAX_CACHE_SIZE 128

static_assert((uint16_t)RTE_PMD_CNXK_AE_EC_ID_P192 == (uint16_t)ROC_AE_EC_ID_P192,
	      "Enum value mismatch");
static_assert((uint16_t)RTE_PMD_CNXK_AE_EC_ID_P224 == (uint16_t)ROC_AE_EC_ID_P224,
	      "Enum value mismatch");
static_assert((uint16_t)RTE_PMD_CNXK_AE_EC_ID_P256 == (uint16_t)ROC_AE_EC_ID_P256,
	      "Enum value mismatch");
static_assert((uint16_t)RTE_PMD_CNXK_AE_EC_ID_P384 == (uint16_t)ROC_AE_EC_ID_P384,
	      "Enum value mismatch");
static_assert((uint16_t)RTE_PMD_CNXK_AE_EC_ID_P521 == (uint16_t)ROC_AE_EC_ID_P521,
	      "Enum value mismatch");
static_assert((uint16_t)RTE_PMD_CNXK_AE_EC_ID_P160 == (uint16_t)ROC_AE_EC_ID_P160,
	      "Enum value mismatch");
static_assert((uint16_t)RTE_PMD_CNXK_AE_EC_ID_P320 == (uint16_t)ROC_AE_EC_ID_P320,
	      "Enum value mismatch");
static_assert((uint16_t)RTE_PMD_CNXK_AE_EC_ID_P512 == (uint16_t)ROC_AE_EC_ID_P512,
	      "Enum value mismatch");
static_assert((uint16_t)RTE_PMD_CNXK_AE_EC_ID_SM2 == (uint16_t)ROC_AE_EC_ID_SM2,
	      "Enum value mismatch");
static_assert((uint16_t)RTE_PMD_CNXK_AE_EC_ID_ED25519 == (uint16_t)ROC_AE_EC_ID_ED25519,
	      "Enum value mismatch");
static_assert((uint16_t)RTE_PMD_CNXK_AE_EC_ID_ED448 == (uint16_t)ROC_AE_EC_ID_ED448,
	      "Enum value mismatch");

static int
cnxk_cpt_get_mlen(void)
{
	uint32_t len;

	/* For MAC */
	len = 2 * sizeof(uint64_t);
	len += ROC_SE_MAX_MAC_LEN * sizeof(uint8_t);

	/* For PDCP_CHAIN passthrough alignment */
	len += 8;
	len += ROC_SE_OFF_CTRL_LEN + ROC_CPT_AES_CBC_IV_LEN;
	len += RTE_ALIGN_CEIL((ROC_SG_LIST_HDR_SIZE +
			       (RTE_ALIGN_CEIL(ROC_MAX_SG_IN_OUT_CNT, 4) >> 2) * ROC_SG_ENTRY_SIZE),
			      8);

	return len;
}

static int
cnxk_cpt_sec_get_mlen(void)
{
	uint32_t len;

	len = ROC_IE_ON_OUTB_DPTR_HDR + ROC_IE_ON_MAX_IV_LEN;
	len += RTE_ALIGN_CEIL((ROC_SG_LIST_HDR_SIZE +
			       (RTE_ALIGN_CEIL(ROC_MAX_SG_IN_OUT_CNT, 4) >> 2) * ROC_SG_ENTRY_SIZE),
			      8);

	return len;
}

int
cnxk_cpt_asym_get_mlen(void)
{
	uint32_t len;

	/* To hold RPTR */
	len = sizeof(uint64_t);

	/* Get meta len for asymmetric operations */
	len += CNXK_CPT_MAX_ASYM_OP_NUM_PARAMS * CNXK_CPT_MAX_ASYM_OP_MOD_LEN;

	return len;
}

static int
cnxk_cpt_dev_clear(struct rte_cryptodev *dev)
{
	struct cnxk_cpt_vf *vf = dev->data->dev_private;
	int ret;

	if (dev->feature_flags & RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO) {
		roc_ae_fpm_put();
		roc_ae_ec_grp_put();
	}

	ret = roc_cpt_int_misc_cb_unregister(cnxk_cpt_int_misc_cb, NULL);
	if (ret < 0) {
		plt_err("Could not unregister CPT_MISC_INT cb");
		return ret;
	}

	roc_cpt_dev_clear(&vf->cpt);

	return 0;
}

int
cnxk_cpt_dev_config(struct rte_cryptodev *dev, struct rte_cryptodev_config *conf)
{
	struct cnxk_cpt_vf *vf = dev->data->dev_private;
	struct roc_cpt *roc_cpt = &vf->cpt;
	uint16_t nb_lf_avail, nb_lf;
	bool rxc_ena = false;
	int ret;

	/* If this is a reconfigure attempt, clear the device and configure again */
	if (roc_cpt->nb_lf > 0) {
		cnxk_cpt_dev_clear(dev);
		roc_cpt->opaque = NULL;
	}

	dev->feature_flags = cnxk_cpt_default_ff_get() & ~conf->ff_disable;

	nb_lf_avail = roc_cpt->nb_lf_avail;
	nb_lf = conf->nb_queue_pairs;

	if (nb_lf > nb_lf_avail)
		return -ENOTSUP;

	if (dev->feature_flags & RTE_CRYPTODEV_FF_SECURITY_RX_INJECT) {
		if (rte_security_dynfield_register() < 0)
			return -ENOTSUP;
		rxc_ena = true;
		vf->rx_chan_base = roc_idev_nix_rx_chan_base_get();
	}

	ret = roc_cpt_dev_configure(roc_cpt, nb_lf, rxc_ena, vf->rx_inject_qp);
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
	roc_cpt->opaque = dev;
	/* Register callback to handle CPT_MISC_INT */
	roc_cpt_int_misc_cb_register(cnxk_cpt_int_misc_cb, NULL);

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
	uint16_t i;
	int ret;

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		ret = cnxk_cpt_queue_pair_release(dev, i);
		if (ret < 0) {
			plt_err("Could not release queue pair %u", i);
			return ret;
		}
	}

	return cnxk_cpt_dev_clear(dev);
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

	/* If the LF ID for RX Inject is less than the available lfs. */
	if (vf->rx_inject_qp > info->max_nb_queue_pairs)
		info->feature_flags &= ~RTE_CRYPTODEV_FF_SECURITY_RX_INJECT;
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

	if (dev->feature_flags & RTE_CRYPTODEV_FF_SECURITY) {
		/* Get meta len for security operations */
		mlen = cnxk_cpt_sec_get_mlen();
	}

	if (dev->feature_flags & RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO) {

		/* Get meta len required for asymmetric operations */
		mlen = RTE_MAX(mlen, cnxk_cpt_asym_get_mlen());
	}

	mb_pool_sz = nb_elements;
	cache_sz = RTE_MIN(CNXK_CPT_META_BUF_MAX_CACHE_SIZE, nb_elements / 1.5);

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

	ret = roc_cpt_lmtline_init(roc_cpt, &qp->lmtline, qp_id, true);
	if (ret < 0) {
		roc_cpt->lf[qp_id] = NULL;
		plt_err("Could not init lmtline for queue pair %d", qp_id);
		goto exit;
	}

	qp->sess_mp = conf->mp_session;
	dev->data->queue_pairs[qp_id] = qp;

	if (qp_id == vf->rx_inject_qp) {
		ret = roc_cpt_lmtline_init(roc_cpt, &vf->rx_inj_lmtline, vf->rx_inject_qp, true);
		if (ret) {
			plt_err("Could not init lmtline Rx inject");
			goto exit;
		}

		vf->rx_inj_sso_pf_func = roc_idev_nix_inl_dev_pffunc_get();

		/* Block the queue for other submissions */
		qp->pend_q.pq_mask = 0;
	}

	return 0;

exit:
	cnxk_cpt_qp_destroy(dev, qp);
	return ret;
}

int
cnxk_cpt_queue_pair_reset(struct rte_cryptodev *dev, uint16_t qp_id,
			  const struct rte_cryptodev_qp_conf *conf, int socket_id)
{
	if (conf == NULL) {
		struct cnxk_cpt_vf *vf = dev->data->dev_private;
		struct roc_cpt_lf *lf;

		if (vf == NULL)
			return -ENOTSUP;

		lf = vf->cpt.lf[qp_id];
		roc_cpt_lf_reset(lf);
		roc_cpt_iq_enable(lf);

		return 0;
	}

	return cnxk_cpt_queue_pair_setup(dev, qp_id, conf, socket_id);
}

uint32_t
cnxk_cpt_qp_depth_used(void *qptr)
{
	struct cnxk_cpt_qp *qp = qptr;
	struct pending_queue *pend_q;
	union cpt_fc_write_s fc;

	pend_q = &qp->pend_q;

	fc.u64[0] = rte_atomic_load_explicit((RTE_ATOMIC(uint64_t)*)(qp->lmtline.fc_addr),
			rte_memory_order_relaxed);

	return RTE_MAX(pending_queue_infl_cnt(pend_q->head, pend_q->tail, pend_q->pq_mask),
		       fc.s.qsize);
}

unsigned int
cnxk_cpt_sym_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct cnxk_se_sess);
}

static bool
is_valid_pdcp_cipher_alg(struct rte_crypto_sym_xform *c_xfrm,
			 struct cnxk_se_sess *sess)
{
	switch (c_xfrm->cipher.algo) {
	case RTE_CRYPTO_CIPHER_SNOW3G_UEA2:
	case RTE_CRYPTO_CIPHER_ZUC_EEA3:
		break;
	case RTE_CRYPTO_CIPHER_AES_CTR:
		sess->aes_ctr_eea2 = 1;
		break;
	default:
		return false;
	}

	return true;
}

static int
cnxk_sess_fill(struct roc_cpt *roc_cpt, struct rte_crypto_sym_xform *xform,
	       struct cnxk_se_sess *sess)
{
	struct rte_crypto_sym_xform *aead_xfrm = NULL;
	struct rte_crypto_sym_xform *c_xfrm = NULL;
	struct rte_crypto_sym_xform *a_xfrm = NULL;
	bool ciph_then_auth = false;

	if (roc_cpt->hw_caps[CPT_ENG_TYPE_SE].pdcp_chain_zuc256)
		sess->roc_se_ctx.pdcp_iv_offset = 24;
	else
		sess->roc_se_ctx.pdcp_iv_offset = 16;

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

	if ((aead_xfrm == NULL) &&
	    (c_xfrm == NULL || c_xfrm->cipher.algo == RTE_CRYPTO_CIPHER_NULL) &&
	    (a_xfrm == NULL || a_xfrm->auth.algo == RTE_CRYPTO_AUTH_NULL))
		sess->passthrough = 1;

	/* Cipher only */
	if (c_xfrm != NULL && (a_xfrm == NULL || a_xfrm->auth.algo == RTE_CRYPTO_AUTH_NULL)) {
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

	if (c_xfrm->cipher.algo == RTE_CRYPTO_CIPHER_AES_XTS) {
		plt_err("AES XTS with auth algorithm is not supported");
		return -ENOTSUP;
	}

	if (c_xfrm->cipher.algo == RTE_CRYPTO_CIPHER_3DES_CBC &&
	    a_xfrm->auth.algo == RTE_CRYPTO_AUTH_SHA1) {
		plt_dp_err("3DES-CBC + SHA1 is not supported");
		return -ENOTSUP;
	}

	/* Cipher then auth */
	if (ciph_then_auth) {
		if (c_xfrm->cipher.op == RTE_CRYPTO_CIPHER_OP_DECRYPT) {
			if (a_xfrm->auth.op != RTE_CRYPTO_AUTH_OP_VERIFY)
				return -EINVAL;
			sess->auth_first = 1;
			switch (a_xfrm->auth.algo) {
			case RTE_CRYPTO_AUTH_SHA1_HMAC:
				switch (c_xfrm->cipher.algo) {
				case RTE_CRYPTO_CIPHER_AES_CBC:
					break;
				default:
					return -ENOTSUP;
				}
				break;
			case RTE_CRYPTO_AUTH_SNOW3G_UIA2:
			case RTE_CRYPTO_AUTH_ZUC_EIA3:
			case RTE_CRYPTO_AUTH_AES_CMAC:
				if (!is_valid_pdcp_cipher_alg(c_xfrm, sess))
					return -ENOTSUP;
				break;
			default:
				return -ENOTSUP;
			}
		}
		sess->roc_se_ctx.ciph_then_auth = 1;
		sess->chained_op = 1;
		if (fill_sess_cipher(c_xfrm, sess))
			return -ENOTSUP;
		if (fill_sess_auth(a_xfrm, sess))
			return -ENOTSUP;
		else
			return 0;
	}

	/* else */

	if (c_xfrm->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
		if (a_xfrm->auth.op != RTE_CRYPTO_AUTH_OP_GENERATE)
			return -EINVAL;
		sess->auth_first = 1;
		switch (a_xfrm->auth.algo) {
		case RTE_CRYPTO_AUTH_SHA1_HMAC:
			switch (c_xfrm->cipher.algo) {
			case RTE_CRYPTO_CIPHER_AES_CBC:
				break;
			default:
				return -ENOTSUP;
			}
			break;
		case RTE_CRYPTO_AUTH_SNOW3G_UIA2:
		case RTE_CRYPTO_AUTH_ZUC_EIA3:
		case RTE_CRYPTO_AUTH_AES_CMAC:
			if (!is_valid_pdcp_cipher_alg(c_xfrm, sess))
				return -ENOTSUP;
			break;
		default:
			return -ENOTSUP;
		}
	}

	sess->roc_se_ctx.auth_then_ciph = 1;
	sess->chained_op = 1;
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

	if (hw_ctx_cache_enable())
		inst_w7.s.ctx_val = 1;
	else
		inst_w7.s.cptr += 8;

	/* Set the engine group */
	if (roc_model_is_cn20k())
		inst_w7.s.egrp = roc_cpt->eng_grp[CPT_ENG_TYPE_SE];
	else if (sess->zsk_flag || sess->aes_ctr_eea2 || sess->is_sha3 || sess->is_sm3 ||
		 sess->passthrough || sess->is_sm4)
		inst_w7.s.egrp = roc_cpt->eng_grp[CPT_ENG_TYPE_SE];
	else
		inst_w7.s.egrp = roc_cpt->eng_grp[CPT_ENG_TYPE_IE];

	return inst_w7.u64;
}

int
sym_session_configure(struct roc_cpt *roc_cpt, struct rte_crypto_sym_xform *xform,
		      struct rte_cryptodev_sym_session *sess, bool is_session_less)
{
	enum cpt_dp_thread_type thr_type;
	struct cnxk_se_sess *sess_priv = (struct cnxk_se_sess *)sess;
	int ret;

	if (is_session_less)
		memset(sess_priv, 0, sizeof(struct cnxk_se_sess));

	ret = cnxk_sess_fill(roc_cpt, xform, sess_priv);
	if (ret)
		goto priv_put;

	sess_priv->lf = roc_cpt->lf[0];

	if (sess_priv->passthrough)
		thr_type = CPT_DP_THREAD_TYPE_PT;
	else if (sess_priv->cpt_op & ROC_SE_OP_CIPHER_MASK) {
		switch (sess_priv->roc_se_ctx.fc_type) {
		case ROC_SE_FC_GEN:
			if (sess_priv->aes_gcm || sess_priv->aes_ccm || sess_priv->chacha_poly)
				thr_type = CPT_DP_THREAD_TYPE_FC_AEAD;
			else
				thr_type = CPT_DP_THREAD_TYPE_FC_CHAIN;
			break;
		case ROC_SE_PDCP:
			thr_type = CPT_DP_THREAD_TYPE_PDCP;
			break;
		case ROC_SE_KASUMI:
			thr_type = CPT_DP_THREAD_TYPE_KASUMI;
			break;
		case ROC_SE_PDCP_CHAIN:
			thr_type = CPT_DP_THREAD_TYPE_PDCP_CHAIN;
			break;
		case ROC_SE_SM:
			thr_type = CPT_DP_THREAD_TYPE_SM;
			break;
		default:
			plt_err("Invalid op type");
			ret = -ENOTSUP;
			goto priv_put;
		}
	} else {
		thr_type = CPT_DP_THREAD_AUTH_ONLY;
	}

	sess_priv->dp_thr_type = thr_type;

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

	if (hw_ctx_cache_enable())
		roc_se_ctx_init(&sess_priv->roc_se_ctx);

	if (sess_priv->roc_se_ctx.se_ctx.w0.s.ctx_size < roc_cpt->ctx_ilen)
		sess_priv->roc_se_ctx.se_ctx.w0.s.ctx_size = roc_cpt->ctx_ilen;

	return 0;

priv_put:
	return ret;
}

int
cnxk_cpt_sym_session_configure(struct rte_cryptodev *dev,
			       struct rte_crypto_sym_xform *xform,
			       struct rte_cryptodev_sym_session *sess)
{
	struct cnxk_cpt_vf *vf = dev->data->dev_private;
	struct roc_cpt *roc_cpt = &vf->cpt;

	return sym_session_configure(roc_cpt, xform, sess, false);
}

void
sym_session_clear(struct rte_cryptodev_sym_session *sess, bool is_session_less)
{
	struct cnxk_se_sess *sess_priv = (struct cnxk_se_sess *)sess;

	/* Trigger CTX flush + invalidate to remove from CTX_CACHE */
	if (hw_ctx_cache_enable())
		roc_cpt_lf_ctx_flush(sess_priv->lf, &sess_priv->roc_se_ctx.se_ctx, true);

	if (sess_priv->roc_se_ctx.auth_key != NULL)
		plt_free(sess_priv->roc_se_ctx.auth_key);

	if (is_session_less)
		memset(sess_priv, 0, cnxk_cpt_sym_session_get_size(NULL));
}

void
cnxk_cpt_sym_session_clear(struct rte_cryptodev *dev __rte_unused,
			   struct rte_cryptodev_sym_session *sess)
{
	return sym_session_clear(sess, false);
}

unsigned int
cnxk_ae_session_size_get(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct cnxk_ae_sess);
}

void
cnxk_ae_session_clear(struct rte_cryptodev *dev __rte_unused,
		      struct rte_cryptodev_asym_session *sess)
{
	struct cnxk_ae_sess *priv = (struct cnxk_ae_sess *)sess;

	/* Trigger CTX flush + invalidate to remove from CTX_CACHE */
	if (roc_errata_cpt_hang_on_mixed_ctx_val())
		roc_cpt_lf_ctx_flush(priv->lf, &priv->hw_ctx, true);

	/* Free resources allocated in session_cfg */
	cnxk_ae_free_session_parameters(priv);

	/* Reset and free object back to pool */
	memset(priv, 0, sizeof(struct cnxk_ae_sess));
}

int
cnxk_ae_session_cfg(struct rte_cryptodev *dev, struct rte_crypto_asym_xform *xform,
		    struct rte_cryptodev_asym_session *sess)
{
	struct cnxk_ae_sess *priv = (struct cnxk_ae_sess *)sess;
	struct cnxk_cpt_vf *vf = dev->data->dev_private;
	struct roc_cpt *roc_cpt = &vf->cpt;
	union cpt_inst_w7 w7;
	struct hw_ctx_s *hwc;
	int ret;

	ret = cnxk_ae_fill_session_parameters(priv, xform);
	if (ret)
		return ret;

	priv->lf = roc_cpt->lf[0];

	w7.u64 = 0;
	w7.s.egrp = roc_cpt->eng_grp[CPT_ENG_TYPE_AE];

	if (roc_errata_cpt_hang_on_mixed_ctx_val()) {
		hwc = &priv->hw_ctx;
		hwc->w0.s.aop_valid = 1;
		hwc->w0.s.ctx_hdr_size = 0;
		hwc->w0.s.ctx_size = 1;
		hwc->w0.s.ctx_push_size = 1;

		w7.s.cptr = (uint64_t)hwc;
		w7.s.ctx_val = 1;
	}

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
	roc_cpt_lfs_print(qp->lf.roc_cpt);
}

int
cnxk_cpt_queue_pair_event_error_query(struct rte_cryptodev *dev, uint16_t qp_id)
{
	struct cnxk_cpt_vf *vf = dev->data->dev_private;
	struct roc_cpt *roc_cpt = &vf->cpt;
	struct roc_cpt_lf *lf;

	lf = roc_cpt->lf[qp_id];
	if (lf && lf->error_event_pending) {
		lf->error_event_pending = 0;
		return 1;
	}
	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_cnxk_crypto_qptr_get, 24.03)
struct rte_pmd_cnxk_crypto_qptr *
rte_pmd_cnxk_crypto_qptr_get(uint8_t dev_id, uint16_t qp_id)
{
	const struct rte_crypto_fp_ops *fp_ops;
	void *qptr;

	fp_ops = &rte_crypto_fp_ops[dev_id];
	qptr = fp_ops->qp.data[qp_id];

	return qptr;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_cnxk_ae_fpm_table_get, 25.07)
const uint64_t *
rte_pmd_cnxk_ae_fpm_table_get(uint8_t dev_id)
{
	struct rte_cryptodev *dev;
	struct cnxk_cpt_vf *vf;

	dev = rte_cryptodev_pmd_get_dev(dev_id);
	if (dev == NULL) {
		plt_err("Invalid dev_id %u", dev_id);
		return NULL;
	}

	vf = dev->data->dev_private;
	if (vf == NULL) {
		plt_err("VF is not initialized");
		return NULL;
	}

	return vf->cnxk_fpm_iova;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_cnxk_ae_ec_grp_table_get, 25.07)
const struct rte_pmd_cnxk_crypto_ae_ec_group_params **
rte_pmd_cnxk_ae_ec_grp_table_get(uint8_t dev_id, uint16_t *nb_max_entries)
{
	struct rte_cryptodev *dev;
	struct cnxk_cpt_vf *vf;

	dev = rte_cryptodev_pmd_get_dev(dev_id);
	if (dev == NULL) {
		plt_err("Invalid dev_id %u", dev_id);
		return NULL;
	}

	vf = dev->data->dev_private;
	if (vf == NULL) {
		plt_err("VF is not initialized");
		return NULL;
	}

	*nb_max_entries = ROC_AE_EC_ID_PMAX;

	return (const struct rte_pmd_cnxk_crypto_ae_ec_group_params **)(void *)vf->ec_grp;
}

static inline void
cnxk_crypto_cn10k_submit(struct rte_pmd_cnxk_crypto_qptr *qptr, void *inst, uint16_t nb_inst)
{
	struct cnxk_cpt_qp *qp = PLT_PTR_CAST(qptr);
	uint64_t lmt_base, io_addr;
	uint16_t lmt_id;
	void *lmt_dst;
	int i;

	lmt_base = qp->lmtline.lmt_base;
	io_addr = qp->lmtline.io_addr;

	ROC_LMT_BASE_ID_GET(lmt_base, lmt_id);

	lmt_dst = PLT_PTR_CAST(lmt_base);
again:
	i = RTE_MIN(nb_inst, CN10K_CPT_PKTS_PER_LOOP);

	memcpy(lmt_dst, inst, i * sizeof(struct cpt_inst_s));

	cn10k_cpt_lmtst_dual_submit(&io_addr, lmt_id, &i);

	if (nb_inst - i > 0) {
		nb_inst -= CN10K_CPT_PKTS_PER_LOOP;
		inst = RTE_PTR_ADD(inst, CN10K_CPT_PKTS_PER_LOOP * sizeof(struct cpt_inst_s));
		goto again;
	}
}

static inline void
cnxk_crypto_cn9k_submit(struct rte_pmd_cnxk_crypto_qptr *qptr, void *inst, uint16_t nb_inst)
{
	struct cnxk_cpt_qp *qp = PLT_PTR_CAST(qptr);

	const uint64_t lmt_base = qp->lf.lmt_base;
	const uint64_t io_addr = qp->lf.io_addr;

	if (unlikely(nb_inst & 1)) {
		cn9k_cpt_inst_submit(inst, lmt_base, io_addr);
		inst = RTE_PTR_ADD(inst, sizeof(struct cpt_inst_s));
		nb_inst -= 1;
	}

	while (nb_inst > 0) {
		cn9k_cpt_inst_submit_dual(inst, lmt_base, io_addr);
		inst = RTE_PTR_ADD(inst, 2 * sizeof(struct cpt_inst_s));
		nb_inst -= 2;
	}
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_cnxk_crypto_submit, 24.03)
void
rte_pmd_cnxk_crypto_submit(struct rte_pmd_cnxk_crypto_qptr *qptr, void *inst, uint16_t nb_inst)
{
	if (roc_model_is_cn10k() || roc_model_is_cn20k())
		return cnxk_crypto_cn10k_submit(qptr, inst, nb_inst);
	else if (roc_model_is_cn9k())
		return cnxk_crypto_cn9k_submit(qptr, inst, nb_inst);

	plt_err("Invalid cnxk model");
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_cnxk_crypto_cptr_flush, 24.07)
int
rte_pmd_cnxk_crypto_cptr_flush(struct rte_pmd_cnxk_crypto_qptr *qptr,
			       struct rte_pmd_cnxk_crypto_cptr *cptr, bool invalidate)
{
	struct cnxk_cpt_qp *qp = PLT_PTR_CAST(qptr);

	if (unlikely(qptr == NULL)) {
		plt_err("Invalid queue pair pointer");
		return -EINVAL;
	}

	if (unlikely(cptr == NULL)) {
		plt_err("Invalid CPTR pointer");
		return -EINVAL;
	}

	if (unlikely(roc_model_is_cn9k())) {
		plt_err("Invalid cnxk model");
		return -EINVAL;
	}

	return roc_cpt_lf_ctx_flush(&qp->lf, cptr, invalidate);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_cnxk_crypto_cptr_get, 24.07)
struct rte_pmd_cnxk_crypto_cptr *
rte_pmd_cnxk_crypto_cptr_get(struct rte_pmd_cnxk_crypto_sess *rte_sess)
{
	if (rte_sess == NULL) {
		plt_err("Invalid session pointer");
		return NULL;
	}

	if (rte_sess->sec_sess == NULL) {
		plt_err("Invalid RTE session pointer");
		return NULL;
	}

	if (rte_sess->op_type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC) {
		struct cnxk_ae_sess *ae_sess = PLT_PTR_CAST(rte_sess->crypto_asym_sess);
		return PLT_PTR_CAST(&ae_sess->hw_ctx);
	}

	if (rte_sess->op_type != RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
		plt_err("Invalid crypto operation type");
		return NULL;
	}

	if (rte_sess->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
		struct cnxk_se_sess *se_sess = PLT_PTR_CAST(rte_sess->crypto_sym_sess);
		return PLT_PTR_CAST(&se_sess->roc_se_ctx.se_ctx);
	}

	if (rte_sess->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION) {
		if (roc_model_is_cn20k()) {
			struct cn20k_sec_session *sec_sess = PLT_PTR_CAST(rte_sess->sec_sess);

			return PLT_PTR_CAST(&sec_sess->sa);
		}

		if (roc_model_is_cn10k()) {
			struct cn10k_sec_session *sec_sess = PLT_PTR_CAST(rte_sess->sec_sess);
			return PLT_PTR_CAST(&sec_sess->sa);
		}

		if (roc_model_is_cn9k()) {
			struct cn9k_sec_session *sec_sess = PLT_PTR_CAST(rte_sess->sec_sess);
			return PLT_PTR_CAST(&sec_sess->sa);
		}

		plt_err("Invalid cnxk model");
		return NULL;
	}

	plt_err("Invalid session type");
	return NULL;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_cnxk_crypto_cptr_read, 24.07)
int
rte_pmd_cnxk_crypto_cptr_read(struct rte_pmd_cnxk_crypto_qptr *qptr,
			      struct rte_pmd_cnxk_crypto_cptr *cptr, void *data, uint32_t len)
{
	struct cnxk_cpt_qp *qp = PLT_PTR_CAST(qptr);
	int ret;

	if (unlikely(qptr == NULL)) {
		plt_err("Invalid queue pair pointer");
		return -EINVAL;
	}

	if (unlikely(cptr == NULL)) {
		plt_err("Invalid CPTR pointer");
		return -EINVAL;
	}

	if (unlikely(data == NULL)) {
		plt_err("Invalid data pointer");
		return -EINVAL;
	}

	ret = roc_cpt_lf_ctx_flush(&qp->lf, cptr, false);
	if (ret)
		return ret;

	/* Wait for the flush to complete. */
	rte_delay_ms(1);

	memcpy(data, cptr, len);
	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_cnxk_crypto_cptr_write, 24.07)
int
rte_pmd_cnxk_crypto_cptr_write(struct rte_pmd_cnxk_crypto_qptr *qptr,
			       struct rte_pmd_cnxk_crypto_cptr *cptr, void *data, uint32_t len)
{
	struct cnxk_cpt_qp *qp = PLT_PTR_CAST(qptr);
	int ret;

	if (unlikely(qptr == NULL)) {
		plt_err("Invalid queue pair pointer");
		return -EINVAL;
	}

	if (unlikely(cptr == NULL)) {
		plt_err("Invalid CPTR pointer");
		return -EINVAL;
	}

	if (unlikely(data == NULL)) {
		plt_err("Invalid data pointer");
		return -EINVAL;
	}

	ret = roc_cpt_ctx_write(&qp->lf, data, cptr, len);
	if (ret) {
		plt_err("Could not write to CPTR");
		return ret;
	}

	ret = roc_cpt_lf_ctx_flush(&qp->lf, cptr, false);
	if (ret)
		return ret;

	rte_atomic_thread_fence(rte_memory_order_seq_cst);

	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_cnxk_crypto_qp_stats_get, 24.07)
int
rte_pmd_cnxk_crypto_qp_stats_get(struct rte_pmd_cnxk_crypto_qptr *qptr,
				 struct rte_pmd_cnxk_crypto_qp_stats *stats)
{
	struct cnxk_cpt_qp *qp = PLT_PTR_CAST(qptr);
	struct roc_cpt_lf *lf;

	if (unlikely(qptr == NULL)) {
		plt_err("Invalid queue pair pointer");
		return -EINVAL;
	}

	if (unlikely(stats == NULL)) {
		plt_err("Invalid stats pointer");
		return -EINVAL;
	}

	if (unlikely(roc_model_is_cn9k())) {
		plt_err("Invalid cnxk model");
		return -EINVAL;
	}

	lf = &qp->lf;

	stats->ctx_enc_pkts = plt_read64(lf->rbase + CPT_LF_CTX_ENC_PKT_CNT);
	stats->ctx_enc_bytes = plt_read64(lf->rbase + CPT_LF_CTX_ENC_BYTE_CNT);
	stats->ctx_dec_pkts = plt_read64(lf->rbase + CPT_LF_CTX_DEC_PKT_CNT);
	stats->ctx_dec_bytes = plt_read64(lf->rbase + CPT_LF_CTX_DEC_BYTE_CNT);

	return 0;
}

#ifdef CPT_INST_DEBUG_ENABLE
void
cnxk_cpt_request_data_sgv2_mode_dump(uint8_t *in_buffer, bool glist, uint16_t components)
{
	struct roc_se_buf_ptr list_ptr[ROC_MAX_SG_CNT];
	const char *list = glist ? "glist" : "slist";
	struct roc_sg2list_comp *sg_ptr = NULL;
	uint16_t list_cnt = 0;
	char suffix[64];
	int i, j;

	sg_ptr = (void *)in_buffer;
	for (i = 0; i < components; i++) {
		for (j = 0; j < sg_ptr->u.s.valid_segs; j++) {
			list_ptr[i * 3 + j].size = sg_ptr->u.s.len[j];
			list_ptr[i * 3 + j].vaddr = (void *)sg_ptr->ptr[j];
			list_ptr[i * 3 + j].vaddr = list_ptr[i * 3 + j].vaddr;
			list_cnt++;
		}
		sg_ptr++;
	}

	plt_err("Current %s: %u", list, list_cnt);

	for (i = 0; i < list_cnt; i++) {
		snprintf(suffix, sizeof(suffix), "%s[%d]: vaddr 0x%" PRIx64 ", vaddr %p len %u",
			 list, i, (uint64_t)list_ptr[i].vaddr, list_ptr[i].vaddr, list_ptr[i].size);
		rte_hexdump(rte_log_get_stream(), suffix, list_ptr[i].vaddr, list_ptr[i].size);
	}
}

void
cnxk_cpt_request_data_sg_mode_dump(uint8_t *in_buffer, bool glist)
{
	struct roc_se_buf_ptr list_ptr[ROC_MAX_SG_CNT];
	const char *list = glist ? "glist" : "slist";
	struct roc_sglist_comp *sg_ptr = NULL;
	uint16_t list_cnt, components;
	char suffix[64];
	int i;

	sg_ptr = (void *)(in_buffer + 8);
	list_cnt = rte_be_to_cpu_16((((uint16_t *)in_buffer)[2]));
	if (!glist) {
		components = list_cnt / 4;
		if (list_cnt % 4)
			components++;
		sg_ptr += components;
		list_cnt = rte_be_to_cpu_16((((uint16_t *)in_buffer)[3]));
	}

	plt_err("Current %s: %u", list, list_cnt);
	components = list_cnt / 4;
	for (i = 0; i < components; i++) {
		list_ptr[i * 4 + 0].size = rte_be_to_cpu_16(sg_ptr->u.s.len[0]);
		list_ptr[i * 4 + 1].size = rte_be_to_cpu_16(sg_ptr->u.s.len[1]);
		list_ptr[i * 4 + 2].size = rte_be_to_cpu_16(sg_ptr->u.s.len[2]);
		list_ptr[i * 4 + 3].size = rte_be_to_cpu_16(sg_ptr->u.s.len[3]);
		list_ptr[i * 4 + 0].vaddr = (void *)rte_be_to_cpu_64(sg_ptr->ptr[0]);
		list_ptr[i * 4 + 1].vaddr = (void *)rte_be_to_cpu_64(sg_ptr->ptr[1]);
		list_ptr[i * 4 + 2].vaddr = (void *)rte_be_to_cpu_64(sg_ptr->ptr[2]);
		list_ptr[i * 4 + 3].vaddr = (void *)rte_be_to_cpu_64(sg_ptr->ptr[3]);
		list_ptr[i * 4 + 0].vaddr = list_ptr[i * 4 + 0].vaddr;
		list_ptr[i * 4 + 1].vaddr = list_ptr[i * 4 + 1].vaddr;
		list_ptr[i * 4 + 2].vaddr = list_ptr[i * 4 + 2].vaddr;
		list_ptr[i * 4 + 3].vaddr = list_ptr[i * 4 + 3].vaddr;
		sg_ptr++;
	}

	components = list_cnt % 4;
	switch (components) {
	case 3:
		list_ptr[i * 4 + 2].size = rte_be_to_cpu_16(sg_ptr->u.s.len[2]);
		list_ptr[i * 4 + 2].vaddr = (void *)rte_be_to_cpu_64(sg_ptr->ptr[2]);
		list_ptr[i * 4 + 2].vaddr = list_ptr[i * 4 + 2].vaddr;
		[[fallthrough]];
	case 2:
		list_ptr[i * 4 + 1].size = rte_be_to_cpu_16(sg_ptr->u.s.len[1]);
		list_ptr[i * 4 + 1].vaddr = (void *)rte_be_to_cpu_64(sg_ptr->ptr[1]);
		list_ptr[i * 4 + 1].vaddr = list_ptr[i * 4 + 1].vaddr;
		[[fallthrough]];
	case 1:
		list_ptr[i * 4 + 0].size = rte_be_to_cpu_16(sg_ptr->u.s.len[0]);
		list_ptr[i * 4 + 0].vaddr = (void *)rte_be_to_cpu_64(sg_ptr->ptr[0]);
		list_ptr[i * 4 + 0].vaddr = list_ptr[i * 4 + 0].vaddr;
		break;
	default:
		break;
	}

	for (i = 0; i < list_cnt; i++) {
		snprintf(suffix, sizeof(suffix), "%s[%d]: vaddr 0x%" PRIx64 ", vaddr %p len %u",
			 list, i, (uint64_t)list_ptr[i].vaddr, list_ptr[i].vaddr, list_ptr[i].size);
		rte_hexdump(rte_log_get_stream(), suffix, list_ptr[i].vaddr, list_ptr[i].size);
	}
}
#endif
