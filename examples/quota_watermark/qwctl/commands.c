/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <termios.h>

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline.h>

#include <rte_ring.h>

#include "qwctl.h"
#include "../include/conf.h"


/**
 * help command
 */

struct cmd_help_tokens {
	cmdline_fixed_string_t verb;
};

cmdline_parse_token_string_t cmd_help_verb =
		TOKEN_STRING_INITIALIZER(struct cmd_help_tokens, verb, "help");

static void
cmd_help_handler(__attribute__((unused)) void *parsed_result,
		struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	cmdline_printf(cl, "Available commands:\n"
			"- help\n"
			"- set  [ring_name|variable] <value>\n"
			"- show [ring_name|variable]\n"
			"\n"
			"Available variables:\n"
			"- low_watermark\n"
			"- quota\n"
			"- ring names follow the core%%u_port%%u format\n");
}

cmdline_parse_inst_t cmd_help = {
		.f = cmd_help_handler,
		.data = NULL,
		.help_str = "show help",
		.tokens = {
				(void *) &cmd_help_verb,
				NULL,
		},
};


/**
 * set command
 */

struct cmd_set_tokens {
	cmdline_fixed_string_t verb;
	cmdline_fixed_string_t variable;
	uint32_t value;
};

cmdline_parse_token_string_t cmd_set_verb =
		TOKEN_STRING_INITIALIZER(struct cmd_set_tokens, verb, "set");

cmdline_parse_token_string_t cmd_set_variable =
		TOKEN_STRING_INITIALIZER(struct cmd_set_tokens, variable, NULL);

cmdline_parse_token_num_t cmd_set_value =
		TOKEN_NUM_INITIALIZER(struct cmd_set_tokens, value, UINT32);

static void
cmd_set_handler(__attribute__((unused)) void *parsed_result,
		struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	struct cmd_set_tokens *tokens = parsed_result;
	struct rte_ring *ring;

	if (!strcmp(tokens->variable, "quota")) {

		if (tokens->value > 0 && tokens->value <= MAX_PKT_QUOTA)
			*quota = tokens->value;
		else
			cmdline_printf(cl, "quota must be between 1 and %u\n",
					MAX_PKT_QUOTA);
	}

	else if (!strcmp(tokens->variable, "low_watermark")) {

		if (tokens->value <= 100)
			*low_watermark = tokens->value * RING_SIZE / 100;
		else
			cmdline_printf(cl,
					"low_watermark must be between 0%% and 100%%\n");
	}

	else {

		ring = rte_ring_lookup(tokens->variable);
		if (ring == NULL)
			cmdline_printf(cl, "Cannot find ring \"%s\"\n",
					tokens->variable);
		else
			if (tokens->value >= *low_watermark * 100 / RING_SIZE
					&& tokens->value <= 100)
				*high_watermark = tokens->value *
						RING_SIZE / 100;
			else
				cmdline_printf(cl,
					"ring high watermark must be between %u%% and 100%%\n",
					*low_watermark * 100 / RING_SIZE);
	}
}

cmdline_parse_inst_t cmd_set = {
		.f = cmd_set_handler,
		.data = NULL,
		.help_str = "Set a variable value",
		.tokens = {
				(void *) &cmd_set_verb,
				(void *) &cmd_set_variable,
				(void *) &cmd_set_value,
				NULL,
		},
};


/**
 * show command
 */

struct cmd_show_tokens {
	cmdline_fixed_string_t verb;
	cmdline_fixed_string_t variable;
};

cmdline_parse_token_string_t cmd_show_verb =
		TOKEN_STRING_INITIALIZER(struct cmd_show_tokens, verb, "show");

cmdline_parse_token_string_t cmd_show_variable =
		TOKEN_STRING_INITIALIZER(struct cmd_show_tokens,
				variable, NULL);


static void
cmd_show_handler(__attribute__((unused)) void *parsed_result,
		struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	struct cmd_show_tokens *tokens = parsed_result;
	struct rte_ring *ring;

	if (!strcmp(tokens->variable, "quota"))
		cmdline_printf(cl, "Global quota: %d\n", *quota);

	else if (!strcmp(tokens->variable, "low_watermark"))
		cmdline_printf(cl, "Global low_watermark: %u\n",
				*low_watermark);

	else {

		ring = rte_ring_lookup(tokens->variable);
		if (ring == NULL)
			cmdline_printf(cl, "Cannot find ring \"%s\"\n",
					tokens->variable);
		else
			rte_ring_dump(stdout, ring);
	}
}

cmdline_parse_inst_t cmd_show = {
		.f = cmd_show_handler,
		.data = NULL,
		.help_str = "Show a variable value",
		.tokens = {
				(void *) &cmd_show_verb,
				(void *) &cmd_show_variable,
				NULL,
		},
};


cmdline_parse_ctx_t qwctl_ctx[] = {
		(cmdline_parse_inst_t *)&cmd_help,
		(cmdline_parse_inst_t *)&cmd_set,
		(cmdline_parse_inst_t *)&cmd_show,
		NULL,
};
