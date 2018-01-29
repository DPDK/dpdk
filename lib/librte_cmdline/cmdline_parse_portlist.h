/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright (c) 2010, Keith Wiles <keith.wiles@windriver.com>
 * All rights reserved.
 */

#ifndef _PARSE_PORTLIST_H_
#define _PARSE_PORTLIST_H_

#include <stdint.h>
#include <cmdline_parse.h>

#ifdef __cplusplus
extern "C" {
#endif

/* size of a parsed string */
#define PORTLIST_TOKEN_SIZE	128
#define PORTLIST_MAX_TOKENS	32

typedef struct cmdline_portlist {
	uint32_t		map;
} cmdline_portlist_t;

struct cmdline_token_portlist {
	struct cmdline_token_hdr hdr;
};
typedef struct cmdline_token_portlist cmdline_parse_token_portlist_t;

extern struct cmdline_token_ops cmdline_token_portlist_ops;

int cmdline_parse_portlist(cmdline_parse_token_hdr_t *tk,
	const char *srcbuf, void *res, unsigned ressize);
int cmdline_get_help_portlist(cmdline_parse_token_hdr_t *tk,
	char *dstbuf, unsigned int size);

#define TOKEN_PORTLIST_INITIALIZER(structure, field)        \
{                                                           \
	/* hdr */                                               \
	{                                                       \
		&cmdline_token_portlist_ops,    /* ops */           \
		offsetof(structure, field),     /* offset */        \
	},                                                      \
}

#ifdef __cplusplus
}
#endif

#endif /* _PARSE_PORTLIST_H_ */
