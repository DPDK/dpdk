/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include <bus_vdev_driver.h>
#include <rte_malloc.h>
#include <rte_cpuflags.h>

#include <openssl/cmac.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/ec.h>

#include "openssl_pmd_private.h"
#include "compat.h"

#define DES_BLOCK_SIZE 8

static uint8_t cryptodev_driver_id;

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
static HMAC_CTX *HMAC_CTX_new(void)
{
	HMAC_CTX *ctx = OPENSSL_malloc(sizeof(*ctx));

	if (ctx != NULL)
		HMAC_CTX_init(ctx);
	return ctx;
}

static void HMAC_CTX_free(HMAC_CTX *ctx)
{
	if (ctx != NULL) {
		HMAC_CTX_cleanup(ctx);
		OPENSSL_free(ctx);
	}
}
#endif

#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)

#include <openssl/provider.h>
#include <openssl/core_names.h>
#include <openssl/param_build.h>

#define MAX_OSSL_ALGO_NAME_SIZE		16

OSSL_PROVIDER *legacy;
OSSL_PROVIDER *deflt;

static void ossl_legacy_provider_load(void)
{
	/* Load Multiple providers into the default (NULL) library context */
	legacy = OSSL_PROVIDER_load(NULL, "legacy");
	if (legacy == NULL) {
		OPENSSL_LOG(ERR, "Failed to load Legacy provider\n");
		return;
	}

	deflt = OSSL_PROVIDER_load(NULL, "default");
	if (deflt == NULL) {
		OPENSSL_LOG(ERR, "Failed to load Default provider\n");
		OSSL_PROVIDER_unload(legacy);
		return;
	}
}

static void ossl_legacy_provider_unload(void)
{
	OSSL_PROVIDER_unload(legacy);
	OSSL_PROVIDER_unload(deflt);
}

static __rte_always_inline const char *
digest_name_get(enum rte_crypto_auth_algorithm algo)
{
	switch (algo) {
	case RTE_CRYPTO_AUTH_MD5_HMAC:
		return OSSL_DIGEST_NAME_MD5;
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		return OSSL_DIGEST_NAME_SHA1;
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
		return OSSL_DIGEST_NAME_SHA2_224;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		return OSSL_DIGEST_NAME_SHA2_256;
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
		return OSSL_DIGEST_NAME_SHA2_384;
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		return OSSL_DIGEST_NAME_SHA2_512;
	default:
		return NULL;
	}
}
#endif

static int cryptodev_openssl_remove(struct rte_vdev_device *vdev);

/*----------------------------------------------------------------------------*/

/**
 * Increment counter by 1
 * Counter is 64 bit array, big-endian
 */
static void
ctr_inc(uint8_t *ctr)
{
	uint64_t *ctr64 = (uint64_t *)ctr;

	*ctr64 = __builtin_bswap64(*ctr64);
	(*ctr64)++;
	*ctr64 = __builtin_bswap64(*ctr64);
}

/*
 *------------------------------------------------------------------------------
 * Session Prepare
 *------------------------------------------------------------------------------
 */

/** Get xform chain order */
static enum openssl_chain_order
openssl_get_chain_order(const struct rte_crypto_sym_xform *xform)
{
	enum openssl_chain_order res = OPENSSL_CHAIN_NOT_SUPPORTED;

	if (xform != NULL) {
		if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
			if (xform->next == NULL)
				res =  OPENSSL_CHAIN_ONLY_AUTH;
			else if (xform->next->type ==
					RTE_CRYPTO_SYM_XFORM_CIPHER)
				res =  OPENSSL_CHAIN_AUTH_CIPHER;
		}
		if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
			if (xform->next == NULL)
				res =  OPENSSL_CHAIN_ONLY_CIPHER;
			else if (xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH)
				res =  OPENSSL_CHAIN_CIPHER_AUTH;
		}
		if (xform->type == RTE_CRYPTO_SYM_XFORM_AEAD)
			res = OPENSSL_CHAIN_COMBINED;
	}

	return res;
}

/** Get session cipher key from input cipher key */
static void
get_cipher_key(const uint8_t *input_key, int keylen, uint8_t *session_key)
{
	memcpy(session_key, input_key, keylen);
}

/** Get key ede 24 bytes standard from input key */
static int
get_cipher_key_ede(const uint8_t *key, int keylen, uint8_t *key_ede)
{
	int res = 0;

	/* Initialize keys - 24 bytes: [key1-key2-key3] */
	switch (keylen) {
	case 24:
		memcpy(key_ede, key, 24);
		break;
	case 16:
		/* K3 = K1 */
		memcpy(key_ede, key, 16);
		memcpy(key_ede + 16, key, 8);
		break;
	case 8:
		/* K1 = K2 = K3 (DES compatibility) */
		memcpy(key_ede, key, 8);
		memcpy(key_ede + 8, key, 8);
		memcpy(key_ede + 16, key, 8);
		break;
	default:
		OPENSSL_LOG(ERR, "Unsupported key size");
		res = -EINVAL;
	}

	return res;
}

/** Get adequate openssl function for input cipher algorithm */
static uint8_t
get_cipher_algo(enum rte_crypto_cipher_algorithm sess_algo, size_t keylen,
		const EVP_CIPHER **algo)
{
	int res = 0;

	if (algo != NULL) {
		switch (sess_algo) {
		case RTE_CRYPTO_CIPHER_3DES_CBC:
			switch (keylen) {
			case 8:
				*algo = EVP_des_cbc();
				break;
			case 16:
				*algo = EVP_des_ede_cbc();
				break;
			case 24:
				*algo = EVP_des_ede3_cbc();
				break;
			default:
				res = -EINVAL;
			}
			break;
		case RTE_CRYPTO_CIPHER_3DES_CTR:
			break;
		case RTE_CRYPTO_CIPHER_AES_CBC:
			switch (keylen) {
			case 16:
				*algo = EVP_aes_128_cbc();
				break;
			case 24:
				*algo = EVP_aes_192_cbc();
				break;
			case 32:
				*algo = EVP_aes_256_cbc();
				break;
			default:
				res = -EINVAL;
			}
			break;
		case RTE_CRYPTO_CIPHER_AES_CTR:
			switch (keylen) {
			case 16:
				*algo = EVP_aes_128_ctr();
				break;
			case 24:
				*algo = EVP_aes_192_ctr();
				break;
			case 32:
				*algo = EVP_aes_256_ctr();
				break;
			default:
				res = -EINVAL;
			}
			break;
		default:
			res = -EINVAL;
			break;
		}
	} else {
		res = -EINVAL;
	}

	return res;
}

/** Get adequate openssl function for input auth algorithm */
static uint8_t
get_auth_algo(enum rte_crypto_auth_algorithm sessalgo,
		const EVP_MD **algo)
{
	int res = 0;

	if (algo != NULL) {
		switch (sessalgo) {
		case RTE_CRYPTO_AUTH_MD5:
		case RTE_CRYPTO_AUTH_MD5_HMAC:
			*algo = EVP_md5();
			break;
		case RTE_CRYPTO_AUTH_SHA1:
		case RTE_CRYPTO_AUTH_SHA1_HMAC:
			*algo = EVP_sha1();
			break;
		case RTE_CRYPTO_AUTH_SHA224:
		case RTE_CRYPTO_AUTH_SHA224_HMAC:
			*algo = EVP_sha224();
			break;
		case RTE_CRYPTO_AUTH_SHA256:
		case RTE_CRYPTO_AUTH_SHA256_HMAC:
			*algo = EVP_sha256();
			break;
		case RTE_CRYPTO_AUTH_SHA384:
		case RTE_CRYPTO_AUTH_SHA384_HMAC:
			*algo = EVP_sha384();
			break;
		case RTE_CRYPTO_AUTH_SHA512:
		case RTE_CRYPTO_AUTH_SHA512_HMAC:
			*algo = EVP_sha512();
			break;
		default:
			res = -EINVAL;
			break;
		}
	} else {
		res = -EINVAL;
	}

	return res;
}

/** Get adequate openssl function for input cipher algorithm */
static uint8_t
get_aead_algo(enum rte_crypto_aead_algorithm sess_algo, size_t keylen,
		const EVP_CIPHER **algo)
{
	int res = 0;

	if (algo != NULL) {
		switch (sess_algo) {
		case RTE_CRYPTO_AEAD_AES_GCM:
			switch (keylen) {
			case 16:
				*algo = EVP_aes_128_gcm();
				break;
			case 24:
				*algo = EVP_aes_192_gcm();
				break;
			case 32:
				*algo = EVP_aes_256_gcm();
				break;
			default:
				res = -EINVAL;
			}
			break;
		case RTE_CRYPTO_AEAD_AES_CCM:
			switch (keylen) {
			case 16:
				*algo = EVP_aes_128_ccm();
				break;
			case 24:
				*algo = EVP_aes_192_ccm();
				break;
			case 32:
				*algo = EVP_aes_256_ccm();
				break;
			default:
				res = -EINVAL;
			}
			break;
		default:
			res = -EINVAL;
			break;
		}
	} else {
		res = -EINVAL;
	}

	return res;
}

/* Set session AEAD encryption parameters */
static int
openssl_set_sess_aead_enc_param(struct openssl_session *sess,
		enum rte_crypto_aead_algorithm algo,
		uint8_t tag_len, const uint8_t *key)
{
	int iv_type = 0;
	unsigned int do_ccm;

	sess->cipher.direction = RTE_CRYPTO_CIPHER_OP_ENCRYPT;
	sess->auth.operation = RTE_CRYPTO_AUTH_OP_GENERATE;

	/* Select AEAD algo */
	switch (algo) {
	case RTE_CRYPTO_AEAD_AES_GCM:
		iv_type = EVP_CTRL_GCM_SET_IVLEN;
		if (tag_len != 16)
			return -EINVAL;
		do_ccm = 0;
		break;
	case RTE_CRYPTO_AEAD_AES_CCM:
		iv_type = EVP_CTRL_CCM_SET_IVLEN;
		/* Digest size can be 4, 6, 8, 10, 12, 14 or 16 bytes */
		if (tag_len < 4 || tag_len > 16 || (tag_len & 1) == 1)
			return -EINVAL;
		do_ccm = 1;
		break;
	default:
		return -ENOTSUP;
	}

	sess->cipher.mode = OPENSSL_CIPHER_LIB;
	sess->cipher.ctx = EVP_CIPHER_CTX_new();

	if (get_aead_algo(algo, sess->cipher.key.length,
			&sess->cipher.evp_algo) != 0)
		return -EINVAL;

	get_cipher_key(key, sess->cipher.key.length, sess->cipher.key.data);

	sess->chain_order = OPENSSL_CHAIN_COMBINED;

	if (EVP_EncryptInit_ex(sess->cipher.ctx, sess->cipher.evp_algo,
			NULL, NULL, NULL) <= 0)
		return -EINVAL;

	if (EVP_CIPHER_CTX_ctrl(sess->cipher.ctx, iv_type, sess->iv.length,
			NULL) <= 0)
		return -EINVAL;

	if (do_ccm)
		EVP_CIPHER_CTX_ctrl(sess->cipher.ctx, EVP_CTRL_CCM_SET_TAG,
				tag_len, NULL);

	if (EVP_EncryptInit_ex(sess->cipher.ctx, NULL, NULL, key, NULL) <= 0)
		return -EINVAL;

	return 0;
}

/* Set session AEAD decryption parameters */
static int
openssl_set_sess_aead_dec_param(struct openssl_session *sess,
		enum rte_crypto_aead_algorithm algo,
		uint8_t tag_len, const uint8_t *key)
{
	int iv_type = 0;
	unsigned int do_ccm = 0;

	sess->cipher.direction = RTE_CRYPTO_CIPHER_OP_DECRYPT;
	sess->auth.operation = RTE_CRYPTO_AUTH_OP_VERIFY;

	/* Select AEAD algo */
	switch (algo) {
	case RTE_CRYPTO_AEAD_AES_GCM:
		iv_type = EVP_CTRL_GCM_SET_IVLEN;
		if (tag_len != 16)
			return -EINVAL;
		break;
	case RTE_CRYPTO_AEAD_AES_CCM:
		iv_type = EVP_CTRL_CCM_SET_IVLEN;
		/* Digest size can be 4, 6, 8, 10, 12, 14 or 16 bytes */
		if (tag_len < 4 || tag_len > 16 || (tag_len & 1) == 1)
			return -EINVAL;
		do_ccm = 1;
		break;
	default:
		return -ENOTSUP;
	}

	sess->cipher.mode = OPENSSL_CIPHER_LIB;
	sess->cipher.ctx = EVP_CIPHER_CTX_new();

	if (get_aead_algo(algo, sess->cipher.key.length,
			&sess->cipher.evp_algo) != 0)
		return -EINVAL;

	get_cipher_key(key, sess->cipher.key.length, sess->cipher.key.data);

	sess->chain_order = OPENSSL_CHAIN_COMBINED;

	if (EVP_DecryptInit_ex(sess->cipher.ctx, sess->cipher.evp_algo,
			NULL, NULL, NULL) <= 0)
		return -EINVAL;

	if (EVP_CIPHER_CTX_ctrl(sess->cipher.ctx, iv_type,
			sess->iv.length, NULL) <= 0)
		return -EINVAL;

	if (do_ccm)
		EVP_CIPHER_CTX_ctrl(sess->cipher.ctx, EVP_CTRL_CCM_SET_TAG,
				tag_len, NULL);

	if (EVP_DecryptInit_ex(sess->cipher.ctx, NULL, NULL, key, NULL) <= 0)
		return -EINVAL;

	return 0;
}

/** Set session cipher parameters */
static int
openssl_set_session_cipher_parameters(struct openssl_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	/* Select cipher direction */
	sess->cipher.direction = xform->cipher.op;
	/* Select cipher key */
	sess->cipher.key.length = xform->cipher.key.length;

	/* Set IV parameters */
	sess->iv.offset = xform->cipher.iv.offset;
	sess->iv.length = xform->cipher.iv.length;

	/* Select cipher algo */
	switch (xform->cipher.algo) {
	case RTE_CRYPTO_CIPHER_3DES_CBC:
	case RTE_CRYPTO_CIPHER_AES_CBC:
	case RTE_CRYPTO_CIPHER_AES_CTR:
		sess->cipher.mode = OPENSSL_CIPHER_LIB;
		sess->cipher.algo = xform->cipher.algo;
		sess->cipher.ctx = EVP_CIPHER_CTX_new();

		if (get_cipher_algo(sess->cipher.algo, sess->cipher.key.length,
				&sess->cipher.evp_algo) != 0)
			return -EINVAL;

		get_cipher_key(xform->cipher.key.data, sess->cipher.key.length,
			sess->cipher.key.data);
		if (sess->cipher.direction == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
			if (EVP_EncryptInit_ex(sess->cipher.ctx,
					sess->cipher.evp_algo,
					NULL, xform->cipher.key.data,
					NULL) != 1) {
				return -EINVAL;
			}
		} else if (sess->cipher.direction ==
				RTE_CRYPTO_CIPHER_OP_DECRYPT) {
			if (EVP_DecryptInit_ex(sess->cipher.ctx,
					sess->cipher.evp_algo,
					NULL, xform->cipher.key.data,
					NULL) != 1) {
				return -EINVAL;
			}
		}

		break;

	case RTE_CRYPTO_CIPHER_3DES_CTR:
		sess->cipher.mode = OPENSSL_CIPHER_DES3CTR;
		sess->cipher.ctx = EVP_CIPHER_CTX_new();

		if (get_cipher_key_ede(xform->cipher.key.data,
				sess->cipher.key.length,
				sess->cipher.key.data) != 0)
			return -EINVAL;
		break;

	case RTE_CRYPTO_CIPHER_DES_CBC:
		sess->cipher.algo = xform->cipher.algo;
		sess->cipher.ctx = EVP_CIPHER_CTX_new();
		sess->cipher.evp_algo = EVP_des_cbc();

		get_cipher_key(xform->cipher.key.data, sess->cipher.key.length,
			sess->cipher.key.data);
		if (sess->cipher.direction == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
			if (EVP_EncryptInit_ex(sess->cipher.ctx,
					sess->cipher.evp_algo,
					NULL, xform->cipher.key.data,
					NULL) != 1) {
				return -EINVAL;
			}
		} else if (sess->cipher.direction ==
				RTE_CRYPTO_CIPHER_OP_DECRYPT) {
			if (EVP_DecryptInit_ex(sess->cipher.ctx,
					sess->cipher.evp_algo,
					NULL, xform->cipher.key.data,
					NULL) != 1) {
				return -EINVAL;
			}
		}

		break;

	case RTE_CRYPTO_CIPHER_DES_DOCSISBPI:
		sess->cipher.algo = xform->cipher.algo;
		sess->chain_order = OPENSSL_CHAIN_CIPHER_BPI;
		sess->cipher.ctx = EVP_CIPHER_CTX_new();
		sess->cipher.evp_algo = EVP_des_cbc();

		sess->cipher.bpi_ctx = EVP_CIPHER_CTX_new();
		/* IV will be ECB encrypted whether direction is encrypt or decrypt */
		if (EVP_EncryptInit_ex(sess->cipher.bpi_ctx, EVP_des_ecb(),
				NULL, xform->cipher.key.data, 0) != 1)
			return -EINVAL;

		get_cipher_key(xform->cipher.key.data, sess->cipher.key.length,
			sess->cipher.key.data);
		if (sess->cipher.direction == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
			if (EVP_EncryptInit_ex(sess->cipher.ctx,
					sess->cipher.evp_algo,
					NULL, xform->cipher.key.data,
					NULL) != 1) {
				return -EINVAL;
			}
		} else if (sess->cipher.direction ==
				RTE_CRYPTO_CIPHER_OP_DECRYPT) {
			if (EVP_DecryptInit_ex(sess->cipher.ctx,
					sess->cipher.evp_algo,
					NULL, xform->cipher.key.data,
					NULL) != 1) {
				return -EINVAL;
			}
		}

		break;
	default:
		sess->cipher.algo = RTE_CRYPTO_CIPHER_NULL;
		return -ENOTSUP;
	}

	return 0;
}

/* Set session auth parameters */
static int
openssl_set_session_auth_parameters(struct openssl_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
# if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
	char algo_name[MAX_OSSL_ALGO_NAME_SIZE];
	OSSL_PARAM params[2];
	const char *algo;
	EVP_MAC *mac;
# endif
	/* Select auth generate/verify */
	sess->auth.operation = xform->auth.op;
	sess->auth.algo = xform->auth.algo;

	sess->auth.digest_length = xform->auth.digest_length;

	/* Select auth algo */
	switch (xform->auth.algo) {
	case RTE_CRYPTO_AUTH_AES_GMAC:
		/*
		 * OpenSSL requires GMAC to be a GCM operation
		 * with no cipher data length
		 */
		sess->cipher.key.length = xform->auth.key.length;

		/* Set IV parameters */
		sess->iv.offset = xform->auth.iv.offset;
		sess->iv.length = xform->auth.iv.length;

		if (sess->auth.operation == RTE_CRYPTO_AUTH_OP_GENERATE)
			return openssl_set_sess_aead_enc_param(sess,
						RTE_CRYPTO_AEAD_AES_GCM,
						xform->auth.digest_length,
						xform->auth.key.data);
		else
			return openssl_set_sess_aead_dec_param(sess,
						RTE_CRYPTO_AEAD_AES_GCM,
						xform->auth.digest_length,
						xform->auth.key.data);
		break;

	case RTE_CRYPTO_AUTH_MD5:
	case RTE_CRYPTO_AUTH_SHA1:
	case RTE_CRYPTO_AUTH_SHA224:
	case RTE_CRYPTO_AUTH_SHA256:
	case RTE_CRYPTO_AUTH_SHA384:
	case RTE_CRYPTO_AUTH_SHA512:
		sess->auth.mode = OPENSSL_AUTH_AS_AUTH;
		if (get_auth_algo(xform->auth.algo,
				&sess->auth.auth.evp_algo) != 0)
			return -EINVAL;
		sess->auth.auth.ctx = EVP_MD_CTX_create();
		break;

	case RTE_CRYPTO_AUTH_AES_CMAC:
# if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
		if (xform->auth.key.length == 16)
			algo = SN_aes_128_cbc;
		else if (xform->auth.key.length == 24)
			algo = SN_aes_192_cbc;
		else if (xform->auth.key.length == 32)
			algo = SN_aes_256_cbc;
		else
			return -EINVAL;

		rte_memcpy(algo_name, algo, strlen(algo) + 1);
		params[0] = OSSL_PARAM_construct_utf8_string(
				OSSL_MAC_PARAM_CIPHER, algo_name, 0);
		params[1] = OSSL_PARAM_construct_end();

		sess->auth.mode = OPENSSL_AUTH_AS_CMAC;
		mac = EVP_MAC_fetch(NULL, OSSL_MAC_NAME_CMAC, NULL);
		sess->auth.cmac.ctx = EVP_MAC_CTX_new(mac);
		EVP_MAC_free(mac);

		if (EVP_MAC_init(sess->auth.cmac.ctx,
				xform->auth.key.data,
				xform->auth.key.length,
				params) != 1)
			return -EINVAL;
# else
		sess->auth.mode = OPENSSL_AUTH_AS_CMAC;
		sess->auth.cmac.ctx = CMAC_CTX_new();
		if (get_cipher_algo(RTE_CRYPTO_CIPHER_AES_CBC,
				    xform->auth.key.length,
				    &sess->auth.cmac.evp_algo) != 0)
			return -EINVAL;
		if (CMAC_Init(sess->auth.cmac.ctx,
			      xform->auth.key.data,
			      xform->auth.key.length,
			      sess->auth.cmac.evp_algo, NULL) != 1)
			return -EINVAL;
# endif
		break;

# if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
	case RTE_CRYPTO_AUTH_MD5_HMAC:
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		sess->auth.mode = OPENSSL_AUTH_AS_HMAC;

		algo = digest_name_get(xform->auth.algo);
		if (!algo)
			return -EINVAL;
		strlcpy(algo_name, algo, sizeof(algo_name));

		mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
		sess->auth.hmac.ctx = EVP_MAC_CTX_new(mac);
		EVP_MAC_free(mac);
		if (get_auth_algo(xform->auth.algo,
				&sess->auth.hmac.evp_algo) != 0)
			return -EINVAL;

		params[0] = OSSL_PARAM_construct_utf8_string("digest",
					algo_name, 0);
		params[1] = OSSL_PARAM_construct_end();
		if (EVP_MAC_init(sess->auth.hmac.ctx,
				xform->auth.key.data,
				xform->auth.key.length,
				params) != 1)
			return -EINVAL;
		break;
# else
	case RTE_CRYPTO_AUTH_MD5_HMAC:
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		sess->auth.mode = OPENSSL_AUTH_AS_HMAC;
		sess->auth.hmac.ctx = HMAC_CTX_new();
		if (get_auth_algo(xform->auth.algo,
				&sess->auth.hmac.evp_algo) != 0)
			return -EINVAL;

		if (HMAC_Init_ex(sess->auth.hmac.ctx,
				xform->auth.key.data,
				xform->auth.key.length,
				sess->auth.hmac.evp_algo, NULL) != 1)
			return -EINVAL;
		break;
# endif
	default:
		return -ENOTSUP;
	}

	return 0;
}

/* Set session AEAD parameters */
static int
openssl_set_session_aead_parameters(struct openssl_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	/* Select cipher key */
	sess->cipher.key.length = xform->aead.key.length;

	/* Set IV parameters */
	if (xform->aead.algo == RTE_CRYPTO_AEAD_AES_CCM)
		/*
		 * For AES-CCM, the actual IV is placed
		 * one byte after the start of the IV field,
		 * according to the API.
		 */
		sess->iv.offset = xform->aead.iv.offset + 1;
	else
		sess->iv.offset = xform->aead.iv.offset;

	sess->iv.length = xform->aead.iv.length;

	sess->auth.aad_length = xform->aead.aad_length;
	sess->auth.digest_length = xform->aead.digest_length;

	sess->aead_algo = xform->aead.algo;
	/* Select cipher direction */
	if (xform->aead.op == RTE_CRYPTO_AEAD_OP_ENCRYPT)
		return openssl_set_sess_aead_enc_param(sess, xform->aead.algo,
				xform->aead.digest_length, xform->aead.key.data);
	else
		return openssl_set_sess_aead_dec_param(sess, xform->aead.algo,
				xform->aead.digest_length, xform->aead.key.data);
}

/** Parse crypto xform chain and set private session parameters */
int
openssl_set_session_parameters(struct openssl_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	const struct rte_crypto_sym_xform *cipher_xform = NULL;
	const struct rte_crypto_sym_xform *auth_xform = NULL;
	const struct rte_crypto_sym_xform *aead_xform = NULL;
	int ret;

	sess->chain_order = openssl_get_chain_order(xform);
	switch (sess->chain_order) {
	case OPENSSL_CHAIN_ONLY_CIPHER:
		cipher_xform = xform;
		break;
	case OPENSSL_CHAIN_ONLY_AUTH:
		auth_xform = xform;
		break;
	case OPENSSL_CHAIN_CIPHER_AUTH:
		cipher_xform = xform;
		auth_xform = xform->next;
		break;
	case OPENSSL_CHAIN_AUTH_CIPHER:
		auth_xform = xform;
		cipher_xform = xform->next;
		break;
	case OPENSSL_CHAIN_COMBINED:
		aead_xform = xform;
		break;
	default:
		return -EINVAL;
	}

	/* Default IV length = 0 */
	sess->iv.length = 0;

	/* cipher_xform must be check before auth_xform */
	if (cipher_xform) {
		ret = openssl_set_session_cipher_parameters(
				sess, cipher_xform);
		if (ret != 0) {
			OPENSSL_LOG(ERR,
				"Invalid/unsupported cipher parameters");
			return ret;
		}
	}

	if (auth_xform) {
		ret = openssl_set_session_auth_parameters(sess, auth_xform);
		if (ret != 0) {
			OPENSSL_LOG(ERR,
				"Invalid/unsupported auth parameters");
			return ret;
		}
	}

	if (aead_xform) {
		ret = openssl_set_session_aead_parameters(sess, aead_xform);
		if (ret != 0) {
			OPENSSL_LOG(ERR,
				"Invalid/unsupported AEAD parameters");
			return ret;
		}
	}

	return 0;
}

/** Reset private session parameters */
void
openssl_reset_session(struct openssl_session *sess)
{
	EVP_CIPHER_CTX_free(sess->cipher.ctx);

	if (sess->chain_order == OPENSSL_CHAIN_CIPHER_BPI)
		EVP_CIPHER_CTX_free(sess->cipher.bpi_ctx);

	switch (sess->auth.mode) {
	case OPENSSL_AUTH_AS_AUTH:
		EVP_MD_CTX_destroy(sess->auth.auth.ctx);
		break;
	case OPENSSL_AUTH_AS_HMAC:
		EVP_PKEY_free(sess->auth.hmac.pkey);
# if OPENSSL_VERSION_NUMBER >= 0x30000000L
		EVP_MAC_CTX_free(sess->auth.hmac.ctx);
# else
		HMAC_CTX_free(sess->auth.hmac.ctx);
# endif
		break;
	case OPENSSL_AUTH_AS_CMAC:
# if OPENSSL_VERSION_NUMBER >= 0x30000000L
		EVP_MAC_CTX_free(sess->auth.cmac.ctx);
# else
		CMAC_CTX_free(sess->auth.cmac.ctx);
# endif
		break;
	default:
		break;
	}
}

/** Provide session for operation */
static void *
get_session(struct openssl_qp *qp, struct rte_crypto_op *op)
{
	struct openssl_session *sess = NULL;
	struct openssl_asym_session *asym_sess = NULL;

	if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
		if (op->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
			/* get existing session */
			if (likely(op->sym->session != NULL))
				sess = CRYPTODEV_GET_SYM_SESS_PRIV(
					op->sym->session);
		} else {
			if (likely(op->asym->session != NULL))
				asym_sess = (struct openssl_asym_session *)
						op->asym->session->sess_private_data;
			if (asym_sess == NULL)
				op->status =
					RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
			return asym_sess;
		}
	} else {
		struct rte_cryptodev_sym_session *_sess;
		/* sessionless asymmetric not supported */
		if (op->type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC)
			return NULL;

		/* provide internal session */
		rte_mempool_get(qp->sess_mp, (void **)&_sess);

		if (_sess == NULL)
			return NULL;

		sess = (struct openssl_session *)_sess->driver_priv_data;

		if (unlikely(openssl_set_session_parameters(sess,
				op->sym->xform) != 0)) {
			rte_mempool_put(qp->sess_mp, _sess);
			sess = NULL;
		}
		op->sym->session = (struct rte_cryptodev_sym_session *)_sess;

	}

	if (sess == NULL)
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;

	return sess;
}

/*
 *------------------------------------------------------------------------------
 * Process Operations
 *------------------------------------------------------------------------------
 */
static inline int
process_openssl_encryption_update(struct rte_mbuf *mbuf_src, int offset,
		uint8_t **dst, int srclen, EVP_CIPHER_CTX *ctx, uint8_t inplace)
{
	struct rte_mbuf *m;
	int dstlen;
	int l, n = srclen;
	uint8_t *src, temp[EVP_CIPHER_CTX_block_size(ctx)];

	for (m = mbuf_src; m != NULL && offset > rte_pktmbuf_data_len(m);
			m = m->next)
		offset -= rte_pktmbuf_data_len(m);

	if (m == 0)
		return -1;

	src = rte_pktmbuf_mtod_offset(m, uint8_t *, offset);
	if (inplace)
		*dst = src;

	l = rte_pktmbuf_data_len(m) - offset;
	if (srclen <= l) {
		if (EVP_EncryptUpdate(ctx, *dst, &dstlen, src, srclen) <= 0)
			return -1;
		*dst += l;
		return 0;
	}

	if (EVP_EncryptUpdate(ctx, *dst, &dstlen, src, l) <= 0)
		return -1;

	*dst += dstlen;
	n -= l;

	for (m = m->next; (m != NULL) && (n > 0); m = m->next) {
		uint8_t diff = l - dstlen, rem;

		src = rte_pktmbuf_mtod(m, uint8_t *);
		l = RTE_MIN(rte_pktmbuf_data_len(m), n);
		if (diff && inplace) {
			rem = RTE_MIN(l,
				(EVP_CIPHER_CTX_block_size(ctx) - diff));
			if (EVP_EncryptUpdate(ctx, temp,
						&dstlen, src, rem) <= 0)
				return -1;
			n -= rem;
			rte_memcpy(*dst, temp, diff);
			rte_memcpy(src, temp + diff, rem);
			src += rem;
			l -= rem;
		}
		if (inplace)
			*dst = src;
		if (EVP_EncryptUpdate(ctx, *dst, &dstlen, src, l) <= 0)
			return -1;
		*dst += dstlen;
		n -= l;
	}

	return 0;
}

static inline int
process_openssl_decryption_update(struct rte_mbuf *mbuf_src, int offset,
		uint8_t **dst, int srclen, EVP_CIPHER_CTX *ctx, uint8_t inplace)
{
	struct rte_mbuf *m;
	int dstlen;
	int l, n = srclen;
	uint8_t *src, temp[EVP_CIPHER_CTX_block_size(ctx)];

	for (m = mbuf_src; m != NULL && offset > rte_pktmbuf_data_len(m);
			m = m->next)
		offset -= rte_pktmbuf_data_len(m);

	if (m == 0)
		return -1;

	src = rte_pktmbuf_mtod_offset(m, uint8_t *, offset);
	if (inplace)
		*dst = src;

	l = rte_pktmbuf_data_len(m) - offset;
	if (srclen <= l) {
		if (EVP_DecryptUpdate(ctx, *dst, &dstlen, src, srclen) <= 0)
			return -1;
		*dst += l;
		return 0;
	}

	if (EVP_DecryptUpdate(ctx, *dst, &dstlen, src, l) <= 0)
		return -1;

	*dst += dstlen;
	n -= l;

	for (m = m->next; (m != NULL) && (n > 0); m = m->next) {
		uint8_t diff = l - dstlen, rem;

		src = rte_pktmbuf_mtod(m, uint8_t *);
		l = RTE_MIN(rte_pktmbuf_data_len(m), n);
		if (diff && inplace) {
			rem = RTE_MIN(l,
				(EVP_CIPHER_CTX_block_size(ctx) - diff));
			if (EVP_DecryptUpdate(ctx, temp,
						&dstlen, src, rem) <= 0)
				return -1;
			n -= rem;
			rte_memcpy(*dst, temp, diff);
			rte_memcpy(src, temp + diff, rem);
			src += rem;
			l -= rem;
		}
		if (inplace)
			*dst = src;
		if (EVP_DecryptUpdate(ctx, *dst, &dstlen, src, l) <= 0)
			return -1;
		*dst += dstlen;
		n -= l;
	}

	return 0;
}

/** Process standard openssl cipher encryption */
static int
process_openssl_cipher_encrypt(struct rte_mbuf *mbuf_src, uint8_t *dst,
		int offset, uint8_t *iv, int srclen, EVP_CIPHER_CTX *ctx,
		uint8_t inplace)
{
	int totlen;

	if (EVP_EncryptInit_ex(ctx, NULL, NULL, NULL, iv) <= 0)
		goto process_cipher_encrypt_err;

	EVP_CIPHER_CTX_set_padding(ctx, 0);

	if (process_openssl_encryption_update(mbuf_src, offset, &dst,
			srclen, ctx, inplace))
		goto process_cipher_encrypt_err;

	if (EVP_EncryptFinal_ex(ctx, dst, &totlen) <= 0)
		goto process_cipher_encrypt_err;

	return 0;

process_cipher_encrypt_err:
	OPENSSL_LOG(ERR, "Process openssl cipher encrypt failed");
	return -EINVAL;
}

/** Process standard openssl cipher encryption */
static int
process_openssl_cipher_bpi_encrypt(uint8_t *src, uint8_t *dst,
		uint8_t *iv, int srclen,
		EVP_CIPHER_CTX *ctx)
{
	uint8_t i;
	uint8_t encrypted_iv[DES_BLOCK_SIZE];
	int encrypted_ivlen;

	if (EVP_EncryptUpdate(ctx, encrypted_iv, &encrypted_ivlen,
			iv, DES_BLOCK_SIZE) <= 0)
		goto process_cipher_encrypt_err;

	for (i = 0; i < srclen; i++)
		*(dst + i) = *(src + i) ^ (encrypted_iv[i]);

	return 0;

process_cipher_encrypt_err:
	OPENSSL_LOG(ERR, "Process openssl cipher bpi encrypt failed");
	return -EINVAL;
}
/** Process standard openssl cipher decryption */
static int
process_openssl_cipher_decrypt(struct rte_mbuf *mbuf_src, uint8_t *dst,
		int offset, uint8_t *iv, int srclen, EVP_CIPHER_CTX *ctx,
		uint8_t inplace)
{
	int totlen;

	if (EVP_DecryptInit_ex(ctx, NULL, NULL, NULL, iv) <= 0)
		goto process_cipher_decrypt_err;

	EVP_CIPHER_CTX_set_padding(ctx, 0);

	if (process_openssl_decryption_update(mbuf_src, offset, &dst,
			srclen, ctx, inplace))
		goto process_cipher_decrypt_err;

	if (EVP_DecryptFinal_ex(ctx, dst, &totlen) <= 0)
		goto process_cipher_decrypt_err;
	return 0;

process_cipher_decrypt_err:
	OPENSSL_LOG(ERR, "Process openssl cipher decrypt failed");
	return -EINVAL;
}

/** Process cipher des 3 ctr encryption, decryption algorithm */
static int
process_openssl_cipher_des3ctr(struct rte_mbuf *mbuf_src, uint8_t *dst,
		int offset, uint8_t *iv, uint8_t *key, int srclen,
		EVP_CIPHER_CTX *ctx)
{
	uint8_t ebuf[8], ctr[8];
	int unused, n;
	struct rte_mbuf *m;
	uint8_t *src;
	int l;

	for (m = mbuf_src; m != NULL && offset > rte_pktmbuf_data_len(m);
			m = m->next)
		offset -= rte_pktmbuf_data_len(m);

	if (m == 0)
		goto process_cipher_des3ctr_err;

	src = rte_pktmbuf_mtod_offset(m, uint8_t *, offset);
	l = rte_pktmbuf_data_len(m) - offset;

	/* We use 3DES encryption also for decryption.
	 * IV is not important for 3DES ecb
	 */
	if (EVP_EncryptInit_ex(ctx, EVP_des_ede3_ecb(), NULL, key, NULL) <= 0)
		goto process_cipher_des3ctr_err;

	memcpy(ctr, iv, 8);

	for (n = 0; n < srclen; n++) {
		if (n % 8 == 0) {
			if (EVP_EncryptUpdate(ctx,
					(unsigned char *)&ebuf, &unused,
					(const unsigned char *)&ctr, 8) <= 0)
				goto process_cipher_des3ctr_err;
			ctr_inc(ctr);
		}
		dst[n] = *(src++) ^ ebuf[n % 8];

		l--;
		if (!l) {
			m = m->next;
			if (m) {
				src = rte_pktmbuf_mtod(m, uint8_t *);
				l = rte_pktmbuf_data_len(m);
			}
		}
	}

	return 0;

process_cipher_des3ctr_err:
	OPENSSL_LOG(ERR, "Process openssl cipher des 3 ede ctr failed");
	return -EINVAL;
}

/** Process AES-GCM encrypt algorithm */
static int
process_openssl_auth_encryption_gcm(struct rte_mbuf *mbuf_src, int offset,
		int srclen, uint8_t *aad, int aadlen, uint8_t *iv,
		uint8_t *dst, uint8_t *tag, EVP_CIPHER_CTX *ctx)
{
	int len = 0;
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	int unused = 0;
	uint8_t empty[] = {};
#endif

	if (EVP_EncryptInit_ex(ctx, NULL, NULL, NULL, iv) <= 0)
		goto process_auth_encryption_gcm_err;

	if (aadlen > 0)
		if (EVP_EncryptUpdate(ctx, NULL, &len, aad, aadlen) <= 0)
			goto process_auth_encryption_gcm_err;

	if (srclen > 0)
		if (process_openssl_encryption_update(mbuf_src, offset, &dst,
				srclen, ctx, 0))
			goto process_auth_encryption_gcm_err;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
	/* Workaround open ssl bug in version less then 1.0.1f */
	if (EVP_EncryptUpdate(ctx, empty, &unused, empty, 0) <= 0)
		goto process_auth_encryption_gcm_err;
#endif

	if (EVP_EncryptFinal_ex(ctx, dst, &len) <= 0)
		goto process_auth_encryption_gcm_err;

	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag) <= 0)
		goto process_auth_encryption_gcm_err;

	return 0;

process_auth_encryption_gcm_err:
	OPENSSL_LOG(ERR, "Process openssl auth encryption gcm failed");
	return -EINVAL;
}

/** Process AES-CCM encrypt algorithm */
static int
process_openssl_auth_encryption_ccm(struct rte_mbuf *mbuf_src, int offset,
		int srclen, uint8_t *aad, int aadlen, uint8_t *iv,
		uint8_t *dst, uint8_t *tag, uint8_t taglen, EVP_CIPHER_CTX *ctx)
{
	int len = 0;

	if (EVP_EncryptInit_ex(ctx, NULL, NULL, NULL, iv) <= 0)
		goto process_auth_encryption_ccm_err;

	if (EVP_EncryptUpdate(ctx, NULL, &len, NULL, srclen) <= 0)
		goto process_auth_encryption_ccm_err;

	if (aadlen > 0)
		/*
		 * For AES-CCM, the actual AAD is placed
		 * 18 bytes after the start of the AAD field,
		 * according to the API.
		 */
		if (EVP_EncryptUpdate(ctx, NULL, &len, aad + 18, aadlen) <= 0)
			goto process_auth_encryption_ccm_err;

	if (srclen >= 0)
		if (process_openssl_encryption_update(mbuf_src, offset, &dst,
				srclen, ctx, 0))
			goto process_auth_encryption_ccm_err;

	if (EVP_EncryptFinal_ex(ctx, dst, &len) <= 0)
		goto process_auth_encryption_ccm_err;

	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_CCM_GET_TAG, taglen, tag) <= 0)
		goto process_auth_encryption_ccm_err;

	return 0;

process_auth_encryption_ccm_err:
	OPENSSL_LOG(ERR, "Process openssl auth encryption ccm failed");
	return -EINVAL;
}

/** Process AES-GCM decrypt algorithm */
static int
process_openssl_auth_decryption_gcm(struct rte_mbuf *mbuf_src, int offset,
		int srclen, uint8_t *aad, int aadlen, uint8_t *iv,
		uint8_t *dst, uint8_t *tag, EVP_CIPHER_CTX *ctx)
{
	int len = 0;
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	int unused = 0;
	uint8_t empty[] = {};
#endif

	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag) <= 0)
		goto process_auth_decryption_gcm_err;

	if (EVP_DecryptInit_ex(ctx, NULL, NULL, NULL, iv) <= 0)
		goto process_auth_decryption_gcm_err;

	if (aadlen > 0)
		if (EVP_DecryptUpdate(ctx, NULL, &len, aad, aadlen) <= 0)
			goto process_auth_decryption_gcm_err;

	if (srclen > 0)
		if (process_openssl_decryption_update(mbuf_src, offset, &dst,
				srclen, ctx, 0))
			goto process_auth_decryption_gcm_err;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
	/* Workaround open ssl bug in version less then 1.0.1f */
	if (EVP_DecryptUpdate(ctx, empty, &unused, empty, 0) <= 0)
		goto process_auth_decryption_gcm_err;
#endif

	if (EVP_DecryptFinal_ex(ctx, dst, &len) <= 0)
		return -EFAULT;

	return 0;

process_auth_decryption_gcm_err:
	OPENSSL_LOG(ERR, "Process openssl auth decryption gcm failed");
	return -EINVAL;
}

/** Process AES-CCM decrypt algorithm */
static int
process_openssl_auth_decryption_ccm(struct rte_mbuf *mbuf_src, int offset,
		int srclen, uint8_t *aad, int aadlen, uint8_t *iv,
		uint8_t *dst, uint8_t *tag, uint8_t tag_len,
		EVP_CIPHER_CTX *ctx)
{
	int len = 0;

	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_CCM_SET_TAG, tag_len, tag) <= 0)
		goto process_auth_decryption_ccm_err;

	if (EVP_DecryptInit_ex(ctx, NULL, NULL, NULL, iv) <= 0)
		goto process_auth_decryption_ccm_err;

	if (EVP_DecryptUpdate(ctx, NULL, &len, NULL, srclen) <= 0)
		goto process_auth_decryption_ccm_err;

	if (aadlen > 0)
		/*
		 * For AES-CCM, the actual AAD is placed
		 * 18 bytes after the start of the AAD field,
		 * according to the API.
		 */
		if (EVP_DecryptUpdate(ctx, NULL, &len, aad + 18, aadlen) <= 0)
			goto process_auth_decryption_ccm_err;

	if (srclen >= 0)
		if (process_openssl_decryption_update(mbuf_src, offset, &dst,
				srclen, ctx, 0))
			return -EFAULT;

	return 0;

process_auth_decryption_ccm_err:
	OPENSSL_LOG(ERR, "Process openssl auth decryption ccm failed");
	return -EINVAL;
}

/** Process standard openssl auth algorithms */
static int
process_openssl_auth(struct rte_mbuf *mbuf_src, uint8_t *dst, int offset,
		__rte_unused uint8_t *iv, __rte_unused EVP_PKEY * pkey,
		int srclen, EVP_MD_CTX *ctx, const EVP_MD *algo)
{
	size_t dstlen;
	struct rte_mbuf *m;
	int l, n = srclen;
	uint8_t *src;

	for (m = mbuf_src; m != NULL && offset > rte_pktmbuf_data_len(m);
			m = m->next)
		offset -= rte_pktmbuf_data_len(m);

	if (m == 0)
		goto process_auth_err;

	if (EVP_DigestInit_ex(ctx, algo, NULL) <= 0)
		goto process_auth_err;

	src = rte_pktmbuf_mtod_offset(m, uint8_t *, offset);

	l = rte_pktmbuf_data_len(m) - offset;
	if (srclen <= l) {
		if (EVP_DigestUpdate(ctx, (char *)src, srclen) <= 0)
			goto process_auth_err;
		goto process_auth_final;
	}

	if (EVP_DigestUpdate(ctx, (char *)src, l) <= 0)
		goto process_auth_err;

	n -= l;

	for (m = m->next; (m != NULL) && (n > 0); m = m->next) {
		src = rte_pktmbuf_mtod(m, uint8_t *);
		l = rte_pktmbuf_data_len(m) < n ? rte_pktmbuf_data_len(m) : n;
		if (EVP_DigestUpdate(ctx, (char *)src, l) <= 0)
			goto process_auth_err;
		n -= l;
	}

process_auth_final:
	if (EVP_DigestFinal_ex(ctx, dst, (unsigned int *)&dstlen) <= 0)
		goto process_auth_err;
	return 0;

process_auth_err:
	OPENSSL_LOG(ERR, "Process openssl auth failed");
	return -EINVAL;
}

# if OPENSSL_VERSION_NUMBER >= 0x30000000L
/** Process standard openssl auth algorithms with hmac/cmac */
static int
process_openssl_auth_mac(struct rte_mbuf *mbuf_src, uint8_t *dst, int offset,
		int srclen, EVP_MAC_CTX *ctx)
{
	size_t dstlen;
	struct rte_mbuf *m;
	int l, n = srclen;
	uint8_t *src;

	for (m = mbuf_src; m != NULL && offset > rte_pktmbuf_data_len(m);
			m = m->next)
		offset -= rte_pktmbuf_data_len(m);

	if (m == 0)
		goto process_auth_err;

	src = rte_pktmbuf_mtod_offset(m, uint8_t *, offset);

	l = rte_pktmbuf_data_len(m) - offset;
	if (srclen <= l) {
		if (EVP_MAC_update(ctx, (unsigned char *)src, srclen) != 1)
			goto process_auth_err;
		goto process_auth_final;
	}

	if (EVP_MAC_update(ctx, (unsigned char *)src, l) != 1)
		goto process_auth_err;

	n -= l;

	for (m = m->next; (m != NULL) && (n > 0); m = m->next) {
		src = rte_pktmbuf_mtod(m, uint8_t *);
		l = rte_pktmbuf_data_len(m) < n ? rte_pktmbuf_data_len(m) : n;
		if (EVP_MAC_update(ctx, (unsigned char *)src, l) != 1)
			goto process_auth_err;
		n -= l;
	}

process_auth_final:
	if (EVP_MAC_final(ctx, dst, &dstlen, DIGEST_LENGTH_MAX) != 1)
		goto process_auth_err;

	EVP_MAC_CTX_free(ctx);
	return 0;

process_auth_err:
	EVP_MAC_CTX_free(ctx);
	OPENSSL_LOG(ERR, "Process openssl auth failed");
	return -EINVAL;
}
# else
/** Process standard openssl auth algorithms with hmac */
static int
process_openssl_auth_hmac(struct rte_mbuf *mbuf_src, uint8_t *dst, int offset,
		int srclen, HMAC_CTX *ctx)
{
	unsigned int dstlen;
	struct rte_mbuf *m;
	int l, n = srclen;
	uint8_t *src;

	for (m = mbuf_src; m != NULL && offset > rte_pktmbuf_data_len(m);
			m = m->next)
		offset -= rte_pktmbuf_data_len(m);

	if (m == 0)
		goto process_auth_err;

	src = rte_pktmbuf_mtod_offset(m, uint8_t *, offset);

	l = rte_pktmbuf_data_len(m) - offset;
	if (srclen <= l) {
		if (HMAC_Update(ctx, (unsigned char *)src, srclen) != 1)
			goto process_auth_err;
		goto process_auth_final;
	}

	if (HMAC_Update(ctx, (unsigned char *)src, l) != 1)
		goto process_auth_err;

	n -= l;

	for (m = m->next; (m != NULL) && (n > 0); m = m->next) {
		src = rte_pktmbuf_mtod(m, uint8_t *);
		l = rte_pktmbuf_data_len(m) < n ? rte_pktmbuf_data_len(m) : n;
		if (HMAC_Update(ctx, (unsigned char *)src, l) != 1)
			goto process_auth_err;
		n -= l;
	}

process_auth_final:
	if (HMAC_Final(ctx, dst, &dstlen) != 1)
		goto process_auth_err;

	if (unlikely(HMAC_Init_ex(ctx, NULL, 0, NULL, NULL) != 1))
		goto process_auth_err;

	return 0;

process_auth_err:
	OPENSSL_LOG(ERR, "Process openssl auth failed");
	return -EINVAL;
}

/** Process standard openssl auth algorithms with cmac */
static int
process_openssl_auth_cmac(struct rte_mbuf *mbuf_src, uint8_t *dst, int offset,
		int srclen, CMAC_CTX *ctx)
{
	unsigned int dstlen;
	struct rte_mbuf *m;
	int l, n = srclen;
	uint8_t *src;

	for (m = mbuf_src; m != NULL && offset > rte_pktmbuf_data_len(m);
			m = m->next)
		offset -= rte_pktmbuf_data_len(m);

	if (m == 0)
		goto process_auth_err;

	src = rte_pktmbuf_mtod_offset(m, uint8_t *, offset);

	l = rte_pktmbuf_data_len(m) - offset;
	if (srclen <= l) {
		if (CMAC_Update(ctx, (unsigned char *)src, srclen) != 1)
			goto process_auth_err;
		goto process_auth_final;
	}

	if (CMAC_Update(ctx, (unsigned char *)src, l) != 1)
		goto process_auth_err;

	n -= l;

	for (m = m->next; (m != NULL) && (n > 0); m = m->next) {
		src = rte_pktmbuf_mtod(m, uint8_t *);
		l = rte_pktmbuf_data_len(m) < n ? rte_pktmbuf_data_len(m) : n;
		if (CMAC_Update(ctx, (unsigned char *)src, l) != 1)
			goto process_auth_err;
		n -= l;
	}

process_auth_final:
	if (CMAC_Final(ctx, dst, (size_t *)&dstlen) != 1)
		goto process_auth_err;

	CMAC_CTX_cleanup(ctx);

	return 0;

process_auth_err:
	OPENSSL_LOG(ERR, "Process openssl cmac auth failed");
	return -EINVAL;
}
# endif
/*----------------------------------------------------------------------------*/

/** Process auth/cipher combined operation */
static void
process_openssl_combined_op
		(struct rte_crypto_op *op, struct openssl_session *sess,
		struct rte_mbuf *mbuf_src, struct rte_mbuf *mbuf_dst)
{
	/* cipher */
	uint8_t *dst = NULL, *iv, *tag, *aad;
	int srclen, aadlen, status = -1;
	uint32_t offset;
	uint8_t taglen;

	/*
	 * Segmented destination buffer is not supported for
	 * encryption/decryption
	 */
	if (!rte_pktmbuf_is_contiguous(mbuf_dst)) {
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		return;
	}

	iv = rte_crypto_op_ctod_offset(op, uint8_t *,
			sess->iv.offset);
	if (sess->auth.algo == RTE_CRYPTO_AUTH_AES_GMAC) {
		srclen = 0;
		offset = op->sym->auth.data.offset;
		aadlen = op->sym->auth.data.length;
		aad = rte_pktmbuf_mtod_offset(mbuf_src, uint8_t *,
				op->sym->auth.data.offset);
		tag = op->sym->auth.digest.data;
		if (tag == NULL)
			tag = rte_pktmbuf_mtod_offset(mbuf_dst, uint8_t *,
				offset + aadlen);
	} else {
		srclen = op->sym->aead.data.length;
		dst = rte_pktmbuf_mtod_offset(mbuf_dst, uint8_t *,
				op->sym->aead.data.offset);
		offset = op->sym->aead.data.offset;
		aad = op->sym->aead.aad.data;
		aadlen = sess->auth.aad_length;
		tag = op->sym->aead.digest.data;
		if (tag == NULL)
			tag = rte_pktmbuf_mtod_offset(mbuf_dst, uint8_t *,
				offset + srclen);
	}

	taglen = sess->auth.digest_length;

	if (sess->cipher.direction == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
		if (sess->auth.algo == RTE_CRYPTO_AUTH_AES_GMAC ||
				sess->aead_algo == RTE_CRYPTO_AEAD_AES_GCM)
			status = process_openssl_auth_encryption_gcm(
					mbuf_src, offset, srclen,
					aad, aadlen, iv,
					dst, tag, sess->cipher.ctx);
		else
			status = process_openssl_auth_encryption_ccm(
					mbuf_src, offset, srclen,
					aad, aadlen, iv,
					dst, tag, taglen, sess->cipher.ctx);

	} else {
		if (sess->auth.algo == RTE_CRYPTO_AUTH_AES_GMAC ||
				sess->aead_algo == RTE_CRYPTO_AEAD_AES_GCM)
			status = process_openssl_auth_decryption_gcm(
					mbuf_src, offset, srclen,
					aad, aadlen, iv,
					dst, tag, sess->cipher.ctx);
		else
			status = process_openssl_auth_decryption_ccm(
					mbuf_src, offset, srclen,
					aad, aadlen, iv,
					dst, tag, taglen, sess->cipher.ctx);
	}

	if (status != 0) {
		if (status == (-EFAULT) &&
				sess->auth.operation ==
						RTE_CRYPTO_AUTH_OP_VERIFY)
			op->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
		else
			op->status = RTE_CRYPTO_OP_STATUS_ERROR;
	}
}

/** Process cipher operation */
static void
process_openssl_cipher_op
		(struct rte_crypto_op *op, struct openssl_session *sess,
		struct rte_mbuf *mbuf_src, struct rte_mbuf *mbuf_dst)
{
	uint8_t *dst, *iv;
	int srclen, status;
	uint8_t inplace = (mbuf_src == mbuf_dst) ? 1 : 0;
	EVP_CIPHER_CTX *ctx_copy;

	/*
	 * Segmented OOP destination buffer is not supported for encryption/
	 * decryption. In case of des3ctr, even inplace segmented buffers are
	 * not supported.
	 */
	if (!rte_pktmbuf_is_contiguous(mbuf_dst) &&
			(!inplace || sess->cipher.mode != OPENSSL_CIPHER_LIB)) {
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		return;
	}

	srclen = op->sym->cipher.data.length;
	dst = rte_pktmbuf_mtod_offset(mbuf_dst, uint8_t *,
			op->sym->cipher.data.offset);

	iv = rte_crypto_op_ctod_offset(op, uint8_t *,
			sess->iv.offset);
	ctx_copy = EVP_CIPHER_CTX_new();
	EVP_CIPHER_CTX_copy(ctx_copy, sess->cipher.ctx);

	if (sess->cipher.mode == OPENSSL_CIPHER_LIB)
		if (sess->cipher.direction == RTE_CRYPTO_CIPHER_OP_ENCRYPT)
			status = process_openssl_cipher_encrypt(mbuf_src, dst,
					op->sym->cipher.data.offset, iv,
					srclen, ctx_copy, inplace);
		else
			status = process_openssl_cipher_decrypt(mbuf_src, dst,
					op->sym->cipher.data.offset, iv,
					srclen, ctx_copy, inplace);
	else
		status = process_openssl_cipher_des3ctr(mbuf_src, dst,
				op->sym->cipher.data.offset, iv,
				sess->cipher.key.data, srclen,
				ctx_copy);

	EVP_CIPHER_CTX_free(ctx_copy);
	if (status != 0)
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
}

/** Process cipher operation */
static void
process_openssl_docsis_bpi_op(struct rte_crypto_op *op,
		struct openssl_session *sess, struct rte_mbuf *mbuf_src,
		struct rte_mbuf *mbuf_dst)
{
	uint8_t *src, *dst, *iv;
	uint8_t block_size, last_block_len;
	int srclen, status = 0;

	srclen = op->sym->cipher.data.length;
	src = rte_pktmbuf_mtod_offset(mbuf_src, uint8_t *,
			op->sym->cipher.data.offset);
	dst = rte_pktmbuf_mtod_offset(mbuf_dst, uint8_t *,
			op->sym->cipher.data.offset);

	iv = rte_crypto_op_ctod_offset(op, uint8_t *,
			sess->iv.offset);

	block_size = DES_BLOCK_SIZE;

	last_block_len = srclen % block_size;
	if (sess->cipher.direction == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
		/* Encrypt only with ECB mode XOR IV */
		if (srclen < block_size) {
			status = process_openssl_cipher_bpi_encrypt(src, dst,
					iv, srclen,
					sess->cipher.bpi_ctx);
		} else {
			srclen -= last_block_len;
			/* Encrypt with the block aligned stream with CBC mode */
			status = process_openssl_cipher_encrypt(mbuf_src, dst,
					op->sym->cipher.data.offset, iv,
					srclen, sess->cipher.ctx, 0);
			if (last_block_len) {
				/* Point at last block */
				dst += srclen;
				/*
				 * IV is the last encrypted block from
				 * the previous operation
				 */
				iv = dst - block_size;
				src += srclen;
				srclen = last_block_len;
				/* Encrypt the last frame with ECB mode */
				status |= process_openssl_cipher_bpi_encrypt(src,
						dst, iv,
						srclen, sess->cipher.bpi_ctx);
			}
		}
	} else {
		/* Decrypt only with ECB mode (encrypt, as it is same operation) */
		if (srclen < block_size) {
			status = process_openssl_cipher_bpi_encrypt(src, dst,
					iv,
					srclen,
					sess->cipher.bpi_ctx);
		} else {
			if (last_block_len) {
				/* Point at last block */
				dst += srclen - last_block_len;
				src += srclen - last_block_len;
				/*
				 * IV is the last full block
				 */
				iv = src - block_size;
				/*
				 * Decrypt the last frame with ECB mode
				 * (encrypt, as it is the same operation)
				 */
				status = process_openssl_cipher_bpi_encrypt(src,
						dst, iv,
						last_block_len, sess->cipher.bpi_ctx);
				/* Prepare parameters for CBC mode op */
				iv = rte_crypto_op_ctod_offset(op, uint8_t *,
						sess->iv.offset);
				dst += last_block_len - srclen;
				srclen -= last_block_len;
			}

			/* Decrypt with CBC mode */
			status |= process_openssl_cipher_decrypt(mbuf_src, dst,
					op->sym->cipher.data.offset, iv,
					srclen, sess->cipher.ctx, 0);
		}
	}

	if (status != 0)
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
}

/** Process auth operation */
static void
process_openssl_auth_op(struct openssl_qp *qp, struct rte_crypto_op *op,
		struct openssl_session *sess, struct rte_mbuf *mbuf_src,
		struct rte_mbuf *mbuf_dst)
{
	uint8_t *dst;
	int srclen, status;
	EVP_MD_CTX *ctx_a;
# if OPENSSL_VERSION_NUMBER >= 0x30000000L
	EVP_MAC_CTX *ctx_h;
	EVP_MAC_CTX *ctx_c;
# else
	HMAC_CTX *ctx_h;
	CMAC_CTX *ctx_c;
# endif

	srclen = op->sym->auth.data.length;

	dst = qp->temp_digest;

	switch (sess->auth.mode) {
	case OPENSSL_AUTH_AS_AUTH:
		ctx_a = EVP_MD_CTX_create();
		EVP_MD_CTX_copy_ex(ctx_a, sess->auth.auth.ctx);
		status = process_openssl_auth(mbuf_src, dst,
				op->sym->auth.data.offset, NULL, NULL, srclen,
				ctx_a, sess->auth.auth.evp_algo);
		EVP_MD_CTX_destroy(ctx_a);
		break;
	case OPENSSL_AUTH_AS_HMAC:
# if OPENSSL_VERSION_NUMBER >= 0x30000000L
		ctx_h = EVP_MAC_CTX_dup(sess->auth.hmac.ctx);
		status = process_openssl_auth_mac(mbuf_src, dst,
				op->sym->auth.data.offset, srclen,
				ctx_h);
# else
		ctx_h = HMAC_CTX_new();
		HMAC_CTX_copy(ctx_h, sess->auth.hmac.ctx);
		status = process_openssl_auth_hmac(mbuf_src, dst,
				op->sym->auth.data.offset, srclen,
				ctx_h);
		HMAC_CTX_free(ctx_h);
# endif
		break;
	case OPENSSL_AUTH_AS_CMAC:
# if OPENSSL_VERSION_NUMBER >= 0x30000000L
		ctx_c = EVP_MAC_CTX_dup(sess->auth.cmac.ctx);
		status = process_openssl_auth_mac(mbuf_src, dst,
				op->sym->auth.data.offset, srclen,
				ctx_c);
# else
		ctx_c = CMAC_CTX_new();
		CMAC_CTX_copy(ctx_c, sess->auth.cmac.ctx);
		status = process_openssl_auth_cmac(mbuf_src, dst,
				op->sym->auth.data.offset, srclen,
				ctx_c);
		CMAC_CTX_free(ctx_c);
# endif
		break;
	default:
		status = -1;
		break;
	}

	if (sess->auth.operation == RTE_CRYPTO_AUTH_OP_VERIFY) {
		if (CRYPTO_memcmp(dst, op->sym->auth.digest.data,
				sess->auth.digest_length) != 0) {
			op->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
		}
	} else {
		uint8_t *auth_dst;

		auth_dst = op->sym->auth.digest.data;
		if (auth_dst == NULL)
			auth_dst = rte_pktmbuf_mtod_offset(mbuf_dst, uint8_t *,
					op->sym->auth.data.offset +
					op->sym->auth.data.length);
		memcpy(auth_dst, dst, sess->auth.digest_length);
	}

	if (status != 0)
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
}

/* process dsa sign operation */
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
static int
process_openssl_dsa_sign_op_evp(struct rte_crypto_op *cop,
		struct openssl_asym_session *sess)
{
	struct rte_crypto_dsa_op_param *op = &cop->asym->dsa;
	EVP_PKEY_CTX *dsa_ctx = NULL;
	EVP_PKEY_CTX *key_ctx = EVP_PKEY_CTX_new_from_name(NULL, "DSA", NULL);
	EVP_PKEY *pkey = NULL;
	OSSL_PARAM_BLD *param_bld = sess->u.s.param_bld;
	OSSL_PARAM *params = NULL;

	size_t outlen;
	unsigned char *dsa_sign_data;
	const unsigned char *dsa_sign_data_p;

	cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
	params = OSSL_PARAM_BLD_to_param(param_bld);
	if (!params) {
		OSSL_PARAM_BLD_free(param_bld);
		return -1;
	}

	if (key_ctx == NULL
		|| EVP_PKEY_fromdata_init(key_ctx) <= 0
		|| EVP_PKEY_fromdata(key_ctx, &pkey,
			EVP_PKEY_KEYPAIR, params) <= 0)
		goto err_dsa_sign;

	dsa_ctx = EVP_PKEY_CTX_new(pkey, NULL);
	if (!dsa_ctx)
		goto err_dsa_sign;

	if (EVP_PKEY_sign_init(dsa_ctx) <= 0)
		goto err_dsa_sign;

	if (EVP_PKEY_sign(dsa_ctx, NULL, &outlen, op->message.data,
						op->message.length) <= 0)
		goto err_dsa_sign;

	if (outlen <= 0)
		goto err_dsa_sign;

	dsa_sign_data = OPENSSL_malloc(outlen);
	if (!dsa_sign_data)
		goto err_dsa_sign;

	if (EVP_PKEY_sign(dsa_ctx, dsa_sign_data, &outlen, op->message.data,
						op->message.length) <= 0) {
		OPENSSL_free(dsa_sign_data);
		goto err_dsa_sign;
	}

	dsa_sign_data_p = (const unsigned char *)dsa_sign_data;
	DSA_SIG *sign = d2i_DSA_SIG(NULL, &dsa_sign_data_p, outlen);
	if (!sign) {
		OPENSSL_LOG(ERR, "%s:%d\n", __func__, __LINE__);
		OPENSSL_free(dsa_sign_data);
		goto err_dsa_sign;
	} else {
		const BIGNUM *r = NULL, *s = NULL;
		get_dsa_sign(sign, &r, &s);

		op->r.length = BN_bn2bin(r, op->r.data);
		op->s.length = BN_bn2bin(s, op->s.data);
		cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
	}

	DSA_SIG_free(sign);
	OPENSSL_free(dsa_sign_data);
	return 0;

err_dsa_sign:
	if (params)
		OSSL_PARAM_free(params);
	if (key_ctx)
		EVP_PKEY_CTX_free(key_ctx);
	if (dsa_ctx)
		EVP_PKEY_CTX_free(dsa_ctx);
	return -1;
}

/* process dsa verify operation */
static int
process_openssl_dsa_verify_op_evp(struct rte_crypto_op *cop,
		struct openssl_asym_session *sess)
{
	struct rte_crypto_dsa_op_param *op = &cop->asym->dsa;
	DSA_SIG *sign = DSA_SIG_new();
	BIGNUM *r = NULL, *s = NULL;
	BIGNUM *pub_key = NULL;
	OSSL_PARAM_BLD *param_bld = sess->u.s.param_bld;
	OSSL_PARAM *params = NULL;
	EVP_PKEY *pkey = NULL;
	EVP_PKEY_CTX *dsa_ctx = NULL;
	EVP_PKEY_CTX *key_ctx = EVP_PKEY_CTX_new_from_name(NULL, "DSA", NULL);
	unsigned char *dsa_sig = NULL;
	size_t sig_len;
	int ret = -1;

	cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
	if (!param_bld) {
		OPENSSL_LOG(ERR, " %s:%d\n", __func__, __LINE__);
		return -1;
	}

	r = BN_bin2bn(op->r.data, op->r.length, r);
	s = BN_bin2bn(op->s.data, op->s.length,	s);
	pub_key = BN_bin2bn(op->y.data, op->y.length, pub_key);
	if (!r || !s || !pub_key) {
		BN_free(r);
		BN_free(s);
		BN_free(pub_key);
		OSSL_PARAM_BLD_free(param_bld);
		goto err_dsa_verify;
	}

	set_dsa_sign(sign, r, s);
	if (!OSSL_PARAM_BLD_push_BN(param_bld, OSSL_PKEY_PARAM_PUB_KEY, pub_key)) {
		OSSL_PARAM_BLD_free(param_bld);
		goto err_dsa_verify;
	}

	params = OSSL_PARAM_BLD_to_param(param_bld);
	if (!params) {
		OSSL_PARAM_BLD_free(param_bld);
		goto err_dsa_verify;
	}

	if (key_ctx == NULL
		|| EVP_PKEY_fromdata_init(key_ctx) <= 0
		|| EVP_PKEY_fromdata(key_ctx, &pkey, EVP_PKEY_KEYPAIR, params) <= 0)
		goto err_dsa_verify;

	dsa_ctx = EVP_PKEY_CTX_new(pkey, NULL);
	if (!dsa_ctx)
		goto err_dsa_verify;

	if (!sign)
		goto err_dsa_verify;

	sig_len = i2d_DSA_SIG(sign, &dsa_sig);
	if (EVP_PKEY_verify_init(dsa_ctx) <= 0)
		goto err_dsa_verify;

	ret = EVP_PKEY_verify(dsa_ctx, dsa_sig, sig_len,
					op->message.data, op->message.length);
	if (ret == 1) {
		cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
		ret = 0;
	}

err_dsa_verify:
	if (sign)
		DSA_SIG_free(sign);
	if (params)
		OSSL_PARAM_free(params);
	if (key_ctx)
		EVP_PKEY_CTX_free(key_ctx);
	if (dsa_ctx)
		EVP_PKEY_CTX_free(dsa_ctx);

	return ret;
}
#else
static int
process_openssl_dsa_sign_op(struct rte_crypto_op *cop,
		struct openssl_asym_session *sess)
{
	struct rte_crypto_dsa_op_param *op = &cop->asym->dsa;
	DSA *dsa = sess->u.s.dsa;
	DSA_SIG *sign = NULL;

	sign = DSA_do_sign(op->message.data,
			op->message.length,
			dsa);

	if (sign == NULL) {
		OPENSSL_LOG(ERR, "%s:%d\n", __func__, __LINE__);
		cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
	} else {
		const BIGNUM *r = NULL, *s = NULL;
		get_dsa_sign(sign, &r, &s);

		op->r.length = BN_bn2bin(r, op->r.data);
		op->s.length = BN_bn2bin(s, op->s.data);
		cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
	}

	DSA_SIG_free(sign);

	return 0;
}

/* process dsa verify operation */
static int
process_openssl_dsa_verify_op(struct rte_crypto_op *cop,
		struct openssl_asym_session *sess)
{
	struct rte_crypto_dsa_op_param *op = &cop->asym->dsa;
	DSA *dsa = sess->u.s.dsa;
	int ret;
	DSA_SIG *sign = DSA_SIG_new();
	BIGNUM *r = NULL, *s = NULL;
	BIGNUM *pub_key = NULL;

	if (sign == NULL) {
		OPENSSL_LOG(ERR, " %s:%d\n", __func__, __LINE__);
		cop->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
		return -1;
	}

	r = BN_bin2bn(op->r.data,
			op->r.length,
			r);
	s = BN_bin2bn(op->s.data,
			op->s.length,
			s);
	pub_key = BN_bin2bn(op->y.data,
			op->y.length,
			pub_key);
	if (!r || !s || !pub_key) {
		BN_free(r);
		BN_free(s);
		BN_free(pub_key);

		cop->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
		return -1;
	}
	set_dsa_sign(sign, r, s);
	set_dsa_pub_key(dsa, pub_key);

	ret = DSA_do_verify(op->message.data,
			op->message.length,
			sign,
			dsa);

	if (ret != 1)
		cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
	else
		cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;

	DSA_SIG_free(sign);

	return 0;
}
#endif

/* process dh operation */
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
static int
process_openssl_dh_op_evp(struct rte_crypto_op *cop,
		struct openssl_asym_session *sess)
{
	struct rte_crypto_dh_op_param *op = &cop->asym->dh;
	OSSL_PARAM_BLD *param_bld = sess->u.dh.param_bld;
	OSSL_PARAM_BLD *param_bld_peer = sess->u.dh.param_bld_peer;
	OSSL_PARAM *params = NULL;
	EVP_PKEY *dhpkey = NULL;
	EVP_PKEY *peerkey = NULL;
	BIGNUM *priv_key = NULL;
	BIGNUM *pub_key = NULL;
	int ret = -1;

	cop->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
	EVP_PKEY_CTX *dh_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_DH, NULL);
	if (dh_ctx == NULL || param_bld == NULL)
		return ret;

	if (op->ke_type == RTE_CRYPTO_ASYM_KE_SHARED_SECRET_COMPUTE) {
		OSSL_PARAM *params_peer = NULL;

		if (!param_bld_peer)
			return ret;

		pub_key = BN_bin2bn(op->pub_key.data, op->pub_key.length,
					pub_key);
		if (pub_key == NULL) {
			OSSL_PARAM_BLD_free(param_bld_peer);
			return ret;
		}

		if (!OSSL_PARAM_BLD_push_BN(param_bld_peer, OSSL_PKEY_PARAM_PUB_KEY,
				pub_key)) {
			OPENSSL_LOG(ERR, "Failed to set public key\n");
			OSSL_PARAM_BLD_free(param_bld_peer);
			BN_free(pub_key);
			return ret;
		}

		params_peer = OSSL_PARAM_BLD_to_param(param_bld_peer);
		if (!params_peer) {
			OSSL_PARAM_BLD_free(param_bld_peer);
			BN_free(pub_key);
			return ret;
		}

		EVP_PKEY_CTX *peer_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_DH, NULL);
		if (EVP_PKEY_keygen_init(peer_ctx) != 1) {
			OSSL_PARAM_free(params_peer);
			BN_free(pub_key);
			return ret;
		}

		if (EVP_PKEY_CTX_set_params(peer_ctx, params_peer) != 1) {
			EVP_PKEY_CTX_free(peer_ctx);
			OSSL_PARAM_free(params_peer);
			BN_free(pub_key);
			return ret;
		}

		if (EVP_PKEY_keygen(peer_ctx, &peerkey) != 1) {
			EVP_PKEY_CTX_free(peer_ctx);
			OSSL_PARAM_free(params_peer);
			BN_free(pub_key);
			return ret;
		}

		priv_key = BN_bin2bn(op->priv_key.data, op->priv_key.length,
					priv_key);
		if (priv_key == NULL) {
			EVP_PKEY_CTX_free(peer_ctx);
			OSSL_PARAM_free(params_peer);
			BN_free(pub_key);
			return ret;
		}

		if (!OSSL_PARAM_BLD_push_BN(param_bld, OSSL_PKEY_PARAM_PRIV_KEY,
				priv_key)) {
			OPENSSL_LOG(ERR, "Failed to set private key\n");
			EVP_PKEY_CTX_free(peer_ctx);
			OSSL_PARAM_free(params_peer);
			BN_free(pub_key);
			BN_free(priv_key);
			return ret;
		}

		OSSL_PARAM_free(params_peer);
		EVP_PKEY_CTX_free(peer_ctx);
	}

	params = OSSL_PARAM_BLD_to_param(param_bld);
	if (!params)
		goto err_dh;

	if (EVP_PKEY_keygen_init(dh_ctx) != 1)
		goto err_dh;

	if (EVP_PKEY_CTX_set_params(dh_ctx, params) != 1)
		goto err_dh;

	if (EVP_PKEY_keygen(dh_ctx, &dhpkey) != 1)
		goto err_dh;

	if (op->ke_type == RTE_CRYPTO_ASYM_KE_PUB_KEY_GENERATE) {
		OPENSSL_LOG(DEBUG, "%s:%d updated pub key\n", __func__, __LINE__);
		if (!EVP_PKEY_get_bn_param(dhpkey, OSSL_PKEY_PARAM_PUB_KEY, &pub_key))
			goto err_dh;
				/* output public key */
		op->pub_key.length = BN_bn2bin(pub_key, op->pub_key.data);
	}

	if (op->ke_type == RTE_CRYPTO_ASYM_KE_PRIV_KEY_GENERATE) {

		OPENSSL_LOG(DEBUG, "%s:%d updated priv key\n", __func__, __LINE__);
		if (!EVP_PKEY_get_bn_param(dhpkey, OSSL_PKEY_PARAM_PRIV_KEY, &priv_key))
			goto err_dh;

		/* provide generated private key back to user */
		op->priv_key.length = BN_bn2bin(priv_key, op->priv_key.data);
	}

	if (op->ke_type == RTE_CRYPTO_ASYM_KE_SHARED_SECRET_COMPUTE) {
		size_t skey_len;
		EVP_PKEY_CTX *sc_ctx = EVP_PKEY_CTX_new(dhpkey, NULL);
		if (!sc_ctx)
			goto err_dh;

		if (EVP_PKEY_derive_init(sc_ctx) <= 0) {
			EVP_PKEY_CTX_free(sc_ctx);
			goto err_dh;
		}

		if (!peerkey) {
			EVP_PKEY_CTX_free(sc_ctx);
			goto err_dh;
		}

		if (EVP_PKEY_derive_set_peer(sc_ctx, peerkey) <= 0) {
			EVP_PKEY_CTX_free(sc_ctx);
			goto err_dh;
		}

		/* Determine buffer length */
		if (EVP_PKEY_derive(sc_ctx, NULL, &skey_len) <= 0) {
			EVP_PKEY_CTX_free(sc_ctx);
			goto err_dh;
		}

		if (EVP_PKEY_derive(sc_ctx, op->shared_secret.data, &skey_len) <= 0) {
			EVP_PKEY_CTX_free(sc_ctx);
			goto err_dh;
		}

		op->shared_secret.length = skey_len;
		EVP_PKEY_CTX_free(sc_ctx);
	}

	cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
	ret = 0;

 err_dh:
	if (pub_key)
		BN_free(pub_key);
	if (priv_key)
		BN_free(priv_key);
	if (params)
		OSSL_PARAM_free(params);
	if (dhpkey)
		EVP_PKEY_free(dhpkey);
	if (peerkey)
		EVP_PKEY_free(peerkey);

	EVP_PKEY_CTX_free(dh_ctx);

	return ret;
}
#else
static int
process_openssl_dh_op(struct rte_crypto_op *cop,
		struct openssl_asym_session *sess)
{
	struct rte_crypto_dh_op_param *op = &cop->asym->dh;
	struct rte_crypto_asym_op *asym_op = cop->asym;
	DH *dh_key = sess->u.dh.dh_key;
	BIGNUM *priv_key = NULL;
	int ret = 0;

	if (asym_op->dh.ke_type == RTE_CRYPTO_ASYM_KE_SHARED_SECRET_COMPUTE) {
		/* compute shared secret using peer public key
		 * and current private key
		 * shared secret = peer_key ^ priv_key mod p
		 */
		BIGNUM *peer_key = NULL;

		/* copy private key and peer key and compute shared secret */
		peer_key = BN_bin2bn(op->pub_key.data,
				op->pub_key.length,
				peer_key);
		if (peer_key == NULL) {
			cop->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
			return -1;
		}
		priv_key = BN_bin2bn(op->priv_key.data,
				op->priv_key.length,
				priv_key);
		if (priv_key == NULL) {
			BN_free(peer_key);
			cop->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
			return -1;
		}
		ret = set_dh_priv_key(dh_key, priv_key);
		if (ret) {
			OPENSSL_LOG(ERR, "Failed to set private key\n");
			cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
			BN_free(peer_key);
			BN_free(priv_key);
			return 0;
		}

		ret = DH_compute_key(
				op->shared_secret.data,
				peer_key, dh_key);
		if (ret < 0) {
			cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
			BN_free(peer_key);
			/* priv key is already loaded into dh,
			 * let's not free that directly here.
			 * DH_free() will auto free it later.
			 */
			return 0;
		}
		cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
		op->shared_secret.length = ret;
		BN_free(peer_key);
		return 0;
	}

	/*
	 * other options are public and private key generations.
	 *
	 * if user provides private key,
	 * then first set DH with user provided private key
	 */
	if (asym_op->dh.ke_type == RTE_CRYPTO_ASYM_KE_PUB_KEY_GENERATE &&
			op->priv_key.length) {
		/* generate public key using user-provided private key
		 * pub_key = g ^ priv_key mod p
		 */

		/* load private key into DH */
		priv_key = BN_bin2bn(op->priv_key.data,
				op->priv_key.length,
				priv_key);
		if (priv_key == NULL) {
			cop->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
			return -1;
		}
		ret = set_dh_priv_key(dh_key, priv_key);
		if (ret) {
			OPENSSL_LOG(ERR, "Failed to set private key\n");
			cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
			BN_free(priv_key);
			return 0;
		}
	}

	/* generate public and private key pair.
	 *
	 * if private key already set, generates only public key.
	 *
	 * if private key is not already set, then set it to random value
	 * and update internal private key.
	 */
	if (!DH_generate_key(dh_key)) {
		cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
		return 0;
	}

	if (asym_op->dh.ke_type == RTE_CRYPTO_ASYM_KE_PUB_KEY_GENERATE) {
		const BIGNUM *pub_key = NULL;

		OPENSSL_LOG(DEBUG, "%s:%d update public key\n",
				__func__, __LINE__);

		/* get the generated keys */
		get_dh_pub_key(dh_key, &pub_key);

		/* output public key */
		op->pub_key.length = BN_bn2bin(pub_key,
				op->pub_key.data);
	}

	if (asym_op->dh.ke_type == RTE_CRYPTO_ASYM_KE_PRIV_KEY_GENERATE) {
		const BIGNUM *priv_key = NULL;

		OPENSSL_LOG(DEBUG, "%s:%d updated priv key\n",
				__func__, __LINE__);

		/* get the generated keys */
		get_dh_priv_key(dh_key, &priv_key);

		/* provide generated private key back to user */
		op->priv_key.length = BN_bn2bin(priv_key,
				op->priv_key.data);
	}

	cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;

	return 0;
}
#endif

/* process modinv operation */
static int
process_openssl_modinv_op(struct rte_crypto_op *cop,
		struct openssl_asym_session *sess)
{
	struct rte_crypto_asym_op *op = cop->asym;
	BIGNUM *base = BN_CTX_get(sess->u.m.ctx);
	BIGNUM *res = BN_CTX_get(sess->u.m.ctx);

	if (unlikely(base == NULL || res == NULL)) {
		BN_free(base);
		BN_free(res);
		cop->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
		return -1;
	}

	base = BN_bin2bn((const unsigned char *)op->modinv.base.data,
			op->modinv.base.length, base);

	if (BN_mod_inverse(res, base, sess->u.m.modulus, sess->u.m.ctx)) {
		cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
		op->modinv.result.length = BN_bn2bin(res, op->modinv.result.data);
	} else {
		cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
	}

	BN_clear(res);
	BN_clear(base);

	return 0;
}

/* process modexp operation */
static int
process_openssl_modexp_op(struct rte_crypto_op *cop,
		struct openssl_asym_session *sess)
{
	struct rte_crypto_asym_op *op = cop->asym;
	BIGNUM *base = BN_CTX_get(sess->u.e.ctx);
	BIGNUM *res = BN_CTX_get(sess->u.e.ctx);

	if (unlikely(base == NULL || res == NULL)) {
		BN_free(base);
		BN_free(res);
		cop->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
		return -1;
	}

	base = BN_bin2bn((const unsigned char *)op->modex.base.data,
			op->modex.base.length, base);

	if (BN_mod_exp(res, base, sess->u.e.exp,
				sess->u.e.mod, sess->u.e.ctx)) {
		op->modex.result.length = BN_bn2bin(res, op->modex.result.data);
		cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
	} else {
		cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
	}

	BN_clear(res);
	BN_clear(base);

	return 0;
}

/* process rsa operations */
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
static int
process_openssl_rsa_op_evp(struct rte_crypto_op *cop,
		struct openssl_asym_session *sess)
{
	struct rte_crypto_asym_op *op = cop->asym;
	uint32_t pad = (op->rsa.padding.type);
	uint8_t *tmp;
	size_t outlen = 0;
	int ret = -1;

	cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
	EVP_PKEY_CTX *rsa_ctx = sess->u.r.ctx;
	if (!rsa_ctx)
		return ret;

	switch (pad) {
	case RTE_CRYPTO_RSA_PADDING_PKCS1_5:
		pad = RSA_PKCS1_PADDING;
		break;
	case RTE_CRYPTO_RSA_PADDING_NONE:
		pad = RSA_NO_PADDING;
		break;
	default:
		cop->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		OPENSSL_LOG(ERR,
				"rsa pad type not supported %d\n", pad);
		return ret;
	}

	switch (op->rsa.op_type) {
	case RTE_CRYPTO_ASYM_OP_ENCRYPT:
		if (EVP_PKEY_encrypt_init(rsa_ctx) != 1)
			goto err_rsa;

		if (EVP_PKEY_CTX_set_rsa_padding(rsa_ctx, pad) <= 0)
			goto err_rsa;

		if (EVP_PKEY_encrypt(rsa_ctx, NULL, &outlen,
				op->rsa.message.data,
				op->rsa.message.length) <= 0)
			goto err_rsa;

		if (outlen <= 0)
			goto err_rsa;

		if (EVP_PKEY_encrypt(rsa_ctx, op->rsa.cipher.data, &outlen,
				op->rsa.message.data,
				op->rsa.message.length) <= 0)
			goto err_rsa;
		op->rsa.cipher.length = outlen;

		OPENSSL_LOG(DEBUG,
				"length of encrypted text %zu\n", outlen);
		break;

	case RTE_CRYPTO_ASYM_OP_DECRYPT:
		if (EVP_PKEY_decrypt_init(rsa_ctx) != 1)
			goto err_rsa;

		if (EVP_PKEY_CTX_set_rsa_padding(rsa_ctx, pad) <= 0)
			goto err_rsa;

		if (EVP_PKEY_decrypt(rsa_ctx, NULL, &outlen,
				op->rsa.cipher.data,
				op->rsa.cipher.length) <= 0)
			goto err_rsa;

		if (outlen <= 0)
			goto err_rsa;

		if (EVP_PKEY_decrypt(rsa_ctx, op->rsa.message.data, &outlen,
				op->rsa.cipher.data,
				op->rsa.cipher.length) <= 0)
			goto err_rsa;
		op->rsa.message.length = outlen;

		OPENSSL_LOG(DEBUG, "length of decrypted text %zu\n", outlen);
		break;

	case RTE_CRYPTO_ASYM_OP_SIGN:
		if (EVP_PKEY_sign_init(rsa_ctx) <= 0)
			goto err_rsa;

		if (EVP_PKEY_CTX_set_rsa_padding(rsa_ctx, pad) <= 0)
			goto err_rsa;

		if (EVP_PKEY_sign(rsa_ctx, NULL, &outlen,
				op->rsa.message.data,
				op->rsa.message.length) <= 0)
			goto err_rsa;

		if (outlen <= 0)
			goto err_rsa;

		if (EVP_PKEY_sign(rsa_ctx, op->rsa.sign.data, &outlen,
				op->rsa.message.data,
				op->rsa.message.length) <= 0)
			goto err_rsa;
		op->rsa.sign.length = outlen;
		break;

	case RTE_CRYPTO_ASYM_OP_VERIFY:
		if (EVP_PKEY_verify_recover_init(rsa_ctx) <= 0)
			goto err_rsa;

		if (EVP_PKEY_CTX_set_rsa_padding(rsa_ctx, pad) <= 0)
			goto err_rsa;

		if (EVP_PKEY_verify_recover(rsa_ctx, NULL, &outlen,
				op->rsa.sign.data,
				op->rsa.sign.length) <= 0)
			goto err_rsa;

		if ((outlen <= 0) || (outlen != op->rsa.sign.length))
			goto err_rsa;

		tmp = OPENSSL_malloc(outlen);
		if (tmp == NULL) {
			OPENSSL_LOG(ERR, "Memory allocation failed");
			goto err_rsa;
		}

		if (EVP_PKEY_verify_recover(rsa_ctx, tmp, &outlen,
				op->rsa.sign.data,
				op->rsa.sign.length) <= 0) {
			OPENSSL_free(tmp);
			goto err_rsa;
		}

		OPENSSL_LOG(DEBUG,
				"Length of public_decrypt %zu "
				"length of message %zd\n",
				outlen, op->rsa.message.length);
		if (CRYPTO_memcmp(tmp, op->rsa.message.data,
				op->rsa.message.length)) {
			OPENSSL_LOG(ERR, "RSA sign Verification failed");
		}
		OPENSSL_free(tmp);
		break;

	default:
		/* allow ops with invalid args to be pushed to
		 * completion queue
		 */
		cop->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		goto err_rsa;
	}

	ret = 0;
	cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
err_rsa:
	return ret;

}

static int
process_openssl_sm2_op_evp(struct rte_crypto_op *cop,
		struct openssl_asym_session *sess)
{
	EVP_PKEY_CTX *kctx = NULL, *sctx = NULL, *cctx = NULL;
	struct rte_crypto_asym_op *op = cop->asym;
	OSSL_PARAM_BLD *param_bld = NULL;
	OSSL_PARAM *params = NULL;
	EVP_PKEY *pkey = NULL;
	BIGNUM *pkey_bn = NULL;
	uint8_t pubkey[64];
	size_t len = 0;
	int ret = -1;

	cop->status = RTE_CRYPTO_OP_STATUS_ERROR;

	if (cop->asym->sm2.k.data != NULL)
		goto err_sm2;

	param_bld = OSSL_PARAM_BLD_new();
	if (!param_bld) {
		OPENSSL_LOG(ERR, "failed to allocate params\n");
		goto err_sm2;
	}

	ret = OSSL_PARAM_BLD_push_utf8_string(param_bld,
		OSSL_PKEY_PARAM_GROUP_NAME, "SM2", 0);
	if (!ret) {
		OPENSSL_LOG(ERR, "failed to push params\n");
		goto err_sm2;
	}

	pkey_bn = BN_bin2bn((const unsigned char *)op->sm2.pkey.data,
						op->sm2.pkey.length, pkey_bn);

	memset(pubkey, 0, RTE_DIM(pubkey));
	pubkey[0] = 0x04;
	len += 1;
	memcpy(&pubkey[len], op->sm2.q.x.data, op->sm2.q.x.length);
	len += op->sm2.q.x.length;
	memcpy(&pubkey[len], op->sm2.q.y.data, op->sm2.q.y.length);
	len += op->sm2.q.y.length;

	ret = OSSL_PARAM_BLD_push_BN(param_bld, OSSL_PKEY_PARAM_PRIV_KEY,
								 pkey_bn);
	if (!ret) {
		OPENSSL_LOG(ERR, "failed to push params\n");
		goto err_sm2;
	}

	ret = OSSL_PARAM_BLD_push_octet_string(param_bld,
			OSSL_PKEY_PARAM_PUB_KEY, pubkey, len);
	if (!ret) {
		OPENSSL_LOG(ERR, "failed to push params\n");
		goto err_sm2;
	}

	params = OSSL_PARAM_BLD_to_param(param_bld);
	if (!params) {
		OPENSSL_LOG(ERR, "failed to push params\n");
		goto err_sm2;
	}

	switch (op->sm2.op_type) {
	case RTE_CRYPTO_ASYM_OP_ENCRYPT:
		{
			OSSL_PARAM *eparams = sess->u.sm2.params;
			size_t output_len = 0;

			kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_SM2, NULL);
			if (kctx == NULL || EVP_PKEY_fromdata_init(kctx) <= 0 ||
				EVP_PKEY_fromdata(kctx, &pkey, EVP_PKEY_KEYPAIR, params) <= 0)
				goto err_sm2;

			cctx = EVP_PKEY_CTX_new_from_pkey(NULL, pkey, NULL);
			if (!cctx)
				goto err_sm2;

			if (!EVP_PKEY_encrypt_init(cctx))
				goto err_sm2;

			if (!EVP_PKEY_CTX_set_params(cctx, eparams))
				goto err_sm2;

			if (!EVP_PKEY_encrypt(cctx, op->sm2.cipher.data, &output_len,
								 op->sm2.message.data,
								 op->sm2.message.length))
				goto err_sm2;
			op->sm2.cipher.length = output_len;
		}
		break;
	case RTE_CRYPTO_ASYM_OP_DECRYPT:
		{
			OSSL_PARAM *eparams = sess->u.sm2.params;

			kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_SM2, NULL);
			if (kctx == NULL
				|| EVP_PKEY_fromdata_init(kctx) <= 0
				|| EVP_PKEY_fromdata(kctx, &pkey, EVP_PKEY_KEYPAIR, params) <= 0)
				goto err_sm2;

			cctx = EVP_PKEY_CTX_new_from_pkey(NULL, pkey, NULL);
			if (!cctx)
				goto err_sm2;

			if (!EVP_PKEY_decrypt_init(cctx))
				goto err_sm2;

			if (!EVP_PKEY_CTX_set_params(cctx, eparams))
				goto err_sm2;

			if (!EVP_PKEY_decrypt(cctx, op->sm2.message.data, &op->sm2.message.length,
					op->sm2.cipher.data, op->sm2.cipher.length))
				goto err_sm2;
		}
		break;
	case RTE_CRYPTO_ASYM_OP_SIGN:
		{
			unsigned char signbuf[128] = {0};
			const unsigned char *signptr;
			EVP_MD_CTX *md_ctx = NULL;
			const BIGNUM *r, *s;
			ECDSA_SIG *ec_sign;
			EVP_MD *check_md;
			size_t signlen;

			kctx = EVP_PKEY_CTX_new_from_name(NULL, "SM2", NULL);
			if (kctx == NULL || EVP_PKEY_fromdata_init(kctx) <= 0 ||
				EVP_PKEY_fromdata(kctx, &pkey, EVP_PKEY_KEYPAIR, params) <= 0)
				goto err_sm2;

			md_ctx = EVP_MD_CTX_new();
			if (!md_ctx)
				goto err_sm2;

			sctx = EVP_PKEY_CTX_new_from_pkey(NULL, pkey, NULL);
			if (!sctx)
				goto err_sm2;

			EVP_MD_CTX_set_pkey_ctx(md_ctx, sctx);

			check_md = EVP_MD_fetch(NULL, "sm3", NULL);
			if (!check_md)
				goto err_sm2;

			if (!EVP_DigestSignInit(md_ctx, NULL, check_md, NULL, pkey))
				goto err_sm2;

			if (EVP_PKEY_CTX_set1_id(sctx, op->sm2.id.data, op->sm2.id.length) <= 0)
				goto err_sm2;

			if (!EVP_DigestSignUpdate(md_ctx, op->sm2.message.data,
					op->sm2.message.length))
				goto err_sm2;

			if (!EVP_DigestSignFinal(md_ctx, NULL, &signlen))
				goto err_sm2;

			if (!EVP_DigestSignFinal(md_ctx, signbuf, &signlen))
				goto err_sm2;

			signptr = signbuf;
			ec_sign = d2i_ECDSA_SIG(NULL, &signptr, signlen);
			if (!ec_sign)
				goto err_sm2;

			r = ECDSA_SIG_get0_r(ec_sign);
			s = ECDSA_SIG_get0_s(ec_sign);
			if (!r || !s)
				goto err_sm2;

			op->sm2.r.length = BN_num_bytes(r);
			op->sm2.s.length = BN_num_bytes(s);
			BN_bn2bin(r, op->sm2.r.data);
			BN_bn2bin(s, op->sm2.s.data);

			ECDSA_SIG_free(ec_sign);
		}
		break;
	case RTE_CRYPTO_ASYM_OP_VERIFY:
		{
			unsigned char signbuf[128] = {0};
			BIGNUM *r = NULL, *s = NULL;
			EVP_MD_CTX *md_ctx = NULL;
			ECDSA_SIG *ec_sign;
			EVP_MD *check_md;
			size_t signlen;

			kctx = EVP_PKEY_CTX_new_from_name(NULL, "SM2", NULL);
			if (kctx == NULL || EVP_PKEY_fromdata_init(kctx) <= 0 ||
				EVP_PKEY_fromdata(kctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) <= 0)
				goto err_sm2;

			if (!EVP_PKEY_is_a(pkey, "SM2"))
				goto err_sm2;

			md_ctx = EVP_MD_CTX_new();
			if (!md_ctx)
				goto err_sm2;

			sctx = EVP_PKEY_CTX_new_from_pkey(NULL, pkey, NULL);
			if (!sctx)
				goto err_sm2;

			EVP_MD_CTX_set_pkey_ctx(md_ctx, sctx);

			check_md = EVP_MD_fetch(NULL, "sm3", NULL);
			if (!check_md)
				goto err_sm2;

			if (!EVP_DigestVerifyInit(md_ctx, NULL, check_md, NULL, pkey))
				goto err_sm2;

			if (EVP_PKEY_CTX_set1_id(sctx, op->sm2.id.data, op->sm2.id.length) <= 0)
				goto err_sm2;

			if (!EVP_DigestVerifyUpdate(md_ctx, op->sm2.message.data,
					op->sm2.message.length))
				goto err_sm2;

			ec_sign = ECDSA_SIG_new();
			if (!ec_sign)
				goto err_sm2;

			r = BN_bin2bn(op->sm2.r.data, op->sm2.r.length, r);
			s = BN_bin2bn(op->sm2.s.data, op->sm2.s.length, s);
			if (!r || !s)
				goto err_sm2;

			if (!ECDSA_SIG_set0(ec_sign, r, s)) {
				BN_free(r);
				BN_free(s);
				goto err_sm2;
			}

			r = NULL;
			s = NULL;

			signlen = i2d_ECDSA_SIG(ec_sign, (unsigned char **)&signbuf);
			if (signlen <= 0)
				goto err_sm2;

			if (!EVP_DigestVerifyFinal(md_ctx, signbuf, signlen))
				goto err_sm2;

			BN_free(r);
			BN_free(s);
			ECDSA_SIG_free(ec_sign);
	}
		break;
	default:
		/* allow ops with invalid args to be pushed to
		 * completion queue
		 */
		cop->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		goto err_sm2;
	}

	ret = 0;
	cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
err_sm2:
	if (kctx)
		EVP_PKEY_CTX_free(kctx);

	if (sctx)
		EVP_PKEY_CTX_free(sctx);

	if (cctx)
		EVP_PKEY_CTX_free(cctx);

	if (pkey)
		EVP_PKEY_free(pkey);

	if (param_bld)
		OSSL_PARAM_BLD_free(param_bld);

	return ret;
}

#else
static int
process_openssl_rsa_op(struct rte_crypto_op *cop,
		struct openssl_asym_session *sess)
{
	int ret = 0;
	struct rte_crypto_asym_op *op = cop->asym;
	RSA *rsa = sess->u.r.rsa;
	uint32_t pad = (op->rsa.padding.type);
	uint8_t *tmp;

	cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;

	switch (pad) {
	case RTE_CRYPTO_RSA_PADDING_PKCS1_5:
		pad = RSA_PKCS1_PADDING;
		break;
	case RTE_CRYPTO_RSA_PADDING_NONE:
		pad = RSA_NO_PADDING;
		break;
	default:
		cop->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		OPENSSL_LOG(ERR,
				"rsa pad type not supported %d\n", pad);
		return 0;
	}

	switch (op->rsa.op_type) {
	case RTE_CRYPTO_ASYM_OP_ENCRYPT:
		ret = RSA_public_encrypt(op->rsa.message.length,
				op->rsa.message.data,
				op->rsa.cipher.data,
				rsa,
				pad);

		if (ret > 0)
			op->rsa.cipher.length = ret;
		OPENSSL_LOG(DEBUG,
				"length of encrypted text %d\n", ret);
		break;

	case RTE_CRYPTO_ASYM_OP_DECRYPT:
		ret = RSA_private_decrypt(op->rsa.cipher.length,
				op->rsa.cipher.data,
				op->rsa.message.data,
				rsa,
				pad);
		if (ret > 0)
			op->rsa.message.length = ret;
		break;

	case RTE_CRYPTO_ASYM_OP_SIGN:
		ret = RSA_private_encrypt(op->rsa.message.length,
				op->rsa.message.data,
				op->rsa.sign.data,
				rsa,
				pad);
		if (ret > 0)
			op->rsa.sign.length = ret;
		break;

	case RTE_CRYPTO_ASYM_OP_VERIFY:
		tmp = rte_malloc(NULL, op->rsa.sign.length, 0);
		if (tmp == NULL) {
			OPENSSL_LOG(ERR, "Memory allocation failed");
			cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
			break;
		}
		ret = RSA_public_decrypt(op->rsa.sign.length,
				op->rsa.sign.data,
				tmp,
				rsa,
				pad);

		OPENSSL_LOG(DEBUG,
				"Length of public_decrypt %d "
				"length of message %zd\n",
				ret, op->rsa.message.length);
		if ((ret <= 0) || (CRYPTO_memcmp(tmp, op->rsa.message.data,
				op->rsa.message.length))) {
			OPENSSL_LOG(ERR, "RSA sign Verification failed");
			cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
		}
		rte_free(tmp);
		break;

	default:
		/* allow ops with invalid args to be pushed to
		 * completion queue
		 */
		cop->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		break;
	}

	if (ret < 0)
		cop->status = RTE_CRYPTO_OP_STATUS_ERROR;

	return 0;
}

static int
process_openssl_sm2_op(struct rte_crypto_op *cop,
		struct openssl_asym_session *sess)
{
	RTE_SET_USED(cop);
	RTE_SET_USED(sess);
	return -ENOTSUP;
}
#endif

static int
process_asym_op(struct openssl_qp *qp, struct rte_crypto_op *op,
		struct openssl_asym_session *sess)
{
	int retval = 0;

	op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;

	switch (sess->xfrm_type) {
	case RTE_CRYPTO_ASYM_XFORM_RSA:
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
		retval = process_openssl_rsa_op_evp(op, sess);
# else
		retval = process_openssl_rsa_op(op, sess);
#endif
		break;
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		retval = process_openssl_modexp_op(op, sess);
		break;
	case RTE_CRYPTO_ASYM_XFORM_MODINV:
		retval = process_openssl_modinv_op(op, sess);
		break;
	case RTE_CRYPTO_ASYM_XFORM_DH:
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
		retval = process_openssl_dh_op_evp(op, sess);
# else
		retval = process_openssl_dh_op(op, sess);
#endif
		break;
	case RTE_CRYPTO_ASYM_XFORM_DSA:
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
		if (op->asym->dsa.op_type == RTE_CRYPTO_ASYM_OP_SIGN)
			retval = process_openssl_dsa_sign_op_evp(op, sess);
		else if (op->asym->dsa.op_type ==
				RTE_CRYPTO_ASYM_OP_VERIFY)
			retval =
				process_openssl_dsa_verify_op_evp(op, sess);
#else
		if (op->asym->dsa.op_type == RTE_CRYPTO_ASYM_OP_SIGN)
			retval = process_openssl_dsa_sign_op(op, sess);
		else if (op->asym->dsa.op_type ==
				RTE_CRYPTO_ASYM_OP_VERIFY)
			retval =
				process_openssl_dsa_verify_op(op, sess);
		else
			op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
#endif
		break;
	case RTE_CRYPTO_ASYM_XFORM_SM2:
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
		retval = process_openssl_sm2_op_evp(op, sess);
#else
		retval = process_openssl_sm2_op(op, sess);
#endif
		break;
	default:
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		break;
	}
	if (!retval) {
		/* op processed so push to completion queue as processed */
		retval = rte_ring_enqueue(qp->processed_ops, (void *)op);
		if (retval)
			/* return error if failed to put in completion queue */
			retval = -1;
	}

	return retval;
}

static void
copy_plaintext(struct rte_mbuf *m_src, struct rte_mbuf *m_dst,
		struct rte_crypto_op *op)
{
	uint8_t *p_src, *p_dst;

	p_src = rte_pktmbuf_mtod(m_src, uint8_t *);
	p_dst = rte_pktmbuf_mtod(m_dst, uint8_t *);

	/**
	 * Copy the content between cipher offset and auth offset
	 * for generating correct digest.
	 */
	if (op->sym->cipher.data.offset > op->sym->auth.data.offset)
		memcpy(p_dst + op->sym->auth.data.offset,
				p_src + op->sym->auth.data.offset,
				op->sym->cipher.data.offset -
				op->sym->auth.data.offset);
}

/** Process crypto operation for mbuf */
static int
process_op(struct openssl_qp *qp, struct rte_crypto_op *op,
		struct openssl_session *sess)
{
	struct rte_mbuf *msrc, *mdst;
	int retval;

	msrc = op->sym->m_src;
	mdst = op->sym->m_dst ? op->sym->m_dst : op->sym->m_src;

	op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;

	switch (sess->chain_order) {
	case OPENSSL_CHAIN_ONLY_CIPHER:
		process_openssl_cipher_op(op, sess, msrc, mdst);
		break;
	case OPENSSL_CHAIN_ONLY_AUTH:
		process_openssl_auth_op(qp, op, sess, msrc, mdst);
		break;
	case OPENSSL_CHAIN_CIPHER_AUTH:
		process_openssl_cipher_op(op, sess, msrc, mdst);
		/* OOP */
		if (msrc != mdst)
			copy_plaintext(msrc, mdst, op);
		process_openssl_auth_op(qp, op, sess, mdst, mdst);
		break;
	case OPENSSL_CHAIN_AUTH_CIPHER:
		process_openssl_auth_op(qp, op, sess, msrc, mdst);
		process_openssl_cipher_op(op, sess, msrc, mdst);
		break;
	case OPENSSL_CHAIN_COMBINED:
		process_openssl_combined_op(op, sess, msrc, mdst);
		break;
	case OPENSSL_CHAIN_CIPHER_BPI:
		process_openssl_docsis_bpi_op(op, sess, msrc, mdst);
		break;
	default:
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		break;
	}

	/* Free session if a session-less crypto op */
	if (op->sess_type == RTE_CRYPTO_OP_SESSIONLESS) {
		openssl_reset_session(sess);
		memset(sess, 0, sizeof(struct openssl_session));
		rte_mempool_put(qp->sess_mp, op->sym->session);
		op->sym->session = NULL;
	}

	if (op->status == RTE_CRYPTO_OP_STATUS_NOT_PROCESSED)
		op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;

	if (op->status != RTE_CRYPTO_OP_STATUS_ERROR)
		retval = rte_ring_enqueue(qp->processed_ops, (void *)op);
	else
		retval = -1;

	return retval;
}

/*
 *------------------------------------------------------------------------------
 * PMD Framework
 *------------------------------------------------------------------------------
 */

/** Enqueue burst */
static uint16_t
openssl_pmd_enqueue_burst(void *queue_pair, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	void *sess;
	struct openssl_qp *qp = queue_pair;
	int i, retval;

	for (i = 0; i < nb_ops; i++) {
		sess = get_session(qp, ops[i]);
		if (unlikely(sess == NULL))
			goto enqueue_err;

		if (ops[i]->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC)
			retval = process_op(qp, ops[i],
					(struct openssl_session *) sess);
		else
			retval = process_asym_op(qp, ops[i],
					(struct openssl_asym_session *) sess);
		if (unlikely(retval < 0))
			goto enqueue_err;
	}

	qp->stats.enqueued_count += i;
	return i;

enqueue_err:
	qp->stats.enqueue_err_count++;
	return i;
}

/** Dequeue burst */
static uint16_t
openssl_pmd_dequeue_burst(void *queue_pair, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	struct openssl_qp *qp = queue_pair;

	unsigned int nb_dequeued = 0;

	nb_dequeued = rte_ring_dequeue_burst(qp->processed_ops,
			(void **)ops, nb_ops, NULL);
	qp->stats.dequeued_count += nb_dequeued;

	return nb_dequeued;
}

/** Create OPENSSL crypto device */
static int
cryptodev_openssl_create(const char *name,
			struct rte_vdev_device *vdev,
			struct rte_cryptodev_pmd_init_params *init_params)
{
	struct rte_cryptodev *dev;
	struct openssl_private *internals;

	dev = rte_cryptodev_pmd_create(name, &vdev->device, init_params);
	if (dev == NULL) {
		OPENSSL_LOG(ERR, "failed to create cryptodev vdev");
		goto init_error;
	}

	dev->driver_id = cryptodev_driver_id;
	dev->dev_ops = rte_openssl_pmd_ops;

	/* register rx/tx burst functions for data path */
	dev->dequeue_burst = openssl_pmd_dequeue_burst;
	dev->enqueue_burst = openssl_pmd_enqueue_burst;

	dev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			RTE_CRYPTODEV_FF_CPU_AESNI |
			RTE_CRYPTODEV_FF_IN_PLACE_SGL |
			RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT |
			RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT |
			RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_RSA_PRIV_OP_KEY_EXP |
			RTE_CRYPTODEV_FF_RSA_PRIV_OP_KEY_QT |
			RTE_CRYPTODEV_FF_SYM_SESSIONLESS;

	internals = dev->data->dev_private;

	internals->max_nb_qpairs = init_params->max_nb_queue_pairs;

	rte_cryptodev_pmd_probing_finish(dev);

# if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
	/* Load legacy provider
	 * Some algorithms are no longer available in earlier version of openssl,
	 * unless the legacy provider explicitly loaded. e.g. DES
	 */
	ossl_legacy_provider_load();
# endif
	return 0;

init_error:
	OPENSSL_LOG(ERR, "driver %s: create failed",
			init_params->name);

	cryptodev_openssl_remove(vdev);
	return -EFAULT;
}

/** Initialise OPENSSL crypto device */
static int
cryptodev_openssl_probe(struct rte_vdev_device *vdev)
{
	struct rte_cryptodev_pmd_init_params init_params = {
		"",
		sizeof(struct openssl_private),
		rte_socket_id(),
		RTE_CRYPTODEV_PMD_DEFAULT_MAX_NB_QUEUE_PAIRS
	};
	const char *name;
	const char *input_args;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;
	input_args = rte_vdev_device_args(vdev);

	rte_cryptodev_pmd_parse_input_args(&init_params, input_args);

	return cryptodev_openssl_create(name, vdev, &init_params);
}

/** Uninitialise OPENSSL crypto device */
static int
cryptodev_openssl_remove(struct rte_vdev_device *vdev)
{
	struct rte_cryptodev *cryptodev;
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	cryptodev = rte_cryptodev_pmd_get_named_dev(name);
	if (cryptodev == NULL)
		return -ENODEV;

# if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
	ossl_legacy_provider_unload();
# endif
	return rte_cryptodev_pmd_destroy(cryptodev);
}

static struct rte_vdev_driver cryptodev_openssl_pmd_drv = {
	.probe = cryptodev_openssl_probe,
	.remove = cryptodev_openssl_remove
};

static struct cryptodev_driver openssl_crypto_drv;

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_OPENSSL_PMD,
	cryptodev_openssl_pmd_drv);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_OPENSSL_PMD,
	"max_nb_queue_pairs=<int> "
	"socket_id=<int>");
RTE_PMD_REGISTER_CRYPTO_DRIVER(openssl_crypto_drv,
		cryptodev_openssl_pmd_drv.driver, cryptodev_driver_id);
RTE_LOG_REGISTER_DEFAULT(openssl_logtype_driver, INFO);
