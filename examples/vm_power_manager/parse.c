/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright(c) 2014 6WIND S.A.
 */

#include <string.h>
#include <rte_log.h>
#include "parse.h"

/*
 * Parse elem, the elem could be single number/range or group
 * 1) A single number elem, it's just a simple digit. e.g. 9
 * 2) A single range elem, two digits with a '-' between. e.g. 2-6
 * 3) A group elem, combines multiple 1) or 2) e.g 0,2-4,6
 *    Within group, '-' used for a range separator;
 *                       ',' used for a single number.
 */
int
parse_set(const char *input, uint16_t set[], unsigned int num)
{
	unsigned int idx;
	const char *str = input;
	char *end = NULL;
	unsigned int min, max;

	memset(set, 0, num * sizeof(uint16_t));

	while (isblank(*str))
		str++;

	/* only digit or left bracket is qualify for start point */
	if (!isdigit(*str) || *str == '\0')
		return -1;

	while (isblank(*str))
		str++;
	if (*str == '\0')
		return -1;

	min = num;
	do {

		/* go ahead to the first digit */
		while (isblank(*str))
			str++;
		if (!isdigit(*str))
			return -1;

		/* get the digit value */
		errno = 0;
		idx = strtoul(str, &end, 10);
		if (errno || end == NULL || idx >= num)
			return -1;

		/* go ahead to separator '-' and ',' */
		while (isblank(*end))
			end++;
		if (*end == '-') {
			if (min == num)
				min = idx;
			else /* avoid continuous '-' */
				return -1;
		} else if ((*end == ',') || (*end == '\0')) {
			max = idx;

			if (min == num)
				min = idx;

			for (idx = RTE_MIN(min, max);
					idx <= RTE_MAX(min, max); idx++) {
				set[idx] = 1;
			}
			min = num;
		} else
			return -1;

		str = end + 1;
	} while (*end != '\0');

	return str - input;
}
