/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Intel Corporation
 */

#include <string.h>
#include <stdlib.h>
#include <rte_string_fns.h>

size_t
rte_basename(const char *path, char *buf, size_t buflen)
{
	char fname[_MAX_FNAME + 1];
	char ext[_MAX_EXT + 1];
	char dir[_MAX_DIR + 1];

	if (path == NULL || path[0] == '\0')
		return (buf == NULL) ? strlen(".") : strlcpy(buf, ".", buflen);

	/* basename is on the end, so if path is too long, use only last PATH_MAX bytes */
	const size_t pathlen = strlen(path);
	if (pathlen > _MAX_PATH)
		path = &path[pathlen - _MAX_PATH];


	/* Use _splitpath_s to separate the path into components */
	int ret = _splitpath_s(path, NULL, 0, dir, sizeof(dir),
			fname, sizeof(fname), ext, sizeof(ext));
	if (ret != 0)
		return (size_t)-1;

	/* if there is a trailing slash, then split_path returns no basename, but
	 * we want to return the last component of the path in all cases.
	 * Therefore re-run removing trailing slash from path.
	 */
	if (fname[0] == '\0' && ext[0] == '\0') {
		size_t dirlen = strlen(dir);
		while (dirlen > 0 && (dir[dirlen - 1] == '\\' || dir[dirlen - 1] == '/')) {
			/* special case for "/" to keep *nix compatibility */
			if (strcmp(dir, "/") == 0)
				return (buf == NULL) ? strlen(dir) : strlcpy(buf, dir, buflen);

			/* Remove trailing backslash */
			dir[--dirlen] = '\0';
		}
		_splitpath_s(dir, NULL, 0, NULL, 0, fname, sizeof(fname), ext, sizeof(ext));
	}

	if (buf == NULL)
		return strlen(fname) + strlen(ext);

	/* Combine the filename and extension into output */
	return snprintf(buf, buflen, "%s%s", fname, ext);
}
