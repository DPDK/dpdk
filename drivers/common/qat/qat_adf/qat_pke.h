/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2022 Intel Corporation
 */

#ifndef _QAT_PKE_FUNCTIONALITY_ARRAYS_H_
#define _QAT_PKE_FUNCTIONALITY_ARRAYS_H_

#include "icp_qat_fw_mmp_ids.h"

/*
 * Modular exponentiation functionality IDs
 */

struct qat_asym_function {
	uint32_t func_id;
	uint32_t bytesize;
};

static struct qat_asym_function
get_modexp_function2(uint32_t bytesize)
{
	struct qat_asym_function qat_function = { };

	if (bytesize <= 64) {
		qat_function.func_id = MATHS_MODEXP_L512;
		qat_function.bytesize = 64;
	} else if (bytesize <= 128) {
		qat_function.func_id = MATHS_MODEXP_L1024;
		qat_function.bytesize = 128;
	} else if (bytesize <= 192) {
		qat_function.func_id = MATHS_MODEXP_L1536;
		qat_function.bytesize = 192;
	} else if (bytesize <= 256) {
		qat_function.func_id = MATHS_MODEXP_L2048;
		qat_function.bytesize = 256;
	} else if (bytesize <= 320) {
		qat_function.func_id = MATHS_MODEXP_L2560;
		qat_function.bytesize = 320;
	} else if (bytesize <= 384) {
		qat_function.func_id = MATHS_MODEXP_L3072;
		qat_function.bytesize = 384;
	} else if (bytesize <= 448) {
		qat_function.func_id = MATHS_MODEXP_L3584;
		qat_function.bytesize = 448;
	} else if (bytesize <= 512) {
		qat_function.func_id = MATHS_MODEXP_L4096;
		qat_function.bytesize = 512;
	}
	return qat_function;
}

static struct qat_asym_function
get_modexp_function(struct rte_crypto_asym_xform *xform)
{
	return get_modexp_function2(xform->modex.modulus.length);
}

static struct qat_asym_function
get_modinv_function(struct rte_crypto_asym_xform *xform)
{
	struct qat_asym_function qat_function = { };

	if (xform->modinv.modulus.data[
		xform->modinv.modulus.length - 1] & 0x01) {
		if (xform->modex.modulus.length <= 16) {
			qat_function.func_id = MATHS_MODINV_ODD_L128;
			qat_function.bytesize = 16;
		} else if (xform->modex.modulus.length <= 24) {
			qat_function.func_id = MATHS_MODINV_ODD_L192;
			qat_function.bytesize = 24;
		} else if (xform->modex.modulus.length <= 32) {
			qat_function.func_id = MATHS_MODINV_ODD_L256;
			qat_function.bytesize = 32;
		} else if (xform->modex.modulus.length <= 48) {
			qat_function.func_id = MATHS_MODINV_ODD_L384;
			qat_function.bytesize = 48;
		} else if (xform->modex.modulus.length <= 64) {
			qat_function.func_id = MATHS_MODINV_ODD_L512;
			qat_function.bytesize = 64;
		} else if (xform->modex.modulus.length <= 96) {
			qat_function.func_id = MATHS_MODINV_ODD_L768;
			qat_function.bytesize = 96;
		} else if (xform->modex.modulus.length <= 128) {
			qat_function.func_id = MATHS_MODINV_ODD_L1024;
			qat_function.bytesize = 128;
		} else if (xform->modex.modulus.length <= 192) {
			qat_function.func_id = MATHS_MODINV_ODD_L1536;
			qat_function.bytesize = 192;
		} else if (xform->modex.modulus.length <= 256) {
			qat_function.func_id = MATHS_MODINV_ODD_L2048;
			qat_function.bytesize = 256;
		} else if (xform->modex.modulus.length <= 384) {
			qat_function.func_id = MATHS_MODINV_ODD_L3072;
			qat_function.bytesize = 384;
		} else if (xform->modex.modulus.length <= 512) {
			qat_function.func_id = MATHS_MODINV_ODD_L4096;
			qat_function.bytesize = 512;
		}
	} else {
		if (xform->modex.modulus.length <= 16) {
			qat_function.func_id = MATHS_MODINV_EVEN_L128;
			qat_function.bytesize = 16;
		} else if (xform->modex.modulus.length <= 24) {
			qat_function.func_id = MATHS_MODINV_EVEN_L192;
			qat_function.bytesize = 24;
		} else if (xform->modex.modulus.length <= 32) {
			qat_function.func_id = MATHS_MODINV_EVEN_L256;
			qat_function.bytesize = 32;
		} else if (xform->modex.modulus.length <= 48) {
			qat_function.func_id = MATHS_MODINV_EVEN_L384;
			qat_function.bytesize = 48;
		} else if (xform->modex.modulus.length <= 64) {
			qat_function.func_id = MATHS_MODINV_EVEN_L512;
			qat_function.bytesize = 64;
		} else if (xform->modex.modulus.length <= 96) {
			qat_function.func_id = MATHS_MODINV_EVEN_L768;
			qat_function.bytesize = 96;
		} else if (xform->modex.modulus.length <= 128) {
			qat_function.func_id = MATHS_MODINV_EVEN_L1024;
			qat_function.bytesize = 128;
		} else if (xform->modex.modulus.length <= 192) {
			qat_function.func_id = MATHS_MODINV_EVEN_L1536;
			qat_function.bytesize = 192;
		} else if (xform->modex.modulus.length <= 256) {
			qat_function.func_id = MATHS_MODINV_EVEN_L2048;
			qat_function.bytesize = 256;
		} else if (xform->modex.modulus.length <= 384) {
			qat_function.func_id = MATHS_MODINV_EVEN_L3072;
			qat_function.bytesize = 384;
		} else if (xform->modex.modulus.length <= 512) {
			qat_function.func_id = MATHS_MODINV_EVEN_L4096;
			qat_function.bytesize = 512;
		}
	}

	return qat_function;
}

static struct qat_asym_function
get_rsa_enc_function(struct rte_crypto_asym_xform *xform)
{
	struct qat_asym_function qat_function = { };

	if (xform->rsa.n.length <= 64) {
		qat_function.func_id = PKE_RSA_EP_512;
		qat_function.bytesize = 64;
	} else if (xform->rsa.n.length <= 128) {
		qat_function.func_id = PKE_RSA_EP_1024;
		qat_function.bytesize = 128;
	} else if (xform->rsa.n.length <= 192) {
		qat_function.func_id = PKE_RSA_EP_1536;
		qat_function.bytesize = 192;
	} else if (xform->rsa.n.length <= 256) {
		qat_function.func_id = PKE_RSA_EP_2048;
		qat_function.bytesize = 256;
	} else if (xform->rsa.n.length <= 384) {
		qat_function.func_id = PKE_RSA_EP_3072;
		qat_function.bytesize = 384;
	} else if (xform->rsa.n.length <= 512) {
		qat_function.func_id = PKE_RSA_EP_4096;
		qat_function.bytesize = 512;
	}
	return qat_function;
}

static struct qat_asym_function
get_rsa_dec_function(struct rte_crypto_asym_xform *xform)
{
	struct qat_asym_function qat_function = { };

	if (xform->rsa.n.length <= 64) {
		qat_function.func_id = PKE_RSA_DP1_512;
		qat_function.bytesize = 64;
	} else if (xform->rsa.n.length <= 128) {
		qat_function.func_id = PKE_RSA_DP1_1024;
		qat_function.bytesize = 128;
	} else if (xform->rsa.n.length <= 192) {
		qat_function.func_id = PKE_RSA_DP1_1536;
		qat_function.bytesize = 192;
	} else if (xform->rsa.n.length <= 256) {
		qat_function.func_id = PKE_RSA_DP1_2048;
		qat_function.bytesize = 256;
	} else if (xform->rsa.n.length <= 384) {
		qat_function.func_id = PKE_RSA_DP1_3072;
		qat_function.bytesize = 384;
	} else if (xform->rsa.n.length <= 512) {
		qat_function.func_id = PKE_RSA_DP1_4096;
		qat_function.bytesize = 512;
	}
	return qat_function;
}

static struct qat_asym_function
get_rsa_crt_function(struct rte_crypto_asym_xform *xform)
{
	struct qat_asym_function qat_function = { };
	int nlen = xform->rsa.qt.p.length * 2;

	if (nlen <= 64) {
		qat_function.func_id = PKE_RSA_DP2_512;
		qat_function.bytesize = 64;
	} else if (nlen <= 128) {
		qat_function.func_id = PKE_RSA_DP2_1024;
		qat_function.bytesize = 128;
	} else if (nlen <= 192) {
		qat_function.func_id = PKE_RSA_DP2_1536;
		qat_function.bytesize = 192;
	} else if (nlen <= 256) {
		qat_function.func_id = PKE_RSA_DP2_2048;
		qat_function.bytesize = 256;
	} else if (nlen <= 384) {
		qat_function.func_id = PKE_RSA_DP2_3072;
		qat_function.bytesize = 384;
	} else if (nlen <= 512) {
		qat_function.func_id = PKE_RSA_DP2_4096;
		qat_function.bytesize = 512;
	}
	return qat_function;
}

static struct qat_asym_function
get_ecdsa_verify_function(struct rte_crypto_asym_xform *xform)
{
	struct qat_asym_function qat_function;

	switch (xform->ec.curve_id) {
	case RTE_CRYPTO_EC_GROUP_SECP256R1:
		qat_function.func_id = PKE_ECDSA_VERIFY_GFP_L256;
		qat_function.bytesize = 32;
		break;
	case RTE_CRYPTO_EC_GROUP_SECP521R1:
		qat_function.func_id = PKE_ECDSA_VERIFY_GFP_521;
		qat_function.bytesize = 66;
		break;
	default:
		qat_function.func_id = 0;
	}
	return qat_function;
}

static struct qat_asym_function
get_ecdsa_function(struct rte_crypto_asym_xform *xform)
{
	struct qat_asym_function qat_function;

	switch (xform->ec.curve_id) {
	case RTE_CRYPTO_EC_GROUP_SECP256R1:
		qat_function.func_id = PKE_ECDSA_SIGN_RS_GFP_L256;
		qat_function.bytesize = 32;
		break;
	case RTE_CRYPTO_EC_GROUP_SECP521R1:
		qat_function.func_id = PKE_ECDSA_SIGN_RS_GFP_521;
		qat_function.bytesize = 66;
		break;
	default:
		qat_function.func_id = 0;
	}
	return qat_function;
}

static struct qat_asym_function
get_ecpm_function(struct rte_crypto_asym_xform *xform)
{
	struct qat_asym_function qat_function;

	switch (xform->ec.curve_id) {
	case RTE_CRYPTO_EC_GROUP_SECP256R1:
		qat_function.func_id = MATHS_POINT_MULTIPLICATION_GFP_L256;
		qat_function.bytesize = 32;
		break;
	case RTE_CRYPTO_EC_GROUP_SECP521R1:
		qat_function.func_id = MATHS_POINT_MULTIPLICATION_GFP_521;
		qat_function.bytesize = 66;
		break;
	default:
		qat_function.func_id = 0;
	}
	return qat_function;
}

#endif
