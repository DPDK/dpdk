/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <string.h>
#include <time.h>
#include <stdio.h>

#include <rte_cryptodev.h>

#include "fips_validation.h"

#define ALGO_PREFIX	"[L="
#define KEYLEN_STR	"Klen = "
#define TAGLEN_STR	"Tlen = "

#define COUNT_STR	"Count = "
#define KEY_STR		"Key = "
#define PT_STR		"Msg = "
#define TAG_STR		"Mac = "

struct hash_size_conversion {
	const char *str;
	enum rte_crypto_auth_algorithm algo;
} hsc[] = {
		{"20", RTE_CRYPTO_AUTH_SHA1_HMAC},
		{"28", RTE_CRYPTO_AUTH_SHA224_HMAC},
		{"32", RTE_CRYPTO_AUTH_SHA256_HMAC},
		{"48", RTE_CRYPTO_AUTH_SHA384_HMAC},
		{"64", RTE_CRYPTO_AUTH_SHA512_HMAC},
};

static int
parse_interim_algo(__rte_unused const char *key,
		char *text,
		__rte_unused struct fips_val *val)
{

	uint32_t i;

	for (i = 0; i < RTE_DIM(hsc); i++) {
		if (strstr(text, hsc[i].str)) {
			info.interim_info.hmac_data.algo = hsc[i].algo;
			break;
		}
	}

	if (i == RTE_DIM(hsc))
		return -1;

	return 0;
}

struct fips_test_callback hmac_tests_vectors[] = {
		{KEYLEN_STR, parser_read_uint32_val, &vec.cipher_auth.key},
		{TAGLEN_STR, parser_read_uint32_val, &vec.cipher_auth.digest},
		{KEY_STR, parse_uint8_hex_str, &vec.cipher_auth.key},
		{PT_STR, parse_uint8_hex_str, &vec.pt},
		{TAG_STR, parse_uint8_hex_str, &vec.cipher_auth.digest},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback hmac_tests_interim_vectors[] = {
		{ALGO_PREFIX, parse_interim_algo, NULL},
		{NULL, NULL, NULL} /**< end pointer */
};

static int
parse_test_hmac_writeback(struct fips_val *val)
{
	struct fips_val val_local;

	fprintf(info.fp_wr, "%s", TAG_STR);

	val_local.val = val->val + vec.pt.len;
	val_local.len = vec.cipher_auth.digest.len;

	parse_write_hex_str(&val_local);
	return 0;
}

static int
rsp_test_hmac_check(struct fips_val *val)
{
	if (memcmp(val->val + vec.pt.len, vec.cipher_auth.digest.val,
			vec.cipher_auth.digest.len) == 0)
		fprintf(info.fp_wr, "Success\n");
	else
		fprintf(info.fp_wr, "Failed\n");

	return 0;
}

int
parse_test_hmac_init(void)
{
	info.op = FIPS_TEST_ENC_AUTH_GEN;
	info.parse_writeback = parse_test_hmac_writeback;
	info.callbacks = hmac_tests_vectors;
	info.interim_callbacks = hmac_tests_interim_vectors;
	info.writeback_callbacks = NULL;
	info.kat_check = rsp_test_hmac_check;

	return 0;
}
