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
#include <rte_table_acl.h>
#include <rte_pipeline.h>

#include "main.h"

enum {
	PROTO_FIELD_IPV4,
	SRC_FIELD_IPV4,
	DST_FIELD_IPV4,
	SRCP_FIELD_IPV4,
	DSTP_FIELD_IPV4,
	NUM_FIELDS_IPV4
};

/*
 * Here we define the 'shape' of the data we're searching for,
 * by defining the meta-data of the ACL rules.
 * in this case, we're defining 5 tuples. IP addresses, ports,
 * and protocol.
 */
struct rte_acl_field_def ipv4_field_formats[NUM_FIELDS_IPV4] = {
	{
		.type = RTE_ACL_FIELD_TYPE_BITMASK,
		.size = sizeof(uint8_t),
		.field_index = PROTO_FIELD_IPV4,
		.input_index = PROTO_FIELD_IPV4,
		.offset = sizeof(struct ether_hdr) +
			offsetof(struct ipv4_hdr, next_proto_id),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = SRC_FIELD_IPV4,
		.input_index = SRC_FIELD_IPV4,
		.offset = sizeof(struct ether_hdr) +
			offsetof(struct ipv4_hdr, src_addr),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = DST_FIELD_IPV4,
		.input_index = DST_FIELD_IPV4,
		.offset = sizeof(struct ether_hdr) +
			offsetof(struct ipv4_hdr, dst_addr),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = SRCP_FIELD_IPV4,
		.input_index = SRCP_FIELD_IPV4,
		.offset = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = DSTP_FIELD_IPV4,
		.input_index = SRCP_FIELD_IPV4,
		.offset = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) +
			sizeof(uint16_t),
	},
};



void
app_main_loop_worker_pipeline_acl(void) {
	struct rte_pipeline_params pipeline_params = {
		.name = "pipeline",
		.socket_id = rte_socket_id(),
	};

	struct rte_pipeline *p;
	uint32_t port_in_id[APP_MAX_PORTS];
	uint32_t port_out_id[APP_MAX_PORTS];
	uint32_t table_id;
	uint32_t i;

	RTE_LOG(INFO, USER1,
		"Core %u is doing work (pipeline with ACL table)\n",
		rte_lcore_id());

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
		struct rte_table_acl_params table_acl_params = {
			.name = "test", /* unique identifier for acl contexts */
			.n_rules = 1 << 5,
			.n_rule_fields = DIM(ipv4_field_formats),
		};

		/* Copy in the rule meta-data defined above into the params */
		memcpy(table_acl_params.field_format, ipv4_field_formats,
			sizeof(ipv4_field_formats));

		struct rte_pipeline_table_params table_params = {
			.ops = &rte_table_acl_ops,
			.arg_create = &table_acl_params,
			.f_action_hit = NULL,
			.f_action_miss = NULL,
			.arg_ah = NULL,
			.action_data_size = 0,
		};

		if (rte_pipeline_table_create(p, &table_params, &table_id))
			rte_panic("Unable to configure the ACL table\n");
	}

	/* Interconnecting ports and tables */
	for (i = 0; i < app.n_ports; i++)
		if (rte_pipeline_port_in_connect_to_table(p, port_in_id[i],
			table_id))
			rte_panic("Unable to connect input port %u to "
				"table %u\n", port_in_id[i],  table_id);

	/* Add entries to tables */
	for (i = 0; i < app.n_ports; i++) {
		struct rte_pipeline_table_entry table_entry = {
			.action = RTE_PIPELINE_ACTION_PORT,
			{.port_id = port_out_id[i & (app.n_ports - 1)]},
		};
		struct rte_table_acl_rule_add_params rule_params;
		struct rte_pipeline_table_entry *entry_ptr;
		int key_found, ret;

		memset(&rule_params, 0, sizeof(rule_params));

		/* Set the rule values */
		rule_params.field_value[SRC_FIELD_IPV4].value.u32 = 0;
		rule_params.field_value[SRC_FIELD_IPV4].mask_range.u32 = 0;
		rule_params.field_value[DST_FIELD_IPV4].value.u32 =
			i << (24 - __builtin_popcount(app.n_ports - 1));
		rule_params.field_value[DST_FIELD_IPV4].mask_range.u32 =
			8 + __builtin_popcount(app.n_ports - 1);
		rule_params.field_value[SRCP_FIELD_IPV4].value.u16 = 0;
		rule_params.field_value[SRCP_FIELD_IPV4].mask_range.u16 =
			UINT16_MAX;
		rule_params.field_value[DSTP_FIELD_IPV4].value.u16 = 0;
		rule_params.field_value[DSTP_FIELD_IPV4].mask_range.u16 =
			UINT16_MAX;
		rule_params.field_value[PROTO_FIELD_IPV4].value.u8 = 0;
		rule_params.field_value[PROTO_FIELD_IPV4].mask_range.u8 = 0;

		rule_params.priority = 0;

		uint32_t dst_addr = rule_params.field_value[DST_FIELD_IPV4].
			value.u32;
		uint32_t dst_mask =
			rule_params.field_value[DST_FIELD_IPV4].mask_range.u32;

		printf("Adding rule to ACL table (IPv4 destination = "
			"%u.%u.%u.%u/%u => port out = %u)\n",
			(dst_addr & 0xFF000000) >> 24,
			(dst_addr & 0x00FF0000) >> 16,
			(dst_addr & 0x0000FF00) >> 8,
			dst_addr & 0x000000FF,
			dst_mask,
			table_entry.port_id);

		/* For ACL, add needs an rte_table_acl_rule_add_params struct */
		ret = rte_pipeline_table_entry_add(p, table_id, &rule_params,
			&table_entry, &key_found, &entry_ptr);
		if (ret < 0)
			rte_panic("Unable to add entry to table %u (%d)\n",
				table_id, ret);
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
