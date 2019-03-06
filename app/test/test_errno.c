/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <rte_per_lcore.h>
#include <rte_errno.h>
#include <rte_string_fns.h>

#include "test.h"

static int
test_errno(void)
{
	const char *rte_retval;
	const char *libc_retval;
#ifdef RTE_EXEC_ENV_FREEBSD
	/* BSD has a colon in the string, unlike linux */
	const char unknown_code_result[] = "Unknown error: %d";
#else
	const char unknown_code_result[] = "Unknown error %d";
#endif
	char expected_libc_retval[sizeof(unknown_code_result)+3];

	/* use a small selection of standard errors for testing */
	int std_errs[] = {EAGAIN, EBADF, EACCES, EINTR, EINVAL};
	/* test ALL registered RTE error codes for overlap */
	int rte_errs[] = {E_RTE_SECONDARY, E_RTE_NO_CONFIG};
	unsigned i;

	rte_errno = 0;
	if (rte_errno != 0)
		return -1;
	/* check for standard errors we return the same as libc */
	for (i = 0; i < sizeof(std_errs)/sizeof(std_errs[0]); i++){
		rte_retval = rte_strerror(std_errs[i]);
		libc_retval = strerror(std_errs[i]);
		printf("rte_strerror: '%s', strerror: '%s'\n",
				rte_retval, libc_retval);
		if (strcmp(rte_retval, libc_retval) != 0)
			return -1;
	}
	/* for rte-specific errors ensure we return a different string
	 * and that the string for libc is for an unknown error
	 */
	for (i = 0; i < sizeof(rte_errs)/sizeof(rte_errs[0]); i++){
		rte_retval = rte_strerror(rte_errs[i]);
		libc_retval = strerror(rte_errs[i]);
		printf("rte_strerror: '%s', strerror: '%s'\n",
				rte_retval, libc_retval);
		if (strcmp(rte_retval, libc_retval) == 0)
			return -1;
		/* generate appropriate error string for unknown error number
		 * and then check that this is what we got back. If not, we have
		 * a duplicate error number that conflicts with errno.h */
		snprintf(expected_libc_retval, sizeof(expected_libc_retval),
				unknown_code_result, rte_errs[i]);
		if ((strcmp(expected_libc_retval, libc_retval) != 0) &&
				(strcmp("", libc_retval) != 0)){
			printf("Error, duplicate error code %d\n", rte_errs[i]);
			return -1;
		}
	}

	/* ensure that beyond RTE_MAX_ERRNO, we always get an unknown code */
	rte_retval = rte_strerror(RTE_MAX_ERRNO + 1);
	libc_retval = strerror(RTE_MAX_ERRNO + 1);
	snprintf(expected_libc_retval, sizeof(expected_libc_retval),
			unknown_code_result, RTE_MAX_ERRNO + 1);
	printf("rte_strerror: '%s', strerror: '%s'\n",
			rte_retval, libc_retval);
	if ((strcmp(rte_retval, libc_retval) != 0) ||
			(strcmp(expected_libc_retval, libc_retval) != 0)){
		if (strcmp("", libc_retval) != 0){
			printf("Failed test for RTE_MAX_ERRNO + 1 value\n");
			return -1;
		}
	}

	return 0;
}

REGISTER_TEST_COMMAND(errno_autotest, test_errno);
