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


#define RTE_LOGTYPE_GUEST_CHANNEL RTE_LOGTYPE_USER1

struct cmd_quit_result {
	cmdline_fixed_string_t quit;
};

static void cmd_quit_parsed(__attribute__((unused)) void *parsed_result,
				__attribute__((unused)) struct cmdline *cl,
			    __attribute__((unused)) void *data)
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

struct cmd_set_cpu_freq_result {
	cmdline_fixed_string_t set_cpu_freq;
	uint8_t lcore_id;
	cmdline_fixed_string_t cmd;
};

static void
cmd_set_cpu_freq_parsed(void *parsed_result, struct cmdline *cl,
		       __attribute__((unused)) void *data)
{
	int ret = -1;
	struct cmd_set_cpu_freq_result *res = parsed_result;

	if (!strcmp(res->cmd , "up"))
		ret = rte_power_freq_up(res->lcore_id);
	else if (!strcmp(res->cmd , "down"))
		ret = rte_power_freq_down(res->lcore_id);
	else if (!strcmp(res->cmd , "min"))
		ret = rte_power_freq_min(res->lcore_id);
	else if (!strcmp(res->cmd , "max"))
		ret = rte_power_freq_max(res->lcore_id);
	else if (!strcmp(res->cmd, "enable_turbo"))
		ret = rte_power_freq_enable_turbo(res->lcore_id);
	else if (!strcmp(res->cmd, "disable_turbo"))
		ret = rte_power_freq_disable_turbo(res->lcore_id);
	if (ret != 1)
		cmdline_printf(cl, "Error sending message: %s\n", strerror(ret));
}

cmdline_parse_token_string_t cmd_set_cpu_freq =
	TOKEN_STRING_INITIALIZER(struct cmd_set_cpu_freq_result,
			set_cpu_freq, "set_cpu_freq");
cmdline_parse_token_string_t cmd_set_cpu_freq_core_num =
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

union PFID {
	struct ether_addr addr;
	uint64_t pfid;
};

static inline int
send_policy(void)
{
	struct channel_packet pkt;
	int ret;

	union PFID pfid;
	/* Use port MAC address as the vfid */
	rte_eth_macaddr_get(0, &pfid.addr);
	printf("Port %u MAC: %02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":"
			"%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 "\n",
			1,
			pfid.addr.addr_bytes[0], pfid.addr.addr_bytes[1],
			pfid.addr.addr_bytes[2], pfid.addr.addr_bytes[3],
			pfid.addr.addr_bytes[4], pfid.addr.addr_bytes[5]);
	pkt.vfid[0] = pfid.pfid;

	pkt.nb_mac_to_monitor = 1;
	pkt.t_boost_status.tbEnabled = false;

	pkt.vcpu_to_control[0] = 0;
	pkt.vcpu_to_control[1] = 1;
	pkt.num_vcpu = 2;
	/* Dummy Population. */
	pkt.traffic_policy.min_packet_thresh = 96000;
	pkt.traffic_policy.avg_max_packet_thresh = 1800000;
	pkt.traffic_policy.max_max_packet_thresh = 2000000;

	pkt.timer_policy.busy_hours[0] = 3;
	pkt.timer_policy.busy_hours[1] = 4;
	pkt.timer_policy.busy_hours[2] = 5;
	pkt.timer_policy.quiet_hours[0] = 11;
	pkt.timer_policy.quiet_hours[1] = 12;
	pkt.timer_policy.quiet_hours[2] = 13;

	pkt.timer_policy.hours_to_use_traffic_profile[0] = 8;
	pkt.timer_policy.hours_to_use_traffic_profile[1] = 10;

	pkt.workload = LOW;
	pkt.policy_to_use = TIME;
	pkt.command = PKT_POLICY;
	strcpy(pkt.vm_name, "ubuntu2");
	ret = rte_power_guest_channel_send_msg(&pkt, 1);
	if (ret == 0)
		return 1;
	RTE_LOG(DEBUG, POWER, "Error sending message: %s\n",
			ret > 0 ? strerror(ret) : "channel not connected");
	return -1;
}

static void
cmd_send_policy_parsed(void *parsed_result, struct cmdline *cl,
		       __attribute__((unused)) void *data)
{
	int ret = -1;
	struct cmd_send_policy_result *res = parsed_result;

	if (!strcmp(res->cmd, "now")) {
		printf("Sending Policy down now!\n");
		ret = send_policy();
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
		NULL,
};

void
run_cli(__attribute__((unused)) void *arg)
{
	struct cmdline *cl;

	cl = cmdline_stdin_new(main_ctx, "vmpower(guest)> ");
	if (cl == NULL)
		return;

	cmdline_interact(cl);
	cmdline_stdin_exit(cl);
}
