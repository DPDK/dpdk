/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <errno.h>

#include <eal_trace_internal.h>
#include <rte_launch.h>
#include <rte_pause.h>
#include <rte_lcore.h>

#include "eal_private.h"
#include "eal_thread.h"

/*
 * Wait until a lcore finished its job.
 */
int
rte_eal_wait_lcore(unsigned worker_id)
{
	while (rte_atomic_load_explicit(&lcore_config[worker_id].state,
			rte_memory_order_acquire) != WAIT)
		rte_pause();

	return lcore_config[worker_id].ret;
}

/*
 * Send a message to a worker lcore identified by worker_id to call a
 * function f with argument arg. Once the execution is done, the
 * remote lcore switches to WAIT state.
 */
int
rte_eal_remote_launch(lcore_function_t *f, void *arg, unsigned int worker_id)
{
	int rc = -EBUSY;

	/* Check if the worker is in 'WAIT' state. Use acquire order
	 * since 'state' variable is used as the guard variable.
	 */
	if (rte_atomic_load_explicit(&lcore_config[worker_id].state,
			rte_memory_order_acquire) != WAIT)
		goto finish;

	lcore_config[worker_id].arg = arg;
	/* Ensure that all the memory operations are completed
	 * before the worker thread starts running the function.
	 * Use worker thread function as the guard variable.
	 */
	rte_atomic_store_explicit(&lcore_config[worker_id].f, f, rte_memory_order_release);

	rc = eal_thread_wake_worker(worker_id);

finish:
	rte_eal_trace_thread_remote_launch(f, arg, worker_id, rc);
	return rc;
}

/*
 * Check that every WORKER lcores are in WAIT state, then call
 * rte_eal_remote_launch() for all of them. If call_main is true
 * (set to CALL_MAIN), also call the function on the main lcore.
 */
int
rte_eal_mp_remote_launch(int (*f)(void *), void *arg,
			 enum rte_rmt_call_main_t call_main)
{
	int lcore_id;
	int main_lcore = rte_get_main_lcore();

	/* check state of lcores */
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (lcore_config[lcore_id].state != WAIT)
			return -EBUSY;
	}

	/* send messages to cores */
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		rte_eal_remote_launch(f, arg, lcore_id);
	}

	if (call_main == CALL_MAIN) {
		lcore_config[main_lcore].ret = f(arg);
		lcore_config[main_lcore].state = WAIT;
	}

	return 0;
}

/*
 * Return the state of the lcore identified by worker_id.
 */
enum rte_lcore_state_t
rte_eal_get_lcore_state(unsigned lcore_id)
{
	return lcore_config[lcore_id].state;
}

/*
 * Do a rte_eal_wait_lcore() for every lcore. The return values are
 * ignored.
 */
void
rte_eal_mp_wait_lcore(void)
{
	unsigned lcore_id;

	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		rte_eal_wait_lcore(lcore_id);
	}
}
