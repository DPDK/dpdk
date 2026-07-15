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
				       sizeof(struct rte_flow_item_pppoe)}};

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
	queue_arr = plt_zmalloc(rss_conf->queue_num * sizeof(uint16_t), 0);
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
	j++;
	free_allocs[j] = (uint64_t)queue_arr;

	return 0;
free_rss:
	plt_free(rss_conf);
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
		 bool is_rep, uint8_t rep_pattern, uint64_t *free_allocs, uint32_t flow_flags)
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
			in_actions[i].is_non_inp = flow_flags & CNXK_FLOW_NON_INPLACE;
			in_actions[i].no_sec_action = flow_flags & CNXK_FLOW_NO_SEC_ACTION;
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
		   uint16_t *dst_pf_func, uint64_t *def_action, bool is_rep, uint64_t *free_allocs,
		   uint32_t flow_flags)
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
				free_allocs, flow_flags);
}

int
cnxk_flow_validate_common(struct rte_eth_dev *eth_dev, const struct rte_flow_attr *attr,
			  const struct rte_flow_item pattern[],
			  const struct rte_flow_action actions[], struct rte_flow_error *error,
			  bool is_rep, uint32_t flow_flags)
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
				is_rep, free_allocs, flow_flags);
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
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_eth_sec_sess *eth_sec = NULL;
	uint32_t flow_flags = 0;

	if (actions[0].type == RTE_FLOW_ACTION_TYPE_SECURITY) {
		eth_sec = cnxk_eth_sec_sess_get_by_sess(dev, actions[0].conf);
		if (eth_sec != NULL) {
			flow_flags = eth_sec->inb_oop ? CNXK_FLOW_NON_INPLACE : 0;
			flow_flags |= CNXK_FLOW_NO_SEC_ACTION;
		}
	}

	return cnxk_flow_validate_common(eth_dev, attr, pattern, actions, error, false, flow_flags);
}

struct roc_npc_flow *
cnxk_flow_create_common(struct rte_eth_dev *eth_dev, const struct rte_flow_attr *attr,
			const struct rte_flow_item pattern[],
			const struct rte_flow_action actions[], struct rte_flow_error *error,
			bool is_rep, uint32_t flow_flags)
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
				&npc_default_action, is_rep, free_allocs, flow_flags);
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
	struct rte_flow_query_count *query = data;
	struct roc_npc_flow *in_flow;
	struct cnxk_rep_dev *rep_dev;
	const char *errmsg = NULL;
	struct cnxk_eth_dev *dev;
	int errcode = ENOTSUP;
	struct roc_npc *npc;
	int rc;

	if (flow == NULL) {
		errmsg = "Invalid flow handle";
		errcode = EINVAL;
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

	/* Let the roc layer resolve async/template handles to the underlying
	 * roc_npc_flow (NULL until committed); else treat as a sync flow.
	 */
	in_flow = (struct roc_npc_flow *)flow;
	if (!is_rep)
		roc_npc_async_flow_resolve(npc, flow, &in_flow);

	if (in_flow == NULL) {
		errmsg = "Flow is not committed yet";
		errcode = EINVAL;
		goto err_exit;
	}

	if (action->type != RTE_FLOW_ACTION_TYPE_COUNT) {
		errmsg = "Only COUNT is supported in query";
		goto err_exit;
	}

	if (in_flow->ctr_id == NPC_COUNTER_NONE) {
		errmsg = "Counter is not available";
		goto err_exit;
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

	rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			   "Flow isolation not supported");

	return -rte_errno;
}

int
cnxk_flow_dev_dump_common(struct rte_eth_dev *eth_dev, struct rte_flow *flow, FILE *file,
			  struct rte_flow_error *error, bool is_rep)
{
	struct roc_npc_flow *in_flow = NULL;
	struct cnxk_eth_dev *dev = NULL;
	struct cnxk_rep_dev *rep_dev;
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
		/* Resolve async/template handles to the underlying roc_npc_flow;
		 * else treat as a sync flow.
		 */
		if (is_rep || !roc_npc_async_flow_resolve(npc, flow, &in_flow))
			in_flow = (struct roc_npc_flow *)flow;

		if (in_flow == NULL)
			return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
						  "flow is not committed yet");

		roc_npc_flow_mcam_dump(file, npc, in_flow);
		return 0;
	}

	roc_npc_flow_dump(file, npc, -1);

	return 0;
}
/* Configure async flow queues for the port. */
int
cnxk_flow_configure(struct rte_eth_dev *eth_dev, const struct rte_flow_port_attr *port_attr,
		    uint16_t nb_queue, const struct rte_flow_queue_attr *queue_attr[],
		    struct rte_flow_error *err)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	const struct roc_npc_flow_queue_attr **rptrs = NULL;
	struct roc_npc_flow_queue_attr *rattr = NULL;
	struct roc_npc *npc = &dev->npc;
	uint16_t i;
	int rc;

	RTE_SET_USED(port_attr);

	/* Translate the rte queue attributes into roc_npc types; the roc
	 * engine validates nb_queue == 0 and a NULL queue_attr.
	 */
	if (nb_queue != 0 && queue_attr != NULL) {
		rattr = plt_zmalloc(nb_queue * sizeof(*rattr), 0);
		if (rattr == NULL)
			return rte_flow_error_set(err, ENOMEM, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						  NULL, "Failed to allocate flow queues");

		rptrs = plt_zmalloc(nb_queue * sizeof(*rptrs), 0);
		if (rptrs == NULL) {
			plt_free(rattr);
			return rte_flow_error_set(err, ENOMEM, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						  NULL, "Failed to allocate flow queues");
		}
		for (i = 0; i < nb_queue; i++) {
			if (queue_attr[i] != NULL) {
				rattr[i].size = queue_attr[i]->size;
				rptrs[i] = &rattr[i];
			} else {
				rptrs[i] = NULL;
			}
		}
	}

	rc = roc_npc_flow_configure(npc, nb_queue, rptrs);

	plt_free(rattr);
	plt_free(rptrs);

	if (rc)
		return rte_flow_error_set(err, -rc, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Failed to configure flow queues");
	return 0;
}

/* Deep-copy a spec/last/mask/conf payload into template-owned memory. */
static int
cnxk_flow_dup_conf(void **copies, uint16_t *nb_copies, const void *src, size_t sz, const void **dst)
{
	void *copy;

	if (src == NULL || sz == 0) {
		*dst = NULL;
		return 0;
	}
	copy = plt_zmalloc(sz, 0);
	if (copy == NULL)
		return -ENOMEM;
	memcpy(copy, src, sz);
	copies[(*nb_copies)++] = copy;
	*dst = copy;
	return 0;
}

struct rte_flow_pattern_template *
cnxk_flow_pattern_template_create(struct rte_eth_dev *eth_dev,
				  const struct rte_flow_pattern_template_attr *attr,
				  const struct rte_flow_item pattern[],
				  struct rte_flow_error *error)
{
	const char *errmsg = "Failed to allocate pattern template";
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_flow_pattern_template *tmpl = NULL;
	struct roc_npc_pattern_template_attr roc_attr;
	struct roc_npc_item_info *roc_pattern = NULL;
	int i, nb_items = 0, errcode = ENOMEM;
	struct roc_npc *npc = &dev->npc;
	int roc_err;

	if (attr == NULL) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "NULL template attr");
		return NULL;
	}

	if (pattern == NULL) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "NULL pattern");
		return NULL;
	}

	while (pattern[nb_items].type != RTE_FLOW_ITEM_TYPE_END)
		nb_items++;

	/* Reject oversized templates; rules are built into fixed-size stack
	 * arrays (combined_items[ROC_NPC_ITEM_TYPE_END + 1]).
	 */
	if (nb_items > ROC_NPC_ITEM_TYPE_END) {
		errcode = ENOTSUP;
		errmsg = "too many pattern items for hardware";
		goto err_nomem;
	}

	tmpl = plt_zmalloc(sizeof(*tmpl), 0);
	if (tmpl == NULL)
		goto err_nomem;

	tmpl->attr = *attr;
	tmpl->nb_items = nb_items;

	tmpl->pattern = plt_zmalloc((nb_items + 1) * sizeof(struct rte_flow_item), 0);
	if (tmpl->pattern == NULL)
		goto err_free;

	/* At most 3 payload copies (spec/last/mask) per item. */
	tmpl->copies = plt_zmalloc(3 * (nb_items + 1) * sizeof(void *), 0);
	if (tmpl->copies == NULL)
		goto err_free;

	for (i = 0; i < nb_items; i++) {
		enum rte_flow_item_type t = pattern[i].type;
		size_t sz = (t < (int)RTE_DIM(term)) ? term[t].item_size : 0;

		/* Reject unrepresentable item types (sz 0), which would drop
		 * spec/last/mask. VOID/ANY legitimately carry no payload.
		 */
		if (t != RTE_FLOW_ITEM_TYPE_VOID && t != RTE_FLOW_ITEM_TYPE_ANY && sz == 0) {
			errcode = ENOTSUP;
			errmsg = "unsupported pattern item type in template";
			goto err_free;
		}

		/* Reject items with nested variable-length payloads; a flat
		 * memcpy would retain a dangling pointer into caller memory.
		 */
		if (t == RTE_FLOW_ITEM_TYPE_RAW) {
			errcode = ENOTSUP;
			errmsg = "Item type with nested pointers is not supported "
				 "in pattern template (e.g. RAW)";
			goto err_free;
		}

		tmpl->pattern[i].type = t;
		if (cnxk_flow_dup_conf(tmpl->copies, &tmpl->nb_copies, pattern[i].spec, sz,
				       &tmpl->pattern[i].spec) ||
		    cnxk_flow_dup_conf(tmpl->copies, &tmpl->nb_copies, pattern[i].last, sz,
				       &tmpl->pattern[i].last) ||
		    cnxk_flow_dup_conf(tmpl->copies, &tmpl->nb_copies, pattern[i].mask, sz,
				       &tmpl->pattern[i].mask))
			goto err_free;
	}
	tmpl->pattern[nb_items].type = RTE_FLOW_ITEM_TYPE_END;
	tmpl->roc_tmpl = NULL;
	tmpl->refcnt = 0;

	roc_pattern = plt_zmalloc((nb_items + 1) * sizeof(*roc_pattern), 0);
	if (roc_pattern == NULL)
		goto err_free;

	for (i = 0; i < nb_items; i++) {
		enum rte_flow_item_type t = tmpl->pattern[i].type;
		size_t sz = (t < (int)RTE_DIM(term)) ? term[t].item_size : 0;

		roc_pattern[i].type =
			(t < (int)RTE_DIM(term)) ? term[t].item_type : ROC_NPC_ITEM_TYPE_VOID;
		roc_pattern[i].size = sz;
		roc_pattern[i].spec = tmpl->pattern[i].spec;
		roc_pattern[i].last = tmpl->pattern[i].last;
		roc_pattern[i].mask = tmpl->pattern[i].mask;
	}
	roc_pattern[nb_items].type = ROC_NPC_ITEM_TYPE_END;

	memset(&roc_attr, 0, sizeof(roc_attr));
	roc_attr.relaxed_matching = attr->relaxed_matching;
	roc_attr.ingress = attr->ingress;
	roc_attr.egress = attr->egress;
	roc_attr.transfer = attr->transfer;

	roc_err = 0;
	tmpl->roc_tmpl = roc_npc_pattern_template_create(npc, &roc_attr, roc_pattern, &roc_err);
	if (tmpl->roc_tmpl == NULL) {
		errcode = (roc_err < 0) ? -roc_err : ENOMEM;
		errmsg = "Failed to create ROC pattern template";
		goto err_free;
	}

	plt_free(roc_pattern);

	return (struct rte_flow_pattern_template *)tmpl;

err_free:
	plt_free(roc_pattern);
	if (tmpl != NULL) {
		uint16_t j;
		if (tmpl->roc_tmpl != NULL)
			roc_npc_pattern_template_destroy(npc, tmpl->roc_tmpl);
		for (j = 0; j < tmpl->nb_copies; j++)
			plt_free(tmpl->copies[j]);
		plt_free(tmpl->copies);
		plt_free(tmpl->pattern);
		plt_free(tmpl);
	}
err_nomem:
	rte_flow_error_set(error, errcode, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL, errmsg);
	return NULL;
}

int
cnxk_flow_pattern_template_destroy(struct rte_eth_dev *eth_dev,
				   struct rte_flow_pattern_template *templ,
				   struct rte_flow_error *error)
{
	struct cnxk_flow_pattern_template *tmpl = (struct cnxk_flow_pattern_template *)templ;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_npc *npc = &dev->npc;
	uint16_t i;
	int rc;

	if (tmpl == NULL)
		return 0;

	if (tmpl->refcnt != 0)
		return rte_flow_error_set(error, EBUSY, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "pattern template still in use by a table");

	rc = roc_npc_pattern_template_destroy(npc, tmpl->roc_tmpl);
	if (rc)
		return rte_flow_error_set(error, -rc, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "failed to destroy ROC pattern template");

	for (i = 0; i < tmpl->nb_copies; i++)
		plt_free(tmpl->copies[i]);
	plt_free(tmpl->copies);
	plt_free(tmpl->pattern);
	plt_free(tmpl);
	return 0;
}
/* Size of an action's conf for the v1-supported set; -1 = unsupported. */
static int
cnxk_flow_action_conf_size(enum rte_flow_action_type type)
{
	switch (type) {
	case RTE_FLOW_ACTION_TYPE_VOID:
	case RTE_FLOW_ACTION_TYPE_DROP:
	case RTE_FLOW_ACTION_TYPE_PF:
	case RTE_FLOW_ACTION_TYPE_FLAG:
	case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
	case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
		return 0; /* no conf */
	case RTE_FLOW_ACTION_TYPE_QUEUE:
		return sizeof(struct rte_flow_action_queue);
	case RTE_FLOW_ACTION_TYPE_VF:
		return sizeof(struct rte_flow_action_vf);
	case RTE_FLOW_ACTION_TYPE_MARK:
		return sizeof(struct rte_flow_action_mark);
	case RTE_FLOW_ACTION_TYPE_COUNT:
		return sizeof(struct rte_flow_action_count);
	case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
		return sizeof(struct rte_flow_action_of_push_vlan);
	case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
		return sizeof(struct rte_flow_action_of_set_vlan_vid);
	case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP:
		return sizeof(struct rte_flow_action_of_set_vlan_pcp);
	case RTE_FLOW_ACTION_TYPE_AGE:
		return sizeof(struct rte_flow_action_age);
	case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
	case RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR:
		return sizeof(struct rte_flow_action_ethdev);
	case RTE_FLOW_ACTION_TYPE_PORT_ID:
		return sizeof(struct rte_flow_action_port_id);
	default:
		return -1;
	}
}

/* Deep-copy an action conf. RSS nests queue[]/key[] pointers, so pack them
 * into one blob and fix the pointers; other confs use a flat memcpy.
 */
static void *
cnxk_flow_dup_action_conf(const struct rte_flow_action *act)
{
	if (act->conf == NULL)
		return NULL;

	if (act->type == RTE_FLOW_ACTION_TYPE_RSS) {
		const struct rte_flow_action_rss *rss = act->conf;
		size_t rss_blob_bytes = sizeof(*rss);
		size_t rss_queue_bytes = (size_t)rss->queue_num * sizeof(uint16_t);
		size_t rss_key_bytes = rss->key_len;
		struct rte_flow_action_rss *rss_copy;
		uint8_t *rss_blob;

		/* Reject a non-zero length paired with a NULL pointer, which
		 * would make the memcpy below dereference NULL.
		 */
		if ((rss_queue_bytes && rss->queue == NULL) || (rss_key_bytes && rss->key == NULL))
			return NULL;

		rss_blob = plt_zmalloc(rss_blob_bytes + rss_queue_bytes + rss_key_bytes, 0);
		if (rss_blob == NULL)
			return NULL;
		rss_copy = (struct rte_flow_action_rss *)rss_blob;
		*rss_copy = *rss;
		if (rss_queue_bytes) {
			memcpy(rss_blob + rss_blob_bytes, rss->queue, rss_queue_bytes);
			rss_copy->queue = (const uint16_t *)(rss_blob + rss_blob_bytes);
		} else {
			rss_copy->queue = NULL;
		}
		if (rss_key_bytes) {
			memcpy(rss_blob + rss_blob_bytes + rss_queue_bytes, rss->key,
			       rss_key_bytes);
			rss_copy->key =
				(const uint8_t *)(rss_blob + rss_blob_bytes + rss_queue_bytes);
		} else {
			rss_copy->key = NULL;
		}
		return rss_copy;
	}

	{
		int conf_size = cnxk_flow_action_conf_size(act->type);
		void *copy;

		if (conf_size <= 0)
			return NULL;
		copy = plt_zmalloc(conf_size, 0);
		if (copy != NULL)
			memcpy(copy, act->conf, conf_size);
		return copy;
	}
}

/* Map a supported rte_flow action type to its roc_npc action type, to mirror
 * the actions template into the roc engine for lifecycle/refcount ownership.
 */
static int
cnxk_flow_roc_action_type(enum rte_flow_action_type type, enum roc_npc_action_type *roc_type)
{
	switch (type) {
	case RTE_FLOW_ACTION_TYPE_VOID:
	case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
		*roc_type = ROC_NPC_ACTION_TYPE_VOID;
		return 0;
	case RTE_FLOW_ACTION_TYPE_DROP:
		*roc_type = ROC_NPC_ACTION_TYPE_DROP;
		return 0;
	case RTE_FLOW_ACTION_TYPE_PF:
		*roc_type = ROC_NPC_ACTION_TYPE_PF;
		return 0;
	case RTE_FLOW_ACTION_TYPE_VF:
		*roc_type = ROC_NPC_ACTION_TYPE_VF;
		return 0;
	case RTE_FLOW_ACTION_TYPE_FLAG:
		*roc_type = ROC_NPC_ACTION_TYPE_FLAG;
		return 0;
	case RTE_FLOW_ACTION_TYPE_MARK:
		*roc_type = ROC_NPC_ACTION_TYPE_MARK;
		return 0;
	case RTE_FLOW_ACTION_TYPE_COUNT:
		*roc_type = ROC_NPC_ACTION_TYPE_COUNT;
		return 0;
	case RTE_FLOW_ACTION_TYPE_QUEUE:
		*roc_type = ROC_NPC_ACTION_TYPE_QUEUE;
		return 0;
	case RTE_FLOW_ACTION_TYPE_RSS:
		*roc_type = ROC_NPC_ACTION_TYPE_RSS;
		return 0;
	case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
		*roc_type = ROC_NPC_ACTION_TYPE_VLAN_STRIP;
		return 0;
	case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
		*roc_type = ROC_NPC_ACTION_TYPE_VLAN_ETHTYPE_INSERT;
		return 0;
	case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
		*roc_type = ROC_NPC_ACTION_TYPE_VLAN_INSERT;
		return 0;
	case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP:
		*roc_type = ROC_NPC_ACTION_TYPE_VLAN_PCP_INSERT;
		return 0;
	case RTE_FLOW_ACTION_TYPE_AGE:
		*roc_type = ROC_NPC_ACTION_TYPE_AGE;
		return 0;
	case RTE_FLOW_ACTION_TYPE_PORT_ID:
	case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
	case RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR:
		*roc_type = ROC_NPC_ACTION_TYPE_PORT_ID;
		return 0;
	default:
		return -1;
	}
}

struct rte_flow_actions_template *
cnxk_flow_actions_template_create(struct rte_eth_dev *eth_dev,
				  const struct rte_flow_actions_template_attr *attr,
				  const struct rte_flow_action actions[],
				  const struct rte_flow_action masks[],
				  struct rte_flow_error *error)
{
	const char *errmsg = "Failed to allocate actions template";
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_flow_actions_template *tmpl = NULL;
	struct roc_npc_actions_template_attr roc_attr;
	struct roc_npc_action *roc_actions = NULL;
	int i, nb_actions = 0, errcode = ENOMEM;
	struct roc_npc *npc = &dev->npc;
	int roc_err = 0;

	if (attr == NULL) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "NULL template attr");
		return NULL;
	}

	if (actions == NULL) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "NULL actions");
		return NULL;
	}

	while (actions[nb_actions].type != RTE_FLOW_ACTION_TYPE_END)
		nb_actions++;

	/* Reject oversized templates; rules are built into fixed-size stack
	 * arrays (combined_actions[ROC_NPC_MAX_ACTION_COUNT] plus END).
	 */
	if (nb_actions > ROC_NPC_MAX_ACTION_COUNT - 1) {
		errcode = ENOTSUP;
		errmsg = "too many actions for hardware";
		goto err;
	}

	tmpl = plt_zmalloc(sizeof(*tmpl), 0);
	if (tmpl == NULL)
		goto err;

	tmpl->attr = *attr;
	tmpl->nb_actions = nb_actions;

	tmpl->actions = plt_zmalloc((nb_actions + 1) * sizeof(struct rte_flow_action), 0);
	if (tmpl->actions == NULL)
		goto err;

	tmpl->masks = plt_zmalloc((nb_actions + 1) * sizeof(struct rte_flow_action), 0);
	if (tmpl->masks == NULL)
		goto err;

	/* up to 2 conf copies (action + mask) per action */
	tmpl->copies = plt_zmalloc(2 * (nb_actions + 1) * sizeof(void *), 0);
	if (tmpl->copies == NULL)
		goto err;

	for (i = 0; i < nb_actions; i++) {
		enum rte_flow_action_type t = actions[i].type;
		int sz = cnxk_flow_action_conf_size(t);

		if (sz < 0 && t != RTE_FLOW_ACTION_TYPE_RSS) {
			errcode = ENOTSUP;
			errmsg = "Unsupported action in template "
				 "(supports VOID/DROP/QUEUE/PF/VF/MARK/FLAG/COUNT/RSS/"
				 "OF_POP_VLAN/OF_PUSH_VLAN/OF_SET_VLAN_VID/"
				 "OF_SET_VLAN_PCP/AGE/REPRESENTED_PORT/"
				 "PORT_REPRESENTOR/PORT_ID/VXLAN_DECAP)";
			goto err;
		}

		tmpl->actions[i].type = t;

		if (t == RTE_FLOW_ACTION_TYPE_RSS) {
			/* RSS has nested pointers — use deep-copy helper. */
			void *copy = cnxk_flow_dup_action_conf(&actions[i]);

			if (copy == NULL && actions[i].conf != NULL) {
				const struct rte_flow_action_rss *rss = actions[i].conf;

				/* Malformed conf vs allocation failure: report a
				 * precise errno instead of the default ENOMEM.
				 */
				if ((rss->queue_num && rss->queue == NULL) ||
				    (rss->key_len && rss->key == NULL)) {
					errcode = EINVAL;
					errmsg = "Malformed RSS action conf in template";
				}
				goto err;
			}
			tmpl->actions[i].conf = copy;
			if (copy)
				tmpl->copies[tmpl->nb_copies++] = copy;
		} else {
			if (cnxk_flow_dup_conf(tmpl->copies, &tmpl->nb_copies, actions[i].conf, sz,
					       &tmpl->actions[i].conf))
				goto err;
		}

		if (masks != NULL) {
			if (masks[i].type != t) {
				errcode = EINVAL;
				errmsg = "actions/masks type mismatch in template";
				goto err;
			}
			tmpl->masks[i].type = t;
			if (t == RTE_FLOW_ACTION_TYPE_RSS) {
				/* RSS nests pointers and cannot be copied by the flat
				 * helper; reject a non-NULL RSS mask.
				 */
				if (masks[i].conf != NULL) {
					errcode = ENOTSUP;
					errmsg = "Non-NULL RSS mask conf is not "
						 "supported in template";
					goto err;
				}
				tmpl->masks[i].conf = NULL;
			} else if (cnxk_flow_dup_conf(tmpl->copies, &tmpl->nb_copies, masks[i].conf,
						      sz > 0 ? sz : 0, &tmpl->masks[i].conf)) {
				goto err;
			}
		}
	}

	tmpl->actions[nb_actions].type = RTE_FLOW_ACTION_TYPE_END;
	tmpl->masks[nb_actions].type = RTE_FLOW_ACTION_TYPE_END;
	tmpl->roc_tmpl = NULL;
	tmpl->refcnt = 0;

	/* Mirror only action types into the roc engine for template lifecycle
	 * and refcount ownership; per-rule conf translation stays in cnxk.
	 */
	roc_actions = plt_zmalloc((nb_actions + 1) * sizeof(*roc_actions), 0);
	if (roc_actions == NULL)
		goto err;

	for (i = 0; i < nb_actions; i++) {
		enum roc_npc_action_type rt;

		if (cnxk_flow_roc_action_type(tmpl->actions[i].type, &rt)) {
			errcode = ENOTSUP;
			errmsg = "Unsupported action in template";
			goto err;
		}
		roc_actions[i].type = rt;
		roc_actions[i].conf = NULL;
	}
	roc_actions[nb_actions].type = ROC_NPC_ACTION_TYPE_END;

	memset(&roc_attr, 0, sizeof(roc_attr));
	roc_attr.ingress = attr->ingress;
	roc_attr.egress = attr->egress;
	roc_attr.transfer = attr->transfer;

	tmpl->roc_tmpl =
		roc_npc_actions_template_create(npc, &roc_attr, roc_actions, NULL, &roc_err);
	plt_free(roc_actions);
	roc_actions = NULL;
	if (tmpl->roc_tmpl == NULL) {
		errcode = (roc_err < 0) ? -roc_err : ENOMEM;
		errmsg = "Failed to create ROC actions template";
		goto err;
	}

	return (struct rte_flow_actions_template *)tmpl;

err:
	plt_free(roc_actions);
	if (tmpl != NULL) {
		uint16_t j;
		if (tmpl->roc_tmpl != NULL)
			roc_npc_actions_template_destroy(npc, tmpl->roc_tmpl);
		for (j = 0; j < tmpl->nb_copies; j++)
			plt_free(tmpl->copies[j]);
		plt_free(tmpl->copies);
		plt_free(tmpl->masks);
		plt_free(tmpl->actions);
		plt_free(tmpl);
	}
	rte_flow_error_set(error, errcode, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL, errmsg);
	return NULL;
}

int
cnxk_flow_actions_template_destroy(struct rte_eth_dev *eth_dev,
				   struct rte_flow_actions_template *templ,
				   struct rte_flow_error *error)
{
	struct cnxk_flow_actions_template *tmpl = (struct cnxk_flow_actions_template *)templ;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_npc *npc = &dev->npc;
	uint16_t i;
	int rc;

	if (tmpl == NULL)
		return 0;

	if (tmpl->refcnt != 0)
		return rte_flow_error_set(error, EBUSY, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "actions template still in use by a table");

	rc = roc_npc_actions_template_destroy(npc, tmpl->roc_tmpl);
	if (rc)
		return rte_flow_error_set(error, -rc, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "failed to destroy ROC actions template");

	for (i = 0; i < tmpl->nb_copies; i++)
		plt_free(tmpl->copies[i]);
	plt_free(tmpl->copies);
	plt_free(tmpl->masks);
	plt_free(tmpl->actions);
	plt_free(tmpl);
	return 0;
}

struct rte_flow_template_table *
cnxk_flow_template_table_create(struct rte_eth_dev *eth_dev,
				const struct rte_flow_template_table_attr *table_attr,
				struct rte_flow_pattern_template *pattern_templates[],
				uint8_t nb_pattern_templates,
				struct rte_flow_actions_template *actions_templates[],
				uint8_t nb_actions_templates, struct rte_flow_error *error)
{
	struct roc_npc_pattern_template **roc_pattern_templates = NULL;
	struct roc_npc_actions_template **roc_actions_templates = NULL;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_npc_template_table_attr roc_attr;
	enum rte_flow_table_insertion_type itype;
	const struct rte_flow_attr *fattr;
	struct roc_npc *npc = &dev->npc;
	struct cnxk_flow_table *table;
	uint32_t nb_flows;
	int errcode = 0;
	uint8_t i;

	if (table_attr == NULL) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "NULL table_attr");
		return NULL;
	}

	itype = table_attr->insertion_type;
	fattr = &table_attr->flow_attr;
	nb_flows = table_attr->nb_flows;

	if (itype == RTE_FLOW_TABLE_INSERTION_TYPE_INDEX) {
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "pure-index insertion not supported; NPC is match-based");
		return NULL;
	}

	/* Validate the template handles before allocating anything. */
	for (i = 0; i < nb_pattern_templates; i++) {
		if (pattern_templates[i] == NULL) {
			rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					   "invalid template handle");
			return NULL;
		}
	}
	for (i = 0; i < nb_actions_templates; i++) {
		if (actions_templates[i] == NULL) {
			rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					   "invalid template handle");
			return NULL;
		}
	}

	table = plt_zmalloc(sizeof(*table), 0);
	if (table == NULL)
		goto enomem;

	table->insertion_type = itype;
	table->nb_flows = nb_flows;

	memset(&table->flow_attr, 0, sizeof(table->flow_attr));
	table->flow_attr.priority = fattr->priority;
	table->flow_attr.ingress = fattr->ingress;
	table->flow_attr.egress = fattr->egress;
	table->transfer = fattr->transfer;

	if (nb_pattern_templates != 0) {
		table->pattern_templates =
			plt_zmalloc(nb_pattern_templates * sizeof(*table->pattern_templates), 0);
		if (table->pattern_templates == NULL)
			goto free_table;
		roc_pattern_templates =
			plt_zmalloc(nb_pattern_templates * sizeof(*roc_pattern_templates), 0);
		if (roc_pattern_templates == NULL)
			goto free_table;
	}
	if (nb_actions_templates != 0) {
		table->actions_templates =
			plt_zmalloc(nb_actions_templates * sizeof(*table->actions_templates), 0);
		if (table->actions_templates == NULL)
			goto free_table;
		roc_actions_templates =
			plt_zmalloc(nb_actions_templates * sizeof(*roc_actions_templates), 0);
		if (roc_actions_templates == NULL)
			goto free_table;
	}

	/* Hand the table to the roc engine, which owns rule storage, MCAM
	 * reservation and the rule lifecycle; cnxk keeps per-rule translation.
	 */
	memset(&roc_attr, 0, sizeof(roc_attr));
	roc_attr.flow_attr = table->flow_attr;
	roc_attr.transfer = table->transfer;
	roc_attr.nb_flows = nb_flows;
	roc_attr.insertion_type = (itype == RTE_FLOW_TABLE_INSERTION_TYPE_INDEX_WITH_PATTERN) ?
					  ROC_NPC_TEMPLATE_INSERTION_INDEX_WITH_PATTERN :
					  ROC_NPC_TEMPLATE_INSERTION_PATTERN;
	/* Stash this wrapper as the table cookie so async_actions_update can
	 * recover the rte templates from just the flow handle.
	 */
	roc_attr.cookie = table;

	for (i = 0; i < nb_pattern_templates; i++) {
		table->pattern_templates[i] =
			(struct cnxk_flow_pattern_template *)pattern_templates[i];
		roc_pattern_templates[i] = table->pattern_templates[i]->roc_tmpl;
		if (roc_pattern_templates[i] == NULL)
			goto bad_handle;
	}
	for (i = 0; i < nb_actions_templates; i++) {
		table->actions_templates[i] =
			(struct cnxk_flow_actions_template *)actions_templates[i];
		roc_actions_templates[i] = table->actions_templates[i]->roc_tmpl;
		if (roc_actions_templates[i] == NULL)
			goto bad_handle;
	}

	/* clang-format off */
	table->roc_table = roc_npc_template_table_create(npc, &roc_attr,
			roc_pattern_templates, nb_pattern_templates,
			roc_actions_templates, nb_actions_templates, &errcode);
	/* clang-format on */
	plt_free(roc_pattern_templates);
	plt_free(roc_actions_templates);
	roc_pattern_templates = NULL;
	roc_actions_templates = NULL;
	if (table->roc_table == NULL) {
		plt_free(table->pattern_templates);
		plt_free(table->actions_templates);
		plt_free(table);
		rte_flow_error_set(error, errcode < 0 ? -errcode : ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "could not create template table");
		return NULL;
	}

	for (i = 0; i < nb_pattern_templates; i++)
		table->pattern_templates[i]->refcnt++;
	table->nb_pattern_templates = nb_pattern_templates;
	for (i = 0; i < nb_actions_templates; i++)
		table->actions_templates[i]->refcnt++;
	table->nb_actions_templates = nb_actions_templates;

	return (struct rte_flow_template_table *)table;

bad_handle:
	plt_free(roc_pattern_templates);
	plt_free(roc_actions_templates);
	plt_free(table->pattern_templates);
	plt_free(table->actions_templates);
	plt_free(table);
	rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			   "invalid template handle");
	return NULL;

free_table:
	plt_free(roc_pattern_templates);
	plt_free(roc_actions_templates);
	plt_free(table->pattern_templates);
	plt_free(table->actions_templates);
	plt_free(table);

enomem:
	rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL, "out of memory");
	return NULL;
}

int
cnxk_flow_template_table_destroy(struct rte_eth_dev *eth_dev,
				 struct rte_flow_template_table *tbl_handle,
				 struct rte_flow_error *error)
{
	struct cnxk_flow_table *table = (struct cnxk_flow_table *)tbl_handle;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_npc *npc = &dev->npc;
	uint8_t i;
	int rc;

	if (table == NULL)
		return 0;

	/* The roc engine guards teardown: a busy table returns -EBUSY and
	 * nothing is freed, so the cnxk wrapper and refcounts stay intact.
	 */
	rc = roc_npc_template_table_destroy(npc, table->roc_table);
	if (rc == -EBUSY)
		return rte_flow_error_set(error, EBUSY, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "table still has live flows; destroy them first");
	if (rc)
		return rte_flow_error_set(error, -rc, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "failed to destroy template table");

	for (i = 0; i < table->nb_pattern_templates; i++)
		table->pattern_templates[i]->refcnt--;
	for (i = 0; i < table->nb_actions_templates; i++)
		table->actions_templates[i]->refcnt--;

	plt_free(table->pattern_templates);
	plt_free(table->actions_templates);
	plt_free(table);
	return 0;
}

/* Reject per-rule pattern/actions arrays whose types/order do not mirror the
 * chosen templates; the merge takes the type from the template but the
 * spec/conf from the per-rule array. A NULL array uses template defaults.
 */
static int
cnxk_flow_check_items(const struct cnxk_flow_pattern_template *pt,
		      const struct rte_flow_item items[])
{
	uint16_t k;

	if (items == NULL)
		return 0;
	for (k = 0; k < pt->nb_items; k++)
		if (items[k].type != pt->pattern[k].type)
			return -EINVAL;
	if (items[pt->nb_items].type != RTE_FLOW_ITEM_TYPE_END)
		return -EINVAL;
	return 0;
}

static int
cnxk_flow_check_actions(const struct cnxk_flow_actions_template *at,
			const struct rte_flow_action actions[])
{
	uint16_t k;

	if (actions == NULL)
		return 0;
	for (k = 0; k < at->nb_actions; k++)
		if (actions[k].type != at->actions[k].type)
			return -EINVAL;
	if (actions[at->nb_actions].type != RTE_FLOW_ACTION_TYPE_END)
		return -EINVAL;
	return 0;
}

/* Enqueue a PATTERN create: merge the per-rule spec with the templates,
 * translate to roc_npc types, and hand to the roc engine (programmed at push).
 */
struct rte_flow *
cnxk_flow_async_create(struct rte_eth_dev *eth_dev, uint32_t queue,
		       const struct rte_flow_op_attr *attr,
		       struct rte_flow_template_table *tbl_handle,
		       const struct rte_flow_item items[], uint8_t pattern_template_index,
		       const struct rte_flow_action actions[], uint8_t action_template_index,
		       void *user_data, struct rte_flow_error *error)
{
	struct cnxk_flow_table *table = (struct cnxk_flow_table *)tbl_handle;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_flow_pattern_template *pt;
	struct cnxk_flow_actions_template *at;
	struct roc_npc_template_flow *flow;
	struct roc_npc *npc = &dev->npc;

	uint64_t free_allocs[ROC_NPC_MAX_ACTION_COUNT + ROC_NPC_ITEM_TYPE_END + 1] = {0};
	struct roc_npc_item_info in_pattern[ROC_NPC_ITEM_TYPE_END + 1] = {0};
	struct rte_flow_action combined_actions[ROC_NPC_MAX_ACTION_COUNT];
	struct roc_npc_action in_actions[ROC_NPC_MAX_ACTION_COUNT] = {0};
	struct rte_flow_item combined_items[ROC_NPC_ITEM_TYPE_END + 1];
	struct roc_npc_action_sample in_sample = {0};
	struct rte_flow_attr rte_attr;
	struct roc_npc_attr in_attr;
	uint16_t dst_pf_func = 0;
	uint64_t def_action = 0;
	int errcode = 0;
	uint16_t k;
	int rc, j;

	RTE_SET_USED(attr);

	if (table == NULL) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "bad table");
		return NULL;
	}
	if (table->insertion_type != RTE_FLOW_TABLE_INSERTION_TYPE_PATTERN) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "table is index-insertion; use create_by_index_with_pattern");
		return NULL;
	}
	if (pattern_template_index >= table->nb_pattern_templates ||
	    action_template_index >= table->nb_actions_templates) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "template index out of range");
		return NULL;
	}

	pt = table->pattern_templates[pattern_template_index];
	at = table->actions_templates[action_template_index];

	if (cnxk_flow_check_items(pt, items) != 0 || cnxk_flow_check_actions(at, actions) != 0) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "pattern/actions do not match the selected templates");
		return NULL;
	}

	/* Merge: spec comes from the per-rule call, last/mask from the template. */
	for (k = 0; k < pt->nb_items; k++) {
		combined_items[k].type = pt->pattern[k].type;
		combined_items[k].spec = items ? items[k].spec : NULL;
		combined_items[k].last = pt->pattern[k].last;
		combined_items[k].mask = pt->pattern[k].mask;
	}
	combined_items[pt->nb_items].type = RTE_FLOW_ITEM_TYPE_END;

	/* Merge: per-rule conf overrides the template conf. */
	for (k = 0; k < at->nb_actions; k++) {
		combined_actions[k].type = at->actions[k].type;
		combined_actions[k].conf =
			(actions && actions[k].conf) ? actions[k].conf : at->actions[k].conf;
	}
	combined_actions[at->nb_actions].type = RTE_FLOW_ACTION_TYPE_END;

	memset(&rte_attr, 0, sizeof(rte_attr));
	rte_attr.priority = table->flow_attr.priority;
	rte_attr.ingress = table->flow_attr.ingress;
	rte_attr.egress = table->flow_attr.egress;
	rte_attr.transfer = table->transfer;

	/* Translate into roc_npc types. The RSS flow key is stashed in
	 * npc->flowkey_cfg_state so the engine can restore it at push().
	 */
	memset(&in_attr, 0, sizeof(in_attr));
	rc = cnxk_map_flow_data(eth_dev, &rte_attr, combined_items, combined_actions, &in_attr,
				in_pattern, in_actions, &in_sample, &npc->flowkey_cfg_state,
				&dst_pf_func, &def_action, false, free_allocs, 0);
	if (rc == 0)
		flow = roc_npc_async_flow_create(npc, queue, table->roc_table, in_pattern,
						 in_actions, dst_pf_func, def_action, user_data,
						 &errcode);
	else
		flow = NULL;

	for (j = 0; j < (int)RTE_DIM(free_allocs) && free_allocs[j]; j++)
		plt_free((void *)free_allocs[j]);

	if (rc != 0) {
		rte_flow_error_set(error, rc < 0 ? -rc : rc, RTE_FLOW_ERROR_TYPE_ACTION_NUM, NULL,
				   "Failed to map flow data");
		return NULL;
	}
	if (flow == NULL) {
		rte_flow_error_set(error, errcode < 0 ? -errcode : EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "async flow create failed");
		return NULL;
	}

	return (struct rte_flow *)flow;
}

/* Enqueue a destroy operation. */
int
cnxk_flow_async_destroy(struct rte_eth_dev *eth_dev, uint32_t queue,
			const struct rte_flow_op_attr *attr, struct rte_flow *flow, void *user_data,
			struct rte_flow_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_npc *npc = &dev->npc;
	int rc;

	RTE_SET_USED(attr);

	rc = roc_npc_async_flow_destroy(npc, queue, (struct roc_npc_template_flow *)flow,
					user_data);
	if (rc)
		return rte_flow_error_set(error, -rc, RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					  "bad queue or flow handle");
	return 0;
}

/* Flush all pending async operations to hardware in one batch. */
int
cnxk_flow_push(struct rte_eth_dev *eth_dev, uint32_t queue, struct rte_flow_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_npc *npc = &dev->npc;
	int rc;

	rc = roc_npc_flow_push(npc, queue);
	if (rc)
		return rte_flow_error_set(error, -rc, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "bad queue");
	return 0;
}

/* Return results for completed ops back to the application. */
int
cnxk_flow_pull(struct rte_eth_dev *eth_dev, uint32_t queue, struct rte_flow_op_result res[],
	       uint16_t n_res, struct rte_flow_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_npc *npc = &dev->npc;
	uint16_t done = 0;

	/* Drain roc engine results in fixed-size batches, translating each
	 * roc op result into the rte op result without a per-pull allocation.
	 */
	while (done < n_res) {
		struct roc_npc_flow_op_result rres[64];
		uint16_t batch = RTE_MIN((uint16_t)(n_res - done), (uint16_t)RTE_DIM(rres));
		int got = roc_npc_flow_pull(npc, queue, rres, batch);
		uint16_t k;

		if (got < 0) {
			if (done != 0)
				break;
			return rte_flow_error_set(error, -got, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						  NULL, "bad queue");
		}

		for (k = 0; k < (uint16_t)got; k++) {
			res[done + k].status =
				(rres[k].rc == 0) ? RTE_FLOW_OP_SUCCESS : RTE_FLOW_OP_ERROR;
			res[done + k].user_data = rres[k].user_data;
		}
		done += (uint16_t)got;

		if (got < batch)
			break;
	}

	return done;
}

struct rte_flow *
cnxk_flow_async_create_by_index(struct rte_eth_dev *eth_dev, uint32_t queue,
				const struct rte_flow_op_attr *attr,
				struct rte_flow_template_table *table, uint32_t rule_index,
				const struct rte_flow_action actions[],
				uint8_t action_template_index, void *user_data,
				struct rte_flow_error *error)
{
	RTE_SET_USED(eth_dev);
	RTE_SET_USED(queue);
	RTE_SET_USED(attr);
	RTE_SET_USED(table);
	RTE_SET_USED(rule_index);
	RTE_SET_USED(actions);
	RTE_SET_USED(action_template_index);
	RTE_SET_USED(user_data);

	rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			   "index-only insertion needs a match key; NPC is match-based");
	return NULL;
}

/* Like async_create but with an app-specified index; the app must avoid
 * concurrent creates to the same index and use a valid table index.
 */
struct rte_flow *
/* clang-format off */
cnxk_flow_async_create_by_index_with_pattern(struct rte_eth_dev *eth_dev,
		uint32_t queue, const struct rte_flow_op_attr *attr,
		struct rte_flow_template_table *tbl_handle, uint32_t rule_index,
		const struct rte_flow_item items[], uint8_t pattern_template_index,
		const struct rte_flow_action actions[],
		uint8_t action_template_index, void *user_data,
		struct rte_flow_error *error)
/* clang-format on */
{
	struct cnxk_flow_table *table = (struct cnxk_flow_table *)tbl_handle;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_flow_pattern_template *pt;
	struct cnxk_flow_actions_template *at;
	struct roc_npc_template_flow *flow;
	struct roc_npc *npc = &dev->npc;

	uint64_t free_allocs[ROC_NPC_MAX_ACTION_COUNT + ROC_NPC_ITEM_TYPE_END + 1] = {0};
	struct roc_npc_item_info in_pattern[ROC_NPC_ITEM_TYPE_END + 1] = {0};
	struct rte_flow_action combined_actions[ROC_NPC_MAX_ACTION_COUNT];
	struct roc_npc_action in_actions[ROC_NPC_MAX_ACTION_COUNT] = {0};
	struct rte_flow_item combined_items[ROC_NPC_ITEM_TYPE_END + 1];
	struct roc_npc_action_sample in_sample = {0};
	struct rte_flow_attr rte_attr;
	struct roc_npc_attr in_attr;
	uint32_t flowkey_cfg = 0;
	uint16_t dst_pf_func = 0;
	uint64_t def_action = 0;
	int errcode = 0;
	uint16_t k;
	int rc, j;

	RTE_SET_USED(attr);

	if (table == NULL) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "bad table");
		return NULL;
	}
	if (table->insertion_type != RTE_FLOW_TABLE_INSERTION_TYPE_INDEX_WITH_PATTERN) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "table is not index-with-pattern insertion");
		return NULL;
	}
	if (pattern_template_index >= table->nb_pattern_templates ||
	    action_template_index >= table->nb_actions_templates) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "template index out of range");
		return NULL;
	}
	if (rule_index >= table->nb_flows) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "rule_index out of range");
		return NULL;
	}

	pt = table->pattern_templates[pattern_template_index];
	at = table->actions_templates[action_template_index];

	if (cnxk_flow_check_items(pt, items) != 0 || cnxk_flow_check_actions(at, actions) != 0) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "pattern/actions do not match the selected templates");
		return NULL;
	}

	for (k = 0; k < pt->nb_items; k++) {
		combined_items[k].type = pt->pattern[k].type;
		combined_items[k].spec = items ? items[k].spec : NULL;
		combined_items[k].last = pt->pattern[k].last;
		combined_items[k].mask = pt->pattern[k].mask;
	}
	combined_items[pt->nb_items].type = RTE_FLOW_ITEM_TYPE_END;

	for (k = 0; k < at->nb_actions; k++) {
		combined_actions[k].type = at->actions[k].type;
		combined_actions[k].conf =
			(actions && actions[k].conf) ? actions[k].conf : at->actions[k].conf;
	}
	combined_actions[at->nb_actions].type = RTE_FLOW_ACTION_TYPE_END;

	memset(&rte_attr, 0, sizeof(rte_attr));
	rte_attr.priority = table->flow_attr.priority;
	rte_attr.ingress = table->flow_attr.ingress;
	rte_attr.egress = table->flow_attr.egress;
	rte_attr.transfer = table->transfer;

	memset(&in_attr, 0, sizeof(in_attr));
	rc = cnxk_map_flow_data(eth_dev, &rte_attr, combined_items, combined_actions, &in_attr,
				in_pattern, in_actions, &in_sample, &flowkey_cfg, &dst_pf_func,
				&def_action, false, free_allocs, 0);
	if (rc == 0)
		/* clang-format off */
		flow = roc_npc_async_flow_create_by_index_with_pattern(npc, queue,
				table->roc_table, rule_index, in_pattern, in_actions,
				user_data, &errcode);
	/* clang-format on */
	else
		flow = NULL;

	for (j = 0; j < (int)RTE_DIM(free_allocs) && free_allocs[j]; j++)
		plt_free((void *)free_allocs[j]);

	if (rc != 0) {
		rte_flow_error_set(error, rc < 0 ? -rc : rc, RTE_FLOW_ERROR_TYPE_ACTION_NUM, NULL,
				   "Failed to map flow data");
		return NULL;
	}
	if (flow == NULL) {
		rte_flow_error_set(error, errcode < 0 ? -errcode : EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "flow parse failed");
		return NULL;
	}

	return (struct rte_flow *)flow;
}

/* Update the actions of an existing flow, keeping key/mask/mcam_id/enable. */
int
cnxk_flow_async_actions_update(struct rte_eth_dev *eth_dev, uint32_t queue,
			       const struct rte_flow_op_attr *attr, struct rte_flow *flow,
			       const struct rte_flow_action actions[],
			       uint8_t action_template_index, void *user_data,
			       struct rte_flow_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_flow_actions_template *at;
	struct roc_npc *npc = &dev->npc;
	struct cnxk_flow_table *table;

	uint64_t free_allocs[ROC_NPC_MAX_ACTION_COUNT + ROC_NPC_ITEM_TYPE_END + 1] = {0};
	struct roc_npc_item_info in_pattern[ROC_NPC_ITEM_TYPE_END + 1] = {0};
	struct rte_flow_action combined_actions[ROC_NPC_MAX_ACTION_COUNT];
	struct roc_npc_action in_actions[ROC_NPC_MAX_ACTION_COUNT] = {0};
	struct roc_npc_action_sample in_sample = {0};
	struct rte_flow_item end_pattern[1];
	struct rte_flow_attr rte_attr;
	struct roc_npc_attr in_attr;
	uint32_t flowkey_cfg = 0;
	uint16_t dst_pf_func = 0;
	uint64_t def_action = 0;
	uint16_t k;
	int rc, j;

	RTE_SET_USED(attr);

	if (flow == NULL)
		return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					  "bad flow handle");

	/* Recover the cnxk table context from the cookie the engine kept for
	 * this rule's table.
	 */
	table = roc_npc_async_flow_table_cookie((struct roc_npc_template_flow *)flow);
	if (table == NULL)
		return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					  "flow handle has no associated table");
	if (action_template_index >= table->nb_actions_templates)
		return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "actions template index out of range");

	at = table->actions_templates[action_template_index];

	if (cnxk_flow_check_actions(at, actions) != 0)
		return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "actions do not match the selected template");

	for (k = 0; k < at->nb_actions; k++) {
		combined_actions[k].type = at->actions[k].type;
		combined_actions[k].conf =
			(actions && actions[k].conf) ? actions[k].conf : at->actions[k].conf;
	}
	combined_actions[at->nb_actions].type = RTE_FLOW_ACTION_TYPE_END;

	end_pattern[0].type = RTE_FLOW_ITEM_TYPE_END;

	memset(&rte_attr, 0, sizeof(rte_attr));
	rte_attr.priority = table->flow_attr.priority;
	rte_attr.ingress = table->flow_attr.ingress;
	rte_attr.egress = table->flow_attr.egress;
	rte_attr.transfer = table->transfer;

	memset(&in_attr, 0, sizeof(in_attr));
	rc = cnxk_map_flow_data(eth_dev, &rte_attr, end_pattern, combined_actions, &in_attr,
				in_pattern, in_actions, &in_sample, &flowkey_cfg, &dst_pf_func,
				&def_action, false, free_allocs, 0);
	if (rc == 0)
		/* clang-format off */
		rc = roc_npc_async_flow_actions_update(npc, queue,
				(struct roc_npc_template_flow *)flow, in_actions, user_data);
	/* clang-format on */

	for (j = 0; j < (int)RTE_DIM(free_allocs) && free_allocs[j]; j++)
		plt_free((void *)free_allocs[j]);

	if (rc != 0)
		return rte_flow_error_set(error, rc < 0 ? -rc : EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL, "failed to update flow actions");

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
