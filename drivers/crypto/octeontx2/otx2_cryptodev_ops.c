/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019 Marvell International Ltd.
 */

#include <unistd.h>

#include <rte_cryptodev_pmd.h>
#include <rte_errno.h>

#include "otx2_cryptodev.h"
#include "otx2_cryptodev_capabilities.h"
#include "otx2_cryptodev_hw_access.h"
#include "otx2_cryptodev_mbox.h"
#include "otx2_cryptodev_ops.h"
#include "otx2_mbox.h"

#include "cpt_hw_types.h"
#include "cpt_pmd_logs.h"
#include "cpt_pmd_ops_helper.h"
#include "cpt_ucode.h"

#define METABUF_POOL_CACHE_SIZE	512

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
				int nb_elements)
{
	char mempool_name[RTE_MEMPOOL_NAMESIZE];
	int sg_mlen, lb_mlen, max_mlen, ret;
	struct cpt_qp_meta_info *meta_info;
	struct rte_mempool *pool;

	/* Get meta len for scatter gather mode */
	sg_mlen = cpt_pmd_ops_helper_get_mlen_sg_mode();

	/* Extra 32B saved for future considerations */
	sg_mlen += 4 * sizeof(uint64_t);

	/* Get meta len for linear buffer (direct) mode */
	lb_mlen = cpt_pmd_ops_helper_get_mlen_direct_mode();

	/* Extra 32B saved for future considerations */
	lb_mlen += 4 * sizeof(uint64_t);

	/* Check max requirement for meta buffer */
	max_mlen = RTE_MAX(lb_mlen, sg_mlen);

	/* Allocate mempool */

	snprintf(mempool_name, RTE_MEMPOOL_NAMESIZE, "otx2_cpt_mb_%u:%u",
		 dev->data->dev_id, qp_id);

	pool = rte_mempool_create_empty(mempool_name, nb_elements, max_mlen,
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
	len = iq_len * RTE_ALIGN(sizeof(struct rid), 8);

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
	qp->pend_q.rid_queue = (struct rid *)va;
	qp->pend_q.enq_tail = 0;
	qp->pend_q.deq_head = 0;
	qp->pend_q.pending_count = 0;

	used_len = iq_len * RTE_ALIGN(sizeof(struct rid), 8);
	used_len += size_div40 * 16;
	used_len = RTE_ALIGN(used_len, pg_sz);
	iova += used_len;

	qp->iq_dma_addr = iova;
	qp->id = qp_id;
	qp->base = OTX2_CPT_LF_BAR2(vf, qp_id);

	lmtline = vf->otx2_dev.bar2 +
		  (RVU_BLOCK_ADDR_LMT << 20 | qp_id << 12) +
		  OTX2_LMT_LF_LMTLINE(0);

	qp->lmtline = (void *)lmtline;

	qp->lf_nq_reg = qp->base + OTX2_CPT_LF_NQ(0);

	otx2_cpt_iq_disable(qp);

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
sym_session_configure(int driver_id, struct rte_crypto_sym_xform *xform,
		      struct rte_cryptodev_sym_session *sess,
		      struct rte_mempool *pool)
{
	struct cpt_sess_misc *misc;
	void *priv;
	int ret;

	if (unlikely(cpt_is_algo_supported(xform))) {
		CPT_LOG_ERR("Crypto xform not supported");
		return -ENOTSUP;
	}

	if (unlikely(rte_mempool_get(pool, &priv))) {
		CPT_LOG_ERR("Could not allocate session private data");
		return -ENOMEM;
	}

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

	set_sym_session_private_data(sess, driver_id, misc);

	misc->ctx_dma_addr = rte_mempool_virt2iova(misc) +
			     sizeof(struct cpt_sess_misc);

	/*
	 * IE engines support IPsec operations
	 * SE engines support IPsec operations and Air-Crypto operations
	 */
	if (misc->zsk_flag)
		misc->egrp = OTX2_CPT_EGRP_SE;
	else
		misc->egrp = OTX2_CPT_EGRP_SE_IE;

	return 0;

priv_put:
	rte_mempool_put(pool, priv);

	CPT_LOG_ERR("Crypto xform not supported");
	return -ENOTSUP;
}

static void
sym_session_clear(int driver_id, struct rte_cryptodev_sym_session *sess)
{
	void *priv = get_sym_session_private_data(sess, driver_id);
	struct rte_mempool *pool;

	if (priv == NULL)
		return;

	memset(priv, 0, cpt_get_session_size());

	pool = rte_mempool_from_obj(priv);

	set_sym_session_private_data(sess, driver_id, NULL);

	rte_mempool_put(pool, priv);
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

	dev->feature_flags &= ~conf->ff_disable;

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

	rte_mb();
	return 0;

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
	RTE_SET_USED(dev);

	CPT_PMD_INIT_FUNC_TRACE();
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
		info->feature_flags = dev->feature_flags;
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
	.queue_pair_count = NULL,

	/* Symmetric crypto ops */
	.sym_session_get_size = otx2_cpt_sym_session_get_size,
	.sym_session_configure = otx2_cpt_sym_session_configure,
	.sym_session_clear = otx2_cpt_sym_session_clear,
};
