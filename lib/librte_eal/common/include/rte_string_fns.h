/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

/**
 * @file
 *
 * String-related functions as replacement for libc equivalents
 */

#ifndef _RTE_STRING_FNS_H_
#define _RTE_STRING_FNS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

/**
 * Takes string "string" parameter and splits it at character "delim"
 * up to maxtokens-1 times - to give "maxtokens" resulting tokens. Like
 * strtok or strsep functions, this modifies its input string, by replacing
 * instances of "delim" with '\\0'. All resultant tokens are returned in the
 * "tokens" array which must have enough entries to hold "maxtokens".
 *
 * @param string
 *   The input string to be split into tokens
 *
 * @param stringlen
 *   The max length of the input buffer
 *
 * @param tokens
 *   The array to hold the pointers to the tokens in the string
 *
 * @param maxtokens
 *   The number of elements in the tokens array. At most, maxtokens-1 splits
 *   of the string will be done.
 *
 * @param delim
 *   The character on which the split of the data will be done
 *
 * @return
 *   The number of tokens in the tokens array.
 */
int
rte_strsplit(char *string, int stringlen,
             char **tokens, int maxtokens, char delim);

/**
 * @internal
 * DPDK-specific version of strlcpy for systems without
 * libc or libbsd copies of the function
 */
static inline size_t
rte_strlcpy(char *dst, const char *src, size_t size)
{
	return (size_t)snprintf(dst, size, "%s", src);
}

/* pull in a strlcpy function */
#ifdef RTE_EXEC_ENV_BSDAPP
#include <string.h>
#ifndef __BSD_VISIBLE /* non-standard functions are hidden */
#define strlcpy(dst, src, size) rte_strlcpy(dst, src, size)
#endif


#else /* non-BSD platforms */
#ifdef RTE_USE_LIBBSD
#include <bsd/string.h>

#else /* no BSD header files, create own */
#define strlcpy(dst, src, size) rte_strlcpy(dst, src, size)

#endif /* RTE_USE_LIBBSD */
#endif /* BSDAPP */

#ifdef __cplusplus
}
#endif

#endif /* RTE_STRING_FNS_H */
