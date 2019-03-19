/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <string.h>
#include <time.h>
#include <stdio.h>

#include <rte_cryptodev.h>

#include "fips_validation.h"

#define ALGO_PREFIX	"[L = "
#define MSGLEN_STR	"Len = "
#define MSG_STR		"Msg = "
#define MD_STR		"MD = "
#define SEED_STR	"Seed = "
#define MCT_STR		"Monte"

struct plain_hash_size_conversion {
	const char *str;
	enum rte_crypto_auth_algorithm algo;
} phsc[] = {
		{"20", RTE_CRYPTO_AUTH_SHA1},
		{"28", RTE_CRYPTO_AUTH_SHA224},
		{"32", RTE_CRYPTO_AUTH_SHA256},
		{"48", RTE_CRYPTO_AUTH_SHA384},
		{"64", RTE_CRYPTO_AUTH_SHA512},
};

static int
parse_interim_algo(__attribute__((__unused__)) const char *key,
		char *text,
		__attribute__((__unused__)) struct fips_val *val)
{
	uint32_t i;

	for (i = 0; i < RTE_DIM(phsc); i++) {
		if (strstr(text, phsc[i].str)) {
			info.interim_info.sha_data.algo = phsc[i].algo;
			parser_read_uint32_val(ALGO_PREFIX,
				text, &vec.cipher_auth.digest);
			break;
		}
	}

	if (i == RTE_DIM(phsc))
		return -1;

	return 0;
}

struct fips_test_callback sha_tests_vectors[] = {
		{MSGLEN_STR, parser_read_uint32_bit_val, &vec.pt},
		{MSG_STR, parse_uint8_known_len_hex_str, &vec.pt},
		{SEED_STR, parse_uint8_hex_str, &vec.cipher_auth.digest},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback sha_tests_interim_vectors[] = {
		{ALGO_PREFIX, parse_interim_algo, NULL},
		{NULL, NULL, NULL} /**< end pointer */
};

static int
parse_test_sha_writeback(struct fips_val *val) // !
{
	struct fips_val val_local;

	fprintf(info.fp_wr, "%s", MD_STR);

	val_local.val = val->val + vec.pt.len;
	val_local.len = vec.cipher_auth.digest.len;

	parse_write_hex_str(&val_local);
	return 0;
}

static int
rsp_test_sha_check(struct fips_val *val)
{
	if (memcmp(val->val + vec.pt.len, vec.cipher_auth.digest.val,
			vec.cipher_auth.digest.len) == 0)
		fprintf(info.fp_wr, "Success\n");
	else
		fprintf(info.fp_wr, "Failed\n");

	return 0;
}

int
parse_test_sha_init(void)
{
	uint32_t i;

	info.interim_info.sha_data.test_type = SHA_KAT;
	for (i = 0; i < info.nb_vec_lines; i++) {
		char *line = info.vec[i];
		if (strstr(line, MCT_STR))
			info.interim_info.sha_data.test_type = SHA_MCT;
	}

	info.op = FIPS_TEST_ENC_AUTH_GEN;
	info.parse_writeback = parse_test_sha_writeback;
	info.callbacks = sha_tests_vectors;
	info.interim_callbacks = sha_tests_interim_vectors;
	info.writeback_callbacks = NULL;
	info.kat_check = rsp_test_sha_check;
	return 0;
}
