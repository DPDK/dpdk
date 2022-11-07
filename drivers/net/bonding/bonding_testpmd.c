/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation.
 */

#include <rte_eth_bond.h>
#include <rte_eth_bond_8023ad.h>

#include <cmdline_parse.h>
#include <cmdline_parse_etheraddr.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>

#include "testpmd.h"

/* *** SET BONDING MODE *** */
struct cmd_set_bonding_mode_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t mode;
	uint8_t value;
	portid_t port_id;
};

static void cmd_set_bonding_mode_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_set_bonding_mode_result *res = parsed_result;
	portid_t port_id = res->port_id;
	struct rte_port *port = &ports[port_id];

	/*
	 * Bonding mode changed means resources of device changed, like whether
	 * started rte timer or not. Device should be restarted when resources
	 * of device changed.
	 */
	if (port->port_status != RTE_PORT_STOPPED) {
		fprintf(stderr,
			"\t Error: Can't set bonding mode when port %d is not stopped\n",
			port_id);
		return;
	}

	/* Set the bonding mode for the relevant port. */
	if (rte_eth_bond_mode_set(port_id, res->value) != 0)
		fprintf(stderr, "\t Failed to set bonding mode for port = %d.\n",
			port_id);
}

static cmdline_parse_token_string_t cmd_setbonding_mode_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_mode_result,
		set, "set");
static cmdline_parse_token_string_t cmd_setbonding_mode_bonding =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_mode_result,
		bonding, "bonding");
static cmdline_parse_token_string_t cmd_setbonding_mode_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_mode_result,
		mode, "mode");
static cmdline_parse_token_num_t cmd_setbonding_mode_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_bonding_mode_result,
		value, RTE_UINT8);
static cmdline_parse_token_num_t cmd_setbonding_mode_port =
	TOKEN_NUM_INITIALIZER(struct cmd_set_bonding_mode_result,
		port_id, RTE_UINT16);

static cmdline_parse_inst_t cmd_set_bonding_mode = {
	.f = cmd_set_bonding_mode_parsed,
	.help_str = "set bonding mode <mode_value> <port_id>: "
		"Set the bonding mode for port_id",
	.data = NULL,
	.tokens = {
		(void *)&cmd_setbonding_mode_set,
		(void *)&cmd_setbonding_mode_bonding,
		(void *)&cmd_setbonding_mode_mode,
		(void *)&cmd_setbonding_mode_value,
		(void *)&cmd_setbonding_mode_port,
		NULL
	}
};

/* *** SET BONDING SLOW_QUEUE SW/HW *** */
struct cmd_set_bonding_lacp_dedicated_queues_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t lacp;
	cmdline_fixed_string_t dedicated_queues;
	portid_t port_id;
	cmdline_fixed_string_t mode;
};

static void cmd_set_bonding_lacp_dedicated_queues_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_set_bonding_lacp_dedicated_queues_result *res = parsed_result;
	portid_t port_id = res->port_id;
	struct rte_port *port;

	port = &ports[port_id];

	/** Check if the port is not started **/
	if (port->port_status != RTE_PORT_STOPPED) {
		fprintf(stderr, "Please stop port %d first\n", port_id);
		return;
	}

	if (!strcmp(res->mode, "enable")) {
		if (rte_eth_bond_8023ad_dedicated_queues_enable(port_id) == 0)
			printf("Dedicate queues for LACP control packets"
					" enabled\n");
		else
			printf("Enabling dedicate queues for LACP control "
					"packets on port %d failed\n", port_id);
	} else if (!strcmp(res->mode, "disable")) {
		if (rte_eth_bond_8023ad_dedicated_queues_disable(port_id) == 0)
			printf("Dedicated queues for LACP control packets "
					"disabled\n");
		else
			printf("Disabling dedicated queues for LACP control "
					"traffic on port %d failed\n", port_id);
	}
}

static cmdline_parse_token_string_t cmd_setbonding_lacp_dedicated_queues_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_lacp_dedicated_queues_result,
		set, "set");
static cmdline_parse_token_string_t cmd_setbonding_lacp_dedicated_queues_bonding =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_lacp_dedicated_queues_result,
		bonding, "bonding");
static cmdline_parse_token_string_t cmd_setbonding_lacp_dedicated_queues_lacp =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_lacp_dedicated_queues_result,
		lacp, "lacp");
static cmdline_parse_token_string_t cmd_setbonding_lacp_dedicated_queues_dedicated_queues =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_lacp_dedicated_queues_result,
		dedicated_queues, "dedicated_queues");
static cmdline_parse_token_num_t cmd_setbonding_lacp_dedicated_queues_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_set_bonding_lacp_dedicated_queues_result,
		port_id, RTE_UINT16);
static cmdline_parse_token_string_t cmd_setbonding_lacp_dedicated_queues_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_lacp_dedicated_queues_result,
		mode, "enable#disable");

static cmdline_parse_inst_t cmd_set_lacp_dedicated_queues = {
	.f = cmd_set_bonding_lacp_dedicated_queues_parsed,
	.help_str = "set bonding lacp dedicated_queues <port_id> "
		"enable|disable: "
		"Enable/disable dedicated queues for LACP control traffic for port_id",
	.data = NULL,
	.tokens = {
		(void *)&cmd_setbonding_lacp_dedicated_queues_set,
		(void *)&cmd_setbonding_lacp_dedicated_queues_bonding,
		(void *)&cmd_setbonding_lacp_dedicated_queues_lacp,
		(void *)&cmd_setbonding_lacp_dedicated_queues_dedicated_queues,
		(void *)&cmd_setbonding_lacp_dedicated_queues_port_id,
		(void *)&cmd_setbonding_lacp_dedicated_queues_mode,
		NULL
	}
};

/* *** SET BALANCE XMIT POLICY *** */
struct cmd_set_bonding_balance_xmit_policy_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t balance_xmit_policy;
	portid_t port_id;
	cmdline_fixed_string_t policy;
};

static void cmd_set_bonding_balance_xmit_policy_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_set_bonding_balance_xmit_policy_result *res = parsed_result;
	portid_t port_id = res->port_id;
	uint8_t policy;

	if (!strcmp(res->policy, "l2")) {
		policy = BALANCE_XMIT_POLICY_LAYER2;
	} else if (!strcmp(res->policy, "l23")) {
		policy = BALANCE_XMIT_POLICY_LAYER23;
	} else if (!strcmp(res->policy, "l34")) {
		policy = BALANCE_XMIT_POLICY_LAYER34;
	} else {
		fprintf(stderr, "\t Invalid xmit policy selection");
		return;
	}

	/* Set the bonding mode for the relevant port. */
	if (rte_eth_bond_xmit_policy_set(port_id, policy) != 0) {
		fprintf(stderr,
			"\t Failed to set bonding balance xmit policy for port = %d.\n",
			port_id);
	}
}

static cmdline_parse_token_string_t cmd_setbonding_balance_xmit_policy_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_balance_xmit_policy_result,
		set, "set");
static cmdline_parse_token_string_t cmd_setbonding_balance_xmit_policy_bonding =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_balance_xmit_policy_result,
		bonding, "bonding");
static cmdline_parse_token_string_t cmd_setbonding_balance_xmit_policy_balance_xmit_policy =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_balance_xmit_policy_result,
		balance_xmit_policy, "balance_xmit_policy");
static cmdline_parse_token_num_t cmd_setbonding_balance_xmit_policy_port =
	TOKEN_NUM_INITIALIZER(struct cmd_set_bonding_balance_xmit_policy_result,
		port_id, RTE_UINT16);
static cmdline_parse_token_string_t cmd_setbonding_balance_xmit_policy_policy =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_balance_xmit_policy_result,
		policy, "l2#l23#l34");

static cmdline_parse_inst_t cmd_set_balance_xmit_policy = {
	.f = cmd_set_bonding_balance_xmit_policy_parsed,
	.help_str = "set bonding balance_xmit_policy <port_id> "
		"l2|l23|l34: "
		"Set the bonding balance_xmit_policy for port_id",
	.data = NULL,
	.tokens = {
		(void *)&cmd_setbonding_balance_xmit_policy_set,
		(void *)&cmd_setbonding_balance_xmit_policy_bonding,
		(void *)&cmd_setbonding_balance_xmit_policy_balance_xmit_policy,
		(void *)&cmd_setbonding_balance_xmit_policy_port,
		(void *)&cmd_setbonding_balance_xmit_policy_policy,
		NULL
	}
};

/* *** SHOW IEEE802.3 BONDING INFORMATION *** */
struct cmd_show_bonding_lacp_info_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t lacp;
	cmdline_fixed_string_t info;
	portid_t port_id;
};

static void port_param_show(struct port_params *params)
{
	char buf[RTE_ETHER_ADDR_FMT_SIZE];

	printf("\t\tsystem priority: %u\n", params->system_priority);
	rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, &params->system);
	printf("\t\tsystem mac address: %s\n", buf);
	printf("\t\tport key: %u\n", params->key);
	printf("\t\tport priority: %u\n", params->port_priority);
	printf("\t\tport number: %u\n", params->port_number);
}

static void lacp_slave_info_show(struct rte_eth_bond_8023ad_slave_info *info)
{
	char a_state[256] = { 0 };
	char p_state[256] = { 0 };
	int a_len = 0;
	int p_len = 0;
	uint32_t i;

	static const char * const state[] = {
		"ACTIVE",
		"TIMEOUT",
		"AGGREGATION",
		"SYNCHRONIZATION",
		"COLLECTING",
		"DISTRIBUTING",
		"DEFAULTED",
		"EXPIRED"
	};
	static const char * const selection[] = {
		"UNSELECTED",
		"STANDBY",
		"SELECTED"
	};

	for (i = 0; i < RTE_DIM(state); i++) {
		if ((info->actor_state >> i) & 1)
			a_len += snprintf(&a_state[a_len],
						RTE_DIM(a_state) - a_len, "%s ",
						state[i]);

		if ((info->partner_state >> i) & 1)
			p_len += snprintf(&p_state[p_len],
						RTE_DIM(p_state) - p_len, "%s ",
						state[i]);
	}
	printf("\tAggregator port id: %u\n", info->agg_port_id);
	printf("\tselection: %s\n", selection[info->selected]);
	printf("\tActor detail info:\n");
	port_param_show(&info->actor);
	printf("\t\tport state: %s\n", a_state);
	printf("\tPartner detail info:\n");
	port_param_show(&info->partner);
	printf("\t\tport state: %s\n", p_state);
	printf("\n");
}

static void lacp_conf_show(struct rte_eth_bond_8023ad_conf *conf)
{
	printf("\tfast period: %u ms\n", conf->fast_periodic_ms);
	printf("\tslow period: %u ms\n", conf->slow_periodic_ms);
	printf("\tshort timeout: %u ms\n", conf->short_timeout_ms);
	printf("\tlong timeout: %u ms\n", conf->long_timeout_ms);
	printf("\taggregate wait timeout: %u ms\n",
			conf->aggregate_wait_timeout_ms);
	printf("\ttx period: %u ms\n", conf->tx_period_ms);
	printf("\trx marker period: %u ms\n", conf->rx_marker_period_ms);
	printf("\tupdate timeout: %u ms\n", conf->update_timeout_ms);
	switch (conf->agg_selection) {
	case AGG_BANDWIDTH:
		printf("\taggregation mode: bandwidth\n");
		break;
	case AGG_STABLE:
		printf("\taggregation mode: stable\n");
		break;
	case AGG_COUNT:
		printf("\taggregation mode: count\n");
		break;
	default:
		printf("\taggregation mode: invalid\n");
		break;
	}

	printf("\n");
}

static void cmd_show_bonding_lacp_info_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_show_bonding_lacp_info_result *res = parsed_result;
	struct rte_eth_bond_8023ad_slave_info slave_info;
	struct rte_eth_bond_8023ad_conf port_conf;
	portid_t slaves[RTE_MAX_ETHPORTS];
	portid_t port_id = res->port_id;
	int num_active_slaves;
	int bonding_mode;
	int i;
	int ret;

	bonding_mode = rte_eth_bond_mode_get(port_id);
	if (bonding_mode != BONDING_MODE_8023AD) {
		fprintf(stderr, "\tBonding mode is not mode 4\n");
		return;
	}

	num_active_slaves = rte_eth_bond_active_slaves_get(port_id, slaves,
			RTE_MAX_ETHPORTS);
	if (num_active_slaves < 0) {
		fprintf(stderr, "\tFailed to get active slave list for port = %u\n",
				port_id);
		return;
	}
	if (num_active_slaves == 0)
		fprintf(stderr, "\tIEEE802.3 port %u has no active slave\n",
			port_id);

	printf("\tIEEE802.3 port: %u\n", port_id);
	ret = rte_eth_bond_8023ad_conf_get(port_id, &port_conf);
	if (ret) {
		fprintf(stderr, "\tGet bonded device %u info failed\n",
			port_id);
		return;
	}
	lacp_conf_show(&port_conf);

	for (i = 0; i < num_active_slaves; i++) {
		ret = rte_eth_bond_8023ad_slave_info(port_id, slaves[i],
				&slave_info);
		if (ret) {
			fprintf(stderr, "\tGet slave device %u info failed\n",
				slaves[i]);
			return;
		}
		printf("\tSlave Port: %u\n", slaves[i]);
		lacp_slave_info_show(&slave_info);
	}
}

static cmdline_parse_token_string_t cmd_show_bonding_lacp_info_show =
	TOKEN_STRING_INITIALIZER(struct cmd_show_bonding_lacp_info_result,
		show, "show");
static cmdline_parse_token_string_t cmd_show_bonding_lacp_info_bonding =
	TOKEN_STRING_INITIALIZER(struct cmd_show_bonding_lacp_info_result,
		bonding, "bonding");
static cmdline_parse_token_string_t cmd_show_bonding_lacp_info_lacp =
	TOKEN_STRING_INITIALIZER(struct cmd_show_bonding_lacp_info_result,
		bonding, "lacp");
static cmdline_parse_token_string_t cmd_show_bonding_lacp_info_info =
	TOKEN_STRING_INITIALIZER(struct cmd_show_bonding_lacp_info_result,
		info, "info");
static cmdline_parse_token_num_t cmd_show_bonding_lacp_info_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_show_bonding_lacp_info_result,
		port_id, RTE_UINT16);

static cmdline_parse_inst_t cmd_show_bonding_lacp_info = {
	.f = cmd_show_bonding_lacp_info_parsed,
	.help_str = "show bonding lacp info <port_id> : "
		"Show bonding IEEE802.3 information for port_id",
	.data = NULL,
	.tokens = {
		(void *)&cmd_show_bonding_lacp_info_show,
		(void *)&cmd_show_bonding_lacp_info_bonding,
		(void *)&cmd_show_bonding_lacp_info_lacp,
		(void *)&cmd_show_bonding_lacp_info_info,
		(void *)&cmd_show_bonding_lacp_info_port_id,
		NULL
	}
};

/* *** SHOW NIC BONDING CONFIGURATION *** */
struct cmd_show_bonding_config_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t config;
	portid_t port_id;
};

static void cmd_show_bonding_config_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_show_bonding_config_result *res = parsed_result;
	int bonding_mode, agg_mode;
	portid_t slaves[RTE_MAX_ETHPORTS];
	int num_slaves, num_active_slaves;
	int primary_id;
	int i;
	portid_t port_id = res->port_id;

	/* Display the bonding mode.*/
	bonding_mode = rte_eth_bond_mode_get(port_id);
	if (bonding_mode < 0) {
		fprintf(stderr, "\tFailed to get bonding mode for port = %d\n",
			port_id);
		return;
	}
	printf("\tBonding mode: %d\n", bonding_mode);

	if (bonding_mode == BONDING_MODE_BALANCE ||
		bonding_mode == BONDING_MODE_8023AD) {
		int balance_xmit_policy;

		balance_xmit_policy = rte_eth_bond_xmit_policy_get(port_id);
		if (balance_xmit_policy < 0) {
			fprintf(stderr,
				"\tFailed to get balance xmit policy for port = %d\n",
				port_id);
			return;
		}
		printf("\tBalance Xmit Policy: ");

		switch (balance_xmit_policy) {
		case BALANCE_XMIT_POLICY_LAYER2:
			printf("BALANCE_XMIT_POLICY_LAYER2");
			break;
		case BALANCE_XMIT_POLICY_LAYER23:
			printf("BALANCE_XMIT_POLICY_LAYER23");
			break;
		case BALANCE_XMIT_POLICY_LAYER34:
			printf("BALANCE_XMIT_POLICY_LAYER34");
			break;
		}
		printf("\n");
	}

	if (bonding_mode == BONDING_MODE_8023AD) {
		agg_mode = rte_eth_bond_8023ad_agg_selection_get(port_id);
		printf("\tIEEE802.3AD Aggregator Mode: ");
		switch (agg_mode) {
		case AGG_BANDWIDTH:
			printf("bandwidth");
			break;
		case AGG_STABLE:
			printf("stable");
			break;
		case AGG_COUNT:
			printf("count");
			break;
		}
		printf("\n");
	}

	num_slaves = rte_eth_bond_slaves_get(port_id, slaves, RTE_MAX_ETHPORTS);

	if (num_slaves < 0) {
		fprintf(stderr, "\tFailed to get slave list for port = %d\n",
			port_id);
		return;
	}
	if (num_slaves > 0) {
		printf("\tSlaves (%d): [", num_slaves);
		for (i = 0; i < num_slaves - 1; i++)
			printf("%d ", slaves[i]);

		printf("%d]\n", slaves[num_slaves - 1]);
	} else {
		printf("\tSlaves: []\n");
	}

	num_active_slaves = rte_eth_bond_active_slaves_get(port_id, slaves,
			RTE_MAX_ETHPORTS);

	if (num_active_slaves < 0) {
		fprintf(stderr,
			"\tFailed to get active slave list for port = %d\n",
			port_id);
		return;
	}
	if (num_active_slaves > 0) {
		printf("\tActive Slaves (%d): [", num_active_slaves);
		for (i = 0; i < num_active_slaves - 1; i++)
			printf("%d ", slaves[i]);

		printf("%d]\n", slaves[num_active_slaves - 1]);

	} else {
		printf("\tActive Slaves: []\n");
	}

	primary_id = rte_eth_bond_primary_get(port_id);
	if (primary_id < 0) {
		fprintf(stderr, "\tFailed to get primary slave for port = %d\n",
			port_id);
		return;
	}
	printf("\tPrimary: [%d]\n", primary_id);
}

static cmdline_parse_token_string_t cmd_showbonding_config_show =
	TOKEN_STRING_INITIALIZER(struct cmd_show_bonding_config_result,
		show, "show");
static cmdline_parse_token_string_t cmd_showbonding_config_bonding =
	TOKEN_STRING_INITIALIZER(struct cmd_show_bonding_config_result,
		bonding, "bonding");
static cmdline_parse_token_string_t cmd_showbonding_config_config =
	TOKEN_STRING_INITIALIZER(struct cmd_show_bonding_config_result,
		config, "config");
static cmdline_parse_token_num_t cmd_showbonding_config_port =
	TOKEN_NUM_INITIALIZER(struct cmd_show_bonding_config_result,
		port_id, RTE_UINT16);

static cmdline_parse_inst_t cmd_show_bonding_config = {
	.f = cmd_show_bonding_config_parsed,
	.help_str = "show bonding config <port_id>: "
		"Show the bonding config for port_id",
	.data = NULL,
	.tokens = {
		(void *)&cmd_showbonding_config_show,
		(void *)&cmd_showbonding_config_bonding,
		(void *)&cmd_showbonding_config_config,
		(void *)&cmd_showbonding_config_port,
		NULL
	}
};

/* *** SET BONDING PRIMARY *** */
struct cmd_set_bonding_primary_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t primary;
	portid_t slave_id;
	portid_t port_id;
};

static void cmd_set_bonding_primary_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_set_bonding_primary_result *res = parsed_result;
	portid_t master_port_id = res->port_id;
	portid_t slave_port_id = res->slave_id;

	/* Set the primary slave for a bonded device. */
	if (rte_eth_bond_primary_set(master_port_id, slave_port_id) != 0) {
		fprintf(stderr, "\t Failed to set primary slave for port = %d.\n",
			master_port_id);
		return;
	}
	init_port_config();
}

static cmdline_parse_token_string_t cmd_setbonding_primary_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_primary_result,
		set, "set");
static cmdline_parse_token_string_t cmd_setbonding_primary_bonding =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_primary_result,
		bonding, "bonding");
static cmdline_parse_token_string_t cmd_setbonding_primary_primary =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_primary_result,
		primary, "primary");
static cmdline_parse_token_num_t cmd_setbonding_primary_slave =
	TOKEN_NUM_INITIALIZER(struct cmd_set_bonding_primary_result,
		slave_id, RTE_UINT16);
static cmdline_parse_token_num_t cmd_setbonding_primary_port =
	TOKEN_NUM_INITIALIZER(struct cmd_set_bonding_primary_result,
		port_id, RTE_UINT16);

static cmdline_parse_inst_t cmd_set_bonding_primary = {
	.f = cmd_set_bonding_primary_parsed,
	.help_str = "set bonding primary <slave_id> <port_id>: "
		"Set the primary slave for port_id",
	.data = NULL,
	.tokens = {
		(void *)&cmd_setbonding_primary_set,
		(void *)&cmd_setbonding_primary_bonding,
		(void *)&cmd_setbonding_primary_primary,
		(void *)&cmd_setbonding_primary_slave,
		(void *)&cmd_setbonding_primary_port,
		NULL
	}
};

/* *** ADD SLAVE *** */
struct cmd_add_bonding_slave_result {
	cmdline_fixed_string_t add;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t slave;
	portid_t slave_id;
	portid_t port_id;
};

static void cmd_add_bonding_slave_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_add_bonding_slave_result *res = parsed_result;
	portid_t master_port_id = res->port_id;
	portid_t slave_port_id = res->slave_id;

	/* add the slave for a bonded device. */
	if (rte_eth_bond_slave_add(master_port_id, slave_port_id) != 0) {
		fprintf(stderr,
			"\t Failed to add slave %d to master port = %d.\n",
			slave_port_id, master_port_id);
		return;
	}
	ports[master_port_id].update_conf = 1;
	init_port_config();
	set_port_slave_flag(slave_port_id);
}

static cmdline_parse_token_string_t cmd_addbonding_slave_add =
	TOKEN_STRING_INITIALIZER(struct cmd_add_bonding_slave_result,
		add, "add");
static cmdline_parse_token_string_t cmd_addbonding_slave_bonding =
	TOKEN_STRING_INITIALIZER(struct cmd_add_bonding_slave_result,
		bonding, "bonding");
static cmdline_parse_token_string_t cmd_addbonding_slave_slave =
	TOKEN_STRING_INITIALIZER(struct cmd_add_bonding_slave_result,
		slave, "slave");
static cmdline_parse_token_num_t cmd_addbonding_slave_slaveid =
	TOKEN_NUM_INITIALIZER(struct cmd_add_bonding_slave_result,
		slave_id, RTE_UINT16);
static cmdline_parse_token_num_t cmd_addbonding_slave_port =
	TOKEN_NUM_INITIALIZER(struct cmd_add_bonding_slave_result,
		port_id, RTE_UINT16);

static cmdline_parse_inst_t cmd_add_bonding_slave = {
	.f = cmd_add_bonding_slave_parsed,
	.help_str = "add bonding slave <slave_id> <port_id>: "
		"Add a slave device to a bonded device",
	.data = NULL,
	.tokens = {
		(void *)&cmd_addbonding_slave_add,
		(void *)&cmd_addbonding_slave_bonding,
		(void *)&cmd_addbonding_slave_slave,
		(void *)&cmd_addbonding_slave_slaveid,
		(void *)&cmd_addbonding_slave_port,
		NULL
	}
};

/* *** REMOVE SLAVE *** */
struct cmd_remove_bonding_slave_result {
	cmdline_fixed_string_t remove;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t slave;
	portid_t slave_id;
	portid_t port_id;
};

static void cmd_remove_bonding_slave_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_remove_bonding_slave_result *res = parsed_result;
	portid_t master_port_id = res->port_id;
	portid_t slave_port_id = res->slave_id;

	/* remove the slave from a bonded device. */
	if (rte_eth_bond_slave_remove(master_port_id, slave_port_id) != 0) {
		fprintf(stderr,
			"\t Failed to remove slave %d from master port = %d.\n",
			slave_port_id, master_port_id);
		return;
	}
	init_port_config();
	clear_port_slave_flag(slave_port_id);
}

static cmdline_parse_token_string_t cmd_removebonding_slave_remove =
	TOKEN_STRING_INITIALIZER(struct cmd_remove_bonding_slave_result,
		remove, "remove");
static cmdline_parse_token_string_t cmd_removebonding_slave_bonding =
	TOKEN_STRING_INITIALIZER(struct cmd_remove_bonding_slave_result,
		bonding, "bonding");
static cmdline_parse_token_string_t cmd_removebonding_slave_slave =
	TOKEN_STRING_INITIALIZER(struct cmd_remove_bonding_slave_result,
		slave, "slave");
static cmdline_parse_token_num_t cmd_removebonding_slave_slaveid =
	TOKEN_NUM_INITIALIZER(struct cmd_remove_bonding_slave_result,
		slave_id, RTE_UINT16);
static cmdline_parse_token_num_t cmd_removebonding_slave_port =
	TOKEN_NUM_INITIALIZER(struct cmd_remove_bonding_slave_result,
		port_id, RTE_UINT16);

static cmdline_parse_inst_t cmd_remove_bonding_slave = {
	.f = cmd_remove_bonding_slave_parsed,
	.help_str = "remove bonding slave <slave_id> <port_id>: "
		"Remove a slave device from a bonded device",
	.data = NULL,
	.tokens = {
		(void *)&cmd_removebonding_slave_remove,
		(void *)&cmd_removebonding_slave_bonding,
		(void *)&cmd_removebonding_slave_slave,
		(void *)&cmd_removebonding_slave_slaveid,
		(void *)&cmd_removebonding_slave_port,
		NULL
	}
};

/* *** CREATE BONDED DEVICE *** */
struct cmd_create_bonded_device_result {
	cmdline_fixed_string_t create;
	cmdline_fixed_string_t bonded;
	cmdline_fixed_string_t device;
	uint8_t mode;
	uint8_t socket;
};

static int bond_dev_num;

static void cmd_create_bonded_device_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_create_bonded_device_result *res = parsed_result;
	char ethdev_name[RTE_ETH_NAME_MAX_LEN];
	int port_id;
	int ret;

	if (test_done == 0) {
		fprintf(stderr, "Please stop forwarding first\n");
		return;
	}

	snprintf(ethdev_name, RTE_ETH_NAME_MAX_LEN, "net_bonding_testpmd_%d",
			bond_dev_num++);

	/* Create a new bonded device. */
	port_id = rte_eth_bond_create(ethdev_name, res->mode, res->socket);
	if (port_id < 0) {
		fprintf(stderr, "\t Failed to create bonded device.\n");
		return;
	}
	printf("Created new bonded device %s on (port %d).\n", ethdev_name,
		port_id);

	/* Update number of ports */
	nb_ports = rte_eth_dev_count_avail();
	reconfig(port_id, res->socket);
	ret = rte_eth_promiscuous_enable(port_id);
	if (ret != 0)
		fprintf(stderr, "Failed to enable promiscuous mode for port %u: %s - ignore\n",
			port_id, rte_strerror(-ret));

	ports[port_id].update_conf = 1;
	ports[port_id].bond_flag = 1;
	ports[port_id].need_setup = 0;
	ports[port_id].port_status = RTE_PORT_STOPPED;
}

static cmdline_parse_token_string_t cmd_createbonded_device_create =
	TOKEN_STRING_INITIALIZER(struct cmd_create_bonded_device_result,
		create, "create");
static cmdline_parse_token_string_t cmd_createbonded_device_bonded =
	TOKEN_STRING_INITIALIZER(struct cmd_create_bonded_device_result,
		bonded, "bonded");
static cmdline_parse_token_string_t cmd_createbonded_device_device =
	TOKEN_STRING_INITIALIZER(struct cmd_create_bonded_device_result,
		device, "device");
static cmdline_parse_token_num_t cmd_createbonded_device_mode =
	TOKEN_NUM_INITIALIZER(struct cmd_create_bonded_device_result,
		mode, RTE_UINT8);
static cmdline_parse_token_num_t cmd_createbonded_device_socket =
	TOKEN_NUM_INITIALIZER(struct cmd_create_bonded_device_result,
		socket, RTE_UINT8);

static cmdline_parse_inst_t cmd_create_bonded_device = {
	.f = cmd_create_bonded_device_parsed,
	.help_str = "create bonded device <mode> <socket>: "
		"Create a new bonded device with specific bonding mode and socket",
	.data = NULL,
	.tokens = {
		(void *)&cmd_createbonded_device_create,
		(void *)&cmd_createbonded_device_bonded,
		(void *)&cmd_createbonded_device_device,
		(void *)&cmd_createbonded_device_mode,
		(void *)&cmd_createbonded_device_socket,
		NULL
	}
};

/* *** SET MAC ADDRESS IN BONDED DEVICE *** */
struct cmd_set_bond_mac_addr_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t mac_addr;
	uint16_t port_num;
	struct rte_ether_addr address;
};

static void cmd_set_bond_mac_addr_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_set_bond_mac_addr_result *res = parsed_result;
	int ret;

	if (port_id_is_invalid(res->port_num, ENABLED_WARN))
		return;

	ret = rte_eth_bond_mac_address_set(res->port_num, &res->address);

	/* check the return value and print it if is < 0 */
	if (ret < 0)
		fprintf(stderr, "set_bond_mac_addr error: (%s)\n",
			strerror(-ret));
}

static cmdline_parse_token_string_t cmd_set_bond_mac_addr_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bond_mac_addr_result,
		set, "set");
static cmdline_parse_token_string_t cmd_set_bond_mac_addr_bonding =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bond_mac_addr_result,
		bonding, "bonding");
static cmdline_parse_token_string_t cmd_set_bond_mac_addr_mac =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bond_mac_addr_result,
		mac_addr, "mac_addr");
static cmdline_parse_token_num_t cmd_set_bond_mac_addr_portnum =
	TOKEN_NUM_INITIALIZER(struct cmd_set_bond_mac_addr_result,
		port_num, RTE_UINT16);
static cmdline_parse_token_etheraddr_t cmd_set_bond_mac_addr_addr =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_set_bond_mac_addr_result,
		address);

static cmdline_parse_inst_t cmd_set_bond_mac_addr = {
	.f = cmd_set_bond_mac_addr_parsed,
	.data = NULL,
	.help_str = "set bonding mac_addr <port_id> <mac_addr>",
	.tokens = {
		(void *)&cmd_set_bond_mac_addr_set,
		(void *)&cmd_set_bond_mac_addr_bonding,
		(void *)&cmd_set_bond_mac_addr_mac,
		(void *)&cmd_set_bond_mac_addr_portnum,
		(void *)&cmd_set_bond_mac_addr_addr,
		NULL
	}
};

/* *** SET LINK STATUS MONITORING POLLING PERIOD ON BONDED DEVICE *** */
struct cmd_set_bond_mon_period_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t mon_period;
	uint16_t port_num;
	uint32_t period_ms;
};

static void cmd_set_bond_mon_period_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_set_bond_mon_period_result *res = parsed_result;
	int ret;

	ret = rte_eth_bond_link_monitoring_set(res->port_num, res->period_ms);

	/* check the return value and print it if is < 0 */
	if (ret < 0)
		fprintf(stderr, "set_bond_mac_addr error: (%s)\n",
			strerror(-ret));
}

static cmdline_parse_token_string_t cmd_set_bond_mon_period_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bond_mon_period_result,
		set, "set");
static cmdline_parse_token_string_t cmd_set_bond_mon_period_bonding =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bond_mon_period_result,
		bonding, "bonding");
static cmdline_parse_token_string_t cmd_set_bond_mon_period_mon_period =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bond_mon_period_result,
		mon_period,	"mon_period");
static cmdline_parse_token_num_t cmd_set_bond_mon_period_portnum =
	TOKEN_NUM_INITIALIZER(struct cmd_set_bond_mon_period_result,
		port_num, RTE_UINT16);
static cmdline_parse_token_num_t cmd_set_bond_mon_period_period_ms =
	TOKEN_NUM_INITIALIZER(struct cmd_set_bond_mon_period_result,
		period_ms, RTE_UINT32);

static cmdline_parse_inst_t cmd_set_bond_mon_period = {
	.f = cmd_set_bond_mon_period_parsed,
	.data = NULL,
	.help_str = "set bonding mon_period <port_id> <period_ms>",
	.tokens = {
		(void *)&cmd_set_bond_mon_period_set,
		(void *)&cmd_set_bond_mon_period_bonding,
		(void *)&cmd_set_bond_mon_period_mon_period,
		(void *)&cmd_set_bond_mon_period_portnum,
		(void *)&cmd_set_bond_mon_period_period_ms,
		NULL
	}
};

struct cmd_set_bonding_agg_mode_policy_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t agg_mode;
	uint16_t port_num;
	cmdline_fixed_string_t policy;
};

static void
cmd_set_bonding_agg_mode(void *parsed_result,
	__rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_set_bonding_agg_mode_policy_result *res = parsed_result;
	uint8_t policy = AGG_BANDWIDTH;

	if (!strcmp(res->policy, "bandwidth"))
		policy = AGG_BANDWIDTH;
	else if (!strcmp(res->policy, "stable"))
		policy = AGG_STABLE;
	else if (!strcmp(res->policy, "count"))
		policy = AGG_COUNT;

	rte_eth_bond_8023ad_agg_selection_set(res->port_num, policy);
}

static cmdline_parse_token_string_t cmd_set_bonding_agg_mode_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_agg_mode_policy_result,
		set, "set");
static cmdline_parse_token_string_t cmd_set_bonding_agg_mode_bonding =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_agg_mode_policy_result,
		bonding, "bonding");
static cmdline_parse_token_string_t cmd_set_bonding_agg_mode_agg_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_agg_mode_policy_result,
		agg_mode, "agg_mode");
static cmdline_parse_token_num_t cmd_set_bonding_agg_mode_portnum =
	TOKEN_NUM_INITIALIZER(struct cmd_set_bonding_agg_mode_policy_result,
		port_num, RTE_UINT16);
static cmdline_parse_token_string_t cmd_set_bonding_agg_mode_policy_string =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_balance_xmit_policy_result,
		policy, "stable#bandwidth#count");

static cmdline_parse_inst_t cmd_set_bonding_agg_mode_policy = {
	.f = cmd_set_bonding_agg_mode,
	.data = NULL,
	.help_str = "set bonding mode IEEE802.3AD aggregator policy <port_id> <agg_name>",
	.tokens = {
		(void *)&cmd_set_bonding_agg_mode_set,
		(void *)&cmd_set_bonding_agg_mode_bonding,
		(void *)&cmd_set_bonding_agg_mode_agg_mode,
		(void *)&cmd_set_bonding_agg_mode_portnum,
		(void *)&cmd_set_bonding_agg_mode_policy_string,
		NULL
	}
};

static struct testpmd_driver_commands bonding_cmds = {
	.commands = {
	{
		&cmd_set_bonding_mode,
		"set bonding mode (value) (port_id)\n"
		"	Set the bonding mode on a bonded device.\n",
	},
	{
		&cmd_show_bonding_config,
		"show bonding config (port_id)\n"
		"	Show the bonding config for port_id.\n",
	},
	{
		&cmd_show_bonding_lacp_info,
		"show bonding lacp info (port_id)\n"
		"	Show the bonding lacp information for port_id.\n",
	},
	{
		&cmd_set_bonding_primary,
		"set bonding primary (slave_id) (port_id)\n"
		"	Set the primary slave for a bonded device.\n",
	},
	{
		&cmd_add_bonding_slave,
		"add bonding slave (slave_id) (port_id)\n"
		"	Add a slave device to a bonded device.\n",
	},
	{
		&cmd_remove_bonding_slave,
		"remove bonding slave (slave_id) (port_id)\n"
		"	Remove a slave device from a bonded device.\n",
	},
	{
		&cmd_create_bonded_device,
		"create bonded device (mode) (socket)\n"
		"	Create a new bonded device with specific bonding mode and socket.\n",
	},
	{
		&cmd_set_bond_mac_addr,
		"set bonding mac_addr (port_id) (address)\n"
		"	Set the MAC address of a bonded device.\n",
	},
	{
		&cmd_set_balance_xmit_policy,
		"set bonding balance_xmit_policy (port_id) (l2|l23|l34)\n"
		"	Set the transmit balance policy for bonded device running in balance mode.\n",
	},
	{
		&cmd_set_bond_mon_period,
		"set bonding mon_period (port_id) (value)\n"
		"	Set the bonding link status monitoring polling period in ms.\n",
	},
	{
		&cmd_set_lacp_dedicated_queues,
		"set bonding lacp dedicated_queues <port_id> (enable|disable)\n"
		"	Enable/disable dedicated queues for LACP control traffic.\n",
	},
	{
		&cmd_set_bonding_agg_mode_policy,
		"set bonding mode IEEE802.3AD aggregator policy (port_id) (agg_name)\n"
		"	Set Aggregation mode for IEEE802.3AD (mode 4)\n",
	},
	{ NULL, NULL },
	},
};
TESTPMD_ADD_DRIVER_COMMANDS(bonding_cmds)
