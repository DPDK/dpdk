/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */


#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <termios.h>

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_string.h>
#include <cmdline_parse_num.h>
#include <cmdline_socket.h>
#include <cmdline.h>
#include <rte_log.h>
#include <rte_lcore.h>
#include <rte_ethdev.h>

#include <rte_power.h>
#include <guest_channel.h>

#include "vm_power_cli_guest.h"


#define CHANNEL_PATH "/dev/virtio-ports/virtio.serial.port.poweragent"


#define RTE_LOGTYPE_GUEST_CLI RTE_LOGTYPE_USER1

struct cmd_quit_result {
	cmdline_fixed_string_t quit;
};

union PFID {
	struct rte_ether_addr addr;
	uint64_t pfid;
};

static struct channel_packet policy;

struct channel_packet *
get_policy(void)
{
	return &policy;
}

int
set_policy_mac(int port, int idx)
{
	struct channel_packet *policy;
	union PFID pfid;
	int ret;

	/* Use port MAC address as the vfid */
	ret = rte_eth_macaddr_get(port, &pfid.addr);
	if (ret != 0) {
		printf("Failed to get device (port %u) MAC address: %s\n",
				port, rte_strerror(-ret));
		return ret;
	}

	printf("Port %u MAC: %02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":"
			"%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 "\n",
			port,
			pfid.addr.addr_bytes[0], pfid.addr.addr_bytes[1],
			pfid.addr.addr_bytes[2], pfid.addr.addr_bytes[3],
			pfid.addr.addr_bytes[4], pfid.addr.addr_bytes[5]);
	policy = get_policy();
	policy->vfid[idx] = pfid.pfid;
	return 0;
}

int
set_policy_defaults(struct channel_packet *pkt)
{
	int ret;

	ret = set_policy_mac(0, 0);
	if (ret != 0)
		pkt->nb_mac_to_monitor = 0;
	else
		pkt->nb_mac_to_monitor = 1;

	pkt->t_boost_status.tbEnabled = false;

	pkt->vcpu_to_control[0] = 0;
	pkt->vcpu_to_control[1] = 1;
	pkt->num_vcpu = 2;
	/* Dummy Population. */
	pkt->traffic_policy.min_packet_thresh = 96000;
	pkt->traffic_policy.avg_max_packet_thresh = 1800000;
	pkt->traffic_policy.max_max_packet_thresh = 2000000;

	pkt->timer_policy.busy_hours[0] = 3;
	pkt->timer_policy.busy_hours[1] = 4;
	pkt->timer_policy.busy_hours[2] = 5;
	pkt->timer_policy.quiet_hours[0] = 11;
	pkt->timer_policy.quiet_hours[1] = 12;
	pkt->timer_policy.quiet_hours[2] = 13;

	pkt->timer_policy.hours_to_use_traffic_profile[0] = 8;
	pkt->timer_policy.hours_to_use_traffic_profile[1] = 10;

	pkt->core_type = CORE_TYPE_VIRTUAL;
	pkt->workload = LOW;
	pkt->policy_to_use = TIME;
	pkt->command = PKT_POLICY;
	strlcpy(pkt->vm_name, "ubuntu2", sizeof(pkt->vm_name));

	return 0;
}

static void cmd_quit_parsed(__rte_unused void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	unsigned lcore_id;

	RTE_LCORE_FOREACH(lcore_id) {
		rte_power_exit(lcore_id);
	}
	cmdline_quit(cl);
}

cmdline_parse_token_string_t cmd_quit_quit =
	TOKEN_STRING_INITIALIZER(struct cmd_quit_result, quit, "quit");

cmdline_parse_inst_t cmd_quit = {
	.f = cmd_quit_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "close the application",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_quit_quit,
		NULL,
	},
};

/* *** VM operations *** */

struct cmd_freq_list_result {
	cmdline_fixed_string_t query_freq;
	cmdline_fixed_string_t cpu_num;
};

static int
query_data(struct channel_packet *pkt, unsigned int lcore_id)
{
	int ret;
	ret = rte_power_guest_channel_send_msg(pkt, lcore_id);
	if (ret < 0) {
		RTE_LOG(ERR, GUEST_CLI, "Error sending message.\n");
		return -1;
	}
	return 0;
}

static int
receive_freq_list(struct channel_packet_freq_list *pkt_freq_list,
		unsigned int lcore_id)
{
	int ret;

	ret = rte_power_guest_channel_receive_msg(pkt_freq_list,
			sizeof(struct channel_packet_freq_list),
			lcore_id);
	if (ret < 0) {
		RTE_LOG(ERR, GUEST_CLI, "Error receiving message.\n");
		return -1;
	}
	if (pkt_freq_list->command != CPU_POWER_FREQ_LIST) {
		RTE_LOG(ERR, GUEST_CLI, "Unexpected message received.\n");
		return -1;
	}
	return 0;
}

static void
cmd_query_freq_list_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_freq_list_result *res = parsed_result;
	unsigned int lcore_id;
	struct channel_packet_freq_list pkt_freq_list;
	struct channel_packet pkt;
	bool query_list = false;
	int ret;
	char *ep;

	memset(&pkt, 0, sizeof(struct channel_packet));
	memset(&pkt_freq_list, 0, sizeof(struct channel_packet_freq_list));

	if (!strcmp(res->cpu_num, "all")) {

		/* Get first enabled lcore. */
		lcore_id = rte_get_next_lcore(-1,
				0,
				0);
		if (lcore_id == RTE_MAX_LCORE) {
			cmdline_printf(cl, "Enabled core not found.\n");
			return;
		}

		pkt.command = CPU_POWER_QUERY_FREQ_LIST;
		strlcpy(pkt.vm_name, policy.vm_name, sizeof(pkt.vm_name));
		query_list = true;
	} else {
		errno = 0;
		lcore_id = (unsigned int)strtol(res->cpu_num, &ep, 10);
		if (errno != 0 || lcore_id >= MAX_VCPU_PER_VM ||
			ep == res->cpu_num) {
			cmdline_printf(cl, "Invalid parameter provided.\n");
			return;
		}
		pkt.command = CPU_POWER_QUERY_FREQ;
		strlcpy(pkt.vm_name, policy.vm_name, sizeof(pkt.vm_name));
		pkt.resource_id = lcore_id;
	}

	ret = query_data(&pkt, lcore_id);
	if (ret < 0) {
		cmdline_printf(cl, "Error during sending frequency list query.\n");
		return;
	}

	ret = receive_freq_list(&pkt_freq_list, lcore_id);
	if (ret < 0) {
		cmdline_printf(cl, "Error during frequency list reception.\n");
		return;
	}
	if (query_list) {
		unsigned int i;
		for (i = 0; i < pkt_freq_list.num_vcpu; ++i)
			cmdline_printf(cl, "Frequency of [%d] vcore is %d.\n",
					i,
					pkt_freq_list.freq_list[i]);
	} else {
		cmdline_printf(cl, "Frequency of [%d] vcore is %d.\n",
				lcore_id,
				pkt_freq_list.freq_list[lcore_id]);
	}
}

cmdline_parse_token_string_t cmd_query_freq_token =
	TOKEN_STRING_INITIALIZER(struct cmd_freq_list_result, query_freq, "query_cpu_freq");
cmdline_parse_token_string_t cmd_query_freq_cpu_num_token =
	TOKEN_STRING_INITIALIZER(struct cmd_freq_list_result, cpu_num, NULL);

cmdline_parse_inst_t cmd_query_freq_list = {
	.f = cmd_query_freq_list_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "query_cpu_freq <core_num>|all, request"
				" information regarding virtual core frequencies."
				" The keyword 'all' will query list of all vcores for the VM",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_query_freq_token,
		(void *)&cmd_query_freq_cpu_num_token,
		NULL,
	},
};

struct cmd_query_caps_result {
	cmdline_fixed_string_t query_caps;
	cmdline_fixed_string_t cpu_num;
};

static int
receive_capabilities(struct channel_packet_caps_list *pkt_caps_list,
		unsigned int lcore_id)
{
	int ret;

	ret = rte_power_guest_channel_receive_msg(pkt_caps_list,
		sizeof(struct channel_packet_caps_list),
		lcore_id);
	if (ret < 0) {
		RTE_LOG(ERR, GUEST_CLI, "Error receiving message.\n");
		return -1;
	}
	if (pkt_caps_list->command != CPU_POWER_CAPS_LIST) {
		RTE_LOG(ERR, GUEST_CLI, "Unexpected message received.\n");
		return -1;
	}
	return 0;
}

static void
cmd_query_caps_list_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_query_caps_result *res = parsed_result;
	unsigned int lcore_id;
	struct channel_packet_caps_list pkt_caps_list;
	struct channel_packet pkt;
	bool query_list = false;
	int ret;
	char *ep;

	memset(&pkt, 0, sizeof(struct channel_packet));
	memset(&pkt_caps_list, 0, sizeof(struct channel_packet_caps_list));

	if (!strcmp(res->cpu_num, "all")) {

		/* Get first enabled lcore. */
		lcore_id = rte_get_next_lcore(-1,
				0,
				0);
		if (lcore_id == RTE_MAX_LCORE) {
			cmdline_printf(cl, "Enabled core not found.\n");
			return;
		}

		pkt.command = CPU_POWER_QUERY_CAPS_LIST;
		strlcpy(pkt.vm_name, policy.vm_name, sizeof(pkt.vm_name));
		query_list = true;
	} else {
		errno = 0;
		lcore_id = (unsigned int)strtol(res->cpu_num, &ep, 10);
		if (errno != 0 || lcore_id >= MAX_VCPU_PER_VM ||
			ep == res->cpu_num) {
			cmdline_printf(cl, "Invalid parameter provided.\n");
			return;
		}
		pkt.command = CPU_POWER_QUERY_CAPS;
		strlcpy(pkt.vm_name, policy.vm_name, sizeof(pkt.vm_name));
		pkt.resource_id = lcore_id;
	}

	ret = query_data(&pkt, lcore_id);
	if (ret < 0) {
		cmdline_printf(cl, "Error during sending capabilities query.\n");
		return;
	}

	ret = receive_capabilities(&pkt_caps_list, lcore_id);
	if (ret < 0) {
		cmdline_printf(cl, "Error during capabilities reception.\n");
		return;
	}
	if (query_list) {
		unsigned int i;
		for (i = 0; i < pkt_caps_list.num_vcpu; ++i)
			cmdline_printf(cl, "Capabilities of [%d] vcore are:"
					" turbo possibility: %" PRId64 ", "
					"is priority core: %" PRId64 ".\n",
					i,
					pkt_caps_list.turbo[i],
					pkt_caps_list.priority[i]);
	} else {
		cmdline_printf(cl, "Capabilities of [%d] vcore are:"
				" turbo possibility: %" PRId64 ", "
				"is priority core: %" PRId64 ".\n",
				lcore_id,
				pkt_caps_list.turbo[lcore_id],
				pkt_caps_list.priority[lcore_id]);
	}
}

cmdline_parse_token_string_t cmd_query_caps_token =
	TOKEN_STRING_INITIALIZER(struct cmd_query_caps_result, query_caps, "query_cpu_caps");
cmdline_parse_token_string_t cmd_query_caps_cpu_num_token =
	TOKEN_STRING_INITIALIZER(struct cmd_query_caps_result, cpu_num, NULL);

cmdline_parse_inst_t cmd_query_caps_list = {
	.f = cmd_query_caps_list_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "query_cpu_caps <core_num>|all, request"
				" information regarding virtual core capabilities."
				" The keyword 'all' will query list of all vcores for the VM",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_query_caps_token,
		(void *)&cmd_query_caps_cpu_num_token,
		NULL,
	},
};

static int
check_response_cmd(unsigned int lcore_id, int *result)
{
	struct channel_packet pkt;
	int ret;

	ret = rte_power_guest_channel_receive_msg(&pkt, sizeof pkt, lcore_id);
	if (ret < 0)
		return -1;

	switch (pkt.command) {
	case(CPU_POWER_CMD_ACK):
		*result = 1;
		break;
	case(CPU_POWER_CMD_NACK):
		*result = 0;
		break;
	default:
		RTE_LOG(ERR, GUEST_CLI,
				"Received invalid response from host, expecting ACK/NACK.\n");
		return -1;
	}

	return 0;
}

struct cmd_set_cpu_freq_result {
	cmdline_fixed_string_t set_cpu_freq;
	uint8_t lcore_id;
	cmdline_fixed_string_t cmd;
};

static void
cmd_set_cpu_freq_parsed(void *parsed_result, struct cmdline *cl,
	       __rte_unused void *data)
{
	int ret = -1;
	struct cmd_set_cpu_freq_result *res = parsed_result;

	if (!strcmp(res->cmd, "up"))
		ret = rte_power_freq_up(res->lcore_id);
	else if (!strcmp(res->cmd, "down"))
		ret = rte_power_freq_down(res->lcore_id);
	else if (!strcmp(res->cmd, "min"))
		ret = rte_power_freq_min(res->lcore_id);
	else if (!strcmp(res->cmd, "max"))
		ret = rte_power_freq_max(res->lcore_id);
	else if (!strcmp(res->cmd, "enable_turbo"))
		ret = rte_power_freq_enable_turbo(res->lcore_id);
	else if (!strcmp(res->cmd, "disable_turbo"))
		ret = rte_power_freq_disable_turbo(res->lcore_id);

	if (ret != 1) {
		cmdline_printf(cl, "Error sending message: %s\n", strerror(ret));
		return;
	}
	int result;
	ret = check_response_cmd(res->lcore_id, &result);
	if (ret < 0) {
		RTE_LOG(ERR, GUEST_CLI, "No confirmation for sent message received\n");
	} else {
		cmdline_printf(cl, "%s received for message sent to host.\n",
				result == 1 ? "ACK" : "NACK");
	}
}

cmdline_parse_token_string_t cmd_set_cpu_freq =
	TOKEN_STRING_INITIALIZER(struct cmd_set_cpu_freq_result,
			set_cpu_freq, "set_cpu_freq");
cmdline_parse_token_num_t cmd_set_cpu_freq_core_num =
	TOKEN_NUM_INITIALIZER(struct cmd_set_cpu_freq_result,
			lcore_id, UINT8);
cmdline_parse_token_string_t cmd_set_cpu_freq_cmd_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_set_cpu_freq_result,
			cmd, "up#down#min#max#enable_turbo#disable_turbo");

cmdline_parse_inst_t cmd_set_cpu_freq_set = {
	.f = cmd_set_cpu_freq_parsed,
	.data = NULL,
	.help_str = "set_cpu_freq <core_num> "
			"<up|down|min|max|enable_turbo|disable_turbo>, "
			"adjust the frequency for the specified core.",
	.tokens = {
		(void *)&cmd_set_cpu_freq,
		(void *)&cmd_set_cpu_freq_core_num,
		(void *)&cmd_set_cpu_freq_cmd_cmd,
		NULL,
	},
};

struct cmd_send_policy_result {
	cmdline_fixed_string_t send_policy;
	cmdline_fixed_string_t cmd;
};

static inline int
send_policy(struct channel_packet *pkt, struct cmdline *cl)
{
	int ret;

	ret = rte_power_guest_channel_send_msg(pkt, 1);
	if (ret < 0) {
		RTE_LOG(ERR, GUEST_CLI, "Error sending message: %s\n",
				ret > 0 ? strerror(ret) : "channel not connected");
		return -1;
	}

	int result;
	ret = check_response_cmd(1, &result);
	if (ret < 0) {
		RTE_LOG(ERR, GUEST_CLI, "No confirmation for sent policy received\n");
	} else {
		cmdline_printf(cl, "%s for sent policy received.\n",
				result == 1 ? "ACK" : "NACK");
	}
	return 1;
}

static void
cmd_send_policy_parsed(void *parsed_result, struct cmdline *cl,
		__rte_unused void *data)
{
	int ret = -1;
	struct cmd_send_policy_result *res = parsed_result;

	if (!strcmp(res->cmd, "now")) {
		printf("Sending Policy down now!\n");
		ret = send_policy(&policy, cl);
	}
	if (ret != 1)
		cmdline_printf(cl, "Error sending message: %s\n",
				strerror(ret));
}

cmdline_parse_token_string_t cmd_send_policy =
	TOKEN_STRING_INITIALIZER(struct cmd_send_policy_result,
			send_policy, "send_policy");
cmdline_parse_token_string_t cmd_send_policy_cmd_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_send_policy_result,
			cmd, "now");

cmdline_parse_inst_t cmd_send_policy_set = {
	.f = cmd_send_policy_parsed,
	.data = NULL,
	.help_str = "send_policy now",
	.tokens = {
		(void *)&cmd_send_policy,
		(void *)&cmd_send_policy_cmd_cmd,
		NULL,
	},
};

cmdline_parse_ctx_t main_ctx[] = {
		(cmdline_parse_inst_t *)&cmd_quit,
		(cmdline_parse_inst_t *)&cmd_send_policy_set,
		(cmdline_parse_inst_t *)&cmd_set_cpu_freq_set,
		(cmdline_parse_inst_t *)&cmd_query_freq_list,
		(cmdline_parse_inst_t *)&cmd_query_caps_list,
		NULL,
};

void
run_cli(__rte_unused void *arg)
{
	struct cmdline *cl;

	cl = cmdline_stdin_new(main_ctx, "vmpower(guest)> ");
	if (cl == NULL)
		return;

	cmdline_interact(cl);
	cmdline_stdin_exit(cl);
}
