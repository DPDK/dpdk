/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019 Marvell International Ltd.
 */

#ifndef _CPT_UCODE_ASYM_H_
#define _CPT_UCODE_ASYM_H_

#include <rte_common.h>
#include <rte_crypto_asym.h>
#include <rte_malloc.h>

#include "cpt_mcode_defines.h"

static __rte_always_inline void
cpt_modex_param_normalize(uint8_t **data, size_t *len)
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
cpt_fill_modex_params(struct cpt_asym_sess_misc *sess,
		      struct rte_crypto_asym_xform *xform)
{
	struct rte_crypto_modex_xform *ctx = &sess->mod_ctx;
	size_t exp_len = xform->modex.exponent.length;
	size_t mod_len = xform->modex.modulus.length;
	uint8_t *exp = xform->modex.exponent.data;
	uint8_t *mod = xform->modex.modulus.data;

	cpt_modex_param_normalize(&mod, &mod_len);
	cpt_modex_param_normalize(&exp, &exp_len);

	if (unlikely(exp_len == 0 || mod_len == 0))
		return -EINVAL;

	if (unlikely(exp_len > mod_len)) {
		CPT_LOG_DP_ERR("Exponent length greater than modulus length is not supported");
		return -ENOTSUP;
	}

	/* Allocate buffer to hold modexp params */
	ctx->modulus.data = rte_malloc(NULL, mod_len + exp_len, 0);
	if (ctx->modulus.data == NULL) {
		CPT_LOG_DP_ERR("Could not allocate buffer for modex params");
		return -ENOMEM;
	}

	/* Set up modexp prime modulus and private exponent */

	memcpy(ctx->modulus.data, mod, mod_len);
	ctx->exponent.data = ctx->modulus.data + mod_len;
	memcpy(ctx->exponent.data, exp, exp_len);

	ctx->modulus.length = mod_len;
	ctx->exponent.length = exp_len;

	return 0;
}

static __rte_always_inline int
cpt_fill_rsa_params(struct cpt_asym_sess_misc *sess,
		    struct rte_crypto_asym_xform *xform)
{
	struct rte_crypto_rsa_priv_key_qt qt = xform->rsa.qt;
	struct rte_crypto_rsa_xform *xfrm_rsa = &xform->rsa;
	struct rte_crypto_rsa_xform *rsa = &sess->rsa_ctx;
	size_t mod_len = xfrm_rsa->n.length;
	size_t exp_len = xfrm_rsa->e.length;
	uint64_t total_size;
	size_t len = 0;

	/* Make sure key length used is not more than mod_len/2 */
	if (qt.p.data != NULL)
		len = (((mod_len / 2) < qt.p.length) ? len : qt.p.length);

	/* Total size required for RSA key params(n,e,(q,dQ,p,dP,qInv)) */
	total_size = mod_len + exp_len + 5 * len;

	/* Allocate buffer to hold all RSA keys */
	rsa->n.data = rte_malloc(NULL, total_size, 0);
	if (rsa->n.data == NULL) {
		CPT_LOG_DP_ERR("Could not allocate buffer for RSA keys");
		return -ENOMEM;
	}

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
cpt_fill_asym_session_parameters(struct cpt_asym_sess_misc *sess,
				 struct rte_crypto_asym_xform *xform)
{
	int ret;

	sess->xfrm_type = xform->xform_type;

	switch (xform->xform_type) {
	case RTE_CRYPTO_ASYM_XFORM_RSA:
		ret = cpt_fill_rsa_params(sess, xform);
		break;
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		ret = cpt_fill_modex_params(sess, xform);
		break;
	default:
		CPT_LOG_DP_ERR("Unsupported transform type");
		return -ENOTSUP;
	}
	return ret;
}

static __rte_always_inline void
cpt_free_asym_session_parameters(struct cpt_asym_sess_misc *sess)
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
	default:
		CPT_LOG_DP_ERR("Invalid transform type");
		break;
	}
}

#endif /* _CPT_UCODE_ASYM_H_ */
