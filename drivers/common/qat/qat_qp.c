/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2018 Intel Corporation
 */

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_dev.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_atomic.h>
#include <rte_prefetch.h>

#include "qat_logs.h"
#include "qat_device.h"
#include "qat_qp.h"
#include "qat_sym.h"
#include "qat_asym.h"
#include "qat_comp.h"
#include "adf_transport_access_macros.h"

#define QAT_CQ_MAX_DEQ_RETRIES 10

#define ADF_MAX_DESC				4096
#define ADF_MIN_DESC				128

#define ADF_ARB_REG_SLOT			0x1000
#define ADF_ARB_RINGSRVARBEN_OFFSET		0x19C

#define WRITE_CSR_ARB_RINGSRVARBEN(csr_addr, index, value) \
	ADF_CSR_WR(csr_addr, ADF_ARB_RINGSRVARBEN_OFFSET + \
	(ADF_ARB_REG_SLOT * index), value)

__extension__
const struct qat_qp_hw_data qat_gen1_qps[QAT_MAX_SERVICES]
					 [ADF_MAX_QPS_ON_ANY_SERVICE] = {
	/* queue pairs which provide an asymmetric crypto service */
	[QAT_SERVICE_ASYMMETRIC] = {
		{
			.service_type = QAT_SERVICE_ASYMMETRIC,
			.hw_bundle_num = 0,
			.tx_ring_num = 0,
			.rx_ring_num = 8,
			.tx_msg_size = 64,
			.rx_msg_size = 32,

		}, {
			.service_type = QAT_SERVICE_ASYMMETRIC,
			.hw_bundle_num = 0,
			.tx_ring_num = 1,
			.rx_ring_num = 9,
			.tx_msg_size = 64,
			.rx_msg_size = 32,
		}
	},
	/* queue pairs which provide a symmetric crypto service */
	[QAT_SERVICE_SYMMETRIC] = {
		{
			.service_type = QAT_SERVICE_SYMMETRIC,
			.hw_bundle_num = 0,
			.tx_ring_num = 2,
			.rx_ring_num = 10,
			.tx_msg_size = 128,
			.rx_msg_size = 32,
		},
		{
			.service_type = QAT_SERVICE_SYMMETRIC,
			.hw_bundle_num = 0,
			.tx_ring_num = 3,
			.rx_ring_num = 11,
			.tx_msg_size = 128,
			.rx_msg_size = 32,
		}
	},
	/* queue pairs which provide a compression service */
	[QAT_SERVICE_COMPRESSION] = {
		{
			.service_type = QAT_SERVICE_COMPRESSION,
			.hw_bundle_num = 0,
			.tx_ring_num = 6,
			.rx_ring_num = 14,
			.tx_msg_size = 128,
			.rx_msg_size = 32,
		}, {
			.service_type = QAT_SERVICE_COMPRESSION,
			.hw_bundle_num = 0,
			.tx_ring_num = 7,
			.rx_ring_num = 15,
			.tx_msg_size = 128,
			.rx_msg_size = 32,
		}
	}
};

__extension__
const struct qat_qp_hw_data qat_gen3_qps[QAT_MAX_SERVICES]
					 [ADF_MAX_QPS_ON_ANY_SERVICE] = {
	/* queue pairs which provide an asymmetric crypto service */
	[QAT_SERVICE_ASYMMETRIC] = {
		{
			.service_type = QAT_SERVICE_ASYMMETRIC,
			.hw_bundle_num = 0,
			.tx_ring_num = 0,
			.rx_ring_num = 4,
			.tx_msg_size = 64,
			.rx_msg_size = 32,
		}
	},
	/* queue pairs which provide a symmetric crypto service */
	[QAT_SERVICE_SYMMETRIC] = {
		{
			.service_type = QAT_SERVICE_SYMMETRIC,
			.hw_bundle_num = 0,
			.tx_ring_num = 1,
			.rx_ring_num = 5,
			.tx_msg_size = 128,
			.rx_msg_size = 32,
		}
	},
	/* queue pairs which provide a compression service */
	[QAT_SERVICE_COMPRESSION] = {
		{
			.service_type = QAT_SERVICE_COMPRESSION,
			.hw_bundle_num = 0,
			.tx_ring_num = 3,
			.rx_ring_num = 7,
			.tx_msg_size = 128,
			.rx_msg_size = 32,
		}
	}
};

static int qat_qp_check_queue_alignment(uint64_t phys_addr,
	uint32_t queue_size_bytes);
static void qat_queue_delete(struct qat_queue *queue);
static int qat_queue_create(struct qat_pci_device *qat_dev,
	struct qat_queue *queue, struct qat_qp_config *, uint8_t dir);
static int adf_verify_queue_size(uint32_t msg_size, uint32_t msg_num,
	uint32_t *queue_size_for_csr);
static void adf_configure_queues(struct qat_qp *queue);
static void adf_queue_arb_enable(struct qat_queue *txq, void *base_addr,
	rte_spinlock_t *lock);
static void adf_queue_arb_disable(struct qat_queue *txq, void *base_addr,
	rte_spinlock_t *lock);


int qat_qps_per_service(const struct qat_qp_hw_data *qp_hw_data,
		enum qat_service_type service)
{
	int i, count;

	for (i = 0, count = 0; i < ADF_MAX_QPS_ON_ANY_SERVICE; i++)
		if (qp_hw_data[i].service_type == service)
			count++;
	return count;
}

static const struct rte_memzone *
queue_dma_zone_reserve(const char *queue_name, uint32_t queue_size,
			int socket_id)
{
	const struct rte_memzone *mz;

	mz = rte_memzone_lookup(queue_name);
	if (mz != 0) {
		if (((size_t)queue_size <= mz->len) &&
				((socket_id == SOCKET_ID_ANY) ||
					(socket_id == mz->socket_id))) {
			QAT_LOG(DEBUG, "re-use memzone already "
					"allocated for %s", queue_name);
			return mz;
		}

		QAT_LOG(ERR, "Incompatible memzone already "
				"allocated %s, size %u, socket %d. "
				"Requested size %u, socket %u",
				queue_name, (uint32_t)mz->len,
				mz->socket_id, queue_size, socket_id);
		return NULL;
	}

	QAT_LOG(DEBUG, "Allocate memzone for %s, size %u on socket %u",
					queue_name, queue_size, socket_id);
	return rte_memzone_reserve_aligned(queue_name, queue_size,
		socket_id, RTE_MEMZONE_IOVA_CONTIG, queue_size);
}

int qat_qp_setup(struct qat_pci_device *qat_dev,
		struct qat_qp **qp_addr,
		uint16_t queue_pair_id,
		struct qat_qp_config *qat_qp_conf)

{
	struct qat_qp *qp;
	struct rte_pci_device *pci_dev =
			qat_pci_devs[qat_dev->qat_dev_id].pci_dev;
	char op_cookie_pool_name[RTE_RING_NAMESIZE];
	uint32_t i;

	QAT_LOG(DEBUG, "Setup qp %u on qat pci device %d gen %d",
		queue_pair_id, qat_dev->qat_dev_id, qat_dev->qat_dev_gen);

	if ((qat_qp_conf->nb_descriptors > ADF_MAX_DESC) ||
		(qat_qp_conf->nb_descriptors < ADF_MIN_DESC)) {
		QAT_LOG(ERR, "Can't create qp for %u descriptors",
				qat_qp_conf->nb_descriptors);
		return -EINVAL;
	}

	if (pci_dev->mem_resource[0].addr == NULL) {
		QAT_LOG(ERR, "Could not find VF config space "
				"(UIO driver attached?).");
		return -EINVAL;
	}

	/* Allocate the queue pair data structure. */
	qp = rte_zmalloc_socket("qat PMD qp metadata",
				sizeof(*qp), RTE_CACHE_LINE_SIZE,
				qat_qp_conf->socket_id);
	if (qp == NULL) {
		QAT_LOG(ERR, "Failed to alloc mem for qp struct");
		return -ENOMEM;
	}
	qp->nb_descriptors = qat_qp_conf->nb_descriptors;
	qp->op_cookies = rte_zmalloc_socket("qat PMD op cookie pointer",
			qat_qp_conf->nb_descriptors * sizeof(*qp->op_cookies),
			RTE_CACHE_LINE_SIZE, qat_qp_conf->socket_id);
	if (qp->op_cookies == NULL) {
		QAT_LOG(ERR, "Failed to alloc mem for cookie");
		rte_free(qp);
		return -ENOMEM;
	}

	qp->mmap_bar_addr = pci_dev->mem_resource[0].addr;
	qp->enqueued = qp->dequeued = 0;

	if (qat_queue_create(qat_dev, &(qp->tx_q), qat_qp_conf,
					ADF_RING_DIR_TX) != 0) {
		QAT_LOG(ERR, "Tx queue create failed "
				"queue_pair_id=%u", queue_pair_id);
		goto create_err;
	}

	qp->max_inflights = ADF_MAX_INFLIGHTS(qp->tx_q.queue_size,
				ADF_BYTES_TO_MSG_SIZE(qp->tx_q.msg_size));

	if (qp->max_inflights < 2) {
		QAT_LOG(ERR, "Invalid num inflights");
		qat_queue_delete(&(qp->tx_q));
		goto create_err;
	}

	if (qat_queue_create(qat_dev, &(qp->rx_q), qat_qp_conf,
					ADF_RING_DIR_RX) != 0) {
		QAT_LOG(ERR, "Rx queue create failed "
				"queue_pair_id=%hu", queue_pair_id);
		qat_queue_delete(&(qp->tx_q));
		goto create_err;
	}

	adf_configure_queues(qp);
	adf_queue_arb_enable(&qp->tx_q, qp->mmap_bar_addr,
					&qat_dev->arb_csr_lock);

	snprintf(op_cookie_pool_name, RTE_RING_NAMESIZE,
					"%s%d_cookies_%s_qp%hu",
		pci_dev->driver->driver.name, qat_dev->qat_dev_id,
		qat_qp_conf->service_str, queue_pair_id);

	QAT_LOG(DEBUG, "cookiepool: %s", op_cookie_pool_name);
	qp->op_cookie_pool = rte_mempool_lookup(op_cookie_pool_name);
	if (qp->op_cookie_pool == NULL)
		qp->op_cookie_pool = rte_mempool_create(op_cookie_pool_name,
				qp->nb_descriptors,
				qat_qp_conf->cookie_size, 64, 0,
				NULL, NULL, NULL, NULL,
				pci_dev->device.numa_node,
				0);
	if (!qp->op_cookie_pool) {
		QAT_LOG(ERR, "QAT PMD Cannot create"
				" op mempool");
		goto create_err;
	}

	for (i = 0; i < qp->nb_descriptors; i++) {
		if (rte_mempool_get(qp->op_cookie_pool, &qp->op_cookies[i])) {
			QAT_LOG(ERR, "QAT PMD Cannot get op_cookie");
			goto create_err;
		}
		memset(qp->op_cookies[i], 0, qat_qp_conf->cookie_size);
	}

	qp->qat_dev_gen = qat_dev->qat_dev_gen;
	qp->service_type = qat_qp_conf->hw->service_type;
	qp->qat_dev = qat_dev;

	QAT_LOG(DEBUG, "QP setup complete: id: %d, cookiepool: %s",
			queue_pair_id, op_cookie_pool_name);

	*qp_addr = qp;
	return 0;

create_err:
	if (qp->op_cookie_pool)
		rte_mempool_free(qp->op_cookie_pool);
	rte_free(qp->op_cookies);
	rte_free(qp);
	return -EFAULT;
}

int qat_qp_release(struct qat_qp **qp_addr)
{
	struct qat_qp *qp = *qp_addr;
	uint32_t i;

	if (qp == NULL) {
		QAT_LOG(DEBUG, "qp already freed");
		return 0;
	}

	QAT_LOG(DEBUG, "Free qp on qat_pci device %d",
				qp->qat_dev->qat_dev_id);

	/* Don't free memory if there are still responses to be processed */
	if ((qp->enqueued - qp->dequeued) == 0) {
		qat_queue_delete(&(qp->tx_q));
		qat_queue_delete(&(qp->rx_q));
	} else {
		return -EAGAIN;
	}

	adf_queue_arb_disable(&(qp->tx_q), qp->mmap_bar_addr,
					&qp->qat_dev->arb_csr_lock);

	for (i = 0; i < qp->nb_descriptors; i++)
		rte_mempool_put(qp->op_cookie_pool, qp->op_cookies[i]);

	if (qp->op_cookie_pool)
		rte_mempool_free(qp->op_cookie_pool);

	rte_free(qp->op_cookies);
	rte_free(qp);
	*qp_addr = NULL;
	return 0;
}


static void qat_queue_delete(struct qat_queue *queue)
{
	const struct rte_memzone *mz;
	int status = 0;

	if (queue == NULL) {
		QAT_LOG(DEBUG, "Invalid queue");
		return;
	}
	QAT_LOG(DEBUG, "Free ring %d, memzone: %s",
			queue->hw_queue_number, queue->memz_name);

	mz = rte_memzone_lookup(queue->memz_name);
	if (mz != NULL)	{
		/* Write an unused pattern to the queue memory. */
		memset(queue->base_addr, 0x7F, queue->queue_size);
		status = rte_memzone_free(mz);
		if (status != 0)
			QAT_LOG(ERR, "Error %d on freeing queue %s",
					status, queue->memz_name);
	} else {
		QAT_LOG(DEBUG, "queue %s doesn't exist",
				queue->memz_name);
	}
}

static int
qat_queue_create(struct qat_pci_device *qat_dev, struct qat_queue *queue,
		struct qat_qp_config *qp_conf, uint8_t dir)
{
	uint64_t queue_base;
	void *io_addr;
	const struct rte_memzone *qp_mz;
	struct rte_pci_device *pci_dev =
			qat_pci_devs[qat_dev->qat_dev_id].pci_dev;
	int ret = 0;
	uint16_t desc_size = (dir == ADF_RING_DIR_TX ?
			qp_conf->hw->tx_msg_size : qp_conf->hw->rx_msg_size);
	uint32_t queue_size_bytes = (qp_conf->nb_descriptors)*(desc_size);

	queue->hw_bundle_number = qp_conf->hw->hw_bundle_num;
	queue->hw_queue_number = (dir == ADF_RING_DIR_TX ?
			qp_conf->hw->tx_ring_num : qp_conf->hw->rx_ring_num);

	if (desc_size > ADF_MSG_SIZE_TO_BYTES(ADF_MAX_MSG_SIZE)) {
		QAT_LOG(ERR, "Invalid descriptor size %d", desc_size);
		return -EINVAL;
	}

	/*
	 * Allocate a memzone for the queue - create a unique name.
	 */
	snprintf(queue->memz_name, sizeof(queue->memz_name),
			"%s_%d_%s_%s_%d_%d",
		pci_dev->driver->driver.name, qat_dev->qat_dev_id,
		qp_conf->service_str, "qp_mem",
		queue->hw_bundle_number, queue->hw_queue_number);
	qp_mz = queue_dma_zone_reserve(queue->memz_name, queue_size_bytes,
			pci_dev->device.numa_node);
	if (qp_mz == NULL) {
		QAT_LOG(ERR, "Failed to allocate ring memzone");
		return -ENOMEM;
	}

	queue->base_addr = (char *)qp_mz->addr;
	queue->base_phys_addr = qp_mz->iova;
	if (qat_qp_check_queue_alignment(queue->base_phys_addr,
			queue_size_bytes)) {
		QAT_LOG(ERR, "Invalid alignment on queue create "
					" 0x%"PRIx64"\n",
					queue->base_phys_addr);
		ret = -EFAULT;
		goto queue_create_err;
	}

	if (adf_verify_queue_size(desc_size, qp_conf->nb_descriptors,
			&(queue->queue_size)) != 0) {
		QAT_LOG(ERR, "Invalid num inflights");
		ret = -EINVAL;
		goto queue_create_err;
	}

	queue->modulo_mask = (1 << ADF_RING_SIZE_MODULO(queue->queue_size)) - 1;
	queue->head = 0;
	queue->tail = 0;
	queue->msg_size = desc_size;

	/* For fast calculation of cookie index, relies on msg_size being 2^n */
	queue->trailz = __builtin_ctz(desc_size);

	/*
	 * Write an unused pattern to the queue memory.
	 */
	memset(queue->base_addr, 0x7F, queue_size_bytes);

	queue_base = BUILD_RING_BASE_ADDR(queue->base_phys_addr,
					queue->queue_size);

	io_addr = pci_dev->mem_resource[0].addr;

	WRITE_CSR_RING_BASE(io_addr, queue->hw_bundle_number,
			queue->hw_queue_number, queue_base);

	QAT_LOG(DEBUG, "RING: Name:%s, size in CSR: %u, in bytes %u,"
		" nb msgs %u, msg_size %u, modulo mask %u",
			queue->memz_name,
			queue->queue_size, queue_size_bytes,
			qp_conf->nb_descriptors, desc_size,
			queue->modulo_mask);

	return 0;

queue_create_err:
	rte_memzone_free(qp_mz);
	return ret;
}

static int qat_qp_check_queue_alignment(uint64_t phys_addr,
					uint32_t queue_size_bytes)
{
	if (((queue_size_bytes - 1) & phys_addr) != 0)
		return -EINVAL;
	return 0;
}

static int adf_verify_queue_size(uint32_t msg_size, uint32_t msg_num,
	uint32_t *p_queue_size_for_csr)
{
	uint8_t i = ADF_MIN_RING_SIZE;

	for (; i <= ADF_MAX_RING_SIZE; i++)
		if ((msg_size * msg_num) ==
				(uint32_t)ADF_SIZE_TO_RING_SIZE_IN_BYTES(i)) {
			*p_queue_size_for_csr = i;
			return 0;
		}
	QAT_LOG(ERR, "Invalid ring size %d", msg_size * msg_num);
	return -EINVAL;
}

static void adf_queue_arb_enable(struct qat_queue *txq, void *base_addr,
					rte_spinlock_t *lock)
{
	uint32_t arb_csr_offset =  ADF_ARB_RINGSRVARBEN_OFFSET +
					(ADF_ARB_REG_SLOT *
							txq->hw_bundle_number);
	uint32_t value;

	rte_spinlock_lock(lock);
	value = ADF_CSR_RD(base_addr, arb_csr_offset);
	value |= (0x01 << txq->hw_queue_number);
	ADF_CSR_WR(base_addr, arb_csr_offset, value);
	rte_spinlock_unlock(lock);
}

static void adf_queue_arb_disable(struct qat_queue *txq, void *base_addr,
					rte_spinlock_t *lock)
{
	uint32_t arb_csr_offset =  ADF_ARB_RINGSRVARBEN_OFFSET +
					(ADF_ARB_REG_SLOT *
							txq->hw_bundle_number);
	uint32_t value;

	rte_spinlock_lock(lock);
	value = ADF_CSR_RD(base_addr, arb_csr_offset);
	value &= ~(0x01 << txq->hw_queue_number);
	ADF_CSR_WR(base_addr, arb_csr_offset, value);
	rte_spinlock_unlock(lock);
}

static void adf_configure_queues(struct qat_qp *qp)
{
	uint32_t queue_config;
	struct qat_queue *queue = &qp->tx_q;

	queue_config = BUILD_RING_CONFIG(queue->queue_size);

	WRITE_CSR_RING_CONFIG(qp->mmap_bar_addr, queue->hw_bundle_number,
			queue->hw_queue_number, queue_config);

	queue = &qp->rx_q;
	queue_config =
			BUILD_RESP_RING_CONFIG(queue->queue_size,
					ADF_RING_NEAR_WATERMARK_512,
					ADF_RING_NEAR_WATERMARK_0);

	WRITE_CSR_RING_CONFIG(qp->mmap_bar_addr, queue->hw_bundle_number,
			queue->hw_queue_number, queue_config);
}

static inline uint32_t adf_modulo(uint32_t data, uint32_t modulo_mask)
{
	return data & modulo_mask;
}

static inline void
txq_write_tail(struct qat_qp *qp, struct qat_queue *q) {
	WRITE_CSR_RING_TAIL(qp->mmap_bar_addr, q->hw_bundle_number,
			q->hw_queue_number, q->tail);
	q->csr_tail = q->tail;
}

static inline
void rxq_free_desc(struct qat_qp *qp, struct qat_queue *q)
{
	uint32_t old_head, new_head;
	uint32_t max_head;

	old_head = q->csr_head;
	new_head = q->head;
	max_head = qp->nb_descriptors * q->msg_size;

	/* write out free descriptors */
	void *cur_desc = (uint8_t *)q->base_addr + old_head;

	if (new_head < old_head) {
		memset(cur_desc, ADF_RING_EMPTY_SIG_BYTE, max_head - old_head);
		memset(q->base_addr, ADF_RING_EMPTY_SIG_BYTE, new_head);
	} else {
		memset(cur_desc, ADF_RING_EMPTY_SIG_BYTE, new_head - old_head);
	}
	q->nb_processed_responses = 0;
	q->csr_head = new_head;

	/* write current head to CSR */
	WRITE_CSR_RING_HEAD(qp->mmap_bar_addr, q->hw_bundle_number,
			    q->hw_queue_number, new_head);
}

uint16_t
qat_enqueue_op_burst(void *qp, void **ops, uint16_t nb_ops)
{
	register struct qat_queue *queue;
	struct qat_qp *tmp_qp = (struct qat_qp *)qp;
	register uint32_t nb_ops_sent = 0;
	register int ret = -1;
	uint16_t nb_ops_possible = nb_ops;
	register uint8_t *base_addr;
	register uint32_t tail;

	if (unlikely(nb_ops == 0))
		return 0;

	/* read params used a lot in main loop into registers */
	queue = &(tmp_qp->tx_q);
	base_addr = (uint8_t *)queue->base_addr;
	tail = queue->tail;

	/* Find how many can actually fit on the ring */
	{
		/* dequeued can only be written by one thread, but it may not
		 * be this thread. As it's 4-byte aligned it will be read
		 * atomically here by any Intel CPU.
		 * enqueued can wrap before dequeued, but cannot
		 * lap it as var size of enq/deq (uint32_t) > var size of
		 * max_inflights (uint16_t). In reality inflights is never
		 * even as big as max uint16_t, as it's <= ADF_MAX_DESC.
		 * On wrapping, the calculation still returns the correct
		 * positive value as all three vars are unsigned.
		 */
		uint32_t inflights =
			tmp_qp->enqueued - tmp_qp->dequeued;

		if ((inflights + nb_ops) > tmp_qp->max_inflights) {
			nb_ops_possible = tmp_qp->max_inflights - inflights;
			if (nb_ops_possible == 0)
				return 0;
		}
		/* QAT has plenty of work queued already, so don't waste cycles
		 * enqueueing, wait til the application has gathered a bigger
		 * burst or some completed ops have been dequeued
		 */
		if (tmp_qp->min_enq_burst_threshold && inflights >
				QAT_QP_MIN_INFL_THRESHOLD && nb_ops_possible <
				tmp_qp->min_enq_burst_threshold) {
			tmp_qp->stats.threshold_hit_count++;
			return 0;
		}
	}

#ifdef BUILD_QAT_SYM
	if (tmp_qp->service_type == QAT_SERVICE_SYMMETRIC)
		qat_sym_preprocess_requests(ops, nb_ops_possible);
#endif

	while (nb_ops_sent != nb_ops_possible) {
		if (tmp_qp->service_type == QAT_SERVICE_SYMMETRIC) {
#ifdef BUILD_QAT_SYM
			ret = qat_sym_build_request(*ops, base_addr + tail,
				tmp_qp->op_cookies[tail >> queue->trailz],
				tmp_qp->qat_dev_gen);
#endif
		} else if (tmp_qp->service_type == QAT_SERVICE_COMPRESSION) {
			ret = qat_comp_build_request(*ops, base_addr + tail,
				tmp_qp->op_cookies[tail >> queue->trailz],
				tmp_qp->qat_dev_gen);
		} else if (tmp_qp->service_type == QAT_SERVICE_ASYMMETRIC) {
#ifdef BUILD_QAT_ASYM
			ret = qat_asym_build_request(*ops, base_addr + tail,
				tmp_qp->op_cookies[tail >> queue->trailz],
				tmp_qp->qat_dev_gen);
#endif
		}
		if (ret != 0) {
			tmp_qp->stats.enqueue_err_count++;
			/* This message cannot be enqueued */
			if (nb_ops_sent == 0)
				return 0;
			goto kick_tail;
		}

		tail = adf_modulo(tail + queue->msg_size, queue->modulo_mask);
		ops++;
		nb_ops_sent++;
	}
kick_tail:
	queue->tail = tail;
	tmp_qp->enqueued += nb_ops_sent;
	tmp_qp->stats.enqueued_count += nb_ops_sent;
	txq_write_tail(tmp_qp, queue);
	return nb_ops_sent;
}

/* Use this for compression only - but keep consistent with above common
 * function as much as possible.
 */
uint16_t
qat_enqueue_comp_op_burst(void *qp, void **ops, uint16_t nb_ops)
{
	register struct qat_queue *queue;
	struct qat_qp *tmp_qp = (struct qat_qp *)qp;
	register uint32_t nb_ops_sent = 0;
	register int nb_desc_to_build;
	uint16_t nb_ops_possible = nb_ops;
	register uint8_t *base_addr;
	register uint32_t tail;

	int descriptors_built, total_descriptors_built = 0;
	int nb_remaining_descriptors;
	int overflow = 0;

	if (unlikely(nb_ops == 0))
		return 0;

	/* read params used a lot in main loop into registers */
	queue = &(tmp_qp->tx_q);
	base_addr = (uint8_t *)queue->base_addr;
	tail = queue->tail;

	/* Find how many can actually fit on the ring */
	{
		/* dequeued can only be written by one thread, but it may not
		 * be this thread. As it's 4-byte aligned it will be read
		 * atomically here by any Intel CPU.
		 * enqueued can wrap before dequeued, but cannot
		 * lap it as var size of enq/deq (uint32_t) > var size of
		 * max_inflights (uint16_t). In reality inflights is never
		 * even as big as max uint16_t, as it's <= ADF_MAX_DESC.
		 * On wrapping, the calculation still returns the correct
		 * positive value as all three vars are unsigned.
		 */
		uint32_t inflights =
			tmp_qp->enqueued - tmp_qp->dequeued;

		/* Find how many can actually fit on the ring */
		overflow = (inflights + nb_ops) - tmp_qp->max_inflights;
		if (overflow > 0) {
			nb_ops_possible = nb_ops - overflow;
			if (nb_ops_possible == 0)
				return 0;
		}

		/* QAT has plenty of work queued already, so don't waste cycles
		 * enqueueing, wait til the application has gathered a bigger
		 * burst or some completed ops have been dequeued
		 */
		if (tmp_qp->min_enq_burst_threshold && inflights >
				QAT_QP_MIN_INFL_THRESHOLD && nb_ops_possible <
				tmp_qp->min_enq_burst_threshold) {
			tmp_qp->stats.threshold_hit_count++;
			return 0;
		}
	}

	/* At this point nb_ops_possible is assuming a 1:1 mapping
	 * between ops and descriptors.
	 * Fewer may be sent if some ops have to be split.
	 * nb_ops_possible is <= burst size.
	 * Find out how many spaces are actually available on the qp in case
	 * more are needed.
	 */
	nb_remaining_descriptors = nb_ops_possible
			 + ((overflow >= 0) ? 0 : overflow * (-1));
	QAT_DP_LOG(DEBUG, "Nb ops requested %d, nb descriptors remaining %d",
			nb_ops, nb_remaining_descriptors);

	while (nb_ops_sent != nb_ops_possible &&
				nb_remaining_descriptors > 0) {
		struct qat_comp_op_cookie *cookie =
				tmp_qp->op_cookies[tail >> queue->trailz];

		descriptors_built = 0;

		QAT_DP_LOG(DEBUG, "--- data length: %u",
			   ((struct rte_comp_op *)*ops)->src.length);

		nb_desc_to_build = qat_comp_build_request(*ops,
				base_addr + tail, cookie, tmp_qp->qat_dev_gen);
		QAT_DP_LOG(DEBUG, "%d descriptors built, %d remaining, "
			"%d ops sent, %d descriptors needed",
			total_descriptors_built, nb_remaining_descriptors,
			nb_ops_sent, nb_desc_to_build);

		if (unlikely(nb_desc_to_build < 0)) {
			/* this message cannot be enqueued */
			tmp_qp->stats.enqueue_err_count++;
			if (nb_ops_sent == 0)
				return 0;
			goto kick_tail;
		} else if (unlikely(nb_desc_to_build > 1)) {
			/* this op is too big and must be split - get more
			 * descriptors and retry
			 */

			QAT_DP_LOG(DEBUG, "Build %d descriptors for this op",
					nb_desc_to_build);

			nb_remaining_descriptors -= nb_desc_to_build;
			if (nb_remaining_descriptors >= 0) {
				/* There are enough remaining descriptors
				 * so retry
				 */
				int ret2 = qat_comp_build_multiple_requests(
						*ops, tmp_qp, tail,
						nb_desc_to_build);

				if (unlikely(ret2 < 1)) {
					QAT_DP_LOG(DEBUG,
							"Failed to build (%d) descriptors, status %d",
							nb_desc_to_build, ret2);

					qat_comp_free_split_op_memzones(cookie,
							nb_desc_to_build - 1);

					tmp_qp->stats.enqueue_err_count++;

					/* This message cannot be enqueued */
					if (nb_ops_sent == 0)
						return 0;
					goto kick_tail;
				} else {
					descriptors_built = ret2;
					total_descriptors_built +=
							descriptors_built;
					nb_remaining_descriptors -=
							descriptors_built;
					QAT_DP_LOG(DEBUG,
							"Multiple descriptors (%d) built ok",
							descriptors_built);
				}
			} else {
				QAT_DP_LOG(ERR, "For the current op, number of requested descriptors (%d) "
						"exceeds number of available descriptors (%d)",
						nb_desc_to_build,
						nb_remaining_descriptors +
							nb_desc_to_build);

				qat_comp_free_split_op_memzones(cookie,
						nb_desc_to_build - 1);

				/* Not enough extra descriptors */
				if (nb_ops_sent == 0)
					return 0;
				goto kick_tail;
			}
		} else {
			descriptors_built = 1;
			total_descriptors_built++;
			nb_remaining_descriptors--;
			QAT_DP_LOG(DEBUG, "Single descriptor built ok");
		}

		tail = adf_modulo(tail + (queue->msg_size * descriptors_built),
				  queue->modulo_mask);
		ops++;
		nb_ops_sent++;
	}

kick_tail:
	queue->tail = tail;
	tmp_qp->enqueued += total_descriptors_built;
	tmp_qp->stats.enqueued_count += nb_ops_sent;
	txq_write_tail(tmp_qp, queue);
	return nb_ops_sent;
}

uint16_t
qat_dequeue_op_burst(void *qp, void **ops, uint16_t nb_ops)
{
	struct qat_queue *rx_queue;
	struct qat_qp *tmp_qp = (struct qat_qp *)qp;
	uint32_t head;
	uint32_t op_resp_counter = 0, fw_resp_counter = 0;
	uint8_t *resp_msg;
	int nb_fw_responses;

	rx_queue = &(tmp_qp->rx_q);
	head = rx_queue->head;
	resp_msg = (uint8_t *)rx_queue->base_addr + rx_queue->head;

	while (*(uint32_t *)resp_msg != ADF_RING_EMPTY_SIG &&
			op_resp_counter != nb_ops) {

		nb_fw_responses = 1;

		if (tmp_qp->service_type == QAT_SERVICE_SYMMETRIC)
			qat_sym_process_response(ops, resp_msg);
		else if (tmp_qp->service_type == QAT_SERVICE_COMPRESSION)
			nb_fw_responses = qat_comp_process_response(
				ops, resp_msg,
				tmp_qp->op_cookies[head >> rx_queue->trailz],
				&tmp_qp->stats.dequeue_err_count);
#ifdef BUILD_QAT_ASYM
		else if (tmp_qp->service_type == QAT_SERVICE_ASYMMETRIC)
			qat_asym_process_response(ops, resp_msg,
				tmp_qp->op_cookies[head >> rx_queue->trailz]);
#endif

		head = adf_modulo(head + rx_queue->msg_size,
				  rx_queue->modulo_mask);

		resp_msg = (uint8_t *)rx_queue->base_addr + head;

		if (nb_fw_responses) {
			/* only move on to next op if one was ready to return
			 * to API
			 */
			ops++;
			op_resp_counter++;
		}

		 /* A compression op may be broken up into multiple fw requests.
		  * Only count fw responses as complete once ALL the responses
		  * associated with an op have been processed, as the cookie
		  * data from the first response must be available until
		  * finished with all firmware responses.
		  */
		fw_resp_counter += nb_fw_responses;

		rx_queue->nb_processed_responses++;
	}

	tmp_qp->dequeued += fw_resp_counter;
	tmp_qp->stats.dequeued_count += op_resp_counter;

	rx_queue->head = head;
	if (rx_queue->nb_processed_responses > QAT_CSR_HEAD_WRITE_THRESH)
		rxq_free_desc(tmp_qp, rx_queue);

	QAT_DP_LOG(DEBUG, "Dequeue burst return: %u, QAT responses: %u",
			op_resp_counter, fw_resp_counter);

	return op_resp_counter;
}

/* This is almost same as dequeue_op_burst, without the atomic, without stats
 * and without the op. Dequeues one response.
 */
static uint8_t
qat_cq_dequeue_response(struct qat_qp *qp, void *out_data)
{
	uint8_t result = 0;
	uint8_t retries = 0;
	struct qat_queue *queue = &(qp->rx_q);
	struct icp_qat_fw_comn_resp *resp_msg = (struct icp_qat_fw_comn_resp *)
			((uint8_t *)queue->base_addr + queue->head);

	while (retries++ < QAT_CQ_MAX_DEQ_RETRIES &&
			*(uint32_t *)resp_msg == ADF_RING_EMPTY_SIG) {
		/* loop waiting for response until we reach the timeout */
		rte_delay_ms(20);
	}

	if (*(uint32_t *)resp_msg != ADF_RING_EMPTY_SIG) {
		/* response received */
		result = 1;

		/* check status flag */
		if (ICP_QAT_FW_COMN_RESP_CRYPTO_STAT_GET(
				resp_msg->comn_hdr.comn_status) ==
				ICP_QAT_FW_COMN_STATUS_FLAG_OK) {
			/* success */
			memcpy(out_data, resp_msg, queue->msg_size);
		} else {
			memset(out_data, 0, queue->msg_size);
		}

		queue->head = adf_modulo(queue->head + queue->msg_size,
				queue->modulo_mask);
		rxq_free_desc(qp, queue);
	}

	return result;
}

/* Sends a NULL message and extracts QAT fw version from the response.
 * Used to determine detailed capabilities based on the fw version number.
 * This assumes that there are no inflight messages, i.e. assumes there's space
 * on the qp, one message is sent and only one response collected.
 * Returns fw version number or 0 for unknown version or a negative error code.
 */
int
qat_cq_get_fw_version(struct qat_qp *qp)
{
	struct qat_queue *queue = &(qp->tx_q);
	uint8_t *base_addr = (uint8_t *)queue->base_addr;
	struct icp_qat_fw_comn_req null_msg;
	struct icp_qat_fw_comn_resp response;

	/* prepare the NULL request */
	memset(&null_msg, 0, sizeof(null_msg));
	null_msg.comn_hdr.hdr_flags =
		ICP_QAT_FW_COMN_HDR_FLAGS_BUILD(ICP_QAT_FW_COMN_REQ_FLAG_SET);
	null_msg.comn_hdr.service_type = ICP_QAT_FW_COMN_REQ_NULL;
	null_msg.comn_hdr.service_cmd_id = ICP_QAT_FW_NULL_REQ_SERV_ID;

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	QAT_DP_HEXDUMP_LOG(DEBUG, "NULL request", &null_msg, sizeof(null_msg));
#endif

	/* send the NULL request */
	memcpy(base_addr + queue->tail, &null_msg, sizeof(null_msg));
	queue->tail = adf_modulo(queue->tail + queue->msg_size,
			queue->modulo_mask);
	txq_write_tail(qp, queue);

	/* receive a response */
	if (qat_cq_dequeue_response(qp, &response)) {

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
		QAT_DP_HEXDUMP_LOG(DEBUG, "NULL response:", &response,
				sizeof(response));
#endif
		/* if LW0 bit 24 is set - then the fw version was returned */
		if (QAT_FIELD_GET(response.comn_hdr.hdr_flags,
				ICP_QAT_FW_COMN_NULL_VERSION_FLAG_BITPOS,
				ICP_QAT_FW_COMN_NULL_VERSION_FLAG_MASK))
			return response.resrvd[0]; /* return LW4 */
		else
			return 0; /* not set - we don't know fw version */
	}

	QAT_LOG(ERR, "No response received");
	return -EINVAL;
}

__rte_weak int
qat_comp_process_response(void **op __rte_unused, uint8_t *resp __rte_unused,
			  void *op_cookie __rte_unused,
			  uint64_t *dequeue_err_count __rte_unused)
{
	return  0;
}
