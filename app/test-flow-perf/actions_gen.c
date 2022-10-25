/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 *
 * The file contains the implementations of actions generators.
 * Each generator is responsible for preparing it's action instance
 * and initializing it with needed data.
 */

#include <stdlib.h>
#include <sys/types.h>
#include <rte_malloc.h>
#include <rte_flow.h>
#include <rte_ethdev.h>
#include <rte_vxlan.h>
#include <rte_gtp.h>
#include <rte_gre.h>
#include <rte_geneve.h>

#include "actions_gen.h"
#include "flow_gen.h"
#include "config.h"


/* Storage for additional parameters for actions */
struct additional_para {
	uint16_t queue;
	uint16_t next_table;
	uint16_t *queues;
	uint16_t queues_number;
	uint32_t counter;
	uint64_t encap_data;
	uint64_t decap_data;
	uint16_t dst_port;
	uint8_t core_idx;
	bool unique_data;
};

/* Storage for struct rte_flow_action_raw_encap including external data. */
struct action_raw_encap_data {
	struct rte_flow_action_raw_encap conf;
	uint8_t data[128];
	uint8_t preserve[128];
	uint16_t idx;
};

/* Storage for struct rte_flow_action_raw_decap including external data. */
struct action_raw_decap_data {
	struct rte_flow_action_raw_decap conf;
	uint8_t data[128];
	uint16_t idx;
};

/* Storage for struct rte_flow_action_rss including external data. */
struct action_rss_data {
	struct rte_flow_action_rss conf;
	uint8_t key[40];
	uint16_t queue[128];
};

static void
add_mark(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static struct rte_flow_action_mark mark_actions[RTE_MAX_LCORE] __rte_cache_aligned;
	uint32_t counter = para.counter;

	do {
		/* Random values from 1 to 256 */
		mark_actions[para.core_idx].id = (counter % 255) + 1;
	} while (0);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_MARK;
	actions[actions_counter].conf = &mark_actions[para.core_idx];
}

static void
add_queue(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static struct rte_flow_action_queue queue_actions[RTE_MAX_LCORE] __rte_cache_aligned;

	do {
		queue_actions[para.core_idx].index = para.queue;
	} while (0);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	actions[actions_counter].conf = &queue_actions[para.core_idx];
}

static void
add_jump(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static struct rte_flow_action_jump jump_action;

	do {
		jump_action.group = para.next_table;
	} while (0);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_JUMP;
	actions[actions_counter].conf = &jump_action;
}

static void
add_rss(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static struct action_rss_data *rss_data[RTE_MAX_LCORE] __rte_cache_aligned;

	uint16_t queue;

	if (rss_data[para.core_idx] == NULL)
		rss_data[para.core_idx] = rte_malloc("rss_data",
			sizeof(struct action_rss_data), 0);

	if (rss_data[para.core_idx] == NULL)
		rte_exit(EXIT_FAILURE, "No Memory available!");

	*rss_data[para.core_idx] = (struct action_rss_data){
		.conf = (struct rte_flow_action_rss){
			.func = RTE_ETH_HASH_FUNCTION_DEFAULT,
			.level = 0,
			.types = GET_RSS_HF(),
			.key_len = sizeof(rss_data[para.core_idx]->key),
			.queue_num = para.queues_number,
			.key = rss_data[para.core_idx]->key,
			.queue = rss_data[para.core_idx]->queue,
		},
		.key = { 1 },
		.queue = { 0 },
	};

	for (queue = 0; queue < para.queues_number; queue++)
		rss_data[para.core_idx]->queue[queue] = para.queues[queue];

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_RSS;
	actions[actions_counter].conf = &rss_data[para.core_idx]->conf;
}

static void
add_set_meta(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_action_set_meta meta_action = {
		.data = RTE_BE32(META_DATA),
		.mask = RTE_BE32(0xffffffff),
	};

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_META;
	actions[actions_counter].conf = &meta_action;
}

static void
add_set_tag(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_action_set_tag tag_action = {
		.data = RTE_BE32(META_DATA),
		.mask = RTE_BE32(0xffffffff),
		.index = TAG_INDEX,
	};

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_TAG;
	actions[actions_counter].conf = &tag_action;
}

static void
add_port_id(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static struct rte_flow_action_port_id port_id = {
		.id = PORT_ID_DST,
	};

	port_id.id = para.dst_port;
	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_PORT_ID;
	actions[actions_counter].conf = &port_id;
}

static void
add_drop(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_DROP;
}

static void
add_count(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_action_count count_action;

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_COUNT;
	actions[actions_counter].conf = &count_action;
}

static void
add_set_src_mac(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static struct rte_flow_action_set_mac set_macs[RTE_MAX_LCORE] __rte_cache_aligned;
	uint32_t mac = para.counter;
	uint16_t i;

	/* Fixed value */
	if (!para.unique_data)
		mac = 1;

	/* Mac address to be set is random each time */
	for (i = 0; i < RTE_ETHER_ADDR_LEN; i++) {
		set_macs[para.core_idx].mac_addr[i] = mac & 0xff;
		mac = mac >> 8;
	}

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_MAC_SRC;
	actions[actions_counter].conf = &set_macs[para.core_idx];
}

static void
add_set_dst_mac(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static struct rte_flow_action_set_mac set_macs[RTE_MAX_LCORE] __rte_cache_aligned;
	uint32_t mac = para.counter;
	uint16_t i;

	/* Fixed value */
	if (!para.unique_data)
		mac = 1;

	/* Mac address to be set is random each time */
	for (i = 0; i < RTE_ETHER_ADDR_LEN; i++) {
		set_macs[para.core_idx].mac_addr[i] = mac & 0xff;
		mac = mac >> 8;
	}

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_MAC_DST;
	actions[actions_counter].conf = &set_macs[para.core_idx];
}

static void
add_set_src_ipv4(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static struct rte_flow_action_set_ipv4 set_ipv4[RTE_MAX_LCORE] __rte_cache_aligned;
	uint32_t ip = para.counter;

	/* Fixed value */
	if (!para.unique_data)
		ip = 1;

	/* IPv4 value to be set is random each time */
	set_ipv4[para.core_idx].ipv4_addr = RTE_BE32(ip + 1);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC;
	actions[actions_counter].conf = &set_ipv4[para.core_idx];
}

static void
add_set_dst_ipv4(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static struct rte_flow_action_set_ipv4 set_ipv4[RTE_MAX_LCORE] __rte_cache_aligned;
	uint32_t ip = para.counter;

	/* Fixed value */
	if (!para.unique_data)
		ip = 1;

	/* IPv4 value to be set is random each time */
	set_ipv4[para.core_idx].ipv4_addr = RTE_BE32(ip + 1);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_IPV4_DST;
	actions[actions_counter].conf = &set_ipv4[para.core_idx];
}

static void
add_set_src_ipv6(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static struct rte_flow_action_set_ipv6 set_ipv6[RTE_MAX_LCORE] __rte_cache_aligned;
	uint32_t ipv6 = para.counter;
	uint8_t i;

	/* Fixed value */
	if (!para.unique_data)
		ipv6 = 1;

	/* IPv6 value to set is random each time */
	for (i = 0; i < 16; i++) {
		set_ipv6[para.core_idx].ipv6_addr[i] = ipv6 & 0xff;
		ipv6 = ipv6 >> 8;
	}

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC;
	actions[actions_counter].conf = &set_ipv6[para.core_idx];
}

static void
add_set_dst_ipv6(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static struct rte_flow_action_set_ipv6 set_ipv6[RTE_MAX_LCORE] __rte_cache_aligned;
	uint32_t ipv6 = para.counter;
	uint8_t i;

	/* Fixed value */
	if (!para.unique_data)
		ipv6 = 1;

	/* IPv6 value to set is random each time */
	for (i = 0; i < 16; i++) {
		set_ipv6[para.core_idx].ipv6_addr[i] = ipv6 & 0xff;
		ipv6 = ipv6 >> 8;
	}

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_IPV6_DST;
	actions[actions_counter].conf = &set_ipv6[para.core_idx];
}

static void
add_set_src_tp(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static struct rte_flow_action_set_tp set_tp[RTE_MAX_LCORE] __rte_cache_aligned;
	uint32_t tp = para.counter;

	/* Fixed value */
	if (!para.unique_data)
		tp = 100;

	/* TP src port is random each time */
	tp = tp % 0xffff;

	set_tp[para.core_idx].port = RTE_BE16(tp & 0xffff);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_TP_SRC;
	actions[actions_counter].conf = &set_tp[para.core_idx];
}

static void
add_set_dst_tp(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static struct rte_flow_action_set_tp set_tp[RTE_MAX_LCORE] __rte_cache_aligned;
	uint32_t tp = para.counter;

	/* Fixed value */
	if (!para.unique_data)
		tp = 100;

	/* TP src port is random each time */
	if (tp > 0xffff)
		tp = tp >> 16;

	set_tp[para.core_idx].port = RTE_BE16(tp & 0xffff);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_TP_DST;
	actions[actions_counter].conf = &set_tp[para.core_idx];
}

static void
add_inc_tcp_ack(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static rte_be32_t value[RTE_MAX_LCORE] __rte_cache_aligned;
	uint32_t ack_value = para.counter;

	/* Fixed value */
	if (!para.unique_data)
		ack_value = 1;

	value[para.core_idx] = RTE_BE32(ack_value);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_INC_TCP_ACK;
	actions[actions_counter].conf = &value[para.core_idx];
}

static void
add_dec_tcp_ack(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static rte_be32_t value[RTE_MAX_LCORE] __rte_cache_aligned;
	uint32_t ack_value = para.counter;

	/* Fixed value */
	if (!para.unique_data)
		ack_value = 1;

	value[para.core_idx] = RTE_BE32(ack_value);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_DEC_TCP_ACK;
	actions[actions_counter].conf = &value[para.core_idx];
}

static void
add_inc_tcp_seq(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static rte_be32_t value[RTE_MAX_LCORE] __rte_cache_aligned;
	uint32_t seq_value = para.counter;

	/* Fixed value */
	if (!para.unique_data)
		seq_value = 1;

	value[para.core_idx] = RTE_BE32(seq_value);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_INC_TCP_SEQ;
	actions[actions_counter].conf = &value[para.core_idx];
}

static void
add_dec_tcp_seq(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static rte_be32_t value[RTE_MAX_LCORE] __rte_cache_aligned;
	uint32_t seq_value = para.counter;

	/* Fixed value */
	if (!para.unique_data)
		seq_value = 1;

	value[para.core_idx] = RTE_BE32(seq_value);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_DEC_TCP_SEQ;
	actions[actions_counter].conf = &value[para.core_idx];
}

static void
add_set_ttl(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static struct rte_flow_action_set_ttl set_ttl[RTE_MAX_LCORE] __rte_cache_aligned;
	uint32_t ttl_value = para.counter;

	/* Fixed value */
	if (!para.unique_data)
		ttl_value = 1;

	/* Set ttl to random value each time */
	ttl_value = ttl_value % 0xff;

	set_ttl[para.core_idx].ttl_value = ttl_value;

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_TTL;
	actions[actions_counter].conf = &set_ttl[para.core_idx];
}

static void
add_dec_ttl(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_DEC_TTL;
}

static void
add_set_ipv4_dscp(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static struct rte_flow_action_set_dscp set_dscp[RTE_MAX_LCORE] __rte_cache_aligned;
	uint32_t dscp_value = para.counter;

	/* Fixed value */
	if (!para.unique_data)
		dscp_value = 1;

	/* Set dscp to random value each time */
	dscp_value = dscp_value % 0xff;

	set_dscp[para.core_idx].dscp = dscp_value;

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_IPV4_DSCP;
	actions[actions_counter].conf = &set_dscp[para.core_idx];
}

static void
add_set_ipv6_dscp(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static struct rte_flow_action_set_dscp set_dscp[RTE_MAX_LCORE] __rte_cache_aligned;
	uint32_t dscp_value = para.counter;

	/* Fixed value */
	if (!para.unique_data)
		dscp_value = 1;

	/* Set dscp to random value each time */
	dscp_value = dscp_value % 0xff;

	set_dscp[para.core_idx].dscp = dscp_value;

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_IPV6_DSCP;
	actions[actions_counter].conf = &set_dscp[para.core_idx];
}

static void
add_flag(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_FLAG;
}

static void
add_ether_header(uint8_t **header, uint64_t data,
	__rte_unused struct additional_para para)
{
	struct rte_ether_hdr eth_hdr;

	if (!(data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_ETH)))
		return;

	memset(&eth_hdr, 0, sizeof(struct rte_ether_hdr));
	if (data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_VLAN))
		eth_hdr.ether_type = RTE_BE16(RTE_ETHER_TYPE_VLAN);
	else if (data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_IPV4))
		eth_hdr.ether_type = RTE_BE16(RTE_ETHER_TYPE_IPV4);
	else if (data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_IPV6))
		eth_hdr.ether_type = RTE_BE16(RTE_ETHER_TYPE_IPV6);
	memcpy(*header, &eth_hdr, sizeof(eth_hdr));
	*header += sizeof(eth_hdr);
}

static void
add_vlan_header(uint8_t **header, uint64_t data,
	__rte_unused struct additional_para para)
{
	struct rte_vlan_hdr vlan_hdr;
	uint16_t vlan_value;

	if (!(data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_VLAN)))
		return;

	vlan_value = VLAN_VALUE;

	memset(&vlan_hdr, 0, sizeof(struct rte_vlan_hdr));
	vlan_hdr.vlan_tci = RTE_BE16(vlan_value);

	if (data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_IPV4))
		vlan_hdr.eth_proto = RTE_BE16(RTE_ETHER_TYPE_IPV4);
	if (data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_IPV6))
		vlan_hdr.eth_proto = RTE_BE16(RTE_ETHER_TYPE_IPV6);
	memcpy(*header, &vlan_hdr, sizeof(vlan_hdr));
	*header += sizeof(vlan_hdr);
}

static void
add_ipv4_header(uint8_t **header, uint64_t data,
	struct additional_para para)
{
	struct rte_ipv4_hdr ipv4_hdr;
	uint32_t ip_dst = para.counter;

	if (!(data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_IPV4)))
		return;

	/* Fixed value */
	if (!para.unique_data)
		ip_dst = 1;

	memset(&ipv4_hdr, 0, sizeof(struct rte_ipv4_hdr));
	ipv4_hdr.src_addr = RTE_IPV4(127, 0, 0, 1);
	ipv4_hdr.dst_addr = RTE_BE32(ip_dst);
	ipv4_hdr.version_ihl = RTE_IPV4_VHL_DEF;
	if (data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_UDP))
		ipv4_hdr.next_proto_id = RTE_IP_TYPE_UDP;
	if (data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_GRE))
		ipv4_hdr.next_proto_id = RTE_IP_TYPE_GRE;
	memcpy(*header, &ipv4_hdr, sizeof(ipv4_hdr));
	*header += sizeof(ipv4_hdr);
}

static void
add_ipv6_header(uint8_t **header, uint64_t data,
	__rte_unused struct additional_para para)
{
	struct rte_ipv6_hdr ipv6_hdr;

	if (!(data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_IPV6)))
		return;

	memset(&ipv6_hdr, 0, sizeof(struct rte_ipv6_hdr));
	if (data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_UDP))
		ipv6_hdr.proto = RTE_IP_TYPE_UDP;
	if (data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_GRE))
		ipv6_hdr.proto = RTE_IP_TYPE_GRE;
	memcpy(*header, &ipv6_hdr, sizeof(ipv6_hdr));
	*header += sizeof(ipv6_hdr);
}

static void
add_udp_header(uint8_t **header, uint64_t data,
	__rte_unused struct additional_para para)
{
	struct rte_udp_hdr udp_hdr;

	if (!(data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_UDP)))
		return;

	memset(&udp_hdr, 0, sizeof(struct rte_flow_item_udp));
	if (data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_VXLAN))
		udp_hdr.dst_port = RTE_BE16(RTE_VXLAN_DEFAULT_PORT);
	if (data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_VXLAN_GPE))
		udp_hdr.dst_port = RTE_BE16(RTE_VXLAN_GPE_UDP_PORT);
	if (data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_GENEVE))
		udp_hdr.dst_port = RTE_BE16(RTE_GENEVE_UDP_PORT);
	if (data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_GTP))
		udp_hdr.dst_port = RTE_BE16(RTE_GTPU_UDP_PORT);
	 memcpy(*header, &udp_hdr, sizeof(udp_hdr));
	 *header += sizeof(udp_hdr);
}

static void
add_vxlan_header(uint8_t **header, uint64_t data,
	struct additional_para para)
{
	struct rte_vxlan_hdr vxlan_hdr;
	uint32_t vni_value = para.counter;

	if (!(data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_VXLAN)))
		return;

	/* Fixed value */
	if (!para.unique_data)
		vni_value = 1;

	memset(&vxlan_hdr, 0, sizeof(struct rte_vxlan_hdr));

	vxlan_hdr.vx_vni = (RTE_BE32(vni_value)) >> 16;
	vxlan_hdr.vx_flags = 0x8;

	memcpy(*header, &vxlan_hdr, sizeof(vxlan_hdr));
	*header += sizeof(vxlan_hdr);
}

static void
add_vxlan_gpe_header(uint8_t **header, uint64_t data,
	struct additional_para para)
{
	struct rte_vxlan_gpe_hdr vxlan_gpe_hdr;
	uint32_t vni_value = para.counter;

	if (!(data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_VXLAN_GPE)))
		return;

	/* Fixed value */
	if (!para.unique_data)
		vni_value = 1;

	memset(&vxlan_gpe_hdr, 0, sizeof(struct rte_vxlan_gpe_hdr));

	vxlan_gpe_hdr.vx_vni = (RTE_BE32(vni_value)) >> 16;
	vxlan_gpe_hdr.vx_flags = 0x0c;

	memcpy(*header, &vxlan_gpe_hdr, sizeof(vxlan_gpe_hdr));
	*header += sizeof(vxlan_gpe_hdr);
}

static void
add_gre_header(uint8_t **header, uint64_t data,
	__rte_unused struct additional_para para)
{
	struct rte_gre_hdr gre_hdr;

	if (!(data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_GRE)))
		return;

	memset(&gre_hdr, 0, sizeof(struct rte_gre_hdr));

	gre_hdr.proto = RTE_BE16(RTE_ETHER_TYPE_TEB);

	memcpy(*header, &gre_hdr, sizeof(gre_hdr));
	*header += sizeof(gre_hdr);
}

static void
add_geneve_header(uint8_t **header, uint64_t data,
	struct additional_para para)
{
	struct rte_geneve_hdr geneve_hdr;
	uint32_t vni_value = para.counter;
	uint8_t i;

	if (!(data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_GENEVE)))
		return;

	/* Fixed value */
	if (!para.unique_data)
		vni_value = 1;

	memset(&geneve_hdr, 0, sizeof(struct rte_geneve_hdr));

	for (i = 0; i < 3; i++)
		geneve_hdr.vni[2 - i] = vni_value >> (i * 8);

	memcpy(*header, &geneve_hdr, sizeof(geneve_hdr));
	*header += sizeof(geneve_hdr);
}

static void
add_gtp_header(uint8_t **header, uint64_t data,
	struct additional_para para)
{
	struct rte_gtp_hdr gtp_hdr;
	uint32_t teid_value = para.counter;

	if (!(data & FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_GTP)))
		return;

	/* Fixed value */
	if (!para.unique_data)
		teid_value = 1;

	memset(&gtp_hdr, 0, sizeof(struct rte_flow_item_gtp));

	gtp_hdr.teid = RTE_BE32(teid_value);
	gtp_hdr.msg_type = 255;

	memcpy(*header, &gtp_hdr, sizeof(gtp_hdr));
	*header += sizeof(gtp_hdr);
}

static const struct encap_decap_headers {
	void (*funct)(
		uint8_t **header,
		uint64_t data,
		struct additional_para para
		);
} headers[] = {
	{.funct = add_ether_header},
	{.funct = add_vlan_header},
	{.funct = add_ipv4_header},
	{.funct = add_ipv6_header},
	{.funct = add_udp_header},
	{.funct = add_vxlan_header},
	{.funct = add_vxlan_gpe_header},
	{.funct = add_gre_header},
	{.funct = add_geneve_header},
	{.funct = add_gtp_header},
};

static void
add_raw_encap(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static struct action_raw_encap_data *action_encap_data[RTE_MAX_LCORE] __rte_cache_aligned;
	uint64_t encap_data = para.encap_data;
	uint8_t *header;
	uint8_t i;

	/* Avoid double allocation. */
	if (action_encap_data[para.core_idx] == NULL)
		action_encap_data[para.core_idx] = rte_malloc("encap_data",
			sizeof(struct action_raw_encap_data), 0);

	/* Check if allocation failed. */
	if (action_encap_data[para.core_idx] == NULL)
		rte_exit(EXIT_FAILURE, "No Memory available!");

	*action_encap_data[para.core_idx] = (struct action_raw_encap_data) {
		.conf = (struct rte_flow_action_raw_encap) {
			.data = action_encap_data[para.core_idx]->data,
		},
			.data = {},
	};
	header = action_encap_data[para.core_idx]->data;

	for (i = 0; i < RTE_DIM(headers); i++)
		headers[i].funct(&header, encap_data, para);

	action_encap_data[para.core_idx]->conf.size = header -
		action_encap_data[para.core_idx]->data;

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_RAW_ENCAP;
	actions[actions_counter].conf = &action_encap_data[para.core_idx]->conf;
}

static void
add_raw_decap(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static struct action_raw_decap_data *action_decap_data[RTE_MAX_LCORE] __rte_cache_aligned;
	uint64_t decap_data = para.decap_data;
	uint8_t *header;
	uint8_t i;

	/* Avoid double allocation. */
	if (action_decap_data[para.core_idx] == NULL)
		action_decap_data[para.core_idx] = rte_malloc("decap_data",
			sizeof(struct action_raw_decap_data), 0);

	/* Check if allocation failed. */
	if (action_decap_data[para.core_idx] == NULL)
		rte_exit(EXIT_FAILURE, "No Memory available!");

	*action_decap_data[para.core_idx] = (struct action_raw_decap_data) {
		.conf = (struct rte_flow_action_raw_decap) {
			.data = action_decap_data[para.core_idx]->data,
		},
			.data = {},
	};
	header = action_decap_data[para.core_idx]->data;

	for (i = 0; i < RTE_DIM(headers); i++)
		headers[i].funct(&header, decap_data, para);

	action_decap_data[para.core_idx]->conf.size = header -
		action_decap_data[para.core_idx]->data;

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_RAW_DECAP;
	actions[actions_counter].conf = &action_decap_data[para.core_idx]->conf;
}

static void
add_vxlan_encap(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_action_vxlan_encap vxlan_encap[RTE_MAX_LCORE] __rte_cache_aligned;
	static struct rte_flow_item items[5];
	static struct rte_flow_item_eth item_eth;
	static struct rte_flow_item_ipv4 item_ipv4;
	static struct rte_flow_item_udp item_udp;
	static struct rte_flow_item_vxlan item_vxlan;
	uint32_t ip_dst = para.counter;

	/* Fixed value */
	if (!para.unique_data)
		ip_dst = 1;

	items[0].spec = &item_eth;
	items[0].mask = &item_eth;
	items[0].type = RTE_FLOW_ITEM_TYPE_ETH;

	item_ipv4.hdr.src_addr = RTE_IPV4(127, 0, 0, 1);
	item_ipv4.hdr.dst_addr = RTE_BE32(ip_dst);
	item_ipv4.hdr.version_ihl = RTE_IPV4_VHL_DEF;
	items[1].spec = &item_ipv4;
	items[1].mask = &item_ipv4;
	items[1].type = RTE_FLOW_ITEM_TYPE_IPV4;


	item_udp.hdr.dst_port = RTE_BE16(RTE_VXLAN_DEFAULT_PORT);
	items[2].spec = &item_udp;
	items[2].mask = &item_udp;
	items[2].type = RTE_FLOW_ITEM_TYPE_UDP;


	item_vxlan.hdr.vni[2] = 1;
	items[3].spec = &item_vxlan;
	items[3].mask = &item_vxlan;
	items[3].type = RTE_FLOW_ITEM_TYPE_VXLAN;

	items[4].type = RTE_FLOW_ITEM_TYPE_END;

	vxlan_encap[para.core_idx].definition = items;

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP;
	actions[actions_counter].conf = &vxlan_encap[para.core_idx];
}

static void
add_vxlan_decap(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_VXLAN_DECAP;
}

static void
add_meter(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_action_meter
		meters[RTE_MAX_LCORE] __rte_cache_aligned;

	meters[para.core_idx].mtr_id = para.counter;
	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_METER;
	actions[actions_counter].conf = &meters[para.core_idx];
}

void
fill_actions(struct rte_flow_action *actions, uint64_t *flow_actions,
	uint32_t counter, uint16_t next_table, uint16_t hairpinq,
	uint64_t encap_data, uint64_t decap_data, uint8_t core_idx,
	bool unique_data, uint8_t rx_queues_count, uint16_t dst_port)
{
	struct additional_para additional_para_data;
	uint8_t actions_counter = 0;
	uint16_t hairpin_queues[hairpinq];
	uint16_t queues[rx_queues_count];
	uint16_t i, j;

	for (i = 0; i < rx_queues_count; i++)
		queues[i] = i;

	for (i = 0; i < hairpinq; i++)
		hairpin_queues[i] = i + rx_queues_count;

	additional_para_data = (struct additional_para){
		.queue = counter % rx_queues_count,
		.next_table = next_table,
		.queues = queues,
		.queues_number = rx_queues_count,
		.counter = counter,
		.encap_data = encap_data,
		.decap_data = decap_data,
		.core_idx = core_idx,
		.unique_data = unique_data,
		.dst_port = dst_port,
	};

	if (hairpinq != 0) {
		additional_para_data.queues = hairpin_queues;
		additional_para_data.queues_number = hairpinq;
		additional_para_data.queue = (counter % hairpinq) + rx_queues_count;
	}

	static const struct actions_dict {
		uint64_t mask;
		void (*funct)(
			struct rte_flow_action *actions,
			uint8_t actions_counter,
			struct additional_para para
			);
	} actions_list[] = {
		{
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_MARK),
			.funct = add_mark,
		},
		{
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_COUNT),
			.funct = add_count,
		},
		{
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_SET_META),
			.funct = add_set_meta,
		},
		{
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_SET_TAG),
			.funct = add_set_tag,
		},
		{
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_FLAG
			),
			.funct = add_flag,
		},
		{
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_SET_MAC_SRC
			),
			.funct = add_set_src_mac,
		},
		{
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_SET_MAC_DST
			),
			.funct = add_set_dst_mac,
		},
		{
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC
			),
			.funct = add_set_src_ipv4,
		},
		{
			.mask =	FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_SET_IPV4_DST
			),
			.funct = add_set_dst_ipv4,
		},
		{
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC
			),
			.funct = add_set_src_ipv6,
		},
		{
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_SET_IPV6_DST
			),
			.funct = add_set_dst_ipv6,
		},
		{
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_SET_TP_SRC
			),
			.funct = add_set_src_tp,
		},
		{
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_SET_TP_DST
			),
			.funct = add_set_dst_tp,
		},
		{
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_INC_TCP_ACK
			),
			.funct = add_inc_tcp_ack,
		},
		{
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_DEC_TCP_ACK
			),
			.funct = add_dec_tcp_ack,
		},
		{
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_INC_TCP_SEQ
			),
			.funct = add_inc_tcp_seq,
		},
		{
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_DEC_TCP_SEQ
			),
			.funct = add_dec_tcp_seq,
		},
		{
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_SET_TTL
			),
			.funct = add_set_ttl,
		},
		{
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_DEC_TTL
			),
			.funct = add_dec_ttl,
		},
		{
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_SET_IPV4_DSCP
			),
			.funct = add_set_ipv4_dscp,
		},
		{
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_SET_IPV6_DSCP
			),
			.funct = add_set_ipv6_dscp,
		},
		{
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_QUEUE),
			.funct = add_queue,
		},
		{
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_RSS),
			.funct = add_rss,
		},
		{
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_JUMP),
			.funct = add_jump,
		},
		{
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_PORT_ID),
			.funct = add_port_id
		},
		{
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_DROP),
			.funct = add_drop,
		},
		{
			.mask = HAIRPIN_QUEUE_ACTION,
			.funct = add_queue,
		},
		{
			.mask = HAIRPIN_RSS_ACTION,
			.funct = add_rss,
		},
		{
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_RAW_ENCAP
			),
			.funct = add_raw_encap,
		},
		{
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_RAW_DECAP
			),
			.funct = add_raw_decap,
		},
		{
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP
			),
			.funct = add_vxlan_encap,
		},
		{
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_VXLAN_DECAP
			),
			.funct = add_vxlan_decap,
		},
		{
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_METER
			),
			.funct = add_meter,
		},
	};

	for (j = 0; j < MAX_ACTIONS_NUM; j++) {
		if (flow_actions[j] == 0)
			break;
		for (i = 0; i < RTE_DIM(actions_list); i++) {
			if ((flow_actions[j] &
				actions_list[i].mask) == 0)
				continue;
			actions_list[i].funct(
				actions, actions_counter++,
				additional_para_data
			);
			break;
		}
	}
	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_END;
}
