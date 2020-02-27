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

#define ETHADDR(a, b, c, d, e, f) (__BYTES_TO_UINT64(a, b, c, d, e, f, 0, 0))

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

extern struct ethaddr_info ethaddr_tbl[RTE_MAX_ETHPORTS];

/* Port mask to identify the unprotected ports */
extern uint32_t unprotected_port_mask;

/* Index of SA in single mode */
extern uint32_t single_sa_idx;

extern volatile bool force_quit;

static inline uint8_t
is_unprotected_port(uint16_t port_id)
{
	return unprotected_port_mask & (1 << port_id);
}

#endif /* _IPSEC_SECGW_H_ */
