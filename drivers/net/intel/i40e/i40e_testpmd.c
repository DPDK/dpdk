/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation.
 */

#include <stdlib.h>

#include <rte_pmd_i40e.h>

#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>

#include "testpmd.h"

/* *** queue region set *** */
struct cmd_queue_region_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t region;
	uint8_t region_id;
	cmdline_fixed_string_t queue_start_index;
	uint8_t queue_id;
	cmdline_fixed_string_t queue_num;
	uint8_t queue_num_value;
};

static void
cmd_queue_region_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_queue_region_result *res = parsed_result;
	int ret = -ENOTSUP;
	struct rte_pmd_i40e_queue_region_conf region_conf;
	enum rte_pmd_i40e_queue_region_op op_type;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	memset(&region_conf, 0, sizeof(region_conf));
	op_type = RTE_PMD_I40E_RSS_QUEUE_REGION_SET;
	region_conf.region_id = res->region_id;
	region_conf.queue_num = res->queue_num_value;
	region_conf.queue_start_index = res->queue_id;

	ret = rte_pmd_i40e_rss_queue_region_conf(res->port_id,
				op_type, &region_conf);
	switch (ret) {
	case 0:
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented or supported\n");
		break;
	default:
		fprintf(stderr, "queue region config error: (%s)\n",
			strerror(-ret));
	}
}

static cmdline_parse_token_string_t cmd_queue_region_set =
	TOKEN_STRING_INITIALIZER(struct cmd_queue_region_result,
		set, "set");
static cmdline_parse_token_string_t cmd_queue_region_port =
	TOKEN_STRING_INITIALIZER(struct cmd_queue_region_result,
		port, "port");
static cmdline_parse_token_num_t cmd_queue_region_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_queue_region_result,
		port_id, RTE_UINT16);
static cmdline_parse_token_string_t cmd_queue_region_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_queue_region_result,
		cmd, "queue-region");
static cmdline_parse_token_string_t cmd_queue_region_id =
	TOKEN_STRING_INITIALIZER(struct cmd_queue_region_result,
		region, "region_id");
static cmdline_parse_token_num_t cmd_queue_region_index =
	TOKEN_NUM_INITIALIZER(struct cmd_queue_region_result,
		region_id, RTE_UINT8);
static cmdline_parse_token_string_t cmd_queue_region_queue_start_index =
	TOKEN_STRING_INITIALIZER(struct cmd_queue_region_result,
		queue_start_index, "queue_start_index");
static cmdline_parse_token_num_t cmd_queue_region_queue_id =
	TOKEN_NUM_INITIALIZER(struct cmd_queue_region_result,
		queue_id, RTE_UINT8);
static cmdline_parse_token_string_t cmd_queue_region_queue_num =
	TOKEN_STRING_INITIALIZER(struct cmd_queue_region_result,
		queue_num, "queue_num");
static cmdline_parse_token_num_t cmd_queue_region_queue_num_value =
	TOKEN_NUM_INITIALIZER(struct cmd_queue_region_result,
		queue_num_value, RTE_UINT8);

static cmdline_parse_inst_t cmd_queue_region = {
	.f = cmd_queue_region_parsed,
	.data = NULL,
	.help_str = "set port <port_id> queue-region region_id <value> "
		"queue_start_index <value> queue_num <value>: Set a queue region",
	.tokens = {
		(void *)&cmd_queue_region_set,
		(void *)&cmd_queue_region_port,
		(void *)&cmd_queue_region_port_id,
		(void *)&cmd_queue_region_cmd,
		(void *)&cmd_queue_region_id,
		(void *)&cmd_queue_region_index,
		(void *)&cmd_queue_region_queue_start_index,
		(void *)&cmd_queue_region_queue_id,
		(void *)&cmd_queue_region_queue_num,
		(void *)&cmd_queue_region_queue_num_value,
		NULL,
	},
};

/* *** queue region and flowtype set *** */
struct cmd_region_flowtype_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t region;
	uint8_t region_id;
	cmdline_fixed_string_t flowtype;
	uint8_t flowtype_id;
};

static void
cmd_region_flowtype_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_region_flowtype_result *res = parsed_result;
	int ret = -ENOTSUP;
	struct rte_pmd_i40e_queue_region_conf region_conf;
	enum rte_pmd_i40e_queue_region_op op_type;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	memset(&region_conf, 0, sizeof(region_conf));

	op_type = RTE_PMD_I40E_RSS_QUEUE_REGION_FLOWTYPE_SET;
	region_conf.region_id = res->region_id;
	region_conf.hw_flowtype = res->flowtype_id;

	ret = rte_pmd_i40e_rss_queue_region_conf(res->port_id,
			op_type, &region_conf);
	switch (ret) {
	case 0:
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented or supported\n");
		break;
	default:
		fprintf(stderr, "region flowtype config error: (%s)\n",
			strerror(-ret));
	}
}

static cmdline_parse_token_string_t cmd_region_flowtype_set =
	TOKEN_STRING_INITIALIZER(struct cmd_region_flowtype_result,
		set, "set");
static cmdline_parse_token_string_t cmd_region_flowtype_port =
	TOKEN_STRING_INITIALIZER(struct cmd_region_flowtype_result,
		port, "port");
static cmdline_parse_token_num_t cmd_region_flowtype_port_index =
	TOKEN_NUM_INITIALIZER(struct cmd_region_flowtype_result,
		port_id, RTE_UINT16);
static cmdline_parse_token_string_t cmd_region_flowtype_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_region_flowtype_result,
		cmd, "queue-region");
static cmdline_parse_token_string_t cmd_region_flowtype_index =
	TOKEN_STRING_INITIALIZER(struct cmd_region_flowtype_result,
		region, "region_id");
static cmdline_parse_token_num_t cmd_region_flowtype_id =
	TOKEN_NUM_INITIALIZER(struct cmd_region_flowtype_result,
		region_id, RTE_UINT8);
static cmdline_parse_token_string_t cmd_region_flowtype_flow_index =
	TOKEN_STRING_INITIALIZER(struct cmd_region_flowtype_result,
		flowtype, "flowtype");
static cmdline_parse_token_num_t cmd_region_flowtype_flow_id =
	TOKEN_NUM_INITIALIZER(struct cmd_region_flowtype_result,
		flowtype_id, RTE_UINT8);
static cmdline_parse_inst_t cmd_region_flowtype = {
	.f = cmd_region_flowtype_parsed,
	.data = NULL,
	.help_str = "set port <port_id> queue-region region_id <value> "
		"flowtype <value>: Set a flowtype region index",
	.tokens = {
		(void *)&cmd_region_flowtype_set,
		(void *)&cmd_region_flowtype_port,
		(void *)&cmd_region_flowtype_port_index,
		(void *)&cmd_region_flowtype_cmd,
		(void *)&cmd_region_flowtype_index,
		(void *)&cmd_region_flowtype_id,
		(void *)&cmd_region_flowtype_flow_index,
		(void *)&cmd_region_flowtype_flow_id,
		NULL,
	},
};

/* *** User Priority (UP) to queue region (region_id) set *** */
struct cmd_user_priority_region_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t user_priority;
	uint8_t user_priority_id;
	cmdline_fixed_string_t region;
	uint8_t region_id;
};

static void
cmd_user_priority_region_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_user_priority_region_result *res = parsed_result;
	int ret = -ENOTSUP;
	struct rte_pmd_i40e_queue_region_conf region_conf;
	enum rte_pmd_i40e_queue_region_op op_type;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	memset(&region_conf, 0, sizeof(region_conf));
	op_type = RTE_PMD_I40E_RSS_QUEUE_REGION_USER_PRIORITY_SET;
	region_conf.user_priority = res->user_priority_id;
	region_conf.region_id = res->region_id;

	ret = rte_pmd_i40e_rss_queue_region_conf(res->port_id,
				op_type, &region_conf);
	switch (ret) {
	case 0:
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented or supported\n");
		break;
	default:
		fprintf(stderr, "user_priority region config error: (%s)\n",
			strerror(-ret));
	}
}

static cmdline_parse_token_string_t cmd_user_priority_region_set =
	TOKEN_STRING_INITIALIZER(struct cmd_user_priority_region_result,
		set, "set");
static cmdline_parse_token_string_t cmd_user_priority_region_port =
	TOKEN_STRING_INITIALIZER(struct cmd_user_priority_region_result,
		port, "port");
static cmdline_parse_token_num_t cmd_user_priority_region_port_index =
	TOKEN_NUM_INITIALIZER(struct cmd_user_priority_region_result,
		port_id, RTE_UINT16);
static cmdline_parse_token_string_t cmd_user_priority_region_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_user_priority_region_result,
		cmd, "queue-region");
static cmdline_parse_token_string_t cmd_user_priority_region_UP =
	TOKEN_STRING_INITIALIZER(struct cmd_user_priority_region_result,
		user_priority, "UP");
static cmdline_parse_token_num_t cmd_user_priority_region_UP_id =
	TOKEN_NUM_INITIALIZER(struct cmd_user_priority_region_result,
		user_priority_id, RTE_UINT8);
static cmdline_parse_token_string_t cmd_user_priority_region_region =
	TOKEN_STRING_INITIALIZER(struct cmd_user_priority_region_result,
		region, "region_id");
static cmdline_parse_token_num_t cmd_user_priority_region_region_id =
	TOKEN_NUM_INITIALIZER(struct cmd_user_priority_region_result,
		region_id, RTE_UINT8);

static cmdline_parse_inst_t cmd_user_priority_region = {
	.f = cmd_user_priority_region_parsed,
	.data = NULL,
	.help_str = "set port <port_id> queue-region UP <value> "
		"region_id <value>: Set the mapping of User Priority (UP) "
		"to queue region (region_id) ",
	.tokens = {
		(void *)&cmd_user_priority_region_set,
		(void *)&cmd_user_priority_region_port,
		(void *)&cmd_user_priority_region_port_index,
		(void *)&cmd_user_priority_region_cmd,
		(void *)&cmd_user_priority_region_UP,
		(void *)&cmd_user_priority_region_UP_id,
		(void *)&cmd_user_priority_region_region,
		(void *)&cmd_user_priority_region_region_id,
		NULL,
	},
};

/* *** flush all queue region related configuration *** */
struct cmd_flush_queue_region_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t flush;
	cmdline_fixed_string_t what;
};

static void
cmd_flush_queue_region_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_flush_queue_region_result *res = parsed_result;
	int ret = -ENOTSUP;
	struct rte_pmd_i40e_queue_region_conf region_conf;
	enum rte_pmd_i40e_queue_region_op op_type;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	memset(&region_conf, 0, sizeof(region_conf));

	if (strcmp(res->what, "on") == 0)
		op_type = RTE_PMD_I40E_RSS_QUEUE_REGION_ALL_FLUSH_ON;
	else
		op_type = RTE_PMD_I40E_RSS_QUEUE_REGION_ALL_FLUSH_OFF;

	ret = rte_pmd_i40e_rss_queue_region_conf(res->port_id,
				op_type, &region_conf);
	switch (ret) {
	case 0:
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented or supported\n");
		break;
	default:
		fprintf(stderr, "queue region config flush error: (%s)\n",
			strerror(-ret));
	}
}

static cmdline_parse_token_string_t cmd_flush_queue_region_set =
	TOKEN_STRING_INITIALIZER(struct cmd_flush_queue_region_result,
		set, "set");
static cmdline_parse_token_string_t cmd_flush_queue_region_port =
	TOKEN_STRING_INITIALIZER(struct cmd_flush_queue_region_result,
		port, "port");
static cmdline_parse_token_num_t cmd_flush_queue_region_port_index =
	TOKEN_NUM_INITIALIZER(struct cmd_flush_queue_region_result,
		port_id, RTE_UINT16);
static cmdline_parse_token_string_t cmd_flush_queue_region_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_flush_queue_region_result,
		cmd, "queue-region");
static cmdline_parse_token_string_t cmd_flush_queue_region_flush =
	TOKEN_STRING_INITIALIZER(struct cmd_flush_queue_region_result,
		flush, "flush");
static cmdline_parse_token_string_t cmd_flush_queue_region_what =
	TOKEN_STRING_INITIALIZER(struct cmd_flush_queue_region_result,
		what, "on#off");

static cmdline_parse_inst_t cmd_flush_queue_region = {
	.f = cmd_flush_queue_region_parsed,
	.data = NULL,
	.help_str = "set port <port_id> queue-region flush on|off"
		": flush all queue region related configuration",
	.tokens = {
		(void *)&cmd_flush_queue_region_set,
		(void *)&cmd_flush_queue_region_port,
		(void *)&cmd_flush_queue_region_port_index,
		(void *)&cmd_flush_queue_region_cmd,
		(void *)&cmd_flush_queue_region_flush,
		(void *)&cmd_flush_queue_region_what,
		NULL,
	},
};

/* *** get all queue region related configuration info *** */
struct cmd_show_queue_region_info {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t cmd;
};

static void
port_queue_region_info_display(portid_t port_id, void *buf)
{
	uint16_t i, j;
	struct rte_pmd_i40e_queue_regions *info =
		(struct rte_pmd_i40e_queue_regions *)buf;
	static const char *queue_region_info_stats_border = "-------";

	if (!info->queue_region_number)
		printf("there is no region has been set before");

	printf("\n	%s All queue region info for port=%2d %s",
			queue_region_info_stats_border, port_id,
			queue_region_info_stats_border);
	printf("\n	queue_region_number: %-14u\n",
			info->queue_region_number);

	for (i = 0; i < info->queue_region_number; i++) {
		printf("\n	region_id: %-14u queue_number: %-14u "
			"queue_start_index: %-14u\n",
			info->region[i].region_id,
			info->region[i].queue_num,
			info->region[i].queue_start_index);

		printf("  user_priority_num is	%-14u :",
					info->region[i].user_priority_num);
		for (j = 0; j < info->region[i].user_priority_num; j++)
			printf(" %-14u ", info->region[i].user_priority[j]);

		printf("\n	flowtype_num is  %-14u :",
				info->region[i].flowtype_num);
		for (j = 0; j < info->region[i].flowtype_num; j++)
			printf(" %-14u ", info->region[i].hw_flowtype[j]);
	}

	printf("\n\n");
}

static void
cmd_show_queue_region_info_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_show_queue_region_info *res = parsed_result;
	int ret = -ENOTSUP;
	struct rte_pmd_i40e_queue_regions rte_pmd_regions;
	enum rte_pmd_i40e_queue_region_op op_type;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	memset(&rte_pmd_regions, 0, sizeof(rte_pmd_regions));

	op_type = RTE_PMD_I40E_RSS_QUEUE_REGION_INFO_GET;

	ret = rte_pmd_i40e_rss_queue_region_conf(res->port_id,
					op_type, &rte_pmd_regions);

	port_queue_region_info_display(res->port_id, &rte_pmd_regions);
	switch (ret) {
	case 0:
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented or supported\n");
		break;
	default:
		fprintf(stderr, "queue region config info show error: (%s)\n",
			strerror(-ret));
	}
}

static cmdline_parse_token_string_t cmd_show_queue_region_info_get =
	TOKEN_STRING_INITIALIZER(struct cmd_show_queue_region_info,
		show, "show");
static cmdline_parse_token_string_t cmd_show_queue_region_info_port =
	TOKEN_STRING_INITIALIZER(struct cmd_show_queue_region_info,
		port, "port");
static cmdline_parse_token_num_t cmd_show_queue_region_info_port_index =
	TOKEN_NUM_INITIALIZER(struct cmd_show_queue_region_info,
		port_id, RTE_UINT16);
static cmdline_parse_token_string_t cmd_show_queue_region_info_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_show_queue_region_info,
		cmd, "queue-region");

static cmdline_parse_inst_t cmd_show_queue_region_info_all = {
	.f = cmd_show_queue_region_info_parsed,
	.data = NULL,
	.help_str = "show port <port_id> queue-region"
		": show all queue region related configuration info",
	.tokens = {
		(void *)&cmd_show_queue_region_info_get,
		(void *)&cmd_show_queue_region_info_port,
		(void *)&cmd_show_queue_region_info_port_index,
		(void *)&cmd_show_queue_region_info_cmd,
		NULL,
	},
};

/* *** deal with flow director filter *** */
struct cmd_flow_director_result {
	cmdline_fixed_string_t flow_director_filter;
	portid_t port_id;
	cmdline_fixed_string_t mode;
	cmdline_fixed_string_t mode_value;
	cmdline_fixed_string_t ops;
	cmdline_fixed_string_t flow;
	cmdline_fixed_string_t flow_type;
	cmdline_fixed_string_t drop;
	cmdline_fixed_string_t queue;
	uint16_t queue_id;
	cmdline_fixed_string_t fd_id;
	uint32_t fd_id_value;
	cmdline_fixed_string_t packet;
	char filepath[];
};

static void
cmd_flow_director_filter_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_flow_director_result *res = parsed_result;
	int ret = 0;
	struct rte_pmd_i40e_flow_type_mapping
			mapping[RTE_PMD_I40E_FLOW_TYPE_MAX];
	struct rte_pmd_i40e_pkt_template_conf conf;
	uint16_t flow_type = str_to_flowtype(res->flow_type);
	uint16_t i, port = res->port_id;
	uint8_t add;

	memset(&conf, 0, sizeof(conf));

	if (flow_type == RTE_ETH_FLOW_UNKNOWN) {
		fprintf(stderr, "Invalid flow type specified.\n");
		return;
	}
	ret = rte_pmd_i40e_flow_type_mapping_get(res->port_id,
						 mapping);
	if (ret)
		return;
	if (mapping[flow_type].pctype == 0ULL) {
		fprintf(stderr, "Invalid flow type specified.\n");
		return;
	}
	for (i = 0; i < RTE_PMD_I40E_PCTYPE_MAX; i++) {
		if (mapping[flow_type].pctype & (1ULL << i)) {
			conf.input.pctype = i;
			break;
		}
	}

	conf.input.packet = open_file(res->filepath,
				&conf.input.length);
	if (!conf.input.packet)
		return;
	if (!strcmp(res->drop, "drop"))
		conf.action.behavior =
			RTE_PMD_I40E_PKT_TEMPLATE_REJECT;
	else
		conf.action.behavior =
			RTE_PMD_I40E_PKT_TEMPLATE_ACCEPT;
	conf.action.report_status =
			RTE_PMD_I40E_PKT_TEMPLATE_REPORT_ID;
	conf.action.rx_queue = res->queue_id;
	conf.soft_id = res->fd_id_value;
	add = strcmp(res->ops, "del") ? 1 : 0;
	ret = rte_pmd_i40e_flow_add_del_packet_template(port,
							&conf,
							add);
	if (ret < 0)
		fprintf(stderr, "flow director config error: (%s)\n",
			strerror(-ret));
	close_file(conf.input.packet);
}

static cmdline_parse_token_string_t cmd_flow_director_filter =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
		flow_director_filter, "flow_director_filter");
static cmdline_parse_token_num_t cmd_flow_director_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_result,
		port_id, RTE_UINT16);
static cmdline_parse_token_string_t cmd_flow_director_ops =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
		ops, "add#del#update");
static cmdline_parse_token_string_t cmd_flow_director_flow =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
		flow, "flow");
static cmdline_parse_token_string_t cmd_flow_director_flow_type =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
		flow_type, NULL);
static cmdline_parse_token_string_t cmd_flow_director_drop =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
		drop, "drop#fwd");
static cmdline_parse_token_string_t cmd_flow_director_queue =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
		queue, "queue");
static cmdline_parse_token_num_t cmd_flow_director_queue_id =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_result,
		queue_id, RTE_UINT16);
static cmdline_parse_token_string_t cmd_flow_director_fd_id =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
		fd_id, "fd_id");
static cmdline_parse_token_num_t cmd_flow_director_fd_id_value =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_result,
		fd_id_value, RTE_UINT32);
static cmdline_parse_token_string_t cmd_flow_director_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
		mode, "mode");
static cmdline_parse_token_string_t cmd_flow_director_mode_raw =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
		mode_value, "raw");
static cmdline_parse_token_string_t cmd_flow_director_packet =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
		packet, "packet");
static cmdline_parse_token_string_t cmd_flow_director_filepath =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
		filepath, NULL);

static cmdline_parse_inst_t cmd_add_del_raw_flow_director = {
	.f = cmd_flow_director_filter_parsed,
	.data = NULL,
	.help_str = "flow_director_filter ... : Add or delete a raw flow "
		"director entry on NIC",
	.tokens = {
		(void *)&cmd_flow_director_filter,
		(void *)&cmd_flow_director_port_id,
		(void *)&cmd_flow_director_mode,
		(void *)&cmd_flow_director_mode_raw,
		(void *)&cmd_flow_director_ops,
		(void *)&cmd_flow_director_flow,
		(void *)&cmd_flow_director_flow_type,
		(void *)&cmd_flow_director_drop,
		(void *)&cmd_flow_director_queue,
		(void *)&cmd_flow_director_queue_id,
		(void *)&cmd_flow_director_fd_id,
		(void *)&cmd_flow_director_fd_id_value,
		(void *)&cmd_flow_director_packet,
		(void *)&cmd_flow_director_filepath,
		NULL,
	},
};

/* VF unicast promiscuous mode configuration */

/* Common result structure for VF unicast promiscuous mode */
struct cmd_vf_promisc_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t vf;
	cmdline_fixed_string_t promisc;
	portid_t port_id;
	uint32_t vf_id;
	cmdline_fixed_string_t on_off;
};

/* Common CLI fields for VF unicast promiscuous mode enable disable */
static cmdline_parse_token_string_t cmd_vf_promisc_set =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_promisc_result,
		set, "set");
static cmdline_parse_token_string_t cmd_vf_promisc_vf =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_promisc_result,
		vf, "vf");
static cmdline_parse_token_string_t cmd_vf_promisc_promisc =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_promisc_result,
		promisc, "promisc");
static cmdline_parse_token_num_t cmd_vf_promisc_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_promisc_result,
		port_id, RTE_UINT16);
static cmdline_parse_token_num_t cmd_vf_promisc_vf_id =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_promisc_result,
		vf_id, RTE_UINT32);
static cmdline_parse_token_string_t cmd_vf_promisc_on_off =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_promisc_result,
		on_off, "on#off");

static void
cmd_set_vf_promisc_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_vf_promisc_result *res = parsed_result;
	int ret = -ENOTSUP;

	__rte_unused int is_on = (strcmp(res->on_off, "on") == 0) ? 1 : 0;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	ret = rte_pmd_i40e_set_vf_unicast_promisc(res->port_id, res->vf_id, is_on);
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid vf_id %d\n", res->vf_id);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

static cmdline_parse_inst_t cmd_set_vf_promisc = {
	.f = cmd_set_vf_promisc_parsed,
	.data = NULL,
	.help_str = "set vf promisc <port_id> <vf_id> on|off: "
		"Set unicast promiscuous mode for a VF from the PF",
	.tokens = {
		(void *)&cmd_vf_promisc_set,
		(void *)&cmd_vf_promisc_vf,
		(void *)&cmd_vf_promisc_promisc,
		(void *)&cmd_vf_promisc_port_id,
		(void *)&cmd_vf_promisc_vf_id,
		(void *)&cmd_vf_promisc_on_off,
		NULL,
	},
};

/* VF multicast promiscuous mode configuration */

/* Common result structure for VF multicast promiscuous mode */
struct cmd_vf_allmulti_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t vf;
	cmdline_fixed_string_t allmulti;
	portid_t port_id;
	uint32_t vf_id;
	cmdline_fixed_string_t on_off;
};

/* Common CLI fields for VF multicast promiscuous mode enable disable */
static cmdline_parse_token_string_t cmd_vf_allmulti_set =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_allmulti_result,
		set, "set");
static cmdline_parse_token_string_t cmd_vf_allmulti_vf =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_allmulti_result,
		vf, "vf");
static cmdline_parse_token_string_t cmd_vf_allmulti_allmulti =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_allmulti_result,
		allmulti, "allmulti");
static cmdline_parse_token_num_t cmd_vf_allmulti_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_allmulti_result,
		port_id, RTE_UINT16);
static cmdline_parse_token_num_t cmd_vf_allmulti_vf_id =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_allmulti_result,
		vf_id, RTE_UINT32);
static cmdline_parse_token_string_t cmd_vf_allmulti_on_off =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_allmulti_result,
		on_off, "on#off");

static void
cmd_set_vf_allmulti_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_vf_allmulti_result *res = parsed_result;
	int ret = -ENOTSUP;

	__rte_unused int is_on = (strcmp(res->on_off, "on") == 0) ? 1 : 0;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	ret = rte_pmd_i40e_set_vf_multicast_promisc(res->port_id, res->vf_id, is_on);
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid vf_id %d\n", res->vf_id);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

static cmdline_parse_inst_t cmd_set_vf_allmulti = {
	.f = cmd_set_vf_allmulti_parsed,
	.data = NULL,
	.help_str = "set vf allmulti <port_id> <vf_id> on|off: "
		"Set multicast promiscuous mode for a VF from the PF",
	.tokens = {
		(void *)&cmd_vf_allmulti_set,
		(void *)&cmd_vf_allmulti_vf,
		(void *)&cmd_vf_allmulti_allmulti,
		(void *)&cmd_vf_allmulti_port_id,
		(void *)&cmd_vf_allmulti_vf_id,
		(void *)&cmd_vf_allmulti_on_off,
		NULL,
	},
};

/* vf broadcast mode configuration */

/* Common result structure for vf broadcast */
struct cmd_set_vf_broadcast_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t vf;
	cmdline_fixed_string_t broadcast;
	portid_t port_id;
	uint16_t vf_id;
	cmdline_fixed_string_t on_off;
};

/* Common CLI fields for vf broadcast enable disable */
static cmdline_parse_token_string_t cmd_set_vf_broadcast_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_broadcast_result,
		set, "set");
static cmdline_parse_token_string_t cmd_set_vf_broadcast_vf =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_broadcast_result,
		vf, "vf");
static cmdline_parse_token_string_t cmd_set_vf_broadcast_broadcast =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_broadcast_result,
		broadcast, "broadcast");
static cmdline_parse_token_num_t cmd_set_vf_broadcast_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_set_vf_broadcast_result,
		port_id, RTE_UINT16);
static cmdline_parse_token_num_t cmd_set_vf_broadcast_vf_id =
	TOKEN_NUM_INITIALIZER(struct cmd_set_vf_broadcast_result,
		vf_id, RTE_UINT16);
static cmdline_parse_token_string_t cmd_set_vf_broadcast_on_off =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_broadcast_result,
		on_off, "on#off");

static void
cmd_set_vf_broadcast_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_set_vf_broadcast_result *res = parsed_result;
	int ret = -ENOTSUP;

	__rte_unused int is_on = (strcmp(res->on_off, "on") == 0) ? 1 : 0;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	ret = rte_pmd_i40e_set_vf_broadcast(res->port_id, res->vf_id, is_on);
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid vf_id %d or is_on %d\n",
			res->vf_id, is_on);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

static cmdline_parse_inst_t cmd_set_vf_broadcast = {
	.f = cmd_set_vf_broadcast_parsed,
	.data = NULL,
	.help_str = "set vf broadcast <port_id> <vf_id> on|off",
	.tokens = {
		(void *)&cmd_set_vf_broadcast_set,
		(void *)&cmd_set_vf_broadcast_vf,
		(void *)&cmd_set_vf_broadcast_broadcast,
		(void *)&cmd_set_vf_broadcast_port_id,
		(void *)&cmd_set_vf_broadcast_vf_id,
		(void *)&cmd_set_vf_broadcast_on_off,
		NULL,
	},
};

/* vf vlan tag configuration */

/* Common result structure for vf vlan tag */
struct cmd_set_vf_vlan_tag_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t vf;
	cmdline_fixed_string_t vlan;
	cmdline_fixed_string_t tag;
	portid_t port_id;
	uint16_t vf_id;
	cmdline_fixed_string_t on_off;
};

/* Common CLI fields for vf vlan tag enable disable */
static cmdline_parse_token_string_t cmd_set_vf_vlan_tag_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_vlan_tag_result,
		set, "set");
static cmdline_parse_token_string_t cmd_set_vf_vlan_tag_vf =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_vlan_tag_result,
		vf, "vf");
static cmdline_parse_token_string_t cmd_set_vf_vlan_tag_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_vlan_tag_result,
		vlan, "vlan");
static cmdline_parse_token_string_t cmd_set_vf_vlan_tag_tag =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_vlan_tag_result,
		tag, "tag");
static cmdline_parse_token_num_t cmd_set_vf_vlan_tag_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_set_vf_vlan_tag_result,
		port_id, RTE_UINT16);
static cmdline_parse_token_num_t cmd_set_vf_vlan_tag_vf_id =
	TOKEN_NUM_INITIALIZER(struct cmd_set_vf_vlan_tag_result,
		vf_id, RTE_UINT16);
static cmdline_parse_token_string_t cmd_set_vf_vlan_tag_on_off =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_vlan_tag_result,
		on_off, "on#off");

static void
cmd_set_vf_vlan_tag_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_set_vf_vlan_tag_result *res = parsed_result;
	int ret = -ENOTSUP;

	__rte_unused int is_on = (strcmp(res->on_off, "on") == 0) ? 1 : 0;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	ret = rte_pmd_i40e_set_vf_vlan_tag(res->port_id, res->vf_id, is_on);
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid vf_id %d or is_on %d\n",
			res->vf_id, is_on);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

static cmdline_parse_inst_t cmd_set_vf_vlan_tag = {
	.f = cmd_set_vf_vlan_tag_parsed,
	.data = NULL,
	.help_str = "set vf vlan tag <port_id> <vf_id> on|off",
	.tokens = {
		(void *)&cmd_set_vf_vlan_tag_set,
		(void *)&cmd_set_vf_vlan_tag_vf,
		(void *)&cmd_set_vf_vlan_tag_vlan,
		(void *)&cmd_set_vf_vlan_tag_tag,
		(void *)&cmd_set_vf_vlan_tag_port_id,
		(void *)&cmd_set_vf_vlan_tag_vf_id,
		(void *)&cmd_set_vf_vlan_tag_on_off,
		NULL,
	},
};

/* Common definition of VF and TC TX bandwidth configuration */
struct cmd_vf_tc_bw_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t vf;
	cmdline_fixed_string_t tc;
	cmdline_fixed_string_t tx;
	cmdline_fixed_string_t max_bw;
	cmdline_fixed_string_t min_bw;
	cmdline_fixed_string_t strict_link_prio;
	portid_t port_id;
	uint16_t vf_id;
	uint8_t tc_no;
	uint32_t bw;
	cmdline_fixed_string_t bw_list;
	uint8_t tc_map;
};

static cmdline_parse_token_string_t cmd_vf_tc_bw_set =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_tc_bw_result,
		set, "set");
static cmdline_parse_token_string_t cmd_vf_tc_bw_tc =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_tc_bw_result,
		tc, "tc");
static cmdline_parse_token_string_t cmd_vf_tc_bw_tx =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_tc_bw_result,
		tx, "tx");
static cmdline_parse_token_string_t cmd_vf_tc_bw_strict_link_prio =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_tc_bw_result,
		strict_link_prio, "strict-link-priority");
static cmdline_parse_token_string_t cmd_vf_tc_bw_max_bw =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_tc_bw_result,
		max_bw, "max-bandwidth");
static cmdline_parse_token_string_t cmd_vf_tc_bw_min_bw =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_tc_bw_result,
		min_bw, "min-bandwidth");
static cmdline_parse_token_num_t cmd_vf_tc_bw_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_tc_bw_result,
		port_id, RTE_UINT16);
static cmdline_parse_token_num_t cmd_vf_tc_bw_vf_id =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_tc_bw_result,
		vf_id, RTE_UINT16);
static cmdline_parse_token_num_t cmd_vf_tc_bw_tc_no =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_tc_bw_result,
		tc_no, RTE_UINT8);
static cmdline_parse_token_num_t cmd_vf_tc_bw_bw =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_tc_bw_result,
		bw, RTE_UINT32);
static cmdline_parse_token_string_t cmd_vf_tc_bw_bw_list =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_tc_bw_result,
		bw_list, NULL);
static cmdline_parse_token_num_t cmd_vf_tc_bw_tc_map =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_tc_bw_result,
		tc_map, RTE_UINT8);
static cmdline_parse_token_string_t cmd_vf_tc_bw_vf =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_tc_bw_result,
		vf, "vf");

/* VF max bandwidth setting */
static void
cmd_vf_max_bw_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_vf_tc_bw_result *res = parsed_result;
	int ret = -ENOTSUP;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	ret = rte_pmd_i40e_set_vf_max_bw(res->port_id,
					 res->vf_id, res->bw);
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid vf_id %d or bandwidth %d\n",
			res->vf_id, res->bw);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

static cmdline_parse_inst_t cmd_vf_max_bw = {
	.f = cmd_vf_max_bw_parsed,
	.data = NULL,
	.help_str = "set vf tx max-bandwidth <port_id> <vf_id> <bandwidth>",
	.tokens = {
		(void *)&cmd_vf_tc_bw_set,
		(void *)&cmd_vf_tc_bw_vf,
		(void *)&cmd_vf_tc_bw_tx,
		(void *)&cmd_vf_tc_bw_max_bw,
		(void *)&cmd_vf_tc_bw_port_id,
		(void *)&cmd_vf_tc_bw_vf_id,
		(void *)&cmd_vf_tc_bw_bw,
		NULL,
	},
};

static int
vf_tc_min_bw_parse_bw_list(uint8_t *bw_list, uint8_t *tc_num, char *str)
{
	uint32_t size;
	const char *p, *p0 = str;
	char s[256];
	char *end;
	char *str_fld[16];
	uint16_t i;
	int ret;

	p = strchr(p0, '(');
	if (p == NULL) {
		fprintf(stderr,
			"The bandwidth-list should be '(bw1, bw2, ...)'\n");
		return -1;
	}
	p++;
	p0 = strchr(p, ')');
	if (p0 == NULL) {
		fprintf(stderr,
			"The bandwidth-list should be '(bw1, bw2, ...)'\n");
		return -1;
	}
	size = p0 - p;
	if (size >= sizeof(s)) {
		fprintf(stderr,
			"The string size exceeds the internal buffer size\n");
		return -1;
	}
	snprintf(s, sizeof(s), "%.*s", size, p);
	ret = rte_strsplit(s, sizeof(s), str_fld, 16, ',');
	if (ret <= 0) {
		fprintf(stderr, "Failed to get the bandwidth list.\n");
		return -1;
	}
	*tc_num = ret;
	for (i = 0; i < ret; i++)
		bw_list[i] = (uint8_t)strtoul(str_fld[i], &end, 0);

	return 0;
}

/* TC min bandwidth setting */
static void
cmd_vf_tc_min_bw_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_vf_tc_bw_result *res = parsed_result;
	uint8_t tc_num;
	uint8_t bw[16];
	int ret = -ENOTSUP;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	ret = vf_tc_min_bw_parse_bw_list(bw, &tc_num, res->bw_list);
	if (ret)
		return;

	ret = rte_pmd_i40e_set_vf_tc_bw_alloc(res->port_id, res->vf_id, tc_num, bw);
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid vf_id %d or bandwidth\n", res->vf_id);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

static cmdline_parse_inst_t cmd_vf_tc_min_bw = {
	.f = cmd_vf_tc_min_bw_parsed,
	.data = NULL,
	.help_str = "set vf tc tx min-bandwidth <port_id> <vf_id>"
		" <bw1, bw2, ...>",
	.tokens = {
		(void *)&cmd_vf_tc_bw_set,
		(void *)&cmd_vf_tc_bw_vf,
		(void *)&cmd_vf_tc_bw_tc,
		(void *)&cmd_vf_tc_bw_tx,
		(void *)&cmd_vf_tc_bw_min_bw,
		(void *)&cmd_vf_tc_bw_port_id,
		(void *)&cmd_vf_tc_bw_vf_id,
		(void *)&cmd_vf_tc_bw_bw_list,
		NULL,
	},
};

/* TC max bandwidth setting */
static void
cmd_vf_tc_max_bw_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_vf_tc_bw_result *res = parsed_result;
	int ret = -ENOTSUP;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	ret = rte_pmd_i40e_set_vf_tc_max_bw(res->port_id, res->vf_id, res->tc_no, res->bw);
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr,
			"invalid vf_id %d, tc_no %d or bandwidth %d\n",
			res->vf_id, res->tc_no, res->bw);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

static cmdline_parse_inst_t cmd_vf_tc_max_bw = {
	.f = cmd_vf_tc_max_bw_parsed,
	.data = NULL,
	.help_str = "set vf tc tx max-bandwidth <port_id> <vf_id> <tc_no>"
		" <bandwidth>",
	.tokens = {
		(void *)&cmd_vf_tc_bw_set,
		(void *)&cmd_vf_tc_bw_vf,
		(void *)&cmd_vf_tc_bw_tc,
		(void *)&cmd_vf_tc_bw_tx,
		(void *)&cmd_vf_tc_bw_max_bw,
		(void *)&cmd_vf_tc_bw_port_id,
		(void *)&cmd_vf_tc_bw_vf_id,
		(void *)&cmd_vf_tc_bw_tc_no,
		(void *)&cmd_vf_tc_bw_bw,
		NULL,
	},
};

/* Strict link priority scheduling mode setting */
static void
cmd_strict_link_prio_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_vf_tc_bw_result *res = parsed_result;
	int ret = -ENOTSUP;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	ret = rte_pmd_i40e_set_tc_strict_prio(res->port_id, res->tc_map);
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid tc_bitmap 0x%x\n", res->tc_map);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

static cmdline_parse_inst_t cmd_strict_link_prio = {
	.f = cmd_strict_link_prio_parsed,
	.data = NULL,
	.help_str = "set tx strict-link-priority <port_id> <tc_bitmap>",
	.tokens = {
		(void *)&cmd_vf_tc_bw_set,
		(void *)&cmd_vf_tc_bw_tx,
		(void *)&cmd_vf_tc_bw_strict_link_prio,
		(void *)&cmd_vf_tc_bw_port_id,
		(void *)&cmd_vf_tc_bw_tc_map,
		NULL,
	},
};

/* Load dynamic device personalization*/
struct cmd_ddp_add_result {
	cmdline_fixed_string_t ddp;
	cmdline_fixed_string_t add;
	portid_t port_id;
	char filepath[];
};

static cmdline_parse_token_string_t cmd_ddp_add_ddp =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_add_result, ddp, "ddp");
static cmdline_parse_token_string_t cmd_ddp_add_add =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_add_result, add, "add");
static cmdline_parse_token_num_t cmd_ddp_add_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_ddp_add_result, port_id, RTE_UINT16);
static cmdline_parse_token_string_t cmd_ddp_add_filepath =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_add_result, filepath, NULL);

static void
cmd_ddp_add_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_ddp_add_result *res = parsed_result;
	uint8_t *buff;
	uint32_t size;
	char *filepath;
	char *file_fld[2];
	int file_num;
	int ret = -ENOTSUP;

	if (!all_ports_stopped()) {
		fprintf(stderr, "Please stop all ports first\n");
		return;
	}

	filepath = strdup(res->filepath);
	if (filepath == NULL) {
		fprintf(stderr, "Failed to allocate memory\n");
		return;
	}
	file_num = rte_strsplit(filepath, strlen(filepath), file_fld, 2, ',');

	buff = open_file(file_fld[0], &size);
	if (!buff) {
		free((void *)filepath);
		return;
	}

	if (ret == -ENOTSUP)
		ret = rte_pmd_i40e_process_ddp_package(res->port_id, buff, size,
			RTE_PMD_I40E_PKG_OP_WR_ADD);
	if (ret == -EEXIST)
		fprintf(stderr, "Profile has already existed.\n");
	else if (ret < 0)
		fprintf(stderr, "Failed to load profile.\n");
	else if (file_num == 2)
		save_file(file_fld[1], buff, size);

	close_file(buff);
	free((void *)filepath);
}

static cmdline_parse_inst_t cmd_ddp_add = {
	.f = cmd_ddp_add_parsed,
	.data = NULL,
	.help_str = "ddp add <port_id> <profile_path[,backup_profile_path]>",
	.tokens = {
		(void *)&cmd_ddp_add_ddp,
		(void *)&cmd_ddp_add_add,
		(void *)&cmd_ddp_add_port_id,
		(void *)&cmd_ddp_add_filepath,
		NULL,
	},
};

/* Delete dynamic device personalization*/
struct cmd_ddp_del_result {
	cmdline_fixed_string_t ddp;
	cmdline_fixed_string_t del;
	portid_t port_id;
	char filepath[];
};

static cmdline_parse_token_string_t cmd_ddp_del_ddp =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_del_result, ddp, "ddp");
static cmdline_parse_token_string_t cmd_ddp_del_del =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_del_result, del, "del");
static cmdline_parse_token_num_t cmd_ddp_del_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_ddp_del_result, port_id, RTE_UINT16);
static cmdline_parse_token_string_t cmd_ddp_del_filepath =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_del_result, filepath, NULL);

static void
cmd_ddp_del_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_ddp_del_result *res = parsed_result;
	uint8_t *buff;
	uint32_t size;
	int ret = -ENOTSUP;

	if (!all_ports_stopped()) {
		fprintf(stderr, "Please stop all ports first\n");
		return;
	}

	buff = open_file(res->filepath, &size);
	if (!buff)
		return;

	if (ret == -ENOTSUP)
		ret = rte_pmd_i40e_process_ddp_package(res->port_id, buff, size,
			RTE_PMD_I40E_PKG_OP_WR_DEL);
	if (ret == -EACCES)
		fprintf(stderr, "Profile does not exist.\n");
	else if (ret < 0)
		fprintf(stderr, "Failed to delete profile.\n");

	close_file(buff);
}

static cmdline_parse_inst_t cmd_ddp_del = {
	.f = cmd_ddp_del_parsed,
	.data = NULL,
	.help_str = "ddp del <port_id> <backup_profile_path>",
	.tokens = {
		(void *)&cmd_ddp_del_ddp,
		(void *)&cmd_ddp_del_del,
		(void *)&cmd_ddp_del_port_id,
		(void *)&cmd_ddp_del_filepath,
		NULL,
	},
};

/* Get dynamic device personalization profile info */
struct cmd_ddp_info_result {
	cmdline_fixed_string_t ddp;
	cmdline_fixed_string_t get;
	cmdline_fixed_string_t info;
	char filepath[];
};

static cmdline_parse_token_string_t cmd_ddp_info_ddp =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_info_result, ddp, "ddp");
static cmdline_parse_token_string_t cmd_ddp_info_get =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_info_result, get, "get");
static cmdline_parse_token_string_t cmd_ddp_info_info =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_info_result, info, "info");
static cmdline_parse_token_string_t cmd_ddp_info_filepath =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_info_result, filepath, NULL);

static void
cmd_ddp_info_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_ddp_info_result *res = parsed_result;
	uint8_t *pkg;
	uint32_t pkg_size;
	int ret = -ENOTSUP;
	uint32_t i, j, n;
	uint8_t *buff;
	uint32_t buff_size = 0;
	struct rte_pmd_i40e_profile_info info;
	uint32_t dev_num = 0;
	struct rte_pmd_i40e_ddp_device_id *devs;
	uint32_t proto_num = 0;
	struct rte_pmd_i40e_proto_info *proto = NULL;
	uint32_t pctype_num = 0;
	struct rte_pmd_i40e_ptype_info *pctype;
	uint32_t ptype_num = 0;
	struct rte_pmd_i40e_ptype_info *ptype;
	uint8_t proto_id;

	pkg = open_file(res->filepath, &pkg_size);
	if (!pkg)
		return;

	ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size,
				(uint8_t *)&info, sizeof(info),
				RTE_PMD_I40E_PKG_INFO_GLOBAL_HEADER);
	if (!ret) {
		printf("Global Track id:       0x%x\n", info.track_id);
		printf("Global Version:        %d.%d.%d.%d\n",
			info.version.major,
			info.version.minor,
			info.version.update,
			info.version.draft);
		printf("Global Package name:   %s\n\n", info.name);
	}

	ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size,
				(uint8_t *)&info, sizeof(info),
				RTE_PMD_I40E_PKG_INFO_HEADER);
	if (!ret) {
		printf("i40e Profile Track id: 0x%x\n", info.track_id);
		printf("i40e Profile Version:  %d.%d.%d.%d\n",
			info.version.major,
			info.version.minor,
			info.version.update,
			info.version.draft);
		printf("i40e Profile name:     %s\n\n", info.name);
	}

	ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size,
				(uint8_t *)&buff_size, sizeof(buff_size),
				RTE_PMD_I40E_PKG_INFO_GLOBAL_NOTES_SIZE);
	if (!ret && buff_size) {
		buff = (uint8_t *)malloc(buff_size);
		if (buff) {
			ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size,
						buff, buff_size,
						RTE_PMD_I40E_PKG_INFO_GLOBAL_NOTES);
			if (!ret)
				printf("Package Notes:\n%s\n\n", buff);
			free(buff);
		}
	}

	ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size,
				(uint8_t *)&dev_num, sizeof(dev_num),
				RTE_PMD_I40E_PKG_INFO_DEVID_NUM);
	if (!ret && dev_num) {
		buff_size = dev_num * sizeof(struct rte_pmd_i40e_ddp_device_id);
		devs = (struct rte_pmd_i40e_ddp_device_id *)malloc(buff_size);
		if (devs) {
			ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size,
						(uint8_t *)devs, buff_size,
						RTE_PMD_I40E_PKG_INFO_DEVID_LIST);
			if (!ret) {
				printf("List of supported devices:\n");
				for (i = 0; i < dev_num; i++) {
					printf("  %04X:%04X %04X:%04X\n",
						devs[i].vendor_dev_id >> 16,
						devs[i].vendor_dev_id & 0xFFFF,
						devs[i].sub_vendor_dev_id >> 16,
						devs[i].sub_vendor_dev_id & 0xFFFF);
				}
				printf("\n");
			}
			free(devs);
		}
	}

	/* get information about protocols and packet types */
	ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size,
		(uint8_t *)&proto_num, sizeof(proto_num),
		RTE_PMD_I40E_PKG_INFO_PROTOCOL_NUM);
	if (ret || !proto_num)
		goto no_print_return;

	buff_size = proto_num * sizeof(struct rte_pmd_i40e_proto_info);
	proto = (struct rte_pmd_i40e_proto_info *)malloc(buff_size);
	if (!proto)
		goto no_print_return;

	ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size, (uint8_t *)proto,
					buff_size,
					RTE_PMD_I40E_PKG_INFO_PROTOCOL_LIST);
	if (!ret) {
		printf("List of used protocols:\n");
		for (i = 0; i < proto_num; i++)
			printf("  %2u: %s\n", proto[i].proto_id, proto[i].name);
		printf("\n");
	}
	ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size,
		(uint8_t *)&pctype_num, sizeof(pctype_num),
		RTE_PMD_I40E_PKG_INFO_PCTYPE_NUM);
	if (ret || !pctype_num)
		goto no_print_pctypes;

	buff_size = pctype_num * sizeof(struct rte_pmd_i40e_ptype_info);
	pctype = (struct rte_pmd_i40e_ptype_info *)malloc(buff_size);
	if (!pctype)
		goto no_print_pctypes;

	ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size, (uint8_t *)pctype,
					buff_size,
					RTE_PMD_I40E_PKG_INFO_PCTYPE_LIST);
	if (ret) {
		free(pctype);
		goto no_print_pctypes;
	}

	printf("List of defined packet classification types:\n");
	for (i = 0; i < pctype_num; i++) {
		printf("  %2u:", pctype[i].ptype_id);
		for (j = 0; j < RTE_PMD_I40E_PROTO_NUM; j++) {
			proto_id = pctype[i].protocols[j];
			if (proto_id != RTE_PMD_I40E_PROTO_UNUSED) {
				for (n = 0; n < proto_num; n++) {
					if (proto[n].proto_id == proto_id) {
						printf(" %s", proto[n].name);
						break;
					}
				}
			}
		}
		printf("\n");
	}
	printf("\n");
	free(pctype);

no_print_pctypes:

	ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size, (uint8_t *)&ptype_num,
					sizeof(ptype_num),
					RTE_PMD_I40E_PKG_INFO_PTYPE_NUM);
	if (ret || !ptype_num)
		goto no_print_return;

	buff_size = ptype_num * sizeof(struct rte_pmd_i40e_ptype_info);
	ptype = (struct rte_pmd_i40e_ptype_info *)malloc(buff_size);
	if (!ptype)
		goto no_print_return;

	ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size, (uint8_t *)ptype,
					buff_size,
					RTE_PMD_I40E_PKG_INFO_PTYPE_LIST);
	if (ret) {
		free(ptype);
		goto no_print_return;
	}
	printf("List of defined packet types:\n");
	for (i = 0; i < ptype_num; i++) {
		printf("  %2u:", ptype[i].ptype_id);
		for (j = 0; j < RTE_PMD_I40E_PROTO_NUM; j++) {
			proto_id = ptype[i].protocols[j];
			if (proto_id != RTE_PMD_I40E_PROTO_UNUSED) {
				for (n = 0; n < proto_num; n++) {
					if (proto[n].proto_id == proto_id) {
						printf(" %s", proto[n].name);
						break;
					}
				}
			}
		}
		printf("\n");
	}
	free(ptype);
	printf("\n");

	ret = 0;
no_print_return:
	free(proto);
	if (ret == -ENOTSUP)
		fprintf(stderr, "Function not supported in PMD\n");
	close_file(pkg);
}

static cmdline_parse_inst_t cmd_ddp_get_info = {
	.f = cmd_ddp_info_parsed,
	.data = NULL,
	.help_str = "ddp get info <profile_path>",
	.tokens = {
		(void *)&cmd_ddp_info_ddp,
		(void *)&cmd_ddp_info_get,
		(void *)&cmd_ddp_info_info,
		(void *)&cmd_ddp_info_filepath,
		NULL,
	},
};

/* Get dynamic device personalization profile info list*/
#define PROFILE_INFO_SIZE 48
#define MAX_PROFILE_NUM 16

struct cmd_ddp_get_list_result {
	cmdline_fixed_string_t ddp;
	cmdline_fixed_string_t get;
	cmdline_fixed_string_t list;
	portid_t port_id;
};

static cmdline_parse_token_string_t cmd_ddp_get_list_ddp =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_get_list_result, ddp, "ddp");
static cmdline_parse_token_string_t cmd_ddp_get_list_get =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_get_list_result, get, "get");
static cmdline_parse_token_string_t cmd_ddp_get_list_list =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_get_list_result, list, "list");
static cmdline_parse_token_num_t cmd_ddp_get_list_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_ddp_get_list_result, port_id,
		RTE_UINT16);

static void
cmd_ddp_get_list_parsed(__rte_unused void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_ddp_get_list_result *res = parsed_result;
	struct rte_pmd_i40e_profile_list *p_list;
	struct rte_pmd_i40e_profile_info *p_info;
	uint32_t p_num;
	uint32_t size;
	uint32_t i;
	int ret = -ENOTSUP;

	size = PROFILE_INFO_SIZE * MAX_PROFILE_NUM + 4;
	p_list = (struct rte_pmd_i40e_profile_list *)malloc(size);
	if (!p_list) {
		fprintf(stderr, "%s: Failed to malloc buffer\n", __func__);
		return;
	}

	if (ret == -ENOTSUP)
		ret = rte_pmd_i40e_get_ddp_list(res->port_id,
						(uint8_t *)p_list, size);

	if (!ret) {
		p_num = p_list->p_count;
		printf("Profile number is: %d\n\n", p_num);

		for (i = 0; i < p_num; i++) {
			p_info = &p_list->p_info[i];
			printf("Profile %d:\n", i);
			printf("Track id:     0x%x\n", p_info->track_id);
			printf("Version:      %d.%d.%d.%d\n",
				p_info->version.major,
				p_info->version.minor,
				p_info->version.update,
				p_info->version.draft);
			printf("Profile name: %s\n\n", p_info->name);
		}
	}

	free(p_list);

	if (ret < 0)
		fprintf(stderr, "Failed to get ddp list\n");
}

static cmdline_parse_inst_t cmd_ddp_get_list = {
	.f = cmd_ddp_get_list_parsed,
	.data = NULL,
	.help_str = "ddp get list <port_id>",
	.tokens = {
		(void *)&cmd_ddp_get_list_ddp,
		(void *)&cmd_ddp_get_list_get,
		(void *)&cmd_ddp_get_list_list,
		(void *)&cmd_ddp_get_list_port_id,
		NULL,
	},
};

/* Configure input set */
struct cmd_cfg_input_set_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t cfg;
	portid_t port_id;
	cmdline_fixed_string_t pctype;
	uint8_t pctype_id;
	cmdline_fixed_string_t inset_type;
	cmdline_fixed_string_t opt;
	cmdline_fixed_string_t field;
	uint8_t field_idx;
};

static void
cmd_cfg_input_set_parsed(__rte_unused void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_cfg_input_set_result *res = parsed_result;
	enum rte_pmd_i40e_inset_type inset_type = INSET_NONE;
	struct rte_pmd_i40e_inset inset;
	int ret = -ENOTSUP;

	if (!all_ports_stopped()) {
		fprintf(stderr, "Please stop all ports first\n");
		return;
	}

	if (!strcmp(res->inset_type, "hash_inset"))
		inset_type = INSET_HASH;
	else if (!strcmp(res->inset_type, "fdir_inset"))
		inset_type = INSET_FDIR;
	else if (!strcmp(res->inset_type, "fdir_flx_inset"))
		inset_type = INSET_FDIR_FLX;
	ret = rte_pmd_i40e_inset_get(res->port_id, res->pctype_id, &inset, inset_type);
	if (ret) {
		fprintf(stderr, "Failed to get input set.\n");
		return;
	}

	if (!strcmp(res->opt, "get")) {
		ret = rte_pmd_i40e_inset_field_get(inset.inset, res->field_idx);
		if (ret)
			printf("Field index %d is enabled.\n", res->field_idx);
		else
			printf("Field index %d is disabled.\n", res->field_idx);
		return;
	}

	if (!strcmp(res->opt, "set"))
		ret = rte_pmd_i40e_inset_field_set(&inset.inset, res->field_idx);
	else if (!strcmp(res->opt, "clear"))
		ret = rte_pmd_i40e_inset_field_clear(&inset.inset, res->field_idx);
	if (ret) {
		fprintf(stderr, "Failed to configure input set field.\n");
		return;
	}

	ret = rte_pmd_i40e_inset_set(res->port_id, res->pctype_id, &inset, inset_type);
	if (ret) {
		fprintf(stderr, "Failed to set input set.\n");
		return;
	}

	if (ret == -ENOTSUP)
		fprintf(stderr, "Function not supported\n");
}

static cmdline_parse_token_string_t cmd_cfg_input_set_port =
	TOKEN_STRING_INITIALIZER(struct cmd_cfg_input_set_result,
		port, "port");
static cmdline_parse_token_string_t cmd_cfg_input_set_cfg =
	TOKEN_STRING_INITIALIZER(struct cmd_cfg_input_set_result,
		cfg, "config");
static cmdline_parse_token_num_t cmd_cfg_input_set_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_cfg_input_set_result,
		port_id, RTE_UINT16);
static cmdline_parse_token_string_t cmd_cfg_input_set_pctype =
	TOKEN_STRING_INITIALIZER(struct cmd_cfg_input_set_result,
		pctype, "pctype");
static cmdline_parse_token_num_t cmd_cfg_input_set_pctype_id =
	TOKEN_NUM_INITIALIZER(struct cmd_cfg_input_set_result,
		pctype_id, RTE_UINT8);
static cmdline_parse_token_string_t cmd_cfg_input_set_inset_type =
	TOKEN_STRING_INITIALIZER(struct cmd_cfg_input_set_result,
		inset_type, "hash_inset#fdir_inset#fdir_flx_inset");
static cmdline_parse_token_string_t cmd_cfg_input_set_opt =
	TOKEN_STRING_INITIALIZER(struct cmd_cfg_input_set_result,
		opt, "get#set#clear");
static cmdline_parse_token_string_t cmd_cfg_input_set_field =
	TOKEN_STRING_INITIALIZER(struct cmd_cfg_input_set_result,
		field, "field");
static cmdline_parse_token_num_t cmd_cfg_input_set_field_idx =
	TOKEN_NUM_INITIALIZER(struct cmd_cfg_input_set_result,
		field_idx, RTE_UINT8);

static cmdline_parse_inst_t cmd_cfg_input_set = {
	.f = cmd_cfg_input_set_parsed,
	.data = NULL,
	.help_str = "port config <port_id> pctype <pctype_id> hash_inset|"
		"fdir_inset|fdir_flx_inset get|set|clear field <field_idx>",
	.tokens = {
		(void *)&cmd_cfg_input_set_port,
		(void *)&cmd_cfg_input_set_cfg,
		(void *)&cmd_cfg_input_set_port_id,
		(void *)&cmd_cfg_input_set_pctype,
		(void *)&cmd_cfg_input_set_pctype_id,
		(void *)&cmd_cfg_input_set_inset_type,
		(void *)&cmd_cfg_input_set_opt,
		(void *)&cmd_cfg_input_set_field,
		(void *)&cmd_cfg_input_set_field_idx,
		NULL,
	},
};

/* Clear input set */
struct cmd_clear_input_set_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t cfg;
	portid_t port_id;
	cmdline_fixed_string_t pctype;
	uint8_t pctype_id;
	cmdline_fixed_string_t inset_type;
	cmdline_fixed_string_t clear;
	cmdline_fixed_string_t all;
};

static void
cmd_clear_input_set_parsed(__rte_unused void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_clear_input_set_result *res = parsed_result;
	enum rte_pmd_i40e_inset_type inset_type = INSET_NONE;
	struct rte_pmd_i40e_inset inset;
	int ret = -ENOTSUP;

	if (!all_ports_stopped()) {
		fprintf(stderr, "Please stop all ports first\n");
		return;
	}

	if (!strcmp(res->inset_type, "hash_inset"))
		inset_type = INSET_HASH;
	else if (!strcmp(res->inset_type, "fdir_inset"))
		inset_type = INSET_FDIR;
	else if (!strcmp(res->inset_type, "fdir_flx_inset"))
		inset_type = INSET_FDIR_FLX;

	memset(&inset, 0, sizeof(inset));

	ret = rte_pmd_i40e_inset_set(res->port_id, res->pctype_id, &inset, inset_type);
	if (ret) {
		fprintf(stderr, "Failed to clear input set.\n");
		return;
	}

	if (ret == -ENOTSUP)
		fprintf(stderr, "Function not supported\n");
}

static cmdline_parse_token_string_t cmd_clear_input_set_port =
	TOKEN_STRING_INITIALIZER(struct cmd_clear_input_set_result,
		port, "port");
static cmdline_parse_token_string_t cmd_clear_input_set_cfg =
	TOKEN_STRING_INITIALIZER(struct cmd_clear_input_set_result,
		cfg, "config");
static cmdline_parse_token_num_t cmd_clear_input_set_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_clear_input_set_result,
		port_id, RTE_UINT16);
static cmdline_parse_token_string_t cmd_clear_input_set_pctype =
	TOKEN_STRING_INITIALIZER(struct cmd_clear_input_set_result,
		pctype, "pctype");
static cmdline_parse_token_num_t cmd_clear_input_set_pctype_id =
	TOKEN_NUM_INITIALIZER(struct cmd_clear_input_set_result,
		pctype_id, RTE_UINT8);
static cmdline_parse_token_string_t cmd_clear_input_set_inset_type =
	TOKEN_STRING_INITIALIZER(struct cmd_clear_input_set_result,
		inset_type, "hash_inset#fdir_inset#fdir_flx_inset");
static cmdline_parse_token_string_t cmd_clear_input_set_clear =
	TOKEN_STRING_INITIALIZER(struct cmd_clear_input_set_result,
		clear, "clear");
static cmdline_parse_token_string_t cmd_clear_input_set_all =
	TOKEN_STRING_INITIALIZER(struct cmd_clear_input_set_result,
		all, "all");

static cmdline_parse_inst_t cmd_clear_input_set = {
	.f = cmd_clear_input_set_parsed,
	.data = NULL,
	.help_str = "port config <port_id> pctype <pctype_id> hash_inset|"
		"fdir_inset|fdir_flx_inset clear all",
	.tokens = {
		(void *)&cmd_clear_input_set_port,
		(void *)&cmd_clear_input_set_cfg,
		(void *)&cmd_clear_input_set_port_id,
		(void *)&cmd_clear_input_set_pctype,
		(void *)&cmd_clear_input_set_pctype_id,
		(void *)&cmd_clear_input_set_inset_type,
		(void *)&cmd_clear_input_set_clear,
		(void *)&cmd_clear_input_set_all,
		NULL,
	},
};

/* port config pctype mapping reset */

/* Common result structure for port config pctype mapping reset */
struct cmd_pctype_mapping_reset_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t config;
	portid_t port_id;
	cmdline_fixed_string_t pctype;
	cmdline_fixed_string_t mapping;
	cmdline_fixed_string_t reset;
};

/* Common CLI fields for port config pctype mapping reset*/
static cmdline_parse_token_string_t cmd_pctype_mapping_reset_port =
	TOKEN_STRING_INITIALIZER(struct cmd_pctype_mapping_reset_result,
		port, "port");
static cmdline_parse_token_string_t cmd_pctype_mapping_reset_config =
	TOKEN_STRING_INITIALIZER(struct cmd_pctype_mapping_reset_result,
		config, "config");
static cmdline_parse_token_num_t cmd_pctype_mapping_reset_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_pctype_mapping_reset_result,
		port_id, RTE_UINT16);
static cmdline_parse_token_string_t cmd_pctype_mapping_reset_pctype =
	TOKEN_STRING_INITIALIZER(struct cmd_pctype_mapping_reset_result,
		pctype, "pctype");
static cmdline_parse_token_string_t cmd_pctype_mapping_reset_mapping =
	TOKEN_STRING_INITIALIZER(struct cmd_pctype_mapping_reset_result,
		mapping, "mapping");
static cmdline_parse_token_string_t cmd_pctype_mapping_reset_reset =
	TOKEN_STRING_INITIALIZER(struct cmd_pctype_mapping_reset_result,
		reset, "reset");

static void
cmd_pctype_mapping_reset_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_pctype_mapping_reset_result *res = parsed_result;
	int ret = -ENOTSUP;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	ret = rte_pmd_i40e_flow_type_mapping_reset(res->port_id);
	switch (ret) {
	case 0:
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

static cmdline_parse_inst_t cmd_pctype_mapping_reset = {
	.f = cmd_pctype_mapping_reset_parsed,
	.data = NULL,
	.help_str = "port config <port_id> pctype mapping reset",
	.tokens = {
		(void *)&cmd_pctype_mapping_reset_port,
		(void *)&cmd_pctype_mapping_reset_config,
		(void *)&cmd_pctype_mapping_reset_port_id,
		(void *)&cmd_pctype_mapping_reset_pctype,
		(void *)&cmd_pctype_mapping_reset_mapping,
		(void *)&cmd_pctype_mapping_reset_reset,
		NULL,
	},
};

/* show port pctype mapping */

/* Common result structure for show port pctype mapping */
struct cmd_pctype_mapping_get_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t pctype;
	cmdline_fixed_string_t mapping;
};

/* Common CLI fields for pctype mapping get */
static cmdline_parse_token_string_t cmd_pctype_mapping_get_show =
	TOKEN_STRING_INITIALIZER(struct cmd_pctype_mapping_get_result,
		show, "show");
static cmdline_parse_token_string_t cmd_pctype_mapping_get_port =
	TOKEN_STRING_INITIALIZER(struct cmd_pctype_mapping_get_result,
		port, "port");
static cmdline_parse_token_num_t cmd_pctype_mapping_get_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_pctype_mapping_get_result,
		port_id, RTE_UINT16);
static cmdline_parse_token_string_t cmd_pctype_mapping_get_pctype =
	TOKEN_STRING_INITIALIZER(struct cmd_pctype_mapping_get_result,
		pctype, "pctype");
static cmdline_parse_token_string_t cmd_pctype_mapping_get_mapping =
	TOKEN_STRING_INITIALIZER(struct cmd_pctype_mapping_get_result,
		mapping, "mapping");

static void
cmd_pctype_mapping_get_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_pctype_mapping_get_result *res = parsed_result;
	int ret = -ENOTSUP;
	struct rte_pmd_i40e_flow_type_mapping
				mapping[RTE_PMD_I40E_FLOW_TYPE_MAX];
	int i, j, first_pctype;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	ret = rte_pmd_i40e_flow_type_mapping_get(res->port_id, mapping);
	switch (ret) {
	case 0:
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		return;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		return;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
		return;
	}

	for (i = 0; i < RTE_PMD_I40E_FLOW_TYPE_MAX; i++) {
		if (mapping[i].pctype != 0ULL) {
			first_pctype = 1;

			printf("pctype: ");
			for (j = 0; j < RTE_PMD_I40E_PCTYPE_MAX; j++) {
				if (mapping[i].pctype & (1ULL << j)) {
					printf(first_pctype ?  "%02d" : ",%02d", j);
					first_pctype = 0;
				}
			}
			printf("  ->  flowtype: %02d\n", mapping[i].flow_type);
		}
	}
}

static cmdline_parse_inst_t cmd_pctype_mapping_get = {
	.f = cmd_pctype_mapping_get_parsed,
	.data = NULL,
	.help_str = "show port <port_id> pctype mapping",
	.tokens = {
		(void *)&cmd_pctype_mapping_get_show,
		(void *)&cmd_pctype_mapping_get_port,
		(void *)&cmd_pctype_mapping_get_port_id,
		(void *)&cmd_pctype_mapping_get_pctype,
		(void *)&cmd_pctype_mapping_get_mapping,
		NULL,
	},
};

/* port config pctype mapping update */

/* Common result structure for port config pctype mapping update */
struct cmd_pctype_mapping_update_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t config;
	portid_t port_id;
	cmdline_fixed_string_t pctype;
	cmdline_fixed_string_t mapping;
	cmdline_fixed_string_t update;
	cmdline_fixed_string_t pctype_list;
	uint16_t flow_type;
};

/* Common CLI fields for pctype mapping update*/
static cmdline_parse_token_string_t cmd_pctype_mapping_update_port =
	TOKEN_STRING_INITIALIZER(struct cmd_pctype_mapping_update_result,
		port, "port");
static cmdline_parse_token_string_t cmd_pctype_mapping_update_config =
	TOKEN_STRING_INITIALIZER(struct cmd_pctype_mapping_update_result,
		config, "config");
static cmdline_parse_token_num_t cmd_pctype_mapping_update_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_pctype_mapping_update_result,
		port_id, RTE_UINT16);
static cmdline_parse_token_string_t cmd_pctype_mapping_update_pctype =
	TOKEN_STRING_INITIALIZER(struct cmd_pctype_mapping_update_result,
		pctype, "pctype");
static cmdline_parse_token_string_t cmd_pctype_mapping_update_mapping =
	TOKEN_STRING_INITIALIZER(struct cmd_pctype_mapping_update_result,
		mapping, "mapping");
static cmdline_parse_token_string_t cmd_pctype_mapping_update_update =
	TOKEN_STRING_INITIALIZER(struct cmd_pctype_mapping_update_result,
		update, "update");
static cmdline_parse_token_string_t cmd_pctype_mapping_update_pc_type =
	TOKEN_STRING_INITIALIZER(struct cmd_pctype_mapping_update_result,
		pctype_list, NULL);
static cmdline_parse_token_num_t cmd_pctype_mapping_update_flow_type =
	TOKEN_NUM_INITIALIZER(struct cmd_pctype_mapping_update_result,
		flow_type, RTE_UINT16);

static void
cmd_pctype_mapping_update_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_pctype_mapping_update_result *res = parsed_result;
	int ret = -ENOTSUP;
	struct rte_pmd_i40e_flow_type_mapping mapping;
	unsigned int i;
	unsigned int nb_item;
	unsigned int pctype_list[RTE_PMD_I40E_PCTYPE_MAX];

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	nb_item = parse_item_list(res->pctype_list, "pctypes", RTE_PMD_I40E_PCTYPE_MAX,
		pctype_list, 1);
	mapping.flow_type = res->flow_type;
	for (i = 0, mapping.pctype = 0ULL; i < nb_item; i++)
		mapping.pctype |= (1ULL << pctype_list[i]);
	ret = rte_pmd_i40e_flow_type_mapping_update(res->port_id,
						&mapping,
						1,
						0);
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid pctype or flow type\n");
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

static cmdline_parse_inst_t cmd_pctype_mapping_update = {
	.f = cmd_pctype_mapping_update_parsed,
	.data = NULL,
	.help_str = "port config <port_id> pctype mapping update"
	" <pctype_id_0,[pctype_id_1]*> <flowtype_id>",
	.tokens = {
		(void *)&cmd_pctype_mapping_update_port,
		(void *)&cmd_pctype_mapping_update_config,
		(void *)&cmd_pctype_mapping_update_port_id,
		(void *)&cmd_pctype_mapping_update_pctype,
		(void *)&cmd_pctype_mapping_update_mapping,
		(void *)&cmd_pctype_mapping_update_update,
		(void *)&cmd_pctype_mapping_update_pc_type,
		(void *)&cmd_pctype_mapping_update_flow_type,
		NULL,
	},
};

/* ptype mapping get */

/* Common result structure for ptype mapping get */
struct cmd_ptype_mapping_get_result {
	cmdline_fixed_string_t ptype;
	cmdline_fixed_string_t mapping;
	cmdline_fixed_string_t get;
	portid_t port_id;
	uint8_t valid_only;
};

/* Common CLI fields for ptype mapping get */
static cmdline_parse_token_string_t cmd_ptype_mapping_get_ptype =
	TOKEN_STRING_INITIALIZER(struct cmd_ptype_mapping_get_result,
		ptype, "ptype");
static cmdline_parse_token_string_t cmd_ptype_mapping_get_mapping =
	TOKEN_STRING_INITIALIZER(struct cmd_ptype_mapping_get_result,
		mapping, "mapping");
static cmdline_parse_token_string_t cmd_ptype_mapping_get_get =
	TOKEN_STRING_INITIALIZER(struct cmd_ptype_mapping_get_result,
		get, "get");
static cmdline_parse_token_num_t cmd_ptype_mapping_get_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_ptype_mapping_get_result,
		port_id, RTE_UINT16);
static cmdline_parse_token_num_t cmd_ptype_mapping_get_valid_only =
	TOKEN_NUM_INITIALIZER(struct cmd_ptype_mapping_get_result,
		valid_only, RTE_UINT8);

static void
cmd_ptype_mapping_get_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_ptype_mapping_get_result *res = parsed_result;
	int ret = -ENOTSUP;
	int max_ptype_num = 256;
	struct rte_pmd_i40e_ptype_mapping mapping[max_ptype_num];
	uint16_t count;
	int i;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	ret = rte_pmd_i40e_ptype_mapping_get(res->port_id,
					mapping,
					max_ptype_num,
					&count,
					res->valid_only);
	switch (ret) {
	case 0:
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}

	if (!ret) {
		for (i = 0; i < count; i++)
			printf("%3d\t0x%08x\n",
				mapping[i].hw_ptype, mapping[i].sw_ptype);
	}
}

static cmdline_parse_inst_t cmd_ptype_mapping_get = {
	.f = cmd_ptype_mapping_get_parsed,
	.data = NULL,
	.help_str = "ptype mapping get <port_id> <valid_only>",
	.tokens = {
		(void *)&cmd_ptype_mapping_get_ptype,
		(void *)&cmd_ptype_mapping_get_mapping,
		(void *)&cmd_ptype_mapping_get_get,
		(void *)&cmd_ptype_mapping_get_port_id,
		(void *)&cmd_ptype_mapping_get_valid_only,
		NULL,
	},
};

/* ptype mapping replace */

/* Common result structure for ptype mapping replace */
struct cmd_ptype_mapping_replace_result {
	cmdline_fixed_string_t ptype;
	cmdline_fixed_string_t mapping;
	cmdline_fixed_string_t replace;
	portid_t port_id;
	uint32_t target;
	uint8_t mask;
	uint32_t pkt_type;
};

/* Common CLI fields for ptype mapping replace */
static cmdline_parse_token_string_t cmd_ptype_mapping_replace_ptype =
	TOKEN_STRING_INITIALIZER(struct cmd_ptype_mapping_replace_result,
		ptype, "ptype");
static cmdline_parse_token_string_t cmd_ptype_mapping_replace_mapping =
	TOKEN_STRING_INITIALIZER(struct cmd_ptype_mapping_replace_result,
		mapping, "mapping");
static cmdline_parse_token_string_t cmd_ptype_mapping_replace_replace =
	TOKEN_STRING_INITIALIZER(struct cmd_ptype_mapping_replace_result,
		replace, "replace");
static cmdline_parse_token_num_t cmd_ptype_mapping_replace_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_ptype_mapping_replace_result,
		port_id, RTE_UINT16);
static cmdline_parse_token_num_t cmd_ptype_mapping_replace_target =
	TOKEN_NUM_INITIALIZER(struct cmd_ptype_mapping_replace_result,
		target, RTE_UINT32);
static cmdline_parse_token_num_t cmd_ptype_mapping_replace_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_ptype_mapping_replace_result,
		mask, RTE_UINT8);
static cmdline_parse_token_num_t cmd_ptype_mapping_replace_pkt_type =
	TOKEN_NUM_INITIALIZER(struct cmd_ptype_mapping_replace_result,
		pkt_type, RTE_UINT32);

static void
cmd_ptype_mapping_replace_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_ptype_mapping_replace_result *res = parsed_result;
	int ret = -ENOTSUP;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	ret = rte_pmd_i40e_ptype_mapping_replace(res->port_id,
					res->target,
					res->mask,
					res->pkt_type);
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid ptype 0x%8x or 0x%8x\n",
			res->target, res->pkt_type);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

static cmdline_parse_inst_t cmd_ptype_mapping_replace = {
	.f = cmd_ptype_mapping_replace_parsed,
	.data = NULL,
	.help_str =
		"ptype mapping replace <port_id> <target> <mask> <pkt_type>",
	.tokens = {
		(void *)&cmd_ptype_mapping_replace_ptype,
		(void *)&cmd_ptype_mapping_replace_mapping,
		(void *)&cmd_ptype_mapping_replace_replace,
		(void *)&cmd_ptype_mapping_replace_port_id,
		(void *)&cmd_ptype_mapping_replace_target,
		(void *)&cmd_ptype_mapping_replace_mask,
		(void *)&cmd_ptype_mapping_replace_pkt_type,
		NULL,
	},
};

/* ptype mapping reset */

/* Common result structure for ptype mapping reset */
struct cmd_ptype_mapping_reset_result {
	cmdline_fixed_string_t ptype;
	cmdline_fixed_string_t mapping;
	cmdline_fixed_string_t reset;
	portid_t port_id;
};

/* Common CLI fields for ptype mapping reset*/
static cmdline_parse_token_string_t cmd_ptype_mapping_reset_ptype =
	TOKEN_STRING_INITIALIZER(struct cmd_ptype_mapping_reset_result,
		ptype, "ptype");
static cmdline_parse_token_string_t cmd_ptype_mapping_reset_mapping =
	TOKEN_STRING_INITIALIZER(struct cmd_ptype_mapping_reset_result,
		mapping, "mapping");
static cmdline_parse_token_string_t cmd_ptype_mapping_reset_reset =
	TOKEN_STRING_INITIALIZER(struct cmd_ptype_mapping_reset_result,
		reset, "reset");
static cmdline_parse_token_num_t cmd_ptype_mapping_reset_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_ptype_mapping_reset_result,
		port_id, RTE_UINT16);

static void
cmd_ptype_mapping_reset_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_ptype_mapping_reset_result *res = parsed_result;
	int ret = -ENOTSUP;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	ret = rte_pmd_i40e_ptype_mapping_reset(res->port_id);
	switch (ret) {
	case 0:
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

static cmdline_parse_inst_t cmd_ptype_mapping_reset = {
	.f = cmd_ptype_mapping_reset_parsed,
	.data = NULL,
	.help_str = "ptype mapping reset <port_id>",
	.tokens = {
		(void *)&cmd_ptype_mapping_reset_ptype,
		(void *)&cmd_ptype_mapping_reset_mapping,
		(void *)&cmd_ptype_mapping_reset_reset,
		(void *)&cmd_ptype_mapping_reset_port_id,
		NULL,
	},
};

/* ptype mapping update */

/* Common result structure for ptype mapping update */
struct cmd_ptype_mapping_update_result {
	cmdline_fixed_string_t ptype;
	cmdline_fixed_string_t mapping;
	cmdline_fixed_string_t reset;
	portid_t port_id;
	uint8_t hw_ptype;
	uint32_t sw_ptype;
};

/* Common CLI fields for ptype mapping update*/
static cmdline_parse_token_string_t cmd_ptype_mapping_update_ptype =
	TOKEN_STRING_INITIALIZER(struct cmd_ptype_mapping_update_result,
		ptype, "ptype");
static cmdline_parse_token_string_t cmd_ptype_mapping_update_mapping =
	TOKEN_STRING_INITIALIZER(struct cmd_ptype_mapping_update_result,
		mapping, "mapping");
static cmdline_parse_token_string_t cmd_ptype_mapping_update_update =
	TOKEN_STRING_INITIALIZER(struct cmd_ptype_mapping_update_result,
		reset, "update");
static cmdline_parse_token_num_t cmd_ptype_mapping_update_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_ptype_mapping_update_result,
		port_id, RTE_UINT16);
static cmdline_parse_token_num_t cmd_ptype_mapping_update_hw_ptype =
	TOKEN_NUM_INITIALIZER(struct cmd_ptype_mapping_update_result,
		hw_ptype, RTE_UINT8);
static cmdline_parse_token_num_t cmd_ptype_mapping_update_sw_ptype =
	TOKEN_NUM_INITIALIZER(struct cmd_ptype_mapping_update_result,
		sw_ptype, RTE_UINT32);

static void
cmd_ptype_mapping_update_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_ptype_mapping_update_result *res = parsed_result;
	int ret = -ENOTSUP;
	struct rte_pmd_i40e_ptype_mapping mapping;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	mapping.hw_ptype = res->hw_ptype;
	mapping.sw_ptype = res->sw_ptype;
	ret = rte_pmd_i40e_ptype_mapping_update(res->port_id,
						&mapping,
						1,
						0);
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid ptype 0x%8x\n", res->sw_ptype);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

static cmdline_parse_inst_t cmd_ptype_mapping_update = {
	.f = cmd_ptype_mapping_update_parsed,
	.data = NULL,
	.help_str = "ptype mapping update <port_id> <hw_ptype> <sw_ptype>",
	.tokens = {
		(void *)&cmd_ptype_mapping_update_ptype,
		(void *)&cmd_ptype_mapping_update_mapping,
		(void *)&cmd_ptype_mapping_update_update,
		(void *)&cmd_ptype_mapping_update_port_id,
		(void *)&cmd_ptype_mapping_update_hw_ptype,
		(void *)&cmd_ptype_mapping_update_sw_ptype,
		NULL,
	},
};

/* *** configure source prune for port *** */
struct cmd_config_src_prune_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	cmdline_fixed_string_t port_all; /* valid if "allports" argument == 1 */
	uint16_t port_id;                /* valid if "allports" argument == 0 */
	cmdline_fixed_string_t item;
	cmdline_fixed_string_t enable;
};

static void cmd_config_pf_src_prune_parsed(void *parsed_result,
					__rte_unused struct cmdline *cl,
					void *allports)
{
	struct cmd_config_src_prune_result *res = parsed_result;
	uint8_t enable;
	uint16_t i;

	if (!strcmp(res->enable, "on"))
		enable = 1;
	else
		enable = 0;

	/* all ports */
	if (allports) {
		RTE_ETH_FOREACH_DEV(i)
			rte_pmd_i40e_set_pf_src_prune(i, enable);
	} else {
		rte_pmd_i40e_set_pf_src_prune(res->port_id, enable);
	}
}

static cmdline_parse_token_string_t cmd_config_src_prune_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_src_prune_result, port, "port");
static cmdline_parse_token_string_t cmd_config_src_prune_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_config_src_prune_result, keyword,
				 "config");
static cmdline_parse_token_string_t cmd_config_src_prune_portall =
	TOKEN_STRING_INITIALIZER(struct cmd_config_src_prune_result, port_all,
				 "all");
static cmdline_parse_token_num_t cmd_config_src_prune_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_config_src_prune_result, port_id,
			      RTE_UINT16);
static cmdline_parse_token_string_t cmd_config_src_prune_item =
	TOKEN_STRING_INITIALIZER(struct cmd_config_src_prune_result,
			item, "i40e_src_prune");
static cmdline_parse_token_string_t cmd_config_src_prune_enable =
	TOKEN_STRING_INITIALIZER(struct cmd_config_src_prune_result, enable,
				 "on#off");

static cmdline_parse_inst_t cmd_config_src_prune_all = {
	.f = cmd_config_pf_src_prune_parsed,
	.data = (void *)1,
	.help_str = "port config all i40e_src_prune on|off: Set source pruning on all pf ports.",
	.tokens = {
		(void *)&cmd_config_src_prune_port,
		(void *)&cmd_config_src_prune_keyword,
		(void *)&cmd_config_src_prune_portall,
		(void *)&cmd_config_src_prune_item,
		(void *)&cmd_config_src_prune_enable,
		NULL,
	},
};

static cmdline_parse_inst_t cmd_config_src_prune_specific = {
	.f = cmd_config_pf_src_prune_parsed,
	.data = (void *)0,
	.help_str = "port config <port_id> i40e_src_prune on|off: Set source pruning on specific pf port.",
	.tokens = {
		(void *)&cmd_config_src_prune_port,
		(void *)&cmd_config_src_prune_keyword,
		(void *)&cmd_config_src_prune_port_id,
		(void *)&cmd_config_src_prune_item,
		(void *)&cmd_config_src_prune_enable,
		NULL,
	},
};

static struct testpmd_driver_commands i40e_cmds = {
	.commands = {
	{
		&cmd_queue_region,
		"set port (port_id) queue-region region_id (value) "
		"queue_start_index (value) queue_num (value)\n"
		"    Set a queue region on a port\n",
	},
	{
		&cmd_region_flowtype,
		"set port (port_id) queue-region region_id (value) "
		"flowtype (value)\n"
		"    Set a flowtype region index on a port\n",
	},
	{
		&cmd_user_priority_region,
		"set port (port_id) queue-region UP (value) region_id (value)\n"
		"    Set the mapping of User Priority to "
		"queue region on a port\n",
	},
	{
		&cmd_flush_queue_region,
		"set port (port_id) queue-region flush (on|off)\n"
		"    flush all queue region related configuration\n",
	},
	{
		&cmd_show_queue_region_info_all,
		"show port (port_id) queue-region\n"
		"    show all queue region related configuration info\n",
	},
	{
		&cmd_add_del_raw_flow_director,
		"flow_director_filter (port_id) mode raw (add|del|update)"
		" flow (flow_id) (drop|fwd) queue (queue_id)"
		" fd_id (fd_id_value) packet (packet file name)\n"
		"    Add/Del a raw type flow director filter.\n",
	},
	{
		&cmd_set_vf_promisc,
		"set vf promisc (port_id) (vf_id) (on|off)\n"
		"    Set unicast promiscuous mode for a VF from the PF.\n",
	},
	{
		&cmd_set_vf_allmulti,
		"set vf allmulti (port_id) (vf_id) (on|off)\n"
		"    Set multicast promiscuous mode for a VF from the PF.\n",
	},
	{
		&cmd_set_vf_broadcast,
		"set vf broadcast (port_id) (vf_id) (on|off)\n"
		"    Set VF broadcast for a VF from the PF.\n",
	},
	{
		&cmd_set_vf_vlan_tag,
		"set vf vlan tag (port_id) (vf_id) (on|off)\n"
		"    Set VLAN tag for a VF from the PF.\n",
	},
	{
		&cmd_vf_max_bw,
		"set vf tx max-bandwidth (port_id) (vf_id) (bandwidth)\n"
		"    Set a VF's max bandwidth(Mbps).\n",
	},
	{
		&cmd_vf_tc_min_bw,
		"set vf tc tx min-bandwidth (port_id) (vf_id) (bw1, bw2, ...)\n"
		"    Set all TCs' min bandwidth(%%) on a VF.\n",
	},
	{
		&cmd_vf_tc_max_bw,
		"set vf tc tx max-bandwidth (port_id) (vf_id) (tc_no) (bandwidth)\n"
		"    Set a TC's max bandwidth(Mbps) on a VF.\n",
	},
	{
		&cmd_strict_link_prio,
		"set tx strict-link-priority (port_id) (tc_bitmap)\n"
		"    Set some TCs' strict link priority mode on a physical port.\n",
	},
	{
		&cmd_ddp_add,
		"ddp add (port_id) (profile_path[,backup_profile_path])\n"
		"    Load a profile package on a port\n",
	},
	{
		&cmd_ddp_del,
		"ddp del (port_id) (backup_profile_path)\n"
		"    Delete a profile package from a port\n",
	},
	{
		&cmd_ddp_get_list,
		"ddp get list (port_id)\n"
		"    Get ddp profile info list\n",
	},
	{
		&cmd_ddp_get_info,
		"ddp get info (profile_path)\n"
		"    Get ddp profile information.\n",
	},
	{
		&cmd_cfg_input_set,
		"port config (port_id) pctype (pctype_id) hash_inset|"
		"fdir_inset|fdir_flx_inset get|set|clear field\n"
		" (field_idx)\n"
		"    Configure RSS|FDIR|FDIR_FLX input set for some pctype\n",
	},
	{
		&cmd_clear_input_set,
		"port config (port_id) pctype (pctype_id) hash_inset|"
		"fdir_inset|fdir_flx_inset clear all\n"
		"    Clear RSS|FDIR|FDIR_FLX input set completely for some pctype\n",
	},
	{
		&cmd_ptype_mapping_get,
		"ptype mapping get (port_id) (valid_only)\n"
		"    Get ptype mapping on a port\n",
	},
	{
		&cmd_ptype_mapping_replace,
		"ptype mapping replace (port_id) (target) (mask) (pky_type)\n"
		"    Replace target with the pkt_type in ptype mapping\n",
	},
	{
		&cmd_ptype_mapping_reset,
		"ptype mapping reset (port_id)\n"
		"    Reset ptype mapping on a port\n",
	},
	{
		&cmd_ptype_mapping_update,
		"ptype mapping update (port_id) (hw_ptype) (sw_ptype)\n"
		"    Update a ptype mapping item on a port\n",
	},
	{
		&cmd_pctype_mapping_get,
		"show port (port_id) pctype mapping\n"
		"    Get flow ptype to pctype mapping on a port\n",
	},
	{
		&cmd_pctype_mapping_reset,
		"port config (port_id) pctype mapping reset\n"
		"    Reset flow type to pctype mapping on a port\n",
	},
	{
		&cmd_pctype_mapping_update,
		"port config (port_id) pctype mapping update"
		" (pctype_id_0[,pctype_id_1]*) (flow_type_id)\n"
		"    Update a flow type to pctype mapping item on a port\n",
	},
	{
		&cmd_config_src_prune_all,
		"port config all i40e_src_prune (on|off)\n"
		"    Set source pruning on pf port all.\n"
	},
	{
		&cmd_config_src_prune_specific,
		"port config (port_id) i40e_src_prune (on|off)\n"
		"    Set source pruning on pf port_id.\n"
	},
	{ NULL, NULL },
	},
};
TESTPMD_ADD_DRIVER_COMMANDS(i40e_cmds)
