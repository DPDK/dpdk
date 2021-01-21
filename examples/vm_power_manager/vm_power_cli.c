/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <errno.h>

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_string.h>
#include <cmdline_parse_num.h>
#include <cmdline_socket.h>
#include <cmdline.h>

#include "vm_power_cli.h"
#include "channel_manager.h"
#include "channel_monitor.h"
#include "power_manager.h"

struct cmd_quit_result {
	cmdline_fixed_string_t quit;
};

static void cmd_quit_parsed(__rte_unused void *parsed_result,
		struct cmdline *cl,
		__rte_unused void *data)
{
	channel_monitor_exit();
	channel_manager_exit();
	power_manager_exit();
	cmdline_quit(cl);
}

cmdline_parse_token_string_t cmd_quit_quit =
	TOKEN_STRING_INITIALIZER(struct cmd_quit_result, quit, "quit");

cmdline_parse_inst_t cmd_quit = {
	.f = cmd_quit_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "close the application",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_quit_quit,
		NULL,
	},
};

/* *** VM operations *** */
struct cmd_show_vm_result {
	cmdline_fixed_string_t show_vm;
	cmdline_fixed_string_t vm_name;
};

static void
cmd_show_vm_parsed(void *parsed_result, struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_show_vm_result *res = parsed_result;
	struct vm_info info;
	unsigned i;

	if (get_info_vm(res->vm_name, &info) != 0)
		return;
	cmdline_printf(cl, "VM: '%s', status = ", info.name);
	if (info.status == CHANNEL_MGR_VM_ACTIVE)
		cmdline_printf(cl, "ACTIVE\n");
	else
		cmdline_printf(cl, "INACTIVE\n");
	cmdline_printf(cl, "Channels %u\n", info.num_channels);
	for (i = 0; i < info.num_channels; i++) {
		cmdline_printf(cl, "  [%u]: %s, status = ", i,
				info.channels[i].channel_path);
		switch (info.channels[i].status) {
		case CHANNEL_MGR_CHANNEL_CONNECTED:
			cmdline_printf(cl, "CONNECTED\n");
			break;
		case CHANNEL_MGR_CHANNEL_DISCONNECTED:
			cmdline_printf(cl, "DISCONNECTED\n");
			break;
		case CHANNEL_MGR_CHANNEL_DISABLED:
			cmdline_printf(cl, "DISABLED\n");
			break;
		case CHANNEL_MGR_CHANNEL_PROCESSING:
			cmdline_printf(cl, "PROCESSING\n");
			break;
		default:
			cmdline_printf(cl, "UNKNOWN\n");
			break;
		}
	}
	cmdline_printf(cl, "Virtual CPU(s): %u\n", info.num_vcpus);
	for (i = 0; i < info.num_vcpus; i++) {
		cmdline_printf(cl, "  [%u]: Physical CPU %d\n", i,
				info.pcpu_map[i]);
	}
}



cmdline_parse_token_string_t cmd_vm_show =
	TOKEN_STRING_INITIALIZER(struct cmd_show_vm_result,
				show_vm, "show_vm");
cmdline_parse_token_string_t cmd_show_vm_name =
	TOKEN_STRING_INITIALIZER(struct cmd_show_vm_result,
			vm_name, NULL);

cmdline_parse_inst_t cmd_show_vm_set = {
	.f = cmd_show_vm_parsed,
	.data = NULL,
	.help_str = "show_vm <vm_name>, prints the information on the "
			"specified VM(s), the information lists the number of vCPUS, the "
			"pinning to pCPU(s) as a bit mask, along with any communication "
			"channels associated with each VM",
	.tokens = {
		(void *)&cmd_vm_show,
		(void *)&cmd_show_vm_name,
		NULL,
	},
};

/* *** vCPU to pCPU mapping operations *** */


struct cmd_set_pcpu_result {
	cmdline_fixed_string_t set_pcpu;
	cmdline_fixed_string_t vm_name;
	uint8_t vcpu;
	uint8_t core;
};

static void
cmd_set_pcpu_parsed(void *parsed_result, struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_set_pcpu_result *res = parsed_result;

	if (set_pcpu(res->vm_name, res->vcpu, res->core) == 0)
		cmdline_printf(cl, "Pinned vCPU(%"PRId8") to pCPU core "
				"%"PRId8")\n", res->vcpu, res->core);
	else
		cmdline_printf(cl, "Unable to pin vCPU(%"PRId8") to pCPU core "
				"%"PRId8")\n", res->vcpu, res->core);
}

cmdline_parse_token_string_t cmd_set_pcpu =
		TOKEN_STRING_INITIALIZER(struct cmd_set_pcpu_result,
				set_pcpu, "set_pcpu");
cmdline_parse_token_string_t cmd_set_pcpu_vm_name =
		TOKEN_STRING_INITIALIZER(struct cmd_set_pcpu_result,
				vm_name, NULL);
cmdline_parse_token_num_t set_pcpu_vcpu =
		TOKEN_NUM_INITIALIZER(struct cmd_set_pcpu_result,
				vcpu, RTE_UINT8);
cmdline_parse_token_num_t set_pcpu_core =
		TOKEN_NUM_INITIALIZER(struct cmd_set_pcpu_result,
				core, RTE_UINT64);


cmdline_parse_inst_t cmd_set_pcpu_set = {
		.f = cmd_set_pcpu_parsed,
		.data = NULL,
		.help_str = "set_pcpu <vm_name> <vcpu> <pcpu>, Set the binding "
				"of Virtual CPU on VM to the Physical CPU.",
				.tokens = {
						(void *)&cmd_set_pcpu,
						(void *)&cmd_set_pcpu_vm_name,
						(void *)&set_pcpu_vcpu,
						(void *)&set_pcpu_core,
						NULL,
		},
};

struct cmd_vm_op_result {
	cmdline_fixed_string_t op_vm;
	cmdline_fixed_string_t vm_name;
};

static void
cmd_vm_op_parsed(void *parsed_result, struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_vm_op_result *res = parsed_result;

	if (!strcmp(res->op_vm, "add_vm")) {
		if (add_vm(res->vm_name) < 0)
			cmdline_printf(cl, "Unable to add VM '%s'\n", res->vm_name);
	} else if (remove_vm(res->vm_name) < 0)
		cmdline_printf(cl, "Unable to remove VM '%s'\n", res->vm_name);
}

cmdline_parse_token_string_t cmd_vm_op =
	TOKEN_STRING_INITIALIZER(struct cmd_vm_op_result,
			op_vm, "add_vm#rm_vm");
cmdline_parse_token_string_t cmd_vm_name =
	TOKEN_STRING_INITIALIZER(struct cmd_vm_op_result,
			vm_name, NULL);

cmdline_parse_inst_t cmd_vm_op_set = {
	.f = cmd_vm_op_parsed,
	.data = NULL,
	.help_str = "add_vm|rm_vm <name>, add a VM for "
			"subsequent operations with the CLI or remove a previously added "
			"VM from the VM Power Manager",
	.tokens = {
		(void *)&cmd_vm_op,
		(void *)&cmd_vm_name,
	NULL,
	},
};

/* *** VM channel operations *** */
struct cmd_channels_op_result {
	cmdline_fixed_string_t op;
	cmdline_fixed_string_t vm_name;
	cmdline_fixed_string_t channel_list;
};
static void
cmd_channels_op_parsed(void *parsed_result, struct cmdline *cl,
			__rte_unused void *data)
{
	unsigned num_channels = 0, channel_num, i;
	int channels_added;
	unsigned int channel_list[RTE_MAX_LCORE];
	char *token, *remaining, *tail_ptr;
	struct cmd_channels_op_result *res = parsed_result;

	if (!strcmp(res->channel_list, "all")) {
		channels_added = add_all_channels(res->vm_name);
		cmdline_printf(cl, "Added %d channels for VM '%s'\n",
				channels_added, res->vm_name);
		return;
	}

	remaining = res->channel_list;
	while (1) {
		if (remaining == NULL || remaining[0] == '\0')
			break;

		token = strsep(&remaining, ",");
		if (token == NULL)
			break;
		errno = 0;
		channel_num = (unsigned)strtol(token, &tail_ptr, 10);
		if ((errno != 0) || tail_ptr == NULL || (*tail_ptr != '\0'))
			break;

		if (channel_num == RTE_MAX_LCORE) {
			cmdline_printf(cl, "Channel number '%u' exceeds the maximum number "
					"of allowable channels(%u) for VM '%s'\n", channel_num,
					RTE_MAX_LCORE, res->vm_name);
			return;
		}
		channel_list[num_channels++] = channel_num;
	}
	for (i = 0; i < num_channels; i++)
		cmdline_printf(cl, "[%u]: Adding channel %u\n", i, channel_list[i]);

	channels_added = add_channels(res->vm_name, channel_list,
			num_channels);
	cmdline_printf(cl, "Enabled %d channels for '%s'\n", channels_added,
			res->vm_name);
}

cmdline_parse_token_string_t cmd_channels_op =
	TOKEN_STRING_INITIALIZER(struct cmd_channels_op_result,
				op, "add_channels");
cmdline_parse_token_string_t cmd_channels_vm_name =
	TOKEN_STRING_INITIALIZER(struct cmd_channels_op_result,
			vm_name, NULL);
cmdline_parse_token_string_t cmd_channels_list =
	TOKEN_STRING_INITIALIZER(struct cmd_channels_op_result,
			channel_list, NULL);

cmdline_parse_inst_t cmd_channels_op_set = {
	.f = cmd_channels_op_parsed,
	.data = NULL,
	.help_str = "add_channels <vm_name> <list>|all, add "
			"communication channels for the specified VM, the "
			"virtio channels must be enabled in the VM "
			"configuration(qemu/libvirt) and the associated VM must be active. "
			"<list> is a comma-separated list of channel numbers to add, using "
			"the keyword 'all' will attempt to add all channels for the VM",
	.tokens = {
		(void *)&cmd_channels_op,
		(void *)&cmd_channels_vm_name,
		(void *)&cmd_channels_list,
		NULL,
	},
};

struct cmd_set_query_result {
	cmdline_fixed_string_t set_query;
	cmdline_fixed_string_t vm_name;
	cmdline_fixed_string_t query_status;
};

static void
cmd_set_query_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_set_query_result *res = parsed_result;

	if (!strcmp(res->query_status, "enable")) {
		if (set_query_status(res->vm_name, true) < 0)
			cmdline_printf(cl, "Unable to allow query for VM '%s'\n",
					res->vm_name);
	} else if (!strcmp(res->query_status, "disable")) {
		if (set_query_status(res->vm_name, false) < 0)
			cmdline_printf(cl, "Unable to disallow query for VM '%s'\n",
					res->vm_name);
	}
}

cmdline_parse_token_string_t cmd_set_query =
	TOKEN_STRING_INITIALIZER(struct cmd_set_query_result,
			set_query, "set_query");
cmdline_parse_token_string_t cmd_set_query_vm_name =
	TOKEN_STRING_INITIALIZER(struct cmd_set_query_result,
			vm_name, NULL);
cmdline_parse_token_string_t cmd_set_query_status =
	TOKEN_STRING_INITIALIZER(struct cmd_set_query_result,
			query_status, "enable#disable");

cmdline_parse_inst_t cmd_set_query_set = {
	.f = cmd_set_query_parsed,
	.data = NULL,
	.help_str = "set_query <vm_name> <enable|disable>, allow or disallow queries"
			" for the specified VM",
	.tokens = {
		(void *)&cmd_set_query,
		(void *)&cmd_set_query_vm_name,
		(void *)&cmd_set_query_status,
		NULL,
	},
};

struct cmd_channels_status_op_result {
	cmdline_fixed_string_t op;
	cmdline_fixed_string_t vm_name;
	cmdline_fixed_string_t channel_list;
	cmdline_fixed_string_t status;
};

static void
cmd_channels_status_op_parsed(void *parsed_result, struct cmdline *cl,
		       __rte_unused void *data)
{
	unsigned num_channels = 0, channel_num;
	int changed;
	unsigned int channel_list[RTE_MAX_LCORE];
	char *token, *remaining, *tail_ptr;
	struct cmd_channels_status_op_result *res = parsed_result;
	enum channel_status status;

	if (!strcmp(res->status, "enabled"))
		status = CHANNEL_MGR_CHANNEL_CONNECTED;
	else
		status = CHANNEL_MGR_CHANNEL_DISABLED;

	if (!strcmp(res->channel_list, "all")) {
		changed = set_channel_status_all(res->vm_name, status);
		cmdline_printf(cl, "Updated status of %d channels "
				"for VM '%s'\n", changed, res->vm_name);
		return;
	}
	remaining = res->channel_list;
	while (1) {
		if (remaining == NULL || remaining[0] == '\0')
			break;
		token = strsep(&remaining, ",");
		if (token == NULL)
			break;
		errno = 0;
		channel_num = (unsigned)strtol(token, &tail_ptr, 10);
		if ((errno != 0) || tail_ptr == NULL || (*tail_ptr != '\0'))
			break;

		if (channel_num == RTE_MAX_LCORE) {
			cmdline_printf(cl, "%u exceeds the maximum number of allowable "
					"channels(%u) for VM '%s'\n", channel_num,
					RTE_MAX_LCORE, res->vm_name);
			return;
		}
		channel_list[num_channels++] = channel_num;
	}
	changed = set_channel_status(res->vm_name, channel_list, num_channels,
			status);
	cmdline_printf(cl, "Updated status of %d channels "
					"for VM '%s'\n", changed, res->vm_name);
}

cmdline_parse_token_string_t cmd_channels_status_op =
	TOKEN_STRING_INITIALIZER(struct cmd_channels_status_op_result,
				op, "set_channel_status");
cmdline_parse_token_string_t cmd_channels_status_vm_name =
	TOKEN_STRING_INITIALIZER(struct cmd_channels_status_op_result,
			vm_name, NULL);
cmdline_parse_token_string_t cmd_channels_status_list =
	TOKEN_STRING_INITIALIZER(struct cmd_channels_status_op_result,
			channel_list, NULL);
cmdline_parse_token_string_t cmd_channels_status =
	TOKEN_STRING_INITIALIZER(struct cmd_channels_status_op_result,
			status, "enabled#disabled");

cmdline_parse_inst_t cmd_channels_status_op_set = {
	.f = cmd_channels_status_op_parsed,
	.data = NULL,
	.help_str = "set_channel_status <vm_name> <list>|all enabled|disabled, "
			" enable or disable the communication channels in "
			"list(comma-separated) for the specified VM, alternatively "
			"list can be replaced with keyword 'all'. "
			"Disabled channels will still receive packets on the host, "
			"however the commands they specify will be ignored. "
			"Set status to 'enabled' to begin processing requests again.",
	.tokens = {
		(void *)&cmd_channels_status_op,
		(void *)&cmd_channels_status_vm_name,
		(void *)&cmd_channels_status_list,
		(void *)&cmd_channels_status,
		NULL,
	},
};

/* *** CPU Frequency operations *** */
struct cmd_show_cpu_freq_result {
	cmdline_fixed_string_t show_cpu_freq;
	uint8_t core_num;
};

static void
cmd_show_cpu_freq_parsed(void *parsed_result, struct cmdline *cl,
		       __rte_unused void *data)
{
	struct cmd_show_cpu_freq_result *res = parsed_result;
	uint32_t curr_freq = power_manager_get_current_frequency(res->core_num);

	if (curr_freq == 0) {
		cmdline_printf(cl, "Unable to get frequency for core %u\n",
				res->core_num);
		return;
	}
	cmdline_printf(cl, "Core %u frequency: %"PRId32"\n", res->core_num,
			curr_freq);
}

cmdline_parse_token_string_t cmd_show_cpu_freq =
	TOKEN_STRING_INITIALIZER(struct cmd_show_cpu_freq_result,
			show_cpu_freq, "show_cpu_freq");

cmdline_parse_token_num_t cmd_show_cpu_freq_core_num =
	TOKEN_NUM_INITIALIZER(struct cmd_show_cpu_freq_result,
			core_num, RTE_UINT8);

cmdline_parse_inst_t cmd_show_cpu_freq_set = {
	.f = cmd_show_cpu_freq_parsed,
	.data = NULL,
	.help_str = "Get the current frequency for the specified core",
	.tokens = {
		(void *)&cmd_show_cpu_freq,
		(void *)&cmd_show_cpu_freq_core_num,
		NULL,
	},
};

struct cmd_set_cpu_freq_result {
	cmdline_fixed_string_t set_cpu_freq;
	uint8_t core_num;
	cmdline_fixed_string_t cmd;
};

static void
cmd_set_cpu_freq_parsed(void *parsed_result, struct cmdline *cl,
		       __rte_unused void *data)
{
	int ret = -1;
	struct cmd_set_cpu_freq_result *res = parsed_result;

	if (!strcmp(res->cmd , "up"))
		ret = power_manager_scale_core_up(res->core_num);
	else if (!strcmp(res->cmd , "down"))
		ret = power_manager_scale_core_down(res->core_num);
	else if (!strcmp(res->cmd , "min"))
		ret = power_manager_scale_core_min(res->core_num);
	else if (!strcmp(res->cmd , "max"))
		ret = power_manager_scale_core_max(res->core_num);
	else if (!strcmp(res->cmd, "enable_turbo"))
		ret = power_manager_enable_turbo_core(res->core_num);
	else if (!strcmp(res->cmd, "disable_turbo"))
		ret = power_manager_disable_turbo_core(res->core_num);
	if (ret < 0) {
		cmdline_printf(cl, "Error scaling core(%u) '%s'\n", res->core_num,
				res->cmd);
	}
}

cmdline_parse_token_string_t cmd_set_cpu_freq =
	TOKEN_STRING_INITIALIZER(struct cmd_set_cpu_freq_result,
			set_cpu_freq, "set_cpu_freq");
cmdline_parse_token_num_t cmd_set_cpu_freq_core_num =
	TOKEN_NUM_INITIALIZER(struct cmd_set_cpu_freq_result,
			core_num, RTE_UINT8);
cmdline_parse_token_string_t cmd_set_cpu_freq_cmd_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_set_cpu_freq_result,
			cmd, "up#down#min#max#enable_turbo#disable_turbo");

cmdline_parse_inst_t cmd_set_cpu_freq_set = {
	.f = cmd_set_cpu_freq_parsed,
	.data = NULL,
	.help_str = "set_cpu_freq <core_num> <up|down|min|max|enable_turbo|disable_turbo>, adjust the current "
			"frequency for the specified core",
	.tokens = {
		(void *)&cmd_set_cpu_freq,
		(void *)&cmd_set_cpu_freq_core_num,
		(void *)&cmd_set_cpu_freq_cmd_cmd,
		NULL,
	},
};

cmdline_parse_ctx_t main_ctx[] = {
		(cmdline_parse_inst_t *)&cmd_quit,
		(cmdline_parse_inst_t *)&cmd_vm_op_set,
		(cmdline_parse_inst_t *)&cmd_channels_op_set,
		(cmdline_parse_inst_t *)&cmd_channels_status_op_set,
		(cmdline_parse_inst_t *)&cmd_show_vm_set,
		(cmdline_parse_inst_t *)&cmd_show_cpu_freq_set,
		(cmdline_parse_inst_t *)&cmd_set_cpu_freq_set,
		(cmdline_parse_inst_t *)&cmd_set_pcpu_set,
		(cmdline_parse_inst_t *)&cmd_set_query_set,
		NULL,
};

void
run_cli(__rte_unused void *arg)
{
	struct cmdline *cl;

	cl = cmdline_stdin_new(main_ctx, "vmpower> ");
	if (cl == NULL)
		return;

	cmdline_interact(cl);
	cmdline_stdin_exit(cl);
}
