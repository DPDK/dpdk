/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */
#include <stdio.h>
#include <sys/stat.h>

#include <rte_cycles.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_flow.h>
#include <rte_meter.h>
#include <rte_eth_softnic.h>
#include <rte_tm.h>

#include "testpmd.h"

#define SUBPORT_NODES_PER_PORT		1
#define PIPE_NODES_PER_SUBPORT		4096
#define TC_NODES_PER_PIPE			4
#define QUEUE_NODES_PER_TC			4

#define NUM_PIPE_NODES						\
	(SUBPORT_NODES_PER_PORT * PIPE_NODES_PER_SUBPORT)

#define NUM_TC_NODES						\
	(NUM_PIPE_NODES * TC_NODES_PER_PIPE)

#define ROOT_NODE_ID				1000000
#define SUBPORT_NODES_START_ID		900000
#define PIPE_NODES_START_ID			800000
#define TC_NODES_START_ID			700000

#define STATS_MASK_DEFAULT					\
	(RTE_TM_STATS_N_PKTS |					\
	RTE_TM_STATS_N_BYTES |					\
	RTE_TM_STATS_N_PKTS_GREEN_DROPPED |			\
	RTE_TM_STATS_N_BYTES_GREEN_DROPPED)

#define STATS_MASK_QUEUE					\
	(STATS_MASK_DEFAULT |					\
	RTE_TM_STATS_N_PKTS_QUEUED)

#define BYTES_IN_MBPS				(1000 * 1000 / 8)
#define TOKEN_BUCKET_SIZE			1000000

/* TM Hierarchy Levels */
enum tm_hierarchy_level {
	TM_NODE_LEVEL_PORT = 0,
	TM_NODE_LEVEL_SUBPORT,
	TM_NODE_LEVEL_PIPE,
	TM_NODE_LEVEL_TC,
	TM_NODE_LEVEL_QUEUE,
	TM_NODE_LEVEL_MAX,
};

struct tm_hierarchy {
	/* TM Nodes */
	uint32_t root_node_id;
	uint32_t subport_node_id[SUBPORT_NODES_PER_PORT];
	uint32_t pipe_node_id[SUBPORT_NODES_PER_PORT][PIPE_NODES_PER_SUBPORT];
	uint32_t tc_node_id[NUM_PIPE_NODES][TC_NODES_PER_PIPE];
	uint32_t queue_node_id[NUM_TC_NODES][QUEUE_NODES_PER_TC];

	/* TM Hierarchy Nodes Shaper Rates */
	uint32_t root_node_shaper_rate;
	uint32_t subport_node_shaper_rate;
	uint32_t pipe_node_shaper_rate;
	uint32_t tc_node_shaper_rate;
	uint32_t tc_node_shared_shaper_rate;

	uint32_t n_shapers;
};

#define BITFIELD(byte_array, slab_pos, slab_mask, slab_shr)	\
({								\
	uint64_t slab = *((uint64_t *) &byte_array[slab_pos]);	\
	uint64_t val =				\
		(rte_be_to_cpu_64(slab) & slab_mask) >> slab_shr;	\
	val;						\
})

#define RTE_SCHED_PORT_HIERARCHY(subport, pipe,           \
	traffic_class, queue, color)                          \
	((((uint64_t) (queue)) & 0x3) |                       \
	((((uint64_t) (traffic_class)) & 0x3) << 2) |         \
	((((uint64_t) (color)) & 0x3) << 4) |                 \
	((((uint64_t) (subport)) & 0xFFFF) << 16) |           \
	((((uint64_t) (pipe)) & 0xFFFFFFFF) << 32))


static void
pkt_metadata_set(struct rte_port *p, struct rte_mbuf **pkts,
	uint32_t n_pkts)
{
	struct softnic_port_tm *tm = &p->softport.tm;
	uint32_t i;

	for (i = 0; i < (n_pkts & (~0x3)); i += 4) {
		struct rte_mbuf *pkt0 = pkts[i];
		struct rte_mbuf *pkt1 = pkts[i + 1];
		struct rte_mbuf *pkt2 = pkts[i + 2];
		struct rte_mbuf *pkt3 = pkts[i + 3];

		uint8_t *pkt0_data = rte_pktmbuf_mtod(pkt0, uint8_t *);
		uint8_t *pkt1_data = rte_pktmbuf_mtod(pkt1, uint8_t *);
		uint8_t *pkt2_data = rte_pktmbuf_mtod(pkt2, uint8_t *);
		uint8_t *pkt3_data = rte_pktmbuf_mtod(pkt3, uint8_t *);

		uint64_t pkt0_subport = BITFIELD(pkt0_data,
					tm->tm_pktfield0_slabpos,
					tm->tm_pktfield0_slabmask,
					tm->tm_pktfield0_slabshr);
		uint64_t pkt0_pipe = BITFIELD(pkt0_data,
					tm->tm_pktfield1_slabpos,
					tm->tm_pktfield1_slabmask,
					tm->tm_pktfield1_slabshr);
		uint64_t pkt0_dscp = BITFIELD(pkt0_data,
					tm->tm_pktfield2_slabpos,
					tm->tm_pktfield2_slabmask,
					tm->tm_pktfield2_slabshr);
		uint32_t pkt0_tc = tm->tm_tc_table[pkt0_dscp & 0x3F] >> 2;
		uint32_t pkt0_tc_q = tm->tm_tc_table[pkt0_dscp & 0x3F] & 0x3;
		uint64_t pkt1_subport = BITFIELD(pkt1_data,
					tm->tm_pktfield0_slabpos,
					tm->tm_pktfield0_slabmask,
					tm->tm_pktfield0_slabshr);
		uint64_t pkt1_pipe = BITFIELD(pkt1_data,
					tm->tm_pktfield1_slabpos,
					tm->tm_pktfield1_slabmask,
					tm->tm_pktfield1_slabshr);
		uint64_t pkt1_dscp = BITFIELD(pkt1_data,
					tm->tm_pktfield2_slabpos,
					tm->tm_pktfield2_slabmask,
					tm->tm_pktfield2_slabshr);
		uint32_t pkt1_tc = tm->tm_tc_table[pkt1_dscp & 0x3F] >> 2;
		uint32_t pkt1_tc_q = tm->tm_tc_table[pkt1_dscp & 0x3F] & 0x3;

		uint64_t pkt2_subport = BITFIELD(pkt2_data,
					tm->tm_pktfield0_slabpos,
					tm->tm_pktfield0_slabmask,
					tm->tm_pktfield0_slabshr);
		uint64_t pkt2_pipe = BITFIELD(pkt2_data,
					tm->tm_pktfield1_slabpos,
					tm->tm_pktfield1_slabmask,
					tm->tm_pktfield1_slabshr);
		uint64_t pkt2_dscp = BITFIELD(pkt2_data,
					tm->tm_pktfield2_slabpos,
					tm->tm_pktfield2_slabmask,
					tm->tm_pktfield2_slabshr);
		uint32_t pkt2_tc = tm->tm_tc_table[pkt2_dscp & 0x3F] >> 2;
		uint32_t pkt2_tc_q = tm->tm_tc_table[pkt2_dscp & 0x3F] & 0x3;

		uint64_t pkt3_subport = BITFIELD(pkt3_data,
					tm->tm_pktfield0_slabpos,
					tm->tm_pktfield0_slabmask,
					tm->tm_pktfield0_slabshr);
		uint64_t pkt3_pipe = BITFIELD(pkt3_data,
					tm->tm_pktfield1_slabpos,
					tm->tm_pktfield1_slabmask,
					tm->tm_pktfield1_slabshr);
		uint64_t pkt3_dscp = BITFIELD(pkt3_data,
					tm->tm_pktfield2_slabpos,
					tm->tm_pktfield2_slabmask,
					tm->tm_pktfield2_slabshr);
		uint32_t pkt3_tc = tm->tm_tc_table[pkt3_dscp & 0x3F] >> 2;
		uint32_t pkt3_tc_q = tm->tm_tc_table[pkt3_dscp & 0x3F] & 0x3;

		uint64_t pkt0_sched = RTE_SCHED_PORT_HIERARCHY(pkt0_subport,
						pkt0_pipe,
						pkt0_tc,
						pkt0_tc_q,
						0);
		uint64_t pkt1_sched = RTE_SCHED_PORT_HIERARCHY(pkt1_subport,
						pkt1_pipe,
						pkt1_tc,
						pkt1_tc_q,
						0);
		uint64_t pkt2_sched = RTE_SCHED_PORT_HIERARCHY(pkt2_subport,
						pkt2_pipe,
						pkt2_tc,
						pkt2_tc_q,
						0);
		uint64_t pkt3_sched = RTE_SCHED_PORT_HIERARCHY(pkt3_subport,
						pkt3_pipe,
						pkt3_tc,
						pkt3_tc_q,
						0);

		pkt0->hash.sched.lo = pkt0_sched & 0xFFFFFFFF;
		pkt0->hash.sched.hi = pkt0_sched >> 32;
		pkt1->hash.sched.lo = pkt1_sched & 0xFFFFFFFF;
		pkt1->hash.sched.hi = pkt1_sched >> 32;
		pkt2->hash.sched.lo = pkt2_sched & 0xFFFFFFFF;
		pkt2->hash.sched.hi = pkt2_sched >> 32;
		pkt3->hash.sched.lo = pkt3_sched & 0xFFFFFFFF;
		pkt3->hash.sched.hi = pkt3_sched >> 32;
	}

	for (; i < n_pkts; i++)	{
		struct rte_mbuf *pkt = pkts[i];

		uint8_t *pkt_data = rte_pktmbuf_mtod(pkt, uint8_t *);

		uint64_t pkt_subport = BITFIELD(pkt_data,
					tm->tm_pktfield0_slabpos,
					tm->tm_pktfield0_slabmask,
					tm->tm_pktfield0_slabshr);
		uint64_t pkt_pipe = BITFIELD(pkt_data,
					tm->tm_pktfield1_slabpos,
					tm->tm_pktfield1_slabmask,
					tm->tm_pktfield1_slabshr);
		uint64_t pkt_dscp = BITFIELD(pkt_data,
					tm->tm_pktfield2_slabpos,
					tm->tm_pktfield2_slabmask,
					tm->tm_pktfield2_slabshr);
		uint32_t pkt_tc = tm->tm_tc_table[pkt_dscp & 0x3F] >> 2;
		uint32_t pkt_tc_q = tm->tm_tc_table[pkt_dscp & 0x3F] & 0x3;

		uint64_t pkt_sched = RTE_SCHED_PORT_HIERARCHY(pkt_subport,
						pkt_pipe,
						pkt_tc,
						pkt_tc_q,
						0);

		pkt->hash.sched.lo = pkt_sched & 0xFFFFFFFF;
		pkt->hash.sched.hi = pkt_sched >> 32;
	}
}

/*
 * Soft port packet forward
 */
static void
softport_packet_fwd(struct fwd_stream *fs)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_port *rte_tx_port = &ports[fs->tx_port];
	uint16_t nb_rx;
	uint16_t nb_tx;
	uint32_t retry;

#ifdef RTE_TEST_PMD_RECORD_CORE_CYCLES
	uint64_t start_tsc;
	uint64_t end_tsc;
	uint64_t core_cycles;
#endif

#ifdef RTE_TEST_PMD_RECORD_CORE_CYCLES
	start_tsc = rte_rdtsc();
#endif

	/*  Packets Receive */
	nb_rx = rte_eth_rx_burst(fs->rx_port, fs->rx_queue,
			pkts_burst, nb_pkt_per_burst);
	fs->rx_packets += nb_rx;

#ifdef RTE_TEST_PMD_RECORD_BURST_STATS
	fs->rx_burst_stats.pkt_burst_spread[nb_rx]++;
#endif

	if (rte_tx_port->softnic_enable) {
		/* Set packet metadata if tm flag enabled */
		if (rte_tx_port->softport.tm_flag)
			pkt_metadata_set(rte_tx_port, pkts_burst, nb_rx);

		/* Softport run */
		rte_pmd_softnic_run(fs->tx_port);
	}
	nb_tx = rte_eth_tx_burst(fs->tx_port, fs->tx_queue,
			pkts_burst, nb_rx);

	/* Retry if necessary */
	if (unlikely(nb_tx < nb_rx) && fs->retry_enabled) {
		retry = 0;
		while (nb_tx < nb_rx && retry++ < burst_tx_retry_num) {
			rte_delay_us(burst_tx_delay_time);
			nb_tx += rte_eth_tx_burst(fs->tx_port, fs->tx_queue,
					&pkts_burst[nb_tx], nb_rx - nb_tx);
		}
	}
	fs->tx_packets += nb_tx;

#ifdef RTE_TEST_PMD_RECORD_BURST_STATS
	fs->tx_burst_stats.pkt_burst_spread[nb_tx]++;
#endif

	if (unlikely(nb_tx < nb_rx)) {
		fs->fwd_dropped += (nb_rx - nb_tx);
		do {
			rte_pktmbuf_free(pkts_burst[nb_tx]);
		} while (++nb_tx < nb_rx);
	}
#ifdef RTE_TEST_PMD_RECORD_CORE_CYCLES
	end_tsc = rte_rdtsc();
	core_cycles = (end_tsc - start_tsc);
	fs->core_cycles = (uint64_t) (fs->core_cycles + core_cycles);
#endif
}

static void
set_tm_hiearchy_nodes_shaper_rate(portid_t port_id, struct tm_hierarchy *h)
{
	struct rte_eth_link link_params;
	uint64_t tm_port_rate;

	memset(&link_params, 0, sizeof(link_params));

	rte_eth_link_get(port_id, &link_params);
	tm_port_rate = (uint64_t)link_params.link_speed * BYTES_IN_MBPS;

	if (tm_port_rate > UINT32_MAX)
		tm_port_rate = UINT32_MAX;

	/* Set tm hierarchy shapers rate */
	h->root_node_shaper_rate = tm_port_rate;
	h->subport_node_shaper_rate =
		tm_port_rate / SUBPORT_NODES_PER_PORT;
	h->pipe_node_shaper_rate
		= h->subport_node_shaper_rate / PIPE_NODES_PER_SUBPORT;
	h->tc_node_shaper_rate = h->pipe_node_shaper_rate;
	h->tc_node_shared_shaper_rate = h->subport_node_shaper_rate;
}

static int
softport_tm_root_node_add(portid_t port_id, struct tm_hierarchy *h,
	struct rte_tm_error *error)
{
	struct rte_tm_node_params rnp;
	struct rte_tm_shaper_params rsp;
	uint32_t priority, weight, level_id, shaper_profile_id;

	memset(&rsp, 0, sizeof(struct rte_tm_shaper_params));
	memset(&rnp, 0, sizeof(struct rte_tm_node_params));

	/* Shaper profile Parameters */
	rsp.peak.rate = h->root_node_shaper_rate;
	rsp.peak.size = TOKEN_BUCKET_SIZE;
	rsp.pkt_length_adjust = RTE_TM_ETH_FRAMING_OVERHEAD_FCS;
	shaper_profile_id = 0;

	if (rte_tm_shaper_profile_add(port_id, shaper_profile_id,
		&rsp, error)) {
		printf("%s ERROR(%d)-%s!(shaper_id %u)\n ",
			__func__, error->type, error->message,
			shaper_profile_id);
		return -1;
	}

	/* Root Node Parameters */
	h->root_node_id = ROOT_NODE_ID;
	weight = 1;
	priority = 0;
	level_id = TM_NODE_LEVEL_PORT;
	rnp.shaper_profile_id = shaper_profile_id;
	rnp.nonleaf.n_sp_priorities = 1;
	rnp.stats_mask = STATS_MASK_DEFAULT;

	/* Add Node to TM Hierarchy */
	if (rte_tm_node_add(port_id, h->root_node_id, RTE_TM_NODE_ID_NULL,
		priority, weight, level_id, &rnp, error)) {
		printf("%s ERROR(%d)-%s!(node_id %u, parent_id %u, level %u)\n",
			__func__, error->type, error->message,
			h->root_node_id, RTE_TM_NODE_ID_NULL,
			level_id);
		return -1;
	}
	/* Update */
	h->n_shapers++;

	printf("  Root node added (Start id %u, Count %u, level %u)\n",
		h->root_node_id, 1, level_id);

	return 0;
}

static int
softport_tm_subport_node_add(portid_t port_id, struct tm_hierarchy *h,
	struct rte_tm_error *error)
{
	uint32_t subport_parent_node_id, subport_node_id = 0;
	struct rte_tm_node_params snp;
	struct rte_tm_shaper_params ssp;
	uint32_t priority, weight, level_id, shaper_profile_id;
	uint32_t i;

	memset(&ssp, 0, sizeof(struct rte_tm_shaper_params));
	memset(&snp, 0, sizeof(struct rte_tm_node_params));

	shaper_profile_id = h->n_shapers;

	/* Add Shaper Profile to TM Hierarchy */
	for (i = 0; i < SUBPORT_NODES_PER_PORT; i++) {
		ssp.peak.rate = h->subport_node_shaper_rate;
		ssp.peak.size = TOKEN_BUCKET_SIZE;
		ssp.pkt_length_adjust = RTE_TM_ETH_FRAMING_OVERHEAD_FCS;

		if (rte_tm_shaper_profile_add(port_id, shaper_profile_id,
			&ssp, error)) {
			printf("%s ERROR(%d)-%s!(shaper_id %u)\n ",
				__func__, error->type, error->message,
				shaper_profile_id);
			return -1;
		}

		/* Node Parameters */
		h->subport_node_id[i] = SUBPORT_NODES_START_ID + i;
		subport_parent_node_id = h->root_node_id;
		weight = 1;
		priority = 0;
		level_id = TM_NODE_LEVEL_SUBPORT;
		snp.shaper_profile_id = shaper_profile_id;
		snp.nonleaf.n_sp_priorities = 1;
		snp.stats_mask = STATS_MASK_DEFAULT;

		/* Add Node to TM Hiearchy */
		if (rte_tm_node_add(port_id,
				h->subport_node_id[i],
				subport_parent_node_id,
				priority, weight,
				level_id,
				&snp,
				error)) {
			printf("%s ERROR(%d)-%s!(node %u,parent %u,level %u)\n",
					__func__,
					error->type,
					error->message,
					h->subport_node_id[i],
					subport_parent_node_id,
					level_id);
			return -1;
		}
		shaper_profile_id++;
		subport_node_id++;
	}
	/* Update */
	h->n_shapers = shaper_profile_id;

	printf("  Subport nodes added (Start id %u, Count %u, level %u)\n",
		h->subport_node_id[0], SUBPORT_NODES_PER_PORT, level_id);

	return 0;
}

static int
softport_tm_pipe_node_add(portid_t port_id, struct tm_hierarchy *h,
	struct rte_tm_error *error)
{
	uint32_t pipe_parent_node_id;
	struct rte_tm_node_params pnp;
	struct rte_tm_shaper_params psp;
	uint32_t priority, weight, level_id, shaper_profile_id;
	uint32_t i, j;

	memset(&psp, 0, sizeof(struct rte_tm_shaper_params));
	memset(&pnp, 0, sizeof(struct rte_tm_node_params));

	shaper_profile_id = h->n_shapers;

	/* Shaper Profile Parameters */
	psp.peak.rate = h->pipe_node_shaper_rate;
	psp.peak.size = TOKEN_BUCKET_SIZE;
	psp.pkt_length_adjust = RTE_TM_ETH_FRAMING_OVERHEAD_FCS;

	/* Pipe Node Parameters */
	weight = 1;
	priority = 0;
	level_id = TM_NODE_LEVEL_PIPE;
	pnp.nonleaf.n_sp_priorities = 4;
	pnp.stats_mask = STATS_MASK_DEFAULT;

	/* Add Shaper Profiles and Nodes to TM Hierarchy */
	for (i = 0; i < SUBPORT_NODES_PER_PORT; i++) {
		for (j = 0; j < PIPE_NODES_PER_SUBPORT; j++) {
			if (rte_tm_shaper_profile_add(port_id,
				shaper_profile_id, &psp, error)) {
				printf("%s ERROR(%d)-%s!(shaper_id %u)\n ",
					__func__, error->type, error->message,
					shaper_profile_id);
				return -1;
			}
			pnp.shaper_profile_id = shaper_profile_id;
			pipe_parent_node_id = h->subport_node_id[i];
			h->pipe_node_id[i][j] = PIPE_NODES_START_ID +
				(i * PIPE_NODES_PER_SUBPORT) + j;

			if (rte_tm_node_add(port_id,
					h->pipe_node_id[i][j],
					pipe_parent_node_id,
					priority, weight, level_id,
					&pnp,
					error)) {
				printf("%s ERROR(%d)-%s!(node %u,parent %u )\n",
					__func__,
					error->type,
					error->message,
					h->pipe_node_id[i][j],
					pipe_parent_node_id);

				return -1;
			}
			shaper_profile_id++;
		}
	}
	/* Update */
	h->n_shapers = shaper_profile_id;

	printf("  Pipe nodes added (Start id %u, Count %u, level %u)\n",
		h->pipe_node_id[0][0], NUM_PIPE_NODES, level_id);

	return 0;
}

static int
softport_tm_tc_node_add(portid_t port_id, struct tm_hierarchy *h,
	struct rte_tm_error *error)
{
	uint32_t tc_parent_node_id;
	struct rte_tm_node_params tnp;
	struct rte_tm_shaper_params tsp, tssp;
	uint32_t shared_shaper_profile_id[TC_NODES_PER_PIPE];
	uint32_t priority, weight, level_id, shaper_profile_id;
	uint32_t pos, n_tc_nodes, i, j, k;

	memset(&tsp, 0, sizeof(struct rte_tm_shaper_params));
	memset(&tssp, 0, sizeof(struct rte_tm_shaper_params));
	memset(&tnp, 0, sizeof(struct rte_tm_node_params));

	shaper_profile_id = h->n_shapers;

	/* Private Shaper Profile (TC) Parameters */
	tsp.peak.rate = h->tc_node_shaper_rate;
	tsp.peak.size = TOKEN_BUCKET_SIZE;
	tsp.pkt_length_adjust = RTE_TM_ETH_FRAMING_OVERHEAD_FCS;

	/* Shared Shaper Profile (TC) Parameters */
	tssp.peak.rate = h->tc_node_shared_shaper_rate;
	tssp.peak.size = TOKEN_BUCKET_SIZE;
	tssp.pkt_length_adjust = RTE_TM_ETH_FRAMING_OVERHEAD_FCS;

	/* TC Node Parameters */
	weight = 1;
	level_id = TM_NODE_LEVEL_TC;
	tnp.n_shared_shapers = 1;
	tnp.nonleaf.n_sp_priorities = 1;
	tnp.stats_mask = STATS_MASK_DEFAULT;

	/* Add Shared Shaper Profiles to TM Hierarchy */
	for (i = 0; i < TC_NODES_PER_PIPE; i++) {
		shared_shaper_profile_id[i] = shaper_profile_id;

		if (rte_tm_shaper_profile_add(port_id,
			shared_shaper_profile_id[i], &tssp, error)) {
			printf("%s ERROR(%d)-%s!(Shared shaper profileid %u)\n",
				__func__, error->type, error->message,
				shared_shaper_profile_id[i]);

			return -1;
		}
		if (rte_tm_shared_shaper_add_update(port_id,  i,
			shared_shaper_profile_id[i], error)) {
			printf("%s ERROR(%d)-%s!(Shared shaper id %u)\n",
				__func__, error->type, error->message, i);

			return -1;
		}
		shaper_profile_id++;
	}

	/* Add Shaper Profiles and Nodes to TM Hierarchy */
	n_tc_nodes = 0;
	for (i = 0; i < SUBPORT_NODES_PER_PORT; i++) {
		for (j = 0; j < PIPE_NODES_PER_SUBPORT; j++) {
			for (k = 0; k < TC_NODES_PER_PIPE ; k++) {
				priority = k;
				tc_parent_node_id = h->pipe_node_id[i][j];
				tnp.shared_shaper_id =
					(uint32_t *)calloc(1, sizeof(uint32_t));
				if (tnp.shared_shaper_id == NULL) {
					printf("Shared shaper mem alloc err\n");
					return -1;
				}
				tnp.shared_shaper_id[0] = k;
				pos = j + (i * PIPE_NODES_PER_SUBPORT);
				h->tc_node_id[pos][k] =
					TC_NODES_START_ID + n_tc_nodes;

				if (rte_tm_shaper_profile_add(port_id,
					shaper_profile_id, &tsp, error)) {
					printf("%s ERROR(%d)-%s!(shaper %u)\n",
						__func__, error->type,
						error->message,
						shaper_profile_id);

					return -1;
				}
				tnp.shaper_profile_id = shaper_profile_id;
				if (rte_tm_node_add(port_id,
						h->tc_node_id[pos][k],
						tc_parent_node_id,
						priority, weight,
						level_id,
						&tnp, error)) {
					printf("%s ERROR(%d)-%s!(node id %u)\n",
						__func__,
						error->type,
						error->message,
						h->tc_node_id[pos][k]);

					return -1;
				}
				shaper_profile_id++;
				n_tc_nodes++;
			}
		}
	}
	/* Update */
	h->n_shapers = shaper_profile_id;

	printf("  TC nodes added (Start id %u, Count %u, level %u)\n",
		h->tc_node_id[0][0], n_tc_nodes, level_id);

	return 0;
}

static int
softport_tm_queue_node_add(portid_t port_id, struct tm_hierarchy *h,
	struct rte_tm_error *error)
{
	uint32_t queue_parent_node_id;
	struct rte_tm_node_params qnp;
	uint32_t priority, weight, level_id, pos;
	uint32_t n_queue_nodes, i, j, k;

	memset(&qnp, 0, sizeof(struct rte_tm_node_params));

	/* Queue Node Parameters */
	priority = 0;
	weight = 1;
	level_id = TM_NODE_LEVEL_QUEUE;
	qnp.shaper_profile_id = RTE_TM_SHAPER_PROFILE_ID_NONE;
	qnp.leaf.cman = RTE_TM_CMAN_TAIL_DROP;
	qnp.stats_mask = STATS_MASK_QUEUE;

	/* Add Queue Nodes to TM Hierarchy */
	n_queue_nodes = 0;
	for (i = 0; i < NUM_PIPE_NODES; i++) {
		for (j = 0; j < TC_NODES_PER_PIPE; j++) {
			queue_parent_node_id = h->tc_node_id[i][j];
			for (k = 0; k < QUEUE_NODES_PER_TC; k++) {
				pos = j + (i * TC_NODES_PER_PIPE);
				h->queue_node_id[pos][k] = n_queue_nodes;
				if (rte_tm_node_add(port_id,
						h->queue_node_id[pos][k],
						queue_parent_node_id,
						priority,
						weight,
						level_id,
						&qnp, error)) {
					printf("%s ERROR(%d)-%s!(node %u)\n",
						__func__,
						error->type,
						error->message,
						h->queue_node_id[pos][k]);

					return -1;
				}
				n_queue_nodes++;
			}
		}
	}
	printf("  Queue nodes added (Start id %u, Count %u, level %u)\n",
		h->queue_node_id[0][0], n_queue_nodes, level_id);

	return 0;
}

/*
 * TM Packet Field Setup
 */
static void
softport_tm_pktfield_setup(portid_t port_id)
{
	struct rte_port *p = &ports[port_id];
	uint64_t pktfield0_mask = 0;
	uint64_t pktfield1_mask = 0x0000000FFF000000LLU;
	uint64_t pktfield2_mask = 0x00000000000000FCLLU;

	p->softport.tm = (struct softnic_port_tm) {
		.n_subports_per_port = SUBPORT_NODES_PER_PORT,
		.n_pipes_per_subport = PIPE_NODES_PER_SUBPORT,

		/* Packet field to identify subport
		 *
		 * Default configuration assumes only one subport, thus
		 * the subport ID is hardcoded to 0
		 */
		.tm_pktfield0_slabpos = 0,
		.tm_pktfield0_slabmask = pktfield0_mask,
		.tm_pktfield0_slabshr =
			__builtin_ctzll(pktfield0_mask),

		/* Packet field to identify pipe.
		 *
		 * Default value assumes Ethernet/IPv4/UDP packets,
		 * UDP payload bits 12 .. 23
		 */
		.tm_pktfield1_slabpos = 40,
		.tm_pktfield1_slabmask = pktfield1_mask,
		.tm_pktfield1_slabshr =
			__builtin_ctzll(pktfield1_mask),

		/* Packet field used as index into TC translation table
		 * to identify the traffic class and queue.
		 *
		 * Default value assumes Ethernet/IPv4 packets, IPv4
		 * DSCP field
		 */
		.tm_pktfield2_slabpos = 8,
		.tm_pktfield2_slabmask = pktfield2_mask,
		.tm_pktfield2_slabshr =
			__builtin_ctzll(pktfield2_mask),

		.tm_tc_table = {
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
		}, /**< TC translation table */
	};
}

static int
softport_tm_hierarchy_specify(portid_t port_id, struct rte_tm_error *error)
{

	struct tm_hierarchy h;
	int status;

	memset(&h, 0, sizeof(struct tm_hierarchy));

	/* TM hierarchy shapers rate */
	set_tm_hiearchy_nodes_shaper_rate(port_id, &h);

	/* Add root node (level 0) */
	status = softport_tm_root_node_add(port_id, &h, error);
	if (status)
		return status;

	/* Add subport node (level 1) */
	status = softport_tm_subport_node_add(port_id, &h, error);
	if (status)
		return status;

	/* Add pipe nodes (level 2) */
	status = softport_tm_pipe_node_add(port_id, &h, error);
	if (status)
		return status;

	/* Add traffic class nodes (level 3) */
	status = softport_tm_tc_node_add(port_id, &h, error);
	if (status)
		return status;

	/* Add queue nodes (level 4) */
	status = softport_tm_queue_node_add(port_id, &h, error);
	if (status)
		return status;

	/* TM packet fields setup */
	softport_tm_pktfield_setup(port_id);

	return 0;
}

/*
 * Soft port Init
 */
static void
softport_tm_begin(portid_t pi)
{
	struct rte_port *port = &ports[pi];

	/* Soft port TM flag */
	if (port->softport.tm_flag == 1) {
		printf("\n\n  TM feature available on port %u\n", pi);

		/* Soft port TM hierarchy configuration */
		if ((port->softport.tm.hierarchy_config == 0) &&
			(port->softport.tm.default_hierarchy_enable == 1)) {
			struct rte_tm_error error;
			int status;

			/* Stop port */
			rte_eth_dev_stop(pi);

			/* TM hierarchy specification */
			status = softport_tm_hierarchy_specify(pi, &error);
			if (status) {
				printf("  TM Hierarchy built error(%d) - %s\n",
					error.type, error.message);
				return;
			}
			printf("\n  TM Hierarchy Specified!\n\v");

			/* TM hierarchy commit */
			status = rte_tm_hierarchy_commit(pi, 0, &error);
			if (status) {
				printf("  Hierarchy commit error(%d) - %s\n",
					error.type, error.message);
				return;
			}
			printf("  Hierarchy Committed (port %u)!", pi);
			port->softport.tm.hierarchy_config = 1;

			/* Start port */
			status = rte_eth_dev_start(pi);
			if (status) {
				printf("\n  Port %u start error!\n", pi);
				return;
			}
			printf("\n  Port %u started!\n", pi);
			return;
		}
	}
	printf("\n  TM feature not available on port %u", pi);
}

struct fwd_engine softnic_tm_engine = {
	.fwd_mode_name  = "tm",
	.port_fwd_begin = softport_tm_begin,
	.port_fwd_end   = NULL,
	.packet_fwd     = softport_packet_fwd,
};

struct fwd_engine softnic_tm_bypass_engine = {
	.fwd_mode_name  = "tm-bypass",
	.port_fwd_begin = NULL,
	.port_fwd_end   = NULL,
	.packet_fwd     = softport_packet_fwd,
};
