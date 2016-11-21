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

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_byteorder.h>
#include <rte_table_stub.h>
#include <rte_table_hash.h>
#include <rte_pipeline.h>

#include "pipeline_passthrough_be.h"
#include "pipeline_actions_common.h"
#include "parser.h"
#include "hash_func.h"

#define SWAP_DIM (PIPELINE_PASSTHROUGH_SWAP_N_FIELDS_MAX * \
	(PIPELINE_PASSTHROUGH_SWAP_FIELD_SIZE_MAX / sizeof(uint64_t)))

struct pipeline_passthrough {
	struct pipeline p;
	struct pipeline_passthrough_params params;
	rte_table_hash_op_hash f_hash;
	uint32_t swap_field0_offset[SWAP_DIM];
	uint32_t swap_field1_offset[SWAP_DIM];
	uint64_t swap_field_mask[SWAP_DIM];
	uint32_t swap_n_fields;
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

static inline __attribute__((always_inline)) void
pkt_work_dma(
	struct rte_mbuf *pkt,
	void *arg,
	uint32_t dma_size,
	uint32_t hash_enabled,
	uint32_t lb_hash,
	uint32_t port_out_pow2)
{
	struct pipeline_passthrough *p = arg;

	uint64_t *dma_dst = RTE_MBUF_METADATA_UINT64_PTR(pkt,
		p->params.dma_dst_offset);
	uint64_t *dma_src = RTE_MBUF_METADATA_UINT64_PTR(pkt,
		p->params.dma_src_offset);
	uint64_t *dma_mask = (uint64_t *) p->params.dma_src_mask;
	uint32_t *dma_hash = RTE_MBUF_METADATA_UINT32_PTR(pkt,
		p->params.dma_hash_offset);
	uint32_t i;

	/* Read (dma_src), compute (dma_dst), write (dma_dst) */
	for (i = 0; i < (dma_size / 8); i++)
		dma_dst[i] = dma_src[i] & dma_mask[i];

	/* Read (dma_dst), compute (hash), write (hash) */
	if (hash_enabled) {
		uint32_t hash = p->f_hash(dma_dst, dma_size, 0);
		*dma_hash = hash;

		if (lb_hash) {
			uint32_t port_out;

			if (port_out_pow2)
				port_out
					= hash & (p->p.n_ports_out - 1);
			else
				port_out
					= hash % p->p.n_ports_out;

			rte_pipeline_port_out_packet_insert(p->p.p,
				port_out, pkt);
		}
	}
}

static inline __attribute__((always_inline)) void
pkt4_work_dma(
	struct rte_mbuf **pkts,
	void *arg,
	uint32_t dma_size,
	uint32_t hash_enabled,
	uint32_t lb_hash,
	uint32_t port_out_pow2)
{
	struct pipeline_passthrough *p = arg;

	uint64_t *dma_dst0 = RTE_MBUF_METADATA_UINT64_PTR(pkts[0],
		p->params.dma_dst_offset);
	uint64_t *dma_dst1 = RTE_MBUF_METADATA_UINT64_PTR(pkts[1],
		p->params.dma_dst_offset);
	uint64_t *dma_dst2 = RTE_MBUF_METADATA_UINT64_PTR(pkts[2],
		p->params.dma_dst_offset);
	uint64_t *dma_dst3 = RTE_MBUF_METADATA_UINT64_PTR(pkts[3],
		p->params.dma_dst_offset);

	uint64_t *dma_src0 = RTE_MBUF_METADATA_UINT64_PTR(pkts[0],
		p->params.dma_src_offset);
	uint64_t *dma_src1 = RTE_MBUF_METADATA_UINT64_PTR(pkts[1],
		p->params.dma_src_offset);
	uint64_t *dma_src2 = RTE_MBUF_METADATA_UINT64_PTR(pkts[2],
		p->params.dma_src_offset);
	uint64_t *dma_src3 = RTE_MBUF_METADATA_UINT64_PTR(pkts[3],
		p->params.dma_src_offset);

	uint64_t *dma_mask = (uint64_t *) p->params.dma_src_mask;

	uint32_t *dma_hash0 = RTE_MBUF_METADATA_UINT32_PTR(pkts[0],
		p->params.dma_hash_offset);
	uint32_t *dma_hash1 = RTE_MBUF_METADATA_UINT32_PTR(pkts[1],
		p->params.dma_hash_offset);
	uint32_t *dma_hash2 = RTE_MBUF_METADATA_UINT32_PTR(pkts[2],
		p->params.dma_hash_offset);
	uint32_t *dma_hash3 = RTE_MBUF_METADATA_UINT32_PTR(pkts[3],
		p->params.dma_hash_offset);

	uint32_t i;

	/* Read (dma_src), compute (dma_dst), write (dma_dst) */
	for (i = 0; i < (dma_size / 8); i++) {
		dma_dst0[i] = dma_src0[i] & dma_mask[i];
		dma_dst1[i] = dma_src1[i] & dma_mask[i];
		dma_dst2[i] = dma_src2[i] & dma_mask[i];
		dma_dst3[i] = dma_src3[i] & dma_mask[i];
	}

	/* Read (dma_dst), compute (hash), write (hash) */
	if (hash_enabled) {
		uint32_t hash0 = p->f_hash(dma_dst0, dma_size, 0);
		uint32_t hash1 = p->f_hash(dma_dst1, dma_size, 0);
		uint32_t hash2 = p->f_hash(dma_dst2, dma_size, 0);
		uint32_t hash3 = p->f_hash(dma_dst3, dma_size, 0);

		*dma_hash0 = hash0;
		*dma_hash1 = hash1;
		*dma_hash2 = hash2;
		*dma_hash3 = hash3;

		if (lb_hash) {
			uint32_t port_out0, port_out1, port_out2, port_out3;

			if (port_out_pow2) {
				port_out0
					= hash0 & (p->p.n_ports_out - 1);
				port_out1
					= hash1 & (p->p.n_ports_out - 1);
				port_out2
					= hash2 & (p->p.n_ports_out - 1);
				port_out3
					= hash3 & (p->p.n_ports_out - 1);
			} else {
				port_out0
					= hash0 % p->p.n_ports_out;
				port_out1
					= hash1 % p->p.n_ports_out;
				port_out2
					= hash2 % p->p.n_ports_out;
				port_out3
					= hash3 % p->p.n_ports_out;
			}
			rte_pipeline_port_out_packet_insert(p->p.p,
				port_out0, pkts[0]);
			rte_pipeline_port_out_packet_insert(p->p.p,
				port_out1, pkts[1]);
			rte_pipeline_port_out_packet_insert(p->p.p,
				port_out2, pkts[2]);
			rte_pipeline_port_out_packet_insert(p->p.p,
				port_out3, pkts[3]);
		}
	}
}

static inline __attribute__((always_inline)) void
pkt_work_swap(
	struct rte_mbuf *pkt,
	void *arg)
{
	struct pipeline_passthrough *p = arg;
	uint32_t i;

	/* Read(field0, field1), compute(field0, field1), write(field0, field1) */
	for (i = 0; i < p->swap_n_fields; i++) {
		uint64_t *field0_ptr = RTE_MBUF_METADATA_UINT64_PTR(pkt,
			p->swap_field0_offset[i]);
		uint64_t *field1_ptr = RTE_MBUF_METADATA_UINT64_PTR(pkt,
			p->swap_field1_offset[i]);
		uint64_t mask = p->swap_field_mask[i];

		uint64_t field0 = *field0_ptr;
		uint64_t field1 = *field1_ptr;

		*field0_ptr = (field0 & (~mask)) + (field1 & mask);
		*field1_ptr = (field0 & mask) + (field1 & (~mask));
	}
}

static inline __attribute__((always_inline)) void
pkt4_work_swap(
	struct rte_mbuf **pkts,
	void *arg)
{
	struct pipeline_passthrough *p = arg;
	uint32_t i;

	/* Read(field0, field1), compute(field0, field1), write(field0, field1) */
	for (i = 0; i < p->swap_n_fields; i++) {
		uint64_t *pkt0_field0_ptr = RTE_MBUF_METADATA_UINT64_PTR(pkts[0],
			p->swap_field0_offset[i]);
		uint64_t *pkt1_field0_ptr = RTE_MBUF_METADATA_UINT64_PTR(pkts[1],
			p->swap_field0_offset[i]);
		uint64_t *pkt2_field0_ptr = RTE_MBUF_METADATA_UINT64_PTR(pkts[2],
			p->swap_field0_offset[i]);
		uint64_t *pkt3_field0_ptr = RTE_MBUF_METADATA_UINT64_PTR(pkts[3],
			p->swap_field0_offset[i]);

		uint64_t *pkt0_field1_ptr = RTE_MBUF_METADATA_UINT64_PTR(pkts[0],
			p->swap_field1_offset[i]);
		uint64_t *pkt1_field1_ptr = RTE_MBUF_METADATA_UINT64_PTR(pkts[1],
			p->swap_field1_offset[i]);
		uint64_t *pkt2_field1_ptr = RTE_MBUF_METADATA_UINT64_PTR(pkts[2],
			p->swap_field1_offset[i]);
		uint64_t *pkt3_field1_ptr = RTE_MBUF_METADATA_UINT64_PTR(pkts[3],
			p->swap_field1_offset[i]);

		uint64_t mask = p->swap_field_mask[i];

		uint64_t pkt0_field0 = *pkt0_field0_ptr;
		uint64_t pkt1_field0 = *pkt1_field0_ptr;
		uint64_t pkt2_field0 = *pkt2_field0_ptr;
		uint64_t pkt3_field0 = *pkt3_field0_ptr;

		uint64_t pkt0_field1 = *pkt0_field1_ptr;
		uint64_t pkt1_field1 = *pkt1_field1_ptr;
		uint64_t pkt2_field1 = *pkt2_field1_ptr;
		uint64_t pkt3_field1 = *pkt3_field1_ptr;

		*pkt0_field0_ptr = (pkt0_field0 & (~mask)) + (pkt0_field1 & mask);
		*pkt1_field0_ptr = (pkt1_field0 & (~mask)) + (pkt1_field1 & mask);
		*pkt2_field0_ptr = (pkt2_field0 & (~mask)) + (pkt2_field1 & mask);
		*pkt3_field0_ptr = (pkt3_field0 & (~mask)) + (pkt3_field1 & mask);

		*pkt0_field1_ptr = (pkt0_field0 & mask) + (pkt0_field1 & (~mask));
		*pkt1_field1_ptr = (pkt1_field0 & mask) + (pkt1_field1 & (~mask));
		*pkt2_field1_ptr = (pkt2_field0 & mask) + (pkt2_field1 & (~mask));
		*pkt3_field1_ptr = (pkt3_field0 & mask) + (pkt3_field1 & (~mask));
	}
}

#define PKT_WORK_DMA(dma_size, hash_enabled, lb_hash, port_pow2)	\
static inline void						\
pkt_work_dma_size##dma_size##_hash##hash_enabled		\
	##_lb##lb_hash##_pw##port_pow2(			\
	struct rte_mbuf *pkt,					\
	void *arg)						\
{								\
	pkt_work_dma(pkt, arg, dma_size, hash_enabled, lb_hash, port_pow2);	\
}

#define PKT4_WORK_DMA(dma_size, hash_enabled, lb_hash, port_pow2)	\
static inline void						\
pkt4_work_dma_size##dma_size##_hash##hash_enabled			\
	##_lb##lb_hash##_pw##port_pow2(			\
	struct rte_mbuf **pkts,					\
	void *arg)						\
{								\
	pkt4_work_dma(pkts, arg, dma_size, hash_enabled, lb_hash, port_pow2); \
}

#define port_in_ah_dma(dma_size, hash_enabled, lb_hash, port_pow2)	\
PKT_WORK_DMA(dma_size, hash_enabled, lb_hash, port_pow2)			\
PKT4_WORK_DMA(dma_size, hash_enabled, lb_hash, port_pow2)			\
PIPELINE_PORT_IN_AH(port_in_ah_dma_size##dma_size##_hash	\
	##hash_enabled##_lb##lb_hash##_pw##port_pow2,		\
	pkt_work_dma_size##dma_size##_hash##hash_enabled		\
	##_lb##lb_hash##_pw##port_pow2,			\
	pkt4_work_dma_size##dma_size##_hash##hash_enabled		\
	##_lb##lb_hash##_pw##port_pow2)


#define port_in_ah_lb(dma_size, hash_enabled, lb_hash, port_pow2) \
PKT_WORK_DMA(dma_size, hash_enabled, lb_hash, port_pow2)		\
PKT4_WORK_DMA(dma_size, hash_enabled, lb_hash, port_pow2)	\
PIPELINE_PORT_IN_AH_HIJACK_ALL(						\
	port_in_ah_lb_size##dma_size##_hash##hash_enabled		\
	##_lb##lb_hash##_pw##port_pow2,			\
	pkt_work_dma_size##dma_size##_hash##hash_enabled		\
	##_lb##lb_hash##_pw##port_pow2,	\
	pkt4_work_dma_size##dma_size##_hash##hash_enabled		\
	##_lb##lb_hash##_pw##port_pow2)

PIPELINE_PORT_IN_AH(port_in_ah_swap, pkt_work_swap,	pkt4_work_swap)


/* Port in AH DMA(dma_size, hash_enabled, lb_hash, port_pow2) */

port_in_ah_dma(8, 0, 0, 0)
port_in_ah_dma(8, 1, 0, 0)
port_in_ah_lb(8, 1, 1, 0)
port_in_ah_lb(8, 1, 1, 1)

port_in_ah_dma(16, 0, 0, 0)
port_in_ah_dma(16, 1, 0, 0)
port_in_ah_lb(16, 1, 1, 0)
port_in_ah_lb(16, 1, 1, 1)

port_in_ah_dma(24, 0, 0, 0)
port_in_ah_dma(24, 1, 0, 0)
port_in_ah_lb(24, 1, 1, 0)
port_in_ah_lb(24, 1, 1, 1)

port_in_ah_dma(32, 0, 0, 0)
port_in_ah_dma(32, 1, 0, 0)
port_in_ah_lb(32, 1, 1, 0)
port_in_ah_lb(32, 1, 1, 1)

port_in_ah_dma(40, 0, 0, 0)
port_in_ah_dma(40, 1, 0, 0)
port_in_ah_lb(40, 1, 1, 0)
port_in_ah_lb(40, 1, 1, 1)

port_in_ah_dma(48, 0, 0, 0)
port_in_ah_dma(48, 1, 0, 0)
port_in_ah_lb(48, 1, 1, 0)
port_in_ah_lb(48, 1, 1, 1)

port_in_ah_dma(56, 0, 0, 0)
port_in_ah_dma(56, 1, 0, 0)
port_in_ah_lb(56, 1, 1, 0)
port_in_ah_lb(56, 1, 1, 1)

port_in_ah_dma(64, 0, 0, 0)
port_in_ah_dma(64, 1, 0, 0)
port_in_ah_lb(64, 1, 1, 0)
port_in_ah_lb(64, 1, 1, 1)

static rte_pipeline_port_in_action_handler
get_port_in_ah(struct pipeline_passthrough *p)
{
	if ((p->params.dma_enabled == 0) &&
		(p->params.swap_enabled == 0))
		return NULL;

	if (p->params.swap_enabled)
		return port_in_ah_swap;

	if (p->params.dma_hash_enabled) {
		if (p->params.dma_hash_lb_enabled) {
			if (rte_is_power_of_2(p->p.n_ports_out))
				switch (p->params.dma_size) {

				case 8: return port_in_ah_lb_size8_hash1_lb1_pw1;
				case 16: return port_in_ah_lb_size16_hash1_lb1_pw1;
				case 24: return port_in_ah_lb_size24_hash1_lb1_pw1;
				case 32: return port_in_ah_lb_size32_hash1_lb1_pw1;
				case 40: return port_in_ah_lb_size40_hash1_lb1_pw1;
				case 48: return port_in_ah_lb_size48_hash1_lb1_pw1;
				case 56: return port_in_ah_lb_size56_hash1_lb1_pw1;
				case 64: return port_in_ah_lb_size64_hash1_lb1_pw1;
				default: return NULL;
				}
			else
				switch (p->params.dma_size) {

				case 8: return port_in_ah_lb_size8_hash1_lb1_pw0;
				case 16: return port_in_ah_lb_size16_hash1_lb1_pw0;
				case 24: return port_in_ah_lb_size24_hash1_lb1_pw0;
				case 32: return port_in_ah_lb_size32_hash1_lb1_pw0;
				case 40: return port_in_ah_lb_size40_hash1_lb1_pw0;
				case 48: return port_in_ah_lb_size48_hash1_lb1_pw0;
				case 56: return port_in_ah_lb_size56_hash1_lb1_pw0;
				case 64: return port_in_ah_lb_size64_hash1_lb1_pw0;
				default: return NULL;
			}
		} else
			switch (p->params.dma_size) {

			case 8: return port_in_ah_dma_size8_hash1_lb0_pw0;
			case 16: return port_in_ah_dma_size16_hash1_lb0_pw0;
			case 24: return port_in_ah_dma_size24_hash1_lb0_pw0;
			case 32: return port_in_ah_dma_size32_hash1_lb0_pw0;
			case 40: return port_in_ah_dma_size40_hash1_lb0_pw0;
			case 48: return port_in_ah_dma_size48_hash1_lb0_pw0;
			case 56: return port_in_ah_dma_size56_hash1_lb0_pw0;
			case 64: return port_in_ah_dma_size64_hash1_lb0_pw0;
			default: return NULL;
		}
	} else
		switch (p->params.dma_size) {

		case 8: return port_in_ah_dma_size8_hash0_lb0_pw0;
		case 16: return port_in_ah_dma_size16_hash0_lb0_pw0;
		case 24: return port_in_ah_dma_size24_hash0_lb0_pw0;
		case 32: return port_in_ah_dma_size32_hash0_lb0_pw0;
		case 40: return port_in_ah_dma_size40_hash0_lb0_pw0;
		case 48: return port_in_ah_dma_size48_hash0_lb0_pw0;
		case 56: return port_in_ah_dma_size56_hash0_lb0_pw0;
		case 64: return port_in_ah_dma_size64_hash0_lb0_pw0;
		default: return NULL;
		}
}

int
pipeline_passthrough_parse_args(struct pipeline_passthrough_params *p,
	struct pipeline_params *params)
{
	uint32_t dma_dst_offset_present = 0;
	uint32_t dma_src_offset_present = 0;
	uint32_t dma_src_mask_present = 0;
	char dma_mask_str[PIPELINE_PASSTHROUGH_DMA_SIZE_MAX * 2 + 1];
	uint32_t dma_size_present = 0;
	uint32_t dma_hash_offset_present = 0;
	uint32_t dma_hash_lb_present = 0;
	uint32_t i;

	/* default values */
	p->dma_enabled = 0;
	p->dma_hash_enabled = 0;
	p->dma_hash_lb_enabled = 0;
	memset(p->dma_src_mask, 0xFF, sizeof(p->dma_src_mask));
	p->swap_enabled = 0;
	p->swap_n_fields = 0;

	for (i = 0; i < params->n_args; i++) {
		char *arg_name = params->args_name[i];
		char *arg_value = params->args_value[i];

		/* dma_dst_offset */
		if (strcmp(arg_name, "dma_dst_offset") == 0) {
			int status;

			PIPELINE_PARSE_ERR_DUPLICATE(
				dma_dst_offset_present == 0, params->name,
				arg_name);
			dma_dst_offset_present = 1;

			status = parser_read_uint32(&p->dma_dst_offset,
				arg_value);
			PIPELINE_PARSE_ERR_INV_VAL((status != -EINVAL),
				params->name, arg_name, arg_value);
			PIPELINE_PARSE_ERR_OUT_RNG((status != -ERANGE),
				params->name, arg_name, arg_value);

			p->dma_enabled = 1;

			continue;
		}

		/* dma_src_offset */
		if (strcmp(arg_name, "dma_src_offset") == 0) {
			int status;

			PIPELINE_PARSE_ERR_DUPLICATE(
				dma_src_offset_present == 0, params->name,
				arg_name);
			dma_src_offset_present = 1;

			status = parser_read_uint32(&p->dma_src_offset,
				arg_value);
			PIPELINE_PARSE_ERR_INV_VAL((status != -EINVAL),
				params->name, arg_name, arg_value);
			PIPELINE_PARSE_ERR_OUT_RNG((status != -ERANGE),
				params->name, arg_name, arg_value);

			p->dma_enabled = 1;

			continue;
		}

		/* dma_size */
		if (strcmp(arg_name, "dma_size") == 0) {
			int status;

			PIPELINE_PARSE_ERR_DUPLICATE(
				dma_size_present == 0, params->name,
				arg_name);
			dma_size_present = 1;

			status = parser_read_uint32(&p->dma_size,
				arg_value);
			PIPELINE_PARSE_ERR_INV_VAL(((status != -EINVAL) &&
				(p->dma_size != 0) &&
				((p->dma_size % 8) == 0)),
				params->name, arg_name, arg_value);
			PIPELINE_PARSE_ERR_OUT_RNG(((status != -ERANGE) &&
				(p->dma_size <=
				PIPELINE_PASSTHROUGH_DMA_SIZE_MAX)),
				params->name, arg_name, arg_value);

			p->dma_enabled = 1;

			continue;
		}

		/* dma_src_mask */
		if (strcmp(arg_name, "dma_src_mask") == 0) {
			int mask_str_len = strlen(arg_value);

			PIPELINE_PARSE_ERR_DUPLICATE(
				dma_src_mask_present == 0,
				params->name, arg_name);
			dma_src_mask_present = 1;

			PIPELINE_ARG_CHECK((mask_str_len <=
				(PIPELINE_PASSTHROUGH_DMA_SIZE_MAX * 2)),
				"Parse error in section \"%s\": entry "
				"\"%s\" too long", params->name,
				arg_name);

			snprintf(dma_mask_str, mask_str_len + 1,
				"%s", arg_value);

			p->dma_enabled = 1;

			continue;
		}

		/* dma_hash_offset */
		if (strcmp(arg_name, "dma_hash_offset") == 0) {
			int status;

			PIPELINE_PARSE_ERR_DUPLICATE(
				dma_hash_offset_present == 0,
				params->name, arg_name);
			dma_hash_offset_present = 1;

			status = parser_read_uint32(&p->dma_hash_offset,
				arg_value);
			PIPELINE_PARSE_ERR_INV_VAL((status != -EINVAL),
				params->name, arg_name, arg_value);
			PIPELINE_PARSE_ERR_OUT_RNG((status != -ERANGE),
				params->name, arg_name, arg_value);

			p->dma_hash_enabled = 1;

			continue;
		}

		/* load_balance mode */
		if (strcmp(arg_name, "lb") == 0) {
			PIPELINE_PARSE_ERR_DUPLICATE(
				dma_hash_lb_present == 0,
				params->name, arg_name);
			dma_hash_lb_present = 1;

			if (strcmp(arg_value, "hash") &&
				strcmp(arg_value, "HASH"))

				PIPELINE_PARSE_ERR_INV_VAL(0,
					params->name,
					arg_name,
					arg_value);

			p->dma_hash_lb_enabled = 1;

			continue;
		}

		/* swap */
		if (strcmp(arg_name, "swap") == 0) {
			uint32_t a, b, n_args;
			int len;

			n_args = sscanf(arg_value, "%" SCNu32 " %" SCNu32 "%n",
				&a, &b, &len);
			PIPELINE_PARSE_ERR_INV_VAL(((n_args == 2) &&
				((size_t) len == strlen(arg_value))),
				params->name, arg_name, arg_value);

			p->swap_field0_offset[p->swap_n_fields] = a;
			p->swap_field1_offset[p->swap_n_fields] = b;
			p->swap_n_fields++;
			p->swap_enabled = 1;

			continue;
		}

		/* any other */
		PIPELINE_PARSE_ERR_INV_ENT(0, params->name, arg_name);
	}

	/* Check correlations between arguments */
	PIPELINE_ARG_CHECK((p->dma_enabled + p->swap_enabled < 2),
		"Parse error in section \"%s\": DMA and SWAP actions are both enabled",
		params->name);
	PIPELINE_ARG_CHECK((dma_dst_offset_present == p->dma_enabled),
		"Parse error in section \"%s\": missing entry "
		"\"dma_dst_offset\"", params->name);
	PIPELINE_ARG_CHECK((dma_src_offset_present == p->dma_enabled),
		"Parse error in section \"%s\": missing entry "
		"\"dma_src_offset\"", params->name);
	PIPELINE_ARG_CHECK((dma_size_present == p->dma_enabled),
		"Parse error in section \"%s\": missing entry "
		"\"dma_size\"", params->name);
	PIPELINE_ARG_CHECK((p->dma_hash_enabled <= p->dma_enabled),
		"Parse error in section \"%s\": missing all DMA entries",
		params->name);
	PIPELINE_ARG_CHECK((p->dma_hash_lb_enabled <= p->dma_hash_enabled),
		"Parse error in section \"%s\": missing all DMA hash entries ",
		params->name);

	if (dma_src_mask_present) {
		uint32_t dma_size = p->dma_size;
		int status;

		PIPELINE_ARG_CHECK((strlen(dma_mask_str) ==
			(dma_size * 2)), "Parse error in section "
			"\"%s\": dma_src_mask should have exactly %u hex "
			"digits", params->name, (dma_size * 2));

		status = parse_hex_string(dma_mask_str, p->dma_src_mask,
			&p->dma_size);

		PIPELINE_PARSE_ERR_INV_VAL(((status == 0) &&
			(dma_size == p->dma_size)), params->name,
			"dma_src_mask", dma_mask_str);
	}

	if (p->dma_hash_lb_enabled)
		PIPELINE_ARG_CHECK((params->n_ports_out > 1),
			"Parse error in section \"%s\": entry \"lb\" not "
			"allowed for single output port pipeline",
			params->name);
	else
		PIPELINE_ARG_CHECK(((params->n_ports_in >= params->n_ports_out)
			&& ((params->n_ports_in % params->n_ports_out) == 0)),
			"Parse error in section \"%s\": n_ports_in needs to be "
			"a multiple of n_ports_out (lb mode disabled)",
			params->name);

	return 0;
}

static rte_table_hash_op_hash
get_hash_function(struct pipeline_passthrough *p)
{
	switch (p->params.dma_size) {

	case 8: return hash_default_key8;
	case 16: return hash_default_key16;
	case 24: return hash_default_key24;
	case 32: return hash_default_key32;
	case 40: return hash_default_key40;
	case 48: return hash_default_key48;
	case 56: return hash_default_key56;
	case 64: return hash_default_key64;
	default: return NULL;
	}
}

static int
pipeline_passthrough_swap_convert(struct pipeline_passthrough *p)
{
	uint32_t i;

	p->swap_n_fields = 0;

	for (i = 0; i < p->params.swap_n_fields; i++) {
		uint32_t offset0 = p->params.swap_field0_offset[i];
		uint32_t offset1 = p->params.swap_field1_offset[i];
		uint32_t size = offset1 - offset0;
		uint32_t j;

		/* Check */
		if ((offset0 >= offset1) ||
			(size > PIPELINE_PASSTHROUGH_SWAP_FIELD_SIZE_MAX) ||
			(p->swap_n_fields >= SWAP_DIM))
			return -1;

		for (j = 0; j < (size / sizeof(uint64_t)); j++) {
			p->swap_field0_offset[p->swap_n_fields] = offset0;
			p->swap_field1_offset[p->swap_n_fields] = offset1;
			p->swap_field_mask[p->swap_n_fields] = UINT64_MAX;
			p->swap_n_fields++;
			offset0 += sizeof(uint64_t);
			offset1 += sizeof(uint64_t);
		}
		if (size % sizeof(uint64_t)) {
			uint32_t n_bits = (size % sizeof(uint64_t)) * 8;

			p->swap_field0_offset[p->swap_n_fields] = offset0;
			p->swap_field1_offset[p->swap_n_fields] = offset1;
			p->swap_field_mask[p->swap_n_fields] =
				RTE_LEN2MASK(n_bits, uint64_t);
			p->swap_n_fields++;
		}
	}

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
		(params->n_ports_out == 0))
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
	if (pipeline_passthrough_parse_args(&p_pt->params, params))
		return NULL;
	if (pipeline_passthrough_swap_convert(p_pt))
		return NULL;
	p_pt->f_hash = get_hash_function(p_pt);

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

	p->n_ports_in = params->n_ports_in;
	p->n_ports_out = params->n_ports_out;
	p->n_tables = p->n_ports_in;

	/*Input ports*/
	for (i = 0; i < p->n_ports_in; i++) {
		struct rte_pipeline_port_in_params port_params = {
			.ops = pipeline_port_in_params_get_ops(
				&params->port_in[i]),
			.arg_create = pipeline_port_in_params_convert(
				&params->port_in[i]),
			.f_action = get_port_in_ah(p_pt),
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
	for (i = 0; i < p->n_ports_out; i++) {
		struct rte_pipeline_port_out_params port_params = {
			.ops = pipeline_port_out_params_get_ops(
				&params->port_out[i]),
			.arg_create = pipeline_port_out_params_convert(
				&params->port_out[i]),
			.f_action = NULL,
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
		uint32_t port_out_id = (p_pt->params.dma_hash_lb_enabled == 0) ?
			(i / (p->n_ports_in / p->n_ports_out)) :
			0;

		struct rte_pipeline_table_entry default_entry = {
			.action = RTE_PIPELINE_ACTION_PORT,
			{.port_id = p->port_out_id[port_out_id]},
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

struct pipeline_be_ops pipeline_passthrough_be_ops = {
	.f_init = pipeline_passthrough_init,
	.f_free = pipeline_passthrough_free,
	.f_run = NULL,
	.f_timer = pipeline_passthrough_timer,
};
