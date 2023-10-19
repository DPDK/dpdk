/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_socket.h>
#include <rte_ethdev.h>
#include <rte_graph_worker.h>
#include <rte_log.h>

#include "graph_priv.h"
#include "module_api.h"

#define RTE_LOGTYPE_APP_GRAPH RTE_LOGTYPE_USER1

static const char
cmd_graph_help[] = "graph <usecases> bsz <size> tmo <ns> coremask <bitmask> "
		   "model <rtc | mcd | default> pcap_enable <0 | 1> num_pcap_pkts <num>"
		   "pcap_file <output_capture_file>";

static const char * const supported_usecases[] = {"l3fwd"};
struct graph_config graph_config;
bool graph_started;

/* Check the link rc of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90  /* 9s (90 * 100ms) in total */
	char link_rc_text[RTE_ETH_LINK_MAX_STR_LEN];
	uint8_t count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;
	uint16_t portid;
	int rc;

	printf("\nChecking link status...");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		if (force_quit)
			return;

		all_ports_up = 1;
		RTE_ETH_FOREACH_DEV(portid)
		{
			if (force_quit)
				return;

			if ((port_mask & (1 << portid)) == 0)
				continue;

			memset(&link, 0, sizeof(link));
			rc = rte_eth_link_get_nowait(portid, &link);
			if (rc < 0) {
				all_ports_up = 0;
				if (print_flag == 1)
					printf("Port %u link get failed: %s\n",
					       portid, rte_strerror(-rc));
				continue;
			}

			/* Print link rc if flag set */
			if (print_flag == 1) {
				rte_eth_link_to_str(link_rc_text, sizeof(link_rc_text),
					&link);
				printf("Port %d %s\n", portid, link_rc_text);
				continue;
			}

			/* Clear all_ports_up flag if any link down */
			if (link.link_status == RTE_ETH_LINK_DOWN) {
				all_ports_up = 0;
				break;
			}
		}

		/* After finally printing all link rc, get out */
		if (print_flag == 1)
			break;

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* Set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf("Done\n");
		}
	}
}

static bool
parser_usecases_read(char *usecases)
{
	bool valid = false;
	uint32_t i, j = 0;
	char *token;

	token = strtok(usecases, ",");
	while (token != NULL) {
		for (i = 0; i < RTE_DIM(supported_usecases); i++) {
			if (strcmp(supported_usecases[i], token) == 0) {
				graph_config.usecases[j].enabled = true;
				rte_strscpy(graph_config.usecases[j].name, token, 31);
				valid = true;
				j++;
				break;
			}
		}
		token = strtok(NULL, ",");
	}

	return valid;
}

static uint64_t
graph_worker_count_get(void)
{
	uint64_t nb_worker = 0;
	uint64_t coremask;

	coremask = graph_config.params.coremask;
	while (coremask) {
		if (coremask & 0x1)
			nb_worker++;

		coremask = (coremask >> 1);
	}

	return nb_worker;
}

static struct rte_node_ethdev_config *
graph_rxtx_node_config_get(uint32_t *num_conf, uint32_t *num_graphs)
{
	uint32_t n_tx_queue, nb_conf = 0, lcore_id;
	uint16_t queueid, portid, nb_graphs = 0;
	uint8_t nb_rx_queue, queue;
	struct lcore_conf *qconf;

	n_tx_queue = graph_worker_count_get();
	if (n_tx_queue > RTE_MAX_ETHPORTS)
		n_tx_queue = RTE_MAX_ETHPORTS;

	RTE_ETH_FOREACH_DEV(portid) {
		/* Skip ports that are not enabled */
		if ((enabled_port_mask & (1 << portid)) == 0) {
			printf("\nSkipping disabled port %d\n", portid);
			continue;
		}

		nb_rx_queue = ethdev_rx_num_rx_queues_get(portid);

		/* Setup ethdev node config */
		ethdev_conf[nb_conf].port_id = portid;
		ethdev_conf[nb_conf].num_rx_queues = nb_rx_queue;
		ethdev_conf[nb_conf].num_tx_queues = n_tx_queue;
		ethdev_conf[nb_conf].mp = ethdev_mempool_list_by_portid(portid);
		ethdev_conf[nb_conf].mp_count = 1; /* Check with pools */

		nb_conf++;
	}

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;

		qconf = &lcore_conf[lcore_id];
		printf("\nInitializing rx queues on lcore %u ... ", lcore_id);
		fflush(stdout);

		/* Init RX queues */
		for (queue = 0; queue < qconf->n_rx_queue; ++queue) {
			portid = qconf->rx_queue_list[queue].port_id;
			queueid = qconf->rx_queue_list[queue].queue_id;

			/* Add this queue node to its graph */
			snprintf(qconf->rx_queue_list[queue].node_name, RTE_NODE_NAMESIZE,
				 "ethdev_rx-%u-%u", portid, queueid);
		}
		if (qconf->n_rx_queue)
			nb_graphs++;
	}

	printf("\n");

	ethdev_start();
	check_all_ports_link_status(enabled_port_mask);

	*num_conf = nb_conf;
	*num_graphs = nb_graphs;
	return ethdev_conf;
}

static void
graph_stats_print_to_file(void)
{
	struct rte_graph_cluster_stats_param s_param;
	struct rte_graph_cluster_stats *stats;
	const char *pattern = "worker_*";
	FILE *fp = NULL;
	size_t sz, len;

	/* Prepare stats object */
	fp = fopen("/tmp/graph_stats.txt", "w+");
	if (fp == NULL)
		rte_exit(EXIT_FAILURE, "Error in opening stats file\n");

	memset(&s_param, 0, sizeof(s_param));
	s_param.f = fp;
	s_param.socket_id = SOCKET_ID_ANY;
	s_param.graph_patterns = &pattern;
	s_param.nb_graph_patterns = 1;

	stats = rte_graph_cluster_stats_create(&s_param);
	if (stats == NULL)
		rte_exit(EXIT_FAILURE, "Unable to create stats object\n");

	/* Clear screen and move to top left */
	rte_graph_cluster_stats_get(stats, 0);
	rte_delay_ms(1E3);

	fseek(fp, 0L, SEEK_END);
	sz = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	len = strlen(conn->msg_out);
	conn->msg_out += len;

	sz = fread(conn->msg_out, sizeof(char), sz, fp);
	len = strlen(conn->msg_out);
	conn->msg_out_len_max -= len;
	rte_graph_cluster_stats_destroy(stats);

	fclose(fp);
}

static void
cli_graph_stats(__rte_unused void *parsed_result, __rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	graph_stats_print_to_file();
}

bool
graph_status_get(void)
{
	return graph_started;
}

static void
cli_graph_start(__rte_unused void *parsed_result, __rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct rte_node_ethdev_config *conf;
	uint32_t nb_graphs = 0, nb_conf, i;
	int rc = -EINVAL;

	conf = graph_rxtx_node_config_get(&nb_conf, &nb_graphs);
	for (i = 0; i < MAX_GRAPH_USECASES; i++) {
		if (!strcmp(graph_config.usecases[i].name, "l3fwd")) {
			if (graph_config.usecases[i].enabled) {
				rc  = usecase_l3fwd_configure(conf, nb_conf, nb_graphs);
				break;
			}
		}
	}

	if (!rc)
		graph_started = true;
}

static int
graph_config_add(char *usecases, struct graph_config *config)
{
	uint64_t lcore_id, core_num;
	uint64_t eal_coremask = 0;

	if (!parser_usecases_read(usecases))
		return -EINVAL;

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id))
			eal_coremask |= RTE_BIT64(lcore_id);
	}

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		core_num = 1 << lcore_id;
		if (config->params.coremask & core_num) {
			if (eal_coremask & core_num)
				continue;
			else
				return -EINVAL;
		}
	}

	graph_config.params.bsz = config->params.bsz;
	graph_config.params.tmo = config->params.tmo;
	graph_config.params.coremask = config->params.coremask;
	graph_config.model = config->model;
	graph_config.pcap_ena = config->pcap_ena;
	graph_config.num_pcap_pkts = config->num_pcap_pkts;
	graph_config.pcap_file = strdup(config->pcap_file);

	return 0;
}

void
graph_pcap_config_get(uint8_t *pcap_ena, uint64_t *num_pkts, char **file)
{

	*pcap_ena = graph_config.pcap_ena;
	*num_pkts = graph_config.num_pcap_pkts;
	*file = graph_config.pcap_file;
}

int
graph_walk_start(void *conf)
{
	struct lcore_conf *qconf;
	struct rte_graph *graph;
	uint32_t lcore_id;

	RTE_SET_USED(conf);

	lcore_id = rte_lcore_id();
	qconf = &lcore_conf[lcore_id];
	graph = qconf->graph;

	if (!graph) {
		RTE_LOG(INFO, APP_GRAPH, "Lcore %u has nothing to do\n", lcore_id);
		return 0;
	}

	RTE_LOG(INFO, APP_GRAPH, "Entering main loop on lcore %u, graph %s(%p)\n", lcore_id,
		qconf->name, graph);

	while (likely(!force_quit))
		rte_graph_walk(graph);

	return 0;
}

void
graph_stats_print(void)
{
	const char topLeft[] = {27, '[', '1', ';', '1', 'H', '\0'};
	const char clr[] = {27, '[', '2', 'J', '\0'};
	struct rte_graph_cluster_stats_param s_param;
	struct rte_graph_cluster_stats *stats;
	const char *pattern = "worker_*";

	/* Prepare stats object */
	memset(&s_param, 0, sizeof(s_param));
	s_param.f = stdout;
	s_param.socket_id = SOCKET_ID_ANY;
	s_param.graph_patterns = &pattern;
	s_param.nb_graph_patterns = 1;

	stats = rte_graph_cluster_stats_create(&s_param);
	if (stats == NULL)
		rte_exit(EXIT_FAILURE, "Unable to create stats object\n");

	while (!force_quit) {
		/* Clear screen and move to top left */
		printf("%s%s", clr, topLeft);
		rte_graph_cluster_stats_get(stats, 0);
		rte_delay_ms(1E3);
		if (app_graph_exit())
			force_quit = true;
	}

	rte_graph_cluster_stats_destroy(stats);
}

uint64_t
graph_coremask_get(void)
{
	return graph_config.params.coremask;
}

static void
cli_graph(void *parsed_result, __rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct graph_config_cmd_tokens *res = parsed_result;
	struct graph_config config;
	char *model_name;
	uint8_t model;
	int rc;

	model_name = res->model_name;
	if (strcmp(model_name, "default") == 0) {
		model = GRAPH_MODEL_RTC;
	} else if (strcmp(model_name, "rtc") == 0) {
		model = GRAPH_MODEL_RTC;
	} else if (strcmp(model_name, "mcd") == 0) {
		model = GRAPH_MODEL_MCD;
	} else {
		printf(MSG_ARG_NOT_FOUND, "model arguments");
		return;
	}

	config.params.bsz = res->size;
	config.params.tmo = res->ns;
	config.params.coremask = res->mask;
	config.model = model;
	config.pcap_ena = res->pcap_ena;
	config.num_pcap_pkts = res->num_pcap_pkts;
	config.pcap_file = res->pcap_file;
	rc = graph_config_add(res->usecase, &config);
	if (rc < 0) {
		cli_exit();
		printf(MSG_CMD_FAIL, res->graph);
		rte_exit(EXIT_FAILURE, "coremask is Invalid\n");
	}
}

static void
cli_graph_help(__rte_unused void *parsed_result, __rte_unused struct cmdline *cl,
	       __rte_unused void *data)
{
	size_t len;

	len = strlen(conn->msg_out);
	conn->msg_out += len;
	snprintf(conn->msg_out, conn->msg_out_len_max, "\n%s\n%s\n%s\n%s\n",
		 "----------------------------- graph command help -----------------------------",
		 cmd_graph_help, "graph start", "graph stats show");

	len = strlen(conn->msg_out);
	conn->msg_out_len_max -= len;
}

cmdline_parse_token_string_t graph_display_graph =
	TOKEN_STRING_INITIALIZER(struct graph_stats_cmd_tokens, graph, "graph");
cmdline_parse_token_string_t graph_display_stats =
	TOKEN_STRING_INITIALIZER(struct graph_stats_cmd_tokens, stats, "stats");
cmdline_parse_token_string_t graph_display_show =
	TOKEN_STRING_INITIALIZER(struct graph_stats_cmd_tokens, show, "show");

cmdline_parse_inst_t graph_stats_cmd_ctx = {
	.f = cli_graph_stats,
	.data = NULL,
	.help_str = "graph stats show",
	.tokens = {
		(void *)&graph_display_graph,
		(void *)&graph_display_stats,
		(void *)&graph_display_show,
		NULL,
	},
};

cmdline_parse_token_string_t graph_config_start_graph =
	TOKEN_STRING_INITIALIZER(struct graph_start_cmd_tokens, graph, "graph");
cmdline_parse_token_string_t graph_config_start =
	TOKEN_STRING_INITIALIZER(struct graph_start_cmd_tokens, start, "start");

cmdline_parse_inst_t graph_start_cmd_ctx = {
	.f = cli_graph_start,
	.data = NULL,
	.help_str = "graph start",
	.tokens = {
		(void *)&graph_config_start_graph,
		(void *)&graph_config_start,
		NULL,
	},
};

cmdline_parse_token_string_t graph_config_add_graph =
	TOKEN_STRING_INITIALIZER(struct graph_config_cmd_tokens, graph, "graph");
cmdline_parse_token_string_t graph_config_add_usecase =
	TOKEN_STRING_INITIALIZER(struct graph_config_cmd_tokens, usecase, NULL);
cmdline_parse_token_string_t graph_config_add_coremask =
	TOKEN_STRING_INITIALIZER(struct graph_config_cmd_tokens, coremask, "coremask");
cmdline_parse_token_num_t graph_config_add_mask =
	TOKEN_NUM_INITIALIZER(struct graph_config_cmd_tokens, mask, RTE_UINT64);
cmdline_parse_token_string_t graph_config_add_bsz =
	TOKEN_STRING_INITIALIZER(struct graph_config_cmd_tokens, bsz, "bsz");
cmdline_parse_token_num_t graph_config_add_size =
	TOKEN_NUM_INITIALIZER(struct graph_config_cmd_tokens, size, RTE_UINT16);
cmdline_parse_token_string_t graph_config_add_tmo =
	TOKEN_STRING_INITIALIZER(struct graph_config_cmd_tokens, tmo, "tmo");
cmdline_parse_token_num_t graph_config_add_ns =
	TOKEN_NUM_INITIALIZER(struct graph_config_cmd_tokens, ns, RTE_UINT64);
cmdline_parse_token_string_t graph_config_add_model =
	TOKEN_STRING_INITIALIZER(struct graph_config_cmd_tokens, model, "model");
cmdline_parse_token_string_t graph_config_add_model_name =
	TOKEN_STRING_INITIALIZER(struct graph_config_cmd_tokens, model_name, "rtc#mcd#default");
cmdline_parse_token_string_t graph_config_add_capt_ena =
	TOKEN_STRING_INITIALIZER(struct graph_config_cmd_tokens, capt_ena, "pcap_enable");
cmdline_parse_token_num_t graph_config_add_pcap_ena =
	TOKEN_NUM_INITIALIZER(struct graph_config_cmd_tokens, pcap_ena, RTE_UINT8);
cmdline_parse_token_string_t graph_config_add_capt_pkts_count =
	TOKEN_STRING_INITIALIZER(struct graph_config_cmd_tokens, capt_pkts_count, "num_pcap_pkts");
cmdline_parse_token_num_t graph_config_add_num_pcap_pkts =
	TOKEN_NUM_INITIALIZER(struct graph_config_cmd_tokens, num_pcap_pkts, RTE_UINT64);
cmdline_parse_token_string_t graph_config_add_capt_file =
	TOKEN_STRING_INITIALIZER(struct graph_config_cmd_tokens, capt_file, "pcap_file");
cmdline_parse_token_string_t graph_config_add_pcap_file =
	TOKEN_STRING_INITIALIZER(struct graph_config_cmd_tokens, pcap_file, NULL);

cmdline_parse_inst_t graph_config_cmd_ctx = {
	.f = cli_graph,
	.data = NULL,
	.help_str = cmd_graph_help,
	.tokens = {
		(void *)&graph_config_add_graph,
		(void *)&graph_config_add_usecase,
		(void *)&graph_config_add_coremask,
		(void *)&graph_config_add_mask,
		(void *)&graph_config_add_bsz,
		(void *)&graph_config_add_size,
		(void *)&graph_config_add_tmo,
		(void *)&graph_config_add_ns,
		(void *)&graph_config_add_model,
		(void *)&graph_config_add_model_name,
		(void *)&graph_config_add_capt_ena,
		(void *)&graph_config_add_pcap_ena,
		(void *)&graph_config_add_capt_pkts_count,
		(void *)&graph_config_add_num_pcap_pkts,
		(void *)&graph_config_add_capt_file,
		(void *)&graph_config_add_pcap_file,
		NULL,
	},
};

cmdline_parse_token_string_t graph_help_cmd =
	TOKEN_STRING_INITIALIZER(struct graph_help_cmd_tokens, help, "help");
cmdline_parse_token_string_t graph_help_graph =
	TOKEN_STRING_INITIALIZER(struct graph_help_cmd_tokens, graph, "graph");

cmdline_parse_inst_t graph_help_cmd_ctx = {
	.f = cli_graph_help,
	.data = NULL,
	.help_str = "",
	.tokens = {
		(void *)&graph_help_cmd,
		(void *)&graph_help_graph,
		NULL,
	},
};
