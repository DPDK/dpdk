/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <rte_flow_driver.h>
#include "nt_util.h"
#include "create_elements.h"
#include "ntnic_mod_reg.h"
#include "ntos_system.h"

#define MAX_RTE_FLOWS 8192

#define NT_MAX_COLOR_FLOW_STATS 0x400

rte_spinlock_t flow_lock = RTE_SPINLOCK_INITIALIZER;
static struct rte_flow nt_flows[MAX_RTE_FLOWS];

int convert_error(struct rte_flow_error *error, struct rte_flow_error *rte_flow_error)
{
	if (error) {
		error->cause = NULL;
		error->message = rte_flow_error->message;

		if (rte_flow_error->type == RTE_FLOW_ERROR_TYPE_NONE ||
			rte_flow_error->type == RTE_FLOW_ERROR_TYPE_NONE)
			error->type = RTE_FLOW_ERROR_TYPE_NONE;

		else
			error->type = RTE_FLOW_ERROR_TYPE_UNSPECIFIED;
	}

	return 0;
}

int create_attr(struct cnv_attr_s *attribute, const struct rte_flow_attr *attr)
{
	memset(&attribute->attr, 0x0, sizeof(struct rte_flow_attr));

	if (attr) {
		attribute->attr.group = attr->group;
		attribute->attr.priority = attr->priority;
	}

	return 0;
}

int create_match_elements(struct cnv_match_s *match, const struct rte_flow_item items[],
	int max_elem)
{
	int eidx = 0;
	int iter_idx = 0;
	int type = -1;

	if (!items) {
		NT_LOG(ERR, FILTER, "ERROR no items to iterate!");
		return -1;
	}

	do {
		type = items[iter_idx].type;

		if (type < 0) {
			if ((int)items[iter_idx].type == NT_RTE_FLOW_ITEM_TYPE_TUNNEL) {
				type = NT_RTE_FLOW_ITEM_TYPE_TUNNEL;

			} else {
				NT_LOG(ERR, FILTER, "ERROR unknown item type received!");
				return -1;
			}
		}

		if (type >= 0) {
			if (items[iter_idx].last) {
				/* Ranges are not supported yet */
				NT_LOG(ERR, FILTER, "ERROR ITEM-RANGE SETUP - NOT SUPPORTED!");
				return -1;
			}

			if (eidx == max_elem) {
				NT_LOG(ERR, FILTER, "ERROR TOO MANY ELEMENTS ENCOUNTERED!");
				return -1;
			}

			match->rte_flow_item[eidx].type = type;
			match->rte_flow_item[eidx].spec = items[iter_idx].spec;
			match->rte_flow_item[eidx].mask = items[iter_idx].mask;

			eidx++;
			iter_idx++;
		}

	} while (type >= 0 && type != RTE_FLOW_ITEM_TYPE_END);

	return (type >= 0) ? 0 : -1;
}

int create_action_elements_inline(struct cnv_action_s *action __rte_unused,
	const struct rte_flow_action actions[] __rte_unused,
	int max_elem __rte_unused,
	uint32_t queue_offset __rte_unused)
{
	int type = -1;

	return (type >= 0) ? 0 : -1;
}

static inline uint16_t get_caller_id(uint16_t port)
{
	return MAX_VDPA_PORTS + port + 1;
}

static int is_flow_handle_typecast(struct rte_flow *flow)
{
	const void *first_element = &nt_flows[0];
	const void *last_element = &nt_flows[MAX_RTE_FLOWS - 1];
	return (void *)flow < first_element || (void *)flow > last_element;
}

static int convert_flow(struct rte_eth_dev *eth_dev,
	const struct rte_flow_attr *attr,
	const struct rte_flow_item items[],
	const struct rte_flow_action actions[],
	struct cnv_attr_s *attribute,
	struct cnv_match_s *match,
	struct cnv_action_s *action,
	struct rte_flow_error *error)
{
	struct pmd_internals *internals = (struct pmd_internals *)eth_dev->data->dev_private;
	struct fpga_info_s *fpga_info = &internals->p_drv->ntdrv.adapter_info.fpga_info;

	static struct rte_flow_error flow_error = {
		.type = RTE_FLOW_ERROR_TYPE_NONE, .message = "none" };
	uint32_t queue_offset = 0;

	/* Set initial error */
	convert_error(error, &flow_error);

	if (!internals) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"Missing eth_dev");
		return -1;
	}

	if (internals->type == PORT_TYPE_OVERRIDE && internals->vpq_nb_vq > 0) {
		/*
		 * The queues coming from the main PMD will always start from 0
		 * When the port is a the VF/vDPA port the queues must be changed
		 * to match the queues allocated for VF/vDPA.
		 */
		queue_offset = internals->vpq[0].id;
	}

	if (create_attr(attribute, attr) != 0) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ATTR, NULL, "Error in attr");
		return -1;
	}

	if (create_match_elements(match, items, MAX_ELEMENTS) != 0) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			"Error in items");
		return -1;
	}

	if (fpga_info->profile == FPGA_INFO_PROFILE_INLINE) {
		if (create_action_elements_inline(action, actions,
			MAX_ACTIONS, queue_offset) != 0) {
			rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				"Error in actions");
			return -1;
		}

	} else {
		rte_flow_error_set(error, EPERM, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"Unsupported adapter profile");
		return -1;
	}

	return 0;
}

static int
eth_flow_destroy(struct rte_eth_dev *eth_dev, struct rte_flow *flow, struct rte_flow_error *error)
{
	const struct flow_filter_ops *flow_filter_ops = get_flow_filter_ops();

	if (flow_filter_ops == NULL) {
		NT_LOG_DBGX(ERR, FILTER, "flow_filter module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = (struct pmd_internals *)eth_dev->data->dev_private;

	static struct rte_flow_error flow_error = {
		.type = RTE_FLOW_ERROR_TYPE_NONE, .message = "none" };
	int res = 0;
	/* Set initial error */
	convert_error(error, &flow_error);

	if (!flow)
		return 0;

	if (is_flow_handle_typecast(flow)) {
		res = flow_filter_ops->flow_destroy(internals->flw_dev, (void *)flow, &flow_error);
		convert_error(error, &flow_error);

	} else {
		res = flow_filter_ops->flow_destroy(internals->flw_dev, flow->flw_hdl,
			&flow_error);
		convert_error(error, &flow_error);

		rte_spinlock_lock(&flow_lock);
		flow->used = 0;
		rte_spinlock_unlock(&flow_lock);
	}

	return res;
}

static struct rte_flow *eth_flow_create(struct rte_eth_dev *eth_dev,
	const struct rte_flow_attr *attr,
	const struct rte_flow_item items[],
	const struct rte_flow_action actions[],
	struct rte_flow_error *error)
{
	const struct flow_filter_ops *flow_filter_ops = get_flow_filter_ops();

	if (flow_filter_ops == NULL) {
		NT_LOG_DBGX(ERR, FILTER, "flow_filter module uninitialized");
		return NULL;
	}

	struct pmd_internals *internals = (struct pmd_internals *)eth_dev->data->dev_private;

	struct fpga_info_s *fpga_info = &internals->p_drv->ntdrv.adapter_info.fpga_info;

	struct cnv_attr_s attribute = { 0 };
	struct cnv_match_s match = { 0 };
	struct cnv_action_s action = { 0 };

	static struct rte_flow_error flow_error = {
		.type = RTE_FLOW_ERROR_TYPE_NONE, .message = "none" };
	uint32_t flow_stat_id = 0;

	if (convert_flow(eth_dev, attr, items, actions, &attribute, &match, &action, error) < 0)
		return NULL;

	/* Main application caller_id is port_id shifted above VF ports */
	attribute.caller_id = get_caller_id(eth_dev->data->port_id);

	if (fpga_info->profile == FPGA_INFO_PROFILE_INLINE && attribute.attr.group > 0) {
		void *flw_hdl = flow_filter_ops->flow_create(internals->flw_dev, &attribute.attr,
			attribute.forced_vlan_vid, attribute.caller_id,
			match.rte_flow_item, action.flow_actions,
			&flow_error);
		convert_error(error, &flow_error);
		return (struct rte_flow *)flw_hdl;
	}

	struct rte_flow *flow = NULL;
	rte_spinlock_lock(&flow_lock);
	int i;

	for (i = 0; i < MAX_RTE_FLOWS; i++) {
		if (!nt_flows[i].used) {
			nt_flows[i].flow_stat_id = flow_stat_id;

			if (nt_flows[i].flow_stat_id < NT_MAX_COLOR_FLOW_STATS) {
				nt_flows[i].used = 1;
				flow = &nt_flows[i];
			}

			break;
		}
	}

	rte_spinlock_unlock(&flow_lock);

	if (flow) {
		flow->flw_hdl = flow_filter_ops->flow_create(internals->flw_dev, &attribute.attr,
			attribute.forced_vlan_vid, attribute.caller_id,
			match.rte_flow_item, action.flow_actions,
			&flow_error);
		convert_error(error, &flow_error);

		if (!flow->flw_hdl) {
			rte_spinlock_lock(&flow_lock);
			flow->used = 0;
			flow = NULL;
			rte_spinlock_unlock(&flow_lock);

		} else {
			rte_spinlock_lock(&flow_lock);
			flow->caller_id = attribute.caller_id;
			rte_spinlock_unlock(&flow_lock);
		}
	}

	return flow;
}

static const struct rte_flow_ops dev_flow_ops = {
	.create = eth_flow_create,
	.destroy = eth_flow_destroy,
};

void dev_flow_init(void)
{
	register_dev_flow_ops(&dev_flow_ops);
}
