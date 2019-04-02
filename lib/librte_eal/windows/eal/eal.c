/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <io.h>
#include <fcntl.h>
#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_lcore.h>
#include <eal_thread.h>
#include <eal_private.h>

/* Address of global and public configuration */
static struct rte_config rte_config;

/* internal configuration (per-core) */
struct lcore_config lcore_config[RTE_MAX_LCORE];

/* Return a pointer to the configuration structure */
struct rte_config *
rte_eal_get_configuration(void)
{
	return &rte_config;
}

static int
sync_func(void *arg __rte_unused)
{
	return 0;
}

static void
rte_eal_init_alert(const char *msg)
{
	fprintf(stderr, "EAL: FATAL: %s\n", msg);
	RTE_LOG(ERR, EAL, "%s\n", msg);
}

 /* Launch threads, called at application init(). */
int
rte_eal_init(int argc __rte_unused, char **argv __rte_unused)
{
	int i;

	/* create a map of all processors in the system */
	eal_create_cpu_map();

	if (rte_eal_cpu_init() < 0) {
		rte_eal_init_alert("Cannot detect lcores.");
		rte_errno = ENOTSUP;
		return -1;
	}

	eal_thread_init_master(rte_config.master_lcore);

	RTE_LCORE_FOREACH_SLAVE(i) {

		/*
		 * create communication pipes between master thread
		 * and children
		 */
		if (_pipe(lcore_config[i].pipe_master2slave,
			sizeof(char), _O_BINARY) < 0)
			rte_panic("Cannot create pipe\n");
		if (_pipe(lcore_config[i].pipe_slave2master,
			sizeof(char), _O_BINARY) < 0)
			rte_panic("Cannot create pipe\n");

		lcore_config[i].state = WAIT;

		/* create a thread for each lcore */
		if (eal_thread_create(&lcore_config[i].thread_id) != 0)
			rte_panic("Cannot create thread\n");
	}

	/*
	 * Launch a dummy function on all slave lcores, so that master lcore
	 * knows they are all ready when this function returns.
	 */
	rte_eal_mp_remote_launch(sync_func, NULL, SKIP_MASTER);
	rte_eal_mp_wait_lcore();
	return 0;
}
