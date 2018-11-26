/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>

#include <rte_ethdev.h>
#include <rte_flow.h>
#include <rte_tm.h>

#include "testpmd.h"
#include "cmdline_tm.h"

#define PARSE_DELIMITER				" \f\n\r\t\v"
#define MAX_NUM_SHARED_SHAPERS		256

#define skip_white_spaces(pos)			\
({						\
	__typeof__(pos) _p = (pos);		\
	for ( ; isspace(*_p); _p++)		\
		;				\
	_p;					\
})

/** Display TM Error Message */
static void
print_err_msg(struct rte_tm_error *error)
{
	static const char *const errstrlist[] = {
		[RTE_TM_ERROR_TYPE_NONE] = "no error",
		[RTE_TM_ERROR_TYPE_UNSPECIFIED] = "cause unspecified",
		[RTE_TM_ERROR_TYPE_CAPABILITIES]
			= "capability parameter null",
		[RTE_TM_ERROR_TYPE_LEVEL_ID] = "level id",
		[RTE_TM_ERROR_TYPE_WRED_PROFILE]
			= "wred profile null",
		[RTE_TM_ERROR_TYPE_WRED_PROFILE_GREEN] = "wred profile(green)",
		[RTE_TM_ERROR_TYPE_WRED_PROFILE_YELLOW]
			= "wred profile(yellow)",
		[RTE_TM_ERROR_TYPE_WRED_PROFILE_RED] = "wred profile(red)",
		[RTE_TM_ERROR_TYPE_WRED_PROFILE_ID] = "wred profile id",
		[RTE_TM_ERROR_TYPE_SHARED_WRED_CONTEXT_ID]
			= "shared wred context id",
		[RTE_TM_ERROR_TYPE_SHAPER_PROFILE] = "shaper profile null",
		[RTE_TM_ERROR_TYPE_SHAPER_PROFILE_COMMITTED_RATE]
			= "committed rate field (shaper profile)",
		[RTE_TM_ERROR_TYPE_SHAPER_PROFILE_COMMITTED_SIZE]
			= "committed size field (shaper profile)",
		[RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_RATE]
			= "peak rate field (shaper profile)",
		[RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_SIZE]
			= "peak size field (shaper profile)",
		[RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PKT_ADJUST_LEN]
			= "packet adjust length field (shaper profile)",
		[RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID] = "shaper profile id",
		[RTE_TM_ERROR_TYPE_SHARED_SHAPER_ID] = "shared shaper id",
		[RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID] = "parent node id",
		[RTE_TM_ERROR_TYPE_NODE_PRIORITY] = "node priority",
		[RTE_TM_ERROR_TYPE_NODE_WEIGHT] = "node weight",
		[RTE_TM_ERROR_TYPE_NODE_PARAMS] = "node parameter null",
		[RTE_TM_ERROR_TYPE_NODE_PARAMS_SHAPER_PROFILE_ID]
			= "shaper profile id field (node params)",
		[RTE_TM_ERROR_TYPE_NODE_PARAMS_SHARED_SHAPER_ID]
			= "shared shaper id field (node params)",
		[RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SHARED_SHAPERS]
			= "num shared shapers field (node params)",
		[RTE_TM_ERROR_TYPE_NODE_PARAMS_WFQ_WEIGHT_MODE]
			= "wfq weght mode field (node params)",
		[RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SP_PRIORITIES]
			= "num strict priorities field (node params)",
		[RTE_TM_ERROR_TYPE_NODE_PARAMS_CMAN]
			= "congestion management mode field (node params)",
		[RTE_TM_ERROR_TYPE_NODE_PARAMS_WRED_PROFILE_ID] =
			"wred profile id field (node params)",
		[RTE_TM_ERROR_TYPE_NODE_PARAMS_SHARED_WRED_CONTEXT_ID]
			= "shared wred context id field (node params)",
		[RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SHARED_WRED_CONTEXTS]
			= "num shared wred contexts field (node params)",
		[RTE_TM_ERROR_TYPE_NODE_PARAMS_STATS]
			= "stats field (node params)",
		[RTE_TM_ERROR_TYPE_NODE_ID] = "node id",
	};

	const char *errstr;
	char buf[64];

	if ((unsigned int)error->type >= RTE_DIM(errstrlist) ||
		!errstrlist[error->type])
		errstr = "unknown type";
	else
		errstr = errstrlist[error->type];

	if (error->cause)
		snprintf(buf, sizeof(buf), "cause: %p, ", error->cause);

	printf("%s: %s%s (error %d)\n", errstr, error->cause ? buf : "",
		error->message ? error->message : "(no stated reason)",
		error->type);
}

static int
read_uint64(uint64_t *value, const char *p)
{
	char *next;
	uint64_t val;

	p = skip_white_spaces(p);
	if (!isdigit(*p))
		return -EINVAL;

	val = strtoul(p, &next, 10);
	if (p == next)
		return -EINVAL;

	p = next;
	switch (*p) {
	case 'T':
		val *= 1024ULL;
		/* fall through */
	case 'G':
		val *= 1024ULL;
		/* fall through */
	case 'M':
		val *= 1024ULL;
		/* fall through */
	case 'k':
	case 'K':
		val *= 1024ULL;
		p++;
		break;
	}

	p = skip_white_spaces(p);
	if (*p != '\0')
		return -EINVAL;

	*value = val;
	return 0;
}

static int
read_uint32(uint32_t *value, const char *p)
{
	uint64_t val = 0;
	int ret = read_uint64(&val, p);

	if (ret < 0)
		return ret;

	if (val > UINT32_MAX)
		return -ERANGE;

	*value = val;
	return 0;
}

static int
parse_multi_ss_id_str(char *s_str, uint32_t *n_ssp, uint32_t shaper_id[])
{
	uint32_t n_shared_shapers = 0, i = 0;
	char *token;

	/* First token: num of shared shapers */
	token = strtok_r(s_str, PARSE_DELIMITER, &s_str);
	if (token ==  NULL)
		return -1;

	if (read_uint32(&n_shared_shapers, token))
		return -1;

	/* Check: num of shared shaper */
	if (n_shared_shapers >= MAX_NUM_SHARED_SHAPERS) {
		printf(" Number of shared shapers exceed the max (error)\n");
		return -1;
	}

	/* Parse shared shaper ids */
	while (1) {
		token = strtok_r(s_str, PARSE_DELIMITER, &s_str);
		if ((token !=  NULL && n_shared_shapers == 0) ||
			(token == NULL && i < n_shared_shapers))
			return -1;

		if (token == NULL)
			break;

		if (read_uint32(&shaper_id[i], token))
			return -1;
		i++;
	}
	*n_ssp = n_shared_shapers;

	return 0;
}
/* *** Port TM Capability *** */
struct cmd_show_port_tm_cap_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t tm;
	cmdline_fixed_string_t cap;
	uint16_t port_id;
};

cmdline_parse_token_string_t cmd_show_port_tm_cap_show =
	TOKEN_STRING_INITIALIZER(struct cmd_show_port_tm_cap_result,
		show, "show");
cmdline_parse_token_string_t cmd_show_port_tm_cap_port =
	TOKEN_STRING_INITIALIZER(struct cmd_show_port_tm_cap_result,
		port, "port");
cmdline_parse_token_string_t cmd_show_port_tm_cap_tm =
	TOKEN_STRING_INITIALIZER(struct cmd_show_port_tm_cap_result,
		tm, "tm");
cmdline_parse_token_string_t cmd_show_port_tm_cap_cap =
	TOKEN_STRING_INITIALIZER(struct cmd_show_port_tm_cap_result,
		cap, "cap");
cmdline_parse_token_num_t cmd_show_port_tm_cap_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_show_port_tm_cap_result,
		 port_id, UINT16);

static void cmd_show_port_tm_cap_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_show_port_tm_cap_result *res = parsed_result;
	struct rte_tm_capabilities cap;
	struct rte_tm_error error;
	portid_t port_id = res->port_id;
	uint32_t i;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&cap, 0, sizeof(struct rte_tm_capabilities));
	memset(&error, 0, sizeof(struct rte_tm_error));
	ret = rte_tm_capabilities_get(port_id, &cap, &error);
	if (ret) {
		print_err_msg(&error);
		return;
	}

	printf("\n****   Port TM Capabilities ****\n\n");
	printf("cap.n_nodes_max %" PRIu32 "\n", cap.n_nodes_max);
	printf("cap.n_levels_max %" PRIu32 "\n", cap.n_levels_max);
	printf("cap.non_leaf_nodes_identical %" PRId32 "\n",
		cap.non_leaf_nodes_identical);
	printf("cap.leaf_nodes_identical %" PRId32 "\n",
		cap.leaf_nodes_identical);
	printf("cap.shaper_n_max %u\n", cap.shaper_n_max);
	printf("cap.shaper_private_n_max %" PRIu32 "\n",
		cap.shaper_private_n_max);
	printf("cap.shaper_private_dual_rate_n_max %" PRId32 "\n",
		cap.shaper_private_dual_rate_n_max);
	printf("cap.shaper_private_rate_min %" PRIu64 "\n",
		cap.shaper_private_rate_min);
	printf("cap.shaper_private_rate_max %" PRIu64 "\n",
		cap.shaper_private_rate_max);
	printf("cap.shaper_shared_n_max %" PRIu32 "\n",
		cap.shaper_shared_n_max);
	printf("cap.shaper_shared_n_nodes_per_shaper_max %" PRIu32 "\n",
		cap.shaper_shared_n_nodes_per_shaper_max);
	printf("cap.shaper_shared_n_shapers_per_node_max %" PRIu32 "\n",
		cap.shaper_shared_n_shapers_per_node_max);
	printf("cap.shaper_shared_dual_rate_n_max %" PRIu32 "\n",
		cap.shaper_shared_dual_rate_n_max);
	printf("cap.shaper_shared_rate_min %" PRIu64 "\n",
		cap.shaper_shared_rate_min);
	printf("cap.shaper_shared_rate_max %" PRIu64 "\n",
		cap.shaper_shared_rate_max);
	printf("cap.shaper_pkt_length_adjust_min %" PRId32 "\n",
		cap.shaper_pkt_length_adjust_min);
	printf("cap.shaper_pkt_length_adjust_max %" PRId32 "\n",
		cap.shaper_pkt_length_adjust_max);
	printf("cap.sched_n_children_max %" PRIu32 "\n",
		cap.sched_n_children_max);
	printf("cap.sched_sp_n_priorities_max %" PRIu32 "\n",
		cap.sched_sp_n_priorities_max);
	printf("cap.sched_wfq_n_children_per_group_max %" PRIu32 "\n",
		cap.sched_wfq_n_children_per_group_max);
	printf("cap.sched_wfq_n_groups_max %" PRIu32 "\n",
		cap.sched_wfq_n_groups_max);
	printf("cap.sched_wfq_weight_max %" PRIu32 "\n",
		cap.sched_wfq_weight_max);
	printf("cap.cman_head_drop_supported %" PRId32 "\n",
		cap.cman_head_drop_supported);
	printf("cap.cman_wred_context_n_max %" PRIu32 "\n",
		cap.cman_wred_context_n_max);
	printf("cap.cman_wred_context_private_n_max %" PRIu32 "\n",
		cap.cman_wred_context_private_n_max);
	printf("cap.cman_wred_context_shared_n_max %" PRIu32 "\n",
		cap.cman_wred_context_shared_n_max);
	printf("cap.cman_wred_context_shared_n_nodes_per_context_max %" PRIu32
		"\n", cap.cman_wred_context_shared_n_nodes_per_context_max);
	printf("cap.cman_wred_context_shared_n_contexts_per_node_max %" PRIu32
		"\n", cap.cman_wred_context_shared_n_contexts_per_node_max);

	for (i = 0; i < RTE_TM_COLORS; i++) {
		printf("cap.mark_vlan_dei_supported %" PRId32 "\n",
			cap.mark_vlan_dei_supported[i]);
		printf("cap.mark_ip_ecn_tcp_supported %" PRId32 "\n",
			cap.mark_ip_ecn_tcp_supported[i]);
		printf("cap.mark_ip_ecn_sctp_supported %" PRId32 "\n",
			cap.mark_ip_ecn_sctp_supported[i]);
		printf("cap.mark_ip_dscp_supported %" PRId32 "\n",
			cap.mark_ip_dscp_supported[i]);
	}

	printf("cap.dynamic_update_mask %" PRIx64 "\n",
		cap.dynamic_update_mask);
	printf("cap.stats_mask %" PRIx64 "\n", cap.stats_mask);
}

cmdline_parse_inst_t cmd_show_port_tm_cap = {
	.f = cmd_show_port_tm_cap_parsed,
	.data = NULL,
	.help_str = "Show Port TM Capabilities",
	.tokens = {
		(void *)&cmd_show_port_tm_cap_show,
		(void *)&cmd_show_port_tm_cap_port,
		(void *)&cmd_show_port_tm_cap_tm,
		(void *)&cmd_show_port_tm_cap_cap,
		(void *)&cmd_show_port_tm_cap_port_id,
		NULL,
	},
};

/* *** Port TM Hierarchical Level Capability *** */
struct cmd_show_port_tm_level_cap_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t tm;
	cmdline_fixed_string_t level;
	cmdline_fixed_string_t cap;
	uint16_t port_id;
	uint32_t level_id;
};

cmdline_parse_token_string_t cmd_show_port_tm_level_cap_show =
	TOKEN_STRING_INITIALIZER(struct cmd_show_port_tm_level_cap_result,
		show, "show");
cmdline_parse_token_string_t cmd_show_port_tm_level_cap_port =
	TOKEN_STRING_INITIALIZER(struct cmd_show_port_tm_level_cap_result,
		port, "port");
cmdline_parse_token_string_t cmd_show_port_tm_level_cap_tm =
	TOKEN_STRING_INITIALIZER(struct cmd_show_port_tm_level_cap_result,
		tm, "tm");
cmdline_parse_token_string_t cmd_show_port_tm_level_cap_level =
	TOKEN_STRING_INITIALIZER(struct cmd_show_port_tm_level_cap_result,
		level, "level");
cmdline_parse_token_string_t cmd_show_port_tm_level_cap_cap =
	TOKEN_STRING_INITIALIZER(struct cmd_show_port_tm_level_cap_result,
		cap, "cap");
cmdline_parse_token_num_t cmd_show_port_tm_level_cap_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_show_port_tm_level_cap_result,
		 port_id, UINT16);
cmdline_parse_token_num_t cmd_show_port_tm_level_cap_level_id =
	TOKEN_NUM_INITIALIZER(struct cmd_show_port_tm_level_cap_result,
		 level_id, UINT32);


static void cmd_show_port_tm_level_cap_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_show_port_tm_level_cap_result *res = parsed_result;
	struct rte_tm_level_capabilities lcap;
	struct rte_tm_error error;
	portid_t port_id = res->port_id;
	uint32_t level_id = res->level_id;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&lcap, 0, sizeof(struct rte_tm_level_capabilities));
	memset(&error, 0, sizeof(struct rte_tm_error));
	ret = rte_tm_level_capabilities_get(port_id, level_id, &lcap, &error);
	if (ret) {
		print_err_msg(&error);
		return;
	}
	printf("\n**   Port TM Hierarchy level %" PRIu32 " Capability **\n\n",
		level_id);

	printf("cap.n_nodes_max %" PRIu32 "\n", lcap.n_nodes_max);
	printf("cap.n_nodes_nonleaf_max %" PRIu32 "\n",
		lcap.n_nodes_nonleaf_max);
	printf("cap.n_nodes_leaf_max %" PRIu32 "\n", lcap.n_nodes_leaf_max);
	printf("cap.non_leaf_nodes_identical %" PRId32 "\n",
		lcap.non_leaf_nodes_identical);
	printf("cap.leaf_nodes_identical %" PRId32 "\n",
		lcap.leaf_nodes_identical);
	if (level_id <= 3) {
		printf("cap.nonleaf.shaper_private_supported %" PRId32 "\n",
			lcap.nonleaf.shaper_private_supported);
		printf("cap.nonleaf.shaper_private_dual_rate_supported %" PRId32
			"\n", lcap.nonleaf.shaper_private_dual_rate_supported);
		printf("cap.nonleaf.shaper_private_rate_min %" PRIu64 "\n",
			lcap.nonleaf.shaper_private_rate_min);
		printf("cap.nonleaf.shaper_private_rate_max %" PRIu64 "\n",
			lcap.nonleaf.shaper_private_rate_max);
		printf("cap.nonleaf.shaper_shared_n_max %" PRIu32 "\n",
			lcap.nonleaf.shaper_shared_n_max);
		printf("cap.nonleaf.sched_n_children_max %" PRIu32 "\n",
			lcap.nonleaf.sched_n_children_max);
		printf("cap.nonleaf.sched_sp_n_priorities_max %" PRIu32 "\n",
			lcap.nonleaf.sched_sp_n_priorities_max);
		printf("cap.nonleaf.sched_wfq_n_children_per_group_max %" PRIu32
			"\n", lcap.nonleaf.sched_wfq_n_children_per_group_max);
		printf("cap.nonleaf.sched_wfq_n_groups_max %" PRIu32 "\n",
			lcap.nonleaf.sched_wfq_n_groups_max);
		printf("cap.nonleaf.sched_wfq_weight_max %" PRIu32 "\n",
			lcap.nonleaf.sched_wfq_weight_max);
		printf("cap.nonleaf.stats_mask %" PRIx64 "\n",
			lcap.nonleaf.stats_mask);
	} else {
		printf("cap.leaf.shaper_private_supported %" PRId32 "\n",
			lcap.leaf.shaper_private_supported);
		printf("cap.leaf.shaper_private_dual_rate_supported %" PRId32
			"\n", lcap.leaf.shaper_private_dual_rate_supported);
		printf("cap.leaf.shaper_private_rate_min %" PRIu64 "\n",
			lcap.leaf.shaper_private_rate_min);
		printf("cap.leaf.shaper_private_rate_max %" PRIu64 "\n",
			lcap.leaf.shaper_private_rate_max);
		printf("cap.leaf.shaper_shared_n_max %" PRIu32 "\n",
			lcap.leaf.shaper_shared_n_max);
		printf("cap.leaf.cman_head_drop_supported %" PRId32 "\n",
			lcap.leaf.cman_head_drop_supported);
		printf("cap.leaf.cman_wred_context_private_supported %"	PRId32
			"\n", lcap.leaf.cman_wred_context_private_supported);
		printf("cap.leaf.cman_wred_context_shared_n_max %" PRIu32 "\n",
			lcap.leaf.cman_wred_context_shared_n_max);
		printf("cap.leaf.stats_mask %" PRIx64 "\n",
			lcap.leaf.stats_mask);
	}
}

cmdline_parse_inst_t cmd_show_port_tm_level_cap = {
	.f = cmd_show_port_tm_level_cap_parsed,
	.data = NULL,
	.help_str = "Show Port TM Hierarhical level Capabilities",
	.tokens = {
		(void *)&cmd_show_port_tm_level_cap_show,
		(void *)&cmd_show_port_tm_level_cap_port,
		(void *)&cmd_show_port_tm_level_cap_tm,
		(void *)&cmd_show_port_tm_level_cap_level,
		(void *)&cmd_show_port_tm_level_cap_cap,
		(void *)&cmd_show_port_tm_level_cap_port_id,
		(void *)&cmd_show_port_tm_level_cap_level_id,
		NULL,
	},
};

/* *** Port TM Hierarchy Node Capability *** */
struct cmd_show_port_tm_node_cap_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t tm;
	cmdline_fixed_string_t node;
	cmdline_fixed_string_t cap;
	uint16_t port_id;
	uint32_t node_id;
};

cmdline_parse_token_string_t cmd_show_port_tm_node_cap_show =
	TOKEN_STRING_INITIALIZER(struct cmd_show_port_tm_node_cap_result,
		show, "show");
cmdline_parse_token_string_t cmd_show_port_tm_node_cap_port =
	TOKEN_STRING_INITIALIZER(struct cmd_show_port_tm_node_cap_result,
		port, "port");
cmdline_parse_token_string_t cmd_show_port_tm_node_cap_tm =
	TOKEN_STRING_INITIALIZER(struct cmd_show_port_tm_node_cap_result,
		tm, "tm");
cmdline_parse_token_string_t cmd_show_port_tm_node_cap_node =
	TOKEN_STRING_INITIALIZER(struct cmd_show_port_tm_node_cap_result,
		node, "node");
cmdline_parse_token_string_t cmd_show_port_tm_node_cap_cap =
	TOKEN_STRING_INITIALIZER(struct cmd_show_port_tm_node_cap_result,
		cap, "cap");
cmdline_parse_token_num_t cmd_show_port_tm_node_cap_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_show_port_tm_node_cap_result,
		 port_id, UINT16);
cmdline_parse_token_num_t cmd_show_port_tm_node_cap_node_id =
	TOKEN_NUM_INITIALIZER(struct cmd_show_port_tm_node_cap_result,
		 node_id, UINT32);

static void cmd_show_port_tm_node_cap_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_show_port_tm_node_cap_result *res = parsed_result;
	struct rte_tm_node_capabilities ncap;
	struct rte_tm_error error;
	uint32_t node_id = res->node_id;
	portid_t port_id = res->port_id;
	int ret, is_leaf = 0;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&error, 0, sizeof(struct rte_tm_error));
	/* Node id must be valid */
	ret = rte_tm_node_type_get(port_id, node_id, &is_leaf, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}

	memset(&ncap, 0, sizeof(struct rte_tm_node_capabilities));
	ret = rte_tm_node_capabilities_get(port_id, node_id, &ncap, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
	printf("\n**   Port TM Hierarchy node %" PRIu32 " Capability **\n\n",
		node_id);
	printf("cap.shaper_private_supported %" PRId32 "\n",
		ncap.shaper_private_supported);
	printf("cap.shaper_private_dual_rate_supported %" PRId32 "\n",
		ncap.shaper_private_dual_rate_supported);
	printf("cap.shaper_private_rate_min %" PRIu64 "\n",
		ncap.shaper_private_rate_min);
	printf("cap.shaper_private_rate_max %" PRIu64 "\n",
		ncap.shaper_private_rate_max);
	printf("cap.shaper_shared_n_max %" PRIu32 "\n",
		ncap.shaper_shared_n_max);
	if (!is_leaf) {
		printf("cap.nonleaf.sched_n_children_max %" PRIu32 "\n",
			ncap.nonleaf.sched_n_children_max);
		printf("cap.nonleaf.sched_sp_n_priorities_max %" PRIu32 "\n",
			ncap.nonleaf.sched_sp_n_priorities_max);
		printf("cap.nonleaf.sched_wfq_n_children_per_group_max %" PRIu32
			"\n", ncap.nonleaf.sched_wfq_n_children_per_group_max);
		printf("cap.nonleaf.sched_wfq_n_groups_max %" PRIu32 "\n",
			ncap.nonleaf.sched_wfq_n_groups_max);
		printf("cap.nonleaf.sched_wfq_weight_max %" PRIu32 "\n",
			ncap.nonleaf.sched_wfq_weight_max);
	} else {
		printf("cap.leaf.cman_head_drop_supported %" PRId32 "\n",
			ncap.leaf.cman_head_drop_supported);
		printf("cap.leaf.cman_wred_context_private_supported %" PRId32
			"\n", ncap.leaf.cman_wred_context_private_supported);
		printf("cap.leaf.cman_wred_context_shared_n_max %" PRIu32 "\n",
			ncap.leaf.cman_wred_context_shared_n_max);
	}
	printf("cap.stats_mask %" PRIx64 "\n", ncap.stats_mask);
}

cmdline_parse_inst_t cmd_show_port_tm_node_cap = {
	.f = cmd_show_port_tm_node_cap_parsed,
	.data = NULL,
	.help_str = "Show Port TM Hierarchy node capabilities",
	.tokens = {
		(void *)&cmd_show_port_tm_node_cap_show,
		(void *)&cmd_show_port_tm_node_cap_port,
		(void *)&cmd_show_port_tm_node_cap_tm,
		(void *)&cmd_show_port_tm_node_cap_node,
		(void *)&cmd_show_port_tm_node_cap_cap,
		(void *)&cmd_show_port_tm_node_cap_port_id,
		(void *)&cmd_show_port_tm_node_cap_node_id,
		NULL,
	},
};

/* *** Show Port TM Node Statistics *** */
struct cmd_show_port_tm_node_stats_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t tm;
	cmdline_fixed_string_t node;
	cmdline_fixed_string_t stats;
	uint16_t port_id;
	uint32_t node_id;
	uint32_t clear;
};

cmdline_parse_token_string_t cmd_show_port_tm_node_stats_show =
	TOKEN_STRING_INITIALIZER(
		struct cmd_show_port_tm_node_stats_result, show, "show");
cmdline_parse_token_string_t cmd_show_port_tm_node_stats_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_show_port_tm_node_stats_result, port, "port");
cmdline_parse_token_string_t cmd_show_port_tm_node_stats_tm =
	TOKEN_STRING_INITIALIZER(
		struct cmd_show_port_tm_node_stats_result, tm, "tm");
cmdline_parse_token_string_t cmd_show_port_tm_node_stats_node =
	TOKEN_STRING_INITIALIZER(
		struct cmd_show_port_tm_node_stats_result, node, "node");
cmdline_parse_token_string_t cmd_show_port_tm_node_stats_stats =
	TOKEN_STRING_INITIALIZER(
		struct cmd_show_port_tm_node_stats_result, stats, "stats");
cmdline_parse_token_num_t cmd_show_port_tm_node_stats_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_show_port_tm_node_stats_result,
			port_id, UINT16);
cmdline_parse_token_num_t cmd_show_port_tm_node_stats_node_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_show_port_tm_node_stats_result,
			node_id, UINT32);
cmdline_parse_token_num_t cmd_show_port_tm_node_stats_clear =
	TOKEN_NUM_INITIALIZER(
		struct cmd_show_port_tm_node_stats_result, clear, UINT32);

static void cmd_show_port_tm_node_stats_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_show_port_tm_node_stats_result *res = parsed_result;
	struct rte_tm_node_stats stats;
	struct rte_tm_error error;
	uint64_t stats_mask = 0;
	uint32_t node_id = res->node_id;
	uint32_t clear = res->clear;
	portid_t port_id = res->port_id;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&error, 0, sizeof(struct rte_tm_error));
	/* Port status */
	if (!port_is_started(port_id)) {
		printf(" Port %u not started (error)\n", port_id);
		return;
	}

	memset(&stats, 0, sizeof(struct rte_tm_node_stats));
	ret = rte_tm_node_stats_read(port_id, node_id, &stats,
			&stats_mask, clear, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}

	/* Display stats */
	if (stats_mask & RTE_TM_STATS_N_PKTS)
		printf("\tPkts scheduled from node: %" PRIu64 "\n",
			stats.n_pkts);
	if (stats_mask & RTE_TM_STATS_N_BYTES)
		printf("\tBytes scheduled from node: %" PRIu64 "\n",
			stats.n_bytes);
	if (stats_mask & RTE_TM_STATS_N_PKTS_GREEN_DROPPED)
		printf("\tPkts dropped (green): %" PRIu64 "\n",
			stats.leaf.n_pkts_dropped[RTE_TM_GREEN]);
	if (stats_mask & RTE_TM_STATS_N_PKTS_YELLOW_DROPPED)
		printf("\tPkts dropped (yellow): %" PRIu64 "\n",
			stats.leaf.n_pkts_dropped[RTE_TM_YELLOW]);
	if (stats_mask & RTE_TM_STATS_N_PKTS_RED_DROPPED)
		printf("\tPkts dropped (red): %" PRIu64 "\n",
			stats.leaf.n_pkts_dropped[RTE_TM_RED]);
	if (stats_mask & RTE_TM_STATS_N_BYTES_GREEN_DROPPED)
		printf("\tBytes dropped (green): %" PRIu64 "\n",
			stats.leaf.n_bytes_dropped[RTE_TM_GREEN]);
	if (stats_mask & RTE_TM_STATS_N_BYTES_YELLOW_DROPPED)
		printf("\tBytes dropped (yellow): %" PRIu64 "\n",
			stats.leaf.n_bytes_dropped[RTE_TM_YELLOW]);
	if (stats_mask & RTE_TM_STATS_N_BYTES_RED_DROPPED)
		printf("\tBytes dropped (red): %" PRIu64 "\n",
			stats.leaf.n_bytes_dropped[RTE_TM_RED]);
	if (stats_mask & RTE_TM_STATS_N_PKTS_QUEUED)
		printf("\tPkts queued: %" PRIu64 "\n",
			stats.leaf.n_pkts_queued);
	if (stats_mask & RTE_TM_STATS_N_BYTES_QUEUED)
		printf("\tBytes queued: %" PRIu64 "\n",
			stats.leaf.n_bytes_queued);
}

cmdline_parse_inst_t cmd_show_port_tm_node_stats = {
	.f = cmd_show_port_tm_node_stats_parsed,
	.data = NULL,
	.help_str = "Show port tm node stats",
	.tokens = {
		(void *)&cmd_show_port_tm_node_stats_show,
		(void *)&cmd_show_port_tm_node_stats_port,
		(void *)&cmd_show_port_tm_node_stats_tm,
		(void *)&cmd_show_port_tm_node_stats_node,
		(void *)&cmd_show_port_tm_node_stats_stats,
		(void *)&cmd_show_port_tm_node_stats_port_id,
		(void *)&cmd_show_port_tm_node_stats_node_id,
		(void *)&cmd_show_port_tm_node_stats_clear,
		NULL,
	},
};

/* *** Show Port TM Node Type *** */
struct cmd_show_port_tm_node_type_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t tm;
	cmdline_fixed_string_t node;
	cmdline_fixed_string_t type;
	uint16_t port_id;
	uint32_t node_id;
};

cmdline_parse_token_string_t cmd_show_port_tm_node_type_show =
	TOKEN_STRING_INITIALIZER(
		struct cmd_show_port_tm_node_type_result, show, "show");
cmdline_parse_token_string_t cmd_show_port_tm_node_type_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_show_port_tm_node_type_result, port, "port");
cmdline_parse_token_string_t cmd_show_port_tm_node_type_tm =
	TOKEN_STRING_INITIALIZER(
		struct cmd_show_port_tm_node_type_result, tm, "tm");
cmdline_parse_token_string_t cmd_show_port_tm_node_type_node =
	TOKEN_STRING_INITIALIZER(
		struct cmd_show_port_tm_node_type_result, node, "node");
cmdline_parse_token_string_t cmd_show_port_tm_node_type_type =
	TOKEN_STRING_INITIALIZER(
		struct cmd_show_port_tm_node_type_result, type, "type");
cmdline_parse_token_num_t cmd_show_port_tm_node_type_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_show_port_tm_node_type_result,
			port_id, UINT16);
cmdline_parse_token_num_t cmd_show_port_tm_node_type_node_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_show_port_tm_node_type_result,
			node_id, UINT32);

static void cmd_show_port_tm_node_type_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_show_port_tm_node_type_result *res = parsed_result;
	struct rte_tm_error error;
	uint32_t node_id = res->node_id;
	portid_t port_id = res->port_id;
	int ret, is_leaf = 0;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&error, 0, sizeof(struct rte_tm_error));
	ret = rte_tm_node_type_get(port_id, node_id, &is_leaf, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}

	if (is_leaf == 1)
		printf("leaf node\n");
	else
		printf("nonleaf node\n");

}

cmdline_parse_inst_t cmd_show_port_tm_node_type = {
	.f = cmd_show_port_tm_node_type_parsed,
	.data = NULL,
	.help_str = "Show port tm node type",
	.tokens = {
		(void *)&cmd_show_port_tm_node_type_show,
		(void *)&cmd_show_port_tm_node_type_port,
		(void *)&cmd_show_port_tm_node_type_tm,
		(void *)&cmd_show_port_tm_node_type_node,
		(void *)&cmd_show_port_tm_node_type_type,
		(void *)&cmd_show_port_tm_node_type_port_id,
		(void *)&cmd_show_port_tm_node_type_node_id,
		NULL,
	},
};

/* *** Add Port TM Private Shaper Profile *** */
struct cmd_add_port_tm_node_shaper_profile_result {
	cmdline_fixed_string_t add;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t tm;
	cmdline_fixed_string_t node;
	cmdline_fixed_string_t shaper;
	cmdline_fixed_string_t profile;
	uint16_t port_id;
	uint32_t shaper_id;
	uint64_t cmit_tb_rate;
	uint64_t cmit_tb_size;
	uint64_t peak_tb_rate;
	uint64_t peak_tb_size;
	uint32_t pktlen_adjust;
};

cmdline_parse_token_string_t cmd_add_port_tm_node_shaper_profile_add =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_node_shaper_profile_result, add, "add");
cmdline_parse_token_string_t cmd_add_port_tm_node_shaper_profile_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_node_shaper_profile_result,
			port, "port");
cmdline_parse_token_string_t cmd_add_port_tm_node_shaper_profile_tm =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_node_shaper_profile_result,
			tm, "tm");
cmdline_parse_token_string_t cmd_add_port_tm_node_shaper_profile_node =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_node_shaper_profile_result,
			node, "node");
cmdline_parse_token_string_t cmd_add_port_tm_node_shaper_profile_shaper =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_node_shaper_profile_result,
			shaper, "shaper");
cmdline_parse_token_string_t cmd_add_port_tm_node_shaper_profile_profile =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_node_shaper_profile_result,
			profile, "profile");
cmdline_parse_token_num_t cmd_add_port_tm_node_shaper_profile_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_shaper_profile_result,
			port_id, UINT16);
cmdline_parse_token_num_t cmd_add_port_tm_node_shaper_profile_shaper_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_shaper_profile_result,
			shaper_id, UINT32);
cmdline_parse_token_num_t cmd_add_port_tm_node_shaper_profile_cmit_tb_rate =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_shaper_profile_result,
			cmit_tb_rate, UINT64);
cmdline_parse_token_num_t cmd_add_port_tm_node_shaper_profile_cmit_tb_size =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_shaper_profile_result,
			cmit_tb_size, UINT64);
cmdline_parse_token_num_t cmd_add_port_tm_node_shaper_profile_peak_tb_rate =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_shaper_profile_result,
			peak_tb_rate, UINT64);
cmdline_parse_token_num_t cmd_add_port_tm_node_shaper_profile_peak_tb_size =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_shaper_profile_result,
			peak_tb_size, UINT64);
cmdline_parse_token_num_t cmd_add_port_tm_node_shaper_profile_pktlen_adjust =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_shaper_profile_result,
			pktlen_adjust, UINT32);

static void cmd_add_port_tm_node_shaper_profile_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_add_port_tm_node_shaper_profile_result *res = parsed_result;
	struct rte_tm_shaper_params sp;
	struct rte_tm_error error;
	uint32_t shaper_id = res->shaper_id;
	uint32_t pkt_len_adjust = res->pktlen_adjust;
	portid_t port_id = res->port_id;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	/* Private shaper profile params */
	memset(&sp, 0, sizeof(struct rte_tm_shaper_params));
	memset(&error, 0, sizeof(struct rte_tm_error));
	sp.committed.rate = res->cmit_tb_rate;
	sp.committed.size = res->cmit_tb_size;
	sp.peak.rate = res->peak_tb_rate;
	sp.peak.size = res->peak_tb_size;
	sp.pkt_length_adjust = pkt_len_adjust;

	ret = rte_tm_shaper_profile_add(port_id, shaper_id, &sp, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_add_port_tm_node_shaper_profile = {
	.f = cmd_add_port_tm_node_shaper_profile_parsed,
	.data = NULL,
	.help_str = "Add port tm node private shaper profile",
	.tokens = {
		(void *)&cmd_add_port_tm_node_shaper_profile_add,
		(void *)&cmd_add_port_tm_node_shaper_profile_port,
		(void *)&cmd_add_port_tm_node_shaper_profile_tm,
		(void *)&cmd_add_port_tm_node_shaper_profile_node,
		(void *)&cmd_add_port_tm_node_shaper_profile_shaper,
		(void *)&cmd_add_port_tm_node_shaper_profile_profile,
		(void *)&cmd_add_port_tm_node_shaper_profile_port_id,
		(void *)&cmd_add_port_tm_node_shaper_profile_shaper_id,
		(void *)&cmd_add_port_tm_node_shaper_profile_cmit_tb_rate,
		(void *)&cmd_add_port_tm_node_shaper_profile_cmit_tb_size,
		(void *)&cmd_add_port_tm_node_shaper_profile_peak_tb_rate,
		(void *)&cmd_add_port_tm_node_shaper_profile_peak_tb_size,
		(void *)&cmd_add_port_tm_node_shaper_profile_pktlen_adjust,
		NULL,
	},
};

/* *** Delete Port TM Private Shaper Profile *** */
struct cmd_del_port_tm_node_shaper_profile_result {
	cmdline_fixed_string_t del;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t tm;
	cmdline_fixed_string_t node;
	cmdline_fixed_string_t shaper;
	cmdline_fixed_string_t profile;
	uint16_t port_id;
	uint32_t shaper_id;
};

cmdline_parse_token_string_t cmd_del_port_tm_node_shaper_profile_del =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_tm_node_shaper_profile_result, del, "del");
cmdline_parse_token_string_t cmd_del_port_tm_node_shaper_profile_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_tm_node_shaper_profile_result,
			port, "port");
cmdline_parse_token_string_t cmd_del_port_tm_node_shaper_profile_tm =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_tm_node_shaper_profile_result, tm, "tm");
cmdline_parse_token_string_t cmd_del_port_tm_node_shaper_profile_node =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_tm_node_shaper_profile_result,
			node, "node");
cmdline_parse_token_string_t cmd_del_port_tm_node_shaper_profile_shaper =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_tm_node_shaper_profile_result,
			shaper, "shaper");
cmdline_parse_token_string_t cmd_del_port_tm_node_shaper_profile_profile =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_tm_node_shaper_profile_result,
			profile, "profile");
cmdline_parse_token_num_t cmd_del_port_tm_node_shaper_profile_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_del_port_tm_node_shaper_profile_result,
			port_id, UINT16);
cmdline_parse_token_num_t cmd_del_port_tm_node_shaper_profile_shaper_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_del_port_tm_node_shaper_profile_result,
			shaper_id, UINT32);

static void cmd_del_port_tm_node_shaper_profile_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_del_port_tm_node_shaper_profile_result *res = parsed_result;
	struct rte_tm_error error;
	uint32_t shaper_id = res->shaper_id;
	portid_t port_id = res->port_id;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&error, 0, sizeof(struct rte_tm_error));
	ret = rte_tm_shaper_profile_delete(port_id, shaper_id, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_del_port_tm_node_shaper_profile = {
	.f = cmd_del_port_tm_node_shaper_profile_parsed,
	.data = NULL,
	.help_str = "Delete port tm node private shaper profile",
	.tokens = {
		(void *)&cmd_del_port_tm_node_shaper_profile_del,
		(void *)&cmd_del_port_tm_node_shaper_profile_port,
		(void *)&cmd_del_port_tm_node_shaper_profile_tm,
		(void *)&cmd_del_port_tm_node_shaper_profile_node,
		(void *)&cmd_del_port_tm_node_shaper_profile_shaper,
		(void *)&cmd_del_port_tm_node_shaper_profile_profile,
		(void *)&cmd_del_port_tm_node_shaper_profile_port_id,
		(void *)&cmd_del_port_tm_node_shaper_profile_shaper_id,
		NULL,
	},
};

/* *** Add/Update Port TM shared Shaper *** */
struct cmd_add_port_tm_node_shared_shaper_result {
	cmdline_fixed_string_t cmd_type;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t tm;
	cmdline_fixed_string_t node;
	cmdline_fixed_string_t shared;
	cmdline_fixed_string_t shaper;
	uint16_t port_id;
	uint32_t shared_shaper_id;
	uint32_t shaper_profile_id;
};

cmdline_parse_token_string_t cmd_add_port_tm_node_shared_shaper_cmd_type =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_node_shared_shaper_result,
			cmd_type, "add#set");
cmdline_parse_token_string_t cmd_add_port_tm_node_shared_shaper_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_node_shared_shaper_result, port, "port");
cmdline_parse_token_string_t cmd_add_port_tm_node_shared_shaper_tm =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_node_shared_shaper_result, tm, "tm");
cmdline_parse_token_string_t cmd_add_port_tm_node_shared_shaper_node =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_node_shared_shaper_result, node, "node");
cmdline_parse_token_string_t cmd_add_port_tm_node_shared_shaper_shared =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_node_shared_shaper_result,
			shared, "shared");
cmdline_parse_token_string_t cmd_add_port_tm_node_shared_shaper_shaper =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_node_shared_shaper_result,
			shaper, "shaper");
cmdline_parse_token_num_t cmd_add_port_tm_node_shared_shaper_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_shared_shaper_result,
			port_id, UINT16);
cmdline_parse_token_num_t cmd_add_port_tm_node_shared_shaper_shared_shaper_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_shared_shaper_result,
			shared_shaper_id, UINT32);
cmdline_parse_token_num_t cmd_add_port_tm_node_shared_shaper_shaper_profile_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_shared_shaper_result,
			shaper_profile_id, UINT32);

static void cmd_add_port_tm_node_shared_shaper_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_add_port_tm_node_shared_shaper_result *res = parsed_result;
	struct rte_tm_error error;
	uint32_t shared_shaper_id = res->shared_shaper_id;
	uint32_t shaper_profile_id = res->shaper_profile_id;
	portid_t port_id = res->port_id;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&error, 0, sizeof(struct rte_tm_error));
	/* Command type: add */
	if ((strcmp(res->cmd_type, "add") == 0) &&
		(port_is_started(port_id))) {
		printf(" Port %u not stopped (error)\n", port_id);
		return;
	}

	/* Command type: set (update) */
	if ((strcmp(res->cmd_type, "set") == 0) &&
		(!port_is_started(port_id))) {
		printf(" Port %u not started (error)\n", port_id);
		return;
	}

	ret = rte_tm_shared_shaper_add_update(port_id, shared_shaper_id,
		shaper_profile_id, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_add_port_tm_node_shared_shaper = {
	.f = cmd_add_port_tm_node_shared_shaper_parsed,
	.data = NULL,
	.help_str = "add/update port tm node shared shaper",
	.tokens = {
		(void *)&cmd_add_port_tm_node_shared_shaper_cmd_type,
		(void *)&cmd_add_port_tm_node_shared_shaper_port,
		(void *)&cmd_add_port_tm_node_shared_shaper_tm,
		(void *)&cmd_add_port_tm_node_shared_shaper_node,
		(void *)&cmd_add_port_tm_node_shared_shaper_shared,
		(void *)&cmd_add_port_tm_node_shared_shaper_shaper,
		(void *)&cmd_add_port_tm_node_shared_shaper_port_id,
		(void *)&cmd_add_port_tm_node_shared_shaper_shared_shaper_id,
		(void *)&cmd_add_port_tm_node_shared_shaper_shaper_profile_id,
		NULL,
	},
};

/* *** Delete Port TM shared Shaper *** */
struct cmd_del_port_tm_node_shared_shaper_result {
	cmdline_fixed_string_t del;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t tm;
	cmdline_fixed_string_t node;
	cmdline_fixed_string_t shared;
	cmdline_fixed_string_t shaper;
	uint16_t port_id;
	uint32_t shared_shaper_id;
};

cmdline_parse_token_string_t cmd_del_port_tm_node_shared_shaper_del =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_tm_node_shared_shaper_result, del, "del");
cmdline_parse_token_string_t cmd_del_port_tm_node_shared_shaper_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_tm_node_shared_shaper_result, port, "port");
cmdline_parse_token_string_t cmd_del_port_tm_node_shared_shaper_tm =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_tm_node_shared_shaper_result, tm, "tm");
cmdline_parse_token_string_t cmd_del_port_tm_node_shared_shaper_node =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_tm_node_shared_shaper_result, node, "node");
cmdline_parse_token_string_t cmd_del_port_tm_node_shared_shaper_shared =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_tm_node_shared_shaper_result,
			shared, "shared");
cmdline_parse_token_string_t cmd_del_port_tm_node_shared_shaper_shaper =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_tm_node_shared_shaper_result,
			shaper, "shaper");
cmdline_parse_token_num_t cmd_del_port_tm_node_shared_shaper_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_del_port_tm_node_shared_shaper_result,
			port_id, UINT16);
cmdline_parse_token_num_t cmd_del_port_tm_node_shared_shaper_shared_shaper_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_del_port_tm_node_shared_shaper_result,
			shared_shaper_id, UINT32);

static void cmd_del_port_tm_node_shared_shaper_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_del_port_tm_node_shared_shaper_result *res = parsed_result;
	struct rte_tm_error error;
	uint32_t shared_shaper_id = res->shared_shaper_id;
	portid_t port_id = res->port_id;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&error, 0, sizeof(struct rte_tm_error));
	ret = rte_tm_shared_shaper_delete(port_id, shared_shaper_id, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_del_port_tm_node_shared_shaper = {
	.f = cmd_del_port_tm_node_shared_shaper_parsed,
	.data = NULL,
	.help_str = "delete port tm node shared shaper",
	.tokens = {
		(void *)&cmd_del_port_tm_node_shared_shaper_del,
		(void *)&cmd_del_port_tm_node_shared_shaper_port,
		(void *)&cmd_del_port_tm_node_shared_shaper_tm,
		(void *)&cmd_del_port_tm_node_shared_shaper_node,
		(void *)&cmd_del_port_tm_node_shared_shaper_shared,
		(void *)&cmd_del_port_tm_node_shared_shaper_shaper,
		(void *)&cmd_del_port_tm_node_shared_shaper_port_id,
		(void *)&cmd_del_port_tm_node_shared_shaper_shared_shaper_id,
		NULL,
	},
};

/* *** Add Port TM Node WRED Profile *** */
struct cmd_add_port_tm_node_wred_profile_result {
	cmdline_fixed_string_t add;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t tm;
	cmdline_fixed_string_t node;
	cmdline_fixed_string_t wred;
	cmdline_fixed_string_t profile;
	uint16_t port_id;
	uint32_t wred_profile_id;
	cmdline_fixed_string_t color_g;
	uint64_t min_th_g;
	uint64_t max_th_g;
	uint16_t maxp_inv_g;
	uint16_t wq_log2_g;
	cmdline_fixed_string_t color_y;
	uint64_t min_th_y;
	uint64_t max_th_y;
	uint16_t maxp_inv_y;
	uint16_t wq_log2_y;
	cmdline_fixed_string_t color_r;
	uint64_t min_th_r;
	uint64_t max_th_r;
	uint16_t maxp_inv_r;
	uint16_t wq_log2_r;
};

cmdline_parse_token_string_t cmd_add_port_tm_node_wred_profile_add =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result, add, "add");
cmdline_parse_token_string_t cmd_add_port_tm_node_wred_profile_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result, port, "port");
cmdline_parse_token_string_t cmd_add_port_tm_node_wred_profile_tm =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result, tm, "tm");
cmdline_parse_token_string_t cmd_add_port_tm_node_wred_profile_node =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result, node, "node");
cmdline_parse_token_string_t cmd_add_port_tm_node_wred_profile_wred =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result, wred, "wred");
cmdline_parse_token_string_t cmd_add_port_tm_node_wred_profile_profile =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result,
			profile, "profile");
cmdline_parse_token_num_t cmd_add_port_tm_node_wred_profile_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result,
			port_id, UINT16);
cmdline_parse_token_num_t cmd_add_port_tm_node_wred_profile_wred_profile_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result,
			wred_profile_id, UINT32);
cmdline_parse_token_string_t cmd_add_port_tm_node_wred_profile_color_g =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result,
			color_g, "G#g");
cmdline_parse_token_num_t cmd_add_port_tm_node_wred_profile_min_th_g =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result,
			min_th_g, UINT64);
cmdline_parse_token_num_t cmd_add_port_tm_node_wred_profile_max_th_g =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result,
			max_th_g, UINT64);
cmdline_parse_token_num_t cmd_add_port_tm_node_wred_profile_maxp_inv_g =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result,
			maxp_inv_g, UINT16);
cmdline_parse_token_num_t cmd_add_port_tm_node_wred_profile_wq_log2_g =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result,
			wq_log2_g, UINT16);
cmdline_parse_token_string_t cmd_add_port_tm_node_wred_profile_color_y =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result,
			color_y, "Y#y");
cmdline_parse_token_num_t cmd_add_port_tm_node_wred_profile_min_th_y =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result,
			min_th_y, UINT64);
cmdline_parse_token_num_t cmd_add_port_tm_node_wred_profile_max_th_y =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result,
			max_th_y, UINT64);
cmdline_parse_token_num_t cmd_add_port_tm_node_wred_profile_maxp_inv_y =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result,
			maxp_inv_y, UINT16);
cmdline_parse_token_num_t cmd_add_port_tm_node_wred_profile_wq_log2_y =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result,
			wq_log2_y, UINT16);
cmdline_parse_token_string_t cmd_add_port_tm_node_wred_profile_color_r =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result,
			color_r, "R#r");
cmdline_parse_token_num_t cmd_add_port_tm_node_wred_profile_min_th_r =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result,
			min_th_r, UINT64);
cmdline_parse_token_num_t cmd_add_port_tm_node_wred_profile_max_th_r =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result,
			max_th_r, UINT64);
cmdline_parse_token_num_t cmd_add_port_tm_node_wred_profile_maxp_inv_r =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result,
			maxp_inv_r, UINT16);
cmdline_parse_token_num_t cmd_add_port_tm_node_wred_profile_wq_log2_r =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_node_wred_profile_result,
			wq_log2_r, UINT16);


static void cmd_add_port_tm_node_wred_profile_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_add_port_tm_node_wred_profile_result *res = parsed_result;
	struct rte_tm_wred_params wp;
	enum rte_tm_color color;
	struct rte_tm_error error;
	uint32_t wred_profile_id = res->wred_profile_id;
	portid_t port_id = res->port_id;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&wp, 0, sizeof(struct rte_tm_wred_params));
	memset(&error, 0, sizeof(struct rte_tm_error));

	/* WRED Params  (Green Color)*/
	color = RTE_TM_GREEN;
	wp.red_params[color].min_th = res->min_th_g;
	wp.red_params[color].max_th = res->max_th_g;
	wp.red_params[color].maxp_inv = res->maxp_inv_g;
	wp.red_params[color].wq_log2 = res->wq_log2_g;


	/* WRED Params  (Yellow Color)*/
	color = RTE_TM_YELLOW;
	wp.red_params[color].min_th = res->min_th_y;
	wp.red_params[color].max_th = res->max_th_y;
	wp.red_params[color].maxp_inv = res->maxp_inv_y;
	wp.red_params[color].wq_log2 = res->wq_log2_y;

	/* WRED Params  (Red Color)*/
	color = RTE_TM_RED;
	wp.red_params[color].min_th = res->min_th_r;
	wp.red_params[color].max_th = res->max_th_r;
	wp.red_params[color].maxp_inv = res->maxp_inv_r;
	wp.red_params[color].wq_log2 = res->wq_log2_r;

	ret = rte_tm_wred_profile_add(port_id, wred_profile_id, &wp, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_add_port_tm_node_wred_profile = {
	.f = cmd_add_port_tm_node_wred_profile_parsed,
	.data = NULL,
	.help_str = "Add port tm node wred profile",
	.tokens = {
		(void *)&cmd_add_port_tm_node_wred_profile_add,
		(void *)&cmd_add_port_tm_node_wred_profile_port,
		(void *)&cmd_add_port_tm_node_wred_profile_tm,
		(void *)&cmd_add_port_tm_node_wred_profile_node,
		(void *)&cmd_add_port_tm_node_wred_profile_wred,
		(void *)&cmd_add_port_tm_node_wred_profile_profile,
		(void *)&cmd_add_port_tm_node_wred_profile_port_id,
		(void *)&cmd_add_port_tm_node_wred_profile_wred_profile_id,
		(void *)&cmd_add_port_tm_node_wred_profile_color_g,
		(void *)&cmd_add_port_tm_node_wred_profile_min_th_g,
		(void *)&cmd_add_port_tm_node_wred_profile_max_th_g,
		(void *)&cmd_add_port_tm_node_wred_profile_maxp_inv_g,
		(void *)&cmd_add_port_tm_node_wred_profile_wq_log2_g,
		(void *)&cmd_add_port_tm_node_wred_profile_color_y,
		(void *)&cmd_add_port_tm_node_wred_profile_min_th_y,
		(void *)&cmd_add_port_tm_node_wred_profile_max_th_y,
		(void *)&cmd_add_port_tm_node_wred_profile_maxp_inv_y,
		(void *)&cmd_add_port_tm_node_wred_profile_wq_log2_y,
		(void *)&cmd_add_port_tm_node_wred_profile_color_r,
		(void *)&cmd_add_port_tm_node_wred_profile_min_th_r,
		(void *)&cmd_add_port_tm_node_wred_profile_max_th_r,
		(void *)&cmd_add_port_tm_node_wred_profile_maxp_inv_r,
		(void *)&cmd_add_port_tm_node_wred_profile_wq_log2_r,
		NULL,
	},
};

/* *** Delete Port TM node WRED Profile *** */
struct cmd_del_port_tm_node_wred_profile_result {
	cmdline_fixed_string_t del;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t tm;
	cmdline_fixed_string_t node;
	cmdline_fixed_string_t wred;
	cmdline_fixed_string_t profile;
	uint16_t port_id;
	uint32_t wred_profile_id;
};

cmdline_parse_token_string_t cmd_del_port_tm_node_wred_profile_del =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_tm_node_wred_profile_result, del, "del");
cmdline_parse_token_string_t cmd_del_port_tm_node_wred_profile_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_tm_node_wred_profile_result, port, "port");
cmdline_parse_token_string_t cmd_del_port_tm_node_wred_profile_tm =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_tm_node_wred_profile_result, tm, "tm");
cmdline_parse_token_string_t cmd_del_port_tm_node_wred_profile_node =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_tm_node_wred_profile_result, node, "node");
cmdline_parse_token_string_t cmd_del_port_tm_node_wred_profile_wred =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_tm_node_wred_profile_result, wred, "wred");
cmdline_parse_token_string_t cmd_del_port_tm_node_wred_profile_profile =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_tm_node_wred_profile_result,
			profile, "profile");
cmdline_parse_token_num_t cmd_del_port_tm_node_wred_profile_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_del_port_tm_node_wred_profile_result,
			port_id, UINT16);
cmdline_parse_token_num_t cmd_del_port_tm_node_wred_profile_wred_profile_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_del_port_tm_node_wred_profile_result,
			wred_profile_id, UINT32);

static void cmd_del_port_tm_node_wred_profile_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_del_port_tm_node_wred_profile_result *res = parsed_result;
	struct rte_tm_error error;
	uint32_t wred_profile_id = res->wred_profile_id;
	portid_t port_id = res->port_id;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&error, 0, sizeof(struct rte_tm_error));
	ret = rte_tm_wred_profile_delete(port_id, wred_profile_id, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_del_port_tm_node_wred_profile = {
	.f = cmd_del_port_tm_node_wred_profile_parsed,
	.data = NULL,
	.help_str = "Delete port tm node wred profile",
	.tokens = {
		(void *)&cmd_del_port_tm_node_wred_profile_del,
		(void *)&cmd_del_port_tm_node_wred_profile_port,
		(void *)&cmd_del_port_tm_node_wred_profile_tm,
		(void *)&cmd_del_port_tm_node_wred_profile_node,
		(void *)&cmd_del_port_tm_node_wred_profile_wred,
		(void *)&cmd_del_port_tm_node_wred_profile_profile,
		(void *)&cmd_del_port_tm_node_wred_profile_port_id,
		(void *)&cmd_del_port_tm_node_wred_profile_wred_profile_id,
		NULL,
	},
};

/* *** Update Port TM Node Shaper profile *** */
struct cmd_set_port_tm_node_shaper_profile_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t tm;
	cmdline_fixed_string_t node;
	cmdline_fixed_string_t shaper;
	cmdline_fixed_string_t profile;
	uint16_t port_id;
	uint32_t node_id;
	uint32_t shaper_profile_id;
};

cmdline_parse_token_string_t cmd_set_port_tm_node_shaper_profile_set =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_tm_node_shaper_profile_result, set, "set");
cmdline_parse_token_string_t cmd_set_port_tm_node_shaper_profile_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_tm_node_shaper_profile_result,
			port, "port");
cmdline_parse_token_string_t cmd_set_port_tm_node_shaper_profile_tm =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_tm_node_shaper_profile_result, tm, "tm");
cmdline_parse_token_string_t cmd_set_port_tm_node_shaper_profile_node =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_tm_node_shaper_profile_result,
			node, "node");
cmdline_parse_token_string_t cmd_set_port_tm_node_shaper_profile_shaper =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_tm_node_shaper_profile_result,
			shaper, "shaper");
cmdline_parse_token_string_t cmd_set_port_tm_node_shaper_profile_profile =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_tm_node_shaper_profile_result,
			profile, "profile");
cmdline_parse_token_num_t cmd_set_port_tm_node_shaper_profile_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_set_port_tm_node_shaper_profile_result,
			port_id, UINT16);
cmdline_parse_token_num_t cmd_set_port_tm_node_shaper_profile_node_id =
	TOKEN_NUM_INITIALIZER(struct cmd_set_port_tm_node_shaper_profile_result,
		node_id, UINT32);
cmdline_parse_token_num_t
	cmd_set_port_tm_node_shaper_shaper_profile_profile_id =
		TOKEN_NUM_INITIALIZER(
			struct cmd_set_port_tm_node_shaper_profile_result,
			shaper_profile_id, UINT32);

static void cmd_set_port_tm_node_shaper_profile_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_set_port_tm_node_shaper_profile_result *res = parsed_result;
	struct rte_tm_error error;
	uint32_t node_id = res->node_id;
	uint32_t shaper_profile_id = res->shaper_profile_id;
	portid_t port_id = res->port_id;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&error, 0, sizeof(struct rte_tm_error));
	/* Port status */
	if (!port_is_started(port_id)) {
		printf(" Port %u not started (error)\n", port_id);
		return;
	}

	ret = rte_tm_node_shaper_update(port_id, node_id,
		shaper_profile_id, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_set_port_tm_node_shaper_profile = {
	.f = cmd_set_port_tm_node_shaper_profile_parsed,
	.data = NULL,
	.help_str = "Set port tm node shaper profile",
	.tokens = {
		(void *)&cmd_set_port_tm_node_shaper_profile_set,
		(void *)&cmd_set_port_tm_node_shaper_profile_port,
		(void *)&cmd_set_port_tm_node_shaper_profile_tm,
		(void *)&cmd_set_port_tm_node_shaper_profile_node,
		(void *)&cmd_set_port_tm_node_shaper_profile_shaper,
		(void *)&cmd_set_port_tm_node_shaper_profile_profile,
		(void *)&cmd_set_port_tm_node_shaper_profile_port_id,
		(void *)&cmd_set_port_tm_node_shaper_profile_node_id,
		(void *)&cmd_set_port_tm_node_shaper_shaper_profile_profile_id,
		NULL,
	},
};

/* *** Add Port TM nonleaf node *** */
struct cmd_add_port_tm_nonleaf_node_result {
	cmdline_fixed_string_t add;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t tm;
	cmdline_fixed_string_t nonleaf;
	cmdline_fixed_string_t node;
	uint16_t port_id;
	uint32_t node_id;
	int32_t parent_node_id;
	uint32_t priority;
	uint32_t weight;
	uint32_t level_id;
	int32_t shaper_profile_id;
	uint32_t n_sp_priorities;
	uint64_t stats_mask;
	cmdline_multi_string_t multi_shared_shaper_id;
};

cmdline_parse_token_string_t cmd_add_port_tm_nonleaf_node_add =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_nonleaf_node_result, add, "add");
cmdline_parse_token_string_t cmd_add_port_tm_nonleaf_node_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_nonleaf_node_result, port, "port");
cmdline_parse_token_string_t cmd_add_port_tm_nonleaf_node_tm =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_nonleaf_node_result, tm, "tm");
cmdline_parse_token_string_t cmd_add_port_tm_nonleaf_node_nonleaf =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_nonleaf_node_result, nonleaf, "nonleaf");
cmdline_parse_token_string_t cmd_add_port_tm_nonleaf_node_node =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_nonleaf_node_result, node, "node");
cmdline_parse_token_num_t cmd_add_port_tm_nonleaf_node_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_tm_nonleaf_node_result,
		 port_id, UINT16);
cmdline_parse_token_num_t cmd_add_port_tm_nonleaf_node_node_id =
	TOKEN_NUM_INITIALIZER(struct cmd_add_port_tm_nonleaf_node_result,
		 node_id, UINT32);
cmdline_parse_token_num_t cmd_add_port_tm_nonleaf_node_parent_node_id =
	TOKEN_NUM_INITIALIZER(struct cmd_add_port_tm_nonleaf_node_result,
		 parent_node_id, INT32);
cmdline_parse_token_num_t cmd_add_port_tm_nonleaf_node_priority =
	TOKEN_NUM_INITIALIZER(struct cmd_add_port_tm_nonleaf_node_result,
		 priority, UINT32);
cmdline_parse_token_num_t cmd_add_port_tm_nonleaf_node_weight =
	TOKEN_NUM_INITIALIZER(struct cmd_add_port_tm_nonleaf_node_result,
		 weight, UINT32);
cmdline_parse_token_num_t cmd_add_port_tm_nonleaf_node_level_id =
	TOKEN_NUM_INITIALIZER(struct cmd_add_port_tm_nonleaf_node_result,
		 level_id, UINT32);
cmdline_parse_token_num_t cmd_add_port_tm_nonleaf_node_shaper_profile_id =
	TOKEN_NUM_INITIALIZER(struct cmd_add_port_tm_nonleaf_node_result,
		 shaper_profile_id, INT32);
cmdline_parse_token_num_t cmd_add_port_tm_nonleaf_node_n_sp_priorities =
	TOKEN_NUM_INITIALIZER(struct cmd_add_port_tm_nonleaf_node_result,
		 n_sp_priorities, UINT32);
cmdline_parse_token_num_t cmd_add_port_tm_nonleaf_node_stats_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_add_port_tm_nonleaf_node_result,
		 stats_mask, UINT64);
cmdline_parse_token_string_t
	cmd_add_port_tm_nonleaf_node_multi_shared_shaper_id =
	TOKEN_STRING_INITIALIZER(struct cmd_add_port_tm_nonleaf_node_result,
		 multi_shared_shaper_id, TOKEN_STRING_MULTI);

static void cmd_add_port_tm_nonleaf_node_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_add_port_tm_nonleaf_node_result *res = parsed_result;
	struct rte_tm_error error;
	struct rte_tm_node_params np;
	uint32_t *shared_shaper_id;
	uint32_t parent_node_id, n_shared_shapers = 0;
	char *s_str = res->multi_shared_shaper_id;
	portid_t port_id = res->port_id;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&np, 0, sizeof(struct rte_tm_node_params));
	memset(&error, 0, sizeof(struct rte_tm_error));

	/* Node parameters */
	if (res->parent_node_id < 0)
		parent_node_id = UINT32_MAX;
	else
		parent_node_id = res->parent_node_id;

	shared_shaper_id = (uint32_t *)malloc(MAX_NUM_SHARED_SHAPERS *
		sizeof(uint32_t));
	if (shared_shaper_id == NULL) {
		printf(" Memory not allocated for shared shapers (error)\n");
		return;
	}

	/* Parse multi shared shaper id string */
	ret = parse_multi_ss_id_str(s_str, &n_shared_shapers, shared_shaper_id);
	if (ret) {
		printf(" Shared shapers params string parse error\n");
		free(shared_shaper_id);
		return;
	}

	if (res->shaper_profile_id < 0)
		np.shaper_profile_id = UINT32_MAX;
	else
		np.shaper_profile_id = res->shaper_profile_id;

	np.n_shared_shapers = n_shared_shapers;
	if (np.n_shared_shapers) {
		np.shared_shaper_id = &shared_shaper_id[0];
	} else {
		free(shared_shaper_id);
		shared_shaper_id = NULL;
	}

	np.nonleaf.n_sp_priorities = res->n_sp_priorities;
	np.stats_mask = res->stats_mask;
	np.nonleaf.wfq_weight_mode = NULL;

	ret = rte_tm_node_add(port_id, res->node_id, parent_node_id,
				res->priority, res->weight, res->level_id,
				&np, &error);
	if (ret != 0) {
		print_err_msg(&error);
		free(shared_shaper_id);
		return;
	}
}

cmdline_parse_inst_t cmd_add_port_tm_nonleaf_node = {
	.f = cmd_add_port_tm_nonleaf_node_parsed,
	.data = NULL,
	.help_str = "Add port tm nonleaf node",
	.tokens = {
		(void *)&cmd_add_port_tm_nonleaf_node_add,
		(void *)&cmd_add_port_tm_nonleaf_node_port,
		(void *)&cmd_add_port_tm_nonleaf_node_tm,
		(void *)&cmd_add_port_tm_nonleaf_node_nonleaf,
		(void *)&cmd_add_port_tm_nonleaf_node_node,
		(void *)&cmd_add_port_tm_nonleaf_node_port_id,
		(void *)&cmd_add_port_tm_nonleaf_node_node_id,
		(void *)&cmd_add_port_tm_nonleaf_node_parent_node_id,
		(void *)&cmd_add_port_tm_nonleaf_node_priority,
		(void *)&cmd_add_port_tm_nonleaf_node_weight,
		(void *)&cmd_add_port_tm_nonleaf_node_level_id,
		(void *)&cmd_add_port_tm_nonleaf_node_shaper_profile_id,
		(void *)&cmd_add_port_tm_nonleaf_node_n_sp_priorities,
		(void *)&cmd_add_port_tm_nonleaf_node_stats_mask,
		(void *)&cmd_add_port_tm_nonleaf_node_multi_shared_shaper_id,
		NULL,
	},
};

/* *** Add Port TM leaf node *** */
struct cmd_add_port_tm_leaf_node_result {
	cmdline_fixed_string_t add;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t tm;
	cmdline_fixed_string_t leaf;
	cmdline_fixed_string_t node;
	uint16_t port_id;
	uint32_t node_id;
	int32_t parent_node_id;
	uint32_t priority;
	uint32_t weight;
	uint32_t level_id;
	int32_t shaper_profile_id;
	uint32_t cman_mode;
	uint32_t wred_profile_id;
	uint64_t stats_mask;
	cmdline_multi_string_t multi_shared_shaper_id;
};

cmdline_parse_token_string_t cmd_add_port_tm_leaf_node_add =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_leaf_node_result, add, "add");
cmdline_parse_token_string_t cmd_add_port_tm_leaf_node_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_leaf_node_result, port, "port");
cmdline_parse_token_string_t cmd_add_port_tm_leaf_node_tm =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_leaf_node_result, tm, "tm");
cmdline_parse_token_string_t cmd_add_port_tm_leaf_node_nonleaf =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_leaf_node_result, leaf, "leaf");
cmdline_parse_token_string_t cmd_add_port_tm_leaf_node_node =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_tm_leaf_node_result, node, "node");
cmdline_parse_token_num_t cmd_add_port_tm_leaf_node_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_add_port_tm_leaf_node_result,
		 port_id, UINT16);
cmdline_parse_token_num_t cmd_add_port_tm_leaf_node_node_id =
	TOKEN_NUM_INITIALIZER(struct cmd_add_port_tm_leaf_node_result,
		 node_id, UINT32);
cmdline_parse_token_num_t cmd_add_port_tm_leaf_node_parent_node_id =
	TOKEN_NUM_INITIALIZER(struct cmd_add_port_tm_leaf_node_result,
		 parent_node_id, INT32);
cmdline_parse_token_num_t cmd_add_port_tm_leaf_node_priority =
	TOKEN_NUM_INITIALIZER(struct cmd_add_port_tm_leaf_node_result,
		 priority, UINT32);
cmdline_parse_token_num_t cmd_add_port_tm_leaf_node_weight =
	TOKEN_NUM_INITIALIZER(struct cmd_add_port_tm_leaf_node_result,
		 weight, UINT32);
cmdline_parse_token_num_t cmd_add_port_tm_leaf_node_level_id =
	TOKEN_NUM_INITIALIZER(struct cmd_add_port_tm_leaf_node_result,
		 level_id, UINT32);
cmdline_parse_token_num_t cmd_add_port_tm_leaf_node_shaper_profile_id =
	TOKEN_NUM_INITIALIZER(struct cmd_add_port_tm_leaf_node_result,
		 shaper_profile_id, INT32);
cmdline_parse_token_num_t cmd_add_port_tm_leaf_node_cman_mode =
	TOKEN_NUM_INITIALIZER(struct cmd_add_port_tm_leaf_node_result,
		 cman_mode, UINT32);
cmdline_parse_token_num_t cmd_add_port_tm_leaf_node_wred_profile_id =
	TOKEN_NUM_INITIALIZER(struct cmd_add_port_tm_leaf_node_result,
		 wred_profile_id, UINT32);
cmdline_parse_token_num_t cmd_add_port_tm_leaf_node_stats_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_add_port_tm_leaf_node_result,
		 stats_mask, UINT64);
cmdline_parse_token_string_t
	cmd_add_port_tm_leaf_node_multi_shared_shaper_id =
	TOKEN_STRING_INITIALIZER(struct cmd_add_port_tm_leaf_node_result,
		 multi_shared_shaper_id, TOKEN_STRING_MULTI);

static void cmd_add_port_tm_leaf_node_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_add_port_tm_leaf_node_result *res = parsed_result;
	struct rte_tm_error error;
	struct rte_tm_node_params np;
	uint32_t *shared_shaper_id;
	uint32_t parent_node_id, n_shared_shapers = 0;
	portid_t port_id = res->port_id;
	char *s_str = res->multi_shared_shaper_id;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&np, 0, sizeof(struct rte_tm_node_params));
	memset(&error, 0, sizeof(struct rte_tm_error));

	/* Node parameters */
	if (res->parent_node_id < 0)
		parent_node_id = UINT32_MAX;
	else
		parent_node_id = res->parent_node_id;

	shared_shaper_id = (uint32_t *)malloc(MAX_NUM_SHARED_SHAPERS *
		sizeof(uint32_t));
	if (shared_shaper_id == NULL) {
		printf(" Memory not allocated for shared shapers (error)\n");
		return;
	}

	/* Parse multi shared shaper id string */
	ret = parse_multi_ss_id_str(s_str, &n_shared_shapers, shared_shaper_id);
	if (ret) {
		printf(" Shared shapers params string parse error\n");
		free(shared_shaper_id);
		return;
	}

	if (res->shaper_profile_id < 0)
		np.shaper_profile_id = UINT32_MAX;
	else
		np.shaper_profile_id = res->shaper_profile_id;

	np.n_shared_shapers = n_shared_shapers;

	if (np.n_shared_shapers) {
		np.shared_shaper_id = &shared_shaper_id[0];
	} else {
		free(shared_shaper_id);
		shared_shaper_id = NULL;
	}

	np.leaf.cman = res->cman_mode;
	np.leaf.wred.wred_profile_id = res->wred_profile_id;
	np.stats_mask = res->stats_mask;

	ret = rte_tm_node_add(port_id, res->node_id, parent_node_id,
				res->priority, res->weight, res->level_id,
				&np, &error);
	if (ret != 0) {
		print_err_msg(&error);
		free(shared_shaper_id);
		return;
	}
}

cmdline_parse_inst_t cmd_add_port_tm_leaf_node = {
	.f = cmd_add_port_tm_leaf_node_parsed,
	.data = NULL,
	.help_str = "Add port tm leaf node",
	.tokens = {
		(void *)&cmd_add_port_tm_leaf_node_add,
		(void *)&cmd_add_port_tm_leaf_node_port,
		(void *)&cmd_add_port_tm_leaf_node_tm,
		(void *)&cmd_add_port_tm_leaf_node_nonleaf,
		(void *)&cmd_add_port_tm_leaf_node_node,
		(void *)&cmd_add_port_tm_leaf_node_port_id,
		(void *)&cmd_add_port_tm_leaf_node_node_id,
		(void *)&cmd_add_port_tm_leaf_node_parent_node_id,
		(void *)&cmd_add_port_tm_leaf_node_priority,
		(void *)&cmd_add_port_tm_leaf_node_weight,
		(void *)&cmd_add_port_tm_leaf_node_level_id,
		(void *)&cmd_add_port_tm_leaf_node_shaper_profile_id,
		(void *)&cmd_add_port_tm_leaf_node_cman_mode,
		(void *)&cmd_add_port_tm_leaf_node_wred_profile_id,
		(void *)&cmd_add_port_tm_leaf_node_stats_mask,
		(void *)&cmd_add_port_tm_leaf_node_multi_shared_shaper_id,
		NULL,
	},
};

/* *** Delete Port TM Node *** */
struct cmd_del_port_tm_node_result {
	cmdline_fixed_string_t del;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t tm;
	cmdline_fixed_string_t node;
	uint16_t port_id;
	uint32_t node_id;
};

cmdline_parse_token_string_t cmd_del_port_tm_node_del =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_tm_node_result, del, "del");
cmdline_parse_token_string_t cmd_del_port_tm_node_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_tm_node_result, port, "port");
cmdline_parse_token_string_t cmd_del_port_tm_node_tm =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_tm_node_result, tm, "tm");
cmdline_parse_token_string_t cmd_del_port_tm_node_node =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_tm_node_result, node, "node");
cmdline_parse_token_num_t cmd_del_port_tm_node_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_del_port_tm_node_result,
		 port_id, UINT16);
cmdline_parse_token_num_t cmd_del_port_tm_node_node_id =
	TOKEN_NUM_INITIALIZER(struct cmd_del_port_tm_node_result,
		node_id, UINT32);

static void cmd_del_port_tm_node_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_del_port_tm_node_result *res = parsed_result;
	struct rte_tm_error error;
	uint32_t node_id = res->node_id;
	portid_t port_id = res->port_id;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&error, 0, sizeof(struct rte_tm_error));
	/* Port status */
	if (port_is_started(port_id)) {
		printf(" Port %u not stopped (error)\n", port_id);
		return;
	}

	ret = rte_tm_node_delete(port_id, node_id, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_del_port_tm_node = {
	.f = cmd_del_port_tm_node_parsed,
	.data = NULL,
	.help_str = "Delete port tm node",
	.tokens = {
		(void *)&cmd_del_port_tm_node_del,
		(void *)&cmd_del_port_tm_node_port,
		(void *)&cmd_del_port_tm_node_tm,
		(void *)&cmd_del_port_tm_node_node,
		(void *)&cmd_del_port_tm_node_port_id,
		(void *)&cmd_del_port_tm_node_node_id,
		NULL,
	},
};

/* *** Update Port TM Node Parent *** */
struct cmd_set_port_tm_node_parent_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t tm;
	cmdline_fixed_string_t node;
	cmdline_fixed_string_t parent;
	uint16_t port_id;
	uint32_t node_id;
	uint32_t parent_id;
	uint32_t priority;
	uint32_t weight;
};

cmdline_parse_token_string_t cmd_set_port_tm_node_parent_set =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_tm_node_parent_result, set, "set");
cmdline_parse_token_string_t cmd_set_port_tm_node_parent_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_tm_node_parent_result, port, "port");
cmdline_parse_token_string_t cmd_set_port_tm_node_parent_tm =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_tm_node_parent_result, tm, "tm");
cmdline_parse_token_string_t cmd_set_port_tm_node_parent_node =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_tm_node_parent_result, node, "node");
cmdline_parse_token_string_t cmd_set_port_tm_node_parent_parent =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_tm_node_parent_result, parent, "parent");
cmdline_parse_token_num_t cmd_set_port_tm_node_parent_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_set_port_tm_node_parent_result, port_id, UINT16);
cmdline_parse_token_num_t cmd_set_port_tm_node_parent_node_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_set_port_tm_node_parent_result, node_id, UINT32);
cmdline_parse_token_num_t cmd_set_port_tm_node_parent_parent_id =
	TOKEN_NUM_INITIALIZER(struct cmd_set_port_tm_node_parent_result,
		parent_id, UINT32);
cmdline_parse_token_num_t cmd_set_port_tm_node_parent_priority =
	TOKEN_NUM_INITIALIZER(struct cmd_set_port_tm_node_parent_result,
		priority, UINT32);
cmdline_parse_token_num_t cmd_set_port_tm_node_parent_weight =
	TOKEN_NUM_INITIALIZER(struct cmd_set_port_tm_node_parent_result,
		weight, UINT32);

static void cmd_set_port_tm_node_parent_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_set_port_tm_node_parent_result *res = parsed_result;
	struct rte_tm_error error;
	uint32_t node_id = res->node_id;
	uint32_t parent_id = res->parent_id;
	uint32_t priority = res->priority;
	uint32_t weight = res->weight;
	portid_t port_id = res->port_id;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&error, 0, sizeof(struct rte_tm_error));
	/* Port status */
	if (!port_is_started(port_id)) {
		printf(" Port %u not started (error)\n", port_id);
		return;
	}

	ret = rte_tm_node_parent_update(port_id, node_id,
		parent_id, priority, weight, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_set_port_tm_node_parent = {
	.f = cmd_set_port_tm_node_parent_parsed,
	.data = NULL,
	.help_str = "Set port tm node parent",
	.tokens = {
		(void *)&cmd_set_port_tm_node_parent_set,
		(void *)&cmd_set_port_tm_node_parent_port,
		(void *)&cmd_set_port_tm_node_parent_tm,
		(void *)&cmd_set_port_tm_node_parent_node,
		(void *)&cmd_set_port_tm_node_parent_parent,
		(void *)&cmd_set_port_tm_node_parent_port_id,
		(void *)&cmd_set_port_tm_node_parent_node_id,
		(void *)&cmd_set_port_tm_node_parent_parent_id,
		(void *)&cmd_set_port_tm_node_parent_priority,
		(void *)&cmd_set_port_tm_node_parent_weight,
		NULL,
	},
};

/* *** Suspend Port TM Node *** */
struct cmd_suspend_port_tm_node_result {
	cmdline_fixed_string_t suspend;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t tm;
	cmdline_fixed_string_t node;
	uint16_t port_id;
	uint32_t node_id;
};

cmdline_parse_token_string_t cmd_suspend_port_tm_node_suspend =
	TOKEN_STRING_INITIALIZER(
		struct cmd_suspend_port_tm_node_result, suspend, "suspend");
cmdline_parse_token_string_t cmd_suspend_port_tm_node_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_suspend_port_tm_node_result, port, "port");
cmdline_parse_token_string_t cmd_suspend_port_tm_node_tm =
	TOKEN_STRING_INITIALIZER(
		struct cmd_suspend_port_tm_node_result, tm, "tm");
cmdline_parse_token_string_t cmd_suspend_port_tm_node_node =
	TOKEN_STRING_INITIALIZER(
		struct cmd_suspend_port_tm_node_result, node, "node");
cmdline_parse_token_num_t cmd_suspend_port_tm_node_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_suspend_port_tm_node_result, port_id, UINT16);
cmdline_parse_token_num_t cmd_suspend_port_tm_node_node_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_suspend_port_tm_node_result, node_id, UINT32);

static void cmd_suspend_port_tm_node_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_suspend_port_tm_node_result *res = parsed_result;
	struct rte_tm_error error;
	uint32_t node_id = res->node_id;
	portid_t port_id = res->port_id;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&error, 0, sizeof(struct rte_tm_error));
	ret = rte_tm_node_suspend(port_id, node_id, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_suspend_port_tm_node = {
	.f = cmd_suspend_port_tm_node_parsed,
	.data = NULL,
	.help_str = "Suspend port tm node",
	.tokens = {
		(void *)&cmd_suspend_port_tm_node_suspend,
		(void *)&cmd_suspend_port_tm_node_port,
		(void *)&cmd_suspend_port_tm_node_tm,
		(void *)&cmd_suspend_port_tm_node_node,
		(void *)&cmd_suspend_port_tm_node_port_id,
		(void *)&cmd_suspend_port_tm_node_node_id,
		NULL,
	},
};

/* *** Resume Port TM Node *** */
struct cmd_resume_port_tm_node_result {
	cmdline_fixed_string_t resume;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t tm;
	cmdline_fixed_string_t node;
	uint16_t port_id;
	uint32_t node_id;
};

cmdline_parse_token_string_t cmd_resume_port_tm_node_resume =
	TOKEN_STRING_INITIALIZER(
		struct cmd_resume_port_tm_node_result, resume, "resume");
cmdline_parse_token_string_t cmd_resume_port_tm_node_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_resume_port_tm_node_result, port, "port");
cmdline_parse_token_string_t cmd_resume_port_tm_node_tm =
	TOKEN_STRING_INITIALIZER(
		struct cmd_resume_port_tm_node_result, tm, "tm");
cmdline_parse_token_string_t cmd_resume_port_tm_node_node =
	TOKEN_STRING_INITIALIZER(
		struct cmd_resume_port_tm_node_result, node, "node");
cmdline_parse_token_num_t cmd_resume_port_tm_node_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_resume_port_tm_node_result, port_id, UINT16);
cmdline_parse_token_num_t cmd_resume_port_tm_node_node_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_resume_port_tm_node_result, node_id, UINT32);

static void cmd_resume_port_tm_node_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_resume_port_tm_node_result *res = parsed_result;
	struct rte_tm_error error;
	uint32_t node_id = res->node_id;
	portid_t port_id = res->port_id;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&error, 0, sizeof(struct rte_tm_error));
	ret = rte_tm_node_resume(port_id, node_id, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_resume_port_tm_node = {
	.f = cmd_resume_port_tm_node_parsed,
	.data = NULL,
	.help_str = "Resume port tm node",
	.tokens = {
		(void *)&cmd_resume_port_tm_node_resume,
		(void *)&cmd_resume_port_tm_node_port,
		(void *)&cmd_resume_port_tm_node_tm,
		(void *)&cmd_resume_port_tm_node_node,
		(void *)&cmd_resume_port_tm_node_port_id,
		(void *)&cmd_resume_port_tm_node_node_id,
		NULL,
	},
};

/* *** Port TM Hierarchy Commit *** */
struct cmd_port_tm_hierarchy_commit_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t tm;
	cmdline_fixed_string_t hierarchy;
	cmdline_fixed_string_t commit;
	uint16_t port_id;
	cmdline_fixed_string_t clean_on_fail;
};

cmdline_parse_token_string_t cmd_port_tm_hierarchy_commit_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_port_tm_hierarchy_commit_result, port, "port");
cmdline_parse_token_string_t cmd_port_tm_hierarchy_commit_tm =
	TOKEN_STRING_INITIALIZER(
		struct cmd_port_tm_hierarchy_commit_result, tm, "tm");
cmdline_parse_token_string_t cmd_port_tm_hierarchy_commit_hierarchy =
	TOKEN_STRING_INITIALIZER(
		struct cmd_port_tm_hierarchy_commit_result,
			hierarchy, "hierarchy");
cmdline_parse_token_string_t cmd_port_tm_hierarchy_commit_commit =
	TOKEN_STRING_INITIALIZER(
		struct cmd_port_tm_hierarchy_commit_result, commit, "commit");
cmdline_parse_token_num_t cmd_port_tm_hierarchy_commit_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_port_tm_hierarchy_commit_result,
			port_id, UINT16);
cmdline_parse_token_string_t cmd_port_tm_hierarchy_commit_clean_on_fail =
	TOKEN_STRING_INITIALIZER(struct cmd_port_tm_hierarchy_commit_result,
		 clean_on_fail, "yes#no");

static void cmd_port_tm_hierarchy_commit_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_port_tm_hierarchy_commit_result *res = parsed_result;
	struct rte_tm_error error;
	uint32_t clean_on_fail;
	portid_t port_id = res->port_id;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	if (strcmp(res->clean_on_fail, "yes") == 0)
		clean_on_fail = 1;
	else
		clean_on_fail = 0;

	memset(&error, 0, sizeof(struct rte_tm_error));
	ret = rte_tm_hierarchy_commit(port_id, clean_on_fail, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_port_tm_hierarchy_commit = {
	.f = cmd_port_tm_hierarchy_commit_parsed,
	.data = NULL,
	.help_str = "Commit port tm hierarchy",
	.tokens = {
		(void *)&cmd_port_tm_hierarchy_commit_port,
		(void *)&cmd_port_tm_hierarchy_commit_tm,
		(void *)&cmd_port_tm_hierarchy_commit_hierarchy,
		(void *)&cmd_port_tm_hierarchy_commit_commit,
		(void *)&cmd_port_tm_hierarchy_commit_port_id,
		(void *)&cmd_port_tm_hierarchy_commit_clean_on_fail,
		NULL,
	},
};

/* *** Port TM Mark IP ECN *** */
struct cmd_port_tm_mark_ip_ecn_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t tm;
	cmdline_fixed_string_t mark;
	cmdline_fixed_string_t ip_ecn;
	uint16_t port_id;
	uint16_t green;
	uint16_t yellow;
	uint16_t red;
};

cmdline_parse_token_string_t cmd_port_tm_mark_ip_ecn_set =
	TOKEN_STRING_INITIALIZER(struct cmd_port_tm_mark_ip_ecn_result,
				 set, "set");

cmdline_parse_token_string_t cmd_port_tm_mark_ip_ecn_port =
	TOKEN_STRING_INITIALIZER(struct cmd_port_tm_mark_ip_ecn_result,
				 port, "port");

cmdline_parse_token_string_t cmd_port_tm_mark_ip_ecn_tm =
	TOKEN_STRING_INITIALIZER(struct cmd_port_tm_mark_ip_ecn_result, tm,
				 "tm");

cmdline_parse_token_string_t cmd_port_tm_mark_ip_ecn_mark =
	TOKEN_STRING_INITIALIZER(struct cmd_port_tm_mark_ip_ecn_result,
				 mark, "mark");

cmdline_parse_token_string_t cmd_port_tm_mark_ip_ecn_ip_ecn =
	TOKEN_STRING_INITIALIZER(struct cmd_port_tm_mark_ip_ecn_result,
				 ip_ecn, "ip_ecn");
cmdline_parse_token_num_t cmd_port_tm_mark_ip_ecn_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_port_tm_mark_ip_ecn_result,
			      port_id, UINT16);

cmdline_parse_token_num_t cmd_port_tm_mark_ip_ecn_green =
	TOKEN_NUM_INITIALIZER(struct cmd_port_tm_mark_ip_ecn_result,
			      green, UINT16);
cmdline_parse_token_num_t cmd_port_tm_mark_ip_ecn_yellow =
	TOKEN_NUM_INITIALIZER(struct cmd_port_tm_mark_ip_ecn_result,
			      yellow, UINT16);
cmdline_parse_token_num_t cmd_port_tm_mark_ip_ecn_red =
	TOKEN_NUM_INITIALIZER(struct cmd_port_tm_mark_ip_ecn_result,
				red, UINT16);

static void cmd_port_tm_mark_ip_ecn_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_port_tm_mark_ip_ecn_result *res = parsed_result;
	struct rte_tm_error error;
	portid_t port_id = res->port_id;
	int green = res->green;
	int yellow = res->yellow;
	int red = res->red;
	int ret;
	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&error, 0, sizeof(struct rte_tm_error));
	ret = rte_tm_mark_ip_ecn(port_id, green, yellow, red, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_port_tm_mark_ip_ecn = {
	.f = cmd_port_tm_mark_ip_ecn_parsed,
	.data = NULL,
	.help_str = "set port tm mark ip_ecn <port> <green> <yellow> <red>",
	.tokens = {
		(void *)&cmd_port_tm_mark_ip_ecn_set,
		(void *)&cmd_port_tm_mark_ip_ecn_port,
		(void *)&cmd_port_tm_mark_ip_ecn_tm,
		(void *)&cmd_port_tm_mark_ip_ecn_mark,
		(void *)&cmd_port_tm_mark_ip_ecn_ip_ecn,
		(void *)&cmd_port_tm_mark_ip_ecn_port_id,
		(void *)&cmd_port_tm_mark_ip_ecn_green,
		(void *)&cmd_port_tm_mark_ip_ecn_yellow,
		(void *)&cmd_port_tm_mark_ip_ecn_red,
		NULL,
	},
};


/* *** Port TM Mark IP DSCP *** */
struct cmd_port_tm_mark_ip_dscp_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t tm;
	cmdline_fixed_string_t mark;
	cmdline_fixed_string_t ip_dscp;
	uint16_t port_id;
	uint16_t green;
	uint16_t yellow;
	uint16_t red;
};

cmdline_parse_token_string_t cmd_port_tm_mark_ip_dscp_set =
	TOKEN_STRING_INITIALIZER(struct cmd_port_tm_mark_ip_dscp_result,
				 set, "set");

cmdline_parse_token_string_t cmd_port_tm_mark_ip_dscp_port =
	TOKEN_STRING_INITIALIZER(struct cmd_port_tm_mark_ip_dscp_result,
				 port, "port");

cmdline_parse_token_string_t cmd_port_tm_mark_ip_dscp_tm =
	TOKEN_STRING_INITIALIZER(struct cmd_port_tm_mark_ip_dscp_result, tm,
				 "tm");

cmdline_parse_token_string_t cmd_port_tm_mark_ip_dscp_mark =
	TOKEN_STRING_INITIALIZER(struct cmd_port_tm_mark_ip_dscp_result,
				 mark, "mark");

cmdline_parse_token_string_t cmd_port_tm_mark_ip_dscp_ip_dscp =
	TOKEN_STRING_INITIALIZER(struct cmd_port_tm_mark_ip_dscp_result,
				 ip_dscp, "ip_dscp");
cmdline_parse_token_num_t cmd_port_tm_mark_ip_dscp_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_port_tm_mark_ip_dscp_result,
			      port_id, UINT16);

cmdline_parse_token_num_t cmd_port_tm_mark_ip_dscp_green =
	TOKEN_NUM_INITIALIZER(struct cmd_port_tm_mark_ip_dscp_result,
				green, UINT16);
cmdline_parse_token_num_t cmd_port_tm_mark_ip_dscp_yellow =
	TOKEN_NUM_INITIALIZER(struct cmd_port_tm_mark_ip_dscp_result,
				yellow, UINT16);
cmdline_parse_token_num_t cmd_port_tm_mark_ip_dscp_red =
	TOKEN_NUM_INITIALIZER(struct cmd_port_tm_mark_ip_dscp_result,
				red, UINT16);

static void cmd_port_tm_mark_ip_dscp_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_port_tm_mark_ip_dscp_result *res = parsed_result;
	struct rte_tm_error error;
	portid_t port_id = res->port_id;
	int green = res->green;
	int yellow = res->yellow;
	int red = res->red;
	int ret;
	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&error, 0, sizeof(struct rte_tm_error));
	ret = rte_tm_mark_ip_dscp(port_id, green, yellow, red, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_port_tm_mark_ip_dscp = {
	.f = cmd_port_tm_mark_ip_dscp_parsed,
	.data = NULL,
	.help_str = "set port tm mark ip_dscp <port> <green> <yellow> <red>",
	.tokens = {
		(void *)&cmd_port_tm_mark_ip_dscp_set,
		(void *)&cmd_port_tm_mark_ip_dscp_port,
		(void *)&cmd_port_tm_mark_ip_dscp_tm,
		(void *)&cmd_port_tm_mark_ip_dscp_mark,
		(void *)&cmd_port_tm_mark_ip_dscp_ip_dscp,
		(void *)&cmd_port_tm_mark_ip_dscp_port_id,
		(void *)&cmd_port_tm_mark_ip_dscp_green,
		(void *)&cmd_port_tm_mark_ip_dscp_yellow,
		(void *)&cmd_port_tm_mark_ip_dscp_red,
		NULL,
	},
};


/* *** Port TM Mark VLAN_DEI *** */
struct cmd_port_tm_mark_vlan_dei_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t tm;
	cmdline_fixed_string_t mark;
	cmdline_fixed_string_t vlan_dei;
	uint16_t port_id;
	uint16_t green;
	uint16_t yellow;
	uint16_t red;
};

cmdline_parse_token_string_t cmd_port_tm_mark_vlan_dei_set =
	TOKEN_STRING_INITIALIZER(struct cmd_port_tm_mark_vlan_dei_result,
				 set, "set");

cmdline_parse_token_string_t cmd_port_tm_mark_vlan_dei_port =
	TOKEN_STRING_INITIALIZER(struct cmd_port_tm_mark_vlan_dei_result,
				 port, "port");

cmdline_parse_token_string_t cmd_port_tm_mark_vlan_dei_tm =
	TOKEN_STRING_INITIALIZER(struct cmd_port_tm_mark_vlan_dei_result, tm,
				 "tm");

cmdline_parse_token_string_t cmd_port_tm_mark_vlan_dei_mark =
	TOKEN_STRING_INITIALIZER(struct cmd_port_tm_mark_vlan_dei_result,
				 mark, "mark");

cmdline_parse_token_string_t cmd_port_tm_mark_vlan_dei_vlan_dei =
	TOKEN_STRING_INITIALIZER(struct cmd_port_tm_mark_vlan_dei_result,
				 vlan_dei, "vlan_dei");
cmdline_parse_token_num_t cmd_port_tm_mark_vlan_dei_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_port_tm_mark_vlan_dei_result,
			      port_id, UINT16);

cmdline_parse_token_num_t cmd_port_tm_mark_vlan_dei_green =
	TOKEN_NUM_INITIALIZER(struct cmd_port_tm_mark_vlan_dei_result,
				green, UINT16);
cmdline_parse_token_num_t cmd_port_tm_mark_vlan_dei_yellow =
	TOKEN_NUM_INITIALIZER(struct cmd_port_tm_mark_vlan_dei_result,
				yellow, UINT16);
cmdline_parse_token_num_t cmd_port_tm_mark_vlan_dei_red =
	TOKEN_NUM_INITIALIZER(struct cmd_port_tm_mark_vlan_dei_result,
				red, UINT16);

static void cmd_port_tm_mark_vlan_dei_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_port_tm_mark_vlan_dei_result *res = parsed_result;
	struct rte_tm_error error;
	portid_t port_id = res->port_id;
	int green = res->green;
	int yellow = res->yellow;
	int red = res->red;
	int ret;
	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&error, 0, sizeof(struct rte_tm_error));
	ret = rte_tm_mark_vlan_dei(port_id, green, yellow, red, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_port_tm_mark_vlan_dei = {
	.f = cmd_port_tm_mark_vlan_dei_parsed,
	.data = NULL,
	.help_str = "set port tm mark vlan_dei <port> <green> <yellow> <red>",
	.tokens = {
		(void *)&cmd_port_tm_mark_vlan_dei_set,
		(void *)&cmd_port_tm_mark_vlan_dei_port,
		(void *)&cmd_port_tm_mark_vlan_dei_tm,
		(void *)&cmd_port_tm_mark_vlan_dei_mark,
		(void *)&cmd_port_tm_mark_vlan_dei_vlan_dei,
		(void *)&cmd_port_tm_mark_vlan_dei_port_id,
		(void *)&cmd_port_tm_mark_vlan_dei_green,
		(void *)&cmd_port_tm_mark_vlan_dei_yellow,
		(void *)&cmd_port_tm_mark_vlan_dei_red,
		NULL,
	},
};
