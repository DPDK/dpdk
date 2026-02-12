/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <eal_export.h>
#include <rte_string_fns.h>
#include <rte_errno.h>

/* split string into tokens */
RTE_EXPORT_SYMBOL(rte_strsplit)
int
rte_strsplit(char *string, int stringlen,
	     char **tokens, int maxtokens, char delim)
{
	int i, tok = 0;
	int tokstart = 1; /* first token is right at start of string */

	if (string == NULL || tokens == NULL)
		goto einval_error;

	for (i = 0; i < stringlen; i++) {
		if (string[i] == '\0' || tok >= maxtokens)
			break;
		if (tokstart) {
			tokstart = 0;
			tokens[tok++] = &string[i];
		}
		if (string[i] == delim) {
			string[i] = '\0';
			tokstart = 1;
		}
	}
	return tok;

einval_error:
	errno = EINVAL;
	return -1;
}

/* Copy src string into dst.
 *
 * Return negative value and NUL-terminate if dst is too short,
 * Otherwise return number of bytes copied.
 */
RTE_EXPORT_SYMBOL(rte_strscpy)
ssize_t
rte_strscpy(char *dst, const char *src, size_t dsize)
{
	size_t nleft = dsize;
	size_t res = 0;

	/* Copy as many bytes as will fit. */
	while (nleft != 0) {
		dst[res] = src[res];
		if (src[res] == '\0')
			return res;
		res++;
		nleft--;
	}

	/* Not enough room in dst, set NUL and return error. */
	if (res != 0)
		dst[res - 1] = '\0';
	rte_errno = E2BIG;
	return -rte_errno;
}

RTE_EXPORT_SYMBOL(rte_str_to_size)
uint64_t
rte_str_to_size(const char *str)
{
	char *endptr;
	unsigned long long size;

	while (isspace((int)*str))
		str++;
	if (*str == '-')
		return 0;

	errno = 0;
	size = strtoull(str, &endptr, 0);
	if (errno)
		return 0;

	if (*endptr == ' ')
		endptr++; /* allow 1 space gap */

	switch (*endptr) {
	case 'E': case 'e':
		size *= 1024; /* fall-through */
	case 'P': case 'p':
		size *= 1024; /* fall-through */
	case 'T': case 't':
		size *= 1024; /* fall-through */
	case 'G': case 'g':
		size *= 1024; /* fall-through */
	case 'M': case 'm':
		size *= 1024; /* fall-through */
	case 'K': case 'k':
		size *= 1024; /* fall-through */
	default:
		break;
	}
	return size;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_size_to_str, 25.07)
char *
rte_size_to_str(char *buf, int buf_size, uint64_t count, bool use_iec, const char *unit)
{
	/* https://en.wikipedia.org/wiki/International_System_of_Units */
	static const char prefix[] = "kMGTPE";
	size_t prefix_index = 0;
	const unsigned int base = use_iec ? 1024 : 1000;
	uint64_t powi = 1;
	uint16_t powj = 1;
	uint8_t precision = 2;
	int result;

	if (count < base) {
		if (unit != NULL && *unit != '\0')
			result = snprintf(buf, buf_size, "%"PRIu64" %s", count, unit);
		else
			result = snprintf(buf, buf_size, "%"PRIu64, count);

		return result < buf_size ? buf : NULL;
	}

	/* increase value by a factor of 1000/1024 and store
	 * if result is something a human can read
	 */
	for (; prefix_index < sizeof(prefix) - 1; ++prefix_index) {
		powi *= base;
		if (count / powi < base)
			break;
	}

	/* try to guess a good number of digits for precision */
	for (; precision > 0; precision--) {
		powj *= 10;
		if (count / powi < powj)
			break;
	}

	result = snprintf(buf, buf_size, "%.*f %c%s%s", precision,
			  (double)count / powi, prefix[prefix_index], use_iec ? "i" : "",
			  (unit != NULL) ? unit : "");
	return result < buf_size ? buf : NULL;
}
