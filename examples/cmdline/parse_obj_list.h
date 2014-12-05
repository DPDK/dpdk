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
