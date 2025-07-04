/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Guido van Rossum.
 */
#ifndef _FNMATCH_H_
#define _FNMATCH_H_

/*
 * Function fnmatch() as specified in POSIX 1003.2-1992, section B.6.
 * Compares a filename or pathname to a pattern.
 */

#include <ctype.h>
#include <string.h>
#include <stdio.h>

#define FNM_NOMATCH 1

#define FNM_NOESCAPE 0x01
#define FNM_PATHNAME 0x02
#define FNM_PERIOD 0x04
#define FNM_LEADING_DIR 0x08
#define FNM_CASEFOLD 0x10
#define FNM_PREFIX_DIRS 0x20

#define FNM_EOS	'\0'

static inline const char *
fnm_rangematch(const char *pattern, char test, int flags)
{
	int negate, ok;
	char c, c2;

	/*
	 * A bracket expression starting with an unquoted circumflex
	 * character produces unspecified results (IEEE 1003.2-1992,
	 * 3.13.2).  This implementation treats it like '!', for
	 * consistency with the regular expression syntax.
	 * J.T. Conklin (conklin@ngai.kaleida.com)
	 */
	negate = (*pattern == '!' || *pattern == '^');
	if (negate)
		++pattern;

	if (flags & FNM_CASEFOLD)
		test = tolower((unsigned char)test);

	for (ok = 0; (c = *pattern++) != ']';) {
		if (c == '\\' && !(flags & FNM_NOESCAPE))
			c = *pattern++;
		if (c == FNM_EOS)
			return (NULL);

		if (flags & FNM_CASEFOLD)
			c = tolower((unsigned char)c);

		c2 = *(pattern + 1);
		if (*pattern == '-' && c2 != FNM_EOS && c2 != ']') {
			pattern += 2;
			if (c2 == '\\' && !(flags & FNM_NOESCAPE))
				c2 = *pattern++;
			if (c2 == FNM_EOS)
				return (NULL);

			if (flags & FNM_CASEFOLD)
				c2 = tolower((unsigned char)c2);

			if ((unsigned char)c <= (unsigned char)test &&
			    (unsigned char)test <= (unsigned char)c2)
				ok = 1;
		} else if (c == test)
			ok = 1;
	}
	return (ok == negate ? NULL : pattern);
}

/**
 * This function is used for searching a given string source
 * with the given regular expression pattern.
 *
 * @param pattern
 *	regular expression notation describing the pattern to match
 *
 * @param string
 *	source string to search for the pattern
 *
 * @param flag
 *	containing information about the pattern
 *
 * @return
 *	if the pattern is found then return 0 or else FNM_NOMATCH
 */
static inline int
fnmatch(const char *pattern, const char *string, int flags)
{
	const char *stringstart;
	char c, test;

	for (stringstart = string;;)
		switch (c = *pattern++) {
		case FNM_EOS:
			if ((flags & FNM_LEADING_DIR) && *string == '/')
				return (0);
			return (*string == FNM_EOS ? 0 : FNM_NOMATCH);
		case '?':
			if (*string == FNM_EOS)
				return (FNM_NOMATCH);
			if (*string == '/' && (flags & FNM_PATHNAME))
				return (FNM_NOMATCH);
			if (*string == '.' && (flags & FNM_PERIOD) &&
			    (string == stringstart ||
			    ((flags & FNM_PATHNAME) && *(string - 1) == '/')))
				return (FNM_NOMATCH);
			++string;
			break;
		case '*':
			c = *pattern;
			/* Collapse multiple stars. */
			while (c == '*')
				c = *++pattern;

			if (*string == '.' && (flags & FNM_PERIOD) &&
			    (string == stringstart ||
			    ((flags & FNM_PATHNAME) && *(string - 1) == '/')))
				return (FNM_NOMATCH);

			/* Optimize for pattern with * at end or before /. */
			if (c == FNM_EOS)
				if (flags & FNM_PATHNAME)
					return ((flags & FNM_LEADING_DIR) ||
					    strchr(string, '/') == NULL ?
					    0 : FNM_NOMATCH);
				else
					return (0);
			else if (c == '/' && flags & FNM_PATHNAME) {
				string = strchr(string, '/');
				if (string == NULL)
					return (FNM_NOMATCH);
				break;
			}

			/* General case, use recursion. */
			while ((test = *string) != FNM_EOS) {
				if (!fnmatch(pattern, string,
					flags & ~FNM_PERIOD))
					return (0);
				if (test == '/' && flags & FNM_PATHNAME)
					break;
				++string;
			}
			return (FNM_NOMATCH);
		case '[':
			if (*string == FNM_EOS)
				return (FNM_NOMATCH);
			if (*string == '/' && flags & FNM_PATHNAME)
				return (FNM_NOMATCH);
			pattern = fnm_rangematch(pattern, *string, flags);
			if (pattern == NULL)
				return (FNM_NOMATCH);
			++string;
			break;
		case '\\':
			if (!(flags & FNM_NOESCAPE)) {
				c = *pattern++;
				if (c == FNM_EOS) {
					c = '\\';
					--pattern;
				}
			}
			/* FALLTHROUGH */
		default:
			if (c == *string)
				;
			else if ((flags & FNM_CASEFOLD) &&
				 (tolower((unsigned char)c) ==
				  tolower((unsigned char)*string)))
				;
			else if ((flags & FNM_PREFIX_DIRS) && *string == FNM_EOS &&
			     ((c == '/' && string != stringstart) ||
			     (string == stringstart+1 && *stringstart == '/')))
				return (0);
			else
				return (FNM_NOMATCH);
			string++;
			break;
		}
	/* NOTREACHED */
}

#endif /* _FNMATCH_H_ */
