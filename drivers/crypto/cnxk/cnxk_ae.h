/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CNXK_AE_H_
#define _CNXK_AE_H_

#include <rte_common.h>
#include <rte_crypto_asym.h>
#include <rte_malloc.h>

#include "roc_api.h"
#include "cnxk_cryptodev_ops.h"

struct cnxk_ae_sess {
	enum rte_crypto_asym_xform_type xfrm_type;
	union {
		struct rte_crypto_rsa_xform rsa_ctx;
		struct rte_crypto_modex_xform mod_ctx;
		struct roc_ae_ec_ctx ec_ctx;
	};
	uint64_t *cnxk_fpm_iova;
	struct roc_ae_ec_group **ec_grp;
	uint64_t cpt_inst_w7;
};

static __rte_always_inline void
cnxk_ae_modex_param_normalize(uint8_t **data, size_t *len)
{
	size_t i;

	/* Strip leading NUL bytes */
	for (i = 0; i < *len; i++) {
		if ((*data)[i] != 0)
			break;
	}
	*data += i;
	*len -= i;
}

static __rte_always_inline int
cnxk_ae_fill_modex_params(struct cnxk_ae_sess *sess,
			  struct rte_crypto_asym_xform *xform)
{
	struct rte_crypto_modex_xform *ctx = &sess->mod_ctx;
	size_t exp_len = xform->modex.exponent.length;
	size_t mod_len = xform->modex.modulus.length;
	uint8_t *exp = xform->modex.exponent.data;
	uint8_t *mod = xform->modex.modulus.data;

	cnxk_ae_modex_param_normalize(&mod, &mod_len);
	cnxk_ae_modex_param_normalize(&exp, &exp_len);

	if (unlikely(exp_len == 0 || mod_len == 0))
		return -EINVAL;

	if (unlikely(exp_len > mod_len))
		return -ENOTSUP;

	/* Allocate buffer to hold modexp params */
	ctx->modulus.data = rte_malloc(NULL, mod_len + exp_len, 0);
	if (ctx->modulus.data == NULL)
		return -ENOMEM;

	/* Set up modexp prime modulus and private exponent */
	memcpy(ctx->modulus.data, mod, mod_len);
	ctx->exponent.data = ctx->modulus.data + mod_len;
	memcpy(ctx->exponent.data, exp, exp_len);

	ctx->modulus.length = mod_len;
	ctx->exponent.length = exp_len;

	return 0;
}

static __rte_always_inline int
cnxk_ae_fill_rsa_params(struct cnxk_ae_sess *sess,
			struct rte_crypto_asym_xform *xform)
{
	struct rte_crypto_rsa_priv_key_qt qt = xform->rsa.qt;
	struct rte_crypto_rsa_xform *xfrm_rsa = &xform->rsa;
	struct rte_crypto_rsa_xform *rsa = &sess->rsa_ctx;
	size_t mod_len = xfrm_rsa->n.length;
	size_t exp_len = xfrm_rsa->e.length;
	size_t len = (mod_len / 2);
	uint64_t total_size;

	/* Make sure key length used is not more than mod_len/2 */
	if (qt.p.data != NULL)
		len = RTE_MIN(len, qt.p.length);

	/* Total size required for RSA key params(n,e,(q,dQ,p,dP,qInv)) */
	total_size = mod_len + exp_len + 5 * len;

	/* Allocate buffer to hold all RSA keys */
	rsa->n.data = rte_malloc(NULL, total_size, 0);
	if (rsa->n.data == NULL)
		return -ENOMEM;

	/* Set up RSA prime modulus and public key exponent */
	memcpy(rsa->n.data, xfrm_rsa->n.data, mod_len);
	rsa->e.data = rsa->n.data + mod_len;
	memcpy(rsa->e.data, xfrm_rsa->e.data, exp_len);

	/* Private key in quintuple format */
	if (len != 0) {
		rsa->qt.q.data = rsa->e.data + exp_len;
		memcpy(rsa->qt.q.data, qt.q.data, qt.q.length);
		rsa->qt.dQ.data = rsa->qt.q.data + qt.q.length;
		memcpy(rsa->qt.dQ.data, qt.dQ.data, qt.dQ.length);
		rsa->qt.p.data = rsa->qt.dQ.data + qt.dQ.length;
		memcpy(rsa->qt.p.data, qt.p.data, qt.p.length);
		rsa->qt.dP.data = rsa->qt.p.data + qt.p.length;
		memcpy(rsa->qt.dP.data, qt.dP.data, qt.dP.length);
		rsa->qt.qInv.data = rsa->qt.dP.data + qt.dP.length;
		memcpy(rsa->qt.qInv.data, qt.qInv.data, qt.qInv.length);

		rsa->qt.q.length = qt.q.length;
		rsa->qt.dQ.length = qt.dQ.length;
		rsa->qt.p.length = qt.p.length;
		rsa->qt.dP.length = qt.dP.length;
		rsa->qt.qInv.length = qt.qInv.length;
	}
	rsa->n.length = mod_len;
	rsa->e.length = exp_len;

	return 0;
}

static __rte_always_inline int
cnxk_ae_fill_ec_params(struct cnxk_ae_sess *sess,
		       struct rte_crypto_asym_xform *xform)
{
	struct roc_ae_ec_ctx *ec = &sess->ec_ctx;

	switch (xform->ec.curve_id) {
	case RTE_CRYPTO_EC_GROUP_SECP192R1:
		ec->curveid = ROC_AE_EC_ID_P192;
		break;
	case RTE_CRYPTO_EC_GROUP_SECP224R1:
		ec->curveid = ROC_AE_EC_ID_P224;
		break;
	case RTE_CRYPTO_EC_GROUP_SECP256R1:
		ec->curveid = ROC_AE_EC_ID_P256;
		break;
	case RTE_CRYPTO_EC_GROUP_SECP384R1:
		ec->curveid = ROC_AE_EC_ID_P384;
		break;
	case RTE_CRYPTO_EC_GROUP_SECP521R1:
		ec->curveid = ROC_AE_EC_ID_P521;
		break;
	default:
		/* Only NIST curves (FIPS 186-4) are supported */
		return -EINVAL;
	}

	return 0;
}

static __rte_always_inline int
cnxk_ae_fill_session_parameters(struct cnxk_ae_sess *sess,
				struct rte_crypto_asym_xform *xform)
{
	int ret;

	sess->xfrm_type = xform->xform_type;

	switch (xform->xform_type) {
	case RTE_CRYPTO_ASYM_XFORM_RSA:
		ret = cnxk_ae_fill_rsa_params(sess, xform);
		break;
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		ret = cnxk_ae_fill_modex_params(sess, xform);
		break;
	case RTE_CRYPTO_ASYM_XFORM_ECDSA:
		/* Fall through */
	case RTE_CRYPTO_ASYM_XFORM_ECPM:
		ret = cnxk_ae_fill_ec_params(sess, xform);
		break;
	default:
		return -ENOTSUP;
	}
	return ret;
}

static inline void
cnxk_ae_free_session_parameters(struct cnxk_ae_sess *sess)
{
	struct rte_crypto_modex_xform *mod;
	struct rte_crypto_rsa_xform *rsa;

	switch (sess->xfrm_type) {
	case RTE_CRYPTO_ASYM_XFORM_RSA:
		rsa = &sess->rsa_ctx;
		if (rsa->n.data)
			rte_free(rsa->n.data);
		break;
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		mod = &sess->mod_ctx;
		if (mod->modulus.data)
			rte_free(mod->modulus.data);
		break;
	case RTE_CRYPTO_ASYM_XFORM_ECDSA:
		/* Fall through */
	case RTE_CRYPTO_ASYM_XFORM_ECPM:
		break;
	default:
		break;
	}
}
#endif /* _CNXK_AE_H_ */
