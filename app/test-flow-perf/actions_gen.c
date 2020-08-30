/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 *
 * The file contains the implementations of actions generators.
 * Each generator is responsible for preparing it's action instance
 * and initializing it with needed data.
 */

#include <sys/types.h>
#include <rte_malloc.h>
#include <rte_flow.h>
#include <rte_ethdev.h>

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
	__rte_unused struct additional_para para)
{
	static struct rte_flow_action_mark mark_action;

	do {
		mark_action.id = MARK_ID;
	} while (0);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_MARK;
	actions[actions_counter].conf = &mark_action;
}

static void
add_queue(struct rte_flow_action *actions,
	uint8_t actions_counter,
	struct additional_para para)
{
	static struct rte_flow_action_queue queue_action;

	do {
		queue_action.index = para.queue;
	} while (0);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	actions[actions_counter].conf = &queue_action;
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
	static struct rte_flow_action_rss *rss_action;
	static struct action_rss_data *rss_data;

	uint16_t queue;

	rss_data = rte_malloc("rss_data",
		sizeof(struct action_rss_data), 0);

	if (rss_data == NULL)
		rte_exit(EXIT_FAILURE, "No Memory available!");

	*rss_data = (struct action_rss_data){
		.conf = (struct rte_flow_action_rss){
			.func = RTE_ETH_HASH_FUNCTION_DEFAULT,
			.level = 0,
			.types = GET_RSS_HF(),
			.key_len = sizeof(rss_data->key),
			.queue_num = para.queues_number,
			.key = rss_data->key,
			.queue = rss_data->queue,
		},
		.key = { 1 },
		.queue = { 0 },
	};

	for (queue = 0; queue < para.queues_number; queue++)
		rss_data->queue[queue] = para.queues[queue];

	rss_action = &rss_data->conf;

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_RSS;
	actions[actions_counter].conf = rss_action;
}

static void
add_set_meta(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_action_set_meta meta_action;

	do {
		meta_action.data = RTE_BE32(META_DATA);
		meta_action.mask = RTE_BE32(0xffffffff);
	} while (0);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_META;
	actions[actions_counter].conf = &meta_action;
}

static void
add_set_tag(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_action_set_tag tag_action;

	do {
		tag_action.data = RTE_BE32(META_DATA);
		tag_action.mask = RTE_BE32(0xffffffff);
		tag_action.index = TAG_INDEX;
	} while (0);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_TAG;
	actions[actions_counter].conf = &tag_action;
}

static void
add_port_id(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_action_port_id port_id;

	do {
		port_id.id = PORT_ID_DST;
	} while (0);

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
	__rte_unused struct additional_para para)
{
	static struct rte_flow_action_set_mac set_mac;
	uint32_t mac = para.counter;
	uint16_t i;

	/* Mac address to be set is random each time */
	for (i = 0; i < RTE_ETHER_ADDR_LEN; i++) {
		set_mac.mac_addr[i] = mac & 0xff;
		mac = mac >> 8;
	}

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_MAC_SRC;
	actions[actions_counter].conf = &set_mac;
}

static void
add_set_dst_mac(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_action_set_mac set_mac;
	uint32_t mac = para.counter;
	uint16_t i;

	/* Mac address to be set is random each time */
	for (i = 0; i < RTE_ETHER_ADDR_LEN; i++) {
		set_mac.mac_addr[i] = mac & 0xff;
		mac = mac >> 8;
	}

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_MAC_DST;
	actions[actions_counter].conf = &set_mac;
}

static void
add_set_src_ipv4(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_action_set_ipv4 set_ipv4;

	/* IPv4 value to be set is random each time */
	set_ipv4.ipv4_addr = RTE_BE32(para.counter + 1);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC;
	actions[actions_counter].conf = &set_ipv4;
}

static void
add_set_dst_ipv4(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_action_set_ipv4 set_ipv4;

	/* IPv4 value to be set is random each time */
	set_ipv4.ipv4_addr = RTE_BE32(para.counter + 1);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_IPV4_DST;
	actions[actions_counter].conf = &set_ipv4;
}

static void
add_set_src_ipv6(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_action_set_ipv6 set_ipv6;
	uint32_t ipv6 = para.counter;
	uint8_t i;

	/* IPv6 value to set is random each time */
	for (i = 0; i < 16; i++) {
		set_ipv6.ipv6_addr[i] = ipv6 & 0xff;
		ipv6 = ipv6 >> 8;
	}

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC;
	actions[actions_counter].conf = &set_ipv6;
}

static void
add_set_dst_ipv6(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_action_set_ipv6 set_ipv6;
	uint32_t ipv6 = para.counter;
	uint8_t i;

	/* IPv6 value to set is random each time */
	for (i = 0; i < 16; i++) {
		set_ipv6.ipv6_addr[i] = ipv6 & 0xff;
		ipv6 = ipv6 >> 8;
	}

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_IPV6_DST;
	actions[actions_counter].conf = &set_ipv6;
}

static void
add_set_src_tp(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_action_set_tp set_tp;
	uint32_t tp = para.counter;

	/* TP src port is random each time */
	if (tp > 0xffff)
		tp = tp >> 16;

	set_tp.port = RTE_BE16(tp & 0xffff);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_TP_SRC;
	actions[actions_counter].conf = &set_tp;
}

static void
add_set_dst_tp(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_action_set_tp set_tp;
	uint32_t tp = para.counter;

	/* TP src port is random each time */
	if (tp > 0xffff)
		tp = tp >> 16;

	set_tp.port = RTE_BE16(tp & 0xffff);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_TP_DST;
	actions[actions_counter].conf = &set_tp;
}

static void
add_inc_tcp_ack(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	static rte_be32_t value = RTE_BE32(1);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_INC_TCP_ACK;
	actions[actions_counter].conf = &value;
}

static void
add_dec_tcp_ack(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	static rte_be32_t value = RTE_BE32(1);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_DEC_TCP_ACK;
	actions[actions_counter].conf = &value;
}

static void
add_inc_tcp_seq(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	static rte_be32_t value = RTE_BE32(1);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_INC_TCP_SEQ;
	actions[actions_counter].conf = &value;
}

static void
add_dec_tcp_seq(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	static rte_be32_t value = RTE_BE32(1);

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_DEC_TCP_SEQ;
	actions[actions_counter].conf = &value;
}

static void
add_set_ttl(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_action_set_ttl set_ttl;
	uint32_t ttl_value = para.counter;

	/* Set ttl to random value each time */
	while (ttl_value > 0xff)
		ttl_value = ttl_value >> 8;

	set_ttl.ttl_value = ttl_value;

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_TTL;
	actions[actions_counter].conf = &set_ttl;
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
	__rte_unused struct additional_para para)
{
	static struct rte_flow_action_set_dscp set_dscp;
	uint32_t dscp_value = para.counter;

	/* Set dscp to random value each time */
	while (dscp_value > 0xff)
		dscp_value = dscp_value >> 8;

	set_dscp.dscp = dscp_value;

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_IPV4_DSCP;
	actions[actions_counter].conf = &set_dscp;
}

static void
add_set_ipv6_dscp(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_action_set_dscp set_dscp;
	uint32_t dscp_value = para.counter;

	/* Set dscp to random value each time */
	while (dscp_value > 0xff)
		dscp_value = dscp_value >> 8;

	set_dscp.dscp = dscp_value;

	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_SET_IPV6_DSCP;
	actions[actions_counter].conf = &set_dscp;
}

static void
add_flag(struct rte_flow_action *actions,
	uint8_t actions_counter,
	__rte_unused struct additional_para para)
{
	actions[actions_counter].type = RTE_FLOW_ACTION_TYPE_FLAG;
}

void
fill_actions(struct rte_flow_action *actions, uint64_t *flow_actions,
	uint32_t counter, uint16_t next_table, uint16_t hairpinq)
{
	struct additional_para additional_para_data;
	uint8_t actions_counter = 0;
	uint16_t hairpin_queues[hairpinq];
	uint16_t queues[RXQ_NUM];
	uint16_t i, j;

	for (i = 0; i < RXQ_NUM; i++)
		queues[i] = i;

	for (i = 0; i < hairpinq; i++)
		hairpin_queues[i] = i + RXQ_NUM;

	additional_para_data = (struct additional_para){
		.queue = counter % RXQ_NUM,
		.next_table = next_table,
		.queues = queues,
		.queues_number = RXQ_NUM,
		.counter = counter,
	};

	if (hairpinq != 0) {
		additional_para_data.queues = hairpin_queues;
		additional_para_data.queues_number = hairpinq;
		additional_para_data.queue = (counter % hairpinq) + RXQ_NUM;
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
