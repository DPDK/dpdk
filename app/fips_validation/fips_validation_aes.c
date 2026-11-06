/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <string.h>
#include <time.h>
#include <stdio.h>

#include <rte_cryptodev.h>
#include <rte_malloc.h>

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

#define ALGO_JSON_STR		"algorithm"
#define TESTTYPE_JSON_STR	"testType"
#define DIR_JSON_STR		"direction"
#define KEYLEN_JSON_STR		"keyLen"
#define OVERFLOW_JSON_STR	"overflow"

#define KEY_JSON_STR	"key"
#define PAYLOADLEN_JSON_STR	"payloadLen"
#define IV_JSON_STR	"iv"
#define PT_JSON_STR	"pt"
#define CT_JSON_STR	"ct"

#define OP_ENC_JSON_STR	"encrypt"
#define OP_DEC_JSON_STR	"decrypt"

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
		{AESAVS_TYPE_AFT, "AFT"},
		{AESAVS_TYPE_CTR, "CTR"},
};

struct aes_test_algo {
	const char *name;
	enum rte_crypto_cipher_algorithm algo;
} const algo_con[] = {
		{"CBC", RTE_CRYPTO_CIPHER_AES_CBC},
		{"ECB", RTE_CRYPTO_CIPHER_AES_ECB},
		{"CTR", RTE_CRYPTO_CIPHER_AES_CTR},
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

#ifdef USE_JANSSON
struct fips_test_callback aes_dec_json_vectors[] = {
		{KEY_JSON_STR, parse_uint8_known_len_hex_str, &vec.cipher_auth.key},
		{IV_JSON_STR, parse_uint8_hex_str, &vec.iv},
		{CT_JSON_STR, parse_uint8_hex_str, &vec.ct},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback aes_interim_json_vectors[] = {
		{KEYLEN_JSON_STR, parser_read_uint32_bit_val, &vec.cipher_auth.key},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback aes_enc_json_vectors[] = {
		{KEY_JSON_STR, parse_uint8_known_len_hex_str, &vec.cipher_auth.key},
		{IV_JSON_STR, parse_uint8_hex_str, &vec.iv},
		{PT_JSON_STR, parse_uint8_hex_str, &vec.pt},
		{NULL, NULL, NULL} /**< end pointer */
};

static int
parse_test_aes_json_writeback(struct fips_val *val)
{
	struct fips_val tmp_val;
	json_t *tcId;

	tcId = json_object_get(json_info.json_test_case, "tcId");

	json_info.json_write_case = json_object();
	json_object_set(json_info.json_write_case, "tcId", tcId);

	if (info.op == FIPS_TEST_ENC_AUTH_GEN) {
		json_t *ct;

		tmp_val.val = val->val;
		tmp_val.len = vec.pt.len;

		writeback_hex_str("", info.one_line_text, &tmp_val);
		ct = json_string(info.one_line_text);
		json_object_set_new(json_info.json_write_case, CT_JSON_STR, ct);

		tmp_val.val = val->val + vec.pt.len;
		tmp_val.len = val->len - vec.pt.len;

		writeback_hex_str("", info.one_line_text, &tmp_val);
	} else {
		if (vec.status == RTE_CRYPTO_OP_STATUS_SUCCESS) {
			tmp_val.val = val->val;
			tmp_val.len = vec.ct.len;

			writeback_hex_str("", info.one_line_text, &tmp_val);
			json_object_set_new(json_info.json_write_case, PT_JSON_STR,
								json_string(info.one_line_text));
		} else {
			json_object_set_new(json_info.json_write_case, "testPassed", json_false());
		}
	}

	return 0;
}

static int
parse_test_aes_mct_json_writeback(struct fips_val *val)
{
	json_t *tcId, *resArr, *res, *ct, *pt, *key, *iv;
	struct fips_val tmp_val;

	tcId = json_object_get(json_info.json_test_case, "tcId");
	if (json_info.json_write_case) {
		json_t *wcId;

		wcId = json_object_get(json_info.json_write_case, "tcId");
		if (!json_equal(tcId, wcId)) {
			json_info.json_write_case = json_object();
			json_object_set(json_info.json_write_case, "tcId", tcId);
			json_object_set(json_info.json_write_case, "resultsArray", json_array());
		}
	} else {
		json_info.json_write_case = json_object();
		json_object_set(json_info.json_write_case, "tcId", tcId);
		json_object_set(json_info.json_write_case, "resultsArray", json_array());
	}

	resArr = json_object_get(json_info.json_write_case, "resultsArray");
	if (!json_is_array(resArr))
		return -EINVAL;

	res = json_object();
	if (info .op == FIPS_TEST_ENC_AUTH_GEN) {
		writeback_hex_str("", info.one_line_text, &vec.cipher_auth.key);
		key = json_string(info.one_line_text);
		json_object_set_new(res, KEY_JSON_STR, key);

		writeback_hex_str("", info.one_line_text, &val[2]);
		iv = json_string(info.one_line_text);
		json_object_set_new(res, IV_JSON_STR, iv);

		writeback_hex_str("", info.one_line_text, &val[1]);
		pt = json_string(info.one_line_text);
		json_object_set_new(res, PT_JSON_STR, pt);

		tmp_val.val = val->val;
		tmp_val.len = vec.pt.len;

		writeback_hex_str("", info.one_line_text, &tmp_val);
		ct = json_string(info.one_line_text);
		json_object_set_new(res, CT_JSON_STR, ct);

		tmp_val.val = val->val + vec.pt.len;
		tmp_val.len = val->len - vec.pt.len;

		writeback_hex_str("", info.one_line_text, &tmp_val);
	} else {
		if (vec.status == RTE_CRYPTO_OP_STATUS_SUCCESS) {
			writeback_hex_str("", info.one_line_text, &vec.cipher_auth.key);
			key = json_string(info.one_line_text);
			json_object_set_new(res, KEY_JSON_STR, key);

			writeback_hex_str("", info.one_line_text, &val[2]);
			iv = json_string(info.one_line_text);
			json_object_set_new(res, IV_JSON_STR, iv);

			tmp_val.val = val->val;
			tmp_val.len = vec.ct.len;

			writeback_hex_str("", info.one_line_text, &tmp_val);
			pt = json_string(info.one_line_text);
			json_object_set_new(res, PT_JSON_STR, pt);

			writeback_hex_str("", info.one_line_text, &val[1]);
			ct = json_string(info.one_line_text);
			json_object_set_new(res, CT_JSON_STR, ct);
		} else {
			json_object_set_new(json_info.json_write_case, "testPassed", json_false());
		}
	}

	json_array_append_new(resArr, res);
	return 0;
}

int
parse_test_aes_json_init(void)
{
	json_t *type_obj = json_object_get(json_info.json_test_group, TESTTYPE_JSON_STR);
	json_t *algo_obj = json_object_get(json_info.json_vector_set, ALGO_JSON_STR);
	const char *type_str = json_string_value(type_obj);
	const char *algo_str = json_string_value(algo_obj);
	uint32_t i;

	if (json_info.json_test_group) {
		json_t *direction_obj;
		const char *direction_str;

		direction_obj = json_object_get(json_info.json_test_group, DIR_JSON_STR);
		direction_str = json_string_value(direction_obj);

		if (strcmp(direction_str, OP_ENC_JSON_STR) == 0) {
			info.op = FIPS_TEST_ENC_AUTH_GEN;
			info.callbacks = aes_enc_json_vectors;

		} else if (strcmp(direction_str, OP_DEC_JSON_STR) == 0) {
			info.op = FIPS_TEST_DEC_AUTH_VERIF;
			info.callbacks = aes_dec_json_vectors;
		} else {
			return -EINVAL;
		}
		info.interim_callbacks = aes_interim_json_vectors;
	}

	for (i = 0; i < RTE_DIM(aes_test_types); i++)
		if (strstr(type_str, aes_test_types[i].desc)) {
			info.interim_info.aes_data.test_type =
				aes_test_types[i].type;
			break;
		}

	if (i >= RTE_DIM(aes_test_types))
		return -EINVAL;

	switch (info.interim_info.aes_data.test_type) {
	case AESAVS_TYPE_MCT:
		info.parse_writeback = parse_test_aes_mct_json_writeback;
		break;
	case AESAVS_TYPE_CTR:
	case AESAVS_TYPE_AFT:
		info.parse_writeback = parse_test_aes_json_writeback;
		break;
	default:
		info.parse_writeback = NULL;
	}

	if (!info.parse_writeback)
		return -EINVAL;

	for (i = 0; i < RTE_DIM(algo_con); i++)
		if (strstr(algo_str, algo_con[i].name)) {
			info.interim_info.aes_data.cipher_algo =
				(uint32_t)algo_con[i].algo;
			break;
		}

	if (i >= RTE_DIM(algo_con))
		return -EINVAL;

	return 0;
}
#endif /* USE_JANSSON */

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
