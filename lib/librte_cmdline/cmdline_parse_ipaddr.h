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

#ifndef _PARSE_IPADDR_H_
#define _PARSE_IPADDR_H_

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
			 void *res);
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
