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

/*
 * For inet_ntop() functions:
 *
 * Copyright (c) 1996 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#ifndef __linux__
#ifndef __FreeBSD__
#include <net/socket.h>
#else
#include <sys/socket.h>
#endif
#endif

#include <rte_string_fns.h>

#include "cmdline_parse.h"
#include "cmdline_parse_ipaddr.h"

struct cmdline_token_ops cmdline_token_ipaddr_ops = {
	.parse = cmdline_parse_ipaddr,
	.complete_get_nb = NULL,
	.complete_get_elt = NULL,
	.get_help = cmdline_get_help_ipaddr,
};

#define INADDRSZ 4
#define IN6ADDRSZ 16
#define PREFIXMAX 128
#define V4PREFIXMAX 32

/*
 * WARNING: Don't even consider trying to compile this on a system where
 * sizeof(int) < 4.  sizeof(int) > 4 is fine; all the world's not a VAX.
 */

static int inet_pton4(const char *src, unsigned char *dst);
static int inet_pton6(const char *src, unsigned char *dst);

/* int
 * inet_pton(af, src, dst)
 *      convert from presentation format (which usually means ASCII printable)
 *      to network format (which is usually some kind of binary format).
 * return:
 *      1 if the address was valid for the specified address family
 *      0 if the address wasn't valid (`dst' is untouched in this case)
 *      -1 if some other error occurred (`dst' is untouched in this case, too)
 * author:
 *      Paul Vixie, 1996.
 */
static int
my_inet_pton(int af, const char *src, void *dst)
{
	switch (af) {
		case AF_INET:
			return (inet_pton4(src, dst));
		case AF_INET6:
			return (inet_pton6(src, dst));
		default:
			errno = EAFNOSUPPORT;
			return (-1);
	}
	/* NOTREACHED */
}

/* int
 * inet_pton4(src, dst)
 *      like inet_aton() but without all the hexadecimal and shorthand.
 * return:
 *      1 if `src' is a valid dotted quad, else 0.
 * notice:
 *      does not touch `dst' unless it's returning 1.
 * author:
 *      Paul Vixie, 1996.
 */
static int
inet_pton4(const char *src, unsigned char *dst)
{
	static const char digits[] = "0123456789";
	int saw_digit, octets, ch;
	unsigned char tmp[INADDRSZ], *tp;

	saw_digit = 0;
	octets = 0;
	*(tp = tmp) = 0;
	while ((ch = *src++) != '\0') {
		const char *pch;

		if ((pch = strchr(digits, ch)) != NULL) {
			unsigned int new = *tp * 10 + (pch - digits);

			if (new > 255)
				return (0);
			if (! saw_digit) {
				if (++octets > 4)
					return (0);
				saw_digit = 1;
			}
			*tp = (unsigned char)new;
		} else if (ch == '.' && saw_digit) {
			if (octets == 4)
				return (0);
			*++tp = 0;
			saw_digit = 0;
		} else
			return (0);
	}
	if (octets < 4)
		return (0);

	memcpy(dst, tmp, INADDRSZ);
	return (1);
}

/* int
 * inet_pton6(src, dst)
 *      convert presentation level address to network order binary form.
 * return:
 *      1 if `src' is a valid [RFC1884 2.2] address, else 0.
 * notice:
 *      (1) does not touch `dst' unless it's returning 1.
 *      (2) :: in a full address is silently ignored.
 * credit:
 *      inspired by Mark Andrews.
 * author:
 *      Paul Vixie, 1996.
 */
static int
inet_pton6(const char *src, unsigned char *dst)
{
	static const char xdigits_l[] = "0123456789abcdef",
		xdigits_u[] = "0123456789ABCDEF";
	unsigned char tmp[IN6ADDRSZ], *tp = 0, *endp = 0, *colonp = 0;
	const char *xdigits = 0, *curtok = 0;
	int ch = 0, saw_xdigit = 0, count_xdigit = 0;
	unsigned int val = 0;
	unsigned dbloct_count = 0;

	memset((tp = tmp), '\0', IN6ADDRSZ);
	endp = tp + IN6ADDRSZ;
	colonp = NULL;
	/* Leading :: requires some special handling. */
	if (*src == ':')
		if (*++src != ':')
			return (0);
	curtok = src;
	saw_xdigit = count_xdigit = 0;
	val = 0;

	while ((ch = *src++) != '\0') {
		const char *pch;

		if ((pch = strchr((xdigits = xdigits_l), ch)) == NULL)
			pch = strchr((xdigits = xdigits_u), ch);
		if (pch != NULL) {
			if (count_xdigit >= 4)
				return (0);
			val <<= 4;
			val |= (pch - xdigits);
			if (val > 0xffff)
				return (0);
			saw_xdigit = 1;
			count_xdigit++;
			continue;
		}
		if (ch == ':') {
			curtok = src;
			if (!saw_xdigit) {
				if (colonp)
					return (0);
				colonp = tp;
				continue;
			} else if (*src == '\0') {
				return (0);
			}
			if (tp + sizeof(int16_t) > endp)
				return (0);
			*tp++ = (unsigned char) ((val >> 8) & 0xff);
			*tp++ = (unsigned char) (val & 0xff);
			saw_xdigit = 0;
			count_xdigit = 0;
			val = 0;
			dbloct_count++;
			continue;
		}
		if (ch == '.' && ((tp + INADDRSZ) <= endp) &&
		    inet_pton4(curtok, tp) > 0) {
			tp += INADDRSZ;
			saw_xdigit = 0;
			dbloct_count += 2;
			break;  /* '\0' was seen by inet_pton4(). */
		}
		return (0);
	}
	if (saw_xdigit) {
		if (tp + sizeof(int16_t) > endp)
			return (0);
		*tp++ = (unsigned char) ((val >> 8) & 0xff);
		*tp++ = (unsigned char) (val & 0xff);
		dbloct_count++;
	}
	if (colonp != NULL) {
		/* if we already have 8 double octets, having a colon means error */
		if (dbloct_count == 8)
			return 0;

		/*
		 * Since some memmove()'s erroneously fail to handle
		 * overlapping regions, we'll do the shift by hand.
		 */
		const int n = tp - colonp;
		int i;

		for (i = 1; i <= n; i++) {
			endp[- i] = colonp[n - i];
			colonp[n - i] = 0;
		}
		tp = endp;
	}
	if (tp != endp)
		return (0);
	memcpy(dst, tmp, IN6ADDRSZ);
	return (1);
}

int
cmdline_parse_ipaddr(cmdline_parse_token_hdr_t *tk, const char *buf, void *res)
{
	struct cmdline_token_ipaddr *tk2;
	unsigned int token_len = 0;
	char ip_str[INET6_ADDRSTRLEN+4+1]; /* '+4' is for prefixlen (if any) */
	cmdline_ipaddr_t ipaddr;
	char *prefix, *prefix_end;
	long prefixlen = 0;

	if (!buf || !tk || ! *buf)
		return -1;

	tk2 = (struct cmdline_token_ipaddr *)tk;

	while (!cmdline_isendoftoken(buf[token_len]))
		token_len++;

	/* if token is too big... */
	if (token_len >= INET6_ADDRSTRLEN+4)
		return -1;

	snprintf(ip_str, token_len+1, "%s", buf);

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
	    my_inet_pton(AF_INET, ip_str, &ipaddr.addr.ipv4) == 1 &&
		prefixlen <= V4PREFIXMAX) {
		ipaddr.family = AF_INET;
		if (res)
			memcpy(res, &ipaddr, sizeof(ipaddr));
		return token_len;
	}
	if ((tk2->ipaddr_data.flags & CMDLINE_IPADDR_V6) &&
	    my_inet_pton(AF_INET6, ip_str, &ipaddr.addr.ipv6) == 1) {
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
