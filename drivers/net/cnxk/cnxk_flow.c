/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include <cnxk_flow.h>

const struct cnxk_rte_flow_term_info term[] = {
	[RTE_FLOW_ITEM_TYPE_ETH] = {ROC_NPC_ITEM_TYPE_ETH,
				    sizeof(struct rte_flow_item_eth)},
	[RTE_FLOW_ITEM_TYPE_VLAN] = {ROC_NPC_ITEM_TYPE_VLAN,
				     sizeof(struct rte_flow_item_vlan)},
	[RTE_FLOW_ITEM_TYPE_E_TAG] = {ROC_NPC_ITEM_TYPE_E_TAG,
				      sizeof(struct rte_flow_item_e_tag)},
	[RTE_FLOW_ITEM_TYPE_IPV4] = {ROC_NPC_ITEM_TYPE_IPV4,
				     sizeof(struct rte_flow_item_ipv4)},
	[RTE_FLOW_ITEM_TYPE_IPV6] = {ROC_NPC_ITEM_TYPE_IPV6,
				     sizeof(struct rte_flow_item_ipv6)},
	[RTE_FLOW_ITEM_TYPE_IPV6_FRAG_EXT] = {
			ROC_NPC_ITEM_TYPE_IPV6_FRAG_EXT,
			sizeof(struct rte_flow_item_ipv6_frag_ext)},
	[RTE_FLOW_ITEM_TYPE_ARP_ETH_IPV4] = {
			ROC_NPC_ITEM_TYPE_ARP_ETH_IPV4,
			sizeof(struct rte_flow_item_arp_eth_ipv4)},
	[RTE_FLOW_ITEM_TYPE_MPLS] = {ROC_NPC_ITEM_TYPE_MPLS,
				     sizeof(struct rte_flow_item_mpls)},
	[RTE_FLOW_ITEM_TYPE_ICMP] = {ROC_NPC_ITEM_TYPE_ICMP,
				     sizeof(struct rte_flow_item_icmp)},
	[RTE_FLOW_ITEM_TYPE_UDP] = {ROC_NPC_ITEM_TYPE_UDP,
				    sizeof(struct rte_flow_item_udp)},
	[RTE_FLOW_ITEM_TYPE_TCP] = {ROC_NPC_ITEM_TYPE_TCP,
				    sizeof(struct rte_flow_item_tcp)},
	[RTE_FLOW_ITEM_TYPE_SCTP] = {ROC_NPC_ITEM_TYPE_SCTP,
				     sizeof(struct rte_flow_item_sctp)},
	[RTE_FLOW_ITEM_TYPE_ESP] = {ROC_NPC_ITEM_TYPE_ESP,
				    sizeof(struct rte_flow_item_esp)},
	[RTE_FLOW_ITEM_TYPE_GRE] = {ROC_NPC_ITEM_TYPE_GRE,
				    sizeof(struct rte_flow_item_gre)},
	[RTE_FLOW_ITEM_TYPE_NVGRE] = {ROC_NPC_ITEM_TYPE_NVGRE,
				      sizeof(struct rte_flow_item_nvgre)},
	[RTE_FLOW_ITEM_TYPE_VXLAN] = {ROC_NPC_ITEM_TYPE_VXLAN,
				      sizeof(struct rte_flow_item_vxlan)},
	[RTE_FLOW_ITEM_TYPE_GTPC] = {ROC_NPC_ITEM_TYPE_GTPC,
				     sizeof(struct rte_flow_item_gtp)},
	[RTE_FLOW_ITEM_TYPE_GTPU] = {ROC_NPC_ITEM_TYPE_GTPU,
				     sizeof(struct rte_flow_item_gtp)},
	[RTE_FLOW_ITEM_TYPE_GENEVE] = {ROC_NPC_ITEM_TYPE_GENEVE,
				       sizeof(struct rte_flow_item_geneve)},
	[RTE_FLOW_ITEM_TYPE_VXLAN_GPE] = {
			ROC_NPC_ITEM_TYPE_VXLAN_GPE,
			sizeof(struct rte_flow_item_vxlan_gpe)},
	[RTE_FLOW_ITEM_TYPE_IPV6_EXT] = {ROC_NPC_ITEM_TYPE_IPV6_EXT,
					 sizeof(struct rte_flow_item_ipv6_ext)},
	[RTE_FLOW_ITEM_TYPE_VOID] = {ROC_NPC_ITEM_TYPE_VOID, 0},
	[RTE_FLOW_ITEM_TYPE_ANY] = {ROC_NPC_ITEM_TYPE_ANY, 0},
	[RTE_FLOW_ITEM_TYPE_GRE_KEY] = {ROC_NPC_ITEM_TYPE_GRE_KEY,
					sizeof(uint32_t)},
	[RTE_FLOW_ITEM_TYPE_HIGIG2] = {ROC_NPC_ITEM_TYPE_HIGIG2,
				       sizeof(struct rte_flow_item_higig2_hdr)},
	[RTE_FLOW_ITEM_TYPE_RAW] = {ROC_NPC_ITEM_TYPE_RAW,
				    sizeof(struct rte_flow_item_raw)},
	[RTE_FLOW_ITEM_TYPE_MARK] = {ROC_NPC_ITEM_TYPE_MARK,
				     sizeof(struct rte_flow_item_mark)}};

static int
npc_rss_action_validate(struct rte_eth_dev *eth_dev,
			const struct rte_flow_attr *attr,
			const struct rte_flow_action *act)
{
	const struct rte_flow_action_rss *rss;

	rss = (const struct rte_flow_action_rss *)act->conf;

	if (attr->egress) {
		plt_err("No support of RSS in egress");
		return -EINVAL;
	}

	if (eth_dev->data->dev_conf.rxmode.mq_mode != RTE_ETH_MQ_RX_RSS) {
		plt_err("multi-queue mode is disabled");
		return -ENOTSUP;
	}

	if (!rss || !rss->queue_num) {
		plt_err("no valid queues");
		return -EINVAL;
	}

	if (rss->func != RTE_ETH_HASH_FUNCTION_DEFAULT) {
		plt_err("non-default RSS hash functions are not supported");
		return -ENOTSUP;
	}

	if (rss->key_len && rss->key_len > ROC_NIX_RSS_KEY_LEN) {
		plt_err("RSS hash key too large");
		return -ENOTSUP;
	}

	return 0;
}

static void
npc_rss_flowkey_get(struct cnxk_eth_dev *eth_dev,
		    const struct roc_npc_action *rss_action,
		    uint32_t *flowkey_cfg)
{
	const struct roc_npc_action_rss *rss;

	rss = (const struct roc_npc_action_rss *)rss_action->conf;

	*flowkey_cfg = cnxk_rss_ethdev_to_nix(eth_dev, rss->types, rss->level);
}

static int
cnxk_map_actions(struct rte_eth_dev *eth_dev, const struct rte_flow_attr *attr,
		 const struct rte_flow_action actions[],
		 struct roc_npc_action in_actions[], uint32_t *flowkey_cfg)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	const struct rte_flow_action_ethdev *act_ethdev;
	const struct rte_flow_action_port_id *port_act;
	const struct rte_flow_action_queue *act_q;
	struct roc_npc *roc_npc_src = &dev->npc;
	struct rte_eth_dev *portid_eth_dev;
	char if_name[RTE_ETH_NAME_MAX_LEN];
	struct cnxk_eth_dev *hw_dst;
	struct roc_npc *roc_npc_dst;
	int i = 0, rc = 0;
	int rq;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			in_actions[i].type = ROC_NPC_ACTION_TYPE_VOID;
			break;

		case RTE_FLOW_ACTION_TYPE_MARK:
			in_actions[i].type = ROC_NPC_ACTION_TYPE_MARK;
			in_actions[i].conf = actions->conf;
			break;

		case RTE_FLOW_ACTION_TYPE_FLAG:
			in_actions[i].type = ROC_NPC_ACTION_TYPE_FLAG;
			break;

		case RTE_FLOW_ACTION_TYPE_COUNT:
			in_actions[i].type = ROC_NPC_ACTION_TYPE_COUNT;
			break;

		case RTE_FLOW_ACTION_TYPE_DROP:
			in_actions[i].type = ROC_NPC_ACTION_TYPE_DROP;
			break;

		case RTE_FLOW_ACTION_TYPE_PF:
			in_actions[i].type = ROC_NPC_ACTION_TYPE_PF;
			break;

		case RTE_FLOW_ACTION_TYPE_VF:
			in_actions[i].type = ROC_NPC_ACTION_TYPE_VF;
			in_actions[i].conf = actions->conf;
			break;

		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			in_actions[i].type = ROC_NPC_ACTION_TYPE_PORT_ID;
			in_actions[i].conf = actions->conf;
			act_ethdev = (const struct rte_flow_action_ethdev *)
					   actions->conf;
			port_act = (const struct rte_flow_action_port_id *)
					   actions->conf;
			if (rte_eth_dev_get_name_by_port(
			    actions->type != RTE_FLOW_ACTION_TYPE_PORT_ID ?
			    act_ethdev->port_id : port_act->id, if_name)) {
				plt_err("Name not found for output port id");
				goto err_exit;
			}
			portid_eth_dev = rte_eth_dev_allocated(if_name);
			if (!portid_eth_dev) {
				plt_err("eth_dev not found for output port id");
				goto err_exit;
			}
			if (strcmp(portid_eth_dev->device->driver->name,
				   eth_dev->device->driver->name) != 0) {
				plt_err("Output port not under same driver");
				goto err_exit;
			}
			hw_dst = portid_eth_dev->data->dev_private;
			roc_npc_dst = &hw_dst->npc;

			rc = roc_npc_validate_portid_action(roc_npc_src,
							    roc_npc_dst);

			if (rc)
				goto err_exit;
			break;

		case RTE_FLOW_ACTION_TYPE_QUEUE:
			act_q = (const struct rte_flow_action_queue *)
					actions->conf;
			rq = act_q->index;
			if (rq >= eth_dev->data->nb_rx_queues) {
				plt_npc_dbg("Invalid queue index");
				goto err_exit;
			}
			in_actions[i].type = ROC_NPC_ACTION_TYPE_QUEUE;
			in_actions[i].conf = actions->conf;
			break;

		case RTE_FLOW_ACTION_TYPE_RSS:
			rc = npc_rss_action_validate(eth_dev, attr, actions);
			if (rc)
				goto err_exit;
			in_actions[i].type = ROC_NPC_ACTION_TYPE_RSS;
			in_actions[i].conf = actions->conf;
			npc_rss_flowkey_get(dev, &in_actions[i], flowkey_cfg);
			break;

		case RTE_FLOW_ACTION_TYPE_SECURITY:
			in_actions[i].type = ROC_NPC_ACTION_TYPE_SEC;
			in_actions[i].conf = actions->conf;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
			in_actions[i].type = ROC_NPC_ACTION_TYPE_VLAN_STRIP;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
			in_actions[i].type = ROC_NPC_ACTION_TYPE_VLAN_INSERT;
			in_actions[i].conf = actions->conf;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
			in_actions[i].type =
				ROC_NPC_ACTION_TYPE_VLAN_ETHTYPE_INSERT;
			in_actions[i].conf = actions->conf;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP:
			in_actions[i].type =
				ROC_NPC_ACTION_TYPE_VLAN_PCP_INSERT;
			in_actions[i].conf = actions->conf;
			break;
		case RTE_FLOW_ACTION_TYPE_METER:
			in_actions[i].type = ROC_NPC_ACTION_TYPE_METER;
			in_actions[i].conf = actions->conf;
			break;
		default:
			plt_npc_dbg("Action is not supported = %d",
				    actions->type);
			goto err_exit;
		}
		i++;
	}
	in_actions[i].type = ROC_NPC_ACTION_TYPE_END;
	return 0;

err_exit:
	return -EINVAL;
}

static int
cnxk_map_flow_data(struct rte_eth_dev *eth_dev,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item pattern[],
		   const struct rte_flow_action actions[],
		   struct roc_npc_attr *in_attr,
		   struct roc_npc_item_info in_pattern[],
		   struct roc_npc_action in_actions[], uint32_t *flowkey_cfg)
{
	int i = 0;

	in_attr->priority = attr->priority;
	in_attr->ingress = attr->ingress;
	in_attr->egress = attr->egress;

	while (pattern->type != RTE_FLOW_ITEM_TYPE_END) {
		in_pattern[i].spec = pattern->spec;
		in_pattern[i].last = pattern->last;
		in_pattern[i].mask = pattern->mask;
		in_pattern[i].type = term[pattern->type].item_type;
		in_pattern[i].size = term[pattern->type].item_size;
		pattern++;
		i++;
	}
	in_pattern[i].type = ROC_NPC_ITEM_TYPE_END;

	return cnxk_map_actions(eth_dev, attr, actions, in_actions,
				flowkey_cfg);
}

static int
cnxk_flow_validate(struct rte_eth_dev *eth_dev,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item pattern[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	struct roc_npc_item_info in_pattern[ROC_NPC_ITEM_TYPE_END + 1];
	struct roc_npc_action in_actions[ROC_NPC_MAX_ACTION_COUNT];
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_npc *npc = &dev->npc;
	struct roc_npc_attr in_attr;
	struct roc_npc_flow flow;
	uint32_t flowkey_cfg = 0;
	int rc;

	memset(&flow, 0, sizeof(flow));
	flow.is_validate = true;

	rc = cnxk_map_flow_data(eth_dev, attr, pattern, actions, &in_attr, in_pattern, in_actions,
				&flowkey_cfg);
	if (rc) {
		rte_flow_error_set(error, 0, RTE_FLOW_ERROR_TYPE_ACTION_NUM, NULL,
				   "Failed to map flow data");
		return rc;
	}

	rc = roc_npc_flow_parse(npc, &in_attr, in_pattern, in_actions, &flow);

	if (rc) {
		rte_flow_error_set(error, 0, rc, NULL,
				   "Flow validation failed");
		return rc;
	}
	return 0;
}

struct roc_npc_flow *
cnxk_flow_create(struct rte_eth_dev *eth_dev, const struct rte_flow_attr *attr,
		 const struct rte_flow_item pattern[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_npc_item_info in_pattern[ROC_NPC_ITEM_TYPE_END + 1];
	struct roc_npc_action in_actions[ROC_NPC_MAX_ACTION_COUNT];
	struct roc_npc *npc = &dev->npc;
	struct roc_npc_attr in_attr;
	struct roc_npc_flow *flow;
	int errcode = 0;
	int rc;

	rc = cnxk_map_flow_data(eth_dev, attr, pattern, actions, &in_attr,
				in_pattern, in_actions,
				&npc->flowkey_cfg_state);
	if (rc) {
		rte_flow_error_set(error, 0, RTE_FLOW_ERROR_TYPE_ACTION_NUM,
				   NULL, "Failed to map flow data");
		return NULL;
	}

	flow = roc_npc_flow_create(npc, &in_attr, in_pattern, in_actions,
				   &errcode);
	if (errcode != 0) {
		rte_flow_error_set(error, errcode, errcode, NULL,
				   roc_error_msg_get(errcode));
		return NULL;
	}

	return flow;
}

int
cnxk_flow_destroy(struct rte_eth_dev *eth_dev, struct roc_npc_flow *flow,
		  struct rte_flow_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_npc *npc = &dev->npc;
	int rc;

	rc = roc_npc_flow_destroy(npc, flow);
	if (rc)
		rte_flow_error_set(error, rc, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "Flow Destroy failed");
	return rc;
}

static int
cnxk_flow_flush(struct rte_eth_dev *eth_dev, struct rte_flow_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_npc *npc = &dev->npc;
	int rc;

	rc = roc_npc_mcam_free_all_resources(npc);
	if (rc) {
		rte_flow_error_set(error, EIO, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "Failed to flush filter");
		return -rte_errno;
	}

	return 0;
}

static int
cnxk_flow_query(struct rte_eth_dev *eth_dev, struct rte_flow *flow,
		const struct rte_flow_action *action, void *data,
		struct rte_flow_error *error)
{
	struct roc_npc_flow *in_flow = (struct roc_npc_flow *)flow;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_npc *npc = &dev->npc;
	struct rte_flow_query_count *query = data;
	const char *errmsg = NULL;
	int errcode = ENOTSUP;
	int rc;

	if (action->type != RTE_FLOW_ACTION_TYPE_COUNT) {
		errmsg = "Only COUNT is supported in query";
		goto err_exit;
	}

	if (in_flow->ctr_id == NPC_COUNTER_NONE) {
		errmsg = "Counter is not available";
		goto err_exit;
	}

	if (in_flow->use_pre_alloc)
		rc = roc_npc_inl_mcam_read_counter(in_flow->ctr_id, &query->hits);
	else
		rc = roc_npc_mcam_read_counter(npc, in_flow->ctr_id, &query->hits);
	if (rc != 0) {
		errcode = EIO;
		errmsg = "Error reading flow counter";
		goto err_exit;
	}
	query->hits_set = 1;
	query->bytes_set = 0;

	if (query->reset) {
		if (in_flow->use_pre_alloc)
			rc = roc_npc_inl_mcam_clear_counter(in_flow->ctr_id);
		else
			rc = roc_npc_mcam_clear_counter(npc, in_flow->ctr_id);
	}
	if (rc != 0) {
		errcode = EIO;
		errmsg = "Error clearing flow counter";
		goto err_exit;
	}

	return 0;

err_exit:
	rte_flow_error_set(error, errcode, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			   NULL, errmsg);
	return -rte_errno;
}

static int
cnxk_flow_isolate(struct rte_eth_dev *eth_dev __rte_unused,
		  int enable __rte_unused, struct rte_flow_error *error)
{
	/* If we support, we need to un-install the default mcam
	 * entry for this port.
	 */

	rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			   NULL, "Flow isolation not supported");

	return -rte_errno;
}

static int
cnxk_flow_dev_dump(struct rte_eth_dev *eth_dev, struct rte_flow *flow,
		   FILE *file, struct rte_flow_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_npc *npc = &dev->npc;

	if (file == NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Invalid file");
		return -rte_errno;
	}

	if (flow != NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL,
				   "Invalid argument");
		return -EINVAL;
	}

	roc_npc_flow_dump(file, npc);

	return 0;
}

struct rte_flow_ops cnxk_flow_ops = {
	.validate = cnxk_flow_validate,
	.flush = cnxk_flow_flush,
	.query = cnxk_flow_query,
	.isolate = cnxk_flow_isolate,
	.dev_dump = cnxk_flow_dev_dump,
};
