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

/*
 * This sample application is a simple multi-process application which
 * demostrates sharing of queues and memory pools between processes, and
 * using those queues/pools for communication between the processes.
 *
 * Application is designed to run with two processes, a primary and a
 * secondary, and each accepts commands on the commandline, the most
 * important of which is "send", which just sends a string to the other
 * process.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_log.h>
#include <rte_mempool.h>
#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_socket.h>
#include <cmdline.h>
#include "mp_commands.h"

#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1

static const char *_MSG_POOL = "MSG_POOL";
static const char *_SEC_2_PRI = "SEC_2_PRI";
static const char *_PRI_2_SEC = "PRI_2_SEC";
const unsigned string_size = 64;

struct rte_ring *send_ring, *recv_ring;
struct rte_mempool *message_pool;
volatile int quit = 0;

static int
lcore_recv(__attribute__((unused)) void *arg)
{
	unsigned lcore_id = rte_lcore_id();

	printf("Starting core %u\n", lcore_id);
	while (!quit){
		void *msg;
		if (rte_ring_dequeue(recv_ring, &msg) < 0){
			usleep(5);
			continue;
		}
		printf("core %u: Received '%s'\n", lcore_id, (char *)msg);
		rte_mempool_put(message_pool, msg);
	}

	return 0;
}

int
main(int argc, char **argv)
{
	const unsigned flags = 0;
	const unsigned ring_size = 64;
	const unsigned pool_size = 1024;
	const unsigned pool_cache = 32;
	const unsigned priv_data_sz = 0;

	int ret;
	unsigned lcore_id;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Cannot init EAL\n");

	if (rte_eal_process_type() == RTE_PROC_PRIMARY){
		send_ring = rte_ring_create(_PRI_2_SEC, ring_size, rte_socket_id(), flags);
		recv_ring = rte_ring_create(_SEC_2_PRI, ring_size, rte_socket_id(), flags);
		message_pool = rte_mempool_create(_MSG_POOL, pool_size,
				string_size, pool_cache, priv_data_sz,
				NULL, NULL, NULL, NULL,
				rte_socket_id(), flags);
	} else {
		recv_ring = rte_ring_lookup(_PRI_2_SEC);
		send_ring = rte_ring_lookup(_SEC_2_PRI);
		message_pool = rte_mempool_lookup(_MSG_POOL);
	}
	if (send_ring == NULL)
		rte_exit(EXIT_FAILURE, "Problem getting sending ring\n");
	if (recv_ring == NULL)
		rte_exit(EXIT_FAILURE, "Problem getting receiving ring\n");
	if (message_pool == NULL)
		rte_exit(EXIT_FAILURE, "Problem getting message pool\n");

	RTE_LOG(INFO, APP, "Finished Process Init.\n");

	/* call lcore_recv() on every slave lcore */
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(lcore_recv, NULL, lcore_id);
	}

	/* call cmd prompt on master lcore */
	struct cmdline *cl = cmdline_stdin_new(simple_mp_ctx, "\nsimple_mp > ");
	if (cl == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create cmdline instance\n");
	cmdline_interact(cl);
	cmdline_stdin_exit(cl);

	rte_eal_mp_wait_lcore();
	return 0;
}
