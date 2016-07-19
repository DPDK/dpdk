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

#include "ipsec.h"
#include "esp.h"

/* SAs Outbound */
const struct ipsec_sa sa_out[] = {
	{
	.spi = 5,
	.src.ip.ip4 = IPv4(172, 16, 1, 5),
	.dst.ip.ip4 = IPv4(172, 16, 2, 5),
	.cipher_algo = RTE_CRYPTO_CIPHER_AES_CBC,
	.auth_algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
	.digest_len = 12,
	.iv_len = 16,
	.block_size = 16,
	.flags = IP4_TUNNEL
	},
	{
	.spi = 6,
	.src.ip.ip4 = IPv4(172, 16, 1, 6),
	.dst.ip.ip4 = IPv4(172, 16, 2, 6),
	.cipher_algo = RTE_CRYPTO_CIPHER_AES_CBC,
	.auth_algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
	.digest_len = 12,
	.iv_len = 16,
	.block_size = 16,
	.flags = IP4_TUNNEL
	},
	{
	.spi = 10,
	.cipher_algo = RTE_CRYPTO_CIPHER_AES_CBC,
	.auth_algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
	.digest_len = 12,
	.iv_len = 16,
	.block_size = 16,
	.flags = TRANSPORT
	},
	{
	.spi = 11,
	.cipher_algo = RTE_CRYPTO_CIPHER_AES_CBC,
	.auth_algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
	.digest_len = 12,
	.iv_len = 16,
	.block_size = 16,
	.flags = TRANSPORT
	},
	{
	.spi = 15,
	.src.ip.ip4 = IPv4(172, 16, 1, 5),
	.dst.ip.ip4 = IPv4(172, 16, 2, 5),
	.cipher_algo = RTE_CRYPTO_CIPHER_NULL,
	.auth_algo = RTE_CRYPTO_AUTH_NULL,
	.digest_len = 0,
	.iv_len = 0,
	.block_size = 4,
	.flags = IP4_TUNNEL
	},
	{
	.spi = 16,
	.src.ip.ip4 = IPv4(172, 16, 1, 6),
	.dst.ip.ip4 = IPv4(172, 16, 2, 6),
	.cipher_algo = RTE_CRYPTO_CIPHER_NULL,
	.auth_algo = RTE_CRYPTO_AUTH_NULL,
	.digest_len = 0,
	.iv_len = 0,
	.block_size = 4,
	.flags = IP4_TUNNEL
	},
	{
	.spi = 25,
	.src.ip.ip6.ip6_b = { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
		0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x55, 0x55 },
	.dst.ip.ip6.ip6_b = { 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
		0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x55, 0x55 },
	.cipher_algo = RTE_CRYPTO_CIPHER_AES_CBC,
	.auth_algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
	.digest_len = 12,
	.iv_len = 16,
	.block_size = 16,
	.flags = IP6_TUNNEL
	},
	{
	.spi = 26,
	.src.ip.ip6.ip6_b = { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
		0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x66, 0x66 },
	.dst.ip.ip6.ip6_b = { 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
		0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x66, 0x66 },
	.cipher_algo = RTE_CRYPTO_CIPHER_AES_CBC,
	.auth_algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
	.digest_len = 12,
	.iv_len = 16,
	.block_size = 16,
	.flags = IP6_TUNNEL
	},
};

/* SAs Inbound */
const struct ipsec_sa sa_in[] = {
	{
	.spi = 105,
	.src.ip.ip4 = IPv4(172, 16, 2, 5),
	.dst.ip.ip4 = IPv4(172, 16, 1, 5),
	.cipher_algo = RTE_CRYPTO_CIPHER_AES_CBC,
	.auth_algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
	.digest_len = 12,
	.iv_len = 16,
	.block_size = 16,
	.flags = IP4_TUNNEL
	},
	{
	.spi = 106,
	.src.ip.ip4 = IPv4(172, 16, 2, 6),
	.dst.ip.ip4 = IPv4(172, 16, 1, 6),
	.cipher_algo = RTE_CRYPTO_CIPHER_AES_CBC,
	.auth_algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
	.digest_len = 12,
	.iv_len = 16,
	.block_size = 16,
	.flags = IP4_TUNNEL
	},
	{
	.spi = 110,
	.cipher_algo = RTE_CRYPTO_CIPHER_AES_CBC,
	.auth_algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
	.digest_len = 12,
	.iv_len = 16,
	.block_size = 16,
	.flags = TRANSPORT
	},
	{
	.spi = 111,
	.cipher_algo = RTE_CRYPTO_CIPHER_AES_CBC,
	.auth_algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
	.digest_len = 12,
	.iv_len = 16,
	.block_size = 16,
	.flags = TRANSPORT
	},
	{
	.spi = 115,
	.src.ip.ip4 = IPv4(172, 16, 2, 5),
	.dst.ip.ip4 = IPv4(172, 16, 1, 5),
	.cipher_algo = RTE_CRYPTO_CIPHER_NULL,
	.auth_algo = RTE_CRYPTO_AUTH_NULL,
	.digest_len = 0,
	.iv_len = 0,
	.block_size = 4,
	.flags = IP4_TUNNEL
	},
	{
	.spi = 116,
	.src.ip.ip4 = IPv4(172, 16, 2, 6),
	.dst.ip.ip4 = IPv4(172, 16, 1, 6),
	.cipher_algo = RTE_CRYPTO_CIPHER_NULL,
	.auth_algo = RTE_CRYPTO_AUTH_NULL,
	.digest_len = 0,
	.iv_len = 0,
	.block_size = 4,
	.flags = IP4_TUNNEL
	},
	{
	.spi = 125,
	.src.ip.ip6.ip6_b = { 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
		0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x55, 0x55 },
	.dst.ip.ip6.ip6_b = { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
		0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x55, 0x55 },
	.cipher_algo = RTE_CRYPTO_CIPHER_AES_CBC,
	.auth_algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
	.digest_len = 12,
	.iv_len = 16,
	.block_size = 16,
	.flags = IP6_TUNNEL
	},
	{
	.spi = 126,
	.src.ip.ip6.ip6_b = { 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
		0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x66, 0x66 },
	.dst.ip.ip6.ip6_b = { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
		0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x66, 0x66 },
	.cipher_algo = RTE_CRYPTO_CIPHER_AES_CBC,
	.auth_algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
	.digest_len = 12,
	.iv_len = 16,
	.block_size = 16,
	.flags = IP6_TUNNEL
	},
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
sa_init(struct socket_ctx *ctx, int32_t socket_id, uint32_t ep)
{
	const struct ipsec_sa *sa_out_entries, *sa_in_entries;
	uint32_t nb_out_entries, nb_in_entries;
	const char *name;

	if (ctx == NULL)
		rte_exit(EXIT_FAILURE, "NULL context.\n");

	if (ctx->sa_in != NULL)
		rte_exit(EXIT_FAILURE, "Inbound SA DB for socket %u already "
				"initialized\n", socket_id);

	if (ctx->sa_out != NULL)
		rte_exit(EXIT_FAILURE, "Outbound SA DB for socket %u already "
				"initialized\n", socket_id);

	if (ep == 0) {
		sa_out_entries = sa_out;
		nb_out_entries = RTE_DIM(sa_out);
		sa_in_entries = sa_in;
		nb_in_entries = RTE_DIM(sa_in);
	} else if (ep == 1) {
		sa_out_entries = sa_in;
		nb_out_entries = RTE_DIM(sa_in);
		sa_in_entries = sa_out;
		nb_in_entries = RTE_DIM(sa_out);
	} else
		rte_exit(EXIT_FAILURE, "Invalid EP value %u. "
				"Only 0 or 1 supported.\n", ep);

	name = "sa_in";
	ctx->sa_in = sa_create(name, socket_id);
	if (ctx->sa_in == NULL)
		rte_exit(EXIT_FAILURE, "Error [%d] creating SA context %s "
				"in socket %d\n", rte_errno, name, socket_id);

	name = "sa_out";
	ctx->sa_out = sa_create(name, socket_id);
	if (ctx->sa_out == NULL)
		rte_exit(EXIT_FAILURE, "Error [%d] creating SA context %s "
				"in socket %d\n", rte_errno, name, socket_id);

	sa_in_add_rules(ctx->sa_in, sa_in_entries, nb_in_entries);

	sa_out_add_rules(ctx->sa_out, sa_out_entries, nb_out_entries);
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
