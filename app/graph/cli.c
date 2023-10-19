/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_socket.h>
#include <rte_common.h>

#include "module_api.h"

#define CMD_MAX_TOKENS 256
#define MAX_LINE_SIZE 2048

cmdline_parse_ctx_t modules_ctx[] = {
	(cmdline_parse_inst_t *)&graph_config_cmd_ctx,
	(cmdline_parse_inst_t *)&graph_start_cmd_ctx,
	(cmdline_parse_inst_t *)&graph_stats_cmd_ctx,
	(cmdline_parse_inst_t *)&graph_help_cmd_ctx,
	(cmdline_parse_inst_t *)&mempool_config_cmd_ctx,
	(cmdline_parse_inst_t *)&mempool_help_cmd_ctx,
	(cmdline_parse_inst_t *)&ethdev_show_cmd_ctx,
	(cmdline_parse_inst_t *)&ethdev_stats_cmd_ctx,
	(cmdline_parse_inst_t *)&ethdev_mtu_cmd_ctx,
	(cmdline_parse_inst_t *)&ethdev_prom_mode_cmd_ctx,
	(cmdline_parse_inst_t *)&ethdev_ip4_cmd_ctx,
	(cmdline_parse_inst_t *)&ethdev_ip6_cmd_ctx,
	(cmdline_parse_inst_t *)&ethdev_cmd_ctx,
	(cmdline_parse_inst_t *)&ethdev_help_cmd_ctx,
	(cmdline_parse_inst_t *)&ethdev_rx_cmd_ctx,
	(cmdline_parse_inst_t *)&ethdev_rx_help_cmd_ctx,
	(cmdline_parse_inst_t *)&ipv4_lookup_cmd_ctx,
	(cmdline_parse_inst_t *)&ipv4_lookup_help_cmd_ctx,
	(cmdline_parse_inst_t *)&ipv6_lookup_cmd_ctx,
	(cmdline_parse_inst_t *)&ipv6_lookup_help_cmd_ctx,
	(cmdline_parse_inst_t *)&neigh_v4_cmd_ctx,
	(cmdline_parse_inst_t *)&neigh_v6_cmd_ctx,
	(cmdline_parse_inst_t *)&neigh_help_cmd_ctx,
	NULL,
};

static struct cmdline *cl;

static int
is_comment(char *in)
{
	if ((strlen(in) && index("!#%;", in[0])) ||
		(strncmp(in, "//", 2) == 0) ||
		(strncmp(in, "--", 2) == 0))
		return 1;

	return 0;
}

void
cli_init(void)
{
	cl = cmdline_stdin_new(modules_ctx, "");
}

void
cli_exit(void)
{
	cmdline_stdin_exit(cl);
}

void
cli_process(char *in, char *out, size_t out_size, __rte_unused void *obj)
{
	int rc;

	if (is_comment(in))
		return;

	rc = cmdline_parse(cl, in);
	if (rc == CMDLINE_PARSE_AMBIGUOUS)
		snprintf(out, out_size, MSG_CMD_FAIL, "Ambiguous command");
	else if (rc == CMDLINE_PARSE_NOMATCH)
		snprintf(out, out_size, MSG_CMD_FAIL, "Command mismatch");
	else if (rc == CMDLINE_PARSE_BAD_ARGS)
		snprintf(out, out_size, MSG_CMD_FAIL, "Bad arguments");

	return;

}

int
cli_script_process(const char *file_name, size_t msg_in_len_max, size_t msg_out_len_max, void *obj)
{
	char *msg_in = NULL, *msg_out = NULL;
	int rc = -EINVAL;
	FILE *f = NULL;

	/* Check input arguments */
	if ((file_name == NULL) || (strlen(file_name) == 0) || (msg_in_len_max == 0) ||
	    (msg_out_len_max == 0))
		return rc;

	msg_in = malloc(msg_in_len_max + 1);
	msg_out = malloc(msg_out_len_max + 1);
	if ((msg_in == NULL) || (msg_out == NULL)) {
		rc = -ENOMEM;
		goto exit;
	}

	/* Open input file */
	f = fopen(file_name, "r");
	if (f == NULL) {
		rc = -EIO;
		goto exit;
	}

	/* Read file */
	while (fgets(msg_in, msg_in_len_max, f) != NULL) {
		msg_out[0] = 0;

		cli_process(msg_in, msg_out, msg_out_len_max, obj);

		if (strlen(msg_out))
			printf("%s", msg_out);
	}

	/* Close file */
	fclose(f);
	rc = 0;

exit:
	free(msg_out);
	free(msg_in);
	return rc;
}
