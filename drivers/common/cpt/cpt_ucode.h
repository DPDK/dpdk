/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#ifndef _CPT_UCODE_H_
#define _CPT_UCODE_H_

#include "cpt_mcode_defines.h"

/*
 * This file defines functions that are interfaces to microcode spec.
 *
 */

static uint8_t zuc_d[32] = {
	0x44, 0xD7, 0x26, 0xBC, 0x62, 0x6B, 0x13, 0x5E,
	0x57, 0x89, 0x35, 0xE2, 0x71, 0x35, 0x09, 0xAF,
	0x4D, 0x78, 0x2F, 0x13, 0x6B, 0xC4, 0x1A, 0xF1,
	0x5E, 0x26, 0x3C, 0x4D, 0x78, 0x9A, 0x47, 0xAC
};

static __rte_always_inline int
cpt_is_algo_supported(struct rte_crypto_sym_xform *xform)
{
	/*
	 * Microcode only supports the following combination.
	 * Encryption followed by authentication
	 * Authentication followed by decryption
	 */
	if (xform->next) {
		if ((xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) &&
		    (xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER) &&
		    (xform->next->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT)) {
			/* Unsupported as of now by microcode */
			CPT_LOG_DP_ERR("Unsupported combination");
			return -1;
		}
		if ((xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) &&
		    (xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH) &&
		    (xform->cipher.op == RTE_CRYPTO_CIPHER_OP_DECRYPT)) {
			/* For GMAC auth there is no cipher operation */
			if (xform->aead.algo != RTE_CRYPTO_AEAD_AES_GCM ||
			    xform->next->auth.algo !=
			    RTE_CRYPTO_AUTH_AES_GMAC) {
				/* Unsupported as of now by microcode */
				CPT_LOG_DP_ERR("Unsupported combination");
				return -1;
			}
		}
	}
	return 0;
}

static __rte_always_inline void
gen_key_snow3g(uint8_t *ck, uint32_t *keyx)
{
	int i, base;

	for (i = 0; i < 4; i++) {
		base = 4 * i;
		keyx[3 - i] = (ck[base] << 24) | (ck[base + 1] << 16) |
			(ck[base + 2] << 8) | (ck[base + 3]);
		keyx[3 - i] = rte_cpu_to_be_32(keyx[3 - i]);
	}
}

static __rte_always_inline int
cpt_fc_ciph_validate_key_aes(uint16_t key_len)
{
	switch (key_len) {
	case CPT_BYTE_16:
	case CPT_BYTE_24:
	case CPT_BYTE_32:
		return 0;
	default:
		return -1;
	}
}

static __rte_always_inline int
cpt_fc_ciph_validate_key(cipher_type_t type, struct cpt_ctx *cpt_ctx,
		uint16_t key_len)
{
	int fc_type = 0;
	switch (type) {
	case PASSTHROUGH:
		fc_type = FC_GEN;
		break;
	case DES3_CBC:
	case DES3_ECB:
		fc_type = FC_GEN;
		break;
	case AES_CBC:
	case AES_ECB:
	case AES_CFB:
	case AES_CTR:
	case AES_GCM:
		if (unlikely(cpt_fc_ciph_validate_key_aes(key_len) != 0))
			return -1;
		fc_type = FC_GEN;
		break;
	case AES_XTS:
		key_len = key_len / 2;
		if (unlikely(key_len == CPT_BYTE_24)) {
			CPT_LOG_DP_ERR("Invalid AES key len for XTS");
			return -1;
		}
		if (unlikely(cpt_fc_ciph_validate_key_aes(key_len) != 0))
			return -1;
		fc_type = FC_GEN;
		break;
	case ZUC_EEA3:
	case SNOW3G_UEA2:
		if (unlikely(key_len != 16))
			return -1;
		/* No support for AEAD yet */
		if (unlikely(cpt_ctx->hash_type))
			return -1;
		fc_type = ZUC_SNOW3G;
		break;
	case KASUMI_F8_CBC:
	case KASUMI_F8_ECB:
		if (unlikely(key_len != 16))
			return -1;
		/* No support for AEAD yet */
		if (unlikely(cpt_ctx->hash_type))
			return -1;
		fc_type = KASUMI;
		break;
	default:
		return -1;
	}
	return fc_type;
}

static __rte_always_inline void
cpt_fc_ciph_set_key_passthrough(struct cpt_ctx *cpt_ctx, mc_fc_context_t *fctx)
{
	cpt_ctx->enc_cipher = 0;
	CPT_P_ENC_CTRL(fctx).enc_cipher = 0;
}

static __rte_always_inline void
cpt_fc_ciph_set_key_set_aes_key_type(mc_fc_context_t *fctx, uint16_t key_len)
{
	mc_aes_type_t aes_key_type = 0;
	switch (key_len) {
	case CPT_BYTE_16:
		aes_key_type = AES_128_BIT;
		break;
	case CPT_BYTE_24:
		aes_key_type = AES_192_BIT;
		break;
	case CPT_BYTE_32:
		aes_key_type = AES_256_BIT;
		break;
	default:
		/* This should not happen */
		CPT_LOG_DP_ERR("Invalid AES key len");
		return;
	}
	CPT_P_ENC_CTRL(fctx).aes_key = aes_key_type;
}

static __rte_always_inline void
cpt_fc_ciph_set_key_snow3g_uea2(struct cpt_ctx *cpt_ctx, uint8_t *key,
		uint16_t key_len)
{
	uint32_t keyx[4];
	cpt_ctx->snow3g = 1;
	gen_key_snow3g(key, keyx);
	memcpy(cpt_ctx->zs_ctx.ci_key, keyx, key_len);
	cpt_ctx->fc_type = ZUC_SNOW3G;
	cpt_ctx->zsk_flags = 0;
}

static __rte_always_inline void
cpt_fc_ciph_set_key_zuc_eea3(struct cpt_ctx *cpt_ctx, uint8_t *key,
		uint16_t key_len)
{
	cpt_ctx->snow3g = 0;
	memcpy(cpt_ctx->zs_ctx.ci_key, key, key_len);
	memcpy(cpt_ctx->zs_ctx.zuc_const, zuc_d, 32);
	cpt_ctx->fc_type = ZUC_SNOW3G;
	cpt_ctx->zsk_flags = 0;
}

static __rte_always_inline void
cpt_fc_ciph_set_key_kasumi_f8_ecb(struct cpt_ctx *cpt_ctx, uint8_t *key,
		uint16_t key_len)
{
	cpt_ctx->k_ecb = 1;
	memcpy(cpt_ctx->k_ctx.ci_key, key, key_len);
	cpt_ctx->zsk_flags = 0;
	cpt_ctx->fc_type = KASUMI;
}

static __rte_always_inline void
cpt_fc_ciph_set_key_kasumi_f8_cbc(struct cpt_ctx *cpt_ctx, uint8_t *key,
		uint16_t key_len)
{
	memcpy(cpt_ctx->k_ctx.ci_key, key, key_len);
	cpt_ctx->zsk_flags = 0;
	cpt_ctx->fc_type = KASUMI;
}

static __rte_always_inline int
cpt_fc_ciph_set_key(void *ctx, cipher_type_t type, uint8_t *key,
		    uint16_t key_len, uint8_t *salt)
{
	struct cpt_ctx *cpt_ctx = ctx;
	mc_fc_context_t *fctx = &cpt_ctx->fctx;
	uint64_t *ctrl_flags = NULL;
	int fc_type;

	/* Validate key before proceeding */
	fc_type = cpt_fc_ciph_validate_key(type, cpt_ctx, key_len);
	if (unlikely(fc_type == -1))
		return -1;

	if (fc_type == FC_GEN) {
		cpt_ctx->fc_type = FC_GEN;
		ctrl_flags = (uint64_t *)&(fctx->enc.enc_ctrl.flags);
		*ctrl_flags = rte_be_to_cpu_64(*ctrl_flags);
		/*
		 * We need to always say IV is from DPTR as user can
		 * sometimes iverride IV per operation.
		 */
		CPT_P_ENC_CTRL(fctx).iv_source = CPT_FROM_DPTR;
	}

	switch (type) {
	case PASSTHROUGH:
		cpt_fc_ciph_set_key_passthrough(cpt_ctx, fctx);
		goto fc_success;
	case DES3_CBC:
		/* CPT performs DES using 3DES with the 8B DES-key
		 * replicated 2 more times to match the 24B 3DES-key.
		 * Eg. If org. key is "0x0a 0x0b", then new key is
		 * "0x0a 0x0b 0x0a 0x0b 0x0a 0x0b"
		 */
		if (key_len == 8) {
			/* Skipping the first 8B as it will be copied
			 * in the regular code flow
			 */
			memcpy(fctx->enc.encr_key+key_len, key, key_len);
			memcpy(fctx->enc.encr_key+2*key_len, key, key_len);
		}
		break;
	case DES3_ECB:
		/* For DES3_ECB IV need to be from CTX. */
		CPT_P_ENC_CTRL(fctx).iv_source = CPT_FROM_CTX;
		break;
	case AES_CBC:
	case AES_ECB:
	case AES_CFB:
	case AES_CTR:
		cpt_fc_ciph_set_key_set_aes_key_type(fctx, key_len);
		break;
	case AES_GCM:
		/* Even though iv source is from dptr,
		 * aes_gcm salt is taken from ctx
		 */
		if (salt) {
			memcpy(fctx->enc.encr_iv, salt, 4);
			/* Assuming it was just salt update
			 * and nothing else
			 */
			if (!key)
				goto fc_success;
		}
		cpt_fc_ciph_set_key_set_aes_key_type(fctx, key_len);
		break;
	case AES_XTS:
		key_len = key_len / 2;
		cpt_fc_ciph_set_key_set_aes_key_type(fctx, key_len);

		/* Copy key2 for XTS into ipad */
		memset(fctx->hmac.ipad, 0, sizeof(fctx->hmac.ipad));
		memcpy(fctx->hmac.ipad, &key[key_len], key_len);
		break;
	case SNOW3G_UEA2:
		cpt_fc_ciph_set_key_snow3g_uea2(cpt_ctx, key, key_len);
		goto success;
	case ZUC_EEA3:
		cpt_fc_ciph_set_key_zuc_eea3(cpt_ctx, key, key_len);
		goto success;
	case KASUMI_F8_ECB:
		cpt_fc_ciph_set_key_kasumi_f8_ecb(cpt_ctx, key, key_len);
		goto success;
	case KASUMI_F8_CBC:
		cpt_fc_ciph_set_key_kasumi_f8_cbc(cpt_ctx, key, key_len);
		goto success;
	default:
		break;
	}

	/* Only for FC_GEN case */

	/* For GMAC auth, cipher must be NULL */
	if (cpt_ctx->hash_type != GMAC_TYPE)
		CPT_P_ENC_CTRL(fctx).enc_cipher = type;

	memcpy(fctx->enc.encr_key, key, key_len);

fc_success:
	*ctrl_flags = rte_cpu_to_be_64(*ctrl_flags);

success:
	cpt_ctx->enc_cipher = type;

	return 0;
}

static __rte_always_inline int
cpt_fc_auth_set_key(void *ctx, auth_type_t type, uint8_t *key,
		    uint16_t key_len, uint16_t mac_len)
{
	struct cpt_ctx *cpt_ctx = ctx;
	mc_fc_context_t *fctx = &cpt_ctx->fctx;
	uint64_t *ctrl_flags = NULL;

	if ((type >= ZUC_EIA3) && (type <= KASUMI_F9_ECB)) {
		uint32_t keyx[4];

		if (key_len != 16)
			return -1;
		/* No support for AEAD yet */
		if (cpt_ctx->enc_cipher)
			return -1;
		/* For ZUC/SNOW3G/Kasumi */
		switch (type) {
		case SNOW3G_UIA2:
			cpt_ctx->snow3g = 1;
			gen_key_snow3g(key, keyx);
			memcpy(cpt_ctx->zs_ctx.ci_key, keyx, key_len);
			cpt_ctx->fc_type = ZUC_SNOW3G;
			cpt_ctx->zsk_flags = 0x1;
			break;
		case ZUC_EIA3:
			cpt_ctx->snow3g = 0;
			memcpy(cpt_ctx->zs_ctx.ci_key, key, key_len);
			memcpy(cpt_ctx->zs_ctx.zuc_const, zuc_d, 32);
			cpt_ctx->fc_type = ZUC_SNOW3G;
			cpt_ctx->zsk_flags = 0x1;
			break;
		case KASUMI_F9_ECB:
			/* Kasumi ECB mode */
			cpt_ctx->k_ecb = 1;
			memcpy(cpt_ctx->k_ctx.ci_key, key, key_len);
			cpt_ctx->fc_type = KASUMI;
			cpt_ctx->zsk_flags = 0x1;
			break;
		case KASUMI_F9_CBC:
			memcpy(cpt_ctx->k_ctx.ci_key, key, key_len);
			cpt_ctx->fc_type = KASUMI;
			cpt_ctx->zsk_flags = 0x1;
			break;
		default:
			return -1;
		}
		cpt_ctx->mac_len = 4;
		cpt_ctx->hash_type = type;
		return 0;
	}

	if (!(cpt_ctx->fc_type == FC_GEN && !type)) {
		if (!cpt_ctx->fc_type || !cpt_ctx->enc_cipher)
			cpt_ctx->fc_type = HASH_HMAC;
	}

	ctrl_flags = (uint64_t *)&fctx->enc.enc_ctrl.flags;
	*ctrl_flags = rte_be_to_cpu_64(*ctrl_flags);

	/* For GMAC auth, cipher must be NULL */
	if (type == GMAC_TYPE)
		CPT_P_ENC_CTRL(fctx).enc_cipher = 0;

	CPT_P_ENC_CTRL(fctx).hash_type = cpt_ctx->hash_type = type;
	CPT_P_ENC_CTRL(fctx).mac_len = cpt_ctx->mac_len = mac_len;

	if (key_len) {
		cpt_ctx->hmac = 1;
		memset(cpt_ctx->auth_key, 0, sizeof(cpt_ctx->auth_key));
		memcpy(cpt_ctx->auth_key, key, key_len);
		cpt_ctx->auth_key_len = key_len;
		memset(fctx->hmac.ipad, 0, sizeof(fctx->hmac.ipad));
		memset(fctx->hmac.opad, 0, sizeof(fctx->hmac.opad));
		memcpy(fctx->hmac.opad, key, key_len);
		CPT_P_ENC_CTRL(fctx).auth_input_type = 1;
	}
	*ctrl_flags = rte_cpu_to_be_64(*ctrl_flags);
	return 0;
}

static __rte_always_inline int
fill_sess_aead(struct rte_crypto_sym_xform *xform,
		 struct cpt_sess_misc *sess)
{
	struct rte_crypto_aead_xform *aead_form;
	cipher_type_t enc_type = 0; /* NULL Cipher type */
	auth_type_t auth_type = 0; /* NULL Auth type */
	uint32_t cipher_key_len = 0;
	uint8_t zsk_flag = 0, aes_gcm = 0;
	aead_form = &xform->aead;
	void *ctx;

	if (aead_form->op == RTE_CRYPTO_AEAD_OP_ENCRYPT &&
	   aead_form->algo == RTE_CRYPTO_AEAD_AES_GCM) {
		sess->cpt_op |= CPT_OP_CIPHER_ENCRYPT;
		sess->cpt_op |= CPT_OP_AUTH_GENERATE;
	} else if (aead_form->op == RTE_CRYPTO_AEAD_OP_DECRYPT &&
		aead_form->algo == RTE_CRYPTO_AEAD_AES_GCM) {
		sess->cpt_op |= CPT_OP_CIPHER_DECRYPT;
		sess->cpt_op |= CPT_OP_AUTH_VERIFY;
	} else {
		CPT_LOG_DP_ERR("Unknown cipher operation\n");
		return -1;
	}
	switch (aead_form->algo) {
	case RTE_CRYPTO_AEAD_AES_GCM:
		enc_type = AES_GCM;
		cipher_key_len = 16;
		aes_gcm = 1;
		break;
	case RTE_CRYPTO_AEAD_AES_CCM:
		CPT_LOG_DP_ERR("Crypto: Unsupported cipher algo %u",
			       aead_form->algo);
		return -1;
	default:
		CPT_LOG_DP_ERR("Crypto: Undefined cipher algo %u specified",
			       aead_form->algo);
		return -1;
	}
	if (aead_form->key.length < cipher_key_len) {
		CPT_LOG_DP_ERR("Invalid cipher params keylen %lu",
			       (unsigned int long)aead_form->key.length);
		return -1;
	}
	sess->zsk_flag = zsk_flag;
	sess->aes_gcm = aes_gcm;
	sess->mac_len = aead_form->digest_length;
	sess->iv_offset = aead_form->iv.offset;
	sess->iv_length = aead_form->iv.length;
	sess->aad_length = aead_form->aad_length;
	ctx = (void *)((uint8_t *)sess + sizeof(struct cpt_sess_misc)),

	cpt_fc_ciph_set_key(ctx, enc_type, aead_form->key.data,
			aead_form->key.length, NULL);

	cpt_fc_auth_set_key(ctx, auth_type, NULL, 0, aead_form->digest_length);

	return 0;
}

static __rte_always_inline int
fill_sess_cipher(struct rte_crypto_sym_xform *xform,
		 struct cpt_sess_misc *sess)
{
	struct rte_crypto_cipher_xform *c_form;
	cipher_type_t enc_type = 0; /* NULL Cipher type */
	uint32_t cipher_key_len = 0;
	uint8_t zsk_flag = 0, aes_gcm = 0, aes_ctr = 0, is_null = 0;

	if (xform->type != RTE_CRYPTO_SYM_XFORM_CIPHER)
		return -1;

	c_form = &xform->cipher;

	if (c_form->op == RTE_CRYPTO_CIPHER_OP_ENCRYPT)
		sess->cpt_op |= CPT_OP_CIPHER_ENCRYPT;
	else if (c_form->op == RTE_CRYPTO_CIPHER_OP_DECRYPT)
		sess->cpt_op |= CPT_OP_CIPHER_DECRYPT;
	else {
		CPT_LOG_DP_ERR("Unknown cipher operation\n");
		return -1;
	}

	switch (c_form->algo) {
	case RTE_CRYPTO_CIPHER_AES_CBC:
		enc_type = AES_CBC;
		cipher_key_len = 16;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		enc_type = DES3_CBC;
		cipher_key_len = 24;
		break;
	case RTE_CRYPTO_CIPHER_DES_CBC:
		/* DES is implemented using 3DES in hardware */
		enc_type = DES3_CBC;
		cipher_key_len = 8;
		break;
	case RTE_CRYPTO_CIPHER_AES_CTR:
		enc_type = AES_CTR;
		cipher_key_len = 16;
		aes_ctr = 1;
		break;
	case RTE_CRYPTO_CIPHER_NULL:
		enc_type = 0;
		is_null = 1;
		break;
	case RTE_CRYPTO_CIPHER_KASUMI_F8:
		enc_type = KASUMI_F8_ECB;
		cipher_key_len = 16;
		zsk_flag = K_F8;
		break;
	case RTE_CRYPTO_CIPHER_SNOW3G_UEA2:
		enc_type = SNOW3G_UEA2;
		cipher_key_len = 16;
		zsk_flag = ZS_EA;
		break;
	case RTE_CRYPTO_CIPHER_ZUC_EEA3:
		enc_type = ZUC_EEA3;
		cipher_key_len = 16;
		zsk_flag = ZS_EA;
		break;
	case RTE_CRYPTO_CIPHER_AES_XTS:
		enc_type = AES_XTS;
		cipher_key_len = 16;
		break;
	case RTE_CRYPTO_CIPHER_3DES_ECB:
		enc_type = DES3_ECB;
		cipher_key_len = 24;
		break;
	case RTE_CRYPTO_CIPHER_AES_ECB:
		enc_type = AES_ECB;
		cipher_key_len = 16;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CTR:
	case RTE_CRYPTO_CIPHER_AES_F8:
	case RTE_CRYPTO_CIPHER_ARC4:
		CPT_LOG_DP_ERR("Crypto: Unsupported cipher algo %u",
			       c_form->algo);
		return -1;
	default:
		CPT_LOG_DP_ERR("Crypto: Undefined cipher algo %u specified",
			       c_form->algo);
		return -1;
	}

	if (c_form->key.length < cipher_key_len) {
		CPT_LOG_DP_ERR("Invalid cipher params keylen %lu",
			       (unsigned long) c_form->key.length);
		return -1;
	}

	sess->zsk_flag = zsk_flag;
	sess->aes_gcm = aes_gcm;
	sess->aes_ctr = aes_ctr;
	sess->iv_offset = c_form->iv.offset;
	sess->iv_length = c_form->iv.length;
	sess->is_null = is_null;

	cpt_fc_ciph_set_key(SESS_PRIV(sess), enc_type, c_form->key.data,
			    c_form->key.length, NULL);

	return 0;
}

static __rte_always_inline int
fill_sess_auth(struct rte_crypto_sym_xform *xform,
	       struct cpt_sess_misc *sess)
{
	struct rte_crypto_auth_xform *a_form;
	auth_type_t auth_type = 0; /* NULL Auth type */
	uint8_t zsk_flag = 0, aes_gcm = 0, is_null = 0;

	if (xform->type != RTE_CRYPTO_SYM_XFORM_AUTH)
		goto error_out;

	a_form = &xform->auth;

	if (a_form->op == RTE_CRYPTO_AUTH_OP_VERIFY)
		sess->cpt_op |= CPT_OP_AUTH_VERIFY;
	else if (a_form->op == RTE_CRYPTO_AUTH_OP_GENERATE)
		sess->cpt_op |= CPT_OP_AUTH_GENERATE;
	else {
		CPT_LOG_DP_ERR("Unknown auth operation");
		return -1;
	}

	if (a_form->key.length > 64) {
		CPT_LOG_DP_ERR("Auth key length is big");
		return -1;
	}

	switch (a_form->algo) {
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		/* Fall through */
	case RTE_CRYPTO_AUTH_SHA1:
		auth_type = SHA1_TYPE;
		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
	case RTE_CRYPTO_AUTH_SHA256:
		auth_type = SHA2_SHA256;
		break;
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
	case RTE_CRYPTO_AUTH_SHA512:
		auth_type = SHA2_SHA512;
		break;
	case RTE_CRYPTO_AUTH_AES_GMAC:
		auth_type = GMAC_TYPE;
		aes_gcm = 1;
		break;
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
	case RTE_CRYPTO_AUTH_SHA224:
		auth_type = SHA2_SHA224;
		break;
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
	case RTE_CRYPTO_AUTH_SHA384:
		auth_type = SHA2_SHA384;
		break;
	case RTE_CRYPTO_AUTH_MD5_HMAC:
	case RTE_CRYPTO_AUTH_MD5:
		auth_type = MD5_TYPE;
		break;
	case RTE_CRYPTO_AUTH_KASUMI_F9:
		auth_type = KASUMI_F9_ECB;
		/*
		 * Indicate that direction needs to be taken out
		 * from end of src
		 */
		zsk_flag = K_F9;
		break;
	case RTE_CRYPTO_AUTH_SNOW3G_UIA2:
		auth_type = SNOW3G_UIA2;
		zsk_flag = ZS_IA;
		break;
	case RTE_CRYPTO_AUTH_ZUC_EIA3:
		auth_type = ZUC_EIA3;
		zsk_flag = ZS_IA;
		break;
	case RTE_CRYPTO_AUTH_NULL:
		auth_type = 0;
		is_null = 1;
		break;
	case RTE_CRYPTO_AUTH_AES_XCBC_MAC:
	case RTE_CRYPTO_AUTH_AES_CMAC:
	case RTE_CRYPTO_AUTH_AES_CBC_MAC:
		CPT_LOG_DP_ERR("Crypto: Unsupported hash algo %u",
			       a_form->algo);
		goto error_out;
	default:
		CPT_LOG_DP_ERR("Crypto: Undefined Hash algo %u specified",
			       a_form->algo);
		goto error_out;
	}

	sess->zsk_flag = zsk_flag;
	sess->aes_gcm = aes_gcm;
	sess->mac_len = a_form->digest_length;
	sess->is_null = is_null;
	if (zsk_flag) {
		sess->auth_iv_offset = a_form->iv.offset;
		sess->auth_iv_length = a_form->iv.length;
	}
	cpt_fc_auth_set_key(SESS_PRIV(sess), auth_type, a_form->key.data,
			    a_form->key.length, a_form->digest_length);

	return 0;

error_out:
	return -1;
}

static __rte_always_inline int
fill_sess_gmac(struct rte_crypto_sym_xform *xform,
		 struct cpt_sess_misc *sess)
{
	struct rte_crypto_auth_xform *a_form;
	cipher_type_t enc_type = 0; /* NULL Cipher type */
	auth_type_t auth_type = 0; /* NULL Auth type */
	uint8_t zsk_flag = 0, aes_gcm = 0;
	void *ctx;

	if (xform->type != RTE_CRYPTO_SYM_XFORM_AUTH)
		return -1;

	a_form = &xform->auth;

	if (a_form->op == RTE_CRYPTO_AUTH_OP_GENERATE)
		sess->cpt_op |= CPT_OP_ENCODE;
	else if (a_form->op == RTE_CRYPTO_AUTH_OP_VERIFY)
		sess->cpt_op |= CPT_OP_DECODE;
	else {
		CPT_LOG_DP_ERR("Unknown auth operation");
		return -1;
	}

	switch (a_form->algo) {
	case RTE_CRYPTO_AUTH_AES_GMAC:
		enc_type = AES_GCM;
		auth_type = GMAC_TYPE;
		break;
	default:
		CPT_LOG_DP_ERR("Crypto: Undefined cipher algo %u specified",
			       a_form->algo);
		return -1;
	}

	sess->zsk_flag = zsk_flag;
	sess->aes_gcm = aes_gcm;
	sess->is_gmac = 1;
	sess->iv_offset = a_form->iv.offset;
	sess->iv_length = a_form->iv.length;
	sess->mac_len = a_form->digest_length;
	ctx = (void *)((uint8_t *)sess + sizeof(struct cpt_sess_misc)),

	cpt_fc_ciph_set_key(ctx, enc_type, a_form->key.data,
			a_form->key.length, NULL);
	cpt_fc_auth_set_key(ctx, auth_type, NULL, 0, a_form->digest_length);

	return 0;
}

#endif /*_CPT_UCODE_H_ */
