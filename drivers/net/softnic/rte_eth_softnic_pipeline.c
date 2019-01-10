/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdlib.h>
#include <string.h>

#include <rte_common.h>
#include <rte_ip.h>
#include <rte_tcp.h>

#include <rte_string_fns.h>
#include <rte_port_ethdev.h>
#include <rte_port_ring.h>
#include <rte_port_source_sink.h>
#include <rte_port_fd.h>
#include <rte_port_sched.h>
#include <rte_port_sym_crypto.h>

#include <rte_table_acl.h>
#include <rte_table_array.h>
#include <rte_table_hash.h>
#include <rte_table_hash_func.h>
#include <rte_table_lpm.h>
#include <rte_table_lpm_ipv6.h>
#include <rte_table_stub.h>

#include "rte_eth_softnic_internals.h"

#ifndef PIPELINE_MSGQ_SIZE
#define PIPELINE_MSGQ_SIZE                                 64
#endif

#ifndef TABLE_LPM_NUMBER_TBL8
#define TABLE_LPM_NUMBER_TBL8                              256
#endif

int
softnic_pipeline_init(struct pmd_internals *p)
{
	TAILQ_INIT(&p->pipeline_list);

	return 0;
}

static void
softnic_pipeline_table_free(struct softnic_table *table)
{
	for ( ; ; ) {
		struct rte_flow *flow;

		flow = TAILQ_FIRST(&table->flows);
		if (flow == NULL)
			break;

		TAILQ_REMOVE(&table->flows, flow, node);
		free(flow);
	}

	for ( ; ; ) {
		struct softnic_table_meter_profile *mp;

		mp = TAILQ_FIRST(&table->meter_profiles);
		if (mp == NULL)
			break;

		TAILQ_REMOVE(&table->meter_profiles, mp, node);
		free(mp);
	}
}

void
softnic_pipeline_free(struct pmd_internals *p)
{
	for ( ; ; ) {
		struct pipeline *pipeline;
		uint32_t table_id;

		pipeline = TAILQ_FIRST(&p->pipeline_list);
		if (pipeline == NULL)
			break;

		TAILQ_REMOVE(&p->pipeline_list, pipeline, node);

		for (table_id = 0; table_id < pipeline->n_tables; table_id++) {
			struct softnic_table *table =
				&pipeline->table[table_id];

			softnic_pipeline_table_free(table);
		}

		rte_ring_free(pipeline->msgq_req);
		rte_ring_free(pipeline->msgq_rsp);
		rte_pipeline_free(pipeline->p);
		free(pipeline);
	}
}

void
softnic_pipeline_disable_all(struct pmd_internals *p)
{
	struct pipeline *pipeline;

	TAILQ_FOREACH(pipeline, &p->pipeline_list, node)
		if (pipeline->enabled)
			softnic_thread_pipeline_disable(p,
				pipeline->thread_id,
				pipeline->name);
}

uint32_t
softnic_pipeline_thread_count(struct pmd_internals *p, uint32_t thread_id)
{
	struct pipeline *pipeline;
	uint32_t count = 0;

	TAILQ_FOREACH(pipeline, &p->pipeline_list, node)
		if ((pipeline->enabled) && (pipeline->thread_id == thread_id))
			count++;

	return count;
}

struct pipeline *
softnic_pipeline_find(struct pmd_internals *p,
	const char *name)
{
	struct pipeline *pipeline;

	if (name == NULL)
		return NULL;

	TAILQ_FOREACH(pipeline, &p->pipeline_list, node)
		if (strcmp(name, pipeline->name) == 0)
			return pipeline;

	return NULL;
}

struct pipeline *
softnic_pipeline_create(struct pmd_internals *softnic,
	const char *name,
	struct pipeline_params *params)
{
	char resource_name[NAME_MAX];
	struct rte_pipeline_params pp;
	struct pipeline *pipeline;
	struct rte_pipeline *p;
	struct rte_ring *msgq_req;
	struct rte_ring *msgq_rsp;

	/* Check input params */
	if (name == NULL ||
		softnic_pipeline_find(softnic, name) ||
		params == NULL ||
		params->timer_period_ms == 0)
		return NULL;

	/* Resource create */
	snprintf(resource_name, sizeof(resource_name), "%s-%s-REQ",
		softnic->params.name,
		name);

	msgq_req = rte_ring_create(resource_name,
		PIPELINE_MSGQ_SIZE,
		softnic->params.cpu_id,
		RING_F_SP_ENQ | RING_F_SC_DEQ);
	if (msgq_req == NULL)
		return NULL;

	snprintf(resource_name, sizeof(resource_name), "%s-%s-RSP",
		softnic->params.name,
		name);

	msgq_rsp = rte_ring_create(resource_name,
		PIPELINE_MSGQ_SIZE,
		softnic->params.cpu_id,
		RING_F_SP_ENQ | RING_F_SC_DEQ);
	if (msgq_rsp == NULL) {
		rte_ring_free(msgq_req);
		return NULL;
	}

	snprintf(resource_name, sizeof(resource_name), "%s_%s",
		softnic->params.name,
		name);

	pp.name = resource_name;
	pp.socket_id = (int)softnic->params.cpu_id;
	pp.offset_port_id = params->offset_port_id;

	p = rte_pipeline_create(&pp);
	if (p == NULL) {
		rte_ring_free(msgq_rsp);
		rte_ring_free(msgq_req);
		return NULL;
	}

	/* Node allocation */
	pipeline = calloc(1, sizeof(struct pipeline));
	if (pipeline == NULL) {
		rte_pipeline_free(p);
		rte_ring_free(msgq_rsp);
		rte_ring_free(msgq_req);
		return NULL;
	}

	/* Node fill in */
	strlcpy(pipeline->name, name, sizeof(pipeline->name));
	pipeline->p = p;
	memcpy(&pipeline->params, params, sizeof(*params));
	pipeline->n_ports_in = 0;
	pipeline->n_ports_out = 0;
	pipeline->n_tables = 0;
	pipeline->msgq_req = msgq_req;
	pipeline->msgq_rsp = msgq_rsp;
	pipeline->timer_period_ms = params->timer_period_ms;
	pipeline->enabled = 0;
	pipeline->cpu_id = softnic->params.cpu_id;

	/* Node add to list */
	TAILQ_INSERT_TAIL(&softnic->pipeline_list, pipeline, node);

	return pipeline;
}

int
softnic_pipeline_port_in_create(struct pmd_internals *softnic,
	const char *pipeline_name,
	struct softnic_port_in_params *params,
	int enabled)
{
	struct rte_pipeline_port_in_params p;

	union {
		struct rte_port_ethdev_reader_params ethdev;
		struct rte_port_ring_reader_params ring;
		struct rte_port_sched_reader_params sched;
		struct rte_port_fd_reader_params fd;
		struct rte_port_source_params source;
		struct rte_port_sym_crypto_reader_params cryptodev;
	} pp;

	struct pipeline *pipeline;
	struct softnic_port_in *port_in;
	struct softnic_port_in_action_profile *ap;
	struct rte_port_in_action *action;
	uint32_t port_id;
	int status;

	memset(&p, 0, sizeof(p));
	memset(&pp, 0, sizeof(pp));

	/* Check input params */
	if (pipeline_name == NULL ||
		params == NULL ||
		params->burst_size == 0 ||
		params->burst_size > RTE_PORT_IN_BURST_SIZE_MAX)
		return -1;

	pipeline = softnic_pipeline_find(softnic, pipeline_name);
	if (pipeline == NULL)
		return -1;

	ap = NULL;
	if (strlen(params->action_profile_name)) {
		ap = softnic_port_in_action_profile_find(softnic,
			params->action_profile_name);
		if (ap == NULL)
			return -1;
	}

	switch (params->type) {
	case PORT_IN_RXQ:
	{
		struct softnic_link *link;

		link = softnic_link_find(softnic, params->dev_name);
		if (link == NULL)
			return -1;

		if (params->rxq.queue_id >= link->n_rxq)
			return -1;

		pp.ethdev.port_id = link->port_id;
		pp.ethdev.queue_id = params->rxq.queue_id;

		p.ops = &rte_port_ethdev_reader_ops;
		p.arg_create = &pp.ethdev;
		break;
	}

	case PORT_IN_SWQ:
	{
		struct softnic_swq *swq;

		swq = softnic_swq_find(softnic, params->dev_name);
		if (swq == NULL)
			return -1;

		pp.ring.ring = swq->r;

		p.ops = &rte_port_ring_reader_ops;
		p.arg_create = &pp.ring;
		break;
	}

	case PORT_IN_TMGR:
	{
		struct softnic_tmgr_port *tmgr_port;

		tmgr_port = softnic_tmgr_port_find(softnic, params->dev_name);
		if (tmgr_port == NULL)
			return -1;

		pp.sched.sched = tmgr_port->s;

		p.ops = &rte_port_sched_reader_ops;
		p.arg_create = &pp.sched;
		break;
	}

	case PORT_IN_TAP:
	{
		struct softnic_tap *tap;
		struct softnic_mempool *mempool;

		tap = softnic_tap_find(softnic, params->dev_name);
		mempool = softnic_mempool_find(softnic, params->tap.mempool_name);
		if (tap == NULL || mempool == NULL)
			return -1;

		pp.fd.fd = tap->fd;
		pp.fd.mempool = mempool->m;
		pp.fd.mtu = params->tap.mtu;

		p.ops = &rte_port_fd_reader_ops;
		p.arg_create = &pp.fd;
		break;
	}

	case PORT_IN_SOURCE:
	{
		struct softnic_mempool *mempool;

		mempool = softnic_mempool_find(softnic, params->source.mempool_name);
		if (mempool == NULL)
			return -1;

		pp.source.mempool = mempool->m;
		pp.source.file_name = params->source.file_name;
		pp.source.n_bytes_per_pkt = params->source.n_bytes_per_pkt;

		p.ops = &rte_port_source_ops;
		p.arg_create = &pp.source;
		break;
	}

	case PORT_IN_CRYPTODEV:
	{
		struct softnic_cryptodev *cryptodev;

		cryptodev = softnic_cryptodev_find(softnic, params->dev_name);
		if (cryptodev == NULL)
			return -1;

		pp.cryptodev.cryptodev_id = cryptodev->dev_id;
		pp.cryptodev.queue_id = params->cryptodev.queue_id;
		pp.cryptodev.f_callback = params->cryptodev.f_callback;
		pp.cryptodev.arg_callback = params->cryptodev.arg_callback;
		p.ops = &rte_port_sym_crypto_reader_ops;
		p.arg_create = &pp.cryptodev;
		break;
	}

	default:
		return -1;
	}

	p.burst_size = params->burst_size;

	/* Resource create */
	action = NULL;
	p.f_action = NULL;
	p.arg_ah = NULL;

	if (ap) {
		action = rte_port_in_action_create(ap->ap,
			softnic->params.cpu_id);
		if (action == NULL)
			return -1;

		status = rte_port_in_action_params_get(action,
			&p);
		if (status) {
			rte_port_in_action_free(action);
			return -1;
		}
	}

	status = rte_pipeline_port_in_create(pipeline->p,
		&p,
		&port_id);
	if (status) {
		rte_port_in_action_free(action);
		return -1;
	}

	if (enabled)
		rte_pipeline_port_in_enable(pipeline->p, port_id);

	/* Pipeline */
	port_in = &pipeline->port_in[pipeline->n_ports_in];
	memcpy(&port_in->params, params, sizeof(*params));
	port_in->ap = ap;
	port_in->a = action;
	pipeline->n_ports_in++;

	return 0;
}

int
softnic_pipeline_port_in_connect_to_table(struct pmd_internals *softnic,
	const char *pipeline_name,
	uint32_t port_id,
	uint32_t table_id)
{
	struct pipeline *pipeline;
	int status;

	/* Check input params */
	if (pipeline_name == NULL)
		return -1;

	pipeline = softnic_pipeline_find(softnic, pipeline_name);
	if (pipeline == NULL ||
		port_id >= pipeline->n_ports_in ||
		table_id >= pipeline->n_tables)
		return -1;

	/* Resource */
	status = rte_pipeline_port_in_connect_to_table(pipeline->p,
		port_id,
		table_id);

	return status;
}

int
softnic_pipeline_port_out_create(struct pmd_internals *softnic,
	const char *pipeline_name,
	struct softnic_port_out_params *params)
{
	struct rte_pipeline_port_out_params p;

	union {
		struct rte_port_ethdev_writer_params ethdev;
		struct rte_port_ring_writer_params ring;
		struct rte_port_sched_writer_params sched;
		struct rte_port_fd_writer_params fd;
		struct rte_port_sink_params sink;
		struct rte_port_sym_crypto_writer_params cryptodev;
	} pp;

	union {
		struct rte_port_ethdev_writer_nodrop_params ethdev;
		struct rte_port_ring_writer_nodrop_params ring;
		struct rte_port_fd_writer_nodrop_params fd;
		struct rte_port_sym_crypto_writer_nodrop_params cryptodev;
	} pp_nodrop;

	struct pipeline *pipeline;
	struct softnic_port_out *port_out;
	uint32_t port_id;
	int status;

	memset(&p, 0, sizeof(p));
	memset(&pp, 0, sizeof(pp));
	memset(&pp_nodrop, 0, sizeof(pp_nodrop));

	/* Check input params */
	if (pipeline_name == NULL ||
		params == NULL ||
		params->burst_size == 0 ||
		params->burst_size > RTE_PORT_IN_BURST_SIZE_MAX)
		return -1;

	pipeline = softnic_pipeline_find(softnic, pipeline_name);
	if (pipeline == NULL)
		return -1;

	switch (params->type) {
	case PORT_OUT_TXQ:
	{
		struct softnic_link *link;

		link = softnic_link_find(softnic, params->dev_name);
		if (link == NULL)
			return -1;

		if (params->txq.queue_id >= link->n_txq)
			return -1;

		pp.ethdev.port_id = link->port_id;
		pp.ethdev.queue_id = params->txq.queue_id;
		pp.ethdev.tx_burst_sz = params->burst_size;

		pp_nodrop.ethdev.port_id = link->port_id;
		pp_nodrop.ethdev.queue_id = params->txq.queue_id;
		pp_nodrop.ethdev.tx_burst_sz = params->burst_size;
		pp_nodrop.ethdev.n_retries = params->n_retries;

		if (params->retry == 0) {
			p.ops = &rte_port_ethdev_writer_ops;
			p.arg_create = &pp.ethdev;
		} else {
			p.ops = &rte_port_ethdev_writer_nodrop_ops;
			p.arg_create = &pp_nodrop.ethdev;
		}
		break;
	}

	case PORT_OUT_SWQ:
	{
		struct softnic_swq *swq;

		swq = softnic_swq_find(softnic, params->dev_name);
		if (swq == NULL)
			return -1;

		pp.ring.ring = swq->r;
		pp.ring.tx_burst_sz = params->burst_size;

		pp_nodrop.ring.ring = swq->r;
		pp_nodrop.ring.tx_burst_sz = params->burst_size;
		pp_nodrop.ring.n_retries = params->n_retries;

		if (params->retry == 0) {
			p.ops = &rte_port_ring_writer_ops;
			p.arg_create = &pp.ring;
		} else {
			p.ops = &rte_port_ring_writer_nodrop_ops;
			p.arg_create = &pp_nodrop.ring;
		}
		break;
	}

	case PORT_OUT_TMGR:
	{
		struct softnic_tmgr_port *tmgr_port;

		tmgr_port = softnic_tmgr_port_find(softnic, params->dev_name);
		if (tmgr_port == NULL)
			return -1;

		pp.sched.sched = tmgr_port->s;
		pp.sched.tx_burst_sz = params->burst_size;

		p.ops = &rte_port_sched_writer_ops;
		p.arg_create = &pp.sched;
		break;
	}

	case PORT_OUT_TAP:
	{
		struct softnic_tap *tap;

		tap = softnic_tap_find(softnic, params->dev_name);
		if (tap == NULL)
			return -1;

		pp.fd.fd = tap->fd;
		pp.fd.tx_burst_sz = params->burst_size;

		pp_nodrop.fd.fd = tap->fd;
		pp_nodrop.fd.tx_burst_sz = params->burst_size;
		pp_nodrop.fd.n_retries = params->n_retries;

		if (params->retry == 0) {
			p.ops = &rte_port_fd_writer_ops;
			p.arg_create = &pp.fd;
		} else {
			p.ops = &rte_port_fd_writer_nodrop_ops;
			p.arg_create = &pp_nodrop.fd;
		}
		break;
	}

	case PORT_OUT_SINK:
	{
		pp.sink.file_name = params->sink.file_name;
		pp.sink.max_n_pkts = params->sink.max_n_pkts;

		p.ops = &rte_port_sink_ops;
		p.arg_create = &pp.sink;
		break;
	}

	case PORT_OUT_CRYPTODEV:
	{
		struct softnic_cryptodev *cryptodev;

		cryptodev = softnic_cryptodev_find(softnic, params->dev_name);
		if (cryptodev == NULL)
			return -1;

		if (params->cryptodev.queue_id >= cryptodev->n_queues)
			return -1;

		pp.cryptodev.cryptodev_id = cryptodev->dev_id;
		pp.cryptodev.queue_id = params->cryptodev.queue_id;
		pp.cryptodev.tx_burst_sz = params->burst_size;
		pp.cryptodev.crypto_op_offset = params->cryptodev.op_offset;

		pp_nodrop.cryptodev.cryptodev_id = cryptodev->dev_id;
		pp_nodrop.cryptodev.queue_id = params->cryptodev.queue_id;
		pp_nodrop.cryptodev.tx_burst_sz = params->burst_size;
		pp_nodrop.cryptodev.n_retries = params->retry;
		pp_nodrop.cryptodev.crypto_op_offset =
				params->cryptodev.op_offset;

		if (params->retry == 0) {
			p.ops = &rte_port_sym_crypto_writer_ops;
			p.arg_create = &pp.cryptodev;
		} else {
			p.ops = &rte_port_sym_crypto_writer_nodrop_ops;
			p.arg_create = &pp_nodrop.cryptodev;
		}

		break;
	}

	default:
		return -1;
	}

	p.f_action = NULL;
	p.arg_ah = NULL;

	/* Resource create */
	status = rte_pipeline_port_out_create(pipeline->p,
		&p,
		&port_id);

	if (status)
		return -1;

	/* Pipeline */
	port_out = &pipeline->port_out[pipeline->n_ports_out];
	memcpy(&port_out->params, params, sizeof(*params));
	pipeline->n_ports_out++;

	return 0;
}

static const struct rte_acl_field_def table_acl_field_format_ipv4[] = {
	/* Protocol */
	[0] = {
		.type = RTE_ACL_FIELD_TYPE_BITMASK,
		.size = sizeof(uint8_t),
		.field_index = 0,
		.input_index = 0,
		.offset = offsetof(struct ipv4_hdr, next_proto_id),
	},

	/* Source IP address (IPv4) */
	[1] = {
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = 1,
		.input_index = 1,
		.offset = offsetof(struct ipv4_hdr, src_addr),
	},

	/* Destination IP address (IPv4) */
	[2] = {
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = 2,
		.input_index = 2,
		.offset = offsetof(struct ipv4_hdr, dst_addr),
	},

	/* Source Port */
	[3] = {
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = 3,
		.input_index = 3,
		.offset = sizeof(struct ipv4_hdr) +
			offsetof(struct tcp_hdr, src_port),
	},

	/* Destination Port */
	[4] = {
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = 4,
		.input_index = 3,
		.offset = sizeof(struct ipv4_hdr) +
			offsetof(struct tcp_hdr, dst_port),
	},
};

static const struct rte_acl_field_def table_acl_field_format_ipv6[] = {
	/* Protocol */
	[0] = {
		.type = RTE_ACL_FIELD_TYPE_BITMASK,
		.size = sizeof(uint8_t),
		.field_index = 0,
		.input_index = 0,
		.offset = offsetof(struct ipv6_hdr, proto),
	},

	/* Source IP address (IPv6) */
	[1] = {
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = 1,
		.input_index = 1,
		.offset = offsetof(struct ipv6_hdr, src_addr[0]),
	},

	[2] = {
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = 2,
		.input_index = 2,
		.offset = offsetof(struct ipv6_hdr, src_addr[4]),
	},

	[3] = {
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = 3,
		.input_index = 3,
		.offset = offsetof(struct ipv6_hdr, src_addr[8]),
	},

	[4] = {
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = 4,
		.input_index = 4,
		.offset = offsetof(struct ipv6_hdr, src_addr[12]),
	},

	/* Destination IP address (IPv6) */
	[5] = {
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = 5,
		.input_index = 5,
		.offset = offsetof(struct ipv6_hdr, dst_addr[0]),
	},

	[6] = {
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = 6,
		.input_index = 6,
		.offset = offsetof(struct ipv6_hdr, dst_addr[4]),
	},

	[7] = {
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = 7,
		.input_index = 7,
		.offset = offsetof(struct ipv6_hdr, dst_addr[8]),
	},

	[8] = {
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = 8,
		.input_index = 8,
		.offset = offsetof(struct ipv6_hdr, dst_addr[12]),
	},

	/* Source Port */
	[9] = {
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = 9,
		.input_index = 9,
		.offset = sizeof(struct ipv6_hdr) +
			offsetof(struct tcp_hdr, src_port),
	},

	/* Destination Port */
	[10] = {
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = 10,
		.input_index = 9,
		.offset = sizeof(struct ipv6_hdr) +
			offsetof(struct tcp_hdr, dst_port),
	},
};

int
softnic_pipeline_table_create(struct pmd_internals *softnic,
	const char *pipeline_name,
	struct softnic_table_params *params)
{
	char name[NAME_MAX];
	struct rte_pipeline_table_params p;

	union {
		struct rte_table_acl_params acl;
		struct rte_table_array_params array;
		struct rte_table_hash_params hash;
		struct rte_table_lpm_params lpm;
		struct rte_table_lpm_ipv6_params lpm_ipv6;
	} pp;

	struct pipeline *pipeline;
	struct softnic_table *table;
	struct softnic_table_action_profile *ap;
	struct rte_table_action *action;
	uint32_t table_id;
	int status;

	memset(&p, 0, sizeof(p));
	memset(&pp, 0, sizeof(pp));

	/* Check input params */
	if (pipeline_name == NULL ||
		params == NULL)
		return -1;

	pipeline = softnic_pipeline_find(softnic, pipeline_name);
	if (pipeline == NULL ||
		pipeline->n_tables >= RTE_PIPELINE_TABLE_MAX)
		return -1;

	ap = NULL;
	if (strlen(params->action_profile_name)) {
		ap = softnic_table_action_profile_find(softnic,
			params->action_profile_name);
		if (ap == NULL)
			return -1;
	}

	snprintf(name, NAME_MAX, "%s_%s_table%u",
		softnic->params.name, pipeline_name, pipeline->n_tables);

	switch (params->match_type) {
	case TABLE_ACL:
	{
		uint32_t ip_header_offset = params->match.acl.ip_header_offset -
			(sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM);
		uint32_t i;

		if (params->match.acl.n_rules == 0)
			return -1;

		pp.acl.name = name;
		pp.acl.n_rules = params->match.acl.n_rules;
		if (params->match.acl.ip_version) {
			memcpy(&pp.acl.field_format,
				&table_acl_field_format_ipv4,
				sizeof(table_acl_field_format_ipv4));
			pp.acl.n_rule_fields =
				RTE_DIM(table_acl_field_format_ipv4);
		} else {
			memcpy(&pp.acl.field_format,
				&table_acl_field_format_ipv6,
				sizeof(table_acl_field_format_ipv6));
			pp.acl.n_rule_fields =
				RTE_DIM(table_acl_field_format_ipv6);
		}

		for (i = 0; i < pp.acl.n_rule_fields; i++)
			pp.acl.field_format[i].offset += ip_header_offset;

		p.ops = &rte_table_acl_ops;
		p.arg_create = &pp.acl;
		break;
	}

	case TABLE_ARRAY:
	{
		if (params->match.array.n_keys == 0)
			return -1;

		pp.array.n_entries = params->match.array.n_keys;
		pp.array.offset = params->match.array.key_offset;

		p.ops = &rte_table_array_ops;
		p.arg_create = &pp.array;
		break;
	}

	case TABLE_HASH:
	{
		struct rte_table_ops *ops;
		rte_table_hash_op_hash f_hash;

		if (params->match.hash.n_keys == 0)
			return -1;

		switch (params->match.hash.key_size) {
		case  8:
			f_hash = rte_table_hash_crc_key8;
			break;
		case 16:
			f_hash = rte_table_hash_crc_key16;
			break;
		case 24:
			f_hash = rte_table_hash_crc_key24;
			break;
		case 32:
			f_hash = rte_table_hash_crc_key32;
			break;
		case 40:
			f_hash = rte_table_hash_crc_key40;
			break;
		case 48:
			f_hash = rte_table_hash_crc_key48;
			break;
		case 56:
			f_hash = rte_table_hash_crc_key56;
			break;
		case 64:
			f_hash = rte_table_hash_crc_key64;
			break;
		default:
			return -1;
		}

		pp.hash.name = name;
		pp.hash.key_size = params->match.hash.key_size;
		pp.hash.key_offset = params->match.hash.key_offset;
		pp.hash.key_mask = params->match.hash.key_mask;
		pp.hash.n_keys = params->match.hash.n_keys;
		pp.hash.n_buckets = params->match.hash.n_buckets;
		pp.hash.f_hash = f_hash;
		pp.hash.seed = 0;

		if (params->match.hash.extendable_bucket)
			switch (params->match.hash.key_size) {
			case  8:
				ops = &rte_table_hash_key8_ext_ops;
				break;
			case 16:
				ops = &rte_table_hash_key16_ext_ops;
				break;
			default:
				ops = &rte_table_hash_ext_ops;
			}
		else
			switch (params->match.hash.key_size) {
			case  8:
				ops = &rte_table_hash_key8_lru_ops;
				break;
			case 16:
				ops = &rte_table_hash_key16_lru_ops;
				break;
			default:
				ops = &rte_table_hash_lru_ops;
			}

		p.ops = ops;
		p.arg_create = &pp.hash;
		break;
	}

	case TABLE_LPM:
	{
		if (params->match.lpm.n_rules == 0)
			return -1;

		switch (params->match.lpm.key_size) {
		case 4:
		{
			pp.lpm.name = name;
			pp.lpm.n_rules = params->match.lpm.n_rules;
			pp.lpm.number_tbl8s = TABLE_LPM_NUMBER_TBL8;
			pp.lpm.flags = 0;
			pp.lpm.entry_unique_size = p.action_data_size +
				sizeof(struct rte_pipeline_table_entry);
			pp.lpm.offset = params->match.lpm.key_offset;

			p.ops = &rte_table_lpm_ops;
			p.arg_create = &pp.lpm;
			break;
		}

		case 16:
		{
			pp.lpm_ipv6.name = name;
			pp.lpm_ipv6.n_rules = params->match.lpm.n_rules;
			pp.lpm_ipv6.number_tbl8s = TABLE_LPM_NUMBER_TBL8;
			pp.lpm_ipv6.entry_unique_size = p.action_data_size +
				sizeof(struct rte_pipeline_table_entry);
			pp.lpm_ipv6.offset = params->match.lpm.key_offset;

			p.ops = &rte_table_lpm_ipv6_ops;
			p.arg_create = &pp.lpm_ipv6;
			break;
		}

		default:
			return -1;
		}

		break;
	}

	case TABLE_STUB:
	{
		p.ops = &rte_table_stub_ops;
		p.arg_create = NULL;
		break;
	}

	default:
		return -1;
	}

	/* Resource create */
	action = NULL;
	p.f_action_hit = NULL;
	p.f_action_miss = NULL;
	p.arg_ah = NULL;

	if (ap) {
		action = rte_table_action_create(ap->ap,
			softnic->params.cpu_id);
		if (action == NULL)
			return -1;

		status = rte_table_action_table_params_get(action,
			&p);
		if (status ||
			((p.action_data_size +
			sizeof(struct rte_pipeline_table_entry)) >
			TABLE_RULE_ACTION_SIZE_MAX)) {
			rte_table_action_free(action);
			return -1;
		}
	}

	if (params->match_type == TABLE_LPM) {
		if (params->match.lpm.key_size == 4)
			pp.lpm.entry_unique_size = p.action_data_size +
				sizeof(struct rte_pipeline_table_entry);

		if (params->match.lpm.key_size == 16)
			pp.lpm_ipv6.entry_unique_size = p.action_data_size +
				sizeof(struct rte_pipeline_table_entry);
	}

	status = rte_pipeline_table_create(pipeline->p,
		&p,
		&table_id);
	if (status) {
		rte_table_action_free(action);
		return -1;
	}

	/* Pipeline */
	table = &pipeline->table[pipeline->n_tables];
	memcpy(&table->params, params, sizeof(*params));
	table->ap = ap;
	table->a = action;
	TAILQ_INIT(&table->flows);
	TAILQ_INIT(&table->meter_profiles);
	memset(&table->dscp_table, 0, sizeof(table->dscp_table));
	pipeline->n_tables++;

	return 0;
}

int
softnic_pipeline_port_out_find(struct pmd_internals *softnic,
		const char *pipeline_name,
		const char *name,
		uint32_t *port_id)
{
	struct pipeline *pipeline;
	uint32_t i;

	if (softnic == NULL ||
			pipeline_name == NULL ||
			name == NULL ||
			port_id == NULL)
		return -1;

	pipeline = softnic_pipeline_find(softnic, pipeline_name);
	if (pipeline == NULL)
		return -1;

	for (i = 0; i < pipeline->n_ports_out; i++)
		if (strcmp(pipeline->port_out[i].params.dev_name, name) == 0) {
			*port_id = i;
			return 0;
		}

	return -1;
}

struct softnic_table_meter_profile *
softnic_pipeline_table_meter_profile_find(struct softnic_table *table,
	uint32_t meter_profile_id)
{
	struct softnic_table_meter_profile *mp;

	TAILQ_FOREACH(mp, &table->meter_profiles, node)
		if (mp->meter_profile_id == meter_profile_id)
			return mp;

	return NULL;
}
