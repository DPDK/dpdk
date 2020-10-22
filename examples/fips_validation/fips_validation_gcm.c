/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <string.h>
#include <time.h>
#include <stdio.h>

#include <rte_cryptodev.h>
#include <rte_malloc.h>

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
/* External/Internal IV generation, specified in file name, following NIST
 * GCMVS Section 6.1
 */
#define OP_ENC_EXT_STR	"ExtIV"
#define OP_ENC_INT_STR	"IntIV"

#define NEG_TEST_STR	"FAIL"

/**
 * GMAC is essentially zero length plaintext and uses AAD as input data.
 * NIST does not have GMAC specific test vector but using zero length "PTlen"
 * and uses AAD as input.
 **/
static int
parser_read_gcm_pt_len(const char *key, char *src,
		__rte_unused struct fips_val *val)
{
	int ret = parser_read_uint32_bit_val(key, src, &vec.pt);

	if (ret < 0)
		return ret;

	if (vec.pt.len == 0) {
		info.interim_info.gcm_data.is_gmac = 1;
		test_ops.prepare_op = prepare_auth_op;
		test_ops.prepare_xform = prepare_gmac_xform;
	} else {
		info.interim_info.gcm_data.is_gmac = 0;
		test_ops.prepare_op = prepare_aead_op;
		test_ops.prepare_xform = prepare_gcm_xform;
	}

	return ret;
}

static int
parse_gcm_aad_str(const char *key, char *src,
		__rte_unused struct fips_val *val)
{
	/* For GMAC test vector, AAD is treated as input */
	if (info.interim_info.gcm_data.is_gmac) {
		vec.pt.len = vec.aead.aad.len;
		return parse_uint8_known_len_hex_str(key, src, &vec.pt);
	} else /* gcm */
		return parse_uint8_known_len_hex_str(key, src, &vec.aead.aad);
}

static int
parse_gcm_pt_ct_str(const char *key, char *src, struct fips_val *val)
{
	/* According to NIST GCMVS section 6.1, IUT should generate IV data */
	if (info.interim_info.gcm_data.gen_iv && vec.iv.len) {
		uint32_t i;

		if (!vec.iv.val) {
			vec.iv.val = rte_malloc(0, vec.iv.len, 0);
			if (!vec.iv.val)
				return -ENOMEM;
		}

		for (i = 0; i < vec.iv.len; i++) {
			int random = rand();
			vec.iv.val[i] = (uint8_t)random;
		}
	}

	/* if PTlen == 0, pt or ct will be handled by AAD later */
	if (info.interim_info.gcm_data.is_gmac)
		return 0;

	return parse_uint8_known_len_hex_str(key, src, val);
}

struct fips_test_callback gcm_dec_vectors[] = {
		{KEY_STR, parse_uint8_known_len_hex_str, &vec.aead.key},
		{IV_STR, parse_uint8_known_len_hex_str, &vec.iv},
		{CT_STR, parse_gcm_pt_ct_str, &vec.ct},
		{AAD_STR, parse_gcm_aad_str, &vec.aead.aad},
		{TAG_STR, parse_uint8_known_len_hex_str,
				&vec.aead.digest},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback gcm_interim_vectors[] = {
		{KEYLEN_STR, parser_read_uint32_bit_val, &vec.aead.key},
		{IVLEN_STR, parser_read_uint32_bit_val, &vec.iv},
		{PTLEN_STR, parser_read_gcm_pt_len, &vec.pt},
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
		{PT_STR, parse_gcm_pt_ct_str, &vec.pt},
		{AAD_STR, parse_gcm_aad_str, &vec.aead.aad},
		{NULL, NULL, NULL} /**< end pointer */
};

static int
parse_test_gcm_writeback(struct fips_val *val)
{
	struct fips_val tmp_val;

	if (info.op == FIPS_TEST_ENC_AUTH_GEN) {
		/* According to NIST GCMVS section 6.1, IUT should provide
		 * generate IV data
		 */
		if (info.interim_info.gcm_data.gen_iv) {
			fprintf(info.fp_wr, "%s", IV_STR);
			tmp_val.val = vec.iv.val;
			tmp_val.len = vec.iv.len;

			parse_write_hex_str(&tmp_val);
			rte_free(vec.iv.val);
			vec.iv.val = NULL;
		}

		fprintf(info.fp_wr, "%s", CT_STR);

		if (!info.interim_info.gcm_data.is_gmac) {
			tmp_val.val = val->val;
			tmp_val.len = vec.pt.len;

			parse_write_hex_str(&tmp_val);
		} else
			fprintf(info.fp_wr, "\n");

		fprintf(info.fp_wr, "%s", TAG_STR);

		tmp_val.val = val->val + vec.pt.len;
		tmp_val.len = val->len - vec.pt.len;

		parse_write_hex_str(&tmp_val);
	} else {
		if (vec.status == RTE_CRYPTO_OP_STATUS_SUCCESS) {
			fprintf(info.fp_wr, "%s", PT_STR);
			if (!info.interim_info.gcm_data.is_gmac) {
				tmp_val.val = val->val;
				tmp_val.len = vec.pt.len;

				parse_write_hex_str(&tmp_val);
			} else
				fprintf(info.fp_wr, "\n");
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
				if (strstr(info.file_name, OP_ENC_INT_STR))
					info.interim_info.gcm_data.gen_iv = 1;
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
