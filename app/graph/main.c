/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_launch.h>

#include "module_api.h"

volatile bool force_quit;
struct conn *conn;

static const char usage[] = "%s EAL_ARGS -- -s SCRIPT [-h HOST] [-p PORT] [--enable-graph-stats] "
			    "[--help]\n";

static struct app_params {
	struct conn_params conn;
	char *script_name;
	bool enable_graph_stats;
} app = {
	.conn = {
		.welcome = "\nWelcome!\n\n",
		.prompt = "graph> ",
		.addr = "0.0.0.0",
		.port = 8086,
		.buf_size = 1024 * 1024,
		.msg_in_len_max = 1024,
		.msg_out_len_max = 1024 * 1024,
		.msg_handle = cli_process,
		.msg_handle_arg = NULL, /* set later. */
	},
	.script_name = NULL,
	.enable_graph_stats = false,
};

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n", signum);
		force_quit = true;
	}
}

static int
app_args_parse(int argc, char **argv)
{
	struct option lgopts[] = {
		{"help", 0, 0, 'H'},
		{"enable-graph-stats", 0, 0, 'g'},
	};
	int h_present, p_present, s_present, n_args, i;
	char *app_name = argv[0];
	int opt, option_index;

	/* Skip EAL input args */
	n_args = argc;
	for (i = 0; i < n_args; i++)
		if (strcmp(argv[i], "--") == 0) {
			argc -= i;
			argv += i;
			break;
		}

	if (i == n_args)
		return 0;

	/* Parse args */
	h_present = 0;
	p_present = 0;
	s_present = 0;

	while ((opt = getopt_long(argc, argv, "h:p:s:", lgopts, &option_index)) != EOF) {
		switch (opt) {
		case 'h':
			if (h_present) {
				printf("Error: Multiple -h arguments\n");
				return -1;
			}
			h_present = 1;

			if (!strlen(optarg)) {
				printf("Error: Argument for -h not provided\n");
				return -1;
			}

			app.conn.addr = strdup(optarg);
			if (app.conn.addr == NULL) {
				printf("Error: Not enough memory\n");
				return -1;
			}
			break;

		case 'p':
			if (p_present) {
				printf("Error: Multiple -p arguments\n");
				return -1;
			}
			p_present = 1;

			if (!strlen(optarg)) {
				printf("Error: Argument for -p not provided\n");
				return -1;
			}

			app.conn.port = (uint16_t)strtoul(optarg, NULL, 10);
			break;

		case 's':
			if (s_present) {
				printf("Error: Multiple -s arguments\n");
				return -1;
			}
			s_present = 1;

			if (!strlen(optarg)) {
				printf("Error: Argument for -s not provided\n");
				return -1;
			}

			app.script_name = strdup(optarg);
			if (app.script_name == NULL) {
				printf("Error: Not enough memory\n");
				return -1;
			}
			break;

		case 'g':
			app.enable_graph_stats = true;
			printf("WARNING! Telnet session can not be accessed with"
			       "--enable-graph-stats");
			break;

		case 'H':
		default:
			printf(usage, app_name);
			return -1;
		}
	}
	optind = 1; /* reset getopt lib */

	return 0;
}

bool
app_graph_stats_enabled(void)
{
	return app.enable_graph_stats;
}

bool
app_graph_exit(void)
{
	struct timeval tv;
	fd_set fds;
	int ret;
	char c;

	FD_ZERO(&fds);
	FD_SET(0, &fds);
	tv.tv_sec = 0;
	tv.tv_usec = 100;
	ret = select(1, &fds, NULL, NULL, &tv);
	if ((ret < 0 && errno == EINTR) || (ret == 1 && read(0, &c, 1) > 0))
		return true;
	else
		return false;

}

int
main(int argc, char **argv)
{
	int rc;

	/* Parse application arguments */
	rc = app_args_parse(argc, argv);
	if (rc < 0)
		return rc;

	/* EAL */
	rc = rte_eal_init(argc, argv);
	if (rc < 0) {
		printf("Error: EAL initialization failed (%d)\n", rc);
		return rc;
	};

	force_quit = false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	cli_init();

	/* Script */
	if (app.script_name) {
		cli_script_process(app.script_name, app.conn.msg_in_len_max,
			app.conn.msg_out_len_max, NULL);
	}

	/* Connectivity */
	app.conn.msg_handle_arg = NULL;
	conn = conn_init(&app.conn);
	if (!conn) {
		printf("Error: Connectivity initialization failed\n");
		goto exit;
	};

	rte_delay_ms(1);
	printf("Press enter to exit\n");

	/* Dispatch loop */
	while (!force_quit) {
		conn_req_poll(conn);

		conn_msg_poll(conn);
		if (app_graph_exit())
			force_quit = true;
	}

exit:
	conn_free(conn);
	ethdev_stop();
	cli_exit();
	rte_eal_cleanup();
	return 0;
}
