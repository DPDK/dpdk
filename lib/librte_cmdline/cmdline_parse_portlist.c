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
 * Copyright (c) 2010, Keith Wiles <keith.wiles@windriver.com>
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
#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include <rte_string_fns.h>
#include "cmdline_parse.h"
#include "cmdline_parse_portlist.h"

struct cmdline_token_ops cmdline_token_portlist_ops = {
	.parse = cmdline_parse_portlist,
	.complete_get_nb = NULL,
	.complete_get_elt = NULL,
	.get_help = cmdline_get_help_portlist,
};

static void
parse_set_list(cmdline_portlist_t *pl, size_t low, size_t high)
{
	do {
		pl->map |= (1 << low++);
	} while (low <= high);
}

static int
parse_ports(cmdline_portlist_t *pl, const char *str)
{
	size_t ps, pe;
	const char *first, *last;
	char *end;

	for (first = str, last = first;
	    first != NULL && last != NULL;
	    first = last + 1) {

		last = strchr(first, ',');

		errno = 0;
		ps = strtoul(first, &end, 10);
		if (errno != 0 || end == first ||
		    (end[0] != '-' && end[0] != 0 && end != last))
			return -1;

		/* Support for N-M portlist format */
		if (end[0] == '-') {
			errno = 0;
			first = end + 1;
			pe = strtoul(first, &end, 10);
			if (errno != 0 || end == first ||
			    (end[0] != 0 && end != last))
				return -1;
		} else {
			pe = ps;
		}

		if (ps > pe || pe >= sizeof (pl->map) * 8)
			return -1;

		parse_set_list(pl, ps, pe);
	}

	return 0;
}

int
cmdline_parse_portlist(__attribute__((unused)) cmdline_parse_token_hdr_t *tk,
	const char *buf, void *res, unsigned ressize)
{
	unsigned int token_len = 0;
	char portlist_str[PORTLIST_TOKEN_SIZE+1];
	cmdline_portlist_t *pl;

	if (!buf || ! *buf)
		return -1;

	if (res && ressize < sizeof(cmdline_portlist_t))
		return -1;

	pl = res;

	while (!cmdline_isendoftoken(buf[token_len]) &&
	    (token_len < PORTLIST_TOKEN_SIZE))
		token_len++;

	if (token_len >= PORTLIST_TOKEN_SIZE)
		return -1;

	snprintf(portlist_str, token_len+1, "%s", buf);

	if (pl) {
		pl->map = 0;
		if (strcmp("all", portlist_str) == 0)
			pl->map	= UINT32_MAX;
		else if (parse_ports(pl, portlist_str) != 0)
			return -1;
	}

	return token_len;
}

int
cmdline_get_help_portlist(__attribute__((unused)) cmdline_parse_token_hdr_t *tk,
		char *dstbuf, unsigned int size)
{
	int ret;
	ret = snprintf(dstbuf, size, "range of ports as 3,4-6,8-19,20");
	if (ret < 0)
		return -1;
	return 0;
}
