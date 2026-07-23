/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef _CNXK_SECURITY_H__
#define _CNXK_SECURITY_H__

#include <errno.h>

#include <rte_crypto.h>
#include <rte_security.h>

#include "roc_cpt.h"
#include "roc_ie_on.h"
#include "roc_ie_ot.h"
#include "roc_ie_ow.h"

/* Response length calculation data */
struct cnxk_ipsec_outb_rlens {
	uint16_t partial_len;
	uint8_t roundup_byte;
	int8_t roundup_len;
	uint16_t max_extended_len;
};

int __roc_api
cnxk_ipsec_outb_rlens_get(struct cnxk_ipsec_outb_rlens *rlens,
			  struct rte_security_ipsec_xform *ipsec_xfrm,
			  struct rte_crypto_sym_xform *crypto_xfrm);
uint8_t __roc_api
cnxk_ipsec_ivlen_get(enum rte_crypto_cipher_algorithm c_algo,
		     enum rte_crypto_auth_algorithm a_algo,
		     enum rte_crypto_aead_algorithm aead_algo);
uint8_t __roc_api
cnxk_ipsec_icvlen_get(enum rte_crypto_cipher_algorithm c_algo,
		      enum rte_crypto_auth_algorithm a_algo,
		      enum rte_crypto_aead_algorithm aead_algo);

uint8_t __roc_api cnxk_ipsec_outb_roundup_byte(enum rte_crypto_cipher_algorithm c_algo,
					       enum rte_crypto_aead_algorithm aead_algo);

/* [CN10K] */
int __roc_api cnxk_ot_ipsec_inb_sa_fill(struct roc_ot_ipsec_inb_sa *sa,
					struct rte_security_ipsec_xform *ipsec_xfrm,
					struct rte_crypto_sym_xform *crypto_xfrm, uint8_t ctx_ilen);
int __roc_api cnxk_ot_ipsec_outb_sa_fill(struct roc_ot_ipsec_outb_sa *sa,
					 struct rte_security_ipsec_xform *ipsec_xfrm,
					 struct rte_crypto_sym_xform *crypto_xfrm,
					 uint8_t ctx_ilen);
bool __roc_api cnxk_ot_ipsec_inb_sa_valid(struct roc_ot_ipsec_inb_sa *sa);
bool __roc_api cnxk_ot_ipsec_outb_sa_valid(struct roc_ot_ipsec_outb_sa *sa);

/* [CN9K] */
int __roc_api cnxk_on_ipsec_inb_sa_create(struct rte_security_ipsec_xform *ipsec,
					  struct rte_crypto_sym_xform *crypto_xform,
					  struct roc_ie_on_inb_sa *in_sa);

int __roc_api cnxk_on_ipsec_outb_sa_create(struct rte_security_ipsec_xform *ipsec,
					   struct rte_crypto_sym_xform *crypto_xform,
					   struct roc_ie_on_outb_sa *out_sa);
/* [CN20K, .) */
int __roc_api cnxk_ow_ipsec_inb_sa_fill(struct roc_ow_ipsec_inb_sa *sa,
					struct rte_security_ipsec_xform *ipsec_xfrm,
					struct rte_crypto_sym_xform *crypto_xfrm, uint8_t ctx_ilen);
int __roc_api cnxk_ow_ipsec_outb_sa_fill(struct roc_ow_ipsec_outb_sa *sa,
					 struct rte_security_ipsec_xform *ipsec_xfrm,
					 struct rte_crypto_sym_xform *crypto_xfrm,
					 uint8_t ctx_ilen);
bool __roc_api cnxk_ow_ipsec_inb_sa_valid(struct roc_ow_ipsec_inb_sa *sa);
bool __roc_api cnxk_ow_ipsec_outb_sa_valid(struct roc_ow_ipsec_outb_sa *sa);

static inline int
ipsec_xform_cipher_verify(struct rte_crypto_sym_xform *crypto_xform)
{
	if (crypto_xform->cipher.algo == RTE_CRYPTO_CIPHER_NULL)
		return 0;

	if (crypto_xform->cipher.algo == RTE_CRYPTO_CIPHER_DES_CBC &&
	    crypto_xform->cipher.key.length == 8)
		return 0;

	if (crypto_xform->cipher.algo == RTE_CRYPTO_CIPHER_AES_CBC ||
	    crypto_xform->cipher.algo == RTE_CRYPTO_CIPHER_AES_CTR) {
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

	if (crypto_xform->cipher.algo == RTE_CRYPTO_CIPHER_3DES_CBC &&
	    crypto_xform->cipher.key.length == 24)
		return 0;

	return -ENOTSUP;
}

static inline int
ipsec_xform_auth_verify(struct rte_crypto_sym_xform *crypto_xform)
{
	uint16_t keylen = crypto_xform->auth.key.length;

	if (crypto_xform->auth.algo == RTE_CRYPTO_AUTH_NULL)
		return 0;

	if (crypto_xform->auth.algo == RTE_CRYPTO_AUTH_MD5_HMAC) {
		if (keylen == 16)
			return 0;
	}

	if (crypto_xform->auth.algo == RTE_CRYPTO_AUTH_SHA1_HMAC) {
		if (keylen >= 20 && keylen <= 64)
			return 0;
	} else if (crypto_xform->auth.algo == RTE_CRYPTO_AUTH_SHA256_HMAC) {
		if (keylen >= 32 && keylen <= 64)
			return 0;
	} else if (crypto_xform->auth.algo == RTE_CRYPTO_AUTH_SHA384_HMAC) {
		if (keylen == 48)
			return 0;
	} else if (crypto_xform->auth.algo == RTE_CRYPTO_AUTH_SHA512_HMAC) {
		if (keylen == 64)
			return 0;
	} else if (crypto_xform->auth.algo == RTE_CRYPTO_AUTH_AES_GMAC) {
		if (keylen >= 16 && keylen <= 32)
			return 0;
	}

	if (crypto_xform->auth.algo == RTE_CRYPTO_AUTH_AES_XCBC_MAC &&
	    keylen == ROC_CPT_AES_XCBC_KEY_LENGTH)
		return 0;

	return -ENOTSUP;
}

static inline int
ipsec_xform_aead_verify(struct rte_security_ipsec_xform *ipsec_xform __rte_unused,
			struct rte_crypto_sym_xform *crypto_xform)
{
	if (crypto_xform->aead.algo == RTE_CRYPTO_AEAD_AES_GCM ||
	    crypto_xform->aead.algo == RTE_CRYPTO_AEAD_AES_CCM) {
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

	if (crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		if (ipsec_xform->proto != RTE_SECURITY_IPSEC_SA_PROTO_ESP)
			return -EINVAL;
		return ipsec_xform_aead_verify(ipsec_xform, crypto_xform);
	}

	if (ipsec_xform->proto == RTE_SECURITY_IPSEC_SA_PROTO_AH) {
		if (ipsec_xform->direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) {
			/* Ingress */
			auth_xform = crypto_xform;
			cipher_xform = crypto_xform->next;

			if (crypto_xform->type != RTE_CRYPTO_SYM_XFORM_AUTH)
				return -EINVAL;

			if ((cipher_xform != NULL) &&
			    ((cipher_xform->type != RTE_CRYPTO_SYM_XFORM_CIPHER) ||
			     (cipher_xform->cipher.algo != RTE_CRYPTO_CIPHER_NULL)))
				return -EINVAL;
		} else {
			/* Egress */
			if (crypto_xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
				cipher_xform = crypto_xform;
				auth_xform = crypto_xform->next;

				if (auth_xform == NULL ||
				    cipher_xform->cipher.algo != RTE_CRYPTO_CIPHER_NULL)
					return -EINVAL;
			} else if (crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AUTH)
				auth_xform = crypto_xform;
			else
				return -EINVAL;
		}
	} else {
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
	}

	return ipsec_xform_auth_verify(auth_xform);
}

#endif /* _CNXK_SECURITY_H__ */
