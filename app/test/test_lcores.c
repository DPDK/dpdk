/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Red Hat, Inc.
 */

#include <string.h>

#include <rte_common.h>
#include <rte_errno.h>
#include <rte_lcore.h>
#include <rte_thread.h>

#include "test.h"

struct thread_context {
	enum { Thread_INIT, Thread_ERROR, Thread_DONE } state;
	bool lcore_id_any;
	rte_thread_t id;
	unsigned int *registered_count;
};

static uint32_t thread_loop(void *arg)
{
	struct thread_context *t = arg;
	unsigned int lcore_id;

	lcore_id = rte_lcore_id();
	if (lcore_id != LCORE_ID_ANY) {
		printf("Error: incorrect lcore id for new thread %u\n", lcore_id);
		t->state = Thread_ERROR;
	}
	if (rte_thread_register() < 0)
		printf("Warning: could not register new thread (this might be expected during this test), reason %s\n",
			rte_strerror(rte_errno));
	lcore_id = rte_lcore_id();
	if ((t->lcore_id_any && lcore_id != LCORE_ID_ANY) ||
			(!t->lcore_id_any && lcore_id == LCORE_ID_ANY)) {
		printf("Error: could not register new thread, got %u while %sexpecting %u\n",
			lcore_id, t->lcore_id_any ? "" : "not ", LCORE_ID_ANY);
		t->state = Thread_ERROR;
	}
	/* Report register happened to the control thread. */
	__atomic_fetch_add(t->registered_count, 1, __ATOMIC_RELEASE);

	/* Wait for release from the control thread. */
	while (__atomic_load_n(t->registered_count, __ATOMIC_ACQUIRE) != 0)
		;
	rte_thread_unregister();
	lcore_id = rte_lcore_id();
	if (lcore_id != LCORE_ID_ANY) {
		printf("Error: could not unregister new thread, %u still assigned\n",
			lcore_id);
		t->state = Thread_ERROR;
	}

	if (t->state != Thread_ERROR)
		t->state = Thread_DONE;

	return 0;
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
		t->state = Thread_INIT;
		t->registered_count = &registered_count;
		t->lcore_id_any = false;
		if (rte_thread_create(&t->id, NULL, thread_loop, t) != 0)
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
	t->state = Thread_INIT;
	t->registered_count = &registered_count;
	t->lcore_id_any = true;
	if (rte_thread_create(&t->id, NULL, thread_loop, t) == 0) {
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
		rte_thread_join(t->id, NULL);
		if (t->state != Thread_DONE)
			ret = -1;
	}

	return ret;
}

struct limit_lcore_context {
	unsigned int init;
	unsigned int max;
	unsigned int uninit;
};

static int
limit_lcores_init(unsigned int lcore_id __rte_unused, void *arg)
{
	struct limit_lcore_context *l = arg;

	l->init++;
	if (l->init > l->max)
		return -1;
	return 0;
}

static void
limit_lcores_uninit(unsigned int lcore_id __rte_unused, void *arg)
{
	struct limit_lcore_context *l = arg;

	l->uninit++;
}

static int
test_lcores_callback(unsigned int eal_threads_count)
{
	struct limit_lcore_context l;
	void *handle;

	/* Refuse last lcore => callback register error. */
	memset(&l, 0, sizeof(l));
	l.max = eal_threads_count - 1;
	handle = rte_lcore_callback_register("limit", limit_lcores_init,
		limit_lcores_uninit, &l);
	if (handle != NULL) {
		printf("Error: lcore callback register should have failed\n");
		goto error;
	}
	/* Refusal happens at the n th call to the init callback.
	 * Besides, n - 1 were accepted, so we expect as many uninit calls when
	 * the rollback happens.
	 */
	if (l.init != eal_threads_count) {
		printf("Error: lcore callback register failed but incorrect init calls, expected %u, got %u\n",
			eal_threads_count, l.init);
		goto error;
	}
	if (l.uninit != eal_threads_count - 1) {
		printf("Error: lcore callback register failed but incorrect uninit calls, expected %u, got %u\n",
			eal_threads_count - 1, l.uninit);
		goto error;
	}

	/* Accept all lcore and unregister. */
	memset(&l, 0, sizeof(l));
	l.max = eal_threads_count;
	handle = rte_lcore_callback_register("limit", limit_lcores_init,
		limit_lcores_uninit, &l);
	if (handle == NULL) {
		printf("Error: lcore callback register failed\n");
		goto error;
	}
	if (l.uninit != 0) {
		printf("Error: lcore callback register succeeded but incorrect uninit calls, expected 0, got %u\n",
			l.uninit);
		goto error;
	}
	rte_lcore_callback_unregister(handle);
	handle = NULL;
	if (l.init != eal_threads_count) {
		printf("Error: lcore callback unregister done but incorrect init calls, expected %u, got %u\n",
			eal_threads_count, l.init);
		goto error;
	}
	if (l.uninit != eal_threads_count) {
		printf("Error: lcore callback unregister done but incorrect uninit calls, expected %u, got %u\n",
			eal_threads_count, l.uninit);
		goto error;
	}

	return 0;

error:
	if (handle != NULL)
		rte_lcore_callback_unregister(handle);

	return -1;
}

static int
test_non_eal_lcores_callback(unsigned int eal_threads_count)
{
	struct thread_context thread_contexts[2];
	unsigned int non_eal_threads_count = 0;
	struct limit_lcore_context l[2] = {};
	unsigned int registered_count = 0;
	struct thread_context *t;
	void *handle[2] = {};
	unsigned int i;
	int ret;

	/* This test requires two empty slots to be sure lcore init refusal is
	 * because of callback execution.
	 */
	if (eal_threads_count + 2 >= RTE_MAX_LCORE)
		return 0;

	/* Register two callbacks:
	 * - first one accepts any lcore,
	 * - second one accepts all EAL lcore + one more for the first non-EAL
	 *   thread, then refuses the next lcore.
	 */
	l[0].max = UINT_MAX;
	handle[0] = rte_lcore_callback_register("no_limit", limit_lcores_init,
		limit_lcores_uninit, &l[0]);
	if (handle[0] == NULL) {
		printf("Error: lcore callback [0] register failed\n");
		goto error;
	}
	l[1].max = eal_threads_count + 1;
	handle[1] = rte_lcore_callback_register("limit", limit_lcores_init,
		limit_lcores_uninit, &l[1]);
	if (handle[1] == NULL) {
		printf("Error: lcore callback [1] register failed\n");
		goto error;
	}
	if (l[0].init != eal_threads_count || l[1].init != eal_threads_count) {
		printf("Error: lcore callbacks register succeeded but incorrect init calls, expected %u, %u, got %u, %u\n",
			eal_threads_count, eal_threads_count,
			l[0].init, l[1].init);
		goto error;
	}
	if (l[0].uninit != 0 || l[1].uninit != 0) {
		printf("Error: lcore callbacks register succeeded but incorrect uninit calls, expected 0, 1, got %u, %u\n",
			l[0].uninit, l[1].uninit);
		goto error;
	}
	/* First thread that expects a valid lcore id. */
	t = &thread_contexts[0];
	t->state = Thread_INIT;
	t->registered_count = &registered_count;
	t->lcore_id_any = false;
	if (rte_thread_create(&t->id, NULL, thread_loop, t) != 0)
		goto cleanup_threads;
	non_eal_threads_count++;
	while (__atomic_load_n(&registered_count, __ATOMIC_ACQUIRE) !=
			non_eal_threads_count)
		;
	if (l[0].init != eal_threads_count + 1 ||
			l[1].init != eal_threads_count + 1) {
		printf("Error: incorrect init calls, expected %u, %u, got %u, %u\n",
			eal_threads_count + 1, eal_threads_count + 1,
			l[0].init, l[1].init);
		goto cleanup_threads;
	}
	if (l[0].uninit != 0 || l[1].uninit != 0) {
		printf("Error: incorrect uninit calls, expected 0, 0, got %u, %u\n",
			l[0].uninit, l[1].uninit);
		goto cleanup_threads;
	}
	/* Second thread, that expects LCORE_ID_ANY because of init refusal. */
	t = &thread_contexts[1];
	t->state = Thread_INIT;
	t->registered_count = &registered_count;
	t->lcore_id_any = true;
	if (rte_thread_create(&t->id, NULL, thread_loop, t) != 0)
		goto cleanup_threads;
	non_eal_threads_count++;
	while (__atomic_load_n(&registered_count, __ATOMIC_ACQUIRE) !=
			non_eal_threads_count)
		;
	if (l[0].init != eal_threads_count + 2 ||
			l[1].init != eal_threads_count + 2) {
		printf("Error: incorrect init calls, expected %u, %u, got %u, %u\n",
			eal_threads_count + 2, eal_threads_count + 2,
			l[0].init, l[1].init);
		goto cleanup_threads;
	}
	if (l[0].uninit != 1 || l[1].uninit != 0) {
		printf("Error: incorrect uninit calls, expected 1, 0, got %u, %u\n",
			l[0].uninit, l[1].uninit);
		goto cleanup_threads;
	}
	rte_lcore_dump(stdout);
	/* Release all threads, and check their states. */
	__atomic_store_n(&registered_count, 0, __ATOMIC_RELEASE);
	ret = 0;
	for (i = 0; i < non_eal_threads_count; i++) {
		t = &thread_contexts[i];
		rte_thread_join(t->id, NULL);
		if (t->state != Thread_DONE)
			ret = -1;
	}
	if (ret < 0)
		goto error;
	rte_lcore_dump(stdout);
	if (l[0].uninit != 2 || l[1].uninit != 1) {
		printf("Error: threads reported having successfully registered and unregistered, but incorrect uninit calls, expected 2, 1, got %u, %u\n",
			l[0].uninit, l[1].uninit);
		goto error;
	}
	rte_lcore_callback_unregister(handle[0]);
	rte_lcore_callback_unregister(handle[1]);
	return 0;

cleanup_threads:
	/* Release all threads */
	__atomic_store_n(&registered_count, 0, __ATOMIC_RELEASE);
	for (i = 0; i < non_eal_threads_count; i++) {
		t = &thread_contexts[i];
		rte_thread_join(t->id, NULL);
	}
error:
	if (handle[1] != NULL)
		rte_lcore_callback_unregister(handle[1]);
	if (handle[0] != NULL)
		rte_lcore_callback_unregister(handle[0]);
	return -1;
}

static uint32_t ctrl_thread_loop(void *arg)
{
	struct thread_context *t = arg;

	printf("Control thread running successfully\n");

	/* Set the thread state to DONE */
	t->state = Thread_DONE;

	return 0;
}

static int
test_ctrl_thread(void)
{
	struct thread_context ctrl_thread_context;
	struct thread_context *t;

	/* Create one control thread */
	t = &ctrl_thread_context;
	t->state = Thread_INIT;
	if (rte_thread_create_control(&t->id, "dpdk-test-ctrlt",
				ctrl_thread_loop, t) != 0)
		return -1;

	/* Wait till the control thread exits.
	 * This also acts as the barrier such that the memory operations
	 * in control thread are visible to this thread.
	 */
	rte_thread_join(t->id, NULL);

	/* Check if the control thread set the correct state */
	if (t->state != Thread_DONE)
		return -1;

	return 0;
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
	rte_lcore_dump(stdout);

	if (test_non_eal_lcores(eal_threads_count) < 0)
		return TEST_FAILED;

	if (test_lcores_callback(eal_threads_count) < 0)
		return TEST_FAILED;

	if (test_non_eal_lcores_callback(eal_threads_count) < 0)
		return TEST_FAILED;

	if (test_ctrl_thread() < 0)
		return TEST_FAILED;

	return TEST_SUCCESS;
}

REGISTER_FAST_TEST(lcores_autotest, true, true, test_lcores);
