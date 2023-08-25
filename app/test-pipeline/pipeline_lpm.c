/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <rte_log.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_byteorder.h>

#include <rte_port_ring.h>
#include <rte_table_lpm.h>
#include <rte_pipeline.h>

#include "main.h"

#ifndef PIPELINE_LPM_TABLE_NUMBER_TABLE8s
#define PIPELINE_LPM_TABLE_NUMBER_TABLE8s 256
#endif

void
app_main_loop_worker_pipeline_lpm(void) {
	struct rte_pipeline_params pipeline_params = {
		.name = "pipeline",
		.socket_id = rte_socket_id(),
	};

	struct rte_pipeline *p;
	uint32_t port_in_id[APP_MAX_PORTS];
	uint32_t port_out_id[APP_MAX_PORTS];
	uint32_t table_id;
	uint32_t i;

	RTE_LOG(INFO, USER1, "Core %u is doing work (pipeline with "
		"LPM table)\n", rte_lcore_id());

	/* Pipeline configuration */
	p = rte_pipeline_create(&pipeline_params);
	if (p == NULL)
		rte_panic("Unable to configure the pipeline\n");

	/* Input port configuration */
	for (i = 0; i < app.n_ports; i++) {
		struct rte_port_ring_reader_params port_ring_params = {
			.ring = app.rings_rx[i],
		};

		struct rte_pipeline_port_in_params port_params = {
			.ops = &rte_port_ring_reader_ops,
			.arg_create = (void *) &port_ring_params,
			.f_action = NULL,
			.arg_ah = NULL,
			.burst_size = app.burst_size_worker_read,
		};

		if (rte_pipeline_port_in_create(p, &port_params,
			&port_in_id[i]))
			rte_panic("Unable to configure input port for "
				"ring %d\n", i);
	}

	/* Output port configuration */
	for (i = 0; i < app.n_ports; i++) {
		struct rte_port_ring_writer_params port_ring_params = {
			.ring = app.rings_tx[i],
			.tx_burst_sz = app.burst_size_worker_write,
		};

		struct rte_pipeline_port_out_params port_params = {
			.ops = &rte_port_ring_writer_ops,
			.arg_create = (void *) &port_ring_params,
			.f_action = NULL,
			.arg_ah = NULL,
		};

		if (rte_pipeline_port_out_create(p, &port_params,
			&port_out_id[i]))
			rte_panic("Unable to configure output port for "
				"ring %d\n", i);
	}

	/* Table configuration */
	{
		struct rte_table_lpm_params table_lpm_params = {
			.name = "LPM",
			.n_rules = 1 << 24,
			.number_tbl8s = PIPELINE_LPM_TABLE_NUMBER_TABLE8s,
			.flags = 0,
			.entry_unique_size =
				sizeof(struct rte_pipeline_table_entry),
			.offset = APP_METADATA_OFFSET(32),
		};

		struct rte_pipeline_table_params table_params = {
			.ops = &rte_table_lpm_ops,
			.arg_create = &table_lpm_params,
			.f_action_hit = NULL,
			.f_action_miss = NULL,
			.arg_ah = NULL,
			.action_data_size = 0,
		};

		if (rte_pipeline_table_create(p, &table_params, &table_id))
			rte_panic("Unable to configure the LPM table\n");
	}

	/* Interconnecting ports and tables */
	for (i = 0; i < app.n_ports; i++)
		if (rte_pipeline_port_in_connect_to_table(p, port_in_id[i],
			table_id))
			rte_panic("Unable to connect input port %u to "
				"table %u\n", port_in_id[i],  table_id);

	/* Add entries to tables */
	for (i = 0; i < app.n_ports; i++) {
		struct rte_pipeline_table_entry entry = {
			.action = RTE_PIPELINE_ACTION_PORT,
			{.port_id = port_out_id[i & (app.n_ports - 1)]},
		};

		struct rte_table_lpm_key key = {
			.ip = i << (24 - rte_popcount32(app.n_ports - 1)),
			.depth = 8 + rte_popcount32(app.n_ports - 1),
		};

		struct rte_pipeline_table_entry *entry_ptr;

		int key_found, status;

		printf("Adding rule to LPM table (IPv4 destination = %"
			PRIu32 ".%" PRIu32 ".%" PRIu32 ".%" PRIu32 "/%" PRIu8
			" => port out = %" PRIu32 ")\n",
			(key.ip & 0xFF000000) >> 24,
			(key.ip & 0x00FF0000) >> 16,
			(key.ip & 0x0000FF00) >> 8,
			key.ip & 0x000000FF,
			key.depth,
			i);

		status = rte_pipeline_table_entry_add(p, table_id, &key, &entry,
			&key_found, &entry_ptr);
		if (status < 0)
			rte_panic("Unable to add entry to table %u (%d)\n",
				table_id, status);
	}

	/* Enable input ports */
	for (i = 0; i < app.n_ports; i++)
		if (rte_pipeline_port_in_enable(p, port_in_id[i]))
			rte_panic("Unable to enable input port %u\n",
				port_in_id[i]);

	/* Check pipeline consistency */
	if (rte_pipeline_check(p) < 0)
		rte_panic("Pipeline consistency check failed\n");

	/* Run-time */
#if APP_FLUSH == 0
	for ( ; ; )
		rte_pipeline_run(p);
#else
	for (i = 0; ; i++) {
		rte_pipeline_run(p);

		if ((i & APP_FLUSH) == 0)
			rte_pipeline_flush(p);
	}
#endif
}
