/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
 *   Copyright(c) 2017 Cavium, Inc. All rights reserved.
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
#include <stdbool.h>

#include <rte_errno.h>
#include <rte_string_fns.h>

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
parser_read_arg_bool(const char *p)
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
parser_read_uint64(uint64_t *value, const char *p)
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
parser_read_int32(int32_t *value, const char *p)
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
parser_read_uint64_hex(uint64_t *value, const char *p)
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
parser_read_uint32(uint32_t *value, const char *p)
{
	uint64_t val = 0;
	int ret = parser_read_uint64(&val, p);

	if (ret < 0)
		return ret;

	if (val > UINT32_MAX)
		return -ERANGE;

	*value = val;
	return 0;
}

int
parser_read_uint32_hex(uint32_t *value, const char *p)
{
	uint64_t val = 0;
	int ret = parser_read_uint64_hex(&val, p);

	if (ret < 0)
		return ret;

	if (val > UINT32_MAX)
		return -ERANGE;

	*value = val;
	return 0;
}

int
parser_read_uint16(uint16_t *value, const char *p)
{
	uint64_t val = 0;
	int ret = parser_read_uint64(&val, p);

	if (ret < 0)
		return ret;

	if (val > UINT16_MAX)
		return -ERANGE;

	*value = val;
	return 0;
}

int
parser_read_uint16_hex(uint16_t *value, const char *p)
{
	uint64_t val = 0;
	int ret = parser_read_uint64_hex(&val, p);

	if (ret < 0)
		return ret;

	if (val > UINT16_MAX)
		return -ERANGE;

	*value = val;
	return 0;
}

int
parser_read_uint8(uint8_t *value, const char *p)
{
	uint64_t val = 0;
	int ret = parser_read_uint64(&val, p);

	if (ret < 0)
		return ret;

	if (val > UINT8_MAX)
		return -ERANGE;

	*value = val;
	return 0;
}

int
parser_read_uint8_hex(uint8_t *value, const char *p)
{
	uint64_t val = 0;
	int ret = parser_read_uint64_hex(&val, p);

	if (ret < 0)
		return ret;

	if (val > UINT8_MAX)
		return -ERANGE;

	*value = val;
	return 0;
}

int
parse_tokenize_string(char *string, char *tokens[], uint32_t *n_tokens)
{
	uint32_t i;

	if ((string == NULL) ||
		(tokens == NULL) ||
		(*n_tokens < 1))
		return -EINVAL;

	for (i = 0; i < *n_tokens; i++) {
		tokens[i] = strtok_r(string, PARSE_DELIMITER, &string);
		if (tokens[i] == NULL)
			break;
	}

	if ((i == *n_tokens) &&
		(strtok_r(string, PARSE_DELIMITER, &string) != NULL))
		return -E2BIG;

	*n_tokens = i;
	return 0;
}

int
parse_hex_string(char *src, uint8_t *dst, uint32_t *size)
{
	char *c;
	uint32_t len, i;

	/* Check input parameters */
	if ((src == NULL) ||
		(dst == NULL) ||
		(size == NULL) ||
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
parse_lcores_list(bool lcores[], const char *corelist)
{
	int i, idx = 0;
	int min, max;
	char *end = NULL;

	if (corelist == NULL)
		return -1;
	while (isblank(*corelist))
		corelist++;
	i = strlen(corelist);
	while ((i > 0) && isblank(corelist[i - 1]))
		i--;

	/* Get list of lcores */
	min = RTE_MAX_LCORE;
	do {
		while (isblank(*corelist))
			corelist++;
		if (*corelist == '\0')
			return -1;
		idx = strtoul(corelist, &end, 10);

		if (end == NULL)
			return -1;
		while (isblank(*end))
			end++;
		if (*end == '-') {
			min = idx;
		} else if ((*end == ',') || (*end == '\0')) {
			max = idx;
			if (min == RTE_MAX_LCORE)
				min = idx;
			for (idx = min; idx <= max; idx++) {
				if (lcores[idx] == 1)
					return -E2BIG;
				lcores[idx] = 1;
			}

			min = RTE_MAX_LCORE;
		} else
			return -1;
		corelist = end + 1;
	} while (*end != '\0');

	return 0;
}
