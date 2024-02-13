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
parser_read_uint64(uint64_t *value, char *p)
{
	uint64_t val = 0;

	if (!value || !p || !p[0])
		return -EINVAL;

	val = strtoull(p, &p, 0);
	if (p[0])
		return -EINVAL;

	*value = val;
	return 0;
}

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
		fclose(spec_file);
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
		goto free;
	}

	snprintf(buffer,
		 MAX_LINE_SIZE,
		 "gcc -c -O3 -fpic -Wno-deprecated-declarations -o %s %s "
		 "-I %s/lib/pipeline "
		 "-I %s/lib/eal/include "
		 "-I %s/lib/eal/x86/include "
		 "-I %s/lib/eal/include/generic "
		 "-I %s/lib/log "
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

static void
table_entry_free(struct rte_swx_table_entry *entry)
{
	if (!entry)
		return;

	free(entry->key);
	free(entry->key_mask);
	free(entry->action_data);
	free(entry);
}

static int
pipeline_table_entries_add(struct rte_swx_ctl_pipeline *p,
			   const char *table_name,
			   FILE *file,
			   uint32_t *file_line_number)
{
	char *line = NULL;
	uint32_t line_id = 0;
	int status = 0;

	/* Buffer allocation. */
	line = malloc(MAX_LINE_SIZE);
	if (!line)
		return -ENOMEM;

	/* File read. */
	for (line_id = 1; ; line_id++) {
		struct rte_swx_table_entry *entry;
		int is_blank_or_comment;

		if (fgets(line, MAX_LINE_SIZE, file) == NULL)
			break;

		entry = rte_swx_ctl_pipeline_table_entry_read(p,
							      table_name,
							      line,
							      &is_blank_or_comment);
		if (!entry) {
			if (is_blank_or_comment)
				continue;

			status = -EINVAL;
			goto error;
		}

		status = rte_swx_ctl_pipeline_table_entry_add(p,
							      table_name,
							      entry);
		table_entry_free(entry);
		if (status)
			goto error;
	}

error:
	free(line);
	*file_line_number = line_id;
	return status;
}

/**
 * pipeline <pipeline_name> table <table_name> add <file_name>
 */
static void
cmd_softnic_pipeline_table_add(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct pipeline *p;
	char *pipeline_name, *table_name, *file_name;
	FILE *file = NULL;
	uint32_t file_line_number = 0;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = softnic_pipeline_find(softnic, pipeline_name);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	table_name = tokens[3];

	file_name = tokens[5];
	file = fopen(file_name, "r");
	if (!file) {
		snprintf(out, out_size, "Cannot open file %s.\n", file_name);
		return;
	}

	status = pipeline_table_entries_add(p->ctl,
					    table_name,
					    file,
					    &file_line_number);
	if (status)
		snprintf(out, out_size, "Invalid entry in file %s at line %u\n",
			 file_name,
			 file_line_number);

	fclose(file);
}

static int
pipeline_table_entries_delete(struct rte_swx_ctl_pipeline *p,
			      const char *table_name,
			      FILE *file,
			      uint32_t *file_line_number)
{
	char *line = NULL;
	uint32_t line_id = 0;
	int status = 0;

	/* Buffer allocation. */
	line = malloc(MAX_LINE_SIZE);
	if (!line)
		return -ENOMEM;

	/* File read. */
	for (line_id = 1; ; line_id++) {
		struct rte_swx_table_entry *entry;
		int is_blank_or_comment;

		if (fgets(line, MAX_LINE_SIZE, file) == NULL)
			break;

		entry = rte_swx_ctl_pipeline_table_entry_read(p,
							      table_name,
							      line,
							      &is_blank_or_comment);
		if (!entry) {
			if (is_blank_or_comment)
				continue;

			status = -EINVAL;
			goto error;
		}

		status = rte_swx_ctl_pipeline_table_entry_delete(p,
								 table_name,
								 entry);
		table_entry_free(entry);
		if (status)
			goto error;
	}

error:
	*file_line_number = line_id;
	free(line);
	return status;
}

/**
 * pipeline <pipeline_name> table <table_name> delete <file_name>
 */
static void
cmd_softnic_pipeline_table_delete(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct pipeline *p;
	char *pipeline_name, *table_name, *file_name;
	FILE *file = NULL;
	uint32_t file_line_number = 0;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = softnic_pipeline_find(softnic, pipeline_name);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	table_name = tokens[3];

	file_name = tokens[5];
	file = fopen(file_name, "r");
	if (!file) {
		snprintf(out, out_size, "Cannot open file %s.\n", file_name);
		return;
	}

	status = pipeline_table_entries_delete(p->ctl,
					       table_name,
					       file,
					       &file_line_number);
	if (status)
		snprintf(out, out_size, "Invalid entry in file %s at line %u\n",
			 file_name,
			 file_line_number);

	fclose(file);
}

static int
pipeline_table_default_entry_add(struct rte_swx_ctl_pipeline *p,
				 const char *table_name,
				 FILE *file,
				 uint32_t *file_line_number)
{
	char *line = NULL;
	uint32_t line_id = 0;
	int status = 0;

	/* Buffer allocation. */
	line = malloc(MAX_LINE_SIZE);
	if (!line)
		return -ENOMEM;

	/* File read. */
	for (line_id = 1; ; line_id++) {
		struct rte_swx_table_entry *entry;
		int is_blank_or_comment;

		if (fgets(line, MAX_LINE_SIZE, file) == NULL)
			break;

		entry = rte_swx_ctl_pipeline_table_entry_read(p,
							      table_name,
							      line,
							      &is_blank_or_comment);
		if (!entry) {
			if (is_blank_or_comment)
				continue;

			status = -EINVAL;
			goto error;
		}

		status = rte_swx_ctl_pipeline_table_default_entry_add(p,
								      table_name,
								      entry);
		table_entry_free(entry);
		if (status)
			goto error;
	}

error:
	*file_line_number = line_id;
	free(line);
	return status;
}

/**
 * pipeline <pipeline_name> table <table_name> default <file_name>
 */
static void
cmd_softnic_pipeline_table_default(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct pipeline *p;
	char *pipeline_name, *table_name, *file_name;
	FILE *file = NULL;
	uint32_t file_line_number = 0;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = softnic_pipeline_find(softnic, pipeline_name);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	table_name = tokens[3];

	file_name = tokens[5];
	file = fopen(file_name, "r");
	if (!file) {
		snprintf(out, out_size, "Cannot open file %s.\n", file_name);
		return;
	}

	status = pipeline_table_default_entry_add(p->ctl,
						  table_name,
						  file,
						  &file_line_number);
	if (status)
		snprintf(out, out_size, "Invalid entry in file %s at line %u\n",
			 file_name,
			 file_line_number);

	fclose(file);
}

/**
 * pipeline <pipeline_name> table <table_name> show [filename]
 */
static void
cmd_softnic_pipeline_table_show(struct pmd_internals *softnic __rte_unused,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct pipeline *p;
	char *pipeline_name, *table_name;
	FILE *file = NULL;
	int status;

	if (n_tokens != 5 && n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = softnic_pipeline_find(softnic, pipeline_name);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	table_name = tokens[3];
	file = (n_tokens == 6) ? fopen(tokens[5], "w") : stdout;
	if (!file) {
		snprintf(out, out_size, "Cannot open file %s.\n", tokens[5]);
		return;
	}

	status = rte_swx_ctl_pipeline_table_fprintf(file, p->ctl, table_name);
	if (status)
		snprintf(out, out_size, MSG_ARG_INVALID, "table_name");

	if (file)
		fclose(file);
}

/**
 * pipeline <pipeline_name> selector <selector_name> group add
 */
static void
cmd_softnic_pipeline_selector_group_add(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct pipeline *p;
	char *pipeline_name, *selector_name;
	uint32_t group_id;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = softnic_pipeline_find(softnic, pipeline_name);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "selector") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "selector");
		return;
	}

	selector_name = tokens[3];

	if (strcmp(tokens[4], "group") ||
		strcmp(tokens[5], "add")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "group add");
		return;
	}

	status = rte_swx_ctl_pipeline_selector_group_add(p->ctl,
		selector_name,
		&group_id);
	if (status)
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
	else
		snprintf(out, out_size, "Group ID: %u\n", group_id);
}

/**
 * pipeline <pipeline_name> selector <selector_name> group delete <group_id>
 */
static void
cmd_softnic_pipeline_selector_group_delete(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct pipeline *p;
	char *pipeline_name, *selector_name;
	uint32_t group_id;
	int status;

	if (n_tokens != 7) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = softnic_pipeline_find(softnic, pipeline_name);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "selector") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "selector");
		return;
	}

	selector_name = tokens[3];

	if (strcmp(tokens[4], "group") ||
		strcmp(tokens[5], "delete")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "group delete");
		return;
	}

	if (parser_read_uint32(&group_id, tokens[6]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "group_id");
		return;
	}

	status = rte_swx_ctl_pipeline_selector_group_delete(p->ctl,
		selector_name,
		group_id);
	if (status)
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
}

#define GROUP_MEMBER_INFO_TOKENS_MAX 6

static int
token_is_comment(const char *token)
{
	if ((token[0] == '#') ||
	    (token[0] == ';') ||
	    ((token[0] == '/') && (token[1] == '/')))
		return 1; /* TRUE. */

	return 0; /* FALSE. */
}

static int
pipeline_selector_group_member_read(const char *string,
				    uint32_t *group_id,
				    uint32_t *member_id,
				    uint32_t *weight,
				    int *is_blank_or_comment)
{
	char *token_array[GROUP_MEMBER_INFO_TOKENS_MAX], **tokens;
	char *s0 = NULL, *s;
	uint32_t n_tokens = 0, group_id_val = 0, member_id_val = 0, weight_val = 0;
	int blank_or_comment = 0;

	/* Check input arguments. */
	if (!string || !string[0])
		goto error;

	/* Memory allocation. */
	s0 = strdup(string);
	if (!s0)
		goto error;

	/* Parse the string into tokens. */
	for (s = s0; ; ) {
		char *token;

		token = strtok_r(s, " \f\n\r\t\v", &s);
		if (!token || token_is_comment(token))
			break;

		if (n_tokens >= GROUP_MEMBER_INFO_TOKENS_MAX)
			goto error;

		token_array[n_tokens] = token;
		n_tokens++;
	}

	if (!n_tokens) {
		blank_or_comment = 1;
		goto error;
	}

	tokens = token_array;

	if (n_tokens < 4 ||
		strcmp(tokens[0], "group") ||
		strcmp(tokens[2], "member"))
		goto error;

	/*
	 * Group ID.
	 */
	if (parser_read_uint32(&group_id_val, tokens[1]) != 0)
		goto error;
	*group_id = group_id_val;

	/*
	 * Member ID.
	 */
	if (parser_read_uint32(&member_id_val, tokens[3]) != 0)
		goto error;
	*member_id = member_id_val;

	tokens += 4;
	n_tokens -= 4;

	/*
	 * Weight.
	 */
	if (n_tokens && !strcmp(tokens[0], "weight")) {
		if (n_tokens < 2)
			goto error;

		if (parser_read_uint32(&weight_val, tokens[1]) != 0)
			goto error;
		*weight = weight_val;

		tokens += 2;
		n_tokens -= 2;
	}

	if (n_tokens)
		goto error;

	free(s0);
	return 0;

error:
	free(s0);
	if (is_blank_or_comment)
		*is_blank_or_comment = blank_or_comment;
	return -EINVAL;
}

static int
pipeline_selector_group_members_add(struct rte_swx_ctl_pipeline *p,
				    const char *selector_name,
				    FILE *file,
				    uint32_t *file_line_number)
{
	char *line = NULL;
	uint32_t line_id = 0;
	int status = 0;

	/* Buffer allocation. */
	line = malloc(MAX_LINE_SIZE);
	if (!line)
		return -ENOMEM;

	/* File read. */
	for (line_id = 1; ; line_id++) {
		uint32_t group_id, member_id, weight;
		int is_blank_or_comment;

		if (fgets(line, MAX_LINE_SIZE, file) == NULL)
			break;

		status = pipeline_selector_group_member_read(line,
							     &group_id,
							     &member_id,
							     &weight,
							     &is_blank_or_comment);
		if (status) {
			if (is_blank_or_comment)
				continue;

			goto error;
		}

		status = rte_swx_ctl_pipeline_selector_group_member_add(p,
			selector_name,
			group_id,
			member_id,
			weight);
		if (status)
			goto error;
	}

error:
	free(line);
	*file_line_number = line_id;
	return status;
}

/**
 * pipeline <pipeline_name> selector <selector_name> group member add <file_name>
 */
static void
cmd_softnic_pipeline_selector_group_member_add(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct pipeline *p;
	char *pipeline_name, *selector_name, *file_name;
	FILE *file = NULL;
	uint32_t file_line_number = 0;
	int status;

	if (n_tokens != 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = softnic_pipeline_find(softnic, pipeline_name);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "selector") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "selector");
		return;
	}

	selector_name = tokens[3];

	if (strcmp(tokens[4], "group") ||
		strcmp(tokens[5], "member") ||
		strcmp(tokens[6], "add")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "group member add");
		return;
	}

	file_name = tokens[7];
	file = fopen(file_name, "r");
	if (!file) {
		snprintf(out, out_size, "Cannot open file %s.\n", file_name);
		return;
	}

	status = pipeline_selector_group_members_add(p->ctl,
		selector_name,
		file,
		&file_line_number);
	if (status)
		snprintf(out, out_size, "Invalid entry in file %s at line %u\n",
			 file_name,
			 file_line_number);

	fclose(file);
}

static int
pipeline_selector_group_members_delete(struct rte_swx_ctl_pipeline *p,
				       const char *selector_name,
				       FILE *file,
				       uint32_t *file_line_number)
{
	char *line = NULL;
	uint32_t line_id = 0;
	int status = 0;

	/* Buffer allocation. */
	line = malloc(MAX_LINE_SIZE);
	if (!line)
		return -ENOMEM;

	/* File read. */
	for (line_id = 1; ; line_id++) {
		uint32_t group_id, member_id, weight;
		int is_blank_or_comment;

		if (fgets(line, MAX_LINE_SIZE, file) == NULL)
			break;

		status = pipeline_selector_group_member_read(line,
							     &group_id,
							     &member_id,
							     &weight,
							     &is_blank_or_comment);
		if (status) {
			if (is_blank_or_comment)
				continue;

			goto error;
		}

		status = rte_swx_ctl_pipeline_selector_group_member_delete(p,
			selector_name,
			group_id,
			member_id);
		if (status)
			goto error;
	}

error:
	free(line);
	*file_line_number = line_id;
	return status;
}

/**
 * pipeline <pipeline_name> selector <selector_name> group member delete <file_name>
 */
static void
cmd_softnic_pipeline_selector_group_member_delete(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct pipeline *p;
	char *pipeline_name, *selector_name, *file_name;
	FILE *file = NULL;
	uint32_t file_line_number = 0;
	int status;

	if (n_tokens != 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = softnic_pipeline_find(softnic, pipeline_name);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "selector") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "selector");
		return;
	}

	selector_name = tokens[3];

	if (strcmp(tokens[4], "group") ||
		strcmp(tokens[5], "member") ||
		strcmp(tokens[6], "delete")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "group member delete");
		return;
	}

	file_name = tokens[7];
	file = fopen(file_name, "r");
	if (!file) {
		snprintf(out, out_size, "Cannot open file %s.\n", file_name);
		return;
	}

	status = pipeline_selector_group_members_delete(p->ctl,
							selector_name,
							file,
							&file_line_number);
	if (status)
		snprintf(out, out_size, "Invalid entry in file %s at line %u\n",
			 file_name,
			 file_line_number);

	fclose(file);
}

/**
 * pipeline <pipeline_name> selector <selector_name> show [filename]
 */
static void
cmd_softnic_pipeline_selector_show(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct pipeline *p;
	char *pipeline_name, *selector_name;
	FILE *file = NULL;
	int status;

	if (n_tokens != 5 && n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = softnic_pipeline_find(softnic, pipeline_name);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	selector_name = tokens[3];

	file = (n_tokens == 6) ? fopen(tokens[5], "w") : stdout;
	if (!file) {
		snprintf(out, out_size, "Cannot open file %s.\n", tokens[5]);
		return;
	}

	status = rte_swx_ctl_pipeline_selector_fprintf(file, p->ctl, selector_name);
	if (status)
		snprintf(out, out_size, MSG_ARG_INVALID, "selector_name");

	if (file)
		fclose(file);
}

static int
pipeline_learner_default_entry_add(struct rte_swx_ctl_pipeline *p,
				   const char *learner_name,
				   FILE *file,
				   uint32_t *file_line_number)
{
	char *line = NULL;
	uint32_t line_id = 0;
	int status = 0;

	/* Buffer allocation. */
	line = malloc(MAX_LINE_SIZE);
	if (!line)
		return -ENOMEM;

	/* File read. */
	for (line_id = 1; ; line_id++) {
		struct rte_swx_table_entry *entry;
		int is_blank_or_comment;

		if (fgets(line, MAX_LINE_SIZE, file) == NULL)
			break;

		entry = rte_swx_ctl_pipeline_learner_default_entry_read(p,
									learner_name,
									line,
									&is_blank_or_comment);
		if (!entry) {
			if (is_blank_or_comment)
				continue;

			status = -EINVAL;
			goto error;
		}

		status = rte_swx_ctl_pipeline_learner_default_entry_add(p,
									learner_name,
									entry);
		table_entry_free(entry);
		if (status)
			goto error;
	}

error:
	*file_line_number = line_id;
	free(line);
	return status;
}

/**
 * pipeline <pipeline_name> learner <learner_name> default <file_name>
 */
static void
cmd_softnic_pipeline_learner_default(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct pipeline *p;
	char *pipeline_name, *learner_name, *file_name;
	FILE *file = NULL;
	uint32_t file_line_number = 0;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = softnic_pipeline_find(softnic, pipeline_name);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	learner_name = tokens[3];

	file_name = tokens[5];
	file = fopen(file_name, "r");
	if (!file) {
		snprintf(out, out_size, "Cannot open file %s.\n", file_name);
		return;
	}

	status = pipeline_learner_default_entry_add(p->ctl,
						    learner_name,
						    file,
						    &file_line_number);
	if (status)
		snprintf(out, out_size, "Invalid entry in file %s at line %u\n",
			 file_name,
			 file_line_number);

	fclose(file);
}

/**
 * pipeline <pipeline_name> commit
 */
static void
cmd_softnic_pipeline_commit(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct pipeline *p;
	char *pipeline_name;
	int status;

	if (n_tokens != 3) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = softnic_pipeline_find(softnic, pipeline_name);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	status = rte_swx_ctl_pipeline_commit(p->ctl, 1);
	if (status)
		snprintf(out, out_size, "Commit failed. "
			"Use \"commit\" to retry or \"abort\" to discard the pending work.\n");
}

/**
 * pipeline <pipeline_name> abort
 */
static void
cmd_softnic_pipeline_abort(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct pipeline *p;
	char *pipeline_name;

	if (n_tokens != 3) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = softnic_pipeline_find(softnic, pipeline_name);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	rte_swx_ctl_pipeline_abort(p->ctl);
}

/**
 * pipeline <pipeline_name> regrd <register_array_name> <index>
 */
static void
cmd_softnic_pipeline_regrd(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct pipeline *p;
	const char *pipeline_name, *name;
	uint64_t value;
	uint32_t idx;
	int status;

	if (n_tokens != 5) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = softnic_pipeline_find(softnic, pipeline_name);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "regrd")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "regrd");
		return;
	}

	name = tokens[3];

	if (parser_read_uint32(&idx, tokens[4])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "index");
		return;
	}

	status = rte_swx_ctl_pipeline_regarray_read(p->p, name, idx, &value);
	if (status) {
		snprintf(out, out_size, "Command failed.\n");
		return;
	}

	snprintf(out, out_size, "0x%" PRIx64 "\n", value);
}

/**
 * pipeline <pipeline_name> regwr <register_array_name> <index> <value>
 */
static void
cmd_softnic_pipeline_regwr(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct pipeline *p;
	const char *pipeline_name, *name;
	uint64_t value;
	uint32_t idx;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = softnic_pipeline_find(softnic, pipeline_name);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "regwr")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "regwr");
		return;
	}

	name = tokens[3];

	if (parser_read_uint32(&idx, tokens[4])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "index");
		return;
	}

	if (parser_read_uint64(&value, tokens[5])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "value");
		return;
	}

	status = rte_swx_ctl_pipeline_regarray_write(p->p, name, idx, value);
	if (status) {
		snprintf(out, out_size, "Command failed.\n");
		return;
	}
}

/**
 * pipeline <pipeline_name> meter profile <profile_name> add cir <cir> pir <pir> cbs <cbs> pbs <pbs>
 */
static void
cmd_softnic_pipeline_meter_profile_add(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct rte_meter_trtcm_params params;
	struct pipeline *p;
	const char *profile_name;
	int status;

	if (n_tokens != 14) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	p = softnic_pipeline_find(softnic, tokens[1]);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "meter")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "meter");
		return;
	}

	if (strcmp(tokens[3], "profile")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "profile");
		return;
	}

	profile_name = tokens[4];

	if (strcmp(tokens[5], "add")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "add");
		return;
	}

	if (strcmp(tokens[6], "cir")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cir");
		return;
	}

	if (parser_read_uint64(&params.cir, tokens[7])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "cir");
		return;
	}

	if (strcmp(tokens[8], "pir")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pir");
		return;
	}

	if (parser_read_uint64(&params.pir, tokens[9])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pir");
		return;
	}

	if (strcmp(tokens[10], "cbs")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cbs");
		return;
	}

	if (parser_read_uint64(&params.cbs, tokens[11])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "cbs");
		return;
	}

	if (strcmp(tokens[12], "pbs")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pbs");
		return;
	}

	if (parser_read_uint64(&params.pbs, tokens[13])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pbs");
		return;
	}

	status = rte_swx_ctl_meter_profile_add(p->p, profile_name, &params);
	if (status) {
		snprintf(out, out_size, "Command failed.\n");
		return;
	}
}

/**
 * pipeline <pipeline_name> meter profile <profile_name> delete
 */
static void
cmd_softnic_pipeline_meter_profile_delete(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct pipeline *p;
	const char *profile_name;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	p = softnic_pipeline_find(softnic, tokens[1]);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "meter")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "meter");
		return;
	}

	if (strcmp(tokens[3], "profile")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "profile");
		return;
	}

	profile_name = tokens[4];

	if (strcmp(tokens[5], "delete")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "delete");
		return;
	}

	status = rte_swx_ctl_meter_profile_delete(p->p, profile_name);
	if (status) {
		snprintf(out, out_size, "Command failed.\n");
		return;
	}
}

/**
 * pipeline <pipeline_name> meter <meter_array_name> from <index0> to <index1> reset
 */
static void
cmd_softnic_pipeline_meter_reset(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct pipeline *p;
	const char *name;
	uint32_t idx0 = 0, idx1 = 0;

	if (n_tokens != 9) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	p = softnic_pipeline_find(softnic, tokens[1]);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "meter")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "meter");
		return;
	}

	name = tokens[3];

	if (strcmp(tokens[4], "from")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "from");
		return;
	}

	if (parser_read_uint32(&idx0, tokens[5])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "index0");
		return;
	}

	if (strcmp(tokens[6], "to")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "to");
		return;
	}

	if (parser_read_uint32(&idx1, tokens[7]) || idx1 < idx0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "index1");
		return;
	}

	if (strcmp(tokens[8], "reset")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "reset");
		return;
	}

	for ( ; idx0 <= idx1; idx0++) {
		int status;

		status = rte_swx_ctl_meter_reset(p->p, name, idx0);
		if (status) {
			snprintf(out, out_size, "Command failed for index %u.\n", idx0);
			return;
		}
	}
}

/**
 * pipeline <pipeline_name> meter <meter_array_name> from <index0> to <index1> set
 *	profile <profile_name>
 */
static void
cmd_softnic_pipeline_meter_set(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct pipeline *p;
	const char *name, *profile_name;
	uint32_t idx0 = 0, idx1 = 0;

	if (n_tokens != 11) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	p = softnic_pipeline_find(softnic, tokens[1]);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "meter")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "meter");
		return;
	}

	name = tokens[3];

	if (strcmp(tokens[4], "from")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "from");
		return;
	}

	if (parser_read_uint32(&idx0, tokens[5])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "index0");
		return;
	}

	if (strcmp(tokens[6], "to")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "to");
		return;
	}

	if (parser_read_uint32(&idx1, tokens[7]) || idx1 < idx0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "index1");
		return;
	}

	if (strcmp(tokens[8], "set")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "set");
		return;
	}

	if (strcmp(tokens[9], "profile")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "profile");
		return;
	}

	profile_name = tokens[10];

	for ( ; idx0 <= idx1; idx0++) {
		int status;

		status = rte_swx_ctl_meter_set(p->p, name, idx0, profile_name);
		if (status) {
			snprintf(out, out_size, "Command failed for index %u.\n", idx0);
			return;
		}
	}
}

/**
 * pipeline <pipeline_name> meter <meter_array_name> from <index0> to <index1> stats
 */
static void
cmd_softnic_pipeline_meter_stats(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct rte_swx_ctl_meter_stats stats;
	struct pipeline *p;
	const char *name;
	uint32_t idx0 = 0, idx1 = 0;

	if (n_tokens != 9) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	p = softnic_pipeline_find(softnic, tokens[1]);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "meter")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "meter");
		return;
	}

	name = tokens[3];

	if (strcmp(tokens[4], "from")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "from");
		return;
	}

	if (parser_read_uint32(&idx0, tokens[5])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "index0");
		return;
	}

	if (strcmp(tokens[6], "to")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "to");
		return;
	}

	if (parser_read_uint32(&idx1, tokens[7]) || idx1 < idx0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "index1");
		return;
	}

	if (strcmp(tokens[8], "stats")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "stats");
		return;
	}

	/* Table header. */
	snprintf(out, out_size, "+-%7s-+-%16s-+-%16s-+-%16s-+-%16s-+-%16s-+-%16s-+\n",
		 "-------",
		 "----------------", "----------------", "----------------",
		 "----------------", "----------------", "----------------");
	out_size -= strlen(out);
	out += strlen(out);

	snprintf(out, out_size, "| %4s | %16s | %16s | %16s | %16s | %16s | %16s |\n",
		 "METER #",
		 "GREEN (packets)", "YELLOW (packets)", "RED (packets)",
		 "GREEN (bytes)", "YELLOW (bytes)", "RED (bytes)");
	out_size -= strlen(out);
	out += strlen(out);

	snprintf(out, out_size, "+-%7s-+-%16s-+-%16s-+-%16s-+-%16s-+-%16s-+-%16s-+\n",
		 "-------",
		 "----------------", "----------------", "----------------",
		 "----------------", "----------------", "----------------");
	out_size -= strlen(out);
	out += strlen(out);

	/* Table rows. */
	for ( ; idx0 <= idx1; idx0++) {
		int status;

		status = rte_swx_ctl_meter_stats_read(p->p, name, idx0, &stats);
		if (status) {
			snprintf(out, out_size, "Pipeline meter stats error at index %u.\n", idx0);
			out_size -= strlen(out);
			out += strlen(out);
			return;
		}

		snprintf(out, out_size, "| %7d | %16" PRIx64 " | %16" PRIx64 " | %16" PRIx64
			 " | %16" PRIx64 " | %16" PRIx64 " | %16" PRIx64 " |\n",
			 idx0,
			 stats.n_pkts[RTE_COLOR_GREEN],
			 stats.n_pkts[RTE_COLOR_YELLOW],
			 stats.n_pkts[RTE_COLOR_RED],
			 stats.n_bytes[RTE_COLOR_GREEN],
			 stats.n_bytes[RTE_COLOR_YELLOW],
			 stats.n_bytes[RTE_COLOR_RED]);
		out_size -= strlen(out);
		out += strlen(out);
	}
}

/**
 * pipeline <pipeline_name> stats
 */
static void
cmd_softnic_pipeline_stats(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct rte_swx_ctl_pipeline_info info;
	struct pipeline *p;
	uint32_t i;
	int status;

	if (n_tokens != 3) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	p = softnic_pipeline_find(softnic, tokens[1]);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "stats")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "stats");
		return;
	}

	status = rte_swx_ctl_pipeline_info_get(p->p, &info);
	if (status) {
		snprintf(out, out_size, "Pipeline info get error.");
		return;
	}

	snprintf(out, out_size, "Input ports:\n");
	out_size -= strlen(out);
	out += strlen(out);

	for (i = 0; i < info.n_ports_in; i++) {
		struct rte_swx_port_in_stats stats;

		rte_swx_ctl_pipeline_port_in_stats_read(p->p, i, &stats);

		snprintf(out, out_size, "\tPort %u:"
			" packets %" PRIu64
			" bytes %" PRIu64
			" empty %" PRIu64 "\n",
			i, stats.n_pkts, stats.n_bytes, stats.n_empty);
		out_size -= strlen(out);
		out += strlen(out);
	}

	snprintf(out, out_size, "\nOutput ports:\n");
	out_size -= strlen(out);
	out += strlen(out);

	for (i = 0; i < info.n_ports_out; i++) {
		struct rte_swx_port_out_stats stats;

		rte_swx_ctl_pipeline_port_out_stats_read(p->p, i, &stats);

		if (i != info.n_ports_out - 1)
			snprintf(out, out_size, "\tPort %u:", i);
		else
			snprintf(out, out_size, "\tDROP:");

		out_size -= strlen(out);
		out += strlen(out);

		snprintf(out,
			out_size,
			" packets %" PRIu64
			" bytes %" PRIu64
			" packets dropped %" PRIu64
			" bytes dropped %" PRIu64
			" clone %" PRIu64
			" clonerr %" PRIu64 "\n",
			stats.n_pkts,
			stats.n_bytes,
			stats.n_pkts_drop,
			stats.n_bytes_drop,
			stats.n_pkts_clone,
			stats.n_pkts_clone_err);

		out_size -= strlen(out);
		out += strlen(out);
	}

	snprintf(out, out_size, "\nTables:\n");
	out_size -= strlen(out);
	out += strlen(out);

	for (i = 0; i < info.n_tables; i++) {
		struct rte_swx_ctl_table_info table_info;
		uint64_t n_pkts_action[info.n_actions];
		struct rte_swx_table_stats stats = {
			.n_pkts_hit = 0,
			.n_pkts_miss = 0,
			.n_pkts_action = n_pkts_action,
		};
		uint32_t j;

		status = rte_swx_ctl_table_info_get(p->p, i, &table_info);
		if (status) {
			snprintf(out, out_size, "Table info get error.");
			return;
		}

		status = rte_swx_ctl_pipeline_table_stats_read(p->p, table_info.name, &stats);
		if (status) {
			snprintf(out, out_size, "Table stats read error.");
			return;
		}

		snprintf(out, out_size, "\tTable %s:\n"
			"\t\tHit (packets): %" PRIu64 "\n"
			"\t\tMiss (packets): %" PRIu64 "\n",
			table_info.name,
			stats.n_pkts_hit,
			stats.n_pkts_miss);
		out_size -= strlen(out);
		out += strlen(out);

		for (j = 0; j < info.n_actions; j++) {
			struct rte_swx_ctl_action_info action_info;

			status = rte_swx_ctl_action_info_get(p->p, j, &action_info);
			if (status) {
				snprintf(out, out_size, "Action info get error.");
				return;
			}

			snprintf(out, out_size, "\t\tAction %s (packets): %" PRIu64 "\n",
				action_info.name,
				stats.n_pkts_action[j]);
			out_size -= strlen(out);
			out += strlen(out);
		}
	}

	snprintf(out, out_size, "\nLearner tables:\n");
	out_size -= strlen(out);
	out += strlen(out);

	for (i = 0; i < info.n_learners; i++) {
		struct rte_swx_ctl_learner_info learner_info;
		uint64_t n_pkts_action[info.n_actions];
		struct rte_swx_learner_stats stats = {
			.n_pkts_hit = 0,
			.n_pkts_miss = 0,
			.n_pkts_action = n_pkts_action,
		};
		uint32_t j;

		status = rte_swx_ctl_learner_info_get(p->p, i, &learner_info);
		if (status) {
			snprintf(out, out_size, "Learner table info get error.");
			return;
		}

		status = rte_swx_ctl_pipeline_learner_stats_read(p->p, learner_info.name, &stats);
		if (status) {
			snprintf(out, out_size, "Learner table stats read error.");
			return;
		}

		snprintf(out, out_size, "\tLearner table %s:\n"
			"\t\tHit (packets): %" PRIu64 "\n"
			"\t\tMiss (packets): %" PRIu64 "\n"
			"\t\tLearn OK (packets): %" PRIu64 "\n"
			"\t\tLearn error (packets): %" PRIu64 "\n"
			"\t\tRearm (packets): %" PRIu64 "\n"
			"\t\tForget (packets): %" PRIu64 "\n",
			learner_info.name,
			stats.n_pkts_hit,
			stats.n_pkts_miss,
			stats.n_pkts_learn_ok,
			stats.n_pkts_learn_err,
			stats.n_pkts_rearm,
			stats.n_pkts_forget);
		out_size -= strlen(out);
		out += strlen(out);

		for (j = 0; j < info.n_actions; j++) {
			struct rte_swx_ctl_action_info action_info;

			status = rte_swx_ctl_action_info_get(p->p, j, &action_info);
			if (status) {
				snprintf(out, out_size, "Action info get error.");
				return;
			}

			snprintf(out, out_size, "\t\tAction %s (packets): %" PRIu64 "\n",
				action_info.name,
				stats.n_pkts_action[j]);
			out_size -= strlen(out);
			out += strlen(out);
		}
	}
}

/**
 * pipeline <pipeline_name> mirror session <session_id> port <port_id> clone fast | slow
 *	truncate <truncation_length>
 */
static void
cmd_softnic_pipeline_mirror_session(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct rte_swx_pipeline_mirroring_session_params params;
	struct pipeline *p;
	uint32_t session_id = 0;
	int status;

	if (n_tokens != 11) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (strcmp(tokens[0], "pipeline")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pipeline");
		return;
	}

	p = softnic_pipeline_find(softnic, tokens[1]);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "mirror")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "mirror");
		return;
	}

	if (strcmp(tokens[3], "session")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "session");
		return;
	}

	if (parser_read_uint32(&session_id, tokens[4])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "session_id");
		return;
	}

	if (strcmp(tokens[5], "port")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
		return;
	}

	if (parser_read_uint32(&params.port_id, tokens[6])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
		return;
	}

	if (strcmp(tokens[7], "clone")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "clone");
		return;
	}

	if (!strcmp(tokens[8], "fast")) {
		params.fast_clone = 1;
	} else if (!strcmp(tokens[8], "slow")) {
		params.fast_clone = 0;
	} else {
		snprintf(out, out_size, MSG_ARG_INVALID, "clone");
		return;
	}

	if (strcmp(tokens[9], "truncate")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "truncate");
		return;
	}

	if (parser_read_uint32(&params.truncation_length, tokens[10])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "truncation_length");
		return;
	}

	status = rte_swx_ctl_pipeline_mirroring_session_set(p->p, session_id, &params);
	if (status) {
		snprintf(out, out_size, "Command failed!\n");
		return;
	}
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

		if (n_tokens >= 5 && !strcmp(tokens[2], "table") && !strcmp(tokens[4], "add")) {
			cmd_softnic_pipeline_table_add(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 5 && !strcmp(tokens[2], "table") && !strcmp(tokens[4], "delete")) {
			cmd_softnic_pipeline_table_delete(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 5 && !strcmp(tokens[2], "table") && !strcmp(tokens[4], "default")) {
			cmd_softnic_pipeline_table_default(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 5 && !strcmp(tokens[2], "table") && !strcmp(tokens[4], "show")) {
			cmd_softnic_pipeline_table_show(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 6 &&
			!strcmp(tokens[2], "selector") &&
			!strcmp(tokens[4], "group") &&
			!strcmp(tokens[5], "add")) {
			cmd_softnic_pipeline_selector_group_add(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 6 &&
			!strcmp(tokens[2], "selector") &&
			!strcmp(tokens[4], "group") &&
			!strcmp(tokens[5], "delete")) {
			cmd_softnic_pipeline_selector_group_delete(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 7 &&
			!strcmp(tokens[2], "selector") &&
			!strcmp(tokens[4], "group") &&
			!strcmp(tokens[5], "member") &&
			!strcmp(tokens[6], "add")) {
			cmd_softnic_pipeline_selector_group_member_add(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 7 &&
			!strcmp(tokens[2], "selector") &&
			!strcmp(tokens[4], "group") &&
			!strcmp(tokens[5], "member") &&
			!strcmp(tokens[6], "delete")) {
			cmd_softnic_pipeline_selector_group_member_delete(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 5 &&
			!strcmp(tokens[2], "selector") &&
			!strcmp(tokens[4], "show")) {
			cmd_softnic_pipeline_selector_show(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 5 &&
			!strcmp(tokens[2], "learner") &&
			!strcmp(tokens[4], "default")) {
			cmd_softnic_pipeline_learner_default(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 3 && !strcmp(tokens[2], "commit")) {
			cmd_softnic_pipeline_commit(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 3 && !strcmp(tokens[2], "abort")) {
			cmd_softnic_pipeline_abort(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 3 && !strcmp(tokens[2], "regrd")) {
			cmd_softnic_pipeline_regrd(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 3 && !strcmp(tokens[2], "regwr")) {
			cmd_softnic_pipeline_regwr(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 6 &&
			!strcmp(tokens[2], "meter") &&
			!strcmp(tokens[3], "profile") &&
			!strcmp(tokens[5], "add")) {
			cmd_softnic_pipeline_meter_profile_add(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 6 &&
			!strcmp(tokens[2], "meter") &&
			!strcmp(tokens[3], "profile") &&
			!strcmp(tokens[5], "delete")) {
			cmd_softnic_pipeline_meter_profile_delete(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 9 && !strcmp(tokens[2], "meter") && !strcmp(tokens[8], "reset")) {
			cmd_softnic_pipeline_meter_reset(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 9 && !strcmp(tokens[2], "meter") && !strcmp(tokens[8], "set")) {
			cmd_softnic_pipeline_meter_set(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 9 && !strcmp(tokens[2], "meter") && !strcmp(tokens[8], "stats")) {
			cmd_softnic_pipeline_meter_stats(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 3 && !strcmp(tokens[2], "stats")) {
			cmd_softnic_pipeline_stats(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 4 &&
			!strcmp(tokens[2], "mirror") &&
			!strcmp(tokens[3], "session")) {
			cmd_softnic_pipeline_mirror_session(softnic, tokens, n_tokens,
				out, out_size);
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
