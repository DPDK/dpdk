/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sched.h>
#include <pthread_np.h>
#include <sys/queue.h>
#include <sys/thr.h>

#include <rte_debug.h>
#include <rte_atomic.h>
#include <rte_launch.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_per_lcore.h>
#include <rte_eal.h>
#include <rte_lcore.h>

#include "eal_private.h"
#include "eal_thread.h"

/* require calling thread tid by gettid() */
int rte_sys_gettid(void)
{
	long lwpid;
	thr_self(&lwpid);
	return (int)lwpid;
}

void rte_thread_set_name(rte_thread_t thread_id, const char *thread_name)
{
	char truncated[RTE_THREAD_NAME_SIZE];
	const size_t truncatedsz = sizeof(truncated);

	if (strlcpy(truncated, thread_name, truncatedsz) >= truncatedsz)
		EAL_LOG(DEBUG, "Truncated thread name");

	pthread_set_name_np((pthread_t)thread_id.opaque_id, truncated);
}
