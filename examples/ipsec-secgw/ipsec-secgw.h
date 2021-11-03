/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */
#ifndef _IPSEC_SECGW_H_
#define _IPSEC_SECGW_H_

#include <stdbool.h>


#define NB_SOCKETS 4

#define MAX_PKT_BURST 32

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
	const uint8_t *data[MAX_PKT_BURST * 2];
	struct rte_mbuf *pkts[MAX_PKT_BURST * 2];
	void *saptr[MAX_PKT_BURST * 2];
	uint32_t res[MAX_PKT_BURST * 2];
	uint32_t num;
};

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
	uint64_t src, dst;
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

extern volatile bool force_quit;

extern uint32_t nb_bufs_in_pool;

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
