/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2017 Intel Corporation
 */

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_pause.h>

#include <rte_crypto.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>

#include "test.h"
#include "test_cryptodev.h"
#include "test_cryptodev_blockcipher.h"
#include "test_cryptodev_aes_test_vectors.h"
#include "test_cryptodev_des_test_vectors.h"
#include "test_cryptodev_hash_test_vectors.h"

static int
verify_algo_support(const struct blockcipher_test_case *t,
		const uint8_t dev_id, const uint32_t digest_len)
{
	int ret = 0;
	const struct blockcipher_test_data *tdata = t->test_data;
	struct rte_cryptodev_sym_capability_idx cap_idx;
	const struct rte_cryptodev_symmetric_capability *capability;

	if (t->op_mask & BLOCKCIPHER_TEST_OP_CIPHER) {
		cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
		cap_idx.algo.cipher = tdata->crypto_algo;
		capability = rte_cryptodev_sym_capability_get(dev_id, &cap_idx);
		if (capability == NULL)
			return -1;

		if (cap_idx.algo.cipher != RTE_CRYPTO_CIPHER_NULL)
			ret = rte_cryptodev_sym_capability_check_cipher(capability,
							tdata->cipher_key.len,
							tdata->iv.len);
		if (ret != 0)
			return -1;
	}

	if (t->op_mask & BLOCKCIPHER_TEST_OP_AUTH) {
		cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
		cap_idx.algo.auth = tdata->auth_algo;
		capability = rte_cryptodev_sym_capability_get(dev_id, &cap_idx);
		if (capability == NULL)
			return -1;

		if (cap_idx.algo.auth != RTE_CRYPTO_AUTH_NULL)
			ret = rte_cryptodev_sym_capability_check_auth(capability,
							tdata->auth_key.len,
							digest_len,
							0);
		if (ret != 0)
			return -1;
	}

	return 0;
}

static int
test_blockcipher_one_case(const struct blockcipher_test_case *t,
	struct rte_mempool *mbuf_pool,
	struct rte_mempool *op_mpool,
	struct rte_mempool *sess_mpool,
	struct rte_mempool *sess_priv_mpool,
	uint8_t dev_id,
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
	struct rte_cryptodev_info dev_info;
	struct rte_cryptodev_sym_session *sess = NULL;

	int status = TEST_SUCCESS;
	const struct blockcipher_test_data *tdata = t->test_data;
	uint8_t cipher_key[tdata->cipher_key.len];
	uint8_t auth_key[tdata->auth_key.len];
	uint32_t buf_len = tdata->ciphertext.len;
	uint32_t digest_len = tdata->digest.len;
	char *buf_p = NULL;
	uint8_t src_pattern = 0xa5;
	uint8_t dst_pattern = 0xb6;
	uint8_t tmp_src_buf[MBUF_SIZE];
	uint8_t tmp_dst_buf[MBUF_SIZE];

	int nb_segs = 1;
	uint32_t nb_iterates = 0;

	rte_cryptodev_info_get(dev_id, &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if (t->feature_mask & BLOCKCIPHER_TEST_FEATURE_SESSIONLESS) {
		if (!(feat_flags & RTE_CRYPTODEV_FF_SYM_SESSIONLESS)) {
			printf("Device doesn't support sesionless operations "
				"Test Skipped.\n");
			snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN,
				"SKIPPED");
			return TEST_SKIPPED;
		}
	}
	if (t->feature_mask & BLOCKCIPHER_TEST_FEATURE_SG) {
		uint64_t oop_flag = RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT;

		if (t->feature_mask & BLOCKCIPHER_TEST_FEATURE_OOP) {
			if (!(feat_flags & oop_flag)) {
				printf("Device doesn't support out-of-place "
					"scatter-gather in input mbuf. "
					"Test Skipped.\n");
				snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN,
					"SKIPPED");
				return TEST_SKIPPED;
			}
		} else {
			if (!(feat_flags & RTE_CRYPTODEV_FF_IN_PLACE_SGL)) {
				printf("Device doesn't support in-place "
					"scatter-gather mbufs. "
					"Test Skipped.\n");
				snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN,
					"SKIPPED");
				return TEST_SKIPPED;
			}
		}

		nb_segs = 3;
	}

	if (t->feature_mask & BLOCKCIPHER_TEST_FEATURE_OOP) {
		uint64_t oop_flags = RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT |
			RTE_CRYPTODEV_FF_OOP_LB_IN_SGL_OUT |
			RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT |
			RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT;
		if (!(feat_flags & oop_flags)) {
			printf("Device doesn't support out-of-place operations."
				"Test Skipped.\n");
			snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN,
				"SKIPPED");
			return TEST_SKIPPED;
		}
	}

	if (tdata->cipher_key.len)
		memcpy(cipher_key, tdata->cipher_key.data,
			tdata->cipher_key.len);
	if (tdata->auth_key.len)
		memcpy(auth_key, tdata->auth_key.data,
			tdata->auth_key.len);

	/* Check if PMD is capable of performing that test */
	if (verify_algo_support(t, dev_id, digest_len) < 0) {
		RTE_LOG(DEBUG, USER1,
			"Device does not support this algorithm."
			"Test Skipped.\n");
		snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN, "SKIPPED");
		return TEST_SKIPPED;
	}

	/* preparing data */
	if (t->op_mask & BLOCKCIPHER_TEST_OP_AUTH)
		buf_len += digest_len;

	/* for contiguous mbuf, nb_segs is 1 */
	ibuf = create_segmented_mbuf(mbuf_pool,
			tdata->ciphertext.len, nb_segs, src_pattern);
	if (ibuf == NULL) {
		snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN,
			"line %u FAILED: %s",
			__LINE__, "Cannot create source mbuf");
		status = TEST_FAILED;
		goto error_exit;
	}

	/* only encryption requires plaintext.data input,
	 * decryption/(digest gen)/(digest verify) use ciphertext.data
	 * to be computed
	 */
	if (t->op_mask & BLOCKCIPHER_TEST_OP_ENCRYPT)
		pktmbuf_write(ibuf, 0, tdata->plaintext.len,
				tdata->plaintext.data);
	else
		pktmbuf_write(ibuf, 0, tdata->ciphertext.len,
				tdata->ciphertext.data);

	buf_p = rte_pktmbuf_append(ibuf, digest_len);
	if (t->op_mask & BLOCKCIPHER_TEST_OP_AUTH_VERIFY)
		rte_memcpy(buf_p, tdata->digest.data, digest_len);
	else
		memset(buf_p, 0, digest_len);

	if (t->feature_mask & BLOCKCIPHER_TEST_FEATURE_OOP) {
		obuf = rte_pktmbuf_alloc(mbuf_pool);
		if (!obuf) {
			snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN, "line %u "
				"FAILED: %s", __LINE__,
				"Allocation of rte_mbuf failed");
			status = TEST_FAILED;
			goto error_exit;
		}
		memset(obuf->buf_addr, dst_pattern, obuf->buf_len);

		buf_p = rte_pktmbuf_append(obuf, buf_len);
		if (!buf_p) {
			snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN, "line %u "
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
		snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN,
			"line %u FAILED: %s",
			__LINE__, "Failed to allocate symmetric crypto "
			"operation struct");
		status = TEST_FAILED;
		goto error_exit;
	}

	sym_op = op->sym;

iterate:
	if (nb_iterates) {
		struct rte_mbuf *tmp_buf = ibuf;

		ibuf = obuf;
		obuf = tmp_buf;

		rte_pktmbuf_reset(ibuf);
		rte_pktmbuf_reset(obuf);

		rte_pktmbuf_append(ibuf, tdata->ciphertext.len);

		/* only encryption requires plaintext.data input,
		 * decryption/(digest gen)/(digest verify) use ciphertext.data
		 * to be computed
		 */
		if (t->op_mask & BLOCKCIPHER_TEST_OP_ENCRYPT)
			pktmbuf_write(ibuf, 0, tdata->plaintext.len,
					tdata->plaintext.data);
		else
			pktmbuf_write(ibuf, 0, tdata->ciphertext.len,
					tdata->ciphertext.data);

		buf_p = rte_pktmbuf_append(ibuf, digest_len);
		if (t->op_mask & BLOCKCIPHER_TEST_OP_AUTH_VERIFY)
			rte_memcpy(buf_p, tdata->digest.data, digest_len);
		else
			memset(buf_p, 0, digest_len);

		memset(obuf->buf_addr, dst_pattern, obuf->buf_len);

		buf_p = rte_pktmbuf_append(obuf, buf_len);
		if (!buf_p) {
			snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN, "line %u "
				"FAILED: %s", __LINE__,
				"No room to append mbuf");
			status = TEST_FAILED;
			goto error_exit;
		}
		memset(buf_p, 0, buf_len);
	}

	sym_op->m_src = ibuf;

	if (t->feature_mask & BLOCKCIPHER_TEST_FEATURE_OOP) {
		sym_op->m_dst = obuf;
		iobuf = obuf;
	} else {
		sym_op->m_dst = NULL;
		iobuf = ibuf;
	}

	/* sessionless op requires allocate xform using
	 * rte_crypto_op_sym_xforms_alloc(), otherwise rte_zmalloc()
	 * is used
	 */
	if (t->feature_mask & BLOCKCIPHER_TEST_FEATURE_SESSIONLESS) {
		uint32_t n_xforms = 0;

		if (t->op_mask & BLOCKCIPHER_TEST_OP_CIPHER)
			n_xforms++;
		if (t->op_mask & BLOCKCIPHER_TEST_OP_AUTH)
			n_xforms++;

		if (rte_crypto_op_sym_xforms_alloc(op, n_xforms)
			== NULL) {
			snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN, "line %u "
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
			snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN, "line %u "
				"FAILED: %s", __LINE__, "Failed to "
				"allocate memory for crypto transforms");
			status = TEST_FAILED;
			goto error_exit;
		}
	}

	/* preparing xform, for sessioned op, init_xform is initialized
	 * here and later as param in rte_cryptodev_sym_session_create() call
	 */
	if (t->op_mask == BLOCKCIPHER_TEST_OP_ENC_AUTH_GEN) {
		if (t->feature_mask & BLOCKCIPHER_TEST_FEATURE_SESSIONLESS) {
			cipher_xform = op->sym->xform;
			auth_xform = cipher_xform->next;
			auth_xform->next = NULL;
		} else {
			cipher_xform->next = auth_xform;
			auth_xform->next = NULL;
			init_xform = cipher_xform;
		}
	} else if (t->op_mask == BLOCKCIPHER_TEST_OP_AUTH_VERIFY_DEC) {
		if (t->feature_mask & BLOCKCIPHER_TEST_FEATURE_SESSIONLESS) {
			auth_xform = op->sym->xform;
			cipher_xform = auth_xform->next;
			cipher_xform->next = NULL;
		} else {
			auth_xform->next = cipher_xform;
			cipher_xform->next = NULL;
			init_xform = auth_xform;
		}
	} else if ((t->op_mask == BLOCKCIPHER_TEST_OP_ENCRYPT) ||
			(t->op_mask == BLOCKCIPHER_TEST_OP_DECRYPT)) {
		if (t->feature_mask & BLOCKCIPHER_TEST_FEATURE_SESSIONLESS)
			cipher_xform = op->sym->xform;
		else
			init_xform = cipher_xform;
		cipher_xform->next = NULL;
	} else if ((t->op_mask == BLOCKCIPHER_TEST_OP_AUTH_GEN) ||
			(t->op_mask == BLOCKCIPHER_TEST_OP_AUTH_VERIFY)) {
		if (t->feature_mask & BLOCKCIPHER_TEST_FEATURE_SESSIONLESS)
			auth_xform = op->sym->xform;
		else
			init_xform = auth_xform;
		auth_xform->next = NULL;
	} else {
		snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN,
			"line %u FAILED: %s",
			__LINE__, "Unrecognized operation");
		status = TEST_FAILED;
		goto error_exit;
	}

	/*configure xforms & sym_op cipher and auth data*/
	if (t->op_mask & BLOCKCIPHER_TEST_OP_CIPHER) {
		cipher_xform->type = RTE_CRYPTO_SYM_XFORM_CIPHER;
		cipher_xform->cipher.algo = tdata->crypto_algo;
		if (t->op_mask & BLOCKCIPHER_TEST_OP_ENCRYPT)
			cipher_xform->cipher.op =
				RTE_CRYPTO_CIPHER_OP_ENCRYPT;
		else
			cipher_xform->cipher.op =
				RTE_CRYPTO_CIPHER_OP_DECRYPT;
		cipher_xform->cipher.key.data = cipher_key;
		cipher_xform->cipher.key.length = tdata->cipher_key.len;
		cipher_xform->cipher.iv.offset = IV_OFFSET;

		if (tdata->crypto_algo == RTE_CRYPTO_CIPHER_NULL)
			cipher_xform->cipher.iv.length = 0;
		else
			cipher_xform->cipher.iv.length = tdata->iv.len;

		sym_op->cipher.data.offset = tdata->cipher_offset;
		sym_op->cipher.data.length = tdata->ciphertext.len -
				tdata->cipher_offset;
		rte_memcpy(rte_crypto_op_ctod_offset(op, uint8_t *, IV_OFFSET),
				tdata->iv.data,
				tdata->iv.len);
	}

	if (t->op_mask & BLOCKCIPHER_TEST_OP_AUTH) {
		uint32_t digest_offset = tdata->ciphertext.len;

		auth_xform->type = RTE_CRYPTO_SYM_XFORM_AUTH;
		auth_xform->auth.algo = tdata->auth_algo;
		auth_xform->auth.key.length = tdata->auth_key.len;
		auth_xform->auth.key.data = auth_key;
		auth_xform->auth.digest_length = digest_len;

		if (t->op_mask & BLOCKCIPHER_TEST_OP_AUTH_GEN) {
			auth_xform->auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;
			sym_op->auth.digest.data = pktmbuf_mtod_offset
				(iobuf, digest_offset);
			sym_op->auth.digest.phys_addr =
				pktmbuf_iova_offset(iobuf,
					digest_offset);
		} else {
			auth_xform->auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;
			sym_op->auth.digest.data = pktmbuf_mtod_offset
				(sym_op->m_src, digest_offset);
			sym_op->auth.digest.phys_addr =
				pktmbuf_iova_offset(sym_op->m_src,
					digest_offset);
		}

		sym_op->auth.data.offset = tdata->auth_offset;
		sym_op->auth.data.length = tdata->ciphertext.len -
				tdata->auth_offset;
	}

	/**
	 * Create session for sessioned op. For mbuf iteration test,
	 * skip the session creation for the second iteration.
	 */
	if (!(t->feature_mask & BLOCKCIPHER_TEST_FEATURE_SESSIONLESS) &&
			nb_iterates == 0) {
		sess = rte_cryptodev_sym_session_create(sess_mpool);

		status = rte_cryptodev_sym_session_init(dev_id, sess,
				init_xform, sess_priv_mpool);
		if (status == -ENOTSUP) {
			snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN, "UNSUPPORTED");
			status = TEST_SKIPPED;
			goto error_exit;
		}
		if (!sess || status < 0) {
			snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN, "line %u "
				"FAILED: %s", __LINE__,
				"Session creation failed");
			status = TEST_FAILED;
			goto error_exit;
		}

		/* attach symmetric crypto session to crypto operations */
		rte_crypto_op_attach_sym_session(op, sess);
	}

	debug_hexdump(stdout, "m_src(before):",
			sym_op->m_src->buf_addr, sym_op->m_src->buf_len);
	rte_memcpy(tmp_src_buf, sym_op->m_src->buf_addr,
						sym_op->m_src->buf_len);
	if (t->feature_mask & BLOCKCIPHER_TEST_FEATURE_OOP) {
		debug_hexdump(stdout, "m_dst(before):",
			sym_op->m_dst->buf_addr, sym_op->m_dst->buf_len);
		rte_memcpy(tmp_dst_buf, sym_op->m_dst->buf_addr,
						sym_op->m_dst->buf_len);
	}

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN,
			"line %u FAILED: %s",
			__LINE__, "Error sending packet for encryption");
		status = TEST_FAILED;
		goto error_exit;
	}

	op = NULL;

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &op, 1) == 0)
		rte_pause();

	if (!op) {
		snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN,
			"line %u FAILED: %s",
			__LINE__, "Failed to process sym crypto op");
		status = TEST_FAILED;
		goto error_exit;
	}

	debug_hexdump(stdout, "m_src(after):",
			sym_op->m_src->buf_addr, sym_op->m_src->buf_len);
	if (t->feature_mask & BLOCKCIPHER_TEST_FEATURE_OOP)
		debug_hexdump(stdout, "m_dst(after):",
			sym_op->m_dst->buf_addr, sym_op->m_dst->buf_len);

	/* Verify results */
	if (op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		if ((t->op_mask & BLOCKCIPHER_TEST_OP_AUTH_VERIFY) &&
			(op->status == RTE_CRYPTO_OP_STATUS_AUTH_FAILED))
			snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN, "line %u "
				"FAILED: Digest verification failed "
				"(0x%X)", __LINE__, op->status);
		else
			snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN, "line %u "
				"FAILED: Operation failed "
				"(0x%X)", __LINE__, op->status);
		status = TEST_FAILED;
		goto error_exit;
	}

	if (t->op_mask & BLOCKCIPHER_TEST_OP_CIPHER) {
		uint8_t buffer[2048];
		const uint8_t *compare_ref;
		uint32_t compare_len;

		if (t->op_mask & BLOCKCIPHER_TEST_OP_ENCRYPT) {
			compare_ref = tdata->ciphertext.data +
					tdata->cipher_offset;
			compare_len = tdata->ciphertext.len -
					tdata->cipher_offset;
		} else {
			compare_ref = tdata->plaintext.data +
					tdata->cipher_offset;
			compare_len = tdata->plaintext.len -
					tdata->cipher_offset;
		}

		if (memcmp(rte_pktmbuf_read(iobuf, tdata->cipher_offset,
				compare_len, buffer), compare_ref,
				compare_len)) {
			snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN, "line %u "
				"FAILED: %s", __LINE__,
				"Crypto data not as expected");
			status = TEST_FAILED;
			goto error_exit;
		}
	}

	if (t->op_mask & BLOCKCIPHER_TEST_OP_AUTH_GEN) {
		uint8_t *auth_res = pktmbuf_mtod_offset(iobuf,
					tdata->ciphertext.len);

		if (memcmp(auth_res, tdata->digest.data, digest_len)) {
			snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN, "line %u "
				"FAILED: %s", __LINE__, "Generated "
				"digest data not as expected");
			status = TEST_FAILED;
			goto error_exit;
		}
	}

	/* The only parts that should have changed in the buffer are
	 * plaintext/ciphertext and digest.
	 * In OOP only the dest buffer should change.
	 */
	if (t->feature_mask & BLOCKCIPHER_TEST_FEATURE_OOP) {
		struct rte_mbuf *mbuf;
		uint8_t value;
		uint32_t head_unchanged_len, changed_len = 0;
		uint32_t i;
		uint32_t hdroom_used = 0, tlroom_used = 0;
		uint32_t hdroom = 0;

		mbuf = sym_op->m_src;
		/*
		 * Crypto PMDs specify the headroom & tailroom it would use
		 * when processing the crypto operation. PMD is free to modify
		 * this space, and so the verification check should skip that
		 * block.
		 */
		hdroom_used = dev_info.min_mbuf_headroom_req;
		tlroom_used = dev_info.min_mbuf_tailroom_req;

		/* Get headroom */
		hdroom = rte_pktmbuf_headroom(mbuf);

		head_unchanged_len = mbuf->buf_len;

		for (i = 0; i < mbuf->buf_len; i++) {

			/* Skip headroom used by PMD */
			if (i == hdroom - hdroom_used)
				i += hdroom_used;

			/* Skip tailroom used by PMD */
			if (i == (hdroom + mbuf->data_len))
				i += tlroom_used;

			value = *((uint8_t *)(mbuf->buf_addr)+i);
			if (value != tmp_src_buf[i]) {
				snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN,
	"line %u FAILED: OOP src outer mbuf data (0x%x) not as expected (0x%x)",
					__LINE__, value, tmp_src_buf[i]);
				status = TEST_FAILED;
				goto error_exit;
			}
		}

		mbuf = sym_op->m_dst;
		if (t->op_mask & BLOCKCIPHER_TEST_OP_AUTH) {
			head_unchanged_len = hdroom + sym_op->auth.data.offset;
			changed_len = sym_op->auth.data.length;
			if (t->op_mask & BLOCKCIPHER_TEST_OP_AUTH_GEN)
				changed_len += digest_len;
		} else {
			/* cipher-only */
			head_unchanged_len = hdroom +
					sym_op->cipher.data.offset;
			changed_len = sym_op->cipher.data.length;
		}

		for (i = 0; i < mbuf->buf_len; i++) {
			if (i == head_unchanged_len)
				i += changed_len;
			value = *((uint8_t *)(mbuf->buf_addr)+i);
			if (value != tmp_dst_buf[i]) {
				snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN,
				"line %u FAILED: OOP dst outer mbuf data "
				"(0x%x) not as expected (0x%x)",
				__LINE__, value, tmp_dst_buf[i]);
				status = TEST_FAILED;
				goto error_exit;
			}
		}

		if (!nb_iterates) {
			nb_iterates++;
			goto iterate;
		}
	} else {
		/* In-place operation */
		struct rte_mbuf *mbuf;
		uint8_t value;
		uint32_t head_unchanged_len = 0, changed_len = 0;
		uint32_t i;
		uint32_t hdroom_used = 0, tlroom_used = 0;
		uint32_t hdroom = 0;

		/*
		 * Crypto PMDs specify the headroom & tailroom it would use
		 * when processing the crypto operation. PMD is free to modify
		 * this space, and so the verification check should skip that
		 * block.
		 */
		hdroom_used = dev_info.min_mbuf_headroom_req;
		tlroom_used = dev_info.min_mbuf_tailroom_req;

		mbuf = sym_op->m_src;

		/* Get headroom */
		hdroom = rte_pktmbuf_headroom(mbuf);

		if (t->op_mask & BLOCKCIPHER_TEST_OP_CIPHER) {
			head_unchanged_len = hdroom +
					sym_op->cipher.data.offset;
			changed_len = sym_op->cipher.data.length;
		} else {
			/* auth-only */
			head_unchanged_len = hdroom +
					sym_op->auth.data.offset +
					sym_op->auth.data.length;
			changed_len = 0;
		}

		if (t->op_mask & BLOCKCIPHER_TEST_OP_AUTH_GEN)
			changed_len += digest_len;

		for (i = 0; i < mbuf->buf_len; i++) {

			/* Skip headroom used by PMD */
			if (i == hdroom - hdroom_used)
				i += hdroom_used;

			if (i == head_unchanged_len)
				i += changed_len;

			/* Skip tailroom used by PMD */
			if (i == (hdroom + mbuf->data_len))
				i += tlroom_used;

			value = *((uint8_t *)(mbuf->buf_addr)+i);
			if (value != tmp_src_buf[i]) {
				snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN,
				"line %u FAILED: outer mbuf data (0x%x) "
				"not as expected (0x%x)",
				__LINE__, value, tmp_src_buf[i]);
				status = TEST_FAILED;
				goto error_exit;
			}
		}
	}

	snprintf(test_msg, BLOCKCIPHER_TEST_MSG_LEN, "PASS");

error_exit:
	if (!(t->feature_mask & BLOCKCIPHER_TEST_FEATURE_SESSIONLESS)) {
		if (sess) {
			rte_cryptodev_sym_session_clear(dev_id, sess);
			rte_cryptodev_sym_session_free(sess);
		}
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
test_blockcipher_all_tests(struct rte_mempool *mbuf_pool,
	struct rte_mempool *op_mpool,
	struct rte_mempool *sess_mpool,
	struct rte_mempool *sess_priv_mpool,
	uint8_t dev_id,
	enum blockcipher_test_type test_type)
{
	int status, overall_status = TEST_SUCCESS;
	uint32_t i, test_index = 0;
	char test_msg[BLOCKCIPHER_TEST_MSG_LEN + 1];
	uint32_t n_test_cases = 0;
	const struct blockcipher_test_case *tcs = NULL;

	switch (test_type) {
	case BLKCIPHER_AES_CHAIN_TYPE:
		n_test_cases = sizeof(aes_chain_test_cases) /
		sizeof(aes_chain_test_cases[0]);
		tcs = aes_chain_test_cases;
		break;
	case BLKCIPHER_AES_CIPHERONLY_TYPE:
		n_test_cases = sizeof(aes_cipheronly_test_cases) /
		sizeof(aes_cipheronly_test_cases[0]);
		tcs = aes_cipheronly_test_cases;
		break;
	case BLKCIPHER_AES_DOCSIS_TYPE:
		n_test_cases = sizeof(aes_docsis_test_cases) /
		sizeof(aes_docsis_test_cases[0]);
		tcs = aes_docsis_test_cases;
		break;
	case BLKCIPHER_3DES_CHAIN_TYPE:
		n_test_cases = sizeof(triple_des_chain_test_cases) /
		sizeof(triple_des_chain_test_cases[0]);
		tcs = triple_des_chain_test_cases;
		break;
	case BLKCIPHER_3DES_CIPHERONLY_TYPE:
		n_test_cases = sizeof(triple_des_cipheronly_test_cases) /
		sizeof(triple_des_cipheronly_test_cases[0]);
		tcs = triple_des_cipheronly_test_cases;
		break;
	case BLKCIPHER_DES_CIPHERONLY_TYPE:
		n_test_cases = sizeof(des_cipheronly_test_cases) /
		sizeof(des_cipheronly_test_cases[0]);
		tcs = des_cipheronly_test_cases;
		break;
	case BLKCIPHER_DES_DOCSIS_TYPE:
		n_test_cases = sizeof(des_docsis_test_cases) /
		sizeof(des_docsis_test_cases[0]);
		tcs = des_docsis_test_cases;
		break;
	case BLKCIPHER_AUTHONLY_TYPE:
		n_test_cases = sizeof(hash_test_cases) /
		sizeof(hash_test_cases[0]);
		tcs = hash_test_cases;
		break;
	default:
		break;
	}

	for (i = 0; i < n_test_cases; i++) {
		const struct blockcipher_test_case *tc = &tcs[i];

		status = test_blockcipher_one_case(tc, mbuf_pool, op_mpool,
			sess_mpool, sess_priv_mpool, dev_id,
			test_msg);

		printf("  %u) TestCase %s %s\n", test_index ++,
			tc->test_descr, test_msg);

		if (status == TEST_FAILED) {
			overall_status = status;

			if (tc->feature_mask & BLOCKCIPHER_TEST_FEATURE_STOPPER)
				break;
		}
	}

	return overall_status;
}
