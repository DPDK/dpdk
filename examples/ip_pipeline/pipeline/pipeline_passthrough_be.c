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
#include <rte_byteorder.h>
#include <rte_table_stub.h>
#include <rte_table_hash.h>
#include <rte_pipeline.h>

#include "pipeline_passthrough_be.h"
#include "pipeline_actions_common.h"
#include "hash_func.h"

struct pipeline_passthrough {
	struct pipeline p;
	struct pipeline_passthrough_params params;
	rte_table_hash_op_hash f_hash;
} __rte_cache_aligned;

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
		pipeline_msg_req_invalid_handler,
};

static inline __attribute__((always_inline)) void
pkt_work(
	struct rte_mbuf *pkt,
	void *arg,
	uint32_t dma_size,
	uint32_t hash_enabled)
{
	struct pipeline_passthrough *p = arg;

	uint64_t *dma_dst = RTE_MBUF_METADATA_UINT64_PTR(pkt,
		p->params.dma_dst_offset);
	uint64_t *dma_src = RTE_MBUF_METADATA_UINT64_PTR(pkt,
		p->params.dma_src_offset);
	uint64_t *dma_mask = (uint64_t *) p->params.dma_src_mask;
	uint32_t *dma_hash = RTE_MBUF_METADATA_UINT32_PTR(pkt,
		p->params.dma_hash_offset);
	uint32_t i;

	/* Read (dma_src), compute (dma_dst), write (dma_dst) */
	for (i = 0; i < (dma_size / 8); i++)
		dma_dst[i] = dma_src[i] & dma_mask[i];

	/* Read (dma_dst), compute (hash), write (hash) */
	if (hash_enabled)
		*dma_hash = p->f_hash(dma_dst, dma_size, 0);
}

static inline __attribute__((always_inline)) void
pkt4_work(
	struct rte_mbuf **pkts,
	void *arg,
	uint32_t dma_size,
	uint32_t hash_enabled)
{
	struct pipeline_passthrough *p = arg;

	uint64_t *dma_dst0 = RTE_MBUF_METADATA_UINT64_PTR(pkts[0],
		p->params.dma_dst_offset);
	uint64_t *dma_dst1 = RTE_MBUF_METADATA_UINT64_PTR(pkts[1],
		p->params.dma_dst_offset);
	uint64_t *dma_dst2 = RTE_MBUF_METADATA_UINT64_PTR(pkts[2],
		p->params.dma_dst_offset);
	uint64_t *dma_dst3 = RTE_MBUF_METADATA_UINT64_PTR(pkts[3],
		p->params.dma_dst_offset);

	uint64_t *dma_src0 = RTE_MBUF_METADATA_UINT64_PTR(pkts[0],
		p->params.dma_src_offset);
	uint64_t *dma_src1 = RTE_MBUF_METADATA_UINT64_PTR(pkts[1],
		p->params.dma_src_offset);
	uint64_t *dma_src2 = RTE_MBUF_METADATA_UINT64_PTR(pkts[2],
		p->params.dma_src_offset);
	uint64_t *dma_src3 = RTE_MBUF_METADATA_UINT64_PTR(pkts[3],
		p->params.dma_src_offset);

	uint64_t *dma_mask = (uint64_t *) p->params.dma_src_mask;

	uint32_t *dma_hash0 = RTE_MBUF_METADATA_UINT32_PTR(pkts[0],
		p->params.dma_hash_offset);
	uint32_t *dma_hash1 = RTE_MBUF_METADATA_UINT32_PTR(pkts[1],
		p->params.dma_hash_offset);
	uint32_t *dma_hash2 = RTE_MBUF_METADATA_UINT32_PTR(pkts[2],
		p->params.dma_hash_offset);
	uint32_t *dma_hash3 = RTE_MBUF_METADATA_UINT32_PTR(pkts[3],
		p->params.dma_hash_offset);

	uint32_t i;

	/* Read (dma_src), compute (dma_dst), write (dma_dst) */
	for (i = 0; i < (dma_size / 8); i++) {
		dma_dst0[i] = dma_src0[i] & dma_mask[i];
		dma_dst1[i] = dma_src1[i] & dma_mask[i];
		dma_dst2[i] = dma_src2[i] & dma_mask[i];
		dma_dst3[i] = dma_src3[i] & dma_mask[i];
	}

	/* Read (dma_dst), compute (hash), write (hash) */
	if (hash_enabled) {
		*dma_hash0 = p->f_hash(dma_dst0, dma_size, 0);
		*dma_hash1 = p->f_hash(dma_dst1, dma_size, 0);
		*dma_hash2 = p->f_hash(dma_dst2, dma_size, 0);
		*dma_hash3 = p->f_hash(dma_dst3, dma_size, 0);
	}
}

#define PKT_WORK(dma_size, hash_enabled)			\
static inline void						\
pkt_work_size##dma_size##_hash##hash_enabled(			\
	struct rte_mbuf *pkt,					\
	void *arg)						\
{								\
	pkt_work(pkt, arg, dma_size, hash_enabled);		\
}

#define PKT4_WORK(dma_size, hash_enabled)			\
static inline void						\
pkt4_work_size##dma_size##_hash##hash_enabled(			\
	struct rte_mbuf **pkts,					\
	void *arg)						\
{								\
	pkt4_work(pkts, arg, dma_size, hash_enabled);		\
}

#define port_in_ah(dma_size, hash_enabled)			\
PKT_WORK(dma_size, hash_enabled)				\
PKT4_WORK(dma_size, hash_enabled)				\
PIPELINE_PORT_IN_AH(port_in_ah_size##dma_size##_hash##hash_enabled,\
	pkt_work_size##dma_size##_hash##hash_enabled,		\
	pkt4_work_size##dma_size##_hash##hash_enabled)


port_in_ah(8, 0)
port_in_ah(8, 1)
port_in_ah(16, 0)
port_in_ah(16, 1)
port_in_ah(24, 0)
port_in_ah(24, 1)
port_in_ah(32, 0)
port_in_ah(32, 1)
port_in_ah(40, 0)
port_in_ah(40, 1)
port_in_ah(48, 0)
port_in_ah(48, 1)
port_in_ah(56, 0)
port_in_ah(56, 1)
port_in_ah(64, 0)
port_in_ah(64, 1)

static rte_pipeline_port_in_action_handler
get_port_in_ah(struct pipeline_passthrough *p)
{
	if (p->params.dma_enabled == 0)
		return NULL;

	if (p->params.dma_hash_enabled)
		switch (p->params.dma_size) {

		case 8: return port_in_ah_size8_hash1;
		case 16: return port_in_ah_size16_hash1;
		case 24: return port_in_ah_size24_hash1;
		case 32: return port_in_ah_size32_hash1;
		case 40: return port_in_ah_size40_hash1;
		case 48: return port_in_ah_size48_hash1;
		case 56: return port_in_ah_size56_hash1;
		case 64: return port_in_ah_size64_hash1;
		default: return NULL;
		}
	else
		switch (p->params.dma_size) {

		case 8: return port_in_ah_size8_hash0;
		case 16: return port_in_ah_size16_hash0;
		case 24: return port_in_ah_size24_hash0;
		case 32: return port_in_ah_size32_hash0;
		case 40: return port_in_ah_size40_hash0;
		case 48: return port_in_ah_size48_hash0;
		case 56: return port_in_ah_size56_hash0;
		case 64: return port_in_ah_size64_hash0;
		default: return NULL;
		}
}

int
pipeline_passthrough_parse_args(struct pipeline_passthrough_params *p,
	struct pipeline_params *params)
{
	uint32_t dma_dst_offset_present = 0;
	uint32_t dma_src_offset_present = 0;
	uint32_t dma_src_mask_present = 0;
	uint32_t dma_size_present = 0;
	uint32_t dma_hash_offset_present = 0;
	uint32_t i;

	/* default values */
	p->dma_enabled = 0;
	p->dma_hash_enabled = 0;
	memset(p->dma_src_mask, 0xFF, sizeof(p->dma_src_mask));

	for (i = 0; i < params->n_args; i++) {
		char *arg_name = params->args_name[i];
		char *arg_value = params->args_value[i];

		/* dma_dst_offset */
		if (strcmp(arg_name, "dma_dst_offset") == 0) {
			if (dma_dst_offset_present)
				return -1;
			dma_dst_offset_present = 1;

			p->dma_dst_offset = atoi(arg_value);
			p->dma_enabled = 1;

			continue;
		}

		/* dma_src_offset */
		if (strcmp(arg_name, "dma_src_offset") == 0) {
			if (dma_src_offset_present)
				return -1;
			dma_src_offset_present = 1;

			p->dma_src_offset = atoi(arg_value);
			p->dma_enabled = 1;

			continue;
		}

		/* dma_size */
		if (strcmp(arg_name, "dma_size") == 0) {
			if (dma_size_present)
				return -1;
			dma_size_present = 1;

			p->dma_size = atoi(arg_value);
			if ((p->dma_size == 0) ||
				(p->dma_size > PIPELINE_PASSTHROUGH_DMA_SIZE_MAX) ||
				((p->dma_size % 8) != 0))
				return -1;

			p->dma_enabled = 1;

			continue;
		}

		/* dma_src_mask */
		if (strcmp(arg_name, "dma_src_mask") == 0) {
			uint32_t dma_size;
			int status;

			if (dma_src_mask_present ||
				(dma_size_present == 0))
				return -1;
			dma_src_mask_present = 1;

			dma_size = p->dma_size;
			status = parse_hex_string(arg_value,
				p->dma_src_mask,
				&dma_size);
			if (status ||
				(dma_size != p->dma_size))
				return -1;

			p->dma_enabled = 1;

			continue;
		}

		/* dma_dst_offset */
		if (strcmp(arg_name, "dma_dst_offset") == 0) {
			if (dma_dst_offset_present)
				return -1;
			dma_dst_offset_present = 1;

			p->dma_dst_offset = atoi(arg_value);
			p->dma_enabled = 1;

			continue;
		}

		/* dma_hash_offset */
		if (strcmp(arg_name, "dma_hash_offset") == 0) {
			if (dma_hash_offset_present)
				return -1;
			dma_hash_offset_present = 1;

			p->dma_hash_offset = atoi(arg_value);
			p->dma_hash_enabled = 1;
			p->dma_enabled = 1;

			continue;
		}

		/* any other */
		return -1;
	}

	/* Check correlations between arguments */
	if ((dma_dst_offset_present != p->dma_enabled) ||
		(dma_src_offset_present != p->dma_enabled) ||
		(dma_size_present != p->dma_enabled) ||
		(dma_hash_offset_present != p->dma_hash_enabled) ||
		(p->dma_hash_enabled > p->dma_enabled))
		return -1;

	return 0;
}


static rte_table_hash_op_hash
get_hash_function(struct pipeline_passthrough *p)
{
	switch (p->params.dma_size) {

	case 8: return hash_default_key8;
	case 16: return hash_default_key16;
	case 24: return hash_default_key24;
	case 32: return hash_default_key32;
	case 40: return hash_default_key40;
	case 48: return hash_default_key48;
	case 56: return hash_default_key56;
	case 64: return hash_default_key64;
	default: return NULL;
	}
}

static void*
pipeline_passthrough_init(struct pipeline_params *params,
	__rte_unused void *arg)
{
	struct pipeline *p;
	struct pipeline_passthrough *p_pt;
	uint32_t size, i;

	/* Check input arguments */
	if ((params == NULL) ||
		(params->n_ports_in == 0) ||
		(params->n_ports_out == 0) ||
		(params->n_ports_in < params->n_ports_out) ||
		(params->n_ports_in % params->n_ports_out))
		return NULL;

	/* Memory allocation */
	size = RTE_CACHE_LINE_ROUNDUP(sizeof(struct pipeline_passthrough));
	p = rte_zmalloc(NULL, size, RTE_CACHE_LINE_SIZE);
	p_pt = (struct pipeline_passthrough *) p;
	if (p == NULL)
		return NULL;

	strcpy(p->name, params->name);
	p->log_level = params->log_level;

	PLOG(p, HIGH, "Pass-through");

	/* Parse arguments */
	if (pipeline_passthrough_parse_args(&p_pt->params, params))
		return NULL;
	p_pt->f_hash = get_hash_function(p_pt);

	/* Pipeline */
	{
		struct rte_pipeline_params pipeline_params = {
			.name = "PASS-THROUGH",
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
			.f_action = get_port_in_ah(p_pt),
			.arg_ah = p_pt,
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
	p->n_tables = p->n_ports_in;
	for (i = 0; i < p->n_ports_in; i++) {
		struct rte_pipeline_table_params table_params = {
			.ops = &rte_table_stub_ops,
			.arg_create = NULL,
			.f_action_hit = NULL,
			.f_action_miss = NULL,
			.arg_ah = NULL,
			.action_data_size = 0,
		};

		int status = rte_pipeline_table_create(p->p,
			&table_params,
			&p->table_id[i]);

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
			p->table_id[i]);

		if (status) {
			rte_pipeline_free(p->p);
			rte_free(p);
			return NULL;
		}
	}

	/* Add entries to tables */
	for (i = 0; i < p->n_ports_in; i++) {
		struct rte_pipeline_table_entry default_entry = {
			.action = RTE_PIPELINE_ACTION_PORT,
			{.port_id = p->port_out_id[
				i / (p->n_ports_in / p->n_ports_out)]},
		};

		struct rte_pipeline_table_entry *default_entry_ptr;

		int status = rte_pipeline_table_default_entry_add(p->p,
			p->table_id[i],
			&default_entry,
			&default_entry_ptr);

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

	return p;
}

static int
pipeline_passthrough_free(void *pipeline)
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
pipeline_passthrough_timer(void *pipeline)
{
	struct pipeline *p = (struct pipeline *) pipeline;

	pipeline_msg_req_handle(p);
	rte_pipeline_flush(p->p);

	return 0;
}

static int
pipeline_passthrough_track(void *pipeline, uint32_t port_in, uint32_t *port_out)
{
	struct pipeline *p = (struct pipeline *) pipeline;

	/* Check input arguments */
	if ((p == NULL) ||
		(port_in >= p->n_ports_in) ||
		(port_out == NULL))
		return -1;

	*port_out = port_in / p->n_ports_in;
	return 0;
}

struct pipeline_be_ops pipeline_passthrough_be_ops = {
	.f_init = pipeline_passthrough_init,
	.f_free = pipeline_passthrough_free,
	.f_run = NULL,
	.f_timer = pipeline_passthrough_timer,
	.f_track = pipeline_passthrough_track,
};
