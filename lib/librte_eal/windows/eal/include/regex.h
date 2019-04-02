/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _REGEX_H_
#define _REGEX_H_

/**
 * This file is required to support the common code in eal_common_log.c
 * as Microsoft libc does not contain regex.h. This may be removed in
 * future releases.
 */
#ifdef __cplusplus
extern "C" {
#endif

#define REG_NOMATCH 1
#define REG_ESPACE 12

#include <rte_common.h>

/* defining regex_t for Windows */
typedef void *regex_t;
/* defining regmatch_t for Windows */
typedef void *regmatch_t;

/**
 * The regcomp() function will compile the regular expression
 * contained in the string pointed to by the pattern argument
 * and place the results in the structure pointed to by preg.
 * The cflags argument is the bitwise inclusive OR of zero or
 * more of the flags
 */
static inline int regcomp(__rte_unused regex_t *preg,
		__rte_unused const char *regex, __rte_unused int cflags)
{
	/* TODO */
	/* This is a stub, not the expected result */
	return REG_ESPACE;
}

/**
 * The regexec() function compares the null-terminated string
 * specified by string with the compiled regular expression
 * preg initialised by a previous call to regcomp(). If it finds
 * a match, regexec() returns 0; otherwise it returns non-zero
 * indicating either no match or an error. The eflags argument
 * is the bitwise inclusive OR of zero or more of the flags.
 */
static inline int regexec(__rte_unused const regex_t *preg,
		__rte_unused const char *string, __rte_unused size_t nmatch,
		__rte_unused regmatch_t pmatch[], __rte_unused int eflags)
{
	/* TODO */
	/* This is a stub, not the expected result */
	return REG_NOMATCH;
}

/**
 * The regerror() function provides a mapping from error codes
 * returned by regcomp() and regexec() to unspecified printable strings.
 */
static inline size_t regerror(__rte_unused int errcode,
		__rte_unused const regex_t *preg, char *errbuf,
		__rte_unused size_t errbuf_size)
{
	/* TODO */
	/* This is a stub, not the expected result */
	if (errbuf) {
		*errbuf = '\0';
		return 1;
	}
	return 0;
}

/**
 * The regfree() function frees any memory allocated by regcomp()
 * associated with preg.
 */
static inline void regfree(__rte_unused regex_t *preg)
{
	/* TODO */
	/* This is a stub, not the expected result */
}

#ifdef __cplusplus
}
#endif

#endif /* _REGEX_H_ */
