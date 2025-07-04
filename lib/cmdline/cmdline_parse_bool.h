/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 NVIDIA Corporation & Affiliates
 */

#ifndef _PARSE_BOOL_H_
#define _PARSE_BOOL_H_

#include <cmdline_parse.h>
#include <cmdline_parse_string.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cmdline_token_bool {
	struct cmdline_token_hdr hdr;
};

typedef struct cmdline_token_bool cmdline_parse_token_bool_t;
extern struct cmdline_token_ops cmdline_token_bool_ops;

int cmdline_parse_bool(cmdline_parse_token_hdr_t *tk,
	const char *srcbuf, void *res, unsigned int ressize);

#define TOKEN_BOOL_INITIALIZER(structure, field)			\
{                                                           \
	/* hdr */                                               \
	{                                                       \
		&cmdline_token_bool_ops,         /* ops */			\
		offsetof(structure, field),     /* offset */		\
	}                                                       \
}


#ifdef __cplusplus
}
#endif

#endif /* _PARSE_BOOL_H_ */
