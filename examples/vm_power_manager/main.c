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

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <sys/queue.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_log.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_config.h>

#include "channel_manager.h"
#include "channel_monitor.h"
#include "power_manager.h"
#include "vm_power_cli.h"

static int
run_monitor(__attribute__((unused)) void *arg)
{
	if (channel_monitor_init() < 0) {
		printf("Unable to initialize channel monitor\n");
		return -1;
	}
	run_channel_monitor();
	return 0;
}

static void
sig_handler(int signo)
{
	printf("Received signal %d, exiting...\n", signo);
	channel_monitor_exit();
	channel_manager_exit();
	power_manager_exit();

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

	lcore_id = rte_get_next_lcore(-1, 1, 0);
	if (lcore_id == RTE_MAX_LCORE) {
		RTE_LOG(ERR, EAL, "A minimum of two cores are required to run "
				"application\n");
		return 0;
	}
	rte_eal_remote_launch(run_monitor, NULL, lcore_id);

	if (power_manager_init() < 0) {
		printf("Unable to initialize power manager\n");
		return -1;
	}
	if (channel_manager_init(CHANNEL_MGR_DEFAULT_HV_PATH) < 0) {
		printf("Unable to initialize channel manager\n");
		return -1;
	}
	run_cli(NULL);

	rte_eal_mp_wait_lcore();
	return 0;
}
