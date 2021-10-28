/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef __CNXK_IPSEC_H__
#define __CNXK_IPSEC_H__

#include <rte_security.h>
#include <rte_security_driver.h>

#include "roc_api.h"

extern struct rte_security_ops cnxk_sec_ops;

struct cnxk_cpt_inst_tmpl {
	uint64_t w2;
	uint64_t w4;
	uint64_t w7;
};

static inline int
ipsec_xform_cipher_verify(struct rte_crypto_sym_xform *crypto_xform)
{
	if (crypto_xform->cipher.algo == RTE_CRYPTO_CIPHER_AES_CBC) {
		switch (crypto_xform->cipher.key.length) {
		case 16:
		case 24:
		case 32:
			break;
		default:
			return -ENOTSUP;
		}
		return 0;
	}

	return -ENOTSUP;
}

static inline int
ipsec_xform_auth_verify(struct rte_crypto_sym_xform *crypto_xform)
{
	uint16_t keylen = crypto_xform->auth.key.length;

	if (crypto_xform->auth.algo == RTE_CRYPTO_AUTH_NULL)
		return 0;

	if (crypto_xform->auth.algo == RTE_CRYPTO_AUTH_SHA1_HMAC) {
		if (keylen >= 20 && keylen <= 64)
			return 0;
	} else if (roc_model_is_cn9k() &&
		   (crypto_xform->auth.algo == RTE_CRYPTO_AUTH_SHA256_HMAC)) {
		if (keylen >= 32 && keylen <= 64)
			return 0;
	}

	return -ENOTSUP;
}

static inline int
ipsec_xform_aead_verify(struct rte_security_ipsec_xform *ipsec_xform,
			struct rte_crypto_sym_xform *crypto_xform)
{
	if (ipsec_xform->direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS &&
	    crypto_xform->aead.op != RTE_CRYPTO_AEAD_OP_ENCRYPT)
		return -EINVAL;

	if (ipsec_xform->direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS &&
	    crypto_xform->aead.op != RTE_CRYPTO_AEAD_OP_DECRYPT)
		return -EINVAL;

	if (crypto_xform->aead.algo == RTE_CRYPTO_AEAD_AES_GCM) {
		switch (crypto_xform->aead.key.length) {
		case 16:
		case 24:
		case 32:
			break;
		default:
			return -EINVAL;
		}
		return 0;
	}

	return -ENOTSUP;
}

static inline int
cnxk_ipsec_xform_verify(struct rte_security_ipsec_xform *ipsec_xform,
			struct rte_crypto_sym_xform *crypto_xform)
{
	struct rte_crypto_sym_xform *auth_xform, *cipher_xform;
	int ret;

	if ((ipsec_xform->direction != RTE_SECURITY_IPSEC_SA_DIR_INGRESS) &&
	    (ipsec_xform->direction != RTE_SECURITY_IPSEC_SA_DIR_EGRESS))
		return -EINVAL;

	if ((ipsec_xform->proto != RTE_SECURITY_IPSEC_SA_PROTO_ESP) &&
	    (ipsec_xform->proto != RTE_SECURITY_IPSEC_SA_PROTO_AH))
		return -EINVAL;

	if ((ipsec_xform->mode != RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT) &&
	    (ipsec_xform->mode != RTE_SECURITY_IPSEC_SA_MODE_TUNNEL))
		return -EINVAL;

	if ((ipsec_xform->mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL) &&
	    (ipsec_xform->tunnel.type != RTE_SECURITY_IPSEC_TUNNEL_IPV4) &&
	    (ipsec_xform->tunnel.type != RTE_SECURITY_IPSEC_TUNNEL_IPV6))
		return -EINVAL;

	if (crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AEAD)
		return ipsec_xform_aead_verify(ipsec_xform, crypto_xform);

	if (crypto_xform->next == NULL)
		return -EINVAL;

	if (ipsec_xform->direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) {
		/* Ingress */
		if (crypto_xform->type != RTE_CRYPTO_SYM_XFORM_AUTH ||
		    crypto_xform->next->type != RTE_CRYPTO_SYM_XFORM_CIPHER)
			return -EINVAL;
		auth_xform = crypto_xform;
		cipher_xform = crypto_xform->next;
	} else {
		/* Egress */
		if (crypto_xform->type != RTE_CRYPTO_SYM_XFORM_CIPHER ||
		    crypto_xform->next->type != RTE_CRYPTO_SYM_XFORM_AUTH)
			return -EINVAL;
		cipher_xform = crypto_xform;
		auth_xform = crypto_xform->next;
	}

	ret = ipsec_xform_cipher_verify(cipher_xform);
	if (ret)
		return ret;

	return ipsec_xform_auth_verify(auth_xform);
}
#endif /* __CNXK_IPSEC_H__ */
