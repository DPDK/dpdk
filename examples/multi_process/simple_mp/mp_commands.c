/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdio.h>
#include <termios.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_branch_prediction.h>
#include <rte_launch.h>
#include <rte_log.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_ring.h>
#include <rte_debug.h>
#include <rte_mempool.h>
#include <rte_string_fns.h>

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_string.h>
#include <cmdline_socket.h>
#include <cmdline.h>
#include "mp_commands.h"

/**********************************************************/

struct cmd_send_result {
	cmdline_fixed_string_t action;
	cmdline_fixed_string_t message;
};

static void cmd_send_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	void *msg = NULL;
	struct cmd_send_result *res = parsed_result;

	if (rte_mempool_get(message_pool, &msg) < 0)
		rte_panic("Failed to get message buffer\n");
	strlcpy((char *)msg, res->message, STR_TOKEN_SIZE);
	if (rte_ring_enqueue(send_ring, msg) < 0) {
		printf("Failed to send message - message discarded\n");
		rte_mempool_put(message_pool, msg);
	}
}

cmdline_parse_token_string_t cmd_send_action =
	TOKEN_STRING_INITIALIZER(struct cmd_send_result, action, "send");
cmdline_parse_token_string_t cmd_send_message =
	TOKEN_STRING_INITIALIZER(struct cmd_send_result, message, NULL);

cmdline_parse_inst_t cmd_send = {
	.f = cmd_send_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "send a string to another process",
	.tokens = {        /* token list, NULL terminated */
			(void *)&cmd_send_action,
			(void *)&cmd_send_message,
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
	quit = 1;
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

/**********************************************************/

struct cmd_help_result {
	cmdline_fixed_string_t help;
};

static void cmd_help_parsed(__rte_unused void *parsed_result,
			    struct cmdline *cl,
			    __rte_unused void *data)
{
	cmdline_printf(cl, "Simple demo example of multi-process in RTE\n\n"
			"This is a readline-like interface that can be used to\n"
			"send commands to the simple app. Commands supported are:\n\n"
			"- send [string]\n" "- help\n" "- quit\n\n");
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

/****** CONTEXT (list of instruction) */
cmdline_parse_ctx_t simple_mp_ctx[] = {
		(cmdline_parse_inst_t *)&cmd_send,
		(cmdline_parse_inst_t *)&cmd_quit,
		(cmdline_parse_inst_t *)&cmd_help,
	NULL,
};
