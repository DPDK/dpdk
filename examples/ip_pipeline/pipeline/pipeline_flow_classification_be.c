/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
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
#include <rte_table_hash.h>
#include <rte_byteorder.h>
#include <pipeline.h>

#include "pipeline_flow_classification_be.h"
#include "pipeline_actions_common.h"
#include "parser.h"
#include "hash_func.h"

struct pipeline_flow_classification {
	struct pipeline p;
	pipeline_msg_req_handler custom_handlers[PIPELINE_FC_MSG_REQS];

	uint32_t n_flows;
	uint32_t key_size;
	uint32_t flow_id;

	uint32_t key_offset;
	uint32_t hash_offset;
	uint8_t key_mask[PIPELINE_FC_FLOW_KEY_MAX_SIZE];
	uint32_t key_mask_present;
	uint32_t flow_id_offset;

} __rte_cache_aligned;

static void *
pipeline_fc_msg_req_custom_handler(struct pipeline *p, void *msg);

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
		pipeline_fc_msg_req_custom_handler,
};

static void *
pipeline_fc_msg_req_add_handler(struct pipeline *p, void *msg);

static void *
pipeline_fc_msg_req_add_bulk_handler(struct pipeline *p, void *msg);

static void *
pipeline_fc_msg_req_del_handler(struct pipeline *p, void *msg);

static void *
pipeline_fc_msg_req_add_default_handler(struct pipeline *p, void *msg);

static void *
pipeline_fc_msg_req_del_default_handler(struct pipeline *p, void *msg);

static pipeline_msg_req_handler custom_handlers[] = {
	[PIPELINE_FC_MSG_REQ_FLOW_ADD] =
		pipeline_fc_msg_req_add_handler,
	[PIPELINE_FC_MSG_REQ_FLOW_ADD_BULK] =
		pipeline_fc_msg_req_add_bulk_handler,
	[PIPELINE_FC_MSG_REQ_FLOW_DEL] =
		pipeline_fc_msg_req_del_handler,
	[PIPELINE_FC_MSG_REQ_FLOW_ADD_DEFAULT] =
		pipeline_fc_msg_req_add_default_handler,
	[PIPELINE_FC_MSG_REQ_FLOW_DEL_DEFAULT] =
		pipeline_fc_msg_req_del_default_handler,
};

/*
 * Flow table
 */
struct flow_table_entry {
	struct rte_pipeline_table_entry head;

	uint32_t flow_id;
	uint32_t pad;
};

rte_table_hash_op_hash hash_func[] = {
	hash_default_key8,
	hash_default_key16,
	hash_default_key24,
	hash_default_key32,
	hash_default_key40,
	hash_default_key48,
	hash_default_key56,
	hash_default_key64
};

/*
 * Flow table AH - Write flow_id to packet meta-data
 */
static inline void
pkt_work_flow_id(
	struct rte_mbuf *pkt,
	struct rte_pipeline_table_entry *table_entry,
	void *arg)
{
	struct pipeline_flow_classification *p_fc = arg;
	uint32_t *flow_id_ptr =
		RTE_MBUF_METADATA_UINT32_PTR(pkt, p_fc->flow_id_offset);
	struct flow_table_entry *entry =
		(struct flow_table_entry *) table_entry;

	/* Read */
	uint32_t flow_id = entry->flow_id;

	/* Compute */

	/* Write */
	*flow_id_ptr = flow_id;
}

static inline void
pkt4_work_flow_id(
	struct rte_mbuf **pkts,
	struct rte_pipeline_table_entry **table_entries,
	void *arg)
{
	struct pipeline_flow_classification *p_fc = arg;

	uint32_t *flow_id_ptr0 =
		RTE_MBUF_METADATA_UINT32_PTR(pkts[0], p_fc->flow_id_offset);
	uint32_t *flow_id_ptr1 =
		RTE_MBUF_METADATA_UINT32_PTR(pkts[1], p_fc->flow_id_offset);
	uint32_t *flow_id_ptr2 =
		RTE_MBUF_METADATA_UINT32_PTR(pkts[2], p_fc->flow_id_offset);
	uint32_t *flow_id_ptr3 =
		RTE_MBUF_METADATA_UINT32_PTR(pkts[3], p_fc->flow_id_offset);

	struct flow_table_entry *entry0 =
		(struct flow_table_entry *) table_entries[0];
	struct flow_table_entry *entry1 =
		(struct flow_table_entry *) table_entries[1];
	struct flow_table_entry *entry2 =
		(struct flow_table_entry *) table_entries[2];
	struct flow_table_entry *entry3 =
		(struct flow_table_entry *) table_entries[3];

	/* Read */
	uint32_t flow_id0 = entry0->flow_id;
	uint32_t flow_id1 = entry1->flow_id;
	uint32_t flow_id2 = entry2->flow_id;
	uint32_t flow_id3 = entry3->flow_id;

	/* Compute */

	/* Write */
	*flow_id_ptr0 = flow_id0;
	*flow_id_ptr1 = flow_id1;
	*flow_id_ptr2 = flow_id2;
	*flow_id_ptr3 = flow_id3;
}

PIPELINE_TABLE_AH_HIT(fc_table_ah_hit,
		pkt_work_flow_id, pkt4_work_flow_id);

static rte_pipeline_table_action_handler_hit
get_fc_table_ah_hit(struct pipeline_flow_classification *p)
{
	if (p->flow_id)
		return fc_table_ah_hit;

	return NULL;
}

/*
 * Argument parsing
 */
static int
pipeline_fc_parse_args(struct pipeline_flow_classification *p,
	struct pipeline_params *params)
{
	uint32_t n_flows_present = 0;
	uint32_t key_offset_present = 0;
	uint32_t key_size_present = 0;
	uint32_t hash_offset_present = 0;
	uint32_t key_mask_present = 0;
	uint32_t flow_id_offset_present = 0;

	uint32_t i;
	char key_mask_str[PIPELINE_FC_FLOW_KEY_MAX_SIZE * 2 + 1];

	p->hash_offset = 0;

	/* default values */
	p->flow_id = 0;

	for (i = 0; i < params->n_args; i++) {
		char *arg_name = params->args_name[i];
		char *arg_value = params->args_value[i];

		/* n_flows */
		if (strcmp(arg_name, "n_flows") == 0) {
			int status;

			PIPELINE_PARSE_ERR_DUPLICATE(
				n_flows_present == 0, params->name,
				arg_name);
			n_flows_present = 1;

			status = parser_read_uint32(&p->n_flows,
				arg_value);
			PIPELINE_PARSE_ERR_INV_VAL(((status != -EINVAL) &&
				(p->n_flows != 0)), params->name,
				arg_name, arg_value);
			PIPELINE_PARSE_ERR_OUT_RNG((status != -ERANGE),
				params->name, arg_name, arg_value);

			continue;
		}

		/* key_offset */
		if (strcmp(arg_name, "key_offset") == 0) {
			int status;

			PIPELINE_PARSE_ERR_DUPLICATE(
				key_offset_present == 0, params->name,
				arg_name);
			key_offset_present = 1;

			status = parser_read_uint32(&p->key_offset,
				arg_value);
			PIPELINE_PARSE_ERR_INV_VAL((status != -EINVAL),
				params->name, arg_name, arg_value);
			PIPELINE_PARSE_ERR_OUT_RNG((status != -ERANGE),
				params->name, arg_name, arg_value);

			continue;
		}

		/* key_size */
		if (strcmp(arg_name, "key_size") == 0) {
			int status;

			PIPELINE_PARSE_ERR_DUPLICATE(
				key_size_present == 0, params->name,
				arg_name);
			key_size_present = 1;

			status = parser_read_uint32(&p->key_size,
				arg_value);
			PIPELINE_PARSE_ERR_INV_VAL(((status != -EINVAL) &&
				(p->key_size != 0) &&
				(p->key_size % 8 == 0)),
				params->name, arg_name, arg_value);
			PIPELINE_PARSE_ERR_OUT_RNG(((status != -ERANGE) &&
				(p->key_size <=
				PIPELINE_FC_FLOW_KEY_MAX_SIZE)),
				params->name, arg_name, arg_value);

			continue;
		}

		/* key_mask */
		if (strcmp(arg_name, "key_mask") == 0) {
			int mask_str_len = strlen(arg_value);

			PIPELINE_PARSE_ERR_DUPLICATE(
				key_mask_present == 0,
				params->name, arg_name);
			key_mask_present = 1;

			PIPELINE_ARG_CHECK((mask_str_len <=
				(PIPELINE_FC_FLOW_KEY_MAX_SIZE * 2)),
				"Parse error in section \"%s\": entry "
				"\"%s\" is too long", params->name,
				arg_name);

			snprintf(key_mask_str, mask_str_len + 1, "%s",
				arg_value);

			continue;
		}

		/* hash_offset */
		if (strcmp(arg_name, "hash_offset") == 0) {
			int status;

			PIPELINE_PARSE_ERR_DUPLICATE(
				hash_offset_present == 0, params->name,
				arg_name);
			hash_offset_present = 1;

			status = parser_read_uint32(&p->hash_offset,
				arg_value);
			PIPELINE_PARSE_ERR_INV_VAL((status != -EINVAL),
				params->name, arg_name, arg_value);
			PIPELINE_PARSE_ERR_OUT_RNG((status != -ERANGE),
				params->name, arg_name, arg_value);

			continue;
		}

		/* flow_id_offset */
		if (strcmp(arg_name, "flowid_offset") == 0) {
			int status;

			PIPELINE_PARSE_ERR_DUPLICATE(
				flow_id_offset_present == 0, params->name,
				arg_name);
			flow_id_offset_present = 1;

			status = parser_read_uint32(&p->flow_id_offset,
				arg_value);
			PIPELINE_PARSE_ERR_INV_VAL((status != -EINVAL),
				params->name, arg_name, arg_value);
			PIPELINE_PARSE_ERR_OUT_RNG((status != -ERANGE),
				params->name, arg_name, arg_value);

			p->flow_id = 1;

			continue;
		}

		/* Unknown argument */
		PIPELINE_PARSE_ERR_INV_ENT(0, params->name, arg_name);
	}

	/* Check that mandatory arguments are present */
	PIPELINE_PARSE_ERR_MANDATORY((n_flows_present), params->name,
		"n_flows");
	PIPELINE_PARSE_ERR_MANDATORY((key_offset_present), params->name,
		"key_offset");
	PIPELINE_PARSE_ERR_MANDATORY((key_size_present), params->name,
		"key_size");

	if (key_mask_present) {
		uint32_t key_size = p->key_size;
		int status;

		PIPELINE_ARG_CHECK(((key_size == 8) || (key_size == 16)),
			"Parse error in section \"%s\": entry key_mask "
			"only allowed for key_size of 8 or 16 bytes",
			params->name);

		PIPELINE_ARG_CHECK((strlen(key_mask_str) ==
			(key_size * 2)), "Parse error in section "
			"\"%s\": key_mask should have exactly %u hex "
			"digits", params->name, (key_size * 2));

		PIPELINE_ARG_CHECK((hash_offset_present == 0), "Parse "
			"error in section \"%s\": entry hash_offset only "
			"allowed when key_mask is not present",
			params->name);

		status = parse_hex_string(key_mask_str, p->key_mask,
			&p->key_size);

		PIPELINE_PARSE_ERR_INV_VAL(((status == 0) &&
			(key_size == p->key_size)), params->name,
			"key_mask", key_mask_str);
	}

	p->key_mask_present = key_mask_present;

	return 0;
}

static void *pipeline_fc_init(struct pipeline_params *params,
	__rte_unused void *arg)
{
	struct pipeline *p;
	struct pipeline_flow_classification *p_fc;
	uint32_t size, i;

	/* Check input arguments */
	if (params == NULL)
		return NULL;

	/* Memory allocation */
	size = RTE_CACHE_LINE_ROUNDUP(
		sizeof(struct pipeline_flow_classification));
	p = rte_zmalloc(NULL, size, RTE_CACHE_LINE_SIZE);
	if (p == NULL)
		return NULL;
	p_fc = (struct pipeline_flow_classification *) p;

	strcpy(p->name, params->name);
	p->log_level = params->log_level;

	PLOG(p, HIGH, "Flow classification");

	/* Parse arguments */
	if (pipeline_fc_parse_args(p_fc, params))
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
		struct rte_table_hash_key8_ext_params
			table_hash_key8_params = {
			.n_entries = p_fc->n_flows,
			.n_entries_ext = p_fc->n_flows,
			.signature_offset = p_fc->hash_offset,
			.key_offset = p_fc->key_offset,
			.f_hash = hash_func[(p_fc->key_size / 8) - 1],
			.key_mask = (p_fc->key_mask_present) ?
				p_fc->key_mask : NULL,
			.seed = 0,
		};

		struct rte_table_hash_key16_ext_params
			table_hash_key16_params = {
			.n_entries = p_fc->n_flows,
			.n_entries_ext = p_fc->n_flows,
			.signature_offset = p_fc->hash_offset,
			.key_offset = p_fc->key_offset,
			.f_hash = hash_func[(p_fc->key_size / 8) - 1],
			.key_mask = (p_fc->key_mask_present) ?
				p_fc->key_mask : NULL,
			.seed = 0,
		};

		struct rte_table_hash_ext_params
			table_hash_params = {
			.key_size = p_fc->key_size,
			.n_keys = p_fc->n_flows,
			.n_buckets = p_fc->n_flows / 4,
			.n_buckets_ext = p_fc->n_flows / 4,
			.f_hash = hash_func[(p_fc->key_size / 8) - 1],
			.seed = 0,
			.signature_offset = p_fc->hash_offset,
			.key_offset = p_fc->key_offset,
		};

		struct rte_pipeline_table_params table_params = {
			.ops = NULL, /* set below */
			.arg_create = NULL, /* set below */
			.f_action_hit = get_fc_table_ah_hit(p_fc),
			.f_action_miss = NULL,
			.arg_ah = p_fc,
			.action_data_size = sizeof(struct flow_table_entry) -
				sizeof(struct rte_pipeline_table_entry),
		};

		int status;

		switch (p_fc->key_size) {
		case 8:
			if (p_fc->hash_offset != 0) {
				table_params.ops =
					&rte_table_hash_key8_ext_ops;
			} else {
				table_params.ops =
					&rte_table_hash_key8_ext_dosig_ops;
			}
			table_params.arg_create = &table_hash_key8_params;
			break;

		case 16:
			if (p_fc->hash_offset != 0) {
				table_params.ops =
					&rte_table_hash_key16_ext_ops;
			} else {
				table_params.ops =
					&rte_table_hash_key16_ext_dosig_ops;
			}
			table_params.arg_create = &table_hash_key16_params;
			break;

		default:
			table_params.ops = &rte_table_hash_ext_ops;
			table_params.arg_create = &table_hash_params;
		}

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
	memcpy(p_fc->custom_handlers,
		custom_handlers,
		sizeof(p_fc->custom_handlers));

	return p;
}

static int
pipeline_fc_free(void *pipeline)
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
pipeline_fc_timer(void *pipeline)
{
	struct pipeline *p = (struct pipeline *) pipeline;

	pipeline_msg_req_handle(p);
	rte_pipeline_flush(p->p);

	return 0;
}

static void *
pipeline_fc_msg_req_custom_handler(struct pipeline *p, void *msg)
{
	struct pipeline_flow_classification *p_fc =
			(struct pipeline_flow_classification *) p;
	struct pipeline_custom_msg_req *req = msg;
	pipeline_msg_req_handler f_handle;

	f_handle = (req->subtype < PIPELINE_FC_MSG_REQS) ?
		p_fc->custom_handlers[req->subtype] :
		pipeline_msg_req_invalid_handler;

	if (f_handle == NULL)
		f_handle = pipeline_msg_req_invalid_handler;

	return f_handle(p, req);
}

static void *
pipeline_fc_msg_req_add_handler(struct pipeline *p, void *msg)
{
	struct pipeline_fc_add_msg_req *req = msg;
	struct pipeline_fc_add_msg_rsp *rsp = msg;

	struct flow_table_entry entry = {
		.head = {
			.action = RTE_PIPELINE_ACTION_PORT,
			{.port_id = p->port_out_id[req->port_id]},
		},
		.flow_id = req->flow_id,
	};

	rsp->status = rte_pipeline_table_entry_add(p->p,
		p->table_id[0],
		&req->key,
		(struct rte_pipeline_table_entry *) &entry,
		&rsp->key_found,
		(struct rte_pipeline_table_entry **) &rsp->entry_ptr);

	return rsp;
}

static void *
pipeline_fc_msg_req_add_bulk_handler(struct pipeline *p, void *msg)
{
	struct pipeline_fc_add_bulk_msg_req *req = msg;
	struct pipeline_fc_add_bulk_msg_rsp *rsp = msg;
	uint32_t i;

	for (i = 0; i < req->n_keys; i++) {
		struct pipeline_fc_add_bulk_flow_req *flow_req = &req->req[i];
		struct pipeline_fc_add_bulk_flow_rsp *flow_rsp = &req->rsp[i];

		struct flow_table_entry entry = {
			.head = {
				.action = RTE_PIPELINE_ACTION_PORT,
				{.port_id = p->port_out_id[flow_req->port_id]},
			},
			.flow_id = flow_req->flow_id,
		};

		int status = rte_pipeline_table_entry_add(p->p,
			p->table_id[0],
			&flow_req->key,
			(struct rte_pipeline_table_entry *) &entry,
			&flow_rsp->key_found,
			(struct rte_pipeline_table_entry **)
				&flow_rsp->entry_ptr);

		if (status)
			break;
	}

	rsp->n_keys = i;

	return rsp;
}

static void *
pipeline_fc_msg_req_del_handler(struct pipeline *p, void *msg)
{
	struct pipeline_fc_del_msg_req *req = msg;
	struct pipeline_fc_del_msg_rsp *rsp = msg;

	rsp->status = rte_pipeline_table_entry_delete(p->p,
		p->table_id[0],
		&req->key,
		&rsp->key_found,
		NULL);

	return rsp;
}

static void *
pipeline_fc_msg_req_add_default_handler(struct pipeline *p, void *msg)
{
	struct pipeline_fc_add_default_msg_req *req = msg;
	struct pipeline_fc_add_default_msg_rsp *rsp = msg;

	struct flow_table_entry default_entry = {
		.head = {
			.action = RTE_PIPELINE_ACTION_PORT,
			{.port_id = p->port_out_id[req->port_id]},
		},

		.flow_id = 0,
	};

	rsp->status = rte_pipeline_table_default_entry_add(p->p,
		p->table_id[0],
		(struct rte_pipeline_table_entry *) &default_entry,
		(struct rte_pipeline_table_entry **) &rsp->entry_ptr);

	return rsp;
}

static void *
pipeline_fc_msg_req_del_default_handler(struct pipeline *p, void *msg)
{
	struct pipeline_fc_del_default_msg_rsp *rsp = msg;

	rsp->status = rte_pipeline_table_default_entry_delete(p->p,
		p->table_id[0],
		NULL);

	return rsp;
}

struct pipeline_be_ops pipeline_flow_classification_be_ops = {
	.f_init = pipeline_fc_init,
	.f_free = pipeline_fc_free,
	.f_run = NULL,
	.f_timer = pipeline_fc_timer,
};
