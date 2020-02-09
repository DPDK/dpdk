/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <string.h>
#include <time.h>
#include <stdio.h>

#include <rte_cryptodev.h>

#include "fips_validation.h"

#define MODE_STR	"AESVS"
#define ALGO_STR	"test data for "
#define OP_STR		"State"
#define KEY_SIZE_STR	"Key Length : "


#define COUNT_STR	"COUNT = "
#define KEY_STR		"KEY = "
#define IV_STR		"IV = "
#define PT_STR		"PLAINTEXT = "
#define CT_STR		"CIPHERTEXT = "

#define OP_ENC_STR	"ENCRYPT"
#define OP_DEC_STR	"DECRYPT"

struct {
	uint32_t type;
	const char *desc;
} aes_test_types[] = {
		{AESAVS_TYPE_GFXBOX, "GFSbox"},
		{AESAVS_TYPE_KEYSBOX, "KeySbox"},
		{AESAVS_TYPE_VARKEY, "VarKey"},
		{AESAVS_TYPE_VARTXT, "VarTxt"},
		{TDES_VARIABLE_TEXT, "VARIABLE PLAINTEXT/CIPHERTEXT"},
		{TDES_VARIABLE_TEXT, "KAT"},
		{AESAVS_TYPE_MMT, "MMT"},
		{AESAVS_TYPE_MCT, "MCT"},
};

struct aes_test_algo {
	const char *name;
	enum rte_crypto_cipher_algorithm algo;
} const algo_con[] = {
		{"CBC", RTE_CRYPTO_CIPHER_AES_CBC},
		{"ECB", RTE_CRYPTO_CIPHER_AES_ECB},
};

static int
parse_interim_enc_dec(const char *key,
		__rte_unused char *text,
		__rte_unused struct fips_val *val)
{
	if (strcmp(key, OP_ENC_STR) == 0)
		info.op = FIPS_TEST_ENC_AUTH_GEN;
	else if (strcmp(key, OP_DEC_STR) == 0)
		info.op = FIPS_TEST_DEC_AUTH_VERIF;
	else
		return -1;

	return 0;
}

struct fips_test_callback aes_tests_interim[] = {
		{OP_ENC_STR, parse_interim_enc_dec, NULL},
		{OP_DEC_STR, parse_interim_enc_dec, NULL},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback aes_tests_vectors[] = {
		{KEY_STR, parse_uint8_hex_str, &vec.cipher_auth.key},
		{IV_STR, parse_uint8_hex_str, &vec.iv},
		{PT_STR, parse_uint8_hex_str, &vec.pt},
		{CT_STR, parse_uint8_hex_str, &vec.ct},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback aes_tests_interim_vectors[] = {
		{OP_ENC_STR, parse_interim_enc_dec, NULL},
		{OP_DEC_STR, parse_interim_enc_dec, NULL},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback aes_writeback_callbacks[] = {
		/** First element is used to pass COUNT string */
		{COUNT_STR, NULL, NULL},
		{IV_STR, writeback_hex_str, &vec.iv},
		{KEY_STR, writeback_hex_str, &vec.cipher_auth.key},
		{PT_STR, writeback_hex_str, &vec.pt},
		{CT_STR, writeback_hex_str, &vec.ct},
		{NULL, NULL, NULL} /**< end pointer */
};

static int
parse_test_aes_writeback(struct fips_val *val)
{
	if (info.op == FIPS_TEST_ENC_AUTH_GEN)
		fprintf(info.fp_wr, "%s", CT_STR);
	else
		fprintf(info.fp_wr, "%s", PT_STR);

	parse_write_hex_str(val);

	return 0;
}

static int
rsp_test_aes_check(struct fips_val *val)
{
	struct fips_val *data;

	if (info.op == FIPS_TEST_ENC_AUTH_GEN)
		data = &vec.ct;
	else
		data = &vec.pt;

	if (memcmp(val->val, data->val, val->len) == 0)
		fprintf(info.fp_wr, "Success\n");
	else
		fprintf(info.fp_wr, "Failed\n");

	return 0;
}

int
parse_test_aes_init(void)
{
	char *tmp;
	uint32_t i, j;

	for (i = 0; i < info.nb_vec_lines; i++) {
		char *line = info.vec[i];

		tmp = strstr(line, MODE_STR);
		if (tmp) {
			for (j = 0; j < RTE_DIM(aes_test_types); j++)
				if (strstr(line, aes_test_types[j].desc)) {
					info.interim_info.aes_data.test_type =
							aes_test_types[j].type;
					break;
				}

			if (j >= RTE_DIM(aes_test_types))
				return -EINVAL;

			tmp = strstr(line, ALGO_STR);
			if (!tmp)
				return -EINVAL;

			tmp += strlen(ALGO_STR);
			for (j = 0; j < RTE_DIM(algo_con); j++)
				if (strcmp(algo_con[j].name, tmp) == 0) {
					info.interim_info.aes_data.cipher_algo =
						(uint32_t)algo_con[j].algo;
					break;
				}
			if (j >= RTE_DIM(algo_con))
				return -EINVAL;

			continue;
		}

		tmp = strstr(line, OP_STR);
		if (tmp)
			continue;

		tmp = strstr(line, KEY_SIZE_STR);
		if (tmp) {
			tmp += strlen(KEY_SIZE_STR);
			if (parser_read_uint32
					(&info.interim_info.aes_data.key_len,
							tmp) < 0)
				return -EINVAL;

			info.interim_info.aes_data.key_len /= 8;

			continue;
		}
	}

	info.parse_writeback = parse_test_aes_writeback;
	info.callbacks = aes_tests_vectors;
	info.interim_callbacks = aes_tests_interim_vectors;
	info.writeback_callbacks = aes_writeback_callbacks;
	info.kat_check = rsp_test_aes_check;

	return 0;
}
