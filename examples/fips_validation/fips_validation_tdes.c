/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <string.h>
#include <stdio.h>

#include <rte_malloc.h>
#include <rte_cryptodev.h>

#include "fips_validation.h"

#define NEW_LINE_STR	"#"
#define TEST_TYPE_KEY	" for CBC"
#define TEST_CBCI_KEY	" for CBCI"

#define ENC_STR		"[ENCRYPT]"
#define DEC_STR		"[DECRYPT]"

#define COUNT_STR	"COUNT = "
#define KEY1_STR	"KEY1 = "
#define KEY2_STR	"KEY2 = "
#define KEY3_STR	"KEY3 = "

#define KEYS_STR	"KEYs = "
#define IV_STR		"IV = "
#define PT_STR		"PLAINTEXT = "
#define CT_STR		"CIPHERTEXT = "
#define NK_STR		"NumKeys = "

#define SET_STR		" = "

#define PLAIN_TEXT	0
#define CIPHER_TEXT	1
#define KEY_TEXT	2
#define IV_TEXT		3

#define DEVICE_STR	"# Config Info for : "

struct {
	uint32_t type;
	const char *desc;
} test_types[] = {
		{TDES_INVERSE_PERMUTATION, "INVERSE PERMUTATION"},
		{TDES_PERMUTATION, "PERMUTATION OPERATION"},
		{TDES_SUBSTITUTION_TABLE, "SUBSTITUTION TABLE"},
		{TDES_VARIABLE_KEY, "VARIABLE KEY"},
		{TDES_VARIABLE_TEXT, "VARIABLE PLAINTEXT/CIPHERTEXT"},
		{TDES_VARIABLE_TEXT, "KAT"},
		{TDES_MCT, "Monte Carlo (Modes) Test"},
		{TDES_MMT, "Multi block Message Test"},
};

static int
writeback_tdes_hex_str(const char *key, char *dst, struct fips_val *val);

static int
parse_tdes_uint8_hex_str(const char *key, char *src, struct fips_val *val);

static int
parse_tdes_interim(const char *key,
		__attribute__((__unused__)) char *text,
		struct fips_val *val);

struct fips_test_callback tdes_tests_vectors[] = {
		{KEYS_STR, parse_tdes_uint8_hex_str, &vec.cipher_auth.key},
		{KEY1_STR, parse_tdes_uint8_hex_str, &vec.cipher_auth.key},
		{KEY2_STR, parse_tdes_uint8_hex_str, &vec.cipher_auth.key},
		{KEY3_STR, parse_tdes_uint8_hex_str, &vec.cipher_auth.key},
		{IV_STR, parse_uint8_hex_str, &vec.iv},
		{PT_STR, parse_uint8_hex_str, &vec.pt},
		{CT_STR, parse_uint8_hex_str, &vec.ct},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback tdes_tests_interim_vectors[] = {
		{ENC_STR, parse_tdes_interim, NULL},
		{DEC_STR, parse_tdes_interim, NULL},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback tdes_writeback_callbacks[] = {
		/** First element is used to pass COUNT string */
		{COUNT_STR, NULL, NULL},
		{IV_STR, writeback_hex_str, &vec.iv},
		{KEY1_STR, writeback_tdes_hex_str, &vec.cipher_auth.key},
		{KEY2_STR, writeback_tdes_hex_str, &vec.cipher_auth.key},
		{KEY3_STR, writeback_tdes_hex_str, &vec.cipher_auth.key},
		{KEYS_STR, writeback_tdes_hex_str, &vec.cipher_auth.key},
		{PT_STR, writeback_hex_str, &vec.pt},
		{CT_STR, writeback_hex_str, &vec.ct},
		{NULL, NULL, NULL} /**< end pointer */
};

static int
parse_tdes_interim(const char *key,
		__attribute__((__unused__)) char *text,
		__attribute__((__unused__)) struct fips_val *val)
{
	if (strstr(key, ENC_STR))
		info.op = FIPS_TEST_ENC_AUTH_GEN;
	else if (strstr(key, DEC_STR))
		info.op = FIPS_TEST_DEC_AUTH_VERIF;
	else if (strstr(NK_STR, "NumKeys = 1"))
		info.interim_info.tdes_data.nb_keys = 1;
	else if (strstr(NK_STR, "NumKeys = 2"))
		info.interim_info.tdes_data.nb_keys = 2;
	else if (strstr(NK_STR, "NumKeys = 3"))
		info.interim_info.tdes_data.nb_keys = 3;
	else
		return -EINVAL;

	return 0;
}

static int
parse_tdes_uint8_hex_str(const char *key, char *src, struct fips_val *val)
{
	uint8_t tmp_key[24] = {0};
	uint32_t len, i;

	src += strlen(key);

	len = strlen(src) / 2;

	if (val->val) {
		memcpy(tmp_key, val->val, val->len);
		rte_free(val->val);
	}

	val->val = rte_zmalloc(NULL, 24, 0);
	if (!val->val)
		return -1;

	memcpy(val->val, tmp_key, 24);

	if (strstr(key, KEYS_STR)) {
		for (i = 0; i < len; i++) {
			char byte[3] = {src[i * 2], src[i * 2 + 1], '\0'};

			if (parser_read_uint8_hex(&val->val[i], byte) < 0)
				goto error_exit;
		}

		memcpy(val->val + 8, val->val, 8);
		memcpy(val->val + 16, val->val, 8);

	} else if (strstr(key, KEY1_STR)) {
		for (i = 0; i < len; i++) {
			char byte[3] = {src[i * 2], src[i * 2 + 1], '\0'};

			if (parser_read_uint8_hex(&val->val[i], byte) < 0)
				goto error_exit;
		}

		if (info.interim_info.tdes_data.nb_keys == 2)
			memcpy(val->val + 16, val->val, 8);

	} else if (strstr(key, KEY2_STR)) {
		for (i = 0; i < len; i++) {
			char byte[3] = {src[i * 2], src[i * 2 + 1], '\0'};

			if (parser_read_uint8_hex(&val->val[i + 8], byte) < 0)
				goto error_exit;
		}

	} else if (strstr(key, KEY3_STR)) {
		for (i = 0; i < len; i++) {
			char byte[3] = {src[i * 2], src[i * 2 + 1], '\0'};

			if (parser_read_uint8_hex(&val->val[i + 16], byte) < 0)
				goto error_exit;
		}
	} else
		return -EINVAL;

	val->len = 24;

	return 0;

error_exit:
	rte_free(val->val);
	memset(val, 0, sizeof(*val));
	return -EINVAL;
}

static int
parse_test_tdes_writeback(struct fips_val *val)
{

	if (info.op == FIPS_TEST_ENC_AUTH_GEN)
		fprintf(info.fp_wr, "%s", CT_STR);
	else
		fprintf(info.fp_wr, "%s", PT_STR);

	parse_write_hex_str(val);

	return 0;

}

static int
writeback_tdes_hex_str(const char *key, char *dst, struct fips_val *val)
{
	struct fips_val tmp_val = {0};

	tmp_val.len = 8;

	if (strstr(key, KEY1_STR))
		tmp_val.val = val->val;
	else if (strstr(key, KEY2_STR))
		tmp_val.val = val->val + 8;
	else if (strstr(key, KEY3_STR))
		tmp_val.val = val->val + 16;
	else
		return -EINVAL;

	return writeback_hex_str(key, dst, &tmp_val);
}

static int
rsp_test_tdes_check(struct fips_val *val)
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
parse_test_tdes_init(void)
{
	uint32_t i;

	for (i = 0; i < info.nb_vec_lines; i++) {
		char *line = info.vec[i];
		uint32_t j;

		if (strstr(line, TEST_CBCI_KEY))
			return -EPERM;

		for (j = 0; j < RTE_DIM(test_types); j++)
			if (strstr(line, test_types[j].desc)) {
				info.interim_info.tdes_data.test_type =
						test_types[j].type;
				break;
			}
	}

	info.parse_writeback = parse_test_tdes_writeback;
	info.callbacks = tdes_tests_vectors;
	info.interim_callbacks = tdes_tests_interim_vectors;
	info.writeback_callbacks = tdes_writeback_callbacks;
	info.kat_check = rsp_test_tdes_check;

	return 0;
}
