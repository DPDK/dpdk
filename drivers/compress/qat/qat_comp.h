/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2019 Intel Corporation
 */

#ifndef _QAT_COMP_H_
#define _QAT_COMP_H_

#ifdef RTE_LIBRTE_COMPRESSDEV

#include <rte_compressdev.h>
#include <rte_compressdev_pmd.h>

#include "qat_common.h"
#include "qat_qp.h"
#include "icp_qat_hw.h"
#include "icp_qat_fw_comp.h"
#include "icp_qat_fw_la.h"

#define QAT_64_BYTE_ALIGN_MASK (~0x3f)
#define QAT_64_BYTE_ALIGN (64)
#define QAT_NUM_BUFS_IN_IM_SGL 1

#define ERR_CODE_QAT_COMP_WRONG_FW -99

/* fallback to fixed compression threshold */
#define QAT_FALLBACK_THLD ((uint32_t)(RTE_PMD_QAT_COMP_IM_BUFFER_SIZE / 1.3))

#define QAT_MIN_OUT_BUF_SIZE 46

/* maximum size of the state registers */
#define QAT_STATE_REGISTERS_MAX_SIZE 64

/* decompressor context size */
#define QAT_INFLATE_CONTEXT_SIZE_GEN1 36864
#define QAT_INFLATE_CONTEXT_SIZE_GEN2 34032
#define QAT_INFLATE_CONTEXT_SIZE_GEN3 34032
#define QAT_INFLATE_CONTEXT_SIZE RTE_MAX(RTE_MAX(QAT_INFLATE_CONTEXT_SIZE_GEN1,\
		QAT_INFLATE_CONTEXT_SIZE_GEN2), QAT_INFLATE_CONTEXT_SIZE_GEN3)

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


struct qat_comp_op_cookie {
	phys_addr_t qat_sgl_src_phys_addr;
	phys_addr_t qat_sgl_dst_phys_addr;
	/* dynamically created SGLs */
	uint8_t error;
	uint8_t socket_id;
	uint16_t src_nb_elems;
	uint16_t dst_nb_elems;
	struct qat_sgl *qat_sgl_src_d;
	struct qat_sgl *qat_sgl_dst_d;
	struct qat_qp *qp;
	uint32_t cookie_index;

	/* QAT IM buffer too small handling: */
	uint8_t split_op;
	uint8_t nb_children;

	/* used by the parent only */
	uint8_t nb_child_responses;
	uint32_t total_consumed;
	uint32_t total_produced;
	const struct rte_memzone **dst_memzones;
	struct rte_mbuf *dst_data;
	uint32_t dst_data_offset;

	/* used by the child only */
	struct qat_comp_op_cookie *parent_cookie;
	void *dest_buffer;
};

struct qat_comp_xform {
	struct icp_qat_fw_comp_req qat_comp_req_tmpl;
	enum qat_comp_request_type qat_comp_request_type;
	enum rte_comp_checksum_type checksum_type;
};

struct qat_comp_stream {
	struct qat_comp_xform qat_xform;
	void *state_registers_decomp;
	phys_addr_t state_registers_decomp_phys;
	void *inflate_context;
	phys_addr_t inflate_context_phys;
	const struct rte_memzone *memzone;
	uint8_t start_of_packet;
	volatile uint8_t op_in_progress;
};

int
qat_comp_build_request(void *in_op, uint8_t *out_msg, void *op_cookie,
		       enum qat_device_gen qat_dev_gen __rte_unused);

int
qat_comp_build_multiple_requests(void *in_op, struct qat_qp *qp,
				 uint32_t parent_tail, int nb_descr);

void
qat_comp_free_split_op_memzones(struct qat_comp_op_cookie *cookie,
				unsigned int nb_children);

int
qat_comp_process_response(void **op, uint8_t *resp, void *op_cookie,
			  uint64_t *dequeue_err_count);

int
qat_comp_private_xform_create(struct rte_compressdev *dev,
			      const struct rte_comp_xform *xform,
			      void **private_xform);

int
qat_comp_private_xform_free(struct rte_compressdev *dev, void *private_xform);

unsigned int
qat_comp_xform_size(void);

unsigned int
qat_comp_stream_size(void);

int
qat_comp_stream_create(struct rte_compressdev *dev,
		       const struct rte_comp_xform *xform,
		       void **stream);

int
qat_comp_stream_free(struct rte_compressdev *dev, void *stream);

#endif
#endif
