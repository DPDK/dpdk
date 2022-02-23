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
#include <rte_ipsec.h>

#include "ipsec-secgw.h"

#define RTE_LOGTYPE_IPSEC_ESP   RTE_LOGTYPE_USER2
#define RTE_LOGTYPE_IPSEC_IPIP  RTE_LOGTYPE_USER3

#define MAX_INFLIGHT 128
#define MAX_QP_PER_LCORE 256

#define MAX_DIGEST_SIZE 32 /* Bytes -- 256 bits */

#define IV_OFFSET		(sizeof(struct rte_crypto_op) + \
				sizeof(struct rte_crypto_sym_op))

#define DEFAULT_MAX_CATEGORIES	1

#define INVALID_SPI (0)

#define DISCARD	INVALID_SPI
#define BYPASS	UINT32_MAX

#define IPSEC_XFORM_MAX 2

#define IP6_VERSION (6)

struct rte_crypto_xform;
struct ipsec_xform;
struct rte_mbuf;

struct ipsec_sa;
/*
 * Keeps number of configured SA's for each address family:
 */
struct ipsec_sa_cnt {
	uint32_t	nb_v4;
	uint32_t	nb_v6;
};

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

#define MAX_KEY_SIZE		64
/*
 * application wide SA parameters
 */
struct app_sa_prm {
	uint32_t enable; /* use librte_ipsec API for ipsec pkt processing */
	uint32_t window_size; /* replay window size */
	uint32_t enable_esn;  /* enable/disable ESN support */
	uint32_t cache_sz;	/* per lcore SA cache size */
	uint32_t udp_encap;   /* enable/disable UDP Encapsulation */
	uint64_t flags;       /* rte_ipsec_sa_prm.flags */
};

extern struct app_sa_prm app_sa_prm;

struct flow_info {
	struct rte_flow *rx_def_flow;
};

extern struct flow_info flow_info_tbl[RTE_MAX_ETHPORTS];

enum {
	IPSEC_SESSION_PRIMARY = 0,
	IPSEC_SESSION_FALLBACK = 1,
	IPSEC_SESSION_MAX
};

#define IPSEC_SA_OFFLOAD_FALLBACK_FLAG (1)

static inline struct ipsec_sa *
ipsec_mask_saptr(void *ptr)
{
	uintptr_t i = (uintptr_t)ptr;
	static const uintptr_t mask = IPSEC_SA_OFFLOAD_FALLBACK_FLAG;

	i &= ~mask;

	return (struct ipsec_sa *)i;
}

struct ipsec_sa {
	struct rte_ipsec_session sessions[IPSEC_SESSION_MAX];
	uint32_t spi;
	uint32_t cdev_id_qp;
	uint64_t seq;
	uint32_t salt;
	uint32_t fallback_sessions;
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
#define IP4_TRANSPORT (1 << 3)
#define IP6_TRANSPORT (1 << 4)
#define SA_TELEMETRY_ENABLE (1 << 5)

	struct ip_addr src;
	struct ip_addr dst;
	struct {
		uint16_t sport;
		uint16_t dport;
	} udp;
	uint8_t cipher_key[MAX_KEY_SIZE];
	uint16_t cipher_key_len;
	uint8_t auth_key[MAX_KEY_SIZE];
	uint16_t auth_key_len;
	uint16_t aad_len;
	union {
		struct rte_crypto_sym_xform *xforms;
		struct rte_security_ipsec_xform *sec_xform;
	};
	enum rte_security_ipsec_sa_direction direction;
	uint8_t udp_encap;
	uint16_t portid;
	uint64_t esn;
	uint16_t mss;
	uint8_t fdir_qid;
	uint8_t fdir_flag;

#define MAX_RTE_FLOW_PATTERN (5)
#define MAX_RTE_FLOW_ACTIONS (3)
	struct rte_flow_item pattern[MAX_RTE_FLOW_PATTERN];
	struct rte_flow_action action[MAX_RTE_FLOW_ACTIONS];
	struct rte_flow_attr attr;
	union {
		struct rte_flow_item_ipv4 ipv4_spec;
		struct rte_flow_item_ipv6 ipv6_spec;
	};
	struct rte_flow_item_udp udp_spec;
	struct rte_flow_item_esp esp_spec;
	struct rte_flow *flow;
	struct rte_security_session_conf sess_conf;
} __rte_cache_aligned;

struct ipsec_xf {
	struct rte_crypto_sym_xform a;
	struct rte_crypto_sym_xform b;
};

struct ipsec_sad {
	struct rte_ipsec_sad *sad_v4;
	struct rte_ipsec_sad *sad_v6;
};

struct sa_ctx {
	void *satbl; /* pointer to array of rte_ipsec_sa objects*/
	struct ipsec_sad sad;
	struct ipsec_xf *xf;
	uint32_t nb_sa;
	struct ipsec_sa sa[];
};

struct ipsec_mbuf_metadata {
	struct ipsec_sa *sa;
	struct rte_crypto_op cop;
	struct rte_crypto_sym_op sym_cop;
	uint8_t buf[32];
} __rte_cache_aligned;

#define IS_TRANSPORT(flags) ((flags) & TRANSPORT)

#define IS_TUNNEL(flags) ((flags) & (IP4_TUNNEL | IP6_TUNNEL))

#define IS_IP4(flags) ((flags) & (IP4_TUNNEL | IP4_TRANSPORT))

#define IS_IP6(flags) ((flags) & (IP6_TUNNEL | IP6_TRANSPORT))

#define IS_IP4_TUNNEL(flags) ((flags) & IP4_TUNNEL)

#define IS_IP6_TUNNEL(flags) ((flags) & IP6_TUNNEL)

/*
 * Macro for getting ipsec_sa flags statuses without version of protocol
 * used for transport (IP4_TRANSPORT and IP6_TRANSPORT flags).
 */
#define WITHOUT_TRANSPORT_VERSION(flags) \
		((flags) & (IP4_TUNNEL | \
			IP6_TUNNEL | \
			TRANSPORT))

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
	struct rte_mempool *session_priv_pool;
	struct rte_mbuf *ol_pkts[MAX_PKT_BURST] __rte_aligned(sizeof(void *));
	uint16_t ol_pkts_cnt;
	uint64_t ipv4_offloads;
	uint64_t ipv6_offloads;
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
	struct rte_mempool *mbuf_pool[RTE_MAX_ETHPORTS];
	struct rte_mempool *mbuf_pool_indir;
	struct rte_mempool *session_pool;
	struct rte_mempool *session_priv_pool;
};

struct cnt_blk {
	uint32_t salt;
	uint64_t iv;
	uint32_t cnt;
} __rte_packed;

/* Socket ctx */
extern struct socket_ctx socket_ctx[NB_SOCKETS];

void
ipsec_poll_mode_worker(void);

int
ipsec_launch_one_lcore(void *args);

extern struct ipsec_sa *sa_out;
extern uint32_t nb_sa_out;

extern struct ipsec_sa *sa_in;
extern uint32_t nb_sa_in;

uint16_t
ipsec_inbound(struct ipsec_ctx *ctx, struct rte_mbuf *pkts[],
		uint16_t nb_pkts, uint16_t len);

uint16_t
ipsec_outbound(struct ipsec_ctx *ctx, struct rte_mbuf *pkts[],
		uint32_t sa_idx[], uint16_t nb_pkts, uint16_t len);

uint16_t
ipsec_inbound_cqp_dequeue(struct ipsec_ctx *ctx, struct rte_mbuf *pkts[],
		uint16_t len);

uint16_t
ipsec_outbound_cqp_dequeue(struct ipsec_ctx *ctx, struct rte_mbuf *pkts[],
		uint16_t len);

void
ipsec_process(struct ipsec_ctx *ctx, struct ipsec_traffic *trf);

void
ipsec_cqp_process(struct ipsec_ctx *ctx, struct ipsec_traffic *trf);

static inline uint16_t
ipsec_metadata_size(void)
{
	return sizeof(struct ipsec_mbuf_metadata);
}

static inline struct ipsec_mbuf_metadata *
get_priv(struct rte_mbuf *m)
{
	return rte_mbuf_to_priv(m);
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

static inline struct rte_ipsec_session *
ipsec_get_primary_session(struct ipsec_sa *sa)
{
	return &sa->sessions[IPSEC_SESSION_PRIMARY];
}

static inline struct rte_ipsec_session *
ipsec_get_fallback_session(struct ipsec_sa *sa)
{
	return &sa->sessions[IPSEC_SESSION_FALLBACK];
}

static inline enum rte_security_session_action_type
ipsec_get_action_type(struct ipsec_sa *sa)
{
	struct rte_ipsec_session *ips;
	ips = ipsec_get_primary_session(sa);
	return ips->type;
}

int
inbound_sa_check(struct sa_ctx *sa_ctx, struct rte_mbuf *m, uint32_t sa_idx);

void
inbound_sa_lookup(struct sa_ctx *sa_ctx, struct rte_mbuf *pkts[],
		void *sa[], uint16_t nb_pkts);

void
outbound_sa_lookup(struct sa_ctx *sa_ctx, uint32_t sa_idx[],
		void *sa[], uint16_t nb_pkts);

void
sp4_init(struct socket_ctx *ctx, int32_t socket_id);

void
sp6_init(struct socket_ctx *ctx, int32_t socket_id);

/*
 * Search through SP rules for given SPI.
 * Returns first rule index if found(greater or equal then zero),
 * or -ENOENT otherwise.
 */
int
sp4_spi_present(uint32_t spi, int inbound, struct ip_addr ip_addr[2],
			uint32_t mask[2]);
int
sp6_spi_present(uint32_t spi, int inbound, struct ip_addr ip_addr[2],
			uint32_t mask[2]);

/*
 * Search through SA entries for given SPI.
 * Returns first entry index if found(greater or equal then zero),
 * or -ENOENT otherwise.
 */
int
sa_spi_present(struct sa_ctx *sa_ctx, uint32_t spi, int inbound);

void
sa_init(struct socket_ctx *ctx, int32_t socket_id);

void
rt_init(struct socket_ctx *ctx, int32_t socket_id);

int
sa_check_offloads(uint16_t port_id, uint64_t *rx_offloads,
		uint64_t *tx_offloads);

int
add_dst_ethaddr(uint16_t port, const struct rte_ether_addr *addr);

void
enqueue_cop_burst(struct cdev_qp *cqp);

int
create_lookaside_session(struct ipsec_ctx *ipsec_ctx, struct ipsec_sa *sa,
		struct rte_ipsec_session *ips);

int
create_inline_session(struct socket_ctx *skt_ctx, struct ipsec_sa *sa,
		struct rte_ipsec_session *ips);
int
check_flow_params(uint16_t fdir_portid, uint8_t fdir_qid);

int
create_ipsec_esp_flow(struct ipsec_sa *sa);

uint32_t
get_nb_crypto_sessions(void);

#endif /* __IPSEC_H__ */
