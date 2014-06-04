/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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
#include <stdlib.h>
#include <stdint.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>

#include <rte_port_ethdev.h>
#include <rte_port_ring.h>
#include <rte_table_stub.h>
#include <rte_pipeline.h>

#include "main.h"

static struct ether_addr local_ether_addr = {
	.addr_bytes = {0, 1, 2, 3, 4, 5},
};

static inline void
app_pkt_metadata_flush(struct rte_mbuf *pkt)
{
	struct app_pkt_metadata *pkt_meta = (struct app_pkt_metadata *)
		RTE_MBUF_METADATA_UINT8_PTR(pkt, 0);
	struct ether_hdr *ether_hdr = (struct ether_hdr *)
		rte_pktmbuf_prepend(pkt, (uint16_t) sizeof(struct ether_hdr));

	ether_addr_copy(&pkt_meta->nh_arp, &ether_hdr->d_addr);
	ether_addr_copy(&local_ether_addr, &ether_hdr->s_addr);
	ether_hdr->ether_type = rte_bswap16(ETHER_TYPE_IPv4);
	pkt->pkt.vlan_macip.f.l2_len = sizeof(struct ether_hdr);
}

static int
app_pipeline_tx_port_in_action_handler(
	struct rte_mbuf **pkts,
	uint32_t n,
	uint64_t *pkts_mask,
	__rte_unused void *arg)
{
	uint32_t i;

	for (i = 0; i < n; i++) {
		struct rte_mbuf *m = pkts[i];

		app_pkt_metadata_flush(m);
	}

	*pkts_mask = (~0LLU) >> (64 - n);

	return 0;
}

void
app_main_loop_pipeline_tx(void) {
	struct rte_pipeline *p;
	uint32_t port_in_id[APP_MAX_PORTS];
	uint32_t port_out_id[APP_MAX_PORTS];
	uint32_t table_id[APP_MAX_PORTS];
	uint32_t i;

	uint32_t core_id = rte_lcore_id();
	struct app_core_params *core_params = app_get_core_params(core_id);

	if ((core_params == NULL) || (core_params->core_type != APP_CORE_TX))
		rte_panic("Core %u misconfiguration\n", core_id);

	RTE_LOG(INFO, USER1, "Core %u is doing TX\n", core_id);

	/* Pipeline configuration */
	struct rte_pipeline_params pipeline_params = {
		.name = "pipeline",
		.socket_id = rte_socket_id(),
	};

	p = rte_pipeline_create(&pipeline_params);
	if (p == NULL)
		rte_panic("%s: Unable to configure the pipeline\n", __func__);

	/* Input port configuration */
	for (i = 0; i < app.n_ports; i++) {
		struct rte_port_ring_reader_params port_ring_params = {
			.ring = app.rings[core_params->swq_in[i]],
		};

		struct rte_pipeline_port_in_params port_params = {
			.ops = &rte_port_ring_reader_ops,
			.arg_create = (void *) &port_ring_params,
			.f_action = (app.ether_hdr_pop_push) ?
				app_pipeline_tx_port_in_action_handler : NULL,
			.arg_ah = NULL,
			.burst_size = app.bsz_swq_rd,
		};

		if (rte_pipeline_port_in_create(p, &port_params,
			&port_in_id[i])) {
			rte_panic("%s: Unable to configure input port for "
				"ring TX %i\n", __func__, i);
		}
	}

	/* Output port configuration */
	for (i = 0; i < app.n_ports; i++) {
		struct rte_port_ethdev_writer_params port_ethdev_params = {
			.port_id = app.ports[i],
			.queue_id = 0,
			.tx_burst_sz = app.bsz_hwq_wr,
		};

		struct rte_pipeline_port_out_params port_params = {
			.ops = &rte_port_ethdev_writer_ops,
			.arg_create = (void *) &port_ethdev_params,
			.f_action = NULL,
			.f_action_bulk = NULL,
			.arg_ah = NULL,
		};

		if (rte_pipeline_port_out_create(p, &port_params,
			&port_out_id[i])) {
			rte_panic("%s: Unable to configure output port for "
				"port %d\n", __func__, app.ports[i]);
		}
	}

	/* Table configuration */
	for (i = 0; i < app.n_ports; i++) {
		struct rte_pipeline_table_params table_params = {
			.ops = &rte_table_stub_ops,
			.arg_create = NULL,
			.f_action_hit = NULL,
			.f_action_miss = NULL,
			.arg_ah = NULL,
			.action_data_size = 0,
		};

		if (rte_pipeline_table_create(p, &table_params, &table_id[i])) {
			rte_panic("%s: Unable to configure table %u\n",
				__func__, table_id[i]);
		}
	}

	/* Interconnecting ports and tables */
	for (i = 0; i < app.n_ports; i++)
		if (rte_pipeline_port_in_connect_to_table(p, port_in_id[i],
			table_id[i]))
			rte_panic("%s: Unable to connect input port %u to "
				"table %u\n", __func__, port_in_id[i],
				table_id[i]);

	/* Add entries to tables */
	for (i = 0; i < app.n_ports; i++) {
		struct rte_pipeline_table_entry default_entry = {
			.action = RTE_PIPELINE_ACTION_PORT,
			{.port_id = port_out_id[i]},
		};

		struct rte_pipeline_table_entry *default_entry_ptr;

		if (rte_pipeline_table_default_entry_add(p, table_id[i],
			&default_entry, &default_entry_ptr))
			rte_panic("%s: Unable to add default entry to "
				"table %u\n", __func__, table_id[i]);
	}

	/* Enable input ports */
	for (i = 0; i < app.n_ports; i++)
		if (rte_pipeline_port_in_enable(p, port_in_id[i]))
			rte_panic("Unable to enable input port %u\n",
				port_in_id[i]);

	/* Check pipeline consistency */
	if (rte_pipeline_check(p) < 0)
		rte_panic("%s: Pipeline consistency check failed\n", __func__);

	/* Run-time */
	for (i = 0; ; i++) {
		rte_pipeline_run(p);

		if ((i & APP_FLUSH) == 0)
			rte_pipeline_flush(p);
	}
}

void
app_main_loop_tx(void) {
	struct app_mbuf_array *m[APP_MAX_PORTS];
	uint32_t i;

	uint32_t core_id = rte_lcore_id();
	struct app_core_params *core_params = app_get_core_params(core_id);

	if ((core_params == NULL) || (core_params->core_type != APP_CORE_TX))
		rte_panic("Core %u misconfiguration\n", core_id);

	RTE_LOG(INFO, USER1, "Core %u is doing TX (no pipeline)\n", core_id);

	for (i = 0; i < APP_MAX_PORTS; i++) {
		m[i] = rte_malloc_socket(NULL, sizeof(struct app_mbuf_array),
			CACHE_LINE_SIZE, rte_socket_id());
		if (m[i] == NULL)
			rte_panic("%s: Cannot allocate buffer space\n",
				__func__);
	}

	for (i = 0; ; i = ((i + 1) & (app.n_ports - 1))) {
		uint32_t n_mbufs, n_pkts;
		int ret;

		n_mbufs = m[i]->n_mbufs;

		ret = rte_ring_sc_dequeue_bulk(
			app.rings[core_params->swq_in[i]],
			(void **) &m[i]->array[n_mbufs],
			app.bsz_swq_rd);

		if (ret == -ENOENT)
			continue;

		n_mbufs += app.bsz_swq_rd;

		if (n_mbufs < app.bsz_hwq_wr) {
			m[i]->n_mbufs = n_mbufs;
			continue;
		}

		n_pkts = rte_eth_tx_burst(
			app.ports[i],
			0,
			m[i]->array,
			n_mbufs);

		if (n_pkts < n_mbufs) {
			uint32_t k;

			for (k = n_pkts; k < n_mbufs; k++) {
				struct rte_mbuf *pkt_to_free;

				pkt_to_free = m[i]->array[k];
				rte_pktmbuf_free(pkt_to_free);
			}
		}

		m[i]->n_mbufs = 0;
	}
}
