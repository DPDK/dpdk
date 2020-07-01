/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 NXP
 */

#include <string.h>

#include <rte_eal.h>
#include <rte_fslmc.h>
#include <rte_atomic.h>
#include <rte_lcore.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>
#include <rte_malloc.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_prefetch.h>
#include <rte_kvargs.h>

#include <mc/fsl_dpdmai.h>
#include <portal/dpaa2_hw_pvt.h>
#include <portal/dpaa2_hw_dpio.h>

#include "rte_pmd_dpaa2_qdma.h"
#include "dpaa2_qdma.h"
#include "dpaa2_qdma_logs.h"

#define DPAA2_QDMA_NO_PREFETCH "no_prefetch"

uint32_t dpaa2_coherent_no_alloc_cache;
uint32_t dpaa2_coherent_alloc_cache;

/* QDMA device */
static struct qdma_device qdma_dev;

/* QDMA H/W queues list */
TAILQ_HEAD(qdma_hw_queue_list, qdma_hw_queue);
static struct qdma_hw_queue_list qdma_queue_list
	= TAILQ_HEAD_INITIALIZER(qdma_queue_list);

/* QDMA Virtual Queues */
static struct qdma_virt_queue *qdma_vqs;

/* QDMA per core data */
static struct qdma_per_core_info qdma_core_info[RTE_MAX_LCORE];

typedef int (dpdmai_dev_dequeue_multijob_t)(struct dpaa2_dpdmai_dev *dpdmai_dev,
					    uint16_t rxq_id,
					    uint16_t *vq_id,
					    struct rte_qdma_job **job,
					    uint16_t nb_jobs);

dpdmai_dev_dequeue_multijob_t *dpdmai_dev_dequeue_multijob;

typedef uint16_t (dpdmai_dev_get_job_t)(const struct qbman_fd *fd,
					struct rte_qdma_job **job);
typedef int (dpdmai_dev_set_fd_t)(struct qbman_fd *fd,
				  struct rte_qdma_job *job,
				  struct rte_qdma_rbp *rbp,
				  uint16_t vq_id);
dpdmai_dev_get_job_t *dpdmai_dev_get_job;
dpdmai_dev_set_fd_t *dpdmai_dev_set_fd;

static inline int
qdma_populate_fd_pci(phys_addr_t src, phys_addr_t dest,
			uint32_t len, struct qbman_fd *fd,
			struct rte_qdma_rbp *rbp)
{
	fd->simple_pci.saddr_lo = lower_32_bits((uint64_t) (src));
	fd->simple_pci.saddr_hi = upper_32_bits((uint64_t) (src));

	fd->simple_pci.len_sl = len;

	fd->simple_pci.bmt = 1;
	fd->simple_pci.fmt = 3;
	fd->simple_pci.sl = 1;
	fd->simple_pci.ser = 1;

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
			uint32_t len, struct qbman_fd *fd)
{
	fd->simple_ddr.saddr_lo = lower_32_bits((uint64_t) (src));
	fd->simple_ddr.saddr_hi = upper_32_bits((uint64_t) (src));

	fd->simple_ddr.len = len;

	fd->simple_ddr.bmt = 1;
	fd->simple_ddr.fmt = 3;
	fd->simple_ddr.sl = 1;
	fd->simple_ddr.ser = 1;
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
			struct rte_qdma_rbp *rbp,
			uint64_t src, uint64_t dest,
			size_t len, uint32_t flags)
{
	struct qdma_sdd *sdd;

	sdd = (struct qdma_sdd *)((uint8_t *)(fle) +
		(DPAA2_QDMA_MAX_FLE * sizeof(struct qbman_fle)));

	/* first frame list to source descriptor */
	DPAA2_SET_FLE_ADDR(fle, DPAA2_VADDR_TO_IOVA(sdd));
	DPAA2_SET_FLE_LEN(fle, (2 * (sizeof(struct qdma_sdd))));

	/* source and destination descriptor */
	if (rbp && rbp->enable) {
		/* source */
		sdd->read_cmd.portid = rbp->sportid;
		sdd->rbpcmd_simple.pfid = rbp->spfid;
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
	if (flags & RTE_QDMA_JOB_SRC_PHY) {
		DPAA2_SET_FLE_ADDR(fle, src);
		DPAA2_SET_FLE_BMT(fle);
	} else {
		DPAA2_SET_FLE_ADDR(fle, DPAA2_VADDR_TO_IOVA(src));
	}
	DPAA2_SET_FLE_LEN(fle, len);

	fle++;
	/* destination frame list to destination buffer */
	if (flags & RTE_QDMA_JOB_DEST_PHY) {
		DPAA2_SET_FLE_BMT(fle);
		DPAA2_SET_FLE_ADDR(fle, dest);
	} else {
		DPAA2_SET_FLE_ADDR(fle, DPAA2_VADDR_TO_IOVA(dest));
	}
	DPAA2_SET_FLE_LEN(fle, len);

	/* Final bit: 1, for last frame list */
	DPAA2_SET_FLE_FIN(fle);
}

static inline int dpdmai_dev_set_fd_us(struct qbman_fd *fd,
					struct rte_qdma_job *job,
					struct rte_qdma_rbp *rbp,
					uint16_t vq_id)
{
	struct rte_qdma_job **ppjob;
	size_t iova;
	int ret = 0;

	if (job->src & QDMA_RBP_UPPER_ADDRESS_MASK)
		iova = (size_t)job->dest;
	else
		iova = (size_t)job->src;

	/* Set the metadata */
	job->vq_id = vq_id;
	ppjob = (struct rte_qdma_job **)DPAA2_IOVA_TO_VADDR(iova) - 1;
	*ppjob = job;

	if ((rbp->drbp == 1) || (rbp->srbp == 1))
		ret = qdma_populate_fd_pci((phys_addr_t) job->src,
					   (phys_addr_t) job->dest,
					   job->len, fd, rbp);
	else
		ret = qdma_populate_fd_ddr((phys_addr_t) job->src,
					   (phys_addr_t) job->dest,
					   job->len, fd);
	return ret;
}
static inline int dpdmai_dev_set_fd_lf(struct qbman_fd *fd,
					struct rte_qdma_job *job,
					struct rte_qdma_rbp *rbp,
					uint16_t vq_id)
{
	struct rte_qdma_job **ppjob;
	struct qbman_fle *fle;
	int ret = 0;
	/*
	 * Get an FLE/SDD from FLE pool.
	 * Note: IO metadata is before the FLE and SDD memory.
	 */
	ret = rte_mempool_get(qdma_dev.fle_pool, (void **)(&ppjob));
	if (ret) {
		DPAA2_QDMA_DP_DEBUG("Memory alloc failed for FLE");
		return ret;
	}

	/* Set the metadata */
	job->vq_id = vq_id;
	*ppjob = job;

	fle = (struct qbman_fle *)(ppjob + 1);

	DPAA2_SET_FD_ADDR(fd, DPAA2_VADDR_TO_IOVA(fle));
	DPAA2_SET_FD_COMPOUND_FMT(fd);
	DPAA2_SET_FD_FRC(fd, QDMA_SER_CTX);

	/* Populate FLE */
	memset(fle, 0, QDMA_FLE_POOL_SIZE);
	dpaa2_qdma_populate_fle(fle, rbp, job->src, job->dest,
				job->len, job->flags);

	return 0;
}

static inline uint16_t dpdmai_dev_get_job_us(const struct qbman_fd *fd,
					struct rte_qdma_job **job)
{
	uint16_t vqid;
	size_t iova;
	struct rte_qdma_job **ppjob;

	if (fd->simple_pci.saddr_hi & (QDMA_RBP_UPPER_ADDRESS_MASK >> 32))
		iova = (size_t) (((uint64_t)fd->simple_pci.daddr_hi) << 32
				| (uint64_t)fd->simple_pci.daddr_lo);
	else
		iova = (size_t)(((uint64_t)fd->simple_pci.saddr_hi) << 32
				| (uint64_t)fd->simple_pci.saddr_lo);

	ppjob = (struct rte_qdma_job **)DPAA2_IOVA_TO_VADDR(iova) - 1;
	*job = (struct rte_qdma_job *)*ppjob;
	(*job)->status = (fd->simple_pci.acc_err << 8) | (fd->simple_pci.error);
	vqid = (*job)->vq_id;

	return vqid;
}

static inline uint16_t dpdmai_dev_get_job_lf(const struct qbman_fd *fd,
					struct rte_qdma_job **job)
{
	struct rte_qdma_job **ppjob;
	uint16_t vqid;
	/*
	 * Fetch metadata from FLE. job and vq_id were set
	 * in metadata in the enqueue operation.
	 */
	ppjob = (struct rte_qdma_job **)
			DPAA2_IOVA_TO_VADDR(DPAA2_GET_FD_ADDR(fd));
	ppjob -= 1;

	*job = (struct rte_qdma_job *)*ppjob;
	(*job)->status = (DPAA2_GET_FD_ERR(fd) << 8) |
			 (DPAA2_GET_FD_FRC(fd) & 0xFF);
	vqid = (*job)->vq_id;

	/* Free FLE to the pool */
	rte_mempool_put(qdma_dev.fle_pool, (void *)ppjob);

	return vqid;
}

static struct qdma_hw_queue *
alloc_hw_queue(uint32_t lcore_id)
{
	struct qdma_hw_queue *queue = NULL;

	DPAA2_QDMA_FUNC_TRACE();

	/* Get a free queue from the list */
	TAILQ_FOREACH(queue, &qdma_queue_list, next) {
		if (queue->num_users == 0) {
			queue->lcore_id = lcore_id;
			queue->num_users++;
			break;
		}
	}

	return queue;
}

static void
free_hw_queue(struct qdma_hw_queue *queue)
{
	DPAA2_QDMA_FUNC_TRACE();

	queue->num_users--;
}


static struct qdma_hw_queue *
get_hw_queue(uint32_t lcore_id)
{
	struct qdma_per_core_info *core_info;
	struct qdma_hw_queue *queue, *temp;
	uint32_t least_num_users;
	int num_hw_queues, i;

	DPAA2_QDMA_FUNC_TRACE();

	core_info = &qdma_core_info[lcore_id];
	num_hw_queues = core_info->num_hw_queues;

	/*
	 * Allocate a HW queue if there are less queues
	 * than maximum per core queues configured
	 */
	if (num_hw_queues < qdma_dev.max_hw_queues_per_core) {
		queue = alloc_hw_queue(lcore_id);
		if (queue) {
			core_info->hw_queues[num_hw_queues] = queue;
			core_info->num_hw_queues++;
			return queue;
		}
	}

	queue = core_info->hw_queues[0];
	/* In case there is no queue associated with the core return NULL */
	if (!queue)
		return NULL;

	/* Fetch the least loaded H/W queue */
	least_num_users = core_info->hw_queues[0]->num_users;
	for (i = 0; i < num_hw_queues; i++) {
		temp = core_info->hw_queues[i];
		if (temp->num_users < least_num_users)
			queue = temp;
	}

	if (queue)
		queue->num_users++;

	return queue;
}

static void
put_hw_queue(struct qdma_hw_queue *queue)
{
	struct qdma_per_core_info *core_info;
	int lcore_id, num_hw_queues, i;

	DPAA2_QDMA_FUNC_TRACE();

	/*
	 * If this is the last user of the queue free it.
	 * Also remove it from QDMA core info.
	 */
	if (queue->num_users == 1) {
		free_hw_queue(queue);

		/* Remove the physical queue from core info */
		lcore_id = queue->lcore_id;
		core_info = &qdma_core_info[lcore_id];
		num_hw_queues = core_info->num_hw_queues;
		for (i = 0; i < num_hw_queues; i++) {
			if (queue == core_info->hw_queues[i])
				break;
		}
		for (; i < num_hw_queues - 1; i++)
			core_info->hw_queues[i] = core_info->hw_queues[i + 1];
		core_info->hw_queues[i] = NULL;
	} else {
		queue->num_users--;
	}
}

int
rte_qdma_init(void)
{
	DPAA2_QDMA_FUNC_TRACE();

	rte_spinlock_init(&qdma_dev.lock);

	return 0;
}

void
rte_qdma_attr_get(struct rte_qdma_attr *qdma_attr)
{
	DPAA2_QDMA_FUNC_TRACE();

	qdma_attr->num_hw_queues = qdma_dev.num_hw_queues;
}

int
rte_qdma_reset(void)
{
	struct qdma_hw_queue *queue;
	int i;

	DPAA2_QDMA_FUNC_TRACE();

	/* In case QDMA device is not in stopped state, return -EBUSY */
	if (qdma_dev.state == 1) {
		DPAA2_QDMA_ERR(
			"Device is in running state. Stop before reset.");
		return -EBUSY;
	}

	/* In case there are pending jobs on any VQ, return -EBUSY */
	for (i = 0; i < qdma_dev.max_vqs; i++) {
		if (qdma_vqs[i].in_use && (qdma_vqs[i].num_enqueues !=
		    qdma_vqs[i].num_dequeues))
			DPAA2_QDMA_ERR("Jobs are still pending on VQ: %d", i);
			return -EBUSY;
	}

	/* Reset HW queues */
	TAILQ_FOREACH(queue, &qdma_queue_list, next)
		queue->num_users = 0;

	/* Reset and free virtual queues */
	for (i = 0; i < qdma_dev.max_vqs; i++) {
		if (qdma_vqs[i].status_ring)
			rte_ring_free(qdma_vqs[i].status_ring);
	}
	if (qdma_vqs)
		rte_free(qdma_vqs);
	qdma_vqs = NULL;

	/* Reset per core info */
	memset(&qdma_core_info, 0,
		sizeof(struct qdma_per_core_info) * RTE_MAX_LCORE);

	/* Free the FLE pool */
	if (qdma_dev.fle_pool)
		rte_mempool_free(qdma_dev.fle_pool);

	/* Reset QDMA device structure */
	qdma_dev.mode = RTE_QDMA_MODE_HW;
	qdma_dev.max_hw_queues_per_core = 0;
	qdma_dev.fle_pool = NULL;
	qdma_dev.fle_pool_count = 0;
	qdma_dev.max_vqs = 0;

	return 0;
}

int
rte_qdma_configure(struct rte_qdma_config *qdma_config)
{
	int ret;
	char fle_pool_name[32]; /* RTE_MEMZONE_NAMESIZE = 32 */

	DPAA2_QDMA_FUNC_TRACE();

	/* In case QDMA device is not in stopped state, return -EBUSY */
	if (qdma_dev.state == 1) {
		DPAA2_QDMA_ERR(
			"Device is in running state. Stop before config.");
		return -1;
	}

	/* Reset the QDMA device */
	ret = rte_qdma_reset();
	if (ret) {
		DPAA2_QDMA_ERR("Resetting QDMA failed");
		return ret;
	}

	/* Set mode */
	qdma_dev.mode = qdma_config->mode;

	/* Set max HW queue per core */
	if (qdma_config->max_hw_queues_per_core > MAX_HW_QUEUE_PER_CORE) {
		DPAA2_QDMA_ERR("H/W queues per core is more than: %d",
			       MAX_HW_QUEUE_PER_CORE);
		return -EINVAL;
	}
	qdma_dev.max_hw_queues_per_core =
		qdma_config->max_hw_queues_per_core;

	/* Allocate Virtual Queues */
	qdma_vqs = rte_malloc("qdma_virtual_queues",
			(sizeof(struct qdma_virt_queue) * qdma_config->max_vqs),
			RTE_CACHE_LINE_SIZE);
	if (!qdma_vqs) {
		DPAA2_QDMA_ERR("qdma_virtual_queues allocation failed");
		return -ENOMEM;
	}
	qdma_dev.max_vqs = qdma_config->max_vqs;

	/* Allocate FLE pool; just append PID so that in case of
	 * multiprocess, the pool's don't collide.
	 */
	snprintf(fle_pool_name, sizeof(fle_pool_name), "qdma_fle_pool%u",
		 getpid());
	qdma_dev.fle_pool = rte_mempool_create(fle_pool_name,
			qdma_config->fle_pool_count, QDMA_FLE_POOL_SIZE,
			QDMA_FLE_CACHE_SIZE(qdma_config->fle_pool_count), 0,
			NULL, NULL, NULL, NULL, SOCKET_ID_ANY, 0);
	if (!qdma_dev.fle_pool) {
		DPAA2_QDMA_ERR("qdma_fle_pool create failed");
		rte_free(qdma_vqs);
		qdma_vqs = NULL;
		return -ENOMEM;
	}
	qdma_dev.fle_pool_count = qdma_config->fle_pool_count;

	if (qdma_config->format == RTE_QDMA_ULTRASHORT_FORMAT) {
		dpdmai_dev_get_job = dpdmai_dev_get_job_us;
		dpdmai_dev_set_fd = dpdmai_dev_set_fd_us;
	} else {
		dpdmai_dev_get_job = dpdmai_dev_get_job_lf;
		dpdmai_dev_set_fd = dpdmai_dev_set_fd_lf;
	}
	return 0;
}

int
rte_qdma_start(void)
{
	DPAA2_QDMA_FUNC_TRACE();

	qdma_dev.state = 1;

	return 0;
}

int
rte_qdma_vq_create(uint32_t lcore_id, uint32_t flags)
{
	char ring_name[32];
	int i;

	DPAA2_QDMA_FUNC_TRACE();

	rte_spinlock_lock(&qdma_dev.lock);

	/* Get a free Virtual Queue */
	for (i = 0; i < qdma_dev.max_vqs; i++) {
		if (qdma_vqs[i].in_use == 0)
			break;
	}

	/* Return in case no VQ is free */
	if (i == qdma_dev.max_vqs) {
		rte_spinlock_unlock(&qdma_dev.lock);
		DPAA2_QDMA_ERR("Unable to get lock on QDMA device");
		return -ENODEV;
	}

	if (qdma_dev.mode == RTE_QDMA_MODE_HW ||
			(flags & RTE_QDMA_VQ_EXCLUSIVE_PQ)) {
		/* Allocate HW queue for a VQ */
		qdma_vqs[i].hw_queue = alloc_hw_queue(lcore_id);
		qdma_vqs[i].exclusive_hw_queue = 1;
	} else {
		/* Allocate a Ring for Virutal Queue in VQ mode */
		snprintf(ring_name, sizeof(ring_name), "status ring %d", i);
		qdma_vqs[i].status_ring = rte_ring_create(ring_name,
			qdma_dev.fle_pool_count, rte_socket_id(), 0);
		if (!qdma_vqs[i].status_ring) {
			DPAA2_QDMA_ERR("Status ring creation failed for vq");
			rte_spinlock_unlock(&qdma_dev.lock);
			return rte_errno;
		}

		/* Get a HW queue (shared) for a VQ */
		qdma_vqs[i].hw_queue = get_hw_queue(lcore_id);
		qdma_vqs[i].exclusive_hw_queue = 0;
	}

	if (qdma_vqs[i].hw_queue == NULL) {
		DPAA2_QDMA_ERR("No H/W queue available for VQ");
		if (qdma_vqs[i].status_ring)
			rte_ring_free(qdma_vqs[i].status_ring);
		qdma_vqs[i].status_ring = NULL;
		rte_spinlock_unlock(&qdma_dev.lock);
		return -ENODEV;
	}

	qdma_vqs[i].in_use = 1;
	qdma_vqs[i].lcore_id = lcore_id;
	memset(&qdma_vqs[i].rbp, 0, sizeof(struct rte_qdma_rbp));
	rte_spinlock_unlock(&qdma_dev.lock);

	return i;
}

/*create vq for route-by-port*/
int
rte_qdma_vq_create_rbp(uint32_t lcore_id, uint32_t flags,
			struct rte_qdma_rbp *rbp)
{
	int i;

	i = rte_qdma_vq_create(lcore_id, flags);

	memcpy(&qdma_vqs[i].rbp, rbp, sizeof(struct rte_qdma_rbp));

	return i;
}

static int
dpdmai_dev_enqueue_multi(struct dpaa2_dpdmai_dev *dpdmai_dev,
			uint16_t txq_id,
			uint16_t vq_id,
			struct rte_qdma_rbp *rbp,
			struct rte_qdma_job **job,
			uint16_t nb_jobs)
{
	struct qbman_fd fd[RTE_QDMA_BURST_NB_MAX];
	struct dpaa2_queue *txq;
	struct qbman_eq_desc eqdesc;
	struct qbman_swp *swp;
	int ret;
	uint32_t num_to_send = 0;
	uint16_t num_tx = 0;

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

	txq = &(dpdmai_dev->tx_queue[txq_id]);

	/* Prepare enqueue descriptor */
	qbman_eq_desc_clear(&eqdesc);
	qbman_eq_desc_set_fq(&eqdesc, txq->fqid);
	qbman_eq_desc_set_no_orp(&eqdesc, 0);
	qbman_eq_desc_set_response(&eqdesc, 0, 0);

	memset(fd, 0, RTE_QDMA_BURST_NB_MAX * sizeof(struct qbman_fd));

	while (nb_jobs > 0) {
		uint32_t loop;

		num_to_send = (nb_jobs > dpaa2_eqcr_size) ?
			dpaa2_eqcr_size : nb_jobs;

		for (loop = 0; loop < num_to_send; loop++) {
			ret = dpdmai_dev_set_fd(&fd[loop],
						job[num_tx], rbp, vq_id);
			if (ret < 0) {
				/* Set nb_jobs to loop, so outer while loop
				 * breaks out.
				 */
				nb_jobs = loop;
				break;
			}

			num_tx++;
		}

		/* Enqueue the packet to the QBMAN */
		uint32_t enqueue_loop = 0, retry_count = 0;
		while (enqueue_loop < loop) {
			ret = qbman_swp_enqueue_multiple(swp,
						&eqdesc,
						&fd[enqueue_loop],
						NULL,
						loop - enqueue_loop);
			if (unlikely(ret < 0)) {
				retry_count++;
				if (retry_count > DPAA2_MAX_TX_RETRY_COUNT)
					return num_tx - (loop - enqueue_loop);
			} else {
				enqueue_loop += ret;
				retry_count = 0;
			}
		}
		nb_jobs -= loop;
	}
	return num_tx;
}

int
rte_qdma_vq_enqueue_multi(uint16_t vq_id,
			  struct rte_qdma_job **job,
			  uint16_t nb_jobs)
{
	struct qdma_virt_queue *qdma_vq = &qdma_vqs[vq_id];
	struct qdma_hw_queue *qdma_pq = qdma_vq->hw_queue;
	struct dpaa2_dpdmai_dev *dpdmai_dev = qdma_pq->dpdmai_dev;
	int ret;

	/* Return error in case of wrong lcore_id */
	if (rte_lcore_id() != qdma_vq->lcore_id) {
		DPAA2_QDMA_ERR("QDMA enqueue for vqid %d on wrong core",
				vq_id);
		return -EINVAL;
	}

	ret = dpdmai_dev_enqueue_multi(dpdmai_dev,
				 qdma_pq->queue_id,
				 vq_id,
				 &qdma_vq->rbp,
				 job,
				 nb_jobs);
	if (ret < 0) {
		DPAA2_QDMA_ERR("DPDMAI device enqueue failed: %d", ret);
		return ret;
	}

	qdma_vq->num_enqueues += ret;

	return ret;
}

int
rte_qdma_vq_enqueue(uint16_t vq_id,
		    struct rte_qdma_job *job)
{
	return rte_qdma_vq_enqueue_multi(vq_id, &job, 1);
}

/* Function to receive a QDMA job for a given device and queue*/
static int
dpdmai_dev_dequeue_multijob_prefetch(
			struct dpaa2_dpdmai_dev *dpdmai_dev,
			uint16_t rxq_id,
			uint16_t *vq_id,
			struct rte_qdma_job **job,
			uint16_t nb_jobs)
{
	struct dpaa2_queue *rxq;
	struct qbman_result *dq_storage, *dq_storage1 = NULL;
	struct qbman_pull_desc pulldesc;
	struct qbman_swp *swp;
	struct queue_storage_info_t *q_storage;
	uint32_t fqid;
	uint8_t status, pending;
	uint8_t num_rx = 0;
	const struct qbman_fd *fd;
	uint16_t vqid;
	int ret, pull_size;

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
	rxq = &(dpdmai_dev->rx_queue[rxq_id]);
	fqid = rxq->fqid;
	q_storage = rxq->q_storage;

	if (unlikely(!q_storage->active_dqs)) {
		q_storage->toggle = 0;
		dq_storage = q_storage->dq_storage[q_storage->toggle];
		q_storage->last_num_pkts = pull_size;
		qbman_pull_desc_clear(&pulldesc);
		qbman_pull_desc_set_numframes(&pulldesc,
					      q_storage->last_num_pkts);
		qbman_pull_desc_set_fq(&pulldesc, fqid);
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
	qbman_pull_desc_set_fq(&pulldesc, fqid);
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

		vqid = dpdmai_dev_get_job(fd, &job[num_rx]);
		if (vq_id)
			vq_id[num_rx] = vqid;

		dq_storage++;
		num_rx++;
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
			DPAA2_QDMA_DP_WARN("VDQ command is not issued."
					  "QBMAN is busy (2)\n");
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
dpdmai_dev_dequeue_multijob_no_prefetch(
		struct dpaa2_dpdmai_dev *dpdmai_dev,
		uint16_t rxq_id,
		uint16_t *vq_id,
		struct rte_qdma_job **job,
		uint16_t nb_jobs)
{
	struct dpaa2_queue *rxq;
	struct qbman_result *dq_storage;
	struct qbman_pull_desc pulldesc;
	struct qbman_swp *swp;
	uint32_t fqid;
	uint8_t status, pending;
	uint8_t num_rx = 0;
	const struct qbman_fd *fd;
	uint16_t vqid;
	int ret, next_pull = nb_jobs, num_pulled = 0;

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

	rxq = &(dpdmai_dev->rx_queue[rxq_id]);
	fqid = rxq->fqid;

	do {
		dq_storage = rxq->q_storage->dq_storage[0];
		/* Prepare dequeue descriptor */
		qbman_pull_desc_clear(&pulldesc);
		qbman_pull_desc_set_fq(&pulldesc, fqid);
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
				DPAA2_QDMA_DP_WARN("VDQ command not issued. QBMAN busy");
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

			vqid = dpdmai_dev_get_job(fd, &job[num_rx]);
			if (vq_id)
				vq_id[num_rx] = vqid;

			dq_storage++;
			num_rx++;
			num_pulled++;

		} while (pending);
	/* Last VDQ provided all packets and more packets are requested */
	} while (next_pull && num_pulled == dpaa2_dqrr_size);

	return num_rx;
}

int
rte_qdma_vq_dequeue_multi(uint16_t vq_id,
			  struct rte_qdma_job **job,
			  uint16_t nb_jobs)
{
	struct qdma_virt_queue *qdma_vq = &qdma_vqs[vq_id];
	struct qdma_hw_queue *qdma_pq = qdma_vq->hw_queue;
	struct qdma_virt_queue *temp_qdma_vq;
	struct dpaa2_dpdmai_dev *dpdmai_dev = qdma_pq->dpdmai_dev;
	int ring_count, ret = 0, i;

	/* Return error in case of wrong lcore_id */
	if (rte_lcore_id() != (unsigned int)(qdma_vq->lcore_id)) {
		DPAA2_QDMA_WARN("QDMA dequeue for vqid %d on wrong core",
				vq_id);
		return -1;
	}

	/* Only dequeue when there are pending jobs on VQ */
	if (qdma_vq->num_enqueues == qdma_vq->num_dequeues)
		return 0;

	if (qdma_vq->num_enqueues < (qdma_vq->num_dequeues + nb_jobs))
		nb_jobs = (qdma_vq->num_enqueues -  qdma_vq->num_dequeues);

	if (qdma_vq->exclusive_hw_queue) {
		/* In case of exclusive queue directly fetch from HW queue */
		ret = dpdmai_dev_dequeue_multijob(dpdmai_dev, qdma_pq->queue_id,
					 NULL, job, nb_jobs);
		if (ret < 0) {
			DPAA2_QDMA_ERR(
				"Dequeue from DPDMAI device failed: %d", ret);
			return ret;
		}
		qdma_vq->num_dequeues += ret;
	} else {
		uint16_t temp_vq_id[RTE_QDMA_BURST_NB_MAX];
		/*
		 * Get the QDMA completed jobs from the software ring.
		 * In case they are not available on the ring poke the HW
		 * to fetch completed jobs from corresponding HW queues
		 */
		ring_count = rte_ring_count(qdma_vq->status_ring);
		if (ring_count < nb_jobs) {
			/* TODO - How to have right budget */
			ret = dpdmai_dev_dequeue_multijob(dpdmai_dev,
					qdma_pq->queue_id,
					temp_vq_id, job, nb_jobs);
			for (i = 0; i < ret; i++) {
				temp_qdma_vq = &qdma_vqs[temp_vq_id[i]];
				rte_ring_enqueue(temp_qdma_vq->status_ring,
					(void *)(job[i]));
			}
			ring_count = rte_ring_count(
					qdma_vq->status_ring);
		}

		if (ring_count) {
			/* Dequeue job from the software ring
			 * to provide to the user
			 */
			ret = rte_ring_dequeue_bulk(qdma_vq->status_ring,
					(void **)job, ring_count, NULL);
			if (ret)
				qdma_vq->num_dequeues += ret;
		}
	}

	return ret;
}

struct rte_qdma_job *
rte_qdma_vq_dequeue(uint16_t vq_id)
{
	int ret;
	struct rte_qdma_job *job = NULL;

	ret = rte_qdma_vq_dequeue_multi(vq_id, &job, 1);
	if (ret < 0)
		DPAA2_QDMA_DP_WARN("DPDMAI device dequeue failed: %d", ret);

	return job;
}

void
rte_qdma_vq_stats(uint16_t vq_id,
		  struct rte_qdma_vq_stats *vq_status)
{
	struct qdma_virt_queue *qdma_vq = &qdma_vqs[vq_id];

	if (qdma_vq->in_use) {
		vq_status->exclusive_hw_queue = qdma_vq->exclusive_hw_queue;
		vq_status->lcore_id = qdma_vq->lcore_id;
		vq_status->num_enqueues = qdma_vq->num_enqueues;
		vq_status->num_dequeues = qdma_vq->num_dequeues;
		vq_status->num_pending_jobs = vq_status->num_enqueues -
				vq_status->num_dequeues;
	}
}

int
rte_qdma_vq_destroy(uint16_t vq_id)
{
	struct qdma_virt_queue *qdma_vq = &qdma_vqs[vq_id];

	DPAA2_QDMA_FUNC_TRACE();

	/* In case there are pending jobs on any VQ, return -EBUSY */
	if (qdma_vq->num_enqueues != qdma_vq->num_dequeues)
		return -EBUSY;

	rte_spinlock_lock(&qdma_dev.lock);

	if (qdma_vq->exclusive_hw_queue)
		free_hw_queue(qdma_vq->hw_queue);
	else {
		if (qdma_vqs->status_ring)
			rte_ring_free(qdma_vqs->status_ring);

		put_hw_queue(qdma_vq->hw_queue);
	}

	memset(qdma_vq, 0, sizeof(struct qdma_virt_queue));

	rte_spinlock_unlock(&qdma_dev.lock);

	return 0;
}

int
rte_qdma_vq_destroy_rbp(uint16_t vq_id)
{
	struct qdma_virt_queue *qdma_vq = &qdma_vqs[vq_id];

	DPAA2_QDMA_FUNC_TRACE();

	/* In case there are pending jobs on any VQ, return -EBUSY */
	if (qdma_vq->num_enqueues != qdma_vq->num_dequeues)
		return -EBUSY;

	rte_spinlock_lock(&qdma_dev.lock);

	if (qdma_vq->exclusive_hw_queue) {
		free_hw_queue(qdma_vq->hw_queue);
	} else {
		if (qdma_vqs->status_ring)
			rte_ring_free(qdma_vqs->status_ring);

		put_hw_queue(qdma_vq->hw_queue);
	}

	memset(qdma_vq, 0, sizeof(struct qdma_virt_queue));

	rte_spinlock_unlock(&qdma_dev.lock);

	return 0;
}

void
rte_qdma_stop(void)
{
	DPAA2_QDMA_FUNC_TRACE();

	qdma_dev.state = 0;
}

void
rte_qdma_destroy(void)
{
	DPAA2_QDMA_FUNC_TRACE();

	rte_qdma_reset();
}

static const struct rte_rawdev_ops dpaa2_qdma_ops;

static int
add_hw_queues_to_list(struct dpaa2_dpdmai_dev *dpdmai_dev)
{
	struct qdma_hw_queue *queue;
	int i;

	DPAA2_QDMA_FUNC_TRACE();

	for (i = 0; i < dpdmai_dev->num_queues; i++) {
		queue = rte_zmalloc(NULL, sizeof(struct qdma_hw_queue), 0);
		if (!queue) {
			DPAA2_QDMA_ERR(
				"Memory allocation failed for QDMA queue");
			return -ENOMEM;
		}

		queue->dpdmai_dev = dpdmai_dev;
		queue->queue_id = i;

		TAILQ_INSERT_TAIL(&qdma_queue_list, queue, next);
		qdma_dev.num_hw_queues++;
	}

	return 0;
}

static void
remove_hw_queues_from_list(struct dpaa2_dpdmai_dev *dpdmai_dev)
{
	struct qdma_hw_queue *queue = NULL;
	struct qdma_hw_queue *tqueue = NULL;

	DPAA2_QDMA_FUNC_TRACE();

	TAILQ_FOREACH_SAFE(queue, &qdma_queue_list, next, tqueue) {
		if (queue->dpdmai_dev == dpdmai_dev) {
			TAILQ_REMOVE(&qdma_queue_list, queue, next);
			rte_free(queue);
			queue = NULL;
		}
	}
}

static int
dpaa2_dpdmai_dev_uninit(struct rte_rawdev *rawdev)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = rawdev->dev_private;
	int ret, i;

	DPAA2_QDMA_FUNC_TRACE();

	/* Remove HW queues from global list */
	remove_hw_queues_from_list(dpdmai_dev);

	ret = dpdmai_disable(&dpdmai_dev->dpdmai, CMD_PRI_LOW,
			     dpdmai_dev->token);
	if (ret)
		DPAA2_QDMA_ERR("dmdmai disable failed");

	/* Set up the DQRR storage for Rx */
	for (i = 0; i < dpdmai_dev->num_queues; i++) {
		struct dpaa2_queue *rxq = &(dpdmai_dev->rx_queue[i]);

		if (rxq->q_storage) {
			dpaa2_free_dq_storage(rxq->q_storage);
			rte_free(rxq->q_storage);
		}
	}

	/* Close the device at underlying layer*/
	ret = dpdmai_close(&dpdmai_dev->dpdmai, CMD_PRI_LOW, dpdmai_dev->token);
	if (ret)
		DPAA2_QDMA_ERR("Failure closing dpdmai device");

	return 0;
}

static int
check_devargs_handler(__rte_unused const char *key, const char *value,
		      __rte_unused void *opaque)
{
	if (strcmp(value, "1"))
		return -1;

	return 0;
}

static int
dpaa2_get_devargs(struct rte_devargs *devargs, const char *key)
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

static int
dpaa2_dpdmai_dev_init(struct rte_rawdev *rawdev, int dpdmai_id)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = rawdev->dev_private;
	struct dpdmai_rx_queue_cfg rx_queue_cfg;
	struct dpdmai_attr attr;
	struct dpdmai_rx_queue_attr rx_attr;
	struct dpdmai_tx_queue_attr tx_attr;
	int ret, i;

	DPAA2_QDMA_FUNC_TRACE();

	/* Open DPDMAI device */
	dpdmai_dev->dpdmai_id = dpdmai_id;
	dpdmai_dev->dpdmai.regs = dpaa2_get_mcp_ptr(MC_PORTAL_INDEX);
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

	/* Set up Rx Queues */
	for (i = 0; i < dpdmai_dev->num_queues; i++) {
		struct dpaa2_queue *rxq;

		memset(&rx_queue_cfg, 0, sizeof(struct dpdmai_rx_queue_cfg));
		ret = dpdmai_set_rx_queue(&dpdmai_dev->dpdmai,
					  CMD_PRI_LOW,
					  dpdmai_dev->token,
					  i, 0, &rx_queue_cfg);
		if (ret) {
			DPAA2_QDMA_ERR("Setting Rx queue failed with err: %d",
				       ret);
			goto init_err;
		}

		/* Allocate DQ storage for the DPDMAI Rx queues */
		rxq = &(dpdmai_dev->rx_queue[i]);
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
	}

	/* Get Rx and Tx queues FQID's */
	for (i = 0; i < dpdmai_dev->num_queues; i++) {
		ret = dpdmai_get_rx_queue(&dpdmai_dev->dpdmai, CMD_PRI_LOW,
					  dpdmai_dev->token, i, 0, &rx_attr);
		if (ret) {
			DPAA2_QDMA_ERR("Reading device failed with err: %d",
				       ret);
			goto init_err;
		}
		dpdmai_dev->rx_queue[i].fqid = rx_attr.fqid;

		ret = dpdmai_get_tx_queue(&dpdmai_dev->dpdmai, CMD_PRI_LOW,
					  dpdmai_dev->token, i, 0, &tx_attr);
		if (ret) {
			DPAA2_QDMA_ERR("Reading device failed with err: %d",
				       ret);
			goto init_err;
		}
		dpdmai_dev->tx_queue[i].fqid = tx_attr.fqid;
	}

	/* Enable the device */
	ret = dpdmai_enable(&dpdmai_dev->dpdmai, CMD_PRI_LOW,
			    dpdmai_dev->token);
	if (ret) {
		DPAA2_QDMA_ERR("Enabling device failed with err: %d", ret);
		goto init_err;
	}

	/* Add the HW queue to the global list */
	ret = add_hw_queues_to_list(dpdmai_dev);
	if (ret) {
		DPAA2_QDMA_ERR("Adding H/W queue to list failed");
		goto init_err;
	}

	if (dpaa2_get_devargs(rawdev->device->devargs,
		DPAA2_QDMA_NO_PREFETCH)) {
		/* If no prefetch is configured. */
		dpdmai_dev_dequeue_multijob =
				dpdmai_dev_dequeue_multijob_no_prefetch;
		DPAA2_QDMA_INFO("No Prefetch RX Mode enabled");
	} else {
		dpdmai_dev_dequeue_multijob =
			dpdmai_dev_dequeue_multijob_prefetch;
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

	return 0;
init_err:
	dpaa2_dpdmai_dev_uninit(rawdev);
	return ret;
}

static int
rte_dpaa2_qdma_probe(struct rte_dpaa2_driver *dpaa2_drv,
		     struct rte_dpaa2_device *dpaa2_dev)
{
	struct rte_rawdev *rawdev;
	int ret;

	DPAA2_QDMA_FUNC_TRACE();

	rawdev = rte_rawdev_pmd_allocate(dpaa2_dev->device.name,
			sizeof(struct dpaa2_dpdmai_dev),
			rte_socket_id());
	if (!rawdev) {
		DPAA2_QDMA_ERR("Unable to allocate rawdevice");
		return -EINVAL;
	}

	dpaa2_dev->rawdev = rawdev;
	rawdev->dev_ops = &dpaa2_qdma_ops;
	rawdev->device = &dpaa2_dev->device;
	rawdev->driver_name = dpaa2_drv->driver.name;

	/* Invoke PMD device initialization function */
	ret = dpaa2_dpdmai_dev_init(rawdev, dpaa2_dev->object_id);
	if (ret) {
		rte_rawdev_pmd_release(rawdev);
		return ret;
	}

	return 0;
}

static int
rte_dpaa2_qdma_remove(struct rte_dpaa2_device *dpaa2_dev)
{
	struct rte_rawdev *rawdev = dpaa2_dev->rawdev;
	int ret;

	DPAA2_QDMA_FUNC_TRACE();

	dpaa2_dpdmai_dev_uninit(rawdev);

	ret = rte_rawdev_pmd_release(rawdev);
	if (ret)
		DPAA2_QDMA_ERR("Device cleanup failed");

	return 0;
}

static struct rte_dpaa2_driver rte_dpaa2_qdma_pmd = {
	.drv_flags = RTE_DPAA2_DRV_IOVA_AS_VA,
	.drv_type = DPAA2_QDMA,
	.probe = rte_dpaa2_qdma_probe,
	.remove = rte_dpaa2_qdma_remove,
};

RTE_PMD_REGISTER_DPAA2(dpaa2_qdma, rte_dpaa2_qdma_pmd);
RTE_PMD_REGISTER_PARAM_STRING(dpaa2_qdma,
	"no_prefetch=<int> ");
RTE_LOG_REGISTER(dpaa2_qdma_logtype, pmd.raw.dpaa2.qdma, INFO);
