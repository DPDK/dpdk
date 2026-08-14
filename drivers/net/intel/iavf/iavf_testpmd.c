/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation.
 */

#include <stdlib.h>

#include <rte_pmd_iavf.h>

#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>

#include "iavf.h"
#include "testpmd.h"
#include "iavf_rxtx.h"

struct cmd_reinit_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t reinit;
	portid_t port_id;
};

static cmdline_parse_token_string_t cmd_reinit_port =
	TOKEN_STRING_INITIALIZER(struct cmd_reinit_result,
		port, "port");
static cmdline_parse_token_string_t cmd_reinit_reinit =
	TOKEN_STRING_INITIALIZER(struct cmd_reinit_result,
		reinit, "reinit");
static cmdline_parse_token_num_t cmd_reinit_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_reinit_result,
		port_id, RTE_UINT16);

static void
cmd_reinit_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_reinit_result *res = parsed_result;
	int ret;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	ret = rte_pmd_iavf_reinit(res->port_id);
	if (ret < 0)
		fprintf(stderr, "Request to reinit VF failed for port %u: %s\n",
			res->port_id, rte_strerror(-ret));
	else
		printf("VF reinit requested for port %u\n", res->port_id);
}

static cmdline_parse_inst_t cmd_reinit = {
	.f = cmd_reinit_parsed,
	.data = NULL,
	.help_str = "port reinit <port_id>",
	.tokens = {
		(void *)&cmd_reinit_port,
		(void *)&cmd_reinit_reinit,
		(void *)&cmd_reinit_port_id,
		NULL,
	},
};

static struct testpmd_driver_commands iavf_cmds = {
	.commands = {
	{
		&cmd_reinit,
		"port reinit (port_id)\n"
		"    Send a request to the PF to reset the VF, then restore the port\n\n",
	},
	{ NULL, NULL },
	},
};
TESTPMD_ADD_DRIVER_COMMANDS(iavf_cmds)
