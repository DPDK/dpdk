/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */
#ifndef _QAT_QP_H_
#define _QAT_QP_H_

#include "qat_common.h"
#include "adf_transport_access_macros.h"

struct qat_pci_device;

#define QAT_CSR_HEAD_WRITE_THRESH 32U
/* number of requests to accumulate before writing head CSR */

#define QAT_QP_MIN_INFL_THRESHOLD	256

/**
 * Structure with data needed for creation of queue pair.
 */
struct qat_qp_hw_data {
	enum qat_service_type service_type;
	uint8_t hw_bundle_num;
	uint8_t tx_ring_num;
	uint8_t rx_ring_num;
	uint16_t tx_msg_size;
	uint16_t rx_msg_size;
};

/**
 * Structure with data needed for creation of queue pair.
 */
struct qat_qp_config {
	const struct qat_qp_hw_data *hw;
	uint32_t nb_descriptors;
	uint32_t cookie_size;
	int socket_id;
	const char *service_str;
};

/**
 * Structure associated with each queue.
 */
struct qat_queue {
	char		memz_name[RTE_MEMZONE_NAMESIZE];
	void		*base_addr;		/* Base address */
	rte_iova_t	base_phys_addr;		/* Queue physical address */
	uint32_t	head;			/* Shadow copy of the head */
	uint32_t	tail;			/* Shadow copy of the tail */
	uint32_t	modulo_mask;
	uint32_t	msg_size;
	uint32_t	queue_size;
	uint8_t		trailz;
	uint8_t		hw_bundle_number;
	uint8_t		hw_queue_number;
	/* HW queue aka ring offset on bundle */
	uint32_t	csr_head;		/* last written head value */
	uint32_t	csr_tail;		/* last written tail value */
	uint16_t	nb_processed_responses;
	/* number of responses processed since last CSR head write */
};

struct qat_qp {
	void			*mmap_bar_addr;
	struct qat_queue	tx_q;
	struct qat_queue	rx_q;
	struct qat_common_stats stats;
	struct rte_mempool *op_cookie_pool;
	void **op_cookies;
	uint32_t nb_descriptors;
	enum qat_device_gen qat_dev_gen;
	enum qat_service_type service_type;
	struct qat_pci_device *qat_dev;
	/**< qat device this qp is on */
	uint32_t enqueued;
	uint32_t dequeued __rte_aligned(4);
	uint16_t max_inflights;
	uint16_t min_enq_burst_threshold;
} __rte_cache_aligned;

extern const struct qat_qp_hw_data qat_gen1_qps[][ADF_MAX_QPS_ON_ANY_SERVICE];
extern const struct qat_qp_hw_data qat_gen3_qps[][ADF_MAX_QPS_ON_ANY_SERVICE];

uint16_t
qat_enqueue_op_burst(void *qp, void **ops, uint16_t nb_ops);

uint16_t
qat_enqueue_comp_op_burst(void *qp, void **ops, uint16_t nb_ops);

uint16_t
qat_dequeue_op_burst(void *qp, void **ops, uint16_t nb_ops);

int
qat_qp_release(struct qat_qp **qp_addr);

int
qat_qp_setup(struct qat_pci_device *qat_dev,
		struct qat_qp **qp_addr, uint16_t queue_pair_id,
		struct qat_qp_config *qat_qp_conf);

int
qat_qps_per_service(const struct qat_qp_hw_data *qp_hw_data,
			enum qat_service_type service);

int
qat_cq_get_fw_version(struct qat_qp *qp);

/* Needed for weak function*/
int
qat_comp_process_response(void **op __rte_unused, uint8_t *resp __rte_unused,
			  void *op_cookie __rte_unused,
			  uint64_t *dequeue_err_count __rte_unused);

#endif /* _QAT_QP_H_ */
