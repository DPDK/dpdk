/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */
#ifndef _QAT_COMMON_H_
#define _QAT_COMMON_H_

#include <stdint.h>

#include <rte_mbuf.h>

/**< Intel(R) QAT device name for PCI registration */
#define QAT_PCI_NAME	qat
#define QAT_64_BTYE_ALIGN_MASK (~0x3f)

/* Intel(R) QuickAssist Technology device generation is enumerated
 * from one according to the generation of the device.
 * QAT_GEN* is used as the index to find all devices
 */
enum qat_device_gen {
	QAT_GEN1,
	QAT_GEN2,
	QAT_GEN3,
	QAT_GEN4,
	QAT_N_GENS
};

enum qat_service_type {
	QAT_SERVICE_ASYMMETRIC,
	QAT_SERVICE_SYMMETRIC,
	QAT_SERVICE_COMPRESSION,
	QAT_MAX_SERVICES
};

#define QAT_SERVICE_INVALID	(QAT_MAX_SERVICES)

enum qat_svc_list {
	QAT_SVC_UNUSED = 0,
	QAT_SVC_CRYPTO = 1,
	QAT_SVC_COMPRESSION = 2,
	QAT_SVC_SYM = 3,
	QAT_SVC_ASYM = 4,
};

/**< Common struct for scatter-gather list operations */
struct qat_flat_buf {
	uint32_t len;
	uint32_t resrvd;
	uint64_t addr;
} __rte_packed;

#define qat_sgl_hdr  struct { \
	uint64_t resrvd; \
	uint32_t num_bufs; \
	uint32_t num_mapped_bufs; \
}

__extension__
struct qat_sgl {
	qat_sgl_hdr;
	/* flexible array of flat buffers*/
	struct qat_flat_buf buffers[0];
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
	uint64_t threshold_hit_count;
	/**< Total number of times min qp threshold condition was fulfilled */

};

struct qat_pci_device;

int
qat_sgl_fill_array(struct rte_mbuf *buf, int64_t offset,
		void *list_in, uint32_t data_len,
		const uint16_t max_segs);
void
qat_stats_get(struct qat_pci_device *dev,
		struct qat_common_stats *stats,
		enum qat_service_type service);
void
qat_stats_reset(struct qat_pci_device *dev,
		enum qat_service_type service);

const char *
qat_service_get_str(enum qat_service_type type);

#endif /* _QAT_COMMON_H_ */
