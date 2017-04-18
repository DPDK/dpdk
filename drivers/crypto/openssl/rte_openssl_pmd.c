/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_vdev.h>
#include <rte_malloc.h>
#include <rte_cpuflags.h>

#include <openssl/evp.h>

#include "rte_openssl_pmd_private.h"

#define DES_BLOCK_SIZE 8

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
	}

	return res;
}

/** Get session cipher key from input cipher key */
static void
get_cipher_key(uint8_t *input_key, int keylen, uint8_t *session_key)
{
	memcpy(session_key, input_key, keylen);
}

/** Get key ede 24 bytes standard from input key */
static int
get_cipher_key_ede(uint8_t *key, int keylen, uint8_t *key_ede)
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
		OPENSSL_LOG_ERR("Unsupported key size");
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
		case RTE_CRYPTO_CIPHER_AES_GCM:
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

/** Set session cipher parameters */
static int
openssl_set_session_cipher_parameters(struct openssl_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	/* Select cipher direction */
	sess->cipher.direction = xform->cipher.op;
	/* Select cipher key */
	sess->cipher.key.length = xform->cipher.key.length;

	/* Select cipher algo */
	switch (xform->cipher.algo) {
	case RTE_CRYPTO_CIPHER_3DES_CBC:
	case RTE_CRYPTO_CIPHER_AES_CBC:
	case RTE_CRYPTO_CIPHER_AES_CTR:
	case RTE_CRYPTO_CIPHER_AES_GCM:
		sess->cipher.mode = OPENSSL_CIPHER_LIB;
		sess->cipher.algo = xform->cipher.algo;
		sess->cipher.ctx = EVP_CIPHER_CTX_new();

		if (get_cipher_algo(sess->cipher.algo, sess->cipher.key.length,
				&sess->cipher.evp_algo) != 0)
			return -EINVAL;

		get_cipher_key(xform->cipher.key.data, sess->cipher.key.length,
			sess->cipher.key.data);

		break;

	case RTE_CRYPTO_CIPHER_3DES_CTR:
		sess->cipher.mode = OPENSSL_CIPHER_DES3CTR;
		sess->cipher.ctx = EVP_CIPHER_CTX_new();

		if (get_cipher_key_ede(xform->cipher.key.data,
				sess->cipher.key.length,
				sess->cipher.key.data) != 0)
			return -EINVAL;
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
		break;
	default:
		sess->cipher.algo = RTE_CRYPTO_CIPHER_NULL;
		return -EINVAL;
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

	/* Select auth algo */
	switch (xform->auth.algo) {
	case RTE_CRYPTO_AUTH_AES_GMAC:
	case RTE_CRYPTO_AUTH_AES_GCM:
		/* Check additional condition for AES_GMAC/GCM */
		if (sess->cipher.algo != RTE_CRYPTO_CIPHER_AES_GCM)
			return -EINVAL;
		sess->chain_order = OPENSSL_CHAIN_COMBINED;
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
		sess->auth.hmac.ctx = EVP_MD_CTX_create();
		if (get_auth_algo(xform->auth.algo,
				&sess->auth.hmac.evp_algo) != 0)
			return -EINVAL;
		sess->auth.hmac.pkey = EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, NULL,
				xform->auth.key.data, xform->auth.key.length);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

/** Parse crypto xform chain and set private session parameters */
int
openssl_set_session_parameters(struct openssl_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	const struct rte_crypto_sym_xform *cipher_xform = NULL;
	const struct rte_crypto_sym_xform *auth_xform = NULL;

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
	default:
		return -EINVAL;
	}

	/* cipher_xform must be check before auth_xform */
	if (cipher_xform) {
		if (openssl_set_session_cipher_parameters(
				sess, cipher_xform)) {
			OPENSSL_LOG_ERR(
				"Invalid/unsupported cipher parameters");
			return -EINVAL;
		}
	}

	if (auth_xform) {
		if (openssl_set_session_auth_parameters(sess, auth_xform)) {
			OPENSSL_LOG_ERR(
				"Invalid/unsupported auth parameters");
			return -EINVAL;
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
		EVP_MD_CTX_destroy(sess->auth.hmac.ctx);
		break;
	default:
		break;
	}
}

/** Provide session for operation */
static struct openssl_session *
get_session(struct openssl_qp *qp, struct rte_crypto_op *op)
{
	struct openssl_session *sess = NULL;

	if (op->sym->sess_type == RTE_CRYPTO_SYM_OP_WITH_SESSION) {
		/* get existing session */
		if (likely(op->sym->session != NULL &&
				op->sym->session->dev_type ==
				RTE_CRYPTODEV_OPENSSL_PMD))
			sess = (struct openssl_session *)
				op->sym->session->_private;
	} else  {
		/* provide internal session */
		void *_sess = NULL;

		if (!rte_mempool_get(qp->sess_mp, (void **)&_sess)) {
			sess = (struct openssl_session *)
				((struct rte_cryptodev_sym_session *)_sess)
				->_private;

			if (unlikely(openssl_set_session_parameters(
					sess, op->sym->xform) != 0)) {
				rte_mempool_put(qp->sess_mp, _sess);
				sess = NULL;
			} else
				op->sym->session = _sess;
		}
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
		uint8_t **dst, int srclen, EVP_CIPHER_CTX *ctx)
{
	struct rte_mbuf *m;
	int dstlen;
	int l, n = srclen;
	uint8_t *src;

	for (m = mbuf_src; m != NULL && offset > rte_pktmbuf_data_len(m);
			m = m->next)
		offset -= rte_pktmbuf_data_len(m);

	if (m == 0)
		return -1;

	src = rte_pktmbuf_mtod_offset(m, uint8_t *, offset);

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
		src = rte_pktmbuf_mtod(m, uint8_t *);
		l = rte_pktmbuf_data_len(m) < n ? rte_pktmbuf_data_len(m) : n;
		if (EVP_EncryptUpdate(ctx, *dst, &dstlen, src, l) <= 0)
			return -1;
		*dst += dstlen;
		n -= l;
	}

	return 0;
}

static inline int
process_openssl_decryption_update(struct rte_mbuf *mbuf_src, int offset,
		uint8_t **dst, int srclen, EVP_CIPHER_CTX *ctx)
{
	struct rte_mbuf *m;
	int dstlen;
	int l, n = srclen;
	uint8_t *src;

	for (m = mbuf_src; m != NULL && offset > rte_pktmbuf_data_len(m);
			m = m->next)
		offset -= rte_pktmbuf_data_len(m);

	if (m == 0)
		return -1;

	src = rte_pktmbuf_mtod_offset(m, uint8_t *, offset);

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
		src = rte_pktmbuf_mtod(m, uint8_t *);
		l = rte_pktmbuf_data_len(m) < n ? rte_pktmbuf_data_len(m) : n;
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
		int offset, uint8_t *iv, uint8_t *key, int srclen,
		EVP_CIPHER_CTX *ctx, const EVP_CIPHER *algo)
{
	int totlen;

	if (EVP_EncryptInit_ex(ctx, algo, NULL, key, iv) <= 0)
		goto process_cipher_encrypt_err;

	EVP_CIPHER_CTX_set_padding(ctx, 0);

	if (process_openssl_encryption_update(mbuf_src, offset, &dst,
			srclen, ctx))
		goto process_cipher_encrypt_err;

	if (EVP_EncryptFinal_ex(ctx, dst, &totlen) <= 0)
		goto process_cipher_encrypt_err;

	return 0;

process_cipher_encrypt_err:
	OPENSSL_LOG_ERR("Process openssl cipher encrypt failed");
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
	OPENSSL_LOG_ERR("Process openssl cipher bpi encrypt failed");
	return -EINVAL;
}
/** Process standard openssl cipher decryption */
static int
process_openssl_cipher_decrypt(struct rte_mbuf *mbuf_src, uint8_t *dst,
		int offset, uint8_t *iv, uint8_t *key, int srclen,
		EVP_CIPHER_CTX *ctx, const EVP_CIPHER *algo)
{
	int totlen;

	if (EVP_DecryptInit_ex(ctx, algo, NULL, key, iv) <= 0)
		goto process_cipher_decrypt_err;

	EVP_CIPHER_CTX_set_padding(ctx, 0);

	if (process_openssl_decryption_update(mbuf_src, offset, &dst,
			srclen, ctx))
		goto process_cipher_decrypt_err;

	if (EVP_DecryptFinal_ex(ctx, dst, &totlen) <= 0)
		goto process_cipher_decrypt_err;
	return 0;

process_cipher_decrypt_err:
	OPENSSL_LOG_ERR("Process openssl cipher decrypt failed");
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
	OPENSSL_LOG_ERR("Process openssl cipher des 3 ede ctr failed");
	return -EINVAL;
}

/** Process auth/encription aes-gcm algorithm */
static int
process_openssl_auth_encryption_gcm(struct rte_mbuf *mbuf_src, int offset,
		int srclen, uint8_t *aad, int aadlen, uint8_t *iv, int ivlen,
		uint8_t *key, uint8_t *dst, uint8_t *tag,
		EVP_CIPHER_CTX *ctx, const EVP_CIPHER *algo)
{
	int len = 0, unused = 0;
	uint8_t empty[] = {};

	if (EVP_EncryptInit_ex(ctx, algo, NULL, NULL, NULL) <= 0)
		goto process_auth_encryption_gcm_err;

	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, ivlen, NULL) <= 0)
		goto process_auth_encryption_gcm_err;

	if (EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) <= 0)
		goto process_auth_encryption_gcm_err;

	if (aadlen > 0)
		if (EVP_EncryptUpdate(ctx, NULL, &len, aad, aadlen) <= 0)
			goto process_auth_encryption_gcm_err;

	if (srclen > 0)
		if (process_openssl_encryption_update(mbuf_src, offset, &dst,
				srclen, ctx))
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
	OPENSSL_LOG_ERR("Process openssl auth encryption gcm failed");
	return -EINVAL;
}

static int
process_openssl_auth_decryption_gcm(struct rte_mbuf *mbuf_src, int offset,
		int srclen, uint8_t *aad, int aadlen, uint8_t *iv, int ivlen,
		uint8_t *key, uint8_t *dst, uint8_t *tag, EVP_CIPHER_CTX *ctx,
		const EVP_CIPHER *algo)
{
	int len = 0, unused = 0;
	uint8_t empty[] = {};

	if (EVP_DecryptInit_ex(ctx, algo, NULL, NULL, NULL) <= 0)
		goto process_auth_decryption_gcm_err;

	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, ivlen, NULL) <= 0)
		goto process_auth_decryption_gcm_err;

	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag) <= 0)
		goto process_auth_decryption_gcm_err;

	if (EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) <= 0)
		goto process_auth_decryption_gcm_err;

	if (aadlen > 0)
		if (EVP_DecryptUpdate(ctx, NULL, &len, aad, aadlen) <= 0)
			goto process_auth_decryption_gcm_err;

	if (srclen > 0)
		if (process_openssl_decryption_update(mbuf_src, offset, &dst,
				srclen, ctx))
			goto process_auth_decryption_gcm_err;

	/* Workaround open ssl bug in version less then 1.0.1f */
	if (EVP_DecryptUpdate(ctx, empty, &unused, empty, 0) <= 0)
		goto process_auth_decryption_gcm_err;

	if (EVP_DecryptFinal_ex(ctx, dst, &len) <= 0)
		goto process_auth_decryption_gcm_final_err;

	return 0;

process_auth_decryption_gcm_err:
	OPENSSL_LOG_ERR("Process openssl auth description gcm failed");
	return -EINVAL;

process_auth_decryption_gcm_final_err:
	return -EFAULT;
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
	OPENSSL_LOG_ERR("Process openssl auth failed");
	return -EINVAL;
}

/** Process standard openssl auth algorithms with hmac */
static int
process_openssl_auth_hmac(struct rte_mbuf *mbuf_src, uint8_t *dst, int offset,
		__rte_unused uint8_t *iv, EVP_PKEY *pkey,
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

	if (EVP_DigestSignInit(ctx, NULL, algo, NULL, pkey) <= 0)
		goto process_auth_err;

	src = rte_pktmbuf_mtod_offset(m, uint8_t *, offset);

	l = rte_pktmbuf_data_len(m) - offset;
	if (srclen <= l) {
		if (EVP_DigestSignUpdate(ctx, (char *)src, srclen) <= 0)
			goto process_auth_err;
		goto process_auth_final;
	}

	if (EVP_DigestSignUpdate(ctx, (char *)src, l) <= 0)
		goto process_auth_err;

	n -= l;

	for (m = m->next; (m != NULL) && (n > 0); m = m->next) {
		src = rte_pktmbuf_mtod(m, uint8_t *);
		l = rte_pktmbuf_data_len(m) < n ? rte_pktmbuf_data_len(m) : n;
		if (EVP_DigestSignUpdate(ctx, (char *)src, l) <= 0)
			goto process_auth_err;
		n -= l;
	}

process_auth_final:
	if (EVP_DigestSignFinal(ctx, dst, &dstlen) <= 0)
		goto process_auth_err;

	return 0;

process_auth_err:
	OPENSSL_LOG_ERR("Process openssl auth failed");
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
	int srclen, ivlen, aadlen, status = -1;

	/*
	 * Segmented destination buffer is not supported for
	 * encryption/decryption
	 */
	if (!rte_pktmbuf_is_contiguous(mbuf_dst)) {
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		return;
	}

	iv = op->sym->cipher.iv.data;
	ivlen = op->sym->cipher.iv.length;
	aad = op->sym->auth.aad.data;
	aadlen = op->sym->auth.aad.length;

	tag = op->sym->auth.digest.data;
	if (tag == NULL)
		tag = rte_pktmbuf_mtod_offset(mbuf_dst, uint8_t *,
				op->sym->cipher.data.offset +
				op->sym->cipher.data.length);

	if (sess->auth.algo == RTE_CRYPTO_AUTH_AES_GMAC)
		srclen = 0;
	else {
		srclen = op->sym->cipher.data.length;
		dst = rte_pktmbuf_mtod_offset(mbuf_dst, uint8_t *,
				op->sym->cipher.data.offset);
	}

	if (sess->cipher.direction == RTE_CRYPTO_CIPHER_OP_ENCRYPT)
		status = process_openssl_auth_encryption_gcm(
				mbuf_src, op->sym->cipher.data.offset, srclen,
				aad, aadlen, iv, ivlen, sess->cipher.key.data,
				dst, tag, sess->cipher.ctx,
				sess->cipher.evp_algo);
	else
		status = process_openssl_auth_decryption_gcm(
				mbuf_src, op->sym->cipher.data.offset, srclen,
				aad, aadlen, iv, ivlen, sess->cipher.key.data,
				dst, tag, sess->cipher.ctx,
				sess->cipher.evp_algo);

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

	/*
	 * Segmented destination buffer is not supported for
	 * encryption/decryption
	 */
	if (!rte_pktmbuf_is_contiguous(mbuf_dst)) {
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		return;
	}

	srclen = op->sym->cipher.data.length;
	dst = rte_pktmbuf_mtod_offset(mbuf_dst, uint8_t *,
			op->sym->cipher.data.offset);

	iv = op->sym->cipher.iv.data;

	if (sess->cipher.mode == OPENSSL_CIPHER_LIB)
		if (sess->cipher.direction == RTE_CRYPTO_CIPHER_OP_ENCRYPT)
			status = process_openssl_cipher_encrypt(mbuf_src, dst,
					op->sym->cipher.data.offset, iv,
					sess->cipher.key.data, srclen,
					sess->cipher.ctx,
					sess->cipher.evp_algo);
		else
			status = process_openssl_cipher_decrypt(mbuf_src, dst,
					op->sym->cipher.data.offset, iv,
					sess->cipher.key.data, srclen,
					sess->cipher.ctx,
					sess->cipher.evp_algo);
	else
		status = process_openssl_cipher_des3ctr(mbuf_src, dst,
				op->sym->cipher.data.offset, iv,
				sess->cipher.key.data, srclen,
				sess->cipher.ctx);

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

	iv = op->sym->cipher.iv.data;

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
					sess->cipher.key.data, srclen,
					sess->cipher.ctx, sess->cipher.evp_algo);
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
				iv = op->sym->cipher.iv.data;
				dst += last_block_len - srclen;
				srclen -= last_block_len;
			}

			/* Decrypt with CBC mode */
			status |= process_openssl_cipher_decrypt(mbuf_src, dst,
					op->sym->cipher.data.offset, iv,
					sess->cipher.key.data, srclen,
					sess->cipher.ctx,
					sess->cipher.evp_algo);
		}
	}

	if (status != 0)
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
}

/** Process auth operation */
static void
process_openssl_auth_op
		(struct rte_crypto_op *op, struct openssl_session *sess,
		struct rte_mbuf *mbuf_src, struct rte_mbuf *mbuf_dst)
{
	uint8_t *dst;
	int srclen, status;

	srclen = op->sym->auth.data.length;

	if (sess->auth.operation == RTE_CRYPTO_AUTH_OP_VERIFY)
		dst = (uint8_t *)rte_pktmbuf_append(mbuf_src,
				op->sym->auth.digest.length);
	else {
		dst = op->sym->auth.digest.data;
		if (dst == NULL)
			dst = rte_pktmbuf_mtod_offset(mbuf_dst, uint8_t *,
					op->sym->auth.data.offset +
					op->sym->auth.data.length);
	}

	switch (sess->auth.mode) {
	case OPENSSL_AUTH_AS_AUTH:
		status = process_openssl_auth(mbuf_src, dst,
				op->sym->auth.data.offset, NULL, NULL, srclen,
				sess->auth.auth.ctx, sess->auth.auth.evp_algo);
		break;
	case OPENSSL_AUTH_AS_HMAC:
		status = process_openssl_auth_hmac(mbuf_src, dst,
				op->sym->auth.data.offset, NULL,
				sess->auth.hmac.pkey, srclen,
				sess->auth.hmac.ctx, sess->auth.hmac.evp_algo);
		break;
	default:
		status = -1;
		break;
	}

	if (sess->auth.operation == RTE_CRYPTO_AUTH_OP_VERIFY) {
		if (memcmp(dst, op->sym->auth.digest.data,
				op->sym->auth.digest.length) != 0) {
			op->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
		}
		/* Trim area used for digest from mbuf. */
		rte_pktmbuf_trim(mbuf_src, op->sym->auth.digest.length);
	}

	if (status != 0)
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
}

/** Process crypto operation for mbuf */
static int
process_op(const struct openssl_qp *qp, struct rte_crypto_op *op,
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
		process_openssl_auth_op(op, sess, msrc, mdst);
		break;
	case OPENSSL_CHAIN_CIPHER_AUTH:
		process_openssl_cipher_op(op, sess, msrc, mdst);
		process_openssl_auth_op(op, sess, mdst, mdst);
		break;
	case OPENSSL_CHAIN_AUTH_CIPHER:
		process_openssl_auth_op(op, sess, msrc, mdst);
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
	if (op->sym->sess_type == RTE_CRYPTO_SYM_OP_SESSIONLESS) {
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
	struct openssl_session *sess;
	struct openssl_qp *qp = queue_pair;
	int i, retval;

	for (i = 0; i < nb_ops; i++) {
		sess = get_session(qp, ops[i]);
		if (unlikely(sess == NULL))
			goto enqueue_err;

		retval = process_op(qp, ops[i], sess);
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
			struct rte_crypto_vdev_init_params *init_params)
{
	struct rte_cryptodev *dev;
	struct openssl_private *internals;

	if (init_params->name[0] == '\0')
		snprintf(init_params->name, sizeof(init_params->name),
				"%s", name);

	dev = rte_cryptodev_pmd_virtual_dev_init(init_params->name,
			sizeof(struct openssl_private),
			init_params->socket_id);
	if (dev == NULL) {
		OPENSSL_LOG_ERR("failed to create cryptodev vdev");
		goto init_error;
	}

	dev->dev_type = RTE_CRYPTODEV_OPENSSL_PMD;
	dev->dev_ops = rte_openssl_pmd_ops;

	/* register rx/tx burst functions for data path */
	dev->dequeue_burst = openssl_pmd_dequeue_burst;
	dev->enqueue_burst = openssl_pmd_enqueue_burst;

	dev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			RTE_CRYPTODEV_FF_CPU_AESNI |
			RTE_CRYPTODEV_FF_MBUF_SCATTER_GATHER;

	/* Set vector instructions mode supported */
	internals = dev->data->dev_private;

	internals->max_nb_qpairs = init_params->max_nb_queue_pairs;
	internals->max_nb_sessions = init_params->max_nb_sessions;

	return 0;

init_error:
	OPENSSL_LOG_ERR("driver %s: cryptodev_openssl_create failed",
			init_params->name);

	cryptodev_openssl_remove(vdev);
	return -EFAULT;
}

/** Initialise OPENSSL crypto device */
static int
cryptodev_openssl_probe(struct rte_vdev_device *vdev)
{
	struct rte_crypto_vdev_init_params init_params = {
		RTE_CRYPTODEV_VDEV_DEFAULT_MAX_NB_QUEUE_PAIRS,
		RTE_CRYPTODEV_VDEV_DEFAULT_MAX_NB_SESSIONS,
		rte_socket_id(),
		{0}
	};
	const char *name;
	const char *input_args;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;
	input_args = rte_vdev_device_args(vdev);

	rte_cryptodev_parse_vdev_init_params(&init_params, input_args);

	RTE_LOG(INFO, PMD, "Initialising %s on NUMA node %d\n", name,
			init_params.socket_id);
	if (init_params.name[0] != '\0')
		RTE_LOG(INFO, PMD, "  User defined name = %s\n",
			init_params.name);
	RTE_LOG(INFO, PMD, "  Max number of queue pairs = %d\n",
			init_params.max_nb_queue_pairs);
	RTE_LOG(INFO, PMD, "  Max number of sessions = %d\n",
			init_params.max_nb_sessions);

	return cryptodev_openssl_create(name, vdev, &init_params);
}

/** Uninitialise OPENSSL crypto device */
static int
cryptodev_openssl_remove(struct rte_vdev_device *vdev)
{
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	RTE_LOG(INFO, PMD,
		"Closing OPENSSL crypto device %s on numa socket %u\n",
		name, rte_socket_id());

	return 0;
}

static struct rte_vdev_driver cryptodev_openssl_pmd_drv = {
	.probe = cryptodev_openssl_probe,
	.remove = cryptodev_openssl_remove
};

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_OPENSSL_PMD,
	cryptodev_openssl_pmd_drv);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_OPENSSL_PMD,
	"max_nb_queue_pairs=<int> "
	"max_nb_sessions=<int> "
	"socket_id=<int>");
