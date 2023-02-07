/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */
#ifndef __INCLUDE_RTE_SWX_PIPELINE_SPEC_H__
#define __INCLUDE_RTE_SWX_PIPELINE_SPEC_H__

#ifdef __cplusplus
extern "C" {
#endif

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
 *	hash HASH_FUNCTION_NAME
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
 *	hash HASH_FUNCTION_NAME
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
 * rss.
 *
 * rss NAME
 */
struct rss_spec {
	char *name;
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

/*
 * Pipeline.
 */
struct pipeline_spec {
	struct extobj_spec *extobjs;
	struct struct_spec *structs;
	struct header_spec *headers;
	struct metadata_spec *metadata;
	struct action_spec *actions;
	struct table_spec *tables;
	struct selector_spec *selectors;
	struct learner_spec *learners;
	struct regarray_spec *regarrays;
	struct metarray_spec *metarrays;
	struct rss_spec *rss;
	struct apply_spec *apply;

	uint32_t n_extobjs;
	uint32_t n_structs;
	uint32_t n_headers;
	uint32_t n_metadata;
	uint32_t n_actions;
	uint32_t n_tables;
	uint32_t n_selectors;
	uint32_t n_learners;
	uint32_t n_regarrays;
	uint32_t n_metarrays;
	uint32_t n_rss;
	uint32_t n_apply;
};

/*
 * Mirroring:
 *      mirroring slots <n_slots> sessions <n_sessions>
 *
 * Input ports:
 *      port in <port_id> ethdev <ethdev_name> rxq <queue_id> bsz <burst_size>
 *      port in <port_id> ring <ring_name> bsz <burst_size>
 *      port in <port_id> source mempool <mempool_name> file <file_name> loop <n_loops>
 *                               packets <n_pkts_max>
 *      port in <port_id> fd <file_descriptor> mtu <mtu> mempool <mempool_name> bsz <burst_size>
 *
 * Output ports:
 *      port out <port_id> ethdev <ethdev_name> txq <queue_id> bsz <burst_size>
 *      port out <port_id> ring <ring_name> bsz <burst_size>
 *      port out <port_id> sink file <file_name> | none
 *      port out <port_id> fd <file_descriptor> bsz <burst_size>
 */
struct pipeline_iospec {
	struct rte_swx_pipeline_mirroring_params mirroring_params;

	uint32_t *port_in_id;
	const char **port_in_type;
	void **port_in_params;

	uint32_t *port_out_id;
	const char **port_out_type;
	void **port_out_params;

	uint32_t n_ports_in;
	uint32_t n_ports_out;
};

void
pipeline_spec_free(struct pipeline_spec *s);

void
pipeline_spec_codegen(FILE *f,
		      struct pipeline_spec *s);

struct pipeline_spec *
pipeline_spec_parse(FILE *spec,
		    uint32_t *err_line,
		    const char **err_msg);

int
pipeline_spec_configure(struct rte_swx_pipeline *p,
			struct pipeline_spec *s,
			const char **err_msg);

void
pipeline_iospec_free(struct pipeline_iospec *s);

struct pipeline_iospec *
pipeline_iospec_parse(FILE *spec,
		      uint32_t *err_line,
		      const char **err_msg);

int
pipeline_iospec_configure(struct rte_swx_pipeline *p,
			  struct pipeline_iospec *s,
			  const char **err_msg);

#ifdef __cplusplus
}
#endif

#endif
