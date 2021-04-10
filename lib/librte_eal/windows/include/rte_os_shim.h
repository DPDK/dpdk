/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _RTE_OS_SHIM_
#define _RTE_OS_SHIM_

#include <rte_os.h>

/**
 * @file
 * @internal
 * Provides semi-standard OS facilities by convenient names.
 */

#ifndef PATH_MAX
#define PATH_MAX _MAX_PATH
#endif

#define strdup(str) _strdup(str)
#define strtok_r(str, delim, saveptr) strtok_s(str, delim, saveptr)
#define strncasecmp(s1, s2, count) _strnicmp(s1, s2, count)

#define open(path, flags, ...) _open(path, flags, ##__VA_ARGS__)
#define read(fd, buf, n) _read(fd, buf, n)
#define write(fd, buf, n) _write(fd, buf, n)
#define close(fd) _close(fd)
#define unlink(path) _unlink(path)

#define IPVERSION	4

#define IPPROTO_IPIP	4
#define IPPROTO_GRE	47
#ifdef RTE_TOOLCHAIN_GCC
#define IPPROTO_SCTP	132
#endif

#endif /* _RTE_OS_SHIM_ */
