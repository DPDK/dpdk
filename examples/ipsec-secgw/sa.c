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

#include <rte_memzone.h>
#include <rte_crypto.h>
#include <rte_cryptodev.h>
#include <rte_byteorder.h>
#include <rte_errno.h>

#include "ipsec.h"
#include "esp.h"

/* SAs EP0 Outbound */
const struct ipsec_sa sa_ep0_out[] = {
	{ 5, 0, IPv4(172, 16, 1, 5), IPv4(172, 16, 2, 5),
		NULL, NULL,
		esp4_tunnel_outbound_pre_crypto,
		esp4_tunnel_outbound_post_crypto,
		RTE_CRYPTO_CIPHER_AES_CBC, RTE_CRYPTO_AUTH_SHA1_HMAC,
		12, 16, 16,
		0, 0 },
	{ 6, 0, IPv4(172, 16, 1, 6), IPv4(172, 16, 2, 6),
		NULL, NULL,
		esp4_tunnel_outbound_pre_crypto,
		esp4_tunnel_outbound_post_crypto,
		RTE_CRYPTO_CIPHER_AES_CBC, RTE_CRYPTO_AUTH_SHA1_HMAC,
		12, 16, 16,
		0, 0 },
	{ 7, 0, IPv4(172, 16, 1, 7), IPv4(172, 16, 2, 7),
		NULL, NULL,
		esp4_tunnel_outbound_pre_crypto,
		esp4_tunnel_outbound_post_crypto,
		RTE_CRYPTO_CIPHER_AES_CBC, RTE_CRYPTO_AUTH_SHA1_HMAC,
		12, 16, 16,
		0, 0 },
	{ 8, 0, IPv4(172, 16, 1, 8), IPv4(172, 16, 2, 8),
		NULL, NULL,
		esp4_tunnel_outbound_pre_crypto,
		esp4_tunnel_outbound_post_crypto,
		RTE_CRYPTO_CIPHER_AES_CBC, RTE_CRYPTO_AUTH_SHA1_HMAC,
		12, 16, 16,
		0, 0 },
	{ 9, 0, IPv4(172, 16, 1, 5), IPv4(172, 16, 2, 5),
		NULL, NULL,
		esp4_tunnel_outbound_pre_crypto,
		esp4_tunnel_outbound_post_crypto,
		RTE_CRYPTO_CIPHER_NULL, RTE_CRYPTO_AUTH_NULL,
		0, 0, 4,
		0, 0 },
};

/* SAs EP0 Inbound */
const struct ipsec_sa sa_ep0_in[] = {
	{ 5, 0, IPv4(172, 16, 2, 5), IPv4(172, 16, 1, 5),
		NULL, NULL,
		esp4_tunnel_inbound_pre_crypto,
		esp4_tunnel_inbound_post_crypto,
		RTE_CRYPTO_CIPHER_AES_CBC, RTE_CRYPTO_AUTH_SHA1_HMAC,
		12, 16, 16,
		0, 0 },
	{ 6, 0, IPv4(172, 16, 2, 6), IPv4(172, 16, 1, 6),
		NULL, NULL,
		esp4_tunnel_inbound_pre_crypto,
		esp4_tunnel_inbound_post_crypto,
		RTE_CRYPTO_CIPHER_AES_CBC, RTE_CRYPTO_AUTH_SHA1_HMAC,
		12, 16, 16,
		0, 0 },
	{ 7, 0, IPv4(172, 16, 2, 7), IPv4(172, 16, 1, 7),
		NULL, NULL,
		esp4_tunnel_inbound_pre_crypto,
		esp4_tunnel_inbound_post_crypto,
		RTE_CRYPTO_CIPHER_AES_CBC, RTE_CRYPTO_AUTH_SHA1_HMAC,
		12, 16, 16,
		0, 0 },
	{ 8, 0, IPv4(172, 16, 2, 8), IPv4(172, 16, 1, 8),
		NULL, NULL,
		esp4_tunnel_inbound_pre_crypto,
		esp4_tunnel_inbound_post_crypto,
		RTE_CRYPTO_CIPHER_AES_CBC, RTE_CRYPTO_AUTH_SHA1_HMAC,
		12, 16, 16,
		0, 0 },
	{ 9, 0, IPv4(172, 16, 2, 5), IPv4(172, 16, 1, 5),
		NULL, NULL,
		esp4_tunnel_inbound_pre_crypto,
		esp4_tunnel_inbound_post_crypto,
		RTE_CRYPTO_CIPHER_NULL, RTE_CRYPTO_AUTH_NULL,
		0, 0, 4,
		0, 0 },
};

/* SAs EP1 Outbound */
const struct ipsec_sa sa_ep1_out[] = {
	{ 5, 0, IPv4(172, 16, 2, 5), IPv4(172, 16, 1, 5),
		NULL, NULL,
		esp4_tunnel_outbound_pre_crypto,
		esp4_tunnel_outbound_post_crypto,
		RTE_CRYPTO_CIPHER_AES_CBC, RTE_CRYPTO_AUTH_SHA1_HMAC,
		12, 16, 16,
		0, 0 },
	{ 6, 0, IPv4(172, 16, 2, 6), IPv4(172, 16, 1, 6),
		NULL, NULL,
		esp4_tunnel_outbound_pre_crypto,
		esp4_tunnel_outbound_post_crypto,
		RTE_CRYPTO_CIPHER_AES_CBC, RTE_CRYPTO_AUTH_SHA1_HMAC,
		12, 16, 16,
		0, 0 },
	{ 7, 0, IPv4(172, 16, 2, 7), IPv4(172, 16, 1, 7),
		NULL, NULL,
		esp4_tunnel_outbound_pre_crypto,
		esp4_tunnel_outbound_post_crypto,
		RTE_CRYPTO_CIPHER_AES_CBC, RTE_CRYPTO_AUTH_SHA1_HMAC,
		12, 16, 16,
		0, 0 },
	{ 8, 0, IPv4(172, 16, 2, 8), IPv4(172, 16, 1, 8),
		NULL, NULL,
		esp4_tunnel_outbound_pre_crypto,
		esp4_tunnel_outbound_post_crypto,
		RTE_CRYPTO_CIPHER_AES_CBC, RTE_CRYPTO_AUTH_SHA1_HMAC,
		12, 16, 16,
		0, 0 },
	{ 9, 0, IPv4(172, 16, 2, 5), IPv4(172, 16, 1, 5),
		NULL, NULL,
		esp4_tunnel_outbound_pre_crypto,
		esp4_tunnel_outbound_post_crypto,
		RTE_CRYPTO_CIPHER_NULL, RTE_CRYPTO_AUTH_NULL,
		0, 0, 4,
		0, 0 },
};

/* SAs EP1 Inbound */
const struct ipsec_sa sa_ep1_in[] = {
	{ 5, 0, IPv4(172, 16, 1, 5), IPv4(172, 16, 2, 5),
		NULL, NULL,
		esp4_tunnel_inbound_pre_crypto,
		esp4_tunnel_inbound_post_crypto,
		RTE_CRYPTO_CIPHER_AES_CBC, RTE_CRYPTO_AUTH_SHA1_HMAC,
		12, 16, 16,
		0, 0 },
	{ 6, 0, IPv4(172, 16, 1, 6), IPv4(172, 16, 2, 6),
		NULL, NULL,
		esp4_tunnel_inbound_pre_crypto,
		esp4_tunnel_inbound_post_crypto,
		RTE_CRYPTO_CIPHER_AES_CBC, RTE_CRYPTO_AUTH_SHA1_HMAC,
		12, 16, 16,
		0, 0 },
	{ 7, 0, IPv4(172, 16, 1, 7), IPv4(172, 16, 2, 7),
		NULL, NULL,
		esp4_tunnel_inbound_pre_crypto,
		esp4_tunnel_inbound_post_crypto,
		RTE_CRYPTO_CIPHER_AES_CBC, RTE_CRYPTO_AUTH_SHA1_HMAC,
		12, 16, 16,
		0, 0 },
	{ 8, 0, IPv4(172, 16, 1, 8), IPv4(172, 16, 2, 8),
		NULL, NULL,
		esp4_tunnel_inbound_pre_crypto,
		esp4_tunnel_inbound_post_crypto,
		RTE_CRYPTO_CIPHER_AES_CBC, RTE_CRYPTO_AUTH_SHA1_HMAC,
		12, 16, 16,
		0, 0 },
	{ 9, 0, IPv4(172, 16, 1, 5), IPv4(172, 16, 2, 5),
		NULL, NULL,
		esp4_tunnel_inbound_pre_crypto,
		esp4_tunnel_inbound_post_crypto,
		RTE_CRYPTO_CIPHER_NULL, RTE_CRYPTO_AUTH_NULL,
		0, 0, 4,
		0, 0 },
};

static uint8_t cipher_key[256] = "sixteenbytes key";

/* AES CBC xform */
const struct rte_crypto_sym_xform aescbc_enc_xf = {
	NULL,
	RTE_CRYPTO_SYM_XFORM_CIPHER,
	{.cipher = { RTE_CRYPTO_CIPHER_OP_ENCRYPT, RTE_CRYPTO_CIPHER_AES_CBC,
		.key = { cipher_key, 16 } }
	}
};

const struct rte_crypto_sym_xform aescbc_dec_xf = {
	NULL,
	RTE_CRYPTO_SYM_XFORM_CIPHER,
	{.cipher = { RTE_CRYPTO_CIPHER_OP_DECRYPT, RTE_CRYPTO_CIPHER_AES_CBC,
		.key = { cipher_key, 16 } }
	}
};

static uint8_t auth_key[256] = "twentybytes hash key";

/* SHA1 HMAC xform */
const struct rte_crypto_sym_xform sha1hmac_gen_xf = {
	NULL,
	RTE_CRYPTO_SYM_XFORM_AUTH,
	{.auth = { RTE_CRYPTO_AUTH_OP_GENERATE, RTE_CRYPTO_AUTH_SHA1_HMAC,
		.key = { auth_key, 20 }, 12, 0 }
	}
};

const struct rte_crypto_sym_xform sha1hmac_verify_xf = {
	NULL,
	RTE_CRYPTO_SYM_XFORM_AUTH,
	{.auth = { RTE_CRYPTO_AUTH_OP_VERIFY, RTE_CRYPTO_AUTH_SHA1_HMAC,
		.key = { auth_key, 20 }, 12, 0 }
	}
};

/* AES CBC xform */
const struct rte_crypto_sym_xform null_cipher_xf = {
	NULL,
	RTE_CRYPTO_SYM_XFORM_CIPHER,
	{.cipher = { .algo = RTE_CRYPTO_CIPHER_NULL }
	}
};

const struct rte_crypto_sym_xform null_auth_xf = {
	NULL,
	RTE_CRYPTO_SYM_XFORM_AUTH,
	{.auth = { .algo = RTE_CRYPTO_AUTH_NULL }
	}
};

struct sa_ctx {
	struct ipsec_sa sa[IPSEC_SA_MAX_ENTRIES];
	struct {
		struct rte_crypto_sym_xform a;
		struct rte_crypto_sym_xform b;
	} xf[IPSEC_SA_MAX_ENTRIES];
};

static struct sa_ctx *
sa_ipv4_create(const char *name, int socket_id)
{
	char s[PATH_MAX];
	struct sa_ctx *sa_ctx;
	unsigned mz_size;
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
		unsigned nb_entries, unsigned inbound)
{
	struct ipsec_sa *sa;
	unsigned i, idx;

	for (i = 0; i < nb_entries; i++) {
		idx = SPI2IDX(entries[i].spi);
		sa = &sa_ctx->sa[idx];
		if (sa->spi != 0) {
			printf("Index %u already in use by SPI %u\n",
					idx, sa->spi);
			return -EINVAL;
		}
		*sa = entries[i];
		sa->src = rte_cpu_to_be_32(sa->src);
		sa->dst = rte_cpu_to_be_32(sa->dst);
		if (inbound) {
			if (sa->cipher_algo == RTE_CRYPTO_CIPHER_NULL) {
				sa_ctx->xf[idx].a = null_auth_xf;
				sa_ctx->xf[idx].b = null_cipher_xf;
			} else {
				sa_ctx->xf[idx].a = sha1hmac_verify_xf;
				sa_ctx->xf[idx].b = aescbc_dec_xf;
			}
		} else { /* outbound */
			if (sa->cipher_algo == RTE_CRYPTO_CIPHER_NULL) {
				sa_ctx->xf[idx].a = null_cipher_xf;
				sa_ctx->xf[idx].b = null_auth_xf;
			} else {
				sa_ctx->xf[idx].a = aescbc_enc_xf;
				sa_ctx->xf[idx].b = sha1hmac_gen_xf;
			}
		}
		sa_ctx->xf[idx].a.next = &sa_ctx->xf[idx].b;
		sa_ctx->xf[idx].b.next = NULL;
		sa->xforms = &sa_ctx->xf[idx].a;
	}

	return 0;
}

static inline int
sa_out_add_rules(struct sa_ctx *sa_ctx, const struct ipsec_sa entries[],
		unsigned nb_entries)
{
	return sa_add_rules(sa_ctx, entries, nb_entries, 0);
}

static inline int
sa_in_add_rules(struct sa_ctx *sa_ctx, const struct ipsec_sa entries[],
		unsigned nb_entries)
{
	return sa_add_rules(sa_ctx, entries, nb_entries, 1);
}

void
sa_init(struct socket_ctx *ctx, int socket_id, unsigned ep)
{
	const struct ipsec_sa *sa_out_entries, *sa_in_entries;
	unsigned nb_out_entries, nb_in_entries;
	const char *name;

	if (ctx == NULL)
		rte_exit(EXIT_FAILURE, "NULL context.\n");

	if (ctx->sa_ipv4_in != NULL)
		rte_exit(EXIT_FAILURE, "Inbound SA DB for socket %u already "
				"initialized\n", socket_id);

	if (ctx->sa_ipv4_out != NULL)
		rte_exit(EXIT_FAILURE, "Outbound SA DB for socket %u already "
				"initialized\n", socket_id);

	if (ep == 0) {
		sa_out_entries = sa_ep0_out;
		nb_out_entries = RTE_DIM(sa_ep0_out);
		sa_in_entries = sa_ep0_in;
		nb_in_entries = RTE_DIM(sa_ep0_in);
	} else if (ep == 1) {
		sa_out_entries = sa_ep1_out;
		nb_out_entries = RTE_DIM(sa_ep1_out);
		sa_in_entries = sa_ep1_in;
		nb_in_entries = RTE_DIM(sa_ep1_in);
	} else
		rte_exit(EXIT_FAILURE, "Invalid EP value %u. "
				"Only 0 or 1 supported.\n", ep);

	name = "sa_ipv4_in";
	ctx->sa_ipv4_in = sa_ipv4_create(name, socket_id);
	if (ctx->sa_ipv4_in == NULL)
		rte_exit(EXIT_FAILURE, "Error [%d] creating SA context %s "
				"in socket %d\n", rte_errno, name, socket_id);

	name = "sa_ipv4_out";
	ctx->sa_ipv4_out = sa_ipv4_create(name, socket_id);
	if (ctx->sa_ipv4_out == NULL)
		rte_exit(EXIT_FAILURE, "Error [%d] creating SA context %s "
				"in socket %d\n", rte_errno, name, socket_id);

	sa_in_add_rules(ctx->sa_ipv4_in, sa_in_entries, nb_in_entries);

	sa_out_add_rules(ctx->sa_ipv4_out, sa_out_entries, nb_out_entries);
}

int
inbound_sa_check(struct sa_ctx *sa_ctx, struct rte_mbuf *m, uint32_t sa_idx)
{
	struct ipsec_mbuf_metadata *priv;

	priv = RTE_PTR_ADD(m, sizeof(struct rte_mbuf));

	return (sa_ctx->sa[sa_idx].spi == priv->sa->spi);
}

void
inbound_sa_lookup(struct sa_ctx *sa_ctx, struct rte_mbuf *pkts[],
		struct ipsec_sa *sa[], uint16_t nb_pkts)
{
	unsigned i;
	uint32_t *src, spi;

	for (i = 0; i < nb_pkts; i++) {
		spi = rte_pktmbuf_mtod_offset(pkts[i], struct esp_hdr *,
				sizeof(struct ip))->spi;

		if (spi == INVALID_SPI)
			continue;

		sa[i] = &sa_ctx->sa[SPI2IDX(spi)];
		if (spi != sa[i]->spi) {
			sa[i] = NULL;
			continue;
		}

		src = rte_pktmbuf_mtod_offset(pkts[i], uint32_t *,
				offsetof(struct ip, ip_src));
		if ((sa[i]->src != *src) || (sa[i]->dst != *(src + 1)))
			sa[i] = NULL;
	}
}

void
outbound_sa_lookup(struct sa_ctx *sa_ctx, uint32_t sa_idx[],
		struct ipsec_sa *sa[], uint16_t nb_pkts)
{
	unsigned i;

	for (i = 0; i < nb_pkts; i++)
		sa[i] = &sa_ctx->sa[sa_idx[i]];
}
