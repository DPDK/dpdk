/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */
#include <jansson.h>
#include <rte_flow.h>

#include "cpfl_ethdev.h"

#ifndef _CPFL_FLOW_PARSER_H_
#define _CPFL_FLOW_PARSER_H_

#define CPFL_JS_STR_SIZE 100
#define CPFL_JS_SEM_FV_KEY_NUM_MAX 64
#define CPFL_JS_PROTO_NUM_MAX 16
#define CPFL_JS_MOD_KEY_NUM_MAX 8
#define CPFL_JS_PROG_CONTENT_FIELD_NUM_MAX 64
#define CPFL_JS_PROG_CONSTANT_VALUE_NUM_MAX 8
#define CPFL_JS_PROG_PARAM_NUM_MAX 10

/* Pattern Rules Storage */
enum cpfl_flow_pr_action_type {
	CPFL_JS_PR_ACTION_TYPE_SEM,
	CPFL_JS_PR_ACTION_TYPE_UNKNOWN = -1,
};

/* This structure matches a sequence of fields in struct rte_flow_attr */
struct cpfl_flow_js_pr_key_attr {
	uint16_t ingress;
	uint16_t egress;
};

struct cpfl_flow_js_pr_key_proto_field {
	char name[CPFL_JS_STR_SIZE];
	union {
		char mask[CPFL_JS_STR_SIZE];
		uint32_t mask_32b;
	};
};

/* This structure matches a sequence of "struct rte_flow_item" */
struct cpfl_flow_js_pr_key_proto {
	enum rte_flow_item_type type;
	struct cpfl_flow_js_pr_key_proto_field *fields;
	int fields_size;
};

enum cpfl_flow_js_fv_type {
	CPFL_FV_TYPE_PROTOCOL,
	CPFL_FV_TYPE_IMMEDIATE,
	CPFL_FV_TYPE_METADATA,
	CPFL_FV_TYPE_UNKNOWN = -1,
};

struct cpfl_flow_js_fv {
	uint16_t offset;
	enum cpfl_flow_js_fv_type type;
	union {
		/*  a 16 bits value */
		uint16_t immediate;
		/* a reference to a protocol header with a <header, layer, offset, mask> tuple */
		struct {
			enum rte_flow_item_type header;
			uint16_t layer;
			uint16_t offset;
			uint16_t mask;
		} proto;
		/* a reference to a metadata */
		struct {
			uint16_t type;
			uint16_t offset;
			uint16_t mask;
		} meta;
	};
};

/**
 * This structure defines the message be used to composite the
 * profile / key of a SEM control packet
 */
struct cpfl_flow_js_pr_action_sem {
	uint16_t prof;		    /* SEM profile ID */
	uint16_t subprof;	    /* SEM subprofile ID */
	uint16_t keysize;	    /* extract key size in bytes */
	struct cpfl_flow_js_fv *fv; /* A SEM field vector array */
	int fv_size;
};

/* define how to map current key to low level pipeline configuration */
struct cpfl_flow_js_pr_action {
	enum cpfl_flow_pr_action_type type;
	union {
		struct cpfl_flow_js_pr_action_sem sem;
	};
};

/**
 * This structure defines a set of rules that direct PMD how to parse rte_flow
 * protocol headers. Each rule be described by a key object and a action array.
 */
struct cpfl_flow_js_pr {
	struct {
		struct cpfl_flow_js_pr_key_proto *protocols;
		uint16_t proto_size;
		struct cpfl_flow_js_pr_key_attr *attributes;
		uint16_t attr_size;
	} key;
	/* An array to define how to map current key to low level pipeline configuration. */
	struct cpfl_flow_js_pr_action *actions;
	uint16_t actions_size;
};

/* Modification Rules Storage */
/**
 * The vxlan_encap action matches RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP
 * and include a sequence of protocol headers defined in field protocols
 * of data.
 */
struct cpfl_flow_js_mr_key_action_vxlan_encap {
	enum rte_flow_item_type protocols[CPFL_JS_PROTO_NUM_MAX];
	int proto_size;
};

struct cpfl_flow_js_prog_parameter {
	bool has_name;
	uint16_t index;
	char name[CPFL_JS_STR_SIZE];
	uint16_t size;
};

struct cpfl_flow_js_mr_key_action_prog {
	bool has_name;
	uint32_t id;
	char name[CPFL_JS_STR_SIZE];
	uint32_t param_size;
	struct cpfl_flow_js_prog_parameter params[CPFL_JS_PROG_PARAM_NUM_MAX];
};

/* A set of modification rte_flow_action_xxx objects can be defined as a type / data pair. */
struct cpfl_flow_js_mr_key_action {
	enum rte_flow_action_type type;
	union {
		struct cpfl_flow_js_mr_key_action_vxlan_encap encap;
		struct cpfl_flow_js_mr_key_action_prog prog;
	};
};

struct cpfl_flow_js_mr_key {
	struct cpfl_flow_js_mr_key_action *actions;
	int actions_size;
};

struct cpfl_flow_js_mr_layout {
	int index;				/* links to the element of the actions array */
	char hint[CPFL_JS_STR_SIZE]; /* where the data to copy from */
	uint16_t offset;			/* the start byte of the data to copy from */
	uint16_t size; /*  bytes of the data to be copied to the memory region */
};

struct cpfl_flow_js_mr_field {
	char type[CPFL_JS_STR_SIZE];
	uint16_t start;
	uint16_t width;
	union {
		uint16_t index;
		uint8_t value[CPFL_JS_PROG_CONSTANT_VALUE_NUM_MAX];
	};
};

struct cpfl_flow_js_mr_content {
	uint16_t size;
	struct cpfl_flow_js_mr_field fields[CPFL_JS_PROG_CONTENT_FIELD_NUM_MAX];
	int field_size;
};

/** For mod data, besides the profile ID, a layout array defines a set of hints that helps
 * driver composing the MOD memory region when the action need to insert/update some packet
 * data from user input.
 */
struct cpfl_flow_js_mr_action_mod {
	uint16_t prof;
	uint16_t byte_len;
	bool is_content;
	union {
		struct {
			struct cpfl_flow_js_mr_layout layout[CPFL_JS_PROTO_NUM_MAX];
			int layout_size;
		};
		struct cpfl_flow_js_mr_content content;
	};
};

enum cpfl_flow_mr_action_type {
	CPFL_JS_MR_ACTION_TYPE_MOD,
};

/** Currently, the type can only be mod.
 *
 * For mod data, besides the profile ID, a layout array defines a set
 * of hints that helps driver composing the MOD memory region when the
 * action need to insert/update some packet data from user input.
 */
struct cpfl_flow_js_mr_action {
	enum cpfl_flow_mr_action_type type;
	union {
		struct cpfl_flow_js_mr_action_mod mod;
	};
};

/**
 * This structure defines a set of rules that direct PMD to parse rte_flow modification
 * actions. Each rule be described by a pair of key and action
 */
struct cpfl_flow_js_mr {
	struct cpfl_flow_js_mr_key key;
	struct cpfl_flow_js_mr_action action;
};

struct cpfl_flow_js_parser {
	struct cpfl_flow_js_pr *patterns;
	int pr_size;
	struct cpfl_flow_js_mr *modifications;
	int mr_size;
};

/* Pattern Rules */
struct cpfl_flow_pr_action_sem {
	uint16_t prof;
	uint16_t subprof;
	uint16_t keysize;
	uint8_t cpfl_flow_pr_fv[CPFL_JS_SEM_FV_KEY_NUM_MAX];
};

struct cpfl_flow_pr_action {
	enum cpfl_flow_pr_action_type type;
	union {
		struct cpfl_flow_pr_action_sem sem;
	};
};

/* Modification Rules */
struct cpfl_flow_mr_key_action_vxlan_encap {
	enum rte_flow_item_type protocols[CPFL_JS_PROTO_NUM_MAX];
	uint16_t proto_size;
	const struct rte_flow_action *action;
};

struct cpfl_flow_mr_key_action_prog {
	const struct rte_flow_action_prog *prog;
	bool has_name;
	char name[CPFL_JS_PROG_PARAM_NUM_MAX][CPFL_JS_STR_SIZE];
};

struct cpfl_flow_mr_key_mod {
	enum rte_flow_action_type type;
	struct cpfl_flow_mr_key_action_vxlan_encap encap;
};

struct cpfl_flow_mr_key_action {
	struct cpfl_flow_mr_key_mod mods[CPFL_JS_MOD_KEY_NUM_MAX];
	struct cpfl_flow_mr_key_action_prog prog;
};

struct cpfl_flow_mr_action_mod {
	uint16_t prof;
	uint16_t byte_len;
	uint8_t data[256];
};

struct cpfl_flow_mr_action {
	enum cpfl_flow_mr_action_type type;
	union {
		struct cpfl_flow_mr_action_mod mod;
	};
};

int cpfl_parser_create(struct cpfl_flow_js_parser **parser, const char *filename);
int cpfl_parser_destroy(struct cpfl_flow_js_parser *parser);
int cpfl_flow_parse_items(struct cpfl_itf *itf,
			  struct cpfl_flow_js_parser *parser,
			  const struct rte_flow_item *items,
			  const struct rte_flow_attr *attr,
			  struct cpfl_flow_pr_action *pr_action);
int cpfl_flow_parse_actions(struct cpfl_flow_js_parser *parser,
			    const struct rte_flow_action *actions,
			    struct cpfl_flow_mr_action *mr_action);
bool cpfl_metadata_write_port_id(struct cpfl_itf *itf);
bool cpfl_metadata_write_vsi(struct cpfl_itf *itf);
bool cpfl_metadata_write_targetvsi(struct cpfl_itf *itf);
bool cpfl_metadata_write_sourcevsi(struct cpfl_itf *itf);

static inline void
cpfl_metadata_init(struct cpfl_metadata *meta)
{
	int i;

	for (i = 0; i < CPFL_META_LENGTH; i++)
		meta->chunks[i].type = i;
}

static inline void
cpfl_metadata_write16(struct cpfl_metadata *meta, int type, int offset, uint16_t data)
{
	memcpy(&meta->chunks[type].data[offset], &data, sizeof(uint16_t));
}

static inline void
cpfl_metadata_write32(struct cpfl_metadata *meta, int type, int offset, uint32_t data)
{
	memcpy(&meta->chunks[type].data[offset], &data, sizeof(uint32_t));
}

static inline uint16_t
cpfl_metadata_read16(struct cpfl_metadata *meta, int type, int offset)
{
	return *((uint16_t *)(&meta->chunks[type].data[offset]));
}

#endif
