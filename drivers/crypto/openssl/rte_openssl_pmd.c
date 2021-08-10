/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include <rte_bus_vdev.h>
#include <rte_malloc.h>
#include <rte_cpuflags.h>

#include <openssl/hmac.h>
#include <openssl/evp.h>

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
		HMAC_CTX_free(sess->auth.hmac.ctx);
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
				sess = (struct openssl_session *)
						get_sym_session_private_data(
						op->sym->session,
						cryptodev_driver_id);
		} else {
			if (likely(op->asym->session != NULL))
				asym_sess = (struct openssl_asym_session *)
						get_asym_session_private_data(
						op->asym->session,
						cryptodev_driver_id);
			if (asym_sess == NULL)
				op->status =
					RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
			return asym_sess;
		}
	} else {
		/* sessionless asymmetric not supported */
		if (op->type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC)
			return NULL;

		/* provide internal session */
		void *_sess = rte_cryptodev_sym_session_create(qp->sess_mp);
		void *_sess_private_data = NULL;

		if (_sess == NULL)
			return NULL;

		if (rte_mempool_get(qp->sess_mp_priv,
				(void **)&_sess_private_data))
			return NULL;

		sess = (struct openssl_session *)_sess_private_data;

		if (unlikely(openssl_set_session_parameters(sess,
				op->sym->xform) != 0)) {
			rte_mempool_put(qp->sess_mp, _sess);
			rte_mempool_put(qp->sess_mp_priv, _sess_private_data);
			sess = NULL;
		}
		op->sym->session = (struct rte_cryptodev_sym_session *)_sess;
		set_sym_session_private_data(op->sym->session,
				cryptodev_driver_id, _sess_private_data);
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
	int len = 0, unused = 0;
	uint8_t empty[] = {};

	if (EVP_EncryptInit_ex(ctx, NULL, NULL, NULL, iv) <= 0)
		goto process_auth_encryption_gcm_err;

	if (aadlen > 0)
		if (EVP_EncryptUpdate(ctx, NULL, &len, aad, aadlen) <= 0)
			goto process_auth_encryption_gcm_err;

	if (srclen > 0)
		if (process_openssl_encryption_update(mbuf_src, offset, &dst,
				srclen, ctx, 0))
			goto process_auth_encryption_gcm_err;

	/* Workaround open ssl bug in version less then 1.0.1f */
	if (EVP_EncryptUpdate(ctx, empty, &unused, empty, 0) <= 0)
		goto process_auth_encryption_gcm_err;

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
	int len = 0, unused = 0;
	uint8_t empty[] = {};

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

	/* Workaround open ssl bug in version less then 1.0.1f */
	if (EVP_DecryptUpdate(ctx, empty, &unused, empty, 0) <= 0)
		goto process_auth_decryption_gcm_err;

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
	EVP_CIPHER_CTX *ctx_copy;

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
	ctx_copy = EVP_CIPHER_CTX_new();
	EVP_CIPHER_CTX_copy(ctx_copy, sess->cipher.ctx);

	if (sess->cipher.direction == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
		if (sess->auth.algo == RTE_CRYPTO_AUTH_AES_GMAC ||
				sess->aead_algo == RTE_CRYPTO_AEAD_AES_GCM)
			status = process_openssl_auth_encryption_gcm(
					mbuf_src, offset, srclen,
					aad, aadlen, iv,
					dst, tag, ctx_copy);
		else
			status = process_openssl_auth_encryption_ccm(
					mbuf_src, offset, srclen,
					aad, aadlen, iv,
					dst, tag, taglen, ctx_copy);

	} else {
		if (sess->auth.algo == RTE_CRYPTO_AUTH_AES_GMAC ||
				sess->aead_algo == RTE_CRYPTO_AEAD_AES_GCM)
			status = process_openssl_auth_decryption_gcm(
					mbuf_src, offset, srclen,
					aad, aadlen, iv,
					dst, tag, ctx_copy);
		else
			status = process_openssl_auth_decryption_ccm(
					mbuf_src, offset, srclen,
					aad, aadlen, iv,
					dst, tag, taglen, ctx_copy);
	}

	EVP_CIPHER_CTX_free(ctx_copy);
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
	HMAC_CTX *ctx_h;

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
		ctx_h = HMAC_CTX_new();
		HMAC_CTX_copy(ctx_h, sess->auth.hmac.ctx);
		status = process_openssl_auth_hmac(mbuf_src, dst,
				op->sym->auth.data.offset, srclen,
				ctx_h);
		HMAC_CTX_free(ctx_h);
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

/* process dh operation */
static int
process_openssl_dh_op(struct rte_crypto_op *cop,
		struct openssl_asym_session *sess)
{
	struct rte_crypto_dh_op_param *op = &cop->asym->dh;
	DH *dh_key = sess->u.dh.dh_key;
	BIGNUM *priv_key = NULL;
	int ret = 0;

	if (sess->u.dh.key_op &
			(1 << RTE_CRYPTO_ASYM_OP_SHARED_SECRET_COMPUTE)) {
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
	if ((sess->u.dh.key_op &
			(1 << RTE_CRYPTO_ASYM_OP_PUBLIC_KEY_GENERATE)) &&
			!(sess->u.dh.key_op &
			(1 << RTE_CRYPTO_ASYM_OP_PRIVATE_KEY_GENERATE))) {
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

	if (sess->u.dh.key_op & (1 << RTE_CRYPTO_ASYM_OP_PUBLIC_KEY_GENERATE)) {
		const BIGNUM *pub_key = NULL;

		OPENSSL_LOG(DEBUG, "%s:%d update public key\n",
				__func__, __LINE__);

		/* get the generated keys */
		get_dh_pub_key(dh_key, &pub_key);

		/* output public key */
		op->pub_key.length = BN_bn2bin(pub_key,
				op->pub_key.data);
	}

	if (sess->u.dh.key_op &
			(1 << RTE_CRYPTO_ASYM_OP_PRIVATE_KEY_GENERATE)) {
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
static int
process_openssl_rsa_op(struct rte_crypto_op *cop,
		struct openssl_asym_session *sess)
{
	int ret = 0;
	struct rte_crypto_asym_op *op = cop->asym;
	RSA *rsa = sess->u.r.rsa;
	uint32_t pad = (op->rsa.pad);
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
process_asym_op(struct openssl_qp *qp, struct rte_crypto_op *op,
		struct openssl_asym_session *sess)
{
	int retval = 0;

	op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;

	switch (sess->xfrm_type) {
	case RTE_CRYPTO_ASYM_XFORM_RSA:
		retval = process_openssl_rsa_op(op, sess);
		break;
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		retval = process_openssl_modexp_op(op, sess);
		break;
	case RTE_CRYPTO_ASYM_XFORM_MODINV:
		retval = process_openssl_modinv_op(op, sess);
		break;
	case RTE_CRYPTO_ASYM_XFORM_DH:
		retval = process_openssl_dh_op(op, sess);
		break;
	case RTE_CRYPTO_ASYM_XFORM_DSA:
		if (op->asym->dsa.op_type == RTE_CRYPTO_ASYM_OP_SIGN)
			retval = process_openssl_dsa_sign_op(op, sess);
		else if (op->asym->dsa.op_type ==
				RTE_CRYPTO_ASYM_OP_VERIFY)
			retval =
				process_openssl_dsa_verify_op(op, sess);
		else
			op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
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
		memset(op->sym->session, 0,
			rte_cryptodev_sym_get_existing_header_session_size(
				op->sym->session));
		rte_mempool_put(qp->sess_mp_priv, sess);
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
