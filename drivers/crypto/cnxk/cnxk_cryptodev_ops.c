/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_errno.h>

#include "roc_cpt.h"

#include "cnxk_cryptodev.h"
#include "cnxk_cryptodev_ops.h"

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

int
cnxk_cpt_dev_config(struct rte_cryptodev *dev,
		    struct rte_cryptodev_config *conf)
{
	struct cnxk_cpt_vf *vf = dev->data->dev_private;
	struct roc_cpt *roc_cpt = &vf->cpt;
	uint16_t nb_lf_avail, nb_lf;
	int ret;

	dev->feature_flags &= ~conf->ff_disable;

	nb_lf_avail = roc_cpt->nb_lf_avail;
	nb_lf = conf->nb_queue_pairs;

	if (nb_lf > nb_lf_avail)
		return -ENOTSUP;

	ret = roc_cpt_dev_configure(roc_cpt, nb_lf);
	if (ret) {
		plt_err("Could not configure device");
		return ret;
	}

	return 0;
}

int
cnxk_cpt_dev_start(struct rte_cryptodev *dev)
{
	RTE_SET_USED(dev);

	return 0;
}

void
cnxk_cpt_dev_stop(struct rte_cryptodev *dev)
{
	RTE_SET_USED(dev);
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
	info->feature_flags = dev->feature_flags;
	info->capabilities = NULL;
	info->sym.max_nb_sessions = 0;
	info->min_mbuf_headroom_req = CNXK_CPT_MIN_HEADROOM_REQ;
	info->min_mbuf_tailroom_req = 0;
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
