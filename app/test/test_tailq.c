/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
 *   All rights reserved.
 * 
 *   Redistribution and use in source and binary forms, with or without 
 *   modification, are permitted provided that the following conditions 
 *   are met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright 
 *       notice, this list of conditions and the following disclaimer in 
 *       the documentation and/or other materials provided with the 
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its 
 *       contributors may be used to endorse or promote products derived 
 *       from this software without specific prior written permission.
 * 
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 *  version: DPDK.L.1.2.3-3
 */

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>

#include <cmdline_parse.h>

#include <rte_string_fns.h>
#include <rte_tailq.h>

#include "test.h"

#define do_return(...) do { \
	printf("Error at %s, line %d: ", __func__, __LINE__); \
	printf(__VA_ARGS__); \
	return 1; \
} while (0)

#define DEFAULT_TAILQ "dummy_q0"

static struct rte_dummy d_elem;

static int
test_tailq_create(void)
{
	struct rte_dummy_head *d_head;
	char name[RTE_TAILQ_NAMESIZE];
	unsigned i;

	/* create a first tailq and check its non-null */
	d_head = RTE_TAILQ_RESERVE(DEFAULT_TAILQ, rte_dummy_head);
	if (d_head == NULL)
		do_return("Error allocating "DEFAULT_TAILQ"\n");

	/* check we can add an item to it
	 */
	TAILQ_INSERT_TAIL(d_head, &d_elem, next);

	/* try allocating dummy_q0 again, and check for failure */
	if (RTE_TAILQ_RESERVE(DEFAULT_TAILQ, rte_dummy_head) != NULL)
		do_return("Error, non-null result returned when attemption to "
				"re-allocate a tailq\n");

	/* now fill up the tailq slots available and check we get an error */
	for (i = 1; i < RTE_MAX_TAILQ; i++){
		rte_snprintf(name, sizeof(name), "dummy_q%u", i);
		if ((d_head = RTE_TAILQ_RESERVE(name, rte_dummy_head)) == NULL)
			break;
	}

	/* check that we had an error return before RTE_MAX_TAILQ */
	if (i == RTE_MAX_TAILQ)
		do_return("Error, we did not have a reservation failure as expected\n");

	return 0;
}

static int
test_tailq_lookup(void)
{
	/* run successful  test - check result is found */
	struct rte_dummy_head *d_head;
	struct rte_dummy *d_ptr;

	d_head = RTE_TAILQ_LOOKUP(DEFAULT_TAILQ, rte_dummy_head);
	if (d_head == NULL)
		do_return("Error with tailq lookup\n");

	TAILQ_FOREACH(d_ptr, d_head, next)
		if (d_ptr != &d_elem)
			do_return("Error with tailq returned from lookup - "
					"expected element not found\n");

	/* now try a bad/error lookup */
	d_head = RTE_TAILQ_LOOKUP("does_not_exist_queue", rte_dummy_head);
	if (d_head != NULL)
		do_return("Error, lookup does not return NULL for bad tailq name\n");

	return 0;
}

int
test_tailq(void)
{
	int ret = 0;
	ret |= test_tailq_create();
	ret |= test_tailq_lookup();
	return ret;
}
