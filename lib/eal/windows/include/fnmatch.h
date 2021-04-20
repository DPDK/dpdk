/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _FNMATCH_H_
#define _FNMATCH_H_

/**
 * This file is required to support the common code in eal_common_log.c
 * as Microsoft libc does not contain fnmatch.h. This may be removed in
 * future releases.
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>

#define FNM_NOMATCH 1

#define FNM_NOESCAPE 0x01
#define FNM_PATHNAME 0x02
#define FNM_PERIOD 0x04
#define FNM_LEADING_DIR 0x08
#define FNM_CASEFOLD 0x10
#define FNM_PREFIX_DIRS 0x20

/**
 * This function is used for searhing a given string source
 * with the given regular expression pattern.
 *
 * @param pattern
 *	regular expression notation decribing the pattern to match
 *
 * @param string
 *	source string to searcg for the pattern
 *
 * @param flag
 *	containing information about the pattern
 *
 * @return
 *	if the pattern is found then return 0 or else FNM_NOMATCH
 */
int fnmatch(const char *pattern, const char *string, int flags);

#ifdef __cplusplus
}
#endif

#endif /* _FNMATCH_H_ */
