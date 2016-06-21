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

#include <rte_ip.h>

#include "app.h"

static void
check_mempools(struct app_params *app)
{
	uint32_t i;

	for (i = 0; i < app->n_mempools; i++) {
		struct app_mempool_params *p = &app->mempool_params[i];

		APP_CHECK((p->pool_size > 0),
			"Mempool %s size is 0\n", p->name);

		APP_CHECK((p->cache_size > 0),
			"Mempool %s cache size is 0\n", p->name);

		APP_CHECK(rte_is_power_of_2(p->cache_size),
			"Mempool %s cache size not a power of 2\n", p->name);
	}
}

static inline uint32_t
link_rxq_used(struct app_link_params *link, uint32_t q_id)
{
	uint32_t i;

	if ((link->arp_q == q_id) ||
		(link->tcp_syn_q == q_id) ||
		(link->ip_local_q == q_id) ||
		(link->tcp_local_q == q_id) ||
		(link->udp_local_q == q_id) ||
		(link->sctp_local_q == q_id))
		return 1;

	for (i = 0; i < link->n_rss_qs; i++)
		if (link->rss_qs[i] == q_id)
			return 1;

	return 0;
}

static void
check_links(struct app_params *app)
{
	uint32_t i;

	/* Check that number of links matches the port mask */
	if (app->port_mask) {
		uint32_t n_links_port_mask =
			__builtin_popcountll(app->port_mask);

		APP_CHECK((app->n_links == n_links_port_mask),
			"Not enough links provided in the PORT_MASK\n");
	}

	for (i = 0; i < app->n_links; i++) {
		struct app_link_params *link = &app->link_params[i];
		uint32_t rxq_max, n_rxq, n_txq, link_id, i;

		APP_PARAM_GET_ID(link, "LINK", link_id);

		/* Check that link RXQs are contiguous */
		rxq_max = 0;
		if (link->arp_q > rxq_max)
			rxq_max = link->arp_q;
		if (link->tcp_syn_q > rxq_max)
			rxq_max = link->tcp_syn_q;
		if (link->ip_local_q > rxq_max)
			rxq_max = link->ip_local_q;
		if (link->tcp_local_q > rxq_max)
			rxq_max = link->tcp_local_q;
		if (link->udp_local_q > rxq_max)
			rxq_max = link->udp_local_q;
		if (link->sctp_local_q > rxq_max)
			rxq_max = link->sctp_local_q;
		for (i = 0; i < link->n_rss_qs; i++)
			if (link->rss_qs[i] > rxq_max)
				rxq_max = link->rss_qs[i];

		for (i = 1; i <= rxq_max; i++)
			APP_CHECK((link_rxq_used(link, i)),
				"%s RXQs are not contiguous (A)\n", link->name);

		n_rxq = app_link_get_n_rxq(app, link);

		APP_CHECK((n_rxq), "%s does not have any RXQ\n", link->name);

		APP_CHECK((n_rxq == rxq_max + 1),
			"%s RXQs are not contiguous (B)\n", link->name);

		for (i = 0; i < n_rxq; i++) {
			char name[APP_PARAM_NAME_SIZE];
			int pos;

			sprintf(name, "RXQ%" PRIu32 ".%" PRIu32,
				link_id, i);
			pos = APP_PARAM_FIND(app->hwq_in_params, name);
			APP_CHECK((pos >= 0),
				"%s RXQs are not contiguous (C)\n", link->name);
		}

		/* Check that link TXQs are contiguous */
		n_txq = app_link_get_n_txq(app, link);

		APP_CHECK((n_txq),  "%s does not have any TXQ\n", link->name);

		for (i = 0; i < n_txq; i++) {
			char name[APP_PARAM_NAME_SIZE];
			int pos;

			sprintf(name, "TXQ%" PRIu32 ".%" PRIu32,
				link_id, i);
			pos = APP_PARAM_FIND(app->hwq_out_params, name);
			APP_CHECK((pos >= 0),
				"%s TXQs are not contiguous\n", link->name);
		}
	}
}

static void
check_rxqs(struct app_params *app)
{
	uint32_t i;

	for (i = 0; i < app->n_pktq_hwq_in; i++) {
		struct app_pktq_hwq_in_params *p = &app->hwq_in_params[i];
		uint32_t n_readers = app_rxq_get_readers(app, p);

		APP_CHECK((p->size > 0),
			"%s size is 0\n", p->name);

		APP_CHECK((rte_is_power_of_2(p->size)),
			"%s size is not a power of 2\n", p->name);

		APP_CHECK((p->burst > 0),
			"%s burst size is 0\n", p->name);

		APP_CHECK((p->burst <= p->size),
			"%s burst size is bigger than its size\n", p->name);

		APP_CHECK((n_readers != 0),
			"%s has no reader\n", p->name);

		APP_CHECK((n_readers == 1),
			"%s has more than one reader\n", p->name);
	}
}

static void
check_txqs(struct app_params *app)
{
	uint32_t i;

	for (i = 0; i < app->n_pktq_hwq_out; i++) {
		struct app_pktq_hwq_out_params *p = &app->hwq_out_params[i];
		uint32_t n_writers = app_txq_get_writers(app, p);

		APP_CHECK((p->size > 0),
			"%s size is 0\n", p->name);

		APP_CHECK((rte_is_power_of_2(p->size)),
			"%s size is not a power of 2\n", p->name);

		APP_CHECK((p->burst > 0),
			"%s burst size is 0\n", p->name);

		APP_CHECK((p->burst <= p->size),
			"%s burst size is bigger than its size\n", p->name);

		APP_CHECK((n_writers != 0),
			"%s has no writer\n", p->name);

		APP_CHECK((n_writers == 1),
			"%s has more than one writer\n", p->name);
	}
}

static void
check_swqs(struct app_params *app)
{
	uint32_t i;

	for (i = 0; i < app->n_pktq_swq; i++) {
		struct app_pktq_swq_params *p = &app->swq_params[i];
		uint32_t n_readers = app_swq_get_readers(app, p);
		uint32_t n_writers = app_swq_get_writers(app, p);
		uint32_t n_flags;

		APP_CHECK((p->size > 0),
			"%s size is 0\n", p->name);

		APP_CHECK((rte_is_power_of_2(p->size)),
			"%s size is not a power of 2\n", p->name);

		APP_CHECK((p->burst_read > 0),
			"%s read burst size is 0\n", p->name);

		APP_CHECK((p->burst_read <= p->size),
			"%s read burst size is bigger than its size\n",
			p->name);

		APP_CHECK((p->burst_write > 0),
			"%s write burst size is 0\n", p->name);

		APP_CHECK((p->burst_write <= p->size),
			"%s write burst size is bigger than its size\n",
			p->name);

		APP_CHECK((n_readers != 0),
			"%s has no reader\n", p->name);

		if (n_readers > 1)
			APP_LOG(app, LOW, "%s has more than one reader", p->name);

		APP_CHECK((n_writers != 0),
			"%s has no writer\n", p->name);

		if (n_writers > 1)
			APP_LOG(app, LOW, "%s has more than one writer", p->name);

		n_flags = p->ipv4_frag + p->ipv6_frag + p->ipv4_ras + p->ipv6_ras;

		APP_CHECK((n_flags < 2),
			"%s has more than one fragmentation or reassembly mode enabled\n",
			p->name);

		APP_CHECK((!((n_readers > 1) && (n_flags == 1))),
			"%s has more than one reader when fragmentation or reassembly"
			" mode enabled\n",
			p->name);

		APP_CHECK((!((n_writers > 1) && (n_flags == 1))),
			"%s has more than one writer when fragmentation or reassembly"
			" mode enabled\n",
			p->name);

		n_flags = p->ipv4_ras + p->ipv6_ras;

		APP_CHECK((!((p->dropless == 1) && (n_flags == 1))),
			"%s has dropless when reassembly mode enabled\n", p->name);

		n_flags = p->ipv4_frag + p->ipv6_frag;

		if (n_flags == 1) {
			uint16_t ip_hdr_size = (p->ipv4_frag) ? sizeof(struct ipv4_hdr) :
				sizeof(struct ipv6_hdr);

			APP_CHECK((p->mtu > ip_hdr_size),
				"%s mtu size is smaller than ip header\n", p->name);

			APP_CHECK((!((p->mtu - ip_hdr_size) % 8)),
				"%s mtu size is incorrect\n", p->name);
		}
	}
}

static void
check_tms(struct app_params *app)
{
	uint32_t i;

	for (i = 0; i < app->n_pktq_tm; i++) {
		struct app_pktq_tm_params *p = &app->tm_params[i];
		uint32_t n_readers = app_tm_get_readers(app, p);
		uint32_t n_writers = app_tm_get_writers(app, p);

		APP_CHECK((n_readers != 0),
			"%s has no reader\n", p->name);

		APP_CHECK((n_readers == 1),
			"%s has more than one reader\n", p->name);

		APP_CHECK((n_writers != 0),
			"%s has no writer\n", p->name);

		APP_CHECK((n_writers == 1),
			"%s has more than one writer\n", p->name);
	}
}

static void
check_knis(struct app_params *app) {
	uint32_t i;

	for (i = 0; i < app->n_pktq_kni; i++) {
		struct app_pktq_kni_params *p = &app->kni_params[i];
		uint32_t n_readers = app_kni_get_readers(app, p);
		uint32_t n_writers = app_kni_get_writers(app, p);

		APP_CHECK((n_readers != 0),
			"%s has no reader\n", p->name);

		APP_CHECK((n_readers == 1),
			"%s has more than one reader\n", p->name);

		APP_CHECK((n_writers != 0),
			"%s has no writer\n", p->name);

		APP_CHECK((n_writers == 1),
			"%s has more than one writer\n", p->name);
	}
}

static void
check_sources(struct app_params *app)
{
	uint32_t i;

	for (i = 0; i < app->n_pktq_source; i++) {
		struct app_pktq_source_params *p = &app->source_params[i];
		uint32_t n_readers = app_source_get_readers(app, p);

		APP_CHECK((n_readers != 0),
			"%s has no reader\n", p->name);

		APP_CHECK((n_readers == 1),
			"%s has more than one reader\n", p->name);
	}
}

static void
check_sinks(struct app_params *app)
{
	uint32_t i;

	for (i = 0; i < app->n_pktq_sink; i++) {
		struct app_pktq_sink_params *p = &app->sink_params[i];
		uint32_t n_writers = app_sink_get_writers(app, p);

		APP_CHECK((n_writers != 0),
			"%s has no writer\n", p->name);

		APP_CHECK((n_writers == 1),
			"%s has more than one writer\n", p->name);
	}
}

static void
check_msgqs(struct app_params *app)
{
	uint32_t i;

	for (i = 0; i < app->n_msgq; i++) {
		struct app_msgq_params *p = &app->msgq_params[i];
		uint32_t n_readers = app_msgq_get_readers(app, p);
		uint32_t n_writers = app_msgq_get_writers(app, p);
		uint32_t msgq_req_pipeline, msgq_rsp_pipeline;
		uint32_t msgq_req_core, msgq_rsp_core;

		APP_CHECK((p->size > 0),
			"%s size is 0\n", p->name);

		APP_CHECK((rte_is_power_of_2(p->size)),
			"%s size is not a power of 2\n", p->name);

		msgq_req_pipeline = (strncmp(p->name, "MSGQ-REQ-PIPELINE",
			strlen("MSGQ-REQ-PIPELINE")) == 0);

		msgq_rsp_pipeline = (strncmp(p->name, "MSGQ-RSP-PIPELINE",
			strlen("MSGQ-RSP-PIPELINE")) == 0);

		msgq_req_core = (strncmp(p->name, "MSGQ-REQ-CORE",
			strlen("MSGQ-REQ-CORE")) == 0);

		msgq_rsp_core = (strncmp(p->name, "MSGQ-RSP-CORE",
			strlen("MSGQ-RSP-CORE")) == 0);

		if ((msgq_req_pipeline == 0) &&
			(msgq_rsp_pipeline == 0) &&
			(msgq_req_core == 0) &&
			(msgq_rsp_core == 0)) {
			APP_CHECK((n_readers != 0),
				"%s has no reader\n", p->name);

			APP_CHECK((n_readers == 1),
				"%s has more than one reader\n", p->name);

			APP_CHECK((n_writers != 0),
				"%s has no writer\n", p->name);

			APP_CHECK((n_writers == 1),
				"%s has more than one writer\n", p->name);
		}

		if (msgq_req_pipeline) {
			struct app_pipeline_params *pipeline;
			uint32_t pipeline_id;

			APP_PARAM_GET_ID(p, "MSGQ-REQ-PIPELINE", pipeline_id);

			APP_PARAM_FIND_BY_ID(app->pipeline_params,
				"PIPELINE",
				pipeline_id,
				pipeline);

			APP_CHECK((pipeline != NULL),
				"%s is not associated with a valid pipeline\n",
				p->name);
		}

		if (msgq_rsp_pipeline) {
			struct app_pipeline_params *pipeline;
			uint32_t pipeline_id;

			APP_PARAM_GET_ID(p, "MSGQ-RSP-PIPELINE", pipeline_id);

			APP_PARAM_FIND_BY_ID(app->pipeline_params,
				"PIPELINE",
				pipeline_id,
				pipeline);

			APP_CHECK((pipeline != NULL),
				"%s is not associated with a valid pipeline\n",
				p->name);
		}
	}
}

static void
check_pipelines(struct app_params *app)
{
	uint32_t i;

	for (i = 0; i < app->n_pipelines; i++) {
		struct app_pipeline_params *p = &app->pipeline_params[i];

		APP_CHECK((p->n_msgq_in == p->n_msgq_out),
			"%s number of input MSGQs does not match "
			"the number of output MSGQs\n", p->name);
	}
}

int
app_config_check(struct app_params *app)
{
	check_mempools(app);
	check_links(app);
	check_rxqs(app);
	check_txqs(app);
	check_swqs(app);
	check_tms(app);
	check_knis(app);
	check_sources(app);
	check_sinks(app);
	check_msgqs(app);
	check_pipelines(app);

	return 0;
}
