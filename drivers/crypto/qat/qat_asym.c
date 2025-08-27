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

#define ASYM_ENQ_THRESHOLD_NAME "qat_asym_enq_threshold"
#define RSA_MODULUS_2048_BITS 2048

uint8_t qat_asym_driver_id;

struct qat_crypto_gen_dev_ops qat_asym_gen_dev_ops[QAT_N_GENS];


static const char *const arguments[] = {
	ASYM_ENQ_THRESHOLD_NAME,
	NULL
};

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

/*
 * Macros with suffix _F are used with some of predefinded identifiers:
 * - cookie->input_buffer
 * - qat_func_alignsize
 */
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
#define HEXDUMP(name, where, size) QAT_DP_HEXDUMP_LOG(DEBUG, name, \
			where, size)
#define HEXDUMP_OFF(name, where, size, idx) QAT_DP_HEXDUMP_LOG(DEBUG, name, \
			&where[idx * size], size)

#define HEXDUMP_OFF_F(name, idx) QAT_DP_HEXDUMP_LOG(DEBUG, name, \
			&cookie->input_buffer[idx * qat_func_alignsize], \
			qat_func_alignsize)
#else
#define HEXDUMP(name, where, size)
#define HEXDUMP_OFF(name, where, size, idx)
#define HEXDUMP_OFF_F(name, idx)
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

#define SET_PKE_LN(what, how, idx) \
	rte_memcpy(cookie->input_array[idx] + how - \
		what.length, \
		what.data, \
		what.length)

#define SET_PKE_LN_EC(curve, p, idx) \
	rte_memcpy(cookie->input_array[idx] + \
		qat_func_alignsize - curve.bytesize, \
		curve.p.data, curve.bytesize)

#define SET_PKE_9A_IN(what, idx) \
	rte_memcpy(&cookie->input_buffer[idx * \
		qat_func_alignsize] + \
		qat_func_alignsize - what.length, \
		what.data, what.length)

#define SET_PKE_9A_EC(curve, p, idx) \
	rte_memcpy(&cookie->input_buffer[idx * \
		qat_func_alignsize] + \
		qat_func_alignsize - curve.bytesize, \
		curve.p.data, curve.bytesize)

#define PARAM_CLR(what) \
	rte_free_sensitive(what.data)

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
		const struct rte_crypto_asym_xform *xform)
{
	if (xform->xform_type == RTE_CRYPTO_ASYM_XFORM_MODEX)
		cleanup_arrays(cookie, QAT_ASYM_MODEXP_NUM_IN_PARAMS,
				QAT_ASYM_MODEXP_NUM_OUT_PARAMS,
				cookie->alg_bytesize);
	else if (xform->xform_type == RTE_CRYPTO_ASYM_XFORM_MODINV)
		cleanup_arrays(cookie, QAT_ASYM_MODINV_NUM_IN_PARAMS,
				QAT_ASYM_MODINV_NUM_OUT_PARAMS,
				cookie->alg_bytesize);
	else if (xform->xform_type == RTE_CRYPTO_ASYM_XFORM_RSA) {
		if (xform->rsa.key_type == RTE_RSA_KEY_TYPE_QT)
			cleanup_crt(cookie, cookie->alg_bytesize);
		else {
			cleanup_arrays(cookie, QAT_ASYM_RSA_NUM_IN_PARAMS,
				QAT_ASYM_RSA_NUM_OUT_PARAMS,
				cookie->alg_bytesize);
		}
	} else {
		cleanup_arrays(cookie, QAT_ASYM_MAX_PARAMS,
				QAT_ASYM_MAX_PARAMS,
				QAT_PKE_MAX_LN_SIZE);
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
get_asym_function(const struct rte_crypto_asym_xform *xform)
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
modexp_set_input(struct icp_qat_fw_pke_request *qat_req,
		struct qat_asym_op_cookie *cookie,
		const struct rte_crypto_asym_op *asym_op,
		const struct rte_crypto_asym_xform *xform)
{
	struct qat_asym_function qat_function;
	uint32_t alg_bytesize, func_id, in_bytesize;
	int status = 0;

	CHECK_IF_NOT_EMPTY(xform->modex.modulus, "mod exp",
			"modulus", status);
	CHECK_IF_NOT_EMPTY(xform->modex.exponent, "mod exp",
				"exponent", status);
	if (status)
		return status;

	if (asym_op->modex.base.length > xform->modex.exponent.length &&
		asym_op->modex.base.length > xform->modex.modulus.length) {
		in_bytesize = asym_op->modex.base.length;
	} else if (xform->modex.exponent.length > xform->modex.modulus.length)
		in_bytesize = xform->modex.exponent.length;
	else
		in_bytesize = xform->modex.modulus.length;

	qat_function = get_modexp_function2(in_bytesize);
	func_id = qat_function.func_id;
	if (qat_function.func_id == 0) {
		QAT_LOG(ERR, "Cannot obtain functionality id");
		return -EINVAL;
	}
	alg_bytesize = qat_function.bytesize;

	SET_PKE_LN(asym_op->modex.base, alg_bytesize, 0);
	SET_PKE_LN(xform->modex.exponent, alg_bytesize, 1);
	SET_PKE_LN(xform->modex.modulus, alg_bytesize, 2);

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
		const struct qat_asym_op_cookie *cookie,
		const struct rte_crypto_asym_xform *xform)
{
	rte_crypto_param n = xform->modex.modulus;
	uint32_t alg_bytesize = cookie->alg_bytesize;
	uint8_t *modexp_result = asym_op->modex.result.data;

	if (n.length > alg_bytesize) {
		QAT_LOG(ERR, "Incorrect length of modexp modulus");
		return RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
	}
	rte_memcpy(modexp_result,
		cookie->output_array[0] + alg_bytesize
		- n.length, n.length);
	asym_op->modex.result.length = alg_bytesize;
	HEXDUMP("ModExp result", cookie->output_array[0],
			alg_bytesize);
	return RTE_CRYPTO_OP_STATUS_SUCCESS;
}

static int
modinv_set_input(struct icp_qat_fw_pke_request *qat_req,
		struct qat_asym_op_cookie *cookie,
		const struct rte_crypto_asym_op *asym_op,
		const struct rte_crypto_asym_xform *xform)
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

	SET_PKE_LN(asym_op->modinv.base, alg_bytesize, 0);
	SET_PKE_LN(xform->modinv.modulus, alg_bytesize, 1);

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
		const struct qat_asym_op_cookie *cookie,
		const struct rte_crypto_asym_xform *xform)
{
	rte_crypto_param n = xform->modinv.modulus;
	uint8_t *modinv_result = asym_op->modinv.result.data;
	uint32_t alg_bytesize = cookie->alg_bytesize;

	if (n.length > alg_bytesize) {
		QAT_LOG(ERR, "Incorrect length of modinv modulus");
		return RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
	}
	rte_memcpy(modinv_result + (asym_op->modinv.result.length
		- n.length),
		cookie->output_array[0] + alg_bytesize
		- n.length, n.length);
	asym_op->modinv.result.length = alg_bytesize;
	HEXDUMP("ModInv result", cookie->output_array[0],
			alg_bytesize);
	return RTE_CRYPTO_OP_STATUS_SUCCESS;
}

static int
rsa_set_pub_input(struct icp_qat_fw_pke_request *qat_req,
		struct qat_asym_op_cookie *cookie,
		const struct rte_crypto_asym_op *asym_op,
		const struct rte_crypto_asym_xform *xform)
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
		switch (xform->rsa.padding.type) {
		case RTE_CRYPTO_RSA_PADDING_NONE:
			SET_PKE_LN(asym_op->rsa.message, alg_bytesize, 0);
			break;
		default:
			QAT_LOG(ERR,
				"Invalid RSA padding (Encryption)"
				);
			return -EINVAL;
		}
		HEXDUMP("RSA Message", cookie->input_array[0], alg_bytesize);
	} else {
		switch (xform->rsa.padding.type) {
		case RTE_CRYPTO_RSA_PADDING_NONE:
			SET_PKE_LN(asym_op->rsa.sign, alg_bytesize, 0);
			break;
		default:
			QAT_LOG(ERR,
				"Invalid RSA padding (Verify)");
			return -EINVAL;
		}
		HEXDUMP("RSA Signature", cookie->input_array[0],
				alg_bytesize);
	}

	SET_PKE_LN(xform->rsa.e, alg_bytesize, 1);
	SET_PKE_LN(xform->rsa.n, alg_bytesize, 2);

	cookie->alg_bytesize = alg_bytesize;
	qat_req->pke_hdr.cd_pars.func_id = func_id;

	HEXDUMP("RSA Public Key", cookie->input_array[1], alg_bytesize);
	HEXDUMP("RSA Modulus", cookie->input_array[2], alg_bytesize);

	return status;
}

static int
rsa_set_priv_input(struct icp_qat_fw_pke_request *qat_req,
		struct qat_asym_op_cookie *cookie,
		const struct rte_crypto_asym_op *asym_op,
		const struct rte_crypto_asym_xform *xform)
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

		SET_PKE_LN(xform->rsa.qt.p, (alg_bytesize >> 1), 1);
		SET_PKE_LN(xform->rsa.qt.q, (alg_bytesize >> 1), 2);
		SET_PKE_LN(xform->rsa.qt.dP, (alg_bytesize >> 1), 3);
		SET_PKE_LN(xform->rsa.qt.dQ, (alg_bytesize >> 1), 4);
		SET_PKE_LN(xform->rsa.qt.qInv, (alg_bytesize >> 1), 5);

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

		SET_PKE_LN(xform->rsa.d, alg_bytesize, 1);
		SET_PKE_LN(xform->rsa.n, alg_bytesize, 2);

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
		switch (xform->rsa.padding.type) {
		case RTE_CRYPTO_RSA_PADDING_NONE:
			SET_PKE_LN(asym_op->rsa.cipher,	alg_bytesize, 0);
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
		switch (xform->rsa.padding.type) {
		case RTE_CRYPTO_RSA_PADDING_NONE:
			SET_PKE_LN(asym_op->rsa.message, alg_bytesize, 0);
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
rsa_set_input(struct icp_qat_fw_pke_request *qat_req,
		struct qat_asym_op_cookie *cookie,
		const struct rte_crypto_asym_op *asym_op,
		const struct rte_crypto_asym_xform *xform)
{
	qat_req->input_param_count =
			QAT_ASYM_RSA_NUM_IN_PARAMS;
	qat_req->output_param_count =
			QAT_ASYM_RSA_NUM_OUT_PARAMS;

	if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_ENCRYPT ||
			asym_op->rsa.op_type ==
				RTE_CRYPTO_ASYM_OP_VERIFY) {
		return rsa_set_pub_input(qat_req, cookie, asym_op, xform);
	} else {
		return rsa_set_priv_input(qat_req, cookie, asym_op, xform);
	}
}

static uint8_t
rsa_collect(struct rte_crypto_asym_op *asym_op,
		const struct qat_asym_op_cookie *cookie,
		const struct rte_crypto_asym_xform *xform)
{
	uint32_t alg_bytesize = cookie->alg_bytesize;

	if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_ENCRYPT ||
		asym_op->rsa.op_type ==	RTE_CRYPTO_ASYM_OP_VERIFY) {

		if (asym_op->rsa.op_type ==
				RTE_CRYPTO_ASYM_OP_ENCRYPT) {
			rte_memcpy(asym_op->rsa.cipher.data,
					cookie->output_array[0],
					alg_bytesize);
			asym_op->rsa.cipher.length = alg_bytesize;
			HEXDUMP("RSA Encrypted data", cookie->output_array[0],
				alg_bytesize);
		} else {
			switch (xform->rsa.padding.type) {
			case RTE_CRYPTO_RSA_PADDING_NONE:
				rte_memcpy(asym_op->rsa.cipher.data,
						cookie->output_array[0],
						alg_bytesize);
				asym_op->rsa.cipher.length = alg_bytesize;
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
			switch (xform->rsa.padding.type) {
			case RTE_CRYPTO_RSA_PADDING_NONE:
				rte_memcpy(asym_op->rsa.message.data,
					cookie->output_array[0],
					alg_bytesize);
				asym_op->rsa.message.length = alg_bytesize;
				HEXDUMP("RSA Decrypted Message",
					cookie->output_array[0],
					alg_bytesize);
				break;
			default:
				QAT_LOG(ERR, "Padding not supported");
				return RTE_CRYPTO_OP_STATUS_ERROR;
			}
		} else {
			rte_memcpy(asym_op->rsa.sign.data,
				cookie->output_array[0],
				alg_bytesize);
			asym_op->rsa.sign.length = alg_bytesize;
			HEXDUMP("RSA Signature", cookie->output_array[0],
				alg_bytesize);
		}
	}
	return RTE_CRYPTO_OP_STATUS_SUCCESS;
}

static int
ecdsa_set_input(struct icp_qat_fw_pke_request *qat_req,
		struct qat_asym_op_cookie *cookie,
		const struct rte_crypto_asym_op *asym_op,
		const struct rte_crypto_asym_xform *xform)
{
	struct qat_asym_function qat_function;
	uint32_t qat_func_alignsize, func_id;
	int curve_id;

	curve_id = pick_curve(xform);
	if (curve_id < 0) {
		QAT_LOG(DEBUG, "Incorrect elliptic curve");
		return -EINVAL;
	}

	switch (asym_op->ecdsa.op_type) {
	case RTE_CRYPTO_ASYM_OP_SIGN:
		qat_function = get_ecdsa_function(xform);
		func_id = qat_function.func_id;
		if (func_id == 0) {
			QAT_LOG(ERR, "Cannot obtain functionality id");
			return -EINVAL;
		}
		qat_func_alignsize =
			RTE_ALIGN_CEIL(qat_function.bytesize, 8);

		SET_PKE_9A_IN(xform->ec.pkey, 0);
		SET_PKE_9A_IN(asym_op->ecdsa.message, 1);
		SET_PKE_9A_IN(asym_op->ecdsa.k, 2);
		SET_PKE_9A_EC(curve[curve_id], b, 3);
		SET_PKE_9A_EC(curve[curve_id], a, 4);
		SET_PKE_9A_EC(curve[curve_id], p, 5);
		SET_PKE_9A_EC(curve[curve_id], n, 6);
		SET_PKE_9A_EC(curve[curve_id], y, 7);
		SET_PKE_9A_EC(curve[curve_id], x, 8);

		cookie->alg_bytesize = curve[curve_id].bytesize;
		cookie->qat_func_alignsize = qat_func_alignsize;
		qat_req->pke_hdr.cd_pars.func_id = func_id;
		qat_req->input_param_count =
				QAT_ASYM_ECDSA_RS_SIGN_IN_PARAMS;
		qat_req->output_param_count =
				QAT_ASYM_ECDSA_RS_SIGN_OUT_PARAMS;

		HEXDUMP_OFF_F("ECDSA d", 0);
		HEXDUMP_OFF_F("ECDSA e", 1);
		HEXDUMP_OFF_F("ECDSA k", 2);
		HEXDUMP_OFF_F("ECDSA b", 3);
		HEXDUMP_OFF_F("ECDSA a", 4);
		HEXDUMP_OFF_F("ECDSA n", 5);
		HEXDUMP_OFF_F("ECDSA y", 6);
		HEXDUMP_OFF_F("ECDSA x", 7);
		break;
	case RTE_CRYPTO_ASYM_OP_VERIFY:
		qat_function = get_ecdsa_verify_function(xform);
		func_id = qat_function.func_id;
		if (func_id == 0) {
			QAT_LOG(ERR, "Cannot obtain functionality id");
			return -EINVAL;
		}
		qat_func_alignsize = RTE_ALIGN_CEIL(qat_function.bytesize, 8);

		SET_PKE_9A_IN(asym_op->ecdsa.message, 10);
		SET_PKE_9A_IN(asym_op->ecdsa.s, 9);
		SET_PKE_9A_IN(asym_op->ecdsa.r, 8);
		SET_PKE_9A_EC(curve[curve_id], n, 7);
		SET_PKE_9A_EC(curve[curve_id], x, 6);
		SET_PKE_9A_EC(curve[curve_id], y, 5);
		SET_PKE_9A_IN(xform->ec.q.x, 4);
		SET_PKE_9A_IN(xform->ec.q.y, 3);
		SET_PKE_9A_EC(curve[curve_id], a, 2);
		SET_PKE_9A_EC(curve[curve_id], b, 1);
		SET_PKE_9A_EC(curve[curve_id], p, 0);

		cookie->alg_bytesize = curve[curve_id].bytesize;
		cookie->qat_func_alignsize = qat_func_alignsize;
		qat_req->pke_hdr.cd_pars.func_id = func_id;
		qat_req->input_param_count =
				QAT_ASYM_ECDSA_RS_VERIFY_IN_PARAMS;
		qat_req->output_param_count =
				QAT_ASYM_ECDSA_RS_VERIFY_OUT_PARAMS;

		HEXDUMP_OFF_F("p", 0);
		HEXDUMP_OFF_F("b", 1);
		HEXDUMP_OFF_F("a", 2);
		HEXDUMP_OFF_F("y", 3);
		HEXDUMP_OFF_F("x", 4);
		HEXDUMP_OFF_F("yG", 5);
		HEXDUMP_OFF_F("xG", 6);
		HEXDUMP_OFF_F("n", 7);
		HEXDUMP_OFF_F("r", 8);
		HEXDUMP_OFF_F("s", 9);
		HEXDUMP_OFF_F("e", 10);
		break;
	default:
		return -1;
	}

	return 0;
}

static uint8_t
ecdsa_collect(struct rte_crypto_asym_op *asym_op,
		const struct qat_asym_op_cookie *cookie)
{
	uint32_t alg_bytesize = cookie->alg_bytesize;
	uint32_t qat_func_alignsize = cookie->qat_func_alignsize;
	uint32_t ltrim = qat_func_alignsize - alg_bytesize;

	if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_SIGN) {
		uint8_t *r = asym_op->ecdsa.r.data;
		uint8_t *s = asym_op->ecdsa.s.data;

		asym_op->ecdsa.r.length = alg_bytesize;
		asym_op->ecdsa.s.length = alg_bytesize;
		rte_memcpy(r, &cookie->output_array[0][ltrim], alg_bytesize);
		rte_memcpy(s, &cookie->output_array[1][ltrim], alg_bytesize);

		HEXDUMP("R", cookie->output_array[0],
			qat_func_alignsize);
		HEXDUMP("S", cookie->output_array[1],
			qat_func_alignsize);
	}
	return RTE_CRYPTO_OP_STATUS_SUCCESS;
}

static int
ecpm_set_input(struct icp_qat_fw_pke_request *qat_req,
		struct qat_asym_op_cookie *cookie,
		const struct rte_crypto_asym_op *asym_op,
		const struct rte_crypto_asym_xform *xform)
{
	struct qat_asym_function qat_function;
	uint32_t qat_func_alignsize, func_id;
	int curve_id;

	curve_id = pick_curve(xform);
	if (curve_id < 0) {
		QAT_LOG(DEBUG, "Incorrect elliptic curve");
		return -EINVAL;
	}

	qat_function = get_ecpm_function(xform);
	func_id = qat_function.func_id;
	if (func_id == 0) {
		QAT_LOG(ERR, "Cannot obtain functionality id");
		return -EINVAL;
	}
	qat_func_alignsize = RTE_ALIGN_CEIL(qat_function.bytesize, 8);

	SET_PKE_LN(asym_op->ecpm.scalar, qat_func_alignsize, 0);
	SET_PKE_LN(asym_op->ecpm.p.x, qat_func_alignsize, 1);
	SET_PKE_LN(asym_op->ecpm.p.y, qat_func_alignsize, 2);
	SET_PKE_LN_EC(curve[curve_id], a, 3);
	SET_PKE_LN_EC(curve[curve_id], b, 4);
	SET_PKE_LN_EC(curve[curve_id], p, 5);
	SET_PKE_LN_EC(curve[curve_id], h, 6);

	cookie->alg_bytesize = curve[curve_id].bytesize;
	cookie->qat_func_alignsize = qat_func_alignsize;
	qat_req->pke_hdr.cd_pars.func_id = func_id;
	qat_req->input_param_count =
			QAT_ASYM_ECPM_IN_PARAMS;
	qat_req->output_param_count =
			QAT_ASYM_ECPM_OUT_PARAMS;

	HEXDUMP("k", cookie->input_array[0], qat_func_alignsize);
	HEXDUMP("xG", cookie->input_array[1], qat_func_alignsize);
	HEXDUMP("yG", cookie->input_array[2], qat_func_alignsize);
	HEXDUMP("a", cookie->input_array[3], qat_func_alignsize);
	HEXDUMP("b", cookie->input_array[4], qat_func_alignsize);
	HEXDUMP("q", cookie->input_array[5], qat_func_alignsize);
	HEXDUMP("h", cookie->input_array[6], qat_func_alignsize);

	return 0;
}

static uint8_t
ecpm_collect(struct rte_crypto_asym_op *asym_op,
		const struct qat_asym_op_cookie *cookie)
{
	uint8_t *x = asym_op->ecpm.r.x.data;
	uint8_t *y = asym_op->ecpm.r.y.data;
	uint32_t alg_bytesize = cookie->alg_bytesize;
	uint32_t qat_func_alignsize = cookie->qat_func_alignsize;
	uint32_t ltrim = qat_func_alignsize - alg_bytesize;

	asym_op->ecpm.r.x.length = alg_bytesize;
	asym_op->ecpm.r.y.length = alg_bytesize;
	rte_memcpy(x, &cookie->output_array[0][ltrim], alg_bytesize);
	rte_memcpy(y, &cookie->output_array[1][ltrim], alg_bytesize);

	HEXDUMP("rX", cookie->output_array[0],
		qat_func_alignsize);
	HEXDUMP("rY", cookie->output_array[1],
		qat_func_alignsize);
	return RTE_CRYPTO_OP_STATUS_SUCCESS;
}

static int
ecdh_set_input(struct icp_qat_fw_pke_request *qat_req,
		struct qat_asym_op_cookie *cookie,
		const struct rte_crypto_asym_op *asym_op,
		const struct rte_crypto_asym_xform *xform)
{
	struct qat_asym_function qat_function;
	uint32_t qat_func_alignsize, func_id;
	int curve_id;

	curve_id = pick_curve(xform);
	if (curve_id < 0) {
		QAT_LOG(DEBUG, "Incorrect elliptic curve");
		return -EINVAL;
	}

	qat_function = get_ecpm_function(xform);
	func_id = qat_function.func_id;
	if (func_id == 0) {
		QAT_LOG(ERR, "Cannot obtain functionality id");
		return -EINVAL;
	}
	qat_func_alignsize = RTE_ALIGN_CEIL(qat_function.bytesize, 8);

	if (asym_op->ecdh.ke_type == RTE_CRYPTO_ASYM_KE_PUB_KEY_GENERATE) {
		SET_PKE_LN(xform->ec.pkey, qat_func_alignsize, 0);
		SET_PKE_LN_EC(curve[curve_id], x, 1);
		SET_PKE_LN_EC(curve[curve_id], y, 2);
	} else if (asym_op->ecdh.ke_type == RTE_CRYPTO_ASYM_KE_SHARED_SECRET_COMPUTE) {
		SET_PKE_LN(xform->ec.pkey, qat_func_alignsize, 0);
		SET_PKE_LN(xform->ec.q.x, qat_func_alignsize, 1);
		SET_PKE_LN(xform->ec.q.y, qat_func_alignsize, 2);
	} else {
		return -EINVAL;
	}

	SET_PKE_LN_EC(curve[curve_id], a, 3);
	SET_PKE_LN_EC(curve[curve_id], b, 4);
	SET_PKE_LN_EC(curve[curve_id], p, 5);
	SET_PKE_LN_EC(curve[curve_id], h, 6);

	cookie->alg_bytesize = curve[curve_id].bytesize;
	cookie->qat_func_alignsize = qat_func_alignsize;
	qat_req->pke_hdr.cd_pars.func_id = func_id;
	qat_req->input_param_count =
			QAT_ASYM_ECPM_IN_PARAMS;
	qat_req->output_param_count =
			QAT_ASYM_ECPM_OUT_PARAMS;

	HEXDUMP("k", cookie->input_array[0], qat_func_alignsize);
	HEXDUMP("xG", cookie->input_array[1], qat_func_alignsize);
	HEXDUMP("yG", cookie->input_array[2], qat_func_alignsize);
	HEXDUMP("a", cookie->input_array[3], qat_func_alignsize);
	HEXDUMP("b", cookie->input_array[4], qat_func_alignsize);
	HEXDUMP("q", cookie->input_array[5], qat_func_alignsize);
	HEXDUMP("h", cookie->input_array[6], qat_func_alignsize);

	return 0;
}

static int
ecdh_verify_set_input(struct icp_qat_fw_pke_request *qat_req,
		struct qat_asym_op_cookie *cookie,
		const struct rte_crypto_asym_op *asym_op,
		const struct rte_crypto_asym_xform *xform)
{
	struct qat_asym_function qat_function;
	uint32_t qat_func_alignsize, func_id;
	int curve_id;

	curve_id = pick_curve(xform);
	if (curve_id < 0) {
		QAT_LOG(DEBUG, "Incorrect elliptic curve");
		return -EINVAL;
	}

	qat_function = get_ec_verify_function(xform);
	func_id = qat_function.func_id;
	if (func_id == 0) {
		QAT_LOG(ERR, "Cannot obtain functionality id");
		return -EINVAL;
	}
	qat_func_alignsize = RTE_ALIGN_CEIL(qat_function.bytesize, 8);

	SET_PKE_LN(asym_op->ecdh.pub_key.x, qat_func_alignsize, 0);
	SET_PKE_LN(asym_op->ecdh.pub_key.y, qat_func_alignsize, 1);
	SET_PKE_LN_EC(curve[curve_id], p, 2);
	SET_PKE_LN_EC(curve[curve_id], a, 3);
	SET_PKE_LN_EC(curve[curve_id], b, 4);

	cookie->alg_bytesize = curve[curve_id].bytesize;
	cookie->qat_func_alignsize = qat_func_alignsize;
	qat_req->pke_hdr.cd_pars.func_id = func_id;
	qat_req->input_param_count =
			5;
	qat_req->output_param_count =
			0;

	HEXDUMP("x", cookie->input_array[0], qat_func_alignsize);
	HEXDUMP("y", cookie->input_array[1], qat_func_alignsize);
	HEXDUMP("p", cookie->input_array[2], qat_func_alignsize);
	HEXDUMP("a", cookie->input_array[3], qat_func_alignsize);
	HEXDUMP("b", cookie->input_array[4], qat_func_alignsize);

	return 0;
}

static uint8_t
ecdh_collect(struct rte_crypto_asym_op *asym_op,
		const struct qat_asym_op_cookie *cookie)
{
	uint8_t *x, *y;
	uint32_t alg_bytesize = cookie->alg_bytesize;
	uint32_t qat_func_alignsize = cookie->qat_func_alignsize;
	uint32_t ltrim = qat_func_alignsize - alg_bytesize;

	if (asym_op->ecdh.ke_type == RTE_CRYPTO_ASYM_KE_PUB_KEY_VERIFY)
		return RTE_CRYPTO_OP_STATUS_SUCCESS;

	if (asym_op->ecdh.ke_type == RTE_CRYPTO_ASYM_KE_PUB_KEY_GENERATE) {
		asym_op->ecdh.pub_key.x.length = alg_bytesize;
		asym_op->ecdh.pub_key.y.length = alg_bytesize;
		x = asym_op->ecdh.pub_key.x.data;
		y = asym_op->ecdh.pub_key.y.data;
	} else if (asym_op->ecdh.ke_type == RTE_CRYPTO_ASYM_KE_SHARED_SECRET_COMPUTE) {
		asym_op->ecdh.shared_secret.x.length = alg_bytesize;
		asym_op->ecdh.shared_secret.y.length = alg_bytesize;
		x = asym_op->ecdh.shared_secret.x.data;
		y = asym_op->ecdh.shared_secret.y.data;
	} else {
		return -EINVAL;
	}

	rte_memcpy(x, &cookie->output_array[0][ltrim], alg_bytesize);
	rte_memcpy(y, &cookie->output_array[1][ltrim], alg_bytesize);

	HEXDUMP("X", cookie->output_array[0],
		qat_func_alignsize);
	HEXDUMP("Y", cookie->output_array[1],
		qat_func_alignsize);
	return RTE_CRYPTO_OP_STATUS_SUCCESS;
}

static int
sm2_ecdsa_sign_set_input(struct icp_qat_fw_pke_request *qat_req,
		struct qat_asym_op_cookie *cookie,
		const struct rte_crypto_asym_op *asym_op,
		const struct rte_crypto_asym_xform *xform)
{
	const struct qat_asym_function qat_function =
		get_sm2_ecdsa_sign_function();
	const uint32_t qat_func_alignsize =
		qat_function.bytesize;

	SET_PKE_LN(asym_op->sm2.k, qat_func_alignsize, 0);
	SET_PKE_LN(asym_op->sm2.message, qat_func_alignsize, 1);
	SET_PKE_LN(xform->ec.pkey, qat_func_alignsize, 2);

	cookie->alg_bytesize = qat_function.bytesize;
	cookie->qat_func_alignsize = qat_function.bytesize;
	qat_req->pke_hdr.cd_pars.func_id = qat_function.func_id;
	qat_req->input_param_count = 3;
	qat_req->output_param_count = 2;

	HEXDUMP("SM2 K test", asym_op->sm2.k.data,
		cookie->alg_bytesize);
	HEXDUMP("SM2 K", cookie->input_array[0],
		cookie->alg_bytesize);
	HEXDUMP("SM2 msg", cookie->input_array[1],
		cookie->alg_bytesize);
	HEXDUMP("SM2 pkey", cookie->input_array[2],
		cookie->alg_bytesize);

	return RTE_CRYPTO_OP_STATUS_SUCCESS;
}

static int
sm2_ecdsa_verify_set_input(struct icp_qat_fw_pke_request *qat_req,
		struct qat_asym_op_cookie *cookie,
		const struct rte_crypto_asym_op *asym_op,
		const struct rte_crypto_asym_xform *xform)
{
	const struct qat_asym_function qat_function =
		get_sm2_ecdsa_verify_function();
	const uint32_t qat_func_alignsize =
		qat_function.bytesize;

	SET_PKE_LN(asym_op->sm2.message, qat_func_alignsize, 0);
	SET_PKE_LN(asym_op->sm2.r, qat_func_alignsize, 1);
	SET_PKE_LN(asym_op->sm2.s, qat_func_alignsize, 2);
	SET_PKE_LN(xform->ec.q.x, qat_func_alignsize, 3);
	SET_PKE_LN(xform->ec.q.y, qat_func_alignsize, 4);

	cookie->alg_bytesize = qat_function.bytesize;
	cookie->qat_func_alignsize = qat_function.bytesize;
	qat_req->pke_hdr.cd_pars.func_id = qat_function.func_id;
	qat_req->input_param_count = 5;
	qat_req->output_param_count = 0;

	return RTE_CRYPTO_OP_STATUS_SUCCESS;
}

static uint8_t
sm2_ecdsa_sign_collect(struct rte_crypto_asym_op *asym_op,
		const struct qat_asym_op_cookie *cookie)
{
	uint32_t alg_bytesize = cookie->alg_bytesize;

	if (asym_op->sm2.op_type == RTE_CRYPTO_ASYM_OP_VERIFY)
		return RTE_CRYPTO_OP_STATUS_SUCCESS;

	rte_memcpy(asym_op->sm2.r.data, cookie->output_array[0], alg_bytesize);
	rte_memcpy(asym_op->sm2.s.data, cookie->output_array[1], alg_bytesize);
	asym_op->sm2.r.length = alg_bytesize;
	asym_op->sm2.s.length = alg_bytesize;

	HEXDUMP("SM2 R", cookie->output_array[0],
		alg_bytesize);
	HEXDUMP("SM2 S", cookie->output_array[1],
		alg_bytesize);
	return RTE_CRYPTO_OP_STATUS_SUCCESS;
}

static int
sm2_encryption_set_input(struct icp_qat_fw_pke_request *qat_req,
	struct qat_asym_op_cookie *cookie,
	const struct rte_crypto_asym_op *asym_op,
	const struct rte_crypto_asym_xform *xform)
{
	const struct qat_asym_function qat_function =
		get_sm2_encryption_function();
	const uint32_t qat_func_alignsize =
		qat_function.bytesize;

	SET_PKE_LN(asym_op->sm2.k, qat_func_alignsize, 0);
	SET_PKE_LN(xform->ec.q.x, qat_func_alignsize, 1);
	SET_PKE_LN(xform->ec.q.y, qat_func_alignsize, 2);

	cookie->alg_bytesize = qat_function.bytesize;
	cookie->qat_func_alignsize = qat_function.bytesize;
	qat_req->pke_hdr.cd_pars.func_id = qat_function.func_id;
	qat_req->input_param_count = 3;
	qat_req->output_param_count = 4;

	HEXDUMP("SM2 K", cookie->input_array[0],
		qat_func_alignsize);
	HEXDUMP("SM2 Q.x", cookie->input_array[1],
		qat_func_alignsize);
	HEXDUMP("SM2 Q.y", cookie->input_array[2],
		qat_func_alignsize);

	return RTE_CRYPTO_OP_STATUS_SUCCESS;
}

static uint8_t
sm2_encryption_collect(struct rte_crypto_asym_op *asym_op,
		const struct qat_asym_op_cookie *cookie)
{
	uint32_t alg_bytesize = cookie->alg_bytesize;

	rte_memcpy(asym_op->sm2.c1.x.data, cookie->output_array[0], alg_bytesize);
	rte_memcpy(asym_op->sm2.c1.y.data, cookie->output_array[1], alg_bytesize);
	rte_memcpy(asym_op->sm2.kp.x.data, cookie->output_array[2], alg_bytesize);
	rte_memcpy(asym_op->sm2.kp.y.data, cookie->output_array[3], alg_bytesize);
	asym_op->sm2.c1.x.length = alg_bytesize;
	asym_op->sm2.c1.y.length = alg_bytesize;
	asym_op->sm2.kp.x.length = alg_bytesize;
	asym_op->sm2.kp.y.length = alg_bytesize;

	HEXDUMP("c1[x1]", cookie->output_array[0],
		alg_bytesize);
	HEXDUMP("c1[y]", cookie->output_array[1],
		alg_bytesize);
	HEXDUMP("kp[x]", cookie->output_array[2],
		alg_bytesize);
	HEXDUMP("kp[y]", cookie->output_array[3],
		alg_bytesize);
	return RTE_CRYPTO_OP_STATUS_SUCCESS;
}


static int
sm2_decryption_set_input(struct icp_qat_fw_pke_request *qat_req,
	struct qat_asym_op_cookie *cookie,
	const struct rte_crypto_asym_op *asym_op,
	const struct rte_crypto_asym_xform *xform)
{
	const struct qat_asym_function qat_function =
		get_sm2_decryption_function();
	const uint32_t qat_func_alignsize =
		qat_function.bytesize;

	SET_PKE_LN(xform->ec.pkey, qat_func_alignsize, 0);
	SET_PKE_LN(asym_op->sm2.c1.x, qat_func_alignsize, 1);
	SET_PKE_LN(asym_op->sm2.c1.y, qat_func_alignsize, 2);

	cookie->alg_bytesize = qat_function.bytesize;
	cookie->qat_func_alignsize = qat_function.bytesize;
	qat_req->pke_hdr.cd_pars.func_id = qat_function.func_id;
	qat_req->input_param_count = 3;
	qat_req->output_param_count = 2;

	HEXDUMP("d", cookie->input_array[0],
		qat_func_alignsize);
	HEXDUMP("c1[x]", cookie->input_array[1],
		qat_func_alignsize);
	HEXDUMP("c1[y]", cookie->input_array[2],
		qat_func_alignsize);

	return RTE_CRYPTO_OP_STATUS_SUCCESS;
}


static uint8_t
sm2_decryption_collect(struct rte_crypto_asym_op *asym_op,
		const struct qat_asym_op_cookie *cookie)
{
	uint32_t alg_bytesize = cookie->alg_bytesize;

	rte_memcpy(asym_op->sm2.kp.x.data, cookie->output_array[0], alg_bytesize);
	rte_memcpy(asym_op->sm2.kp.y.data, cookie->output_array[1], alg_bytesize);
	asym_op->sm2.kp.x.length = alg_bytesize;
	asym_op->sm2.kp.y.length = alg_bytesize;

	HEXDUMP("kp[x]", cookie->output_array[0],
		alg_bytesize);
	HEXDUMP("kp[y]", cookie->output_array[1],
		alg_bytesize);
	return RTE_CRYPTO_OP_STATUS_SUCCESS;
}

static int
asym_set_input(struct icp_qat_fw_pke_request *qat_req,
		struct qat_asym_op_cookie *cookie,
		const struct rte_crypto_asym_op *asym_op,
		const struct rte_crypto_asym_xform *xform,
		uint8_t legacy_alg)
{
	switch (xform->xform_type) {
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		return modexp_set_input(qat_req, cookie, asym_op, xform);
	case RTE_CRYPTO_ASYM_XFORM_MODINV:
		return modinv_set_input(qat_req, cookie, asym_op, xform);
	case RTE_CRYPTO_ASYM_XFORM_RSA:{
		if (unlikely((xform->rsa.n.length < RSA_MODULUS_2048_BITS)
					&& (legacy_alg == 0)))
			return RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return rsa_set_input(qat_req, cookie, asym_op, xform);
	}
	case RTE_CRYPTO_ASYM_XFORM_ECDSA:
		return ecdsa_set_input(qat_req, cookie, asym_op, xform);
	case RTE_CRYPTO_ASYM_XFORM_ECPM:
		return ecpm_set_input(qat_req, cookie, asym_op, xform);
	case RTE_CRYPTO_ASYM_XFORM_ECDH:
		if (asym_op->ecdh.ke_type ==
			RTE_CRYPTO_ASYM_KE_PUB_KEY_VERIFY) {
			return ecdh_verify_set_input(qat_req, cookie,
				asym_op, xform);
		} else {
			return ecdh_set_input(qat_req, cookie,
				asym_op, xform);
		}
	case RTE_CRYPTO_ASYM_XFORM_SM2:
		if (asym_op->sm2.op_type == RTE_CRYPTO_ASYM_OP_ENCRYPT) {
			return sm2_encryption_set_input(qat_req, cookie,
				asym_op, xform);
		} else if (asym_op->sm2.op_type == RTE_CRYPTO_ASYM_OP_DECRYPT) {
			return sm2_decryption_set_input(qat_req, cookie,
				asym_op, xform);
		} else if (asym_op->sm2.op_type == RTE_CRYPTO_ASYM_OP_VERIFY) {
			return sm2_ecdsa_verify_set_input(qat_req, cookie,
						asym_op, xform);
		} else if (asym_op->sm2.op_type == RTE_CRYPTO_ASYM_OP_SIGN) {
			return sm2_ecdsa_sign_set_input(qat_req, cookie,
					asym_op, xform);
		}
		break;
	default:
		QAT_LOG(ERR, "Invalid/unsupported asymmetric crypto xform");
		return -EINVAL;
	}
	return 1;
}

static int
qat_asym_build_request(void *in_op,
	uint8_t *out_msg,
	void *op_cookie,
	struct qat_qp *qp)
{
	struct rte_crypto_op *op = (struct rte_crypto_op *)in_op;
	struct rte_crypto_asym_op *asym_op = op->asym;
	struct icp_qat_fw_pke_request *qat_req =
			(struct icp_qat_fw_pke_request *)out_msg;
	struct qat_asym_op_cookie *cookie =
			(struct qat_asym_op_cookie *)op_cookie;
	struct rte_crypto_asym_xform *xform;
	struct qat_asym_session *qat_session = (struct qat_asym_session *)
			op->asym->session->sess_private_data;
	int err = 0;

	op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
	switch (op->sess_type) {
	case RTE_CRYPTO_OP_WITH_SESSION:
		request_init(qat_req);
		if (unlikely(qat_session == NULL)) {
			QAT_DP_LOG(ERR,
				"Session was not created for this device");
			op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
			goto error;
		}
		xform = &qat_session->xform;
		break;
	case RTE_CRYPTO_OP_SESSIONLESS:
		request_init(qat_req);
		xform = op->asym->xform;
		break;
	default:
		QAT_DP_LOG(ERR, "Invalid session/xform settings");
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
		goto error;
	}
	err = asym_set_input(qat_req, cookie, asym_op, xform,
		qp->qat_dev->options.legacy_alg);
	if (err) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
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
qat_asym_collect_response(struct rte_crypto_op *op,
		struct qat_asym_op_cookie *cookie,
		struct rte_crypto_asym_xform *xform)
{
	struct rte_crypto_asym_op *asym_op = op->asym;

	switch (xform->xform_type) {
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		return modexp_collect(asym_op, cookie, xform);
	case RTE_CRYPTO_ASYM_XFORM_MODINV:
		return modinv_collect(asym_op, cookie, xform);
	case RTE_CRYPTO_ASYM_XFORM_RSA:
		return rsa_collect(asym_op, cookie, xform);
	case RTE_CRYPTO_ASYM_XFORM_ECDSA:
		return ecdsa_collect(asym_op, cookie);
	case RTE_CRYPTO_ASYM_XFORM_ECPM:
		return ecpm_collect(asym_op, cookie);
	case RTE_CRYPTO_ASYM_XFORM_ECDH:
		return ecdh_collect(asym_op, cookie);
	case RTE_CRYPTO_ASYM_XFORM_SM2:
		if (asym_op->sm2.op_type == RTE_CRYPTO_ASYM_OP_ENCRYPT)
			return sm2_encryption_collect(asym_op, cookie);
		else if (asym_op->sm2.op_type == RTE_CRYPTO_ASYM_OP_DECRYPT)
			return sm2_decryption_collect(asym_op, cookie);
		else
			return sm2_ecdsa_sign_collect(asym_op, cookie);

	default:
		QAT_LOG(ERR, "Not supported xform type");
		return  RTE_CRYPTO_OP_STATUS_ERROR;
	}
}

static int
qat_asym_process_response(void **out_op, uint8_t *resp,
		void *op_cookie, __rte_unused uint64_t *dequeue_err_count)
{
	struct icp_qat_fw_pke_resp *resp_msg =
			(struct icp_qat_fw_pke_resp *)resp;
	struct rte_crypto_op *op = (struct rte_crypto_op *)(uintptr_t)
			(resp_msg->opaque);
	struct qat_asym_op_cookie *cookie = op_cookie;
	struct rte_crypto_asym_xform *xform = NULL;
	struct qat_asym_session *qat_session = (struct qat_asym_session *)
			op->asym->session->sess_private_data;

	*out_op = op;
	if (cookie->error) {
		cookie->error = 0;
		if (op->status == RTE_CRYPTO_OP_STATUS_NOT_PROCESSED)
			op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		QAT_DP_LOG(DEBUG, "Cookie status returned error");
	} else {
		if (ICP_QAT_FW_PKE_RESP_PKE_STAT_GET(
			resp_msg->pke_resp_hdr.resp_status.pke_resp_flags)) {
			if (op->status == RTE_CRYPTO_OP_STATUS_NOT_PROCESSED)
				op->status = RTE_CRYPTO_OP_STATUS_ERROR;
			QAT_DP_LOG(DEBUG, "Asymmetric response status"
					" returned error");
		}
		if (resp_msg->pke_resp_hdr.resp_status.comn_err_code) {
			if (op->status == RTE_CRYPTO_OP_STATUS_NOT_PROCESSED)
				op->status = RTE_CRYPTO_OP_STATUS_ERROR;
			QAT_DP_LOG(ERR, "Asymmetric common status"
					" returned error");
		}
	}

	switch (op->sess_type) {
	case RTE_CRYPTO_OP_WITH_SESSION:
		xform = &qat_session->xform;
		break;
	case RTE_CRYPTO_OP_SESSIONLESS:
		xform = op->asym->xform;
		break;
	default:
		QAT_DP_LOG(ERR,
			"Invalid session/xform settings in response ring!");
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
	}
	if (op->status == RTE_CRYPTO_OP_STATUS_NOT_PROCESSED)
		op->status = qat_asym_collect_response(op, cookie, xform);
	HEXDUMP("resp_msg:", resp_msg, sizeof(struct icp_qat_fw_pke_resp));
	if (likely(xform != NULL))
		cleanup(cookie, xform);

	return 1;
}

static int
session_set_modexp(struct qat_asym_session *qat_session,
			struct rte_crypto_asym_xform *xform)
{
	uint8_t *modulus = xform->modex.modulus.data;
	uint8_t *exponent = xform->modex.exponent.data;

	qat_session->xform.modex.modulus.data =
		rte_malloc(NULL, xform->modex.modulus.length, 0);
	if (qat_session->xform.modex.modulus.data == NULL)
		return -ENOMEM;
	qat_session->xform.modex.modulus.length = xform->modex.modulus.length;
	qat_session->xform.modex.exponent.data = rte_malloc(NULL,
				xform->modex.exponent.length, 0);
	if (qat_session->xform.modex.exponent.data == NULL) {
		rte_free(qat_session->xform.modex.exponent.data);
		return -ENOMEM;
	}
	qat_session->xform.modex.exponent.length = xform->modex.exponent.length;

	rte_memcpy(qat_session->xform.modex.modulus.data, modulus,
			xform->modex.modulus.length);
	rte_memcpy(qat_session->xform.modex.exponent.data, exponent,
			xform->modex.exponent.length);

	return 0;
}

static int
session_set_modinv(struct qat_asym_session *qat_session,
			struct rte_crypto_asym_xform *xform)
{
	uint8_t *modulus = xform->modinv.modulus.data;

	qat_session->xform.modinv.modulus.data =
		rte_malloc(NULL, xform->modinv.modulus.length, 0);
	if (qat_session->xform.modinv.modulus.data == NULL)
		return -ENOMEM;
	qat_session->xform.modinv.modulus.length = xform->modinv.modulus.length;

	rte_memcpy(qat_session->xform.modinv.modulus.data, modulus,
			xform->modinv.modulus.length);

	return 0;
}

static int
session_set_rsa(struct qat_asym_session *qat_session,
			struct rte_crypto_asym_xform *xform)
{
	uint8_t *n = xform->rsa.n.data;
	uint8_t *e = xform->rsa.e.data;
	int ret = 0;

	qat_session->xform.rsa.key_type = xform->rsa.key_type;

	qat_session->xform.rsa.n.data =
		rte_malloc(NULL, xform->rsa.n.length, 0);
	if (qat_session->xform.rsa.n.data == NULL)
		return -ENOMEM;
	qat_session->xform.rsa.n.length =
		xform->rsa.n.length;

	qat_session->xform.rsa.e.data =
		rte_malloc(NULL, xform->rsa.e.length, 0);
	if (qat_session->xform.rsa.e.data == NULL) {
		ret = -ENOMEM;
		goto err;
	}
	qat_session->xform.rsa.e.length =
		xform->rsa.e.length;

	if (xform->rsa.key_type == RTE_RSA_KEY_TYPE_QT) {
		uint8_t *p = xform->rsa.qt.p.data;
		uint8_t *q = xform->rsa.qt.q.data;
		uint8_t *dP = xform->rsa.qt.dP.data;
		uint8_t *dQ = xform->rsa.qt.dQ.data;
		uint8_t *qInv = xform->rsa.qt.qInv.data;

		qat_session->xform.rsa.qt.p.data =
			rte_malloc(NULL, xform->rsa.qt.p.length, 0);
		if (qat_session->xform.rsa.qt.p.data == NULL) {
			ret = -ENOMEM;
			goto err;
		}
		qat_session->xform.rsa.qt.p.length =
			xform->rsa.qt.p.length;

		qat_session->xform.rsa.qt.q.data =
			rte_malloc(NULL, xform->rsa.qt.q.length, 0);
		if (qat_session->xform.rsa.qt.q.data == NULL) {
			ret = -ENOMEM;
			goto err;
		}
		qat_session->xform.rsa.qt.q.length =
			xform->rsa.qt.q.length;

		qat_session->xform.rsa.qt.dP.data =
			rte_malloc(NULL, xform->rsa.qt.dP.length, 0);
		if (qat_session->xform.rsa.qt.dP.data == NULL) {
			ret = -ENOMEM;
			goto err;
		}
		qat_session->xform.rsa.qt.dP.length =
			xform->rsa.qt.dP.length;

		qat_session->xform.rsa.qt.dQ.data =
			rte_malloc(NULL, xform->rsa.qt.dQ.length, 0);
		if (qat_session->xform.rsa.qt.dQ.data == NULL) {
			ret = -ENOMEM;
			goto err;
		}
		qat_session->xform.rsa.qt.dQ.length =
			xform->rsa.qt.dQ.length;

		qat_session->xform.rsa.qt.qInv.data =
			rte_malloc(NULL, xform->rsa.qt.qInv.length, 0);
		if (qat_session->xform.rsa.qt.qInv.data == NULL) {
			ret = -ENOMEM;
			goto err;
		}
		qat_session->xform.rsa.qt.qInv.length =
			xform->rsa.qt.qInv.length;

		rte_memcpy(qat_session->xform.rsa.qt.p.data, p,
				xform->rsa.qt.p.length);
		rte_memcpy(qat_session->xform.rsa.qt.q.data, q,
				xform->rsa.qt.q.length);
		rte_memcpy(qat_session->xform.rsa.qt.dP.data, dP,
				xform->rsa.qt.dP.length);
		rte_memcpy(qat_session->xform.rsa.qt.dQ.data, dQ,
				xform->rsa.qt.dQ.length);
		rte_memcpy(qat_session->xform.rsa.qt.qInv.data, qInv,
				xform->rsa.qt.qInv.length);

	} else {
		uint8_t *d = xform->rsa.d.data;

		qat_session->xform.rsa.d.data =
			rte_malloc(NULL, xform->rsa.d.length, 0);
		if (qat_session->xform.rsa.d.data == NULL) {
			ret = -ENOMEM;
			goto err;
		}
		qat_session->xform.rsa.d.length =
			xform->rsa.d.length;
		rte_memcpy(qat_session->xform.rsa.d.data, d,
			xform->rsa.d.length);
	}

	rte_memcpy(qat_session->xform.rsa.n.data, n,
		xform->rsa.n.length);
	rte_memcpy(qat_session->xform.rsa.e.data, e,
		xform->rsa.e.length);

	return 0;

err:
	rte_free(qat_session->xform.rsa.n.data);
	rte_free(qat_session->xform.rsa.e.data);
	rte_free(qat_session->xform.rsa.d.data);
	rte_free(qat_session->xform.rsa.qt.p.data);
	rte_free(qat_session->xform.rsa.qt.q.data);
	rte_free(qat_session->xform.rsa.qt.dP.data);
	rte_free(qat_session->xform.rsa.qt.dQ.data);
	rte_free(qat_session->xform.rsa.qt.qInv.data);
	return ret;
}

static int
session_set_ec(struct qat_asym_session *qat_session,
			struct rte_crypto_asym_xform *xform)
{
	uint8_t *pkey = xform->ec.pkey.data;
	uint8_t *q_x = xform->ec.q.x.data;
	uint8_t *q_y = xform->ec.q.y.data;

	qat_session->xform.ec.pkey.data =
		rte_malloc(NULL, xform->ec.pkey.length, 0);
	if (qat_session->xform.ec.pkey.length &&
		qat_session->xform.ec.pkey.data == NULL)
		return -ENOMEM;
	qat_session->xform.ec.q.x.data = rte_malloc(NULL,
		xform->ec.q.x.length, 0);
	if (qat_session->xform.ec.q.x.length &&
		qat_session->xform.ec.q.x.data == NULL) {
		rte_free(qat_session->xform.ec.pkey.data);
		return -ENOMEM;
	}
	qat_session->xform.ec.q.y.data = rte_malloc(NULL,
		xform->ec.q.y.length, 0);
	if (qat_session->xform.ec.q.y.length &&
		qat_session->xform.ec.q.y.data == NULL) {
		rte_free(qat_session->xform.ec.pkey.data);
		rte_free(qat_session->xform.ec.q.x.data);
		return -ENOMEM;
	}

	memcpy(qat_session->xform.ec.pkey.data, pkey,
		xform->ec.pkey.length);
	qat_session->xform.ec.pkey.length = xform->ec.pkey.length;
	memcpy(qat_session->xform.ec.q.x.data, q_x,
		xform->ec.q.x.length);
	qat_session->xform.ec.q.x.length = xform->ec.q.x.length;
	memcpy(qat_session->xform.ec.q.y.data, q_y,
		xform->ec.q.y.length);
	qat_session->xform.ec.q.y.length = xform->ec.q.y.length;
	qat_session->xform.ec.curve_id = xform->ec.curve_id;

	return 0;

}

int
qat_asym_session_configure(struct rte_cryptodev *dev __rte_unused,
		struct rte_crypto_asym_xform *xform,
		struct rte_cryptodev_asym_session *session)
{
	struct qat_cryptodev_private *crypto_qat;
	struct qat_asym_session *qat_session;
	int ret = 0;

	crypto_qat = dev->data->dev_private;
	qat_session = (struct qat_asym_session *) session->sess_private_data;
	memset(qat_session, 0, sizeof(*qat_session));

	qat_session->xform.xform_type = xform->xform_type;
	switch (xform->xform_type) {
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		ret = session_set_modexp(qat_session, xform);
		break;
	case RTE_CRYPTO_ASYM_XFORM_MODINV:
		ret = session_set_modinv(qat_session, xform);
		break;
	case RTE_CRYPTO_ASYM_XFORM_RSA: {
		if (unlikely((xform->rsa.n.length < RSA_MODULUS_2048_BITS)
				&& (crypto_qat->qat_dev->options.legacy_alg == 0))) {
			ret = -ENOTSUP;
			return ret;
		}
		ret = session_set_rsa(qat_session, xform);
		}
		break;
	case RTE_CRYPTO_ASYM_XFORM_ECDSA:
	case RTE_CRYPTO_ASYM_XFORM_ECPM:
	case RTE_CRYPTO_ASYM_XFORM_ECDH:
	case RTE_CRYPTO_ASYM_XFORM_SM2:
		ret = session_set_ec(qat_session, xform);
		break;
	default:
		ret = -ENOTSUP;
	}

	if (ret) {
		QAT_LOG(ERR, "Unsupported xform type");
		return ret;
	}

	return 0;
}

unsigned int
qat_asym_session_get_private_size(struct rte_cryptodev *dev __rte_unused)
{
	return RTE_ALIGN_CEIL(sizeof(struct qat_asym_session), 8);
}

static void
session_clear_modexp(struct rte_crypto_modex_xform *modex)
{
	PARAM_CLR(modex->modulus);
	PARAM_CLR(modex->exponent);
}

static void
session_clear_modinv(struct rte_crypto_modinv_xform *modinv)
{
	PARAM_CLR(modinv->modulus);
}

static void
session_clear_rsa(struct rte_crypto_rsa_xform *rsa)
{
	PARAM_CLR(rsa->n);
	PARAM_CLR(rsa->e);
	if (rsa->key_type == RTE_RSA_KEY_TYPE_EXP) {
		PARAM_CLR(rsa->d);
	} else {
		PARAM_CLR(rsa->qt.p);
		PARAM_CLR(rsa->qt.q);
		PARAM_CLR(rsa->qt.dP);
		PARAM_CLR(rsa->qt.dQ);
		PARAM_CLR(rsa->qt.qInv);
	}
}

static void
session_clear_xform(struct qat_asym_session *qat_session)
{
	switch (qat_session->xform.xform_type) {
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		session_clear_modexp(&qat_session->xform.modex);
		break;
	case RTE_CRYPTO_ASYM_XFORM_MODINV:
		session_clear_modinv(&qat_session->xform.modinv);
		break;
	case RTE_CRYPTO_ASYM_XFORM_RSA:
		session_clear_rsa(&qat_session->xform.rsa);
		break;
	default:
		break;
	}
}

void
qat_asym_session_clear(struct rte_cryptodev *dev,
		struct rte_cryptodev_asym_session *session)
{
	void *sess_priv = session->sess_private_data;
	struct qat_asym_session *qat_session =
		(struct qat_asym_session *)sess_priv;

	if (sess_priv) {
		session_clear_xform(qat_session);
		memset(qat_session, 0, qat_asym_session_get_private_size(dev));
	}
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

static int
qat_asym_dev_create(struct qat_pci_device *qat_pci_dev)
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
	const struct qat_crypto_gen_dev_ops *gen_dev_ops =
		&qat_asym_gen_dev_ops[qat_pci_dev->qat_dev_gen];
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	char capa_memz_name[RTE_CRYPTODEV_NAME_MAX_LEN];
	uint16_t sub_id = qat_dev_instance->pci_dev->id.subsystem_device_id;
	char *cmdline = NULL;

	snprintf(name, RTE_CRYPTODEV_NAME_MAX_LEN, "%s_%s",
			qat_pci_dev->name, "asym");
	QAT_LOG(DEBUG, "Creating QAT ASYM device %s", name);

	if (qat_pci_dev->qat_dev_gen == QAT_VQAT &&
		sub_id != ADF_VQAT_ASYM_PCI_SUBSYSTEM_ID) {
		QAT_LOG(ERR, "Device (vqat instance) %s does not support asymmetric crypto",
				name);
		return -EFAULT;
	}
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

	cmdline = qat_dev_cmdline_get_val(qat_pci_dev,
			ASYM_ENQ_THRESHOLD_NAME);
	if (cmdline) {
		internals->min_enq_burst_threshold =
			atoi(cmdline) > MAX_QP_THRESHOLD_SIZE ?
			MAX_QP_THRESHOLD_SIZE :
			atoi(cmdline);
	}

	if (qat_pci_dev->options.slice_map & ICP_ACCEL_MASK_PKE_SLICE) {
		QAT_LOG(ERR, "Device %s does not support PKE slice",
				name);
		rte_cryptodev_pmd_destroy(cryptodev);
		memset(&qat_dev_instance->asym_rte_dev, 0,
			sizeof(qat_dev_instance->asym_rte_dev));
		return -1;
	}

	if (gen_dev_ops->get_capabilities(internals,
			capa_memz_name, qat_pci_dev->options.slice_map) < 0) {
		QAT_LOG(ERR,
			"Device cannot obtain capabilities, destroying PMD for %s",
			name);
		rte_cryptodev_pmd_destroy(cryptodev);
		memset(&qat_dev_instance->asym_rte_dev, 0,
			sizeof(qat_dev_instance->asym_rte_dev));
		return -1;
	}

	qat_pci_dev->pmd[QAT_SERVICE_ASYMMETRIC] = internals;
	internals->service_type = QAT_SERVICE_ASYMMETRIC;
	QAT_LOG(DEBUG, "Created QAT ASYM device %s as cryptodev instance %d",
			cryptodev->data->name, internals->dev_id);
	return 0;
}

static int
qat_asym_dev_destroy(struct qat_pci_device *qat_pci_dev)
{
	struct rte_cryptodev *cryptodev;
	struct qat_cryptodev_private *dev;

	if (qat_pci_dev == NULL)
		return -ENODEV;
	dev = qat_pci_dev->pmd[QAT_SERVICE_ASYMMETRIC];
	if (dev == NULL)
		return 0;
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_memzone_free(dev->capa_mz);
	/* free crypto device */
	cryptodev = rte_cryptodev_pmd_get_dev(dev->dev_id);
	rte_cryptodev_pmd_destroy(cryptodev);
	qat_pci_devs[qat_pci_dev->qat_dev_id].asym_rte_dev.name = NULL;
	qat_pci_dev->pmd[QAT_SERVICE_ASYMMETRIC] = NULL;

	return 0;
}

static struct cryptodev_driver qat_crypto_drv;
RTE_PMD_REGISTER_CRYPTO_DRIVER(qat_crypto_drv,
		cryptodev_qat_asym_driver,
		qat_asym_driver_id);

RTE_INIT(qat_asym_init)
{
	qat_cmdline_defines[QAT_SERVICE_ASYMMETRIC] = arguments;
	qat_service[QAT_SERVICE_ASYMMETRIC].name = "asymmetric crypto";
	qat_service[QAT_SERVICE_ASYMMETRIC].dev_create = qat_asym_dev_create;
	qat_service[QAT_SERVICE_ASYMMETRIC].dev_destroy = qat_asym_dev_destroy;
}
