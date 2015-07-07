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

#include <stdio.h>
#include <string.h>
#include <sys/queue.h>
#include <netinet/in.h>

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_malloc.h>
#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_parse_ipaddr.h>
#include <cmdline_parse_etheraddr.h>

#include "app.h"
#include "pipeline_common_fe.h"
#include "pipeline_flow_classification.h"
#include "hash_func.h"

/*
 * Key conversion
 */

struct pkt_key_qinq {
	uint16_t ethertype_svlan;
	uint16_t svlan;
	uint16_t ethertype_cvlan;
	uint16_t cvlan;
} __attribute__((__packed__));

struct pkt_key_ipv4_5tuple {
	uint8_t ttl;
	uint8_t proto;
	uint16_t checksum;
	uint32_t ip_src;
	uint32_t ip_dst;
	uint16_t port_src;
	uint16_t port_dst;
} __attribute__((__packed__));

struct pkt_key_ipv6_5tuple {
	uint16_t payload_length;
	uint8_t proto;
	uint8_t hop_limit;
	uint8_t ip_src[16];
	uint8_t ip_dst[16];
	uint16_t port_src;
	uint16_t port_dst;
} __attribute__((__packed__));

static int
app_pipeline_fc_key_convert(struct pipeline_fc_key *key_in,
	uint8_t *key_out,
	uint32_t *signature)
{
	uint8_t buffer[PIPELINE_FC_FLOW_KEY_MAX_SIZE];
	void *key_buffer = (key_out) ? key_out : buffer;

	switch (key_in->type) {
	case FLOW_KEY_QINQ:
	{
		struct pkt_key_qinq *qinq = key_buffer;

		qinq->ethertype_svlan = 0;
		qinq->svlan = rte_bswap16(key_in->key.qinq.svlan);
		qinq->ethertype_cvlan = 0;
		qinq->cvlan = rte_bswap16(key_in->key.qinq.cvlan);

		if (signature)
			*signature = (uint32_t) hash_default_key8(qinq, 8, 0);
		return 0;
	}

	case FLOW_KEY_IPV4_5TUPLE:
	{
		struct pkt_key_ipv4_5tuple *ipv4 = key_buffer;

		ipv4->ttl = 0;
		ipv4->proto = key_in->key.ipv4_5tuple.proto;
		ipv4->checksum = 0;
		ipv4->ip_src = rte_bswap32(key_in->key.ipv4_5tuple.ip_src);
		ipv4->ip_dst = rte_bswap32(key_in->key.ipv4_5tuple.ip_dst);
		ipv4->port_src = rte_bswap16(key_in->key.ipv4_5tuple.port_src);
		ipv4->port_dst = rte_bswap16(key_in->key.ipv4_5tuple.port_dst);

		if (signature)
			*signature = (uint32_t) hash_default_key16(ipv4, 16, 0);
		return 0;
	}

	case FLOW_KEY_IPV6_5TUPLE:
	{
		struct pkt_key_ipv6_5tuple *ipv6 = key_buffer;

		memset(ipv6, 0, 64);
		ipv6->payload_length = 0;
		ipv6->proto = key_in->key.ipv6_5tuple.proto;
		ipv6->hop_limit = 0;
		memcpy(&ipv6->ip_src, &key_in->key.ipv6_5tuple.ip_src, 16);
		memcpy(&ipv6->ip_dst, &key_in->key.ipv6_5tuple.ip_dst, 16);
		ipv6->port_src = rte_bswap16(key_in->key.ipv6_5tuple.port_src);
		ipv6->port_dst = rte_bswap16(key_in->key.ipv6_5tuple.port_dst);

		if (signature)
			*signature = (uint32_t) hash_default_key64(ipv6, 64, 0);
		return 0;
	}

	default:
		return -1;
	}
}

/*
 * Flow classification pipeline
 */

struct app_pipeline_fc_flow {
	struct pipeline_fc_key key;
	uint32_t port_id;
	uint32_t signature;
	void *entry_ptr;

	TAILQ_ENTRY(app_pipeline_fc_flow) node;
};

#define N_BUCKETS                                65536

struct app_pipeline_fc {
	/* Parameters */
	uint32_t n_ports_in;
	uint32_t n_ports_out;

	/* Flows */
	TAILQ_HEAD(, app_pipeline_fc_flow) flows[N_BUCKETS];
	uint32_t n_flows;

	/* Default flow */
	uint32_t default_flow_present;
	uint32_t default_flow_port_id;
	void *default_flow_entry_ptr;
};

static struct app_pipeline_fc_flow *
app_pipeline_fc_flow_find(struct app_pipeline_fc *p,
	struct pipeline_fc_key *key)
{
	struct app_pipeline_fc_flow *f;
	uint32_t signature, bucket_id;

	app_pipeline_fc_key_convert(key, NULL, &signature);
	bucket_id = signature & (N_BUCKETS - 1);

	TAILQ_FOREACH(f, &p->flows[bucket_id], node)
		if ((signature == f->signature) &&
			(memcmp(key,
				&f->key,
				sizeof(struct pipeline_fc_key)) == 0))
			return f;

	return NULL;
}

static void*
app_pipeline_fc_init(struct pipeline_params *params,
	__rte_unused void *arg)
{
	struct app_pipeline_fc *p;
	uint32_t size, i;

	/* Check input arguments */
	if ((params == NULL) ||
		(params->n_ports_in == 0) ||
		(params->n_ports_out == 0))
		return NULL;

	/* Memory allocation */
	size = RTE_CACHE_LINE_ROUNDUP(sizeof(struct app_pipeline_fc));
	p = rte_zmalloc(NULL, size, RTE_CACHE_LINE_SIZE);
	if (p == NULL)
		return NULL;

	/* Initialization */
	p->n_ports_in = params->n_ports_in;
	p->n_ports_out = params->n_ports_out;

	for (i = 0; i < N_BUCKETS; i++)
		TAILQ_INIT(&p->flows[i]);
	p->n_flows = 0;

	return (void *) p;
}

static int
app_pipeline_fc_free(void *pipeline)
{
	struct app_pipeline_fc *p = pipeline;
	uint32_t i;

	/* Check input arguments */
	if (p == NULL)
		return -1;

	/* Free resources */
	for (i = 0; i < N_BUCKETS; i++)
		while (!TAILQ_EMPTY(&p->flows[i])) {
			struct app_pipeline_fc_flow *flow;

			flow = TAILQ_FIRST(&p->flows[i]);
			TAILQ_REMOVE(&p->flows[i], flow, node);
			rte_free(flow);
		}

	rte_free(p);
	return 0;
}

static int
app_pipeline_fc_key_check(struct pipeline_fc_key *key)
{
	switch (key->type) {
	case FLOW_KEY_QINQ:
	{
		uint16_t svlan = key->key.qinq.svlan;
		uint16_t cvlan = key->key.qinq.cvlan;

		if ((svlan & 0xF000) ||
			(cvlan & 0xF000))
			return -1;

		return 0;
	}

	case FLOW_KEY_IPV4_5TUPLE:
		return 0;

	case FLOW_KEY_IPV6_5TUPLE:
		return 0;

	default:
		return -1;
	}
}

int
app_pipeline_fc_add(struct app_params *app,
	uint32_t pipeline_id,
	struct pipeline_fc_key *key,
	uint32_t port_id)
{
	struct app_pipeline_fc *p;
	struct app_pipeline_fc_flow *flow;

	struct pipeline_fc_add_msg_req *req;
	struct pipeline_fc_add_msg_rsp *rsp;

	uint32_t signature;
	int new_flow;

	/* Check input arguments */
	if ((app == NULL) ||
		(key == NULL))
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id);
	if (p == NULL)
		return -1;

	if (port_id >= p->n_ports_out)
		return -1;

	if (app_pipeline_fc_key_check(key) != 0)
		return -1;

	/* Find existing flow or allocate new flow */
	flow = app_pipeline_fc_flow_find(p, key);
	new_flow = (flow == NULL);
	if (flow == NULL) {
		flow = rte_malloc(NULL, sizeof(*flow), RTE_CACHE_LINE_SIZE);

		if (flow == NULL)
			return -1;
	}

	/* Allocate and write request */
	req = app_msg_alloc(app);
	if (req == NULL)
		return -1;

	req->type = PIPELINE_MSG_REQ_CUSTOM;
	req->subtype = PIPELINE_FC_MSG_REQ_FLOW_ADD;
	app_pipeline_fc_key_convert(key, req->key, &signature);
	req->port_id = port_id;

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL) {
		if (new_flow)
			rte_free(flow);
		return -1;
	}

	/* Read response and write flow */
	if (rsp->status ||
		(rsp->entry_ptr == NULL) ||
		((new_flow == 0) && (rsp->key_found == 0)) ||
		((new_flow == 1) && (rsp->key_found == 1))) {
		app_msg_free(app, rsp);
		if (new_flow)
			rte_free(flow);
		return -1;
	}

	memset(&flow->key, 0, sizeof(flow->key));
	memcpy(&flow->key, key, sizeof(flow->key));
	flow->port_id = port_id;
	flow->signature = signature;
	flow->entry_ptr = rsp->entry_ptr;

	/* Commit rule */
	if (new_flow) {
		uint32_t bucket_id = signature & (N_BUCKETS - 1);

		TAILQ_INSERT_TAIL(&p->flows[bucket_id], flow, node);
		p->n_flows++;
	}

	/* Free response */
	app_msg_free(app, rsp);

	return 0;
}

int
app_pipeline_fc_add_bulk(struct app_params *app,
	uint32_t pipeline_id,
	struct pipeline_fc_key *key,
	uint32_t *port_id,
	uint32_t n_keys)
{
	struct app_pipeline_fc *p;
	struct pipeline_fc_add_bulk_msg_req *req;
	struct pipeline_fc_add_bulk_msg_rsp *rsp;

	struct app_pipeline_fc_flow **flow;
	uint32_t *signature;
	int *new_flow;
	struct pipeline_fc_add_bulk_flow_req *flow_req;
	struct pipeline_fc_add_bulk_flow_rsp *flow_rsp;

	uint32_t i;
	int status;

	/* Check input arguments */
	if ((app == NULL) ||
		(key == NULL) ||
		(port_id == NULL) ||
		(n_keys == 0))
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id);
	if (p == NULL)
		return -1;

	for (i = 0; i < n_keys; i++)
		if (port_id[i] >= p->n_ports_out)
			return -1;

	for (i = 0; i < n_keys; i++)
		if (app_pipeline_fc_key_check(&key[i]) != 0)
			return -1;

	/* Memory allocation */
	flow = rte_malloc(NULL,
		n_keys * sizeof(struct app_pipeline_fc_flow *),
		RTE_CACHE_LINE_SIZE);
	if (flow == NULL)
		return -1;

	signature = rte_malloc(NULL,
		n_keys * sizeof(uint32_t),
		RTE_CACHE_LINE_SIZE);
	if (signature == NULL) {
		rte_free(flow);
		return -1;
	}

	new_flow = rte_malloc(
		NULL,
		n_keys * sizeof(int),
		RTE_CACHE_LINE_SIZE);
	if (new_flow == NULL) {
		rte_free(signature);
		rte_free(flow);
		return -1;
	}

	flow_req = rte_malloc(NULL,
		n_keys * sizeof(struct pipeline_fc_add_bulk_flow_req),
		RTE_CACHE_LINE_SIZE);
	if (flow_req == NULL) {
		rte_free(new_flow);
		rte_free(signature);
		rte_free(flow);
		return -1;
	}

	flow_rsp = rte_malloc(NULL,
		n_keys * sizeof(struct pipeline_fc_add_bulk_flow_rsp),
		RTE_CACHE_LINE_SIZE);
	if (flow_req == NULL) {
		rte_free(flow_req);
		rte_free(new_flow);
		rte_free(signature);
		rte_free(flow);
		return -1;
	}

	/* Find existing flow or allocate new flow */
	for (i = 0; i < n_keys; i++) {
		flow[i] = app_pipeline_fc_flow_find(p, &key[i]);
		new_flow[i] = (flow[i] == NULL);
		if (flow[i] == NULL) {
			flow[i] = rte_zmalloc(NULL,
				sizeof(struct app_pipeline_fc_flow),
				RTE_CACHE_LINE_SIZE);

			if (flow[i] == NULL) {
				uint32_t j;

				for (j = 0; j < i; j++)
					if (new_flow[j])
						rte_free(flow[j]);

				rte_free(flow_rsp);
				rte_free(flow_req);
				rte_free(new_flow);
				rte_free(signature);
				rte_free(flow);
				return -1;
			}
		}
	}

	/* Allocate and write request */
	req = app_msg_alloc(app);
	if (req == NULL) {
		for (i = 0; i < n_keys; i++)
			if (new_flow[i])
				rte_free(flow[i]);

		rte_free(flow_rsp);
		rte_free(flow_req);
		rte_free(new_flow);
		rte_free(signature);
		rte_free(flow);
		return -1;
	}

	for (i = 0; i < n_keys; i++) {
		app_pipeline_fc_key_convert(&key[i],
			flow_req[i].key,
			&signature[i]);
		flow_req[i].port_id = port_id[i];
	}

	req->type = PIPELINE_MSG_REQ_CUSTOM;
	req->subtype = PIPELINE_FC_MSG_REQ_FLOW_ADD_BULK;
	req->req = flow_req;
	req->rsp = flow_rsp;
	req->n_keys = n_keys;

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, 10000);
	if (rsp == NULL) {
		for (i = 0; i < n_keys; i++)
			if (new_flow[i])
				rte_free(flow[i]);

		rte_free(flow_rsp);
		rte_free(flow_req);
		rte_free(new_flow);
		rte_free(signature);
		rte_free(flow);
		return -1;
	}

	/* Read response */
	status = 0;

	for (i = 0; i < rsp->n_keys; i++)
		if ((flow_rsp[i].entry_ptr == NULL) ||
			((new_flow[i] == 0) && (flow_rsp[i].key_found == 0)) ||
			((new_flow[i] == 1) && (flow_rsp[i].key_found == 1)))
			status = -1;

	if (rsp->n_keys < n_keys)
		status = -1;

	/* Commit flows */
	for (i = 0; i < rsp->n_keys; i++) {
		memcpy(&flow[i]->key, &key[i], sizeof(flow[i]->key));
		flow[i]->port_id = port_id[i];
		flow[i]->signature = signature[i];
		flow[i]->entry_ptr = flow_rsp[i].entry_ptr;

		if (new_flow[i]) {
			uint32_t bucket_id = signature[i] & (N_BUCKETS - 1);

			TAILQ_INSERT_TAIL(&p->flows[bucket_id], flow[i], node);
			p->n_flows++;
		}
	}

	/* Free resources */
	app_msg_free(app, rsp);

	for (i = rsp->n_keys; i < n_keys; i++)
		if (new_flow[i])
			rte_free(flow[i]);

	rte_free(flow_rsp);
	rte_free(flow_req);
	rte_free(new_flow);
	rte_free(signature);
	rte_free(flow);

	return status;
}

int
app_pipeline_fc_del(struct app_params *app,
	uint32_t pipeline_id,
	struct pipeline_fc_key *key)
{
	struct app_pipeline_fc *p;
	struct app_pipeline_fc_flow *flow;

	struct pipeline_fc_del_msg_req *req;
	struct pipeline_fc_del_msg_rsp *rsp;

	uint32_t signature, bucket_id;

	/* Check input arguments */
	if ((app == NULL) ||
		(key == NULL))
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id);
	if (p == NULL)
		return -1;

	if (app_pipeline_fc_key_check(key) != 0)
		return -1;

	/* Find rule */
	flow = app_pipeline_fc_flow_find(p, key);
	if (flow == NULL)
		return 0;

	/* Allocate and write request */
	req = app_msg_alloc(app);
	if (req == NULL)
		return -1;

	req->type = PIPELINE_MSG_REQ_CUSTOM;
	req->subtype = PIPELINE_FC_MSG_REQ_FLOW_DEL;
	app_pipeline_fc_key_convert(key, req->key, &signature);

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -1;

	/* Read response */
	if (rsp->status || !rsp->key_found) {
		app_msg_free(app, rsp);
		return -1;
	}

	/* Remove rule */
	bucket_id = signature & (N_BUCKETS - 1);
	TAILQ_REMOVE(&p->flows[bucket_id], flow, node);
	p->n_flows--;
	rte_free(flow);

	/* Free response */
	app_msg_free(app, rsp);

	return 0;
}

int
app_pipeline_fc_add_default(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t port_id)
{
	struct app_pipeline_fc *p;

	struct pipeline_fc_add_default_msg_req *req;
	struct pipeline_fc_add_default_msg_rsp *rsp;

	/* Check input arguments */
	if (app == NULL)
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id);
	if (p == NULL)
		return -1;

	if (port_id >= p->n_ports_out)
		return -1;

	/* Allocate and write request */
	req = app_msg_alloc(app);
	if (req == NULL)
		return -1;

	req->type = PIPELINE_MSG_REQ_CUSTOM;
	req->subtype = PIPELINE_FC_MSG_REQ_FLOW_ADD_DEFAULT;
	req->port_id = port_id;

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -1;

	/* Read response and write flow */
	if (rsp->status || (rsp->entry_ptr == NULL)) {
		app_msg_free(app, rsp);
		return -1;
	}

	p->default_flow_port_id = port_id;
	p->default_flow_entry_ptr = rsp->entry_ptr;

	/* Commit route */
	p->default_flow_present = 1;

	/* Free response */
	app_msg_free(app, rsp);

	return 0;
}

int
app_pipeline_fc_del_default(struct app_params *app,
	uint32_t pipeline_id)
{
	struct app_pipeline_fc *p;

	struct pipeline_fc_del_default_msg_req *req;
	struct pipeline_fc_del_default_msg_rsp *rsp;

	/* Check input arguments */
	if (app == NULL)
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id);
	if (p == NULL)
		return -EINVAL;

	/* Allocate and write request */
	req = app_msg_alloc(app);
	if (req == NULL)
		return -1;

	req->type = PIPELINE_MSG_REQ_CUSTOM;
	req->subtype = PIPELINE_FC_MSG_REQ_FLOW_DEL_DEFAULT;

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -1;

	/* Read response */
	if (rsp->status) {
		app_msg_free(app, rsp);
		return -1;
	}

	/* Commit route */
	p->default_flow_present = 0;

	/* Free response */
	app_msg_free(app, rsp);

	return 0;
}

/*
 * Flow ls
 */

static void
print_fc_qinq_flow(struct app_pipeline_fc_flow *flow)
{
	printf("(SVLAN = %" PRIu32 ", "
		"CVLAN = %" PRIu32 ") => "
		"Port = %" PRIu32 " "
		"(signature = 0x%08" PRIx32 ", "
		"entry_ptr = %p)\n",

		flow->key.key.qinq.svlan,
		flow->key.key.qinq.cvlan,
		flow->port_id,
		flow->signature,
		flow->entry_ptr);
}

static void
print_fc_ipv4_5tuple_flow(struct app_pipeline_fc_flow *flow)
{
	printf("(SA = %" PRIu32 ".%" PRIu32 ".%" PRIu32 ".%" PRIu32 ", "
		   "DA = %" PRIu32 ".%" PRIu32 ".%" PRIu32 ".%" PRIu32 ", "
		   "SP = %" PRIu32 ", "
		   "DP = %" PRIu32 ", "
		   "Proto = %" PRIu32 ") => "
		   "Port = %" PRIu32 " "
		   "(signature = 0x%08" PRIx32 ", "
		   "entry_ptr = %p)\n",

		   (flow->key.key.ipv4_5tuple.ip_src >> 24) & 0xFF,
		   (flow->key.key.ipv4_5tuple.ip_src >> 16) & 0xFF,
		   (flow->key.key.ipv4_5tuple.ip_src >> 8) & 0xFF,
		   flow->key.key.ipv4_5tuple.ip_src & 0xFF,

		   (flow->key.key.ipv4_5tuple.ip_dst >> 24) & 0xFF,
		   (flow->key.key.ipv4_5tuple.ip_dst >> 16) & 0xFF,
		   (flow->key.key.ipv4_5tuple.ip_dst >> 8) & 0xFF,
		   flow->key.key.ipv4_5tuple.ip_dst & 0xFF,

		   flow->key.key.ipv4_5tuple.port_src,
		   flow->key.key.ipv4_5tuple.port_dst,

		   flow->key.key.ipv4_5tuple.proto,

		   flow->port_id,
		   flow->signature,
		   flow->entry_ptr);
}

static void
print_fc_ipv6_5tuple_flow(struct app_pipeline_fc_flow *flow) {
	printf("(SA = %02" PRIx32 "%02" PRIx32 ":%02" PRIx32 "%02" PRIx32
		":%02" PRIx32 "%02" PRIx32 ":%02" PRIx32 "%02" PRIx32
		":%02" PRIx32 "%02" PRIx32 ":%02" PRIx32 "%02" PRIx32
		":%02" PRIx32 "%02" PRIx32 ":%02" PRIx32 "%02" PRIx32 ", "
		"DA = %02" PRIx32 "%02" PRIx32 ":%02" PRIx32 "%02" PRIx32
		":%02" PRIx32 "%02" PRIx32 ":%02" PRIx32 "%02" PRIx32
		":%02" PRIx32 "%02" PRIx32 ":%02" PRIx32 "%02" PRIx32
		":%02" PRIx32 "%02" PRIx32 ":%02" PRIx32 "%02" PRIx32 ", "
		"SP = %" PRIu32 ", "
		"DP = %" PRIu32 " "
		"Proto = %" PRIu32 " "
		"=> Port = %" PRIu32 " "
		"(signature = 0x%08" PRIx32 ", "
		"entry_ptr = %p)\n",

		flow->key.key.ipv6_5tuple.ip_src[0],
		flow->key.key.ipv6_5tuple.ip_src[1],
		flow->key.key.ipv6_5tuple.ip_src[2],
		flow->key.key.ipv6_5tuple.ip_src[3],
		flow->key.key.ipv6_5tuple.ip_src[4],
		flow->key.key.ipv6_5tuple.ip_src[5],
		flow->key.key.ipv6_5tuple.ip_src[6],
		flow->key.key.ipv6_5tuple.ip_src[7],
		flow->key.key.ipv6_5tuple.ip_src[8],
		flow->key.key.ipv6_5tuple.ip_src[9],
		flow->key.key.ipv6_5tuple.ip_src[10],
		flow->key.key.ipv6_5tuple.ip_src[11],
		flow->key.key.ipv6_5tuple.ip_src[12],
		flow->key.key.ipv6_5tuple.ip_src[13],
		flow->key.key.ipv6_5tuple.ip_src[14],
		flow->key.key.ipv6_5tuple.ip_src[15],

		flow->key.key.ipv6_5tuple.ip_dst[0],
		flow->key.key.ipv6_5tuple.ip_dst[1],
		flow->key.key.ipv6_5tuple.ip_dst[2],
		flow->key.key.ipv6_5tuple.ip_dst[3],
		flow->key.key.ipv6_5tuple.ip_dst[4],
		flow->key.key.ipv6_5tuple.ip_dst[5],
		flow->key.key.ipv6_5tuple.ip_dst[6],
		flow->key.key.ipv6_5tuple.ip_dst[7],
		flow->key.key.ipv6_5tuple.ip_dst[8],
		flow->key.key.ipv6_5tuple.ip_dst[9],
		flow->key.key.ipv6_5tuple.ip_dst[10],
		flow->key.key.ipv6_5tuple.ip_dst[11],
		flow->key.key.ipv6_5tuple.ip_dst[12],
		flow->key.key.ipv6_5tuple.ip_dst[13],
		flow->key.key.ipv6_5tuple.ip_dst[14],
		flow->key.key.ipv6_5tuple.ip_dst[15],

		flow->key.key.ipv6_5tuple.port_src,
		flow->key.key.ipv6_5tuple.port_dst,

		flow->key.key.ipv6_5tuple.proto,

		flow->port_id,
		flow->signature,
		flow->entry_ptr);
}

static void
print_fc_flow(struct app_pipeline_fc_flow *flow)
{
	switch (flow->key.type) {
	case FLOW_KEY_QINQ:
		print_fc_qinq_flow(flow);
		break;

	case FLOW_KEY_IPV4_5TUPLE:
		print_fc_ipv4_5tuple_flow(flow);
		break;

	case FLOW_KEY_IPV6_5TUPLE:
		print_fc_ipv6_5tuple_flow(flow);
		break;
	}
}

static int
app_pipeline_fc_ls(struct app_params *app,
		uint32_t pipeline_id)
{
	struct app_pipeline_fc *p;
	struct app_pipeline_fc_flow *flow;
	uint32_t i;

	/* Check input arguments */
	if (app == NULL)
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id);
	if (p == NULL)
		return -1;

	for (i = 0; i < N_BUCKETS; i++)
		TAILQ_FOREACH(flow, &p->flows[i], node)
			print_fc_flow(flow);

	if (p->default_flow_present)
		printf("Default flow: port %" PRIu32 " (entry ptr = %p)\n",
			p->default_flow_port_id,
			p->default_flow_entry_ptr);
	else
		printf("Default: DROP\n");

	return 0;
}

/*
 * flow add qinq
 */

struct cmd_fc_add_qinq_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t flow_string;
	cmdline_fixed_string_t add_string;
	cmdline_fixed_string_t qinq_string;
	uint16_t svlan;
	uint16_t cvlan;
	uint32_t port;
};

static void
cmd_fc_add_qinq_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_fc_add_qinq_result *params = parsed_result;
	struct app_params *app = data;
	struct pipeline_fc_key key;
	int status;

	memset(&key, 0, sizeof(key));
	key.type = FLOW_KEY_QINQ;
	key.key.qinq.svlan = params->svlan;
	key.key.qinq.cvlan = params->cvlan;

	status = app_pipeline_fc_add(app,
		params->pipeline_id,
		&key,
		params->port);
	if (status != 0)
		printf("Command failed\n");
}

cmdline_parse_token_string_t cmd_fc_add_qinq_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_qinq_result, p_string, "p");

cmdline_parse_token_num_t cmd_fc_add_qinq_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_qinq_result, pipeline_id,
		UINT32);

cmdline_parse_token_string_t cmd_fc_add_qinq_flow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_qinq_result, flow_string,
		"flow");

cmdline_parse_token_string_t cmd_fc_add_qinq_add_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_qinq_result, add_string,
		"add");

cmdline_parse_token_string_t cmd_fc_add_qinq_qinq_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_qinq_result, qinq_string,
		"qinq");

cmdline_parse_token_num_t cmd_fc_add_qinq_svlan =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_qinq_result, svlan, UINT16);

cmdline_parse_token_num_t cmd_fc_add_qinq_cvlan =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_qinq_result, cvlan, UINT16);

cmdline_parse_token_num_t cmd_fc_add_qinq_port =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_qinq_result, port, UINT32);

cmdline_parse_inst_t cmd_fc_add_qinq = {
	.f = cmd_fc_add_qinq_parsed,
	.data = NULL,
	.help_str = "Flow add (Q-in-Q)",
	.tokens = {
		(void *) &cmd_fc_add_qinq_p_string,
		(void *) &cmd_fc_add_qinq_pipeline_id,
		(void *) &cmd_fc_add_qinq_flow_string,
		(void *) &cmd_fc_add_qinq_add_string,
		(void *) &cmd_fc_add_qinq_qinq_string,
		(void *) &cmd_fc_add_qinq_svlan,
		(void *) &cmd_fc_add_qinq_cvlan,
		(void *) &cmd_fc_add_qinq_port,
		NULL,
	},
};

/*
 * flow add qinq all
 */

struct cmd_fc_add_qinq_all_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t flow_string;
	cmdline_fixed_string_t add_string;
	cmdline_fixed_string_t qinq_string;
	cmdline_fixed_string_t all_string;
	uint32_t n_flows;
	uint32_t n_ports;
};

#ifndef N_FLOWS_BULK
#define N_FLOWS_BULK					4096
#endif

static void
cmd_fc_add_qinq_all_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_fc_add_qinq_all_result *params = parsed_result;
	struct app_params *app = data;
	struct pipeline_fc_key *key;
	uint32_t *port_id;
	uint32_t flow_id;

	key = rte_zmalloc(NULL,
		N_FLOWS_BULK * sizeof(*key),
		RTE_CACHE_LINE_SIZE);
	if (key == NULL) {
		printf("Memory allocation failed\n");
		return;
	}

	port_id = rte_malloc(NULL,
		N_FLOWS_BULK * sizeof(*port_id),
		RTE_CACHE_LINE_SIZE);
	if (port_id == NULL) {
		rte_free(key);
		printf("Memory allocation failed\n");
		return;
	}

	for (flow_id = 0; flow_id < params->n_flows; flow_id++) {
		uint32_t pos = flow_id & (N_FLOWS_BULK - 1);

		key[pos].type = FLOW_KEY_QINQ;
		key[pos].key.qinq.svlan = flow_id >> 12;
		key[pos].key.qinq.cvlan = flow_id & 0xFFF;

		port_id[pos] = flow_id % params->n_ports;

		if ((pos == N_FLOWS_BULK - 1) ||
			(flow_id == params->n_flows - 1)) {
			int status;

			status = app_pipeline_fc_add_bulk(app,
				params->pipeline_id,
				key,
				port_id,
				pos + 1);

			if (status != 0) {
				printf("Command failed\n");

				break;
			}
		}
	}

	rte_free(port_id);
	rte_free(key);
}

cmdline_parse_token_string_t cmd_fc_add_qinq_all_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_qinq_all_result, p_string,
		"p");

cmdline_parse_token_num_t cmd_fc_add_qinq_all_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_qinq_all_result, pipeline_id,
		UINT32);

cmdline_parse_token_string_t cmd_fc_add_qinq_all_flow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_qinq_all_result, flow_string,
		"flow");

cmdline_parse_token_string_t cmd_fc_add_qinq_all_add_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_qinq_all_result, add_string,
		"add");

cmdline_parse_token_string_t cmd_fc_add_qinq_all_qinq_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_qinq_all_result, qinq_string,
		"qinq");

cmdline_parse_token_string_t cmd_fc_add_qinq_all_all_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_qinq_all_result, all_string,
		"all");

cmdline_parse_token_num_t cmd_fc_add_qinq_all_n_flows =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_qinq_all_result, n_flows,
		UINT32);

cmdline_parse_token_num_t cmd_fc_add_qinq_all_n_ports =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_qinq_all_result, n_ports,
		UINT32);

cmdline_parse_inst_t cmd_fc_add_qinq_all = {
	.f = cmd_fc_add_qinq_all_parsed,
	.data = NULL,
	.help_str = "Flow add all (Q-in-Q)",
	.tokens = {
		(void *) &cmd_fc_add_qinq_all_p_string,
		(void *) &cmd_fc_add_qinq_all_pipeline_id,
		(void *) &cmd_fc_add_qinq_all_flow_string,
		(void *) &cmd_fc_add_qinq_all_add_string,
		(void *) &cmd_fc_add_qinq_all_qinq_string,
		(void *) &cmd_fc_add_qinq_all_all_string,
		(void *) &cmd_fc_add_qinq_all_n_flows,
		(void *) &cmd_fc_add_qinq_all_n_ports,
		NULL,
	},
};

/*
 * flow add ipv4_5tuple
 */

struct cmd_fc_add_ipv4_5tuple_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t flow_string;
	cmdline_fixed_string_t add_string;
	cmdline_fixed_string_t ipv4_5tuple_string;
	cmdline_ipaddr_t ip_src;
	cmdline_ipaddr_t ip_dst;
	uint16_t port_src;
	uint16_t port_dst;
	uint32_t proto;
	uint32_t port;
};

static void
cmd_fc_add_ipv4_5tuple_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_fc_add_ipv4_5tuple_result *params = parsed_result;
	struct app_params *app = data;
	struct pipeline_fc_key key;
	int status;

	memset(&key, 0, sizeof(key));
	key.type = FLOW_KEY_IPV4_5TUPLE;
	key.key.ipv4_5tuple.ip_src = rte_bswap32(
		params->ip_src.addr.ipv4.s_addr);
	key.key.ipv4_5tuple.ip_dst = rte_bswap32(
		params->ip_dst.addr.ipv4.s_addr);
	key.key.ipv4_5tuple.port_src = params->port_src;
	key.key.ipv4_5tuple.port_dst = params->port_dst;
	key.key.ipv4_5tuple.proto = params->proto;

	status = app_pipeline_fc_add(app,
		params->pipeline_id,
		&key,
		params->port);
	if (status != 0)
		printf("Command failed\n");
}

cmdline_parse_token_string_t cmd_fc_add_ipv4_5tuple_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_ipv4_5tuple_result, p_string,
		"p");

cmdline_parse_token_num_t cmd_fc_add_ipv4_5tuple_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_ipv4_5tuple_result, pipeline_id,
		UINT32);

cmdline_parse_token_string_t cmd_fc_add_ipv4_5tuple_flow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_ipv4_5tuple_result,
		flow_string, "flow");

cmdline_parse_token_string_t cmd_fc_add_ipv4_5tuple_add_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_ipv4_5tuple_result,
		add_string, "add");

cmdline_parse_token_string_t cmd_fc_add_ipv4_5tuple_ipv4_5tuple_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_ipv4_5tuple_result,
		ipv4_5tuple_string, "ipv4_5tuple");

cmdline_parse_token_ipaddr_t cmd_fc_add_ipv4_5tuple_ip_src =
	TOKEN_IPV4_INITIALIZER(struct cmd_fc_add_ipv4_5tuple_result, ip_src);

cmdline_parse_token_ipaddr_t cmd_fc_add_ipv4_5tuple_ip_dst =
	TOKEN_IPV4_INITIALIZER(struct cmd_fc_add_ipv4_5tuple_result, ip_dst);

cmdline_parse_token_num_t cmd_fc_add_ipv4_5tuple_port_src =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_ipv4_5tuple_result, port_src,
		UINT16);

cmdline_parse_token_num_t cmd_fc_add_ipv4_5tuple_port_dst =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_ipv4_5tuple_result, port_dst,
		UINT16);

cmdline_parse_token_num_t cmd_fc_add_ipv4_5tuple_proto =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_ipv4_5tuple_result, proto,
		UINT32);

cmdline_parse_token_num_t cmd_fc_add_ipv4_5tuple_port =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_ipv4_5tuple_result, port,
		UINT32);

cmdline_parse_inst_t cmd_fc_add_ipv4_5tuple = {
	.f = cmd_fc_add_ipv4_5tuple_parsed,
	.data = NULL,
	.help_str = "Flow add (IPv4 5-tuple)",
	.tokens = {
		(void *) &cmd_fc_add_ipv4_5tuple_p_string,
		(void *) &cmd_fc_add_ipv4_5tuple_pipeline_id,
		(void *) &cmd_fc_add_ipv4_5tuple_flow_string,
		(void *) &cmd_fc_add_ipv4_5tuple_add_string,
		(void *) &cmd_fc_add_ipv4_5tuple_ipv4_5tuple_string,
		(void *) &cmd_fc_add_ipv4_5tuple_ip_src,
		(void *) &cmd_fc_add_ipv4_5tuple_ip_dst,
		(void *) &cmd_fc_add_ipv4_5tuple_port_src,
		(void *) &cmd_fc_add_ipv4_5tuple_port_dst,
		(void *) &cmd_fc_add_ipv4_5tuple_proto,
		(void *) &cmd_fc_add_ipv4_5tuple_port,
		NULL,
	},
};

/*
 * flow add ipv4_5tuple all
 */

struct cmd_fc_add_ipv4_5tuple_all_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t flow_string;
	cmdline_fixed_string_t add_string;
	cmdline_fixed_string_t ipv4_5tuple_string;
	cmdline_fixed_string_t all_string;
	uint32_t n_flows;
	uint32_t n_ports;
};

static void
cmd_fc_add_ipv4_5tuple_all_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_fc_add_ipv4_5tuple_all_result *params = parsed_result;
	struct app_params *app = data;
	struct pipeline_fc_key *key;
	uint32_t *port_id;
	uint32_t flow_id;

	key = rte_zmalloc(NULL,
		N_FLOWS_BULK * sizeof(*key),
		RTE_CACHE_LINE_SIZE);
	if (key == NULL) {
		printf("Memory allocation failed\n");
		return;
	}

	port_id = rte_malloc(NULL,
		N_FLOWS_BULK * sizeof(*port_id),
		RTE_CACHE_LINE_SIZE);
	if (port_id == NULL) {
		rte_free(key);
		printf("Memory allocation failed\n");
		return;
	}

	for (flow_id = 0; flow_id < params->n_flows; flow_id++) {
		uint32_t pos = flow_id & (N_FLOWS_BULK - 1);

		key[pos].type = FLOW_KEY_IPV4_5TUPLE;
		key[pos].key.ipv4_5tuple.ip_src = 0;
		key[pos].key.ipv4_5tuple.ip_dst = flow_id;
		key[pos].key.ipv4_5tuple.port_src = 0;
		key[pos].key.ipv4_5tuple.port_dst = 0;
		key[pos].key.ipv4_5tuple.proto = 6;

		port_id[pos] = flow_id % params->n_ports;

		if ((pos == N_FLOWS_BULK - 1) ||
			(flow_id == params->n_flows - 1)) {
			int status;

			status = app_pipeline_fc_add_bulk(app,
				params->pipeline_id,
				key,
				port_id,
				pos + 1);

			if (status != 0) {
				printf("Command failed\n");

				break;
			}
		}
	}

	rte_free(port_id);
	rte_free(key);
}

cmdline_parse_token_string_t cmd_fc_add_ipv4_5tuple_all_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_ipv4_5tuple_all_result,
		p_string, "p");

cmdline_parse_token_num_t cmd_fc_add_ipv4_5tuple_all_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_ipv4_5tuple_all_result,
		pipeline_id, UINT32);

cmdline_parse_token_string_t cmd_fc_add_ipv4_5tuple_all_flow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_ipv4_5tuple_all_result,
		flow_string, "flow");

cmdline_parse_token_string_t cmd_fc_add_ipv4_5tuple_all_add_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_ipv4_5tuple_all_result,
		add_string, "add");

cmdline_parse_token_string_t cmd_fc_add_ipv4_5tuple_all_ipv4_5tuple_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_ipv4_5tuple_all_result,
		ipv4_5tuple_string, "ipv4_5tuple");

cmdline_parse_token_string_t cmd_fc_add_ipv4_5tuple_all_all_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_ipv4_5tuple_all_result,
		all_string, "all");

cmdline_parse_token_num_t cmd_fc_add_ipv4_5tuple_all_n_flows =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_ipv4_5tuple_all_result,
		n_flows, UINT32);

cmdline_parse_token_num_t cmd_fc_add_ipv4_5tuple_all_n_ports =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_ipv4_5tuple_all_result,
		n_ports, UINT32);

cmdline_parse_inst_t cmd_fc_add_ipv4_5tuple_all = {
	.f = cmd_fc_add_ipv4_5tuple_all_parsed,
	.data = NULL,
	.help_str = "Flow add all (IPv4 5-tuple)",
	.tokens = {
		(void *) &cmd_fc_add_ipv4_5tuple_all_p_string,
		(void *) &cmd_fc_add_ipv4_5tuple_all_pipeline_id,
		(void *) &cmd_fc_add_ipv4_5tuple_all_flow_string,
		(void *) &cmd_fc_add_ipv4_5tuple_all_add_string,
		(void *) &cmd_fc_add_ipv4_5tuple_all_ipv4_5tuple_string,
		(void *) &cmd_fc_add_ipv4_5tuple_all_all_string,
		(void *) &cmd_fc_add_ipv4_5tuple_all_n_flows,
		(void *) &cmd_fc_add_ipv4_5tuple_all_n_ports,
		NULL,
	},
};

/*
 * flow add ipv6_5tuple
 */

struct cmd_fc_add_ipv6_5tuple_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t flow_string;
	cmdline_fixed_string_t add_string;
	cmdline_fixed_string_t ipv6_5tuple_string;
	cmdline_ipaddr_t ip_src;
	cmdline_ipaddr_t ip_dst;
	uint16_t port_src;
	uint16_t port_dst;
	uint32_t proto;
	uint32_t port;
};

static void
cmd_fc_add_ipv6_5tuple_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_fc_add_ipv6_5tuple_result *params = parsed_result;
	struct app_params *app = data;
	struct pipeline_fc_key key;
	int status;

	memset(&key, 0, sizeof(key));
	key.type = FLOW_KEY_IPV6_5TUPLE;
	memcpy(key.key.ipv6_5tuple.ip_src,
		params->ip_src.addr.ipv6.s6_addr,
		16);
	memcpy(key.key.ipv6_5tuple.ip_dst,
		params->ip_dst.addr.ipv6.s6_addr,
		16);
	key.key.ipv6_5tuple.port_src = params->port_src;
	key.key.ipv6_5tuple.port_dst = params->port_dst;
	key.key.ipv6_5tuple.proto = params->proto;

	status = app_pipeline_fc_add(app,
		params->pipeline_id,
		&key,
		params->port);
	if (status != 0)
		printf("Command failed\n");
}

cmdline_parse_token_string_t cmd_fc_add_ipv6_5tuple_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_ipv6_5tuple_result,
		p_string, "p");

cmdline_parse_token_num_t cmd_fc_add_ipv6_5tuple_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_ipv6_5tuple_result, pipeline_id,
		UINT32);

cmdline_parse_token_string_t cmd_fc_add_ipv6_5tuple_flow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_ipv6_5tuple_result,
		flow_string, "flow");

cmdline_parse_token_string_t cmd_fc_add_ipv6_5tuple_add_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_ipv6_5tuple_result,
		add_string, "add");

cmdline_parse_token_string_t cmd_fc_add_ipv6_5tuple_ipv6_5tuple_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_ipv6_5tuple_result,
		ipv6_5tuple_string, "ipv6_5tuple");

cmdline_parse_token_ipaddr_t cmd_fc_add_ipv6_5tuple_ip_src =
	TOKEN_IPV6_INITIALIZER(struct cmd_fc_add_ipv6_5tuple_result, ip_src);

cmdline_parse_token_ipaddr_t cmd_fc_add_ipv6_5tuple_ip_dst =
	TOKEN_IPV6_INITIALIZER(struct cmd_fc_add_ipv6_5tuple_result, ip_dst);

cmdline_parse_token_num_t cmd_fc_add_ipv6_5tuple_port_src =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_ipv6_5tuple_result, port_src,
		UINT16);

cmdline_parse_token_num_t cmd_fc_add_ipv6_5tuple_port_dst =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_ipv6_5tuple_result, port_dst,
		UINT16);

cmdline_parse_token_num_t cmd_fc_add_ipv6_5tuple_proto =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_ipv6_5tuple_result, proto,
		UINT32);

cmdline_parse_token_num_t cmd_fc_add_ipv6_5tuple_port =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_ipv6_5tuple_result, port,
		UINT32);

cmdline_parse_inst_t cmd_fc_add_ipv6_5tuple = {
	.f = cmd_fc_add_ipv6_5tuple_parsed,
	.data = NULL,
	.help_str = "Flow add (IPv6 5-tuple)",
	.tokens = {
		(void *) &cmd_fc_add_ipv6_5tuple_p_string,
		(void *) &cmd_fc_add_ipv6_5tuple_pipeline_id,
		(void *) &cmd_fc_add_ipv6_5tuple_flow_string,
		(void *) &cmd_fc_add_ipv6_5tuple_add_string,
		(void *) &cmd_fc_add_ipv6_5tuple_ipv6_5tuple_string,
		(void *) &cmd_fc_add_ipv6_5tuple_ip_src,
		(void *) &cmd_fc_add_ipv6_5tuple_ip_dst,
		(void *) &cmd_fc_add_ipv6_5tuple_port_src,
		(void *) &cmd_fc_add_ipv6_5tuple_port_dst,
		(void *) &cmd_fc_add_ipv6_5tuple_proto,
		(void *) &cmd_fc_add_ipv6_5tuple_port,
		NULL,
	},
};

/*
 * flow add ipv6_5tuple all
 */

struct cmd_fc_add_ipv6_5tuple_all_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t flow_string;
	cmdline_fixed_string_t add_string;
	cmdline_fixed_string_t ipv6_5tuple_string;
	cmdline_fixed_string_t all_string;
	uint32_t n_flows;
	uint32_t n_ports;
};

static void
cmd_fc_add_ipv6_5tuple_all_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_fc_add_ipv6_5tuple_all_result *params = parsed_result;
	struct app_params *app = data;
	struct pipeline_fc_key *key;
	uint32_t *port_id;
	uint32_t flow_id;

	key = rte_zmalloc(NULL,
		N_FLOWS_BULK * sizeof(*key),
		RTE_CACHE_LINE_SIZE);
	if (key == NULL) {
		printf("Memory allocation failed\n");
		return;
	}

	port_id = rte_malloc(NULL,
		N_FLOWS_BULK * sizeof(*port_id),
		RTE_CACHE_LINE_SIZE);
	if (port_id == NULL) {
		rte_free(key);
		printf("Memory allocation failed\n");
		return;
	}

	for (flow_id = 0; flow_id < params->n_flows; flow_id++) {
		uint32_t pos = flow_id & (N_FLOWS_BULK - 1);
		uint32_t *x;

		key[pos].type = FLOW_KEY_IPV6_5TUPLE;
		x = (uint32_t *) key[pos].key.ipv6_5tuple.ip_dst;
		*x = rte_bswap32(flow_id);
		key[pos].key.ipv6_5tuple.proto = 6;

		port_id[pos] = flow_id % params->n_ports;

		if ((pos == N_FLOWS_BULK - 1) ||
			(flow_id == params->n_flows - 1)) {
			int status;

			status = app_pipeline_fc_add_bulk(app,
				params->pipeline_id,
				key,
				port_id,
				pos + 1);

			if (status != 0) {
				printf("Command failed\n");

				break;
			}
		}
	}

	rte_free(port_id);
	rte_free(key);
}

cmdline_parse_token_string_t cmd_fc_add_ipv6_5tuple_all_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_ipv6_5tuple_all_result,
		p_string, "p");

cmdline_parse_token_num_t cmd_fc_add_ipv6_5tuple_all_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_ipv6_5tuple_all_result,
		pipeline_id, UINT32);

cmdline_parse_token_string_t cmd_fc_add_ipv6_5tuple_all_flow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_ipv6_5tuple_all_result,
		flow_string, "flow");

cmdline_parse_token_string_t cmd_fc_add_ipv6_5tuple_all_add_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_ipv6_5tuple_all_result,
		add_string, "add");

cmdline_parse_token_string_t cmd_fc_add_ipv6_5tuple_all_ipv6_5tuple_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_ipv6_5tuple_all_result,
		ipv6_5tuple_string, "ipv6_5tuple");

cmdline_parse_token_string_t cmd_fc_add_ipv6_5tuple_all_all_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_ipv6_5tuple_all_result,
		all_string, "all");

cmdline_parse_token_num_t cmd_fc_add_ipv6_5tuple_all_n_flows =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_ipv6_5tuple_all_result,
		n_flows, UINT32);

cmdline_parse_token_num_t cmd_fc_add_ipv6_5tuple_all_n_ports =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_ipv6_5tuple_all_result,
		n_ports, UINT32);

cmdline_parse_inst_t cmd_fc_add_ipv6_5tuple_all = {
	.f = cmd_fc_add_ipv6_5tuple_all_parsed,
	.data = NULL,
	.help_str = "Flow add all (ipv6 5-tuple)",
	.tokens = {
		(void *) &cmd_fc_add_ipv6_5tuple_all_p_string,
		(void *) &cmd_fc_add_ipv6_5tuple_all_pipeline_id,
		(void *) &cmd_fc_add_ipv6_5tuple_all_flow_string,
		(void *) &cmd_fc_add_ipv6_5tuple_all_add_string,
		(void *) &cmd_fc_add_ipv6_5tuple_all_ipv6_5tuple_string,
		(void *) &cmd_fc_add_ipv6_5tuple_all_all_string,
		(void *) &cmd_fc_add_ipv6_5tuple_all_n_flows,
		(void *) &cmd_fc_add_ipv6_5tuple_all_n_ports,
		NULL,
	},
};

/*
 * flow del qinq
 */
struct cmd_fc_del_qinq_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t flow_string;
	cmdline_fixed_string_t del_string;
	cmdline_fixed_string_t qinq_string;
	uint16_t svlan;
	uint16_t cvlan;
};

static void
cmd_fc_del_qinq_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_fc_del_qinq_result *params = parsed_result;
	struct app_params *app = data;
	struct pipeline_fc_key key;
	int status;

	memset(&key, 0, sizeof(key));
	key.type = FLOW_KEY_QINQ;
	key.key.qinq.svlan = params->svlan;
	key.key.qinq.cvlan = params->cvlan;
	status = app_pipeline_fc_del(app, params->pipeline_id, &key);

	if (status != 0)
		printf("Command failed\n");
}

cmdline_parse_token_string_t cmd_fc_del_qinq_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_del_qinq_result, p_string, "p");

cmdline_parse_token_num_t cmd_fc_del_qinq_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_del_qinq_result, pipeline_id,
		UINT32);

cmdline_parse_token_string_t cmd_fc_del_qinq_flow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_del_qinq_result, flow_string,
		"flow");

cmdline_parse_token_string_t cmd_fc_del_qinq_del_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_del_qinq_result, del_string,
		"del");

cmdline_parse_token_string_t cmd_fc_del_qinq_qinq_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_del_qinq_result, qinq_string,
		"qinq");

cmdline_parse_token_num_t cmd_fc_del_qinq_svlan =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_del_qinq_result, svlan, UINT16);

cmdline_parse_token_num_t cmd_fc_del_qinq_cvlan =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_del_qinq_result, cvlan, UINT16);

cmdline_parse_inst_t cmd_fc_del_qinq = {
	.f = cmd_fc_del_qinq_parsed,
	.data = NULL,
	.help_str = "Flow delete (Q-in-Q)",
	.tokens = {
		(void *) &cmd_fc_del_qinq_p_string,
		(void *) &cmd_fc_del_qinq_pipeline_id,
		(void *) &cmd_fc_del_qinq_flow_string,
		(void *) &cmd_fc_del_qinq_del_string,
		(void *) &cmd_fc_del_qinq_qinq_string,
		(void *) &cmd_fc_del_qinq_svlan,
		(void *) &cmd_fc_del_qinq_cvlan,
		NULL,
	},
};

/*
 * flow del ipv4_5tuple
 */

struct cmd_fc_del_ipv4_5tuple_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t flow_string;
	cmdline_fixed_string_t del_string;
	cmdline_fixed_string_t ipv4_5tuple_string;
	cmdline_ipaddr_t ip_src;
	cmdline_ipaddr_t ip_dst;
	uint16_t port_src;
	uint16_t port_dst;
	uint32_t proto;
};

static void
cmd_fc_del_ipv4_5tuple_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_fc_del_ipv4_5tuple_result *params = parsed_result;
	struct app_params *app = data;
	struct pipeline_fc_key key;
	int status;

	memset(&key, 0, sizeof(key));
	key.type = FLOW_KEY_IPV4_5TUPLE;
	key.key.ipv4_5tuple.ip_src = rte_bswap32(
		params->ip_src.addr.ipv4.s_addr);
	key.key.ipv4_5tuple.ip_dst = rte_bswap32(
		params->ip_dst.addr.ipv4.s_addr);
	key.key.ipv4_5tuple.port_src = params->port_src;
	key.key.ipv4_5tuple.port_dst = params->port_dst;
	key.key.ipv4_5tuple.proto = params->proto;

	status = app_pipeline_fc_del(app, params->pipeline_id, &key);
	if (status != 0)
		printf("Command failed\n");
}

cmdline_parse_token_string_t cmd_fc_del_ipv4_5tuple_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_del_ipv4_5tuple_result,
		p_string, "p");

cmdline_parse_token_num_t cmd_fc_del_ipv4_5tuple_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_del_ipv4_5tuple_result,
		pipeline_id, UINT32);

cmdline_parse_token_string_t cmd_fc_del_ipv4_5tuple_flow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_del_ipv4_5tuple_result,
		flow_string, "flow");

cmdline_parse_token_string_t cmd_fc_del_ipv4_5tuple_del_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_del_ipv4_5tuple_result,
		del_string, "del");

cmdline_parse_token_string_t cmd_fc_del_ipv4_5tuple_ipv4_5tuple_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_del_ipv4_5tuple_result,
		ipv4_5tuple_string, "ipv4_5tuple");

cmdline_parse_token_ipaddr_t cmd_fc_del_ipv4_5tuple_ip_src =
	TOKEN_IPV4_INITIALIZER(struct cmd_fc_del_ipv4_5tuple_result,
		ip_src);

cmdline_parse_token_ipaddr_t cmd_fc_del_ipv4_5tuple_ip_dst =
	TOKEN_IPV4_INITIALIZER(struct cmd_fc_del_ipv4_5tuple_result, ip_dst);

cmdline_parse_token_num_t cmd_fc_del_ipv4_5tuple_port_src =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_del_ipv4_5tuple_result,
		port_src, UINT16);

cmdline_parse_token_num_t cmd_fc_del_ipv4_5tuple_port_dst =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_del_ipv4_5tuple_result,
		port_dst, UINT16);

cmdline_parse_token_num_t cmd_fc_del_ipv4_5tuple_proto =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_del_ipv4_5tuple_result,
		proto, UINT32);

cmdline_parse_inst_t cmd_fc_del_ipv4_5tuple = {
	.f = cmd_fc_del_ipv4_5tuple_parsed,
	.data = NULL,
	.help_str = "Flow delete (IPv4 5-tuple)",
	.tokens = {
		(void *) &cmd_fc_del_ipv4_5tuple_p_string,
		(void *) &cmd_fc_del_ipv4_5tuple_pipeline_id,
		(void *) &cmd_fc_del_ipv4_5tuple_flow_string,
		(void *) &cmd_fc_del_ipv4_5tuple_del_string,
		(void *) &cmd_fc_del_ipv4_5tuple_ipv4_5tuple_string,
		(void *) &cmd_fc_del_ipv4_5tuple_ip_src,
		(void *) &cmd_fc_del_ipv4_5tuple_ip_dst,
		(void *) &cmd_fc_del_ipv4_5tuple_port_src,
		(void *) &cmd_fc_del_ipv4_5tuple_port_dst,
		(void *) &cmd_fc_del_ipv4_5tuple_proto,
		NULL,
	},
};

/*
 * flow del ipv6_5tuple
 */

struct cmd_fc_del_ipv6_5tuple_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t flow_string;
	cmdline_fixed_string_t del_string;
	cmdline_fixed_string_t ipv6_5tuple_string;
	cmdline_ipaddr_t ip_src;
	cmdline_ipaddr_t ip_dst;
	uint16_t port_src;
	uint16_t port_dst;
	uint32_t proto;
};

static void
cmd_fc_del_ipv6_5tuple_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_fc_del_ipv6_5tuple_result *params = parsed_result;
	struct app_params *app = data;
	struct pipeline_fc_key key;
	int status;

	memset(&key, 0, sizeof(key));
	key.type = FLOW_KEY_IPV6_5TUPLE;
	memcpy(key.key.ipv6_5tuple.ip_src,
		params->ip_src.addr.ipv6.s6_addr,
		16);
	memcpy(key.key.ipv6_5tuple.ip_dst,
		params->ip_dst.addr.ipv6.s6_addr,
		16);
	key.key.ipv6_5tuple.port_src = params->port_src;
	key.key.ipv6_5tuple.port_dst = params->port_dst;
	key.key.ipv6_5tuple.proto = params->proto;

	status = app_pipeline_fc_del(app, params->pipeline_id, &key);
	if (status != 0)
		printf("Command failed\n");
}

cmdline_parse_token_string_t cmd_fc_del_ipv6_5tuple_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_del_ipv6_5tuple_result,
		p_string, "p");

cmdline_parse_token_num_t cmd_fc_del_ipv6_5tuple_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_del_ipv6_5tuple_result,
		pipeline_id, UINT32);

cmdline_parse_token_string_t cmd_fc_del_ipv6_5tuple_flow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_del_ipv6_5tuple_result,
		flow_string, "flow");

cmdline_parse_token_string_t cmd_fc_del_ipv6_5tuple_del_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_del_ipv6_5tuple_result,
		del_string, "del");

cmdline_parse_token_string_t cmd_fc_del_ipv6_5tuple_ipv6_5tuple_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_del_ipv6_5tuple_result,
		ipv6_5tuple_string, "ipv6_5tuple");

cmdline_parse_token_ipaddr_t cmd_fc_del_ipv6_5tuple_ip_src =
	TOKEN_IPV6_INITIALIZER(struct cmd_fc_del_ipv6_5tuple_result, ip_src);

cmdline_parse_token_ipaddr_t cmd_fc_del_ipv6_5tuple_ip_dst =
	TOKEN_IPV6_INITIALIZER(struct cmd_fc_del_ipv6_5tuple_result, ip_dst);

cmdline_parse_token_num_t cmd_fc_del_ipv6_5tuple_port_src =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_del_ipv6_5tuple_result, port_src,
		UINT16);

cmdline_parse_token_num_t cmd_fc_del_ipv6_5tuple_port_dst =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_del_ipv6_5tuple_result, port_dst,
		UINT16);

cmdline_parse_token_num_t cmd_fc_del_ipv6_5tuple_proto =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_del_ipv6_5tuple_result, proto,
		UINT32);

cmdline_parse_inst_t cmd_fc_del_ipv6_5tuple = {
	.f = cmd_fc_del_ipv6_5tuple_parsed,
	.data = NULL,
	.help_str = "Flow delete (IPv6 5-tuple)",
	.tokens = {
		(void *) &cmd_fc_del_ipv6_5tuple_p_string,
		(void *) &cmd_fc_del_ipv6_5tuple_pipeline_id,
		(void *) &cmd_fc_del_ipv6_5tuple_flow_string,
		(void *) &cmd_fc_del_ipv6_5tuple_del_string,
		(void *) &cmd_fc_del_ipv6_5tuple_ipv6_5tuple_string,
		(void *) &cmd_fc_del_ipv6_5tuple_ip_src,
		(void *) &cmd_fc_del_ipv6_5tuple_ip_dst,
		(void *) &cmd_fc_del_ipv6_5tuple_port_src,
		(void *) &cmd_fc_del_ipv6_5tuple_port_dst,
		(void *) &cmd_fc_del_ipv6_5tuple_proto,
		NULL,
	},
};

/*
 * flow add default
 */

struct cmd_fc_add_default_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t flow_string;
	cmdline_fixed_string_t add_string;
	cmdline_fixed_string_t default_string;
	uint32_t port;
};

static void
cmd_fc_add_default_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_fc_add_default_result *params = parsed_result;
	struct app_params *app = data;
	int status;

	status = app_pipeline_fc_add_default(app, params->pipeline_id,
		params->port);

	if (status != 0)
		printf("Command failed\n");
}

cmdline_parse_token_string_t cmd_fc_add_default_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_default_result, p_string,
		"p");

cmdline_parse_token_num_t cmd_fc_add_default_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_default_result, pipeline_id,
		UINT32);

cmdline_parse_token_string_t cmd_fc_add_default_flow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_default_result, flow_string,
		"flow");

cmdline_parse_token_string_t cmd_fc_add_default_add_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_default_result, add_string,
		"add");

cmdline_parse_token_string_t cmd_fc_add_default_default_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_add_default_result,
		default_string, "default");

cmdline_parse_token_num_t cmd_fc_add_default_port =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_add_default_result, port, UINT32);

cmdline_parse_inst_t cmd_fc_add_default = {
	.f = cmd_fc_add_default_parsed,
	.data = NULL,
	.help_str = "Flow add default",
	.tokens = {
		(void *) &cmd_fc_add_default_p_string,
		(void *) &cmd_fc_add_default_pipeline_id,
		(void *) &cmd_fc_add_default_flow_string,
		(void *) &cmd_fc_add_default_add_string,
		(void *) &cmd_fc_add_default_default_string,
		(void *) &cmd_fc_add_default_port,
		NULL,
	},
};

/*
 * flow del default
 */

struct cmd_fc_del_default_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t flow_string;
	cmdline_fixed_string_t del_string;
	cmdline_fixed_string_t default_string;
};

static void
cmd_fc_del_default_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_fc_del_default_result *params = parsed_result;
	struct app_params *app = data;
	int status;

	status = app_pipeline_fc_del_default(app, params->pipeline_id);
	if (status != 0)
		printf("Command failed\n");
}

cmdline_parse_token_string_t cmd_fc_del_default_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_del_default_result, p_string,
		"p");

cmdline_parse_token_num_t cmd_fc_del_default_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_del_default_result, pipeline_id,
		UINT32);

cmdline_parse_token_string_t cmd_fc_del_default_flow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_del_default_result, flow_string,
		"flow");

cmdline_parse_token_string_t cmd_fc_del_default_del_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_del_default_result, del_string,
		"del");

cmdline_parse_token_string_t cmd_fc_del_default_default_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_del_default_result,
		default_string, "default");

cmdline_parse_inst_t cmd_fc_del_default = {
	.f = cmd_fc_del_default_parsed,
	.data = NULL,
	.help_str = "Flow delete default",
	.tokens = {
		(void *) &cmd_fc_del_default_p_string,
		(void *) &cmd_fc_del_default_pipeline_id,
		(void *) &cmd_fc_del_default_flow_string,
		(void *) &cmd_fc_del_default_del_string,
		(void *) &cmd_fc_del_default_default_string,
		NULL,
	},
};

/*
 * flow ls
 */

struct cmd_fc_ls_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t flow_string;
	cmdline_fixed_string_t ls_string;
};

static void
cmd_fc_ls_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	void *data)
{
	struct cmd_fc_ls_result *params = parsed_result;
	struct app_params *app = data;
	int status;

	status = app_pipeline_fc_ls(app, params->pipeline_id);
	if (status != 0)
		printf("Command failed\n");
}

cmdline_parse_token_string_t cmd_fc_ls_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_ls_result, p_string, "p");

cmdline_parse_token_num_t cmd_fc_ls_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fc_ls_result, pipeline_id, UINT32);

cmdline_parse_token_string_t cmd_fc_ls_flow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_ls_result,
	flow_string, "flow");

cmdline_parse_token_string_t cmd_fc_ls_ls_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fc_ls_result, ls_string,
	"ls");

cmdline_parse_inst_t cmd_fc_ls = {
	.f = cmd_fc_ls_parsed,
	.data = NULL,
	.help_str = "Flow list",
	.tokens = {
		(void *) &cmd_fc_ls_p_string,
		(void *) &cmd_fc_ls_pipeline_id,
		(void *) &cmd_fc_ls_flow_string,
		(void *) &cmd_fc_ls_ls_string,
		NULL,
	},
};

static cmdline_parse_ctx_t pipeline_cmds[] = {
	(cmdline_parse_inst_t *) &cmd_fc_add_qinq,
	(cmdline_parse_inst_t *) &cmd_fc_add_ipv4_5tuple,
	(cmdline_parse_inst_t *) &cmd_fc_add_ipv6_5tuple,

	(cmdline_parse_inst_t *) &cmd_fc_del_qinq,
	(cmdline_parse_inst_t *) &cmd_fc_del_ipv4_5tuple,
	(cmdline_parse_inst_t *) &cmd_fc_del_ipv6_5tuple,

	(cmdline_parse_inst_t *) &cmd_fc_add_default,
	(cmdline_parse_inst_t *) &cmd_fc_del_default,

	(cmdline_parse_inst_t *) &cmd_fc_add_qinq_all,
	(cmdline_parse_inst_t *) &cmd_fc_add_ipv4_5tuple_all,
	(cmdline_parse_inst_t *) &cmd_fc_add_ipv6_5tuple_all,

	(cmdline_parse_inst_t *) &cmd_fc_ls,
	NULL,
};

static struct pipeline_fe_ops pipeline_flow_classification_fe_ops = {
	.f_init = app_pipeline_fc_init,
	.f_free = app_pipeline_fc_free,
	.cmds = pipeline_cmds,
};

struct pipeline_type pipeline_flow_classification = {
	.name = "FLOW_CLASSIFICATION",
	.be_ops = &pipeline_flow_classification_be_ops,
	.fe_ops = &pipeline_flow_classification_fe_ops,
};
