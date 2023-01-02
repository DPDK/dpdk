/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <rte_cryptodev.h>
#include <rte_ether.h>
#include <rte_ip.h>

#include "cperf_ops.h"
#include "cperf_test_vectors.h"

static void
cperf_set_ops_asym(struct rte_crypto_op **ops,
		   uint32_t src_buf_offset __rte_unused,
		   uint32_t dst_buf_offset __rte_unused, uint16_t nb_ops,
		   void *sess,
		   const struct cperf_options *options,
		   const struct cperf_test_vector *test_vector __rte_unused,
		   uint16_t iv_offset __rte_unused,
		   uint32_t *imix_idx __rte_unused,
		   uint64_t *tsc_start __rte_unused)
{
	uint16_t i;
	void *asym_sess = (void *)sess;

	for (i = 0; i < nb_ops; i++) {
		struct rte_crypto_asym_op *asym_op = ops[i]->asym;

		ops[i]->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
		asym_op->modex.base.data = options->modex_data->base.data;
		asym_op->modex.base.length = options->modex_data->base.len;
		asym_op->modex.result.data = options->modex_data->result.data;
		asym_op->modex.result.length = options->modex_data->result.len;
		rte_crypto_op_attach_asym_session(ops[i], asym_sess);
	}
}

#ifdef RTE_LIB_SECURITY
static void
test_ipsec_vec_populate(struct rte_mbuf *m, const struct cperf_options *options,
			const struct cperf_test_vector *test_vector)
{
	struct rte_ipv4_hdr *ip = rte_pktmbuf_mtod(m, struct rte_ipv4_hdr *);

	if (options->is_outbound) {
		memcpy(ip, test_vector->plaintext.data,
		       sizeof(struct rte_ipv4_hdr));

		ip->total_length = rte_cpu_to_be_16(m->data_len);
	}
}

static void
cperf_set_ops_security(struct rte_crypto_op **ops,
		uint32_t src_buf_offset __rte_unused,
		uint32_t dst_buf_offset __rte_unused,
		uint16_t nb_ops, void *sess,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector,
		uint16_t iv_offset __rte_unused, uint32_t *imix_idx,
		uint64_t *tsc_start)
{
	uint16_t i;

	for (i = 0; i < nb_ops; i++) {
		struct rte_crypto_sym_op *sym_op = ops[i]->sym;
		void *sec_sess = (void *)sess;
		uint32_t buf_sz;

		uint32_t *per_pkt_hfn = rte_crypto_op_ctod_offset(ops[i],
					uint32_t *, iv_offset);
		*per_pkt_hfn = options->pdcp_ses_hfn_en ? 0 : PDCP_DEFAULT_HFN;

		ops[i]->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
		rte_security_attach_session(ops[i], sec_sess);
		sym_op->m_src = (struct rte_mbuf *)((uint8_t *)ops[i] +
							src_buf_offset);

		if (options->op_type == CPERF_PDCP) {
			sym_op->m_src->buf_len = options->segment_sz;
			sym_op->m_src->data_len = options->test_buffer_size;
			sym_op->m_src->pkt_len = sym_op->m_src->data_len;
		}

		if (options->op_type == CPERF_DOCSIS) {
			if (options->imix_distribution_count) {
				buf_sz = options->imix_buffer_sizes[*imix_idx];
				*imix_idx = (*imix_idx + 1) % options->pool_sz;
			} else
				buf_sz = options->test_buffer_size;

			sym_op->m_src->buf_len = options->segment_sz;
			sym_op->m_src->data_len = buf_sz;
			sym_op->m_src->pkt_len = buf_sz;

			/* DOCSIS header is not CRC'ed */
			sym_op->auth.data.offset = options->docsis_hdr_sz;
			sym_op->auth.data.length = buf_sz -
				sym_op->auth.data.offset - RTE_ETHER_CRC_LEN;
			/*
			 * DOCSIS header and SRC and DST MAC addresses are not
			 * ciphered
			 */
			sym_op->cipher.data.offset = sym_op->auth.data.offset +
				RTE_ETHER_HDR_LEN - RTE_ETHER_TYPE_LEN;
			sym_op->cipher.data.length = buf_sz -
				sym_op->cipher.data.offset;
		}

		/* Set dest mbuf to NULL if out-of-place (dst_buf_offset = 0) */
		if (dst_buf_offset == 0)
			sym_op->m_dst = NULL;
		else
			sym_op->m_dst = (struct rte_mbuf *)((uint8_t *)ops[i] +
							dst_buf_offset);
	}

	RTE_SET_USED(tsc_start);
	RTE_SET_USED(test_vector);
}

static void
cperf_set_ops_security_ipsec(struct rte_crypto_op **ops,
		uint32_t src_buf_offset __rte_unused,
		uint32_t dst_buf_offset __rte_unused,
		uint16_t nb_ops, void *sess,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector,
		uint16_t iv_offset __rte_unused, uint32_t *imix_idx,
		uint64_t *tsc_start)
{
	void *sec_sess = sess;
	const uint32_t test_buffer_size = options->test_buffer_size;
	const uint32_t headroom_sz = options->headroom_sz;
	const uint32_t segment_sz = options->segment_sz;
	uint64_t tsc_start_temp, tsc_end_temp;
	uint16_t i = 0;

	RTE_SET_USED(imix_idx);

	for (i = 0; i < nb_ops; i++) {
		struct rte_crypto_sym_op *sym_op = ops[i]->sym;
		struct rte_mbuf *m = sym_op->m_src;

		ops[i]->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
		rte_security_attach_session(ops[i], sec_sess);
		sym_op->m_src = (struct rte_mbuf *)((uint8_t *)ops[i] +
							src_buf_offset);

		/* In case of IPsec, headroom is consumed by PMD,
		 * hence resetting it.
		 */
		m->data_off = headroom_sz;

		m->buf_len = segment_sz;
		m->data_len = test_buffer_size;
		m->pkt_len = test_buffer_size;

		sym_op->m_dst = NULL;
	}

	if (options->test_file != NULL)
		return;

	tsc_start_temp = rte_rdtsc_precise();

	for (i = 0; i < nb_ops; i++) {
		struct rte_crypto_sym_op *sym_op = ops[i]->sym;
		struct rte_mbuf *m = sym_op->m_src;

		test_ipsec_vec_populate(m, options, test_vector);
	}

	tsc_end_temp = rte_rdtsc_precise();
	*tsc_start += tsc_end_temp - tsc_start_temp;
}

#endif

static void
cperf_set_ops_null_cipher(struct rte_crypto_op **ops,
		uint32_t src_buf_offset, uint32_t dst_buf_offset,
		uint16_t nb_ops, void *sess,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector __rte_unused,
		uint16_t iv_offset __rte_unused, uint32_t *imix_idx,
		uint64_t *tsc_start __rte_unused)
{
	uint16_t i;

	for (i = 0; i < nb_ops; i++) {
		struct rte_crypto_sym_op *sym_op = ops[i]->sym;

		ops[i]->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
		rte_crypto_op_attach_sym_session(ops[i], sess);

		sym_op->m_src = (struct rte_mbuf *)((uint8_t *)ops[i] +
							src_buf_offset);

		/* Set dest mbuf to NULL if out-of-place (dst_buf_offset = 0) */
		if (dst_buf_offset == 0)
			sym_op->m_dst = NULL;
		else
			sym_op->m_dst = (struct rte_mbuf *)((uint8_t *)ops[i] +
							dst_buf_offset);

		/* cipher parameters */
		if (options->imix_distribution_count) {
			sym_op->cipher.data.length =
				options->imix_buffer_sizes[*imix_idx];
			*imix_idx = (*imix_idx + 1) % options->pool_sz;
		} else
			sym_op->cipher.data.length = options->test_buffer_size;
		sym_op->cipher.data.offset = 0;
	}
}

static void
cperf_set_ops_null_auth(struct rte_crypto_op **ops,
		uint32_t src_buf_offset, uint32_t dst_buf_offset,
		uint16_t nb_ops, void *sess,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector __rte_unused,
		uint16_t iv_offset __rte_unused, uint32_t *imix_idx,
		uint64_t *tsc_start __rte_unused)
{
	uint16_t i;

	for (i = 0; i < nb_ops; i++) {
		struct rte_crypto_sym_op *sym_op = ops[i]->sym;

		ops[i]->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
		rte_crypto_op_attach_sym_session(ops[i], sess);

		sym_op->m_src = (struct rte_mbuf *)((uint8_t *)ops[i] +
							src_buf_offset);

		/* Set dest mbuf to NULL if out-of-place (dst_buf_offset = 0) */
		if (dst_buf_offset == 0)
			sym_op->m_dst = NULL;
		else
			sym_op->m_dst = (struct rte_mbuf *)((uint8_t *)ops[i] +
							dst_buf_offset);

		/* auth parameters */
		if (options->imix_distribution_count) {
			sym_op->auth.data.length =
				options->imix_buffer_sizes[*imix_idx];
			*imix_idx = (*imix_idx + 1) % options->pool_sz;
		} else
			sym_op->auth.data.length = options->test_buffer_size;
		sym_op->auth.data.offset = 0;
	}
}

static void
cperf_set_ops_cipher(struct rte_crypto_op **ops,
		uint32_t src_buf_offset, uint32_t dst_buf_offset,
		uint16_t nb_ops, void *sess,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector,
		uint16_t iv_offset, uint32_t *imix_idx,
		uint64_t *tsc_start __rte_unused)
{
	uint16_t i;

	for (i = 0; i < nb_ops; i++) {
		struct rte_crypto_sym_op *sym_op = ops[i]->sym;

		ops[i]->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
		rte_crypto_op_attach_sym_session(ops[i], sess);

		sym_op->m_src = (struct rte_mbuf *)((uint8_t *)ops[i] +
							src_buf_offset);

		/* Set dest mbuf to NULL if out-of-place (dst_buf_offset = 0) */
		if (dst_buf_offset == 0)
			sym_op->m_dst = NULL;
		else
			sym_op->m_dst = (struct rte_mbuf *)((uint8_t *)ops[i] +
							dst_buf_offset);

		/* cipher parameters */
		if (options->imix_distribution_count) {
			sym_op->cipher.data.length =
				options->imix_buffer_sizes[*imix_idx];
			*imix_idx = (*imix_idx + 1) % options->pool_sz;
		} else
			sym_op->cipher.data.length = options->test_buffer_size;

		if (options->cipher_algo == RTE_CRYPTO_CIPHER_SNOW3G_UEA2 ||
				options->cipher_algo == RTE_CRYPTO_CIPHER_KASUMI_F8 ||
				options->cipher_algo == RTE_CRYPTO_CIPHER_ZUC_EEA3)
			sym_op->cipher.data.length <<= 3;

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
}

static void
cperf_set_ops_auth(struct rte_crypto_op **ops,
		uint32_t src_buf_offset, uint32_t dst_buf_offset,
		uint16_t nb_ops, void *sess,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector,
		uint16_t iv_offset, uint32_t *imix_idx,
		uint64_t *tsc_start __rte_unused)
{
	uint16_t i;

	for (i = 0; i < nb_ops; i++) {
		struct rte_crypto_sym_op *sym_op = ops[i]->sym;

		ops[i]->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
		rte_crypto_op_attach_sym_session(ops[i], sess);

		sym_op->m_src = (struct rte_mbuf *)((uint8_t *)ops[i] +
							src_buf_offset);

		/* Set dest mbuf to NULL if out-of-place (dst_buf_offset = 0) */
		if (dst_buf_offset == 0)
			sym_op->m_dst = NULL;
		else
			sym_op->m_dst = (struct rte_mbuf *)((uint8_t *)ops[i] +
							dst_buf_offset);

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
				buf = sym_op->m_dst;
			} else {
				tbuf = sym_op->m_src;
				while ((tbuf->next != NULL) &&
						(offset >= tbuf->data_len)) {
					offset -= tbuf->data_len;
					tbuf = tbuf->next;
				}
				/*
				 * If there is not enough room in segment,
				 * place the digest in the next segment
				 */
				if ((tbuf->data_len - offset) < options->digest_sz) {
					tbuf = tbuf->next;
					offset = 0;
				}
				buf = tbuf;
			}

			sym_op->auth.digest.data = rte_pktmbuf_mtod_offset(buf,
					uint8_t *, offset);
			sym_op->auth.digest.phys_addr =
					rte_pktmbuf_iova_offset(buf, offset);

		}

		if (options->imix_distribution_count) {
			sym_op->auth.data.length =
				options->imix_buffer_sizes[*imix_idx];
			*imix_idx = (*imix_idx + 1) % options->pool_sz;
		} else
			sym_op->auth.data.length = options->test_buffer_size;

		if (options->auth_algo == RTE_CRYPTO_AUTH_SNOW3G_UIA2 ||
				options->auth_algo == RTE_CRYPTO_AUTH_KASUMI_F9 ||
				options->auth_algo == RTE_CRYPTO_AUTH_ZUC_EIA3)
			sym_op->auth.data.length <<= 3;

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
}

static void
cperf_set_ops_cipher_auth(struct rte_crypto_op **ops,
		uint32_t src_buf_offset, uint32_t dst_buf_offset,
		uint16_t nb_ops, void *sess,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector,
		uint16_t iv_offset, uint32_t *imix_idx,
		uint64_t *tsc_start __rte_unused)
{
	uint16_t i;

	for (i = 0; i < nb_ops; i++) {
		struct rte_crypto_sym_op *sym_op = ops[i]->sym;

		ops[i]->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
		rte_crypto_op_attach_sym_session(ops[i], sess);

		sym_op->m_src = (struct rte_mbuf *)((uint8_t *)ops[i] +
							src_buf_offset);

		/* Set dest mbuf to NULL if out-of-place (dst_buf_offset = 0) */
		if (dst_buf_offset == 0)
			sym_op->m_dst = NULL;
		else
			sym_op->m_dst = (struct rte_mbuf *)((uint8_t *)ops[i] +
							dst_buf_offset);

		/* cipher parameters */
		if (options->imix_distribution_count) {
			sym_op->cipher.data.length =
				options->imix_buffer_sizes[*imix_idx];
			*imix_idx = (*imix_idx + 1) % options->pool_sz;
		} else
			sym_op->cipher.data.length = options->test_buffer_size;

		if ((options->auth_op == RTE_CRYPTO_AUTH_OP_GENERATE) &&
				(options->op_type == CPERF_AUTH_THEN_CIPHER))
			sym_op->cipher.data.length += options->digest_sz;

		if (options->cipher_algo == RTE_CRYPTO_CIPHER_SNOW3G_UEA2 ||
				options->cipher_algo == RTE_CRYPTO_CIPHER_KASUMI_F8 ||
				options->cipher_algo == RTE_CRYPTO_CIPHER_ZUC_EEA3)
			sym_op->cipher.data.length <<= 3;

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
				buf = sym_op->m_dst;
			} else {
				tbuf = sym_op->m_src;
				while ((tbuf->next != NULL) &&
						(offset >= tbuf->data_len)) {
					offset -= tbuf->data_len;
					tbuf = tbuf->next;
				}
				/*
				 * If there is not enough room in segment,
				 * place the digest in the next segment
				 */
				if ((tbuf->data_len - offset) < options->digest_sz) {
					tbuf = tbuf->next;
					offset = 0;
				}
				buf = tbuf;
			}

			sym_op->auth.digest.data = rte_pktmbuf_mtod_offset(buf,
					uint8_t *, offset);
			sym_op->auth.digest.phys_addr =
					rte_pktmbuf_iova_offset(buf, offset);
		}

		if (options->imix_distribution_count) {
			sym_op->auth.data.length =
				options->imix_buffer_sizes[*imix_idx];
			*imix_idx = (*imix_idx + 1) % options->pool_sz;
		} else
			sym_op->auth.data.length = options->test_buffer_size;

		if (options->auth_algo == RTE_CRYPTO_AUTH_SNOW3G_UIA2 ||
				options->auth_algo == RTE_CRYPTO_AUTH_KASUMI_F9 ||
				options->auth_algo == RTE_CRYPTO_AUTH_ZUC_EIA3)
			sym_op->auth.data.length <<= 3;

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
}

static void
cperf_set_ops_aead(struct rte_crypto_op **ops,
		uint32_t src_buf_offset, uint32_t dst_buf_offset,
		uint16_t nb_ops, void *sess,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector,
		uint16_t iv_offset, uint32_t *imix_idx,
		uint64_t *tsc_start __rte_unused)
{
	uint16_t i;
	/* AAD is placed after the IV */
	uint16_t aad_offset = iv_offset +
			RTE_ALIGN_CEIL(test_vector->aead_iv.length, 16);

	for (i = 0; i < nb_ops; i++) {
		struct rte_crypto_sym_op *sym_op = ops[i]->sym;

		ops[i]->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
		rte_crypto_op_attach_sym_session(ops[i], sess);

		sym_op->m_src = (struct rte_mbuf *)((uint8_t *)ops[i] +
							src_buf_offset);

		/* Set dest mbuf to NULL if out-of-place (dst_buf_offset = 0) */
		if (dst_buf_offset == 0)
			sym_op->m_dst = NULL;
		else
			sym_op->m_dst = (struct rte_mbuf *)((uint8_t *)ops[i] +
							dst_buf_offset);

		/* AEAD parameters */
		if (options->imix_distribution_count) {
			sym_op->aead.data.length =
				options->imix_buffer_sizes[*imix_idx];
			*imix_idx = (*imix_idx + 1) % options->pool_sz;
		} else
			sym_op->aead.data.length = options->test_buffer_size;
		sym_op->aead.data.offset = 0;

		sym_op->aead.aad.data = rte_crypto_op_ctod_offset(ops[i],
					uint8_t *, aad_offset);
		sym_op->aead.aad.phys_addr = rte_crypto_op_ctophys_offset(ops[i],
					aad_offset);

		if (options->aead_op == RTE_CRYPTO_AEAD_OP_DECRYPT) {
			sym_op->aead.digest.data = test_vector->digest.data;
			sym_op->aead.digest.phys_addr =
					test_vector->digest.phys_addr;
		} else {

			uint32_t offset = sym_op->aead.data.length +
						sym_op->aead.data.offset;
			struct rte_mbuf *buf, *tbuf;

			if (options->out_of_place) {
				buf = sym_op->m_dst;
			} else {
				tbuf = sym_op->m_src;
				while ((tbuf->next != NULL) &&
						(offset >= tbuf->data_len)) {
					offset -= tbuf->data_len;
					tbuf = tbuf->next;
				}
				/*
				 * If there is not enough room in segment,
				 * place the digest in the next segment
				 */
				if ((tbuf->data_len - offset) < options->digest_sz) {
					tbuf = tbuf->next;
					offset = 0;
				}
				buf = tbuf;
			}

			sym_op->aead.digest.data = rte_pktmbuf_mtod_offset(buf,
					uint8_t *, offset);
			sym_op->aead.digest.phys_addr =
					rte_pktmbuf_iova_offset(buf, offset);
		}
	}

	if ((options->test == CPERF_TEST_TYPE_VERIFY) ||
			(options->test == CPERF_TEST_TYPE_LATENCY)) {
		for (i = 0; i < nb_ops; i++) {
			uint8_t *iv_ptr = rte_crypto_op_ctod_offset(ops[i],
					uint8_t *, iv_offset);

			/*
			 * If doing AES-CCM, nonce is copied one byte
			 * after the start of IV field, and AAD is copied
			 * 18 bytes after the start of the AAD field.
			 */
			if (options->aead_algo == RTE_CRYPTO_AEAD_AES_CCM) {
				memcpy(iv_ptr + 1, test_vector->aead_iv.data,
					test_vector->aead_iv.length);

				memcpy(ops[i]->sym->aead.aad.data + 18,
					test_vector->aad.data,
					test_vector->aad.length);
			} else {
				memcpy(iv_ptr, test_vector->aead_iv.data,
					test_vector->aead_iv.length);

				memcpy(ops[i]->sym->aead.aad.data,
					test_vector->aad.data,
					test_vector->aad.length);
			}
		}
	}
}

static void *
create_ipsec_session(struct rte_mempool *sess_mp,
		uint8_t dev_id,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector,
		uint16_t iv_offset)
{
	struct rte_crypto_sym_xform auth_xform = {0};
	struct rte_crypto_sym_xform *crypto_xform;
	struct rte_crypto_sym_xform xform = {0};

	if (options->aead_algo != 0) {
		/* Setup AEAD Parameters */
		xform.type = RTE_CRYPTO_SYM_XFORM_AEAD;
		xform.next = NULL;
		xform.aead.algo = options->aead_algo;
		xform.aead.op = options->aead_op;
		xform.aead.iv.offset = iv_offset;
		xform.aead.key.data = test_vector->aead_key.data;
		xform.aead.key.length = test_vector->aead_key.length;
		xform.aead.iv.length = test_vector->aead_iv.length;
		xform.aead.digest_length = options->digest_sz;
		xform.aead.aad_length = options->aead_aad_sz;
		crypto_xform = &xform;
	} else if (options->cipher_algo != 0 && options->auth_algo != 0) {
		/* Setup Cipher Parameters */
		xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
		xform.cipher.algo = options->cipher_algo;
		xform.cipher.op = options->cipher_op;
		xform.cipher.iv.offset = iv_offset;
		xform.cipher.iv.length = test_vector->cipher_iv.length;
		/* cipher different than null */
		if (options->cipher_algo != RTE_CRYPTO_CIPHER_NULL) {
			xform.cipher.key.data = test_vector->cipher_key.data;
			xform.cipher.key.length =
				test_vector->cipher_key.length;
		} else {
			xform.cipher.key.data = NULL;
			xform.cipher.key.length = 0;
		}

		/* Setup Auth Parameters */
		auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
		auth_xform.auth.algo = options->auth_algo;
		auth_xform.auth.op = options->auth_op;
		auth_xform.auth.iv.offset = iv_offset +
				xform.cipher.iv.length;
		/* auth different than null */
		if (options->auth_algo != RTE_CRYPTO_AUTH_NULL) {
			auth_xform.auth.digest_length = options->digest_sz;
			auth_xform.auth.key.length =
						test_vector->auth_key.length;
			auth_xform.auth.key.data = test_vector->auth_key.data;
			auth_xform.auth.iv.length = test_vector->auth_iv.length;
		} else {
			auth_xform.auth.digest_length = 0;
			auth_xform.auth.key.length = 0;
			auth_xform.auth.key.data = NULL;
			auth_xform.auth.iv.length = 0;
		}

		if (options->is_outbound) {
			crypto_xform = &xform;
			xform.next = &auth_xform;
			auth_xform.next = NULL;
		} else {
			crypto_xform = &auth_xform;
			auth_xform.next = &xform;
			xform.next = NULL;
		}
	} else {
		return NULL;
	}

#define CPERF_IPSEC_SRC_IP	0x01010101
#define CPERF_IPSEC_DST_IP	0x02020202
#define CPERF_IPSEC_SALT	0x0
#define CPERF_IPSEC_DEFTTL	64
	struct rte_security_ipsec_tunnel_param tunnel = {
		.type = RTE_SECURITY_IPSEC_TUNNEL_IPV4,
		{.ipv4 = {
			.src_ip = { .s_addr = CPERF_IPSEC_SRC_IP},
			.dst_ip = { .s_addr = CPERF_IPSEC_DST_IP},
			.dscp = 0,
			.df = 0,
			.ttl = CPERF_IPSEC_DEFTTL,
		} },
	};
	struct rte_security_session_conf sess_conf = {
		.action_type = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		{.ipsec = {
			.spi = rte_lcore_id() + 1,
			/**< For testing sake, lcore_id is taken as SPI so that
			 * for every core a different session is created.
			 */
			.salt = CPERF_IPSEC_SALT,
			.options = { 0 },
			.replay_win_sz = 0,
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.tunnel = tunnel,
		} },
		.userdata = NULL,
		.crypto_xform = crypto_xform,
	};

	if (options->is_outbound)
		sess_conf.ipsec.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS;
	else
		sess_conf.ipsec.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS;

	struct rte_security_ctx *ctx = (struct rte_security_ctx *)
				rte_cryptodev_get_sec_ctx(dev_id);

	/* Create security session */
	return (void *)rte_security_session_create(ctx, &sess_conf, sess_mp);
}

static void *
cperf_create_session(struct rte_mempool *sess_mp,
	uint8_t dev_id,
	const struct cperf_options *options,
	const struct cperf_test_vector *test_vector,
	uint16_t iv_offset)
{
	struct rte_crypto_sym_xform cipher_xform;
	struct rte_crypto_sym_xform auth_xform;
	struct rte_crypto_sym_xform aead_xform;
	void *sess = NULL;
	void *asym_sess = NULL;
	struct rte_crypto_asym_xform xform = {0};
	int ret;

	if (options->op_type == CPERF_ASYM_MODEX) {
		xform.next = NULL;
		xform.xform_type = RTE_CRYPTO_ASYM_XFORM_MODEX;
		xform.modex.modulus.data = options->modex_data->modulus.data;
		xform.modex.modulus.length = options->modex_data->modulus.len;
		xform.modex.exponent.data = options->modex_data->exponent.data;
		xform.modex.exponent.length = options->modex_data->exponent.len;

		ret = rte_cryptodev_asym_session_create(dev_id, &xform,
				sess_mp, &asym_sess);
		if (ret < 0) {
			RTE_LOG(ERR, USER1, "Asym session create failed\n");
			return NULL;
		}
		return asym_sess;
	}
#ifdef RTE_LIB_SECURITY
	/*
	 * security only
	 */
	if (options->op_type == CPERF_PDCP) {
		/* Setup Cipher Parameters */
		cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
		cipher_xform.next = NULL;
		cipher_xform.cipher.algo = options->cipher_algo;
		cipher_xform.cipher.op = options->cipher_op;
		cipher_xform.cipher.iv.offset = iv_offset;
		cipher_xform.cipher.iv.length = 4;

		/* cipher different than null */
		if (options->cipher_algo != RTE_CRYPTO_CIPHER_NULL) {
			cipher_xform.cipher.key.data = test_vector->cipher_key.data;
			cipher_xform.cipher.key.length = test_vector->cipher_key.length;
		} else {
			cipher_xform.cipher.key.data = NULL;
			cipher_xform.cipher.key.length = 0;
		}

		/* Setup Auth Parameters */
		if (options->auth_algo != 0) {
			auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
			auth_xform.next = NULL;
			auth_xform.auth.algo = options->auth_algo;
			auth_xform.auth.op = options->auth_op;
			auth_xform.auth.iv.offset = iv_offset +
				cipher_xform.cipher.iv.length;

			/* auth different than null */
			if (options->auth_algo != RTE_CRYPTO_AUTH_NULL) {
				auth_xform.auth.digest_length = options->digest_sz;
				auth_xform.auth.key.length = test_vector->auth_key.length;
				auth_xform.auth.key.data = test_vector->auth_key.data;
				auth_xform.auth.iv.length = test_vector->auth_iv.length;
			} else {
				auth_xform.auth.digest_length = 0;
				auth_xform.auth.key.length = 0;
				auth_xform.auth.key.data = NULL;
				auth_xform.auth.iv.length = 0;
			}

			cipher_xform.next = &auth_xform;
		} else {
			cipher_xform.next = NULL;
		}

		struct rte_security_session_conf sess_conf = {
			.action_type = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
			.protocol = RTE_SECURITY_PROTOCOL_PDCP,
			{.pdcp = {
				.bearer = 0x16,
				.domain = options->pdcp_domain,
				.pkt_dir = 0,
				.sn_size = options->pdcp_sn_sz,
				.hfn = options->pdcp_ses_hfn_en ?
					PDCP_DEFAULT_HFN : 0,
				.hfn_threshold = 0x70C0A,
				.sdap_enabled = options->pdcp_sdap,
				.hfn_ovrd = !(options->pdcp_ses_hfn_en),
			} },
			.crypto_xform = &cipher_xform
		};

		struct rte_security_ctx *ctx = (struct rte_security_ctx *)
					rte_cryptodev_get_sec_ctx(dev_id);

		/* Create security session */
		return (void *)rte_security_session_create(ctx, &sess_conf, sess_mp);
	}

	if (options->op_type == CPERF_IPSEC) {
		return create_ipsec_session(sess_mp, dev_id,
				options, test_vector, iv_offset);
	}

	if (options->op_type == CPERF_DOCSIS) {
		enum rte_security_docsis_direction direction;

		cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
		cipher_xform.next = NULL;
		cipher_xform.cipher.algo = options->cipher_algo;
		cipher_xform.cipher.op = options->cipher_op;
		cipher_xform.cipher.iv.offset = iv_offset;
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
		cipher_xform.next = NULL;

		if (options->cipher_op == RTE_CRYPTO_CIPHER_OP_ENCRYPT)
			direction = RTE_SECURITY_DOCSIS_DOWNLINK;
		else
			direction = RTE_SECURITY_DOCSIS_UPLINK;

		struct rte_security_session_conf sess_conf = {
			.action_type =
				RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
			.protocol = RTE_SECURITY_PROTOCOL_DOCSIS,
			{.docsis = {
				.direction = direction,
			} },
			.crypto_xform = &cipher_xform
		};
		struct rte_security_ctx *ctx = (struct rte_security_ctx *)
					rte_cryptodev_get_sec_ctx(dev_id);

		/* Create security session */
		return (void *)rte_security_session_create(ctx, &sess_conf, sess_mp);
	}
#endif
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
		sess = rte_cryptodev_sym_session_create(dev_id, &cipher_xform,
				sess_mp);
	/*
	 *  auth only
	 */
	} else if (options->op_type == CPERF_AUTH_ONLY) {
		auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
		auth_xform.next = NULL;
		auth_xform.auth.algo = options->auth_algo;
		auth_xform.auth.op = options->auth_op;
		auth_xform.auth.iv.offset = iv_offset;

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
		sess = rte_cryptodev_sym_session_create(dev_id, &auth_xform,
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
		auth_xform.auth.iv.offset = iv_offset +
			cipher_xform.cipher.iv.length;

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
			sess = rte_cryptodev_sym_session_create(dev_id,
					&cipher_xform, sess_mp);
		} else { /* auth then cipher */
			auth_xform.next = &cipher_xform;
			/* create crypto session */
			sess = rte_cryptodev_sym_session_create(dev_id,
					&auth_xform, sess_mp);
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
		sess = rte_cryptodev_sym_session_create(dev_id, &aead_xform,
				sess_mp);
	}

	return sess;
}

int
cperf_get_op_functions(const struct cperf_options *options,
		struct cperf_op_fns *op_fns)
{
	memset(op_fns, 0, sizeof(struct cperf_op_fns));

	op_fns->sess_create = cperf_create_session;

	switch (options->op_type) {
	case CPERF_AEAD:
		op_fns->populate_ops = cperf_set_ops_aead;
		break;

	case CPERF_AUTH_THEN_CIPHER:
	case CPERF_CIPHER_THEN_AUTH:
		op_fns->populate_ops = cperf_set_ops_cipher_auth;
		break;
	case CPERF_AUTH_ONLY:
		if (options->auth_algo == RTE_CRYPTO_AUTH_NULL)
			op_fns->populate_ops = cperf_set_ops_null_auth;
		else
			op_fns->populate_ops = cperf_set_ops_auth;
		break;
	case CPERF_CIPHER_ONLY:
		if (options->cipher_algo == RTE_CRYPTO_CIPHER_NULL)
			op_fns->populate_ops = cperf_set_ops_null_cipher;
		else
			op_fns->populate_ops = cperf_set_ops_cipher;
		break;
	case CPERF_ASYM_MODEX:
		op_fns->populate_ops = cperf_set_ops_asym;
		break;
#ifdef RTE_LIB_SECURITY
	case CPERF_PDCP:
	case CPERF_DOCSIS:
		op_fns->populate_ops = cperf_set_ops_security;
		break;
	case CPERF_IPSEC:
		op_fns->populate_ops = cperf_set_ops_security_ipsec;
		break;
#endif
	default:
		return -1;
	}

	return 0;
}
