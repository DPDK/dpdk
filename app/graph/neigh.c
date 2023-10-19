/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_socket.h>
#include <rte_ethdev.h>
#include <rte_node_ip4_api.h>
#include <rte_node_ip6_api.h>

#include "neigh_priv.h"
#include "module_api.h"

static const char
cmd_neigh_v4_help[] = "neigh add ipv4 <ip> <mac>";

static const char
cmd_neigh_v6_help[] = "neigh add ipv6 <ip> <mac>";

struct neigh4_head neigh4 = TAILQ_HEAD_INITIALIZER(neigh4);
struct neigh6_head neigh6 = TAILQ_HEAD_INITIALIZER(neigh6);

void
neigh4_list_clean(void)
{
	struct neigh_ipv4_config *v4_config;

	while (!TAILQ_EMPTY(&neigh4)) {
		v4_config = TAILQ_FIRST(&neigh4);
		TAILQ_REMOVE(&neigh4, v4_config, next);
	}
}

void
neigh6_list_clean(void)
{
	struct neigh_ipv6_config *v6_config;

	while (!TAILQ_EMPTY(&neigh6)) {
		v6_config = TAILQ_FIRST(&neigh6);
		TAILQ_REMOVE(&neigh6, v6_config, next);
	}
}

static struct neigh_ipv4_config *
find_neigh4_entry(uint32_t ip, uint64_t mac)
{
	struct neigh_ipv4_config *v4_config;

	TAILQ_FOREACH(v4_config, &neigh4, next) {
		if ((v4_config->ip == ip) && (v4_config->mac == mac))
			return v4_config;
	}
	return NULL;
}

static struct neigh_ipv6_config *
find_neigh6_entry(uint8_t *ip, uint64_t mac)
{
	struct neigh_ipv6_config *v6_config;

	TAILQ_FOREACH(v6_config, &neigh6, next) {
		if (!(memcmp(v6_config->ip, ip, 16)) && (v6_config->mac == mac))
			return v6_config;
	}
	return NULL;
}

static int
ip6_rewrite_node_add(struct neigh_ipv6_config *v6_config)
{
	uint8_t data[2 * RTE_ETHER_ADDR_LEN];
	uint8_t len = 2 * RTE_ETHER_ADDR_LEN;
	struct rte_ether_addr smac;
	int16_t portid = 0;
	int rc;

	portid = ethdev_portid_by_ip6(v6_config->ip, NULL);
	if (portid < 0) {
		printf("Invalid portid found to add neigh\n");
		return -EINVAL;
	}

	memset(data, 0, len);

	/* Copy dst mac */
	rte_memcpy((void *)&data[0], (void *)&v6_config->mac, RTE_ETHER_ADDR_LEN);

	/* Copy src mac */
	rc = rte_eth_macaddr_get(portid, &smac);
	if (rc < 0)
		return rc;

	rte_memcpy(&data[RTE_ETHER_ADDR_LEN], smac.addr_bytes, RTE_ETHER_ADDR_LEN);

	return rte_node_ip6_rewrite_add(portid, data, len, portid);
}

static int
ip4_rewrite_node_add(struct neigh_ipv4_config *v4_config)
{
	uint8_t data[2 * RTE_ETHER_ADDR_LEN];
	uint8_t len = 2 * RTE_ETHER_ADDR_LEN;
	struct rte_ether_addr smac;
	int16_t portid = 0;
	int rc;

	portid = ethdev_portid_by_ip4(v4_config->ip, 0);
	if (portid < 0) {
		printf("Invalid portid found to add  neigh\n");
		return -EINVAL;
	}

	memset(data, 0, len);

	/* Copy dst mac */
	rte_memcpy((void *)&data[0], (void *)&v4_config->mac, RTE_ETHER_ADDR_LEN);

	/* Copy src mac */
	rc = rte_eth_macaddr_get(portid, &smac);
	if (rc < 0) {
		printf("Cannot get MAC address: err=%d, port=%d\n", rc, portid);
		return rc;
	}

	rte_memcpy(&data[RTE_ETHER_ADDR_LEN], smac.addr_bytes, RTE_ETHER_ADDR_LEN);

	return rte_node_ip4_rewrite_add(portid, data, len, portid);
}


static int
neigh_ip4_add(uint32_t ip, uint64_t mac)
{
	struct neigh_ipv4_config *v4_config;
	int rc = -EINVAL;

	v4_config = find_neigh4_entry(ip, mac);

	if (!v4_config) {
		v4_config = malloc(sizeof(struct neigh_ipv4_config));
		if (!v4_config)
			return -ENOMEM;
	}

	v4_config->ip = ip;
	v4_config->mac = mac;
	v4_config->is_used = true;

	if (!graph_status_get())
		goto exit;

	rc = ip4_rewrite_node_add(v4_config);
	if (rc)
		goto free;

exit:
	TAILQ_INSERT_TAIL(&neigh4, v4_config, next);
	return 0;
free:
	free(v4_config);
	return rc;
}

static int
neigh_ip6_add(uint8_t *ip, uint64_t mac)
{
	struct neigh_ipv6_config *v6_config;
	int rc = -EINVAL;
	int j;

	v6_config = find_neigh6_entry(ip, mac);

	if (!v6_config) {
		v6_config = malloc(sizeof(struct neigh_ipv6_config));
		if (!v6_config)
			return -ENOMEM;
	}

	for (j = 0; j < ETHDEV_IPV6_ADDR_LEN; j++)
		v6_config->ip[j] = ip[j];

	v6_config->mac = mac;
	v6_config->is_used = true;

	if (!graph_status_get())
		goto exit;

	rc =  ip6_rewrite_node_add(v6_config);
	if (rc)
		goto free;

exit:
	TAILQ_INSERT_TAIL(&neigh6, v6_config, next);
	return 0;
free:
	free(v6_config);
	return rc;
}

int
neigh_ip4_add_to_rewrite(void)
{
	struct neigh_ipv4_config *neigh;
	int rc;

	TAILQ_FOREACH(neigh, &neigh4, next) {
		rc = ip4_rewrite_node_add(neigh);
		if (rc)
			return rc;
	}
	return 0;
}

int
neigh_ip6_add_to_rewrite(void)
{
	struct neigh_ipv6_config *neigh;
	int rc;


	TAILQ_FOREACH(neigh, &neigh6, next) {
		rc = ip6_rewrite_node_add(neigh);
		if (rc < 0)
			return rc;
	}

	return 0;
}

static void
cli_neigh_v4(void *parsed_result, __rte_unused struct cmdline *cl, void *data __rte_unused)
{
	struct neigh_v4_cmd_tokens *res = parsed_result;
	int rc = -EINVAL;
	uint64_t mac;
	uint32_t ip;

	if (parser_ip4_read(&ip, res->ip)) {
		printf(MSG_ARG_INVALID, "ip");
		return;
	}

	if (parser_mac_read(&mac, res->mac)) {
		printf(MSG_ARG_INVALID, "mac");
		return;
	}

	rc = neigh_ip4_add(ip, mac);
	if (rc < 0)
		printf(MSG_CMD_FAIL, res->cmd);
}

static void
cli_neigh_v6(void *parsed_result, __rte_unused struct cmdline *cl, void *data __rte_unused)
{
	struct neigh_v6_cmd_tokens *res = parsed_result;
	uint8_t ip[ETHDEV_IPV6_ADDR_LEN];
	int rc = -EINVAL;
	uint64_t mac;

	if (parser_ip6_read(ip, res->ip)) {
		printf(MSG_ARG_INVALID, "ip");
		return;
	}

	if (parser_mac_read(&mac, res->mac)) {
		printf(MSG_ARG_INVALID, "mac");
		return;
	}

	rc = neigh_ip6_add(ip, mac);
	if (rc < 0)
		printf(MSG_CMD_FAIL, res->cmd);
}

static void
cli_neigh_help(__rte_unused void *parsed_result, __rte_unused struct cmdline *cl,
	       __rte_unused void *data)
{
	size_t len;

	len = strlen(conn->msg_out);
	conn->msg_out += len;
	snprintf(conn->msg_out, conn->msg_out_len_max, "\n%s\n%s\n%s\n",
		 "--------------------------- neigh command help ---------------------------",
		 cmd_neigh_v4_help, cmd_neigh_v6_help);

	len = strlen(conn->msg_out);
	conn->msg_out_len_max -= len;
}

cmdline_parse_token_string_t neigh_v4_cmd =
	TOKEN_STRING_INITIALIZER(struct neigh_v4_cmd_tokens, cmd, "neigh");
cmdline_parse_token_string_t neigh_v4_add =
	TOKEN_STRING_INITIALIZER(struct neigh_v4_cmd_tokens, add, "add");
cmdline_parse_token_string_t neigh_v4_ip4 =
	TOKEN_STRING_INITIALIZER(struct neigh_v4_cmd_tokens, ip4, "ipv4");
cmdline_parse_token_string_t neigh_v4_ip =
	TOKEN_STRING_INITIALIZER(struct neigh_v4_cmd_tokens, ip, NULL);
cmdline_parse_token_string_t neigh_v4_mac =
	TOKEN_STRING_INITIALIZER(struct neigh_v4_cmd_tokens, mac, NULL);

cmdline_parse_inst_t neigh_v4_cmd_ctx = {
	.f = cli_neigh_v4,
	.data = NULL,
	.help_str = cmd_neigh_v4_help,
	.tokens = {
		(void *)&neigh_v4_cmd,
		(void *)&neigh_v4_add,
		(void *)&neigh_v4_ip4,
		(void *)&neigh_v4_ip,
		(void *)&neigh_v4_mac,
		NULL,
	},
};

cmdline_parse_token_string_t neigh_v6_cmd =
	TOKEN_STRING_INITIALIZER(struct neigh_v6_cmd_tokens, cmd, "neigh");
cmdline_parse_token_string_t neigh_v6_add =
	TOKEN_STRING_INITIALIZER(struct neigh_v6_cmd_tokens, add, "add");
cmdline_parse_token_string_t neigh_v6_ip6 =
	TOKEN_STRING_INITIALIZER(struct neigh_v6_cmd_tokens, ip6, "ipv6");
cmdline_parse_token_string_t neigh_v6_ip =
	TOKEN_STRING_INITIALIZER(struct neigh_v6_cmd_tokens, ip, NULL);
cmdline_parse_token_string_t neigh_v6_mac =
	TOKEN_STRING_INITIALIZER(struct neigh_v6_cmd_tokens, mac, NULL);

cmdline_parse_inst_t neigh_v6_cmd_ctx = {
	.f = cli_neigh_v6,
	.data = NULL,
	.help_str = cmd_neigh_v6_help,
	.tokens = {
		(void *)&neigh_v6_cmd,
		(void *)&neigh_v6_add,
		(void *)&neigh_v6_ip6,
		(void *)&neigh_v6_ip,
		(void *)&neigh_v6_mac,
		NULL,
	},
};

cmdline_parse_token_string_t neigh_help_cmd =
	TOKEN_STRING_INITIALIZER(struct neigh_help_cmd_tokens, cmd, "help");
cmdline_parse_token_string_t neigh_help_module =
	TOKEN_STRING_INITIALIZER(struct neigh_help_cmd_tokens, module, "neigh");

cmdline_parse_inst_t neigh_help_cmd_ctx = {
	.f = cli_neigh_help,
	.data = NULL,
	.help_str = "",
	.tokens = {
		(void *)&neigh_help_cmd,
		(void *)&neigh_help_module,
		NULL,
	},
};
