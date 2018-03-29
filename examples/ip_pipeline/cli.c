/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_common.h>

#include "cli.h"
#include "mempool.h"
#include "parser.h"

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
