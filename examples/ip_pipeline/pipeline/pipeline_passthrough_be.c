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

enum flow_key_type {
	FLOW_KEY_QINQ,
	FLOW_KEY_IPV4_5TUPLE,
	FLOW_KEY_IPV6_5TUPLE,
};

struct pipeline_passthrough {
	struct pipeline p;

	uint32_t key_type_valid;
	enum flow_key_type key_type;
	uint32_t key_offset_rd;
	uint32_t key_offset_wr;
	uint32_t hash_offset;

	rte_table_hash_op_hash f_hash;
	rte_pipeline_port_in_action_handler f_port_in_ah;
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

static inline void
pkt_work_key_qinq(
	struct rte_mbuf *pkt,
	void *arg)
{
	struct pipeline_passthrough *p_pt = arg;
	uint32_t key_offset_rd = p_pt->key_offset_rd;
	uint32_t key_offset_wr = p_pt->key_offset_wr;
	uint32_t hash_offset = p_pt->hash_offset;

	uint64_t *key_rd = RTE_MBUF_METADATA_UINT64_PTR(pkt, key_offset_rd);
	uint64_t *key_wr = RTE_MBUF_METADATA_UINT64_PTR(pkt, key_offset_wr);
	uint32_t *hash = RTE_MBUF_METADATA_UINT32_PTR(pkt, hash_offset);

	/* Read */
	uint64_t key_qinq = *key_rd & rte_bswap64(0x00000FFF00000FFFLLU);

	/* Compute */
	uint32_t hash_qinq = p_pt->f_hash(&key_qinq, 8, 0);

	/* Write */
	*key_wr = key_qinq;
	*hash = hash_qinq;
}

static inline void
pkt4_work_key_qinq(
	struct rte_mbuf **pkt,
	void *arg)
{
	struct pipeline_passthrough *p_pt = arg;
	uint32_t key_offset_rd = p_pt->key_offset_rd;
	uint32_t key_offset_wr = p_pt->key_offset_wr;
	uint32_t hash_offset = p_pt->hash_offset;

	uint64_t *key_rd0 = RTE_MBUF_METADATA_UINT64_PTR(pkt[0], key_offset_rd);
	uint64_t *key_wr0 = RTE_MBUF_METADATA_UINT64_PTR(pkt[0], key_offset_wr);
	uint32_t *hash0 = RTE_MBUF_METADATA_UINT32_PTR(pkt[0], hash_offset);

	uint64_t *key_rd1 = RTE_MBUF_METADATA_UINT64_PTR(pkt[1], key_offset_rd);
	uint64_t *key_wr1 = RTE_MBUF_METADATA_UINT64_PTR(pkt[1], key_offset_wr);
	uint32_t *hash1 = RTE_MBUF_METADATA_UINT32_PTR(pkt[1], hash_offset);

	uint64_t *key_rd2 = RTE_MBUF_METADATA_UINT64_PTR(pkt[2], key_offset_rd);
	uint64_t *key_wr2 = RTE_MBUF_METADATA_UINT64_PTR(pkt[2], key_offset_wr);
	uint32_t *hash2 = RTE_MBUF_METADATA_UINT32_PTR(pkt[2], hash_offset);

	uint64_t *key_rd3 = RTE_MBUF_METADATA_UINT64_PTR(pkt[3], key_offset_rd);
	uint64_t *key_wr3 = RTE_MBUF_METADATA_UINT64_PTR(pkt[3], key_offset_wr);
	uint32_t *hash3 = RTE_MBUF_METADATA_UINT32_PTR(pkt[3], hash_offset);

	/* Read */
	uint64_t key_qinq0 = *key_rd0 & rte_bswap64(0x00000FFF00000FFFLLU);
	uint64_t key_qinq1 = *key_rd1 & rte_bswap64(0x00000FFF00000FFFLLU);
	uint64_t key_qinq2 = *key_rd2 & rte_bswap64(0x00000FFF00000FFFLLU);
	uint64_t key_qinq3 = *key_rd3 & rte_bswap64(0x00000FFF00000FFFLLU);

	/* Compute */
	uint32_t hash_qinq0 = p_pt->f_hash(&key_qinq0, 8, 0);
	uint32_t hash_qinq1 = p_pt->f_hash(&key_qinq1, 8, 0);
	uint32_t hash_qinq2 = p_pt->f_hash(&key_qinq2, 8, 0);
	uint32_t hash_qinq3 = p_pt->f_hash(&key_qinq3, 8, 0);

	/* Write */
	*key_wr0 = key_qinq0;
	*key_wr1 = key_qinq1;
	*key_wr2 = key_qinq2;
	*key_wr3 = key_qinq3;

	*hash0 = hash_qinq0;
	*hash1 = hash_qinq1;
	*hash2 = hash_qinq2;
	*hash3 = hash_qinq3;
}

PIPELINE_PORT_IN_AH(port_in_ah_key_qinq, pkt_work_key_qinq, pkt4_work_key_qinq);

static inline void
pkt_work_key_ipv4(
	struct rte_mbuf *pkt,
	void *arg)
{
	struct pipeline_passthrough *p_pt = arg;
	uint32_t key_offset_rd = p_pt->key_offset_rd;
	uint32_t key_offset_wr = p_pt->key_offset_wr;
	uint32_t hash_offset = p_pt->hash_offset;

	uint64_t *key_rd = RTE_MBUF_METADATA_UINT64_PTR(pkt, key_offset_rd);
	uint64_t *key_wr = RTE_MBUF_METADATA_UINT64_PTR(pkt, key_offset_wr);
	uint32_t *hash = RTE_MBUF_METADATA_UINT32_PTR(pkt, hash_offset);
	uint64_t key_ipv4[2];
	uint32_t hash_ipv4;

	/* Read */
	key_ipv4[0] = key_rd[0] & rte_bswap64(0x00FF0000FFFFFFFFLLU);
	key_ipv4[1] = key_rd[1];

	/* Compute */
	hash_ipv4 = p_pt->f_hash(key_ipv4, 16, 0);

	/* Write */
	key_wr[0] = key_ipv4[0];
	key_wr[1] = key_ipv4[1];
	*hash = hash_ipv4;
}

static inline void
pkt4_work_key_ipv4(
	struct rte_mbuf **pkt,
	void *arg)
{
	struct pipeline_passthrough *p_pt = arg;
	uint32_t key_offset_rd = p_pt->key_offset_rd;
	uint32_t key_offset_wr = p_pt->key_offset_wr;
	uint32_t hash_offset = p_pt->hash_offset;

	uint64_t *key_rd0 = RTE_MBUF_METADATA_UINT64_PTR(pkt[0], key_offset_rd);
	uint64_t *key_wr0 = RTE_MBUF_METADATA_UINT64_PTR(pkt[0], key_offset_wr);
	uint32_t *hash0 = RTE_MBUF_METADATA_UINT32_PTR(pkt[0], hash_offset);

	uint64_t *key_rd1 = RTE_MBUF_METADATA_UINT64_PTR(pkt[1], key_offset_rd);
	uint64_t *key_wr1 = RTE_MBUF_METADATA_UINT64_PTR(pkt[1], key_offset_wr);
	uint32_t *hash1 = RTE_MBUF_METADATA_UINT32_PTR(pkt[1], hash_offset);

	uint64_t *key_rd2 = RTE_MBUF_METADATA_UINT64_PTR(pkt[2], key_offset_rd);
	uint64_t *key_wr2 = RTE_MBUF_METADATA_UINT64_PTR(pkt[2], key_offset_wr);
	uint32_t *hash2 = RTE_MBUF_METADATA_UINT32_PTR(pkt[2], hash_offset);

	uint64_t *key_rd3 = RTE_MBUF_METADATA_UINT64_PTR(pkt[3], key_offset_rd);
	uint64_t *key_wr3 = RTE_MBUF_METADATA_UINT64_PTR(pkt[3], key_offset_wr);
	uint32_t *hash3 = RTE_MBUF_METADATA_UINT32_PTR(pkt[3], hash_offset);

	uint64_t key_ipv4_0[2];
	uint64_t key_ipv4_1[2];
	uint64_t key_ipv4_2[2];
	uint64_t key_ipv4_3[2];

	uint32_t hash_ipv4_0;
	uint32_t hash_ipv4_1;
	uint32_t hash_ipv4_2;
	uint32_t hash_ipv4_3;

	/* Read */
	key_ipv4_0[0] = key_rd0[0] & rte_bswap64(0x00FF0000FFFFFFFFLLU);
	key_ipv4_1[0] = key_rd1[0] & rte_bswap64(0x00FF0000FFFFFFFFLLU);
	key_ipv4_2[0] = key_rd2[0] & rte_bswap64(0x00FF0000FFFFFFFFLLU);
	key_ipv4_3[0] = key_rd3[0] & rte_bswap64(0x00FF0000FFFFFFFFLLU);

	key_ipv4_0[1] = key_rd0[1];
	key_ipv4_1[1] = key_rd1[1];
	key_ipv4_2[1] = key_rd2[1];
	key_ipv4_3[1] = key_rd3[1];

	/* Compute */
	hash_ipv4_0 = p_pt->f_hash(key_ipv4_0, 16, 0);
	hash_ipv4_1 = p_pt->f_hash(key_ipv4_1, 16, 0);
	hash_ipv4_2 = p_pt->f_hash(key_ipv4_2, 16, 0);
	hash_ipv4_3 = p_pt->f_hash(key_ipv4_3, 16, 0);

	/* Write */
	key_wr0[0] = key_ipv4_0[0];
	key_wr1[0] = key_ipv4_1[0];
	key_wr2[0] = key_ipv4_2[0];
	key_wr3[0] = key_ipv4_3[0];

	key_wr0[1] = key_ipv4_0[1];
	key_wr1[1] = key_ipv4_1[1];
	key_wr2[1] = key_ipv4_2[1];
	key_wr3[1] = key_ipv4_3[1];

	*hash0 = hash_ipv4_0;
	*hash1 = hash_ipv4_1;
	*hash2 = hash_ipv4_2;
	*hash3 = hash_ipv4_3;
}

PIPELINE_PORT_IN_AH(port_in_ah_key_ipv4, pkt_work_key_ipv4, pkt4_work_key_ipv4);

static inline void
pkt_work_key_ipv6(
	struct rte_mbuf *pkt,
	void *arg)
{
	struct pipeline_passthrough *p_pt = arg;
	uint32_t key_offset_rd = p_pt->key_offset_rd;
	uint32_t key_offset_wr = p_pt->key_offset_wr;
	uint32_t hash_offset = p_pt->hash_offset;

	uint64_t *key_rd = RTE_MBUF_METADATA_UINT64_PTR(pkt, key_offset_rd);
	uint64_t *key_wr = RTE_MBUF_METADATA_UINT64_PTR(pkt, key_offset_wr);
	uint32_t *hash = RTE_MBUF_METADATA_UINT32_PTR(pkt, hash_offset);
	uint64_t key_ipv6[8];
	uint32_t hash_ipv6;

	/* Read */
	key_ipv6[0] = key_rd[0] & rte_bswap64(0x0000FF00FFFFFFFFLLU);
	key_ipv6[1] = key_rd[1];
	key_ipv6[2] = key_rd[2];
	key_ipv6[3] = key_rd[3];
	key_ipv6[4] = key_rd[4];
	key_ipv6[5] = 0;
	key_ipv6[6] = 0;
	key_ipv6[7] = 0;

	/* Compute */
	hash_ipv6 = p_pt->f_hash(key_ipv6, 64, 0);

	/* Write */
	key_wr[0] = key_ipv6[0];
	key_wr[1] = key_ipv6[1];
	key_wr[2] = key_ipv6[2];
	key_wr[3] = key_ipv6[3];
	key_wr[4] = key_ipv6[4];
	key_wr[5] = 0;
	key_wr[6] = 0;
	key_wr[7] = 0;
	*hash = hash_ipv6;
}

static inline void
pkt4_work_key_ipv6(
	struct rte_mbuf **pkt,
	void *arg)
{
	struct pipeline_passthrough *p_pt = arg;
	uint32_t key_offset_rd = p_pt->key_offset_rd;
	uint32_t key_offset_wr = p_pt->key_offset_wr;
	uint32_t hash_offset = p_pt->hash_offset;

	uint64_t *key_rd0 = RTE_MBUF_METADATA_UINT64_PTR(pkt[0], key_offset_rd);
	uint64_t *key_wr0 = RTE_MBUF_METADATA_UINT64_PTR(pkt[0], key_offset_wr);
	uint32_t *hash0 = RTE_MBUF_METADATA_UINT32_PTR(pkt[0], hash_offset);

	uint64_t *key_rd1 = RTE_MBUF_METADATA_UINT64_PTR(pkt[1], key_offset_rd);
	uint64_t *key_wr1 = RTE_MBUF_METADATA_UINT64_PTR(pkt[1], key_offset_wr);
	uint32_t *hash1 = RTE_MBUF_METADATA_UINT32_PTR(pkt[1], hash_offset);

	uint64_t *key_rd2 = RTE_MBUF_METADATA_UINT64_PTR(pkt[2], key_offset_rd);
	uint64_t *key_wr2 = RTE_MBUF_METADATA_UINT64_PTR(pkt[2], key_offset_wr);
	uint32_t *hash2 = RTE_MBUF_METADATA_UINT32_PTR(pkt[2], hash_offset);

	uint64_t *key_rd3 = RTE_MBUF_METADATA_UINT64_PTR(pkt[3], key_offset_rd);
	uint64_t *key_wr3 = RTE_MBUF_METADATA_UINT64_PTR(pkt[3], key_offset_wr);
	uint32_t *hash3 = RTE_MBUF_METADATA_UINT32_PTR(pkt[3], hash_offset);

	uint64_t key_ipv6_0[8];
	uint64_t key_ipv6_1[8];
	uint64_t key_ipv6_2[8];
	uint64_t key_ipv6_3[8];

	uint32_t hash_ipv6_0;
	uint32_t hash_ipv6_1;
	uint32_t hash_ipv6_2;
	uint32_t hash_ipv6_3;

	/* Read */
	key_ipv6_0[0] = key_rd0[0] & rte_bswap64(0x0000FF00FFFFFFFFLLU);
	key_ipv6_1[0] = key_rd1[0] & rte_bswap64(0x0000FF00FFFFFFFFLLU);
	key_ipv6_2[0] = key_rd2[0] & rte_bswap64(0x0000FF00FFFFFFFFLLU);
	key_ipv6_3[0] = key_rd3[0] & rte_bswap64(0x0000FF00FFFFFFFFLLU);

	key_ipv6_0[1] = key_rd0[1];
	key_ipv6_1[1] = key_rd1[1];
	key_ipv6_2[1] = key_rd2[1];
	key_ipv6_3[1] = key_rd3[1];

	key_ipv6_0[2] = key_rd0[2];
	key_ipv6_1[2] = key_rd1[2];
	key_ipv6_2[2] = key_rd2[2];
	key_ipv6_3[2] = key_rd3[2];

	key_ipv6_0[3] = key_rd0[3];
	key_ipv6_1[3] = key_rd1[3];
	key_ipv6_2[3] = key_rd2[3];
	key_ipv6_3[3] = key_rd3[3];

	key_ipv6_0[4] = key_rd0[4];
	key_ipv6_1[4] = key_rd1[4];
	key_ipv6_2[4] = key_rd2[4];
	key_ipv6_3[4] = key_rd3[4];

	key_ipv6_0[5] = 0;
	key_ipv6_1[5] = 0;
	key_ipv6_2[5] = 0;
	key_ipv6_3[5] = 0;

	key_ipv6_0[6] = 0;
	key_ipv6_1[6] = 0;
	key_ipv6_2[6] = 0;
	key_ipv6_3[6] = 0;

	key_ipv6_0[7] = 0;
	key_ipv6_1[7] = 0;
	key_ipv6_2[7] = 0;
	key_ipv6_3[7] = 0;

	/* Compute */
	hash_ipv6_0 = p_pt->f_hash(key_ipv6_0, 64, 0);
	hash_ipv6_1 = p_pt->f_hash(key_ipv6_1, 64, 0);
	hash_ipv6_2 = p_pt->f_hash(key_ipv6_2, 64, 0);
	hash_ipv6_3 = p_pt->f_hash(key_ipv6_3, 64, 0);

	/* Write */
	key_wr0[0] = key_ipv6_0[0];
	key_wr1[0] = key_ipv6_1[0];
	key_wr2[0] = key_ipv6_2[0];
	key_wr3[0] = key_ipv6_3[0];

	key_wr0[1] = key_ipv6_0[1];
	key_wr1[1] = key_ipv6_1[1];
	key_wr2[1] = key_ipv6_2[1];
	key_wr3[1] = key_ipv6_3[1];

	key_wr0[2] = key_ipv6_0[2];
	key_wr1[2] = key_ipv6_1[2];
	key_wr2[2] = key_ipv6_2[2];
	key_wr3[2] = key_ipv6_3[2];

	key_wr0[3] = key_ipv6_0[3];
	key_wr1[3] = key_ipv6_1[3];
	key_wr2[3] = key_ipv6_2[3];
	key_wr3[3] = key_ipv6_3[3];

	key_wr0[4] = key_ipv6_0[4];
	key_wr1[4] = key_ipv6_1[4];
	key_wr2[4] = key_ipv6_2[4];
	key_wr3[4] = key_ipv6_3[4];

	key_wr0[5] = 0;
	key_wr0[5] = 0;
	key_wr0[5] = 0;
	key_wr0[5] = 0;

	key_wr0[6] = 0;
	key_wr0[6] = 0;
	key_wr0[6] = 0;
	key_wr0[6] = 0;

	key_wr0[7] = 0;
	key_wr0[7] = 0;
	key_wr0[7] = 0;
	key_wr0[7] = 0;

	*hash0 = hash_ipv6_0;
	*hash1 = hash_ipv6_1;
	*hash2 = hash_ipv6_2;
	*hash3 = hash_ipv6_3;
}

PIPELINE_PORT_IN_AH(port_in_ah_key_ipv6, pkt_work_key_ipv6, pkt4_work_key_ipv6);

static int
pipeline_passthrough_parse_args(struct pipeline_passthrough *p,
	struct pipeline_params *params)
{
	uint32_t key_type_present = 0;
	uint32_t key_offset_rd_present = 0;
	uint32_t key_offset_wr_present = 0;
	uint32_t hash_offset_present = 0;
	uint32_t i;

	for (i = 0; i < params->n_args; i++) {
		char *arg_name = params->args_name[i];
		char *arg_value = params->args_value[i];

		/* key_type */
		if (strcmp(arg_name, "key_type") == 0) {
			if (key_type_present)
				return -1;
			key_type_present = 1;

			if ((strcmp(arg_value, "q-in-q") == 0) ||
				(strcmp(arg_value, "qinq") == 0))
				p->key_type = FLOW_KEY_QINQ;
			else if (strcmp(arg_value, "ipv4_5tuple") == 0)
				p->key_type = FLOW_KEY_IPV4_5TUPLE;
			else if (strcmp(arg_value, "ipv6_5tuple") == 0)
				p->key_type = FLOW_KEY_IPV6_5TUPLE;
			else
				return -1;

			p->key_type_valid = 1;

			continue;
		}

		/* key_offset_rd */
		if (strcmp(arg_name, "key_offset_rd") == 0) {
			if (key_offset_rd_present)
				return -1;
			key_offset_rd_present = 1;

			p->key_offset_rd = atoi(arg_value);

			continue;
		}

		/* key_offset_wr */
		if (strcmp(arg_name, "key_offset_wr") == 0) {
			if (key_offset_wr_present)
				return -1;
			key_offset_wr_present = 1;

			p->key_offset_wr = atoi(arg_value);

			continue;
		}

		/* hash_offset */
		if (strcmp(arg_name, "hash_offset") == 0) {
			if (hash_offset_present)
				return -1;
			hash_offset_present = 1;

			p->hash_offset = atoi(arg_value);

			continue;
		}

		/* any other */
		return -1;
	}

	/* Check that mandatory arguments are present */
	if ((key_offset_rd_present != key_type_present) ||
		(key_offset_wr_present != key_type_present) ||
		(hash_offset_present != key_type_present))
		return -1;

	return 0;
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
	if (pipeline_passthrough_parse_args(p_pt, params))
		return NULL;

	if (p_pt->key_type_valid == 0) {
		p_pt->f_hash = NULL;
		p_pt->f_port_in_ah = NULL;
	} else
		switch (p_pt->key_type) {
		case FLOW_KEY_QINQ:
			p_pt->f_hash = hash_default_key8;
			p_pt->f_port_in_ah = port_in_ah_key_qinq;
			break;

		case FLOW_KEY_IPV4_5TUPLE:
			p_pt->f_hash = hash_default_key16;
			p_pt->f_port_in_ah = port_in_ah_key_ipv4;
			break;

		case FLOW_KEY_IPV6_5TUPLE:
			p_pt->f_hash = hash_default_key64;
			p_pt->f_port_in_ah = port_in_ah_key_ipv6;
			break;

		default:
			p_pt->f_hash = NULL;
			p_pt->f_port_in_ah = NULL;
		}

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
			.f_action = p_pt->f_port_in_ah,
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
