/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
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

#define TESTTYPE_JSON_STR "testType"
#define CURVE_JSON_STR    "curve"
#define PH_JSON_STR       "preHash"

#define MSG_JSON_STR "message"
#define CTX_JSON_STR "context"
#define Q_JSON_STR	 "q"
#define SIG_JSON_STR "signature"

#ifdef USE_JANSSON
struct {
	uint8_t type;
	const char *desc;
} eddsa_test_types[] = {
	{EDDSA_AFT, "AFT"},
	{EDDSA_BFT, "BFT"}
};

struct {
	enum rte_crypto_curve_id curve_id;
	const char *desc;
} eddsa_curve_ids[] = {
	{RTE_CRYPTO_EC_GROUP_ED25519, "ED-25519"},
	{RTE_CRYPTO_EC_GROUP_ED448, "ED-448"},
};

struct {
	uint8_t curve_len;
	const char *desc;
} eddsa_curve_len[] = {
	{64, "ED-25519"},
	{114, "ED-448"},
};

#ifdef USE_OPENSSL
#define MAX_TRIES 10
static int
prepare_vec_eddsa(void)
{
	BIGNUM *pkey = NULL, *order = NULL;
	int ret = -1, j;
	unsigned long pid;

	/* For EdDSA prime fields, order of base points (RFC 8032 Section 5.1 and 5.2).
	 */
	static const char * const orderstr[] = {
			"7237005577332262213973186563042994240857116359379907606001950938285454250989",
			"181709681073901722637330951972001133588410340171829515070372549795146003961539585716195755291692375963310293709091662304773755859649779",
	};

	pid = getpid();
	RAND_seed(&pid, sizeof(pid));

	if (!RAND_status())
		return -1;

	order = BN_new();
	if (!order)
		goto err;

	j = info.interim_info.eddsa_data.curve_id - RTE_CRYPTO_EC_GROUP_ED25519;
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

	parse_uint8_hex_str("", BN_bn2hex(pkey), &vec.eddsa.pkey);

	ret = 0;
err:
	BN_free(order);
	BN_free(pkey);
	return ret;
}
#else
static int
prepare_vec_eddsa(void)
{
	/*
	 * Generate EdDSA values.
	 */
	return -ENOTSUP;
}
#endif /* USE_OPENSSL */

static int
parse_test_eddsa_json_interim_writeback(struct fips_val *val)
{
	RTE_SET_USED(val);

	if (info.op == FIPS_TEST_ASYM_SIGGEN) {
		/* For siggen tests, EdDSA values can be created soon after
		 * the test group data are parsed.
		 */
		if (vec.eddsa.pkey.val) {
			rte_free(vec.eddsa.pkey.val);
			vec.eddsa.pkey.val = NULL;
		}

		if (prepare_vec_eddsa() < 0)
			return -1;

		info.interim_info.eddsa_data.pubkey_gen = 1;
	}

	return 0;
}

static int
post_test_eddsa_json_interim_writeback(struct fips_val *val)
{
	RTE_SET_USED(val);

	if (info.op == FIPS_TEST_ASYM_KEYGEN) {
		json_t *obj;

		writeback_hex_str("", info.one_line_text, &vec.eddsa.q);
		obj = json_string(info.one_line_text);
		json_object_set_new(json_info.json_write_group, "q", obj);
	}

	return 0;
}

static int
parse_test_eddsa_json_writeback(struct fips_val *val)
{
	json_t *tcId;

	RTE_SET_USED(val);

	tcId = json_object_get(json_info.json_test_case, "tcId");

	json_info.json_write_case = json_object();
	json_object_set(json_info.json_write_case, "tcId", tcId);

	if (info.op == FIPS_TEST_ASYM_SIGGEN) {
		json_t *obj;

		writeback_hex_str("", info.one_line_text, &vec.eddsa.sign);
		obj = json_string(info.one_line_text);
		json_object_set_new(json_info.json_write_case, "signature", obj);
	} else if (info.op == FIPS_TEST_ASYM_SIGVER) {
		if (vec.status == RTE_CRYPTO_OP_STATUS_SUCCESS)
			json_object_set_new(json_info.json_write_case, "testPassed", json_true());
		else
			json_object_set_new(json_info.json_write_case, "testPassed", json_false());
	} else if (info.op == FIPS_TEST_ASYM_KEYGEN) {
		json_t *obj;

		writeback_hex_str("", info.one_line_text, &vec.eddsa.pkey);
		obj = json_string(info.one_line_text);
		json_object_set_new(json_info.json_write_case, "d", obj);

		writeback_hex_str("", info.one_line_text, &vec.eddsa.q);
		obj = json_string(info.one_line_text);
		json_object_set_new(json_info.json_write_case, "q", obj);
	}

	return 0;
}

static int
parse_interim_str(const char *key, char *src, struct fips_val *val)
{
	uint32_t i;

	RTE_SET_USED(val);

	if (strcmp(key, TESTTYPE_JSON_STR) == 0) {
		for (i = 0; i < RTE_DIM(eddsa_test_types); i++)
			if (strstr(src, eddsa_test_types[i].desc)) {
				info.parse_writeback = parse_test_eddsa_json_writeback;
				break;
			}

		if (!info.parse_writeback || i >= RTE_DIM(eddsa_test_types))
			return -EINVAL;

	} else if (strcmp(key, CURVE_JSON_STR) == 0) {
		for (i = 0; i < RTE_DIM(eddsa_curve_ids); i++)
			if (strstr(src, eddsa_curve_ids[i].desc)) {
				info.interim_info.eddsa_data.curve_id = eddsa_curve_ids[i].curve_id;
				info.interim_info.eddsa_data.curve_len =
					eddsa_curve_len[i].curve_len;
				break;
			}

		if (i >= RTE_DIM(eddsa_curve_ids))
			return -EINVAL;
	} else if (strcmp(key, PH_JSON_STR) == 0) {
		info.interim_info.eddsa_data.prehash = false;
	} else {
		return -EINVAL;
	}

	return 0;
}

static int
parse_keygen_tc_str(const char *key, char *src, struct fips_val *val)
{
	RTE_SET_USED(key);
	RTE_SET_USED(src);
	RTE_SET_USED(val);

	if (info.op == FIPS_TEST_ASYM_KEYGEN) {
		if (vec.eddsa.pkey.val) {
			rte_free(vec.eddsa.pkey.val);
			vec.eddsa.pkey.val = NULL;
		}

		if (prepare_vec_eddsa() < 0)
			return -1;

		info.interim_info.eddsa_data.pubkey_gen = 1;
	}

	return 0;
}

struct fips_test_callback eddsa_interim_json_vectors[] = {
		{TESTTYPE_JSON_STR, parse_interim_str, NULL},
		{CURVE_JSON_STR, parse_interim_str, NULL},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback eddsa_siggen_json_vectors[] = {
		{MSG_JSON_STR, parse_uint8_hex_str, &vec.pt},
		{CTX_JSON_STR, parse_uint8_hex_str, &vec.eddsa.ctx},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback eddsa_sigver_json_vectors[] = {
		{MSG_JSON_STR, parse_uint8_hex_str, &vec.pt},
		{Q_JSON_STR, parse_uint8_hex_str, &vec.eddsa.q},
		{SIG_JSON_STR, parse_uint8_hex_str, &vec.eddsa.sign},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback eddsa_keygen_json_vectors[] = {
		{"tcId", parse_keygen_tc_str, &vec.pt},
		{NULL, NULL, NULL} /**< end pointer */
};

int
parse_test_eddsa_json_init(void)
{
	json_t *mode_obj = json_object_get(json_info.json_vector_set, "mode");
	const char *mode_str = json_string_value(mode_obj);

	info.callbacks = NULL;
	info.parse_writeback = NULL;

	info.interim_callbacks = eddsa_interim_json_vectors;
	info.post_interim_writeback = post_test_eddsa_json_interim_writeback;
	info.parse_interim_writeback = parse_test_eddsa_json_interim_writeback;
	if (strcmp(mode_str, "sigGen") == 0) {
		info.op = FIPS_TEST_ASYM_SIGGEN;
		info.callbacks = eddsa_siggen_json_vectors;
	} else if (strcmp(mode_str, "sigVer") == 0) {
		info.op = FIPS_TEST_ASYM_SIGVER;
		info.callbacks = eddsa_sigver_json_vectors;
	} else if (strcmp(mode_str, "keyGen") == 0) {
		info.op = FIPS_TEST_ASYM_KEYGEN;
		info.callbacks = eddsa_keygen_json_vectors;
	} else {
		return -EINVAL;
	}

	return 0;
}
#endif /* USE_JANSSON */
