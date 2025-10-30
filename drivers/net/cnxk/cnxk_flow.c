/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include <cnxk_flow.h>
#include <cnxk_rep.h>

#define IS_REP_BIT 7

#define TNL_DCP_MATCH_ID 5
#define NRML_MATCH_ID	 1
const struct cnxk_rte_flow_term_info term[] = {
	[RTE_FLOW_ITEM_TYPE_ETH] = {ROC_NPC_ITEM_TYPE_ETH, sizeof(struct rte_flow_item_eth)},
	[RTE_FLOW_ITEM_TYPE_VLAN] = {ROC_NPC_ITEM_TYPE_VLAN, sizeof(struct rte_flow_item_vlan)},
	[RTE_FLOW_ITEM_TYPE_E_TAG] = {ROC_NPC_ITEM_TYPE_E_TAG, sizeof(struct rte_flow_item_e_tag)},
	[RTE_FLOW_ITEM_TYPE_IPV4] = {ROC_NPC_ITEM_TYPE_IPV4, sizeof(struct rte_flow_item_ipv4)},
	[RTE_FLOW_ITEM_TYPE_IPV6] = {ROC_NPC_ITEM_TYPE_IPV6, sizeof(struct rte_flow_item_ipv6)},
	[RTE_FLOW_ITEM_TYPE_IPV6_FRAG_EXT] = {ROC_NPC_ITEM_TYPE_IPV6_FRAG_EXT,
					      sizeof(struct rte_flow_item_ipv6_frag_ext)},
	[RTE_FLOW_ITEM_TYPE_ARP_ETH_IPV4] = {ROC_NPC_ITEM_TYPE_ARP_ETH_IPV4,
					     sizeof(struct rte_flow_item_arp_eth_ipv4)},
	[RTE_FLOW_ITEM_TYPE_MPLS] = {ROC_NPC_ITEM_TYPE_MPLS, sizeof(struct rte_flow_item_mpls)},
	[RTE_FLOW_ITEM_TYPE_ICMP] = {ROC_NPC_ITEM_TYPE_ICMP, sizeof(struct rte_flow_item_icmp)},
	[RTE_FLOW_ITEM_TYPE_UDP] = {ROC_NPC_ITEM_TYPE_UDP, sizeof(struct rte_flow_item_udp)},
	[RTE_FLOW_ITEM_TYPE_TCP] = {ROC_NPC_ITEM_TYPE_TCP, sizeof(struct rte_flow_item_tcp)},
	[RTE_FLOW_ITEM_TYPE_SCTP] = {ROC_NPC_ITEM_TYPE_SCTP, sizeof(struct rte_flow_item_sctp)},
	[RTE_FLOW_ITEM_TYPE_ESP] = {ROC_NPC_ITEM_TYPE_ESP, sizeof(struct rte_flow_item_esp)},
	[RTE_FLOW_ITEM_TYPE_GRE] = {ROC_NPC_ITEM_TYPE_GRE, sizeof(struct rte_flow_item_gre)},
	[RTE_FLOW_ITEM_TYPE_NVGRE] = {ROC_NPC_ITEM_TYPE_NVGRE, sizeof(struct rte_flow_item_nvgre)},
	[RTE_FLOW_ITEM_TYPE_VXLAN] = {ROC_NPC_ITEM_TYPE_VXLAN, sizeof(struct rte_flow_item_vxlan)},
	[RTE_FLOW_ITEM_TYPE_GTPC] = {ROC_NPC_ITEM_TYPE_GTPC, sizeof(struct rte_flow_item_gtp)},
	[RTE_FLOW_ITEM_TYPE_GTPU] = {ROC_NPC_ITEM_TYPE_GTPU, sizeof(struct rte_flow_item_gtp)},
	[RTE_FLOW_ITEM_TYPE_GENEVE] = {ROC_NPC_ITEM_TYPE_GENEVE,
				       sizeof(struct rte_flow_item_geneve)},
	[RTE_FLOW_ITEM_TYPE_VXLAN_GPE] = {ROC_NPC_ITEM_TYPE_VXLAN_GPE,
					  sizeof(struct rte_flow_item_vxlan_gpe)},
	[RTE_FLOW_ITEM_TYPE_IPV6_EXT] = {ROC_NPC_ITEM_TYPE_IPV6_EXT,
					 sizeof(struct rte_flow_item_ipv6_ext)},
	[RTE_FLOW_ITEM_TYPE_VOID] = {ROC_NPC_ITEM_TYPE_VOID, 0},
	[RTE_FLOW_ITEM_TYPE_ANY] = {ROC_NPC_ITEM_TYPE_ANY, 0},
	[RTE_FLOW_ITEM_TYPE_GRE_KEY] = {ROC_NPC_ITEM_TYPE_GRE_KEY, sizeof(uint32_t)},
	[RTE_FLOW_ITEM_TYPE_HIGIG2] = {ROC_NPC_ITEM_TYPE_HIGIG2,
				       sizeof(struct rte_flow_item_higig2_hdr)},
	[RTE_FLOW_ITEM_TYPE_RAW] = {ROC_NPC_ITEM_TYPE_RAW, sizeof(struct rte_flow_item_raw)},
	[RTE_FLOW_ITEM_TYPE_MARK] = {ROC_NPC_ITEM_TYPE_MARK, sizeof(struct rte_flow_item_mark)},
	[RTE_FLOW_ITEM_TYPE_IPV6_ROUTING_EXT] = {ROC_NPC_ITEM_TYPE_IPV6_ROUTING_EXT,
						 sizeof(struct rte_flow_item_ipv6_routing_ext)},
	[RTE_FLOW_ITEM_TYPE_TX_QUEUE] = {ROC_NPC_ITEM_TYPE_TX_QUEUE,
					 sizeof(struct rte_flow_item_tx_queue)},
	[RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT] = {ROC_NPC_ITEM_TYPE_REPRESENTED_PORT,
						 sizeof(struct rte_flow_item_ethdev)},
	[RTE_FLOW_ITEM_TYPE_PPPOES] = {ROC_NPC_ITEM_TYPE_PPPOES,
				       sizeof(struct rte_flow_item_pppoe)}
};

static int
npc_rss_action_validate(struct rte_eth_dev *eth_dev, const struct rte_flow_attr *attr,
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
npc_rss_flowkey_get(struct cnxk_eth_dev *eth_dev, const struct roc_npc_action *rss_action,
		    uint32_t *flowkey_cfg, uint64_t default_rss_types)
{
	const struct roc_npc_action_rss *rss;
	uint64_t rss_types;

	rss = (const struct roc_npc_action_rss *)rss_action->conf;
	rss_types = rss->types;
	/* If no RSS types are specified, use default one */
	if (rss_types == 0)
		rss_types = default_rss_types;

	*flowkey_cfg = cnxk_rss_ethdev_to_nix(eth_dev, rss_types, rss->level);
}

static int
npc_parse_port_id_action(struct rte_eth_dev *eth_dev, const struct rte_flow_action *action,
			 uint16_t *dst_pf_func, uint16_t *dst_channel)
{
	const struct rte_flow_action_port_id *port_act;
	struct rte_eth_dev *portid_eth_dev;
	char if_name[RTE_ETH_NAME_MAX_LEN];
	struct cnxk_eth_dev *hw_dst;
	struct roc_npc *roc_npc_dst;
	int rc = 0;

	port_act = (const struct rte_flow_action_port_id *)action->conf;

	rc = rte_eth_dev_get_name_by_port(port_act->id, if_name);
	if (rc) {
		plt_err("Name not found for output port id");
		goto err_exit;
	}
	portid_eth_dev = rte_eth_dev_allocated(if_name);
	if (!portid_eth_dev) {
		plt_err("eth_dev not found for output port id");
		goto err_exit;
	}
	if (strcmp(portid_eth_dev->device->driver->name, eth_dev->device->driver->name) != 0) {
		plt_err("Output port not under same driver");
		goto err_exit;
	}
	hw_dst = portid_eth_dev->data->dev_private;
	roc_npc_dst = &hw_dst->npc;
	*dst_pf_func = roc_npc_dst->pf_func;
	*dst_channel = hw_dst->npc.channel;

	return 0;

err_exit:
	return -EINVAL;
}

static int
roc_npc_parse_sample_subaction(struct rte_eth_dev *eth_dev, const struct rte_flow_action actions[],
			       struct roc_npc_action_sample *sample_action)
{
	uint16_t dst_pf_func = 0, dst_channel = 0;
	const struct roc_npc_action_vf *vf_act;
	int rc = 0, count = 0;
	bool is_empty = true;

	if (sample_action->ratio != 1) {
		plt_err("Sample ratio must be 1");
		return -EINVAL;
	}

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		is_empty = false;
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_PF:
			count++;
			sample_action->action_type |= ROC_NPC_ACTION_TYPE_PF;
			break;
		case RTE_FLOW_ACTION_TYPE_VF:
			count++;
			vf_act = (const struct roc_npc_action_vf *)actions->conf;
			sample_action->action_type |= ROC_NPC_ACTION_TYPE_VF;
			sample_action->pf_func = vf_act->id & NPC_PFVF_FUNC_MASK;
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			rc = npc_parse_port_id_action(eth_dev, actions, &dst_pf_func, &dst_channel);
			if (rc)
				return -EINVAL;

			count++;
			sample_action->action_type |= ROC_NPC_ACTION_TYPE_PORT_ID;
			sample_action->pf_func = dst_pf_func;
			sample_action->channel = dst_channel;
			break;
		default:
			continue;
		}
	}

	if (count > 1 || is_empty)
		return -EINVAL;

	return 0;
}

static int
append_mark_action(struct roc_npc_action *in_actions, uint8_t has_tunnel_pattern,
		   uint64_t *free_allocs, int *act_cnt)
{
	struct rte_flow_action_mark *act_mark;
	int i = *act_cnt, j = 0;

	/* Add Mark action */
	i++;
	act_mark = plt_zmalloc(sizeof(struct rte_flow_action_mark), 0);
	if (!act_mark) {
		plt_err("Error allocation memory");
		return -ENOMEM;
	}

	while (free_allocs[j] != 0)
		j++;
	free_allocs[j] = (uint64_t)act_mark;
	/* Mark ID format: (tunnel type - VxLAN, Geneve << 6) | Tunnel decap */
	act_mark->id =
		has_tunnel_pattern ? ((has_tunnel_pattern << 6) | TNL_DCP_MATCH_ID) : NRML_MATCH_ID;
	in_actions[i].type = ROC_NPC_ACTION_TYPE_MARK;
	in_actions[i].conf = (struct rte_flow_action_mark *)act_mark;

	plt_rep_dbg("Assigned mark ID %x", act_mark->id);

	*act_cnt = i;

	return 0;
}

static int
append_rss_action(struct cnxk_eth_dev *dev, struct roc_npc_action *in_actions, uint16_t nb_rxq,
		  uint32_t *flowkey_cfg, uint64_t *free_allocs, uint16_t rss_repte_pf_func,
		  int *act_cnt)
{
	struct roc_npc_action_rss *rss_conf;
	int i = *act_cnt, j = 0, l, rc = 0;
	uint16_t *queue_arr;

	rss_conf = plt_zmalloc(sizeof(struct roc_npc_action_rss), 0);
	if (!rss_conf) {
		plt_err("Failed to allocate memory for rss conf");
		rc = -ENOMEM;
		goto fail;
	}

	/* Add RSS action */
	rss_conf->queue_num = nb_rxq;
	queue_arr = calloc(1, rss_conf->queue_num * sizeof(uint16_t));
	if (!queue_arr) {
		plt_err("Failed to allocate memory for rss queue");
		rc = -ENOMEM;
		goto free_rss;
	}

	for (l = 0; l < nb_rxq; l++)
		queue_arr[l] = l;
	rss_conf->queue = queue_arr;
	rss_conf->key = NULL;
	rss_conf->types = RTE_ETH_RSS_IP | RTE_ETH_RSS_UDP | RTE_ETH_RSS_TCP;

	i++;

	in_actions[i].type = ROC_NPC_ACTION_TYPE_RSS;
	in_actions[i].conf = (struct roc_npc_action_rss *)rss_conf;
	in_actions[i].rss_repte_pf_func = rss_repte_pf_func;

	npc_rss_flowkey_get(dev, &in_actions[i], flowkey_cfg,
			    RTE_ETH_RSS_IP | RTE_ETH_RSS_UDP | RTE_ETH_RSS_TCP);

	*act_cnt = i;

	while (free_allocs[j] != 0)
		j++;
	free_allocs[j] = (uint64_t)rss_conf;

	return 0;
free_rss:
	rte_free(rss_conf);
fail:
	return rc;
}

static int
representor_rep_portid_action(struct roc_npc_action *in_actions, struct rte_eth_dev *eth_dev,
			      struct rte_eth_dev *portid_eth_dev,
			      enum rte_flow_action_type act_type, uint8_t rep_pattern,
			      uint16_t *dst_pf_func, bool is_rep, uint8_t has_tunnel_pattern,
			      uint64_t *free_allocs, int *act_cnt, uint32_t *flowkey_cfg)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct rte_eth_dev *rep_eth_dev = portid_eth_dev;
	struct rte_flow_action_of_set_vlan_vid *vlan_vid;
	struct rte_flow_action_of_set_vlan_pcp *vlan_pcp;
	struct rte_flow_action_of_push_vlan *push_vlan;
	struct rte_flow_action_queue *act_q = NULL;
	struct cnxk_rep_dev *rep_dev;
	struct roc_npc *npc;
	uint16_t vlan_tci;
	int j = 0, rc;

	/* For inserting an action in the list */
	int i = *act_cnt;

	rep_dev = cnxk_rep_pmd_priv(rep_eth_dev);
	if (!is_rep) {
		dev = cnxk_eth_pmd_priv(eth_dev);
		npc = &dev->npc;
	} else {
		npc = &rep_dev->parent_dev->npc;
	}
	if (rep_pattern >> IS_REP_BIT) { /* Check for normal/representor port as action */
		if ((rep_pattern & 0x7f) == RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR) {
			/* Case: Repr port pattern -> Default TX rule -> LBK ->
			 *  Pattern RX LBK rule hit -> Action: send to new pf_func
			 */
			if (act_type == RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR) {
				/* New pf_func corresponds to ESW + queue corresponding to rep_id */
				act_q = plt_zmalloc(sizeof(struct rte_flow_action_queue), 0);
				if (!act_q) {
					plt_err("Error allocation memory");
					return -ENOMEM;
				}
				act_q->index = rep_dev->rep_id;

				while (free_allocs[j] != 0)
					j++;
				free_allocs[j] = (uint64_t)act_q;
				in_actions[i].type = ROC_NPC_ACTION_TYPE_QUEUE;
				in_actions[i].conf = (struct rte_flow_action_queue *)act_q;
				npc->rep_act_pf_func = rep_dev->parent_dev->npc.pf_func;
			} else {
				/* New pf_func corresponds to hw_func of representee */
				in_actions[i].type = ROC_NPC_ACTION_TYPE_PORT_ID;
				npc->rep_act_pf_func = rep_dev->hw_func;
				*dst_pf_func = rep_dev->hw_func;
			}
			/* Additional action to strip the VLAN from packets received by LBK */
			i++;
			in_actions[i].type = ROC_NPC_ACTION_TYPE_VLAN_STRIP;
			goto done;
		}
		/* Case: Repd port pattern -> TX Rule with VLAN -> LBK -> Default RX LBK rule hit
		 * base on vlan, if packet goes to ESW or actual pf_func -> Action :
		 *    act port_representor: send to ESW respective using 1<<8 | rep_id as tci value
		 *    act represented_port: send to actual port using rep_id as tci value.
		 */
		/* Add RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN action */
		push_vlan = plt_zmalloc(sizeof(struct rte_flow_action_of_push_vlan), 0);
		if (!push_vlan) {
			plt_err("Error allocation memory");
			return -ENOMEM;
		}

		while (free_allocs[j] != 0)
			j++;
		free_allocs[j] = (uint64_t)push_vlan;
		push_vlan->ethertype = ntohs(ROC_ESWITCH_VLAN_TPID);
		in_actions[i].type = ROC_NPC_ACTION_TYPE_VLAN_ETHTYPE_INSERT;
		in_actions[i].conf = (struct rte_flow_action_of_push_vlan *)push_vlan;
		i++;

		/* Add RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP action */
		vlan_pcp = plt_zmalloc(sizeof(struct rte_flow_action_of_set_vlan_pcp), 0);
		if (!vlan_pcp) {
			plt_err("Error allocation memory");
			return -ENOMEM;
		}

		free_allocs[j + 1] = (uint64_t)vlan_pcp;
		vlan_pcp->vlan_pcp = 0;
		in_actions[i].type = ROC_NPC_ACTION_TYPE_VLAN_PCP_INSERT;
		in_actions[i].conf = (struct rte_flow_action_of_set_vlan_pcp *)vlan_pcp;
		i++;

		/* Add RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID action */
		vlan_vid = plt_zmalloc(sizeof(struct rte_flow_action_of_set_vlan_vid), 0);
		if (!vlan_vid) {
			plt_err("Error allocation memory");
			return -ENOMEM;
		}

		free_allocs[j + 2] = (uint64_t)vlan_vid;
		if (act_type == RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR)
			vlan_tci = rep_dev->rep_id | (1ULL << CNXK_ESWITCH_VFPF_SHIFT);
		else
			vlan_tci = rep_dev->rep_id;
		vlan_vid->vlan_vid = ntohs(vlan_tci);
		in_actions[i].type = ROC_NPC_ACTION_TYPE_VLAN_INSERT;
		in_actions[i].conf = (struct rte_flow_action_of_set_vlan_vid *)vlan_vid;

		/* Change default channel to UCAST_CHAN (63) while sending */
		npc->rep_act_rep = true;
	} else {
		if (act_type == RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR) {
			/* Case: Pattern wire port ->  Pattern RX rule->
			 * Action: pf_func = ESW. queue = rep_id
			 */
			act_q = plt_zmalloc(sizeof(struct rte_flow_action_queue), 0);
			if (!act_q) {
				plt_err("Error allocation memory");
				return -ENOMEM;
			}
			while (free_allocs[j] != 0)
				j++;
			free_allocs[j] = (uint64_t)act_q;
			act_q->index = rep_dev->rep_id;

			in_actions[i].type = ROC_NPC_ACTION_TYPE_QUEUE;
			in_actions[i].conf = (struct rte_flow_action_queue *)act_q;
			npc->rep_act_pf_func = rep_dev->parent_dev->npc.pf_func;
		} else {
			/* Case: Pattern wire port -> Pattern RX rule->
			 * Action: Receive at actual hw_func
			 */
			in_actions[i].type = ROC_NPC_ACTION_TYPE_PORT_ID;
			npc->rep_act_pf_func = rep_dev->hw_func;
			*dst_pf_func = rep_dev->hw_func;

			/* Append a mark action - needed to identify the flow */
			rc = append_mark_action(in_actions, has_tunnel_pattern, free_allocs, &i);
			if (rc)
				return rc;
			/* Append RSS action if representee has RSS enabled */
			if (rep_dev->nb_rxq > 1) {
				/* PF can install rule for only its VF acting as representee */
				if (rep_dev->hw_func &&
				    roc_eswitch_is_repte_pfs_vf(rep_dev->hw_func,
							roc_nix_get_pf_func(npc->roc_nix))) {
					rc = append_rss_action(dev, in_actions, rep_dev->nb_rxq,
							       flowkey_cfg, free_allocs,
							       rep_dev->hw_func, &i);
					if (rc)
						return rc;
				}
			}
		}
	}
done:
	*act_cnt = i;

	return 0;
}

static int
representor_portid_action(struct roc_npc_action *in_actions, struct rte_eth_dev *portid_eth_dev,
			  uint16_t *dst_pf_func, uint8_t has_tunnel_pattern, uint64_t *free_allocs,
			  int *act_cnt)
{
	struct rte_eth_dev *rep_eth_dev = portid_eth_dev;
	struct cnxk_rep_dev *rep_dev;
	/* For inserting an action in the list */
	int i = *act_cnt, rc;

	rep_dev = cnxk_rep_pmd_priv(rep_eth_dev);

	*dst_pf_func = rep_dev->hw_func;

	rc = append_mark_action(in_actions, has_tunnel_pattern, free_allocs, &i);
	if (rc)
		return rc;

	*act_cnt = i;
	plt_rep_dbg("Rep port %d ID %d rep_dev->hw_func 0x%x", rep_dev->port_id, rep_dev->rep_id,
		    rep_dev->hw_func);

	return 0;
}

static int
cnxk_map_actions(struct rte_eth_dev *eth_dev, const struct rte_flow_attr *attr,
		 const struct rte_flow_action actions[], struct roc_npc_action in_actions[],
		 struct roc_npc_action_sample *in_sample_actions, uint32_t *flowkey_cfg,
		 uint16_t *dst_pf_func, uint64_t *npc_default_action, uint8_t has_tunnel_pattern,
		 bool is_rep, uint8_t rep_pattern, uint64_t *free_allocs)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	const struct rte_flow_action_queue *act_q = NULL;
	const struct rte_flow_action_ethdev *act_ethdev;
	const struct rte_flow_action_sample *act_sample;
	const struct rte_flow_action_port_id *port_act;
	struct rte_eth_dev *portid_eth_dev;
	char if_name[RTE_ETH_NAME_MAX_LEN];
	struct cnxk_eth_dev *hw_dst;
	struct roc_npc *roc_npc_dst;
	bool is_vf_action = false;
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
			is_vf_action = true;
			break;

		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
		case RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR:
			in_actions[i].conf = actions->conf;
			act_ethdev = (const struct rte_flow_action_ethdev *)actions->conf;
			if (rte_eth_dev_get_name_by_port(act_ethdev->port_id, if_name)) {
				plt_err("Name not found for output port id");
				goto err_exit;
			}
			portid_eth_dev = rte_eth_dev_allocated(if_name);
			if (!portid_eth_dev) {
				plt_err("eth_dev not found for output port id");
				goto err_exit;
			}

			plt_rep_dbg("Rule installed by port %d if_name %s act_ethdev->port_id %d",
				    eth_dev->data->port_id, if_name, act_ethdev->port_id);
			if (cnxk_ethdev_is_representor(if_name)) {
				if (representor_rep_portid_action(in_actions, eth_dev,
								  portid_eth_dev, actions->type,
								  rep_pattern, dst_pf_func,
								  is_rep, has_tunnel_pattern,
								  free_allocs, &i, flowkey_cfg)) {
					plt_err("Representor port action set failed");
					goto err_exit;
				}
			} else {
				if (actions->type == RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT)
					continue;
				/* Normal port as represented_port as action not supported*/
				return -ENOTSUP;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			/* No port ID action on representor ethdevs */
			if (is_rep)
				continue;
			in_actions[i].type = ROC_NPC_ACTION_TYPE_PORT_ID;
			in_actions[i].conf = actions->conf;
			act_ethdev = (const struct rte_flow_action_ethdev *)actions->conf;
			port_act = (const struct rte_flow_action_port_id *)actions->conf;
			if (rte_eth_dev_get_name_by_port(
				    actions->type != RTE_FLOW_ACTION_TYPE_PORT_ID ?
					    act_ethdev->port_id :
					    port_act->id,
				    if_name)) {
				plt_err("Name not found for output port id");
				goto err_exit;
			}
			portid_eth_dev = rte_eth_dev_allocated(if_name);
			if (!portid_eth_dev) {
				plt_err("eth_dev not found for output port id");
				goto err_exit;
			}

			if (cnxk_ethdev_is_representor(if_name)) {
				plt_rep_dbg("Representor port %d act port %d", port_act->id,
					    act_ethdev->port_id);
				if (representor_portid_action(in_actions, portid_eth_dev,
							      dst_pf_func, has_tunnel_pattern,
							      free_allocs, &i)) {
					plt_err("Representor port action set failed");
					goto err_exit;
				}
			} else {
				if (strcmp(portid_eth_dev->device->driver->name,
					   eth_dev->device->driver->name) != 0) {
					plt_err("Output port not under same driver");
					goto err_exit;
				}

				hw_dst = portid_eth_dev->data->dev_private;
				roc_npc_mcam_default_rule_action_get(&hw_dst->npc,
								     npc_default_action);
				roc_npc_dst = &hw_dst->npc;
				*dst_pf_func = roc_npc_dst->pf_func;
			}
			break;

		case RTE_FLOW_ACTION_TYPE_QUEUE:
			act_q = (const struct rte_flow_action_queue *)actions->conf;
			in_actions[i].type = ROC_NPC_ACTION_TYPE_QUEUE;
			in_actions[i].conf = actions->conf;
			break;

		case RTE_FLOW_ACTION_TYPE_RSS:
			/* No RSS action on representor ethdevs */
			if (is_rep)
				continue;
			rc = npc_rss_action_validate(eth_dev, attr, actions);
			if (rc)
				goto err_exit;

			in_actions[i].type = ROC_NPC_ACTION_TYPE_RSS;
			in_actions[i].conf = actions->conf;
			npc_rss_flowkey_get(dev, &in_actions[i], flowkey_cfg,
					    eth_dev->data->dev_conf.rx_adv_conf.rss_conf.rss_hf);
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
		case RTE_FLOW_ACTION_TYPE_AGE:
			in_actions[i].type = ROC_NPC_ACTION_TYPE_AGE;
			in_actions[i].conf = actions->conf;
			break;
		case RTE_FLOW_ACTION_TYPE_SAMPLE:
			act_sample = actions->conf;
			in_sample_actions->ratio = act_sample->ratio;
			rc = roc_npc_parse_sample_subaction(eth_dev, act_sample->actions,
							    in_sample_actions);
			if (rc) {
				plt_err("Sample subaction parsing failed.");
				goto err_exit;
			}

			in_actions[i].type = ROC_NPC_ACTION_TYPE_SAMPLE;
			in_actions[i].conf = in_sample_actions;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
			continue;
		default:
			plt_npc_dbg("Action is not supported = %d", actions->type);
			goto err_exit;
		}
		i++;
	}

	if (!is_vf_action && act_q) {
		rq = act_q->index;
		if (rq >= eth_dev->data->nb_rx_queues) {
			plt_npc_dbg("Invalid queue index");
			goto err_exit;
		}
	}
	in_actions[i].type = ROC_NPC_ACTION_TYPE_END;
	return 0;

err_exit:
	return -EINVAL;
}

static int
cnxk_map_pattern(struct rte_eth_dev *eth_dev, const struct rte_flow_item pattern[],
		 struct roc_npc_item_info in_pattern[], uint8_t *has_tunnel_pattern, bool is_rep,
		 uint8_t *rep_pattern, uint64_t *free_allocs)
{
	const struct rte_flow_item_ethdev *rep_eth_dev;
	struct rte_eth_dev *portid_eth_dev;
	char if_name[RTE_ETH_NAME_MAX_LEN];
	struct cnxk_eth_dev *hw_dst;
	struct cnxk_rep_dev *rdev;
	struct cnxk_eth_dev *dev;
	struct roc_npc *npc;
	int i = 0, j = 0;

	if (!is_rep) {
		dev = cnxk_eth_pmd_priv(eth_dev);
		npc = &dev->npc;
	} else {
		rdev = cnxk_rep_pmd_priv(eth_dev);
		npc = &rdev->parent_dev->npc;

		npc->rep_npc = npc;
		npc->rep_port_id = rdev->port_id;
		npc->rep_pf_func = rdev->hw_func;
	}

	while (pattern->type != RTE_FLOW_ITEM_TYPE_END) {
		in_pattern[i].spec = pattern->spec;
		in_pattern[i].last = pattern->last;
		in_pattern[i].mask = pattern->mask;
		in_pattern[i].type = term[pattern->type].item_type;
		in_pattern[i].size = term[pattern->type].item_size;
		if (pattern->type == RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT ||
		    pattern->type == RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR) {
			rep_eth_dev = (const struct rte_flow_item_ethdev *)pattern->spec;
			if (rte_eth_dev_get_name_by_port(rep_eth_dev->port_id, if_name)) {
				plt_err("Name not found for output port id");
				goto fail;
			}
			portid_eth_dev = rte_eth_dev_allocated(if_name);
			if (!portid_eth_dev) {
				plt_err("eth_dev not found for output port id");
				goto fail;
			}
			*rep_pattern = pattern->type;
			if (cnxk_ethdev_is_representor(if_name)) {
				/* Case where represented port not part of same
				 * app and represented by a representor port.
				 */
				struct cnxk_rep_dev *rep_dev;
				struct cnxk_eswitch_dev *eswitch_dev;

				rep_dev = cnxk_rep_pmd_priv(portid_eth_dev);
				eswitch_dev = rep_dev->parent_dev;
				npc->rep_npc = &eswitch_dev->npc;
				npc->rep_port_id = rep_eth_dev->port_id;
				npc->rep_pf_func = rep_dev->hw_func;

				if (pattern->type == RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR) {
					struct rte_flow_item_vlan *vlan;

					npc->rep_pf_func = eswitch_dev->npc.pf_func;
					/* Add VLAN pattern corresponding to rep_id */
					i++;
					vlan = plt_zmalloc(sizeof(struct rte_flow_item_vlan), 0);
					if (!vlan) {
						plt_err("error allocation memory");
						return -ENOMEM;
					}

					while (free_allocs[j] != 0)
						j++;
					free_allocs[j] = (uint64_t)vlan;

					npc->rep_rx_channel = ROC_ESWITCH_LBK_CHAN;
					vlan->hdr.vlan_tci = RTE_BE16(rep_dev->rep_id);
					in_pattern[i].spec = (struct rte_flow_item_vlan *)vlan;
					in_pattern[i].last = NULL;
					in_pattern[i].mask = &rte_flow_item_vlan_mask;
					in_pattern[i].type =
						term[RTE_FLOW_ITEM_TYPE_VLAN].item_type;
					in_pattern[i].size =
						term[RTE_FLOW_ITEM_TYPE_VLAN].item_size;
				}
				*rep_pattern |= 1 << IS_REP_BIT;
				plt_rep_dbg("Represented port %d act port %d rep_dev->hw_func 0x%x",
					    rep_eth_dev->port_id, eth_dev->data->port_id,
					    rep_dev->hw_func);
			} else {
				if (strcmp(portid_eth_dev->device->driver->name,
					   eth_dev->device->driver->name) != 0) {
					plt_err("Output port not under same driver");
					goto fail;
				}
				/* Normal port as port_representor pattern can't be supported */
				if (pattern->type == RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR)
					return -ENOTSUP;
				/* Case where represented port part of same app
				 * as PF.
				 */
				hw_dst = portid_eth_dev->data->dev_private;
				npc->rep_npc = &hw_dst->npc;
				npc->rep_port_id = rep_eth_dev->port_id;
				npc->rep_pf_func = hw_dst->npc.pf_func;
			}
		}

		if (pattern->type == RTE_FLOW_ITEM_TYPE_VXLAN ||
		    pattern->type == RTE_FLOW_ITEM_TYPE_VXLAN_GPE ||
		    pattern->type == RTE_FLOW_ITEM_TYPE_GRE)
			*has_tunnel_pattern = pattern->type;

		pattern++;
		i++;
	}
	in_pattern[i].type = ROC_NPC_ITEM_TYPE_END;
	return 0;
fail:
	return -EINVAL;
}

static int
cnxk_map_flow_data(struct rte_eth_dev *eth_dev, const struct rte_flow_attr *attr,
		   const struct rte_flow_item pattern[], const struct rte_flow_action actions[],
		   struct roc_npc_attr *in_attr, struct roc_npc_item_info in_pattern[],
		   struct roc_npc_action in_actions[],
		   struct roc_npc_action_sample *in_sample_actions, uint32_t *flowkey_cfg,
		   uint16_t *dst_pf_func, uint64_t *def_action, bool is_rep, uint64_t *free_allocs)
{
	uint8_t has_tunnel_pattern = 0, rep_pattern = 0;
	int rc;

	in_attr->priority = attr->priority;
	in_attr->ingress = attr->ingress;
	in_attr->egress = attr->egress;

	rc = cnxk_map_pattern(eth_dev, pattern, in_pattern, &has_tunnel_pattern, is_rep,
			      &rep_pattern, free_allocs);
	if (rc) {
		plt_err("Failed to map pattern list");
		return rc;
	}

	if (attr->transfer) {
		/* rep_pattern is used to identify if RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT
		 * OR RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR is defined + if pattern's portid is
		 * normal port or representor port.
		 * For normal port_id, rep_pattern = pattern-> type
		 * For representor port, rep_pattern = pattern-> type | 1 << IS_REP_BIT
		 */
		if (is_rep || rep_pattern) {
			if (rep_pattern == RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT ||
			    ((rep_pattern & 0x7f) == RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR))
				/* If pattern is port_representor or pattern has normal port as
				 * represented port, install ingress rule.
				 */
				in_attr->ingress = attr->transfer;
			else
				in_attr->egress = attr->transfer;
		} else {
			in_attr->ingress = attr->transfer;
		}
	}

	return cnxk_map_actions(eth_dev, attr, actions, in_actions, in_sample_actions, flowkey_cfg,
				dst_pf_func, def_action, has_tunnel_pattern, is_rep, rep_pattern,
				free_allocs);
}

int
cnxk_flow_validate_common(struct rte_eth_dev *eth_dev, const struct rte_flow_attr *attr,
			  const struct rte_flow_item pattern[],
			  const struct rte_flow_action actions[], struct rte_flow_error *error,
			  bool is_rep)
{
	struct roc_npc_item_info in_pattern[ROC_NPC_ITEM_TYPE_END + 1];
	struct roc_npc_action in_actions[ROC_NPC_MAX_ACTION_COUNT];
	struct roc_npc_action_sample in_sample_action;
	uint64_t npc_default_action = 0;
	struct cnxk_rep_dev *rep_dev;
	struct roc_npc_attr in_attr;
	uint64_t *free_allocs, sz;
	struct cnxk_eth_dev *dev;
	struct roc_npc_flow flow;
	uint16_t dst_pf_func = 0;
	uint32_t flowkey_cfg = 0;
	struct roc_npc *npc = 0;
	int rc, j;

	/* is_rep set for operation performed via representor ports */
	if (!is_rep) {
		dev = cnxk_eth_pmd_priv(eth_dev);
		npc = &dev->npc;
		/* Skip flow validation for MACsec. */
		if (actions[0].type == RTE_FLOW_ACTION_TYPE_SECURITY &&
		    cnxk_eth_macsec_sess_get_by_sess(dev, actions[0].conf) != NULL)
			return 0;
	} else {
		rep_dev = cnxk_rep_pmd_priv(eth_dev);
		npc = &rep_dev->parent_dev->npc;
	}

	memset(&flow, 0, sizeof(flow));
	memset(&in_sample_action, 0, sizeof(in_sample_action));
	flow.is_validate = true;

	sz = ROC_NPC_MAX_ACTION_COUNT + ROC_NPC_ITEM_TYPE_END + 1;
	free_allocs = plt_zmalloc(sz * sizeof(uint64_t), 0);
	if (!free_allocs) {
		rte_flow_error_set(error, -ENOMEM, RTE_FLOW_ERROR_TYPE_ACTION_NUM, NULL,
				   "Failed to map flow data");
		return -ENOMEM;
	}
	rc = cnxk_map_flow_data(eth_dev, attr, pattern, actions, &in_attr, in_pattern, in_actions,
				&in_sample_action, &flowkey_cfg, &dst_pf_func, &npc_default_action,
				is_rep, free_allocs);
	if (rc) {
		rte_flow_error_set(error, 0, RTE_FLOW_ERROR_TYPE_ACTION_NUM, NULL,
				   "Failed to map flow data");
		goto clean;
	}

	rc = roc_npc_flow_parse(npc, &in_attr, in_pattern, in_actions, &flow);

	if (rc) {
		rte_flow_error_set(error, 0, rc, NULL,
				   "Flow validation failed");
		goto clean;
	}
clean:
	/* Freeing the allocations done for additional patterns/actions */
	for (j = 0; (j < (int)sz) && free_allocs[j]; j++)
		plt_free((void *)free_allocs[j]);
	plt_free(free_allocs);

	return rc;
}

static int
cnxk_flow_validate(struct rte_eth_dev *eth_dev, const struct rte_flow_attr *attr,
		   const struct rte_flow_item pattern[], const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	return cnxk_flow_validate_common(eth_dev, attr, pattern, actions, error, false);
}

struct roc_npc_flow *
cnxk_flow_create_common(struct rte_eth_dev *eth_dev, const struct rte_flow_attr *attr,
			const struct rte_flow_item pattern[],
			const struct rte_flow_action actions[], struct rte_flow_error *error,
			bool is_rep)
{
	struct roc_npc_item_info in_pattern[ROC_NPC_ITEM_TYPE_END + 1] = {0};
	struct roc_npc_action in_actions[ROC_NPC_MAX_ACTION_COUNT] = {0};
	struct roc_npc_action_sample in_sample_action;
	struct cnxk_rep_dev *rep_dev = NULL;
	struct roc_npc_flow *flow = NULL;
	struct cnxk_eth_dev *dev = NULL;
	uint64_t npc_default_action = 0;
	struct roc_npc_attr in_attr;
	struct roc_npc *npc = NULL;
	uint64_t *free_allocs, sz;
	uint16_t dst_pf_func = 0;
	int errcode = 0;
	int rc, j;

	/* is_rep set for operation performed via representor ports */
	if (!is_rep) {
		dev = cnxk_eth_pmd_priv(eth_dev);
		npc = &dev->npc;
	} else {
		rep_dev = cnxk_rep_pmd_priv(eth_dev);
		npc = &rep_dev->parent_dev->npc;
	}

	sz = ROC_NPC_MAX_ACTION_COUNT + ROC_NPC_ITEM_TYPE_END + 1;
	free_allocs = plt_zmalloc(sz * sizeof(uint64_t), 0);
	if (!free_allocs) {
		rte_flow_error_set(error, -ENOMEM, RTE_FLOW_ERROR_TYPE_ACTION_NUM, NULL,
				   "Failed to map flow data");
		return NULL;
	}
	memset(&in_sample_action, 0, sizeof(in_sample_action));
	memset(&in_attr, 0, sizeof(struct roc_npc_attr));
	rc = cnxk_map_flow_data(eth_dev, attr, pattern, actions, &in_attr, in_pattern, in_actions,
				&in_sample_action, &npc->flowkey_cfg_state, &dst_pf_func,
				&npc_default_action, is_rep, free_allocs);
	if (rc) {
		rte_flow_error_set(error, rc, RTE_FLOW_ERROR_TYPE_ACTION_NUM, NULL,
				   "Failed to map flow data");
		goto clean;
	}

	flow = roc_npc_flow_create(npc, &in_attr, in_pattern, in_actions, dst_pf_func,
				   npc_default_action, &errcode);
	if (errcode != 0) {
		rte_flow_error_set(error, errcode, errcode, NULL, roc_error_msg_get(errcode));
		goto clean;
	}

clean:
	/* Freeing the allocations done for additional patterns/actions */
	for (j = 0; (j < (int)sz) && free_allocs[j]; j++)
		plt_free((void *)free_allocs[j]);
	plt_free(free_allocs);

	return flow;
}

int
cnxk_flow_destroy_common(struct rte_eth_dev *eth_dev, struct roc_npc_flow *flow,
			 struct rte_flow_error *error, bool is_rep)
{
	struct cnxk_rep_dev *rep_dev;
	struct cnxk_eth_dev *dev;
	struct roc_npc *npc;
	int rc;

	/* is_rep set for operation performed via representor ports */
	if (!is_rep) {
		dev = cnxk_eth_pmd_priv(eth_dev);
		npc = &dev->npc;
	} else {
		rep_dev = cnxk_rep_pmd_priv(eth_dev);
		npc = &rep_dev->parent_dev->npc;
	}

	rc = roc_npc_flow_destroy(npc, flow);
	if (rc)
		rte_flow_error_set(error, rc, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Flow Destroy failed");
	return rc;
}

int
cnxk_flow_destroy(struct rte_eth_dev *eth_dev, struct roc_npc_flow *flow,
		  struct rte_flow_error *error)
{
	return cnxk_flow_destroy_common(eth_dev, flow, error, false);
}

int
cnxk_flow_flush_common(struct rte_eth_dev *eth_dev, struct rte_flow_error *error, bool is_rep)
{
	struct cnxk_rep_dev *rep_dev;
	struct cnxk_eth_dev *dev;
	struct roc_npc *npc;
	int rc;

	/* is_rep set for operation performed via representor ports */
	if (!is_rep) {
		dev = cnxk_eth_pmd_priv(eth_dev);
		npc = &dev->npc;
	} else {
		rep_dev = cnxk_rep_pmd_priv(eth_dev);
		npc = &rep_dev->parent_dev->npc;
	}

	rc = roc_npc_mcam_free_all_resources(npc);
	if (rc) {
		rte_flow_error_set(error, EIO, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Failed to flush filter");
		return -rte_errno;
	}

	return 0;
}

static int
cnxk_flow_flush(struct rte_eth_dev *eth_dev, struct rte_flow_error *error)
{
	return cnxk_flow_flush_common(eth_dev, error, false);
}

int
cnxk_flow_query_common(struct rte_eth_dev *eth_dev, struct rte_flow *flow,
		       const struct rte_flow_action *action, void *data,
		       struct rte_flow_error *error, bool is_rep)
{
	struct roc_npc_flow *in_flow = (struct roc_npc_flow *)flow;
	struct rte_flow_query_count *query = data;
	struct cnxk_rep_dev *rep_dev;
	struct cnxk_eth_dev *dev;
	struct roc_npc *npc;
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

	/* is_rep set for operation performed via representor ports */
	if (!is_rep) {
		dev = cnxk_eth_pmd_priv(eth_dev);
		npc = &dev->npc;
	} else {
		rep_dev = cnxk_rep_pmd_priv(eth_dev);
		npc = &rep_dev->parent_dev->npc;
	}

	if (in_flow->use_pre_alloc) {
		rc = roc_npc_inl_mcam_read_counter(in_flow->ctr_id, &query->hits);
	} else {
		if (roc_model_is_cn20k())
			rc = roc_npc_mcam_get_stats(npc, in_flow, &query->hits);
		else
			rc = roc_npc_mcam_read_counter(npc, in_flow->ctr_id, &query->hits);
	}
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
cnxk_flow_query(struct rte_eth_dev *eth_dev, struct rte_flow *flow,
		const struct rte_flow_action *action, void *data, struct rte_flow_error *error)
{
	return cnxk_flow_query_common(eth_dev, flow, action, data, error, false);
}

static int
cnxk_flow_isolate(struct rte_eth_dev *eth_dev __rte_unused, int enable __rte_unused,
		  struct rte_flow_error *error)
{
	/* If we support, we need to un-install the default mcam
	 * entry for this port.
	 */

	rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			   NULL, "Flow isolation not supported");

	return -rte_errno;
}

int
cnxk_flow_dev_dump_common(struct rte_eth_dev *eth_dev, struct rte_flow *flow, FILE *file,
			  struct rte_flow_error *error, bool is_rep)
{
	struct cnxk_rep_dev *rep_dev;
	struct cnxk_eth_dev *dev;
	struct roc_npc *npc;

	/* is_rep set for operation performed via representor ports */
	if (!is_rep) {
		dev = cnxk_eth_pmd_priv(eth_dev);
		npc = &dev->npc;
	} else {
		rep_dev = cnxk_rep_pmd_priv(eth_dev);
		npc = &rep_dev->parent_dev->npc;
	}

	if (file == NULL) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Invalid file");
		return -rte_errno;
	}

	if (flow != NULL) {
		roc_npc_flow_mcam_dump(file, npc, (struct roc_npc_flow *)flow);
		return 0;
	}

	roc_npc_flow_dump(file, npc, -1);

	return 0;
}

static int
cnxk_flow_dev_dump(struct rte_eth_dev *eth_dev, struct rte_flow *flow, FILE *file,
		   struct rte_flow_error *error)
{
	return cnxk_flow_dev_dump_common(eth_dev, flow, file, error, false);
}

static int
cnxk_flow_get_aged_flows(struct rte_eth_dev *eth_dev, void **context, uint32_t nb_contexts,
			 struct rte_flow_error *err)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_npc *roc_npc = &dev->npc;
	struct roc_npc_flow_age *flow_age;
	uint32_t start_id;
	uint32_t end_id;
	int cnt = 0;
	uint32_t sn;
	uint32_t i;

	RTE_SET_USED(err);

	flow_age = &roc_npc->flow_age;

	if (!flow_age->age_flow_refcnt)
		return 0;

	do {
		sn = plt_seqcount_read_begin(&flow_age->seq_cnt);

		if (nb_contexts == 0) {
			cnt = flow_age->aged_flows_cnt;
		} else {
			start_id = flow_age->start_id;
			end_id = flow_age->end_id;
			for (i = start_id; i <= end_id; i++) {
				if ((int)nb_contexts == cnt)
					break;
				if (plt_bitmap_get(flow_age->aged_flows, i)) {
					context[cnt] =
						roc_npc_aged_flow_ctx_get(roc_npc, i);
					cnt++;
				}
			}
		}
	} while (plt_seqcount_read_retry(&flow_age->seq_cnt, sn));

	return cnt;
}

static int
cnxk_flow_tunnel_decap_set(__rte_unused struct rte_eth_dev *dev, struct rte_flow_tunnel *tunnel,
			   struct rte_flow_action **pmd_actions, uint32_t *num_of_actions,
			   __rte_unused struct rte_flow_error *err)
{
	struct rte_flow_action *nfp_action;

	nfp_action = rte_zmalloc("nfp_tun_action", sizeof(struct rte_flow_action), 0);
	if (nfp_action == NULL) {
		plt_err("Alloc memory for nfp tunnel action failed.");
		return -ENOMEM;
	}

	if (tunnel->is_ipv6)
		nfp_action->conf = (void *)~0;

	switch (tunnel->type) {
	case RTE_FLOW_ITEM_TYPE_VXLAN:
		nfp_action->type = RTE_FLOW_ACTION_TYPE_VXLAN_DECAP;
		*pmd_actions = nfp_action;
		*num_of_actions = 1;
		break;
	default:
		*pmd_actions = NULL;
		*num_of_actions = 0;
		rte_free(nfp_action);
		break;
	}

	return 0;
}

static int
cnxk_flow_tunnel_action_decap_release(__rte_unused struct rte_eth_dev *dev,
				      struct rte_flow_action *pmd_actions, uint32_t num_of_actions,
				      __rte_unused struct rte_flow_error *err)
{
	uint32_t i;
	struct rte_flow_action *nfp_action;

	for (i = 0; i < num_of_actions; i++) {
		nfp_action = &pmd_actions[i];
		nfp_action->conf = NULL;
		rte_free(nfp_action);
	}

	return 0;
}

static int
cnxk_flow_tunnel_match(__rte_unused struct rte_eth_dev *dev,
		       __rte_unused struct rte_flow_tunnel *tunnel,
		       __rte_unused struct rte_flow_item **pmd_items, uint32_t *num_of_items,
		       __rte_unused struct rte_flow_error *err)
{
	*num_of_items = 0;

	return 0;
}

static int
cnxk_flow_tunnel_item_release(__rte_unused struct rte_eth_dev *dev,
			      __rte_unused struct rte_flow_item *pmd_items,
			      __rte_unused uint32_t num_of_items,
			      __rte_unused struct rte_flow_error *err)
{
	return 0;
}

struct rte_flow_ops cnxk_flow_ops = {
	.validate = cnxk_flow_validate,
	.flush = cnxk_flow_flush,
	.query = cnxk_flow_query,
	.isolate = cnxk_flow_isolate,
	.dev_dump = cnxk_flow_dev_dump,
	.get_aged_flows = cnxk_flow_get_aged_flows,
	.tunnel_match = cnxk_flow_tunnel_match,
	.tunnel_item_release = cnxk_flow_tunnel_item_release,
	.tunnel_decap_set = cnxk_flow_tunnel_decap_set,
	.tunnel_action_decap_release = cnxk_flow_tunnel_action_decap_release,
};
