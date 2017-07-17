/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
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

#include <rte_cryptodev.h>

#include "cperf_ops.h"
#include "cperf_test_vectors.h"

static int
cperf_set_ops_null_cipher(struct rte_crypto_op **ops,
		struct rte_mbuf **bufs_in, struct rte_mbuf **bufs_out,
		uint16_t nb_ops, struct rte_cryptodev_sym_session *sess,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector __rte_unused,
		uint16_t iv_offset __rte_unused)
{
	uint16_t i;

	for (i = 0; i < nb_ops; i++) {
		struct rte_crypto_sym_op *sym_op = ops[i]->sym;

		rte_crypto_op_attach_sym_session(ops[i], sess);

		sym_op->m_src = bufs_in[i];
		sym_op->m_dst = bufs_out[i];

		/* cipher parameters */
		sym_op->cipher.data.length = options->test_buffer_size;
		sym_op->cipher.data.offset = 0;
	}

	return 0;
}

static int
cperf_set_ops_null_auth(struct rte_crypto_op **ops,
		struct rte_mbuf **bufs_in, struct rte_mbuf **bufs_out,
		uint16_t nb_ops, struct rte_cryptodev_sym_session *sess,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector __rte_unused,
		uint16_t iv_offset __rte_unused)
{
	uint16_t i;

	for (i = 0; i < nb_ops; i++) {
		struct rte_crypto_sym_op *sym_op = ops[i]->sym;

		rte_crypto_op_attach_sym_session(ops[i], sess);

		sym_op->m_src = bufs_in[i];
		sym_op->m_dst = bufs_out[i];

		/* auth parameters */
		sym_op->auth.data.length = options->test_buffer_size;
		sym_op->auth.data.offset = 0;
	}

	return 0;
}

static int
cperf_set_ops_cipher(struct rte_crypto_op **ops,
		struct rte_mbuf **bufs_in, struct rte_mbuf **bufs_out,
		uint16_t nb_ops, struct rte_cryptodev_sym_session *sess,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector,
		uint16_t iv_offset)
{
	uint16_t i;

	for (i = 0; i < nb_ops; i++) {
		struct rte_crypto_sym_op *sym_op = ops[i]->sym;

		rte_crypto_op_attach_sym_session(ops[i], sess);

		sym_op->m_src = bufs_in[i];
		sym_op->m_dst = bufs_out[i];

		/* cipher parameters */
		if (options->cipher_algo == RTE_CRYPTO_CIPHER_SNOW3G_UEA2 ||
				options->cipher_algo == RTE_CRYPTO_CIPHER_KASUMI_F8 ||
				options->cipher_algo == RTE_CRYPTO_CIPHER_ZUC_EEA3)
			sym_op->cipher.data.length = options->test_buffer_size << 3;
		else
			sym_op->cipher.data.length = options->test_buffer_size;

		sym_op->cipher.data.offset = 0;
	}

	if (options->test == CPERF_TEST_TYPE_VERIFY) {
		for (i = 0; i < nb_ops; i++) {
			uint8_t *iv_ptr = rte_crypto_op_ctod_offset(ops[i],
					uint8_t *, iv_offset);

			memcpy(iv_ptr, test_vector->cipher_iv.data,
					test_vector->cipher_iv.length);

		}
	}

	return 0;
}

static int
cperf_set_ops_auth(struct rte_crypto_op **ops,
		struct rte_mbuf **bufs_in, struct rte_mbuf **bufs_out,
		uint16_t nb_ops, struct rte_cryptodev_sym_session *sess,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector,
		uint16_t iv_offset)
{
	uint16_t i;

	for (i = 0; i < nb_ops; i++) {
		struct rte_crypto_sym_op *sym_op = ops[i]->sym;

		rte_crypto_op_attach_sym_session(ops[i], sess);

		sym_op->m_src = bufs_in[i];
		sym_op->m_dst = bufs_out[i];

		if (test_vector->auth_iv.length) {
			uint8_t *iv_ptr = rte_crypto_op_ctod_offset(ops[i],
								uint8_t *,
								iv_offset);
			memcpy(iv_ptr, test_vector->auth_iv.data,
					test_vector->auth_iv.length);
		}

		/* authentication parameters */
		if (options->auth_op == RTE_CRYPTO_AUTH_OP_VERIFY) {
			sym_op->auth.digest.data = test_vector->digest.data;
			sym_op->auth.digest.phys_addr =
					test_vector->digest.phys_addr;
		} else {

			uint32_t offset = options->test_buffer_size;
			struct rte_mbuf *buf, *tbuf;

			if (options->out_of_place) {
				buf =  bufs_out[i];
			} else {
				tbuf =  bufs_in[i];
				while ((tbuf->next != NULL) &&
						(offset >= tbuf->data_len)) {
					offset -= tbuf->data_len;
					tbuf = tbuf->next;
				}
				buf = tbuf;
			}

			sym_op->auth.digest.data = rte_pktmbuf_mtod_offset(buf,
					uint8_t *, offset);
			sym_op->auth.digest.phys_addr =
					rte_pktmbuf_mtophys_offset(buf,	offset);

		}

		if (options->auth_algo == RTE_CRYPTO_AUTH_SNOW3G_UIA2 ||
				options->auth_algo == RTE_CRYPTO_AUTH_KASUMI_F9 ||
				options->auth_algo == RTE_CRYPTO_AUTH_ZUC_EIA3)
			sym_op->auth.data.length = options->test_buffer_size << 3;
		else
			sym_op->auth.data.length = options->test_buffer_size;

		sym_op->auth.data.offset = 0;
	}

	if (options->test == CPERF_TEST_TYPE_VERIFY) {
		if (test_vector->auth_iv.length) {
			for (i = 0; i < nb_ops; i++) {
				uint8_t *iv_ptr = rte_crypto_op_ctod_offset(ops[i],
						uint8_t *, iv_offset);

				memcpy(iv_ptr, test_vector->auth_iv.data,
						test_vector->auth_iv.length);
			}
		}
	}
	return 0;
}

static int
cperf_set_ops_cipher_auth(struct rte_crypto_op **ops,
		struct rte_mbuf **bufs_in, struct rte_mbuf **bufs_out,
		uint16_t nb_ops, struct rte_cryptodev_sym_session *sess,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector,
		uint16_t iv_offset)
{
	uint16_t i;

	for (i = 0; i < nb_ops; i++) {
		struct rte_crypto_sym_op *sym_op = ops[i]->sym;

		rte_crypto_op_attach_sym_session(ops[i], sess);

		sym_op->m_src = bufs_in[i];
		sym_op->m_dst = bufs_out[i];

		/* cipher parameters */
		if (options->cipher_algo == RTE_CRYPTO_CIPHER_SNOW3G_UEA2 ||
				options->cipher_algo == RTE_CRYPTO_CIPHER_KASUMI_F8 ||
				options->cipher_algo == RTE_CRYPTO_CIPHER_ZUC_EEA3)
			sym_op->cipher.data.length = options->test_buffer_size << 3;
		else
			sym_op->cipher.data.length = options->test_buffer_size;

		sym_op->cipher.data.offset = 0;

		/* authentication parameters */
		if (options->auth_op == RTE_CRYPTO_AUTH_OP_VERIFY) {
			sym_op->auth.digest.data = test_vector->digest.data;
			sym_op->auth.digest.phys_addr =
					test_vector->digest.phys_addr;
		} else {

			uint32_t offset = options->test_buffer_size;
			struct rte_mbuf *buf, *tbuf;

			if (options->out_of_place) {
				buf =  bufs_out[i];
			} else {
				tbuf =  bufs_in[i];
				while ((tbuf->next != NULL) &&
						(offset >= tbuf->data_len)) {
					offset -= tbuf->data_len;
					tbuf = tbuf->next;
				}
				buf = tbuf;
			}

			sym_op->auth.digest.data = rte_pktmbuf_mtod_offset(buf,
					uint8_t *, offset);
			sym_op->auth.digest.phys_addr =
					rte_pktmbuf_mtophys_offset(buf,	offset);
		}

		if (options->auth_algo == RTE_CRYPTO_AUTH_SNOW3G_UIA2 ||
				options->auth_algo == RTE_CRYPTO_AUTH_KASUMI_F9 ||
				options->auth_algo == RTE_CRYPTO_AUTH_ZUC_EIA3)
			sym_op->auth.data.length = options->test_buffer_size << 3;
		else
			sym_op->auth.data.length = options->test_buffer_size;

		sym_op->auth.data.offset = 0;
	}

	if (options->test == CPERF_TEST_TYPE_VERIFY) {
		for (i = 0; i < nb_ops; i++) {
			uint8_t *iv_ptr = rte_crypto_op_ctod_offset(ops[i],
					uint8_t *, iv_offset);

			memcpy(iv_ptr, test_vector->cipher_iv.data,
					test_vector->cipher_iv.length);
			if (test_vector->auth_iv.length) {
				/*
				 * Copy IV after the crypto operation and
				 * the cipher IV
				 */
				iv_ptr += test_vector->cipher_iv.length;
				memcpy(iv_ptr, test_vector->auth_iv.data,
						test_vector->auth_iv.length);
			}
		}

	}

	return 0;
}

static int
cperf_set_ops_aead(struct rte_crypto_op **ops,
		struct rte_mbuf **bufs_in, struct rte_mbuf **bufs_out,
		uint16_t nb_ops, struct rte_cryptodev_sym_session *sess,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector,
		uint16_t iv_offset)
{
	uint16_t i;

	for (i = 0; i < nb_ops; i++) {
		struct rte_crypto_sym_op *sym_op = ops[i]->sym;

		rte_crypto_op_attach_sym_session(ops[i], sess);

		sym_op->m_src = bufs_in[i];
		sym_op->m_dst = bufs_out[i];

		/* AEAD parameters */
		sym_op->aead.data.length = options->test_buffer_size;
		sym_op->aead.data.offset =
				RTE_ALIGN_CEIL(options->aead_aad_sz, 16);

		sym_op->aead.aad.data = rte_pktmbuf_mtod(bufs_in[i], uint8_t *);
		sym_op->aead.aad.phys_addr = rte_pktmbuf_mtophys(bufs_in[i]);

		if (options->aead_op == RTE_CRYPTO_AEAD_OP_DECRYPT) {
			sym_op->aead.digest.data = test_vector->digest.data;
			sym_op->aead.digest.phys_addr =
					test_vector->digest.phys_addr;
		} else {

			uint32_t offset = sym_op->aead.data.length +
						sym_op->aead.data.offset;
			struct rte_mbuf *buf, *tbuf;

			if (options->out_of_place) {
				buf =  bufs_out[i];
			} else {
				tbuf =  bufs_in[i];
				while ((tbuf->next != NULL) &&
						(offset >= tbuf->data_len)) {
					offset -= tbuf->data_len;
					tbuf = tbuf->next;
				}
				buf = tbuf;
			}

			sym_op->aead.digest.data = rte_pktmbuf_mtod_offset(buf,
					uint8_t *, offset);
			sym_op->aead.digest.phys_addr =
					rte_pktmbuf_mtophys_offset(buf,	offset);
		}
	}

	if (options->test == CPERF_TEST_TYPE_VERIFY) {
		for (i = 0; i < nb_ops; i++) {
			uint8_t *iv_ptr = rte_crypto_op_ctod_offset(ops[i],
					uint8_t *, iv_offset);

			memcpy(iv_ptr, test_vector->aead_iv.data,
					test_vector->aead_iv.length);
		}
	}

	return 0;
}

static struct rte_cryptodev_sym_session *
cperf_create_session(struct rte_mempool *sess_mp,
	uint8_t dev_id,
	const struct cperf_options *options,
	const struct cperf_test_vector *test_vector,
	uint16_t iv_offset)
{
	struct rte_crypto_sym_xform cipher_xform;
	struct rte_crypto_sym_xform auth_xform;
	struct rte_crypto_sym_xform aead_xform;
	struct rte_cryptodev_sym_session *sess = NULL;

	sess = rte_cryptodev_sym_session_create(sess_mp);
	/*
	 * cipher only
	 */
	if (options->op_type == CPERF_CIPHER_ONLY) {
		cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
		cipher_xform.next = NULL;
		cipher_xform.cipher.algo = options->cipher_algo;
		cipher_xform.cipher.op = options->cipher_op;
		cipher_xform.cipher.iv.offset = iv_offset;

		/* cipher different than null */
		if (options->cipher_algo != RTE_CRYPTO_CIPHER_NULL) {
			cipher_xform.cipher.key.data =
					test_vector->cipher_key.data;
			cipher_xform.cipher.key.length =
					test_vector->cipher_key.length;
			cipher_xform.cipher.iv.length =
					test_vector->cipher_iv.length;
		} else {
			cipher_xform.cipher.key.data = NULL;
			cipher_xform.cipher.key.length = 0;
			cipher_xform.cipher.iv.length = 0;
		}
		/* create crypto session */
		rte_cryptodev_sym_session_init(dev_id, sess, &cipher_xform,
				sess_mp);
	/*
	 *  auth only
	 */
	} else if (options->op_type == CPERF_AUTH_ONLY) {
		auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
		auth_xform.next = NULL;
		auth_xform.auth.algo = options->auth_algo;
		auth_xform.auth.op = options->auth_op;

		/* auth different than null */
		if (options->auth_algo != RTE_CRYPTO_AUTH_NULL) {
			auth_xform.auth.digest_length =
					options->digest_sz;
			auth_xform.auth.key.length =
					test_vector->auth_key.length;
			auth_xform.auth.key.data = test_vector->auth_key.data;
			auth_xform.auth.iv.length =
					test_vector->auth_iv.length;
		} else {
			auth_xform.auth.digest_length = 0;
			auth_xform.auth.key.length = 0;
			auth_xform.auth.key.data = NULL;
			auth_xform.auth.iv.length = 0;
		}
		/* create crypto session */
		rte_cryptodev_sym_session_init(dev_id, sess, &auth_xform,
				sess_mp);
	/*
	 * cipher and auth
	 */
	} else if (options->op_type == CPERF_CIPHER_THEN_AUTH
			|| options->op_type == CPERF_AUTH_THEN_CIPHER) {
		/*
		 * cipher
		 */
		cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
		cipher_xform.next = NULL;
		cipher_xform.cipher.algo = options->cipher_algo;
		cipher_xform.cipher.op = options->cipher_op;
		cipher_xform.cipher.iv.offset = iv_offset;

		/* cipher different than null */
		if (options->cipher_algo != RTE_CRYPTO_CIPHER_NULL) {
			cipher_xform.cipher.key.data =
					test_vector->cipher_key.data;
			cipher_xform.cipher.key.length =
					test_vector->cipher_key.length;
			cipher_xform.cipher.iv.length =
					test_vector->cipher_iv.length;
		} else {
			cipher_xform.cipher.key.data = NULL;
			cipher_xform.cipher.key.length = 0;
			cipher_xform.cipher.iv.length = 0;
		}

		/*
		 * auth
		 */
		auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
		auth_xform.next = NULL;
		auth_xform.auth.algo = options->auth_algo;
		auth_xform.auth.op = options->auth_op;

		/* auth different than null */
		if (options->auth_algo != RTE_CRYPTO_AUTH_NULL) {
			auth_xform.auth.digest_length = options->digest_sz;
			auth_xform.auth.iv.length = test_vector->auth_iv.length;
			auth_xform.auth.key.length =
					test_vector->auth_key.length;
			auth_xform.auth.key.data =
					test_vector->auth_key.data;
		} else {
			auth_xform.auth.digest_length = 0;
			auth_xform.auth.key.length = 0;
			auth_xform.auth.key.data = NULL;
			auth_xform.auth.iv.length = 0;
		}

		/* cipher then auth */
		if (options->op_type == CPERF_CIPHER_THEN_AUTH) {
			cipher_xform.next = &auth_xform;
			/* create crypto session */
			rte_cryptodev_sym_session_init(dev_id,
					sess, &cipher_xform, sess_mp);
		} else { /* auth then cipher */
			auth_xform.next = &cipher_xform;
			/* create crypto session */
			rte_cryptodev_sym_session_init(dev_id,
					sess, &auth_xform, sess_mp);
		}
	} else { /* options->op_type == CPERF_AEAD */
		aead_xform.type = RTE_CRYPTO_SYM_XFORM_AEAD;
		aead_xform.next = NULL;
		aead_xform.aead.algo = options->aead_algo;
		aead_xform.aead.op = options->aead_op;
		aead_xform.aead.iv.offset = iv_offset;

		aead_xform.aead.key.data =
					test_vector->aead_key.data;
		aead_xform.aead.key.length =
					test_vector->aead_key.length;
		aead_xform.aead.iv.length = test_vector->aead_iv.length;

		aead_xform.aead.digest_length = options->digest_sz;
		aead_xform.aead.aad_length =
					options->aead_aad_sz;

		/* Create crypto session */
		rte_cryptodev_sym_session_init(dev_id,
					sess, &aead_xform, sess_mp);
	}

	return sess;
}

int
cperf_get_op_functions(const struct cperf_options *options,
		struct cperf_op_fns *op_fns)
{
	memset(op_fns, 0, sizeof(struct cperf_op_fns));

	op_fns->sess_create = cperf_create_session;

	if (options->op_type == CPERF_AEAD) {
		op_fns->populate_ops = cperf_set_ops_aead;
		return 0;
	}

	if (options->op_type == CPERF_AUTH_THEN_CIPHER
			|| options->op_type == CPERF_CIPHER_THEN_AUTH) {
		op_fns->populate_ops = cperf_set_ops_cipher_auth;
		return 0;
	}
	if (options->op_type == CPERF_AUTH_ONLY) {
		if (options->auth_algo == RTE_CRYPTO_AUTH_NULL)
			op_fns->populate_ops = cperf_set_ops_null_auth;
		else
			op_fns->populate_ops = cperf_set_ops_auth;
		return 0;
	}
	if (options->op_type == CPERF_CIPHER_ONLY) {
		if (options->cipher_algo == RTE_CRYPTO_CIPHER_NULL)
			op_fns->populate_ops = cperf_set_ops_null_cipher;
		else
			op_fns->populate_ops = cperf_set_ops_cipher;
		return 0;
	}

	return -1;
}
