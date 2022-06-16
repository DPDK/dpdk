/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 6WIND S.A.
 * Copyright 2021 Mellanox Technologies, Ltd
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <rte_prefetch.h>
#include <rte_common.h>
#include <rte_branch_prediction.h>
#include <rte_ether.h>
#include <rte_alarm.h>
#include <rte_pmd_mlx5.h>
#include <rte_ethdev.h>
#include "mlx5_testpmd.h"
#include "testpmd.h"

static uint8_t host_shaper_avail_thresh_triggered[RTE_MAX_ETHPORTS];
#define SHAPER_DISABLE_DELAY_US 100000 /* 100ms */

/**
 * Disable the host shaper and re-arm available descriptor threshold event.
 *
 * @param[in] args
 *   uint32_t integer combining port_id and rxq_id.
 */
static void
mlx5_test_host_shaper_disable(void *args)
{
	uint32_t port_rxq_id = (uint32_t)(uintptr_t)args;
	uint16_t port_id = port_rxq_id & 0xffff;
	uint16_t qid = (port_rxq_id >> 16) & 0xffff;
	struct rte_eth_rxq_info qinfo;

	printf("%s disable shaper\n", __func__);
	if (rte_eth_rx_queue_info_get(port_id, qid, &qinfo)) {
		printf("rx_queue_info_get returns error\n");
		return;
	}
	/* Rearm the available descriptor threshold event. */
	if (rte_eth_rx_avail_thresh_set(port_id, qid, qinfo.avail_thresh)) {
		printf("config avail_thresh returns error\n");
		return;
	}
	/* Only disable the shaper when avail_thresh_triggered is set. */
	if (host_shaper_avail_thresh_triggered[port_id] &&
	    rte_pmd_mlx5_host_shaper_config(port_id, 0, 0))
		printf("%s disable shaper returns error\n", __func__);
}

void
mlx5_test_avail_thresh_event_handler(uint16_t port_id, uint16_t rxq_id)
{
	struct rte_eth_dev_info dev_info;
	uint32_t port_rxq_id = port_id | (rxq_id << 16);

	/* Ensure it's MLX5 port. */
	if (rte_eth_dev_info_get(port_id, &dev_info) != 0 ||
	    (strncmp(dev_info.driver_name, "mlx5", 4) != 0))
		return;
	rte_eal_alarm_set(SHAPER_DISABLE_DELAY_US,
			  mlx5_test_host_shaper_disable,
			  (void *)(uintptr_t)port_rxq_id);
	printf("%s port_id:%u rxq_id:%u\n", __func__, port_id, rxq_id);
}

/**
 * Configure host shaper's avail_thresh_triggered and current rate.
 *
 * @param[in] avail_thresh_triggered
 *   Disable/enable avail_thresh_triggered.
 * @param[in] rate
 *   Configure current host shaper rate.
 * @return
 *   On success, returns 0.
 *   On failure, returns < 0.
 */
static int
mlx5_test_set_port_host_shaper(uint16_t port_id, uint16_t avail_thresh_triggered, uint8_t rate)
{
	struct rte_eth_link link;
	bool port_id_valid = false;
	uint16_t pid;
	int ret;

	RTE_ETH_FOREACH_DEV(pid)
		if (port_id == pid) {
			port_id_valid = true;
			break;
		}
	if (!port_id_valid)
		return -EINVAL;
	ret = rte_eth_link_get_nowait(port_id, &link);
	if (ret < 0)
		return ret;
	host_shaper_avail_thresh_triggered[port_id] = avail_thresh_triggered ? 1 : 0;
	if (!avail_thresh_triggered) {
		ret = rte_pmd_mlx5_host_shaper_config(port_id, 0,
		RTE_BIT32(MLX5_HOST_SHAPER_FLAG_AVAIL_THRESH_TRIGGERED));
	} else {
		ret = rte_pmd_mlx5_host_shaper_config(port_id, 1,
		RTE_BIT32(MLX5_HOST_SHAPER_FLAG_AVAIL_THRESH_TRIGGERED));
	}
	if (ret)
		return ret;
	ret = rte_pmd_mlx5_host_shaper_config(port_id, rate, 0);
	if (ret)
		return ret;
	return 0;
}

/* *** SET HOST_SHAPER FOR A PORT *** */
struct cmd_port_host_shaper_result {
	cmdline_fixed_string_t mlx5;
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	uint16_t port_num;
	cmdline_fixed_string_t host_shaper;
	cmdline_fixed_string_t avail_thresh_triggered;
	uint16_t fr;
	cmdline_fixed_string_t rate;
	uint8_t rate_num;
};

static void cmd_port_host_shaper_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_port_host_shaper_result *res = parsed_result;
	int ret = 0;

	if ((strcmp(res->mlx5, "mlx5") == 0) &&
	    (strcmp(res->set, "set") == 0) &&
	    (strcmp(res->port, "port") == 0) &&
	    (strcmp(res->host_shaper, "host_shaper") == 0) &&
	    (strcmp(res->avail_thresh_triggered, "avail_thresh_triggered") == 0) &&
	    (strcmp(res->rate, "rate") == 0))
		ret = mlx5_test_set_port_host_shaper(res->port_num, res->fr,
					   res->rate_num);
	if (ret < 0)
		printf("cmd_port_host_shaper error: (%s)\n", strerror(-ret));
}

static cmdline_parse_token_string_t cmd_port_host_shaper_mlx5 =
	TOKEN_STRING_INITIALIZER(struct cmd_port_host_shaper_result,
				mlx5, "mlx5");
static cmdline_parse_token_string_t cmd_port_host_shaper_set =
	TOKEN_STRING_INITIALIZER(struct cmd_port_host_shaper_result,
				set, "set");
static cmdline_parse_token_string_t cmd_port_host_shaper_port =
	TOKEN_STRING_INITIALIZER(struct cmd_port_host_shaper_result,
				port, "port");
static cmdline_parse_token_num_t cmd_port_host_shaper_portnum =
	TOKEN_NUM_INITIALIZER(struct cmd_port_host_shaper_result,
				port_num, RTE_UINT16);
static cmdline_parse_token_string_t cmd_port_host_shaper_host_shaper =
	TOKEN_STRING_INITIALIZER(struct cmd_port_host_shaper_result,
				 host_shaper, "host_shaper");
static cmdline_parse_token_string_t cmd_port_host_shaper_avail_thresh_triggered =
	TOKEN_STRING_INITIALIZER(struct cmd_port_host_shaper_result,
				 avail_thresh_triggered, "avail_thresh_triggered");
static cmdline_parse_token_num_t cmd_port_host_shaper_fr =
	TOKEN_NUM_INITIALIZER(struct cmd_port_host_shaper_result,
			      fr, RTE_UINT16);
static cmdline_parse_token_string_t cmd_port_host_shaper_rate =
	TOKEN_STRING_INITIALIZER(struct cmd_port_host_shaper_result,
				 rate, "rate");
static cmdline_parse_token_num_t cmd_port_host_shaper_rate_num =
	TOKEN_NUM_INITIALIZER(struct cmd_port_host_shaper_result,
			      rate_num, RTE_UINT8);
static cmdline_parse_inst_t mlx5_test_cmd_port_host_shaper = {
	.f = cmd_port_host_shaper_parsed,
	.data = (void *)0,
	.help_str = "mlx5 set port <port_id> host_shaper avail_thresh_triggered <0|1> "
	"rate <rate_num>: Set HOST_SHAPER avail_thresh_triggered and rate with port_id",
	.tokens = {
		(void *)&cmd_port_host_shaper_mlx5,
		(void *)&cmd_port_host_shaper_set,
		(void *)&cmd_port_host_shaper_port,
		(void *)&cmd_port_host_shaper_portnum,
		(void *)&cmd_port_host_shaper_host_shaper,
		(void *)&cmd_port_host_shaper_avail_thresh_triggered,
		(void *)&cmd_port_host_shaper_fr,
		(void *)&cmd_port_host_shaper_rate,
		(void *)&cmd_port_host_shaper_rate_num,
		NULL,
	}
};

static struct testpmd_driver_commands mlx5_driver_cmds = {
	.commands = {
		{
			.ctx = &mlx5_test_cmd_port_host_shaper,
			.help = "mlx5 set port (port_id) host_shaper avail_thresh_triggered (on|off)"
				"rate (rate_num):\n"
				"    Set HOST_SHAPER avail_thresh_triggered and rate with port_id\n\n",
		},
		{
			.ctx = NULL,
		},
	}
};
TESTPMD_ADD_DRIVER_COMMANDS(mlx5_driver_cmds);
