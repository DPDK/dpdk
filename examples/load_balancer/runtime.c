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
#include <rte_memzone.h>
#include <rte_eal.h>
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
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_lpm.h>

#include "main.h"

#ifndef APP_LCORE_IO_FLUSH
#define APP_LCORE_IO_FLUSH           1000000
#endif

#ifndef APP_LCORE_WORKER_FLUSH
#define APP_LCORE_WORKER_FLUSH       1000000
#endif

#ifndef APP_STATS
#define APP_STATS                    1000000
#endif

#define APP_IO_RX_DROP_ALL_PACKETS   0
#define APP_WORKER_DROP_ALL_PACKETS  0
#define APP_IO_TX_DROP_ALL_PACKETS   0

#ifndef APP_IO_RX_PREFETCH_ENABLE
#define APP_IO_RX_PREFETCH_ENABLE    1
#endif

#ifndef APP_WORKER_PREFETCH_ENABLE
#define APP_WORKER_PREFETCH_ENABLE   1
#endif

#ifndef APP_IO_TX_PREFETCH_ENABLE
#define APP_IO_TX_PREFETCH_ENABLE    1
#endif

#if APP_IO_RX_PREFETCH_ENABLE
#define APP_IO_RX_PREFETCH0(p)       rte_prefetch0(p)
#define APP_IO_RX_PREFETCH1(p)       rte_prefetch1(p)
#else
#define APP_IO_RX_PREFETCH0(p)
#define APP_IO_RX_PREFETCH1(p)
#endif

#if APP_WORKER_PREFETCH_ENABLE
#define APP_WORKER_PREFETCH0(p)      rte_prefetch0(p)
#define APP_WORKER_PREFETCH1(p)      rte_prefetch1(p)
#else
#define APP_WORKER_PREFETCH0(p)
#define APP_WORKER_PREFETCH1(p)
#endif

#if APP_IO_TX_PREFETCH_ENABLE
#define APP_IO_TX_PREFETCH0(p)       rte_prefetch0(p)
#define APP_IO_TX_PREFETCH1(p)       rte_prefetch1(p)
#else
#define APP_IO_TX_PREFETCH0(p)
#define APP_IO_TX_PREFETCH1(p)
#endif

static inline void
app_lcore_io_rx_buffer_to_send (
	struct app_lcore_params_io *lp,
	uint32_t worker,
	struct rte_mbuf *mbuf,
	uint32_t bsz)
{
	uint32_t pos;
	int ret;

	pos = lp->rx.mbuf_out[worker].n_mbufs;
	lp->rx.mbuf_out[worker].array[pos ++] = mbuf;
	if (likely(pos < bsz)) {
		lp->rx.mbuf_out[worker].n_mbufs = pos;
		return;
	}

	ret = rte_ring_sp_enqueue_bulk(
		lp->rx.rings[worker],
		(void **) lp->rx.mbuf_out[worker].array,
		bsz,
		NULL);

	if (unlikely(ret == 0)) {
		uint32_t k;
		for (k = 0; k < bsz; k ++) {
			struct rte_mbuf *m = lp->rx.mbuf_out[worker].array[k];
			rte_pktmbuf_free(m);
		}
	}

	lp->rx.mbuf_out[worker].n_mbufs = 0;
	lp->rx.mbuf_out_flush[worker] = 0;

#if APP_STATS
	lp->rx.rings_iters[worker] ++;
	if (likely(ret == 0)) {
		lp->rx.rings_count[worker] ++;
	}
	if (unlikely(lp->rx.rings_iters[worker] == APP_STATS)) {
		unsigned lcore = rte_lcore_id();

		printf("\tI/O RX %u out (worker %u): enq success rate = %.2f\n",
			lcore,
			(unsigned)worker,
			((double) lp->rx.rings_count[worker]) / ((double) lp->rx.rings_iters[worker]));
		lp->rx.rings_iters[worker] = 0;
		lp->rx.rings_count[worker] = 0;
	}
#endif
}

static inline void
app_lcore_io_rx(
	struct app_lcore_params_io *lp,
	uint32_t n_workers,
	uint32_t bsz_rd,
	uint32_t bsz_wr,
	uint8_t pos_lb)
{
	struct rte_mbuf *mbuf_1_0, *mbuf_1_1, *mbuf_2_0, *mbuf_2_1;
	uint8_t *data_1_0, *data_1_1 = NULL;
	uint32_t i;

	for (i = 0; i < lp->rx.n_nic_queues; i ++) {
		uint8_t port = lp->rx.nic_queues[i].port;
		uint8_t queue = lp->rx.nic_queues[i].queue;
		uint32_t n_mbufs, j;

		n_mbufs = rte_eth_rx_burst(
			port,
			queue,
			lp->rx.mbuf_in.array,
			(uint16_t) bsz_rd);

		if (unlikely(n_mbufs == 0)) {
			continue;
		}

#if APP_STATS
		lp->rx.nic_queues_iters[i] ++;
		lp->rx.nic_queues_count[i] += n_mbufs;
		if (unlikely(lp->rx.nic_queues_iters[i] == APP_STATS)) {
			struct rte_eth_stats stats;
			unsigned lcore = rte_lcore_id();

			rte_eth_stats_get(port, &stats);

			printf("I/O RX %u in (NIC port %u): NIC drop ratio = %.2f avg burst size = %.2f\n",
				lcore,
				(unsigned) port,
				(double) stats.imissed / (double) (stats.imissed + stats.ipackets),
				((double) lp->rx.nic_queues_count[i]) / ((double) lp->rx.nic_queues_iters[i]));
			lp->rx.nic_queues_iters[i] = 0;
			lp->rx.nic_queues_count[i] = 0;
		}
#endif

#if APP_IO_RX_DROP_ALL_PACKETS
		for (j = 0; j < n_mbufs; j ++) {
			struct rte_mbuf *pkt = lp->rx.mbuf_in.array[j];
			rte_pktmbuf_free(pkt);
		}

		continue;
#endif

		mbuf_1_0 = lp->rx.mbuf_in.array[0];
		mbuf_1_1 = lp->rx.mbuf_in.array[1];
		data_1_0 = rte_pktmbuf_mtod(mbuf_1_0, uint8_t *);
		if (likely(n_mbufs > 1)) {
			data_1_1 = rte_pktmbuf_mtod(mbuf_1_1, uint8_t *);
		}

		mbuf_2_0 = lp->rx.mbuf_in.array[2];
		mbuf_2_1 = lp->rx.mbuf_in.array[3];
		APP_IO_RX_PREFETCH0(mbuf_2_0);
		APP_IO_RX_PREFETCH0(mbuf_2_1);

		for (j = 0; j + 3 < n_mbufs; j += 2) {
			struct rte_mbuf *mbuf_0_0, *mbuf_0_1;
			uint8_t *data_0_0, *data_0_1;
			uint32_t worker_0, worker_1;

			mbuf_0_0 = mbuf_1_0;
			mbuf_0_1 = mbuf_1_1;
			data_0_0 = data_1_0;
			data_0_1 = data_1_1;

			mbuf_1_0 = mbuf_2_0;
			mbuf_1_1 = mbuf_2_1;
			data_1_0 = rte_pktmbuf_mtod(mbuf_2_0, uint8_t *);
			data_1_1 = rte_pktmbuf_mtod(mbuf_2_1, uint8_t *);
			APP_IO_RX_PREFETCH0(data_1_0);
			APP_IO_RX_PREFETCH0(data_1_1);

			mbuf_2_0 = lp->rx.mbuf_in.array[j+4];
			mbuf_2_1 = lp->rx.mbuf_in.array[j+5];
			APP_IO_RX_PREFETCH0(mbuf_2_0);
			APP_IO_RX_PREFETCH0(mbuf_2_1);

			worker_0 = data_0_0[pos_lb] & (n_workers - 1);
			worker_1 = data_0_1[pos_lb] & (n_workers - 1);

			app_lcore_io_rx_buffer_to_send(lp, worker_0, mbuf_0_0, bsz_wr);
			app_lcore_io_rx_buffer_to_send(lp, worker_1, mbuf_0_1, bsz_wr);
		}

		/* Handle the last 1, 2 (when n_mbufs is even) or 3 (when n_mbufs is odd) packets  */
		for ( ; j < n_mbufs; j += 1) {
			struct rte_mbuf *mbuf;
			uint8_t *data;
			uint32_t worker;

			mbuf = mbuf_1_0;
			mbuf_1_0 = mbuf_1_1;
			mbuf_1_1 = mbuf_2_0;
			mbuf_2_0 = mbuf_2_1;

			data = rte_pktmbuf_mtod(mbuf, uint8_t *);

			APP_IO_RX_PREFETCH0(mbuf_1_0);

			worker = data[pos_lb] & (n_workers - 1);

			app_lcore_io_rx_buffer_to_send(lp, worker, mbuf, bsz_wr);
		}
	}
}

static inline void
app_lcore_io_rx_flush(struct app_lcore_params_io *lp, uint32_t n_workers)
{
	uint32_t worker;

	for (worker = 0; worker < n_workers; worker ++) {
		int ret;

		if (likely((lp->rx.mbuf_out_flush[worker] == 0) ||
		           (lp->rx.mbuf_out[worker].n_mbufs == 0))) {
			lp->rx.mbuf_out_flush[worker] = 1;
			continue;
		}

		ret = rte_ring_sp_enqueue_bulk(
			lp->rx.rings[worker],
			(void **) lp->rx.mbuf_out[worker].array,
			lp->rx.mbuf_out[worker].n_mbufs,
			NULL);

		if (unlikely(ret == 0)) {
			uint32_t k;
			for (k = 0; k < lp->rx.mbuf_out[worker].n_mbufs; k ++) {
				struct rte_mbuf *pkt_to_free = lp->rx.mbuf_out[worker].array[k];
				rte_pktmbuf_free(pkt_to_free);
			}
		}

		lp->rx.mbuf_out[worker].n_mbufs = 0;
		lp->rx.mbuf_out_flush[worker] = 1;
	}
}

static inline void
app_lcore_io_tx(
	struct app_lcore_params_io *lp,
	uint32_t n_workers,
	uint32_t bsz_rd,
	uint32_t bsz_wr)
{
	uint32_t worker;

	for (worker = 0; worker < n_workers; worker ++) {
		uint32_t i;

		for (i = 0; i < lp->tx.n_nic_ports; i ++) {
			uint8_t port = lp->tx.nic_ports[i];
			struct rte_ring *ring = lp->tx.rings[port][worker];
			uint32_t n_mbufs, n_pkts;
			int ret;

			n_mbufs = lp->tx.mbuf_out[port].n_mbufs;
			ret = rte_ring_sc_dequeue_bulk(
				ring,
				(void **) &lp->tx.mbuf_out[port].array[n_mbufs],
				bsz_rd,
				NULL);

			if (unlikely(ret == 0))
				continue;

			n_mbufs += bsz_rd;

#if APP_IO_TX_DROP_ALL_PACKETS
			{
				uint32_t j;
				APP_IO_TX_PREFETCH0(lp->tx.mbuf_out[port].array[0]);
				APP_IO_TX_PREFETCH0(lp->tx.mbuf_out[port].array[1]);

				for (j = 0; j < n_mbufs; j ++) {
					if (likely(j < n_mbufs - 2)) {
						APP_IO_TX_PREFETCH0(lp->tx.mbuf_out[port].array[j + 2]);
					}

					rte_pktmbuf_free(lp->tx.mbuf_out[port].array[j]);
				}

				lp->tx.mbuf_out[port].n_mbufs = 0;

				continue;
			}
#endif

			if (unlikely(n_mbufs < bsz_wr)) {
				lp->tx.mbuf_out[port].n_mbufs = n_mbufs;
				continue;
			}

			n_pkts = rte_eth_tx_burst(
				port,
				0,
				lp->tx.mbuf_out[port].array,
				(uint16_t) n_mbufs);

#if APP_STATS
			lp->tx.nic_ports_iters[port] ++;
			lp->tx.nic_ports_count[port] += n_pkts;
			if (unlikely(lp->tx.nic_ports_iters[port] == APP_STATS)) {
				unsigned lcore = rte_lcore_id();

				printf("\t\t\tI/O TX %u out (port %u): avg burst size = %.2f\n",
					lcore,
					(unsigned) port,
					((double) lp->tx.nic_ports_count[port]) / ((double) lp->tx.nic_ports_iters[port]));
				lp->tx.nic_ports_iters[port] = 0;
				lp->tx.nic_ports_count[port] = 0;
			}
#endif

			if (unlikely(n_pkts < n_mbufs)) {
				uint32_t k;
				for (k = n_pkts; k < n_mbufs; k ++) {
					struct rte_mbuf *pkt_to_free = lp->tx.mbuf_out[port].array[k];
					rte_pktmbuf_free(pkt_to_free);
				}
			}
			lp->tx.mbuf_out[port].n_mbufs = 0;
			lp->tx.mbuf_out_flush[port] = 0;
		}
	}
}

static inline void
app_lcore_io_tx_flush(struct app_lcore_params_io *lp)
{
	uint8_t port;
	uint32_t i;

	for (i = 0; i < lp->tx.n_nic_ports; i++) {
		uint32_t n_pkts;

		port = lp->tx.nic_ports[i];
		if (likely((lp->tx.mbuf_out_flush[port] == 0) ||
		           (lp->tx.mbuf_out[port].n_mbufs == 0))) {
			lp->tx.mbuf_out_flush[port] = 1;
			continue;
		}

		n_pkts = rte_eth_tx_burst(
			port,
			0,
			lp->tx.mbuf_out[port].array,
			(uint16_t) lp->tx.mbuf_out[port].n_mbufs);

		if (unlikely(n_pkts < lp->tx.mbuf_out[port].n_mbufs)) {
			uint32_t k;
			for (k = n_pkts; k < lp->tx.mbuf_out[port].n_mbufs; k ++) {
				struct rte_mbuf *pkt_to_free = lp->tx.mbuf_out[port].array[k];
				rte_pktmbuf_free(pkt_to_free);
			}
		}

		lp->tx.mbuf_out[port].n_mbufs = 0;
		lp->tx.mbuf_out_flush[port] = 1;
	}
}

static void
app_lcore_main_loop_io(void)
{
	uint32_t lcore = rte_lcore_id();
	struct app_lcore_params_io *lp = &app.lcore_params[lcore].io;
	uint32_t n_workers = app_get_lcores_worker();
	uint64_t i = 0;

	uint32_t bsz_rx_rd = app.burst_size_io_rx_read;
	uint32_t bsz_rx_wr = app.burst_size_io_rx_write;
	uint32_t bsz_tx_rd = app.burst_size_io_tx_read;
	uint32_t bsz_tx_wr = app.burst_size_io_tx_write;

	uint8_t pos_lb = app.pos_lb;

	for ( ; ; ) {
		if (APP_LCORE_IO_FLUSH && (unlikely(i == APP_LCORE_IO_FLUSH))) {
			if (likely(lp->rx.n_nic_queues > 0)) {
				app_lcore_io_rx_flush(lp, n_workers);
			}

			if (likely(lp->tx.n_nic_ports > 0)) {
				app_lcore_io_tx_flush(lp);
			}

			i = 0;
		}

		if (likely(lp->rx.n_nic_queues > 0)) {
			app_lcore_io_rx(lp, n_workers, bsz_rx_rd, bsz_rx_wr, pos_lb);
		}

		if (likely(lp->tx.n_nic_ports > 0)) {
			app_lcore_io_tx(lp, n_workers, bsz_tx_rd, bsz_tx_wr);
		}

		i ++;
	}
}

static inline void
app_lcore_worker(
	struct app_lcore_params_worker *lp,
	uint32_t bsz_rd,
	uint32_t bsz_wr)
{
	uint32_t i;

	for (i = 0; i < lp->n_rings_in; i ++) {
		struct rte_ring *ring_in = lp->rings_in[i];
		uint32_t j;
		int ret;

		ret = rte_ring_sc_dequeue_bulk(
			ring_in,
			(void **) lp->mbuf_in.array,
			bsz_rd,
			NULL);

		if (unlikely(ret == 0))
			continue;

#if APP_WORKER_DROP_ALL_PACKETS
		for (j = 0; j < bsz_rd; j ++) {
			struct rte_mbuf *pkt = lp->mbuf_in.array[j];
			rte_pktmbuf_free(pkt);
		}

		continue;
#endif

		APP_WORKER_PREFETCH1(rte_pktmbuf_mtod(lp->mbuf_in.array[0], unsigned char *));
		APP_WORKER_PREFETCH0(lp->mbuf_in.array[1]);

		for (j = 0; j < bsz_rd; j ++) {
			struct rte_mbuf *pkt;
			struct ipv4_hdr *ipv4_hdr;
			uint32_t ipv4_dst, pos;
			uint32_t port;

			if (likely(j < bsz_rd - 1)) {
				APP_WORKER_PREFETCH1(rte_pktmbuf_mtod(lp->mbuf_in.array[j+1], unsigned char *));
			}
			if (likely(j < bsz_rd - 2)) {
				APP_WORKER_PREFETCH0(lp->mbuf_in.array[j+2]);
			}

			pkt = lp->mbuf_in.array[j];
			ipv4_hdr = rte_pktmbuf_mtod_offset(pkt,
							   struct ipv4_hdr *,
							   sizeof(struct ether_hdr));
			ipv4_dst = rte_be_to_cpu_32(ipv4_hdr->dst_addr);

			if (unlikely(rte_lpm_lookup(lp->lpm_table, ipv4_dst, &port) != 0)) {
				port = pkt->port;
			}

			pos = lp->mbuf_out[port].n_mbufs;

			lp->mbuf_out[port].array[pos ++] = pkt;
			if (likely(pos < bsz_wr)) {
				lp->mbuf_out[port].n_mbufs = pos;
				continue;
			}

			ret = rte_ring_sp_enqueue_bulk(
				lp->rings_out[port],
				(void **) lp->mbuf_out[port].array,
				bsz_wr,
				NULL);

#if APP_STATS
			lp->rings_out_iters[port] ++;
			if (ret > 0) {
				lp->rings_out_count[port] += 1;
			}
			if (lp->rings_out_iters[port] == APP_STATS){
				printf("\t\tWorker %u out (NIC port %u): enq success rate = %.2f\n",
					(unsigned) lp->worker_id,
					(unsigned) port,
					((double) lp->rings_out_count[port]) / ((double) lp->rings_out_iters[port]));
				lp->rings_out_iters[port] = 0;
				lp->rings_out_count[port] = 0;
			}
#endif

			if (unlikely(ret == 0)) {
				uint32_t k;
				for (k = 0; k < bsz_wr; k ++) {
					struct rte_mbuf *pkt_to_free = lp->mbuf_out[port].array[k];
					rte_pktmbuf_free(pkt_to_free);
				}
			}

			lp->mbuf_out[port].n_mbufs = 0;
			lp->mbuf_out_flush[port] = 0;
		}
	}
}

static inline void
app_lcore_worker_flush(struct app_lcore_params_worker *lp)
{
	uint32_t port;

	for (port = 0; port < APP_MAX_NIC_PORTS; port ++) {
		int ret;

		if (unlikely(lp->rings_out[port] == NULL)) {
			continue;
		}

		if (likely((lp->mbuf_out_flush[port] == 0) ||
		           (lp->mbuf_out[port].n_mbufs == 0))) {
			lp->mbuf_out_flush[port] = 1;
			continue;
		}

		ret = rte_ring_sp_enqueue_bulk(
			lp->rings_out[port],
			(void **) lp->mbuf_out[port].array,
			lp->mbuf_out[port].n_mbufs,
			NULL);

		if (unlikely(ret == 0)) {
			uint32_t k;
			for (k = 0; k < lp->mbuf_out[port].n_mbufs; k ++) {
				struct rte_mbuf *pkt_to_free = lp->mbuf_out[port].array[k];
				rte_pktmbuf_free(pkt_to_free);
			}
		}

		lp->mbuf_out[port].n_mbufs = 0;
		lp->mbuf_out_flush[port] = 1;
	}
}

static void
app_lcore_main_loop_worker(void) {
	uint32_t lcore = rte_lcore_id();
	struct app_lcore_params_worker *lp = &app.lcore_params[lcore].worker;
	uint64_t i = 0;

	uint32_t bsz_rd = app.burst_size_worker_read;
	uint32_t bsz_wr = app.burst_size_worker_write;

	for ( ; ; ) {
		if (APP_LCORE_WORKER_FLUSH && (unlikely(i == APP_LCORE_WORKER_FLUSH))) {
			app_lcore_worker_flush(lp);
			i = 0;
		}

		app_lcore_worker(lp, bsz_rd, bsz_wr);

		i ++;
	}
}

int
app_lcore_main_loop(__attribute__((unused)) void *arg)
{
	struct app_lcore_params *lp;
	unsigned lcore;

	lcore = rte_lcore_id();
	lp = &app.lcore_params[lcore];

	if (lp->type == e_APP_LCORE_IO) {
		printf("Logical core %u (I/O) main loop.\n", lcore);
		app_lcore_main_loop_io();
	}

	if (lp->type == e_APP_LCORE_WORKER) {
		printf("Logical core %u (worker %u) main loop.\n",
			lcore,
			(unsigned) lp->worker.worker_id);
		app_lcore_main_loop_worker();
	}

	return 0;
}
