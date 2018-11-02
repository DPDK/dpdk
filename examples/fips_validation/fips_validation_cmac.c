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

struct hash_algo_conversion {
	const char *str;
	enum fips_test_algorithms algo;
} cmac_algo[] = {
		{"AES", FIPS_TEST_ALGO_AES_CMAC},
};

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
