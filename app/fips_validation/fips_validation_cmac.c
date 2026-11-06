/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <string.h>
#include <time.h>
#include <stdio.h>
#include <rte_string_fns.h>

#include <rte_cryptodev.h>

#include "fips_validation.h"

#define NEW_LINE_STR	"#"
#define OP_STR		"CMAC"

#define ALGO_STR	"Alg = "
#define MODE_STR	"Mode = "

#define COUNT_STR	"Count = "
#define KLEN_STR	"Klen = "
#define PTLEN_STR	"Mlen = "
#define TAGLEN_STR	"Tlen = "
#define KEY_STR		"Key = "
#define PT_STR		"Msg = "
#define TAG_STR		"Mac = "

#define GEN_STR		"Generate"
#define VERIF_STR	"Verify"

#define POS_NEG_STR	"Result = "
#define PASS_STR	"P"
#define FAIL_STR	"F"

#define KLEN_JSON_STR		"keyLen"
#define PTLEN_JSON_STR		"msgLen"
#define TAGLEN_JSON_STR		"macLen"
#define KEY_JSON_STR		"key"
#define PT_JSON_STR			"message"
#define TAG_JSON_STR		"mac"
#define DIRECTION_JSON_STR	"direction"
#define POS_NEG_JSON_STR	"testPassed"

#define GEN_JSON_STR	"gen"
#define VERIF_JSON_STR	"ver"

struct hash_algo_conversion {
	const char *str;
	enum fips_test_algorithms algo;
} cmac_algo[] = {
		{"AES", FIPS_TEST_ALGO_AES_CMAC},
};

#ifdef USE_JANSSON
static int
parser_read_cmac_direction_str(__rte_unused const char *key, char *src,
		__rte_unused struct fips_val *val)
{
	if (strcmp(src, "gen") == 0)
		info.op = FIPS_TEST_ENC_AUTH_GEN;
	else if (strcmp(src, "ver") == 0)
		info.op = FIPS_TEST_DEC_AUTH_VERIF;

	return 0;
}

struct fips_test_callback cmac_tests_interim_json_vectors[] = {
		{KLEN_JSON_STR, parser_read_uint32_bit_val, &vec.cipher_auth.key},
		{PTLEN_JSON_STR, parser_read_uint32_bit_val, &vec.pt},
		{TAGLEN_JSON_STR, parser_read_uint32_bit_val, &vec.cipher_auth.digest},
		{DIRECTION_JSON_STR, parser_read_cmac_direction_str, NULL},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback cmac_tests_json_vectors[] = {
		{KEY_JSON_STR, parse_uint8_hex_str, &vec.cipher_auth.key},
		{PT_JSON_STR, parse_uint8_known_len_hex_str, &vec.pt},
		{TAG_JSON_STR, parse_uint8_known_len_hex_str,
				&vec.cipher_auth.digest},
		{NULL, NULL, NULL} /**< end pointer */
};

static int
parse_test_cmac_json_writeback(struct fips_val *val)
{
	json_info.json_write_case = json_object();
	json_object_set(json_info.json_write_case, "tcId",
		json_object_get(json_info.json_test_case, "tcId"));

	if (info.op == FIPS_TEST_ENC_AUTH_GEN) {
		struct fips_val tmp_val = {val->val + vec.pt.len,
				vec.cipher_auth.digest.len};

		writeback_hex_str("", info.one_line_text, &tmp_val);
		json_object_set_new(json_info.json_write_case, TAG_JSON_STR,
			json_string(info.one_line_text));
	} else {
		if (vec.status == RTE_CRYPTO_OP_STATUS_SUCCESS)
			json_object_set_new(json_info.json_write_case, POS_NEG_JSON_STR,
				json_boolean(true));
		else if (vec.status == RTE_CRYPTO_OP_STATUS_AUTH_FAILED)
			json_object_set_new(json_info.json_write_case, POS_NEG_JSON_STR,
				json_boolean(false));
	}

	return 0;
}

int
parse_test_cmac_json_init(void)
{
	info.algo = FIPS_TEST_ALGO_AES_CMAC;

	info.parse_writeback = parse_test_cmac_json_writeback;
	info.callbacks = cmac_tests_json_vectors;
	info.interim_callbacks = cmac_tests_interim_json_vectors;

	return 0;
}
#endif /* USE_JANSSON */

static int
parse_test_cmac_writeback(struct fips_val *val)
{
	if (info.op == FIPS_TEST_ENC_AUTH_GEN) {
		struct fips_val tmp_val = {val->val + vec.pt.len,
				vec.cipher_auth.digest.len};

		fprintf(info.fp_wr, "%s", TAG_STR);
		parse_write_hex_str(&tmp_val);
	} else {
		fprintf(info.fp_wr, "%s", POS_NEG_STR);

		if (vec.status == RTE_CRYPTO_OP_STATUS_SUCCESS)
			fprintf(info.fp_wr, "%s\n", PASS_STR);
		else if (vec.status == RTE_CRYPTO_OP_STATUS_AUTH_FAILED)
			fprintf(info.fp_wr, "%s\n", FAIL_STR);
		else
			fprintf(info.fp_wr, "Error\n");
	}

	return 0;
}

struct fips_test_callback cmac_tests_vectors[] = {
		{KLEN_STR, parser_read_uint32_val, &vec.cipher_auth.key},
		{PTLEN_STR, parser_read_uint32_val, &vec.pt},
		{TAGLEN_STR, parser_read_uint32_val, &vec.cipher_auth.digest},
		{KEY_STR, parse_uint8_hex_str, &vec.cipher_auth.key},
		{PT_STR, parse_uint8_known_len_hex_str, &vec.pt},
		{TAG_STR, parse_uint8_known_len_hex_str,
				&vec.cipher_auth.digest},
		{NULL, NULL, NULL} /**< end pointer */
};

int
parse_test_cmac_init(void)
{
	char *tmp;
	uint32_t i, j;

	for (i = 0; i < info.nb_vec_lines; i++) {
		char *line = info.vec[i];

		tmp = strstr(line, ALGO_STR);
		if (!tmp)
			continue;

		for (j = 0; j < RTE_DIM(cmac_algo); j++) {
			if (!strstr(line, cmac_algo[j].str))
				continue;

			info.algo = cmac_algo[j].algo;
			break;
		}

		if (j == RTE_DIM(cmac_algo))
			return -EINVAL;

		tmp = strstr(line, MODE_STR);
		if (!tmp)
			return -1;

		if (strstr(tmp, GEN_STR))
			info.op = FIPS_TEST_ENC_AUTH_GEN;
		else if (strstr(tmp, VERIF_STR))
			info.op = FIPS_TEST_DEC_AUTH_VERIF;
		else
			return -EINVAL;
	}

	info.parse_writeback = parse_test_cmac_writeback;
	info.callbacks = cmac_tests_vectors;

	return 0;
}
