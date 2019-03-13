/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright(c) 2014 6WIND S.A.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <termios.h>
#ifndef __linux__
#ifndef __FreeBSD__
#include <net/socket.h>
#endif
#endif
#include <inttypes.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_devargs.h>

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_ipaddr.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline.h>
#include <rte_string_fns.h>

#include "test.h"

/****************/

static struct test_commands_list commands_list =
	TAILQ_HEAD_INITIALIZER(commands_list);

void
add_test_command(struct test_command *t)
{
	TAILQ_INSERT_TAIL(&commands_list, t, next);
}

struct cmd_autotest_result {
	cmdline_fixed_string_t autotest;
};

static void cmd_autotest_parsed(void *parsed_result,
				__attribute__((unused)) struct cmdline *cl,
				__attribute__((unused)) void *data)
{
	struct test_command *t;
	struct cmd_autotest_result *res = parsed_result;
	int ret = 0;

	TAILQ_FOREACH(t, &commands_list, next) {
		if (!strcmp(res->autotest, t->command))
			ret = t->callback();
	}

	last_test_result = ret;
	if (ret == 0)
		printf("Test OK\n");
	else if (ret == TEST_SKIPPED)
		printf("Test Skipped\n");
	else
		printf("Test Failed\n");
	fflush(stdout);
}

cmdline_parse_token_string_t cmd_autotest_autotest =
	TOKEN_STRING_INITIALIZER(struct cmd_autotest_result, autotest,
				 "");

cmdline_parse_inst_t cmd_autotest = {
	.f = cmd_autotest_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "launch autotest",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_autotest_autotest,
		NULL,
	},
};

/****************/

struct cmd_dump_result {
	cmdline_fixed_string_t dump;
};

static void
dump_struct_sizes(void)
{
#define DUMP_SIZE(t) printf("sizeof(" #t ") = %u\n", (unsigned)sizeof(t));
	DUMP_SIZE(struct rte_mbuf);
	DUMP_SIZE(struct rte_mempool);
	DUMP_SIZE(struct rte_ring);
#undef DUMP_SIZE
}

static void cmd_dump_parsed(void *parsed_result,
			    __attribute__((unused)) struct cmdline *cl,
			    __attribute__((unused)) void *data)
{
	struct cmd_dump_result *res = parsed_result;

	if (!strcmp(res->dump, "dump_physmem"))
		rte_dump_physmem_layout(stdout);
	else if (!strcmp(res->dump, "dump_memzone"))
		rte_memzone_dump(stdout);
	else if (!strcmp(res->dump, "dump_struct_sizes"))
		dump_struct_sizes();
	else if (!strcmp(res->dump, "dump_ring"))
		rte_ring_list_dump(stdout);
	else if (!strcmp(res->dump, "dump_mempool"))
		rte_mempool_list_dump(stdout);
	else if (!strcmp(res->dump, "dump_devargs"))
		rte_devargs_dump(stdout);
	else if (!strcmp(res->dump, "dump_log_types"))
		rte_log_dump(stdout);
	else if (!strcmp(res->dump, "dump_malloc_stats"))
		rte_malloc_dump_stats(stdout, NULL);
	else if (!strcmp(res->dump, "dump_malloc_heaps"))
		rte_malloc_dump_heaps(stdout);
}

cmdline_parse_token_string_t cmd_dump_dump =
	TOKEN_STRING_INITIALIZER(struct cmd_dump_result, dump,
				 "dump_physmem#"
				 "dump_memzone#"
				 "dump_struct_sizes#"
				 "dump_ring#"
				 "dump_mempool#"
				 "dump_malloc_stats#"
				 "dump_malloc_heaps#"
				 "dump_devargs#"
				 "dump_log_types");

cmdline_parse_inst_t cmd_dump = {
	.f = cmd_dump_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "dump status",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_dump_dump,
		NULL,
	},
};

/****************/

struct cmd_dump_one_result {
	cmdline_fixed_string_t dump;
	cmdline_fixed_string_t name;
};

static void cmd_dump_one_parsed(void *parsed_result, struct cmdline *cl,
				__attribute__((unused)) void *data)
{
	struct cmd_dump_one_result *res = parsed_result;

	if (!strcmp(res->dump, "dump_ring")) {
		struct rte_ring *r;
		r = rte_ring_lookup(res->name);
		if (r == NULL) {
			cmdline_printf(cl, "Cannot find ring\n");
			return;
		}
		rte_ring_dump(stdout, r);
	}
	else if (!strcmp(res->dump, "dump_mempool")) {
		struct rte_mempool *mp;
		mp = rte_mempool_lookup(res->name);
		if (mp == NULL) {
			cmdline_printf(cl, "Cannot find mempool\n");
			return;
		}
		rte_mempool_dump(stdout, mp);
	}
}

cmdline_parse_token_string_t cmd_dump_one_dump =
	TOKEN_STRING_INITIALIZER(struct cmd_dump_one_result, dump,
				 "dump_ring#dump_mempool");

cmdline_parse_token_string_t cmd_dump_one_name =
	TOKEN_STRING_INITIALIZER(struct cmd_dump_one_result, name, NULL);

cmdline_parse_inst_t cmd_dump_one = {
	.f = cmd_dump_one_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "dump one ring/mempool: dump_ring|dump_mempool <name>",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_dump_one_dump,
		(void *)&cmd_dump_one_name,
		NULL,
	},
};

/****************/

struct cmd_quit_result {
	cmdline_fixed_string_t quit;
};

static void
cmd_quit_parsed(__attribute__((unused)) void *parsed_result,
		struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	cmdline_quit(cl);
}

cmdline_parse_token_string_t cmd_quit_quit =
	TOKEN_STRING_INITIALIZER(struct cmd_quit_result, quit,
				 "quit");

cmdline_parse_inst_t cmd_quit = {
	.f = cmd_quit_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "exit application",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_quit_quit,
		NULL,
	},
};

/****************/

struct cmd_set_rxtx_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t mode;
};

static void cmd_set_rxtx_parsed(void *parsed_result, struct cmdline *cl,
				__attribute__((unused)) void *data)
{
	struct cmd_set_rxtx_result *res = parsed_result;
	if (test_set_rxtx_conf(res->mode) < 0)
		cmdline_printf(cl, "Cannot find such mode\n");
}

cmdline_parse_token_string_t cmd_set_rxtx_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_rxtx_result, set,
				 "set_rxtx_mode");

cmdline_parse_token_string_t cmd_set_rxtx_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_rxtx_result, mode, NULL);

cmdline_parse_inst_t cmd_set_rxtx = {
	.f = cmd_set_rxtx_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "set rxtx routine: "
			"set_rxtx <mode>",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_set_rxtx_set,
		(void *)&cmd_set_rxtx_mode,
		NULL,
	},
};

/****************/

struct cmd_set_rxtx_anchor {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t type;
};

static void
cmd_set_rxtx_anchor_parsed(void *parsed_result,
			   struct cmdline *cl,
			   __attribute__((unused)) void *data)
{
	struct cmd_set_rxtx_anchor *res = parsed_result;
	if (test_set_rxtx_anchor(res->type) < 0)
		cmdline_printf(cl, "Cannot find such anchor\n");
}

cmdline_parse_token_string_t cmd_set_rxtx_anchor_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_rxtx_anchor, set,
				 "set_rxtx_anchor");

cmdline_parse_token_string_t cmd_set_rxtx_anchor_type =
	TOKEN_STRING_INITIALIZER(struct cmd_set_rxtx_anchor, type, NULL);

cmdline_parse_inst_t cmd_set_rxtx_anchor = {
	.f = cmd_set_rxtx_anchor_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "set rxtx anchor: "
			"set_rxtx_anchor <type>",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_set_rxtx_anchor_set,
		(void *)&cmd_set_rxtx_anchor_type,
		NULL,
	},
};

/****************/

/* for stream control */
struct cmd_set_rxtx_sc {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t type;
};

static void
cmd_set_rxtx_sc_parsed(void *parsed_result,
			   struct cmdline *cl,
			   __attribute__((unused)) void *data)
{
	struct cmd_set_rxtx_sc *res = parsed_result;
	if (test_set_rxtx_sc(res->type) < 0)
		cmdline_printf(cl, "Cannot find such stream control\n");
}

cmdline_parse_token_string_t cmd_set_rxtx_sc_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_rxtx_sc, set,
				 "set_rxtx_sc");

cmdline_parse_token_string_t cmd_set_rxtx_sc_type =
	TOKEN_STRING_INITIALIZER(struct cmd_set_rxtx_sc, type, NULL);

cmdline_parse_inst_t cmd_set_rxtx_sc = {
	.f = cmd_set_rxtx_sc_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "set rxtx stream control: "
			"set_rxtx_sc <type>",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_set_rxtx_sc_set,
		(void *)&cmd_set_rxtx_sc_type,
		NULL,
	},
};

/****************/


cmdline_parse_ctx_t main_ctx[] = {
	(cmdline_parse_inst_t *)&cmd_autotest,
	(cmdline_parse_inst_t *)&cmd_dump,
	(cmdline_parse_inst_t *)&cmd_dump_one,
	(cmdline_parse_inst_t *)&cmd_quit,
	(cmdline_parse_inst_t *)&cmd_set_rxtx,
	(cmdline_parse_inst_t *)&cmd_set_rxtx_anchor,
	(cmdline_parse_inst_t *)&cmd_set_rxtx_sc,
	NULL,
};

int commands_init(void)
{
	struct test_command *t;
	char *commands;
	int commands_len = 0;

	TAILQ_FOREACH(t, &commands_list, next) {
		commands_len += strlen(t->command) + 1;
	}

	commands = (char *)calloc(commands_len, sizeof(char));
	if (!commands)
		return -1;

	TAILQ_FOREACH(t, &commands_list, next) {
		strlcat(commands, t->command, commands_len);
		if (TAILQ_NEXT(t, next) != NULL)
			strlcat(commands, "#", commands_len);
	}

	cmd_autotest_autotest.string_data.str = commands;
	return 0;
}
