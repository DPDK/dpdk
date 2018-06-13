/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */
#ifndef _QAT_COMMON_H_
#define _QAT_COMMON_H_

#include <stdint.h>

/**< Intel(R) QAT Symmetric Crypto PMD device name */
#define CRYPTODEV_NAME_QAT_SYM_PMD	crypto_qat

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

struct qat_sym_op_cookie {
	struct qat_sgl qat_sgl_src;
	struct qat_sgl qat_sgl_dst;
	phys_addr_t qat_sgl_src_phys_addr;
	phys_addr_t qat_sgl_dst_phys_addr;
};

#endif /* _QAT_COMMON_H_ */
