/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2017 Intel Corporation
 */

#include <intel-ipsec-mb.h>

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include <rte_bus_vdev.h>
#include <rte_malloc.h>
#include <rte_cpuflags.h>
#include <rte_per_lcore.h>
#include <rte_ether.h>

#include "aesni_mb_pmd_private.h"

#define AES_CCM_DIGEST_MIN_LEN 4
#define AES_CCM_DIGEST_MAX_LEN 16
#define HMAC_MAX_BLOCK_SIZE 128
static uint8_t cryptodev_driver_id;

/*
 * Needed to support CPU-CRYPTO API (rte_cryptodev_sym_cpu_crypto_process),
 * as we still use JOB based API even for synchronous processing.
 */
static RTE_DEFINE_PER_LCORE(MB_MGR *, sync_mb_mgr);

typedef void (*hash_one_block_t)(const void *data, void *digest);
typedef void (*aes_keyexp_t)(const void *key, void *enc_exp_keys, void *dec_exp_keys);

/**
 * Calculate the authentication pre-computes
 *
 * @param one_block_hash	Function pointer to calculate digest on ipad/opad
 * @param ipad			Inner pad output byte array
 * @param opad			Outer pad output byte array
 * @param hkey			Authentication key
 * @param hkey_len		Authentication key length
 * @param blocksize		Block size of selected hash algo
 */
static void
calculate_auth_precomputes(hash_one_block_t one_block_hash,
		uint8_t *ipad, uint8_t *opad,
		const uint8_t *hkey, uint16_t hkey_len,
		uint16_t blocksize)
{
	unsigned i, length;

	uint8_t ipad_buf[blocksize] __rte_aligned(16);
	uint8_t opad_buf[blocksize] __rte_aligned(16);

	/* Setup inner and outer pads */
	memset(ipad_buf, HMAC_IPAD_VALUE, blocksize);
	memset(opad_buf, HMAC_OPAD_VALUE, blocksize);

	/* XOR hash key with inner and outer pads */
	length = hkey_len > blocksize ? blocksize : hkey_len;

	for (i = 0; i < length; i++) {
		ipad_buf[i] ^= hkey[i];
		opad_buf[i] ^= hkey[i];
	}

	/* Compute partial hashes */
	(*one_block_hash)(ipad_buf, ipad);
	(*one_block_hash)(opad_buf, opad);

	/* Clean up stack */
	memset(ipad_buf, 0, blocksize);
	memset(opad_buf, 0, blocksize);
}

/** Get xform chain order */
static enum aesni_mb_operation
aesni_mb_get_chain_order(const struct rte_crypto_sym_xform *xform)
{
	if (xform == NULL)
		return AESNI_MB_OP_NOT_SUPPORTED;

	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		if (xform->next == NULL)
			return AESNI_MB_OP_CIPHER_ONLY;
		if (xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH)
			return AESNI_MB_OP_CIPHER_HASH;
	}

	if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		if (xform->next == NULL)
			return AESNI_MB_OP_HASH_ONLY;
		if (xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER)
			return AESNI_MB_OP_HASH_CIPHER;
	}
#if IMB_VERSION_NUM > IMB_VERSION(0, 52, 0)
	if (xform->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		if (xform->aead.op == RTE_CRYPTO_AEAD_OP_ENCRYPT) {
			/*
			 * CCM requires to hash first and cipher later
			 * when encrypting
			 */
			if (xform->aead.algo == RTE_CRYPTO_AEAD_AES_CCM)
				return AESNI_MB_OP_AEAD_HASH_CIPHER;
			else
				return AESNI_MB_OP_AEAD_CIPHER_HASH;
		} else {
			if (xform->aead.algo == RTE_CRYPTO_AEAD_AES_CCM)
				return AESNI_MB_OP_AEAD_CIPHER_HASH;
			else
				return AESNI_MB_OP_AEAD_HASH_CIPHER;
		}
	}
#else
	if (xform->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		if (xform->aead.algo == RTE_CRYPTO_AEAD_AES_CCM ||
				xform->aead.algo == RTE_CRYPTO_AEAD_AES_GCM) {
			if (xform->aead.op == RTE_CRYPTO_AEAD_OP_ENCRYPT)
				return AESNI_MB_OP_AEAD_CIPHER_HASH;
			else
				return AESNI_MB_OP_AEAD_HASH_CIPHER;
		}
	}
#endif

	return AESNI_MB_OP_NOT_SUPPORTED;
}

static inline int
is_aead_algo(JOB_HASH_ALG hash_alg, JOB_CIPHER_MODE cipher_mode)
{
#if IMB_VERSION(0, 54, 3) <= IMB_VERSION_NUM
	return (hash_alg == IMB_AUTH_CHACHA20_POLY1305 || hash_alg == AES_CCM ||
		(hash_alg == AES_GMAC && cipher_mode == GCM));
#else
	return ((hash_alg == AES_GMAC && cipher_mode == GCM) ||
		hash_alg == AES_CCM);
#endif
}

/** Set session authentication parameters */
static int
aesni_mb_set_session_auth_parameters(const MB_MGR *mb_mgr,
		struct aesni_mb_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	hash_one_block_t hash_oneblock_fn = NULL;
	unsigned int key_larger_block_size = 0;
	uint8_t hashed_key[HMAC_MAX_BLOCK_SIZE] = { 0 };
	uint32_t auth_precompute = 1;

	if (xform == NULL) {
		sess->auth.algo = NULL_HASH;
		return 0;
	}

	if (xform->type != RTE_CRYPTO_SYM_XFORM_AUTH) {
		AESNI_MB_LOG(ERR, "Crypto xform struct not of type auth");
		return -1;
	}

	/* Set IV parameters */
	sess->auth_iv.offset = xform->auth.iv.offset;
	sess->auth_iv.length = xform->auth.iv.length;

	/* Set the request digest size */
	sess->auth.req_digest_len = xform->auth.digest_length;

	/* Select auth generate/verify */
	sess->auth.operation = xform->auth.op;

	/* Set Authentication Parameters */
	if (xform->auth.algo == RTE_CRYPTO_AUTH_AES_XCBC_MAC) {
		sess->auth.algo = AES_XCBC;

		uint16_t xcbc_mac_digest_len =
			get_truncated_digest_byte_length(AES_XCBC);
		if (sess->auth.req_digest_len != xcbc_mac_digest_len) {
			AESNI_MB_LOG(ERR, "Invalid digest size\n");
			return -EINVAL;
		}
		sess->auth.gen_digest_len = sess->auth.req_digest_len;

		IMB_AES_XCBC_KEYEXP(mb_mgr, xform->auth.key.data,
				sess->auth.xcbc.k1_expanded,
				sess->auth.xcbc.k2, sess->auth.xcbc.k3);
		return 0;
	}

	if (xform->auth.algo == RTE_CRYPTO_AUTH_AES_CMAC) {
		uint32_t dust[4*15];

		sess->auth.algo = AES_CMAC;

		uint16_t cmac_digest_len = get_digest_byte_length(AES_CMAC);

		if (sess->auth.req_digest_len > cmac_digest_len) {
			AESNI_MB_LOG(ERR, "Invalid digest size\n");
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
			sess->auth.gen_digest_len = cmac_digest_len;
		else
			sess->auth.gen_digest_len = sess->auth.req_digest_len;

		IMB_AES_KEYEXP_128(mb_mgr, xform->auth.key.data,
				sess->auth.cmac.expkey, dust);
		IMB_AES_CMAC_SUBKEY_GEN_128(mb_mgr, sess->auth.cmac.expkey,
				sess->auth.cmac.skey1, sess->auth.cmac.skey2);
		return 0;
	}

	if (xform->auth.algo == RTE_CRYPTO_AUTH_AES_GMAC) {
		if (xform->auth.op == RTE_CRYPTO_AUTH_OP_GENERATE) {
			sess->cipher.direction = ENCRYPT;
			sess->chain_order = CIPHER_HASH;
		} else
			sess->cipher.direction = DECRYPT;

		sess->auth.algo = AES_GMAC;
		if (sess->auth.req_digest_len > get_digest_byte_length(AES_GMAC)) {
			AESNI_MB_LOG(ERR, "Invalid digest size\n");
			return -EINVAL;
		}
		sess->auth.gen_digest_len = sess->auth.req_digest_len;
		sess->iv.length = xform->auth.iv.length;
		sess->iv.offset = xform->auth.iv.offset;

		switch (xform->auth.key.length) {
		case AES_128_BYTES:
			IMB_AES128_GCM_PRE(mb_mgr, xform->auth.key.data,
				&sess->cipher.gcm_key);
			sess->cipher.key_length_in_bytes = AES_128_BYTES;
			break;
		case AES_192_BYTES:
			IMB_AES192_GCM_PRE(mb_mgr, xform->auth.key.data,
				&sess->cipher.gcm_key);
			sess->cipher.key_length_in_bytes = AES_192_BYTES;
			break;
		case AES_256_BYTES:
			IMB_AES256_GCM_PRE(mb_mgr, xform->auth.key.data,
				&sess->cipher.gcm_key);
			sess->cipher.key_length_in_bytes = AES_256_BYTES;
			break;
		default:
			RTE_LOG(ERR, PMD, "failed to parse test type\n");
			return -EINVAL;
		}

		return 0;
	}

#if IMB_VERSION(0, 53, 3) <= IMB_VERSION_NUM
	if (xform->auth.algo == RTE_CRYPTO_AUTH_ZUC_EIA3) {
		sess->auth.algo = IMB_AUTH_ZUC_EIA3_BITLEN;
		uint16_t zuc_eia3_digest_len =
			get_truncated_digest_byte_length(IMB_AUTH_ZUC_EIA3_BITLEN);
		if (sess->auth.req_digest_len != zuc_eia3_digest_len) {
			AESNI_MB_LOG(ERR, "Invalid digest size\n");
			return -EINVAL;
		}
		sess->auth.gen_digest_len = sess->auth.req_digest_len;

		memcpy(sess->auth.zuc_auth_key, xform->auth.key.data, 16);
		return 0;
	} else if (xform->auth.algo == RTE_CRYPTO_AUTH_SNOW3G_UIA2) {
		sess->auth.algo = IMB_AUTH_SNOW3G_UIA2_BITLEN;
		uint16_t snow3g_uia2_digest_len =
			get_truncated_digest_byte_length(IMB_AUTH_SNOW3G_UIA2_BITLEN);
		if (sess->auth.req_digest_len != snow3g_uia2_digest_len) {
			AESNI_MB_LOG(ERR, "Invalid digest size\n");
			return -EINVAL;
		}
		sess->auth.gen_digest_len = sess->auth.req_digest_len;

		IMB_SNOW3G_INIT_KEY_SCHED(mb_mgr, xform->auth.key.data,
					&sess->auth.pKeySched_snow3g_auth);
		return 0;
	} else if (xform->auth.algo == RTE_CRYPTO_AUTH_KASUMI_F9) {
		sess->auth.algo = IMB_AUTH_KASUMI_UIA1;
		uint16_t kasumi_f9_digest_len =
			get_truncated_digest_byte_length(IMB_AUTH_KASUMI_UIA1);
		if (sess->auth.req_digest_len != kasumi_f9_digest_len) {
			AESNI_MB_LOG(ERR, "Invalid digest size\n");
			return -EINVAL;
		}
		sess->auth.gen_digest_len = sess->auth.req_digest_len;

		IMB_KASUMI_INIT_F9_KEY_SCHED(mb_mgr, xform->auth.key.data,
					&sess->auth.pKeySched_kasumi_auth);
		return 0;
	}
#endif

	switch (xform->auth.algo) {
	case RTE_CRYPTO_AUTH_MD5_HMAC:
		sess->auth.algo = MD5;
		hash_oneblock_fn = mb_mgr->md5_one_block;
		break;
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		sess->auth.algo = SHA1;
		hash_oneblock_fn = mb_mgr->sha1_one_block;
		if (xform->auth.key.length > get_auth_algo_blocksize(SHA1)) {
			IMB_SHA1(mb_mgr,
				xform->auth.key.data,
				xform->auth.key.length,
				hashed_key);
			key_larger_block_size = 1;
		}
		break;
	case RTE_CRYPTO_AUTH_SHA1:
		sess->auth.algo = PLAIN_SHA1;
		auth_precompute = 0;
		break;
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
		sess->auth.algo = SHA_224;
		hash_oneblock_fn = mb_mgr->sha224_one_block;
		if (xform->auth.key.length > get_auth_algo_blocksize(SHA_224)) {
			IMB_SHA224(mb_mgr,
				xform->auth.key.data,
				xform->auth.key.length,
				hashed_key);
			key_larger_block_size = 1;
		}
		break;
	case RTE_CRYPTO_AUTH_SHA224:
		sess->auth.algo = PLAIN_SHA_224;
		auth_precompute = 0;
		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		sess->auth.algo = SHA_256;
		hash_oneblock_fn = mb_mgr->sha256_one_block;
		if (xform->auth.key.length > get_auth_algo_blocksize(SHA_256)) {
			IMB_SHA256(mb_mgr,
				xform->auth.key.data,
				xform->auth.key.length,
				hashed_key);
			key_larger_block_size = 1;
		}
		break;
	case RTE_CRYPTO_AUTH_SHA256:
		sess->auth.algo = PLAIN_SHA_256;
		auth_precompute = 0;
		break;
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
		sess->auth.algo = SHA_384;
		hash_oneblock_fn = mb_mgr->sha384_one_block;
		if (xform->auth.key.length > get_auth_algo_blocksize(SHA_384)) {
			IMB_SHA384(mb_mgr,
				xform->auth.key.data,
				xform->auth.key.length,
				hashed_key);
			key_larger_block_size = 1;
		}
		break;
	case RTE_CRYPTO_AUTH_SHA384:
		sess->auth.algo = PLAIN_SHA_384;
		auth_precompute = 0;
		break;
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		sess->auth.algo = SHA_512;
		hash_oneblock_fn = mb_mgr->sha512_one_block;
		if (xform->auth.key.length > get_auth_algo_blocksize(SHA_512)) {
			IMB_SHA512(mb_mgr,
				xform->auth.key.data,
				xform->auth.key.length,
				hashed_key);
			key_larger_block_size = 1;
		}
		break;
	case RTE_CRYPTO_AUTH_SHA512:
		sess->auth.algo = PLAIN_SHA_512;
		auth_precompute = 0;
		break;
	default:
		AESNI_MB_LOG(ERR, "Unsupported authentication algorithm selection");
		return -ENOTSUP;
	}
	uint16_t trunc_digest_size =
			get_truncated_digest_byte_length(sess->auth.algo);
	uint16_t full_digest_size =
			get_digest_byte_length(sess->auth.algo);

	if (sess->auth.req_digest_len > full_digest_size ||
			sess->auth.req_digest_len == 0) {
		AESNI_MB_LOG(ERR, "Invalid digest size\n");
		return -EINVAL;
	}

	if (sess->auth.req_digest_len != trunc_digest_size &&
			sess->auth.req_digest_len != full_digest_size)
		sess->auth.gen_digest_len = full_digest_size;
	else
		sess->auth.gen_digest_len = sess->auth.req_digest_len;

	/* Plain SHA does not require precompute key */
	if (auth_precompute == 0)
		return 0;

	/* Calculate Authentication precomputes */
	if (key_larger_block_size) {
		calculate_auth_precomputes(hash_oneblock_fn,
			sess->auth.pads.inner, sess->auth.pads.outer,
			hashed_key,
			xform->auth.key.length,
			get_auth_algo_blocksize(sess->auth.algo));
	} else {
		calculate_auth_precomputes(hash_oneblock_fn,
			sess->auth.pads.inner, sess->auth.pads.outer,
			xform->auth.key.data,
			xform->auth.key.length,
			get_auth_algo_blocksize(sess->auth.algo));
	}

	return 0;
}

/** Set session cipher parameters */
static int
aesni_mb_set_session_cipher_parameters(const MB_MGR *mb_mgr,
		struct aesni_mb_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	uint8_t is_aes = 0;
	uint8_t is_3DES = 0;
	uint8_t is_docsis = 0;
#if IMB_VERSION(0, 53, 3) <= IMB_VERSION_NUM
	uint8_t is_zuc = 0;
	uint8_t is_snow3g = 0;
	uint8_t is_kasumi = 0;
#endif

	if (xform == NULL) {
		sess->cipher.mode = NULL_CIPHER;
		return 0;
	}

	if (xform->type != RTE_CRYPTO_SYM_XFORM_CIPHER) {
		AESNI_MB_LOG(ERR, "Crypto xform struct not of type cipher");
		return -EINVAL;
	}

	/* Select cipher direction */
	switch (xform->cipher.op) {
	case RTE_CRYPTO_CIPHER_OP_ENCRYPT:
		sess->cipher.direction = ENCRYPT;
		break;
	case RTE_CRYPTO_CIPHER_OP_DECRYPT:
		sess->cipher.direction = DECRYPT;
		break;
	default:
		AESNI_MB_LOG(ERR, "Invalid cipher operation parameter");
		return -EINVAL;
	}

	/* Select cipher mode */
	switch (xform->cipher.algo) {
	case RTE_CRYPTO_CIPHER_AES_CBC:
		sess->cipher.mode = CBC;
		is_aes = 1;
		break;
	case RTE_CRYPTO_CIPHER_AES_CTR:
		sess->cipher.mode = CNTR;
		is_aes = 1;
		break;
	case RTE_CRYPTO_CIPHER_AES_DOCSISBPI:
		sess->cipher.mode = DOCSIS_SEC_BPI;
		is_docsis = 1;
		break;
	case RTE_CRYPTO_CIPHER_DES_CBC:
		sess->cipher.mode = DES;
		break;
	case RTE_CRYPTO_CIPHER_DES_DOCSISBPI:
		sess->cipher.mode = DOCSIS_DES;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		sess->cipher.mode = DES3;
		is_3DES = 1;
		break;
#if IMB_VERSION(0, 53, 0) <= IMB_VERSION_NUM
	case RTE_CRYPTO_CIPHER_AES_ECB:
		sess->cipher.mode = ECB;
		is_aes = 1;
		break;
#endif
#if IMB_VERSION(0, 53, 3) <= IMB_VERSION_NUM
	case RTE_CRYPTO_CIPHER_ZUC_EEA3:
		sess->cipher.mode = IMB_CIPHER_ZUC_EEA3;
		is_zuc = 1;
		break;
	case RTE_CRYPTO_CIPHER_SNOW3G_UEA2:
		sess->cipher.mode = IMB_CIPHER_SNOW3G_UEA2_BITLEN;
		is_snow3g = 1;
		break;
	case RTE_CRYPTO_CIPHER_KASUMI_F8:
		sess->cipher.mode = IMB_CIPHER_KASUMI_UEA1_BITLEN;
		is_kasumi = 1;
		break;
#endif
	default:
		AESNI_MB_LOG(ERR, "Unsupported cipher mode parameter");
		return -ENOTSUP;
	}

	/* Set IV parameters */
	sess->iv.offset = xform->cipher.iv.offset;
	sess->iv.length = xform->cipher.iv.length;

	/* Check key length and choose key expansion function for AES */
	if (is_aes) {
		switch (xform->cipher.key.length) {
		case AES_128_BYTES:
			sess->cipher.key_length_in_bytes = AES_128_BYTES;
			IMB_AES_KEYEXP_128(mb_mgr, xform->cipher.key.data,
					sess->cipher.expanded_aes_keys.encode,
					sess->cipher.expanded_aes_keys.decode);
			break;
		case AES_192_BYTES:
			sess->cipher.key_length_in_bytes = AES_192_BYTES;
			IMB_AES_KEYEXP_192(mb_mgr, xform->cipher.key.data,
					sess->cipher.expanded_aes_keys.encode,
					sess->cipher.expanded_aes_keys.decode);
			break;
		case AES_256_BYTES:
			sess->cipher.key_length_in_bytes = AES_256_BYTES;
			IMB_AES_KEYEXP_256(mb_mgr, xform->cipher.key.data,
					sess->cipher.expanded_aes_keys.encode,
					sess->cipher.expanded_aes_keys.decode);
			break;
		default:
			AESNI_MB_LOG(ERR, "Invalid cipher key length");
			return -EINVAL;
		}
	} else if (is_docsis) {
		switch (xform->cipher.key.length) {
		case AES_128_BYTES:
			sess->cipher.key_length_in_bytes = AES_128_BYTES;
			IMB_AES_KEYEXP_128(mb_mgr, xform->cipher.key.data,
					sess->cipher.expanded_aes_keys.encode,
					sess->cipher.expanded_aes_keys.decode);
			break;
#if IMB_VERSION(0, 53, 3) <= IMB_VERSION_NUM
		case AES_256_BYTES:
			sess->cipher.key_length_in_bytes = AES_256_BYTES;
			IMB_AES_KEYEXP_256(mb_mgr, xform->cipher.key.data,
					sess->cipher.expanded_aes_keys.encode,
					sess->cipher.expanded_aes_keys.decode);
			break;
#endif
		default:
			AESNI_MB_LOG(ERR, "Invalid cipher key length");
			return -EINVAL;
		}
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
			AESNI_MB_LOG(ERR, "Invalid cipher key length");
			return -EINVAL;
		}

		sess->cipher.key_length_in_bytes = 24;
#if IMB_VERSION(0, 53, 3) <= IMB_VERSION_NUM
	} else if (is_zuc) {
		if (xform->cipher.key.length != 16) {
			AESNI_MB_LOG(ERR, "Invalid cipher key length");
			return -EINVAL;
		}
		sess->cipher.key_length_in_bytes = 16;
		memcpy(sess->cipher.zuc_cipher_key, xform->cipher.key.data,
			16);
	} else if (is_snow3g) {
		if (xform->cipher.key.length != 16) {
			AESNI_MB_LOG(ERR, "Invalid cipher key length");
			return -EINVAL;
		}
		sess->cipher.key_length_in_bytes = 16;
		IMB_SNOW3G_INIT_KEY_SCHED(mb_mgr, xform->cipher.key.data,
					&sess->cipher.pKeySched_snow3g_cipher);
	} else if (is_kasumi) {
		if (xform->cipher.key.length != 16) {
			AESNI_MB_LOG(ERR, "Invalid cipher key length");
			return -EINVAL;
		}
		sess->cipher.key_length_in_bytes = 16;
		IMB_KASUMI_INIT_F8_KEY_SCHED(mb_mgr, xform->cipher.key.data,
					&sess->cipher.pKeySched_kasumi_cipher);
#endif
	} else {
		if (xform->cipher.key.length != 8) {
			AESNI_MB_LOG(ERR, "Invalid cipher key length");
			return -EINVAL;
		}
		sess->cipher.key_length_in_bytes = 8;

		IMB_DES_KEYSCHED(mb_mgr,
			(uint64_t *)sess->cipher.expanded_aes_keys.encode,
				xform->cipher.key.data);
		IMB_DES_KEYSCHED(mb_mgr,
			(uint64_t *)sess->cipher.expanded_aes_keys.decode,
				xform->cipher.key.data);
	}

	return 0;
}

static int
aesni_mb_set_session_aead_parameters(const MB_MGR *mb_mgr,
		struct aesni_mb_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	switch (xform->aead.op) {
	case RTE_CRYPTO_AEAD_OP_ENCRYPT:
		sess->cipher.direction = ENCRYPT;
		sess->auth.operation = RTE_CRYPTO_AUTH_OP_GENERATE;
		break;
	case RTE_CRYPTO_AEAD_OP_DECRYPT:
		sess->cipher.direction = DECRYPT;
		sess->auth.operation = RTE_CRYPTO_AUTH_OP_VERIFY;
		break;
	default:
		AESNI_MB_LOG(ERR, "Invalid aead operation parameter");
		return -EINVAL;
	}

	/* Set IV parameters */
	sess->iv.offset = xform->aead.iv.offset;
	sess->iv.length = xform->aead.iv.length;

	/* Set digest sizes */
	sess->auth.req_digest_len = xform->aead.digest_length;
	sess->auth.gen_digest_len = sess->auth.req_digest_len;

	switch (xform->aead.algo) {
	case RTE_CRYPTO_AEAD_AES_CCM:
		sess->cipher.mode = CCM;
		sess->auth.algo = AES_CCM;

		/* Check key length and choose key expansion function for AES */
		switch (xform->aead.key.length) {
		case AES_128_BYTES:
			sess->cipher.key_length_in_bytes = AES_128_BYTES;
			IMB_AES_KEYEXP_128(mb_mgr, xform->aead.key.data,
					sess->cipher.expanded_aes_keys.encode,
					sess->cipher.expanded_aes_keys.decode);
			break;
		case AES_256_BYTES:
			sess->cipher.key_length_in_bytes = AES_256_BYTES;
			IMB_AES_KEYEXP_256(mb_mgr, xform->aead.key.data,
					sess->cipher.expanded_aes_keys.encode,
					sess->cipher.expanded_aes_keys.decode);
			break;
		default:
			AESNI_MB_LOG(ERR, "Invalid cipher key length");
			return -EINVAL;
		}

		/* CCM digests must be between 4 and 16 and an even number */
		if (sess->auth.req_digest_len < AES_CCM_DIGEST_MIN_LEN ||
				sess->auth.req_digest_len > AES_CCM_DIGEST_MAX_LEN ||
				(sess->auth.req_digest_len & 1) == 1) {
			AESNI_MB_LOG(ERR, "Invalid digest size\n");
			return -EINVAL;
		}
		break;

	case RTE_CRYPTO_AEAD_AES_GCM:
		sess->cipher.mode = GCM;
		sess->auth.algo = AES_GMAC;

		switch (xform->aead.key.length) {
		case AES_128_BYTES:
			sess->cipher.key_length_in_bytes = AES_128_BYTES;
			IMB_AES128_GCM_PRE(mb_mgr, xform->aead.key.data,
				&sess->cipher.gcm_key);
			break;
		case AES_192_BYTES:
			sess->cipher.key_length_in_bytes = AES_192_BYTES;
			IMB_AES192_GCM_PRE(mb_mgr, xform->aead.key.data,
				&sess->cipher.gcm_key);
			break;
		case AES_256_BYTES:
			sess->cipher.key_length_in_bytes = AES_256_BYTES;
			IMB_AES256_GCM_PRE(mb_mgr, xform->aead.key.data,
				&sess->cipher.gcm_key);
			break;
		default:
			AESNI_MB_LOG(ERR, "Invalid cipher key length");
			return -EINVAL;
		}

		/* GCM digest size must be between 1 and 16 */
		if (sess->auth.req_digest_len == 0 ||
				sess->auth.req_digest_len > 16) {
			AESNI_MB_LOG(ERR, "Invalid digest size\n");
			return -EINVAL;
		}
		break;

#if IMB_VERSION(0, 54, 3) <= IMB_VERSION_NUM
	case RTE_CRYPTO_AEAD_CHACHA20_POLY1305:
		sess->cipher.mode = IMB_CIPHER_CHACHA20_POLY1305;
		sess->auth.algo = IMB_AUTH_CHACHA20_POLY1305;

		if (xform->aead.key.length != 32) {
			AESNI_MB_LOG(ERR, "Invalid key length");
			return -EINVAL;
		}
		sess->cipher.key_length_in_bytes = 32;
		memcpy(sess->cipher.expanded_aes_keys.encode,
			xform->aead.key.data, 32);
		if (sess->auth.req_digest_len != 16) {
			AESNI_MB_LOG(ERR, "Invalid digest size\n");
			return -EINVAL;
		}
		break;
#endif
	default:
		AESNI_MB_LOG(ERR, "Unsupported aead mode parameter");
		return -ENOTSUP;
	}

	return 0;
}

/** Parse crypto xform chain and set private session parameters */
int
aesni_mb_set_session_parameters(const MB_MGR *mb_mgr,
		struct aesni_mb_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	const struct rte_crypto_sym_xform *auth_xform = NULL;
	const struct rte_crypto_sym_xform *cipher_xform = NULL;
	const struct rte_crypto_sym_xform *aead_xform = NULL;
	int ret;

	/* Select Crypto operation - hash then cipher / cipher then hash */
	switch (aesni_mb_get_chain_order(xform)) {
	case AESNI_MB_OP_HASH_CIPHER:
		sess->chain_order = HASH_CIPHER;
		auth_xform = xform;
		cipher_xform = xform->next;
		break;
	case AESNI_MB_OP_CIPHER_HASH:
		sess->chain_order = CIPHER_HASH;
		auth_xform = xform->next;
		cipher_xform = xform;
		break;
	case AESNI_MB_OP_HASH_ONLY:
		sess->chain_order = HASH_CIPHER;
		auth_xform = xform;
		cipher_xform = NULL;
		break;
	case AESNI_MB_OP_CIPHER_ONLY:
		/*
		 * Multi buffer library operates only at two modes,
		 * CIPHER_HASH and HASH_CIPHER. When doing ciphering only,
		 * chain order depends on cipher operation: encryption is always
		 * the first operation and decryption the last one.
		 */
		if (xform->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT)
			sess->chain_order = CIPHER_HASH;
		else
			sess->chain_order = HASH_CIPHER;
		auth_xform = NULL;
		cipher_xform = xform;
		break;
	case AESNI_MB_OP_AEAD_CIPHER_HASH:
		sess->chain_order = CIPHER_HASH;
		sess->aead.aad_len = xform->aead.aad_length;
		aead_xform = xform;
		break;
	case AESNI_MB_OP_AEAD_HASH_CIPHER:
		sess->chain_order = HASH_CIPHER;
		sess->aead.aad_len = xform->aead.aad_length;
		aead_xform = xform;
		break;
	case AESNI_MB_OP_NOT_SUPPORTED:
	default:
		AESNI_MB_LOG(ERR, "Unsupported operation chain order parameter");
		return -ENOTSUP;
	}

	/* Default IV length = 0 */
	sess->iv.length = 0;
	sess->auth_iv.length = 0;

	ret = aesni_mb_set_session_auth_parameters(mb_mgr, sess, auth_xform);
	if (ret != 0) {
		AESNI_MB_LOG(ERR, "Invalid/unsupported authentication parameters");
		return ret;
	}

	ret = aesni_mb_set_session_cipher_parameters(mb_mgr, sess,
			cipher_xform);
	if (ret != 0) {
		AESNI_MB_LOG(ERR, "Invalid/unsupported cipher parameters");
		return ret;
	}

	if (aead_xform) {
		ret = aesni_mb_set_session_aead_parameters(mb_mgr, sess,
				aead_xform);
		if (ret != 0) {
			AESNI_MB_LOG(ERR, "Invalid/unsupported aead parameters");
			return ret;
		}
	}

	return 0;
}

#ifdef AESNI_MB_DOCSIS_SEC_ENABLED
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
		    (crypto_sym->cipher.key.length == IMB_KEY_AES_128_BYTES ||
		     crypto_sym->cipher.key.length == IMB_KEY_AES_256_BYTES) &&
		    crypto_sym->cipher.iv.length == AES_BLOCK_SIZE &&
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
		    (crypto_sym->cipher.key.length == IMB_KEY_AES_128_BYTES ||
		     crypto_sym->cipher.key.length == IMB_KEY_AES_256_BYTES) &&
		    crypto_sym->cipher.iv.length == AES_BLOCK_SIZE &&
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
		AESNI_MB_LOG(ERR, "Invalid DOCSIS xform");
		return -EINVAL;
	}

	/* Select CRC generate/verify */
	if (xform->direction == RTE_SECURITY_DOCSIS_UPLINK) {
		sess->auth.algo = IMB_AUTH_DOCSIS_CRC32;
		sess->auth.operation = RTE_CRYPTO_AUTH_OP_VERIFY;
	} else if (xform->direction == RTE_SECURITY_DOCSIS_DOWNLINK) {
		sess->auth.algo = IMB_AUTH_DOCSIS_CRC32;
		sess->auth.operation = RTE_CRYPTO_AUTH_OP_GENERATE;
	} else {
		AESNI_MB_LOG(ERR, "Unsupported DOCSIS direction");
		return -ENOTSUP;
	}

	sess->auth.req_digest_len = RTE_ETHER_CRC_LEN;
	sess->auth.gen_digest_len = RTE_ETHER_CRC_LEN;

	return 0;
}

/**
 * Parse DOCSIS security session configuration and set private session
 * parameters
 */
int
aesni_mb_set_docsis_sec_session_parameters(
		__rte_unused struct rte_cryptodev *dev,
		struct rte_security_session_conf *conf,
		void *sess)
{
	struct rte_security_docsis_xform *docsis_xform;
	struct rte_crypto_sym_xform *cipher_xform;
	struct aesni_mb_session *aesni_sess = sess;
	struct aesni_mb_private *internals = dev->data->dev_private;
	int ret;

	ret = check_docsis_sec_session(conf);
	if (ret) {
		AESNI_MB_LOG(ERR, "Unsupported DOCSIS security configuration");
		return ret;
	}

	switch (conf->docsis.direction) {
	case RTE_SECURITY_DOCSIS_UPLINK:
		aesni_sess->chain_order = IMB_ORDER_CIPHER_HASH;
		docsis_xform = &conf->docsis;
		cipher_xform = conf->crypto_xform;
		break;
	case RTE_SECURITY_DOCSIS_DOWNLINK:
		aesni_sess->chain_order = IMB_ORDER_HASH_CIPHER;
		cipher_xform = conf->crypto_xform;
		docsis_xform = &conf->docsis;
		break;
	default:
		return -EINVAL;
	}

	/* Default IV length = 0 */
	aesni_sess->iv.length = 0;

	ret = aesni_mb_set_docsis_sec_session_auth_parameters(aesni_sess,
			docsis_xform);
	if (ret != 0) {
		AESNI_MB_LOG(ERR, "Invalid/unsupported DOCSIS parameters");
		return -EINVAL;
	}

	ret = aesni_mb_set_session_cipher_parameters(internals->mb_mgr,
			aesni_sess, cipher_xform);

	if (ret != 0) {
		AESNI_MB_LOG(ERR, "Invalid/unsupported cipher parameters");
		return -EINVAL;
	}

	return 0;
}
#endif

/**
 * burst enqueue, place crypto operations on ingress queue for processing.
 *
 * @param __qp         Queue Pair to process
 * @param ops          Crypto operations for processing
 * @param nb_ops       Number of crypto operations for processing
 *
 * @return
 * - Number of crypto operations enqueued
 */
static uint16_t
aesni_mb_pmd_enqueue_burst(void *__qp, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	struct aesni_mb_qp *qp = __qp;

	unsigned int nb_enqueued;

	nb_enqueued = rte_ring_enqueue_burst(qp->ingress_queue,
			(void **)ops, nb_ops, NULL);

	qp->stats.enqueued_count += nb_enqueued;

	return nb_enqueued;
}

/** Get multi buffer session */
static inline struct aesni_mb_session *
get_session(struct aesni_mb_qp *qp, struct rte_crypto_op *op)
{
	struct aesni_mb_session *sess = NULL;

	if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
		if (likely(op->sym->session != NULL))
			sess = (struct aesni_mb_session *)
					get_sym_session_private_data(
					op->sym->session,
					cryptodev_driver_id);
#ifdef AESNI_MB_DOCSIS_SEC_ENABLED
	} else if (op->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION) {
		if (likely(op->sym->sec_session != NULL))
			sess = (struct aesni_mb_session *)
					get_sec_session_private_data(
						op->sym->sec_session);
#endif
	} else {
		void *_sess = rte_cryptodev_sym_session_create(qp->sess_mp);
		void *_sess_private_data = NULL;

		if (_sess == NULL)
			return NULL;

		if (rte_mempool_get(qp->sess_mp_priv,
				(void **)&_sess_private_data))
			return NULL;

		sess = (struct aesni_mb_session *)_sess_private_data;

		if (unlikely(aesni_mb_set_session_parameters(qp->mb_mgr,
				sess, op->sym->xform) != 0)) {
			rte_mempool_put(qp->sess_mp, _sess);
			rte_mempool_put(qp->sess_mp_priv, _sess_private_data);
			sess = NULL;
		}
		op->sym->session = (struct rte_cryptodev_sym_session *)_sess;
		set_sym_session_private_data(op->sym->session,
				cryptodev_driver_id, _sess_private_data);
	}

	if (unlikely(sess == NULL))
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;

	return sess;
}

static inline uint64_t
auth_start_offset(struct rte_crypto_op *op, struct aesni_mb_session *session,
		uint32_t oop)
{
	struct rte_mbuf *m_src, *m_dst;
	uint8_t *p_src, *p_dst;
	uintptr_t u_src, u_dst;
	uint32_t cipher_end, auth_end;

	/* Only cipher then hash needs special calculation. */
	if (!oop || session->chain_order != CIPHER_HASH)
		return op->sym->auth.data.offset;

	m_src = op->sym->m_src;
	m_dst = op->sym->m_dst;

	p_src = rte_pktmbuf_mtod(m_src, uint8_t *);
	p_dst = rte_pktmbuf_mtod(m_dst, uint8_t *);
	u_src = (uintptr_t)p_src;
	u_dst = (uintptr_t)p_dst + op->sym->auth.data.offset;

	/**
	 * Copy the content between cipher offset and auth offset for generating
	 * correct digest.
	 */
	if (op->sym->cipher.data.offset > op->sym->auth.data.offset)
		memcpy(p_dst + op->sym->auth.data.offset,
				p_src + op->sym->auth.data.offset,
				op->sym->cipher.data.offset -
				op->sym->auth.data.offset);

	/**
	 * Copy the content between (cipher offset + length) and (auth offset +
	 * length) for generating correct digest
	 */
	cipher_end = op->sym->cipher.data.offset + op->sym->cipher.data.length;
	auth_end = op->sym->auth.data.offset + op->sym->auth.data.length;
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
set_cpu_mb_job_params(JOB_AES_HMAC *job, struct aesni_mb_session *session,
		union rte_crypto_sym_ofs sofs, void *buf, uint32_t len,
		struct rte_crypto_va_iova_ptr *iv,
		struct rte_crypto_va_iova_ptr *aad, void *digest, void *udata)
{
	/* Set crypto operation */
	job->chain_order = session->chain_order;

	/* Set cipher parameters */
	job->cipher_direction = session->cipher.direction;
	job->cipher_mode = session->cipher.mode;

	job->aes_key_len_in_bytes = session->cipher.key_length_in_bytes;

	/* Set authentication parameters */
	job->hash_alg = session->auth.algo;
	job->iv = iv->va;

	switch (job->hash_alg) {
	case AES_XCBC:
		job->u.XCBC._k1_expanded = session->auth.xcbc.k1_expanded;
		job->u.XCBC._k2 = session->auth.xcbc.k2;
		job->u.XCBC._k3 = session->auth.xcbc.k3;

		job->aes_enc_key_expanded =
				session->cipher.expanded_aes_keys.encode;
		job->aes_dec_key_expanded =
				session->cipher.expanded_aes_keys.decode;
		break;

	case AES_CCM:
		job->u.CCM.aad = (uint8_t *)aad->va + 18;
		job->u.CCM.aad_len_in_bytes = session->aead.aad_len;
		job->aes_enc_key_expanded =
				session->cipher.expanded_aes_keys.encode;
		job->aes_dec_key_expanded =
				session->cipher.expanded_aes_keys.decode;
		job->iv++;
		break;

	case AES_CMAC:
		job->u.CMAC._key_expanded = session->auth.cmac.expkey;
		job->u.CMAC._skey1 = session->auth.cmac.skey1;
		job->u.CMAC._skey2 = session->auth.cmac.skey2;
		job->aes_enc_key_expanded =
				session->cipher.expanded_aes_keys.encode;
		job->aes_dec_key_expanded =
				session->cipher.expanded_aes_keys.decode;
		break;

	case AES_GMAC:
		if (session->cipher.mode == GCM) {
			job->u.GCM.aad = aad->va;
			job->u.GCM.aad_len_in_bytes = session->aead.aad_len;
		} else {
			/* For GMAC */
			job->u.GCM.aad = buf;
			job->u.GCM.aad_len_in_bytes = len;
			job->cipher_mode = GCM;
		}
		job->aes_enc_key_expanded = &session->cipher.gcm_key;
		job->aes_dec_key_expanded = &session->cipher.gcm_key;
		break;

#if IMB_VERSION(0, 54, 3) <= IMB_VERSION_NUM
	case IMB_AUTH_CHACHA20_POLY1305:
		job->u.CHACHA20_POLY1305.aad = aad->va;
		job->u.CHACHA20_POLY1305.aad_len_in_bytes = session->aead.aad_len;
		job->aes_enc_key_expanded = session->cipher.expanded_aes_keys.encode;
		job->aes_dec_key_expanded = session->cipher.expanded_aes_keys.encode;
		break;
#endif
	default:
		job->u.HMAC._hashed_auth_key_xor_ipad =
				session->auth.pads.inner;
		job->u.HMAC._hashed_auth_key_xor_opad =
				session->auth.pads.outer;

		if (job->cipher_mode == DES3) {
			job->aes_enc_key_expanded =
				session->cipher.exp_3des_keys.ks_ptr;
			job->aes_dec_key_expanded =
				session->cipher.exp_3des_keys.ks_ptr;
		} else {
			job->aes_enc_key_expanded =
				session->cipher.expanded_aes_keys.encode;
			job->aes_dec_key_expanded =
				session->cipher.expanded_aes_keys.decode;
		}
	}

	/*
	 * Multi-buffer library current only support returning a truncated
	 * digest length as specified in the relevant IPsec RFCs
	 */

	/* Set digest location and length */
	job->auth_tag_output = digest;
	job->auth_tag_output_len_in_bytes = session->auth.gen_digest_len;

	/* Set IV parameters */
	job->iv_len_in_bytes = session->iv.length;

	/* Data Parameters */
	job->src = buf;
	job->dst = (uint8_t *)buf + sofs.ofs.cipher.head;
	job->cipher_start_src_offset_in_bytes = sofs.ofs.cipher.head;
	job->hash_start_src_offset_in_bytes = sofs.ofs.auth.head;
	if (job->hash_alg == AES_GMAC && session->cipher.mode != GCM) {
		job->msg_len_to_hash_in_bytes = 0;
		job->msg_len_to_cipher_in_bytes = 0;
	} else {
		job->msg_len_to_hash_in_bytes = len - sofs.ofs.auth.head -
			sofs.ofs.auth.tail;
		job->msg_len_to_cipher_in_bytes = len - sofs.ofs.cipher.head -
			sofs.ofs.cipher.tail;
	}

	job->user_data = udata;
}

/**
 * Process a crypto operation and complete a JOB_AES_HMAC job structure for
 * submission to the multi buffer library for processing.
 *
 * @param	qp	queue pair
 * @param	job	JOB_AES_HMAC structure to fill
 * @param	m	mbuf to process
 *
 * @return
 * - Completed JOB_AES_HMAC structure pointer on success
 * - NULL pointer if completion of JOB_AES_HMAC structure isn't possible
 */
static inline int
set_mb_job_params(JOB_AES_HMAC *job, struct aesni_mb_qp *qp,
		struct rte_crypto_op *op, uint8_t *digest_idx)
{
	struct rte_mbuf *m_src = op->sym->m_src, *m_dst;
	struct aesni_mb_session *session;
	uint32_t m_offset, oop;

	session = get_session(qp, op);
	if (session == NULL) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
		return -1;
	}

	/* Set crypto operation */
	job->chain_order = session->chain_order;

	/* Set cipher parameters */
	job->cipher_direction = session->cipher.direction;
	job->cipher_mode = session->cipher.mode;

	job->aes_key_len_in_bytes = session->cipher.key_length_in_bytes;

	/* Set authentication parameters */
	job->hash_alg = session->auth.algo;

	const int aead = is_aead_algo(job->hash_alg, job->cipher_mode);

	switch (job->hash_alg) {
	case AES_XCBC:
		job->u.XCBC._k1_expanded = session->auth.xcbc.k1_expanded;
		job->u.XCBC._k2 = session->auth.xcbc.k2;
		job->u.XCBC._k3 = session->auth.xcbc.k3;

		job->aes_enc_key_expanded =
				session->cipher.expanded_aes_keys.encode;
		job->aes_dec_key_expanded =
				session->cipher.expanded_aes_keys.decode;
		break;

	case AES_CCM:
		job->u.CCM.aad = op->sym->aead.aad.data + 18;
		job->u.CCM.aad_len_in_bytes = session->aead.aad_len;
		job->aes_enc_key_expanded =
				session->cipher.expanded_aes_keys.encode;
		job->aes_dec_key_expanded =
				session->cipher.expanded_aes_keys.decode;
		break;

	case AES_CMAC:
		job->u.CMAC._key_expanded = session->auth.cmac.expkey;
		job->u.CMAC._skey1 = session->auth.cmac.skey1;
		job->u.CMAC._skey2 = session->auth.cmac.skey2;
		job->aes_enc_key_expanded =
				session->cipher.expanded_aes_keys.encode;
		job->aes_dec_key_expanded =
				session->cipher.expanded_aes_keys.decode;
		break;

	case AES_GMAC:
		if (session->cipher.mode == GCM) {
			job->u.GCM.aad = op->sym->aead.aad.data;
			job->u.GCM.aad_len_in_bytes = session->aead.aad_len;
		} else {
			/* For GMAC */
			job->u.GCM.aad = rte_pktmbuf_mtod_offset(m_src,
					uint8_t *, op->sym->auth.data.offset);
			job->u.GCM.aad_len_in_bytes = op->sym->auth.data.length;
			job->cipher_mode = GCM;
		}
		job->aes_enc_key_expanded = &session->cipher.gcm_key;
		job->aes_dec_key_expanded = &session->cipher.gcm_key;
		break;
#if IMB_VERSION(0, 53, 3) <= IMB_VERSION_NUM
	case IMB_AUTH_ZUC_EIA3_BITLEN:
		job->u.ZUC_EIA3._key = session->auth.zuc_auth_key;
		job->u.ZUC_EIA3._iv = rte_crypto_op_ctod_offset(op, uint8_t *,
						session->auth_iv.offset);
		break;
	case IMB_AUTH_SNOW3G_UIA2_BITLEN:
		job->u.SNOW3G_UIA2._key = (void *) &session->auth.pKeySched_snow3g_auth;
		job->u.SNOW3G_UIA2._iv = rte_crypto_op_ctod_offset(op, uint8_t *,
						session->auth_iv.offset);
		break;
	case IMB_AUTH_KASUMI_UIA1:
		job->u.KASUMI_UIA1._key = (void *) &session->auth.pKeySched_kasumi_auth;
		break;
#endif
#if IMB_VERSION(0, 54, 3) <= IMB_VERSION_NUM
	case IMB_AUTH_CHACHA20_POLY1305:
		job->u.CHACHA20_POLY1305.aad = op->sym->aead.aad.data;
		job->u.CHACHA20_POLY1305.aad_len_in_bytes = session->aead.aad_len;
		job->aes_enc_key_expanded = session->cipher.expanded_aes_keys.encode;
		job->aes_dec_key_expanded = session->cipher.expanded_aes_keys.encode;
		break;
#endif
	default:
		job->u.HMAC._hashed_auth_key_xor_ipad = session->auth.pads.inner;
		job->u.HMAC._hashed_auth_key_xor_opad = session->auth.pads.outer;

		if (job->cipher_mode == DES3) {
			job->aes_enc_key_expanded =
				session->cipher.exp_3des_keys.ks_ptr;
			job->aes_dec_key_expanded =
				session->cipher.exp_3des_keys.ks_ptr;
		} else {
			job->aes_enc_key_expanded =
				session->cipher.expanded_aes_keys.encode;
			job->aes_dec_key_expanded =
				session->cipher.expanded_aes_keys.decode;
		}
	}

	if (aead)
		m_offset = op->sym->aead.data.offset;
	else
		m_offset = op->sym->cipher.data.offset;

#if IMB_VERSION(0, 53, 3) <= IMB_VERSION_NUM
	if (job->cipher_mode == IMB_CIPHER_ZUC_EEA3) {
		job->aes_enc_key_expanded = session->cipher.zuc_cipher_key;
		job->aes_dec_key_expanded = session->cipher.zuc_cipher_key;
	} else if (job->cipher_mode == IMB_CIPHER_SNOW3G_UEA2_BITLEN) {
		job->enc_keys = &session->cipher.pKeySched_snow3g_cipher;
		m_offset = 0;
	} else if (job->cipher_mode == IMB_CIPHER_KASUMI_UEA1_BITLEN) {
		job->enc_keys = &session->cipher.pKeySched_kasumi_cipher;
		m_offset = 0;
	}
#endif

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

	/* Set digest output location */
	if (job->hash_alg != NULL_HASH &&
			session->auth.operation == RTE_CRYPTO_AUTH_OP_VERIFY) {
		job->auth_tag_output = qp->temp_digests[*digest_idx];
		*digest_idx = (*digest_idx + 1) % MAX_JOBS;
	} else {
		if (aead)
			job->auth_tag_output = op->sym->aead.digest.data;
		else
			job->auth_tag_output = op->sym->auth.digest.data;

		if (session->auth.req_digest_len != session->auth.gen_digest_len) {
			job->auth_tag_output = qp->temp_digests[*digest_idx];
			*digest_idx = (*digest_idx + 1) % MAX_JOBS;
		}
	}
	/*
	 * Multi-buffer library current only support returning a truncated
	 * digest length as specified in the relevant IPsec RFCs
	 */

	/* Set digest length */
	job->auth_tag_output_len_in_bytes = session->auth.gen_digest_len;

	/* Set IV parameters */
	job->iv_len_in_bytes = session->iv.length;

	/* Data Parameters */
	job->src = rte_pktmbuf_mtod(m_src, uint8_t *);
	job->dst = rte_pktmbuf_mtod_offset(m_dst, uint8_t *, m_offset);

	switch (job->hash_alg) {
	case AES_CCM:
		job->cipher_start_src_offset_in_bytes =
				op->sym->aead.data.offset;
		job->msg_len_to_cipher_in_bytes = op->sym->aead.data.length;
		job->hash_start_src_offset_in_bytes = op->sym->aead.data.offset;
		job->msg_len_to_hash_in_bytes = op->sym->aead.data.length;

		job->iv = rte_crypto_op_ctod_offset(op, uint8_t *,
			session->iv.offset + 1);
		break;

	case AES_GMAC:
		if (session->cipher.mode == GCM) {
			job->cipher_start_src_offset_in_bytes =
					op->sym->aead.data.offset;
			job->hash_start_src_offset_in_bytes =
					op->sym->aead.data.offset;
			job->msg_len_to_cipher_in_bytes =
					op->sym->aead.data.length;
			job->msg_len_to_hash_in_bytes =
					op->sym->aead.data.length;
		} else {
			job->cipher_start_src_offset_in_bytes =
					op->sym->auth.data.offset;
			job->hash_start_src_offset_in_bytes =
					op->sym->auth.data.offset;
			job->msg_len_to_cipher_in_bytes = 0;
			job->msg_len_to_hash_in_bytes = 0;
		}

		job->iv = rte_crypto_op_ctod_offset(op, uint8_t *,
				session->iv.offset);
		break;

#if IMB_VERSION(0, 54, 3) <= IMB_VERSION_NUM
	case IMB_AUTH_CHACHA20_POLY1305:
		job->cipher_start_src_offset_in_bytes = op->sym->aead.data.offset;
		job->hash_start_src_offset_in_bytes = op->sym->aead.data.offset;
		job->msg_len_to_cipher_in_bytes =
				op->sym->aead.data.length;
		job->msg_len_to_hash_in_bytes =
					op->sym->aead.data.length;

		job->iv = rte_crypto_op_ctod_offset(op, uint8_t *,
				session->iv.offset);
		break;
#endif
	default:
		/* For SNOW3G, length and offsets are already in bits */
		job->cipher_start_src_offset_in_bytes =
				op->sym->cipher.data.offset;
		job->msg_len_to_cipher_in_bytes = op->sym->cipher.data.length;

		job->hash_start_src_offset_in_bytes = auth_start_offset(op,
				session, oop);
		job->msg_len_to_hash_in_bytes = op->sym->auth.data.length;

		job->iv = rte_crypto_op_ctod_offset(op, uint8_t *,
			session->iv.offset);
	}

#if IMB_VERSION(0, 53, 3) <= IMB_VERSION_NUM
	if (job->cipher_mode == IMB_CIPHER_ZUC_EEA3)
		job->msg_len_to_cipher_in_bytes >>= 3;
	else if (job->hash_alg == IMB_AUTH_KASUMI_UIA1)
		job->msg_len_to_hash_in_bytes >>= 3;
#endif

	/* Set user data to be crypto operation data struct */
	job->user_data = op;

	return 0;
}

#ifdef AESNI_MB_DOCSIS_SEC_ENABLED
/**
 * Process a crypto operation containing a security op and complete a
 * JOB_AES_HMAC job structure for submission to the multi buffer library for
 * processing.
 */
static inline int
set_sec_mb_job_params(JOB_AES_HMAC *job, struct aesni_mb_qp *qp,
		struct rte_crypto_op *op, uint8_t *digest_idx)
{
	struct rte_mbuf *m_src, *m_dst;
	struct rte_crypto_sym_op *sym;
	struct aesni_mb_session *session;

	session = get_session(qp, op);
	if (unlikely(session == NULL)) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
		return -1;
	}

	/* Only DOCSIS protocol operations supported now */
	if (session->cipher.mode != IMB_CIPHER_DOCSIS_SEC_BPI ||
			session->auth.algo != IMB_AUTH_DOCSIS_CRC32) {
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

	/* Set crypto operation */
	job->chain_order = session->chain_order;

	/* Set cipher parameters */
	job->cipher_direction = session->cipher.direction;
	job->cipher_mode = session->cipher.mode;

	job->aes_key_len_in_bytes = session->cipher.key_length_in_bytes;
	job->aes_enc_key_expanded = session->cipher.expanded_aes_keys.encode;
	job->aes_dec_key_expanded = session->cipher.expanded_aes_keys.decode;

	/* Set IV parameters */
	job->iv_len_in_bytes = session->iv.length;
	job->iv = (uint8_t *)op + session->iv.offset;

	/* Set authentication parameters */
	job->hash_alg = session->auth.algo;

	/* Set digest output location */
	job->auth_tag_output = qp->temp_digests[*digest_idx];
	*digest_idx = (*digest_idx + 1) % MAX_JOBS;

	/* Set digest length */
	job->auth_tag_output_len_in_bytes = session->auth.gen_digest_len;

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
verify_docsis_sec_crc(JOB_AES_HMAC *job, uint8_t *status)
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
	if (memcmp(job->auth_tag_output, crc, RTE_ETHER_CRC_LEN) != 0)
		*status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
}
#endif

static inline void
verify_digest(JOB_AES_HMAC *job, void *digest, uint16_t len, uint8_t *status)
{
	/* Verify digest if required */
	if (memcmp(job->auth_tag_output, digest, len) != 0)
		*status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
}

static inline void
generate_digest(JOB_AES_HMAC *job, struct rte_crypto_op *op,
		struct aesni_mb_session *sess)
{
	/* No extra copy needed */
	if (likely(sess->auth.req_digest_len == sess->auth.gen_digest_len))
		return;

	/*
	 * This can only happen for HMAC, so only digest
	 * for authentication algos is required
	 */
	memcpy(op->sym->auth.digest.data, job->auth_tag_output,
			sess->auth.req_digest_len);
}

/**
 * Process a completed job and return rte_mbuf which job processed
 *
 * @param qp		Queue Pair to process
 * @param job	JOB_AES_HMAC job to process
 *
 * @return
 * - Returns processed crypto operation.
 * - Returns NULL on invalid job
 */
static inline struct rte_crypto_op *
post_process_mb_job(struct aesni_mb_qp *qp, JOB_AES_HMAC *job)
{
	struct rte_crypto_op *op = (struct rte_crypto_op *)job->user_data;
	struct aesni_mb_session *sess = NULL;

#ifdef AESNI_MB_DOCSIS_SEC_ENABLED
	uint8_t is_docsis_sec = 0;

	if (op->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION) {
		/*
		 * Assuming at this point that if it's a security type op, that
		 * this is for DOCSIS
		 */
		is_docsis_sec = 1;
		sess = get_sec_session_private_data(op->sym->sec_session);
	} else
#endif
	{
		sess = get_sym_session_private_data(op->sym->session,
						cryptodev_driver_id);
	}

	if (unlikely(sess == NULL)) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
		return op;
	}

	if (likely(op->status == RTE_CRYPTO_OP_STATUS_NOT_PROCESSED)) {
		switch (job->status) {
		case STS_COMPLETED:
			op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;

			if (job->hash_alg == NULL_HASH)
				break;

			if (sess->auth.operation == RTE_CRYPTO_AUTH_OP_VERIFY) {
				if (is_aead_algo(job->hash_alg, sess->cipher.mode))
					verify_digest(job,
						op->sym->aead.digest.data,
						sess->auth.req_digest_len,
						&op->status);
#ifdef AESNI_MB_DOCSIS_SEC_ENABLED
				else if (is_docsis_sec)
					verify_docsis_sec_crc(job,
						&op->status);
#endif
				else
					verify_digest(job,
						op->sym->auth.digest.data,
						sess->auth.req_digest_len,
						&op->status);
			} else
				generate_digest(job, op, sess);
			break;
		default:
			op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		}
	}

	/* Free session if a session-less crypto op */
	if (op->sess_type == RTE_CRYPTO_OP_SESSIONLESS) {
		memset(sess, 0, sizeof(struct aesni_mb_session));
		memset(op->sym->session, 0,
			rte_cryptodev_sym_get_existing_header_session_size(
				op->sym->session));
		rte_mempool_put(qp->sess_mp_priv, sess);
		rte_mempool_put(qp->sess_mp, op->sym->session);
		op->sym->session = NULL;
	}

	return op;
}

static inline void
post_process_mb_sync_job(JOB_AES_HMAC *job)
{
	uint32_t *st;

	st = job->user_data;
	st[0] = (job->status == STS_COMPLETED) ? 0 : EBADMSG;
}

/**
 * Process a completed JOB_AES_HMAC job and keep processing jobs until
 * get_completed_job return NULL
 *
 * @param qp		Queue Pair to process
 * @param job		JOB_AES_HMAC job
 *
 * @return
 * - Number of processed jobs
 */
static unsigned
handle_completed_jobs(struct aesni_mb_qp *qp, JOB_AES_HMAC *job,
		struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct rte_crypto_op *op = NULL;
	unsigned processed_jobs = 0;

	while (job != NULL) {
		op = post_process_mb_job(qp, job);

		if (op) {
			ops[processed_jobs++] = op;
			qp->stats.dequeued_count++;
		} else {
			qp->stats.dequeue_err_count++;
			break;
		}
		if (processed_jobs == nb_ops)
			break;

		job = IMB_GET_COMPLETED_JOB(qp->mb_mgr);
	}

	return processed_jobs;
}

static inline uint32_t
handle_completed_sync_jobs(JOB_AES_HMAC *job, MB_MGR *mb_mgr)
{
	uint32_t i;

	for (i = 0; job != NULL; i++, job = IMB_GET_COMPLETED_JOB(mb_mgr))
		post_process_mb_sync_job(job);

	return i;
}

static inline uint32_t
flush_mb_sync_mgr(MB_MGR *mb_mgr)
{
	JOB_AES_HMAC *job;

	job = IMB_FLUSH_JOB(mb_mgr);
	return handle_completed_sync_jobs(job, mb_mgr);
}

static inline uint16_t
flush_mb_mgr(struct aesni_mb_qp *qp, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	int processed_ops = 0;

	/* Flush the remaining jobs */
	JOB_AES_HMAC *job = IMB_FLUSH_JOB(qp->mb_mgr);

	if (job)
		processed_ops += handle_completed_jobs(qp, job,
				&ops[processed_ops], nb_ops - processed_ops);

	return processed_ops;
}

static inline JOB_AES_HMAC *
set_job_null_op(JOB_AES_HMAC *job, struct rte_crypto_op *op)
{
	job->chain_order = HASH_CIPHER;
	job->cipher_mode = NULL_CIPHER;
	job->hash_alg = NULL_HASH;
	job->cipher_direction = DECRYPT;

	/* Set user data to be crypto operation data struct */
	job->user_data = op;

	return job;
}

static uint16_t
aesni_mb_pmd_dequeue_burst(void *queue_pair, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	struct aesni_mb_qp *qp = queue_pair;

	struct rte_crypto_op *op;
	JOB_AES_HMAC *job;

	int retval, processed_jobs = 0;

	if (unlikely(nb_ops == 0))
		return 0;

	uint8_t digest_idx = qp->digest_idx;
	do {
		/* Get next free mb job struct from mb manager */
		job = IMB_GET_NEXT_JOB(qp->mb_mgr);
		if (unlikely(job == NULL)) {
			/* if no free mb job structs we need to flush mb_mgr */
			processed_jobs += flush_mb_mgr(qp,
					&ops[processed_jobs],
					nb_ops - processed_jobs);

			if (nb_ops == processed_jobs)
				break;

			job = IMB_GET_NEXT_JOB(qp->mb_mgr);
		}

		/*
		 * Get next operation to process from ingress queue.
		 * There is no need to return the job to the MB_MGR
		 * if there are no more operations to process, since the MB_MGR
		 * can use that pointer again in next get_next calls.
		 */
		retval = rte_ring_dequeue(qp->ingress_queue, (void **)&op);
		if (retval < 0)
			break;

#ifdef AESNI_MB_DOCSIS_SEC_ENABLED
		if (op->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION)
			retval = set_sec_mb_job_params(job, qp, op,
						&digest_idx);
		else
#endif
			retval = set_mb_job_params(job, qp, op, &digest_idx);

		if (unlikely(retval != 0)) {
			qp->stats.dequeue_err_count++;
			set_job_null_op(job, op);
		}

		/* Submit job to multi-buffer for processing */
#ifdef RTE_LIBRTE_PMD_AESNI_MB_DEBUG
		job = IMB_SUBMIT_JOB(qp->mb_mgr);
#else
		job = IMB_SUBMIT_JOB_NOCHECK(qp->mb_mgr);
#endif
		/*
		 * If submit returns a processed job then handle it,
		 * before submitting subsequent jobs
		 */
		if (job)
			processed_jobs += handle_completed_jobs(qp, job,
					&ops[processed_jobs],
					nb_ops - processed_jobs);

	} while (processed_jobs < nb_ops);

	qp->digest_idx = digest_idx;

	if (processed_jobs < 1)
		processed_jobs += flush_mb_mgr(qp,
				&ops[processed_jobs],
				nb_ops - processed_jobs);

	return processed_jobs;
}

static MB_MGR *
alloc_init_mb_mgr(enum aesni_mb_vector_mode vector_mode)
{
	MB_MGR *mb_mgr = alloc_mb_mgr(0);
	if (mb_mgr == NULL)
		return NULL;

	switch (vector_mode) {
	case RTE_AESNI_MB_SSE:
		init_mb_mgr_sse(mb_mgr);
		break;
	case RTE_AESNI_MB_AVX:
		init_mb_mgr_avx(mb_mgr);
		break;
	case RTE_AESNI_MB_AVX2:
		init_mb_mgr_avx2(mb_mgr);
		break;
	case RTE_AESNI_MB_AVX512:
		init_mb_mgr_avx512(mb_mgr);
		break;
	default:
		AESNI_MB_LOG(ERR, "Unsupported vector mode %u\n", vector_mode);
		free_mb_mgr(mb_mgr);
		return NULL;
	}

	return mb_mgr;
}

static inline void
aesni_mb_fill_error_code(struct rte_crypto_sym_vec *vec, int32_t err)
{
	uint32_t i;

	for (i = 0; i != vec->num; ++i)
		vec->status[i] = err;
}

static inline int
check_crypto_sgl(union rte_crypto_sym_ofs so, const struct rte_crypto_sgl *sgl)
{
	/* no multi-seg support with current AESNI-MB PMD */
	if (sgl->num != 1)
		return ENOTSUP;
	else if (so.ofs.cipher.head + so.ofs.cipher.tail > sgl->vec[0].len)
		return EINVAL;
	return 0;
}

static inline JOB_AES_HMAC *
submit_sync_job(MB_MGR *mb_mgr)
{
#ifdef RTE_LIBRTE_PMD_AESNI_MB_DEBUG
	return IMB_SUBMIT_JOB(mb_mgr);
#else
	return IMB_SUBMIT_JOB_NOCHECK(mb_mgr);
#endif
}

static inline uint32_t
generate_sync_dgst(struct rte_crypto_sym_vec *vec,
	const uint8_t dgst[][DIGEST_LENGTH_MAX], uint32_t len)
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
	const uint8_t dgst[][DIGEST_LENGTH_MAX], uint32_t len)
{
	uint32_t i, k;

	for (i = 0, k = 0; i != vec->num; i++) {
		if (vec->status[i] == 0) {
			if (memcmp(vec->digest[i].va, dgst[i], len) != 0)
				vec->status[i] = EBADMSG;
			else
				k++;
		}
	}

	return k;
}

uint32_t
aesni_mb_cpu_crypto_process_bulk(struct rte_cryptodev *dev,
	struct rte_cryptodev_sym_session *sess, union rte_crypto_sym_ofs sofs,
	struct rte_crypto_sym_vec *vec)
{
	int32_t ret;
	uint32_t i, j, k, len;
	void *buf;
	JOB_AES_HMAC *job;
	MB_MGR *mb_mgr;
	struct aesni_mb_private *priv;
	struct aesni_mb_session *s;
	uint8_t tmp_dgst[vec->num][DIGEST_LENGTH_MAX];

	s = get_sym_session_private_data(sess, dev->driver_id);
	if (s == NULL) {
		aesni_mb_fill_error_code(vec, EINVAL);
		return 0;
	}

	/* get per-thread MB MGR, create one if needed */
	mb_mgr = RTE_PER_LCORE(sync_mb_mgr);
	if (mb_mgr == NULL) {

		priv = dev->data->dev_private;
		mb_mgr = alloc_init_mb_mgr(priv->vector_mode);
		if (mb_mgr == NULL) {
			aesni_mb_fill_error_code(vec, ENOMEM);
			return 0;
		}
		RTE_PER_LCORE(sync_mb_mgr) = mb_mgr;
	}

	for (i = 0, j = 0, k = 0; i != vec->num; i++) {


		ret = check_crypto_sgl(sofs, vec->sgl + i);
		if (ret != 0) {
			vec->status[i] = ret;
			continue;
		}

		buf = vec->sgl[i].vec[0].base;
		len = vec->sgl[i].vec[0].len;

		job = IMB_GET_NEXT_JOB(mb_mgr);
		if (job == NULL) {
			k += flush_mb_sync_mgr(mb_mgr);
			job = IMB_GET_NEXT_JOB(mb_mgr);
			RTE_ASSERT(job != NULL);
		}

		/* Submit job for processing */
		set_cpu_mb_job_params(job, s, sofs, buf, len, &vec->iv[i],
			&vec->aad[i], tmp_dgst[i], &vec->status[i]);
		job = submit_sync_job(mb_mgr);
		j++;

		/* handle completed jobs */
		k += handle_completed_sync_jobs(job, mb_mgr);
	}

	/* flush remaining jobs */
	while (k != j)
		k += flush_mb_sync_mgr(mb_mgr);

	/* finish processing for successful jobs: check/update digest */
	if (k != 0) {
		if (s->auth.operation == RTE_CRYPTO_AUTH_OP_VERIFY)
			k = verify_sync_dgst(vec,
				(const uint8_t (*)[DIGEST_LENGTH_MAX])tmp_dgst,
				s->auth.req_digest_len);
		else
			k = generate_sync_dgst(vec,
				(const uint8_t (*)[DIGEST_LENGTH_MAX])tmp_dgst,
				s->auth.req_digest_len);
	}

	return k;
}

static int cryptodev_aesni_mb_remove(struct rte_vdev_device *vdev);

static uint64_t
vec_mode_to_flags(enum aesni_mb_vector_mode mode)
{
	switch (mode) {
	case RTE_AESNI_MB_SSE:
		return RTE_CRYPTODEV_FF_CPU_SSE;
	case RTE_AESNI_MB_AVX:
		return RTE_CRYPTODEV_FF_CPU_AVX;
	case RTE_AESNI_MB_AVX2:
		return RTE_CRYPTODEV_FF_CPU_AVX2;
	case RTE_AESNI_MB_AVX512:
		return RTE_CRYPTODEV_FF_CPU_AVX512;
	default:
		AESNI_MB_LOG(ERR, "Unsupported vector mode %u\n", mode);
		return 0;
	}
}

static int
cryptodev_aesni_mb_create(const char *name,
			struct rte_vdev_device *vdev,
			struct rte_cryptodev_pmd_init_params *init_params)
{
	struct rte_cryptodev *dev;
	struct aesni_mb_private *internals;
	enum aesni_mb_vector_mode vector_mode;
	MB_MGR *mb_mgr;

	dev = rte_cryptodev_pmd_create(name, &vdev->device, init_params);
	if (dev == NULL) {
		AESNI_MB_LOG(ERR, "failed to create cryptodev vdev");
		return -ENODEV;
	}

	/* Check CPU for supported vector instruction set */
	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512F))
		vector_mode = RTE_AESNI_MB_AVX512;
	else if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX2))
		vector_mode = RTE_AESNI_MB_AVX2;
	else if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX))
		vector_mode = RTE_AESNI_MB_AVX;
	else
		vector_mode = RTE_AESNI_MB_SSE;

	dev->driver_id = cryptodev_driver_id;
	dev->dev_ops = rte_aesni_mb_pmd_ops;

	/* register rx/tx burst functions for data path */
	dev->dequeue_burst = aesni_mb_pmd_dequeue_burst;
	dev->enqueue_burst = aesni_mb_pmd_enqueue_burst;

	dev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT |
			RTE_CRYPTODEV_FF_SYM_CPU_CRYPTO |
			RTE_CRYPTODEV_FF_NON_BYTE_ALIGNED_DATA |
			RTE_CRYPTODEV_FF_SYM_SESSIONLESS;

#ifdef AESNI_MB_DOCSIS_SEC_ENABLED
	struct rte_security_ctx *security_instance;
	security_instance = rte_malloc("aesni_mb_sec",
				sizeof(struct rte_security_ctx),
				RTE_CACHE_LINE_SIZE);
	if (security_instance == NULL) {
		AESNI_MB_LOG(ERR, "rte_security_ctx memory alloc failed");
		rte_cryptodev_pmd_destroy(dev);
		return -ENOMEM;
	}

	security_instance->device = (void *)dev;
	security_instance->ops = rte_aesni_mb_pmd_sec_ops;
	security_instance->sess_cnt = 0;
	dev->security_ctx = security_instance;
	dev->feature_flags |= RTE_CRYPTODEV_FF_SECURITY;
#endif

	/* Check CPU for support for AES instruction set */
	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AES))
		dev->feature_flags |= RTE_CRYPTODEV_FF_CPU_AESNI;
	else
		AESNI_MB_LOG(WARNING, "AES instructions not supported by CPU");

	dev->feature_flags |= vec_mode_to_flags(vector_mode);

	mb_mgr = alloc_init_mb_mgr(vector_mode);
	if (mb_mgr == NULL) {
#ifdef AESNI_MB_DOCSIS_SEC_ENABLED
		rte_free(dev->security_ctx);
		dev->security_ctx = NULL;
#endif
		rte_cryptodev_pmd_destroy(dev);
		return -ENOMEM;
	}

	/* Set vector instructions mode supported */
	internals = dev->data->dev_private;

	internals->vector_mode = vector_mode;
	internals->max_nb_queue_pairs = init_params->max_nb_queue_pairs;
	internals->mb_mgr = mb_mgr;

	AESNI_MB_LOG(INFO, "IPSec Multi-buffer library version used: %s\n",
			imb_get_version_str());
	return 0;
}

static int
cryptodev_aesni_mb_probe(struct rte_vdev_device *vdev)
{
	struct rte_cryptodev_pmd_init_params init_params = {
		"",
		sizeof(struct aesni_mb_private),
		rte_socket_id(),
		RTE_CRYPTODEV_PMD_DEFAULT_MAX_NB_QUEUE_PAIRS
	};
	const char *name, *args;
	int retval;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	args = rte_vdev_device_args(vdev);

	retval = rte_cryptodev_pmd_parse_input_args(&init_params, args);
	if (retval) {
		AESNI_MB_LOG(ERR, "Failed to parse initialisation arguments[%s]",
				args);
		return -EINVAL;
	}

	return cryptodev_aesni_mb_create(name, vdev, &init_params);
}

static int
cryptodev_aesni_mb_remove(struct rte_vdev_device *vdev)
{
	struct rte_cryptodev *cryptodev;
	struct aesni_mb_private *internals;
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	cryptodev = rte_cryptodev_pmd_get_named_dev(name);
	if (cryptodev == NULL)
		return -ENODEV;

	internals = cryptodev->data->dev_private;

	free_mb_mgr(internals->mb_mgr);
	if (RTE_PER_LCORE(sync_mb_mgr)) {
		free_mb_mgr(RTE_PER_LCORE(sync_mb_mgr));
		RTE_PER_LCORE(sync_mb_mgr) = NULL;
	}

#ifdef AESNI_MB_DOCSIS_SEC_ENABLED
	rte_free(cryptodev->security_ctx);
	cryptodev->security_ctx = NULL;
#endif

	return rte_cryptodev_pmd_destroy(cryptodev);
}

static struct rte_vdev_driver cryptodev_aesni_mb_pmd_drv = {
	.probe = cryptodev_aesni_mb_probe,
	.remove = cryptodev_aesni_mb_remove
};

static struct cryptodev_driver aesni_mb_crypto_drv;

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_AESNI_MB_PMD, cryptodev_aesni_mb_pmd_drv);
RTE_PMD_REGISTER_ALIAS(CRYPTODEV_NAME_AESNI_MB_PMD, cryptodev_aesni_mb_pmd);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_AESNI_MB_PMD,
	"max_nb_queue_pairs=<int> "
	"socket_id=<int>");
RTE_PMD_REGISTER_CRYPTO_DRIVER(aesni_mb_crypto_drv,
		cryptodev_aesni_mb_pmd_drv.driver,
		cryptodev_driver_id);
RTE_LOG_REGISTER_DEFAULT(aesni_mb_logtype_driver, NOTICE);
