/*-
 *  BSD LICENSE
 *
 *  Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *    * Neither the name of Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_interrupts.h>

#include "test.h"

#define TEST_INTERRUPT_CHECK_INTERVAL 1000 /* ms */

/* predefined interrupt handle types */
enum test_interrupt_handle_type {
	TEST_INTERRUPT_HANDLE_INVALID,
	TEST_INTERRUPT_HANDLE_VALID,
	TEST_INTERRUPT_HANDLE_VALID_UIO,
	TEST_INTERRUPT_HANDLE_VALID_ALARM,
	TEST_INTERRUPT_HANDLE_CASE1,
	TEST_INTERRUPT_HANDLE_MAX
};

/* flag of if callback is called */
static volatile int flag;
static struct rte_intr_handle intr_handles[TEST_INTERRUPT_HANDLE_MAX];
static enum test_interrupt_handle_type test_intr_type =
				TEST_INTERRUPT_HANDLE_MAX;

#ifdef RTE_EXEC_ENV_LINUXAPP
union intr_pipefds{
	struct {
		int pipefd[2];
	};
	struct {
		int readfd;
		int writefd;
	};
};

static union intr_pipefds pfds;

/**
 * Check if the interrupt handle is valid.
 */
static inline int
test_interrupt_handle_sanity_check(struct rte_intr_handle *intr_handle)
{
	if (!intr_handle || intr_handle->fd < 0)
		return -1;

	return 0;
}

/**
 * Initialization for interrupt test.
 */
static int
test_interrupt_init(void)
{
	if (pipe(pfds.pipefd) < 0)
		return -1;

	intr_handles[TEST_INTERRUPT_HANDLE_INVALID].fd = -1;
	intr_handles[TEST_INTERRUPT_HANDLE_INVALID].type =
					RTE_INTR_HANDLE_UNKNOWN;

	intr_handles[TEST_INTERRUPT_HANDLE_VALID].fd = pfds.readfd;
	intr_handles[TEST_INTERRUPT_HANDLE_VALID].type =
					RTE_INTR_HANDLE_UNKNOWN;

	intr_handles[TEST_INTERRUPT_HANDLE_VALID_UIO].fd = pfds.readfd;
	intr_handles[TEST_INTERRUPT_HANDLE_VALID_UIO].type =
					RTE_INTR_HANDLE_UIO;

	intr_handles[TEST_INTERRUPT_HANDLE_VALID_ALARM].fd = pfds.readfd;
	intr_handles[TEST_INTERRUPT_HANDLE_VALID_ALARM].type =
					RTE_INTR_HANDLE_ALARM;

	intr_handles[TEST_INTERRUPT_HANDLE_CASE1].fd = pfds.writefd;
	intr_handles[TEST_INTERRUPT_HANDLE_CASE1].type = RTE_INTR_HANDLE_UIO;

	return 0;
}

/**
 * Deinitialization for interrupt test.
 */
static int
test_interrupt_deinit(void)
{
	close(pfds.pipefd[0]);
	close(pfds.pipefd[1]);

	return 0;
}

/**
 * Write the pipe to simulate an interrupt.
 */
static int
test_interrupt_trigger_interrupt(void)
{
	if (write(pfds.writefd, "1", 1) < 0)
		return -1;

	return 0;
}

/**
 * Check if two interrupt handles are the same.
 */
static int
test_interrupt_handle_compare(struct rte_intr_handle *intr_handle_l,
				struct rte_intr_handle *intr_handle_r)
{
	if (!intr_handle_l || !intr_handle_r)
		return -1;

	if (intr_handle_l->fd != intr_handle_r->fd ||
		intr_handle_l->type != intr_handle_r->type)
		return -1;

	return 0;
}

#else
/* to be implemented for baremetal later */
static inline int
test_interrupt_handle_sanity_check(struct rte_intr_handle *intr_handle)
{
	RTE_SET_USED(intr_handle);

	return 0;
}

static int
test_interrupt_init(void)
{
	return 0;
}

static int
test_interrupt_deinit(void)
{
	return 0;
}

static int
test_interrupt_trigger_interrupt(void)
{
	return 0;
}

static int
test_interrupt_handle_compare(struct rte_intr_handle *intr_handle_l,
				struct rte_intr_handle *intr_handle_r)
{
	(void)intr_handle_l;
	(void)intr_handle_r;

	return 0;
}
#endif /* RTE_EXEC_ENV_LINUXAPP */

/**
 * Callback for the test interrupt.
 */
static void
test_interrupt_callback(struct rte_intr_handle *intr_handle, void *arg)
{
	if (test_intr_type >= TEST_INTERRUPT_HANDLE_MAX) {
		printf("invalid interrupt type\n");
		flag = -1;
		return;
	}

	if (test_interrupt_handle_sanity_check(intr_handle) < 0) {
		printf("null or invalid intr_handle for %s\n", __func__);
		flag = -1;
		return;
	}

	if (rte_intr_callback_unregister(intr_handle,
			test_interrupt_callback, arg) >= 0) {
		printf("%s: unexpectedly able to unregister itself\n",
			__func__);
		flag = -1;
		return;
	}

	if (test_interrupt_handle_compare(intr_handle,
			&(intr_handles[test_intr_type])) == 0)
		flag = 1;
}

/**
 * Callback for the test interrupt.
 */
static void
test_interrupt_callback_1(struct rte_intr_handle *intr_handle,
	__attribute__((unused)) void *arg)
{
	if (test_interrupt_handle_sanity_check(intr_handle) < 0) {
		printf("null or invalid intr_handle for %s\n", __func__);
		flag = -1;
		return;
	}
}

/**
 * Tests for rte_intr_enable().
 */
static int
test_interrupt_enable(void)
{
	struct rte_intr_handle test_intr_handle;

	/* check with null intr_handle */
	if (rte_intr_enable(NULL) == 0) {
		printf("unexpectedly enable null intr_handle successfully\n");
		return -1;
	}

	/* check with invalid intr_handle */
	test_intr_handle = intr_handles[TEST_INTERRUPT_HANDLE_INVALID];
	if (rte_intr_enable(&test_intr_handle) == 0) {
		printf("unexpectedly enable invalid intr_handle "
			"successfully\n");
		return -1;
	}

	/* check with valid intr_handle */
	test_intr_handle = intr_handles[TEST_INTERRUPT_HANDLE_VALID];
	if (rte_intr_enable(&test_intr_handle) == 0) {
		printf("unexpectedly enable a specific intr_handle "
			"successfully\n");
		return -1;
	}

	/* check with specific valid intr_handle */
	test_intr_handle = intr_handles[TEST_INTERRUPT_HANDLE_VALID_ALARM];
	if (rte_intr_enable(&test_intr_handle) == 0) {
		printf("unexpectedly enable a specific intr_handle "
			"successfully\n");
		return -1;
	}

	/* check with valid handler and its type */
	test_intr_handle = intr_handles[TEST_INTERRUPT_HANDLE_CASE1];
	if (rte_intr_enable(&test_intr_handle) < 0) {
		printf("fail to enable interrupt on a simulated handler\n");
		return -1;
	}

	test_intr_handle = intr_handles[TEST_INTERRUPT_HANDLE_VALID_UIO];
	if (rte_intr_enable(&test_intr_handle) == 0) {
		printf("unexpectedly enable a specific intr_handle "
			"successfully\n");
		return -1;
	}

	return 0;
}

/**
 * Tests for rte_intr_disable().
 */
static int
test_interrupt_disable(void)
{
	struct rte_intr_handle test_intr_handle;

	/* check with null intr_handle */
	if (rte_intr_disable(NULL) == 0) {
		printf("unexpectedly disable null intr_handle "
			"successfully\n");
		return -1;
	}

	/* check with invalid intr_handle */
	test_intr_handle = intr_handles[TEST_INTERRUPT_HANDLE_INVALID];
	if (rte_intr_disable(&test_intr_handle) == 0) {
		printf("unexpectedly disable invalid intr_handle "
			"successfully\n");
		return -1;
	}

	/* check with valid intr_handle */
	test_intr_handle = intr_handles[TEST_INTERRUPT_HANDLE_VALID];
	if (rte_intr_disable(&test_intr_handle) == 0) {
		printf("unexpectedly disable a specific intr_handle "
			"successfully\n");
		return -1;
	}

	/* check with specific valid intr_handle */
	test_intr_handle = intr_handles[TEST_INTERRUPT_HANDLE_VALID_ALARM];
	if (rte_intr_disable(&test_intr_handle) == 0) {
		printf("unexpectedly disable a specific intr_handle "
			"successfully\n");
		return -1;
	}

	/* check with valid handler and its type */
	test_intr_handle = intr_handles[TEST_INTERRUPT_HANDLE_CASE1];
	if (rte_intr_disable(&test_intr_handle) < 0) {
		printf("fail to disable interrupt on a simulated handler\n");
		return -1;
	}

	test_intr_handle = intr_handles[TEST_INTERRUPT_HANDLE_VALID_UIO];
	if (rte_intr_disable(&test_intr_handle) == 0) {
		printf("unexpectedly disable a specific intr_handle "
			"successfully\n");
		return -1;
	}

	return 0;
}

/**
 * Check the full path of a specified type of interrupt simulated.
 */
static int
test_interrupt_full_path_check(enum test_interrupt_handle_type intr_type)
{
	int count;
	struct rte_intr_handle test_intr_handle;

	flag = 0;
	test_intr_handle = intr_handles[intr_type];
	test_intr_type = intr_type;
	if (rte_intr_callback_register(&test_intr_handle,
			test_interrupt_callback, NULL) < 0) {
		printf("fail to register callback\n");
		return -1;
	}

	if (test_interrupt_trigger_interrupt() < 0)
		return -1;

	/* check flag in 3 seconds */
	for (count = 0; flag == 0 && count < 3; count++)
		rte_delay_ms(TEST_INTERRUPT_CHECK_INTERVAL);

	rte_delay_ms(TEST_INTERRUPT_CHECK_INTERVAL);
	if (rte_intr_callback_unregister(&test_intr_handle,
			test_interrupt_callback, NULL) < 0)
		return -1;

	if (flag == 0) {
		printf("callback has not been called\n");
		return -1;
	} else if (flag < 0) {
		printf("it has internal error in callback\n");
		return -1;
	}

	return 0;
}

/**
 * Main function of testing interrupt.
 */
int
test_interrupt(void)
{
	int ret = -1;
	struct rte_intr_handle test_intr_handle;

	if (test_interrupt_init() < 0) {
		printf("fail to initialize for testing interrupt\n");
		return -1;
	}

	printf("Check unknown valid interrupt full path\n");
	if (test_interrupt_full_path_check(TEST_INTERRUPT_HANDLE_VALID) < 0) {
		printf("failure occured during checking unknown valid "
						"interrupt full path\n");
		goto out;
	}

	printf("Check valid UIO interrupt full path\n");
	if (test_interrupt_full_path_check(TEST_INTERRUPT_HANDLE_VALID_UIO)
									< 0) {
		printf("failure occured during checking valid UIO interrupt "
								"full path\n");
		goto out;
	}

	printf("Check valid alarm interrupt full path\n");
	if (test_interrupt_full_path_check(TEST_INTERRUPT_HANDLE_VALID_ALARM)
									< 0) {
		printf("failure occured during checking valid alarm "
						"interrupt full path\n");
		goto out;
	}

	printf("start register/unregister test\n");
	/* check if it will fail to register cb with intr_handle = NULL */
	if (rte_intr_callback_register(NULL, test_interrupt_callback,
							NULL) == 0) {
		printf("unexpectedly register successfully with null "
			"intr_handle\n");
		goto out;
	}

	/* check if it will fail to register cb with invalid intr_handle */
	test_intr_handle = intr_handles[TEST_INTERRUPT_HANDLE_INVALID];
	if (rte_intr_callback_register(&test_intr_handle,
			test_interrupt_callback, NULL) == 0) {
		printf("unexpectedly register successfully with invalid "
			"intr_handle\n");
		goto out;
	}

	/* check if it will fail to register without callback */
	test_intr_handle = intr_handles[TEST_INTERRUPT_HANDLE_VALID];
	if (rte_intr_callback_register(&test_intr_handle, NULL, NULL) == 0) {
		printf("unexpectedly register successfully with "
			"null callback\n");
		goto out;
	}

	/* check if it will fail to unregister cb with intr_handle = NULL */
	if (rte_intr_callback_unregister(NULL,
			test_interrupt_callback, NULL) > 0) {
		printf("unexpectedly unregister successfully with "
			"null intr_handle\n");
		goto out;
	}

	/* check if it will fail to unregister cb with invalid intr_handle */
	test_intr_handle = intr_handles[TEST_INTERRUPT_HANDLE_INVALID];
	if (rte_intr_callback_unregister(&test_intr_handle,
			test_interrupt_callback, NULL) > 0) {
		printf("unexpectedly unregister successfully with "
			"invalid intr_handle\n");
		goto out;
	}

	/* check if it is ok to register the same intr_handle twice */
	test_intr_handle = intr_handles[TEST_INTERRUPT_HANDLE_VALID];
	if (rte_intr_callback_register(&test_intr_handle,
			test_interrupt_callback, NULL) < 0) {
		printf("it fails to register test_interrupt_callback\n");
		goto out;
	}
	if (rte_intr_callback_register(&test_intr_handle,
			test_interrupt_callback_1, NULL) < 0) {
		printf("it fails to register test_interrupt_callback_1\n");
		goto out;
	}
	/* check if it will fail to unregister with invalid parameter */
	if (rte_intr_callback_unregister(&test_intr_handle,
			test_interrupt_callback, (void *)0xff) != 0) {
		printf("unexpectedly unregisters successfully with "
							"invalid arg\n");
		goto out;
	}
	if (rte_intr_callback_unregister(&test_intr_handle,
			test_interrupt_callback, NULL) <= 0) {
		printf("it fails to unregister test_interrupt_callback\n");
		goto out;
	}
	if (rte_intr_callback_unregister(&test_intr_handle,
			test_interrupt_callback_1, (void *)-1) <= 0) {
		printf("it fails to unregister test_interrupt_callback_1 "
			"for all\n");
		goto out;
	}
	rte_delay_ms(TEST_INTERRUPT_CHECK_INTERVAL);

	printf("start interrupt enable/disable test\n");
	/* check interrupt enable/disable functions */
	if (test_interrupt_enable() < 0) {
		printf("fail to check interrupt enabling\n");
		goto out;
	}
	rte_delay_ms(TEST_INTERRUPT_CHECK_INTERVAL);

	if (test_interrupt_disable() < 0) {
		printf("fail to check interrupt disabling\n");
		goto out;
	}
	rte_delay_ms(TEST_INTERRUPT_CHECK_INTERVAL);

	ret = 0;

out:
	printf("Clearing for interrupt tests\n");
	/* clear registered callbacks */
	test_intr_handle = intr_handles[TEST_INTERRUPT_HANDLE_VALID];
	rte_intr_callback_unregister(&test_intr_handle,
			test_interrupt_callback, (void *)-1);
	rte_intr_callback_unregister(&test_intr_handle,
			test_interrupt_callback_1, (void *)-1);

	test_intr_handle = intr_handles[TEST_INTERRUPT_HANDLE_VALID_UIO];
	rte_intr_callback_unregister(&test_intr_handle,
			test_interrupt_callback, (void *)-1);
	rte_intr_callback_unregister(&test_intr_handle,
			test_interrupt_callback_1, (void *)-1);

	test_intr_handle = intr_handles[TEST_INTERRUPT_HANDLE_VALID_ALARM];
	rte_intr_callback_unregister(&test_intr_handle,
			test_interrupt_callback, (void *)-1);
	rte_intr_callback_unregister(&test_intr_handle,
			test_interrupt_callback_1, (void *)-1);

	rte_delay_ms(2 * TEST_INTERRUPT_CHECK_INTERVAL);
	/* deinit */
	test_interrupt_deinit();

	return ret;
}

