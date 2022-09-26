/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA Corporation & Affiliates
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <rte_common.h>
#include <rte_ethdev.h>
#include <cmdline_parse.h>
#include <cmdline_parse_string.h>
#include <cmdline_parse_num.h>
#include <rte_flow.h>

#include "testpmd.h"

struct flex_item *flex_items[RTE_MAX_ETHPORTS][FLEX_MAX_PARSERS_NUM];
struct flex_pattern flex_patterns[FLEX_MAX_PATTERNS_NUM];

#ifdef RTE_HAS_JANSSON

static struct flex_item *
flex_parser_fetch(uint16_t port_id, uint16_t flex_id)
{
	if (port_id >= RTE_MAX_ETHPORTS) {
		printf("Invalid port_id: %u\n", port_id);
		return FLEX_PARSER_ERR;
	}
	if (flex_id >= FLEX_MAX_PARSERS_NUM) {
		printf("Invalid flex item flex_id: %u\n", flex_id);
		return FLEX_PARSER_ERR;
	}
	return flex_items[port_id][flex_id];
}

static __rte_always_inline bool
match_strkey(const char *key, const char *pattern)
{
	return strncmp(key, pattern, strlen(key)) == 0;
}

static int
flex_tunnel_parse(json_t *jtun, enum rte_flow_item_flex_tunnel_mode *tunnel)
{
	int tun = -1;

	if (json_is_integer(jtun))
		tun = (int)json_integer_value(jtun);
	else if (json_is_real(jtun))
		tun = (int)json_real_value(jtun);
	else if (json_is_string(jtun)) {
		const char *mode = json_string_value(jtun);

		if (match_strkey(mode, "FLEX_TUNNEL_MODE_SINGLE"))
			tun = FLEX_TUNNEL_MODE_SINGLE;
		else if (match_strkey(mode, "FLEX_TUNNEL_MODE_OUTER"))
			tun = FLEX_TUNNEL_MODE_OUTER;
		else if (match_strkey(mode, "FLEX_TUNNEL_MODE_INNER"))
			tun = FLEX_TUNNEL_MODE_INNER;
		else if (match_strkey(mode, "FLEX_TUNNEL_MODE_MULTI"))
			tun = FLEX_TUNNEL_MODE_MULTI;
		else if (match_strkey(mode, "FLEX_TUNNEL_MODE_TUNNEL"))
			tun = FLEX_TUNNEL_MODE_TUNNEL;
		else
			return -EINVAL;
	} else
		return -EINVAL;
	*tunnel = (enum rte_flow_item_flex_tunnel_mode)tun;
	return 0;
}

static int
flex_field_parse(json_t *jfld, struct rte_flow_item_flex_field *fld)
{
	const char *key;
	json_t *je;

#define FLEX_FIELD_GET(fm, t) \
do {                  \
	if (!strncmp(key, # fm, strlen(# fm))) { \
		if (json_is_real(je))   \
			fld->fm = (t) json_real_value(je); \
		else if (json_is_integer(je))   \
			fld->fm = (t) json_integer_value(je); \
		else   \
			return -EINVAL; \
	}         \
} while (0)

	json_object_foreach(jfld, key, je) {
		FLEX_FIELD_GET(field_size, uint32_t);
		FLEX_FIELD_GET(field_base, int32_t);
		FLEX_FIELD_GET(offset_base, uint32_t);
		FLEX_FIELD_GET(offset_mask, uint32_t);
		FLEX_FIELD_GET(offset_shift, int32_t);
		FLEX_FIELD_GET(field_id, uint16_t);
		if (match_strkey(key, "field_mode")) {
			const char *mode;
			if (!json_is_string(je))
				return -EINVAL;
			mode = json_string_value(je);
			if (match_strkey(mode, "FIELD_MODE_DUMMY"))
				fld->field_mode = FIELD_MODE_DUMMY;
			else if (match_strkey(mode, "FIELD_MODE_FIXED"))
				fld->field_mode = FIELD_MODE_FIXED;
			else if (match_strkey(mode, "FIELD_MODE_OFFSET"))
				fld->field_mode = FIELD_MODE_OFFSET;
			else if (match_strkey(mode, "FIELD_MODE_BITMASK"))
				fld->field_mode = FIELD_MODE_BITMASK;
			else
				return -EINVAL;
		}
	}
	return 0;
}

enum flex_link_type {
	FLEX_LINK_IN = 0,
	FLEX_LINK_OUT = 1
};

static int
flex_link_item_parse(const char *src, struct rte_flow_item *item)
{
#define  FLEX_PARSE_DATA_SIZE 1024

	int ret;
	uint8_t *ptr, data[FLEX_PARSE_DATA_SIZE] = {0,};
	char flow_rule[256];
	struct rte_flow_attr *attr;
	struct rte_flow_item *pattern;
	struct rte_flow_action *actions;

	sprintf(flow_rule,
		"flow create 0 pattern %s / end actions drop / end", src);
	src = flow_rule;
	ret = flow_parse(src, (void *)data, sizeof(data),
			 &attr, &pattern, &actions);
	if (ret)
		return ret;
	item->type = pattern->type;
	if (pattern->spec) {
		ptr = (void *)(uintptr_t)item->spec;
		memcpy(ptr, pattern->spec, FLEX_MAX_FLOW_PATTERN_LENGTH);
	} else {
		item->spec = NULL;
	}
	if (pattern->mask) {
		ptr = (void *)(uintptr_t)item->mask;
		memcpy(ptr, pattern->mask, FLEX_MAX_FLOW_PATTERN_LENGTH);
	} else {
		item->mask = NULL;
	}
	if (pattern->last) {
		ptr = (void *)(uintptr_t)item->last;
		memcpy(ptr, pattern->last, FLEX_MAX_FLOW_PATTERN_LENGTH);
	} else {
		item->last = NULL;
	}
	return 0;
}

static int
flex_link_parse(json_t *jobj, struct rte_flow_item_flex_link *link,
		enum flex_link_type link_type)
{
	const char *key;
	json_t *je;
	int ret;
	json_object_foreach(jobj, key, je) {
		if (match_strkey(key, "item")) {
			if (!json_is_string(je))
				return -EINVAL;
			ret = flex_link_item_parse(json_string_value(je),
						   &link->item);
			if (ret)
				return -EINVAL;
			if (link_type == FLEX_LINK_IN) {
				if (!link->item.spec || !link->item.mask)
					return -EINVAL;
				if (link->item.last)
					return -EINVAL;
			}
		}
		if (match_strkey(key, "next")) {
			if (json_is_integer(je))
				link->next = (typeof(link->next))
					     json_integer_value(je);
			else if (json_is_real(je))
				link->next = (typeof(link->next))
					     json_real_value(je);
			else
				return -EINVAL;
		}
	}
	return 0;
}

static int flex_item_config(json_t *jroot,
			    struct rte_flow_item_flex_conf *flex_conf)
{
	const char *key;
	json_t *jobj = NULL;
	int ret = 0;

	json_object_foreach(jroot, key, jobj) {
		if (match_strkey(key, "tunnel")) {
			ret = flex_tunnel_parse(jobj, &flex_conf->tunnel);
			if (ret) {
				printf("Can't parse tunnel value\n");
				goto out;
			}
		} else if (match_strkey(key, "next_header")) {
			ret = flex_field_parse(jobj, &flex_conf->next_header);
			if (ret) {
				printf("Can't parse next_header field\n");
				goto out;
			}
		} else if (match_strkey(key, "next_protocol")) {
			ret = flex_field_parse(jobj,
					       &flex_conf->next_protocol);
			if (ret) {
				printf("Can't parse next_protocol field\n");
				goto out;
			}
		} else if (match_strkey(key, "sample_data")) {
			json_t *ji;
			uint32_t i, size = json_array_size(jobj);
			for (i = 0; i < size; i++) {
				ji = json_array_get(jobj, i);
				ret = flex_field_parse
					(ji, flex_conf->sample_data + i);
				if (ret) {
					printf("Can't parse sample_data field(s)\n");
					goto out;
				}
			}
			flex_conf->nb_samples = size;
		} else if (match_strkey(key, "input_link")) {
			json_t *ji;
			uint32_t i, size = json_array_size(jobj);
			for (i = 0; i < size; i++) {
				ji = json_array_get(jobj, i);
				ret = flex_link_parse(ji,
						      flex_conf->input_link + i,
						      FLEX_LINK_IN);
				if (ret) {
					printf("Can't parse input_link(s)\n");
					goto out;
				}
			}
			flex_conf->nb_inputs = size;
		} else if (match_strkey(key, "output_link")) {
			json_t *ji;
			uint32_t i, size = json_array_size(jobj);
			for (i = 0; i < size; i++) {
				ji = json_array_get(jobj, i);
				ret = flex_link_parse
					(ji, flex_conf->output_link + i,
					 FLEX_LINK_OUT);
				if (ret) {
					printf("Can't parse output_link(s)\n");
					goto out;
				}
			}
			flex_conf->nb_outputs = size;
		}
	}
out:
	return ret;
}

static struct flex_item *
flex_item_init(void)
{
	size_t base_size, samples_size, links_size, spec_size;
	struct rte_flow_item_flex_conf *conf;
	struct flex_item *fp;
	uint8_t (*pattern)[FLEX_MAX_FLOW_PATTERN_LENGTH];
	int i;

	base_size = RTE_ALIGN(sizeof(*conf), sizeof(uintptr_t));
	samples_size = RTE_ALIGN(FLEX_ITEM_MAX_SAMPLES_NUM *
				 sizeof(conf->sample_data[0]),
				 sizeof(uintptr_t));
	links_size = RTE_ALIGN(FLEX_ITEM_MAX_LINKS_NUM *
			       sizeof(conf->input_link[0]),
			       sizeof(uintptr_t));
	/* spec & mask for all input links */
	spec_size = 2 * FLEX_MAX_FLOW_PATTERN_LENGTH * FLEX_ITEM_MAX_LINKS_NUM;
	fp = calloc(1, base_size + samples_size + 2 * links_size + spec_size);
	if (fp == NULL) {
		printf("Can't allocate memory for flex item\n");
		return NULL;
	}
	conf = &fp->flex_conf;
	conf->sample_data = (typeof(conf->sample_data))
			    ((uint8_t *)fp + base_size);
	conf->input_link = (typeof(conf->input_link))
			   ((uint8_t *)conf->sample_data + samples_size);
	conf->output_link = (typeof(conf->output_link))
			    ((uint8_t *)conf->input_link + links_size);
	pattern = (typeof(pattern))((uint8_t *)conf->output_link + links_size);
	for (i = 0; i < FLEX_ITEM_MAX_LINKS_NUM; i++) {
		struct rte_flow_item_flex_link *in = conf->input_link + i;
		in->item.spec = pattern++;
		in->item.mask = pattern++;
	}
	return fp;
}

static int
flex_item_build_config(struct flex_item *fp, const char *filename)
{
	int ret;
	json_error_t json_error;
	json_t *jroot = json_load_file(filename, 0, &json_error);

	if (!jroot) {
		printf("Bad JSON file \"%s\": %s\n", filename, json_error.text);
		return -1;
	}
	ret = flex_item_config(jroot, &fp->flex_conf);
	json_decref(jroot);
	return ret;
}

void
flex_item_create(portid_t port_id, uint16_t flex_id, const char *filename)
{
	struct rte_flow_error flow_error;
	struct flex_item *fp = flex_parser_fetch(port_id, flex_id);
	int ret;

	if (fp == FLEX_PARSER_ERR) {
		printf("Bad parameters: port_id=%u flex_id=%u\n",
		       port_id, flex_id);
		return;
	}
	if (fp) {
		printf("port-%u: flex item #%u is already in use\n",
		       port_id, flex_id);
		return;
	}
	fp = flex_item_init();
	if (!fp) {
		printf("Could not allocate flex item\n");
		goto out;
	}
	ret = flex_item_build_config(fp, filename);
	if (ret)
		goto out;
	fp->flex_handle = rte_flow_flex_item_create(port_id,
						    &fp->flex_conf,
						    &flow_error);
	if (fp->flex_handle) {
		flex_items[port_id][flex_id] = fp;
		printf("port-%u: created flex item #%u\n", port_id, flex_id);
		fp = NULL;
	} else {
		printf("port-%u: flex item #%u creation failed: %s\n",
		       port_id, flex_id,
		       flow_error.message ? flow_error.message : "");
	}
out:
	free(fp);
}

void
flex_item_destroy(portid_t port_id, uint16_t flex_id)
{
	int ret;
	struct rte_flow_error error;
	struct flex_item *fp = flex_parser_fetch(port_id, flex_id);
	if (fp == FLEX_PARSER_ERR) {
		printf("Bad parameters: port_id=%u flex_id=%u\n",
		       port_id, flex_id);
		return;
	}
	if (!fp)
		return;
	ret = rte_flow_flex_item_release(port_id, fp->flex_handle, &error);
	if (!ret) {
		free(fp);
		flex_items[port_id][flex_id] = NULL;
		printf("port-%u: released flex item #%u\n",
		       port_id, flex_id);

	} else {
		printf("port-%u: cannot release flex item #%u: %s\n",
		       port_id, flex_id, error.message);
	}
}

#else /* RTE_HAS_JANSSON */
void flex_item_create(__rte_unused portid_t port_id,
		      __rte_unused uint16_t flex_id,
		      __rte_unused const char *filename)
{
	printf("cannot create flex item - no JSON library configured\n");
}

void
flex_item_destroy(__rte_unused portid_t port_id, __rte_unused uint16_t flex_id)
{

}

#endif /* RTE_HAS_JANSSON */

void
port_flex_item_flush(portid_t port_id)
{
	uint16_t i;

	for (i = 0; i < FLEX_MAX_PARSERS_NUM; i++) {
		if (flex_items[port_id][i] != NULL) {
			flex_item_destroy(port_id, i);
			flex_items[port_id][i] = NULL;
		}
	}
}

struct flex_pattern_set {
	cmdline_fixed_string_t set, flex_pattern;
	cmdline_fixed_string_t is_spec, mask;
	cmdline_fixed_string_t spec_data, mask_data;
	uint16_t id;
};

static cmdline_parse_token_string_t flex_pattern_set_token =
	TOKEN_STRING_INITIALIZER(struct flex_pattern_set, set, "set");
static cmdline_parse_token_string_t flex_pattern_token =
	TOKEN_STRING_INITIALIZER(struct flex_pattern_set,
flex_pattern, "flex_pattern");
static cmdline_parse_token_string_t flex_pattern_is_token =
	TOKEN_STRING_INITIALIZER(struct flex_pattern_set,
is_spec, "is");
static cmdline_parse_token_string_t flex_pattern_spec_token =
	TOKEN_STRING_INITIALIZER(struct flex_pattern_set,
is_spec, "spec");
static cmdline_parse_token_string_t flex_pattern_mask_token =
	TOKEN_STRING_INITIALIZER(struct flex_pattern_set, mask, "mask");
static cmdline_parse_token_string_t flex_pattern_spec_data_token =
	TOKEN_STRING_INITIALIZER(struct flex_pattern_set, spec_data, NULL);
static cmdline_parse_token_string_t flex_pattern_mask_data_token =
	TOKEN_STRING_INITIALIZER(struct flex_pattern_set, mask_data, NULL);
static cmdline_parse_token_num_t flex_pattern_id_token =
	TOKEN_NUM_INITIALIZER(struct flex_pattern_set, id, RTE_UINT16);

/*
 * flex pattern data - spec or mask is a string representation of byte array
 * in hexadecimal format. Each byte in data string must have 2 characters:
 * 0x15 - "15"
 * 0x1  - "01"
 * Bytes in data array are in network order.
 */
static uint32_t
flex_pattern_data(const char *str, uint8_t *data)
{
	uint32_t i, len = strlen(str);
	char b[3], *endptr;

	if (len & 01)
		return 0;
	len /= 2;
	if (len >= FLEX_MAX_FLOW_PATTERN_LENGTH)
		return 0;
	for (i = 0, b[2] = '\0'; i < len; i++) {
		b[0] = str[2 * i];
		b[1] = str[2 * i + 1];
		data[i] = strtoul(b, &endptr, 16);
		if (endptr != &b[2])
			return 0;
	}
	return len;
}

static void
flex_pattern_parsed_fn(void *parsed_result,
		       __rte_unused struct cmdline *cl,
		       __rte_unused void *data)
{
	struct flex_pattern_set *res = parsed_result;
	struct flex_pattern *fp;
	bool full_spec;

	if (res->id >= FLEX_MAX_PATTERNS_NUM) {
		printf("Bad flex pattern id\n");
		return;
	}
	fp = flex_patterns + res->id;
	memset(fp->spec_pattern, 0, sizeof(fp->spec_pattern));
	memset(fp->mask_pattern, 0, sizeof(fp->mask_pattern));
	fp->spec.length = flex_pattern_data(res->spec_data, fp->spec_pattern);
	if (!fp->spec.length) {
		printf("Bad flex pattern spec\n");
		return;
	}
	full_spec = strncmp(res->is_spec, "spec", strlen("spec")) == 0;
	if (full_spec) {
		fp->mask.length = flex_pattern_data(res->mask_data,
						    fp->mask_pattern);
		if (!fp->mask.length) {
			printf("Bad flex pattern mask\n");
			return;
		}
	} else {
		memset(fp->mask_pattern, 0xFF, fp->spec.length);
		fp->mask.length = fp->spec.length;
	}
	if (fp->mask.length != fp->spec.length) {
		printf("Spec length do not match mask length\n");
		return;
	}
	fp->spec.pattern = fp->spec_pattern;
	fp->mask.pattern = fp->mask_pattern;
	printf("created pattern #%u\n", res->id);
}

cmdline_parse_inst_t cmd_set_flex_is_pattern = {
	.f = flex_pattern_parsed_fn,
	.data = NULL,
	.help_str = "set flex_pattern <id> is <spec_data>",
	.tokens = {
		(void *)&flex_pattern_set_token,
		(void *)&flex_pattern_token,
		(void *)&flex_pattern_id_token,
		(void *)&flex_pattern_is_token,
		(void *)&flex_pattern_spec_data_token,
		NULL,
	}
};

cmdline_parse_inst_t cmd_set_flex_spec_pattern = {
	.f = flex_pattern_parsed_fn,
	.data = NULL,
	.help_str = "set flex_pattern <id> spec <spec_data> mask <mask_data>",
	.tokens = {
		(void *)&flex_pattern_set_token,
		(void *)&flex_pattern_token,
		(void *)&flex_pattern_id_token,
		(void *)&flex_pattern_spec_token,
		(void *)&flex_pattern_spec_data_token,
		(void *)&flex_pattern_mask_token,
		(void *)&flex_pattern_mask_data_token,
		NULL,
	}
};
