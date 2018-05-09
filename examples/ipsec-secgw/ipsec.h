/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#ifndef __IPSEC_H__
#define __IPSEC_H__

#include <stdint.h>

#include <rte_byteorder.h>
#include <rte_crypto.h>
#include <rte_security.h>
#include <rte_flow.h>

#define RTE_LOGTYPE_IPSEC       RTE_LOGTYPE_USER1
#define RTE_LOGTYPE_IPSEC_ESP   RTE_LOGTYPE_USER2
#define RTE_LOGTYPE_IPSEC_IPIP  RTE_LOGTYPE_USER3

#define MAX_PKT_BURST 32
#define MAX_INFLIGHT 128
#define MAX_QP_PER_LCORE 256

#define MAX_DIGEST_SIZE 32 /* Bytes -- 256 bits */

#define IPSEC_OFFLOAD_ESN_SOFTLIMIT 0xffffff00

#define IV_OFFSET		(sizeof(struct rte_crypto_op) + \
				sizeof(struct rte_crypto_sym_op))

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

#define MAX_KEY_SIZE		32

struct ipsec_sa {
	uint32_t spi;
	uint32_t cdev_id_qp;
	uint64_t seq;
	uint32_t salt;
	union {
		struct rte_cryptodev_sym_session *crypto_session;
		struct rte_security_session *sec_session;
	};
	enum rte_crypto_cipher_algorithm cipher_algo;
	enum rte_crypto_auth_algorithm auth_algo;
	enum rte_crypto_aead_algorithm aead_algo;
	uint16_t digest_len;
	uint16_t iv_len;
	uint16_t block_size;
	uint16_t flags;
#define IP4_TUNNEL (1 << 0)
#define IP6_TUNNEL (1 << 1)
#define TRANSPORT  (1 << 2)
	struct ip_addr src;
	struct ip_addr dst;
	uint8_t cipher_key[MAX_KEY_SIZE];
	uint16_t cipher_key_len;
	uint8_t auth_key[MAX_KEY_SIZE];
	uint16_t auth_key_len;
	uint16_t aad_len;
	union {
		struct rte_crypto_sym_xform *xforms;
		struct rte_security_ipsec_xform *sec_xform;
	};
	enum rte_security_session_action_type type;
	enum rte_security_ipsec_sa_direction direction;
	uint16_t portid;
	struct rte_security_ctx *security_ctx;
	uint32_t ol_flags;

#define MAX_RTE_FLOW_PATTERN (4)
#define MAX_RTE_FLOW_ACTIONS (3)
	struct rte_flow_item pattern[MAX_RTE_FLOW_PATTERN];
	struct rte_flow_action action[MAX_RTE_FLOW_ACTIONS];
	struct rte_flow_attr attr;
	union {
		struct rte_flow_item_ipv4 ipv4_spec;
		struct rte_flow_item_ipv6 ipv6_spec;
	};
	struct rte_flow_item_esp esp_spec;
	struct rte_flow *flow;
	struct rte_security_session_conf sess_conf;
} __rte_cache_aligned;

struct ipsec_mbuf_metadata {
	struct ipsec_sa *sa;
	struct rte_crypto_op cop;
	struct rte_crypto_sym_op sym_cop;
	uint8_t buf[32];
} __rte_cache_aligned;

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
	struct rte_mempool *session_pool;
	struct rte_mbuf *ol_pkts[MAX_PKT_BURST] __rte_aligned(sizeof(void *));
	uint16_t ol_pkts_cnt;
};

struct cdev_key {
	uint16_t lcore_id;
	uint8_t cipher_algo;
	uint8_t auth_algo;
	uint8_t aead_algo;
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
	struct rte_mempool *session_pool;
};

struct cnt_blk {
	uint32_t salt;
	uint64_t iv;
	uint32_t cnt;
} __attribute__((packed));

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

static inline void *
get_cnt_blk(struct rte_mbuf *m)
{
	struct ipsec_mbuf_metadata *priv = get_priv(m);

	return &priv->buf[0];
}

static inline void *
get_aad(struct rte_mbuf *m)
{
	struct ipsec_mbuf_metadata *priv = get_priv(m);

	return &priv->buf[16];
}

static inline void *
get_sym_cop(struct rte_crypto_op *cop)
{
	return (cop + 1);
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
sp4_init(struct socket_ctx *ctx, int32_t socket_id);

void
sp6_init(struct socket_ctx *ctx, int32_t socket_id);

void
sa_init(struct socket_ctx *ctx, int32_t socket_id);

void
rt_init(struct socket_ctx *ctx, int32_t socket_id);

#endif /* __IPSEC_H__ */
