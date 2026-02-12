/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2021 Intel Corporation
 */

#include <unistd.h>

#include "pmd_aesni_mb_priv.h"

RTE_DEFINE_PER_LCORE(pid_t, pid);

uint8_t pmd_driver_id_aesni_mb;

struct aesni_mb_op_buf_data {
	struct rte_mbuf *m;
	uint32_t offset;
};

static inline int
is_aead_algo(IMB_HASH_ALG hash_alg, IMB_CIPHER_MODE cipher_mode)
{
	return (hash_alg == IMB_AUTH_CHACHA20_POLY1305 ||
		hash_alg == IMB_AUTH_AES_CCM ||
		cipher_mode == IMB_CIPHER_GCM
#if IMB_VERSION(1, 5, 0) < IMB_VERSION_NUM
		|| cipher_mode == IMB_CIPHER_SM4_GCM
#endif
		);
}

/** Set session authentication parameters */
static int
aesni_mb_set_session_auth_parameters(IMB_MGR *mb_mgr,
		struct aesni_mb_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	uint8_t hashed_key[HMAC_MAX_BLOCK_SIZE] = { 0 };
	uint32_t auth_precompute = 1;

	if (xform == NULL) {
		sess->template_job.hash_alg = IMB_AUTH_NULL;
		return 0;
	}

	if (xform->type != RTE_CRYPTO_SYM_XFORM_AUTH) {
		IPSEC_MB_LOG(ERR, "Crypto xform struct not of type auth");
		return -1;
	}

	/* Set IV parameters */
	sess->auth_iv.offset = xform->auth.iv.offset;

	/* Set the request digest size */
	sess->auth.req_digest_len = xform->auth.digest_length;

	/* Select auth generate/verify */
	sess->auth.operation = xform->auth.op;

	/* Set Authentication Parameters */
	if (xform->auth.algo == RTE_CRYPTO_AUTH_NULL) {
		sess->template_job.hash_alg = IMB_AUTH_NULL;
		sess->template_job.auth_tag_output_len_in_bytes = 0;
		return 0;
	}

	if (xform->auth.algo == RTE_CRYPTO_AUTH_AES_XCBC_MAC) {
		sess->template_job.hash_alg = IMB_AUTH_AES_XCBC;

		uint16_t xcbc_mac_digest_len =
			get_truncated_digest_byte_length(IMB_AUTH_AES_XCBC);
		if (sess->auth.req_digest_len != xcbc_mac_digest_len) {
			IPSEC_MB_LOG(ERR, "Invalid digest size");
			return -EINVAL;
		}
		sess->template_job.auth_tag_output_len_in_bytes = sess->auth.req_digest_len;

		IMB_AES_XCBC_KEYEXP(mb_mgr, xform->auth.key.data,
				sess->auth.xcbc.k1_expanded,
				sess->auth.xcbc.k2, sess->auth.xcbc.k3);
		sess->template_job.u.XCBC._k1_expanded = sess->auth.xcbc.k1_expanded;
		sess->template_job.u.XCBC._k2 = sess->auth.xcbc.k2;
		sess->template_job.u.XCBC._k3 = sess->auth.xcbc.k3;
		return 0;
	}

	if (xform->auth.algo == RTE_CRYPTO_AUTH_AES_CMAC) {
		uint32_t dust[4*15];

		sess->template_job.hash_alg = IMB_AUTH_AES_CMAC;

		uint16_t cmac_digest_len =
				get_digest_byte_length(IMB_AUTH_AES_CMAC);

		if (sess->auth.req_digest_len > cmac_digest_len) {
			IPSEC_MB_LOG(ERR, "Invalid digest size");
			return -EINVAL;
		}
		/*
		 * Multi-buffer lib supports digest sizes from 4 to 16 bytes
		 * in version 0.50 and sizes of 12 and 16 bytes,
		 * in version 0.49.
		 * If size requested is different, generate the full digest
		 * (16 bytes) in a temporary location and then memcpy
		 * the requested number of bytes.
		 */
		if (sess->auth.req_digest_len < 4)
			sess->template_job.auth_tag_output_len_in_bytes = cmac_digest_len;
		else
			sess->template_job.auth_tag_output_len_in_bytes = sess->auth.req_digest_len;

		IMB_AES_KEYEXP_128(mb_mgr, xform->auth.key.data,
				sess->auth.cmac.expkey, dust);
		IMB_AES_CMAC_SUBKEY_GEN_128(mb_mgr, sess->auth.cmac.expkey,
				sess->auth.cmac.skey1, sess->auth.cmac.skey2);
		sess->template_job.u.CMAC._key_expanded = sess->auth.cmac.expkey;
		sess->template_job.u.CMAC._skey1 = sess->auth.cmac.skey1;
		sess->template_job.u.CMAC._skey2 = sess->auth.cmac.skey2;
		return 0;
	}

	if (xform->auth.algo == RTE_CRYPTO_AUTH_AES_GMAC) {
		if (xform->auth.op == RTE_CRYPTO_AUTH_OP_GENERATE) {
			sess->template_job.cipher_direction = IMB_DIR_ENCRYPT;
			sess->template_job.chain_order = IMB_ORDER_CIPHER_HASH;
		} else
			sess->template_job.cipher_direction = IMB_DIR_DECRYPT;

		if (sess->auth.req_digest_len >
			get_digest_byte_length(IMB_AUTH_AES_GMAC)) {
			IPSEC_MB_LOG(ERR, "Invalid digest size");
			return -EINVAL;
		}
		sess->template_job.auth_tag_output_len_in_bytes = sess->auth.req_digest_len;
		sess->template_job.u.GMAC.iv_len_in_bytes = xform->auth.iv.length;
		sess->iv.offset = xform->auth.iv.offset;

		switch (xform->auth.key.length) {
		case IMB_KEY_128_BYTES:
			sess->template_job.hash_alg = IMB_AUTH_AES_GMAC_128;
			IMB_AES128_GCM_PRE(mb_mgr, xform->auth.key.data,
				&sess->cipher.gcm_key);
			sess->template_job.key_len_in_bytes = IMB_KEY_128_BYTES;
			break;
		case IMB_KEY_192_BYTES:
			sess->template_job.hash_alg = IMB_AUTH_AES_GMAC_192;
			IMB_AES192_GCM_PRE(mb_mgr, xform->auth.key.data,
				&sess->cipher.gcm_key);
			sess->template_job.key_len_in_bytes = IMB_KEY_192_BYTES;
			break;
		case IMB_KEY_256_BYTES:
			sess->template_job.hash_alg = IMB_AUTH_AES_GMAC_256;
			IMB_AES256_GCM_PRE(mb_mgr, xform->auth.key.data,
				&sess->cipher.gcm_key);
			sess->template_job.key_len_in_bytes = IMB_KEY_256_BYTES;
			break;
		default:
			IPSEC_MB_LOG(ERR, "Invalid authentication key length");
			return -EINVAL;
		}
		sess->template_job.u.GMAC._key = &sess->cipher.gcm_key;

		return 0;
	}

	if (xform->auth.algo == RTE_CRYPTO_AUTH_ZUC_EIA3) {
		if (xform->auth.key.length == 16) {
			sess->template_job.hash_alg = IMB_AUTH_ZUC_EIA3_BITLEN;

			if (sess->auth.req_digest_len != 4) {
				IPSEC_MB_LOG(ERR, "Invalid digest size");
				return -EINVAL;
			}
		} else if (xform->auth.key.length == 32) {
			sess->template_job.hash_alg = IMB_AUTH_ZUC256_EIA3_BITLEN;
			if (sess->auth.req_digest_len != 4 &&
					sess->auth.req_digest_len != 8 &&
					sess->auth.req_digest_len != 16) {
				IPSEC_MB_LOG(ERR, "Invalid digest size");
				return -EINVAL;
			}
		} else {
			IPSEC_MB_LOG(ERR, "Invalid authentication key length");
			return -EINVAL;
		}

		sess->template_job.auth_tag_output_len_in_bytes = sess->auth.req_digest_len;

		memcpy(sess->auth.zuc_auth_key, xform->auth.key.data,
			xform->auth.key.length);
		sess->template_job.u.ZUC_EIA3._key = sess->auth.zuc_auth_key;
		return 0;
	} else if (xform->auth.algo == RTE_CRYPTO_AUTH_SNOW3G_UIA2) {
		sess->template_job.hash_alg = IMB_AUTH_SNOW3G_UIA2_BITLEN;
		uint16_t snow3g_uia2_digest_len =
			get_truncated_digest_byte_length(
						IMB_AUTH_SNOW3G_UIA2_BITLEN);
		if (sess->auth.req_digest_len != snow3g_uia2_digest_len) {
			IPSEC_MB_LOG(ERR, "Invalid digest size");
			return -EINVAL;
		}
		sess->template_job.auth_tag_output_len_in_bytes = sess->auth.req_digest_len;

		IMB_SNOW3G_INIT_KEY_SCHED(mb_mgr, xform->auth.key.data,
					&sess->auth.pKeySched_snow3g_auth);
		sess->template_job.u.SNOW3G_UIA2._key = (void *)
			&sess->auth.pKeySched_snow3g_auth;
		return 0;
	} else if (xform->auth.algo == RTE_CRYPTO_AUTH_KASUMI_F9) {
		sess->template_job.hash_alg = IMB_AUTH_KASUMI_UIA1;
		uint16_t kasumi_f9_digest_len =
			get_truncated_digest_byte_length(IMB_AUTH_KASUMI_UIA1);
		if (sess->auth.req_digest_len != kasumi_f9_digest_len) {
			IPSEC_MB_LOG(ERR, "Invalid digest size");
			return -EINVAL;
		}
		sess->template_job.auth_tag_output_len_in_bytes = sess->auth.req_digest_len;

		IMB_KASUMI_INIT_F9_KEY_SCHED(mb_mgr, xform->auth.key.data,
					&sess->auth.pKeySched_kasumi_auth);
		sess->template_job.u.KASUMI_UIA1._key = (void *)
			&sess->auth.pKeySched_kasumi_auth;
		return 0;
	}

	switch (xform->auth.algo) {
	case RTE_CRYPTO_AUTH_MD5_HMAC:
		sess->template_job.hash_alg = IMB_AUTH_MD5;
		break;
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		sess->template_job.hash_alg = IMB_AUTH_HMAC_SHA_1;
		if (xform->auth.key.length > get_auth_algo_blocksize(
				IMB_AUTH_HMAC_SHA_1)) {
			IMB_SHA1(mb_mgr,
				xform->auth.key.data,
				xform->auth.key.length,
				hashed_key);
		}
		break;
	case RTE_CRYPTO_AUTH_SHA1:
		sess->template_job.hash_alg = IMB_AUTH_SHA_1;
		auth_precompute = 0;
		break;
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
		sess->template_job.hash_alg = IMB_AUTH_HMAC_SHA_224;
		if (xform->auth.key.length > get_auth_algo_blocksize(
				IMB_AUTH_HMAC_SHA_224)) {
			IMB_SHA224(mb_mgr,
				xform->auth.key.data,
				xform->auth.key.length,
				hashed_key);
		}
		break;
	case RTE_CRYPTO_AUTH_SHA224:
		sess->template_job.hash_alg = IMB_AUTH_SHA_224;
		auth_precompute = 0;
		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		sess->template_job.hash_alg = IMB_AUTH_HMAC_SHA_256;
		if (xform->auth.key.length > get_auth_algo_blocksize(
				IMB_AUTH_HMAC_SHA_256)) {
			IMB_SHA256(mb_mgr,
				xform->auth.key.data,
				xform->auth.key.length,
				hashed_key);
		}
		break;
	case RTE_CRYPTO_AUTH_SHA256:
		sess->template_job.hash_alg = IMB_AUTH_SHA_256;
		auth_precompute = 0;
		break;
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
		sess->template_job.hash_alg = IMB_AUTH_HMAC_SHA_384;
		if (xform->auth.key.length > get_auth_algo_blocksize(
				IMB_AUTH_HMAC_SHA_384)) {
			IMB_SHA384(mb_mgr,
				xform->auth.key.data,
				xform->auth.key.length,
				hashed_key);
		}
		break;
	case RTE_CRYPTO_AUTH_SHA384:
		sess->template_job.hash_alg = IMB_AUTH_SHA_384;
		auth_precompute = 0;
		break;
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		sess->template_job.hash_alg = IMB_AUTH_HMAC_SHA_512;
		if (xform->auth.key.length > get_auth_algo_blocksize(
				IMB_AUTH_HMAC_SHA_512)) {
			IMB_SHA512(mb_mgr,
				xform->auth.key.data,
				xform->auth.key.length,
				hashed_key);
		}
		break;
	case RTE_CRYPTO_AUTH_SHA512:
		sess->template_job.hash_alg = IMB_AUTH_SHA_512;
		auth_precompute = 0;
		break;
#if IMB_VERSION(1, 5, 0) <= IMB_VERSION_NUM
	case RTE_CRYPTO_AUTH_SM3:
		sess->template_job.hash_alg = IMB_AUTH_SM3;
		break;
	case RTE_CRYPTO_AUTH_SM3_HMAC:
		sess->template_job.hash_alg = IMB_AUTH_HMAC_SM3;
		break;
#endif
	default:
		IPSEC_MB_LOG(ERR,
			"Unsupported authentication algorithm selection");
		return -ENOTSUP;
	}
	uint16_t trunc_digest_size =
			get_truncated_digest_byte_length(sess->template_job.hash_alg);
	uint16_t full_digest_size =
			get_digest_byte_length(sess->template_job.hash_alg);

	if (sess->auth.req_digest_len > full_digest_size ||
			sess->auth.req_digest_len == 0) {
		IPSEC_MB_LOG(ERR, "Invalid digest size");
		return -EINVAL;
	}

	if (sess->auth.req_digest_len != trunc_digest_size &&
			sess->auth.req_digest_len != full_digest_size)
		sess->template_job.auth_tag_output_len_in_bytes = full_digest_size;
	else
		sess->template_job.auth_tag_output_len_in_bytes = sess->auth.req_digest_len;

	/* Plain SHA does not require precompute key */
	if (auth_precompute == 0)
		return 0;

	/* Calculate Authentication precomputes */
	imb_hmac_ipad_opad(mb_mgr, sess->template_job.hash_alg,
				xform->auth.key.data, xform->auth.key.length,
				sess->auth.pads.inner, sess->auth.pads.outer);
	sess->template_job.u.HMAC._hashed_auth_key_xor_ipad =
		sess->auth.pads.inner;
	sess->template_job.u.HMAC._hashed_auth_key_xor_opad =
		sess->auth.pads.outer;

	return 0;
}

/** Set session cipher parameters */
static int
aesni_mb_set_session_cipher_parameters(const IMB_MGR *mb_mgr,
		struct aesni_mb_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	uint8_t is_aes = 0;
	uint8_t is_3DES = 0;
	uint8_t is_docsis = 0;
	uint8_t is_zuc = 0;
	uint8_t is_snow3g = 0;
	uint8_t is_kasumi = 0;
#if IMB_VERSION(1, 5, 0) <= IMB_VERSION_NUM
	uint8_t is_sm4 = 0;
#endif

	if (xform == NULL) {
		sess->template_job.cipher_mode = IMB_CIPHER_NULL;
		return 0;
	}

	if (xform->type != RTE_CRYPTO_SYM_XFORM_CIPHER) {
		IPSEC_MB_LOG(ERR, "Crypto xform struct not of type cipher");
		return -EINVAL;
	}

	/* Select cipher direction */
	switch (xform->cipher.op) {
	case RTE_CRYPTO_CIPHER_OP_ENCRYPT:
		sess->template_job.cipher_direction = IMB_DIR_ENCRYPT;
		break;
	case RTE_CRYPTO_CIPHER_OP_DECRYPT:
		sess->template_job.cipher_direction = IMB_DIR_DECRYPT;
		break;
	default:
		IPSEC_MB_LOG(ERR, "Invalid cipher operation parameter");
		return -EINVAL;
	}

	/* Select cipher mode */
	switch (xform->cipher.algo) {
	case RTE_CRYPTO_CIPHER_AES_CBC:
		sess->template_job.cipher_mode = IMB_CIPHER_CBC;
		is_aes = 1;
		break;
	case RTE_CRYPTO_CIPHER_AES_CTR:
		sess->template_job.cipher_mode = IMB_CIPHER_CNTR;
		is_aes = 1;
		break;
	case RTE_CRYPTO_CIPHER_AES_DOCSISBPI:
		sess->template_job.cipher_mode = IMB_CIPHER_DOCSIS_SEC_BPI;
		is_docsis = 1;
		break;
	case RTE_CRYPTO_CIPHER_DES_CBC:
		sess->template_job.cipher_mode = IMB_CIPHER_DES;
		break;
	case RTE_CRYPTO_CIPHER_DES_DOCSISBPI:
		sess->template_job.cipher_mode = IMB_CIPHER_DOCSIS_DES;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		sess->template_job.cipher_mode = IMB_CIPHER_DES3;
		is_3DES = 1;
		break;
	case RTE_CRYPTO_CIPHER_AES_ECB:
		sess->template_job.cipher_mode = IMB_CIPHER_ECB;
		is_aes = 1;
		break;
	case RTE_CRYPTO_CIPHER_ZUC_EEA3:
		sess->template_job.cipher_mode = IMB_CIPHER_ZUC_EEA3;
		is_zuc = 1;
		break;
	case RTE_CRYPTO_CIPHER_SNOW3G_UEA2:
		sess->template_job.cipher_mode = IMB_CIPHER_SNOW3G_UEA2_BITLEN;
		is_snow3g = 1;
		break;
	case RTE_CRYPTO_CIPHER_KASUMI_F8:
		sess->template_job.cipher_mode = IMB_CIPHER_KASUMI_UEA1_BITLEN;
		is_kasumi = 1;
		break;
	case RTE_CRYPTO_CIPHER_NULL:
		sess->template_job.cipher_mode = IMB_CIPHER_NULL;
		sess->template_job.key_len_in_bytes = 0;
		sess->iv.offset = xform->cipher.iv.offset;
		sess->template_job.iv_len_in_bytes = xform->cipher.iv.length;
		return 0;
#if IMB_VERSION(1, 5, 0) <= IMB_VERSION_NUM
	case RTE_CRYPTO_CIPHER_SM4_CBC:
		sess->template_job.cipher_mode = IMB_CIPHER_SM4_CBC;
		is_sm4 = 1;
		break;
	case RTE_CRYPTO_CIPHER_SM4_ECB:
		sess->template_job.cipher_mode = IMB_CIPHER_SM4_ECB;
		is_sm4 = 1;
		break;
#endif
#if IMB_VERSION(1, 5, 0) < IMB_VERSION_NUM
	case RTE_CRYPTO_CIPHER_SM4_CTR:
		sess->template_job.cipher_mode = IMB_CIPHER_SM4_CNTR;
		is_sm4 = 1;
		break;
#endif
	default:
		IPSEC_MB_LOG(ERR, "Unsupported cipher mode parameter");
		return -ENOTSUP;
	}

	/* Set IV parameters */
	sess->iv.offset = xform->cipher.iv.offset;
	sess->template_job.iv_len_in_bytes = xform->cipher.iv.length;

	/* Check key length and choose key expansion function for AES */
	if (is_aes) {
		switch (xform->cipher.key.length) {
		case IMB_KEY_128_BYTES:
			sess->template_job.key_len_in_bytes = IMB_KEY_128_BYTES;
			IMB_AES_KEYEXP_128(mb_mgr, xform->cipher.key.data,
					sess->cipher.expanded_aes_keys.encode,
					sess->cipher.expanded_aes_keys.decode);
			break;
		case IMB_KEY_192_BYTES:
			sess->template_job.key_len_in_bytes = IMB_KEY_192_BYTES;
			IMB_AES_KEYEXP_192(mb_mgr, xform->cipher.key.data,
					sess->cipher.expanded_aes_keys.encode,
					sess->cipher.expanded_aes_keys.decode);
			break;
		case IMB_KEY_256_BYTES:
			sess->template_job.key_len_in_bytes = IMB_KEY_256_BYTES;
			IMB_AES_KEYEXP_256(mb_mgr, xform->cipher.key.data,
					sess->cipher.expanded_aes_keys.encode,
					sess->cipher.expanded_aes_keys.decode);
			break;
		default:
			IPSEC_MB_LOG(ERR, "Invalid cipher key length");
			return -EINVAL;
		}

		sess->template_job.enc_keys = sess->cipher.expanded_aes_keys.encode;
		sess->template_job.dec_keys = sess->cipher.expanded_aes_keys.decode;
	} else if (is_docsis) {
		switch (xform->cipher.key.length) {
		case IMB_KEY_128_BYTES:
			sess->template_job.key_len_in_bytes = IMB_KEY_128_BYTES;
			IMB_AES_KEYEXP_128(mb_mgr, xform->cipher.key.data,
					sess->cipher.expanded_aes_keys.encode,
					sess->cipher.expanded_aes_keys.decode);
			break;
		case IMB_KEY_256_BYTES:
			sess->template_job.key_len_in_bytes = IMB_KEY_256_BYTES;
			IMB_AES_KEYEXP_256(mb_mgr, xform->cipher.key.data,
					sess->cipher.expanded_aes_keys.encode,
					sess->cipher.expanded_aes_keys.decode);
			break;
		default:
			IPSEC_MB_LOG(ERR, "Invalid cipher key length");
			return -EINVAL;
		}
		sess->template_job.enc_keys = sess->cipher.expanded_aes_keys.encode;
		sess->template_job.dec_keys = sess->cipher.expanded_aes_keys.decode;
	} else if (is_3DES) {
		uint64_t *keys[3] = {sess->cipher.exp_3des_keys.key[0],
				sess->cipher.exp_3des_keys.key[1],
				sess->cipher.exp_3des_keys.key[2]};

		switch (xform->cipher.key.length) {
		case  24:
			IMB_DES_KEYSCHED(mb_mgr, keys[0],
					xform->cipher.key.data);
			IMB_DES_KEYSCHED(mb_mgr, keys[1],
					xform->cipher.key.data + 8);
			IMB_DES_KEYSCHED(mb_mgr, keys[2],
					xform->cipher.key.data + 16);

			/* Initialize keys - 24 bytes: [K1-K2-K3] */
			sess->cipher.exp_3des_keys.ks_ptr[0] = keys[0];
			sess->cipher.exp_3des_keys.ks_ptr[1] = keys[1];
			sess->cipher.exp_3des_keys.ks_ptr[2] = keys[2];
			break;
		case 16:
			IMB_DES_KEYSCHED(mb_mgr, keys[0],
					xform->cipher.key.data);
			IMB_DES_KEYSCHED(mb_mgr, keys[1],
					xform->cipher.key.data + 8);
			/* Initialize keys - 16 bytes: [K1=K1,K2=K2,K3=K1] */
			sess->cipher.exp_3des_keys.ks_ptr[0] = keys[0];
			sess->cipher.exp_3des_keys.ks_ptr[1] = keys[1];
			sess->cipher.exp_3des_keys.ks_ptr[2] = keys[0];
			break;
		case 8:
			IMB_DES_KEYSCHED(mb_mgr, keys[0],
					xform->cipher.key.data);

			/* Initialize keys - 8 bytes: [K1 = K2 = K3] */
			sess->cipher.exp_3des_keys.ks_ptr[0] = keys[0];
			sess->cipher.exp_3des_keys.ks_ptr[1] = keys[0];
			sess->cipher.exp_3des_keys.ks_ptr[2] = keys[0];
			break;
		default:
			IPSEC_MB_LOG(ERR, "Invalid cipher key length");
			return -EINVAL;
		}

		sess->template_job.enc_keys = sess->cipher.exp_3des_keys.ks_ptr;
		sess->template_job.dec_keys = sess->cipher.exp_3des_keys.ks_ptr;
		sess->template_job.key_len_in_bytes = 24;
	} else if (is_zuc) {
		if (xform->cipher.key.length != 16 &&
				xform->cipher.key.length != 32) {
			IPSEC_MB_LOG(ERR, "Invalid cipher key length");
			return -EINVAL;
		}
		sess->template_job.key_len_in_bytes = xform->cipher.key.length;
		memcpy(sess->cipher.zuc_cipher_key, xform->cipher.key.data,
			xform->cipher.key.length);
		sess->template_job.enc_keys = sess->cipher.zuc_cipher_key;
		sess->template_job.dec_keys = sess->cipher.zuc_cipher_key;
	} else if (is_snow3g) {
		if (xform->cipher.key.length != 16) {
			IPSEC_MB_LOG(ERR, "Invalid cipher key length");
			return -EINVAL;
		}
		sess->template_job.key_len_in_bytes = 16;
		IMB_SNOW3G_INIT_KEY_SCHED(mb_mgr, xform->cipher.key.data,
					&sess->cipher.pKeySched_snow3g_cipher);
		sess->template_job.enc_keys = &sess->cipher.pKeySched_snow3g_cipher;
		sess->template_job.dec_keys = &sess->cipher.pKeySched_snow3g_cipher;
	} else if (is_kasumi) {
		if (xform->cipher.key.length != 16) {
			IPSEC_MB_LOG(ERR, "Invalid cipher key length");
			return -EINVAL;
		}
		sess->template_job.key_len_in_bytes = 16;
		IMB_KASUMI_INIT_F8_KEY_SCHED(mb_mgr, xform->cipher.key.data,
					&sess->cipher.pKeySched_kasumi_cipher);
		sess->template_job.enc_keys = &sess->cipher.pKeySched_kasumi_cipher;
		sess->template_job.dec_keys = &sess->cipher.pKeySched_kasumi_cipher;
#if IMB_VERSION(1, 5, 0) <= IMB_VERSION_NUM
	} else if (is_sm4) {
		sess->template_job.key_len_in_bytes = IMB_KEY_128_BYTES;
		IMB_SM4_KEYEXP(mb_mgr, xform->cipher.key.data,
				sess->cipher.expanded_sm4_keys.encode,
				sess->cipher.expanded_sm4_keys.decode);
		sess->template_job.enc_keys = sess->cipher.expanded_sm4_keys.encode;
		sess->template_job.dec_keys = sess->cipher.expanded_sm4_keys.decode;
#endif
	} else {
		if (xform->cipher.key.length != 8) {
			IPSEC_MB_LOG(ERR, "Invalid cipher key length");
			return -EINVAL;
		}
		sess->template_job.key_len_in_bytes = 8;

		IMB_DES_KEYSCHED(mb_mgr,
			(uint64_t *)sess->cipher.expanded_aes_keys.encode,
				xform->cipher.key.data);
		IMB_DES_KEYSCHED(mb_mgr,
			(uint64_t *)sess->cipher.expanded_aes_keys.decode,
				xform->cipher.key.data);
		sess->template_job.enc_keys = sess->cipher.expanded_aes_keys.encode;
		sess->template_job.dec_keys = sess->cipher.expanded_aes_keys.decode;
	}

	return 0;
}

static int
aesni_mb_set_session_aead_parameters(IMB_MGR *mb_mgr,
		struct aesni_mb_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	switch (xform->aead.op) {
	case RTE_CRYPTO_AEAD_OP_ENCRYPT:
		sess->template_job.cipher_direction = IMB_DIR_ENCRYPT;
		sess->auth.operation = RTE_CRYPTO_AUTH_OP_GENERATE;
		break;
	case RTE_CRYPTO_AEAD_OP_DECRYPT:
		sess->template_job.cipher_direction = IMB_DIR_DECRYPT;
		sess->auth.operation = RTE_CRYPTO_AUTH_OP_VERIFY;
		break;
	default:
		IPSEC_MB_LOG(ERR, "Invalid aead operation parameter");
		return -EINVAL;
	}

	/* Set IV parameters */
	sess->iv.offset = xform->aead.iv.offset;
	sess->template_job.iv_len_in_bytes = xform->aead.iv.length;

	/* Set digest sizes */
	sess->auth.req_digest_len = xform->aead.digest_length;
	sess->template_job.auth_tag_output_len_in_bytes = sess->auth.req_digest_len;

	switch (xform->aead.algo) {
	case RTE_CRYPTO_AEAD_AES_CCM:
		sess->template_job.cipher_mode = IMB_CIPHER_CCM;
		sess->template_job.hash_alg = IMB_AUTH_AES_CCM;
		sess->template_job.u.CCM.aad_len_in_bytes = xform->aead.aad_length;

		/* Check key length and choose key expansion function for AES */
		switch (xform->aead.key.length) {
		case IMB_KEY_128_BYTES:
			sess->template_job.key_len_in_bytes = IMB_KEY_128_BYTES;
			IMB_AES_KEYEXP_128(mb_mgr, xform->aead.key.data,
					sess->cipher.expanded_aes_keys.encode,
					sess->cipher.expanded_aes_keys.decode);
			break;
		case IMB_KEY_256_BYTES:
			sess->template_job.key_len_in_bytes = IMB_KEY_256_BYTES;
			IMB_AES_KEYEXP_256(mb_mgr, xform->aead.key.data,
					sess->cipher.expanded_aes_keys.encode,
					sess->cipher.expanded_aes_keys.decode);
			break;
		default:
			IPSEC_MB_LOG(ERR, "Invalid cipher key length");
			return -EINVAL;
		}

		sess->template_job.enc_keys = sess->cipher.expanded_aes_keys.encode;
		sess->template_job.dec_keys = sess->cipher.expanded_aes_keys.decode;
		/* CCM digests must be between 4 and 16 and an even number */
		if (sess->auth.req_digest_len < AES_CCM_DIGEST_MIN_LEN ||
			sess->auth.req_digest_len > AES_CCM_DIGEST_MAX_LEN ||
			(sess->auth.req_digest_len & 1) == 1) {
			IPSEC_MB_LOG(ERR, "Invalid digest size");
			return -EINVAL;
		}
		break;

	case RTE_CRYPTO_AEAD_AES_GCM:
		sess->template_job.cipher_mode = IMB_CIPHER_GCM;
		sess->template_job.hash_alg = IMB_AUTH_AES_GMAC;
		sess->template_job.u.GCM.aad_len_in_bytes = xform->aead.aad_length;

		switch (xform->aead.key.length) {
		case IMB_KEY_128_BYTES:
			sess->template_job.key_len_in_bytes = IMB_KEY_128_BYTES;
			IMB_AES128_GCM_PRE(mb_mgr, xform->aead.key.data,
				&sess->cipher.gcm_key);
			break;
		case IMB_KEY_192_BYTES:
			sess->template_job.key_len_in_bytes = IMB_KEY_192_BYTES;
			IMB_AES192_GCM_PRE(mb_mgr, xform->aead.key.data,
				&sess->cipher.gcm_key);
			break;
		case IMB_KEY_256_BYTES:
			sess->template_job.key_len_in_bytes = IMB_KEY_256_BYTES;
			IMB_AES256_GCM_PRE(mb_mgr, xform->aead.key.data,
				&sess->cipher.gcm_key);
			break;
		default:
			IPSEC_MB_LOG(ERR, "Invalid cipher key length");
			return -EINVAL;
		}

		sess->template_job.enc_keys = &sess->cipher.gcm_key;
		sess->template_job.dec_keys = &sess->cipher.gcm_key;
		/* GCM digest size must be between 1 and 16 */
		if (sess->auth.req_digest_len == 0 ||
				sess->auth.req_digest_len > 16) {
			IPSEC_MB_LOG(ERR, "Invalid digest size");
			return -EINVAL;
		}
		break;

	case RTE_CRYPTO_AEAD_CHACHA20_POLY1305:
		sess->template_job.cipher_mode = IMB_CIPHER_CHACHA20_POLY1305;
		sess->template_job.hash_alg = IMB_AUTH_CHACHA20_POLY1305;
		sess->template_job.u.CHACHA20_POLY1305.aad_len_in_bytes =
			xform->aead.aad_length;

		if (xform->aead.key.length != 32) {
			IPSEC_MB_LOG(ERR, "Invalid key length");
			return -EINVAL;
		}
		sess->template_job.key_len_in_bytes = 32;
		memcpy(sess->cipher.expanded_aes_keys.encode,
			xform->aead.key.data, 32);
		sess->template_job.enc_keys = sess->cipher.expanded_aes_keys.encode;
		sess->template_job.dec_keys = sess->cipher.expanded_aes_keys.decode;
		if (sess->auth.req_digest_len != 16) {
			IPSEC_MB_LOG(ERR, "Invalid digest size");
			return -EINVAL;
		}
		break;
#if IMB_VERSION(1, 5, 0) < IMB_VERSION_NUM
	case RTE_CRYPTO_AEAD_SM4_GCM:
		sess->template_job.cipher_mode = IMB_CIPHER_SM4_GCM;
		sess->template_job.hash_alg = IMB_AUTH_SM4_GCM;
		sess->template_job.u.GCM.aad_len_in_bytes = xform->aead.aad_length;

		if (xform->aead.key.length != 16) {
			IPSEC_MB_LOG(ERR, "Invalid key length");
			return -EINVAL;
		}
		sess->template_job.key_len_in_bytes = 16;
		imb_sm4_gcm_pre(mb_mgr, xform->aead.key.data, &sess->cipher.gcm_key);
		sess->template_job.enc_keys = &sess->cipher.gcm_key;
		sess->template_job.dec_keys = &sess->cipher.gcm_key;
		break;
#endif
	default:
		IPSEC_MB_LOG(ERR, "Unsupported aead mode parameter");
		return -ENOTSUP;
	}

	return 0;
}

/** Configure a aesni multi-buffer session from a crypto xform chain */
int
aesni_mb_session_configure(IMB_MGR *mb_mgr,
		void *priv_sess,
		const struct rte_crypto_sym_xform *xform)
{
	const struct rte_crypto_sym_xform *auth_xform = NULL;
	const struct rte_crypto_sym_xform *cipher_xform = NULL;
	const struct rte_crypto_sym_xform *aead_xform = NULL;
	enum ipsec_mb_operation mode;
	struct aesni_mb_session *sess = (struct aesni_mb_session *) priv_sess;
	int ret;

	ret = ipsec_mb_parse_xform(xform, &mode, &auth_xform,
				&cipher_xform, &aead_xform);
	if (ret)
		return ret;

	/* Select Crypto operation - hash then cipher / cipher then hash */
	switch (mode) {
	case IPSEC_MB_OP_HASH_VERIFY_THEN_DECRYPT:
		sess->template_job.chain_order = IMB_ORDER_HASH_CIPHER;
		break;
	case IPSEC_MB_OP_ENCRYPT_THEN_HASH_GEN:
	case IPSEC_MB_OP_DECRYPT_THEN_HASH_VERIFY:
		sess->template_job.chain_order = IMB_ORDER_CIPHER_HASH;
		break;
	case IPSEC_MB_OP_HASH_GEN_ONLY:
	case IPSEC_MB_OP_HASH_VERIFY_ONLY:
	case IPSEC_MB_OP_HASH_GEN_THEN_ENCRYPT:
		sess->template_job.chain_order = IMB_ORDER_HASH_CIPHER;
		break;
	/*
	 * Multi buffer library operates only at two modes,
	 * IMB_ORDER_CIPHER_HASH and IMB_ORDER_HASH_CIPHER.
	 * When doing ciphering only, chain order depends
	 * on cipher operation: encryption is always
	 * the first operation and decryption the last one.
	 */
	case IPSEC_MB_OP_ENCRYPT_ONLY:
		sess->template_job.chain_order = IMB_ORDER_CIPHER_HASH;
		break;
	case IPSEC_MB_OP_DECRYPT_ONLY:
		sess->template_job.chain_order = IMB_ORDER_HASH_CIPHER;
		break;
	case IPSEC_MB_OP_AEAD_AUTHENTICATED_ENCRYPT:
		sess->template_job.chain_order = IMB_ORDER_CIPHER_HASH;
		break;
	case IPSEC_MB_OP_AEAD_AUTHENTICATED_DECRYPT:
		sess->template_job.chain_order = IMB_ORDER_HASH_CIPHER;
		break;
	case IPSEC_MB_OP_NOT_SUPPORTED:
	default:
		IPSEC_MB_LOG(ERR,
			"Unsupported operation chain order parameter");
		return -ENOTSUP;
	}

	/* Default IV length = 0 */
	sess->template_job.iv_len_in_bytes = 0;

	ret = aesni_mb_set_session_auth_parameters(mb_mgr, sess, auth_xform);
	if (ret != 0) {
		IPSEC_MB_LOG(ERR,
			"Invalid/unsupported authentication parameters");
		return ret;
	}

	ret = aesni_mb_set_session_cipher_parameters(mb_mgr, sess,
			cipher_xform);
	if (ret != 0) {
		IPSEC_MB_LOG(ERR, "Invalid/unsupported cipher parameters");
		return ret;
	}

	if (aead_xform) {
		ret = aesni_mb_set_session_aead_parameters(mb_mgr, sess,
				aead_xform);
		if (ret != 0) {
			IPSEC_MB_LOG(ERR,
				"Invalid/unsupported aead parameters");
			return ret;
		}
	}

	sess->session_id = imb_set_session(mb_mgr, &sess->template_job);
	sess->pid = getpid();
	RTE_PER_LCORE(pid) = sess->pid;

	return 0;
}

/** Check DOCSIS security session configuration is valid */
static int
check_docsis_sec_session(struct rte_security_session_conf *conf)
{
	struct rte_crypto_sym_xform *crypto_sym = conf->crypto_xform;
	struct rte_security_docsis_xform *docsis = &conf->docsis;

	/* Downlink: CRC generate -> Cipher encrypt */
	if (docsis->direction == RTE_SECURITY_DOCSIS_DOWNLINK) {

		if (crypto_sym != NULL &&
		    crypto_sym->type ==	RTE_CRYPTO_SYM_XFORM_CIPHER &&
		    crypto_sym->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT &&
		    crypto_sym->cipher.algo ==
					RTE_CRYPTO_CIPHER_AES_DOCSISBPI &&
		    (crypto_sym->cipher.key.length == IMB_KEY_128_BYTES ||
		     crypto_sym->cipher.key.length == IMB_KEY_256_BYTES) &&
		    crypto_sym->cipher.iv.length == IMB_AES_BLOCK_SIZE &&
		    crypto_sym->next == NULL) {
			return 0;
		}
	/* Uplink: Cipher decrypt -> CRC verify */
	} else if (docsis->direction == RTE_SECURITY_DOCSIS_UPLINK) {

		if (crypto_sym != NULL &&
		    crypto_sym->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
		    crypto_sym->cipher.op == RTE_CRYPTO_CIPHER_OP_DECRYPT &&
		    crypto_sym->cipher.algo ==
					RTE_CRYPTO_CIPHER_AES_DOCSISBPI &&
		    (crypto_sym->cipher.key.length == IMB_KEY_128_BYTES ||
		     crypto_sym->cipher.key.length == IMB_KEY_256_BYTES) &&
		    crypto_sym->cipher.iv.length == IMB_AES_BLOCK_SIZE &&
		    crypto_sym->next == NULL) {
			return 0;
		}
	}

	return -EINVAL;
}

/** Set DOCSIS security session auth (CRC) parameters */
static int
aesni_mb_set_docsis_sec_session_auth_parameters(struct aesni_mb_session *sess,
		struct rte_security_docsis_xform *xform)
{
	if (xform == NULL) {
		IPSEC_MB_LOG(ERR, "Invalid DOCSIS xform");
		return -EINVAL;
	}

	/* Select CRC generate/verify */
	if (xform->direction == RTE_SECURITY_DOCSIS_UPLINK) {
		sess->template_job.hash_alg = IMB_AUTH_DOCSIS_CRC32;
		sess->auth.operation = RTE_CRYPTO_AUTH_OP_VERIFY;
	} else if (xform->direction == RTE_SECURITY_DOCSIS_DOWNLINK) {
		sess->template_job.hash_alg = IMB_AUTH_DOCSIS_CRC32;
		sess->auth.operation = RTE_CRYPTO_AUTH_OP_GENERATE;
	} else {
		IPSEC_MB_LOG(ERR, "Unsupported DOCSIS direction");
		return -ENOTSUP;
	}

	sess->auth.req_digest_len = RTE_ETHER_CRC_LEN;
	sess->template_job.auth_tag_output_len_in_bytes = RTE_ETHER_CRC_LEN;

	return 0;
}

/**
 * Parse DOCSIS security session configuration and set private session
 * parameters
 */
static int
aesni_mb_set_docsis_sec_session_parameters(
		__rte_unused struct rte_cryptodev *dev,
		struct rte_security_session_conf *conf,
		void *sess)
{
	IMB_MGR  *mb_mgr = alloc_init_mb_mgr();
	struct rte_security_docsis_xform *docsis_xform;
	struct rte_crypto_sym_xform *cipher_xform;
	struct aesni_mb_session *ipsec_sess = sess;
	int ret = 0;

	if (!mb_mgr)
		return -ENOMEM;

	ret = check_docsis_sec_session(conf);
	if (ret) {
		IPSEC_MB_LOG(ERR, "Unsupported DOCSIS security configuration");
		goto error_exit;
	}

	switch (conf->docsis.direction) {
	case RTE_SECURITY_DOCSIS_UPLINK:
		ipsec_sess->template_job.chain_order = IMB_ORDER_CIPHER_HASH;
		docsis_xform = &conf->docsis;
		cipher_xform = conf->crypto_xform;
		break;
	case RTE_SECURITY_DOCSIS_DOWNLINK:
		ipsec_sess->template_job.chain_order = IMB_ORDER_HASH_CIPHER;
		cipher_xform = conf->crypto_xform;
		docsis_xform = &conf->docsis;
		break;
	default:
		IPSEC_MB_LOG(ERR, "Unsupported DOCSIS security configuration");
		ret = -EINVAL;
		goto error_exit;
	}

	/* Default IV length = 0 */
	ipsec_sess->template_job.iv_len_in_bytes = 0;

	ret = aesni_mb_set_docsis_sec_session_auth_parameters(ipsec_sess,
			docsis_xform);
	if (ret != 0) {
		IPSEC_MB_LOG(ERR, "Invalid/unsupported DOCSIS parameters");
		goto error_exit;
	}

	ret = aesni_mb_set_session_cipher_parameters(mb_mgr,
			ipsec_sess, cipher_xform);

	if (ret != 0) {
		IPSEC_MB_LOG(ERR, "Invalid/unsupported cipher parameters");
		goto error_exit;
	}

	ipsec_sess->session_id = imb_set_session(mb_mgr, &ipsec_sess->template_job);

error_exit:
	free_mb_mgr(mb_mgr);
	return ret;
}

static inline uint8_t *get_src_buf(struct rte_crypto_op *op)
{
	return op->sym->m_src ?
	rte_pktmbuf_mtod(op->sym->m_src, uint8_t *) : NULL;
}
static inline uint8_t *get_dst_buf(struct rte_crypto_op *op)
{
	return op->sym->m_dst ?
	rte_pktmbuf_mtod(op->sym->m_dst, uint8_t *) : NULL;
}

static inline uint64_t
auth_start_offset(uint8_t *p_src, uint8_t *p_dst,
		struct aesni_mb_session *session,
		uint32_t oop, const uint32_t auth_offset,
		const uint32_t cipher_offset, const uint32_t auth_length,
		const uint32_t cipher_length, uint8_t lb_sgl)
{
	uintptr_t u_src, u_dst;
	uint32_t cipher_end, auth_end;

	/* Only cipher then hash needs special calculation. */
	if (!oop || session->template_job.chain_order != IMB_ORDER_CIPHER_HASH || lb_sgl)
		return auth_offset;

	u_src = (uintptr_t)p_src;
	u_dst = (uintptr_t)p_dst + auth_offset;

	/**
	 * Copy the content between cipher offset and auth offset for generating
	 * correct digest.
	 */
	if (cipher_offset > auth_offset)
		memcpy(p_dst + auth_offset,
				p_src + auth_offset,
				cipher_offset -
				auth_offset);

	/**
	 * Copy the content between (cipher offset + length) and (auth offset +
	 * length) for generating correct digest
	 */
	cipher_end = cipher_offset + cipher_length;
	auth_end = auth_offset + auth_length;
	if (cipher_end < auth_end)
		memcpy(p_dst + cipher_end, p_src + cipher_end,
				auth_end - cipher_end);

	/**
	 * Since intel-ipsec-mb only supports positive values,
	 * we need to deduct the correct offset between src and dst.
	 */

	return u_src < u_dst ? (u_dst - u_src) :
			(UINT64_MAX - u_src + u_dst + 1);
}

static inline void
set_cpu_mb_job_params(IMB_JOB *job, struct aesni_mb_session *session,
		union rte_crypto_sym_ofs sofs, void *src_buf, void *dst_buf,
		uint32_t len, struct rte_crypto_va_iova_ptr *iv,
		struct rte_crypto_va_iova_ptr *aad,
		void **digest, struct rte_crypto_va_iova_ptr *vec_digest,
		void *udata)
{
	memcpy(job, &session->template_job, sizeof(IMB_JOB));

	/* Set authentication parameters */
	job->iv = iv->va;

	switch (job->hash_alg) {
	case IMB_AUTH_AES_CCM:
		job->u.CCM.aad = (uint8_t *)aad->va + 18;
		job->iv++;
		break;

	case IMB_AUTH_AES_GMAC:
		job->u.GCM.aad = aad->va;
		break;

	case IMB_AUTH_AES_GMAC_128:
	case IMB_AUTH_AES_GMAC_192:
	case IMB_AUTH_AES_GMAC_256:
		job->u.GMAC._iv = iv->va;
		break;

	case IMB_AUTH_CHACHA20_POLY1305:
		job->u.CHACHA20_POLY1305.aad = aad->va;
		break;

#if IMB_VERSION(1, 5, 0) < IMB_VERSION_NUM
	case IMB_AUTH_SM4_GCM:
		job->u.GCM.aad = aad->va;
		break;
#endif

	default:
		break;
	}

	/*
	 * Multi-buffer library current only support returning a truncated
	 * digest length as specified in the relevant IPsec RFCs
	 */

	/* Data Parameters */
	job->src = src_buf;
	if (dst_buf)
		job->dst = (uint8_t *)dst_buf + sofs.ofs.cipher.head;
	else
		job->dst = (uint8_t *)src_buf + sofs.ofs.cipher.head;
	job->cipher_start_src_offset_in_bytes = sofs.ofs.cipher.head;
	job->msg_len_to_hash_in_bytes = len - sofs.ofs.auth.head -
		sofs.ofs.auth.tail;
	job->msg_len_to_cipher_in_bytes = len - sofs.ofs.cipher.head -
		sofs.ofs.cipher.tail;
	job->hash_start_src_offset_in_bytes = auth_start_offset(
				src_buf, dst_buf,
				session, dst_buf != NULL,
				sofs.ofs.auth.head,
				sofs.ofs.cipher.head,
				job->msg_len_to_hash_in_bytes,
				job->msg_len_to_cipher_in_bytes, 0);

	job->user_data = udata;

	/** Check if conditions are met for digest-appended operations */
	if (job->cipher_mode != IMB_CIPHER_NULL && job->chain_order == IMB_ORDER_HASH_CIPHER) {
		uintptr_t end_cipher, start_cipher;
		if (dst_buf == NULL) {
			end_cipher = (uintptr_t)src_buf + job->msg_len_to_cipher_in_bytes;
			start_cipher = (uintptr_t)src_buf;
		} else {
			end_cipher = (uintptr_t)dst_buf + job->msg_len_to_cipher_in_bytes;
			start_cipher = (uintptr_t)dst_buf;
		}
		if (start_cipher < (uintptr_t)vec_digest->va &&
			(uintptr_t)vec_digest->va < end_cipher)
			*digest = (void *)((uintptr_t)src_buf + job->msg_len_to_hash_in_bytes);
	}
	job->auth_tag_output = *digest;

	if (job->cipher_mode == IMB_CIPHER_NULL && dst_buf) {
		memcpy(job->dst + job->cipher_start_src_offset_in_bytes,
			job->src + job->cipher_start_src_offset_in_bytes,
			job->msg_len_to_cipher_in_bytes);
	}
}

static int
handle_aead_sgl_job(IMB_JOB *job, IMB_MGR *mb_mgr,
		uint32_t *total_len,
		struct aesni_mb_op_buf_data *src_data,
		struct aesni_mb_op_buf_data *dst_data)
{
	uint32_t data_len, part_len;

	if (*total_len == 0) {
		job->sgl_state = IMB_SGL_COMPLETE;
		return 0;
	}

	if (src_data->m == NULL) {
		IPSEC_MB_LOG(ERR, "Invalid source buffer");
		return -EINVAL;
	}

	job->sgl_state = IMB_SGL_UPDATE;

	data_len = src_data->m->data_len - src_data->offset;

	job->src = rte_pktmbuf_mtod_offset(src_data->m, uint8_t *,
			src_data->offset);

	if (dst_data->m != NULL) {
		if (dst_data->m->data_len - dst_data->offset == 0) {
			dst_data->m = dst_data->m->next;
			if (dst_data->m == NULL) {
				IPSEC_MB_LOG(ERR, "Invalid destination buffer");
				return -EINVAL;
			}
			dst_data->offset = 0;
		}
		part_len = RTE_MIN(data_len, (dst_data->m->data_len -
				dst_data->offset));
		job->dst = rte_pktmbuf_mtod_offset(dst_data->m,
				uint8_t *, dst_data->offset);
		dst_data->offset += part_len;
	} else {
		part_len = RTE_MIN(data_len, *total_len);
		job->dst = rte_pktmbuf_mtod_offset(src_data->m, uint8_t *,
			src_data->offset);
	}

	job->msg_len_to_cipher_in_bytes = part_len;
	job->msg_len_to_hash_in_bytes = part_len;

	job = IMB_SUBMIT_JOB(mb_mgr);

	*total_len -= part_len;

	if (part_len != data_len) {
		src_data->offset += part_len;
	} else {
		src_data->m = src_data->m->next;
		src_data->offset = 0;
	}

	return 0;
}

static uint64_t
sgl_linear_cipher_auth_len(IMB_JOB *job, uint64_t *auth_len)
{
	uint64_t cipher_len;

	if (job->cipher_mode == IMB_CIPHER_SNOW3G_UEA2_BITLEN ||
			job->cipher_mode == IMB_CIPHER_KASUMI_UEA1_BITLEN)
		cipher_len = (job->msg_len_to_cipher_in_bits >> 3) +
				(job->cipher_start_src_offset_in_bits >> 3);
	else
		cipher_len = job->msg_len_to_cipher_in_bytes +
				job->cipher_start_src_offset_in_bytes;

	if (job->hash_alg == IMB_AUTH_SNOW3G_UIA2_BITLEN ||
			job->hash_alg == IMB_AUTH_ZUC_EIA3_BITLEN)
		*auth_len = (job->msg_len_to_hash_in_bits >> 3) +
				job->hash_start_src_offset_in_bytes;
	else
		*auth_len = job->msg_len_to_hash_in_bytes +
				job->hash_start_src_offset_in_bytes;

	return RTE_MAX(*auth_len, cipher_len);
}

static int
handle_sgl_linear(IMB_JOB *job, struct rte_crypto_op *op, uint32_t dst_offset,
		struct aesni_mb_session *session)
{
	uint64_t auth_len, total_len;
	uint8_t *src, *linear_buf = NULL;
	int lb_offset = 0;
	struct rte_mbuf *src_seg;
	uint16_t src_len;

	total_len = sgl_linear_cipher_auth_len(job, &auth_len);
	linear_buf = rte_zmalloc(NULL, total_len + job->auth_tag_output_len_in_bytes, 0);
	if (linear_buf == NULL) {
		IPSEC_MB_LOG(ERR, "Error allocating memory for SGL Linear Buffer");
		return -1;
	}

	for (src_seg = op->sym->m_src; (src_seg != NULL) &&
			(total_len - lb_offset > 0);
			src_seg = src_seg->next) {
		src = rte_pktmbuf_mtod(src_seg, uint8_t *);
		src_len =  RTE_MIN(src_seg->data_len, total_len - lb_offset);
		rte_memcpy(linear_buf + lb_offset, src, src_len);
		lb_offset += src_len;
	}

	job->src = linear_buf;
	job->dst = linear_buf + dst_offset;
	job->user_data2 = linear_buf;

	if (job->hash_alg == IMB_AUTH_AES_GMAC)
		job->u.GCM.aad = linear_buf;

	if (session->auth.operation == RTE_CRYPTO_AUTH_OP_VERIFY)
		job->auth_tag_output = linear_buf + lb_offset;
	else
		job->auth_tag_output = linear_buf + auth_len;

	return 0;
}

static inline int
imb_lib_support_sgl_algo(IMB_CIPHER_MODE alg)
{
	if (alg == IMB_CIPHER_CHACHA20_POLY1305 ||
			alg == IMB_CIPHER_CHACHA20_POLY1305_SGL ||
			alg == IMB_CIPHER_GCM_SGL ||
			alg == IMB_CIPHER_GCM)
		return 1;
	return 0;
}

static inline int
single_sgl_job(IMB_JOB *job, struct rte_crypto_op *op,
		int oop, uint32_t offset, struct rte_mbuf *m_src,
		struct rte_mbuf *m_dst, struct IMB_SGL_IOV *sgl_segs)
{
	uint32_t num_segs = 0;
	struct aesni_mb_op_buf_data src_sgl = {0};
	struct aesni_mb_op_buf_data dst_sgl = {0};
	uint32_t total_len;

	job->sgl_state = IMB_SGL_ALL;

	src_sgl.m = m_src;
	src_sgl.offset = offset;

	while (src_sgl.offset >= src_sgl.m->data_len) {
		src_sgl.offset -= src_sgl.m->data_len;
		src_sgl.m = src_sgl.m->next;

		RTE_ASSERT(src_sgl.m != NULL);
	}

	if (oop) {
		dst_sgl.m = m_dst;
		dst_sgl.offset = offset;

		while (dst_sgl.offset >= dst_sgl.m->data_len) {
			dst_sgl.offset -= dst_sgl.m->data_len;
			dst_sgl.m = dst_sgl.m->next;

			RTE_ASSERT(dst_sgl.m != NULL);
		}
	}
	total_len = op->sym->aead.data.length;

	while (total_len != 0) {
		uint32_t data_len, part_len;

		if (src_sgl.m == NULL) {
			IPSEC_MB_LOG(ERR, "Invalid source buffer");
			return -EINVAL;
		}

		data_len = src_sgl.m->data_len - src_sgl.offset;

		sgl_segs[num_segs].in = rte_pktmbuf_mtod_offset(src_sgl.m, uint8_t *,
				src_sgl.offset);

		if (dst_sgl.m != NULL) {
			if (dst_sgl.m->data_len - dst_sgl.offset == 0) {
				dst_sgl.m = dst_sgl.m->next;
				if (dst_sgl.m == NULL) {
					IPSEC_MB_LOG(ERR, "Invalid destination buffer");
					return -EINVAL;
				}
				dst_sgl.offset = 0;
			}
			part_len = RTE_MIN(data_len, (dst_sgl.m->data_len -
					dst_sgl.offset));
			sgl_segs[num_segs].out = rte_pktmbuf_mtod_offset(dst_sgl.m,
					uint8_t *, dst_sgl.offset);
			dst_sgl.offset += part_len;
		} else {
			part_len = RTE_MIN(data_len, total_len);
			sgl_segs[num_segs].out = rte_pktmbuf_mtod_offset(src_sgl.m, uint8_t *,
				src_sgl.offset);
		}

		sgl_segs[num_segs].len = part_len;

		total_len -= part_len;

		if (part_len != data_len) {
			src_sgl.offset += part_len;
		} else {
			src_sgl.m = src_sgl.m->next;
			src_sgl.offset = 0;
		}
		num_segs++;
	}
	job->num_sgl_io_segs = num_segs;
	job->sgl_io_segs = sgl_segs;
	return 0;
}

static inline int
multi_sgl_job(IMB_JOB *job, struct rte_crypto_op *op,
		int oop, uint32_t offset, struct rte_mbuf *m_src,
		struct rte_mbuf *m_dst, IMB_MGR *mb_mgr)
{
	int ret;
	IMB_JOB base_job;
	struct aesni_mb_op_buf_data src_sgl = {0};
	struct aesni_mb_op_buf_data dst_sgl = {0};
	uint32_t total_len;

	base_job = *job;
	job->sgl_state = IMB_SGL_INIT;
	job = IMB_SUBMIT_JOB(mb_mgr);
	total_len = op->sym->aead.data.length;

	src_sgl.m = m_src;
	src_sgl.offset = offset;

	while (src_sgl.offset >= src_sgl.m->data_len) {
		src_sgl.offset -= src_sgl.m->data_len;
		src_sgl.m = src_sgl.m->next;

		RTE_ASSERT(src_sgl.m != NULL);
	}

	if (oop) {
		dst_sgl.m = m_dst;
		dst_sgl.offset = offset;

		while (dst_sgl.offset >= dst_sgl.m->data_len) {
			dst_sgl.offset -= dst_sgl.m->data_len;
			dst_sgl.m = dst_sgl.m->next;

			RTE_ASSERT(dst_sgl.m != NULL);
		}
	}

	while (job->sgl_state != IMB_SGL_COMPLETE) {
		job = IMB_GET_NEXT_JOB(mb_mgr);
		*job = base_job;
		ret = handle_aead_sgl_job(job, mb_mgr, &total_len,
			&src_sgl, &dst_sgl);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static inline int
set_gcm_job(IMB_MGR *mb_mgr, IMB_JOB *job, const uint8_t sgl,
	struct aesni_mb_qp_data *qp_data,
	struct rte_crypto_op *op, uint8_t *digest_idx,
	const struct aesni_mb_session *session,
	struct rte_mbuf *m_src, struct rte_mbuf *m_dst,
	const int oop)
{
	const uint32_t m_offset = op->sym->aead.data.offset;

	job->u.GCM.aad = op->sym->aead.aad.data;
	if (sgl) {
		job->u.GCM.ctx = &qp_data->gcm_sgl_ctx;
		job->cipher_mode = IMB_CIPHER_GCM_SGL;
		job->hash_alg = IMB_AUTH_GCM_SGL;
		job->hash_start_src_offset_in_bytes = 0;
		job->msg_len_to_hash_in_bytes = 0;
		job->msg_len_to_cipher_in_bytes = 0;
		job->cipher_start_src_offset_in_bytes = 0;
		imb_set_session(mb_mgr, job);
	} else {
		job->hash_start_src_offset_in_bytes =
				op->sym->aead.data.offset;
		job->msg_len_to_hash_in_bytes =
				op->sym->aead.data.length;
		job->cipher_start_src_offset_in_bytes =
			op->sym->aead.data.offset;
		job->msg_len_to_cipher_in_bytes = op->sym->aead.data.length;
	}

	if (session->auth.operation == RTE_CRYPTO_AUTH_OP_VERIFY) {
		job->auth_tag_output = qp_data->temp_digests[*digest_idx];
		*digest_idx = (*digest_idx + 1) % IMB_MAX_JOBS;
	} else {
		job->auth_tag_output = op->sym->aead.digest.data;
	}

	job->iv = rte_crypto_op_ctod_offset(op, uint8_t *,
			session->iv.offset);

	/* Set user data to be crypto operation data struct */
	job->user_data = op;

	if (sgl) {
		job->src = NULL;
		job->dst = NULL;

		if (m_src->nb_segs <= MAX_NUM_SEGS)
			return single_sgl_job(job, op, oop,
					m_offset, m_src, m_dst,
					qp_data->sgl_segs);
		else
			return multi_sgl_job(job, op, oop,
					m_offset, m_src, m_dst, mb_mgr);
	} else {
		job->src = rte_pktmbuf_mtod(m_src, uint8_t *);
		job->dst = rte_pktmbuf_mtod_offset(m_dst, uint8_t *, m_offset);
	}

	return 0;
}

/** Check if conditions are met for digest-appended operations */
static uint8_t *
aesni_mb_digest_appended_in_src(struct rte_crypto_op *op, IMB_JOB *job,
		uint32_t oop)
{
	unsigned int auth_size, cipher_size;
	uint8_t *end_cipher;
	uint8_t *start_cipher;

	if (job->cipher_mode == IMB_CIPHER_NULL)
		return NULL;

	if (job->cipher_mode == IMB_CIPHER_ZUC_EEA3 ||
		job->cipher_mode == IMB_CIPHER_SNOW3G_UEA2_BITLEN ||
		job->cipher_mode == IMB_CIPHER_KASUMI_UEA1_BITLEN) {
		cipher_size = (op->sym->cipher.data.offset >> 3) +
			(op->sym->cipher.data.length >> 3);
	} else {
		cipher_size = (op->sym->cipher.data.offset) +
			(op->sym->cipher.data.length);
	}
	if (job->hash_alg == IMB_AUTH_ZUC_EIA3_BITLEN ||
		job->hash_alg == IMB_AUTH_SNOW3G_UIA2_BITLEN ||
		job->hash_alg == IMB_AUTH_KASUMI_UIA1 ||
		job->hash_alg == IMB_AUTH_ZUC256_EIA3_BITLEN) {
		auth_size = (op->sym->auth.data.offset >> 3) +
			(op->sym->auth.data.length >> 3);
	} else {
		auth_size = (op->sym->auth.data.offset) +
			(op->sym->auth.data.length);
	}

	if (!oop) {
		end_cipher = rte_pktmbuf_mtod_offset(op->sym->m_src, uint8_t *, cipher_size);
		start_cipher = rte_pktmbuf_mtod(op->sym->m_src, uint8_t *);
	} else {
		end_cipher = rte_pktmbuf_mtod_offset(op->sym->m_dst, uint8_t *, cipher_size);
		start_cipher = rte_pktmbuf_mtod(op->sym->m_dst, uint8_t *);
	}

	if (start_cipher < op->sym->auth.digest.data &&
		op->sym->auth.digest.data < end_cipher) {
		return rte_pktmbuf_mtod_offset(op->sym->m_src, uint8_t *, auth_size);
	} else {
		return NULL;
	}
}

/**
 * Process a crypto operation and complete a IMB_JOB job structure for
 * submission to the multi buffer library for processing.
 *
 * @param	qp		queue pair
 * @param	job		IMB_JOB structure to fill
 * @param	op		crypto op to process
 * @param	digest_idx	ID for digest to use
 *
 * @return
 * - 0 on success, the IMB_JOB will be filled
 * - -1 if invalid session or errors allocating SGL linear buffer,
 *   IMB_JOB will not be filled
 */
static inline int
set_mb_job_params(IMB_JOB *job, struct ipsec_mb_qp *qp,
		struct rte_crypto_op *op, uint8_t *digest_idx,
		IMB_MGR *mb_mgr, pid_t pid)
{
	struct rte_mbuf *m_src = op->sym->m_src, *m_dst;
	struct aesni_mb_qp_data *qp_data = ipsec_mb_get_qp_private_data(qp);
	struct aesni_mb_session *session;
	uint32_t m_offset;
	int oop;
	uint32_t auth_off_in_bytes;
	uint32_t ciph_off_in_bytes;
	uint32_t auth_len_in_bytes;
	uint32_t ciph_len_in_bytes;
	uint8_t sgl = 0;
	uint8_t lb_sgl = 0;

	session = ipsec_mb_get_session_private(qp, op);
	if (session == NULL) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
		return -1;
	}

	const IMB_CIPHER_MODE cipher_mode =
			session->template_job.cipher_mode;

	if (session->pid != pid) {
		memcpy(job, &session->template_job, sizeof(IMB_JOB));
		imb_set_session(mb_mgr, job);
	} else if (job->session_id != session->session_id)
		memcpy(job, &session->template_job, sizeof(IMB_JOB));

	if (!op->sym->m_dst) {
		/* in-place operation */
		m_dst = m_src;
		oop = 0;
	} else if (op->sym->m_dst == op->sym->m_src) {
		/* in-place operation */
		m_dst = m_src;
		oop = 0;
	} else {
		/* out-of-place operation */
		m_dst = op->sym->m_dst;
		oop = 1;
	}

	if (m_src->nb_segs > 1 || m_dst->nb_segs > 1) {
		sgl = 1;
		if (!imb_lib_support_sgl_algo(cipher_mode))
			lb_sgl = 1;
	}

	if (cipher_mode == IMB_CIPHER_GCM)
		return set_gcm_job(mb_mgr, job, sgl, qp_data,
				op, digest_idx, session, m_src, m_dst, oop);

	/* Set authentication parameters */
	const int aead = is_aead_algo(job->hash_alg, cipher_mode);

	switch (job->hash_alg) {
	case IMB_AUTH_AES_CCM:
		job->u.CCM.aad = op->sym->aead.aad.data + 18;
		break;

	case IMB_AUTH_AES_GMAC:
		job->u.GCM.aad = op->sym->aead.aad.data;
		if (sgl) {
			job->u.GCM.ctx = &qp_data->gcm_sgl_ctx;
			job->cipher_mode = IMB_CIPHER_GCM_SGL;
			job->hash_alg = IMB_AUTH_GCM_SGL;
			imb_set_session(mb_mgr, job);
		}
		break;
	case IMB_AUTH_AES_GMAC_128:
	case IMB_AUTH_AES_GMAC_192:
	case IMB_AUTH_AES_GMAC_256:
		job->u.GMAC._iv = rte_crypto_op_ctod_offset(op, uint8_t *,
						session->auth_iv.offset);
		break;
	case IMB_AUTH_ZUC_EIA3_BITLEN:
	case IMB_AUTH_ZUC256_EIA3_BITLEN:
		job->u.ZUC_EIA3._iv = rte_crypto_op_ctod_offset(op, uint8_t *,
						session->auth_iv.offset);
		break;
	case IMB_AUTH_SNOW3G_UIA2_BITLEN:
		job->u.SNOW3G_UIA2._iv =
			rte_crypto_op_ctod_offset(op, uint8_t *,
						session->auth_iv.offset);
		break;
	case IMB_AUTH_CHACHA20_POLY1305:
		job->u.CHACHA20_POLY1305.aad = op->sym->aead.aad.data;
		if (sgl) {
			job->u.CHACHA20_POLY1305.ctx = &qp_data->chacha_sgl_ctx;
			job->cipher_mode = IMB_CIPHER_CHACHA20_POLY1305_SGL;
			job->hash_alg = IMB_AUTH_CHACHA20_POLY1305_SGL;
			imb_set_session(mb_mgr, job);
		}
		break;
#if IMB_VERSION(1, 5, 0) < IMB_VERSION_NUM
	case IMB_AUTH_SM4_GCM:
		job->u.GCM.aad = op->sym->aead.aad.data;
		break;
#endif
	default:
		break;
	}

	if (aead)
		m_offset = op->sym->aead.data.offset;
	else
		m_offset = op->sym->cipher.data.offset;

	if (cipher_mode == IMB_CIPHER_ZUC_EEA3)
		m_offset >>= 3;
	else if (cipher_mode == IMB_CIPHER_SNOW3G_UEA2_BITLEN)
		m_offset = 0;
	else if (cipher_mode == IMB_CIPHER_KASUMI_UEA1_BITLEN)
		m_offset = 0;

	/* Set digest output location */
	if (job->hash_alg != IMB_AUTH_NULL &&
			session->auth.operation == RTE_CRYPTO_AUTH_OP_VERIFY) {
		job->auth_tag_output = qp_data->temp_digests[*digest_idx];
		*digest_idx = (*digest_idx + 1) % IMB_MAX_JOBS;
	} else {
		if (aead)
			job->auth_tag_output = op->sym->aead.digest.data;
		else {
			job->auth_tag_output = aesni_mb_digest_appended_in_src(op, job, oop);
			if (job->auth_tag_output == NULL) {
				job->auth_tag_output = op->sym->auth.digest.data;
			}
		}
		if (session->auth.req_digest_len !=
				job->auth_tag_output_len_in_bytes) {
			job->auth_tag_output =
				qp_data->temp_digests[*digest_idx];
			*digest_idx = (*digest_idx + 1) % IMB_MAX_JOBS;
		}
	}
	/*
	 * Multi-buffer library current only support returning a truncated
	 * digest length as specified in the relevant IPsec RFCs
	 */

	/* Data Parameters */
	if (sgl) {
		job->src = NULL;
		job->dst = NULL;
	} else {
		job->src = rte_pktmbuf_mtod(m_src, uint8_t *);
		job->dst = rte_pktmbuf_mtod_offset(m_dst, uint8_t *, m_offset);
	}

	switch (job->hash_alg) {
	case IMB_AUTH_AES_CCM:
		job->hash_start_src_offset_in_bytes = op->sym->aead.data.offset;
		job->msg_len_to_hash_in_bytes = op->sym->aead.data.length;

		job->iv = rte_crypto_op_ctod_offset(op, uint8_t *,
			session->iv.offset + 1);
		break;

	case IMB_AUTH_AES_GMAC:
		job->hash_start_src_offset_in_bytes =
				op->sym->aead.data.offset;
		job->msg_len_to_hash_in_bytes =
				op->sym->aead.data.length;
		job->iv = rte_crypto_op_ctod_offset(op, uint8_t *,
				session->iv.offset);
		break;
	case IMB_AUTH_AES_GMAC_128:
	case IMB_AUTH_AES_GMAC_192:
	case IMB_AUTH_AES_GMAC_256:
		job->hash_start_src_offset_in_bytes =
				op->sym->auth.data.offset;
		job->msg_len_to_hash_in_bytes =
				op->sym->auth.data.length;
		break;

	case IMB_AUTH_GCM_SGL:
	case IMB_AUTH_CHACHA20_POLY1305_SGL:
		job->hash_start_src_offset_in_bytes = 0;
		job->msg_len_to_hash_in_bytes = 0;
		job->iv = rte_crypto_op_ctod_offset(op, uint8_t *,
			session->iv.offset);
		break;

	case IMB_AUTH_CHACHA20_POLY1305:
		job->hash_start_src_offset_in_bytes =
			op->sym->aead.data.offset;
		job->msg_len_to_hash_in_bytes =
					op->sym->aead.data.length;
		job->iv = rte_crypto_op_ctod_offset(op, uint8_t *,
				session->iv.offset);
		break;
	/* ZUC and SNOW3G require length in bits and offset in bytes */
	case IMB_AUTH_ZUC_EIA3_BITLEN:
	case IMB_AUTH_ZUC256_EIA3_BITLEN:
	case IMB_AUTH_SNOW3G_UIA2_BITLEN:
		auth_off_in_bytes = op->sym->auth.data.offset >> 3;
		ciph_off_in_bytes = op->sym->cipher.data.offset >> 3;
		auth_len_in_bytes = op->sym->auth.data.length >> 3;
		ciph_len_in_bytes = op->sym->cipher.data.length >> 3;

		job->hash_start_src_offset_in_bytes = auth_start_offset(
				get_src_buf(op), get_dst_buf(op),
				session, oop, auth_off_in_bytes,
				ciph_off_in_bytes, auth_len_in_bytes,
				ciph_len_in_bytes, lb_sgl);
		job->msg_len_to_hash_in_bits = op->sym->auth.data.length;

		job->iv = rte_crypto_op_ctod_offset(op, uint8_t *,
			session->iv.offset);
		break;

	/* KASUMI requires lengths and offset in bytes */
	case IMB_AUTH_KASUMI_UIA1:
		auth_off_in_bytes = op->sym->auth.data.offset >> 3;
		ciph_off_in_bytes = op->sym->cipher.data.offset >> 3;
		auth_len_in_bytes = op->sym->auth.data.length >> 3;
		ciph_len_in_bytes = op->sym->cipher.data.length >> 3;

		job->hash_start_src_offset_in_bytes = auth_start_offset(
				get_src_buf(op), get_dst_buf(op),
				session, oop, auth_off_in_bytes,
				ciph_off_in_bytes, auth_len_in_bytes,
				ciph_len_in_bytes, lb_sgl);
		job->msg_len_to_hash_in_bytes = auth_len_in_bytes;

		job->iv = rte_crypto_op_ctod_offset(op, uint8_t *,
			session->iv.offset);
		break;
#if IMB_VERSION(1, 5, 0) < IMB_VERSION_NUM
	case IMB_AUTH_SM4_GCM:
		job->hash_start_src_offset_in_bytes = 0;
		/*
		 * Adding offset as a bug exists in the IPsec MB library which is
		 * fixed after v2.0 release.
		 */
		job->src += op->sym->aead.data.offset;
		job->msg_len_to_hash_in_bytes =
					op->sym->aead.data.length;
		job->iv = rte_crypto_op_ctod_offset(op, uint8_t *,
				session->iv.offset);
		break;
#endif

	default:
		job->hash_start_src_offset_in_bytes = auth_start_offset(
				get_src_buf(op), get_dst_buf(op),
				session, oop, op->sym->auth.data.offset,
				op->sym->cipher.data.offset,
				op->sym->auth.data.length,
				op->sym->cipher.data.length, lb_sgl);
		job->msg_len_to_hash_in_bytes = op->sym->auth.data.length;

		job->iv = rte_crypto_op_ctod_offset(op, uint8_t *,
			session->iv.offset);
	}

	switch (job->cipher_mode) {
	/* ZUC requires length and offset in bytes */
	case IMB_CIPHER_ZUC_EEA3:
		job->cipher_start_src_offset_in_bytes =
					op->sym->cipher.data.offset >> 3;
		job->msg_len_to_cipher_in_bytes =
					op->sym->cipher.data.length >> 3;
		break;
	/* ZUC and SNOW3G require length and offset in bits */
	case IMB_CIPHER_SNOW3G_UEA2_BITLEN:
	case IMB_CIPHER_KASUMI_UEA1_BITLEN:
		job->cipher_start_src_offset_in_bits =
					op->sym->cipher.data.offset;
		job->msg_len_to_cipher_in_bits =
					op->sym->cipher.data.length;
		break;
	case IMB_CIPHER_GCM:
		job->cipher_start_src_offset_in_bytes =
				op->sym->aead.data.offset;
		job->msg_len_to_cipher_in_bytes = op->sym->aead.data.length;
		break;
	case IMB_CIPHER_CCM:
	case IMB_CIPHER_CHACHA20_POLY1305:
		job->cipher_start_src_offset_in_bytes =
				op->sym->aead.data.offset;
		job->msg_len_to_cipher_in_bytes = op->sym->aead.data.length;
		break;
	case IMB_CIPHER_GCM_SGL:
	case IMB_CIPHER_CHACHA20_POLY1305_SGL:
		job->msg_len_to_cipher_in_bytes = 0;
		job->cipher_start_src_offset_in_bytes = 0;
		break;
#if IMB_VERSION(1, 5, 0) < IMB_VERSION_NUM
	case IMB_CIPHER_SM4_GCM:
		job->msg_len_to_cipher_in_bytes = op->sym->aead.data.length;
		break;
#endif
	default:
		job->cipher_start_src_offset_in_bytes =
					op->sym->cipher.data.offset;
		job->msg_len_to_cipher_in_bytes = op->sym->cipher.data.length;
	}

	if (cipher_mode == IMB_CIPHER_NULL && oop) {
		memcpy(job->dst + job->cipher_start_src_offset_in_bytes,
			job->src + job->cipher_start_src_offset_in_bytes,
			job->msg_len_to_cipher_in_bytes);
	}

	/* Set user data to be crypto operation data struct */
	job->user_data = op;

	if (sgl) {

		if (lb_sgl)
			return handle_sgl_linear(job, op, m_offset, session);

		if (m_src->nb_segs <= MAX_NUM_SEGS)
			return single_sgl_job(job, op, oop,
					m_offset, m_src, m_dst,
					qp_data->sgl_segs);
		else
			return multi_sgl_job(job, op, oop,
					m_offset, m_src, m_dst, mb_mgr);
	}

	return 0;
}

/**
 * Process a crypto operation containing a security op and complete a
 * IMB_JOB job structure for submission to the multi buffer library for
 * processing.
 */
static inline int
set_sec_mb_job_params(IMB_JOB *job, struct ipsec_mb_qp *qp,
			struct rte_crypto_op *op, uint8_t *digest_idx)
{
	struct aesni_mb_qp_data *qp_data = ipsec_mb_get_qp_private_data(qp);
	struct rte_mbuf *m_src, *m_dst;
	struct rte_crypto_sym_op *sym;
	struct aesni_mb_session *session = NULL;

	if (unlikely(op->sess_type != RTE_CRYPTO_OP_SECURITY_SESSION)) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
		return -1;
	}
	session = SECURITY_GET_SESS_PRIV(op->sym->session);

	if (unlikely(session == NULL)) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
		return -1;
	}
	/* Only DOCSIS protocol operations supported now */
	if (session->template_job.cipher_mode != IMB_CIPHER_DOCSIS_SEC_BPI ||
			session->template_job.hash_alg != IMB_AUTH_DOCSIS_CRC32) {
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		return -1;
	}

	sym = op->sym;
	m_src = sym->m_src;

	if (likely(sym->m_dst == NULL || sym->m_dst == m_src)) {
		/* in-place operation */
		m_dst = m_src;
	} else {
		/* out-of-place operation not supported */
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		return -ENOTSUP;
	}

	memcpy(job, &session->template_job, sizeof(IMB_JOB));

	/* Set cipher parameters */
	job->enc_keys = session->cipher.expanded_aes_keys.encode;
	job->dec_keys = session->cipher.expanded_aes_keys.decode;

	/* Set IV parameters */
	job->iv = (uint8_t *)op + session->iv.offset;

	/* Set digest output location */
	job->auth_tag_output = qp_data->temp_digests[*digest_idx];
	*digest_idx = (*digest_idx + 1) % IMB_MAX_JOBS;

	/* Set data parameters */
	job->src = rte_pktmbuf_mtod(m_src, uint8_t *);
	job->dst = rte_pktmbuf_mtod_offset(m_dst, uint8_t *,
						sym->cipher.data.offset);

	job->cipher_start_src_offset_in_bytes = sym->cipher.data.offset;
	job->msg_len_to_cipher_in_bytes = sym->cipher.data.length;

	job->hash_start_src_offset_in_bytes = sym->auth.data.offset;
	job->msg_len_to_hash_in_bytes = sym->auth.data.length;

	job->user_data = op;

	return 0;
}

static inline void
verify_docsis_sec_crc(IMB_JOB *job, uint8_t *status)
{
	uint16_t crc_offset;
	uint8_t *crc;

	if (!job->msg_len_to_hash_in_bytes)
		return;

	crc_offset = job->hash_start_src_offset_in_bytes +
			job->msg_len_to_hash_in_bytes -
			job->cipher_start_src_offset_in_bytes;
	crc = job->dst + crc_offset;

	/* Verify CRC (at the end of the message) */
	if (!rte_memeq_timingsafe(job->auth_tag_output, crc, RTE_ETHER_CRC_LEN))
		*status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
}

static inline void
verify_digest(IMB_JOB *job, void *digest, uint16_t len, uint8_t *status)
{
	/* Verify digest if required */
	if (!rte_memeq_timingsafe(job->auth_tag_output, digest, len))
		*status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
}

static inline void
generate_digest(IMB_JOB *job, struct rte_crypto_op *op,
		struct aesni_mb_session *sess)
{
	/* No extra copy needed */
	if (likely(sess->auth.req_digest_len == job->auth_tag_output_len_in_bytes))
		return;

	/*
	 * This can only happen for HMAC, so only digest
	 * for authentication algos is required
	 */
	memcpy(op->sym->auth.digest.data, job->auth_tag_output,
			sess->auth.req_digest_len);
}

static void
post_process_sgl_linear(struct rte_crypto_op *op, IMB_JOB *job,
		struct aesni_mb_session *sess, uint8_t *linear_buf)
{

	int lb_offset = 0;
	struct rte_mbuf *m_dst = op->sym->m_dst == NULL ?
			op->sym->m_src : op->sym->m_dst;
	uint16_t total_len, dst_len;
	uint64_t auth_len;
	uint8_t *dst;

	total_len = sgl_linear_cipher_auth_len(job, &auth_len);

	if (sess->auth.operation != RTE_CRYPTO_AUTH_OP_VERIFY)
		total_len += job->auth_tag_output_len_in_bytes;

	for (; (m_dst != NULL) && (total_len - lb_offset > 0); m_dst = m_dst->next) {
		dst = rte_pktmbuf_mtod(m_dst, uint8_t *);
		dst_len = RTE_MIN(m_dst->data_len, total_len - lb_offset);
		rte_memcpy(dst, linear_buf + lb_offset, dst_len);
		lb_offset += dst_len;
	}
}

/**
 * Process a completed job and return rte_mbuf which job processed
 *
 * @param qp	Queue Pair to process
 * @param job	IMB_JOB job to process
 *
 * @return
 * - Returns processed crypto operation.
 * - Returns NULL on invalid job
 */
static inline struct rte_crypto_op *
post_process_mb_job(struct ipsec_mb_qp *qp, IMB_JOB *job)
{
	struct rte_crypto_op *op = (struct rte_crypto_op *)job->user_data;
	struct aesni_mb_session *sess = NULL;
	uint8_t *linear_buf = NULL;
	int sgl = 0;
	uint8_t oop = 0;
	uint8_t is_docsis_sec = 0;

	if (op->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION) {
		/*
		 * Assuming at this point that if it's a security type op, that
		 * this is for DOCSIS
		 */
		is_docsis_sec = 1;
		sess = SECURITY_GET_SESS_PRIV(op->sym->session);
	} else
		sess = CRYPTODEV_GET_SYM_SESS_PRIV(op->sym->session);

	if (likely(op->status == RTE_CRYPTO_OP_STATUS_NOT_PROCESSED)) {
		switch (job->status) {
		case IMB_STATUS_COMPLETED:
			op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;

			if ((op->sym->m_src->nb_segs > 1 ||
					(op->sym->m_dst != NULL &&
					op->sym->m_dst->nb_segs > 1)) &&
					!imb_lib_support_sgl_algo(job->cipher_mode)) {
				linear_buf = (uint8_t *) job->user_data2;
				sgl = 1;

				post_process_sgl_linear(op, job, sess, linear_buf);
			}

			if (job->hash_alg == IMB_AUTH_NULL)
				break;

			if (sess->auth.operation == RTE_CRYPTO_AUTH_OP_VERIFY) {
				if (is_aead_algo(job->hash_alg,
						job->cipher_mode))
					verify_digest(job,
						op->sym->aead.digest.data,
						sess->auth.req_digest_len,
						&op->status);
				else if (is_docsis_sec)
					verify_docsis_sec_crc(job,
						&op->status);
				else
					verify_digest(job,
						op->sym->auth.digest.data,
						sess->auth.req_digest_len,
						&op->status);
			} else {
				if (!op->sym->m_dst || op->sym->m_dst == op->sym->m_src) {
					/* in-place operation */
					oop = 0;
				} else { /* out-of-place operation */
					oop = 1;
				}

				/* Enable digest check */
				if (op->sym->m_src->nb_segs == 1 && op->sym->m_dst != NULL
				&& !is_aead_algo(job->hash_alg,	sess->template_job.cipher_mode) &&
				aesni_mb_digest_appended_in_src(op, job, oop) != NULL) {
					unsigned int auth_size, cipher_size;
					int unencrypted_bytes = 0;
					if (job->cipher_mode == IMB_CIPHER_SNOW3G_UEA2_BITLEN ||
						job->cipher_mode == IMB_CIPHER_KASUMI_UEA1_BITLEN ||
						job->cipher_mode == IMB_CIPHER_ZUC_EEA3) {
						cipher_size = (op->sym->cipher.data.offset >> 3) +
							(op->sym->cipher.data.length >> 3);
					} else {
						cipher_size = (op->sym->cipher.data.offset) +
							(op->sym->cipher.data.length);
					}
					if (job->hash_alg == IMB_AUTH_ZUC_EIA3_BITLEN ||
						job->hash_alg == IMB_AUTH_SNOW3G_UIA2_BITLEN ||
						job->hash_alg == IMB_AUTH_KASUMI_UIA1 ||
						job->hash_alg == IMB_AUTH_ZUC256_EIA3_BITLEN) {
						auth_size = (op->sym->auth.data.offset >> 3) +
							(op->sym->auth.data.length >> 3);
					} else {
						auth_size = (op->sym->auth.data.offset) +
						(op->sym->auth.data.length);
					}
					/* Check for unencrypted bytes in partial digest cases */
					if (job->cipher_mode != IMB_CIPHER_NULL) {
						unencrypted_bytes = auth_size +
						job->auth_tag_output_len_in_bytes - cipher_size;
					}
					if (unencrypted_bytes > 0)
						rte_memcpy(
						rte_pktmbuf_mtod_offset(op->sym->m_dst, uint8_t *,
						cipher_size),
						rte_pktmbuf_mtod_offset(op->sym->m_src, uint8_t *,
						cipher_size),
						unencrypted_bytes);
				}
				generate_digest(job, op, sess);
			}
			break;
		default:
			op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		}
		if (sgl)
			rte_free(linear_buf);
	}

	/* Free session if a session-less crypto op */
	if (op->sess_type == RTE_CRYPTO_OP_SESSIONLESS) {
		memset(sess, 0, sizeof(struct aesni_mb_session));
		rte_mempool_put(qp->sess_mp, op->sym->session);
		op->sym->session = NULL;
	}

	return op;
}

static inline void
post_process_mb_sync_job(IMB_JOB *job)
{
	uint32_t *st;

	st = job->user_data;
	st[0] = (job->status == IMB_STATUS_COMPLETED) ? 0 : EBADMSG;
}

static inline uint32_t
handle_completed_sync_jobs(IMB_JOB *job, IMB_MGR *mb_mgr)
{
	uint32_t i;

	for (i = 0; job != NULL; i++, job = IMB_GET_COMPLETED_JOB(mb_mgr))
		post_process_mb_sync_job(job);

	return i;
}

static inline uint32_t
flush_mb_sync_mgr(IMB_MGR *mb_mgr)
{
	IMB_JOB *job;

	job = IMB_FLUSH_JOB(mb_mgr);
	return handle_completed_sync_jobs(job, mb_mgr);
}

static inline IMB_JOB *
set_job_null_op(IMB_JOB *job, struct rte_crypto_op *op)
{
	job->chain_order = IMB_ORDER_HASH_CIPHER;
	job->cipher_mode = IMB_CIPHER_NULL;
	job->hash_alg = IMB_AUTH_NULL;
	job->cipher_direction = IMB_DIR_DECRYPT;

	/* Set user data to be crypto operation data struct */
	job->user_data = op;

	return job;
}

uint16_t
aesni_mb_dequeue_burst(void *queue_pair, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	struct ipsec_mb_qp *qp = queue_pair;
	IMB_MGR *mb_mgr = qp->mb_mgr;
	struct rte_crypto_op *op;
	struct rte_crypto_op *deqd_ops[IMB_MAX_BURST_SIZE];
	IMB_JOB *job;
	int retval, processed_jobs = 0;
	uint16_t i, nb_jobs;
	IMB_JOB *jobs[IMB_MAX_BURST_SIZE] = {NULL};
	pid_t pid;

	if (unlikely(nb_ops == 0 || mb_mgr == NULL))
		return 0;

	uint8_t digest_idx = qp->digest_idx;
	uint16_t burst_sz = (nb_ops > IMB_MAX_BURST_SIZE) ?
		IMB_MAX_BURST_SIZE : nb_ops;

	/*
	 * If nb_ops is greater than the max supported
	 * ipsec_mb burst size, then process in bursts of
	 * IMB_MAX_BURST_SIZE until all operations are submitted
	 */
	while (nb_ops) {
		uint16_t nb_submit_ops;
		uint16_t n = (nb_ops / burst_sz) ?
			burst_sz : nb_ops;

		if (unlikely((IMB_GET_NEXT_BURST(mb_mgr, n, jobs)) < n)) {
			/*
			 * Not enough free jobs in the queue
			 * Flush n jobs until enough jobs available
			 */
			nb_jobs = IMB_FLUSH_BURST(mb_mgr, n, jobs);
			for (i = 0; i < nb_jobs; i++) {
				job = jobs[i];

				op = post_process_mb_job(qp, job);
				if (op) {
					ops[processed_jobs++] = op;
					qp->stats.dequeued_count++;
				} else {
					qp->stats.dequeue_err_count++;
					break;
				}
			}
			nb_ops -= i;
			continue;
		}

		if (!RTE_PER_LCORE(pid))
			RTE_PER_LCORE(pid) = getpid();

		pid = RTE_PER_LCORE(pid);

		/*
		 * Get the next operations to process from ingress queue.
		 * There is no need to return the job to the IMB_MGR
		 * if there are no more operations to process, since
		 * the IMB_MGR can use that pointer again in next
		 * get_next calls.
		 */
		nb_submit_ops = rte_ring_dequeue_burst(qp->ingress_queue,
						(void **)deqd_ops, n, NULL);
		for (i = 0; i < nb_submit_ops; i++) {
			job = jobs[i];
			op = deqd_ops[i];

			if (op->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION)
				retval = set_sec_mb_job_params(job, qp, op,
							       &digest_idx);
			else
				retval = set_mb_job_params(job, qp, op,
							   &digest_idx, mb_mgr, pid);

			if (unlikely(retval != 0)) {
				qp->stats.dequeue_err_count++;
				set_job_null_op(job, op);
			}
		}

		/* Submit jobs to multi-buffer for processing */
#ifdef RTE_LIBRTE_PMD_AESNI_MB_DEBUG
		int err = 0;

		nb_jobs = IMB_SUBMIT_BURST(mb_mgr, nb_submit_ops, jobs);
		err = imb_get_errno(mb_mgr);
		if (err)
			IPSEC_MB_LOG(ERR, "%s", imb_get_strerror(err));
#else
		nb_jobs = IMB_SUBMIT_BURST_NOCHECK(mb_mgr,
						   nb_submit_ops, jobs);
#endif
		for (i = 0; i < nb_jobs; i++) {
			job = jobs[i];

			op = post_process_mb_job(qp, job);
			if (op) {
				ops[processed_jobs++] = op;
				qp->stats.dequeued_count++;
			} else {
				qp->stats.dequeue_err_count++;
				break;
			}
		}

		qp->digest_idx = digest_idx;

		if (processed_jobs < 1) {
			nb_jobs = IMB_FLUSH_BURST(mb_mgr, n, jobs);

			for (i = 0; i < nb_jobs; i++) {
				job = jobs[i];

				op = post_process_mb_job(qp, job);
				if (op) {
					ops[processed_jobs++] = op;
					qp->stats.dequeued_count++;
				} else {
					qp->stats.dequeue_err_count++;
					break;
				}
			}
		}
		nb_ops -= n;
	}

	return processed_jobs;
}

static inline int
check_crypto_sgl(union rte_crypto_sym_ofs so,
	const struct rte_crypto_sym_vec *vec, uint32_t idx)
{
	const struct rte_crypto_sgl *sgl_src = vec->src_sgl + idx;
	const struct rte_crypto_sgl *sgl_dst = vec->dest_sgl ?
		vec->dest_sgl + idx : NULL;
	/* no multi-seg support with current AESNI-MB PMD */
	if (sgl_src->num != 1 || (sgl_dst && sgl_dst->num != 1))
		return -ENOTSUP;
	else if (so.ofs.cipher.head + so.ofs.cipher.tail > sgl_src->vec[0].len)
		return -EINVAL;
	return 0;
}

static inline IMB_JOB *
submit_sync_job(IMB_MGR *mb_mgr)
{
#ifdef RTE_LIBRTE_PMD_AESNI_MB_DEBUG
	IMB_JOB *job = IMB_SUBMIT_JOB(mb_mgr);
	int err = imb_get_errno(mb_mgr);
	if (err)
		IPSEC_MB_LOG(ERR, "%s", imb_get_strerror(err));
	return job;
#else
	return IMB_SUBMIT_JOB_NOCHECK(mb_mgr);
#endif
}

static inline uint32_t
generate_sync_dgst(struct rte_crypto_sym_vec *vec,
	uint8_t *dgst[], uint32_t len)
{
	uint32_t i, k;

	for (i = 0, k = 0; i != vec->num; i++) {
		if (vec->status[i] == 0) {
			memcpy(vec->digest[i].va, dgst[i], len);
			k++;
		}
	}

	return k;
}

static inline uint32_t
verify_sync_dgst(struct rte_crypto_sym_vec *vec,
	uint8_t *dgst[], uint32_t len)
{
	uint32_t i, k;

	for (i = 0, k = 0; i != vec->num; i++) {
		if (vec->status[i] == 0) {
			if (!rte_memeq_timingsafe(vec->digest[i].va, dgst[i], len))
				vec->status[i] = EBADMSG;
			else
				k++;
		}
	}

	return k;
}

uint32_t
aesni_mb_process_bulk(struct rte_cryptodev *dev __rte_unused,
	struct rte_cryptodev_sym_session *sess, union rte_crypto_sym_ofs sofs,
	struct rte_crypto_sym_vec *vec)
{
	int32_t ret;
	uint32_t i, j, k, len;
	void *src_buf, *dst_buf = NULL;
	IMB_JOB *job;
	IMB_MGR *mb_mgr;
	struct aesni_mb_session *s = CRYPTODEV_GET_SYM_SESS_PRIV(sess);
	uint8_t tmp_dgst[vec->num][DIGEST_LENGTH_MAX];
	uint8_t *tmp_dgst_ptr[vec->num];

	/* get per-thread MB MGR, create one if needed */
	mb_mgr = get_per_thread_mb_mgr();
	if (unlikely(mb_mgr == NULL))
		return 0;

	for (i = 0, j = 0, k = 0; i != vec->num; i++) {
		ret = check_crypto_sgl(sofs, vec, i);
		if (ret != 0) {
			vec->status[i] = ret;
			continue;
		}

		src_buf = vec->src_sgl[i].vec[0].base;
		if (vec->dest_sgl != NULL && vec->dest_sgl[i].vec[0].base != NULL)
			dst_buf = vec->dest_sgl[i].vec[0].base;
		len = vec->src_sgl[i].vec[0].len;

		job = IMB_GET_NEXT_JOB(mb_mgr);
		if (job == NULL) {
			k += flush_mb_sync_mgr(mb_mgr);
			job = IMB_GET_NEXT_JOB(mb_mgr);
			RTE_ASSERT(job != NULL);
		}

		/* Submit job for processing */
		tmp_dgst_ptr[i] = tmp_dgst[i];
		set_cpu_mb_job_params(job, s, sofs, src_buf, dst_buf, len, &vec->iv[i],
			&vec->aad[i], (void **)&tmp_dgst_ptr[i], &vec->digest[i],
			&vec->status[i]);
		job = submit_sync_job(mb_mgr);
		j++;

		/* handle completed jobs */
		k += handle_completed_sync_jobs(job, mb_mgr);
	}

	/* flush remaining jobs */
	while (k != j)
		k += flush_mb_sync_mgr(mb_mgr);

	/* finish processing for successful jobs: check/update digest */
	if (k != 0 && s->template_job.hash_alg != IMB_AUTH_NULL) {
		if (s->auth.operation == RTE_CRYPTO_AUTH_OP_VERIFY)
			k = verify_sync_dgst(vec,
				tmp_dgst_ptr,
				s->auth.req_digest_len);
		else
			k = generate_sync_dgst(vec,
				tmp_dgst_ptr,
				s->auth.req_digest_len);
	}

	return k;
}

struct rte_cryptodev_ops aesni_mb_pmd_ops = {
	.dev_configure = ipsec_mb_config,
	.dev_start = ipsec_mb_start,
	.dev_stop = ipsec_mb_stop,
	.dev_close = ipsec_mb_close,

	.stats_get = ipsec_mb_stats_get,
	.stats_reset = ipsec_mb_stats_reset,

	.dev_infos_get = ipsec_mb_info_get,

	.queue_pair_setup = ipsec_mb_qp_setup,
	.queue_pair_release = ipsec_mb_qp_release,

	.sym_cpu_process = aesni_mb_process_bulk,

	.sym_session_get_size = ipsec_mb_sym_session_get_size,
	.sym_session_configure = ipsec_mb_sym_session_configure,
	.sym_session_clear = ipsec_mb_sym_session_clear
};

/**
 * Configure a aesni multi-buffer session from a security session
 * configuration
 */
static int
aesni_mb_pmd_sec_sess_create(void *dev, struct rte_security_session_conf *conf,
		struct rte_security_session *sess)
{
	void *sess_private_data = SECURITY_GET_SESS_PRIV(sess);
	struct rte_cryptodev *cdev = (struct rte_cryptodev *)dev;
	int ret;

	if (conf->action_type != RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL ||
			conf->protocol != RTE_SECURITY_PROTOCOL_DOCSIS) {
		IPSEC_MB_LOG(ERR, "Invalid security protocol");
		return -EINVAL;
	}

	ret = aesni_mb_set_docsis_sec_session_parameters(cdev, conf,
			sess_private_data);

	if (ret != 0) {
		IPSEC_MB_LOG(ERR, "Failed to configure session parameters");
		return ret;
	}

	return ret;
}

/** Clear the memory of session so it does not leave key material behind */
static int
aesni_mb_pmd_sec_sess_destroy(void *dev __rte_unused,
		struct rte_security_session *sess)
{
	void *sess_priv = SECURITY_GET_SESS_PRIV(sess);

	if (sess_priv) {
		memset(sess_priv, 0, sizeof(struct aesni_mb_session));
	}
	return 0;
}

static unsigned int
aesni_mb_pmd_sec_sess_get_size(void *device __rte_unused)
{
	return sizeof(struct aesni_mb_session);
}

/** Get security capabilities for aesni multi-buffer */
static const struct rte_security_capability *
aesni_mb_pmd_sec_capa_get(void *device __rte_unused)
{
	return aesni_mb_pmd_security_cap;
}

static struct rte_security_ops aesni_mb_pmd_sec_ops = {
		.session_create = aesni_mb_pmd_sec_sess_create,
		.session_update = NULL,
		.session_get_size = aesni_mb_pmd_sec_sess_get_size,
		.session_stats_get = NULL,
		.session_destroy = aesni_mb_pmd_sec_sess_destroy,
		.set_pkt_metadata = NULL,
		.capabilities_get = aesni_mb_pmd_sec_capa_get
};

struct rte_security_ops *rte_aesni_mb_pmd_sec_ops = &aesni_mb_pmd_sec_ops;

static int
aesni_mb_configure_dev(struct rte_cryptodev *dev)
{
	struct rte_security_ctx *security_instance;

	security_instance = rte_malloc("aesni_mb_sec",
				sizeof(struct rte_security_ctx),
				RTE_CACHE_LINE_SIZE);
	if (security_instance != NULL) {
		security_instance->device = (void *)dev;
		security_instance->ops = rte_aesni_mb_pmd_sec_ops;
		security_instance->sess_cnt = 0;
		dev->security_ctx = security_instance;

		return 0;
	}

	return -ENOMEM;
}

static int
aesni_mb_probe(struct rte_vdev_device *vdev)
{
	return ipsec_mb_create(vdev, IPSEC_MB_PMD_TYPE_AESNI_MB);
}

static struct rte_vdev_driver cryptodev_aesni_mb_pmd_drv = {
	.probe = aesni_mb_probe,
	.remove = ipsec_mb_remove
};

static struct cryptodev_driver aesni_mb_crypto_drv;

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_AESNI_MB_PMD,
	cryptodev_aesni_mb_pmd_drv);
RTE_PMD_REGISTER_ALIAS(CRYPTODEV_NAME_AESNI_MB_PMD, cryptodev_aesni_mb_pmd);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_AESNI_MB_PMD,
			"max_nb_queue_pairs=<int> socket_id=<int>");
RTE_PMD_REGISTER_CRYPTO_DRIVER(
	aesni_mb_crypto_drv,
	cryptodev_aesni_mb_pmd_drv.driver,
	pmd_driver_id_aesni_mb);

/* Constructor function to register aesni-mb PMD */
RTE_INIT(ipsec_mb_register_aesni_mb)
{
	struct ipsec_mb_internals *aesni_mb_data =
		&ipsec_mb_pmds[IPSEC_MB_PMD_TYPE_AESNI_MB];

	aesni_mb_data->caps = aesni_mb_capabilities;
	aesni_mb_data->dequeue_burst = aesni_mb_dequeue_burst;
	aesni_mb_data->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT |
			RTE_CRYPTODEV_FF_SYM_CPU_CRYPTO |
			RTE_CRYPTODEV_FF_NON_BYTE_ALIGNED_DATA |
			RTE_CRYPTODEV_FF_SYM_SESSIONLESS |
			RTE_CRYPTODEV_FF_IN_PLACE_SGL |
			RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT |
			RTE_CRYPTODEV_FF_OOP_LB_IN_SGL_OUT |
			RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT |
			RTE_CRYPTODEV_FF_SECURITY |
			RTE_CRYPTODEV_FF_DIGEST_ENCRYPTED;

	aesni_mb_data->internals_priv_size = 0;
	aesni_mb_data->ops = &aesni_mb_pmd_ops;
	aesni_mb_data->qp_priv_size = sizeof(struct aesni_mb_qp_data);
	aesni_mb_data->queue_pair_configure = NULL;
	aesni_mb_data->security_ops = &aesni_mb_pmd_sec_ops;
	aesni_mb_data->dev_config = aesni_mb_configure_dev;
	aesni_mb_data->session_configure = aesni_mb_session_configure;
	aesni_mb_data->session_priv_size = sizeof(struct aesni_mb_session);
}
