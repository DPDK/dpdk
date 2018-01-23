/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 */

#ifndef _PARSE_OBJ_LIST_H_
#define _PARSE_OBJ_LIST_H_

/* This file is an example of extension of libcmdline. It provides an
 * example of objects stored in a list. */

#include <sys/queue.h>
#include <cmdline_parse.h>

#define OBJ_NAME_LEN_MAX 64

struct object {
	SLIST_ENTRY(object) next;
	char name[OBJ_NAME_LEN_MAX];
	cmdline_ipaddr_t ip;
};

/* define struct object_list */
SLIST_HEAD(object_list, object);

/* data is a pointer to a list */
struct token_obj_list_data {
	struct object_list *list;
};

struct token_obj_list {
	struct cmdline_token_hdr hdr;
	struct token_obj_list_data obj_list_data;
};
typedef struct token_obj_list parse_token_obj_list_t;

extern struct cmdline_token_ops token_obj_list_ops;

int parse_obj_list(cmdline_parse_token_hdr_t *tk, const char *srcbuf, void *res,
	unsigned ressize);
int complete_get_nb_obj_list(cmdline_parse_token_hdr_t *tk);
int complete_get_elt_obj_list(cmdline_parse_token_hdr_t *tk, int idx,
			      char *dstbuf, unsigned int size);
int get_help_obj_list(cmdline_parse_token_hdr_t *tk, char *dstbuf, unsigned int size);

#define TOKEN_OBJ_LIST_INITIALIZER(structure, field, obj_list_ptr)  \
{								    \
	.hdr = {						    \
		.ops = &token_obj_list_ops,			    \
		.offset = offsetof(structure, field),		    \
	},							    \
		.obj_list_data = {				    \
		.list = obj_list_ptr,				    \
	},							    \
}

#endif /* _PARSE_OBJ_LIST_H_ */
