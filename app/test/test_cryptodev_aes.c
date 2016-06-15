/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015-2016 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *	 * Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	 * Redistributions in binary form must reproduce the above copyright
 *	   notice, this list of conditions and the following disclaimer in
 *	   the documentation and/or other materials provided with the
 *	   distribution.
 *	 * Neither the name of Intel Corporation nor the names of its
 *	   contributors may be used to endorse or promote products derived
 *	   from this software without specific prior written permission.
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
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>

#include <rte_crypto.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>

#include "test.h"
#include "test_cryptodev_aes.h"

#ifndef AES_TEST_MSG_LEN
#define AES_TEST_MSG_LEN		256
#endif

#define AES_TEST_OP_ENCRYPT		0x01
#define AES_TEST_OP_DECRYPT		0x02
#define AES_TEST_OP_AUTH_GEN		0x04
#define AES_TEST_OP_AUTH_VERIFY		0x08

#define AES_TEST_FEATURE_OOP		0x01
#define AES_TEST_FEATURE_SESSIONLESS	0x02
#define AES_TEST_FEATURE_STOPPER	0x04 /* stop upon failing */

#define AES_TEST_TARGET_PMD_MB		0x0001 /* Multi-buffer flag */
#define AES_TEST_TARGET_PMD_QAT		0x0002 /* QAT flag */

#define AES_TEST_OP_CIPHER		(AES_TEST_OP_ENCRYPT |		\
					AES_TEST_OP_DECRYPT)

#define AES_TEST_OP_AUTH		(AES_TEST_OP_AUTH_GEN |		\
					AES_TEST_OP_AUTH_VERIFY)

#define AES_TEST_OP_ENC_AUTH_GEN	(AES_TEST_OP_ENCRYPT |		\
					AES_TEST_OP_AUTH_GEN)

#define AES_TEST_OP_AUTH_VERIFY_DEC	(AES_TEST_OP_DECRYPT |		\
					AES_TEST_OP_AUTH_VERIFY)

struct aes_test_case {
	const char *test_descr; /* test description */
	const struct aes_test_data *test_data;
	uint8_t op_mask; /* operation mask */
	uint8_t feature_mask;
	uint32_t pmd_mask;
};

static const struct aes_test_case aes_test_cases[] = {
	{
		.test_descr = "AES-128-CTR HMAC-SHA1 Encryption Digest",
		.test_data = &aes_test_data_1,
		.op_mask = AES_TEST_OP_ENC_AUTH_GEN,
		.pmd_mask = AES_TEST_TARGET_PMD_MB |
			AES_TEST_TARGET_PMD_QAT
	},
	{
		.test_descr = "AES-128-CTR HMAC-SHA1 Decryption Digest "
			"Verify",
		.test_data = &aes_test_data_1,
		.op_mask = AES_TEST_OP_AUTH_VERIFY_DEC,
		.pmd_mask = AES_TEST_TARGET_PMD_MB |
			AES_TEST_TARGET_PMD_QAT
	},
	{
		.test_descr = "AES-192-CTR XCBC Encryption Digest",
		.test_data = &aes_test_data_2,
		.op_mask = AES_TEST_OP_ENC_AUTH_GEN,
		.pmd_mask = AES_TEST_TARGET_PMD_MB |
			AES_TEST_TARGET_PMD_QAT
	},
	{
		.test_descr = "AES-192-CTR XCBC Decryption Digest Verify",
		.test_data = &aes_test_data_2,
		.op_mask = AES_TEST_OP_AUTH_VERIFY_DEC,
		.pmd_mask = AES_TEST_TARGET_PMD_MB |
			AES_TEST_TARGET_PMD_QAT
	},
	{
		.test_descr = "AES-256-CTR HMAC-SHA1 Encryption Digest",
		.test_data = &aes_test_data_3,
		.op_mask = AES_TEST_OP_ENC_AUTH_GEN,
		.pmd_mask = AES_TEST_TARGET_PMD_MB |
			AES_TEST_TARGET_PMD_QAT
	},
	{
		.test_descr = "AES-256-CTR HMAC-SHA1 Decryption Digest "
			"Verify",
		.test_data = &aes_test_data_3,
		.op_mask = AES_TEST_OP_AUTH_VERIFY_DEC,
		.pmd_mask = AES_TEST_TARGET_PMD_MB |
			AES_TEST_TARGET_PMD_QAT
	},
	{
		.test_descr = "AES-128-CBC HMAC-SHA1 Encryption Digest",
		.test_data = &aes_test_data_4,
		.op_mask = AES_TEST_OP_ENC_AUTH_GEN,
		.pmd_mask = AES_TEST_TARGET_PMD_MB |
			AES_TEST_TARGET_PMD_QAT
	},
	{
		.test_descr = "AES-128-CBC HMAC-SHA1 Decryption Digest "
			"Verify",
		.test_data = &aes_test_data_4,
		.op_mask = AES_TEST_OP_AUTH_VERIFY_DEC,
		.pmd_mask = AES_TEST_TARGET_PMD_MB |
			AES_TEST_TARGET_PMD_QAT
	},
	{
		.test_descr = "AES-128-CBC HMAC-SHA256 Encryption Digest",
		.test_data = &aes_test_data_5,
		.op_mask = AES_TEST_OP_ENC_AUTH_GEN,
		.pmd_mask = AES_TEST_TARGET_PMD_MB |
			AES_TEST_TARGET_PMD_QAT
	},
	{
		.test_descr = "AES-128-CBC HMAC-SHA256 Decryption Digest "
			"Verify",
		.test_data = &aes_test_data_5,
		.op_mask = AES_TEST_OP_AUTH_VERIFY_DEC,
		.pmd_mask = AES_TEST_TARGET_PMD_MB |
			AES_TEST_TARGET_PMD_QAT
	},
	{
		.test_descr = "AES-128-CBC HMAC-SHA512 Encryption Digest",
		.test_data = &aes_test_data_6,
		.op_mask = AES_TEST_OP_ENC_AUTH_GEN,
		.pmd_mask = AES_TEST_TARGET_PMD_MB |
			AES_TEST_TARGET_PMD_QAT
	},
	{
		.test_descr = "AES-128-CBC HMAC-SHA512 Encryption Digest "
			"Sessionless",
		.test_data = &aes_test_data_6,
		.op_mask = AES_TEST_OP_ENC_AUTH_GEN,
		.feature_mask = AES_TEST_FEATURE_SESSIONLESS,
		.pmd_mask = AES_TEST_TARGET_PMD_MB
	},
	{
		.test_descr = "AES-128-CBC HMAC-SHA512 Decryption Digest "
			"Verify",
		.test_data = &aes_test_data_6,
		.op_mask = AES_TEST_OP_AUTH_VERIFY_DEC,
		.pmd_mask = AES_TEST_TARGET_PMD_MB |
			AES_TEST_TARGET_PMD_QAT
	},
	{
		.test_descr = "AES-128-CBC XCBC Encryption Digest",
		.test_data = &aes_test_data_7,
		.op_mask = AES_TEST_OP_ENC_AUTH_GEN,
		.pmd_mask = AES_TEST_TARGET_PMD_MB |
			AES_TEST_TARGET_PMD_QAT
	},
	{
		.test_descr = "AES-128-CBC XCBC Decryption Digest Verify",
		.test_data = &aes_test_data_7,
		.op_mask = AES_TEST_OP_AUTH_VERIFY_DEC,
		.pmd_mask = AES_TEST_TARGET_PMD_MB |
			AES_TEST_TARGET_PMD_QAT
	},
	{
		.test_descr = "AES-128-CBC HMAC-SHA1 Encryption Digest "
			"OOP",
		.test_data = &aes_test_data_4,
		.op_mask = AES_TEST_OP_ENC_AUTH_GEN,
		.feature_mask = AES_TEST_FEATURE_OOP,
		.pmd_mask = AES_TEST_TARGET_PMD_QAT
	},
	{
		.test_descr = "AES-128-CBC HMAC-SHA1 Decryption Digest "
			"Verify OOP",
		.test_data = &aes_test_data_4,
		.op_mask = AES_TEST_OP_AUTH_VERIFY_DEC,
		.feature_mask = AES_TEST_FEATURE_OOP,
		.pmd_mask = AES_TEST_TARGET_PMD_QAT
	},
	{
		.test_descr = "AES-128-CBC HMAC-SHA224 Encryption Digest",
		.test_data = &aes_test_data_8,
		.op_mask = AES_TEST_OP_ENC_AUTH_GEN,
		.pmd_mask = AES_TEST_TARGET_PMD_MB
	},
	{
		.test_descr = "AES-128-CBC HMAC-SHA224 Decryption Digest "
			"Verify",
		.test_data = &aes_test_data_8,
		.op_mask = AES_TEST_OP_AUTH_VERIFY_DEC,
		.pmd_mask = AES_TEST_TARGET_PMD_MB
	},
	{
		.test_descr = "AES-128-CBC HMAC-SHA384 Encryption Digest",
		.test_data = &aes_test_data_9,
		.op_mask = AES_TEST_OP_ENC_AUTH_GEN,
		.pmd_mask = AES_TEST_TARGET_PMD_MB
	},
	{
		.test_descr = "AES-128-CBC HMAC-SHA384 Decryption Digest "
			"Verify",
		.test_data = &aes_test_data_9,
		.op_mask = AES_TEST_OP_AUTH_VERIFY_DEC,
		.pmd_mask = AES_TEST_TARGET_PMD_MB
	},
};

static int
test_AES_one_case(const struct aes_test_case *t,
	struct rte_mempool *mbuf_pool,
	struct rte_mempool *op_mpool,
	uint8_t dev_id,
	enum rte_cryptodev_type cryptodev_type,
	char *test_msg)
{
	struct rte_mbuf *ibuf = NULL;
	struct rte_mbuf *obuf = NULL;
	struct rte_mbuf *iobuf;
	struct rte_crypto_sym_xform *cipher_xform = NULL;
	struct rte_crypto_sym_xform *auth_xform = NULL;
	struct rte_crypto_sym_xform *init_xform = NULL;
	struct rte_crypto_sym_op *sym_op = NULL;
	struct rte_crypto_op *op = NULL;
	struct rte_cryptodev_sym_session *sess = NULL;

	int status = TEST_SUCCESS;
	const struct aes_test_data *tdata = t->test_data;
	uint8_t cipher_key[tdata->cipher_key.len];
	uint8_t auth_key[tdata->auth_key.len];
	uint32_t buf_len = tdata->ciphertext.len;
	uint32_t digest_len = 0;
	char *buf_p = NULL;

	if (tdata->cipher_key.len)
		memcpy(cipher_key, tdata->cipher_key.data,
			tdata->cipher_key.len);
	if (tdata->auth_key.len)
		memcpy(auth_key, tdata->auth_key.data,
			tdata->auth_key.len);

	switch (cryptodev_type) {
	case RTE_CRYPTODEV_QAT_SYM_PMD:
		digest_len = tdata->digest.len;
		break;
	case RTE_CRYPTODEV_AESNI_MB_PMD:
		digest_len = tdata->digest.truncated_len;
		break;
	default:
		snprintf(test_msg, AES_TEST_MSG_LEN, "line %u FAILED: %s",
			__LINE__, "Unsupported PMD type");
		status = TEST_FAILED;
		goto error_exit;
	}

	/* preparing data */
	ibuf = rte_pktmbuf_alloc(mbuf_pool);
	if (!ibuf) {
		snprintf(test_msg, AES_TEST_MSG_LEN, "line %u FAILED: %s",
			__LINE__, "Allocation of rte_mbuf failed");
		status = TEST_FAILED;
		goto error_exit;
	}

	if (t->op_mask & AES_TEST_OP_CIPHER)
		buf_len += tdata->iv.len;
	if (t->op_mask & AES_TEST_OP_AUTH)
		buf_len += digest_len;

	buf_p = rte_pktmbuf_append(ibuf, buf_len);
	if (!buf_p) {
		snprintf(test_msg, AES_TEST_MSG_LEN, "line %u FAILED: %s",
			__LINE__, "No room to append mbuf");
		status = TEST_FAILED;
		goto error_exit;
	}

	if (t->op_mask & AES_TEST_OP_CIPHER) {
		rte_memcpy(buf_p, tdata->iv.data, tdata->iv.len);
		buf_p += tdata->iv.len;
	}

	/* only encryption requires plaintext.data input,
	 * decryption/(digest gen)/(digest verify) use ciphertext.data
	 * to be computed */
	if (t->op_mask & AES_TEST_OP_ENCRYPT) {
		rte_memcpy(buf_p, tdata->plaintext.data,
			tdata->plaintext.len);
		buf_p += tdata->plaintext.len;
	} else {
		rte_memcpy(buf_p, tdata->ciphertext.data,
			tdata->ciphertext.len);
		buf_p += tdata->ciphertext.len;
	}

	if (t->op_mask & AES_TEST_OP_AUTH_VERIFY)
		rte_memcpy(buf_p, tdata->digest.data, digest_len);
	else
		memset(buf_p, 0, digest_len);

	if (t->feature_mask & AES_TEST_FEATURE_OOP) {
		obuf = rte_pktmbuf_alloc(mbuf_pool);
		if (!obuf) {
			snprintf(test_msg, AES_TEST_MSG_LEN, "line %u "
				"FAILED: %s", __LINE__,
				"Allocation of rte_mbuf failed");
			status = TEST_FAILED;
			goto error_exit;
		}

		buf_p = rte_pktmbuf_append(obuf, buf_len);
		if (!buf_p) {
			snprintf(test_msg, AES_TEST_MSG_LEN, "line %u "
				"FAILED: %s", __LINE__,
				"No room to append mbuf");
			status = TEST_FAILED;
			goto error_exit;
		}
		memset(buf_p, 0, buf_len);
	}

	/* Generate Crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	if (!op) {
		snprintf(test_msg, AES_TEST_MSG_LEN, "line %u FAILED: %s",
			__LINE__, "Failed to allocate symmetric crypto "
			"operation struct");
		status = TEST_FAILED;
		goto error_exit;
	}

	sym_op = op->sym;

	sym_op->m_src = ibuf;

	if (t->feature_mask & AES_TEST_FEATURE_OOP) {
		sym_op->m_dst = obuf;
		iobuf = obuf;
	} else {
		sym_op->m_dst = NULL;
		iobuf = ibuf;
	}

	/* sessionless op requires allocate xform using
	 * rte_crypto_op_sym_xforms_alloc(), otherwise rte_zmalloc()
	 * is used */
	if (t->feature_mask & AES_TEST_FEATURE_SESSIONLESS) {
		uint32_t n_xforms = 0;

		if (t->op_mask & AES_TEST_OP_CIPHER)
			n_xforms++;
		if (t->op_mask & AES_TEST_OP_AUTH)
			n_xforms++;

		if (rte_crypto_op_sym_xforms_alloc(op, n_xforms)
			== NULL) {
			snprintf(test_msg, AES_TEST_MSG_LEN, "line %u "
				"FAILED: %s", __LINE__, "Failed to "
				"allocate space for crypto transforms");
			status = TEST_FAILED;
			goto error_exit;
		}
	} else {
		cipher_xform = rte_zmalloc(NULL,
			sizeof(struct rte_crypto_sym_xform), 0);

		auth_xform = rte_zmalloc(NULL,
			sizeof(struct rte_crypto_sym_xform), 0);

		if (!cipher_xform || !auth_xform) {
			snprintf(test_msg, AES_TEST_MSG_LEN, "line %u "
				"FAILED: %s", __LINE__, "Failed to "
				"allocate memory for crypto transforms");
			status = TEST_FAILED;
			goto error_exit;
		}
	}

	/* preparing xform, for sessioned op, init_xform is initialized
	 * here and later as param in rte_cryptodev_sym_session_create()
	 * call */
	if (t->op_mask == AES_TEST_OP_ENC_AUTH_GEN) {
		if (t->feature_mask & AES_TEST_FEATURE_SESSIONLESS) {
			cipher_xform = op->sym->xform;
			auth_xform = cipher_xform->next;
			auth_xform->next = NULL;
		} else {
			cipher_xform->next = auth_xform;
			auth_xform->next = NULL;
			init_xform = cipher_xform;
		}
	} else if (t->op_mask == AES_TEST_OP_AUTH_VERIFY_DEC) {
		if (t->feature_mask & AES_TEST_FEATURE_SESSIONLESS) {
			auth_xform = op->sym->xform;
			cipher_xform = auth_xform->next;
			cipher_xform->next = NULL;
		} else {
			auth_xform->next = cipher_xform;
			cipher_xform->next = NULL;
			init_xform = auth_xform;
		}
	} else if ((t->op_mask == AES_TEST_OP_ENCRYPT) ||
			(t->op_mask == AES_TEST_OP_DECRYPT)) {
		if (t->feature_mask & AES_TEST_FEATURE_SESSIONLESS)
			cipher_xform = op->sym->xform;
		else
			init_xform = cipher_xform;
		cipher_xform->next = NULL;
	} else if ((t->op_mask == AES_TEST_OP_AUTH_GEN) ||
			(t->op_mask == AES_TEST_OP_AUTH_VERIFY)) {
		if (t->feature_mask & AES_TEST_FEATURE_SESSIONLESS)
			auth_xform = op->sym->xform;
		else
			init_xform = auth_xform;
		auth_xform->next = NULL;
	} else {
		snprintf(test_msg, AES_TEST_MSG_LEN, "line %u FAILED: %s",
			__LINE__, "Unrecognized operation");
		status = TEST_FAILED;
		goto error_exit;
	}

	/*configure xforms & sym_op cipher and auth data*/
	if (t->op_mask & AES_TEST_OP_CIPHER) {
		cipher_xform->type = RTE_CRYPTO_SYM_XFORM_CIPHER;
		cipher_xform->cipher.algo = tdata->crypto_algo;
		if (t->op_mask & AES_TEST_OP_ENCRYPT)
			cipher_xform->cipher.op =
				RTE_CRYPTO_CIPHER_OP_ENCRYPT;
		else
			cipher_xform->cipher.op =
				RTE_CRYPTO_CIPHER_OP_DECRYPT;
		cipher_xform->cipher.key.data = cipher_key;
		cipher_xform->cipher.key.length = tdata->cipher_key.len;

		sym_op->cipher.data.offset = tdata->iv.len;
		sym_op->cipher.data.length = tdata->ciphertext.len;
		sym_op->cipher.iv.data = rte_pktmbuf_mtod(sym_op->m_src,
			uint8_t *);
		sym_op->cipher.iv.length = tdata->iv.len;
		sym_op->cipher.iv.phys_addr = rte_pktmbuf_mtophys(
			sym_op->m_src);
	}

	if (t->op_mask & AES_TEST_OP_AUTH) {
		uint32_t auth_data_offset = 0;
		uint32_t digest_offset = tdata->ciphertext.len;

		if (t->op_mask & AES_TEST_OP_CIPHER) {
			digest_offset += tdata->iv.len;
			auth_data_offset += tdata->iv.len;
		}

		auth_xform->type = RTE_CRYPTO_SYM_XFORM_AUTH;
		auth_xform->auth.algo = tdata->auth_algo;
		auth_xform->auth.key.length = tdata->auth_key.len;
		auth_xform->auth.key.data = auth_key;
		auth_xform->auth.digest_length = digest_len;

		if (t->op_mask & AES_TEST_OP_AUTH_GEN) {
			auth_xform->auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;
			sym_op->auth.digest.data = rte_pktmbuf_mtod_offset
				(iobuf, uint8_t *, digest_offset);
			sym_op->auth.digest.phys_addr =
				rte_pktmbuf_mtophys_offset(iobuf,
					digest_offset);
		} else {
			auth_xform->auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;
			sym_op->auth.digest.data = rte_pktmbuf_mtod_offset
				(sym_op->m_src, uint8_t *, digest_offset);
			sym_op->auth.digest.phys_addr =
				rte_pktmbuf_mtophys_offset(sym_op->m_src,
					digest_offset);
		}

		sym_op->auth.data.offset = auth_data_offset;
		sym_op->auth.data.length = tdata->ciphertext.len;
		sym_op->auth.digest.length = digest_len;
	}

	/* create session for sessioned op */
	if (!(t->feature_mask & AES_TEST_FEATURE_SESSIONLESS)) {
		sess = rte_cryptodev_sym_session_create(dev_id,
			init_xform);
		if (!sess) {
			snprintf(test_msg, AES_TEST_MSG_LEN, "line %u "
				"FAILED: %s", __LINE__,
				"Session creation failed");
			status = TEST_FAILED;
			goto error_exit;
		}

		/* attach symmetric crypto session to crypto operations */
		rte_crypto_op_attach_sym_session(op, sess);
	}

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		snprintf(test_msg, AES_TEST_MSG_LEN, "line %u FAILED: %s",
			__LINE__, "Error sending packet for encryption");
		status = TEST_FAILED;
		goto error_exit;
	}

	op = NULL;

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &op, 1) == 0)
		rte_pause();

	if (!op) {
		snprintf(test_msg, AES_TEST_MSG_LEN, "line %u FAILED: %s",
			__LINE__, "Failed to process sym crypto op");
		status = TEST_FAILED;
		goto error_exit;
	}

	TEST_HEXDUMP(stdout, "m_src:",
		rte_pktmbuf_mtod(sym_op->m_src, uint8_t *), buf_len);
	if (t->feature_mask & AES_TEST_FEATURE_OOP)
		TEST_HEXDUMP(stdout, "m_dst:",
			rte_pktmbuf_mtod(sym_op->m_dst, uint8_t *),
			buf_len);

	/* Verify results */
	if (op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		if (t->op_mask & AES_TEST_OP_AUTH_VERIFY)
			snprintf(test_msg, AES_TEST_MSG_LEN, "line %u "
				"FAILED: Digest verification failed "
				"(0x%X)", __LINE__, op->status);
		else
			snprintf(test_msg, AES_TEST_MSG_LEN, "line %u "
				"FAILED: Digest verification failed "
				"(0x%X)", __LINE__, op->status);
		status = TEST_FAILED;
		goto error_exit;
	}

	if (t->op_mask & AES_TEST_OP_CIPHER) {
		uint8_t *crypto_res;
		const uint8_t *compare_ref;
		uint32_t compare_len;

		crypto_res = rte_pktmbuf_mtod_offset(iobuf, uint8_t *,
			tdata->iv.len);

		if (t->op_mask & AES_TEST_OP_ENCRYPT) {
			compare_ref = tdata->ciphertext.data;
			compare_len = tdata->ciphertext.len;
		} else {
			compare_ref = tdata->plaintext.data;
			compare_len = tdata->plaintext.len;
		}

		if (memcmp(crypto_res, compare_ref, compare_len)) {
			snprintf(test_msg, AES_TEST_MSG_LEN, "line %u "
				"FAILED: %s", __LINE__,
				"Crypto data not as expected");
			status = TEST_FAILED;
			goto error_exit;
		}
	}

	if (t->op_mask & AES_TEST_OP_AUTH_GEN) {
		uint8_t *auth_res;

		if (t->op_mask & AES_TEST_OP_CIPHER)
			auth_res = rte_pktmbuf_mtod_offset(iobuf,
				uint8_t *,
				tdata->iv.len + tdata->ciphertext.len);
		else
			auth_res = rte_pktmbuf_mtod_offset(iobuf,
				uint8_t *, tdata->ciphertext.len);

		if (memcmp(auth_res, tdata->digest.data, digest_len)) {
			snprintf(test_msg, AES_TEST_MSG_LEN, "line %u "
				"FAILED: %s", __LINE__, "Generated "
				"digest data not as expected");
			status = TEST_FAILED;
			goto error_exit;
		}
	}

	snprintf(test_msg, AES_TEST_MSG_LEN, "PASS");

error_exit:
	if (!(t->feature_mask & AES_TEST_FEATURE_SESSIONLESS)) {
		if (sess)
			rte_cryptodev_sym_session_free(dev_id, sess);
		if (cipher_xform)
			rte_free(cipher_xform);
		if (auth_xform)
			rte_free(auth_xform);
	}

	if (op)
		rte_crypto_op_free(op);

	if (obuf)
		rte_pktmbuf_free(obuf);

	if (ibuf)
		rte_pktmbuf_free(ibuf);

	return status;
}

int
test_AES_all_tests(struct rte_mempool *mbuf_pool,
	struct rte_mempool *op_mpool,
	uint8_t dev_id,
	enum rte_cryptodev_type cryptodev_type)
{
	int status, overall_status = TEST_SUCCESS;
	uint32_t i, test_index = 0;
	char test_msg[AES_TEST_MSG_LEN + 1];
	uint32_t n_test_cases = sizeof(aes_test_cases) /
			sizeof(aes_test_cases[0]);
	uint32_t target_pmd_mask = 0;

	switch (cryptodev_type) {
	case RTE_CRYPTODEV_AESNI_MB_PMD:
		target_pmd_mask = AES_TEST_TARGET_PMD_MB;
		break;
	case RTE_CRYPTODEV_QAT_SYM_PMD:
		target_pmd_mask = AES_TEST_TARGET_PMD_QAT;
		break;
	default:
		TEST_ASSERT(-1, "Unrecognized cryptodev type");
		break;
	}

	for (i = 0; i < n_test_cases; i++) {
		const struct aes_test_case *tc = &aes_test_cases[i];

		if (!(tc->pmd_mask & target_pmd_mask))
			continue;

		status = test_AES_one_case(tc, mbuf_pool, op_mpool,
			dev_id, cryptodev_type, test_msg);

		printf("  %u) TestCase %s %s\n", test_index ++,
			tc->test_descr, test_msg);

		if (status != TEST_SUCCESS) {
			if (overall_status == TEST_SUCCESS)
				overall_status = status;

			if (tc->feature_mask & AES_TEST_FEATURE_STOPPER)
				break;
		}
	}

	return overall_status;
}
