/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <rte_malloc.h>
#include <rte_log.h>

#include <rte_port_ring.h>
#include <rte_table_stub.h>
#include <rte_pipeline.h>

#include "main.h"

void
app_main_loop_pipeline_passthrough(void) {
	struct rte_pipeline_params pipeline_params = {
		.name = "pipeline",
		.socket_id = rte_socket_id(),
	};

	struct rte_pipeline *p;
	uint32_t port_in_id[APP_MAX_PORTS];
	uint32_t port_out_id[APP_MAX_PORTS];
	uint32_t table_id[APP_MAX_PORTS];
	uint32_t i;

	uint32_t core_id = rte_lcore_id();
	struct app_core_params *core_params = app_get_core_params(core_id);

	if ((core_params == NULL) || (core_params->core_type != APP_CORE_PT))
		rte_panic("Core %u misconfiguration\n", core_id);

	RTE_LOG(INFO, USER1, "Core %u is doing pass-through\n", core_id);

	/* Pipeline configuration */
	p = rte_pipeline_create(&pipeline_params);
	if (p == NULL)
		rte_panic("%s: Unable to configure the pipeline\n", __func__);

	/* Input port configuration */
	for (i = 0; i < app.n_ports; i++) {
		struct rte_port_ring_reader_params port_ring_params = {
			.ring = app.rings[core_params->swq_in[i]],
		};

		struct rte_pipeline_port_in_params port_params = {
			.ops = &rte_port_ring_reader_ops,
			.arg_create = (void *) &port_ring_params,
			.f_action = NULL,
			.arg_ah = NULL,
			.burst_size = app.bsz_swq_rd,
		};

		if (rte_pipeline_port_in_create(p, &port_params,
			&port_in_id[i])) {
			rte_panic("%s: Unable to configure input port for "
				"ring %d\n", __func__, i);
		}
	}

	/* Output port configuration */
	for (i = 0; i < app.n_ports; i++) {
		struct rte_port_ring_writer_params port_ring_params = {
			.ring = app.rings[core_params->swq_out[i]],
			.tx_burst_sz = app.bsz_swq_wr,
		};

		struct rte_pipeline_port_out_params port_params = {
			.ops = &rte_port_ring_writer_ops,
			.arg_create = (void *) &port_ring_params,
			.f_action = NULL,
			.f_action_bulk = NULL,
			.arg_ah = NULL,
		};

		if (rte_pipeline_port_out_create(p, &port_params,
			&port_out_id[i])) {
			rte_panic("%s: Unable to configure output port for "
				"ring %d\n", __func__, i);
		}
	}

	/* Table configuration */
	for (i = 0; i < app.n_ports; i++) {
		struct rte_pipeline_table_params table_params = {
			.ops = &rte_table_stub_ops,
			.arg_create = NULL,
			.f_action_hit = NULL,
			.f_action_miss = NULL,
			.arg_ah = NULL,
			.action_data_size = 0,
		};

		if (rte_pipeline_table_create(p, &table_params, &table_id[i]))
			rte_panic("%s: Unable to configure table %u\n",
				__func__, i);
	}

	/* Interconnecting ports and tables */
	for (i = 0; i < app.n_ports; i++) {
		if (rte_pipeline_port_in_connect_to_table(p, port_in_id[i],
			table_id[i])) {
			rte_panic("%s: Unable to connect input port %u to "
				"table %u\n", __func__, port_in_id[i],
				table_id[i]);
		}
	}

	/* Add entries to tables */
	for (i = 0; i < app.n_ports; i++) {
		struct rte_pipeline_table_entry default_entry = {
			.action = RTE_PIPELINE_ACTION_PORT,
			{.port_id = port_out_id[i]},
		};

		struct rte_pipeline_table_entry *default_entry_ptr;

		if (rte_pipeline_table_default_entry_add(p, table_id[i],
			&default_entry, &default_entry_ptr))
			rte_panic("%s: Unable to add default entry to "
				"table %u\n", __func__, table_id[i]);
	}

	/* Enable input ports */
	for (i = 0; i < app.n_ports; i++)
		if (rte_pipeline_port_in_enable(p, port_in_id[i]))
			rte_panic("Unable to enable input port %u\n",
				port_in_id[i]);

	/* Check pipeline consistency */
	if (rte_pipeline_check(p) < 0)
		rte_panic("%s: Pipeline consistency check failed\n", __func__);

	/* Run-time */
	for (i = 0; ; i++) {
		rte_pipeline_run(p);

		if ((i & APP_FLUSH) == 0)
			rte_pipeline_flush(p);
	}
}

void
app_main_loop_passthrough(void) {
	struct app_mbuf_array *m;
	uint32_t i;

	uint32_t core_id = rte_lcore_id();
	struct app_core_params *core_params = app_get_core_params(core_id);

	if ((core_params == NULL) || (core_params->core_type != APP_CORE_PT))
		rte_panic("Core %u misconfiguration\n", core_id);

	RTE_LOG(INFO, USER1, "Core %u is doing pass-through (no pipeline)\n",
		core_id);

	m = rte_malloc_socket(NULL, sizeof(struct app_mbuf_array),
		RTE_CACHE_LINE_SIZE, rte_socket_id());
	if (m == NULL)
		rte_panic("%s: cannot allocate buffer space\n", __func__);

	for (i = 0; ; i = ((i + 1) & (app.n_ports - 1))) {
		int ret;

		ret = rte_ring_sc_dequeue_bulk(
			app.rings[core_params->swq_in[i]],
			(void **) m->array,
			app.bsz_swq_rd);

		if (ret == -ENOENT)
			continue;

		do {
			ret = rte_ring_sp_enqueue_bulk(
				app.rings[core_params->swq_out[i]],
				(void **) m->array,
				app.bsz_swq_wr);
		} while (ret < 0);
	}
}
