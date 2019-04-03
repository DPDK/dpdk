/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <string.h>

#include <rte_atomic.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_stack.h>

#include "test.h"

#define STACK_SIZE 4096
#define MAX_BULK 32

static int
test_stack_push_pop(struct rte_stack *s, void **obj_table, unsigned int bulk_sz)
{
	unsigned int i, ret;
	void **popped_objs;

	popped_objs = rte_calloc(NULL, STACK_SIZE, sizeof(void *), 0);
	if (popped_objs == NULL) {
		printf("[%s():%u] failed to calloc %zu bytes\n",
		       __func__, __LINE__, STACK_SIZE * sizeof(void *));
		return -1;
	}

	for (i = 0; i < STACK_SIZE; i += bulk_sz) {
		ret = rte_stack_push(s, &obj_table[i], bulk_sz);

		if (ret != bulk_sz) {
			printf("[%s():%u] push returned: %d (expected %u)\n",
			       __func__, __LINE__, ret, bulk_sz);
			rte_free(popped_objs);
			return -1;
		}

		if (rte_stack_count(s) != i + bulk_sz) {
			printf("[%s():%u] stack count: %u (expected %u)\n",
			       __func__, __LINE__, rte_stack_count(s),
			       i + bulk_sz);
			rte_free(popped_objs);
			return -1;
		}

		if (rte_stack_free_count(s) != STACK_SIZE - i - bulk_sz) {
			printf("[%s():%u] stack free count: %u (expected %u)\n",
			       __func__, __LINE__, rte_stack_count(s),
			       STACK_SIZE - i - bulk_sz);
			rte_free(popped_objs);
			return -1;
		}
	}

	for (i = 0; i < STACK_SIZE; i += bulk_sz) {
		ret = rte_stack_pop(s, &popped_objs[i], bulk_sz);

		if (ret != bulk_sz) {
			printf("[%s():%u] pop returned: %d (expected %u)\n",
			       __func__, __LINE__, ret, bulk_sz);
			rte_free(popped_objs);
			return -1;
		}

		if (rte_stack_count(s) != STACK_SIZE - i - bulk_sz) {
			printf("[%s():%u] stack count: %u (expected %u)\n",
			       __func__, __LINE__, rte_stack_count(s),
			       STACK_SIZE - i - bulk_sz);
			rte_free(popped_objs);
			return -1;
		}

		if (rte_stack_free_count(s) != i + bulk_sz) {
			printf("[%s():%u] stack free count: %u (expected %u)\n",
			       __func__, __LINE__, rte_stack_count(s),
			       i + bulk_sz);
			rte_free(popped_objs);
			return -1;
		}
	}

	for (i = 0; i < STACK_SIZE; i++) {
		if (obj_table[i] != popped_objs[STACK_SIZE - i - 1]) {
			printf("[%s():%u] Incorrect value %p at index 0x%x\n",
			       __func__, __LINE__,
			       popped_objs[STACK_SIZE - i - 1], i);
			rte_free(popped_objs);
			return -1;
		}
	}

	rte_free(popped_objs);

	return 0;
}

static int
test_stack_basic(uint32_t flags)
{
	struct rte_stack *s = NULL;
	void **obj_table = NULL;
	int i, ret = -1;

	obj_table = rte_calloc(NULL, STACK_SIZE, sizeof(void *), 0);
	if (obj_table == NULL) {
		printf("[%s():%u] failed to calloc %zu bytes\n",
		       __func__, __LINE__, STACK_SIZE * sizeof(void *));
		goto fail_test;
	}

	for (i = 0; i < STACK_SIZE; i++)
		obj_table[i] = (void *)(uintptr_t)i;

	s = rte_stack_create(__func__, STACK_SIZE, rte_socket_id(), flags);
	if (s == NULL) {
		printf("[%s():%u] failed to create a stack\n",
		       __func__, __LINE__);
		goto fail_test;
	}

	if (rte_stack_lookup(__func__) != s) {
		printf("[%s():%u] failed to lookup a stack\n",
		       __func__, __LINE__);
		goto fail_test;
	}

	if (rte_stack_count(s) != 0) {
		printf("[%s():%u] stack count: %u (expected 0)\n",
		       __func__, __LINE__, rte_stack_count(s));
		goto fail_test;
	}

	if (rte_stack_free_count(s) != STACK_SIZE) {
		printf("[%s():%u] stack free count: %u (expected %u)\n",
		       __func__, __LINE__, rte_stack_count(s), STACK_SIZE);
		goto fail_test;
	}

	ret = test_stack_push_pop(s, obj_table, 1);
	if (ret) {
		printf("[%s():%u] Single object push/pop failed\n",
		       __func__, __LINE__);
		goto fail_test;
	}

	ret = test_stack_push_pop(s, obj_table, MAX_BULK);
	if (ret) {
		printf("[%s():%u] Bulk object push/pop failed\n",
		       __func__, __LINE__);
		goto fail_test;
	}

	ret = rte_stack_push(s, obj_table, 2 * STACK_SIZE);
	if (ret != 0) {
		printf("[%s():%u] Excess objects push succeeded\n",
		       __func__, __LINE__);
		goto fail_test;
	}

	ret = rte_stack_pop(s, obj_table, 1);
	if (ret != 0) {
		printf("[%s():%u] Empty stack pop succeeded\n",
		       __func__, __LINE__);
		goto fail_test;
	}

	ret = 0;

fail_test:
	rte_stack_free(s);

	rte_free(obj_table);

	return ret;
}

static int
test_stack_name_reuse(uint32_t flags)
{
	struct rte_stack *s[2];

	s[0] = rte_stack_create("test", STACK_SIZE, rte_socket_id(), flags);
	if (s[0] == NULL) {
		printf("[%s():%u] Failed to create a stack\n",
		       __func__, __LINE__);
		return -1;
	}

	s[1] = rte_stack_create("test", STACK_SIZE, rte_socket_id(), flags);
	if (s[1] != NULL) {
		printf("[%s():%u] Failed to detect re-used name\n",
		       __func__, __LINE__);
		return -1;
	}

	rte_stack_free(s[0]);

	return 0;
}

static int
test_stack_name_length(uint32_t flags)
{
	char name[RTE_STACK_NAMESIZE + 1];
	struct rte_stack *s;

	memset(name, 's', sizeof(name));
	name[RTE_STACK_NAMESIZE] = '\0';

	s = rte_stack_create(name, STACK_SIZE, rte_socket_id(), flags);
	if (s != NULL) {
		printf("[%s():%u] Failed to prevent long name\n",
		       __func__, __LINE__);
		return -1;
	}

	if (rte_errno != ENAMETOOLONG) {
		printf("[%s():%u] rte_stack failed to set correct errno on failed lookup\n",
		       __func__, __LINE__);
		return -1;
	}

	return 0;
}

static int
test_lookup_null(void)
{
	struct rte_stack *s = rte_stack_lookup("stack_not_found");

	if (s != NULL) {
		printf("[%s():%u] rte_stack found a non-existent stack\n",
		       __func__, __LINE__);
		return -1;
	}

	if (rte_errno != ENOENT) {
		printf("[%s():%u] rte_stack failed to set correct errno on failed lookup\n",
		       __func__, __LINE__);
		return -1;
	}

	s = rte_stack_lookup(NULL);

	if (s != NULL) {
		printf("[%s():%u] rte_stack found a non-existent stack\n",
		       __func__, __LINE__);
		return -1;
	}

	if (rte_errno != EINVAL) {
		printf("[%s():%u] rte_stack failed to set correct errno on failed lookup\n",
		       __func__, __LINE__);
		return -1;
	}

	return 0;
}

static int
test_free_null(void)
{
	/* Check whether the library proper handles a NULL pointer */
	rte_stack_free(NULL);

	return 0;
}

#define NUM_ITERS_PER_THREAD 100000

struct test_args {
	struct rte_stack *s;
	rte_atomic64_t *sz;
};

static int
stack_thread_push_pop(void *args)
{
	struct test_args *t = args;
	void **obj_table;
	int i;

	obj_table = rte_calloc(NULL, STACK_SIZE, sizeof(void *), 0);
	if (obj_table == NULL) {
		printf("[%s():%u] failed to calloc %zu bytes\n",
		       __func__, __LINE__, STACK_SIZE * sizeof(void *));
		return -1;
	}

	for (i = 0; i < NUM_ITERS_PER_THREAD; i++) {
		unsigned int success, num;

		/* Reserve up to min(MAX_BULK, available slots) stack entries,
		 * then push and pop those stack entries.
		 */
		do {
			uint64_t sz = rte_atomic64_read(t->sz);
			volatile uint64_t *sz_addr;

			sz_addr = (volatile uint64_t *)t->sz;

			num = RTE_MIN(rte_rand() % MAX_BULK, STACK_SIZE - sz);

			success = rte_atomic64_cmpset(sz_addr, sz, sz + num);
		} while (success == 0);

		if (rte_stack_push(t->s, obj_table, num) != num) {
			printf("[%s():%u] Failed to push %u pointers\n",
			       __func__, __LINE__, num);
			rte_free(obj_table);
			return -1;
		}

		if (rte_stack_pop(t->s, obj_table, num) != num) {
			printf("[%s():%u] Failed to pop %u pointers\n",
			       __func__, __LINE__, num);
			rte_free(obj_table);
			return -1;
		}

		rte_atomic64_sub(t->sz, num);
	}

	rte_free(obj_table);
	return 0;
}

static int
test_stack_multithreaded(uint32_t flags)
{
	struct test_args *args;
	unsigned int lcore_id;
	struct rte_stack *s;
	rte_atomic64_t size;

	printf("[%s():%u] Running with %u lcores\n",
	       __func__, __LINE__, rte_lcore_count());

	if (rte_lcore_count() < 2)
		return 0;

	args = rte_malloc(NULL, sizeof(struct test_args) * RTE_MAX_LCORE, 0);
	if (args == NULL) {
		printf("[%s():%u] failed to malloc %zu bytes\n",
		       __func__, __LINE__,
		       sizeof(struct test_args) * RTE_MAX_LCORE);
		return -1;
	}

	s = rte_stack_create("test", STACK_SIZE, rte_socket_id(), flags);
	if (s == NULL) {
		printf("[%s():%u] Failed to create a stack\n",
		       __func__, __LINE__);
		rte_free(args);
		return -1;
	}

	rte_atomic64_init(&size);

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		args[lcore_id].s = s;
		args[lcore_id].sz = &size;

		if (rte_eal_remote_launch(stack_thread_push_pop,
					  &args[lcore_id], lcore_id))
			rte_panic("Failed to launch lcore %d\n", lcore_id);
	}

	lcore_id = rte_lcore_id();

	args[lcore_id].s = s;
	args[lcore_id].sz = &size;

	stack_thread_push_pop(&args[lcore_id]);

	rte_eal_mp_wait_lcore();

	rte_stack_free(s);
	rte_free(args);

	return 0;
}

static int
__test_stack(uint32_t flags)
{
	if (test_stack_basic(flags) < 0)
		return -1;

	if (test_lookup_null() < 0)
		return -1;

	if (test_free_null() < 0)
		return -1;

	if (test_stack_name_reuse(flags) < 0)
		return -1;

	if (test_stack_name_length(flags) < 0)
		return -1;

	if (test_stack_multithreaded(flags) < 0)
		return -1;

	return 0;
}

static int
test_stack(void)
{
	return __test_stack(0);
}

static int
test_lf_stack(void)
{
	return __test_stack(RTE_STACK_F_LF);
}

REGISTER_TEST_COMMAND(stack_autotest, test_stack);
REGISTER_TEST_COMMAND(stack_lf_autotest, test_lf_stack);
