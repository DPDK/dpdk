/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <rte_crypto.h>

#include "test.h"
#include "test_cryptodev_security_tls_record.h"
#include "test_cryptodev_security_tls_record_test_vectors.h"

int
test_tls_record_status_check(struct rte_crypto_op *op)
{
	int ret = TEST_SUCCESS;

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

	res_d->tls_record_xform.type = RTE_SECURITY_TLS_SESS_TYPE_READ;
	if (res_d->aead) {
		res_d->xform.aead.aead.op = RTE_CRYPTO_AEAD_OP_DECRYPT;
	} else {
		res_d->xform.chain.cipher.cipher.op = RTE_CRYPTO_CIPHER_OP_DECRYPT;
		res_d->xform.chain.auth.auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;
	}

	return TEST_SUCCESS;
}

int
test_tls_record_post_process(const struct rte_mbuf *m, const struct tls_record_test_data *td,
			     struct tls_record_test_data *res_d, bool silent)
{
	uint32_t len = rte_pktmbuf_pkt_len(m), data_len;
	uint8_t output_text[TLS_RECORD_MAX_LEN];
	const struct rte_mbuf *seg;
	const uint8_t *output;

	memset(output_text, 0, TLS_RECORD_MAX_LEN);

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
	TEST_ASSERT(len <= TLS_RECORD_MAX_LEN, "Invalid packet length: %u", len);

	/* Copy mbuf payload to continuous buffer */
	output = rte_pktmbuf_read(m, 0, len, output_text);
	if (output != output_text) {
		/* Single segment mbuf, copy manually */
		memcpy(output_text, output, len);
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
