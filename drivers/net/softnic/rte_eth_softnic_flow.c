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

union flow_item {
	uint8_t raw[TABLE_RULE_MATCH_SIZE_MAX];
	struct rte_flow_item_eth eth;
	struct rte_flow_item_vlan vlan;
	struct rte_flow_item_ipv4 ipv4;
	struct rte_flow_item_ipv6 ipv6;
	struct rte_flow_item_icmp icmp;
	struct rte_flow_item_udp udp;
	struct rte_flow_item_tcp tcp;
	struct rte_flow_item_sctp sctp;
	struct rte_flow_item_vxlan vxlan;
	struct rte_flow_item_e_tag e_tag;
	struct rte_flow_item_nvgre nvgre;
	struct rte_flow_item_mpls mpls;
	struct rte_flow_item_gre gre;
	struct rte_flow_item_gtp gtp;
	struct rte_flow_item_esp esp;
	struct rte_flow_item_geneve geneve;
	struct rte_flow_item_vxlan_gpe vxlan_gpe;
	struct rte_flow_item_arp_eth_ipv4 arp_eth_ipv4;
	struct rte_flow_item_ipv6_ext ipv6_ext;
	struct rte_flow_item_icmp6 icmp6;
	struct rte_flow_item_icmp6_nd_ns icmp6_nd_ns;
	struct rte_flow_item_icmp6_nd_na icmp6_nd_na;
	struct rte_flow_item_icmp6_nd_opt icmp6_nd_opt;
	struct rte_flow_item_icmp6_nd_opt_sla_eth icmp6_nd_opt_sla_eth;
	struct rte_flow_item_icmp6_nd_opt_tla_eth icmp6_nd_opt_tla_eth;
};

static const union flow_item flow_item_raw_mask;

static int
flow_item_is_proto(enum rte_flow_item_type type,
	const void **mask,
	size_t *size)
{
	switch (type) {
	case RTE_FLOW_ITEM_TYPE_RAW:
		*mask = &flow_item_raw_mask;
		*size = sizeof(flow_item_raw_mask);
		return 1; /* TRUE */

	case RTE_FLOW_ITEM_TYPE_ETH:
		*mask = &rte_flow_item_eth_mask;
		*size = sizeof(struct rte_flow_item_eth);
		return 1; /* TRUE */

	case RTE_FLOW_ITEM_TYPE_VLAN:
		*mask = &rte_flow_item_vlan_mask;
		*size = sizeof(struct rte_flow_item_vlan);
		return 1;

	case RTE_FLOW_ITEM_TYPE_IPV4:
		*mask = &rte_flow_item_ipv4_mask;
		*size = sizeof(struct rte_flow_item_ipv4);
		return 1;

	case RTE_FLOW_ITEM_TYPE_IPV6:
		*mask = &rte_flow_item_ipv6_mask;
		*size = sizeof(struct rte_flow_item_ipv6);
		return 1;

	case RTE_FLOW_ITEM_TYPE_ICMP:
		*mask = &rte_flow_item_icmp_mask;
		*size = sizeof(struct rte_flow_item_icmp);
		return 1;

	case RTE_FLOW_ITEM_TYPE_UDP:
		*mask = &rte_flow_item_udp_mask;
		*size = sizeof(struct rte_flow_item_udp);
		return 1;

	case RTE_FLOW_ITEM_TYPE_TCP:
		*mask = &rte_flow_item_tcp_mask;
		*size = sizeof(struct rte_flow_item_tcp);
		return 1;

	case RTE_FLOW_ITEM_TYPE_SCTP:
		*mask = &rte_flow_item_sctp_mask;
		*size = sizeof(struct rte_flow_item_sctp);
		return 1;

	case RTE_FLOW_ITEM_TYPE_VXLAN:
		*mask = &rte_flow_item_vxlan_mask;
		*size = sizeof(struct rte_flow_item_vxlan);
		return 1;

	case RTE_FLOW_ITEM_TYPE_E_TAG:
		*mask = &rte_flow_item_e_tag_mask;
		*size = sizeof(struct rte_flow_item_e_tag);
		return 1;

	case RTE_FLOW_ITEM_TYPE_NVGRE:
		*mask = &rte_flow_item_nvgre_mask;
		*size = sizeof(struct rte_flow_item_nvgre);
		return 1;

	case RTE_FLOW_ITEM_TYPE_MPLS:
		*mask = &rte_flow_item_mpls_mask;
		*size = sizeof(struct rte_flow_item_mpls);
		return 1;

	case RTE_FLOW_ITEM_TYPE_GRE:
		*mask = &rte_flow_item_gre_mask;
		*size = sizeof(struct rte_flow_item_gre);
		return 1;

	case RTE_FLOW_ITEM_TYPE_GTP:
	case RTE_FLOW_ITEM_TYPE_GTPC:
	case RTE_FLOW_ITEM_TYPE_GTPU:
		*mask = &rte_flow_item_gtp_mask;
		*size = sizeof(struct rte_flow_item_gtp);
		return 1;

	case RTE_FLOW_ITEM_TYPE_ESP:
		*mask = &rte_flow_item_esp_mask;
		*size = sizeof(struct rte_flow_item_esp);
		return 1;

	case RTE_FLOW_ITEM_TYPE_GENEVE:
		*mask = &rte_flow_item_geneve_mask;
		*size = sizeof(struct rte_flow_item_geneve);
		return 1;

	case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
		*mask = &rte_flow_item_vxlan_gpe_mask;
		*size = sizeof(struct rte_flow_item_vxlan_gpe);
		return 1;

	case RTE_FLOW_ITEM_TYPE_ARP_ETH_IPV4:
		*mask = &rte_flow_item_arp_eth_ipv4_mask;
		*size = sizeof(struct rte_flow_item_arp_eth_ipv4);
		return 1;

	case RTE_FLOW_ITEM_TYPE_IPV6_EXT:
		*mask = &rte_flow_item_ipv6_ext_mask;
		*size = sizeof(struct rte_flow_item_ipv6_ext);
		return 1;

	case RTE_FLOW_ITEM_TYPE_ICMP6:
		*mask = &rte_flow_item_icmp6_mask;
		*size = sizeof(struct rte_flow_item_icmp6);
		return 1;

	case RTE_FLOW_ITEM_TYPE_ICMP6_ND_NS:
		*mask = &rte_flow_item_icmp6_nd_ns_mask;
		*size = sizeof(struct rte_flow_item_icmp6_nd_ns);
		return 1;

	case RTE_FLOW_ITEM_TYPE_ICMP6_ND_NA:
		*mask = &rte_flow_item_icmp6_nd_na_mask;
		*size = sizeof(struct rte_flow_item_icmp6_nd_na);
		return 1;

	case RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT:
		*mask = &rte_flow_item_icmp6_nd_opt_mask;
		*size = sizeof(struct rte_flow_item_icmp6_nd_opt);
		return 1;

	case RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT_SLA_ETH:
		*mask = &rte_flow_item_icmp6_nd_opt_sla_eth_mask;
		*size = sizeof(struct rte_flow_item_icmp6_nd_opt_sla_eth);
		return 1;

	case RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT_TLA_ETH:
		*mask = &rte_flow_item_icmp6_nd_opt_tla_eth_mask;
		*size = sizeof(struct rte_flow_item_icmp6_nd_opt_tla_eth);
		return 1;

	default: return 0; /* FALSE */
	}
}

static int
flow_item_proto_preprocess(const struct rte_flow_item *item,
	union flow_item *item_spec,
	union flow_item *item_mask,
	size_t *item_size,
	int *item_disabled,
	struct rte_flow_error *error)
{
	const void *mask_default;
	uint8_t *spec = (uint8_t *)item_spec;
	uint8_t *mask = (uint8_t *)item_mask;
	size_t size, i;

	if (!flow_item_is_proto(item->type, &mask_default, &size))
		return rte_flow_error_set(error,
			ENOTSUP,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item,
			"Item type not supported");

	/* spec */
	if (!item->spec) {
		/* If spec is NULL, then last and mask also have to be NULL. */
		if (item->last || item->mask)
			return rte_flow_error_set(error,
				EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item,
				"Invalid item (NULL spec with non-NULL last or mask)");

		memset(item_spec, 0, size);
		memset(item_mask, 0, size);
		*item_size = size;
		*item_disabled = 1; /* TRUE */
		return 0;
	}

	memcpy(spec, item->spec, size);
	*item_size = size;

	/* mask */
	if (item->mask)
		memcpy(mask, item->mask, size);
	else
		memcpy(mask, mask_default, size);

	/* disabled */
	for (i = 0; i < size; i++)
		if (mask[i])
			break;
	*item_disabled = (i == size) ? 1 : 0;

	/* Apply mask over spec. */
	for (i = 0; i < size; i++)
		spec[i] &= mask[i];

	/* last */
	if (item->last) {
		uint8_t last[size];

		/* init last */
		memcpy(last, item->last, size);
		for (i = 0; i < size; i++)
			last[i] &= mask[i];

		/* check for range */
		for (i = 0; i < size; i++)
			if (last[i] != spec[i])
				return rte_flow_error_set(error,
					ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Range not supported");
	}

	return 0;
}

/***
 * Skip disabled protocol items and VOID items
 * until any of the mutually exclusive conditions
 * from the list below takes place:
 *    (A) A protocol present in the proto_mask
 *        is met (either ENABLED or DISABLED);
 *    (B) A protocol NOT present in the proto_mask is met in ENABLED state;
 *    (C) The END item is met.
 */
static int
flow_item_skip_disabled_protos(const struct rte_flow_item **item,
	uint64_t proto_mask,
	size_t *length,
	struct rte_flow_error *error)
{
	size_t len = 0;

	for ( ; (*item)->type != RTE_FLOW_ITEM_TYPE_END; (*item)++) {
		union flow_item spec, mask;
		size_t size;
		int disabled = 0, status;

		if ((*item)->type == RTE_FLOW_ITEM_TYPE_VOID)
			continue;

		status = flow_item_proto_preprocess(*item,
				&spec,
				&mask,
				&size,
				&disabled,
				error);
		if (status)
			return status;

		if ((proto_mask & (1LLU << (*item)->type)) ||
				!disabled)
			break;

		len += size;
	}

	if (length)
		*length = len;

	return 0;
}

#define FLOW_ITEM_PROTO_IP \
	((1LLU << RTE_FLOW_ITEM_TYPE_IPV4) | \
	 (1LLU << RTE_FLOW_ITEM_TYPE_IPV6))

static int
flow_rule_match_acl_get(struct pmd_internals *softnic __rte_unused,
		struct pipeline *pipeline __rte_unused,
		struct softnic_table *table __rte_unused,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item *item,
		struct softnic_table_rule_match *rule_match,
		struct rte_flow_error *error)
{
	union flow_item spec, mask;
	size_t size, length = 0;
	int disabled = 0, status;

	memset(rule_match, 0, sizeof(*rule_match));
	rule_match->match_type = TABLE_ACL;
	rule_match->match.acl.priority = attr->priority;

	/* VOID or disabled protos only, if any. */
	status = flow_item_skip_disabled_protos(&item,
			FLOW_ITEM_PROTO_IP, &length, error);
	if (status)
		return status;

	/* IP only. */
	status = flow_item_proto_preprocess(item, &spec, &mask,
			&size, &disabled, error);
	if (status)
		return status;

	switch (item->type) {
	default:
		return rte_flow_error_set(error,
			ENOTSUP,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item,
			"ACL: IP protocol required");
	} /* switch */
}

static int
flow_rule_match_get(struct pmd_internals *softnic,
		struct pipeline *pipeline,
		struct softnic_table *table,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item *item,
		struct softnic_table_rule_match *rule_match,
		struct rte_flow_error *error)
{
	switch (table->params.match_type) {
	case TABLE_ACL:
		return flow_rule_match_acl_get(softnic,
			pipeline,
			table,
			attr,
			item,
			rule_match,
			error);
		/* FALLTHROUGH */
	default:
		return rte_flow_error_set(error,
			ENOTSUP,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			NULL,
			"Unsupported pipeline table match type");
	}
}

static int
pmd_flow_validate(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item item[],
		const struct rte_flow_action action[],
		struct rte_flow_error *error)
{
	struct softnic_table_rule_match rule_match;

	struct pmd_internals *softnic = dev->data->dev_private;
	struct pipeline *pipeline;
	struct softnic_table *table;
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

	table = &pipeline->table[table_id];

	/* Rule match. */
	memset(&rule_match, 0, sizeof(rule_match));
	status = flow_rule_match_get(softnic,
			pipeline,
			table,
			attr,
			item,
			&rule_match,
			error);
	if (status)
		return status;

	return 0;
}

const struct rte_flow_ops pmd_flow_ops = {
	.validate = pmd_flow_validate,
};
