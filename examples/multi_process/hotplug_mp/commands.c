/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2023 Intel Corporation.
 */

#include <rte_bus.h>
#include <rte_ethdev.h>
#include "commands.h"

void cmd_help_parsed(__rte_unused void *parsed_result,
			    struct cmdline *cl,
			    __rte_unused void *data)
{
	cmdline_printf(cl,
		       "commands:\n"
		       "- attach <devargs>\n"
		       "- detach <devargs>\n"
		       "- list\n\n");
}

void
cmd_quit_parsed(__rte_unused void *parsed_result,
			    struct cmdline *cl,
			    __rte_unused void *data)
{
	cmdline_quit(cl);
}

void
cmd_list_parsed(__rte_unused void *parsed_result,
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

void
cmd_attach_parsed(void *parsed_result,
				  struct cmdline *cl,
				  __rte_unused void *data)
{
	struct cmd_attach_result *res = parsed_result;
	struct rte_devargs da;

	memset(&da, 0, sizeof(da));

	if (rte_devargs_parsef(&da, "%s", res->devargs)) {
		cmdline_printf(cl, "cannot parse devargs\n");
		return;
	}

	if (!rte_eal_hotplug_add(rte_bus_name(da.bus), da.name, da.args))
		cmdline_printf(cl, "attached device %s\n", da.name);
	else
		cmdline_printf(cl, "failed to attached device %s\n",
				da.name);
	rte_devargs_reset(&da);
}

void
cmd_detach_parsed(void *parsed_result,
				   struct cmdline *cl,
				   __rte_unused void *data)
{
	struct cmd_detach_result *res = parsed_result;
	struct rte_devargs da;

	memset(&da, 0, sizeof(da));

	if (rte_devargs_parsef(&da, "%s", res->devargs)) {
		cmdline_printf(cl, "cannot parse devargs\n");
		return;
	}

	printf("detaching...\n");
	if (!rte_eal_hotplug_remove(rte_bus_name(da.bus), da.name))
		cmdline_printf(cl, "detached device %s\n",
			da.name);
	else
		cmdline_printf(cl, "failed to detach device %s\n",
			da.name);
	rte_devargs_reset(&da);
}
