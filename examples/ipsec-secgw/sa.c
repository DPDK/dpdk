/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
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

#define IPDEFTTL 64

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
		.digest_len = 12,
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
	}
};

static struct ipsec_sa sa_out[IPSEC_SA_MAX_ENTRIES];
static uint32_t nb_sa_out;

static struct ipsec_sa sa_in[IPSEC_SA_MAX_ENTRIES];
static uint32_t nb_sa_in;

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

void
parse_sa_tokens(char **tokens, uint32_t n_tokens,
	struct parse_status *status)
{
	struct ipsec_sa *rule = NULL;
	uint32_t ti; /*token index*/
	uint32_t *ri /*rule index*/;
	uint32_t cipher_algo_p = 0;
	uint32_t auth_algo_p = 0;
	uint32_t aead_algo_p = 0;
	uint32_t src_p = 0;
	uint32_t dst_p = 0;
	uint32_t mode_p = 0;
	uint32_t type_p = 0;
	uint32_t portid_p = 0;

	if (strcmp(tokens[0], "in") == 0) {
		ri = &nb_sa_in;

		APP_CHECK(*ri <= IPSEC_SA_MAX_ENTRIES - 1, status,
			"too many sa rules, abort insertion\n");
		if (status->status < 0)
			return;

		rule = &sa_in[*ri];
	} else {
		ri = &nb_sa_out;

		APP_CHECK(*ri <= IPSEC_SA_MAX_ENTRIES - 1, status,
			"too many sa rules, abort insertion\n");
		if (status->status < 0)
			return;

		rule = &sa_out[*ri];
	}

	/* spi number */
	APP_CHECK_TOKEN_IS_NUM(tokens, 1, status);
	if (status->status < 0)
		return;
	if (atoi(tokens[1]) == INVALID_SPI)
		return;
	rule->spi = atoi(tokens[1]);

	for (ti = 2; ti < n_tokens; ti++) {
		if (strcmp(tokens[ti], "mode") == 0) {
			APP_CHECK_PRESENCE(mode_p, tokens[ti], status);
			if (status->status < 0)
				return;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			if (strcmp(tokens[ti], "ipv4-tunnel") == 0)
				rule->flags = IP4_TUNNEL;
			else if (strcmp(tokens[ti], "ipv6-tunnel") == 0)
				rule->flags = IP6_TUNNEL;
			else if (strcmp(tokens[ti], "transport") == 0)
				rule->flags = TRANSPORT;
			else {
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

			if (rule->flags == IP4_TUNNEL) {
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
			} else if (rule->flags == IP6_TUNNEL) {
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
			} else if (rule->flags == TRANSPORT) {
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

			if (rule->flags == IP4_TUNNEL) {
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
			} else if (rule->flags == IP6_TUNNEL) {
				struct in6_addr ip;

				APP_CHECK(parse_ipv6_addr(tokens[ti], &ip,
					NULL) == 0, status,
					"unrecognized input \"%s\", "
					"expect valid ipv6 addr",
					tokens[ti]);
				if (status->status < 0)
					return;
				memcpy(rule->dst.ip.ip6.ip6_b, ip.s6_addr, 16);
			} else if (rule->flags == TRANSPORT) {
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
				rule->type =
					RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO;
			else if (strcmp(tokens[ti],
					"inline-protocol-offload") == 0)
				rule->type =
				RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL;
			else if (strcmp(tokens[ti],
					"lookaside-protocol-offload") == 0)
				rule->type =
				RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL;
			else if (strcmp(tokens[ti], "no-offload") == 0)
				rule->type = RTE_SECURITY_ACTION_TYPE_NONE;
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
			rule->portid = atoi(tokens[ti]);
			if (status->status < 0)
				return;
			portid_p = 1;
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

	if ((rule->type != RTE_SECURITY_ACTION_TYPE_NONE) && (portid_p == 0))
		printf("Missing portid option, falling back to non-offload\n");

	if (!type_p || !portid_p) {
		rule->type = RTE_SECURITY_ACTION_TYPE_NONE;
		rule->portid = -1;
	}

	*ri = *ri + 1;
}

static void
print_one_sa_rule(const struct ipsec_sa *sa, int inbound)
{
	uint32_t i;
	uint8_t a, b, c, d;

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
		if (aead_algos[i].algo == sa->aead_algo) {
			printf("%s ", aead_algos[i].keyword);
			break;
		}
	}

	printf("mode:");

	switch (sa->flags) {
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
	printf(" type:");
	switch (sa->type) {
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
	}
	printf("\n");
}

struct sa_ctx {
	void *satbl; /* pointer to array of rte_ipsec_sa objects*/
	struct ipsec_sa sa[IPSEC_SA_MAX_ENTRIES];
	union {
		struct {
			struct rte_crypto_sym_xform a;
			struct rte_crypto_sym_xform b;
		};
	} xf[IPSEC_SA_MAX_ENTRIES];
};

static struct sa_ctx *
sa_create(const char *name, int32_t socket_id)
{
	char s[PATH_MAX];
	struct sa_ctx *sa_ctx;
	uint32_t mz_size;
	const struct rte_memzone *mz;

	snprintf(s, sizeof(s), "%s_%u", name, socket_id);

	/* Create SA array table */
	printf("Creating SA context with %u maximum entries on socket %d\n",
			IPSEC_SA_MAX_ENTRIES, socket_id);

	mz_size = sizeof(struct sa_ctx);
	mz = rte_memzone_reserve(s, mz_size, socket_id,
			RTE_MEMZONE_1GB | RTE_MEMZONE_SIZE_HINT_ONLY);
	if (mz == NULL) {
		printf("Failed to allocate SA DB memory\n");
		rte_errno = -ENOMEM;
		return NULL;
	}

	sa_ctx = (struct sa_ctx *)mz->addr;

	return sa_ctx;
}

static int
check_eth_dev_caps(uint16_t portid, uint32_t inbound)
{
	struct rte_eth_dev_info dev_info;

	rte_eth_dev_info_get(portid, &dev_info);

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


static int
sa_add_rules(struct sa_ctx *sa_ctx, const struct ipsec_sa entries[],
		uint32_t nb_entries, uint32_t inbound)
{
	struct ipsec_sa *sa;
	uint32_t i, idx;
	uint16_t iv_length, aad_length;

	/* for ESN upper 32 bits of SQN also need to be part of AAD */
	aad_length = (app_sa_prm.enable_esn != 0) ? sizeof(uint32_t) : 0;

	for (i = 0; i < nb_entries; i++) {
		idx = SPI2IDX(entries[i].spi);
		sa = &sa_ctx->sa[idx];
		if (sa->spi != 0) {
			printf("Index %u already in use by SPI %u\n",
					idx, sa->spi);
			return -EINVAL;
		}
		*sa = entries[i];
		sa->seq = 0;

		if (sa->type == RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL ||
			sa->type == RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO) {
			if (check_eth_dev_caps(sa->portid, inbound))
				return -EINVAL;
		}

		sa->direction = (inbound == 1) ?
				RTE_SECURITY_IPSEC_SA_DIR_INGRESS :
				RTE_SECURITY_IPSEC_SA_DIR_EGRESS;

		switch (sa->flags) {
		case IP4_TUNNEL:
			sa->src.ip.ip4 = rte_cpu_to_be_32(sa->src.ip.ip4);
			sa->dst.ip.ip4 = rte_cpu_to_be_32(sa->dst.ip.ip4);
		}

		if (sa->aead_algo == RTE_CRYPTO_AEAD_AES_GCM) {
			iv_length = 16;

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

			print_one_sa_rule(sa, inbound);
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

			print_one_sa_rule(sa, inbound);
		}
	}

	return 0;
}

static inline int
sa_out_add_rules(struct sa_ctx *sa_ctx, const struct ipsec_sa entries[],
		uint32_t nb_entries)
{
	return sa_add_rules(sa_ctx, entries, nb_entries, 0);
}

static inline int
sa_in_add_rules(struct sa_ctx *sa_ctx, const struct ipsec_sa entries[],
		uint32_t nb_entries)
{
	return sa_add_rules(sa_ctx, entries, nb_entries, 1);
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
	prm->replay_win_sz = app_prm->window_size;
}

/*
 * Helper function, tries to determine next_proto for SPI
 * by searching though SP rules.
 */
static int
get_spi_proto(uint32_t spi, enum rte_security_ipsec_sa_direction dir)
{
	int32_t rc4, rc6;

	rc4 = sp4_spi_present(spi, dir == RTE_SECURITY_IPSEC_SA_DIR_INGRESS);
	rc6 = sp6_spi_present(spi, dir == RTE_SECURITY_IPSEC_SA_DIR_INGRESS);

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

static int
fill_ipsec_sa_prm(struct rte_ipsec_sa_prm *prm, const struct ipsec_sa *ss,
	const struct ipv4_hdr *v4, struct ipv6_hdr *v6)
{
	int32_t rc;

	/*
	 * Try to get SPI next proto by searching that SPI in SPD.
	 * probably not the optimal way, but there seems nothing
	 * better right now.
	 */
	rc = get_spi_proto(ss->spi, ss->direction);
	if (rc < 0)
		return rc;

	fill_ipsec_app_sa_prm(prm, &app_sa_prm);
	prm->userdata = (uintptr_t)ss;

	/* setup ipsec xform */
	prm->ipsec_xform.spi = ss->spi;
	prm->ipsec_xform.salt = ss->salt;
	prm->ipsec_xform.direction = ss->direction;
	prm->ipsec_xform.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP;
	prm->ipsec_xform.mode = (ss->flags == TRANSPORT) ?
		RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT :
		RTE_SECURITY_IPSEC_SA_MODE_TUNNEL;

	if (ss->flags == IP4_TUNNEL) {
		prm->ipsec_xform.tunnel.type = RTE_SECURITY_IPSEC_TUNNEL_IPV4;
		prm->tun.hdr_len = sizeof(*v4);
		prm->tun.next_proto = rc;
		prm->tun.hdr = v4;
	} else if (ss->flags == IP6_TUNNEL) {
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

static void
fill_ipsec_session(struct rte_ipsec_session *ss, struct rte_ipsec_sa *sa,
	const struct ipsec_sa *lsa)
{
	ss->sa = sa;
	ss->type = lsa->type;

	/* setup crypto section */
	if (ss->type == RTE_SECURITY_ACTION_TYPE_NONE) {
		ss->crypto.ses = lsa->crypto_session;
	/* setup session action type */
	} else {
		ss->security.ses = lsa->sec_session;
		ss->security.ctx = lsa->security_ctx;
		ss->security.ol_flags = lsa->ol_flags;
	}
}

/*
 * Initialise related rte_ipsec_sa object.
 */
static int
ipsec_sa_init(struct ipsec_sa *lsa, struct rte_ipsec_sa *sa, uint32_t sa_size)
{
	int rc;
	struct rte_ipsec_sa_prm prm;
	struct ipv4_hdr v4  = {
		.version_ihl = IPVERSION << 4 |
			sizeof(v4) / IPV4_IHL_MULTIPLIER,
		.time_to_live = IPDEFTTL,
		.next_proto_id = IPPROTO_ESP,
		.src_addr = lsa->src.ip.ip4,
		.dst_addr = lsa->dst.ip.ip4,
	};
	struct ipv6_hdr v6 = {
		.vtc_flow = htonl(IP6_VERSION << 28),
		.proto = IPPROTO_ESP,
	};

	if (lsa->flags == IP6_TUNNEL) {
		memcpy(v6.src_addr, lsa->src.ip.ip6.ip6_b, sizeof(v6.src_addr));
		memcpy(v6.dst_addr, lsa->dst.ip.ip6.ip6_b, sizeof(v6.dst_addr));
	}

	rc = fill_ipsec_sa_prm(&prm, lsa, &v4, &v6);
	if (rc == 0)
		rc = rte_ipsec_sa_init(sa, &prm, sa_size);
	if (rc < 0)
		return rc;

	fill_ipsec_session(&lsa->ips, sa, lsa);
	return 0;
}

/*
 * Allocate space and init rte_ipsec_sa strcutures,
 * one per session.
 */
static int
ipsec_satbl_init(struct sa_ctx *ctx, const struct ipsec_sa *ent,
	uint32_t nb_ent, int32_t socket)
{
	int32_t rc, sz;
	uint32_t i, idx;
	size_t tsz;
	struct rte_ipsec_sa *sa;
	struct ipsec_sa *lsa;
	struct rte_ipsec_sa_prm prm;

	/* determine SA size */
	idx = SPI2IDX(ent[0].spi);
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

		idx = SPI2IDX(ent[i].spi);

		sa = (struct rte_ipsec_sa *)((uintptr_t)ctx->satbl + sz * i);
		lsa = ctx->sa + idx;

		rc = ipsec_sa_init(lsa, sa, sz);
	}

	return rc;
}

/*
 * Walk through all SA rules to find an SA with given SPI
 */
int
sa_spi_present(uint32_t spi, int inbound)
{
	uint32_t i, num;
	const struct ipsec_sa *sar;

	if (inbound != 0) {
		sar = sa_in;
		num = nb_sa_in;
	} else {
		sar = sa_out;
		num = nb_sa_out;
	}

	for (i = 0; i != num; i++) {
		if (sar[i].spi == spi)
			return i;
	}

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
		ctx->sa_in = sa_create(name, socket_id);
		if (ctx->sa_in == NULL)
			rte_exit(EXIT_FAILURE, "Error [%d] creating SA "
				"context %s in socket %d\n", rte_errno,
				name, socket_id);

		sa_in_add_rules(ctx->sa_in, sa_in, nb_sa_in);

		if (app_sa_prm.enable != 0) {
			rc = ipsec_satbl_init(ctx->sa_in, sa_in, nb_sa_in,
				socket_id);
			if (rc != 0)
				rte_exit(EXIT_FAILURE,
					"failed to init inbound SAs\n");
		}
	} else
		RTE_LOG(WARNING, IPSEC, "No SA Inbound rule specified\n");

	if (nb_sa_out > 0) {
		name = "sa_out";
		ctx->sa_out = sa_create(name, socket_id);
		if (ctx->sa_out == NULL)
			rte_exit(EXIT_FAILURE, "Error [%d] creating SA "
				"context %s in socket %d\n", rte_errno,
				name, socket_id);

		sa_out_add_rules(ctx->sa_out, sa_out, nb_sa_out);

		if (app_sa_prm.enable != 0) {
			rc = ipsec_satbl_init(ctx->sa_out, sa_out, nb_sa_out,
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

static inline void
single_inbound_lookup(struct ipsec_sa *sadb, struct rte_mbuf *pkt,
		struct ipsec_sa **sa_ret)
{
	struct esp_hdr *esp;
	struct ip *ip;
	uint32_t *src4_addr;
	uint8_t *src6_addr;
	struct ipsec_sa *sa;

	*sa_ret = NULL;

	ip = rte_pktmbuf_mtod(pkt, struct ip *);
	if (ip->ip_v == IPVERSION)
		esp = (struct esp_hdr *)(ip + 1);
	else
		esp = (struct esp_hdr *)(((struct ip6_hdr *)ip) + 1);

	if (esp->spi == INVALID_SPI)
		return;

	sa = &sadb[SPI2IDX(rte_be_to_cpu_32(esp->spi))];
	if (rte_be_to_cpu_32(esp->spi) != sa->spi)
		return;

	switch (sa->flags) {
	case IP4_TUNNEL:
		src4_addr = RTE_PTR_ADD(ip, offsetof(struct ip, ip_src));
		if ((ip->ip_v == IPVERSION) &&
				(sa->src.ip.ip4 == *src4_addr) &&
				(sa->dst.ip.ip4 == *(src4_addr + 1)))
			*sa_ret = sa;
		break;
	case IP6_TUNNEL:
		src6_addr = RTE_PTR_ADD(ip, offsetof(struct ip6_hdr, ip6_src));
		if ((ip->ip_v == IP6_VERSION) &&
				!memcmp(&sa->src.ip.ip6.ip6, src6_addr, 16) &&
				!memcmp(&sa->dst.ip.ip6.ip6, src6_addr + 16, 16))
			*sa_ret = sa;
		break;
	case TRANSPORT:
		*sa_ret = sa;
	}
}

void
inbound_sa_lookup(struct sa_ctx *sa_ctx, struct rte_mbuf *pkts[],
		struct ipsec_sa *sa[], uint16_t nb_pkts)
{
	uint32_t i;

	for (i = 0; i < nb_pkts; i++)
		single_inbound_lookup(sa_ctx->sa, pkts[i], &sa[i]);
}

void
outbound_sa_lookup(struct sa_ctx *sa_ctx, uint32_t sa_idx[],
		struct ipsec_sa *sa[], uint16_t nb_pkts)
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

	*rx_offloads = 0;
	*tx_offloads = 0;

	/* Check for inbound rules that use offloads and use this port */
	for (idx_sa = 0; idx_sa < nb_sa_in; idx_sa++) {
		rule = &sa_in[idx_sa];
		if ((rule->type == RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO ||
				rule->type ==
				RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL)
				&& rule->portid == port_id)
			*rx_offloads |= DEV_RX_OFFLOAD_SECURITY;
	}

	/* Check for outbound rules that use offloads and use this port */
	for (idx_sa = 0; idx_sa < nb_sa_out; idx_sa++) {
		rule = &sa_out[idx_sa];
		if ((rule->type == RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO ||
				rule->type ==
				RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL)
				&& rule->portid == port_id)
			*tx_offloads |= DEV_TX_OFFLOAD_SECURITY;
	}
	return 0;
}
