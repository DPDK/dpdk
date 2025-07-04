/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _RTE_TELEMETRY_JSON_H_
#define _RTE_TELEMETRY_JSON_H_

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <rte_common.h>
#include <rte_telemetry.h>

/**
 * @file
 * Internal Telemetry Utility functions
 *
 * This file contains small inline functions to make it easier for applications
 * to build up valid JSON responses to telemetry requests.
 */

/**
 * @internal
 * Copies a value into a buffer if the buffer has enough available space.
 * Nothing written to buffer if an overflow occurs.
 * This function is not for use for values larger than given buffer length.
 */
__rte_format_printf(3, 4)
static inline int
__json_snprintf(char *buf, const int len, const char *format, ...)
{
	va_list ap;
	char tmp[4];
	char *newbuf;
	int ret;

	if (len == 0)
		return 0;

	/* to ensure unmodified if we overflow, we save off any values currently in buf
	 * before we printf, if they are short enough. We restore them on error.
	 */
	if (strnlen(buf, sizeof(tmp)) < sizeof(tmp)) {
		strcpy(tmp, buf);  /* strcpy is safe as we know the length */
		va_start(ap, format);
		ret = vsnprintf(buf, len, format, ap);
		va_end(ap);
		if (ret > 0 && ret < len)
			return ret;
		strcpy(buf, tmp);  /* restore on error */
		return 0;
	}

	/* in normal operations should never hit this, but can do if buffer is
	 * incorrectly initialized e.g. in unit test cases
	 */
	newbuf = malloc(len);
	if (newbuf == NULL)
		return 0;

	va_start(ap, format);
	ret = vsnprintf(newbuf, len, format, ap);
	va_end(ap);
	if (ret > 0 && ret < len) {
		strcpy(buf, newbuf);
		free(newbuf);
		return ret;
	}
	free(newbuf);
	return 0; /* nothing written or modified */
}

static const char control_chars[0x20] = {
		['\n'] = 'n',
		['\r'] = 'r',
		['\t'] = 't',
};

/**
 * @internal
 * Function that does the actual printing, used by __json_format_str. Modifies buffer
 * directly, but returns 0 on overflow. Otherwise returns number of chars written to buffer.
 */
static inline int
__json_format_str_to_buf(char *buf, const int len,
		const char *prefix, const char *str, const char *suffix)
{
	int bufidx = 0;

	while (*prefix != '\0' && bufidx < len)
		buf[bufidx++] = *prefix++;
	if (bufidx >= len)
		return 0;

	while (*str != '\0') {
		if (*str < (int)RTE_DIM(control_chars)) {
			int idx = *str;  /* compilers don't like char type as index */
			if (control_chars[idx] != 0) {
				buf[bufidx++] = '\\';
				buf[bufidx++] = control_chars[idx];
			}
		} else if (*str == '"' || *str == '\\') {
			buf[bufidx++] = '\\';
			buf[bufidx++] = *str;
		} else
			buf[bufidx++] = *str;
		/* we always need space for (at minimum) closing quote and null character.
		 * Ensuring at least two free characters also means we can always take an
		 * escaped character like "\n" without overflowing
		 */
		if (bufidx > len - 2)
			return 0;
		str++;
	}

	while (*suffix != '\0' && bufidx < len)
		buf[bufidx++] = *suffix++;
	if (bufidx >= len)
		return 0;

	buf[bufidx] = '\0';
	return bufidx;
}

/**
 * @internal
 * This function acts the same as __json_snprintf(buf, len, "%s%s%s", prefix, str, suffix)
 * except that it does proper escaping of "str" as necessary. Prefix and suffix should be compile-
 * time constants, or values not needing escaping.
 * Drops any invalid characters we don't support
 */
static inline int
__json_format_str(char *buf, const int len, const char *prefix, const char *str, const char *suffix)
{
	int ret;
	char saved[4] = "";
	char *tmp;

	if (strnlen(buf, sizeof(saved)) < sizeof(saved)) {
		/* we have only a few bytes in buffer, so save them off to restore on error*/
		strcpy(saved, buf);
		ret = __json_format_str_to_buf(buf, len, prefix, str, suffix);
		if (ret == 0)
			strcpy(buf, saved); /* restore */
		return ret;
	}

	tmp = malloc(len);
	if (tmp == NULL)
		return 0;

	ret = __json_format_str_to_buf(tmp, len, prefix, str, suffix);
	if (ret > 0)
		strcpy(buf, tmp);

	free(tmp);
	return ret;
}

/* Copies an empty array into the provided buffer. */
static inline int
rte_tel_json_empty_array(char *buf, const int len, const int used)
{
	return used + __json_snprintf(buf + used, len - used, "[]");
}

/* Copies an empty object into the provided buffer. */
static inline int
rte_tel_json_empty_obj(char *buf, const int len, const int used)
{
	return used + __json_snprintf(buf + used, len - used, "{}");
}

/* Copies a string into the provided buffer, in JSON format. */
static inline int
rte_tel_json_str(char *buf, const int len, const int used, const char *str)
{
	return used + __json_format_str(buf + used, len - used, "\"", str, "\"");
}

/* Appends a string into the JSON array in the provided buffer. */
static inline int
rte_tel_json_add_array_string(char *buf, const int len, const int used,
		const char *str)
{
	int ret, end = used - 1; /* strip off final delimiter */
	if (used <= 2) /* assume empty, since minimum is '[]' */
		return __json_format_str(buf, len, "[\"", str, "\"]");

	ret = __json_format_str(buf + end, len - end, ",\"", str, "\"]");
	return ret == 0 ? used : end + ret;
}

/* Appends an integer into the JSON array in the provided buffer. */
static inline int
rte_tel_json_add_array_int(char *buf, const int len, const int used, int64_t val)
{
	int ret, end = used - 1; /* strip off final delimiter */
	if (used <= 2) /* assume empty, since minimum is '[]' */
		return __json_snprintf(buf, len, "[%"PRId64"]", val);

	ret = __json_snprintf(buf + end, len - end, ",%"PRId64"]", val);
	return ret == 0 ? used : end + ret;
}

/* Appends a uint64_t into the JSON array in the provided buffer. */
static inline int
rte_tel_json_add_array_uint(char *buf, const int len, const int used,
		uint64_t val)
{
	int ret, end = used - 1; /* strip off final delimiter */
	if (used <= 2) /* assume empty, since minimum is '[]' */
		return __json_snprintf(buf, len, "[%"PRIu64"]", val);

	ret = __json_snprintf(buf + end, len - end, ",%"PRIu64"]", val);
	return ret == 0 ? used : end + ret;
}

/*
 * Add a new element with raw JSON value to the JSON array stored in the
 * provided buffer.
 */
static inline int
rte_tel_json_add_array_json(char *buf, const int len, const int used,
		const char *str)
{
	int ret, end = used - 1; /* strip off final delimiter */
	if (used <= 2) /* assume empty, since minimum is '[]' */
		return __json_snprintf(buf, len, "[%s]", str);

	ret = __json_snprintf(buf + end, len - end, ",%s]", str);
	return ret == 0 ? used : end + ret;
}

/**
 * Add a new element with uint64_t value to the JSON object stored in the
 * provided buffer.
 */
static inline int
rte_tel_json_add_obj_uint(char *buf, const int len, const int used,
		const char *name, uint64_t val)
{
	int ret, end = used - 1;
	if (used <= 2) /* assume empty, since minimum is '{}' */
		return __json_snprintf(buf, len, "{\"%s\":%"PRIu64"}", name,
				val);

	ret = __json_snprintf(buf + end, len - end, ",\"%s\":%"PRIu64"}",
			name, val);
	return ret == 0 ? used : end + ret;
}

/**
 * Add a new element with int value to the JSON object stored in the
 * provided buffer.
 */
static inline int
rte_tel_json_add_obj_int(char *buf, const int len, const int used,
		const char *name, int64_t val)
{
	int ret, end = used - 1;
	if (used <= 2) /* assume empty, since minimum is '{}' */
		return __json_snprintf(buf, len, "{\"%s\":%"PRId64"}", name,
				val);

	ret = __json_snprintf(buf + end, len - end, ",\"%s\":%"PRId64"}",
			name, val);
	return ret == 0 ? used : end + ret;
}

/**
 * Add a new element with string value to the JSON object stored in the
 * provided buffer.
 */
static inline int
rte_tel_json_add_obj_str(char *buf, const int len, const int used,
		const char *name, const char *val)
{
	char tmp_name[RTE_TEL_MAX_STRING_LEN + 5];
	int ret, end = used - 1;

	/* names are limited to certain characters so need no escaping */
	snprintf(tmp_name, sizeof(tmp_name), "{\"%s\":\"", name);
	if (used <= 2) /* assume empty, since minimum is '{}' */
		return __json_format_str(buf, len, tmp_name, val, "\"}");

	tmp_name[0] = ',';  /* replace '{' with ',' at start */
	ret = __json_format_str(buf + end, len - end, tmp_name, val, "\"}");
	return ret == 0 ? used : end + ret;
}

/**
 * Add a new element with raw JSON value to the JSON object stored in the
 * provided buffer.
 */
static inline int
rte_tel_json_add_obj_json(char *buf, const int len, const int used,
		const char *name, const char *val)
{
	int ret, end = used - 1;
	if (used <= 2) /* assume empty, since minimum is '{}' */
		return __json_snprintf(buf, len, "{\"%s\":%s}", name, val);

	ret = __json_snprintf(buf + end, len - end, ",\"%s\":%s}",
			name, val);
	return ret == 0 ? used : end + ret;
}

#endif /*_RTE_TELEMETRY_JSON_H_*/
