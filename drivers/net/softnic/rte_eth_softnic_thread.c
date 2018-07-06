/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdlib.h>

#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_ring.h>

#include "rte_eth_softnic_internals.h"

/**
 * Master thread: data plane thread init
 */
void
softnic_thread_free(struct pmd_internals *softnic)
{
	uint32_t i;

	RTE_LCORE_FOREACH_SLAVE(i) {
		struct softnic_thread *t = &softnic->thread[i];

		/* MSGQs */
		if (t->msgq_req)
			rte_ring_free(t->msgq_req);

		if (t->msgq_rsp)
			rte_ring_free(t->msgq_rsp);
	}
}

int
softnic_thread_init(struct pmd_internals *softnic)
{
	uint32_t i;

	RTE_LCORE_FOREACH_SLAVE(i) {
		char ring_name[NAME_MAX];
		struct rte_ring *msgq_req, *msgq_rsp;
		struct softnic_thread *t = &softnic->thread[i];
		struct softnic_thread_data *t_data = &softnic->thread_data[i];
		uint32_t cpu_id = rte_lcore_to_socket_id(i);

		/* MSGQs */
		snprintf(ring_name, sizeof(ring_name), "%s-TH%u-REQ",
			softnic->params.name,
			i);

		msgq_req = rte_ring_create(ring_name,
			THREAD_MSGQ_SIZE,
			cpu_id,
			RING_F_SP_ENQ | RING_F_SC_DEQ);

		if (msgq_req == NULL) {
			softnic_thread_free(softnic);
			return -1;
		}

		snprintf(ring_name, sizeof(ring_name), "%s-TH%u-RSP",
			softnic->params.name,
			i);

		msgq_rsp = rte_ring_create(ring_name,
			THREAD_MSGQ_SIZE,
			cpu_id,
			RING_F_SP_ENQ | RING_F_SC_DEQ);

		if (msgq_rsp == NULL) {
			softnic_thread_free(softnic);
			return -1;
		}

		/* Master thread records */
		t->msgq_req = msgq_req;
		t->msgq_rsp = msgq_rsp;
		t->enabled = 1;

		/* Data plane thread records */
		t_data->n_pipelines = 0;
		t_data->msgq_req = msgq_req;
		t_data->msgq_rsp = msgq_rsp;
		t_data->timer_period =
			(rte_get_tsc_hz() * THREAD_TIMER_PERIOD_MS) / 1000;
		t_data->time_next = rte_get_tsc_cycles() + t_data->timer_period;
		t_data->time_next_min = t_data->time_next;
	}

	return 0;
}
