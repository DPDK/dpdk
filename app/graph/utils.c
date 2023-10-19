/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rte_common.h>

#include "module_api.h"

#define white_spaces_skip(pos)			\
({						\
	__typeof__(pos) _p = (pos);		\
	for ( ; isspace(*_p); _p++)		\
		;				\
	_p;					\
})

static void
hex_string_to_uint64(uint64_t *dst, const char *hexs)
{
	char buf[2] = {0};
	uint8_t shift = 4;
	int iter = 0;
	char c;

	while ((c = *hexs++)) {
		buf[0] = c;
		*dst |= (strtol(buf, NULL, 16) << shift);
		shift -= 4;
		iter++;
		if (iter == 2) {
			iter = 0;
			shift = 4;
			dst++;
		}
	}
}

int
parser_uint64_read(uint64_t *value, const char *p)
{
	char *next;
	uint64_t val;

	p = white_spaces_skip(p);
	if (!isdigit(*p))
		return -EINVAL;

	val = strtoul(p, &next, 0);
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

	p = white_spaces_skip(p);
	if (*p != '\0')
		return -EINVAL;

	*value = val;
	return 0;
}

int
parser_uint32_read(uint32_t *value, const char *p)
{
	uint64_t val = 0;
	int rc = parser_uint64_read(&val, p);

	if (rc < 0)
		return rc;

	if (val > UINT32_MAX)
		return -ERANGE;

	*value = val;
	return 0;
}

int
parser_ip4_read(uint32_t *value, char *p)
{
	uint8_t shift = 24;
	uint32_t ip = 0;
	char *token;

	token = strtok(p, ".");
	while (token != NULL) {
		ip |= (((uint32_t)strtoul(token, NULL, 10)) << shift);
		token = strtok(NULL, ".");
		shift -= 8;
	}

	*value = ip;

	return 0;
}

int
parser_ip6_read(uint8_t *value, char *p)
{
	uint64_t val = 0;
	char *token;

	token = strtok(p, ":");
	while (token != NULL) {
		hex_string_to_uint64(&val, token);
		*value = val;
		token = strtok(NULL, ":");
		value++;
		val = 0;
	}

	return 0;
}

int
parser_mac_read(uint64_t *value, char *p)
{
	uint64_t mac = 0, val = 0;
	uint8_t shift = 40;
	char *token;

	token = strtok(p, ":");
	while (token != NULL) {
		hex_string_to_uint64(&val, token);
		mac |= val << shift;
		token = strtok(NULL, ":");
		shift -= 8;
		val = 0;
	}

	*value = mac;

	return 0;
}
