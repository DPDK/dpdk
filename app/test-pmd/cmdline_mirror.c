/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Stephen Hemminger <stephen@networkplumber.org>
 */

#include <stdlib.h>

#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>

#include <rte_ethdev.h>

#include "testpmd.h"
#include "cmdline_mirror.h"

/* *** Create MIRROR port Object *** */
struct cmd_create_port_mirror_result {
	cmdline_fixed_string_t create;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t mirror;
	uint16_t port_id;
	cmdline_fixed_string_t destination;
	uint16_t target_id;
};

static cmdline_parse_token_string_t cmd_create_port_mirror_create =
	TOKEN_STRING_INITIALIZER(struct cmd_create_port_mirror_result, create, "create");
static cmdline_parse_token_string_t cmd_create_port_mirror_port =
	TOKEN_STRING_INITIALIZER(struct cmd_create_port_mirror_result, port, "port");
static cmdline_parse_token_string_t cmd_create_port_mirror_mirror =
	TOKEN_STRING_INITIALIZER(struct cmd_create_port_mirror_result, mirror, "mirror");
static cmdline_parse_token_string_t cmd_create_port_mirror_destination =
	TOKEN_STRING_INITIALIZER(struct cmd_create_port_mirror_result, destination, "destination");
static cmdline_parse_token_num_t cmd_create_port_mirror_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_create_port_mirror_result, port_id, RTE_UINT16);
static cmdline_parse_token_num_t cmd_create_port_mirror_target_id =
	TOKEN_NUM_INITIALIZER(struct cmd_create_port_mirror_result, target_id, RTE_UINT16);

static void cmd_create_port_mirror_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	const struct cmd_create_port_mirror_result *res = parsed_result;
	/* TODO these should be set by command */
	struct rte_eth_mirror_conf mirror_conf = {
		.direction = RTE_ETH_MIRROR_DIRECTION_INGRESS | RTE_ETH_MIRROR_DIRECTION_EGRESS
	};
	int ret;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	if (port_id_is_invalid(res->target_id, ENABLED_WARN))
		return;

	mirror_conf.target = res->target_id;

	ret = rte_eth_add_mirror(res->port_id,  &mirror_conf);
	if (ret != 0)
		fprintf(stderr, "%s\n", rte_strerror(-ret));
}

cmdline_parse_inst_t cmd_create_port_mirror = {
	.f = cmd_create_port_mirror_parsed,
	.data = NULL,
	.help_str = "create port mirror <port_id> destination <port_id>",
	.tokens = {
		(void *)&cmd_create_port_mirror_create,
		(void *)&cmd_create_port_mirror_port,
		(void *)&cmd_create_port_mirror_mirror,
		(void *)&cmd_create_port_mirror_port_id,
		(void *)&cmd_create_port_mirror_destination,
		(void *)&cmd_create_port_mirror_target_id,
		NULL
	},
};

/* *** Delete Port Mirror Object *** */
struct cmd_delete_port_mirror_result {
	cmdline_fixed_string_t delete;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t mirror;
	uint16_t port_id;
	cmdline_fixed_string_t destination;
	uint16_t target_id;
};

static void cmd_delete_port_mirror_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	const struct cmd_delete_port_mirror_result *res = parsed_result;
	int ret;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	ret = rte_eth_remove_mirror(res->port_id, res->target_id);
	if (ret != 0)
		fprintf(stderr, "%s\n", rte_strerror(-ret));
}

static cmdline_parse_token_string_t cmd_delete_port_mirror_delete =
	TOKEN_STRING_INITIALIZER(struct cmd_delete_port_mirror_result, delete, "delete");
static cmdline_parse_token_string_t cmd_delete_port_mirror_port =
	TOKEN_STRING_INITIALIZER(struct cmd_delete_port_mirror_result, port, "port");
static cmdline_parse_token_string_t cmd_delete_port_mirror_mirror =
	TOKEN_STRING_INITIALIZER(struct cmd_delete_port_mirror_result, mirror, "mirror");
static cmdline_parse_token_num_t cmd_delete_port_mirror_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_delete_port_mirror_result, port_id, RTE_UINT16);
static cmdline_parse_token_string_t cmd_delete_port_mirror_destination =
	TOKEN_STRING_INITIALIZER(struct cmd_create_port_mirror_result, destination, "destination");
static cmdline_parse_token_num_t cmd_delete_port_mirror_target_id =
	TOKEN_NUM_INITIALIZER(struct cmd_delete_port_mirror_result, target_id, RTE_UINT16);

cmdline_parse_inst_t cmd_delete_port_mirror = {
	.f = cmd_delete_port_mirror_parsed,
	.data = NULL,
	.help_str = "delete port mirror <port_id> destination <port_id>",
	.tokens = {
		(void *)&cmd_delete_port_mirror_delete,
		(void *)&cmd_delete_port_mirror_port,
		(void *)&cmd_delete_port_mirror_mirror,
		(void *)&cmd_delete_port_mirror_port_id,
		(void *)&cmd_delete_port_mirror_destination,
		(void *)&cmd_delete_port_mirror_target_id,
		NULL
	},
};
