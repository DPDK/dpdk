/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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
 */

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_string_fns.h>

#include "test.h"

#define do_return(...) do { \
	printf("Error at %s, line %d: ", __func__, __LINE__); \
	printf(__VA_ARGS__); \
	return 1; \
} while (0)

static struct rte_tailq_elem rte_dummy_tailq = {
	.name = "dummy",
};
EAL_REGISTER_TAILQ(rte_dummy_tailq)

static struct rte_tailq_elem rte_dummy_dyn_tailq = {
	.name = "dummy_dyn",
};
static struct rte_tailq_elem rte_dummy_dyn2_tailq = {
	.name = "dummy_dyn",
};

static struct rte_tailq_entry d_elem;
static struct rte_tailq_entry d_dyn_elem;

static int
test_tailq_early(void)
{
	struct rte_tailq_entry_head *d_head;

	d_head = RTE_TAILQ_CAST(rte_dummy_tailq.head, rte_tailq_entry_head);
	if (d_head == NULL)
		do_return("Error %s has not been initialised\n",
			  rte_dummy_tailq.name);

	/* check we can add an item to it */
	TAILQ_INSERT_TAIL(d_head, &d_elem, next);

	return 0;
}

static int
test_tailq_create(void)
{
	struct rte_tailq_entry_head *d_head;

	/* create a tailq and check its non-null (since we are post-eal init) */
	if ((rte_eal_tailq_register(&rte_dummy_dyn_tailq) < 0) ||
	    (rte_dummy_dyn_tailq.head == NULL))
		do_return("Error allocating %s\n", rte_dummy_dyn_tailq.name);

	d_head = RTE_TAILQ_CAST(rte_dummy_dyn_tailq.head, rte_tailq_entry_head);

	/* check we can add an item to it */
	TAILQ_INSERT_TAIL(d_head, &d_dyn_elem, next);

	if (strcmp(rte_dummy_dyn2_tailq.name, rte_dummy_dyn_tailq.name))
		do_return("Error, something is wrong in the tailq test\n");

	/* try allocating again, and check for failure */
	if (!rte_eal_tailq_register(&rte_dummy_dyn2_tailq))
		do_return("Error, registering the same tailq %s did not fail\n",
			  rte_dummy_dyn2_tailq.name);

	return 0;
}

static int
test_tailq_lookup(void)
{
	/* run successful  test - check result is found */
	struct rte_tailq_entry_head *d_head;
	struct rte_tailq_entry *d_ptr;

	d_head = RTE_TAILQ_LOOKUP(rte_dummy_tailq.name, rte_tailq_entry_head);
	/* rte_dummy_tailq has been registered by EAL_REGISTER_TAILQ */
	if (d_head == NULL ||
	    d_head != RTE_TAILQ_CAST(rte_dummy_tailq.head, rte_tailq_entry_head))
		do_return("Error with tailq lookup\n");

	TAILQ_FOREACH(d_ptr, d_head, next)
		if (d_ptr != &d_elem)
			do_return("Error with tailq returned from lookup - "
					"expected element not found\n");

	d_head = RTE_TAILQ_LOOKUP(rte_dummy_dyn_tailq.name, rte_tailq_entry_head);
	/* rte_dummy_dyn_tailq has been registered by test_tailq_create */
	if (d_head == NULL ||
	    d_head != RTE_TAILQ_CAST(rte_dummy_dyn_tailq.head, rte_tailq_entry_head))
		do_return("Error with tailq lookup\n");

	TAILQ_FOREACH(d_ptr, d_head, next)
		if (d_ptr != &d_dyn_elem)
			do_return("Error with tailq returned from lookup - "
					"expected element not found\n");

	/* now try a bad/error lookup */
	d_head = RTE_TAILQ_LOOKUP("coucou", rte_tailq_entry_head);
	if (d_head != NULL)
		do_return("Error, lookup does not return NULL for bad tailq name\n");

	return 0;
}

static int
test_tailq(void)
{
	int ret = 0;
	ret |= test_tailq_early();
	ret |= test_tailq_create();
	ret |= test_tailq_lookup();
	return ret;
}

static struct test_command tailq_cmd = {
	.command = "tailq_autotest",
	.callback = test_tailq,
};
REGISTER_TEST_COMMAND(tailq_cmd);
