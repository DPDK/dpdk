/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */
#ifndef _QAT_COMMON_H_
#define _QAT_COMMON_H_

#include <stdint.h>

#include <rte_mbuf.h>

/**< Intel(R) QAT Symmetric Crypto PMD device name */
#define CRYPTODEV_NAME_QAT_SYM_PMD	crypto_qat

/**< Intel(R) QAT device name for PCI registration */
#define QAT_PCI_NAME	qat
/*
 * Maximum number of SGL entries
 */
#define QAT_SGL_MAX_NUMBER	16

/* Intel(R) QuickAssist Technology device generation is enumerated
 * from one according to the generation of the device
 */

enum qat_device_gen {
	QAT_GEN1 = 1,
	QAT_GEN2,
};

enum qat_service_type {
	QAT_SERVICE_ASYMMETRIC = 0,
	QAT_SERVICE_SYMMETRIC,
	QAT_SERVICE_COMPRESSION,
	QAT_SERVICE_INVALID
};
#define QAT_MAX_SERVICES		(QAT_SERVICE_INVALID)

/**< Common struct for scatter-gather list operations */
struct qat_flat_buf {
	uint32_t len;
	uint32_t resrvd;
	uint64_t addr;
} __rte_packed;

struct qat_sgl {
	uint64_t resrvd;
	uint32_t num_bufs;
	uint32_t num_mapped_bufs;
	struct qat_flat_buf buffers[QAT_SGL_MAX_NUMBER];
} __rte_packed __rte_cache_aligned;

/** Common, i.e. not service-specific, statistics */
struct qat_common_stats {
	uint64_t enqueued_count;
	/**< Count of all operations enqueued */
	uint64_t dequeued_count;
	/**< Count of all operations dequeued */

	uint64_t enqueue_err_count;
	/**< Total error count on operations enqueued */
	uint64_t dequeue_err_count;
	/**< Total error count on operations dequeued */
};

int
qat_sgl_fill_array(struct rte_mbuf *buf, uint64_t buf_start,
		struct qat_sgl *list, uint32_t data_len);

#endif /* _QAT_COMMON_H_ */
