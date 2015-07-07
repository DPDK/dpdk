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

#ifndef __INCLUDE_PIPELINE_FLOW_CLASSIFICATION_H__
#define __INCLUDE_PIPELINE_FLOW_CLASSIFICATION_H__

#include "pipeline.h"
#include "pipeline_flow_classification_be.h"

enum flow_key_type {
	FLOW_KEY_QINQ,
	FLOW_KEY_IPV4_5TUPLE,
	FLOW_KEY_IPV6_5TUPLE,
};

struct flow_key_qinq {
	uint16_t svlan;
	uint16_t cvlan;
};

struct flow_key_ipv4_5tuple {
	uint32_t ip_src;
	uint32_t ip_dst;
	uint16_t port_src;
	uint16_t port_dst;
	uint32_t proto;
};

struct flow_key_ipv6_5tuple {
	uint8_t ip_src[16];
	uint8_t ip_dst[16];
	uint16_t port_src;
	uint16_t port_dst;
	uint32_t proto;
};

struct pipeline_fc_key {
	enum flow_key_type type;
	union {
		struct flow_key_qinq qinq;
		struct flow_key_ipv4_5tuple ipv4_5tuple;
		struct flow_key_ipv6_5tuple ipv6_5tuple;
	} key;
};

int
app_pipeline_fc_add(struct app_params *app,
	uint32_t pipeline_id,
	struct pipeline_fc_key *key,
	uint32_t port_id);

int
app_pipeline_fc_add_bulk(struct app_params *app,
	uint32_t pipeline_id,
	struct pipeline_fc_key *key,
	uint32_t *port_id,
	uint32_t n_keys);

int
app_pipeline_fc_del(struct app_params *app,
	uint32_t pipeline_id,
	struct pipeline_fc_key *key);

int
app_pipeline_fc_add_default(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t port_id);

int
app_pipeline_fc_del_default(struct app_params *app,
	uint32_t pipeline_id);

extern struct pipeline_type pipeline_flow_classification;

#endif
