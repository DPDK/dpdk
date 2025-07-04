/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
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
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_lpm.h>
#include <rte_lpm6.h>
#include <rte_malloc.h>

#include "main.h"

void
app_main_loop_rx(void) {
	uint32_t i;
	int ret;

	RTE_LOG(INFO, USER1, "Core %u is doing RX\n", rte_lcore_id());

	while (!force_quit) {
		for (i = 0; i < app.n_ports; i++) {
			uint16_t n_mbufs;

			n_mbufs = rte_eth_rx_burst(
				app.ports[i],
				0,
				app.mbuf_rx.array,
				app.burst_size_rx_read);

			if (n_mbufs == 0)
				continue;

			do {
				ret = rte_ring_sp_enqueue_bulk(
					app.rings_rx[i],
					(void **) app.mbuf_rx.array,
					n_mbufs, NULL);
			} while (ret == 0 && !force_quit);
		}
	}
}

void
app_main_loop_worker(void) {
	struct app_mbuf_array *worker_mbuf;
	uint32_t i;

	RTE_LOG(INFO, USER1, "Core %u is doing work (no pipeline)\n",
		rte_lcore_id());

	worker_mbuf = rte_malloc_socket(NULL, sizeof(struct app_mbuf_array),
			RTE_CACHE_LINE_SIZE, rte_socket_id());
	if (worker_mbuf == NULL)
		rte_panic("Worker thread: cannot allocate buffer space\n");

	while (!force_quit) {
		for (i = 0; i < app.n_ports; i++) {
			int ret;

			ret = rte_ring_sc_dequeue_bulk(
				app.rings_rx[i],
				(void **) worker_mbuf->array,
				app.burst_size_worker_read,
				NULL);

			if (ret == 0)
				continue;

			do {
				ret = rte_ring_sp_enqueue_bulk(
					app.rings_tx[i ^ 1],
					(void **) worker_mbuf->array,
					app.burst_size_worker_write,
					NULL);
			} while (ret == 0 && !force_quit);
		}
	}
}

void
app_main_loop_tx(void) {
	uint32_t i;

	RTE_LOG(INFO, USER1, "Core %u is doing TX\n", rte_lcore_id());

	while (!force_quit) {
		for (i = 0; i < app.n_ports; i++) {
			uint16_t n_mbufs, n_pkts;
			int ret;

			n_mbufs = app.mbuf_tx[i].n_mbufs;

			ret = rte_ring_sc_dequeue_bulk(
				app.rings_tx[i],
				(void **) &app.mbuf_tx[i].array[n_mbufs],
				app.burst_size_tx_read,
				NULL);

			if (ret == 0)
				continue;

			n_mbufs += app.burst_size_tx_read;

			if (n_mbufs < app.burst_size_tx_write) {
				app.mbuf_tx[i].n_mbufs = n_mbufs;
				continue;
			}

			n_pkts = rte_eth_tx_burst(
				app.ports[i],
				0,
				app.mbuf_tx[i].array,
				n_mbufs);

			if (n_pkts < n_mbufs) {
				uint16_t k;

				for (k = n_pkts; k < n_mbufs; k++) {
					struct rte_mbuf *pkt_to_free;

					pkt_to_free = app.mbuf_tx[i].array[k];
					rte_pktmbuf_free(pkt_to_free);
				}
			}

			app.mbuf_tx[i].n_mbufs = 0;
		}
	}
}
