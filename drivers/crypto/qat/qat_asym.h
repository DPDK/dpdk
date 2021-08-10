/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _QAT_ASYM_H_
#define _QAT_ASYM_H_

#include <cryptodev_pmd.h>
#include <rte_crypto_asym.h>
#include "icp_qat_fw_pke.h"
#include "qat_common.h"
#include "qat_asym_pmd.h"
#include "icp_qat_fw.h"

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

struct qat_asym_op_cookie {
	size_t alg_size;
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
	struct rte_crypto_asym_xform *xform;
};

int
qat_asym_session_configure(struct rte_cryptodev *dev,
		struct rte_crypto_asym_xform *xform,
		struct rte_cryptodev_asym_session *sess,
		struct rte_mempool *mempool);

unsigned int
qat_asym_session_get_private_size(struct rte_cryptodev *dev);

void
qat_asym_session_clear(struct rte_cryptodev *dev,
		struct rte_cryptodev_asym_session *sess);

/*
 * Build PKE request to be sent to the fw, partially uses template
 * request generated during session creation.
 *
 * @param	in_op		Pointer to the crypto operation, for every
 *				service it points to service specific struct.
 * @param	out_msg		Message to be returned to enqueue function
 * @param	op_cookie	Cookie pointer that holds private metadata
 * @param	qat_dev_gen	Generation of QAT hardware
 *
 * @return
 *	This function always returns zero,
 *	it is because of backward compatibility.
 *	- 0: Always returned
 *
 */
int
qat_asym_build_request(void *in_op, uint8_t *out_msg,
		void *op_cookie, enum qat_device_gen qat_dev_gen);

/*
 * Process PKE response received from outgoing queue of QAT
 *
 * @param	op		a ptr to the rte_crypto_op referred to by
 *				the response message is returned in this param
 * @param	resp		icp_qat_fw_pke_resp message received from
 *				outgoing fw message queue
 * @param	op_cookie	Cookie pointer that holds private metadata
 *
 */
void
qat_asym_process_response(void __rte_unused **op, uint8_t *resp,
		void *op_cookie);

#endif /* _QAT_ASYM_H_ */
