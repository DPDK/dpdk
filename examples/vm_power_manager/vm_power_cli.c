/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include <rte_config.h>

#include "vm_power_cli.h"
#include "channel_manager.h"
#include "channel_monitor.h"
#include "power_manager.h"
#include "channel_commands.h"

struct cmd_quit_result {
	cmdline_fixed_string_t quit;
};

static void cmd_quit_parsed(__attribute__((unused)) void *parsed_result,
		struct cmdline *cl,
		__attribute__((unused)) void *data)
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
		__attribute__((unused)) void *data)
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
		cmdline_printf(cl, "  [%u]: Physical CPU Mask 0x%"PRIx64"\n", i,
				info.pcpu_mask[i]);
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
struct cmd_set_pcpu_mask_result {
	cmdline_fixed_string_t set_pcpu_mask;
	cmdline_fixed_string_t vm_name;
	uint8_t vcpu;
	uint64_t core_mask;
};

static void
cmd_set_pcpu_mask_parsed(void *parsed_result, struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	struct cmd_set_pcpu_mask_result *res = parsed_result;

	if (set_pcpus_mask(res->vm_name, res->vcpu, res->core_mask) == 0)
		cmdline_printf(cl, "Pinned vCPU(%"PRId8") to pCPU core "
				"mask(0x%"PRIx64")\n", res->vcpu, res->core_mask);
	else
		cmdline_printf(cl, "Unable to pin vCPU(%"PRId8") to pCPU core "
				"mask(0x%"PRIx64")\n", res->vcpu, res->core_mask);
}

cmdline_parse_token_string_t cmd_set_pcpu_mask =
		TOKEN_STRING_INITIALIZER(struct cmd_set_pcpu_mask_result,
				set_pcpu_mask, "set_pcpu_mask");
cmdline_parse_token_string_t cmd_set_pcpu_mask_vm_name =
		TOKEN_STRING_INITIALIZER(struct cmd_set_pcpu_mask_result,
				vm_name, NULL);
cmdline_parse_token_num_t set_pcpu_mask_vcpu =
		TOKEN_NUM_INITIALIZER(struct cmd_set_pcpu_mask_result,
				vcpu, UINT8);
cmdline_parse_token_num_t set_pcpu_mask_core_mask =
		TOKEN_NUM_INITIALIZER(struct cmd_set_pcpu_mask_result,
				core_mask, UINT64);


cmdline_parse_inst_t cmd_set_pcpu_mask_set = {
		.f = cmd_set_pcpu_mask_parsed,
		.data = NULL,
		.help_str = "set_pcpu_mask <vm_name> <vcpu> <pcpu>, Set the binding "
				"of Virtual CPU on VM to the Physical CPU mask.",
				.tokens = {
						(void *)&cmd_set_pcpu_mask,
						(void *)&cmd_set_pcpu_mask_vm_name,
						(void *)&set_pcpu_mask_vcpu,
						(void *)&set_pcpu_mask_core_mask,
						NULL,
		},
};

struct cmd_set_pcpu_result {
	cmdline_fixed_string_t set_pcpu;
	cmdline_fixed_string_t vm_name;
	uint8_t vcpu;
	uint8_t core;
};

static void
cmd_set_pcpu_parsed(void *parsed_result, struct cmdline *cl,
		__attribute__((unused)) void *data)
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
				vcpu, UINT8);
cmdline_parse_token_num_t set_pcpu_core =
		TOKEN_NUM_INITIALIZER(struct cmd_set_pcpu_result,
				core, UINT64);


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
		__attribute__((unused)) void *data)
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
			__attribute__((unused)) void *data)
{
	unsigned num_channels = 0, channel_num, i;
	int channels_added;
	unsigned channel_list[CHANNEL_CMDS_MAX_VM_CHANNELS];
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

		if (channel_num == CHANNEL_CMDS_MAX_VM_CHANNELS) {
			cmdline_printf(cl, "Channel number '%u' exceeds the maximum number "
					"of allowable channels(%u) for VM '%s'\n", channel_num,
					CHANNEL_CMDS_MAX_VM_CHANNELS, res->vm_name);
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

struct cmd_channels_status_op_result {
	cmdline_fixed_string_t op;
	cmdline_fixed_string_t vm_name;
	cmdline_fixed_string_t channel_list;
	cmdline_fixed_string_t status;
};

static void
cmd_channels_status_op_parsed(void *parsed_result, struct cmdline *cl,
		       __attribute__((unused)) void *data)
{
	unsigned num_channels = 0, channel_num;
	int changed;
	unsigned channel_list[CHANNEL_CMDS_MAX_VM_CHANNELS];
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

		if (channel_num == CHANNEL_CMDS_MAX_VM_CHANNELS) {
			cmdline_printf(cl, "%u exceeds the maximum number of allowable "
					"channels(%u) for VM '%s'\n", channel_num,
					CHANNEL_CMDS_MAX_VM_CHANNELS, res->vm_name);
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
struct cmd_show_cpu_freq_mask_result {
	cmdline_fixed_string_t show_cpu_freq_mask;
	uint64_t core_mask;
};

static void
cmd_show_cpu_freq_mask_parsed(void *parsed_result, struct cmdline *cl,
		       __attribute__((unused)) void *data)
{
	struct cmd_show_cpu_freq_mask_result *res = parsed_result;
	unsigned i;
	uint64_t mask = res->core_mask;
	uint32_t freq;

	for (i = 0; mask; mask &= ~(1ULL << i++)) {
		if ((mask >> i) & 1) {
			freq = power_manager_get_current_frequency(i);
			if (freq > 0)
				cmdline_printf(cl, "Core %u: %"PRId32"\n", i, freq);
		}
	}
}

cmdline_parse_token_string_t cmd_show_cpu_freq_mask =
	TOKEN_STRING_INITIALIZER(struct cmd_show_cpu_freq_mask_result,
			show_cpu_freq_mask, "show_cpu_freq_mask");
cmdline_parse_token_num_t cmd_show_cpu_freq_mask_core_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_show_cpu_freq_mask_result,
			core_mask, UINT64);

cmdline_parse_inst_t cmd_show_cpu_freq_mask_set = {
	.f = cmd_show_cpu_freq_mask_parsed,
	.data = NULL,
	.help_str = "show_cpu_freq_mask <mask>, Get the current frequency for each "
			"core specified in the mask",
	.tokens = {
		(void *)&cmd_show_cpu_freq_mask,
		(void *)&cmd_show_cpu_freq_mask_core_mask,
		NULL,
	},
};

struct cmd_set_cpu_freq_mask_result {
	cmdline_fixed_string_t set_cpu_freq_mask;
	uint64_t core_mask;
	cmdline_fixed_string_t cmd;
};

static void
cmd_set_cpu_freq_mask_parsed(void *parsed_result, struct cmdline *cl,
			__attribute__((unused)) void *data)
{
	struct cmd_set_cpu_freq_mask_result *res = parsed_result;
	int ret = -1;

	if (!strcmp(res->cmd , "up"))
		ret = power_manager_scale_mask_up(res->core_mask);
	else if (!strcmp(res->cmd , "down"))
		ret = power_manager_scale_mask_down(res->core_mask);
	else if (!strcmp(res->cmd , "min"))
		ret = power_manager_scale_mask_min(res->core_mask);
	else if (!strcmp(res->cmd , "max"))
		ret = power_manager_scale_mask_max(res->core_mask);
	if (ret < 0) {
		cmdline_printf(cl, "Error scaling core_mask(0x%"PRIx64") '%s' , not "
				"all cores specified have been scaled\n",
				res->core_mask, res->cmd);
	};
}

cmdline_parse_token_string_t cmd_set_cpu_freq_mask =
	TOKEN_STRING_INITIALIZER(struct cmd_set_cpu_freq_mask_result,
			set_cpu_freq_mask, "set_cpu_freq_mask");
cmdline_parse_token_num_t cmd_set_cpu_freq_mask_core_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_set_cpu_freq_mask_result,
			core_mask, UINT64);
cmdline_parse_token_string_t cmd_set_cpu_freq_mask_result =
	TOKEN_STRING_INITIALIZER(struct cmd_set_cpu_freq_mask_result,
			cmd, "up#down#min#max");

cmdline_parse_inst_t cmd_set_cpu_freq_mask_set = {
	.f = cmd_set_cpu_freq_mask_parsed,
	.data = NULL,
	.help_str = "set_cpu_freq <core_mask> <up|down|min|max>, Set the current "
			"frequency for the cores specified in <core_mask> by scaling "
			"each up/down/min/max.",
	.tokens = {
		(void *)&cmd_set_cpu_freq_mask,
		(void *)&cmd_set_cpu_freq_mask_core_mask,
		(void *)&cmd_set_cpu_freq_mask_result,
		NULL,
	},
};



struct cmd_show_cpu_freq_result {
	cmdline_fixed_string_t show_cpu_freq;
	uint8_t core_num;
};

static void
cmd_show_cpu_freq_parsed(void *parsed_result, struct cmdline *cl,
		       __attribute__((unused)) void *data)
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
			core_num, UINT8);

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
		       __attribute__((unused)) void *data)
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
			core_num, UINT8);
cmdline_parse_token_string_t cmd_set_cpu_freq_cmd_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_set_cpu_freq_result,
			cmd, "up#down#min#max");

cmdline_parse_inst_t cmd_set_cpu_freq_set = {
	.f = cmd_set_cpu_freq_parsed,
	.data = NULL,
	.help_str = "set_cpu_freq <core_num> <up|down|min|max>, Set the current "
			"frequency for the specified core by scaling up/down/min/max",
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
		(cmdline_parse_inst_t *)&cmd_show_cpu_freq_mask_set,
		(cmdline_parse_inst_t *)&cmd_set_cpu_freq_mask_set,
		(cmdline_parse_inst_t *)&cmd_show_cpu_freq_set,
		(cmdline_parse_inst_t *)&cmd_set_cpu_freq_set,
		(cmdline_parse_inst_t *)&cmd_set_pcpu_mask_set,
		(cmdline_parse_inst_t *)&cmd_set_pcpu_set,
		NULL,
};

void
run_cli(__attribute__((unused)) void *arg)
{
	struct cmdline *cl;

	cl = cmdline_stdin_new(main_ctx, "vmpower> ");
	if (cl == NULL)
		return;

	cmdline_interact(cl);
	cmdline_stdin_exit(cl);
}
