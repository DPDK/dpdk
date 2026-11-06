/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#include <string.h>
#include <time.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef USE_OPENSSL
#include <openssl/bn.h>
#include <openssl/rand.h>
#endif /* USE_OPENSSL */

#include <rte_cryptodev.h>
#include <rte_malloc.h>

#include "fips_validation.h"

#define CONFORMANCE_JSON_STR	"conformance"
#define TESTTYPE_JSON_STR	"testType"
#define CURVE_JSON_STR	"curve"
#define HASH_JSON_STR	"hashAlg"
#define RV_JSON_STR	"randomValue"

#define MSG_JSON_STR	"message"
#define QX_JSON_STR	"qx"
#define QY_JSON_STR	"qy"
#define R_JSON_STR	"r"
#define S_JSON_STR	"s"

#define RV_BUF_LEN (1024/8)
#define RV_BIT_LEN (256)

#ifdef USE_JANSSON
struct {
	uint8_t type;
	const char *desc;
} ecdsa_test_types[] = {
		{ECDSA_AFT, "AFT"}
};

struct {
	enum rte_crypto_auth_algorithm auth;
	const char *desc;
} ecdsa_auth_algs[] = {
		{RTE_CRYPTO_AUTH_SHA1, "SHA-1"},
		{RTE_CRYPTO_AUTH_SHA224, "SHA2-224"},
		{RTE_CRYPTO_AUTH_SHA256, "SHA2-256"},
		{RTE_CRYPTO_AUTH_SHA384, "SHA2-384"},
		{RTE_CRYPTO_AUTH_SHA512, "SHA2-512"},
		{RTE_CRYPTO_AUTH_SHA3_224, "SHA3-224"},
		{RTE_CRYPTO_AUTH_SHA3_256, "SHA3-256"},
		{RTE_CRYPTO_AUTH_SHA3_384, "SHA3-384"},
		{RTE_CRYPTO_AUTH_SHA3_512, "SHA3-512"},
};

struct {
	enum rte_crypto_curve_id curve_id;
	const char *desc;
} ecdsa_curve_ids[] = {
	{RTE_CRYPTO_EC_GROUP_SECP192R1, "P-192"},
	{RTE_CRYPTO_EC_GROUP_SECP224R1, "P-224"},
	{RTE_CRYPTO_EC_GROUP_SECP256R1, "P-256"},
	{RTE_CRYPTO_EC_GROUP_SECP384R1, "P-384"},
	{RTE_CRYPTO_EC_GROUP_SECP521R1, "P-521"},
};

struct {
	uint8_t curve_len;
	const char *desc;
} ecdsa_curve_len[] = {
	{24, "P-192"},
	{28, "P-224"},
	{32, "P-256"},
	{48, "P-384"},
	{66, "P-521"},
};

#ifdef USE_OPENSSL
#define MAX_TRIES 10
static int
prepare_vec_ecdsa(void)
{
	BIGNUM *pkey = NULL, *order = NULL, *r = NULL;
	int ret = -1, j;
	unsigned long pid;

	/* For ECDSA prime fields, order of base points.
	 * Below string array is indexed by starting with first supported
	 * curve (SECP-192R1).
	 */
	static const char * const orderstr[] = {
			"FFFFFFFFFFFFFFFFFFFFFFFF99DEF836146BC9B1B4D22831",
			"",
			"FFFFFFFFFFFFFFFFFFFFFFFFFFFF16A2E0B8F03E13DD29455C5C2A3D",
			"",
			"FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551",
			"FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFC7634D81F4372DDF581A0DB248B0A77AECEC196ACCC52973",
			"1FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFA51868783BF2F966B7FCC0148F709A5D03BB5C9B8899C47AEBB6FB71E91386409"
	};

	/* Seed PRNG */
	if (vec.ecdsa.seed.val) {
		writeback_hex_str("", info.one_line_text, &vec.ecdsa.seed);
		RAND_seed((char *)info.one_line_text, strlen(info.one_line_text));
	} else {
		pid = getpid();
		RAND_seed(&pid, sizeof(pid));
	}

	if (!RAND_status())
		return -1;

	order = BN_new();
	if (!order)
		goto err;

	j = info.interim_info.ecdsa_data.curve_id - RTE_CRYPTO_EC_GROUP_SECP192R1;
	if (!BN_hex2bn(&order, orderstr[j]))
		goto err;

	pkey = BN_new();
	if (!pkey)
		goto err;

	for (j = 0; j < MAX_TRIES; j++) {
		/* pkey should be in [1, order - 1] */
		if (!BN_rand_range(pkey, order))
			goto err;

		if (!BN_is_zero(pkey))
			break;
	}

	if (j == MAX_TRIES)
		goto err;

	parse_uint8_hex_str("", BN_bn2hex(pkey), &vec.ecdsa.pkey);

	r = BN_new();
	if (!r)
		goto err;

	if (info.interim_info.ecdsa_data.random_msg) {
		if (!BN_rand(r, RV_BIT_LEN, 0, 0))
			goto err;

		parse_uint8_hex_str("", BN_bn2hex(r), &vec.ecdsa.seed);
	}

	ret = 0;
err:
	BN_free(order);
	BN_free(pkey);
	BN_free(r);
	return ret;
}

static int
prepare_vec_ecdsa_k(void)
{
	BIGNUM *pkey = NULL, *k = NULL;
	int ret = -1;

	if (!vec.ecdsa.pkey.len)
		return -1;

	pkey = BN_new();
	if (!pkey)
		goto err;

	writeback_hex_str("", info.one_line_text, &vec.ecdsa.pkey);
	ret = BN_hex2bn(&pkey, info.one_line_text);
	if ((uint32_t)ret != strlen(info.one_line_text))
		goto err;

	k = BN_new();
	if (!k)
		goto err;

	if (!BN_sub(k, pkey, BN_value_one()))
		goto err;

	if (BN_is_zero(pkey)) {
		if (!BN_add(k, pkey, BN_value_one()))
			goto err;
	}

	parse_uint8_hex_str("", BN_bn2hex(k), &vec.ecdsa.k);
	ret = 0;
err:
	BN_free(pkey);
	BN_free(k);
	return ret;
}

#else
static int
prepare_vec_ecdsa(void)
{
	/*
	 * Generate ECDSA values.
	 */
	return -ENOTSUP;
}

static int
prepare_vec_ecdsa_k(void)
{
	/*
	 * Generate ECDSA values.
	 */
	return -ENOTSUP;
}
#endif /* USE_OPENSSL */

static int
parse_test_ecdsa_json_interim_writeback(struct fips_val *val)
{
	RTE_SET_USED(val);

	if (info.interim_info.ecdsa_data.random_msg) {
		json_object_set_new(json_info.json_write_group, "conformance",
							json_string("SP800-106"));
	}

	if (info.op == FIPS_TEST_ASYM_SIGGEN) {
		/* For siggen tests, ECDSA values can be created soon after
		 * the test group data are parsed.
		 */
		if (vec.ecdsa.pkey.val) {
			rte_free(vec.ecdsa.pkey.val);
			vec.ecdsa.pkey.val = NULL;
		}

		if (prepare_vec_ecdsa() < 0)
			return -1;

		info.interim_info.ecdsa_data.pubkey_gen = 1;
	}

	return 0;
}

static int
post_test_ecdsa_json_interim_writeback(struct fips_val *val)
{
	RTE_SET_USED(val);

	if (info.op == FIPS_TEST_ASYM_KEYGEN) {
		json_t *obj;

		writeback_hex_str("", info.one_line_text, &vec.ecdsa.qx);
		obj = json_string(info.one_line_text);
		json_object_set_new(json_info.json_write_group, "qx", obj);

		writeback_hex_str("", info.one_line_text, &vec.ecdsa.qy);
		obj = json_string(info.one_line_text);
		json_object_set_new(json_info.json_write_group, "qy", obj);
	}

	return 0;
}

static int
parse_test_ecdsa_json_writeback(struct fips_val *val)
{
	json_t *tcId;

	RTE_SET_USED(val);

	tcId = json_object_get(json_info.json_test_case, "tcId");

	json_info.json_write_case = json_object();
	json_object_set(json_info.json_write_case, "tcId", tcId);

	if (info.op == FIPS_TEST_ASYM_SIGGEN) {
		json_t *obj;

		writeback_hex_str("", info.one_line_text, &vec.ecdsa.r);
		obj = json_string(info.one_line_text);
		json_object_set_new(json_info.json_write_case, "r", obj);

		writeback_hex_str("", info.one_line_text, &vec.ecdsa.s);
		obj = json_string(info.one_line_text);
		json_object_set_new(json_info.json_write_case, "s", obj);

		if (info.interim_info.ecdsa_data.random_msg) {
			writeback_hex_str("", info.one_line_text, &vec.ecdsa.seed);
			obj = json_string(info.one_line_text);
			json_object_set_new(json_info.json_write_case, "randomValue", obj);
			json_object_set_new(json_info.json_write_case, "randomValueLen",
				json_integer(vec.ecdsa.seed.len * 8));
		}
	} else if (info.op == FIPS_TEST_ASYM_SIGVER) {
		if (vec.status == RTE_CRYPTO_OP_STATUS_SUCCESS)
			json_object_set_new(json_info.json_write_case, "testPassed", json_true());
		else
			json_object_set_new(json_info.json_write_case, "testPassed", json_false());
	} else if (info.op == FIPS_TEST_ASYM_KEYGEN) {
		json_t *obj;

		writeback_hex_str("", info.one_line_text, &vec.ecdsa.pkey);
		obj = json_string(info.one_line_text);
		json_object_set_new(json_info.json_write_case, "d", obj);

		writeback_hex_str("", info.one_line_text, &vec.ecdsa.qx);
		obj = json_string(info.one_line_text);
		json_object_set_new(json_info.json_write_case, "qx", obj);

		writeback_hex_str("", info.one_line_text, &vec.ecdsa.qy);
		obj = json_string(info.one_line_text);
		json_object_set_new(json_info.json_write_case, "qy", obj);
	}

	return 0;
}

static int
parse_interim_str(const char *key, char *src, struct fips_val *val)
{
	uint32_t i;

	RTE_SET_USED(val);

	if (strcmp(key, TESTTYPE_JSON_STR) == 0) {
		for (i = 0; i < RTE_DIM(ecdsa_test_types); i++)
			if (strstr(src, ecdsa_test_types[i].desc)) {
				info.parse_writeback = parse_test_ecdsa_json_writeback;
				break;
			}

		if (!info.parse_writeback || i >= RTE_DIM(ecdsa_test_types))
			return -EINVAL;

	} else if (strcmp(key, CURVE_JSON_STR) == 0) {
		for (i = 0; i < RTE_DIM(ecdsa_curve_ids); i++)
			if (strstr(src, ecdsa_curve_ids[i].desc)) {
				info.interim_info.ecdsa_data.curve_id = ecdsa_curve_ids[i].curve_id;
				info.interim_info.ecdsa_data.curve_len =
					ecdsa_curve_len[i].curve_len;
				break;
			}

		if (i >= RTE_DIM(ecdsa_curve_ids))
			return -EINVAL;
	} else if (strcmp(key, HASH_JSON_STR) == 0) {
		for (i = 0; i < RTE_DIM(ecdsa_auth_algs); i++)
			if (strstr(src, ecdsa_auth_algs[i].desc)) {
				info.interim_info.ecdsa_data.auth = ecdsa_auth_algs[i].auth;
				break;
			}

		if (i >= RTE_DIM(ecdsa_auth_algs))
			return -EINVAL;
	} else if (strcmp(key, CONFORMANCE_JSON_STR) == 0) {
		info.interim_info.ecdsa_data.random_msg = 1;
	} else {
		return -EINVAL;
	}

	return 0;
}

static int
parse_siggen_message_str(const char *key, char *src, struct fips_val *val)
{
	int ret = 0;

	parse_uint8_hex_str(key, src, val);
	if (info.interim_info.ecdsa_data.random_msg) {
		ret = fips_test_randomize_message(val, &vec.ecdsa.seed);
		if (ret < 0)
			return ret;
	}

	if (vec.ecdsa.k.val) {
		rte_free(vec.ecdsa.k.val);
		vec.ecdsa.k.val = NULL;
	}

	ret = prepare_vec_ecdsa_k();
	return ret;
}

static int
parse_keygen_tc_str(const char *key, char *src, struct fips_val *val)
{
	RTE_SET_USED(key);
	RTE_SET_USED(src);
	RTE_SET_USED(val);

	if (info.op == FIPS_TEST_ASYM_KEYGEN) {
		if (vec.ecdsa.pkey.val) {
			rte_free(vec.ecdsa.pkey.val);
			vec.ecdsa.pkey.val = NULL;
		}

		if (vec.ecdsa.k.val) {
			rte_free(vec.ecdsa.k.val);
			vec.ecdsa.k.val = NULL;
		}

		if (prepare_vec_ecdsa() < 0)
			return -1;

		if (prepare_vec_ecdsa_k() < 0)
			return -1;

		info.interim_info.ecdsa_data.pubkey_gen = 1;
	}

	return 0;
}

static int
parse_sigver_randomvalue_str(const char *key, char *src, struct fips_val *val)
{
	int ret = 0;

	parse_uint8_hex_str(key, src, val);
	if (info.interim_info.ecdsa_data.random_msg)
		ret = fips_test_randomize_message(&vec.pt, val);

	return ret;
}

struct fips_test_callback ecdsa_interim_json_vectors[] = {
		{TESTTYPE_JSON_STR, parse_interim_str, NULL},
		{CURVE_JSON_STR, parse_interim_str, NULL},
		{HASH_JSON_STR, parse_interim_str, NULL},
		{CONFORMANCE_JSON_STR, parse_interim_str, NULL},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback ecdsa_siggen_json_vectors[] = {
		{MSG_JSON_STR, parse_siggen_message_str, &vec.pt},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback ecdsa_sigver_json_vectors[] = {
		{MSG_JSON_STR, parse_uint8_hex_str, &vec.pt},
		{QX_JSON_STR, parse_uint8_hex_str, &vec.ecdsa.qx},
		{QY_JSON_STR, parse_uint8_hex_str, &vec.ecdsa.qy},
		{R_JSON_STR, parse_uint8_hex_str, &vec.ecdsa.r},
		{S_JSON_STR, parse_uint8_hex_str, &vec.ecdsa.s},
		{RV_JSON_STR, parse_sigver_randomvalue_str, &vec.ecdsa.seed},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback ecdsa_keygen_json_vectors[] = {
		{"tcId", parse_keygen_tc_str, &vec.pt},
		{NULL, NULL, NULL} /**< end pointer */
};

int
parse_test_ecdsa_json_init(void)
{
	json_t *mode_obj = json_object_get(json_info.json_vector_set, "mode");
	const char *mode_str = json_string_value(mode_obj);

	info.callbacks = NULL;
	info.parse_writeback = NULL;
	info.interim_info.ecdsa_data.random_msg = 0;

	info.interim_callbacks = ecdsa_interim_json_vectors;
	info.post_interim_writeback = post_test_ecdsa_json_interim_writeback;
	info.parse_interim_writeback = parse_test_ecdsa_json_interim_writeback;
	if (strcmp(mode_str, "sigGen") == 0) {
		info.op = FIPS_TEST_ASYM_SIGGEN;
		info.callbacks = ecdsa_siggen_json_vectors;
	} else if (strcmp(mode_str, "sigVer") == 0) {
		info.op = FIPS_TEST_ASYM_SIGVER;
		info.callbacks = ecdsa_sigver_json_vectors;
	} else if (strcmp(mode_str, "keyGen") == 0) {
		info.op = FIPS_TEST_ASYM_KEYGEN;
		info.callbacks = ecdsa_keygen_json_vectors;
	} else {
		return -EINVAL;
	}

	return 0;
}
#endif /* USE_JANSSON */
