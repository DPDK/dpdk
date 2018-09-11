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

static int
flow_pipeline_table_get(struct pmd_internals *softnic,
		const struct rte_flow_attr *attr,
		const char **pipeline_name,
		uint32_t *table_id,
		struct rte_flow_error *error)
{
	struct flow_attr_map *map;

	if (attr == NULL)
		return rte_flow_error_set(error,
				EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR,
				NULL,
				"Null attr");

	if (!attr->ingress && !attr->egress)
		return rte_flow_error_set(error,
				EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
				attr,
				"Ingress/egress not specified");

	if (attr->ingress && attr->egress)
		return rte_flow_error_set(error,
				EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
				attr,
				"Setting both ingress and egress is not allowed");

	map = flow_attr_map_get(softnic,
			attr->group,
			attr->ingress);
	if (map == NULL ||
			map->valid == 0)
		return rte_flow_error_set(error,
				EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
				attr,
				"Invalid group ID");

	if (pipeline_name)
		*pipeline_name = map->pipeline_name;

	if (table_id)
		*table_id = map->table_id;

	return 0;
}

static int
pmd_flow_validate(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item item[],
		const struct rte_flow_action action[],
		struct rte_flow_error *error)
{
	struct pmd_internals *softnic = dev->data->dev_private;
	struct pipeline *pipeline;
	const char *pipeline_name = NULL;
	uint32_t table_id = 0;
	int status;

	/* Check input parameters. */
	if (attr == NULL)
		return rte_flow_error_set(error,
				EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR,
				NULL, "Null attr");

	if (item == NULL)
		return rte_flow_error_set(error,
				EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				NULL,
				"Null item");

	if (action == NULL)
		return rte_flow_error_set(error,
				EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION,
				NULL,
				"Null action");

	/* Identify the pipeline table to add this flow to. */
	status = flow_pipeline_table_get(softnic, attr, &pipeline_name,
					&table_id, error);
	if (status)
		return status;

	pipeline = softnic_pipeline_find(softnic, pipeline_name);
	if (pipeline == NULL)
		return rte_flow_error_set(error,
				EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"Invalid pipeline name");

	if (table_id >= pipeline->n_tables)
		return rte_flow_error_set(error,
				EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"Invalid pipeline table ID");

	return 0;
}

const struct rte_flow_ops pmd_flow_ops = {
	.validate = pmd_flow_validate,
};
