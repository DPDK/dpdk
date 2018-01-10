/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_cycles.h>
#include <rte_ethdev.h>

#include "rte_gro.h"
#include "gro_tcp4.h"

typedef void *(*gro_tbl_create_fn)(uint16_t socket_id,
		uint16_t max_flow_num,
		uint16_t max_item_per_flow);
typedef void (*gro_tbl_destroy_fn)(void *tbl);
typedef uint32_t (*gro_tbl_pkt_count_fn)(void *tbl);

static gro_tbl_create_fn tbl_create_fn[RTE_GRO_TYPE_MAX_NUM] = {
		gro_tcp4_tbl_create, NULL};
static gro_tbl_destroy_fn tbl_destroy_fn[RTE_GRO_TYPE_MAX_NUM] = {
			gro_tcp4_tbl_destroy, NULL};
static gro_tbl_pkt_count_fn tbl_pkt_count_fn[RTE_GRO_TYPE_MAX_NUM] = {
			gro_tcp4_tbl_pkt_count, NULL};

#define IS_IPV4_TCP_PKT(ptype) (RTE_ETH_IS_IPV4_HDR(ptype) && \
		((ptype & RTE_PTYPE_L4_TCP) == RTE_PTYPE_L4_TCP))

/*
 * GRO context structure. It keeps the table structures, which are
 * used to merge packets, for different GRO types. Before using
 * rte_gro_reassemble(), applications need to create the GRO context
 * first.
 */
struct gro_ctx {
	/* GRO types to perform */
	uint64_t gro_types;
	/* reassembly tables */
	void *tbls[RTE_GRO_TYPE_MAX_NUM];
};

void *
rte_gro_ctx_create(const struct rte_gro_param *param)
{
	struct gro_ctx *gro_ctx;
	gro_tbl_create_fn create_tbl_fn;
	uint64_t gro_type_flag = 0;
	uint64_t gro_types = 0;
	uint8_t i;

	gro_ctx = rte_zmalloc_socket(__func__,
			sizeof(struct gro_ctx),
			RTE_CACHE_LINE_SIZE,
			param->socket_id);
	if (gro_ctx == NULL)
		return NULL;

	for (i = 0; i < RTE_GRO_TYPE_MAX_NUM; i++) {
		gro_type_flag = 1ULL << i;
		if ((param->gro_types & gro_type_flag) == 0)
			continue;

		create_tbl_fn = tbl_create_fn[i];
		if (create_tbl_fn == NULL)
			continue;

		gro_ctx->tbls[i] = create_tbl_fn(param->socket_id,
				param->max_flow_num,
				param->max_item_per_flow);
		if (gro_ctx->tbls[i] == NULL) {
			/* destroy all created tables */
			gro_ctx->gro_types = gro_types;
			rte_gro_ctx_destroy(gro_ctx);
			return NULL;
		}
		gro_types |= gro_type_flag;
	}
	gro_ctx->gro_types = param->gro_types;

	return gro_ctx;
}

void
rte_gro_ctx_destroy(void *ctx)
{
	gro_tbl_destroy_fn destroy_tbl_fn;
	struct gro_ctx *gro_ctx = ctx;
	uint64_t gro_type_flag;
	uint8_t i;

	for (i = 0; i < RTE_GRO_TYPE_MAX_NUM; i++) {
		gro_type_flag = 1ULL << i;
		if ((gro_ctx->gro_types & gro_type_flag) == 0)
			continue;
		destroy_tbl_fn = tbl_destroy_fn[i];
		if (destroy_tbl_fn)
			destroy_tbl_fn(gro_ctx->tbls[i]);
	}
	rte_free(gro_ctx);
}

uint16_t
rte_gro_reassemble_burst(struct rte_mbuf **pkts,
		uint16_t nb_pkts,
		const struct rte_gro_param *param)
{
	/* allocate a reassembly table for TCP/IPv4 GRO */
	struct gro_tcp4_tbl tcp_tbl;
	struct gro_tcp4_flow tcp_flows[RTE_GRO_MAX_BURST_ITEM_NUM];
	struct gro_tcp4_item tcp_items[RTE_GRO_MAX_BURST_ITEM_NUM] = {{0} };

	struct rte_mbuf *unprocess_pkts[nb_pkts];
	uint32_t item_num;
	int32_t ret;
	uint16_t i, unprocess_num = 0, nb_after_gro = nb_pkts;

	if (unlikely((param->gro_types & RTE_GRO_TCP_IPV4) == 0))
		return nb_pkts;

	/* Get the maximum number of packets */
	item_num = RTE_MIN(nb_pkts, (param->max_flow_num *
				param->max_item_per_flow));
	item_num = RTE_MIN(item_num, RTE_GRO_MAX_BURST_ITEM_NUM);

	for (i = 0; i < item_num; i++)
		tcp_flows[i].start_index = INVALID_ARRAY_INDEX;

	tcp_tbl.flows = tcp_flows;
	tcp_tbl.items = tcp_items;
	tcp_tbl.flow_num = 0;
	tcp_tbl.item_num = 0;
	tcp_tbl.max_flow_num = item_num;
	tcp_tbl.max_item_num = item_num;

	for (i = 0; i < nb_pkts; i++) {
		if (IS_IPV4_TCP_PKT(pkts[i]->packet_type)) {
			/*
			 * The timestamp is ignored, since all packets
			 * will be flushed from the tables.
			 */
			ret = gro_tcp4_reassemble(pkts[i], &tcp_tbl, 0);
			if (ret > 0)
				/* merge successfully */
				nb_after_gro--;
			else if (ret < 0)
				unprocess_pkts[unprocess_num++] = pkts[i];
		} else
			unprocess_pkts[unprocess_num++] = pkts[i];
	}

	if (nb_after_gro < nb_pkts) {
		/* Flush all packets from the tables */
		i = gro_tcp4_tbl_timeout_flush(&tcp_tbl, 0, pkts, nb_pkts);
		/* Copy unprocessed packets */
		if (unprocess_num > 0) {
			memcpy(&pkts[i], unprocess_pkts,
					sizeof(struct rte_mbuf *) *
					unprocess_num);
		}
	}

	return nb_after_gro;
}

uint16_t
rte_gro_reassemble(struct rte_mbuf **pkts,
		uint16_t nb_pkts,
		void *ctx)
{
	struct rte_mbuf *unprocess_pkts[nb_pkts];
	struct gro_ctx *gro_ctx = ctx;
	void *tcp_tbl;
	uint64_t current_time;
	uint16_t i, unprocess_num = 0;

	if (unlikely((gro_ctx->gro_types & RTE_GRO_TCP_IPV4) == 0))
		return nb_pkts;

	tcp_tbl = gro_ctx->tbls[RTE_GRO_TCP_IPV4_INDEX];
	current_time = rte_rdtsc();

	for (i = 0; i < nb_pkts; i++) {
		if (IS_IPV4_TCP_PKT(pkts[i]->packet_type)) {
			if (gro_tcp4_reassemble(pkts[i], tcp_tbl,
						current_time) < 0)
				unprocess_pkts[unprocess_num++] = pkts[i];
		} else
			unprocess_pkts[unprocess_num++] = pkts[i];
	}
	if (unprocess_num > 0) {
		memcpy(pkts, unprocess_pkts, sizeof(struct rte_mbuf *) *
				unprocess_num);
	}

	return unprocess_num;
}

uint16_t
rte_gro_timeout_flush(void *ctx,
		uint64_t timeout_cycles,
		uint64_t gro_types,
		struct rte_mbuf **out,
		uint16_t max_nb_out)
{
	struct gro_ctx *gro_ctx = ctx;
	uint64_t flush_timestamp;

	gro_types = gro_types & gro_ctx->gro_types;
	flush_timestamp = rte_rdtsc() - timeout_cycles;

	if (gro_types & RTE_GRO_TCP_IPV4) {
		return gro_tcp4_tbl_timeout_flush(
				gro_ctx->tbls[RTE_GRO_TCP_IPV4_INDEX],
				flush_timestamp,
				out, max_nb_out);
	}

	return 0;
}

uint64_t
rte_gro_get_pkt_count(void *ctx)
{
	struct gro_ctx *gro_ctx = ctx;
	gro_tbl_pkt_count_fn pkt_count_fn;
	uint64_t gro_types = gro_ctx->gro_types, flag;
	uint64_t item_num = 0;
	uint8_t i;

	for (i = 0; i < RTE_GRO_TYPE_MAX_NUM && gro_types; i++) {
		flag = 1ULL << i;
		if ((gro_types & flag) == 0)
			continue;

		gro_types ^= flag;
		pkt_count_fn = tbl_pkt_count_fn[i];
		if (pkt_count_fn)
			item_num += pkt_count_fn(gro_ctx->tbls[i]);
	}

	return item_num;
}
