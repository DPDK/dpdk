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
#include <rte_cycles.h>
#include <rte_table_array.h>
#include <rte_byteorder.h>
#include <rte_ip.h>

#include "pipeline_actions_common.h"
#include "pipeline_flow_actions_be.h"
#include "parser.h"
#include "hash_func.h"

int
pipeline_fa_flow_params_set_default(struct pipeline_fa_flow_params *params)
{
	uint32_t i;

	if (params == NULL)
		return -1;

	for (i = 0; i < PIPELINE_FA_N_TC_MAX; i++) {
		struct rte_meter_trtcm_params *m = &params->m[i];

		m->cir = 1;
		m->cbs = 1;
		m->pir = 1;
		m->pbs = 2;
	}

	for (i = 0; i < PIPELINE_FA_N_TC_MAX; i++) {
		struct pipeline_fa_policer_params *p = &params->p[i];
		uint32_t j;

		for (j = 0; j < e_RTE_METER_COLORS; j++) {
			struct pipeline_fa_policer_action *a = &p->action[j];

			a->drop = 0;
			a->color = (enum rte_meter_color) j;
		}
	}

	params->port_id = 0;

	return 0;
}

struct dscp_entry {
	uint32_t traffic_class;
	enum rte_meter_color color;
};

struct pipeline_flow_actions {
	struct pipeline p;
	struct pipeline_fa_params params;
	pipeline_msg_req_handler custom_handlers[PIPELINE_FA_MSG_REQS];

	struct dscp_entry dscp[PIPELINE_FA_N_DSCP];
} __rte_cache_aligned;

static void *
pipeline_fa_msg_req_custom_handler(struct pipeline *p, void *msg);

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
		pipeline_fa_msg_req_custom_handler,
};

static void *
pipeline_fa_msg_req_flow_config_handler(struct pipeline *p, void *msg);

static void *
pipeline_fa_msg_req_flow_config_bulk_handler(struct pipeline *p, void *msg);

static void *
pipeline_fa_msg_req_dscp_config_handler(struct pipeline *p, void *msg);

static void *
pipeline_fa_msg_req_policer_stats_read_handler(struct pipeline *p, void *msg);

static pipeline_msg_req_handler custom_handlers[] = {
	[PIPELINE_FA_MSG_REQ_FLOW_CONFIG] =
		pipeline_fa_msg_req_flow_config_handler,
	[PIPELINE_FA_MSG_REQ_FLOW_CONFIG_BULK] =
		pipeline_fa_msg_req_flow_config_bulk_handler,
	[PIPELINE_FA_MSG_REQ_DSCP_CONFIG] =
		pipeline_fa_msg_req_dscp_config_handler,
	[PIPELINE_FA_MSG_REQ_POLICER_STATS_READ] =
		pipeline_fa_msg_req_policer_stats_read_handler,
};

/*
 * Flow table
 */
struct meter_policer {
	struct rte_meter_trtcm meter;
	struct pipeline_fa_policer_params policer;
	struct pipeline_fa_policer_stats stats;
};

struct flow_table_entry {
	struct rte_pipeline_table_entry head;
	struct meter_policer mp[PIPELINE_FA_N_TC_MAX];
};

static int
flow_table_entry_set_meter(struct flow_table_entry *entry,
	uint32_t meter_id,
	struct pipeline_fa_flow_params *params)
{
	struct rte_meter_trtcm *meter = &entry->mp[meter_id].meter;
	struct rte_meter_trtcm_params *meter_params = &params->m[meter_id];

	return rte_meter_trtcm_config(meter, meter_params);
}

static void
flow_table_entry_set_policer(struct flow_table_entry *entry,
	uint32_t policer_id,
	struct pipeline_fa_flow_params *params)
{
	struct pipeline_fa_policer_params *p0 = &entry->mp[policer_id].policer;
	struct pipeline_fa_policer_params *p1 = &params->p[policer_id];

	memcpy(p0, p1, sizeof(*p0));
}

static void
flow_table_entry_set_port_id(struct pipeline_flow_actions *p,
	struct flow_table_entry *entry,
	struct pipeline_fa_flow_params *params)
{
	entry->head.action = RTE_PIPELINE_ACTION_PORT;
	entry->head.port_id = p->p.port_out_id[params->port_id];
}

static int
flow_table_entry_set_default(struct pipeline_flow_actions *p,
	struct flow_table_entry *entry)
{
	struct pipeline_fa_flow_params params;
	uint32_t i;

	pipeline_fa_flow_params_set_default(&params);

	memset(entry, 0, sizeof(*entry));

	flow_table_entry_set_port_id(p, entry, &params);

	for (i = 0; i < PIPELINE_FA_N_TC_MAX; i++) {
		int status;

		status = flow_table_entry_set_meter(entry, i, &params);
		if (status)
			return status;
	}

	for (i = 0; i < PIPELINE_FA_N_TC_MAX; i++)
		flow_table_entry_set_policer(entry, i, &params);

	return 0;
}

static inline uint64_t
pkt_work(
	struct rte_mbuf *pkt,
	struct rte_pipeline_table_entry *table_entry,
	void *arg,
	uint64_t time)
{
	struct pipeline_flow_actions *p = arg;
	struct flow_table_entry *entry =
		(struct flow_table_entry *) table_entry;

	struct ipv4_hdr *pkt_ip = (struct ipv4_hdr *)
		RTE_MBUF_METADATA_UINT32_PTR(pkt, p->params.ip_hdr_offset);
	enum rte_meter_color *pkt_color = (enum rte_meter_color *)
		RTE_MBUF_METADATA_UINT32_PTR(pkt, p->params.color_offset);

	/* Read (IP header) */
	uint32_t total_length = rte_bswap16(pkt_ip->total_length);
	uint32_t dscp = pkt_ip->type_of_service >> 2;

	uint32_t tc = p->dscp[dscp].traffic_class;
	enum rte_meter_color color = p->dscp[dscp].color;

	struct rte_meter_trtcm *meter = &entry->mp[tc].meter;
	struct pipeline_fa_policer_params *policer = &entry->mp[tc].policer;
	struct pipeline_fa_policer_stats *stats = &entry->mp[tc].stats;

	/* Read (entry), compute */
	enum rte_meter_color color2 = rte_meter_trtcm_color_aware_check(meter,
		time,
		total_length,
		color);

	enum rte_meter_color color3 = policer->action[color2].color;
	uint64_t drop = policer->action[color2].drop;

	/* Read (entry), write (entry, color) */
	stats->n_pkts[color3] += drop ^ 1LLU;
	stats->n_pkts_drop += drop;
	*pkt_color = color3;

	return drop;
}

static inline uint64_t
pkt4_work(
	struct rte_mbuf **pkts,
	struct rte_pipeline_table_entry **table_entries,
	void *arg,
	uint64_t time)
{
	struct pipeline_flow_actions *p = arg;

	struct flow_table_entry *entry0 =
		(struct flow_table_entry *) table_entries[0];
	struct flow_table_entry *entry1 =
		(struct flow_table_entry *) table_entries[1];
	struct flow_table_entry *entry2 =
		(struct flow_table_entry *) table_entries[2];
	struct flow_table_entry *entry3 =
		(struct flow_table_entry *) table_entries[3];

	struct ipv4_hdr *pkt0_ip = (struct ipv4_hdr *)
		RTE_MBUF_METADATA_UINT32_PTR(pkts[0], p->params.ip_hdr_offset);
	struct ipv4_hdr *pkt1_ip = (struct ipv4_hdr *)
		RTE_MBUF_METADATA_UINT32_PTR(pkts[1], p->params.ip_hdr_offset);
	struct ipv4_hdr *pkt2_ip = (struct ipv4_hdr *)
		RTE_MBUF_METADATA_UINT32_PTR(pkts[2], p->params.ip_hdr_offset);
	struct ipv4_hdr *pkt3_ip = (struct ipv4_hdr *)
		RTE_MBUF_METADATA_UINT32_PTR(pkts[3], p->params.ip_hdr_offset);

	enum rte_meter_color *pkt0_color = (enum rte_meter_color *)
		RTE_MBUF_METADATA_UINT32_PTR(pkts[0], p->params.color_offset);
	enum rte_meter_color *pkt1_color = (enum rte_meter_color *)
		RTE_MBUF_METADATA_UINT32_PTR(pkts[1], p->params.color_offset);
	enum rte_meter_color *pkt2_color = (enum rte_meter_color *)
		RTE_MBUF_METADATA_UINT32_PTR(pkts[2], p->params.color_offset);
	enum rte_meter_color *pkt3_color = (enum rte_meter_color *)
		RTE_MBUF_METADATA_UINT32_PTR(pkts[3], p->params.color_offset);

	/* Read (IP header) */
	uint32_t total_length0 = rte_bswap16(pkt0_ip->total_length);
	uint32_t dscp0 = pkt0_ip->type_of_service >> 2;

	uint32_t total_length1 = rte_bswap16(pkt1_ip->total_length);
	uint32_t dscp1 = pkt1_ip->type_of_service >> 2;

	uint32_t total_length2 = rte_bswap16(pkt2_ip->total_length);
	uint32_t dscp2 = pkt2_ip->type_of_service >> 2;

	uint32_t total_length3 = rte_bswap16(pkt3_ip->total_length);
	uint32_t dscp3 = pkt3_ip->type_of_service >> 2;

	uint32_t tc0 = p->dscp[dscp0].traffic_class;
	enum rte_meter_color color0 = p->dscp[dscp0].color;

	uint32_t tc1 = p->dscp[dscp1].traffic_class;
	enum rte_meter_color color1 = p->dscp[dscp1].color;

	uint32_t tc2 = p->dscp[dscp2].traffic_class;
	enum rte_meter_color color2 = p->dscp[dscp2].color;

	uint32_t tc3 = p->dscp[dscp3].traffic_class;
	enum rte_meter_color color3 = p->dscp[dscp3].color;

	struct rte_meter_trtcm *meter0 = &entry0->mp[tc0].meter;
	struct pipeline_fa_policer_params *policer0 = &entry0->mp[tc0].policer;
	struct pipeline_fa_policer_stats *stats0 = &entry0->mp[tc0].stats;

	struct rte_meter_trtcm *meter1 = &entry1->mp[tc1].meter;
	struct pipeline_fa_policer_params *policer1 = &entry1->mp[tc1].policer;
	struct pipeline_fa_policer_stats *stats1 = &entry1->mp[tc1].stats;

	struct rte_meter_trtcm *meter2 = &entry2->mp[tc2].meter;
	struct pipeline_fa_policer_params *policer2 = &entry2->mp[tc2].policer;
	struct pipeline_fa_policer_stats *stats2 = &entry2->mp[tc2].stats;

	struct rte_meter_trtcm *meter3 = &entry3->mp[tc3].meter;
	struct pipeline_fa_policer_params *policer3 = &entry3->mp[tc3].policer;
	struct pipeline_fa_policer_stats *stats3 = &entry3->mp[tc3].stats;

	/* Read (entry), compute, write (entry) */
	enum rte_meter_color color2_0 = rte_meter_trtcm_color_aware_check(
		meter0,
		time,
		total_length0,
		color0);

	enum rte_meter_color color2_1 = rte_meter_trtcm_color_aware_check(
		meter1,
		time,
		total_length1,
		color1);

	enum rte_meter_color color2_2 = rte_meter_trtcm_color_aware_check(
		meter2,
		time,
		total_length2,
		color2);

	enum rte_meter_color color2_3 = rte_meter_trtcm_color_aware_check(
		meter3,
		time,
		total_length3,
		color3);

	enum rte_meter_color color3_0 = policer0->action[color2_0].color;
	enum rte_meter_color color3_1 = policer1->action[color2_1].color;
	enum rte_meter_color color3_2 = policer2->action[color2_2].color;
	enum rte_meter_color color3_3 = policer3->action[color2_3].color;

	uint64_t drop0 = policer0->action[color2_0].drop;
	uint64_t drop1 = policer1->action[color2_1].drop;
	uint64_t drop2 = policer2->action[color2_2].drop;
	uint64_t drop3 = policer3->action[color2_3].drop;

	/* Read (entry), write (entry, color) */
	stats0->n_pkts[color3_0] += drop0 ^ 1LLU;
	stats0->n_pkts_drop += drop0;

	stats1->n_pkts[color3_1] += drop1 ^ 1LLU;
	stats1->n_pkts_drop += drop1;

	stats2->n_pkts[color3_2] += drop2 ^ 1LLU;
	stats2->n_pkts_drop += drop2;

	stats3->n_pkts[color3_3] += drop3 ^ 1LLU;
	stats3->n_pkts_drop += drop3;

	*pkt0_color = color3_0;
	*pkt1_color = color3_1;
	*pkt2_color = color3_2;
	*pkt3_color = color3_3;

	return drop0 | (drop1 << 1) | (drop2 << 2) | (drop3 << 3);
}

PIPELINE_TABLE_AH_HIT_DROP_TIME(fa_table_ah_hit, pkt_work, pkt4_work);

static rte_pipeline_table_action_handler_hit
get_fa_table_ah_hit(__rte_unused struct pipeline_flow_actions *p)
{
	return fa_table_ah_hit;
}

/*
 * Argument parsing
 */
int
pipeline_fa_parse_args(struct pipeline_fa_params *p,
	struct pipeline_params *params)
{
	uint32_t n_flows_present = 0;
	uint32_t n_meters_per_flow_present = 0;
	uint32_t flow_id_offset_present = 0;
	uint32_t ip_hdr_offset_present = 0;
	uint32_t color_offset_present = 0;
	uint32_t i;

	/* Default values */
	p->n_meters_per_flow = 1;
	p->dscp_enabled = 0;

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

		/* n_meters_per_flow */
		if (strcmp(arg_name, "n_meters_per_flow") == 0) {
			int status;

			PIPELINE_PARSE_ERR_DUPLICATE(
				n_meters_per_flow_present == 0,
				params->name, arg_name);
			n_meters_per_flow_present = 1;

			status = parser_read_uint32(&p->n_meters_per_flow,
				arg_value);
			PIPELINE_PARSE_ERR_INV_VAL(((status != -EINVAL) &&
				(p->n_meters_per_flow != 0)),
				params->name, arg_name, arg_value);
			PIPELINE_PARSE_ERR_OUT_RNG(((status != -ERANGE) &&
				(p->n_meters_per_flow <=
				PIPELINE_FA_N_TC_MAX)), params->name,
				arg_name, arg_value);

			continue;
		}

		/* flow_id_offset */
		if (strcmp(arg_name, "flow_id_offset") == 0) {
			int status;

			PIPELINE_PARSE_ERR_DUPLICATE(
				flow_id_offset_present == 0,
				params->name, arg_name);
			flow_id_offset_present = 1;

			status = parser_read_uint32(&p->flow_id_offset,
				arg_value);
			PIPELINE_PARSE_ERR_INV_VAL((status != -EINVAL),
				params->name, arg_name, arg_value);
			PIPELINE_PARSE_ERR_OUT_RNG((status != -ERANGE),
				params->name, arg_name, arg_value);

			continue;
		}

		/* ip_hdr_offset */
		if (strcmp(arg_name, "ip_hdr_offset") == 0) {
			int status;

			PIPELINE_PARSE_ERR_DUPLICATE(
				ip_hdr_offset_present == 0,
				params->name, arg_name);
			ip_hdr_offset_present = 1;

			status = parser_read_uint32(&p->ip_hdr_offset,
				arg_value);
			PIPELINE_PARSE_ERR_INV_VAL((status != -EINVAL),
				params->name, arg_name, arg_value);
			PIPELINE_PARSE_ERR_OUT_RNG((status != -ERANGE),
				params->name, arg_name, arg_value);

			continue;
		}

		/* color_offset */
		if (strcmp(arg_name, "color_offset") == 0) {
			int status;

			PIPELINE_PARSE_ERR_DUPLICATE(
				color_offset_present == 0, params->name,
				arg_name);
			color_offset_present = 1;

			status = parser_read_uint32(&p->color_offset,
				arg_value);
			PIPELINE_PARSE_ERR_INV_VAL((status != -EINVAL),
				params->name, arg_name, arg_value);
			PIPELINE_PARSE_ERR_OUT_RNG((status != -ERANGE),
				params->name, arg_name, arg_value);

			p->dscp_enabled = 1;

			continue;
		}

		/* Unknown argument */
		PIPELINE_PARSE_ERR_INV_ENT(0, params->name, arg_name);
	}

	/* Check that mandatory arguments are present */
	PIPELINE_PARSE_ERR_MANDATORY((n_flows_present), params->name,
		"n_flows");
	PIPELINE_PARSE_ERR_MANDATORY((flow_id_offset_present),
		params->name, "flow_id_offset");
	PIPELINE_PARSE_ERR_MANDATORY((ip_hdr_offset_present),
		params->name, "ip_hdr_offset");
	PIPELINE_PARSE_ERR_MANDATORY((color_offset_present), params->name,
		"color_offset");

	return 0;
}

static void
dscp_init(struct pipeline_flow_actions *p)
{
	uint32_t i;

	for (i = 0; i < PIPELINE_FA_N_DSCP; i++) {
		p->dscp[i].traffic_class = 0;
		p->dscp[i].color = e_RTE_METER_GREEN;
	}
}

static void *pipeline_fa_init(struct pipeline_params *params,
	__rte_unused void *arg)
{
	struct pipeline *p;
	struct pipeline_flow_actions *p_fa;
	uint32_t size, i;

	/* Check input arguments */
	if (params == NULL)
		return NULL;

	if (params->n_ports_in != params->n_ports_out)
		return NULL;

	/* Memory allocation */
	size = RTE_CACHE_LINE_ROUNDUP(
		sizeof(struct pipeline_flow_actions));
	p = rte_zmalloc(NULL, size, RTE_CACHE_LINE_SIZE);
	if (p == NULL)
		return NULL;
	p_fa = (struct pipeline_flow_actions *) p;

	strcpy(p->name, params->name);
	p->log_level = params->log_level;

	PLOG(p, HIGH, "Flow actions");

	/* Parse arguments */
	if (pipeline_fa_parse_args(&p_fa->params, params))
		return NULL;

	dscp_init(p_fa);

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
		struct rte_table_array_params table_array_params = {
			.n_entries = p_fa->params.n_flows,
			.offset = p_fa->params.flow_id_offset,
		};

		struct rte_pipeline_table_params table_params = {
			.ops = &rte_table_array_ops,
			.arg_create = &table_array_params,
			.f_action_hit = get_fa_table_ah_hit(p_fa),
			.f_action_miss = NULL,
			.arg_ah = p_fa,
			.action_data_size =
				sizeof(struct flow_table_entry) -
				sizeof(struct rte_pipeline_table_entry),
		};

		int status;

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

	/* Initialize table entries */
	for (i = 0; i < p_fa->params.n_flows; i++) {
		struct rte_table_array_key key = {
			.pos = i,
		};

		struct flow_table_entry entry;
		struct rte_pipeline_table_entry *entry_ptr;
		int key_found, status;

		flow_table_entry_set_default(p_fa, &entry);

		status = rte_pipeline_table_entry_add(p->p,
			p->table_id[0],
			&key,
			(struct rte_pipeline_table_entry *) &entry,
			&key_found,
			&entry_ptr);

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
	memcpy(p_fa->custom_handlers,
		custom_handlers,
		sizeof(p_fa->custom_handlers));

	return p;
}

static int
pipeline_fa_free(void *pipeline)
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
pipeline_fa_timer(void *pipeline)
{
	struct pipeline *p = (struct pipeline *) pipeline;

	pipeline_msg_req_handle(p);
	rte_pipeline_flush(p->p);

	return 0;
}

void *
pipeline_fa_msg_req_custom_handler(struct pipeline *p, void *msg)
{
	struct pipeline_flow_actions *p_fa =
			(struct pipeline_flow_actions *) p;
	struct pipeline_custom_msg_req *req = msg;
	pipeline_msg_req_handler f_handle;

	f_handle = (req->subtype < PIPELINE_FA_MSG_REQS) ?
		p_fa->custom_handlers[req->subtype] :
		pipeline_msg_req_invalid_handler;

	if (f_handle == NULL)
		f_handle = pipeline_msg_req_invalid_handler;

	return f_handle(p, req);
}

void *
pipeline_fa_msg_req_flow_config_handler(struct pipeline *p, void *msg)
{
	struct pipeline_flow_actions *p_fa = (struct pipeline_flow_actions *) p;
	struct pipeline_fa_flow_config_msg_req *req = msg;
	struct pipeline_fa_flow_config_msg_rsp *rsp = msg;
	struct flow_table_entry *entry;
	uint32_t mask, i;

	/* Set flow table entry to default if not configured before */
	if (req->entry_ptr == NULL) {
		struct rte_table_array_key key = {
			.pos = req->flow_id % p_fa->params.n_flows,
		};

		struct flow_table_entry default_entry;

		int key_found, status;

		flow_table_entry_set_default(p_fa, &default_entry);

		status = rte_pipeline_table_entry_add(p->p,
			p->table_id[0],
			&key,
			(struct rte_pipeline_table_entry *) &default_entry,
			&key_found,
			(struct rte_pipeline_table_entry **) &entry);
		if (status) {
			rsp->status = -1;
			return rsp;
		}
	} else
		entry = (struct flow_table_entry *) req->entry_ptr;

	/* Meter */
	for (i = 0, mask = 1; i < PIPELINE_FA_N_TC_MAX; i++, mask <<= 1) {
		int status;

		if ((mask & req->meter_update_mask) == 0)
			continue;

		status = flow_table_entry_set_meter(entry, i, &req->params);
		if (status) {
			rsp->status = -1;
			return rsp;
		}
	}

	/* Policer */
	for (i = 0, mask = 1; i < PIPELINE_FA_N_TC_MAX; i++, mask <<= 1) {
		if ((mask & req->policer_update_mask) == 0)
			continue;

		flow_table_entry_set_policer(entry, i, &req->params);
	}

	/* Port */
	if (req->port_update)
		flow_table_entry_set_port_id(p_fa, entry, &req->params);

	/* Response */
	rsp->status = 0;
	rsp->entry_ptr = (void *) entry;
	return rsp;
}

void *
pipeline_fa_msg_req_flow_config_bulk_handler(struct pipeline *p, void *msg)
{
	struct pipeline_flow_actions *p_fa = (struct pipeline_flow_actions *) p;
	struct pipeline_fa_flow_config_bulk_msg_req *req = msg;
	struct pipeline_fa_flow_config_bulk_msg_rsp *rsp = msg;
	uint32_t i;

	for (i = 0; i < req->n_flows; i++) {
		struct flow_table_entry *entry;
		uint32_t j, mask;

		/* Set flow table entry to default if not configured before */
		if (req->entry_ptr[i] == NULL) {
			struct rte_table_array_key key = {
				.pos = req->flow_id[i] % p_fa->params.n_flows,
			};

			struct flow_table_entry entry_to_add;

			int key_found, status;

			flow_table_entry_set_default(p_fa, &entry_to_add);

			status = rte_pipeline_table_entry_add(p->p,
			 p->table_id[0],
			 &key,
			 (struct rte_pipeline_table_entry *) &entry_to_add,
			 &key_found,
			 (struct rte_pipeline_table_entry **) &entry);
			if (status) {
				rsp->n_flows = i;
				return rsp;
			}

			req->entry_ptr[i] = (void *) entry;
		} else
			entry = (struct flow_table_entry *) req->entry_ptr[i];

		/* Meter */
		for (j = 0, mask = 1;
			j < PIPELINE_FA_N_TC_MAX;
			j++, mask <<= 1) {
			int status;

			if ((mask & req->meter_update_mask) == 0)
				continue;

			status = flow_table_entry_set_meter(entry,
				j, &req->params[i]);
			if (status) {
				rsp->n_flows = i;
				return rsp;
			}
		}

		/* Policer */
		for (j = 0, mask = 1;
			j < PIPELINE_FA_N_TC_MAX;
			j++, mask <<= 1) {
			if ((mask & req->policer_update_mask) == 0)
				continue;

			flow_table_entry_set_policer(entry,
			 j, &req->params[i]);
		}

		/* Port */
		if (req->port_update)
			flow_table_entry_set_port_id(p_fa,
			 entry, &req->params[i]);
	}

	/* Response */
	rsp->n_flows = i;
	return rsp;
}

void *
pipeline_fa_msg_req_dscp_config_handler(struct pipeline *p, void *msg)
{
	struct pipeline_flow_actions *p_fa = (struct pipeline_flow_actions *) p;
	struct pipeline_fa_dscp_config_msg_req *req = msg;
	struct pipeline_fa_dscp_config_msg_rsp *rsp = msg;

	/* Check request */
	if ((req->dscp >= PIPELINE_FA_N_DSCP) ||
		(req->traffic_class >= PIPELINE_FA_N_TC_MAX) ||
		(req->color >= e_RTE_METER_COLORS)) {
		rsp->status = -1;
		return rsp;
	}

	p_fa->dscp[req->dscp].traffic_class = req->traffic_class;
	p_fa->dscp[req->dscp].color = req->color;
	rsp->status = 0;
	return rsp;
}

void *
pipeline_fa_msg_req_policer_stats_read_handler(__rte_unused struct pipeline *p,
	void *msg)
{
	struct pipeline_fa_policer_stats_msg_req *req = msg;
	struct pipeline_fa_policer_stats_msg_rsp *rsp = msg;

	struct flow_table_entry *entry = req->entry_ptr;
	uint32_t policer_id = req->policer_id;
	int clear = req->clear;

	/* Check request */
	if ((req->entry_ptr == NULL) ||
		(req->policer_id >= PIPELINE_FA_N_TC_MAX)) {
		rsp->status = -1;
		return rsp;
	}

	memcpy(&rsp->stats,
		&entry->mp[policer_id].stats,
		sizeof(rsp->stats));
	if (clear)
		memset(&entry->mp[policer_id].stats,
			0, sizeof(entry->mp[policer_id].stats));
	rsp->status = 0;
	return rsp;
}

struct pipeline_be_ops pipeline_flow_actions_be_ops = {
	.f_init = pipeline_fa_init,
	.f_free = pipeline_fa_free,
	.f_run = NULL,
	.f_timer = pipeline_fa_timer,
};
