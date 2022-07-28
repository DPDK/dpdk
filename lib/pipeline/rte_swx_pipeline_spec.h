/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */
#include <stdint.h>
#include <stdio.h>

#include <rte_common.h>

#include <rte_swx_pipeline.h>

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

/*
 * header.
 *
 * header HEADER_NAME instanceof STRUCT_TYPE_NAME
 */
struct header_spec {
	char *name;
	char *struct_type_name;
};

/*
 * metadata.
 *
 * metadata instanceof STRUCT_TYPE_NAME
 */
struct metadata_spec {
	char *struct_type_name;
};

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

/*
 * table.
 *
 * table TABLE_NAME {
 *	key {
 *		MATCH_FIELD_NAME exact | wildcard | lpm
 *		...
 *	}
 *	actions {
 *		ACTION_NAME [ @tableonly | @defaultonly ]
 *		...
 *	}
 *	default_action ACTION_NAME args none | ARG0_NAME ARG0_VALUE ... [ const ]
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

/*
 * learner.
 *
 * learner LEARNER_NAME {
 *	key {
 *		MATCH_FIELD_NAME
 *		...
 *	}
 *	actions {
 *		ACTION_NAME [ @tableonly | @defaultonly]
 *		...
 *	}
 *	default_action ACTION_NAME args none | ARG0_NAME ARG0_VALUE ... [ const ]
 *	size SIZE
 *	timeout {
 *		TIMEOUT_IN_SECONDS
 *		...
 *	}
 * }
 */
struct learner_spec {
	char *name;
	struct rte_swx_pipeline_learner_params params;
	uint32_t size;
	uint32_t *timeout;
	uint32_t n_timeouts;
};

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

/*
 * metarray.
 *
 * metarray NAME size SIZE
 */
struct metarray_spec {
	char *name;
	uint32_t size;
};

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
