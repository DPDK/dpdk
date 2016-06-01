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

#include <stdio.h>
#include <string.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_malloc.h>
#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>

#include "app.h"
#include "pipeline_common_fe.h"
#include "pipeline_flow_classification.h"
#include "hash_func.h"
#include "parser.h"

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
		qinq->svlan = rte_cpu_to_be_16(key_in->key.qinq.svlan);
		qinq->ethertype_cvlan = 0;
		qinq->cvlan = rte_cpu_to_be_16(key_in->key.qinq.cvlan);

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
		ipv4->ip_src = rte_cpu_to_be_32(key_in->key.ipv4_5tuple.ip_src);
		ipv4->ip_dst = rte_cpu_to_be_32(key_in->key.ipv4_5tuple.ip_dst);
		ipv4->port_src = rte_cpu_to_be_16(key_in->key.ipv4_5tuple.port_src);
		ipv4->port_dst = rte_cpu_to_be_16(key_in->key.ipv4_5tuple.port_dst);

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
		ipv6->port_src = rte_cpu_to_be_16(key_in->key.ipv6_5tuple.port_src);
		ipv6->port_dst = rte_cpu_to_be_16(key_in->key.ipv6_5tuple.port_dst);

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
	uint32_t flow_id;
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
app_pipeline_fc_load_file_qinq(char *filename,
	struct pipeline_fc_key *keys,
	uint32_t *port_ids,
	uint32_t *flow_ids,
	uint32_t *n_keys,
	uint32_t *line)
{
	FILE *f = NULL;
	char file_buf[1024];
	uint32_t i, l;

	/* Check input arguments */
	if ((filename == NULL) ||
		(keys == NULL) ||
		(port_ids == NULL) ||
		(flow_ids == NULL) ||
		(n_keys == NULL) ||
		(*n_keys == 0) ||
		(line == NULL)) {
		if (line)
			*line = 0;
		return -1;
		}

	/* Open input file */
	f = fopen(filename, "r");
	if (f == NULL) {
		*line = 0;
		return -1;
	}

	/* Read file */
	for (i = 0, l = 1; i < *n_keys; l++) {
		char *tokens[32];
		uint32_t n_tokens = RTE_DIM(tokens);

		uint16_t svlan, cvlan;
		uint32_t portid, flowid;
		int status;

		if (fgets(file_buf, sizeof(file_buf), f) == NULL)
			break;

		status = parse_tokenize_string(file_buf, tokens, &n_tokens);
		if (status)
			goto error1;

		if ((n_tokens == 0) || (tokens[0][0] == '#'))
			continue;

		if ((n_tokens != 7) ||
			strcmp(tokens[0], "qinq") ||
			parser_read_uint16(&svlan, tokens[1]) ||
			parser_read_uint16(&cvlan, tokens[2]) ||
			strcmp(tokens[3], "port") ||
			parser_read_uint32(&portid, tokens[4]) ||
			strcmp(tokens[5], "id") ||
			parser_read_uint32(&flowid, tokens[6]))
			goto error1;

		keys[i].type = FLOW_KEY_QINQ;
		keys[i].key.qinq.svlan = svlan;
		keys[i].key.qinq.cvlan = cvlan;

		port_ids[i] = portid;
		flow_ids[i] = flowid;

		if (app_pipeline_fc_key_check(&keys[i]))
			goto error1;

		i++;
	}

	/* Close file */
	*n_keys = i;
	fclose(f);
	return 0;

error1:
	*line = l;
	fclose(f);
	return -1;
}

int
app_pipeline_fc_load_file_ipv4(char *filename,
	struct pipeline_fc_key *keys,
	uint32_t *port_ids,
	uint32_t *flow_ids,
	uint32_t *n_keys,
	uint32_t *line)
{
	FILE *f = NULL;
	char file_buf[1024];
	uint32_t i, l;

	/* Check input arguments */
	if ((filename == NULL) ||
		(keys == NULL) ||
		(port_ids == NULL) ||
		(flow_ids == NULL) ||
		(n_keys == NULL) ||
		(*n_keys == 0) ||
		(line == NULL)) {
		if (line)
			*line = 0;
		return -1;
		}

	/* Open input file */
	f = fopen(filename, "r");
	if (f == NULL) {
		*line = 0;
		return -1;
	}

	/* Read file */
	for (i = 0, l = 1; i < *n_keys; l++) {
		char *tokens[32];
		uint32_t n_tokens = RTE_DIM(tokens);

		struct in_addr sipaddr, dipaddr;
		uint16_t sport, dport;
		uint8_t proto;
		uint32_t portid, flowid;
		int status;

		if (fgets(file_buf, sizeof(file_buf), f) == NULL)
			break;

		status = parse_tokenize_string(file_buf, tokens, &n_tokens);
		if (status)
			goto error2;

		if ((n_tokens == 0) || (tokens[0][0] == '#'))
			continue;

		if ((n_tokens != 10) ||
			strcmp(tokens[0], "ipv4") ||
			parse_ipv4_addr(tokens[1], &sipaddr) ||
			parse_ipv4_addr(tokens[2], &dipaddr) ||
			parser_read_uint16(&sport, tokens[3]) ||
			parser_read_uint16(&dport, tokens[4]) ||
			parser_read_uint8(&proto, tokens[5]) ||
			strcmp(tokens[6], "port") ||
			parser_read_uint32(&portid, tokens[7]) ||
			strcmp(tokens[8], "id") ||
			parser_read_uint32(&flowid, tokens[9]))
			goto error2;

		keys[i].type = FLOW_KEY_IPV4_5TUPLE;
		keys[i].key.ipv4_5tuple.ip_src = rte_be_to_cpu_32(sipaddr.s_addr);
		keys[i].key.ipv4_5tuple.ip_dst = rte_be_to_cpu_32(dipaddr.s_addr);
		keys[i].key.ipv4_5tuple.port_src = sport;
		keys[i].key.ipv4_5tuple.port_dst = dport;
		keys[i].key.ipv4_5tuple.proto = proto;

		port_ids[i] = portid;
		flow_ids[i] = flowid;

		if (app_pipeline_fc_key_check(&keys[i]))
			goto error2;

		i++;
	}

	/* Close file */
	*n_keys = i;
	fclose(f);
	return 0;

error2:
	*line = l;
	fclose(f);
	return -1;
}

int
app_pipeline_fc_load_file_ipv6(char *filename,
	struct pipeline_fc_key *keys,
	uint32_t *port_ids,
	uint32_t *flow_ids,
	uint32_t *n_keys,
	uint32_t *line)
{
	FILE *f = NULL;
	char file_buf[1024];
	uint32_t i, l;

	/* Check input arguments */
	if ((filename == NULL) ||
		(keys == NULL) ||
		(port_ids == NULL) ||
		(flow_ids == NULL) ||
		(n_keys == NULL) ||
		(*n_keys == 0) ||
		(line == NULL)) {
		if (line)
			*line = 0;
		return -1;
		}

	/* Open input file */
	f = fopen(filename, "r");
	if (f == NULL) {
		*line = 0;
		return -1;
	}

	/* Read file */
	for (i = 0, l = 1; i < *n_keys; l++) {
		char *tokens[32];
		uint32_t n_tokens = RTE_DIM(tokens);

		struct in6_addr sipaddr, dipaddr;
		uint16_t sport, dport;
		uint8_t proto;
		uint32_t portid, flowid;
		int status;

		if (fgets(file_buf, sizeof(file_buf), f) == NULL)
			break;

		status = parse_tokenize_string(file_buf, tokens, &n_tokens);
		if (status)
			goto error3;

		if ((n_tokens == 0) || (tokens[0][0] == '#'))
			continue;

		if ((n_tokens != 10) ||
			strcmp(tokens[0], "ipv6") ||
			parse_ipv6_addr(tokens[1], &sipaddr) ||
			parse_ipv6_addr(tokens[2], &dipaddr) ||
			parser_read_uint16(&sport, tokens[3]) ||
			parser_read_uint16(&dport, tokens[4]) ||
			parser_read_uint8(&proto, tokens[5]) ||
			strcmp(tokens[6], "port") ||
			parser_read_uint32(&portid, tokens[7]) ||
			strcmp(tokens[8], "id") ||
			parser_read_uint32(&flowid, tokens[9]))
			goto error3;

		keys[i].type = FLOW_KEY_IPV6_5TUPLE;
		memcpy(keys[i].key.ipv6_5tuple.ip_src,
			sipaddr.s6_addr,
			sizeof(sipaddr.s6_addr));
		memcpy(keys[i].key.ipv6_5tuple.ip_dst,
			dipaddr.s6_addr,
			sizeof(dipaddr.s6_addr));
		keys[i].key.ipv6_5tuple.port_src = sport;
		keys[i].key.ipv6_5tuple.port_dst = dport;
		keys[i].key.ipv6_5tuple.proto = proto;

		port_ids[i] = portid;
		flow_ids[i] = flowid;

		if (app_pipeline_fc_key_check(&keys[i]))
			goto error3;

		i++;
	}

	/* Close file */
	*n_keys = i;
	fclose(f);
	return 0;

error3:
	*line = l;
	fclose(f);
	return -1;
}



int
app_pipeline_fc_add(struct app_params *app,
	uint32_t pipeline_id,
	struct pipeline_fc_key *key,
	uint32_t port_id,
	uint32_t flow_id)
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

	p = app_pipeline_data_fe(app, pipeline_id, &pipeline_flow_classification);
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
	req->flow_id = flow_id;

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
	flow->flow_id = flow_id;
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
	uint32_t *flow_id,
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
		(flow_id == NULL) ||
		(n_keys == 0))
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id, &pipeline_flow_classification);
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
	if (flow_rsp == NULL) {
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
		flow_req[i].flow_id = flow_id[i];
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
		flow[i]->flow_id = flow_id[i];
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

	p = app_pipeline_data_fe(app, pipeline_id, &pipeline_flow_classification);
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

	p = app_pipeline_data_fe(app, pipeline_id, &pipeline_flow_classification);
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

	p = app_pipeline_data_fe(app, pipeline_id, &pipeline_flow_classification);
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
		"Port = %" PRIu32 ", "
		"Flow ID = %" PRIu32 ", "
		"(signature = 0x%08" PRIx32 ", "
		"entry_ptr = %p)\n",

		flow->key.key.qinq.svlan,
		flow->key.key.qinq.cvlan,
		flow->port_id,
		flow->flow_id,
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
		   "Port = %" PRIu32 ", "
		   "Flow ID = %" PRIu32 " "
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
		   flow->flow_id,
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
		"=> Port = %" PRIu32 ", "
		"Flow ID = %" PRIu32 " "
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
		flow->flow_id,
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

	p = app_pipeline_data_fe(app, pipeline_id, &pipeline_flow_classification);
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
 * flow
 *
 * flow add:
 *    p <pipelineid> flow add qinq <svlan> <cvlan> port <portid> id <flowid>
 *    p <pipelineid> flow add qinq bulk <file>
 *    p <pipelineid> flow add ipv4 <sipaddr> <dipaddr> <sport> <dport> <proto> port <port ID> id <flowid>
 *    p <pipelineid> flow add ipv4 bulk <file>
 *    p <pipelineid> flow add ipv6 <sipaddr> <dipaddr> <sport> <dport> <proto> port <port ID> id <flowid>
 *    p <pipelineid> flow add ipv6 bulk <file>
 *
 * flow add default:
 *    p <pipelineid> flow add default <portid>
 *
 * flow del:
 *    p <pipelineid> flow del qinq <svlan> <cvlan>
 *    p <pipelineid> flow del ipv4 <sipaddr> <dipaddr> <sport> <dport> <proto>
 *    p <pipelineid> flow del ipv6 <sipaddr> <dipaddr> <sport> <dport> <proto>
 *
 * flow del default:
 *    p <pipelineid> flow del default
 *
 * flow ls:
 *    p <pipelineid> flow ls
 */

struct cmd_flow_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t flow_string;
	cmdline_multi_string_t multi_string;
};

static void
cmd_flow_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	void *data)
{
	struct cmd_flow_result *results = parsed_result;
	struct app_params *app = data;

	char *tokens[16];
	uint32_t n_tokens = RTE_DIM(tokens);
	int status;

	status = parse_tokenize_string(results->multi_string, tokens, &n_tokens);
	if (status) {
		printf(CMD_MSG_TOO_MANY_ARGS, "flow");
		return;
	}

	/* flow add qinq */
	if ((n_tokens >= 3) &&
		(strcmp(tokens[0], "add") == 0) &&
		(strcmp(tokens[1], "qinq") == 0) &&
		strcmp(tokens[2], "bulk")) {
		struct pipeline_fc_key key;
		uint32_t svlan;
		uint32_t cvlan;
		uint32_t port_id;
		uint32_t flow_id;

		memset(&key, 0, sizeof(key));

		if (n_tokens != 8) {
			printf(CMD_MSG_MISMATCH_ARGS, "flow add qinq");
			return;
		}

		if (parser_read_uint32(&svlan, tokens[2]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "svlan");
			return;
		}

		if (parser_read_uint32(&cvlan, tokens[3]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "cvlan");
			return;
		}

		if (strcmp(tokens[4], "port") != 0) {
			printf(CMD_MSG_ARG_NOT_FOUND, "port");
			return;
		}

		if (parser_read_uint32(&port_id, tokens[5]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "portid");
			return;
		}

		if (strcmp(tokens[6], "id") != 0) {
			printf(CMD_MSG_ARG_NOT_FOUND, "id");
			return;
		}

		if (parser_read_uint32(&flow_id, tokens[7]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "flowid");
			return;
		}

		key.type = FLOW_KEY_QINQ;
		key.key.qinq.svlan = svlan;
		key.key.qinq.cvlan = cvlan;

		status = app_pipeline_fc_add(app,
			results->pipeline_id,
			&key,
			port_id,
			flow_id);
		if (status)
			printf(CMD_MSG_FAIL, "flow add qinq");

		return;
	} /* flow add qinq */

	/* flow add ipv4 */
	if ((n_tokens >= 3) &&
		(strcmp(tokens[0], "add") == 0) &&
		(strcmp(tokens[1], "ipv4") == 0) &&
		strcmp(tokens[2], "bulk")) {
		struct pipeline_fc_key key;
		struct in_addr sipaddr;
		struct in_addr dipaddr;
		uint32_t sport;
		uint32_t dport;
		uint32_t proto;
		uint32_t port_id;
		uint32_t flow_id;

		memset(&key, 0, sizeof(key));

		if (n_tokens != 11) {
			printf(CMD_MSG_MISMATCH_ARGS, "flow add ipv4");
			return;
		}

		if (parse_ipv4_addr(tokens[2], &sipaddr) != 0) {
			printf(CMD_MSG_INVALID_ARG, "sipv4addr");
			return;
		}
		if (parse_ipv4_addr(tokens[3], &dipaddr) != 0) {
			printf(CMD_MSG_INVALID_ARG, "dipv4addr");
			return;
		}

		if (parser_read_uint32(&sport, tokens[4]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "sport");
			return;
		}

		if (parser_read_uint32(&dport, tokens[5]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "dport");
			return;
		}

		if (parser_read_uint32(&proto, tokens[6]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "proto");
			return;
		}

		if (strcmp(tokens[7], "port") != 0) {
			printf(CMD_MSG_ARG_NOT_FOUND, "port");
			return;
		}

		if (parser_read_uint32(&port_id, tokens[8]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "portid");
			return;
		}

		if (strcmp(tokens[9], "id") != 0) {
			printf(CMD_MSG_ARG_NOT_FOUND, "id");
			return;
		}

		if (parser_read_uint32(&flow_id, tokens[10]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "flowid");
			return;
		}

		key.type = FLOW_KEY_IPV4_5TUPLE;
		key.key.ipv4_5tuple.ip_src = rte_be_to_cpu_32(sipaddr.s_addr);
		key.key.ipv4_5tuple.ip_dst = rte_be_to_cpu_32(dipaddr.s_addr);
		key.key.ipv4_5tuple.port_src = sport;
		key.key.ipv4_5tuple.port_dst = dport;
		key.key.ipv4_5tuple.proto = proto;

		status = app_pipeline_fc_add(app,
			results->pipeline_id,
			&key,
			port_id,
			flow_id);
		if (status)
			printf(CMD_MSG_FAIL, "flow add ipv4");

		return;
	} /* flow add ipv4 */

	/* flow add ipv6 */
	if ((n_tokens >= 3) &&
		(strcmp(tokens[0], "add") == 0) &&
		(strcmp(tokens[1], "ipv6") == 0) &&
		strcmp(tokens[2], "bulk")) {
		struct pipeline_fc_key key;
		struct in6_addr sipaddr;
		struct in6_addr dipaddr;
		uint32_t sport;
		uint32_t dport;
		uint32_t proto;
		uint32_t port_id;
		uint32_t flow_id;

		memset(&key, 0, sizeof(key));

		if (n_tokens != 11) {
			printf(CMD_MSG_MISMATCH_ARGS, "flow add ipv6");
			return;
		}

		if (parse_ipv6_addr(tokens[2], &sipaddr) != 0) {
			printf(CMD_MSG_INVALID_ARG, "sipv6addr");
			return;
		}
		if (parse_ipv6_addr(tokens[3], &dipaddr) != 0) {
			printf(CMD_MSG_INVALID_ARG, "dipv6addr");
			return;
		}

		if (parser_read_uint32(&sport, tokens[4]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "sport");
			return;
		}

		if (parser_read_uint32(&dport, tokens[5]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "dport");
			return;
		}

		if (parser_read_uint32(&proto, tokens[6]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "proto");
			return;
		}

		if (strcmp(tokens[7], "port") != 0) {
			printf(CMD_MSG_ARG_NOT_FOUND, "port");
			return;
		}

		if (parser_read_uint32(&port_id, tokens[8]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "portid");
			return;
		}

		if (strcmp(tokens[9], "id") != 0) {
			printf(CMD_MSG_ARG_NOT_FOUND, "id");
			return;
		}

		if (parser_read_uint32(&flow_id, tokens[10]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "flowid");
			return;
		}

		key.type = FLOW_KEY_IPV6_5TUPLE;
		memcpy(key.key.ipv6_5tuple.ip_src, (void *)&sipaddr, 16);
		memcpy(key.key.ipv6_5tuple.ip_dst, (void *)&dipaddr, 16);
		key.key.ipv6_5tuple.port_src = sport;
		key.key.ipv6_5tuple.port_dst = dport;
		key.key.ipv6_5tuple.proto = proto;

		status = app_pipeline_fc_add(app,
			results->pipeline_id,
			&key,
			port_id,
			flow_id);
		if (status)
			printf(CMD_MSG_FAIL, "flow add ipv6");

		return;
	} /* flow add ipv6 */

	/* flow add qinq bulk */
	if ((n_tokens >= 3) &&
		(strcmp(tokens[0], "add") == 0) &&
		(strcmp(tokens[1], "qinq") == 0) &&
		(strcmp(tokens[2], "bulk") == 0)) {
		struct pipeline_fc_key *keys;
		uint32_t *port_ids, *flow_ids, n_keys, line;
		char *filename;

		if (n_tokens != 4) {
			printf(CMD_MSG_MISMATCH_ARGS, "flow add qinq bulk");
			return;
		}

		filename = tokens[3];

		n_keys = APP_PIPELINE_FC_MAX_FLOWS_IN_FILE;
		keys = malloc(n_keys * sizeof(struct pipeline_fc_key));
		if (keys == NULL)
			return;
		memset(keys, 0, n_keys * sizeof(struct pipeline_fc_key));

		port_ids = malloc(n_keys * sizeof(uint32_t));
		if (port_ids == NULL) {
			free(keys);
			return;
		}

		flow_ids = malloc(n_keys * sizeof(uint32_t));
		if (flow_ids == NULL) {
			free(port_ids);
			free(keys);
			return;
		}

		status = app_pipeline_fc_load_file_qinq(filename,
			keys,
			port_ids,
			flow_ids,
			&n_keys,
			&line);
		if (status != 0) {
			printf(CMD_MSG_FILE_ERR, filename, line);
			free(flow_ids);
			free(port_ids);
			free(keys);
			return;
		}

		status = app_pipeline_fc_add_bulk(app,
			results->pipeline_id,
			keys,
			port_ids,
			flow_ids,
			n_keys);
		if (status)
			printf(CMD_MSG_FAIL, "flow add qinq bulk");

		free(flow_ids);
		free(port_ids);
		free(keys);
		return;
	} /* flow add qinq bulk */

	/* flow add ipv4 bulk */
	if ((n_tokens >= 3) &&
		(strcmp(tokens[0], "add") == 0) &&
		(strcmp(tokens[1], "ipv4") == 0) &&
		(strcmp(tokens[2], "bulk") == 0)) {
		struct pipeline_fc_key *keys;
		uint32_t *port_ids, *flow_ids, n_keys, line;
		char *filename;

		if (n_tokens != 4) {
			printf(CMD_MSG_MISMATCH_ARGS, "flow add ipv4 bulk");
			return;
		}

		filename = tokens[3];

		n_keys = APP_PIPELINE_FC_MAX_FLOWS_IN_FILE;
		keys = malloc(n_keys * sizeof(struct pipeline_fc_key));
		if (keys == NULL)
			return;
		memset(keys, 0, n_keys * sizeof(struct pipeline_fc_key));

		port_ids = malloc(n_keys * sizeof(uint32_t));
		if (port_ids == NULL) {
			free(keys);
			return;
		}

		flow_ids = malloc(n_keys * sizeof(uint32_t));
		if (flow_ids == NULL) {
			free(port_ids);
			free(keys);
			return;
		}

		status = app_pipeline_fc_load_file_ipv4(filename,
			keys,
			port_ids,
			flow_ids,
			&n_keys,
			&line);
		if (status != 0) {
			printf(CMD_MSG_FILE_ERR, filename, line);
			free(flow_ids);
			free(port_ids);
			free(keys);
			return;
		}

		status = app_pipeline_fc_add_bulk(app,
			results->pipeline_id,
			keys,
			port_ids,
			flow_ids,
			n_keys);
		if (status)
			printf(CMD_MSG_FAIL, "flow add ipv4 bulk");

		free(flow_ids);
		free(port_ids);
		free(keys);
		return;
	} /* flow add ipv4 bulk */

	/* flow add ipv6 bulk */
	if ((n_tokens >= 3) &&
		(strcmp(tokens[0], "add") == 0) &&
		(strcmp(tokens[1], "ipv6") == 0) &&
		(strcmp(tokens[2], "bulk") == 0)) {
		struct pipeline_fc_key *keys;
		uint32_t *port_ids, *flow_ids, n_keys, line;
		char *filename;

		if (n_tokens != 4) {
			printf(CMD_MSG_MISMATCH_ARGS, "flow add ipv6 bulk");
			return;
		}

		filename = tokens[3];

		n_keys = APP_PIPELINE_FC_MAX_FLOWS_IN_FILE;
		keys = malloc(n_keys * sizeof(struct pipeline_fc_key));
		if (keys == NULL)
			return;
		memset(keys, 0, n_keys * sizeof(struct pipeline_fc_key));

		port_ids = malloc(n_keys * sizeof(uint32_t));
		if (port_ids == NULL) {
			free(keys);
			return;
		}

		flow_ids = malloc(n_keys * sizeof(uint32_t));
		if (flow_ids == NULL) {
			free(port_ids);
			free(keys);
			return;
		}

		status = app_pipeline_fc_load_file_ipv6(filename,
			keys,
			port_ids,
			flow_ids,
			&n_keys,
			&line);
		if (status != 0) {
			printf(CMD_MSG_FILE_ERR, filename, line);
			free(flow_ids);
			free(port_ids);
			free(keys);
			return;
		}

		status = app_pipeline_fc_add_bulk(app,
			results->pipeline_id,
			keys,
			port_ids,
			flow_ids,
			n_keys);
		if (status)
			printf(CMD_MSG_FAIL, "flow add ipv6 bulk");

		free(flow_ids);
		free(port_ids);
		free(keys);
		return;
	} /* flow add ipv6 bulk */

	/* flow add default*/
	if ((n_tokens >= 2) &&
		(strcmp(tokens[0], "add") == 0) &&
		(strcmp(tokens[1], "default") == 0)) {
		uint32_t port_id;

		if (n_tokens != 3) {
			printf(CMD_MSG_MISMATCH_ARGS, "flow add default");
			return;
		}

		if (parser_read_uint32(&port_id, tokens[2]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "portid");
			return;
		}

		status = app_pipeline_fc_add_default(app,
			results->pipeline_id,
			port_id);
		if (status)
			printf(CMD_MSG_FAIL, "flow add default");

		return;
	} /* flow add default */

	/* flow del qinq */
	if ((n_tokens >= 2) &&
		(strcmp(tokens[0], "del") == 0) &&
		(strcmp(tokens[1], "qinq") == 0)) {
		struct pipeline_fc_key key;
		uint32_t svlan;
		uint32_t cvlan;

		memset(&key, 0, sizeof(key));

		if (n_tokens != 4) {
			printf(CMD_MSG_MISMATCH_ARGS, "flow del qinq");
			return;
		}

		if (parser_read_uint32(&svlan, tokens[2]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "svlan");
			return;
		}

		if (parser_read_uint32(&cvlan, tokens[3]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "cvlan");
			return;
		}

		key.type = FLOW_KEY_QINQ;
		key.key.qinq.svlan = svlan;
		key.key.qinq.cvlan = cvlan;

		status = app_pipeline_fc_del(app,
			results->pipeline_id,
			&key);
		if (status)
			printf(CMD_MSG_FAIL, "flow del qinq");

		return;
	} /* flow del qinq */

	/* flow del ipv4 */
	if ((n_tokens >= 2) &&
		(strcmp(tokens[0], "del") == 0) &&
		(strcmp(tokens[1], "ipv4") == 0)) {
		struct pipeline_fc_key key;
		struct in_addr sipaddr;
		struct in_addr dipaddr;
		uint32_t sport;
		uint32_t dport;
		uint32_t proto;

		memset(&key, 0, sizeof(key));

		if (n_tokens != 7) {
			printf(CMD_MSG_MISMATCH_ARGS, "flow del ipv4");
			return;
		}

		if (parse_ipv4_addr(tokens[2], &sipaddr) != 0) {
			printf(CMD_MSG_INVALID_ARG, "sipv4addr");
			return;
		}
		if (parse_ipv4_addr(tokens[3], &dipaddr) != 0) {
			printf(CMD_MSG_INVALID_ARG, "dipv4addr");
			return;
		}

		if (parser_read_uint32(&sport, tokens[4]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "sport");
			return;
		}

		if (parser_read_uint32(&dport, tokens[5]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "dport");
			return;
		}

		if (parser_read_uint32(&proto, tokens[6]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "proto");
			return;
		}

		key.type = FLOW_KEY_IPV4_5TUPLE;
		key.key.ipv4_5tuple.ip_src = rte_be_to_cpu_32(sipaddr.s_addr);
		key.key.ipv4_5tuple.ip_dst = rte_be_to_cpu_32(dipaddr.s_addr);
		key.key.ipv4_5tuple.port_src = sport;
		key.key.ipv4_5tuple.port_dst = dport;
		key.key.ipv4_5tuple.proto = proto;

		status = app_pipeline_fc_del(app,
			results->pipeline_id,
			&key);
		if (status)
			printf(CMD_MSG_FAIL, "flow del ipv4");

		return;
	} /* flow del ipv4 */

	/* flow del ipv6 */
	if ((n_tokens >= 2) &&
		(strcmp(tokens[0], "del") == 0) &&
		(strcmp(tokens[1], "ipv6") == 0)) {
		struct pipeline_fc_key key;
		struct in6_addr sipaddr;
		struct in6_addr dipaddr;
		uint32_t sport;
		uint32_t dport;
		uint32_t proto;

		memset(&key, 0, sizeof(key));

		if (n_tokens != 7) {
			printf(CMD_MSG_MISMATCH_ARGS, "flow del ipv6");
			return;
		}

		if (parse_ipv6_addr(tokens[2], &sipaddr) != 0) {
			printf(CMD_MSG_INVALID_ARG, "sipv6addr");
			return;
		}

		if (parse_ipv6_addr(tokens[3], &dipaddr) != 0) {
			printf(CMD_MSG_INVALID_ARG, "dipv6addr");
			return;
		}

		if (parser_read_uint32(&sport, tokens[4]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "sport");
			return;
		}

		if (parser_read_uint32(&dport, tokens[5]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "dport");
			return;
		}

		if (parser_read_uint32(&proto, tokens[6]) != 0) {
			printf(CMD_MSG_INVALID_ARG, "proto");
			return;
		}

		key.type = FLOW_KEY_IPV6_5TUPLE;
		memcpy(key.key.ipv6_5tuple.ip_src, &sipaddr, 16);
		memcpy(key.key.ipv6_5tuple.ip_dst, &dipaddr, 16);
		key.key.ipv6_5tuple.port_src = sport;
		key.key.ipv6_5tuple.port_dst = dport;
		key.key.ipv6_5tuple.proto = proto;

		status = app_pipeline_fc_del(app,
			results->pipeline_id,
			&key);
		if (status)
			printf(CMD_MSG_FAIL, "flow del ipv6");

		return;
	} /* flow del ipv6 */

	/* flow del default*/
	if ((n_tokens >= 2) &&
		(strcmp(tokens[0], "del") == 0) &&
		(strcmp(tokens[1], "default") == 0)) {
		if (n_tokens != 2) {
			printf(CMD_MSG_MISMATCH_ARGS, "flow del default");
			return;
		}

		status = app_pipeline_fc_del_default(app,
			results->pipeline_id);
		if (status)
			printf(CMD_MSG_FAIL, "flow del default");

		return;
	} /* flow del default */

	/* flow ls */
	if ((n_tokens >= 1) && (strcmp(tokens[0], "ls") == 0)) {
		if (n_tokens != 1) {
			printf(CMD_MSG_MISMATCH_ARGS, "flow ls");
			return;
		}

		status = app_pipeline_fc_ls(app, results->pipeline_id);
		if (status)
			printf(CMD_MSG_FAIL, "flow ls");

		return;
	} /* flow ls */

	printf(CMD_MSG_MISMATCH_ARGS, "flow");
}

static cmdline_parse_token_string_t cmd_flow_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_result, p_string, "p");

static cmdline_parse_token_num_t cmd_flow_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_result, pipeline_id, UINT32);

static cmdline_parse_token_string_t cmd_flow_flow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_result, flow_string, "flow");

static cmdline_parse_token_string_t cmd_flow_multi_string =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_result, multi_string,
		TOKEN_STRING_MULTI);

static cmdline_parse_inst_t cmd_flow = {
	.f = cmd_flow_parsed,
	.data = NULL,
	.help_str = "flow add / add bulk / add default / del / del default / ls",
	.tokens = {
		(void *) &cmd_flow_p_string,
		(void *) &cmd_flow_pipeline_id,
		(void *) &cmd_flow_flow_string,
		(void *) &cmd_flow_multi_string,
		NULL,
	},
};

static cmdline_parse_ctx_t pipeline_cmds[] = {
	(cmdline_parse_inst_t *) &cmd_flow,
	NULL,
};

static struct pipeline_fe_ops pipeline_flow_classification_fe_ops = {
	.f_init = app_pipeline_fc_init,
	.f_post_init = NULL,
	.f_free = app_pipeline_fc_free,
	.f_track = app_pipeline_track_default,
	.cmds = pipeline_cmds,
};

struct pipeline_type pipeline_flow_classification = {
	.name = "FLOW_CLASSIFICATION",
	.be_ops = &pipeline_flow_classification_be_ops,
	.fe_ops = &pipeline_flow_classification_fe_ops,
};
