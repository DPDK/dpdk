/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 */

#ifndef _PARSE_NUM_H_
#define _PARSE_NUM_H_

#include <cmdline_parse.h>

#ifdef __cplusplus
extern "C" {
#endif

enum cmdline_numtype {
	RTE_UINT8 = 0,
	RTE_UINT16,
	RTE_UINT32,
	RTE_UINT64,
	RTE_INT8,
	RTE_INT16,
	RTE_INT32,
	RTE_INT64
};

struct cmdline_token_num_data {
	enum cmdline_numtype type;
};

struct cmdline_token_num {
	struct cmdline_token_hdr hdr;
	struct cmdline_token_num_data num_data;
};
typedef struct cmdline_token_num cmdline_parse_token_num_t;

extern struct cmdline_token_ops cmdline_token_num_ops;

int cmdline_parse_num(cmdline_parse_token_hdr_t *tk,
	const char *srcbuf, void *res, unsigned ressize);
int cmdline_get_help_num(cmdline_parse_token_hdr_t *tk,
	char *dstbuf, unsigned int size);

#define TOKEN_NUM_INITIALIZER(structure, field, numtype)    \
{                                                           \
	/* hdr */                                               \
	{                                                       \
		&cmdline_token_num_ops,         /* ops */           \
		offsetof(structure, field),     /* offset */        \
	},                                                      \
	/* num_data */                                          \
	{                                                       \
		numtype,                        /* type */          \
	},                                                      \
}

#ifdef __cplusplus
}
#endif

#endif /* _PARSE_NUM_H_ */
