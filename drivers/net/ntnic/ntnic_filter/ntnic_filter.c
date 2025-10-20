/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <rte_flow_driver.h>
#include <rte_pci.h>
#include <rte_version.h>
#include <rte_pmd_ntnic.h>

#include "ntlog.h"
#include "nt_util.h"
#include "create_elements.h"
#include "ntnic_mod_reg.h"
#include "ntos_system.h"
#include "ntos_drv.h"
#include "nt_service.h"
#include "rte_service.h"

#define MAX_RTE_FLOWS 8192

#define MAX_COLOR_FLOW_STATS 0x400
#define NT_MAX_COLOR_FLOW_STATS 0x400

#if (MAX_COLOR_FLOW_STATS != NT_MAX_COLOR_FLOW_STATS)
#error Difference in COLOR_FLOW_STATS. Please synchronize the defines.
#endif

static struct rte_flow nt_flows[MAX_RTE_FLOWS];

rte_spinlock_t flow_lock = RTE_SPINLOCK_INITIALIZER;
static struct rte_flow nt_flows[MAX_RTE_FLOWS];

static int nthw_interpret_raw_data(uint8_t *data, uint8_t *preserve, int size,
	struct rte_flow_item *out)
{
	int hdri = 0;
	int pkti = 0;

	/* Ethernet */
	if (size - pkti == 0)
		goto interpret_end;

	if (size - pkti < (int)sizeof(struct rte_ether_hdr))
		return -1;

	out[hdri].type = RTE_FLOW_ITEM_TYPE_ETH;
	out[hdri].spec = &data[pkti];
	out[hdri].mask = (preserve != NULL) ? &preserve[pkti] : NULL;

	rte_be16_t ether_type = ((struct rte_ether_hdr *)&data[pkti])->ether_type;

	hdri += 1;
	pkti += sizeof(struct rte_ether_hdr);

	if (size - pkti == 0)
		goto interpret_end;

	/* VLAN */
	while (ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN) ||
		ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_QINQ) ||
		ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_QINQ1)) {
		if (size - pkti == 0)
			goto interpret_end;

		if (size - pkti < (int)sizeof(struct rte_vlan_hdr))
			return -1;

		out[hdri].type = RTE_FLOW_ITEM_TYPE_VLAN;
		out[hdri].spec = &data[pkti];
		out[hdri].mask = (preserve != NULL) ? &preserve[pkti] : NULL;

		ether_type = ((struct rte_vlan_hdr *)&data[pkti])->eth_proto;

		hdri += 1;
		pkti += sizeof(struct rte_vlan_hdr);
	}

	if (size - pkti == 0)
		goto interpret_end;

	/* Layer 3 */
	uint8_t next_header = 0;

	if (ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4) && (data[pkti] & 0xF0) == 0x40) {
		if (size - pkti < (int)sizeof(struct rte_ipv4_hdr))
			return -1;

		out[hdri].type = RTE_FLOW_ITEM_TYPE_IPV4;
		out[hdri].spec = &data[pkti];
		out[hdri].mask = (preserve != NULL) ? &preserve[pkti] : NULL;

		next_header = data[pkti + 9];

		hdri += 1;
		pkti += sizeof(struct rte_ipv4_hdr);

	} else if (ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6) &&
			(data[pkti] & 0xF0) == 0x60) {
		if (size - pkti < (int)sizeof(struct rte_ipv6_hdr))
			return -1;

		out[hdri].type = RTE_FLOW_ITEM_TYPE_IPV6;
		out[hdri].spec = &data[pkti];
		out[hdri].mask = (preserve != NULL) ? &preserve[pkti] : NULL;

		next_header = data[pkti + 6];

		hdri += 1;
		pkti += sizeof(struct rte_ipv6_hdr);
	} else {
		return -1;
	}

	if (size - pkti == 0)
		goto interpret_end;

	/* Layer 4 */
	int gtpu_encap = 0;

	if (next_header == 1) {	/* ICMP */
		if (size - pkti < (int)sizeof(struct rte_icmp_hdr))
			return -1;

		out[hdri].type = RTE_FLOW_ITEM_TYPE_ICMP;
		out[hdri].spec = &data[pkti];
		out[hdri].mask = (preserve != NULL) ? &preserve[pkti] : NULL;

		hdri += 1;
		pkti += sizeof(struct rte_icmp_hdr);

	} else if (next_header == 58) {	/* ICMP6 */
		if (size - pkti < (int)sizeof(struct rte_flow_item_icmp6))
			return -1;

		out[hdri].type = RTE_FLOW_ITEM_TYPE_ICMP6;
		out[hdri].spec = &data[pkti];
		out[hdri].mask = (preserve != NULL) ? &preserve[pkti] : NULL;

		hdri += 1;
		pkti += sizeof(struct rte_icmp_hdr);

	} else if (next_header == 6) {	/* TCP */
		if (size - pkti < (int)sizeof(struct rte_tcp_hdr))
			return -1;

		out[hdri].type = RTE_FLOW_ITEM_TYPE_TCP;
		out[hdri].spec = &data[pkti];
		out[hdri].mask = (preserve != NULL) ? &preserve[pkti] : NULL;

		hdri += 1;
		pkti += sizeof(struct rte_tcp_hdr);

	} else if (next_header == 17) {	/* UDP */
		if (size - pkti < (int)sizeof(struct rte_udp_hdr))
			return -1;

		out[hdri].type = RTE_FLOW_ITEM_TYPE_UDP;
		out[hdri].spec = &data[pkti];
		out[hdri].mask = (preserve != NULL) ? &preserve[pkti] : NULL;

		gtpu_encap = ((struct rte_udp_hdr *)&data[pkti])->dst_port ==
			rte_cpu_to_be_16(RTE_GTPU_UDP_PORT);

		hdri += 1;
		pkti += sizeof(struct rte_udp_hdr);

	} else if (next_header == 132) {/* SCTP */
		if (size - pkti < (int)sizeof(struct rte_sctp_hdr))
			return -1;

		out[hdri].type = RTE_FLOW_ITEM_TYPE_SCTP;
		out[hdri].spec = &data[pkti];
		out[hdri].mask = (preserve != NULL) ? &preserve[pkti] : NULL;

		hdri += 1;
		pkti += sizeof(struct rte_sctp_hdr);

	} else {
		return -1;
	}

	if (size - pkti == 0)
		goto interpret_end;

	/* GTPv1-U */
	if (gtpu_encap) {
		if (size - pkti < (int)sizeof(struct rte_gtp_hdr))
			return -1;

		out[hdri]
		.type = RTE_FLOW_ITEM_TYPE_GTP;
		out[hdri]
		.spec = &data[pkti];
		out[hdri]
		.mask = (preserve != NULL) ? &preserve[pkti] : NULL;

		int extension_present_bit = ((struct rte_gtp_hdr *)&data[pkti])
			->e;

		hdri += 1;
		pkti += sizeof(struct rte_gtp_hdr);

		if (extension_present_bit) {
			if (size - pkti < (int)sizeof(struct rte_gtp_hdr_ext_word))
				return -1;

			out[hdri]
			.type = RTE_FLOW_ITEM_TYPE_GTP;
			out[hdri]
			.spec = &data[pkti];
			out[hdri]
			.mask = (preserve != NULL) ? &preserve[pkti] : NULL;

			uint8_t next_ext = ((struct rte_gtp_hdr_ext_word *)&data[pkti])
				->next_ext;

			hdri += 1;
			pkti += sizeof(struct rte_gtp_hdr_ext_word);

			while (next_ext) {
				size_t ext_len = data[pkti] * 4;

				if (size - pkti < (int)ext_len)
					return -1;

				out[hdri]
				.type = RTE_FLOW_ITEM_TYPE_GTP;
				out[hdri]
				.spec = &data[pkti];
				out[hdri]
				.mask = (preserve != NULL) ? &preserve[pkti] : NULL;

				next_ext = data[pkti + ext_len - 1];

				hdri += 1;
				pkti += ext_len;
			}
		}
	}

	if (size - pkti != 0)
		return -1;

interpret_end:
	out[hdri].type = RTE_FLOW_ITEM_TYPE_END;
	out[hdri].spec = NULL;
	out[hdri].mask = NULL;

	return hdri + 1;
}

static int nthw_create_attr(struct cnv_attr_s *attribute, const struct rte_flow_attr *attr)
{
	if (!attribute) {
		NT_LOG(ERR, FILTER, "ERROR no attribute to iterate!");
		return -1;
	}

	memset(&attribute->attr, 0x0, sizeof(struct rte_flow_attr));

	if (attr) {
		attribute->attr.group = attr->group;
		attribute->attr.priority = attr->priority;
	}

	return 0;
}

static int nthw_create_match_elements(struct cnv_match_s *match, const struct rte_flow_item items[],
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

static int nthw_create_action_elements_inline(struct cnv_action_s *action,
	const struct rte_flow_action actions[],
	int max_elem,
	uint32_t queue_offset)
{
	int aidx = 0;
	int type = -1;

	do {
		type = actions[aidx].type;
		if (type >= 0) {
			action->flow_actions[aidx].type = type;

			/*
			 * Non-compatible actions handled here
			 */
			switch (type) {
			case RTE_FLOW_ACTION_TYPE_RSS: {
				const struct rte_flow_action_rss *rss =
					(const struct rte_flow_action_rss *)actions[aidx].conf;

				switch (rss->func) {
				case RTE_ETH_HASH_FUNCTION_DEFAULT:
					action->flow_rss.func =
						(enum rte_eth_hash_function)
						RTE_ETH_HASH_FUNCTION_DEFAULT;
					break;

				case RTE_ETH_HASH_FUNCTION_TOEPLITZ:
					action->flow_rss.func =
						(enum rte_eth_hash_function)
						RTE_ETH_HASH_FUNCTION_TOEPLITZ;

					if (rte_is_power_of_2(rss->queue_num) == 0) {
						NT_LOG(ERR, FILTER,
							"RTE ACTION RSS - for Toeplitz the number of queues must be power of two");
						return -1;
					}

					break;

				case RTE_ETH_HASH_FUNCTION_SIMPLE_XOR:
				case RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ:
				case RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ_SORT:
				case RTE_ETH_HASH_FUNCTION_MAX:
				default:
					NT_LOG(ERR, FILTER,
						"RTE ACTION RSS - unsupported function: %i",
						(int)rss->func);
					return -1;
				}

				uint64_t tmp_rss_types = 0;

				switch (rss->level) {
				case 1:
					/* clear/override level mask specified at types */
					tmp_rss_types = rss->types & (~RTE_ETH_RSS_LEVEL_MASK);
					action->flow_rss.types =
						tmp_rss_types | RTE_ETH_RSS_LEVEL_OUTERMOST;
					break;

				case 2:
					/* clear/override level mask specified at types */
					tmp_rss_types = rss->types & (~RTE_ETH_RSS_LEVEL_MASK);
					action->flow_rss.types =
						tmp_rss_types | RTE_ETH_RSS_LEVEL_INNERMOST;
					break;

				case 0:
					/* keep level mask specified at types */
					action->flow_rss.types = rss->types;
					break;

				default:
					NT_LOG(ERR, FILTER,
						"RTE ACTION RSS - unsupported level: %u",
						rss->level);
					return -1;
				}

				action->flow_rss.level = 0;
				action->flow_rss.key_len = rss->key_len;
				action->flow_rss.queue_num = rss->queue_num;
				action->flow_rss.key = rss->key;
				action->flow_rss.queue = rss->queue;
				action->flow_actions[aidx].conf = &action->flow_rss;
			}
			break;

			case RTE_FLOW_ACTION_TYPE_RAW_DECAP: {
				const struct rte_flow_action_raw_decap *decap =
					(const struct rte_flow_action_raw_decap *)actions[aidx]
					.conf;
				int item_count = nthw_interpret_raw_data(decap->data, NULL,
					decap->size, action->decap.items);

				if (item_count < 0)
					return item_count;
				action->decap.data = decap->data;
				action->decap.size = decap->size;
				action->decap.item_count = item_count;
				action->flow_actions[aidx].conf = &action->decap;
			}
			break;

			case RTE_FLOW_ACTION_TYPE_RAW_ENCAP: {
				const struct rte_flow_action_raw_encap *encap =
					(const struct rte_flow_action_raw_encap *)actions[aidx]
					.conf;
				int item_count = nthw_interpret_raw_data(encap->data,
					encap->preserve, encap->size, action->encap.items);

				if (item_count < 0)
					return item_count;
				action->encap.data = encap->data;
				action->encap.preserve = encap->preserve;
				action->encap.size = encap->size;
				action->encap.item_count = item_count;
				action->flow_actions[aidx].conf = &action->encap;
			}
			break;

			case RTE_FLOW_ACTION_TYPE_QUEUE: {
				const struct rte_flow_action_queue *queue =
					(const struct rte_flow_action_queue *)actions[aidx].conf;
				action->queue.index = queue->index + queue_offset;
				action->flow_actions[aidx].conf = &action->queue;
			}
			break;

			default: {
				action->flow_actions[aidx].conf = actions[aidx].conf;
			}
			break;
			}

			aidx++;

			if (aidx == max_elem)
				return -1;
		}

	} while (type >= 0 && type != RTE_FLOW_ITEM_TYPE_END);

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
	struct pmd_internals *internals = eth_dev->data->dev_private;

	if (!internals) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"Missing eth_dev");
		return -1;
	}

	struct fpga_info_s *fpga_info = &internals->p_drv->ntdrv.adapter_info.fpga_info;

	error->type = RTE_FLOW_ERROR_TYPE_NONE;
	error->message = "none";
	uint32_t queue_offset = 0;

	if (internals->type == PORT_TYPE_OVERRIDE && internals->vpq_nb_vq > 0) {
		/*
		 * The queues coming from the main PMD will always start from 0
		 * When the port is a the VF/vDPA port the queues must be changed
		 * to match the queues allocated for VF/vDPA.
		 */
		queue_offset = internals->vpq[0].id;
	}

	if (nthw_create_attr(attribute, attr) != 0) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ATTR, NULL, "Error in attr");
		return -1;
	}

	if (nthw_create_match_elements(match, items, MAX_ELEMENTS) != 0) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			"Error in items");
		return -1;
	}

	if (fpga_info->profile == FPGA_INFO_PROFILE_INLINE) {
		if (nthw_create_action_elements_inline(action, actions,
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
	const struct flow_filter_ops *flow_filter_ops = nthw_get_flow_filter_ops();

	if (flow_filter_ops == NULL) {
		NT_LOG_DBGX(ERR, FILTER, "flow_filter module uninitialized");
		return -1;
	}

	struct nt_service *srv = nthw_service_get_info(RTE_NTNIC_SERVICE_FLM_UPDATE);

	if (!srv || !NT_SERVICE_GET_STATE(srv) || !rte_service_runstate_get(srv->id))	{
		NT_LOG(ERR, FILTER, "flm update service is not started. Flow cannot be destroyed");
		return -1;
	}

	struct pmd_internals *internals = eth_dev->data->dev_private;

	error->type = RTE_FLOW_ERROR_TYPE_NONE;
	error->message = "none";
	int res = 0;

	if (!flow)
		return 0;

	if (is_flow_handle_typecast(flow)) {
		res = flow_filter_ops->flow_destroy(internals->flw_dev, (void *)flow, error);

	} else {
		res = flow_filter_ops->flow_destroy(internals->flw_dev, flow->flw_hdl,
			error);

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
	const struct flow_filter_ops *flow_filter_ops = nthw_get_flow_filter_ops();

	if (flow_filter_ops == NULL) {
		NT_LOG_DBGX(ERR, FILTER, "flow_filter module uninitialized");
		return NULL;
	}

	struct nt_service *srv = nthw_service_get_info(RTE_NTNIC_SERVICE_FLM_UPDATE);

	if (!srv || !NT_SERVICE_GET_STATE(srv) || !rte_service_runstate_get(srv->id))	{
		NT_LOG(ERR, FILTER, "flm update service is not started. Flow cannot be created");
		return NULL;
	}

	struct pmd_internals *internals = eth_dev->data->dev_private;

	struct fpga_info_s *fpga_info = &internals->p_drv->ntdrv.adapter_info.fpga_info;

	struct cnv_attr_s attribute = { 0 };
	struct cnv_match_s match = { 0 };
	struct cnv_action_s action = { 0 };

	error->type = RTE_FLOW_ERROR_TYPE_NONE;
	error->message = "none";
	uint32_t flow_stat_id = 0;

	if (convert_flow(eth_dev, attr, items, actions, &attribute, &match, &action, error) < 0)
		return NULL;

	/* Main application caller_id is port_id shifted above VF ports */
	attribute.caller_id = get_caller_id(eth_dev->data->port_id);

	if (fpga_info->profile == FPGA_INFO_PROFILE_INLINE && attribute.attr.group > 0) {
		void *flw_hdl = flow_filter_ops->flow_create(internals->flw_dev, &attribute.attr,
			attribute.forced_vlan_vid, attribute.caller_id,
			match.rte_flow_item, action.flow_actions,
			error);
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
			error);

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

static int eth_flow_flush(struct rte_eth_dev *eth_dev, struct rte_flow_error *error)
{
	const struct flow_filter_ops *flow_filter_ops = nthw_get_flow_filter_ops();

	if (flow_filter_ops == NULL) {
		NT_LOG_DBGX(ERR, FILTER, "flow_filter module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = eth_dev->data->dev_private;

	error->type = RTE_FLOW_ERROR_TYPE_NONE;
	error->message = "none";
	int res = 0;
	/* Main application caller_id is port_id shifted above VDPA ports */
	uint16_t caller_id = get_caller_id(eth_dev->data->port_id);

	if (internals->flw_dev) {
		res = flow_filter_ops->flow_flush(internals->flw_dev, caller_id, error);
		rte_spinlock_lock(&flow_lock);

		for (int flow = 0; flow < MAX_RTE_FLOWS; flow++) {
			if (nt_flows[flow].used && nt_flows[flow].caller_id == caller_id) {
				/* Cleanup recorded flows */
				nt_flows[flow].used = 0;
				nt_flows[flow].caller_id = 0;
				nt_flows[flow].stat_bytes = 0UL;
				nt_flows[flow].stat_pkts = 0UL;
				nt_flows[flow].stat_tcp_flags = 0;
			}
		}

		rte_spinlock_unlock(&flow_lock);
	}

	return res;
}

static int eth_flow_actions_update(struct rte_eth_dev *eth_dev,
	struct rte_flow *flow,
	const struct rte_flow_action actions[],
	struct rte_flow_error *error)
{
	const struct flow_filter_ops *flow_filter_ops = nthw_get_flow_filter_ops();

	if (flow_filter_ops == NULL) {
		NT_LOG(ERR, FILTER, "flow_filter module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = eth_dev->data->dev_private;

	error->type = RTE_FLOW_ERROR_TYPE_NONE;
	error->message = "none";
	int res = -1;

	if (internals->flw_dev) {
		struct pmd_internals *dev_private =
			eth_dev->data->dev_private;
		struct fpga_info_s *fpga_info = &dev_private->p_drv->ntdrv.adapter_info.fpga_info;
		struct cnv_action_s action = { 0 };

		if (fpga_info->profile == FPGA_INFO_PROFILE_INLINE) {
			uint32_t queue_offset = 0;

			if (dev_private->type == PORT_TYPE_OVERRIDE &&
				dev_private->vpq_nb_vq > 0) {
				/*
				 * The queues coming from the main PMD will always start from 0
				 * When the port is a the VF/vDPA port the queues must be changed
				 * to match the queues allocated for VF/vDPA.
				 */
				queue_offset = dev_private->vpq[0].id;
			}

			if (nthw_create_action_elements_inline(&action, actions, MAX_ACTIONS,
					queue_offset) != 0) {
				rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					"Error in actions");
				return -1;
			}
		}

		if (is_flow_handle_typecast(flow)) {
			res = flow_filter_ops->flow_actions_update(internals->flw_dev,
					(void *)flow,
					action.flow_actions,
					error);

		} else {
			res = flow_filter_ops->flow_actions_update(internals->flw_dev,
					flow->flw_hdl,
					action.flow_actions,
					error);
		}
	}

	return res;
}

static int eth_flow_dev_dump(struct rte_eth_dev *eth_dev,
	struct rte_flow *flow,
	FILE *file,
	struct rte_flow_error *error)
{
	const struct flow_filter_ops *flow_filter_ops = nthw_get_flow_filter_ops();

	if (flow_filter_ops == NULL) {
		NT_LOG(ERR, NTNIC, "%s: flow_filter module uninitialized", __func__);
		return -1;
	}

	struct pmd_internals *internals = eth_dev->data->dev_private;

	error->type = RTE_FLOW_ERROR_TYPE_NONE;
	error->message = "none";

	uint16_t caller_id = get_caller_id(eth_dev->data->port_id);

	int res = flow_filter_ops->flow_dev_dump(internals->flw_dev,
			is_flow_handle_typecast(flow) ? (void *)flow
			: flow->flw_hdl,
			caller_id, file, error);

	return res;
}

static int eth_flow_get_aged_flows(struct rte_eth_dev *eth_dev,
	void **context,
	uint32_t nb_contexts,
	struct rte_flow_error *error)
{
	const struct flow_filter_ops *flow_filter_ops = nthw_get_flow_filter_ops();

	if (flow_filter_ops == NULL) {
		NT_LOG_DBGX(ERR, NTNIC, "flow_filter module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = eth_dev->data->dev_private;

	error->type = RTE_FLOW_ERROR_TYPE_NONE;
	error->message = "none";

	uint16_t caller_id = get_caller_id(eth_dev->data->port_id);

	int res = flow_filter_ops->flow_get_aged_flows(internals->flw_dev, caller_id, context,
			nb_contexts, error);

	return res;
}

/*
 * NT Flow asynchronous operations API
 */

static int eth_flow_info_get(struct rte_eth_dev *dev, struct rte_flow_port_info *port_info,
	struct rte_flow_queue_info *queue_info, struct rte_flow_error *error)
{
	const struct flow_filter_ops *flow_filter_ops = nthw_get_flow_filter_ops();

	if (flow_filter_ops == NULL) {
		NT_LOG_DBGX(ERR, FILTER, "flow_filter module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = dev->data->dev_private;

	error->type = RTE_FLOW_ERROR_TYPE_NONE;
	error->message = "none";

	int res = flow_filter_ops->flow_info_get(internals->flw_dev,
			get_caller_id(dev->data->port_id),
			(struct rte_flow_port_info *)port_info,
			(struct rte_flow_queue_info *)queue_info,
			error);

	return res;
}

static int eth_flow_configure(struct rte_eth_dev *dev, const struct rte_flow_port_attr *port_attr,
	uint16_t nb_queue, const struct rte_flow_queue_attr *queue_attr[],
	struct rte_flow_error *error)
{
	const struct flow_filter_ops *flow_filter_ops = nthw_get_flow_filter_ops();

	if (flow_filter_ops == NULL) {
		NT_LOG_DBGX(ERR, FILTER, "flow_filter module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = dev->data->dev_private;

	error->type = RTE_FLOW_ERROR_TYPE_NONE;
	error->message = "none";

	int res = flow_filter_ops->flow_configure(internals->flw_dev,
			get_caller_id(dev->data->port_id),
			(const struct rte_flow_port_attr *)port_attr,
			nb_queue,
			(const struct rte_flow_queue_attr **)queue_attr,
			error);

	return res;
}

static struct rte_flow_pattern_template *eth_flow_pattern_template_create(struct rte_eth_dev *dev,
	const struct rte_flow_pattern_template_attr *template_attr,
	const struct rte_flow_item pattern[], struct rte_flow_error *error)
{
	const struct flow_filter_ops *flow_filter_ops = nthw_get_flow_filter_ops();

	if (flow_filter_ops == NULL) {
		NT_LOG_DBGX(ERR, FILTER, "flow_filter module uninitialized");
		return NULL;
	}

	struct pmd_internals *internals = dev->data->dev_private;

	error->type = RTE_FLOW_ERROR_TYPE_NONE;
	error->message = "none";

	struct cnv_match_s match = { 0 };
	struct rte_flow_pattern_template_attr attr = {
		.relaxed_matching = template_attr->relaxed_matching,
		.ingress = template_attr->ingress,
		.egress = template_attr->egress,
		.transfer = template_attr->transfer,
	};

	uint16_t caller_id = get_caller_id(dev->data->port_id);

	if (nthw_create_match_elements(&match, pattern, MAX_ELEMENTS) != 0) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			"Error in pattern");
		return NULL;
	}

	struct flow_pattern_template *res =
		flow_filter_ops->flow_pattern_template_create(internals->flw_dev, &attr, caller_id,
			match.rte_flow_item, error);

	return (struct rte_flow_pattern_template *)res;
}

static int eth_flow_pattern_template_destroy(struct rte_eth_dev *dev,
	struct rte_flow_pattern_template *pattern_template,
	struct rte_flow_error *error)
{
	const struct flow_filter_ops *flow_filter_ops = nthw_get_flow_filter_ops();

	if (flow_filter_ops == NULL) {
		NT_LOG_DBGX(ERR, FILTER, "flow_filter module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = dev->data->dev_private;

	error->type = RTE_FLOW_ERROR_TYPE_NONE;
	error->message = "none";

	int res = flow_filter_ops->flow_pattern_template_destroy(internals->flw_dev,
			(struct flow_pattern_template *)
			pattern_template,
			error);

	return res;
}

static struct rte_flow_actions_template *eth_flow_actions_template_create(struct rte_eth_dev *dev,
	const struct rte_flow_actions_template_attr *template_attr,
	const struct rte_flow_action actions[], const struct rte_flow_action masks[],
	struct rte_flow_error *error)
{
	const struct flow_filter_ops *flow_filter_ops = nthw_get_flow_filter_ops();

	if (flow_filter_ops == NULL) {
		NT_LOG_DBGX(ERR, FILTER, "flow_filter module uninitialized");
		return NULL;
	}

	struct pmd_internals *internals = dev->data->dev_private;

	struct fpga_info_s *fpga_info = &internals->p_drv->ntdrv.adapter_info.fpga_info;
	error->type = RTE_FLOW_ERROR_TYPE_NONE;
	error->message = "none";

	struct cnv_action_s action = { 0 };
	struct cnv_action_s mask = { 0 };
	struct rte_flow_actions_template_attr attr = {
		.ingress = template_attr->ingress,
		.egress = template_attr->egress,
		.transfer = template_attr->transfer,
	};
	uint16_t caller_id = get_caller_id(dev->data->port_id);

	if (fpga_info->profile == FPGA_INFO_PROFILE_INLINE) {
		uint32_t queue_offset = 0;

		if (internals->type == PORT_TYPE_OVERRIDE && internals->vpq_nb_vq > 0)
			queue_offset = internals->vpq[0].id;

		if (nthw_create_action_elements_inline(&action, actions,
				MAX_ACTIONS,
				queue_offset) != 0) {
			rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				"Error in actions");
			return NULL;
		}

		if (nthw_create_action_elements_inline(&mask, masks,
				MAX_ACTIONS, queue_offset) != 0) {
			rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				"Error in masks");
			return NULL;
		}

	} else {
		rte_flow_error_set(error, EPERM, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"Unsupported adapter profile");
		return NULL;
	}

	struct flow_actions_template *res =
		flow_filter_ops->flow_actions_template_create(internals->flw_dev, &attr, caller_id,
			action.flow_actions,
			mask.flow_actions, error);

	return (struct rte_flow_actions_template *)res;
}

static int eth_flow_actions_template_destroy(struct rte_eth_dev *dev,
	struct rte_flow_actions_template *actions_template,
	struct rte_flow_error *error)
{
	const struct flow_filter_ops *flow_filter_ops = nthw_get_flow_filter_ops();

	if (flow_filter_ops == NULL) {
		NT_LOG_DBGX(ERR, NTNIC, "flow_filter module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = dev->data->dev_private;

	error->type = RTE_FLOW_ERROR_TYPE_NONE;
	error->message = "none";

	int res = flow_filter_ops->flow_actions_template_destroy(internals->flw_dev,
			(struct flow_actions_template *)
			actions_template,
			error);

	return res;
}

static struct rte_flow_template_table *eth_flow_template_table_create(struct rte_eth_dev *dev,
	const struct rte_flow_template_table_attr *table_attr,
	struct rte_flow_pattern_template *pattern_templates[], uint8_t nb_pattern_templates,
	struct rte_flow_actions_template *actions_templates[], uint8_t nb_actions_templates,
	struct rte_flow_error *error)
{
	const struct flow_filter_ops *flow_filter_ops = nthw_get_flow_filter_ops();

	if (flow_filter_ops == NULL) {
		NT_LOG_DBGX(ERR, FILTER, "flow_filter module uninitialized");
		return NULL;
	}

	struct pmd_internals *internals = dev->data->dev_private;

	error->type = RTE_FLOW_ERROR_TYPE_NONE;
	error->message = "none";

	struct rte_flow_template_table_attr attr = {
		.flow_attr = {
			.group = table_attr->flow_attr.group,
			.priority = table_attr->flow_attr.priority,
			.ingress = table_attr->flow_attr.ingress,
			.egress = table_attr->flow_attr.egress,
			.transfer = table_attr->flow_attr.transfer,
		},
		.nb_flows = table_attr->nb_flows,
	};
	uint16_t forced_vlan_vid = 0;
	uint16_t caller_id = get_caller_id(dev->data->port_id);

	struct flow_template_table *res =
		flow_filter_ops->flow_template_table_create(internals->flw_dev, &attr,
			forced_vlan_vid, caller_id,
			(struct flow_pattern_template **)pattern_templates,
			nb_pattern_templates, (struct flow_actions_template **)actions_templates,
			nb_actions_templates, error);

	return (struct rte_flow_template_table *)res;
}

static int eth_flow_template_table_destroy(struct rte_eth_dev *dev,
	struct rte_flow_template_table *template_table,
	struct rte_flow_error *error)
{
	const struct flow_filter_ops *flow_filter_ops = nthw_get_flow_filter_ops();

	if (flow_filter_ops == NULL) {
		NT_LOG_DBGX(ERR, FILTER, "flow_filter module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = dev->data->dev_private;

	error->type = RTE_FLOW_ERROR_TYPE_NONE;
	error->message = "none";

	int res = flow_filter_ops->flow_template_table_destroy(internals->flw_dev,
			(struct flow_template_table *)
			template_table,
			error);

	return res;
}

static struct rte_flow *eth_flow_async_create(struct rte_eth_dev *dev, uint32_t queue_id,
	const struct rte_flow_op_attr *op_attr,
	struct rte_flow_template_table *template_table, const struct rte_flow_item pattern[],
	uint8_t pattern_template_index, const struct rte_flow_action actions[],
	uint8_t actions_template_index, void *user_data, struct rte_flow_error *error)
{
	const struct flow_filter_ops *flow_filter_ops = nthw_get_flow_filter_ops();

	if (flow_filter_ops == NULL) {
		NT_LOG_DBGX(ERR, FILTER, "flow_filter module uninitialized");
		return NULL;
	}

	struct pmd_internals *internals = dev->data->dev_private;

	struct fpga_info_s *fpga_info = &internals->p_drv->ntdrv.adapter_info.fpga_info;
	error->type = RTE_FLOW_ERROR_TYPE_NONE;
	error->message = "none";

	struct cnv_action_s action = { 0 };
	struct cnv_match_s match = { 0 };

	if (nthw_create_match_elements(&match, pattern, MAX_ELEMENTS) != 0) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			"Error in pattern");
		return NULL;
	}

	if (fpga_info->profile == FPGA_INFO_PROFILE_INLINE) {
		uint32_t queue_offset = 0;

		if (internals->type == PORT_TYPE_OVERRIDE && internals->vpq_nb_vq > 0)
			queue_offset = internals->vpq[0].id;

		if (nthw_create_action_elements_inline(&action, actions,
				MAX_ACTIONS, queue_offset) != 0) {
			rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				"Error in actions");
			return NULL;
		}

	} else {
		rte_flow_error_set(error, EPERM, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"Unsupported adapter profile");
		return NULL;
	}

	struct flow_handle *res =
		flow_filter_ops->flow_async_create(internals->flw_dev,
			queue_id,
			(const struct rte_flow_op_attr *)op_attr,
			(struct flow_template_table *)template_table,
			match.rte_flow_item,
			pattern_template_index,
			action.flow_actions,
			actions_template_index,
			user_data,
			error);

	return (struct rte_flow *)res;
}

static int eth_flow_async_destroy(struct rte_eth_dev *dev, uint32_t queue_id,
	const struct rte_flow_op_attr *op_attr, struct rte_flow *flow,
	void *user_data, struct rte_flow_error *error)
{
	const struct flow_filter_ops *flow_filter_ops = nthw_get_flow_filter_ops();

	if (flow_filter_ops == NULL) {
		NT_LOG_DBGX(ERR, FILTER, "flow_filter module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = dev->data->dev_private;

	error->type = RTE_FLOW_ERROR_TYPE_NONE;
	error->message = "none";

	int res = flow_filter_ops->flow_async_destroy(internals->flw_dev,
			queue_id,
			(const struct rte_flow_op_attr *)op_attr,
			(struct flow_handle *)flow,
			user_data,
			error);

	return res;
}

static int poll_statistics(struct pmd_internals *internals)
{
	int flow;
	struct drv_s *p_drv = internals->p_drv;
	struct ntdrv_4ga_s *p_nt_drv = &p_drv->ntdrv;
	nt4ga_stat_t *p_nt4ga_stat = &p_nt_drv->adapter_info.nt4ga_stat;
	const int n_intf_no = internals->n_intf_no;
	uint64_t last_stat_rtc = 0;

	if (!p_nt4ga_stat || n_intf_no < 0 || n_intf_no >= NUM_ADAPTER_PORTS_MAX)
		return -1;

	RTE_ASSERT(rte_tsc_freq > 0);

	rte_spinlock_lock(&hwlock);

	uint64_t now_rtc = rte_get_tsc_cycles();

	/*
	 * Check per port max once a second
	 * if more than a second since last stat read, do a new one
	 */
	if ((now_rtc - internals->last_stat_rtc) < rte_tsc_freq) {
		rte_spinlock_unlock(&hwlock);
		return 0;
	}

	internals->last_stat_rtc = now_rtc;

	rte_spinlock_lock(&p_nt_drv->stat_lck);

	/*
	 * Add the RX statistics increments since last time we polled.
	 * (No difference if physical or virtual port)
	 */
	internals->rxq_scg[0].rx_pkts += p_nt4ga_stat->a_port_rx_packets_total[n_intf_no] -
		p_nt4ga_stat->a_port_rx_packets_base[n_intf_no];
	internals->rxq_scg[0].rx_bytes += p_nt4ga_stat->a_port_rx_octets_total[n_intf_no] -
		p_nt4ga_stat->a_port_rx_octets_base[n_intf_no];
	internals->rxq_scg[0].err_pkts += 0;
	internals->rx_missed += p_nt4ga_stat->a_port_rx_drops_total[n_intf_no] -
		p_nt4ga_stat->a_port_rx_drops_base[n_intf_no];

	/* Update the increment bases */
	p_nt4ga_stat->a_port_rx_packets_base[n_intf_no] =
		p_nt4ga_stat->a_port_rx_packets_total[n_intf_no];
	p_nt4ga_stat->a_port_rx_octets_base[n_intf_no] =
		p_nt4ga_stat->a_port_rx_octets_total[n_intf_no];
	p_nt4ga_stat->a_port_rx_drops_base[n_intf_no] =
		p_nt4ga_stat->a_port_rx_drops_total[n_intf_no];

	/* Tx (here we must distinguish between physical and virtual ports) */
	if (internals->type == PORT_TYPE_PHYSICAL) {
		/* Add the statistics increments since last time we polled */
		internals->txq_scg[0].tx_pkts += p_nt4ga_stat->a_port_tx_packets_total[n_intf_no] -
			p_nt4ga_stat->a_port_tx_packets_base[n_intf_no];
		internals->txq_scg[0].tx_bytes += p_nt4ga_stat->a_port_tx_octets_total[n_intf_no] -
			p_nt4ga_stat->a_port_tx_octets_base[n_intf_no];
		internals->txq_scg[0].err_pkts += 0;

		/* Update the increment bases */
		p_nt4ga_stat->a_port_tx_packets_base[n_intf_no] =
			p_nt4ga_stat->a_port_tx_packets_total[n_intf_no];
		p_nt4ga_stat->a_port_tx_octets_base[n_intf_no] =
			p_nt4ga_stat->a_port_tx_octets_total[n_intf_no];
	}

	/* Globally only once a second */
	if ((now_rtc - last_stat_rtc) < rte_tsc_freq) {
		rte_spinlock_unlock(&hwlock);
		rte_spinlock_unlock(&p_nt_drv->stat_lck);
		return 0;
	}

	last_stat_rtc = now_rtc;

	/* All color counter are global, therefore only 1 pmd must update them */
	const struct color_counters *p_color_counters = p_nt4ga_stat->mp_stat_structs_color;
	struct color_counters *p_color_counters_base = p_nt4ga_stat->a_stat_structs_color_base;
	uint64_t color_packets_accumulated, color_bytes_accumulated;

	for (flow = 0; flow < MAX_RTE_FLOWS; flow++) {
		if (nt_flows[flow].used) {
			unsigned int color = nt_flows[flow].flow_stat_id;

			if (color < NT_MAX_COLOR_FLOW_STATS) {
				color_packets_accumulated = p_color_counters[color].color_packets;
				nt_flows[flow].stat_pkts +=
					(color_packets_accumulated -
						p_color_counters_base[color].color_packets);

				nt_flows[flow].stat_tcp_flags |= p_color_counters[color].tcp_flags;

				color_bytes_accumulated = p_color_counters[color].color_bytes;
				nt_flows[flow].stat_bytes +=
					(color_bytes_accumulated -
						p_color_counters_base[color].color_bytes);

				/* Update the counter bases */
				p_color_counters_base[color].color_packets =
					color_packets_accumulated;
				p_color_counters_base[color].color_bytes = color_bytes_accumulated;
			}
		}
	}

	rte_spinlock_unlock(&hwlock);
	rte_spinlock_unlock(&p_nt_drv->stat_lck);

	return 0;
}

static const struct ntnic_filter_ops ntnic_filter_ops = {
	.poll_statistics = poll_statistics,
};

void ntnic_filter_init(void)
{
	nthw_reg_filter_ops(&ntnic_filter_ops);
}

static const struct rte_flow_ops dev_flow_ops = {
	.create = eth_flow_create,
	.destroy = eth_flow_destroy,
	.flush = eth_flow_flush,
	.actions_update = eth_flow_actions_update,
	.dev_dump = eth_flow_dev_dump,
	.get_aged_flows = eth_flow_get_aged_flows,
	.info_get = eth_flow_info_get,
	.configure = eth_flow_configure,
	.pattern_template_create = eth_flow_pattern_template_create,
	.pattern_template_destroy = eth_flow_pattern_template_destroy,
	.actions_template_create = eth_flow_actions_template_create,
	.actions_template_destroy = eth_flow_actions_template_destroy,
	.template_table_create = eth_flow_template_table_create,
	.template_table_destroy = eth_flow_template_table_destroy,
};

void nthw_dev_flow_init(void)
{
	nthw_reg_dev_flow_ops(&dev_flow_ops);
}

static struct rte_flow_fp_ops async_dev_flow_ops = {
	.async_create = eth_flow_async_create,
	.async_destroy = eth_flow_async_destroy,
};

void nthw_dev_fp_flow_init(void)
{
	nthw_reg_dev_fp_flow_ops(&async_dev_flow_ops);
}
