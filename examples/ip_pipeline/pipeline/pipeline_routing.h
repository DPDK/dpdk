/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
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
