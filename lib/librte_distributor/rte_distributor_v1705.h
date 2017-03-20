/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
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
