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


#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <termios.h>

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_string.h>
#include <cmdline_parse_num.h>
#include <cmdline_socket.h>
#include <cmdline.h>
#include <rte_config.h>
#include <rte_log.h>
#include <rte_lcore.h>

#include <rte_power.h>

#include "vm_power_cli_guest.h"


#define CHANNEL_PATH "/dev/virtio-ports/virtio.serial.port.poweragent"


#define RTE_LOGTYPE_GUEST_CHANNEL RTE_LOGTYPE_USER1

struct cmd_quit_result {
	cmdline_fixed_string_t quit;
};

static void cmd_quit_parsed(__attribute__((unused)) void *parsed_result,
				__attribute__((unused)) struct cmdline *cl,
			    __attribute__((unused)) void *data)
{
	unsigned lcore_id;

	RTE_LCORE_FOREACH(lcore_id) {
		rte_power_exit(lcore_id);
	}
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

struct cmd_set_cpu_freq_result {
	cmdline_fixed_string_t set_cpu_freq;
	uint8_t lcore_id;
	cmdline_fixed_string_t cmd;
};

static void
cmd_set_cpu_freq_parsed(void *parsed_result, struct cmdline *cl,
		       __attribute__((unused)) void *data)
{
	int ret = -1;
	struct cmd_set_cpu_freq_result *res = parsed_result;

	if (!strcmp(res->cmd , "up"))
		ret = rte_power_freq_up(res->lcore_id);
	else if (!strcmp(res->cmd , "down"))
		ret = rte_power_freq_down(res->lcore_id);
	else if (!strcmp(res->cmd , "min"))
		ret = rte_power_freq_min(res->lcore_id);
	else if (!strcmp(res->cmd , "max"))
		ret = rte_power_freq_max(res->lcore_id);
	if (ret != 1)
		cmdline_printf(cl, "Error sending message: %s\n", strerror(ret));
}

cmdline_parse_token_string_t cmd_set_cpu_freq =
	TOKEN_STRING_INITIALIZER(struct cmd_set_cpu_freq_result,
			set_cpu_freq, "set_cpu_freq");
cmdline_parse_token_string_t cmd_set_cpu_freq_core_num =
	TOKEN_NUM_INITIALIZER(struct cmd_set_cpu_freq_result,
			lcore_id, UINT8);
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
		(cmdline_parse_inst_t *)&cmd_set_cpu_freq_set,
		NULL,
};

void
run_cli(__attribute__((unused)) void *arg)
{
	struct cmdline *cl;

	cl = cmdline_stdin_new(main_ctx, "vmpower(guest)> ");
	if (cl == NULL)
		return;

	cmdline_interact(cl);
	cmdline_stdin_exit(cl);
}
