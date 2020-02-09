/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>

#include <rte_string_fns.h>
#include <rte_ether.h>

#include "cmdline_parse.h"
#include "cmdline_parse_etheraddr.h"

struct cmdline_token_ops cmdline_token_etheraddr_ops = {
	.parse = cmdline_parse_etheraddr,
	.complete_get_nb = NULL,
	.complete_get_elt = NULL,
	.get_help = cmdline_get_help_etheraddr,
};

int
cmdline_parse_etheraddr(__rte_unused cmdline_parse_token_hdr_t *tk,
	const char *buf, void *res, unsigned ressize)
{
	unsigned int token_len = 0;
	char ether_str[RTE_ETHER_ADDR_FMT_SIZE];
	struct rte_ether_addr tmp;

	if (res && ressize < sizeof(tmp))
		return -1;

	if (!buf || ! *buf)
		return -1;

	while (!cmdline_isendoftoken(buf[token_len]))
		token_len++;

	/* if token doesn't match possible string lengths... */
	if (token_len >= RTE_ETHER_ADDR_FMT_SIZE)
		return -1;

	strlcpy(ether_str, buf, token_len + 1);

	if (rte_ether_unformat_addr(ether_str, &tmp) < 0)
		return -1;

	if (res)
		memcpy(res, &tmp, sizeof(tmp));
	return token_len;
}

int
cmdline_get_help_etheraddr(__rte_unused cmdline_parse_token_hdr_t *tk,
			       char *dstbuf, unsigned int size)
{
	int ret;

	ret = snprintf(dstbuf, size, "Ethernet address");
	if (ret < 0)
		return -1;
	return 0;
}
