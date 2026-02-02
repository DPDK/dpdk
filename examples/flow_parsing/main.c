/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 DynaNIC Semiconductors, Ltd.
 */

/*
 * Flow Parsing Example
 * ====================
 * This example demonstrates how to use the flow_parser library to parse
 * flow rule strings into rte_flow C structures. The library provides ONE WAY
 * to create rte_flow structures - by parsing testpmd-style command strings.
 *
 * Alternative approaches:
 *   - Construct rte_flow_attr, rte_flow_item[], and rte_flow_action[]
 *     directly in C code and call rte_flow_create()/rte_flow_validate().
 *
 * This string-based approach is useful when:
 *   - Accepting flow rules from user input or configuration files
 *   - Reusing the familiar testpmd flow command syntax
 *   - Rapid prototyping without manually constructing C structures
 *
 * Key functions demonstrated:
 *   - rte_flow_parser_parse_attr_str() - Parse flow attributes (ingress/egress/priority)
 *   - rte_flow_parser_parse_pattern_str() - Parse match patterns (eth/ipv4/tcp/etc)
 *   - rte_flow_parser_parse_actions_str() - Parse actions (drop/queue/mark/etc)
 *
 * NOTE: The parse functions return pointers to internal static buffers.
 * Each subsequent parse call overwrites the previous result.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_ether.h>
#include <rte_flow.h>
#include <rte_flow_parser.h>

/* Helper to print flow attributes */
static void
print_attr(const struct rte_flow_attr *attr)
{
	printf("  Attributes:\n");
	printf("    group=%u priority=%u\n", attr->group, attr->priority);
	printf("    ingress=%u egress=%u transfer=%u\n",
	       attr->ingress, attr->egress, attr->transfer);
}

/* Helper to print a MAC address */
static void
print_mac(const char *label, const struct rte_ether_addr *mac)
{
	char buf[RTE_ETHER_ADDR_FMT_SIZE];

	rte_ether_format_addr(buf, sizeof(buf), mac);
	printf("    %s: %s\n", label, buf);
}

/* Helper to print pattern items */
static void
print_pattern(const struct rte_flow_item *pattern, uint32_t pattern_n)
{
	uint32_t i;

	printf("  Pattern (%u items):\n", pattern_n);
	for (i = 0; i < pattern_n; i++) {
		const struct rte_flow_item *item = &pattern[i];

		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_END:
			printf("    [%u] END\n", i);
			break;
		case RTE_FLOW_ITEM_TYPE_ETH:
			printf("    [%u] ETH", i);
			if (item->spec) {
				const struct rte_flow_item_eth *eth = item->spec;
				printf("\n");
				print_mac("dst", &eth->hdr.dst_addr);
				print_mac("src", &eth->hdr.src_addr);
			} else {
				printf(" (any)\n");
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			printf("    [%u] IPV4", i);
			if (item->spec) {
				const struct rte_flow_item_ipv4 *ipv4 = item->spec;
				const uint8_t *s = (const uint8_t *)&ipv4->hdr.src_addr;
				const uint8_t *d = (const uint8_t *)&ipv4->hdr.dst_addr;
				printf(" src=%u.%u.%u.%u dst=%u.%u.%u.%u\n",
				       s[0], s[1], s[2], s[3],
				       d[0], d[1], d[2], d[3]);
			} else {
				printf(" (any)\n");
			}
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			printf("    [%u] TCP", i);
			if (item->spec) {
				const struct rte_flow_item_tcp *tcp = item->spec;
				printf(" sport=%u dport=%u\n",
				       rte_be_to_cpu_16(tcp->hdr.src_port),
				       rte_be_to_cpu_16(tcp->hdr.dst_port));
			} else {
				printf(" (any)\n");
			}
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			printf("    [%u] UDP", i);
			if (item->spec) {
				const struct rte_flow_item_udp *udp = item->spec;
				printf(" sport=%u dport=%u\n",
				       rte_be_to_cpu_16(udp->hdr.src_port),
				       rte_be_to_cpu_16(udp->hdr.dst_port));
			} else {
				printf(" (any)\n");
			}
			break;
		default:
			printf("    [%u] type=%d\n", i, item->type);
			break;
		}
	}
}

/* Helper to print actions */
static void
print_actions(const struct rte_flow_action *actions, uint32_t actions_n)
{
	uint32_t i;

	printf("  Actions (%u items):\n", actions_n);
	for (i = 0; i < actions_n; i++) {
		const struct rte_flow_action *action = &actions[i];

		switch (action->type) {
		case RTE_FLOW_ACTION_TYPE_END:
			printf("    [%u] END\n", i);
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			printf("    [%u] DROP\n", i);
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			if (action->conf) {
				const struct rte_flow_action_queue *q = action->conf;
				printf("    [%u] QUEUE index=%u\n", i, q->index);
			}
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			if (action->conf) {
				const struct rte_flow_action_mark *m = action->conf;
				printf("    [%u] MARK id=%u\n", i, m->id);
			}
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			printf("    [%u] COUNT\n", i);
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			if (action->conf) {
				const struct rte_flow_action_port_id *p = action->conf;
				printf("    [%u] PORT_ID id=%u\n", i, p->id);
			}
			break;
		default:
			printf("    [%u] type=%d\n", i, action->type);
			break;
		}
	}
}

/*
 * Demonstrate parsing flow attributes
 */
static void
demo_parse_attr(void)
{
	static const char * const attr_strings[] = {
		"ingress",
		"egress",
		"ingress priority 5",
		"ingress group 1 priority 10",
		"transfer",
	};
	struct rte_flow_attr attr;
	unsigned int i;
	int ret;

	printf("\n=== Parsing Flow Attributes ===\n");
	printf("Use rte_flow_parser_parse_attr_str() to parse attribute strings.\n\n");

	for (i = 0; i < RTE_DIM(attr_strings); i++) {
		printf("Input: \"%s\"\n", attr_strings[i]);
		memset(&attr, 0, sizeof(attr));
		ret = rte_flow_parser_parse_attr_str(attr_strings[i], &attr);
		if (ret == 0)
			print_attr(&attr);
		else
			printf("  ERROR: %d (%s)\n", ret, strerror(-ret));
		printf("\n");
	}
}

/*
 * Demonstrate parsing flow patterns
 */
static void
demo_parse_pattern(void)
{
	static const char * const pattern_strings[] = {
		"eth / end",
		"eth dst is 90:61:ae:fd:41:43 / end",
		"eth / ipv4 src is 192.168.1.1 / end",
		"eth / ipv4 / tcp dst is 80 / end",
		"eth / ipv4 src is 10.0.0.1 dst is 10.0.0.2 / udp src is 1234 dst is 5678 / end",
	};
	const struct rte_flow_item *pattern;
	uint32_t pattern_n;
	unsigned int i;
	int ret;

	printf("\n=== Parsing Flow Patterns ===\n");
	printf("Use rte_flow_parser_parse_pattern_str() to parse pattern strings.\n\n");

	for (i = 0; i < RTE_DIM(pattern_strings); i++) {
		printf("Input: \"%s\"\n", pattern_strings[i]);
		ret = rte_flow_parser_parse_pattern_str(pattern_strings[i],
							&pattern, &pattern_n);
		if (ret == 0)
			print_pattern(pattern, pattern_n);
		else
			printf("  ERROR: %d (%s)\n", ret, strerror(-ret));
		printf("\n");
	}
}

/*
 * Demonstrate parsing flow actions
 */
static void
demo_parse_actions(void)
{
	static const char * const action_strings[] = {
		"drop / end",
		"queue index 3 / end",
		"mark id 42 / end",
		"count / queue index 1 / end",
		"mark id 100 / count / queue index 5 / end",
	};
	const struct rte_flow_action *actions;
	uint32_t actions_n;
	unsigned int i;
	int ret;

	printf("\n=== Parsing Flow Actions ===\n");
	printf("Use rte_flow_parser_parse_actions_str() to parse action strings.\n\n");

	for (i = 0; i < RTE_DIM(action_strings); i++) {
		printf("Input: \"%s\"\n", action_strings[i]);
		ret = rte_flow_parser_parse_actions_str(action_strings[i],
							&actions, &actions_n);
		if (ret == 0)
			print_actions(actions, actions_n);
		else
			printf("  ERROR: %d (%s)\n", ret, strerror(-ret));
		printf("\n");
	}
}

int
main(void)
{
	int ret;

	printf("Flow Parser Library Example\n");
	printf("===========================\n");

	/* Initialize the flow parser */
	ret = rte_flow_parser_init(NULL);
	if (ret != 0) {
		fprintf(stderr, "Failed to initialize flow parser: %d\n", ret);
		return 1;
	}

	/* Run demonstrations */
	demo_parse_attr();
	demo_parse_pattern();
	demo_parse_actions();

	printf("\n=== Example Complete ===\n");
	return 0;
}
