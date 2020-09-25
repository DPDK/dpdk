/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <rte_interrupts.h>

#include "eal_private.h"
#include "eal_windows.h"

static pthread_t intr_thread;

static HANDLE intr_iocp;

static void
eal_intr_process(const OVERLAPPED_ENTRY *event)
{
	RTE_SET_USED(event);
}

static void *
eal_intr_thread_main(LPVOID arg __rte_unused)
{
	while (1) {
		OVERLAPPED_ENTRY events[16];
		ULONG event_count, i;
		BOOL result;

		result = GetQueuedCompletionStatusEx(
			intr_iocp, events, RTE_DIM(events), &event_count,
			INFINITE, /* no timeout */
			TRUE);    /* alertable wait for alarm APCs */

		if (!result) {
			DWORD error = GetLastError();
			if (error != WAIT_IO_COMPLETION) {
				RTE_LOG_WIN32_ERR("GetQueuedCompletionStatusEx()");
				RTE_LOG(ERR, EAL, "Failed waiting for interrupts\n");
				break;
			}

			/* No I/O events, all work is done in completed APCs. */
			continue;
		}

		for (i = 0; i < event_count; i++)
			eal_intr_process(&events[i]);
	}

	CloseHandle(intr_iocp);
	intr_iocp = NULL;
	return NULL;
}

int
rte_eal_intr_init(void)
{
	int ret = 0;

	intr_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
	if (intr_iocp == NULL) {
		RTE_LOG_WIN32_ERR("CreateIoCompletionPort()");
		RTE_LOG(ERR, EAL, "Cannot create interrupt IOCP\n");
		return -1;
	}

	ret = rte_ctrl_thread_create(&intr_thread, "eal-intr-thread", NULL,
			eal_intr_thread_main, NULL);
	if (ret != 0) {
		rte_errno = -ret;
		RTE_LOG(ERR, EAL, "Cannot create interrupt thread\n");
	}

	return ret;
}

int
rte_thread_is_intr(void)
{
	return pthread_equal(intr_thread, pthread_self());
}

int
rte_intr_rx_ctl(__rte_unused struct rte_intr_handle *intr_handle,
		__rte_unused int epfd, __rte_unused int op,
		__rte_unused unsigned int vec, __rte_unused void *data)
{
	return -ENOTSUP;
}

int
eal_intr_thread_schedule(void (*func)(void *arg), void *arg)
{
	HANDLE handle;

	handle = OpenThread(THREAD_ALL_ACCESS, FALSE, intr_thread);
	if (handle == NULL) {
		RTE_LOG_WIN32_ERR("OpenThread(%llu)", intr_thread);
		return -ENOENT;
	}

	if (!QueueUserAPC((PAPCFUNC)(ULONG_PTR)func, handle, (ULONG_PTR)arg)) {
		RTE_LOG_WIN32_ERR("QueueUserAPC()");
		return -EINVAL;
	}

	return 0;
}
