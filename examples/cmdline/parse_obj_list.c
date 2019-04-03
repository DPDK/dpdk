/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <netinet/in.h>

#include <cmdline_parse.h>
#include <cmdline_parse_ipaddr.h>

#include <rte_string_fns.h>

#include "parse_obj_list.h"

/* This file is an example of extension of libcmdline. It provides an
 * example of objects stored in a list. */

struct cmdline_token_ops token_obj_list_ops = {
	.parse = parse_obj_list,
	.complete_get_nb = complete_get_nb_obj_list,
	.complete_get_elt = complete_get_elt_obj_list,
	.get_help = get_help_obj_list,
};

int
parse_obj_list(cmdline_parse_token_hdr_t *tk, const char *buf, void *res,
	unsigned ressize)
{
	struct token_obj_list *tk2 = (struct token_obj_list *)tk;
	struct token_obj_list_data *tkd = &tk2->obj_list_data;
	struct object *o;
	unsigned int token_len = 0;

	if (*buf == 0)
		return -1;

	if (res && ressize < sizeof(struct object *))
		return -1;

	while(!cmdline_isendoftoken(buf[token_len]))
		token_len++;

	SLIST_FOREACH(o, tkd->list, next) {
		if (token_len != strnlen(o->name, OBJ_NAME_LEN_MAX))
			continue;
		if (strncmp(buf, o->name, token_len))
			continue;
		break;
	}
	if (!o) /* not found */
		return -1;

	/* store the address of object in structure */
	if (res)
		*(struct object **)res = o;

	return token_len;
}

int complete_get_nb_obj_list(cmdline_parse_token_hdr_t *tk)
{
	struct token_obj_list *tk2 = (struct token_obj_list *)tk;
	struct token_obj_list_data *tkd = &tk2->obj_list_data;
	struct object *o;
	int ret = 0;

	SLIST_FOREACH(o, tkd->list, next) {
		ret ++;
	}
	return ret;
}

int complete_get_elt_obj_list(cmdline_parse_token_hdr_t *tk,
			      int idx, char *dstbuf, unsigned int size)
{
	struct token_obj_list *tk2 = (struct token_obj_list *)tk;
	struct token_obj_list_data *tkd = &tk2->obj_list_data;
	struct object *o;
	int i = 0;
	unsigned len;

	SLIST_FOREACH(o, tkd->list, next) {
		if (i++ == idx)
			break;
	}
	if (!o)
		return -1;

	len = strnlen(o->name, OBJ_NAME_LEN_MAX);
	if ((len + 1) > size)
		return -1;

	if (dstbuf)
		strlcpy(dstbuf, o->name, size);

	return 0;
}


int get_help_obj_list(__attribute__((unused)) cmdline_parse_token_hdr_t *tk,
		      char *dstbuf, unsigned int size)
{
	snprintf(dstbuf, size, "Obj-List");
	return 0;
}
