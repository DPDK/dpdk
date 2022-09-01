/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_string_fns.h>

#include "rte_eth_softnic_internals.h"

#ifndef CMD_MAX_TOKENS
#define CMD_MAX_TOKENS     256
#endif

#ifndef MAX_LINE_SIZE
#define MAX_LINE_SIZE 2048
#endif

#define MSG_OUT_OF_MEMORY   "Not enough memory.\n"
#define MSG_CMD_UNKNOWN     "Unknown command \"%s\".\n"
#define MSG_CMD_UNIMPLEM    "Command \"%s\" not implemented.\n"
#define MSG_ARG_NOT_ENOUGH  "Not enough arguments for command \"%s\".\n"
#define MSG_ARG_TOO_MANY    "Too many arguments for command \"%s\".\n"
#define MSG_ARG_MISMATCH    "Wrong number of arguments for command \"%s\".\n"
#define MSG_ARG_NOT_FOUND   "Argument \"%s\" not found.\n"
#define MSG_ARG_INVALID     "Invalid value for argument \"%s\".\n"
#define MSG_FILE_ERR        "Error in file \"%s\" at line %u.\n"
#define MSG_FILE_NOT_ENOUGH "Not enough rules in file \"%s\".\n"
#define MSG_CMD_FAIL        "Command \"%s\" failed.\n"

static int
parser_read_uint32(uint32_t *value, char *p)
{
	uint32_t val = 0;

	if (!value || !p || !p[0])
		return -EINVAL;

	val = strtoul(p, &p, 0);
	if (p[0])
		return -EINVAL;

	*value = val;
	return 0;
}

#define PARSE_DELIMITER " \f\n\r\t\v"

static int
parse_tokenize_string(char *string, char *tokens[], uint32_t *n_tokens)
{
	uint32_t i;

	if (!string || !tokens || !n_tokens || !*n_tokens)
		return -EINVAL;

	for (i = 0; i < *n_tokens; i++) {
		tokens[i] = strtok_r(string, PARSE_DELIMITER, &string);
		if (!tokens[i])
			break;
	}

	if (i == *n_tokens && strtok_r(string, PARSE_DELIMITER, &string))
		return -E2BIG;

	*n_tokens = i;
	return 0;
}

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
 */
static void
cmd_mempool(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct softnic_mempool_params p;
	char *name;
	struct softnic_mempool *mempool;

	if (n_tokens != 8) {
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

	mempool = softnic_mempool_create(softnic, name, &p);
	if (mempool == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * swq <swq_name>
 *  size <size>
 */
static void
cmd_swq(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct softnic_swq_params p;
	char *name;
	struct softnic_swq *swq;

	if (n_tokens != 4) {
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

	swq = softnic_swq_create(softnic, name, &p);
	if (swq == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline codegen <spec_file> <code_file>
 */
static void
cmd_softnic_pipeline_codegen(struct pmd_internals *softnic __rte_unused,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	FILE *spec_file = NULL;
	FILE *code_file = NULL;
	uint32_t err_line;
	const char *err_msg;
	int status;

	if (n_tokens != 4) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	spec_file = fopen(tokens[2], "r");
	if (!spec_file) {
		snprintf(out, out_size, "Cannot open file %s.\n", tokens[2]);
		return;
	}

	code_file = fopen(tokens[3], "w");
	if (!code_file) {
		snprintf(out, out_size, "Cannot open file %s.\n", tokens[3]);
		return;
	}

	status = rte_swx_pipeline_codegen(spec_file,
					  code_file,
					  &err_line,
					  &err_msg);

	fclose(spec_file);
	fclose(code_file);

	if (status) {
		snprintf(out, out_size, "Error %d at line %u: %s\n.",
			status, err_line, err_msg);
		return;
	}
}

/**
 * pipeline libbuild <code_file> <lib_file>
 */
static void
cmd_softnic_pipeline_libbuild(struct pmd_internals *softnic __rte_unused,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	char *code_file, *lib_file, *obj_file = NULL, *log_file = NULL;
	char *install_dir, *cwd = NULL, *buffer = NULL;
	size_t length;
	int status = 0;

	if (n_tokens != 4) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		goto free;
	}

	install_dir = getenv("RTE_INSTALL_DIR");
	if (!install_dir) {
		cwd = malloc(MAX_LINE_SIZE);
		if (!cwd) {
			snprintf(out, out_size, MSG_OUT_OF_MEMORY);
			goto free;
		}

		install_dir = getcwd(cwd, MAX_LINE_SIZE);
		if (!install_dir) {
			snprintf(out, out_size, "Error: Path too long.\n");
			goto free;
		}
	}

	snprintf(out, out_size, "Using DPDK source code from \"%s\".\n", install_dir);
	out_size -= strlen(out);
	out += strlen(out);

	code_file = tokens[2];
	length = strnlen(code_file, MAX_LINE_SIZE);
	if (length < 3 ||
	    code_file[length - 2] != '.' ||
	    code_file[length - 1] != 'c') {
		snprintf(out, out_size, MSG_ARG_INVALID, "code_file");
		goto free;
	}

	lib_file = tokens[3];
	length = strnlen(lib_file, MAX_LINE_SIZE);
	if (length < 4 ||
	    lib_file[length - 3] != '.' ||
	    lib_file[length - 2] != 's' ||
	    lib_file[length - 1] != 'o') {
		snprintf(out, out_size, MSG_ARG_INVALID, "lib_file");
		goto free;
	}

	obj_file = malloc(length);
	log_file = malloc(length + 2);
	if (!obj_file || !log_file) {
		snprintf(out, out_size, MSG_OUT_OF_MEMORY);
		goto free;
	}

	memcpy(obj_file, lib_file, length - 2);
	obj_file[length - 2] = 'o';
	obj_file[length - 1] = 0;

	memcpy(log_file, lib_file, length - 2);
	log_file[length - 2] = 'l';
	log_file[length - 1] = 'o';
	log_file[length] = 'g';
	log_file[length + 1] = 0;

	buffer = malloc(MAX_LINE_SIZE);
	if (!buffer) {
		snprintf(out, out_size, MSG_OUT_OF_MEMORY);
		return;
	}

	snprintf(buffer,
		 MAX_LINE_SIZE,
		 "gcc -c -O3 -fpic -Wno-deprecated-declarations -o %s %s "
		 "-I %s/lib/pipeline "
		 "-I %s/lib/eal/include "
		 "-I %s/lib/eal/x86/include "
		 "-I %s/lib/eal/include/generic "
		 "-I %s/lib/meter "
		 "-I %s/lib/port "
		 "-I %s/lib/table "
		 "-I %s/lib/pipeline "
		 "-I %s/config "
		 "-I %s/build "
		 "-I %s/lib/eal/linux/include "
		 ">%s 2>&1 "
		 "&& "
		 "gcc -shared %s -o %s "
		 ">>%s 2>&1",
		 obj_file,
		 code_file,
		 install_dir,
		 install_dir,
		 install_dir,
		 install_dir,
		 install_dir,
		 install_dir,
		 install_dir,
		 install_dir,
		 install_dir,
		 install_dir,
		 install_dir,
		 log_file,
		 obj_file,
		 lib_file,
		 log_file);

	status = system(buffer);
	if (status) {
		snprintf(out,
			 out_size,
			 "Library build failed, see file \"%s\" for details.\n",
			 log_file);
		goto free;
	}

free:
	free(cwd);
	free(obj_file);
	free(log_file);
	free(buffer);
}

/**
 * pipeline <pipeline_name> build lib <lib_file> io <iospec_file> numa <numa_node>
 */
static void
cmd_softnic_pipeline_build(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct pipeline *p = NULL;
	char *pipeline_name, *lib_file_name, *iospec_file_name;
	uint32_t numa_node = 0;

	/* Parsing. */
	if (n_tokens != 9) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "build")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "build");
		return;
	}

	if (strcmp(tokens[3], "lib")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "lib");
		return;
	}

	lib_file_name = tokens[4];

	if (strcmp(tokens[5], "io")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "io");
		return;
	}

	iospec_file_name = tokens[6];

	if (strcmp(tokens[7], "numa")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "numa");
		return;
	}

	if (parser_read_uint32(&numa_node, tokens[8])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "numa_node");
		return;
	}

	/* Pipeline create. */
	p = softnic_pipeline_create(softnic,
				    pipeline_name,
				    lib_file_name,
				    iospec_file_name,
				    (int)numa_node);
	if (!p)
		snprintf(out, out_size, "Pipeline creation failed.\n");
}

/**
 * thread <thread_id> pipeline <pipeline_name> enable [ period <timer_period_ms> ]
 */
static void
cmd_softnic_thread_pipeline_enable(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	char *pipeline_name;
	struct pipeline *p;
	uint32_t thread_id;
	int status;

	if (n_tokens != 5) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (parser_read_uint32(&thread_id, tokens[1]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "thread_id");
		return;
	}

	if (strcmp(tokens[2], "pipeline") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pipeline");
		return;
	}

	pipeline_name = tokens[3];
	p = softnic_pipeline_find(softnic, pipeline_name);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[4], "enable") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "enable");
		return;
	}

	status = softnic_thread_pipeline_enable(softnic, thread_id, p);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, "thread pipeline enable");
		return;
	}
}

/**
 * thread <thread_id> pipeline <pipeline_name> disable
 */
static void
cmd_softnic_thread_pipeline_disable(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	char *pipeline_name;
	struct pipeline *p;
	uint32_t thread_id;
	int status;

	if (n_tokens != 5) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (parser_read_uint32(&thread_id, tokens[1]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "thread_id");
		return;
	}

	if (strcmp(tokens[2], "pipeline") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pipeline");
		return;
	}

	pipeline_name = tokens[3];
	p = softnic_pipeline_find(softnic, pipeline_name);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[4], "disable") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "disable");
		return;
	}

	status = softnic_thread_pipeline_disable(softnic, thread_id, p);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL,
			"thread pipeline disable");
		return;
	}
}

void
softnic_cli_process(char *in, char *out, size_t out_size, void *arg)
{
	char *tokens[CMD_MAX_TOKENS];
	uint32_t n_tokens = RTE_DIM(tokens);
	struct pmd_internals *softnic = arg;
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
		cmd_mempool(softnic, tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "swq") == 0) {
		cmd_swq(softnic, tokens, n_tokens, out, out_size);
		return;
	}

	if (!strcmp(tokens[0], "pipeline")) {
		if (n_tokens >= 2 && !strcmp(tokens[1], "codegen")) {
			cmd_softnic_pipeline_codegen(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 3 && !strcmp(tokens[1], "libbuild")) {
			cmd_softnic_pipeline_libbuild(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 3 && !strcmp(tokens[2], "build")) {
			cmd_softnic_pipeline_build(softnic, tokens, n_tokens, out, out_size);
			return;
		}
	}

	if (strcmp(tokens[0], "thread") == 0) {
		if (n_tokens >= 5 &&
			(strcmp(tokens[4], "enable") == 0)) {
			cmd_softnic_thread_pipeline_enable(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 5 &&
			(strcmp(tokens[4], "disable") == 0)) {
			cmd_softnic_thread_pipeline_disable(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}
	}

	snprintf(out, out_size, MSG_CMD_UNKNOWN, tokens[0]);
}

int
softnic_cli_script_process(struct pmd_internals *softnic,
	const char *file_name,
	size_t msg_in_len_max,
	size_t msg_out_len_max)
{
	char *msg_in = NULL, *msg_out = NULL;
	FILE *f = NULL;

	/* Check input arguments */
	if (file_name == NULL ||
		(strlen(file_name) == 0) ||
		msg_in_len_max == 0 ||
		msg_out_len_max == 0)
		return -EINVAL;

	msg_in = malloc(msg_in_len_max + 1);
	msg_out = malloc(msg_out_len_max + 1);
	if (msg_in == NULL ||
		msg_out == NULL) {
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

		softnic_cli_process(msg_in,
			msg_out,
			msg_out_len_max,
			softnic);

		if (strlen(msg_out))
			printf("%s", msg_out);
	}

	/* Close file */
	fclose(f);
	free(msg_out);
	free(msg_in);
	return 0;
}
