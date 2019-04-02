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
#include <eal_thread.h>


RTE_DEFINE_PER_LCORE(unsigned int, _lcore_id) = LCORE_ID_ANY;

/*
 * Send a message to a slave lcore identified by slave_id to call a
 * function f with argument arg. Once the execution is done, the
 * remote lcore switch in FINISHED state.
 */
int
rte_eal_remote_launch(lcore_function_t *f, void *arg, unsigned int slave_id)
{
	int n;
	char c = 0;
	int m2s = lcore_config[slave_id].pipe_master2slave[1];
	int s2m = lcore_config[slave_id].pipe_slave2master[0];

	if (lcore_config[slave_id].state != WAIT)
		return -EBUSY;

	lcore_config[slave_id].f = f;
	lcore_config[slave_id].arg = arg;

	/* send message */
	n = 0;
	while (n == 0 || (n < 0 && errno == EINTR))
		n = _write(m2s, &c, 1);
	if (n < 0)
		rte_panic("cannot write on configuration pipe\n");

	/* wait ack */
	do {
		n = _read(s2m, &c, 1);
	} while (n < 0 && errno == EINTR);

	if (n <= 0)
		rte_panic("cannot read on configuration pipe\n");

	return 0;
}

void
eal_thread_init_master(unsigned int lcore_id)
{
	/* set the lcore ID in per-lcore memory area */
	RTE_PER_LCORE(_lcore_id) = lcore_id;
}

static inline pthread_t
eal_thread_self(void)
{
	return GetCurrentThreadId();
}

/* main loop of threads */
void *
eal_thread_loop(void *arg __rte_unused)
{
	char c;
	int n, ret;
	unsigned int lcore_id;
	pthread_t thread_id;
	int m2s, s2m;
	char cpuset[RTE_CPU_AFFINITY_STR_LEN];

	thread_id = eal_thread_self();

	/* retrieve our lcore_id from the configuration structure */
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (thread_id == lcore_config[lcore_id].thread_id)
			break;
	}
	if (lcore_id == RTE_MAX_LCORE)
		rte_panic("cannot retrieve lcore id\n");

	m2s = lcore_config[lcore_id].pipe_master2slave[0];
	s2m = lcore_config[lcore_id].pipe_slave2master[1];

	/* set the lcore ID in per-lcore memory area */
	RTE_PER_LCORE(_lcore_id) = lcore_id;

	RTE_LOG(DEBUG, EAL, "lcore %u is ready (tid=%zx;cpuset=[%s])\n",
		lcore_id, (uintptr_t)thread_id, cpuset);

	/* read on our pipe to get commands */
	while (1) {
		void *fct_arg;

		/* wait command */
		do {
			n = _read(m2s, &c, 1);
		} while (n < 0 && errno == EINTR);

		if (n <= 0)
			rte_panic("cannot read on configuration pipe\n");

		lcore_config[lcore_id].state = RUNNING;

		/* send ack */
		n = 0;
		while (n == 0 || (n < 0 && errno == EINTR))
			n = _write(s2m, &c, 1);
		if (n < 0)
			rte_panic("cannot write on configuration pipe\n");

		if (lcore_config[lcore_id].f == NULL)
			rte_panic("NULL function pointer\n");

		/* call the function and store the return value */
		fct_arg = lcore_config[lcore_id].arg;
		ret = lcore_config[lcore_id].f(fct_arg);
		lcore_config[lcore_id].ret = ret;
		rte_wmb();

		/* when a service core returns, it should go directly to WAIT
		 * state, because the application will not lcore_wait() for it.
		 */
		if (lcore_config[lcore_id].core_role == ROLE_SERVICE)
			lcore_config[lcore_id].state = WAIT;
		else
			lcore_config[lcore_id].state = FINISHED;
	}
}

/* function to create threads */
int
eal_thread_create(pthread_t *thread)
{
	HANDLE th;

	th = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)eal_thread_loop,
						NULL, 0, (LPDWORD)thread);
	if (!th)
		return -1;

	SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
	SetThreadPriority(th, THREAD_PRIORITY_TIME_CRITICAL);

	return 0;
}
