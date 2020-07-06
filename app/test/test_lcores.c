/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Red Hat, Inc.
 */

#include <pthread.h>
#include <string.h>

#include <rte_errno.h>
#include <rte_lcore.h>

#include "test.h"

struct thread_context {
	enum { INIT, ERROR, DONE } state;
	bool lcore_id_any;
	pthread_t id;
	unsigned int *registered_count;
};

static void *thread_loop(void *arg)
{
	struct thread_context *t = arg;
	unsigned int lcore_id;

	lcore_id = rte_lcore_id();
	if (lcore_id != LCORE_ID_ANY) {
		printf("Error: incorrect lcore id for new thread %u\n", lcore_id);
		t->state = ERROR;
	}
	if (rte_thread_register() < 0)
		printf("Warning: could not register new thread (this might be expected during this test), reason %s\n",
			rte_strerror(rte_errno));
	lcore_id = rte_lcore_id();
	if ((t->lcore_id_any && lcore_id != LCORE_ID_ANY) ||
			(!t->lcore_id_any && lcore_id == LCORE_ID_ANY)) {
		printf("Error: could not register new thread, got %u while %sexpecting %u\n",
			lcore_id, t->lcore_id_any ? "" : "not ", LCORE_ID_ANY);
		t->state = ERROR;
	}
	/* Report register happened to the control thread. */
	__atomic_add_fetch(t->registered_count, 1, __ATOMIC_RELEASE);

	/* Wait for release from the control thread. */
	while (__atomic_load_n(t->registered_count, __ATOMIC_ACQUIRE) != 0)
		;
	rte_thread_unregister();
	lcore_id = rte_lcore_id();
	if (lcore_id != LCORE_ID_ANY) {
		printf("Error: could not unregister new thread, %u still assigned\n",
			lcore_id);
		t->state = ERROR;
	}

	if (t->state != ERROR)
		t->state = DONE;

	return NULL;
}

static int
test_non_eal_lcores(unsigned int eal_threads_count)
{
	struct thread_context thread_contexts[RTE_MAX_LCORE];
	unsigned int non_eal_threads_count;
	unsigned int registered_count;
	struct thread_context *t;
	unsigned int i;
	int ret;

	non_eal_threads_count = 0;
	registered_count = 0;

	/* Try to create as many threads as possible. */
	for (i = 0; i < RTE_MAX_LCORE - eal_threads_count; i++) {
		t = &thread_contexts[i];
		t->state = INIT;
		t->registered_count = &registered_count;
		t->lcore_id_any = false;
		if (pthread_create(&t->id, NULL, thread_loop, t) != 0)
			break;
		non_eal_threads_count++;
	}
	printf("non-EAL threads count: %u\n", non_eal_threads_count);
	/* Wait all non-EAL threads to register. */
	while (__atomic_load_n(&registered_count, __ATOMIC_ACQUIRE) !=
			non_eal_threads_count)
		;

	/* We managed to create the max number of threads, let's try to create
	 * one more. This will allow one more check.
	 */
	if (eal_threads_count + non_eal_threads_count < RTE_MAX_LCORE)
		goto skip_lcore_any;
	t = &thread_contexts[non_eal_threads_count];
	t->state = INIT;
	t->registered_count = &registered_count;
	t->lcore_id_any = true;
	if (pthread_create(&t->id, NULL, thread_loop, t) == 0) {
		non_eal_threads_count++;
		printf("non-EAL threads count: %u\n", non_eal_threads_count);
		while (__atomic_load_n(&registered_count, __ATOMIC_ACQUIRE) !=
				non_eal_threads_count)
			;
	}

skip_lcore_any:
	/* Release all threads, and check their states. */
	__atomic_store_n(&registered_count, 0, __ATOMIC_RELEASE);
	ret = 0;
	for (i = 0; i < non_eal_threads_count; i++) {
		t = &thread_contexts[i];
		pthread_join(t->id, NULL);
		if (t->state != DONE)
			ret = -1;
	}

	return ret;
}

static int
test_lcores(void)
{
	unsigned int eal_threads_count = 0;
	unsigned int i;

	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (!rte_lcore_has_role(i, ROLE_OFF))
			eal_threads_count++;
	}
	if (eal_threads_count == 0) {
		printf("Error: something is broken, no EAL thread detected.\n");
		return TEST_FAILED;
	}
	printf("EAL threads count: %u, RTE_MAX_LCORE=%u\n", eal_threads_count,
		RTE_MAX_LCORE);

	if (test_non_eal_lcores(eal_threads_count) < 0)
		return TEST_FAILED;

	return TEST_SUCCESS;
}

REGISTER_TEST_COMMAND(lcores_autotest, test_lcores);
