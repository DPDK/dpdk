/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */
#include <eal_export.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>
#include <rte_graph_feature_arc_worker.h>
#include <rte_ip.h>
#include <rte_malloc.h>
#include <rte_vect.h>

#include "rte_node_ip4_api.h"

#include "ip4_rewrite_priv.h"
#include "node_private.h"
#include "interface_tx_feature_priv.h"

#ifndef RTE_IP4_OUTPUT_ARC_INDEXES
#define RTE_IP4_OUTPUT_ARC_INDEXES RTE_MAX_ETHPORTS
#endif

struct ip4_rewrite_node_ctx {
	/* Dynamic offset to mbuf priv1 */
	int mbuf_priv1_off;
	/* Dynamic offset to feature arc field */
	int arc_dyn_off;
	/* Cached next index */
	uint16_t next_index;
	/* tx interface of last mbuf */
	uint16_t last_tx_if;
	/* Cached feature arc handle */
	rte_graph_feature_arc_t output_feature_arc;
};

static struct ip4_rewrite_node_main *ip4_rewrite_nm;
static int port_to_next_index_diff = -1;

#define IP4_REWRITE_NODE_LAST_NEXT(ctx) \
	(((struct ip4_rewrite_node_ctx *)ctx)->next_index)

#define IP4_REWRITE_NODE_PRIV1_OFF(ctx) \
	(((struct ip4_rewrite_node_ctx *)ctx)->mbuf_priv1_off)

#define IP4_REWRITE_NODE_FEAT_OFF(ctx) \
	(((struct ip4_rewrite_node_ctx *)ctx)->arc_dyn_off)

#define IP4_REWRITE_NODE_OUTPUT_FEATURE_ARC(ctx) \
	(((struct ip4_rewrite_node_ctx *)ctx)->output_feature_arc)

#define IP4_REWRITE_NODE_LAST_TX_IF(ctx) \
	(((struct ip4_rewrite_node_ctx *)ctx)->last_tx_if)

static __rte_always_inline void
check_output_feature_arc_x1(struct rte_graph_feature_arc *arc, uint16_t *tx_if,
			    struct rte_mbuf *mbuf0, uint16_t *next0,
			    uint16_t *last_next_index,
			    rte_graph_feature_data_t *feature_data, const int feat_dyn)
{
	struct rte_graph_feature_arc_mbuf_dynfields *d0 = NULL;
	uint16_t port0;

	/* make sure packets are not being sent to pkt_drop node */
	if (likely(*next0 >= port_to_next_index_diff)) {

		port0 = (*next0) - port_to_next_index_diff;

		/* get pointer to feature arc mbuf */
		d0 = rte_graph_feature_arc_mbuf_dynfields_get(mbuf0, feat_dyn);

		/* Check if last packet's tx port not same as current */
		if (*tx_if != port0) {
			if (rte_graph_feature_data_first_feature_get(arc, port0,
								     &d0->feature_data,
								     next0)) {
				mbuf0->port = port0;
				*last_next_index = *next0;
			}
			*tx_if = port0;
			*feature_data = d0->feature_data;
		} else {
			if (rte_graph_feature_data_is_valid(*feature_data)) {
				*next0 = *last_next_index;
				mbuf0->port = port0;
				d0->feature_data = *feature_data;
			}
		}
	}
}

static __rte_always_inline void
check_output_feature_arc_x4(struct rte_graph_feature_arc *arc, uint16_t *tx_if,
			    struct rte_mbuf *mbuf0, struct rte_mbuf *mbuf1,
			    struct rte_mbuf *mbuf2, struct rte_mbuf *mbuf3,
			    uint16_t *next0, uint16_t *next1, uint16_t *next2,
			    uint16_t *next3, uint16_t *last_next_index,
			    rte_graph_feature_data_t *feature_data, const int feat_dyn)
{
	struct rte_graph_feature_arc_mbuf_dynfields *d0 = NULL, *d1 = NULL, *d2 = NULL, *d3 = NULL;
	uint16_t port0, port1, port2, port3;
	uint16_t xor = 0;

	/* get pointer to feature arc dyn field */
	d0 = rte_graph_feature_arc_mbuf_dynfields_get(mbuf0, feat_dyn);
	d1 = rte_graph_feature_arc_mbuf_dynfields_get(mbuf1, feat_dyn);
	d2 = rte_graph_feature_arc_mbuf_dynfields_get(mbuf2, feat_dyn);
	d3 = rte_graph_feature_arc_mbuf_dynfields_get(mbuf3, feat_dyn);

	/*
	 * Check if all four packets are going to same next_index/port
	 */
	xor = (*tx_if + port_to_next_index_diff) ^ (*next0);
	xor += (*next0) ^ (*next1);
	xor += (*next1) ^ (*next2);
	xor += (*next2) ^ (*next3);

	if (xor) {
		/* packets tx ports are not same, check first feature for each mbuf
		 * make sure next0 != 0 which is pkt_drop
		 */
		port0 = (*next0) - port_to_next_index_diff;
		port1 = (*next1) - port_to_next_index_diff;
		port2 = (*next2) - port_to_next_index_diff;
		port3 = (*next3) - port_to_next_index_diff;
		if (unlikely((*next0 >= port_to_next_index_diff) &&
			     rte_graph_feature_data_first_feature_get(arc, port0,
								      &d0->feature_data,
								      next0))) {
			/* update next0 from feature arc */
			mbuf0->port = port0;
		}

		if (unlikely((*next1 >= port_to_next_index_diff) &&
			     rte_graph_feature_data_first_feature_get(arc, port1,
								      &d1->feature_data,
								      next1))) {
			mbuf1->port = port1;
		}

		if (unlikely((*next2 >= port_to_next_index_diff) &&
			     rte_graph_feature_data_first_feature_get(arc, port2,
								      &d2->feature_data,
								      next2))) {
			mbuf2->port = port2;
		}

		if (unlikely((*next3 >= port_to_next_index_diff) &&
			     rte_graph_feature_data_first_feature_get(arc, port3,
								      &d3->feature_data,
								      next3))) {
			mbuf3->port = port3;

			*tx_if = port3;
			*last_next_index = *next3;
		}
		*feature_data = d3->feature_data;
	} else {
		/* All packets are same as last tx port. Check if feature enabled
		 * on last packet is valid or not. If invalid no need to
		 * change any next[0-3]
		 * Also check packet is not being sent to pkt_drop node
		 */
		if (unlikely(rte_graph_feature_data_is_valid(*feature_data) &&
			     (*next0 != 0))) {
			*next0 = *last_next_index;
			*next1 = *last_next_index;
			*next2 = *last_next_index;
			*next3 = *last_next_index;

			d0->feature_data = *feature_data;
			d1->feature_data = *feature_data;
			d2->feature_data = *feature_data;
			d3->feature_data = *feature_data;

			mbuf0->port = *tx_if;
			mbuf1->port = *tx_if;
			mbuf2->port = *tx_if;
			mbuf3->port = *tx_if;
		}
	}
}

static __rte_always_inline uint16_t
__ip4_rewrite_node_process(struct rte_graph *graph, struct rte_node *node,
			   void **objs, uint16_t nb_objs,
			   const int dyn, const int feat_dyn, const int check_enabled_features,
			   struct rte_graph_feature_arc *out_feature_arc)
{
	rte_graph_feature_data_t feature_data = RTE_GRAPH_FEATURE_DATA_INVALID;
	struct rte_mbuf *mbuf0, *mbuf1, *mbuf2, *mbuf3, **pkts;
	struct ip4_rewrite_nh_header *nh = ip4_rewrite_nm->nh;
	uint16_t next0, next1, next2, next3, next_index;
	struct rte_ipv4_hdr *ip0, *ip1, *ip2, *ip3;
	uint16_t n_left_from, held = 0, last_spec = 0;
	uint16_t last_tx_if, last_next_index;
	void *d0, *d1, *d2, *d3;
	void **to_next, **from;
	rte_xmm_t priv01;
	rte_xmm_t priv23;
	int i;

	/* Speculative next as last next */
	next_index = IP4_REWRITE_NODE_LAST_NEXT(node->ctx);
	rte_prefetch0(nh);

	pkts = (struct rte_mbuf **)objs;
	from = objs;
	n_left_from = nb_objs;

	for (i = 0; i < 4 && i < n_left_from; i++)
		rte_prefetch0(pkts[i]);

	if (check_enabled_features) {
		rte_graph_feature_arc_prefetch(out_feature_arc);

		last_tx_if = IP4_REWRITE_NODE_LAST_TX_IF(node->ctx);

		/* If feature is enabled on last_tx_if, prefetch data
		 * corresponding to first feature
		 */
		if (unlikely(rte_graph_feature_data_first_feature_get(out_feature_arc,
								      last_tx_if,
								      &feature_data,
								      &last_next_index)))
			rte_graph_feature_arc_feature_data_prefetch(out_feature_arc,
								    feature_data);

		/* Reset last_tx_if and last_next_index to call feature arc APIs
		 * for initial packets in every node loop
		 */
		last_tx_if = UINT16_MAX;
		last_next_index = UINT16_MAX;
	}

	/* Get stream for the speculated next node */
	to_next = rte_node_next_stream_get(graph, node, next_index, nb_objs);
	/* Update Ethernet header of pkts */
	while (n_left_from >= 4) {
		if (likely(n_left_from > 7)) {
			/* Prefetch only next-mbuf struct and priv area.
			 * Data need not be prefetched as we only write.
			 */
			rte_prefetch0(pkts[4]);
			rte_prefetch0(pkts[5]);
			rte_prefetch0(pkts[6]);
			rte_prefetch0(pkts[7]);
		}

		mbuf0 = pkts[0];
		mbuf1 = pkts[1];
		mbuf2 = pkts[2];
		mbuf3 = pkts[3];

		pkts += 4;
		n_left_from -= 4;

		priv01.u64[0] = node_mbuf_priv1(mbuf0, dyn)->u;
		priv01.u64[1] = node_mbuf_priv1(mbuf1, dyn)->u;
		priv23.u64[0] = node_mbuf_priv1(mbuf2, dyn)->u;
		priv23.u64[1] = node_mbuf_priv1(mbuf3, dyn)->u;

		/* Increment checksum by one. */
		priv01.u32[1] += rte_cpu_to_be_16(0x0100);
		priv01.u32[3] += rte_cpu_to_be_16(0x0100);
		priv23.u32[1] += rte_cpu_to_be_16(0x0100);
		priv23.u32[3] += rte_cpu_to_be_16(0x0100);

		/* Update ttl,cksum rewrite ethernet hdr on mbuf0 */
		d0 = rte_pktmbuf_mtod(mbuf0, void *);
		rte_memcpy(d0, nh[priv01.u16[0]].rewrite_data,
			   nh[priv01.u16[0]].rewrite_len);

		next0 = nh[priv01.u16[0]].tx_node;
		ip0 = (struct rte_ipv4_hdr *)((uint8_t *)d0 +
					      sizeof(struct rte_ether_hdr));
		ip0->time_to_live = priv01.u16[1] - 1;
		ip0->hdr_checksum = priv01.u16[2] + priv01.u16[3];

		/* Update ttl,cksum rewrite ethernet hdr on mbuf1 */
		d1 = rte_pktmbuf_mtod(mbuf1, void *);
		rte_memcpy(d1, nh[priv01.u16[4]].rewrite_data,
			   nh[priv01.u16[4]].rewrite_len);

		next1 = nh[priv01.u16[4]].tx_node;
		ip1 = (struct rte_ipv4_hdr *)((uint8_t *)d1 +
					      sizeof(struct rte_ether_hdr));
		ip1->time_to_live = priv01.u16[5] - 1;
		ip1->hdr_checksum = priv01.u16[6] + priv01.u16[7];

		/* Update ttl,cksum rewrite ethernet hdr on mbuf2 */
		d2 = rte_pktmbuf_mtod(mbuf2, void *);
		rte_memcpy(d2, nh[priv23.u16[0]].rewrite_data,
			   nh[priv23.u16[0]].rewrite_len);
		next2 = nh[priv23.u16[0]].tx_node;
		ip2 = (struct rte_ipv4_hdr *)((uint8_t *)d2 +
					      sizeof(struct rte_ether_hdr));
		ip2->time_to_live = priv23.u16[1] - 1;
		ip2->hdr_checksum = priv23.u16[2] + priv23.u16[3];

		/* Update ttl,cksum rewrite ethernet hdr on mbuf3 */
		d3 = rte_pktmbuf_mtod(mbuf3, void *);
		rte_memcpy(d3, nh[priv23.u16[4]].rewrite_data,
			   nh[priv23.u16[4]].rewrite_len);

		next3 = nh[priv23.u16[4]].tx_node;
		ip3 = (struct rte_ipv4_hdr *)((uint8_t *)d3 +
					      sizeof(struct rte_ether_hdr));
		ip3->time_to_live = priv23.u16[5] - 1;
		ip3->hdr_checksum = priv23.u16[6] + priv23.u16[7];

		/* Once all mbufs are updated with next hop data.
		 * check if any feature is enabled to override
		 * next edges
		 */
		if (check_enabled_features)
			check_output_feature_arc_x4(out_feature_arc, &last_tx_if,
						    mbuf0, mbuf1, mbuf2, mbuf3,
						    &next0, &next1, &next2, &next3,
						    &last_next_index, &feature_data, feat_dyn);

		/* Enqueue four to next node */
		rte_edge_t fix_spec =
			((next_index == next0) && (next0 == next1) &&
			 (next1 == next2) && (next2 == next3));

		if (unlikely(fix_spec == 0)) {
			/* Copy things successfully speculated till now */
			rte_memcpy(to_next, from, last_spec * sizeof(from[0]));
			from += last_spec;
			to_next += last_spec;
			held += last_spec;
			last_spec = 0;

			/* next0 */
			if (next_index == next0) {
				to_next[0] = from[0];
				to_next++;
				held++;
			} else {
				rte_node_enqueue_x1(graph, node, next0,
						    from[0]);
			}

			/* next1 */
			if (next_index == next1) {
				to_next[0] = from[1];
				to_next++;
				held++;
			} else {
				rte_node_enqueue_x1(graph, node, next1,
						    from[1]);
			}

			/* next2 */
			if (next_index == next2) {
				to_next[0] = from[2];
				to_next++;
				held++;
			} else {
				rte_node_enqueue_x1(graph, node, next2,
						    from[2]);
			}

			/* next3 */
			if (next_index == next3) {
				to_next[0] = from[3];
				to_next++;
				held++;
			} else {
				rte_node_enqueue_x1(graph, node, next3,
						    from[3]);
			}

			from += 4;

			/* Change speculation if last two are same */
			if ((next_index != next3) && (next2 == next3)) {
				/* Put the current speculated node */
				rte_node_next_stream_put(graph, node,
							 next_index, held);
				held = 0;

				/* Get next speculated stream */
				next_index = next3;
				to_next = rte_node_next_stream_get(
					graph, node, next_index, nb_objs);
			}
		} else {
			last_spec += 4;
		}
	}

	while (n_left_from > 0) {
		uint16_t chksum;

		mbuf0 = pkts[0];

		pkts += 1;
		n_left_from -= 1;

		d0 = rte_pktmbuf_mtod(mbuf0, void *);
		rte_memcpy(d0, nh[node_mbuf_priv1(mbuf0, dyn)->nh].rewrite_data,
			   nh[node_mbuf_priv1(mbuf0, dyn)->nh].rewrite_len);

		next0 = nh[node_mbuf_priv1(mbuf0, dyn)->nh].tx_node;
		ip0 = (struct rte_ipv4_hdr *)((uint8_t *)d0 +
					      sizeof(struct rte_ether_hdr));
		chksum = node_mbuf_priv1(mbuf0, dyn)->cksum +
			 rte_cpu_to_be_16(0x0100);
		chksum += chksum >= 0xffff;
		ip0->hdr_checksum = chksum;
		ip0->time_to_live = node_mbuf_priv1(mbuf0, dyn)->ttl - 1;

		if (check_enabled_features)
			check_output_feature_arc_x1(out_feature_arc, &last_tx_if,
						    mbuf0, &next0, &last_next_index,
						    &feature_data, feat_dyn);

		if (unlikely(next_index ^ next0)) {
			/* Copy things successfully speculated till now */
			rte_memcpy(to_next, from, last_spec * sizeof(from[0]));
			from += last_spec;
			to_next += last_spec;
			held += last_spec;
			last_spec = 0;

			rte_node_enqueue_x1(graph, node, next0, from[0]);
			from += 1;
		} else {
			last_spec += 1;
		}
	}

	/* !!! Home run !!! */
	if (likely(last_spec == nb_objs)) {
		rte_node_next_stream_move(graph, node, next_index);
		return nb_objs;
	}

	held += last_spec;
	rte_memcpy(to_next, from, last_spec * sizeof(from[0]));
	rte_node_next_stream_put(graph, node, next_index, held);
	/* Save the last next used */
	IP4_REWRITE_NODE_LAST_NEXT(node->ctx) = next_index;

	if (check_enabled_features)
		IP4_REWRITE_NODE_LAST_TX_IF(node->ctx) = last_tx_if;

	return nb_objs;
}

static uint16_t
ip4_rewrite_feature_node_process(struct rte_graph *graph, struct rte_node *node,
				 void **objs, uint16_t nb_objs)
{
	const int dyn = IP4_REWRITE_NODE_PRIV1_OFF(node->ctx);
	const int feat_dyn = IP4_REWRITE_NODE_FEAT_OFF(node->ctx);
	struct rte_graph_feature_arc *arc = NULL;

	arc = rte_graph_feature_arc_get(IP4_REWRITE_NODE_OUTPUT_FEATURE_ARC(node->ctx));
	if (unlikely(rte_graph_feature_arc_is_any_feature_enabled(arc) &&
		     (port_to_next_index_diff > 0)))
		return __ip4_rewrite_node_process(graph, node, objs, nb_objs, dyn, feat_dyn,
						  1 /* check features */, arc);

	return __ip4_rewrite_node_process(graph, node, objs, nb_objs, dyn, 0,
					  0/* don't check features*/,
					  arc /* don't care*/);
}

static uint16_t
ip4_rewrite_node_process(struct rte_graph *graph, struct rte_node *node,
			 void **objs, uint16_t nb_objs)
{
	const int dyn = IP4_REWRITE_NODE_PRIV1_OFF(node->ctx);

	return __ip4_rewrite_node_process(graph, node, objs, nb_objs, dyn, 0,
					  0/* don't check features*/,
					  NULL/* don't care */);
}


static int
ip4_rewrite_node_init(const struct rte_graph *graph, struct rte_node *node)
{
	rte_graph_feature_arc_t feature_arc = RTE_GRAPH_FEATURE_ARC_INITIALIZER;
	static bool init_once;

	RTE_SET_USED(graph);
	RTE_BUILD_BUG_ON(sizeof(struct ip4_rewrite_node_ctx) > RTE_NODE_CTX_SZ);

	if (!init_once) {
		node_mbuf_priv1_dynfield_offset = rte_mbuf_dynfield_register(
				&node_mbuf_priv1_dynfield_desc);
		if (node_mbuf_priv1_dynfield_offset < 0)
			return -rte_errno;

		/* Create ipv4-output feature arc, if not created
		 */
		if (rte_graph_feature_arc_lookup_by_name(RTE_IP4_OUTPUT_FEATURE_ARC_NAME,
						     &feature_arc) < 0) {
			node_err("ip4_rewrite", "Feature arc \"%s\" not found",
				 RTE_IP4_OUTPUT_FEATURE_ARC_NAME);
		} else {
			node_err("ip4_rewrite", "Feature arc \"%s\" found",
				 RTE_IP4_OUTPUT_FEATURE_ARC_NAME);
		}

		init_once = true;
	}
	IP4_REWRITE_NODE_PRIV1_OFF(node->ctx) = node_mbuf_priv1_dynfield_offset;
	IP4_REWRITE_NODE_OUTPUT_FEATURE_ARC(node->ctx) = feature_arc;

	if (rte_graph_feature_arc_get(feature_arc))
		IP4_REWRITE_NODE_FEAT_OFF(node->ctx) =
			rte_graph_feature_arc_get(feature_arc)->mbuf_dyn_offset;

	/* By default, set cached next node to pkt_drop */
	IP4_REWRITE_NODE_LAST_NEXT(node->ctx) = 0;
	IP4_REWRITE_NODE_LAST_TX_IF(node->ctx) = 0;

	node_dbg("ip4_rewrite", "Initialized ip4_rewrite node initialized");

	return 0;
}

int
ip4_rewrite_set_next(uint16_t port_id, uint16_t next_index)
{
	static int once;

	if (ip4_rewrite_nm == NULL) {
		ip4_rewrite_nm = rte_zmalloc(
			"ip4_rewrite", sizeof(struct ip4_rewrite_node_main),
			RTE_CACHE_LINE_SIZE);
		if (ip4_rewrite_nm == NULL)
			return -ENOMEM;
	}
	if (!once) {
		port_to_next_index_diff = next_index - port_id;
		once = 1;
	}
	ip4_rewrite_nm->next_index[port_id] = next_index;

	return 0;
}

RTE_EXPORT_SYMBOL(rte_node_ip4_rewrite_add)
int
rte_node_ip4_rewrite_add(uint16_t next_hop, uint8_t *rewrite_data,
			 uint8_t rewrite_len, uint16_t dst_port)
{
	struct ip4_rewrite_nh_header *nh;

	if (next_hop >= RTE_GRAPH_IP4_REWRITE_MAX_NH)
		return -EINVAL;

	if (rewrite_len > RTE_GRAPH_IP4_REWRITE_MAX_LEN)
		return -EINVAL;

	if (ip4_rewrite_nm == NULL) {
		ip4_rewrite_nm = rte_zmalloc(
			"ip4_rewrite", sizeof(struct ip4_rewrite_node_main),
			RTE_CACHE_LINE_SIZE);
		if (ip4_rewrite_nm == NULL)
			return -ENOMEM;
	}

	/* Check if dst port doesn't exist as edge */
	if (!ip4_rewrite_nm->next_index[dst_port])
		return -EINVAL;

	/* Update next hop */
	nh = &ip4_rewrite_nm->nh[next_hop];

	memcpy(nh->rewrite_data, rewrite_data, rewrite_len);
	nh->tx_node = ip4_rewrite_nm->next_index[dst_port];
	nh->rewrite_len = rewrite_len;
	nh->enabled = true;

	return 0;
}

static struct rte_node_register ip4_rewrite_node = {
	.process = ip4_rewrite_node_process,
	.name = "ip4_rewrite",
	/* Default edge i.e '0' is pkt drop */
	.nb_edges = 1,
	.next_nodes = {
		[0] = "pkt_drop",
	},
	.init = ip4_rewrite_node_init,
};

struct rte_node_register *
ip4_rewrite_node_get(void)
{
	return &ip4_rewrite_node;
}

RTE_NODE_REGISTER(ip4_rewrite_node);

/* IP4 output arc */
static struct rte_graph_feature_arc_register ip4_output_arc = {
	.arc_name = RTE_IP4_OUTPUT_FEATURE_ARC_NAME,

	/* This arc works on all ethdevs */
	.max_indexes = RTE_IP4_OUTPUT_ARC_INDEXES,

	.start_node = &ip4_rewrite_node,

	/* overwrites start_node->process() function with following only if
	 * application calls rte_graph_feature_arc_init()
	 */
	.start_node_feature_process_fn = ip4_rewrite_feature_node_process,

	/* end feature node of an arc*/
	.end_feature = &if_tx_feature,
};

RTE_GRAPH_FEATURE_ARC_REGISTER(ip4_output_arc);
