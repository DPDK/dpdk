/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#include <stdlib.h>
#include <string.h>

#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_socket.h>
#include <rte_ethdev.h>

#include "ethdev_rx_priv.h"
#include "module_api.h"

static const char
cmd_ethdev_rx_help[] = "ethdev_rx map port <ethdev_name> queue <q_num> core <core_id>";

static struct lcore_params lcore_params_array[ETHDEV_RX_LCORE_PARAMS_MAX];
struct rte_node_ethdev_config ethdev_conf[RTE_MAX_ETHPORTS];
struct lcore_params *lcore_params = lcore_params_array;
struct lcore_conf lcore_conf[RTE_MAX_LCORE];
uint16_t nb_lcore_params;

static void
rx_map_configure(uint8_t port_id, uint32_t queue, uint32_t core)
{
	uint8_t n_rx_queue;

	n_rx_queue = lcore_conf[core].n_rx_queue;
	lcore_conf[core].rx_queue_list[n_rx_queue].port_id = port_id;
	lcore_conf[core].rx_queue_list[n_rx_queue].queue_id = queue;
	lcore_conf[core].n_rx_queue++;
}

uint8_t
ethdev_rx_num_rx_queues_get(uint16_t port)
{
	int queue = -1;
	uint16_t i;

	for (i = 0; i < nb_lcore_params; ++i) {
		if (lcore_params[i].port_id == port) {
			if (lcore_params[i].queue_id == queue + 1)
				queue = lcore_params[i].queue_id;
			else
				rte_exit(EXIT_FAILURE,
					 "Queue ids of the port %d must be"
					 " in sequence and must start with 0\n",
					 lcore_params[i].port_id);
		}
	}

	return (uint8_t)(++queue);
}

static int
ethdev_rx_map_add(char *name, uint32_t queue, uint32_t core)
{
	uint64_t coremask;
	uint16_t port_id;
	int rc;

	if (nb_lcore_params >= ETHDEV_RX_LCORE_PARAMS_MAX)
		return -EINVAL;

	rc = rte_eth_dev_get_port_by_name(name, &port_id);
	if (rc)
		return -EINVAL;

	coremask = graph_coremask_get();

	if (!(coremask & (1 << core)))
		return -EINVAL;

	rx_map_configure(port_id, queue, core);

	lcore_params_array[nb_lcore_params].port_id = port_id;
	lcore_params_array[nb_lcore_params].queue_id = queue;
	lcore_params_array[nb_lcore_params].lcore_id = core;
	nb_lcore_params++;
	return 0;
}

void
cmd_help_ethdev_rx_parsed(__rte_unused void *parsed_result, __rte_unused struct cmdline *cl,
			  __rte_unused void *data)
{
	size_t len;

	len = strlen(conn->msg_out);
	conn->msg_out += len;
	snprintf(conn->msg_out, conn->msg_out_len_max, "\n%s\n%s\n",
		 "---------------------------- ethdev_rx command help ----------------------------",
		 cmd_ethdev_rx_help);

	len = strlen(conn->msg_out);
	conn->msg_out_len_max -= len;
}

void
cmd_ethdev_rx_map_port_parsed(void *parsed_result, __rte_unused struct cmdline *cl,
			      void *data __rte_unused)
{
	struct cmd_ethdev_rx_map_port_result *res = parsed_result;
	int rc = -EINVAL;

	rc = ethdev_rx_map_add(res->dev, res->qid, res->core_id);
	if (rc < 0) {
		cli_exit();
		printf(MSG_CMD_FAIL, res->ethdev_rx);
		rte_exit(EXIT_FAILURE, "input core is Invalid\n");
	}

}
