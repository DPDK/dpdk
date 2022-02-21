/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef _QAT_ASYM_H_
#define _QAT_ASYM_H_

#include <cryptodev_pmd.h>
#include <rte_crypto_asym.h>
#include "icp_qat_fw_pke.h"
#include "qat_device.h"
#include "qat_crypto.h"
#include "icp_qat_fw.h"

/** Intel(R) QAT Asymmetric Crypto PMD driver name */
#define CRYPTODEV_NAME_QAT_ASYM_PMD	crypto_qat_asym

typedef uint64_t large_int_ptr;
#define MAX_PKE_PARAMS	8
#define QAT_PKE_MAX_LN_SIZE 512
#define _PKE_ALIGN_ __rte_aligned(8)

#define QAT_ASYM_MAX_PARAMS			8
#define QAT_ASYM_MODINV_NUM_IN_PARAMS		2
#define QAT_ASYM_MODINV_NUM_OUT_PARAMS		1
#define QAT_ASYM_MODEXP_NUM_IN_PARAMS		3
#define QAT_ASYM_MODEXP_NUM_OUT_PARAMS		1
#define QAT_ASYM_RSA_NUM_IN_PARAMS		3
#define QAT_ASYM_RSA_NUM_OUT_PARAMS		1
#define QAT_ASYM_RSA_QT_NUM_IN_PARAMS		6
#define QAT_ASYM_ECDSA_RS_SIGN_IN_PARAMS	1
#define QAT_ASYM_ECDSA_RS_SIGN_OUT_PARAMS	2
#define QAT_ASYM_ECDSA_RS_VERIFY_IN_PARAMS	1
#define QAT_ASYM_ECDSA_RS_VERIFY_OUT_PARAMS	0
#define QAT_ASYM_ECPM_IN_PARAMS			7
#define QAT_ASYM_ECPM_OUT_PARAMS		2

/**
 * helper function to add an asym capability
 * <name> <op type> <modlen (min, max, increment)>
 **/
#define QAT_ASYM_CAP(n, o, l, r, i)					\
	{								\
		.op = RTE_CRYPTO_OP_TYPE_ASYMMETRIC,			\
		{.asym = {						\
			.xform_capa = {					\
				.xform_type = RTE_CRYPTO_ASYM_XFORM_##n,\
				.op_types = o,				\
				{					\
				.modlen = {				\
				.min = l,				\
				.max = r,				\
				.increment = i				\
				}, }					\
			}						\
		},							\
		}							\
	}

struct qat_asym_op_cookie {
	size_t alg_bytesize;
	uint64_t error;
	rte_iova_t input_addr;
	rte_iova_t output_addr;
	large_int_ptr input_params_ptrs[MAX_PKE_PARAMS] _PKE_ALIGN_;
	large_int_ptr output_params_ptrs[MAX_PKE_PARAMS] _PKE_ALIGN_;
	union {
		uint8_t input_array[MAX_PKE_PARAMS][QAT_PKE_MAX_LN_SIZE];
		uint8_t input_buffer[MAX_PKE_PARAMS * QAT_PKE_MAX_LN_SIZE];
	} _PKE_ALIGN_;
	uint8_t output_array[MAX_PKE_PARAMS][QAT_PKE_MAX_LN_SIZE] _PKE_ALIGN_;
} _PKE_ALIGN_;

struct qat_asym_session {
	struct icp_qat_fw_pke_request req_tmpl;
	struct rte_crypto_asym_xform xform;
};

static inline void
qat_fill_req_tmpl(struct icp_qat_fw_pke_request *qat_req)
{
	memset(qat_req, 0, sizeof(*qat_req));
	qat_req->pke_hdr.service_type = ICP_QAT_FW_COMN_REQ_CPM_FW_PKE;

	qat_req->pke_hdr.hdr_flags =
			ICP_QAT_FW_COMN_HDR_FLAGS_BUILD
			(ICP_QAT_FW_COMN_REQ_FLAG_SET);
}

static inline void
qat_asym_build_req_tmpl(void *sess_private_data)
{
	struct icp_qat_fw_pke_request *qat_req;
	struct qat_asym_session *session = sess_private_data;

	qat_req = &session->req_tmpl;
	qat_fill_req_tmpl(qat_req);
}

int
qat_asym_session_configure(struct rte_cryptodev *dev __rte_unused,
		struct rte_crypto_asym_xform *xform,
		struct rte_cryptodev_asym_session *sess);

unsigned int
qat_asym_session_get_private_size(struct rte_cryptodev *dev);

void
qat_asym_session_clear(struct rte_cryptodev *dev,
		struct rte_cryptodev_asym_session *sess);

void
qat_asym_init_op_cookie(void *cookie);

#endif /* _QAT_ASYM_H_ */
