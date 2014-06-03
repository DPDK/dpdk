/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <dirent.h>
#include <signal.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_string_fns.h>

#include "flib.h"

#define SIG_PARENT_EXIT SIGUSR1

struct lcore_stat {
	pid_t pid;           /**< pthread identifier */
	lcore_function_t *f; /**< function to call */
	void *arg;           /**< argument of function */
	slave_exit_notify *cb_fn;
} __rte_cache_aligned;


static struct lcore_stat *core_cfg;
static uint16_t *lcore_cfg = NULL;

/* signal handler to be notified after parent leaves */
static void
sighand_parent_exit(int sig)
{
	printf("lcore = %u : Find parent leaves, sig=%d\n", rte_lcore_id(),
			sig);
	printf("Child leaving\n");
	exit(0);

	return;
}

/**
 * Real function entrance ran in slave process
 **/
static int
slave_proc_func(void)
{
	struct rte_config *config;
	unsigned slave_id = rte_lcore_id();
	struct lcore_stat *cfg = &core_cfg[slave_id];

	if (prctl(PR_SET_PDEATHSIG, SIG_PARENT_EXIT, 0, 0, 0, 0) != 0)
		printf("Warning: Slave can't register for being notified in"
               "case master process exited\n");
	else {
		struct sigaction act;
		memset(&act, 0 , sizeof(act));
		act.sa_handler = sighand_parent_exit;
		if (sigaction(SIG_PARENT_EXIT, &act, NULL) != 0)
			printf("Fail to register signal handler:%d\n", SIG_PARENT_EXIT);
	}

	/* Set slave process to SECONDARY to avoid operation like dev_start/stop etc */
	config = rte_eal_get_configuration();
	if (NULL == config)
		printf("Warning:Can't get rte_config\n");
	else
		config->process_type = RTE_PROC_SECONDARY;

	printf("Core %u is ready (pid=%d)\n", slave_id, (int)cfg->pid);

	exit(cfg->f(cfg->arg));
}

/**
 * function entrance ran in master thread, which will spawn slave process and wait until
 * specific slave exited.
 **/
static int
lcore_func(void *arg __attribute__((unused)))
{
	unsigned slave_id = rte_lcore_id();
	struct lcore_stat *cfg = &core_cfg[slave_id];
	int pid, stat;

	if (rte_get_master_lcore() == slave_id)
		return cfg->f(cfg->arg);

	/* fork a slave process */
	pid = fork();

	if (pid == -1) {
		printf("Failed to fork\n");
		return -1;
	} else if (pid == 0) /* child */
		return slave_proc_func();
	else { /* parent */
		cfg->pid = pid;

		waitpid(pid, &stat, 0);

		cfg->pid = 0;
		cfg->f = NULL;
		cfg->arg = NULL;
		/* Notify slave's exit if applicable */
		if(cfg->cb_fn)
			cfg->cb_fn(slave_id, stat);
		return stat;
	}
}

static int
lcore_id_init(void)
{
	int i;
	/* Setup lcore ID allocation map */
	lcore_cfg = rte_zmalloc("LCORE_ID_MAP",
						sizeof(uint16_t) * RTE_MAX_LCORE,
						CACHE_LINE_SIZE);

	if(lcore_cfg == NULL)
		rte_panic("Failed to malloc\n");

	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (rte_lcore_is_enabled(i))
			lcore_cfg[i] = 1;
	}
	return 0;
}

int
flib_assign_lcore_id(void)
{
	unsigned i;
	int ret;

	/**
	 * thread assigned a lcore id previously, or a  slave thread. But still have
	 * a bug here: If the core mask includes core 0, and that core call this
	 * function, it still can get a new lcore id.
	 **/
	if (rte_lcore_id() != 0)
		return -1;

	do {
		/* Find a lcore id not used yet, avoid to use lcore ID 0 */
		for (i = 1; i < RTE_MAX_LCORE; i++) {
			if (lcore_cfg[i] == 0)
				break;
		}
		if (i == RTE_MAX_LCORE)
			return -1;

		/* Assign new lcore id to this thread */

		ret = rte_atomic16_cmpset(&lcore_cfg[i], 0, 1);
	} while (unlikely(ret == 0));

	RTE_PER_LCORE(_lcore_id) = i;
	return i;
}

void
flib_free_lcore_id(unsigned lcore_id)
{
	/* id is not valid or belongs to pinned core id */
	if (lcore_id >= RTE_MAX_LCORE || lcore_id == 0 ||
		rte_lcore_is_enabled(lcore_id))
		return;

	lcore_cfg[lcore_id] = 0;
}

int
flib_register_slave_exit_notify(unsigned slave_id,
	slave_exit_notify *cb)
{
	if (cb == NULL)
		return -EFAULT;

	if (!rte_lcore_is_enabled(slave_id))
		return -ENOENT;

	core_cfg[slave_id].cb_fn = cb;

	return 0;
}

enum slave_stat
flib_query_slave_status(unsigned slave_id)
{
	if (!rte_lcore_is_enabled(slave_id))
		return ST_FREEZE;
	/* pid only be set when slave process spawned */
	if (core_cfg[slave_id].pid != 0)
		return ST_RUN;
	else
		return ST_IDLE;
}

int
flib_remote_launch(lcore_function_t *f,
					void *arg, unsigned slave_id)
{
	if (f == NULL)
		return -1;

	if (!rte_lcore_is_enabled(slave_id))
		return -1;

	/* Wait until specific lcore state change to WAIT */
	rte_eal_wait_lcore(slave_id);

	core_cfg[slave_id].f = f;
	core_cfg[slave_id].arg = arg;

	return rte_eal_remote_launch(lcore_func, NULL, slave_id);
}

int
flib_mp_remote_launch(lcore_function_t *f, void *arg,
			enum rte_rmt_call_master_t call_master)
{
	int i;

	RTE_LCORE_FOREACH_SLAVE(i) {
		core_cfg[i].arg = arg;
		core_cfg[i].f = f;
	}

	return rte_eal_mp_remote_launch(lcore_func, NULL, call_master);
}

int
flib_init(void)
{
	if ((core_cfg = rte_zmalloc("core_cfg",
		sizeof(struct lcore_stat) * RTE_MAX_LCORE,
		CACHE_LINE_SIZE)) == NULL ) {
		printf("rte_zmalloc failed\n");
		return -1;
	}

	if (lcore_id_init() != 0) {
		printf("lcore_id_init failed\n");
		return -1;
	}

	return 0;
}
