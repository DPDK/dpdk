/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#include <stdio.h>
#include <stdlib.h>

#include <rte_common.h>
#include <rte_flow.h>
#include <rte_ip.h>

#include "flow.h"
#include "ipsec-secgw.h"
#include "parser.h"

#define FLOW_RULES_MAX 128

struct flow_rule_entry {
	uint8_t is_eth;
	uint8_t is_ipv4;
	uint8_t is_ipv6;
	union {
		struct {
			struct rte_flow_item_ipv4 spec;
			struct rte_flow_item_ipv4 mask;
		} ipv4;
		struct {
			struct rte_flow_item_ipv6 spec;
			struct rte_flow_item_ipv6 mask;
		} ipv6;
	};
	struct rte_flow_item_mark mark_val;
	uint16_t port;
	uint16_t queue;
	bool is_queue_set;
	bool enable_count;
	bool enable_mark;
	bool set_security_action;
	bool set_mark_action;
	uint32_t mark_action_val;
	struct rte_flow *flow;
} flow_rule_tbl[FLOW_RULES_MAX];

int nb_flow_rule;

static void
ipv4_hdr_print(struct rte_ipv4_hdr *hdr)
{
	char a, b, c, d;

	uint32_t_to_char(rte_bswap32(hdr->src_addr), &a, &b, &c, &d);
	printf("src: %3hhu.%3hhu.%3hhu.%3hhu \t", a, b, c, d);

	uint32_t_to_char(rte_bswap32(hdr->dst_addr), &a, &b, &c, &d);
	printf("dst: %3hhu.%3hhu.%3hhu.%3hhu", a, b, c, d);
}

static int
ipv4_addr_cpy(rte_be32_t *spec, rte_be32_t *mask, char *token,
	      struct parse_status *status)
{
	struct in_addr ip;
	uint32_t depth;

	APP_CHECK(parse_ipv4_addr(token, &ip, &depth) == 0, status,
		 "unrecognized input \"%s\", expect valid ipv4 addr", token);
	if (status->status < 0)
		return -1;

	if (depth > 32)
		return -1;

	memcpy(mask, &rte_flow_item_ipv4_mask.hdr.src_addr, sizeof(ip));

	*spec = ip.s_addr;

	if (depth < 32)
		*mask = htonl(*mask << (32 - depth));

	return 0;
}

static void
ipv6_hdr_print(struct rte_ipv6_hdr *hdr)
{
	uint8_t *addr;

	addr = hdr->src_addr;
	printf("src: %4hx:%4hx:%4hx:%4hx:%4hx:%4hx:%4hx:%4hx \t",
	       (uint16_t)((addr[0] << 8) | addr[1]),
	       (uint16_t)((addr[2] << 8) | addr[3]),
	       (uint16_t)((addr[4] << 8) | addr[5]),
	       (uint16_t)((addr[6] << 8) | addr[7]),
	       (uint16_t)((addr[8] << 8) | addr[9]),
	       (uint16_t)((addr[10] << 8) | addr[11]),
	       (uint16_t)((addr[12] << 8) | addr[13]),
	       (uint16_t)((addr[14] << 8) | addr[15]));

	addr = hdr->dst_addr;
	printf("dst: %4hx:%4hx:%4hx:%4hx:%4hx:%4hx:%4hx:%4hx",
	       (uint16_t)((addr[0] << 8) | addr[1]),
	       (uint16_t)((addr[2] << 8) | addr[3]),
	       (uint16_t)((addr[4] << 8) | addr[5]),
	       (uint16_t)((addr[6] << 8) | addr[7]),
	       (uint16_t)((addr[8] << 8) | addr[9]),
	       (uint16_t)((addr[10] << 8) | addr[11]),
	       (uint16_t)((addr[12] << 8) | addr[13]),
	       (uint16_t)((addr[14] << 8) | addr[15]));
}

static int
ipv6_addr_cpy(uint8_t *spec, uint8_t *mask, char *token,
	      struct parse_status *status)
{
	struct in6_addr ip;
	uint32_t depth, i;

	APP_CHECK(parse_ipv6_addr(token, &ip, &depth) == 0, status,
		"unrecognized input \"%s\", expect valid ipv6 address", token);
	if (status->status < 0)
		return -1;

	memcpy(mask, &rte_flow_item_ipv6_mask.hdr.src_addr, sizeof(ip));
	memcpy(spec, ip.s6_addr, sizeof(struct in6_addr));

	for (i = 0; i < depth && (i%8 <= sizeof(struct in6_addr)); i++)
		mask[i/8] &= ~(1 << (7-i%8));

	return 0;
}

void
parse_flow_tokens(char **tokens, uint32_t n_tokens,
		  struct parse_status *status)
{
	struct flow_rule_entry *rule;
	uint32_t ti = 0;

	if (nb_flow_rule >= FLOW_RULES_MAX) {
		printf("Too many flow rules\n");
		return;
	}

	rule = &flow_rule_tbl[nb_flow_rule];
	memset(rule, 0, sizeof(*rule));

	for (ti = 0; ti < n_tokens; ti++) {
		if (strcmp(tokens[ti], "mark") == 0) {
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;
			APP_CHECK_TOKEN_IS_NUM(tokens, ti, status);
			if (status->status < 0)
				return;

			rule->mark_val.id = atoi(tokens[ti]);
			rule->enable_mark = true;
			continue;
		}
		if (strcmp(tokens[ti], "eth") == 0) {
			rule->is_eth = true;
			continue;
		}

		if (strcmp(tokens[ti], "ipv4") == 0) {
			rule->is_ipv4 = true;
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;
			if (strcmp(tokens[ti], "src") == 0) {
				INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
				if (status->status < 0)
					return;
				if (ipv4_addr_cpy(&rule->ipv4.spec.hdr.src_addr,
						  &rule->ipv4.mask.hdr.src_addr,
						  tokens[ti], status))
					return;
			}
			if (strcmp(tokens[ti], "dst") == 0) {
				INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
				if (status->status < 0)
					return;
				if (ipv4_addr_cpy(&rule->ipv4.spec.hdr.dst_addr,
						  &rule->ipv4.mask.hdr.dst_addr,
						  tokens[ti], status))
					return;
			}
			continue;
		}
		if (strcmp(tokens[ti], "ipv6") == 0) {
			rule->is_ipv6 = true;
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;
			if (strcmp(tokens[ti], "src") == 0) {
				INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
				if (status->status < 0)
					return;
				if (ipv6_addr_cpy(rule->ipv6.spec.hdr.src_addr,
						  rule->ipv6.mask.hdr.src_addr,
						  tokens[ti], status))
					return;
			}
			if (strcmp(tokens[ti], "dst") == 0) {
				INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
				if (status->status < 0)
					return;
				if (ipv6_addr_cpy(rule->ipv6.spec.hdr.dst_addr,
						  rule->ipv6.mask.hdr.dst_addr,
						  tokens[ti], status))
					return;
			}
			continue;
		}

		if (strcmp(tokens[ti], "port") == 0) {
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;
			APP_CHECK_TOKEN_IS_NUM(tokens, ti, status);
			if (status->status < 0)
				return;

			rule->port = atoi(tokens[ti]);
			continue;
		}

		if (strcmp(tokens[ti], "queue") == 0) {
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;
			APP_CHECK_TOKEN_IS_NUM(tokens, ti, status);
			if (status->status < 0)
				return;

			rule->queue = atoi(tokens[ti]);
			rule->is_queue_set = true;
			continue;
		}

		if (strcmp(tokens[ti], "count") == 0) {
			rule->enable_count = true;
			continue;
		}

		if (strcmp(tokens[ti], "security") == 0) {
			rule->set_security_action = true;
			continue;
		}
		if (strcmp(tokens[ti], "set_mark") == 0) {
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;
			APP_CHECK_TOKEN_IS_NUM(tokens, ti, status);
			if (status->status < 0)
				return;

			rule->set_mark_action = true;
			rule->mark_action_val = atoi(tokens[ti]);
			continue;
		}

		sprintf(status->parse_msg, "Unrecognized input:%s\n",
			tokens[ti]);
		status->status = -1;
		return;
	}
	printf("\n");

	nb_flow_rule++;
}

#define MAX_RTE_FLOW_PATTERN (5)
#define MAX_RTE_FLOW_ACTIONS (5)

static void
flow_init_single(struct flow_rule_entry *rule)
{
	struct rte_flow_action action[MAX_RTE_FLOW_ACTIONS] = {};
	struct rte_flow_item pattern[MAX_RTE_FLOW_PATTERN] = {};
	struct rte_flow_action_queue queue_action;
	struct rte_flow_action_mark mark_action;
	int ret, pattern_idx = 0, act_idx = 0;
	struct rte_flow_item_mark mark_mask;
	struct rte_flow_attr attr = {};
	struct rte_flow_error err = {};

	attr.egress = 0;
	attr.ingress = 1;

	if (rule->is_queue_set) {
		queue_action.index = rule->queue;
		action[act_idx].type = RTE_FLOW_ACTION_TYPE_QUEUE;
		action[act_idx].conf = &queue_action;
		act_idx++;
	}

	if (rule->enable_count) {
		action[act_idx].type = RTE_FLOW_ACTION_TYPE_COUNT;
		act_idx++;
	}

	if (rule->set_security_action) {
		action[act_idx].type = RTE_FLOW_ACTION_TYPE_SECURITY;
		action[act_idx].conf = NULL;
		act_idx++;
	}

	if (rule->set_mark_action) {
		mark_action.id = rule->mark_action_val;
		action[act_idx].type = RTE_FLOW_ACTION_TYPE_MARK;
		action[act_idx].conf = &mark_action;
		act_idx++;
	}

	action[act_idx].type = RTE_FLOW_ACTION_TYPE_END;
	action[act_idx].conf = NULL;

	if (rule->enable_mark) {
		mark_mask.id = UINT32_MAX;
		pattern[pattern_idx].type = RTE_FLOW_ITEM_TYPE_MARK;
		pattern[pattern_idx].spec = &rule->mark_val;
		pattern[pattern_idx].mask = &mark_mask;
		pattern_idx++;
	}

	if (rule->is_eth) {
		pattern[pattern_idx].type = RTE_FLOW_ITEM_TYPE_ETH;
		pattern_idx++;
	}

	if (rule->is_ipv4) {
		pattern[pattern_idx].type = RTE_FLOW_ITEM_TYPE_IPV4;
		pattern[pattern_idx].spec = &rule->ipv4.spec;
		pattern[pattern_idx].mask = &rule->ipv4.mask;
		pattern_idx++;
	} else if (rule->is_ipv6) {
		pattern[pattern_idx].type = RTE_FLOW_ITEM_TYPE_IPV6;
		pattern[pattern_idx].spec = &rule->ipv6.spec;
		pattern[pattern_idx].mask = &rule->ipv6.mask;
		pattern_idx++;
	}

	if (rule->set_security_action) {
		pattern[pattern_idx].type = RTE_FLOW_ITEM_TYPE_ESP;
		pattern[pattern_idx].spec = NULL;
		pattern[pattern_idx].mask = NULL;
		pattern[pattern_idx].last = NULL;
		pattern_idx++;
	}

	pattern[pattern_idx].type = RTE_FLOW_ITEM_TYPE_END;

	ret = rte_flow_validate(rule->port, &attr, pattern, action, &err);
	if (ret < 0) {
		RTE_LOG(ERR, IPSEC, "Flow validation failed %s\n", err.message);
		rule->flow = 0;
		return;
	}

	rule->flow = rte_flow_create(rule->port, &attr, pattern, action, &err);
	if (rule->flow == NULL)
		RTE_LOG(ERR, IPSEC, "Flow creation return %s\n", err.message);
}

void
flow_print_counters(void)
{
	struct rte_flow_query_count count_query;
	struct rte_flow_action action;
	struct flow_rule_entry *rule;
	struct rte_flow_error error;
	int i = 0, ret = 0;

	action.type = RTE_FLOW_ACTION_TYPE_COUNT;

	for (i = 0; i < nb_flow_rule; i++) {
		rule = &flow_rule_tbl[i];
		if (!rule->flow || !rule->enable_count)
			continue;

		/* Poisoning to make sure PMDs update it in case of error. */
		memset(&error, 0x55, sizeof(error));
		memset(&count_query, 0, sizeof(count_query));
		ret = rte_flow_query(rule->port, rule->flow, &action,
				     &count_query, &error);
		if (ret)
			RTE_LOG(ERR, IPSEC,
				"Failed to get flow counter "
				" for port %u, err msg: %s\n",
				rule->port, error.message);

		printf("Flow #%3d:", i);
		if (rule->is_ipv4) {
			printf(" spec ipv4 ");
			ipv4_hdr_print(&rule->ipv4.spec.hdr);
		}
		if (rule->is_ipv6) {
			printf(" spec ipv6 ");
			ipv6_hdr_print(&rule->ipv6.spec.hdr);
		}

		if (rule->set_security_action)
			printf(" Security action set,");

		if (rule->enable_mark)
			printf(" Mark Enabled");

		printf(" Port: %d,", rule->port);
		if (rule->is_queue_set)
			printf(" Queue: %d", rule->queue);
		printf(" Hits: %"PRIu64"\n", count_query.hits);
	}
}

void
flow_init(void)
{
	struct flow_rule_entry *rule;
	int i;

	for (i = 0; i < nb_flow_rule; i++) {
		rule = &flow_rule_tbl[i];
		flow_init_single(rule);
	}

	for (i = 0; i < nb_flow_rule; i++) {
		rule = &flow_rule_tbl[i];
		printf("Flow #%3d: ", i);
		if (rule->is_ipv4) {
			printf("spec ipv4 ");
			ipv4_hdr_print(&rule->ipv4.spec.hdr);
			printf("\n");
			printf(" mask ipv4 ");
			ipv4_hdr_print(&rule->ipv4.mask.hdr);
		}
		if (rule->is_ipv6) {
			printf("spec ipv6 ");
			ipv6_hdr_print(&rule->ipv6.spec.hdr);
			printf("\n");
			printf(" mask ipv6 ");
			ipv6_hdr_print(&rule->ipv6.mask.hdr);
		}

		if (rule->enable_mark)
			printf(", Mark enabled");

		printf("\tPort: %d,", rule->port);
		if (rule->is_queue_set)
			printf(" Queue: %d,", rule->queue);

		if (rule->set_security_action)
			printf(" Security action set,");

		if (rule->set_mark_action)
			printf(" Mark: %d,", rule->mark_action_val);

		if (rule->enable_count)
			printf(" Counter enabled,");

		if (rule->flow == NULL)
			printf(" [UNSUPPORTED]");
		printf("\n");
	}
}
