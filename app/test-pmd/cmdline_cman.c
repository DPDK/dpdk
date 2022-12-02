/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell International Ltd.
 */

#include <stdlib.h>

#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>

#include <rte_ethdev.h>

#include "testpmd.h"

#define PARSE_DELIMITER				" \f\n\r\t\v"

static int
parse_uint(uint64_t *value, const char *str)
{
	char *next = NULL;
	uint64_t n;

	errno = 0;
	/* Parse number string */
	n = strtol(str, &next, 10);
	if (errno != 0 || str == next || *next != '\0')
		return -1;

	*value = n;

	return 0;
}

static int
parse_cman_obj_str(char *str, uint64_t *obj)
{
	char *token;

	token = strtok_r(str, PARSE_DELIMITER, &str);
	if (token == NULL)
		return 0;

	if (strcasecmp(token, "queue") == 0)
		*obj = RTE_ETH_CMAN_OBJ_RX_QUEUE;
	else if (strcasecmp(token, "queue_mempool") == 0)
		*obj = RTE_ETH_CMAN_OBJ_RX_QUEUE_MEMPOOL;
	else
		return -1;

	return 0;
}

static int
parse_cman_mode_str(char *str, uint64_t *mode)
{
	char *token;

	token = strtok_r(str, PARSE_DELIMITER, &str);
	if (token == NULL)
		return 0;

	if (strcasecmp(token, "red") == 0)
		*mode = RTE_CMAN_RED;
	else
		return -1;

	return 0;
}

static int
parse_cman_params_str(uint16_t port_id, char *str,
		      struct rte_eth_cman_config *cfg)
{
	uint64_t obj = 0, mode = 0, min_th = 0, max_th = 0, maxp_inv = 0;
	struct rte_eth_cman_info info;
	char *token;
	int ret;

	token = strtok_r(str, PARSE_DELIMITER, &str);
	if (!strcasecmp(token, "default")) {
		ret = rte_eth_cman_config_init(port_id, cfg);
		if (ret) {
			fprintf(stderr, "error in default initialization\n");
			return ret;
		}
		return 0;
	}

	/* First token: obj name */
	token = strtok_r(str, PARSE_DELIMITER, &str);
	if (token == NULL) {
		fprintf(stderr, "Object param parse error\n");
		goto error;
	}

	ret = parse_cman_obj_str(token, &obj);
	if (ret) {
		fprintf(stderr, "Object value is invalid\n");
		goto error;
	}

	/* Second token: mode name */
	token = strtok_r(str, PARSE_DELIMITER, &str);
	if (token == NULL) {
		fprintf(stderr, " Mode param is invalid\n");
		goto error;
	}

	token = strtok_r(str, PARSE_DELIMITER, &str);
	if (token == NULL) {
		fprintf(stderr, " Mode value is invalid\n");
		goto error;
	}

	ret = parse_cman_mode_str(token, &mode);
	if (ret) {
		fprintf(stderr, "mode string parse error\n");
		goto error;
	}

	/* Third token: minimum threshold */
	token = strtok_r(str, PARSE_DELIMITER, &str);
	if (token == NULL) {
		fprintf(stderr, "Minimum threshold parse error\n");
		goto error;
	}

	ret = parse_uint(&min_th, token);
	if (ret != 0 || min_th > UINT8_MAX) {
		fprintf(stderr, "Minimum threshold is invalid\n");
		goto error;
	}

	/* Fourth token: maximum threshold */
	token = strtok_r(str, PARSE_DELIMITER, &str);
	if (token == NULL) {
		fprintf(stderr, "Maximum threshold parse error\n");
		goto error;
	}

	ret = parse_uint(&max_th, token);
	if (ret != 0 || max_th > UINT8_MAX) {
		fprintf(stderr, "Maximum threshold is invalid\n");
		goto error;
	}

	/* Fifth token: probability inversion */
	token = strtok_r(str, PARSE_DELIMITER, &str);
	if (token == NULL) {
		fprintf(stderr, "Maximum probability inversion parse error\n");
		goto error;
	}

	ret = parse_uint(&maxp_inv, token);
	if (ret != 0 || maxp_inv == 0 || maxp_inv > UINT16_MAX) {
		fprintf(stderr, "Maximum probability inversion is invalid\n");
		goto error;
	}

	memset(&info, 0, sizeof(struct rte_eth_cman_info));
	ret = rte_eth_cman_info_get(port_id, &info);
	if (ret) {
		fprintf(stderr, "Congestion management capa get error\n");
		goto error;
	}

	if (!(info.objs_supported & obj)) {
		fprintf(stderr, "Object type is not supported by driver\n");
		goto error;
	}

	if (!(info.modes_supported & mode)) {
		fprintf(stderr, "Mode is not supported by driver\n");
		goto error;
	}

	cfg->obj = obj;
	cfg->mode = mode;
	cfg->mode_param.red.min_th = min_th;
	cfg->mode_param.red.max_th = max_th;
	cfg->mode_param.red.maxp_inv = maxp_inv;

	return 0;

error:
	return -EINVAL;
}

/* *** Show Port Congestion Management Capabilities *** */
struct cmd_show_port_cman_capa_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t cman;
	cmdline_fixed_string_t capa;
	uint16_t port_id;
};

static cmdline_parse_token_string_t cmd_show_port_cman_capa_show =
	TOKEN_STRING_INITIALIZER(
		struct cmd_show_port_cman_capa_result, show, "show");

static cmdline_parse_token_string_t cmd_show_port_cman_capa_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_show_port_cman_capa_result, port, "port");

static cmdline_parse_token_string_t cmd_show_port_cman_capa_cman =
	TOKEN_STRING_INITIALIZER(
		struct cmd_show_port_cman_capa_result, cman, "cman");

static cmdline_parse_token_string_t cmd_show_port_cman_capa_capa =
	TOKEN_STRING_INITIALIZER(
		struct cmd_show_port_cman_capa_result, capa, "capa");

static cmdline_parse_token_num_t cmd_show_port_cman_capa_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_show_port_cman_capa_result, port_id, RTE_UINT16);

static void cmd_show_port_cman_capa_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_show_port_cman_capa_result *res = parsed_result;
	uint16_t port_id = res->port_id;
	struct rte_eth_cman_info info;
	int ret;

	memset(&info, 0, sizeof(struct rte_eth_cman_info));
	ret = rte_eth_cman_info_get(port_id, &info);
	if (ret)
		return;

	printf("\n****   Port Congestion Management Capabilities   ****\n\n");
	printf("modes_supported 0x%" PRIx64 "\n", info.modes_supported);
	printf("objs_supported 0x%" PRIx64 "\n", info.objs_supported);
}

cmdline_parse_inst_t cmd_show_port_cman_capa = {
	.f = cmd_show_port_cman_capa_parsed,
	.data = NULL,
	.help_str = "show port cman capa <port_id>",
	.tokens = {
		(void *)&cmd_show_port_cman_capa_show,
		(void *)&cmd_show_port_cman_capa_port,
		(void *)&cmd_show_port_cman_capa_cman,
		(void *)&cmd_show_port_cman_capa_capa,
		(void *)&cmd_show_port_cman_capa_port_id,
		NULL,
	},
};

/* *** Show Port Congestion Management configuration *** */
struct cmd_show_port_cman_cfg_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t cman;
	cmdline_fixed_string_t cfg;
	uint16_t port_id;
};

static cmdline_parse_token_string_t cmd_show_port_cman_cfg_show =
	TOKEN_STRING_INITIALIZER(
		struct cmd_show_port_cman_cfg_result, show, "show");

static cmdline_parse_token_string_t cmd_show_port_cman_cfg_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_show_port_cman_cfg_result, port, "port");

static cmdline_parse_token_string_t cmd_show_port_cman_cfg_cman =
	TOKEN_STRING_INITIALIZER(
		struct cmd_show_port_cman_cfg_result, cman, "cman");

static cmdline_parse_token_string_t cmd_show_port_cman_cfg_cfg =
	TOKEN_STRING_INITIALIZER(
		struct cmd_show_port_cman_cfg_result, cfg, "config");

static cmdline_parse_token_num_t cmd_show_port_cman_cfg_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_show_port_cman_cfg_result, port_id, RTE_UINT16);

static void cmd_show_port_cman_cfg_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_show_port_cman_cfg_result *res = parsed_result;
	uint16_t port_id = res->port_id;
	struct rte_eth_cman_config cfg;
	int ret;

	memset(&cfg, 0, sizeof(struct rte_eth_cman_config));
	ret = rte_eth_cman_config_get(port_id, &cfg);
	if (ret)
		return;

	printf("\n****   Port Congestion Management Configuration   ****\n\n");
	printf("cman object 0x%" PRIx32 "\n", cfg.obj);
	printf("cman Rx queue %" PRIx16 "\n", cfg.obj_param.rx_queue);
	printf("cman mode 0x%" PRIx32 "\n", cfg.mode);
	printf("cman RED min thresh %" PRIx8 "\n", cfg.mode_param.red.min_th);
	printf("cman RED max thresh %" PRIx8 "\n", cfg.mode_param.red.max_th);
	printf("cman RED Prob inversion %" PRIx16 "\n",
		cfg.mode_param.red.maxp_inv);
}

cmdline_parse_inst_t cmd_show_port_cman_config = {
	.f = cmd_show_port_cman_cfg_parsed,
	.data = NULL,
	.help_str = "show port cman config <port_id>",
	.tokens = {
		(void *)&cmd_show_port_cman_cfg_show,
		(void *)&cmd_show_port_cman_cfg_port,
		(void *)&cmd_show_port_cman_cfg_cman,
		(void *)&cmd_show_port_cman_cfg_cfg,
		(void *)&cmd_show_port_cman_cfg_port_id,
		NULL,
	},
};

/* *** Set Port Congestion Management configuration *** */
struct cmd_set_port_cman_cfg_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t cman;
	cmdline_fixed_string_t cfg;
	uint16_t port_id;
	uint16_t qid;
	cmdline_multi_string_t params;
};

static cmdline_parse_token_string_t cmd_set_port_cman_cfg_set =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_cman_cfg_result, set, "set");

static cmdline_parse_token_string_t cmd_set_port_cman_cfg_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_cman_cfg_result, port, "port");

static cmdline_parse_token_string_t cmd_set_port_cman_cfg_cman =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_cman_cfg_result, cman, "cman");

static cmdline_parse_token_string_t cmd_set_port_cman_cfg_cfg =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_cman_cfg_result, cfg, "config");

static cmdline_parse_token_num_t cmd_set_port_cman_cfg_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_set_port_cman_cfg_result, port_id, RTE_UINT16);

static cmdline_parse_token_num_t cmd_set_port_cman_cfg_qid =
	TOKEN_NUM_INITIALIZER(
		struct cmd_set_port_cman_cfg_result, qid, RTE_UINT16);

static cmdline_parse_token_string_t cmd_set_port_cman_cfg_params =
	TOKEN_STRING_INITIALIZER(struct cmd_set_port_cman_cfg_result,
		params, TOKEN_STRING_MULTI);

static void cmd_set_port_cman_cfg_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_set_port_cman_cfg_result *res = parsed_result;
	uint16_t port_id = res->port_id;
	struct rte_eth_cman_config cfg;
	int ret;

	ret = parse_cman_params_str(port_id, res->params, &cfg);
	if (ret) {
		fprintf(stderr, "params string parse error\n");
		return;
	}

	cfg.obj_param.rx_queue = res->qid;
	rte_eth_cman_config_set(port_id, &cfg);
}

cmdline_parse_inst_t cmd_set_port_cman_config = {
	.f = cmd_set_port_cman_cfg_parsed,
	.data = NULL,
	.help_str = "set port cman config <port_id> <queue_id> "
		    "default | [obj <queue|queue_mempool> mode red "
		    "<min_thresh> <max_thresh> <prob_inv>]",
	.tokens = {
		(void *)&cmd_set_port_cman_cfg_set,
		(void *)&cmd_set_port_cman_cfg_port,
		(void *)&cmd_set_port_cman_cfg_cman,
		(void *)&cmd_set_port_cman_cfg_cfg,
		(void *)&cmd_set_port_cman_cfg_port_id,
		(void *)&cmd_set_port_cman_cfg_qid,
		(void *)&cmd_set_port_cman_cfg_params,
		NULL,
	},
};
