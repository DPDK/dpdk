/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
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

#include <rte_malloc.h>
#include <rte_mbuf.h>

#include "rte_gro.h"

typedef void *(*gro_tbl_create_fn)(uint16_t socket_id,
		uint16_t max_flow_num,
		uint16_t max_item_per_flow);
typedef void (*gro_tbl_destroy_fn)(void *tbl);
typedef uint32_t (*gro_tbl_pkt_count_fn)(void *tbl);

static gro_tbl_create_fn tbl_create_fn[RTE_GRO_TYPE_MAX_NUM];
static gro_tbl_destroy_fn tbl_destroy_fn[RTE_GRO_TYPE_MAX_NUM];
static gro_tbl_pkt_count_fn tbl_pkt_count_fn[RTE_GRO_TYPE_MAX_NUM];

/*
 * GRO context structure, which is used to merge packets. It keeps
 * many reassembly tables of desired GRO types. Applications need to
 * create GRO context objects before using rte_gro_reassemble to
 * perform GRO.
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
		gro_type_flag = 1 << i;
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

	if (gro_ctx == NULL)
		return;
	for (i = 0; i < RTE_GRO_TYPE_MAX_NUM; i++) {
		gro_type_flag = 1 << i;
		if ((gro_ctx->gro_types & gro_type_flag) == 0)
			continue;
		destroy_tbl_fn = tbl_destroy_fn[i];
		if (destroy_tbl_fn)
			destroy_tbl_fn(gro_ctx->tbls[i]);
	}
	rte_free(gro_ctx);
}

uint16_t
rte_gro_reassemble_burst(struct rte_mbuf **pkts __rte_unused,
		uint16_t nb_pkts,
		const struct rte_gro_param *param __rte_unused)
{
	return nb_pkts;
}

uint16_t
rte_gro_reassemble(struct rte_mbuf **pkts __rte_unused,
		uint16_t nb_pkts,
		void *ctx __rte_unused)
{
	return nb_pkts;
}

uint16_t
rte_gro_timeout_flush(void *ctx __rte_unused,
		uint64_t timeout_cycles __rte_unused,
		uint64_t gro_types __rte_unused,
		struct rte_mbuf **out __rte_unused,
		uint16_t max_nb_out __rte_unused)
{
	return 0;
}

uint64_t
rte_gro_get_pkt_count(void *ctx)
{
	struct gro_ctx *gro_ctx = ctx;
	gro_tbl_pkt_count_fn pkt_count_fn;
	uint64_t item_num = 0;
	uint64_t gro_type_flag;
	uint8_t i;

	for (i = 0; i < RTE_GRO_TYPE_MAX_NUM; i++) {
		gro_type_flag = 1 << i;
		if ((gro_ctx->gro_types & gro_type_flag) == 0)
			continue;

		pkt_count_fn = tbl_pkt_count_fn[i];
		if (pkt_count_fn == NULL)
			continue;
		item_num += pkt_count_fn(gro_ctx->tbls[i]);
	}
	return item_num;
}
