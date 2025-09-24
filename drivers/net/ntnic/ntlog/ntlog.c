/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"

#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <rte_log.h>
#include <rte_string_fns.h>

#define NTLOG_HELPER_STR_SIZE_MAX (1024)

char *nthw_log_helper_str_alloc(const char *sinit)
{
	char *s = malloc(NTLOG_HELPER_STR_SIZE_MAX);

	if (!s)
		return NULL;

	if (sinit)
		snprintf(s, NTLOG_HELPER_STR_SIZE_MAX, "%s", sinit);

	else
		s[0] = '\0';

	return s;
}

__rte_format_printf(2, 0)
void nthw_log_helper_str_add(char *s, const char *format, ...)
{
	if (!s)
		return;

	va_list args;
	va_start(args, format);
	int len = strlen(s);
	vsnprintf(&s[len], (NTLOG_HELPER_STR_SIZE_MAX - 1 - len), format, args);
	va_end(args);
}

void nthw_log_helper_str_free(char *s)
{
	free(s);
}
