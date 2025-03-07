/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 NVIDIA Corporation & Affiliates
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <eal_export.h>
#include <rte_string_fns.h>
#include <stdlib.h>

#include "cmdline_parse.h"
#include "cmdline_parse_bool.h"


RTE_EXPORT_EXPERIMENTAL_SYMBOL(cmdline_token_bool_ops, 25.03)
struct cmdline_token_ops cmdline_token_bool_ops = {
	.parse = cmdline_parse_bool,
	.complete_get_nb = NULL,
	.complete_get_elt = NULL,
	.get_help = cmdline_get_help_string,
};

static cmdline_parse_token_string_t cmd_parse_token_bool = {
	{
		&cmdline_token_string_ops,
		0
	},
	{
		"on#off"
	}
};

/* parse string to bool */
int
cmdline_parse_bool(__rte_unused cmdline_parse_token_hdr_t *tk, const char *srcbuf, void *res,
	__rte_unused unsigned int ressize)
{
	cmdline_fixed_string_t on_off = {0};
	if (cmdline_token_string_ops.parse
			(&cmd_parse_token_bool.hdr, srcbuf, on_off, sizeof(on_off)) < 0)
		return -1;

	if (strcmp((char *)on_off, "on") == 0)
		*(uint8_t *)res = 1;
	else if (strcmp((char *)on_off, "off") == 0)
		*(uint8_t *)res = 0;

	return strlen(on_off);
}
