/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#include <string.h>
#include <stdio.h>
#include <time.h>

#include <rte_cryptodev.h>

#include "fips_validation.h"

#define MODE_STR	"XTS"
#define ALGO_STR	"test data for "
#define OP_STR		"State"
#define KEY_SIZE_STR	"Key Length : "

#define COUNT_STR	"COUNT = "
#define KEY_STR		"Key = "
#define IV_STR		"i = "
#define PT_STR		"PT = "
#define CT_STR		"CT = "

#define OP_ENC_STR	"ENCRYPT"
#define OP_DEC_STR	"DECRYPT"

static int
parse_interim_xts_enc_dec(const char *key,
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

struct fips_test_callback xts_tests_vectors[] = {
		{KEY_STR, parse_uint8_hex_str, &vec.cipher_auth.key},
		{IV_STR, parse_uint8_hex_str, &vec.iv},
		{PT_STR, parse_uint8_hex_str, &vec.pt},
		{CT_STR, parse_uint8_hex_str, &vec.ct},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback xts_tests_interim_vectors[] = {
		{OP_ENC_STR, parse_interim_xts_enc_dec, NULL},
		{OP_DEC_STR, parse_interim_xts_enc_dec, NULL},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback xts_writeback_callbacks[] = {
		/** First element is used to pass COUNT string */
		{COUNT_STR, NULL, NULL},
		{IV_STR, writeback_hex_str, &vec.iv},
		{KEY_STR, writeback_hex_str, &vec.cipher_auth.key},
		{PT_STR, writeback_hex_str, &vec.pt},
		{CT_STR, writeback_hex_str, &vec.ct},
		{NULL, NULL, NULL} /**< end pointer */
};

static int
parse_test_xts_writeback(struct fips_val *val)
{
	if (info.op == FIPS_TEST_ENC_AUTH_GEN)
		fprintf(info.fp_wr, "%s", CT_STR);
	else
		fprintf(info.fp_wr, "%s", PT_STR);

	parse_write_hex_str(val);
	return 0;
}

static int
rsp_test_xts_check(struct fips_val *val)
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

int parse_test_xts_init(void)
{
	char *tmp;
	uint32_t i;
	for (i = 0; i < info.nb_vec_lines; i++) {
		char *line = info.vec[i];
		tmp = strstr(line, KEY_SIZE_STR);
		if (tmp) {
			tmp += (strlen(KEY_SIZE_STR) + strlen("AES"));
			if (parser_read_uint32(
				&info.interim_info.aes_data.key_len,
					tmp) < 0)
				return -EINVAL;
			info.interim_info.aes_data.key_len =
			(info.interim_info.aes_data.key_len*2) / 8;
			continue;
		}

	}
	info.parse_writeback = parse_test_xts_writeback;
	info.callbacks = xts_tests_vectors;
	info.interim_callbacks = xts_tests_interim_vectors;
	info.writeback_callbacks = xts_writeback_callbacks;
	info.kat_check = rsp_test_xts_check;

	return 0;
}
