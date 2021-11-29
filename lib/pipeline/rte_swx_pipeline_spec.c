/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "rte_swx_pipeline.h"
#include "rte_swx_ctl.h"

#define MAX_LINE_LENGTH RTE_SWX_INSTRUCTION_SIZE
#define MAX_TOKENS RTE_SWX_INSTRUCTION_TOKENS_MAX

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
#define APPLY_BLOCK 10

/*
 * extobj.
 *
 * extobj OBJ_NAME instanceof OBJ_TYPE [ pragma OBJ_CREATE_ARGS ]
 */
struct extobj_spec {
	char *name;
	char *extern_type_name;
	char *pragma;
};

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
 *
 * struct STRUCT_TYPE_NAME {
 *	bit<SIZE> | varbit<SIZE> FIELD_NAME
 *	...
 * }
 */
struct struct_spec {
	char *name;
	struct rte_swx_field_params *fields;
	uint32_t n_fields;
	int varbit;
};

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
	    (n_bits % 8) ||
	    ((n_bits > 64) && !varbit)) {
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
 *
 * header HEADER_NAME instanceof STRUCT_TYPE_NAME
 */
struct header_spec {
	char *name;
	char *struct_type_name;
};

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
 *
 * metadata instanceof STRUCT_TYPE_NAME
 */
struct metadata_spec {
	char *struct_type_name;
};

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
 *
 * action ACTION_NAME args none | instanceof STRUCT_TYPE_NAME {
 *	INSTRUCTION
 *	...
 * }
 */
struct action_spec {
	char *name;
	char *args_struct_type_name;
	const char **instructions;
	uint32_t n_instructions;
};

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
 *
 * table {
 *	key {
 *		MATCH_FIELD_NAME exact | wildcard | lpm
 *		...
 *	}
 *	actions {
 *		ACTION_NAME [ @tableonly | @defaultonly ]
 *		...
 *	}
 *	default_action ACTION_NAME args none | ARGS_BYTE_ARRAY [ const ]
 *	instanceof TABLE_TYPE_NAME
 *	pragma ARGS
 *	size SIZE
 * }
 */
struct table_spec {
	char *name;
	struct rte_swx_pipeline_table_params params;
	char *recommended_table_type_name;
	char *args;
	uint32_t size;
};

static void
table_spec_free(struct table_spec *s)
{
	uintptr_t default_action_name;
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

	free(s->params.default_action_data);
	s->params.default_action_data = NULL;

	free(s->params.action_is_for_table_entries);
	s->params.action_is_for_table_entries = NULL;

	free(s->params.action_is_for_default_entry);
	s->params.action_is_for_default_entry = NULL;

	s->params.default_action_is_const = 0;

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

	if (!strcmp(tokens[0], "default_action")) {
		if (((n_tokens != 4) && (n_tokens != 5)) ||
		    strcmp(tokens[2], "args") ||
		    strcmp(tokens[3], "none") ||
		    ((n_tokens == 5) && strcmp(tokens[4], "const"))) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Invalid default_action statement.";
			return -EINVAL;
		}

		if (s->params.default_action_name) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Duplicate default_action stmt.";
			return -EINVAL;
		}

		s->params.default_action_name = strdup(tokens[1]);
		if (!s->params.default_action_name) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Memory allocation failed.";
			return -ENOMEM;
		}

		if (n_tokens == 5)
			s->params.default_action_is_const = 1;

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
 *
 * selector SELECTOR_NAME {
 *	group_id FIELD_NAME
 *	selector {
 *		FIELD_NAME
 *		...
 *	}
 *	member_id FIELD_NAME
 *	n_groups N_GROUPS
 *	n_members_per_group N_MEMBERS_PER_GROUP
 * }
 */
struct selector_spec {
	char *name;
	struct rte_swx_pipeline_selector_params params;
};

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
 *
 * learner {
 *	key {
 *		MATCH_FIELD_NAME
 *		...
 *	}
 *	actions {
 *		ACTION_NAME [ @tableonly | @defaultonly]
 *		...
 *	}
 *	default_action ACTION_NAME args none | ARGS_BYTE_ARRAY [ const ]
 *	size SIZE
 *	timeout TIMEOUT_IN_SECONDS
 * }
 */
struct learner_spec {
	char *name;
	struct rte_swx_pipeline_learner_params params;
	uint32_t size;
	uint32_t timeout;
};

static void
learner_spec_free(struct learner_spec *s)
{
	uintptr_t default_action_name;
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

	free(s->params.default_action_data);
	s->params.default_action_data = NULL;

	free(s->params.action_is_for_table_entries);
	s->params.action_is_for_table_entries = NULL;

	free(s->params.action_is_for_default_entry);
	s->params.action_is_for_default_entry = NULL;

	s->params.default_action_is_const = 0;

	s->size = 0;

	s->timeout = 0;
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

	if (!strcmp(tokens[0], "default_action")) {
		if (((n_tokens != 4) && (n_tokens != 5)) ||
		    strcmp(tokens[2], "args") ||
		    strcmp(tokens[3], "none") ||
		    ((n_tokens == 5) && strcmp(tokens[4], "const"))) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Invalid default_action statement.";
			return -EINVAL;
		}

		if (s->params.default_action_name) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Duplicate default_action stmt.";
			return -EINVAL;
		}

		s->params.default_action_name = strdup(tokens[1]);
		if (!s->params.default_action_name) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Memory allocation failed.";
			return -ENOMEM;
		}

		if (n_tokens == 5)
			s->params.default_action_is_const = 1;

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

	if (!strcmp(tokens[0], "timeout")) {
		char *p = tokens[1];

		if (n_tokens != 2) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Invalid timeout statement.";
			return -EINVAL;
		}

		s->timeout = strtoul(p, &p, 0);
		if (p[0]) {
			if (err_line)
				*err_line = n_lines;
			if (err_msg)
				*err_msg = "Invalid timeout argument.";
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
 * regarray.
 *
 * regarray NAME size SIZE initval INITVAL
 */
struct regarray_spec {
	char *name;
	uint64_t init_val;
	uint32_t size;
};

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
 *
 * metarray NAME size SIZE
 */
struct metarray_spec {
	char *name;
	uint32_t size;
};

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
 * apply.
 *
 * apply {
 *	INSTRUCTION
 *	...
 * }
 */
struct apply_spec {
	const char **instructions;
	uint32_t n_instructions;
};

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
int
rte_swx_pipeline_build_from_spec(struct rte_swx_pipeline *p,
				 FILE *spec,
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
	struct apply_spec apply_spec = {0};
	uint32_t n_lines;
	uint32_t block_mask = 0;
	int status;

	/* Check the input arguments. */
	if (!p) {
		if (err_line)
			*err_line = 0;
		if (err_msg)
			*err_msg = "Null pipeline argument.";
		status = -EINVAL;
		goto error;
	}

	if (!spec) {
		if (err_line)
			*err_line = 0;
		if (err_msg)
			*err_msg = "Null specification file argument.";
		status = -EINVAL;
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
			if (n_tokens >= MAX_TOKENS) {
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
			status = rte_swx_pipeline_struct_type_register(p,
				struct_spec.name,
				struct_spec.fields,
				struct_spec.n_fields,
				struct_spec.varbit);
			if (status) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Struct registration error.";
				goto error;
			}

			struct_spec_free(&struct_spec);

			continue;
		}

		/* action block. */
		if (block_mask & (1 << ACTION_BLOCK)) {
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
			status = rte_swx_pipeline_action_config(p,
				action_spec.name,
				action_spec.args_struct_type_name,
				action_spec.instructions,
				action_spec.n_instructions);
			if (status) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Action config error.";
				goto error;
			}

			action_spec_free(&action_spec);

			continue;
		}

		/* table block. */
		if (block_mask & (1 << TABLE_BLOCK)) {
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
			status = rte_swx_pipeline_table_config(p,
				table_spec.name,
				&table_spec.params,
				table_spec.recommended_table_type_name,
				table_spec.args,
				table_spec.size);
			if (status) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Table configuration error.";
				goto error;
			}

			table_spec_free(&table_spec);

			continue;
		}

		/* selector block. */
		if (block_mask & (1 << SELECTOR_BLOCK)) {
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
			status = rte_swx_pipeline_selector_config(p,
				selector_spec.name,
				&selector_spec.params);
			if (status) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Selector configuration error.";
				goto error;
			}

			selector_spec_free(&selector_spec);

			continue;
		}

		/* learner block. */
		if (block_mask & (1 << LEARNER_BLOCK)) {
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
			status = rte_swx_pipeline_learner_config(p,
				learner_spec.name,
				&learner_spec.params,
				learner_spec.size,
				learner_spec.timeout);
			if (status) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Learner table configuration error.";
				goto error;
			}

			learner_spec_free(&learner_spec);

			continue;
		}

		/* apply block. */
		if (block_mask & (1 << APPLY_BLOCK)) {
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
			status = rte_swx_pipeline_instructions_config(p,
				apply_spec.instructions,
				apply_spec.n_instructions);
			if (status) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Pipeline instructions err.";
				goto error;
			}

			apply_spec_free(&apply_spec);

			continue;
		}

		/* extobj. */
		if (!strcmp(tokens[0], "extobj")) {
			status = extobj_statement_parse(&extobj_spec,
							tokens,
							n_tokens,
							n_lines,
							err_line,
							err_msg);
			if (status)
				goto error;

			status = rte_swx_pipeline_extern_object_config(p,
				extobj_spec.name,
				extobj_spec.extern_type_name,
				extobj_spec.pragma);
			if (status) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Extern object config err.";
				goto error;
			}

			extobj_spec_free(&extobj_spec);

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
			status = header_statement_parse(&header_spec,
							tokens,
							n_tokens,
							n_lines,
							err_line,
							err_msg);
			if (status)
				goto error;

			status = rte_swx_pipeline_packet_header_register(p,
				header_spec.name,
				header_spec.struct_type_name);
			if (status) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Header registration error.";
				goto error;
			}

			header_spec_free(&header_spec);

			continue;
		}

		/* metadata. */
		if (!strcmp(tokens[0], "metadata")) {
			status = metadata_statement_parse(&metadata_spec,
							  tokens,
							  n_tokens,
							  n_lines,
							  err_line,
							  err_msg);
			if (status)
				goto error;

			status = rte_swx_pipeline_packet_metadata_register(p,
				metadata_spec.struct_type_name);
			if (status) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Meta-data reg err.";
				goto error;
			}

			metadata_spec_free(&metadata_spec);

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
			status = regarray_statement_parse(&regarray_spec,
							  tokens,
							  n_tokens,
							  n_lines,
							  err_line,
							  err_msg);
			if (status)
				goto error;

			status = rte_swx_pipeline_regarray_config(p,
				regarray_spec.name,
				regarray_spec.size,
				regarray_spec.init_val);
			if (status) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Register array configuration error.";
				goto error;
			}

			regarray_spec_free(&regarray_spec);

			continue;
		}

		/* metarray. */
		if (!strcmp(tokens[0], "metarray")) {
			status = metarray_statement_parse(&metarray_spec,
							  tokens,
							  n_tokens,
							  n_lines,
							  err_line,
							  err_msg);
			if (status)
				goto error;

			status = rte_swx_pipeline_metarray_config(p,
				metarray_spec.name,
				metarray_spec.size);
			if (status) {
				if (err_line)
					*err_line = n_lines;
				if (err_msg)
					*err_msg = "Meter array configuration error.";
				goto error;
			}

			metarray_spec_free(&metarray_spec);

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

	/* Pipeline build. */
	status = rte_swx_pipeline_build(p);
	if (status) {
		if (err_line)
			*err_line = n_lines;
		if (err_msg)
			*err_msg = "Pipeline build error.";
		goto error;
	}

	return 0;

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
	apply_spec_free(&apply_spec);
	return status;
}
