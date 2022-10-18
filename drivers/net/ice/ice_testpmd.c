/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation.
 */

#include <stdlib.h>

#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>

#include "testpmd.h"
#include "ice_ethdev.h"

/* Fixed size for ICE ddp runtime configure */
#define ICE_BUFF_SIZE	0x000c9000
#define ICE_SWITCH_BUFF_SIZE	(4 * 1024 * 1024)

/* Dump device ddp package, only for ice PF */
struct cmd_ddp_dump_result {
	cmdline_fixed_string_t ddp;
	cmdline_fixed_string_t dump;
	portid_t port_id;
	char filepath[];
};

cmdline_parse_token_string_t cmd_ddp_dump_ddp =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_dump_result, ddp, "ddp");
cmdline_parse_token_string_t cmd_ddp_dump_dump =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_dump_result, dump, "dump");
cmdline_parse_token_num_t cmd_ddp_dump_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_ddp_dump_result, port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_ddp_dump_filepath =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_dump_result, filepath, NULL);

static void
cmd_ddp_dump_parsed(void *parsed_result,
		    __rte_unused struct cmdline *cl,
		    __rte_unused void *data)
{
	struct cmd_ddp_dump_result *res = parsed_result;
	uint8_t *buff;
	uint32_t size;
	int ret = -ENOTSUP;

	size = ICE_BUFF_SIZE;
	buff = (uint8_t *)malloc(ICE_BUFF_SIZE);
	if (buff) {
		ret = rte_pmd_ice_dump_package(res->port_id, &buff, &size);
		switch (ret) {
		case 0:
			save_file(res->filepath, buff, size);
			break;
		case -EINVAL:
			fprintf(stderr, "Invalid buffer size\n");
			break;
		case -ENOTSUP:
			fprintf(stderr,
				"Device doesn't support "
				"dump DDP runtime configure.\n");
			break;
		default:
			fprintf(stderr,
				"Failed to dump DDP runtime configure,"
				" error: (%s)\n", strerror(-ret));
		}
	}
	free(buff);
}

cmdline_parse_inst_t cmd_ddp_dump = {
	.f = cmd_ddp_dump_parsed,
	.data = NULL,
	.help_str = "ddp dump <port_id> <config_path>",
	.tokens = {
		(void *)&cmd_ddp_dump_ddp,
		(void *)&cmd_ddp_dump_dump,
		(void *)&cmd_ddp_dump_port_id,
		(void *)&cmd_ddp_dump_filepath,
		NULL,
	},
};

struct cmd_ddp_dump_switch_result {
	cmdline_fixed_string_t ddp;
	cmdline_fixed_string_t dump;
	cmdline_fixed_string_t swt;
	portid_t port_id;
	char filepath[];
};

cmdline_parse_token_string_t cmd_ddp_dump_swt_ddp =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_dump_switch_result, ddp, "ddp");
cmdline_parse_token_string_t cmd_ddp_dump_swt_dump =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_dump_switch_result, dump, "dump");
cmdline_parse_token_string_t cmd_ddp_dump_swt_switch =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_dump_switch_result, swt, "switch");
cmdline_parse_token_num_t cmd_ddp_dump_swt_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_ddp_dump_switch_result, port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_ddp_dump_swt_filepath =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_dump_switch_result, filepath, NULL);

static void
cmd_ddp_dump_switch_parsed(void *parsed_result,
			   __rte_unused struct cmdline *cl,
			   __rte_unused void *data)
{
	struct cmd_ddp_dump_switch_result *res = parsed_result;
	uint8_t *buff;
	uint32_t size;
	int ret = -ENOTSUP;

	size = ICE_SWITCH_BUFF_SIZE;
	buff = malloc(size);
	if (buff) {
		ret = rte_pmd_ice_dump_switch(res->port_id, &buff, &size);
		switch (ret) {
		case 0:
			save_file(res->filepath, buff, size);
			break;
		case -EINVAL:
			fprintf(stderr, "Invalid buffer size\n");
			break;
		case -ENOTSUP:
			fprintf(stderr,
				"Device doesn't support "
				"dump DDP switch runtime configure.\n");
			break;
		default:
			fprintf(stderr,
				"Failed to dump DDP switch runtime configure,"
				" error: (%s)\n", strerror(-ret));
		}
	}
	free(buff);
}


cmdline_parse_inst_t cmd_ddp_dump_switch = {
	.f = cmd_ddp_dump_switch_parsed,
	.data = NULL,
	.help_str = "ddp dump switch <port_id> <config_path>",
	.tokens = {
		(void *)&cmd_ddp_dump_swt_ddp,
		(void *)&cmd_ddp_dump_swt_dump,
		(void *)&cmd_ddp_dump_swt_switch,
		(void *)&cmd_ddp_dump_swt_port_id,
		(void *)&cmd_ddp_dump_swt_filepath,
		NULL,
	},
};

static struct testpmd_driver_commands ice_cmds = {
	.commands = {
	{
		&cmd_ddp_dump,
		"ddp dump (port_id) (config_path)\n"
		"    Dump a runtime configure on a port\n\n",

	},
	{
		&cmd_ddp_dump_switch,
		"ddp dump switch (port_id) (config_path)\n"
		"    Dump a runtime switch configure on a port\n\n",

	},
	{ NULL, NULL },
	},
};
TESTPMD_ADD_DRIVER_COMMANDS(ice_cmds)
