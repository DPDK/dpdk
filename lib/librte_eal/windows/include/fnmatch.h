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

#define FNM_NOMATCH 1

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
static inline int fnmatch(__rte_unused const char *pattern,
		__rte_unused const char *string,
		__rte_unused int flags)
{
	/* TODO */
	/* This is a stub, not the expected result */
	return FNM_NOMATCH;
}

#ifdef __cplusplus
}
#endif

#endif /* _FNMATCH_H_ */
