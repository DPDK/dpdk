/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell International Ltd.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_socket.h>
#include <rte_ethdev.h>

#include "module_api.h"

static const char
cmd_feature_arcs_help[] = "feature arcs    # Display all feature arcs";

static const char
cmd_feature_show_help[] = "feature <arc_name> show   # Display features within an arc";

static const char
cmd_feature_enable_help[] = "feature enable <arc name> <feature name> <port-id>";

static const char
cmd_feature_disable_help[] = "feature disable <arc name> <feature name> <port-id>";

static void
feature_show(const char *arc_name)
{
	rte_graph_feature_arc_t _arc;
	uint32_t length, count, i;

	length = strlen(conn->msg_out);
	conn->msg_out += length;

	if (rte_graph_feature_arc_lookup_by_name(arc_name, &_arc) < 0)
		return;

	count = rte_graph_feature_arc_num_features(_arc);

	if (count) {
		snprintf(conn->msg_out, conn->msg_out_len_max, "\n%s%s%s\n",
			 "----------------------------- feature arc: ",
			 rte_graph_feature_arc_get(_arc)->feature_arc_name,
			 " -----------------------------");
		for (i = 0; i < count; i++)
			snprintf(conn->msg_out + strlen(conn->msg_out),
				 conn->msg_out_len_max, "%s\n",
				 rte_graph_feature_arc_feature_to_name(_arc, i));
	}
	length = strlen(conn->msg_out);
	conn->msg_out_len_max -= length;
}

static void
feature_arcs_show(void)
{
	uint32_t length, count, i;
	char **names;

	length = strlen(conn->msg_out);
	conn->msg_out += length;

	count = rte_graph_feature_arc_names_get(NULL);

	if (count) {
		names = malloc(count);
		if (!names) {
			snprintf(conn->msg_out, conn->msg_out_len_max, "Failed to allocate memory\n");
			return;
		}
		count = rte_graph_feature_arc_names_get(names);
		snprintf(conn->msg_out, conn->msg_out_len_max, "\n%s\n",
			 "----------------------------- feature arcs -----------------------------");
		for (i = 0; i < count; i++)
			feature_show(names[i]);
		free(names);
	}
	length = strlen(conn->msg_out);
	conn->msg_out_len_max -= length;
}

void
cmd_feature_parsed(void *parsed_result, __rte_unused struct cmdline *cl,
		   __rte_unused void *data)
{
	struct cmd_feature_result *res = parsed_result;

	feature_show(res->name);
}


void
cmd_feature_arcs_parsed(__rte_unused void *parsed_result, __rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
		feature_arcs_show();
}

void
cmd_feature_enable_parsed(void *parsed_result, __rte_unused struct cmdline *cl,
			  __rte_unused void *data)
{
	struct cmd_feature_enable_result *res = parsed_result;
	rte_graph_feature_arc_t arc;

	if (!rte_graph_feature_arc_lookup_by_name(res->arc_name, &arc))
		rte_graph_feature_enable(arc, res->interface, res->feature_name,
					 res->interface, NULL);
}

void
cmd_feature_disable_parsed(void *parsed_result, __rte_unused struct cmdline *cl,
			   __rte_unused void *data)
{
	struct cmd_feature_disable_result *res = parsed_result;
	rte_graph_feature_arc_t arc;

	if (!rte_graph_feature_arc_lookup_by_name(res->arc_name, &arc))
		rte_graph_feature_disable(arc, res->interface, res->feature_name, NULL);

}

void
cmd_help_feature_parsed(__rte_unused void *parsed_result, __rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
	size_t len;

	len = strlen(conn->msg_out);
	conn->msg_out += len;
	snprintf(conn->msg_out, conn->msg_out_len_max, "\n%s\n%s\n%s\n%s\n%s\n",
		 "----------------------------- feature command help -----------------------------",
		 cmd_feature_arcs_help, cmd_feature_show_help, cmd_feature_enable_help,
		 cmd_feature_disable_help);

	len = strlen(conn->msg_out);
	conn->msg_out_len_max -= len;
}
