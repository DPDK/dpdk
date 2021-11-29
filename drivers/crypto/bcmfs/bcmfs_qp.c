/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Broadcom.
 * All rights reserved.
 */

#include <inttypes.h>

#include <rte_atomic.h>
#include <rte_bitmap.h>
#include <rte_common.h>
#include <rte_dev.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_prefetch.h>
#include <rte_string_fns.h>

#include "bcmfs_logs.h"
#include "bcmfs_qp.h"
#include "bcmfs_hw_defs.h"

/* TX or submission queue name */
static const char *txq_name = "tx";
/* Completion or receive queue name */
static const char *cmplq_name = "cmpl";

/* Helper function */
static int
bcmfs_qp_check_queue_alignment(uint64_t phys_addr,
			       uint32_t align)
{
	if (((align - 1) & phys_addr) != 0)
		return -EINVAL;
	return 0;
}

static void
bcmfs_queue_delete(struct bcmfs_queue *queue,
		   uint16_t queue_pair_id)
{
	const struct rte_memzone *mz;
	int status = 0;

	if (queue == NULL) {
		BCMFS_LOG(DEBUG, "Invalid queue");
		return;
	}
	BCMFS_LOG(DEBUG, "Free ring %d type %d, memzone: %s",
		  queue_pair_id, queue->q_type, queue->memz_name);

	mz = rte_memzone_lookup(queue->memz_name);
	if (mz != NULL)	{
		/* Write an unused pattern to the queue memory. */
		memset(queue->base_addr, 0x9B, queue->queue_size);
		status = rte_memzone_free(mz);
		if (status != 0)
			BCMFS_LOG(ERR, "Error %d on freeing queue %s",
					status, queue->memz_name);
	} else {
		BCMFS_LOG(DEBUG, "queue %s doesn't exist",
				queue->memz_name);
	}
}

static const struct rte_memzone *
queue_dma_zone_reserve(const char *queue_name, uint32_t queue_size,
		       int socket_id, unsigned int align)
{
	const struct rte_memzone *mz;

	mz = rte_memzone_lookup(queue_name);
	if (mz != NULL) {
		if (((size_t)queue_size <= mz->len) &&
		    (socket_id == SOCKET_ID_ANY ||
		     socket_id == mz->socket_id)) {
			BCMFS_LOG(DEBUG, "re-use memzone already "
					"allocated for %s", queue_name);
			return mz;
		}

		BCMFS_LOG(ERR, "Incompatible memzone already "
				"allocated %s, size %u, socket %d. "
				"Requested size %u, socket %u",
				queue_name, (uint32_t)mz->len,
				mz->socket_id, queue_size, socket_id);
		return NULL;
	}

	BCMFS_LOG(DEBUG, "Allocate memzone for %s, size %u on socket %u",
		  queue_name, queue_size, socket_id);
	return rte_memzone_reserve_aligned(queue_name, queue_size,
		socket_id, RTE_MEMZONE_IOVA_CONTIG, align);
}

static int
bcmfs_queue_create(struct bcmfs_queue *queue,
		   struct bcmfs_qp_config *qp_conf,
		   uint16_t queue_pair_id,
		   enum bcmfs_queue_type qtype)
{
	const struct rte_memzone *qp_mz;
	char q_name[16];
	unsigned int align;
	uint32_t queue_size_bytes;
	int ret;

	if (qtype == BCMFS_RM_TXQ) {
		strlcpy(q_name, txq_name, sizeof(q_name));
		align = 1U << FS_RING_BD_ALIGN_ORDER;
		queue_size_bytes = qp_conf->nb_descriptors *
				   qp_conf->max_descs_req * FS_RING_DESC_SIZE;
		queue_size_bytes = RTE_ALIGN_MUL_CEIL(queue_size_bytes,
						      FS_RING_PAGE_SIZE);
		/* make queue size to multiple for 4K pages */
	} else if (qtype == BCMFS_RM_CPLQ) {
		strlcpy(q_name, cmplq_name, sizeof(q_name));
		align = 1U << FS_RING_CMPL_ALIGN_ORDER;

		/*
		 * Memory size for cmpl + MSI
		 * For MSI allocate here itself and so we allocate twice
		 */
		queue_size_bytes = 2 * FS_RING_CMPL_SIZE;
	} else {
		BCMFS_LOG(ERR, "Invalid queue selection");
		return -EINVAL;
	}

	queue->q_type = qtype;

	/*
	 * Allocate a memzone for the queue - create a unique name.
	 */
	snprintf(queue->memz_name, sizeof(queue->memz_name),
		 "%s_%d_%s_%d_%s", "bcmfs", qtype, "qp_mem",
		 queue_pair_id, q_name);
	qp_mz = queue_dma_zone_reserve(queue->memz_name, queue_size_bytes,
				       0, align);
	if (qp_mz == NULL) {
		BCMFS_LOG(ERR, "Failed to allocate ring memzone");
		return -ENOMEM;
	}

	if (bcmfs_qp_check_queue_alignment(qp_mz->iova, align)) {
		BCMFS_LOG(ERR, "Invalid alignment on queue create "
					" 0x%" PRIx64 "\n",
					queue->base_phys_addr);
		ret = -EFAULT;
		goto queue_create_err;
	}

	queue->base_addr = (char *)qp_mz->addr;
	queue->base_phys_addr = qp_mz->iova;
	queue->queue_size = queue_size_bytes;

	return 0;

queue_create_err:
	rte_memzone_free(qp_mz);

	return ret;
}

int
bcmfs_qp_release(struct bcmfs_qp **qp_addr)
{
	struct bcmfs_qp *qp = *qp_addr;

	if (qp == NULL) {
		BCMFS_LOG(DEBUG, "qp already freed");
		return 0;
	}

	/* Don't free memory if there are still responses to be processed */
	if ((qp->stats.enqueued_count - qp->stats.dequeued_count) == 0) {
		/* Stop the h/w ring */
		qp->ops->stopq(qp);
		/* Delete the queue pairs */
		bcmfs_queue_delete(&qp->tx_q, qp->qpair_id);
		bcmfs_queue_delete(&qp->cmpl_q, qp->qpair_id);
	} else {
		return -EAGAIN;
	}

	rte_bitmap_reset(qp->ctx_bmp);
	rte_free(qp->ctx_bmp_mem);
	rte_free(qp->ctx_pool);

	rte_free(qp);
	*qp_addr = NULL;

	return 0;
}

int
bcmfs_qp_setup(struct bcmfs_qp **qp_addr,
	       uint16_t queue_pair_id,
	       struct bcmfs_qp_config *qp_conf)
{
	struct bcmfs_qp *qp;
	uint32_t bmp_size;
	uint32_t nb_descriptors = qp_conf->nb_descriptors;
	uint16_t i;
	int rc;

	if (nb_descriptors < FS_RM_MIN_REQS) {
		BCMFS_LOG(ERR, "Can't create qp for %u descriptors",
			  nb_descriptors);
		return -EINVAL;
	}

	if (nb_descriptors > FS_RM_MAX_REQS)
		nb_descriptors = FS_RM_MAX_REQS;

	if (qp_conf->iobase == NULL) {
		BCMFS_LOG(ERR, "IO config space null");
		return -EINVAL;
	}

	qp = rte_zmalloc_socket("BCM FS PMD qp metadata",
				sizeof(*qp), RTE_CACHE_LINE_SIZE,
				qp_conf->socket_id);
	if (qp == NULL) {
		BCMFS_LOG(ERR, "Failed to alloc mem for qp struct");
		return -ENOMEM;
	}

	qp->qpair_id = queue_pair_id;
	qp->ioreg = qp_conf->iobase;
	qp->nb_descriptors = nb_descriptors;
	qp->ops = qp_conf->ops;

	qp->stats.enqueued_count = 0;
	qp->stats.dequeued_count = 0;

	rc = bcmfs_queue_create(&qp->tx_q, qp_conf, qp->qpair_id,
				BCMFS_RM_TXQ);
	if (rc) {
		BCMFS_LOG(ERR, "Tx queue create failed queue_pair_id %u",
			  queue_pair_id);
		goto create_err;
	}

	rc = bcmfs_queue_create(&qp->cmpl_q, qp_conf, qp->qpair_id,
				BCMFS_RM_CPLQ);
	if (rc) {
		BCMFS_LOG(ERR, "Cmpl queue create failed queue_pair_id= %u",
			  queue_pair_id);
		goto q_create_err;
	}

	/* ctx saving bitmap */
	bmp_size = rte_bitmap_get_memory_footprint(nb_descriptors);

	/* Allocate memory for bitmap */
	qp->ctx_bmp_mem = rte_zmalloc("ctx_bmp_mem", bmp_size,
				      RTE_CACHE_LINE_SIZE);
	if (qp->ctx_bmp_mem == NULL) {
		rc = -ENOMEM;
		goto qp_create_err;
	}

	/* Initialize pool resource bitmap array */
	qp->ctx_bmp = rte_bitmap_init(nb_descriptors, qp->ctx_bmp_mem,
				      bmp_size);
	if (qp->ctx_bmp == NULL) {
		rc = -EINVAL;
		goto bmap_mem_free;
	}

	/* Mark all pools available */
	for (i = 0; i < nb_descriptors; i++)
		rte_bitmap_set(qp->ctx_bmp, i);

	/* Allocate memory for context */
	qp->ctx_pool = rte_zmalloc("qp_ctx_pool",
				   sizeof(unsigned long) *
				   nb_descriptors, 0);
	if (qp->ctx_pool == NULL) {
		BCMFS_LOG(ERR, "ctx allocation pool fails");
		rc = -ENOMEM;
		goto bmap_free;
	}

	/* Start h/w ring */
	qp->ops->startq(qp);

	*qp_addr = qp;

	return 0;

bmap_free:
	rte_bitmap_reset(qp->ctx_bmp);
bmap_mem_free:
	rte_free(qp->ctx_bmp_mem);
qp_create_err:
	bcmfs_queue_delete(&qp->cmpl_q, queue_pair_id);
q_create_err:
	bcmfs_queue_delete(&qp->tx_q, queue_pair_id);
create_err:
	rte_free(qp);

	return rc;
}

uint16_t
bcmfs_enqueue_op_burst(void *qp, void **ops, uint16_t nb_ops)
{
	struct bcmfs_qp *tmp_qp = (struct bcmfs_qp *)qp;
	register uint32_t nb_ops_sent = 0;
	uint16_t nb_ops_possible = nb_ops;
	int ret;

	if (unlikely(nb_ops == 0))
		return 0;

	while (nb_ops_sent != nb_ops_possible) {
		ret = tmp_qp->ops->enq_one_req(qp, *ops);
		if (ret != 0) {
			tmp_qp->stats.enqueue_err_count++;
			/* This message cannot be enqueued */
			if (nb_ops_sent == 0)
				return 0;
			goto ring_db;
		}

		ops++;
		nb_ops_sent++;
	}

ring_db:
	tmp_qp->stats.enqueued_count += nb_ops_sent;
	tmp_qp->ops->ring_db(tmp_qp);

	return nb_ops_sent;
}

uint16_t
bcmfs_dequeue_op_burst(void *qp, void **ops, uint16_t nb_ops)
{
	struct bcmfs_qp *tmp_qp = (struct bcmfs_qp *)qp;
	uint32_t deq = tmp_qp->ops->dequeue(tmp_qp, ops, nb_ops);

	tmp_qp->stats.dequeued_count += deq;

	return deq;
}

void bcmfs_qp_stats_get(struct bcmfs_qp **qp, int num_qp,
			struct bcmfs_qp_stats *stats)
{
	int i;

	if (stats == NULL) {
		BCMFS_LOG(ERR, "invalid param: stats %p",
			  stats);
		return;
	}

	for (i = 0; i < num_qp; i++) {
		if (qp[i] == NULL) {
			BCMFS_LOG(DEBUG, "Uninitialised qp %d", i);
			continue;
		}

		stats->enqueued_count += qp[i]->stats.enqueued_count;
		stats->dequeued_count += qp[i]->stats.dequeued_count;
		stats->enqueue_err_count += qp[i]->stats.enqueue_err_count;
		stats->dequeue_err_count += qp[i]->stats.dequeue_err_count;
	}
}

void bcmfs_qp_stats_reset(struct bcmfs_qp **qp, int num_qp)
{
	int i;

	for (i = 0; i < num_qp; i++) {
		if (qp[i] == NULL) {
			BCMFS_LOG(DEBUG, "Uninitialised qp %d", i);
			continue;
		}
		memset(&qp[i]->stats, 0, sizeof(qp[i]->stats));
	}
}
