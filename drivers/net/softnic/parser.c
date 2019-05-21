/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation.
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 */

/* For inet_pton4() and inet_pton6() functions:
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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <getopt.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/wait.h>

#include <rte_errno.h>

#include "parser.h"

static uint32_t
get_hex_val(char c)
{
	switch (c) {
	case '0': case '1': case '2': case '3': case '4': case '5':
	case '6': case '7': case '8': case '9':
		return c - '0';
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		return c - 'A' + 10;
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
		return c - 'a' + 10;
	default:
		return 0;
	}
}

int
softnic_parser_read_arg_bool(const char *p)
{
	p = skip_white_spaces(p);
	int result = -EINVAL;

	if (((p[0] == 'y') && (p[1] == 'e') && (p[2] == 's')) ||
		((p[0] == 'Y') && (p[1] == 'E') && (p[2] == 'S'))) {
		p += 3;
		result = 1;
	}

	if (((p[0] == 'o') && (p[1] == 'n')) ||
		((p[0] == 'O') && (p[1] == 'N'))) {
		p += 2;
		result = 1;
	}

	if (((p[0] == 'n') && (p[1] == 'o')) ||
		((p[0] == 'N') && (p[1] == 'O'))) {
		p += 2;
		result = 0;
	}

	if (((p[0] == 'o') && (p[1] == 'f') && (p[2] == 'f')) ||
		((p[0] == 'O') && (p[1] == 'F') && (p[2] == 'F'))) {
		p += 3;
		result = 0;
	}

	p = skip_white_spaces(p);

	if (p[0] != '\0')
		return -EINVAL;

	return result;
}

int
softnic_parser_read_int32(int32_t *value, const char *p)
{
	char *next;
	int32_t val;

	p = skip_white_spaces(p);
	if (!isdigit(*p))
		return -EINVAL;

	val = strtol(p, &next, 10);
	if (p == next)
		return -EINVAL;

	*value = val;
	return 0;
}

int
softnic_parser_read_uint64(uint64_t *value, const char *p)
{
	char *next;
	uint64_t val;

	p = skip_white_spaces(p);
	if (!isdigit(*p))
		return -EINVAL;

	val = strtoul(p, &next, 10);
	if (p == next)
		return -EINVAL;

	p = next;
	switch (*p) {
	case 'T':
		val *= 1024ULL;
		/* fall through */
	case 'G':
		val *= 1024ULL;
		/* fall through */
	case 'M':
		val *= 1024ULL;
		/* fall through */
	case 'k':
	case 'K':
		val *= 1024ULL;
		p++;
		break;
	}

	p = skip_white_spaces(p);
	if (*p != '\0')
		return -EINVAL;

	*value = val;
	return 0;
}

int
softnic_parser_read_uint64_hex(uint64_t *value, const char *p)
{
	char *next;
	uint64_t val;

	p = skip_white_spaces(p);

	val = strtoul(p, &next, 16);
	if (p == next)
		return -EINVAL;

	p = skip_white_spaces(next);
	if (*p != '\0')
		return -EINVAL;

	*value = val;
	return 0;
}

int
softnic_parser_read_uint32(uint32_t *value, const char *p)
{
	uint64_t val = 0;
	int ret = softnic_parser_read_uint64(&val, p);

	if (ret < 0)
		return ret;

	if (val > UINT32_MAX)
		return -ERANGE;

	*value = val;
	return 0;
}

int
softnic_parser_read_uint32_hex(uint32_t *value, const char *p)
{
	uint64_t val = 0;
	int ret = softnic_parser_read_uint64_hex(&val, p);

	if (ret < 0)
		return ret;

	if (val > UINT32_MAX)
		return -ERANGE;

	*value = val;
	return 0;
}

int
softnic_parser_read_uint16(uint16_t *value, const char *p)
{
	uint64_t val = 0;
	int ret = softnic_parser_read_uint64(&val, p);

	if (ret < 0)
		return ret;

	if (val > UINT16_MAX)
		return -ERANGE;

	*value = val;
	return 0;
}

int
softnic_parser_read_uint16_hex(uint16_t *value, const char *p)
{
	uint64_t val = 0;
	int ret = softnic_parser_read_uint64_hex(&val, p);

	if (ret < 0)
		return ret;

	if (val > UINT16_MAX)
		return -ERANGE;

	*value = val;
	return 0;
}

int
softnic_parser_read_uint8(uint8_t *value, const char *p)
{
	uint64_t val = 0;
	int ret = softnic_parser_read_uint64(&val, p);

	if (ret < 0)
		return ret;

	if (val > UINT8_MAX)
		return -ERANGE;

	*value = val;
	return 0;
}

int
softnic_parser_read_uint8_hex(uint8_t *value, const char *p)
{
	uint64_t val = 0;
	int ret = softnic_parser_read_uint64_hex(&val, p);

	if (ret < 0)
		return ret;

	if (val > UINT8_MAX)
		return -ERANGE;

	*value = val;
	return 0;
}

int
softnic_parse_tokenize_string(char *string, char *tokens[], uint32_t *n_tokens)
{
	uint32_t i;

	if (string == NULL ||
		tokens == NULL ||
		(*n_tokens < 1))
		return -EINVAL;

	for (i = 0; i < *n_tokens; i++) {
		tokens[i] = strtok_r(string, PARSE_DELIMITER, &string);
		if (tokens[i] == NULL)
			break;
	}

	if (i == *n_tokens &&
		strtok_r(string, PARSE_DELIMITER, &string) != NULL)
		return -E2BIG;

	*n_tokens = i;
	return 0;
}

int
softnic_parse_hex_string(char *src, uint8_t *dst, uint32_t *size)
{
	char *c;
	uint32_t len, i;

	/* Check input parameters */
	if (src == NULL ||
		dst == NULL ||
		size == NULL ||
		(*size == 0))
		return -1;

	len = strlen(src);
	if (((len & 3) != 0) ||
		(len > (*size) * 2))
		return -1;
	*size = len / 2;

	for (c = src; *c != 0; c++) {
		if ((((*c) >= '0') && ((*c) <= '9')) ||
			(((*c) >= 'A') && ((*c) <= 'F')) ||
			(((*c) >= 'a') && ((*c) <= 'f')))
			continue;

		return -1;
	}

	/* Convert chars to bytes */
	for (i = 0; i < *size; i++)
		dst[i] = get_hex_val(src[2 * i]) * 16 +
			get_hex_val(src[2 * i + 1]);

	return 0;
}

int
softnic_parse_mpls_labels(char *string, uint32_t *labels, uint32_t *n_labels)
{
	uint32_t n_max_labels = *n_labels, count = 0;

	/* Check for void list of labels */
	if (strcmp(string, "<void>") == 0) {
		*n_labels = 0;
		return 0;
	}

	/* At least one label should be present */
	for ( ; (*string != '\0'); ) {
		char *next;
		int value;

		if (count >= n_max_labels)
			return -1;

		if (count > 0) {
			if (string[0] != ':')
				return -1;

			string++;
		}

		value = strtol(string, &next, 10);
		if (next == string)
			return -1;
		string = next;

		labels[count++] = (uint32_t)value;
	}

	*n_labels = count;
	return 0;
}

#define INADDRSZ 4
#define IN6ADDRSZ 16

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

		pch = strchr(digits, ch);
		if (pch != NULL) {
			unsigned int new = *tp * 10 + (pch - digits);

			if (new > 255)
				return 0;
			if (!saw_digit) {
				if (++octets > 4)
					return 0;
				saw_digit = 1;
			}
			*tp = (unsigned char)new;
		} else if (ch == '.' && saw_digit) {
			if (octets == 4)
				return 0;
			*++tp = 0;
			saw_digit = 0;
		} else
			return 0;
	}
	if (octets < 4)
		return 0;

	memcpy(dst, tmp, INADDRSZ);
	return 1;
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
	unsigned int dbloct_count = 0;

	memset((tp = tmp), '\0', IN6ADDRSZ);
	endp = tp + IN6ADDRSZ;
	colonp = NULL;
	/* Leading :: requires some special handling. */
	if (*src == ':')
		if (*++src != ':')
			return 0;
	curtok = src;
	saw_xdigit = count_xdigit = 0;
	val = 0;

	while ((ch = *src++) != '\0') {
		const char *pch;

		pch = strchr((xdigits = xdigits_l), ch);
		if (pch == NULL)
			pch = strchr((xdigits = xdigits_u), ch);
		if (pch != NULL) {
			if (count_xdigit >= 4)
				return 0;
			val <<= 4;
			val |= (pch - xdigits);
			if (val > 0xffff)
				return 0;
			saw_xdigit = 1;
			count_xdigit++;
			continue;
		}
		if (ch == ':') {
			curtok = src;
			if (!saw_xdigit) {
				if (colonp)
					return 0;
				colonp = tp;
				continue;
			} else if (*src == '\0') {
				return 0;
			}
			if (tp + sizeof(int16_t) > endp)
				return 0;
			*tp++ = (unsigned char)((val >> 8) & 0xff);
			*tp++ = (unsigned char)(val & 0xff);
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
		return 0;
	}
	if (saw_xdigit) {
		if (tp + sizeof(int16_t) > endp)
			return 0;
		*tp++ = (unsigned char)((val >> 8) & 0xff);
		*tp++ = (unsigned char)(val & 0xff);
		dbloct_count++;
	}
	if (colonp != NULL) {
		/* if we already have 8 double octets, having a colon means error */
		if (dbloct_count == 8)
			return 0;

		/* Since some memmove()'s erroneously fail to handle
		 * overlapping regions, we'll do the shift by hand.
		 */
		const int n = tp - colonp;
		int i;

		for (i = 1; i <= n; i++) {
			endp[-i] = colonp[n - i];
			colonp[n - i] = 0;
		}
		tp = endp;
	}
	if (tp != endp)
		return 0;
	memcpy(dst, tmp, IN6ADDRSZ);
	return 1;
}

static struct rte_ether_addr *
my_ether_aton(const char *a)
{
	int i;
	char *end;
	unsigned long o[RTE_ETHER_ADDR_LEN];
	static struct rte_ether_addr ether_addr;

	i = 0;
	do {
		errno = 0;
		o[i] = strtoul(a, &end, 16);
		if (errno != 0 || end == a || (end[0] != ':' && end[0] != 0))
			return NULL;
		a = end + 1;
	} while (++i != sizeof(o) / sizeof(o[0]) && end[0] != 0);

	/* Junk at the end of line */
	if (end[0] != 0)
		return NULL;

	/* Support the format XX:XX:XX:XX:XX:XX */
	if (i == RTE_ETHER_ADDR_LEN) {
		while (i-- != 0) {
			if (o[i] > UINT8_MAX)
				return NULL;
			ether_addr.addr_bytes[i] = (uint8_t)o[i];
		}
	/* Support the format XXXX:XXXX:XXXX */
	} else if (i == RTE_ETHER_ADDR_LEN / 2) {
		while (i-- != 0) {
			if (o[i] > UINT16_MAX)
				return NULL;
			ether_addr.addr_bytes[i * 2] = (uint8_t)(o[i] >> 8);
			ether_addr.addr_bytes[i * 2 + 1] = (uint8_t)(o[i] & 0xff);
		}
	/* unknown format */
	} else
		return NULL;

	return (struct rte_ether_addr *)&ether_addr;
}

int
softnic_parse_ipv4_addr(const char *token, struct in_addr *ipv4)
{
	if (strlen(token) >= INET_ADDRSTRLEN)
		return -EINVAL;

	if (inet_pton4(token, (unsigned char *)ipv4) != 1)
		return -EINVAL;

	return 0;
}

int
softnic_parse_ipv6_addr(const char *token, struct in6_addr *ipv6)
{
	if (strlen(token) >= INET6_ADDRSTRLEN)
		return -EINVAL;

	if (inet_pton6(token, (unsigned char *)ipv6) != 1)
		return -EINVAL;

	return 0;
}

int
softnic_parse_mac_addr(const char *token, struct rte_ether_addr *addr)
{
	struct rte_ether_addr *tmp;

	tmp = my_ether_aton(token);
	if (tmp == NULL)
		return -1;

	memcpy(addr, tmp, sizeof(struct rte_ether_addr));
	return 0;
}

int
softnic_parse_cpu_core(const char *entry,
	struct softnic_cpu_core_params *p)
{
	size_t num_len;
	char num[8];

	uint32_t s = 0, c = 0, h = 0, val;
	uint8_t s_parsed = 0, c_parsed = 0, h_parsed = 0;
	const char *next = skip_white_spaces(entry);
	char type;

	if (p == NULL)
		return -EINVAL;

	/* Expect <CORE> or [sX][cY][h]. At least one parameter is required. */
	while (*next != '\0') {
		/* If everything parsed nothing should left */
		if (s_parsed && c_parsed && h_parsed)
			return -EINVAL;

		type = *next;
		switch (type) {
		case 's':
		case 'S':
			if (s_parsed || c_parsed || h_parsed)
				return -EINVAL;
			s_parsed = 1;
			next++;
			break;
		case 'c':
		case 'C':
			if (c_parsed || h_parsed)
				return -EINVAL;
			c_parsed = 1;
			next++;
			break;
		case 'h':
		case 'H':
			if (h_parsed)
				return -EINVAL;
			h_parsed = 1;
			next++;
			break;
		default:
			/* If it start from digit it must be only core id. */
			if (!isdigit(*next) || s_parsed || c_parsed || h_parsed)
				return -EINVAL;

			type = 'C';
		}

		for (num_len = 0; *next != '\0'; next++, num_len++) {
			if (num_len == RTE_DIM(num))
				return -EINVAL;

			if (!isdigit(*next))
				break;

			num[num_len] = *next;
		}

		if (num_len == 0 && type != 'h' && type != 'H')
			return -EINVAL;

		if (num_len != 0 && (type == 'h' || type == 'H'))
			return -EINVAL;

		num[num_len] = '\0';
		val = strtol(num, NULL, 10);

		h = 0;
		switch (type) {
		case 's':
		case 'S':
			s = val;
			break;
		case 'c':
		case 'C':
			c = val;
			break;
		case 'h':
		case 'H':
			h = 1;
			break;
		}
	}

	p->socket_id = s;
	p->core_id = c;
	p->thread_id = h;
	return 0;
}
