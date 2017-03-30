/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
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
#include <unistd.h>
#include <sys/queue.h>

#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>

#include <rte_eventdev.h>
#include "test.h"

#define MAX_PORTS 16
#define MAX_QIDS 16
#define NUM_PACKETS (1<<18)

static int evdev;

struct test {
	struct rte_mempool *mbuf_pool;
	uint8_t port[MAX_PORTS];
	uint8_t qid[MAX_QIDS];
	int nb_qids;
};


struct test_event_dev_stats {
	uint64_t rx_pkts;       /**< Total packets received */
	uint64_t rx_dropped;    /**< Total packets dropped (Eg Invalid QID) */
	uint64_t tx_pkts;       /**< Total packets transmitted */

	/** Packets received on this port */
	uint64_t port_rx_pkts[MAX_PORTS];
	/** Packets dropped on this port */
	uint64_t port_rx_dropped[MAX_PORTS];
	/** Packets inflight on this port */
	uint64_t port_inflight[MAX_PORTS];
	/** Packets transmitted on this port */
	uint64_t port_tx_pkts[MAX_PORTS];
	/** Packets received on this qid */
	uint64_t qid_rx_pkts[MAX_QIDS];
	/** Packets dropped on this qid */
	uint64_t qid_rx_dropped[MAX_QIDS];
	/** Packets transmitted on this qid */
	uint64_t qid_tx_pkts[MAX_QIDS];
};

static struct rte_mempool *eventdev_func_mempool;

static int
test_sw_eventdev(void)
{
	struct test *t = malloc(sizeof(struct test));

	const char *eventdev_name = "event_sw0";
	evdev = rte_event_dev_get_dev_id(eventdev_name);
	if (evdev < 0) {
		printf("%d: Eventdev %s not found - creating.\n",
				__LINE__, eventdev_name);
		if (rte_eal_vdev_init(eventdev_name, NULL) < 0) {
			printf("Error creating eventdev\n");
			return -1;
		}
		evdev = rte_event_dev_get_dev_id(eventdev_name);
		if (evdev < 0) {
			printf("Error finding newly created eventdev\n");
			return -1;
		}
	}

	/* Only create mbuf pool once, reuse for each test run */
	if (!eventdev_func_mempool) {
		eventdev_func_mempool = rte_pktmbuf_pool_create(
				"EVENTDEV_SW_SA_MBUF_POOL",
				(1<<12), /* 4k buffers */
				32 /*MBUF_CACHE_SIZE*/,
				0,
				512, /* use very small mbufs */
				rte_socket_id());
		if (!eventdev_func_mempool) {
			printf("ERROR creating mempool\n");
			return -1;
		}
	}
	t->mbuf_pool = eventdev_func_mempool;

	/*
	 * Free test instance, leaving mempool initialized, and a pointer to it
	 * in static eventdev_func_mempool, as it is re-used on re-runs
	 */
	free(t);

	return 0;
}

REGISTER_TEST_COMMAND(eventdev_sw_autotest, test_sw_eventdev);
