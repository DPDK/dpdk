/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"

static uint8_t zuc_d[32] = {0x44, 0xD7, 0x26, 0xBC, 0x62, 0x6B, 0x13, 0x5E,
			    0x57, 0x89, 0x35, 0xE2, 0x71, 0x35, 0x09, 0xAF,
			    0x4D, 0x78, 0x2F, 0x13, 0x6B, 0xC4, 0x1A, 0xF1,
			    0x5E, 0x26, 0x3C, 0x4D, 0x78, 0x9A, 0x47, 0xAC};

static inline void
cpt_snow3g_key_gen(const uint8_t *ck, uint32_t *keyx)
{
	int i, base;

	for (i = 0; i < 4; i++) {
		base = 4 * i;
		keyx[3 - i] = (ck[base] << 24) | (ck[base + 1] << 16) |
			      (ck[base + 2] << 8) | (ck[base + 3]);
		keyx[3 - i] = plt_cpu_to_be_32(keyx[3 - i]);
	}
}

static inline int
cpt_ciph_aes_key_validate(uint16_t key_len)
{
	switch (key_len) {
	case 16:
	case 24:
	case 32:
		return 0;
	default:
		return -1;
	}
}

static inline int
cpt_ciph_type_set(roc_se_cipher_type type, struct roc_se_ctx *ctx,
		  uint16_t key_len)
{
	int fc_type = 0;

	switch (type) {
	case ROC_SE_PASSTHROUGH:
		fc_type = ROC_SE_FC_GEN;
		break;
	case ROC_SE_DES3_CBC:
	case ROC_SE_DES3_ECB:
		fc_type = ROC_SE_FC_GEN;
		break;
	case ROC_SE_AES_CBC:
	case ROC_SE_AES_ECB:
	case ROC_SE_AES_CFB:
	case ROC_SE_AES_CTR:
	case ROC_SE_AES_GCM:
		if (unlikely(cpt_ciph_aes_key_validate(key_len) != 0))
			return -1;
		fc_type = ROC_SE_FC_GEN;
		break;
	case ROC_SE_CHACHA20:
		fc_type = ROC_SE_FC_GEN;
		break;
	case ROC_SE_AES_XTS:
		key_len = key_len / 2;
		if (unlikely(key_len == 24)) {
			plt_err("Invalid AES key len for XTS");
			return -1;
		}
		if (unlikely(cpt_ciph_aes_key_validate(key_len) != 0))
			return -1;
		fc_type = ROC_SE_FC_GEN;
		break;
	case ROC_SE_ZUC_EEA3:
	case ROC_SE_SNOW3G_UEA2:
		if (unlikely(key_len != 16))
			return -1;
		/* No support for AEAD yet */
		if (unlikely(ctx->hash_type))
			return -1;
		fc_type = ROC_SE_PDCP;
		break;
	case ROC_SE_AES_CTR_EEA2:
		fc_type = ROC_SE_PDCP;
		break;
	case ROC_SE_KASUMI_F8_CBC:
	case ROC_SE_KASUMI_F8_ECB:
		if (unlikely(key_len != 16))
			return -1;
		/* No support for AEAD yet */
		if (unlikely(ctx->hash_type))
			return -1;
		fc_type = ROC_SE_KASUMI;
		break;
	default:
		return -1;
	}

	ctx->fc_type = fc_type;
	return 0;
}

static inline void
cpt_ciph_aes_key_type_set(struct roc_se_context *fctx, uint16_t key_len)
{
	roc_se_aes_type aes_key_type = 0;

	switch (key_len) {
	case 16:
		aes_key_type = ROC_SE_AES_128_BIT;
		break;
	case 24:
		aes_key_type = ROC_SE_AES_192_BIT;
		break;
	case 32:
		aes_key_type = ROC_SE_AES_256_BIT;
		break;
	default:
		/* This should not happen */
		plt_err("Invalid AES key len");
		return;
	}
	fctx->enc.aes_key = aes_key_type;
}

int
roc_se_auth_key_set(struct roc_se_ctx *se_ctx, roc_se_auth_type type,
		    const uint8_t *key, uint16_t key_len, uint16_t mac_len)
{
	struct roc_se_zuc_snow3g_ctx *zs_ctx;
	struct roc_se_kasumi_ctx *k_ctx;
	struct roc_se_context *fctx;

	if (se_ctx == NULL)
		return -1;

	zs_ctx = &se_ctx->se_ctx.zs_ctx;
	k_ctx = &se_ctx->se_ctx.k_ctx;
	fctx = &se_ctx->se_ctx.fctx;

	if ((type >= ROC_SE_ZUC_EIA3) && (type <= ROC_SE_KASUMI_F9_ECB)) {
		uint32_t keyx[4];

		if (key_len != 16)
			return -1;
		/* No support for AEAD yet */
		if (se_ctx->enc_cipher)
			return -1;
		/* For ZUC/SNOW3G/Kasumi */
		switch (type) {
		case ROC_SE_SNOW3G_UIA2:
			se_ctx->pdcp_alg_type = ROC_SE_PDCP_ALG_TYPE_SNOW3G;
			cpt_snow3g_key_gen(key, keyx);
			memcpy(zs_ctx->ci_key, keyx, key_len);
			se_ctx->fc_type = ROC_SE_PDCP;
			se_ctx->zsk_flags = 0x1;
			break;
		case ROC_SE_ZUC_EIA3:
			se_ctx->pdcp_alg_type = ROC_SE_PDCP_ALG_TYPE_ZUC;
			memcpy(zs_ctx->ci_key, key, key_len);
			memcpy(zs_ctx->zuc_const, zuc_d, 32);
			se_ctx->fc_type = ROC_SE_PDCP;
			se_ctx->zsk_flags = 0x1;
			break;
		case ROC_SE_AES_CMAC_EIA2:
			se_ctx->pdcp_alg_type = ROC_SE_PDCP_ALG_TYPE_AES_CTR;
			memcpy(zs_ctx->ci_key, key, key_len);
			se_ctx->fc_type = ROC_SE_PDCP;
			se_ctx->zsk_flags = 0x1;
			break;
		case ROC_SE_KASUMI_F9_ECB:
			/* Kasumi ECB mode */
			se_ctx->k_ecb = 1;
			memcpy(k_ctx->ci_key, key, key_len);
			se_ctx->fc_type = ROC_SE_KASUMI;
			se_ctx->zsk_flags = 0x1;
			break;
		case ROC_SE_KASUMI_F9_CBC:
			memcpy(k_ctx->ci_key, key, key_len);
			se_ctx->fc_type = ROC_SE_KASUMI;
			se_ctx->zsk_flags = 0x1;
			break;
		default:
			return -1;
		}
		se_ctx->mac_len = 4;
		se_ctx->hash_type = type;
		return 0;
	}

	if (!se_ctx->fc_type ||
	    (type && type != ROC_SE_GMAC_TYPE && !se_ctx->enc_cipher))
		se_ctx->fc_type = ROC_SE_HASH_HMAC;

	if (se_ctx->fc_type == ROC_SE_FC_GEN && key_len > 64)
		return -1;

	/* For GMAC auth, cipher must be NULL */
	if (type == ROC_SE_GMAC_TYPE)
		fctx->enc.enc_cipher = 0;

	fctx->enc.hash_type = type;
	se_ctx->hash_type = type;
	fctx->enc.mac_len = mac_len;
	se_ctx->mac_len = mac_len;

	if (key_len) {
		se_ctx->hmac = 1;

		se_ctx->auth_key = plt_zmalloc(key_len, 8);
		if (se_ctx->auth_key == NULL)
			return -1;

		memcpy(se_ctx->auth_key, key, key_len);
		se_ctx->auth_key_len = key_len;
		memset(fctx->hmac.ipad, 0, sizeof(fctx->hmac.ipad));
		memset(fctx->hmac.opad, 0, sizeof(fctx->hmac.opad));

		if (key_len <= 64)
			memcpy(fctx->hmac.opad, key, key_len);
		fctx->enc.auth_input_type = 1;
	}
	return 0;
}

int
roc_se_ciph_key_set(struct roc_se_ctx *se_ctx, roc_se_cipher_type type,
		    const uint8_t *key, uint16_t key_len, uint8_t *salt)
{
	struct roc_se_context *fctx = &se_ctx->se_ctx.fctx;
	struct roc_se_zuc_snow3g_ctx *zs_ctx;
	uint32_t keyx[4];
	int ret;

	/* For AES-GCM, salt is taken from ctx even if IV source
	 * is from DPTR
	 */
	if ((salt != NULL) && (type == ROC_SE_AES_GCM)) {
		memcpy(fctx->enc.encr_iv, salt, 4);
		/* Assuming it was just salt update
		 * and nothing else
		 */
		if (key == NULL)
			return 0;
	}

	ret = cpt_ciph_type_set(type, se_ctx, key_len);
	if (unlikely(ret))
		return -1;

	if (se_ctx->fc_type == ROC_SE_FC_GEN) {
		/*
		 * We need to always say IV is from DPTR as user can
		 * sometimes iverride IV per operation.
		 */
		fctx->enc.iv_source = ROC_SE_FROM_DPTR;

		if (se_ctx->auth_key_len > 64)
			return -1;
	}

	switch (type) {
	case ROC_SE_PASSTHROUGH:
		se_ctx->enc_cipher = 0;
		fctx->enc.enc_cipher = 0;
		goto success;
	case ROC_SE_DES3_CBC:
		/* CPT performs DES using 3DES with the 8B DES-key
		 * replicated 2 more times to match the 24B 3DES-key.
		 * Eg. If org. key is "0x0a 0x0b", then new key is
		 * "0x0a 0x0b 0x0a 0x0b 0x0a 0x0b"
		 */
		if (key_len == 8) {
			/* Skipping the first 8B as it will be copied
			 * in the regular code flow
			 */
			memcpy(fctx->enc.encr_key + key_len, key, key_len);
			memcpy(fctx->enc.encr_key + 2 * key_len, key, key_len);
		}
		break;
	case ROC_SE_DES3_ECB:
		/* For DES3_ECB IV need to be from CTX. */
		fctx->enc.iv_source = ROC_SE_FROM_CTX;
		break;
	case ROC_SE_AES_CBC:
	case ROC_SE_AES_ECB:
	case ROC_SE_AES_CFB:
	case ROC_SE_AES_CTR:
	case ROC_SE_CHACHA20:
		cpt_ciph_aes_key_type_set(fctx, key_len);
		break;
	case ROC_SE_AES_GCM:
		cpt_ciph_aes_key_type_set(fctx, key_len);
		break;
	case ROC_SE_AES_XTS:
		key_len = key_len / 2;
		cpt_ciph_aes_key_type_set(fctx, key_len);

		/* Copy key2 for XTS into ipad */
		memset(fctx->hmac.ipad, 0, sizeof(fctx->hmac.ipad));
		memcpy(fctx->hmac.ipad, &key[key_len], key_len);
		break;
	case ROC_SE_SNOW3G_UEA2:
		se_ctx->pdcp_alg_type = ROC_SE_PDCP_ALG_TYPE_SNOW3G;
		cpt_snow3g_key_gen(key, keyx);
		memcpy(se_ctx->se_ctx.zs_ctx.ci_key, keyx, key_len);
		se_ctx->zsk_flags = 0;
		goto success;
	case ROC_SE_ZUC_EEA3:
		zs_ctx = &se_ctx->se_ctx.zs_ctx;
		se_ctx->pdcp_alg_type = ROC_SE_PDCP_ALG_TYPE_ZUC;
		memcpy(zs_ctx->ci_key, key, key_len);
		memcpy(zs_ctx->zuc_const, zuc_d, 32);
		se_ctx->zsk_flags = 0;
		goto success;
	case ROC_SE_AES_CTR_EEA2:
		se_ctx->pdcp_alg_type = ROC_SE_PDCP_ALG_TYPE_AES_CTR;
		memcpy(se_ctx->se_ctx.zs_ctx.ci_key, key, key_len);
		se_ctx->zsk_flags = 0;
		goto success;
	case ROC_SE_KASUMI_F8_ECB:
		se_ctx->k_ecb = 1;
		memcpy(se_ctx->se_ctx.k_ctx.ci_key, key, key_len);
		se_ctx->zsk_flags = 0;
		goto success;
	case ROC_SE_KASUMI_F8_CBC:
		memcpy(se_ctx->se_ctx.k_ctx.ci_key, key, key_len);
		se_ctx->zsk_flags = 0;
		goto success;
	default:
		return -1;
	}

	/* Only for ROC_SE_FC_GEN case */

	/* For GMAC auth, cipher must be NULL */
	if (se_ctx->hash_type != ROC_SE_GMAC_TYPE)
		fctx->enc.enc_cipher = type;

	memcpy(fctx->enc.encr_key, key, key_len);

success:
	se_ctx->enc_cipher = type;

	return 0;
}
