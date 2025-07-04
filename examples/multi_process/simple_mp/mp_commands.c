/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2023 Intel Corporation
 */
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_string_fns.h>

#include "mp_commands.h"

void
cmd_send_parsed(void *parsed_result,
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

void
cmd_quit_parsed(__rte_unused void *parsed_result,
			    struct cmdline *cl,
			    __rte_unused void *data)
{
	quit = 1;
	cmdline_quit(cl);
}

void
cmd_help_parsed(__rte_unused void *parsed_result,
			    struct cmdline *cl,
			    __rte_unused void *data)
{
	cmdline_printf(cl, "Simple demo example of multi-process in RTE\n\n"
			"This is a readline-like interface that can be used to\n"
			"send commands to the simple app. Commands supported are:\n\n"
			"- send [string]\n" "- help\n" "- quit\n\n");
}
