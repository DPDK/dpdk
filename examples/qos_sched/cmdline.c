/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#include <stdio.h>
#include <termios.h>
#include <inttypes.h>
#include <string.h>

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_socket.h>
#include <cmdline.h>

#include "main.h"

/* *** Help command with introduction. *** */
struct cmd_help_result {
	cmdline_fixed_string_t help;
};

static void cmd_help_parsed(__attribute__((unused)) void *parsed_result,
                                  struct cmdline *cl,
                                  __attribute__((unused)) void *data)
{
	cmdline_printf(
		cl,
		"\n"
		"The following commands are currently available:\n\n"
		"Control:\n"
		"    quit                                      : Quit the application.\n"
		"\nStatistics:\n"
		"    stats app                                 : Show app statistics.\n"
		"    stats port X subport Y                    : Show stats of a specific subport.\n"
		"    stats port X subport Y pipe Z             : Show stats of a specific pipe.\n"
		"\nAverage queue size:\n"
		"    qavg port X subport Y                     : Show average queue size per subport.\n"
		"    qavg port X subport Y tc Z                : Show average queue size per subport and TC.\n"
		"    qavg port X subport Y pipe Z              : Show average queue size per pipe.\n"
		"    qavg port X subport Y pipe Z tc A         : Show average queue size per pipe and TC.\n"
		"    qavg port X subport Y pipe Z tc A q B     : Show average queue size of a specific queue.\n"
		"    qavg [n|period] X                     : Set number of times and peiod (us).\n\n"
	);

}

cmdline_parse_token_string_t cmd_help_help =
	TOKEN_STRING_INITIALIZER(struct cmd_help_result, help, "help");

cmdline_parse_inst_t cmd_help = {
	.f = cmd_help_parsed,
	.data = NULL,
	.help_str = "show help",
	.tokens = {
		(void *)&cmd_help_help,
		NULL,
	},
};

/* *** QUIT *** */
struct cmd_quit_result {
	cmdline_fixed_string_t quit;
};

static void cmd_quit_parsed(__attribute__((unused)) void *parsed_result,
		struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	cmdline_quit(cl);
}

cmdline_parse_token_string_t cmd_quit_quit =
		TOKEN_STRING_INITIALIZER(struct cmd_quit_result, quit, "quit");

cmdline_parse_inst_t cmd_quit = {
	.f = cmd_quit_parsed,
	.data = NULL,
	.help_str = "exit application",
	.tokens = {
		(void *)&cmd_quit_quit,
		NULL,
		},
};

/* *** SET QAVG PARAMETERS *** */
struct cmd_setqavg_result {
        cmdline_fixed_string_t qavg_string;
        cmdline_fixed_string_t param_string;
        uint32_t number;
};

static void cmd_setqavg_parsed(void *parsed_result,
                                __attribute__((unused)) struct cmdline *cl,
                                __attribute__((unused)) void *data)
{
        struct cmd_setqavg_result *res = parsed_result;

	if (!strcmp(res->param_string, "period"))
		qavg_period = res->number;
	else if (!strcmp(res->param_string, "n"))
		qavg_ntimes = res->number;
	else
		printf("\nUnknown parameter.\n\n");
}

cmdline_parse_token_string_t cmd_setqavg_qavg_string =
        TOKEN_STRING_INITIALIZER(struct cmd_setqavg_result, qavg_string,
                                "qavg");
cmdline_parse_token_string_t cmd_setqavg_param_string =
        TOKEN_STRING_INITIALIZER(struct cmd_setqavg_result, param_string,
                                "period#n");
cmdline_parse_token_num_t cmd_setqavg_number =
        TOKEN_NUM_INITIALIZER(struct cmd_setqavg_result, number,
                                UINT32);

cmdline_parse_inst_t cmd_setqavg = {
        .f = cmd_setqavg_parsed,
        .data = NULL,
        .help_str = "Show subport stats.",
        .tokens = {
                (void *)&cmd_setqavg_qavg_string,
                (void *)&cmd_setqavg_param_string,
                (void *)&cmd_setqavg_number,
                NULL,
        },
};

/* *** SHOW APP STATS *** */
struct cmd_appstats_result {
	cmdline_fixed_string_t stats_string;
	cmdline_fixed_string_t app_string;
};

static void cmd_appstats_parsed(__attribute__((unused)) void *parsed_result,
				__attribute__((unused)) struct cmdline *cl,
				__attribute__((unused)) void *data)
{
	app_stat();
}

cmdline_parse_token_string_t cmd_appstats_stats_string =
	TOKEN_STRING_INITIALIZER(struct cmd_appstats_result, stats_string,
				"stats");
cmdline_parse_token_string_t cmd_appstats_app_string =
	TOKEN_STRING_INITIALIZER(struct cmd_appstats_result, app_string,
				"app");

cmdline_parse_inst_t cmd_appstats = {
	.f = cmd_appstats_parsed,
	.data = NULL,
	.help_str = "Show app stats.",
	.tokens = {
		(void *)&cmd_appstats_stats_string,
		(void *)&cmd_appstats_app_string,
		NULL,
	},
};

/* *** SHOW SUBPORT STATS *** */
struct cmd_subportstats_result {
        cmdline_fixed_string_t stats_string;
        cmdline_fixed_string_t port_string;
	uint8_t port_number;
        cmdline_fixed_string_t subport_string;
        uint32_t subport_number;
};

static void cmd_subportstats_parsed(void *parsed_result,
                                __attribute__((unused)) struct cmdline *cl,
                                __attribute__((unused)) void *data)
{
	struct cmd_subportstats_result *res = parsed_result;

	if (subport_stat(res->port_number, res->subport_number) < 0)
		printf ("\nStats not available for these parameters. Check that both the port and subport are correct.\n\n");
}

cmdline_parse_token_string_t cmd_subportstats_stats_string =
        TOKEN_STRING_INITIALIZER(struct cmd_subportstats_result, stats_string,
                                "stats");
cmdline_parse_token_string_t cmd_subportstats_port_string =
        TOKEN_STRING_INITIALIZER(struct cmd_subportstats_result, port_string,
                                "port");
cmdline_parse_token_string_t cmd_subportstats_subport_string =
        TOKEN_STRING_INITIALIZER(struct cmd_subportstats_result, subport_string,
                                "subport");
cmdline_parse_token_num_t cmd_subportstats_subport_number =
        TOKEN_NUM_INITIALIZER(struct cmd_subportstats_result, subport_number,
                                UINT32);
cmdline_parse_token_num_t cmd_subportstats_port_number =
        TOKEN_NUM_INITIALIZER(struct cmd_subportstats_result, port_number,
                                UINT8);

cmdline_parse_inst_t cmd_subportstats = {
        .f = cmd_subportstats_parsed,
        .data = NULL,
        .help_str = "Show subport stats.",
        .tokens = {
                (void *)&cmd_subportstats_stats_string,
                (void *)&cmd_subportstats_port_string,
                (void *)&cmd_subportstats_port_number,
                (void *)&cmd_subportstats_subport_string,
                (void *)&cmd_subportstats_subport_number,
                NULL,
        },
};

/* *** SHOW PIPE STATS *** */
struct cmd_pipestats_result {
        cmdline_fixed_string_t stats_string;
        cmdline_fixed_string_t port_string;
        uint8_t port_number;
        cmdline_fixed_string_t subport_string;
        uint32_t subport_number;
        cmdline_fixed_string_t pipe_string;
        uint32_t pipe_number;
};

static void cmd_pipestats_parsed(void *parsed_result,
                                __attribute__((unused)) struct cmdline *cl,
                                __attribute__((unused)) void *data)
{
        struct cmd_pipestats_result *res = parsed_result;

        if (pipe_stat(res->port_number, res->subport_number, res->pipe_number) < 0)
                printf ("\nStats not available for these parameters. Check that both the port and subport are correct.\n\n");
}

cmdline_parse_token_string_t cmd_pipestats_stats_string =
        TOKEN_STRING_INITIALIZER(struct cmd_pipestats_result, stats_string,
                                "stats");
cmdline_parse_token_string_t cmd_pipestats_port_string =
        TOKEN_STRING_INITIALIZER(struct cmd_pipestats_result, port_string,
                                "port");
cmdline_parse_token_num_t cmd_pipestats_port_number =
        TOKEN_NUM_INITIALIZER(struct cmd_pipestats_result, port_number,
                                UINT8);
cmdline_parse_token_string_t cmd_pipestats_subport_string =
        TOKEN_STRING_INITIALIZER(struct cmd_pipestats_result, subport_string,
                                "subport");
cmdline_parse_token_num_t cmd_pipestats_subport_number =
        TOKEN_NUM_INITIALIZER(struct cmd_pipestats_result, subport_number,
                                UINT32);
cmdline_parse_token_string_t cmd_pipestats_pipe_string =
        TOKEN_STRING_INITIALIZER(struct cmd_pipestats_result, pipe_string,
                                "pipe");
cmdline_parse_token_num_t cmd_pipestats_pipe_number =
        TOKEN_NUM_INITIALIZER(struct cmd_pipestats_result, pipe_number,
                                UINT32);

cmdline_parse_inst_t cmd_pipestats = {
        .f = cmd_pipestats_parsed,
        .data = NULL,
        .help_str = "Show pipe stats.",
        .tokens = {
                (void *)&cmd_pipestats_stats_string,
                (void *)&cmd_pipestats_port_string,
                (void *)&cmd_pipestats_port_number,
                (void *)&cmd_pipestats_subport_string,
                (void *)&cmd_pipestats_subport_number,
                (void *)&cmd_pipestats_pipe_string,
                (void *)&cmd_pipestats_pipe_number,
                NULL,
        },
};

/* *** SHOW AVERAGE QUEUE SIZE (QUEUE) *** */
struct cmd_avg_q_result {
        cmdline_fixed_string_t qavg_string;
        cmdline_fixed_string_t port_string;
        uint8_t port_number;
        cmdline_fixed_string_t subport_string;
        uint32_t subport_number;
        cmdline_fixed_string_t pipe_string;
        uint32_t pipe_number;
        cmdline_fixed_string_t tc_string;
        uint8_t tc_number;
        cmdline_fixed_string_t q_string;
        uint8_t q_number;
};

static void cmd_avg_q_parsed(void *parsed_result,
                                __attribute__((unused)) struct cmdline *cl,
                                __attribute__((unused)) void *data)
{
        struct cmd_avg_q_result *res = parsed_result;

        if (qavg_q(res->port_number, res->subport_number, res->pipe_number, res->tc_number, res->q_number) < 0)
                printf ("\nStats not available for these parameters. Check that both the port and subport are correct.\n\n");
}

cmdline_parse_token_string_t cmd_avg_q_qavg_string =
        TOKEN_STRING_INITIALIZER(struct cmd_avg_q_result, qavg_string,
                                "qavg");
cmdline_parse_token_string_t cmd_avg_q_port_string =
        TOKEN_STRING_INITIALIZER(struct cmd_avg_q_result, port_string,
                                "port");
cmdline_parse_token_num_t cmd_avg_q_port_number =
        TOKEN_NUM_INITIALIZER(struct cmd_avg_q_result, port_number,
                                UINT8);
cmdline_parse_token_string_t cmd_avg_q_subport_string =
        TOKEN_STRING_INITIALIZER(struct cmd_avg_q_result, subport_string,
                                "subport");
cmdline_parse_token_num_t cmd_avg_q_subport_number =
        TOKEN_NUM_INITIALIZER(struct cmd_avg_q_result, subport_number,
                                UINT32);
cmdline_parse_token_string_t cmd_avg_q_pipe_string =
        TOKEN_STRING_INITIALIZER(struct cmd_avg_q_result, pipe_string,
                                "pipe");
cmdline_parse_token_num_t cmd_avg_q_pipe_number =
        TOKEN_NUM_INITIALIZER(struct cmd_avg_q_result, pipe_number,
                                UINT32);
cmdline_parse_token_string_t cmd_avg_q_tc_string =
        TOKEN_STRING_INITIALIZER(struct cmd_avg_q_result, tc_string,
                                "tc");
cmdline_parse_token_num_t cmd_avg_q_tc_number =
        TOKEN_NUM_INITIALIZER(struct cmd_avg_q_result, tc_number,
                                UINT8);
cmdline_parse_token_string_t cmd_avg_q_q_string =
        TOKEN_STRING_INITIALIZER(struct cmd_avg_q_result, q_string,
                                "q");
cmdline_parse_token_num_t cmd_avg_q_q_number =
        TOKEN_NUM_INITIALIZER(struct cmd_avg_q_result, q_number,
                                UINT8);

cmdline_parse_inst_t cmd_avg_q = {
        .f = cmd_avg_q_parsed,
        .data = NULL,
        .help_str = "Show pipe stats.",
        .tokens = {
                (void *)&cmd_avg_q_qavg_string,
                (void *)&cmd_avg_q_port_string,
                (void *)&cmd_avg_q_port_number,
                (void *)&cmd_avg_q_subport_string,
                (void *)&cmd_avg_q_subport_number,
                (void *)&cmd_avg_q_pipe_string,
                (void *)&cmd_avg_q_pipe_number,
                (void *)&cmd_avg_q_tc_string,
                (void *)&cmd_avg_q_tc_number,
                (void *)&cmd_avg_q_q_string,
                (void *)&cmd_avg_q_q_number,
                NULL,
        },
};

/* *** SHOW AVERAGE QUEUE SIZE (tc/pipe) *** */
struct cmd_avg_tcpipe_result {
        cmdline_fixed_string_t qavg_string;
        cmdline_fixed_string_t port_string;
        uint8_t port_number;
        cmdline_fixed_string_t subport_string;
        uint32_t subport_number;
        cmdline_fixed_string_t pipe_string;
        uint32_t pipe_number;
        cmdline_fixed_string_t tc_string;
        uint8_t tc_number;
};

static void cmd_avg_tcpipe_parsed(void *parsed_result,
                                __attribute__((unused)) struct cmdline *cl,
                                __attribute__((unused)) void *data)
{
        struct cmd_avg_tcpipe_result *res = parsed_result;

        if (qavg_tcpipe(res->port_number, res->subport_number, res->pipe_number, res->tc_number) < 0)
                printf ("\nStats not available for these parameters. Check that both the port and subport are correct.\n\n");
}

cmdline_parse_token_string_t cmd_avg_tcpipe_qavg_string =
        TOKEN_STRING_INITIALIZER(struct cmd_avg_tcpipe_result, qavg_string,
                                "qavg");
cmdline_parse_token_string_t cmd_avg_tcpipe_port_string =
        TOKEN_STRING_INITIALIZER(struct cmd_avg_tcpipe_result, port_string,
                                "port");
cmdline_parse_token_num_t cmd_avg_tcpipe_port_number =
        TOKEN_NUM_INITIALIZER(struct cmd_avg_tcpipe_result, port_number,
                                UINT8);
cmdline_parse_token_string_t cmd_avg_tcpipe_subport_string =
        TOKEN_STRING_INITIALIZER(struct cmd_avg_tcpipe_result, subport_string,
                                "subport");
cmdline_parse_token_num_t cmd_avg_tcpipe_subport_number =
        TOKEN_NUM_INITIALIZER(struct cmd_avg_tcpipe_result, subport_number,
                                UINT32);
cmdline_parse_token_string_t cmd_avg_tcpipe_pipe_string =
        TOKEN_STRING_INITIALIZER(struct cmd_avg_tcpipe_result, pipe_string,
                                "pipe");
cmdline_parse_token_num_t cmd_avg_tcpipe_pipe_number =
        TOKEN_NUM_INITIALIZER(struct cmd_avg_tcpipe_result, pipe_number,
                                UINT32);
cmdline_parse_token_string_t cmd_avg_tcpipe_tc_string =
        TOKEN_STRING_INITIALIZER(struct cmd_avg_tcpipe_result, tc_string,
                                "tc");
cmdline_parse_token_num_t cmd_avg_tcpipe_tc_number =
        TOKEN_NUM_INITIALIZER(struct cmd_avg_tcpipe_result, tc_number,
                                UINT8);

cmdline_parse_inst_t cmd_avg_tcpipe = {
        .f = cmd_avg_tcpipe_parsed,
        .data = NULL,
        .help_str = "Show pipe stats.",
        .tokens = {
                (void *)&cmd_avg_tcpipe_qavg_string,
                (void *)&cmd_avg_tcpipe_port_string,
                (void *)&cmd_avg_tcpipe_port_number,
                (void *)&cmd_avg_tcpipe_subport_string,
                (void *)&cmd_avg_tcpipe_subport_number,
                (void *)&cmd_avg_tcpipe_pipe_string,
                (void *)&cmd_avg_tcpipe_pipe_number,
                (void *)&cmd_avg_tcpipe_tc_string,
                (void *)&cmd_avg_tcpipe_tc_number,
                NULL,
        },
};

/* *** SHOW AVERAGE QUEUE SIZE (pipe) *** */
struct cmd_avg_pipe_result {
        cmdline_fixed_string_t qavg_string;
        cmdline_fixed_string_t port_string;
        uint8_t port_number;
        cmdline_fixed_string_t subport_string;
        uint32_t subport_number;
        cmdline_fixed_string_t pipe_string;
        uint32_t pipe_number;
};

static void cmd_avg_pipe_parsed(void *parsed_result,
                                __attribute__((unused)) struct cmdline *cl,
                                __attribute__((unused)) void *data)
{
        struct cmd_avg_pipe_result *res = parsed_result;

        if (qavg_pipe(res->port_number, res->subport_number, res->pipe_number) < 0)
                printf ("\nStats not available for these parameters. Check that both the port and subport are correct.\n\n");
}

cmdline_parse_token_string_t cmd_avg_pipe_qavg_string =
        TOKEN_STRING_INITIALIZER(struct cmd_avg_pipe_result, qavg_string,
                                "qavg");
cmdline_parse_token_string_t cmd_avg_pipe_port_string =
        TOKEN_STRING_INITIALIZER(struct cmd_avg_pipe_result, port_string,
                                "port");
cmdline_parse_token_num_t cmd_avg_pipe_port_number =
        TOKEN_NUM_INITIALIZER(struct cmd_avg_pipe_result, port_number,
                                UINT8);
cmdline_parse_token_string_t cmd_avg_pipe_subport_string =
        TOKEN_STRING_INITIALIZER(struct cmd_avg_pipe_result, subport_string,
                                "subport");
cmdline_parse_token_num_t cmd_avg_pipe_subport_number =
        TOKEN_NUM_INITIALIZER(struct cmd_avg_pipe_result, subport_number,
                                UINT32);
cmdline_parse_token_string_t cmd_avg_pipe_pipe_string =
        TOKEN_STRING_INITIALIZER(struct cmd_avg_pipe_result, pipe_string,
                                "pipe");
cmdline_parse_token_num_t cmd_avg_pipe_pipe_number =
        TOKEN_NUM_INITIALIZER(struct cmd_avg_pipe_result, pipe_number,
                                UINT32);

cmdline_parse_inst_t cmd_avg_pipe = {
        .f = cmd_avg_pipe_parsed,
        .data = NULL,
        .help_str = "Show pipe stats.",
        .tokens = {
                (void *)&cmd_avg_pipe_qavg_string,
                (void *)&cmd_avg_pipe_port_string,
                (void *)&cmd_avg_pipe_port_number,
                (void *)&cmd_avg_pipe_subport_string,
                (void *)&cmd_avg_pipe_subport_number,
                (void *)&cmd_avg_pipe_pipe_string,
                (void *)&cmd_avg_pipe_pipe_number,
                NULL,
        },
};

/* *** SHOW AVERAGE QUEUE SIZE (tc/subport) *** */
struct cmd_avg_tcsubport_result {
        cmdline_fixed_string_t qavg_string;
        cmdline_fixed_string_t port_string;
        uint8_t port_number;
        cmdline_fixed_string_t subport_string;
        uint32_t subport_number;
        cmdline_fixed_string_t tc_string;
        uint8_t tc_number;
};

static void cmd_avg_tcsubport_parsed(void *parsed_result,
                                __attribute__((unused)) struct cmdline *cl,
                                __attribute__((unused)) void *data)
{
        struct cmd_avg_tcsubport_result *res = parsed_result;

        if (qavg_tcsubport(res->port_number, res->subport_number, res->tc_number) < 0)
                printf ("\nStats not available for these parameters. Check that both the port and subport are correct.\n\n");
}

cmdline_parse_token_string_t cmd_avg_tcsubport_qavg_string =
        TOKEN_STRING_INITIALIZER(struct cmd_avg_tcsubport_result, qavg_string,
                                "qavg");
cmdline_parse_token_string_t cmd_avg_tcsubport_port_string =
        TOKEN_STRING_INITIALIZER(struct cmd_avg_tcsubport_result, port_string,
                                "port");
cmdline_parse_token_num_t cmd_avg_tcsubport_port_number =
        TOKEN_NUM_INITIALIZER(struct cmd_avg_tcsubport_result, port_number,
                                UINT8);
cmdline_parse_token_string_t cmd_avg_tcsubport_subport_string =
        TOKEN_STRING_INITIALIZER(struct cmd_avg_tcsubport_result, subport_string,
                                "subport");
cmdline_parse_token_num_t cmd_avg_tcsubport_subport_number =
        TOKEN_NUM_INITIALIZER(struct cmd_avg_tcsubport_result, subport_number,
                                UINT32);
cmdline_parse_token_string_t cmd_avg_tcsubport_tc_string =
        TOKEN_STRING_INITIALIZER(struct cmd_avg_tcsubport_result, tc_string,
                                "tc");
cmdline_parse_token_num_t cmd_avg_tcsubport_tc_number =
        TOKEN_NUM_INITIALIZER(struct cmd_avg_tcsubport_result, tc_number,
                                UINT8);

cmdline_parse_inst_t cmd_avg_tcsubport = {
        .f = cmd_avg_tcsubport_parsed,
        .data = NULL,
        .help_str = "Show pipe stats.",
        .tokens = {
                (void *)&cmd_avg_tcsubport_qavg_string,
                (void *)&cmd_avg_tcsubport_port_string,
                (void *)&cmd_avg_tcsubport_port_number,
                (void *)&cmd_avg_tcsubport_subport_string,
                (void *)&cmd_avg_tcsubport_subport_number,
                (void *)&cmd_avg_tcsubport_tc_string,
                (void *)&cmd_avg_tcsubport_tc_number,
                NULL,
        },
};

/* *** SHOW AVERAGE QUEUE SIZE (subport) *** */
struct cmd_avg_subport_result {
        cmdline_fixed_string_t qavg_string;
        cmdline_fixed_string_t port_string;
        uint8_t port_number;
        cmdline_fixed_string_t subport_string;
        uint32_t subport_number;
};

static void cmd_avg_subport_parsed(void *parsed_result,
                                __attribute__((unused)) struct cmdline *cl,
                                __attribute__((unused)) void *data)
{
        struct cmd_avg_subport_result *res = parsed_result;

        if (qavg_subport(res->port_number, res->subport_number) < 0)
                printf ("\nStats not available for these parameters. Check that both the port and subport are correct.\n\n");
}

cmdline_parse_token_string_t cmd_avg_subport_qavg_string =
        TOKEN_STRING_INITIALIZER(struct cmd_avg_subport_result, qavg_string,
                                "qavg");
cmdline_parse_token_string_t cmd_avg_subport_port_string =
        TOKEN_STRING_INITIALIZER(struct cmd_avg_subport_result, port_string,
                                "port");
cmdline_parse_token_num_t cmd_avg_subport_port_number =
        TOKEN_NUM_INITIALIZER(struct cmd_avg_subport_result, port_number,
                                UINT8);
cmdline_parse_token_string_t cmd_avg_subport_subport_string =
        TOKEN_STRING_INITIALIZER(struct cmd_avg_subport_result, subport_string,
                                "subport");
cmdline_parse_token_num_t cmd_avg_subport_subport_number =
        TOKEN_NUM_INITIALIZER(struct cmd_avg_subport_result, subport_number,
                                UINT32);

cmdline_parse_inst_t cmd_avg_subport = {
        .f = cmd_avg_subport_parsed,
        .data = NULL,
        .help_str = "Show pipe stats.",
        .tokens = {
                (void *)&cmd_avg_subport_qavg_string,
                (void *)&cmd_avg_subport_port_string,
                (void *)&cmd_avg_subport_port_number,
                (void *)&cmd_avg_subport_subport_string,
                (void *)&cmd_avg_subport_subport_number,
                NULL,
        },
};

/* ******************************************************************************** */

/* list of instructions */
cmdline_parse_ctx_t main_ctx[] = {
	(cmdline_parse_inst_t *)&cmd_help,
	(cmdline_parse_inst_t *)&cmd_setqavg,
	(cmdline_parse_inst_t *)&cmd_appstats,
	(cmdline_parse_inst_t *)&cmd_subportstats,
        (cmdline_parse_inst_t *)&cmd_pipestats,
	(cmdline_parse_inst_t *)&cmd_avg_q,
	(cmdline_parse_inst_t *)&cmd_avg_tcpipe,
	(cmdline_parse_inst_t *)&cmd_avg_pipe,
	(cmdline_parse_inst_t *)&cmd_avg_tcsubport,
	(cmdline_parse_inst_t *)&cmd_avg_subport,
	(cmdline_parse_inst_t *)&cmd_quit,
	NULL,
};

/* prompt function, called from main on MASTER lcore */
void
prompt(void)
{
	struct cmdline *cl;

	cl = cmdline_stdin_new(main_ctx, "qos_sched> ");
	if (cl == NULL) {
		return;
	}
	cmdline_interact(cl);
	cmdline_stdin_exit(cl);
}
