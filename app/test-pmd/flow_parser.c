/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2016 Mellanox Technologies, Ltd
 * Copyright 2026 DynaNIC Semiconductors, Ltd.
 */

#include <errno.h>
#include <string.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_geneve.h>
#include <rte_gre.h>
#include <rte_gtp.h>
#include <rte_mpls.h>
#include <rte_string_fns.h>
#include <rte_vxlan.h>
#include <rte_ip.h>
#include <rte_flow.h>
#include <rte_flow_parser_private.h>

#include "testpmd.h"

static struct rte_port *
parser_port_get(uint16_t port_id)
{
	if (port_id_is_invalid(port_id, DISABLED_WARN) != 0 ||
	    port_id == (portid_t)RTE_PORT_ALL)
		return NULL;
	return &ports[port_id];
}

static struct port_flow *
parser_flow_by_index(struct rte_port *port, unsigned int index)
{
	struct port_flow *pf = port->flow_list;

	while (pf && index--)
		pf = pf->next;
	return pf;
}

static struct port_template *
parser_template_by_index(struct port_template *list, unsigned int index)
{
	struct port_template *pt = list;

	while (pt && index--)
		pt = pt->next;
	return pt;
}

static struct port_table *
parser_table_by_index(struct port_table *list, unsigned int index)
{
	struct port_table *pt = list;

	while (pt && index--)
		pt = pt->next;
	return pt;
}

static const struct tunnel_ops *
parser_tunnel_convert(const struct rte_flow_parser_tunnel_ops *src,
		      struct tunnel_ops *dst)
{
	if (src == NULL)
		return NULL;
	memset(dst, 0, sizeof(*dst));
	dst->id = src->id;
	strlcpy(dst->type, src->type, sizeof(dst->type));
	dst->enabled = src->enabled;
	dst->actions = src->actions;
	dst->items = src->items;
	return dst;
}

static int
parser_port_validate(uint16_t port_id)
{
	return port_id_is_invalid(port_id, DISABLED_WARN);
}

static uint16_t
parser_flow_rule_count(uint16_t port_id)
{
	struct rte_port *port = parser_port_get(port_id);
	uint16_t count = 0;

	if (port == NULL)
		return 0;
	for (struct port_flow *pf = port->flow_list; pf; pf = pf->next)
		count++;
	return count;
}

static int
parser_flow_rule_id_get(uint16_t port_id, unsigned int index,
			uint64_t *rule_id)
{
	struct rte_port *port = parser_port_get(port_id);
	struct port_flow *pf;

	if (port == NULL || rule_id == NULL)
		return -ENOENT;
	pf = parser_flow_by_index(port, index);
	if (pf == NULL)
		return -ENOENT;
	*rule_id = pf->id;
	return 0;
}

static uint16_t
parser_pattern_template_count(uint16_t port_id)
{
	struct rte_port *port = parser_port_get(port_id);
	uint16_t count = 0;

	if (port == NULL)
		return 0;
	for (struct port_template *pt = port->pattern_templ_list;
	     pt;
	     pt = pt->next)
		count++;
	return count;
}

static int
parser_pattern_template_id_get(uint16_t port_id, unsigned int index,
			       uint32_t *template_id)
{
	struct rte_port *port = parser_port_get(port_id);
	struct port_template *pt;

	if (port == NULL || template_id == NULL)
		return -ENOENT;
	pt = parser_template_by_index(port->pattern_templ_list, index);
	if (pt == NULL)
		return -ENOENT;
	*template_id = pt->id;
	return 0;
}

static uint16_t
parser_actions_template_count(uint16_t port_id)
{
	struct rte_port *port = parser_port_get(port_id);
	uint16_t count = 0;

	if (port == NULL)
		return 0;
	for (struct port_template *pt = port->actions_templ_list;
	     pt;
	     pt = pt->next)
		count++;
	return count;
}

static int
parser_actions_template_id_get(uint16_t port_id, unsigned int index,
			       uint32_t *template_id)
{
	struct rte_port *port = parser_port_get(port_id);
	struct port_template *pt;

	if (port == NULL || template_id == NULL)
		return -ENOENT;
	pt = parser_template_by_index(port->actions_templ_list, index);
	if (pt == NULL)
		return -ENOENT;
	*template_id = pt->id;
	return 0;
}

static uint16_t
parser_table_count(uint16_t port_id)
{
	struct rte_port *port = parser_port_get(port_id);
	uint16_t count = 0;

	if (port == NULL)
		return 0;
	for (struct port_table *pt = port->table_list; pt; pt = pt->next)
		count++;
	return count;
}

static int
parser_table_id_get(uint16_t port_id, unsigned int index,
		    uint32_t *table_id)
{
	struct rte_port *port = parser_port_get(port_id);
	struct port_table *pt;

	if (port == NULL || table_id == NULL)
		return -ENOENT;
	pt = parser_table_by_index(port->table_list, index);
	if (pt == NULL)
		return -ENOENT;
	*table_id = pt->id;
	return 0;
}

static uint16_t
parser_queue_count(uint16_t port_id)
{
	struct rte_port *port = parser_port_get(port_id);

	if (port == NULL)
		return 0;
	return port->queue_nb;
}

static uint16_t
parser_rss_queue_count(uint16_t port_id)
{
	struct rte_port *port = parser_port_get(port_id);

	if (port == NULL)
		return 0;
	return port->queue_nb ? port->queue_nb : port->dev_info.max_rx_queues;
}

static struct rte_flow_template_table *
parser_table_get(uint16_t port_id, uint32_t table_id)
{
	struct rte_port *port = parser_port_get(port_id);
	struct port_table *pt;

	if (port == NULL)
		return NULL;
	for (pt = port->table_list; pt; pt = pt->next)
		if (pt->id == table_id)
			return pt->table;
	return NULL;
}

static struct rte_flow_action_handle *
parser_action_handle_get(uint16_t port_id, uint32_t action_id)
{
	return port_action_handle_get_by_id(port_id, action_id);
}

static struct rte_flow_meter_profile *
parser_meter_profile_get(uint16_t port_id, uint32_t profile_id)
{
	return port_meter_profile_get_by_id(port_id, profile_id);
}

static struct rte_flow_meter_policy *
parser_meter_policy_get(uint16_t port_id, uint32_t policy_id)
{
	return port_meter_policy_get_by_id(port_id, policy_id);
}

static struct rte_flow_item_flex_handle *
parser_flex_handle_get(uint16_t port_id, uint16_t flex_id)
{
	struct flex_item *fp;

	if (port_id >= RTE_MAX_ETHPORTS || flex_id >= FLEX_MAX_PARSERS_NUM)
		return NULL;
	fp = flex_items[port_id][flex_id];
	return fp ? fp->flex_handle : NULL;
}

static int
parser_flex_pattern_get(uint16_t pattern_id,
			const struct rte_flow_item_flex **spec,
			const struct rte_flow_item_flex **mask)
{
	if (pattern_id >= FLEX_MAX_PATTERNS_NUM || spec == NULL || mask == NULL)
		return -ENOENT;
	*spec = &flex_patterns[pattern_id].spec;
	*mask = &flex_patterns[pattern_id].mask;
	return 0;
}

static uint16_t
parser_verbose_level_get(void)
{
	return verbose_level;
}
static int
parser_flow_validate(uint16_t port_id, const struct rte_flow_attr *attr,
		     const struct rte_flow_item pattern[],
		     const struct rte_flow_action actions[],
		     const struct rte_flow_parser_tunnel_ops *tunnel_ops)
{
	struct tunnel_ops ops;

	return port_flow_validate(port_id, attr, pattern, actions,
				tunnel_ops ? parser_tunnel_convert(tunnel_ops, &ops)
					   : NULL);
}

static int
parser_flow_create(uint16_t port_id, const struct rte_flow_attr *attr,
		   const struct rte_flow_item pattern[],
		   const struct rte_flow_action actions[],
		   const struct rte_flow_parser_tunnel_ops *tunnel_ops,
		   uintptr_t user_id)
{
	struct tunnel_ops ops;

	return port_flow_create(port_id, attr, pattern, actions,
				tunnel_ops ? parser_tunnel_convert(tunnel_ops, &ops)
					   : NULL,
				user_id);
}

static void
parser_flow_aged(uint16_t port_id, int destroy)
{
	port_flow_aged(port_id, (uint8_t)destroy);
}

static void
parser_flow_tunnel_create(uint16_t port_id,
			  const struct rte_flow_parser_tunnel_ops *ops_cfg)
{
	struct tunnel_ops ops;

	port_flow_tunnel_create(port_id,
				ops_cfg ? parser_tunnel_convert(ops_cfg, &ops)
					: NULL);
}

static const struct rte_flow_parser_ops_query parser_query_ops = {
	.port_validate = parser_port_validate,
	.flow_rule_count = parser_flow_rule_count,
	.flow_rule_id_get = parser_flow_rule_id_get,
	.pattern_template_count = parser_pattern_template_count,
	.pattern_template_id_get = parser_pattern_template_id_get,
	.actions_template_count = parser_actions_template_count,
	.actions_template_id_get = parser_actions_template_id_get,
	.table_count = parser_table_count,
	.table_id_get = parser_table_id_get,
	.queue_count = parser_queue_count,
	.rss_queue_count = parser_rss_queue_count,
	.table_get = parser_table_get,
	.action_handle_get = parser_action_handle_get,
	.meter_profile_get = parser_meter_profile_get,
	.meter_policy_get = parser_meter_policy_get,
	.verbose_level_get = parser_verbose_level_get,
	.flex_handle_get = parser_flex_handle_get,
	.flex_pattern_get = parser_flex_pattern_get,
};

static const struct rte_flow_parser_ops_command parser_command_ops = {
	.flow_get_info = port_flow_get_info,
	.flow_configure = port_flow_configure,
	.flow_pattern_template_create = port_flow_pattern_template_create,
	.flow_pattern_template_destroy = port_flow_pattern_template_destroy,
	.flow_actions_template_create = port_flow_actions_template_create,
	.flow_actions_template_destroy = port_flow_actions_template_destroy,
	.flow_template_table_create = port_flow_template_table_create,
	.flow_template_table_destroy = port_flow_template_table_destroy,
	.flow_template_table_resize_complete =
		port_flow_template_table_resize_complete,
	.queue_group_set_miss_actions = port_queue_group_set_miss_actions,
	.flow_template_table_resize = port_flow_template_table_resize,
	.queue_flow_create = port_queue_flow_create,
	.queue_flow_destroy = port_queue_flow_destroy,
	.queue_flow_update_resized = port_queue_flow_update_resized,
	.queue_flow_update = port_queue_flow_update,
	.queue_flow_push = port_queue_flow_push,
	.queue_flow_pull = port_queue_flow_pull,
	.flow_hash_calc = port_flow_hash_calc,
	.flow_hash_calc_encap = port_flow_hash_calc_encap,
	.queue_flow_aged = port_queue_flow_aged,
	.queue_action_handle_create = port_queue_action_handle_create,
	.queue_action_handle_destroy = port_queue_action_handle_destroy,
	.queue_action_handle_update = port_queue_action_handle_update,
	.queue_action_handle_query = port_queue_action_handle_query,
	.queue_action_handle_query_update = port_queue_action_handle_query_update,
	.action_handle_create = port_action_handle_create,
	.action_handle_destroy = port_action_handle_destroy,
	.action_handle_update = port_action_handle_update,
	.action_handle_query = port_action_handle_query,
	.action_handle_query_update = port_action_handle_query_update,
	.flow_validate = parser_flow_validate,
	.flow_create = parser_flow_create,
	.flow_destroy = port_flow_destroy,
	.flow_update = port_flow_update,
	.flow_flush = port_flow_flush,
	.flow_dump = port_flow_dump,
	.flow_query = port_flow_query,
	.flow_list = port_flow_list,
	.flow_isolate = port_flow_isolate,
	.flow_aged = parser_flow_aged,
	.flow_tunnel_create = parser_flow_tunnel_create,
	.flow_tunnel_destroy = port_flow_tunnel_destroy,
	.flow_tunnel_list = port_flow_tunnel_list,
	.meter_policy_add = port_meter_policy_add,
	.flex_item_create = flex_item_create,
	.flex_item_destroy = flex_item_destroy,
};

static const struct rte_flow_parser_ops parser_ops = {
	.query = &parser_query_ops,
	.command = &parser_command_ops,
};

int
testpmd_flow_parser_init(void)
{
	return rte_flow_parser_init(&parser_ops);
}
