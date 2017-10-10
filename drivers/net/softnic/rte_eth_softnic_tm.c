/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_malloc.h>

#include "rte_eth_softnic_internals.h"
#include "rte_eth_softnic.h"

#define BYTES_IN_MBPS (1000 * 1000 / 8)

int
tm_params_check(struct pmd_params *params, uint32_t hard_rate)
{
	uint64_t hard_rate_bytes_per_sec = hard_rate * BYTES_IN_MBPS;
	uint32_t i;

	/* rate */
	if (params->soft.tm.rate) {
		if (params->soft.tm.rate > hard_rate_bytes_per_sec)
			return -EINVAL;
	} else {
		params->soft.tm.rate =
			(hard_rate_bytes_per_sec > UINT32_MAX) ?
				UINT32_MAX : hard_rate_bytes_per_sec;
	}

	/* nb_queues */
	if (params->soft.tm.nb_queues == 0)
		return -EINVAL;

	if (params->soft.tm.nb_queues < RTE_SCHED_QUEUES_PER_PIPE)
		params->soft.tm.nb_queues = RTE_SCHED_QUEUES_PER_PIPE;

	params->soft.tm.nb_queues =
		rte_align32pow2(params->soft.tm.nb_queues);

	/* qsize */
	for (i = 0; i < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; i++) {
		if (params->soft.tm.qsize[i] == 0)
			return -EINVAL;

		params->soft.tm.qsize[i] =
			rte_align32pow2(params->soft.tm.qsize[i]);
	}

	/* enq_bsz, deq_bsz */
	if (params->soft.tm.enq_bsz == 0 ||
		params->soft.tm.deq_bsz == 0 ||
		params->soft.tm.deq_bsz >= params->soft.tm.enq_bsz)
		return -EINVAL;

	return 0;
}

int
tm_init(struct pmd_internals *p,
	struct pmd_params *params,
	int numa_node)
{
	uint32_t enq_bsz = params->soft.tm.enq_bsz;
	uint32_t deq_bsz = params->soft.tm.deq_bsz;

	p->soft.tm.pkts_enq = rte_zmalloc_socket(params->soft.name,
		2 * enq_bsz * sizeof(struct rte_mbuf *),
		0,
		numa_node);

	if (p->soft.tm.pkts_enq == NULL)
		return -ENOMEM;

	p->soft.tm.pkts_deq = rte_zmalloc_socket(params->soft.name,
		deq_bsz * sizeof(struct rte_mbuf *),
		0,
		numa_node);

	if (p->soft.tm.pkts_deq == NULL) {
		rte_free(p->soft.tm.pkts_enq);
		return -ENOMEM;
	}

	return 0;
}

void
tm_free(struct pmd_internals *p)
{
	rte_free(p->soft.tm.pkts_enq);
	rte_free(p->soft.tm.pkts_deq);
}

int
tm_start(struct pmd_internals *p)
{
	struct tm_params *t = &p->soft.tm.params;
	uint32_t n_subports, subport_id;
	int status;

	/* Port */
	p->soft.tm.sched = rte_sched_port_config(&t->port_params);
	if (p->soft.tm.sched == NULL)
		return -1;

	/* Subport */
	n_subports = t->port_params.n_subports_per_port;
	for (subport_id = 0; subport_id < n_subports; subport_id++) {
		uint32_t n_pipes_per_subport =
			t->port_params.n_pipes_per_subport;
		uint32_t pipe_id;

		status = rte_sched_subport_config(p->soft.tm.sched,
			subport_id,
			&t->subport_params[subport_id]);
		if (status) {
			rte_sched_port_free(p->soft.tm.sched);
			return -1;
		}

		/* Pipe */
		n_pipes_per_subport = t->port_params.n_pipes_per_subport;
		for (pipe_id = 0; pipe_id < n_pipes_per_subport; pipe_id++) {
			int pos = subport_id * TM_MAX_PIPES_PER_SUBPORT +
				pipe_id;
			int profile_id = t->pipe_to_profile[pos];

			if (profile_id < 0)
				continue;

			status = rte_sched_pipe_config(p->soft.tm.sched,
				subport_id,
				pipe_id,
				profile_id);
			if (status) {
				rte_sched_port_free(p->soft.tm.sched);
				return -1;
			}
		}
	}

	return 0;
}

void
tm_stop(struct pmd_internals *p)
{
	if (p->soft.tm.sched)
		rte_sched_port_free(p->soft.tm.sched);
}
