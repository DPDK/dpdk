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
#define SIGTYPE_JSON_STR "sigType"
#define MOD_JSON_STR	"modulo"
#define HASH_JSON_STR	"hashAlg"
#define SALT_JSON_STR	"saltLen"
#define RV_JSON_STR	"randomValue"
#define E_JSON_STR	"e"
#define N_JSON_STR	"n"

#define SEED_JSON_STR	"seed"
#define MSG_JSON_STR	"message"
#define SIG_JSON_STR	"signature"


#define RV_BUF_LEN (1024/8)
#define RV_BIT_LEN (256)

#ifdef USE_JANSSON
struct {
	uint8_t type;
	const char *desc;
} rsa_test_types[] = {
		{RSA_AFT, "AFT"},
		{RSA_GDT, "GDT"},
		{RSA_KAT, "KAT"},
};

struct {
	enum rte_crypto_auth_algorithm auth;
	const char *desc;
} rsa_auth_algs[] = {
		{RTE_CRYPTO_AUTH_SHA1, "SHA-1"},
		{RTE_CRYPTO_AUTH_SHA224, "SHA2-224"},
		{RTE_CRYPTO_AUTH_SHA256, "SHA2-256"},
		{RTE_CRYPTO_AUTH_SHA384, "SHA2-384"},
		{RTE_CRYPTO_AUTH_SHA512, "SHA2-512"},
};

struct {
	enum rte_crypto_rsa_padding_type padding;
	const char *desc;
} rsa_padding_types[] = {
		{RTE_CRYPTO_RSA_PADDING_NONE, "none"},
		{RTE_CRYPTO_RSA_PADDING_PKCS1_5, "pkcs1v1.5"},
		{RTE_CRYPTO_RSA_PADDING_OAEP, "oaep"},
		{RTE_CRYPTO_RSA_PADDING_PSS, "pss"},
};

#ifdef USE_OPENSSL
static int
prepare_vec_rsa(void)
{
	BIGNUM *p = NULL, *q = NULL, *n = NULL, *d = NULL, *e = NULL;
	BIGNUM *dp = NULL, *dq = NULL, *qinv = NULL;
	BIGNUM *r0, *r1, *r2, *r3, *r4;
	BIGNUM *m = NULL, *r = NULL;
	int bits, ret = -1, i;
	char modbuf[8], *buf;
	BN_CTX *ctx = NULL;
	unsigned long pid;

	/* Seed PRNG */
	if (vec.rsa.seed.val) {
		writeback_hex_str("", info.one_line_text, &vec.rsa.seed);
		RAND_seed((char *)info.one_line_text, strlen(info.one_line_text));
	} else {
		pid = getpid();
		RAND_seed(&pid, sizeof(pid));
	}

	if (!RAND_status())
		return -1;

	/* Check if e is known already */
	if (vec.rsa.e.val) {
		writeback_hex_str("", info.one_line_text, &vec.rsa.e);
		ret = BN_hex2bn(&e, info.one_line_text);
		if ((uint32_t)ret != strlen(info.one_line_text))
			goto err;
	}

	/* BN context initialization */
	ctx = BN_CTX_new();
	if (!ctx)
		goto err;

	BN_CTX_start(ctx);
	r0 = BN_CTX_get(ctx);
	r1 = BN_CTX_get(ctx);
	r2 = BN_CTX_get(ctx);
	r3 = BN_CTX_get(ctx);
	r4 = BN_CTX_get(ctx);
	if (!r4)
		goto err;

	/* Calculate bit length for prime numbers */
	m = BN_new();
	if (!m)
		goto err;

	snprintf(modbuf, sizeof(modbuf), "%d", info.interim_info.rsa_data.modulo);
	if (!BN_dec2bn(&m, modbuf))
		goto err;

	r = BN_new();
	if (!r)
		goto err;

	if (!BN_rshift1(r, m))
		goto err;

	buf = BN_bn2dec(r);
	bits = atoi(buf);

	p = BN_new();
	if (!p)
		goto err;

	q = BN_new();
	if (!q)
		goto err;

	n = BN_new();
	if (!n)
		goto err;

	d = BN_new();
	if (!d)
		goto err;

	/* Generate p and q suitably for RSA */
	for (i = 0; i < 10; i++) {
		uint8_t j = 0;

		if (!BN_generate_prime_ex(p, bits, 0, NULL, NULL, NULL))
			goto err;

		do {
			RAND_add(&j, sizeof(j), 1);
			if (!BN_generate_prime_ex(q, bits, 0, NULL, NULL, NULL))
				goto err;

		} while ((BN_cmp(p, q) == 0) && (j++ < 100));

		if (j >= 100) {
			RTE_LOG(ERR, USER1, "Error: insufficient %d retries to generate q", j);
			goto err;
		}

		/* pq */
		if (!BN_mul(n, p, q, ctx))
			goto err;

		/* p-1 */
		if (!BN_sub(r1, p, BN_value_one()))
			goto err;

		/* q-1 */
		if (!BN_sub(r2, q, BN_value_one()))
			goto err;

		/* (p-1 * q-1) */
		if (!BN_mul(r0, r1, r2, ctx))
			goto err;

		/* gcd(p-1, q-1)*/
		if (!BN_gcd(r3, r1, r2, ctx))
			goto err;

		/* lcm(p-1, q-1) */
		if (!BN_div(r4, r, r0, r3, ctx))
			goto err;

		/* check if div and rem are non-zero */
		if (!r4 || !r)
			goto err;

		/* 0 < e < lcm */
		if (!e) {
			int k = 0;

			e = BN_new();
			do {
				RAND_add(&k, sizeof(k), 1);
				if (!BN_rand(e, 32, 1, 1))
					goto err;

				if (!BN_gcd(r3, e, r4, ctx))
					goto err;

				if (BN_is_one(r3))
					break;
			} while (k++ < 10);

			if (k >= 10) {
				RTE_LOG(ERR, USER1, "Error: insufficient %d retries to generate e",
					k);
				goto err;
			}
		}

		/* (de) mod lcm == 1 */
		if (!BN_mod_inverse(d, e, r4, ctx))
			goto err;

		if (!BN_gcd(r3, r1, e, ctx))
			goto err;

		if (!BN_gcd(r4, r2, e, ctx))
			goto err;

		/* check if gcd(p-1, e) and gcd(q-1, e) are 1 */
		if (BN_is_one(r3) && BN_is_one(r4))
			break;
	}

	if (i >= 10) {
		RTE_LOG(ERR, USER1, "Error: insufficient %d retries to generate p and q", i);
		goto err;
	}

	/* d mod (p-1) */
	dp = BN_new();
	if (!dp)
		goto err;

	if (!BN_mod(dp, d, r1, ctx))
		goto err;

	/* d mod (q-1) */
	dq = BN_new();
	if (!dq)
		goto err;

	if (!BN_mod(dq, d, r2, ctx))
		goto err;

	/* modinv of q and p */
	qinv = BN_new();
	if (!qinv)
		goto err;

	if (!BN_mod_inverse(qinv, q, p, ctx))
		goto err;

	if (info.interim_info.rsa_data.random_msg) {
		if (!BN_generate_prime_ex(r, RV_BIT_LEN, 0, NULL, NULL, NULL))
			goto err;

		parse_uint8_hex_str("", BN_bn2hex(r), &vec.rsa.seed);
	}

	parse_uint8_hex_str("", BN_bn2hex(e), &vec.rsa.e);
	parse_uint8_hex_str("", BN_bn2hex(p), &vec.rsa.p);
	parse_uint8_hex_str("", BN_bn2hex(q), &vec.rsa.q);
	parse_uint8_hex_str("", BN_bn2hex(n), &vec.rsa.n);
	parse_uint8_hex_str("", BN_bn2hex(d), &vec.rsa.d);
	parse_uint8_hex_str("", BN_bn2hex(dp), &vec.rsa.dp);
	parse_uint8_hex_str("", BN_bn2hex(dq), &vec.rsa.dq);
	parse_uint8_hex_str("", BN_bn2hex(qinv), &vec.rsa.qinv);

	ret = 0;
err:
	BN_CTX_end(ctx);
	BN_CTX_free(ctx);
	BN_free(m);
	BN_free(r);
	BN_free(p);
	BN_free(q);
	BN_free(n);
	BN_free(d);
	BN_free(e);
	return ret;
}
#else
static int
prepare_vec_rsa(void)
{
	/*
	 * Generate RSA values.
	 */
	return -ENOTSUP;
}
#endif /* USE_OPENSSL */

static int
parse_test_rsa_json_interim_writeback(struct fips_val *val)
{
	RTE_SET_USED(val);

	if (info.interim_info.rsa_data.random_msg) {
		json_object_set_new(json_info.json_write_group, "conformance",
							json_string("SP800-106"));
	}

	if (info.op == FIPS_TEST_ASYM_SIGGEN) {
		json_t *obj;

		/* For siggen tests, RSA values can be created soon after
		 * the test group data are parsed.
		 */
		if (vec.rsa.e.val) {
			rte_free(vec.rsa.e.val);
			vec.rsa.e.val = NULL;
		}

		if (prepare_vec_rsa() < 0)
			return -1;

		writeback_hex_str("", info.one_line_text, &vec.rsa.n);
		obj = json_string(info.one_line_text);
		json_object_set_new(json_info.json_write_group, "n", obj);

		writeback_hex_str("", info.one_line_text, &vec.rsa.e);
		obj = json_string(info.one_line_text);
		json_object_set_new(json_info.json_write_group, "e", obj);
	}

	return 0;
}

static int
parse_test_rsa_json_writeback(struct fips_val *val)
{
	json_t *tcId;

	RTE_SET_USED(val);

	tcId = json_object_get(json_info.json_test_case, "tcId");

	json_info.json_write_case = json_object();
	json_object_set(json_info.json_write_case, "tcId", tcId);

	if (info.op == FIPS_TEST_ASYM_KEYGEN) {
		json_t *obj;

		writeback_hex_str("", info.one_line_text, &vec.rsa.seed);
		obj = json_string(info.one_line_text);
		json_object_set_new(json_info.json_write_case, "seed", obj);

		writeback_hex_str("", info.one_line_text, &vec.rsa.n);
		obj = json_string(info.one_line_text);
		json_object_set_new(json_info.json_write_case, "n", obj);

		writeback_hex_str("", info.one_line_text, &vec.rsa.e);
		obj = json_string(info.one_line_text);
		json_object_set_new(json_info.json_write_case, "e", obj);

		writeback_hex_str("", info.one_line_text, &vec.rsa.p);
		obj = json_string(info.one_line_text);
		json_object_set_new(json_info.json_write_case, "p", obj);

		writeback_hex_str("", info.one_line_text, &vec.rsa.q);
		obj = json_string(info.one_line_text);
		json_object_set_new(json_info.json_write_case, "q", obj);

		writeback_hex_str("", info.one_line_text, &vec.rsa.d);
		obj = json_string(info.one_line_text);
		json_object_set_new(json_info.json_write_case, "d", obj);
	} else if (info.op == FIPS_TEST_ASYM_SIGGEN) {
		json_t *obj;

		writeback_hex_str("", info.one_line_text, &vec.rsa.signature);
		obj = json_string(info.one_line_text);
		json_object_set_new(json_info.json_write_case, "signature", obj);

		if (info.interim_info.rsa_data.random_msg) {
			writeback_hex_str("", info.one_line_text, &vec.rsa.seed);
			obj = json_string(info.one_line_text);
			json_object_set_new(json_info.json_write_case, "randomValue", obj);
			json_object_set_new(json_info.json_write_case, "randomValueLen",
				json_integer(vec.rsa.seed.len * 8));
		}
	} else if (info.op == FIPS_TEST_ASYM_SIGVER) {
		if (vec.status == RTE_CRYPTO_OP_STATUS_SUCCESS)
			json_object_set_new(json_info.json_write_case, "testPassed", json_true());
		else
			json_object_set_new(json_info.json_write_case, "testPassed", json_false());
	}

	return 0;
}

static int
parse_interim_str(const char *key, char *src, struct fips_val *val)
{
	uint32_t i;

	RTE_SET_USED(val);

	if (strcmp(key, SIGTYPE_JSON_STR) == 0) {
		for (i = 0; i < RTE_DIM(rsa_padding_types); i++)
			if (strstr(src, rsa_padding_types[i].desc)) {
				info.interim_info.rsa_data.padding = rsa_padding_types[i].padding;
				break;
			}

		if (i >= RTE_DIM(rsa_padding_types))
			return -EINVAL;

	}  else if (strcmp(key, MOD_JSON_STR) == 0) {
		info.interim_info.rsa_data.modulo = atoi(src);
	} else if (strcmp(key, HASH_JSON_STR) == 0) {
		for (i = 0; i < RTE_DIM(rsa_auth_algs); i++)
			if (strstr(src, rsa_auth_algs[i].desc)) {
				info.interim_info.rsa_data.auth = rsa_auth_algs[i].auth;
				break;
			}

		if (i >= RTE_DIM(rsa_auth_algs))
			return -EINVAL;

	}  else if (strcmp(key, CONFORMANCE_JSON_STR) == 0) {
		info.interim_info.rsa_data.random_msg = 1;
	}  else if (strcmp(key, SALT_JSON_STR) == 0) {
		info.interim_info.rsa_data.saltlen = atoi(src);
	} else if (strcmp(key, TESTTYPE_JSON_STR) == 0) {
		for (i = 0; i < RTE_DIM(rsa_test_types); i++)
			if (strstr(src, rsa_test_types[i].desc)) {
				info.parse_writeback = parse_test_rsa_json_writeback;
				break;
			}

		if (!info.parse_writeback || i >= RTE_DIM(rsa_test_types))
			return -EINVAL;

	} else {
		return -EINVAL;
	}

	return 0;
}

static int
parse_keygen_e_str(const char *key, char *src, struct fips_val *val)
{
	parse_uint8_hex_str(key, src, val);

	/* For keygen tests, key "e" can be the end of input data
	 * to generate RSA values.
	 */
	return prepare_vec_rsa();
}

/*
 * Message randomization function as per NIST SP 800-106.
 */
int
fips_test_randomize_message(struct fips_val *msg, struct fips_val *rand)
{
	uint8_t m[FIPS_TEST_JSON_BUF_LEN], rv[RV_BUF_LEN];
	uint32_t m_bitlen, rv_bitlen, count, remain, i, j;
	uint16_t rv_len;

	if (!msg->val || !rand->val || rand->len > RV_BUF_LEN
		|| msg->len > FIPS_TEST_JSON_BUF_LEN)
		return -EINVAL;

	memset(rv, 0, sizeof(rv));
	memcpy(rv, rand->val, rand->len);
	rv_bitlen = rand->len * 8;
	rv_len = rand->len;

	memset(m, 0, sizeof(m));
	memcpy(m, msg->val, msg->len);
	m_bitlen = msg->len * 8;

	if (m_bitlen >= (rv_bitlen - 1)) {
		m[msg->len] = 0x80;
		m_bitlen += 8;
	} else {
		m[msg->len] = 0x80;
		m_bitlen += (rv_bitlen - m_bitlen - 8);
	}

	count = m_bitlen / rv_bitlen;
	remain = m_bitlen % rv_bitlen;
	for (i = 0; i < count * rv_len; i++)
		m[i] ^= rv[i % rv_len];

	for (j = 0; j < remain / 8; j++)
		m[i + j] ^= rv[j];

	m[i + j] = ((uint8_t *)&rv_bitlen)[0];
	m[i + j + 1] = (((uint8_t *)&rv_bitlen)[1] >> 8) & 0xFF;

	rte_free(msg->val);
	msg->len = (rv_bitlen + m_bitlen + 16) / 8;
	msg->val = rte_zmalloc(NULL, msg->len, 0);
	if (!msg->val)
		return -EPERM;

	memcpy(msg->val, rv, rv_len);
	memcpy(&msg->val[rv_len], m, (m_bitlen + 16) / 8);
	return 0;
}

static int
parse_siggen_message_str(const char *key, char *src, struct fips_val *val)
{
	int ret = 0;

	parse_uint8_hex_str(key, src, val);
	if (info.interim_info.rsa_data.random_msg)
		ret = fips_test_randomize_message(val, &vec.rsa.seed);

	return ret;
}

static int
parse_sigver_randomvalue_str(const char *key, char *src, struct fips_val *val)
{
	int ret = 0;

	parse_uint8_hex_str(key, src, val);
	if (info.interim_info.rsa_data.random_msg)
		ret = fips_test_randomize_message(&vec.pt, val);

	return ret;
}

struct fips_test_callback rsa_keygen_interim_json_vectors[] = {
		{MOD_JSON_STR, parse_interim_str, NULL},
		{HASH_JSON_STR, parse_interim_str, NULL},
		{TESTTYPE_JSON_STR, parse_interim_str, NULL},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback rsa_siggen_interim_json_vectors[] = {
		{SIGTYPE_JSON_STR, parse_interim_str, NULL},
		{MOD_JSON_STR, parse_interim_str, NULL},
		{HASH_JSON_STR, parse_interim_str, NULL},
		{CONFORMANCE_JSON_STR, parse_interim_str, NULL},
		{SALT_JSON_STR, parse_interim_str, NULL},
		{TESTTYPE_JSON_STR, parse_interim_str, NULL},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback rsa_sigver_interim_json_vectors[] = {
		{SIGTYPE_JSON_STR, parse_interim_str, NULL},
		{MOD_JSON_STR, parse_interim_str, NULL},
		{HASH_JSON_STR, parse_interim_str, NULL},
		{CONFORMANCE_JSON_STR, parse_interim_str, NULL},
		{SALT_JSON_STR, parse_interim_str, NULL},
		{N_JSON_STR, parse_uint8_hex_str, &vec.rsa.n},
		{E_JSON_STR, parse_uint8_hex_str, &vec.rsa.e},
		{TESTTYPE_JSON_STR, parse_interim_str, NULL},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback rsa_keygen_json_vectors[] = {
		{SEED_JSON_STR, parse_uint8_hex_str, &vec.rsa.seed},
		{E_JSON_STR, parse_keygen_e_str, &vec.rsa.e},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback rsa_siggen_json_vectors[] = {
		{MSG_JSON_STR, parse_siggen_message_str, &vec.pt},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback rsa_sigver_json_vectors[] = {
		{MSG_JSON_STR, parse_uint8_hex_str, &vec.pt},
		{SIG_JSON_STR, parse_uint8_hex_str, &vec.rsa.signature},
		{RV_JSON_STR, parse_sigver_randomvalue_str, &vec.rsa.seed},
		{NULL, NULL, NULL} /**< end pointer */
};

int
parse_test_rsa_json_init(void)
{
	json_t *keyfmt_obj = json_object_get(json_info.json_vector_set, "keyFormat");
	json_t *mode_obj = json_object_get(json_info.json_vector_set, "mode");
	const char *keyfmt_str = json_string_value(keyfmt_obj);
	const char *mode_str = json_string_value(mode_obj);

	info.callbacks = NULL;
	info.parse_writeback = NULL;
	info.interim_callbacks = NULL;
	info.parse_interim_writeback = NULL;
	info.interim_info.rsa_data.random_msg = 0;

	if (strcmp(mode_str, "keyGen") == 0) {
		info.op = FIPS_TEST_ASYM_KEYGEN;
		info.callbacks = rsa_keygen_json_vectors;
		info.interim_callbacks = rsa_keygen_interim_json_vectors;
	} else if (strcmp(mode_str, "sigGen") == 0) {
		info.op = FIPS_TEST_ASYM_SIGGEN;
		info.callbacks = rsa_siggen_json_vectors;
		info.interim_callbacks = rsa_siggen_interim_json_vectors;
		info.parse_interim_writeback = parse_test_rsa_json_interim_writeback;
	} else if (strcmp(mode_str, "sigVer") == 0) {
		info.op = FIPS_TEST_ASYM_SIGVER;
		info.callbacks = rsa_sigver_json_vectors;
		info.interim_callbacks = rsa_sigver_interim_json_vectors;
		info.parse_interim_writeback = parse_test_rsa_json_interim_writeback;
	} else {
		return -EINVAL;
	}

	info.interim_info.rsa_data.privkey = RTE_RSA_KEY_TYPE_QT;
	if (keyfmt_str != NULL && strcmp(keyfmt_str, "standard") == 0)
		info.interim_info.rsa_data.privkey = RTE_RSA_KEY_TYPE_EXP;

	return 0;
}

#endif /* USE_JANSSON */
