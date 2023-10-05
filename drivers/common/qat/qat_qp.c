/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2022 Intel Corporation
 */

#include <rte_common.h>
#include <rte_cycles.h>
#include <dev_driver.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_pci.h>
#include <bus_pci_driver.h>
#include <rte_atomic.h>
#include <rte_prefetch.h>
#ifdef BUILD_QAT_SYM
#include <rte_ether.h>
#endif

#include "qat_logs.h"
#include "qat_device.h"
#include "qat_qp.h"
#include "qat_sym.h"
#include "qat_asym.h"
#include "qat_comp.h"

#define QAT_CQ_MAX_DEQ_RETRIES 10

#define ADF_MAX_DESC				4096
#define ADF_MIN_DESC				128

#ifdef BUILD_QAT_SYM
/* Cipher-CRC capability check test parameters */
static const uint8_t cipher_crc_cap_check_iv[] = {
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11
};

static const uint8_t cipher_crc_cap_check_key[] = {
	0x00, 0x00, 0x00, 0x00, 0xAA, 0xBB, 0xCC, 0xDD,
	0xEE, 0xFF, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55
};

static const uint8_t cipher_crc_cap_check_plaintext[] = {
	/* Outer protocol header */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* Ethernet frame */
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x06, 0x05,
	0x04, 0x03, 0x02, 0x01, 0x08, 0x00, 0xAA, 0xAA,
	0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	/* CRC */
	0xFF, 0xFF, 0xFF, 0xFF
};

static const uint8_t cipher_crc_cap_check_ciphertext[] = {
	/* Outer protocol header */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* Ethernet frame */
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x06, 0x05,
	0x04, 0x03, 0x02, 0x01, 0xD6, 0xE2, 0x70, 0x5C,
	0xE6, 0x4D, 0xCC, 0x8C, 0x47, 0xB7, 0x09, 0xD6,
	/* CRC */
	0x54, 0x85, 0xF8, 0x32
};

static const uint8_t cipher_crc_cap_check_cipher_offset = 18;
static const uint8_t cipher_crc_cap_check_crc_offset = 6;
#endif

struct qat_qp_hw_spec_funcs*
	qat_qp_hw_spec[QAT_N_GENS];

static int qat_qp_check_queue_alignment(uint64_t phys_addr,
	uint32_t queue_size_bytes);
static void qat_queue_delete(struct qat_queue *queue);
static int qat_queue_create(struct qat_pci_device *qat_dev,
	struct qat_queue *queue, struct qat_qp_config *, uint8_t dir);
static int adf_verify_queue_size(uint32_t msg_size, uint32_t msg_num,
	uint32_t *queue_size_for_csr);
static int adf_configure_queues(struct qat_qp *queue,
	enum qat_device_gen qat_dev_gen);
static int adf_queue_arb_enable(struct qat_pci_device *qat_dev,
	struct qat_queue *txq, void *base_addr, rte_spinlock_t *lock);
static int adf_queue_arb_disable(enum qat_device_gen qat_dev_gen,
	struct qat_queue *txq, void *base_addr, rte_spinlock_t *lock);
static int qat_qp_build_ring_base(struct qat_pci_device *qat_dev,
	void *io_addr, struct qat_queue *queue);
static const struct rte_memzone *queue_dma_zone_reserve(const char *queue_name,
	uint32_t queue_size, int socket_id);
static int qat_qp_csr_setup(struct qat_pci_device *qat_dev, void *io_addr,
	struct qat_qp *qp);

int
qat_qp_setup(struct qat_pci_device *qat_dev,
		struct qat_qp **qp_addr,
		uint16_t queue_pair_id,
		struct qat_qp_config *qat_qp_conf)
{
	struct qat_qp *qp = NULL;
	struct rte_pci_device *pci_dev =
			qat_pci_devs[qat_dev->qat_dev_id].pci_dev;
	char op_cookie_pool_name[RTE_RING_NAMESIZE];
	struct qat_dev_hw_spec_funcs *ops_hw =
		qat_dev_hw_spec[qat_dev->qat_dev_gen];
	void *io_addr;
	uint32_t i;

	QAT_LOG(DEBUG, "Setup qp %u on qat pci device %d gen %d",
		queue_pair_id, qat_dev->qat_dev_id, qat_dev->qat_dev_gen);

	if ((qat_qp_conf->nb_descriptors > ADF_MAX_DESC) ||
		(qat_qp_conf->nb_descriptors < ADF_MIN_DESC)) {
		QAT_LOG(ERR, "Can't create qp for %u descriptors",
				qat_qp_conf->nb_descriptors);
		return -EINVAL;
	}

	if (ops_hw->qat_dev_get_transport_bar == NULL)	{
		QAT_LOG(ERR,
			"QAT Internal Error: qat_dev_get_transport_bar not set for gen %d",
			qat_dev->qat_dev_gen);
		goto create_err;
	}

	io_addr = ops_hw->qat_dev_get_transport_bar(pci_dev)->addr;
	if (io_addr == NULL) {
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

	qp->mmap_bar_addr = io_addr;
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
		qat_queue_delete(&(qp->tx_q));
		qat_queue_delete(&(qp->rx_q));
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

	qat_qp_csr_setup(qat_dev, io_addr, qp);

	*qp_addr = qp;
	return 0;

create_err:
	if (qp) {
		rte_mempool_free(qp->op_cookie_pool);

		rte_free(qp->op_cookies);

		rte_free(qp);
	}

	return -EFAULT;
}

static int
qat_queue_create(struct qat_pci_device *qat_dev, struct qat_queue *queue,
		struct qat_qp_config *qp_conf, uint8_t dir)
{
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
	queue->trailz = rte_ctz32(desc_size);

	/*
	 * Write an unused pattern to the queue memory.
	 */
	memset(queue->base_addr, 0x7F, queue_size_bytes);

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

int
qat_qp_release(enum qat_device_gen qat_dev_gen, struct qat_qp **qp_addr)
{
	int ret;
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

	ret = adf_queue_arb_disable(qat_dev_gen, &(qp->tx_q),
			qp->mmap_bar_addr, &qp->qat_dev->arb_csr_lock);
	if (ret)
		return ret;

	for (i = 0; i < qp->nb_descriptors; i++)
		rte_mempool_put(qp->op_cookie_pool, qp->op_cookies[i]);

	rte_mempool_free(qp->op_cookie_pool);

	rte_free(qp->op_cookies);
	rte_free(qp);
	*qp_addr = NULL;
	return 0;
}


static void
qat_queue_delete(struct qat_queue *queue)
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

static int __rte_unused
adf_queue_arb_enable(struct qat_pci_device *qat_dev, struct qat_queue *txq,
		void *base_addr, rte_spinlock_t *lock)
{
	struct qat_qp_hw_spec_funcs *ops =
		qat_qp_hw_spec[qat_dev->qat_dev_gen];

	if (ops->qat_qp_adf_arb_enable == NULL)
		return -ENOTSUP;
	ops->qat_qp_adf_arb_enable(txq, base_addr, lock);
	return 0;
}

static int
adf_queue_arb_disable(enum qat_device_gen qat_dev_gen, struct qat_queue *txq,
		void *base_addr, rte_spinlock_t *lock)
{
	struct qat_qp_hw_spec_funcs *ops =
		qat_qp_hw_spec[qat_dev_gen];

	if (ops->qat_qp_adf_arb_disable == NULL)
		return -ENOTSUP;
	ops->qat_qp_adf_arb_disable(txq, base_addr, lock);
	return 0;
}

static int __rte_unused
qat_qp_build_ring_base(struct qat_pci_device *qat_dev, void *io_addr,
		struct qat_queue *queue)
{
	struct qat_qp_hw_spec_funcs *ops =
		qat_qp_hw_spec[qat_dev->qat_dev_gen];

	if (ops->qat_qp_build_ring_base == NULL)
		return -ENOTSUP;
	ops->qat_qp_build_ring_base(io_addr, queue);
	return 0;
}

int
qat_qps_per_service(struct qat_pci_device *qat_dev,
		enum qat_service_type service)
{
	struct qat_qp_hw_spec_funcs *ops =
		qat_qp_hw_spec[qat_dev->qat_dev_gen];

	if (ops->qat_qp_rings_per_service == NULL)
		return -ENOTSUP;
	return ops->qat_qp_rings_per_service(qat_dev, service);
}

const struct qat_qp_hw_data *
qat_qp_get_hw_data(struct qat_pci_device *qat_dev,
		enum qat_service_type service, uint16_t qp_id)
{
	struct qat_qp_hw_spec_funcs *ops =
		qat_qp_hw_spec[qat_dev->qat_dev_gen];

	if (ops->qat_qp_get_hw_data == NULL)
		return NULL;
	return ops->qat_qp_get_hw_data(qat_dev, service, qp_id);
}

int
qat_read_qp_config(struct qat_pci_device *qat_dev)
{
	struct qat_dev_hw_spec_funcs *ops_hw =
		qat_dev_hw_spec[qat_dev->qat_dev_gen];

	if (ops_hw->qat_dev_read_config == NULL)
		return -ENOTSUP;
	return ops_hw->qat_dev_read_config(qat_dev);
}

static int __rte_unused
adf_configure_queues(struct qat_qp *qp, enum qat_device_gen qat_dev_gen)
{
	struct qat_qp_hw_spec_funcs *ops =
		qat_qp_hw_spec[qat_dev_gen];

	if (ops->qat_qp_adf_configure_queues == NULL)
		return -ENOTSUP;
	ops->qat_qp_adf_configure_queues(qp);
	return 0;
}

static inline void
qat_qp_csr_write_head(enum qat_device_gen qat_dev_gen, struct qat_qp *qp,
			struct qat_queue *q, uint32_t new_head)
{
	struct qat_qp_hw_spec_funcs *ops =
		qat_qp_hw_spec[qat_dev_gen];

	/*
	 * Pointer check should be done during
	 * initialization
	 */
	ops->qat_qp_csr_write_head(qp, q, new_head);
}

static int
qat_qp_csr_setup(struct qat_pci_device *qat_dev,
		void *io_addr, struct qat_qp *qp)
{
	struct qat_qp_hw_spec_funcs *ops =
		qat_qp_hw_spec[qat_dev->qat_dev_gen];

	if (ops->qat_qp_csr_setup == NULL)
		return -ENOTSUP;
	ops->qat_qp_csr_setup(qat_dev, io_addr, qp);
	return 0;
}


static inline
void rxq_free_desc(enum qat_device_gen qat_dev_gen, struct qat_qp *qp,
				struct qat_queue *q)
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

	qat_qp_csr_write_head(qat_dev_gen, qp, q, new_head);
}

static int
qat_qp_check_queue_alignment(uint64_t phys_addr, uint32_t queue_size_bytes)
{
	if (((queue_size_bytes - 1) & phys_addr) != 0)
		return -EINVAL;
	return 0;
}

static int
adf_verify_queue_size(uint32_t msg_size, uint32_t msg_num,
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

static inline uint32_t
adf_modulo(uint32_t data, uint32_t modulo_mask)
{
	return data & modulo_mask;
}

uint16_t
qat_enqueue_op_burst(void *qp, qat_op_build_request_t op_build_request,
		void **ops, uint16_t nb_ops)
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

	if (tmp_qp->service_type == QAT_SERVICE_SYMMETRIC)
		qat_sym_preprocess_requests(ops, nb_ops_possible);

	memset(tmp_qp->opaque, 0xff, sizeof(tmp_qp->opaque));

	while (nb_ops_sent != nb_ops_possible) {
		ret = op_build_request(*ops, base_addr + tail,
				tmp_qp->op_cookies[tail >> queue->trailz],
				tmp_qp->opaque, tmp_qp->qat_dev_gen);

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
	txq_write_tail(tmp_qp->qat_dev_gen, tmp_qp, queue);
	return nb_ops_sent;
}

uint16_t
qat_dequeue_op_burst(void *qp, void **ops,
		qat_op_dequeue_t qat_dequeue_process_response, uint16_t nb_ops)
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

		nb_fw_responses = qat_dequeue_process_response(
				ops, resp_msg,
				tmp_qp->op_cookies[head >> rx_queue->trailz],
				&tmp_qp->stats.dequeue_err_count);

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
		rxq_free_desc(tmp_qp->qat_dev_gen, tmp_qp, rx_queue);

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
		rxq_free_desc(qp->qat_dev_gen, qp, queue);
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
	txq_write_tail(qp->qat_dev_gen, qp, queue);

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

#ifdef BUILD_QAT_SYM
/* Sends an LA bulk req message to determine if a QAT device supports Cipher-CRC
 * offload. This assumes that there are no inflight messages, i.e. assumes
 * there's space  on the qp, one message is sent and only one response
 * collected. The status bit of the response and returned data are checked.
 * Returns:
 *     1 if status bit indicates success and returned data matches expected
 *     data (i.e. Cipher-CRC supported)
 *     0 if status bit indicates error or returned data does not match expected
 *     data (i.e. Cipher-CRC not supported)
 *     Negative error code in case of error
 */
int
qat_cq_get_fw_cipher_crc_cap(struct qat_qp *qp)
{
	struct qat_queue *queue = &(qp->tx_q);
	uint8_t *base_addr = (uint8_t *)queue->base_addr;
	struct icp_qat_fw_la_bulk_req cipher_crc_cap_msg = {{0}};
	struct icp_qat_fw_comn_resp response = {{0}};
	struct icp_qat_fw_la_cipher_req_params *cipher_param;
	struct icp_qat_fw_la_auth_req_params *auth_param;
	struct qat_sym_session *session;
	phys_addr_t phy_src_addr;
	uint64_t *src_data_addr;
	int ret;

	session = rte_zmalloc(NULL, sizeof(struct qat_sym_session), 0);
	if (session == NULL)
		return -EINVAL;

	/* Verify the session physical address is known */
	rte_iova_t session_paddr = rte_mem_virt2iova(session);
	if (session_paddr == 0 || session_paddr == RTE_BAD_IOVA) {
		QAT_LOG(ERR, "Session physical address unknown.");
		return -EINVAL;
	}

	/* Prepare the LA bulk request */
	ret = qat_cipher_crc_cap_msg_sess_prepare(session,
					session_paddr,
					cipher_crc_cap_check_key,
					sizeof(cipher_crc_cap_check_key),
					qp->qat_dev_gen);
	if (ret < 0) {
		rte_free(session);
		/* Returning 0 here to allow qp setup to continue, but
		 * indicate that Cipher-CRC offload is not supported on the
		 * device
		 */
		return 0;
	}

	cipher_crc_cap_msg = session->fw_req;

	src_data_addr = rte_zmalloc(NULL,
					sizeof(cipher_crc_cap_check_plaintext),
					0);
	if (src_data_addr == NULL) {
		rte_free(session);
		return -EINVAL;
	}

	rte_memcpy(src_data_addr,
			cipher_crc_cap_check_plaintext,
			sizeof(cipher_crc_cap_check_plaintext));

	phy_src_addr = rte_mem_virt2iova(src_data_addr);
	if (phy_src_addr == 0 || phy_src_addr == RTE_BAD_IOVA) {
		QAT_LOG(ERR, "Source physical address unknown.");
		return -EINVAL;
	}

	cipher_crc_cap_msg.comn_mid.src_data_addr = phy_src_addr;
	cipher_crc_cap_msg.comn_mid.src_length =
					sizeof(cipher_crc_cap_check_plaintext);
	cipher_crc_cap_msg.comn_mid.dest_data_addr = phy_src_addr;
	cipher_crc_cap_msg.comn_mid.dst_length =
					sizeof(cipher_crc_cap_check_plaintext);

	cipher_param = (void *)&cipher_crc_cap_msg.serv_specif_rqpars;
	auth_param = (void *)((uint8_t *)cipher_param +
			ICP_QAT_FW_HASH_REQUEST_PARAMETERS_OFFSET);

	rte_memcpy(cipher_param->u.cipher_IV_array,
			cipher_crc_cap_check_iv,
			sizeof(cipher_crc_cap_check_iv));

	cipher_param->cipher_offset = cipher_crc_cap_check_cipher_offset;
	cipher_param->cipher_length =
			sizeof(cipher_crc_cap_check_plaintext) -
			cipher_crc_cap_check_cipher_offset;
	auth_param->auth_off = cipher_crc_cap_check_crc_offset;
	auth_param->auth_len = sizeof(cipher_crc_cap_check_plaintext) -
				cipher_crc_cap_check_crc_offset -
				RTE_ETHER_CRC_LEN;

	ICP_QAT_FW_LA_DIGEST_IN_BUFFER_SET(
			cipher_crc_cap_msg.comn_hdr.serv_specif_flags,
			ICP_QAT_FW_LA_DIGEST_IN_BUFFER);

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	QAT_DP_HEXDUMP_LOG(DEBUG, "LA Bulk request", &cipher_crc_cap_msg,
			sizeof(cipher_crc_cap_msg));
#endif

	/* Send the cipher_crc_cap_msg request */
	memcpy(base_addr + queue->tail,
	       &cipher_crc_cap_msg,
	       sizeof(cipher_crc_cap_msg));
	queue->tail = adf_modulo(queue->tail + queue->msg_size,
			queue->modulo_mask);
	txq_write_tail(qp->qat_dev_gen, qp, queue);

	/* Check for response and verify data is same as ciphertext */
	if (qat_cq_dequeue_response(qp, &response)) {
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
		QAT_DP_HEXDUMP_LOG(DEBUG, "LA response:", &response,
				sizeof(response));
#endif

		if (memcmp(src_data_addr,
				cipher_crc_cap_check_ciphertext,
				sizeof(cipher_crc_cap_check_ciphertext)) != 0)
			ret = 0; /* Cipher-CRC offload not supported */
		else
			ret = 1;
	} else {
		ret = -EINVAL;
	}

	rte_free(src_data_addr);
	rte_free(session);
	return ret;
}
#endif

__rte_weak int
qat_comp_process_response(void **op __rte_unused, uint8_t *resp __rte_unused,
			  void *op_cookie __rte_unused,
			  uint64_t *dequeue_err_count __rte_unused)
{
	return  0;
}
