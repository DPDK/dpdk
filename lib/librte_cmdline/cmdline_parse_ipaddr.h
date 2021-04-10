/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 */

#ifndef _PARSE_IPADDR_H_
#define _PARSE_IPADDR_H_

#include <cmdline_parse.h>
#include <rte_ip.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CMDLINE_IPADDR_V4      0x01
#define CMDLINE_IPADDR_V6      0x02
#define CMDLINE_IPADDR_NETWORK 0x04

struct cmdline_ipaddr {
	uint8_t family;
	union {
		struct in_addr ipv4;
		struct in6_addr ipv6;
	} addr;
	unsigned int prefixlen; /* in case of network only */
};
typedef struct cmdline_ipaddr cmdline_ipaddr_t;

struct cmdline_token_ipaddr_data {
	uint8_t flags;
};

struct cmdline_token_ipaddr {
	struct cmdline_token_hdr hdr;
	struct cmdline_token_ipaddr_data ipaddr_data;
};
typedef struct cmdline_token_ipaddr cmdline_parse_token_ipaddr_t;

extern struct cmdline_token_ops cmdline_token_ipaddr_ops;

int cmdline_parse_ipaddr(cmdline_parse_token_hdr_t *tk, const char *srcbuf,
	void *res, unsigned ressize);
int cmdline_get_help_ipaddr(cmdline_parse_token_hdr_t *tk, char *dstbuf,
	unsigned int size);

#define TOKEN_IPADDR_INITIALIZER(structure, field)      \
{                                                       \
	/* hdr */                                           \
	{                                                   \
		&cmdline_token_ipaddr_ops,      /* ops */       \
		offsetof(structure, field),     /* offset */    \
	},                                                  \
	/* ipaddr_data */                                   \
	{                                                   \
		CMDLINE_IPADDR_V4 |             /* flags */     \
		CMDLINE_IPADDR_V6,                              \
	},                                                  \
}

#define TOKEN_IPV4_INITIALIZER(structure, field)        \
{                                                       \
	/* hdr */                                           \
	{                                                   \
		&cmdline_token_ipaddr_ops,      /* ops */       \
		offsetof(structure, field),     /* offset */    \
	},                                                  \
	/* ipaddr_data */                                   \
	{                                                   \
		CMDLINE_IPADDR_V4,              /* flags */     \
	},                                                  \
}

#define TOKEN_IPV6_INITIALIZER(structure, field)        \
{                                                       \
	/* hdr */                                           \
	{                                                   \
		&cmdline_token_ipaddr_ops,      /* ops */       \
		offsetof(structure, field),     /* offset */    \
	},                                                  \
	/* ipaddr_data */                                   \
	{                                                   \
		CMDLINE_IPADDR_V6,              /* flags */     \
	},                                                  \
}

#define TOKEN_IPNET_INITIALIZER(structure, field)       \
{                                                       \
	/* hdr */                                           \
	{                                                   \
		&cmdline_token_ipaddr_ops,      /* ops */       \
		offsetof(structure, field),     /* offset */    \
	},                                                  \
	/* ipaddr_data */                                   \
	{                                                   \
		CMDLINE_IPADDR_V4 |             /* flags */     \
		CMDLINE_IPADDR_V6 |                             \
		CMDLINE_IPADDR_NETWORK,                         \
	},                                                  \
}

#define TOKEN_IPV4NET_INITIALIZER(structure, field)     \
{                                                       \
	/* hdr */                                           \
	{                                                   \
		&cmdline_token_ipaddr_ops,      /* ops */       \
		offsetof(structure, field),     /* offset */    \
	},                                                  \
	/* ipaddr_data */                                   \
	{                                                   \
		CMDLINE_IPADDR_V4 |             /* flags */     \
		CMDLINE_IPADDR_NETWORK,                         \
	},                                                  \
}

#define TOKEN_IPV6NET_INITIALIZER(structure, field)     \
{                                                       \
	/* hdr */                                           \
	{                                                   \
		&cmdline_token_ipaddr_ops,      /* ops */       \
		offsetof(structure, field),     /* offset */    \
	},                                                  \
	/* ipaddr_data */                                   \
	{                                                   \
		CMDLINE_IPADDR_V4 |             /* flags */     \
		CMDLINE_IPADDR_NETWORK,                         \
	},                                                  \
}

#ifdef __cplusplus
}
#endif

#endif /* _PARSE_IPADDR_H_ */
