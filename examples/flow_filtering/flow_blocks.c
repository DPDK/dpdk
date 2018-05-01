/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#define MAX_PATTERN_NUM		4

struct rte_flow *
generate_ipv4_flow(uint16_t port_id, uint16_t rx_q,
		uint32_t src_ip, uint32_t src_mask,
		uint32_t dest_ip, uint32_t dest_mask,
		struct rte_flow_error *error);


/**
 * create a flow rule that sends packets with matching src and dest ip
 * to selected queue.
 *
 * @param port_id
 *   The selected port.
 * @param rx_q
 *   The selected target queue.
 * @param src_ip
 *   The src ip value to match the input packet.
 * @param src_mask
 *   The mask to apply to the src ip.
 * @param dest_ip
 *   The dest ip value to match the input packet.
 * @param dest_mask
 *   The mask to apply to the dest ip.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   A flow if the rule could be created else return NULL.
 */
struct rte_flow *
generate_ipv4_flow(uint16_t port_id, uint16_t rx_q,
		uint32_t src_ip, uint32_t src_mask,
		uint32_t dest_ip, uint32_t dest_mask,
		struct rte_flow_error *error)
{
	struct rte_flow_attr attr;
	struct rte_flow_item pattern[MAX_PATTERN_NUM];
	struct rte_flow_action action[MAX_PATTERN_NUM];
	struct rte_flow *flow = NULL;
	struct rte_flow_action_queue queue = { .index = rx_q };
	struct rte_flow_item_eth eth_spec;
	struct rte_flow_item_eth eth_mask;
	struct rte_flow_item_vlan vlan_spec;
	struct rte_flow_item_vlan vlan_mask;
	struct rte_flow_item_ipv4 ip_spec;
	struct rte_flow_item_ipv4 ip_mask;
	int res;

	memset(pattern, 0, sizeof(pattern));
	memset(action, 0, sizeof(action));

	/*
	 * set the rule attribute.
	 * in this case only ingress packets will be checked.
	 */
	memset(&attr, 0, sizeof(struct rte_flow_attr));
	attr.ingress = 1;

	/*
	 * create the action sequence.
	 * one action only,  move packet to queue
	 */

	action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	action[0].conf = &queue;
	action[1].type = RTE_FLOW_ACTION_TYPE_END;

	/*
	 * set the first level of the pattern (eth).
	 * since in this example we just want to get the
	 * ipv4 we set this level to allow all.
	 */
	memset(&eth_spec, 0, sizeof(struct rte_flow_item_eth));
	memset(&eth_mask, 0, sizeof(struct rte_flow_item_eth));
	eth_spec.type = 0;
	eth_mask.type = 0;
	pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[0].spec = &eth_spec;
	pattern[0].mask = &eth_mask;

	/*
	 * setting the second level of the pattern (vlan).
	 * since in this example we just want to get the
	 * ipv4 we also set this level to allow all.
	 */
	memset(&vlan_spec, 0, sizeof(struct rte_flow_item_vlan));
	memset(&vlan_mask, 0, sizeof(struct rte_flow_item_vlan));
	pattern[1].type = RTE_FLOW_ITEM_TYPE_VLAN;
	pattern[1].spec = &vlan_spec;
	pattern[1].mask = &vlan_mask;

	/*
	 * setting the third level of the pattern (ip).
	 * in this example this is the level we care about
	 * so we set it according to the parameters.
	 */
	memset(&ip_spec, 0, sizeof(struct rte_flow_item_ipv4));
	memset(&ip_mask, 0, sizeof(struct rte_flow_item_ipv4));
	ip_spec.hdr.dst_addr = htonl(dest_ip);
	ip_mask.hdr.dst_addr = dest_mask;
	ip_spec.hdr.src_addr = htonl(src_ip);
	ip_mask.hdr.src_addr = src_mask;
	pattern[2].type = RTE_FLOW_ITEM_TYPE_IPV4;
	pattern[2].spec = &ip_spec;
	pattern[2].mask = &ip_mask;

	/* the final level must be always type end */
	pattern[3].type = RTE_FLOW_ITEM_TYPE_END;

	res = rte_flow_validate(port_id, &attr, pattern, action, error);
	if (!res)
		flow = rte_flow_create(port_id, &attr, pattern, action, error);

	return flow;
}

