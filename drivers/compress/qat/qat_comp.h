/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2018 Intel Corporation
 */

#ifndef _QAT_COMP_H_
#define _QAT_COMP_H_

#ifdef RTE_LIBRTE_COMPRESSDEV

#include <rte_compressdev.h>
#include <rte_compressdev_pmd.h>

#include "qat_common.h"
#include "icp_qat_hw.h"
#include "icp_qat_fw_comp.h"
#include "icp_qat_fw_la.h"

#define QAT_64_BYTE_ALIGN_MASK (~0x3f)
#define QAT_64_BYTE_ALIGN (64)
#define QAT_NUM_BUFS_IN_IM_SGL 1

#define ERR_CODE_QAT_COMP_WRONG_FW -99

enum qat_comp_request_type {
	QAT_COMP_REQUEST_FIXED_COMP_STATELESS,
	QAT_COMP_REQUEST_DYNAMIC_COMP_STATELESS,
	QAT_COMP_REQUEST_DECOMPRESS,
	REQ_COMP_END
};

struct array_of_ptrs {
	phys_addr_t pointer[0];
};

struct qat_inter_sgl {
	qat_sgl_hdr;
	struct qat_flat_buf buffers[QAT_NUM_BUFS_IN_IM_SGL];
} __rte_packed __rte_cache_aligned;

struct qat_comp_sgl {
	qat_sgl_hdr;
	struct qat_flat_buf buffers[RTE_PMD_QAT_COMP_SGL_MAX_SEGMENTS];
} __rte_packed __rte_cache_aligned;

struct qat_comp_op_cookie {
	struct qat_comp_sgl qat_sgl_src;
	struct qat_comp_sgl qat_sgl_dst;
	phys_addr_t qat_sgl_src_phys_addr;
	phys_addr_t qat_sgl_dst_phys_addr;
};

struct qat_comp_xform {
	struct icp_qat_fw_comp_req qat_comp_req_tmpl;
	enum qat_comp_request_type qat_comp_request_type;
	enum rte_comp_checksum_type checksum_type;
};

int
qat_comp_build_request(void *in_op, uint8_t *out_msg, void *op_cookie,
		       enum qat_device_gen qat_dev_gen __rte_unused);

int
qat_comp_process_response(void **op, uint8_t *resp,
			  uint64_t *dequeue_err_count);

int
qat_comp_private_xform_create(struct rte_compressdev *dev,
			      const struct rte_comp_xform *xform,
			      void **private_xform);

int
qat_comp_private_xform_free(struct rte_compressdev *dev, void *private_xform);

unsigned int
qat_comp_xform_size(void);

#endif
#endif
