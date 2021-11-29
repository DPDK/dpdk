/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation.
 */

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_ipaddr.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline.h>
#include <rte_ethdev.h>

/**********************************************************/

struct cmd_help_result {
	cmdline_fixed_string_t help;
};

static void cmd_help_parsed(__rte_unused void *parsed_result,
			    struct cmdline *cl,
			    __rte_unused void *data)
{
	cmdline_printf(cl,
		       "commands:\n"
		       "- attach <devargs>\n"
		       "- detach <devargs>\n"
		       "- list\n\n");
}

cmdline_parse_token_string_t cmd_help_help =
	TOKEN_STRING_INITIALIZER(struct cmd_help_result, help, "help");

cmdline_parse_inst_t cmd_help = {
	.f = cmd_help_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "show help",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_help_help,
		NULL,
	},
};

/**********************************************************/

struct cmd_quit_result {
	cmdline_fixed_string_t quit;
};

static void cmd_quit_parsed(__rte_unused void *parsed_result,
			    struct cmdline *cl,
			    __rte_unused void *data)
{
	cmdline_quit(cl);
}

cmdline_parse_token_string_t cmd_quit_quit =
	TOKEN_STRING_INITIALIZER(struct cmd_quit_result, quit, "quit");

cmdline_parse_inst_t cmd_quit = {
	.f = cmd_quit_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "quit",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_quit_quit,
		NULL,
	},
};

/**********************************************************/

struct cmd_list_result {
	cmdline_fixed_string_t list;
};

static void cmd_list_parsed(__rte_unused void *parsed_result,
			    struct cmdline *cl,
			    __rte_unused void *data)
{
	uint16_t port_id;
	char dev_name[RTE_DEV_NAME_MAX_LEN];

	cmdline_printf(cl, "list all etherdev\n");

	RTE_ETH_FOREACH_DEV(port_id) {
		rte_eth_dev_get_name_by_port(port_id, dev_name);
		if (strlen(dev_name) > 0)
			cmdline_printf(cl, "%d\t%s\n", port_id, dev_name);
		else
			printf("empty dev_name is not expected!\n");
	}
}

cmdline_parse_token_string_t cmd_list_list =
	TOKEN_STRING_INITIALIZER(struct cmd_list_result, list, "list");

cmdline_parse_inst_t cmd_list = {
	.f = cmd_list_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "list all devices",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_list_list,
		NULL,
	},
};

/**********************************************************/

struct cmd_dev_attach_result {
	cmdline_fixed_string_t attach;
	cmdline_fixed_string_t devargs;
};

static void cmd_dev_attach_parsed(void *parsed_result,
				  struct cmdline *cl,
				  __rte_unused void *data)
{
	struct cmd_dev_attach_result *res = parsed_result;
	struct rte_devargs da;

	memset(&da, 0, sizeof(da));

	if (rte_devargs_parsef(&da, "%s", res->devargs)) {
		cmdline_printf(cl, "cannot parse devargs\n");
		return;
	}

	if (!rte_eal_hotplug_add(da.bus->name, da.name, da.args))
		cmdline_printf(cl, "attached device %s\n", da.name);
	else
		cmdline_printf(cl, "failed to attached device %s\n",
				da.name);
	rte_devargs_reset(&da);
}

cmdline_parse_token_string_t cmd_dev_attach_attach =
	TOKEN_STRING_INITIALIZER(struct cmd_dev_attach_result, attach,
				 "attach");
cmdline_parse_token_string_t cmd_dev_attach_devargs =
	TOKEN_STRING_INITIALIZER(struct cmd_dev_attach_result, devargs, NULL);

cmdline_parse_inst_t cmd_attach_device = {
	.f = cmd_dev_attach_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "attach a device",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_dev_attach_attach,
		(void *)&cmd_dev_attach_devargs,
		NULL,
	},
};

/**********************************************************/

struct cmd_dev_detach_result {
	cmdline_fixed_string_t detach;
	cmdline_fixed_string_t devargs;
};

static void cmd_dev_detach_parsed(void *parsed_result,
				   struct cmdline *cl,
				   __rte_unused void *data)
{
	struct cmd_dev_detach_result *res = parsed_result;
	struct rte_devargs da;

	memset(&da, 0, sizeof(da));

	if (rte_devargs_parsef(&da, "%s", res->devargs)) {
		cmdline_printf(cl, "cannot parse devargs\n");
		return;
	}

	printf("detaching...\n");
	if (!rte_eal_hotplug_remove(da.bus->name, da.name))
		cmdline_printf(cl, "detached device %s\n",
			da.name);
	else
		cmdline_printf(cl, "failed to detach device %s\n",
			da.name);
	rte_devargs_reset(&da);
}

cmdline_parse_token_string_t cmd_dev_detach_detach =
	TOKEN_STRING_INITIALIZER(struct cmd_dev_detach_result, detach,
				 "detach");

cmdline_parse_token_string_t cmd_dev_detach_devargs =
	TOKEN_STRING_INITIALIZER(struct cmd_dev_detach_result, devargs, NULL);

cmdline_parse_inst_t cmd_detach_device = {
	.f = cmd_dev_detach_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "detach a device",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_dev_detach_detach,
		(void *)&cmd_dev_detach_devargs,
		NULL,
	},
};

/**********************************************************/
/**********************************************************/
/****** CONTEXT (list of instruction) */

cmdline_parse_ctx_t main_ctx[] = {
	(cmdline_parse_inst_t *)&cmd_help,
	(cmdline_parse_inst_t *)&cmd_quit,
	(cmdline_parse_inst_t *)&cmd_list,
	(cmdline_parse_inst_t *)&cmd_attach_device,
	(cmdline_parse_inst_t *)&cmd_detach_device,
	NULL,
};
