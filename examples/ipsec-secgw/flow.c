/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#include <stdio.h>

#include <rte_common.h>
#include <rte_flow.h>
#include <rte_ip.h>

#include "flow.h"
#include "ipsec-secgw.h"
#include "parser.h"

#define FLOW_RULES_MAX 128

struct flow_rule_entry {
	uint8_t is_ipv4;
	RTE_STD_C11
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
	uint16_t port;
	uint16_t queue;
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
		*mask = *mask << (32-depth);

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
	uint32_t ti;

	if (nb_flow_rule >= FLOW_RULES_MAX) {
		printf("Too many flow rules\n");
		return;
	}

	rule = &flow_rule_tbl[nb_flow_rule];
	memset(rule, 0, sizeof(*rule));

	if (strcmp(tokens[0], "ipv4") == 0) {
		rule->is_ipv4 = 1;
	} else if (strcmp(tokens[0], "ipv6") == 0) {
		rule->is_ipv4 = 0;
	} else {
		APP_CHECK(0, status, "unrecognized input \"%s\"", tokens[0]);
		return;
	}

	for (ti = 1; ti < n_tokens; ti++) {
		if (strcmp(tokens[ti], "src") == 0) {
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			if (rule->is_ipv4) {
				if (ipv4_addr_cpy(&rule->ipv4.spec.hdr.src_addr,
						  &rule->ipv4.mask.hdr.src_addr,
						  tokens[ti], status))
					return;
			} else {
				if (ipv6_addr_cpy(rule->ipv6.spec.hdr.src_addr,
						  rule->ipv6.mask.hdr.src_addr,
						  tokens[ti], status))
					return;
			}
		}
		if (strcmp(tokens[ti], "dst") == 0) {
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			if (rule->is_ipv4) {
				if (ipv4_addr_cpy(&rule->ipv4.spec.hdr.dst_addr,
						  &rule->ipv4.mask.hdr.dst_addr,
						  tokens[ti], status))
					return;
			} else {
				if (ipv6_addr_cpy(rule->ipv6.spec.hdr.dst_addr,
						  rule->ipv6.mask.hdr.dst_addr,
						  tokens[ti], status))
					return;
			}
		}

		if (strcmp(tokens[ti], "port") == 0) {
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;
			APP_CHECK_TOKEN_IS_NUM(tokens, ti, status);
			if (status->status < 0)
				return;

			rule->port = atoi(tokens[ti]);
		}

		if (strcmp(tokens[ti], "queue") == 0) {
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;
			APP_CHECK_TOKEN_IS_NUM(tokens, ti, status);
			if (status->status < 0)
				return;

			rule->queue = atoi(tokens[ti]);
		}
	}

	nb_flow_rule++;
}

#define MAX_RTE_FLOW_PATTERN (3)
#define MAX_RTE_FLOW_ACTIONS (2)

static void
flow_init_single(struct flow_rule_entry *rule)
{
	struct rte_flow_item pattern[MAX_RTE_FLOW_PATTERN] = {};
	struct rte_flow_action action[MAX_RTE_FLOW_ACTIONS] = {};
	struct rte_flow_attr attr = {};
	struct rte_flow_error err;
	int ret;

	attr.egress = 0;
	attr.ingress = 1;

	action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	action[0].conf = &(struct rte_flow_action_queue) {
				.index = rule->queue,
	};
	action[1].type = RTE_FLOW_ACTION_TYPE_END;

	pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;

	if (rule->is_ipv4) {
		pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
		pattern[1].spec = &rule->ipv4.spec;
		pattern[1].mask = &rule->ipv4.mask;
	} else {
		pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV6;
		pattern[1].spec = &rule->ipv6.spec;
		pattern[1].mask = &rule->ipv6.mask;
	}

	pattern[2].type = RTE_FLOW_ITEM_TYPE_END;

	ret = rte_flow_validate(rule->port, &attr, pattern, action, &err);
	if (ret < 0) {
		RTE_LOG(ERR, IPSEC, "Flow validation failed %s\n", err.message);
		return;
	}

	rule->flow = rte_flow_create(rule->port, &attr, pattern, action, &err);
	if (rule->flow == NULL)
		RTE_LOG(ERR, IPSEC, "Flow creation return %s\n", err.message);
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
		if (rule->is_ipv4) {
			printf("Flow #%3d: spec ipv4 ", i);
			ipv4_hdr_print(&rule->ipv4.spec.hdr);
			printf("\n");
			printf("           mask ipv4 ");
			ipv4_hdr_print(&rule->ipv4.mask.hdr);
		} else {
			printf("Flow #%3d: spec ipv6 ", i);
			ipv6_hdr_print(&rule->ipv6.spec.hdr);
			printf("\n");
			printf("           mask ipv6 ");
			ipv6_hdr_print(&rule->ipv6.mask.hdr);
		}

		printf("\tPort: %d, Queue: %d", rule->port, rule->queue);

		if (rule->flow == NULL)
			printf(" [UNSUPPORTED]");
		printf("\n");
	}
}
