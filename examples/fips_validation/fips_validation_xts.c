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

#define ALGO_JSON_STR		"algorithm"
#define TESTTYPE_JSON_STR	"testType"
#define DIR_JSON_STR		"direction"
#define KEYLEN_JSON_STR		"keyLen"
#define TWEAKMODE_JSON_STR	"tweakMode"

#define KEY_JSON_STR		"key"
#define DATAUNITLEN_JSON_STR	"dataUnitLen"
#define PAYLOADLEN_JSON_STR	"payloadLen"
#define TWEAKVALUE_JSON_STR	"tweakValue"
#define SEQNUMBER_JSON_STR	"sequenceNumber"
#define PT_JSON_STR	"pt"
#define CT_JSON_STR	"ct"

#define OP_ENC_JSON_STR	"encrypt"
#define OP_DEC_JSON_STR	"decrypt"

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

#ifdef USE_JANSSON
static int
parser_xts_read_keylen(const char *key, char *src, struct fips_val *val)
{
	int ret;

	ret = parser_read_uint32_bit_val(key, src, val);
	if (ret < 0)
		return ret;

	val->len *= 2;
	return 0;
}

static int
parser_xts_read_tweakval(const char *key, char *src, struct fips_val *val)
{
	char num_str[4] = {0};
	int ret;

	if (info.interim_info.xts_data.tweak_mode == XTS_TWEAK_MODE_HEX) {
		ret = parse_uint8_hex_str(key, src, val);
	} else if (info.interim_info.xts_data.tweak_mode == XTS_TWEAK_MODE_NUMBER) {
		snprintf(num_str, RTE_DIM(num_str), "%x", atoi(src));
		ret = parse_uint8_hex_str(key, num_str, val);
	} else {
		ret = -1;
	}

	return ret;
}

struct fips_test_callback xts_dec_json_vectors[] = {
		{KEY_JSON_STR, parse_uint8_known_len_hex_str, &vec.cipher_auth.key},
		{TWEAKVALUE_JSON_STR, parser_xts_read_tweakval, &vec.iv},
		{CT_JSON_STR, parse_uint8_hex_str, &vec.ct},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback xts_interim_json_vectors[] = {
		{KEYLEN_JSON_STR, parser_xts_read_keylen, &vec.cipher_auth.key},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback xts_enc_json_vectors[] = {
		{KEY_JSON_STR, parse_uint8_known_len_hex_str, &vec.cipher_auth.key},
		{TWEAKVALUE_JSON_STR, parser_xts_read_tweakval, &vec.iv},
		{SEQNUMBER_JSON_STR, parser_xts_read_tweakval, &vec.iv},
		{PT_JSON_STR, parse_uint8_hex_str, &vec.pt},
		{NULL, NULL, NULL} /**< end pointer */
};

static int
parse_test_xts_json_writeback(struct fips_val *val)
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

int
parse_test_xts_json_init(void)
{
	if (json_info.json_test_group) {
		json_t *direction_obj, *tweakmode_obj;
		const char *direction_str, *tweakmode_str;

		direction_obj = json_object_get(json_info.json_test_group, DIR_JSON_STR);
		direction_str = json_string_value(direction_obj);

		if (strcmp(direction_str, OP_ENC_JSON_STR) == 0) {
			info.op = FIPS_TEST_ENC_AUTH_GEN;
			info.callbacks = xts_enc_json_vectors;

		} else if (strcmp(direction_str, OP_DEC_JSON_STR) == 0) {
			info.op = FIPS_TEST_DEC_AUTH_VERIF;
			info.callbacks = xts_dec_json_vectors;
		} else {
			return -EINVAL;
		}

		tweakmode_obj = json_object_get(json_info.json_test_group, TWEAKMODE_JSON_STR);
		tweakmode_str = json_string_value(tweakmode_obj);
		if (strcmp(tweakmode_str, "hex") == 0)
			info.interim_info.xts_data.tweak_mode = XTS_TWEAK_MODE_HEX;
		else
			info.interim_info.xts_data.tweak_mode = XTS_TWEAK_MODE_NUMBER;

		info.interim_callbacks = xts_interim_json_vectors;
	}

	info.parse_writeback = parse_test_xts_json_writeback;
	return 0;
}
#endif /* USE_JANSSON */

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
