/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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

#include <string.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_byteorder.h>
#include <rte_table_acl.h>

#include "pipeline_firewall_be.h"

struct pipeline_firewall {
	struct pipeline p;
	pipeline_msg_req_handler custom_handlers[PIPELINE_FIREWALL_MSG_REQS];

	uint32_t n_rules;
	uint32_t n_rule_fields;
	struct rte_acl_field_def *field_format;
	uint32_t field_format_size;
} __rte_cache_aligned;

static void *
pipeline_firewall_msg_req_custom_handler(struct pipeline *p, void *msg);

static pipeline_msg_req_handler handlers[] = {
	[PIPELINE_MSG_REQ_PING] =
		pipeline_msg_req_ping_handler,
	[PIPELINE_MSG_REQ_STATS_PORT_IN] =
		pipeline_msg_req_stats_port_in_handler,
	[PIPELINE_MSG_REQ_STATS_PORT_OUT] =
		pipeline_msg_req_stats_port_out_handler,
	[PIPELINE_MSG_REQ_STATS_TABLE] =
		pipeline_msg_req_stats_table_handler,
	[PIPELINE_MSG_REQ_PORT_IN_ENABLE] =
		pipeline_msg_req_port_in_enable_handler,
	[PIPELINE_MSG_REQ_PORT_IN_DISABLE] =
		pipeline_msg_req_port_in_disable_handler,
	[PIPELINE_MSG_REQ_CUSTOM] =
		pipeline_firewall_msg_req_custom_handler,
};

static void *
pipeline_firewall_msg_req_add_handler(struct pipeline *p, void *msg);

static void *
pipeline_firewall_msg_req_del_handler(struct pipeline *p, void *msg);

static void *
pipeline_firewall_msg_req_add_default_handler(struct pipeline *p, void *msg);

static void *
pipeline_firewall_msg_req_del_default_handler(struct pipeline *p, void *msg);

static pipeline_msg_req_handler custom_handlers[] = {
	[PIPELINE_FIREWALL_MSG_REQ_ADD] =
		pipeline_firewall_msg_req_add_handler,
	[PIPELINE_FIREWALL_MSG_REQ_DEL] =
		pipeline_firewall_msg_req_del_handler,
	[PIPELINE_FIREWALL_MSG_REQ_ADD_DEFAULT] =
		pipeline_firewall_msg_req_add_default_handler,
	[PIPELINE_FIREWALL_MSG_REQ_DEL_DEFAULT] =
		pipeline_firewall_msg_req_del_default_handler,
};

/*
 * Firewall table
 */
struct firewall_table_entry {
	struct rte_pipeline_table_entry head;
};

static struct rte_acl_field_def field_format_ipv4[] = {
	/* Protocol */
	[0] = {
		.type = RTE_ACL_FIELD_TYPE_BITMASK,
		.size = sizeof(uint8_t),
		.field_index = 0,
		.input_index = 0,
		.offset = sizeof(struct ether_hdr) +
			offsetof(struct ipv4_hdr, next_proto_id),
	},

	/* Source IP address (IPv4) */
	[1] = {
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = 1,
		.input_index = 1,
		.offset = sizeof(struct ether_hdr) +
			offsetof(struct ipv4_hdr, src_addr),
	},

	/* Destination IP address (IPv4) */
	[2] = {
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = 2,
		.input_index = 2,
		.offset = sizeof(struct ether_hdr) +
			offsetof(struct ipv4_hdr, dst_addr),
	},

	/* Source Port */
	[3] = {
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = 3,
		.input_index = 3,
		.offset = sizeof(struct ether_hdr) +
			sizeof(struct ipv4_hdr) +
			offsetof(struct tcp_hdr, src_port),
	},

	/* Destination Port */
	[4] = {
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = 4,
		.input_index = 4,
		.offset = sizeof(struct ether_hdr) +
			sizeof(struct ipv4_hdr) +
			offsetof(struct tcp_hdr, dst_port),
	},
};

#define SIZEOF_VLAN_HDR                          4

static struct rte_acl_field_def field_format_vlan_ipv4[] = {
	/* Protocol */
	[0] = {
		.type = RTE_ACL_FIELD_TYPE_BITMASK,
		.size = sizeof(uint8_t),
		.field_index = 0,
		.input_index = 0,
		.offset = sizeof(struct ether_hdr) +
			SIZEOF_VLAN_HDR +
			offsetof(struct ipv4_hdr, next_proto_id),
	},

	/* Source IP address (IPv4) */
	[1] = {
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = 1,
		.input_index = 1,
		.offset = sizeof(struct ether_hdr) +
			SIZEOF_VLAN_HDR +
			offsetof(struct ipv4_hdr, src_addr),
	},

	/* Destination IP address (IPv4) */
	[2] = {
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = 2,
		.input_index = 2,
		.offset = sizeof(struct ether_hdr) +
			SIZEOF_VLAN_HDR +
			offsetof(struct ipv4_hdr, dst_addr),
	},

	/* Source Port */
	[3] = {
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = 3,
		.input_index = 3,
		.offset = sizeof(struct ether_hdr) +
			SIZEOF_VLAN_HDR +
			sizeof(struct ipv4_hdr) +
			offsetof(struct tcp_hdr, src_port),
	},

	/* Destination Port */
	[4] = {
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = 4,
		.input_index = 4,
		.offset = sizeof(struct ether_hdr) +
			SIZEOF_VLAN_HDR +
			sizeof(struct ipv4_hdr) +
			offsetof(struct tcp_hdr, dst_port),
	},
};

#define SIZEOF_QINQ_HEADER                       8

static struct rte_acl_field_def field_format_qinq_ipv4[] = {
	/* Protocol */
	[0] = {
		.type = RTE_ACL_FIELD_TYPE_BITMASK,
		.size = sizeof(uint8_t),
		.field_index = 0,
		.input_index = 0,
		.offset = sizeof(struct ether_hdr) +
			SIZEOF_QINQ_HEADER +
			offsetof(struct ipv4_hdr, next_proto_id),
	},

	/* Source IP address (IPv4) */
	[1] = {
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = 1,
		.input_index = 1,
		.offset = sizeof(struct ether_hdr) +
			SIZEOF_QINQ_HEADER +
			offsetof(struct ipv4_hdr, src_addr),
	},

	/* Destination IP address (IPv4) */
	[2] = {
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = 2,
		.input_index = 2,
		.offset = sizeof(struct ether_hdr) +
			SIZEOF_QINQ_HEADER +
			offsetof(struct ipv4_hdr, dst_addr),
	},

	/* Source Port */
	[3] = {
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = 3,
		.input_index = 3,
		.offset = sizeof(struct ether_hdr) +
			SIZEOF_QINQ_HEADER +
			sizeof(struct ipv4_hdr) +
			offsetof(struct tcp_hdr, src_port),
	},

	/* Destination Port */
	[4] = {
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = 4,
		.input_index = 4,
		.offset = sizeof(struct ether_hdr) +
			SIZEOF_QINQ_HEADER +
			sizeof(struct ipv4_hdr) +
			offsetof(struct tcp_hdr, dst_port),
	},
};

static int
pipeline_firewall_parse_args(struct pipeline_firewall *p,
	struct pipeline_params *params)
{
	uint32_t n_rules_present = 0;
	uint32_t pkt_type_present = 0;
	uint32_t i;

	/* defaults */
	p->n_rules = 4 * 1024;
	p->n_rule_fields = RTE_DIM(field_format_ipv4);
	p->field_format = field_format_ipv4;
	p->field_format_size = sizeof(field_format_ipv4);

	for (i = 0; i < params->n_args; i++) {
		char *arg_name = params->args_name[i];
		char *arg_value = params->args_value[i];

		if (strcmp(arg_name, "n_rules") == 0) {
			if (n_rules_present)
				return -1;
			n_rules_present = 1;

			p->n_rules = atoi(arg_value);
			continue;
		}

		if (strcmp(arg_name, "pkt_type") == 0) {
			if (pkt_type_present)
				return -1;
			pkt_type_present = 1;

			/* ipv4 */
			if (strcmp(arg_value, "ipv4") == 0) {
				p->n_rule_fields = RTE_DIM(field_format_ipv4);
				p->field_format = field_format_ipv4;
				p->field_format_size =
					sizeof(field_format_ipv4);
				continue;
			}

			/* vlan_ipv4 */
			if (strcmp(arg_value, "vlan_ipv4") == 0) {
				p->n_rule_fields =
					RTE_DIM(field_format_vlan_ipv4);
				p->field_format = field_format_vlan_ipv4;
				p->field_format_size =
					sizeof(field_format_vlan_ipv4);
				continue;
			}

			/* qinq_ipv4 */
			if (strcmp(arg_value, "qinq_ipv4") == 0) {
				p->n_rule_fields =
					RTE_DIM(field_format_qinq_ipv4);
				p->field_format = field_format_qinq_ipv4;
				p->field_format_size =
					sizeof(field_format_qinq_ipv4);
				continue;
			}

			/* other */
			return -1;
		}

		/* other */
		return -1;
	}

	return 0;
}

static void *
pipeline_firewall_init(struct pipeline_params *params,
	__rte_unused void *arg)
{
	struct pipeline *p;
	struct pipeline_firewall *p_fw;
	uint32_t size, i;

	/* Check input arguments */
	if ((params == NULL) ||
		(params->n_ports_in == 0) ||
		(params->n_ports_out == 0))
		return NULL;

	/* Memory allocation */
	size = RTE_CACHE_LINE_ROUNDUP(sizeof(struct pipeline_firewall));
	p = rte_zmalloc(NULL, size, RTE_CACHE_LINE_SIZE);
	p_fw = (struct pipeline_firewall *) p;
	if (p == NULL)
		return NULL;

	strcpy(p->name, params->name);
	p->log_level = params->log_level;

	PLOG(p, HIGH, "Firewall");

	/* Parse arguments */
	if (pipeline_firewall_parse_args(p_fw, params))
		return NULL;

	/* Pipeline */
	{
		struct rte_pipeline_params pipeline_params = {
			.name = params->name,
			.socket_id = params->socket_id,
			.offset_port_id = 0,
		};

		p->p = rte_pipeline_create(&pipeline_params);
		if (p->p == NULL) {
			rte_free(p);
			return NULL;
		}
	}

	/* Input ports */
	p->n_ports_in = params->n_ports_in;
	for (i = 0; i < p->n_ports_in; i++) {
		struct rte_pipeline_port_in_params port_params = {
			.ops = pipeline_port_in_params_get_ops(
				&params->port_in[i]),
			.arg_create = pipeline_port_in_params_convert(
				&params->port_in[i]),
			.f_action = NULL,
			.arg_ah = NULL,
			.burst_size = params->port_in[i].burst_size,
		};

		int status = rte_pipeline_port_in_create(p->p,
			&port_params,
			&p->port_in_id[i]);

		if (status) {
			rte_pipeline_free(p->p);
			rte_free(p);
			return NULL;
		}
	}

	/* Output ports */
	p->n_ports_out = params->n_ports_out;
	for (i = 0; i < p->n_ports_out; i++) {
		struct rte_pipeline_port_out_params port_params = {
			.ops = pipeline_port_out_params_get_ops(
				&params->port_out[i]),
			.arg_create = pipeline_port_out_params_convert(
				&params->port_out[i]),
			.f_action = NULL,
			.f_action_bulk = NULL,
			.arg_ah = NULL,
		};

		int status = rte_pipeline_port_out_create(p->p,
			&port_params,
			&p->port_out_id[i]);

		if (status) {
			rte_pipeline_free(p->p);
			rte_free(p);
			return NULL;
		}
	}

	/* Tables */
	p->n_tables = 1;
	{
		struct rte_table_acl_params table_acl_params = {
			.name = params->name,
			.n_rules = p_fw->n_rules,
			.n_rule_fields = p_fw->n_rule_fields,
		};

		struct rte_pipeline_table_params table_params = {
				.ops = &rte_table_acl_ops,
				.arg_create = &table_acl_params,
				.f_action_hit = NULL,
				.f_action_miss = NULL,
				.arg_ah = NULL,
				.action_data_size =
					sizeof(struct firewall_table_entry) -
					sizeof(struct rte_pipeline_table_entry),
			};

		int status;

		memcpy(table_acl_params.field_format,
			p_fw->field_format,
			p_fw->field_format_size);

		status = rte_pipeline_table_create(p->p,
			&table_params,
			&p->table_id[0]);

		if (status) {
			rte_pipeline_free(p->p);
			rte_free(p);
			return NULL;
		}
	}

	/* Connecting input ports to tables */
	for (i = 0; i < p->n_ports_in; i++) {
		int status = rte_pipeline_port_in_connect_to_table(p->p,
			p->port_in_id[i],
			p->table_id[0]);

		if (status) {
			rte_pipeline_free(p->p);
			rte_free(p);
			return NULL;
		}
	}

	/* Enable input ports */
	for (i = 0; i < p->n_ports_in; i++) {
		int status = rte_pipeline_port_in_enable(p->p,
			p->port_in_id[i]);

		if (status) {
			rte_pipeline_free(p->p);
			rte_free(p);
			return NULL;
		}
	}

	/* Check pipeline consistency */
	if (rte_pipeline_check(p->p) < 0) {
		rte_pipeline_free(p->p);
		rte_free(p);
		return NULL;
	}

	/* Message queues */
	p->n_msgq = params->n_msgq;
	for (i = 0; i < p->n_msgq; i++)
		p->msgq_in[i] = params->msgq_in[i];
	for (i = 0; i < p->n_msgq; i++)
		p->msgq_out[i] = params->msgq_out[i];

	/* Message handlers */
	memcpy(p->handlers, handlers, sizeof(p->handlers));
	memcpy(p_fw->custom_handlers,
		custom_handlers,
		sizeof(p_fw->custom_handlers));

	return p;
}

static int
pipeline_firewall_free(void *pipeline)
{
	struct pipeline *p = (struct pipeline *) pipeline;

	/* Check input arguments */
	if (p == NULL)
		return -1;

	/* Free resources */
	rte_pipeline_free(p->p);
	rte_free(p);
	return 0;
}

static int
pipeline_firewall_track(void *pipeline,
	__rte_unused uint32_t port_in,
	uint32_t *port_out)
{
	struct pipeline *p = (struct pipeline *) pipeline;

	/* Check input arguments */
	if ((p == NULL) ||
		(port_in >= p->n_ports_in) ||
		(port_out == NULL))
		return -1;

	if (p->n_ports_in == 1) {
		*port_out = 0;
		return 0;
	}

	return -1;
}

static int
pipeline_firewall_timer(void *pipeline)
{
	struct pipeline *p = (struct pipeline *) pipeline;

	pipeline_msg_req_handle(p);
	rte_pipeline_flush(p->p);

	return 0;
}

void *
pipeline_firewall_msg_req_custom_handler(struct pipeline *p,
	void *msg)
{
	struct pipeline_firewall *p_fw = (struct pipeline_firewall *) p;
	struct pipeline_custom_msg_req *req = msg;
	pipeline_msg_req_handler f_handle;

	f_handle = (req->subtype < PIPELINE_FIREWALL_MSG_REQS) ?
		p_fw->custom_handlers[req->subtype] :
		pipeline_msg_req_invalid_handler;

	if (f_handle == NULL)
		f_handle = pipeline_msg_req_invalid_handler;

	return f_handle(p, req);
}

void *
pipeline_firewall_msg_req_add_handler(struct pipeline *p, void *msg)
{
	struct pipeline_firewall_add_msg_req *req = msg;
	struct pipeline_firewall_add_msg_rsp *rsp = msg;

	struct rte_table_acl_rule_add_params params;
	struct firewall_table_entry entry = {
		.head = {
			.action = RTE_PIPELINE_ACTION_PORT,
			{.port_id = p->port_out_id[req->port_id]},
		},
	};

	memset(&params, 0, sizeof(params));

	switch (req->key.type) {
	case PIPELINE_FIREWALL_IPV4_5TUPLE:
		params.priority = req->priority;
		params.field_value[0].value.u8 =
			req->key.key.ipv4_5tuple.proto;
		params.field_value[0].mask_range.u8 =
			req->key.key.ipv4_5tuple.proto_mask;
		params.field_value[1].value.u32 =
			req->key.key.ipv4_5tuple.src_ip;
		params.field_value[1].mask_range.u32 =
			req->key.key.ipv4_5tuple.src_ip_mask;
		params.field_value[2].value.u32 =
			req->key.key.ipv4_5tuple.dst_ip;
		params.field_value[2].mask_range.u32 =
			req->key.key.ipv4_5tuple.dst_ip_mask;
		params.field_value[3].value.u16 =
			req->key.key.ipv4_5tuple.src_port_from;
		params.field_value[3].mask_range.u16 =
			req->key.key.ipv4_5tuple.src_port_to;
		params.field_value[4].value.u16 =
			req->key.key.ipv4_5tuple.dst_port_from;
		params.field_value[4].mask_range.u16 =
			req->key.key.ipv4_5tuple.dst_port_to;
		break;

	default:
		rsp->status = -1; /* Error */
		return rsp;
	}

	rsp->status = rte_pipeline_table_entry_add(p->p,
		p->table_id[0],
		&params,
		(struct rte_pipeline_table_entry *) &entry,
		&rsp->key_found,
		(struct rte_pipeline_table_entry **) &rsp->entry_ptr);

	return rsp;
}

void *
pipeline_firewall_msg_req_del_handler(struct pipeline *p, void *msg)
{
	struct pipeline_firewall_del_msg_req *req = msg;
	struct pipeline_firewall_del_msg_rsp *rsp = msg;

	struct rte_table_acl_rule_delete_params params;

	memset(&params, 0, sizeof(params));

	switch (req->key.type) {
	case PIPELINE_FIREWALL_IPV4_5TUPLE:
		params.field_value[0].value.u8 =
			req->key.key.ipv4_5tuple.proto;
		params.field_value[0].mask_range.u8 =
			req->key.key.ipv4_5tuple.proto_mask;
		params.field_value[1].value.u32 =
			req->key.key.ipv4_5tuple.src_ip;
		params.field_value[1].mask_range.u32 =
			req->key.key.ipv4_5tuple.src_ip_mask;
		params.field_value[2].value.u32 =
			req->key.key.ipv4_5tuple.dst_ip;
		params.field_value[2].mask_range.u32 =
			req->key.key.ipv4_5tuple.dst_ip_mask;
		params.field_value[3].value.u16 =
			req->key.key.ipv4_5tuple.src_port_from;
		params.field_value[3].mask_range.u16 =
			req->key.key.ipv4_5tuple.src_port_to;
		params.field_value[4].value.u16 =
			req->key.key.ipv4_5tuple.dst_port_from;
		params.field_value[4].mask_range.u16 =
			req->key.key.ipv4_5tuple.dst_port_to;
		break;

	default:
		rsp->status = -1; /* Error */
		return rsp;
	}

	rsp->status = rte_pipeline_table_entry_delete(p->p,
		p->table_id[0],
		&params,
		&rsp->key_found,
		NULL);

	return rsp;
}

void *
pipeline_firewall_msg_req_add_default_handler(struct pipeline *p, void *msg)
{
	struct pipeline_firewall_add_default_msg_req *req = msg;
	struct pipeline_firewall_add_default_msg_rsp *rsp = msg;

	struct firewall_table_entry default_entry = {
		.head = {
			.action = RTE_PIPELINE_ACTION_PORT,
			{.port_id = p->port_out_id[req->port_id]},
		},
	};

	rsp->status = rte_pipeline_table_default_entry_add(p->p,
		p->table_id[0],
		(struct rte_pipeline_table_entry *) &default_entry,
		(struct rte_pipeline_table_entry **) &rsp->entry_ptr);

	return rsp;
}

void *
pipeline_firewall_msg_req_del_default_handler(struct pipeline *p, void *msg)
{
	struct pipeline_firewall_del_default_msg_rsp *rsp = msg;

	rsp->status = rte_pipeline_table_default_entry_delete(p->p,
		p->table_id[0],
		NULL);

	return rsp;
}

struct pipeline_be_ops pipeline_firewall_be_ops = {
	.f_init = pipeline_firewall_init,
	.f_free = pipeline_firewall_free,
	.f_run = NULL,
	.f_timer = pipeline_firewall_timer,
	.f_track = pipeline_firewall_track,
};
