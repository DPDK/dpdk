/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <getopt.h>

#include <cmdline_parse_string.h>
#include <cmdline_socket.h>
#include <cmdline.h>
#include <rte_common.h>
#include <rte_rawdev.h>
#include <rte_lcore.h>

#define NTB_DRV_NAME_LEN	7
static uint64_t max_file_size = 0x400000;
static uint8_t interactive = 1;
static uint16_t dev_id;

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
		"    quit                                      :"
		" Quit the application.\n"
		"\nFile transmit:\n"
		"    send [path]                               :"
		" Send [path] file. (No more than %"PRIu64")\n"
		"    recv [path]                            :"
		" Receive file to [path]. Make sure sending is done"
		" on the other side.\n",
		max_file_size
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
	/* Stop traffic and Close port. */
	rte_rawdev_stop(dev_id);
	rte_rawdev_close(dev_id);

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

/* *** SEND FILE PARAMETERS *** */
struct cmd_sendfile_result {
	cmdline_fixed_string_t send_string;
	char filepath[];
};

static void
cmd_sendfile_parsed(void *parsed_result,
		    __attribute__((unused)) struct cmdline *cl,
		    __attribute__((unused)) void *data)
{
	struct cmd_sendfile_result *res = parsed_result;
	struct rte_rawdev_buf *pkts_send[1];
	uint64_t rsize, size, link;
	uint8_t *buff;
	uint32_t val;
	FILE *file;

	if (!rte_rawdevs[dev_id].started) {
		printf("Device needs to be up first. Try later.\n");
		return;
	}

	rte_rawdev_get_attr(dev_id, "link_status", &link);
	if (!link) {
		printf("Link is not up, cannot send file.\n");
		return;
	}

	file = fopen(res->filepath, "r");
	if (file == NULL) {
		printf("Fail to open the file.\n");
		return;
	}

	fseek(file, 0, SEEK_END);
	size = ftell(file);
	fseek(file, 0, SEEK_SET);

	/**
	 * No FIFO now. Only test memory. Limit sending file
	 * size <= max_file_size.
	 */
	if (size > max_file_size) {
		printf("Warning: The file is too large. Only send first"
		       " %"PRIu64" bits.\n", max_file_size);
		size = max_file_size;
	}

	buff = (uint8_t *)malloc(size);
	rsize = fread(buff, size, 1, file);
	if (rsize != 1) {
		printf("Fail to read file.\n");
		fclose(file);
		free(buff);
		return;
	}

	/* Tell remote about the file size. */
	val = size >> 32;
	rte_rawdev_set_attr(dev_id, "spad_user_0", val);
	val = size;
	rte_rawdev_set_attr(dev_id, "spad_user_1", val);

	pkts_send[0] = (struct rte_rawdev_buf *)malloc
			(sizeof(struct rte_rawdev_buf));
	pkts_send[0]->buf_addr = buff;

	if (rte_rawdev_enqueue_buffers(dev_id, pkts_send, 1,
				       (void *)(size_t)size)) {
		printf("Fail to enqueue.\n");
		goto clean;
	}
	printf("Done sending file.\n");

clean:
	fclose(file);
	free(buff);
	free(pkts_send[0]);
}

cmdline_parse_token_string_t cmd_send_file_send =
	TOKEN_STRING_INITIALIZER(struct cmd_sendfile_result, send_string,
				 "send");
cmdline_parse_token_string_t cmd_send_file_filepath =
	TOKEN_STRING_INITIALIZER(struct cmd_sendfile_result, filepath, NULL);


cmdline_parse_inst_t cmd_send_file = {
	.f = cmd_sendfile_parsed,
	.data = NULL,
	.help_str = "send <file_path>",
	.tokens = {
		(void *)&cmd_send_file_send,
		(void *)&cmd_send_file_filepath,
		NULL,
	},
};

/* *** RECEIVE FILE PARAMETERS *** */
struct cmd_recvfile_result {
	cmdline_fixed_string_t recv_string;
	char filepath[];
};

static void
cmd_recvfile_parsed(void *parsed_result,
		    __attribute__((unused)) struct cmdline *cl,
		    __attribute__((unused)) void *data)
{
	struct cmd_sendfile_result *res = parsed_result;
	struct rte_rawdev_buf *pkts_recv[1];
	uint8_t *buff;
	uint64_t val;
	size_t size;
	FILE *file;

	if (!rte_rawdevs[dev_id].started) {
		printf("Device needs to be up first. Try later.\n");
		return;
	}

	rte_rawdev_get_attr(dev_id, "link_status", &val);
	if (!val) {
		printf("Link is not up, cannot receive file.\n");
		return;
	}

	file = fopen(res->filepath, "w");
	if (file == NULL) {
		printf("Fail to open the file.\n");
		return;
	}

	rte_rawdev_get_attr(dev_id, "spad_user_0", &val);
	size = val << 32;
	rte_rawdev_get_attr(dev_id, "spad_user_1", &val);
	size |= val;

	buff = (uint8_t *)malloc(size);
	pkts_recv[0] = (struct rte_rawdev_buf *)malloc
			(sizeof(struct rte_rawdev_buf));
	pkts_recv[0]->buf_addr = buff;

	if (rte_rawdev_dequeue_buffers(dev_id, pkts_recv, 1, (void *)size)) {
		printf("Fail to dequeue.\n");
		goto clean;
	}

	fwrite(buff, size, 1, file);
	printf("Done receiving to file.\n");

clean:
	fclose(file);
	free(buff);
	free(pkts_recv[0]);
}

cmdline_parse_token_string_t cmd_recv_file_recv =
	TOKEN_STRING_INITIALIZER(struct cmd_recvfile_result, recv_string,
				 "recv");
cmdline_parse_token_string_t cmd_recv_file_filepath =
	TOKEN_STRING_INITIALIZER(struct cmd_recvfile_result, filepath, NULL);


cmdline_parse_inst_t cmd_recv_file = {
	.f = cmd_recvfile_parsed,
	.data = NULL,
	.help_str = "recv <file_path>",
	.tokens = {
		(void *)&cmd_recv_file_recv,
		(void *)&cmd_recv_file_filepath,
		NULL,
	},
};

/* list of instructions */
cmdline_parse_ctx_t main_ctx[] = {
	(cmdline_parse_inst_t *)&cmd_help,
	(cmdline_parse_inst_t *)&cmd_send_file,
	(cmdline_parse_inst_t *)&cmd_recv_file,
	(cmdline_parse_inst_t *)&cmd_quit,
	NULL,
};

/* prompt function, called from main on MASTER lcore */
static void
prompt(void)
{
	struct cmdline *cl;

	cl = cmdline_stdin_new(main_ctx, "ntb> ");
	if (cl == NULL)
		return;

	cmdline_interact(cl);
	cmdline_stdin_exit(cl);
}

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\nSignal %d received, preparing to exit...\n", signum);
		signal(signum, SIG_DFL);
		kill(getpid(), signum);
	}
}

static void
ntb_usage(const char *prgname)
{
	printf("%s [EAL options] -- [options]\n"
	       "-i : run in interactive mode (default value is 1)\n",
	       prgname);
}

static int
parse_args(int argc, char **argv)
{
	char *prgname = argv[0], **argvopt = argv;
	int opt, ret;

	/* Only support interactive mode to send/recv file first. */
	while ((opt = getopt(argc, argvopt, "i")) != EOF) {
		switch (opt) {
		case 'i':
			printf("Interactive-mode selected\n");
			interactive = 1;
			break;

		default:
			ntb_usage(prgname);
			return -1;
		}
	}

	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 1; /* reset getopt lib */
	return ret;
}

int
main(int argc, char **argv)
{
	int ret, i;

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization.\n");

	/* Find 1st ntb rawdev. */
	for (i = 0; i < RTE_RAWDEV_MAX_DEVS; i++)
		if (rte_rawdevs[i].driver_name &&
		    (strncmp(rte_rawdevs[i].driver_name, "raw_ntb",
		    NTB_DRV_NAME_LEN) == 0) && (rte_rawdevs[i].attached == 1))
			break;

	if (i == RTE_RAWDEV_MAX_DEVS)
		rte_exit(EXIT_FAILURE, "Cannot find any ntb device.\n");

	dev_id = i;

	argc -= ret;
	argv += ret;

	ret = parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid arguments\n");

	rte_rawdev_start(dev_id);

	if (interactive) {
		sleep(1);
		prompt();
	}

	return 0;
}
