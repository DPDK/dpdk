/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

/*
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
*/
#include <signal.h>

#include <rte_lcore.h>
#include <rte_power.h>
#include <rte_debug.h>

#include "vm_power_cli_guest.h"

static void
sig_handler(int signo)
{
	printf("Received signal %d, exiting...\n", signo);
	unsigned lcore_id;

	RTE_LCORE_FOREACH(lcore_id) {
		rte_power_exit(lcore_id);
	}

}

int
main(int argc, char **argv)
{
	int ret;
	unsigned lcore_id;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	rte_power_set_env(PM_ENV_KVM_VM);
	RTE_LCORE_FOREACH(lcore_id) {
		rte_power_init(lcore_id);
	}
	run_cli(NULL);

	return 0;
}
