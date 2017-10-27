/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>

#include <rte_ethdev.h>
#include <rte_flow.h>
#include <rte_mtr.h>

#include "testpmd.h"
#include "cmdline_mtr.h"

/** Display Meter Error Message */
static void
print_err_msg(struct rte_mtr_error *error)
{
	static const char *const errstrlist[] = {
		[RTE_MTR_ERROR_TYPE_NONE] = "no error",
		[RTE_MTR_ERROR_TYPE_UNSPECIFIED] = "cause unspecified",
		[RTE_MTR_ERROR_TYPE_METER_PROFILE_ID] = "meter profile id",
		[RTE_MTR_ERROR_TYPE_METER_PROFILE] = "meter profile null",
		[RTE_MTR_ERROR_TYPE_MTR_ID] = "meter id",
		[RTE_MTR_ERROR_TYPE_MTR_PARAMS] = "meter params null",
		[RTE_MTR_ERROR_TYPE_POLICER_ACTION_GREEN]
			= "policer action(green)",
		[RTE_MTR_ERROR_TYPE_POLICER_ACTION_YELLOW]
			= "policer action(yellow)",
		[RTE_MTR_ERROR_TYPE_POLICER_ACTION_RED]
			= "policer action(red)",
		[RTE_MTR_ERROR_TYPE_STATS_MASK] = "stats mask",
		[RTE_MTR_ERROR_TYPE_STATS] = "stats",
		[RTE_MTR_ERROR_TYPE_SHARED]
			= "shared meter",
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
string_to_policer_action(char *s)
{
	if (strcmp(s, "G") == 0)
		return MTR_POLICER_ACTION_COLOR_GREEN;

	if (strcmp(s, "Y") == 0)
		return MTR_POLICER_ACTION_COLOR_YELLOW;

	if (strcmp(s, "R") == 0)
		return MTR_POLICER_ACTION_COLOR_RED;

	if (strcmp(s, "D") == 0)
		return MTR_POLICER_ACTION_DROP;

	return -1;
}

/* *** Add Port Meter Profile srtcm_rfc2697 *** */
struct cmd_add_port_meter_profile_srtcm_result {
	cmdline_fixed_string_t add;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t meter;
	cmdline_fixed_string_t profile;
	cmdline_fixed_string_t srtcm_rfc2697;
	uint16_t port_id;
	uint32_t profile_id;
	uint64_t cir;
	uint64_t cbs;
	uint64_t ebs;
	uint8_t color_aware;
};

cmdline_parse_token_string_t cmd_add_port_meter_profile_srtcm_add =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_meter_profile_srtcm_result, add, "add");
cmdline_parse_token_string_t cmd_add_port_meter_profile_srtcm_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_meter_profile_srtcm_result,
			port, "port");
cmdline_parse_token_string_t cmd_add_port_meter_profile_srtcm_meter =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_meter_profile_srtcm_result,
			meter, "meter");
cmdline_parse_token_string_t cmd_add_port_meter_profile_srtcm_profile =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_meter_profile_srtcm_result,
			profile, "profile");
cmdline_parse_token_string_t cmd_add_port_meter_profile_srtcm_srtcm_rfc2697 =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_meter_profile_srtcm_result,
			srtcm_rfc2697, "srtcm_rfc2697");
cmdline_parse_token_num_t cmd_add_port_meter_profile_srtcm_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_meter_profile_srtcm_result,
			port_id, UINT16);
cmdline_parse_token_num_t cmd_add_port_meter_profile_srtcm_profile_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_meter_profile_srtcm_result,
			profile_id, UINT32);
cmdline_parse_token_num_t cmd_add_port_meter_profile_srtcm_cir =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_meter_profile_srtcm_result,
			cir, UINT64);
cmdline_parse_token_num_t cmd_add_port_meter_profile_srtcm_cbs =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_meter_profile_srtcm_result,
			cbs, UINT64);
cmdline_parse_token_num_t cmd_add_port_meter_profile_srtcm_ebs =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_meter_profile_srtcm_result,
			ebs, UINT64);

static void cmd_add_port_meter_profile_srtcm_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_add_port_meter_profile_srtcm_result *res = parsed_result;
	struct rte_mtr_meter_profile mp;
	struct rte_mtr_error error;
	uint32_t profile_id = res->profile_id;
	uint16_t port_id = res->port_id;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	/* Private shaper profile params */
	memset(&mp, 0, sizeof(struct rte_mtr_meter_profile));
	mp.alg = 0;
	mp.srtcm_rfc2697.cir = res->cir;
	mp.srtcm_rfc2697.cbs = res->cbs;
	mp.srtcm_rfc2697.ebs = res->ebs;

	ret = rte_mtr_meter_profile_add(port_id, profile_id, &mp, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_add_port_meter_profile_srtcm = {
	.f = cmd_add_port_meter_profile_srtcm_parsed,
	.data = NULL,
	.help_str = "Add port meter profile srtcm (rfc2697)",
	.tokens = {
		(void *)&cmd_add_port_meter_profile_srtcm_add,
		(void *)&cmd_add_port_meter_profile_srtcm_port,
		(void *)&cmd_add_port_meter_profile_srtcm_meter,
		(void *)&cmd_add_port_meter_profile_srtcm_profile,
		(void *)&cmd_add_port_meter_profile_srtcm_port_id,
		(void *)&cmd_add_port_meter_profile_srtcm_profile_id,
		(void *)&cmd_add_port_meter_profile_srtcm_srtcm_rfc2697,
		(void *)&cmd_add_port_meter_profile_srtcm_cir,
		(void *)&cmd_add_port_meter_profile_srtcm_cbs,
		(void *)&cmd_add_port_meter_profile_srtcm_ebs,
		NULL,
	},
};

/* *** Add Port Meter Profile trtcm_rfc2698 *** */
struct cmd_add_port_meter_profile_trtcm_result {
	cmdline_fixed_string_t add;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t meter;
	cmdline_fixed_string_t profile;
	cmdline_fixed_string_t trtcm_rfc2698;
	uint16_t port_id;
	uint32_t profile_id;
	uint64_t cir;
	uint64_t pir;
	uint64_t cbs;
	uint64_t pbs;
};

cmdline_parse_token_string_t cmd_add_port_meter_profile_trtcm_add =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_meter_profile_trtcm_result, add, "add");
cmdline_parse_token_string_t cmd_add_port_meter_profile_trtcm_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_meter_profile_trtcm_result,
			port, "port");
cmdline_parse_token_string_t cmd_add_port_meter_profile_trtcm_meter =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_meter_profile_trtcm_result,
			meter, "meter");
cmdline_parse_token_string_t cmd_add_port_meter_profile_trtcm_profile =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_meter_profile_trtcm_result,
			profile, "profile");
cmdline_parse_token_string_t cmd_add_port_meter_profile_trtcm_trtcm_rfc2698 =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_meter_profile_trtcm_result,
			trtcm_rfc2698, "trtcm_rfc2698");
cmdline_parse_token_num_t cmd_add_port_meter_profile_trtcm_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_meter_profile_trtcm_result,
			port_id, UINT16);
cmdline_parse_token_num_t cmd_add_port_meter_profile_trtcm_profile_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_meter_profile_trtcm_result,
			profile_id, UINT32);
cmdline_parse_token_num_t cmd_add_port_meter_profile_trtcm_cir =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_meter_profile_trtcm_result,
			cir, UINT64);
cmdline_parse_token_num_t cmd_add_port_meter_profile_trtcm_pir =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_meter_profile_trtcm_result,
			pir, UINT64);
cmdline_parse_token_num_t cmd_add_port_meter_profile_trtcm_cbs =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_meter_profile_trtcm_result,
			cbs, UINT64);
cmdline_parse_token_num_t cmd_add_port_meter_profile_trtcm_pbs =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_meter_profile_trtcm_result,
			pbs, UINT64);

static void cmd_add_port_meter_profile_trtcm_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_add_port_meter_profile_trtcm_result *res = parsed_result;
	struct rte_mtr_meter_profile mp;
	struct rte_mtr_error error;
	uint32_t profile_id = res->profile_id;
	uint16_t port_id = res->port_id;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	/* Private shaper profile params */
	memset(&mp, 0, sizeof(struct rte_mtr_meter_profile));
	mp.alg = 0;
	mp.trtcm_rfc2698.cir = res->cir;
	mp.trtcm_rfc2698.pir = res->pir;
	mp.trtcm_rfc2698.cbs = res->cbs;
	mp.trtcm_rfc2698.pbs = res->pbs;

	ret = rte_mtr_meter_profile_add(port_id, profile_id, &mp, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_add_port_meter_profile_trtcm = {
	.f = cmd_add_port_meter_profile_trtcm_parsed,
	.data = NULL,
	.help_str = "Add port meter profile trtcm (rfc2698)",
	.tokens = {
		(void *)&cmd_add_port_meter_profile_trtcm_add,
		(void *)&cmd_add_port_meter_profile_trtcm_port,
		(void *)&cmd_add_port_meter_profile_trtcm_meter,
		(void *)&cmd_add_port_meter_profile_trtcm_profile,
		(void *)&cmd_add_port_meter_profile_trtcm_port_id,
		(void *)&cmd_add_port_meter_profile_trtcm_profile_id,
		(void *)&cmd_add_port_meter_profile_trtcm_trtcm_rfc2698,
		(void *)&cmd_add_port_meter_profile_trtcm_cir,
		(void *)&cmd_add_port_meter_profile_trtcm_pir,
		(void *)&cmd_add_port_meter_profile_trtcm_cbs,
		(void *)&cmd_add_port_meter_profile_trtcm_pbs,
		NULL,
	},
};

/* *** Add Port Meter Profile trtcm_rfc4115 *** */
struct cmd_add_port_meter_profile_trtcm_rfc4115_result {
	cmdline_fixed_string_t add;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t meter;
	cmdline_fixed_string_t profile;
	cmdline_fixed_string_t trtcm_rfc4115;
	uint16_t port_id;
	uint32_t profile_id;
	uint64_t cir;
	uint64_t eir;
	uint64_t cbs;
	uint64_t ebs;
};

cmdline_parse_token_string_t cmd_add_port_meter_profile_trtcm_rfc4115_add =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_meter_profile_trtcm_rfc4115_result, add,
		"add");
cmdline_parse_token_string_t cmd_add_port_meter_profile_trtcm_rfc4115_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_meter_profile_trtcm_rfc4115_result,
			port, "port");
cmdline_parse_token_string_t cmd_add_port_meter_profile_trtcm_rfc4115_meter =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_meter_profile_trtcm_rfc4115_result,
			meter, "meter");
cmdline_parse_token_string_t cmd_add_port_meter_profile_trtcm_rfc4115_profile =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_meter_profile_trtcm_rfc4115_result,
			profile, "profile");
cmdline_parse_token_string_t
	cmd_add_port_meter_profile_trtcm_rfc4115_trtcm_rfc4115 =
	TOKEN_STRING_INITIALIZER(
		struct cmd_add_port_meter_profile_trtcm_rfc4115_result,
			trtcm_rfc4115, "trtcm_rfc4115");
cmdline_parse_token_num_t cmd_add_port_meter_profile_trtcm_rfc4115_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_meter_profile_trtcm_rfc4115_result,
			port_id, UINT16);
cmdline_parse_token_num_t cmd_add_port_meter_profile_trtcm_rfc4115_profile_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_meter_profile_trtcm_rfc4115_result,
			profile_id, UINT32);
cmdline_parse_token_num_t cmd_add_port_meter_profile_trtcm_rfc4115_cir =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_meter_profile_trtcm_rfc4115_result,
			cir, UINT64);
cmdline_parse_token_num_t cmd_add_port_meter_profile_trtcm_rfc4115_eir =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_meter_profile_trtcm_rfc4115_result,
			eir, UINT64);
cmdline_parse_token_num_t cmd_add_port_meter_profile_trtcm_rfc4115_cbs =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_meter_profile_trtcm_rfc4115_result,
			cbs, UINT64);
cmdline_parse_token_num_t cmd_add_port_meter_profile_trtcm_rfc4115_ebs =
	TOKEN_NUM_INITIALIZER(
		struct cmd_add_port_meter_profile_trtcm_rfc4115_result,
			ebs, UINT64);

static void cmd_add_port_meter_profile_trtcm_rfc4115_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_add_port_meter_profile_trtcm_rfc4115_result *res =
		parsed_result;
	struct rte_mtr_meter_profile mp;
	struct rte_mtr_error error;
	uint32_t profile_id = res->profile_id;
	uint16_t port_id = res->port_id;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	/* Private shaper profile params */
	memset(&mp, 0, sizeof(struct rte_mtr_meter_profile));
	mp.alg = 0;
	mp.trtcm_rfc4115.cir = res->cir;
	mp.trtcm_rfc4115.eir = res->eir;
	mp.trtcm_rfc4115.cbs = res->cbs;
	mp.trtcm_rfc4115.ebs = res->ebs;

	ret = rte_mtr_meter_profile_add(port_id, profile_id, &mp, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_add_port_meter_profile_trtcm_rfc4115 = {
	.f = cmd_add_port_meter_profile_trtcm_rfc4115_parsed,
	.data = NULL,
	.help_str = "Add port meter profile trtcm (rfc4115)",
	.tokens = {
		(void *)&cmd_add_port_meter_profile_trtcm_rfc4115_add,
		(void *)&cmd_add_port_meter_profile_trtcm_rfc4115_port,
		(void *)&cmd_add_port_meter_profile_trtcm_rfc4115_meter,
		(void *)&cmd_add_port_meter_profile_trtcm_rfc4115_profile,
		(void *)&cmd_add_port_meter_profile_trtcm_rfc4115_port_id,
		(void *)&cmd_add_port_meter_profile_trtcm_rfc4115_profile_id,
		(void *)&cmd_add_port_meter_profile_trtcm_rfc4115_trtcm_rfc4115,
		(void *)&cmd_add_port_meter_profile_trtcm_rfc4115_cir,
		(void *)&cmd_add_port_meter_profile_trtcm_rfc4115_eir,
		(void *)&cmd_add_port_meter_profile_trtcm_rfc4115_cbs,
		(void *)&cmd_add_port_meter_profile_trtcm_rfc4115_ebs,
		NULL,
	},
};

/* *** Delete Port Meter Profile *** */
struct cmd_del_port_meter_profile_result {
	cmdline_fixed_string_t del;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t meter;
	cmdline_fixed_string_t profile;
	uint16_t port_id;
	uint32_t profile_id;
};

cmdline_parse_token_string_t cmd_del_port_meter_profile_del =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_meter_profile_result, del, "del");
cmdline_parse_token_string_t cmd_del_port_meter_profile_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_meter_profile_result,
			port, "port");
cmdline_parse_token_string_t cmd_del_port_meter_profile_meter =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_meter_profile_result,
			meter, "meter");
cmdline_parse_token_string_t cmd_del_port_meter_profile_profile =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_meter_profile_result,
			profile, "profile");
cmdline_parse_token_num_t cmd_del_port_meter_profile_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_del_port_meter_profile_result,
			port_id, UINT16);
cmdline_parse_token_num_t cmd_del_port_meter_profile_profile_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_del_port_meter_profile_result,
			profile_id, UINT32);

static void cmd_del_port_meter_profile_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_del_port_meter_profile_result *res = parsed_result;
	struct rte_mtr_error error;
	uint32_t profile_id = res->profile_id;
	uint16_t port_id = res->port_id;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	/* Delete meter profile */
	ret = rte_mtr_meter_profile_delete(port_id, profile_id, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_del_port_meter_profile = {
	.f = cmd_del_port_meter_profile_parsed,
	.data = NULL,
	.help_str = "Delete port meter profile",
	.tokens = {
		(void *)&cmd_del_port_meter_profile_del,
		(void *)&cmd_del_port_meter_profile_port,
		(void *)&cmd_del_port_meter_profile_meter,
		(void *)&cmd_del_port_meter_profile_profile,
		(void *)&cmd_del_port_meter_profile_port_id,
		(void *)&cmd_del_port_meter_profile_profile_id,
		NULL,
	},
};

/* *** Create Port Meter Object *** */
struct cmd_set_port_meter_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t meter;
	uint16_t port_id;
	uint32_t mtr_id;
	uint32_t profile_id;
	cmdline_fixed_string_t g_action;
	cmdline_fixed_string_t y_action;
	cmdline_fixed_string_t r_action;
	uint64_t statistics_mask;
	uint32_t shared;
};

cmdline_parse_token_string_t cmd_set_port_meter_set =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_meter_result, set, "set");
cmdline_parse_token_string_t cmd_set_port_meter_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_meter_result, port, "port");
cmdline_parse_token_string_t cmd_set_port_meter_meter =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_meter_result, meter, "meter");
cmdline_parse_token_num_t cmd_set_port_meter_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_set_port_meter_result, port_id, UINT16);
cmdline_parse_token_num_t cmd_set_port_meter_mtr_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_set_port_meter_result, mtr_id, UINT32);
cmdline_parse_token_num_t cmd_set_port_meter_profile_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_set_port_meter_result, profile_id, UINT32);
cmdline_parse_token_string_t cmd_set_port_meter_g_action =
	TOKEN_STRING_INITIALIZER(struct cmd_set_port_meter_result,
		g_action, "R#Y#G#D");
cmdline_parse_token_string_t cmd_set_port_meter_y_action =
	TOKEN_STRING_INITIALIZER(struct cmd_set_port_meter_result,
		y_action, "R#Y#G#D");
cmdline_parse_token_string_t cmd_set_port_meter_r_action =
	TOKEN_STRING_INITIALIZER(struct cmd_set_port_meter_result,
		r_action, "R#Y#G#D");
cmdline_parse_token_num_t cmd_set_port_meter_statistics_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_set_port_meter_result,
		statistics_mask, UINT64);
cmdline_parse_token_num_t cmd_set_port_meter_shared =
	TOKEN_NUM_INITIALIZER(struct cmd_set_port_meter_result,
		shared, UINT32);

static void cmd_set_port_meter_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_set_port_meter_result *res = parsed_result;
	struct rte_mtr_error error;
	struct rte_mtr_params params;
	uint32_t mtr_id = res->mtr_id;
	uint32_t shared = res->shared;
	uint16_t port_id = res->port_id;

	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	/* Meter params */
	memset(&params, 0, sizeof(struct rte_mtr_params));
	params.meter_profile_id = res->profile_id;
	params.use_prev_mtr_color = 1;
	params.dscp_table = NULL;
	params.meter_enable = 1;
	params.action[RTE_MTR_GREEN] =
		string_to_policer_action(res->g_action);
	params.action[RTE_MTR_YELLOW] =
		string_to_policer_action(res->y_action);
	params.action[RTE_MTR_RED] =
		string_to_policer_action(res->r_action);
	params.stats_mask = res->statistics_mask;

	ret = rte_mtr_create(port_id, mtr_id, &params, shared, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_set_port_meter = {
	.f = cmd_set_port_meter_parsed,
	.data = NULL,
	.help_str = "Set port meter",
	.tokens = {
		(void *)&cmd_set_port_meter_set,
		(void *)&cmd_set_port_meter_port,
		(void *)&cmd_set_port_meter_meter,
		(void *)&cmd_set_port_meter_port_id,
		(void *)&cmd_set_port_meter_mtr_id,
		(void *)&cmd_set_port_meter_profile_id,
		(void *)&cmd_set_port_meter_g_action,
		(void *)&cmd_set_port_meter_y_action,
		(void *)&cmd_set_port_meter_r_action,
		(void *)&cmd_set_port_meter_statistics_mask,
		(void *)&cmd_set_port_meter_shared,
		NULL,
	},
};

/* *** Delete Port Meter Object *** */
struct cmd_del_port_meter_result {
	cmdline_fixed_string_t del;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t meter;
	uint16_t port_id;
	uint32_t mtr_id;
};

cmdline_parse_token_string_t cmd_del_port_meter_del =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_meter_result, del, "del");
cmdline_parse_token_string_t cmd_del_port_meter_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_meter_result, port, "port");
cmdline_parse_token_string_t cmd_del_port_meter_meter =
	TOKEN_STRING_INITIALIZER(
		struct cmd_del_port_meter_result, meter, "meter");
cmdline_parse_token_num_t cmd_del_port_meter_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_del_port_meter_result, port_id, UINT16);
cmdline_parse_token_num_t cmd_del_port_meter_mtr_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_del_port_meter_result, mtr_id, UINT32);

static void cmd_del_port_meter_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_del_port_meter_result *res = parsed_result;
	struct rte_mtr_error error;
	uint32_t mtr_id = res->mtr_id;
	uint16_t port_id = res->port_id;

	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	/* Destroy Meter */
	ret = rte_mtr_destroy(port_id, mtr_id, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_del_port_meter = {
	.f = cmd_del_port_meter_parsed,
	.data = NULL,
	.help_str = "Delete port meter",
	.tokens = {
		(void *)&cmd_del_port_meter_del,
		(void *)&cmd_del_port_meter_port,
		(void *)&cmd_del_port_meter_meter,
		(void *)&cmd_del_port_meter_port_id,
		(void *)&cmd_del_port_meter_mtr_id,
		NULL,
	},
};

/* *** Set Port Meter Profile *** */
struct cmd_set_port_meter_profile_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t meter;
	cmdline_fixed_string_t profile;
	uint16_t port_id;
	uint32_t mtr_id;
	uint32_t profile_id;
};

cmdline_parse_token_string_t cmd_set_port_meter_profile_set =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_meter_profile_result, set, "set");
cmdline_parse_token_string_t cmd_set_port_meter_profile_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_meter_profile_result, port, "port");
cmdline_parse_token_string_t cmd_set_port_meter_profile_meter =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_meter_profile_result, meter, "meter");
cmdline_parse_token_string_t cmd_set_port_meter_profile_profile =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_meter_profile_result, profile, "profile");
cmdline_parse_token_num_t cmd_set_port_meter_profile_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_set_port_meter_profile_result, port_id, UINT16);
cmdline_parse_token_num_t cmd_set_port_meter_profile_mtr_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_set_port_meter_profile_result, mtr_id, UINT32);
cmdline_parse_token_num_t cmd_set_port_meter_profile_profile_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_set_port_meter_profile_result, profile_id, UINT32);

static void cmd_set_port_meter_profile_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_set_port_meter_profile_result *res = parsed_result;
	struct rte_mtr_error error;
	uint32_t mtr_id = res->mtr_id;
	uint32_t profile_id = res->profile_id;
	uint16_t port_id = res->port_id;

	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	/* Set meter profile */
	ret = rte_mtr_meter_profile_update(port_id, mtr_id,
		profile_id, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_set_port_meter_profile = {
	.f = cmd_set_port_meter_profile_parsed,
	.data = NULL,
	.help_str = "Set port meter profile",
	.tokens = {
		(void *)&cmd_set_port_meter_profile_set,
		(void *)&cmd_set_port_meter_profile_port,
		(void *)&cmd_set_port_meter_profile_meter,
		(void *)&cmd_set_port_meter_profile_profile,
		(void *)&cmd_set_port_meter_profile_port_id,
		(void *)&cmd_set_port_meter_profile_mtr_id,
		(void *)&cmd_set_port_meter_profile_profile_id,
		NULL,
	},
};

/* *** Set Port Meter Policer Action *** */
struct cmd_set_port_meter_policer_action_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t meter;
	cmdline_fixed_string_t policer;
	cmdline_fixed_string_t action;
	uint16_t port_id;
	uint32_t mtr_id;
	cmdline_fixed_string_t color;
	cmdline_fixed_string_t policer_action;
};

cmdline_parse_token_string_t cmd_set_port_meter_policer_action_set =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_meter_policer_action_result, set, "set");
cmdline_parse_token_string_t cmd_set_port_meter_policer_action_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_meter_policer_action_result, port, "port");
cmdline_parse_token_string_t cmd_set_port_meter_policer_action_meter =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_meter_policer_action_result, meter,
		"meter");
cmdline_parse_token_string_t cmd_set_port_meter_policer_action_policer =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_meter_policer_action_result, policer,
		"policer");
cmdline_parse_token_string_t cmd_set_port_meter_policer_action_action =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_meter_policer_action_result, action,
		"action");
cmdline_parse_token_num_t cmd_set_port_meter_policer_action_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_set_port_meter_policer_action_result, port_id,
		UINT16);
cmdline_parse_token_num_t cmd_set_port_meter_policer_action_mtr_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_set_port_meter_policer_action_result, mtr_id,
		UINT32);
cmdline_parse_token_string_t cmd_set_port_meter_policer_action_color =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_meter_policer_action_result, color,
		"G#Y#R");
cmdline_parse_token_string_t cmd_set_port_meter_policer_action_policer_action =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_meter_policer_action_result,
		policer_action, "G#Y#R#D");

static void cmd_set_port_meter_policer_action_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_set_port_meter_policer_action_result *res = parsed_result;
	enum rte_mtr_color color;
	enum rte_mtr_policer_action action[RTE_MTR_COLORS];
	struct rte_mtr_error error;
	uint32_t mtr_id = res->mtr_id;
	uint16_t port_id = res->port_id;
	char *c = res->color;
	char *a = res->policer_action;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	/* Color */
	if (strcmp(c, "G") == 0)
		color = RTE_MTR_GREEN;
	else if (strcmp(c, "Y") == 0)
		color = RTE_MTR_YELLOW;
	else
		color = RTE_MTR_RED;

	/* Action */
	if (strcmp(a, "G") == 0)
		action[color] = MTR_POLICER_ACTION_COLOR_GREEN;
	else if (strcmp(a, "Y") == 0)
		action[color] = MTR_POLICER_ACTION_COLOR_YELLOW;
	else if (strcmp(a, "R") == 0)
		action[color] = MTR_POLICER_ACTION_COLOR_RED;
	else
		action[color] = MTR_POLICER_ACTION_DROP;

	ret = rte_mtr_policer_actions_update(port_id, mtr_id,
		1 << color, action, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_set_port_meter_policer_action = {
	.f = cmd_set_port_meter_policer_action_parsed,
	.data = NULL,
	.help_str = "Set port meter policer action",
	.tokens = {
		(void *)&cmd_set_port_meter_policer_action_set,
		(void *)&cmd_set_port_meter_policer_action_port,
		(void *)&cmd_set_port_meter_policer_action_meter,
		(void *)&cmd_set_port_meter_policer_action_policer,
		(void *)&cmd_set_port_meter_policer_action_action,
		(void *)&cmd_set_port_meter_policer_action_port_id,
		(void *)&cmd_set_port_meter_policer_action_mtr_id,
		(void *)&cmd_set_port_meter_policer_action_color,
		(void *)&cmd_set_port_meter_policer_action_policer_action,
		NULL,
	},
};

/* *** Set Port Meter Stats Mask *** */
struct cmd_set_port_meter_stats_mask_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t meter;
	cmdline_fixed_string_t stats;
	cmdline_fixed_string_t mask;
	uint16_t port_id;
	uint32_t mtr_id;
	uint64_t stats_mask;
};

cmdline_parse_token_string_t cmd_set_port_meter_stats_mask_set =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_meter_stats_mask_result, set, "set");
cmdline_parse_token_string_t cmd_set_port_meter_stats_mask_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_meter_stats_mask_result, port, "port");
cmdline_parse_token_string_t cmd_set_port_meter_stats_mask_meter =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_meter_stats_mask_result, meter, "meter");
cmdline_parse_token_string_t cmd_set_port_meter_stats_mask_stats =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_meter_stats_mask_result, stats, "stats");
cmdline_parse_token_string_t cmd_set_port_meter_stats_mask_mask =
	TOKEN_STRING_INITIALIZER(
		struct cmd_set_port_meter_stats_mask_result, mask, "mask");
cmdline_parse_token_num_t cmd_set_port_meter_stats_mask_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_set_port_meter_stats_mask_result, port_id, UINT16);
cmdline_parse_token_num_t cmd_set_port_meter_stats_mask_mtr_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_set_port_meter_stats_mask_result, mtr_id, UINT32);
cmdline_parse_token_num_t cmd_set_port_meter_stats_mask_stats_mask =
	TOKEN_NUM_INITIALIZER(
		struct cmd_set_port_meter_stats_mask_result, stats_mask,
		UINT64);

static void cmd_set_port_meter_stats_mask_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_set_port_meter_stats_mask_result *res = parsed_result;
	struct rte_mtr_error error;
	uint64_t stats_mask = res->stats_mask;
	uint32_t mtr_id = res->mtr_id;
	uint16_t port_id = res->port_id;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	ret = rte_mtr_stats_update(port_id, mtr_id, stats_mask, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}
}

cmdline_parse_inst_t cmd_set_port_meter_stats_mask = {
	.f = cmd_set_port_meter_stats_mask_parsed,
	.data = NULL,
	.help_str = "Set port meter stats mask",
	.tokens = {
		(void *)&cmd_set_port_meter_stats_mask_set,
		(void *)&cmd_set_port_meter_stats_mask_port,
		(void *)&cmd_set_port_meter_stats_mask_meter,
		(void *)&cmd_set_port_meter_stats_mask_stats,
		(void *)&cmd_set_port_meter_stats_mask_mask,
		(void *)&cmd_set_port_meter_stats_mask_port_id,
		(void *)&cmd_set_port_meter_stats_mask_mtr_id,
		(void *)&cmd_set_port_meter_stats_mask_stats_mask,
		NULL,
	},
};

/* *** Show Port Meter Stats *** */
struct cmd_show_port_meter_stats_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t meter;
	cmdline_fixed_string_t stats;
	uint16_t port_id;
	uint32_t mtr_id;
	uint32_t clear;
};

cmdline_parse_token_string_t cmd_show_port_meter_stats_show =
	TOKEN_STRING_INITIALIZER(
		struct cmd_show_port_meter_stats_result, show, "show");
cmdline_parse_token_string_t cmd_show_port_meter_stats_port =
	TOKEN_STRING_INITIALIZER(
		struct cmd_show_port_meter_stats_result, port, "port");
cmdline_parse_token_string_t cmd_show_port_meter_stats_meter =
	TOKEN_STRING_INITIALIZER(
		struct cmd_show_port_meter_stats_result, meter, "meter");
cmdline_parse_token_string_t cmd_show_port_meter_stats_stats =
	TOKEN_STRING_INITIALIZER(
		struct cmd_show_port_meter_stats_result, stats, "stats");
cmdline_parse_token_num_t cmd_show_port_meter_stats_port_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_show_port_meter_stats_result, port_id, UINT16);
cmdline_parse_token_num_t cmd_show_port_meter_stats_mtr_id =
	TOKEN_NUM_INITIALIZER(
		struct cmd_show_port_meter_stats_result, mtr_id, UINT32);
cmdline_parse_token_num_t cmd_show_port_meter_stats_clear =
	TOKEN_NUM_INITIALIZER(
		struct cmd_show_port_meter_stats_result, clear, UINT32);

static void cmd_show_port_meter_stats_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_show_port_meter_stats_result *res = parsed_result;
	struct rte_mtr_stats stats;
	uint64_t stats_mask = 0;
	struct rte_mtr_error error;
	uint32_t mtr_id = res->mtr_id;
	uint32_t clear = res->clear;
	uint16_t port_id = res->port_id;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&stats, 0, sizeof(struct rte_mtr_stats));
	ret = rte_mtr_stats_read(port_id, mtr_id, &stats,
		&stats_mask, clear, &error);
	if (ret != 0) {
		print_err_msg(&error);
		return;
	}

	/* Display stats */
	if (stats_mask & RTE_MTR_STATS_N_PKTS_GREEN)
		printf("\tPkts G: %" PRIu64 "\n",
			stats.n_pkts[RTE_MTR_GREEN]);
	if (stats_mask & RTE_MTR_STATS_N_BYTES_GREEN)
		printf("\tBytes G: %" PRIu64 "\n",
			stats.n_bytes[RTE_MTR_GREEN]);
	if (stats_mask & RTE_MTR_STATS_N_PKTS_YELLOW)
		printf("\tPkts Y: %" PRIu64 "\n",
			stats.n_pkts[RTE_MTR_YELLOW]);
	if (stats_mask & RTE_MTR_STATS_N_BYTES_YELLOW)
		printf("\tBytes Y: %" PRIu64 "\n",
			stats.n_bytes[RTE_MTR_YELLOW]);
	if (stats_mask & RTE_MTR_STATS_N_PKTS_RED)
		printf("\tPkts R: %" PRIu64 "\n",
			stats.n_pkts[RTE_MTR_RED]);
	if (stats_mask & RTE_MTR_STATS_N_BYTES_RED)
		printf("\tBytes Y: %" PRIu64 "\n",
			stats.n_bytes[RTE_MTR_RED]);
	if (stats_mask & RTE_MTR_STATS_N_PKTS_DROPPED)
		printf("\tPkts DROPPED: %" PRIu64 "\n",
			stats.n_pkts_dropped);
	if (stats_mask & RTE_MTR_STATS_N_BYTES_DROPPED)
		printf("\tBytes DROPPED: %" PRIu64 "\n",
			stats.n_bytes_dropped);
}

cmdline_parse_inst_t cmd_show_port_meter_stats = {
	.f = cmd_show_port_meter_stats_parsed,
	.data = NULL,
	.help_str = "Show port meter stats",
	.tokens = {
		(void *)&cmd_show_port_meter_stats_show,
		(void *)&cmd_show_port_meter_stats_port,
		(void *)&cmd_show_port_meter_stats_meter,
		(void *)&cmd_show_port_meter_stats_stats,
		(void *)&cmd_show_port_meter_stats_port_id,
		(void *)&cmd_show_port_meter_stats_mtr_id,
		(void *)&cmd_show_port_meter_stats_clear,
		NULL,
	},
};
