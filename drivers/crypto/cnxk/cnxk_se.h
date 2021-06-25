/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CNXK_SE_H_
#define _CNXK_SE_H_
#include <stdbool.h>

#include "roc_se.h"

struct cnxk_se_sess {
	uint16_t cpt_op : 4;
	uint16_t zsk_flag : 4;
	uint16_t aes_gcm : 1;
	uint16_t aes_ctr : 1;
	uint16_t chacha_poly : 1;
	uint16_t is_null : 1;
	uint16_t is_gmac : 1;
	uint16_t rsvd1 : 3;
	uint16_t aad_length;
	uint8_t mac_len;
	uint8_t iv_length;
	uint8_t auth_iv_length;
	uint16_t iv_offset;
	uint16_t auth_iv_offset;
	uint32_t salt;
	uint64_t cpt_inst_w7;
	struct roc_se_ctx roc_se_ctx;
} __rte_cache_aligned;

static __rte_always_inline int
cpt_mac_len_verify(struct rte_crypto_auth_xform *auth)
{
	uint16_t mac_len = auth->digest_length;
	int ret;

	switch (auth->algo) {
	case RTE_CRYPTO_AUTH_MD5:
	case RTE_CRYPTO_AUTH_MD5_HMAC:
		ret = (mac_len == 16) ? 0 : -1;
		break;
	case RTE_CRYPTO_AUTH_SHA1:
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		ret = (mac_len == 20) ? 0 : -1;
		break;
	case RTE_CRYPTO_AUTH_SHA224:
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
		ret = (mac_len == 28) ? 0 : -1;
		break;
	case RTE_CRYPTO_AUTH_SHA256:
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		ret = (mac_len == 32) ? 0 : -1;
		break;
	case RTE_CRYPTO_AUTH_SHA384:
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
		ret = (mac_len == 48) ? 0 : -1;
		break;
	case RTE_CRYPTO_AUTH_SHA512:
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		ret = (mac_len == 64) ? 0 : -1;
		break;
	case RTE_CRYPTO_AUTH_NULL:
		ret = 0;
		break;
	default:
		ret = -1;
	}

	return ret;
}


static __rte_always_inline int
fill_sess_cipher(struct rte_crypto_sym_xform *xform, struct cnxk_se_sess *sess)
{
	struct rte_crypto_cipher_xform *c_form;
	roc_se_cipher_type enc_type = 0; /* NULL Cipher type */
	uint32_t cipher_key_len = 0;
	uint8_t zsk_flag = 0, aes_ctr = 0, is_null = 0;

	c_form = &xform->cipher;

	if (c_form->op == RTE_CRYPTO_CIPHER_OP_ENCRYPT)
		sess->cpt_op |= ROC_SE_OP_CIPHER_ENCRYPT;
	else if (c_form->op == RTE_CRYPTO_CIPHER_OP_DECRYPT) {
		sess->cpt_op |= ROC_SE_OP_CIPHER_DECRYPT;
		if (xform->next != NULL &&
		    xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
			/* Perform decryption followed by auth verify */
			sess->roc_se_ctx.template_w4.s.opcode_minor =
				ROC_SE_FC_MINOR_OP_HMAC_FIRST;
		}
	} else {
		plt_dp_err("Unknown cipher operation\n");
		return -1;
	}

	switch (c_form->algo) {
	case RTE_CRYPTO_CIPHER_AES_CBC:
		enc_type = ROC_SE_AES_CBC;
		cipher_key_len = 16;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		enc_type = ROC_SE_DES3_CBC;
		cipher_key_len = 24;
		break;
	case RTE_CRYPTO_CIPHER_DES_CBC:
		/* DES is implemented using 3DES in hardware */
		enc_type = ROC_SE_DES3_CBC;
		cipher_key_len = 8;
		break;
	case RTE_CRYPTO_CIPHER_AES_CTR:
		enc_type = ROC_SE_AES_CTR;
		cipher_key_len = 16;
		aes_ctr = 1;
		break;
	case RTE_CRYPTO_CIPHER_NULL:
		enc_type = 0;
		is_null = 1;
		break;
	case RTE_CRYPTO_CIPHER_KASUMI_F8:
		enc_type = ROC_SE_KASUMI_F8_ECB;
		cipher_key_len = 16;
		zsk_flag = ROC_SE_K_F8;
		break;
	case RTE_CRYPTO_CIPHER_SNOW3G_UEA2:
		enc_type = ROC_SE_SNOW3G_UEA2;
		cipher_key_len = 16;
		zsk_flag = ROC_SE_ZS_EA;
		break;
	case RTE_CRYPTO_CIPHER_ZUC_EEA3:
		enc_type = ROC_SE_ZUC_EEA3;
		cipher_key_len = 16;
		zsk_flag = ROC_SE_ZS_EA;
		break;
	case RTE_CRYPTO_CIPHER_AES_XTS:
		enc_type = ROC_SE_AES_XTS;
		cipher_key_len = 16;
		break;
	case RTE_CRYPTO_CIPHER_3DES_ECB:
		enc_type = ROC_SE_DES3_ECB;
		cipher_key_len = 24;
		break;
	case RTE_CRYPTO_CIPHER_AES_ECB:
		enc_type = ROC_SE_AES_ECB;
		cipher_key_len = 16;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CTR:
	case RTE_CRYPTO_CIPHER_AES_F8:
	case RTE_CRYPTO_CIPHER_ARC4:
		plt_dp_err("Crypto: Unsupported cipher algo %u", c_form->algo);
		return -1;
	default:
		plt_dp_err("Crypto: Undefined cipher algo %u specified",
			   c_form->algo);
		return -1;
	}

	if (c_form->key.length < cipher_key_len) {
		plt_dp_err("Invalid cipher params keylen %u",
			   c_form->key.length);
		return -1;
	}

	sess->zsk_flag = zsk_flag;
	sess->aes_gcm = 0;
	sess->aes_ctr = aes_ctr;
	sess->iv_offset = c_form->iv.offset;
	sess->iv_length = c_form->iv.length;
	sess->is_null = is_null;

	if (unlikely(roc_se_ciph_key_set(&sess->roc_se_ctx, enc_type,
					 c_form->key.data, c_form->key.length,
					 NULL)))
		return -1;

	return 0;
}

static __rte_always_inline int
fill_sess_auth(struct rte_crypto_sym_xform *xform, struct cnxk_se_sess *sess)
{
	struct rte_crypto_auth_xform *a_form;
	roc_se_auth_type auth_type = 0; /* NULL Auth type */
	uint8_t zsk_flag = 0, aes_gcm = 0, is_null = 0;

	if (xform->next != NULL &&
	    xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
	    xform->next->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
		/* Perform auth followed by encryption */
		sess->roc_se_ctx.template_w4.s.opcode_minor =
			ROC_SE_FC_MINOR_OP_HMAC_FIRST;
	}

	a_form = &xform->auth;

	if (a_form->op == RTE_CRYPTO_AUTH_OP_VERIFY)
		sess->cpt_op |= ROC_SE_OP_AUTH_VERIFY;
	else if (a_form->op == RTE_CRYPTO_AUTH_OP_GENERATE)
		sess->cpt_op |= ROC_SE_OP_AUTH_GENERATE;
	else {
		plt_dp_err("Unknown auth operation");
		return -1;
	}

	switch (a_form->algo) {
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		/* Fall through */
	case RTE_CRYPTO_AUTH_SHA1:
		auth_type = ROC_SE_SHA1_TYPE;
		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
	case RTE_CRYPTO_AUTH_SHA256:
		auth_type = ROC_SE_SHA2_SHA256;
		break;
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
	case RTE_CRYPTO_AUTH_SHA512:
		auth_type = ROC_SE_SHA2_SHA512;
		break;
	case RTE_CRYPTO_AUTH_AES_GMAC:
		auth_type = ROC_SE_GMAC_TYPE;
		aes_gcm = 1;
		break;
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
	case RTE_CRYPTO_AUTH_SHA224:
		auth_type = ROC_SE_SHA2_SHA224;
		break;
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
	case RTE_CRYPTO_AUTH_SHA384:
		auth_type = ROC_SE_SHA2_SHA384;
		break;
	case RTE_CRYPTO_AUTH_MD5_HMAC:
	case RTE_CRYPTO_AUTH_MD5:
		auth_type = ROC_SE_MD5_TYPE;
		break;
	case RTE_CRYPTO_AUTH_KASUMI_F9:
		auth_type = ROC_SE_KASUMI_F9_ECB;
		/*
		 * Indicate that direction needs to be taken out
		 * from end of src
		 */
		zsk_flag = ROC_SE_K_F9;
		break;
	case RTE_CRYPTO_AUTH_SNOW3G_UIA2:
		auth_type = ROC_SE_SNOW3G_UIA2;
		zsk_flag = ROC_SE_ZS_IA;
		break;
	case RTE_CRYPTO_AUTH_ZUC_EIA3:
		auth_type = ROC_SE_ZUC_EIA3;
		zsk_flag = ROC_SE_ZS_IA;
		break;
	case RTE_CRYPTO_AUTH_NULL:
		auth_type = 0;
		is_null = 1;
		break;
	case RTE_CRYPTO_AUTH_AES_XCBC_MAC:
	case RTE_CRYPTO_AUTH_AES_CMAC:
	case RTE_CRYPTO_AUTH_AES_CBC_MAC:
		plt_dp_err("Crypto: Unsupported hash algo %u", a_form->algo);
		return -1;
	default:
		plt_dp_err("Crypto: Undefined Hash algo %u specified",
			   a_form->algo);
		return -1;
	}

	sess->zsk_flag = zsk_flag;
	sess->aes_gcm = aes_gcm;
	sess->mac_len = a_form->digest_length;
	sess->is_null = is_null;
	if (zsk_flag) {
		sess->auth_iv_offset = a_form->iv.offset;
		sess->auth_iv_length = a_form->iv.length;
	}
	if (unlikely(roc_se_auth_key_set(&sess->roc_se_ctx, auth_type,
					 a_form->key.data, a_form->key.length,
					 a_form->digest_length)))
		return -1;

	return 0;
}

static __rte_always_inline int
fill_sess_gmac(struct rte_crypto_sym_xform *xform, struct cnxk_se_sess *sess)
{
	struct rte_crypto_auth_xform *a_form;
	roc_se_cipher_type enc_type = 0; /* NULL Cipher type */
	roc_se_auth_type auth_type = 0;	 /* NULL Auth type */

	a_form = &xform->auth;

	if (a_form->op == RTE_CRYPTO_AUTH_OP_GENERATE)
		sess->cpt_op |= ROC_SE_OP_ENCODE;
	else if (a_form->op == RTE_CRYPTO_AUTH_OP_VERIFY)
		sess->cpt_op |= ROC_SE_OP_DECODE;
	else {
		plt_dp_err("Unknown auth operation");
		return -1;
	}

	switch (a_form->algo) {
	case RTE_CRYPTO_AUTH_AES_GMAC:
		enc_type = ROC_SE_AES_GCM;
		auth_type = ROC_SE_GMAC_TYPE;
		break;
	default:
		plt_dp_err("Crypto: Undefined cipher algo %u specified",
			   a_form->algo);
		return -1;
	}

	sess->zsk_flag = 0;
	sess->aes_gcm = 0;
	sess->is_gmac = 1;
	sess->iv_offset = a_form->iv.offset;
	sess->iv_length = a_form->iv.length;
	sess->mac_len = a_form->digest_length;

	if (unlikely(roc_se_ciph_key_set(&sess->roc_se_ctx, enc_type,
					 a_form->key.data, a_form->key.length,
					 NULL)))
		return -1;

	if (unlikely(roc_se_auth_key_set(&sess->roc_se_ctx, auth_type, NULL, 0,
					 a_form->digest_length)))
		return -1;

	return 0;
}

#endif /*_CNXK_SE_H_ */
