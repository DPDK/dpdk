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

#ifndef __IPSEC_H__
#define __IPSEC_H__

#include <stdint.h>

#include <rte_byteorder.h>
#include <rte_crypto.h>

#define RTE_LOGTYPE_IPSEC       RTE_LOGTYPE_USER1
#define RTE_LOGTYPE_IPSEC_ESP   RTE_LOGTYPE_USER2
#define RTE_LOGTYPE_IPSEC_IPIP  RTE_LOGTYPE_USER3

#define MAX_PKT_BURST 32
#define MAX_QP_PER_LCORE 256

#define MAX_DIGEST_SIZE 32 /* Bytes -- 256 bits */

#define uint32_t_to_char(ip, a, b, c, d) do {\
		*a = (uint8_t)(ip >> 24 & 0xff);\
		*b = (uint8_t)(ip >> 16 & 0xff);\
		*c = (uint8_t)(ip >> 8 & 0xff);\
		*d = (uint8_t)(ip & 0xff);\
	} while (0)

#define DEFAULT_MAX_CATEGORIES	1

#define IPSEC_SA_MAX_ENTRIES (128) /* must be power of 2, max 2 power 30 */
#define SPI2IDX(spi) (spi & (IPSEC_SA_MAX_ENTRIES - 1))
#define INVALID_SPI (0)

#define DISCARD (0x80000000)
#define BYPASS (0x40000000)
#define PROTECT_MASK (0x3fffffff)
#define PROTECT(sa_idx) (SPI2IDX(sa_idx) & PROTECT_MASK) /* SA idx 30 bits */

#define IPSEC_XFORM_MAX 2

#define IP6_VERSION (6)

struct rte_crypto_xform;
struct ipsec_xform;
struct rte_cryptodev_session;
struct rte_mbuf;

struct ipsec_sa;

typedef int32_t (*ipsec_xform_fn)(struct rte_mbuf *m, struct ipsec_sa *sa,
		struct rte_crypto_op *cop);

struct ip_addr {
	union {
		uint32_t ip4;
		union {
			uint64_t ip6[2];
			uint8_t ip6_b[16];
		} ip6;
	} ip;
};

struct ipsec_sa {
	uint32_t spi;
	uint32_t cdev_id_qp;
	struct rte_cryptodev_sym_session *crypto_session;
	uint32_t seq;
	enum rte_crypto_cipher_algorithm cipher_algo;
	enum rte_crypto_auth_algorithm auth_algo;
	uint16_t digest_len;
	uint16_t iv_len;
	uint16_t block_size;
	uint16_t flags;
#define IP4_TUNNEL (1 << 0)
#define IP6_TUNNEL (1 << 1)
#define TRANSPORT  (1 << 2)
	struct ip_addr src;
	struct ip_addr dst;
	struct rte_crypto_sym_xform *xforms;
} __rte_cache_aligned;

struct ipsec_mbuf_metadata {
	struct ipsec_sa *sa;
	struct rte_crypto_op cop;
	struct rte_crypto_sym_op sym_cop;
};

struct cdev_qp {
	uint16_t id;
	uint16_t qp;
	uint16_t in_flight;
	uint16_t len;
	struct rte_crypto_op *buf[MAX_PKT_BURST] __rte_aligned(sizeof(void *));
};

struct ipsec_ctx {
	struct rte_hash *cdev_map;
	struct sp_ctx *sp4_ctx;
	struct sp_ctx *sp6_ctx;
	struct sa_ctx *sa_ctx;
	uint16_t nb_qps;
	uint16_t last_qp;
	struct cdev_qp tbl[MAX_QP_PER_LCORE];
};

struct cdev_key {
	uint16_t lcore_id;
	uint8_t cipher_algo;
	uint8_t auth_algo;
};

struct socket_ctx {
	struct sa_ctx *sa_in;
	struct sa_ctx *sa_out;
	struct sp_ctx *sp_ip4_in;
	struct sp_ctx *sp_ip4_out;
	struct sp_ctx *sp_ip6_in;
	struct sp_ctx *sp_ip6_out;
	struct rt_ctx *rt_ip4;
	struct rt_ctx *rt_ip6;
	struct rte_mempool *mbuf_pool;
};

uint16_t
ipsec_inbound(struct ipsec_ctx *ctx, struct rte_mbuf *pkts[],
		uint16_t nb_pkts, uint16_t len);

uint16_t
ipsec_outbound(struct ipsec_ctx *ctx, struct rte_mbuf *pkts[],
		uint32_t sa_idx[], uint16_t nb_pkts, uint16_t len);

static inline uint16_t
ipsec_metadata_size(void)
{
	return sizeof(struct ipsec_mbuf_metadata);
}

static inline struct ipsec_mbuf_metadata *
get_priv(struct rte_mbuf *m)
{
	return RTE_PTR_ADD(m, sizeof(struct rte_mbuf));
}

int
inbound_sa_check(struct sa_ctx *sa_ctx, struct rte_mbuf *m, uint32_t sa_idx);

void
inbound_sa_lookup(struct sa_ctx *sa_ctx, struct rte_mbuf *pkts[],
		struct ipsec_sa *sa[], uint16_t nb_pkts);

void
outbound_sa_lookup(struct sa_ctx *sa_ctx, uint32_t sa_idx[],
		struct ipsec_sa *sa[], uint16_t nb_pkts);

void
sp4_init(struct socket_ctx *ctx, int32_t socket_id, uint32_t ep);

void
sp6_init(struct socket_ctx *ctx, int32_t socket_id, uint32_t ep);

void
sa_init(struct socket_ctx *ctx, int32_t socket_id, uint32_t ep);

void
rt_init(struct socket_ctx *ctx, int32_t socket_id, uint32_t ep);

#endif /* __IPSEC_H__ */
