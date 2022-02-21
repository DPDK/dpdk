/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 - 2022 Intel Corporation
 */

#include <stdarg.h>

#include <cryptodev_pmd.h>

#include "qat_device.h"
#include "qat_logs.h"

#include "qat_asym.h"
#include "icp_qat_fw_pke.h"
#include "icp_qat_fw.h"
#include "qat_pke.h"
#include "qat_ec.h"

uint8_t qat_asym_driver_id;

struct qat_crypto_gen_dev_ops qat_asym_gen_dev_ops[QAT_N_GENS];

/* An rte_driver is needed in the registration of both the device and the driver
 * with cryptodev.
 * The actual qat pci's rte_driver can't be used as its name represents
 * the whole pci device with all services. Think of this as a holder for a name
 * for the crypto part of the pci device.
 */
static const char qat_asym_drv_name[] = RTE_STR(CRYPTODEV_NAME_QAT_ASYM_PMD);
static const struct rte_driver cryptodev_qat_asym_driver = {
	.name = qat_asym_drv_name,
	.alias = qat_asym_drv_name
};

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
#define HEXDUMP(name, where, size) QAT_DP_HEXDUMP_LOG(DEBUG, name, \
			where, size)
#define HEXDUMP_OFF(name, where, size, idx) QAT_DP_HEXDUMP_LOG(DEBUG, name, \
			&where[idx * size], size)
#else
#define HEXDUMP(name, where, size)
#define HEXDUMP_OFF(name, where, size, idx)
#endif

#define CHECK_IF_NOT_EMPTY(param, name, pname, status) \
	do { \
		if (param.length == 0) {	\
			QAT_LOG(ERR,			\
				"Invalid " name	\
				" input parameter, zero length " pname	\
			);	\
			status = -EINVAL;	\
		} else if (check_zero(param)) { \
			QAT_LOG(ERR,	\
				"Invalid " name " input parameter, empty " \
				pname ", length = %d", \
				(int)param.length \
			); \
			status = -EINVAL;	\
		} \
	} while (0)

#define SET_PKE_LN(where, what, how, idx) \
	rte_memcpy(where[idx] + how - \
		what.length, \
		what.data, \
		what.length)

#define SET_PKE_LN_9A(where, what, how, idx) \
		rte_memcpy(&where[idx * RTE_ALIGN_CEIL(how, 8)] + \
			RTE_ALIGN_CEIL(how, 8) - \
			what.length, \
			what.data, \
			what.length)

#define SET_PKE_LN_EC(where, what, how, idx) \
		rte_memcpy(&where[idx * RTE_ALIGN_CEIL(how, 8)] + \
			RTE_ALIGN_CEIL(how, 8) - \
			how, \
			what.data, \
			how)

static void
request_init(struct icp_qat_fw_pke_request *qat_req)
{
	memset(qat_req, 0, sizeof(*qat_req));
	qat_req->pke_hdr.service_type = ICP_QAT_FW_COMN_REQ_CPM_FW_PKE;
	qat_req->pke_hdr.hdr_flags =
			ICP_QAT_FW_COMN_HDR_FLAGS_BUILD
			(ICP_QAT_FW_COMN_REQ_FLAG_SET);
}

static void
cleanup_arrays(struct qat_asym_op_cookie *cookie,
		int in_count, int out_count, int alg_size)
{
	int i;

	for (i = 0; i < in_count; i++)
		memset(cookie->input_array[i], 0x0, alg_size);
	for (i = 0; i < out_count; i++)
		memset(cookie->output_array[i], 0x0, alg_size);
}

static void
cleanup_crt(struct qat_asym_op_cookie *cookie,
		int alg_size)
{
	int i;

	memset(cookie->input_array[0], 0x0, alg_size);
	for (i = 1; i < QAT_ASYM_RSA_QT_NUM_IN_PARAMS; i++)
		memset(cookie->input_array[i], 0x0, alg_size / 2);
	for (i = 0; i < QAT_ASYM_RSA_NUM_OUT_PARAMS; i++)
		memset(cookie->output_array[i], 0x0, alg_size);
}

static void
cleanup(struct qat_asym_op_cookie *cookie,
		struct rte_crypto_asym_xform *xform, int alg_size)
{
	if (xform->xform_type == RTE_CRYPTO_ASYM_XFORM_MODEX)
		cleanup_arrays(cookie, QAT_ASYM_MODEXP_NUM_IN_PARAMS,
				QAT_ASYM_MODEXP_NUM_OUT_PARAMS, alg_size);
	else if (xform->xform_type == RTE_CRYPTO_ASYM_XFORM_MODINV)
		cleanup_arrays(cookie, QAT_ASYM_MODINV_NUM_IN_PARAMS,
				QAT_ASYM_MODINV_NUM_OUT_PARAMS, alg_size);
	else if (xform->xform_type == RTE_CRYPTO_ASYM_XFORM_RSA) {
		if (xform->rsa.key_type == RTE_RSA_KEY_TYPE_QT)
			cleanup_crt(cookie, alg_size);
		else {
			cleanup_arrays(cookie, QAT_ASYM_RSA_NUM_IN_PARAMS,
				QAT_ASYM_RSA_NUM_OUT_PARAMS, alg_size);
		}
	}
}

static int
check_zero(rte_crypto_param n)
{
	int i, len = n.length;

	if (len < 8) {
		for (i = len - 1; i >= 0; i--) {
			if (n.data[i] != 0x0)
				return 0;
		}
	} else if (len == 8 && *(uint64_t *)&n.data[len - 8] == 0) {
		return 1;
	} else if (*(uint64_t *)&n.data[len - 8] == 0) {
		for (i = len - 9; i >= 0; i--) {
			if (n.data[i] != 0x0)
				return 0;
		}
	} else
		return 0;

	return 1;
}

static struct qat_asym_function
get_asym_function(struct rte_crypto_asym_xform *xform)
{
	struct qat_asym_function qat_function;

	switch (xform->xform_type) {
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		qat_function = get_modexp_function(xform);
		break;
	case RTE_CRYPTO_ASYM_XFORM_MODINV:
		qat_function = get_modinv_function(xform);
		break;
	default:
		qat_function.func_id = 0;
		break;
	}

	return qat_function;
}

static int
modexp_set_input(struct rte_crypto_asym_op *asym_op,
		struct icp_qat_fw_pke_request *qat_req,
		struct qat_asym_op_cookie *cookie,
		struct rte_crypto_asym_xform *xform)
{
	struct qat_asym_function qat_function;
	uint32_t alg_bytesize, func_id;
	int status = 0;

	CHECK_IF_NOT_EMPTY(xform->modex.modulus, "mod exp",
			"modulus", status);
	CHECK_IF_NOT_EMPTY(xform->modex.exponent, "mod exp",
				"exponent", status);
	if (status)
		return status;

	qat_function = get_asym_function(xform);
	func_id = qat_function.func_id;
	if (qat_function.func_id == 0) {
		QAT_LOG(ERR, "Cannot obtain functionality id");
		return -EINVAL;
	}
	alg_bytesize = qat_function.bytesize;

	SET_PKE_LN(cookie->input_array, asym_op->modex.base,
			alg_bytesize, 0);
	SET_PKE_LN(cookie->input_array, xform->modex.exponent,
			alg_bytesize, 1);
	SET_PKE_LN(cookie->input_array, xform->modex.modulus,
			alg_bytesize, 2);

	cookie->alg_bytesize = alg_bytesize;
	qat_req->pke_hdr.cd_pars.func_id = func_id;
	qat_req->input_param_count = QAT_ASYM_MODEXP_NUM_IN_PARAMS;
	qat_req->output_param_count = QAT_ASYM_MODEXP_NUM_OUT_PARAMS;

	HEXDUMP("ModExp base", cookie->input_array[0], alg_bytesize);
	HEXDUMP("ModExp exponent", cookie->input_array[1], alg_bytesize);
	HEXDUMP("ModExp modulus", cookie->input_array[2], alg_bytesize);

	return status;
}

static uint8_t
modexp_collect(struct rte_crypto_asym_op *asym_op,
		struct qat_asym_op_cookie *cookie,
		struct rte_crypto_asym_xform *xform)
{
	rte_crypto_param n = xform->modex.modulus;
	uint32_t alg_bytesize = cookie->alg_bytesize;
	uint8_t *modexp_result = asym_op->modex.result.data;

	rte_memcpy(modexp_result,
		cookie->output_array[0] + alg_bytesize
		- n.length, n.length);
	HEXDUMP("ModExp result", cookie->output_array[0],
			alg_bytesize);
	return RTE_CRYPTO_OP_STATUS_SUCCESS;
}

static int
modinv_set_input(struct rte_crypto_asym_op *asym_op,
		struct icp_qat_fw_pke_request *qat_req,
		struct qat_asym_op_cookie *cookie,
		struct rte_crypto_asym_xform *xform)
{
	struct qat_asym_function qat_function;
	uint32_t alg_bytesize, func_id;
	int status = 0;

	CHECK_IF_NOT_EMPTY(xform->modex.modulus, "mod inv",
			"modulus", status);
	if (status)
		return status;

	qat_function = get_asym_function(xform);
	func_id = qat_function.func_id;
	if (func_id == 0) {
		QAT_LOG(ERR, "Cannot obtain functionality id");
		return -EINVAL;
	}
	alg_bytesize = qat_function.bytesize;

	SET_PKE_LN(cookie->input_array, asym_op->modinv.base,
			alg_bytesize, 0);
	SET_PKE_LN(cookie->input_array, xform->modinv.modulus,
			alg_bytesize, 1);

	cookie->alg_bytesize = alg_bytesize;
	qat_req->pke_hdr.cd_pars.func_id = func_id;
	qat_req->input_param_count =
			QAT_ASYM_MODINV_NUM_IN_PARAMS;
	qat_req->output_param_count =
			QAT_ASYM_MODINV_NUM_OUT_PARAMS;

	HEXDUMP("ModInv base", cookie->input_array[0], alg_bytesize);
	HEXDUMP("ModInv modulus", cookie->input_array[1], alg_bytesize);

	return 0;
}

static uint8_t
modinv_collect(struct rte_crypto_asym_op *asym_op,
		struct qat_asym_op_cookie *cookie,
		struct rte_crypto_asym_xform *xform)
{
	rte_crypto_param n = xform->modinv.modulus;
	uint8_t *modinv_result = asym_op->modinv.result.data;
	uint32_t alg_bytesize = cookie->alg_bytesize;

	rte_memcpy(modinv_result + (asym_op->modinv.result.length
		- n.length),
		cookie->output_array[0] + alg_bytesize
		- n.length, n.length);
	HEXDUMP("ModInv result", cookie->output_array[0],
			alg_bytesize);
	return RTE_CRYPTO_OP_STATUS_SUCCESS;
}

static int
rsa_set_pub_input(struct rte_crypto_asym_op *asym_op,
		struct icp_qat_fw_pke_request *qat_req,
		struct qat_asym_op_cookie *cookie,
		struct rte_crypto_asym_xform *xform)
{
	struct qat_asym_function qat_function;
	uint32_t alg_bytesize, func_id;
	int status = 0;

	qat_function = get_rsa_enc_function(xform);
	func_id = qat_function.func_id;
	if (func_id == 0) {
		QAT_LOG(ERR, "Cannot obtain functionality id");
		return -EINVAL;
	}
	alg_bytesize = qat_function.bytesize;

	if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_ENCRYPT) {
		switch (asym_op->rsa.pad) {
		case RTE_CRYPTO_RSA_PADDING_NONE:
			SET_PKE_LN(cookie->input_array, asym_op->rsa.message,
					alg_bytesize, 0);
			break;
		default:
			QAT_LOG(ERR,
				"Invalid RSA padding (Encryption)"
				);
			return -EINVAL;
		}
		HEXDUMP("RSA Message", cookie->input_array[0], alg_bytesize);
	} else {
		switch (asym_op->rsa.pad) {
		case RTE_CRYPTO_RSA_PADDING_NONE:
			SET_PKE_LN(cookie->input_array, asym_op->rsa.sign,
					alg_bytesize, 0);
			break;
		default:
			QAT_LOG(ERR,
				"Invalid RSA padding (Verify)");
			return -EINVAL;
		}
		HEXDUMP("RSA Signature", cookie->input_array[0],
				alg_bytesize);
	}

	SET_PKE_LN(cookie->input_array, xform->rsa.e,
			alg_bytesize, 1);
	SET_PKE_LN(cookie->input_array, xform->rsa.n,
			alg_bytesize, 2);

	cookie->alg_bytesize = alg_bytesize;
	qat_req->pke_hdr.cd_pars.func_id = func_id;

	HEXDUMP("RSA Public Key", cookie->input_array[1], alg_bytesize);
	HEXDUMP("RSA Modulus", cookie->input_array[2], alg_bytesize);

	return status;
}

static int
rsa_set_priv_input(struct rte_crypto_asym_op *asym_op,
		struct icp_qat_fw_pke_request *qat_req,
		struct qat_asym_op_cookie *cookie,
		struct rte_crypto_asym_xform *xform)
{
	struct qat_asym_function qat_function;
	uint32_t alg_bytesize, func_id;
	int status = 0;

	if (xform->rsa.key_type == RTE_RSA_KEY_TYPE_QT) {
		qat_function = get_rsa_crt_function(xform);
		func_id = qat_function.func_id;
		if (func_id == 0) {
			QAT_LOG(ERR, "Cannot obtain functionality id");
			return -EINVAL;
		}
		alg_bytesize = qat_function.bytesize;
		qat_req->input_param_count =
				QAT_ASYM_RSA_QT_NUM_IN_PARAMS;

		SET_PKE_LN(cookie->input_array, xform->rsa.qt.p,
			(alg_bytesize >> 1), 1);
		SET_PKE_LN(cookie->input_array, xform->rsa.qt.q,
			(alg_bytesize >> 1), 2);
		SET_PKE_LN(cookie->input_array, xform->rsa.qt.dP,
			(alg_bytesize >> 1), 3);
		SET_PKE_LN(cookie->input_array, xform->rsa.qt.dQ,
			(alg_bytesize >> 1), 4);
		SET_PKE_LN(cookie->input_array, xform->rsa.qt.qInv,
			(alg_bytesize >> 1), 5);

		HEXDUMP("RSA p", cookie->input_array[1],
				alg_bytesize);
		HEXDUMP("RSA q", cookie->input_array[2],
				alg_bytesize);
		HEXDUMP("RSA dP", cookie->input_array[3],
				alg_bytesize);
		HEXDUMP("RSA dQ", cookie->input_array[4],
				alg_bytesize);
		HEXDUMP("RSA qInv", cookie->input_array[5],
				alg_bytesize);
	} else if (xform->rsa.key_type ==
			RTE_RSA_KEY_TYPE_EXP) {
		qat_function = get_rsa_dec_function(xform);
		func_id = qat_function.func_id;
		if (func_id == 0) {
			QAT_LOG(ERR, "Cannot obtain functionality id");
			return -EINVAL;
		}
		alg_bytesize = qat_function.bytesize;

		SET_PKE_LN(cookie->input_array, xform->rsa.d,
			alg_bytesize, 1);
		SET_PKE_LN(cookie->input_array, xform->rsa.n,
			alg_bytesize, 2);

		HEXDUMP("RSA d", cookie->input_array[1],
				alg_bytesize);
		HEXDUMP("RSA n", cookie->input_array[2],
				alg_bytesize);
	} else {
		QAT_LOG(ERR, "Invalid RSA key type");
		return -EINVAL;
	}

	if (asym_op->rsa.op_type ==
			RTE_CRYPTO_ASYM_OP_DECRYPT) {
		switch (asym_op->rsa.pad) {
		case RTE_CRYPTO_RSA_PADDING_NONE:
			SET_PKE_LN(cookie->input_array, asym_op->rsa.cipher,
				alg_bytesize, 0);
			HEXDUMP("RSA ciphertext", cookie->input_array[0],
				alg_bytesize);
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
			SET_PKE_LN(cookie->input_array, asym_op->rsa.message,
				alg_bytesize, 0);
			HEXDUMP("RSA text to be signed", cookie->input_array[0],
				alg_bytesize);
			break;
		default:
			QAT_LOG(ERR,
				"Invalid padding of RSA (Signature)");
			return -(EINVAL);
		}
	}

	cookie->alg_bytesize = alg_bytesize;
	qat_req->pke_hdr.cd_pars.func_id = func_id;
	return status;
}

static int
rsa_set_input(struct rte_crypto_asym_op *asym_op,
		struct icp_qat_fw_pke_request *qat_req,
		struct qat_asym_op_cookie *cookie,
		struct rte_crypto_asym_xform *xform)
{
	qat_req->input_param_count =
			QAT_ASYM_RSA_NUM_IN_PARAMS;
	qat_req->output_param_count =
			QAT_ASYM_RSA_NUM_OUT_PARAMS;

	if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_ENCRYPT ||
			asym_op->rsa.op_type ==
				RTE_CRYPTO_ASYM_OP_VERIFY) {
		return rsa_set_pub_input(asym_op, qat_req, cookie, xform);
	} else {
		return rsa_set_priv_input(asym_op, qat_req, cookie, xform);
	}
}

static uint8_t
rsa_collect(struct rte_crypto_asym_op *asym_op,
		struct qat_asym_op_cookie *cookie)
{
	uint32_t alg_bytesize = cookie->alg_bytesize;

	if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_ENCRYPT ||
		asym_op->rsa.op_type ==	RTE_CRYPTO_ASYM_OP_VERIFY) {

		if (asym_op->rsa.op_type ==
				RTE_CRYPTO_ASYM_OP_ENCRYPT) {
			uint8_t *rsa_result = asym_op->rsa.cipher.data;

			rte_memcpy(rsa_result,
					cookie->output_array[0],
					alg_bytesize);
			HEXDUMP("RSA Encrypted data", cookie->output_array[0],
				alg_bytesize);
		} else {
			uint8_t *rsa_result = asym_op->rsa.cipher.data;

			switch (asym_op->rsa.pad) {
			case RTE_CRYPTO_RSA_PADDING_NONE:
				rte_memcpy(rsa_result,
						cookie->output_array[0],
						alg_bytesize);
				HEXDUMP("RSA signature",
					cookie->output_array[0],
					alg_bytesize);
				break;
			default:
				QAT_LOG(ERR, "Padding not supported");
				return RTE_CRYPTO_OP_STATUS_ERROR;
			}
		}
	} else {
		if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_DECRYPT) {
			uint8_t *rsa_result = asym_op->rsa.message.data;

			switch (asym_op->rsa.pad) {
			case RTE_CRYPTO_RSA_PADDING_NONE:
				rte_memcpy(rsa_result,
					cookie->output_array[0],
					alg_bytesize);
				HEXDUMP("RSA Decrypted Message",
					cookie->output_array[0],
					alg_bytesize);
				break;
			default:
				QAT_LOG(ERR, "Padding not supported");
				return RTE_CRYPTO_OP_STATUS_ERROR;
			}
		} else {
			uint8_t *rsa_result = asym_op->rsa.sign.data;

			rte_memcpy(rsa_result,
					cookie->output_array[0],
					alg_bytesize);
			HEXDUMP("RSA Signature", cookie->output_array[0],
				alg_bytesize);
		}
	}
	return RTE_CRYPTO_OP_STATUS_SUCCESS;
}


static int
asym_set_input(struct rte_crypto_asym_op *asym_op,
		struct icp_qat_fw_pke_request *qat_req,
		struct qat_asym_op_cookie *cookie,
		struct rte_crypto_asym_xform *xform)
{
	switch (xform->xform_type) {
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		return modexp_set_input(asym_op, qat_req,
				cookie, xform);
	case RTE_CRYPTO_ASYM_XFORM_MODINV:
		return modinv_set_input(asym_op, qat_req,
				cookie, xform);
	case RTE_CRYPTO_ASYM_XFORM_RSA:
		return rsa_set_input(asym_op, qat_req,
				cookie, xform);
	default:
		QAT_LOG(ERR, "Invalid/unsupported asymmetric crypto xform");
		return -EINVAL;
	}
	return 1;
}

static int
qat_asym_build_request(void *in_op, uint8_t *out_msg, void *op_cookie,
			__rte_unused uint64_t *opaque,
			__rte_unused enum qat_device_gen qat_dev_gen)
{
	struct rte_crypto_op *op = (struct rte_crypto_op *)in_op;
	struct rte_crypto_asym_op *asym_op = op->asym;
	struct icp_qat_fw_pke_request *qat_req =
			(struct icp_qat_fw_pke_request *)out_msg;
	struct qat_asym_op_cookie *cookie =
			(struct qat_asym_op_cookie *)op_cookie;
	int err = 0;

	op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
	switch (op->sess_type) {
	case RTE_CRYPTO_OP_WITH_SESSION:
		QAT_LOG(ERR,
			"QAT asymmetric crypto PMD does not support session"
			);
		goto error;
	case RTE_CRYPTO_OP_SESSIONLESS:
		request_init(qat_req);
		err = asym_set_input(asym_op, qat_req, cookie,
				op->asym->xform);
		if (err) {
			op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			goto error;
		}
		break;
	default:
		QAT_DP_LOG(ERR, "Invalid session/xform settings");
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
		goto error;
	}

	qat_req->pke_mid.opaque = (uint64_t)(uintptr_t)op;
	qat_req->pke_mid.src_data_addr = cookie->input_addr;
	qat_req->pke_mid.dest_data_addr = cookie->output_addr;

	HEXDUMP("qat_req:", qat_req, sizeof(struct icp_qat_fw_pke_request));

	return 0;
error:
	qat_req->pke_mid.opaque = (uint64_t)(uintptr_t)op;
	HEXDUMP("qat_req:", qat_req, sizeof(struct icp_qat_fw_pke_request));
	qat_req->output_param_count = 0;
	qat_req->input_param_count = 0;
	qat_req->pke_hdr.service_type = ICP_QAT_FW_COMN_REQ_NULL;
	cookie->error |= err;

	return 0;
}

static uint8_t
qat_asym_collect_response(struct rte_crypto_op *rx_op,
		struct qat_asym_op_cookie *cookie,
		struct rte_crypto_asym_xform *xform)
{
	struct rte_crypto_asym_op *asym_op = rx_op->asym;

	switch (xform->xform_type) {
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		return modexp_collect(asym_op, cookie, xform);
	case RTE_CRYPTO_ASYM_XFORM_MODINV:
		return modinv_collect(asym_op, cookie, xform);
	case RTE_CRYPTO_ASYM_XFORM_RSA:
		return rsa_collect(asym_op, cookie);
	default:
		QAT_LOG(ERR, "Not supported xform type");
		return  RTE_CRYPTO_OP_STATUS_ERROR;
	}
}

static int
qat_asym_process_response(void **op, uint8_t *resp,
		void *op_cookie, __rte_unused uint64_t *dequeue_err_count)
{
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
	if (rx_op->status == RTE_CRYPTO_OP_STATUS_NOT_PROCESSED) {
		rx_op->status = qat_asym_collect_response(rx_op,
					cookie, rx_op->asym->xform);
		cleanup(cookie, rx_op->asym->xform,
					cookie->alg_bytesize);
	}

	*op = rx_op;
	HEXDUMP("resp_msg:", resp_msg, sizeof(struct icp_qat_fw_pke_resp));

	return 1;
}

int
qat_asym_session_configure(struct rte_cryptodev *dev __rte_unused,
		struct rte_crypto_asym_xform *xform __rte_unused,
		struct rte_cryptodev_asym_session *sess __rte_unused)
{
	QAT_LOG(ERR, "QAT asymmetric PMD currently does not support session");
	return -ENOTSUP;
}

unsigned int
qat_asym_session_get_private_size(struct rte_cryptodev *dev __rte_unused)
{
	QAT_LOG(ERR, "QAT asymmetric PMD currently does not support session");
	return 0;
}

void
qat_asym_session_clear(struct rte_cryptodev *dev __rte_unused,
		struct rte_cryptodev_asym_session *sess __rte_unused)
{
	QAT_LOG(ERR, "QAT asymmetric PMD currently does not support session");
}

static uint16_t
qat_asym_crypto_enqueue_op_burst(void *qp, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	return qat_enqueue_op_burst(qp, qat_asym_build_request, (void **)ops,
			nb_ops);
}

static uint16_t
qat_asym_crypto_dequeue_op_burst(void *qp, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	return qat_dequeue_op_burst(qp, (void **)ops, qat_asym_process_response,
				nb_ops);
}

void
qat_asym_init_op_cookie(void *op_cookie)
{
	int j;
	struct qat_asym_op_cookie *cookie = op_cookie;

	cookie->input_addr = rte_mempool_virt2iova(cookie) +
			offsetof(struct qat_asym_op_cookie,
					input_params_ptrs);

	cookie->output_addr = rte_mempool_virt2iova(cookie) +
			offsetof(struct qat_asym_op_cookie,
					output_params_ptrs);

	for (j = 0; j < 8; j++) {
		cookie->input_params_ptrs[j] =
				rte_mempool_virt2iova(cookie) +
				offsetof(struct qat_asym_op_cookie,
						input_array[j]);
		cookie->output_params_ptrs[j] =
				rte_mempool_virt2iova(cookie) +
				offsetof(struct qat_asym_op_cookie,
						output_array[j]);
	}
}

int
qat_asym_dev_create(struct qat_pci_device *qat_pci_dev,
		struct qat_dev_cmd_param *qat_dev_cmd_param)
{
	struct qat_cryptodev_private *internals;
	struct rte_cryptodev *cryptodev;
	struct qat_device_info *qat_dev_instance =
		&qat_pci_devs[qat_pci_dev->qat_dev_id];
	struct rte_cryptodev_pmd_init_params init_params = {
		.name = "",
		.socket_id = qat_dev_instance->pci_dev->device.numa_node,
		.private_data_size = sizeof(struct qat_cryptodev_private)
	};
	struct qat_capabilities_info capa_info;
	const struct rte_cryptodev_capabilities *capabilities;
	const struct qat_crypto_gen_dev_ops *gen_dev_ops =
		&qat_asym_gen_dev_ops[qat_pci_dev->qat_dev_gen];
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	char capa_memz_name[RTE_CRYPTODEV_NAME_MAX_LEN];
	uint64_t capa_size;
	int i = 0;

	snprintf(name, RTE_CRYPTODEV_NAME_MAX_LEN, "%s_%s",
			qat_pci_dev->name, "asym");
	QAT_LOG(DEBUG, "Creating QAT ASYM device %s\n", name);

	if (gen_dev_ops->cryptodev_ops == NULL) {
		QAT_LOG(ERR, "Device %s does not support asymmetric crypto",
				name);
		return -(EFAULT);
	}

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		qat_pci_dev->qat_asym_driver_id =
				qat_asym_driver_id;
	} else if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		if (qat_pci_dev->qat_asym_driver_id !=
				qat_asym_driver_id) {
			QAT_LOG(ERR,
				"Device %s have different driver id than corresponding device in primary process",
				name);
			return -(EFAULT);
		}
	}

	/* Populate subset device to use in cryptodev device creation */
	qat_dev_instance->asym_rte_dev.driver = &cryptodev_qat_asym_driver;
	qat_dev_instance->asym_rte_dev.numa_node =
			qat_dev_instance->pci_dev->device.numa_node;
	qat_dev_instance->asym_rte_dev.devargs = NULL;

	cryptodev = rte_cryptodev_pmd_create(name,
			&(qat_dev_instance->asym_rte_dev), &init_params);

	if (cryptodev == NULL)
		return -ENODEV;

	qat_dev_instance->asym_rte_dev.name = cryptodev->data->name;
	cryptodev->driver_id = qat_asym_driver_id;
	cryptodev->dev_ops = gen_dev_ops->cryptodev_ops;

	cryptodev->enqueue_burst = qat_asym_crypto_enqueue_op_burst;
	cryptodev->dequeue_burst = qat_asym_crypto_dequeue_op_burst;

	cryptodev->feature_flags = gen_dev_ops->get_feature_flags(qat_pci_dev);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	snprintf(capa_memz_name, RTE_CRYPTODEV_NAME_MAX_LEN,
			"QAT_ASYM_CAPA_GEN_%d",
			qat_pci_dev->qat_dev_gen);

	internals = cryptodev->data->dev_private;
	internals->qat_dev = qat_pci_dev;
	internals->dev_id = cryptodev->data->dev_id;

	capa_info = gen_dev_ops->get_capabilities(qat_pci_dev);
	capabilities = capa_info.data;
	capa_size = capa_info.size;

	internals->capa_mz = rte_memzone_lookup(capa_memz_name);
	if (internals->capa_mz == NULL) {
		internals->capa_mz = rte_memzone_reserve(capa_memz_name,
				capa_size, rte_socket_id(), 0);
		if (internals->capa_mz == NULL) {
			QAT_LOG(DEBUG,
				"Error allocating memzone for capabilities, "
				"destroying PMD for %s",
				name);
			rte_cryptodev_pmd_destroy(cryptodev);
			memset(&qat_dev_instance->asym_rte_dev, 0,
				sizeof(qat_dev_instance->asym_rte_dev));
			return -EFAULT;
		}
	}

	memcpy(internals->capa_mz->addr, capabilities, capa_size);
	internals->qat_dev_capabilities = internals->capa_mz->addr;

	while (1) {
		if (qat_dev_cmd_param[i].name == NULL)
			break;
		if (!strcmp(qat_dev_cmd_param[i].name, ASYM_ENQ_THRESHOLD_NAME))
			internals->min_enq_burst_threshold =
					qat_dev_cmd_param[i].val;
		i++;
	}

	qat_pci_dev->asym_dev = internals;
	internals->service_type = QAT_SERVICE_ASYMMETRIC;
	QAT_LOG(DEBUG, "Created QAT ASYM device %s as cryptodev instance %d",
			cryptodev->data->name, internals->dev_id);
	return 0;
}

int
qat_asym_dev_destroy(struct qat_pci_device *qat_pci_dev)
{
	struct rte_cryptodev *cryptodev;

	if (qat_pci_dev == NULL)
		return -ENODEV;
	if (qat_pci_dev->asym_dev == NULL)
		return 0;
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_memzone_free(qat_pci_dev->asym_dev->capa_mz);

	/* free crypto device */
	cryptodev = rte_cryptodev_pmd_get_dev(
			qat_pci_dev->asym_dev->dev_id);
	rte_cryptodev_pmd_destroy(cryptodev);
	qat_pci_devs[qat_pci_dev->qat_dev_id].asym_rte_dev.name = NULL;
	qat_pci_dev->asym_dev = NULL;

	return 0;
}

static struct cryptodev_driver qat_crypto_drv;
RTE_PMD_REGISTER_CRYPTO_DRIVER(qat_crypto_drv,
		cryptodev_qat_asym_driver,
		qat_asym_driver_id);
