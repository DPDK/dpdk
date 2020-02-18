/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <string.h>
#include <time.h>
#include <stdio.h>

#include <rte_cryptodev.h>

#include "fips_validation.h"

#define NEW_LINE_STR	"#"
#define OP_STR		"GCM "

#define PARAM_PREFIX	"["
#define KEYLEN_STR	"Keylen = "
#define IVLEN_STR	"IVlen = "
#define PTLEN_STR	"PTlen = "
#define AADLEN_STR	"AADlen = "
#define TAGLEN_STR	"Taglen = "

#define COUNT_STR	"Count = "
#define KEY_STR		"Key = "
#define IV_STR		"IV = "
#define PT_STR		"PT = "
#define CT_STR		"CT = "
#define TAG_STR		"Tag = "
#define AAD_STR		"AAD = "

#define OP_ENC_STR	"Encrypt"
#define OP_DEC_STR	"Decrypt"

#define NEG_TEST_STR	"FAIL"

struct fips_test_callback gcm_dec_vectors[] = {
		{KEY_STR, parse_uint8_known_len_hex_str, &vec.aead.key},
		{IV_STR, parse_uint8_known_len_hex_str, &vec.iv},
		{CT_STR, parse_uint8_known_len_hex_str, &vec.ct},
		{AAD_STR, parse_uint8_known_len_hex_str, &vec.aead.aad},
		{TAG_STR, parse_uint8_known_len_hex_str,
				&vec.aead.digest},
		{NULL, NULL, NULL} /**< end pointer */
};
struct fips_test_callback gcm_interim_vectors[] = {
		{KEYLEN_STR, parser_read_uint32_bit_val, &vec.aead.key},
		{IVLEN_STR, parser_read_uint32_bit_val, &vec.iv},
		{PTLEN_STR, parser_read_uint32_bit_val, &vec.pt},
		{PTLEN_STR, parser_read_uint32_bit_val, &vec.ct},
		/**< The NIST test vectors use 'PTlen' to denote input text
		 *  length in case of decrypt & encrypt operations.
		 */
		{AADLEN_STR, parser_read_uint32_bit_val, &vec.aead.aad},
		{TAGLEN_STR, parser_read_uint32_bit_val,
				&vec.aead.digest},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback gcm_enc_vectors[] = {
		{KEY_STR, parse_uint8_known_len_hex_str, &vec.aead.key},
		{IV_STR, parse_uint8_known_len_hex_str, &vec.iv},
		{PT_STR, parse_uint8_known_len_hex_str, &vec.pt},
		{AAD_STR, parse_uint8_known_len_hex_str, &vec.aead.aad},
		{NULL, NULL, NULL} /**< end pointer */
};

static int
parse_test_gcm_writeback(struct fips_val *val)
{
	struct fips_val tmp_val;

	if (info.op == FIPS_TEST_ENC_AUTH_GEN) {
		fprintf(info.fp_wr, "%s", CT_STR);

		tmp_val.val = val->val;
		tmp_val.len = vec.pt.len;

		parse_write_hex_str(&tmp_val);

		fprintf(info.fp_wr, "%s", TAG_STR);

		tmp_val.val = val->val + vec.pt.len;
		tmp_val.len = val->len - vec.pt.len;

		parse_write_hex_str(&tmp_val);
	} else {
		if (vec.status == RTE_CRYPTO_OP_STATUS_SUCCESS) {
			fprintf(info.fp_wr, "%s", PT_STR);

			tmp_val.val = val->val;
			tmp_val.len = vec.pt.len;

			parse_write_hex_str(&tmp_val);
		} else
			fprintf(info.fp_wr, "%s\n", NEG_TEST_STR);
	}

	return 0;
}

int
parse_test_gcm_init(void)
{
	char *tmp;
	uint32_t i;


	for (i = 0; i < info.nb_vec_lines; i++) {
		char *line = info.vec[i];


		tmp = strstr(line, OP_STR);
		if (tmp) {
			if (strstr(line, OP_ENC_STR)) {
				info.op = FIPS_TEST_ENC_AUTH_GEN;
				info.callbacks = gcm_enc_vectors;
			} else if (strstr(line, OP_DEC_STR)) {
				info.op = FIPS_TEST_DEC_AUTH_VERIF;
				info.callbacks = gcm_dec_vectors;
			} else
				return -EINVAL;
		}
	}

	info.interim_callbacks = gcm_interim_vectors;
	info.parse_writeback = parse_test_gcm_writeback;

	return 0;
}
