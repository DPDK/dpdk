/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Intel Corporation
 */

#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <limits.h>

#include <eal_export.h>
#include <rte_string_fns.h>

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_basename, 25.11)
size_t
rte_basename(const char *path, char *buf, size_t buflen)
{
	char copy[PATH_MAX + 1];
	size_t retval = 0;

	if (path == NULL)
		return (buf == NULL) ? strlen(".") : strlcpy(buf, ".", buflen);

	/* basename is on the end, so if path is too long, use only last PATH_MAX bytes */
	const size_t pathlen = strlen(path);
	if (pathlen > PATH_MAX)
		path = &path[pathlen - PATH_MAX];

	/* make a copy of buffer since basename may modify it */
	strlcpy(copy, path, sizeof(copy));

	/* if passed a null buffer, just return length of basename, otherwise strlcpy it */
	retval = (buf == NULL) ?
			strlen(basename(copy)) :
			strlcpy(buf, basename(copy), buflen);

	return retval;
}
