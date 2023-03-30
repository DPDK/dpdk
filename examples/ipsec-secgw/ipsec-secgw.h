/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */
#ifndef _IPSEC_SECGW_H_
#define _IPSEC_SECGW_H_

#include <stdbool.h>
#include <rte_ethdev.h>

#define MAX_RX_QUEUE_PER_LCORE 16

#define NB_SOCKETS 4

#define MAX_PKT_BURST 32
#define MAX_PKT_BURST_VEC 256

#define MAX_PKTS                                  \
	((MAX_PKT_BURST_VEC > MAX_PKT_BURST ?     \
	  MAX_PKT_BURST_VEC : MAX_PKT_BURST) * 2)

#define RTE_LOGTYPE_IPSEC RTE_LOGTYPE_USER1

#if RTE_BYTE_ORDER != RTE_LITTLE_ENDIAN
#define __BYTES_TO_UINT64(a, b, c, d, e, f, g, h) \
	(((uint64_t)((a) & 0xff) << 56) | \
	((uint64_t)((b) & 0xff) << 48) | \
	((uint64_t)((c) & 0xff) << 40) | \
	((uint64_t)((d) & 0xff) << 32) | \
	((uint64_t)((e) & 0xff) << 24) | \
	((uint64_t)((f) & 0xff) << 16) | \
	((uint64_t)((g) & 0xff) << 8)  | \
	((uint64_t)(h) & 0xff))
#else
#define __BYTES_TO_UINT64(a, b, c, d, e, f, g, h) \
	(((uint64_t)((h) & 0xff) << 56) | \
	((uint64_t)((g) & 0xff) << 48) | \
	((uint64_t)((f) & 0xff) << 40) | \
	((uint64_t)((e) & 0xff) << 32) | \
	((uint64_t)((d) & 0xff) << 24) | \
	((uint64_t)((c) & 0xff) << 16) | \
	((uint64_t)((b) & 0xff) << 8) | \
	((uint64_t)(a) & 0xff))
#endif

#define uint32_t_to_char(ip, a, b, c, d) do {\
		*a = (uint8_t)(ip >> 24 & 0xff);\
		*b = (uint8_t)(ip >> 16 & 0xff);\
		*c = (uint8_t)(ip >> 8 & 0xff);\
		*d = (uint8_t)(ip & 0xff);\
	} while (0)

#define ETHADDR(a, b, c, d, e, f) (__BYTES_TO_UINT64(a, b, c, d, e, f, 0, 0))

#define IPSEC_NAT_T_PORT 4500
#define MBUF_PTYPE_TUNNEL_ESP_IN_UDP (RTE_PTYPE_TUNNEL_ESP | RTE_PTYPE_L4_UDP)

struct traffic_type {
	uint32_t num;
	struct rte_mbuf *pkts[MAX_PKTS];
	const uint8_t *data[MAX_PKTS];
	void *saptr[MAX_PKTS];
	uint32_t res[MAX_PKTS];
} __rte_cache_aligned;

struct ipsec_traffic {
	struct traffic_type ipsec;
	struct traffic_type ip4;
	struct traffic_type ip6;
};

/* Fields optimized for devices without burst */
struct traffic_type_nb {
	const uint8_t *data;
	struct rte_mbuf *pkt;
	uint32_t res;
	uint32_t num;
};

struct ipsec_traffic_nb {
	struct traffic_type_nb ipsec;
	struct traffic_type_nb ip4;
	struct traffic_type_nb ip6;
};

/* port/source ethernet addr and destination ethernet addr */
struct ethaddr_info {
	struct rte_ether_addr src, dst;
};

struct ipsec_spd_stats {
	uint64_t protect;
	uint64_t bypass;
	uint64_t discard;
};

struct ipsec_sa_stats {
	uint64_t hit;
	uint64_t miss;
};

struct ipsec_core_statistics {
	uint64_t tx;
	uint64_t rx;
	uint64_t rx_call;
	uint64_t tx_call;
	uint64_t dropped;
	uint64_t frag_dropped;
	uint64_t burst_rx;

	struct {
		struct ipsec_spd_stats spd4;
		struct ipsec_spd_stats spd6;
		struct ipsec_sa_stats sad;
	} outbound;

	struct {
		struct ipsec_spd_stats spd4;
		struct ipsec_spd_stats spd6;
		struct ipsec_sa_stats sad;
	} inbound;

	struct {
		uint64_t miss;
	} lpm4;

	struct {
		uint64_t miss;
	} lpm6;
} __rte_cache_aligned;

extern struct ipsec_core_statistics core_statistics[RTE_MAX_LCORE];

extern struct ethaddr_info ethaddr_tbl[RTE_MAX_ETHPORTS];

/* Port mask to identify the unprotected ports */
extern uint32_t unprotected_port_mask;

/* Index of SA in single mode */
extern uint32_t single_sa_idx;
extern uint32_t single_sa;

extern volatile bool force_quit;

extern uint32_t nb_bufs_in_pool;

extern bool per_port_pool;
extern int ip_reassembly_dynfield_offset;
extern uint64_t ip_reassembly_dynflag;
extern uint32_t mtu_size;
extern uint32_t frag_tbl_sz;
extern uint32_t qp_desc_nb;

#define SS_F		(1U << 0)	/* Single SA mode */
#define INL_PR_F	(1U << 1)	/* Inline Protocol */
#define INL_CR_F	(1U << 2)	/* Inline Crypto */
#define LA_PR_F		(1U << 3)	/* Lookaside Protocol */
#define LA_ANY_F	(1U << 4)	/* Lookaside Any */
#define MAX_F		(LA_ANY_F << 1)

extern uint16_t wrkr_flags;

static inline uint8_t
is_unprotected_port(uint16_t port_id)
{
	return unprotected_port_mask & (1 << port_id);
}

static inline void
core_stats_update_rx(int n)
{
	int lcore_id = rte_lcore_id();
	core_statistics[lcore_id].rx += n;
	core_statistics[lcore_id].rx_call++;
	if (n == MAX_PKT_BURST)
		core_statistics[lcore_id].burst_rx += n;
}

static inline void
core_stats_update_tx(int n)
{
	int lcore_id = rte_lcore_id();
	core_statistics[lcore_id].tx += n;
	core_statistics[lcore_id].tx_call++;
}

static inline void
core_stats_update_drop(int n)
{
	int lcore_id = rte_lcore_id();
	core_statistics[lcore_id].dropped += n;
}

static inline void
core_stats_update_frag_drop(int n)
{
	int lcore_id = rte_lcore_id();
	core_statistics[lcore_id].frag_dropped += n;
}

static inline int
is_ip_reassembly_incomplete(struct rte_mbuf *mbuf)
{
	if (ip_reassembly_dynflag == 0)
		return -1;
	return (mbuf->ol_flags & ip_reassembly_dynflag) != 0;
}

static inline void
free_reassembly_fail_pkt(struct rte_mbuf *mb)
{
	if (ip_reassembly_dynfield_offset >= 0) {
		rte_eth_ip_reassembly_dynfield_t dynfield;
		uint32_t frag_cnt = 0;

		while (mb) {
			dynfield = *RTE_MBUF_DYNFIELD(mb,
					ip_reassembly_dynfield_offset,
					rte_eth_ip_reassembly_dynfield_t *);
			rte_pktmbuf_free(mb);
			mb = dynfield.next_frag;
			frag_cnt++;
		}

		core_stats_update_frag_drop(frag_cnt);
	} else {
		rte_pktmbuf_free(mb);
		core_stats_update_drop(1);
	}
}

/* helper routine to free bulk of packets */
static inline void
free_pkts(struct rte_mbuf *mb[], uint32_t n)
{
	uint32_t i;

	for (i = 0; i != n; i++)
		rte_pktmbuf_free(mb[i]);

	core_stats_update_drop(n);
}

#endif /* _IPSEC_SECGW_H_ */
