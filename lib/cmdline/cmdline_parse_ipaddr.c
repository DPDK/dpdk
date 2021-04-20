/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <rte_string_fns.h>

#include "cmdline_parse.h"
#include "cmdline_parse_ipaddr.h"

struct cmdline_token_ops cmdline_token_ipaddr_ops = {
	.parse = cmdline_parse_ipaddr,
	.complete_get_nb = NULL,
	.complete_get_elt = NULL,
	.get_help = cmdline_get_help_ipaddr,
};

#define PREFIXMAX 128
#define V4PREFIXMAX 32

int
cmdline_parse_ipaddr(cmdline_parse_token_hdr_t *tk, const char *buf, void *res,
	unsigned ressize)
{
	struct cmdline_token_ipaddr *tk2;
	unsigned int token_len = 0;
	char ip_str[INET6_ADDRSTRLEN+4+1]; /* '+4' is for prefixlen (if any) */
	cmdline_ipaddr_t ipaddr;
	char *prefix, *prefix_end;
	long prefixlen = 0;

	if (res && ressize < sizeof(cmdline_ipaddr_t))
		return -1;

	if (!buf || !tk || ! *buf)
		return -1;

	tk2 = (struct cmdline_token_ipaddr *)tk;

	while (!cmdline_isendoftoken(buf[token_len]))
		token_len++;

	/* if token is too big... */
	if (token_len >= INET6_ADDRSTRLEN+4)
		return -1;

	strlcpy(ip_str, buf, token_len + 1);

	/* convert the network prefix */
	if (tk2->ipaddr_data.flags & CMDLINE_IPADDR_NETWORK) {
		prefix = strrchr(ip_str, '/');
		if (prefix == NULL)
			return -1;
		*prefix = '\0';
		prefix ++;
		errno = 0;
		prefixlen = strtol(prefix, &prefix_end, 10);
		if (errno || (*prefix_end != '\0')
			|| prefixlen < 0 || prefixlen > PREFIXMAX)
			return -1;
		ipaddr.prefixlen = prefixlen;
	}
	else {
		ipaddr.prefixlen = 0;
	}

	/* convert the IP addr */
	if ((tk2->ipaddr_data.flags & CMDLINE_IPADDR_V4) &&
	    inet_pton(AF_INET, ip_str, &ipaddr.addr.ipv4) == 1 &&
		prefixlen <= V4PREFIXMAX) {
		ipaddr.family = AF_INET;
		if (res)
			memcpy(res, &ipaddr, sizeof(ipaddr));
		return token_len;
	}
	if ((tk2->ipaddr_data.flags & CMDLINE_IPADDR_V6) &&
	    inet_pton(AF_INET6, ip_str, &ipaddr.addr.ipv6) == 1) {
		ipaddr.family = AF_INET6;
		if (res)
			memcpy(res, &ipaddr, sizeof(ipaddr));
		return token_len;
	}
	return -1;

}

int cmdline_get_help_ipaddr(cmdline_parse_token_hdr_t *tk, char *dstbuf,
			    unsigned int size)
{
	struct cmdline_token_ipaddr *tk2;

	if (!tk || !dstbuf)
		return -1;

	tk2 = (struct cmdline_token_ipaddr *)tk;

	switch (tk2->ipaddr_data.flags) {
	case CMDLINE_IPADDR_V4:
		snprintf(dstbuf, size, "IPv4");
		break;
	case CMDLINE_IPADDR_V6:
		snprintf(dstbuf, size, "IPv6");
		break;
	case CMDLINE_IPADDR_V4|CMDLINE_IPADDR_V6:
		snprintf(dstbuf, size, "IPv4/IPv6");
		break;
	case CMDLINE_IPADDR_NETWORK|CMDLINE_IPADDR_V4:
		snprintf(dstbuf, size, "IPv4 network");
		break;
	case CMDLINE_IPADDR_NETWORK|CMDLINE_IPADDR_V6:
		snprintf(dstbuf, size, "IPv6 network");
		break;
	case CMDLINE_IPADDR_NETWORK|CMDLINE_IPADDR_V4|CMDLINE_IPADDR_V6:
		snprintf(dstbuf, size, "IPv4/IPv6 network");
		break;
	default:
		snprintf(dstbuf, size, "IPaddr (bad flags)");
		break;
	}
	return 0;
}
