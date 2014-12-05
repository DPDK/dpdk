/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University of California, Berkeley nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
		snprintf(dstbuf, size, "%s", o->name);

	return 0;
}


int get_help_obj_list(__attribute__((unused)) cmdline_parse_token_hdr_t *tk,
		      char *dstbuf, unsigned int size)
{
	snprintf(dstbuf, size, "Obj-List");
	return 0;
}
