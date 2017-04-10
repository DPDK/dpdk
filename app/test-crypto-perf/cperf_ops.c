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
		const struct cperf_test_vector *test_vector __rte_unused)
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
		const struct cperf_test_vector *test_vector __rte_unused)
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
		const struct cperf_test_vector *test_vector)
{
	uint16_t i;

	for (i = 0; i < nb_ops; i++) {
		struct rte_crypto_sym_op *sym_op = ops[i]->sym;

		rte_crypto_op_attach_sym_session(ops[i], sess);

		sym_op->m_src = bufs_in[i];
		sym_op->m_dst = bufs_out[i];

		/* cipher parameters */
		sym_op->cipher.iv.data = test_vector->iv.data;
		sym_op->cipher.iv.phys_addr = test_vector->iv.phys_addr;
		sym_op->cipher.iv.length = test_vector->iv.length;

		if (options->cipher_algo == RTE_CRYPTO_CIPHER_SNOW3G_UEA2 ||
				options->cipher_algo == RTE_CRYPTO_CIPHER_KASUMI_F8 ||
				options->cipher_algo == RTE_CRYPTO_CIPHER_ZUC_EEA3)
			sym_op->cipher.data.length = options->test_buffer_size << 3;
		else
			sym_op->cipher.data.length = options->test_buffer_size;

		sym_op->cipher.data.offset = 0;
	}

	return 0;
}

static int
cperf_set_ops_auth(struct rte_crypto_op **ops,
		struct rte_mbuf **bufs_in, struct rte_mbuf **bufs_out,
		uint16_t nb_ops, struct rte_cryptodev_sym_session *sess,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector)
{
	uint16_t i;

	for (i = 0; i < nb_ops; i++) {
		struct rte_crypto_sym_op *sym_op = ops[i]->sym;

		rte_crypto_op_attach_sym_session(ops[i], sess);

		sym_op->m_src = bufs_in[i];
		sym_op->m_dst = bufs_out[i];

		/* authentication parameters */
		if (options->auth_op == RTE_CRYPTO_AUTH_OP_VERIFY) {
			sym_op->auth.digest.data = test_vector->digest.data;
			sym_op->auth.digest.phys_addr =
					test_vector->digest.phys_addr;
			sym_op->auth.digest.length = options->auth_digest_sz;
		} else {

			uint32_t offset = options->test_buffer_size;
			struct rte_mbuf *buf, *tbuf;

			if (options->out_of_place) {
				buf =  bufs_out[i];
			} else {
				buf =  bufs_in[i];

				tbuf = buf;
				while ((tbuf->next != NULL) &&
						(offset >= tbuf->data_len)) {
					offset -= tbuf->data_len;
					tbuf = tbuf->next;
				}
			}

			sym_op->auth.digest.data = rte_pktmbuf_mtod_offset(buf,
					uint8_t *, offset);
			sym_op->auth.digest.phys_addr =
					rte_pktmbuf_mtophys_offset(buf,	offset);
			sym_op->auth.digest.length = options->auth_digest_sz;
			sym_op->auth.aad.phys_addr = test_vector->aad.phys_addr;
			sym_op->auth.aad.data = test_vector->aad.data;
			sym_op->auth.aad.length = options->auth_aad_sz;

		}

		if (options->auth_algo == RTE_CRYPTO_AUTH_SNOW3G_UIA2 ||
				options->auth_algo == RTE_CRYPTO_AUTH_KASUMI_F9 ||
				options->auth_algo == RTE_CRYPTO_AUTH_ZUC_EIA3)
			sym_op->auth.data.length = options->test_buffer_size << 3;
		else
			sym_op->auth.data.length = options->test_buffer_size;

		sym_op->auth.data.offset = 0;
	}

	return 0;
}

static int
cperf_set_ops_cipher_auth(struct rte_crypto_op **ops,
		struct rte_mbuf **bufs_in, struct rte_mbuf **bufs_out,
		uint16_t nb_ops, struct rte_cryptodev_sym_session *sess,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector)
{
	uint16_t i;

	for (i = 0; i < nb_ops; i++) {
		struct rte_crypto_sym_op *sym_op = ops[i]->sym;

		rte_crypto_op_attach_sym_session(ops[i], sess);

		sym_op->m_src = bufs_in[i];
		sym_op->m_dst = bufs_out[i];

		/* cipher parameters */
		sym_op->cipher.iv.data = test_vector->iv.data;
		sym_op->cipher.iv.phys_addr = test_vector->iv.phys_addr;
		sym_op->cipher.iv.length = test_vector->iv.length;

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
			sym_op->auth.digest.length = options->auth_digest_sz;
		} else {

			uint32_t offset = options->test_buffer_size;
			struct rte_mbuf *buf, *tbuf;

			if (options->out_of_place) {
				buf =  bufs_out[i];
			} else {
				buf =  bufs_in[i];

				tbuf = buf;
				while ((tbuf->next != NULL) &&
						(offset >= tbuf->data_len)) {
					offset -= tbuf->data_len;
					tbuf = tbuf->next;
				}
			}

			sym_op->auth.digest.data = rte_pktmbuf_mtod_offset(buf,
					uint8_t *, offset);
			sym_op->auth.digest.phys_addr =
					rte_pktmbuf_mtophys_offset(buf,	offset);
			sym_op->auth.digest.length = options->auth_digest_sz;
			sym_op->auth.aad.phys_addr = test_vector->aad.phys_addr;
			sym_op->auth.aad.data = test_vector->aad.data;
			sym_op->auth.aad.length = options->auth_aad_sz;
		}

		if (options->auth_algo == RTE_CRYPTO_AUTH_SNOW3G_UIA2 ||
				options->auth_algo == RTE_CRYPTO_AUTH_KASUMI_F9 ||
				options->auth_algo == RTE_CRYPTO_AUTH_ZUC_EIA3)
			sym_op->auth.data.length = options->test_buffer_size << 3;
		else
			sym_op->auth.data.length = options->test_buffer_size;

		sym_op->auth.data.offset = 0;
	}

	return 0;
}

static int
cperf_set_ops_aead(struct rte_crypto_op **ops,
		struct rte_mbuf **bufs_in, struct rte_mbuf **bufs_out,
		uint16_t nb_ops, struct rte_cryptodev_sym_session *sess,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector)
{
	uint16_t i;

	for (i = 0; i < nb_ops; i++) {
		struct rte_crypto_sym_op *sym_op = ops[i]->sym;

		rte_crypto_op_attach_sym_session(ops[i], sess);

		sym_op->m_src = bufs_in[i];
		sym_op->m_dst = bufs_out[i];

		/* cipher parameters */
		sym_op->cipher.iv.data = test_vector->iv.data;
		sym_op->cipher.iv.phys_addr = test_vector->iv.phys_addr;
		sym_op->cipher.iv.length = test_vector->iv.length;

		sym_op->cipher.data.length = options->test_buffer_size;
		sym_op->cipher.data.offset =
				RTE_ALIGN_CEIL(options->auth_aad_sz, 16);

		sym_op->auth.aad.data = rte_pktmbuf_mtod(bufs_in[i], uint8_t *);
		sym_op->auth.aad.phys_addr = rte_pktmbuf_mtophys(bufs_in[i]);
		sym_op->auth.aad.length = options->auth_aad_sz;

		/* authentication parameters */
		if (options->auth_op == RTE_CRYPTO_AUTH_OP_VERIFY) {
			sym_op->auth.digest.data = test_vector->digest.data;
			sym_op->auth.digest.phys_addr =
					test_vector->digest.phys_addr;
			sym_op->auth.digest.length = options->auth_digest_sz;
		} else {

			uint32_t offset = sym_op->cipher.data.length +
						sym_op->cipher.data.offset;
			struct rte_mbuf *buf, *tbuf;

			if (options->out_of_place) {
				buf =  bufs_out[i];
			} else {
				buf =  bufs_in[i];

				tbuf = buf;
				while ((tbuf->next != NULL) &&
						(offset >= tbuf->data_len)) {
					offset -= tbuf->data_len;
					tbuf = tbuf->next;
				}
			}

			sym_op->auth.digest.data = rte_pktmbuf_mtod_offset(buf,
					uint8_t *, offset);
			sym_op->auth.digest.phys_addr =
					rte_pktmbuf_mtophys_offset(buf,	offset);

			sym_op->auth.digest.length = options->auth_digest_sz;
		}

		sym_op->auth.data.length = options->test_buffer_size;
		sym_op->auth.data.offset = options->auth_aad_sz;
	}

	return 0;
}

static struct rte_cryptodev_sym_session *
cperf_create_session(uint8_t dev_id,
	const struct cperf_options *options,
	const struct cperf_test_vector *test_vector)
{
	struct rte_crypto_sym_xform cipher_xform;
	struct rte_crypto_sym_xform auth_xform;
	struct rte_cryptodev_sym_session *sess = NULL;

	/*
	 * cipher only
	 */
	if (options->op_type == CPERF_CIPHER_ONLY) {
		cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
		cipher_xform.next = NULL;
		cipher_xform.cipher.algo = options->cipher_algo;
		cipher_xform.cipher.op = options->cipher_op;

		/* cipher different than null */
		if (options->cipher_algo != RTE_CRYPTO_CIPHER_NULL) {
			cipher_xform.cipher.key.data =
					test_vector->cipher_key.data;
			cipher_xform.cipher.key.length =
					test_vector->cipher_key.length;
		} else {
			cipher_xform.cipher.key.data = NULL;
			cipher_xform.cipher.key.length = 0;
		}
		/* create crypto session */
		sess = rte_cryptodev_sym_session_create(dev_id,	&cipher_xform);
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
					options->auth_digest_sz;
			auth_xform.auth.add_auth_data_length =
					options->auth_aad_sz;
			auth_xform.auth.key.length =
					test_vector->auth_key.length;
			auth_xform.auth.key.data = test_vector->auth_key.data;
		} else {
			auth_xform.auth.digest_length = 0;
			auth_xform.auth.add_auth_data_length = 0;
			auth_xform.auth.key.length = 0;
			auth_xform.auth.key.data = NULL;
		}
		/* create crypto session */
		sess =  rte_cryptodev_sym_session_create(dev_id, &auth_xform);
	/*
	 * cipher and auth
	 */
	} else if (options->op_type == CPERF_CIPHER_THEN_AUTH
			|| options->op_type == CPERF_AUTH_THEN_CIPHER
			|| options->op_type == CPERF_AEAD) {

		/*
		 * cipher
		 */
		cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
		cipher_xform.next = NULL;
		cipher_xform.cipher.algo = options->cipher_algo;
		cipher_xform.cipher.op = options->cipher_op;

		/* cipher different than null */
		if (options->cipher_algo != RTE_CRYPTO_CIPHER_NULL) {
			cipher_xform.cipher.key.data =
					test_vector->cipher_key.data;
			cipher_xform.cipher.key.length =
					test_vector->cipher_key.length;
		} else {
			cipher_xform.cipher.key.data = NULL;
			cipher_xform.cipher.key.length = 0;
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
			auth_xform.auth.digest_length = options->auth_digest_sz;
			auth_xform.auth.add_auth_data_length =
					options->auth_aad_sz;
			/* auth options for aes gcm */
			if (options->cipher_algo == RTE_CRYPTO_CIPHER_AES_GCM &&
				options->auth_algo == RTE_CRYPTO_AUTH_AES_GCM) {
				auth_xform.auth.key.length = 0;
				auth_xform.auth.key.data = NULL;
			} else { /* auth options for others */
				auth_xform.auth.key.length =
					test_vector->auth_key.length;
				auth_xform.auth.key.data =
						test_vector->auth_key.data;
			}
		} else {
			auth_xform.auth.digest_length = 0;
			auth_xform.auth.add_auth_data_length = 0;
			auth_xform.auth.key.length = 0;
			auth_xform.auth.key.data = NULL;
		}

		/* create crypto session for aes gcm */
		if (options->cipher_algo == RTE_CRYPTO_CIPHER_AES_GCM) {
			if (options->cipher_op ==
					RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
				cipher_xform.next = &auth_xform;
				/* create crypto session */
				sess = rte_cryptodev_sym_session_create(dev_id,
					&cipher_xform);
			} else { /* decrypt */
				auth_xform.next = &cipher_xform;
				/* create crypto session */
				sess = rte_cryptodev_sym_session_create(dev_id,
					&auth_xform);
			}
		} else { /* create crypto session for other */
			/* cipher then auth */
			if (options->op_type == CPERF_CIPHER_THEN_AUTH) {
				cipher_xform.next = &auth_xform;
				/* create crypto session */
				sess = rte_cryptodev_sym_session_create(dev_id,
						&cipher_xform);
			} else { /* auth then cipher */
				auth_xform.next = &cipher_xform;
				/* create crypto session */
				sess = rte_cryptodev_sym_session_create(dev_id,
						&auth_xform);
			}
		}
	}
	return sess;
}

int
cperf_get_op_functions(const struct cperf_options *options,
		struct cperf_op_fns *op_fns)
{
	memset(op_fns, 0, sizeof(struct cperf_op_fns));

	op_fns->sess_create = cperf_create_session;

	if (options->op_type == CPERF_AEAD
			|| options->op_type == CPERF_AUTH_THEN_CIPHER
			|| options->op_type == CPERF_CIPHER_THEN_AUTH) {
		if (options->cipher_algo == RTE_CRYPTO_CIPHER_AES_GCM &&
				options->auth_algo == RTE_CRYPTO_AUTH_AES_GCM)
			op_fns->populate_ops = cperf_set_ops_aead;
		else
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
