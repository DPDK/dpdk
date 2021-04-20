/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Dieter Baron and Thomas Klausner.
 */

/**
 * @file
 * getopt compat.
 *
 * This module provides getopt() and getopt_long().
 */

#ifndef _USUAL_GETOPT_H_
#define _USUAL_GETOPT_H_

#ifndef NEED_USUAL_GETOPT
#if !defined(HAVE_GETOPT_H) || !defined(HAVE_GETOPT) || \
	!defined(HAVE_GETOPT_LONG)
#define NEED_USUAL_GETOPT
#endif
#endif

#ifndef NEED_USUAL_GETOPT

/* Use system getopt */
#ifdef RTE_TOOLCHAIN_GCC
#include_next <getopt.h>
#else
#include <getopt.h>
#endif

#else /* NEED_USUAL_GETOPT */

/* avoid name collision */
#define optarg usual_optarg
#define opterr usual_opterr
#define optind usual_optind
#define optopt usual_optopt
#define getopt(a, b, c) usual_getopt(a, b, c)
#define getopt_long(a, b, c, d, e) usual_getopt_long(a, b, c, d, e)


/** argument to current option, or NULL if it has none */
extern const char *optarg;
/** Current position in arg string.  Starts from 1.
 * Setting to 0 resets state.
 */
extern int optind;
/** whether getopt() should print error messages on problems.  Default: 1. */
extern int opterr;
/** Option char which caused error */
extern int optopt;

/** long option takes no argument */
#define no_argument        0
/** long option requires argument */
#define required_argument  1
/** long option has optional argument */
#define optional_argument  2

/** Long option description */
struct option {
	/** name of long option */
	const char *name;

	/**
	 * whether option takes an argument.
	 * One of no_argument, required_argument, and optional_argument.
	 */
	int has_arg;

	/** if not NULL, set *flag to val when option found */
	int *flag;

	/** if flag not NULL, value to set *flag to; else return value */
	int val;
};

/** Compat: getopt */
int getopt(int argc, char *argv[], const char *options);

/** Compat: getopt_long */
int getopt_long(int argc, char *argv[], const char *options,
		const struct option *longopts, int *longindex);

/** Compat: getopt_long_only */
int getopt_long_only(int nargc, char *argv[], const char *options,
		     const struct option *long_options, int *idx);


#endif /* NEED_USUAL_GETOPT */

#endif /* !_USUAL_GETOPT_H_ */
