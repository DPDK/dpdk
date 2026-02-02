/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2016 Mellanox Technologies, Ltd
 * Copyright 2026 DynaNIC Semiconductors, Ltd.
 */

#include <stdio.h>
#include <string.h>

#include <cmdline_parse.h>
#include <cmdline_parse_string.h>
#include <cmdline_parse_num.h>
#include <rte_hexdump.h>

#include <rte_flow_parser_private.h>

/* Dynamic token callbacks live in librte_flow_parser, but cmdline instances
 * are owned by testpmd.
 */
static void
cmd_flow_cb(void *arg0, struct cmdline *cl, void *arg2)
{
	if (cl == NULL)
		rte_flow_parser_cmd_flow_tok(arg0, arg2);
	else
		rte_flow_parser_cmd_flow_dispatch(arg0);
}

static void
cmd_set_raw_cb(void *arg0, struct cmdline *cl, void *arg2)
{
	if (cl == NULL)
		rte_flow_parser_cmd_set_raw_tok(arg0, arg2);
	else
		rte_flow_parser_cmd_set_raw_dispatch(arg0);
}

/* show raw_encap/raw_decap support */
struct cmd_show_set_raw_result {
	cmdline_fixed_string_t cmd_show;
	cmdline_fixed_string_t cmd_what;
	cmdline_fixed_string_t cmd_all;
	uint16_t cmd_index;
};

static void
cmd_show_set_raw_parsed(void *parsed_result, struct cmdline *cl, void *data)
{
	struct cmd_show_set_raw_result *res = parsed_result;
	uint16_t index = res->cmd_index;
	const uint8_t *raw_data = NULL;
	size_t raw_size = 0;
	char title[16] = { 0 };
	int all = 0;

	RTE_SET_USED(cl);
	RTE_SET_USED(data);
	if (strcmp(res->cmd_all, "all") == 0) {
		all = 1;
		index = 0;
	} else if (index >= RAW_ENCAP_CONFS_MAX_NUM) {
		fprintf(stderr, "index should be 0-%u\n",
			RAW_ENCAP_CONFS_MAX_NUM - 1);
		return;
	}
	do {
		if (strcmp(res->cmd_what, "raw_encap") == 0) {
			const struct rte_flow_action_raw_encap *conf =
				rte_flow_parser_raw_encap_conf_get(index);

			if (conf == NULL || conf->data == NULL || conf->size == 0) {
				fprintf(stderr,
					"raw_encap %u not configured\n",
					index);
				goto next;
			}
			raw_data = conf->data;
			raw_size = conf->size;
		} else if (strcmp(res->cmd_what, "raw_decap") == 0) {
			const struct rte_flow_action_raw_decap *conf =
				rte_flow_parser_raw_decap_conf_get(index);

			if (conf == NULL || conf->data == NULL || conf->size == 0) {
				fprintf(stderr,
					"raw_decap %u not configured\n",
					index);
				goto next;
			}
			raw_data = conf->data;
			raw_size = conf->size;
		}
		snprintf(title, sizeof(title), "\nindex: %u", index);
		rte_hexdump(stdout, title, raw_data, raw_size);
next:
		raw_data = NULL;
		raw_size = 0;
	} while (all && ++index < RAW_ENCAP_CONFS_MAX_NUM);
}

static cmdline_parse_token_string_t cmd_show_set_raw_cmd_show =
	TOKEN_STRING_INITIALIZER(struct cmd_show_set_raw_result,
			cmd_show, "show");
static cmdline_parse_token_string_t cmd_show_set_raw_cmd_what =
	TOKEN_STRING_INITIALIZER(struct cmd_show_set_raw_result,
			cmd_what, "raw_encap#raw_decap");
static cmdline_parse_token_num_t cmd_show_set_raw_cmd_index =
	TOKEN_NUM_INITIALIZER(struct cmd_show_set_raw_result,
			cmd_index, RTE_UINT16);
static cmdline_parse_token_string_t cmd_show_set_raw_cmd_all =
	TOKEN_STRING_INITIALIZER(struct cmd_show_set_raw_result,
			cmd_all, "all");

cmdline_parse_inst_t cmd_flow = {
	.f = cmd_flow_cb,
	.data = NULL,
	.help_str = NULL,
	.tokens = {
		NULL,
	},
};

cmdline_parse_inst_t cmd_set_raw = {
	.f = cmd_set_raw_cb,
	.data = NULL,
	.help_str = NULL,
	.tokens = {
		NULL,
	},
};

cmdline_parse_inst_t cmd_show_set_raw = {
	.f = cmd_show_set_raw_parsed,
	.data = NULL,
	.help_str = "show <raw_encap|raw_decap> <index>",
	.tokens = {
		(void *)&cmd_show_set_raw_cmd_show,
		(void *)&cmd_show_set_raw_cmd_what,
		(void *)&cmd_show_set_raw_cmd_index,
		NULL,
	},
};

cmdline_parse_inst_t cmd_show_set_raw_all = {
	.f = cmd_show_set_raw_parsed,
	.data = NULL,
	.help_str = "show <raw_encap|raw_decap> all",
	.tokens = {
		(void *)&cmd_show_set_raw_cmd_show,
		(void *)&cmd_show_set_raw_cmd_what,
		(void *)&cmd_show_set_raw_cmd_all,
		NULL,
	},
};
