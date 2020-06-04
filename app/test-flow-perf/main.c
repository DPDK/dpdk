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

#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_flow.h>

#include "config.h"
#include "flow_gen.h"

#define MAX_ITERATIONS             100
#define DEFAULT_RULES_COUNT    4000000
#define DEFAULT_ITERATION       100000

struct rte_flow *flow;
static uint8_t flow_group;

static uint64_t flow_items;
static uint64_t flow_actions;
static uint64_t flow_attrs;
static volatile bool force_quit;
static bool dump_iterations;
static bool delete_flag;
static struct rte_mempool *mbuf_mp;
static uint32_t nb_lcores;
static uint32_t flows_count;
static uint32_t iterations_number;
static uint32_t hairpinq;

static void
usage(char *progname)
{
	printf("\nusage: %s\n", progname);
	printf("\nControl configurations:\n");
	printf("  --flows-count=N: to set the number of needed"
		" flows to insert, default is 4,000,000\n");
	printf("  --dump-iterations: To print rates for each"
		" iteration\n");
	printf("  --deletion-rate: Enable deletion rate"
		" calculations\n");

	printf("To set flow attributes:\n");
	printf("  --ingress: set ingress attribute in flows\n");
	printf("  --egress: set egress attribute in flows\n");
	printf("  --transfer: set transfer attribute in flows\n");
	printf("  --group=N: set group for all flows,"
		" default is 0\n");

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
	printf("  --hairpin-rss=N: add hairping-rss action in flow actions\n");
}

static void
args_parse(int argc, char **argv)
{
	char **argvopt;
	int n, opt;
	int opt_idx;
	size_t i;

	static const struct option_dict {
		const char *str;
		const uint64_t mask;
		uint64_t *bitmap;
	} flow_options[] = {
		{
			.str = "ether",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_ETH),
			.bitmap = &flow_items
		},
		{
			.str = "ipv4",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_IPV4),
			.bitmap = &flow_items
		},
		{
			.str = "ipv6",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_IPV6),
			.bitmap = &flow_items
		},
		{
			.str = "vlan",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_VLAN),
			.bitmap = &flow_items
		},
		{
			.str = "tcp",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_TCP),
			.bitmap = &flow_items
		},
		{
			.str = "udp",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_UDP),
			.bitmap = &flow_items
		},
		{
			.str = "vxlan",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_VXLAN),
			.bitmap = &flow_items
		},
		{
			.str = "vxlan-gpe",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_VXLAN_GPE),
			.bitmap = &flow_items
		},
		{
			.str = "gre",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_GRE),
			.bitmap = &flow_items
		},
		{
			.str = "geneve",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_GENEVE),
			.bitmap = &flow_items
		},
		{
			.str = "gtp",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_GTP),
			.bitmap = &flow_items
		},
		{
			.str = "meta",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_META),
			.bitmap = &flow_items
		},
		{
			.str = "tag",
			.mask = FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_TAG),
			.bitmap = &flow_items
		},
		{
			.str = "ingress",
			.mask = INGRESS,
			.bitmap = &flow_attrs
		},
		{
			.str = "egress",
			.mask = EGRESS,
			.bitmap = &flow_attrs
		},
		{
			.str = "transfer",
			.mask = TRANSFER,
			.bitmap = &flow_attrs
		},
		{
			.str = "port-id",
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_PORT_ID),
			.bitmap = &flow_actions
		},
		{
			.str = "rss",
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_RSS),
			.bitmap = &flow_actions
		},
		{
			.str = "queue",
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_QUEUE),
			.bitmap = &flow_actions
		},
		{
			.str = "jump",
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_JUMP),
			.bitmap = &flow_actions
		},
		{
			.str = "mark",
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_MARK),
			.bitmap = &flow_actions
		},
		{
			.str = "count",
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_COUNT),
			.bitmap = &flow_actions
		},
		{
			.str = "set-meta",
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_SET_META),
			.bitmap = &flow_actions
		},
		{
			.str = "set-tag",
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_SET_TAG),
			.bitmap = &flow_actions
		},
		{
			.str = "drop",
			.mask = FLOW_ACTION_MASK(RTE_FLOW_ACTION_TYPE_DROP),
			.bitmap = &flow_actions
		}
	};

	static const struct option lgopts[] = {
		/* Control */
		{ "help",                       0, 0, 0 },
		{ "flows-count",                1, 0, 0 },
		{ "dump-iterations",            0, 0, 0 },
		{ "deletion-rate",              0, 0, 0 },
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
	};

	flow_items = 0;
	flow_actions = 0;
	flow_attrs = 0;
	hairpinq = 0;
	argvopt = argv;

	printf(":: Flow -> ");
	while ((opt = getopt_long(argc, argvopt, "",
				lgopts, &opt_idx)) != EOF) {
		switch (opt) {
		case 0:
			if (strcmp(lgopts[opt_idx].name, "help") == 0) {
				usage(argv[0]);
				rte_exit(EXIT_SUCCESS, "Displayed help\n");
			}

			if (strcmp(lgopts[opt_idx].name, "group") == 0) {
				n = atoi(optarg);
				if (n >= 0)
					flow_group = n;
				else
					rte_exit(EXIT_SUCCESS,
						"flow group should be >= 0\n");
				printf("group %d ", flow_group);
			}

			for (i = 0; i < RTE_DIM(flow_options); i++)
				if (strcmp(lgopts[opt_idx].name,
						flow_options[i].str) == 0) {
					*flow_options[i].bitmap |=
						flow_options[i].mask;
					printf("%s / ", flow_options[i].str);
				}

			if (strcmp(lgopts[opt_idx].name,
					"hairpin-rss") == 0) {
				n = atoi(optarg);
				if (n > 0)
					hairpinq = n;
				else
					rte_exit(EXIT_SUCCESS,
						"Hairpin queues should be > 0\n");

				flow_actions |= HAIRPIN_RSS_ACTION;
				printf("hairpin-rss / ");
			}
			if (strcmp(lgopts[opt_idx].name,
					"hairpin-queue") == 0) {
				n = atoi(optarg);
				if (n > 0)
					hairpinq = n;
				else
					rte_exit(EXIT_SUCCESS,
						"Hairpin queues should be > 0\n");

				flow_actions |= HAIRPIN_QUEUE_ACTION;
				printf("hairpin-queue / ");
			}

			/* Control */
			if (strcmp(lgopts[opt_idx].name,
					"flows-count") == 0) {
				n = atoi(optarg);
				if (n > (int) iterations_number)
					flows_count = n;
				else {
					printf("\n\nflows_count should be > %d\n",
						iterations_number);
					rte_exit(EXIT_SUCCESS, " ");
				}
			}
			if (strcmp(lgopts[opt_idx].name,
					"dump-iterations") == 0)
				dump_iterations = true;
			if (strcmp(lgopts[opt_idx].name,
					"deletion-rate") == 0)
				delete_flag = true;
			break;
		default:
			fprintf(stderr, "Invalid option: %s\n", argv[optind]);
			usage(argv[0]);
			rte_exit(EXIT_SUCCESS, "Invalid option\n");
			break;
		}
	}
	printf("end_flow\n");
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

	if (iterations_number > flows_count)
		iterations_number = flows_count;

	/* Deletion Rate */
	printf("Flows Deletion on port = %d\n", port_id);
	start_iter = clock();
	for (i = 0; i < flows_count; i++) {
		if (flow_list[i] == 0)
			break;

		memset(&error, 0x33, sizeof(error));
		if (rte_flow_destroy(port_id, flow_list[i], &error)) {
			print_flow_error(error);
			rte_exit(EXIT_FAILURE, "Error in deleting flow");
		}

		if (i && !((i + 1) % iterations_number)) {
			/* Save the deletion rate of each iter */
			end_iter = clock();
			delta = (double) (end_iter - start_iter);
			iter_id = ((i + 1) / iterations_number) - 1;
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
			delta = (double)(iterations_number /
				cpu_time_per_iter[i]);
			flows_rate = delta / 1000;
			printf(":: Iteration #%d: %d flows "
				"in %f sec[ Rate = %f K/Sec ]\n",
				i, iterations_number,
				cpu_time_per_iter[i], flows_rate);
		}

	/* Deletion rate for all flows */
	flows_rate = ((double) (flows_count / cpu_time_used) / 1000);
	printf("\n:: Total flow deletion rate -> %f K/Sec\n",
		flows_rate);
	printf(":: The time for deleting %d in flows %f seconds\n",
		flows_count, cpu_time_used);
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

	nr_ports = rte_eth_dev_count_avail();

	for (i = 0; i < MAX_ITERATIONS; i++)
		cpu_time_per_iter[i] = -1;

	if (iterations_number > flows_count)
		iterations_number = flows_count;

	printf(":: Flows Count per port: %d\n", flows_count);

	flow_list = rte_zmalloc("flow_list",
		(sizeof(struct rte_flow *) * flows_count) + 1, 0);
	if (flow_list == NULL)
		rte_exit(EXIT_FAILURE, "No Memory available!");

	for (port_id = 0; port_id < nr_ports; port_id++) {
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
				FLOW_ITEM_MASK(RTE_FLOW_ITEM_TYPE_ETH),
				FLOW_ITEM_MASK(RTE_FLOW_ACTION_TYPE_JUMP),
				flow_group, 0, 0, &error);

			if (flow == NULL) {
				print_flow_error(error);
				rte_exit(EXIT_FAILURE, "error in creating flow");
			}
			flow_list[flow_index++] = flow;
		}

		/* Insertion Rate */
		printf("Flows insertion on port = %d\n", port_id);
		start_iter = clock();
		for (i = 0; i < flows_count; i++) {
			flow = generate_flow(port_id, flow_group,
				flow_attrs, flow_items, flow_actions,
				JUMP_ACTION_TABLE, i, hairpinq, &error);

			if (force_quit)
				i = flows_count;

			if (!flow) {
				print_flow_error(error);
				rte_exit(EXIT_FAILURE, "error in creating flow");
			}

			flow_list[flow_index++] = flow;

			if (i && !((i + 1) % iterations_number)) {
				/* Save the insertion rate of each iter */
				end_iter = clock();
				delta = (double) (end_iter - start_iter);
				iter_id = ((i + 1) / iterations_number) - 1;
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
				delta = (double)(iterations_number /
					cpu_time_per_iter[i]);
				flows_rate = delta / 1000;
				printf(":: Iteration #%d: %d flows "
					"in %f sec[ Rate = %f K/Sec ]\n",
					i, iterations_number,
					cpu_time_per_iter[i], flows_rate);
			}

		/* Insertion rate for all flows */
		flows_rate = ((double) (flows_count / cpu_time_used) / 1000);
		printf("\n:: Total flow insertion rate -> %f K/Sec\n",
						flows_rate);
		printf(":: The time for creating %d in flows %f seconds\n",
						flows_count, cpu_time_used);

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

static void
init_port(void)
{
	int ret;
	uint16_t std_queue;
	uint16_t hairpin_q;
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
	if (hairpinq != 0)
		nr_queues = RXQ_NUM + hairpinq;

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

		if (hairpinq != 0) {
			for (hairpin_q = RXQ_NUM, std_queue = 0;
					std_queue < nr_queues;
					hairpin_q++, std_queue++) {
				hairpin_conf.peers[0].port = port_id;
				hairpin_conf.peers[0].queue =
					std_queue + TXQ_NUM;
				ret = rte_eth_rx_hairpin_queue_setup(
						port_id, hairpin_q,
						NR_RXD, &hairpin_conf);
				if (ret != 0)
					rte_exit(EXIT_FAILURE,
						":: Hairpin rx queue setup failed: err=%d, port=%u\n",
						ret, port_id);
			}

			for (hairpin_q = TXQ_NUM, std_queue = 0;
					std_queue < nr_queues;
					hairpin_q++, std_queue++) {
				hairpin_conf.peers[0].port = port_id;
				hairpin_conf.peers[0].queue =
					std_queue + RXQ_NUM;
				ret = rte_eth_tx_hairpin_queue_setup(
						port_id, hairpin_q,
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

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "EAL init failed\n");

	force_quit = false;
	dump_iterations = false;
	flows_count = DEFAULT_RULES_COUNT;
	iterations_number = DEFAULT_ITERATION;
	delete_flag = false;
	flow_group = 0;

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

	flows_handler();

	RTE_ETH_FOREACH_DEV(port) {
		rte_flow_flush(port, &error);
		rte_eth_dev_stop(port);
		rte_eth_dev_close(port);
	}
	return 0;
}
