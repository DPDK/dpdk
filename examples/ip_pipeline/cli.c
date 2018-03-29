/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_common.h>

#include "cli.h"
#include "link.h"
#include "mempool.h"
#include "parser.h"
#include "swq.h"

#ifndef CMD_MAX_TOKENS
#define CMD_MAX_TOKENS     256
#endif

#define MSG_OUT_OF_MEMORY  "Not enough memory.\n"
#define MSG_CMD_UNKNOWN    "Unknown command \"%s\".\n"
#define MSG_CMD_UNIMPLEM   "Command \"%s\" not implemented.\n"
#define MSG_ARG_NOT_ENOUGH "Not enough arguments for command \"%s\".\n"
#define MSG_ARG_TOO_MANY   "Too many arguments for command \"%s\".\n"
#define MSG_ARG_MISMATCH   "Wrong number of arguments for command \"%s\".\n"
#define MSG_ARG_NOT_FOUND  "Argument \"%s\" not found.\n"
#define MSG_ARG_INVALID    "Invalid value for argument \"%s\".\n"
#define MSG_FILE_ERR       "Error in file \"%s\" at line %u.\n"
#define MSG_CMD_FAIL       "Command \"%s\" failed.\n"

static int
is_comment(char *in)
{
	if ((strlen(in) && index("!#%;", in[0])) ||
		(strncmp(in, "//", 2) == 0) ||
		(strncmp(in, "--", 2) == 0))
		return 1;

	return 0;
}

/**
 * mempool <mempool_name>
 *  buffer <buffer_size>
 *  pool <pool_size>
 *  cache <cache_size>
 *  cpu <cpu_id>
 */
static void
cmd_mempool(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct mempool_params p;
	char *name;
	struct mempool *mempool;

	if (n_tokens != 10) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	if (strcmp(tokens[2], "buffer") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "buffer");
		return;
	}

	if (parser_read_uint32(&p.buffer_size, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "buffer_size");
		return;
	}

	if (strcmp(tokens[4], "pool") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pool");
		return;
	}

	if (parser_read_uint32(&p.pool_size, tokens[5]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pool_size");
		return;
	}

	if (strcmp(tokens[6], "cache") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cache");
		return;
	}

	if (parser_read_uint32(&p.cache_size, tokens[7]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "cache_size");
		return;
	}

	if (strcmp(tokens[8], "cpu") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cpu");
		return;
	}

	if (parser_read_uint32(&p.cpu_id, tokens[9]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "cpu_id");
		return;
	}

	mempool = mempool_create(name, &p);
	if (mempool == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * link <link_name>
 *  dev <device_name> | port <port_id>
 *  rxq <n_queues> <queue_size> <mempool_name>
 *  txq <n_queues> <queue_size>
 *  promiscuous on | off
 *  [rss <qid_0> ... <qid_n>]
 */
static void
cmd_link(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct link_params p;
	struct link_params_rss rss;
	struct link *link;
	char *name;

	if ((n_tokens < 13) || (n_tokens > 14 + LINK_RXQ_RSS_MAX)) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}
	name = tokens[1];

	if (strcmp(tokens[2], "dev") == 0)
		p.dev_name = tokens[3];
	else if (strcmp(tokens[2], "port") == 0) {
		p.dev_name = NULL;

		if (parser_read_uint16(&p.port_id, tokens[3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
			return;
		}
	} else {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "dev or port");
		return;
	}

	if (strcmp(tokens[4], "rxq") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rxq");
		return;
	}

	if (parser_read_uint32(&p.rx.n_queues, tokens[5]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "n_queues");
		return;
	}
	if (parser_read_uint32(&p.rx.queue_size, tokens[6]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "queue_size");
		return;
	}

	p.rx.mempool_name = tokens[7];

	if (strcmp(tokens[8], "txq") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "txq");
		return;
	}

	if (parser_read_uint32(&p.tx.n_queues, tokens[9]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "n_queues");
		return;
	}

	if (parser_read_uint32(&p.tx.queue_size, tokens[10]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "queue_size");
		return;
	}

	if (strcmp(tokens[11], "promiscuous") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "promiscuous");
		return;
	}

	if (strcmp(tokens[12], "on") == 0)
		p.promiscuous = 1;
	else if (strcmp(tokens[12], "off") == 0)
		p.promiscuous = 0;
	else {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "on or off");
		return;
	}

	/* RSS */
	p.rx.rss = NULL;
	if (n_tokens > 13) {
		uint32_t queue_id, i;

		if (strcmp(tokens[13], "rss") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rss");
			return;
		}

		p.rx.rss = &rss;

		rss.n_queues = 0;
		for (i = 14; i < n_tokens; i++) {
			if (parser_read_uint32(&queue_id, tokens[i]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"queue_id");
				return;
			}

			rss.queue_id[rss.n_queues] = queue_id;
			rss.n_queues++;
		}
	}

	link = link_create(name, &p);
	if (link == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * swq <swq_name>
 *  size <size>
 *  cpu <cpu_id>
 */
static void
cmd_swq(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct swq_params p;
	char *name;
	struct swq *swq;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	if (strcmp(tokens[2], "size") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "size");
		return;
	}

	if (parser_read_uint32(&p.size, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "size");
		return;
	}

	if (strcmp(tokens[4], "cpu") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cpu");
		return;
	}

	if (parser_read_uint32(&p.cpu_id, tokens[5]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "cpu_id");
		return;
	}

	swq = swq_create(name, &p);
	if (swq == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

void
cli_process(char *in, char *out, size_t out_size)
{
	char *tokens[CMD_MAX_TOKENS];
	uint32_t n_tokens = RTE_DIM(tokens);
	int status;

	if (is_comment(in))
		return;

	status = parse_tokenize_string(in, tokens, &n_tokens);
	if (status) {
		snprintf(out, out_size, MSG_ARG_TOO_MANY, "");
		return;
	}

	if (n_tokens == 0)
		return;

	if (strcmp(tokens[0], "mempool") == 0) {
		cmd_mempool(tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "link") == 0) {
		cmd_link(tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "swq") == 0) {
		cmd_swq(tokens, n_tokens, out, out_size);
		return;
	}

	snprintf(out, out_size, MSG_CMD_UNKNOWN, tokens[0]);
}

int
cli_script_process(const char *file_name,
	size_t msg_in_len_max,
	size_t msg_out_len_max)
{
	char *msg_in = NULL, *msg_out = NULL;
	FILE *f = NULL;

	/* Check input arguments */
	if ((file_name == NULL) ||
		(strlen(file_name) == 0) ||
		(msg_in_len_max == 0) ||
		(msg_out_len_max == 0))
		return -EINVAL;

	msg_in = malloc(msg_in_len_max + 1);
	msg_out = malloc(msg_out_len_max + 1);
	if ((msg_in == NULL) ||
		(msg_out == NULL)) {
		free(msg_out);
		free(msg_in);
		return -ENOMEM;
	}

	/* Open input file */
	f = fopen(file_name, "r");
	if (f == NULL) {
		free(msg_out);
		free(msg_in);
		return -EIO;
	}

	/* Read file */
	for ( ; ; ) {
		if (fgets(msg_in, msg_in_len_max + 1, f) == NULL)
			break;

		printf("%s", msg_in);
		msg_out[0] = 0;

		cli_process(msg_in,
			msg_out,
			msg_out_len_max);

		if (strlen(msg_out))
			printf("%s", msg_out);
	}

	/* Close file */
	fclose(f);
	free(msg_out);
	free(msg_in);
	return 0;
}
