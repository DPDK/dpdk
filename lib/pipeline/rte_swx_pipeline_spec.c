/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <rte_common.h>
#include <rte_mempool.h>

#include <rte_swx_port_ethdev.h>
#include <rte_swx_port_ring.h>
#include <rte_swx_port_source_sink.h>
#include <rte_swx_port_fd.h>

#include "rte_swx_pipeline_spec.h"

#ifndef MAX_LINE_LENGTH
#define MAX_LINE_LENGTH 2048
#endif

#ifndef MAX_TOKENS
#define MAX_TOKENS 256
#endif

#define STRUCT_BLOCK 0
#define ACTION_BLOCK 1
#define TABLE_BLOCK 2
#define TABLE_KEY_BLOCK 3
#define TABLE_ACTIONS_BLOCK 4
#define SELECTOR_BLOCK 5
#define SELECTOR_SELECTOR_BLOCK 6
#define LEARNER_BLOCK 7
#define LEARNER_KEY_BLOCK 8
#define LEARNER_ACTIONS_BLOCK 9
#define LEARNER_TIMEOUT_BLOCK 10
#define APPLY_BLOCK 11

/*
 * extobj.
 */
static void
extobj_spec_free(struct extobj_spec *s)
{
	if (!s)
		return;

	free(s->name);
	s->name = NULL;

	free(s->extern_type_name);
	s->extern_type_name = NULL;

	free(s->pragma);
	s->pragma = NULL;
}

static int
extobj_statement_parse(struct extobj_spec *s,
		       char **tokens,
		       uint32_t n_tokens,
		       uint32_t n_lines,
		       uint32_t *err_line,
		       const char **err_msg)
{
	/* Check format. */
	if (((n_tokens != 4) && (n_tokens != 6)) ||
	    ((n_tokens == 4) && strcmp(tokens[2], "instanceof")) ||
	    ((n_tokens == 6) && (strcmp(tokens[2], "instanceof") ||
				 strcmp(tokens[4], "pragma")))) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid extobj statement.";
		return -EINVAL;
	}

	/* spec. */
	s->name = strdup(tokens[1]);
	s->extern_type_name = strdup(tokens[3]);
	s->pragma = (n_tokens == 6) ? strdup(tokens[5]) : NULL;

	if (!s->name ||
	    !s->extern_type_name ||
	    ((n_tokens == 6) && !s->pragma)) {
		free(s->name);
		free(s->extern_type_name);
		free(s->pragma);

		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return -ENOMEM;
	}

	return 0;
}

/*
 * struct.
 */
static void
struct_spec_free(struct struct_spec *s)
{
	uint32_t i;

	if (!s)
		return;

	free(s->name);
	s->name = NULL;

	for (i = 0; i < s->n_fields; i++) {
		uintptr_t name = (uintptr_t)s->fields[i].name;

		free((void *)name);
	}

	free(s->fields);
	s->fields = NULL;

	s->n_fields = 0;

	s->varbit = 0;
}

static int
struct_statement_parse(struct struct_spec *s,
		       uint32_t *block_mask,
		       char **tokens,
		       uint32_t n_tokens,
		       uint32_t n_lines,
		       uint32_t *err_line,
		       const char **err_msg)
{
	/* Check format. */
	if ((n_tokens != 3) || strcmp(tokens[2], "{")) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid struct statement.";
		return -EINVAL;
	}

	/* spec. */
	s->name = strdup(tokens[1]);
	if (!s->name) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return -ENOMEM;
	}

	/* block_mask. */
	*block_mask |= 1 << STRUCT_BLOCK;

	return 0;
}

static int
struct_block_parse(struct struct_spec *s,
		   uint32_t *block_mask,
		   char **tokens,
		   uint32_t n_tokens,
		   uint32_t n_lines,
		   uint32_t *err_line,
		   const char **err_msg)
{
	struct rte_swx_field_params *new_fields;
	char *p = tokens[0], *name = NULL;
	uint32_t n_bits;
	int varbit = 0, error = 0, error_size_invalid = 0, error_varbit_not_last = 0;

	/* Handle end of block. */
	if ((n_tokens == 1) && !strcmp(tokens[0], "}")) {
		*block_mask &= ~(1 << STRUCT_BLOCK);
		return 0;
	}

	/* Check format. */
	if (n_tokens != 2) {
		error = -EINVAL;
		goto error;
	}

	if (s->varbit) {
		error = -EINVAL;
		error_varbit_not_last = 1;
		goto error;
	}

	if (!strncmp(p, "bit<", strlen("bit<"))) {
		size_t len = strlen(p);

		if ((len < strlen("bit< >")) || (p[len - 1] != '>')) {
			error = -EINVAL;
			goto error;
		}

		/* Remove the "bit<" and ">". */
		p[strlen(p) - 1] = 0;
		p += strlen("bit<");
	} else if (!strncmp(p, "varbit<", strlen("varbit<"))) {
		size_t len = strlen(p);

		if ((len < strlen("varbit< >")) || (p[len - 1] != '>')) {
			error = -EINVAL;
			goto error;
		}

		/* Remove the "varbit<" and ">". */
		p[strlen(p) - 1] = 0;
		p += strlen("varbit<");

		/* Set the varbit flag. */
		varbit = 1;
	} else {
		error = -EINVAL;
		goto error;
	}

	n_bits = strtoul(p, &p, 0);
	if ((p[0]) ||
	    !n_bits ||
	    (n_bits % 8)) {
		error = -EINVAL;
		error_size_invalid = 1;
		goto error;
	}

	/* spec. */
	name = strdup(tokens[1]);
	if (!name) {
		error = -ENOMEM;
		goto error;
	}

	new_fields = realloc(s->fields, (s->n_fields + 1) * sizeof(struct rte_swx_field_params));
	if (!new_fields) {
		error = -ENOMEM;
		goto error;
	}

	s->fields = new_fields;
	s->fields[s->n_fields].name = name;
	s->fields[s->n_fields].n_bits = n_bits;
	s->n_fields++;
	s->varbit = varbit;

	return 0;

error:
	free(name);

	if (err_line)
		*err_line = n_lines;

	if (err_msg) {
		*err_msg = "Invalid struct field statement.";

		if ((error == -EINVAL) && error_varbit_not_last)
			*err_msg = "Varbit field is not the last struct field.";

		if ((error == -EINVAL) && error_size_invalid)
			*err_msg = "Invalid struct field size.";

		if (error == -ENOMEM)
			*err_msg = "Memory allocation failed.";
	}

	return error;
}

/*
 * header.
 */
static void
header_spec_free(struct header_spec *s)
{
	if (!s)
		return;

	free(s->name);
	s->name = NULL;

	free(s->struct_type_name);
	s->struct_type_name = NULL;
}

static int
header_statement_parse(struct header_spec *s,
		       char **tokens,
		       uint32_t n_tokens,
		       uint32_t n_lines,
		       uint32_t *err_line,
		       const char **err_msg)
{
	/* Check format. */
	if ((n_tokens != 4) || strcmp(tokens[2], "instanceof")) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid header statement.";
		return -EINVAL;
	}

	/* spec. */
	s->name = strdup(tokens[1]);
	s->struct_type_name = strdup(tokens[3]);

	if (!s->name || !s->struct_type_name) {
		free(s->name);
		free(s->struct_type_name);

		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return -ENOMEM;
	}

	return 0;
}

/*
 * metadata.
 */
static void
metadata_spec_free(struct metadata_spec *s)
{
	if (!s)
		return;

	free(s->struct_type_name);
	s->struct_type_name = NULL;
}

static int
metadata_statement_parse(struct metadata_spec *s,
			 char **tokens,
			 uint32_t n_tokens,
			 uint32_t n_lines,
			 uint32_t *err_line,
			 const char **err_msg)
{
	/* Check format. */
	if ((n_tokens != 3) || strcmp(tokens[1], "instanceof")) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid metadata statement.";
		return -EINVAL;
	}

	/* spec. */
	s->struct_type_name = strdup(tokens[2]);
	if (!s->struct_type_name) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return -ENOMEM;
	}

	return 0;
}

/*
 * action.
 */
static void
action_spec_free(struct action_spec *s)
{
	uint32_t i;

	if (!s)
		return;

	free(s->name);
	s->name = NULL;

	free(s->args_struct_type_name);
	s->args_struct_type_name = NULL;

	for (i = 0; i < s->n_instructions; i++) {
		uintptr_t instr = (uintptr_t)s->instructions[i];

		free((void *)instr);
	}

	free(s->instructions);
	s->instructions = NULL;

	s->n_instructions = 0;
}

static int
action_statement_parse(struct action_spec *s,
		       uint32_t *block_mask,
		       char **tokens,
		       uint32_t n_tokens,
		       uint32_t n_lines,
		       uint32_t *err_line,
		       const char **err_msg)
{
	/* Check format. */
	if (((n_tokens != 5) && (n_tokens != 6)) ||
	    ((n_tokens == 5) &&
	     (strcmp(tokens[2], "args") ||
	      strcmp(tokens[3], "none") ||
	      strcmp(tokens[4], "{"))) ||
	    ((n_tokens == 6) &&
	     (strcmp(tokens[2], "args") ||
	      strcmp(tokens[3], "instanceof") ||
	      strcmp(tokens[5], "{")))) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid action statement.";
		return -EINVAL;
	}

	/* spec. */
	s->name = strdup(tokens[1]);
	s->args_struct_type_name = (n_tokens == 6) ? strdup(tokens[4]) : NULL;

	if ((!s->name) || ((n_tokens == 6) && !s->args_struct_type_name)) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return -ENOMEM;
	}

	/* block_mask. */
	*block_mask |= 1 << ACTION_BLOCK;

	return 0;
}

static int
action_block_parse(struct action_spec *s,
		   uint32_t *block_mask,
		   char **tokens,
		   uint32_t n_tokens,
		   uint32_t n_lines,
		   uint32_t *err_line,
		   const char **err_msg)
{
	char buffer[RTE_SWX_INSTRUCTION_SIZE], *instr;
	const char **new_instructions;
	uint32_t i;

	/* Handle end of block. */
	if ((n_tokens == 1) && !strcmp(tokens[0], "}")) {
		*block_mask &= ~(1 << ACTION_BLOCK);
		return 0;
	}

	/* spec. */
	buffer[0] = 0;
	for (i = 0; i < n_tokens; i++) {
		if (i)
			strcat(buffer, " ");
		strcat(buffer, tokens[i]);
	}

	instr = strdup(buffer);
	if (!instr) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return -ENOMEM;
	}

	new_instructions = realloc(s->instructions,
				   (s->n_instructions + 1) * sizeof(char *));
	if (!new_instructions) {
		free(instr);

		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return -ENOMEM;
	}

	s->instructions = new_instructions;
	s->instructions[s->n_instructions] = instr;
	s->n_instructions++;

	return 0;
}

/*
 * table.
 */
static void
table_spec_free(struct table_spec *s)
{
	uintptr_t default_action_name, default_action_args, hash_func_name;
	uint32_t i;

	if (!s)
		return;

	free(s->name);
	s->name = NULL;

	for (i = 0; i < s->params.n_fields; i++) {
		uintptr_t name = (uintptr_t)s->params.fields[i].name;

		free((void *)name);
	}

	free(s->params.fields);
	s->params.fields = NULL;

	s->params.n_fields = 0;

	for (i = 0; i < s->params.n_actions; i++) {
		uintptr_t name = (uintptr_t)s->params.action_names[i];

		free((void *)name);
	}

	free(s->params.action_names);
	s->params.action_names = NULL;

	s->params.n_actions = 0;

	default_action_name = (uintptr_t)s->params.default_action_name;
	free((void *)default_action_name);
	s->params.default_action_name = NULL;

	default_action_args = (uintptr_t)s->params.default_action_args;
	free((void *)default_action_args);
	s->params.default_action_args = NULL;

	free(s->params.action_is_for_table_entries);
	s->params.action_is_for_table_entries = NULL;

	free(s->params.action_is_for_default_entry);
	s->params.action_is_for_default_entry = NULL;

	s->params.default_action_is_const = 0;

	hash_func_name = (uintptr_t)s->params.hash_func_name;
	free((void *)hash_func_name);
	s->params.hash_func_name = NULL;

	free(s->recommended_table_type_name);
	s->recommended_table_type_name = NULL;

	free(s->args);
	s->args = NULL;

	s->size = 0;
}

static int
table_key_statement_parse(uint32_t *block_mask,
			  char **tokens,
			  uint32_t n_tokens,
			  uint32_t n_lines,
			  uint32_t *err_line,
			  const char **err_msg)
{
	/* Check format. */
	if ((n_tokens != 2) || strcmp(tokens[1], "{")) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid key statement.";
		return -EINVAL;
	}

	/* block_mask. */
	*block_mask |= 1 << TABLE_KEY_BLOCK;

	return 0;
}

static int
table_key_block_parse(struct table_spec *s,
		      uint32_t *block_mask,
		      char **tokens,
		      uint32_t n_tokens,
		      uint32_t n_lines,
		      uint32_t *err_line,
		      const char **err_msg)
{
	struct rte_swx_match_field_params *new_fields;
	enum rte_swx_table_match_type match_type = RTE_SWX_TABLE_MATCH_WILDCARD;
	char *name;

	/* Handle end of block. */
	if ((n_tokens == 1) && !strcmp(tokens[0], "}")) {
		*block_mask &= ~(1 << TABLE_KEY_BLOCK);
		return 0;
	}

	/* Check input arguments. */
	if ((n_tokens != 2) ||
	    (strcmp(tokens[1], "exact") &&
	     strcmp(tokens[1], "wildcard") &&
	     strcmp(tokens[1], "lpm"))) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid match field statement.";
		return -EINVAL;
	}

	if (!strcmp(tokens[1], "wildcard"))
		match_type = RTE_SWX_TABLE_MATCH_WILDCARD;
	if (!strcmp(tokens[1], "lpm"))
		match_type = RTE_SWX_TABLE_MATCH_LPM;
	if (!strcmp(tokens[1], "exact"))
		match_type = RTE_SWX_TABLE_MATCH_EXACT;

	name = strdup(tokens[0]);
	if (!name) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return -ENOMEM;
	}

	new_fields = realloc(s->params.fields,
			     (s->params.n_fields + 1) * sizeof(struct rte_swx_match_field_params));
	if (!new_fields) {
		free(name);

		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return -ENOMEM;
	}

	s->params.fields = new_fields;
	s->params.fields[s->params.n_fields].name = name;
	s->params.fields[s->params.n_fields].match_type = match_type;
	s->params.n_fields++;

	return 0;
}

static int
table_actions_statement_parse(uint32_t *block_mask,
			      char **tokens,
			      uint32_t n_tokens,
			      uint32_t n_lines,
			      uint32_t *err_line,
			      const char **err_msg)
{
	/* Check format. */
	if ((n_tokens != 2) || strcmp(tokens[1], "{")) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid actions statement.";
		return -EINVAL;
	}

	/* block_mask. */
	*block_mask |= 1 << TABLE_ACTIONS_BLOCK;

	return 0;
}

static int
table_actions_block_parse(struct table_spec *s,
			  uint32_t *block_mask,
			  char **tokens,
			  uint32_t n_tokens,
			  uint32_t n_lines,
			  uint32_t *err_line,
			  const char **err_msg)
{
	const char **new_action_names = NULL;
	int *new_action_is_for_table_entries = NULL, *new_action_is_for_default_entry = NULL;
	char *name = NULL;
	int action_is_for_table_entries = 1, action_is_for_default_entry = 1;

	/* Handle end of block. */
	if ((n_tokens == 1) && !strcmp(tokens[0], "}")) {
		*block_mask &= ~(1 << TABLE_ACTIONS_BLOCK);
		return 0;
	}

	/* Check input arguments. */
	if ((n_tokens > 2) ||
	    ((n_tokens == 2) && strcmp(tokens[1], "@tableonly") &&
	      strcmp(tokens[1], "@defaultonly"))) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid action name statement.";
		return -EINVAL;
	}

	name = strdup(tokens[0]);

	if (n_tokens == 2) {
		if (!strcmp(tokens[1], "@tableonly"))
			action_is_for_default_entry = 0;

		if (!strcmp(tokens[1], "@defaultonly"))
			action_is_for_table_entries = 0;
	}

	new_action_names = realloc(s->params.action_names,
				   (s->params.n_actions + 1) * sizeof(char *));
	new_action_is_for_table_entries = realloc(s->params.action_is_for_table_entries,
						  (s->params.n_actions + 1) * sizeof(int));
	new_action_is_for_default_entry = realloc(s->params.action_is_for_default_entry,
						  (s->params.n_actions + 1) * sizeof(int));

	if (!name ||
	    !new_action_names ||
	    !new_action_is_for_table_entries ||
	    !new_action_is_for_default_entry) {
		free(name);
		free(new_action_names);
		free(new_action_is_for_table_entries);
		free(new_action_is_for_default_entry);

		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return -ENOMEM;
	}

	s->params.action_names = new_action_names;
	s->params.action_names[s->params.n_actions] = name;

	s->params.action_is_for_table_entries = new_action_is_for_table_entries;
	s->params.action_is_for_table_entries[s->params.n_actions] = action_is_for_table_entries;

	s->params.action_is_for_default_entry = new_action_is_for_default_entry;
	s->params.action_is_for_default_entry[s->params.n_actions] = action_is_for_default_entry;

	s->params.n_actions++;

	return 0;
}

static int
table_default_action_statement_parse(struct table_spec *s,
				     char **tokens,
				     uint32_t n_tokens,
				     uint32_t n_lines,
				     uint32_t *err_line,
				     const char **err_msg)
{
	uint32_t i;
	int status = 0, duplicate = 0;

	/* Check format. */
	if ((n_tokens < 4) ||
	    strcmp(tokens[2], "args")) {
		status = -EINVAL;
		goto error;
	}

	if (s->params.default_action_name) {
		duplicate = 1;
		status = -EINVAL;
		goto error;
	}

	s->params.default_action_name = strdup(tokens[1]);
	if (!s->params.default_action_name) {
		status = -ENOMEM;
		goto error;
	}

	if (strcmp(tokens[3], "none")) {
		char buffer[MAX_LINE_LENGTH];
		uint32_t n_tokens_args = n_tokens - 3;

		if (!strcmp(tokens[n_tokens - 1], "const"))
			n_tokens_args--;

		if (!n_tokens_args) {
			status = -EINVAL;
			goto error;
		}

		buffer[0] = 0;
		for (i = 0; i < n_tokens_args; i++) {
			if (i)
				strcat(buffer, " ");

			strcat(buffer, tokens[3 + i]);
		}

		s->params.default_action_args = strdup(buffer);
		if (!s->params.default_action_args) {
			status = -ENOMEM;
			goto error;
		}
	} else {
		if (((n_tokens != 4) && (n_tokens != 5)) ||
		    ((n_tokens == 5) && (strcmp(tokens[4], "const")))) {
			status = -EINVAL;
			goto error;
		}
	}

	if (!strcmp(tokens[n_tokens - 1], "const"))
		s->params.default_action_is_const = 1;

	return 0;

error:
	if (err_line)
		*err_line = n_lines;

	if (err_msg)
		switch (status) {
		case -ENOMEM:
			*err_msg = "Memory allocation failed.";
			break;

		default:
			if (duplicate)
				*err_msg = "Duplicate default_action statement.";

			*err_msg = "Invalid default_action statement.";
		}

	return status;
}

static int
table_statement_parse(struct table_spec *s,
		      uint32_t *block_mask,
		      char **tokens,
		      uint32_t n_tokens,
		      uint32_t n_lines,
		      uint32_t *err_line,
		      const char **err_msg)
{
	/* Check format. */
	if ((n_tokens != 3) || strcmp(tokens[2], "{")) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid table statement.";
		return -EINVAL;
	}

	/* spec. */
	s->name = strdup(tokens[1]);
	if (!s->name) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return -ENOMEM;
	}

	/* block_mask. */
	*block_mask |= 1 << TABLE_BLOCK;

	return 0;
}

static int
table_block_parse(struct table_spec *s,
		  uint32_t *block_mask,
		  char **tokens,
		  uint32_t n_tokens,
		  uint32_t n_lines,
		  uint32_t *err_line,
		  const char **err_msg)
{
	if (*block_mask & (1 << TABLE_KEY_BLOCK))
		return table_key_block_parse(s,
					     block_mask,
					     tokens,
					     n_tokens,
					     n_lines,
					     err_line,
					     err_msg);

	if (*block_mask & (1 << TABLE_ACTIONS_BLOCK))
		return table_actions_block_parse(s,
						 block_mask,
						 tokens,
						 n_tokens,
						 n_lines,
						 err_line,
						 err_msg);

	/* Handle end of block. */
	if ((n_tokens == 1) && !strcmp(tokens[0], "}")) {
		*block_mask &= ~(1 << TABLE_BLOCK);
		return 0;
	}

	if (!strcmp(tokens[0], "key"))
		return table_key_statement_parse(block_mask,
						 tokens,
						 n_tokens,
						 n_lines,
						 err_line,
						 err_msg);

	if (!strcmp(tokens[0], "actions"))
		return table_actions_statement_parse(block_mask,
						     tokens,
						     n_tokens,
						     n_lines,
						     err_line,
						     err_msg);

	if (!strcmp(tokens[0], "default_action"))
		return table_default_action_statement_parse(s,
							    tokens,
							    n_tokens,
							    n_lines,
							    err_line,
							    err_msg);

	if (!strcmp(tokens[0], "hash")) {
		if (n_tokens != 2) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Invalid hash statement.";
			return -EINVAL;
		}

		if (s->params.hash_func_name) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Duplicate hash statement.";
			return -EINVAL;
		}

		s->params.hash_func_name = strdup(tokens[1]);
		if (!s->params.hash_func_name) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Memory allocation failed.";
			return -ENOMEM;
		}

		return 0;
	}

	if (!strcmp(tokens[0], "instanceof")) {
		if (n_tokens != 2) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Invalid instanceof statement.";
			return -EINVAL;
		}

		if (s->recommended_table_type_name) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Duplicate instanceof statement.";
			return -EINVAL;
		}

		s->recommended_table_type_name = strdup(tokens[1]);
		if (!s->recommended_table_type_name) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Memory allocation failed.";
			return -ENOMEM;
		}

		return 0;
	}

	if (!strcmp(tokens[0], "pragma")) {
		if (n_tokens != 2) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Invalid pragma statement.";
			return -EINVAL;
		}

		if (s->args) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Duplicate pragma statement.";
			return -EINVAL;
		}

		s->args = strdup(tokens[1]);
		if (!s->args) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Memory allocation failed.";
			return -ENOMEM;
		}

		return 0;
	}

	if (!strcmp(tokens[0], "size")) {
		char *p = tokens[1];

		if (n_tokens != 2) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Invalid pragma statement.";
			return -EINVAL;
		}

		s->size = strtoul(p, &p, 0);
		if (p[0]) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Invalid size argument.";
			return -EINVAL;
		}

		return 0;
	}

	/* Anything else. */
	if (err_line)
		*err_line = n_lines;
	if (err_msg)
		*err_msg = "Invalid statement.";
	return -EINVAL;
}

/*
 * selector.
 */
static void
selector_spec_free(struct selector_spec *s)
{
	uintptr_t field_name;
	uint32_t i;

	if (!s)
		return;

	/* name. */
	free(s->name);
	s->name = NULL;

	/* params->group_id_field_name. */
	field_name = (uintptr_t)s->params.group_id_field_name;
	free((void *)field_name);
	s->params.group_id_field_name = NULL;

	/* params->selector_field_names. */
	for (i = 0; i < s->params.n_selector_fields; i++) {
		field_name = (uintptr_t)s->params.selector_field_names[i];

		free((void *)field_name);
	}

	free(s->params.selector_field_names);
	s->params.selector_field_names = NULL;

	s->params.n_selector_fields = 0;

	/* params->member_id_field_name. */
	field_name = (uintptr_t)s->params.member_id_field_name;
	free((void *)field_name);
	s->params.member_id_field_name = NULL;

	/* params->n_groups_max. */
	s->params.n_groups_max = 0;

	/* params->n_members_per_group_max. */
	s->params.n_members_per_group_max = 0;
}

static int
selector_statement_parse(struct selector_spec *s,
			 uint32_t *block_mask,
			 char **tokens,
			 uint32_t n_tokens,
			 uint32_t n_lines,
			 uint32_t *err_line,
			 const char **err_msg)
{
	/* Check format. */
	if ((n_tokens != 3) || strcmp(tokens[2], "{")) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid selector statement.";
		return -EINVAL;
	}

	/* spec. */
	s->name = strdup(tokens[1]);
	if (!s->name) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return -ENOMEM;
	}

	/* block_mask. */
	*block_mask |= 1 << SELECTOR_BLOCK;

	return 0;
}

static int
selector_selector_statement_parse(uint32_t *block_mask,
				  char **tokens,
				  uint32_t n_tokens,
				  uint32_t n_lines,
				  uint32_t *err_line,
				  const char **err_msg)
{
	/* Check format. */
	if ((n_tokens != 2) || strcmp(tokens[1], "{")) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid selector statement.";
		return -EINVAL;
	}

	/* block_mask. */
	*block_mask |= 1 << SELECTOR_SELECTOR_BLOCK;

	return 0;
}

static int
selector_selector_block_parse(struct selector_spec *s,
			      uint32_t *block_mask,
			      char **tokens,
			      uint32_t n_tokens,
			      uint32_t n_lines,
			      uint32_t *err_line,
			      const char **err_msg)
{
	const char **new_fields;
	char *name;

	/* Handle end of block. */
	if ((n_tokens == 1) && !strcmp(tokens[0], "}")) {
		*block_mask &= ~(1 << SELECTOR_SELECTOR_BLOCK);
		return 0;
	}

	/* Check input arguments. */
	if (n_tokens != 1) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid selector field statement.";
		return -EINVAL;
	}

	name = strdup(tokens[0]);
	if (!name) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return -ENOMEM;
	}

	new_fields = realloc(s->params.selector_field_names,
			     (s->params.n_selector_fields + 1) * sizeof(char *));
	if (!new_fields) {
		free(name);

		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return -ENOMEM;
	}

	s->params.selector_field_names = new_fields;
	s->params.selector_field_names[s->params.n_selector_fields] = name;
	s->params.n_selector_fields++;

	return 0;
}

static int
selector_block_parse(struct selector_spec *s,
		     uint32_t *block_mask,
		     char **tokens,
		     uint32_t n_tokens,
		     uint32_t n_lines,
		     uint32_t *err_line,
		     const char **err_msg)
{
	if (*block_mask & (1 << SELECTOR_SELECTOR_BLOCK))
		return selector_selector_block_parse(s,
						     block_mask,
						     tokens,
						     n_tokens,
						     n_lines,
						     err_line,
						     err_msg);

	/* Handle end of block. */
	if ((n_tokens == 1) && !strcmp(tokens[0], "}")) {
		*block_mask &= ~(1 << SELECTOR_BLOCK);
		return 0;
	}

	if (!strcmp(tokens[0], "group_id")) {
		if (n_tokens != 2) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Invalid group_id statement.";
			return -EINVAL;
		}

		s->params.group_id_field_name = strdup(tokens[1]);
		if (!s->params.group_id_field_name) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Memory allocation failed.";
			return -ENOMEM;
		}

		return 0;
	}

	if (!strcmp(tokens[0], "selector"))
		return selector_selector_statement_parse(block_mask,
							 tokens,
							 n_tokens,
							 n_lines,
							 err_line,
							 err_msg);

	if (!strcmp(tokens[0], "member_id")) {
		if (n_tokens != 2) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Invalid member_id statement.";
			return -EINVAL;
		}

		s->params.member_id_field_name = strdup(tokens[1]);
		if (!s->params.member_id_field_name) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Memory allocation failed.";
			return -ENOMEM;
		}

		return 0;
	}

	if (!strcmp(tokens[0], "n_groups_max")) {
		char *p = tokens[1];

		if (n_tokens != 2) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Invalid n_groups statement.";
			return -EINVAL;
		}

		s->params.n_groups_max = strtoul(p, &p, 0);
		if (p[0]) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Invalid n_groups argument.";
			return -EINVAL;
		}

		return 0;
	}

	if (!strcmp(tokens[0], "n_members_per_group_max")) {
		char *p = tokens[1];

		if (n_tokens != 2) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Invalid n_members_per_group statement.";
			return -EINVAL;
		}

		s->params.n_members_per_group_max = strtoul(p, &p, 0);
		if (p[0]) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Invalid n_members_per_group argument.";
			return -EINVAL;
		}

		return 0;
	}

	/* Anything else. */
	if (err_line)
		*err_line = n_lines;
	if (err_msg)
		*err_msg = "Invalid statement.";
	return -EINVAL;
}

/*
 * learner.
 */
static void
learner_spec_free(struct learner_spec *s)
{
	uintptr_t default_action_name, default_action_args, hash_func_name;
	uint32_t i;

	if (!s)
		return;

	free(s->name);
	s->name = NULL;

	for (i = 0; i < s->params.n_fields; i++) {
		uintptr_t name = (uintptr_t)s->params.field_names[i];

		free((void *)name);
	}

	free(s->params.field_names);
	s->params.field_names = NULL;

	s->params.n_fields = 0;

	for (i = 0; i < s->params.n_actions; i++) {
		uintptr_t name = (uintptr_t)s->params.action_names[i];

		free((void *)name);
	}

	free(s->params.action_names);
	s->params.action_names = NULL;

	s->params.n_actions = 0;

	default_action_name = (uintptr_t)s->params.default_action_name;
	free((void *)default_action_name);
	s->params.default_action_name = NULL;

	default_action_args = (uintptr_t)s->params.default_action_args;
	free((void *)default_action_args);
	s->params.default_action_args = NULL;

	free(s->params.action_is_for_table_entries);
	s->params.action_is_for_table_entries = NULL;

	free(s->params.action_is_for_default_entry);
	s->params.action_is_for_default_entry = NULL;

	s->params.default_action_is_const = 0;

	hash_func_name = (uintptr_t)s->params.hash_func_name;
	free((void *)hash_func_name);
	s->params.hash_func_name = NULL;

	s->size = 0;

	free(s->timeout);
	s->timeout = NULL;

	s->n_timeouts = 0;
}

static int
learner_key_statement_parse(uint32_t *block_mask,
			    char **tokens,
			    uint32_t n_tokens,
			    uint32_t n_lines,
			    uint32_t *err_line,
			    const char **err_msg)
{
	/* Check format. */
	if ((n_tokens != 2) || strcmp(tokens[1], "{")) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid key statement.";
		return -EINVAL;
	}

	/* block_mask. */
	*block_mask |= 1 << LEARNER_KEY_BLOCK;

	return 0;
}

static int
learner_key_block_parse(struct learner_spec *s,
			uint32_t *block_mask,
			char **tokens,
			uint32_t n_tokens,
			uint32_t n_lines,
			uint32_t *err_line,
			const char **err_msg)
{
	const char **new_field_names = NULL;
	char *field_name = NULL;

	/* Handle end of block. */
	if ((n_tokens == 1) && !strcmp(tokens[0], "}")) {
		*block_mask &= ~(1 << LEARNER_KEY_BLOCK);
		return 0;
	}

	/* Check input arguments. */
	if (n_tokens != 1) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid match field statement.";
		return -EINVAL;
	}

	field_name = strdup(tokens[0]);
	new_field_names = realloc(s->params.field_names, (s->params.n_fields + 1) * sizeof(char *));
	if (!field_name || !new_field_names) {
		free(field_name);
		free(new_field_names);

		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return -ENOMEM;
	}

	s->params.field_names = new_field_names;
	s->params.field_names[s->params.n_fields] = field_name;
	s->params.n_fields++;

	return 0;
}

static int
learner_actions_statement_parse(uint32_t *block_mask,
				char **tokens,
				uint32_t n_tokens,
				uint32_t n_lines,
				uint32_t *err_line,
				const char **err_msg)
{
	/* Check format. */
	if ((n_tokens != 2) || strcmp(tokens[1], "{")) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid actions statement.";
		return -EINVAL;
	}

	/* block_mask. */
	*block_mask |= 1 << LEARNER_ACTIONS_BLOCK;

	return 0;
}

static int
learner_actions_block_parse(struct learner_spec *s,
			    uint32_t *block_mask,
			    char **tokens,
			    uint32_t n_tokens,
			    uint32_t n_lines,
			    uint32_t *err_line,
			    const char **err_msg)
{
	const char **new_action_names = NULL;
	int *new_action_is_for_table_entries = NULL, *new_action_is_for_default_entry = NULL;
	char *name = NULL;
	int action_is_for_table_entries = 1, action_is_for_default_entry = 1;

	/* Handle end of block. */
	if ((n_tokens == 1) && !strcmp(tokens[0], "}")) {
		*block_mask &= ~(1 << LEARNER_ACTIONS_BLOCK);
		return 0;
	}

	/* Check input arguments. */
	if ((n_tokens > 2) ||
	    ((n_tokens == 2) && strcmp(tokens[1], "@tableonly") &&
	      strcmp(tokens[1], "@defaultonly"))) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid action name statement.";
		return -EINVAL;
	}

	name = strdup(tokens[0]);

	if (n_tokens == 2) {
		if (!strcmp(tokens[1], "@tableonly"))
			action_is_for_default_entry = 0;

		if (!strcmp(tokens[1], "@defaultonly"))
			action_is_for_table_entries = 0;
	}

	new_action_names = realloc(s->params.action_names,
				   (s->params.n_actions + 1) * sizeof(char *));
	new_action_is_for_table_entries = realloc(s->params.action_is_for_table_entries,
						  (s->params.n_actions + 1) * sizeof(int));
	new_action_is_for_default_entry = realloc(s->params.action_is_for_default_entry,
						  (s->params.n_actions + 1) * sizeof(int));

	if (!name ||
	    !new_action_names ||
	    !new_action_is_for_table_entries ||
	    !new_action_is_for_default_entry) {
		free(name);
		free(new_action_names);
		free(new_action_is_for_table_entries);
		free(new_action_is_for_default_entry);

		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return -ENOMEM;
	}

	s->params.action_names = new_action_names;
	s->params.action_names[s->params.n_actions] = name;

	s->params.action_is_for_table_entries = new_action_is_for_table_entries;
	s->params.action_is_for_table_entries[s->params.n_actions] = action_is_for_table_entries;

	s->params.action_is_for_default_entry = new_action_is_for_default_entry;
	s->params.action_is_for_default_entry[s->params.n_actions] = action_is_for_default_entry;

	s->params.n_actions++;

	return 0;
}

static int
learner_default_action_statement_parse(struct learner_spec *s,
				       char **tokens,
				       uint32_t n_tokens,
				       uint32_t n_lines,
				       uint32_t *err_line,
				       const char **err_msg)
{
	uint32_t i;
	int status = 0, duplicate = 0;

	/* Check format. */
	if ((n_tokens < 4) ||
	    strcmp(tokens[2], "args")) {
		status = -EINVAL;
		goto error;
	}

	if (s->params.default_action_name) {
		duplicate = 1;
		status = -EINVAL;
		goto error;
	}

	s->params.default_action_name = strdup(tokens[1]);
	if (!s->params.default_action_name) {
		status = -ENOMEM;
		goto error;
	}

	if (strcmp(tokens[3], "none")) {
		char buffer[MAX_LINE_LENGTH];
		uint32_t n_tokens_args = n_tokens - 3;

		if (!strcmp(tokens[n_tokens - 1], "const"))
			n_tokens_args--;

		if (!n_tokens_args) {
			status = -EINVAL;
			goto error;
		}

		buffer[0] = 0;
		for (i = 0; i < n_tokens_args; i++) {
			if (i)
				strcat(buffer, " ");

			strcat(buffer, tokens[3 + i]);
		}

		s->params.default_action_args = strdup(buffer);
		if (!s->params.default_action_args) {
			status = -ENOMEM;
			goto error;
		}
	} else {
		if (((n_tokens != 4) && (n_tokens != 5)) ||
		    ((n_tokens == 5) && (strcmp(tokens[4], "const")))) {
			status = -EINVAL;
			goto error;
		}
	}

	if (!strcmp(tokens[n_tokens - 1], "const"))
		s->params.default_action_is_const = 1;

	return 0;

error:
	if (err_line)
		*err_line = n_lines;

	if (err_msg)
		switch (status) {
		case -ENOMEM:
			*err_msg = "Memory allocation failed.";
			break;

		default:
			if (duplicate)
				*err_msg = "Duplicate default_action statement.";

			*err_msg = "Invalid default_action statement.";
		}

	return status;
}

static int
learner_timeout_statement_parse(uint32_t *block_mask,
				char **tokens,
				uint32_t n_tokens,
				uint32_t n_lines,
				uint32_t *err_line,
				const char **err_msg)
{
	/* Check format. */
	if ((n_tokens != 2) || strcmp(tokens[1], "{")) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid timeout statement.";
		return -EINVAL;
	}

	/* block_mask. */
	*block_mask |= 1 << LEARNER_TIMEOUT_BLOCK;

	return 0;
}

static int
learner_timeout_block_parse(struct learner_spec *s,
			    uint32_t *block_mask,
			    char **tokens,
			    uint32_t n_tokens,
			    uint32_t n_lines,
			    uint32_t *err_line,
			    const char **err_msg)
{
	uint32_t *new_timeout = NULL;
	char *str;
	uint32_t val;
	int status = 0;

	/* Handle end of block. */
	if ((n_tokens == 1) && !strcmp(tokens[0], "}")) {
		*block_mask &= ~(1 << LEARNER_TIMEOUT_BLOCK);
		return 0;
	}

	/* Check input arguments. */
	if (n_tokens != 1) {
		status = -EINVAL;
		goto error;
	}

	str = tokens[0];
	val = strtoul(str, &str, 0);
	if (str[0]) {
		status = -EINVAL;
		goto error;
	}

	new_timeout = realloc(s->timeout, (s->n_timeouts + 1) * sizeof(uint32_t));
	if (!new_timeout) {
		status = -ENOMEM;
		goto error;
	}

	s->timeout = new_timeout;
	s->timeout[s->n_timeouts] = val;
	s->n_timeouts++;

	return 0;

error:
	free(new_timeout);

	if (err_line)
		*err_line = n_lines;

	if (err_msg)
		switch (status) {
		case -ENOMEM:
			*err_msg = "Memory allocation failed.";
			break;

		default:
			*err_msg = "Invalid timeout value statement.";
			break;
		}

	return status;
}


static int
learner_statement_parse(struct learner_spec *s,
		      uint32_t *block_mask,
		      char **tokens,
		      uint32_t n_tokens,
		      uint32_t n_lines,
		      uint32_t *err_line,
		      const char **err_msg)
{
	/* Check format. */
	if ((n_tokens != 3) || strcmp(tokens[2], "{")) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid learner statement.";
		return -EINVAL;
	}

	/* spec. */
	s->name = strdup(tokens[1]);
	if (!s->name) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return -ENOMEM;
	}

	/* block_mask. */
	*block_mask |= 1 << LEARNER_BLOCK;

	return 0;
}

static int
learner_block_parse(struct learner_spec *s,
		    uint32_t *block_mask,
		    char **tokens,
		    uint32_t n_tokens,
		    uint32_t n_lines,
		    uint32_t *err_line,
		    const char **err_msg)
{
	if (*block_mask & (1 << LEARNER_KEY_BLOCK))
		return learner_key_block_parse(s,
					       block_mask,
					       tokens,
					       n_tokens,
					       n_lines,
					       err_line,
					       err_msg);

	if (*block_mask & (1 << LEARNER_ACTIONS_BLOCK))
		return learner_actions_block_parse(s,
						   block_mask,
						   tokens,
						   n_tokens,
						   n_lines,
						   err_line,
						   err_msg);

	if (*block_mask & (1 << LEARNER_TIMEOUT_BLOCK))
		return learner_timeout_block_parse(s,
						   block_mask,
						   tokens,
						   n_tokens,
						   n_lines,
						   err_line,
						   err_msg);

	/* Handle end of block. */
	if ((n_tokens == 1) && !strcmp(tokens[0], "}")) {
		*block_mask &= ~(1 << LEARNER_BLOCK);
		return 0;
	}

	if (!strcmp(tokens[0], "key"))
		return learner_key_statement_parse(block_mask,
						   tokens,
						   n_tokens,
						   n_lines,
						   err_line,
						   err_msg);

	if (!strcmp(tokens[0], "actions"))
		return learner_actions_statement_parse(block_mask,
						       tokens,
						       n_tokens,
						       n_lines,
						       err_line,
						       err_msg);

	if (!strcmp(tokens[0], "default_action"))
		return learner_default_action_statement_parse(s,
							      tokens,
							      n_tokens,
							      n_lines,
							      err_line,
							      err_msg);

	if (!strcmp(tokens[0], "hash")) {
		if (n_tokens != 2) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Invalid hash statement.";
			return -EINVAL;
		}

		if (s->params.hash_func_name) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Duplicate hash statement.";
			return -EINVAL;
		}

		s->params.hash_func_name = strdup(tokens[1]);
		if (!s->params.hash_func_name) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Memory allocation failed.";
			return -ENOMEM;
		}

		return 0;
	}

	if (!strcmp(tokens[0], "size")) {
		char *p = tokens[1];

		if (n_tokens != 2) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Invalid size statement.";
			return -EINVAL;
		}

		s->size = strtoul(p, &p, 0);
		if (p[0]) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Invalid size argument.";
			return -EINVAL;
		}

		return 0;
	}

	if (!strcmp(tokens[0], "timeout"))
		return learner_timeout_statement_parse(block_mask,
						       tokens,
						       n_tokens,
						       n_lines,
						       err_line,
						       err_msg);

	/* Anything else. */
	if (err_line)
		*err_line = n_lines;
	if (err_msg)
		*err_msg = "Invalid statement.";
	return -EINVAL;
}

/*
 * regarray.
 */
static void
regarray_spec_free(struct regarray_spec *s)
{
	if (!s)
		return;

	free(s->name);
	s->name = NULL;
}

static int
regarray_statement_parse(struct regarray_spec *s,
			 char **tokens,
			 uint32_t n_tokens,
			 uint32_t n_lines,
			 uint32_t *err_line,
			 const char **err_msg)
{
	char *p;

	/* Check format. */
	if ((n_tokens != 6) ||
	     strcmp(tokens[2], "size") ||
	     strcmp(tokens[4], "initval")) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid regarray statement.";
		return -EINVAL;
	}

	/* spec. */
	s->name = strdup(tokens[1]);
	if (!s->name) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return -ENOMEM;
	}

	p = tokens[3];
	s->size = strtoul(p, &p, 0);
	if (p[0] || !s->size) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid size argument.";
		return -EINVAL;
	}

	p = tokens[5];
	s->init_val = strtoull(p, &p, 0);
	if (p[0]) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid initval argument.";
		return -EINVAL;
	}

	return 0;
}

/*
 * metarray.
 */
static void
metarray_spec_free(struct metarray_spec *s)
{
	if (!s)
		return;

	free(s->name);
	s->name = NULL;
}

static int
metarray_statement_parse(struct metarray_spec *s,
			 char **tokens,
			 uint32_t n_tokens,
			 uint32_t n_lines,
			 uint32_t *err_line,
			 const char **err_msg)
{
	char *p;

	/* Check format. */
	if ((n_tokens != 4) || strcmp(tokens[2], "size")) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid metarray statement.";
		return -EINVAL;
	}

	/* spec. */
	s->name = strdup(tokens[1]);
	if (!s->name) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return -ENOMEM;
	}

	p = tokens[3];
	s->size = strtoul(p, &p, 0);
	if (p[0] || !s->size) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid size argument.";
		return -EINVAL;
	}

	return 0;
}

/*
 *
 * rss
 */

static void
rss_spec_free(struct rss_spec *s)
{
	if (!s)
		return;

	free(s->name);
	s->name = NULL;
}

static int
rss_statement_parse(struct rss_spec *s,
			 char **tokens,
			 uint32_t n_tokens,
			 uint32_t n_lines,
			 uint32_t *err_line,
			 const char **err_msg)
{
	/* Check format. */
	if ((n_tokens != 2)) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid rss statement.";
		return -EINVAL;
	}

	/* spec. */
	s->name = strdup(tokens[1]);
	if (!s->name) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return -ENOMEM;
	}

	return 0;
}

/*
 * apply.
 */
static void
apply_spec_free(struct apply_spec *s)
{
	uint32_t i;

	if (!s)
		return;

	for (i = 0; i < s->n_instructions; i++) {
		uintptr_t instr = (uintptr_t)s->instructions[i];

		free((void *)instr);
	}

	free(s->instructions);
	s->instructions = NULL;

	s->n_instructions = 0;
}

static int
apply_statement_parse(uint32_t *block_mask,
		      char **tokens,
		      uint32_t n_tokens,
		      uint32_t n_lines,
		      uint32_t *err_line,
		      const char **err_msg)
{
	/* Check format. */
	if ((n_tokens != 2) || strcmp(tokens[1], "{")) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid apply statement.";
		return -EINVAL;
	}

	/* block_mask. */
	*block_mask |= 1 << APPLY_BLOCK;

	return 0;
}

static int
apply_block_parse(struct apply_spec *s,
		  uint32_t *block_mask,
		  char **tokens,
		  uint32_t n_tokens,
		  uint32_t n_lines,
		  uint32_t *err_line,
		  const char **err_msg)
{
	char buffer[RTE_SWX_INSTRUCTION_SIZE], *instr;
	const char **new_instructions;
	uint32_t i;

	/* Handle end of block. */
	if ((n_tokens == 1) && !strcmp(tokens[0], "}")) {
		*block_mask &= ~(1 << APPLY_BLOCK);
		return 0;
	}

	/* spec. */
	buffer[0] = 0;
	for (i = 0; i < n_tokens; i++) {
		if (i)
			strcat(buffer, " ");
		strcat(buffer, tokens[i]);
	}

	instr = strdup(buffer);
	if (!instr) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return -ENOMEM;
	}

	new_instructions = realloc(s->instructions,
				   (s->n_instructions + 1) * sizeof(char *));
	if (!new_instructions) {
		free(instr);

		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return -ENOMEM;
	}

	s->instructions = new_instructions;
	s->instructions[s->n_instructions] = instr;
	s->n_instructions++;

	return 0;
}

/*
 * Pipeline.
 */
void
pipeline_spec_free(struct pipeline_spec *s)
{
	if (!s)
		return;

	free(s->extobjs);
	free(s->structs);
	free(s->headers);
	free(s->metadata);
	free(s->actions);
	free(s->tables);
	free(s->selectors);
	free(s->learners);
	free(s->regarrays);
	free(s->metarrays);
	free(s->apply);

	memset(s, 0, sizeof(struct pipeline_spec));
}

static const char *
match_type_string_get(enum rte_swx_table_match_type match_type)
{
	switch (match_type) {
	case RTE_SWX_TABLE_MATCH_WILDCARD: return "RTE_SWX_TABLE_MATCH_WILDCARD";
	case RTE_SWX_TABLE_MATCH_LPM: return "RTE_SWX_TABLE_MATCH_LPM";
	case RTE_SWX_TABLE_MATCH_EXACT: return "RTE_SWX_TABLE_MATCH_EXACT";
	default: return "RTE_SWX_TABLE_MATCH_UNKNOWN";
	}
}

void
pipeline_spec_codegen(FILE *f,
		      struct pipeline_spec *s)
{
	uint32_t i;

	/* Check the input arguments. */
	if (!f || !s)
		return;

	/* extobj. */
	fprintf(f, "static struct extobj_spec extobjs[] = {\n");

	for (i = 0; i < s->n_extobjs; i++) {
		struct extobj_spec *extobj_spec = &s->extobjs[i];

		fprintf(f, "\t[%d] = {\n", i);
		fprintf(f, "\t\t.name = \"%s\",\n", extobj_spec->name);
		fprintf(f, "\t\t.extern_type_name = \"%s\",\n", extobj_spec->extern_type_name);
		if (extobj_spec->pragma)
			fprintf(f, "\t\t.pragma = \"%s\",\n", extobj_spec->pragma);
		else
			fprintf(f, "\t\t.pragma = NULL,\n");
		fprintf(f, "\t},\n");
	}

	fprintf(f, "};\n\n");

	/* regarray. */
	fprintf(f, "static struct regarray_spec regarrays[] = {\n");

	for (i = 0; i < s->n_regarrays; i++) {
		struct regarray_spec *regarray_spec = &s->regarrays[i];

		fprintf(f, "\t[%d] = {\n", i);
		fprintf(f, "\t\t.name = \"%s\",\n", regarray_spec->name);
		fprintf(f, "\t\t.init_val = %" PRIu64 ",\n", regarray_spec->init_val);
		fprintf(f, "\t\t.size = %u,\n", regarray_spec->size);
		fprintf(f, "\t},\n");
	}

	fprintf(f, "};\n\n");

	/* metarray. */
	fprintf(f, "static struct metarray_spec metarrays[] = {\n");

	for (i = 0; i < s->n_metarrays; i++) {
		struct metarray_spec *metarray_spec = &s->metarrays[i];

		fprintf(f, "\t[%d] = {\n", i);
		fprintf(f, "\t\t.name = \"%s\",\n", metarray_spec->name);
		fprintf(f, "\t\t.size = %u,\n", metarray_spec->size);
		fprintf(f, "\t},\n");
	}

	fprintf(f, "};\n\n");

	/* rss. */
	fprintf(f, "static struct rss_spec rss[] = {\n");

	for (i = 0; i < s->n_rss; i++) {
		struct rss_spec *rss_spec = &s->rss[i];
		fprintf(f, "\t[%d] = {\n", i);
		fprintf(f, "\t\t.name = \"%s\",\n", rss_spec->name);
		fprintf(f, "\t},\n");
	}
	fprintf(f, "};\n\n");

	/* struct. */
	for (i = 0; i < s->n_structs; i++) {
		struct struct_spec *struct_spec = &s->structs[i];
		uint32_t j;

		fprintf(f, "static struct rte_swx_field_params struct_%s_fields[] = {\n",
			struct_spec->name);

		for (j = 0; j < struct_spec->n_fields; j++) {
			struct rte_swx_field_params *field = &struct_spec->fields[j];

			fprintf(f, "\t[%d] = {\n", j);
			fprintf(f, "\t\t.name = \"%s\",\n", field->name);
			fprintf(f, "\t\t.n_bits = %u,\n", field->n_bits);
			fprintf(f, "\t},\n");
		}

		fprintf(f, "};\n\n");
	}

	fprintf(f, "static struct struct_spec structs[] = {\n");

	for (i = 0; i < s->n_structs; i++) {
		struct struct_spec *struct_spec = &s->structs[i];

		fprintf(f, "\t[%d] = {\n", i);
		fprintf(f, "\t\t.name = \"%s\",\n", struct_spec->name);
		fprintf(f, "\t\t.fields = struct_%s_fields,\n", struct_spec->name);
		fprintf(f, "\t\t.n_fields = "
			"sizeof(struct_%s_fields) / sizeof(struct_%s_fields[0]),\n",
			struct_spec->name,
			struct_spec->name);
		fprintf(f, "\t\t.varbit = %d,\n", struct_spec->varbit);
		fprintf(f, "\t},\n");
	}

	fprintf(f, "};\n\n");

	/* header. */
	fprintf(f, "static struct header_spec headers[] = {\n");

	for (i = 0; i < s->n_headers; i++) {
		struct header_spec *header_spec = &s->headers[i];

		fprintf(f, "\t[%d] = {\n", i);
		fprintf(f, "\t\t.name = \"%s\",\n", header_spec->name);
		fprintf(f, "\t\t.struct_type_name = \"%s\",\n", header_spec->struct_type_name);
		fprintf(f, "\t},\n");
	}

	fprintf(f, "};\n\n");

	/* metadata. */
	fprintf(f, "static struct metadata_spec metadata[] = {\n");

	for (i = 0; i < s->n_metadata; i++) {
		struct metadata_spec *metadata_spec = &s->metadata[i];

		fprintf(f, "\t[%d] = {\n", i);
		fprintf(f, "\t\t.struct_type_name = \"%s\",\n", metadata_spec->struct_type_name);
		fprintf(f, "\t},\n");

	}

	fprintf(f, "};\n\n");

	/* action. */
	for (i = 0; i < s->n_actions; i++) {
		struct action_spec *action_spec = &s->actions[i];
		uint32_t j;

		fprintf(f, "static const char *action_%s_initial_instructions[] = {\n",
			action_spec->name);

		for (j = 0; j < action_spec->n_instructions; j++) {
			const char *instr = action_spec->instructions[j];

			fprintf(f, "\t[%d] = \"%s\",\n", j, instr);
		}

		fprintf(f, "};\n\n");
	}

	fprintf(f, "static struct action_spec actions[] = {\n");

	for (i = 0; i < s->n_actions; i++) {
		struct action_spec *action_spec = &s->actions[i];

		fprintf(f, "\t[%d] = {\n", i);
		fprintf(f, "\t\t.name = \"%s\",\n", action_spec->name);

		if (action_spec->args_struct_type_name)
			fprintf(f, "\t\t.args_struct_type_name = \"%s\",\n",
				action_spec->args_struct_type_name);
		else
			fprintf(f, "\t\t.args_struct_type_name = NULL,\n");

		fprintf(f, "\t\t.instructions = action_%s_initial_instructions,\n",
			action_spec->name);
		fprintf(f, "\t\t.n_instructions = "
			"sizeof(action_%s_initial_instructions) / "
			"sizeof(action_%s_initial_instructions[0]),\n",
			action_spec->name,
			action_spec->name);
		fprintf(f, "\t},\n");
	}

	fprintf(f, "};\n\n");

	/* table. */
	for (i = 0; i < s->n_tables; i++) {
		struct table_spec *table_spec = &s->tables[i];
		uint32_t j;

		/* fields. */
		if (table_spec->params.fields && table_spec->params.n_fields) {
			fprintf(f, "static struct rte_swx_match_field_params "
				"table_%s_fields[] = {\n",
				table_spec->name);

			for (j = 0; j < table_spec->params.n_fields; j++) {
				struct rte_swx_match_field_params *field =
					&table_spec->params.fields[j];

				fprintf(f, "\t[%d] = {\n", j);
				fprintf(f, "\t\t.name = \"%s\",\n", field->name);
				fprintf(f, "\t\t.match_type = %s,\n",
					match_type_string_get(field->match_type));
				fprintf(f, "\t},\n");
			}

			fprintf(f, "};\n\n");
		}

		/* action_names. */
		if (table_spec->params.action_names && table_spec->params.n_actions) {
			fprintf(f, "static const char *table_%s_action_names[] = {\n",
				table_spec->name);

			for (j = 0; j < table_spec->params.n_actions; j++) {
				const char *action_name = table_spec->params.action_names[j];

				fprintf(f, "\t[%d] = \"%s\",\n", j, action_name);
			}

			fprintf(f, "};\n\n");
		}

		/* action_is_for_table_entries. */
		if (table_spec->params.action_is_for_table_entries &&
		    table_spec->params.n_actions) {
			fprintf(f, "static int table_%s_action_is_for_table_entries[] = {\n",
				table_spec->name);

			for (j = 0; j < table_spec->params.n_actions; j++) {
				int value = table_spec->params.action_is_for_table_entries[j];

				fprintf(f, "\t[%d] = %d,\n", j, value);
			}

			fprintf(f, "};\n\n");
		}

		/* action_is_for_default_entry. */
		if (table_spec->params.action_is_for_default_entry &&
		    table_spec->params.n_actions) {
			fprintf(f, "static int table_%s_action_is_for_default_entry[] = {\n",
				table_spec->name);

			for (j = 0; j < table_spec->params.n_actions; j++) {
				int value = table_spec->params.action_is_for_default_entry[j];

				fprintf(f, "\t[%d] = %d,\n", j, value);
			}

			fprintf(f, "};\n\n");
		}
	}

	fprintf(f, "static struct table_spec tables[] = {\n");

	for (i = 0; i < s->n_tables; i++) {
		struct table_spec *table_spec = &s->tables[i];

		fprintf(f, "\t[%d] = {\n", i);
		fprintf(f, "\t\t.name = \"%s\",\n", table_spec->name);

		fprintf(f, "\t\t.params = {\n");

		if (table_spec->params.fields && table_spec->params.n_fields) {
			fprintf(f, "\t\t\t.fields = table_%s_fields,\n", table_spec->name);
			fprintf(f, "\t\t\t.n_fields = "
				"sizeof(table_%s_fields) / sizeof(table_%s_fields[0]),\n",
				table_spec->name,
				table_spec->name);
		} else {
			fprintf(f, "\t\t\t.fields = NULL,\n");
			fprintf(f, "\t\t\t.n_fields = 0,\n");
		}

		if (table_spec->params.action_names && table_spec->params.n_actions)
			fprintf(f, "\t\t\t.action_names = table_%s_action_names,\n",
				table_spec->name);
		else
			fprintf(f, "\t\t\t.action_names = NULL,\n");

		if (table_spec->params.action_is_for_table_entries && table_spec->params.n_actions)
			fprintf(f, "\t\t\t.action_is_for_table_entries = "
				"table_%s_action_is_for_table_entries,\n",
				table_spec->name);
		else
			fprintf(f, "\t\t\t.action_is_for_table_entries = NULL,\n");

		if (table_spec->params.action_is_for_default_entry && table_spec->params.n_actions)
			fprintf(f, "\t\t\t.action_is_for_default_entry = "
				"table_%s_action_is_for_default_entry,\n",
				table_spec->name);
		else
			fprintf(f, "\t\t\t.action_is_for_default_entry = NULL,\n");

		if (table_spec->params.n_actions)
			fprintf(f, "\t\t\t.n_actions = sizeof(table_%s_action_names) / "
				"sizeof(table_%s_action_names[0]),\n",
				table_spec->name,
				table_spec->name);
		else
			fprintf(f, "\t\t\t.n_actions = 0,\n");

		if (table_spec->params.default_action_name)
			fprintf(f, "\t\t\t.default_action_name = \"%s\",\n",
				table_spec->params.default_action_name);
		else
			fprintf(f, "\t\t\t.default_action_name = NULL,\n");

		if (table_spec->params.default_action_args)
			fprintf(f, "\t\t\t.default_action_args = \"%s\",\n",
				table_spec->params.default_action_args);
		else
			fprintf(f, "\t\t\t.default_action_args = NULL,\n");

		fprintf(f, "\t\t\t.default_action_is_const = %d,\n",
			table_spec->params.default_action_is_const);

		if (table_spec->params.hash_func_name)
			fprintf(f, "\t\t\t.hash_func_name = \"%s\",\n",
				table_spec->params.hash_func_name);
		else
			fprintf(f, "\t\t\t.hash_func_name = NULL,\n");

		fprintf(f, "\t\t},\n");

		if (table_spec->recommended_table_type_name)
			fprintf(f, "\t\t.recommended_table_type_name = \"%s\",\n",
				table_spec->recommended_table_type_name);
		else
			fprintf(f, "\t\t.recommended_table_type_name = NULL,\n");

		if (table_spec->args)
			fprintf(f, "\t\t.args = \"%s\",\n", table_spec->args);
		else
			fprintf(f, "\t\t.args = NULL,\n");

		fprintf(f, "\t\t.size = %u,\n", table_spec->size);

		fprintf(f, "\t},\n");
	}

	fprintf(f, "};\n\n");

	/* selector. */
	for (i = 0; i < s->n_selectors; i++) {
		struct selector_spec *selector_spec = &s->selectors[i];
		uint32_t j;

		if (selector_spec->params.selector_field_names &&
		    selector_spec->params.n_selector_fields) {
			fprintf(f, "static const char *selector_%s_field_names[] = {\n",
				selector_spec->name);

			for (j = 0; j < selector_spec->params.n_selector_fields; j++) {
				const char *field_name =
					selector_spec->params.selector_field_names[j];

				fprintf(f, "\t[%d] = \"%s\",\n", j, field_name);
			}

			fprintf(f, "};\n\n");
		}
	}

	fprintf(f, "static struct selector_spec selectors[] = {\n");

	for (i = 0; i < s->n_selectors; i++) {
		struct selector_spec *selector_spec = &s->selectors[i];

		fprintf(f, "\t[%d] = {\n", i);

		fprintf(f, "\t\t.name = \"%s\",\n", selector_spec->name);
		fprintf(f, "\t\t.params = {\n");

		if (selector_spec->params.group_id_field_name)
			fprintf(f, "\t\t\t.group_id_field_name = \"%s\",\n",
				selector_spec->params.group_id_field_name);
		else
			fprintf(f, "\t\t\t.group_id_field_name = NULL,\n");

		if (selector_spec->params.selector_field_names &&
		    selector_spec->params.n_selector_fields) {
			fprintf(f, "\t\t\t.selector_field_names = selector_%s_field_names,\n",
				selector_spec->name);
			fprintf(f, "\t\t\t.n_selector_fields = "
				"sizeof(selector_%s_field_names) / sizeof(selector_%s_field_names[0]),\n",
				selector_spec->name,
				selector_spec->name);
		} else {
			fprintf(f, "\t\t\t.selector_field_names = NULL,\n");
			fprintf(f, "\t\t\t.n_selector_fields = 0,\n");
		}

		if (selector_spec->params.member_id_field_name)
			fprintf(f, "\t\t\t.member_id_field_name = \"%s\",\n",
				selector_spec->params.member_id_field_name);
		else
			fprintf(f, "\t\t\t.member_id_field_name = NULL,\n");

		fprintf(f, "\t\t\t.n_groups_max = %u,\n", selector_spec->params.n_groups_max);

		fprintf(f, "\t\t\t.n_members_per_group_max = %u,\n",
			selector_spec->params.n_members_per_group_max);

		fprintf(f, "\t\t},\n");
		fprintf(f, "\t},\n");
	}

	fprintf(f, "};\n\n");

	/* learner. */
	for (i = 0; i < s->n_learners; i++) {
		struct learner_spec *learner_spec = &s->learners[i];
		uint32_t j;

		/* field_names. */
		if (learner_spec->params.field_names && learner_spec->params.n_fields) {
			fprintf(f, "static const char *learner_%s_field_names[] = {\n",
				learner_spec->name);

			for (j = 0; j < learner_spec->params.n_fields; j++) {
				const char *field_name = learner_spec->params.field_names[j];

				fprintf(f, "\t[%d] = \"%s\",\n", j, field_name);
			}

			fprintf(f, "};\n\n");
		}

		/* action_names. */
		if (learner_spec->params.action_names && learner_spec->params.n_actions) {
			fprintf(f, "static const char *learner_%s_action_names[] = {\n",
				learner_spec->name);

			for (j = 0; j < learner_spec->params.n_actions; j++) {
				const char *action_name = learner_spec->params.action_names[j];

				fprintf(f, "\t[%d] = \"%s\",\n", j, action_name);
			}

			fprintf(f, "};\n\n");
		}

		/* action_is_for_table_entries. */
		if (learner_spec->params.action_is_for_table_entries &&
		    learner_spec->params.n_actions) {
			fprintf(f, "static int learner_%s_action_is_for_table_entries[] = {\n",
				learner_spec->name);

			for (j = 0; j < learner_spec->params.n_actions; j++) {
				int value = learner_spec->params.action_is_for_table_entries[j];

				fprintf(f, "\t[%d] = %d,\n", j, value);
			}

			fprintf(f, "};\n\n");
		}

		/* action_is_for_default_entry. */
		if (learner_spec->params.action_is_for_default_entry &&
		    learner_spec->params.n_actions) {
			fprintf(f, "static int learner_%s_action_is_for_default_entry[] = {\n",
				learner_spec->name);

			for (j = 0; j < learner_spec->params.n_actions; j++) {
				int value = learner_spec->params.action_is_for_default_entry[j];

				fprintf(f, "\t[%d] = %d,\n", j, value);
			}

			fprintf(f, "};\n\n");
		}

		/* timeout. */
		if (learner_spec->timeout && learner_spec->n_timeouts) {
			fprintf(f, "static uint32_t learner_%s_timeout[] = {\n",
				learner_spec->name);

			for (j = 0; j < learner_spec->n_timeouts; j++) {
				uint32_t value = learner_spec->timeout[j];

				fprintf(f, "\t[%d] = %u,\n", j, value);
			}

			fprintf(f, "};\n\n");
		}
	}

	fprintf(f, "static struct learner_spec learners[] = {\n");

	for (i = 0; i < s->n_learners; i++) {
		struct learner_spec *learner_spec = &s->learners[i];

		fprintf(f, "\t[%d] = {\n", i);
		fprintf(f, "\t\t.name = \"%s\",\n", learner_spec->name);

		fprintf(f, "\t\t.params = {\n");

		if (learner_spec->params.field_names && learner_spec->params.n_fields) {
			fprintf(f, "\t\t\t.field_names = learner_%s_field_names,\n",
				learner_spec->name);
			fprintf(f, "\t\t\t.n_fields = "
				"sizeof(learner_%s_field_names) / "
				"sizeof(learner_%s_field_names[0]),\n",
				learner_spec->name,
				learner_spec->name);
		} else {
			fprintf(f, "\t\t\t.field_names = NULL,\n");
			fprintf(f, "\t\t\t.n_fields = 0,\n");
		}

		if (learner_spec->params.action_names && learner_spec->params.n_actions)
			fprintf(f, "\t\t\t.action_names = learner_%s_action_names,\n",
				learner_spec->name);
		else
			fprintf(f, "\t\t\t.action_names = NULL,\n");

		if (learner_spec->params.action_is_for_table_entries &&
		    learner_spec->params.n_actions)
			fprintf(f, "\t\t\t.action_is_for_table_entries = "
				"learner_%s_action_is_for_table_entries,\n",
				learner_spec->name);
		else
			fprintf(f, "\t\t\t.action_is_for_table_entries = NULL,\n");

		if (learner_spec->params.action_is_for_default_entry &&
		    learner_spec->params.n_actions)
			fprintf(f, "\t\t\t.action_is_for_default_entry = "
				"learner_%s_action_is_for_default_entry,\n",
				learner_spec->name);
		else
			fprintf(f, "\t\t\t.action_is_for_default_entry = NULL,\n");

		if (learner_spec->params.action_names && learner_spec->params.n_actions)
			fprintf(f, "\t\t\t.n_actions = "
				"sizeof(learner_%s_action_names) / sizeof(learner_%s_action_names[0]),\n",
				learner_spec->name,
				learner_spec->name);
		else
			fprintf(f, "\t\t\t.n_actions = NULL,\n");

		if (learner_spec->params.default_action_name)
			fprintf(f, "\t\t\t.default_action_name = \"%s\",\n",
				learner_spec->params.default_action_name);
		else
			fprintf(f, "\t\t\t.default_action_name = NULL,\n");

		if (learner_spec->params.default_action_args)
			fprintf(f, "\t\t\t.default_action_args = \"%s\",\n",
				learner_spec->params.default_action_args);
		else
			fprintf(f, "\t\t\t.default_action_args = NULL,\n");

		fprintf(f, "\t\t\t.default_action_is_const = %d,\n",
			learner_spec->params.default_action_is_const);

		if (learner_spec->params.hash_func_name)
			fprintf(f, "\t\t\t.hash_func_name = \"%s\",\n",
				learner_spec->params.hash_func_name);
		else
			fprintf(f, "\t\t\t.hash_func_name = NULL,\n");

		fprintf(f, "\t\t},\n");

		fprintf(f, "\t\t.size = %u,\n", learner_spec->size);

		if (learner_spec->timeout && learner_spec->n_timeouts) {
			fprintf(f, "\t\t.timeout = learner_%s_timeout,\n", learner_spec->name);
			fprintf(f, "\t\t\t.n_timeouts = "
				"sizeof(learner_%s_timeout) / sizeof(learner_%s_timeout[0]),\n",
				learner_spec->name,
				learner_spec->name);
		} else {
			fprintf(f, "\t\t.timeout = NULL,\n");
			fprintf(f, "\t\t\t.n_timeouts = 0,\n");
		}

		fprintf(f, "\t},\n");
	}

	fprintf(f, "};\n\n");

	/* apply. */
	for (i = 0; i < s->n_apply; i++) {
		struct apply_spec *apply_spec = &s->apply[i];
		uint32_t j;

		fprintf(f, "static const char *apply%u_initial_instructions[] = {\n", i);

		for (j = 0; j < apply_spec->n_instructions; j++) {
			const char *instr = apply_spec->instructions[j];

			fprintf(f, "\t[%d] = \"%s\",\n", j, instr);
		}

		fprintf(f, "};\n\n");
	}

	fprintf(f, "static struct apply_spec apply[] = {\n");

	for (i = 0; i < s->n_apply; i++) {
		fprintf(f, "\t[%d] = {\n", i);
		fprintf(f, "\t.instructions = apply%u_initial_instructions,\n", i);
		fprintf(f, "\t.n_instructions = "
			"sizeof(apply%u_initial_instructions) / "
			"sizeof(apply%u_initial_instructions[0]),\n",
			i,
			i);
		fprintf(f, "\t},\n");
	}

	fprintf(f, "};\n\n");

	/* pipeline. */
	fprintf(f, "struct pipeline_spec pipeline_spec = {\n");
	fprintf(f, "\t.extobjs = extobjs,\n");
	fprintf(f, "\t.structs = structs,\n");
	fprintf(f, "\t.headers = headers,\n");
	fprintf(f, "\t.metadata = metadata,\n");
	fprintf(f, "\t.actions = actions,\n");
	fprintf(f, "\t.tables = tables,\n");
	fprintf(f, "\t.selectors = selectors,\n");
	fprintf(f, "\t.learners = learners,\n");
	fprintf(f, "\t.regarrays = regarrays,\n");
	fprintf(f, "\t.metarrays = metarrays,\n");
	fprintf(f, "\t.rss = rss,\n");
	fprintf(f, "\t.apply = apply,\n");
	fprintf(f, "\t.n_extobjs = sizeof(extobjs) / sizeof(extobjs[0]),\n");
	fprintf(f, "\t.n_structs = sizeof(structs) / sizeof(structs[0]),\n");
	fprintf(f, "\t.n_headers = sizeof(headers) / sizeof(headers[0]),\n");
	fprintf(f, "\t.n_metadata = sizeof(metadata) / sizeof(metadata[0]),\n");
	fprintf(f, "\t.n_actions = sizeof(actions) / sizeof(actions[0]),\n");
	fprintf(f, "\t.n_tables = sizeof(tables) / sizeof(tables[0]),\n");
	fprintf(f, "\t.n_selectors = sizeof(selectors) / sizeof(selectors[0]),\n");
	fprintf(f, "\t.n_learners = sizeof(learners) / sizeof(learners[0]),\n");
	fprintf(f, "\t.n_regarrays = sizeof(regarrays) / sizeof(regarrays[0]),\n");
	fprintf(f, "\t.n_metarrays = sizeof(metarrays) / sizeof(metarrays[0]),\n");
	fprintf(f, "\t.n_rss = sizeof(rss) / sizeof(rss[0]),\n");
	fprintf(f, "\t.n_apply = sizeof(apply) / sizeof(apply[0]),\n");
	fprintf(f, "};\n");
}

struct pipeline_spec *
pipeline_spec_parse(FILE *spec,
		    uint32_t *err_line,
		    const char **err_msg)
{
	struct extobj_spec extobj_spec = {0};
	struct struct_spec struct_spec = {0};
	struct header_spec header_spec = {0};
	struct metadata_spec metadata_spec = {0};
	struct action_spec action_spec = {0};
	struct table_spec table_spec = {0};
	struct selector_spec selector_spec = {0};
	struct learner_spec learner_spec = {0};
	struct regarray_spec regarray_spec = {0};
	struct metarray_spec metarray_spec = {0};
	struct rss_spec rss_spec = {0};
	struct apply_spec apply_spec = {0};
	struct pipeline_spec *s = NULL;
	uint32_t n_lines = 0;
	uint32_t block_mask = 0;
	int status = 0;

	/* Check the input arguments. */
	if (!spec) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid input argument.";
		status = -EINVAL;
		goto error;
	}

	/* Memory allocation. */
	s = calloc(1, sizeof(struct pipeline_spec));
	if (!s) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		status = -ENOMEM;
		goto error;
	}

	for (n_lines = 1; ; n_lines++) {
		char line[MAX_LINE_LENGTH];
		char *tokens[MAX_TOKENS], *ptr = line;
		uint32_t n_tokens = 0;

		/* Read next line. */
		if (!fgets(line, sizeof(line), spec))
			break;

		/* Parse the line into tokens. */
		for ( ; ; ) {
			char *token;

			/* Get token. */
			token = strtok_r(ptr, " \f\n\r\t\v", &ptr);
			if (!token)
				break;

			/* Handle comments. */
			if ((token[0] == '#') ||
			    (token[0] == ';') ||
			    ((token[0] == '/') && (token[1] == '/'))) {
				break;
			}

			/* Handle excessively long lines. */
			if (n_tokens >= RTE_DIM(tokens)) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Too many tokens.";
				status = -EINVAL;
				goto error;
			}

			/* Handle excessively long tokens. */
			if (strnlen(token, RTE_SWX_NAME_SIZE) >=
			    RTE_SWX_NAME_SIZE) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Token too big.";
				status = -EINVAL;
				goto error;
			}

			/* Save token. */
			tokens[n_tokens] = token;
			n_tokens++;
		}

		/* Handle empty lines. */
		if (!n_tokens)
			continue;

		/* struct block. */
		if (block_mask & (1 << STRUCT_BLOCK)) {
			struct struct_spec *new_structs;

			status = struct_block_parse(&struct_spec,
						    &block_mask,
						    tokens,
						    n_tokens,
						    n_lines,
						    err_line,
						    err_msg);
			if (status)
				goto error;

			if (block_mask & (1 << STRUCT_BLOCK))
				continue;

			/* End of block. */
			new_structs = realloc(s->structs,
					      (s->n_structs + 1) * sizeof(struct struct_spec));
			if (!new_structs) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Memory allocation failed.";
				status = -ENOMEM;
				goto error;
			}

			s->structs = new_structs;
			memcpy(&s->structs[s->n_structs], &struct_spec, sizeof(struct struct_spec));
			s->n_structs++;
			memset(&struct_spec, 0, sizeof(struct struct_spec));

			continue;
		}

		/* action block. */
		if (block_mask & (1 << ACTION_BLOCK)) {
			struct action_spec *new_actions;

			status = action_block_parse(&action_spec,
						    &block_mask,
						    tokens,
						    n_tokens,
						    n_lines,
						    err_line,
						    err_msg);
			if (status)
				goto error;

			if (block_mask & (1 << ACTION_BLOCK))
				continue;

			/* End of block. */
			new_actions = realloc(s->actions,
					      (s->n_actions + 1) * sizeof(struct action_spec));
			if (!new_actions) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Memory allocation failed.";
				status = -ENOMEM;
				goto error;
			}

			s->actions = new_actions;
			memcpy(&s->actions[s->n_actions], &action_spec, sizeof(struct action_spec));
			s->n_actions++;
			memset(&action_spec, 0, sizeof(struct action_spec));

			continue;
		}

		/* table block. */
		if (block_mask & (1 << TABLE_BLOCK)) {
			struct table_spec *new_tables;

			status = table_block_parse(&table_spec,
						   &block_mask,
						   tokens,
						   n_tokens,
						   n_lines,
						   err_line,
						   err_msg);
			if (status)
				goto error;

			if (block_mask & (1 << TABLE_BLOCK))
				continue;

			/* End of block. */
			new_tables = realloc(s->tables,
					     (s->n_tables + 1) * sizeof(struct table_spec));
			if (!new_tables) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Memory allocation failed.";
				status = -ENOMEM;
				goto error;
			}

			s->tables = new_tables;
			memcpy(&s->tables[s->n_tables], &table_spec, sizeof(struct table_spec));
			s->n_tables++;
			memset(&table_spec, 0, sizeof(struct table_spec));

			continue;
		}

		/* selector block. */
		if (block_mask & (1 << SELECTOR_BLOCK)) {
			struct selector_spec *new_selectors;

			status = selector_block_parse(&selector_spec,
						      &block_mask,
						      tokens,
						      n_tokens,
						      n_lines,
						      err_line,
						      err_msg);
			if (status)
				goto error;

			if (block_mask & (1 << SELECTOR_BLOCK))
				continue;

			/* End of block. */
			new_selectors = realloc(s->selectors,
				(s->n_selectors + 1) * sizeof(struct selector_spec));
			if (!new_selectors) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Memory allocation failed.";
				status = -ENOMEM;
				goto error;
			}

			s->selectors = new_selectors;
			memcpy(&s->selectors[s->n_selectors],
			       &selector_spec,
			       sizeof(struct selector_spec));
			s->n_selectors++;
			memset(&selector_spec, 0, sizeof(struct selector_spec));

			continue;
		}

		/* learner block. */
		if (block_mask & (1 << LEARNER_BLOCK)) {
			struct learner_spec *new_learners;

			status = learner_block_parse(&learner_spec,
						     &block_mask,
						     tokens,
						     n_tokens,
						     n_lines,
						     err_line,
						     err_msg);
			if (status)
				goto error;

			if (block_mask & (1 << LEARNER_BLOCK))
				continue;

			/* End of block. */
			new_learners = realloc(s->learners,
					       (s->n_learners + 1) * sizeof(struct learner_spec));
			if (!new_learners) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Memory allocation failed.";
				status = -ENOMEM;
				goto error;
			}

			s->learners = new_learners;
			memcpy(&s->learners[s->n_learners],
			       &learner_spec,
			       sizeof(struct learner_spec));
			s->n_learners++;
			memset(&learner_spec, 0, sizeof(struct learner_spec));

			continue;
		}

		/* apply block. */
		if (block_mask & (1 << APPLY_BLOCK)) {
			struct apply_spec *new_apply;

			status = apply_block_parse(&apply_spec,
						   &block_mask,
						   tokens,
						   n_tokens,
						   n_lines,
						   err_line,
						   err_msg);
			if (status)
				goto error;

			if (block_mask & (1 << APPLY_BLOCK))
				continue;

			/* End of block. */
			new_apply = realloc(s->apply, (s->n_apply + 1) * sizeof(struct apply_spec));
			if (!new_apply) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Memory allocation failed.";
				status = -ENOMEM;
				goto error;
			}

			s->apply = new_apply;
			memcpy(&s->apply[s->n_apply], &apply_spec, sizeof(struct apply_spec));
			s->n_apply++;
			memset(&apply_spec, 0, sizeof(struct apply_spec));

			continue;
		}

		/* extobj. */
		if (!strcmp(tokens[0], "extobj")) {
			struct extobj_spec *new_extobjs;

			status = extobj_statement_parse(&extobj_spec,
							tokens,
							n_tokens,
							n_lines,
							err_line,
							err_msg);
			if (status)
				goto error;

			new_extobjs = realloc(s->extobjs,
					      (s->n_extobjs + 1) * sizeof(struct extobj_spec));
			if (!new_extobjs) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Memory allocation failed.";
				status = -ENOMEM;
				goto error;
			}

			s->extobjs = new_extobjs;
			memcpy(&s->extobjs[s->n_extobjs], &extobj_spec, sizeof(struct extobj_spec));
			s->n_extobjs++;
			memset(&extobj_spec, 0, sizeof(struct extobj_spec));

			continue;
		}

		/* struct. */
		if (!strcmp(tokens[0], "struct")) {
			status = struct_statement_parse(&struct_spec,
							&block_mask,
							tokens,
							n_tokens,
							n_lines,
							err_line,
							err_msg);
			if (status)
				goto error;

			continue;
		}

		/* header. */
		if (!strcmp(tokens[0], "header")) {
			struct header_spec *new_headers;

			status = header_statement_parse(&header_spec,
							tokens,
							n_tokens,
							n_lines,
							err_line,
							err_msg);
			if (status)
				goto error;

			new_headers = realloc(s->headers,
					      (s->n_headers + 1) * sizeof(struct header_spec));
			if (!new_headers) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Memory allocation failed.";
				status = -ENOMEM;
				goto error;
			}

			s->headers = new_headers;
			memcpy(&s->headers[s->n_headers], &header_spec, sizeof(struct header_spec));
			s->n_headers++;
			memset(&header_spec, 0, sizeof(struct header_spec));

			continue;
		}

		/* metadata. */
		if (!strcmp(tokens[0], "metadata")) {
			struct metadata_spec *new_metadata;

			status = metadata_statement_parse(&metadata_spec,
							  tokens,
							  n_tokens,
							  n_lines,
							  err_line,
							  err_msg);
			if (status)
				goto error;

			new_metadata = realloc(s->metadata,
					       (s->n_metadata + 1) * sizeof(struct metadata_spec));
			if (!new_metadata) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Memory allocation failed.";
				status = -ENOMEM;
				goto error;
			}

			s->metadata = new_metadata;
			memcpy(&s->metadata[s->n_metadata],
			       &metadata_spec,
			       sizeof(struct metadata_spec));
			s->n_metadata++;
			memset(&metadata_spec, 0, sizeof(struct metadata_spec));

			continue;
		}

		/* action. */
		if (!strcmp(tokens[0], "action")) {
			status = action_statement_parse(&action_spec,
							&block_mask,
							tokens,
							n_tokens,
							n_lines,
							err_line,
							err_msg);
			if (status)
				goto error;

			continue;
		}

		/* table. */
		if (!strcmp(tokens[0], "table")) {
			status = table_statement_parse(&table_spec,
						       &block_mask,
						       tokens,
						       n_tokens,
						       n_lines,
						       err_line,
						       err_msg);
			if (status)
				goto error;

			continue;
		}

		/* selector. */
		if (!strcmp(tokens[0], "selector")) {
			status = selector_statement_parse(&selector_spec,
							  &block_mask,
							  tokens,
							  n_tokens,
							  n_lines,
							  err_line,
							  err_msg);
			if (status)
				goto error;

			continue;
		}

		/* learner. */
		if (!strcmp(tokens[0], "learner")) {
			status = learner_statement_parse(&learner_spec,
							 &block_mask,
							 tokens,
							 n_tokens,
							 n_lines,
							 err_line,
							 err_msg);
			if (status)
				goto error;

			continue;
		}

		/* regarray. */
		if (!strcmp(tokens[0], "regarray")) {
			struct regarray_spec *new_regarrays;

			status = regarray_statement_parse(&regarray_spec,
							  tokens,
							  n_tokens,
							  n_lines,
							  err_line,
							  err_msg);
			if (status)
				goto error;

			new_regarrays = realloc(s->regarrays,
				(s->n_regarrays + 1) * sizeof(struct regarray_spec));
			if (!new_regarrays) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Memory allocation failed.";
				status = -ENOMEM;
				goto error;
			}

			s->regarrays = new_regarrays;
			memcpy(&s->regarrays[s->n_regarrays],
			       &regarray_spec,
			       sizeof(struct regarray_spec));
			s->n_regarrays++;
			memset(&regarray_spec, 0, sizeof(struct regarray_spec));

			continue;
		}

		/* metarray. */
		if (!strcmp(tokens[0], "metarray")) {
			struct metarray_spec *new_metarrays;

			status = metarray_statement_parse(&metarray_spec,
							  tokens,
							  n_tokens,
							  n_lines,
							  err_line,
							  err_msg);
			if (status)
				goto error;

			new_metarrays = realloc(s->metarrays,
				(s->n_metarrays + 1) * sizeof(struct metarray_spec));
			if (!new_metarrays) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Memory allocation failed.";
				status = -ENOMEM;
				goto error;
			}

			s->metarrays = new_metarrays;
			memcpy(&s->metarrays[s->n_metarrays],
			       &metarray_spec,
			       sizeof(struct metarray_spec));
			s->n_metarrays++;
			memset(&metarray_spec, 0, sizeof(struct metarray_spec));

			continue;
		}

		/* rss object configuration */
		if (!strcmp(tokens[0], "rss")) {
			struct rss_spec *new_rss;

			status = rss_statement_parse(&rss_spec,
						     tokens,
						     n_tokens,
						     n_lines,
						     err_line,
						     err_msg);
			if (status)
				goto error;

			new_rss = realloc(s->rss,
				(s->n_rss + 1) * sizeof(struct rss_spec));
			if (!new_rss) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Memory allocation failed.";
				status = -ENOMEM;
				goto error;
			}

			s->rss = new_rss;
			memcpy(&s->rss[s->n_rss],
			       &rss_spec,
			       sizeof(struct rss_spec));
			s->n_rss++;
			memset(&rss_spec, 0, sizeof(struct rss_spec));

			continue;
		}

		/* apply. */
		if (!strcmp(tokens[0], "apply")) {
			status = apply_statement_parse(&block_mask,
						       tokens,
						       n_tokens,
						       n_lines,
						       err_line,
						       err_msg);
			if (status)
				goto error;

			continue;
		}

		/* Anything else. */
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Unknown statement.";
		status = -EINVAL;
		goto error;
	}

	/* Handle unfinished block. */
	if (block_mask) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Missing }.";
		status = -EINVAL;
		goto error;
	}

	return s;

error:
	extobj_spec_free(&extobj_spec);
	struct_spec_free(&struct_spec);
	header_spec_free(&header_spec);
	metadata_spec_free(&metadata_spec);
	action_spec_free(&action_spec);
	table_spec_free(&table_spec);
	selector_spec_free(&selector_spec);
	learner_spec_free(&learner_spec);
	regarray_spec_free(&regarray_spec);
	metarray_spec_free(&metarray_spec);
	rss_spec_free(&rss_spec);
	apply_spec_free(&apply_spec);
	pipeline_spec_free(s);

	return NULL;
}

int
pipeline_spec_configure(struct rte_swx_pipeline *p,
			struct pipeline_spec *s,
			const char **err_msg)
{
	uint32_t i;
	int status = 0;

	/* extobj. */
	for (i = 0; i < s->n_extobjs; i++) {
		struct extobj_spec *extobj_spec = &s->extobjs[i];

		status = rte_swx_pipeline_extern_object_config(p,
			extobj_spec->name,
			extobj_spec->extern_type_name,
			extobj_spec->pragma);
		if (status) {
			if (err_msg)
				*err_msg = "Extern object configuration error.";
			return status;
		}
	}

	/* regarray. */
	for (i = 0; i < s->n_regarrays; i++) {
		struct regarray_spec *regarray_spec = &s->regarrays[i];

		status = rte_swx_pipeline_regarray_config(p,
			regarray_spec->name,
			regarray_spec->size,
			regarray_spec->init_val);
		if (status) {
			if (err_msg)
				*err_msg = "Register array configuration error.";
			return status;
		}
	}

	/* rss. */
	for (i = 0; i < s->n_rss; i++) {
		struct rss_spec *rss_spec = &s->rss[i];

		status = rte_swx_pipeline_rss_config(p, rss_spec->name);
		if (status) {
			if (err_msg)
				*err_msg = "rss object configuration error.";
			return status;
		}
	}

	/* metarray. */
	for (i = 0; i < s->n_metarrays; i++) {
		struct metarray_spec *metarray_spec = &s->metarrays[i];

		status = rte_swx_pipeline_metarray_config(p,
			metarray_spec->name,
			metarray_spec->size);
		if (status) {
			if (err_msg)
				*err_msg = "Meter array configuration error.";
			return status;
		}
	}

	/* struct. */
	for (i = 0; i < s->n_structs; i++) {
		struct struct_spec *struct_spec = &s->structs[i];

		status = rte_swx_pipeline_struct_type_register(p,
			struct_spec->name,
			struct_spec->fields,
			struct_spec->n_fields,
			struct_spec->varbit);
		if (status) {
			if (err_msg)
				*err_msg = "Struct type registration error.";
			return status;
		}
	}

	/* header. */
	for (i = 0; i < s->n_headers; i++) {
		struct header_spec *header_spec = &s->headers[i];

		status = rte_swx_pipeline_packet_header_register(p,
			header_spec->name,
			header_spec->struct_type_name);
		if (status) {
			if (err_msg)
				*err_msg = "Header configuration error.";
			return status;
		}
	}

	/* metadata. */
	for (i = 0; i < s->n_metadata; i++) {
		struct metadata_spec *metadata_spec = &s->metadata[i];

		status = rte_swx_pipeline_packet_metadata_register(p,
			metadata_spec->struct_type_name);
		if (status) {
			if (err_msg)
				*err_msg = "Meta-data registration error.";
			return status;
		}
	}

	/* action. */
	for (i = 0; i < s->n_actions; i++) {
		struct action_spec *action_spec = &s->actions[i];

		status = rte_swx_pipeline_action_config(p,
			action_spec->name,
			action_spec->args_struct_type_name,
			action_spec->instructions,
			action_spec->n_instructions);
		if (status) {
			if (err_msg)
				*err_msg = "Action configuration error.";
			return status;
		}
	}

	/* table. */
	for (i = 0; i < s->n_tables; i++) {
		struct table_spec *table_spec = &s->tables[i];

		status = rte_swx_pipeline_table_config(p,
			table_spec->name,
			&table_spec->params,
			table_spec->recommended_table_type_name,
			table_spec->args,
			table_spec->size);
		if (status) {
			if (err_msg)
				*err_msg = "Table configuration error.";
			return status;
		}
	}

	/* selector. */
	for (i = 0; i < s->n_selectors; i++) {
		struct selector_spec *selector_spec = &s->selectors[i];

		status = rte_swx_pipeline_selector_config(p,
			selector_spec->name,
			&selector_spec->params);
		if (status) {
			if (err_msg)
				*err_msg = "Selector table configuration error.";
			return status;
		}
	}

	/* learner. */
	for (i = 0; i < s->n_learners; i++) {
		struct learner_spec *learner_spec = &s->learners[i];

		status = rte_swx_pipeline_learner_config(p,
			learner_spec->name,
			&learner_spec->params,
			learner_spec->size,
			learner_spec->timeout,
			learner_spec->n_timeouts);
		if (status) {
			if (err_msg)
				*err_msg = "Learner table configuration error.";
			return status;
		}
	}

	/* apply. */
	for (i = 0; i < s->n_apply; i++) {
		struct apply_spec *apply_spec = &s->apply[i];

		status = rte_swx_pipeline_instructions_config(p,
			apply_spec->instructions,
			apply_spec->n_instructions);
		if (status) {
			if (err_msg)
				*err_msg = "Pipeline instructions configuration error.";
			return status;
		}
	}

	return 0;
}

static void
port_in_params_free(void *params, const char *port_type)
{
	uintptr_t dev_name;

	if (!params || !port_type)
		return;

	if (!strcmp(port_type, "ethdev")) {
		struct rte_swx_port_ethdev_reader_params *p = params;

		dev_name = (uintptr_t)p->dev_name;
	} else if (!strcmp(port_type, "ring")) {
		struct rte_swx_port_ring_reader_params *p = params;

		dev_name = (uintptr_t)p->name;
	} else if (!strcmp(port_type, "source")) {
		struct rte_swx_port_source_params *p = params;

		dev_name = (uintptr_t)p->file_name;
	} else
		dev_name = (uintptr_t)NULL;

	free((void *)dev_name);
	free(params);
}

static void
port_out_params_free(void *params, const char *port_type)
{
	uintptr_t dev_name;

	if (!params || !port_type)
		return;

	if (!strcmp(port_type, "ethdev")) {
		struct rte_swx_port_ethdev_writer_params *p = params;

		dev_name = (uintptr_t)p->dev_name;
	} else if (!strcmp(port_type, "ring")) {
		struct rte_swx_port_ring_writer_params *p = params;

		dev_name = (uintptr_t)p->name;
	} else if (!strcmp(port_type, "sink")) {
		struct rte_swx_port_sink_params *p = params;

		dev_name = (uintptr_t)p->file_name;
	} else
		dev_name = (uintptr_t)NULL;

	free((void *)dev_name);
	free(params);
}

void
pipeline_iospec_free(struct pipeline_iospec *s)
{
	uint32_t i;

	if (!s)
		return;

	/* Input ports. */
	for (i = 0; i < s->n_ports_in; i++) {
		uintptr_t name = (uintptr_t)s->port_in_type[i];

		port_in_params_free(s->port_in_params[i], s->port_in_type[i]);
		free((void *)name);
	}

	free(s->port_in_type);
	free(s->port_in_params);

	/* Output ports. */
	for (i = 0; i < s->n_ports_out; i++) {
		uintptr_t name = (uintptr_t)s->port_out_type[i];

		port_out_params_free(s->port_out_params[i], s->port_out_type[i]);
		free((void *)name);
	}

	free(s->port_out_type);
	free(s->port_out_params);

	free(s);
}

static int
mirroring_parse(struct rte_swx_pipeline_mirroring_params *p,
		char **tokens,
		uint32_t n_tokens,
		const char **err_msg)
{
	char *token;

	if ((n_tokens != 4) || strcmp(tokens[0], "slots") || strcmp(tokens[2], "sessions")) {
		if (err_msg)
			*err_msg = "Invalid statement.";
		return -EINVAL;
	}

	/* <n_slots>. */
	token = tokens[1];
	p->n_slots = strtoul(token, &token, 0);
	if (token[0]) {
		if (err_msg)
			*err_msg = "Invalid <n_slots> parameter.";
		return -EINVAL;
	}

	/* <n_sessions>. */
	token = tokens[3];
	p->n_sessions = strtoul(token, &token, 0);
	if (token[0]) {
		if (err_msg)
			*err_msg = "Invalid <n_sessions> parameter.";
		return -EINVAL;
	}

	return 0;
}

static void *
port_in_ethdev_parse(char **tokens, uint32_t n_tokens, const char **err_msg)
{
	struct rte_swx_port_ethdev_reader_params *p = NULL;
	char *token, *dev_name = NULL;
	uint32_t queue_id, burst_size;

	if ((n_tokens != 5) || strcmp(tokens[1], "rxq") || strcmp(tokens[3], "bsz")) {
		if (err_msg)
			*err_msg = "Invalid statement.";
		return NULL;
	}

	/* <queue_id>. */
	token = tokens[2];
	queue_id = strtoul(token, &token, 0);
	if (token[0]) {
		if (err_msg)
			*err_msg = "Invalid <queue_id> parameter.";
		return NULL;
	}

	/* <burst_size>. */
	token = tokens[4];
	burst_size = strtoul(token, &token, 0);
	if (token[0]) {
		if (err_msg)
			*err_msg = "Invalid <burst_size> parameter.";
		return NULL;
	}

	/* Memory allocation. */
	dev_name = strdup(tokens[0]);
	p = malloc(sizeof(struct rte_swx_port_ethdev_reader_params));
	if (!dev_name || !p) {
		free(dev_name);
		free(p);

		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return NULL;
	}

	/* Initialization. */
	p->dev_name = dev_name;
	p->queue_id = queue_id;
	p->burst_size = burst_size;

	return p;
}

static void *
port_in_ring_parse(char **tokens, uint32_t n_tokens, const char **err_msg)
{
	struct rte_swx_port_ring_reader_params *p = NULL;
	char *token, *name = NULL;
	uint32_t burst_size;

	if ((n_tokens != 3) || strcmp(tokens[1], "bsz")) {
		if (err_msg)
			*err_msg = "Invalid statement.";
		return NULL;
	}

	/* <burst_size>. */
	token = tokens[2];
	burst_size = strtoul(token, &token, 0);
	if (token[0]) {
		if (err_msg)
			*err_msg = "Invalid <burst_size> parameter.";
		return NULL;
	}

	/* Memory allocation. */
	name = strdup(tokens[0]);
	p = malloc(sizeof(struct rte_swx_port_ring_reader_params));
	if (!name || !p) {
		free(name);
		free(p);

		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return NULL;
	}

	/* Initialization. */
	p->name = name;
	p->burst_size = burst_size;

	return p;
}

static void *
port_in_source_parse(char **tokens, uint32_t n_tokens, const char **err_msg)
{
	struct rte_swx_port_source_params *p = NULL;
	struct rte_mempool *pool = NULL;
	char *token, *file_name = NULL;
	uint32_t n_loops, n_pkts_max;

	if ((n_tokens != 8) ||
	    strcmp(tokens[0], "mempool") ||
	    strcmp(tokens[2], "file") ||
	    strcmp(tokens[4], "loop") ||
	    strcmp(tokens[6], "packets")) {
		if (err_msg)
			*err_msg = "Invalid statement.";
		return NULL;
	}

	/* <mempool_name>. */
	pool = rte_mempool_lookup(tokens[1]);
	if (!pool) {
		if (err_msg)
			*err_msg = "Invalid <mempool_name> parameter.";
		return NULL;
	}

	/* <n_loops>. */
	token = tokens[5];
	n_loops = strtoul(token, &token, 0);
	if (token[0]) {
		if (err_msg)
			*err_msg = "Invalid <n_loops> parameter.";
		return NULL;
	}

	/* <n_pkts_max>. */
	token = tokens[7];
	n_pkts_max = strtoul(token, &token, 0);
	if (token[0]) {
		if (err_msg)
			*err_msg = "Invalid <n_pkts_max> parameter.";
		return NULL;
	}

	/* Memory allocation. */
	file_name = strdup(tokens[3]);
	p = malloc(sizeof(struct rte_swx_port_source_params));
	if (!file_name || !p) {
		free(file_name);
		free(p);

		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return NULL;
	}

	/* Initialization. */
	p->pool = pool;
	p->file_name = file_name;
	p->n_loops = n_loops;
	p->n_pkts_max = n_pkts_max;

	return p;
}

static void *
port_in_fd_parse(char **tokens,
		 uint32_t n_tokens,
		 const char **err_msg)
{
	struct rte_swx_port_fd_reader_params *p = NULL;
	struct rte_mempool *mempool = NULL;
	char *token;
	uint32_t mtu, burst_size;
	int fd;

	if ((n_tokens != 7) ||
	    strcmp(tokens[1], "mtu") ||
	    strcmp(tokens[3], "mempool") ||
	    strcmp(tokens[5], "bsz")) {
		if (err_msg)
			*err_msg = "Invalid statement.";
		return NULL;
	}

	/* <file_descriptor>. */
	token = tokens[0];
	fd = strtol(token, &token, 0);
	if (token[0]) {
		if (err_msg)
			*err_msg = "Invalid <file_descriptor> parameter.";
		return NULL;
	}

	/* <mtu>. */
	token = tokens[2];
	mtu = strtoul(token, &token, 0);
	if (token[0]) {
		if (err_msg)
			*err_msg = "Invalid <mtu> parameter.";
		return NULL;
	}

	/* <mempool_name>. */
	mempool = rte_mempool_lookup(tokens[4]);
	if (!mempool) {
		if (err_msg)
			*err_msg = "Invalid <mempool_name> parameter.";
		return NULL;
	}

	/* <burst_size>. */
	token = tokens[6];
	burst_size = strtoul(token, &token, 0);
	if (token[0]) {
		if (err_msg)
			*err_msg = "Invalid <burst_size> parameter.";
		return NULL;
	}

	/* Memory allocation. */
	p = malloc(sizeof(struct rte_swx_port_fd_reader_params));
	if (!p) {
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return NULL;
	}

	/* Initialization. */
	p->fd = fd;
	p->mtu = mtu;
	p->mempool = mempool;
	p->burst_size = burst_size;

	return p;
}

static void *
port_out_ethdev_parse(char **tokens, uint32_t n_tokens, const char **err_msg)
{
	struct rte_swx_port_ethdev_writer_params *p = NULL;
	char *token, *dev_name = NULL;
	uint32_t queue_id, burst_size;

	if ((n_tokens != 5) || strcmp(tokens[1], "txq") || strcmp(tokens[3], "bsz")) {
		if (err_msg)
			*err_msg = "Invalid statement.";
		return NULL;
	}

	/* <queue_id>. */
	token = tokens[2];
	queue_id = strtoul(token, &token, 0);
	if (token[0]) {
		if (err_msg)
			*err_msg = "Invalid <queue_id> parameter.";
		return NULL;
	}

	/* <burst_size>. */
	token = tokens[4];
	burst_size = strtoul(token, &token, 0);
	if (token[0]) {
		if (err_msg)
			*err_msg = "Invalid <burst_size> parameter.";
		return NULL;
	}

	/* Memory allocation. */
	dev_name = strdup(tokens[0]);
	p = malloc(sizeof(struct rte_swx_port_ethdev_writer_params));
	if (!dev_name || !p) {
		free(dev_name);
		free(p);

		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return NULL;
	}

	/* Initialization. */
	p->dev_name = dev_name;
	p->queue_id = queue_id;
	p->burst_size = burst_size;

	return p;
}

static void *
port_out_ring_parse(char **tokens, uint32_t n_tokens, const char **err_msg)
{
	struct rte_swx_port_ring_writer_params *p = NULL;
	char *token, *name = NULL;
	uint32_t burst_size;

	if ((n_tokens != 3) || strcmp(tokens[1], "bsz")) {
		if (err_msg)
			*err_msg = "Invalid statement.";
		return NULL;
	}

	/* <burst_size>. */
	token = tokens[2];
	burst_size = strtoul(token, &token, 0);
	if (token[0]) {
		if (err_msg)
			*err_msg = "Invalid <burst_size> parameter.";
		return NULL;
	}

	/* Memory allocation. */
	name = strdup(tokens[0]);
	p = malloc(sizeof(struct rte_swx_port_ring_writer_params));
	if (!name || !p) {
		free(name);
		free(p);

		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return NULL;
	}

	/* Initialization. */
	p->name = name;
	p->burst_size = burst_size;

	return p;
}

static void *
port_out_sink_parse(char **tokens, uint32_t n_tokens, const char **err_msg)
{
	struct rte_swx_port_sink_params *p = NULL;
	char *file_name = NULL;
	int file_name_valid = 0;

	if ((n_tokens != 2) || strcmp(tokens[0], "file")) {
		if (err_msg)
			*err_msg = "Invalid statement.";
		return NULL;
	}

	/* Memory allocation. */
	if (strcmp(tokens[1], "none")) {
		file_name_valid = 1;
		file_name = strdup(tokens[1]);
	}

	p = malloc(sizeof(struct rte_swx_port_ring_writer_params));
	if ((file_name_valid && !file_name) || !p) {
		free(file_name);
		free(p);

		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return NULL;
	}

	/* Initialization. */
	p->file_name = file_name;

	return p;
}

static void *
port_out_fd_parse(char **tokens,
		  uint32_t n_tokens,
		  const char **err_msg)
{
	struct rte_swx_port_fd_writer_params *p = NULL;
	char *token;
	uint32_t burst_size;
	int fd;

	if ((n_tokens != 3) || strcmp(tokens[1], "bsz")) {
		if (err_msg)
			*err_msg = "Invalid statement.";
		return NULL;
	}

	/* <file_descriptor>. */
	token = tokens[0];
	fd = strtol(token, &token, 0);
	if (token[0]) {
		if (err_msg)
			*err_msg = "Invalid <file_descriptor> parameter.";
		return NULL;
	}

	/* <burst_size>. */
	token = tokens[2];
	burst_size = strtoul(token, &token, 0);
	if (token[0]) {
		if (err_msg)
			*err_msg = "Invalid <burst_size> parameter.";
		return NULL;
	}

	/* Memory allocation. */
	p = malloc(sizeof(struct rte_swx_port_fd_writer_params));
	if (!p) {
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		return NULL;
	}

	/* Initialization. */
	p->fd = fd;
	p->burst_size = burst_size;

	return p;
}

struct pipeline_iospec *
pipeline_iospec_parse(FILE *spec,
		      uint32_t *err_line,
		      const char **err_msg)
{
	struct pipeline_iospec *s = NULL;
	uint32_t n_lines = 0;

	/* Check the input arguments. */
	if (!spec) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Invalid input argument.";
		goto error;
	}

	/* Memory allocation. */
	s = calloc(1, sizeof(struct pipeline_iospec));
	if (!s) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		goto error;
	}

	/* Initialize with the default values. */
	s->mirroring_params.n_slots = RTE_SWX_PACKET_MIRRORING_SLOTS_DEFAULT;
	s->mirroring_params.n_sessions = RTE_SWX_PACKET_MIRRORING_SESSIONS_DEFAULT;

	for (n_lines = 1; ; n_lines++) {
		char line[MAX_LINE_LENGTH];
		char *tokens[MAX_TOKENS], *ptr = line;
		uint32_t n_tokens = 0;

		/* Read next line. */
		if (!fgets(line, sizeof(line), spec))
			break;

		/* Parse the line into tokens. */
		for ( ; ; ) {
			char *token;

			/* Get token. */
			token = strtok_r(ptr, " \f\n\r\t\v", &ptr);
			if (!token)
				break;

			/* Handle comments. */
			if ((token[0] == '#') ||
			    (token[0] == ';') ||
			    ((token[0] == '/') && (token[1] == '/'))) {
				break;
			}

			/* Handle excessively long lines. */
			if (n_tokens >= RTE_DIM(tokens)) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Too many tokens.";
				goto error;
			}

			/* Handle excessively long tokens. */
			if (strnlen(token, RTE_SWX_NAME_SIZE) >=
			    RTE_SWX_NAME_SIZE) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Token too big.";
				goto error;
			}

			/* Save token. */
			tokens[n_tokens] = token;
			n_tokens++;
		}

		/* Handle empty lines. */
		if (!n_tokens)
			continue;

		/* mirroring. */
		if ((n_tokens >= 1) && !strcmp(tokens[0], "mirroring")) {
			int status = 0;

			status = mirroring_parse(&s->mirroring_params,
						 &tokens[1],
						 n_tokens - 1,
						 err_msg);
			if (status) {
				if (err_line)
					*err_line = n_lines;
				goto error;
			}

			continue;
		}

		/* port in. */
		if ((n_tokens >= 4) && !strcmp(tokens[0], "port") && !strcmp(tokens[1], "in")) {
			char *token = tokens[2];
			uint32_t *new_id = NULL;
			const char **new_type = NULL, *port_type = NULL;
			void **new_params = NULL, *p = NULL;
			uint32_t port_id;

			/* <port_id>. */
			port_id = strtoul(token, &token, 0);
			if (token[0]) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Invalid port ID.";
				goto error;
			}

			/* <port_type>. */
			if (!strcmp(tokens[3], "ethdev"))
				p = port_in_ethdev_parse(&tokens[4], n_tokens - 4, err_msg);
			else if (!strcmp(tokens[3], "ring"))
				p = port_in_ring_parse(&tokens[4], n_tokens - 4, err_msg);
			else if (!strcmp(tokens[3], "source"))
				p = port_in_source_parse(&tokens[4], n_tokens - 4, err_msg);
			else if (!strcmp(tokens[3], "fd"))
				p = port_in_fd_parse(&tokens[4], n_tokens - 4, err_msg);
			else {
				p = NULL;
				if (err_msg)
					*err_msg = "Invalid port type.";
			}

			if (!p) {
				if (err_line)
					*err_line = n_lines;
				goto error;
			}

			/* New port. */
			port_type = strdup(tokens[3]);
			new_id = realloc(s->port_in_id,
					 (s->n_ports_in + 1) * sizeof(uint32_t));
			new_type = realloc(s->port_in_type,
					   (s->n_ports_in + 1) * sizeof(char *));
			new_params = realloc(s->port_in_params,
					     (s->n_ports_in + 1) * sizeof(void *));
			if (!port_type || !new_id || !new_type || !new_params) {
				uintptr_t pt = (uintptr_t)port_type;

				port_in_params_free(p, tokens[3]);
				free((void *)pt);
				free(new_id);
				free(new_type);
				free(new_params);

				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Memory allocation failed.";
				goto error;
			}

			s->port_in_id = new_id;
			s->port_in_type = new_type;
			s->port_in_params = new_params;

			s->port_in_id[s->n_ports_in] = port_id;
			s->port_in_type[s->n_ports_in] = port_type;
			s->port_in_params[s->n_ports_in] = p;
			s->n_ports_in++;

			continue;
		}

		/* port out. */
		if ((n_tokens >= 4) && !strcmp(tokens[0], "port") && !strcmp(tokens[1], "out")) {
			char *token = tokens[2];
			uint32_t *new_id = NULL;
			const char **new_type = NULL, *port_type = NULL;
			void **new_params = NULL, *p = NULL;
			uint32_t port_id;

			/* <port_id>. */
			port_id = strtoul(token, &token, 0);
			if (token[0]) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Invalid port ID.";
				goto error;
			}

			/* <port_type>. */
			if (!strcmp(tokens[3], "ethdev"))
				p = port_out_ethdev_parse(&tokens[4], n_tokens - 4, err_msg);
			else if (!strcmp(tokens[3], "ring"))
				p = port_out_ring_parse(&tokens[4], n_tokens - 4, err_msg);
			else if (!strcmp(tokens[3], "sink"))
				p = port_out_sink_parse(&tokens[4], n_tokens - 4, err_msg);
			else if (!strcmp(tokens[3], "fd"))
				p = port_out_fd_parse(&tokens[4], n_tokens - 4, err_msg);
			else {
				p = NULL;
				if (err_msg)
					*err_msg = "Invalid port type.";
			}

			if (!p) {
				if (err_line)
					*err_line = n_lines;
				goto error;
			}

			/* New port. */
			port_type = strdup(tokens[3]);
			new_id = realloc(s->port_out_id,
					 (s->n_ports_out + 1) * sizeof(uint32_t));
			new_type = realloc(s->port_out_type,
					   (s->n_ports_out + 1) * sizeof(char *));
			new_params = realloc(s->port_out_params,
					     (s->n_ports_out + 1) * sizeof(void *));
			if (!port_type || !new_id || !new_type || !new_params) {
				uintptr_t pt = (uintptr_t)port_type;

				port_out_params_free(p, tokens[3]);
				free((void *)pt);
				free(new_id);
				free(new_type);
				free(new_params);

				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Memory allocation failed.";
				goto error;
			}

			s->port_out_id = new_id;
			s->port_out_type = new_type;
			s->port_out_params = new_params;

			s->port_out_id[s->n_ports_out] = port_id;
			s->port_out_type[s->n_ports_out] = port_type;
			s->port_out_params[s->n_ports_out] = p;
			s->n_ports_out++;

			continue;
		}

		/* Anything else. */
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Unknown I/O statement.";
		goto error;
	}

	return s;

error:
	pipeline_iospec_free(s);

	return NULL;
}

int
pipeline_iospec_configure(struct rte_swx_pipeline *p,
			  struct pipeline_iospec *s,
			  const char **err_msg)
{
	uint32_t i;
	int status = 0;

	/* Check input arguments. */
	if (!p || !s) {
		if (err_msg)
			*err_msg = "Invalid input argument";
		return -EINVAL;
	}

	/* Mirroring. */
	status = rte_swx_pipeline_mirroring_config(p, &s->mirroring_params);
	if (status) {
		if (err_msg)
			*err_msg = "Pipeline mirroring configuration error.";
		return status;
	}

	/* Input ports. */
	for (i = 0; i < s->n_ports_in; i++) {
		status = rte_swx_pipeline_port_in_config(p,
							 i,
							 s->port_in_type[i],
							 s->port_in_params[i]);
		if (status) {
			if (err_msg)
				*err_msg = "Pipeline input port configuration error.";
			return status;
		}
	}

	/* Output ports. */
	for (i = 0; i < s->n_ports_out; i++) {
		status = rte_swx_pipeline_port_out_config(p,
							  i,
							  s->port_out_type[i],
							  s->port_out_params[i]);
		if (status) {
			if (err_msg)
				*err_msg = "Pipeline output port configuration error.";
			return status;
		}
	}

	return 0;
}
