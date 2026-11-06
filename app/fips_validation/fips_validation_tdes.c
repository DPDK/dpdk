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
#define TEST_TYPE_ECB_KEY	" for ECB"
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

#define ALGO_JSON_STR	"algorithm"
#define TESTTYPE_JSON_STR "testType"
#define DIR_JSON_STR	"direction"
#define KEYOPT_JSON_STR	"keyingOption"

#define PT_JSON_STR	"pt"
#define CT_JSON_STR	"ct"
#define IV_JSON_STR	"iv"
#define KEY1_JSON_STR	"key1"
#define KEY2_JSON_STR	"key2"
#define KEY3_JSON_STR	"key3"

#define OP_ENC_JSON_STR	"encrypt"
#define OP_DEC_JSON_STR	"decrypt"

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
		{TDES_AFT, "Functional Test"},
		{TDES_MCT, "Monte Carlo (Modes) Test"},
		{TDES_MMT, "Multi block Message Test"},
};

static int
writeback_tdes_hex_str(const char *key, char *dst, struct fips_val *val);

static int
parse_tdes_uint8_hex_str(const char *key, char *src, struct fips_val *val);

static int
parse_tdes_interim(const char *key, char *text, struct fips_val *val);

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
		{NK_STR, parse_tdes_interim, NULL},
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

#ifdef USE_JANSSON
static struct {
	enum fips_tdes_test_types type;
	const char *desc;
} const tdes_test_types[] = {
	{TDES_AFT, "AFT"},
	{TDES_MCT, "MCT"},
};

static struct {
	enum fips_tdes_test_mode mode;
	const char *desc;
} const tdes_test_modes[] = {
	{TDES_MODE_CBC, "CBC"},
	{TDES_MODE_ECB, "ECB"},
};

#define TDES_KEYS_TUPLE_LEN 24

static int
parser_tdes_read_key(const char *key, char *src, struct fips_val *val)
{
	uint8_t tmp_key[TDES_KEYS_TUPLE_LEN] = {0};
	uint32_t len, i;

	len = strlen(src) / 2;

	if (val->val) {
		memcpy(tmp_key, val->val, val->len);
		rte_free(val->val);
	}

	val->val = rte_zmalloc(NULL, TDES_KEYS_TUPLE_LEN, 0);
	if (!val->val)
		return -1;

	memcpy(val->val, tmp_key, TDES_KEYS_TUPLE_LEN);

	if (strstr(key, KEY1_JSON_STR)) {
		for (i = 0; i < len; i++) {
			char byte[3] = {src[i * 2], src[i * 2 + 1], '\0'};

			if (parser_read_uint8_hex(&val->val[i], byte) < 0)
				goto error_exit;
		}

		if (info.interim_info.tdes_data.nb_keys == 2)
			memcpy(val->val + 16, val->val, 8);

	} else if (strstr(key, KEY2_JSON_STR)) {
		for (i = 0; i < len; i++) {
			char byte[3] = {src[i * 2], src[i * 2 + 1], '\0'};

			if (parser_read_uint8_hex(&val->val[i + 8], byte) < 0)
				goto error_exit;
		}

	} else if (strstr(key, KEY3_JSON_STR)) {
		for (i = 0; i < len; i++) {
			char byte[3] = {src[i * 2], src[i * 2 + 1], '\0'};

			if (parser_read_uint8_hex(&val->val[i + 16], byte) < 0)
				goto error_exit;
		}
	} else
		return -EINVAL;

	val->len = TDES_KEYS_TUPLE_LEN;

	return 0;

error_exit:
	rte_free(val->val);
	memset(val, 0, sizeof(*val));
	return -EINVAL;
}

struct fips_test_callback tdes_dec_json_vectors[] =	{
	{KEY1_JSON_STR, parser_tdes_read_key, &vec.cipher_auth.key},
	{KEY2_JSON_STR, parser_tdes_read_key, &vec.cipher_auth.key},
	{KEY3_JSON_STR, parser_tdes_read_key, &vec.cipher_auth.key},
	{IV_JSON_STR, parse_uint8_hex_str, &vec.iv},
	{CT_JSON_STR, parse_uint8_hex_str, &vec.ct},
	{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback tdes_enc_json_vectors[] =	{
	{KEY1_JSON_STR, parser_tdes_read_key, &vec.cipher_auth.key},
	{KEY2_JSON_STR, parser_tdes_read_key, &vec.cipher_auth.key},
	{KEY3_JSON_STR, parser_tdes_read_key, &vec.cipher_auth.key},
	{IV_JSON_STR, parse_uint8_hex_str, &vec.iv},
	{PT_JSON_STR, parse_uint8_hex_str, &vec.pt},
	{NULL, NULL, NULL} /**< end pointer */
};

static int
parse_test_tdes_json_writeback(struct fips_val *val)
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
parse_test_tdes_mct_json_writeback(struct fips_val *val)
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
		tmp_val.len = 8;
		tmp_val.val = vec.cipher_auth.key.val;
		writeback_hex_str("", info.one_line_text, &tmp_val);
		key = json_string(info.one_line_text);
		json_object_set_new(res, KEY1_JSON_STR, key);

		tmp_val.val = vec.cipher_auth.key.val + 8;
		writeback_hex_str("", info.one_line_text, &tmp_val);
		key = json_string(info.one_line_text);
		json_object_set_new(res, KEY2_JSON_STR, key);

		tmp_val.val =  vec.cipher_auth.key.val + 16;
		writeback_hex_str("", info.one_line_text, &tmp_val);
		key = json_string(info.one_line_text);
		json_object_set_new(res, KEY3_JSON_STR, key);

		if (info.interim_info.tdes_data.test_mode == TDES_MODE_CBC) {
			writeback_hex_str("", info.one_line_text, &val[2]);
			iv = json_string(info.one_line_text);
			json_object_set_new(res, IV_JSON_STR, iv);
		}

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
			tmp_val.len = 8;
			tmp_val.val = vec.cipher_auth.key.val;
			writeback_hex_str("", info.one_line_text, &tmp_val);
			key = json_string(info.one_line_text);
			json_object_set_new(res, KEY1_JSON_STR, key);

			tmp_val.val = vec.cipher_auth.key.val + 8;
			writeback_hex_str("", info.one_line_text, &tmp_val);
			key = json_string(info.one_line_text);
			json_object_set_new(res, KEY2_JSON_STR, key);

			tmp_val.val = vec.cipher_auth.key.val + 16;
			writeback_hex_str("", info.one_line_text, &tmp_val);
			key = json_string(info.one_line_text);
			json_object_set_new(res, KEY3_JSON_STR, key);

			if (info.interim_info.tdes_data.test_mode == TDES_MODE_CBC) {
				writeback_hex_str("", info.one_line_text, &val[2]);
				iv = json_string(info.one_line_text);
				json_object_set_new(res, IV_JSON_STR, iv);
			}

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

int parse_test_tdes_json_init(void)
{
	json_t *type_obj = json_object_get(json_info.json_test_group, TESTTYPE_JSON_STR);
	json_t *algo_obj = json_object_get(json_info.json_vector_set, ALGO_JSON_STR);
	const char *type_str = json_string_value(type_obj);
	const char *algo_str = json_string_value(algo_obj);
	uint32_t i;

	if (json_info.json_test_group) {
		json_t *direction_obj, *opt_obj;
		const char *direction_str;
		int opt_val;

		direction_obj = json_object_get(json_info.json_test_group, DIR_JSON_STR);
		direction_str = json_string_value(direction_obj);

		if (strcmp(direction_str, OP_ENC_JSON_STR) == 0) {
			info.op = FIPS_TEST_ENC_AUTH_GEN;
			info.callbacks = tdes_enc_json_vectors;
		} else if (strcmp(direction_str, OP_DEC_JSON_STR) == 0) {
			info.op = FIPS_TEST_DEC_AUTH_VERIF;
			info.callbacks = tdes_dec_json_vectors;
		} else {
			return -EINVAL;
		}

		opt_obj = json_object_get(json_info.json_test_group, KEYOPT_JSON_STR);
		opt_val = json_integer_value(opt_obj);
		if (opt_val == 2)
			info.interim_info.tdes_data.nb_keys = 2;
		else
			info.interim_info.tdes_data.nb_keys = 3;

		info.interim_callbacks = NULL;
	}

	for (i = 0; i < RTE_DIM(tdes_test_types); i++)
		if (strstr(type_str, tdes_test_types[i].desc)) {
			info.interim_info.tdes_data.test_type =
				tdes_test_types[i].type;
			break;
		}

	if (i >= RTE_DIM(tdes_test_types))
		return -EINVAL;

	for (i = 0; RTE_DIM(tdes_test_modes); i++)
		if (strstr(algo_str, tdes_test_modes[i].desc)) {
			info.interim_info.tdes_data.test_mode =
				tdes_test_modes[i].mode;
			break;
		}

	if (i >= RTE_DIM(tdes_test_modes))
		return -EINVAL;

	switch (info.interim_info.tdes_data.test_type) {
	case TDES_AFT:
		info.parse_writeback = parse_test_tdes_json_writeback;
		break;
	case TDES_MCT:
		info.parse_writeback = parse_test_tdes_mct_json_writeback;
		break;
	default:
		info.parse_writeback = NULL;
	};

	if (info.parse_writeback == NULL)
		return -EINVAL;

	return 0;
}
#endif /* USE_JANSSON */

static int
parse_tdes_interim(const char *key, char *text,
		__rte_unused struct fips_val *val)
{
	if (strstr(key, ENC_STR))
		info.op = FIPS_TEST_ENC_AUTH_GEN;
	else if (strstr(key, DEC_STR))
		info.op = FIPS_TEST_DEC_AUTH_VERIF;
	else if (strstr(key, NK_STR)) {
		if (strcmp(text, "NumKeys = 1") == 0)
			info.interim_info.tdes_data.nb_keys = 1;
		else if (strcmp(text, "NumKeys = 2") == 0)
			info.interim_info.tdes_data.nb_keys = 2;
		else if (strcmp(text, "NumKeys = 3") == 0)
			info.interim_info.tdes_data.nb_keys = 3;
		else
			return -EINVAL;
	} else
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
				if (strstr(line, TEST_TYPE_ECB_KEY))
					info.interim_info.tdes_data.test_mode =
						TDES_MODE_ECB;
				else
					info.interim_info.tdes_data.test_mode =
						TDES_MODE_CBC;
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
