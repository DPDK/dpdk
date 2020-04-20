/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

/*
 * Security Associations
 */
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>

#include <rte_memzone.h>
#include <rte_crypto.h>
#include <rte_security.h>
#include <rte_cryptodev.h>
#include <rte_byteorder.h>
#include <rte_errno.h>
#include <rte_ip.h>
#include <rte_random.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>

#include "ipsec.h"
#include "esp.h"
#include "parser.h"
#include "sad.h"

#define IPDEFTTL 64

#define IP4_FULL_MASK (sizeof(((struct ip_addr *)NULL)->ip.ip4) * CHAR_BIT)

#define IP6_FULL_MASK (sizeof(((struct ip_addr *)NULL)->ip.ip6.ip6) * CHAR_BIT)

#define MBUF_NO_SEC_OFFLOAD(m) ((m->ol_flags & PKT_RX_SEC_OFFLOAD) == 0)

struct supported_cipher_algo {
	const char *keyword;
	enum rte_crypto_cipher_algorithm algo;
	uint16_t iv_len;
	uint16_t block_size;
	uint16_t key_len;
};

struct supported_auth_algo {
	const char *keyword;
	enum rte_crypto_auth_algorithm algo;
	uint16_t digest_len;
	uint16_t key_len;
	uint8_t key_not_req;
};

struct supported_aead_algo {
	const char *keyword;
	enum rte_crypto_aead_algorithm algo;
	uint16_t iv_len;
	uint16_t block_size;
	uint16_t digest_len;
	uint16_t key_len;
	uint8_t aad_len;
};


const struct supported_cipher_algo cipher_algos[] = {
	{
		.keyword = "null",
		.algo = RTE_CRYPTO_CIPHER_NULL,
		.iv_len = 0,
		.block_size = 4,
		.key_len = 0
	},
	{
		.keyword = "aes-128-cbc",
		.algo = RTE_CRYPTO_CIPHER_AES_CBC,
		.iv_len = 16,
		.block_size = 16,
		.key_len = 16
	},
	{
		.keyword = "aes-192-cbc",
		.algo = RTE_CRYPTO_CIPHER_AES_CBC,
		.iv_len = 16,
		.block_size = 16,
		.key_len = 24
	},
	{
		.keyword = "aes-256-cbc",
		.algo = RTE_CRYPTO_CIPHER_AES_CBC,
		.iv_len = 16,
		.block_size = 16,
		.key_len = 32
	},
	{
		.keyword = "aes-128-ctr",
		.algo = RTE_CRYPTO_CIPHER_AES_CTR,
		.iv_len = 8,
		.block_size = 4,
		.key_len = 20
	},
	{
		.keyword = "3des-cbc",
		.algo = RTE_CRYPTO_CIPHER_3DES_CBC,
		.iv_len = 8,
		.block_size = 8,
		.key_len = 24
	}
};

const struct supported_auth_algo auth_algos[] = {
	{
		.keyword = "null",
		.algo = RTE_CRYPTO_AUTH_NULL,
		.digest_len = 0,
		.key_len = 0,
		.key_not_req = 1
	},
	{
		.keyword = "sha1-hmac",
		.algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
		.digest_len = 12,
		.key_len = 20
	},
	{
		.keyword = "sha256-hmac",
		.algo = RTE_CRYPTO_AUTH_SHA256_HMAC,
		.digest_len = 16,
		.key_len = 32
	}
};

const struct supported_aead_algo aead_algos[] = {
	{
		.keyword = "aes-128-gcm",
		.algo = RTE_CRYPTO_AEAD_AES_GCM,
		.iv_len = 8,
		.block_size = 4,
		.key_len = 20,
		.digest_len = 16,
		.aad_len = 8,
	},
	{
		.keyword = "aes-192-gcm",
		.algo = RTE_CRYPTO_AEAD_AES_GCM,
		.iv_len = 8,
		.block_size = 4,
		.key_len = 28,
		.digest_len = 16,
		.aad_len = 8,
	},
	{
		.keyword = "aes-256-gcm",
		.algo = RTE_CRYPTO_AEAD_AES_GCM,
		.iv_len = 8,
		.block_size = 4,
		.key_len = 36,
		.digest_len = 16,
		.aad_len = 8,
	}
};

#define SA_INIT_NB	128

static uint32_t nb_crypto_sessions;
struct ipsec_sa *sa_out;
uint32_t nb_sa_out;
static uint32_t sa_out_sz;
static struct ipsec_sa_cnt sa_out_cnt;

struct ipsec_sa *sa_in;
uint32_t nb_sa_in;
static uint32_t sa_in_sz;
static struct ipsec_sa_cnt sa_in_cnt;

static const struct supported_cipher_algo *
find_match_cipher_algo(const char *cipher_keyword)
{
	size_t i;

	for (i = 0; i < RTE_DIM(cipher_algos); i++) {
		const struct supported_cipher_algo *algo =
			&cipher_algos[i];

		if (strcmp(cipher_keyword, algo->keyword) == 0)
			return algo;
	}

	return NULL;
}

static const struct supported_auth_algo *
find_match_auth_algo(const char *auth_keyword)
{
	size_t i;

	for (i = 0; i < RTE_DIM(auth_algos); i++) {
		const struct supported_auth_algo *algo =
			&auth_algos[i];

		if (strcmp(auth_keyword, algo->keyword) == 0)
			return algo;
	}

	return NULL;
}

static const struct supported_aead_algo *
find_match_aead_algo(const char *aead_keyword)
{
	size_t i;

	for (i = 0; i < RTE_DIM(aead_algos); i++) {
		const struct supported_aead_algo *algo =
			&aead_algos[i];

		if (strcmp(aead_keyword, algo->keyword) == 0)
			return algo;
	}

	return NULL;
}

/** parse_key_string
 *  parse x:x:x:x.... hex number key string into uint8_t *key
 *  return:
 *  > 0: number of bytes parsed
 *  0:   failed
 */
static uint32_t
parse_key_string(const char *key_str, uint8_t *key)
{
	const char *pt_start = key_str, *pt_end = key_str;
	uint32_t nb_bytes = 0;

	while (pt_end != NULL) {
		char sub_str[3] = {0};

		pt_end = strchr(pt_start, ':');

		if (pt_end == NULL) {
			if (strlen(pt_start) > 2)
				return 0;
			strncpy(sub_str, pt_start, 2);
		} else {
			if (pt_end - pt_start > 2)
				return 0;

			strncpy(sub_str, pt_start, pt_end - pt_start);
			pt_start = pt_end + 1;
		}

		key[nb_bytes++] = strtol(sub_str, NULL, 16);
	}

	return nb_bytes;
}

static int
extend_sa_arr(struct ipsec_sa **sa_tbl, uint32_t cur_cnt, uint32_t *cur_sz)
{
	if (*sa_tbl == NULL) {
		*sa_tbl = calloc(SA_INIT_NB, sizeof(struct ipsec_sa));
		if (*sa_tbl == NULL)
			return -1;
		*cur_sz = SA_INIT_NB;
		return 0;
	}

	if (cur_cnt >= *cur_sz) {
		*sa_tbl = realloc(*sa_tbl,
			*cur_sz * sizeof(struct ipsec_sa) * 2);
		if (*sa_tbl == NULL)
			return -1;
		/* clean reallocated extra space */
		memset(&(*sa_tbl)[*cur_sz], 0,
			*cur_sz * sizeof(struct ipsec_sa));
		*cur_sz *= 2;
	}

	return 0;
}

void
parse_sa_tokens(char **tokens, uint32_t n_tokens,
	struct parse_status *status)
{
	struct ipsec_sa *rule = NULL;
	struct rte_ipsec_session *ips;
	uint32_t ti; /*token index*/
	uint32_t *ri /*rule index*/;
	struct ipsec_sa_cnt *sa_cnt;
	uint32_t cipher_algo_p = 0;
	uint32_t auth_algo_p = 0;
	uint32_t aead_algo_p = 0;
	uint32_t src_p = 0;
	uint32_t dst_p = 0;
	uint32_t mode_p = 0;
	uint32_t type_p = 0;
	uint32_t portid_p = 0;
	uint32_t fallback_p = 0;
	int16_t status_p = 0;

	if (strcmp(tokens[0], "in") == 0) {
		ri = &nb_sa_in;
		sa_cnt = &sa_in_cnt;
		if (extend_sa_arr(&sa_in, nb_sa_in, &sa_in_sz) < 0)
			return;
		rule = &sa_in[*ri];
		rule->direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS;
	} else {
		ri = &nb_sa_out;
		sa_cnt = &sa_out_cnt;
		if (extend_sa_arr(&sa_out, nb_sa_out, &sa_out_sz) < 0)
			return;
		rule = &sa_out[*ri];
		rule->direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS;
	}

	/* spi number */
	APP_CHECK_TOKEN_IS_NUM(tokens, 1, status);
	if (status->status < 0)
		return;
	if (atoi(tokens[1]) == INVALID_SPI)
		return;
	rule->spi = atoi(tokens[1]);
	rule->portid = UINT16_MAX;
	ips = ipsec_get_primary_session(rule);

	for (ti = 2; ti < n_tokens; ti++) {
		if (strcmp(tokens[ti], "mode") == 0) {
			APP_CHECK_PRESENCE(mode_p, tokens[ti], status);
			if (status->status < 0)
				return;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			if (strcmp(tokens[ti], "ipv4-tunnel") == 0) {
				sa_cnt->nb_v4++;
				rule->flags = IP4_TUNNEL;
			} else if (strcmp(tokens[ti], "ipv6-tunnel") == 0) {
				sa_cnt->nb_v6++;
				rule->flags = IP6_TUNNEL;
			} else if (strcmp(tokens[ti], "transport") == 0) {
				sa_cnt->nb_v4++;
				sa_cnt->nb_v6++;
				rule->flags = TRANSPORT;
			} else {
				APP_CHECK(0, status, "unrecognized "
					"input \"%s\"", tokens[ti]);
				return;
			}

			mode_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "cipher_algo") == 0) {
			const struct supported_cipher_algo *algo;
			uint32_t key_len;

			APP_CHECK_PRESENCE(cipher_algo_p, tokens[ti],
				status);
			if (status->status < 0)
				return;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			algo = find_match_cipher_algo(tokens[ti]);

			APP_CHECK(algo != NULL, status, "unrecognized "
				"input \"%s\"", tokens[ti]);

			if (status->status < 0)
				return;

			rule->cipher_algo = algo->algo;
			rule->block_size = algo->block_size;
			rule->iv_len = algo->iv_len;
			rule->cipher_key_len = algo->key_len;

			/* for NULL algorithm, no cipher key required */
			if (rule->cipher_algo == RTE_CRYPTO_CIPHER_NULL) {
				cipher_algo_p = 1;
				continue;
			}

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			APP_CHECK(strcmp(tokens[ti], "cipher_key") == 0,
				status, "unrecognized input \"%s\", "
				"expect \"cipher_key\"", tokens[ti]);
			if (status->status < 0)
				return;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			key_len = parse_key_string(tokens[ti],
				rule->cipher_key);
			APP_CHECK(key_len == rule->cipher_key_len, status,
				"unrecognized input \"%s\"", tokens[ti]);
			if (status->status < 0)
				return;

			if (algo->algo == RTE_CRYPTO_CIPHER_AES_CBC ||
				algo->algo == RTE_CRYPTO_CIPHER_3DES_CBC)
				rule->salt = (uint32_t)rte_rand();

			if (algo->algo == RTE_CRYPTO_CIPHER_AES_CTR) {
				key_len -= 4;
				rule->cipher_key_len = key_len;
				memcpy(&rule->salt,
					&rule->cipher_key[key_len], 4);
			}

			cipher_algo_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "auth_algo") == 0) {
			const struct supported_auth_algo *algo;
			uint32_t key_len;

			APP_CHECK_PRESENCE(auth_algo_p, tokens[ti],
				status);
			if (status->status < 0)
				return;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			algo = find_match_auth_algo(tokens[ti]);
			APP_CHECK(algo != NULL, status, "unrecognized "
				"input \"%s\"", tokens[ti]);

			if (status->status < 0)
				return;

			rule->auth_algo = algo->algo;
			rule->auth_key_len = algo->key_len;
			rule->digest_len = algo->digest_len;

			/* NULL algorithm and combined algos do not
			 * require auth key
			 */
			if (algo->key_not_req) {
				auth_algo_p = 1;
				continue;
			}

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			APP_CHECK(strcmp(tokens[ti], "auth_key") == 0,
				status, "unrecognized input \"%s\", "
				"expect \"auth_key\"", tokens[ti]);
			if (status->status < 0)
				return;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			key_len = parse_key_string(tokens[ti],
				rule->auth_key);
			APP_CHECK(key_len == rule->auth_key_len, status,
				"unrecognized input \"%s\"", tokens[ti]);
			if (status->status < 0)
				return;

			auth_algo_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "aead_algo") == 0) {
			const struct supported_aead_algo *algo;
			uint32_t key_len;

			APP_CHECK_PRESENCE(aead_algo_p, tokens[ti],
				status);
			if (status->status < 0)
				return;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			algo = find_match_aead_algo(tokens[ti]);

			APP_CHECK(algo != NULL, status, "unrecognized "
				"input \"%s\"", tokens[ti]);

			if (status->status < 0)
				return;

			rule->aead_algo = algo->algo;
			rule->cipher_key_len = algo->key_len;
			rule->digest_len = algo->digest_len;
			rule->aad_len = algo->aad_len;
			rule->block_size = algo->block_size;
			rule->iv_len = algo->iv_len;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			APP_CHECK(strcmp(tokens[ti], "aead_key") == 0,
				status, "unrecognized input \"%s\", "
				"expect \"aead_key\"", tokens[ti]);
			if (status->status < 0)
				return;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			key_len = parse_key_string(tokens[ti],
				rule->cipher_key);
			APP_CHECK(key_len == rule->cipher_key_len, status,
				"unrecognized input \"%s\"", tokens[ti]);
			if (status->status < 0)
				return;

			key_len -= 4;
			rule->cipher_key_len = key_len;
			memcpy(&rule->salt,
				&rule->cipher_key[key_len], 4);

			aead_algo_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "src") == 0) {
			APP_CHECK_PRESENCE(src_p, tokens[ti], status);
			if (status->status < 0)
				return;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			if (IS_IP4_TUNNEL(rule->flags)) {
				struct in_addr ip;

				APP_CHECK(parse_ipv4_addr(tokens[ti],
					&ip, NULL) == 0, status,
					"unrecognized input \"%s\", "
					"expect valid ipv4 addr",
					tokens[ti]);
				if (status->status < 0)
					return;
				rule->src.ip.ip4 = rte_bswap32(
					(uint32_t)ip.s_addr);
			} else if (IS_IP6_TUNNEL(rule->flags)) {
				struct in6_addr ip;

				APP_CHECK(parse_ipv6_addr(tokens[ti], &ip,
					NULL) == 0, status,
					"unrecognized input \"%s\", "
					"expect valid ipv6 addr",
					tokens[ti]);
				if (status->status < 0)
					return;
				memcpy(rule->src.ip.ip6.ip6_b,
					ip.s6_addr, 16);
			} else if (IS_TRANSPORT(rule->flags)) {
				APP_CHECK(0, status, "unrecognized input "
					"\"%s\"", tokens[ti]);
				return;
			}

			src_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "dst") == 0) {
			APP_CHECK_PRESENCE(dst_p, tokens[ti], status);
			if (status->status < 0)
				return;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			if (IS_IP4_TUNNEL(rule->flags)) {
				struct in_addr ip;

				APP_CHECK(parse_ipv4_addr(tokens[ti],
					&ip, NULL) == 0, status,
					"unrecognized input \"%s\", "
					"expect valid ipv4 addr",
					tokens[ti]);
				if (status->status < 0)
					return;
				rule->dst.ip.ip4 = rte_bswap32(
					(uint32_t)ip.s_addr);
			} else if (IS_IP6_TUNNEL(rule->flags)) {
				struct in6_addr ip;

				APP_CHECK(parse_ipv6_addr(tokens[ti], &ip,
					NULL) == 0, status,
					"unrecognized input \"%s\", "
					"expect valid ipv6 addr",
					tokens[ti]);
				if (status->status < 0)
					return;
				memcpy(rule->dst.ip.ip6.ip6_b, ip.s6_addr, 16);
			} else if (IS_TRANSPORT(rule->flags)) {
				APP_CHECK(0, status, "unrecognized "
					"input \"%s\"",	tokens[ti]);
				return;
			}

			dst_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "type") == 0) {
			APP_CHECK_PRESENCE(type_p, tokens[ti], status);
			if (status->status < 0)
				return;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			if (strcmp(tokens[ti], "inline-crypto-offload") == 0)
				ips->type =
					RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO;
			else if (strcmp(tokens[ti],
					"inline-protocol-offload") == 0)
				ips->type =
				RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL;
			else if (strcmp(tokens[ti],
					"lookaside-protocol-offload") == 0)
				ips->type =
				RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL;
			else if (strcmp(tokens[ti], "no-offload") == 0)
				ips->type = RTE_SECURITY_ACTION_TYPE_NONE;
			else if (strcmp(tokens[ti], "cpu-crypto") == 0)
				ips->type = RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO;
			else {
				APP_CHECK(0, status, "Invalid input \"%s\"",
						tokens[ti]);
				return;
			}

			type_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "port_id") == 0) {
			APP_CHECK_PRESENCE(portid_p, tokens[ti], status);
			if (status->status < 0)
				return;
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;
			if (rule->portid == UINT16_MAX)
				rule->portid = atoi(tokens[ti]);
			else if (rule->portid != atoi(tokens[ti])) {
				APP_CHECK(0, status,
					"portid %s not matching with already assigned portid %u",
					tokens[ti], rule->portid);
				return;
			}
			portid_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "fallback") == 0) {
			struct rte_ipsec_session *fb;

			APP_CHECK(app_sa_prm.enable, status, "Fallback session "
				"not allowed for legacy mode.");
			if (status->status < 0)
				return;
			APP_CHECK(ips->type ==
				RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO, status,
				"Fallback session allowed if primary session "
				"is of type inline-crypto-offload only.");
			if (status->status < 0)
				return;
			APP_CHECK(rule->direction ==
				RTE_SECURITY_IPSEC_SA_DIR_INGRESS, status,
				"Fallback session not allowed for egress "
				"rule");
			if (status->status < 0)
				return;
			APP_CHECK_PRESENCE(fallback_p, tokens[ti], status);
			if (status->status < 0)
				return;
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;
			fb = ipsec_get_fallback_session(rule);
			if (strcmp(tokens[ti], "lookaside-none") == 0)
				fb->type = RTE_SECURITY_ACTION_TYPE_NONE;
			else if (strcmp(tokens[ti], "cpu-crypto") == 0)
				fb->type = RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO;
			else {
				APP_CHECK(0, status, "unrecognized fallback "
					"type %s.", tokens[ti]);
				return;
			}

			rule->fallback_sessions = 1;
			nb_crypto_sessions++;
			fallback_p = 1;
			continue;
		}
		if (strcmp(tokens[ti], "flow-direction") == 0) {
			switch (ips->type) {
			case RTE_SECURITY_ACTION_TYPE_NONE:
			case RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO:
				rule->fdir_flag = 1;
				INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
				if (status->status < 0)
					return;
				if (rule->portid == UINT16_MAX)
					rule->portid = atoi(tokens[ti]);
				else if (rule->portid != atoi(tokens[ti])) {
					APP_CHECK(0, status,
						"portid %s not matching with already assigned portid %u",
						tokens[ti], rule->portid);
					return;
				}
				INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
				if (status->status < 0)
					return;
				rule->fdir_qid = atoi(tokens[ti]);
				/* validating portid and queueid */
				status_p = check_flow_params(rule->portid,
						rule->fdir_qid);
				if (status_p < 0) {
					printf("port id %u / queue id %u is "
						"not valid\n", rule->portid,
						 rule->fdir_qid);
				}
				break;
			case RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO:
			case RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL:
			case RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL:
			default:
				APP_CHECK(0, status,
					"flow director not supported for security session type %d",
					ips->type);
				return;
			}
			continue;
		}

		/* unrecognizeable input */
		APP_CHECK(0, status, "unrecognized input \"%s\"",
			tokens[ti]);
		return;
	}

	if (aead_algo_p) {
		APP_CHECK(cipher_algo_p == 0, status,
				"AEAD used, no need for cipher options");
		if (status->status < 0)
			return;

		APP_CHECK(auth_algo_p == 0, status,
				"AEAD used, no need for auth options");
		if (status->status < 0)
			return;
	} else {
		APP_CHECK(cipher_algo_p == 1, status, "missing cipher or AEAD options");
		if (status->status < 0)
			return;

		APP_CHECK(auth_algo_p == 1, status, "missing auth or AEAD options");
		if (status->status < 0)
			return;
	}

	APP_CHECK(mode_p == 1, status, "missing mode option");
	if (status->status < 0)
		return;

	if ((ips->type != RTE_SECURITY_ACTION_TYPE_NONE && ips->type !=
			RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO) && (portid_p == 0))
		printf("Missing portid option, falling back to non-offload\n");

	if (!type_p || (!portid_p && ips->type !=
			RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)) {
		ips->type = RTE_SECURITY_ACTION_TYPE_NONE;
	}

	nb_crypto_sessions++;
	*ri = *ri + 1;
}

static void
print_one_sa_rule(const struct ipsec_sa *sa, int inbound)
{
	uint32_t i;
	uint8_t a, b, c, d;
	const struct rte_ipsec_session *ips;
	const struct rte_ipsec_session *fallback_ips;

	printf("\tspi_%s(%3u):", inbound?"in":"out", sa->spi);

	for (i = 0; i < RTE_DIM(cipher_algos); i++) {
		if (cipher_algos[i].algo == sa->cipher_algo &&
				cipher_algos[i].key_len == sa->cipher_key_len) {
			printf("%s ", cipher_algos[i].keyword);
			break;
		}
	}

	for (i = 0; i < RTE_DIM(auth_algos); i++) {
		if (auth_algos[i].algo == sa->auth_algo) {
			printf("%s ", auth_algos[i].keyword);
			break;
		}
	}

	for (i = 0; i < RTE_DIM(aead_algos); i++) {
		if (aead_algos[i].algo == sa->aead_algo &&
				aead_algos[i].key_len-4 == sa->cipher_key_len) {
			printf("%s ", aead_algos[i].keyword);
			break;
		}
	}

	printf("mode:");

	switch (WITHOUT_TRANSPORT_VERSION(sa->flags)) {
	case IP4_TUNNEL:
		printf("IP4Tunnel ");
		uint32_t_to_char(sa->src.ip.ip4, &a, &b, &c, &d);
		printf("%hhu.%hhu.%hhu.%hhu ", d, c, b, a);
		uint32_t_to_char(sa->dst.ip.ip4, &a, &b, &c, &d);
		printf("%hhu.%hhu.%hhu.%hhu", d, c, b, a);
		break;
	case IP6_TUNNEL:
		printf("IP6Tunnel ");
		for (i = 0; i < 16; i++) {
			if (i % 2 && i != 15)
				printf("%.2x:", sa->src.ip.ip6.ip6_b[i]);
			else
				printf("%.2x", sa->src.ip.ip6.ip6_b[i]);
		}
		printf(" ");
		for (i = 0; i < 16; i++) {
			if (i % 2 && i != 15)
				printf("%.2x:", sa->dst.ip.ip6.ip6_b[i]);
			else
				printf("%.2x", sa->dst.ip.ip6.ip6_b[i]);
		}
		break;
	case TRANSPORT:
		printf("Transport ");
		break;
	}

	ips = &sa->sessions[IPSEC_SESSION_PRIMARY];
	printf(" type:");
	switch (ips->type) {
	case RTE_SECURITY_ACTION_TYPE_NONE:
		printf("no-offload ");
		break;
	case RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO:
		printf("inline-crypto-offload ");
		break;
	case RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL:
		printf("inline-protocol-offload ");
		break;
	case RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL:
		printf("lookaside-protocol-offload ");
		break;
	case RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO:
		printf("cpu-crypto-accelerated ");
		break;
	}

	fallback_ips = &sa->sessions[IPSEC_SESSION_FALLBACK];
	if (fallback_ips != NULL && sa->fallback_sessions > 0) {
		printf("inline fallback: ");
		switch (fallback_ips->type) {
		case RTE_SECURITY_ACTION_TYPE_NONE:
			printf("lookaside-none");
			break;
		case RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO:
			printf("cpu-crypto-accelerated");
			break;
		default:
			printf("invalid");
			break;
		}
	}
	if (sa->fdir_flag == 1)
		printf("flow-direction port %d queue %d", sa->portid,
				sa->fdir_qid);

	printf("\n");
}

static struct sa_ctx *
sa_create(const char *name, int32_t socket_id, uint32_t nb_sa)
{
	char s[PATH_MAX];
	struct sa_ctx *sa_ctx;
	uint32_t mz_size;
	const struct rte_memzone *mz;

	snprintf(s, sizeof(s), "%s_%u", name, socket_id);

	/* Create SA context */
	printf("Creating SA context with %u maximum entries on socket %d\n",
			nb_sa, socket_id);

	mz_size = sizeof(struct ipsec_xf) * nb_sa;
	mz = rte_memzone_reserve(s, mz_size, socket_id,
			RTE_MEMZONE_1GB | RTE_MEMZONE_SIZE_HINT_ONLY);
	if (mz == NULL) {
		printf("Failed to allocate SA XFORM memory\n");
		rte_errno = ENOMEM;
		return NULL;
	}

	sa_ctx = rte_zmalloc(NULL, sizeof(struct sa_ctx) +
		sizeof(struct ipsec_sa) * nb_sa, RTE_CACHE_LINE_SIZE);

	if (sa_ctx == NULL) {
		printf("Failed to allocate SA CTX memory\n");
		rte_errno = ENOMEM;
		rte_memzone_free(mz);
		return NULL;
	}

	sa_ctx->xf = (struct ipsec_xf *)mz->addr;
	sa_ctx->nb_sa = nb_sa;

	return sa_ctx;
}

static int
check_eth_dev_caps(uint16_t portid, uint32_t inbound)
{
	struct rte_eth_dev_info dev_info;
	int retval;

	retval = rte_eth_dev_info_get(portid, &dev_info);
	if (retval != 0) {
		RTE_LOG(ERR, IPSEC,
			"Error during getting device (port %u) info: %s\n",
			portid, strerror(-retval));

		return retval;
	}

	if (inbound) {
		if ((dev_info.rx_offload_capa &
				DEV_RX_OFFLOAD_SECURITY) == 0) {
			RTE_LOG(WARNING, PORT,
				"hardware RX IPSec offload is not supported\n");
			return -EINVAL;
		}

	} else { /* outbound */
		if ((dev_info.tx_offload_capa &
				DEV_TX_OFFLOAD_SECURITY) == 0) {
			RTE_LOG(WARNING, PORT,
				"hardware TX IPSec offload is not supported\n");
			return -EINVAL;
		}
	}
	return 0;
}

/*
 * Helper function, tries to determine next_proto for SPI
 * by searching though SP rules.
 */
static int
get_spi_proto(uint32_t spi, enum rte_security_ipsec_sa_direction dir,
		struct ip_addr ip_addr[2], uint32_t mask[2])
{
	int32_t rc4, rc6;

	rc4 = sp4_spi_present(spi, dir == RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
				ip_addr, mask);
	rc6 = sp6_spi_present(spi, dir == RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
				ip_addr, mask);

	if (rc4 >= 0) {
		if (rc6 >= 0) {
			RTE_LOG(ERR, IPSEC,
				"%s: SPI %u used simultaeously by "
				"IPv4(%d) and IPv6 (%d) SP rules\n",
				__func__, spi, rc4, rc6);
			return -EINVAL;
		} else
			return IPPROTO_IPIP;
	} else if (rc6 < 0) {
		RTE_LOG(ERR, IPSEC,
			"%s: SPI %u is not used by any SP rule\n",
			__func__, spi);
		return -EINVAL;
	} else
		return IPPROTO_IPV6;
}

/*
 * Helper function for getting source and destination IP addresses
 * from SP. Needed for inline crypto transport mode, as addresses are not
 * provided in config file for that mode. It checks if SP for current SA exists,
 * and based on what type of protocol is returned, it stores appropriate
 * addresses got from SP into SA.
 */
static int
sa_add_address_inline_crypto(struct ipsec_sa *sa)
{
	int protocol;
	struct ip_addr ip_addr[2];
	uint32_t mask[2];

	protocol = get_spi_proto(sa->spi, sa->direction, ip_addr, mask);
	if (protocol < 0)
		return protocol;
	else if (protocol == IPPROTO_IPIP) {
		sa->flags |= IP4_TRANSPORT;
		if (mask[0] == IP4_FULL_MASK &&
				mask[1] == IP4_FULL_MASK &&
				ip_addr[0].ip.ip4 != 0 &&
				ip_addr[1].ip.ip4 != 0) {

			sa->src.ip.ip4 = ip_addr[0].ip.ip4;
			sa->dst.ip.ip4 = ip_addr[1].ip.ip4;
		} else {
			RTE_LOG(ERR, IPSEC,
			"%s: No valid address or mask entry in"
			" IPv4 SP rule for SPI %u\n",
			__func__, sa->spi);
			return -EINVAL;
		}
	} else if (protocol == IPPROTO_IPV6) {
		sa->flags |= IP6_TRANSPORT;
		if (mask[0] == IP6_FULL_MASK &&
				mask[1] == IP6_FULL_MASK &&
				(ip_addr[0].ip.ip6.ip6[0] != 0 ||
				ip_addr[0].ip.ip6.ip6[1] != 0) &&
				(ip_addr[1].ip.ip6.ip6[0] != 0 ||
				ip_addr[1].ip.ip6.ip6[1] != 0)) {

			sa->src.ip.ip6 = ip_addr[0].ip.ip6;
			sa->dst.ip.ip6 = ip_addr[1].ip.ip6;
		} else {
			RTE_LOG(ERR, IPSEC,
			"%s: No valid address or mask entry in"
			" IPv6 SP rule for SPI %u\n",
			__func__, sa->spi);
			return -EINVAL;
		}
	}
	return 0;
}

static int
sa_add_rules(struct sa_ctx *sa_ctx, const struct ipsec_sa entries[],
		uint32_t nb_entries, uint32_t inbound,
		struct socket_ctx *skt_ctx)
{
	struct ipsec_sa *sa;
	uint32_t i, idx;
	uint16_t iv_length, aad_length;
	int inline_status;
	int32_t rc;
	struct rte_ipsec_session *ips;

	/* for ESN upper 32 bits of SQN also need to be part of AAD */
	aad_length = (app_sa_prm.enable_esn != 0) ? sizeof(uint32_t) : 0;

	for (i = 0; i < nb_entries; i++) {
		idx = i;
		sa = &sa_ctx->sa[idx];
		if (sa->spi != 0) {
			printf("Index %u already in use by SPI %u\n",
					idx, sa->spi);
			return -EINVAL;
		}
		*sa = entries[i];

		if (inbound) {
			rc = ipsec_sad_add(&sa_ctx->sad, sa);
			if (rc != 0)
				return rc;
		}

		sa->seq = 0;
		ips = ipsec_get_primary_session(sa);

		if (ips->type == RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL ||
			ips->type == RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO) {
			if (check_eth_dev_caps(sa->portid, inbound))
				return -EINVAL;
		}

		switch (WITHOUT_TRANSPORT_VERSION(sa->flags)) {
		case IP4_TUNNEL:
			sa->src.ip.ip4 = rte_cpu_to_be_32(sa->src.ip.ip4);
			sa->dst.ip.ip4 = rte_cpu_to_be_32(sa->dst.ip.ip4);
			break;
		case TRANSPORT:
			if (ips->type ==
				RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO) {
				inline_status =
					sa_add_address_inline_crypto(sa);
				if (inline_status < 0)
					return inline_status;
			}
			break;
		}

		if (sa->aead_algo == RTE_CRYPTO_AEAD_AES_GCM) {
			iv_length = 12;

			sa_ctx->xf[idx].a.type = RTE_CRYPTO_SYM_XFORM_AEAD;
			sa_ctx->xf[idx].a.aead.algo = sa->aead_algo;
			sa_ctx->xf[idx].a.aead.key.data = sa->cipher_key;
			sa_ctx->xf[idx].a.aead.key.length =
				sa->cipher_key_len;
			sa_ctx->xf[idx].a.aead.op = (inbound == 1) ?
				RTE_CRYPTO_AEAD_OP_DECRYPT :
				RTE_CRYPTO_AEAD_OP_ENCRYPT;
			sa_ctx->xf[idx].a.next = NULL;
			sa_ctx->xf[idx].a.aead.iv.offset = IV_OFFSET;
			sa_ctx->xf[idx].a.aead.iv.length = iv_length;
			sa_ctx->xf[idx].a.aead.aad_length =
				sa->aad_len + aad_length;
			sa_ctx->xf[idx].a.aead.digest_length =
				sa->digest_len;

			sa->xforms = &sa_ctx->xf[idx].a;
		} else {
			switch (sa->cipher_algo) {
			case RTE_CRYPTO_CIPHER_NULL:
			case RTE_CRYPTO_CIPHER_3DES_CBC:
			case RTE_CRYPTO_CIPHER_AES_CBC:
				iv_length = sa->iv_len;
				break;
			case RTE_CRYPTO_CIPHER_AES_CTR:
				iv_length = 16;
				break;
			default:
				RTE_LOG(ERR, IPSEC_ESP,
						"unsupported cipher algorithm %u\n",
						sa->cipher_algo);
				return -EINVAL;
			}

			if (inbound) {
				sa_ctx->xf[idx].b.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
				sa_ctx->xf[idx].b.cipher.algo = sa->cipher_algo;
				sa_ctx->xf[idx].b.cipher.key.data = sa->cipher_key;
				sa_ctx->xf[idx].b.cipher.key.length =
					sa->cipher_key_len;
				sa_ctx->xf[idx].b.cipher.op =
					RTE_CRYPTO_CIPHER_OP_DECRYPT;
				sa_ctx->xf[idx].b.next = NULL;
				sa_ctx->xf[idx].b.cipher.iv.offset = IV_OFFSET;
				sa_ctx->xf[idx].b.cipher.iv.length = iv_length;

				sa_ctx->xf[idx].a.type = RTE_CRYPTO_SYM_XFORM_AUTH;
				sa_ctx->xf[idx].a.auth.algo = sa->auth_algo;
				sa_ctx->xf[idx].a.auth.key.data = sa->auth_key;
				sa_ctx->xf[idx].a.auth.key.length =
					sa->auth_key_len;
				sa_ctx->xf[idx].a.auth.digest_length =
					sa->digest_len;
				sa_ctx->xf[idx].a.auth.op =
					RTE_CRYPTO_AUTH_OP_VERIFY;
			} else { /* outbound */
				sa_ctx->xf[idx].a.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
				sa_ctx->xf[idx].a.cipher.algo = sa->cipher_algo;
				sa_ctx->xf[idx].a.cipher.key.data = sa->cipher_key;
				sa_ctx->xf[idx].a.cipher.key.length =
					sa->cipher_key_len;
				sa_ctx->xf[idx].a.cipher.op =
					RTE_CRYPTO_CIPHER_OP_ENCRYPT;
				sa_ctx->xf[idx].a.next = NULL;
				sa_ctx->xf[idx].a.cipher.iv.offset = IV_OFFSET;
				sa_ctx->xf[idx].a.cipher.iv.length = iv_length;

				sa_ctx->xf[idx].b.type = RTE_CRYPTO_SYM_XFORM_AUTH;
				sa_ctx->xf[idx].b.auth.algo = sa->auth_algo;
				sa_ctx->xf[idx].b.auth.key.data = sa->auth_key;
				sa_ctx->xf[idx].b.auth.key.length =
					sa->auth_key_len;
				sa_ctx->xf[idx].b.auth.digest_length =
					sa->digest_len;
				sa_ctx->xf[idx].b.auth.op =
					RTE_CRYPTO_AUTH_OP_GENERATE;
			}

			sa_ctx->xf[idx].a.next = &sa_ctx->xf[idx].b;
			sa_ctx->xf[idx].b.next = NULL;
			sa->xforms = &sa_ctx->xf[idx].a;
		}

		if (ips->type ==
			RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL ||
			ips->type ==
			RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO) {
			rc = create_inline_session(skt_ctx, sa, ips);
			if (rc != 0) {
				RTE_LOG(ERR, IPSEC_ESP,
					"create_inline_session() failed\n");
				return -EINVAL;
			}
		}

		if (sa->fdir_flag && inbound) {
			rc = create_ipsec_esp_flow(sa);
			if (rc != 0)
				RTE_LOG(ERR, IPSEC_ESP,
					"create_ipsec_esp_flow() failed\n");
		}
		print_one_sa_rule(sa, inbound);
	}

	return 0;
}

static inline int
sa_out_add_rules(struct sa_ctx *sa_ctx, const struct ipsec_sa entries[],
		uint32_t nb_entries, struct socket_ctx *skt_ctx)
{
	return sa_add_rules(sa_ctx, entries, nb_entries, 0, skt_ctx);
}

static inline int
sa_in_add_rules(struct sa_ctx *sa_ctx, const struct ipsec_sa entries[],
		uint32_t nb_entries, struct socket_ctx *skt_ctx)
{
	return sa_add_rules(sa_ctx, entries, nb_entries, 1, skt_ctx);
}

/*
 * helper function, fills parameters that are identical for all SAs
 */
static void
fill_ipsec_app_sa_prm(struct rte_ipsec_sa_prm *prm,
	const struct app_sa_prm *app_prm)
{
	memset(prm, 0, sizeof(*prm));

	prm->flags = app_prm->flags;
	prm->ipsec_xform.options.esn = app_prm->enable_esn;
	prm->ipsec_xform.replay_win_sz = app_prm->window_size;
}

static int
fill_ipsec_sa_prm(struct rte_ipsec_sa_prm *prm, const struct ipsec_sa *ss,
	const struct rte_ipv4_hdr *v4, struct rte_ipv6_hdr *v6)
{
	int32_t rc;

	/*
	 * Try to get SPI next proto by searching that SPI in SPD.
	 * probably not the optimal way, but there seems nothing
	 * better right now.
	 */
	rc = get_spi_proto(ss->spi, ss->direction, NULL, NULL);
	if (rc < 0)
		return rc;

	fill_ipsec_app_sa_prm(prm, &app_sa_prm);
	prm->userdata = (uintptr_t)ss;

	/* setup ipsec xform */
	prm->ipsec_xform.spi = ss->spi;
	prm->ipsec_xform.salt = ss->salt;
	prm->ipsec_xform.direction = ss->direction;
	prm->ipsec_xform.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP;
	prm->ipsec_xform.mode = (IS_TRANSPORT(ss->flags)) ?
		RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT :
		RTE_SECURITY_IPSEC_SA_MODE_TUNNEL;
	prm->ipsec_xform.options.ecn = 1;
	prm->ipsec_xform.options.copy_dscp = 1;

	if (IS_IP4_TUNNEL(ss->flags)) {
		prm->ipsec_xform.tunnel.type = RTE_SECURITY_IPSEC_TUNNEL_IPV4;
		prm->tun.hdr_len = sizeof(*v4);
		prm->tun.next_proto = rc;
		prm->tun.hdr = v4;
	} else if (IS_IP6_TUNNEL(ss->flags)) {
		prm->ipsec_xform.tunnel.type = RTE_SECURITY_IPSEC_TUNNEL_IPV6;
		prm->tun.hdr_len = sizeof(*v6);
		prm->tun.next_proto = rc;
		prm->tun.hdr = v6;
	} else {
		/* transport mode */
		prm->trs.proto = rc;
	}

	/* setup crypto section */
	prm->crypto_xform = ss->xforms;
	return 0;
}

static int
fill_ipsec_session(struct rte_ipsec_session *ss, struct rte_ipsec_sa *sa)
{
	int32_t rc = 0;

	ss->sa = sa;

	if (ss->type == RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO ||
		ss->type == RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL) {
		if (ss->security.ses != NULL) {
			rc = rte_ipsec_session_prepare(ss);
			if (rc != 0)
				memset(ss, 0, sizeof(*ss));
		}
	}

	return rc;
}

/*
 * Initialise related rte_ipsec_sa object.
 */
static int
ipsec_sa_init(struct ipsec_sa *lsa, struct rte_ipsec_sa *sa, uint32_t sa_size)
{
	int rc;
	struct rte_ipsec_sa_prm prm;
	struct rte_ipsec_session *ips;
	struct rte_ipv4_hdr v4  = {
		.version_ihl = IPVERSION << 4 |
			sizeof(v4) / RTE_IPV4_IHL_MULTIPLIER,
		.time_to_live = IPDEFTTL,
		.next_proto_id = IPPROTO_ESP,
		.src_addr = lsa->src.ip.ip4,
		.dst_addr = lsa->dst.ip.ip4,
	};
	struct rte_ipv6_hdr v6 = {
		.vtc_flow = htonl(IP6_VERSION << 28),
		.proto = IPPROTO_ESP,
	};

	if (IS_IP6_TUNNEL(lsa->flags)) {
		memcpy(v6.src_addr, lsa->src.ip.ip6.ip6_b, sizeof(v6.src_addr));
		memcpy(v6.dst_addr, lsa->dst.ip.ip6.ip6_b, sizeof(v6.dst_addr));
	}

	rc = fill_ipsec_sa_prm(&prm, lsa, &v4, &v6);
	if (rc == 0)
		rc = rte_ipsec_sa_init(sa, &prm, sa_size);
	if (rc < 0)
		return rc;

	/* init primary processing session */
	ips = ipsec_get_primary_session(lsa);
	rc = fill_ipsec_session(ips, sa);
	if (rc != 0)
		return rc;

	/* init inline fallback processing session */
	if (lsa->fallback_sessions == 1)
		rc = fill_ipsec_session(ipsec_get_fallback_session(lsa), sa);

	return rc;
}

/*
 * Allocate space and init rte_ipsec_sa strcutures,
 * one per session.
 */
static int
ipsec_satbl_init(struct sa_ctx *ctx, uint32_t nb_ent, int32_t socket)
{
	int32_t rc, sz;
	uint32_t i, idx;
	size_t tsz;
	struct rte_ipsec_sa *sa;
	struct ipsec_sa *lsa;
	struct rte_ipsec_sa_prm prm;

	/* determine SA size */
	idx = 0;
	fill_ipsec_sa_prm(&prm, ctx->sa + idx, NULL, NULL);
	sz = rte_ipsec_sa_size(&prm);
	if (sz < 0) {
		RTE_LOG(ERR, IPSEC, "%s(%p, %u, %d): "
			"failed to determine SA size, error code: %d\n",
			__func__, ctx, nb_ent, socket, sz);
		return sz;
	}

	tsz = sz * nb_ent;

	ctx->satbl = rte_zmalloc_socket(NULL, tsz, RTE_CACHE_LINE_SIZE, socket);
	if (ctx->satbl == NULL) {
		RTE_LOG(ERR, IPSEC,
			"%s(%p, %u, %d): failed to allocate %zu bytes\n",
			__func__,  ctx, nb_ent, socket, tsz);
		return -ENOMEM;
	}

	rc = 0;
	for (i = 0; i != nb_ent && rc == 0; i++) {

		idx = i;

		sa = (struct rte_ipsec_sa *)((uintptr_t)ctx->satbl + sz * i);
		lsa = ctx->sa + idx;

		rc = ipsec_sa_init(lsa, sa, sz);
	}

	return rc;
}

static int
sa_cmp(const void *p, const void *q)
{
	uint32_t spi1 = ((const struct ipsec_sa *)p)->spi;
	uint32_t spi2 = ((const struct ipsec_sa *)q)->spi;

	return (int)(spi1 - spi2);
}

/*
 * Walk through all SA rules to find an SA with given SPI
 */
int
sa_spi_present(struct sa_ctx *sa_ctx, uint32_t spi, int inbound)
{
	uint32_t num;
	struct ipsec_sa *sa;
	struct ipsec_sa tmpl;
	const struct ipsec_sa *sar;

	sar = sa_ctx->sa;
	if (inbound != 0)
		num = nb_sa_in;
	else
		num = nb_sa_out;

	tmpl.spi = spi;

	sa = bsearch(&tmpl, sar, num, sizeof(struct ipsec_sa), sa_cmp);
	if (sa != NULL)
		return RTE_PTR_DIFF(sa, sar) / sizeof(struct ipsec_sa);

	return -ENOENT;
}

void
sa_init(struct socket_ctx *ctx, int32_t socket_id)
{
	int32_t rc;
	const char *name;

	if (ctx == NULL)
		rte_exit(EXIT_FAILURE, "NULL context.\n");

	if (ctx->sa_in != NULL)
		rte_exit(EXIT_FAILURE, "Inbound SA DB for socket %u already "
				"initialized\n", socket_id);

	if (ctx->sa_out != NULL)
		rte_exit(EXIT_FAILURE, "Outbound SA DB for socket %u already "
				"initialized\n", socket_id);

	if (nb_sa_in > 0) {
		name = "sa_in";
		ctx->sa_in = sa_create(name, socket_id, nb_sa_in);
		if (ctx->sa_in == NULL)
			rte_exit(EXIT_FAILURE, "Error [%d] creating SA "
				"context %s in socket %d\n", rte_errno,
				name, socket_id);

		rc = ipsec_sad_create(name, &ctx->sa_in->sad, socket_id,
				&sa_in_cnt);
		if (rc != 0)
			rte_exit(EXIT_FAILURE, "failed to init SAD\n");

		sa_in_add_rules(ctx->sa_in, sa_in, nb_sa_in, ctx);

		if (app_sa_prm.enable != 0) {
			rc = ipsec_satbl_init(ctx->sa_in, nb_sa_in,
				socket_id);
			if (rc != 0)
				rte_exit(EXIT_FAILURE,
					"failed to init inbound SAs\n");
		}
	} else
		RTE_LOG(WARNING, IPSEC, "No SA Inbound rule specified\n");

	if (nb_sa_out > 0) {
		name = "sa_out";
		ctx->sa_out = sa_create(name, socket_id, nb_sa_out);
		if (ctx->sa_out == NULL)
			rte_exit(EXIT_FAILURE, "Error [%d] creating SA "
				"context %s in socket %d\n", rte_errno,
				name, socket_id);

		sa_out_add_rules(ctx->sa_out, sa_out, nb_sa_out, ctx);

		if (app_sa_prm.enable != 0) {
			rc = ipsec_satbl_init(ctx->sa_out, nb_sa_out,
				socket_id);
			if (rc != 0)
				rte_exit(EXIT_FAILURE,
					"failed to init outbound SAs\n");
		}
	} else
		RTE_LOG(WARNING, IPSEC, "No SA Outbound rule "
			"specified\n");
}

int
inbound_sa_check(struct sa_ctx *sa_ctx, struct rte_mbuf *m, uint32_t sa_idx)
{
	struct ipsec_mbuf_metadata *priv;
	struct ipsec_sa *sa;

	priv = get_priv(m);
	sa = priv->sa;
	if (sa != NULL)
		return (sa_ctx->sa[sa_idx].spi == sa->spi);

	RTE_LOG(ERR, IPSEC, "SA not saved in private data\n");
	return 0;
}

void
inbound_sa_lookup(struct sa_ctx *sa_ctx, struct rte_mbuf *pkts[],
		void *sa_arr[], uint16_t nb_pkts)
{
	uint32_t i;
	void *result_sa;
	struct ipsec_sa *sa;

	sad_lookup(&sa_ctx->sad, pkts, sa_arr, nb_pkts);

	/*
	 * Mark need for inline offload fallback on the LSB of SA pointer.
	 * Thanks to packet grouping mechanism which ipsec_process is using
	 * packets marked for fallback processing will form separate group.
	 *
	 * Because it is not safe to use SA pointer it is casted to generic
	 * pointer to prevent from unintentional use. Use ipsec_mask_saptr
	 * to get valid struct pointer.
	 */
	for (i = 0; i < nb_pkts; i++) {
		if (sa_arr[i] == NULL)
			continue;

		result_sa = sa = sa_arr[i];
		if (MBUF_NO_SEC_OFFLOAD(pkts[i]) &&
			sa->fallback_sessions > 0) {
			uintptr_t intsa = (uintptr_t)sa;
			intsa |= IPSEC_SA_OFFLOAD_FALLBACK_FLAG;
			result_sa = (void *)intsa;
		}
		sa_arr[i] = result_sa;
	}
}

void
outbound_sa_lookup(struct sa_ctx *sa_ctx, uint32_t sa_idx[],
		void *sa[], uint16_t nb_pkts)
{
	uint32_t i;

	for (i = 0; i < nb_pkts; i++)
		sa[i] = &sa_ctx->sa[sa_idx[i]];
}

/*
 * Select HW offloads to be used.
 */
int
sa_check_offloads(uint16_t port_id, uint64_t *rx_offloads,
		uint64_t *tx_offloads)
{
	struct ipsec_sa *rule;
	uint32_t idx_sa;
	enum rte_security_session_action_type rule_type;

	*rx_offloads = 0;
	*tx_offloads = 0;

	/* Check for inbound rules that use offloads and use this port */
	for (idx_sa = 0; idx_sa < nb_sa_in; idx_sa++) {
		rule = &sa_in[idx_sa];
		rule_type = ipsec_get_action_type(rule);
		if ((rule_type == RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO ||
				rule_type ==
				RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL)
				&& rule->portid == port_id)
			*rx_offloads |= DEV_RX_OFFLOAD_SECURITY;
	}

	/* Check for outbound rules that use offloads and use this port */
	for (idx_sa = 0; idx_sa < nb_sa_out; idx_sa++) {
		rule = &sa_out[idx_sa];
		rule_type = ipsec_get_action_type(rule);
		if ((rule_type == RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO ||
				rule_type ==
				RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL)
				&& rule->portid == port_id)
			*tx_offloads |= DEV_TX_OFFLOAD_SECURITY;
	}
	return 0;
}

void
sa_sort_arr(void)
{
	qsort(sa_in, nb_sa_in, sizeof(struct ipsec_sa), sa_cmp);
	qsort(sa_out, nb_sa_out, sizeof(struct ipsec_sa), sa_cmp);
}

uint32_t
get_nb_crypto_sessions(void)
{
	return nb_crypto_sessions;
}
