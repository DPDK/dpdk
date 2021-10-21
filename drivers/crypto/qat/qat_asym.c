/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <stdarg.h>

#include "qat_asym.h"
#include "icp_qat_fw_pke.h"
#include "icp_qat_fw.h"
#include "qat_pke_functionality_arrays.h"

#define qat_asym_sz_2param(arg) (arg, sizeof(arg)/sizeof(*arg))

static int qat_asym_get_sz_and_func_id(const uint32_t arr[][2],
		size_t arr_sz, size_t *size, uint32_t *func_id)
{
	size_t i;

	for (i = 0; i < arr_sz; i++) {
		if (*size <= arr[i][0]) {
			*size = arr[i][0];
			*func_id = arr[i][1];
			return 0;
		}
	}
	return -1;
}

static inline void qat_fill_req_tmpl(struct icp_qat_fw_pke_request *qat_req)
{
	memset(qat_req, 0, sizeof(*qat_req));
	qat_req->pke_hdr.service_type = ICP_QAT_FW_COMN_REQ_CPM_FW_PKE;

	qat_req->pke_hdr.hdr_flags =
			ICP_QAT_FW_COMN_HDR_FLAGS_BUILD
			(ICP_QAT_FW_COMN_REQ_FLAG_SET);
}

static inline void qat_asym_build_req_tmpl(void *sess_private_data)
{
	struct icp_qat_fw_pke_request *qat_req;
	struct qat_asym_session *session = sess_private_data;

	qat_req = &session->req_tmpl;
	qat_fill_req_tmpl(qat_req);
}

static size_t max_of(int n, ...)
{
	va_list args;
	size_t len = 0, num;
	int i;

	va_start(args, n);
	len = va_arg(args, size_t);

	for (i = 0; i < n - 1; i++) {
		num = va_arg(args, size_t);
		if (num > len)
			len = num;
	}
	va_end(args);

	return len;
}

static void qat_clear_arrays(struct qat_asym_op_cookie *cookie,
		int in_count, int out_count, int alg_size)
{
	int i;

	for (i = 0; i < in_count; i++)
		memset(cookie->input_array[i], 0x0, alg_size);
	for (i = 0; i < out_count; i++)
		memset(cookie->output_array[i], 0x0, alg_size);
}

static void qat_clear_arrays_crt(struct qat_asym_op_cookie *cookie,
		int alg_size)
{
	int i;

	memset(cookie->input_array[0], 0x0, alg_size);
	for (i = 1; i < QAT_ASYM_RSA_QT_NUM_IN_PARAMS; i++)
		memset(cookie->input_array[i], 0x0, alg_size / 2);
	for (i = 0; i < QAT_ASYM_RSA_NUM_OUT_PARAMS; i++)
		memset(cookie->output_array[i], 0x0, alg_size);
}

static void qat_clear_arrays_by_alg(struct qat_asym_op_cookie *cookie,
		struct rte_crypto_asym_xform *xform, int alg_size)
{
	if (xform->xform_type == RTE_CRYPTO_ASYM_XFORM_MODEX)
		qat_clear_arrays(cookie, QAT_ASYM_MODEXP_NUM_IN_PARAMS,
				QAT_ASYM_MODEXP_NUM_OUT_PARAMS, alg_size);
	else if (xform->xform_type == RTE_CRYPTO_ASYM_XFORM_MODINV)
		qat_clear_arrays(cookie, QAT_ASYM_MODINV_NUM_IN_PARAMS,
				QAT_ASYM_MODINV_NUM_OUT_PARAMS, alg_size);
	else if (xform->xform_type == RTE_CRYPTO_ASYM_XFORM_RSA) {
		if (xform->rsa.key_type == RTE_RSA_KET_TYPE_QT)
			qat_clear_arrays_crt(cookie, alg_size);
		else {
			qat_clear_arrays(cookie, QAT_ASYM_RSA_NUM_IN_PARAMS,
				QAT_ASYM_RSA_NUM_OUT_PARAMS, alg_size);
		}
	}
}

static int qat_asym_check_nonzero(rte_crypto_param n)
{
	if (n.length < 8) {
		/* Not a case for any cryptograpic function except for DH
		 * generator which very often can be of one byte length
		 */
		size_t i;

		if (n.data[n.length - 1] == 0x0) {
			for (i = 0; i < n.length - 1; i++)
				if (n.data[i] != 0x0)
					break;
			if (i == n.length - 1)
				return -(EINVAL);
		}
	} else if (*(uint64_t *)&n.data[
				n.length - 8] == 0) {
		/* Very likely it is zeroed modulus */
		size_t i;

		for (i = 0; i < n.length - 8; i++)
			if (n.data[i] != 0x0)
				break;
		if (i == n.length - 8)
			return -(EINVAL);
	}

	return 0;
}

static int
qat_asym_fill_arrays(struct rte_crypto_asym_op *asym_op,
		struct icp_qat_fw_pke_request *qat_req,
		struct qat_asym_op_cookie *cookie,
		struct rte_crypto_asym_xform *xform)
{
	int err = 0;
	size_t alg_size;
	size_t alg_size_in_bytes;
	uint32_t func_id = 0;

	if (xform->xform_type == RTE_CRYPTO_ASYM_XFORM_MODEX) {
		err = qat_asym_check_nonzero(xform->modex.modulus);
		if (err) {
			QAT_LOG(ERR, "Empty modulus in modular exponentiation,"
					" aborting this operation");
			return err;
		}

		alg_size_in_bytes = max_of(3, asym_op->modex.base.length,
			       xform->modex.exponent.length,
			       xform->modex.modulus.length);
		alg_size = alg_size_in_bytes << 3;

		if (qat_asym_get_sz_and_func_id(MOD_EXP_SIZE,
				sizeof(MOD_EXP_SIZE)/sizeof(*MOD_EXP_SIZE),
				&alg_size, &func_id)) {
			return -(EINVAL);
		}

		alg_size_in_bytes = alg_size >> 3;
		rte_memcpy(cookie->input_array[0] + alg_size_in_bytes -
			asym_op->modex.base.length
			, asym_op->modex.base.data,
			asym_op->modex.base.length);
		rte_memcpy(cookie->input_array[1] + alg_size_in_bytes -
			xform->modex.exponent.length
			, xform->modex.exponent.data,
			xform->modex.exponent.length);
		rte_memcpy(cookie->input_array[2]  + alg_size_in_bytes -
			xform->modex.modulus.length,
			xform->modex.modulus.data,
			xform->modex.modulus.length);
		cookie->alg_size = alg_size;
		qat_req->pke_hdr.cd_pars.func_id = func_id;
		qat_req->input_param_count = QAT_ASYM_MODEXP_NUM_IN_PARAMS;
		qat_req->output_param_count = QAT_ASYM_MODEXP_NUM_OUT_PARAMS;
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
		QAT_DP_HEXDUMP_LOG(DEBUG, "ModExp base",
				cookie->input_array[0],
				alg_size_in_bytes);
		QAT_DP_HEXDUMP_LOG(DEBUG, "ModExp exponent",
				cookie->input_array[1],
				alg_size_in_bytes);
		QAT_DP_HEXDUMP_LOG(DEBUG, " ModExpmodulus",
				cookie->input_array[2],
				alg_size_in_bytes);
#endif
	} else if (xform->xform_type == RTE_CRYPTO_ASYM_XFORM_MODINV) {
		err = qat_asym_check_nonzero(xform->modinv.modulus);
		if (err) {
			QAT_LOG(ERR, "Empty modulus in modular multiplicative"
					" inverse, aborting this operation");
			return err;
		}

		alg_size_in_bytes = max_of(2, asym_op->modinv.base.length,
				xform->modinv.modulus.length);
		alg_size = alg_size_in_bytes << 3;

		if (xform->modinv.modulus.data[
				xform->modinv.modulus.length - 1] & 0x01) {
			if (qat_asym_get_sz_and_func_id(MOD_INV_IDS_ODD,
					sizeof(MOD_INV_IDS_ODD)/
					sizeof(*MOD_INV_IDS_ODD),
					&alg_size, &func_id)) {
				return -(EINVAL);
			}
		} else {
			if (qat_asym_get_sz_and_func_id(MOD_INV_IDS_EVEN,
					sizeof(MOD_INV_IDS_EVEN)/
					sizeof(*MOD_INV_IDS_EVEN),
					&alg_size, &func_id)) {
				return -(EINVAL);
			}
		}

		alg_size_in_bytes = alg_size >> 3;
		rte_memcpy(cookie->input_array[0] + alg_size_in_bytes -
			asym_op->modinv.base.length
				, asym_op->modinv.base.data,
				asym_op->modinv.base.length);
		rte_memcpy(cookie->input_array[1] + alg_size_in_bytes -
				xform->modinv.modulus.length
				, xform->modinv.modulus.data,
				xform->modinv.modulus.length);
		cookie->alg_size = alg_size;
		qat_req->pke_hdr.cd_pars.func_id = func_id;
		qat_req->input_param_count =
				QAT_ASYM_MODINV_NUM_IN_PARAMS;
		qat_req->output_param_count =
				QAT_ASYM_MODINV_NUM_OUT_PARAMS;
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
		QAT_DP_HEXDUMP_LOG(DEBUG, "ModInv base",
				cookie->input_array[0],
				alg_size_in_bytes);
		QAT_DP_HEXDUMP_LOG(DEBUG, "ModInv modulus",
				cookie->input_array[1],
				alg_size_in_bytes);
#endif
	} else if (xform->xform_type == RTE_CRYPTO_ASYM_XFORM_RSA) {
		err = qat_asym_check_nonzero(xform->rsa.n);
		if (err) {
			QAT_LOG(ERR, "Empty modulus in RSA"
					" inverse, aborting this operation");
			return err;
		}

		alg_size_in_bytes = xform->rsa.n.length;
		alg_size = alg_size_in_bytes << 3;

		qat_req->input_param_count =
				QAT_ASYM_RSA_NUM_IN_PARAMS;
		qat_req->output_param_count =
				QAT_ASYM_RSA_NUM_OUT_PARAMS;

		if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_ENCRYPT ||
				asym_op->rsa.op_type ==
						RTE_CRYPTO_ASYM_OP_VERIFY) {

			if (qat_asym_get_sz_and_func_id(RSA_ENC_IDS,
					sizeof(RSA_ENC_IDS)/
					sizeof(*RSA_ENC_IDS),
					&alg_size, &func_id)) {
				err = -(EINVAL);
				QAT_LOG(ERR,
					"Not supported RSA parameter size (key)");
				return err;
			}
			alg_size_in_bytes = alg_size >> 3;
			if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_ENCRYPT) {
				switch (asym_op->rsa.pad) {
				case RTE_CRYPTO_RSA_PADDING_NONE:
					rte_memcpy(cookie->input_array[0] +
						alg_size_in_bytes -
						asym_op->rsa.message.length
						, asym_op->rsa.message.data,
						asym_op->rsa.message.length);
					break;
				default:
					err = -(EINVAL);
					QAT_LOG(ERR,
						"Invalid RSA padding (Encryption)");
					return err;
				}
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
				QAT_DP_HEXDUMP_LOG(DEBUG, "RSA Message",
						cookie->input_array[0],
						alg_size_in_bytes);
#endif
			} else {
				switch (asym_op->rsa.pad) {
				case RTE_CRYPTO_RSA_PADDING_NONE:
					rte_memcpy(cookie->input_array[0],
						asym_op->rsa.sign.data,
						alg_size_in_bytes);
					break;
				default:
					err = -(EINVAL);
					QAT_LOG(ERR,
						"Invalid RSA padding (Verify)");
					return err;
				}

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
				QAT_DP_HEXDUMP_LOG(DEBUG, " RSA Signature",
						cookie->input_array[0],
						alg_size_in_bytes);
#endif

			}
			rte_memcpy(cookie->input_array[1] +
					alg_size_in_bytes -
					xform->rsa.e.length
					, xform->rsa.e.data,
					xform->rsa.e.length);
			rte_memcpy(cookie->input_array[2] +
					alg_size_in_bytes -
					xform->rsa.n.length,
					xform->rsa.n.data,
					xform->rsa.n.length);

			cookie->alg_size = alg_size;
			qat_req->pke_hdr.cd_pars.func_id = func_id;

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
			QAT_DP_HEXDUMP_LOG(DEBUG, "RSA Public Key",
					cookie->input_array[1], alg_size_in_bytes);
			QAT_DP_HEXDUMP_LOG(DEBUG, "RSA Modulus",
					cookie->input_array[2], alg_size_in_bytes);
#endif
		} else {
			if (asym_op->rsa.op_type ==
					RTE_CRYPTO_ASYM_OP_DECRYPT) {
				switch (asym_op->rsa.pad) {
				case RTE_CRYPTO_RSA_PADDING_NONE:
					rte_memcpy(cookie->input_array[0]
						+ alg_size_in_bytes -
						asym_op->rsa.cipher.length,
						asym_op->rsa.cipher.data,
						asym_op->rsa.cipher.length);
					break;
				default:
					QAT_LOG(ERR,
						"Invalid padding of RSA (Decrypt)");
					return -(EINVAL);
				}

			} else if (asym_op->rsa.op_type ==
					RTE_CRYPTO_ASYM_OP_SIGN) {
				switch (asym_op->rsa.pad) {
				case RTE_CRYPTO_RSA_PADDING_NONE:
					rte_memcpy(cookie->input_array[0]
						+ alg_size_in_bytes -
						asym_op->rsa.message.length,
						asym_op->rsa.message.data,
						asym_op->rsa.message.length);
					break;
				default:
					QAT_LOG(ERR,
						"Invalid padding of RSA (Signature)");
					return -(EINVAL);
				}
			}
			if (xform->rsa.key_type == RTE_RSA_KET_TYPE_QT) {

				qat_req->input_param_count =
						QAT_ASYM_RSA_QT_NUM_IN_PARAMS;
				if (qat_asym_get_sz_and_func_id(RSA_DEC_CRT_IDS,
						sizeof(RSA_DEC_CRT_IDS)/
						sizeof(*RSA_DEC_CRT_IDS),
						&alg_size, &func_id)) {
					return -(EINVAL);
				}
				alg_size_in_bytes = alg_size >> 3;

				rte_memcpy(cookie->input_array[1] +
						(alg_size_in_bytes >> 1) -
						xform->rsa.qt.p.length
						, xform->rsa.qt.p.data,
						xform->rsa.qt.p.length);
				rte_memcpy(cookie->input_array[2] +
						(alg_size_in_bytes >> 1) -
						xform->rsa.qt.q.length
						, xform->rsa.qt.q.data,
						xform->rsa.qt.q.length);
				rte_memcpy(cookie->input_array[3] +
						(alg_size_in_bytes >> 1) -
						xform->rsa.qt.dP.length
						, xform->rsa.qt.dP.data,
						xform->rsa.qt.dP.length);
				rte_memcpy(cookie->input_array[4] +
						(alg_size_in_bytes >> 1) -
						xform->rsa.qt.dQ.length
						, xform->rsa.qt.dQ.data,
						xform->rsa.qt.dQ.length);
				rte_memcpy(cookie->input_array[5] +
						(alg_size_in_bytes >> 1) -
						xform->rsa.qt.qInv.length
						, xform->rsa.qt.qInv.data,
						xform->rsa.qt.qInv.length);
				cookie->alg_size = alg_size;
				qat_req->pke_hdr.cd_pars.func_id = func_id;

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
				QAT_DP_HEXDUMP_LOG(DEBUG, "C",
						cookie->input_array[0],
						alg_size_in_bytes);
				QAT_DP_HEXDUMP_LOG(DEBUG, "p",
						cookie->input_array[1],
						alg_size_in_bytes);
				QAT_DP_HEXDUMP_LOG(DEBUG, "q",
						cookie->input_array[2],
						alg_size_in_bytes);
				QAT_DP_HEXDUMP_LOG(DEBUG,
						"dP", cookie->input_array[3],
						alg_size_in_bytes);
				QAT_DP_HEXDUMP_LOG(DEBUG,
						"dQ", cookie->input_array[4],
						alg_size_in_bytes);
				QAT_DP_HEXDUMP_LOG(DEBUG,
						"qInv", cookie->input_array[5],
						alg_size_in_bytes);
#endif
			} else if (xform->rsa.key_type ==
					RTE_RSA_KEY_TYPE_EXP) {
				if (qat_asym_get_sz_and_func_id(
						RSA_DEC_IDS,
						sizeof(RSA_DEC_IDS)/
						sizeof(*RSA_DEC_IDS),
						&alg_size, &func_id)) {
					return -(EINVAL);
				}
				alg_size_in_bytes = alg_size >> 3;
				rte_memcpy(cookie->input_array[1] +
						alg_size_in_bytes -
						xform->rsa.d.length,
						xform->rsa.d.data,
						xform->rsa.d.length);
				rte_memcpy(cookie->input_array[2] +
						alg_size_in_bytes -
						xform->rsa.n.length,
						xform->rsa.n.data,
						xform->rsa.n.length);
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
			QAT_DP_HEXDUMP_LOG(DEBUG, "RSA ciphertext",
					cookie->input_array[0],
					alg_size_in_bytes);
			QAT_DP_HEXDUMP_LOG(DEBUG, "RSA d", cookie->input_array[1],
					alg_size_in_bytes);
			QAT_DP_HEXDUMP_LOG(DEBUG, "RSA n", cookie->input_array[2],
					alg_size_in_bytes);
#endif

				cookie->alg_size = alg_size;
				qat_req->pke_hdr.cd_pars.func_id = func_id;
			} else {
				QAT_LOG(ERR, "Invalid RSA key type");
				return -(EINVAL);
			}
		}
	} else {
		QAT_LOG(ERR, "Invalid asymmetric crypto xform");
		return -(EINVAL);
	}
	return 0;
}

int
qat_asym_build_request(void *in_op,
			uint8_t *out_msg,
			void *op_cookie,
			__rte_unused enum qat_device_gen qat_dev_gen)
{
	struct qat_asym_session *ctx;
	struct rte_crypto_op *op = (struct rte_crypto_op *)in_op;
	struct rte_crypto_asym_op *asym_op = op->asym;
	struct icp_qat_fw_pke_request *qat_req =
			(struct icp_qat_fw_pke_request *)out_msg;
	struct qat_asym_op_cookie *cookie =
				(struct qat_asym_op_cookie *)op_cookie;
	int err = 0;

	op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
	if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
		ctx = (struct qat_asym_session *)
			get_asym_session_private_data(
			op->asym->session, qat_asym_driver_id);
		if (unlikely(ctx == NULL)) {
			QAT_LOG(ERR, "Session has not been created for this device");
			goto error;
		}
		rte_mov64((uint8_t *)qat_req, (const uint8_t *)&(ctx->req_tmpl));
		err = qat_asym_fill_arrays(asym_op, qat_req, cookie, ctx->xform);
		if (err) {
			op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			goto error;
		}
	} else if (op->sess_type == RTE_CRYPTO_OP_SESSIONLESS) {
		qat_fill_req_tmpl(qat_req);
		err = qat_asym_fill_arrays(asym_op, qat_req, cookie,
				op->asym->xform);
		if (err) {
			op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			goto error;
		}
	} else {
		QAT_DP_LOG(ERR, "Invalid session/xform settings");
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
		goto error;
	}

	qat_req->pke_mid.opaque = (uint64_t)(uintptr_t)op;
	qat_req->pke_mid.src_data_addr = cookie->input_addr;
	qat_req->pke_mid.dest_data_addr = cookie->output_addr;

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	QAT_DP_HEXDUMP_LOG(DEBUG, "qat_req:", qat_req,
			sizeof(struct icp_qat_fw_pke_request));
#endif

	return 0;
error:

	qat_req->pke_mid.opaque = (uint64_t)(uintptr_t)op;

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	QAT_DP_HEXDUMP_LOG(DEBUG, "qat_req:", qat_req,
		sizeof(struct icp_qat_fw_pke_request));
#endif

	qat_req->output_param_count = 0;
	qat_req->input_param_count = 0;
	qat_req->pke_hdr.service_type = ICP_QAT_FW_COMN_REQ_NULL;
	cookie->error |= err;

	return 0;
}

static void qat_asym_collect_response(struct rte_crypto_op *rx_op,
		struct qat_asym_op_cookie *cookie,
		struct rte_crypto_asym_xform *xform)
{
	size_t alg_size, alg_size_in_bytes = 0;
	struct rte_crypto_asym_op *asym_op = rx_op->asym;

	if (xform->xform_type == RTE_CRYPTO_ASYM_XFORM_MODEX) {
		rte_crypto_param n = xform->modex.modulus;

		alg_size = cookie->alg_size;
		alg_size_in_bytes = alg_size >> 3;
		uint8_t *modexp_result = asym_op->modex.result.data;

		if (rx_op->status == RTE_CRYPTO_OP_STATUS_NOT_PROCESSED) {
			rte_memcpy(modexp_result +
				(asym_op->modex.result.length -
					n.length),
				cookie->output_array[0] + alg_size_in_bytes
				- n.length, n.length
				);
			rx_op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
			QAT_DP_HEXDUMP_LOG(DEBUG, "ModExp result",
					cookie->output_array[0],
					alg_size_in_bytes);

#endif
		}
	} else if (xform->xform_type == RTE_CRYPTO_ASYM_XFORM_MODINV) {
		rte_crypto_param n = xform->modinv.modulus;

		alg_size = cookie->alg_size;
		alg_size_in_bytes = alg_size >> 3;
		uint8_t *modinv_result = asym_op->modinv.result.data;

		if (rx_op->status == RTE_CRYPTO_OP_STATUS_NOT_PROCESSED) {
			rte_memcpy(modinv_result + (asym_op->modinv.result.length
				- n.length),
				cookie->output_array[0] + alg_size_in_bytes
				- n.length, n.length);
			rx_op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
			QAT_DP_HEXDUMP_LOG(DEBUG, "ModInv result",
					cookie->output_array[0],
					alg_size_in_bytes);
#endif
		}
	} else if (xform->xform_type == RTE_CRYPTO_ASYM_XFORM_RSA) {

		alg_size = cookie->alg_size;
		alg_size_in_bytes = alg_size >> 3;
		if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_ENCRYPT ||
				asym_op->rsa.op_type ==
					RTE_CRYPTO_ASYM_OP_VERIFY) {
			if (asym_op->rsa.op_type ==
					RTE_CRYPTO_ASYM_OP_ENCRYPT) {
				uint8_t *rsa_result = asym_op->rsa.cipher.data;

				rte_memcpy(rsa_result,
						cookie->output_array[0],
						alg_size_in_bytes);
				rx_op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
				QAT_DP_HEXDUMP_LOG(DEBUG, "RSA Encrypted data",
						cookie->output_array[0],
						alg_size_in_bytes);
#endif
			} else if (asym_op->rsa.op_type ==
					RTE_CRYPTO_ASYM_OP_VERIFY) {
				uint8_t *rsa_result = asym_op->rsa.cipher.data;

				switch (asym_op->rsa.pad) {
				case RTE_CRYPTO_RSA_PADDING_NONE:
					rte_memcpy(rsa_result,
							cookie->output_array[0],
							alg_size_in_bytes);
					rx_op->status =
						RTE_CRYPTO_OP_STATUS_SUCCESS;
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
				QAT_DP_HEXDUMP_LOG(DEBUG, "RSA Signature",
						cookie->output_array[0],
						alg_size_in_bytes);
#endif
					break;
				default:
					QAT_LOG(ERR, "Padding not supported");
					rx_op->status =
						RTE_CRYPTO_OP_STATUS_ERROR;
					break;
				}
			}
		} else {
			if (asym_op->rsa.op_type ==
					RTE_CRYPTO_ASYM_OP_DECRYPT) {
				uint8_t *rsa_result = asym_op->rsa.message.data;

				switch (asym_op->rsa.pad) {
				case RTE_CRYPTO_RSA_PADDING_NONE:
					rte_memcpy(rsa_result,
						cookie->output_array[0],
						alg_size_in_bytes);
					rx_op->status =
						RTE_CRYPTO_OP_STATUS_SUCCESS;
					break;
				default:
					QAT_LOG(ERR, "Padding not supported");
					rx_op->status =
						RTE_CRYPTO_OP_STATUS_ERROR;
					break;
				}
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
				QAT_DP_HEXDUMP_LOG(DEBUG, "RSA Decrypted Message",
						rsa_result, alg_size_in_bytes);
#endif
			} else if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_SIGN) {
				uint8_t *rsa_result = asym_op->rsa.sign.data;

				rte_memcpy(rsa_result,
						cookie->output_array[0],
						alg_size_in_bytes);
				rx_op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
				QAT_DP_HEXDUMP_LOG(DEBUG, "RSA Signature",
						cookie->output_array[0],
						alg_size_in_bytes);
#endif
			}
		}
	}
	qat_clear_arrays_by_alg(cookie, xform, alg_size_in_bytes);
}

void
qat_asym_process_response(void **op, uint8_t *resp,
		void *op_cookie)
{
	struct qat_asym_session *ctx;
	struct icp_qat_fw_pke_resp *resp_msg =
			(struct icp_qat_fw_pke_resp *)resp;
	struct rte_crypto_op *rx_op = (struct rte_crypto_op *)(uintptr_t)
			(resp_msg->opaque);
	struct qat_asym_op_cookie *cookie = op_cookie;

	if (cookie->error) {
		cookie->error = 0;
		if (rx_op->status == RTE_CRYPTO_OP_STATUS_NOT_PROCESSED)
			rx_op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		QAT_DP_LOG(ERR, "Cookie status returned error");
	} else {
		if (ICP_QAT_FW_PKE_RESP_PKE_STAT_GET(
			resp_msg->pke_resp_hdr.resp_status.pke_resp_flags)) {
			if (rx_op->status == RTE_CRYPTO_OP_STATUS_NOT_PROCESSED)
				rx_op->status = RTE_CRYPTO_OP_STATUS_ERROR;
			QAT_DP_LOG(ERR, "Asymmetric response status"
					" returned error");
		}
		if (resp_msg->pke_resp_hdr.resp_status.comn_err_code) {
			if (rx_op->status == RTE_CRYPTO_OP_STATUS_NOT_PROCESSED)
				rx_op->status = RTE_CRYPTO_OP_STATUS_ERROR;
			QAT_DP_LOG(ERR, "Asymmetric common status"
					" returned error");
		}
	}

	if (rx_op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
		ctx = (struct qat_asym_session *)get_asym_session_private_data(
			rx_op->asym->session, qat_asym_driver_id);
		qat_asym_collect_response(rx_op, cookie, ctx->xform);
	} else if (rx_op->sess_type == RTE_CRYPTO_OP_SESSIONLESS) {
		qat_asym_collect_response(rx_op, cookie, rx_op->asym->xform);
	}
	*op = rx_op;

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	QAT_DP_HEXDUMP_LOG(DEBUG, "resp_msg:", resp_msg,
			sizeof(struct icp_qat_fw_pke_resp));
#endif
}

int
qat_asym_session_configure(struct rte_cryptodev *dev,
		struct rte_crypto_asym_xform *xform,
		struct rte_cryptodev_asym_session *sess,
		struct rte_mempool *mempool)
{
	int err = 0;
	void *sess_private_data;
	struct qat_asym_session *session;

	if (rte_mempool_get(mempool, &sess_private_data)) {
		QAT_LOG(ERR,
			"Couldn't get object from session mempool");
		return -ENOMEM;
	}

	session = sess_private_data;
	if (xform->xform_type == RTE_CRYPTO_ASYM_XFORM_MODEX) {
		if (xform->modex.exponent.length == 0 ||
				xform->modex.modulus.length == 0) {
			QAT_LOG(ERR, "Invalid mod exp input parameter");
			err = -EINVAL;
			goto error;
		}
	} else if (xform->xform_type == RTE_CRYPTO_ASYM_XFORM_MODINV) {
		if (xform->modinv.modulus.length == 0) {
			QAT_LOG(ERR, "Invalid mod inv input parameter");
			err = -EINVAL;
			goto error;
		}
	} else if (xform->xform_type == RTE_CRYPTO_ASYM_XFORM_RSA) {
		if (xform->rsa.n.length == 0) {
			QAT_LOG(ERR, "Invalid rsa input parameter");
			err = -EINVAL;
			goto error;
		}
	} else if (xform->xform_type >= RTE_CRYPTO_ASYM_XFORM_TYPE_LIST_END
			|| xform->xform_type <= RTE_CRYPTO_ASYM_XFORM_NONE) {
		QAT_LOG(ERR, "Invalid asymmetric crypto xform");
		err = -EINVAL;
		goto error;
	} else {
		QAT_LOG(ERR, "Asymmetric crypto xform not implemented");
		err = -EINVAL;
		goto error;
	}

	session->xform = xform;
	qat_asym_build_req_tmpl(sess_private_data);
	set_asym_session_private_data(sess, dev->driver_id,
		sess_private_data);

	return 0;
error:
	rte_mempool_put(mempool, sess_private_data);
	return err;
}

unsigned int qat_asym_session_get_private_size(
		struct rte_cryptodev *dev __rte_unused)
{
	return RTE_ALIGN_CEIL(sizeof(struct qat_asym_session), 8);
}

void
qat_asym_session_clear(struct rte_cryptodev *dev,
		struct rte_cryptodev_asym_session *sess)
{
	uint8_t index = dev->driver_id;
	void *sess_priv = get_asym_session_private_data(sess, index);
	struct qat_asym_session *s = (struct qat_asym_session *)sess_priv;

	if (sess_priv) {
		memset(s, 0, qat_asym_session_get_private_size(dev));
		struct rte_mempool *sess_mp = rte_mempool_from_obj(sess_priv);

		set_asym_session_private_data(sess, index, NULL);
		rte_mempool_put(sess_mp, sess_priv);
	}
}
