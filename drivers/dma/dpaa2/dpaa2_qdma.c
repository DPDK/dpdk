/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2024 NXP
 */

#include <rte_eal.h>
#include <bus_fslmc_driver.h>
#include <rte_dmadev.h>
#include <rte_dmadev_pmd.h>
#include <rte_kvargs.h>

#include <mc/fsl_dpdmai.h>

#include <rte_pmd_dpaax_qdma.h>
#include "dpaa2_hw_dpio.h"
#include "dpaa2_qdma.h"
#include "dpaa2_qdma_logs.h"

#define DPAA2_QDMA_FLE_PRE_POPULATE "fle_pre_populate"
#define DPAA2_QDMA_DESC_DEBUG "desc_debug"
#define DPAA2_QDMA_USING_SHORT_FD "short_fd"

static uint32_t dpaa2_coherent_no_alloc_cache;
static uint32_t dpaa2_coherent_alloc_cache;

static struct fsl_mc_io s_proc_mc_reg;

static int
check_devargs_handler(__rte_unused const char *key, const char *value,
		      __rte_unused void *opaque)
{
	if (strcmp(value, "1"))
		return -1;

	return 0;
}

static int
dpaa2_qdma_get_devargs(struct rte_devargs *devargs, const char *key)
{
	struct rte_kvargs *kvlist;

	if (!devargs)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (!kvlist)
		return 0;

	if (!rte_kvargs_count(kvlist, key)) {
		rte_kvargs_free(kvlist);
		return 0;
	}

	if (rte_kvargs_process(kvlist, key,
			       check_devargs_handler, NULL) < 0) {
		rte_kvargs_free(kvlist);
		return 0;
	}
	rte_kvargs_free(kvlist);

	return 1;
}

static inline int
qdma_cntx_idx_ring_eq(struct qdma_cntx_idx_ring *ring,
	const uint16_t *elem, uint16_t nb,
	uint16_t *free_space)
{
	uint16_t i;

	if (unlikely(nb > ring->free_space))
		return 0;

	for (i = 0; i < nb; i++) {
		ring->cntx_idx_ring[ring->tail] = elem[i];
		ring->tail = (ring->tail + 1) &
			(DPAA2_QDMA_MAX_DESC - 1);
	}
	ring->free_space -= nb;
	ring->nb_in_ring += nb;

	if (free_space)
		*free_space = ring->free_space;

	return nb;
}

static inline int
qdma_cntx_idx_ring_dq(struct qdma_cntx_idx_ring *ring,
	uint16_t *elem, uint16_t max)
{
	int ret = ring->nb_in_ring > max ? max : ring->nb_in_ring;

	if (!ret)
		return 0;

	if ((ring->start + ret) < DPAA2_QDMA_MAX_DESC) {
		rte_memcpy(elem,
			&ring->cntx_idx_ring[ring->start],
			ret * sizeof(uint16_t));
		ring->start += ret;
	} else {
		rte_memcpy(elem,
			&ring->cntx_idx_ring[ring->start],
			(DPAA2_QDMA_MAX_DESC - ring->start) *
			sizeof(uint16_t));
		rte_memcpy(&elem[DPAA2_QDMA_MAX_DESC - ring->start],
			&ring->cntx_idx_ring[0],
			(ret - DPAA2_QDMA_MAX_DESC + ring->start) *
			sizeof(uint16_t));
		ring->start = (ring->start + ret) & (DPAA2_QDMA_MAX_DESC - 1);
	}
	ring->free_space += ret;
	ring->nb_in_ring -= ret;

	return ret;
}

static int
dpaa2_qdma_multi_eq(struct qdma_virt_queue *qdma_vq)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = qdma_vq->dpdmai_dev;
	uint16_t txq_id = dpdmai_dev->tx_queue[qdma_vq->vq_id].fqid;
	struct qbman_eq_desc eqdesc;
	struct qbman_swp *swp;
	uint32_t num_to_send = 0;
	uint16_t num_tx = 0;
	uint32_t enqueue_loop, loop;
	int ret;
	struct qbman_fd *fd = qdma_vq->fd;
	uint16_t nb_fds = qdma_vq->fd_idx, idx, dst_idx;

	if (unlikely(!DPAA2_PER_LCORE_DPIO)) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			DPAA2_QDMA_ERR("Failed to allocate IO portal, tid: %d",
				rte_gettid());
			return -EIO;
		}
	}
	swp = DPAA2_PER_LCORE_PORTAL;

	/* Prepare enqueue descriptor */
	qbman_eq_desc_clear(&eqdesc);
	qbman_eq_desc_set_fq(&eqdesc, txq_id);
	qbman_eq_desc_set_no_orp(&eqdesc, 0);
	qbman_eq_desc_set_response(&eqdesc, 0, 0);

	while (nb_fds > 0) {
		num_to_send = (nb_fds > dpaa2_eqcr_size) ?
			dpaa2_eqcr_size : nb_fds;

		/* Enqueue the packet to the QBMAN */
		enqueue_loop = 0;
		loop = num_to_send;

		while (enqueue_loop < loop) {
			ret = qbman_swp_enqueue_multiple(swp,
				&eqdesc,
				&fd[num_tx + enqueue_loop],
				NULL,
				loop - enqueue_loop);
			if (likely(ret >= 0))
				enqueue_loop += ret;
		}
		num_tx += num_to_send;
		nb_fds -= loop;
	}

	qdma_vq->num_enqueues += num_tx;
	if (unlikely(num_tx != qdma_vq->fd_idx)) {
		dst_idx = 0;
		for (idx = num_tx; idx < qdma_vq->fd_idx; idx++) {
			qdma_vq->fd[dst_idx] = qdma_vq->fd[idx];
			dst_idx++;
		}
	}
	qdma_vq->fd_idx -= num_tx;

	return num_tx;
}

static void
fle_sdd_pre_populate(struct qdma_cntx_fle_sdd *fle_sdd,
	struct dpaa2_qdma_rbp *rbp, uint64_t src, uint64_t dest,
	uint32_t fmt)
{
	struct qbman_fle *fle = fle_sdd->fle;
	struct qdma_sdd *sdd = fle_sdd->sdd;
	uint64_t sdd_iova = DPAA2_VADDR_TO_IOVA(sdd);

	/* first frame list to source descriptor */
	DPAA2_SET_FLE_ADDR(&fle[DPAA2_QDMA_SDD_FLE], sdd_iova);
	DPAA2_SET_FLE_LEN(&fle[DPAA2_QDMA_SDD_FLE],
		DPAA2_QDMA_MAX_SDD * (sizeof(struct qdma_sdd)));

	/* source and destination descriptor */
	if (rbp && rbp->enable) {
		/* source */
		sdd[DPAA2_QDMA_SRC_SDD].read_cmd.portid =
			rbp->sportid;
		sdd[DPAA2_QDMA_SRC_SDD].rbpcmd_simple.pfid =
			rbp->spfid;
		sdd[DPAA2_QDMA_SRC_SDD].rbpcmd_simple.vfid =
			rbp->svfid;
		sdd[DPAA2_QDMA_SRC_SDD].rbpcmd_simple.vfa =
			rbp->svfa;

		if (rbp->srbp) {
			sdd[DPAA2_QDMA_SRC_SDD].read_cmd.rbp =
				rbp->srbp;
			sdd[DPAA2_QDMA_SRC_SDD].read_cmd.rdtype =
				DPAA2_RBP_MEM_RW;
		} else {
			sdd[DPAA2_QDMA_SRC_SDD].read_cmd.rdtype =
				dpaa2_coherent_no_alloc_cache;
		}
		/* destination */
		sdd[DPAA2_QDMA_DST_SDD].write_cmd.portid =
			rbp->dportid;
		sdd[DPAA2_QDMA_DST_SDD].rbpcmd_simple.pfid =
			rbp->dpfid;
		sdd[DPAA2_QDMA_DST_SDD].rbpcmd_simple.vfid =
			rbp->dvfid;
		sdd[DPAA2_QDMA_DST_SDD].rbpcmd_simple.vfa =
			rbp->dvfa;

		if (rbp->drbp) {
			sdd[DPAA2_QDMA_DST_SDD].write_cmd.rbp =
				rbp->drbp;
			sdd[DPAA2_QDMA_DST_SDD].write_cmd.wrttype =
				DPAA2_RBP_MEM_RW;
		} else {
			sdd[DPAA2_QDMA_DST_SDD].write_cmd.wrttype =
				dpaa2_coherent_alloc_cache;
		}
	} else {
		sdd[DPAA2_QDMA_SRC_SDD].read_cmd.rdtype =
			dpaa2_coherent_no_alloc_cache;
		sdd[DPAA2_QDMA_DST_SDD].write_cmd.wrttype =
			dpaa2_coherent_alloc_cache;
	}
	/* source frame list to source buffer */
	DPAA2_SET_FLE_ADDR(&fle[DPAA2_QDMA_SRC_FLE], src);
	/** IOMMU is always on for either VA or PA mode,
	 * so Bypass Memory Translation should be disabled.
	 *
	 * DPAA2_SET_FLE_BMT(&fle[DPAA2_QDMA_SRC_FLE]);
	 * DPAA2_SET_FLE_BMT(&fle[DPAA2_QDMA_DST_FLE]);
	 */
	fle[DPAA2_QDMA_SRC_FLE].word4.fmt = fmt;

	/* destination frame list to destination buffer */
	DPAA2_SET_FLE_ADDR(&fle[DPAA2_QDMA_DST_FLE], dest);
	fle[DPAA2_QDMA_DST_FLE].word4.fmt = fmt;

	/* Final bit: 1, for last frame list */
	DPAA2_SET_FLE_FIN(&fle[DPAA2_QDMA_DST_FLE]);
}

static void
sg_entry_pre_populate(struct qdma_cntx_sg *sg_cntx)
{
	uint16_t i;
	struct qdma_sg_entry *src_sge = sg_cntx->sg_src_entry;
	struct qdma_sg_entry *dst_sge = sg_cntx->sg_dst_entry;

	for (i = 0; i < RTE_DPAAX_QDMA_JOB_SUBMIT_MAX; i++) {
		/* source SG */
		src_sge[i].ctrl.sl = QDMA_SG_SL_LONG;
		src_sge[i].ctrl.fmt = QDMA_SG_FMT_SDB;
		/** IOMMU is always on for either VA or PA mode,
		 * so Bypass Memory Translation should be disabled.
		 */
		src_sge[i].ctrl.bmt = QDMA_SG_BMT_DISABLE;
		/* destination SG */
		dst_sge[i].ctrl.sl = QDMA_SG_SL_LONG;
		dst_sge[i].ctrl.fmt = QDMA_SG_FMT_SDB;
		/** IOMMU is always on for either VA or PA mode,
		 * so Bypass Memory Translation should be disabled.
		 */
		dst_sge[i].ctrl.bmt = QDMA_SG_BMT_DISABLE;
	}
}

static void
fle_sdd_sg_pre_populate(struct qdma_cntx_sg *sg_cntx,
	struct qdma_virt_queue *qdma_vq)
{
	struct qdma_sg_entry *src_sge = sg_cntx->sg_src_entry;
	struct qdma_sg_entry *dst_sge = sg_cntx->sg_dst_entry;
	rte_iova_t src_sge_iova, dst_sge_iova;
	struct dpaa2_qdma_rbp *rbp = &qdma_vq->rbp;

	memset(sg_cntx, 0, sizeof(struct qdma_cntx_sg));

	src_sge_iova = DPAA2_VADDR_TO_IOVA(src_sge);
	dst_sge_iova = DPAA2_VADDR_TO_IOVA(dst_sge);

	sg_entry_pre_populate(sg_cntx);
	fle_sdd_pre_populate(&sg_cntx->fle_sdd,
		rbp, src_sge_iova, dst_sge_iova,
		QBMAN_FLE_WORD4_FMT_SGE);
}

static inline uint32_t
sg_entry_post_populate(const struct rte_dma_sge *src,
	const struct rte_dma_sge *dst, struct qdma_cntx_sg *sg_cntx,
	uint16_t nb_sge)
{
	uint16_t i;
	uint32_t total_len = 0;
	struct qdma_sg_entry *src_sge = sg_cntx->sg_src_entry;
	struct qdma_sg_entry *dst_sge = sg_cntx->sg_dst_entry;

	for (i = 0; i < (nb_sge - 1); i++) {
		if (unlikely(src[i].length != dst[i].length))
			return -ENOTSUP;
		src_sge->addr_lo = (uint32_t)src[i].addr;
		src_sge->addr_hi = (src[i].addr >> 32);
		src_sge->data_len.data_len_sl0 = src[i].length;

		dst_sge->addr_lo = (uint32_t)dst[i].addr;
		dst_sge->addr_hi = (dst[i].addr >> 32);
		dst_sge->data_len.data_len_sl0 = dst[i].length;
		total_len += dst[i].length;

		src_sge->ctrl.f = 0;
		dst_sge->ctrl.f = 0;
		src_sge++;
		dst_sge++;
	}

	if (unlikely(src[i].length != dst[i].length))
		return -ENOTSUP;

	src_sge->addr_lo = (uint32_t)src[i].addr;
	src_sge->addr_hi = (src[i].addr >> 32);
	src_sge->data_len.data_len_sl0 = src[i].length;

	dst_sge->addr_lo = (uint32_t)dst[i].addr;
	dst_sge->addr_hi = (dst[i].addr >> 32);
	dst_sge->data_len.data_len_sl0 = dst[i].length;

	total_len += dst[i].length;
	sg_cntx->job_nb = nb_sge;

	src_sge->ctrl.f = QDMA_SG_F;
	dst_sge->ctrl.f = QDMA_SG_F;

	return total_len;
}

static inline void
sg_fle_post_populate(struct qbman_fle fle[],
	size_t len)
{
	DPAA2_SET_FLE_LEN(&fle[DPAA2_QDMA_SRC_FLE], len);
	DPAA2_SET_FLE_LEN(&fle[DPAA2_QDMA_DST_FLE], len);
}

static inline uint32_t
sg_entry_populate(const struct rte_dma_sge *src,
	const struct rte_dma_sge *dst, struct qdma_cntx_sg *sg_cntx,
	uint16_t nb_sge)
{
	uint16_t i;
	uint32_t total_len = 0;
	struct qdma_sg_entry *src_sge = sg_cntx->sg_src_entry;
	struct qdma_sg_entry *dst_sge = sg_cntx->sg_dst_entry;

	for (i = 0; i < nb_sge; i++) {
		if (unlikely(src[i].length != dst[i].length))
			return -ENOTSUP;

		src_sge->addr_lo = (uint32_t)src[i].addr;
		src_sge->addr_hi = (src[i].addr >> 32);
		src_sge->data_len.data_len_sl0 = src[i].length;
		src_sge->ctrl.sl = QDMA_SG_SL_LONG;
		src_sge->ctrl.fmt = QDMA_SG_FMT_SDB;
		/** IOMMU is always on for either VA or PA mode,
		 * so Bypass Memory Translation should be disabled.
		 */
		src_sge->ctrl.bmt = QDMA_SG_BMT_DISABLE;
		dst_sge->addr_lo = (uint32_t)dst[i].addr;
		dst_sge->addr_hi = (dst[i].addr >> 32);
		dst_sge->data_len.data_len_sl0 = dst[i].length;
		dst_sge->ctrl.sl = QDMA_SG_SL_LONG;
		dst_sge->ctrl.fmt = QDMA_SG_FMT_SDB;
		/** IOMMU is always on for either VA or PA mode,
		 * so Bypass Memory Translation should be disabled.
		 */
		dst_sge->ctrl.bmt = QDMA_SG_BMT_DISABLE;
		total_len += src[i].length;

		if (i == (nb_sge - 1)) {
			src_sge->ctrl.f = QDMA_SG_F;
			dst_sge->ctrl.f = QDMA_SG_F;
		} else {
			src_sge->ctrl.f = 0;
			dst_sge->ctrl.f = 0;
		}
		src_sge++;
		dst_sge++;
	}

	sg_cntx->job_nb = nb_sge;

	return total_len;
}

static inline void
fle_populate(struct qbman_fle fle[],
	struct qdma_sdd sdd[], uint64_t sdd_iova,
	struct dpaa2_qdma_rbp *rbp,
	uint64_t src_iova, uint64_t dst_iova, size_t len,
	uint32_t fmt)
{
	/* first frame list to source descriptor */
	DPAA2_SET_FLE_ADDR(&fle[DPAA2_QDMA_SDD_FLE], sdd_iova);
	DPAA2_SET_FLE_LEN(&fle[DPAA2_QDMA_SDD_FLE],
		(DPAA2_QDMA_MAX_SDD * (sizeof(struct qdma_sdd))));

	/* source and destination descriptor */
	if (rbp && rbp->enable) {
		/* source */
		sdd[DPAA2_QDMA_SRC_SDD].read_cmd.portid =
			rbp->sportid;
		sdd[DPAA2_QDMA_SRC_SDD].rbpcmd_simple.pfid =
			rbp->spfid;
		sdd[DPAA2_QDMA_SRC_SDD].rbpcmd_simple.vfid =
			rbp->svfid;
		sdd[DPAA2_QDMA_SRC_SDD].rbpcmd_simple.vfa =
			rbp->svfa;

		if (rbp->srbp) {
			sdd[DPAA2_QDMA_SRC_SDD].read_cmd.rbp =
				rbp->srbp;
			sdd[DPAA2_QDMA_SRC_SDD].read_cmd.rdtype =
				DPAA2_RBP_MEM_RW;
		} else {
			sdd[DPAA2_QDMA_SRC_SDD].read_cmd.rdtype =
				dpaa2_coherent_no_alloc_cache;
		}
		/* destination */
		sdd[DPAA2_QDMA_DST_SDD].write_cmd.portid =
			rbp->dportid;
		sdd[DPAA2_QDMA_DST_SDD].rbpcmd_simple.pfid =
			rbp->dpfid;
		sdd[DPAA2_QDMA_DST_SDD].rbpcmd_simple.vfid =
			rbp->dvfid;
		sdd[DPAA2_QDMA_DST_SDD].rbpcmd_simple.vfa =
			rbp->dvfa;

		if (rbp->drbp) {
			sdd[DPAA2_QDMA_DST_SDD].write_cmd.rbp =
				rbp->drbp;
			sdd[DPAA2_QDMA_DST_SDD].write_cmd.wrttype =
				DPAA2_RBP_MEM_RW;
		} else {
			sdd[DPAA2_QDMA_DST_SDD].write_cmd.wrttype =
				dpaa2_coherent_alloc_cache;
		}

	} else {
		sdd[DPAA2_QDMA_SRC_SDD].read_cmd.rdtype =
			dpaa2_coherent_no_alloc_cache;
		sdd[DPAA2_QDMA_DST_SDD].write_cmd.wrttype =
			dpaa2_coherent_alloc_cache;
	}
	/* source frame list to source buffer */
	DPAA2_SET_FLE_ADDR(&fle[DPAA2_QDMA_SRC_FLE], src_iova);
	/** IOMMU is always on for either VA or PA mode,
	 * so Bypass Memory Translation should be disabled.
	 * DPAA2_SET_FLE_BMT(&fle[DPAA2_QDMA_SRC_FLE]);
	 * DPAA2_SET_FLE_BMT(&fle[DPAA2_QDMA_DST_FLE]);
	 */
	fle[DPAA2_QDMA_SRC_FLE].word4.fmt = fmt;
	DPAA2_SET_FLE_LEN(&fle[DPAA2_QDMA_SRC_FLE], len);

	/* destination frame list to destination buffer */
	DPAA2_SET_FLE_ADDR(&fle[DPAA2_QDMA_DST_FLE], dst_iova);
	fle[DPAA2_QDMA_DST_FLE].word4.fmt = fmt;
	DPAA2_SET_FLE_LEN(&fle[DPAA2_QDMA_DST_FLE], len);

	/* Final bit: 1, for last frame list */
	DPAA2_SET_FLE_FIN(&fle[DPAA2_QDMA_DST_FLE]);
}

static inline void
fle_post_populate(struct qbman_fle fle[],
	uint64_t src, uint64_t dest, size_t len)
{
	DPAA2_SET_FLE_ADDR(&fle[DPAA2_QDMA_SRC_FLE], src);
	DPAA2_SET_FLE_LEN(&fle[DPAA2_QDMA_SRC_FLE], len);

	DPAA2_SET_FLE_ADDR(&fle[DPAA2_QDMA_DST_FLE], dest);
	DPAA2_SET_FLE_LEN(&fle[DPAA2_QDMA_DST_FLE], len);
}

static inline int
dpaa2_qdma_submit(void *dev_private, uint16_t vchan)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;
	struct qdma_virt_queue *qdma_vq = &qdma_dev->vqs[vchan];
	uint16_t expected = qdma_vq->fd_idx;
	int ret;

	ret = dpaa2_qdma_multi_eq(qdma_vq);
	if (likely(ret == expected))
		return 0;

	return -EBUSY;
}

static inline void
dpaa2_qdma_fle_dump(const struct qbman_fle *fle)
{
	DPAA2_QDMA_INFO("addr:0x%08x-0x%08x, len:%d, frc:0x%08x, bpid:%d",
		fle->addr_hi, fle->addr_lo, fle->length, fle->frc,
		fle->word4.bpid);
	DPAA2_QDMA_INFO("ivp:%d, bmt:%d, off:%d, fmt:%d, sl:%d, f:%d",
		fle->word4.ivp, fle->word4.bmt, fle->word4.offset,
		fle->word4.fmt, fle->word4.sl, fle->word4.f);
}

static inline void
dpaa2_qdma_sdd_dump(const struct qdma_sdd *sdd)
{
	DPAA2_QDMA_INFO("stride:%d, rbpcmd:0x%08x, cmd:0x%08x",
		sdd->stride, sdd->rbpcmd, sdd->cmd);
}

static inline void
dpaa2_qdma_sge_dump(const struct qdma_sg_entry *sge)
{
	DPAA2_QDMA_INFO("addr 0x%08x-0x%08x, len:0x%08x, ctl:0x%08x",
		sge->addr_hi, sge->addr_lo, sge->data_len.data_len_sl0,
		sge->ctrl_fields);
}

static void
dpaa2_qdma_long_fmt_dump(const struct qbman_fle *fle)
{
	int i;
	const struct qdma_cntx_fle_sdd *fle_sdd;
	const struct qdma_sdd *sdd;
	const struct qdma_cntx_sg *cntx_sg = NULL;

	fle_sdd = container_of(fle, const struct qdma_cntx_fle_sdd, fle[0]);
	sdd = fle_sdd->sdd;

	for (i = 0; i < DPAA2_QDMA_MAX_FLE; i++) {
		DPAA2_QDMA_INFO("fle[%d] info:", i);
		dpaa2_qdma_fle_dump(&fle[i]);
	}

	if (fle[DPAA2_QDMA_SRC_FLE].word4.fmt !=
		fle[DPAA2_QDMA_DST_FLE].word4.fmt) {
		DPAA2_QDMA_ERR("fle[%d].fmt(%d) != fle[%d].fmt(%d)",
			DPAA2_QDMA_SRC_FLE,
			fle[DPAA2_QDMA_SRC_FLE].word4.fmt,
			DPAA2_QDMA_DST_FLE,
			fle[DPAA2_QDMA_DST_FLE].word4.fmt);

		return;
	} else if (fle[DPAA2_QDMA_SRC_FLE].word4.fmt ==
		QBMAN_FLE_WORD4_FMT_SGE) {
		cntx_sg = container_of(fle_sdd, const struct qdma_cntx_sg,
			fle_sdd);
	} else if (fle[DPAA2_QDMA_SRC_FLE].word4.fmt !=
		QBMAN_FLE_WORD4_FMT_SBF) {
		DPAA2_QDMA_ERR("Unsupported fle format:%d",
			fle[DPAA2_QDMA_SRC_FLE].word4.fmt);
		return;
	}

	for (i = 0; i < DPAA2_QDMA_MAX_SDD; i++) {
		DPAA2_QDMA_INFO("sdd[%d] info:", i);
		dpaa2_qdma_sdd_dump(&sdd[i]);
	}

	if (cntx_sg) {
		DPAA2_QDMA_INFO("long format/SG format, job number:%d",
			cntx_sg->job_nb);
		if (!cntx_sg->job_nb ||
			cntx_sg->job_nb > RTE_DPAAX_QDMA_JOB_SUBMIT_MAX) {
			DPAA2_QDMA_ERR("Invalid SG job number:%d",
				cntx_sg->job_nb);
			return;
		}
		for (i = 0; i < cntx_sg->job_nb; i++) {
			DPAA2_QDMA_INFO("sg[%d] src info:", i);
			dpaa2_qdma_sge_dump(&cntx_sg->sg_src_entry[i]);
			DPAA2_QDMA_INFO("sg[%d] dst info:", i);
			dpaa2_qdma_sge_dump(&cntx_sg->sg_dst_entry[i]);
			DPAA2_QDMA_INFO("cntx_idx[%d]:%d", i,
				cntx_sg->cntx_idx[i]);
		}
	} else {
		DPAA2_QDMA_INFO("long format/Single buffer cntx");
	}
}

static int
dpaa2_qdma_copy_sg(void *dev_private,
	uint16_t vchan,
	const struct rte_dma_sge *src,
	const struct rte_dma_sge *dst,
	uint16_t nb_src, uint16_t nb_dst,
	uint64_t flags)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;
	struct qdma_virt_queue *qdma_vq = &qdma_dev->vqs[vchan];
	int ret = 0, expected, i;
	uint32_t len;
	struct qbman_fd *fd = &qdma_vq->fd[qdma_vq->fd_idx];
	struct qdma_cntx_sg *cntx_sg = NULL;
	rte_iova_t cntx_iova, fle_iova, sdd_iova;
	rte_iova_t src_sge_iova, dst_sge_iova;
	struct qbman_fle *fle;
	struct qdma_sdd *sdd;
	const uint16_t *idx_addr = NULL;

	if (unlikely(nb_src != nb_dst)) {
		DPAA2_QDMA_ERR("SG entry src num(%d) != dst num(%d)",
			nb_src, nb_dst);
		return -ENOTSUP;
	}

	if (unlikely(!nb_src)) {
		DPAA2_QDMA_ERR("No SG entry specified");
		return -EINVAL;
	}

	if (unlikely(nb_src > RTE_DPAAX_QDMA_JOB_SUBMIT_MAX)) {
		DPAA2_QDMA_ERR("SG entry number(%d) > MAX(%d)",
			nb_src, RTE_DPAAX_QDMA_JOB_SUBMIT_MAX);
		return -EINVAL;
	}

	memset(fd, 0, sizeof(struct qbman_fd));

	if (qdma_dev->is_silent) {
		cntx_sg = qdma_vq->cntx_sg[qdma_vq->silent_idx];
	} else {
		ret = rte_mempool_get(qdma_vq->fle_pool,
			(void **)&cntx_sg);
		if (ret)
			return ret;
		DPAA2_SET_FD_FRC(fd, QDMA_SER_CTX);
		idx_addr = DPAA2_QDMA_IDXADDR_FROM_SG_FLAG(flags);
		for (i = 0; i < nb_src; i++)
			cntx_sg->cntx_idx[i] = idx_addr[i];
	}

	cntx_iova = (uint64_t)cntx_sg - qdma_vq->fle_iova2va_offset;

	fle = cntx_sg->fle_sdd.fle;
	fle_iova = cntx_iova +
		offsetof(struct qdma_cntx_sg, fle_sdd) +
		offsetof(struct qdma_cntx_fle_sdd, fle);

	dpaa2_qdma_fd_set_addr(fd, fle_iova);
	DPAA2_SET_FD_COMPOUND_FMT(fd);
	DPAA2_SET_FD_FLC(fd, (uint64_t)cntx_sg);

	if (qdma_vq->fle_pre_populate) {
		if (unlikely(!fle[DPAA2_QDMA_SRC_FLE].length)) {
			fle_sdd_sg_pre_populate(cntx_sg, qdma_vq);
			if (!qdma_dev->is_silent && cntx_sg && idx_addr) {
				for (i = 0; i < nb_src; i++)
					cntx_sg->cntx_idx[i] = idx_addr[i];
			}
		}

		len = sg_entry_post_populate(src, dst,
			cntx_sg, nb_src);
		sg_fle_post_populate(fle, len);
	} else {
		sdd = cntx_sg->fle_sdd.sdd;
		sdd_iova = cntx_iova +
			offsetof(struct qdma_cntx_sg, fle_sdd) +
			offsetof(struct qdma_cntx_fle_sdd, sdd);
		src_sge_iova = cntx_iova +
			offsetof(struct qdma_cntx_sg, sg_src_entry);
		dst_sge_iova = cntx_iova +
			offsetof(struct qdma_cntx_sg, sg_dst_entry);
		len = sg_entry_populate(src, dst, cntx_sg, nb_src);

		fle_populate(fle, sdd, sdd_iova,
			&qdma_vq->rbp, src_sge_iova, dst_sge_iova, len,
			QBMAN_FLE_WORD4_FMT_SGE);
	}

	if (unlikely(qdma_vq->flags & DPAA2_QDMA_DESC_DEBUG_FLAG))
		dpaa2_qdma_long_fmt_dump(cntx_sg->fle_sdd.fle);

	dpaa2_qdma_fd_save_att(fd, 0, DPAA2_QDMA_FD_SG);
	qdma_vq->fd_idx++;
	qdma_vq->silent_idx =
		(qdma_vq->silent_idx + 1) & (DPAA2_QDMA_MAX_DESC - 1);

	if (flags & RTE_DMA_OP_FLAG_SUBMIT) {
		expected = qdma_vq->fd_idx;
		ret = dpaa2_qdma_multi_eq(qdma_vq);
		if (likely(ret == expected)) {
			qdma_vq->copy_num += nb_src;
			return (qdma_vq->copy_num - 1) & UINT16_MAX;
		}
	} else {
		qdma_vq->copy_num += nb_src;
		return (qdma_vq->copy_num - 1) & UINT16_MAX;
	}

	return ret;
}

static inline void
qdma_populate_fd_pci(uint64_t src, uint64_t dest,
	uint32_t len, struct qbman_fd *fd,
	struct dpaa2_qdma_rbp *rbp, int ser)
{
	fd->simple_pci.saddr_lo = lower_32_bits(src);
	fd->simple_pci.saddr_hi = upper_32_bits(src);

	fd->simple_pci.len_sl = len;

	fd->simple_pci.bmt = DPAA2_QDMA_BMT_DISABLE;
	fd->simple_pci.fmt = DPAA2_QDMA_FD_SHORT_FORMAT;
	fd->simple_pci.sl = 1;
	fd->simple_pci.ser = ser;
	if (ser)
		fd->simple.frc |= QDMA_SER_CTX;

	fd->simple_pci.sportid = rbp->sportid;

	fd->simple_pci.svfid = rbp->svfid;
	fd->simple_pci.spfid = rbp->spfid;
	fd->simple_pci.svfa = rbp->svfa;
	fd->simple_pci.dvfid = rbp->dvfid;
	fd->simple_pci.dpfid = rbp->dpfid;
	fd->simple_pci.dvfa = rbp->dvfa;

	fd->simple_pci.srbp = rbp->srbp;
	if (rbp->srbp)
		fd->simple_pci.rdttype = 0;
	else
		fd->simple_pci.rdttype = dpaa2_coherent_alloc_cache;

	/*dest is pcie memory */
	fd->simple_pci.dportid = rbp->dportid;
	fd->simple_pci.drbp = rbp->drbp;
	if (rbp->drbp)
		fd->simple_pci.wrttype = 0;
	else
		fd->simple_pci.wrttype = dpaa2_coherent_no_alloc_cache;

	fd->simple_pci.daddr_lo = lower_32_bits(dest);
	fd->simple_pci.daddr_hi = upper_32_bits(dest);
}

static inline void
qdma_populate_fd_ddr(uint64_t src, uint64_t dest,
	uint32_t len, struct qbman_fd *fd, int ser)
{
	fd->simple_ddr.saddr_lo = lower_32_bits(src);
	fd->simple_ddr.saddr_hi = upper_32_bits(src);

	fd->simple_ddr.len = len;

	fd->simple_ddr.bmt = DPAA2_QDMA_BMT_DISABLE;
	fd->simple_ddr.fmt = DPAA2_QDMA_FD_SHORT_FORMAT;
	fd->simple_ddr.sl = 1;
	fd->simple_ddr.ser = ser;
	if (ser)
		fd->simple.frc |= QDMA_SER_CTX;
	/**
	 * src If RBP=0 {NS,RDTTYPE[3:0]}: 0_1011
	 * Coherent copy of cacheable memory,
	 * lookup in downstream cache, no allocate
	 * on miss.
	 */
	fd->simple_ddr.rns = 0;
	fd->simple_ddr.rdttype = dpaa2_coherent_alloc_cache;
	/**
	 * dest If RBP=0 {NS,WRTTYPE[3:0]}: 0_0111
	 * Coherent write of cacheable memory,
	 * lookup in downstream cache, no allocate on miss
	 */
	fd->simple_ddr.wns = 0;
	fd->simple_ddr.wrttype = dpaa2_coherent_no_alloc_cache;

	fd->simple_ddr.daddr_lo = lower_32_bits(dest);
	fd->simple_ddr.daddr_hi = upper_32_bits(dest);
}

static int
dpaa2_qdma_short_copy(struct qdma_virt_queue *qdma_vq,
	rte_iova_t src, rte_iova_t dst, uint32_t length,
	int is_silent, uint64_t flags)
{
	int ret = 0, expected;
	struct qbman_fd *fd = &qdma_vq->fd[qdma_vq->fd_idx];

	memset(fd, 0, sizeof(struct qbman_fd));

	if (qdma_vq->rbp.drbp || qdma_vq->rbp.srbp) {
		/** PCIe EP*/
		qdma_populate_fd_pci(src,
			dst, length,
			fd, &qdma_vq->rbp,
			is_silent ? 0 : 1);
	} else {
		/** DDR or PCIe RC*/
		qdma_populate_fd_ddr(src,
			dst, length,
			fd, is_silent ? 0 : 1);
	}
	dpaa2_qdma_fd_save_att(fd, DPAA2_QDMA_IDX_FROM_FLAG(flags),
		DPAA2_QDMA_FD_SHORT);
	qdma_vq->fd_idx++;

	if (flags & RTE_DMA_OP_FLAG_SUBMIT) {
		expected = qdma_vq->fd_idx;
		ret = dpaa2_qdma_multi_eq(qdma_vq);
		if (likely(ret == expected)) {
			qdma_vq->copy_num++;
			return (qdma_vq->copy_num - 1) & UINT16_MAX;
		}
	} else {
		qdma_vq->copy_num++;
		return (qdma_vq->copy_num - 1) & UINT16_MAX;
	}

	return ret;
}

static int
dpaa2_qdma_long_copy(struct qdma_virt_queue *qdma_vq,
	rte_iova_t src, rte_iova_t dst, uint32_t length,
	int is_silent, uint64_t flags)
{
	int ret = 0, expected;
	struct qbman_fd *fd = &qdma_vq->fd[qdma_vq->fd_idx];
	struct qdma_cntx_fle_sdd *fle_sdd = NULL;
	rte_iova_t fle_iova, sdd_iova;
	struct qbman_fle *fle;
	struct qdma_sdd *sdd;

	memset(fd, 0, sizeof(struct qbman_fd));

	if (is_silent) {
		fle_sdd = qdma_vq->cntx_fle_sdd[qdma_vq->silent_idx];
	} else {
		ret = rte_mempool_get(qdma_vq->fle_pool,
			(void **)&fle_sdd);
		if (ret)
			return ret;
		DPAA2_SET_FD_FRC(fd, QDMA_SER_CTX);
	}

	fle = fle_sdd->fle;
	fle_iova = (uint64_t)fle - qdma_vq->fle_iova2va_offset;

	dpaa2_qdma_fd_set_addr(fd, fle_iova);
	DPAA2_SET_FD_COMPOUND_FMT(fd);
	DPAA2_SET_FD_FLC(fd, (uint64_t)fle);

	if (qdma_vq->fle_pre_populate) {
		if (unlikely(!fle[DPAA2_QDMA_SRC_FLE].length)) {
			fle_sdd_pre_populate(fle_sdd,
				&qdma_vq->rbp,
				0, 0, QBMAN_FLE_WORD4_FMT_SBF);
		}

		fle_post_populate(fle, src, dst, length);
	} else {
		sdd = fle_sdd->sdd;
		sdd_iova = (uint64_t)sdd - qdma_vq->fle_iova2va_offset;
		fle_populate(fle, sdd, sdd_iova, &qdma_vq->rbp,
			src, dst, length,
			QBMAN_FLE_WORD4_FMT_SBF);
	}

	if (unlikely(qdma_vq->flags & DPAA2_QDMA_DESC_DEBUG_FLAG))
		dpaa2_qdma_long_fmt_dump(fle);

	dpaa2_qdma_fd_save_att(fd, DPAA2_QDMA_IDX_FROM_FLAG(flags),
		DPAA2_QDMA_FD_LONG);
	qdma_vq->fd_idx++;
	qdma_vq->silent_idx =
		(qdma_vq->silent_idx + 1) & (DPAA2_QDMA_MAX_DESC - 1);

	if (flags & RTE_DMA_OP_FLAG_SUBMIT) {
		expected = qdma_vq->fd_idx;
		ret = dpaa2_qdma_multi_eq(qdma_vq);
		if (likely(ret == expected)) {
			qdma_vq->copy_num++;
			return (qdma_vq->copy_num - 1) & UINT16_MAX;
		}
	} else {
		qdma_vq->copy_num++;
		return (qdma_vq->copy_num - 1) & UINT16_MAX;
	}

	return ret;
}

static int
dpaa2_qdma_copy(void *dev_private, uint16_t vchan,
	rte_iova_t src, rte_iova_t dst,
	uint32_t length, uint64_t flags)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;
	struct qdma_virt_queue *qdma_vq = &qdma_dev->vqs[vchan];

	if (qdma_vq->using_short_fd)
		return dpaa2_qdma_short_copy(qdma_vq, src, dst,
				length, qdma_dev->is_silent, flags);
	else
		return dpaa2_qdma_long_copy(qdma_vq, src, dst,
				length, qdma_dev->is_silent, flags);
}

static inline int
dpaa2_qdma_dq_fd(const struct qbman_fd *fd,
	struct qdma_virt_queue *qdma_vq,
	uint16_t *free_space, uint16_t *fle_elem_nb)
{
	uint16_t idx, att;
	enum dpaa2_qdma_fd_type type;
	int ret;
	struct qdma_cntx_sg *cntx_sg;
	struct qdma_cntx_fle_sdd *fle_sdd;

	att = dpaa2_qdma_fd_get_att(fd);
	type = DPAA2_QDMA_FD_ATT_TYPE(att);
	if (type == DPAA2_QDMA_FD_SHORT) {
		idx = DPAA2_QDMA_FD_ATT_CNTX(att);
		ret = qdma_cntx_idx_ring_eq(qdma_vq->ring_cntx_idx,
				&idx, 1, free_space);
		if (unlikely(ret != 1))
			return -ENOSPC;

		return 0;
	}
	if (type == DPAA2_QDMA_FD_LONG) {
		idx = DPAA2_QDMA_FD_ATT_CNTX(att);
		fle_sdd = (void *)(uintptr_t)DPAA2_GET_FD_FLC(fd);
		qdma_vq->fle_elem[*fle_elem_nb] = fle_sdd;
		(*fle_elem_nb)++;
		ret = qdma_cntx_idx_ring_eq(qdma_vq->ring_cntx_idx,
				&idx, 1, free_space);
		if (unlikely(ret != 1))
			return -ENOSPC;

		return 0;
	}
	if (type == DPAA2_QDMA_FD_SG) {
		fle_sdd = (void *)(uintptr_t)DPAA2_GET_FD_FLC(fd);
		qdma_vq->fle_elem[*fle_elem_nb] = fle_sdd;
		(*fle_elem_nb)++;
		cntx_sg = container_of(fle_sdd,
				struct qdma_cntx_sg, fle_sdd);
		ret = qdma_cntx_idx_ring_eq(qdma_vq->ring_cntx_idx,
				cntx_sg->cntx_idx,
				cntx_sg->job_nb, free_space);
		if (unlikely(ret < cntx_sg->job_nb))
			return -ENOSPC;

		return 0;
	}

	DPAA2_QDMA_ERR("Invalid FD type, ATT=0x%04x",
		fd->simple_ddr.rsv1_att);
	return -EIO;
}

static uint16_t
dpaa2_qdma_dequeue(void *dev_private,
	uint16_t vchan, const uint16_t nb_cpls,
	uint16_t *cntx_idx, bool *has_error)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;
	struct qdma_virt_queue *qdma_vq = &qdma_dev->vqs[vchan];

	struct dpaa2_queue *rxq;
	struct qbman_result *dq_storage, *dq_storage1 = NULL;
	struct qbman_pull_desc pulldesc;
	struct qbman_swp *swp;
	struct queue_storage_info_t *q_storage;
	uint32_t fqid;
	uint8_t status, pending;
	uint8_t num_rx = 0;
	const struct qbman_fd *fd;
	int ret, pull_size;
	uint16_t free_space = 0, fle_elem_nb = 0;

	if (unlikely(qdma_dev->is_silent))
		return 0;

	if (unlikely(!DPAA2_PER_LCORE_DPIO)) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			DPAA2_QDMA_ERR("Allocate portal err, tid(%d)",
				rte_gettid());
			if (has_error)
				*has_error = true;
			return 0;
		}
	}
	swp = DPAA2_PER_LCORE_PORTAL;

	pull_size = (nb_cpls > dpaa2_dqrr_size) ?
		dpaa2_dqrr_size : nb_cpls;
	rxq = &(dpdmai_dev->rx_queue[qdma_vq->vq_id]);
	fqid = rxq->fqid;
	q_storage = rxq->q_storage[0];

	if (unlikely(!q_storage->active_dqs)) {
		q_storage->toggle = 0;
		dq_storage = q_storage->dq_storage[q_storage->toggle];
		q_storage->last_num_pkts = pull_size;
		qbman_pull_desc_clear(&pulldesc);
		qbman_pull_desc_set_numframes(&pulldesc,
			q_storage->last_num_pkts);
		qbman_pull_desc_set_fq(&pulldesc, fqid);
		qbman_pull_desc_set_storage(&pulldesc, dq_storage,
			DPAA2_VADDR_TO_IOVA(dq_storage), 1);
		if (check_swp_active_dqs(DPAA2_PER_LCORE_DPIO->index)) {
			while (!qbman_check_command_complete(
			       get_swp_active_dqs(
			       DPAA2_PER_LCORE_DPIO->index)))
				;
			clear_swp_active_dqs(DPAA2_PER_LCORE_DPIO->index);
		}
		while (1) {
			if (qbman_swp_pull(swp, &pulldesc)) {
				DPAA2_QDMA_DP_WARN("QBMAN busy");
					/* Portal was busy, try again */
				continue;
			}
			break;
		}
		q_storage->active_dqs = dq_storage;
		q_storage->active_dpio_id = DPAA2_PER_LCORE_DPIO->index;
		set_swp_active_dqs(DPAA2_PER_LCORE_DPIO->index,
			dq_storage);
	}

	dq_storage = q_storage->active_dqs;
	rte_prefetch0((void *)(size_t)(dq_storage));
	rte_prefetch0((void *)(size_t)(dq_storage + 1));

	/* Prepare next pull descriptor. This will give space for the
	 * prefething done on DQRR entries
	 */
	q_storage->toggle ^= 1;
	dq_storage1 = q_storage->dq_storage[q_storage->toggle];
	qbman_pull_desc_clear(&pulldesc);
	qbman_pull_desc_set_numframes(&pulldesc, pull_size);
	qbman_pull_desc_set_fq(&pulldesc, fqid);
	qbman_pull_desc_set_storage(&pulldesc, dq_storage1,
		DPAA2_VADDR_TO_IOVA(dq_storage1), 1);

	/* Check if the previous issued command is completed.
	 * Also seems like the SWP is shared between the Ethernet Driver
	 * and the SEC driver.
	 */
	while (!qbman_check_command_complete(dq_storage))
		;
	if (dq_storage == get_swp_active_dqs(q_storage->active_dpio_id))
		clear_swp_active_dqs(q_storage->active_dpio_id);

	pending = 1;

	do {
		/* Loop until the dq_storage is updated with
		 * new token by QBMAN
		 */
		while (!qbman_check_new_result(dq_storage))
			;
		rte_prefetch0((void *)((size_t)(dq_storage + 2)));
		/* Check whether Last Pull command is Expired and
		 * setting Condition for Loop termination
		 */
		if (qbman_result_DQ_is_pull_complete(dq_storage)) {
			pending = 0;
			/* Check for valid frame. */
			status = qbman_result_DQ_flags(dq_storage);
			if (unlikely((status & QBMAN_DQ_STAT_VALIDFRAME) == 0))
				continue;
		}
		fd = qbman_result_DQ_fd(dq_storage);
		ret = dpaa2_qdma_dq_fd(fd, qdma_vq, &free_space, &fle_elem_nb);
		if (ret || free_space < RTE_DPAAX_QDMA_JOB_SUBMIT_MAX)
			pending = 0;

		dq_storage++;
	} while (pending);

	if (check_swp_active_dqs(DPAA2_PER_LCORE_DPIO->index)) {
		while (!qbman_check_command_complete(
		       get_swp_active_dqs(DPAA2_PER_LCORE_DPIO->index)))
			;
		clear_swp_active_dqs(DPAA2_PER_LCORE_DPIO->index);
	}
	/* issue a volatile dequeue command for next pull */
	while (1) {
		if (qbman_swp_pull(swp, &pulldesc)) {
			DPAA2_QDMA_DP_WARN("QBMAN is busy (2)");
			continue;
		}
		break;
	}

	q_storage->active_dqs = dq_storage1;
	q_storage->active_dpio_id = DPAA2_PER_LCORE_DPIO->index;
	set_swp_active_dqs(DPAA2_PER_LCORE_DPIO->index, dq_storage1);

	if (fle_elem_nb > 0) {
		rte_mempool_put_bulk(qdma_vq->fle_pool,
			qdma_vq->fle_elem, fle_elem_nb);
	}

	num_rx = qdma_cntx_idx_ring_dq(qdma_vq->ring_cntx_idx,
		cntx_idx, nb_cpls);

	if (has_error)
		*has_error = false;

	return num_rx;
}

static int
dpaa2_qdma_info_get(const struct rte_dma_dev *dev,
	struct rte_dma_info *dev_info,
	uint32_t info_sz __rte_unused)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev->data->dev_private;

	dev_info->dev_capa = RTE_DMA_CAPA_MEM_TO_MEM |
		RTE_DMA_CAPA_MEM_TO_DEV |
		RTE_DMA_CAPA_DEV_TO_DEV |
		RTE_DMA_CAPA_DEV_TO_MEM |
		RTE_DMA_CAPA_SILENT |
		RTE_DMA_CAPA_OPS_COPY |
		RTE_DMA_CAPA_OPS_COPY_SG;
	dev_info->dev_capa |= RTE_DMA_CAPA_DPAAX_QDMA_FLAGS_INDEX;
	dev_info->max_vchans = dpdmai_dev->num_queues;
	dev_info->max_desc = DPAA2_QDMA_MAX_DESC;
	dev_info->min_desc = DPAA2_QDMA_MIN_DESC;
	dev_info->max_sges = RTE_DPAAX_QDMA_JOB_SUBMIT_MAX;
	dev_info->dev_name = dev->device->name;
	if (dpdmai_dev->qdma_dev)
		dev_info->nb_vchans = dpdmai_dev->qdma_dev->num_vqs;

	return 0;
}

static int
dpaa2_qdma_configure(struct rte_dma_dev *dev,
	const struct rte_dma_conf *dev_conf,
	uint32_t conf_sz)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev->data->dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;
	uint16_t i;
	struct dpdmai_rx_queue_cfg rx_queue_cfg;
	struct dpdmai_rx_queue_attr rx_attr;
	struct dpdmai_tx_queue_attr tx_attr;
	struct dpaa2_queue *rxq;
	int ret = 0;

	DPAA2_QDMA_FUNC_TRACE();

	RTE_SET_USED(conf_sz);

	if (dev_conf->nb_vchans > dpdmai_dev->num_queues) {
		DPAA2_QDMA_ERR("%s config queues(%d) > hw queues(%d)",
			dev->data->dev_name, dev_conf->nb_vchans,
			dpdmai_dev->num_queues);

		return -ENOTSUP;
	}

	if (qdma_dev->vqs) {
		DPAA2_QDMA_DEBUG("%s: queues de-config(%d)/re-config(%d)",
			dev->data->dev_name,
			qdma_dev->num_vqs, dev_conf->nb_vchans);
		for (i = 0; i < qdma_dev->num_vqs; i++) {
			if ((qdma_dev->vqs[i].num_enqueues !=
				qdma_dev->vqs[i].num_dequeues) &&
				!qdma_dev->is_silent) {
				DPAA2_QDMA_ERR("VQ(%d) %"PRIu64" jobs in dma.",
					i, qdma_dev->vqs[i].num_enqueues -
					qdma_dev->vqs[i].num_dequeues);
				return -EBUSY;
			}
		}
		for (i = 0; i < qdma_dev->num_vqs; i++) {
			if (qdma_dev->vqs[i].fle_pool) {
				rte_mempool_free(qdma_dev->vqs[i].fle_pool);
				qdma_dev->vqs[i].fle_pool = NULL;
			}
			if (qdma_dev->vqs[i].ring_cntx_idx) {
				rte_free(qdma_dev->vqs[i].ring_cntx_idx);
				qdma_dev->vqs[i].ring_cntx_idx = NULL;
			}
			rxq = &dpdmai_dev->rx_queue[i];
			dpaa2_queue_storage_free(rxq, 1);
		}
		rte_free(qdma_dev->vqs);
		qdma_dev->vqs = NULL;
		qdma_dev->num_vqs = 0;
	}

	/* Set up Rx Queues */
	for (i = 0; i < dev_conf->nb_vchans; i++) {
		memset(&rx_queue_cfg, 0, sizeof(struct dpdmai_rx_queue_cfg));
		rxq = &dpdmai_dev->rx_queue[i];
		ret = dpdmai_set_rx_queue(&s_proc_mc_reg,
				CMD_PRI_LOW,
				dpdmai_dev->token,
				i, 0, &rx_queue_cfg);
		if (ret) {
			DPAA2_QDMA_ERR("%s RXQ%d set failed(%d)",
				dev->data->dev_name, i, ret);
			return ret;
		}
	}

	/* Get Rx and Tx queues FQID's */
	for (i = 0; i < dev_conf->nb_vchans; i++) {
		ret = dpdmai_get_rx_queue(&s_proc_mc_reg, CMD_PRI_LOW,
				dpdmai_dev->token, i, 0, &rx_attr);
		if (ret) {
			DPAA2_QDMA_ERR("Get DPDMAI%d-RXQ%d failed(%d)",
				dpdmai_dev->dpdmai_id, i, ret);
			return ret;
		}
		dpdmai_dev->rx_queue[i].fqid = rx_attr.fqid;

		ret = dpdmai_get_tx_queue(&s_proc_mc_reg, CMD_PRI_LOW,
				dpdmai_dev->token, i, 0, &tx_attr);
		if (ret) {
			DPAA2_QDMA_ERR("Get DPDMAI%d-TXQ%d failed(%d)",
				dpdmai_dev->dpdmai_id, i, ret);
			return ret;
		}
		dpdmai_dev->tx_queue[i].fqid = tx_attr.fqid;
	}

	/* Allocate Virtual Queues */
	qdma_dev->vqs = rte_zmalloc(NULL,
		(sizeof(struct qdma_virt_queue) * dev_conf->nb_vchans),
		RTE_CACHE_LINE_SIZE);
	if (!qdma_dev->vqs) {
		DPAA2_QDMA_ERR("%s: VQs(%d) alloc failed.",
			dev->data->dev_name, dev_conf->nb_vchans);
		return -ENOMEM;
	}
	for (i = 0; i < dev_conf->nb_vchans; i++) {
		qdma_dev->vqs[i].vq_id = i;
		rxq = &dpdmai_dev->rx_queue[i];
		/* Allocate DQ storage for the DPDMAI Rx queues */
		ret = dpaa2_queue_storage_alloc(rxq, 1);
		if (ret)
			goto alloc_failed;
	}

	qdma_dev->num_vqs = dev_conf->nb_vchans;
	qdma_dev->is_silent = dev_conf->enable_silent;

	return 0;

alloc_failed:
	for (i = 0; i < dev_conf->nb_vchans; i++) {
		rxq = &dpdmai_dev->rx_queue[i];
		dpaa2_queue_storage_free(rxq, 1);
	}

	rte_free(qdma_dev->vqs);
	qdma_dev->vqs = NULL;
	qdma_dev->num_vqs = 0;

	return ret;
}

static int
dpaa2_qdma_vchan_rbp_set(struct qdma_virt_queue *vq,
	const struct rte_dma_vchan_conf *conf)
{
	if (conf->direction == RTE_DMA_DIR_MEM_TO_DEV ||
		conf->direction == RTE_DMA_DIR_DEV_TO_DEV) {
		if (conf->dst_port.port_type != RTE_DMA_PORT_PCIE)
			return -EINVAL;
		vq->rbp.enable = 1;
		vq->rbp.dportid = conf->dst_port.pcie.coreid;
		vq->rbp.dpfid = conf->dst_port.pcie.pfid;
		if (conf->dst_port.pcie.vfen) {
			vq->rbp.dvfa = 1;
			vq->rbp.dvfid = conf->dst_port.pcie.vfid;
		}
		vq->rbp.drbp = 1;
	}
	if (conf->direction == RTE_DMA_DIR_DEV_TO_MEM ||
		conf->direction == RTE_DMA_DIR_DEV_TO_DEV) {
		if (conf->src_port.port_type != RTE_DMA_PORT_PCIE)
			return -EINVAL;
		vq->rbp.enable = 1;
		vq->rbp.sportid = conf->src_port.pcie.coreid;
		vq->rbp.spfid = conf->src_port.pcie.pfid;
		if (conf->src_port.pcie.vfen) {
			vq->rbp.svfa = 1;
			vq->rbp.dvfid = conf->src_port.pcie.vfid;
		}
		vq->rbp.srbp = 1;
	}

	return 0;
}

static int
dpaa2_qdma_vchan_setup(struct rte_dma_dev *dev, uint16_t vchan,
	const struct rte_dma_vchan_conf *conf,
	uint32_t conf_sz)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev->data->dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;
	uint32_t pool_size;
	char pool_name[64];
	int ret;
	uint64_t iova, va;

	DPAA2_QDMA_FUNC_TRACE();

	RTE_SET_USED(conf_sz);

	ret = dpaa2_qdma_vchan_rbp_set(&qdma_dev->vqs[vchan], conf);
	if (ret)
		return ret;

	if (dpaa2_qdma_get_devargs(dev->device->devargs, DPAA2_QDMA_FLE_PRE_POPULATE))
		qdma_dev->vqs[vchan].fle_pre_populate = 1;
	else
		qdma_dev->vqs[vchan].fle_pre_populate = 0;

	if (dpaa2_qdma_get_devargs(dev->device->devargs, DPAA2_QDMA_DESC_DEBUG))
		qdma_dev->vqs[vchan].flags |= DPAA2_QDMA_DESC_DEBUG_FLAG;
	else
		qdma_dev->vqs[vchan].flags &= (~DPAA2_QDMA_DESC_DEBUG_FLAG);

	if (dpaa2_qdma_get_devargs(dev->device->devargs, DPAA2_QDMA_USING_SHORT_FD))
		qdma_dev->vqs[vchan].using_short_fd = 1;
	else
		qdma_dev->vqs[vchan].using_short_fd = 0;

	snprintf(pool_name, sizeof(pool_name),
		"qdma_fle_pool_dev%d_qid%d", dpdmai_dev->dpdmai_id, vchan);
	pool_size = sizeof(struct qdma_cntx_sg);
	qdma_dev->vqs[vchan].fle_pool = rte_mempool_create(pool_name,
			DPAA2_QDMA_MAX_DESC * 2, pool_size,
			512, 0, NULL, NULL, NULL, NULL,
			SOCKET_ID_ANY, 0);
	if (!qdma_dev->vqs[vchan].fle_pool) {
		DPAA2_QDMA_ERR("%s create failed", pool_name);
		return -ENOMEM;
	}
	iova = qdma_dev->vqs[vchan].fle_pool->mz->iova;
	va = qdma_dev->vqs[vchan].fle_pool->mz->addr_64;
	qdma_dev->vqs[vchan].fle_iova2va_offset = va - iova;

	if (qdma_dev->is_silent) {
		ret = rte_mempool_get_bulk(qdma_dev->vqs[vchan].fle_pool,
				(void **)qdma_dev->vqs[vchan].cntx_sg,
				DPAA2_QDMA_MAX_DESC);
		if (ret) {
			DPAA2_QDMA_ERR("sg cntx get from %s for silent mode",
				       pool_name);
			return ret;
		}
		ret = rte_mempool_get_bulk(qdma_dev->vqs[vchan].fle_pool,
				(void **)qdma_dev->vqs[vchan].cntx_fle_sdd,
				DPAA2_QDMA_MAX_DESC);
		if (ret) {
			DPAA2_QDMA_ERR("long cntx get from %s for silent mode",
				       pool_name);
			return ret;
		}
	} else {
		qdma_dev->vqs[vchan].ring_cntx_idx = rte_malloc(NULL,
				sizeof(struct qdma_cntx_idx_ring),
				RTE_CACHE_LINE_SIZE);
		if (!qdma_dev->vqs[vchan].ring_cntx_idx) {
			DPAA2_QDMA_ERR("DQ response ring alloc failed.");
			return -ENOMEM;
		}
		qdma_dev->vqs[vchan].ring_cntx_idx->start = 0;
		qdma_dev->vqs[vchan].ring_cntx_idx->tail = 0;
		qdma_dev->vqs[vchan].ring_cntx_idx->free_space =
				QDMA_CNTX_IDX_RING_MAX_FREE;
		qdma_dev->vqs[vchan].ring_cntx_idx->nb_in_ring = 0;
		qdma_dev->vqs[vchan].fle_elem = rte_malloc(NULL,
				sizeof(void *) * DPAA2_QDMA_MAX_DESC,
				RTE_CACHE_LINE_SIZE);
	}

	qdma_dev->vqs[vchan].dpdmai_dev = dpdmai_dev;
	qdma_dev->vqs[vchan].nb_desc = conf->nb_desc;

	return 0;
}

static int
dpaa2_qdma_start(struct rte_dma_dev *dev)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev->data->dev_private;
	int ret;

	DPAA2_QDMA_FUNC_TRACE();

	/* Enable the device */
	ret = dpdmai_enable(&s_proc_mc_reg, CMD_PRI_LOW,
			dpdmai_dev->token);
	if (ret) {
		DPAA2_QDMA_ERR("Enabling device failed with err: %d", ret);
		return ret;
	}

	return 0;
}

static int
dpaa2_qdma_stop(struct rte_dma_dev *dev)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev->data->dev_private;
	int ret;

	DPAA2_QDMA_FUNC_TRACE();

	/* Disable the device */
	ret = dpdmai_disable(&s_proc_mc_reg, CMD_PRI_LOW,
			dpdmai_dev->token);
	if (ret) {
		DPAA2_QDMA_ERR("Disable device failed with err: %d", ret);
		return ret;
	}

	return 0;
}

static int
dpaa2_qdma_close(struct rte_dma_dev *dev)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev->data->dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;
	struct dpaa2_queue *rxq;
	int i;

	DPAA2_QDMA_FUNC_TRACE();

	if (!qdma_dev)
		return 0;

	/* In case there are pending jobs on any VQ, return -EBUSY */
	for (i = 0; i < qdma_dev->num_vqs; i++) {
		if ((qdma_dev->vqs[i].num_enqueues !=
		    qdma_dev->vqs[i].num_dequeues) &&
		    !qdma_dev->is_silent) {
			DPAA2_QDMA_ERR("VQ(%d) pending: eq(%"PRIu64") != dq(%"PRId64")",
				i, qdma_dev->vqs[i].num_enqueues,
				qdma_dev->vqs[i].num_dequeues);
			return -EBUSY;
		}
	}

	/* Free RXQ storages */
	for (i = 0; i < qdma_dev->num_vqs; i++) {
		rxq = &dpdmai_dev->rx_queue[i];
		dpaa2_queue_storage_free(rxq, 1);
	}

	if (qdma_dev->vqs) {
		/* Free RXQ fle pool */
		for (i = 0; i < qdma_dev->num_vqs; i++) {
			if (qdma_dev->vqs[i].fle_pool) {
				rte_mempool_free(qdma_dev->vqs[i].fle_pool);
				qdma_dev->vqs[i].fle_pool = NULL;
			}
			if (qdma_dev->vqs[i].ring_cntx_idx) {
				rte_free(qdma_dev->vqs[i].ring_cntx_idx);
				qdma_dev->vqs[i].ring_cntx_idx = NULL;
			}
		}
		rte_free(qdma_dev->vqs);
		qdma_dev->vqs = NULL;
	}

	/* Reset QDMA device structure */
	qdma_dev->num_vqs = 0;

	return 0;
}

static int
dpaa2_qdma_stats_get(const struct rte_dma_dev *dmadev,
	uint16_t vchan, struct rte_dma_stats *rte_stats, uint32_t size)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dmadev->data->dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;
	struct qdma_virt_queue *qdma_vq = &qdma_dev->vqs[vchan];
	struct rte_dma_stats *stats = &qdma_vq->stats;

	RTE_SET_USED(size);

	/* TODO - directly use stats */
	stats->submitted = qdma_vq->num_enqueues;
	stats->completed = qdma_vq->num_dequeues;
	*rte_stats = *stats;

	return 0;
}

static int
dpaa2_qdma_stats_reset(struct rte_dma_dev *dmadev, uint16_t vchan)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dmadev->data->dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;
	struct qdma_virt_queue *qdma_vq = &qdma_dev->vqs[vchan];

	qdma_vq->num_enqueues = 0;
	qdma_vq->num_dequeues = 0;

	return 0;
}

static uint16_t
dpaa2_qdma_burst_capacity(const void *dev_private, uint16_t vchan)
{
	const struct dpaa2_dpdmai_dev *dpdmai_dev = dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;
	struct qdma_virt_queue *qdma_vq = &qdma_dev->vqs[vchan];

	return qdma_vq->nb_desc - qdma_vq->num_valid_jobs;
}

static struct rte_dma_dev_ops dpaa2_qdma_ops = {
	.dev_info_get     = dpaa2_qdma_info_get,
	.dev_configure    = dpaa2_qdma_configure,
	.dev_start        = dpaa2_qdma_start,
	.dev_stop         = dpaa2_qdma_stop,
	.dev_close        = dpaa2_qdma_close,
	.vchan_setup      = dpaa2_qdma_vchan_setup,
	.stats_get        = dpaa2_qdma_stats_get,
	.stats_reset      = dpaa2_qdma_stats_reset,
};

static int
dpaa2_dpdmai_dev_uninit(struct rte_dma_dev *dev)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev->data->dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;
	int ret;

	DPAA2_QDMA_FUNC_TRACE();

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		DPAA2_QDMA_DEBUG("Un-attach DMA(%d) in the 2nd proess.",
			dpdmai_dev->dpdmai_id);
		return 0;
	}

	/* Close the device at underlying layer*/
	ret = dpdmai_close(&s_proc_mc_reg, CMD_PRI_LOW,
			dpdmai_dev->token);
	if (ret) {
		DPAA2_QDMA_ERR("dpdmai(%d) close failed(%d)",
			dpdmai_dev->dpdmai_id, ret);

		return ret;
	}

	if (qdma_dev) {
		rte_free(qdma_dev);
		dpdmai_dev->qdma_dev = NULL;
	}

	return ret;
}

static int
dpaa2_dpdmai_dev_init(struct rte_dma_dev *dev, uint32_t dpdmai_id)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev->data->dev_private;
	struct dpdmai_attr attr;
	int ret, err;

	DPAA2_QDMA_FUNC_TRACE();

	if (!dpaa2_coherent_no_alloc_cache) {
		if (dpaa2_svr_family == SVR_LX2160A) {
			dpaa2_coherent_no_alloc_cache =
				DPAA2_LX2_COHERENT_NO_ALLOCATE_CACHE;
			dpaa2_coherent_alloc_cache =
				DPAA2_LX2_COHERENT_ALLOCATE_CACHE;
		} else {
			dpaa2_coherent_no_alloc_cache =
				DPAA2_COHERENT_NO_ALLOCATE_CACHE;
			dpaa2_coherent_alloc_cache =
				DPAA2_COHERENT_ALLOCATE_CACHE;
		}
	}

	if (!s_proc_mc_reg.regs)
		s_proc_mc_reg.regs = dpaa2_get_mcp_ptr(MC_PORTAL_INDEX);

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		DPAA2_QDMA_DEBUG("Attach DMA(%d) in the 2nd proess.",
			dpdmai_id);
		if (dpdmai_id != dpdmai_dev->dpdmai_id) {
			DPAA2_QDMA_ERR("Fatal: Attach DMA(%d) to DMA(%d)",
				dpdmai_id, dpdmai_dev->dpdmai_id);
			return -EINVAL;
		}
		if (!dpdmai_dev->qdma_dev) {
			DPAA2_QDMA_ERR("Fatal: DMA(%d) qdma_dev NOT allocated",
				dpdmai_id);
			return -ENOMEM;
		}
		if (dpdmai_dev->qdma_dev->num_vqs) {
			DPAA2_QDMA_WARN("DMA(%d) %d vqs were configured",
				dpdmai_id, dpdmai_dev->qdma_dev->num_vqs);
		}

		return 0;
	}

	/* Open DPDMAI device */
	dpdmai_dev->dpdmai_id = dpdmai_id;

	if (dpdmai_dev->qdma_dev) {
		rte_free(dpdmai_dev->qdma_dev);
		dpdmai_dev->qdma_dev = NULL;
	}
	dpdmai_dev->qdma_dev = rte_zmalloc(NULL,
		sizeof(struct qdma_device), RTE_CACHE_LINE_SIZE);
	if (!dpdmai_dev->qdma_dev) {
		DPAA2_QDMA_ERR("DMA(%d) alloc memory failed",
			dpdmai_id);
		return -ENOMEM;
	}
	ret = dpdmai_open(&s_proc_mc_reg, CMD_PRI_LOW,
			dpdmai_dev->dpdmai_id, &dpdmai_dev->token);
	if (ret) {
		DPAA2_QDMA_ERR("%s: dma(%d) open failed(%d)",
			__func__, dpdmai_dev->dpdmai_id, ret);
		return ret;
	}

	/* Get DPDMAI attributes */
	ret = dpdmai_get_attributes(&s_proc_mc_reg, CMD_PRI_LOW,
			dpdmai_dev->token, &attr);
	if (ret) {
		DPAA2_QDMA_ERR("%s: dma(%d) get attributes failed(%d)",
			__func__, dpdmai_dev->dpdmai_id, ret);
		err = dpdmai_close(&s_proc_mc_reg, CMD_PRI_LOW,
			dpdmai_dev->token);
		if (err) {
			DPAA2_QDMA_ERR("dpdmai(%d) close failed(%d)",
				dpdmai_dev->dpdmai_id, err);
		}
		return ret;
	}
	dpdmai_dev->num_queues = attr.num_of_queues;

	DPAA2_QDMA_DEBUG("DMA(%d) is initialized.", dpdmai_id);

	return 0;
}

static int
dpaa2_qdma_probe(struct rte_dpaa2_driver *dpaa2_drv,
	struct rte_dpaa2_device *dpaa2_dev)
{
	struct rte_dma_dev *dmadev;
	int ret;

	DPAA2_QDMA_FUNC_TRACE();

	RTE_SET_USED(dpaa2_drv);

	dmadev = rte_dma_pmd_allocate(dpaa2_dev->device.name,
		rte_socket_id(),
		sizeof(struct dpaa2_dpdmai_dev));
	if (!dmadev) {
		DPAA2_QDMA_ERR("Unable to allocate dmadevice");
		return -EINVAL;
	}

	dpaa2_dev->dmadev = dmadev;
	dmadev->dev_ops = &dpaa2_qdma_ops;
	dmadev->device = &dpaa2_dev->device;
	dmadev->fp_obj->dev_private = dmadev->data->dev_private;
	dmadev->fp_obj->copy = dpaa2_qdma_copy;
	dmadev->fp_obj->copy_sg = dpaa2_qdma_copy_sg;
	dmadev->fp_obj->submit = dpaa2_qdma_submit;
	dmadev->fp_obj->completed = dpaa2_qdma_dequeue;
	dmadev->fp_obj->burst_capacity = dpaa2_qdma_burst_capacity;

	/* Invoke PMD device initialization function */
	ret = dpaa2_dpdmai_dev_init(dmadev, dpaa2_dev->object_id);
	if (ret) {
		rte_dma_pmd_release(dpaa2_dev->device.name);
		return ret;
	}

	dmadev->state = RTE_DMA_DEV_READY;
	return 0;
}

static int
dpaa2_qdma_remove(struct rte_dpaa2_device *dpaa2_dev)
{
	struct rte_dma_dev *dmadev = dpaa2_dev->dmadev;
	int ret;

	DPAA2_QDMA_FUNC_TRACE();

	dpaa2_dpdmai_dev_uninit(dmadev);

	ret = rte_dma_pmd_release(dpaa2_dev->device.name);
	if (ret)
		DPAA2_QDMA_ERR("Device cleanup failed");

	return 0;
}

static struct rte_dpaa2_driver rte_dpaa2_qdma_pmd;

static struct rte_dpaa2_driver rte_dpaa2_qdma_pmd = {
	.drv_flags = RTE_DPAA2_DRV_IOVA_AS_VA,
	.drv_type = DPAA2_QDMA,
	.probe = dpaa2_qdma_probe,
	.remove = dpaa2_qdma_remove,
};

RTE_PMD_REGISTER_DPAA2(dpaa2_qdma, rte_dpaa2_qdma_pmd);
RTE_PMD_REGISTER_PARAM_STRING(dpaa2_qdma,
	DPAA2_QDMA_FLE_PRE_POPULATE "=<int>"
	DPAA2_QDMA_DESC_DEBUG"=<int>"
	DPAA2_QDMA_USING_SHORT_FD"=<int>");
RTE_LOG_REGISTER_DEFAULT(dpaa2_qdma_logtype, INFO);
