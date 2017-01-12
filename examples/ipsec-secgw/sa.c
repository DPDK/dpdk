/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include <rte_cryptodev.h>
#include <rte_byteorder.h>
#include <rte_errno.h>
#include <rte_ip.h>
#include <rte_random.h>

#include "ipsec.h"
#include "esp.h"
#include "parser.h"

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
	uint8_t aad_len;
	uint8_t key_not_req;
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
		.keyword = "aes-128-gcm",
		.algo = RTE_CRYPTO_CIPHER_AES_GCM,
		.iv_len = 8,
		.block_size = 4,
		.key_len = 20
	},
	{
		.keyword = "aes-128-ctr",
		.algo = RTE_CRYPTO_CIPHER_AES_CTR,
		.iv_len = 8,
		.block_size = 16, /* XXX AESNI MB limition, should be 4 */
		.key_len = 20
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
	},
	{
		.keyword = "aes-128-gcm",
		.algo = RTE_CRYPTO_AUTH_AES_GCM,
		.digest_len = 16,
		.aad_len = 8,
		.key_not_req = 1
	}
};

struct ipsec_sa sa_out[IPSEC_SA_MAX_ENTRIES];
uint32_t nb_sa_out;

struct ipsec_sa sa_in[IPSEC_SA_MAX_ENTRIES];
uint32_t nb_sa_in;

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
	uint32_t src_p = 0;
	uint32_t dst_p = 0;
	uint32_t mode_p = 0;

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

			if (algo->algo == RTE_CRYPTO_CIPHER_AES_CBC)
				rule->salt = (uint32_t)rte_rand();

			if ((algo->algo == RTE_CRYPTO_CIPHER_AES_CTR) ||
				(algo->algo == RTE_CRYPTO_CIPHER_AES_GCM)) {
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
			rule->aad_len = algo->key_len;

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

		/* unrecognizeable input */
		APP_CHECK(0, status, "unrecognized input \"%s\"",
			tokens[ti]);
		return;
	}

	APP_CHECK(cipher_algo_p == 1, status, "missing cipher options");
	if (status->status < 0)
		return;

	APP_CHECK(auth_algo_p == 1, status, "missing auth options");
	if (status->status < 0)
		return;

	APP_CHECK(mode_p == 1, status, "missing mode option");
	if (status->status < 0)
		return;

	*ri = *ri + 1;
}

static inline void
print_one_sa_rule(const struct ipsec_sa *sa, int inbound)
{
	uint32_t i;
	uint8_t a, b, c, d;

	printf("\tspi_%s(%3u):", inbound?"in":"out", sa->spi);

	for (i = 0; i < RTE_DIM(cipher_algos); i++) {
		if (cipher_algos[i].algo == sa->cipher_algo) {
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
		printf("Transport");
		break;
	}
	printf("\n");
}

struct sa_ctx {
	struct ipsec_sa sa[IPSEC_SA_MAX_ENTRIES];
	struct {
		struct rte_crypto_sym_xform a;
		struct rte_crypto_sym_xform b;
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
	printf("Creating SA context with %u maximum entries\n",
			IPSEC_SA_MAX_ENTRIES);

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
sa_add_rules(struct sa_ctx *sa_ctx, const struct ipsec_sa entries[],
		uint32_t nb_entries, uint32_t inbound)
{
	struct ipsec_sa *sa;
	uint32_t i, idx;

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

		switch (sa->flags) {
		case IP4_TUNNEL:
			sa->src.ip.ip4 = rte_cpu_to_be_32(sa->src.ip.ip4);
			sa->dst.ip.ip4 = rte_cpu_to_be_32(sa->dst.ip.ip4);
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

			sa_ctx->xf[idx].a.type = RTE_CRYPTO_SYM_XFORM_AUTH;
			sa_ctx->xf[idx].a.auth.algo = sa->auth_algo;
			sa_ctx->xf[idx].a.auth.add_auth_data_length =
				sa->aad_len;
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

			sa_ctx->xf[idx].b.type = RTE_CRYPTO_SYM_XFORM_AUTH;
			sa_ctx->xf[idx].b.auth.algo = sa->auth_algo;
			sa_ctx->xf[idx].b.auth.add_auth_data_length =
				sa->aad_len;
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

void
sa_init(struct socket_ctx *ctx, int32_t socket_id)
{
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
	} else
		RTE_LOG(WARNING, IPSEC, "No SA Outbound rule "
			"specified\n");
}

int
inbound_sa_check(struct sa_ctx *sa_ctx, struct rte_mbuf *m, uint32_t sa_idx)
{
	struct ipsec_mbuf_metadata *priv;

	priv = RTE_PTR_ADD(m, sizeof(struct rte_mbuf));

	return (sa_ctx->sa[sa_idx].spi == priv->sa->spi);
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
