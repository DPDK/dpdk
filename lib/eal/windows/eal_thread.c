/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <io.h>

#include <rte_atomic.h>
#include <rte_debug.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_common.h>
#include <rte_memory.h>

#include "eal_private.h"
#include "eal_thread.h"
#include "eal_windows.h"

int
eal_thread_wake_worker(unsigned int worker_id)
{
	int m2w = lcore_config[worker_id].pipe_main2worker[1];
	int w2m = lcore_config[worker_id].pipe_worker2main[0];
	char c = 0;
	int n;

	do {
		n = _write(m2w, &c, 1);
	} while (n == 0 || (n < 0 && errno == EINTR));
	if (n < 0)
		return -EPIPE;

	do {
		n = _read(w2m, &c, 1);
	} while (n < 0 && errno == EINTR);
	if (n <= 0)
		return -EPIPE;
	return 0;
}

void
eal_thread_wait_command(void)
{
	unsigned int lcore_id = rte_lcore_id();
	int m2w;
	char c;
	int n;

	m2w = lcore_config[lcore_id].pipe_main2worker[0];
	do {
		n = _read(m2w, &c, 1);
	} while (n < 0 && errno == EINTR);
	if (n <= 0)
		rte_panic("cannot read on configuration pipe\n");
}

void
eal_thread_ack_command(void)
{
	unsigned int lcore_id = rte_lcore_id();
	char c = 0;
	int w2m;
	int n;

	w2m = lcore_config[lcore_id].pipe_worker2main[1];
	do {
		n = _write(w2m, &c, 1);
	} while (n == 0 || (n < 0 && errno == EINTR));
	if (n < 0)
		rte_panic("cannot write on configuration pipe\n");
}

/* get current thread ID */
int
rte_sys_gettid(void)
{
	return GetCurrentThreadId();
}

int
rte_thread_setname(__rte_unused pthread_t id, __rte_unused const char *name)
{
	/* TODO */
	/* This is a stub, not the expected result */
	return 0;
}
