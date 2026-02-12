/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <rte_crypto.h>
#include <rte_dtls.h>
#include <rte_tls.h>

#include "test.h"
#include "test_cryptodev_security_tls_record.h"
#include "test_cryptodev_security_tls_record_test_vectors.h"
#include "test_security_proto.h"

int
test_tls_record_status_check(struct rte_crypto_op *op,
			     const struct tls_record_test_data *td)
{
	int ret = TEST_SUCCESS;

	if ((td->tls_record_xform.type == RTE_SECURITY_TLS_SESS_TYPE_READ) &&
	     td->ar_packet) {
		if (op->status != RTE_CRYPTO_OP_STATUS_ERROR) {
			printf("Anti replay test case failed\n");
			return TEST_FAILED;
		} else {
			return TEST_SUCCESS;
		}
	}

	if (op->status != RTE_CRYPTO_OP_STATUS_SUCCESS)
		ret = TEST_FAILED;

	return ret;
}

int
test_tls_record_sec_caps_verify(struct rte_security_tls_record_xform *tls_record_xform,
				const struct rte_security_capability *sec_cap, bool silent)
{
	/* Verify security capabilities */

	RTE_SET_USED(tls_record_xform);
	RTE_SET_USED(sec_cap);
	RTE_SET_USED(silent);

	return 0;
}

void
test_tls_record_td_read_from_write(const struct tls_record_test_data *td_out,
				   struct tls_record_test_data *td_in)
{
	memcpy(td_in, td_out, sizeof(*td_in));

	/* Populate output text of td_in with input text of td_out */
	memcpy(td_in->output_text.data, td_out->input_text.data, td_out->input_text.len);
	td_in->output_text.len = td_out->input_text.len;

	/* Populate input text of td_in with output text of td_out */
	memcpy(td_in->input_text.data, td_out->output_text.data, td_out->output_text.len);
	td_in->input_text.len = td_out->output_text.len;

	td_in->tls_record_xform.type = RTE_SECURITY_TLS_SESS_TYPE_READ;

	if (td_in->aead) {
		td_in->xform.aead.aead.op = RTE_CRYPTO_AEAD_OP_DECRYPT;
	} else {
		td_in->xform.chain.auth.auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;
		td_in->xform.chain.cipher.cipher.op = RTE_CRYPTO_CIPHER_OP_DECRYPT;
	}
}

int
test_tls_record_td_prepare(const struct crypto_param *param1, const struct crypto_param *param2,
			   const struct tls_record_test_flags *flags,
			   struct tls_record_test_data *td_array,
			   int nb_td, unsigned int data_len)
{
	int i, min_padding, hdr_len, tls_pkt_size, mac_len = 0, exp_nonce_len = 0, roundup_len = 0;
	struct tls_record_test_data *td = NULL;

	if ((flags->tls_version == RTE_SECURITY_VERSION_TLS_1_3) &&
	    (param1->type != RTE_CRYPTO_SYM_XFORM_AEAD))
		return TEST_SKIPPED;

	memset(td_array, 0, nb_td * sizeof(*td));

	for (i = 0; i < nb_td; i++) {
		td = &td_array[i];

		/* Prepare fields based on param */

		if (param1->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
			/* Copy template for packet & key fields */
			switch (flags->tls_version) {
			case RTE_SECURITY_VERSION_TLS_1_2:
				memcpy(td, &tls_test_data_aes_128_gcm_v1, sizeof(*td));
				break;
			case RTE_SECURITY_VERSION_DTLS_1_2:
				memcpy(td, &dtls_test_data_aes_128_gcm, sizeof(*td));
				break;
			case RTE_SECURITY_VERSION_TLS_1_3:
				memcpy(td, &tls13_test_data_aes_128_gcm, sizeof(*td));
				break;
			}

			td->aead = true;
			td->xform.aead.aead.algo = param1->alg.aead;
			td->xform.aead.aead.key.length = param1->key_length;
			td->xform.aead.aead.digest_length = param1->digest_length;
		} else {
			/* Copy template for packet & key fields */
			if (flags->tls_version == RTE_SECURITY_VERSION_DTLS_1_2)
				memcpy(td, &dtls_test_data_aes_128_cbc_sha1_hmac, sizeof(*td));
			else
				memcpy(td, &tls_test_data_aes_128_cbc_sha1_hmac, sizeof(*td));

			td->aead = false;
			td->xform.chain.cipher.cipher.algo = param1->alg.cipher;
			td->xform.chain.cipher.cipher.key.length = param1->key_length;
			td->xform.chain.cipher.cipher.iv.length = param1->iv_length;
			td->xform.chain.auth.auth.algo = param2->alg.auth;
			td->xform.chain.auth.auth.key.length = param2->key_length;
			td->xform.chain.auth.auth.digest_length = param2->digest_length;
		}

		if (flags->data_walkthrough || flags->zero_len) {
			test_sec_proto_pattern_set(td->input_text.data, data_len);
			td->input_text.len = data_len;
		}

		if (flags->content_type == TLS_RECORD_TEST_CONTENT_TYPE_CUSTOM)
			td->app_type = RTE_TLS_TYPE_MAX;
		else if (flags->content_type == TLS_RECORD_TEST_CONTENT_TYPE_HANDSHAKE)
			td->app_type = RTE_TLS_TYPE_HANDSHAKE;

		tls_pkt_size = td->input_text.len;

		if (!td->aead) {
			mac_len = td->xform.chain.auth.auth.digest_length;
			min_padding = 1;
			switch (td->xform.chain.cipher.cipher.algo) {
			case RTE_CRYPTO_CIPHER_3DES_CBC:
				roundup_len = 8;
				exp_nonce_len = 8;
				break;
			case RTE_CRYPTO_CIPHER_AES_CBC:
				roundup_len = 16;
				exp_nonce_len = 16;
				break;
			default:
				roundup_len = 0;
				exp_nonce_len = 0;
				break;
			}
		} else {
			mac_len = td->xform.aead.aead.digest_length;
			min_padding = 0;
			roundup_len = 0;
			if (td->tls_record_xform.ver == RTE_SECURITY_VERSION_TLS_1_3)
				exp_nonce_len = 0;
			else
				exp_nonce_len = 8;
		}

		switch (td->tls_record_xform.ver) {
		case RTE_SECURITY_VERSION_TLS_1_2:
			hdr_len = sizeof(struct rte_tls_hdr);
			break;
		case RTE_SECURITY_VERSION_TLS_1_3:
			hdr_len = sizeof(struct rte_tls_hdr);
			/* Add 1 byte for content type in packet */
			tls_pkt_size += 1;
			break;
		case RTE_SECURITY_VERSION_DTLS_1_2:
			hdr_len = sizeof(struct rte_dtls_hdr);
			break;
		default:
			return TEST_SKIPPED;
		}

		tls_pkt_size += mac_len;

		/* Padding */
		tls_pkt_size += min_padding;

		if (roundup_len)
			tls_pkt_size = RTE_ALIGN_MUL_CEIL(tls_pkt_size, roundup_len);

		/* Explicit nonce */
		tls_pkt_size += exp_nonce_len;

		/* Add TLS header */
		tls_pkt_size += hdr_len;

		td->output_text.len = tls_pkt_size;

	}
	return TEST_SUCCESS;
}

void
test_tls_record_td_update(struct tls_record_test_data td_inb[],
			  const struct tls_record_test_data td_outb[], int nb_td,
			  const struct tls_record_test_flags *flags)
{
	int i;

	for (i = 0; i < nb_td; i++) {
		memcpy(td_inb[i].output_text.data, td_outb[i].input_text.data,
		       td_outb[i].input_text.len);
		td_inb[i].output_text.len = td_outb->input_text.len;

		/* Corrupt the content type in the TLS header of encrypted packet */
		if (flags->pkt_corruption)
			td_inb[i].input_text.data[0] = ~td_inb[i].input_text.data[0];

		/* Corrupt a byte in the last but one block */
		if (flags->padding_corruption) {
			int offset = td_inb[i].input_text.len - TLS_RECORD_PAD_CORRUPT_OFFSET;

			td_inb[i].input_text.data[offset] = ~td_inb[i].input_text.data[offset];
		}

		/* Clear outbound specific flags */
		td_inb[i].tls_record_xform.options.iv_gen_disable = 0;
	}

	RTE_SET_USED(flags);
}

static int
test_tls_record_td_verify(uint8_t *output_text, uint32_t len, const struct tls_record_test_data *td,
			 bool silent)
{
	if (len != td->output_text.len) {
		printf("Output length (%d) not matching with expected (%d)\n",
			len, td->output_text.len);
		return TEST_FAILED;
	}

	if (memcmp(output_text, td->output_text.data, len)) {
		if (silent)
			return TEST_FAILED;

		printf("[%s : %d] %s\n", __func__, __LINE__, "Output text not as expected\n");

		rte_hexdump(stdout, "expected", td->output_text.data, len);
		rte_hexdump(stdout, "actual", output_text, len);
		return TEST_FAILED;
	}

	return TEST_SUCCESS;
}

static int
test_tls_record_res_d_prepare(const uint8_t *output_text, uint32_t len,
			      const struct tls_record_test_data *td,
			      struct tls_record_test_data *res_d)
{
	memcpy(res_d, td, sizeof(*res_d));

	memcpy(&res_d->input_text.data, output_text, len);
	res_d->input_text.len = len;
	res_d->output_text.len = td->input_text.len;

	res_d->tls_record_xform.type = RTE_SECURITY_TLS_SESS_TYPE_READ;
	if (res_d->aead) {
		res_d->xform.aead.aead.op = RTE_CRYPTO_AEAD_OP_DECRYPT;
	} else {
		res_d->xform.chain.cipher.cipher.op = RTE_CRYPTO_CIPHER_OP_DECRYPT;
		res_d->xform.chain.auth.auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;
	}

	return TEST_SUCCESS;
}

static int
tls_record_hdr_verify(const struct tls_record_test_data *td, const uint8_t *output_text,
		      const struct tls_record_test_flags *flags)
{
	uint16_t length, hdr_len;
	uint8_t content_type;

	if (td->tls_record_xform.ver == RTE_SECURITY_VERSION_TLS_1_2) {
		const struct rte_tls_hdr *hdr = (const struct rte_tls_hdr *)output_text;
		if (rte_be_to_cpu_16(hdr->version) != RTE_TLS_VERSION_1_2) {
			printf("Incorrect header version [expected - %4x, received - %4x]\n",
			       RTE_TLS_VERSION_1_2, rte_be_to_cpu_16(hdr->version));
			return TEST_FAILED;
		}
		content_type = hdr->type;
		length = rte_be_to_cpu_16(hdr->length);
		hdr_len = sizeof(struct rte_tls_hdr);
	} else if (td->tls_record_xform.ver == RTE_SECURITY_VERSION_TLS_1_3) {
		const struct rte_tls_hdr *hdr = (const struct rte_tls_hdr *)output_text;
		if (rte_be_to_cpu_16(hdr->version) != RTE_TLS_VERSION_1_2) {
			printf("Incorrect header version [expected - %4x, received - %4x]\n",
			       RTE_TLS_VERSION_1_2, rte_be_to_cpu_16(hdr->version));
			return TEST_FAILED;
		}
		content_type = hdr->type;
		length = rte_be_to_cpu_16(hdr->length);
		hdr_len = sizeof(struct rte_tls_hdr);
	} else if (td->tls_record_xform.ver == RTE_SECURITY_VERSION_DTLS_1_2) {
		const struct rte_dtls_hdr *hdr = (const struct rte_dtls_hdr *)output_text;
		if (rte_be_to_cpu_16(hdr->version) != RTE_DTLS_VERSION_1_2) {
			printf("Incorrect header version [expected - %4x, received - %4x]\n",
			       RTE_DTLS_VERSION_1_2, rte_be_to_cpu_16(hdr->version));
			return TEST_FAILED;
		}
		content_type = hdr->type;
		length = rte_be_to_cpu_16(hdr->length);
		hdr_len = sizeof(struct rte_dtls_hdr);
	} else {
		return TEST_FAILED;
	}

	if (td->tls_record_xform.ver == RTE_SECURITY_VERSION_TLS_1_3) {
		if (content_type != RTE_TLS_TYPE_APPDATA) {
			printf("Incorrect content type in packet [expected - %d, received - %d]\n",
			       td->app_type, content_type);
			return TEST_FAILED;
		}
	} else {
		if (content_type != td->app_type) {
			printf("Incorrect content type in packet [expected - %d, received - %d]\n",
			       td->app_type, content_type);
			return TEST_FAILED;
		}
	}

	if (!flags->opt_padding) {
		if (length != td->output_text.len - hdr_len) {
			printf("Incorrect packet length [expected - %d, received - %d]\n",
			       td->output_text.len - hdr_len, length);
			return TEST_FAILED;
		}
	} else {
		int pad_len = (flags->opt_padding * 8) > 256 ? 256 : (flags->opt_padding * 8);
		int expect_len = td->output_text.len - hdr_len + pad_len;

		if (length - expect_len > 32) {
			printf("Incorrect packet length [expected - %d, received - %d]\n",
			       expect_len, length);
			return TEST_FAILED;
		}

	}

	return TEST_SUCCESS;
}

int
test_tls_record_post_process(const struct rte_mbuf *m, const struct tls_record_test_data *td,
			     struct tls_record_test_data *res_d, bool silent,
			     const struct tls_record_test_flags *flags)
{
	uint8_t output_text[TEST_SEC_CIPHERTEXT_MAX_LEN];
	uint32_t len = rte_pktmbuf_pkt_len(m), data_len;
	const struct rte_mbuf *seg;
	const uint8_t *output;
	int ret;

	memset(output_text, 0, TEST_SEC_CIPHERTEXT_MAX_LEN);

	/*
	 * Actual data in packet might be less in error cases, hence take minimum of pkt_len and sum
	 * of data_len. This is done to run through negative test cases.
	 */
	data_len = 0;
	seg = m;
	while (seg) {
		data_len += seg->data_len;
		seg = seg->next;
	}

	len = RTE_MIN(len, data_len);
	TEST_ASSERT(len <= TEST_SEC_CIPHERTEXT_MAX_LEN, "Invalid packet length: %u", len);

	/* Copy mbuf payload to continuous buffer */
	output = rte_pktmbuf_read(m, 0, len, output_text);
	if (output != output_text) {
		/* Single segment mbuf, copy manually */
		memcpy(output_text, output, len);
	}

	if (td->tls_record_xform.type == RTE_SECURITY_TLS_SESS_TYPE_WRITE) {
		ret = tls_record_hdr_verify(td, output_text, flags);
		if (ret != TEST_SUCCESS)
			return ret;
	}

	/*
	 * In case of known vector tests & all record read (decrypt) tests, res_d provided would be
	 * NULL and output data need to be validated against expected. For record read (decrypt),
	 * output_text would be plain payload and for record write (encrypt), output_text would TLS
	 * record. Validate by comparing against known vectors.
	 *
	 * In case of combined mode tests, the output_text from TLS write (encrypt) operation (ie,
	 * TLS record) would need to be decrypted using a TLS read operation to obtain the plain
	 * text. Copy output_text to result data, 'res_d', so that inbound processing can be done.
	 */

	if (res_d == NULL)
		return test_tls_record_td_verify(output_text, len, td, silent);
	else
		return test_tls_record_res_d_prepare(output_text, len, td, res_d);
}
