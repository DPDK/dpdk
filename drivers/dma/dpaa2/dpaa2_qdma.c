/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2022 NXP
 */

#include <rte_eal.h>
#include <bus_fslmc_driver.h>
#include <rte_dmadev.h>
#include <rte_dmadev_pmd.h>
#include <rte_kvargs.h>

#include <mc/fsl_dpdmai.h>

#include "rte_pmd_dpaa2_qdma.h"
#include "dpaa2_qdma.h"
#include "dpaa2_qdma_logs.h"

#define DPAA2_QDMA_PREFETCH "prefetch"

uint32_t dpaa2_coherent_no_alloc_cache;
uint32_t dpaa2_coherent_alloc_cache;

static inline int
qdma_populate_fd_pci(phys_addr_t src, phys_addr_t dest,
		     uint32_t len, struct qbman_fd *fd,
		     struct rte_dpaa2_qdma_rbp *rbp, int ser)
{
	fd->simple_pci.saddr_lo = lower_32_bits((uint64_t) (src));
	fd->simple_pci.saddr_hi = upper_32_bits((uint64_t) (src));

	fd->simple_pci.len_sl = len;

	fd->simple_pci.bmt = 1;
	fd->simple_pci.fmt = 3;
	fd->simple_pci.sl = 1;
	fd->simple_pci.ser = ser;

	fd->simple_pci.sportid = rbp->sportid;	/*pcie 3 */
	fd->simple_pci.srbp = rbp->srbp;
	if (rbp->srbp)
		fd->simple_pci.rdttype = 0;
	else
		fd->simple_pci.rdttype = dpaa2_coherent_alloc_cache;

	/*dest is pcie memory */
	fd->simple_pci.dportid = rbp->dportid;	/*pcie 3 */
	fd->simple_pci.drbp = rbp->drbp;
	if (rbp->drbp)
		fd->simple_pci.wrttype = 0;
	else
		fd->simple_pci.wrttype = dpaa2_coherent_no_alloc_cache;

	fd->simple_pci.daddr_lo = lower_32_bits((uint64_t) (dest));
	fd->simple_pci.daddr_hi = upper_32_bits((uint64_t) (dest));

	return 0;
}

static inline int
qdma_populate_fd_ddr(phys_addr_t src, phys_addr_t dest,
		     uint32_t len, struct qbman_fd *fd, int ser)
{
	fd->simple_ddr.saddr_lo = lower_32_bits((uint64_t) (src));
	fd->simple_ddr.saddr_hi = upper_32_bits((uint64_t) (src));

	fd->simple_ddr.len = len;

	fd->simple_ddr.bmt = 1;
	fd->simple_ddr.fmt = 3;
	fd->simple_ddr.sl = 1;
	fd->simple_ddr.ser = ser;
	/**
	 * src If RBP=0 {NS,RDTTYPE[3:0]}: 0_1011
	 * Coherent copy of cacheable memory,
	* lookup in downstream cache, no allocate
	 * on miss
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

	fd->simple_ddr.daddr_lo = lower_32_bits((uint64_t) (dest));
	fd->simple_ddr.daddr_hi = upper_32_bits((uint64_t) (dest));

	return 0;
}

static void
dpaa2_qdma_populate_fle(struct qbman_fle *fle,
			uint64_t fle_iova,
			struct rte_dpaa2_qdma_rbp *rbp,
			uint64_t src, uint64_t dest,
			size_t len, uint32_t flags, uint32_t fmt)
{
	struct qdma_sdd *sdd;
	uint64_t sdd_iova;

	sdd = (struct qdma_sdd *)
			((uintptr_t)(uint64_t)fle - QDMA_FLE_FLE_OFFSET +
			QDMA_FLE_SDD_OFFSET);
	sdd_iova = fle_iova - QDMA_FLE_FLE_OFFSET + QDMA_FLE_SDD_OFFSET;

	/* first frame list to source descriptor */
	DPAA2_SET_FLE_ADDR(fle, sdd_iova);
	DPAA2_SET_FLE_LEN(fle, (2 * (sizeof(struct qdma_sdd))));

	/* source and destination descriptor */
	if (rbp && rbp->enable) {
		/* source */
		sdd->read_cmd.portid = rbp->sportid;
		sdd->rbpcmd_simple.pfid = rbp->spfid;
		sdd->rbpcmd_simple.vfa = rbp->vfa;
		sdd->rbpcmd_simple.vfid = rbp->svfid;

		if (rbp->srbp) {
			sdd->read_cmd.rbp = rbp->srbp;
			sdd->read_cmd.rdtype = DPAA2_RBP_MEM_RW;
		} else {
			sdd->read_cmd.rdtype = dpaa2_coherent_no_alloc_cache;
		}
		sdd++;
		/* destination */
		sdd->write_cmd.portid = rbp->dportid;
		sdd->rbpcmd_simple.pfid = rbp->dpfid;
		sdd->rbpcmd_simple.vfa = rbp->vfa;
		sdd->rbpcmd_simple.vfid = rbp->dvfid;

		if (rbp->drbp) {
			sdd->write_cmd.rbp = rbp->drbp;
			sdd->write_cmd.wrttype = DPAA2_RBP_MEM_RW;
		} else {
			sdd->write_cmd.wrttype = dpaa2_coherent_alloc_cache;
		}

	} else {
		sdd->read_cmd.rdtype = dpaa2_coherent_no_alloc_cache;
		sdd++;
		sdd->write_cmd.wrttype = dpaa2_coherent_alloc_cache;
	}
	fle++;
	/* source frame list to source buffer */
	if (flags & RTE_DPAA2_QDMA_JOB_SRC_PHY) {
		DPAA2_SET_FLE_ADDR(fle, src);
#ifdef RTE_LIBRTE_DPAA2_USE_PHYS_IOVA
		DPAA2_SET_FLE_BMT(fle);
#endif
	} else {
		DPAA2_SET_FLE_ADDR(fle, DPAA2_VADDR_TO_IOVA(src));
	}
	fle->word4.fmt = fmt;
	DPAA2_SET_FLE_LEN(fle, len);

	fle++;
	/* destination frame list to destination buffer */
	if (flags & RTE_DPAA2_QDMA_JOB_DEST_PHY) {
#ifdef RTE_LIBRTE_DPAA2_USE_PHYS_IOVA
		DPAA2_SET_FLE_BMT(fle);
#endif
		DPAA2_SET_FLE_ADDR(fle, dest);
	} else {
		DPAA2_SET_FLE_ADDR(fle, DPAA2_VADDR_TO_IOVA(dest));
	}
	fle->word4.fmt = fmt;
	DPAA2_SET_FLE_LEN(fle, len);

	/* Final bit: 1, for last frame list */
	DPAA2_SET_FLE_FIN(fle);
}

static inline int
dpdmai_dev_set_fd_us(struct qdma_virt_queue *qdma_vq,
		     struct qbman_fd *fd,
		     struct rte_dpaa2_qdma_job **job,
		     uint16_t nb_jobs)
{
	struct rte_dpaa2_qdma_rbp *rbp = &qdma_vq->rbp;
	struct rte_dpaa2_qdma_job **ppjob;
	size_t iova;
	int ret = 0, loop;
	int ser = (qdma_vq->flags & DPAA2_QDMA_VQ_NO_RESPONSE) ?
				0 : 1;

	for (loop = 0; loop < nb_jobs; loop++) {
		if (job[loop]->src & QDMA_RBP_UPPER_ADDRESS_MASK)
			iova = (size_t)job[loop]->dest;
		else
			iova = (size_t)job[loop]->src;

		/* Set the metadata */
		job[loop]->vq_id = qdma_vq->vq_id;
		ppjob = (struct rte_dpaa2_qdma_job **)DPAA2_IOVA_TO_VADDR(iova) - 1;
		*ppjob = job[loop];

		if ((rbp->drbp == 1) || (rbp->srbp == 1))
			ret = qdma_populate_fd_pci((phys_addr_t)job[loop]->src,
					(phys_addr_t)job[loop]->dest,
					job[loop]->len, &fd[loop], rbp, ser);
		else
			ret = qdma_populate_fd_ddr((phys_addr_t)job[loop]->src,
					(phys_addr_t)job[loop]->dest,
					job[loop]->len, &fd[loop], ser);
	}

	return ret;
}

static uint32_t
qdma_populate_sg_entry(struct rte_dpaa2_qdma_job **jobs,
		       struct qdma_sg_entry *src_sge,
		       struct qdma_sg_entry *dst_sge,
		       uint16_t nb_jobs)
{
	uint16_t i;
	uint32_t total_len = 0;
	uint64_t iova;

	for (i = 0; i < nb_jobs; i++) {
		/* source SG */
		if (likely(jobs[i]->flags & RTE_DPAA2_QDMA_JOB_SRC_PHY)) {
			src_sge->addr_lo = (uint32_t)jobs[i]->src;
			src_sge->addr_hi = (jobs[i]->src >> 32);
		} else {
			iova = DPAA2_VADDR_TO_IOVA(jobs[i]->src);
			src_sge->addr_lo = (uint32_t)iova;
			src_sge->addr_hi = iova >> 32;
		}
		src_sge->data_len.data_len_sl0 = jobs[i]->len;
		src_sge->ctrl.sl = QDMA_SG_SL_LONG;
		src_sge->ctrl.fmt = QDMA_SG_FMT_SDB;
#ifdef RTE_LIBRTE_DPAA2_USE_PHYS_IOVA
		src_sge->ctrl.bmt = QDMA_SG_BMT_ENABLE;
#else
		src_sge->ctrl.bmt = QDMA_SG_BMT_DISABLE;
#endif
		/* destination SG */
		if (likely(jobs[i]->flags & RTE_DPAA2_QDMA_JOB_DEST_PHY)) {
			dst_sge->addr_lo = (uint32_t)jobs[i]->dest;
			dst_sge->addr_hi = (jobs[i]->dest >> 32);
		} else {
			iova = DPAA2_VADDR_TO_IOVA(jobs[i]->dest);
			dst_sge->addr_lo = (uint32_t)iova;
			dst_sge->addr_hi = iova >> 32;
		}
		dst_sge->data_len.data_len_sl0 = jobs[i]->len;
		dst_sge->ctrl.sl = QDMA_SG_SL_LONG;
		dst_sge->ctrl.fmt = QDMA_SG_FMT_SDB;
#ifdef RTE_LIBRTE_DPAA2_USE_PHYS_IOVA
		dst_sge->ctrl.bmt = QDMA_SG_BMT_ENABLE;
#else
		dst_sge->ctrl.bmt = QDMA_SG_BMT_DISABLE;
#endif
		total_len += jobs[i]->len;

		if (i == (nb_jobs - 1)) {
			src_sge->ctrl.f = QDMA_SG_F;
			dst_sge->ctrl.f = QDMA_SG_F;
		} else {
			src_sge->ctrl.f = 0;
			dst_sge->ctrl.f = 0;
		}
		src_sge++;
		dst_sge++;
	}

	return total_len;
}

static inline int
dpdmai_dev_set_multi_fd_lf_no_rsp(struct qdma_virt_queue *qdma_vq,
				  struct qbman_fd *fd,
				  struct rte_dpaa2_qdma_job **job,
				  uint16_t nb_jobs)
{
	struct rte_dpaa2_qdma_rbp *rbp = &qdma_vq->rbp;
	struct rte_dpaa2_qdma_job **ppjob;
	uint16_t i;
	void *elem;
	struct qbman_fle *fle;
	uint64_t elem_iova, fle_iova;

	for (i = 0; i < nb_jobs; i++) {
		elem = job[i]->usr_elem;
#ifdef RTE_LIBRTE_DPAA2_USE_PHYS_IOVA
		elem_iova = rte_mempool_virt2iova(elem);
#else
		elem_iova = DPAA2_VADDR_TO_IOVA(elem);
#endif

		ppjob = (struct rte_dpaa2_qdma_job **)
			((uintptr_t)(uint64_t)elem +
			 QDMA_FLE_SINGLE_JOB_OFFSET);
		*ppjob = job[i];

		job[i]->vq_id = qdma_vq->vq_id;

		fle = (struct qbman_fle *)
			((uintptr_t)(uint64_t)elem + QDMA_FLE_FLE_OFFSET);
		fle_iova = elem_iova + QDMA_FLE_FLE_OFFSET;

		DPAA2_SET_FD_ADDR(&fd[i], fle_iova);
		DPAA2_SET_FD_COMPOUND_FMT(&fd[i]);

		memset(fle, 0, DPAA2_QDMA_MAX_FLE * sizeof(struct qbman_fle) +
				DPAA2_QDMA_MAX_SDD * sizeof(struct qdma_sdd));

		dpaa2_qdma_populate_fle(fle, fle_iova, rbp,
			job[i]->src, job[i]->dest, job[i]->len,
			job[i]->flags, QBMAN_FLE_WORD4_FMT_SBF);
	}

	return 0;
}

static inline int
dpdmai_dev_set_multi_fd_lf(struct qdma_virt_queue *qdma_vq,
			   struct qbman_fd *fd,
			   struct rte_dpaa2_qdma_job **job,
			   uint16_t nb_jobs)
{
	struct rte_dpaa2_qdma_rbp *rbp = &qdma_vq->rbp;
	struct rte_dpaa2_qdma_job **ppjob;
	uint16_t i;
	int ret;
	void *elem[DPAA2_QDMA_MAX_DESC];
	struct qbman_fle *fle;
	uint64_t elem_iova, fle_iova;

	ret = rte_mempool_get_bulk(qdma_vq->fle_pool, elem, nb_jobs);
	if (ret) {
		DPAA2_QDMA_DP_DEBUG("Memory alloc failed for FLE");
		return ret;
	}

	for (i = 0; i < nb_jobs; i++) {
#ifdef RTE_LIBRTE_DPAA2_USE_PHYS_IOVA
		elem_iova = rte_mempool_virt2iova(elem[i]);
#else
		elem_iova = DPAA2_VADDR_TO_IOVA(elem[i]);
#endif

		ppjob = (struct rte_dpaa2_qdma_job **)
			((uintptr_t)(uint64_t)elem[i] +
			 QDMA_FLE_SINGLE_JOB_OFFSET);
		*ppjob = job[i];

		job[i]->vq_id = qdma_vq->vq_id;

		fle = (struct qbman_fle *)
			((uintptr_t)(uint64_t)elem[i] + QDMA_FLE_FLE_OFFSET);
		fle_iova = elem_iova + QDMA_FLE_FLE_OFFSET;

		DPAA2_SET_FD_ADDR(&fd[i], fle_iova);
		DPAA2_SET_FD_COMPOUND_FMT(&fd[i]);
		DPAA2_SET_FD_FRC(&fd[i], QDMA_SER_CTX);

		memset(fle, 0, DPAA2_QDMA_MAX_FLE * sizeof(struct qbman_fle) +
			DPAA2_QDMA_MAX_SDD * sizeof(struct qdma_sdd));

		dpaa2_qdma_populate_fle(fle, fle_iova, rbp,
				job[i]->src, job[i]->dest, job[i]->len,
				job[i]->flags, QBMAN_FLE_WORD4_FMT_SBF);
	}

	return 0;
}

static inline int
dpdmai_dev_set_sg_fd_lf(struct qdma_virt_queue *qdma_vq,
			struct qbman_fd *fd,
			struct rte_dpaa2_qdma_job **job,
			uint16_t nb_jobs)
{
	struct rte_dpaa2_qdma_rbp *rbp = &qdma_vq->rbp;
	struct rte_dpaa2_qdma_job **ppjob;
	void *elem;
	struct qbman_fle *fle;
	uint64_t elem_iova, fle_iova, src, dst;
	int ret = 0, i;
	struct qdma_sg_entry *src_sge, *dst_sge;
	uint32_t len, fmt, flags;

	/*
	 * Get an FLE/SDD from FLE pool.
	 * Note: IO metadata is before the FLE and SDD memory.
	 */
	if (qdma_vq->flags & DPAA2_QDMA_VQ_NO_RESPONSE) {
		elem = job[0]->usr_elem;
	} else {
		ret = rte_mempool_get(qdma_vq->fle_pool, &elem);
		if (ret) {
			DPAA2_QDMA_DP_DEBUG("Memory alloc failed for FLE");
			return ret;
		}
	}

#ifdef RTE_LIBRTE_DPAA2_USE_PHYS_IOVA
	elem_iova = rte_mempool_virt2iova(elem);
#else
	elem_iova = DPAA2_VADDR_TO_IOVA(elem);
#endif

	/* Set the metadata */
	/* Save job context. */
	*((uint16_t *)
	((uintptr_t)(uint64_t)elem + QDMA_FLE_JOB_NB_OFFSET)) = nb_jobs;
	ppjob = (struct rte_dpaa2_qdma_job **)
		((uintptr_t)(uint64_t)elem + QDMA_FLE_SG_JOBS_OFFSET);
	for (i = 0; i < nb_jobs; i++)
		ppjob[i] = job[i];

	ppjob[0]->vq_id = qdma_vq->vq_id;

	fle = (struct qbman_fle *)
		((uintptr_t)(uint64_t)elem + QDMA_FLE_FLE_OFFSET);
	fle_iova = elem_iova + QDMA_FLE_FLE_OFFSET;

	DPAA2_SET_FD_ADDR(fd, fle_iova);
	DPAA2_SET_FD_COMPOUND_FMT(fd);
	if (!(qdma_vq->flags & DPAA2_QDMA_VQ_NO_RESPONSE))
		DPAA2_SET_FD_FRC(fd, QDMA_SER_CTX);

	/* Populate FLE */
	if (likely(nb_jobs > 1)) {
		src_sge = (struct qdma_sg_entry *)
			((uintptr_t)(uint64_t)elem + QDMA_FLE_SG_ENTRY_OFFSET);
		dst_sge = src_sge + DPAA2_QDMA_MAX_SG_NB;
		src = elem_iova + QDMA_FLE_SG_ENTRY_OFFSET;
		dst = src +
			DPAA2_QDMA_MAX_SG_NB * sizeof(struct qdma_sg_entry);
		len = qdma_populate_sg_entry(job, src_sge, dst_sge, nb_jobs);
		fmt = QBMAN_FLE_WORD4_FMT_SGE;
		flags = RTE_DPAA2_QDMA_JOB_SRC_PHY | RTE_DPAA2_QDMA_JOB_DEST_PHY;
	} else {
		src = job[0]->src;
		dst = job[0]->dest;
		len = job[0]->len;
		fmt = QBMAN_FLE_WORD4_FMT_SBF;
		flags = job[0]->flags;
	}

	memset(fle, 0, DPAA2_QDMA_MAX_FLE * sizeof(struct qbman_fle) +
			DPAA2_QDMA_MAX_SDD * sizeof(struct qdma_sdd));

	dpaa2_qdma_populate_fle(fle, fle_iova, rbp,
					src, dst, len, flags, fmt);

	return 0;
}

static inline uint16_t
dpdmai_dev_get_job_us(struct qdma_virt_queue *qdma_vq __rte_unused,
		      const struct qbman_fd *fd,
		      struct rte_dpaa2_qdma_job **job, uint16_t *nb_jobs)
{
	uint16_t vqid;
	size_t iova;
	struct rte_dpaa2_qdma_job **ppjob;

	if (fd->simple_pci.saddr_hi & (QDMA_RBP_UPPER_ADDRESS_MASK >> 32))
		iova = (size_t)(((uint64_t)fd->simple_pci.daddr_hi) << 32
				| (uint64_t)fd->simple_pci.daddr_lo);
	else
		iova = (size_t)(((uint64_t)fd->simple_pci.saddr_hi) << 32
				| (uint64_t)fd->simple_pci.saddr_lo);

	ppjob = (struct rte_dpaa2_qdma_job **)DPAA2_IOVA_TO_VADDR(iova) - 1;
	*job = (struct rte_dpaa2_qdma_job *)*ppjob;
	(*job)->status = (fd->simple_pci.acc_err << 8) |
					(fd->simple_pci.error);
	vqid = (*job)->vq_id;
	*nb_jobs = 1;

	return vqid;
}

static inline uint16_t
dpdmai_dev_get_single_job_lf(struct qdma_virt_queue *qdma_vq,
			     const struct qbman_fd *fd,
			     struct rte_dpaa2_qdma_job **job,
			     uint16_t *nb_jobs)
{
	struct qbman_fle *fle;
	struct rte_dpaa2_qdma_job **ppjob = NULL;
	uint16_t status;

	/*
	 * Fetch metadata from FLE. job and vq_id were set
	 * in metadata in the enqueue operation.
	 */
	fle = (struct qbman_fle *)
			DPAA2_IOVA_TO_VADDR(DPAA2_GET_FD_ADDR(fd));

	*nb_jobs = 1;
	ppjob = (struct rte_dpaa2_qdma_job **)((uintptr_t)(uint64_t)fle -
			QDMA_FLE_FLE_OFFSET + QDMA_FLE_SINGLE_JOB_OFFSET);

	status = (DPAA2_GET_FD_ERR(fd) << 8) | (DPAA2_GET_FD_FRC(fd) & 0xFF);

	*job = *ppjob;
	(*job)->status = status;

	/* Free FLE to the pool */
	rte_mempool_put(qdma_vq->fle_pool,
			(void *)
			((uintptr_t)(uint64_t)fle - QDMA_FLE_FLE_OFFSET));

	return (*job)->vq_id;
}

static inline uint16_t
dpdmai_dev_get_sg_job_lf(struct qdma_virt_queue *qdma_vq,
			 const struct qbman_fd *fd,
			 struct rte_dpaa2_qdma_job **job,
			 uint16_t *nb_jobs)
{
	struct qbman_fle *fle;
	struct rte_dpaa2_qdma_job **ppjob = NULL;
	uint16_t i, status;

	/*
	 * Fetch metadata from FLE. job and vq_id were set
	 * in metadata in the enqueue operation.
	 */
	fle = (struct qbman_fle *)
			DPAA2_IOVA_TO_VADDR(DPAA2_GET_FD_ADDR(fd));
	*nb_jobs = *((uint16_t *)((uintptr_t)(uint64_t)fle -
				QDMA_FLE_FLE_OFFSET + QDMA_FLE_JOB_NB_OFFSET));
	ppjob = (struct rte_dpaa2_qdma_job **)((uintptr_t)(uint64_t)fle -
				QDMA_FLE_FLE_OFFSET + QDMA_FLE_SG_JOBS_OFFSET);
	status = (DPAA2_GET_FD_ERR(fd) << 8) | (DPAA2_GET_FD_FRC(fd) & 0xFF);

	for (i = 0; i < (*nb_jobs); i++) {
		job[i] = ppjob[i];
		job[i]->status = status;
	}

	/* Free FLE to the pool */
	rte_mempool_put(qdma_vq->fle_pool,
			(void *)
			((uintptr_t)(uint64_t)fle - QDMA_FLE_FLE_OFFSET));

	return job[0]->vq_id;
}

/* Function to receive a QDMA job for a given device and queue*/
static int
dpdmai_dev_dequeue_multijob_prefetch(struct qdma_virt_queue *qdma_vq,
				     uint16_t *vq_id,
				     struct rte_dpaa2_qdma_job **job,
				     uint16_t nb_jobs)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = qdma_vq->dpdmai_dev;
	struct dpaa2_queue *rxq = &(dpdmai_dev->rx_queue[0]);
	struct qbman_result *dq_storage, *dq_storage1 = NULL;
	struct qbman_pull_desc pulldesc;
	struct qbman_swp *swp;
	struct queue_storage_info_t *q_storage;
	uint8_t status, pending;
	uint8_t num_rx = 0;
	const struct qbman_fd *fd;
	uint16_t vqid, num_rx_ret;
	uint16_t rx_fqid = rxq->fqid;
	int ret, pull_size;

	if (qdma_vq->flags & DPAA2_QDMA_VQ_FD_SG_FORMAT) {
		/** Make sure there are enough space to get jobs.*/
		if (unlikely(nb_jobs < DPAA2_QDMA_MAX_SG_NB))
			return -EINVAL;
		nb_jobs = 1;
	}

	if (unlikely(!DPAA2_PER_LCORE_DPIO)) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			DPAA2_QDMA_ERR(
				"Failed to allocate IO portal, tid: %d\n",
				rte_gettid());
			return 0;
		}
	}
	swp = DPAA2_PER_LCORE_PORTAL;

	pull_size = (nb_jobs > dpaa2_dqrr_size) ? dpaa2_dqrr_size : nb_jobs;
	q_storage = rxq->q_storage;

	if (unlikely(!q_storage->active_dqs)) {
		q_storage->toggle = 0;
		dq_storage = q_storage->dq_storage[q_storage->toggle];
		q_storage->last_num_pkts = pull_size;
		qbman_pull_desc_clear(&pulldesc);
		qbman_pull_desc_set_numframes(&pulldesc,
					      q_storage->last_num_pkts);
		qbman_pull_desc_set_fq(&pulldesc, rx_fqid);
		qbman_pull_desc_set_storage(&pulldesc, dq_storage,
				(size_t)(DPAA2_VADDR_TO_IOVA(dq_storage)), 1);
		if (check_swp_active_dqs(DPAA2_PER_LCORE_DPIO->index)) {
			while (!qbman_check_command_complete(
				get_swp_active_dqs(
				DPAA2_PER_LCORE_DPIO->index)))
				;
			clear_swp_active_dqs(DPAA2_PER_LCORE_DPIO->index);
		}
		while (1) {
			if (qbman_swp_pull(swp, &pulldesc)) {
				DPAA2_QDMA_DP_WARN(
					"VDQ command not issued.QBMAN busy\n");
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
	qbman_pull_desc_set_fq(&pulldesc, rx_fqid);
	qbman_pull_desc_set_storage(&pulldesc, dq_storage1,
		(size_t)(DPAA2_VADDR_TO_IOVA(dq_storage1)), 1);

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

		vqid = qdma_vq->get_job(qdma_vq, fd, &job[num_rx],
								&num_rx_ret);
		if (vq_id)
			vq_id[num_rx] = vqid;

		dq_storage++;
		num_rx += num_rx_ret;
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
			DPAA2_QDMA_DP_WARN(
				"VDQ command is not issued. QBMAN is busy (2)\n");
			continue;
		}
		break;
	}

	q_storage->active_dqs = dq_storage1;
	q_storage->active_dpio_id = DPAA2_PER_LCORE_DPIO->index;
	set_swp_active_dqs(DPAA2_PER_LCORE_DPIO->index, dq_storage1);

	return num_rx;
}

static int
dpdmai_dev_dequeue_multijob_no_prefetch(struct qdma_virt_queue *qdma_vq,
					uint16_t *vq_id,
					struct rte_dpaa2_qdma_job **job,
					uint16_t nb_jobs)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = qdma_vq->dpdmai_dev;
	struct dpaa2_queue *rxq = &(dpdmai_dev->rx_queue[0]);
	struct qbman_result *dq_storage;
	struct qbman_pull_desc pulldesc;
	struct qbman_swp *swp;
	uint8_t status, pending;
	uint8_t num_rx = 0;
	const struct qbman_fd *fd;
	uint16_t vqid, num_rx_ret;
	uint16_t rx_fqid = rxq->fqid;
	int ret, next_pull, num_pulled = 0;

	if (qdma_vq->flags & DPAA2_QDMA_VQ_FD_SG_FORMAT) {
		/** Make sure there are enough space to get jobs.*/
		if (unlikely(nb_jobs < DPAA2_QDMA_MAX_SG_NB))
			return -EINVAL;
		nb_jobs = 1;
	}

	next_pull = nb_jobs;

	if (unlikely(!DPAA2_PER_LCORE_DPIO)) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			DPAA2_QDMA_ERR(
				"Failed to allocate IO portal, tid: %d\n",
				rte_gettid());
			return 0;
		}
	}
	swp = DPAA2_PER_LCORE_PORTAL;

	rxq = &(dpdmai_dev->rx_queue[0]);

	do {
		dq_storage = rxq->q_storage->dq_storage[0];
		/* Prepare dequeue descriptor */
		qbman_pull_desc_clear(&pulldesc);
		qbman_pull_desc_set_fq(&pulldesc, rx_fqid);
		qbman_pull_desc_set_storage(&pulldesc, dq_storage,
			(uint64_t)(DPAA2_VADDR_TO_IOVA(dq_storage)), 1);

		if (next_pull > dpaa2_dqrr_size) {
			qbman_pull_desc_set_numframes(&pulldesc,
					dpaa2_dqrr_size);
			next_pull -= dpaa2_dqrr_size;
		} else {
			qbman_pull_desc_set_numframes(&pulldesc, next_pull);
			next_pull = 0;
		}

		while (1) {
			if (qbman_swp_pull(swp, &pulldesc)) {
				DPAA2_QDMA_DP_WARN(
					"VDQ command not issued. QBMAN busy");
				/* Portal was busy, try again */
				continue;
			}
			break;
		}

		rte_prefetch0((void *)((size_t)(dq_storage + 1)));
		/* Check if the previous issued command is completed. */
		while (!qbman_check_command_complete(dq_storage))
			;

		num_pulled = 0;
		pending = 1;

		do {
			/* Loop until dq_storage is updated
			 * with new token by QBMAN
			 */
			while (!qbman_check_new_result(dq_storage))
				;
			rte_prefetch0((void *)((size_t)(dq_storage + 2)));

			if (qbman_result_DQ_is_pull_complete(dq_storage)) {
				pending = 0;
				/* Check for valid frame. */
				status = qbman_result_DQ_flags(dq_storage);
				if (unlikely((status &
					QBMAN_DQ_STAT_VALIDFRAME) == 0))
					continue;
			}
			fd = qbman_result_DQ_fd(dq_storage);

			vqid = qdma_vq->get_job(qdma_vq, fd,
						&job[num_rx], &num_rx_ret);
			if (vq_id)
				vq_id[num_rx] = vqid;

			dq_storage++;
			num_rx += num_rx_ret;
			num_pulled++;

		} while (pending);
	/* Last VDQ provided all packets and more packets are requested */
	} while (next_pull && num_pulled == dpaa2_dqrr_size);

	return num_rx;
}

static int
dpdmai_dev_submit_multi(struct qdma_virt_queue *qdma_vq,
			struct rte_dpaa2_qdma_job **job,
			uint16_t nb_jobs)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = qdma_vq->dpdmai_dev;
	uint16_t txq_id = dpdmai_dev->tx_queue[0].fqid;
	struct qbman_fd fd[DPAA2_QDMA_MAX_DESC];
	struct qbman_eq_desc eqdesc;
	struct qbman_swp *swp;
	uint32_t num_to_send = 0;
	uint16_t num_tx = 0;
	uint32_t enqueue_loop, loop;
	int ret;

	if (unlikely(!DPAA2_PER_LCORE_DPIO)) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			DPAA2_QDMA_ERR(
				"Failed to allocate IO portal, tid: %d\n",
				rte_gettid());
			return 0;
		}
	}
	swp = DPAA2_PER_LCORE_PORTAL;

	/* Prepare enqueue descriptor */
	qbman_eq_desc_clear(&eqdesc);
	qbman_eq_desc_set_fq(&eqdesc, txq_id);
	qbman_eq_desc_set_no_orp(&eqdesc, 0);
	qbman_eq_desc_set_response(&eqdesc, 0, 0);

	if (qdma_vq->flags & DPAA2_QDMA_VQ_FD_SG_FORMAT) {
		uint16_t fd_nb;
		uint16_t sg_entry_nb = nb_jobs > DPAA2_QDMA_MAX_SG_NB ?
						DPAA2_QDMA_MAX_SG_NB : nb_jobs;
		uint16_t job_idx = 0;
		uint16_t fd_sg_nb[8];
		uint16_t nb_jobs_ret = 0;

		if (nb_jobs % DPAA2_QDMA_MAX_SG_NB)
			fd_nb = nb_jobs / DPAA2_QDMA_MAX_SG_NB + 1;
		else
			fd_nb = nb_jobs / DPAA2_QDMA_MAX_SG_NB;

		memset(&fd[0], 0, sizeof(struct qbman_fd) * fd_nb);

		for (loop = 0; loop < fd_nb; loop++) {
			ret = qdma_vq->set_fd(qdma_vq, &fd[loop], &job[job_idx],
					      sg_entry_nb);
			if (unlikely(ret < 0))
				return 0;
			fd_sg_nb[loop] = sg_entry_nb;
			nb_jobs -= sg_entry_nb;
			job_idx += sg_entry_nb;
			sg_entry_nb = nb_jobs > DPAA2_QDMA_MAX_SG_NB ?
						DPAA2_QDMA_MAX_SG_NB : nb_jobs;
		}

		/* Enqueue the packet to the QBMAN */
		enqueue_loop = 0;

		while (enqueue_loop < fd_nb) {
			ret = qbman_swp_enqueue_multiple(swp,
					&eqdesc, &fd[enqueue_loop],
					NULL, fd_nb - enqueue_loop);
			if (likely(ret >= 0)) {
				for (loop = 0; loop < (uint32_t)ret; loop++)
					nb_jobs_ret +=
						fd_sg_nb[enqueue_loop + loop];
				enqueue_loop += ret;
			}
		}

		return nb_jobs_ret;
	}

	memset(fd, 0, nb_jobs * sizeof(struct qbman_fd));

	while (nb_jobs > 0) {
		num_to_send = (nb_jobs > dpaa2_eqcr_size) ?
			dpaa2_eqcr_size : nb_jobs;

		ret = qdma_vq->set_fd(qdma_vq, &fd[num_tx],
						&job[num_tx], num_to_send);
		if (unlikely(ret < 0))
			break;

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
		nb_jobs -= loop;
	}

	qdma_vq->num_enqueues += num_tx;

	return num_tx;
}

static inline int
dpaa2_qdma_submit(void *dev_private, uint16_t vchan)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;
	struct qdma_virt_queue *qdma_vq = &qdma_dev->vqs[vchan];

	dpdmai_dev_submit_multi(qdma_vq, qdma_vq->job_list,
				qdma_vq->num_valid_jobs);

	qdma_vq->num_valid_jobs = 0;

	return 0;
}

static int
dpaa2_qdma_enqueue(void *dev_private, uint16_t vchan,
		   rte_iova_t src, rte_iova_t dst,
		   uint32_t length, uint64_t flags)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;
	struct qdma_virt_queue *qdma_vq = &qdma_dev->vqs[vchan];
	struct rte_dpaa2_qdma_job *job;
	int idx, ret;

	idx = (uint16_t)(qdma_vq->num_enqueues + qdma_vq->num_valid_jobs);

	ret = rte_mempool_get(qdma_vq->job_pool, (void **)&job);
	if (ret) {
		DPAA2_QDMA_DP_DEBUG("Memory alloc failed for FLE");
		return -ENOSPC;
	}

	job->src = src;
	job->dest = dst;
	job->len = length;
	job->flags = flags;
	job->status = 0;
	job->vq_id = vchan;

	qdma_vq->job_list[qdma_vq->num_valid_jobs] = job;
	qdma_vq->num_valid_jobs++;

	if (flags & RTE_DMA_OP_FLAG_SUBMIT)
		dpaa2_qdma_submit(dev_private, vchan);

	return idx;
}

int
rte_dpaa2_qdma_copy_multi(int16_t dev_id, uint16_t vchan,
			  struct rte_dpaa2_qdma_job **jobs,
			  uint16_t nb_cpls)
{
	struct rte_dma_fp_object *obj = &rte_dma_fp_objs[dev_id];
	struct dpaa2_dpdmai_dev *dpdmai_dev = obj->dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;
	struct qdma_virt_queue *qdma_vq = &qdma_dev->vqs[vchan];

	return dpdmai_dev_submit_multi(qdma_vq, jobs, nb_cpls);
}

static uint16_t
dpaa2_qdma_dequeue_multi(struct qdma_device *qdma_dev,
			 struct qdma_virt_queue *qdma_vq,
			 struct rte_dpaa2_qdma_job **jobs,
			 uint16_t nb_jobs)
{
	struct qdma_virt_queue *temp_qdma_vq;
	int ring_count;
	int ret = 0, i;

	if (qdma_vq->flags & DPAA2_QDMA_VQ_FD_SG_FORMAT) {
		/** Make sure there are enough space to get jobs.*/
		if (unlikely(nb_jobs < DPAA2_QDMA_MAX_SG_NB))
			return -EINVAL;
	}

	/* Only dequeue when there are pending jobs on VQ */
	if (qdma_vq->num_enqueues == qdma_vq->num_dequeues)
		return 0;

	if (!(qdma_vq->flags & DPAA2_QDMA_VQ_FD_SG_FORMAT) &&
		qdma_vq->num_enqueues < (qdma_vq->num_dequeues + nb_jobs))
		nb_jobs = RTE_MIN((qdma_vq->num_enqueues -
				qdma_vq->num_dequeues), nb_jobs);

	if (qdma_vq->exclusive_hw_queue) {
		/* In case of exclusive queue directly fetch from HW queue */
		ret = qdma_vq->dequeue_job(qdma_vq, NULL, jobs, nb_jobs);
		if (ret < 0) {
			DPAA2_QDMA_ERR(
				"Dequeue from DPDMAI device failed: %d", ret);
			return ret;
		}
	} else {
		uint16_t temp_vq_id[DPAA2_QDMA_MAX_DESC];

		/* Get the QDMA completed jobs from the software ring.
		 * In case they are not available on the ring poke the HW
		 * to fetch completed jobs from corresponding HW queues
		 */
		ring_count = rte_ring_count(qdma_vq->status_ring);
		if (ring_count < nb_jobs) {
			ret = qdma_vq->dequeue_job(qdma_vq,
					temp_vq_id, jobs, nb_jobs);
			for (i = 0; i < ret; i++) {
				temp_qdma_vq = &qdma_dev->vqs[temp_vq_id[i]];
				rte_ring_enqueue(temp_qdma_vq->status_ring,
					(void *)(jobs[i]));
			}
			ring_count = rte_ring_count(
					qdma_vq->status_ring);
		}

		if (ring_count) {
			/* Dequeue job from the software ring
			 * to provide to the user
			 */
			ret = rte_ring_dequeue_bulk(qdma_vq->status_ring,
						    (void **)jobs,
						    ring_count, NULL);
		}
	}

	qdma_vq->num_dequeues += ret;
	return ret;
}

static uint16_t
dpaa2_qdma_dequeue_status(void *dev_private, uint16_t vchan,
			  const uint16_t nb_cpls,
			  uint16_t *last_idx,
			  enum rte_dma_status_code *st)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;
	struct qdma_virt_queue *qdma_vq = &qdma_dev->vqs[vchan];
	struct rte_dpaa2_qdma_job *jobs[DPAA2_QDMA_MAX_DESC];
	int ret, i;

	ret = dpaa2_qdma_dequeue_multi(qdma_dev, qdma_vq, jobs, nb_cpls);

	for (i = 0; i < ret; i++)
		st[i] = jobs[i]->status;

	rte_mempool_put_bulk(qdma_vq->job_pool, (void **)jobs, ret);

	if (last_idx != NULL)
		*last_idx = (uint16_t)(qdma_vq->num_dequeues - 1);

	return ret;
}

static uint16_t
dpaa2_qdma_dequeue(void *dev_private,
		   uint16_t vchan, const uint16_t nb_cpls,
		   uint16_t *last_idx, bool *has_error)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;
	struct qdma_virt_queue *qdma_vq = &qdma_dev->vqs[vchan];
	struct rte_dpaa2_qdma_job *jobs[DPAA2_QDMA_MAX_DESC];
	int ret;

	RTE_SET_USED(has_error);

	ret = dpaa2_qdma_dequeue_multi(qdma_dev, qdma_vq,
				jobs, nb_cpls);

	rte_mempool_put_bulk(qdma_vq->job_pool, (void **)jobs, ret);

	if (last_idx != NULL)
		*last_idx = (uint16_t)(qdma_vq->num_dequeues - 1);

	return ret;
}

uint16_t
rte_dpaa2_qdma_completed_multi(int16_t dev_id, uint16_t vchan,
			       struct rte_dpaa2_qdma_job **jobs,
			       uint16_t nb_cpls)
{
	struct rte_dma_fp_object *obj = &rte_dma_fp_objs[dev_id];
	struct dpaa2_dpdmai_dev *dpdmai_dev = obj->dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;
	struct qdma_virt_queue *qdma_vq = &qdma_dev->vqs[vchan];

	return dpaa2_qdma_dequeue_multi(qdma_dev, qdma_vq, jobs, nb_cpls);
}

static int
dpaa2_qdma_info_get(const struct rte_dma_dev *dev,
		    struct rte_dma_info *dev_info,
		    uint32_t info_sz)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(info_sz);

	dev_info->dev_capa = RTE_DMA_CAPA_MEM_TO_MEM |
			     RTE_DMA_CAPA_MEM_TO_DEV |
			     RTE_DMA_CAPA_DEV_TO_DEV |
			     RTE_DMA_CAPA_DEV_TO_MEM |
			     RTE_DMA_CAPA_SILENT |
			     RTE_DMA_CAPA_OPS_COPY;
	dev_info->max_vchans = DPAA2_QDMA_MAX_VHANS;
	dev_info->max_desc = DPAA2_QDMA_MAX_DESC;
	dev_info->min_desc = DPAA2_QDMA_MIN_DESC;

	return 0;
}

static int
dpaa2_qdma_configure(struct rte_dma_dev *dev,
		     const struct rte_dma_conf *dev_conf,
		     uint32_t conf_sz)
{
	char name[32]; /* RTE_MEMZONE_NAMESIZE = 32 */
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev->data->dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;

	DPAA2_QDMA_FUNC_TRACE();

	RTE_SET_USED(conf_sz);

	/* In case QDMA device is not in stopped state, return -EBUSY */
	if (qdma_dev->state == 1) {
		DPAA2_QDMA_ERR(
			"Device is in running state. Stop before config.");
		return -1;
	}

	/* Allocate Virtual Queues */
	sprintf(name, "qdma_%d_vq", dev->data->dev_id);
	qdma_dev->vqs = rte_malloc(name,
			(sizeof(struct qdma_virt_queue) * dev_conf->nb_vchans),
			RTE_CACHE_LINE_SIZE);
	if (!qdma_dev->vqs) {
		DPAA2_QDMA_ERR("qdma_virtual_queues allocation failed");
		return -ENOMEM;
	}
	qdma_dev->num_vqs = dev_conf->nb_vchans;

	return 0;
}

static int
check_devargs_handler(__rte_unused const char *key,
		      const char *value,
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

/* Enable FD in Ultra Short format */
void
rte_dpaa2_qdma_vchan_fd_us_enable(int16_t dev_id, uint16_t vchan)
{
	struct rte_dma_fp_object *obj = &rte_dma_fp_objs[dev_id];
	struct dpaa2_dpdmai_dev *dpdmai_dev = obj->dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;

	qdma_dev->vqs[vchan].flags |= DPAA2_QDMA_VQ_FD_SHORT_FORMAT;
}

/* Enable internal SG processing */
void
rte_dpaa2_qdma_vchan_internal_sg_enable(int16_t dev_id, uint16_t vchan)
{
	struct rte_dma_fp_object *obj = &rte_dma_fp_objs[dev_id];
	struct dpaa2_dpdmai_dev *dpdmai_dev = obj->dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;

	qdma_dev->vqs[vchan].flags |= DPAA2_QDMA_VQ_FD_SG_FORMAT;
}

/* Enable RBP */
void
rte_dpaa2_qdma_vchan_rbp_enable(int16_t dev_id, uint16_t vchan,
				struct rte_dpaa2_qdma_rbp *rbp_config)
{
	struct rte_dma_fp_object *obj = &rte_dma_fp_objs[dev_id];
	struct dpaa2_dpdmai_dev *dpdmai_dev = obj->dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;

	memcpy(&qdma_dev->vqs[vchan].rbp, rbp_config,
			sizeof(struct rte_dpaa2_qdma_rbp));
}

static int
dpaa2_qdma_vchan_setup(struct rte_dma_dev *dev, uint16_t vchan,
		       const struct rte_dma_vchan_conf *conf,
		       uint32_t conf_sz)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev->data->dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;
	uint32_t pool_size;
	char ring_name[32];
	char pool_name[64];
	int fd_long_format = 1;
	int sg_enable = 0;

	DPAA2_QDMA_FUNC_TRACE();

	RTE_SET_USED(conf_sz);

	if (qdma_dev->vqs[vchan].flags & DPAA2_QDMA_VQ_FD_SG_FORMAT)
		sg_enable = 1;

	if (qdma_dev->vqs[vchan].flags & DPAA2_QDMA_VQ_FD_SHORT_FORMAT)
		fd_long_format = 0;

	if (dev->data->dev_conf.enable_silent)
		qdma_dev->vqs[vchan].flags |= DPAA2_QDMA_VQ_NO_RESPONSE;

	if (sg_enable) {
		if (qdma_dev->num_vqs != 1) {
			DPAA2_QDMA_ERR(
				"qDMA SG format only supports physical queue!");
			return -ENODEV;
		}
		if (!fd_long_format) {
			DPAA2_QDMA_ERR(
				"qDMA SG format only supports long FD format!");
			return -ENODEV;
		}
		pool_size = QDMA_FLE_SG_POOL_SIZE;
	} else {
		pool_size = QDMA_FLE_SINGLE_POOL_SIZE;
	}

	if (qdma_dev->num_vqs == 1)
		qdma_dev->vqs[vchan].exclusive_hw_queue = 1;
	else {
		/* Allocate a Ring for Virtual Queue in VQ mode */
		snprintf(ring_name, sizeof(ring_name), "status ring %d %d",
			 dev->data->dev_id, vchan);
		qdma_dev->vqs[vchan].status_ring = rte_ring_create(ring_name,
			conf->nb_desc, rte_socket_id(), 0);
		if (!qdma_dev->vqs[vchan].status_ring) {
			DPAA2_QDMA_ERR("Status ring creation failed for vq");
			return rte_errno;
		}
	}

	snprintf(pool_name, sizeof(pool_name),
		"qdma_fle_pool_dev%d_qid%d", dpdmai_dev->dpdmai_id, vchan);
	qdma_dev->vqs[vchan].fle_pool = rte_mempool_create(pool_name,
			conf->nb_desc, pool_size,
			QDMA_FLE_CACHE_SIZE(conf->nb_desc), 0,
			NULL, NULL, NULL, NULL, SOCKET_ID_ANY, 0);
	if (!qdma_dev->vqs[vchan].fle_pool) {
		DPAA2_QDMA_ERR("qdma_fle_pool create failed");
		return -ENOMEM;
	}

	snprintf(pool_name, sizeof(pool_name),
		"qdma_job_pool_dev%d_qid%d", dpdmai_dev->dpdmai_id, vchan);
	qdma_dev->vqs[vchan].job_pool = rte_mempool_create(pool_name,
			conf->nb_desc, pool_size,
			QDMA_FLE_CACHE_SIZE(conf->nb_desc), 0,
			NULL, NULL, NULL, NULL, SOCKET_ID_ANY, 0);
	if (!qdma_dev->vqs[vchan].job_pool) {
		DPAA2_QDMA_ERR("qdma_job_pool create failed");
		return -ENOMEM;
	}

	if (fd_long_format) {
		if (sg_enable) {
			qdma_dev->vqs[vchan].set_fd = dpdmai_dev_set_sg_fd_lf;
			qdma_dev->vqs[vchan].get_job = dpdmai_dev_get_sg_job_lf;
		} else {
			if (dev->data->dev_conf.enable_silent)
				qdma_dev->vqs[vchan].set_fd =
					dpdmai_dev_set_multi_fd_lf_no_rsp;
			else
				qdma_dev->vqs[vchan].set_fd =
					dpdmai_dev_set_multi_fd_lf;
			qdma_dev->vqs[vchan].get_job = dpdmai_dev_get_single_job_lf;
		}
	} else {
		qdma_dev->vqs[vchan].set_fd = dpdmai_dev_set_fd_us;
		qdma_dev->vqs[vchan].get_job = dpdmai_dev_get_job_us;
	}

	if (dpaa2_qdma_get_devargs(dev->device->devargs,
			DPAA2_QDMA_PREFETCH)) {
		/* If no prefetch is configured. */
		qdma_dev->vqs[vchan].dequeue_job =
				dpdmai_dev_dequeue_multijob_prefetch;
		DPAA2_QDMA_INFO("Prefetch RX Mode enabled");
	} else {
		qdma_dev->vqs[vchan].dequeue_job =
			dpdmai_dev_dequeue_multijob_no_prefetch;
	}

	qdma_dev->vqs[vchan].dpdmai_dev = dpdmai_dev;
	qdma_dev->vqs[vchan].nb_desc = conf->nb_desc;
	qdma_dev->vqs[vchan].enqueue_job = dpdmai_dev_submit_multi;

	return 0;
}

static int
dpaa2_qdma_start(struct rte_dma_dev *dev)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev->data->dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;

	DPAA2_QDMA_FUNC_TRACE();

	qdma_dev->state = 1;

	return 0;
}

static int
dpaa2_qdma_stop(struct rte_dma_dev *dev)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev->data->dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;

	DPAA2_QDMA_FUNC_TRACE();

	qdma_dev->state = 0;

	return 0;
}

static int
dpaa2_qdma_reset(struct rte_dma_dev *dev)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev->data->dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;
	int i;

	DPAA2_QDMA_FUNC_TRACE();

	/* In case QDMA device is not in stopped state, return -EBUSY */
	if (qdma_dev->state == 1) {
		DPAA2_QDMA_ERR(
			"Device is in running state. Stop before reset.");
		return -EBUSY;
	}

	/* In case there are pending jobs on any VQ, return -EBUSY */
	for (i = 0; i < qdma_dev->num_vqs; i++) {
		if (qdma_dev->vqs[i].in_use && (qdma_dev->vqs[i].num_enqueues !=
		    qdma_dev->vqs[i].num_dequeues)) {
			DPAA2_QDMA_ERR("Jobs are still pending on VQ: %d", i);
			return -EBUSY;
		}
	}

	/* Reset and free virtual queues */
	for (i = 0; i < qdma_dev->num_vqs; i++) {
		rte_ring_free(qdma_dev->vqs[i].status_ring);
	}
	rte_free(qdma_dev->vqs);
	qdma_dev->vqs = NULL;

	/* Reset QDMA device structure */
	qdma_dev->num_vqs = 0;

	return 0;
}

static int
dpaa2_qdma_close(__rte_unused struct rte_dma_dev *dev)
{
	DPAA2_QDMA_FUNC_TRACE();

	dpaa2_qdma_reset(dev);

	return 0;
}

static int
dpaa2_qdma_stats_get(const struct rte_dma_dev *dmadev, uint16_t vchan,
		    struct rte_dma_stats *rte_stats, uint32_t size)
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
	int ret;

	DPAA2_QDMA_FUNC_TRACE();

	ret = dpdmai_disable(&dpdmai_dev->dpdmai, CMD_PRI_LOW,
			     dpdmai_dev->token);
	if (ret)
		DPAA2_QDMA_ERR("dmdmai disable failed");

	/* Set up the DQRR storage for Rx */
	struct dpaa2_queue *rxq = &(dpdmai_dev->rx_queue[0]);

	if (rxq->q_storage) {
		dpaa2_free_dq_storage(rxq->q_storage);
		rte_free(rxq->q_storage);
	}

	/* Close the device at underlying layer*/
	ret = dpdmai_close(&dpdmai_dev->dpdmai, CMD_PRI_LOW, dpdmai_dev->token);
	if (ret)
		DPAA2_QDMA_ERR("Failure closing dpdmai device");

	return 0;
}

static int
dpaa2_dpdmai_dev_init(struct rte_dma_dev *dev, int dpdmai_id)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev->data->dev_private;
	struct dpdmai_rx_queue_cfg rx_queue_cfg;
	struct dpdmai_attr attr;
	struct dpdmai_rx_queue_attr rx_attr;
	struct dpdmai_tx_queue_attr tx_attr;
	struct dpaa2_queue *rxq;
	int ret;

	DPAA2_QDMA_FUNC_TRACE();

	/* Open DPDMAI device */
	dpdmai_dev->dpdmai_id = dpdmai_id;
	dpdmai_dev->dpdmai.regs = dpaa2_get_mcp_ptr(MC_PORTAL_INDEX);
	dpdmai_dev->qdma_dev = rte_malloc(NULL, sizeof(struct qdma_device),
					  RTE_CACHE_LINE_SIZE);
	ret = dpdmai_open(&dpdmai_dev->dpdmai, CMD_PRI_LOW,
			  dpdmai_dev->dpdmai_id, &dpdmai_dev->token);
	if (ret) {
		DPAA2_QDMA_ERR("dpdmai_open() failed with err: %d", ret);
		return ret;
	}

	/* Get DPDMAI attributes */
	ret = dpdmai_get_attributes(&dpdmai_dev->dpdmai, CMD_PRI_LOW,
				    dpdmai_dev->token, &attr);
	if (ret) {
		DPAA2_QDMA_ERR("dpdmai get attributes failed with err: %d",
			       ret);
		goto init_err;
	}
	dpdmai_dev->num_queues = attr.num_of_queues;

	/* Set up Rx Queue */
	memset(&rx_queue_cfg, 0, sizeof(struct dpdmai_rx_queue_cfg));
	ret = dpdmai_set_rx_queue(&dpdmai_dev->dpdmai,
				  CMD_PRI_LOW,
				  dpdmai_dev->token,
				  0, 0, &rx_queue_cfg);
	if (ret) {
		DPAA2_QDMA_ERR("Setting Rx queue failed with err: %d",
			       ret);
		goto init_err;
	}

	/* Allocate DQ storage for the DPDMAI Rx queues */
	rxq = &(dpdmai_dev->rx_queue[0]);
	rxq->q_storage = rte_malloc("dq_storage",
				    sizeof(struct queue_storage_info_t),
				    RTE_CACHE_LINE_SIZE);
	if (!rxq->q_storage) {
		DPAA2_QDMA_ERR("q_storage allocation failed");
		ret = -ENOMEM;
		goto init_err;
	}

	memset(rxq->q_storage, 0, sizeof(struct queue_storage_info_t));
	ret = dpaa2_alloc_dq_storage(rxq->q_storage);
	if (ret) {
		DPAA2_QDMA_ERR("dpaa2_alloc_dq_storage failed");
		goto init_err;
	}

	/* Get Rx and Tx queues FQID */
	ret = dpdmai_get_rx_queue(&dpdmai_dev->dpdmai, CMD_PRI_LOW,
				  dpdmai_dev->token, 0, 0, &rx_attr);
	if (ret) {
		DPAA2_QDMA_ERR("Reading device failed with err: %d",
			       ret);
		goto init_err;
	}
	dpdmai_dev->rx_queue[0].fqid = rx_attr.fqid;

	ret = dpdmai_get_tx_queue(&dpdmai_dev->dpdmai, CMD_PRI_LOW,
				  dpdmai_dev->token, 0, 0, &tx_attr);
	if (ret) {
		DPAA2_QDMA_ERR("Reading device failed with err: %d",
			       ret);
		goto init_err;
	}
	dpdmai_dev->tx_queue[0].fqid = tx_attr.fqid;

	/* Enable the device */
	ret = dpdmai_enable(&dpdmai_dev->dpdmai, CMD_PRI_LOW,
			    dpdmai_dev->token);
	if (ret) {
		DPAA2_QDMA_ERR("Enabling device failed with err: %d", ret);
		goto init_err;
	}

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

	DPAA2_QDMA_DEBUG("Initialized dpdmai object successfully");

	/* Reset the QDMA device */
	ret = dpaa2_qdma_reset(dev);
	if (ret) {
		DPAA2_QDMA_ERR("Resetting QDMA failed");
		goto init_err;
	}

	return 0;
init_err:
	dpaa2_dpdmai_dev_uninit(dev);
	return ret;
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
	dmadev->fp_obj->copy = dpaa2_qdma_enqueue;
	dmadev->fp_obj->submit = dpaa2_qdma_submit;
	dmadev->fp_obj->completed = dpaa2_qdma_dequeue;
	dmadev->fp_obj->completed_status = dpaa2_qdma_dequeue_status;
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
	"no_prefetch=<int> ");
RTE_LOG_REGISTER_DEFAULT(dpaa2_qdma_logtype, INFO);
