/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2018 Intel Corporation
 */

#include <rte_common.h>
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
	struct rte_pci_device *pci_dev = qat_dev->pci_dev;
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
	qp->inflights16 = 0;

	if (qat_queue_create(qat_dev, &(qp->tx_q), qat_qp_conf,
					ADF_RING_DIR_TX) != 0) {
		QAT_LOG(ERR, "Tx queue create failed "
				"queue_pair_id=%u", queue_pair_id);
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
				qat_dev->pci_dev->device.numa_node,
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
	qp->build_request = qat_qp_conf->build_request;
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
	if (qp->inflights16 == 0) {
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
	struct rte_pci_device *pci_dev = qat_dev->pci_dev;
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
			qat_dev->pci_dev->device.numa_node);
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

	queue->max_inflights = ADF_MAX_INFLIGHTS(queue->queue_size,
					ADF_BYTES_TO_MSG_SIZE(desc_size));
	queue->modulo_mask = (1 << ADF_RING_SIZE_MODULO(queue->queue_size)) - 1;

	if (queue->max_inflights < 2) {
		QAT_LOG(ERR, "Invalid num inflights");
		ret = -EINVAL;
		goto queue_create_err;
	}
	queue->head = 0;
	queue->tail = 0;
	queue->msg_size = desc_size;

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
		" nb msgs %u, msg_size %u, max_inflights %u modulo mask %u",
			queue->memz_name,
			queue->queue_size, queue_size_bytes,
			qp_conf->nb_descriptors, desc_size,
			queue->max_inflights, queue->modulo_mask);

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
	q->nb_pending_requests = 0;
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
	register int ret;
	uint16_t nb_ops_possible = nb_ops;
	register uint8_t *base_addr;
	register uint32_t tail;
	int overflow;

	if (unlikely(nb_ops == 0))
		return 0;

	/* read params used a lot in main loop into registers */
	queue = &(tmp_qp->tx_q);
	base_addr = (uint8_t *)queue->base_addr;
	tail = queue->tail;

	/* Find how many can actually fit on the ring */
	tmp_qp->inflights16 += nb_ops;
	overflow = tmp_qp->inflights16 - queue->max_inflights;
	if (overflow > 0) {
		tmp_qp->inflights16 -= overflow;
		nb_ops_possible = nb_ops - overflow;
		if (nb_ops_possible == 0)
			return 0;
	}

	while (nb_ops_sent != nb_ops_possible) {
		ret = tmp_qp->build_request(*ops, base_addr + tail,
				tmp_qp->op_cookies[tail / queue->msg_size],
				tmp_qp->qat_dev_gen);
		if (ret != 0) {
			tmp_qp->stats.enqueue_err_count++;
			/*
			 * This message cannot be enqueued,
			 * decrease number of ops that wasn't sent
			 */
			tmp_qp->inflights16 -= nb_ops_possible - nb_ops_sent;
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
	tmp_qp->stats.enqueued_count += nb_ops_sent;
	queue->nb_pending_requests += nb_ops_sent;
	if (tmp_qp->inflights16 < QAT_CSR_TAIL_FORCE_WRITE_THRESH ||
		    queue->nb_pending_requests > QAT_CSR_TAIL_WRITE_THRESH) {
		txq_write_tail(tmp_qp, queue);
	}
	return nb_ops_sent;
}

uint16_t
qat_dequeue_op_burst(void *qp, void **ops, uint16_t nb_ops)
{
	struct qat_queue *rx_queue, *tx_queue;
	struct qat_qp *tmp_qp = (struct qat_qp *)qp;
	uint32_t head;
	uint32_t resp_counter = 0;
	uint8_t *resp_msg;

	rx_queue = &(tmp_qp->rx_q);
	tx_queue = &(tmp_qp->tx_q);
	head = rx_queue->head;
	resp_msg = (uint8_t *)rx_queue->base_addr + rx_queue->head;

	while (*(uint32_t *)resp_msg != ADF_RING_EMPTY_SIG &&
			resp_counter != nb_ops) {

		if (tmp_qp->service_type == QAT_SERVICE_SYMMETRIC)
			qat_sym_process_response(ops, resp_msg);
		else if (tmp_qp->service_type == QAT_SERVICE_COMPRESSION)
			qat_comp_process_response(ops, resp_msg,
					&tmp_qp->stats.dequeue_err_count);
		else if (tmp_qp->service_type == QAT_SERVICE_ASYMMETRIC) {
#ifdef BUILD_QAT_ASYM
			qat_asym_process_response(ops, resp_msg,
				tmp_qp->op_cookies[head / rx_queue->msg_size]);
#endif
		}

		head = adf_modulo(head + rx_queue->msg_size,
				  rx_queue->modulo_mask);

		resp_msg = (uint8_t *)rx_queue->base_addr + head;
		ops++;
		resp_counter++;
	}
	if (resp_counter > 0) {
		rx_queue->head = head;
		tmp_qp->stats.dequeued_count += resp_counter;
		rx_queue->nb_processed_responses += resp_counter;
		tmp_qp->inflights16 -= resp_counter;

		if (rx_queue->nb_processed_responses >
						QAT_CSR_HEAD_WRITE_THRESH)
			rxq_free_desc(tmp_qp, rx_queue);
	}
	/* also check if tail needs to be advanced */
	if (tmp_qp->inflights16 <= QAT_CSR_TAIL_FORCE_WRITE_THRESH &&
		tx_queue->tail != tx_queue->csr_tail) {
		txq_write_tail(tmp_qp, tx_queue);
	}
	return resp_counter;
}

__rte_weak int
qat_comp_process_response(void **op __rte_unused, uint8_t *resp __rte_unused,
			  uint64_t *dequeue_err_count __rte_unused)
{
	return  0;
}
