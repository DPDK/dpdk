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

#ifndef __INCLUDE_PIPELINE_ROUTING_H__
#define __INCLUDE_PIPELINE_ROUTING_H__

#include "pipeline.h"
#include "pipeline_routing_be.h"

/*
 * Route
 */

int
app_pipeline_routing_add_route(struct app_params *app,
	uint32_t pipeline_id,
	struct pipeline_routing_route_key *key,
	struct pipeline_routing_route_data *data);

int
app_pipeline_routing_delete_route(struct app_params *app,
	uint32_t pipeline_id,
	struct pipeline_routing_route_key *key);

int
app_pipeline_routing_add_default_route(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t port_id);

int
app_pipeline_routing_delete_default_route(struct app_params *app,
	uint32_t pipeline_id);

/*
 * ARP
 */

int
app_pipeline_routing_add_arp_entry(struct app_params *app,
	uint32_t pipeline_id,
	struct pipeline_routing_arp_key *key,
	struct ether_addr *macaddr);

int
app_pipeline_routing_delete_arp_entry(struct app_params *app,
	uint32_t pipeline_id,
	struct pipeline_routing_arp_key *key);

int
app_pipeline_routing_add_default_arp_entry(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t port_id);

int
app_pipeline_routing_delete_default_arp_entry(struct app_params *app,
	uint32_t pipeline_id);

/*
 * SETTINGS
 */
int
app_pipeline_routing_set_macaddr(struct app_params *app,
	uint32_t pipeline_id);

/*
 * Pipeline type
 */
extern struct pipeline_type pipeline_routing;

#endif
