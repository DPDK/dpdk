/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"

static uint8_t zuc_key128[32] = {
	0x44, 0xD7, 0x26, 0xBC, 0x62, 0x6B, 0x13, 0x5E, 0x57, 0x89, 0x35,
	0xE2, 0x71, 0x35, 0x09, 0xAF, 0x4D, 0x78, 0x2F, 0x13, 0x6B, 0xC4,
	0x1A, 0xF1, 0x5E, 0x26, 0x3C, 0x4D, 0x78, 0x9A, 0x47, 0xAC};

static uint8_t zuc_key256[16] = {0x22, 0x2f, 0x24, 0x2a, 0x6d, 0x40,
				 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
				 0x40, 0x52, 0x10, 0x30};

static uint8_t zuc_key256_mac4[16] = {0x22, 0x2f, 0x25, 0x2a, 0x6d, 0x40,
				      0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
				      0x40, 0x52, 0x10, 0x30};

static uint8_t zuc_key256_mac8[16] = {0x23, 0x2f, 0x24, 0x2a, 0x6d, 0x40,
				      0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
				      0x40, 0x52, 0x10, 0x30};

static uint8_t zuc_key256_mac16[16] = {0x23, 0x2f, 0x25, 0x2a, 0x6d, 0x40,
				       0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
				       0x40, 0x52, 0x10, 0x30};

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
cpt_ciph_type_set(roc_se_cipher_type type, struct roc_se_ctx *ctx, uint16_t key_len)
{
	bool chained_op = ctx->ciph_then_auth || ctx->auth_then_ciph;
	int fc_type = 0;

	switch (type) {
	case ROC_SE_PASSTHROUGH:
		fc_type = ROC_SE_FC_GEN;
		break;
	case ROC_SE_DES3_CBC:
	case ROC_SE_DES3_ECB:
	case ROC_SE_DES_DOCSISBPI:
		fc_type = ROC_SE_FC_GEN;
		break;
	case ROC_SE_AES_CBC:
	case ROC_SE_AES_ECB:
	case ROC_SE_AES_CFB:
	case ROC_SE_AES_CTR:
	case ROC_SE_AES_GCM:
	case ROC_SE_AES_CCM:
	case ROC_SE_AES_DOCSISBPI:
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
		if (chained_op) {
			if (unlikely(key_len != 16))
				return -1;
			fc_type = ROC_SE_PDCP_CHAIN;
		} else {
			fc_type = ROC_SE_PDCP;
		}
		break;
	case ROC_SE_SNOW3G_UEA2:
		if (unlikely(key_len != 16))
			return -1;
		if (chained_op)
			fc_type = ROC_SE_PDCP_CHAIN;
		else
			fc_type = ROC_SE_PDCP;
		break;
	case ROC_SE_AES_CTR_EEA2:
		if (chained_op)
			fc_type = ROC_SE_PDCP_CHAIN;
		else
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

static void
cpt_hmac_opad_ipad_gen(roc_se_auth_type auth_type, const uint8_t *key, uint16_t length,
		       struct roc_se_hmac_context *hmac)
{
	uint8_t opad[128] = {[0 ... 127] = 0x5c};
	uint8_t ipad[128] = {[0 ... 127] = 0x36};
	uint32_t i;

	/* HMAC OPAD and IPAD */
	for (i = 0; i < 128 && i < length; i++) {
		opad[i] = opad[i] ^ key[i];
		ipad[i] = ipad[i] ^ key[i];
	}

	/* Precompute hash of HMAC OPAD and IPAD to avoid
	 * per packet computation
	 */
	switch (auth_type) {
	case ROC_SE_MD5_TYPE:
		roc_hash_md5_gen(opad, (uint32_t *)hmac->opad);
		roc_hash_md5_gen(ipad, (uint32_t *)hmac->ipad);
		break;
	case ROC_SE_SHA1_TYPE:
		roc_hash_sha1_gen(opad, (uint32_t *)hmac->opad);
		roc_hash_sha1_gen(ipad, (uint32_t *)hmac->ipad);
		break;
	case ROC_SE_SHA2_SHA224:
		roc_hash_sha256_gen(opad, (uint32_t *)hmac->opad, 224);
		roc_hash_sha256_gen(ipad, (uint32_t *)hmac->ipad, 224);
		break;
	case ROC_SE_SHA2_SHA256:
		roc_hash_sha256_gen(opad, (uint32_t *)hmac->opad, 256);
		roc_hash_sha256_gen(ipad, (uint32_t *)hmac->ipad, 256);
		break;
	case ROC_SE_SHA2_SHA384:
		roc_hash_sha512_gen(opad, (uint64_t *)hmac->opad, 384);
		roc_hash_sha512_gen(ipad, (uint64_t *)hmac->ipad, 384);
		break;
	case ROC_SE_SHA2_SHA512:
		roc_hash_sha512_gen(opad, (uint64_t *)hmac->opad, 512);
		roc_hash_sha512_gen(ipad, (uint64_t *)hmac->ipad, 512);
		break;
	default:
		break;
	}
}

static int
cpt_pdcp_key_type_set(struct roc_se_zuc_snow3g_ctx *zs_ctx, uint16_t key_len)
{
	roc_se_aes_type key_type = 0;

	if (roc_model_is_cn9k()) {
		if (key_len != 16) {
			plt_err("Only key len 16 is supported on cn9k");
			return -ENOTSUP;
		}
	}

	switch (key_len) {
	case 16:
		key_type = ROC_SE_AES_128_BIT;
		break;
	case 32:
		key_type = ROC_SE_AES_256_BIT;
		break;
	default:
		plt_err("Invalid AES key len");
		return -ENOTSUP;
	}
	zs_ctx->zuc.otk_ctx.w0.s.key_len = key_type;
	return 0;
}

static int
cpt_pdcp_chain_key_type_get(uint16_t key_len)
{
	roc_se_aes_type key_type;

	switch (key_len) {
	case 16:
		key_type = ROC_SE_AES_128_BIT;
		break;
	case 24:
		key_type = ROC_SE_AES_192_BIT;
		break;
	case 32:
		key_type = ROC_SE_AES_256_BIT;
		break;
	default:
		plt_err("Invalid key len");
		return -ENOTSUP;
	}

	return key_type;
}

static int
cpt_pdcp_mac_len_set(struct roc_se_zuc_snow3g_ctx *zs_ctx, uint16_t mac_len)
{
	roc_se_pdcp_mac_len_type mac_type = 0;

	if (roc_model_is_cn9k()) {
		if (mac_len != 4) {
			plt_err("Only mac len 4 is supported on cn9k");
			return -ENOTSUP;
		}
	}

	switch (mac_len) {
	case 4:
		mac_type = ROC_SE_PDCP_MAC_LEN_32_BIT;
		break;
	case 8:
		mac_type = ROC_SE_PDCP_MAC_LEN_64_BIT;
		break;
	case 16:
		mac_type = ROC_SE_PDCP_MAC_LEN_128_BIT;
		break;
	default:
		plt_err("Invalid ZUC MAC len");
		return -ENOTSUP;
	}
	zs_ctx->zuc.otk_ctx.w0.s.mac_len = mac_type;
	return 0;
}

static void
cpt_zuc_const_update(uint8_t *zuc_const, int key_len, int mac_len)
{
	if (key_len == 16) {
		memcpy(zuc_const, zuc_key128, 32);
	} else if (key_len == 32) {
		switch (mac_len) {
		case 4:
			memcpy(zuc_const, zuc_key256_mac4, 16);
			break;
		case 8:
			memcpy(zuc_const, zuc_key256_mac8, 16);
			break;
		case 16:
			memcpy(zuc_const, zuc_key256_mac16, 16);
			break;
		default:
			plt_err("Unsupported mac len");
		}
	}
}

int
roc_se_auth_key_set(struct roc_se_ctx *se_ctx, roc_se_auth_type type,
		    const uint8_t *key, uint16_t key_len, uint16_t mac_len)
{
	struct roc_se_zuc_snow3g_chain_ctx *zs_ch_ctx;
	struct roc_se_zuc_snow3g_ctx *zs_ctx;
	struct roc_se_kasumi_ctx *k_ctx;
	struct roc_se_context *fctx;
	uint8_t opcode_minor;
	uint8_t pdcp_alg;
	bool chained_op;
	int ret;

	if (se_ctx == NULL)
		return -1;

	zs_ctx = &se_ctx->se_ctx.zs_ctx;
	zs_ch_ctx = &se_ctx->se_ctx.zs_ch_ctx;
	k_ctx = &se_ctx->se_ctx.k_ctx;
	fctx = &se_ctx->se_ctx.fctx;

	chained_op = se_ctx->ciph_then_auth || se_ctx->auth_then_ciph;

	if ((type >= ROC_SE_ZUC_EIA3) && (type <= ROC_SE_KASUMI_F9_ECB)) {
		uint8_t *zuc_const;
		uint32_t keyx[4];
		uint8_t *ci_key;

		if (!key_len)
			return -1;

		if (se_ctx->fc_type == ROC_SE_FC_GEN) {
			plt_err("Cipher and Auth algorithm combination is not supported");
			return -1;
		}

		if (roc_model_is_cn9k()) {
			ci_key = zs_ctx->zuc.onk_ctx.ci_key;
			zuc_const = zs_ctx->zuc.onk_ctx.zuc_const;
		} else {
			ci_key = zs_ctx->zuc.otk_ctx.ci_key;
			zuc_const = zs_ctx->zuc.otk_ctx.zuc_const;
		}

		/* For ZUC/SNOW3G/Kasumi */
		switch (type) {
		case ROC_SE_SNOW3G_UIA2:
			if (chained_op) {
				struct roc_se_onk_zuc_chain_ctx *ctx =
					&zs_ch_ctx->zuc.onk_ctx;
				zs_ch_ctx->zuc.onk_ctx.w0.s.state_conf =
					ROC_SE_PDCP_CHAIN_CTX_KEY_IV;
				ctx->w0.s.auth_type =
					ROC_SE_PDCP_CHAIN_ALG_TYPE_SNOW3G;
				ctx->w0.s.mac_len = mac_len;
				ctx->w0.s.auth_key_len = key_len;
				se_ctx->fc_type = ROC_SE_PDCP_CHAIN;
				cpt_snow3g_key_gen(key, keyx);
				memcpy(ctx->st.auth_key, keyx, key_len);
			} else {
				zs_ctx->zuc.otk_ctx.w0.s.alg_type =
					ROC_SE_PDCP_ALG_TYPE_SNOW3G;
				zs_ctx->zuc.otk_ctx.w0.s.mac_len =
					ROC_SE_PDCP_MAC_LEN_32_BIT;
				cpt_snow3g_key_gen(key, keyx);
				memcpy(ci_key, keyx, key_len);
				se_ctx->fc_type = ROC_SE_PDCP;
			}
			se_ctx->pdcp_auth_alg = ROC_SE_PDCP_ALG_TYPE_SNOW3G;
			se_ctx->zsk_flags = 0x1;
			break;
		case ROC_SE_ZUC_EIA3:
			if (chained_op) {
				struct roc_se_onk_zuc_chain_ctx *ctx =
					&zs_ch_ctx->zuc.onk_ctx;
				ctx->w0.s.state_conf =
					ROC_SE_PDCP_CHAIN_CTX_KEY_IV;
				ctx->w0.s.auth_type =
					ROC_SE_PDCP_CHAIN_ALG_TYPE_ZUC;
				ctx->w0.s.mac_len = mac_len;
				ctx->w0.s.auth_key_len = key_len;
				memcpy(ctx->st.auth_key, key, key_len);
				cpt_zuc_const_update(ctx->st.auth_zuc_const,
						     key_len, mac_len);
				se_ctx->fc_type = ROC_SE_PDCP_CHAIN;
			} else {
				zs_ctx->zuc.otk_ctx.w0.s.alg_type =
					ROC_SE_PDCP_ALG_TYPE_ZUC;
				ret = cpt_pdcp_key_type_set(zs_ctx, key_len);
				if (ret)
					return ret;
				ret = cpt_pdcp_mac_len_set(zs_ctx, mac_len);
				if (ret)
					return ret;
				memcpy(ci_key, key, key_len);
				if (key_len == 32)
					roc_se_zuc_bytes_swap(ci_key, key_len);
				cpt_zuc_const_update(zuc_const, key_len,
						     mac_len);
				se_ctx->fc_type = ROC_SE_PDCP;
			}
			se_ctx->pdcp_auth_alg = ROC_SE_PDCP_ALG_TYPE_ZUC;
			se_ctx->zsk_flags = 0x1;
			break;
		case ROC_SE_AES_CMAC_EIA2:
			if (chained_op) {
				struct roc_se_onk_zuc_chain_ctx *ctx =
					&zs_ch_ctx->zuc.onk_ctx;
				int key_type;
				key_type = cpt_pdcp_chain_key_type_get(key_len);
				if (key_type < 0)
					return key_type;
				ctx->w0.s.auth_key_len = key_type;
				ctx->w0.s.state_conf =
					ROC_SE_PDCP_CHAIN_CTX_KEY_IV;
				ctx->w0.s.auth_type =
					ROC_SE_PDCP_ALG_TYPE_AES_CTR;
				ctx->w0.s.mac_len = mac_len;
				memcpy(ctx->st.auth_key, key, key_len);
				se_ctx->fc_type = ROC_SE_PDCP_CHAIN;
			} else {
				zs_ctx->zuc.otk_ctx.w0.s.alg_type =
					ROC_SE_PDCP_ALG_TYPE_AES_CTR;
				zs_ctx->zuc.otk_ctx.w0.s.mac_len =
					ROC_SE_PDCP_MAC_LEN_32_BIT;
				memcpy(ci_key, key, key_len);
				se_ctx->fc_type = ROC_SE_PDCP;
			}
			se_ctx->pdcp_auth_alg = ROC_SE_PDCP_ALG_TYPE_AES_CMAC;
			se_ctx->eia2 = 1;
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

		if ((se_ctx->fc_type == ROC_SE_PDCP_CHAIN) && (mac_len != 4)) {
			plt_err("Only digest length of 4 is supported with PDCP chain");
			return -1;
		}

		se_ctx->mac_len = mac_len;
		se_ctx->hash_type = type;
		pdcp_alg = zs_ctx->zuc.otk_ctx.w0.s.alg_type;
		if (chained_op)
			opcode_minor = se_ctx->ciph_then_auth ? 2 : 3;
		else if (roc_model_is_cn9k())
			opcode_minor = ((1 << 7) | (pdcp_alg << 5) | 1);
		else
			opcode_minor = ((1 << 4) | 1);

		se_ctx->template_w4.s.opcode_minor = opcode_minor;
		return 0;
	}

	if (!se_ctx->fc_type || (type && type != ROC_SE_GMAC_TYPE && !se_ctx->enc_cipher))
		se_ctx->fc_type = ROC_SE_HASH_HMAC;

	if (se_ctx->fc_type == ROC_SE_FC_GEN && key_len > 64) {
		plt_err("Maximum auth key length supported is 64");
		return -1;
	}

	/* For GMAC auth, cipher must be NULL */
	if (type == ROC_SE_GMAC_TYPE) {
		fctx->enc.enc_cipher = 0;
		se_ctx->template_w4.s.opcode_minor = BIT(5);
	}

	fctx->enc.hash_type = type;
	se_ctx->hash_type = type;
	fctx->enc.mac_len = mac_len;
	se_ctx->mac_len = mac_len;

	if (key_len) {
		/*
		 * Chained operation (FC opcode) requires precomputed ipad and opad hashes, but for
		 * auth only (HMAC opcode) this is not required
		 */
		if (chained_op) {
			memset(fctx->hmac.ipad, 0, sizeof(fctx->hmac.ipad));
			memset(fctx->hmac.opad, 0, sizeof(fctx->hmac.opad));
			cpt_hmac_opad_ipad_gen(type, key, key_len, &fctx->hmac);
			fctx->enc.auth_input_type = 0;
		} else {
			se_ctx->hmac = 1;

			se_ctx->auth_key = plt_zmalloc(key_len, 8);
			if (se_ctx->auth_key == NULL)
				return -1;

			memcpy(se_ctx->auth_key, key, key_len);
			se_ctx->auth_key_len = key_len;
		}
	}
	return 0;
}

int
roc_se_ciph_key_set(struct roc_se_ctx *se_ctx, roc_se_cipher_type type, const uint8_t *key,
		    uint16_t key_len)
{
	bool chained_op = se_ctx->ciph_then_auth || se_ctx->auth_then_ciph;
	struct roc_se_zuc_snow3g_ctx *zs_ctx = &se_ctx->se_ctx.zs_ctx;
	struct roc_se_context *fctx = &se_ctx->se_ctx.fctx;
	struct roc_se_zuc_snow3g_chain_ctx *zs_ch_ctx;
	uint8_t opcode_minor = 0;
	uint8_t *zuc_const;
	uint32_t keyx[4];
	uint8_t *ci_key;
	int i, ret;

	zs_ch_ctx = &se_ctx->se_ctx.zs_ch_ctx;

	if (roc_model_is_cn9k()) {
		ci_key = zs_ctx->zuc.onk_ctx.ci_key;
		zuc_const = zs_ctx->zuc.onk_ctx.zuc_const;
	} else {
		ci_key = zs_ctx->zuc.otk_ctx.ci_key;
		zuc_const = zs_ctx->zuc.otk_ctx.zuc_const;
	}

	if ((type == ROC_SE_AES_GCM) || (type == ROC_SE_AES_CCM))
		se_ctx->template_w4.s.opcode_minor = BIT(5);

	ret = cpt_ciph_type_set(type, se_ctx, key_len);
	if (unlikely(ret))
		return -1;

	if (se_ctx->fc_type == ROC_SE_FC_GEN) {
		/*
		 * We need to always say IV is from DPTR as user can
		 * sometimes override IV per operation.
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
	case ROC_SE_AES_CCM:
		cpt_ciph_aes_key_type_set(fctx, key_len);
		break;
	case ROC_SE_AES_XTS:
		key_len = key_len / 2;
		cpt_ciph_aes_key_type_set(fctx, key_len);

		/* Copy key2 for XTS into ipad */
		memset(fctx->hmac.ipad, 0, sizeof(fctx->hmac.ipad));
		memcpy(fctx->hmac.ipad, &key[key_len], key_len);
		break;
	case ROC_SE_AES_DOCSISBPI:
		/*
		 * DOCSIS uses the combination of AES-CBC and residual termination blocks that are
		 * less than 128. Pass it as regular AES-CBC cipher to CPT, but keep type in
		 * se_ctx as AES_DOCSISBPI to skip block size checks in instruction preparation.
		 */
		cpt_ciph_aes_key_type_set(fctx, key_len);
		fctx->enc.enc_cipher = ROC_SE_AES_CBC;
		memcpy(fctx->enc.encr_key, key, key_len);
		goto success;
	case ROC_SE_DES_DOCSISBPI:
		/* See case ROC_SE_DES3_CBC: for explanation */
		for (i = 0; i < 3; i++)
			memcpy(fctx->enc.encr_key + key_len * i, key, key_len);
		/*
		 * DOCSIS uses DES-CBC mode with special handling of residual termination blocks
		 * that are less than 64 bits. Pass it as regular DES-CBC, but keep type in
		 * se_ctx as DES_DOCSISBPI to skip block size checks in instruction preparation.
		 */
		fctx->enc.enc_cipher = ROC_SE_DES3_CBC;
		goto success;
	case ROC_SE_SNOW3G_UEA2:
		if (chained_op == true) {
			struct roc_se_onk_zuc_chain_ctx *ctx =
				&zs_ch_ctx->zuc.onk_ctx;
			zs_ch_ctx->zuc.onk_ctx.w0.s.state_conf =
				ROC_SE_PDCP_CHAIN_CTX_KEY_IV;
			zs_ch_ctx->zuc.onk_ctx.w0.s.cipher_type =
				ROC_SE_PDCP_CHAIN_ALG_TYPE_SNOW3G;
			zs_ch_ctx->zuc.onk_ctx.w0.s.ci_key_len = key_len;
			cpt_snow3g_key_gen(key, keyx);
			memcpy(ctx->st.ci_key, keyx, key_len);
		} else {
			zs_ctx->zuc.otk_ctx.w0.s.key_len = ROC_SE_AES_128_BIT;
			zs_ctx->zuc.otk_ctx.w0.s.alg_type =
				ROC_SE_PDCP_ALG_TYPE_SNOW3G;
			cpt_snow3g_key_gen(key, keyx);
			memcpy(ci_key, keyx, key_len);
		}
		se_ctx->pdcp_ci_alg = ROC_SE_PDCP_ALG_TYPE_SNOW3G;
		se_ctx->zsk_flags = 0;
		goto success;
	case ROC_SE_ZUC_EEA3:
		if (chained_op == true) {
			struct roc_se_onk_zuc_chain_ctx *ctx =
				&zs_ch_ctx->zuc.onk_ctx;
			zs_ch_ctx->zuc.onk_ctx.w0.s.state_conf =
				ROC_SE_PDCP_CHAIN_CTX_KEY_IV;
			zs_ch_ctx->zuc.onk_ctx.w0.s.cipher_type =
				ROC_SE_PDCP_CHAIN_ALG_TYPE_ZUC;
			memcpy(ctx->st.ci_key, key, key_len);
			memcpy(ctx->st.ci_zuc_const, zuc_key128, 32);
			zs_ch_ctx->zuc.onk_ctx.w0.s.ci_key_len = key_len;
		} else {
			ret = cpt_pdcp_key_type_set(zs_ctx, key_len);
			if (ret)
				return ret;
			zs_ctx->zuc.otk_ctx.w0.s.alg_type =
				ROC_SE_PDCP_ALG_TYPE_ZUC;
			memcpy(ci_key, key, key_len);
			if (key_len == 32) {
				roc_se_zuc_bytes_swap(ci_key, key_len);
				memcpy(zuc_const, zuc_key256, 16);
			} else
				memcpy(zuc_const, zuc_key128, 32);
		}

		se_ctx->pdcp_ci_alg = ROC_SE_PDCP_ALG_TYPE_ZUC;
		se_ctx->zsk_flags = 0;
		goto success;
	case ROC_SE_AES_CTR_EEA2:
		if (chained_op == true) {
			struct roc_se_onk_zuc_chain_ctx *ctx =
				&zs_ch_ctx->zuc.onk_ctx;
			int key_type;
			key_type = cpt_pdcp_chain_key_type_get(key_len);
			if (key_type < 0)
				return key_type;
			ctx->w0.s.ci_key_len = key_type;
			ctx->w0.s.state_conf = ROC_SE_PDCP_CHAIN_CTX_KEY_IV;
			ctx->w0.s.cipher_type = ROC_SE_PDCP_ALG_TYPE_AES_CTR;
			memcpy(ctx->st.ci_key, key, key_len);
		} else {
			zs_ctx->zuc.otk_ctx.w0.s.key_len = ROC_SE_AES_128_BIT;
			zs_ctx->zuc.otk_ctx.w0.s.alg_type =
				ROC_SE_PDCP_ALG_TYPE_AES_CTR;
			memcpy(ci_key, key, key_len);
		}
		se_ctx->pdcp_ci_alg = ROC_SE_PDCP_ALG_TYPE_AES_CTR;
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
	if (se_ctx->fc_type == ROC_SE_PDCP_CHAIN) {
		se_ctx->template_w4.s.opcode_minor = se_ctx->ciph_then_auth ? 2 : 3;
	} else if (se_ctx->fc_type == ROC_SE_PDCP) {
		if (roc_model_is_cn9k())
			opcode_minor =
				((1 << 7) | (se_ctx->pdcp_ci_alg << 5) | (se_ctx->zsk_flags & 0x7));
		else
			opcode_minor = ((1 << 4));
		se_ctx->template_w4.s.opcode_minor = opcode_minor;
	}
	return 0;
}

void
roc_se_ctx_swap(struct roc_se_ctx *se_ctx)
{
	struct roc_se_zuc_snow3g_ctx *zs_ctx = &se_ctx->se_ctx.zs_ctx;

	if (roc_model_is_cn9k())
		return;

	if (se_ctx->fc_type == ROC_SE_PDCP_CHAIN)
		return;

	zs_ctx->zuc.otk_ctx.w0.u64 = htobe64(zs_ctx->zuc.otk_ctx.w0.u64);
}

void
roc_se_ctx_init(struct roc_se_ctx *roc_se_ctx)
{
	struct se_ctx_s *ctx = &roc_se_ctx->se_ctx;
	uint64_t ctx_len, *uc_ctx;
	uint8_t i;

	switch (roc_se_ctx->fc_type) {
	case ROC_SE_FC_GEN:
		ctx_len = sizeof(struct roc_se_context);
		break;
	case ROC_SE_PDCP:
		ctx_len = sizeof(struct roc_se_zuc_snow3g_ctx);
		break;
	case ROC_SE_KASUMI:
		ctx_len = sizeof(struct roc_se_kasumi_ctx);
		break;
	case ROC_SE_PDCP_CHAIN:
		ctx_len = sizeof(struct roc_se_zuc_snow3g_chain_ctx);
		break;
	case ROC_SE_SM:
		ctx_len = sizeof(struct roc_se_sm_context);
		break;
	default:
		ctx_len = 0;
	}

	ctx_len = PLT_ALIGN_CEIL(ctx_len, 8);

	/* Skip w0 for swap */
	uc_ctx = PLT_PTR_ADD(ctx, sizeof(ctx->w0));
	for (i = 0; i < (ctx_len / 8); i++)
		uc_ctx[i] = plt_cpu_to_be_64(((uint64_t *)uc_ctx)[i]);

	/* Include w0 */
	ctx_len += sizeof(ctx->w0);
	ctx_len = PLT_ALIGN_CEIL(ctx_len, 8);

	ctx->w0.s.aop_valid = 1;
	ctx->w0.s.ctx_hdr_size = 0;

	ctx->w0.s.ctx_size = PLT_ALIGN_FLOOR(ctx_len, 128);
	if (ctx->w0.s.ctx_size == 0)
		ctx->w0.s.ctx_size = 1;

	ctx->w0.s.ctx_push_size = ctx_len / 8;
	if (ctx->w0.s.ctx_push_size > 32)
		ctx->w0.s.ctx_push_size = 32;
}
