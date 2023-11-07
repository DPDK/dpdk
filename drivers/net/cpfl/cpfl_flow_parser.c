/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include <arpa/inet.h>

#include "cpfl_flow_parser.h"

static enum rte_flow_item_type
cpfl_get_item_type_by_str(const char *type)
{
	if (strcmp(type, "eth") == 0)
		return RTE_FLOW_ITEM_TYPE_ETH;
	else if (strcmp(type, "ipv4") == 0)
		return RTE_FLOW_ITEM_TYPE_IPV4;
	else if (strcmp(type, "tcp") == 0)
		return RTE_FLOW_ITEM_TYPE_TCP;
	else if (strcmp(type, "udp") == 0)
		return RTE_FLOW_ITEM_TYPE_UDP;
	else if (strcmp(type, "vxlan") == 0)
		return RTE_FLOW_ITEM_TYPE_VXLAN;
	else if (strcmp(type, "icmp") == 0)
		return RTE_FLOW_ITEM_TYPE_ICMP;
	else if (strcmp(type, "vlan") == 0)
		return RTE_FLOW_ITEM_TYPE_VLAN;

	PMD_DRV_LOG(ERR, "Not support this type: %s.", type);
	return RTE_FLOW_ITEM_TYPE_VOID;
}

static enum rte_flow_action_type
cpfl_get_action_type_by_str(const char *type)
{
	if (strcmp(type, "vxlan_encap") == 0)
		return RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP;
	else if (strcmp(type, "vxlan_decap") == 0)
		return RTE_FLOW_ACTION_TYPE_VXLAN_DECAP;
	else if (strcmp(type, "prog") == 0)
		return RTE_FLOW_ACTION_TYPE_PROG;

	PMD_DRV_LOG(ERR, "Not support this type: %s.", type);
	return RTE_FLOW_ACTION_TYPE_VOID;
}

static const char *
cpfl_json_t_to_string(json_t *object, const char *name)
{
	json_t *subobject;

	if (!object) {
		PMD_DRV_LOG(ERR, "object doesn't exist.");
		return NULL;
	}
	subobject = json_object_get(object, name);
	if (!subobject) {
		PMD_DRV_LOG(ERR, "%s doesn't exist.", name);
		return NULL;
	}

	return json_string_value(subobject);
}

static int
cpfl_json_t_to_int(json_t *object, const char *name, int *value)
{
	json_t *subobject;

	if (!object) {
		PMD_DRV_LOG(ERR, "object doesn't exist.");
		return -EINVAL;
	}
	subobject = json_object_get(object, name);
	if (!subobject) {
		PMD_DRV_LOG(ERR, "%s doesn't exist.", name);
		return -EINVAL;
	}
	if (!json_is_integer(subobject)) {
		PMD_DRV_LOG(ERR, "%s is not an integer.", name);
		return -EINVAL;
	}
	*value = (int)json_integer_value(subobject);

	return 0;
}

static int
cpfl_json_t_to_uint16(json_t *object, const char *name, uint16_t *value)
{
	json_t *subobject;

	if (!object) {
		PMD_DRV_LOG(ERR, "object doesn't exist.");
		return -EINVAL;
	}
	subobject = json_object_get(object, name);
	if (!subobject) {
		PMD_DRV_LOG(ERR, "%s doesn't exist.", name);
		return -EINVAL;
	}
	if (!json_is_integer(subobject)) {
		PMD_DRV_LOG(ERR, "%s is not an integer.", name);
		return -EINVAL;
	}
	*value = (uint16_t)json_integer_value(subobject);

	return 0;
}

static int
cpfl_json_t_to_uint32(json_t *object, const char *name, uint32_t *value)
{
	json_t *subobject;

	if (!object) {
		PMD_DRV_LOG(ERR, "object doesn't exist.");
		return -EINVAL;
	}
	subobject = json_object_get(object, name);
	if (!subobject) {
		PMD_DRV_LOG(ERR, "%s doesn't exist.", name);
		return -EINVAL;
	}
	if (!json_is_integer(subobject)) {
		PMD_DRV_LOG(ERR, "%s is not an integer.", name);
		return -EINVAL;
	}
	*value = (uint32_t)json_integer_value(subobject);

	return 0;
}

static int
cpfl_flow_js_pattern_key_attr(json_t *ob_pr_key_attrs, struct cpfl_flow_js_pr *js_pr)
{
	int i, len;
	struct cpfl_flow_js_pr_key_attr *attr;

	len = json_array_size(ob_pr_key_attrs);
	js_pr->key.attributes = rte_malloc(NULL, sizeof(struct cpfl_flow_js_pr_key_attr), 0);
	if (!js_pr->key.attributes) {
		PMD_DRV_LOG(ERR, "Failed to alloc memory.");
		return -ENOMEM;
	}
	js_pr->key.attr_size = len;
	attr = js_pr->key.attributes;
	for (i = 0; i < len; i++) {
		json_t *object;
		const char *name;
		uint16_t value = 0;
		int ret;

		object = json_array_get(ob_pr_key_attrs, i);
		name = cpfl_json_t_to_string(object, "Name");
		if (!name) {
			PMD_DRV_LOG(ERR, "Can not parse string 'Name'.");
			goto err;
		}
		ret = cpfl_json_t_to_uint16(object, "Value", &value);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Can not parse 'value'.");
			goto err;
		}
		if (strcmp(name, "ingress") == 0) {
			attr->ingress = value;
		} else if (strcmp(name, "egress") == 0) {
			attr->egress = value;
		} else {
			/* TODO: more... */
			PMD_DRV_LOG(ERR, "Not support attr name: %s.", name);
			goto err;
		}
	}

	return 0;
err:
	rte_free(js_pr->key.attributes);
	return -EINVAL;
}

static int
cpfl_flow_js_pattern_key_proto_field(json_t *ob_fields,
				     struct cpfl_flow_js_pr_key_proto *js_field)
{
	int len, i;

	if (!ob_fields)
		return 0;
	len = json_array_size(ob_fields);
	if (len == 0)
		return 0;
	js_field->fields_size = len;
	js_field->fields =
	    rte_malloc(NULL, sizeof(struct cpfl_flow_js_pr_key_proto_field) * len, 0);
	if (!js_field->fields) {
		PMD_DRV_LOG(ERR, "Failed to alloc memory.");
		return -ENOMEM;
	}
	for (i = 0; i < len; i++) {
		json_t *object;
		const char *name, *mask;

		object = json_array_get(ob_fields, i);
		name = cpfl_json_t_to_string(object, "name");
		if (!name) {
			PMD_DRV_LOG(ERR, "Can not parse string 'name'.");
			goto err;
		}
		if (strlen(name) > CPFL_JS_STR_SIZE - 1) {
			PMD_DRV_LOG(ERR, "The 'name' is too long.");
			goto err;
		}
		strncpy(js_field->fields[i].name, name, CPFL_JS_STR_SIZE - 1);

		if (js_field->type == RTE_FLOW_ITEM_TYPE_ETH ||
		    js_field->type == RTE_FLOW_ITEM_TYPE_IPV4) {
			mask = cpfl_json_t_to_string(object, "mask");
			if (!mask) {
				PMD_DRV_LOG(ERR, "Can not parse string 'mask'.");
				goto err;
			}
			if (strlen(mask) > CPFL_JS_STR_SIZE - 1) {
				PMD_DRV_LOG(ERR, "The 'mask' is too long.");
				goto err;
			}
			strncpy(js_field->fields[i].mask, mask, CPFL_JS_STR_SIZE - 1);
		} else {
			uint32_t mask_32b;
			int ret;

			ret = cpfl_json_t_to_uint32(object, "mask", &mask_32b);
			if (ret < 0) {
				PMD_DRV_LOG(ERR, "Can not parse uint32 'mask'.");
				goto err;
			}
			js_field->fields[i].mask_32b = mask_32b;
		}
	}

	return 0;

err:
	rte_free(js_field->fields);
	return -EINVAL;
}

static int
cpfl_flow_js_pattern_key_proto(json_t *ob_pr_key_protos, struct cpfl_flow_js_pr *js_pr)
{
	int len, i, ret;

	len = json_array_size(ob_pr_key_protos);
	if (len == 0)
		return 0;
	js_pr->key.proto_size = len;
	js_pr->key.protocols = rte_malloc(NULL, sizeof(struct cpfl_flow_js_pr_key_proto) * len, 0);
	if (!js_pr->key.protocols) {
		PMD_DRV_LOG(ERR, "Failed to alloc memory.");
		return -ENOMEM;
	}

	for (i = 0; i < len; i++) {
		json_t *object, *ob_fields;
		const char *type;
		enum rte_flow_item_type item_type;

		object = json_array_get(ob_pr_key_protos, i);
		/* pr->key->proto->type */
		type = cpfl_json_t_to_string(object, "type");
		if (!type) {
			PMD_DRV_LOG(ERR, "Can not parse string 'type'.");
			goto err;
		}
		item_type = cpfl_get_item_type_by_str(type);
		if (item_type == RTE_FLOW_ITEM_TYPE_VOID)
			goto err;
		js_pr->key.protocols[i].type = item_type;
		/* pr->key->proto->fields */
		ob_fields = json_object_get(object, "fields");
		ret = cpfl_flow_js_pattern_key_proto_field(ob_fields,
							   &js_pr->key.protocols[i]);
		if (ret < 0)
			goto err;
	}

	return 0;

err:
	rte_free(js_pr->key.protocols);
	return -EINVAL;
}

static int
cpfl_flow_js_pattern_act_fv_proto(json_t *ob_value, struct cpfl_flow_js_fv *js_fv)
{
	uint16_t layer = 0, offset = 0, mask = 0;
	const char *header;
	enum rte_flow_item_type type;
	int ret;

	ret = cpfl_json_t_to_uint16(ob_value, "layer", &layer);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Can not parse 'value'.");
		return -EINVAL;
	}

	header = cpfl_json_t_to_string(ob_value, "header");
	if (!header) {
		PMD_DRV_LOG(ERR, "Can not parse string 'header'.");
		return -EINVAL;
	}
	ret = cpfl_json_t_to_uint16(ob_value, "offset", &offset);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Can not parse 'offset'.");
		return -EINVAL;
	}
	ret = cpfl_json_t_to_uint16(ob_value, "mask", &mask);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Can not parse 'mask'.");
		return -EINVAL;
	}
	type = cpfl_get_item_type_by_str(header);
	if (type == RTE_FLOW_ITEM_TYPE_VOID)
		return -EINVAL;
	js_fv->proto.layer = layer;
	js_fv->proto.offset = offset;
	js_fv->proto.mask = mask;
	js_fv->proto.header = type;

	return 0;
}

static int
cpfl_flow_js_pattern_act_fv_metadata(json_t *ob_value, struct cpfl_flow_js_fv *js_fv)
{
	int ret;

	ret = cpfl_json_t_to_uint16(ob_value, "type", &js_fv->meta.type);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Can not parse 'size'.");
		return ret;
	}
	ret = cpfl_json_t_to_uint16(ob_value, "offset", &js_fv->meta.offset);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Can not parse 'size'.");
		return ret;
	}
	ret = cpfl_json_t_to_uint16(ob_value, "mask", &js_fv->meta.mask);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Can not parse 'size'.");
		return ret;
	}

	return 0;
}

static int
cpfl_flow_js_pattern_act_fv(json_t *ob_fvs, struct cpfl_flow_js_pr_action *js_act)
{
	int len, i;

	len = json_array_size(ob_fvs);
	if (len == 0)
		return 0;
	js_act->sem.fv = rte_malloc(NULL, sizeof(struct cpfl_flow_js_fv) * len, 0);
	if (!js_act->sem.fv) {
		PMD_DRV_LOG(ERR, "Failed to alloc memory.");
		return -ENOMEM;
	}
	js_act->sem.fv_size = len;
	for (i = 0; i < len; i++) {
		struct cpfl_flow_js_fv *js_fv;
		json_t *object, *ob_value;
		uint16_t offset = 0;
		const char *type;
		int ret;

		js_fv = &js_act->sem.fv[i];
		object = json_array_get(ob_fvs, i);
		ret = cpfl_json_t_to_uint16(object, "offset", &offset);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Can not parse 'offset'.");
			goto err;
		}
		js_fv->offset = offset;

		type = cpfl_json_t_to_string(object, "type");
		if (!type) {
			PMD_DRV_LOG(ERR, "Can not parse string 'type'.");
			goto err;
		}
		ob_value = json_object_get(object, "value");
		if (strcmp(type, "immediate") == 0) {
			js_fv->type = CPFL_FV_TYPE_IMMEDIATE;
			js_fv->immediate = json_integer_value(ob_value);
		} else if (strcmp(type, "metadata") == 0) {
			js_fv->type = CPFL_FV_TYPE_METADATA;
			cpfl_flow_js_pattern_act_fv_metadata(ob_value, js_fv);
		} else if (strcmp(type, "protocol") == 0) {
			js_fv->type = CPFL_FV_TYPE_PROTOCOL;
			cpfl_flow_js_pattern_act_fv_proto(ob_value, js_fv);
		} else {
			PMD_DRV_LOG(ERR, "Not support this type: %s.", type);
			goto err;
		}
	}

	return 0;

err:
	rte_free(js_act->sem.fv);
	return -EINVAL;
}

static int
cpfl_flow_js_pattern_per_act(json_t *ob_per_act, struct cpfl_flow_js_pr_action *js_act)
{
	const char *type;
	int ret;

	/* pr->actions->type */
	type = cpfl_json_t_to_string(ob_per_act, "type");
	if (!type) {
		PMD_DRV_LOG(ERR, "Can not parse string 'type'.");
		return -EINVAL;
	}
	/* pr->actions->data */
	if (strcmp(type, "sem") == 0) {
		json_t *ob_fvs, *ob_sem;

		js_act->type = CPFL_JS_PR_ACTION_TYPE_SEM;
		ob_sem = json_object_get(ob_per_act, "data");
		ret = cpfl_json_t_to_uint16(ob_sem, "profile", &js_act->sem.prof);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Can not parse 'profile'.");
			return -EINVAL;
		}
		ret = cpfl_json_t_to_uint16(ob_sem, "subprofile", &js_act->sem.subprof);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Can not parse 'subprofile'.");
			return -EINVAL;
		}
		ret = cpfl_json_t_to_uint16(ob_sem, "keysize", &js_act->sem.keysize);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Can not parse 'keysize'.");
			return -EINVAL;
		}
		ob_fvs = json_object_get(ob_sem, "fieldvectors");
		ret = cpfl_flow_js_pattern_act_fv(ob_fvs, js_act);
		if (ret < 0)
			return ret;
	} else {
		PMD_DRV_LOG(ERR, "Not support this type: %s.", type);
		return -EINVAL;
	}

	return 0;
}

static int
cpfl_flow_js_pattern_act(json_t *ob_pr_acts, struct cpfl_flow_js_pr *js_pr)
{
	int i, len, ret;

	len = json_array_size(ob_pr_acts);
	if (len == 0)
		return 0;
	js_pr->actions = rte_malloc(NULL, sizeof(struct cpfl_flow_js_pr_action) * len, 0);
	if (!js_pr->actions) {
		PMD_DRV_LOG(ERR, "Failed to alloc memory.");
		return -ENOMEM;
	}
	js_pr->actions_size = len;
	for (i = 0; i < len; i++) {
		struct cpfl_flow_js_pr_action *js_act;
		json_t *object;

		object = json_array_get(ob_pr_acts, i);
		js_act = &js_pr->actions[i];
		ret = cpfl_flow_js_pattern_per_act(object, js_act);
		if (ret < 0) {
			rte_free(js_pr->actions);
			PMD_DRV_LOG(ERR, "Can not parse pattern action.");
			return -EINVAL;
		}
	}

	return 0;
}

/**
 * The patterns object array defines a set of rules directing the PMD to match sequences of
 * rte_flow protocol headers and translate them into profile/field vectors for each pipeline
 * stage. This object is mandatory.
 */
static int
cpfl_flow_js_pattern_rule(json_t *ob_root, struct cpfl_flow_js_parser *parser)
{
	json_t *ob_prs;
	int i, len;

	/* Pattern Rules */
	ob_prs = json_object_get(ob_root, "patterns");
	if (!ob_prs) {
		PMD_DRV_LOG(ERR, "The patterns is mandatory.");
		return -EINVAL;
	}

	len = json_array_size(ob_prs);
	if (len == 0)
		return 0;
	parser->patterns = rte_malloc(NULL, sizeof(struct cpfl_flow_js_pr) * len, 0);
	if (!parser->patterns) {
		PMD_DRV_LOG(ERR, "Failed to alloc memory.");
		return -ENOMEM;
	}
	parser->pr_size = len;
	for (i = 0; i < len; i++) {
		json_t *object;
		json_t *ob_pr_actions;
		json_t *ob_pr_key;
		json_t *ob_pr_key_protos;
		json_t *ob_pr_key_attrs;
		int ret;

		object = json_array_get(ob_prs, i);
		/* pr->key */
		ob_pr_key = json_object_get(object, "key");
		/* pr->key->protocols */
		ob_pr_key_protos = json_object_get(ob_pr_key, "protocols");
		ret = cpfl_flow_js_pattern_key_proto(ob_pr_key_protos, &parser->patterns[i]);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Can not parse key->protocols.");
			goto err;
		}
		/* pr->key->attributes */
		ob_pr_key_attrs = json_object_get(ob_pr_key, "attributes");
		ret = cpfl_flow_js_pattern_key_attr(ob_pr_key_attrs, &parser->patterns[i]);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Can not parse key->attributes.");
			goto err;
		}
		/* pr->actions */
		ob_pr_actions = json_object_get(object, "actions");
		ret = cpfl_flow_js_pattern_act(ob_pr_actions, &parser->patterns[i]);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Can not parse pattern action.");
			goto err;
		}
	}

	return 0;

err:
	rte_free(parser->patterns);
	return -EINVAL;
}

static int
cpfl_flow_js_mr_key(json_t *ob_mr_keys, struct cpfl_flow_js_mr_key *js_mr_key)
{
	int len, i;

	len = json_array_size(ob_mr_keys);
	if (len == 0)
		return 0;
	js_mr_key->actions = rte_malloc(NULL, sizeof(struct cpfl_flow_js_mr_key_action) * len, 0);
	if (!js_mr_key->actions) {
		PMD_DRV_LOG(ERR, "Failed to alloc memory.");
		return -ENOMEM;
	}
	js_mr_key->actions_size = len;
	for (i = 0; i < len; i++) {
		json_t *object, *ob_data;
		const char *type;
		enum rte_flow_action_type act_type;

		object = json_array_get(ob_mr_keys, i);
		/* mr->key->actions->type */
		type = cpfl_json_t_to_string(object, "type");
		if (!type) {
			PMD_DRV_LOG(ERR, "Can not parse string 'type'.");
			goto err;
		}
		act_type = cpfl_get_action_type_by_str(type);
		if (act_type == RTE_FLOW_ACTION_TYPE_VOID)
			goto err;
		js_mr_key->actions[i].type = act_type;
		/* mr->key->actions->data */
		ob_data = json_object_get(object, "data");
		if (js_mr_key->actions[i].type == RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP) {
			json_t *ob_protos;
			int proto_size, j;
			struct cpfl_flow_js_mr_key_action_vxlan_encap *encap;

			ob_protos = json_object_get(ob_data, "protocols");
			encap = &js_mr_key->actions[i].encap;
			if (!ob_protos) {
				encap->proto_size = 0;
				continue;
			}
			proto_size = json_array_size(ob_protos);
			encap->proto_size = proto_size;
			for (j = 0; j < proto_size; j++) {
				const char *s;
				json_t *subobject;
				enum rte_flow_item_type proto_type;

				subobject = json_array_get(ob_protos, j);
				s = json_string_value(subobject);
				proto_type = cpfl_get_item_type_by_str(s);
				if (proto_type == RTE_FLOW_ITEM_TYPE_VOID) {
					PMD_DRV_LOG(ERR, "parse VXLAN_ENCAP failed.");
					goto err;
				}
				encap->protocols[j] = proto_type;
			}
		} else if (js_mr_key->actions[i].type == RTE_FLOW_ACTION_TYPE_PROG) {
			int ret;
			uint32_t param_size, j;
			uint16_t value = 0;
			json_t *ob_param, *subobject;
			const char *name;

			ret = cpfl_json_t_to_uint32(object, "id", &js_mr_key->actions[i].prog.id);
			if (ret < 0) {
				PMD_DRV_LOG(ERR, "Can not parse uint32 'id'.");
				goto err;
			}
			if (json_object_get(object, "name")) {
				js_mr_key->actions[i].prog.has_name = TRUE;
				name = cpfl_json_t_to_string(object, "name");
				if (!name) {
					PMD_DRV_LOG(ERR, "Can not parse string 'name'.");
					goto err;
				}
				if (strlen(name) > CPFL_JS_STR_SIZE - 1) {
					PMD_DRV_LOG(ERR, "The 'name' is too long.");
					goto err;
				}
				strncpy(js_mr_key->actions[i].prog.name, name,
					CPFL_JS_STR_SIZE - 1);
			}

			ob_param = json_object_get(object, "parameters");
			param_size = json_array_size(ob_param);
			js_mr_key->actions[i].prog.param_size = param_size;
			for (j = 0; j < param_size; j++) {
				subobject = json_array_get(ob_param, j);
				ret = cpfl_json_t_to_uint16(subobject, "index", &value);
				if (ret < 0) {
					PMD_DRV_LOG(ERR, "Can not parse 'index'.");
					goto err;
				}
				js_mr_key->actions[i].prog.params[j].index = value;
				if (json_object_get(subobject, "name")) {
					js_mr_key->actions[i].prog.params[j].has_name = TRUE;
					name = cpfl_json_t_to_string(subobject, "name");
					if (!name) {
						PMD_DRV_LOG(ERR, "Can not parse string 'name'.");
						goto err;
					}
					if (strlen(name) > CPFL_JS_STR_SIZE - 1) {
						PMD_DRV_LOG(ERR, "The 'name' is too long.");
						goto err;
					}
					strncpy(js_mr_key->actions[i].prog.params[j].name, name,
						CPFL_JS_STR_SIZE - 1);
				}
				ret = cpfl_json_t_to_uint16(subobject, "size", &value);
				if (ret < 0) {
					PMD_DRV_LOG(ERR, "Can not parse 'size'.");
					goto err;
				}
				js_mr_key->actions[i].prog.params[j].size = value;
			}

		} else if (js_mr_key->actions[i].type != RTE_FLOW_ACTION_TYPE_VXLAN_DECAP) {
			PMD_DRV_LOG(ERR, "not support this type: %d.", js_mr_key->actions[i].type);
			goto err;
		}
	}

	return 0;

err:
	rte_free(js_mr_key->actions);
	return -EINVAL;
}

static int
cpfl_flow_js_mr_layout(json_t *ob_layouts, struct cpfl_flow_js_mr_action_mod *js_mod)
{
	int len, i;

	len = json_array_size(ob_layouts);
	js_mod->layout_size = len;
	if (len == 0)
		return 0;

	for (i = 0; i < len; i++) {
		json_t *object;
		int index = 0, size = 0, offset = 0;
		int ret;
		const char *hint;

		object = json_array_get(ob_layouts, i);
		ret = cpfl_json_t_to_int(object, "index", &index);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Can not parse 'index'.");
			goto err;
		}
		js_mod->layout[i].index = index;
		ret = cpfl_json_t_to_int(object, "size", &size);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Can not parse 'size'.");
			goto err;
		}
		js_mod->layout[i].size = size;
		ret = cpfl_json_t_to_int(object, "offset", &offset);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Can not parse 'offset'.");
			goto err;
		}
		js_mod->layout[i].offset = offset;
		hint = cpfl_json_t_to_string(object, "hint");
		if (!hint) {
			PMD_DRV_LOG(ERR, "Can not parse string 'hint'.");
			goto err;
		}
		if (strlen(hint) > CPFL_JS_STR_SIZE - 1) {
			PMD_DRV_LOG(ERR, "The 'hint' is too long.");
			goto err;
		}
		strncpy(js_mod->layout[i].hint, hint, CPFL_JS_STR_SIZE - 1);
	}

	return 0;

err:
	rte_free(js_mod->layout);
	return -EINVAL;
}

static int
cpfl_flow_js_mr_content(json_t *ob_content, struct cpfl_flow_js_mr_action_mod *js_mod)
{
	int ret, len, i;
	json_t *ob_field;

	if (!ob_content)
		return 0;

	js_mod->is_content = TRUE;
	ret = cpfl_json_t_to_uint16(ob_content, "size", &js_mod->content.size);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Can not parse 'size'.");
		return -EINVAL;
	}

	ob_field = json_object_get(ob_content, "fields");
	len = json_array_size(ob_field);
	js_mod->content.field_size = len;
	if (len == 0)
		return 0;

	for (i = 0; i < len; i++) {
		json_t *object;
		uint16_t start = 0, width = 0, index = 0;
		const char *type;

		object = json_array_get(ob_field, i);
		type = cpfl_json_t_to_string(object, "type");
		if (!type) {
			PMD_DRV_LOG(ERR, "Can not parse string 'type'.");
			goto err;
		}
		if (strlen(type) > CPFL_JS_STR_SIZE - 1) {
			PMD_DRV_LOG(ERR, "The 'type' is too long.");
			goto err;
		}
		strncpy(js_mod->content.fields[i].type, type, CPFL_JS_STR_SIZE - 1);
		ret = cpfl_json_t_to_uint16(object, "start", &start);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Can not parse 'start'.");
			goto err;
		}
		js_mod->content.fields[i].start = start;
		ret = cpfl_json_t_to_uint16(object, "width", &width);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Can not parse 'width'.");
			goto err;
		}
		js_mod->content.fields[i].width = width;
		if (strcmp(type, "parameter") == 0) {
			ret = cpfl_json_t_to_uint16(object, "index", &index);
			if (ret < 0) {
				PMD_DRV_LOG(ERR, "Can not parse 'index'.");
				goto err;
			}
			js_mod->content.fields[i].index = index;
		} else if (strcmp(type, "constant") == 0) {
			json_t *ob_value, *subobj;
			int value_len, j;

			ob_value = json_object_get(object, "value");
			value_len = json_array_size(ob_value);
			for (j = 0; j < value_len; j++) {
				subobj = json_array_get(ob_value, j);
				js_mod->content.fields[i].value[j] =
				    (uint8_t)json_integer_value(subobj);
			}
		}
	}

	return 0;

err:
	return -EINVAL;
}

static int
cpfl_flow_js_mr_action(json_t *ob_mr_act, struct cpfl_flow_js_mr_action *js_mr_act)
{
	json_t *ob_data;
	const char *type;

	/* mr->action->type */
	type = cpfl_json_t_to_string(ob_mr_act, "type");
	if (!type) {
		PMD_DRV_LOG(ERR, "Can not parse string 'type'.");
		return -EINVAL;
	}
	/* mr->action->data */
	ob_data = json_object_get(ob_mr_act, "data");
	if (strcmp(type, "mod") == 0) {
		json_t *ob_layouts, *ob_content;
		uint16_t profile = 0;
		int ret;

		js_mr_act->type = CPFL_JS_MR_ACTION_TYPE_MOD;
		ret = cpfl_json_t_to_uint16(ob_data, "profile", &profile);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Can not parse 'profile'.");
			return -EINVAL;
		}
		js_mr_act->mod.prof = profile;
		ob_layouts = json_object_get(ob_data, "layout");
		ret = cpfl_flow_js_mr_layout(ob_layouts, &js_mr_act->mod);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Can not parse layout.");
			return ret;
		}
		ob_content = json_object_get(ob_data, "content");
		ret = cpfl_flow_js_mr_content(ob_content, &js_mr_act->mod);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Can not parse 'content'.");
			return ret;
		}
	} else  {
		PMD_DRV_LOG(ERR, "not support this type: %s.", type);
		return -EINVAL;
	}

	return 0;
}

/**
 * The modifications object array defines a set of rules for the PMD to match rte_flow
 * modification actions and translate them into the Modification profile. This object
 * is optional.
 */
static int
cpfl_flow_js_mod_rule(json_t *ob_root, struct cpfl_flow_js_parser *parser)
{
	json_t *ob_mrs;
	int i, len;

	ob_mrs = json_object_get(ob_root, "modifications");
	if (!ob_mrs) {
		PMD_DRV_LOG(INFO, "The modifications is optional.");
		return 0;
	}
	len = json_array_size(ob_mrs);
	if (len == 0)
		return 0;
	parser->mr_size = len;
	parser->modifications = rte_malloc(NULL, sizeof(struct cpfl_flow_js_mr) * len, 0);
	if (!parser->modifications) {
		PMD_DRV_LOG(ERR, "Failed to alloc memory.");
		return -ENOMEM;
	}
	for (i = 0; i < len; i++) {
		int ret;
		json_t *object, *ob_mr_key, *ob_mr_action, *ob_mr_key_action;

		object = json_array_get(ob_mrs, i);
		/* mr->key */
		ob_mr_key = json_object_get(object, "key");
		/* mr->key->actions */
		ob_mr_key_action = json_object_get(ob_mr_key, "actions");
		ret = cpfl_flow_js_mr_key(ob_mr_key_action, &parser->modifications[i].key);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "parse mr_key failed.");
			goto err;
		}
		/* mr->action */
		ob_mr_action = json_object_get(object, "action");
		ret = cpfl_flow_js_mr_action(ob_mr_action, &parser->modifications[i].action);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "parse mr_action failed.");
			goto err;
		}
	}

	return 0;

err:
	rte_free(parser->modifications);
	return -EINVAL;
}

static int
cpfl_parser_init(json_t *ob_root, struct cpfl_flow_js_parser *parser)
{
	int ret = 0;

	ret = cpfl_flow_js_pattern_rule(ob_root, parser);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "parse pattern_rule failed.");
		return ret;
	}
	ret = cpfl_flow_js_mod_rule(ob_root, parser);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "parse mod_rule failed.");
		return ret;
	}

	return 0;
}

int
cpfl_parser_create(struct cpfl_flow_js_parser **flow_parser, const char *filename)
{
	struct cpfl_flow_js_parser *parser;
	json_error_t json_error;
	json_t *root;
	int ret;

	parser = rte_zmalloc("flow_parser", sizeof(struct cpfl_flow_js_parser), 0);
	if (!parser) {
		PMD_DRV_LOG(ERR, "Not enough memory to create flow parser.");
		return -ENOMEM;
	}
	root = json_load_file(filename, 0, &json_error);
	if (!root) {
		PMD_DRV_LOG(ERR, "Bad JSON file \"%s\": %s", filename, json_error.text);
		goto free_parser;
	}
	ret = cpfl_parser_init(root, parser);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "parser init failed.");
		goto free_parser;
	}
	*flow_parser = parser;
	json_decref(root);

	return 0;
free_parser:
	rte_free(parser);
	return -EINVAL;
}

static void
cpfl_parser_free_pr_action(struct cpfl_flow_js_pr_action *pr_act)
{
	if (pr_act->type == CPFL_JS_PR_ACTION_TYPE_SEM)
		rte_free(pr_act->sem.fv);
}

int
cpfl_parser_destroy(struct cpfl_flow_js_parser *parser)
{
	int i, j;

	if (!parser)
		return 0;

	for (i = 0; i < parser->pr_size; i++) {
		struct cpfl_flow_js_pr *pattern = &parser->patterns[i];

		for (j = 0; j < pattern->key.proto_size; j++)
			rte_free(pattern->key.protocols[j].fields);
		rte_free(pattern->key.protocols);
		rte_free(pattern->key.attributes);

		for (j = 0; j < pattern->actions_size; j++) {
			struct cpfl_flow_js_pr_action *pr_act;

			pr_act = &pattern->actions[j];
			cpfl_parser_free_pr_action(pr_act);
		}
		rte_free(pattern->actions);
	}
	rte_free(parser->patterns);
	for (i = 0; i < parser->mr_size; i++) {
		struct cpfl_flow_js_mr *mr = &parser->modifications[i];

		rte_free(mr->key.actions);
	}
	rte_free(parser->modifications);
	rte_free(parser);

	return 0;
}

static int
cpfl_get_items_length(const struct rte_flow_item *items)
{
	int length = 0;
	const struct rte_flow_item *item = items;

	while ((item + length++)->type != RTE_FLOW_ITEM_TYPE_END)
		continue;
	return length;
}

static int
cpfl_get_actions_length(const struct rte_flow_action *actions)
{
	int length = 0;
	const struct rte_flow_action *action = actions;

	while ((action + length++)->type != RTE_FLOW_ACTION_TYPE_END)
		continue;
	return length;
}

static int
cpfl_parse_fv_protocol(struct cpfl_flow_js_fv *js_fv, const struct rte_flow_item *items,
		       uint16_t offset, uint8_t *fv)
{
	uint16_t v_layer, v_offset, v_mask;
	enum rte_flow_item_type v_header;
	int j, layer, length;
	uint16_t temp_fv;

	length = cpfl_get_items_length(items);
	v_layer = js_fv->proto.layer;
	v_header = js_fv->proto.header;
	v_offset = js_fv->proto.offset;
	v_mask = js_fv->proto.mask;
	layer = 0;
	for (j = 0; j < length - 1; j++) {
		if (items[j].type == v_header) {
			if (layer == v_layer) {
				/* copy out 16 bits from offset */
				const uint8_t *pointer;

				pointer = &(((const uint8_t *)(items[j].spec))[v_offset]);
				temp_fv = ntohs((*((const uint16_t *)pointer)) & v_mask);
				fv[2 * offset] = (uint8_t)(temp_fv >> 8);
				fv[2 * offset + 1] = (uint8_t)(temp_fv & 0x00ff);
				break;
			}
			layer++;
		}
	}

	return 0;
}

static int
cpfl_parse_fieldvectors(struct cpfl_itf *itf, struct cpfl_flow_js_fv *js_fvs, int size,
			uint8_t *fv, const struct rte_flow_item *items)
{
	int i, ret;

	for (i = 0; i < size; i++) {
		uint16_t offset, temp_fv, value_int;
		enum cpfl_flow_js_fv_type type;
		struct cpfl_flow_js_fv *js_fv;

		js_fv = &js_fvs[i];
		offset = js_fv->offset;
		type = js_fv->type;
		if (type == CPFL_FV_TYPE_IMMEDIATE) {
			value_int = js_fv->immediate;
			temp_fv = (value_int << 8) & 0xff00;
			fv[2 * offset] = (uint8_t)(temp_fv >> 8);
			fv[2 * offset + 1] = (uint8_t)(temp_fv & 0x00ff);
		} else if (type == CPFL_FV_TYPE_METADATA) {
			uint16_t type, v_offset, mask;

			type = js_fv->meta.type;
			v_offset = js_fv->meta.offset;
			mask = js_fv->meta.mask;
			temp_fv = cpfl_metadata_read16(&itf->adapter->meta, type, v_offset) & mask;
			fv[2 * offset] = (uint8_t)(temp_fv & 0x00ff);
			fv[2 * offset + 1] = (uint8_t)(temp_fv >> 8);
		} else if (type == CPFL_FV_TYPE_PROTOCOL) {
			ret = cpfl_parse_fv_protocol(js_fv, items, offset, fv);
			if (ret)
				return ret;
		} else {
			PMD_DRV_LOG(DEBUG, "not support this type: %d.", type);
			return -EINVAL;
		}
	}

	return 0;
}

static int
cpfl_parse_pr_actions(struct cpfl_itf *itf,
		      struct cpfl_flow_js_pr_action *actions,
		      int size,
		      const struct rte_flow_item *items,
		      const struct rte_flow_attr *attr,
		      struct cpfl_flow_pr_action *pr_action)
{
	int i, ret;

	for (i = 0; i < size; i++) {
		struct cpfl_flow_js_pr_action *pr_act;
		enum cpfl_flow_pr_action_type type;

		pr_act = &actions[i];
		/* pr->actions->type */
		type = pr_act->type;
		/* pr->actions->data */
		if (attr->group == 1  && type == CPFL_JS_PR_ACTION_TYPE_SEM) {
			struct cpfl_flow_js_pr_action_sem *sem = &pr_act->sem;

			pr_action->type = CPFL_JS_PR_ACTION_TYPE_SEM;
			pr_action->sem.prof = sem->prof;
			pr_action->sem.subprof = sem->subprof;
			pr_action->sem.keysize = sem->keysize;
			memset(pr_action->sem.cpfl_flow_pr_fv, 0,
			       sizeof(pr_action->sem.cpfl_flow_pr_fv));
			ret = cpfl_parse_fieldvectors(itf, sem->fv, sem->fv_size,
						      pr_action->sem.cpfl_flow_pr_fv, items);
			return ret;
		} else if (attr->group > 4 || attr->group == 0) {
			return -EPERM;
		}
	}

	return 0;
}

static int
cpfl_check_eth_mask(const char *mask, struct rte_ether_addr addr)
{
	int i, ret;
	struct rte_ether_addr mask_bytes;

	ret = rte_ether_unformat_addr(mask, &mask_bytes);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "translate mac address from string to rte_ether_addr failed.");
		return -EINVAL;
	}
	/* validate eth mask addr if match */
	for (i = 0; i < RTE_ETHER_ADDR_LEN; i++) {
		if (mask_bytes.addr_bytes[i] != addr.addr_bytes[i])
			return -EINVAL;
	}

	return 0;
}

static int
cpfl_check_ipv4_mask(const char *mask, rte_be32_t addr)
{
	uint32_t out_addr;

	/* 0: success; -EINVAL: invalid; -ENOTSUP: fail */
	int ret = inet_pton(AF_INET, mask, &out_addr);

	if (ret < 0)
		return -EINVAL;
	/* validate ipv4 mask addr if match */
	if (out_addr != addr)
		return -EINVAL;

	return 0;
}

static int
cpfl_check_eth(struct cpfl_flow_js_pr_key_proto *proto, const struct rte_flow_item_eth *eth_mask)
{
	int field_size, j;
	int flag_dst_addr, flag_src_addr, flag_ether_type;
	struct cpfl_flow_js_pr_key_proto_field *field;

	if (!proto)
		return 0;
	field_size = proto->fields_size;
	if (field_size != 0 && !eth_mask)
		return -EINVAL;

	if (field_size == 0 && eth_mask)
		return -EINVAL;

	if (field_size == 0 && !eth_mask)
		return 0;

	flag_dst_addr = false;
	flag_src_addr = false;
	flag_ether_type = false;
	for (j = 0; j < field_size; j++) {
		const char *name, *s_mask;

		field = &proto->fields[j];
		/* match: rte_flow_item_eth.dst, more see Field Mapping
		 */
		name = field->name;
		/* match: rte_flow_item->mask */
		if (strcmp(name, "src_addr") == 0) {
			s_mask = field->mask;
			if (cpfl_check_eth_mask(s_mask, eth_mask->src) < 0)
				return -EINVAL;
			flag_src_addr = true;
		} else if (strcmp(name, "dst_addr") == 0) {
			s_mask = field->mask;
			if (cpfl_check_eth_mask(s_mask, eth_mask->dst) < 0)
				return -EINVAL;
			flag_dst_addr = true;
		} else if (strcmp(name, "ether_type") == 0) {
			uint16_t mask = (uint16_t)field->mask_32b;

			if (mask != eth_mask->type)
				return -EINVAL;
			flag_ether_type = true;
		} else {
			/* TODO: more type... */
			PMD_DRV_LOG(ERR, "not support this name.");
			return -EINVAL;
		}
	}
	if (!flag_src_addr) {
		if (strcmp((const char *)eth_mask->src.addr_bytes, "\x00\x00\x00\x00\x00\x00") != 0)
			return -EINVAL;
	}
	if (!flag_dst_addr) {
		if (strcmp((const char *)eth_mask->dst.addr_bytes, "\x00\x00\x00\x00\x00\x00") != 0)
			return -EINVAL;
	}
	if (!flag_ether_type) {
		if (eth_mask->hdr.ether_type != (rte_be16_t)0)
			return -EINVAL;
	}

	return 0;
}

static int
cpfl_check_ipv4(struct cpfl_flow_js_pr_key_proto *proto, const struct rte_flow_item_ipv4 *ipv4_mask)
{
	int field_size, j;
	int flag_next_proto_id, flag_src_addr, flag_dst_addr;
	struct cpfl_flow_js_pr_key_proto_field *field;

	if (!proto)
		return 0;

	field_size = proto->fields_size;
	if (field_size != 0 && !ipv4_mask)
		return -EINVAL;

	if (field_size == 0 && ipv4_mask)
		return -EINVAL;

	if (field_size == 0 && !ipv4_mask)
		return 0;

	flag_dst_addr = false;
	flag_src_addr = false;
	flag_next_proto_id = false;
	for (j = 0; j < field_size; j++) {
		const char *name;

		field = &proto->fields[j];
		name = field->name;
		if (strcmp(name, "src_addr") == 0) {
			const char *mask;

			mask = field->mask;
			if (cpfl_check_ipv4_mask(mask, ipv4_mask->hdr.src_addr) < 0)
				return -EINVAL;
			flag_src_addr = true;
		} else if (strcmp(name, "dst_addr") == 0) {
			const char *mask;

			mask = field->mask;
			if (cpfl_check_ipv4_mask(mask, ipv4_mask->hdr.dst_addr) < 0)
				return -EINVAL;
			flag_dst_addr = true;
		} else if (strcmp(name, "next_proto_id") == 0) {
			uint8_t mask;

			mask = (uint8_t)field->mask_32b;
			if (mask != ipv4_mask->hdr.next_proto_id)
				return -EINVAL;
			flag_next_proto_id = true;
		} else {
			PMD_DRV_LOG(ERR, "not support this name.");
			return -EINVAL;
		}
	}
	if (!flag_src_addr) {
		if (ipv4_mask->hdr.src_addr != (rte_be32_t)0)
			return -EINVAL;
	}
	if (!flag_dst_addr) {
		if (ipv4_mask->hdr.dst_addr != (rte_be32_t)0)
			return -EINVAL;
	}
	if (!flag_next_proto_id) {
		if (ipv4_mask->hdr.next_proto_id != (uint8_t)0)
			return -EINVAL;
	}

	return 0;
}

static int
cpfl_check_tcp(struct cpfl_flow_js_pr_key_proto *proto, const struct rte_flow_item_tcp *tcp_mask)
{
	int field_size, j;
	int flag_src_port, flag_dst_port;
	struct cpfl_flow_js_pr_key_proto_field *field;

	if (!proto)
		return 0;

	field_size = proto->fields_size;
	if (field_size != 0 && !tcp_mask)
		return -EINVAL;

	if (field_size == 0 && tcp_mask)
		return -EINVAL;

	if (field_size == 0 && !tcp_mask)
		return 0;

	flag_src_port = false;
	flag_dst_port = false;
	for (j = 0; j < field_size; j++) {
		const char *name;
		uint16_t mask;

		field = &proto->fields[j];
		name = field->name;
		mask = (uint16_t)field->mask_32b;
		if (strcmp(name, "src_port") == 0) {
			if (tcp_mask->hdr.src_port != mask)
				return -EINVAL;
			flag_src_port = true;
		} else if (strcmp(name, "dst_port") == 0) {
			if (tcp_mask->hdr.dst_port != mask)
				return -EINVAL;
			flag_dst_port = true;
		} else {
			PMD_DRV_LOG(ERR, "not support this name.");
			return -EINVAL;
		}
	}
	if (!flag_src_port) {
		if (tcp_mask->hdr.src_port != (rte_be16_t)0)
			return -EINVAL;
	}
	if (!flag_dst_port) {
		if (tcp_mask->hdr.dst_port != (rte_be16_t)0)
			return -EINVAL;
	}

	return 0;
}

static int
cpfl_check_udp(struct cpfl_flow_js_pr_key_proto *proto, const struct rte_flow_item_udp *udp_mask)
{
	int field_size, j;
	bool flag_src_port, flag_dst_port;
	struct cpfl_flow_js_pr_key_proto_field *field;

	if (!proto)
		return 0;
	field_size = proto->fields_size;
	if (field_size != 0 && !udp_mask)
		return -EINVAL;
	if (field_size == 0 && udp_mask)
		return -EINVAL;
	if (field_size == 0 && !udp_mask)
		return 0;
	flag_src_port = false;
	flag_dst_port = false;
	for (j = 0; j < field_size; j++) {
		const char *name;
		uint16_t mask;

		field = &proto->fields[j];
		/* match: rte_flow_item_eth.dst */
		name = field->name; /* match: rte_flow_item->mask */
		mask = (uint16_t)field->mask_32b;
		if (strcmp(name, "src_port") == 0) {
			if (udp_mask->hdr.src_port != mask)
				return -EINVAL;
			flag_src_port = true;
		} else if (strcmp(name, "dst_port") == 0) {
			if (udp_mask->hdr.dst_port != mask)
				return -EINVAL;
			flag_dst_port = true;
		} else {
			PMD_DRV_LOG(ERR, "not support this name: %s.", name);
			return -EINVAL;
		}
	}
	if (!flag_src_port) {
		if (udp_mask->hdr.src_port != (rte_be16_t)0)
			return -EINVAL;
	}
	if (!flag_dst_port) {
		if (udp_mask->hdr.dst_port != (rte_be16_t)0)
			return -EINVAL;
	}

	return 0;
}

static int
cpfl_check_vxlan(struct cpfl_flow_js_pr_key_proto *proto,
		 const struct rte_flow_item_vxlan *vxlan_mask)
{
	int field_size, j;
	struct cpfl_flow_js_pr_key_proto_field *field;

	if (!proto)
		return 0;
	field_size = proto->fields_size;
	if (field_size != 0 && !vxlan_mask)
		return -EINVAL;
	if (field_size == 0 && vxlan_mask)
		return -EINVAL;
	if (field_size == 0 && !vxlan_mask)
		return 0;
	for (j = 0; j < field_size; j++) {
		const char *name;
		int64_t mask;

		field = &proto->fields[j];
		name = field->name;
		/* match: rte_flow_item->mask */
		mask = (int64_t)field->mask_32b;
		if (strcmp(name, "vx_vni") == 0) {
			if ((int64_t)RTE_BE32(vxlan_mask->hdr.vx_vni) != mask)
				return -EINVAL;
		} else {
			PMD_DRV_LOG(ERR, "not support this name.");
			return -EINVAL;
		}
	}

	return 0;
}

static int
cpfl_check_icmp(struct cpfl_flow_js_pr_key_proto *proto, const struct rte_flow_item_icmp *icmp_mask)
{
	int field_size;

	if (!proto)
		return 0;
	field_size = proto->fields_size;
	if ((field_size != 0 && !icmp_mask) || (field_size == 0 && icmp_mask))
		return -EINVAL;

	return 0;
}

static int
cpfl_check_pattern_key_proto(struct cpfl_flow_js_pr_key_proto *protocols,
			     int proto_size,
			     const struct rte_flow_item *items)
{
	int i, length;
	int j = 0;

	length = cpfl_get_items_length(items);
	if (proto_size > length - 1)
		return -EINVAL;
	for (i = 0; i < proto_size; i++) {
		struct cpfl_flow_js_pr_key_proto *key_proto;
		enum rte_flow_item_type type;

		key_proto = &protocols[i];
		/* pr->key->proto->type */
		type = key_proto->type;
		/* pr->key->proto->fields */
		switch (type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			if (items[j++].type == RTE_FLOW_ITEM_TYPE_ETH) {
				const struct rte_flow_item_eth *eth_mask;
				int ret;

				eth_mask = (const struct rte_flow_item_eth *)items[i].mask;
				ret = cpfl_check_eth(key_proto, eth_mask);
				if (ret < 0)
					return ret;
			} else {
				return -EINVAL;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			if (items[j++].type == RTE_FLOW_ITEM_TYPE_IPV4) {
				const struct rte_flow_item_ipv4 *ipv4_mask;
				int ret;

				ipv4_mask = (const struct rte_flow_item_ipv4 *)items[i].mask;
				ret = cpfl_check_ipv4(key_proto, ipv4_mask);
				if (ret < 0)
					return ret;
			} else {
				return -EINVAL;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			if (items[j++].type == RTE_FLOW_ITEM_TYPE_TCP) {
				const struct rte_flow_item_tcp *tcp_mask;
				int ret;

				tcp_mask = (const struct rte_flow_item_tcp *)items[i].mask;
				ret = cpfl_check_tcp(key_proto, tcp_mask);
				if (ret < 0)
					return ret;
			} else {
				return -EINVAL;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			if (items[j++].type == RTE_FLOW_ITEM_TYPE_UDP) {
				const struct rte_flow_item_udp *udp_mask;
				int ret;

				udp_mask = (const struct rte_flow_item_udp *)items[i].mask;
				ret = cpfl_check_udp(key_proto, udp_mask);
				if (ret < 0)
					return ret;
			} else {
				return -EINVAL;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			if (items[j++].type == RTE_FLOW_ITEM_TYPE_VXLAN) {
				const struct rte_flow_item_vxlan *vxlan_mask;
				int ret;

				vxlan_mask = (const struct rte_flow_item_vxlan *)items[i].mask;
				ret = cpfl_check_vxlan(key_proto, vxlan_mask);
				if (ret < 0)
					return ret;
			} else {
				return -EINVAL;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_ICMP:
			if (items[j++].type == RTE_FLOW_ITEM_TYPE_ICMP) {
				const struct rte_flow_item_icmp *icmp_mask;
				int ret;

				icmp_mask = (const struct rte_flow_item_icmp *)items[i].mask;
				ret = cpfl_check_icmp(key_proto, icmp_mask);
				if (ret < 0)
					return ret;
			} else {
				return -EINVAL;
			}
			break;
		default:
			PMD_DRV_LOG(ERR, "Not support this type: %d.", type);
			return -EPERM;
		}
	}
	if (items[j].type != RTE_FLOW_ITEM_TYPE_END)
		return -EINVAL;

	return 0;
}

static int
cpfl_check_pattern_key_attr(struct cpfl_flow_js_pr_key_attr *key_attr,
			    const struct rte_flow_attr *attr)
{
	if (key_attr->ingress != attr->ingress) {
		PMD_DRV_LOG(DEBUG, "ingress not match.");
		return -EINVAL;
	}
	if (key_attr->egress != attr->egress) {
		PMD_DRV_LOG(DEBUG, "egress not match.");
		return -EINVAL;
	}

	return 0;
}

static int
cpfl_check_pattern_key(struct cpfl_flow_js_pr *pattern,
		       const struct rte_flow_item *items,
		       const struct rte_flow_attr *attr)
{
	int ret;

	/* pr->key */
	/* pr->key->protocols */
	ret = cpfl_check_pattern_key_proto(pattern->key.protocols,
					   pattern->key.proto_size, items);
	if (ret < 0)
		return -EINVAL;
	/* pr->key->attributes */
	ret = cpfl_check_pattern_key_attr(pattern->key.attributes, attr);
	if (ret < 0)
		return -EINVAL;

	return 0;
}

/* output: struct cpfl_flow_pr_action* pr_action */
int
cpfl_flow_parse_items(struct cpfl_itf *itf,
		      struct cpfl_flow_js_parser *parser,
		      const struct rte_flow_item *items,
		      const struct rte_flow_attr *attr,
		      struct cpfl_flow_pr_action *pr_action)
{
	int i, size;
	struct cpfl_flow_js_pr *pattern;

	size = parser->pr_size;
	for (i = 0; i < size; i++) {
		int ret;

		pattern = &parser->patterns[i];
		ret = cpfl_check_pattern_key(pattern, items, attr);
		if (ret < 0)
			continue;
		/* pr->actions */
		ret = cpfl_parse_pr_actions(itf, pattern->actions, pattern->actions_size,
					    items, attr, pr_action);
		return ret;
	}

	return -EINVAL;
}

/* modifications rules */
static int
cpfl_check_actions_vxlan_encap(struct cpfl_flow_mr_key_action_vxlan_encap *encap,
			       const struct rte_flow_action *action)
{
	const struct rte_flow_action_vxlan_encap *action_vxlan_encap;
	struct rte_flow_item *definition;
	int def_length, i, proto_size;

	action_vxlan_encap = (const struct rte_flow_action_vxlan_encap *)action->conf;
	definition = action_vxlan_encap->definition;
	def_length = cpfl_get_items_length(definition);
	proto_size = encap->proto_size;
	if (proto_size != def_length - 1) {
		PMD_DRV_LOG(DEBUG, "protocols not match.");
		return -EINVAL;
	}
	for (i = 0; i < proto_size; i++) {
		enum rte_flow_item_type proto;

		proto = encap->protocols[i];
		if (proto == RTE_FLOW_ITEM_TYPE_VLAN) {
			if (definition[i].type != RTE_FLOW_ITEM_TYPE_VOID) {
				PMD_DRV_LOG(DEBUG, "protocols not match.");
				return -EINVAL;
			}
		} else if (proto != definition[i].type) {
			PMD_DRV_LOG(DEBUG, "protocols not match.");
			return -EINVAL;
		}
	}

	return 0;
}

static int
cpfl_parse_check_prog_action(struct cpfl_flow_js_mr_key_action *key_act,
			     struct cpfl_flow_mr_key_action_prog *mr_key_prog,
			     const struct rte_flow_action_prog *prog)
{
	uint32_t k;
	bool check_name;

	check_name = key_act->prog.has_name ? strcmp(prog->name, key_act->prog.name) == 0
					    : atol(prog->name) == key_act->prog.id;
	if (!check_name) {
		PMD_DRV_LOG(ERR, "Not support this prog type: %s.", prog->name);
		return -EINVAL;
	}
	if (key_act->prog.param_size != prog->args_num)
		return -EINVAL;
	for (k = 0; k < key_act->prog.param_size; k++) {
		const struct rte_flow_action_prog_argument *arg = &prog->args[k];
		struct cpfl_flow_js_prog_parameter *param = &key_act->prog.params[k];

		check_name = param->has_name ? strcmp(arg->name, param->name) == 0
					     : atoi(arg->name) == param->index;
		if (!check_name || arg->size != param->size)
			return -EINVAL;
		if (param->has_name) {
			mr_key_prog->has_name = TRUE;
			strncpy(mr_key_prog->name[param->index], param->name,
				CPFL_JS_STR_SIZE - 1);
		}
	}

	return 0;
}

/* check and parse */
static int
cpfl_parse_mr_key_action(struct cpfl_flow_js_mr_key_action *key_acts, int size,
			 const struct rte_flow_action *actions,
			 struct cpfl_flow_mr_key_action *mr_key_action)
{
	int actions_length, i;
	int j = 0;
	int ret;

	actions_length = cpfl_get_actions_length(actions);
	if (size > actions_length - 1)
		return -EINVAL;
	for (i = 0; i < size; i++) {
		enum rte_flow_action_type type;
		struct cpfl_flow_js_mr_key_action *key_act;

		key_act = &key_acts[i];
		/* mr->key->actions->type */
		type = key_act->type;
		/* mr->key->actions->data */
		if (type == RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP) {
			int proto_size, k;
			struct cpfl_flow_mr_key_action_vxlan_encap *encap;

			while (j < actions_length &&
			       actions[j].type != RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP) {
				j++;
			}
			if (j >= actions_length)
				return -EINVAL;
			mr_key_action->mods[i].type = RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP;
			mr_key_action->mods[i].encap.action = &actions[j];
			encap = &mr_key_action->mods[i].encap;

			proto_size = key_act->encap.proto_size;
			encap->proto_size = proto_size;
			for (k = 0; k < proto_size; k++) {
				enum rte_flow_item_type proto;

				proto = key_act->encap.protocols[k];
				encap->protocols[k] = proto;
			}
			ret = cpfl_check_actions_vxlan_encap(encap, &actions[j]);
			if (ret < 0)
				return -EINVAL;
			j++;
		} else if (type == RTE_FLOW_ACTION_TYPE_VXLAN_DECAP) {
			while (j < actions_length &&
			       actions[j].type != RTE_FLOW_ACTION_TYPE_VXLAN_DECAP) {
				j++;
			}
			if (j >= actions_length)
				return -EINVAL;
			mr_key_action->mods[i].type = RTE_FLOW_ACTION_TYPE_VXLAN_DECAP;
			j++;
		} else if (type == RTE_FLOW_ACTION_TYPE_PROG) {
			const struct rte_flow_action_prog *prog;

			while (j < actions_length &&
			       actions[j].type != RTE_FLOW_ACTION_TYPE_PROG) {
				j++;
			}
			if (j >= actions_length)
				return -EINVAL;
			prog = actions[j].conf;
			mr_key_action->prog.prog = prog;
			ret = cpfl_parse_check_prog_action(key_act, &mr_key_action->prog, prog);
			if (ret < 0)
				return -EINVAL;
		} else {
			PMD_DRV_LOG(ERR, "Not support this type: %d.", type);
			return -EPERM;
		}
	}

	return 0;
}

/* output: uint8_t *buffer, uint16_t *byte_len */
static int
cpfl_parse_layout(struct cpfl_flow_js_mr_layout *layouts, int layout_size,
		  struct cpfl_flow_mr_key_mod *mods,
		  uint8_t *buffer, uint16_t *byte_len)
{
	int i;
	int start = 0;

	for (i = 0; i < layout_size; i++) {
		int index, size, offset;
		const char *hint;
		const uint8_t *addr = NULL;
		struct cpfl_flow_mr_key_mod *temp;
		struct cpfl_flow_js_mr_layout *layout;

		layout = &layouts[i];
		/* index links to the element of the actions array. */
		index = layout->index;
		size = layout->size;
		offset = layout->offset;
		if (index == -1) {
			hint = "dummpy";
			start += size;
			continue;
		}
		hint = layout->hint;
		temp = mods + index;
		if (temp->type == RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP) {
			const struct rte_flow_action_vxlan_encap *action_vxlan_encap;
			struct rte_flow_item *definition;
			int def_length, k;

			action_vxlan_encap =
			    (const struct rte_flow_action_vxlan_encap *)temp->encap.action->conf;
			definition = action_vxlan_encap->definition;
			def_length = cpfl_get_items_length(definition);
			for (k = 0; k < def_length - 1; k++) {
				if ((strcmp(hint, "eth") == 0 &&
				     definition[k].type == RTE_FLOW_ITEM_TYPE_ETH) ||
				    (strcmp(hint, "ipv4") == 0 &&
				     definition[k].type == RTE_FLOW_ITEM_TYPE_IPV4) ||
				    (strcmp(hint, "udp") == 0 &&
				     definition[k].type == RTE_FLOW_ITEM_TYPE_UDP) ||
				    (strcmp(hint, "tcp") == 0 &&
				     definition[k].type == RTE_FLOW_ITEM_TYPE_TCP) ||
				    (strcmp(hint, "vxlan") == 0 &&
				     definition[k].type == RTE_FLOW_ITEM_TYPE_VXLAN)) {
					addr = (const uint8_t *)(definition[k].spec);
					if (start > 255) {
						*byte_len = 0;
						PMD_DRV_LOG(ERR, "byte length is too long: %s",
							    hint);
						return -EINVAL;
					}
					memcpy(buffer + start, addr + offset, size);
					break;
				} /* TODO: more hint... */
			}
			if (k == def_length - 1) {
				*byte_len = 0;
				PMD_DRV_LOG(ERR, "can not find corresponding hint: %s", hint);
				return -EINVAL;
			}
		} else {
			*byte_len = 0;
			PMD_DRV_LOG(ERR, "Not support this type: %d.", temp->type);
			return -EINVAL;
		} /* else TODO: more type... */
		start += size;
	}
	*byte_len = start;

	return 0;
}

static int
cpfl_parse_content(struct cpfl_flow_js_mr_content *content,
		   struct cpfl_flow_mr_key_action_prog *prog, uint8_t *buffer)
{
	int i, j;

	for (i = 0; i < content->field_size; i++) {
		uint16_t start, width, shift_bit;

		start = content->fields[i].start / 8;
		width = (content->fields[i].width + 7) / 8;
		shift_bit = (8 - content->fields[i].start % 8 - content->fields[i].width % 8) % 8;

		for (j = 0; j < width; j++) {
			uint8_t old_value = 0;

			if (strcmp(content->fields[i].type, "parameter") == 0) {
				uint32_t k;
				uint16_t index = content->fields[i].index;
				const struct rte_flow_action_prog *act_prog = prog->prog;

				for (k = 0; k < act_prog->args_num; k++) {
					const char *name = act_prog->args[k].name;

					if ((prog->has_name &&
					     strcmp(prog->name[index], name) == 0) ||
					    (!prog->has_name && atoi(name) == index)) {
						old_value = act_prog->args[k].value[j];
						break;
					}
				}
				if (k == act_prog->args_num)
					return -EINVAL;
			} else if (strcmp(content->fields[i].type, "constant") == 0) {
				old_value = content->fields[i].value[j];
			} else {
				return -EINVAL;
			}
			memset(buffer + start + j, buffer[start + j] | old_value << shift_bit, 1);
		}
	}

	return 0;
}

static int
cpfl_parse_mr_action(struct cpfl_flow_js_mr_action *action,
		     struct cpfl_flow_mr_key_action *mr_key_action,
		     struct cpfl_flow_mr_action *mr_action)
{
	enum cpfl_flow_mr_action_type type;

	/* mr->action->type */
	type = action->type;
	/* mr->action->data */
	if (action->mod.is_content) {
		struct cpfl_flow_js_mr_content *content = &action->mod.content;

		mr_action->type = CPFL_JS_MR_ACTION_TYPE_MOD;
		mr_action->mod.byte_len = 0;
		mr_action->mod.prof = action->mod.prof;
		mr_action->mod.byte_len = content->size;
		memset(mr_action->mod.data, 0, sizeof(mr_action->mod.data));

		return cpfl_parse_content(content, &mr_key_action->prog, mr_action->mod.data);
	}

	if (type == CPFL_JS_MR_ACTION_TYPE_MOD) {
		struct cpfl_flow_js_mr_layout *layout;

		mr_action->type = CPFL_JS_MR_ACTION_TYPE_MOD;
		mr_action->mod.byte_len = 0;
		mr_action->mod.prof = action->mod.prof;
		layout = action->mod.layout;
		if (!layout)
			return 0;
		memset(mr_action->mod.data, 0, sizeof(mr_action->mod.data));

		return cpfl_parse_layout(layout, action->mod.layout_size, mr_key_action->mods,
					 mr_action->mod.data, &mr_action->mod.byte_len);
	}
	PMD_DRV_LOG(ERR, "Not support this type: %d.", type);

	return -EINVAL;
}

static int
cpfl_check_mod_key(struct cpfl_flow_js_mr *mr, const struct rte_flow_action *actions,
		   struct cpfl_flow_mr_key_action *mr_key_action)
{
	int key_action_size;

	/* mr->key->actions */
	key_action_size = mr->key.actions_size;
	return cpfl_parse_mr_key_action(mr->key.actions, key_action_size, actions, mr_key_action);
}

/* output: struct cpfl_flow_mr_action *mr_action */
static int
cpfl_parse_mod_rules(struct cpfl_flow_js_parser *parser, const struct rte_flow_action *actions,
		     struct cpfl_flow_mr_action *mr_action)
{
	int i;
	struct cpfl_flow_mr_key_action mr_key_action = {0};

	for (i = 0; i < parser->mr_size; i++) {
		int ret;
		struct cpfl_flow_js_mr *mr;

		mr = &parser->modifications[i];
		if (!mr)
			return -EINVAL;
		ret = cpfl_check_mod_key(mr, actions, &mr_key_action);
		if (ret < 0)
			continue;
		/* mr->action */
		return cpfl_parse_mr_action(&mr->action, &mr_key_action, mr_action);
	}

	return -EINVAL;
}

int
cpfl_flow_parse_actions(struct cpfl_flow_js_parser *parser, const struct rte_flow_action *actions,
			struct cpfl_flow_mr_action *mr_action)
{
	/* modifications rules */
	if (!parser->modifications) {
		PMD_DRV_LOG(INFO, "The modifications is optional.");
		return 0;
	}

	return cpfl_parse_mod_rules(parser, actions, mr_action);
}

bool
cpfl_metadata_write_port_id(struct cpfl_itf *itf)
{
	uint16_t dev_id;
	const int type = 0;
	const int offset = 5;

	dev_id = cpfl_get_port_id(itf);
	if (dev_id == CPFL_INVALID_HW_ID) {
		PMD_DRV_LOG(ERR, "fail to get hw ID\n");
		return false;
	}
	cpfl_metadata_write16(&itf->adapter->meta, type, offset, dev_id << 3);

	return true;
}

bool
cpfl_metadata_write_targetvsi(struct cpfl_itf *itf)
{
	uint16_t dev_id;
	const int type = 6;
	const int offset = 2;

	dev_id = cpfl_get_vsi_id(itf);
	if (dev_id == CPFL_INVALID_HW_ID) {
		PMD_DRV_LOG(ERR, "fail to get hw ID");
		return false;
	}
	cpfl_metadata_write16(&itf->adapter->meta, type, offset, dev_id << 1);

	return true;
}

bool
cpfl_metadata_write_sourcevsi(struct cpfl_itf *itf)
{
	uint16_t dev_id;
	const int type = 6;
	const int offset = 0;

	dev_id = cpfl_get_vsi_id(itf);
	if (dev_id == CPFL_INVALID_HW_ID) {
		PMD_DRV_LOG(ERR, "fail to get hw ID");
		return false;
	}
	cpfl_metadata_write16(&itf->adapter->meta, type, offset, dev_id);

	return true;
}

bool cpfl_metadata_write_vsi(struct cpfl_itf *itf)
{
	uint16_t dev_id;
	const int type = 0;
	const int offset = 24;

	dev_id = cpfl_get_vsi_id(itf);
	if (dev_id == CPFL_INVALID_HW_ID) {
		PMD_DRV_LOG(ERR, "fail to get hw ID");
		return false;
	}
	cpfl_metadata_write16(&itf->adapter->meta, type, offset, dev_id);

	return true;
}
