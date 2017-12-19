/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _RTE_DISTRIB_V1705_H_
#define _RTE_DISTRIB_V1705_H_

/**
 * @file
 * RTE distributor
 *
 * The distributor is a component which is designed to pass packets
 * one-at-a-time to workers, with dynamic load balancing.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct rte_distributor *
rte_distributor_create_v1705(const char *name, unsigned int socket_id,
		unsigned int num_workers,
		unsigned int alg_type);

int
rte_distributor_process_v1705(struct rte_distributor *d,
		struct rte_mbuf **mbufs, unsigned int num_mbufs);

int
rte_distributor_returned_pkts_v1705(struct rte_distributor *d,
		struct rte_mbuf **mbufs, unsigned int max_mbufs);

int
rte_distributor_flush_v1705(struct rte_distributor *d);

void
rte_distributor_clear_returns_v1705(struct rte_distributor *d);

int
rte_distributor_get_pkt_v1705(struct rte_distributor *d,
	unsigned int worker_id, struct rte_mbuf **pkts,
	struct rte_mbuf **oldpkt, unsigned int retcount);

int
rte_distributor_return_pkt_v1705(struct rte_distributor *d,
	unsigned int worker_id, struct rte_mbuf **oldpkt, int num);

void
rte_distributor_request_pkt_v1705(struct rte_distributor *d,
		unsigned int worker_id, struct rte_mbuf **oldpkt,
		unsigned int count);

int
rte_distributor_poll_pkt_v1705(struct rte_distributor *d,
		unsigned int worker_id, struct rte_mbuf **mbufs);

#ifdef __cplusplus
}
#endif

#endif
