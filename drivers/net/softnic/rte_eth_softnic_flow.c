/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include "rte_eth_softnic_internals.h"
#include "rte_eth_softnic.h"

int
flow_attr_map_set(struct pmd_internals *softnic,
		uint32_t group_id,
		int ingress,
		const char *pipeline_name,
		uint32_t table_id)
{
	struct pipeline *pipeline;
	struct flow_attr_map *map;

	if (group_id >= SOFTNIC_FLOW_MAX_GROUPS ||
			pipeline_name == NULL)
		return -1;

	pipeline = softnic_pipeline_find(softnic, pipeline_name);
	if (pipeline == NULL ||
			table_id >= pipeline->n_tables)
		return -1;

	map = (ingress) ? &softnic->flow.ingress_map[group_id] :
		&softnic->flow.egress_map[group_id];
	strcpy(map->pipeline_name, pipeline_name);
	map->table_id = table_id;
	map->valid = 1;

	return 0;
}

struct flow_attr_map *
flow_attr_map_get(struct pmd_internals *softnic,
		uint32_t group_id,
		int ingress)
{
	if (group_id >= SOFTNIC_FLOW_MAX_GROUPS)
		return NULL;

	return (ingress) ? &softnic->flow.ingress_map[group_id] :
		&softnic->flow.egress_map[group_id];
}
