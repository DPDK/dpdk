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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <net/ethernet.h>

#include <rte_string_fns.h>

#include "cmdline_parse.h"
#include "cmdline_parse_etheraddr.h"

struct cmdline_token_ops cmdline_token_etheraddr_ops = {
	.parse = cmdline_parse_etheraddr,
	.complete_get_nb = NULL,
	.complete_get_elt = NULL,
	.get_help = cmdline_get_help_etheraddr,
};

/* the format can be either XX:XX:XX:XX:XX:XX or XXXX:XXXX:XXXX */
#define ETHER_ADDRSTRLENLONG 18
#define ETHER_ADDRSTRLENSHORT 15

#ifdef __linux__
#define ea_oct ether_addr_octet
#else
#define ea_oct octet
#endif


static struct ether_addr *
my_ether_aton(const char *a)
{
	int i;
	char *end;
	unsigned long o[ETHER_ADDR_LEN];
	static struct ether_addr ether_addr;

	i = 0;
	do {
		errno = 0;
		o[i] = strtoul(a, &end, 16);
		if (errno != 0 || end == a || (end[0] != ':' && end[0] != 0))
			return NULL;
		a = end + 1;
	} while (++i != sizeof (o) / sizeof (o[0]) && end[0] != 0);

	/* Junk at the end of line */
	if (end[0] != 0)
		return NULL;

	/* Support the format XX:XX:XX:XX:XX:XX */
	if (i == ETHER_ADDR_LEN) {
		while (i-- != 0) {
			if (o[i] > UINT8_MAX)
				return NULL;
			ether_addr.ea_oct[i] = (uint8_t)o[i];
		}
	/* Support the format XXXX:XXXX:XXXX */
	} else if (i == ETHER_ADDR_LEN / 2) {
		while (i-- != 0) {
			if (o[i] > UINT16_MAX)
				return NULL;
			ether_addr.ea_oct[i * 2] = (uint8_t)(o[i] >> 8);
			ether_addr.ea_oct[i * 2 + 1] = (uint8_t)(o[i] & 0xff);
		}
	/* unknown format */
	} else
		return NULL;

	return (struct ether_addr *)&ether_addr;
}

int
cmdline_parse_etheraddr(__attribute__((unused)) cmdline_parse_token_hdr_t *tk,
	const char *buf, void *res, unsigned ressize)
{
	unsigned int token_len = 0;
	char ether_str[ETHER_ADDRSTRLENLONG+1];
	struct ether_addr *tmp;

	if (res && ressize < sizeof(struct ether_addr))
		return -1;

	if (!buf || ! *buf)
		return -1;

	while (!cmdline_isendoftoken(buf[token_len]))
		token_len++;

	/* if token doesn't match possible string lengths... */
	if ((token_len != ETHER_ADDRSTRLENLONG - 1) &&
			(token_len != ETHER_ADDRSTRLENSHORT - 1))
		return -1;

	snprintf(ether_str, token_len+1, "%s", buf);

	tmp = my_ether_aton(ether_str);
	if (tmp == NULL)
		return -1;
	if (res)
		memcpy(res, tmp, sizeof(struct ether_addr));
	return token_len;
}

int
cmdline_get_help_etheraddr(__attribute__((unused)) cmdline_parse_token_hdr_t *tk,
			       char *dstbuf, unsigned int size)
{
	int ret;

	ret = snprintf(dstbuf, size, "Ethernet address");
	if (ret < 0)
		return -1;
	return 0;
}
