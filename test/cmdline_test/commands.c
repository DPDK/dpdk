/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <inttypes.h>

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_string.h>
#include <cmdline_parse_num.h>
#include <cmdline.h>

#include "cmdline_test.h"

/*** quit ***/
/* exit application */

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

cmdline_parse_token_string_t cmd_quit_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_quit_result, quit,
				 "quit");

cmdline_parse_inst_t cmd_quit = {
	.f = cmd_quit_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "exit application",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_quit_tok,
		NULL,
	},
};



/*** single ***/
/* a simple single-word command */

struct cmd_single_result {
	cmdline_fixed_string_t single;
};

static void
cmd_single_parsed(__attribute__((unused)) void *parsed_result,
		struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	cmdline_printf(cl, "Single word command parsed!\n");
}

cmdline_parse_token_string_t cmd_single_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_single_result, single,
				 "single");

cmdline_parse_inst_t cmd_single = {
	.f = cmd_single_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "a simple single-word command",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_single_tok,
		NULL,
	},
};



/*** single_long ***/
/* a variant of "single" command. useful to test autocomplete */

struct cmd_single_long_result {
	cmdline_fixed_string_t single_long;
};

static void
cmd_single_long_parsed(__attribute__((unused)) void *parsed_result,
		struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	cmdline_printf(cl, "Single long word command parsed!\n");
}

cmdline_parse_token_string_t cmd_single_long_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_single_long_result, single_long,
				 "single_long");

cmdline_parse_inst_t cmd_single_long = {
	.f = cmd_single_long_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "a variant of \"single\" command, useful to test autocomplete",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_single_long_tok,
		NULL,
	},
};



/*** autocomplete_1 ***/
/* first command to test autocomplete when multiple commands have chars
 * in common but none should complete due to ambiguity
 */

struct cmd_autocomplete_1_result {
	cmdline_fixed_string_t token;
};

static void
cmd_autocomplete_1_parsed(__attribute__((unused)) void *parsed_result,
		struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	cmdline_printf(cl, "Autocomplete command 1 parsed!\n");
}

cmdline_parse_token_string_t cmd_autocomplete_1_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_autocomplete_1_result, token,
				 "autocomplete_1");

cmdline_parse_inst_t cmd_autocomplete_1 = {
	.f = cmd_autocomplete_1_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "first ambiguous autocomplete command",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_autocomplete_1_tok,
		NULL,
	},
};



/*** autocomplete_2 ***/
/* second command to test autocomplete when multiple commands have chars
 * in common but none should complete due to ambiguity
 */

struct cmd_autocomplete_2_result {
	cmdline_fixed_string_t token;
};

static void
cmd_autocomplete_2_parsed(__attribute__((unused)) void *parsed_result,
		struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	cmdline_printf(cl, "Autocomplete command 2 parsed!\n");
}

cmdline_parse_token_string_t cmd_autocomplete_2_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_autocomplete_2_result, token,
				 "autocomplete_2");

cmdline_parse_inst_t cmd_autocomplete_2 = {
	.f = cmd_autocomplete_2_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "second ambiguous autocomplete command",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_autocomplete_2_tok,
		NULL,
	},
};



/*** number command ***/
/* a command that simply returns whatever (uint32) number is supplied to it */

struct cmd_num_result {
	unsigned num;
};

static void
cmd_num_parsed(void *parsed_result,
		struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	unsigned result = ((struct cmd_num_result*)parsed_result)->num;
	cmdline_printf(cl, "%u\n", result);
}

cmdline_parse_token_num_t cmd_num_tok =
	TOKEN_NUM_INITIALIZER(struct cmd_num_result, num, UINT32);

cmdline_parse_inst_t cmd_num = {
	.f = cmd_num_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "a command that simply returns whatever number is entered",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_num_tok,
		NULL,
	},
};



/*** ambiguous first|ambiguous ***/
/* first command used to test command ambiguity */

struct cmd_ambig_result_1 {
	cmdline_fixed_string_t common_part;
	cmdline_fixed_string_t ambig_part;
};

static void
cmd_ambig_1_parsed(__attribute__((unused)) void *parsed_result,
		struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	cmdline_printf(cl, "Command 1 parsed!\n");
}

cmdline_parse_token_string_t cmd_ambig_common_1 =
	TOKEN_STRING_INITIALIZER(struct cmd_ambig_result_1, common_part,
				 "ambiguous");
cmdline_parse_token_string_t cmd_ambig_ambig_1 =
	TOKEN_STRING_INITIALIZER(struct cmd_ambig_result_1, ambig_part,
				 "first#ambiguous#ambiguous2");

cmdline_parse_inst_t cmd_ambig_1 = {
	.f = cmd_ambig_1_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "first command used to test command ambiguity",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_ambig_common_1,
		(void*)&cmd_ambig_ambig_1,
		NULL,
	},
};



/*** ambiguous second|ambiguous ***/
/* second command used to test command ambiguity */

struct cmd_ambig_result_2 {
	cmdline_fixed_string_t common_part;
	cmdline_fixed_string_t ambig_part;
};

static void
cmd_ambig_2_parsed(__attribute__((unused)) void *parsed_result,
		struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	cmdline_printf(cl, "Command 2 parsed!\n");
}

cmdline_parse_token_string_t cmd_ambig_common_2 =
	TOKEN_STRING_INITIALIZER(struct cmd_ambig_result_2, common_part,
				 "ambiguous");
cmdline_parse_token_string_t cmd_ambig_ambig_2 =
	TOKEN_STRING_INITIALIZER(struct cmd_ambig_result_2, ambig_part,
				 "second#ambiguous#ambiguous2");

cmdline_parse_inst_t cmd_ambig_2 = {
	.f = cmd_ambig_2_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "second command used to test command ambiguity",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_ambig_common_2,
		(void*)&cmd_ambig_ambig_2,
		NULL,
	},
};



/*** get_history_bufsize ***/
/* command that displays total space in history buffer
 * this will be useful for testing history (to fill it up just enough to
 * remove the last entry, we need to know how big it is).
 */

struct cmd_get_history_bufsize_result {
	cmdline_fixed_string_t str;
};

static void
cmd_get_history_bufsize_parsed(__attribute__((unused)) void *parsed_result,
		struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	cmdline_printf(cl, "History buffer size: %zu\n",
			sizeof(cl->rdl.history_buf));
}

cmdline_parse_token_string_t cmd_get_history_bufsize_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_get_history_bufsize_result, str,
				 "get_history_bufsize");

cmdline_parse_inst_t cmd_get_history_bufsize = {
	.f = cmd_get_history_bufsize_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "command that displays total space in history buffer",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_get_history_bufsize_tok,
		NULL,
	},
};



/*** clear_history ***/
/* clears history buffer */

struct cmd_clear_history_result {
	cmdline_fixed_string_t str;
};

static void
cmd_clear_history_parsed(__attribute__((unused)) void *parsed_result,
		struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	rdline_clear_history(&cl->rdl);
}

cmdline_parse_token_string_t cmd_clear_history_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_clear_history_result, str,
				 "clear_history");

cmdline_parse_inst_t cmd_clear_history = {
	.f = cmd_clear_history_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "clear command history",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_clear_history_tok,
		NULL,
	},
};



/****************/

cmdline_parse_ctx_t main_ctx[] = {
		(cmdline_parse_inst_t *)&cmd_quit,
		(cmdline_parse_inst_t *)&cmd_ambig_1,
		(cmdline_parse_inst_t *)&cmd_ambig_2,
		(cmdline_parse_inst_t *)&cmd_single,
		(cmdline_parse_inst_t *)&cmd_single_long,
		(cmdline_parse_inst_t *)&cmd_num,
		(cmdline_parse_inst_t *)&cmd_get_history_bufsize,
		(cmdline_parse_inst_t *)&cmd_clear_history,
		(cmdline_parse_inst_t *)&cmd_autocomplete_1,
		(cmdline_parse_inst_t *)&cmd_autocomplete_2,
	NULL,
};
