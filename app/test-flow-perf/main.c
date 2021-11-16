/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 *
 * This file contain the application main file
 * This application provides the user the ability to test the
 * insertion rate for specific rte_flow rule under stress state ~4M rule/
 *
 * Then it will also provide packet per second measurement after installing
 * all rules, the user may send traffic to test the PPS that match the rules
 * after all rules are installed, to check performance or functionality after
 * the stress.
 *
 * The flows insertion will go for all ports first, then it will print the
 * results, after that the application will go into forwarding packets mode
 * it will start receiving traffic if any and then forwarding it back and
 * gives packet per second measurement.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>

#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_flow.h>

#include "config.h"
#include "flow_gen.h"

#define MAX_ITERATIONS             100
#define DEFAULT_RULES_COUNT    4000000
#define DEFAULT_RULES_BATCH     100000
#define DEFAULT_GROUP                0

struct rte_flow *flow;
static uint8_t flow_group;

static uint64_t encap_data;
static uint64_t decap_data;

static uint64_t flow_items[MAX_ITEMS_NUM];
static uint64_t flow_actions[MAX_ACTIONS_NUM];
static uint64_t flow_attrs[MAX_ATTRS_NUM];
static uint8_t items_idx, actions_idx, attrs_idx;

static uint64_t ports_mask;
static volatile bool force_quit;
static bool dump_iterations;
static bool delete_flag;
static bool dump_socket_mem_flag;
static bool enable_fwd;

static struct rte_mempool *mbuf_mp;
static uint32_t nb_lcores;
static uint32_t rules_count;
static uint32_t rules_batch;
static uint32_t hairpin_queues_num; /* total hairpin q number - default: 0 */
static uint32_t nb_lcores;

#define MAX_PKT_BURST    32
#define LCORE_MODE_PKT    1
#define LCORE_MODE_STATS  2
#define MAX_STREAMS      64
#define MAX_LCORES       64

struct stream {
	int tx_port;
	int tx_queue;
	int rx_port;
	int rx_queue;
};

struct lcore_info {
	int mode;
	int streams_nb;
	struct stream streams[MAX_STREAMS];
	/* stats */
	uint64_t tx_pkts;
	uint64_t tx_drops;
	uint64_t rx_pkts;
	struct rte_mbuf *pkts[MAX_PKT_BURST];
} __rte_cache_aligned;

static struct lcore_info lcore_infos[MAX_LCORES];

static void
usage(char *progname)
{
	printf("\nusage: %s\n", progname);
	printf("\nControl configurations:\n");
	printf("  --rules-count=N: to set the number of needed"
		" rules to insert, default is %d\n", DEFAULT_RULES_COUNT);
	printf("  --rules-batch=N: set number of batched rules,"
		" default is %d\n", DEFAULT_RULES_BATCH);
	printf("  --dump-iterations: To print rates for each"
		" iteration\n");
	printf("  --deletion-rate: Enable deletion rate"
		" calculations\n");
	printf("  --dump-socket-mem: To dump all socket memory\n");
	printf("  --enable-fwd: To enable packets forwarding"
		" after insertion\n");
	printf("  --portmask=N: hexadecimal bitmask of ports used\n");

	printf("To set flow attributes:\n");
	printf("  --ingress: set ingress attribute in flows\n");
	printf("  --egress: set egress attribute in flows\n");
	printf("  --transfer: set transfer attribute in flows\n");
	printf("  --group=N: set group for all flows,"
		" default is %d\n", DEFAULT_GROUP);

	printf("To set flow items:\n");
	printf("  --ether: add ether layer in flow items\n");
	printf("  --vlan: add vlan layer in flow items\n");
	printf("  --ipv4: add ipv4 layer in flow items\n");
	printf("  --ipv6: add ipv6 layer in flow items\n");
	printf("  --tcp: add tcp layer in flow items\n");
	printf("  --udp: add udp layer in flow items\n");
	printf("  --vxlan: add vxlan layer in flow items\n");
	printf("  --vxlan-gpe: add vxlan-gpe layer in flow items\n");
	printf("  --gre: add gre layer in flow items\n");
	printf("  --geneve: add geneve layer in flow items\n");
	printf("  --gtp: add gtp layer in flow items\n");
	printf("  --meta: add meta layer in flow items\n");
	printf("  --tag: add tag layer in flow items\n");
	printf("  --icmpv4: add icmpv4 layer in flow items\n");
	printf("  --icmpv6: add icmpv6 layer in flow items\n");

	printf("To set flow actions:\n");
	printf("  --port-id: add port-id action in flow actions\n");
	printf("  --rss: add rss action in flow actions\n");
	printf("  --queue: add queue action in flow actions\n");
	printf("  --jump: add jump action in flow actions\n");
	printf("  --mark: add mark action in flow actions\n");
	printf("  --count: add count action in flow actions\n");
	printf("  --set-meta: add set meta action in flow actions\n");
	printf("  --set-tag: add set tag action in flow actions\n");
	printf("  --drop: add drop action in flow actions\n");
	printf("  --hairpin-queue=N: add hairpin-queue action in flow actions\n");
	printf("  --hairpin-rss=N: add hairpin-rss action in flow actions\n");
	printf("  --set-src-mac: add set src mac action to flow actions\n"
		"Src mac to be set is random each flow\n");
	printf("  --set-dst-mac: add set dst mac action to flow actions\n"
		 "Dst mac to be set is random each flow\n");
	printf("  --set-src-ipv4: add set src ipv4 action to flow actions\n"
		"Src ipv4 to be set is random each flow\n");
	printf("  --set-dst-ipv4 add set dst ipv4 action to flow actions\n"
		"Dst ipv4 to be set is random each flow\n");
	printf("  --set-src-ipv6: add set src ipv6 action to flow actions\n"
		"Src ipv6 to be set is random each flow\n");
	printf("  --set-dst-ipv6: add set dst ipv6 action to flow actions\n"
		"Dst ipv6 to be set is random each flow\n");
	printf("  --set-src-tp: add set src tp action to flow actions\n"
		"Src tp to be set is random each flow\n");
	printf("  --set-dst-tp: add set dst tp action to flow actions\n"
		"Dst tp to be set is random each flow\n");
	printf("  --inc-tcp-ack: add inc tcp ack action to flow actions\n"
		"tcp ack will be increments by 1\n");
	printf("  --dec-tcp-ack: add dec tcp ack action to flow actions\n"
		"tcp ack will be decrements by 1\n");
	printf("  --inc-tcp-seq: add inc tcp seq action to flow actions\n"
		"tcp seq will be increments by 1\n");
	printf("  --dec-tcp-seq: add dec tcp seq action to flow actions\n"
		"tcp seq will be decrements by 1\n");
	printf("  --set-ttl: add set ttl action to flow actions\n"
		"L3 ttl to be set is random each flow\n");
	printf("  --dec-ttl: add dec ttl action to flow actions\n"
		"L3 ttl will be decrements by 1\n");
	printf("  --set-ipv4-dscp: add set ipv4 dscp action to flow actions\n"
		"ipv4 dscp value to be set is random each flow\n");
	printf("  --set-ipv6-dscp: add set ipv6 dscp action to flow actions\n"
		"ipv6 dscp value to be set is random each flow\n");
	printf("  --flag: add flag action to flow actions\n");
	printf("  --raw-encap=<data>: add raw encap action to flow actions\n"
		"Data is the data needed to be encaped\n"
		"Example: raw-encap=ether,ipv4,udp,vxlan\n");
	printf("  --raw-decap=<data>: add raw decap action to flow actions\n"
		"Data is the data needed to be decaped\n"
		"Example: raw-decap=ether,ipv4,udp,vxlan\n");
	printf("  --vxlan-encap: add vxlan-encap action to flow actions\n"
		"Encapped data is fixed with pattern: ether,ipv4,udp,vxlan\n"
		"With fixed values\n");
	printf("  --vxlan-decap: add vxlan_decap action to flow actions\n");
}

static void
args_parse(int argc, char **argv)
{
	uint64_t pm;
	char **argvopt;
	char *token;
	char *end;
	int n, opt;
	int opt_idx;
	size_t i;

	static const struct option_dict {
		const char *str;
		const uint64_t mask;
		uint64_t *map;
		uint8_t *map_idx;

	} flow_options[] = {
		{
			.str = "ether",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_ETH),
			.map = &flow_items[0],
			.map_idx = &items_idx
		},
		{
			.str = "ipv4",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_IPV4),
			.map = &flow_items[0],
			.map_idx = &items_idx
		},
		{
			.str = "ipv6",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_IPV6),
			.map = &flow_items[0],
			.map_idx = &items_idx
		},
		{
			.str = "vlan",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_VLAN),
			.map = &flow_items[0],
			.map_idx = &items_idx
		},
		{
			.str = "tcp",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_TCP),
			.map = &flow_items[0],
			.map_idx = &items_idx
		},
		{
			.str = "udp",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_UDP),
			.map = &flow_items[0],
			.map_idx = &items_idx
		},
		{
			.str = "vxlan",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_VXLAN),
			.map = &flow_items[0],
			.map_idx = &items_idx
		},
		{
			.str = "vxlan-gpe",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_VXLAN_GPE),
			.map = &flow_items[0],
			.map_idx = &items_idx
		},
		{
			.str = "gre",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_GRE),
			.map = &flow_items[0],
			.map_idx = &items_idx
		},
		{
			.str = "geneve",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_GENEVE),
			.map = &flow_items[0],
			.map_idx = &items_idx
		},
		{
			.str = "gtp",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_GTP),
			.map = &flow_items[0],
			.map_idx = &items_idx
		},
		{
			.str = "meta",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_META),
			.map = &flow_items[0],
			.map_idx = &items_idx
		},
		{
			.str = "tag",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_TAG),
			.map = &flow_items[0],
			.map_idx = &items_idx
		},
		{
			.str = "icmpv4",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_ICMP),
			.map = &flow_items[0],
			.map_idx = &items_idx
		},
		{
			.str = "icmpv6",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_ICMP6),
			.map = &flow_items[0],
			.map_idx = &items_idx
		},
		{
			.str = "ingress",
			.mask = INGRESS,
			.map = &flow_attrs[0],
			.map_idx = &attrs_idx
		},
		{
			.str = "egress",
			.mask = EGRESS,
			.map = &flow_attrs[0],
			.map_idx = &attrs_idx
		},
		{
			.str = "transfer",
			.mask = TRANSFER,
			.map = &flow_attrs[0],
			.map_idx = &attrs_idx
		},
		{
			.str = "port-id",
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_PORT_ID),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "rss",
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_RSS),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "queue",
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_QUEUE),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "jump",
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_JUMP),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "mark",
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_MARK),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "count",
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_COUNT),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "set-meta",
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_SET_META),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "set-tag",
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_SET_TAG),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "drop",
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_DROP),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "set-src-mac",
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_SET_MAC_SRC
			),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "set-dst-mac",
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_SET_MAC_DST
			),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "set-src-ipv4",
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC
			),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "set-dst-ipv4",
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_SET_IPV4_DST
			),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "set-src-ipv6",
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC
			),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "set-dst-ipv6",
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_SET_IPV6_DST
			),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "set-src-tp",
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_SET_TP_SRC
			),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "set-dst-tp",
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_SET_TP_DST
			),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "inc-tcp-ack",
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_INC_TCP_ACK
			),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "dec-tcp-ack",
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_DEC_TCP_ACK
			),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "inc-tcp-seq",
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_INC_TCP_SEQ
			),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "dec-tcp-seq",
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_DEC_TCP_SEQ
			),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "set-ttl",
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_SET_TTL
			),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "dec-ttl",
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_DEC_TTL
			),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "set-ipv4-dscp",
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_SET_IPV4_DSCP
			),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "set-ipv6-dscp",
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_SET_IPV6_DSCP
			),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "flag",
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_FLAG
			),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "vxlan-encap",
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP
			),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
		{
			.str = "vxlan-decap",
			.mask = FLOW_ACTION_MASK(
				RTE_FLOW_ACTION_TYPE_VXLAN_DECAP
			),
			.map = &flow_actions[0],
			.map_idx = &actions_idx
		},
	};

	static const struct option lgopts[] = {
		/* Control */
		{ "help",                       0, 0, 0 },
		{ "rules-count",                1, 0, 0 },
		{ "rules-batch",                1, 0, 0 },
		{ "dump-iterations",            0, 0, 0 },
		{ "deletion-rate",              0, 0, 0 },
		{ "dump-socket-mem",            0, 0, 0 },
		{ "enable-fwd",                 0, 0, 0 },
		{ "portmask",                   1, 0, 0 },
		/* Attributes */
		{ "ingress",                    0, 0, 0 },
		{ "egress",                     0, 0, 0 },
		{ "transfer",                   0, 0, 0 },
		{ "group",                      1, 0, 0 },
		/* Items */
		{ "ether",                      0, 0, 0 },
		{ "vlan",                       0, 0, 0 },
		{ "ipv4",                       0, 0, 0 },
		{ "ipv6",                       0, 0, 0 },
		{ "tcp",                        0, 0, 0 },
		{ "udp",                        0, 0, 0 },
		{ "vxlan",                      0, 0, 0 },
		{ "vxlan-gpe",                  0, 0, 0 },
		{ "gre",                        0, 0, 0 },
		{ "geneve",                     0, 0, 0 },
		{ "gtp",                        0, 0, 0 },
		{ "meta",                       0, 0, 0 },
		{ "tag",                        0, 0, 0 },
		{ "icmpv4",                     0, 0, 0 },
		{ "icmpv6",                     0, 0, 0 },
		/* Actions */
		{ "port-id",                    0, 0, 0 },
		{ "rss",                        0, 0, 0 },
		{ "queue",                      0, 0, 0 },
		{ "jump",                       0, 0, 0 },
		{ "mark",                       0, 0, 0 },
		{ "count",                      0, 0, 0 },
		{ "set-meta",                   0, 0, 0 },
		{ "set-tag",                    0, 0, 0 },
		{ "drop",                       0, 0, 0 },
		{ "hairpin-queue",              1, 0, 0 },
		{ "hairpin-rss",                1, 0, 0 },
		{ "set-src-mac",                0, 0, 0 },
		{ "set-dst-mac",                0, 0, 0 },
		{ "set-src-ipv4",               0, 0, 0 },
		{ "set-dst-ipv4",               0, 0, 0 },
		{ "set-src-ipv6",               0, 0, 0 },
		{ "set-dst-ipv6",               0, 0, 0 },
		{ "set-src-tp",                 0, 0, 0 },
		{ "set-dst-tp",                 0, 0, 0 },
		{ "inc-tcp-ack",                0, 0, 0 },
		{ "dec-tcp-ack",                0, 0, 0 },
		{ "inc-tcp-seq",                0, 0, 0 },
		{ "dec-tcp-seq",                0, 0, 0 },
		{ "set-ttl",                    0, 0, 0 },
		{ "dec-ttl",                    0, 0, 0 },
		{ "set-ipv4-dscp",              0, 0, 0 },
		{ "set-ipv6-dscp",              0, 0, 0 },
		{ "flag",                       0, 0, 0 },
		{ "raw-encap",                  1, 0, 0 },
		{ "raw-decap",                  1, 0, 0 },
		{ "vxlan-encap",                0, 0, 0 },
		{ "vxlan-decap",                0, 0, 0 },
		{ 0, 0, 0, 0 },
	};

	RTE_ETH_FOREACH_DEV(i)
		ports_mask |= 1 << i;

	hairpin_queues_num = 0;
	argvopt = argv;

	printf(":: Flow -> ");
	while ((opt = getopt_long(argc, argvopt, "",
				lgopts, &opt_idx)) != EOF) {
		switch (opt) {
		case 0:
			if (strcmp(lgopts[opt_idx].name, "help") == 0) {
				usage(argv[0]);
				exit(EXIT_SUCCESS);
			}

			if (strcmp(lgopts[opt_idx].name, "group") == 0) {
				n = atoi(optarg);
				if (n >= 0)
					flow_group = n;
				else
					rte_exit(EXIT_FAILURE,
						"flow group should be >= 0\n");
				printf("group %d / ", flow_group);
			}

			for (i = 0; i < RTE_DIM(flow_options); i++)
				if (strcmp(lgopts[opt_idx].name,
						flow_options[i].str) == 0) {
					flow_options[i].map[
					(*flow_options[i].map_idx)++] =
						flow_options[i].mask;
					printf("%s / ", flow_options[i].str);
				}

			if (strcmp(lgopts[opt_idx].name,
					"hairpin-rss") == 0) {
				n = atoi(optarg);
				if (n > 0)
					hairpin_queues_num = n;
				else
					rte_exit(EXIT_FAILURE,
						"Hairpin queues should be > 0\n");

				flow_actions[actions_idx++] =
					HAIRPIN_RSS_ACTION;
				printf("hairpin-rss / ");
			}
			if (strcmp(lgopts[opt_idx].name,
					"hairpin-queue") == 0) {
				n = atoi(optarg);
				if (n > 0)
					hairpin_queues_num = n;
				else
					rte_exit(EXIT_FAILURE,
						"Hairpin queues should be > 0\n");

				flow_actions[actions_idx++] =
					HAIRPIN_QUEUE_ACTION;
				printf("hairpin-queue / ");
			}

			if (strcmp(lgopts[opt_idx].name, "raw-encap") == 0) {
				printf("raw-encap ");
				flow_actions[actions_idx++] =
					FLOW_ITEM_MASK(
						RTE_FLOW_ACTION_TYPE_RAW_ENCAP
					);

				token = strtok(optarg, ",");
				while (token != NULL) {
					for (i = 0; i < RTE_DIM(flow_options); i++) {
						if (strcmp(flow_options[i].str, token) == 0) {
							printf("%s,", token);
							encap_data |= flow_options[i].mask;
							break;
						}
						/* Reached last item with no match */
						if (i == (RTE_DIM(flow_options) - 1))
							rte_exit(EXIT_FAILURE,
								"Invalid encap item: %s\n", token);
					}
					token = strtok(NULL, ",");
				}
				printf(" / ");
			}
			if (strcmp(lgopts[opt_idx].name, "raw-decap") == 0) {
				printf("raw-decap ");
				flow_actions[actions_idx++] =
					FLOW_ITEM_MASK(
						RTE_FLOW_ACTION_TYPE_RAW_DECAP
					);

				token = strtok(optarg, ",");
				while (token != NULL) {
					for (i = 0; i < RTE_DIM(flow_options); i++) {
						if (strcmp(flow_options[i].str, token) == 0) {
							printf("%s,", token);
							decap_data |= flow_options[i].mask;
							break;
						}
						/* Reached last item with no match */
						if (i == (RTE_DIM(flow_options) - 1))
							rte_exit(EXIT_FAILURE,
								"Invalid decap item %s\n", token);
					}
					token = strtok(NULL, ",");
				}
				printf(" / ");
			}
			/* Control */
			if (strcmp(lgopts[opt_idx].name,
					"rules-batch") == 0) {
				n = atoi(optarg);
				if (n >= DEFAULT_RULES_BATCH)
					rules_batch = n;
				else {
					rte_exit(EXIT_FAILURE,
						"rules_batch should be >= %d\n",
						DEFAULT_RULES_BATCH);
				}
			}
			if (strcmp(lgopts[opt_idx].name,
					"rules-count") == 0) {
				n = atoi(optarg);
				if (n >= (int) rules_batch)
					rules_count = n;
				else {
					rte_exit(EXIT_FAILURE,
						"rules_count should be >= %d\n",
						rules_batch);
				}
			}
			if (strcmp(lgopts[opt_idx].name,
					"dump-iterations") == 0)
				dump_iterations = true;
			if (strcmp(lgopts[opt_idx].name,
					"deletion-rate") == 0)
				delete_flag = true;
			if (strcmp(lgopts[opt_idx].name,
					"dump-socket-mem") == 0)
				dump_socket_mem_flag = true;
			if (strcmp(lgopts[opt_idx].name,
					"enable-fwd") == 0)
				enable_fwd = true;
			if (strcmp(lgopts[opt_idx].name,
					"portmask") == 0) {
				/* parse hexadecimal string */
				end = NULL;
				pm = strtoull(optarg, &end, 16);
				if ((optarg[0] == '\0') || (end == NULL) || (*end != '\0'))
					rte_exit(EXIT_FAILURE, "Invalid fwd port mask\n");
				ports_mask = pm;
			}
			break;
		default:
			usage(argv[0]);
			rte_exit(EXIT_FAILURE, "Invalid option: %s\n",
					argv[optind - 1]);
			break;
		}
	}
	printf("end_flow\n");
}

/* Dump the socket memory statistics on console */
static size_t
dump_socket_mem(FILE *f)
{
	struct rte_malloc_socket_stats socket_stats;
	unsigned int i = 0;
	size_t total = 0;
	size_t alloc = 0;
	size_t free = 0;
	unsigned int n_alloc = 0;
	unsigned int n_free = 0;
	bool active_nodes = false;


	for (i = 0; i < RTE_MAX_NUMA_NODES; i++) {
		if (rte_malloc_get_socket_stats(i, &socket_stats) ||
		    !socket_stats.heap_totalsz_bytes)
			continue;
		active_nodes = true;
		total += socket_stats.heap_totalsz_bytes;
		alloc += socket_stats.heap_allocsz_bytes;
		free += socket_stats.heap_freesz_bytes;
		n_alloc += socket_stats.alloc_count;
		n_free += socket_stats.free_count;
		if (dump_socket_mem_flag) {
			fprintf(f, "::::::::::::::::::::::::::::::::::::::::");
			fprintf(f,
				"\nSocket %u:\nsize(M) total: %.6lf\nalloc:"
				" %.6lf(%.3lf%%)\nfree: %.6lf"
				"\nmax: %.6lf"
				"\ncount alloc: %u\nfree: %u\n",
				i,
				socket_stats.heap_totalsz_bytes / 1.0e6,
				socket_stats.heap_allocsz_bytes / 1.0e6,
				(double)socket_stats.heap_allocsz_bytes * 100 /
				(double)socket_stats.heap_totalsz_bytes,
				socket_stats.heap_freesz_bytes / 1.0e6,
				socket_stats.greatest_free_size / 1.0e6,
				socket_stats.alloc_count,
				socket_stats.free_count);
				fprintf(f, "::::::::::::::::::::::::::::::::::::::::");
		}
	}
	if (dump_socket_mem_flag && active_nodes) {
		fprintf(f,
			"\nTotal: size(M)\ntotal: %.6lf"
			"\nalloc: %.6lf(%.3lf%%)\nfree: %.6lf"
			"\ncount alloc: %u\nfree: %u\n",
			total / 1.0e6, alloc / 1.0e6,
			(double)alloc * 100 / (double)total, free / 1.0e6,
			n_alloc, n_free);
		fprintf(f, "::::::::::::::::::::::::::::::::::::::::\n");
	}
	return alloc;
}

static void
print_flow_error(struct rte_flow_error error)
{
	printf("Flow can't be created %d message: %s\n",
		error.type,
		error.message ? error.message : "(no stated reason)");
}

static inline void
destroy_flows(int port_id, struct rte_flow **flow_list)
{
	struct rte_flow_error error;
	clock_t start_iter, end_iter;
	double cpu_time_used = 0;
	double flows_rate;
	double cpu_time_per_iter[MAX_ITERATIONS];
	double delta;
	uint32_t i;
	int iter_id;

	for (i = 0; i < MAX_ITERATIONS; i++)
		cpu_time_per_iter[i] = -1;

	if (rules_batch > rules_count)
		rules_batch = rules_count;

	/* Deletion Rate */
	printf("Flows Deletion on port = %d\n", port_id);
	start_iter = clock();
	for (i = 0; i < rules_count; i++) {
		if (flow_list[i] == 0)
			break;

		memset(&error, 0x33, sizeof(error));
		if (rte_flow_destroy(port_id, flow_list[i], &error)) {
			print_flow_error(error);
			rte_exit(EXIT_FAILURE, "Error in deleting flow\n");
		}

		if (i && !((i + 1) % rules_batch)) {
			/* Save the deletion rate of each iter */
			end_iter = clock();
			delta = (double) (end_iter - start_iter);
			iter_id = ((i + 1) / rules_batch) - 1;
			cpu_time_per_iter[iter_id] =
				delta / CLOCKS_PER_SEC;
			cpu_time_used += cpu_time_per_iter[iter_id];
			start_iter = clock();
		}
	}

	/* Deletion rate per iteration */
	if (dump_iterations)
		for (i = 0; i < MAX_ITERATIONS; i++) {
			if (cpu_time_per_iter[i] == -1)
				continue;
			delta = (double)(rules_batch /
				cpu_time_per_iter[i]);
			flows_rate = delta / 1000;
			printf(":: Iteration #%d: %d flows "
				"in %f sec[ Rate = %f K/Sec ]\n",
				i, rules_batch,
				cpu_time_per_iter[i], flows_rate);
		}

	/* Deletion rate for all flows */
	flows_rate = ((double) (rules_count / cpu_time_used) / 1000);
	printf("\n:: Total flow deletion rate -> %f K/Sec\n",
		flows_rate);
	printf(":: The time for deleting %d in flows %f seconds\n",
		rules_count, cpu_time_used);
}

static inline void
flows_handler(void)
{
	struct rte_flow **flow_list;
	struct rte_flow_error error;
	clock_t start_iter, end_iter;
	double cpu_time_used;
	double flows_rate;
	double cpu_time_per_iter[MAX_ITERATIONS];
	double delta;
	uint16_t nr_ports;
	uint32_t i;
	int port_id;
	int iter_id;
	uint32_t flow_index;
	uint64_t global_items[MAX_ITEMS_NUM] = { 0 };
	uint64_t global_actions[MAX_ACTIONS_NUM] = { 0 };

	global_items[0] = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_ETH);
	global_actions[0] = FLOW_ITEM_MASK(RTE_FLOW_ACTION_TYPE_JUMP);

	nr_ports = rte_eth_dev_count_avail();

	for (i = 0; i < MAX_ITERATIONS; i++)
		cpu_time_per_iter[i] = -1;

	if (rules_batch > rules_count)
		rules_batch = rules_count;

	printf(":: Flows Count per port: %d\n", rules_count);

	flow_list = rte_zmalloc("flow_list",
		(sizeof(struct rte_flow *) * rules_count) + 1, 0);
	if (flow_list == NULL)
		rte_exit(EXIT_FAILURE, "No Memory available!\n");

	for (port_id = 0; port_id < nr_ports; port_id++) {
		/* If port outside portmask */
		if (!((ports_mask >> port_id) & 0x1))
			continue;
		cpu_time_used = 0;
		flow_index = 0;
		if (flow_group > 0) {
			/*
			 * Create global rule to jump into flow_group,
			 * this way the app will avoid the default rules.
			 *
			 * Global rule:
			 * group 0 eth / end actions jump group <flow_group>
			 *
			 */
			flow = generate_flow(port_id, 0, flow_attrs,
				global_items, global_actions,
				flow_group, 0, 0, 0, 0, &error);

			if (flow == NULL) {
				print_flow_error(error);
				rte_exit(EXIT_FAILURE, "Error in creating flow\n");
			}
			flow_list[flow_index++] = flow;
		}

		/* Insertion Rate */
		printf("Flows insertion on port = %d\n", port_id);
		start_iter = clock();
		for (i = 0; i < rules_count; i++) {
			flow = generate_flow(port_id, flow_group,
				flow_attrs, flow_items, flow_actions,
				JUMP_ACTION_TABLE, i,
				hairpin_queues_num,
				encap_data, decap_data,
				&error);

			if (force_quit)
				i = rules_count;

			if (!flow) {
				print_flow_error(error);
				rte_exit(EXIT_FAILURE, "Error in creating flow\n");
			}

			flow_list[flow_index++] = flow;

			if (i && !((i + 1) % rules_batch)) {
				/* Save the insertion rate of each iter */
				end_iter = clock();
				delta = (double) (end_iter - start_iter);
				iter_id = ((i + 1) / rules_batch) - 1;
				cpu_time_per_iter[iter_id] =
					delta / CLOCKS_PER_SEC;
				cpu_time_used += cpu_time_per_iter[iter_id];
				start_iter = clock();
			}
		}

		/* Iteration rate per iteration */
		if (dump_iterations)
			for (i = 0; i < MAX_ITERATIONS; i++) {
				if (cpu_time_per_iter[i] == -1)
					continue;
				delta = (double)(rules_batch /
					cpu_time_per_iter[i]);
				flows_rate = delta / 1000;
				printf(":: Iteration #%d: %d flows "
					"in %f sec[ Rate = %f K/Sec ]\n",
					i, rules_batch,
					cpu_time_per_iter[i], flows_rate);
			}

		/* Insertion rate for all flows */
		flows_rate = ((double) (rules_count / cpu_time_used) / 1000);
		printf("\n:: Total flow insertion rate -> %f K/Sec\n",
						flows_rate);
		printf(":: The time for creating %d in flows %f seconds\n",
						rules_count, cpu_time_used);

		if (delete_flag)
			destroy_flows(port_id, flow_list);
	}
}

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n",
					signum);
		printf("Error: Stats are wrong due to sudden signal!\n\n");
		force_quit = true;
	}
}

static inline uint16_t
do_rx(struct lcore_info *li, uint16_t rx_port, uint16_t rx_queue)
{
	uint16_t cnt = 0;
	cnt = rte_eth_rx_burst(rx_port, rx_queue, li->pkts, MAX_PKT_BURST);
	li->rx_pkts += cnt;
	return cnt;
}

static inline void
do_tx(struct lcore_info *li, uint16_t cnt, uint16_t tx_port,
			uint16_t tx_queue)
{
	uint16_t nr_tx = 0;
	uint16_t i;

	nr_tx = rte_eth_tx_burst(tx_port, tx_queue, li->pkts, cnt);
	li->tx_pkts  += nr_tx;
	li->tx_drops += cnt - nr_tx;

	for (i = nr_tx; i < cnt; i++)
		rte_pktmbuf_free(li->pkts[i]);
}

/*
 * Method to convert numbers into pretty numbers that easy
 * to read. The design here is to add comma after each three
 * digits and set all of this inside buffer.
 *
 * For example if n = 1799321, the output will be
 * 1,799,321 after this method which is easier to read.
 */
static char *
pretty_number(uint64_t n, char *buf)
{
	char p[6][4];
	int i = 0;
	int off = 0;

	while (n > 1000) {
		sprintf(p[i], "%03d", (int)(n % 1000));
		n /= 1000;
		i += 1;
	}

	sprintf(p[i++], "%d", (int)n);

	while (i--)
		off += sprintf(buf + off, "%s,", p[i]);
	buf[strlen(buf) - 1] = '\0';

	return buf;
}

static void
packet_per_second_stats(void)
{
	struct lcore_info *old;
	struct lcore_info *li, *oli;
	int nr_lines = 0;
	int i;

	old = rte_zmalloc("old",
		sizeof(struct lcore_info) * MAX_LCORES, 0);
	if (old == NULL)
		rte_exit(EXIT_FAILURE, "No Memory available!\n");

	memcpy(old, lcore_infos,
		sizeof(struct lcore_info) * MAX_LCORES);

	while (!force_quit) {
		uint64_t total_tx_pkts = 0;
		uint64_t total_rx_pkts = 0;
		uint64_t total_tx_drops = 0;
		uint64_t tx_delta, rx_delta, drops_delta;
		char buf[3][32];
		int nr_valid_core = 0;

		sleep(1);

		if (nr_lines) {
			char go_up_nr_lines[16];

			sprintf(go_up_nr_lines, "%c[%dA\r", 27, nr_lines);
			printf("%s\r", go_up_nr_lines);
		}

		printf("\n%6s %16s %16s %16s\n", "core", "tx", "tx drops", "rx");
		printf("%6s %16s %16s %16s\n", "------", "----------------",
			"----------------", "----------------");
		nr_lines = 3;
		for (i = 0; i < MAX_LCORES; i++) {
			li  = &lcore_infos[i];
			oli = &old[i];
			if (li->mode != LCORE_MODE_PKT)
				continue;

			tx_delta    = li->tx_pkts  - oli->tx_pkts;
			rx_delta    = li->rx_pkts  - oli->rx_pkts;
			drops_delta = li->tx_drops - oli->tx_drops;
			printf("%6d %16s %16s %16s\n", i,
				pretty_number(tx_delta,    buf[0]),
				pretty_number(drops_delta, buf[1]),
				pretty_number(rx_delta,    buf[2]));

			total_tx_pkts  += tx_delta;
			total_rx_pkts  += rx_delta;
			total_tx_drops += drops_delta;

			nr_valid_core++;
			nr_lines += 1;
		}

		if (nr_valid_core > 1) {
			printf("%6s %16s %16s %16s\n", "total",
				pretty_number(total_tx_pkts,  buf[0]),
				pretty_number(total_tx_drops, buf[1]),
				pretty_number(total_rx_pkts,  buf[2]));
			nr_lines += 1;
		}

		memcpy(old, lcore_infos,
			sizeof(struct lcore_info) * MAX_LCORES);
	}
}

static int
start_forwarding(void *data __rte_unused)
{
	int lcore = rte_lcore_id();
	int stream_id;
	uint16_t cnt;
	struct lcore_info *li = &lcore_infos[lcore];

	if (!li->mode)
		return 0;

	if (li->mode == LCORE_MODE_STATS) {
		printf(":: started stats on lcore %u\n", lcore);
		packet_per_second_stats();
		return 0;
	}

	while (!force_quit)
		for (stream_id = 0; stream_id < MAX_STREAMS; stream_id++) {
			if (li->streams[stream_id].rx_port == -1)
				continue;

			cnt = do_rx(li,
					li->streams[stream_id].rx_port,
					li->streams[stream_id].rx_queue);
			if (cnt)
				do_tx(li, cnt,
					li->streams[stream_id].tx_port,
					li->streams[stream_id].tx_queue);
		}
	return 0;
}

static void
init_lcore_info(void)
{
	int i, j;
	unsigned int lcore;
	uint16_t nr_port;
	uint16_t queue;
	int port;
	int stream_id = 0;
	int streams_per_core;
	int unassigned_streams;
	int nb_fwd_streams;
	nr_port = rte_eth_dev_count_avail();

	/* First logical core is reserved for stats printing */
	lcore = rte_get_next_lcore(-1, 0, 0);
	lcore_infos[lcore].mode = LCORE_MODE_STATS;

	/*
	 * Initialize all cores
	 * All cores at first must have -1 value in all streams
	 * This means that this stream is not used, or not set
	 * yet.
	 */
	for (i = 0; i < MAX_LCORES; i++)
		for (j = 0; j < MAX_STREAMS; j++) {
			lcore_infos[i].streams[j].tx_port = -1;
			lcore_infos[i].streams[j].rx_port = -1;
			lcore_infos[i].streams[j].tx_queue = -1;
			lcore_infos[i].streams[j].rx_queue = -1;
			lcore_infos[i].streams_nb = 0;
		}

	/*
	 * Calculate the total streams count.
	 * Also distribute those streams count between the available
	 * logical cores except first core, since it's reserved for
	 * stats prints.
	 */
	nb_fwd_streams = nr_port * RXQ_NUM;
	if ((int)(nb_lcores - 1) >= nb_fwd_streams)
		for (i = 0; i < (int)(nb_lcores - 1); i++) {
			lcore = rte_get_next_lcore(lcore, 0, 0);
			lcore_infos[lcore].streams_nb = 1;
		}
	else {
		streams_per_core = nb_fwd_streams / (nb_lcores - 1);
		unassigned_streams = nb_fwd_streams % (nb_lcores - 1);
		for (i = 0; i < (int)(nb_lcores - 1); i++) {
			lcore = rte_get_next_lcore(lcore, 0, 0);
			lcore_infos[lcore].streams_nb = streams_per_core;
			if (unassigned_streams) {
				lcore_infos[lcore].streams_nb++;
				unassigned_streams--;
			}
		}
	}

	/*
	 * Set the streams for the cores according to each logical
	 * core stream count.
	 * The streams is built on the design of what received should
	 * forward as well, this means that if you received packets on
	 * port 0 queue 0 then the same queue should forward the
	 * packets, using the same logical core.
	 */
	lcore = rte_get_next_lcore(-1, 0, 0);
	for (port = 0; port < nr_port; port++) {
		/* Create FWD stream */
		for (queue = 0; queue < RXQ_NUM; queue++) {
			if (!lcore_infos[lcore].streams_nb ||
				!(stream_id % lcore_infos[lcore].streams_nb)) {
				lcore = rte_get_next_lcore(lcore, 0, 0);
				lcore_infos[lcore].mode = LCORE_MODE_PKT;
				stream_id = 0;
			}
			lcore_infos[lcore].streams[stream_id].rx_queue = queue;
			lcore_infos[lcore].streams[stream_id].tx_queue = queue;
			lcore_infos[lcore].streams[stream_id].rx_port = port;
			lcore_infos[lcore].streams[stream_id].tx_port = port;
			stream_id++;
		}
	}

	/* Print all streams */
	printf(":: Stream -> core id[N]: (rx_port, rx_queue)->(tx_port, tx_queue)\n");
	for (i = 0; i < MAX_LCORES; i++)
		for (j = 0; j < MAX_STREAMS; j++) {
			/* No streams for this core */
			if (lcore_infos[i].streams[j].tx_port == -1)
				break;
			printf("Stream -> core id[%d]: (%d,%d)->(%d,%d)\n",
				i,
				lcore_infos[i].streams[j].rx_port,
				lcore_infos[i].streams[j].rx_queue,
				lcore_infos[i].streams[j].tx_port,
				lcore_infos[i].streams[j].tx_queue);
		}
}

static void
init_port(void)
{
	int ret;
	uint16_t std_queue;
	uint16_t hairpin_queue;
	uint16_t port_id;
	uint16_t nr_ports;
	uint16_t nr_queues;
	struct rte_eth_hairpin_conf hairpin_conf = {
		.peer_count = 1,
	};
	struct rte_eth_conf port_conf = {
		.rx_adv_conf = {
			.rss_conf.rss_hf =
				GET_RSS_HF(),
		}
	};
	struct rte_eth_txconf txq_conf;
	struct rte_eth_rxconf rxq_conf;
	struct rte_eth_dev_info dev_info;

	nr_queues = RXQ_NUM;
	if (hairpin_queues_num != 0)
		nr_queues = RXQ_NUM + hairpin_queues_num;

	nr_ports = rte_eth_dev_count_avail();
	if (nr_ports == 0)
		rte_exit(EXIT_FAILURE, "Error: no port detected\n");

	mbuf_mp = rte_pktmbuf_pool_create("mbuf_pool",
					TOTAL_MBUF_NUM, MBUF_CACHE_SIZE,
					0, MBUF_SIZE,
					rte_socket_id());
	if (mbuf_mp == NULL)
		rte_exit(EXIT_FAILURE, "Error: can't init mbuf pool\n");

	for (port_id = 0; port_id < nr_ports; port_id++) {
		ret = rte_eth_dev_info_get(port_id, &dev_info);
		if (ret != 0)
			rte_exit(EXIT_FAILURE,
				"Error during getting device"
				" (port %u) info: %s\n",
				port_id, strerror(-ret));

		port_conf.txmode.offloads &= dev_info.tx_offload_capa;
		port_conf.rxmode.offloads &= dev_info.rx_offload_capa;

		printf(":: initializing port: %d\n", port_id);

		ret = rte_eth_dev_configure(port_id, nr_queues,
				nr_queues, &port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				":: cannot configure device: err=%d, port=%u\n",
				ret, port_id);

		rxq_conf = dev_info.default_rxconf;
		for (std_queue = 0; std_queue < RXQ_NUM; std_queue++) {
			ret = rte_eth_rx_queue_setup(port_id, std_queue, NR_RXD,
					rte_eth_dev_socket_id(port_id),
					&rxq_conf,
					mbuf_mp);
			if (ret < 0)
				rte_exit(EXIT_FAILURE,
					":: Rx queue setup failed: err=%d, port=%u\n",
					ret, port_id);
		}

		txq_conf = dev_info.default_txconf;
		for (std_queue = 0; std_queue < TXQ_NUM; std_queue++) {
			ret = rte_eth_tx_queue_setup(port_id, std_queue, NR_TXD,
					rte_eth_dev_socket_id(port_id),
					&txq_conf);
			if (ret < 0)
				rte_exit(EXIT_FAILURE,
					":: Tx queue setup failed: err=%d, port=%u\n",
					ret, port_id);
		}

		/* Catch all packets from traffic generator. */
		ret = rte_eth_promiscuous_enable(port_id);
		if (ret != 0)
			rte_exit(EXIT_FAILURE,
				":: promiscuous mode enable failed: err=%s, port=%u\n",
				rte_strerror(-ret), port_id);

		if (hairpin_queues_num != 0) {
			/*
			 * Configure peer which represents hairpin Tx.
			 * Hairpin queue numbers start after standard queues
			 * (RXQ_NUM and TXQ_NUM).
			 */
			for (hairpin_queue = RXQ_NUM, std_queue = 0;
					hairpin_queue < nr_queues;
					hairpin_queue++, std_queue++) {
				hairpin_conf.peers[0].port = port_id;
				hairpin_conf.peers[0].queue =
					std_queue + TXQ_NUM;
				ret = rte_eth_rx_hairpin_queue_setup(
						port_id, hairpin_queue,
						NR_RXD, &hairpin_conf);
				if (ret != 0)
					rte_exit(EXIT_FAILURE,
						":: Hairpin rx queue setup failed: err=%d, port=%u\n",
						ret, port_id);
			}

			for (hairpin_queue = TXQ_NUM, std_queue = 0;
					hairpin_queue < nr_queues;
					hairpin_queue++, std_queue++) {
				hairpin_conf.peers[0].port = port_id;
				hairpin_conf.peers[0].queue =
					std_queue + RXQ_NUM;
				ret = rte_eth_tx_hairpin_queue_setup(
						port_id, hairpin_queue,
						NR_TXD, &hairpin_conf);
				if (ret != 0)
					rte_exit(EXIT_FAILURE,
						":: Hairpin tx queue setup failed: err=%d, port=%u\n",
						ret, port_id);
			}
		}

		ret = rte_eth_dev_start(port_id);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				"rte_eth_dev_start:err=%d, port=%u\n",
				ret, port_id);

		printf(":: initializing port: %d done\n", port_id);
	}
}

int
main(int argc, char **argv)
{
	int ret;
	uint16_t port;
	struct rte_flow_error error;
	int64_t alloc, last_alloc;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "EAL init failed\n");

	force_quit = false;
	dump_iterations = false;
	rules_count = DEFAULT_RULES_COUNT;
	rules_batch = DEFAULT_RULES_BATCH;
	delete_flag = false;
	dump_socket_mem_flag = false;
	flow_group = DEFAULT_GROUP;

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	argc -= ret;
	argv += ret;
	if (argc > 1)
		args_parse(argc, argv);

	init_port();

	nb_lcores = rte_lcore_count();
	if (nb_lcores <= 1)
		rte_exit(EXIT_FAILURE, "This app needs at least two cores\n");

	last_alloc = (int64_t)dump_socket_mem(stdout);
	flows_handler();
	alloc = (int64_t)dump_socket_mem(stdout);

	if (last_alloc)
		fprintf(stdout, ":: Memory allocation change(M): %.6lf\n",
		(alloc - last_alloc) / 1.0e6);

	if (enable_fwd) {
		init_lcore_info();
		rte_eal_mp_remote_launch(start_forwarding, NULL, CALL_MAIN);
	}

	RTE_ETH_FOREACH_DEV(port) {
		rte_flow_flush(port, &error);
		if (rte_eth_dev_stop(port) != 0)
			printf("Failed to stop device on port %u\n", port);
		rte_eth_dev_close(port);
	}
	return 0;
}
