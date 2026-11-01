/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Kevin Bowling
 */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <rte_alarm.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_interrupts.h>

#include "test.h"

static struct rte_intr_handle *handles[3];
static int pipes[3][2];
static RTE_ATOMIC(bool) entered;
static RTE_ATOMIC(bool) release_gate;
static RTE_ATOMIC(unsigned int) callbacks;
static RTE_ATOMIC(unsigned int) drained;
static RTE_ATOMIC(bool) batch_done;
static RTE_ATOMIC(int) alarm_status;

static void
freebsd_gate(void *arg __rte_unused)
{
	char byte;

	if (read(pipes[0][0], &byte, 1) != 1)
		return;
	rte_atomic_store_explicit(&entered, true, rte_memory_order_release);
	while (!rte_atomic_load_explicit(&release_gate, rte_memory_order_acquire))
		rte_delay_us_sleep(1000);
}

static void
freebsd_after_batch(void *arg __rte_unused)
{
	rte_atomic_store_explicit(&batch_done, true, rte_memory_order_release);
}

static void
freebsd_drain_peer(void *arg)
{
	int *peer = arg;
	char byte;

	if (read(*peer, &byte, 1) == 1)
		rte_atomic_fetch_add_explicit(&drained, 1, rte_memory_order_relaxed);
	rte_atomic_store_explicit(&alarm_status,
		rte_eal_alarm_set(1000, freebsd_after_batch, NULL), rte_memory_order_relaxed);
	rte_atomic_fetch_add_explicit(&callbacks, 1, rte_memory_order_release);
}

static int
freebsd_unregister(unsigned int i)
{
	int ret;
	unsigned int tries = 1000;

	do {
		ret = rte_intr_callback_unregister(handles[i],
			i == 0 ? freebsd_gate : freebsd_drain_peer, (void *)-1);
		if (ret != -EAGAIN)
			return ret;
		rte_delay_us_sleep(1000);
	} while (--tries != 0);
	return ret;
}

static void
freebsd_interrupt_teardown(void)
{
	unsigned int i;

	rte_atomic_store_explicit(&release_gate, true, rte_memory_order_release);
	for (i = 0; i < RTE_DIM(handles); i++) {
		/* A failed unregister leaves the source referenced by the EAL. */
		if (handles[i] != NULL && freebsd_unregister(i) == -EAGAIN)
			continue;
		if (handles[i] != NULL) {
			rte_intr_fd_set(handles[i], -1);
			rte_intr_instance_free(handles[i]);
			handles[i] = NULL;
		}
		if (pipes[i][0] >= 0)
			close(pipes[i][0]);
		if (pipes[i][1] >= 0)
			close(pipes[i][1]);
	}
	rte_eal_alarm_cancel(freebsd_after_batch, NULL);
}

static int
freebsd_interrupt_setup(void)
{
	unsigned int i;

	for (i = 0; i < RTE_DIM(handles); i++)
		pipes[i][0] = pipes[i][1] = -1;
	rte_atomic_store_explicit(&entered, false, rte_memory_order_relaxed);
	rte_atomic_store_explicit(&release_gate, false, rte_memory_order_relaxed);
	rte_atomic_store_explicit(&callbacks, 0, rte_memory_order_relaxed);
	rte_atomic_store_explicit(&drained, 0, rte_memory_order_relaxed);
	rte_atomic_store_explicit(&batch_done, false, rte_memory_order_relaxed);
	rte_atomic_store_explicit(&alarm_status, 0, rte_memory_order_relaxed);
	for (i = 0; i < RTE_DIM(handles); i++) {
		if (pipe2(pipes[i], O_NONBLOCK) != 0)
			goto fail;
		handles[i] = rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_PRIVATE);
		if (handles[i] == NULL)
			goto fail;
		if (rte_intr_fd_set(handles[i], pipes[i][0]) != 0 ||
				rte_intr_type_set(handles[i], i == 0 ?
				RTE_INTR_HANDLE_EXT : RTE_INTR_HANDLE_UNKNOWN) != 0)
			goto fail;
	}
	return TEST_SUCCESS;
fail:
	freebsd_interrupt_teardown();
	return TEST_FAILED;
}

static int
freebsd_interrupt_read_race(void)
{
	unsigned int i;

	TEST_ASSERT_SUCCESS(rte_intr_callback_register(handles[0], freebsd_gate, NULL),
		"Cannot register gate");
	TEST_ASSERT_SUCCESS(rte_intr_callback_register(handles[1], freebsd_drain_peer,
		&pipes[2][0]), "Cannot register first source");
	TEST_ASSERT_SUCCESS(rte_intr_callback_register(handles[2], freebsd_drain_peer,
		&pipes[1][0]), "Cannot register second source");
	TEST_ASSERT_EQUAL(write(pipes[0][1], "g", 1), 1, "Cannot trigger gate");
	for (i = 0; i < 1000 &&
			!rte_atomic_load_explicit(&entered, rte_memory_order_acquire); i++)
		rte_delay_us_sleep(1000);
	TEST_ASSERT(rte_atomic_load_explicit(&entered, rte_memory_order_acquire),
		"Interrupt thread did not reach gate");

	/* Both descriptors are ready in the next kevent batch. The first callback
	 * drains its peer, so the second read sees EAGAIN. Neither a callback on
	 * the empty source nor a permanently active source is valid.
	 */
	TEST_ASSERT_EQUAL(write(pipes[1][1], "a", 1), 1, "Cannot trigger first source");
	TEST_ASSERT_EQUAL(write(pipes[2][1], "b", 1), 1, "Cannot trigger second source");
	rte_atomic_store_explicit(&release_gate, true, rte_memory_order_release);
	for (i = 0; i < 1000 &&
			rte_atomic_load_explicit(&callbacks, rte_memory_order_acquire) == 0; i++)
		rte_delay_us_sleep(1000);
	/* An alarm installed by the callback is dispatched in a later kevent
	 * batch, after the peer's read and source cleanup have been attempted.
	 */
	TEST_ASSERT_EQUAL(rte_atomic_load_explicit(&alarm_status, rte_memory_order_relaxed),
		0, "Cannot schedule completion alarm");
	for (i = 0; i < 1000 &&
			!rte_atomic_load_explicit(&batch_done, rte_memory_order_acquire); i++)
		rte_delay_us_sleep(1000);
	TEST_ASSERT(rte_atomic_load_explicit(&batch_done, rte_memory_order_acquire),
		"Interrupt thread did not finish the event batch");
	for (i = 0; i < RTE_DIM(handles); i++)
		TEST_ASSERT_EQUAL(freebsd_unregister(i), 1, "Source %u remained active", i);
	TEST_ASSERT_EQUAL(rte_atomic_load_explicit(&callbacks, rte_memory_order_acquire),
		1, "Callback ran without a readable event");
	TEST_ASSERT_EQUAL(rte_atomic_load_explicit(&drained, rte_memory_order_relaxed),
		1, "Peer was not drained");
	return TEST_SUCCESS;
}

static struct unit_test_suite freebsd_interrupt_suite = {
	.suite_name = "FreeBSD interrupt dispatch",
	.unit_test_cases = {
		TEST_CASE_ST(freebsd_interrupt_setup, freebsd_interrupt_teardown,
			freebsd_interrupt_read_race),
		TEST_CASES_END()
	}
};

static int
freebsd_interrupt_test(void)
{
	return unit_test_suite_runner(&freebsd_interrupt_suite);
}

REGISTER_FAST_TEST(interrupt_freebsd_autotest, NOHUGE_OK, ASAN_OK, freebsd_interrupt_test);
